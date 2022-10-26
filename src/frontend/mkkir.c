#include "../kir.h"
#include "mkkir.h"

// printf("%s:%d -> %s\n", __FILE__, __LINE__, __func__);
#define KIR_ADD(last, next) if (next) (last->next = next, last = next)
#define KIR_MOVE_LAST(last) while (last->next) last = last->next

static kl_kir_inst *gen_block(kl_context *ctx, kl_symbol *sym, kl_stmt *s);
static kl_kir_inst *gen_expr(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e);
static kl_kir_inst *gen_stmt(kl_context *ctx, kl_symbol *sym, kl_stmt *s);

static char *mkkir_const_str(kl_context *ctx, const char *str)
{
    return const_str(ctx, "GenKIR", 0, 0, 0, str);
}

static inline int get_next_label(kl_context *ctx)
{
    return ctx->labelid++;
}

static kl_kir_inst *new_inst(kl_kir op)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->opcode = op;
    return inst;
}

static kl_kir_inst *new_inst_label(int labelid)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->opcode = KIR_LABEL;
    inst->labelid = labelid;
    return inst;
}

static kl_kir_inst *new_inst_jumpiff(kl_kir_opr *r1, int labelid)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->opcode = KIR_JMPIFF;
    inst->r1 = *r1;
    inst->labelid = labelid;
    return inst;
}

static kl_kir_inst *new_inst_jump(int labelid, kl_kir_inst *last)
{
    if (last->opcode == KIR_RET) {
        return NULL;
    }
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->opcode = KIR_JMP;
    inst->labelid = labelid;
    return inst;
}

static kl_kir_inst *new_inst_ret(kl_kir_opr *r1)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->opcode = KIR_RET;
    inst->r1 = *r1;
    return inst;
}

static kl_kir_inst *new_inst_op1(kl_kir op, kl_kir_opr *r1)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->opcode = op;
    inst->r1 = *r1;
    return inst;
}

static kl_kir_inst *new_inst_op2(kl_kir op, kl_kir_opr *r1, kl_kir_opr *r2)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->opcode = op;
    inst->r1 = *r1;
    inst->r2 = *r2;
    return inst;
}

static kl_kir_inst *new_inst_op3(kl_kir op, kl_kir_opr *r1, kl_kir_opr *r2, kl_kir_opr *r3)
{
    kl_kir_inst *inst = (kl_kir_inst *)calloc(1, sizeof(kl_kir_inst));
    inst->opcode = op;
    inst->r1 = *r1;
    inst->r2 = *r2;
    inst->r3 = *r3;
    return inst;
}

static kl_kir_func *new_func(kl_context *ctx, const char *name)
{
    kl_kir_func *func = (kl_kir_func *)calloc(1, sizeof(kl_kir_func));
    func->name = mkkir_const_str(ctx, name);
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
        (rn).t = TK_VSINT; \
        (rn).i64 = (e)->val.i64; \
        (rn).typeid = TK_TSINT64; \
        (rn).typestr = mkkir_const_str(ctx, typeidname(TK_TSINT64)); \
        break; \
    case TK_VUINT: \
        (rn).t = TK_VUINT; \
        (rn).u64 = (e)->val.u64; \
        (rn).typeid = TK_TUINT64; \
        (rn).typestr = mkkir_const_str(ctx, typeidname(TK_TUINT64)); \
        break; \
    case TK_VAR: \
        (rn).t = TK_VAR; \
        (rn).index = (e)->sym->ref ? (e)->sym->ref->index : (e)->sym->index; \
        (rn).level = (e)->sym->level; \
        (rn).typeid = (e)->typeid; \
        (rn).typestr = mkkir_const_str(ctx, typeidname((e)->typeid)); \
        break; \
    default: \
        if ((rn).index == 0) (rn) = make_var(ctx, sym); \
        (rni) = gen_expr(ctx, sym, &(rn), e); \
        break; \
    } \
/**/

static kl_kir_inst *get_last(kl_kir_inst *i)
{
    if (i) {
        while (i->next) {
            i = i->next;
        }
    }
    return i;
}

static kl_kir_opr make_var(kl_context *ctx, kl_symbol *sym)
{
    kl_kir_opr r1 = {0};
    r1.t = TK_VAR;
    r1.index = sym->idxmax++;   /* TODO: index could be reusable */
    r1.level = 0;
    r1.typeid = TK_VAR;
    return r1;
}

