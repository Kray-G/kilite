#ifndef KILITE_AMALGAMATION
#include "common.h"
#endif

/***************************************************************************
 * Allocators
*/

static void setfrmvars(vmfrm *m, int vars)
{
    if (vars < VARS_MIN_IN_FRAME) vars = VARS_MIN_IN_FRAME;
    if (!m->v) {
        m->vars = vars;
        m->v = (vmvar **)calloc(vars, sizeof(vmvar **));
    } else {
        if (m->vars < vars) {
            free(m->v);
            m->v = (vmvar **)calloc(vars, sizeof(vmvar **));
            m->vars = vars;
        } else {
            memset(m->v, 0x00, vars * sizeof(vmvar*));
        }
    }
}

// funcs
static void alloc_fncs(vmctx *ctx, int n)
{
    while (n--) {
        vmfnc *v = (vmfnc *)calloc(1, sizeof(vmfnc));
        v->nxt = ctx->alc.fnc.nxt;
        v->chn = ctx->alc.fnc.chn;
        ctx->alc.fnc.nxt = ctx->alc.fnc.chn = v;
    }
}

vmfnc *alcfnc(vmctx *ctx, void *f, vmfrm *lex, const char *name, int args)
{
    if (ctx->alc.fnc.nxt == &(ctx->alc.fnc)) {
        alloc_fncs(ctx, ALC_UNIT);
        ctx->cnt.fnc += ALC_UNIT;
        ctx->fre.fnc += ALC_UNIT;
    }
    vmfnc *v = ctx->alc.fnc.nxt;
    ctx->alc.fnc.nxt = v->nxt;
    v->nxt = NULL;
    v->prv = NULL;
    v->name = name;
    v->liv = ctx->alc.fnc.liv;
    ctx->alc.fnc.liv = v;
    if (v->liv) {
        v->liv->prv = v;
    }

    v->f = f;
    v->lex = lex;
    v->args = args;
    ctx->fre.fnc--;
    return v;
}

void pbakfnc(vmctx *ctx, vmfnc *p)
{
    if (p && !p->nxt) {
        p->nxt = ctx->alc.fnc.nxt;
        ctx->alc.fnc.nxt = p;
        ctx->fre.fnc++;

        if (p->prv) {
            p->prv->liv = p->liv;
        } else {
            ctx->alc.fnc.liv = p->liv;
        }
        if (p->liv) {
            p->liv->prv = p->prv;
        }
    }
}

// frames
static void alloc_frms(vmctx *ctx, int n)
{
    while (n--) {
        vmfrm *v = (vmfrm *)calloc(1, sizeof(vmfrm));
        v->nxt = ctx->alc.frm.nxt;
        v->chn = ctx->alc.frm.chn;
        ctx->alc.frm.nxt = ctx->alc.frm.chn = v;
    }
}

vmfrm *alcfrm(vmctx *ctx, vmfrm *lex, int args)
{
    if (ctx->alc.frm.nxt == &(ctx->alc.frm)) {
        alloc_frms(ctx, ALC_UNIT_FRM);
        ctx->cnt.frm += ALC_UNIT_FRM;
        ctx->fre.frm += ALC_UNIT_FRM;
    }
    vmfrm *v = ctx->alc.frm.nxt;
    ctx->alc.frm.nxt = v->nxt;
    v->nxt = NULL;
    v->prv = NULL;
    v->liv = ctx->alc.frm.liv;
    ctx->alc.frm.liv = v;
    if (v->liv) {
        v->liv->prv = v;
    }

    v->lex = lex;
    if (args > 0) {
        setfrmvars(v, args);
    }
    ctx->fre.frm--;
    return v;
}

void pbakfrm(vmctx *ctx, vmfrm *p)
{
    if (p && !p->nxt) {
        p->nxt = ctx->alc.frm.nxt;
        ctx->alc.frm.nxt = p;
        ctx->fre.frm++;

        if (p->prv) {
            p->prv->liv = p->liv;
        } else {
            ctx->alc.frm.liv = p->liv;
        }
        if (p->liv) {
            p->liv->prv = p->prv;
        }
    }
}

