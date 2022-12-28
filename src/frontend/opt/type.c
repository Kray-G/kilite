#include "type.h"

typedef struct type_context {
    int errors;
    int error_stdout;
} type_context;

static int type_error(type_context *tc, const char *phase, int line, int pos, int len, const char *fmt, ...)
{
    tc->errors++;
    va_list ap;
    va_start(ap, fmt);
    int r = error_print(tc->error_stdout, phase, line, pos, len, fmt, ap);
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
        if (e->nodetype == TK_EQ && l->typeid == TK_TANY) {
            l->typeid = r->typeid;
            if (!l->prototype && r->prototype) {
                l->prototype = r->prototype;
            }
        }
    }
    if (e->sym) {
        kl_symbol *ref = e->sym->ref;
        if (ref && ref->has_i64) {
            e->nodetype = TK_VSINT;
            e->typeid = TK_TSINT64;
            e->val.i64 = ref->i64;
        } else {
            if (ref) {
                e->sym->typeid = ref->typeid;
                e->sym->prototype = ref->prototype;
            }
            e->typeid = e->sym->typeid;
        }
    }
}

static void type_stmt(kl_context *ctx, type_context *tc, kl_stmt *stmt)
{
    kl_stmt *s = stmt;
    while (s) {
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
        s = s->next;
    }
}

static void type_program(kl_context *ctx, type_context *tc, kl_stmt *s)
{
    type_stmt(ctx, tc, s);
}

void update_ast_type(kl_context *ctx)
{
    type_context tc = { .error_stdout = (ctx->options & PARSER_OPT_ERR_STDOUT) == PARSER_OPT_ERR_STDOUT };
    type_program(ctx, &tc, ctx->head);
}

static int check_pure(tk_token tk)
{
    int pure = 0;
    switch (tk) {
    // Literals
    case TK_VSINT: /* 1 */
    // Keywords
    case TK_CONST: /* 1 */
    case TK_LET: /* 1 */
    case TK_IF: /* 1 */
    case TK_ELSE: /* 1 */
    case TK_DO: /* 1 */
    case TK_WHILE: /* 1 */
    case TK_FOR: /* 1 */
    case TK_RETURN: /* 1 */
    case TK_BREAK: /* 1 */
    case TK_CONTINUE: /* 1 */
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
    case TK_POWEQ: /* 1 */
    case TK_LSHEQ: /* 1 */
    case TK_RSHEQ: /* 1 */
    case TK_LANDEQ: /* 1 */
    case TK_LOREQ: /* 1 */
        // Comparison
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
    case TK_MOD: /* 1 */
    case TK_AND: /* 1 */
    case TK_OR: /* 1 */
    case TK_XOR: /* 1 */
    case TK_QES: /* 1 */
    case TK_POW: /* 1 */
    case TK_LSH: /* 1 */
    case TK_RSH: /* 1 */
    case TK_LAND: /* 1 */
    case TK_LOR: /* 1 */
    case TK_INC: /* 1 */
    case TK_INCP: /* 1 */
    case TK_DEC: /* 1 */
    case TK_DECP: /* 1 */
    // Punctuations
    case TK_COMMA: /* 1 */
    // For extra keywords in parsing.
    case TK_BLOCK: /* 1 */
    case TK_CONNECT: /* 1 */
    case TK_VAR: /* 1 */
    case TK_MINUS: /* 1 */
    case TK_EXPR: /* 1 */
    case TK_CALL: /* 1 */
        pure = 1;
        break;
    default:
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
    case TK_CALL: {
        kl_expr *l = e->lhs;
        if (l->nodetype == TK_VAR) {
            if (!(l->sym->is_callable && l->sym->is_recursive)) {
                return 0;
            }
        }
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
        switch (s->nodetype) {
        case TK_RETURN:
            if (!s->e1) {
                return 0;
            }
            break;
        }

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
