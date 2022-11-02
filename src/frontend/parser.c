/*
 * Hand-made parser by recursive descent parcing.
 */
#include "error.h"
#include "parser.h"

static kl_expr *parse_expr_list(kl_context *ctx, kl_lexer *l, int endch);
static kl_expr *parse_expr_assignment(kl_context *ctx, kl_lexer *l);
static kl_expr *parse_expression(kl_context *ctx, kl_lexer *l);
static kl_expr *parse_decl_expr(kl_context *ctx, kl_lexer *l, int is_const);
static kl_stmt *parse_statement(kl_context *ctx, kl_lexer *l);
static kl_stmt *parse_statement_list(kl_context *ctx, kl_lexer *l);

#define DEBUG_PARSER_PHASE()\
if ((ctx->options & PARSER_OPT_PHASE) == PARSER_OPT_PHASE) printf("[parser] %s\n", __func__);\
/**/

static int parse_error(kl_context *ctx, int sline, const char *phase, kl_lexer *l, const char *fmt, ...)
{
    fprintf(stderr, "%d: ", sline);
    int line = l->tokline;
    int pos = l->tokpos;
    int len = l->toklen;
    ctx->errors++;
    va_list ap;
    va_start(ap, fmt);
    int r = error_print(phase, line, pos, len, fmt, ap);
    va_end(ap);
    return r;
}

/*
 * Node management
 */

unsigned int hash(const char *s)
{
	unsigned int h = (int)*s;
	if (h) for (++s ; *s; ++s) {
        h = (h << 5) - h + (int)*s;
    }
    return h % HASHSIZE;
}

char *const_str(kl_context *ctx, const char *phase, int line, int pos, int len, const char *str)
{
    kl_conststr *p;

    unsigned int h = hash(str);
    for (p = ctx->hash[h]; p != NULL; p = p->next) {
        if (strcmp(str, (*p).str) == 0) {
            return (*p).str;
        }
    }

    /* Register the new string */
    if ((p = (kl_conststr *)calloc(1, sizeof(kl_conststr))) == NULL) {
        return NULL;
    }

    (*p).str = strdup(str);
    p->next = ctx->hash[h];
    ctx->hash[h] = p;
    return (*p).str;
}

static char *parse_const_str(kl_context *ctx, kl_lexer *l, const char *str)
{
    return const_str(ctx, "Compile", l->tokline, l->tokpos, l->toklen, str);
}

kl_context *parser_new_context(void)
{
    return (kl_context *)calloc(1, sizeof(kl_context));
}

static inline int append_ns_prefix(char *buf, int pos)
{
    /* This prefix is to make risk lower used by users. */
    buf[pos++] = '_'; buf[pos++] = '_';
    buf[pos++] = 'w'; buf[pos++] = 'i'; buf[pos++] = 't'; buf[pos++] = 'h';
    buf[pos++] = 'n'; buf[pos++] = 's';
    buf[pos++] = '_';
    return pos;
}

static char *make_func_name(kl_context *ctx, kl_lexer *l, const char *str)
{
    char buf[1024] = {0};
    int pos = 0;
    int len = strlen(str);
    if (pos + len > 1016) {
        parse_error(ctx, __LINE__, "Compile", l, "Internal error with allocation failed.");
    } else {
        kl_nsstack *n = ctx->ns;
        strcpy(buf + pos, str);
        pos += len;
        if (n->prev) {
            pos = append_ns_prefix(buf, pos);
        }
        buf[pos] = 0;
        while (n->prev) {   /* skips a global namespace. */
            buf[pos++] = '_';
            int len = strlen(n->name);
            if (pos + len + 3 > 1016) {
                parse_error(ctx, __LINE__, "Compile", l, "Internal error with allocation failed.");
                break;
            }
            strcpy(buf + pos, n->name);
            pos += len;
            n = n->prev;
        }
    }
    return const_str(ctx, "Compile", l->tokline, l->tokpos, l->toklen, buf);
}

static int append_str(char *buf, int i, int max, const char *str)
{
    while (*str && i < max) {
        buf[i++] = *str++;
    }
    return i;
}

static int create_proto_string_expr(char *buf, int i, int max, kl_expr *e)
{
    if (!e) {
        return i;
    }

    switch (e->nodetype) {
    case TK_VAR:
    case TK_TYPENODE:
        i = append_str(buf, i, max, typeidname(e->typeid));
        break;
    case TK_COMMA:
        i = create_proto_string_expr(buf, i, max, e->lhs);
        i = append_str(buf, i, max, ", ");
        i = create_proto_string_expr(buf, i, max, e->rhs);
        break;
    case TK_DARROW:
        i = append_str(buf, i, max, "(");
        i = create_proto_string_expr(buf, i, max, e->lhs);
        i = append_str(buf, i, max, ") => ");
        i = create_proto_string_expr(buf, i, max, e->rhs);
        break;
    case TK_VARY:
        i = append_str(buf, i, max, "[]");
        break;
    case TK_VOBJ:
        i = append_str(buf, i, max, "{}");
        break;
    }
    return i;
}

static const char *create_prototype(kl_context *ctx, kl_lexer *l, kl_symbol *sym)
{
    int i = 0;
    char buf[256] = {0};
    i = append_str(buf, i, 254, "(");
    i = create_proto_string_expr(buf, i, 254, sym->args);
    i = append_str(buf, i, 254, ") => ");
    if (sym->typetree) {
        i = append_str(buf, i, 254, "(");
        i = create_proto_string_expr(buf, i, 254, sym->typetree);
        i = append_str(buf, i, 254, ")");
    } else {
        append_str(buf, i, 254, typeidname(sym->rettype));
    }
    return parse_const_str(ctx, l, buf);
}

