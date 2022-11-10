#include "common.h"

static int print(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    for (int i = 0; i < ac; ++i) {
        vmvar *an = local_var(ctx, i);
        print_obj(ctx, an);
    }
}

static int println(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    for (int i = 0; i < ac; ++i) {
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
    return 0;
}