// string
static void alloc_strs(vmctx *ctx, int n)
{
    while (n--) {
        vmstr *s = (vmstr *)calloc(1, sizeof(vmstr));
        s->nxt = ctx->alc.str.nxt;
        s->chn = ctx->alc.str.chn;
        ctx->alc.str.nxt = ctx->alc.str.chn = s;
    }
}

static vmstr *alcstr_pure(vmctx *ctx)
{
    if (ctx->alc.str.nxt == &(ctx->alc.str)) {
        alloc_strs(ctx, ALC_UNIT);
        ctx->cnt.str += ALC_UNIT;
        ctx->fre.str += ALC_UNIT;
    }
    vmstr *v = ctx->alc.str.nxt;
    ctx->alc.str.nxt = v->nxt;
    v->nxt = NULL;
    v->prv = NULL;
    v->liv = ctx->alc.str.liv;
    ctx->alc.str.liv = v;
    if (v->liv) {
        v->liv->prv = v;
    }

    ctx->fre.str--;
    return v;
}

vmstr *alcstr_allocated_str(vmctx *ctx, char *s, int alloclen)
{
    /* Caution!
        If you want to use this function, `s` should be allocated by malloc() or calloc() because it will be freed by free()!
    */
    vmstr *v = alcstr_pure(ctx);
    v->cap = v->len = alloclen;
    v->s = v->hd = s;
    return v;
}

vmstr *alcstr_str(vmctx *ctx, const char *s)
{
    vmstr *v = alcstr_pure(ctx);
    int len = s ? strlen(s) : -1;
    if (v->cap > 0) {
        if (0 <= len && len < v->cap) {
            strcpy(v->s, s);
            v->len = len;
            v->hd = v->s;
            return v;
        }
        free(v->s);
    }

    if (len < 0) {
        v->s = v->hd = (char *)calloc(STR_UNIT, sizeof(char));
        v->cap = STR_UNIT;
        v->len = 0;
        return v;
    }
    v->cap = (len < STR_UNIT) ? STR_UNIT : ((len / STR_UNIT) * (STR_UNIT << 1)); 
    v->s = v->hd = (char *)calloc(v->cap, sizeof(char));
    strcpy(v->s, s);
    v->len = len;
    return v;
}

void pbakstr(vmctx *ctx, vmstr *p)
{
    if (p && !p->nxt) {
        p->nxt = ctx->alc.str.nxt;
        ctx->alc.str.nxt = p;
        ctx->fre.str++;

        if (p->prv) {
            p->prv->liv = p->liv;
        } else {
            ctx->alc.str.liv = p->liv;
        }
        if (p->liv) {
            p->liv->prv = p->prv;
        }
    }
}

// binary
static void alloc_bins(vmctx *ctx, int n)
{
    while (n--) {
        vmbin *s = (vmbin *)calloc(1, sizeof(vmbin));
        s->nxt = ctx->alc.bin.nxt;
        s->chn = ctx->alc.bin.chn;
        ctx->alc.bin.nxt = ctx->alc.bin.chn = s;
    }
}

static vmbin *alcbin_pure(vmctx *ctx)
{
    if (ctx->alc.bin.nxt == &(ctx->alc.bin)) {
        alloc_bins(ctx, ALC_UNIT);
        ctx->cnt.bin += ALC_UNIT;
        ctx->fre.bin += ALC_UNIT;
    }
    vmbin *v = ctx->alc.bin.nxt;
    ctx->alc.bin.nxt = v->nxt;
    v->nxt = NULL;
    v->prv = NULL;
    v->liv = ctx->alc.bin.liv;
    ctx->alc.bin.liv = v;
    if (v->liv) {
        v->liv->prv = v;
    }

    ctx->fre.bin--;
    return v;
}

