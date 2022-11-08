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
    int push_count;
    int push_max;
    int temp_count;
    int skip;
} func_context;

#define XSTR_UNIT (64)
#define xstra_inst(code, ...) xstra(code, "    ", 4), xstraf(code, __VA_ARGS__)

typedef struct xstr {
    int cap;
    int len;
    char *s;
} xstr;

static xstr *xstrc(xstr *vs, const char c)
{
    int len = vs->len + 1;
    if (len < vs->cap) {
        vs->s[vs->len] = c;
        vs->s[vs->len + 1] = 0;
        vs->len = len;
    } else {
        int cap = (len < XSTR_UNIT) ? XSTR_UNIT : ((len / XSTR_UNIT) * (XSTR_UNIT << 1));
        char *ns = (char *)calloc(cap, sizeof(char));
        strcpy(ns, vs->s);
        ns[vs->len] = c;
        ns[vs->len + 1] = 0;
        free(vs->s);
        vs->s = ns;
        vs->len = len;
        vs->cap = cap;
    }
    return vs;
}

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

static inline xstr *xstrs(xstr *vs, const char *s)
{
    return xstra(vs, s, strlen(s));
}

static xstr *xstraf(xstr *vs, const char *fmt, ...)
{
    char buf[1024] = {0};
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(buf, fmt, ap);
    va_end(ap);
    xstra(vs, buf, strlen(buf));
    return vs;
}

static void escape_str(xstr *code, const char *s)
{
    while (*s) {
        if (*s == '"' || *s == '\\') {
            xstrc(code, '\\');
        }
        xstrc(code, *s);
        s++;
    }
}

static inline int is_var(kl_kir_opr *rn)
{
    return rn->t == TK_VAR;
}

static const char *var_value_pure(char *buf, kl_kir_opr *rn)  /* buf should have at least 256 bytes. */
{
    switch (rn->t) {
    case TK_VSINT:
        sprintf(buf, "%" PRId64, rn->i64);
        break;
    case TK_VAR:
        if (rn->index < 0) {
            sprintf(buf, "(r)");
        } else {
            /* The level should be 0 */
            sprintf(buf, "n%d", rn->index);
        }
        break;
    default:
        sprintf(buf, "<ERROR>");
        /* TODO: error */
        break;
    }

    return buf;
}

static void translate_pushvar_pure(xstr *code, func_context *fctx, kl_kir_opr *rn)
{
    int pushed = fctx->push_count++;
    const char *typestr = (fctx->push_max <= pushed) ? "int64_t " : "";
    char buf1[256] = {0};
    switch (rn->t) {
    case TK_VSINT:
        xstra_inst(code, "%st%d = %" PRId64 ";\n", typestr, pushed, rn->i64);
        break;
    case TK_VAR:
        xstra_inst(code, "%st%d = %s;\n", typestr, pushed, var_value_pure(buf1, rn));
        break;
    default:
        xstra_inst(code, "<ERROR>;");
        /* TODO: error */
        break;
    }
    if (fctx->push_max < fctx->push_count) {
        fctx->push_max = fctx->push_count;
    }
}

static void translate_call_pure(xstr *code, kl_kir_func *f, func_context *fctx, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value_pure(buf1, &(i->r1));
    if (i->r2.funcid > 0 && i->r2.recursive) {
        xstra_inst(code, "%s = %s__pure(e", buf1, f->funcname);
        if (f->arg_count > 0) {
            for (int i = f->arg_count - 1; i >= 0; --i) {
                xstraf(code, ", t%d", i);
            }
            fctx->push_count -= f->arg_count;
        }
        xstraf(code, ");\n");
        int output_jump = 1;
        if (i->next) {
            kl_kir_inst *n = i->next;
            while (n && (n->opcode == KIR_CHKEXCEPT || n->opcode == KIR_RSSTKP)) {
                n = n->next;
            }
            if (n) {
                if (n->opcode == KIR_JMP && n->labelid == f->funcend) {
                    output_jump = 0;
                } else if (n->opcode == KIR_LABEL && n->labelid == f->funcend) {
                    output_jump = 0;
                }
            }
        }
        if (output_jump) {
            xstra_inst(code, "if (*e > 0) goto L%d;\n", f->funcend);
        }
    } else {
        xstra_inst(code, "<ERROR>");
        /* TODO: error */
    }
}

