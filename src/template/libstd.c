#include "common.h"

/* System */

static int print(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    /* Skip the 1st argument because it is the System object itself. */
    for (int i = 1; i < ac; ++i) {
        vmvar *an = local_var(ctx, i);
        print_obj(ctx, an);
    }
}

static int println(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    /* Skip the 1st argument because it is the System object itself. */
    for (int i = 1; i < ac; ++i) {
        vmvar *an = local_var(ctx, i);
        print_obj(ctx, an);
    }
    printf("\n");
}

int System(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, print, print, 1)
    KL_SET_METHOD(o, println, println, 1)
    SET_OBJ(r, o);
    o->is_sysobj = 1;
    return 0;
}

/* SystemTimer */

extern void *SystemTimer_init(void);
extern void SystemTimer_restart_impl(void *p);
extern double SystemTimer_elapsed_impl(void *p);

static int SystemTimer_elapsed(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    if (a0->t == VAR_OBJ && 0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_VOIDP) {
        r->t = VAR_DBL;
        r->d = SystemTimer_elapsed_impl(a0->o->ary[0]->p);
    }
    return 0;
}

static int SystemTimer_restart(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    if (a0->t == VAR_OBJ && 0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_VOIDP) {
        r->t = VAR_INT64;
        r->i = 0;
        SystemTimer_restart_impl(a0->o->ary[0]->p);
    }
    return 0;
}

int SystemTimer_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    vmvar *timer = alcvar(ctx, VAR_VOIDP, 0);
    timer->p = SystemTimer_init();
    array_set(ctx, o, 0, timer);
    KL_SET_METHOD(o, elapsed, SystemTimer_elapsed, 1)
    KL_SET_METHOD(o, restart, SystemTimer_restart, 1)
    SET_OBJ(r, o);
    o->is_sysobj = 1;
    return 0;
}

int SystemTimer(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, create, SystemTimer_create, 1)
    SET_OBJ(r, o);
    return 0;
}