static inline kl_nsstack *make_nsstack(kl_context *ctx, kl_lexer *l, const char *name, int scope)
{
    kl_nsstack *n = (kl_nsstack *)calloc(1, sizeof(kl_nsstack));
    n->name = parse_const_str(ctx, l, name);
    n->scopetype = scope;
    n->chn = ctx->nsstchn;
    ctx->nsstchn = n;
    return n;
}

static inline void push_nsstack(kl_context *ctx, kl_nsstack *n)
{
    n->prev = ctx->ns;
    ctx->ns = n;
}

static inline void pop_nsstack(kl_context *ctx)
{
    ctx->ns = ctx->ns->prev;
}

static inline void add_sym2nsstack(kl_nsstack *n, kl_symbol *sym)
{
    sym->next = n->list;
    n->list = sym;
}

static inline void add_sym2method(kl_symbol *scope, kl_symbol *sym)
{
    switch (scope->symtoken) {
    case TK_NAMESPACE:
    case TK_CLASS:
    case TK_MODULE:
        sym->method = scope->method;
        scope->method = sym;
        break;
    }
}

static inline kl_symbol *search_symbol_in_scope(kl_context *ctx, kl_lexer *l, kl_nsstack *ns, const char *name)
{
    kl_symbol *ref = ns->list;
    while (ref && ref->name) {
        if (strcmp(ref->name, name) == 0) {
            return ref;
        }
        ref = ref->next;
    }
    return NULL;
}

static inline kl_symbol *make_ref_symbol(kl_context *ctx, kl_lexer *l, tk_token tk, const char *name)
{
    int level = 0;
    kl_symbol *sym = (kl_symbol *)calloc(1, sizeof(kl_symbol));
    sym->chn = ctx->symchn;
    ctx->symchn = sym;
    sym->symtoken = tk;
    sym->line = l->tokline;
    sym->pos = l->tokpos;
    kl_nsstack *ns = ctx->ns;
    while (ns) {
        kl_symbol *ref = search_symbol_in_scope(ctx, l, ns, name);
        if (ref) {
            sym->ref = ref;
            sym->index = ref->index;
            sym->level = level;
            sym->typeid = ref->typeid;
            sym->rettype = ref->rettype;
            sym->typetree = ref->typetree;
            sym->prototype = ref->prototype;
            sym->is_callable = ref->is_callable;
            return sym;
        }
        if (ns->scopetype != TK_BLOCK) {
            level++;
        }
        ns = ns->prev;
    }

    // l->toklen = strlen(name);
    parse_error(ctx, __LINE__, "Compile", l, "The symbol(%s) not found.", name);
    return sym;
}

static inline kl_symbol *make_symbol(kl_context *ctx, kl_lexer *l, tk_token tk)
{
    kl_symbol *sym = (kl_symbol *)calloc(1, sizeof(kl_symbol));
    sym->chn = ctx->symchn;
    ctx->symchn = sym;
    sym->symtoken = tk;
    sym->index = ctx->scope ? ctx->scope->idxmax++ : 0;
    sym->ref = NULL;
    sym->line = l->tokline;
    sym->pos = l->tokpos;
    add_sym2nsstack(ctx->ns, sym);
    return sym;
}

static inline kl_expr *make_expr(kl_context *ctx, kl_lexer *l, tk_token tk)
{
    kl_expr *e = (kl_expr *)calloc(1, sizeof(kl_expr));
    e->chn = ctx->exprchn;
    ctx->exprchn = e;
    e->nodetype = tk;
    e->line = l->tokline;
    e->pos = l->tokpos;
    return e;
}

static inline kl_expr *make_bin_expr(kl_context *ctx, kl_lexer *l, tk_token tk, kl_expr *lhs, kl_expr *rhs)
{
    if (!lhs) return rhs;
    kl_expr *e = make_expr(ctx, l, tk);
    e->lhs = lhs;
    e->rhs = rhs;
    return e;
}

static inline kl_expr *make_conn_expr(kl_context *ctx, kl_lexer *l, tk_token tk, kl_expr *lhs, kl_expr *rhs)
{
    kl_expr *e = make_expr(ctx, l, tk);
    e->lhs = lhs;
    e->rhs = rhs;
    return e;
}

static inline kl_expr *copy_expr(kl_context *ctx, kl_lexer *l, kl_expr *src)
{
    kl_expr *e = make_expr(ctx, l, src->nodetype);
    e->typeid = src->typeid;
    e->sym = src->sym;
    e->val = src->val;
    if (src->lhs) e->lhs = copy_expr(ctx, l, src->lhs);
    if (src->rhs) e->rhs = copy_expr(ctx, l, src->rhs);
    return e;
}

static inline kl_stmt *make_stmt(kl_context *ctx, kl_lexer *l, tk_token tk)
{
    kl_stmt *s = (kl_stmt *)calloc(1, sizeof(kl_stmt));
    s->chn = ctx->stmtchn;
    ctx->stmtchn = s;
    s->nodetype = tk;
    s->line = l->tokline;
    s->pos = l->tokpos;
    return s;
}

static inline kl_stmt *connect_stmt(kl_stmt *s1, kl_stmt *s2)
{
    if (!s2) {
        return s1;
    }
    kl_stmt *n = s1;
    if (!n) {
        n = s2;
    } else {
        while (n->next) {
            n = n->next;
        }
        n->next = s2;
    }

    /* Returns the last node */
    while (n->next) {
        n = n->next;
    }
    return n;
}