static const char *get_min(kl_kir_func *f, char *buf)
{
    sprintf(buf, "get_min%d(n0", f->arg_count);
    int p = strlen(buf);
    for (int i = 1; i < f->arg_count; ++i) {
        sprintf(buf+p, ", n%d", i);
        p = strlen(buf);
    }
    sprintf(buf+p, ")");
    return buf;
}

static void translate_op3_pure(func_context *fctx, xstr *code, kl_kir_func *f, const char *op, const char *sop, kl_kir_inst *i)
{
    kl_kir_opr *r1 = &(i->r1);
    kl_kir_opr *r2 = &(i->r2);
    kl_kir_opr *r3 = &(i->r3);

    char buf1[256] = {0};
    char buf2[256] = {0};
    char buf3[256] = {0};
    char buf4[256] = {0};
    var_value_pure(buf1, r1);
    var_value_pure(buf2, r2);
    var_value_pure(buf3, r3);
    if (f->arg_count == 0) {
        xstra_inst(code, "OP_N%s_I_I(e, 1, %s, %s, %s, L%d);\n", op, buf1, buf2, buf3, f->funcend);
    } else if (f->arg_count == 1) {
        xstra_inst(code, "OP_N%s_I_I(e, n0, %s, %s, %s, L%d);\n", op, buf1, buf2, buf3, f->funcend);
    } else {
        xstra_inst(code, "{ int64_t min = %s;\n", get_min(f, buf4));
        xstra_inst(code, "  OP_N%s_I_I(e, min, %s, %s, %s, L%d); }\n", op, buf1, buf2, buf3, f->funcend);
    }
}

static void translate_chkcnd_pure(func_context *fctx, xstr *code, kl_kir_func *f, const char *op, const char *sop, kl_kir_inst *i)
{
    if (i->next && (i->next->opcode == KIR_JMPIFT || i->next->opcode == KIR_JMPIFF)) {
        kl_kir_inst *n = i->next;
        if (i->r1.level == n->r1.level && i->r1.index == n->r1.index) {
            kl_kir_opr *r1 = &(i->r1);
            kl_kir_opr *r2 = &(i->r2);
            kl_kir_opr *r3 = &(i->r3);

            char buf2[256] = {0};
            char buf3[256] = {0};
            var_value_pure(buf2, r2);
            var_value_pure(buf3, r3);
            xstra_inst(code, "if (%s((%s) %s (%s))) goto L%d;\n",
                i->next->opcode == KIR_JMPIFT ? "" : "!",
                buf2, sop, buf3, n->labelid);
            fctx->skip = 1;
            return;
        }
    }
    translate_op3_pure(fctx, code, f, op, sop, i);
}

static void translate_incdec_pure(func_context *fctx, xstr *code, const char *op, const char *sop, int is_postfix, kl_kir_inst *i)
{
    kl_kir_opr *r1 = &(i->r1);
    kl_kir_opr *r2 = &(i->r2);

    char buf1[256] = {0};
    char buf2[256] = {0};

    var_value_pure(buf1, r1);
    var_value_pure(buf2, r2);
    if (is_postfix) {
        if (r1->prevent) {
            xstra_inst(code, "(%s)%s;\n", buf2, sop);
        }  else {
            xstra_inst(code, "%s = (%s)%s;\n", buf1, buf2, sop);
        }
    } else {
        if (r1->prevent) {
            xstra_inst(code, "%s(%s);\n", sop, buf2);
        }  else {
            xstra_inst(code, "%s = %s(%s);\n", buf1, sop, buf2);
        }
    }
}

