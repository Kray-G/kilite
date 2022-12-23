#include "../kir.h"
#include "mkkir.h"

#define KIR_ADD_NEXT(last, next) if (next) (last->next = next, last = next)
#define KIR_MOVE_LAST(last) while (last->next) last = last->next

static kl_kir_inst *gen_block(kl_context *ctx, kl_symbol *sym, kl_stmt *s);
static kl_kir_inst *gen_range(kl_context *ctx, kl_symbol *sym, kl_expr *e, kl_kir_opr *r1, int range);
static kl_kir_inst *gen_assign_object(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e);
static kl_kir_inst *gen_expr(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e);
static kl_kir_inst *gen_stmt(kl_context *ctx, kl_symbol *sym, kl_stmt *s);
static kl_kir_func *gen_function(kl_context *ctx, kl_symbol *sym, kl_stmt *s, kl_symbol *initer);
static kl_kir_func *gen_namespace(kl_context *ctx, kl_symbol *sym, kl_stmt *s);

static int mkkir_error(kl_context *ctx, int sline, kl_symbol *sym, const char *fmt, ...)
{
    // fprintf(stderr, "%d: ", sline);
    int line = sym->line;
    int pos = sym->pos;
    int len = 0;
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

static char *mkkir_const_str(kl_context *ctx, const char *str)
{
    return const_str(ctx, str);
}

static inline int get_next_label(kl_context *ctx)
{
    return ctx->labelid++;
}

static const char *translate_vartype(tk_typeid tp)
{
    switch (tp) {
    case TK_TBOOL:
        return "VAR_BOOL";
    case TK_TSINT64:
        return "VAR_INT64";
    case TK_TBIGINT:
        return "VAR_BIG";
    case TK_TDBL:
        return "VAR_DBL";
    case TK_TSTR:
        return "VAR_STR";
    case TK_TBIN:
        return "VAR_BIN";
    case TK_TOBJ:
        return "VAR_OBJ";
    case TK_TFUNC:
        return "VAR_FNC";
    }
    return NULL;
}

static kl_kir_inst *new_inst(kl_kir_program *p, int line, int pos, kl_kir op)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = op;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_nop(kl_kir_program *p, int line, int pos)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = KIR_NOP;
    inst->line = line;
    inst->pos = pos;
    inst->disabled = 1;
    return inst;
}

static kl_kir_inst *new_inst_label(kl_kir_program *p, int line, int pos, int labelid, kl_kir_inst *last, int gcable)
{
    if (last &&
            ((last->opcode == KIR_JMP || last->opcode == KIR_JMPIFT || last->opcode == KIR_JMPIFF) &&
            last->labelid == labelid)) {
        last->disabled = 1;
    }
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = KIR_LABEL;
    inst->labelid = labelid;
    inst->gcable = gcable;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_jumpifne(kl_kir_program *p, int line, int pos, kl_kir_opr *r1, int labelid)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = KIR_JMPIFNE;
    inst->r1 = *r1;
    inst->labelid = labelid;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_jumpift(kl_kir_program *p, int line, int pos, kl_kir_opr *r1, int labelid)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = KIR_JMPIFT;
    inst->r1 = *r1;
    inst->labelid = labelid;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_jumpiff(kl_kir_program *p, int line, int pos, kl_kir_opr *r1, int labelid)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = KIR_JMPIFF;
    inst->r1 = *r1;
    inst->labelid = labelid;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_jump(kl_kir_program *p, int line, int pos, int labelid, kl_kir_inst *last)
{
    if (last && (last->opcode == KIR_RET || last->opcode == KIR_JMP)) {
        return NULL;
    }
    if (last && ((last->opcode == KIR_JMPIFT || last->opcode == KIR_JMPIFF) && last->labelid == labelid)) {
        last->disabled = 1;
    }
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = KIR_JMP;
    inst->labelid = labelid;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_chkexcept(kl_kir_program *p, int line, int pos, int labelid)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = KIR_CHKEXCEPT;
    inst->labelid = labelid;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_ret(kl_kir_program *p, int line, int pos)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = KIR_RET;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_op1(kl_kir_program *p, int line, int pos, kl_kir op, kl_kir_opr *r1)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = op;
    inst->r1 = *r1;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_op2(kl_kir_program *p, int line, int pos, kl_kir op, kl_kir_opr *r1, kl_kir_opr *r2)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = op;
    inst->r1 = *r1;
    inst->r2 = *r2;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_inst *new_inst_op3(kl_kir_program *p, int line, int pos, kl_kir op, kl_kir_opr *r1, kl_kir_opr *r2, kl_kir_opr *r3)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->chn = p->ichn;
    p->ichn = inst;
    inst->opcode = op;
    inst->r1 = *r1;
    inst->r2 = *r2;
    inst->r3 = *r3;
    inst->line = line;
    inst->pos = pos;
    return inst;
}

static kl_kir_func *new_func(kl_context *ctx, int line, int pos, const char *name)
{
    kl_kir_func *func = (kl_kir_func *)calloc(1, sizeof(kl_kir_func));
    func->chn = ctx->program->fchn;
    ctx->program->fchn = func;
    func->name = mkkir_const_str(ctx, name);
    func->line = line;
    func->pos = pos;
    return func;
}

static void add_func(kl_kir_program *prog, kl_kir_func *func)
{
    if (!prog->last) {
        prog->head = func;
    } else {
        prog->last->next = func;
    }
    prog->last = func;
}

#define KL_KIR_CHECK_LITERAL(e, rn, rni) \
    switch ((e)->nodetype) { \
    case TK_VSINT: \
        (rn) = make_lit_i64(ctx, (e)->val.i64); \
        break; \
    case TK_VBIGINT: \
        (rn) = make_lit_big(ctx, (e)->val.big); \
        break; \
    case TK_VDBL: \
        (rn) = make_lit_dbl(ctx, (e)->val.dbl); \
        break; \
    case TK_VSTR: \
        (rn) = make_lit_str(ctx, (e)->val.str); \
        break; \
    case TK_VAR: \
        check_autoset_flag(ctx, (e)->sym); \
        (rn) = make_var_index(ctx, (e)->sym->ref ? (e)->sym->ref->index : (e)->sym->index, (e)->sym->level, (e)->typeid); \
        if ((e)->sym->is_dot3) { \
            (rn).has_dot3 = 1; \
        } \
        break; \
    case TK_DOT3: \
        if ((rn).index == 0) (rn) = make_var(ctx, sym, TK_TANY); \
        (rn).has_dot3 = 1; \
        (rni) = gen_expr(ctx, sym, &(rn), e->lhs); \
        break; \
    case TK_FUNC: \
        if ((e)->s && (e)->s->nodetype == TK_NAMESPACE) { \
            if ((rn).index == 0) (rn) = make_var(ctx, sym, e->typeid); \
            kl_symbol *f = (e)->s->sym; \
            (rni) = gen_stmt(ctx, sym, (e)->s); \
            kl_kir_opr rfnc = make_lit_func(ctx, f); \
            if (!rni) { \
                rni = new_inst_op2(ctx->program, (e)->line, (e)->pos, KIR_MOV, &(rn), &(rfnc)); \
            } else { \
                kl_kir_inst *last = get_last(rni); \
                last->next = new_inst_op2(ctx->program, (e)->line, (e)->pos, KIR_MOV, &(rn), &(rfnc)); \
            } \
        } else { \
            kl_symbol *f = e->sym; \
            kl_kir_func *func = gen_function(ctx, f, (e)->s, NULL); \
            add_func(ctx->program, func); \
            kl_kir_opr rfnc = make_lit_func(ctx, (e)->sym); \
            if ((rn).index == 0) (rn) = make_var(ctx, sym, TK_TANY); \
            (rni) = new_inst_op2(ctx->program, (e)->line, (e)->pos, KIR_MOV, &(rn), &(rfnc)); \
        } \
        break; \
    default: \
        if ((rn).index == 0) (rn) = make_var(ctx, sym, e->typeid); \
        (rni) = gen_expr(ctx, sym, &(rn), e); \
        break; \
    } \
/**/

#define KL_KIR_CHECK_LVALUE(e, rn, rni) \
    switch ((e)->nodetype) { \
    case TK_VAR: \
        (rn) = make_var_index(ctx, (e)->sym->ref ? (e)->sym->ref->index : (e)->sym->index, (e)->sym->level, (e)->typeid); \
        if ((e)->sym->is_dot3) { \
            (rn).has_dot3 = 1;\
        } \
        break; \
    case TK_EQ: \
        if ((e)->lhs->nodetype == TK_VAR) { \
            kl_expr *lhs = (e)->lhs; \
            (rn) = make_var_index(ctx, lhs->sym->ref ? lhs->sym->ref->index : lhs->sym->index, lhs->sym->level, lhs->typeid); \
            (rni) = gen_expr(ctx, sym, &(rn), e); \
            break; \
        } \
        /* fallthrough */ \
    default: \
        if ((rn).index == 0) (rn) = make_var(ctx, sym, e->typeid); \
        (rni) = gen_expr(ctx, sym, &(rn), e); \
        break; \
    } \
/**/

static void check_autoset_flag(kl_context *ctx, kl_symbol *sym)
{
    if (!sym->ref && sym->is_autoset) {
        mkkir_error(ctx, __LINE__, sym, "Symbol(%s) is not found", sym->name);
    }
    if (sym->ref && sym->ref->is_autoset) {
        mkkir_error(ctx, __LINE__, sym, "Symbol(%s) is not found", sym->name);
    }
}

static kl_kir_inst *get_last(kl_kir_inst *i)
{
    if (i) {
        while (i->next) {
            i = i->next;
        }
    }
    return i;
}

static kl_kir_inst *make_head(kl_kir_inst *head, kl_kir_inst *next)
{
    if (!head) {
        return next;
    }
    kl_kir_inst *last = get_last(head);
    last->next = next;
    return head;
}

static void set_file_func(kl_context *ctx, kl_symbol *sym, kl_kir_inst *i)
{
    i->funcname = strcmp(sym->name, "run_global") == 0 ? "<main-block>" : sym->name;
    i->filename = ctx->filename;
    i->catchid = ctx->tclabel;
    if (ctx->options & PARSER_OPT_MAKELIB) {
        i->line = 0;    // reset the line info when it's a makelib mode.
    }
}

static kl_kir_opr make_var(kl_context *ctx, kl_symbol *sym, tk_typeid tid)
{
    kl_kir_opr r1 = {
        .t = TK_VAR,
        .index = sym->idxmax++,   /* TODO: index could be reusable */
        .level = 0,
        .typeid = tid,
    };
    return r1;
}

static kl_kir_opr make_ret_var(kl_context *ctx, kl_symbol *sym)
{
    if (ctx->in_finally) {
        return make_var(ctx, sym, TK_TANY);
    }
    kl_kir_opr r1 = {
        .t = TK_VAR,
        .index = -1,
        .typeid = sym->typeid,
    };
    return r1;
}

static kl_kir_opr make_lit_undef(kl_context *ctx)
{
    kl_kir_opr r1 = {
        .t = TK_UNKNOWN,
    };
    return r1;
}

static kl_kir_opr make_lit_bool(kl_context *ctx, int64_t i64)
{
    kl_kir_opr r1 = {
        .t = TK_VBOOL,
        .i64 = i64,
        .typeid = TK_TBOOL,
    };
    return r1;
}

static kl_kir_opr make_lit_i64(kl_context *ctx, int64_t i64)
{
    kl_kir_opr r1 = {
        .t = TK_VSINT,
        .i64 = i64,
        .typeid = TK_TSINT64,
    };
    return r1;
}

static kl_kir_opr make_lit_big(kl_context *ctx, const char *big)
{
    kl_kir_opr r1 = {
        .t = TK_VBIGINT,
        .str = big,
        .typeid = TK_TBIGINT,
    };
    return r1;
}

static kl_kir_opr make_lit_dbl(kl_context *ctx, const char *dbl)
{
    kl_kir_opr r1 = {
        .t = TK_VDBL,
        .dbl = dbl,
        .typeid = TK_TSINT64,
    };
    return r1;
}

static kl_kir_opr make_lit_str(kl_context *ctx, const char *str)
{
    kl_kir_opr r1 = {
        .t = TK_VSTR,
        .str = str,
        .typeid = TK_TSTR,
    };
    return r1;
}

static kl_kir_opr make_lit_func(kl_context *ctx, kl_symbol *sym)
{
    kl_kir_opr r1 = {
        .t = TK_FUNC,
        .funcid = sym->funcid,
        .typeid = TK_TFUNC,
        .str = mkkir_const_str(ctx, sym->name),
        .name = mkkir_const_str(ctx, sym->funcname ? sym->funcname : sym->name),
    };
    return r1;
}

static kl_kir_opr make_var_index(kl_context *ctx, int index, int level, tk_typeid id)
{
    kl_kir_opr r1 = {
        .t = TK_VAR,
        .index = index,
        .level = level,
        .typeid = id,
    };
    return r1;
}

static kl_kir_inst *gen_binary_array_literal(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_inst *head = NULL;
    if (!e) {
        kl_kir_inst *inst = new_inst_op1(ctx->program, e->line, e->pos, KIR_PUSHN, r1);
        set_file_func(ctx, sym, inst);
        return inst;
    }

    switch (e->nodetype) {
    case TK_COMMA: {
        head = gen_binary_array_literal(ctx, sym, r1, e->lhs);
        kl_kir_inst *last = get_last(head);
        if (last) {
            last->next = gen_binary_array_literal(ctx, sym, r1, e->rhs);
        } else {
            head = gen_binary_array_literal(ctx, sym, r1, e->rhs);
        }
        break;
    }
    default: {
        kl_kir_opr rs = {0};
        KL_KIR_CHECK_LITERAL(e, rs, head);
        kl_kir_inst *inst = new_inst_op2(ctx->program, e->line, e->pos, rs.has_dot3 ? KIR_PUSHX : KIR_PUSH, r1, &rs);
        set_file_func(ctx, sym, inst);
        if (!head) {
            head = inst;
        } else {
            kl_kir_inst *last = get_last(head);
            last->next = inst;
        }
        break;
    }}
    return head;
}

static kl_kir_inst *gen_object_literal(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_kir_opr *r2, kl_expr *e)
{
    kl_kir_inst *head = NULL;
    switch (e->nodetype) {
    case TK_VAR: {
        /* e->has_dot3 should be true. */
        check_autoset_flag(ctx, e->sym);
        kl_kir_opr rs = make_var_index(ctx, e->sym->ref ? e->sym->ref->index : e->sym->index, e->sym->level, e->typeid);
        kl_kir_inst *inst = new_inst_op2(ctx->program, e->line, e->pos, KIR_EXPAND, r1, &rs);
        set_file_func(ctx, sym, inst);
        if (!head) {
            head = inst;
        } else {
            kl_kir_inst *last = get_last(head);
            last->next = inst;
        }
        break;
    }
    case TK_DOT3: {
        kl_kir_opr rs = {0};
        KL_KIR_CHECK_LITERAL(e->lhs, rs, head);
        kl_kir_inst *inst = new_inst_op2(ctx->program, e->line, e->pos, KIR_EXPAND, r1, &rs);
        set_file_func(ctx, sym, inst);
        if (!head) {
            head = inst;
        } else {
            kl_kir_inst *last = get_last(head);
            last->next = inst;
        }
        break;
    }
    case TK_VKV: {
        /* e->lhs should be a string. */
        kl_kir_opr rs = {0};
        KL_KIR_CHECK_LITERAL(e->rhs, rs, head);
        kl_kir_opr r3 = make_lit_str(ctx, e->lhs->val.str);
        kl_kir_inst *inst = new_inst_op3(ctx->program, e->line, e->pos, KIR_APLYL, r2, r1, &r3);
        if (!head) {
            head = inst;
        } else {
            kl_kir_inst *last = get_last(head);
            last->next = inst;
        }
        inst->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOVA, r2, &rs);
        set_file_func(ctx, sym, inst->next);
        break;
    }
    case TK_COMMA: {
        head = gen_object_literal(ctx, sym, r1, r2, e->lhs);
        kl_kir_inst *last = get_last(head);
        last->next = gen_object_literal(ctx, sym, r1, r2, e->rhs);
        break;
    }
    default:
        break;
    }
    return head;
}

