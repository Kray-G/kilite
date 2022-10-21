#ifndef KILITE_TEMPLATE_OP_H
#define KILITE_TEMPLATE_OP_H

#include "lib/bigz.h"

extern BigZ i64maxp1;
extern BigZ i64minm1;

/* Operator macros */

/* Copy Variable */

#define COPY_VAR_TO(ctx, dst, src) { \
    switch ((src)->t) { \
    case VAR_INT64: \
        (dst)->t = VAR_INT64; \
        (dst)->i = (src)->i; \
        break; \
    case VAR_DBL: \
        (dst)->t = VAR_DBL; \
        (dst)->d = (src)->d; \
        break; \
    case VAR_BIG: \
        (dst)->t = VAR_BIG; \
        (dst)->bi = bi_copy(ctx, (src)->bi); \
        break; \
    case VAR_STR: \
        (dst)->t = VAR_STR; \
        /* TODO */ \
        break; \
    case VAR_FNC: \
        (dst)->t = VAR_FNC; \
        (dst)->f = (src)->f; \
        break; \
    default: \
        /* Error */ \
        (dst)->t = VAR_INT64; \
        (dst)->i = 0; \
        break; \
    } \
} \
/**/

/* ADD */

#define OP_ADD_I_I(ctx, r, i0, i1) { \
    if ((i0) >= 0) { \
        if (((i1) <= 0) || ((i0) <= (INT64_MAX - (i1)))) { \
            (r)->t = VAR_INT64; \
            (r)->i = (i0) + (i1); \
        } else { \
            BigZ b2 = BzFromInteger(i1); \
            BigZ bi = BzFromInteger(i0); \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzAdd(bi, b2)); \
            BzFree(bi); \
            BzFree(b2); \
            bi_normalize(r); \
        } \
    } else if (((i1) >= 0) || ((i0) >= (INT64_MIN - (i1))))  { \
        (r)->t = VAR_INT64; \
        (r)->i = (i0) + (i1); \
    } else { \
        BigZ b2 = BzFromInteger(i1); \
        BigZ bi = BzFromInteger(i0); \
        (r)->t = VAR_BIG; \
        (r)->bi = alcbgi_bigz(ctx, BzAdd(bi, b2)); \
        BzFree(bi); \
        BzFree(b2); \
        bi_normalize(r); \
    } \
} \
/**/

#define OP_ADD_B_I(ctx, r, v0, i1) { \
    BigZ bi = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAdd((v0)->bi->b, bi)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_ADD_I_B(ctx, r, i0, v1) { \
    BigZ bi = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAdd(bi, (v1)->bi->b)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_ADD_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_ADD_I_I(ctx, r, i0, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_ADD_B_I(ctx, r, v0, i1) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_ADD(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i0 = (v0)->i; \
            int64_t i1 = (v1)->i; \
            OP_ADD_I_I(ctx, r, i0, i1) \
        } else if ((v1)->t == VAR_BIG) { \
            int64_t i0 = (v0)->i; \
            OP_ADD_I_B(ctx, r, i0, v1) \
        } else { \
            /* TODO */ \
        } \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_ADD_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzAdd((v0)->bi->b, (v1)->bi->b)); \
            bi_normalize(r); \
        } else { \
            /* TODO */ \
        } \
    } else { \
        /* TODO */ \
    } \
} \
/**/

/* SUB */

#define OP_SUB_I_I(ctx, r, i0, i1) { \
    if ((i0) >= 0) { \
        if (((i1) >= 0) || ((i0) <= (INT64_MAX + (i1)))) { \
            (r)->t = VAR_INT64; \
            (r)->i = (i0) - (i1); \
        } else { \
            BigZ b2 = BzFromInteger(i1); \
            BigZ bi = BzFromInteger(i0); \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzSubtract(bi, b2)); \
            BzFree(bi); \
            BzFree(b2); \
            bi_normalize(r); \
        } \
    } else if (((i1) <= 0) || ((i0) >= (INT64_MIN + (i1)))) { \
        (r)->t = VAR_INT64; \
        (r)->i = (i0) - (i1); \
    } else { \
        BigZ b2 = BzFromInteger(i1); \
        BigZ bi = BzFromInteger(i0); \
        (r)->t = VAR_BIG; \
        (r)->bi = alcbgi_bigz(ctx, BzSubtract(bi, b2)); \
        BzFree(bi); \
        BzFree(b2); \
        bi_normalize(r); \
    } \
} \
/**/

#define OP_SUB_B_I(ctx, r, v0, i1) { \
    BigZ bi = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzSubtract((v0)->bi->b, bi)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_SUB_I_B(ctx, r, i0, v1) { \
    BigZ bi = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzSubtract(bi, (v1)->bi->b)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_SUB_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_SUB_I_I(ctx, r, i0, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_SUB_B_I(ctx, r, v0, i1) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_SUB(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i0 = (v0)->i; \
            int64_t i1 = (v1)->i; \
            OP_SUB_I_I(ctx, r, i0, i1) \
        } else if ((v1)->t == VAR_BIG) { \
            int64_t i0 = (v0)->i; \
            OP_SUB_I_B(ctx, r, i0, v1) \
        } else { \
            /* TODO */ \
        } \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_SUB_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzSubtract((v0)->bi->b, (v1)->bi->b)); \
        } else { \
            /* TODO */ \
        } \
    } else { \
        /* TODO */ \
    } \
} \
/**/

/* LT */

#define OP_LT_I_I(ri, i0, i1) { \
    ri = ((v0)->i < (v1)->i); \
} \
/**/

#define OP_LT_B_I(ri, v0, i1) { \
    BigZ b1 = BzFromInteger(i1); \
    BzCmp r = BzCompare((v0)->bi->b, b1); \
    BzFree(b1); \
    ri = (r == BZ_LT); \
} \
/**/

#define OP_LT_I_B(ri, i0, v1) { \
    BigZ b0 = BzFromInteger(i0); \
    BzCmp r = BzCompare(b0, (v1)->bi->b); \
    BzFree(b0); \
    ri = (r == BZ_LT); \
} \
/**/

#define OP_LT_V_I(ri, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        ri = ((v0)->i < i1); \
    } else if ((v0)->t == VAR_BIG) { \
        OP_LT_B_I(ri, v0, i1) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_LT(ri, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        if ((v1)->t == VAR_INT64) { \
            OP_LT_I_I(ri, (v0)->i, (v1)->i); \
        } else if ((v1)->t = VAR_BIG) { \
            int64_t i0 = (v0)->i; \
            OP_LT_I_B(ri, i0, v1) \
        } else { \
            /* TODO */ \
        } \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_LT_B_I(ri, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            BzCmp r = BzCompare((v0)->bi->b, (v1)->bi->b); \
            ri = (r == BZ_LT); \
        } else { \
            /* TODO */ \
        } \
    } else { \
            /* TODO */ \
    } \
} \
/**/

#endif /* KILITE_TEMPLATE_OP_H */
