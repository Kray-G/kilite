/*
 * Hand-made parser by recursive descent parcing.
 */
#include "error.h"
#include "parser.h"

static kl_expr *parse_expr_factor(kl_context *ctx, kl_lexer *l);
static kl_expr *parse_expr_list(kl_context *ctx, kl_lexer *l, int endch);
static kl_expr *parse_expr_assignment(kl_context *ctx, kl_lexer *l);
static kl_stmt *parse_if_modifier(kl_context *ctx, kl_lexer *l);
static kl_expr *parse_expression(kl_context *ctx, kl_lexer *l);
static kl_expr *parse_decl_expr(kl_context *ctx, kl_lexer *l, int decltype);
static kl_stmt *parse_statement(kl_context *ctx, kl_lexer *l);
static kl_stmt *parse_statement_list(kl_context *ctx, kl_lexer *l);
static kl_expr *parse_block_function(kl_context *ctx, kl_lexer *l, int is_block);
static kl_expr *parse_anonymous_function(kl_context *ctx, kl_lexer *l);

#define DEBUG_PARSER_PHASE()\
if ((ctx->options & PARSER_OPT_PHASE) == PARSER_OPT_PHASE) printf("[parser] %s\n", __func__);\
/**/

static int parse_error(kl_context *ctx, int sline, kl_lexer *l, const char *fmt, ...)
{
    // fprintf(stderr, "%d: ", sline);
    int line = l->tokline;
    int pos = l->tokpos;
    int len = l->toklen;
    ctx->errors++;
    va_list ap;
    va_start(ap, fmt);
    int r = error_print((ctx->options & PARSER_OPT_ERR_STDOUT) == PARSER_OPT_ERR_STDOUT, ctx->filename, line, pos, len, fmt, ap);
    va_end(ap);

    if (ctx->error_limit < ctx->errors) {
        fprintf(stderr, "... Sorry, a lot of errors occured, and the program stopped.\n");
        exit(1);
    }
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

char *const_str(kl_context *ctx, const char *str)
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
    return const_str(ctx, str);
}

static char *parse_const_varidname(kl_context *ctx, kl_lexer *l, int id)
{
    char str[32] = {0};
    snprintf(str, 30, "v%d", id);
    return const_str(ctx, str);
}

static char *parse_const_funcidname(kl_context *ctx, kl_lexer *l, int id)
{
    char str[32] = {0};
    snprintf(str, 30, "anonymous_func%d", id);
    return const_str(ctx, str);
}