static kl_kir_inst *gen_incdec(kl_context *ctx, kl_symbol *sym, kl_kir op, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_opr rr = {0};
    if (!r1) {
        rr = make_ret_var(ctx, sym);
        r1 = &rr;
    }
    kl_kir_inst *r2i = NULL;
    kl_kir_opr r2 = {0};
    kl_expr *l = e->lhs;

    int lvalue = ctx->in_lvalue;
    ctx->in_lvalue = 1;
    KL_KIR_CHECK_LITERAL(l, r2, r2i);
    ctx->in_lvalue = lvalue;
    if (!r2i && r2.t != TK_VSINT && r2.t != TK_VAR) {
        kl_kir_opr r2x = make_var(ctx, sym, l->typeid);
        r2i = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, &r2x, &r2);
        r2 = r2x;
    }
    kl_kir_inst *r2l = get_last(r2i);

    kl_kir_inst *inst = new_inst_op2(ctx->program, e->line, e->pos, op, r1, &r2);
    set_file_func(ctx, sym, inst);
    if (r2i) {
        r2l->next = inst;
        inst = r2i;
    }

    return inst;
}

static kl_kir_inst *gen_logical_and(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e, int l1)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *r1l = NULL;
    if (e->lhs->nodetype == TK_LAND) {
        head = gen_logical_and(ctx, sym, r1, e->lhs, l1);
    } else {
        head = gen_expr(ctx, sym, r1, e->lhs);
    }
    r1l = get_last(head);

    r1l->next = new_inst_jumpiff(ctx->program, e->line, e->pos, r1, l1);
    r1l = r1l->next;

    r1l->next = gen_expr(ctx, sym, r1, e->rhs);
    return head;
}

static kl_kir_inst *gen_logical_or(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e, int l1)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *r1l = NULL;
    if (e->lhs->nodetype == TK_LOR) {
        head = gen_logical_or(ctx, sym, r1, e->lhs, l1);
    } else {
        head = gen_expr(ctx, sym, r1, e->lhs);
    }
    r1l = get_last(head);

    r1l->next = new_inst_jumpift(ctx->program, e->line, e->pos, r1, l1);
    r1l = r1l->next;

    r1l->next = gen_expr(ctx, sym, r1, e->rhs);
    return head;
}

static kl_kir_inst *gen_null_coalescing(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_kir_opr *rc, kl_expr *e, int l1)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *r1l = NULL;
    if (e->lhs->nodetype == TK_NULLC) {
        head = gen_null_coalescing(ctx, sym, r1, rc, e->lhs, l1);
    } else {
        head = gen_expr(ctx, sym, r1, e->lhs);
    }
    r1l = get_last(head);

    kl_kir_opr r3 = make_lit_i64(ctx, VAR_DEF);
    r1l->next = new_inst_op3(ctx->program, e->line, e->pos, KIR_TYPE, rc, r1, &r3);
    r1l = r1l->next;
    r1l->next = new_inst_jumpift(ctx->program, e->line, e->pos, rc, l1);
    r1l = r1l->next;

    r1l->next = gen_expr(ctx, sym, r1, e->rhs);
    return head;
}

static kl_kir_inst *gen_apply(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_expr *l = e->lhs;
    kl_expr *r = e->rhs;

    kl_kir_inst *r2i = NULL;
    kl_kir_opr r2 = {0};
    KL_KIR_CHECK_LVALUE(l, r2, r2i);
    kl_kir_inst *r3i = NULL;
    kl_kir_opr r3 = {0};
    int lvalue = ctx->in_lvalue;
    ctx->in_lvalue = 0;
    KL_KIR_CHECK_LITERAL(r, r3, r3i);
    ctx->in_lvalue = lvalue;

    if (r2.t != TK_VAR) {
        mkkir_error(ctx, __LINE__, e->sym ? e->sym : sym, "Can't apply the index");
    }

    kl_kir_inst *inst = new_inst_op3(ctx->program, e->line, e->pos, ctx->in_lvalue ? KIR_IDXL : KIR_IDX, r1, &r2, &r3);
    if (r2i) {
        kl_kir_inst *r2l = get_last(r2i);
        r2l->next = inst;
        inst = r2i;
    }
    if (r3i) {
        kl_kir_inst *r3l = get_last(r3i);
        r3l->next = inst;
        inst = r3i;
    }

    return inst;
}

static kl_kir_inst *gen_check_type(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *l, kl_expr *r)
{
    const char *str = r->val.str;
    if (!str) {
        return NULL;
    }

    kl_kir_inst *inst = NULL;
    kl_kir_inst *r2i = NULL;
    kl_kir_opr r2 = {0};
    if (ctx->in_lvalue) {
        KL_KIR_CHECK_LVALUE(l, r2, r2i);
    } else {
        KL_KIR_CHECK_LITERAL(l, r2, r2i);
    }

    if (strcmp(str, "isUndefined") == 0) {
        kl_kir_opr r3 = make_lit_str(ctx, "VAR_UNDEF");
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    } else if (strcmp(str, "isDefined") == 0) {
        kl_kir_opr r3 = make_lit_i64(ctx, VAR_DEF);
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    } else if (strcmp(str, "isInteger") == 0) {
        kl_kir_opr r3 = make_lit_i64(ctx, VAR_INT64);
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    } else if (strcmp(str, "isBigInteger") == 0) {
        kl_kir_opr r3 = make_lit_str(ctx, "VAR_BIG");
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    } else if (strcmp(str, "isDouble") == 0 || strcmp(str, "isReal") == 0) {
        kl_kir_opr r3 = make_lit_str(ctx, "VAR_DBL");
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    } else if (strcmp(str, "isString") == 0) {
        kl_kir_opr r3 = make_lit_str(ctx, "VAR_STR");
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    } else if (strcmp(str, "isBinary") == 0) {
        kl_kir_opr r3 = make_lit_str(ctx, "VAR_BIN");
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    } else if (strcmp(str, "isFunction") == 0) {
        kl_kir_opr r3 = make_lit_str(ctx, "VAR_FNC");
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    } else if (strcmp(str, "isObject") == 0) {
        kl_kir_opr r3 = make_lit_str(ctx, "VAR_OBJ");
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    } else if (strcmp(str, "isArray") == 0) {
        kl_kir_opr r3 = make_lit_i64(ctx, VAR_ARY);
        inst = new_inst_op3(ctx->program, r->line, r->pos, KIR_TYPE, r1, &r2, &r3);
    }

    if (inst && r2i) {
        kl_kir_inst *r2l = get_last(r2i);
        r2l->next = inst;
        inst = r2i;
    }

    return inst;
}

static kl_kir_inst *gen_apply_str(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_expr *l = e->lhs;
    kl_expr *r = e->rhs;
    if (r->nodetype != TK_VSTR) {
        return NULL;
    }

    kl_kir_inst *chktype = gen_check_type(ctx, sym, r1, l, r);
    if (chktype) {
        return chktype;
    }

    kl_kir_inst *r2i = NULL;
    kl_kir_opr r2 = {0};
    KL_KIR_CHECK_LVALUE(l, r2, r2i);
    kl_kir_opr r3 = make_lit_str(ctx, r->val.str);
    kl_kir_inst *inst;
    inst = new_inst_op3(ctx->program, e->line, e->pos, ctx->in_lvalue ? KIR_APLYL : KIR_APLY, r1, &r2, &r3);
    if (r2i) {
        kl_kir_inst *r2l = get_last(r2i);
        r2l->next = inst;
        inst = r2i;
    }

    return inst;
}

static kl_kir_inst *gen_op3_inst(kl_context *ctx, kl_symbol *sym, kl_kir op, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_opr rr = {0};
    if (!r1) {
        rr = make_ret_var(ctx, sym);
        r1 = &rr;
    }
    kl_kir_inst *r2i = NULL;
    kl_kir_inst *r3i = NULL;
    kl_kir_opr r2 = {0};
    kl_kir_opr r3 = {0};
    kl_expr *l = e->lhs;
    kl_expr *r = e->rhs;

    KL_KIR_CHECK_LITERAL(l, r2, r2i);
    if (!r2i && r2.t != TK_VSINT && r2.t != TK_VAR) {
        kl_kir_opr r2x = make_var(ctx, sym, l->typeid);
        r2i = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, &r2x, &r2);
        r2 = r2x;
    }
    kl_kir_inst *r2l = get_last(r2i);

    KL_KIR_CHECK_LITERAL(r, r3, r3i);
    if (!r3i && r3.t != TK_VSINT && r3.t != TK_VAR) {
        kl_kir_opr r3x = make_var(ctx, sym, r->typeid);
        r3i = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, &r3x, &r3);
        r3 = r3x;
    }
    kl_kir_inst *r3l = get_last(r3i);

    kl_kir_inst *inst = new_inst_op3(ctx->program, e->line, e->pos, op, r1, &r2, &r3);
    set_file_func(ctx, sym, inst);
    inst->catchid = ctx->tclabel > 0 ? ctx->tclabel : sym->funcend;

    if (r3i) {
        r3l->next = inst;
        inst = r3i;
    }
    if (r2i) {
        r2l->next = inst;
        inst = r2i;
    }

    return inst;
}

static kl_kir_inst *gen_callargs(kl_context *ctx, kl_symbol *sym, kl_expr *e, int *args, int callcnt)
{
    if (!e) {
        return NULL;
    }

    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    if (e->nodetype == TK_COMMA) {        
        if (e->rhs) {
            head = gen_callargs(ctx, sym, e->rhs, args, callcnt);
            last = get_last(head);
            if (last) {
                if (e->lhs) {
                    kl_kir_inst *next = gen_callargs(ctx, sym, e->lhs, args, callcnt);
                    KIR_ADD_NEXT(last, next);
                }
            }
        }
    } else {
        kl_kir_inst *r2i = NULL;
        kl_kir_opr r2 = {0};
        KL_KIR_CHECK_LITERAL(e, r2, r2i);
        r2.callcnt = callcnt;
        if (r2i) {
            head = r2i;
            last = get_last(head);
        }
        if (!last) {
            head = new_inst_op1(ctx->program, e->line, e->pos, KIR_PUSHARG, &r2);
            set_file_func(ctx, sym, head);
        } else {
            last->next = new_inst_op1(ctx->program, e->line, e->pos, KIR_PUSHARG, &r2);
            last = last->next;
            set_file_func(ctx, sym, last);
        }
        ++(*args);
    }
    return head;
}

