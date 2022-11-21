#include "common.h"

#define DEF_ARG(a0, ac, type) \
    if (ac < ac) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TOO_FEW_ARGUMENTS); \
    } \
    vmvar *a0 = local_var(ctx, 0); \
    if (a0->t != type) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH); \
    } \
/**/

/* This is the prototype that the functions written here will need. */

extern void *SystemTimer_init(void);
extern void SystemTimer_restart_impl(void *p);
extern double SystemTimer_elapsed_impl(void *p);

/* Exceptions */

int exception_addtrace(vmctx *ctx, vmvar *e, const char *funcname, const char *filename, int linenum)
{
    if (e->t != VAR_OBJ || linenum <= 0) {
        return 0;
    }
    vmobj *o = e->o;
    vmvar *trace = alcvar_obj(ctx, alcobj(ctx));
    vmvar *func = alcvar_str(ctx, funcname);
    vmvar *file = alcvar_str(ctx, filename);
    vmvar *line = alcvar_int64(ctx, linenum, 0);
    hashmap_set(ctx, trace->o, "_func", func);
    hashmap_set(ctx, trace->o, "_file", file);
    hashmap_set(ctx, trace->o, "_line", line);
    array_push(ctx, o, trace);
    return 0;
}

int exception_printtrace(vmctx *ctx, vmvar *e)
{
    if (!e || e->t != VAR_OBJ) {
        return 0;
    }

    vmobj *o = e->o;
    printf("Stack Trace Information:\n");
    int sz = o->idxsz;
    for (int i = 0; i < sz; ++i) {
        vmvar *trace = o->ary[i];
        vmvar *func = hashmap_search(trace->o, "_func");
        vmvar *file = hashmap_search(trace->o, "_file");
        vmvar *line = hashmap_search(trace->o, "_line");
        if (func && func->t == VAR_STR && file && file->t == VAR_STR && line && line->t == VAR_INT64) {
            if (func->s->s[0] == '<') {
                printf("        at %s(%s:%lld)\n", func->s->s, file->s->s, line->i);
            } else {
                printf("        at function %s(%s:%lld)\n", func->s->s, file->s->s, line->i);
            }
        }
    }

    return 0;
}

int exception_uncaught(vmctx *ctx, vmvar *e)
{
    if (!e) {
        return 0;
    }

    printf("Uncaught exception: No one catch the exception.\n");
    if (e->t != VAR_OBJ) {
        print_obj(ctx, e);
        return 0;
    }

    vmvar *type = hashmap_search(e->o, "_type");
    if (!type || type->t != VAR_STR) {
        print_obj(ctx, e);
    } else {
        const char *t = type->s->s;
        vmvar *what = hashmap_search(e->o, "_what");
        const char *w = (what && what->t == VAR_STR) ? what->s->s : "No message";
        printf("%s: %s\n", t, w);
        exception_printtrace(ctx, e);
    }

    return 0;
}

static int Exception_type(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *e = local_var(ctx, 0);

    // e->t should be always VAR_OBJ.
    vmvar *type = hashmap_search(e->o, "_type");
    if (type) {
        COPY_VAR_TO(ctx, r, type);
    } else {
        r->t = VAR_STR;
        r->s = alcstr_str(ctx, "SystemException");
    }

    return 0;
}

static int Exception_what(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *e = local_var(ctx, 0);

    // e->t should be always VAR_OBJ.
    vmvar *what = hashmap_search(e->o, "_what");
    if (what) {
        COPY_VAR_TO(ctx, r, what);
    } else {
        r->t = VAR_STR;
        r->s = alcstr_str(ctx, "");
    }

    return 0;
}

static int Exception_printStackTrace(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *e = local_var(ctx, 0);
    return exception_printtrace(ctx, e);
}

static int Exception_create(vmctx *ctx, vmfrm *lex, vmvar *r, const char *name)
{
    vmobj *o = alcobj(ctx);
    vmvar *type = alcvar_str(ctx, name);
    hashmap_set(ctx, o, "_type", type);
    KL_SET_METHOD(o, type, Exception_type, 1)
    KL_SET_METHOD(o, printStackTrace, Exception_printStackTrace, 1)
    SET_OBJ(r, o);
    o->is_sysobj = 1;
    return 0;
}