vmbin *alcbin_allocated_bin(vmctx *ctx, uint8_t *s, int alloclen)
{
    /* Caution!
        If you want to use this function, `s` should be allocated by malloc() or calloc() because it will be freed by free()!
    */
    vmbin *v = alcbin_pure(ctx);
    v->cap = v->len = alloclen;
    v->s = v->hd = s;
    return v;
}

vmbin *alcbin_bin(vmctx *ctx, const uint8_t *s, int len)
{
    vmbin *v = alcbin_pure(ctx);
    if (s == NULL || len == 0) {
        v->s = v->hd = (uint8_t *)calloc(BIN_UNIT, sizeof(uint8_t));
        v->cap = BIN_UNIT;
        v->len = 0;
        return v;
    }
    if (v->cap > 0) {
        if (0 <= len && len < v->cap) {
            memcpy(v->s, s, len);
            v->len = len;
            v->hd = v->s;
            return v;
        }
        free(v->s);
    }

    if (len < 0) {
        v->s = v->hd = (uint8_t *)calloc(BIN_UNIT, sizeof(uint8_t));
        v->cap = BIN_UNIT;
        v->len = 0;
        return v;
    }
    v->cap = (len < BIN_UNIT) ? BIN_UNIT : ((len / BIN_UNIT) * (BIN_UNIT << 1)); 
    v->s = v->hd = (uint8_t *)calloc(v->cap, sizeof(uint8_t));
    memcpy(v->s, s, len);
    v->len = len;
    return v;
}

vmbin *alcbin(vmctx *ctx)
{
    return alcbin_bin(ctx, NULL, 0);
}

void pbakbin(vmctx *ctx, vmbin *p)
{
    if (p && !p->nxt) {
        p->nxt = ctx->alc.bin.nxt;
        ctx->alc.bin.nxt = p;
        ctx->fre.bin++;

        if (p->prv) {
            p->prv->liv = p->liv;
        } else {
            ctx->alc.bin.liv = p->liv;
        }
        if (p->liv) {
            p->liv->prv = p->prv;
        }
    }
}

// bgint
static void alloc_bgis(vmctx *ctx, int n)
{
    while (n--) {
        vmbgi *bi = (vmbgi *)calloc(1, sizeof(vmbgi));
        bi->nxt = ctx->alc.bgi.nxt;
        bi->chn = ctx->alc.bgi.chn;
        ctx->alc.bgi.nxt = ctx->alc.bgi.chn = bi;
    }
}

static vmbgi *alcbgi_pure(vmctx *ctx)
{
    if (ctx->alc.bgi.nxt == &(ctx->alc.bgi)) {
        alloc_bgis(ctx, ALC_UNIT);
        ctx->cnt.bgi += ALC_UNIT;
        ctx->fre.bgi += ALC_UNIT;
    }
    vmbgi *v = ctx->alc.bgi.nxt;
    ctx->alc.bgi.nxt = v->nxt;
    v->nxt = NULL;
    v->prv = NULL;
    v->liv = ctx->alc.bgi.liv;
    ctx->alc.bgi.liv = v;
    if (v->liv) {
        v->liv->prv = v;
    }

    ctx->fre.bgi--;
    return v;
}

vmbgi *alcbgi_bigz(vmctx *ctx, BigZ bz)
{
    vmbgi *v = alcbgi_pure(ctx);
    v->b = bz;
    return v;
}

void pbakbgi(vmctx *ctx, vmbgi *p)
{
    if (p && !p->nxt) {
        p->nxt = ctx->alc.bgi.nxt;
        ctx->alc.bgi.nxt = p;
        ctx->fre.bgi++;

        if (p->prv) {
            p->prv->liv = p->liv;
        } else {
            ctx->alc.bgi.liv = p->liv;
        }
        if (p->liv) {
            p->liv->prv = p->prv;
        }
    }
}