static kl_kir_inst *gen_call(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_opr rr = {0};
    if (!r1) {
        rr = make_ret_var(ctx, sym);
        r1 = &rr;
    }
    kl_kir_opr r2 = {0};
    kl_kir_inst *r1i = NULL;
    kl_kir_inst *r1l = NULL;
    kl_kir_inst *pushobj = NULL;

    int pcallcnt = ctx->callcnt;
    int callcnt = sym->callcnt++;
    ctx->callcnt = callcnt;
    kl_expr *f = e->lhs;
    kl_symbol *fsym = (f->sym && f->sym->ref) ? f->sym->ref : f->sym;
    if (f->nodetype == TK_VAR && fsym && fsym->symtoken != TK_VAR) {
        r2.t = TK_VAR;
        r2.funcid = fsym->funcid;
        r2.name = fsym->funcname;
        r2.index = fsym->index;
        r2.level = f->sym->level;   // ref doesn't have a level.
        r2.recursive = f->sym->is_recursive;
        r2.typeid = f->typeid;
    } else {
        KL_KIR_CHECK_LITERAL(e->lhs, r2, r1i);
        if (r1i) {
            r1l = get_last(r1i);
            if (r1l->opcode == KIR_APLY) {
                r1l->next = new_inst_op1(ctx->program, e->line, e->pos, KIR_PUSHSYS, &(r1l->r2));
                r1l = r1l->next;
                r1l->r1.callcnt = callcnt;
                set_file_func(ctx, sym, r1l);
            }
        }
    }
    ctx->callcnt = pcallcnt;
    r1->typeid = TK_TANY;

    r2.callcnt = callcnt;
    kl_kir_inst *r2i = gen_callargs(ctx, sym, e->rhs, &(r2.args), callcnt);
    kl_kir_inst *inst = new_inst_op2(ctx->program, e->line, e->pos, KIR_CALL, r1, &r2);
    set_file_func(ctx, sym, inst);
    kl_kir_inst *ilst = get_last(inst);

    kl_kir_opr rc = { .t = TK_VSINT, .i64 = callcnt, .typeid = TK_TSINT64 };
    ilst->next = new_inst_op1(ctx->program, e->line, e->pos, KIR_RSSTKP, &rc);
    ilst = ilst->next;
    ilst->next = new_inst_chkexcept(ctx->program, e->line, e->pos, ctx->tclabel > 0 ? ctx->tclabel : sym->funcend);
    ilst = ilst->next;
    set_file_func(ctx, sym, ilst);
    if (!sym->is_global && !ctx->in_finally) {
        ilst->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_YIELDC, r1, &r2);
        ilst = ilst->next;
        set_file_func(ctx, sym, ilst);
        ilst->labelid = e->yield;
    }

    /* r2i: arguments */
    /* r1i: func */
    /* inst: call */

    if (r1l) {
        r1l->next = inst;
        inst = r1i;
    }
    if (r2i) {
        kl_kir_inst *r2l = get_last(r2i);
        r2l->next = inst;
        inst = r2i;
    }

    kl_kir_inst *head = new_inst_op1(ctx->program, e->line, e->pos, KIR_SVSTKP, &rc);
    head->next = inst;
    return head;
}

static kl_kir_inst *gen_array_lvalue(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r2, kl_expr *e, int *idx)
{
    if (!e) {
        ++(*idx);
        return NULL;
    }

    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    kl_kir_inst *inst = NULL;
    kl_kir_opr rs = {0};
    kl_kir_opr r3 = {0};
    switch (e->nodetype) {
    case TK_COMMA:
        head = gen_array_lvalue(ctx, sym, r2, e->lhs, idx);
        last = get_last(head);
        if (last) {
            last->next = gen_array_lvalue(ctx, sym, r2, e->rhs, idx);
        } else {
            head = gen_array_lvalue(ctx, sym, r2, e->rhs, idx);
        }
        break;
    case TK_VAR:
        if (r2->t != TK_VAR) {
            mkkir_error(ctx, __LINE__, e->sym ? e->sym : sym, "Can't apply the index");
        }
        KL_KIR_CHECK_LITERAL(e, rs, head);
        r3 = make_lit_i64(ctx, *idx);
        ++(*idx);
        inst = new_inst_op3(ctx->program, e->line, e->pos, rs.has_dot3 ? KIR_IDXFRM : KIR_IDX, &rs, r2, &r3);
        if (!head) {
            head = inst;
        } else {
            last = get_last(head);
            last->next = inst;
        }
        break;
    case TK_IDX: {
        if (r2->t != TK_VAR) {
            mkkir_error(ctx, __LINE__, e->sym ? e->sym : sym, "Can't apply the index");
        }
        rs = make_lit_i64(ctx, *idx);
        r3 = make_var(ctx, sym, TK_TANY);
        ++(*idx);
        head = new_inst_op3(ctx->program, e->line, e->pos, KIR_IDX, &r3, r2, &rs);
        last = get_last(head);
        rs = make_var(ctx, sym, TK_TANY);
        last->next = gen_apply(ctx, sym, &rs, e);
        last = get_last(head);
        last->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOVA, &rs, &r3);
        set_file_func(ctx, sym, last->next);
        break;
    }
    case TK_RANGE2:
    case TK_RANGE3: {
        if (r2->t != TK_VAR) {
            mkkir_error(ctx, __LINE__, e->sym ? e->sym : sym, "Can't apply the index");
        }
        rs = make_var(ctx, sym, TK_TANY);
        r3 = make_lit_i64(ctx, *idx);
        ++(*idx);
        head = new_inst_op3(ctx->program, e->line, e->pos, KIR_IDX, &rs, r2, &r3);
        kl_kir_opr r3 = make_var(ctx, sym, TK_TANY);
        head->next = gen_range(ctx, sym, e, &r3, e->nodetype == TK_RANGE2 ? 2 : 3);
        last = get_last(head);
        last->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_CHKRANGE, &r3, &rs);
        set_file_func(ctx, sym, last->next);
        break;
    }
    default:
        if (r2->t != TK_VAR) {
            mkkir_error(ctx, __LINE__, e->sym ? e->sym : sym, "Can't apply the index");
        }
        rs = make_var(ctx, sym, TK_TANY);
        r3 = make_lit_i64(ctx, *idx);
        ++(*idx);
        head = new_inst_op3(ctx->program, e->line, e->pos, KIR_IDX, &rs, r2, &r3);
        head->next = gen_assign_object(ctx, sym, &rs, e);
        break;
    }
    return head;
}

static int check_dot_symbol(kl_expr *e)
{
    int dot3 = 0;
    switch (e->nodetype) {
    case TK_VKV: {
        kl_expr *l = e->rhs;
        switch (l->nodetype) {
        case TK_VAR:
            if (l->sym && l->sym->is_dot3) {
                dot3 = 1;
            }
            break;
        case TK_DOT3:
            dot3 = 1;
            break;
        }
        break;
    }
    case TK_COMMA:
        dot3 = check_dot_symbol(e->lhs);
        if (!dot3) {
            dot3 = check_dot_symbol(e->rhs);
        }
        break;
    }
    return dot3;
}

static kl_kir_inst *gen_object_lvalue(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_kir_opr *r2, kl_expr *e, int dot3)
{
    /* e->lhs should be a string. */
    kl_kir_inst *head = NULL;
    kl_kir_opr r3 = make_lit_str(ctx, e->lhs->val.str);
    switch (e->nodetype) {
    case TK_VKV: {
        kl_kir_opr rs = {0};
        kl_expr *r = e->rhs;
        switch (r->nodetype) {
        case TK_VAR:
            check_autoset_flag(ctx, r->sym);
            rs = make_var_index(ctx, r->sym->ref ? r->sym->ref->index : r->sym->index, r->sym->level, r->typeid);
            if (r->sym->is_dot3) {
                rs.has_dot3 = 1;
            }
            kl_kir_inst *inst;
            if (rs.has_dot3) {
                inst = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, &rs, r1);
            } else {
                inst = new_inst_op3(ctx->program, e->line, e->pos, KIR_APLY, &rs, r2, &r3);
            }
            if (!head) {
                head = inst;
            } else {
                kl_kir_inst *last = get_last(head);
                last->next = inst;
            }
            if (dot3 && !rs.has_dot3) {
                inst->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_REMOVE, r1, &r3);
            }
            break;
        case TK_VBOOL: {
            kl_kir_opr rr = make_lit_bool(ctx, r->val.i64);
            rs = make_var(ctx, sym, TK_TANY);
            head = new_inst_op3(ctx->program, r->line, r->pos, KIR_APLY, &rs, r2, &r3);
            head->next = new_inst_op2(ctx->program, r->line, r->pos, KIR_CHKMATCH, &rs, &rr);
            set_file_func(ctx, sym, head->next);
            break;
        }
        case TK_VSINT: {
            kl_kir_opr rr = make_lit_i64(ctx, r->val.i64);
            rs = make_var(ctx, sym, TK_TANY);
            head = new_inst_op3(ctx->program, r->line, r->pos, KIR_APLY, &rs, r2, &r3);
            head->next = new_inst_op2(ctx->program, r->line, r->pos, KIR_CHKMATCH, &rs, &rr);
            set_file_func(ctx, sym, head->next);
            break;
        }
        case TK_VDBL: {
            kl_kir_opr rr = make_lit_dbl(ctx, r->val.dbl);
            rs = make_var(ctx, sym, TK_TANY);
            head = new_inst_op3(ctx->program, r->line, r->pos, KIR_APLY, &rs, r2, &r3);
            head->next = new_inst_op2(ctx->program, r->line, r->pos, KIR_CHKMATCH, &rs, &rr);
            set_file_func(ctx, sym, head->next);
            break;
        }
        case TK_VSTR: {
            kl_kir_opr rr = make_lit_str(ctx, r->val.str);
            rs = make_var(ctx, sym, TK_TANY);
            head = new_inst_op3(ctx->program, r->line, r->pos, KIR_APLY, &rs, r2, &r3);
            head->next = new_inst_op2(ctx->program, r->line, r->pos, KIR_CHKMATCH, &rs, &rr);
            set_file_func(ctx, sym, head->next);
            break;
        }
        default:
            rs = make_var(ctx, sym, TK_TANY);
            head = new_inst_op3(ctx->program, r->line, r->pos, KIR_APLY, &rs, r2, &r3);
            head->next = gen_assign_object(ctx, sym, &(rs), r);
            break;
        }
        break;
    }
    case TK_COMMA:
        head = gen_object_lvalue(ctx, sym, r1, r2, e->lhs, dot3);
        kl_kir_inst *last = get_last(head);
        last->next = gen_object_lvalue(ctx, sym, r1, r2, e->rhs, dot3);
        break;
    }
    return head;
}

static kl_kir_inst *gen_assign_object(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_inst *head = NULL;
    switch (e->nodetype) {
    case TK_VARY: {
        int idx = 0;
        head = gen_array_lvalue(ctx, sym, r1, e->lhs, &idx);
        break;
    }
    case TK_VOBJ: {
        int dot3 = check_dot_symbol(e->lhs);
        if (dot3) {
            kl_kir_opr rr = make_var(ctx, sym, TK_TANY);
            head = new_inst_op2(ctx->program, e->line, e->pos, KIR_OBJCPY, &rr, r1);
            head->next = gen_object_lvalue(ctx, sym, &rr, r1, e->lhs, dot3);
        } else {
            head = gen_object_lvalue(ctx, sym, r1, r1, e->lhs, dot3);
        }
        break;
    }
    default:
        break;
    }
    return head;
}

static kl_kir_inst *gen_bnot(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_inst *r2i = NULL;
    kl_kir_opr r2 = {0};
    KL_KIR_CHECK_LITERAL(e->lhs, r2, r2i);
    kl_kir_inst *head = new_inst_op2(ctx->program, e->line, e->pos, KIR_BNOT, r1, &r2);
    set_file_func(ctx, sym, head);
    if (r2i) {
        kl_kir_inst *r2l = get_last(r2i);
        r2l->next = head;
        head = r2i;
    }
    return head;
}

static kl_kir_inst *gen_not(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_inst *r2i = NULL;
    kl_kir_opr r2 = {0};
    KL_KIR_CHECK_LITERAL(e->lhs, r2, r2i);
    kl_kir_inst *head = new_inst_op2(ctx->program, e->line, e->pos, KIR_NOT, r1, &r2);
    set_file_func(ctx, sym, head);
    if (r2i) {
        kl_kir_inst *r2l = get_last(r2i);
        r2l->next = head;
        head = r2i;
    }
    return head;
}

static int is_same_array_index(kl_expr *a, kl_expr *b)
{
    if ((a->nodetype == TK_VSINT || a->nodetype == TK_VBOOL) && (b->nodetype == TK_VSINT || b->nodetype == TK_VBOOL)) {
        return a->val.i64 == b->val.i64;
    }
    return 0;
}

static int is_same_var(kl_expr *a, kl_expr *b)
{
    if (a->nodetype == TK_VAR && b->nodetype == TK_VAR) {
        kl_symbol *sa = a->sym;
        kl_symbol *sb = b->sym;
        return sa && sb && sa->index == sb->index && sa->level == sb->level;
    }
    return 0;
}

static int is_same_elem(kl_expr *a, kl_expr *b)
{
    if (a->nodetype == TK_IDX && b->nodetype == TK_IDX) {
        if (is_same_var(a->lhs, b->lhs)) {
            if (is_same_var(a->rhs, b->rhs) || is_same_array_index(a->rhs, b->rhs)) {
                return 1;
            }
        }
    }

    return 0;
}