static void translate_mov_pure(xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value_pure(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VAR: {
        char buf2[256] = {0};
        var_value_pure(buf2, &(i->r2));
        xstra_inst(code, "%s = %s;\n", buf1, buf2);
        break;
    }
    case TK_VSINT:
        xstra_inst(code, "%s = %" PRId64 ";\n", buf1, i->r2.i64);
        break;
    default:
        break;
    }
}

static void translate_inst_pure(xstr *code, kl_kir_func *f, kl_kir_inst *i, func_context *fctx, int blank)
{
    if (i->disabled) {
        return;
    }

    if (blank) {
        xstra(code, "\n", 1);
    }

    char buf1[256] = {0};
    char buf2[256] = {0};
    switch (i->opcode) {
    case KIR_ALOCAL:
        fctx->total_vars = i->r1.i64;
        fctx->local_vars = i->r2.i64;   // including arguments.
        fctx->arg_count = i->r3.i64;
        xstra_inst(code, "int64_t r = 0;\n");
        for (int idx = 0; idx < fctx->total_vars; ++idx) {
            if (idx >= fctx->arg_count) {
                xstra_inst(code, "int64_t n%d = 0;\n", idx);
            }
        }
        xstra_inst(code, "\n");
        break;

    case KIR_PUSHARG:
        translate_pushvar_pure(code, fctx, &(i->r1));
        break;
    case KIR_CALL:
        translate_call_pure(code, f, fctx, i);
        break;

    case KIR_RET:
        xstra_inst(code, "return r;\n");
        break;

    case KIR_JMPIFT:
        break;
    case KIR_JMPIFF:
        break;
    case KIR_JMP:
        xstra_inst(code, "goto L%d;\n", i->labelid);
        break;
    case KIR_LABEL:
        xstraf(code, "L%d:;\n", i->labelid);
        break;

    case KIR_MOV:
    case KIR_MOVA:
        translate_mov_pure(code, i);
        break;

    case KIR_ADD:
        translate_chkcnd_pure(fctx, code, f, "ADD", "+", i);
        break;
    case KIR_SUB:
        translate_chkcnd_pure(fctx, code, f, "SUB", "-", i);
        break;
    case KIR_MUL:
        translate_chkcnd_pure(fctx, code, f, "MUL", "*", i);
        break;
    case KIR_DIV:
        break;
    case KIR_MOD:
        translate_chkcnd_pure(fctx, code, f, "mod", "%", i);
        break;

    case KIR_EQEQ:
        translate_chkcnd_pure(fctx, code, f, "EQEQ", "==", i);
        break;
    case KIR_NEQ:
        translate_chkcnd_pure(fctx, code, f, "NEQ", "!=", i);
        break;
    case KIR_LT:
        translate_chkcnd_pure(fctx, code, f, "LT", "<", i);
        break;
    case KIR_LE:
        translate_chkcnd_pure(fctx, code, f, "LE", "<=", i);
        break;
    case KIR_GT:
        translate_chkcnd_pure(fctx, code, f, "GT", ">", i);
        break;
    case KIR_GE:
        translate_chkcnd_pure(fctx, code, f, "GE", ">=", i);
        break;

    case KIR_INC:
        translate_incdec_pure(fctx, code, "INC", "++", 0, i);
        break;
    case KIR_INCP:
        translate_incdec_pure(fctx, code, "INCP", "++", 1, i);
        break;
    case KIR_DEC:
        translate_incdec_pure(fctx, code, "DEC", "++", 0, i);
        break;
    case KIR_DECP:
        translate_incdec_pure(fctx, code, "DECP", "++", 1, i);
        break;
    case KIR_MINUS:
        break;
    }
}

