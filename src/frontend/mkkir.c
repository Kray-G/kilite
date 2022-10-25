#include "../kir.h"
#include "parser.h"

static void gen_stmt(kl_stmt *s)
{
    if (!s) return;

    switch (s->nodetype) {
    case TK_BLOCK:
    case TK_NAMESPACE:
    case TK_FUNC:
    case TK_CLASS:
    case TK_IF:
    case TK_WHILE:
    case TK_DO:
    case TK_RETURN:
    case TK_CONST:
    case TK_LET:
    case TK_EXPR:
    }
}

static void gen_stmt_list(kl_stmt *s)
{
    while (s) {
        gen_stmt(s);
        s = s->next;
    }
}

int make_kir(kl_context *ctx)
{
    gen_stmt_list(ctx->head);
}
