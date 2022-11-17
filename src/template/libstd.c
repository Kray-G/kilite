#include "common.h"

/* This is the prototype that the functions written here will need. */

extern void *SystemTimer_init(void);
extern void SystemTimer_restart_impl(void *p);
extern double SystemTimer_elapsed_impl(void *p);

/* Exceptions */

int exception_addtrace(vmctx *ctx, vmvar *e, const char *funcname, const char *filename, int linenum)
{
    if (e->t != VAR_OBJ) {
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

    int p = vstackp(ctx);
    for (int i = 1; i < ac; ++i) {
        vmvar *a = local_var(ctx, i);
        push_var(ctx, a, L0, "<libstd>", "libstd.c", __LINE__);
    }
    vmfnc *callee = ctx->callee;
    ctx->callee = f1;
    e = ((vmfunc_t)(f1->f))(ctx, f1->lex, r, ac - 1);
    ctx->callee = callee;
    restore_vstackp(ctx, p);
    CHECK_EXCEPTION(L0, "<libstd>", "libstd.c", __LINE__);
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
    if (ac < 1) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_TOO_FEW_ARGUMENTS);
    }
    vmvar *a0 = local_var(ctx, 0);
    if (a0->t != VAR_FNC) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH);
    }

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
