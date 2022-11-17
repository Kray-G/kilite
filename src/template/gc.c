#include "common.h"

/***************************************************************************
 * Garbage Collection
*/
void mark_frm(vmfrm *m);
void mark_var(vmvar *v);

void unmark_all(vmctx *ctx)
{
    vmstr *s = ctx->alc.str.liv;
    while (s) {
        UNMARK(s);
        s = s->liv;
    }
    vmbgi *bi = ctx->alc.bgi.liv;
    while (bi) {
        UNMARK(bi);
        bi = bi->liv;
    }
    vmobj *h = ctx->alc.obj.liv;
    while (h) {
        UNMARK(h);
        h = h->liv;
    }
    vmvar *v = ctx->alc.var.liv;
    while (v) {
        UNMARK(v);
        v = v->liv;
    }
    vmfnc *f = ctx->alc.fnc.liv;
    while (f) {
        UNMARK(f);
        f = f->liv;
    }
    vmfrm *m = ctx->alc.frm.liv;
    while (m) {
        UNMARK(m);
        m = m->liv;
    }
}

void mark_fnc(vmfnc *f)
{
    if (!f) {
        return;
    }
    if (IS_MARKED(f)) {
        return;
    }

    MARK(f);
    if (f->yfnc) {
        mark_fnc(f->yfnc);
    }
    if (f->frm) {
        mark_frm(f->frm);
    }
    if (f->lex) {
        mark_frm(f->lex);
    }
    if (f->varcnt > 0) {
        for (int i = 0; i < f->varcnt; ++i) {
            mark_var(f->vars[i]);
        }
    }
}

void mark_obj(vmobj *h)
{
    if (!h) {
        return;
    }
    MARK(h);

    vmvar *v = h->map;
    if (v) {
        for (int i = 0; i < h->hsz; ++i) {
            if (v[i].s) {
                MARK(v[i].s);
            }
            if (v[i].a) {
                mark_var(v[i].a);
            }
        }
    }
    vmvar **a = h->ary;
    if (a) {
        for (int i = 0; i < h->idxsz; ++i) {
            if (a[i]) {
                mark_var(a[i]);
            }
        }
    }
}

void mark_var(vmvar *v)
{
    if (!v) {
        return;
    }
    if (IS_MARKED(v)) {
        return;
    }

    MARK(v);
    if (v->f) {
        if (v->t == VAR_FNC) {
            mark_fnc(v->f);
        } else {
            v->f = NULL;
        }
    }
    if (v->o) {
        if (v->t == VAR_OBJ) {
            mark_obj(v->o);
        } else {
            v->o = NULL;
        }
    }
    if (v->s) {
        if (v->t == VAR_STR) {
            MARK(v->s);
        } else {
            v->s = NULL;
        }
    }
    if (v->bi) {
        if (v->t == VAR_BIG) {
            MARK(v->bi);
        } else {
            v->bi = NULL;
        }
    }
    if (v->p) {
        if (v->t != VAR_VOIDP) {
            free(v->p);
            v->p = NULL;
        }
    }
    if (v->a) {
        mark_var(v->a);
    }
}

void mark_frm(vmfrm *m)
{
    if (!m) {
        return;
    }
    if (IS_MARKED(m)) {
        return;
    }
    MARK(m);

    if (m->lex) {
        mark_frm(m->lex);
    }

    if (m->v) {
        for (int i = 0; i < m->vars; ++i) {
            mark_var(m->v[i]);
        }
    }
}

void premark_all(vmctx *ctx)
{
    vmstr *s = ctx->alc.str.liv;
    while (s) {
        vmstr *n = s->liv;
        if (IS_HELD(s)) {
            MARK(s);
        }
        s = n;
    }
    vmbgi *bi = ctx->alc.bgi.liv;
    while (bi) {
        vmbgi *n = bi->liv;
        if (IS_HELD(bi)) {
            MARK(bi);
        }
        bi = n;
    }
    vmobj *h = ctx->alc.obj.liv;
    while (h) {
        vmobj *n = h->liv;
        if (IS_HELD(h)) {
            mark_obj(h);
        }
        h = n;
    }
    vmvar *v = ctx->alc.var.liv;
    while (v) {
        vmvar *n = v->liv;
        if (IS_HELD(v)) {
            mark_var(v);
        }
        v = n;
    }
    vmfnc *f = ctx->alc.fnc.liv;
    while (f) {
        vmfnc *n = f->liv;
        if (IS_HELD(f)) {
            mark_fnc(f);
        }
        f = n;
    }
    vmfrm *m = ctx->alc.frm.liv;
    while (m) {
        vmfrm *n = m->liv;
        if (IS_HELD(m)) {
            mark_frm(m);
        }
        m = n;
    }
}

