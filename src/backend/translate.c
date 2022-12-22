#include "../kir.h"
#include "header.h"
#include "translate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>

#define XSTR_UNIT (64)
#define xstra_inst(code, ...) xstra(code, "    ", 4), xstraf(code, __VA_ARGS__)
#define is_escape_ch(c) ((c) == '\a') || ((c) == '\b') || ((c) == '\x1b') || ((c) == '\f') || ((c) == '\n') || ((c) == '\r') || ((c) == '\t') || ((c) == '\v')
const char replace_escape_ch[256] = {
    0, 0, 0, 0, 0, 0, 0, 'a', 'b', 't', 'n', 'v', 'f', 'r', 0, 0,
    0, 0, 0, 0, 0, 0, 0,   0,   0,   0,   0,   0,   0,   0, 0, 0,
    0, 0, '"',
};

typedef struct xstr {
    int cap;
    int len;
    char *s;
} xstr;

typedef struct func_context {
    int has_frame;
    int total_vars;
    int local_vars;
    int argcount;
    int push_count;
    int push_max;
    int temp_count;
    int resume_start;
    int skip;
    xstr str;
} func_context;

static void clear_xstr(xstr *s)
{
    if (!s->s) {
        s->len = 0;
        s->cap = XSTR_UNIT;
        s->s = (char *)calloc(XSTR_UNIT, sizeof(char));
    } else {
        s->s[0] = 0;
        s->len = 0;
    }
}

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

static void escape_out(xstr *str, const char *s)
{
    while (*s) {
        if (*s == '"' || *s == '\\') {
            xstrc(str, '\\');
        }
        switch (*s) {
        case '\a':
            xstrs(str, "\\a");
            s++;
            break;
        case '\b':
            xstrs(str, "\\b");
            s++;
            break;
        case '\x1b':
            xstrs(str, "\\e");
            s++;
            break;
        case '\f':
            xstrs(str, "\\f");
            s++;
            break;
        case '\n':
            xstrs(str, "\\n");
            s++;
            break;
        case '\r':
            xstrs(str, "\\r");
            s++;
            break;
        case '\t':
            xstrs(str, "\\t");
            s++;
            break;
        case '\v':
            xstrs(str, "\\v");
            s++;
            break;
        default:
            xstrc(str, *s);
            s++;
        }
    }
}

static const char *escape(xstr *str, const char *s)
{
    clear_xstr(str);
    escape_out(str, s);
    return str->s;
}

static inline int is_var(kl_kir_opr *rn)
{
    return rn->t == TK_VAR;
}

static const char *var_value_pure(char *buf, kl_kir_opr *rn)  /* buf should have at least 256 bytes. */
{
    switch (rn->t) {
    case TK_VBOOL:
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

static void translate_pusharg_pure(xstr *code, func_context *fctx, kl_kir_opr *rn)
{
    int pushed = fctx->push_count++;
    const char *typestr = (fctx->push_max <= pushed) ? "int64_t " : "";
    char buf1[256] = {0};
    switch (rn->t) {
    case TK_VBOOL:
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
        if (f->argcount > 0) {
            for (int i = f->argcount - 1; i >= 0; --i) {
                xstraf(code, ", t%d", i);
            }
            fctx->push_count -= f->argcount;
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
        xstra_inst(code, "<ERROR:%d>", __LINE__);
        /* TODO: error */
    }
}

static const char *get_min(kl_kir_func *f, char *buf)
{
    sprintf(buf, "get_min%d(n0", f->argcount);
    int p = strlen(buf);
    for (int i = 1; i < f->argcount; ++i) {
        sprintf(buf+p, ", n%d", i);
        p = strlen(buf);
    }
    sprintf(buf+p, ")");
    return buf;
}

static void translate_op3_pure(func_context *fctx, xstr *code, kl_kir_func *f, const char *op, kl_kir_inst *i)
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
    if (f->argcount == 0) {
        xstra_inst(code, "OP_N%s_I_I(e, 1, %s, %s, %s, L%d);\n", op, buf1, buf2, buf3, f->funcend);
    } else if (f->argcount == 1) {
        xstra_inst(code, "OP_N%s_I_I(e, n0, %s, %s, %s, L%d);\n", op, buf1, buf2, buf3, f->funcend);
    } else {
        xstra_inst(code, "{ int64_t min = %s;\n", get_min(f, buf4));
        xstra_inst(code, "  OP_N%s_I_I(e, min, %s, %s, %s, L%d); }\n", op, buf1, buf2, buf3, f->funcend);
    }
}

static int translate_chkcnd_precheck_pure(func_context *fctx, xstr *code, kl_kir_func *f, const char *sop, kl_kir_inst *i)
{
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
        return 1;
    }
    return 0;
}

static void translate_chkcnd_pure(func_context *fctx, xstr *code, kl_kir_func *f, const char *op, const char *sop, kl_kir_inst *i)
{
    if (sop && i->next && (i->next->opcode == KIR_JMPIFT || i->next->opcode == KIR_JMPIFF)) {
        if (translate_chkcnd_precheck_pure(fctx, code, f, sop, i) > 0) {
            return;
        }
    }
    translate_op3_pure(fctx, code, f, op, i);
}

static void translate_op3_direct_pure(func_context *fctx, xstr *code, kl_kir_func *f, const char *sop, kl_kir_inst *i)
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
    xstra_inst(code, "%s = %s %s %s;\n", buf1, buf2, sop, buf3);
}

static void translate_chkcnd_direct_pure(func_context *fctx, xstr *code, kl_kir_func *f, const char *sop, kl_kir_inst *i)
{
    if (sop && i->next && (i->next->opcode == KIR_JMPIFT || i->next->opcode == KIR_JMPIFF)) {
        if (translate_chkcnd_precheck_pure(fctx, code, f, sop, i) > 0) {
            return;
        }
    }
    translate_op3_direct_pure(fctx, code, f, sop, i);
}

static void translate_incdec_pure(func_context *fctx, xstr *code, const char *sop, int is_postfix, kl_kir_inst *i)
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

static void translate_bnot_pure(xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value_pure(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VAR: {
        char buf2[256] = {0};
        var_value_pure(buf2, &(i->r2));
        xstra_inst(code, "%s = ~%s;\n", buf1, buf2);
        break;
    }
    case TK_VBOOL:
    case TK_VSINT:
        xstra_inst(code, "%s = %" PRId64 ";\n", buf1, ~(i->r2.i64));
        break;
    default:
        break;
    }
}

