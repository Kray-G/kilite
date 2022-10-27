#include "common.h"

int main(void)
{
    vmctx *ctx = initialize();

    vmvar r;
    run_global_0(ctx, NULL, &r, 0);
    printf("r-> = %lld\n", r.i);
    mark_and_sweep(ctx);

    count(ctx);
    finalize(ctx);
    return 0;
}
