#include "header.h"

#define DEF_COUNT(type, idx)\
    void count_##idx(vmctx *ctx)\
    {\
        printf(#idx " allocated = %d, free = %d\n", ctx->cnt.idx, ctx->fre.idx);\
    }\
/**/

DEF_COUNT(vmstr, str)
DEF_COUNT(vmbgi, bgi)
DEF_COUNT(vmfrm, frm)
DEF_COUNT(vmfnc, fnc)
DEF_COUNT(vmvar, var)

void count(vmctx *ctx)
{
    count_str(ctx);
    count_bgi(ctx);
    count_frm(ctx);
    count_fnc(ctx);
    count_var(ctx);
}
