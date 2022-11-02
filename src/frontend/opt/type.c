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
