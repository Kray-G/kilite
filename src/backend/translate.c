#include "../kir.h"
#include "header.h"
#include "translate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>

typedef struct func_context {
    int has_frame;
    int total_vars;
    int local_vars;
    int arg_count;
    int temp_count;
} func_context;

#define XSTR_UNIT (64)
#define xstra_set(code, ...) xstra_f(code, __VA_ARGS__)
#define xstra_inst(code, ...) xstra(code, "    ", 4), xstra_f(code, __VA_ARGS__)

typedef struct xstr {
    int cap;
    int len;
    char *s;
} xstr;

static xstr *xstra(xstr *vs, const char *s, int l)
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

static xstr *xstra_f(xstr *vs, const char *fmt, ...)
{
    char buf[1024] = {0};
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(buf, fmt, ap);
    va_end(ap);
    xstra(vs, buf, strlen(buf));
    return vs;
}

static inline int is_var(kl_kir_opr *rn)
{
    return rn->t == TK_VAR;
}

static const char *var_or_int(char *buf, kl_kir_opr *rn)  /* buf should have at least 256 bytes. */
{
    switch (rn->t) {
    case TK_VSINT:
        sprintf(buf, "%" PRId64, rn->i64);
        break;
    case TK_VAR:
        if (rn->index < 0) {
            sprintf(buf, "(r)");
        } else {
            switch (rn->level) {
            case 0:
                sprintf(buf, "n%d", rn->index);
                break;
            case 1:
                sprintf(buf, "(lex->v[%d])", rn->index);
                break;
            default:
                sprintf(buf, "(get_lex(lex, %d)->v[%d])", rn->level, rn->index);
                break;
            }
        }
        break;
    default:
        sprintf(buf, "<ERROR>");
        /* TODO: error */
        break;
    }

    return buf;
}

static const char *varvalue(char *buf, kl_kir_opr *rn)  /* buf should have at least 256 bytes. */
{
    var_or_int(buf, rn);
    if (rn->t == TK_VAR) {
        strcat(buf, "->i");
    }
    return buf;
}

static void translate_pushvar(xstr *code, kl_kir_opr *rn)
{
    char buf1[256] = {0};
    switch (rn->t) {
    case TK_VSINT:
        xstra_inst(code, "{ vmvar c = { .t = VAR_INT64, .i = %" PRId64 " }; push_var(ctx, &c); }\n", rn->i64);
        break;
    case TK_VAR:
        xstra_inst(code, "push_var(ctx, %s);\n", var_or_int(buf1, rn));
        break;
    default:
        xstra_inst(code, "<ERROR>");
        /* TODO: error */
        break;
    }

}