void mark_all(vmctx *ctx)
{
    premark_all(ctx);
    mark_var(ctx->except);
    mark_fnc(ctx->callee);
    mark_fnc(ctx->methodmissing);

    int fstkp = ctx->fstkp;
    vmfrm **m = ctx->fstk;
    while (fstkp--) {
        mark_frm(*m);
        ++m;
    }

    int vstkp = ctx->vstkp;
    vmvar *v = ctx->vstk;
    while (vstkp--) {
        if (v->a) mark_var(v->a);
        ++v;
    }
}

void sweep(vmctx *ctx)
{
    int sliv = 0;
    int sc = 0;
    vmstr *s = ctx->alc.str.liv;
    while (s) {
        ++sliv;
        vmstr *n = s->liv;
        if (!IS_MARKED(s)) {
            ++sc;
            if (STR_UNIT < s->cap) {
                free(s->s);
                s->s = s->hd = NULL;
                s->len = s->cap = 0;
            }
            pbakstr(ctx, s);
        }
        s = n;
    }
    int biliv = 0;
    int bic = 0;
    vmbgi *bi = ctx->alc.bgi.liv;
    while (bi) {
        ++biliv;
        vmbgi *n = bi->liv;
        if (!IS_MARKED(bi)) {
            ++bic;
            BzFree(bi->b);
            bi->b = NULL;
            pbakbgi(ctx, bi);
        }
        bi = n;
    }
    int hliv = 0;
    int hc = 0;
    vmobj *h = ctx->alc.obj.liv;
    while (h) {
        ++hliv;
        vmobj *n = h->liv;
        if (!IS_MARKED(h)) {
            ++hc;
            free(h->map);
            h->map = NULL;
            h->hsz = 0;
            free(h->ary);
            h->ary = NULL;
            h->asz = 0;
            pbakobj(ctx, h);
        }
        h = n;
    }
    int vliv = 0;
    int vc = 0;
    vmvar *v = ctx->alc.var.liv;
    while (v) {
        ++vliv;
        vmvar *n = v->liv;
        if (!IS_MARKED(v)) {
            ++vc;
            if (v->o) {
                v->o = NULL;
            }
            if (v->p) {
                free(v->p);
                v->p = NULL;
            }
            pbakvar(ctx, v);
        }
        v = n;
    }
    int fliv = 0;
    int fc = 0;
    vmfnc *f = ctx->alc.fnc.liv;
    while (f) {
        ++fliv;
        vmfnc *n = f->liv;
        if (!IS_MARKED(f)) {
            ++fc;
            if (f->vars) {
                free(f->vars);
                f->vars = NULL;
                f->varcnt = 0;
            }
            pbakfnc(ctx, f);
        }
        f = n;
    }
    int mliv = 0;
    int mc = 0;
    vmfrm *m = ctx->alc.frm.liv;
    while (m) {
        ++mliv;
        vmfrm *n = m->liv;
        if (!IS_MARKED(m)) {
            ++mc;
            pbakfrm(ctx, m);
        }
        m = n;
    }
    ctx->sweep = sc + bic + hc + vc + fc + mc;
    ++(ctx->gccnt);
    if (ctx->verbose && ctx->sweep > 0) {
        printf("GC %d done, vstk(%d), ", ctx->gccnt, ctx->vstkp);
        if (sc > 0) {
            printf("(str:%d,scan:%d)", sc, sliv);
        }
        if (bic > 0) {
            printf("(bgi:%d,scan:%d)", bic, biliv);
        }
        if (hc > 0) {
            printf("(obj:%d,scan:%d)", hc, hliv);
        }
        if (vc > 0) {
            printf("(var:%d,scan:%d)", vc, vliv);
        }
        if (fc > 0) {
            printf("(fnc:%d,scan:%d)", fc, fliv);
        }
        if (mc > 0) {
            printf("(frm:%d,scan:%d)", mc, mliv);
        }
        printf("\n");
    }
}

void mark_and_sweep(vmctx *ctx)
{
    ctx->tick = TICK_UNIT;
    ctx->sweep = 0;
    unmark_all(ctx);
    mark_all(ctx);
    sweep(ctx);
}