kl_context *parser_new_context(void)
{
    kl_context *ctx = (kl_context *)calloc(1, sizeof(kl_context));
    ctx->error_limit = PARSE_ERRORLIMIT;
    return ctx;
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

static inline int append_module_name(char *buf, int pos)
{
    /* This prefix is to make risk lower used by users. */
    buf[pos++] = '_'; buf[pos++] = '_';
    buf[pos++] = 'e'; buf[pos++] = 'x'; buf[pos++] = 't'; buf[pos++] = 'e';buf[pos++] = 'n'; buf[pos++] = 'd';
    return pos;
}

static inline int append_constructor_name(char *buf, int pos)
{
    /* This prefix is to make risk lower used by users. */
    buf[pos++] = '_'; buf[pos++] = '_';
    buf[pos++] = 'c'; buf[pos++] = 'r'; buf[pos++] = 'e'; buf[pos++] = 'a';buf[pos++] = 't'; buf[pos++] = 'e';
    return pos;
}

static int copy_func_name(char *d, const char *s)
{
    int len = 0;
    while (*s) {
        if (isalnum(*s) || *s == '_') {
            *d++ = *s++;
            ++len;
        } else {
            *d++ = '_'; *d++ = 'x'; *d++ = '_';
            ++s;
            len += 3;
        }
    }
    return len;
}

static char *make_func_name(kl_context *ctx, kl_lexer *l, const char *str, tk_token type, int id)
{
    int makelib_opt = (ctx->options & PARSER_OPT_MAKELIB) == PARSER_OPT_MAKELIB;
    char buf[1024] = {0};
    int pos = 0;
    int len = strlen(str);
    if (pos + len > 1016) {
        parse_error(ctx, __LINE__, l, "Internal error with allocation failed");
    } else {
        kl_nsstack *n = ctx->ns;
        if (!makelib_opt) {
            sprintf(buf, "kl%03d_", id);
            pos = 6;
        }
        int added = copy_func_name(buf + pos, str);
        if (makelib_opt && added != len) {
            parse_error(ctx, __LINE__, l, "Found the word which can't be used in -makelib mode");
        } else {
            pos += added;
            switch (type) {
            case TK_CLASS:
                pos = append_constructor_name(buf, pos);
                break;
            case TK_MODULE:
                pos = append_module_name(buf, pos);
                break;
            }
            if (n->prev) {
                pos = append_ns_prefix(buf, pos);
            }
            buf[pos] = 0;
            while (n->prev) {   /* skips a global namespace. */
                buf[pos++] = '_';
                int len = strlen(n->name);
                if (pos + len + 3 > 1016) {
                    parse_error(ctx, __LINE__, l, "Internal error with allocation failed");
                    break;
                }
                strcpy(buf + pos, n->name);
                pos += len;
                n = n->prev;
            }
        }
    }
    return const_str(ctx, buf);
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

static inline int is_global_namespace(kl_context *ctx)
{
    return ctx->ns->is_global;
}

static inline int is_type_token(tk_token tok)
{
    return tok == TK_TYPEID || tok == TK_FUNC;
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

static inline const char *make_class_func_name(kl_context *ctx, kl_lexer *l, const char *name)
{
    kl_symbol *sym = ctx->scope;
    if (sym) {
        sym = sym->scope;
        if (sym && sym->symtoken == TK_CLASS) {
            char buf[1024] = {0};
            snprintf(buf, 1000, "%s#%s", sym->name, name);
            return parse_const_str(ctx, l, buf);
        }
    }
    return parse_const_str(ctx, l, name);
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

static inline kl_symbol *search_symbol(kl_context *ctx, kl_lexer *l, const char *name)
{
    kl_nsstack *ns = ctx->ns;
    while (ns) {
        kl_symbol *ref = search_symbol_in_scope(ctx, l, ns, name);
        if (ref) {
            return ref;
        }
        ns = ns->prev;
    }
    return NULL;
}

static inline kl_symbol *make_ref_symbol(kl_context *ctx, kl_lexer *l, tk_token tk, const char *name)
{
    int level = 0;
    kl_symbol *sym = NULL;
    kl_nsstack *ns = ctx->ns;
    while (ns) {
        kl_symbol *ref = search_symbol_in_scope(ctx, l, ns, name);
        if (ref) {
            ref->refcount++;
            sym = (kl_symbol *)calloc(1, sizeof(kl_symbol));
            sym->chn = ctx->symchn;
            ctx->symchn = sym;
            sym->symtoken = tk;
            sym->line = l->tokline;
            sym->pos = l->tokpos;
            sym->ref = ref;
            sym->name = ref->name;
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

    return sym;
}

static inline kl_symbol *make_symbol(kl_context *ctx, kl_lexer *l, tk_token tk, int noindex)
{
    kl_symbol *sym = (kl_symbol *)calloc(1, sizeof(kl_symbol));
    sym->chn = ctx->symchn;
    ctx->symchn = sym;
    sym->symtoken = tk;
    sym->filename = ctx->filename;
    sym->index = noindex ? 0 : (ctx->scope ? ctx->scope->idxmax++ : 0);
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

static inline kl_expr *make_bool_expr(kl_context *ctx, kl_lexer *l, int64_t i64)
{
    kl_expr *e = make_expr(ctx, l, TK_VBOOL);
    e->typeid = TK_TBOOL;
    e->val.i64 = i64;
    return e;
}

static inline kl_expr *make_i64_expr(kl_context *ctx, kl_lexer *l, int64_t i64)
{
    kl_expr *e = make_expr(ctx, l, TK_VSINT);
    e->typeid = TK_TSINT64;
    e->val.i64 = i64;
    return e;
}

static inline kl_expr *make_dbl_expr(kl_context *ctx, kl_lexer *l, const char *dbl)
{
    kl_expr *e = make_expr(ctx, l, TK_VDBL);
    e->typeid = TK_TDBL;
    e->val.dbl = parse_const_str(ctx, l, dbl);
    return e;
}

static inline kl_expr *make_str_expr(kl_context *ctx, kl_lexer *l, const char *str)
{
    kl_expr *e = make_expr(ctx, l, TK_VSTR);
    e->typeid = TK_TSTR;
    e->val.str = parse_const_str(ctx, l, str);
    return e;
}

static inline kl_expr *make_regex_expr(kl_context *ctx, kl_lexer *l, const char *str)
{
    kl_expr *e = make_expr(ctx, l, TK_VREGEX);
    e->typeid = TK_TOBJ;
    e->val.str = parse_const_str(ctx, l, str);
    return e;
}

static kl_expr *gen_specialized_call(kl_context *ctx, kl_lexer *l, tk_token tk, kl_expr *lhs, kl_expr *rhs)
{
    /* e->nodetype should be TK_CALL */
    kl_expr *sp = NULL;
    if (lhs && lhs->nodetype == TK_DOT) {
        kl_expr *ll = lhs->lhs;
        kl_expr *lr = lhs->rhs;
        if (ll && lr && ll->nodetype == TK_VAR && ll->typeid == TK_TOBJ && lr->nodetype == TK_VSTR) {
            if (strcmp(lr->val.str, "size") == 0 || strcmp(lr->val.str, "length") == 0) {
                sp = make_expr(ctx, l, TK_ARYSIZE);
                sp->lhs = ll;
            }
        }
    }
    return sp;
}


static inline kl_expr *make_bin_expr(kl_context *ctx, kl_lexer *l, tk_token tk, kl_expr *lhs, kl_expr *rhs)
{
    if (!lhs) return rhs;
    if (tk == TK_CALL && !ctx->in_finally) {
        kl_expr *sp = gen_specialized_call(ctx, l, tk, lhs, rhs);
        if (sp) {
            return sp;
        }
    }

    kl_expr *e = make_expr(ctx, l, tk);
    if (tk == TK_CALL && !ctx->in_finally) {
        ++(ctx->scope->yield);
        e->yield = ctx->scope->yield;   /* This number will be used when yield check. */
    }
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
    free(ctx->timer);
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
        parse_error(ctx, __LINE__, l, "The symbol(%s) was already declared in this scope", chk->name);
    }
}

static void check_assigned_dot(kl_context *ctx, kl_lexer *l, kl_expr *lhs)
{
    if (lhs->nodetype == TK_VAR && lhs->sym) {
        kl_symbol *v = lhs->sym->ref ? lhs->sym->ref : lhs->sym;
        v->is_autoset = 0;    /* clear this flag when it's lvalue. */
    } else {
        if (lhs->lhs) {
            check_assigned_dot(ctx, l, lhs->lhs);
        }
        if (lhs->rhs) {
            check_assigned_dot(ctx, l, lhs->rhs);
        }
    }
}

static void check_assigned(kl_context *ctx, kl_lexer *l, kl_expr *lhs)
{
    if (!lhs) {
        return;
    }
    if (lhs->nodetype == TK_VAR && lhs->sym) {
        kl_symbol *v = lhs->sym->ref ? lhs->sym->ref : lhs->sym;
        v->is_autoset = 0;    /* clear this flag when it's lvalue. */
        v->assigned++;
        if (v->is_const && (v->assigned > 1 || v->has_i64)) {
            parse_error(ctx, __LINE__, l, "Can not assign a value to the 'const' variable");
        }
    } else if (lhs->nodetype == TK_DOT) {
        check_assigned_dot(ctx, l, lhs);
    } else {
        if (lhs->lhs) {
            check_assigned(ctx, l, lhs->lhs);
        }
        if (lhs->nodetype != TK_IDX) {
            if (lhs->rhs) {
                check_assigned(ctx, l, lhs->rhs);
            }
            if (lhs->xhs) {
                check_assigned(ctx, l, lhs->xhs);
            }
        }
    }
}

static int get_var_num(kl_expr *e, int *idx, int num)
{
    if (!e) {
        return -1;
    }
    if (e->nodetype == TK_COMMA) {
        int n = get_var_num(e->lhs, idx, num);
        if (n >= 0) {
            return n;
        }
        return get_var_num(e->rhs, idx, num);
    } else {
        if (*idx == num && e->sym) {
            return e->sym->index;
        }
        ++*idx;
    }
    return -1;
}

static kl_symbol *set_placeholder(kl_context *ctx, kl_lexer *l, const char *name)
{
    if (name[0] == '_') {
        int num = 0;
        switch (name[1]) {
        case '1': num = 1; break;
        case '2': num = 2; break;
        case '3': num = 3; break;
        case '4': num = 4; break;
        case '5': num = 5; break;
        case '6': num = 6; break;
        case '7': num = 7; break;
        case '8': num = 8; break;
        case '9': num = 9; break;
        }
        if (num > 0) {
            kl_symbol *sym = make_symbol(ctx, l, TK_VAR, 1);
            if (ctx->scope->placemax < num) {
                ctx->scope->placemax = num;
            }
            int idx = 0;
            int n = get_var_num(ctx->scope->args, &idx, num-1);
            sym->index = n >= 0 ? n : num - 1;
            sym->is_const = 1;
            return sym;
        }
    }
    return NULL;
}

static kl_expr *parse_expr_varname(kl_context *ctx, kl_lexer *l, const char *name, int decltype)
{
    if (!name) {
        char buf[256] = {0};
        sprintf(buf, "_tmp%d", (ctx->tmpnum)++);
        name = parse_const_str(ctx, l, buf);
    }

    kl_symbol *sym;
    if (decltype) {
        sym = set_placeholder(ctx, l, name);
        if (!sym) {
            check_symbol(ctx, l, name);
            sym = make_symbol(ctx, l, TK_VAR, 0);
            sym->is_const = decltype == TK_CONST;
        }
    } else {
        sym = make_ref_symbol(ctx, l, TK_VAR, name);
        if (!sym) {
            sym = set_placeholder(ctx, l, name);
            if (!sym) {
                sym = make_symbol(ctx, l, TK_VAR, 0);
                sym->is_autoset = 1;
            }
        }
    }
    sym->name = parse_const_str(ctx, l, name);

    kl_expr *e = make_expr(ctx, l, TK_VAR);
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
    while (l->tok == TK_DOT3 || l->tok == TK_NAME || l->tok == TK_VSTR || l->tok == TK_LSBR) {
        tk_token tok = l->tok;
        const char *name = NULL;
        if (tok == TK_LSBR) {
            lexer_fetch(l);
            if (l->tok != TK_VSTR) {
                parse_error(ctx, __LINE__, l, "The key name should be a string");
                return panic_mode_expr(e, ')', ctx, l);
            }
            tok = TK_VSTR;
            name = parse_const_str(ctx, l, l->str);
            lexer_fetch(l);
            if (l->tok != TK_RSBR) {
                parse_error(ctx, __LINE__, l, "Invalid key string");
                return panic_mode_expr(e, ')', ctx, l);
            }
        }
        if (tok == TK_DOT3) {
            lexer_fetch(l);
            if (l->tok == TK_NAME) {
                kl_expr *e1 = parse_expr_varname(ctx, l, l->str, ctx->in_lvalue);
                e1->sym->is_dot3 = 1;
                e = make_bin_expr(ctx, l, TK_COMMA, e, e1);
                lexer_fetch(l);
            } else {
                kl_expr *e1 = parse_expr_factor(ctx, l);
                e1 = make_conn_expr(ctx, l, TK_DOT3, e1, NULL);
                e = make_bin_expr(ctx, l, TK_COMMA, e, e1);
            }
        } else {
            if (!name) {
                name = parse_const_str(ctx, l, l->str);
            }
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
                    parse_error(ctx, __LINE__, l, "The ':' is missing in key value");
                    return panic_mode_expr(e, ';', ctx, l);
                }
                kl_expr *e3 = parse_expr_varname(ctx, l, name, ctx->in_lvalue);
                e2 = make_bin_expr(ctx, l, TK_VKV, e2, e3);
                e = make_bin_expr(ctx, l, TK_COMMA, e, e2);
            }
        }
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
    }
    return e;
}

static inline kl_stmt *add_method2class(kl_context *ctx, kl_lexer *l, kl_symbol *sym, tk_token funcscope)
{
    if (ctx->scope->symtoken != TK_CLASS && ctx->scope->symtoken != TK_MODULE) {
        return NULL;
    }

    kl_stmt *s = NULL;
    switch (funcscope) {
    case TK_PROTECTED:
    case TK_PUBLIC: {
        s = make_stmt(ctx, l, TK_EXPR);
        kl_expr *ell = parse_expr_varname(ctx, l, "this", ctx->in_lvalue);
        kl_expr *elr = make_str_expr(ctx, l, sym->name);
        kl_expr *el = make_bin_expr(ctx, l, TK_DOT, ell, elr);
        kl_expr *er = parse_expr_varname(ctx, l, sym->name, ctx->in_lvalue);
        s->e1 = make_bin_expr(ctx, l, TK_EQ, el, er);
        break;
    }
    // case TK_PRIVATE:
    default:
        break;
    }

    return s;
}

#define PARSER_MAKE_SPECIAL_NAME(l, s) { l->tok = TK_NAME; strcpy(l->str, s); }
#define PARSER_MAKE_SPECIAL_NAME2(l, ntok, s) { lexer_fetch(l); if (l->tok != ntok) { return NULL; } l->tok = TK_NAME; strcpy(l->str, s); }

static const char *parse_special_funcname(kl_context *ctx, kl_lexer *l)
{
    switch (l->tok) {
    case TK_EXTERN:
        PARSER_MAKE_SPECIAL_NAME(l, "extern");
        break;
    case TK_ENUM:
        PARSER_MAKE_SPECIAL_NAME(l, "enum");
        break;
    case TK_CONST:
        PARSER_MAKE_SPECIAL_NAME(l, "const");
        break;
    case TK_LET:
        PARSER_MAKE_SPECIAL_NAME(l, "let");
        break;
    case TK_NEW:
        PARSER_MAKE_SPECIAL_NAME(l, "new");
        break;
    case TK_IMPORT:
        PARSER_MAKE_SPECIAL_NAME(l, "import");
        break;
    case TK_NAMESPACE:
        PARSER_MAKE_SPECIAL_NAME(l, "namespace");
        break;
    case TK_MODULE:
        PARSER_MAKE_SPECIAL_NAME(l, "module");
        break;
    case TK_CLASS:
        PARSER_MAKE_SPECIAL_NAME(l, "class");
        break;
    case TK_PRIVATE:
        PARSER_MAKE_SPECIAL_NAME(l, "private");
        break;
    case TK_PROTECTED:
        PARSER_MAKE_SPECIAL_NAME(l, "protected");
        break;
    case TK_PUBLIC:
        PARSER_MAKE_SPECIAL_NAME(l, "public");
        break;
    case TK_MIXIN:
        PARSER_MAKE_SPECIAL_NAME(l, "mixin");
        break;
    case TK_NATIVE:
        PARSER_MAKE_SPECIAL_NAME(l, "native");
        break;
    case TK_SWITCH:
        PARSER_MAKE_SPECIAL_NAME(l, "switch");
        break;
    case TK_CASE:
        PARSER_MAKE_SPECIAL_NAME(l, "case");
        break;
    case TK_DEFAULT:
        PARSER_MAKE_SPECIAL_NAME(l, "default");
        break;
    case TK_OTHERWISE:
        PARSER_MAKE_SPECIAL_NAME(l, "otherwise");
        break;
    case TK_IF:
        PARSER_MAKE_SPECIAL_NAME(l, "if");
        break;
    case TK_ELSE:
        PARSER_MAKE_SPECIAL_NAME(l, "else");
        break;
    case TK_DO:
        PARSER_MAKE_SPECIAL_NAME(l, "do");
        break;
    case TK_WHILE:
        PARSER_MAKE_SPECIAL_NAME(l, "while");
        break;
    case TK_FOR:
        PARSER_MAKE_SPECIAL_NAME(l, "for");
        break;
    case TK_IN:
        PARSER_MAKE_SPECIAL_NAME(l, "in");
        break;
    case TK_RETURN:
        PARSER_MAKE_SPECIAL_NAME(l, "return");
        break;
    case TK_WHEN:
        PARSER_MAKE_SPECIAL_NAME(l, "when");
        break;
    case TK_FALLTHROUGH:
        PARSER_MAKE_SPECIAL_NAME(l, "fallthrough");
        break;
    case TK_BREAK:
        PARSER_MAKE_SPECIAL_NAME(l, "break");
        break;
    case TK_CONTINUE:
        PARSER_MAKE_SPECIAL_NAME(l, "continue");
        break;
    case TK_YIELD:
        PARSER_MAKE_SPECIAL_NAME(l, "yield");
        break;
    case TK_TRY:
        PARSER_MAKE_SPECIAL_NAME(l, "try");
        break;
    case TK_CATCH:
        PARSER_MAKE_SPECIAL_NAME(l, "catch");
        break;
    case TK_FINALLY:
        PARSER_MAKE_SPECIAL_NAME(l, "finally");
        break;
    case TK_THROW:
        PARSER_MAKE_SPECIAL_NAME(l, "throw");
        break;
    case TK_EQEQ:
        PARSER_MAKE_SPECIAL_NAME(l, "==");
        break;
    case TK_NEQ:
        PARSER_MAKE_SPECIAL_NAME(l, "!=");
        break;
    case TK_LT:
        PARSER_MAKE_SPECIAL_NAME(l, "<");
        break;
    case TK_LE:
        PARSER_MAKE_SPECIAL_NAME(l, "<=");
        break;
    case TK_GT:
        PARSER_MAKE_SPECIAL_NAME(l, ">");
        break;
    case TK_GE:
        PARSER_MAKE_SPECIAL_NAME(l, ">=");
        break;
    case TK_LGE:
        PARSER_MAKE_SPECIAL_NAME(l, "<=>");
        break;
    case TK_LSH:
        PARSER_MAKE_SPECIAL_NAME(l, "<<");
        break;
    case TK_RSH:
        PARSER_MAKE_SPECIAL_NAME(l, ">>");
        break;
    case TK_ADD:
        PARSER_MAKE_SPECIAL_NAME(l, "+");
        break;
    case TK_SUB:
        PARSER_MAKE_SPECIAL_NAME(l, "-");
        break;
    case TK_MUL:
        PARSER_MAKE_SPECIAL_NAME(l, "*");
        break;
    case TK_DIV:
        PARSER_MAKE_SPECIAL_NAME(l, "/");
        break;
    case TK_MOD:
        PARSER_MAKE_SPECIAL_NAME(l, "%");
        break;
    case TK_LSBR:
        PARSER_MAKE_SPECIAL_NAME2(l, TK_RSBR, "()");
        break;
    case TK_LLBR:
        PARSER_MAKE_SPECIAL_NAME2(l, TK_RLBR, "[]");
        break;
    case TK_ARROW:
        PARSER_MAKE_SPECIAL_NAME(l, "->");
        break;
    case TK_DARROW:
        PARSER_MAKE_SPECIAL_NAME(l, "=>");
        break;
    default:
        return NULL;
    }
    return parse_const_str(ctx, l, l->str);
}

#define is_underscore_name(l) ((l)->tok == TK_NAME && (l)->str[0] == '_' && (l)->str[1] == 0)

static kl_expr *parse_expr_arrayitem(kl_context *ctx, kl_lexer *l)
{
    kl_expr *e = NULL;
    if (is_underscore_name(l)) {
        lexer_fetch(l);
    }
    if (l->tok == TK_COMMA) {
        lexer_fetch(l);
    } else {
        e = parse_expr_assignment(ctx, l);
        if (is_underscore_name(l)) {
            lexer_fetch(l);
        }
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
    }
    while (l->tok != TK_RLBR && l->tok != TK_EOF) {
        if (is_underscore_name(l)) {
            lexer_fetch(l);
        }
        while (l->tok == TK_COMMA) {
            e = make_conn_expr(ctx, l, TK_COMMA, e, NULL);
            lexer_fetch(l);
            if (is_underscore_name(l)) {
                lexer_fetch(l);
            }
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
        if (l->tok == TK_AND || l->tok == TK_DARROW) {
            if (ctx->in_lvalue) {
                parse_error(ctx, __LINE__, l, "The block function can't be the l-value");
            }
            return parse_block_function(ctx, l, 1);
        }
        e = make_expr(ctx, l, TK_VOBJ);
        if (l->tok != TK_RXBR) {
            e->lhs = parse_expr_keyvalue(ctx, l);
            if (l->tok != TK_RXBR) {
                parse_error(ctx, __LINE__, l, "The '}' is missing");
            }
        }
        lexer_fetch(l);
        break;
    case TK_LLBR:
        lexer_fetch(l);
        e = make_expr(ctx, l, TK_VARY);
        if (l->tok != TK_RLBR) {
            e->lhs = parse_expr_arrayitem(ctx, l);
            if (l->tok != TK_RLBR) {
                parse_error(ctx, __LINE__, l, "The ']' is missing");
            }
        }
        lexer_fetch(l);
        break;
    case TK_LSBR:
        lexer_fetch(l);
        e = parse_expression(ctx, l);
        if (l->tok != TK_RSBR) {
            parse_error(ctx, __LINE__, l, "The ')' is missing");
        }
        lexer_fetch(l);
        while (l->tok == TK_LSBR && e->nodetype == TK_VSTR) {
            lexer_fetch(l);
            kl_expr *e2 = parse_expression(ctx, l);
            if (l->tok != TK_RSBR) {
                parse_error(ctx, __LINE__, l, "The ')' is missing");
            }
            e = make_bin_expr(ctx, l, TK_ADD, e, e2);
            lexer_fetch(l);
        }
        break;
    }

    return e;
}

static kl_expr *parse_regex_literal(kl_context *ctx, kl_lexer *l)
{
    if (l->tok != TK_DIV && l->tok != TK_DIVEQ && l->tok != TK_MLIT) {
        return NULL;
    }

    char buf[2048] = {0};
    char *p = buf;
    if (l->tok == TK_MLIT) {
        lexer_raw(l, p, 2040, 'm');
    } else if (l->tok == TK_DIVEQ) {
        *p++ = '=';
        lexer_raw(l, p, 2040, '/');
    } else {
        lexer_raw(l, p, 2040, '/');
    }

    kl_expr *e = make_regex_expr(ctx, l, buf);
    lexer_get_regex_flags(l);
    e->strx = parse_const_str(ctx, l, l->str);

    lexer_fetch(l);
    return e;
}

static kl_expr *parse_expr_factor(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *e = parse_lvalue_factor(ctx, l);
    if (e) {
        return e;
    }
    e = parse_regex_literal(ctx, l);
    if (e) {
        return e;
    }

    // (expr)
    // variable name
    // int, real, string, array, object, ...
    switch (l->tok) {
    case TK_DOT3:
        lexer_fetch(l);
        if (l->tok == TK_NAME) {
            e = parse_expr_varname(ctx, l, l->str, ctx->in_lvalue);
            e->sym->is_dot3 = 1;
            lexer_fetch(l);
        } else {
            e = parse_expr_factor(ctx, l);
            e = make_conn_expr(ctx, l, TK_DOT3, e, NULL);
        }
        break;
    case TK_AND:
        if (ctx->in_lvalue) {
            parse_error(ctx, __LINE__, l, "The block function can't be the l-value");
        }
        e = parse_block_function(ctx, l, 0);
        break;
    case TK_XOR:
        lexer_fetch(l);
        if (l->tok != TK_NAME) {
            parse_error(ctx, __LINE__, l, "The pin operator should be attached to the variable");
            return e;
        }
        e = parse_expr_varname(ctx, l, l->str, ctx->in_lvalue);
        e->sym->is_pinvar = 1;
        lexer_fetch(l);
        break;
    case TK_NAME:
        e = parse_expr_varname(ctx, l, l->str, ctx->in_lvalue);
        lexer_fetch(l);
        break;
    case TK_LT:
        e = make_expr(ctx, l, TK_VBIN);
        e->typeid = TK_TBIN;
        l->binmode = 1;
        lexer_fetch(l);
        e->lhs = parse_expr_list(ctx, l, TK_BINEND);
        if (l->tok != TK_BINEND) {
            parse_error(ctx, __LINE__, l, "The '>' is missing");
        }
        l->binmode = 0;
        lexer_fetch(l);
        break;
    case TK_VBOOL:
        e = make_bool_expr(ctx, l, l->i64);
        lexer_fetch(l);
        break;
    case TK_VSINT:
        e = make_i64_expr(ctx, l, l->i64);
        lexer_fetch(l);
        break;
    case TK_VDBL:
        e = make_dbl_expr(ctx, l, l->str);
        lexer_fetch(l);
        break;
    case TK_VBIGINT:
        e = make_expr(ctx, l, TK_VBIGINT);
        e->typeid = TK_TBIGINT;
        e->val.big = parse_const_str(ctx, l, l->str);
        lexer_fetch(l);
        break;
    case TK_VSTR:
        e = make_str_expr(ctx, l, l->str);
        lexer_fetch(l);
        break;
    case TK_HEREDOC:
        e = make_str_expr(ctx, l, l->heredoc);
        lexer_fetch(l);
        break;
    case TK_FUNC:
        lexer_fetch(l);
        e = parse_anonymous_function(ctx, l);
        break;
    default:
        parse_error(ctx, __LINE__, l, "Found the unknown factor in an expression(%s)", tokenname(l->tok));
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
            parse_error(ctx, __LINE__, l, "Unexpected ';' was found");
            break;
        }
    }

    return lhs;
}

static kl_expr *parse_expr_postfix(kl_context *ctx, kl_lexer *l, int is_new)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_factor(ctx, l);
    tk_token tok = l->tok;

    while (tok == TK_LSBR || tok == TK_LXBR || tok == TK_LLBR || tok == TK_DOT || tok == TK_DOT2 || tok == TK_DOT3 || tok == TK_INC || tok == TK_DEC) {
        if (tok == TK_INC || tok == TK_DEC) {
            kl_expr *e = make_expr(ctx, l, tok == TK_INC ? TK_INCP : TK_DECP);
            e->lhs = lhs;
            e->typeid = lhs->typeid;
            lhs = e;
            lexer_fetch(l);
            tok = l->tok;
        } else if (tok == TK_LSBR) {
            lexer_fetch(l);
            kl_expr *rhs = parse_expr_list(ctx, l, TK_RSBR);
            kl_symbol *sym = lhs->sym;
            tk_typeid tid = sym ? sym->rettype : lhs->typeid;
            lhs = make_bin_expr(ctx, l, TK_CALL, lhs, rhs);
            lhs->typeid = tid;
            if (l->tok != TK_RSBR) {
                parse_error(ctx, __LINE__, l, "The ')' is missing");
                return panic_mode_expr(lhs, ';', ctx, l);
            }
            if (is_new) {
                lhs->lhs = make_bin_expr(ctx, l, TK_DOT, lhs->lhs, make_str_expr(ctx, l, "create"));
                is_new = 0; /* `new` operator will take effect to only the first call operation. */
            }
            lexer_fetch(l);
            if (l->tok == TK_LXBR) {
                lexer_fetch(l);
                kl_expr *block = parse_block_function(ctx, l, 1);
                lhs->rhs = make_bin_expr(ctx, l, TK_COMMA, lhs->rhs, block);
            }
            tok = l->tok;
        } else if (tok == TK_LXBR) {
            if (ctx->in_casewhen) {
                break;
            }
            kl_symbol *sym = lhs->sym;
            tk_typeid tid = sym ? sym->rettype : lhs->typeid;
            if (is_new) {
                lhs = make_bin_expr(ctx, l, TK_DOT, lhs, make_str_expr(ctx, l, "create"));
                is_new = 0; /* `new` operator will take effect to only the first call operation. */
            }
            lexer_fetch(l);
            kl_expr *block = parse_block_function(ctx, l, 1);
            lhs = make_bin_expr(ctx, l, TK_CALL, lhs, block);
            lhs->typeid = tid;
            tok = l->tok;
        } else if (tok == TK_LLBR) {
            lexer_fetch(l);
            kl_expr *rhs = parse_expression(ctx, l);
            lhs = make_bin_expr(ctx, l, TK_IDX, lhs, rhs);
            if (l->tok != TK_RLBR) {
                parse_error(ctx, __LINE__, l, "The ']' is missing");
                return panic_mode_expr(lhs, ';', ctx, l);
            }
            lexer_fetch(l);
            tok = l->tok;
        } else if (tok == TK_DOT) {
            lexer_fetch(l);
            const char *name = (is_type_token(l->tok))
                ? typeidname(l->typeid)
                : (l->tok == TK_NAME ? l->str : parse_special_funcname(ctx, l));
            if (!name || name[0] == 0) {
                parse_error(ctx, __LINE__, l, "Property name is missing or invalid");
                return panic_mode_expr(lhs, ';', ctx, l);
            }
            kl_expr *rhs = make_expr(ctx, l, TK_VSTR);
            rhs->typeid = TK_TSTR;
            rhs->val.str = parse_const_str(ctx, l, l->str);
            lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
            lexer_fetch(l);
            tok = l->tok;
        } else if (tok == TK_DOT2 || tok == TK_DOT3) {
            lexer_fetch(l);
            kl_expr *rhs = parse_expr_factor(ctx, l);
            lhs = make_bin_expr(ctx, l, tok == TK_DOT2 ? TK_RANGE2 : TK_RANGE3, lhs, rhs);
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
    if (tok == TK_NEW) {
        lexer_fetch(l);
        lhs = parse_expr_postfix(ctx, l, 1);
        return lhs;
    }

    if (tok == TK_ADD) {
        lexer_fetch(l); // ignore this.
        lhs = parse_expr_postfix(ctx, l, 0);
        if (lhs->nodetype == TK_VBOOL) {
            lhs->nodetype = TK_VSINT;
            lhs->typeid = TK_TSINT64;
        }
        return lhs;
    }

    if (tok == TK_SUB) {
        lexer_fetch(l);
        lhs = parse_expr_postfix(ctx, l, 0);
        if (lhs->nodetype == TK_VSINT) {
            lhs->val.i64 = -(lhs->val.i64);
            return lhs;
        }
        if (lhs->nodetype == TK_VBOOL) {
            lhs->nodetype = TK_VSINT;
            lhs->typeid = TK_TSINT64;
            lhs->val.i64 = -(lhs->val.i64);
            return lhs;
        }
        if (lhs->nodetype == TK_VDBL) {
            char buf[256] = {0};
            buf[0] = '-';
            strcpy(buf+1, lhs->val.dbl);
            lhs->val.dbl = parse_const_str(ctx, l, buf);
            return lhs;
        }
        kl_expr *e = make_expr(ctx, l, TK_MINUS);
        e->lhs = lhs;
        e->typeid = lhs->typeid;
        return e;
    }

    if (tok == TK_MUL) {
        lhs = make_expr(ctx, l, TK_CONV);
        lexer_fetch(l);
        lhs->lhs = parse_expr_postfix(ctx, l, 0);
        lhs->typeid = lhs->typeid;
        return lhs;
    }

    if (tok == TK_INC || tok == TK_DEC) {
        lhs = make_expr(ctx, l, tok);
        lexer_fetch(l);
        lhs->lhs = parse_expr_postfix(ctx, l, 0);
        lhs->typeid = lhs->lhs->typeid;
        return lhs;
    }

    if (tok == TK_NOT) {
        lexer_fetch(l);
        kl_expr *lhs = parse_expr_prefix(ctx, l);   // Right recursion.
        lhs = make_bin_expr(ctx, l, tok, lhs, NULL);
        return lhs;
    }

    if (tok == TK_BNOT || tok == TK_ADD || tok == TK_SUB) {
        lexer_fetch(l);
        kl_expr *lhs = parse_expr_postfix(ctx, l, 0);
        lhs = make_bin_expr(ctx, l, tok, lhs, NULL);
        return lhs;
    }

    return parse_expr_postfix(ctx, l, 0);
}

static kl_expr *parse_expr_regmatch(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_prefix(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_REGEQ || tok == TK_REGNE) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_prefix(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_term(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_regmatch(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_MUL || tok == TK_DIV || tok == TK_MOD || tok == TK_POW) {
        lexer_fetch(l);
        kl_expr *rhs = (tok == TK_POW) ? parse_expr_term(ctx, l) : parse_expr_regmatch(ctx, l);
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

static kl_expr *parse_expr_null_coalescing(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_logical_or(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_NULLC) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_logical_or(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_expr *parse_expr_ternary(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_null_coalescing(ctx, l);
    tk_token tok = l->tok;
    if (tok == TK_QES) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_ternary(ctx, l);  // Right recursion.
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        if (l->tok != TK_COLON) {
            parse_error(ctx, __LINE__, l, "The ':' is missing in ternary expression");
            return panic_mode_expr(lhs, ';', ctx, l);
        }
        lexer_fetch(l);
        lhs->xhs = parse_expr_ternary(ctx, l);      // Right recursion.
    }
    return lhs;
}

static kl_expr *parse_expr_yield(kl_context *ctx, kl_lexer *l, kl_expr *lhs)
{
    lexer_fetch(l);
    kl_expr *rhs = (l->tok == TK_SEMICOLON || l->tok == TK_IF) ? NULL : parse_expr_ternary(ctx, l);
    if (lhs) {
        check_assigned(ctx, l, lhs);
        kl_expr *v = parse_expr_varname(ctx, l, NULL, TK_LET);
        lhs = make_bin_expr(ctx, l, TK_EQ, lhs, v);
    }
    lhs = make_conn_expr(ctx, l, TK_YIELD, lhs, rhs);
    ++(ctx->scope->yield);
    lhs->yield = ctx->scope->yield;
    return lhs;
}

static void set_constant_value(kl_context *ctx, kl_lexer *l, kl_expr *lhs, kl_expr *rhs)
{
    if (lhs->sym && lhs->sym->is_const) {
        kl_symbol *ls = lhs->sym;
        if (rhs->nodetype == TK_VSINT) {
            ls->has_i64 = 1;
            ls->i64 = rhs->val.i64;
        } else if (rhs->nodetype == TK_VAR && rhs->sym) {
            kl_symbol *chk = search_symbol(ctx, l, rhs->sym->name);
            if (chk && chk->has_i64) {
                ls->has_i64 = 1;
                ls->i64 = chk->i64;
            }
        }
    }
}

static kl_expr *parse_case_when_condition(kl_context *ctx, kl_lexer *l)
{
    int in_casewhen = ctx->in_casewhen;
    ctx->in_casewhen = 1;
    kl_expr *lhs = parse_expr_prefix(ctx, l);
    while (l->tok == TK_OR) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_prefix(ctx, l);
        lhs = make_bin_expr(ctx, l, TK_OR, lhs, rhs);
    }
    check_assigned(ctx, l, lhs);
    ctx->in_casewhen = in_casewhen;
    if (l->tok == TK_IF) {
        kl_stmt *m = parse_if_modifier(ctx, l);
        lhs->s = m;
    }
    return lhs;
}

static kl_expr *parse_case_when_expr(kl_context *ctx, kl_lexer *l, kl_expr *cexpr)
{
    kl_expr *lhs = NULL;
    while (l->tok == TK_WHEN || l->tok == TK_OTHERWISE) {
        tk_token tok = l->tok;
        lexer_fetch(l);
        kl_expr *rhs = NULL;
        if (tok == TK_WHEN) {
            rhs = make_expr(ctx, l, TK_WHEN);
            rhs->lhs = parse_case_when_condition(ctx, l);
        } else {
            rhs = make_expr(ctx, l, TK_OTHERWISE);
        }
        if (l->tok == TK_COLON) {
            lexer_fetch(l); /* Skip it. */
        }
        if (l->tok == TK_LXBR) {
            lexer_fetch(l);
            kl_expr *f = parse_block_function(ctx, l, 1);
            rhs->rhs = make_bin_expr(ctx, l, TK_CALL, f, NULL);
        } else {
            rhs->rhs = parse_expression(ctx, l);
        }
        lhs = make_bin_expr(ctx, l, TK_COMMA, lhs, rhs);
    }

    kl_expr *e = make_expr(ctx, l, TK_CASE);
    e->lhs = cexpr;
    e->rhs = lhs;
    return e;
}

static kl_expr *parse_expr_assignment(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    if (l->tok == TK_YIELD) {
        return parse_expr_yield(ctx, l, NULL);
    }
    if (l->tok == TK_CASE) {
        lexer_fetch(l);
        kl_expr *e = parse_expression(ctx, l);
        return parse_case_when_expr(ctx, l, e);
    }
    kl_expr *lhs = parse_expr_ternary(ctx, l);
    tk_token tok = l->tok;
    if (TK_EQ <= tok && tok <= TK_NULLCEQ) {
        lexer_fetch(l);
        if (tok == TK_EQ && l->tok == TK_YIELD) {
            return parse_expr_yield(ctx, l, lhs);
        }
        kl_expr *rhs = parse_expr_assignment(ctx, l);   // Right recursion.
        check_assigned(ctx, l, lhs);
        set_constant_value(ctx, l, lhs, rhs);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
    }
    return lhs;
}

static kl_expr *parse_expression(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_assignment(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_COMMA) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_assignment(ctx, l);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        tok = l->tok;
    }
    return lhs;
}

static kl_stmt *parse_if_modifier(kl_context *ctx, kl_lexer *l)
{
    kl_stmt *s = make_stmt(ctx, l, TK_IF);

    lexer_fetch(l);
    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, l, "The '(' is missing");
        return s;
    }
    lexer_fetch(l);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, l, "The ')' is missing");
        return s;
    }
    lexer_fetch(l);

    return s;
}

static kl_stmt *parse_expression_stmt(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_EXPR);
    s->e1 = parse_expression(ctx, l);
    if (l->tok == TK_IF) {
        kl_stmt *m = parse_if_modifier(ctx, l);
        m->s1 = s;
        s = m;
    }
    if (l->tok == TK_COLON) {
        if (s->e1->nodetype != TK_VAR) {
            parse_error(ctx, __LINE__, l, "Invalid label name");
            return panic_mode_exprstmt(s, ';', ctx, l);
        }
        s->nodetype = TK_LABEL;
        ctx->scope->idxmax--;   // cancelled the index.
        s->sym = s->e1->sym;
        lexer_fetch(l);
        return s;
    }

    if (l->tok != TK_SEMICOLON) {
        parse_error(ctx, __LINE__, l, "The ';' is missing");
        return panic_mode_exprstmt(s, ';', ctx, l);
    }
    lexer_fetch(l);
    return s;
}

static kl_expr *parse_type(kl_context *ctx, kl_lexer *l)
{
    kl_expr *e = NULL;
    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, l, "No type name after ':' in argument");
        return panic_mode_expr(make_expr(ctx, l, TK_TYPENODE), ';', ctx, l);
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
        if (is_type_token(l->tok)) {
            kl_expr *e1 = make_expr(ctx, l, TK_TYPENODE);
            e1->typeid = l->typeid;
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
        if (is_type_token(l->tok)) {
            kl_expr *e1 = make_expr(ctx, l, TK_TYPENODE);
            e1->typeid = l->typeid;
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
    if (is_type_token(l->tok)) {
        e->typeid = sym->typeid = l->typeid;
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
    if (is_type_token(l->tok)) {
        s->typeid = sym->typeid = l->typeid;
        lexer_fetch(l);
    } else {
        sym->typetree = parse_type(ctx, l);
        s->typeid = sym->typeid = sym->typetree->typeid;
    }
    return s;
}

static kl_expr *parse_def_arglist(kl_context *ctx, kl_lexer *l, kl_symbol *func)
{
    DEBUG_PARSER_PHASE();
    int varid = 0;
    kl_expr *e = NULL;
    do {
        int dot3 = 0;
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
        if (l->tok == TK_RSBR) {
            // No more arguments.
            break;
        }
        if (l->tok == TK_DOT3) {
            // This should be used only for the last argument.
            dot3 = 1;
            func->is_dot3 = 1;
            lexer_fetch(l);
        }
        if (l->tok == TK_NAME) {
            check_symbol(ctx, l, l->str);
            kl_expr *e1 = make_expr(ctx, l, TK_VAR);
            kl_symbol *sym = make_symbol(ctx, l, TK_VAR, 0);
            sym->is_dot3 = dot3;
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
            if (l->tok == TK_LXBR || l->tok == TK_LLBR) {
                kl_expr *e1 = make_expr(ctx, l, TK_VAR);
                kl_symbol *sym = make_symbol(ctx, l, TK_VAR, 0);
                sym->name = parse_const_varidname(ctx, l, varid++);
                e1->sym = sym;
                kl_expr *es = parse_lvalue_factor(ctx, l);
                kl_stmt *as = make_stmt(ctx, l, TK_LET);
                as->e1 = make_bin_expr(ctx, l, TK_EQ, es, parse_expr_varname(ctx, l, sym->name, 0));
                e1->s = as;
                e = make_bin_expr(ctx, l, TK_COMMA, e, e1);
            } else {
                parse_error(ctx, __LINE__, l, "Invalid argument");
            }
        }
        if (dot3 && l->tok != TK_RSBR) {
            parse_error(ctx, __LINE__, l, "The 3 dots are being used at not the last argument");
        }
    } while (l->tok == TK_COMMA);
    return e;
}

static kl_stmt *parse_switch(kl_context *ctx, kl_lexer *l)
{
    kl_stmt *s = make_stmt(ctx, l, TK_SWITCH);
    kl_stmt *prev = ctx->switchstmt;
    s->sym = make_symbol(ctx, l, TK_VAR, 0);
    s->sym->name = parse_const_varidname(ctx, l, s->sym->index);
    ctx->switchstmt = s;
    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, l, "Invalid switch value");
        return panic_mode_exprstmt(s, '}', ctx, l);
    }
    lexer_fetch(l);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, l, "The ')' is missing");
        return panic_mode_exprstmt(s, '}', ctx, l);
    }
    lexer_fetch(l);
    if (l->tok != TK_LXBR) {
        parse_error(ctx, __LINE__, l, "Invalid switch statement");
        return panic_mode_exprstmt(s, '}', ctx, l);
    }
    s->s1 = parse_statement(ctx, l);
    ctx->switchstmt = prev;
    return s;
}

static void add_case(kl_stmt *sw, kl_stmt *cs)
{
    cs->ncase = sw->ncase;
    sw->ncase = cs;
}


static kl_stmt *parse_case_when(kl_context *ctx, kl_lexer *l, tk_token tok)
{
    kl_expr *e = parse_expression(ctx, l);
    if (tok == TK_CASE && l->tok == TK_WHEN) {
        /* case-when expression */
        kl_stmt *s = make_stmt(ctx, l, TK_EXPR);
        e = parse_case_when_expr(ctx, l, e);
        s->e1 = e;
        return s;
    }

    kl_stmt *s = make_stmt(ctx, l, tok);
    if (!ctx->switchstmt) {
        parse_error(ctx, __LINE__, l, "No switch for case statement");
        return s;
    }
    add_case(ctx->switchstmt, s);
    s->e1 = e;
    if (l->tok != TK_COLON) {
        parse_error(ctx, __LINE__, l, "Invalid case label");
        return panic_mode_exprstmt(s, ';', ctx, l);
    }
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_default_otherwise(kl_context *ctx, kl_lexer *l, tk_token tok)
{
    kl_stmt *s = make_stmt(ctx, l, tok);
    if (!ctx->switchstmt) {
        parse_error(ctx, __LINE__, l, "No switch for case statement");
        return s;
    }
    add_case(ctx->switchstmt, s);
    ctx->switchstmt->defcase = s;
    if (l->tok != TK_COLON) {
        parse_error(ctx, __LINE__, l, "Invalid default label");
        return panic_mode_exprstmt(s, ';', ctx, l);
    }
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_if(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_IF);

    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, l, "The '(' is missing");
        return s;
    }
    lexer_fetch(l);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, l, "The ')' is missing");
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
        parse_error(ctx, __LINE__, l, "The '(' is missing");
        return s;
    }
    lexer_fetch(l);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, l, "The ')' is missing");
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
        parse_error(ctx, __LINE__, l, "'while' is missing at the end of 'do' block");
        return s;
    }
    lexer_fetch(l);

    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, l, "The '(' is missing");
        return s;
    }
    lexer_fetch(l);
    s->e1 = parse_expression(ctx, l);
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, l, "The ')' is missing");
        return s;
    }
    lexer_fetch(l);
    if (l->tok != TK_SEMICOLON) {
        parse_error(ctx, __LINE__, l, "The ';' is missing");
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
        parse_error(ctx, __LINE__, l, "The '(' is missing");
        return s;
    }

    /* for loop has a block scope because a new variable could be declared in the 1st part. */
    kl_nsstack *n = make_nsstack(ctx, l, "block", TK_BLOCK);
    push_nsstack(ctx, n);
    lexer_fetch(l);
    if (l->tok != TK_SEMICOLON) {
        if (l->tok == TK_LET) {
            lexer_fetch(l);
            s->e1 = parse_decl_expr(ctx, l, TK_LET);
        } else {
            s->e1 = parse_expression(ctx, l);
        }
    }
    if (l->tok == TK_IN) {
        s->nodetype = TK_FORIN; /* replace the tag */
        lexer_fetch(l);
        kl_expr *e2 = parse_expression(ctx, l);
        kl_expr *v1 = parse_expr_varname(ctx, l, "_$forin", TK_LET);
        s->e2 = make_bin_expr(ctx, l, TK_EQ, v1, e2);
        kl_expr *v2 = make_bin_expr(ctx, l, TK_DOT, parse_expr_varname(ctx, l, "_$forin", 0), make_str_expr(ctx, l, "next"));
        check_assigned(ctx, l, s->e1);
        s->e1 = make_bin_expr(ctx, l, TK_EQ, s->e1, make_bin_expr(ctx, l, TK_CALL, v2, NULL));
        s->sym = v1->sym;
    } else {
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
    }

    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, l, "The ')' is missing");
    } else {
        /* then part */
        lexer_fetch(l);
        s->s1 = parse_statement(ctx, l);
    }

    pop_nsstack(ctx);
    return s;
}