static kl_kir_inst *check_swap(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_inst *head = NULL;
    kl_expr *l = e->lhs;
    kl_expr *r = e->rhs;
    if (l->nodetype == TK_VARY && r->nodetype == TK_VARY) {
        l = l->lhs; /* l should be TK_COMMA */
        r = r->lhs; /* r should be TK_COMMA */
        kl_expr *ll = l->lhs;
        kl_expr *lr = l->rhs;
        kl_expr *rl = r->lhs;
        kl_expr *rr = r->rhs;
        if (is_same_var(ll, rr) && is_same_var(lr, rl)) {
            kl_symbol *sl = ll->sym;
            kl_symbol *sr = lr->sym;
            kl_kir_opr r2 = make_var_index(ctx, sl->index, sl->level, sl->typeid);
            kl_kir_opr r3 = make_var_index(ctx, sr->index, sr->level, sr->typeid);
            head = new_inst_op2(ctx->program, e->line, e->pos, KIR_SWAP, &r2, &r3);
        } else if (is_same_elem(ll, rr) && is_same_elem(lr, rl)) {
            kl_kir_opr r2 = make_var(ctx, sym, l->lhs->typeid);
            kl_kir_opr r3 = make_var(ctx, sym, l->rhs->typeid);
            ctx->in_lvalue = 1;
            kl_kir_inst *r2i = gen_apply(ctx, sym, &r2, l->lhs);
            kl_kir_inst *r3i = gen_apply(ctx, sym, &r3, l->rhs);
            ctx->in_lvalue = 0;
            head = new_inst_op2(ctx->program, e->line, e->pos, KIR_SWAPA, &r2, &r3);
            kl_kir_inst *last = get_last(r2i);
            last->next = r3i;
            last = get_last(r3i);
            last->next = head;
            head = r2i;
        }
    }

    return head;
}

static kl_kir_inst *gen_assign(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_kir_opr *r2, kl_expr *e)
{
    kl_expr *l = e->lhs;
    kl_kir_inst *head = NULL;
    if (!r2 && e->lhs && e->rhs && e->lhs->nodetype == TK_VAR) {
        kl_symbol *lsym = l->sym->ref ? l->sym->ref : l->sym;
        kl_kir_opr rr = make_var_index(ctx, lsym->index, l->sym->level, l->typeid);
        head = gen_expr(ctx, sym, &rr, e->rhs);
        kl_kir_inst *last = get_last(head);
        if (r1 && r1->index >= 0 && (r1->index != rr.index || r1->level != rr.level)) {
            last->next = new_inst_op2(ctx->program, l->line, l->pos, KIR_MOV, r1, &rr);
        }
        return head;
    }

    head = check_swap(ctx, sym, r1, e);
    if (head) {
        return head;
    }

    kl_kir_opr r2x = {0};
    kl_kir_inst *last = NULL;
    if (!r2) {
        r2 = &r2x;
        KL_KIR_CHECK_LITERAL(e->rhs, *r2, head);
        last = get_last(head);
    }

    int lvalue = ctx->in_lvalue;
    ctx->in_lvalue = 1;
    if (l->nodetype == TK_VAR) {
        kl_symbol *lsym = l->sym->ref ? l->sym->ref : l->sym;
        if (last &&
                (l->sym->level == last->r2.level) && (l->sym->index == last->r2.index) &&
                (r2->level == last->r1.level) && (r2->index == last->r1.index)) {
            last->r1.level = last->r2.level;
            last->r1.index = last->r2.index;
        } else {
            kl_kir_opr rr = make_var_index(ctx, lsym->index, l->sym->level, l->typeid);
            if (!head) {
                head = last = new_inst_op2(ctx->program, l->line, l->pos, KIR_MOV, &rr, r2);
            } else {
                last->next = new_inst_op2(ctx->program, l->line, l->pos, KIR_MOV, &rr, r2);
                last = last->next;
            }
            if (r1 && r1->index >= 0) {
                if (r1->index != rr.index || r1->level != rr.level) {
                    last->next = new_inst_op2(ctx->program, l->line, l->pos, KIR_MOV, r1, &rr);
                }
            }
        }
    } else if (l->nodetype == TK_DOT || l->nodetype == TK_IDX) {
        int lvalue = ctx->in_lvalue;
        ctx->in_lvalue = 1;
        kl_kir_opr rr = {0};
        if (!r1 || r1->index < 0) {
            rr = make_var(ctx, sym, l->typeid);
            r1 = &rr;
        }
        kl_kir_inst *r1i = gen_expr(ctx, sym, r1, l);
        kl_kir_inst *r1l = get_last(r1i);
        ctx->in_lvalue = lvalue;
        r1l->next = new_inst_op2(ctx->program, l->line, l->pos, KIR_MOVA, r1, r2);
        set_file_func(ctx, sym, r1l->next);
        if (!head) {
            head = r1i;
        } else {
            last->next = r1i;
        }
    } else {
        /* Destructuring assignment */
        if (!head) {
            head = gen_assign_object(ctx, sym, r2, l);
        } else {
            last->next = gen_assign_object(ctx, sym, r2, l);
        }
    }
    ctx->in_lvalue = lvalue;

    return head;
}

static kl_kir_inst *gen_xassign(kl_context *ctx, kl_symbol *sym, kl_kir op, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_inst *head = gen_op3_inst(ctx, sym, op, r1, e);
    kl_kir_inst *last = get_last(head);

    kl_expr *l = e->lhs;
    if (l->nodetype == TK_VAR) {
        kl_kir_opr r3 = make_var_index(ctx, l->sym->ref ? l->sym->ref->index : l->sym->index, l->sym->level, l->typeid);
        last->next = new_inst_op2(ctx->program, l->line, l->pos, KIR_MOV, &r3, r1);
    } else if (l->nodetype == TK_DOT || l->nodetype == TK_IDX) {
        int lvalue = ctx->in_lvalue;
        ctx->in_lvalue = 1;
        kl_kir_opr r3 = {0};
        kl_kir_inst *r3i = NULL;
        KL_KIR_CHECK_LVALUE(l, r3, r3i);
        ctx->in_lvalue = lvalue;
        kl_kir_inst *inst = new_inst_op2(ctx->program, l->line, l->pos, KIR_MOVA, &r3, r1);
        set_file_func(ctx, sym, inst);
        if (r3i) {
            kl_kir_inst *r3l = get_last(r3i);
            r3l->next = inst;
            inst = r3i;
        }
        last->next = inst;
    } else {
        /* TODO: Error */
    }

    return head;
}

static kl_kir_inst *gen_land_lor_assign(kl_context *ctx, kl_symbol *sym, kl_expr *e, int land)
{
    int l1 = get_next_label(ctx);
    kl_expr *l = e->lhs;
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    if (l->nodetype == TK_VAR) {
        kl_kir_opr r2 = make_var_index(ctx, l->sym->ref ? l->sym->ref->index : l->sym->index, l->sym->level, l->typeid);
        head = land
            ? new_inst_jumpiff(ctx->program, e->line, e->pos, &r2, l1)
            : new_inst_jumpift(ctx->program, e->line, e->pos, &r2, l1);
        kl_kir_opr r3 = make_var_index(ctx, l->sym->ref ? l->sym->ref->index : l->sym->index, l->sym->level, l->typeid);
        last = get_last(head);
        last->next = gen_expr(ctx, sym, &r3, e->rhs);
    } else if (l->nodetype == TK_DOT || l->nodetype == TK_IDX) {
        kl_kir_opr r2 = make_var(ctx, sym, TK_TANY);
        head = gen_expr(ctx, sym, &r2, l);
        kl_kir_inst *r1l = get_last(head);
        r1l->next = land
            ? new_inst_jumpiff(ctx->program, e->line, e->pos, &r2, l1)
            : new_inst_jumpift(ctx->program, e->line, e->pos, &r2, l1);
        r1l = r1l->next;
        r1l->next = gen_expr(ctx, sym, &r2, e->rhs);
        last = get_last(r1l);
        int lvalue = ctx->in_lvalue;
        ctx->in_lvalue = 1;
        kl_kir_opr r3 = {0};
        kl_kir_inst *r3i = NULL;
        KL_KIR_CHECK_LVALUE(l, r3, r3i);
        ctx->in_lvalue = lvalue;
        kl_kir_inst *inst = new_inst_op2(ctx->program, l->line, l->pos, KIR_MOVA, &r3, &r2);
        set_file_func(ctx, sym, inst);
        if (r3i) {
            kl_kir_inst *r3l = get_last(r3i);
            r3l->next = inst;
            inst = r3i;
        }
        last->next = inst;
    } else {
        /* TODO: Error */
    }

    KIR_MOVE_LAST(last);
    last->next = new_inst_label(ctx->program, e->line, e->pos, l1, last, 0);
    return head;
}

static kl_kir_inst *gen_nullc_assign(kl_context *ctx, kl_symbol *sym, kl_expr *e)
{
    int l1 = get_next_label(ctx);
    kl_expr *l = e->lhs;
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    if (l->nodetype == TK_VAR) {
        kl_kir_opr r1 = make_var_index(ctx, l->sym->ref ? l->sym->ref->index : l->sym->index, l->sym->level, l->typeid);
        kl_kir_opr r2 = make_lit_i64(ctx, VAR_DEF);
        kl_kir_opr r3 = make_var(ctx, sym, TK_TANY);
        head = new_inst_op3(ctx->program, e->line, e->pos, KIR_TYPE, &r3, &r1, &r2);
        head->next = new_inst_jumpift(ctx->program, e->line, e->pos, &r3, l1);
        r3 = make_var_index(ctx, l->sym->ref ? l->sym->ref->index : l->sym->index, l->sym->level, l->typeid);
        last = get_last(head);
        last->next = gen_expr(ctx, sym, &r3, e->rhs);
    } else if (l->nodetype == TK_DOT || l->nodetype == TK_IDX) {
        kl_kir_opr r1 = make_var(ctx, sym, TK_TANY);
        kl_kir_opr r2 = make_lit_i64(ctx, VAR_DEF);
        kl_kir_opr r3 = make_var(ctx, sym, TK_TANY);
        head = gen_expr(ctx, sym, &r1, l);
        kl_kir_inst *r1l = get_last(head);
        r1l->next = new_inst_op3(ctx->program, e->line, e->pos, KIR_TYPE, &r3, &r1, &r2);
        r1l = r1l->next;
        r1l->next = new_inst_jumpift(ctx->program, e->line, e->pos, &r3, l1);
        r1l = r1l->next;
        r1l->next = gen_expr(ctx, sym, &r1, e->rhs);
        last = get_last(r1l);
        int lvalue = ctx->in_lvalue;
        ctx->in_lvalue = 1;
        kl_kir_opr r4 = {0};
        kl_kir_inst *r4i = NULL;
        KL_KIR_CHECK_LVALUE(l, r4, r4i);
        ctx->in_lvalue = lvalue;
        kl_kir_inst *inst = new_inst_op2(ctx->program, l->line, l->pos, KIR_MOVA, &r4, &r1);
        set_file_func(ctx, sym, inst);
        if (r4i) {
            kl_kir_inst *r4l = get_last(r4i);
            r4l->next = inst;
            inst = r4i;
        }
        last->next = inst;
    } else {
        /* TODO: Error */
    }

    KIR_MOVE_LAST(last);
    last->next = new_inst_label(ctx->program, e->line, e->pos, l1, last, 0);
    return head;
}

static kl_kir_inst *gen_regmatch(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e, tk_token op)
{
    kl_kir_inst *lhead = NULL;
    kl_kir_opr r2 = {0};
    KL_KIR_CHECK_LITERAL(e->lhs, r2, lhead);

    kl_kir_inst *rhead = NULL;
    kl_kir_opr r3 = {0};
    KL_KIR_CHECK_LITERAL(e->rhs, r3, rhead);

    kl_kir_inst *inst = new_inst_op3(ctx->program, e->line, e->pos, op == TK_REGEQ ? KIR_REGEQ : KIR_REGNE, r1, &r2, &r3);
    set_file_func(ctx, sym, inst);

    if (rhead) {
        kl_kir_inst *rlast = get_last(rhead);
        rlast->next = inst;
        inst = rhead;
    }
    if (lhead) {
        kl_kir_inst *llast = get_last(lhead);
        llast->next = inst;
        inst = lhead;
    }

    return inst;
}

static kl_kir_inst *gen_ternary_expr(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e, int l1)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *r1l = NULL;
    head = gen_expr(ctx, sym, r1, e->lhs);
    kl_kir_inst *last = get_last(head);

    int l2 = get_next_label(ctx);
    last->next = new_inst_jumpiff(ctx->program, e->line, e->pos, r1, l2);
    last = last->next;

    last->next = gen_expr(ctx, sym, r1, e->rhs);
    last = get_last(last);

    last->next = new_inst_jump(ctx->program, e->line, e->pos, l1, last);
    last = last->next;

    last->next = new_inst_label(ctx->program, e->line, e->pos, l2, last, 0);
    last = last->next;

    last->next = gen_expr(ctx, sym, r1, e->xhs);

    return head;
}

static kl_kir_inst *gen_ret(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    kl_kir_opr r1 = make_ret_var(ctx, sym);

    if (s->e1) {
        KL_KIR_CHECK_LITERAL(s->e1, r1, head);
        last = get_last(head);
        if (!head) {
            kl_kir_opr r2 = make_ret_var(ctx, sym);
            head = last = new_inst_op2(ctx->program, s->line, s->pos, KIR_MOV, &r2, &r1);
        }
    } else {
        head = last = new_inst_op2(ctx->program, s->line, s->pos, KIR_MOV, &r1, &r1);  /* make undefined */
    }

    if (ctx->fincode) {
        ctx->in_finally = 1;
        last->next = gen_block(ctx, sym, ctx->fincode);
        KIR_MOVE_LAST(last);
        ctx->in_finally = 0;
    }

    last->next = new_inst_jump(ctx->program, s->line, s->pos, sym->funcend, last);
    return head;
}