static kl_kir_inst *gen_op3_inst(kl_context *ctx, kl_symbol *sym, kl_kir op, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_inst *r2i = NULL;
    kl_kir_inst *r3i = NULL;
    kl_kir_opr r2 = {0};
    kl_kir_opr r3 = {0};
    kl_expr *l = e->lhs;
    kl_expr *r = e->rhs;

    KL_KIR_CHECK_LITERAL(l, r2, r2i);
    KL_KIR_CHECK_LITERAL(r, r3, r3i);
    kl_kir_inst *r2l = get_last(r2i);
    kl_kir_inst *r3l = get_last(r3i);

    kl_kir_inst *inst = new_inst_op3(op, r1, &r2, &r3);
    if (r2i) {
        if (r3i) {
            r2l->next = r3i;
            r3l->next = inst;
            inst = r2i;
        } else {
            r2l->next = inst;
            inst = r2i;
        }
    } else if (r3i) {
        r3l->next = inst;
        inst = r3i;
    }

    return inst;
}

static kl_kir_inst *gen_callargs(kl_context *ctx, kl_symbol *sym, kl_expr *e)
{
    if (!e) {
        return NULL;
    }

    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    if (e->nodetype == TK_COMMA) {        
        if (e->lhs) {
            head = last = gen_callargs(ctx, sym, e->lhs);
            if (last) {
                if (e->rhs) {
                    kl_kir_inst *next = gen_callargs(ctx, sym, e->lhs);
                    KIR_ADD(last, next);
                }
            }
        }
    } else {
        kl_kir_opr r = make_var(ctx, sym);
        head = last = gen_expr(ctx, sym, &r, e);
        KIR_MOVE_LAST(last);
        last->next = new_inst_op1(KIR_PUSHARG, &(last->r1));
        if (last->next) {
            last = last->next;
        }
    }
    return head;
}

static kl_kir_inst *gen_call(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_opr r2 = {0};
    kl_kir_inst *r1i = NULL;
    kl_kir_inst *r1l = NULL;

    kl_expr *f = e->lhs;
    kl_symbol *fsym = f->sym->ref ? f->sym->ref : f->sym;
    if (f->nodetype == TK_VAR) {
        r2.t = TK_VAR;
        r2.index = fsym->index;
        r2.level = f->sym->level;
        r2.typeid = f->typeid;
        r1->typeid = r2.typeid; // return value's type is same as r2's type.
        r2.typestr = mkkir_const_str(ctx, typeidname(f->typeid));
        r1->typestr = r2.typestr;
    } else {
        r2 = make_var(ctx, sym);
        r1i = gen_expr(ctx, sym, &r2, e->lhs);
        r1l = get_last(r1i);
    }
 
    kl_kir_inst *r2i = gen_callargs(ctx, sym, e->rhs);
    kl_kir_inst *inst = new_inst_op2(KIR_CALL, r1, &r2);
    kl_kir_inst *ilst = get_last(inst);
    ilst->next = new_inst_op1(KIR_RSSTKP, r1);  // temporary holding the ret value.

    if (r2i) {
        kl_kir_inst *r2l = get_last(r2i);
        if (r1l) {
            r1l->next = r2i;
            r2l->next = inst;
            inst = r1i;
        } else {
            r2l->next = inst;
            inst = r2i;
        }
    } else {
        if (r1l) {
            r1l->next = inst;
            inst = r1i;
        }
    }

    kl_kir_inst *head = new_inst(KIR_SVSTKP);
    head->next = inst;
    return head;
}

static kl_kir_inst *gen_ret(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *r1i = NULL;
    kl_kir_opr r1 = {
        .index = -1,
        .typeid = sym->type,
        .typestr = mkkir_const_str(ctx, typeidname(sym->type)),
    };

    KL_KIR_CHECK_LITERAL(s->e1, r1, r1i);
    kl_kir_inst *r1l = get_last(r1i);
    if (sym->has_func) {
        if (r1l) {
            r1l->next = new_inst(KIR_POPFRM);
            r1l = r1l->next;
        } else {
            r1l = new_inst(KIR_POPFRM);
        }
    }

    kl_kir_inst *inst = new_inst_ret(&r1);
    if (r1l) {
        r1l->next = inst;
        inst = r1i;
    }

    return inst;
}