static kl_stmt *parse_try(kl_context *ctx, kl_lexer *l)
{
    kl_stmt *s = make_stmt(ctx, l, TK_TRY);

    /* try part */
    s->s1 = parse_statement(ctx, l);

    /* catch part */
    if (l->tok == TK_CATCH) {
        lexer_fetch(l);
        if (l->tok == TK_LSBR) {
            lexer_fetch(l);
            if (l->tok != TK_NAME) {
                parse_error(ctx, __LINE__, l, "The symbol name is missing in catch clause");
            } else {
                s->e1 = parse_expr_varname(ctx, l, l->str, TK_CONST);
                lexer_fetch(l);
            }
            if (l->tok != TK_RSBR) {
                parse_error(ctx, __LINE__, l, "The ')' is missing");
            }
            lexer_fetch(l);
        }
        int in_catch = ctx->in_catch;
        ctx->in_catch = 1;
        s->s2 = parse_statement(ctx, l);
        ctx->in_catch = in_catch;
    }

    /* finally part */
    if (l->tok == TK_FINALLY) {
        lexer_fetch(l);
        int in_finally = ctx->in_finally;
        ctx->in_finally = 1;
        s->s3 = parse_statement(ctx, l);
        ctx->in_finally = in_finally;
    }

    return s;
}

