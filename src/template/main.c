#include "common.h"

int main(void)
{
    vmctx *ctx = initialize();
    setup_context(ctx);
    ctx->except = alcvar_initial(ctx);

    int ri = 0;
    vmvar r = {0};
    ctx->callee = alcfnc(ctx, run_global, NULL, "run_global", 0); // dummy.
    int e = run_global(ctx, NULL, &r, 0);
    ctx->callee = NULL;
    if (e) {
        exception_uncaught(ctx, ctx->except);
    } else {
        ri = (r.t == VAR_INT64) ? r.i : 0;
        if (ctx->verbose || ctx->print_result) {
            print_obj(ctx, &r);
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
