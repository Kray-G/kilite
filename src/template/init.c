#include "header.h"

/***************************************************************************
 * Initializing and finalizing
*/

vmctx *initialize(void)
{
    vmctx *ctx = (vmctx *)calloc(1, sizeof(vmctx));
    ctx->tick = TICK_UNIT;

    ctx->fstksz = FRM_STACK_SIZE;
    ctx->fstkp = 0;
    ctx->fstk = (vmfrm **)calloc(FRM_STACK_SIZE, sizeof(vmfrm*));

    ctx->vstksz = VAR_STACK_SIZE;
    ctx->vstkp = 0;
    ctx->vstk = (vmvar *)calloc(VAR_STACK_SIZE, sizeof(vmvar));

    initialize_allocators(ctx);
    bi_initialize();
    return ctx;
}

void finalize(vmctx *ctx)
{
    bi_finalize();

    #define FREELIST(mem, blk) {\
        vm##mem *v = ctx->alc.mem.chn;\
        while (v) {\
            vm##mem *n = v->chn;\
            blk;\
            free(v);\
            v = n;\
        }\
    }\
    /**/
    FREELIST(var, {});
    FREELIST(str, { if (v->s) free(v->s); });
    FREELIST(bgi, { if (v->b) BzFree(v->b); });
    FREELIST(fnc, {});
    FREELIST(frm, { free(v->v); });
    FREELIST(hsh, {});
    #undef FREELIST

    free(ctx->fstk);
    free(ctx->vstk);
    free(ctx);
}