static inline kl_stmt *copy_stmt(kl_context *ctx, kl_lexer *l, kl_stmt *src)
{
    kl_stmt *e = make_stmt(ctx, l, src->nodetype);
    e->typeid = src->typeid;
    e->sym = src->sym;
    if (src->e1) e->e1 = copy_expr(ctx, l, src->e1);
    if (src->e2) e->e2 = copy_expr(ctx, l, src->e2);
    if (src->e3) e->e3 = copy_expr(ctx, l, src->e3);
    if (src->s1) e->s1 = copy_stmt(ctx, l, src->s1);
    if (src->s2) e->s2 = copy_stmt(ctx, l, src->s2);
    if (src->s3) e->s3 = copy_stmt(ctx, l, src->s3);
    if (src->next) e->next = copy_stmt(ctx, l, src->next);
    return e;
}

kl_stmt *copy_tree(kl_context *ctx, kl_lexer *l, kl_stmt *src)
{
    return copy_stmt(ctx, l, src);
}

void free_context(kl_context *ctx)
{
    kl_stmt *s = ctx->stmtchn;
    while (s) {
        kl_stmt *n = s->chn;
        free(s);
        s = n;
    }
    kl_expr *e = ctx->exprchn;
    while (e) {
        kl_expr *n = e->chn;
        free(e);
        e = n;
    }
    kl_symbol *sym = ctx->symchn;
    while (sym) {
        kl_symbol *n = sym->chn;
        free(sym);
        sym = n;
    }
    kl_nsstack *ns = ctx->nsstchn;
    while (ns) {
        kl_nsstack *n = ns->chn;
        free(ns);
        ns = n;
    }

    if (ctx->program) {
        kl_kir_inst *i = ctx->program->ichn;
        while (i) {
            kl_kir_inst *n = i->chn;
            free(i);
            i = n;
        }
        kl_kir_func *f = ctx->program->fchn;
        while (f) {
            kl_kir_func *n = f->chn;
            free(i);
            f = n;
        }
        free(ctx->program);
    }

    for (int h = 0; h < HASHSIZE; ++h) {
        for (kl_conststr *p = ctx->hash[h]; p != NULL; ) {
            if (p->str) {
                free((*p).str);
            }
            kl_conststr *n = p->next;
            free(p);
            p = n;
        }
    }
    free(ctx);
}

/*
 * Parser
 */

static kl_expr *panic_mode_expr(kl_expr *e, int ch, kl_context *ctx, kl_lexer *l)
{
    while (l->ch != ch && l->ch != EOF) {
        lexer_fetch(l);
    }
    lexer_fetch(l);
    return e;
}

static kl_stmt *panic_mode_exprstmt(kl_stmt *s, int ch, kl_context *ctx, kl_lexer *l)
{
    while (l->ch != ch && l->ch != EOF) {
        lexer_fetch(l);
    }
    lexer_fetch(l);
    return s;
}

static kl_stmt *panic_mode_stmt(kl_stmt *s, int ch, kl_context *ctx, kl_lexer *l)
{
    while (l->ch != ch && l->ch != EOF) {
        lexer_fetch(l);
    }
    if (l->ch != EOF) {
        lexer_fetch(l);
        if (l->tok != TK_RXBR) {
            s = parse_statement_list(ctx, l);
            if (l->tok == TK_RXBR) {
                lexer_fetch(l);
            }
        }
    }
    return s;
}

static void check_symbol(kl_context *ctx, kl_lexer *l, const char *name)
{
    kl_symbol *chk = search_symbol_in_scope(ctx, l, ctx->ns, name);
    if (chk) {
        parse_error(ctx, __LINE__, "Compile", l, "The symbol(%s) was already declared in this scope.", chk->name);
    }
}

static kl_expr *parse_expr_varname(kl_context *ctx, kl_lexer *l, const char *name)
{
    kl_expr *e = make_expr(ctx, l, TK_VAR);
    kl_symbol *sym;
    if (ctx->in_lvalue) {
        check_symbol(ctx, l, name);
        sym = make_symbol(ctx, l, TK_VAR);
    } else {
        sym = make_ref_symbol(ctx, l, TK_VAR, name);
    }
    sym->name = parse_const_str(ctx, l, name);
    e->sym = sym;
    e->typeid = sym->typeid;
    e->prototype = sym->prototype;
    if (sym->ref) {
        kl_symbol *ref = sym->ref;
        if (ref->is_callable && sym->level == 1 && strcmp(ctx->scope->name, ref->name) == 0) {
            sym->is_recursive = 1;
        }
    }
    return e;
}

static kl_expr *parse_expr_keyvalue(kl_context *ctx, kl_lexer *l)
{
    kl_expr *e = NULL;
    while (l->tok == TK_NAME || l->tok == TK_VSTR) {
        tk_token tok = l->tok;
        const char *name = parse_const_str(ctx, l, l->str);
        kl_expr *e2 = make_expr(ctx, l, TK_VSTR);
        e2->val.str = name;
        lexer_fetch(l);
        if (l->tok == TK_COLON) {
            lexer_fetch(l);
            kl_expr *e3 = parse_expr_assignment(ctx, l);
            e2 = make_bin_expr(ctx, l, TK_VKV, e2, e3);
            e = make_bin_expr(ctx, l, TK_COMMA, e, e2);
        } else {
            if (tok != TK_NAME) {
                parse_error(ctx, __LINE__, "Compile", l, "The ':' is missing in key value.");
                return panic_mode_expr(e, ';', ctx, l);
            }
            kl_expr *e3 = parse_expr_varname(ctx, l, name);
            e2 = make_bin_expr(ctx, l, TK_VKV, e2, e3);
            e = make_bin_expr(ctx, l, TK_COMMA, e, e2);
        }
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
    }
    return e;
}

