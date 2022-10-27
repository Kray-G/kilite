#include "common.h"

BigZ i64maxp1 = BZNULL;
BigZ i64minm1 = BZNULL;

void bi_initialize(void)
{
    if (i64maxp1 == BZNULL) {
        i64maxp1 = BzFromString("8000000000000000", 16, BZ_UNTIL_END);
    }
    if (i64minm1 == BZNULL) {
        i64minm1 = BzFromString("8000000000000001", 16, BZ_UNTIL_END);
        BzSetSign(i64minm1, BZ_MINUS);
    }
}

void bi_finalize(void)
{
    BzFree(i64maxp1);
    i64maxp1 = BZNULL;
    BzFree(i64minm1);
    i64minm1 = BZNULL;
}

vmbgi *bi_copy(vmctx *ctx, vmbgi *src)
{
    vmbgi *dst = alcbgi_bigz(ctx, BzCopy(src->b));
    return dst;
}

void bi_print(vmbgi *b)
{
    char s1[1024] = {0};
    bi_str(s1, 1024, b);
    printf("%s\n", s1);
}

void bi_str(char *buf, int max, vmbgi *b)
{
    char *bs = BzToString(b->b, 10, 0);
    snprintf(buf, max, "%s", bs);
    BzFreeString(bs);
}

void bi_normalize(vmvar *v)
{
    if (v->t != VAR_BIG) {
        return;
    }
    vmbgi *r = v->bi;
    BigZ bz = r->b;
    if (BzGetSign(bz) == BZ_ZERO) {
        v->t = VAR_INT64;
        v->i = 0;
    } else if (BzGetSign(bz) == BZ_MINUS) {
        BzCmp comp = BzCompare(bz, i64minm1);
        if (comp == BZ_GT) {
            v->t = VAR_INT64;
            v->i = BzToInteger(bz);
        }
    } else {
        BzCmp comp = BzCompare(bz, i64maxp1);
        if (comp == BZ_LT) {
            v->t = VAR_INT64;
            v->i = BzToInteger(bz);
        }
    }
}