static kl_kir_inst *gen_if(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;
    int l1, l2, l3;

    kl_kir_opr r1 = make_var(ctx, sym);
    head = last = gen_expr(ctx, sym, &r1, s->e1);
    KIR_MOVE_LAST(last);
    l1 = get_next_label(ctx);
    l2 = get_next_label(ctx);
    kl_kir_inst *next = new_inst_jumpiff(&r1, l1);
    KIR_ADD(last, next);
    if (s->s1) {
        kl_kir_inst *next = gen_block(ctx, sym, s->s1);
        KIR_ADD(last, next);
        next = new_inst_jump(l2, last);
        KIR_ADD(last, next);
    }
    next = new_inst_label(l1);
    KIR_ADD(last, next);
    if (s->s2) {
        kl_kir_inst *next = gen_block(ctx, sym, s->s1);
        KIR_ADD(last, next);
    }
    next = new_inst_label(l2);
    KIR_ADD(last, next);

    return head;
}

static kl_kir_inst *gen_expr(kl_context *ctx, kl_symbol *sym, kl_kir_opr *r1, kl_expr *e)
{
    kl_kir_inst *head = NULL;
    if (!e) return head;

    switch (e->nodetype) {
    case TK_VAR:
        break;

    case TK_CALL:
        head = gen_call(ctx, sym, r1, e);
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
        break;
    case TK_LT:
        head = gen_op3_inst(ctx, sym, KIR_LT, r1, e);
        break;
    case TK_LE:
    case TK_GT:
    case TK_GE:
    case TK_LGE:
        break;
    case TK_ADD:
        head = gen_op3_inst(ctx, sym, KIR_ADD, r1, e);
        break;
    case TK_SUB:
        head = gen_op3_inst(ctx, sym, KIR_SUB, r1, e);
        break;
    case TK_MUL:
    case TK_DIV:
    case TK_MOD:
        break;
    case TK_AND:
    case TK_OR:
    case TK_XOR:
    case TK_EXP:
    case TK_LSH:
    case TK_RSH:
    case TK_LAND:
    case TK_LOR:
    case TK_COMMA:
    default:
        ;
    }

    return head;
}

static kl_kir_inst *gen_block(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_inst *head = NULL;
    kl_kir_inst *last = NULL;

    while (s) {
        kl_kir_inst *next = gen_stmt(ctx, sym, s);
        if (next) {
            if (!last) {
                head = next;
            } else {
                last->next = next;
            }
            last = next;
            KIR_MOVE_LAST(last);
        }
        s = s->next;
    }

    return head;
}

static kl_kir_func *gen_function(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_func *func = new_func(ctx, sym->name);
    kl_kir_inst *last = NULL;

    if (sym->has_func) {
        func->head = last = new_inst(KIR_MKFRM);
    } else {
        func->head = last = new_inst(KIR_LOCAL);
    }

    while (s) {
        kl_kir_inst *next = gen_stmt(ctx, sym, s);
        if (next) {
            last->next = next;
            last = next;
            KIR_MOVE_LAST(last);
        }
        s = s->next;
    }

    func->head->r1 = (kl_kir_opr){ .t = TK_VSINT, .i64 = sym->count };
    return func;
}

static kl_kir_func *gen_namespace(kl_context *ctx, kl_symbol *sym, kl_stmt *s)
{
    kl_kir_func *func = new_func(ctx, sym->name);
    kl_kir_inst *last = NULL;

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

    return func;
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

    case TK_FUNC:
        if (s->s1) {
            kl_kir_func *func = gen_function(ctx, s->sym, s->s1);
            add_func(ctx->program, func);
        }
        break;

    case TK_CLASS:
        break;

    case TK_IF:
        if (s->e1) {
            head = gen_if(ctx, sym, s);
        }
        break;
    case TK_WHILE:
    case TK_DO:
        break;
    case TK_RETURN:
        head = gen_ret(ctx, sym, s);
        break;
    case TK_CONST:
    case TK_LET:
        break;
    case TK_EXPR:
        if (s->e1) {
            kl_kir_opr r1 = make_var(ctx, sym);
            head = gen_expr(ctx, sym, &r1, s->e1);
        }
        break;
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
    return 0;
}