void translate_pure_func(kl_kir_program *p, xstr *code, kl_kir_func *f)
{
    func_context fctx = {
        .has_frame = 0,
    };

    xstraf(code, "/* pure function:%s */\n", f->name);
    xstraf(code, "int64_t %s__pure(int64_t *e", f->funcname);
    for (int i = 0; i < f->arg_count; ++i) {
        xstraf(code, ", int64_t n%d", i);
    }
    xstraf(code, ")\n{\n");

    kl_kir_inst *i = f->head;
    kl_kir_inst *prev = NULL;
    while (i) {
        int blank = prev && prev->opcode != KIR_LABEL && i->opcode == KIR_LABEL;
        translate_inst_pure(code, f, i, &fctx, blank);
        while (fctx.skip > 0) {
            fctx.skip--;
            i = i->next;
        }
        prev = i;
        i = i->next;
    }

    xstraf(code, "}\n\n");
}

static const char *var_value(char *buf, kl_kir_opr *rn)  /* buf should have at least 256 bytes. */
{
    switch (rn->t) {
    case TK_VSINT:
        sprintf(buf, "%" PRId64, rn->i64);
        break;
    case TK_VDBL:
        sprintf(buf, "%f", rn->dbl);
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

static const char *int_value(char *buf, kl_kir_opr *rn)  /* buf should have at least 256 bytes. */
{
    var_value(buf, rn);
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
        xstra_inst(code, "{ push_var_i(ctx, %" PRId64 "); }\n", rn->i64);
        break;
    case TK_VDBL:
        xstra_inst(code, "{ push_var_d(ctx, %f); }\n", rn->dbl);
        break;
    case TK_VSTR:
        xstra_inst(code, "{ push_var_s(ctx, \"");
        escape_str(code, rn->str);
        xstrs(code, "\"); }\n");
        break;
    case TK_VAR:
        xstra_inst(code, "push_var(ctx, %s);\n", var_value(buf1, rn));
        break;
    default:
        xstra_inst(code, "<ERROR>");
        /* TODO: error */
        break;
    }

}

static void translate_incdec(func_context *fctx, xstr *code, const char *op, const char *sop, int is_postfix, kl_kir_inst *i)
{
    kl_kir_opr *r1 = &(i->r1);
    kl_kir_opr *r2 = &(i->r2);

    char buf1[256] = {0};
    char buf2[256] = {0};

    var_value(buf1, r1);
    if (r2->typeid == TK_TSINT64) {
        int_value(buf2, r2);
        if (is_postfix) {
            if (r1->prevent) {
                xstra_inst(code, "(%s)%s;\n", buf2, sop);
            }  else {
                xstra_inst(code, "SET_I64(%s, (%s)%s);\n", buf1, buf2, sop);
            }
        } else {
            if (r1->prevent) {
                xstra_inst(code, "%s(%s);\n", sop, buf2);
            }  else {
                xstra_inst(code, "SET_I64(%s, %s(%s));\n", buf1, sop, buf2);
            }
        }
    } else {
        if (r1->prevent) {
            xstra_inst(code, "OP_%s_SAME(ctx, %s);\n", op, buf1);
        } else {
            var_value(buf2, r2);
            xstra_inst(code, "OP_%s(ctx, %s, %s);\n", op, buf1, buf2);
        }
    }
}

static void translate_idx(func_context *fctx, xstr *code, kl_kir_inst *i, int r3typeid, int lvalue)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    char buf3[256] = {0};
    var_value(buf1, &(i->r1));
    var_value(buf2, &(i->r2));
    switch (i->r3.t) {
    case TK_VSINT:
        xstra_inst(code, "OP_ARRAY_REF%s_I(ctx, %s, %s, %" PRId64 ");\n", (lvalue ? "L" : ""), buf1, buf2, i->r3.i64);
        break;
    case TK_VDBL:
        xstra_inst(code, "OP_ARRAY_REF%s_I(ctx, %s, %s, %" PRId64 ");\n", (lvalue ? "L" : ""), buf1, buf2, (int)(i->r3.dbl));
        break;
    case TK_VSTR:
        xstra_inst(code, "OP_HASH_APPLY%s(ctx, %s, %s, \"", (lvalue ? "L" : ""), buf1, buf2);
        escape_str(code, i->r3.str);
        xstrs(code, "\");\n");
        break;
    case TK_VAR:
        switch (r3typeid) {
        case TK_TSINT64:
            xstra_inst(code, "OP_ARRAY_REF%s_I(ctx, %s, %s, %s);\n", (lvalue ? "L" : ""), buf1, buf2, int_value(buf3, &(i->r3)));
            break;
        default:
            xstra_inst(code, "OP_ARRAY_REF%s(ctx, %s, %s, %s);\n", (lvalue ? "L" : ""), buf1, buf2, var_value(buf3, &(i->r3)));
            break;
        }
        break;
    default:
        break;
    }
}

