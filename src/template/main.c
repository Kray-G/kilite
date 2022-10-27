#include "common.h"

int main(void)
{
    vmctx *ctx = initialize();
    setup_context(ctx);

    vmvar r;
    run_global_0(ctx, NULL, &r, 0);
    
    int ri = (r.t == VAR_INT64) ? r.i : 0;
    if (ctx->verbose) {
        if (r.t == VAR_INT64) {
            printf("return value = %lld\n", r.i);
        } else if (r.t == VAR_DBL) {
            printf("return value = %f\n", r.d);
        } else if (r.t == VAR_BIG) {
            char *bs = BzToString(r.bi->b, 10, 0);
            printf("return value = %s\n", bs);
            BzFreeString(bs);
        } else if (r.t == VAR_STR) {
            printf("return value = %s\n", r.s->s);
        } else {
            printf("return value = <OBJ>\n");
        }
        mark_and_sweep(ctx);
        count(ctx);
    }

    finalize(ctx);
    return ri;
}