static kl_expr *parse_decl_expr(kl_context *ctx, kl_lexer *l, int decltype)
{
    DEBUG_PARSER_PHASE();
    kl_expr *e1 = NULL;

    int is_const = (decltype == TK_CONST);
    while (l->tok == TK_DOT3 || l->tok == TK_NAME || l->tok == TK_LLBR || l->tok == TK_LXBR) {
        ctx->in_lvalue = decltype;
        kl_expr *lhs = NULL;
        int dot3 = 0;
        if (l->tok == TK_DOT3) {
            dot3 = 1;
            lexer_fetch(l);
        }
        if (l->tok == TK_NAME) {
            check_symbol(ctx, l, l->str);
            lhs = make_expr(ctx, l, TK_VAR);
            kl_symbol *sym = make_symbol(ctx, l, TK_VAR, 0);
            sym->is_dot3 = dot3;
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
            if (lhs->typeid == TK_TANY) {
                lhs->typeid = rhs->typeid;
            }
            check_assigned(ctx, l, lhs);
            if (lhs->sym && lhs->sym->typeid == TK_TANY) {
                lhs->sym->typeid = rhs->typeid;
            }
            set_constant_value(ctx, l, lhs, rhs);
            lhs = make_bin_expr(ctx, l, TK_EQ, lhs, rhs);
        }

        e1 = make_bin_expr(ctx, l, TK_COMMA, e1, lhs);
        if (l->tok != TK_COMMA) {
            break;
        }
        lexer_fetch(l);
        if (l->tok != TK_NAME) {
            parse_error(ctx, __LINE__, l, "The symbol name is needed in declaration");
        }
    }
    return e1;
}

