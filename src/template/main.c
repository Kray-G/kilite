#include "common.h"

int main(void)
{
    vmctx *ctx = initialize();
    setup_context(ctx);

    vmvar r = {0};
    run_global_0(ctx, NULL, &r, 0);
    
    int ri = (r.t == VAR_INT64) ? r.i : 0;
    if (ctx->verbose || ctx->print_result) {
        if (r.t == VAR_INT64) {
            printf("%lld\n", r.i);
        } else if (r.t == VAR_DBL) {
            printf("%f\n", r.d);
        } else if (r.t == VAR_BIG) {
            char *bs = BzToString(r.bi->b, 10, 0);
            printf("%s\n", bs);
            BzFreeString(bs);
        } else if (r.t == VAR_STR) {
            printf("%s\n", r.s->s);
        } else {
            printf("<OBJ>\n");
        }
    }
    if (ctx->verbose) {
        mark_and_sweep(ctx);
        count(ctx);
    }

    finalize(ctx);
    return ri;
}
