#include "type.h"

typedef struct type_context {
    int errors;
    int in_native;
} type_context;

static int type_error(type_context *tc, const char *phase, int line, int pos, int len, const char *fmt, ...)
{
    tc->errors++;
    va_list ap;
    va_start(ap, fmt);
    int r = error_print(phase, line, pos, len, fmt, ap);
    va_end(ap);
    return r;
}

static void type_expr(kl_context *ctx, type_context *tc, kl_expr *e)
{
    kl_expr *l = e->lhs;
    kl_expr *r = e->rhs;
    if (l) {
        type_expr(ctx, tc, l);
    }
    if (r) {
        type_expr(ctx, tc, r);
    }
    if (l && r) {
        if (e->nodetype == TK_EQ) {
            l->typeid = r->typeid;
            if (!l->prototype && r->prototype) {
                l->prototype = r->prototype;
            }
        }
        if (l->typeid == r->typeid && (!tc->in_native || l->typeid > TK_TUINT64)) {
            /* Even if it's an integer type, it's never promoted to the big integer in native. */
            /* When it's an integer type, it could be promoted to the big integer. */
            e->typeid = l->typeid;
            e->prototype = l->prototype;
        }
    }
    if (e->sym) {
        if (e->sym->ref) {
            e->sym->typeid = e->sym->ref->typeid;
            e->sym->prototype = e->sym->ref->prototype;
        }
        e->typeid = e->sym->typeid;
    }
}

static void type_stmt(kl_context *ctx, type_context *tc, kl_stmt *stmt)
{
    kl_stmt *s = stmt;
    while (s) {
        int in_native = tc->in_native;
        if (s->sym && s->sym->is_callable && s->sym->is_native) {
            tc->in_native = 1;
        }
        if (s->s1) {
            type_stmt(ctx, tc, s->s1);
        }
        if (s->s2) {
            type_stmt(ctx, tc, s->s2);
        }
        if (s->s3) {
            type_stmt(ctx, tc, s->s3);
        }
        if (s->e1) {
            type_expr(ctx, tc, s->e1);
        }
        if (s->e2) {
            type_expr(ctx, tc, s->e2);
        }
        if (s->e3) {
            type_expr(ctx, tc, s->e3);
        }
        if (s->sym && s->sym->is_callable && s->sym->is_native) {
            tc->in_native = in_native;
        }
        s = s->next;
    }
}

static void type_program(kl_context *ctx, type_context *tc, kl_stmt *s)
{
    type_stmt(ctx, tc, s);
}

void update_ast_type(kl_context *ctx)
{
    type_context tc = { .in_native = 0 };
    type_program(ctx, &tc, ctx->head);
}