static kl_expr *parse_expr_arrayitem(kl_context *ctx, kl_lexer *l)
{
    kl_expr *e = NULL;
    if (l->tok == TK_COMMA) {
        lexer_fetch(l);
    } else {
        e = parse_expr_assignment(ctx, l);
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
    }
    while (l->tok != TK_RLBR && l->tok != TK_EOF) {
        while (l->tok == TK_COMMA) {
            e = make_conn_expr(ctx, l, TK_COMMA, e, NULL);
            lexer_fetch(l);
        }
        if (l->tok == TK_RLBR || l->tok == TK_EOF) {
            e = make_conn_expr(ctx, l, TK_COMMA, e, NULL);
            break;
        }
        kl_expr *e2 = parse_expr_assignment(ctx, l);
        e = make_conn_expr(ctx, l, TK_COMMA, e, e2);
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
    }
    return e;
}

static kl_expr *parse_lvalue_factor(kl_context *ctx, kl_lexer *l)
{
    kl_expr *e = NULL;

    switch (l->tok) {
    case TK_LXBR:
        lexer_fetch(l);
        e = make_expr(ctx, l, TK_VOBJ);
        if (l->tok != TK_RXBR) {
            e->lhs = parse_expr_keyvalue(ctx, l);
            if (l->tok != TK_RXBR) {
                parse_error(ctx, __LINE__, "Compile", l, "The '}' is missing.");
            }
            lexer_fetch(l);
        }
        break;
    case TK_LLBR:
        lexer_fetch(l);
        e = make_expr(ctx, l, TK_VARY);
        if (l->tok != TK_RLBR) {
            e->lhs = parse_expr_arrayitem(ctx, l);
            if (l->tok != TK_RLBR) {
                parse_error(ctx, __LINE__, "Compile", l, "The ']' is missing.");
            }
            lexer_fetch(l);
        }
        break;
    case TK_LSBR:
        lexer_fetch(l);
        e = parse_expression(ctx, l);
        if (l->tok != TK_RSBR) {
            parse_error(ctx, __LINE__, "Compile", l, "The ')' is missing.");
        }
        lexer_fetch(l);
        while (l->tok == TK_LSBR) {
            lexer_fetch(l);
            kl_expr *e2 = parse_expression(ctx, l);
            if (l->tok != TK_RSBR) {
                parse_error(ctx, __LINE__, "Compile", l, "The ')' is missing.");
            }
            e = make_bin_expr(ctx, l, TK_ADD, e, e2);
            lexer_fetch(l);
        }
        break;
    }
    return e;
}

static kl_expr *parse_expr_factor(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *e = parse_lvalue_factor(ctx, l);
    if (e) {
        return e;
    }

    // (expr)
    // variable name
    // int, real, string, array, object, ...
    switch (l->tok) {
    case TK_NAME:
        e = parse_expr_varname(ctx, l, l->str);
        lexer_fetch(l);
        break;
    case TK_LT:
        e = make_expr(ctx, l, TK_VBIN);
        e->typeid = TK_TBIN;
        l->binmode = 1;
        lexer_fetch(l);
        e->lhs = parse_expr_list(ctx, l, TK_BINEND);
        if (l->tok != TK_BINEND) {
            parse_error(ctx, __LINE__, "Compile", l, "The '>' is missing.");
        }
        l->binmode = 0;
        lexer_fetch(l);
        break;
    case TK_VSINT:
        e = make_expr(ctx, l, TK_VSINT);
        e->typeid = TK_TSINT64;
        e->val.i64 = l->i64;
        lexer_fetch(l);
        break;
    case TK_VDBL:
        e = make_expr(ctx, l, TK_VDBL);
        e->typeid = TK_TDBL;
        e->val.dbl = l->dbl;
        lexer_fetch(l);
        break;
    case TK_VBIGINT:
        e = make_expr(ctx, l, TK_VBIGINT);
        e->typeid = TK_TBIGINT;
        e->val.big = parse_const_str(ctx, l, l->str);
        lexer_fetch(l);
        break;
    case TK_VSTR:
        e = make_expr(ctx, l, TK_VSTR);
        e->typeid = TK_TSTR;
        e->val.str = parse_const_str(ctx, l, l->str);
        lexer_fetch(l);
        break;
    default:
        ;
    }

    return e;
}

static kl_expr *parse_expr_list(kl_context *ctx, kl_lexer *l, int endch)
{
    if (l->tok == endch) {
        return NULL;
    }
    kl_expr *lhs = parse_expression(ctx, l);
    if (l->tok == TK_COMMA) {
        lexer_fetch(l);
    }
    while (l->tok != endch && l->tok != TK_EOF) {
        kl_expr *rhs = parse_expression(ctx, l);
        lhs = make_bin_expr(ctx, l, TK_COMMA, lhs, rhs);
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        } else if (l->tok == TK_SEMICOLON) {
            parse_error(ctx, __LINE__, "Compile", l, "Unexpected ';' was found.");
            break;
        }
    }

    return lhs;
}

