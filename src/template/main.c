#include "common.h"

int main(void)
{
    vmctx *ctx = initialize();

    vmvar r;
    vmvar *ref = &r;
    run_global_0(ctx, NULL, &ref, 0);
    printf("r-> = %lld\n", r.i);
    mark_and_sweep(ctx);

    count(ctx);
    finalize(ctx);
    return 0;
}
