#include "common.h"

int main(void)
{
    vmctx *ctx = initialize();
    setup_context(ctx);

    int ri = 0;
    vmvar r = {0};
    int e = run_global(ctx, NULL, &r, 0);
    if (e) {
        printf("Uncaught exception: <not implemented>\n");
    } else {
        ri = (r.t == VAR_INT64) ? r.i : 0;
        if (ctx->verbose || ctx->print_result) {
            print(&r);
            printf("\n");
        }
    }

    if (ctx->verbose) {
        mark_and_sweep(ctx);
        count(ctx);
    }

    finalize(ctx);
    return ri;
}
