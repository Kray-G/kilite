#include "dispast.h"

static void disp_expr(kl_expr *e, int indent);
static void disp_stmt_list(kl_stmt *s, int indent);

void escape_print(const char *s)
{
    while (*s) {
        if (*s == '"' || *s == '\\') {
            printf("%c", '\\');
        }
        switch (*s) {
        case '\a':
            printf("\\a");
            s++;
            break;
        case '\b':
            printf("\\b");
            s++;
            break;
        case '\x1b':
            printf("\\e");
            s++;
            break;
        case '\f':
            printf("\\f");
            s++;
            break;
        case '\n':
            printf("\\n");
            s++;
            break;
        case '\r':
            printf("\\r");
            s++;
            break;
        case '\t':
            printf("\\t");
            s++;
            break;
        case '\v':
            printf("\\v");
            s++;
            break;
        default:
            printf("%c", *s);
            s++;
        }
    }
}

static void make_indent(int indent)
{
    if (indent > 0)
        for (int i = 0; i < indent; ++i) printf("  ");
}

static void print_typenode(kl_expr *e)
{
    if (!e) return;
    switch (e->nodetype) {
    case TK_TYPENODE:
        printf("%s", typeidname(e->typeid));
        break;
    case TK_COMMA:
        print_typenode(e->lhs);
        printf(", ");
        print_typenode(e->rhs);
        break;
    case TK_DARROW:
        printf("(");
        print_typenode(e->lhs);
        printf(") => ");
        print_typenode(e->rhs);
        break;
    }
}

static void print_array(kl_expr *e, int indent)
{
    if (!e) {
        make_indent(indent);
        printf("-\n");
        return;
    }

    switch (e->nodetype) {
    case TK_COMMA:
        print_array(e->lhs, indent);
        print_array(e->rhs, indent);
        break;
    default:
        disp_expr(e, indent);
        break;
    }
}

static void print_object(kl_expr *e, int indent)
{
    if (!e) return;

    switch (e->nodetype) {
    case TK_VKV:
        make_indent(indent);
        printf("(keyvalue)\n");
        disp_expr(e->lhs, indent + 1);
        disp_expr(e->rhs, indent + 1);
        break;
    case TK_COMMA:
        print_object(e->lhs, indent);
        print_object(e->rhs, indent);
        break;
    }
}

