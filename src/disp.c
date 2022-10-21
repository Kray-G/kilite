#include "disp.h"

static void disp_stmt_list(kl_stmt *s, int indent);

static void make_indent(int indent)
{
    if (indent > 0)
        for (int i = 0; i < indent; ++i) printf("  ");
}

static int append_str(char *buf, int i, int max, const char *str)
{
    while (*str && i < max) {
        buf[i++] = *str++;
    }
    return i;
}

static int create_proto_string_expr(char *buf, int i, int max, kl_expr *args)
{
    if (args->nodetype == TK_COMMA) {
        i = create_proto_string_expr(buf, i, max, args->lhs);
        i = append_str(buf, i, max, ", ");
        i = create_proto_string_expr(buf, i, max, args->rhs);
    } else {
        i = append_str(buf, i, max, typeidname(args->typeid));
    }
    return i;
}

static void print_prototype(kl_symbol *sym)
{
    int i = 0;
    char buf[256] = {0};
    i = append_str(buf, i, 254, "(");
    i = create_proto_string_expr(buf, i, 254, sym->args);
    i = append_str(buf, i, 254, ") -> ");
    append_str(buf, i, 254, typeidname(sym->type));
    printf(", %s", buf);
}

static void disp_expr(kl_expr *e, int indent)
{
    if (!e) return;

    make_indent(indent);
    switch (e->nodetype) {
    case TK_VSINT:
        printf("(int64):%lld", e->val.i64);
        if (indent > 0) printf("\n");
        break;
    case TK_VUINT:
        printf("(uint64):%llu", e->val.u64);
        if (indent > 0) printf("\n");
        break;
    case TK_VDBL:
        printf("(real):%f", e->val.dbl);
        if (indent > 0) printf("\n");
        break;
    case TK_VBIGINT:
        printf("(bigint):%s", e->val.big);
        if (indent > 0) printf("\n");
        break;
    case TK_VSTR:
        printf("(string):%s", e->val.str);
        if (indent > 0) printf("\n");
        break;
    case TK_VBIN:
        printf("(binary)\n");
        disp_expr(e->lhs, indent + 1);
        if (indent > 0) printf("\n");
        break;

    case TK_VAR:
        if (e->sym) {
            if (e->sym->ref) {
                int index = e->sym->ref->index;
                int level = e->sym->level;
                printf("[$%d(%d)] %s:%s", index, level, e->sym->name, typeidname(e->typeid));
            } else {
                int index = e->sym->index;
                printf("[$%d] %s:%s", index, e->sym->name, typeidname(e->typeid));
            }
            if (e->sym->ref) {
                kl_symbol *ref = e->sym->ref;
                if (ref->is_callable) {
                    if (e->sym->is_recursive) {
                        printf(", [=]");
                    }
                    print_prototype(ref);
                }
            }
            if (indent > 0) printf("\n");
        }
        break;

    case TK_CALL:
        if (e->lhs) {
            printf("call\n");
            disp_expr(e->lhs, indent + 1);
        }
        if (e->rhs) {
            disp_expr(e->rhs, indent + 1);
        }
        break;
    case TK_NOT:
    case TK_EQ:
    case TK_ADDEQ:
    case TK_SUBEQ:
    case TK_MULEQ:
    case TK_DIVEQ:
    case TK_MODEQ:
    case TK_ANDEQ:
    case TK_OREQ:
    case TK_XOREQ:
    case TK_EXPEQ:
    case TK_LSHEQ:
    case TK_RSHEQ:
    case TK_LANDEQ:
    case TK_LOREQ:
    case TK_REGEQ:
    case TK_REGNE:
    case TK_EQEQ:
    case TK_NEQ:
    case TK_LT:
    case TK_LE:
    case TK_GT:
    case TK_GE:
    case TK_LGE:
    case TK_ADD:
    case TK_SUB:
    case TK_MUL:
    case TK_DIV:
    case TK_MOD:
    case TK_AND:
    case TK_OR:
    case TK_XOR:
    case TK_EXP:
    case TK_LSH:
    case TK_RSH:
    case TK_LAND:
    case TK_LOR:
    case TK_COMMA:
        printf("op(%s)\n", tokenname(e->nodetype));
        disp_expr(e->lhs, indent + 1);
        disp_expr(e->rhs, indent + 1);
        break;
    default:
        printf("[MISSING]: %s\n", tokenname(e->nodetype));
    }
}

static int method_count(kl_symbol *sym)
{
    int count = 0;
    while (sym->method) {
        ++count;
        sym = sym->method;
    }
    return count;
}

static void disp_stmt(kl_stmt *s, int indent)
{
    if (!s) return;

    if (s->nodetype != TK_EXPR) {
        for (int i = 0; i < indent; ++i) printf("  ");
    }
    if (s->sym) {
        printf("%s", s->sym->has_func ? "*" : "");
    }
    switch (s->nodetype) {
    case TK_BLOCK:
        printf("block:\n");
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        break;
    case TK_NAMESPACE:
        if (s->sym->name) {
            printf("namespace: %s", s->sym->name);
        } else {
            printf("namespace: (anonymous)");
        }
        printf(" -> method(%d)\n", method_count(s->sym));
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        break;
    case TK_FUNC:
        if (s->sym) {
            printf("def ");
            switch (s->sym->symtype) {
            case TK_PRIVATE: printf("private "); break;
            case TK_PROTECTED: printf("protected "); break;
            case TK_PUBLIC: printf("public "); break;
            }
            printf("func %s(\n", s->sym->name);
            if (s->e1) {
                disp_expr(s->e1, indent + 1);
            }
            make_indent(indent);
            printf(") : %s\n", typeidname(s->typeid));
            if (s->s1) {
                disp_stmt_list(s->s1, indent + 1);
            }
        }
        break;
    case TK_CLASS:
        if (s->sym) {
            printf("def class %s(", s->sym->name);
            if (s->e1) {
                disp_expr(s->e1, -1);
            }
            printf(") -> method(%d)\n", method_count(s->sym));
            if (s->s1) {
                disp_stmt_list(s->s1, indent + 1);
            }
        }
        break;
    case TK_IF:
        printf("if\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 1);
        }
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        if (s->s2) {
            disp_stmt_list(s->s2, indent + 1);
        }
        break;
    case TK_WHILE:
        printf("while\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 1);
        }
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        break;
    case TK_DO:
        printf("do\n");
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        make_indent(indent);
        printf("- while condition\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 1);
        }
        break;
    case TK_RETURN:
        printf("return\n");
        disp_expr(s->e1, indent + 1);
        break;
    case TK_CONST:
        printf("const\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 1);
        }
        break;
    case TK_LET:
        printf("let\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 1);
        }
        break;
    case TK_EXPR:
        disp_expr(s->e1, indent);
        break;
    }
}

static void disp_stmt_list(kl_stmt *s, int indent)
{
    while (s) {
        disp_stmt(s, indent);
        s = s->next;
    }
}

void dispast(kl_context *ctx)
{
    disp_stmt_list(ctx->head, 0);
    // kl_stmt *copy = copy_tree(ctx, ctx->head);
    // disp_stmt_list(copy, 0);
}