// object
static void alloc_objs(vmctx *ctx, int n)
{
    while (n--) {
        vmobj *h = (vmobj *)calloc(1, sizeof(vmobj));
        h->nxt = ctx->alc.obj.nxt;
        h->chn = ctx->alc.obj.chn;
        ctx->alc.obj.nxt = ctx->alc.obj.chn = h;
    }
}

vmobj *alcobj(vmctx *ctx)
{
    if (ctx->alc.obj.nxt == &(ctx->alc.obj)) {
        alloc_objs(ctx, ALC_UNIT);
        ctx->cnt.obj += ALC_UNIT;
        ctx->fre.obj += ALC_UNIT;
    }
    vmobj *v = ctx->alc.obj.nxt;
    ctx->alc.obj.nxt = v->nxt;
    v->nxt = NULL;
    v->prv = NULL;
    v->liv = ctx->alc.obj.liv;
    ctx->alc.obj.liv = v;
    if (v->liv) {
        v->liv->prv = v;
    }

    v->idxsz = 0;
    ctx->fre.obj--;
    return v;
}

void pbakobj(vmctx *ctx, vmobj *p)
{
    if (p && !p->nxt) {
        p->nxt = ctx->alc.obj.nxt;
        ctx->alc.obj.nxt = p;
        ctx->fre.obj++;

        if (p->prv) {
            p->prv->liv = p->liv;
        } else {
            ctx->alc.obj.liv = p->liv;
        }
        if (p->liv) {
            p->liv->prv = p->prv;
        }
    }
}

// vars
static void alloc_vars(vmctx *ctx, int n)
{
    while (n--) {
        vmvar *v = (vmvar *)calloc(1, sizeof(vmvar));
        v->nxt = ctx->alc.var.nxt;
        v->chn = ctx->alc.var.chn;
        ctx->alc.var.nxt = ctx->alc.var.chn = v;
    }
}

static vmvar *alcvar_pure(vmctx *ctx, vartype t)
{
    if (ctx->alc.var.nxt == &(ctx->alc.var)) {
        alloc_vars(ctx, ALC_UNIT);
        ctx->cnt.var += ALC_UNIT;
        ctx->fre.var += ALC_UNIT;
    }
    vmvar *v = ctx->alc.var.nxt;
    ctx->alc.var.nxt = v->nxt;
    v->nxt = NULL;
    v->prv = NULL;
    v->liv = ctx->alc.var.liv;
    ctx->alc.var.liv = v;
    if (v->liv) {
        v->liv->prv = v;
    }
    v->t = t;
    ctx->fre.var--;
    return v;
}

vmvar *alcvar(vmctx *ctx, vartype t, int hold)
{
    vmvar *v = alcvar_pure(ctx, t);
    v->i = 0;
    if (hold) {
        HOLD(v);
    }
    if (t == VAR_BIG) {
        v->bi = alcbgi_bigz(ctx, BzFromInteger(0));
    }
    return v;
}

vmvar *alcvar_initial(vmctx *ctx)
{
    return alcvar_pure(ctx, VAR_UNDEF);
}

vmvar *alcvar_obj(vmctx *ctx, vmobj *o)
{
    vmvar *v = alcvar_pure(ctx, VAR_OBJ);
    v->o = o;
    return v;
}

vmvar *alcvar_fnc(vmctx *ctx, vmfnc *f)
{
    vmvar *v = alcvar_pure(ctx, VAR_FNC);
    v->f = f;
    return v;
}

vmvar *alcvar_int64(vmctx *ctx, int64_t i, int hold)
{
    vmvar *v = alcvar_pure(ctx, VAR_INT64);
    v->i = i;
    if (hold) {
        HOLD(v);
    }
    return v;
}

vmvar *alcvar_double(vmctx *ctx, double *d)
{
    vmvar *v = alcvar_pure(ctx, VAR_DBL);
    v->d = *d;
    return v;
}

vmvar *alcvar_str(vmctx *ctx, const char *s)
{
    vmvar *v = alcvar_pure(ctx, VAR_STR);
    v->s = alcstr_str(ctx, s);
    return v;
}