static kl_expr *parse_expr_postfix(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_factor(ctx, l);
    tk_token tok = l->tok;
    if (tok == TK_INC || tok == TK_DEC) {
        kl_expr *e = make_expr(ctx, l, TK_INC ? TK_INCP : TK_DECP);
        e->lhs = lhs;
        e->typeid = lhs->typeid;
        lexer_fetch(l);
        return e;
    }

    while (tok == TK_LSBR || tok == TK_LLBR || tok == TK_DOT) {
        if (tok == TK_LSBR) {
            lexer_fetch(l);
            kl_expr *rhs = parse_expr_list(ctx, l, TK_RSBR);
            kl_symbol *sym = lhs->sym;
            tk_typeid tid = lhs->sym ? sym->rettype : lhs->typeid;
            lhs = make_bin_expr(ctx, l, TK_CALL, lhs, rhs);
            lhs->typeid = tid;
            if (l->tok != TK_RSBR) {
                parse_error(ctx, __LINE__, "Compile", l, "The ')' is missing.");
                return panic_mode_expr(lhs, ';', ctx, l);
            }
            lexer_fetch(l);
            tok = l->tok;
        } else if (tok == TK_LLBR) {
            lexer_fetch(l);
            kl_expr *rhs = parse_expression(ctx, l);
            lhs = make_bin_expr(ctx, l, TK_IDX, lhs, rhs);
            if (l->tok != TK_RSBR) {
                parse_error(ctx, __LINE__, "Compile", l, "The ']' is missing.");
                return panic_mode_expr(lhs, ';', ctx, l);
            }
            lexer_fetch(l);
            tok = l->tok;
        } else if (tok == TK_DOT) {
            lexer_fetch(l);
            if (l->tok != TK_NAME) {
                parse_error(ctx, __LINE__, "Compile", l, "Property name is needed.");
                return panic_mode_expr(lhs, ';', ctx, l);
            }
            kl_expr *rhs = make_expr(ctx, l, TK_VSTR);
            rhs->typeid = TK_TSTR;
            rhs->val.str = parse_const_str(ctx, l, l->str);
            lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
            lexer_fetch(l);
            tok = l->tok;
        }
    }

    return lhs;
}

static kl_expr *parse_expr_prefix(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = NULL;
    tk_token tok = l->tok;
    if (tok == TK_INC || tok == TK_DEC) {
        lhs = make_expr(ctx, l, tok);
        lexer_fetch(l);
        lhs->lhs = parse_expr_postfix(ctx, l);
        lhs->typeid = lhs->lhs->typeid;
        return lhs;
    }

    while (tok == TK_BNOT || tok == TK_NOT || tok == TK_ADD || tok == TK_SUB) {
        lexer_fetch(l);
        kl_expr *lhs = parse_expr_postfix(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, NULL);
        tok = l->tok;
    }
    return lhs ? lhs : parse_expr_postfix(ctx, l);
}

