#include "../kir.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>

#define XSTR_UNIT (64)
#define xstra_set(code, ...) xstra_f(code, __VA_ARGS__)
#define xstra_inst(code, ...) xstra(code, "    ", 4), xstra_f(code, __VA_ARGS__)

typedef struct xstr {
    int cap;
    int len;
    char *s;
} xstr;

xstr *xstra(xstr *vs, const char *s, int l)
{
    int len = vs->len + l;
    if (len < vs->cap) {
        strcpy(vs->s + vs->len, s);
        vs->len = len;
    } else {
        int cap = (len < XSTR_UNIT) ? XSTR_UNIT : ((len / XSTR_UNIT) * (XSTR_UNIT << 1));
        char *ns = (char *)calloc(cap, sizeof(char));
        strcpy(ns, vs->s);
        strcpy(ns + vs->len, s);
        free(vs->s);
        vs->s = ns;
        vs->len = len;
        vs->cap = cap;
    }
    return vs;
}

xstr *xstra_f(xstr *vs, const char *fmt, ...)
{
    char buf[1024] = {0};
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(buf, fmt, ap);
    va_end(ap);
    xstra(vs, buf, strlen(buf));
    return vs;
}

const char *varname(char *buf, kl_kir_opr *rn)  /* buf should have at least 256 bytes. */
{
    if (rn->t == TK_VAR) {
        if (rn->index < 0) {
            sprintf(buf, "(*r)");
        } else {
            switch (rn->level) {
            case 0:
                sprintf(buf, "n%d", rn->index);
                break;
            case 1:
                sprintf(buf, "lex->v[%d]", rn->index);
                break;
            default:
                sprintf(buf, "get_lex(lex, %d)->v[%d]", rn->level, rn->index);
                break;
            }
        }
    } else {
        switch (rn->t) {
        case TK_VSINT:
            sprintf(buf, "%" PRId64, rn->i64);
            break;
        case TK_VUINT:
            sprintf(buf, "%" PRIu64, rn->u64);
            break;
        default:
            sprintf(buf, "<ERROR>");
            /* TODO: error */
            break;
        }
    }

    return buf;
}

const char *varvalue(char *buf, kl_kir_opr *rn)  /* buf should have at least 256 bytes. */
{
    varname(buf, rn);
    if (rn->t == TK_VAR) {
        strcat(buf, "->i");
    }
    return buf;
}

void translate_op3(xstr *code, const char *op, const char *sop, kl_kir_inst *i)
{
    kl_kir_opr *r1 = &(i->r1);
    kl_kir_opr *r2 = &(i->r2);
    kl_kir_opr *r3 = &(i->r3);

    char buf1[256] = {0};
    char buf2[256] = {0};
    char buf3[256] = {0};
    varname(buf1, r1);
    if (r1->typeid == TK_TSINT64 && r2->typeid == TK_TSINT64 && r3->typeid == TK_TSINT64) {
        varvalue(buf2, r2);
        varvalue(buf3, r3);
        xstra_inst(code, "%s->t = VAR_INT64; %s->i = %s %s %s;\n", buf1, buf1, buf2, sop, buf3);
    } else {
        varname(buf2, r2);
        varname(buf3, r3);
        if (r2->t == TK_VSINT) {
            if (r3->t == TK_VSINT) {
                xstra_inst(code, "OP_%s_I_I(ctx, %s, %s, %s);\n", op, buf1, buf2, buf3);
            } else {
                xstra_inst(code, "OP_%s_I_V(ctx, %s, %s, %s);\n", op, buf1, buf2, buf3);
            }
        } else {
            if (r3->t == TK_VSINT) {
                xstra_inst(code, "OP_%s_V_I(ctx, %s, %s, %s);\n", op, buf1, buf2, buf3);
            } else {
                xstra_inst(code, "OP_%s(ctx, %s, %s, %s);\n", op, buf1, buf2, buf3);
            }
        }
    }
}

void translate_funcref(xstr *code, kl_kir_func *f)
{
    int check[256] = {0};
    int p = 0;

    char buf2[256] = {0};
    kl_kir_inst *i = f->head;
    while (i) {
        if (i->opcode == KIR_CALL) {
            int id = i->r2.funcid;
            for (int idx = 0; idx < p; ++idx) {
                if (check[idx] == id) {
                    id = 0;
                    break;
                }
            }
            if (id > 0) {
                varname(buf2, &(i->r2));
                xstra_inst(code, "vmfnc *f%d = (%s)->f;\n", id, buf2);
                check[p++] = id;
            }
        }
        i = i->next;
    }
}