static kl_stmt *parse_enum(kl_context *ctx, kl_lexer *l)
{
    kl_stmt *s = make_stmt(ctx, l, TK_ENUM);
    if (l->tok != TK_LXBR) {
        parse_error(ctx, __LINE__, l, "The '{' is missing");
        return panic_mode_exprstmt(s, '}', ctx, l);
    }
    lexer_fetch(l);

    kl_expr *val = NULL;
    kl_expr *e = NULL;
    for ( ; ; ) {
        if (l->tok != TK_NAME) {
            parse_error(ctx, __LINE__, l, "The constant variable name is needed in enum");
            return panic_mode_exprstmt(s, ';', ctx, l);
        }
        check_symbol(ctx, l, l->str);
        kl_expr *lhs = make_expr(ctx, l, TK_VAR);
        kl_symbol *sym = make_symbol(ctx, l, TK_VAR, 0);
        sym->is_const = 1;
        sym->name = parse_const_str(ctx, l, l->str);
        lhs->sym = sym;

        lexer_fetch(l);
        if (l->tok != TK_EQ) {
            if (!val) {
                val = make_i64_expr(ctx, l, 0);
            } else {
                int64_t v = val->val.i64;
                val = make_i64_expr(ctx, l, v + 1);
            }
            sym->has_i64 = 1;
            sym->i64 = val->val.i64;
            lhs = make_bin_expr(ctx, l, TK_EQ, lhs, val);
        } else {
            lexer_fetch(l);
            kl_expr *rhs = parse_expr_assignment(ctx, l);
            if (rhs->nodetype == TK_VSINT || rhs->nodetype == TK_VBOOL) {
                val = make_i64_expr(ctx, l, rhs->val.i64);
                sym->has_i64 = 1;
                sym->i64 = val->val.i64;
            } else if (rhs->nodetype == TK_VAR) {
                kl_symbol *rs = rhs->sym;
                if (rs) {
                    kl_symbol *chk = search_symbol(ctx, l, rs->name);
                    if (chk && chk->has_i64) {
                        val = make_i64_expr(ctx, l, chk->i64);
                        sym->has_i64 = 1;
                        sym->i64 = val->val.i64;
                    } else {
                        parse_error(ctx, __LINE__, l, "The symbol(%s) was not found in the scope", rs->name);
                    }
                }
            } else {
                parse_error(ctx, __LINE__, l, "The constant value in enum must be the literal integer value");
            }
            lhs = make_bin_expr(ctx, l, TK_EQ, lhs, rhs);
        }
        e = make_bin_expr(ctx, l, TK_COMMA, e, lhs);
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
        if (l->tok != TK_NAME) {
            break;
        }
    }
    s->e1 = e;

    if (l->tok != TK_RXBR) {
        parse_error(ctx, __LINE__, l, "The '}' is missing");
        return s;
    }

    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_declaration(kl_context *ctx, kl_lexer *l, int decltype)
{
    kl_stmt *s = make_stmt(ctx, l, decltype);

    s->e1 = parse_decl_expr(ctx, l, decltype);
    if (l->tok != TK_SEMICOLON) {
        parse_error(ctx, __LINE__, l, "The ';' is missing");
        return panic_mode_exprstmt(s, ';', ctx, l);
    }
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_return_throw(kl_context *ctx, kl_lexer *l, int tok)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, tok);

    if (l->tok != TK_IF && l->tok != TK_SEMICOLON) {
        s->e1 = parse_expression(ctx, l);
    } else if (tok == TK_THROW && !ctx->in_catch) {
        parse_error(ctx, __LINE__, l, "Can not use throw without expression outside catch clause");
    }

    if (l->tok == TK_IF) {
        kl_stmt *m = parse_if_modifier(ctx, l);
        m->s1 = s;
        s = m;
    }
    if (l->tok != TK_SEMICOLON) {
        parse_error(ctx, __LINE__, l, "The ';' is missing");
        return panic_mode_exprstmt(s, ';', ctx, l);
    }
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_mixin(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_MIXIN);
    kl_expr *e = NULL;
    do {
        if (l->tok == TK_COMMA) {
            lexer_fetch(l);
        }
        if (l->tok == TK_SEMICOLON) {
            // No more modules.
            break;
        }
        if (l->tok != TK_NAME) {
            parse_error(ctx, __LINE__, l, "Invalid module name");
            return panic_mode_exprstmt(s, ';', ctx, l);
        }
        kl_expr *e1 = parse_expr_varname(ctx, l, l->str, 0);
        e = make_bin_expr(ctx, l, TK_COMMA, e, e1);
        lexer_fetch(l);
    } while (l->tok == TK_COMMA);
    s->e1 = e;
    return s;
}