static kl_kir_inst *gen_throw(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *fin = NULL;
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;

    if (ctx->in_catch && ctx->fincode) {
        ctx->in_finally = 1;
        fin = gen_block(ctx, sym, ctx->fincode);
        ctx->in_finally = 0;
    }

    if (s->e1) {
        kl_kir_opr r1 = make_ret_var(ctx, sym);
        r1 = make_var(ctx, sym, s->e1->typeid);
        head = gen_expr(ctx, sym, &r1, s->e1);
        last = get_last(head);

        if (!head) {
            head = last = new_inst_op1(ctx->program, s->line, s->pos, KIR_THROWE, &r1);
        } else {
            last = get_last(head);
            last->next = new_inst_op1(ctx->program, s->line, s->pos, KIR_THROWE, &r1);
            last = last->next;
        }
    } else {
        if (!head) {
            head = last = new_inst(ctx->program, s->line, s->pos, KIR_THROW);
        } else {
            last->next = new_inst(ctx->program, s->line, s->pos, KIR_THROW);
            last = last->next;
        }
    }
    set_file_func(ctx, sym, last);
    last->labelid = ctx->tclabel > 0 ? ctx->tclabel : sym->funcend;

    if (fin) {
        last = get_last(fin);
        last->next = head;
        head = fin;
    }
    return head;
}

static void gen_case_label(kl_context *ctx, kl_stmt *s, int *n)
{
    if (s->ncase) {
        gen_case_label(ctx, s->ncase, n);
    }
    s->labelno = get_next_label(ctx);

    kl_expr *v = s->e1;
    if (v && v->nodetype == TK_VSINT) {
        (*n)++;
    }
}

static kl_kir_inst *gen_case_others(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *inst = NULL;
    if (s->ncase) {
        head = gen_case_others(ctx, sym, s->ncase);
    }
    if (s->nodetype == TK_DEFAULT || s->nodetype == TK_OTHERWISE) {
        return head;
    }

    kl_expr *v = s->e1;
    if (v && v->nodetype != TK_VSINT) {
        kl_symbol *ssym = ctx->switchstmt->sym;
        kl_kir_opr sv = make_var_index(ctx, ssym->index, 0, TK_TANY);
        kl_kir_inst *r1i = NULL;
        kl_kir_opr r1 = {0};
        KL_KIR_CHECK_LITERAL(v, r1, r1i);
        inst = new_inst_op2(ctx->program, s->line, s->pos, KIR_CASEV, &sv, &r1);
        set_file_func(ctx, sym, inst);
        inst->labelid = s->labelno;
        if (r1i) {
            kl_kir_inst *r1l = get_last(r1i);
            r1l->next = inst;
            inst = r1i;
        }
        if (head) {
            kl_kir_inst *last = get_last(head);
            last->next = inst;
        } else {
            head = inst;
        }
    }

    return head;
}

static kl_kir_inst *gen_switch(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    int l1 = get_next_label(ctx);
    int ldef = get_next_label(ctx);
    int blabel = ctx->blabel;
    ctx->blabel = s->labelno = l1;
    if (s->sym) {
        s->sym->blabel = l1;
    }

    int n = 0;
    if (s->ncase) {
        gen_case_label(ctx, s->ncase, &n);
    }

    kl_stmt *prev = ctx->switchstmt;
    ctx->switchstmt = s;
    kl_stmt *ccase = ctx->whenstmt;
    ctx->whenstmt = NULL;

    kl_kir_opr r1 = {0};
    KL_KIR_CHECK_LITERAL(s->e1, r1, head);
    kl_kir_opr sv = make_var_index(ctx, s->sym->index, 0, TK_TANY);
    if (n == 0) {
        head = last = new_inst_op2(ctx->program, s->line, s->pos, KIR_MOV, &sv, &r1);
        last->next = gen_case_others(ctx, sym, s->ncase);
        last = get_last(last);
        if (s->defcase) {    /* default: */
            last->next = new_inst_jump(ctx->program, s->defcase->line, s->defcase->pos, s->defcase->labelno, NULL);
            last = last->next;
        }
    } else {
        if (!head) {
            head = last = new_inst_op2(ctx->program, s->line, s->pos, KIR_SWITCHS, &sv, &r1);
            head->labelid = ldef;
        } else {
            last = get_last(head);
            last->next = new_inst_op2(ctx->program, s->line, s->pos, KIR_SWITCHS, &sv, &r1);
            last = last->next;
            last->labelid = ldef;
        }
    }

    last->next = gen_stmt(ctx, sym, s->s1);
    last = get_last(last);

    if (n != 0) {
        last->next = new_inst_jump(ctx->program, s->line, s->pos, l1, NULL);
        last = last->next;
        last->next = new_inst_label(ctx->program, s->line, s->pos, ldef, last, 0);
        last = last->next;
        last->next = new_inst(ctx->program, s->line, s->pos, KIR_DEFAULT);
        last = last->next;
        last->next = gen_case_others(ctx, sym, s->ncase);
        last = get_last(last);
        if (s->defcase) {    /* default: */
            last->next = new_inst_jump(ctx->program, s->defcase->line, s->defcase->pos, s->defcase->labelno, NULL);
            last = last->next;
        }
        last->next = new_inst(ctx->program, s->line, s->pos, KIR_SWITCHE);
        last = last->next;
    }

    ctx->whenstmt = ccase;
    ctx->switchstmt = prev;
    last->next = new_inst_label(ctx->program, s->line, s->pos, l1, last, 0);
    ctx->blabel = blabel;
    return head;
}

static kl_kir_inst *gen_case(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *when = NULL;
    if (ctx->whenstmt) {
        when = new_inst_jump(ctx->program, s->line, s->pos, ctx->blabel, NULL);
    }
    kl_kir_inst *head, *last;
    kl_expr *v = s->e1;
    if (v && v->nodetype != TK_VSINT) {
        head = new_inst_label(ctx->program, s->line, s->pos, s->labelno, NULL, 0);
    } else {
        kl_kir_opr r1 = make_lit_i64(ctx, s->e1->val.i64);
        head = new_inst_op1(ctx->program, s->line, s->pos, KIR_CASEI, &r1);
        head->next = new_inst_label(ctx->program, s->line, s->pos, s->labelno, NULL, 0);
        last = get_last(head);
    }

    ctx->whenstmt = NULL;
    if (when) {
        when->next = head;
        return when;
    }
    return head;
}

static kl_kir_inst *gen_when(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *when = NULL;
    if (ctx->whenstmt) {
        when = new_inst_jump(ctx->program, s->line, s->pos, ctx->blabel, NULL);
    }
    kl_kir_inst *head, *last;
    kl_expr *v = s->e1;
    if (v && v->nodetype != TK_VSINT) {
        head = new_inst_label(ctx->program, s->line, s->pos, s->labelno, NULL, 0);
    } else {
        kl_kir_opr r1 = make_lit_i64(ctx, s->e1->val.i64);
        head = new_inst_op1(ctx->program, s->line, s->pos, KIR_CASEI, &r1);
        head->next = new_inst_label(ctx->program, s->line, s->pos, s->labelno, NULL, 0);
        last = get_last(head);
    }

    ctx->whenstmt = s;
    if (when) {
        when->next = head;
        return when;
    }
    return head;
}

static kl_kir_inst *gen_default(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *when = NULL;
    if (ctx->whenstmt) {
        when = new_inst_jump(ctx->program, s->line, s->pos, ctx->blabel, NULL);
    }
    kl_kir_inst *head = new_inst_label(ctx->program, s->line, s->pos, s->labelno, NULL, 0);

    ctx->whenstmt = NULL;
    if (when) {
        when->next = head;
        return when;
    }
    return head;
}

static kl_kir_inst *gen_otherwise(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *when = NULL;
    if (ctx->whenstmt) {
        when = new_inst_jump(ctx->program, s->line, s->pos, ctx->blabel, NULL);
    }
    kl_kir_inst *head = new_inst_label(ctx->program, s->line, s->pos, s->labelno, NULL, 0);

    ctx->whenstmt = s;
    if (when) {
        when->next = head;
        return when;
    }
    return head;
}

static kl_kir_inst *gen_if(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;

    int l1 = get_next_label(ctx);
    int l2 = get_next_label(ctx);
    int l3 = get_next_label(ctx);

    kl_kir_opr r1 = make_var(ctx, sym, s->e1->typeid);
    head = last = gen_expr(ctx, sym, &r1, s->e1);
    KIR_MOVE_LAST(last);
    kl_kir_inst *next = new_inst_jumpiff(ctx->program, s->line, s->pos, &r1, l2);
    KIR_ADD_NEXT(last, next);
    next = new_inst_label(ctx->program, s->line, s->pos, l1, last, 0);
    KIR_ADD_NEXT(last, next);
    if (s->s1) {
        next = gen_stmt(ctx, sym, s->s1);
        KIR_ADD_NEXT(last, next);
        KIR_MOVE_LAST(last);
        next = new_inst_jump(ctx->program, s->line, s->pos, l3, last);
        KIR_ADD_NEXT(last, next);
    }
    next = new_inst_label(ctx->program, s->line, s->pos, l2, last, 0);
    KIR_ADD_NEXT(last, next);
    if (s->s2) {
        next = gen_stmt(ctx, sym, s->s2);
        KIR_ADD_NEXT(last, next);
        KIR_MOVE_LAST(last);
    }
    next = new_inst_label(ctx->program, s->line, s->pos, l3, last, 0);
    KIR_ADD_NEXT(last, next);

    return head;
}

static kl_kir_inst *gen_while(kl_context *ctx, kl_symbol *sym, kl_stmt *s, int dowhile)
{
    int l1 = get_next_label(ctx);
    int l2;

    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    kl_kir_inst *next = NULL;

    int infinite = s->e1 && (s->e1->nodetype == TK_VSINT || s->e1->nodetype == TK_VBOOL) && s->e1->val.i64 != 0;

    if (dowhile || infinite) {
        l2 = l1;
        head = last = new_inst_label(ctx->program, s->line, s->pos, l1, head, 0);
    } else {
        l2 = get_next_label(ctx);
        head = new_inst_jump(ctx->program, s->line, s->pos, l2, NULL);
        last = new_inst_label(ctx->program, s->line, s->pos, l1, head, 1);
        head->next = last;
    }

    int l3 = get_next_label(ctx);
    int blabel = ctx->blabel;
    int clabel = ctx->clabel;
    ctx->blabel = l3;
    ctx->clabel = l2;
    if (s->sym) {
        s->sym->blabel = l3;
        s->sym->clabel = l2;
    }

    if (s->s1) {
        kl_kir_inst *next = gen_stmt(ctx, sym, s->s1);
        KIR_ADD_NEXT(last, next);
        KIR_MOVE_LAST(last);
    }
    if (!dowhile && !infinite) {
        next = new_inst_label(ctx->program, s->line, s->pos, l2, last, 0);
        KIR_ADD_NEXT(last, next);
    }

    if (infinite) {
        next = new_inst_jump(ctx->program, s->line, s->pos, l1, last);
        KIR_ADD_NEXT(last, next);
    } else {
        int noloop = s->e1 && (s->e1->nodetype == TK_VSINT || s->e1->nodetype == TK_VBOOL) && s->e1->val.i64 == 0;
        if (!noloop) {
            kl_kir_opr r1 = make_var(ctx, sym, s->e1->typeid);
            next = gen_expr(ctx, sym, &r1, s->e1);
            KIR_ADD_NEXT(last, next);
            KIR_MOVE_LAST(last);
            next = new_inst_jumpift(ctx->program, s->line, s->pos, &r1, l1);
            KIR_ADD_NEXT(last, next);
        }
    }

    next = new_inst_label(ctx->program, s->line, s->pos, l3, last, 0);
    KIR_ADD_NEXT(last, next);

    ctx->clabel = clabel;
    ctx->blabel = blabel;
    return head;
}

