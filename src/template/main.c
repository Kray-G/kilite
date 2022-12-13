#ifndef KILITE_AMALGAMATION
#include "common.h"
#include "lib.h"
#endif

#ifdef __MIRC__
FILE *stdout, *stderr, *stdin;
#endif

int main(int ac, char **av)
{
    #ifdef __MIRC__
    /* Should use a host system's standard I/O. */
    stdin  = get_stdio_stdin();
    stdout = get_stdio_stdout();
    stderr = get_stdio_stderr();
    #endif

    Regex_initialize();
    Math_initialize();

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

    Math_finalize();
    Regex_finalize();
    return ri;
}