int RuntimeException_create(vmctx *ctx, vmfrm *lex, vmvar *r, const char *name, const char *msg)
{
    Exception_create(ctx, lex, r, name);
    vmobj *o = r->o;
    hashmap_set(ctx, o, "_what", alcvar_str(ctx, msg));
    KL_SET_METHOD(o, what, Exception_what, 1)
    return 0;
}

int RuntimeException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *msg = ac < 1 ? NULL : local_var(ctx, 0);
    if (msg && msg->t == VAR_STR) {
        RuntimeException_create(ctx, lex, r, "RuntimeException", msg->s->s);
    } else {
        RuntimeException_create(ctx, lex, r, "RuntimeException", "Unknown");
    }
    return 0;
}

int StackOverflowException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "StackOverflowException", "System stack has been exhausted");
    return 0;
}

int DivideByZeroException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "DivideByZeroException", "Divide by zero");
    return 0;
}

int UnsupportedOperationException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "UnsupportedOperationException", "Unsupported operation");
    return 0;
}

int MethodMissingException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "MethodMissingException", "Method missing");
    return 0;
}

int NoMatchingPatternException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "NoMatchingPatternException", "Pattern not matched");
    return 0;
}

int TooFewArgumentsException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "TooFewArgumentsException", "Too few arguments");
    return 0;
}

int TypeMismatchException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "TypeMismatchException", "Type mismatch");
    return 0;
}

int InvalidFiberStateException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "InvalidFiberStateException", "Invalid fiber state");
    return 0;
}

int DeadFiberCalledException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "DeadFiberCalledException", "Dead fiber called");
    return 0;
}

/* Make exception */

int throw_system_exception(int line, vmctx *ctx, int id)
{
    vmvar *r = alcvar_initial(ctx);
    switch (id) {
    case EXCEPT_EXCEPTION:
        RuntimeException_create(ctx, NULL, r, "SystemException", "Unknown exception");
        break;
    case EXCEPT_RUNTIME_EXCEPTION:
        RuntimeException_create(ctx, NULL, r, "RuntimeException", "Unknown exception");
        break;
    case EXCEPT_STACK_OVERFLOW:
        StackOverflowException(ctx, NULL, r, 0);
        break;
    case EXCEPT_DIVIDE_BY_ZERO:
        DivideByZeroException(ctx, NULL, r, 0);
        break;
    case EXCEPT_UNSUPPORTED_OPERATION:
        UnsupportedOperationException(ctx, NULL, r, 0);
        break;
    case EXCEPT_METHOD_MISSING:
        MethodMissingException(ctx, NULL, r, 0);
        break;
    case EXCEPT_NO_MATCHING_PATTERN:
        NoMatchingPatternException(ctx, NULL, r, 0);
        break;
    case EXCEPT_TOO_FEW_ARGUMENTS:
        TooFewArgumentsException(ctx, NULL, r, 0);
        break;
    case EXCEPT_TYPE_MISMATCH:
        TypeMismatchException(ctx, NULL, r, 0);
        break;
    case EXCEPT_INVALID_FIBER_STATE:
        InvalidFiberStateException(ctx, NULL, r, 0);
        break;
    case EXCEPT_DEAD_FIBER_CALLED:
        DeadFiberCalledException(ctx, NULL, r, 0);
        break;
    default:
        RuntimeException_create(ctx, NULL, r, "SystemException", "Unknown exception");
        break;
    }

    ctx->except = r;
    return FLOW_EXCEPTION;
}

/* System */

static int print(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    for (int i = 0; i < ac; ++i) {
        vmvar *an = local_var(ctx, i);
        print_obj(ctx, an);
    }
}

static int println(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    for (int i = 0; i < ac; ++i) {
        vmvar *an = local_var(ctx, i);
        print_obj(ctx, an);
    }
    printf("\n");
}

int System(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, print, print, 1)
    KL_SET_METHOD(o, println, println, 1)
    SET_OBJ(r, o);
    return 0;
}

