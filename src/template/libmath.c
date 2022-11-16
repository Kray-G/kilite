#include "common.h"

#define KL_DEF_MATH_FUNCTION(name) \
extern double name(double x); \
int Math_##name(vmctx *ctx, vmfrm *lex, vmvar *r, int ac) \
{ \
    if (ac < 1) { \
        /* TODO: too few arguments */ \
        return 1; \
    } \
    vmvar *v = local_var(ctx, 0); \
    KL_CAST_TO_DBL(ctx, v) \
    SET_DBL((r), name(v->d)); \
    return 0; \
} \
/**/

#define KL_DEF_MATH_FUNCTION2(name) \
extern double name(double x, double y); \
int Math_##name(vmctx *ctx, vmfrm *lex, vmvar *r, int ac) \
{ \
    if (ac < 2) { \
        /* TODO: too few arguments */ \
        return 1; \
    } \
    vmvar *v1 = local_var(ctx, 0); \
    KL_CAST_TO_DBL(ctx, v1) \
    vmvar *v2 = local_var(ctx, 1); \
    KL_CAST_TO_DBL(ctx, v2) \
    SET_DBL((r), name(v1->d, v2->d)); \
    return 0; \
} \
/**/

#define KL_DEF_MATH_FUNCTION2_INT(name) \
extern double name(double, int); \
int Math_##name(vmctx *ctx, vmfrm *lex, vmvar *r, int ac) \
{ \
    if (ac < 2) { \
        /* TODO: too few arguments */ \
        return 1; \
    } \
    vmvar *v1 = local_var(ctx, 0); \
    KL_CAST_TO_DBL(ctx, v1) \
    vmvar *v2 = local_var(ctx, 1); \
    KL_CAST_TO_I64(ctx, v2) \
    SET_DBL((r), name(v1->d, (int)(v2->i))); \
    return 0; \
} \
/**/

extern uint32_t Math_random_impl(void);
int Math_random(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    double rnd = ((double)(Math_random_impl() % 0x7fffffff)) / 0x7fffffff;
    SET_DBL(r, rnd);
    return 0;
}

KL_DEF_MATH_FUNCTION(acos)
KL_DEF_MATH_FUNCTION(asin)
KL_DEF_MATH_FUNCTION(atan)
KL_DEF_MATH_FUNCTION2(atan2)
KL_DEF_MATH_FUNCTION(cos)
KL_DEF_MATH_FUNCTION(sin)
KL_DEF_MATH_FUNCTION(tan)
KL_DEF_MATH_FUNCTION(cosh)
KL_DEF_MATH_FUNCTION(sinh)
KL_DEF_MATH_FUNCTION(tanh)
KL_DEF_MATH_FUNCTION(exp)
KL_DEF_MATH_FUNCTION2_INT(ldexp)
KL_DEF_MATH_FUNCTION(log)
KL_DEF_MATH_FUNCTION(log10)
KL_DEF_MATH_FUNCTION2(pow)
KL_DEF_MATH_FUNCTION(sqrt)
KL_DEF_MATH_FUNCTION(ceil)
KL_DEF_MATH_FUNCTION(fabs)
KL_DEF_MATH_FUNCTION(floor)
KL_DEF_MATH_FUNCTION2(fmod)

#define KL_SET_MATHMETHOD(o, name, args) \
    hashmap_set(ctx, o, #name, alcvar_fnc(ctx, alcfnc(ctx, Math_##name, NULL, #name, args))); \
/**/

int Math(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_MATHMETHOD(o, acos, 1)
    KL_SET_MATHMETHOD(o, asin, 1)
    KL_SET_MATHMETHOD(o, atan, 1)
    KL_SET_MATHMETHOD(o, atan2, 2)
    KL_SET_MATHMETHOD(o, cos, 1)
    KL_SET_MATHMETHOD(o, sin, 1)
    KL_SET_MATHMETHOD(o, tan, 1)
    KL_SET_MATHMETHOD(o, cosh, 1)
    KL_SET_MATHMETHOD(o, sinh, 1)
    KL_SET_MATHMETHOD(o, tanh, 1)
    KL_SET_MATHMETHOD(o, exp, 1)
    KL_SET_MATHMETHOD(o, ldexp, 2)
    KL_SET_MATHMETHOD(o, log, 1)
    KL_SET_MATHMETHOD(o, log10, 1)
    KL_SET_MATHMETHOD(o, pow, 2)
    KL_SET_MATHMETHOD(o, sqrt, 1)
    KL_SET_MATHMETHOD(o, ceil, 1)
    KL_SET_MATHMETHOD(o, fabs, 1)
    KL_SET_MATHMETHOD(o, floor, 1)
    KL_SET_MATHMETHOD(o, fmod, 2)
    KL_SET_MATHMETHOD(o, random, 0)
    SET_OBJ(r, o);
    return 0;
}