static kl_stmt *make_instanceof(kl_context *ctx, kl_lexer *l, const char *cname, int has_base)
{
    kl_stmt *s = make_stmt(ctx, l, TK_FUNC);
    kl_symbol *sym = make_symbol(ctx, l, TK_PUBLIC, 0);
    s->sym = sym;
    sym->funcid = ++ctx->funcid;
    sym->is_callable = 1;

    /* The name is not needed for function */
    sym->name = parse_const_str(ctx, l, "instanceOf");
    sym->funcname = make_func_name(ctx, l, "instanceOf", 0, sym->funcid);
    add_sym2method(ctx->scope, sym);

    /* Push the scope */
    kl_nsstack *n = make_nsstack(ctx, l, sym->name ? sym->name : "anonymous func", TK_FUNC);
    push_nsstack(ctx, n);
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;

    ctx->in_lvalue = TK_LET;
    s->e1 = sym->args = parse_expr_varname(ctx, l, "c", TK_CONST);
    sym->argcount = 1;
    ctx->in_lvalue = 0;
    sym->typeid = TK_TFUNC;

    /* Function body */
    kl_stmt *s1 = make_stmt(ctx, l, TK_IF);
    kl_expr* lhs = make_bin_expr(ctx, l, TK_DOT, parse_expr_varname(ctx, l, "c", 0), make_str_expr(ctx, l, "_id"));
    kl_expr* rhs = make_bin_expr(ctx, l, TK_DOT, parse_expr_varname(ctx, l, cname, 0), make_str_expr(ctx, l, "_id"));
    s1->e1 = make_bin_expr(ctx, l, TK_EQEQ, lhs, rhs);
    s1->s1 = make_stmt(ctx, l, TK_RETURN);
    s1->s1->e1 = make_i64_expr(ctx, l, 1);
    kl_stmt *s2 = make_stmt(ctx, l, TK_RETURN);
    if (has_base) {
        lhs = make_bin_expr(ctx, l, TK_DOT, parse_expr_varname(ctx, l, "super", 0), make_str_expr(ctx, l, "instanceOf"));
        rhs = parse_expr_varname(ctx, l, "c", 0);
        s2->e1 = make_bin_expr(ctx, l, TK_CALL, lhs, rhs);
    } else {
        s2->e1 = make_i64_expr(ctx, l, 0);
    }
    connect_stmt(s1, s2);
    s->s1 = s1;

    sym->prototype = create_prototype(ctx, l, sym);
    ctx->scope = sym->scope;
    pop_nsstack(ctx);

    kl_stmt *addm = add_method2class(ctx, l, sym, TK_PUBLIC);
    connect_stmt(s, addm);
    return s;
}

static kl_expr *parse_class_base_expression(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_expr *lhs = parse_expr_factor(ctx, l);
    tk_token tok = l->tok;
    while (tok == TK_DOT) {
        lexer_fetch(l);
        const char *name = (is_type_token(l->tok)) ? typeidname(l->typeid) : l->str;
        if (!name || name[0] == 0) {
            parse_error(ctx, __LINE__, l, "Property name is needed");
            return panic_mode_expr(lhs, ';', ctx, l);
        }
        kl_expr *rhs = make_expr(ctx, l, TK_VSTR);
        rhs->typeid = TK_TSTR;
        rhs->val.str = parse_const_str(ctx, l, l->str);
        lhs = make_bin_expr(ctx, l, tok, lhs, rhs);
        lexer_fetch(l);
        tok = l->tok;
    }
    if (tok == TK_LSBR) {
        lexer_fetch(l);
        kl_expr *rhs = parse_expr_list(ctx, l, TK_RSBR);
        kl_symbol *sym = lhs->sym;
        tk_typeid tid = sym ? sym->rettype : lhs->typeid;
        lhs = make_bin_expr(ctx, l, TK_CALL, lhs, rhs);
        lhs->typeid = tid;
        if (l->tok != TK_RSBR) {
            parse_error(ctx, __LINE__, l, "The ')' is missing");
            return panic_mode_expr(lhs, ';', ctx, l);
        }
        lexer_fetch(l);
    }
    return lhs;
}

static kl_stmt *add_sym2namespace(kl_context *ctx, kl_lexer *l, kl_symbol *nsym, const char *symname)
{
    if (!nsym) {
        nsym = ctx->nsym;
    }
    if (nsym && nsym->name) {        
        kl_stmt *s = make_stmt(ctx, l, TK_EXPR);
        s->e1 = make_bin_expr(ctx, l, TK_NULLCEQ,
            make_bin_expr(ctx, l, TK_DOT,
                parse_expr_varname(ctx, l, nsym->name, 0),
                make_str_expr(ctx, l, symname)
            ),
            parse_expr_varname(ctx, l, symname, 0)
        );
        return s;
    }
    return NULL;
}

