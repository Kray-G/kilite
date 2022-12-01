#ifndef KILITE_AMALGAMATION
#include "common.h"
#endif

int main(int ac, char **av)
{
    vmctx *ctx = initialize();
    setup_context(ctx);
    ctx->except = alcvar_initial(ctx);
    ctx->args = alcobj(ctx);
    for (int i = 0; i < ac; ++i) {
        array_push(ctx, ctx->args, alcvar_str(ctx, av[i]));
    }

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

    ctx->except = NULL;
    if (ctx->verbose) {
        mark_and_sweep(ctx);
        count(ctx);
    }

    finalize_context(ctx);
    return ri;
}