static kl_kir_inst *gen_for(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;

    int l1 = get_next_label(ctx);
    int l3 = get_next_label(ctx);
    int blabel = ctx->blabel;
    int clabel = ctx->clabel;
    ctx->blabel = l3;
    if (s->sym) {
        s->sym->blabel = l3;
    }

    if (!s->e1 && !s->e2 && !s->e3) {
        /* infinite loop */
        ctx->clabel = l1;
        if (s->sym) {
            s->sym->clabel = l1;
        }

        head = new_inst_label(ctx->program, s->line, s->pos, l1, head, 0);
        if (s->s1) {
            head->next = gen_stmt(ctx, sym, s->s1);
            last = get_last(head);
        }
        last->next = new_inst_jump(ctx->program, s->line, s->pos, l1, last);
        if (last->next) {
            last = last->next;
        }
        last->next = new_inst_label(ctx->program, s->line, s->pos, l3, last, 0);

        ctx->clabel = clabel;
        ctx->blabel = blabel;
        return head;
    }

    int l2 = get_next_label(ctx);
    ctx->clabel = get_next_label(ctx);
    if (s->sym) {
        s->sym->clabel = l2;
    }
    if (s->e1) {
        kl_kir_opr r1 = make_ret_var(ctx, sym);
        head = gen_expr(ctx, sym, &r1, s->e1);
        last = get_last(head);
    }

    kl_kir_inst *next = new_inst_jump(ctx->program, s->line, s->pos, l2, NULL);
    if (last) {
        last->next = next;
        last = next;
    } else {
        head = last = next;
    }

    next = new_inst_label(ctx->program, s->line, s->pos, l1, head, 1);
    KIR_ADD_NEXT(last, next);

    if (s->s1) {
        kl_kir_inst *next = gen_stmt(ctx, sym, s->s1);
        KIR_ADD_NEXT(last, next);
        KIR_MOVE_LAST(last);
    }

    next = new_inst_label(ctx->program, s->line, s->pos, ctx->clabel, last, 0);
    KIR_ADD_NEXT(last, next);

    if (s->e3) {
        kl_kir_opr r1 = make_ret_var(ctx, sym);
        r1.prevent = 1;
        next = gen_expr(ctx, sym, &r1, s->e3);
        if (KIR_INC <= next->opcode && next->opcode <= TK_DECP) {
            next->r1.prevent = 1;
        }
        KIR_ADD_NEXT(last, next);
        KIR_MOVE_LAST(last);
    }

    next = new_inst_label(ctx->program, s->line, s->pos, l2, last, 0);
    KIR_ADD_NEXT(last, next);

    if (s->e2) {
        if (s->e2->nodetype == TK_VSINT || s->e2->nodetype == TK_VBOOL) {
            if (s->e2->val.i64 != 0) {
                next = new_inst_jump(ctx->program, s->line, s->pos, l1, last);
                KIR_ADD_NEXT(last, next);
            } else {
                /* do nothing because of no loop. */;
            }
        } else {
            kl_kir_opr r1 = make_var(ctx, sym, s->e2->typeid);
            next = gen_expr(ctx, sym, &r1, s->e2);
            KIR_ADD_NEXT(last, next);
            KIR_MOVE_LAST(last);
            next = new_inst_jumpift(ctx->program, s->line, s->pos, &r1, l1);
            KIR_ADD_NEXT(last, next);
        }
    } else {
        next = new_inst_jump(ctx->program, s->line, s->pos, l1, last);
        KIR_ADD_NEXT(last, next);
    }

    last->next = new_inst_label(ctx->program, s->line, s->pos, l3, last, 0);

    ctx->clabel = clabel;
    ctx->blabel = blabel;
    return head;
}

static kl_kir_inst *gen_forin(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;

    int l1 = get_next_label(ctx);
    if (!s->e1 || !s->e2 || !s->sym) {
        mkkir_error(ctx, __LINE__, sym, "Invalid for-in statement");
        return new_inst_label(ctx->program, s->line, s->pos, l1, NULL, 0);
    }

    /* for break/continue */
    int l3 = get_next_label(ctx);
    int blabel = ctx->blabel;
    int clabel = ctx->clabel;
    ctx->blabel = l3;
    ctx->clabel = l1;
    if (s->sym) {
        s->sym->blabel = l3;
        s->sym->clabel = l1;
    }

    /* prologue */
    kl_kir_opr r1 = make_var_index(ctx, s->sym->index, s->sym->level, TK_TANY);
    int l2 = get_next_label(ctx);
    head = gen_expr(ctx, sym, &r1, s->e2);
    if (head) {
        last = get_last(head);
        last->next = new_inst_op1(ctx->program, s->line, s->pos, KIR_GETITER, &r1);
        last = last->next;
    } else {
        head = last = new_inst_op1(ctx->program, s->line, s->pos, KIR_GETITER, &r1);
    }
    set_file_func(ctx, sym, last);
    last->next = new_inst_jump(ctx->program, s->line, s->pos, l1, last);
    last = last->next;
    last->next = new_inst_label(ctx->program, s->line, s->pos, l2, last, 1);
    last = last->next;

    /* body */
    if (s->s1) {
        last->next = gen_stmt(ctx, sym, s->s1);
        last = get_last(last);
    }

    /* epilogue */
    last->next = new_inst_label(ctx->program, s->line, s->pos, l1, last, 0);
    last = last->next;

    last->next = gen_expr(ctx, sym, NULL, s->e1);
    last = get_last(last);

    last->next = new_inst_jumpifne(ctx->program, s->line, s->pos, &r1, l2);
    last = last->next;
    set_file_func(ctx, sym, last);

    last->next = new_inst_label(ctx->program, s->line, s->pos, l3, last, 0);

    ctx->clabel = clabel;
    ctx->blabel = blabel;
    return head;
}

static kl_kir_inst *gen_try(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_stmt *fincode = ctx->fincode;
    if (s->s3) {
        ctx->fincode = s->s3;
    }

    int flabel = -1;
    int tclabel = ctx->tclabel;
    if (s->s2 || s->s3) {
        ctx->tclabel = get_next_label(ctx);
        flabel = ctx->tclabel;
    }

    kl_kir_inst *head = gen_stmt(ctx, sym, s->s1);
    kl_kir_inst *last = get_last(head);
    if (s->s3) {
        ctx->in_finally = 1;
        last->next = gen_stmt(ctx, sym, s->s3);
        KIR_MOVE_LAST(last);
        ctx->in_finally = 0;
    }

    int nlabel = get_next_label(ctx);
    last->next = new_inst_jump(ctx->program, s->line, s->pos, nlabel, last);
    last = last->next;

    if (s->s2) {
        ctx->in_catch = 1;
        last->next = new_inst_label(ctx->program, s->line, s->pos, ctx->tclabel, last, 0);
        last = last->next;
        ctx->tclabel = tclabel;
        if (s->e1) {
            kl_expr *e = s->e1;
            kl_kir_opr rs = make_var_index(ctx, e->sym->index, e->sym->level, e->typeid);
            last->next = new_inst_op1(ctx->program, s->s2->line, s->s2->pos, KIR_CATCH, &rs);
            last = last->next;
        }
        last->next = gen_stmt(ctx, sym, s->s2);
        KIR_MOVE_LAST(last);
        ctx->in_catch = 0;
        if (last->opcode != KIR_JMP && last->opcode != KIR_THROW && last->opcode != KIR_THROWE && last->opcode != KIR_RET) {
            if (s->s3) {
                ctx->in_finally = 1;
                last->next = gen_stmt(ctx, sym, s->s3);
                KIR_MOVE_LAST(last);
                ctx->in_finally = 0;
            }
            last->next = new_inst_jump(ctx->program, s->line, s->pos, nlabel, last);
            last = last->next;
        }
    } else {
        ctx->tclabel = tclabel;
    }

    /* This is for if no catch clause */
    if (!s->s2 && s->s3) {
        last->next = new_inst_label(ctx->program, s->line, s->pos, flabel, last, 0);
        last = last->next;
        ctx->in_finally = 1;
        last->next = gen_stmt(ctx, sym, s->s3);
        KIR_MOVE_LAST(last);
        ctx->in_finally = 0;
        last->next = new_inst(ctx->program, 0, 0, KIR_THROW);
        last = last->next;
        set_file_func(ctx, sym, last);
        last->labelid = ctx->tclabel > 0 ? ctx->tclabel : sym->funcend;
    }

    last->next = new_inst_label(ctx->program, s->line, s->pos, nlabel, last, 0);
    ctx->fincode = fincode;
    return head;
}

static kl_kir_inst *gen_label(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;

    if (s->s1) {
        head = gen_stmt(ctx, sym, s->s1);
    }

    return head;
}

static kl_kir_inst *gen_yield(kl_context *ctx, kl_symbol *sym, kl_expr *e)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    kl_kir_opr r1 = make_ret_var(ctx, sym);
    if (e->rhs) {
        head = gen_expr(ctx, sym, &r1, e->rhs);
    } else {
        head = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, &r1, &r1);  /* make undefined */
    }
    kl_kir_inst *inst = new_inst(ctx->program, e->line, e->pos, KIR_YIELD);
    inst->labelid = e->yield;
    last = get_last(head);
    last->next = inst;
    last = inst;

    if (e->lhs) {
        kl_expr *r = e->lhs->rhs;
        kl_kir_opr rs = make_var_index(ctx, r->sym->index, 0, r->typeid);
        inst = new_inst_op1(ctx->program, e->line, e->pos, KIR_RESUME, &rs);
        set_file_func(ctx, sym, inst);
        last->next = inst;
        last = inst;
        kl_kir_opr r1 = make_ret_var(ctx, sym);
        last->next = gen_assign(ctx, sym, &r1, NULL, e->lhs);
    }
    return head;
}

static kl_kir_inst *gen_range(kl_context *ctx, kl_symbol *sym, kl_expr *e, kl_kir_opr *r1, int range)
{
    kl_kir_inst *head = NULL;
    kl_expr *l = e->lhs;
    kl_expr *r = e->rhs;
    if (!l && !r) {
        /* TODO: Error */
        return NULL;
    }
    if (l && !r) {
        if (l->nodetype == TK_VSINT) {
            kl_kir_opr r2 = make_lit_i64(ctx, l->val.i64);
            kl_kir_opr r3 = make_lit_undef(ctx);
            head = new_inst_op3(ctx->program, e->line, e->pos, range == 2 ? KIR_RANGEF : KIR_RANGET, r1, &r2, &r3);
            set_file_func(ctx, sym, head);
        }
    } else if (!l && r) {
        if (r->nodetype == TK_VSINT) {
            kl_kir_opr r2 = make_lit_undef(ctx);
            kl_kir_opr r3 = make_lit_i64(ctx, r->val.i64);
            head = new_inst_op3(ctx->program, e->line, e->pos, range == 2 ? KIR_RANGEF : KIR_RANGET, r1, &r2, &r3);
            set_file_func(ctx, sym, head);
        }
    } else if (l->nodetype == TK_VSINT && r->nodetype == TK_VSINT) {
        kl_kir_opr r2 = make_lit_i64(ctx, l->val.i64);
        kl_kir_opr r3 = make_lit_i64(ctx, r->val.i64);
        head = new_inst_op3(ctx->program, e->line, e->pos, range == 2 ? KIR_RANGEF : KIR_RANGET, r1, &r2, &r3);
        set_file_func(ctx, sym, head);
    }

    if (!head) {
        kl_kir_opr r2 = {0};
        kl_kir_inst *r2i = NULL;
        if (!l) {
            r2 = make_lit_undef(ctx);
        } else {
            KL_KIR_CHECK_LITERAL(l, r2, r2i)
        }
        kl_kir_opr r3 = {0};
        kl_kir_inst *r3i = NULL;
        if (!r) {
            r3 = make_lit_undef(ctx);
        } else {
            KL_KIR_CHECK_LITERAL(r, r3, r3i)
        }
        head = new_inst_op3(ctx->program, e->line, e->pos, range == 2 ? KIR_RANGEF : KIR_RANGET, r1, &r2, &r3);
        set_file_func(ctx, sym, head);
        if (r3i) {
            kl_kir_inst *r3l = get_last(r3i);
            r3l->next = head;
            head = r3i;
        }
        if (r2i) {
            kl_kir_inst *r2l = get_last(r2i);
            r2l->next = head;
            head = r2i;
        }
    }

    return head;
}