static kl_stmt *parse_class(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_CLASS);
    kl_symbol *sym = make_symbol(ctx, l, TK_CLASS, 0);
    s->sym = sym;
    sym->funcid = ++ctx->funcid;
    sym->is_callable = 1;
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;

    /* The name is needed */
    if (l->tok != TK_NAME) {
        parse_error(ctx, __LINE__, l, "Class name is missing");
        return s;
    }
    sym->name = parse_const_str(ctx, l, l->str);
    sym->funcname = make_func_name(ctx, l, sym->name, TK_CLASS, sym->funcid);

    /* Push the scope */
    kl_nsstack *n = make_nsstack(ctx, l, sym->name, TK_CLASS);
    push_nsstack(ctx, n);
    lexer_fetch(l);

    /* Constructor arguments if exists */
    if (l->tok == TK_LSBR) {
        lexer_fetch(l);
        int lvalue = ctx->in_lvalue;
        ctx->in_lvalue = TK_LET;
        s->e1 = sym->args = parse_def_arglist(ctx, l, sym);
        sym->argcount = sym->idxmax;
        ctx->in_lvalue = lvalue;
        if (l->tok != TK_RSBR) {
            parse_error(ctx, __LINE__, l, "The ')' is missing");
            return s;
        }
        lexer_fetch(l);
    }

    if (l->tok == TK_COLON) {
        lexer_fetch(l);
        if (l->tok != TK_NAME) {
            parse_error(ctx, __LINE__, l, "Base class name is missing");
            return s;
        }
        sym->basename = parse_const_str(ctx, l, l->str);
        sym->base = parse_class_base_expression(ctx, l);
        if (sym->base->nodetype != TK_CALL) {
            sym->base = make_bin_expr(ctx, l, TK_CALL, sym->base, NULL);
        }
        kl_expr *r = make_str_expr(ctx, l, "create");
        sym->base->lhs = make_bin_expr(ctx, l, TK_DOT, sym->base->lhs, r);
    }

    /* Making `this` and `super`. */
    kl_stmt *thisobj = make_stmt(ctx, l, TK_LET);
    kl_expr *thisexpr = parse_expr_varname(ctx, l, "this", TK_CONST);
    thisobj->e1 = thisexpr;
    sym->thisobj = thisexpr->sym;
    kl_stmt *thisid = make_stmt(ctx, l, TK_LET);
    thisid->e1 = make_bin_expr(ctx, l, TK_EQ,
        make_bin_expr(ctx, l, TK_DOT, parse_expr_varname(ctx, l, "this", 0), make_str_expr(ctx, l, "_id")),
        make_i64_expr(ctx, l, sym->funcid)
    );
    thisobj->next = thisid;
    if (sym->base) {
        thisobj->e1 = make_bin_expr(ctx, l, TK_EQ, thisobj->e1, sym->base);
        kl_stmt *superobj = make_stmt(ctx, l, TK_MKSUPER);
        superobj->e1 = parse_expr_varname(ctx, l, "super", TK_CONST);
        superobj->e2 = parse_expr_varname(ctx, l, "this", 0);
        thisid->next = superobj;
    }

    /* Class body */
    if (l->tok != TK_LXBR) {
        parse_error(ctx, __LINE__, l, "The '{' is missing");
        return s;
    }
    lexer_fetch(l);
    s->s1 = sym->body = thisobj;
    if (l->tok != TK_RXBR) {
        kl_stmt *body = parse_statement_list(ctx, l);
        connect_stmt(thisobj, body);
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, l, "The '}' is missing");
            return s;
        }
    }

    /* instanceof */
    kl_stmt *instof = make_instanceof(ctx, l, sym->name, sym->base ? 1 : 0);
    connect_stmt(thisobj, instof);

    /* return this */
    kl_stmt *retthis = make_stmt(ctx, l, TK_RETURN);
    retthis->e1 = parse_expr_varname(ctx, l, "this", 0);
    connect_stmt(instof, retthis);

    ctx->scope = sym->scope;
    pop_nsstack(ctx);
    lexer_fetch(l);

    /* append the class to namespace  */
    kl_stmt *classns = add_sym2namespace(ctx, l, NULL, sym->name);
    if (classns) {
        connect_stmt(s, classns);
    }

    /* set a class id  */
    kl_stmt *classobj = make_stmt(ctx, l, TK_LET);
    classobj->e1 = make_bin_expr(ctx, l, TK_EQ,
        make_bin_expr(ctx, l, TK_DOT, parse_expr_varname(ctx, l, sym->name, 0), make_str_expr(ctx, l, "_id")),
        make_i64_expr(ctx, l, sym->funcid)
    );
    connect_stmt(classobj, s);

    return classobj;
}

static kl_stmt *parse_module(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_MODULE);
    kl_symbol *sym = make_symbol(ctx, l, TK_MODULE, 0);
    s->sym = sym;
    sym->funcid = ++ctx->funcid;
    sym->is_callable = 1;
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;

    /* The name is needed */
    if (l->tok != TK_NAME) {
        parse_error(ctx, __LINE__, l, "Module name is missing");
        return s;
    }
    sym->name = parse_const_str(ctx, l, l->str);
    sym->funcname = make_func_name(ctx, l, sym->name, TK_MODULE, sym->funcid);

    /* Push the scope */
    kl_nsstack *n = make_nsstack(ctx, l, sym->name, TK_CLASS);
    push_nsstack(ctx, n);

    kl_expr *e = make_expr(ctx, l, TK_VAR);
    kl_symbol *thissym = make_symbol(ctx, l, TK_VAR, 0);
    thissym->name = parse_const_str(ctx, l, "this");
    e->sym = thissym;
    s->e1 = sym->args = e;
    sym->argcount = sym->idxmax;

    lexer_fetch(l);

    /* Module body */
    if (l->tok != TK_LXBR) {
        parse_error(ctx, __LINE__, l, "The '{' is missing");
        return s;
    }
    lexer_fetch(l);
    if (l->tok != TK_RXBR) {
        kl_stmt *body = parse_statement_list(ctx, l);
        s->s1 = sym->body = body;
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, l, "The '}' is missing");
            return s;
        }
    }

    ctx->scope = sym->scope;
    pop_nsstack(ctx);
    lexer_fetch(l);

    /* append the module to namespace  */
    kl_stmt *modulens = add_sym2namespace(ctx, l, NULL, sym->name);
    if (modulens) {
        connect_stmt(s, modulens);
    }
    return s;
}

static kl_expr *parse_block_function(kl_context *ctx, kl_lexer *l, int is_block)
{
    kl_expr *e = make_expr(ctx, l, TK_FUNC);
    kl_symbol *sym = make_symbol(ctx, l, TK_FUNC, 1);
    e->sym = sym;
    sym->funcid = ++ctx->funcid;
    sym->is_callable = 1;
    sym->name = parse_const_funcidname(ctx, l, sym->funcid);
    sym->funcname = make_func_name(ctx, l, sym->name, 0, sym->funcid);
    add_sym2method(ctx->scope, sym);

    /* Push the scope */
    kl_nsstack *n = make_nsstack(ctx, l, sym->name, TK_FUNC);
    push_nsstack(ctx, n);
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;
    const char *prevfunc = l->funcname;
    l->funcname = make_class_func_name(ctx, l, sym->name);

    if (l->tok == TK_AND) {
        lexer_fetch(l);
        if (!is_block && l->tok == TK_LXBR) {
            is_block = 1;
            lexer_fetch(l);
        } else {
            /* Function arguments */
            if (l->tok != TK_LSBR) {
                parse_error(ctx, __LINE__, l, "The '(' is missing");
                return panic_mode_expr(e, '{', ctx, l);
            }
            lexer_fetch(l);
            ctx->in_lvalue = TK_LET;
            e->e = sym->args = parse_def_arglist(ctx, l, sym);
            sym->argcount = sym->idxmax;
            ctx->in_lvalue = 0;
            if (l->tok != TK_RSBR) {
                parse_error(ctx, __LINE__, l, "The ')' is missing");
                return e;
            }
            lexer_fetch(l);
        }
    } 

    if (l->tok == TK_DARROW) {
        lexer_fetch(l);
        if (l->tok == TK_LXBR) {
            e->s = parse_statement(ctx, l);
        } else {
            e->s = make_stmt(ctx, l, TK_RETURN);
            e->s->e1 = parse_expr_assignment(ctx, l);
        }
    } else {
        e->s = parse_statement_list(ctx, l);
    }

    if (is_block) {
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, l, "The '}' is missing");
            return e;
        }
        lexer_fetch(l);
    }

    l->funcname = prevfunc;
    sym->prototype = create_prototype(ctx, l, sym);
    ctx->scope = sym->scope;
    pop_nsstack(ctx);
    return e;
}

static kl_expr *parse_anonymous_function(kl_context *ctx, kl_lexer *l)
{
    kl_expr *e = make_expr(ctx, l, TK_FUNC);
    kl_symbol *sym = make_symbol(ctx, l, TK_FUNC, 1);
    e->sym = sym;
    sym->funcid = ++ctx->funcid;
    sym->is_callable = 1;
    sym->name = parse_const_funcidname(ctx, l, sym->funcid);
    sym->funcname = make_func_name(ctx, l, sym->name, 0, sym->funcid);
    add_sym2method(ctx->scope, sym);

    /* Push the scope */
    kl_nsstack *n = make_nsstack(ctx, l, sym->name, TK_FUNC);
    push_nsstack(ctx, n);
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;
    const char *prevfunc = l->funcname;
    l->funcname = make_class_func_name(ctx, l, sym->name);

    /* Function arguments */
    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, l, "The '(' is missing");
        return panic_mode_expr(e, '{', ctx, l);
    }
    lexer_fetch(l);
    int lvalue = ctx->in_lvalue;
    ctx->in_lvalue = TK_LET;
    e->e = sym->args = parse_def_arglist(ctx, l, sym);
    sym->argcount = sym->idxmax;
    ctx->in_lvalue = lvalue;
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, l, "The ')' is missing");
        return e;
    }

    /* Function body */
    lexer_fetch(l);
    if (l->tok != TK_LXBR) {
        parse_error(ctx, __LINE__, l, "The '{' is missing");
        return e;
    }
    lexer_fetch(l);
    if (l->tok != TK_RXBR) {
        e->s = parse_statement_list(ctx, l);
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, l, "The '}' is missing");
            return e;
        }
    }

    l->funcname = prevfunc;
    sym->prototype = create_prototype(ctx, l, sym);
    ctx->scope = sym->scope;
    pop_nsstack(ctx);
    lexer_fetch(l);
    return e;
}

