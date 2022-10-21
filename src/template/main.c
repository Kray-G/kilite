#include "header.h"

int main(void)
{
    vmctx *ctx = initialize();

    run_global(ctx);
    mark_and_sweep(ctx);

    count(ctx);
    finalize(ctx);
    return 0;
}