static kl_kir_inst *gen_expr(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_inst *head = NULL;
    if (!e) return head;

    kl_kir_opr rr = {0};
    if (!r1) {
        rr = make_ret_var(ctx, sym);
        r1 = &rr;
    }

    switch (e->nodetype) {
    case TK_VBOOL: {
        kl_kir_opr rs = make_lit_bool(ctx, e->val.i64);
        head = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &rs);
        break;
    }
    case TK_VSINT: {
        kl_kir_opr rs = make_lit_i64(ctx, e->val.i64);
        head = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &rs);
        break;
    }
    case TK_VDBL: {
        kl_kir_opr rs = make_lit_dbl(ctx, e->val.dbl);
        head = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &rs);
        break;
    }
    case TK_VSTR: {
        kl_kir_opr rs = make_lit_str(ctx, e->val.str);
        head = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &rs);
        break;
    }
    case TK_VBIN: {
        head = new_inst_op1(ctx->program, e->line, e->pos, KIR_NEWBIN, r1);
        if (e->lhs) {
            head->next = gen_binary_array_literal(ctx, sym, r1, e->lhs);
        }
        break;
    }
    case TK_VARY: {
        head = new_inst_op1(ctx->program, e->line, e->pos, KIR_NEWOBJ, r1);
        if (e->lhs) {
            head->next = gen_binary_array_literal(ctx, sym, r1, e->lhs);
        }
        break;
    }
    case TK_VOBJ: {
        head = new_inst_op1(ctx->program, e->line, e->pos, KIR_NEWOBJ, r1);
        if (e->lhs) {
            kl_kir_opr r2 = make_var(ctx, sym, TK_TANY);
            head->next = gen_object_literal(ctx, sym, r1, &r2, e->lhs);
        }
        break;
    }
    case TK_VREGEX: {
        kl_kir_opr rs = make_lit_str(ctx, e->val.str);
        kl_kir_opr rf = make_lit_str(ctx, e->strx);
        head = new_inst_op3(ctx->program, e->line, e->pos, KIR_NEWREGEX, r1, &rs, &rf);
        set_file_func(ctx, sym, head);
        break;
    }
    case TK_VAR: {
        if (!r1->prevent) {
            check_autoset_flag(ctx, e->sym);
            kl_kir_opr rs = make_var_index(ctx, e->sym->index, e->sym->level, e->typeid);
            head = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &rs);
        }
        break;
    }

    case TK_FUNC: {
        kl_stmt *s = e->s;
        if (s && s->nodetype == TK_NAMESPACE) {
            kl_symbol *f = s->sym;
            kl_kir_opr rx = {0};
            head = gen_stmt(ctx, sym, s);
            if (!r1) {
                rx = make_var(ctx, sym, TK_TANY);
                r1 = &rx;
            }
            kl_kir_opr r2 = make_lit_func(ctx, f);
            if (!head) {
                head = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &r2);
            } else {
                kl_kir_inst *last = get_last(head);
                last->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &r2);
            }
        } else {
            kl_symbol *f = e->sym;
            kl_kir_func *func = gen_function(ctx, f, e->s, NULL);
            add_func(ctx->program, func);
            kl_kir_opr r2 = make_lit_func(ctx, f);
            head = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &r2);
        }
        break;
    }

    case TK_CALL:
        head = gen_call(ctx, sym, r1, e);
        break;

    case TK_BNOT:
        head = gen_bnot(ctx, sym, r1, e);
        break;
    case TK_NOT:
        head = gen_not(ctx, sym, r1, e);
        break;
    case TK_EQ:
        head = gen_assign(ctx, sym, r1, NULL, e);
        break;

    case TK_ADDEQ:
        head = gen_xassign(ctx, sym, KIR_ADD, r1, e);
        break;
    case TK_SUBEQ:
        head = gen_xassign(ctx, sym, KIR_SUB, r1, e);
        break;
    case TK_MULEQ:
        head = gen_xassign(ctx, sym, KIR_MUL, r1, e);
        break;
    case TK_DIVEQ:
        head = gen_xassign(ctx, sym, KIR_DIV, r1, e);
        break;
    case TK_MODEQ:
        head = gen_xassign(ctx, sym, KIR_MOD, r1, e);
        break;
    case TK_ANDEQ:
        head = gen_xassign(ctx, sym, KIR_BAND, r1, e);
        break;
    case TK_OREQ:
        head = gen_xassign(ctx, sym, KIR_BOR, r1, e);
        break;
    case TK_XOREQ:
        head = gen_xassign(ctx, sym, KIR_BXOR, r1, e);
        break;
    case TK_POWEQ:
        head = gen_xassign(ctx, sym, KIR_POW, r1, e);
        break;
    case TK_LSHEQ:
        head = gen_xassign(ctx, sym, KIR_BSHL, r1, e);
        break;
    case TK_RSHEQ:
        head = gen_xassign(ctx, sym, KIR_BSHR, r1, e);
        break;
    case TK_LANDEQ:
        head = gen_land_lor_assign(ctx, sym, e, 1);
        break;
    case TK_LOREQ:
        head = gen_land_lor_assign(ctx, sym, e, 0);
        break;
    case TK_NULLCEQ:
        head = gen_nullc_assign(ctx, sym, e);
        break;
    case TK_REGEQ:
    case TK_REGNE:
        head = gen_regmatch(ctx, sym, r1, e, e->nodetype);
        break;
    case TK_EQEQ:
        head = gen_op3_inst(ctx, sym, KIR_EQEQ, r1, e);
        break;
    case TK_NEQ:
        head = gen_op3_inst(ctx, sym, KIR_NEQ, r1, e);
        break;
    case TK_LT:
        head = gen_op3_inst(ctx, sym, KIR_LT, r1, e);
        break;
    case TK_LE:
        head = gen_op3_inst(ctx, sym, KIR_LE, r1, e);
        break;
    case TK_GT:
        head = gen_op3_inst(ctx, sym, KIR_GT, r1, e);
        break;
    case TK_GE:
        head = gen_op3_inst(ctx, sym, KIR_GE, r1, e);
        break;
    case TK_LGE:
        head = gen_op3_inst(ctx, sym, KIR_LGE, r1, e);
        break;
    case TK_ADD:
        head = gen_op3_inst(ctx, sym, KIR_ADD, r1, e);
        break;
    case TK_SUB:
        head = gen_op3_inst(ctx, sym, KIR_SUB, r1, e);
        break;
    case TK_MUL:
        head = gen_op3_inst(ctx, sym, KIR_MUL, r1, e);
        break;
    case TK_DIV:
        head = gen_op3_inst(ctx, sym, KIR_DIV, r1, e);
        break;
    case TK_MOD:
        head = gen_op3_inst(ctx, sym, KIR_MOD, r1, e);
        break;
    case TK_AND:
        head = gen_op3_inst(ctx, sym, KIR_BAND, r1, e);
        break;
    case TK_OR:
        head = gen_op3_inst(ctx, sym, KIR_BOR, r1, e);
        break;
    case TK_XOR:
        head = gen_op3_inst(ctx, sym, KIR_BXOR, r1, e);
        break;
    case TK_QES: {
        int l1 = get_next_label(ctx);
        head = gen_ternary_expr(ctx, sym, r1, e, l1);
        kl_kir_inst *last = get_last(head);
        if (last) {
            last->next = new_inst_label(ctx->program, e->line, e->pos, l1, last, 0);
        }
        break;
    }
    case TK_POW:
        head = gen_op3_inst(ctx, sym, KIR_POW, r1, e);
        break;
    case TK_LSH:
        head = gen_op3_inst(ctx, sym, KIR_BSHL, r1, e);
        break;
    case TK_RSH:
        head = gen_op3_inst(ctx, sym, KIR_BSHR, r1, e);
        break;
    case TK_LAND: {
        kl_kir_opr r2 = make_var(ctx, sym, TK_TANY);
        int l1 = get_next_label(ctx);
        head = gen_logical_and(ctx, sym, &r2, e, l1);
        kl_kir_inst *last = get_last(head);
        if (last) {
            last->next = new_inst_label(ctx->program, e->line, e->pos, l1, last, 0);
            last = last->next;
            last->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &r2);
        }
        break;
    }
    case TK_LOR: {
        kl_kir_opr r2 = make_var(ctx, sym, TK_TANY);
        int l1 = get_next_label(ctx);
        head = gen_logical_or(ctx, sym, &r2, e, l1);
        kl_kir_inst *last = get_last(head);
        if (last) {
            last->next = new_inst_label(ctx->program, e->line, e->pos, l1, last, 0);
            last = last->next;
            last->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &r2);
        }
        break;
    }
    case TK_NULLC: {
        kl_kir_opr r2 = make_var(ctx, sym, TK_TANY);
        kl_kir_opr r4 = make_var(ctx, sym, TK_TANY);
        int l1 = get_next_label(ctx);
        head = gen_null_coalescing(ctx, sym, &r2, &r4, e, l1);
        kl_kir_inst *last = get_last(head);
        if (last) {
            last->next = new_inst_label(ctx->program, e->line, e->pos, l1, last, 0);
            last = last->next;
            last->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_MOV, r1, &r2);
        }
        break;
    }
    case TK_IDX:
        head = gen_apply(ctx, sym, r1, e);
        break;
    case TK_DOT:
        head = gen_apply_str(ctx, sym, r1, e);
        break;
    case TK_COMMA: {
        kl_kir_opr rr = make_ret_var(ctx, sym);
        rr.prevent = 1;
        head = gen_expr(ctx, sym, &rr, e->lhs);
        kl_kir_inst *last = get_last(head);
        kl_kir_inst *next = gen_expr(ctx, sym, r1, e->rhs);
        if (last) {
            last->next = next;
        } else {
            head = next;
        }
        break;
    }
    case TK_INC:
        head = gen_incdec(ctx, sym, KIR_INC, r1, e);
        break;
    case TK_INCP:
        head = gen_incdec(ctx, sym, KIR_INCP, r1, e);
        break;
    case TK_DEC:
        head = gen_incdec(ctx, sym, KIR_DEC, r1, e);
        break;
    case TK_DECP:
        head = gen_incdec(ctx, sym, KIR_DECP, r1, e);
        break;

    case TK_YIELD:
        head = gen_yield(ctx, sym, e);
        break;

    case TK_MINUS: {
        kl_kir_opr rr = make_var(ctx, sym, TK_TANY);
        head = gen_expr(ctx, sym, &rr, e->lhs);
        kl_kir_inst *last = get_last(head);
        last->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_MINUS, r1, &rr);
        set_file_func(ctx, sym, last->next);
        break;
    }

    case TK_CONV: {
        kl_kir_opr rr = make_var(ctx, sym, TK_TANY);
        head = gen_expr(ctx, sym, &rr, e->lhs);
        kl_kir_inst *last = get_last(head);
        last->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_CONV, r1, &rr);
        set_file_func(ctx, sym, last->next);
        break;
    }

    case TK_RANGE2: {
        head = gen_range(ctx, sym, e, r1, 2);
        break;
    }
    case TK_RANGE3: {
        head = gen_range(ctx, sym, e, r1, 3);
        break;
    }

    case TK_ARYSIZE: {
        kl_expr *l = e->lhs;
        kl_kir_opr rn = make_var_index(ctx, l->sym->ref ? l->sym->ref->index : l->sym->index, l->sym->level, l->typeid);
        head = new_inst_op2(ctx->program, e->line, e->pos, KIR_ARYSIZE, r1, &rn);
        break;
    }
    default:
        ;
    }

    return head;
}

static kl_kir_inst *gen_block(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;

    kl_symbol *label = s->sym;
    int blabel;
    if (label) {
        blabel = get_next_label(ctx);
        label->blabel = blabel;
    }

    while (s) {
        kl_kir_inst *next = gen_stmt(ctx, sym, s);
        if (next) {
            if (!head) {
                head = next;
            } else {
                last->next = next;
            }
            last = next;
            KIR_MOVE_LAST(last);
        }
        s = s->next;
    }

    if (label) {
        last->next = new_inst_label(ctx->program, sym->line, sym->pos, blabel, last, 0);
    }

    return head;
}

static kl_kir_inst *gen_setargs(kl_context *ctx, kl_symbol *sym, kl_expr *e, int *idx)
{
    if (!e) {
        return NULL;
    }
    kl_kir_inst *head = NULL;
    if (e->nodetype == TK_COMMA) {
        head = gen_setargs(ctx, sym, e->lhs, idx);
        if (!head) {
            head = gen_setargs(ctx, sym, e->rhs, idx);
        } else {
            kl_kir_inst *last = get_last(head);
            last->next = gen_setargs(ctx, sym, e->rhs, idx);
        }
    } else {
        if (e->sym) {
            int index = e->sym->index;
            kl_kir_opr rn = make_lit_i64(ctx, index);
            kl_kir_opr ri = make_lit_i64(ctx, *idx);
            ri.typestr = translate_vartype(e->sym->typeid);
            head = new_inst_op2(ctx->program, e->line, e->pos, e->sym->is_dot3 ? KIR_SETARGL : KIR_SETARG, &rn, &ri);
        }
        ++*idx;
    }
    return head;
}

static kl_kir_inst *gen_args(kl_context *ctx, kl_symbol *sym, kl_expr *e)
{
    if (!e) {
        return NULL;
    }
    kl_kir_inst *head = NULL;
    if (e->nodetype == TK_COMMA) {
        head = gen_args(ctx, sym, e->lhs);
        if (!head) {
            head = gen_args(ctx, sym, e->rhs);
        } else {
            kl_kir_inst *last = get_last(head);
            last->next = gen_args(ctx, sym, e->rhs);
        }
    } else {
        if (e->s) {
            head = gen_stmt(ctx, sym, e->s);
        }
    }
    return head;
}

static kl_kir_func *gen_function(kl_context *ctx, kl_symbol *sym, kl_stmt *s, kl_symbol *initer)
{
    // adjustment of arguments count.
    if (sym->idxmax < sym->placemax) {
        sym->idxmax = sym->placemax;
    }
    if (sym->argcount < sym->placemax) {
        sym->argcount = sym->placemax;
    }

    int line = s ? s->line : 0;
    int pos = s ? s->pos : 0;
    kl_kir_inst *last = NULL;
    kl_kir_func *func = new_func(ctx, sym->line, sym->pos, sym->name);
    func->has_frame = sym->has_func;
    int localvars = sym->idxmax;
    func->funcend = sym->funcend = get_next_label(ctx);
    int tclabel = ctx->tclabel;
    ctx->tclabel = func->funcend;
    if (func->has_frame) {
        func->head = last = new_inst(ctx->program, sym->line, sym->pos, KIR_MKFRM);
    } else {
        func->head = last = new_inst(ctx->program, sym->line, sym->pos, KIR_ALOCAL);
    }
    set_file_func(ctx, sym, last);

    if (!s) {
        kl_kir_opr r1 = make_ret_var(ctx, sym);
        last->next = new_inst_op2(ctx->program, line, pos, KIR_MOV, &r1, &r1);  /* make undefined */
        last = last->next;
        localvars = sym->count = sym->argcount = 0; /* all variables won't be used. */
    } else {
        int idx = 0;
        kl_kir_inst *setargs = gen_setargs(ctx, sym, sym->args, &idx);
        if (setargs) {
            last->next = setargs;
            last = get_last(last);
        }
        if (idx < sym->placemax) {
            for (int i = idx; i < sym->placemax; ++i) {
                kl_kir_opr rn = make_lit_i64(ctx, i);
                kl_kir_opr ri = make_lit_i64(ctx, i);
                last->next = new_inst_op2(ctx->program, line, pos, KIR_SETARG, &rn, &ri);
                last = last->next;
            }
        }
        kl_kir_inst *args = gen_args(ctx, sym, sym->args);
        if (args) {
            last->next = args;
            last = get_last(last);
        }
        last->next = new_inst(ctx->program, sym->line, sym->pos, KIR_PURE);
        last = last->next;
        set_file_func(ctx, sym, last);
        last->labelid = sym->yield;

        kl_stmt *sh = s;
        while (sh) {
            if (initer && sh->nodetype == TK_RETURN) {
                int callcnt = sym->callcnt++;
                kl_kir_opr rc = { .t = TK_VSINT, .i64 = callcnt, .typeid = TK_TSINT64 };
                kl_kir_opr rr = make_ret_var(ctx, sym);
                kl_kir_opr r1 = make_var_index(ctx, initer->index, initer->level, TK_TFUNC);
                r1.callcnt = callcnt;
                last->next = new_inst_op1(ctx->program, line, pos, KIR_SVSTKP, &rc);
                last = last->next;
                last->next = new_inst_op2(ctx->program, line, pos, KIR_CALL, &rr, &r1);
                last = last->next;
                set_file_func(ctx, sym, last);
                last->next = new_inst_op1(ctx->program, line, pos, KIR_RSSTKP, &rc);
                last = last->next;
            }
            kl_kir_inst *next = gen_stmt(ctx, sym, sh);
            if (next) {
                last->next = next;
                last = next;
                KIR_MOVE_LAST(last);
            }
            sh = sh->next;
        }
    }

    func->funcname = sym->funcname ? sym->funcname : sym->name;
    func->funcid = sym->funcid;
    func->has_dot3 = sym->is_dot3;
    func->argcount = sym->argcount;
    func->yield = sym->yield;
    func->head->r1 = (kl_kir_opr){ .t = TK_VSINT, .i64 = sym->count, .typeid = TK_TSINT64 };
    func->head->r2 = (kl_kir_opr){ .t = TK_VSINT, .i64 = localvars, .typeid = TK_TSINT64 };
    func->head->r3 = (kl_kir_opr){ .t = TK_VSINT, .i64 = sym->argcount, .typeid = TK_TSINT64 };

    last->next = new_inst_label(ctx->program, sym->line, sym->pos, sym->funcend, last, 0);
    last = last->next;

    kl_kir_inst *out = NULL;
    if (func->has_frame) {
        out = new_inst(ctx->program, sym->line, sym->pos, KIR_POPFRM);
    } else {
        out = new_inst(ctx->program, sym->line, sym->pos, KIR_RLOCAL);
    }
    last->next = out;
    out->next = new_inst_ret(ctx->program, sym->line, sym->pos);

    ctx->tclabel = tclabel;
    return func;
}