static kl_stmt *parse_function(kl_context *ctx, kl_lexer *l, tk_token funcscope)
{
    DEBUG_PARSER_PHASE();

    kl_stmt *s = make_stmt(ctx, l, TK_FUNC);
    const char *name = NULL;
    int funcid = ++ctx->funcid;

    /* The name is not needed for function */
    if (l->tok == TK_NAME) {
        name = parse_const_str(ctx, l, l->str);
        lexer_fetch(l);
    } else if (funcscope == TK_PUBLIC) {
        name = parse_special_funcname(ctx, l);
        lexer_fetch(l);
    } else {
        name = parse_const_funcidname(ctx, l, funcid);
    }
    if (!name) {
        parse_error(ctx, __LINE__, l, "Invalid function name");
        return s;
    }
    kl_symbol *sym = make_ref_symbol(ctx, l, funcscope, name);
    if (!sym) {
        sym = make_symbol(ctx, l, funcscope, 0);
    }
    if (ctx->scope->symtoken == TK_CLASS && strcmp(name, "initialize") == 0) {
        ctx->scope->initer = sym;
    }
    s->sym = sym;
    sym->funcid = funcid;
    sym->name = name;
    sym->funcname = make_func_name(ctx, l, name, 0, funcid);
    sym->is_callable = 1;
    sym->is_native = (funcscope == TK_NATIVE);
    add_sym2method(ctx->scope, sym);

    /* Push the scope */
    kl_nsstack *n = make_nsstack(ctx, l, sym->name ? sym->name : "anonymous func", TK_FUNC);
    push_nsstack(ctx, n);
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;
    const char *prevfunc = l->funcname;
    l->funcname = make_class_func_name(ctx, l, sym->name);

    /* Function arguments */
    if (l->tok != TK_LSBR) {
        parse_error(ctx, __LINE__, l, "The '(' is missing");
        return panic_mode_stmt(s, '{', ctx, l);
    }
    lexer_fetch(l);
    ctx->in_lvalue = TK_LET;
    s->e1 = sym->args = parse_def_arglist(ctx, l, sym);
    sym->argcount = sym->idxmax;
    ctx->in_lvalue = 0;
    if (l->tok != TK_RSBR) {
        parse_error(ctx, __LINE__, l, "The ')' is missing");
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
        parse_error(ctx, __LINE__, l, "The '{' is missing");
        return s;
    }
    lexer_fetch(l);
    if (l->tok != TK_RXBR) {
        s->s1 = sym->body = parse_statement_list(ctx, l);
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, l, "The '}' is missing");
            return s;
        }
    }

    l->funcname = prevfunc;
    sym->prototype = create_prototype(ctx, l, sym);
    ctx->scope = sym->scope;
    pop_nsstack(ctx);

    kl_stmt *addm = add_method2class(ctx, l, sym, funcscope);
    connect_stmt(s, addm);
    lexer_fetch(l);
    return s;
}

static kl_stmt *parse_namespace(kl_context *ctx, kl_lexer *l)
{
    DEBUG_PARSER_PHASE();
    kl_stmt *s = make_stmt(ctx, l, TK_NAMESPACE);

    /* No name is okay for namespace */
    const char *nsname;
    if (l->tok == TK_NAME) {
        nsname = parse_const_str(ctx, l, l->str);
        lexer_fetch(l);
    } else {
        nsname = parse_const_str(ctx, l, "_anonymousns");
    }

    /* Check if the namespace was declared before. */
    int ns_already = 1;
    kl_symbol *sym = make_ref_symbol(ctx, l, TK_VAR, nsname);
    if (!sym) {
        ns_already = 0;
        sym = make_symbol(ctx, l, TK_NAMESPACE, 0);
    }
    s->sym = sym;
    sym->funcid = ++ctx->funcid;
    sym->name = nsname;
    sym->funcname = make_func_name(ctx, l, sym->name, 0, sym->funcid);

    kl_symbol *nsym = ctx->nsym;
    ctx->nsym = make_ref_symbol(ctx, l, TK_VAR, sym->name);
    /* ctx->nsym must not be NULL. */

    /* Scope in */
    kl_nsstack *n = make_nsstack(ctx, l, sym->name, TK_NAMESPACE);
    push_nsstack(ctx, n);
    ctx->scope->has_func = 1;
    sym->scope = ctx->scope;
    ctx->scope = sym;

    /* Namespace statement */
    if (l->tok != TK_LXBR) {
        parse_error(ctx, __LINE__, l, "The '{' is missing");
        return s;
    }
    lexer_fetch(l);
    kl_stmt *namespcns = (ns_already || !nsym) ? NULL : add_sym2namespace(ctx, l, nsym, sym->name);
    kl_stmt *ns;
    if (l->tok != TK_RXBR) {
        kl_stmt *st = parse_statement_list(ctx, l);
        if (namespcns) {
            connect_stmt(namespcns, st);
            st = namespcns;
        }
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, l, "The '}' is missing");
            return s;
        }
        if (ns_already) {
            ns = st;
        } else {
            ns = make_stmt(ctx, l, TK_EXPR);
            ns->e1 = make_bin_expr(ctx, l, TK_NULLCEQ, parse_expr_varname(ctx, l, ctx->nsym->name, 0), make_expr(ctx, l, TK_VOBJ));
            connect_stmt(ns, st);
        }
    } else {
        if (ns_already) {
            ns = namespcns;
        } else {
            ns = make_stmt(ctx, l, TK_EXPR);
            ns->e1 = make_bin_expr(ctx, l, TK_NULLCEQ, parse_expr_varname(ctx, l, ctx->nsym->name, 0), make_expr(ctx, l, TK_VOBJ));
            if (namespcns) {
                connect_stmt(ns, namespcns);
            }
        }
    }

    /* namespace statements */
    sym->body = s->s1 = ns;

    ctx->scope = sym->scope;
    pop_nsstack(ctx);
    lexer_fetch(l);
    ctx->nsym = nsym;

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
            parse_error(ctx, __LINE__, l, "The '}' is missing");
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
    case TK_SEMICOLON:
        lexer_fetch(l);
        r = make_stmt(ctx, l, TK_EXPR);
        break;
    case TK_LXBR:
        lexer_fetch(l);
        r = parse_block(ctx, l);
        if (l->tok != TK_RXBR) {
            parse_error(ctx, __LINE__, l, "The '}' is missing");
        }
        lexer_fetch(l);
        break;
    case TK_NAMESPACE: {
        lexer_fetch(l);
        r = parse_namespace(ctx, l);
        kl_expr *e = make_expr(ctx, l, TK_FUNC);
        kl_symbol *nsym = e->sym = r->sym;
        e->s = r;
        e = make_bin_expr(ctx, l, TK_CALL, e, NULL);
        r = make_stmt(ctx, l, TK_EXPR);
        r->e1 = e;
        break;
    }
    case TK_PRIVATE:
    case TK_PROTECTED:
    case TK_PUBLIC:
    case TK_FUNC:
    case TK_NATIVE:
        lexer_fetch(l);
        r = parse_function(ctx, l, tok);
        break;
    case TK_MIXIN:
        lexer_fetch(l);
        r = parse_mixin(ctx, l);
        break;
    case TK_CLASS:
        lexer_fetch(l);
        r = parse_class(ctx, l);
        break;
    case TK_MODULE:
        lexer_fetch(l);
        r = parse_module(ctx, l);
        break;
    case TK_SWITCH:
        lexer_fetch(l);
        r = parse_switch(ctx, l);
        break;
    case TK_CASE:
        lexer_fetch(l);
        r = parse_case_when(ctx, l, TK_CASE);
        break;
    case TK_WHEN:
        lexer_fetch(l);
        r = parse_case_when(ctx, l, TK_WHEN);
        break;
    case TK_DEFAULT:
        lexer_fetch(l);
        r = parse_default_otherwise(ctx, l, TK_DEFAULT);
        break;
    case TK_OTHERWISE:
        lexer_fetch(l);
        r = parse_default_otherwise(ctx, l, TK_OTHERWISE);
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
    case TK_TRY:
        lexer_fetch(l);
        r = parse_try(ctx, l);
        break;
    case TK_FALLTHROUGH:
        r = make_stmt(ctx, l, TK_FALLTHROUGH);
        lexer_fetch(l);
        if (l->tok != TK_SEMICOLON) {
            parse_error(ctx, __LINE__, l, "The ';' is missing");
            return panic_mode_stmt(r, ';', ctx, l);
        }
        lexer_fetch(l);
        break;
    case TK_CONTINUE:
    case TK_BREAK:
        r = make_stmt(ctx, l, tok);
        lexer_fetch(l);
        if (l->tok == TK_NAME) {
            kl_symbol *sym = make_ref_symbol(ctx, l, TK_VAR, l->str);
            if (!sym) {
                parse_error(ctx, __LINE__, l, "Label(%s) not found", l->str);
                return panic_mode_stmt(r, ';', ctx, l);
            }
            r->sym = sym->ref ? sym->ref : sym;
            lexer_fetch(l);
        }
        if (l->tok != TK_SEMICOLON) {
            parse_error(ctx, __LINE__, l, "The ';' is missing");
            return panic_mode_stmt(r, ';', ctx, l);
        }
        lexer_fetch(l);
        break;
    case TK_RETURN:
    case TK_THROW:
        lexer_fetch(l);
        r = parse_return_throw(ctx, l, tok);
        break;
    case TK_EXTERN:
        lexer_fetch(l);
        if (l->tok != TK_NAME) {
            parse_error(ctx, __LINE__, l, "Function/Object name is needed after 'extern'");
            return panic_mode_stmt(r, ';', ctx, l);
        }
        r = make_stmt(ctx, l, TK_EXTERN);
        r->e1 = make_str_expr(ctx, l, l->str);
        r->e2 = parse_expr_varname(ctx, l, l->str, TK_CONST);
        lexer_fetch(l);
        if (l->tok == TK_LSBR) {
            lexer_fetch(l);
            if (l->tok != TK_RSBR) {
                parse_error(ctx, __LINE__, l, "The ')' is missing in key value");
                return panic_mode_stmt(r, ';', ctx, l);
            }
            lexer_fetch(l);
            r->typeid = TK_TFUNC;
        } else {
            r->typeid = TK_TANY;
        }
        if (l->tok != TK_SEMICOLON) {
            parse_error(ctx, __LINE__, l, "The ';' is missing");
            return panic_mode_stmt(r, ';', ctx, l);
        }
        lexer_fetch(l);
        break;
    case TK_ENUM:
        lexer_fetch(l);
        r = parse_enum(ctx, l);
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
        if (s2 == NULL) {
            break;
        }
        if (!s1) {
            head = s2;
        }
        if (s1 && s1->nodetype == TK_LABEL && !s1->s1) {
            if (!s2) {
                parse_error(ctx, __LINE__, l, "No label block");
            } else {
                s1->s1 = s2;
                s1->s1->sym = s1->sym;
            }
        } else {
            s1 = connect_stmt(s1, s2);
        }
        while (l->tok == TK_SEMICOLON) {
            /* Skip semicolons because it means no statement. */
            lexer_fetch(l);
        }
        if (l->tok == TK_RXBR) {
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
    kl_symbol *sym = make_symbol(ctx, l, TK_NAMESPACE, 0);
    sym->is_global = 1;
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