static kl_expr *parse_expr_term(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_prefix(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_MUL || tok == TK_DIV || tok == TK_MOD || tok == TK_EXP) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_prefix(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_addsub(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_term(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_ADD || tok == TK_SUB) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_term(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_shift(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_addsub(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_LSH || tok == TK_RSH) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_addsub(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_comp(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_shift(ctx, l);
    tk_token tok = l->tok;
    while (TK_LT <= tok && tok <= TK_LGE) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_shift(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_compeq(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_comp(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_EQEQ || tok == TK_NEQ) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_comp(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_bit_and(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_compeq(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_AND) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_compeq(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_bit_xor(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_bit_and(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_XOR) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_bit_and(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_bit_or(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_bit_xor(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_OR) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_bit_xor(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_logical_and(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_bit_or(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_LAND) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_bit_or(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_logical_or(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_logical_and(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_LOR) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_logical_and(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_ternary(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_logical_or(ctx, l);
    tk_token tok = l->tok;
    if (tok == TK_QES) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_logical_or(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        if (l->tok != TK_COLON) {
            parse_error(ctx, __LINE__, "Compile", l, "The ':' is missing in ternary expression.");
            return panic_mode_expr(lhs, ';', ctx, l);
        }
        lexer_fetch(l);
        lhs->xhs = parse_expr_logical_or(ctx, l);
    }
    return lhs;
}

static kl_expr *parse_expr_assignment(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_ternary(ctx, l);
    tk_token tok = l->tok;
    if (TK_EQ <= tok && tok <= TK_LOREQ) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_assignment(ctx, l);   // Right recursion.
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expression(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_assignment(ctx, l);
    tk_token tok = l->tok;
    if (tok == TK_COMMA) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_assignment(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_stmt *parse_expression_stmt(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_EXPR);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_SEMICOLON) {
        parse_error(ctx, __LINE__, "Compile", l, "The ';' is missing.");
        return panic_mode_exprstmt(s, ';', ctx, l);
    }
    lexer_fetch(l);
    return s;
}

static kl_expr *parse_type(kl_context *ctx, kl_lexer *l)
{
    kl_expr *e = NULL;
    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "No type name after ':' in argument.");
        return panic_mode_expr(e, ';', ctx, l);
    }
    lexer_fetch(l);
    do {
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
        if (l->tok == TK_RSBR) {
            // No more types.
            break;
        }
        if (l->tok == TK_TYPEID) {
            kl_expr *e1 = make_expr(ctx, l, TK_TYPENODE);
            e1->typeid = l->type;
            e = make_bin_expr(ctx, l, TK_COMMA, e, e1);
            lexer_fetch(l);
        } else {
            kl_expr *e1 = parse_type(ctx, l);
            e1->typeid = TK_TFUNC;
            e = make_bin_expr(ctx, l, TK_COMMA, e, e1);
        }
    } while (l->tok == TK_COMMA);

    if (l->tok == TK_RSBR) {
        lexer_fetch(l);
    }
    if (l->tok == TK_DARROW) {
        lexer_fetch(l);
        if (l->tok == TK_TYPEID) {
            kl_expr *e1 = make_expr(ctx, l, TK_TYPENODE);
            e1->typeid = l->type;
            e = make_conn_expr(ctx, l, TK_DARROW, e, e1);
            lexer_fetch(l);
        } else {
            kl_expr *e1 = parse_type(ctx, l);
            e1->typeid = TK_TFUNC;
            e = make_conn_expr(ctx, l, TK_DARROW, e, e1);
        }
    } else {
        e = make_conn_expr(ctx, l, TK_DARROW, e, NULL);
    }
    if (e->rhs && e->rhs->typeid) {
        e->typeid = e->rhs->typeid;
    } else {
        e->typeid = TK_TANY;
    }

    return e;
}

static tk_typeid check_rettype(kl_symbol *sym)
{
    kl_expr *tree = sym->typetree;
    if (tree->rhs && tree->rhs->nodetype == TK_TYPENODE) {
        return tree->rhs->typeid;
    }
    return TK_TANY;
}

static kl_expr *parse_type_expr(kl_context *ctx, kl_lexer *l, kl_expr *e, kl_symbol *sym)
{
    lexer_fetch(l);
    if (l->tok == TK_TYPEID) {
        e->typeid = sym->typeid = l->type;
        lexer_fetch(l);
    } else {
        sym->typetree = parse_type(ctx, l);
        e->typeid = sym->typeid = sym->typetree->typeid;
    }
    return e;
}

static kl_stmt *parse_type_stmt(kl_context *ctx, kl_lexer *l, kl_stmt *s, kl_symbol *sym)
{
    lexer_fetch(l);
    if (l->tok == TK_TYPEID) {
        s->typeid = sym->typeid = l->type;
        lexer_fetch(l);
    } else {
        sym->typetree = parse_type(ctx, l);
        s->typeid = sym->typeid = sym->typetree->typeid;
    }
    return s;
}

static kl_expr *parse_def_arglist(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *e = NULL;
    do {
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
        if (l->tok == TK_RSBR) {
            // No more arguments.
            break;
        }
        if (l->tok == TK_NAME) {
            check_symbol(ctx, l, l->str);
            kl_expr *e1 = make_expr(ctx, l, TK_VAR);
            kl_symbol *sym = make_symbol(ctx, l, TK_VAR);
            sym->name = parse_const_str(ctx, l, l->str);
            e1->sym = sym;
            lexer_fetch(l);
            if (l->tok == TK_COLON) {
                parse_type_expr(ctx, l, e1, sym);
                if (sym->typetree && sym->typetree->nodetype == TK_DARROW) {
                    e1->typeid = sym->typeid = TK_TFUNC;
                    sym->rettype = check_rettype(sym);
                }
            }
            e = make_bin_expr(ctx, l, TK_COMMA, e, e1);
        } else {
            if (l->tok == TK_LXBR) {
                lexer_fetch(l);
                kl_expr *e1 = make_expr(ctx, l, TK_VOBJ);
                if (l->tok != TK_RXBR) {
                    e1->lhs = parse_expr_keyvalue(ctx, l);
                    if (l->tok != TK_RXBR) {
                        parse_error(ctx, __LINE__, "Compile", l, "The '}' is missing.");
                    }
                    lexer_fetch(l);
                }
                e = make_bin_expr(ctx, l, TK_COMMA, e, e1);
            } else if (l->tok == TK_LLBR) {
                lexer_fetch(l);
                kl_expr *e1 = make_expr(ctx, l, TK_VARY);
                if (l->tok != TK_RLBR) {
                    e1->lhs = parse_expr_arrayitem(ctx, l);
                    if (l->tok != TK_RLBR) {
                        parse_error(ctx, __LINE__, "Compile", l, "The ']' is missing.");
                    }
                    lexer_fetch(l);
                }
                e = make_bin_expr(ctx, l, TK_COMMA, e, e1);
            } else {
                parse_error(ctx, __LINE__, "Compile", l, "Invalid argument.");
            }
        }
    } while (l->tok == TK_COMMA);
    return e;
}

static kl_stmt *parse_if(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_IF);

    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The '(' is missing.");
        return s;
    }
    lexer_fetch(l);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The ')' is missing.");
        return s;
    }
    lexer_fetch(l);

    /* then part */
    s->s1 = parse_statement(ctx, l);

    /* else part */
    if (l->tok == TK_ELSE) {
        lexer_fetch(l);
        s->s2 = parse_statement(ctx, l);
    }

    return s;
}

static kl_stmt *parse_while(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_WHILE);

    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The '(' is missing.");
        return s;
    }
    lexer_fetch(l);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The ')' is missing.");
        return s;
    }
    lexer_fetch(l);

    /* then part */
    s->s1 = parse_statement(ctx, l);
    return s;
}

static kl_stmt *parse_dowhile(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_DO);

    /* then part */
    s->s1 = parse_statement(ctx, l);

    if (l->tok != TK_WHILE) {
        parse_error(ctx, __LINE__, "Compile", l, "'while' is missing at the end of 'do' block.");
        return s;
    }
    lexer_fetch(l);

    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The '(' is missing.");
        return s;
    }
    lexer_fetch(l);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The ')' is missing.");
        return s;
    }
    lexer_fetch(l);
    if (l->tok != TK_SEMICOLON) {
        parse_error(ctx, __LINE__, "Compile", l, "The ';' is missing.");
        return panic_mode_exprstmt(s, ';', ctx, l);
    }
    lexer_fetch(l);

    return s;
}

static kl_stmt *parse_for(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_FOR);

    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The '(' is missing.");
        return s;
    }

    /* for loop has a block scope because a new variable could be declared in the 1st part. */
    kl_nsstack *n = make_nsstack(ctx, l, "block", TK_BLOCK);
    push_nsstack(ctx, n);
    lexer_fetch(l);
    if (l->tok != TK_SEMICOLON) {
        if (l->tok == TK_LET) {
            lexer_fetch(l);
            s->e1 = parse_decl_expr(ctx, l, 0);
        } else {
            s->e1 = parse_expression(ctx, l);
        }
    }
    if (l->tok == TK_SEMICOLON) {
        lexer_fetch(l);
        if (l->tok != TK_SEMICOLON) {
            /* 2nd part */
            s->e2 = parse_expression(ctx, l);
        }
    }
    if (l->tok == TK_SEMICOLON) {
        lexer_fetch(l);
        if (l->tok != TK_RSBR) {
            /* 3nd part */
            s->e3 = parse_expression(ctx, l);
        }
    }
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The ')' is missing.");
    } else {
        /* then part */
        lexer_fetch(l);
        s->s1 = parse_statement(ctx, l);
    }

    pop_nsstack(ctx);
    return s;
}

static kl_expr *parse_decl_expr(kl_context *ctx, kl_lexer *l, int is_const)
{
    DEBUG_PARSER_PHASE();
    kl_expr *e1 = NULL;

    if (l->tok != TK_NAME) {
        parse_error(ctx, __LINE__, "Compile", l, "The symbol name is needed in declaration.");
    }

    while (l->tok == TK_NAME || l->tok == TK_LLBR || l->tok == TK_LXBR) {
        ctx->in_lvalue = 1;
        kl_expr *lhs = NULL;
        if (l->tok == TK_NAME) {
            check_symbol(ctx, l, l->str);
            lhs = make_expr(ctx, l, TK_VAR);
            kl_symbol *sym = make_symbol(ctx, l, TK_VAR);
            sym->is_const = is_const;
            sym->name = parse_const_str(ctx, l, l->str);
            lhs->sym = sym;
            lexer_fetch(l);
            if (l->tok == TK_COLON) {
                parse_type_expr(ctx, l, lhs, sym);
                if (sym->typetree && sym->typetree->nodetype == TK_DARROW) {
                    lhs->typeid = sym->typeid = TK_TFUNC;
                    sym->rettype = check_rettype(sym);
                }
            }
        } else {
            lhs = parse_lvalue_factor(ctx, l);
        }
        ctx->in_lvalue = 0;

        if (l->tok == TK_EQ) {
            lexer_fetch(l);
            kl_expr *rhs = parse_expr_assignment(ctx, l);
            lhs->typeid = rhs->typeid;
            if (lhs->sym) lhs->sym->typeid = rhs->typeid;
            lhs = make_bin_expr(ctx, l, TK_EQ, lhs, rhs);
        }

        e1 = make_bin_expr(ctx, l, TK_COMMA, e1, lhs);
        if (l->tok != TK_COMMA) {
            break;
        }
        lexer_fetch(l);
        if (l->tok != TK_NAME) {
            parse_error(ctx, __LINE__, "Compile", l, "The symbol name is needed in declaration.");
        }
    }

    return e1;
}

static kl_stmt *parse_declaration(kl_context *ctx, kl_lexer *l, int decltype)
{
    kl_stmt *s = make_stmt(ctx, l, decltype);

    s->e1 = parse_decl_expr(ctx, l, decltype == TK_CONST);
    if (l->tok != TK_SEMICOLON) {
        parse_error(ctx, __LINE__, "Compile", l, "The ';' is missing.");
        return panic_mode_exprstmt(s, ';', ctx, l);
    }
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_return(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_RETURN);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_SEMICOLON) {
        parse_error(ctx, __LINE__, "Compile", l, "The ';' is missing.");
        return panic_mode_exprstmt(s, ';', ctx, l);
    }
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_module(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    return NULL;
}

static kl_stmt *parse_class(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_CLASS);
    kl_symbol *sym = make_symbol(ctx, l, TK_CLASS);
    s->sym = sym;
    sym->is_callable = 1;
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;

    /* The name is needed */
    if (l->tok != TK_NAME) {
        parse_error(ctx, __LINE__, "Compile", l, "Class name is missing.");
        return s;
    }

    /* Push the scope */
    kl_nsstack *n = make_nsstack(ctx, l, l->str, TK_CLASS);
    push_nsstack(ctx, n);

    sym->name = parse_const_str(ctx, l, l->str);
    lexer_fetch(l);

    /* Constructor arguments if exists */
    if (l->tok == TK_LSBR) {
        lexer_fetch(l);
        ctx->in_lvalue = 1;
        s->e1 = sym->args = parse_def_arglist(ctx, l);
        sym->argcount = sym->idxmax;
        ctx->in_lvalue = 0;
        if (l->tok != TK_RSBR) {
            parse_error(ctx, __LINE__, "Compile", l, "The ')' is missing.");
            return s;
        }
        lexer_fetch(l);
    }

    if (l->tok == TK_COLON) {
        lexer_fetch(l);
        if (l->tok != TK_NAME) {
            parse_error(ctx, __LINE__, "Compile", l, "Base class name is missing.");
            return s;
        }
        sym->base = parse_expression(ctx, l);
    }

    /* Class body */
    if (l->tok != TK_LXBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The '{' is missing.");
        return s;
    }
    lexer_fetch(l);
    if (l->tok != TK_RXBR) {
        s->s1 = sym->body = parse_statement_list(ctx, l);
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, "Compile", l, "The '}' is missing.");
            return s;
        }
    }

    ctx->scope = sym->scope;
    pop_nsstack(ctx);
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_function(kl_context *ctx, kl_lexer *l, int funcscope)
{
    DEBUG_PARSER_PHASE();

    kl_stmt *s = make_stmt(ctx, l, TK_FUNC);
    kl_symbol *sym = make_symbol(ctx, l, funcscope);
    s->sym = sym;
    sym->is_callable = 1;
    sym->is_native = funcscope == TK_NATIVE;

    /* The name is not needed for function */
    if (l->tok == TK_NAME) {
        sym->name = parse_const_str(ctx, l, l->str);
        sym->funcname = make_func_name(ctx, l, l->str);
        lexer_fetch(l);
    }
    add_sym2method(ctx->scope, sym);

    /* Push the scope */
    kl_nsstack *n = make_nsstack(ctx, l, sym->name ? sym->name : "anonymous func", TK_FUNC);
    push_nsstack(ctx, n);
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;

    /* Function arguments */
    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The '(' is missing.");
        return panic_mode_stmt(s, '{', ctx, l);
    }
    lexer_fetch(l);
    ctx->in_lvalue = 1;
    s->e1 = sym->args = parse_def_arglist(ctx, l);
    sym->argcount = sym->idxmax;
    ctx->in_lvalue = 0;
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The ')' is missing.");
        return s;
    }

    /* Return type */
    lexer_fetch(l);
    if (l->tok == TK_COLON) {
        parse_type_stmt(ctx, l, s, sym);
        sym->rettype = sym->typeid;
    }
    sym->typeid = TK_TFUNC;

    /* Function body */
    if (l->tok != TK_LXBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The '{' is missing.");
        return s;
    }
    lexer_fetch(l);
    if (l->tok != TK_RXBR) {
        s->s1 = sym->body = parse_statement_list(ctx, l);
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, "Compile", l, "The '}' is missing.");
            return s;
        }
    }

    sym->prototype = create_prototype(ctx, l, sym);
    ctx->scope = sym->scope;
    pop_nsstack(ctx);
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_namespace(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_NAMESPACE);
    kl_symbol *sym = make_symbol(ctx, l, TK_NAMESPACE);
    s->sym = sym;

    kl_nsstack *n = make_nsstack(ctx, l, l->tok == TK_NAME ? l->str : "anonymous ns", TK_NAMESPACE);
    push_nsstack(ctx, n);
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;

    /* No name is okay for namespace */
    if (l->tok == TK_NAME) {
        sym->name = parse_const_str(ctx, l, l->str);
        lexer_fetch(l);
    }

    /* Namespace statement */
    if (l->tok != TK_LXBR) {
        parse_error(ctx, __LINE__, "Compile", l, "The '{' is missing.");
        return s;
    }
    lexer_fetch(l);
    if (l->tok != TK_RXBR) {
        sym->body = s->s1 = parse_statement_list(ctx, l);
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, "Compile", l, "The '}' is missing.");
            return s;
        }
    }

    ctx->scope = sym->scope;
    pop_nsstack(ctx);
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_block(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_BLOCK);

    /* Block statement */
    if (l->tok != TK_RXBR) {
        kl_nsstack *n = make_nsstack(ctx, l, "block", TK_BLOCK);
        push_nsstack(ctx, n);
        s->s1 = parse_statement_list(ctx, l);
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, "Compile", l, "The '}' is missing.");
            return s;
        }
        pop_nsstack(ctx);
    }

    return s;
}