static void disp_expr(kl_expr *e, int indent)
{
    if (!e) return;

    make_indent(indent);
    switch (e->nodetype) {
    case TK_VSINT:
        printf("(int64):%" PRId64, e->val.i64);
        if (indent > 0) printf("\n");
        break;
    case TK_VDBL:
        printf("(real):%s", e->val.dbl);
        if (indent > 0) printf("\n");
        break;
    case TK_VBIGINT:
        printf("(bigint):%s", e->val.big);
        if (indent > 0) printf("\n");
        break;
    case TK_VSTR:
        printf("(string):\"");
        escape_print(e->val.str);
        printf("\"");
        if (indent > 0) printf("\n");
        break;
    case TK_VBIN:
        printf("(binary)\n");
        disp_expr(e->lhs, indent + 1);
        if (indent > 0) printf("\n");
        break;
    case TK_VARY:
        printf("(array) [\n");
        if (e->lhs) {
            print_array(e->lhs, indent + 1);
        }
        make_indent(indent);
        printf("]\n");
        break;
    case TK_VOBJ:
        printf("(object)\n");
        if (e->lhs) {
            print_object(e->lhs, indent + 1);
        }
        break;
    case TK_DOT3:
        printf("(...)\n");
        if (e->lhs) {
            disp_expr(e->lhs, indent + 1);
        }
        break;

    case TK_VAR:
        if (e->sym) {
            kl_symbol *ref = e->sym->ref;
            kl_symbol *sym = e->sym;
            int index = sym->index;
            int level = sym->level;
            if (ref) {
                printf("[$%d(%d)] %s%s:%s", index, level, sym->is_dot3 ? "..." : "", sym->name, typeidname(e->typeid));
            } else {
                printf("[$%d] %s%s:%s", index, sym->is_dot3 ? "..." : "", sym->name, typeidname(e->typeid));
            }
            if (sym->typetree) {
                printf(", ");
                print_typenode(sym->typetree);
            }
            if (ref) {
                if (ref->is_callable) {
                    if (sym->is_recursive) {
                        printf(", [=]");
                    }
                    if (sym->prototype) {
                        printf(", %s", sym->prototype);
                    }
                }
            }
            if (indent > 0) printf("\n");
        }
        break;

    case TK_FUNC:
        if (e->sym) {
            printf("def funcexpr %s(\n", e->sym->name);
            if (e->e) {
                disp_expr(e->e, indent + 1);
            }
            make_indent(indent);
            if (e->sym->typetree) {
                printf(") : ");
                print_typenode(e->sym->typetree);
                printf("\n");
            } else {
                printf(") : %s\n", typeidname(e->typeid));
            }
            if (e->s) {
                disp_stmt_list(e->s, indent + 1);
            }
        }
        break;

    case TK_CALL:
        if (e->lhs) {
            printf("call:%s\n", typeidname(e->typeid));
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
    case TK_POWEQ:
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
    case TK_POW:
    case TK_LSH:
    case TK_RSH:
    case TK_LAND:
    case TK_LOR:
    case TK_IDX:
    case TK_DOT:
    case TK_COMMA:
    case TK_RANGE2:
    case TK_RANGE3:
        printf("op(%s): %s", tokenname(e->nodetype), typeidname(e->typeid));
        if (e->prototype) {
            printf(", %s", e->prototype);
        }
        printf("\n");
        disp_expr(e->lhs, indent + 1);
        disp_expr(e->rhs, indent + 1);
        break;
    case TK_QES:
        printf("op(%s)\n", tokenname(e->nodetype));
        disp_expr(e->lhs, indent + 1);
        disp_expr(e->rhs, indent + 1);
        disp_expr(e->xhs, indent + 1);
        break;
    case TK_INC:
    case TK_INCP:
    case TK_DEC:
    case TK_DECP:
    case TK_MINUS:
        printf("op(%s): %s\n", tokenname(e->nodetype), typeidname(e->typeid));
        disp_expr(e->lhs, indent + 1);
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
        make_indent(indent);
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
        printf(", method(%d)\n", method_count(s->sym));
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        break;
    case TK_FUNC:
        if (s->sym) {
            printf("def[$%d] ", s->sym->index);
            switch (s->sym->symtoken) {
            case TK_PRIVATE: printf("private "); break;
            case TK_PROTECTED: printf("protected "); break;
            case TK_PUBLIC: printf("public "); break;
            }
            printf("%s %s(\n", s->sym->is_native ? "native" : "func", s->sym->name);
            if (s->e1) {
                disp_expr(s->e1, indent + 1);
            }
            make_indent(indent);
            if (s->sym->typetree) {
                printf(") : ");
                print_typenode(s->sym->typetree);
                printf("\n");
            } else {
                printf(") : %s\n", typeidname(s->typeid));
            }
            if (s->s1) {
                disp_stmt_list(s->s1, indent + 1);
            }
        }
        break;
    case TK_CLASS:
        if (s->sym) {
            printf("def class %s, method(%d)", s->sym->name, method_count(s->sym));
            if (s->sym->base) {
                printf(" : %s\n", s->sym->basename);
            } else {
                printf("\n");
            }
            if (s->e1) {
                make_indent(indent);
                printf("(\n");
                disp_expr(s->e1, indent + 1);
                make_indent(indent);
                printf(")\n");
            }
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
    case TK_DO:
        printf("do\n");
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        make_indent(indent+1);
        printf("while-cond:\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 2);
        }
        break;
    case TK_WHILE:
        printf("while\n");
        make_indent(indent+1);
        printf("cond:\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 2);
        }
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        break;
    case TK_FOR:
        printf("for\n");
        make_indent(indent+1);
        printf("cond1:\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 2);
        }
        make_indent(indent+1);
        printf("cond2:\n");
        if (s->e2) {
            disp_expr(s->e2, indent + 2);
        }
        make_indent(indent+1);
        printf("cond3:\n");
        if (s->e3) {
            disp_expr(s->e3, indent + 2);
        }
        /* then part */
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        break;
    case TK_FORIN:
        printf("for-in\n");
        make_indent(indent+1);
        printf("cond1:\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 2);
        }
        make_indent(indent+1);
        printf("cond2:\n");
        if (s->e2) {
            disp_expr(s->e2, indent + 2);
        }
        /* then part */
        if (s->s1) {
            disp_stmt_list(s->s1, indent + 1);
        }
        break;
    case TK_TRY:
        printf("try\n");
        if (s->s1) {
            disp_stmt(s->s1, indent + 1);
        }
        if (s->s2) {
            make_indent(indent);
            printf("catch");
            if (s->e1) {
                printf(" (");
                disp_expr(s->e1, -1);
            }
            printf(")\n");
            disp_stmt(s->s2, indent + 1);
        }
        if (s->s3) {
            make_indent(indent);
            printf("finally\n");
            disp_stmt(s->s2, indent + 1);
        }
        break;
    case TK_RETURN:
        printf("return\n");
        if (s->e1) {
            disp_expr(s->e1, indent + 1);
        }
        break;
    case TK_EXTERN:
        printf("extern: ");
        disp_expr(s->e2, -1);
        printf(" <- %s", s->e1->val.str);
        if (s->typeid == TK_TFUNC) {
            printf("()");
        }
        printf("\n");
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
    case TK_MKSUPER:
        printf("makesuper\n");
        make_indent(indent + 1);
        printf("[method copy] super <- this\n");
        break;
    default:
        printf("[MISSING]: %s\n", tokenname(s->nodetype));
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

void disp_ast(kl_context *ctx)
{
    disp_stmt_list(ctx->head, 0);
    // kl_stmt *copy = copy_tree(ctx, ctx->head);
    // disp_stmt_list(copy, 0);
}