static int check_pure(tk_token tk)
{
    int pure = 0;
    switch (tk) {
    // Literals
    case TK_VSINT: /* 1 */
        pure = 1;
        break;
    case TK_VBIGINT: /* 0 */
    case TK_VDBL: /* 0 */
    case TK_VSTR: /* 0 */
    case TK_VBIN: /* 0 */
    case TK_VARY: /* 0 */
    case TK_VOBJ: /* 0 */
    case TK_VKV: /* 0 */
        pure = 0;
        break;

    // Keywords
    case TK_CONST: /* 1 */
    case TK_LET: /* 1 */
        pure = 1;
        break;
    case TK_NEW: /* 0 */
    case TK_IMPORT: /* 0 */
    case TK_NAMESPACE: /* 0 */
    case TK_MODULE: /* 0 */
    case TK_CLASS: /* 0 */
    case TK_PRIVATE: /* 0 */
    case TK_PROTECTED: /* 0 */
    case TK_PUBLIC: /* 0 */
    case TK_MIXIN: /* 0 */
    case TK_FUNC: /* 0 */
    case TK_NATIVE: /* 0 */
        pure = 0;
        break;
    case TK_IF: /* 1 */
    case TK_ELSE: /* 1 */
    case TK_DO: /* 1 */
    case TK_WHILE: /* 1 */
    case TK_FOR: /* 1 */
        pure = 1;
        break;
    case TK_IN: /* 0 */
    case TK_FORIN: /* 0 */
        pure = 0;
        break;
    case TK_RETURN: /* 1 */
    case TK_SWITCH: /* 1 */
    case TK_CASE: /* 1 */
        pure = 1;
        break;
    case TK_WHEN: /* 0 */
        pure = 0;
        break;
    case TK_BREAK: /* 1 */
    case TK_CONTINUE: /* 1 */
    case TK_DEFAULT: /* 1 */
        pure = 1;
        break;
    case TK_OTHERWISE: /* 0 */
    case TK_FALLTHROUGH: /* 0 */
    case TK_YIELD: /* 0 */
    case TK_TRY: /* 0 */
    case TK_CATCH: /* 0 */
    case TK_FINALLY: /* 0 */
    case TK_THROW: /* 0 */
        pure = 0;
        break;

    // Operators
        // Unary operators
    case TK_BNOT: /* 1 */
    case TK_NOT: /* 1 */
        // Assignment
    case TK_EQ: /* 1 */
    case TK_ADDEQ: /* 1 */
    case TK_SUBEQ: /* 1 */
    case TK_MULEQ: /* 1 */
    case TK_DIVEQ: /* 1 */
    case TK_MODEQ: /* 1 */
    case TK_ANDEQ: /* 1 */
    case TK_OREQ: /* 1 */
    case TK_XOREQ: /* 1 */
    case TK_EXPEQ: /* 1 */
    case TK_LSHEQ: /* 1 */
    case TK_RSHEQ: /* 1 */
    case TK_LANDEQ: /* 1 */
    case TK_LOREQ: /* 1 */
        pure = 1;
        break;
        // Comparison
    case TK_REGEQ: /* 0 */
    case TK_REGNE: /* 0 */
        pure = 0;
        break;
    case TK_EQEQ: /* 1 */
    case TK_NEQ: /* 1 */
    case TK_LT: /* 1 */
    case TK_LE: /* 1 */
    case TK_GT: /* 1 */
    case TK_GE: /* 1 */
    case TK_LGE: /* 1 */
        // binary operators
    case TK_ADD: /* 1 */
    case TK_SUB: /* 1 */
    case TK_MUL: /* 1 */
        pure = 1;
        break;
    case TK_DIV: /* 0 */
        pure = 0;
        break;
    case TK_MOD: /* 1 */
    case TK_AND: /* 1 */
    case TK_OR: /* 1 */
    case TK_XOR: /* 1 */
    case TK_QES: /* 1 */
    case TK_EXP: /* 1 */
    case TK_LSH: /* 1 */
    case TK_RSH: /* 1 */
    case TK_LAND: /* 1 */
    case TK_LOR: /* 1 */
    case TK_INC: /* 1 */
    case TK_INCP: /* 1 */
    case TK_DEC: /* 1 */
    case TK_DECP: /* 1 */
        pure = 1;
        break;

    // Punctuations
    case TK_COMMA: /* 1 */
        pure = 1;
        break;
    case TK_COLON: /* 0 */
    case TK_SEMICOLON: /* 0 */
    case TK_DOT: /* 0 */
    case TK_DOT2: /* 0 */
    case TK_DOT3: /* 0 */
    case TK_LSBR: /* 0 */
    case TK_RSBR: /* 0 */
    case TK_LLBR: /* 0 */
    case TK_RLBR: /* 0 */
    case TK_LXBR: /* 0 */
    case TK_RXBR: /* 0 */
        pure = 0;
        break;

    // Others
    case TK_ARROW: /* 0 */
    case TK_DARROW: /* 0 */
    case TK_TYPEID: /* 0 */
    case TK_NAME: /* 0 */
        pure = 0;
        break;

    // For extra keywords in parsing.
    case TK_BLOCK: /* 1 */
    case TK_CONNECT: /* 1 */
    case TK_VAR: /* 1 */
    case TK_MINUS: /* 1 */
    case TK_EXPR: /* 1 */
    case TK_CALL: /* 1 */
        pure = 1;
        break;
    case TK_IDX: /* 0 */
    case TK_TYPENODE: /* 0 */
    case TK_MKSUPER: /* 0 */
    case TK_BINEND: /* 0 */
    case TK_COMMENT1: /* 0 */
    case TK_COMMENTX: /* 0 */
        pure = 0;
        break;
    }

    return pure;
};

static int check_pure_function_expr(kl_context *ctx, kl_expr *e)
{
    int pure = 1;

    kl_expr *l = e->lhs;
    kl_expr *r = e->rhs;
    kl_expr *x = e->xhs;
    if (l) {
        pure = check_pure_function_expr(ctx, l);
        if (!pure) return 0;
    }
    if (r) {
        pure = check_pure_function_expr(ctx, r);
        if (!pure) return 0;
    }
    if (x) {
        pure = check_pure_function_expr(ctx, x);
        if (!pure) return 0;
    }

    pure = check_pure(e->nodetype);
    if (!pure) return 0;
    switch (e->nodetype) {
    case TK_VAR: {
        if (e->sym && e->sym->level > 0) {
            if (!(e->sym->is_callable && e->sym->is_recursive)) {
                return 0;
            }
        }
        break;
    }
    default:
        break;
    }
    return pure;
}

int check_pure_function(kl_context *ctx, kl_stmt *stmt)
{
    int pure = 1;
    kl_stmt *s = stmt;
    while (s) {
        pure = check_pure(s->nodetype);
        if (!pure) return 0;

        if (s->s1) {
            pure = check_pure_function(ctx, s->s1);
            if (!pure) return 0;
        }
        if (s->s2) {
            pure = check_pure_function(ctx, s->s2);
            if (!pure) return 0;
        }
        if (s->s3) {
            pure = check_pure_function(ctx, s->s3);
            if (!pure) return 0;
        }
        if (s->e1) {
            pure = check_pure_function_expr(ctx, s->e1);
            if (!pure) return 0;
        }
        if (s->e2) {
            pure = check_pure_function_expr(ctx, s->e2);
            if (!pure) return 0;
        }
        if (s->e3) {
            pure = check_pure_function_expr(ctx, s->e3);
            if (!pure) return 0;
        }
        s = s->next;
    }
    return pure;
}