static kl_kir_func *gen_class(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    return gen_function(ctx, sym, s, sym->initer);
}

static kl_kir_func *gen_namespace(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    if (!s) return NULL;
    kl_kir_func *func = new_func(ctx, sym->line, sym->pos, sym->name);
    kl_kir_inst *last = NULL;

    int localvars = sym->idxmax;
    func->has_frame = 1;
    func->is_global = sym->is_global;
    func->funcend = sym->funcend = get_next_label(ctx);
    int tclabel = ctx->tclabel;
    ctx->tclabel = func->funcend;
    func->head = last = new_inst(ctx->program, sym->line, sym->pos, KIR_MKFRM);
    set_file_func(ctx, sym, last);

    if (!sym->is_global) {
        last->next = new_inst(ctx->program, sym->line, sym->pos, KIR_PURE);
        last = last->next;
        set_file_func(ctx, sym, last);
        last->labelid = sym->yield;
    }

    while (s) {
        kl_kir_inst *next = gen_stmt(ctx, sym, s);
        if (next) {
            if (!last) {
                func->head = next;
            } else {
                last->next = next;
            }
            last = next;
            KIR_MOVE_LAST(last);
        }
        s = s->next;
    }

    func->funcname = sym->funcname ? sym->funcname : sym->name;
    func->funcid = sym->funcid;
    func->head->r1 = (kl_kir_opr){ .t = TK_VSINT, .i64 = sym->count, .typeid = TK_TSINT64 };
    func->head->r2 = (kl_kir_opr){ .t = TK_VSINT, .i64 = localvars, .typeid = TK_TSINT64 };
    func->head->r3 = (kl_kir_opr){ .t = TK_VSINT, .i64 = sym->argcount, .typeid = TK_TSINT64 };

    last->next = new_inst_label(ctx->program, sym->line, sym->pos, sym->funcend, last, 0);
    last = last->next;
    kl_kir_inst *out = new_inst(ctx->program, sym->line, sym->pos, KIR_POPFRM);
    last->next = out;
    out->next = new_inst_ret(ctx->program, sym->line, sym->pos);

    ctx->tclabel = tclabel;
    return func;
}

static kl_kir_inst *gen_mixin(kl_context *ctx, kl_symbol *sym, kl_expr *e, kl_kir_opr *rx)
{
    kl_kir_inst *head = NULL;
    if (e->nodetype == TK_COMMA) {
        head = gen_mixin(ctx, sym, e->lhs, rx);
        if (!head) {
            head = gen_mixin(ctx, sym, e->rhs, rx);
        } else {
            kl_kir_inst *last = get_last(head);
            last->next = gen_mixin(ctx, sym, e->rhs, rx);
        }
    } else {
        kl_kir_opr rm = make_var_index(ctx, e->sym->ref ? e->sym->ref->index : e->sym->index, e->sym->level, e->typeid);
        kl_kir_opr rs = make_lit_str(ctx, "extend");
        kl_kir_opr rr = make_ret_var(ctx, sym);
        kl_kir_inst *last;
        head = last = new_inst_op3(ctx->program, e->line, e->pos, KIR_APLY, rx, &rm, &rs);

        int callcnt = sym->callcnt++;
        kl_kir_opr rc = { .t = TK_VSINT, .i64 = callcnt, .typeid = TK_TSINT64 };
        last->next = new_inst_op1(ctx->program, e->line, e->pos, KIR_SVSTKP, &rc);
        last = last->next;

        kl_kir_opr rt = make_var_index(ctx, sym->thisobj->index, sym->thisobj->level, TK_TANY);
        last->next = new_inst_op1(ctx->program, e->line, e->pos, KIR_PUSHARG, &rt);
        last = last->next;
        set_file_func(ctx, sym, last);

        rx->callcnt = callcnt;
        rx->args = 1;
        last->next = new_inst_op2(ctx->program, e->line, e->pos, KIR_CALL, &rr, rx);
        last = last->next;
        set_file_func(ctx, sym, last);
        last->next = new_inst_op1(ctx->program, e->line, e->pos, KIR_RSSTKP, &rc);
        last = last->next;
        last->next = new_inst_chkexcept(ctx->program, e->line, e->pos, sym->funcend);
        set_file_func(ctx, sym, last->next);
    }
    return head;
}

static kl_kir_inst *gen_stmt(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    if (!s) return head;

    int idxmax = sym ? sym->idxmax : 0;
    switch (s->nodetype) {
    case TK_BLOCK:
        if (s->s1) {
            head = gen_block(ctx, sym, s->s1);
        }
        break;

    case TK_NAMESPACE:
        if (s->s1) {
            kl_kir_func *func = gen_namespace(ctx, s->sym, s->s1);
            add_func(ctx->program, func);
        }
        break;

    case TK_MIXIN:
        if (s->e1) {
            kl_kir_opr rr = make_var(ctx, sym, TK_TANY);
            head = gen_mixin(ctx, sym, s->e1, &rr);
        }
        break;

    case TK_FUNC: {
        kl_symbol *f = s->sym;
        kl_kir_func *func = gen_function(ctx, f, s->s1, NULL);
        if (s->s1 && !func->has_dot3 && (1 <= func->argcount && func->argcount < 5)) {
            if ((ctx->options & PARSER_OPT_DISABLE_PURE) == 0) {
                func->is_pure = check_pure_function(ctx, s->s1);
            }
        }
        add_func(ctx->program, func);
        kl_kir_opr r1 = make_var_index(ctx, f->ref ? f->ref->index : f->index, f->level, TK_TFUNC);
        kl_kir_opr r2 = make_lit_func(ctx, s->sym);
        head = new_inst_op2(ctx->program, s->line, s->pos, KIR_MOV, &r1, &r2);
        break;
    }

    case TK_CLASS: {
        kl_kir_opr rr = make_var(ctx, sym, TK_TANY);
        kl_symbol *f = s->sym;
        kl_kir_func *func = gen_class(ctx, f, s->s1);
        add_func(ctx->program, func);
        kl_kir_opr r1 = make_var_index(ctx, f->ref ? f->ref->index : f->index, f->level, TK_TFUNC);
        kl_kir_opr r2 = make_lit_str(ctx, "create");
        head = new_inst_op3(ctx->program, s->line, s->pos, KIR_APLYL, &rr, &r1, &r2);
        kl_kir_opr r3 = make_lit_func(ctx, s->sym);
        head->next = new_inst_op2(ctx->program, s->line, s->pos, KIR_MOVA, &rr, &r3);
        set_file_func(ctx, sym, head->next);
        break;
    }

    case TK_MODULE: {
        kl_kir_opr rr = make_var(ctx, sym, TK_TANY);
        kl_symbol *f = s->sym;
        kl_kir_func *func = gen_class(ctx, f, s->s1);
        add_func(ctx->program, func);
        kl_kir_opr r1 = make_var_index(ctx, f->ref ? f->ref->index : f->index, f->level, TK_TFUNC);
        kl_kir_opr r2 = make_lit_str(ctx, "extend");
        head = new_inst_op3(ctx->program, s->line, s->pos, KIR_APLYL, &rr, &r1, &r2);
        kl_kir_opr r3 = make_lit_func(ctx, s->sym);
        head->next = new_inst_op2(ctx->program, s->line, s->pos, KIR_MOVA, &rr, &r3);
        set_file_func(ctx, sym, head->next);
        break;
    }

    case TK_SWITCH:
        if (s->e1 && s->s1) {
            head = gen_switch(ctx, sym, s);
        }
        break;
    case TK_CASE:
        head = gen_case(ctx, sym, s);
        break;
    case TK_WHEN:
        head = gen_when(ctx, sym, s);
        break;
    case TK_DEFAULT:
        head = gen_default(ctx, sym, s);
        break;
    case TK_OTHERWISE:
        head = gen_otherwise(ctx, sym, s);
        break;
    case TK_IF:
        if (s->e1) {
            head = gen_if(ctx, sym, s);
        }
        break;
    case TK_DO:
        if (s->e1) {
            head = gen_while(ctx, sym, s, 1);
        }
        break;
    case TK_WHILE:
        if (s->e1) {
            head = gen_while(ctx, sym, s, 0);
        }
        break;
    case TK_FOR:
        head = gen_for(ctx, sym, s);
        break;
    case TK_FORIN:
        head = gen_forin(ctx, sym, s);
        break;
    case TK_TRY:
        head = gen_try(ctx, sym, s);
        break;
    case TK_LABEL:
        head = gen_label(ctx, sym, s);
        break;
    case TK_FALLTHROUGH:
        ctx->whenstmt = NULL;
        break;
    case TK_BREAK:
        if (s->sym) {
            if (!s->sym->blabel) {
                mkkir_error(ctx, __LINE__, s->sym, "Label destination not found");
            }
            head = new_inst_jump(ctx->program, s->line, s->pos, s->sym->blabel, NULL);
        } else {
            head = new_inst_jump(ctx->program, s->line, s->pos, ctx->blabel, NULL);
        }
        break;
    case TK_CONTINUE:
        if (s->sym) {
            if (!s->sym->clabel) {
                mkkir_error(ctx, __LINE__, s->sym, "Label destination not found");
            }
            head = new_inst_jump(ctx->program, s->line, s->pos, s->sym->clabel, NULL);
        } else {
            head = new_inst_jump(ctx->program, s->line, s->pos, ctx->clabel, NULL);
        }
        break;
    case TK_RETURN:
        head = gen_ret(ctx, sym, s);
        break;
    case TK_THROW:
        head = gen_throw(ctx, sym, s);
        break;
    case TK_EXTERN: {
        kl_expr *e2 = s->e2;
        kl_symbol *sym = e2 ? e2->sym : NULL;
        if (sym) {
            kl_kir_opr r1 = make_var_index(ctx, sym->index, sym->level, s->typeid);
            kl_kir_opr r2 = make_lit_str(ctx, s->e1->val.str);
            head = new_inst_op2(ctx->program, sym->line, sym->pos, KIR_EXTERN, &r1, &r2);
        }
        break;
    }
    case TK_ENUM:
    case TK_CONST:
    case TK_LET:
        if (s->e1) {
            kl_kir_opr rr = make_ret_var(ctx, sym);
            rr.prevent = 1;
            head = gen_expr(ctx, sym, &rr, s->e1);
        }
        break;
    case TK_EXPR:
        if (s->e1) {
            head = gen_expr(ctx, sym, NULL, s->e1);
        }
        break;

    case TK_MKSUPER: {
        kl_expr *e1 = s->e1;
        kl_symbol *super = e1 ? e1->sym : NULL;
        kl_expr *e2 = s->e2;
        kl_symbol *thisobj = e2 ? e2->sym : NULL;
        if (super && thisobj) {
            kl_kir_opr r1 = make_var_index(ctx, super->index, super->level, TK_TANY);
            kl_kir_opr r2 = make_var_index(ctx, thisobj->index, thisobj->level, TK_TANY);
            head = new_inst_op2(ctx->program, s->line, s->pos, KIR_MKSUPER, &r1, &r2);
        }
        break;
    }
    default:
        ;
    }

    if (sym) {
        if (sym->count < sym->idxmax) {
            sym->count = sym->idxmax;
        }
        sym->idxmax = idxmax;
    }
    return head;
}

static void gen_program(kl_context *ctx, kl_kir_program *p, kl_stmt *s)
{
    if (s->nodetype != TK_NAMESPACE || !s->s1) {
        return;
    }
    kl_kir_func *func = gen_namespace(ctx, s->sym, s->s1);
    add_func(ctx->program, func);
}

int make_kir(kl_context *ctx)
{
    ctx->program = (kl_kir_program*)calloc(1, sizeof(kl_kir_program));
    gen_program(ctx, ctx->program, ctx->head);
    return ctx->errors;
}