/* SystemTimer */

static int SystemTimer_elapsed(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    if (a0->t == VAR_OBJ && 0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_VOIDP) {
        r->t = VAR_DBL;
        r->d = SystemTimer_elapsed_impl(a0->o->ary[0]->p);
    }
    return 0;
}

static int SystemTimer_restart(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    if (a0->t == VAR_OBJ && 0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_VOIDP) {
        r->t = VAR_INT64;
        r->i = 0;
        SystemTimer_restart_impl(a0->o->ary[0]->p);
    }
    return 0;
}

int SystemTimer_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    vmvar *timer = alcvar(ctx, VAR_VOIDP, 0);
    timer->p = SystemTimer_init();
    array_set(ctx, o, 0, timer);
    KL_SET_METHOD(o, elapsed, SystemTimer_elapsed, 1)
    KL_SET_METHOD(o, restart, SystemTimer_restart, 1)
    SET_OBJ(r, o);
    o->is_sysobj = 1;
    return 0;
}

int SystemTimer(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, create, SystemTimer_create, 0)
    SET_OBJ(r, o);
    return 0;
}

/* Fiber */

static int Fiber_resume(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    int e = 0;

    /* a0 should be the Fiber object. */
    vmvar *a0 = local_var(ctx, 0);
    vmfnc *f1 = a0->o->ary[0]->f;
    if (f1->created == 0 && f1->yield == 0) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_DEAD_FIBER_CALLED);
    }
    f1->created = 0;

    int args;
    if (ac == 2) {
        vmvar *vn = local_var(ctx, 1);
        push_var(ctx, vn, L0, __func__, __FILE__, __LINE__);
        args = 1;
    } else if (ac > 2) {
        vmvar *vn = alcvar_obj(ctx, alcobj(ctx));
        for (int i = 1; i < ac; ++i) {
            vmvar *aa = local_var(ctx, i);
            vmvar *ax = alcvar_initial(ctx);
            SHCOPY_VAR_TO(ctx, ax, aa);
            array_push(ctx, vn->o, ax);
        }
        push_var(ctx, vn, L0, __func__, __FILE__, __LINE__);
        args = 1;
    } else {
        args = 0;
    }
    int p = vstackp(ctx);
    vmfnc *callee = ctx->callee;
    ctx->callee = f1;
    e = ((vmfunc_t)(f1->f))(ctx, f1->lex, r, args);
    ctx->callee = callee;
    restore_vstackp(ctx, p);
    if (e == FLOW_YIELD) {
        e = 0;
    }

L0:;
    return e;
}

static int Fiber_isAlive(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    /* a0 should be the Fiber object. */
    vmvar *a0 = local_var(ctx, 0);
    vmfnc *f1 = a0->o->ary[0]->f;

    SET_I64(r, f1->created > 0 || f1->yield > 0);
    return 0;
}

static int Fiber_reset(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    /* a0 should be the Fiber object. */
    vmvar *a0 = local_var(ctx, 0);
    vmfnc *f1 = a0->o->ary[0]->f;

    f1->yield = 0;
    SET_I64(r, 1);
    return 0;
}

static int Fiber_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 1, VAR_FNC);

    vmvar *n0 = alcvar_initial(ctx);
    SHCOPY_VAR_TO(ctx, n0, a0);
    n0->f->created = 1;

    vmobj *o = alcobj(ctx);
    array_push(ctx, o, n0);
    KL_SET_PROPERTY_I(o, isFiber, 1)
    KL_SET_METHOD(o, resume, Fiber_resume, 0)
    KL_SET_METHOD(o, isAlive, Fiber_isAlive, 0)
    KL_SET_METHOD(o, reset, Fiber_reset, 0)
    SET_OBJ(r, o);
    o->is_sysobj = 1;

    return 0;
}

int Fiber(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, create, Fiber_create, 1)
    SET_OBJ(r, o);
    return 0;
}

/* Math */

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

/* Integer */

int Integer_next(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 1, VAR_INT64);
    SET_I64(r, a0->i + 1);
    return 0;
}

extern int Integer_times(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);