static void translate_op3(xstr *code, const char *op, const char *sop, kl_kir_inst *i)
{
    kl_kir_opr *r1 = &(i->r1);
    kl_kir_opr *r2 = &(i->r2);
    kl_kir_opr *r3 = &(i->r3);

    char buf1[256] = {0};
    char buf2[256] = {0};
    char buf3[256] = {0};
    var_or_int(buf1, r1);
    if (r1->typeid == TK_TSINT64 && r2->typeid == TK_TSINT64 && r3->typeid == TK_TSINT64) {
        varvalue(buf2, r2);
        varvalue(buf3, r3);
        xstra_inst(code, "SET_I64(%s, %s %s %s);\n", buf1, buf1, buf2, sop, buf3);
    } else {
        var_or_int(buf2, r2);
        var_or_int(buf3, r3);
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

static void translate_call(xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_or_int(buf1, &(i->r1));
    if (i->r2.funcid > 0) {
        if (i->r2.recursive) {
            xstra_inst(code, "e = (f%d->f)(ctx, lex, %s, 1);\n", i->r2.funcid, buf1);
        } else {
            xstra_inst(code, "e = (f%d->f)(ctx, f%d->lex, %s, 1);\n", i->r2.funcid, i->r2.funcid, buf1);
        }
    } else {
        var_or_int(buf2, &(i->r2));
        xstra_inst(code, "e = (((%s)->f)->f)(ctx, ((%s)->f)->lex, %s, 1);\n", buf2, buf2, buf1);
    }
}

static void translate_mov(xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_or_int(buf1, &(i->r1));
    var_or_int(buf2, &(i->r2));
    if (i->r2.t == TK_VAR) {
        xstra_inst(code, "COPY_VAR_TO(ctx, %s, %s);\n", buf1, buf2);
    } else if (i->r2.t == TK_VSINT) {
        xstra_inst(code, "SET_I64(%s, %s);\n", buf1, buf2);
    }
}

static void translate_funcref(xstr *code, kl_kir_func *f)
{
    int check[256] = {0};
    int p = 0;

    char buf2[256] = {0};
    kl_kir_inst *i = f->head;
    while (i) {
        if (i->opcode == KIR_CALL && i->r2.level > 0) {
            int id = i->r2.funcid;
            for (int idx = 0; idx < p; ++idx) {
                if (check[idx] == id) {
                    id = 0;
                    break;
                }
            }
            if (id > 0) {
                var_or_int(buf2, &(i->r2));
                xstra_inst(code, "vmfnc *f%d = (%s)->f;\n", id, buf2);
                check[p++] = id;
            }
        }
        i = i->next;
    }
}

static void translate_inst(xstr *code, kl_kir_func *f, kl_kir_inst *i, func_context *fctx, int blank)
{
    if (i->disabled) {
        return;
    }

    if (blank) {
        xstra(code, "\n", 1);
    }

    // if (i->line > 0) {
    //     xstra_f(code, "#line %d\n", i->line);
    // }
    char buf1[256] = {0};
    char buf2[256] = {0};
    switch (i->opcode) {
    case KIR_ALOCAL:
        fctx->total_vars = i->r1.i64;
        fctx->local_vars = i->r2.i64;   // including arguments.
        fctx->arg_count = i->r3.i64;
        if (fctx->total_vars > 0) {
            xstra_inst(code, "const int allocated_local = %" PRId64 ";\n", fctx->total_vars);
            xstra_inst(code, "alloc_var(ctx, %" PRId64 ");\n", fctx->total_vars);
            xstra_inst(code, "if (e) goto L%d;\n", f->funcend);
        }
        for (int idx = 0; idx < fctx->total_vars; ++idx) {
            xstra_inst(code, "vmvar *n%d = local_var(ctx, %d);\n", idx, idx);
        }
        for (int idx = 0; idx < fctx->arg_count; ++idx) {
            xstra_inst(code, "SET_ARGVAR(%d, %" PRId64 ");\n", idx, fctx->total_vars);
        }
        translate_funcref(code, f);
        xstra_inst(code, "\n");
        break;
    case KIR_RLOCAL:
        if (fctx->total_vars > 0) {
            xstra_inst(code, "reduce_vstackp(ctx, allocated_local);\n");
        }
        break;
    case KIR_MKFRM:
        fctx->total_vars = i->r1.i64;
        fctx->local_vars = i->r2.i64;   // including arguments.
        fctx->arg_count = i->r3.i64;
        fctx->temp_count = fctx->total_vars - fctx->local_vars;
        xstra_inst(code, "vmfrm *frm = alcfrm(ctx, %" PRId64 ");\n", i->r1.i64);
        xstra_inst(code, "push_frm(ctx, frm);\n");
        for (int idx = 0; idx < fctx->local_vars; ++idx) {
            xstra_inst(code, "vmvar *n%d = frm->v[%d] = alcvar_initial(ctx);\n", idx, idx);
        }
        if (fctx->total_vars > 0 && fctx->temp_count > 0) {
            xstra_inst(code, "const int allocated_local = %" PRId64 ";\n", fctx->temp_count);
            xstra_inst(code, "alloc_var(ctx, %" PRId64 ");\n", fctx->temp_count);
            xstra_inst(code, "if (e) goto L%d;\n", f->funcend);
            for (int idx = fctx->local_vars; idx < fctx->total_vars; ++idx) {
                xstra_inst(code, "vmvar *n%d = local_var(ctx, %d);\n", idx, idx - fctx->local_vars);
            }
        }
        translate_funcref(code, f);
        xstra_inst(code, "\n");
        break;
    case KIR_POPFRM:
        if (fctx->total_vars > 0 && fctx->temp_count > 0) {
            xstra_inst(code, "reduce_vstackp(ctx, allocated_local);\n");
        }
        xstra_inst(code, "pop_frm(ctx);\n");
        break;

    case KIR_SVSTKP:
        xstra_inst(code, "p = vstackp(ctx);\n");
        break;
    case KIR_PUSHARG:
        translate_pushvar(code, &(i->r1));
        xstra_inst(code, "if (e) goto L%d;\n", f->funcend);
        break;
    case KIR_CALL:
        translate_call(code, i);
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
        xstra_inst(code, "OP_JMP_IF_FALSE(%s, L%d);\n", var_or_int(buf1, &(i->r1)), i->labelid);
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
        translate_mov(code, i);
        break;

    case KIR_MOVFNC:
        var_or_int(buf1, &(i->r1));
        xstra_inst(code, "vmfnc *f%d = alcfnc(ctx, %s_%d, frm, 0);\n", i->r2.funcid, i->r2.name, i->r2.funcid);
        xstra_inst(code, "SET_FNC(%s, f%d);\n", buf1, i->r2.funcid);
        break;

    case KIR_ADD:
        translate_op3(code, "ADD", "+", i);
        break;
    case KIR_SUB:
        translate_op3(code, "SUB", "-", i);
        break;
    case KIR_MUL:
        translate_op3(code, "MUL", "*", i);
        break;
    case KIR_DIV:
        translate_op3(code, "DIV", "/", i);
        break;
    case KIR_MOD:
        translate_op3(code, "mod", "%", i);
        break;

    case KIR_EQEQ:
        translate_op3(code, "EQEQ", "==", i);
        break;
    case KIR_NEQ:
        translate_op3(code, "NEQ", "!=", i);
        break;
    case KIR_LT:
        translate_op3(code, "LT", "<", i);
        break;

    }
}

void translate_func(kl_kir_program *p, xstr *code, kl_kir_func *f)
{
    func_context fctx = {
        .has_frame = f->has_frame,
    };
    xstra_set(code, "/* function:%s */\n", f->name);
    xstra_set(code, "int %s_%d(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)\n{\n", f->name, f->funcid);
    xstra_inst(code, "GC_CHECK(ctx);\n");
    xstra_inst(code, "int p, e = 0;\n");

    kl_kir_inst *i = f->head;
    kl_kir_inst *prev = NULL;
    while (i) {
        int blank = prev && prev->opcode != KIR_LABEL && i->opcode == KIR_LABEL;
        translate_inst(code, f, i, &fctx, blank);
        prev = i;
        i = i->next;
    }

    xstra_set(code, "}\n\n");
}

char *translate(kl_kir_program *p, int mode)
{
    if (!p) {
        return NULL;
    }

    const char *header = vmheader();
    int len = strlen(header);
    xstr str = {
        .len = 0,
        .cap = XSTR_UNIT,
        .s = (char *)calloc(len * 2, sizeof(char)),
    };

    if (mode == TRANS_FULL) {
        xstra(&str, header, len);
    }
    kl_kir_func *f = p->head;
    while (f) {
        translate_func(p, &str, f);
        f = f->next;
    }

    if (mode == TRANS_FULL) {
        xstra_set(&str, "void setup_context(vmctx *ctx)\n{\n");
        xstra_inst(&str, "ctx->print_result = %d;\n", p->print_result);
        xstra_inst(&str, "ctx->verbose = %d;\n", p->verbose);
        xstra_set(&str, "}\n");
    }

    return str.s;   /* this should be freed by the caller. */
}