static void translate_not_pure(xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value_pure(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VAR: {
        char buf2[256] = {0};
        var_value_pure(buf2, &(i->r2));
        xstra_inst(code, "%s = !%s;\n", buf1, buf2);
        break;
    }
    case TK_VBOOL:
    case TK_VSINT:
        xstra_inst(code, "%s = %" PRId64 ";\n", buf1, i->r2.i64 == 0 ? 1 : 0);
        break;
    default:
        break;
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
    case TK_VBOOL:
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
        fctx->argcount = i->r3.i64;
        xstra_inst(code, "int64_t r = 0;\n");
        for (int idx = 0; idx < fctx->total_vars; ++idx) {
            if (idx >= fctx->argcount) {
                xstra_inst(code, "int64_t n%d = 0;\n", idx);
            }
        }
        xstra_inst(code, "\n");
        break;

    case KIR_PUSHARG:
        translate_pusharg_pure(code, fctx, &(i->r1));
        break;
    case KIR_CALL:
        translate_call_pure(code, f, fctx, i);
        break;

    case KIR_RET:
        xstra_inst(code, "return r;\n");
        break;

    case KIR_JMPIFT:
        xstra_inst(code, "if (%s) goto L%d;\n", var_value_pure(buf1, &(i->r1)), i->labelid);
        break;
    case KIR_JMPIFF:
        xstra_inst(code, "if (!(%s)) goto L%d;\n", var_value_pure(buf1, &(i->r1)), i->labelid);
        break;
    case KIR_JMP:
        xstra_inst(code, "goto L%d;\n", i->labelid);
        break;
    case KIR_LABEL:
        xstraf(code, "L%d:;\n", i->labelid);
        break;

    case KIR_NOT:
        translate_not_pure(code, i);
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
        translate_chkcnd_pure(fctx, code, f, "MOD", "%", i);
        break;
    case KIR_POW:
        translate_chkcnd_pure(fctx, code, f, "POW", NULL, i);
        break;
    case KIR_BNOT:
        translate_bnot_pure(code, i);
        break;
    case KIR_BSHL:
        translate_chkcnd_direct_pure(fctx, code, f, "<<", i);
        break;
    case KIR_BSHR:
        translate_chkcnd_direct_pure(fctx, code, f, ">>", i);
        break;
    case KIR_BAND:
        translate_chkcnd_direct_pure(fctx, code, f, "&", i);
        break;
    case KIR_BOR:
        translate_chkcnd_direct_pure(fctx, code, f, "|", i);
        break;
    case KIR_BXOR:
        translate_chkcnd_direct_pure(fctx, code, f, "^", i);
        break;

    case KIR_EQEQ:
        translate_chkcnd_direct_pure(fctx, code, f, "==", i);
        break;
    case KIR_NEQ:
        translate_chkcnd_direct_pure(fctx, code, f, "!=", i);
        break;
    case KIR_LT:
        translate_chkcnd_direct_pure(fctx, code, f, "<", i);
        break;
    case KIR_LE:
        translate_chkcnd_direct_pure(fctx, code, f, "<=", i);
        break;
    case KIR_GT:
        translate_chkcnd_direct_pure(fctx, code, f, ">", i);
        break;
    case KIR_GE:
        translate_chkcnd_direct_pure(fctx, code, f, ">=", i);
        break;

    case KIR_INC:
        translate_incdec_pure(fctx, code, "++", 0, i);
        break;
    case KIR_INCP:
        translate_incdec_pure(fctx, code, "++", 1, i);
        break;
    case KIR_DEC:
        translate_incdec_pure(fctx, code, "--", 0, i);
        break;
    case KIR_DECP:
        translate_incdec_pure(fctx, code, "--", 1, i);
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
    for (int i = 0; i < f->argcount; ++i) {
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
    case TK_VBOOL:
    case TK_VSINT:
        sprintf(buf, "%" PRId64, rn->i64);
        break;
    case TK_VDBL:
        sprintf(buf, "%s", rn->dbl);
        break;
    case TK_VSTR: {
        char *p = buf;
        *p++ = '"';
        const char *s = rn->str;
        while (*s) {
            if (replace_escape_ch[*s] != 0) {
                *p++ = '\\';
                *p++ = replace_escape_ch[*s];
            } else {
                *p++ = *s++;
            }
        }
        *p++ = '"';
        *p = 0;
        break;
    }
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

static void translate_pusharg(func_context *fctx, xstr *code, kl_kir_inst *i, int catchid)
{
    kl_kir_opr *rn = &(i->r1);
    char buf1[256] = {0};
    switch (rn->t) {
    case TK_VBOOL:
        xstra_inst(code, "{ push_var_l(ctx, %" PRId64 ", L%d, \"%s\", \"%s\", %d); }\n",
            rn->i64, catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VSINT:
        xstra_inst(code, "{ push_var_i(ctx, %" PRId64 ", L%d, \"%s\", \"%s\", %d); }\n",
            rn->i64, catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VBIGINT:
        xstra_inst(code, "{ push_var_b(ctx, \"%s\", L%d, \"%s\", \"%s\", %d); }\n",
            rn->str, catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VDBL:
        xstra_inst(code, "{ push_var_d(ctx, %s, L%d, \"%s\", \"%s\", %d); }\n",
            rn->dbl, catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VSTR:
        xstra_inst(code, "{ push_var_s(ctx, \"%s\", L%d, \"%s\", \"%s\", %d); }\n",
            escape(&(fctx->str), rn->str), catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VAR:
        if (rn->has_dot3) {
            xstra_inst(code, "{ push_var_a(ctx, %s, ad%d, L%d, \"%s\", \"%s\", %d); }\n",
                var_value(buf1, rn), rn->callcnt, catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        } else {
            xstra_inst(code, "{ push_var(ctx, %s, L%d, \"%s\", \"%s\", %d); }\n",
                var_value(buf1, rn), catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        }
        break;
    case TK_FUNC:
        xstra_inst(code, "{ push_var_f(ctx, f%d, L%d, \"%s\", \"%s\", %d); }\n",
            rn->funcid, catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    default:
        xstra_inst(code, "<ERROR:%d>", __LINE__);
        /* TODO: error */
        break;
    }
}

static void translate_incdec(func_context *fctx, xstr *code, const char *op, const char *sop, kl_kir_inst *i)
{
    kl_kir_opr *r1 = &(i->r1);
    kl_kir_opr *r2 = &(i->r2);

    char buf1[256] = {0};
    char buf2[256] = {0};

    var_value(buf2, r2);
    if (r1->prevent) {
        xstra_inst(code, "OP_%s_SAME(ctx, %s, L%d, \"%s\", \"%s\", %d);\n", op, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else {
        var_value(buf1, r1);
        xstra_inst(code, "OP_%s(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", op, buf1, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    }
}

static void translate_setbin(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));

    switch (i->r3.t) {
    case TK_VBOOL:
    case TK_VSINT:
        xstra_inst(code, "SET_BIN_DATA_I(ctx, %s, %d, %" PRId64 ");\n", buf1, i->r2.i64, i->r3.i64);
        break;
    case TK_VDBL:
        xstra_inst(code, "SET_BIN_DATA_I(ctx, %s, %d, %s);\n", buf1, i->r2.i64, i->r3.dbl);
        break;
    case TK_VSTR:
        xstra_inst(code, "SET_BIN_DATA_I(ctx, %s, %d, %d);\n", buf1, i->r2.i64, i->r3.str[0]);
        break;
    case TK_VAR:
        var_value(buf2, &(i->r3));
        xstra_inst(code, "SET_BIN_DATA(ctx, %s, %d, %s, L%d, \"%s\", \"%s\", %d);\n",
            buf1, i->r2.i64, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    default:
        xstra_inst(code, "<ERROR:%d>", __LINE__);
        /* TODO: error */
        break;
    }
}

static void translate_idxfrm(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    char buf3[256] = {0};
    var_value(buf1, &(i->r1));
    var_value(buf2, &(i->r2));
    xstra_inst(code, "OP_ARRAY_REF_IDXFRM(ctx, %s, %s, %s);\n", buf1, buf2, int_value(buf3, &(i->r3)));
}

static void translate_idx(func_context *fctx, xstr *code, kl_kir_inst *i, int r3typeid, int lvalue)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    char buf3[256] = {0};
    var_value(buf1, &(i->r1));
    var_value(buf2, &(i->r2));
    switch (i->r3.t) {
    case TK_VBOOL:
    case TK_VSINT:
        xstra_inst(code, "OP_ARRAY_REF%s_I(ctx, %s, %s, %" PRId64 ");\n", (lvalue ? "L" : ""), buf1, buf2, i->r3.i64);
        break;
    case TK_VDBL:
        xstra_inst(code, "OP_ARRAY_REF%s_I(ctx, %s, %s, ((int)%s));\n", (lvalue ? "L" : ""), buf1, buf2, i->r3.dbl);
        break;
    case TK_VSTR:
        xstra_inst(code, "OP_HASH_APPLY%s(ctx, %s, %s, \"%s\");\n", (lvalue ? "L" : ""), buf1, buf2, escape(&(fctx->str), i->r3.str));
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

static void translate_casev(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    kl_kir_opr *r1 = &(i->r1);
    kl_kir_opr *r2 = &(i->r2);
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, r1);

    xstra_inst(code, "{ vmvar sv = {0};\n");
    switch (i->r2.t) {
    case TK_VDBL:
        xstra_inst(code, "  vmvar cv = {0}; SET_DBL(&cv, %s);\n", i->r2.dbl);
        xstra_inst(code, "  OP_EQEQ(ctx, &sv, %s, &cv, L%d, \"%s\", \"%s\", %d);\n",
            buf1, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VSTR:
        xstra_inst(code, "  vmvar cv = {0}; SET_STR(&cv, \"%s\");\n", escape(&(fctx->str), i->r2.str));
        xstra_inst(code, "  OP_EQEQ(ctx, &sv, %s, &cv, L%d, \"%s\", \"%s\", %d);\n",
            buf1, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VAR:
        var_value(buf2, r2);
        xstra_inst(code, "  OP_EQEQ(ctx, &sv, %s, %s, L%d, \"%s\", \"%s\", %d);\n",
            buf1, buf2, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    default:
        ;
    }
    xstra_inst(code, "  OP_JMP_IF_TRUE(&sv, L%d);\n", i->labelid);
    xstra_inst(code, "}\n");
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
    if (sop && r2->typeid == TK_TSINT64 && r3->typeid == TK_TSINT64) {
        int_value(buf2, r2);
        int_value(buf3, r3);
        xstra_inst(code, "SET_I64(%s, (%s) %s (%s));\n", buf1, buf2, sop, buf3);
    } else {
        var_value(buf2, r2);
        var_value(buf3, r3);
        if (r2->t == TK_VSINT) {
            if (r3->t == TK_VSINT) {
                xstra_inst(code, "OP_%s_I_I(ctx, %s, %s, %s, L%d, \"%s\", \"%s\", %d);\n",
                    op, buf1, buf2, buf3, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
            } else {
                xstra_inst(code, "OP_%s_I_V(ctx, %s, %s, %s, L%d, \"%s\", \"%s\", %d);\n",
                    op, buf1, buf2, buf3, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
            }
        } else {
            if (r3->t == TK_VSINT) {
                xstra_inst(code, "OP_%s_V_I(ctx, %s, %s, %s, L%d, \"%s\", \"%s\", %d);\n",
                    op, buf1, buf2, buf3, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
            } else {
                xstra_inst(code, "OP_%s(ctx, %s, %s, %s, L%d, \"%s\", \"%s\", %d);\n",
                    op, buf1, buf2, buf3, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
            }
        }
    }
}

static void translate_chkcnd(func_context *fctx, xstr *code, const char *op, const char *sop, kl_kir_inst *i)
{
    if (sop && i->next && (i->next->opcode == KIR_JMPIFT || i->next->opcode == KIR_JMPIFF)) {
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

static void translate_regexp(func_context *fctx, xstr *code, kl_kir_inst *i, const char *method, int str)
{
    char buf1[256] = {0};
    if (str) {
        xstra_inst(code, "push_var_s(ctx, \"%s\", ", escape(&(fctx->str), i->r3.str));
        xstraf(code, "L%d, \"%s\", \"%s\", %d);\n", i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        xstra_inst(code, "push_var_s(ctx, \"%s\", ", escape(&(fctx->str), i->r2.str));
        xstraf(code, "L%d, \"%s\", \"%s\", %d);\n", i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else {
        xstra_inst(code, "push_var(ctx, %s, ", var_value(buf1, &(i->r3)));
        xstraf(code, "L%d, \"%s\", \"%s\", %d);\n", i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        xstra_inst(code, "push_var(ctx, %s, ", var_value(buf1, &(i->r2)));
        xstraf(code, "L%d, \"%s\", \"%s\", %d);\n", i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    }
    xstra_inst(code, "CALL(ctx->%s, ctx->%s->lex, %s, 2);\n", method, method, var_value(buf1, &(i->r1)));
    xstra_inst(code, "reduce_vstackp(ctx, 2);\n");
}

static void translate_call(func_context *fctx, xstr *code, kl_kir_func *f, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));
    var_value(buf2, &(i->r2));
    if (i->r2.funcid > 0) {
        if (i->r2.t == TK_FUNC) {
            /* This means a direct call of an anonymous function. */
            xstra_inst(code, "vmfnc *f%d = alcfnc(ctx, %s, frm, \"%s\", 0);\n", i->r2.funcid, i->r2.name, i->r2.str);
        }
        if (i->r2.recursive) {
            xstra_inst(code, "CALL(f%d, lex, %s, %d + ad%d)\n", i->r2.funcid, buf1, i->r2.args, i->r2.callcnt);
        } else {
            xstra_inst(code, "CALL(f%d, f%d->lex, %s, %d + ad%d)\n", i->r2.funcid, i->r2.funcid, buf1, i->r2.args, i->r2.callcnt);
        }
    } else {
        int tclabel = i->catchid > 0 ? i->catchid : f->funcend;
        xstra_inst(code, "CHECK_CALL(ctx, %s, L%d, %s, %d + ad%d, \"%s\", \"%s\", %d);\n",
            buf2, tclabel, buf1, i->r2.args, i->r2.callcnt,
            i->funcname, escape(&(fctx->str), i->filename), i->line);
    }
}

static void translate_yield(func_context *fctx, xstr *code, kl_kir_func *f, kl_kir_inst *i)
{
    if (f->has_frame) {
        xstra_inst(code, "YIELD_FRM(ctx, cur, %d, %d, SAVE_LOCAL());\n\n", i->labelid, fctx->total_vars);
    } else {
        xstra_inst(code, "YIELD(ctx, %d, %d, SAVE_LOCAL());\n\n", i->labelid, fctx->total_vars);
    }
    xstraf(code, "Y%d:;\n", i->labelid);
    xstra_inst(code, "ctx->callee->yield = 0;\n");
}

static void translate_resume(func_context *fctx, xstr *code, kl_kir_func *f, kl_kir_inst *i)
{
    xstra_inst(code, "RESUME(ctx, n%d, ac, allocated_local, L%d, \"%s\", \"%s\", %d);\n",
        i->r1.index, f->funcend, i->funcname, escape(&(fctx->str), i->filename), i->line);
}

static void translate_yield_check(func_context *fctx, xstr *code, kl_kir_func *f, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));
    var_value(buf2, &(i->r2));
    if (!f->is_global && f->yield > 0) {
        if (i->labelid > 0) {
            /* There is the case when returned back by yield. */
            if (i->r2.funcid > 0) {
                if (f->has_frame) {
                    xstra_inst(code, "CHECK_YIELD_FRM(ctx, %s, frm, f%d, %d, %d, SAVE_LOCAL());\n",
                        buf1, i->r2.funcid, i->labelid, fctx->temp_count);
                } else {
                    xstra_inst(code, "CHECK_YIELD(ctx, %s, f%d, %d, %d, SAVE_LOCAL());\n",
                        buf1, i->r2.funcid, i->labelid, fctx->temp_count);
                }
            } else {
                if (f->has_frame) {
                    xstra_inst(code, "CHECK_YIELD_FRM(ctx, %s, frm, (%s)->f, %d, %d, SAVE_LOCAL());\n",
                        buf1, buf2, i->labelid, fctx->temp_count);
                } else {
                    xstra_inst(code, "CHECK_YIELD(ctx, %s, (%s)->f, %d, %d, SAVE_LOCAL());\n",
                        buf1, buf2, i->labelid, fctx->temp_count);
                }
            }
            xstraf(code, "\nY%d:;\n", i->labelid);
            xstra_inst(code, "RESUME_SETUP(%d, %s, L%d, \"%s\", \"%s\", %d)\n",
                i->labelid, buf1, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        }
    }
}

static void translate_bnot(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));
    if (i->r2.t == TK_VSINT) {
        int_value(buf2, &(i->r2));
        xstra_inst(code, "OP_BNOT_I(ctx, %s, %d, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else {
        var_value(buf2, &(i->r2));
        xstra_inst(code, "OP_BNOT_V(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    }
}

static void translate_not(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));
    if (i->r2.t == TK_VSINT) {
        int_value(buf2, &(i->r2));
        xstra_inst(code, "OP_NOT_I(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else {
        var_value(buf2, &(i->r2));
        xstra_inst(code, "OP_NOT_V(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    }
}

static void translate_mov(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value(buf1, &(i->r1));
    if (i->r1.index < 0 && i->r2.index < 0) {
        xstra_inst(code, "SET_UNDEF(%s);\n", buf1);
        return;
    }

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
    case TK_VBOOL:
        xstra_inst(code, "SET_BOOL(%s, %" PRId64 ");\n", buf1, i->r2.i64);
        break;
    case TK_VSINT:
        xstra_inst(code, "SET_I64(%s, %" PRId64 ");\n", buf1, i->r2.i64);
        break;
    case TK_VBIGINT:
        xstra_inst(code, "SET_BIG(%s, \"%s\");\n", buf1, i->r2.str);
        break;
    case TK_VDBL:
        xstra_inst(code, "SET_DBL(%s, %s);\n", buf1, i->r2.dbl);
        break;
    case TK_VSTR:
        xstra_inst(code, "SET_STR(%s, \"", buf1);
        escape_out(code, i->r2.str);
        xstrs(code, "\");\n");
        break;
    case TK_FUNC:
        xstra_inst(code, "vmfnc *f%d = alcfnc(ctx, %s, frm, \"%s\", 0);\n", i->r2.funcid, i->r2.name, i->r2.str);
        xstra_inst(code, "SET_FNC(%s, f%d);\n", buf1, i->r2.funcid);
        /* 6 means kl000_xxxxx is an actual function name. See `make_func_name` function in parse.c */
        if (strcmp(i->r2.name, "methodMissing") == 0 || strcmp(i->r2.name + 6, "methodMissing") == 0) {
            xstra_inst(code, "ctx->methodmissing = f%d;\n", i->r2.funcid);
        }
        break;
    default:
        break;
    }
}

static void translate_mova(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VAR: {
        char buf2[256] = {0};
        if (i->r1.typeid == TK_TSINT64 && i->r2.typeid == TK_TSINT64) {
            int_value(buf2, &(i->r2));
            xstra_inst(code, "SET_APPLY_I(ctx, (%s), %s, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2,
                i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        } else {
            var_value(buf2, &(i->r2));
            xstra_inst(code, "SET_APPLY(ctx, (%s), %s, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2,
                i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        }
        break;
    }
    case TK_VBOOL:
    case TK_VSINT:
        xstra_inst(code, "SET_APPLY_I(ctx, (%s), %" PRId64 ", L%d, \"%s\", \"%s\", %d);\n", buf1, i->r2.i64,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VDBL:
        xstra_inst(code, "SET_APPLY_D(ctx, (%s), %s, L%d, \"%s\", \"%s\", %d);\n", buf1, i->r2.dbl,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VSTR:
        xstra_inst(code, "SET_APPLY_S(ctx, (%s), \"%s\", ", buf1, escape(&(fctx->str), i->r2.str));
        xstraf(code, "L%d, \"%s\", \"%s\", %d);\n", i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_FUNC:
        xstra_inst(code, "vmfnc *f%d = alcfnc(ctx, %s, frm, \"%s\", 0);\n", i->r2.funcid, i->r2.name, i->r2.str);
        xstra_inst(code, "SET_APPLY_F(ctx, (%s), f%d, L%d, \"%s\", \"%s\", %d);\n", buf1, i->r2.funcid,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        if (strcmp(i->r2.name, "methodMissing") == 0) {
            xstra_inst(code, "ctx->methodmissing = f%d;\n", i->r2.funcid);
        }
        break;
    default:
        // TODO: error
        break;
    }
}

static void translate_type(xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));
    var_value(buf2, &(i->r2));
    if (i->r3.i64 == VAR_DEF) {
        xstra_inst(code, "SET_I64((%s), ((%s)->t != VAR_UNDEF));\n", buf1, buf2);
    } else if (i->r3.i64 == VAR_ARY) {
        xstra_inst(code, "SET_I64((%s), ((%s)->t == VAR_OBJ && (%s)->o->idxsz > 0));\n", buf1, buf2, buf2);
    } else if (i->r3.i64 == VAR_INT64) {
        xstra_inst(code, "SET_I64((%s), (((%s)->t == VAR_INT64) || ((%s)->t == VAR_BIG)));\n", buf1, buf2, buf2);
    } else {
        xstra_inst(code, "SET_I64((%s), ((%s)->t == %s));\n", buf1, buf2, i->r3.str);
    }
}

static void translate_swap(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));
    var_value(buf2, &(i->r2));
    xstra_inst(code, "{ vmvar tmp; SHCOPY_VAR_TO(ctx, (&tmp), %s); SHCOPY_VAR_TO(ctx, %s, %s); SHCOPY_VAR_TO(ctx, %s, (&tmp)); }\n",
        buf1, buf1, buf2, buf2
    );
}

static void translate_swapa(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));
    var_value(buf2, &(i->r2));
    xstra_inst(code, "{ vmvar tmp; SHCOPY_VAR_TO(ctx, (&tmp), (%s)->a); SHCOPY_VAR_TO(ctx, (%s)->a, (%s)->a); SHCOPY_VAR_TO(ctx, (%s)->a, (&tmp)); }\n",
        buf1, buf1, buf2, buf2
    );
    xstra_inst(code, "SHMOVE_VAR_TO(ctx, (%s), (%s)->a); SHMOVE_VAR_TO(ctx, (%s), (%s)->a);\n",
        buf1, buf1, buf2, buf2
    );
}

static void translate_pushn(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value(buf1, &(i->r1));
    xstra_inst(code, "VALUE_PUSHN(ctx, %s, L%d, \"%s\", \"%s\", %d);\n", buf1,
        i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
}

static void translate_push(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VAR: {
        char buf2[256] = {0};
        var_value(buf2, &(i->r2));
        xstra_inst(code, "VALUE_PUSH(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    }
    case TK_VBOOL:
        xstra_inst(code, "VALUE_PUSH_BOOL(ctx, %s, %" PRId64 ", L%d, \"%s\", \"%s\", %d);\n", buf1, i->r2.i64,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VSINT:
        xstra_inst(code, "VALUE_PUSH_I(ctx, %s, %" PRId64 ", L%d, \"%s\", \"%s\", %d);\n", buf1, i->r2.i64,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VDBL:
        xstra_inst(code, "VALUE_PUSH_D(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", buf1, i->r2.dbl,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VSTR:
        xstra_inst(code, "VALUE_PUSH_S(ctx, %s, \"%s\", ", buf1, escape(&(fctx->str), i->r2.str));
        xstraf(code, "L%d, \"%s\", \"%s\", %d);\n", i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    default:
        // TODO: error
        break;
    }
}

static void translate_pushx(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VAR: {
        char buf2[256] = {0};
        var_value(buf2, &(i->r2));
        xstra_inst(code, "VALUE_PUSHX(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    }
    case TK_VSTR:
        xstra_inst(code, "VALUE_PUSHX_S(ctx, %s, \"%s\", ", buf1, escape(&(fctx->str), i->r2.str));
        xstraf(code, "L%d, \"%s\", \"%s\", %d);\n", i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    default:
        // TODO: error
        break;
    }
}

static void translate_expand(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VAR: {
        char buf2[256] = {0};
        var_value(buf2, &(i->r2));
        xstra_inst(code, "VALUE_EXPAND(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    }
    default:
        // TODO: error
        break;
    }
}

static void translate_range(func_context *fctx, xstr *code, kl_kir_inst *i, int excl)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    char buf3[256] = {0};
    if (i->r2.t == TK_VSINT && i->r3.t == TK_VSINT) {
        var_value(buf1, &(i->r1));
        int_value(buf2, &(i->r2));
        int_value(buf3, &(i->r3));
        xstra_inst(code, "MAKE_RANGE_I_I(ctx, %s, %s, %s, %d, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2, buf3, excl,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else if (i->r2.t == TK_VSINT && i->r3.t == TK_UNKNOWN) {
        var_value(buf1, &(i->r1));
        int_value(buf2, &(i->r2));
        xstra_inst(code, "MAKE_RANGE_I_N(ctx, %s, %s, %d, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2, excl,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else if (i->r2.t == TK_UNKNOWN && i->r3.t == TK_VSINT) {
        var_value(buf1, &(i->r1));
        int_value(buf2, &(i->r3));
        xstra_inst(code, "MAKE_RANGE_N_I(ctx, %s, %s, %d, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2, excl,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else if (i->r2.t == TK_VSTR && i->r3.t == TK_VSTR) {
        var_value(buf1, &(i->r1));
        var_value(buf2, &(i->r2));
        var_value(buf3, &(i->r3));
        xstra_inst(code, "MAKE_RANGE_S_S(ctx, %s, %s, %s, %d, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2, buf3, excl,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else if (i->r2.t == TK_VSTR && i->r3.t == TK_UNKNOWN) {
        var_value(buf1, &(i->r1));
        var_value(buf2, &(i->r2));
        xstra_inst(code, "MAKE_RANGE_S_N(ctx, %s, %s, %d, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2, excl,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else if (i->r2.t == TK_UNKNOWN && i->r3.t == TK_VSTR) {
        var_value(buf1, &(i->r1));
        var_value(buf2, &(i->r3));
        xstra_inst(code, "MAKE_RANGE_N_S(ctx, %s, %s, %d, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2, excl,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else {
        var_value(buf1, &(i->r1));
        var_value(buf2, &(i->r2));
        var_value(buf3, &(i->r3));
        xstra_inst(code, "MAKE_RANGE(ctx, %s, %s, %s, %d, L%d, \"%s\", \"%s\", %d);\n", buf1, buf2, buf3, excl,
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    }
}

static void translate_chkmatch(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    var_value(buf1, &(i->r1));
    switch (i->r2.t) {
    case TK_VBOOL:
        xstra_inst(code, "CHKMATCH_BOOL(%s, %" PRId64 ", L%d, \"%s\", \"%s\", %d);\n",
            buf1, i->r2.i64, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VSINT:
        xstra_inst(code, "CHKMATCH_I64(%s, %" PRId64 ", L%d, \"%s\", \"%s\", %d);\n",
            buf1, i->r2.i64, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VDBL:
        xstra_inst(code, "CHKMATCH_DBL(%s, %s, L%d, \"%s\", \"%s\", %d);\n",
            buf1, i->r2.dbl, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case TK_VSTR:
        xstra_inst(code, "CHKMATCH_STR(%s, \"%s\", L%d, \"%s\", \"%s\", %d);\n",
            buf1, escape(&(fctx->str), i->r2.str), i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    }
}

static void translate_chkrange(func_context *fctx, xstr *code, kl_kir_inst *i)
{
    char buf1[256] = {0};
    char buf2[256] = {0};
    var_value(buf1, &(i->r1));
    var_value(buf2, &(i->r2));
    xstra_inst(code, "CHECK_RANGE(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n",
        buf1, buf2, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
}

static void set_last_args(xstr *code, kl_kir_func *f, int idx, int total)
{
    xstra_inst(code, "SET_OBJ(n%d, alcobj(ctx));\n", idx);
    xstra_inst(code, "for (int i = 0, idx = %d; idx < ac; ++i, ++idx) {\n", total);
    xstra_inst(code, "    vmvar *aa = local_var(ctx, idx + allocated_local);\n");
    xstra_inst(code, "    array_set(ctx, n%d->o, i, copy_var(ctx, aa, 0));\n", idx);
    xstra_inst(code, "}\n");
}

static void translate_pure_hook(xstr *code, kl_kir_func *f)
{
    int args = f->argcount;
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

static void translate_resume_hook(func_context *fctx, xstr *code, kl_kir_func *f, kl_kir_inst *i)
{
    int yield = i->labelid;
    int total_vars = fctx->total_vars;
    xstraf(code, "RESUMEHOOK:;\n");
    for (int idx = 0; idx < fctx->resume_start; ++idx) {
        xstra_inst(code, "n%d = frm->v[%d];\n", idx, idx);
    }
    for (int idx = fctx->resume_start; idx < total_vars; ++idx) {
        xstra_inst(code, "n%d = ctx->callee->vars[%d];\n", idx, idx - fctx->resume_start);
    }
    xstra_inst(code, "RESUME_HOOK(ctx, ac, allocated_local, L%d, \"%s\", \"%s\", %d);\n",
        i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
    xstra_inst(code, "switch (yieldno) {\n");
    for (int idx = 1; idx <= yield; ++idx) {
        xstra_inst(code, "case %d: goto Y%d;\n", idx, idx);
    }
    xstra_inst(code, "default: e = throw_system_exception(__LINE__, ctx, EXCEPT_INVALID_FIBER_STATE, \"Unknown yield number\"); goto L%d;\n", f->funcend);
    xstra_inst(code, "}\n");
    xstraf(code, "\nHEAD:;\n");
}

static void translate_alocal(func_context *fctx, xstr *code, kl_kir_func *f, kl_kir_inst *i)
{
    fctx->total_vars = i->r1.i64;
    fctx->local_vars = i->r2.i64;   // including arguments.
    fctx->temp_count = fctx->total_vars;
    fctx->resume_start = 0;

    if (!f->is_global && f->yield > 0) {
        xstra_inst(code, "#define SAVE_LOCAL() {");
        for (int i = 0; i < fctx->total_vars; ++i) {
            xstraf(code, " SAVEN(%d, n%d);", i, i);
        }
        xstraf(code, " }\n");
    }

    if (!f->is_global && f->yield > 0) {
        xstra_inst(code, "int yieldno = ctx->callee->yield;\n");
    }
    xstra_inst(code, "const int allocated_local = %" PRId64 ";\n", fctx->total_vars);
    if (fctx->total_vars > 0) {
        xstra_inst(code, "int pstop = vstackp(ctx);\n");
        xstra_inst(code, "alloc_var(ctx, allocated_local, L%d, \"%s\", \"%s\", %d);\n",
            f->funcend, i->funcname, escape(&(fctx->str), i->filename), i->line);
        xstra_inst(code, "vmvar ");
        for (int idx = 0; idx < fctx->total_vars; ++idx) {
            if (idx != 0) xstraf(code, ", ");
            xstraf(code, "*n%d", idx);
        }
        xstraf(code, ";\n");
    }
    if (!f->is_global && f->yield > 0) {
        xstra_inst(code, "vmvar yy = {0}; SET_UNDEF(&yy);\n");
        xstra_inst(code, "if (yieldno > 0) goto RESUMEHOOK;\n");
    }
    for (int idx = 0; idx < fctx->total_vars; ++idx) {
        xstra_inst(code, "n%d = local_var(ctx, %d);\n", idx, idx);
    }
}

static void translate_mkfrm(func_context *fctx, xstr *code, kl_kir_func *f, kl_kir_inst *i)
{
    fctx->total_vars = i->r1.i64;
    fctx->local_vars = i->r2.i64;   // including arguments.
    fctx->temp_count = fctx->total_vars - fctx->local_vars;
    fctx->resume_start = fctx->local_vars;

    if (!f->is_global && f->yield > 0) {
        xstra_inst(code, "#define SAVE_LOCAL() {");
        for (int i = fctx->local_vars; i < fctx->total_vars; ++i) {
            xstraf(code, " SAVEN(%d, n%d);", i - fctx->local_vars, i);
        }
        xstraf(code, " }\n");
    }

    if (!f->is_global && f->yield > 0) {
        xstra_inst(code, "int yieldno = ctx->callee->yield;\n");
    }
    xstra_inst(code, "vmfrm *frm;\n", fctx->local_vars);
    xstra_inst(code, "vmvar ");
    for (int idx = 0; idx < fctx->total_vars; ++idx) {
        if (idx != 0) xstraf(code, ", ");
        xstraf(code, "*n%d", idx);
    }
    xstraf(code, ";\n");
    xstra_inst(code, "vmvar yy = {0}; SET_UNDEF(&yy);\n");
    if (fctx->total_vars > 0 && fctx->temp_count > 0) {
        xstra_inst(code, "int pstop = vstackp(ctx);\n");
        xstra_inst(code, "const int allocated_local = %" PRId64 ";\n", fctx->temp_count);
        xstra_inst(code, "alloc_var(ctx, %" PRId64 ", L%d, \"%s\", \"%s\", %d);\n",
            fctx->temp_count, f->funcend, i->funcname, escape(&(fctx->str), i->filename), i->line);
    } else {
        xstra_inst(code, "const int allocated_local = 0;\n");
    }
    if (!f->is_global && f->yield > 0) {
        xstra_inst(code, "if (yieldno > 0) {\n");
        xstra_inst(code, "    frm = ctx->callee->frm;\n");
        xstra_inst(code, "    push_frm(ctx, e, frm, L%d, \"%s\", \"%s\", %d);\n", f->funcend, i->funcname, escape(&(fctx->str), i->filename), i->line);
        xstra_inst(code, "    goto RESUMEHOOK;\n");
        xstra_inst(code, "}\n");
    }
    if (f->is_global) {
        xstra_inst(code, "ctx->frm = frm = alcfrm(ctx, lex, %" PRId64 ");\n", fctx->local_vars);
    } else {
        xstra_inst(code, "frm = alcfrm(ctx, lex, %" PRId64 ");\n", fctx->local_vars);
    }
    xstra_inst(code, "push_frm(ctx, e, frm, L%d, \"%s\", \"%s\", %d);\n", f->funcend, i->funcname, escape(&(fctx->str), i->filename), i->line);
    for (int idx = 0; idx < fctx->local_vars; ++idx) {
        xstra_inst(code, "n%d = frm->v[%d] = alcvar_initial(ctx);\n", idx, idx);
    }
    if (fctx->total_vars > 0 && fctx->temp_count > 0) {
        for (int idx = fctx->local_vars; idx < fctx->total_vars; ++idx) {
            xstra_inst(code, "n%d = local_var(ctx, %d);\n", idx, idx - fctx->local_vars);
        }
    }
    if (f->is_global) {
        xstra_inst(code, "SETUP_PROGRAM_ARGS(n0)\n");
    }
}

static void translate_rlocal(func_context *fctx, xstr *code)
{
    if (fctx->total_vars > 0) {
        xstra_inst(code, "restore_vstackp(ctx, pstop);\n");
    }
}

static void translate_popfrm(func_context *fctx, xstr *code)
{
    if (fctx->total_vars > 0 && fctx->temp_count > 0) {
        xstra_inst(code, "restore_vstackp(ctx, pstop);\n");
    }
    xstra_inst(code, "pop_frm(ctx);\n");
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
    case KIR_EXTERN:
        if (i->r1.typeid == TK_TFUNC) {
            xstra_inst(code, "EXTERN_FUNC(%s, %s)\n", i->r2.str, var_value(buf1, &(i->r1)));
        } else {
            xstra_inst(code, "EXTERN_OBJECT(%s, %s)\n", i->r2.str, var_value(buf1, &(i->r1)));
        }
        break;

    case KIR_ALOCAL:
        translate_alocal(fctx, code, f, i);
        break;
    case KIR_RLOCAL:
        translate_rlocal(fctx, code);
        break;
    case KIR_MKFRM:
        translate_mkfrm(fctx, code, f, i);
        break;
    case KIR_POPFRM:
        translate_popfrm(fctx, code);
        break;
    case KIR_SETARG:
        if (i->r2.typestr != NULL) {
            xstra_inst(code, "SET_ARGVAR_TYPE(%d, %d, allocated_local, %s);\n", (int)i->r1.i64, (int)i->r2.i64, i->r2.typestr);
        } else {
            xstra_inst(code, "SET_ARGVAR(%d, %d, allocated_local);\n", (int)i->r1.i64, (int)i->r2.i64);
        }
        break;
    case KIR_SETARGL:
        set_last_args(code, f, (int)i->r1.i64, (int)i->r2.i64);
        break;
    case KIR_PURE:
        if (f->is_pure) {
            translate_pure_hook(code, f);
        }
        if (f->yield > 0) {
            xstra_inst(code, "goto HEAD;\n\n");
            translate_resume_hook(fctx, code, f, i);
        }
        break;

    case KIR_SVSTKP:
        xstra_inst(code, "int ad%d = 0, p%" PRId64 " = vstackp(ctx);\n", i->r1.i64, i->r1.i64);
        break;
    case KIR_PUSHSYS:
        xstra_inst(code, "{ push_var_sys(ctx, %s, ad%d, L%d, \"%s\", \"%s\", %d); }\n",
            var_value(buf1, &(i->r1)), i->r1.callcnt, i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case KIR_PUSHARG:
        translate_pusharg(fctx, code, i, i->catchid > 0 ? i->catchid : f->funcend);
        break;
    case KIR_CALL:
        translate_call(fctx, code, f, i);
        break;
    case KIR_RSSTKP:
        xstra_inst(code, "restore_vstackp(ctx, p%" PRId64 ");\n", i->r1.i64);
        break;
    case KIR_CHKEXCEPT:
        xstra_inst(code, "CHECK_EXCEPTION(L%d, \"%s\", \"%s\", %d);\n", i->labelid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case KIR_YIELDC:
        translate_yield_check(fctx, code, f, i);
        break;

    case KIR_RET:
        if (!f->is_global && f->yield > 0) {
            xstra_inst(code, "if (e != FLOW_YIELD) ctx->callee->yield = 0;\n");
        }
        xstra_inst(code, "return e;\n");
        break;
    case KIR_THROWE:
        xstra_inst(code, "THROW_EXCEPTION(ctx, %s, L%d, \"%s\", \"%s\", %d);\n", var_value(buf1, &(i->r1)), i->labelid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case KIR_THROW:
        xstra_inst(code, "RETHROW(ctx, L%d, \"%s\", \"%s\", %d);\n", i->labelid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case KIR_CATCH:
        xstra_inst(code, "CATCH_EXCEPTION(ctx, %s);\n", var_value(buf1, &(i->r1)));
        break;
    case KIR_YIELD:
        translate_yield(fctx, code, f, i);
        break;
    case KIR_RESUME:
        translate_resume(fctx, code, f, i);
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

    case KIR_NOT:
        translate_not(fctx, code, i);
        break;
    case KIR_MOV:
        translate_mov(fctx, code, i);
        break;
    case KIR_MOVA:
        translate_mova(fctx, code, i);
        break;

    case KIR_ADD:
        translate_chkcnd(fctx, code, "ADD", NULL, i);
        break;
    case KIR_SUB:
        translate_chkcnd(fctx, code, "SUB", NULL, i);
        break;
    case KIR_MUL:
        translate_chkcnd(fctx, code, "MUL", NULL, i);
        break;
    case KIR_DIV:
        translate_chkcnd(fctx, code, "DIV", NULL, i);
        break;
    case KIR_MOD:
        translate_chkcnd(fctx, code, "MOD", NULL, i);
        break;
    case KIR_POW:
        translate_chkcnd(fctx, code, "POW", NULL, i);
        break;
    case KIR_BNOT:
        translate_bnot(fctx, code, i);
        break;
    case KIR_BSHL:
        translate_chkcnd(fctx, code, "BSHL", NULL, i);
        break;
    case KIR_BSHR:
        translate_chkcnd(fctx, code, "BSHR", NULL, i);
        break;
    case KIR_BAND:
        translate_chkcnd(fctx, code, "BAND", NULL, i);
        break;
    case KIR_BOR:
        translate_chkcnd(fctx, code, "BOR", NULL, i);
        break;
    case KIR_BXOR:
        translate_chkcnd(fctx, code, "BXOR", NULL, i);
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
    case KIR_LGE:
        translate_chkcnd(fctx, code, "LGE", NULL, i);
        break;
    case KIR_REGEQ:
        translate_regexp(fctx, code, i, "regeq", 0);
        break;
    case KIR_REGNE:
        translate_regexp(fctx, code, i, "regne", 0);
        break;

    case KIR_INC:
        translate_incdec(fctx, code, "INC", NULL, i);
        break;
    case KIR_INCP:
        translate_incdec(fctx, code, "INCP", NULL, i);
        break;
    case KIR_DEC:
        translate_incdec(fctx, code, "DEC", NULL, i);
        break;
    case KIR_DECP:
        translate_incdec(fctx, code, "DECP", NULL, i);
        break;
    case KIR_MINUS:
        xstra_inst(code, "OP_UMINUS(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", var_value(buf1, &(i->r1)), var_value(buf2, &(i->r2)),
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case KIR_CONV:
        xstra_inst(code, "OP_CONV(ctx, %s, %s, L%d, \"%s\", \"%s\", %d);\n", var_value(buf1, &(i->r1)), var_value(buf2, &(i->r2)),
            i->catchid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;

    case KIR_NEWBIN:
        xstra_inst(code, "SET_BIN(%s, alcbin(ctx));\n", var_value(buf1, &(i->r1)));
        break;
    case KIR_NEWOBJ:
        xstra_inst(code, "SET_OBJ(%s, alcobj(ctx));\n", var_value(buf1, &(i->r1)));
        break;
    case KIR_NEWREGEX:
        translate_regexp(fctx, code, i, "regex", 1);
        break;
    case KIR_SETBIN:
        translate_setbin(fctx, code, i);
        break;
    case KIR_OBJCPY:
        xstra_inst(code, "OBJCOPY(ctx, %s, %s);\n", var_value(buf1, &(i->r1)), var_value(buf2, &(i->r2)));
        break;
    case KIR_MKSUPER:
        xstra_inst(code, "MAKE_SUPER(ctx, %s, %s);\n", var_value(buf1, &(i->r1)), var_value(buf2, &(i->r2)));
        break;

    case KIR_IDXFRM:
        translate_idxfrm(fctx, code, i);
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
    case KIR_REMOVE:
        xstra_inst(code, "hashmap_remove(ctx, (%s)->o, \"%s\");\n", var_value(buf1, &(i->r1)), escape(&(fctx->str), i->r2.str));
        break;
    case KIR_CHKMATCH:
        translate_chkmatch(fctx, code, i);
        break;
    case KIR_CHKRANGE:
        translate_chkrange(fctx, code, i);
        break;

    case KIR_TYPE:
        translate_type(code, i);
        break;

    case KIR_SWAP:
        translate_swap(fctx, code, i);
        break;
    case KIR_SWAPA:
        translate_swapa(fctx, code, i);
        break;
    case KIR_RANGEF:
        translate_range(fctx, code, i, 0);
        break;
    case KIR_RANGET:
        translate_range(fctx, code, i, 1);
        break;

    case KIR_PUSH:
        translate_push(fctx, code, i);
        break;
    case KIR_PUSHX:
        translate_pushx(fctx, code, i);
        break;
    case KIR_PUSHN:
        translate_pushn(fctx, code, i);
        break;
    case KIR_EXPAND:
        translate_expand(fctx, code, i);
        break;

    case KIR_ARYSIZE:
        xstra_inst(code, "SET_I64(%s, (%s)->o->idxsz);\n", var_value(buf1, &(i->r1)), var_value(buf2, &(i->r2)));
        break;
    case KIR_GETITER:
        xstra_inst(code, "GET_ITERATOR(ctx, lex, %s, L%d, \"%s\", \"%s\", %d);\n", var_value(buf1, &(i->r1)),
            i->labelid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case KIR_JMPIFNE:
        xstra_inst(code, "OP_JMP_IF_NOTEND(ctx, lex, %s, L%d, \"%s\", \"%s\", %d);\n", var_value(buf1, &(i->r1)),
            i->labelid, i->funcname, escape(&(fctx->str), i->filename), i->line);
        break;
    case KIR_SWITCHS:
        xstra_inst(code, "COPY_VAR_TO(ctx, %s, %s);\n", var_value(buf1, &(i->r1)), var_value(buf2, &(i->r2)));
        xstra_inst(code, "if ((%s)->t != VAR_INT64) goto L%d;\n", var_value(buf1, &(i->r1)), i->labelid);
        xstra_inst(code, "switch ((%s)->i) {\n", var_value(buf1, &(i->r1)));
        break;
    case KIR_SWITCHE:
        xstra_inst(code, "}\n");
        break;
    case KIR_CASEI:
        xstra_inst(code, "case %d:;\n", i->r1.i64);
        break;
    case KIR_CASEV:
        translate_casev(fctx, code, i);
        break;
    case KIR_DEFAULT:
        xstra_inst(code, "default:;\n");
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

    translate_funcref(code, f);

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

    if (!f->is_global && f->yield > 0) {
        xstraf(code, "YEND:;\n");
        xstra_inst(code, "#undef SAVE_LOCAL\n");
    }
    xstraf(code, "\nEND:;\n");
    if (f->has_frame) {
        translate_popfrm(&fctx, code);
    } else {
        translate_rlocal(&fctx, code);
    }
    xstra_inst(code, "return e;\n");
    xstraf(code, "}\n\n");
}

char *translate(kl_kir_program *p, int mode)
{
    if (!p) {
        return NULL;
    }

    int cap = 2048;
    xstr str = {
        .len = 0,
        .cap = cap,
        .s = (char *)calloc(cap, sizeof(char)),
    };

    kl_kir_func *f = p->head;
    while (f) {
        if (f->is_global && mode == TRANS_LIB) {
            f = f->next;
            continue;
        }
        translate_func(p, &str, f);
        f = f->next;
    }

    if (mode == TRANS_FULL) {
        xstraf(&str, "void setup_context(vmctx *ctx)\n{\n");
        xstra_inst(&str, "ctx->print_result = %d;\n", p->print_result);
        xstra_inst(&str, "ctx->verbose = %d;\n", p->verbose);
        xstraf(&str, "}\n");
        xstraf(&str, "void finalize_context(vmctx *ctx)\n{\n");
        xstra_inst(&str, "finalize(ctx);\n");
        xstraf(&str, "}\n");
    }

    return str.s;   /* this should be freed by the caller. */
}