static kl_stmt *parse_statement(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    /*
        namespace, func/function, class, module, let, const
        block
     */
    kl_stmt *r = NULL;
    tk_token tok = l->tok;
    switch (tok) {
    case TK_LXBR:
        lexer_fetch(l);
        r = parse_block(ctx, l);
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, "Compile", l, "The '}' is missing.");
        }
        lexer_fetch(l);
        break;
    case TK_NAMESPACE:
        lexer_fetch(l);
        r = parse_namespace(ctx, l);
        break;
    case TK_PRIVATE:
    case TK_PROTECTED:
    case TK_PUBLIC:
    case TK_FUNC:
    case TK_NATIVE:
        lexer_fetch(l);
        r = parse_function(ctx, l, tok);
        break;
    case TK_CLASS:
        lexer_fetch(l);
        r = parse_class(ctx, l);
        break;
    case TK_MODULE:
        lexer_fetch(l);
        r = parse_module(ctx, l);
        break;
    case TK_IF:
        lexer_fetch(l);
        r = parse_if(ctx, l);
        break;
    case TK_DO:
        lexer_fetch(l);
        r = parse_dowhile(ctx, l);
        break;
    case TK_WHILE:
        lexer_fetch(l);
        r = parse_while(ctx, l);
        break;
    case TK_FOR:
        lexer_fetch(l);
        r = parse_for(ctx, l);
        break;
    case TK_RETURN:
        lexer_fetch(l);
        r = parse_return(ctx, l);
        break;
    case TK_LET:
    case TK_CONST:
        lexer_fetch(l);
        r = parse_declaration(ctx, l, tok);
        break;
    case TK_EOF:
        break;
    default:
        r = parse_expression_stmt(ctx, l);
        break;
    }
    return r;
}

static kl_stmt *parse_statement_list(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    /* statement list */
    kl_stmt *head = NULL;
    kl_stmt *s1 = NULL, *s2 = NULL;
    for ( ; ; ) {
        s2 = parse_statement(ctx, l);
        if (!s1) {
            head = s2;
        }
        s1 = connect_stmt(s1, s2);
        if (s2 == NULL || l->tok == TK_RXBR) {
            break;
        }
    }

    return head;
}

int parse(kl_context *ctx, kl_lexer *l)
{
    kl_nsstack *n = make_nsstack(ctx, l, "run_global", TK_NAMESPACE);
    n->is_global = 1;
    push_nsstack(ctx, n);
    kl_stmt *s = make_stmt(ctx, l, TK_NAMESPACE);
    kl_symbol *sym = make_symbol(ctx, l, TK_NAMESPACE);
    sym->name = parse_const_str(ctx, l, "run_global");
    s->sym = sym;

    ctx->scope = ctx->global = sym;


    lexer_fetch(l);
    s->s1 = parse_statement_list(ctx, l);
    ctx->head = s;

    pop_nsstack(ctx);
    ctx->errors += l->errors;
    return ctx->errors > 0;
}