static void translate_op3(func_context *fctx, xstr *code, const char *op, const char *sop, kl_kir_inst *i)
{
    kl_kir_opr *r1 = &(i->r1);
    kl_kir_opr *r2 = &(i->r2);
    kl_kir_opr *r3 = &(i->r3);

    char buf1[256] = {0};
    char buf2[256] = {0};
    char buf3[256] = {0};
    var_value(buf1, r1);
    if (r2->typeid == TK_TSINT64 && r3->typeid == TK_TSINT64) {
        int_value(buf2, r2);
        int_value(buf3, r3);
        xstra_inst(code, "SET_I64(%s, (%s) %s (%s));\n", buf1, buf2, sop, buf3);
    } else {
        var_value(buf2, r2);
        var_value(buf3, r3);
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

static void translate_chkcnd(func_context *fctx, xstr *code, const char *op, const char *sop, kl_kir_inst *i)
{
    if (i->next && (i->next->opcode == KIR_JMPIFT || i->next->opcode == KIR_JMPIFF)) {
        kl_kir_inst *n = i->next;
        if (i->r1.level == n->r1.level && i->r1.index == n->r1.index) {
            kl_kir_opr *r1 = &(i->r1);
            kl_kir_opr *r2 = &(i->r2);
            kl_kir_opr *r3 = &(i->r3);
            if (r1->typeid == TK_TSINT64 && r2->typeid == TK_TSINT64 && r3->typeid == TK_TSINT64) {
                char buf2[256] = {0};
                char buf3[256] = {0};
                int_value(buf2, r2);
                int_value(buf3, r3);
                xstra_inst(code, "if (%s((%s) %s (%s))) goto L%d;\n",
                    i->next->opcode == KIR_JMPIFT ? "" : "!",
                    buf2, sop, buf3, n->labelid);
                fctx->skip = 1;
                return;
            }
        }
    }
    translate_op3(fctx, code, op, sop, i);
}

static void translate_call(xstr *code, kl_kir_func *f, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));
    if (i->r2.funcid > 0) {
        if (i->r2.t == TK_FUNC) {
            /* This means a direct call of an anonymous function. */
            xstra_inst(code, "vmfnc *f%d = alcfnc(ctx, %s, frm, 0);\n", i->r2.funcid, i->r2.name);
        }

        xstra_inst(code, "{\n");
        xstra_inst(code, "    vmfnc *callee = ctx->callee;\n");
        xstra_inst(code, "    ctx->callee = f%d;\n", i->r2.funcid);
                if (i->r2.recursive) {
            xstra_inst(code, "    e = ((vmfunc_t)(f%d->f))(ctx, lex, %s, %d);\n", i->r2.funcid, buf1, i->r2.args);
        } else {
            xstra_inst(code, "    e = ((vmfunc_t)(f%d->f))(ctx, f%d->lex, %s, %d);\n", i->r2.funcid, i->r2.funcid, buf1, i->r2.args);
        }
        xstra_inst(code, "    ctx->callee = callee;\n", i->r2.funcid);
        xstra_inst(code, "}\n");
    } else {
        var_value(buf2, &(i->r2));
        xstra_inst(code, "CHECK_FUNC(%s, L%d);\n", buf2, f->funcend);
        xstra_inst(code, "{\n");
        xstra_inst(code, "    vmfnc *callee = ctx->callee;\n");
        xstra_inst(code, "    ctx->callee = (%s)->f;\n", buf2);
        xstra_inst(code, "    e = ((vmfunc_t)(((%s)->f)->f))(ctx, ((%s)->f)->lex, %s, %d);\n", buf2, buf2, buf1, i->r2.args);
        xstra_inst(code, "    ctx->callee = callee;\n", i->r2.funcid);
        xstra_inst(code, "}\n");
    }
}