int Integer(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->i = o;
    KL_SET_METHOD(o, times, Integer_times, 2)
    KL_SET_METHOD(o, next, Integer_next, 2)

    KL_SET_METHOD(o, acos, Math_acos, 1)
    KL_SET_METHOD(o, asin, Math_asin, 1)
    KL_SET_METHOD(o, atan, Math_atan, 1)
    KL_SET_METHOD(o, atan2, Math_atan2, 2)
    KL_SET_METHOD(o, cos, Math_cos, 1)
    KL_SET_METHOD(o, sin, Math_sin, 1)
    KL_SET_METHOD(o, tan, Math_tan, 1)
    KL_SET_METHOD(o, cosh, Math_cosh, 1)
    KL_SET_METHOD(o, sinh, Math_sinh, 1)
    KL_SET_METHOD(o, tanh, Math_tanh, 1)
    KL_SET_METHOD(o, exp, Math_exp, 1)
    KL_SET_METHOD(o, ldexp, Math_ldexp, 2)
    KL_SET_METHOD(o, log, Math_log, 1)
    KL_SET_METHOD(o, log10, Math_log10, 1)
    KL_SET_METHOD(o, pow, Math_pow, 2)
    KL_SET_METHOD(o, sqrt, Math_sqrt, 1)
    KL_SET_METHOD(o, ceil, Math_ceil, 1)
    KL_SET_METHOD(o, fabs, Math_fabs, 1)
    KL_SET_METHOD(o, floor, Math_floor, 1)
    KL_SET_METHOD(o, fmod, Math_fmod, 2)
    KL_SET_METHOD(o, random, Math_random, 0)
    SET_OBJ(r, o);
    return 0;
}

/* Double */

int Double(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->d = o;
    KL_SET_METHOD(o, acos, Math_acos, 1)
    KL_SET_METHOD(o, asin, Math_asin, 1)
    KL_SET_METHOD(o, atan, Math_atan, 1)
    KL_SET_METHOD(o, atan2, Math_atan2, 2)
    KL_SET_METHOD(o, cos, Math_cos, 1)
    KL_SET_METHOD(o, sin, Math_sin, 1)
    KL_SET_METHOD(o, tan, Math_tan, 1)
    KL_SET_METHOD(o, cosh, Math_cosh, 1)
    KL_SET_METHOD(o, sinh, Math_sinh, 1)
    KL_SET_METHOD(o, tanh, Math_tanh, 1)
    KL_SET_METHOD(o, exp, Math_exp, 1)
    KL_SET_METHOD(o, ldexp, Math_ldexp, 2)
    KL_SET_METHOD(o, log, Math_log, 1)
    KL_SET_METHOD(o, log10, Math_log10, 1)
    KL_SET_METHOD(o, pow, Math_pow, 2)
    KL_SET_METHOD(o, sqrt, Math_sqrt, 1)
    KL_SET_METHOD(o, ceil, Math_ceil, 1)
    KL_SET_METHOD(o, fabs, Math_fabs, 1)
    KL_SET_METHOD(o, floor, Math_floor, 1)
    KL_SET_METHOD(o, fmod, Math_fmod, 2)
    KL_SET_METHOD(o, random, Math_random, 0)
    SET_OBJ(r, o);
    return 0;
}

/* String */

int String(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->s = o;
    SET_OBJ(r, o);
    return 0;
}

/* Binary */

int Binary(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->s = o;
    SET_OBJ(r, o);
    return 0;
}

/* Array/Object */

extern int Array_each(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);

int Array_size(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 1, VAR_OBJ);
    SET_I64(r, a0->o->idxsz);
    return 0;
}

int Object_keys(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 1, VAR_OBJ);
    vmobj *keys = object_get_keys(ctx, a0->o);
    SET_OBJ(r, keys);
    return 0;
}

int Array(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->o = o;
    KL_SET_METHOD(o, size, Array_size, 1)
    KL_SET_METHOD(o, each, Array_each, 1)
    KL_SET_METHOD(o, keys, Object_keys, 1)
    SET_OBJ(r, o);
    return 0;
}