void translate_inst(xstr *code, kl_kir_func *f, kl_kir_inst *i, int gc)
{
    if (i->disabled) {
        return;
    }

    char buf1[256] = {0};
    char buf2[256] = {0};
    switch (i->opcode) {
    case KIR_ALOCAL:
        xstra_inst(code, "int allocated_local = %" PRId64 ";\n", i->r1.i64);
        xstra_inst(code, "alloc_var(ctx, %" PRId64 ");\n", i->r1.i64);
        for (int idx = 0; idx < i->r1.i64; ++idx) {
            xstra_inst(code, "vmvar *n%d = local_var(ctx, %d);\n", idx, idx);
        }
        for (int idx = 0; idx < i->r2.i64; ++idx) {
            xstra_inst(code, "SET_ARGVAR(%d, %" PRId64 ");\n", idx, i->r1.i64);
        }
        xstra_inst(code, "\n");
        translate_funcref(code, f);
        xstra_inst(code, "\n");
        break;
    case KIR_RLOCAL:
        xstra_inst(code, "reduce_vstackp(ctx, allocated_local);\n");
        break;
    case KIR_MKFRM:
        break;
    case KIR_POPFRM:
        break;

    case KIR_SVSTKP:
        xstra_inst(code, "p = vstackp(ctx);\n");
        break;
    case KIR_PUSHARG:
        xstra_inst(code, "push_var(ctx, %s);\n", varname(buf1, &(i->r1)));
        break;
    case KIR_CALL:
        varname(buf1, &(i->r1));
        if (i->r2.funcid > 0) {
            if (i->r2.recursive) {
                xstra_inst(code, "e = (f%d->f)(ctx, lex, &(%s), 1);\n", i->r2.funcid, buf1);
            } else {
                xstra_inst(code, "e = (f%d->f)(ctx, f%d->lex, &(%s), 1);\n", i->r2.funcid, i->r2.funcid, buf1);
            }
        } else {
            varname(buf2, &(i->r2));
            xstra_inst(code, "e = (((%s)->f)->f)(ctx, ((%s)->f)->lex, &(%s), 1);\n", buf2, buf2, buf1);
        }
        break;
    case KIR_RSSTKP:
        xstra_inst(code, "restore_vstackp(ctx, p);\n");
        break;
    case KIR_CHKEXCEPT:
        xstra_inst(code, "if (e) goto L%d;\n", i->labelid);
        break;

    case KIR_RET:
        xstra_inst(code, "return e;\n");
        break;

    case KIR_JMPIFF:
        xstra_inst(code, "OP_JMP_IF_FALSE(%s, L%d);\n", varname(buf1, &(i->r1)), i->labelid);
        break;
    case KIR_JMP:
        xstra_inst(code, "goto L%d;\n", i->labelid);
        break;
    case KIR_LABEL:
        xstra_set(code, "L%d:;\n", i->labelid);
        if (i->gcable) {
            xstra_inst(code, "GC_CHECK(ctx);\n");
        }
        break;

    case KIR_MOV:
        xstra_inst(code, "COPY_VAR_TO(ctx, %s, %s);\n", varname(buf1, &(i->r1)), varname(buf2, &(i->r2)));
        break;

    case KIR_ADD:
        translate_op3(code, "ADD", "+", i);
        break;
    case KIR_SUB:
        translate_op3(code, "SUB", "-", i);
        break;

    case KIR_LT:
        translate_op3(code, "LT", "<", i);
        break;

    }
}

void translate_func(kl_kir_program *p, xstr *code, kl_kir_func *f)
{
    xstra_set(code, "/* function:%s */\n", f->name);
    xstra_set(code, "int func_fib(vmctx *ctx, vmfrm *lex, vmvar **r, int ac)\n{\n");
    xstra_inst(code, "GC_CHECK(ctx);\n");
    xstra_inst(code, "int p, e = 0;\n");

    kl_kir_inst *i = f->head;
    kl_kir_inst *prev = NULL;
    while (i) {
        int gc = prev && prev->opcode == KIR_LABEL && i->opcode != KIR_LABEL;
        translate_inst(code, f, i, gc);
        prev = i;
        i = i->next;
    }

    xstra_set(code, "}\n\n");
}

char *translate(kl_kir_program *p)
{
    xstr str = {
        .len = 0,
        .cap = XSTR_UNIT,
        .s = (char *)calloc(XSTR_UNIT, sizeof(char)),
    };

    if (!p) {
        return str.s;
    }

    kl_kir_func *f = p->head;
    while (f) {
        translate_func(p, &str, f);
        f = f->next;
    }

    return str.s;   /* this should be freed by the caller. */
}