static void translate_mov(xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VAR: {
        char buf2[256] = {0};
        if (i->r1.typeid == TK_TSINT64 && i->r2.typeid == TK_TSINT64) {
            int_value(buf2, &(i->r2));
            xstra_inst(code, "SET_I64(%s, %s);\n", buf1, buf2);
        } else {
            var_value(buf2, &(i->r2));
            xstra_inst(code, "COPY_VAR_TO(ctx, %s, %s);\n", buf1, buf2);
        }
        break;
    }
    case TK_VSINT:
        xstra_inst(code, "SET_I64(%s, %" PRId64 ");\n", buf1, i->r2.i64);
        break;
    case TK_VDBL:
        xstra_inst(code, "SET_DBL(%s, %f);\n", buf1, i->r2.dbl);
        break;
    case TK_VSTR:
        xstra_inst(code, "SET_STR(%s, \"", buf1);
        escape_str(code, i->r2.str);
        xstrs(code, "\");\n");
        break;
    case TK_FUNC:
        xstra_inst(code, "vmfnc *f%d = alcfnc(ctx, %s, frm, 0);\n", i->r2.funcid, i->r2.name);
        xstra_inst(code, "SET_FNC(%s, f%d);\n", buf1, i->r2.funcid);
        break;
    default:
        break;
    }
}

static void translate_mova(xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VAR: {
        char buf2[256] = {0};
        if (i->r1.typeid == TK_TSINT64 && i->r2.typeid == TK_TSINT64) {
            int_value(buf2, &(i->r2));
            xstra_inst(code, "SET_I64((%s)->a, %s);\n", buf1, buf2);
        } else {
            var_value(buf2, &(i->r2));
            xstra_inst(code, "COPY_VAR_TO(ctx, (%s)->a, %s);\n", buf1, buf2);
        }
        break;
    }
    case TK_VSINT:
        xstra_inst(code, "SET_I64((%s)->a, %" PRId64 ");\n", buf1, i->r2.i64);
        break;
    case TK_VDBL:
        xstra_inst(code, "SET_DBL((%s)->a, %f);\n", buf1, i->r2.dbl);
        break;
    case TK_VSTR:
        xstra_inst(code, "SET_STR((%s)->a, \"", buf1);
        escape_str(code, i->r2.str);
        xstrs(code, "\");\n");
        break;
    case TK_FUNC:
        xstra_inst(code, "vmfnc *f%d = alcfnc(ctx, %s, frm, 0);\n", i->r2.funcid, i->r2.name);
        xstra_inst(code, "SET_FNC((%s)->a, f%d);\n", buf1, i->r2.funcid);
        break;
    default:
        break;
    }
    xstra_inst(code, "SHCOPY_VAR_TO(ctx, (%s), (%s)->a);\n", buf1, buf1);
    xstra_inst(code, "(%s)->a = NULL;\n", buf1);
}