vmvar *alcvar_sv(vmctx *ctx, vmstr *sv)
{
    vmvar *v = alcvar_pure(ctx, VAR_STR);
    v->s = sv;
    return v;
}

vmvar *alcvar_bin(vmctx *ctx, const uint8_t *s, int len)
{
    vmvar *v = alcvar_pure(ctx, VAR_BIN);
    v->bn = alcbin_bin(ctx, s, len);
    return v;
}

vmvar *alcvar_bgistr(vmctx *ctx, const char *s, int radix)
{
    vmvar *v = alcvar_pure(ctx, VAR_BIG);
    v->bi = alcbgi_bigz(ctx, BzFromString(s, radix, BZ_UNTIL_END));
    bi_normalize(v);
    return v;
}

void pbakvar(vmctx *ctx, vmvar *p)
{
    if (p && !p->nxt) {
        if (p->bi) {
            pbakbgi(ctx, p->bi);
        }
        if (p->s) {
            pbakstr(ctx, p->s);
        }
        p->nxt = ctx->alc.var.nxt;
        ctx->alc.var.nxt = p;
        UNHOLD(p);
        ctx->fre.var++;

        if (p->prv) {
            p->prv->liv = p->liv;
        } else {
            ctx->alc.var.liv = p->liv;
        }
        if (p->liv) {
            p->liv->prv = p->prv;
        }
    }
}

vmvar *copy_var(vmctx *ctx, vmvar *src, int hold)
{
    vmvar *v = NULL;
    switch (src->t) {
    case VAR_UNDEF:
        v = alcvar_pure(ctx, VAR_UNDEF);
        if (hold) HOLD(v);
        break;
    case VAR_INT64:
        v = alcvar_pure(ctx, VAR_INT64);
        v->i = src->i;
        if (hold) HOLD(v);
        break;
    case VAR_DBL:
        v = alcvar_pure(ctx, VAR_DBL);
        v->d = src->d;
        if (hold) HOLD(v);
        break;
    case VAR_BIG:
        v = alcvar_pure(ctx, VAR_BIG);
        v->bi = bi_copy(ctx, src->bi);
        if (hold) HOLD(v);
        break;
    case VAR_STR:
        v = alcvar_pure(ctx, VAR_STR);
        v->s = alcstr_str(ctx, src->s->s);
        if (hold) HOLD(v);
        break;
    case VAR_BIN:
        v = alcvar_pure(ctx, VAR_BIN);
        v->bn = src->bn;
        if (hold) HOLD(v);
        break;
    case VAR_FNC:
        v = alcvar_pure(ctx, VAR_FNC);
        v->f = src->f;
        if (hold) HOLD(v);
        break;
    case VAR_OBJ:
        v = alcvar_pure(ctx, VAR_OBJ);
        v->o = src->o;
        if (hold) HOLD(v);
        break;
    default:
        // Error.
        v = alcvar(ctx, VAR_INT64, hold);
        break;
    }
    return v;
}

// init
void initialize_allocators(vmctx *ctx)
{
    ctx->alc.var.nxt = &(ctx->alc.var);
    HOLD(ctx->alc.var.nxt);
    ctx->alc.fnc.nxt = &(ctx->alc.fnc);
    HOLD(ctx->alc.fnc.nxt);
    ctx->alc.frm.nxt = &(ctx->alc.frm);
    HOLD(ctx->alc.frm.nxt);
    ctx->alc.str.nxt = &(ctx->alc.str);
    HOLD(ctx->alc.str.nxt);
    ctx->alc.bin.nxt = &(ctx->alc.bin);
    HOLD(ctx->alc.bin.nxt);
    ctx->alc.bgi.nxt = &(ctx->alc.bgi);
    HOLD(ctx->alc.bgi.nxt);
    ctx->alc.obj.nxt = &(ctx->alc.obj);
    HOLD(ctx->alc.obj.nxt);
}