static void translate_pure_hook(xstr *code, kl_kir_func *f)
{
    int args = f->arg_count;
    if (args > 0) {
        xstra_inst(code, "if ((n0->t == VAR_INT64)");
        for (int i = 1; i < args; ++i) {
            xstraf(code, " && (n%d->t == VAR_INT64)", i);
        }
        xstraf(code, " && (ctx->callee->n == 0 || (n0->i < ctx->callee->n");
        for (int i = 1; i < args; ++i) {
            xstraf(code, " && n%d->i < ctx->callee->n", i);
        }
        xstraf(code, "))) {\n");

        xstra_inst(code, "    int64_t error = 0;\n");
        xstra_inst(code, "    int64_t ret = %s__pure(&error, n0->i", f->funcname);
        for (int i = 1; i < args; ++i) {
            xstraf(code, ", n%d->i", i);
        }
        xstraf(code, ");\n");
        xstra_inst(code, "    if (error == 0) {\n");
        xstra_inst(code, "        SET_I64((r), ret);\n");
        xstra_inst(code, "        goto L%d;\n", f->funcend);
        xstra_inst(code, "    }\n");

        xstra_inst(code, "    ctx->callee->n = error;\n");
        xstra_inst(code, "}\n");
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
                var_value(buf2, &(i->r2));
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
    //     xstraf(code, "#line %d\n", i->line);
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
            xstra_inst(code, "CHECK_EXCEPTION(L%d);\n", f->funcend);
        }
        for (int idx = 0; idx < fctx->total_vars; ++idx) {
            xstra_inst(code, "vmvar *n%d = local_var(ctx, %d);\n", idx, idx);
        }
        for (int idx = 0; idx < fctx->arg_count; ++idx) {
            xstra_inst(code, "SET_ARGVAR(%d, %" PRId64 ");\n", idx, fctx->total_vars);
        }
        if (f->is_pure) {
            translate_pure_hook(code, f);
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
        xstra_inst(code, "vmfrm *frm = alcfrm(ctx, %" PRId64 ");\n", fctx->local_vars);
        xstra_inst(code, "push_frm(ctx, frm);\n");
        for (int idx = 0; idx < fctx->local_vars; ++idx) {
            xstra_inst(code, "vmvar *n%d = frm->v[%d] = alcvar_initial(ctx);\n", idx, idx);
        }
        if (fctx->total_vars > 0 && fctx->temp_count > 0) {
            xstra_inst(code, "const int allocated_local = %" PRId64 ";\n", fctx->temp_count);
            xstra_inst(code, "alloc_var(ctx, %" PRId64 ");\n", fctx->temp_count);
            xstra_inst(code, "CHECK_EXCEPTION(L%d);\n", f->funcend);
            for (int idx = fctx->local_vars; idx < fctx->total_vars; ++idx) {
                xstra_inst(code, "vmvar *n%d = local_var(ctx, %d);\n", idx, idx - fctx->local_vars);
            }
        }
        for (int idx = 0; idx < fctx->arg_count; ++idx) {
            xstra_inst(code, "SET_ARGVAR(%d, %" PRId64 ");\n", idx, fctx->temp_count);
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
        xstra_inst(code, "int p%" PRId64 " = vstackp(ctx);\n", i->r1.i64);
        break;
    case KIR_PUSHARG:
        translate_pushvar(code, &(i->r1));
        xstra_inst(code, "CHECK_EXCEPTION(L%d);\n", f->funcend);
        break;
    case KIR_CALL:
        translate_call(code, f, i);
        break;
    case KIR_RSSTKP:
        xstra_inst(code, "restore_vstackp(ctx, p%" PRId64 ");\n", i->r1.i64);
        break;
    case KIR_CHKEXCEPT:
        xstra_inst(code, "CHECK_EXCEPTION(L%d);\n", i->labelid);
        break;

    case KIR_RET:
        xstra_inst(code, "return e;\n");
        break;

    case KIR_JMPIFT:
        xstra_inst(code, "OP_JMP_IF_TRUE(%s, L%d);\n", var_value(buf1, &(i->r1)), i->labelid);
        break;
    case KIR_JMPIFF:
        xstra_inst(code, "OP_JMP_IF_FALSE(%s, L%d);\n", var_value(buf1, &(i->r1)), i->labelid);
        break;
    case KIR_JMP:
        xstra_inst(code, "goto L%d;\n", i->labelid);
        break;
    case KIR_LABEL:
        xstraf(code, "L%d:;\n", i->labelid);
        if (i->gcable) {
            xstra_inst(code, "GC_CHECK(ctx);\n");
        }
        break;

    case KIR_MOV:
        translate_mov(code, i);
        break;
    case KIR_MOVA:
        translate_mova(code, i);
        break;

    case KIR_ADD:
        translate_chkcnd(fctx, code, "ADD", "+", i);
        break;
    case KIR_SUB:
        translate_chkcnd(fctx, code, "SUB", "-", i);
        break;
    case KIR_MUL:
        translate_chkcnd(fctx, code, "MUL", "*", i);
        break;
    case KIR_DIV:
        translate_chkcnd(fctx, code, "DIV", "/", i);
        break;
    case KIR_MOD:
        translate_chkcnd(fctx, code, "mod", "%", i);
        break;

    case KIR_EQEQ:
        translate_chkcnd(fctx, code, "EQEQ", "==", i);
        break;
    case KIR_NEQ:
        translate_chkcnd(fctx, code, "NEQ", "!=", i);
        break;
    case KIR_LT:
        translate_chkcnd(fctx, code, "LT", "<", i);
        break;
    case KIR_LE:
        translate_chkcnd(fctx, code, "LE", "<=", i);
        break;
    case KIR_GT:
        translate_chkcnd(fctx, code, "GT", ">", i);
        break;
    case KIR_GE:
        translate_chkcnd(fctx, code, "GE", ">=", i);
        break;

    case KIR_INC:
        translate_incdec(fctx, code, "INC", "++", 0, i);
        break;
    case KIR_INCP:
        translate_incdec(fctx, code, "INCP", "++", 1, i);
        break;
    case KIR_DEC:
        translate_incdec(fctx, code, "DEC", "++", 0, i);
        break;
    case KIR_DECP:
        translate_incdec(fctx, code, "DECP", "++", 1, i);
        break;
    case KIR_MINUS:
        xstra_inst(code, "OP_UMINUS(ctx, %s, %s);\n", var_value(buf1, &(i->r1)), var_value(buf2, &(i->r2)));
        break;

    case KIR_NEWOBJ:
        xstra_inst(code, "SET_OBJ(%s, alcobj(ctx));\n", var_value(buf1, &(i->r1)));
        break;
    case KIR_MKSUPER:
        xstra_inst(code, "MAKE_SUPER(ctx, %s, %s);\n", var_value(buf1, &(i->r1)), var_value(buf2, &(i->r2)));
        break;

    case KIR_IDX:
        translate_idx(fctx, code, i, i->r3.typeid, 0);
        break;
    case KIR_IDXL:
        translate_idx(fctx, code, i, i->r3.typeid, 1);
        break;

    case KIR_APLY:
        translate_idx(fctx, code, i, TK_TSTR, 0);
        break;
    case KIR_APLYL:
        translate_idx(fctx, code, i, TK_TSTR, 1);
        break;
    }
}

void translate_func(kl_kir_program *p, xstr *code, kl_kir_func *f)
{
    if (f->is_pure) {
        translate_pure_func(p, code, f);
    }

    func_context fctx = {
        .has_frame = f->has_frame,
    };
    xstraf(code, "/* function:%s */\n", f->name);
    xstraf(code, "int %s(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)\n{\n", f->funcname);
    xstra_inst(code, "GC_CHECK(ctx);\n");
    xstra_inst(code, "int e = 0;\n\n");

    kl_kir_inst *i = f->head;
    kl_kir_inst *prev = NULL;
    while (i) {
        int blank = prev && prev->opcode != KIR_LABEL && i->opcode == KIR_LABEL;
        translate_inst(code, f, i, &fctx, blank);
        while (fctx.skip > 0) {
            fctx.skip--;
            i = i->next;
        }
        prev = i;
        i = i->next;
    }

    xstraf(code, "}\n\n");
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
        xstraf(&str, "#define USE_INT64\n");
        xstra(&str, header, len);
    }
    kl_kir_func *f = p->head;
    while (f) {
        translate_func(p, &str, f);
        f = f->next;
    }

    if (mode == TRANS_FULL) {
        xstraf(&str, "void setup_context(vmctx *ctx)\n{\n");
        xstra_inst(&str, "ctx->print_result = %d;\n", p->print_result);
        xstra_inst(&str, "ctx->verbose = %d;\n", p->verbose);
        xstraf(&str, "}\n");
    }

    return str.s;   /* this should be freed by the caller. */
}
