#ifndef KILITE_AMALGAMATION
#include "common.h"
#include "lib.h"
#endif

/* This is the prototype that the functions written here will need. */

extern void *SystemTimer_init(void);
extern void SystemTimer_restart_impl(void *p);
extern double SystemTimer_elapsed_impl(void *p);
extern int Array_sort(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);

/* Basic functions */

void _putchar(char ch)
{
    putchar(ch);
}

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
            if (func->s->hd[0] == '<') {
                printf("        at %s(%s:%" PRId64 ")\n", func->s->hd, file->s->hd, line->i);
            } else {
                printf("        at function %s(%s:%" PRId64 ")\n", func->s->hd, file->s->hd, line->i);
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
        const char *t = type->s->hd;
        vmvar *what = hashmap_search(e->o, "_what");
        const char *w = (what && what->t == VAR_STR) ? what->s->hd : "No message";
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
    o->is_sysobj = 1;
    vmvar *type = alcvar_str(ctx, name);
    hashmap_set(ctx, o, "_type", type);
    KL_SET_METHOD(o, type, Exception_type, lex, 1)
    KL_SET_METHOD(o, printStackTrace, Exception_printStackTrace, lex, 1)
    SET_OBJ(r, o);
    return 0;
}

int RuntimeException_create(vmctx *ctx, vmfrm *lex, vmvar *r, const char *name, const char *msg)
{
    Exception_create(ctx, lex, r, name);
    vmobj *o = r->o;
    hashmap_set(ctx, o, "_what", alcvar_str(ctx, msg));
    KL_SET_METHOD(o, what, Exception_what, lex, 1)
    return 0;
}

int RuntimeException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *msg = ac < 1 ? NULL : local_var(ctx, 0);
    if (msg && msg->t == VAR_STR) {
        RuntimeException_create(ctx, lex, r, "RuntimeException", msg->s->hd);
    } else {
        RuntimeException_create(ctx, lex, r, "RuntimeException", "Unknown");
    }
    return 0;
}

int NotImplementedException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "NotImplementedException", ctx->msgbuf ? ctx->msgbuf : "Function not implemented");
    return 0;
}

int StackOverflowException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "StackOverflowException", ctx->msgbuf ? ctx->msgbuf : "System stack has been exhausted");
    return 0;
}

int DivideByZeroException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "DivideByZeroException", ctx->msgbuf ? ctx->msgbuf : "Divide by zero");
    return 0;
}

int UnsupportedOperationException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "UnsupportedOperationException", ctx->msgbuf ? ctx->msgbuf : "Unsupported operation");
    return 0;
}

int MethodMissingException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "MethodMissingException", ctx->msgbuf ? ctx->msgbuf : "Method missing");
    return 0;
}

int NoMatchingPatternException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "NoMatchingPatternException", ctx->msgbuf ? ctx->msgbuf : "Pattern not matched");
    return 0;
}

int TooFewArgumentsException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "TooFewArgumentsException", ctx->msgbuf ? ctx->msgbuf : "Too few arguments");
    return 0;
}

int TypeMismatchException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "TypeMismatchException", ctx->msgbuf ? ctx->msgbuf : "Type mismatch");
    return 0;
}

int InvalidFiberStateException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "InvalidFiberStateException", ctx->msgbuf ? ctx->msgbuf : "Invalid fiber state");
    return 0;
}

int DeadFiberCalledException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "DeadFiberCalledException", ctx->msgbuf ? ctx->msgbuf : "Dead fiber called");
    return 0;
}

int RangeErrorException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "RangeErrorException", ctx->msgbuf ? ctx->msgbuf : "Range error");
    return 0;
}

int OutOfRangeErrorException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "OutOfRangeErrorException", ctx->msgbuf ? ctx->msgbuf : "Out of range error");
    return 0;
}

int TooDeepException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "TooDeepException", ctx->msgbuf ? ctx->msgbuf : "Too deep");
    return 0;
}

int XmlErrorException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "XmlErrorException", ctx->msgbuf ? ctx->msgbuf : "Xml error");
    return 0;
}

int FileErrorException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "FileErrorException", ctx->msgbuf ? ctx->msgbuf : "File error");
    return 0;
}

int ZipErrorException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "ZipErrorException", ctx->msgbuf ? ctx->msgbuf : "Zip error");
    return 0;
}

int RegexErrorException(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    RuntimeException_create(ctx, lex, r, "RegexErrorException", ctx->msgbuf ? ctx->msgbuf : "Regular expression error");
    return 0;
}

/* Make exception */

int throw_system_exception(int line, vmctx *ctx, int id, const char *msg)
{
    ctx->msgbuf = msg;
    vmvar *r = alcvar_initial(ctx);
    switch (id) {
    case EXCEPT_EXCEPTION:
        RuntimeException_create(ctx, NULL, r, "SystemException", msg ? msg : "Unknown exception");
        break;
    case EXCEPT_NOT_IMPLEMENTED:
        NotImplementedException(ctx, NULL, r, 0);
        break;
    case EXCEPT_RUNTIME_EXCEPTION:
        RuntimeException_create(ctx, NULL, r, "RuntimeException", msg ? msg : "Unknown exception");
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
    case EXCEPT_RANGE_ERROR:
        RangeErrorException(ctx, NULL, r, 0);
        break;
    case EXCEPT_OUT_OF_RANGE_ERROR:
        OutOfRangeErrorException(ctx, NULL, r, 0);
        break;
    case EXCEPT_TOO_DEEP:
        TooDeepException(ctx, NULL, r, 0);
        break;
    case EXCEPT_XML_ERROR:
        XmlErrorException(ctx, NULL, r, 0);
        break;
    case EXCEPT_FILE_ERROR:
        FileErrorException(ctx, NULL, r, 0);
        break;
    case EXCEPT_ZIP_ERROR:
        ZipErrorException(ctx, NULL, r, 0);
        break;
    case EXCEPT_REGEX_ERROR:
        RegexErrorException(ctx, NULL, r, 0);
        break;
    default:
        RuntimeException_create(ctx, NULL, r, "SystemException", msg ? msg : "Unknown exception");
        break;
    }

    ctx->msgbuf = NULL;
    ctx->except = r;
    return FLOW_EXCEPTION;
}

/* True/False */

int True(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_PROPERTY_I(o, _False, 0)
    SET_OBJ(r, o);
    return 0;
}

int False(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_PROPERTY_I(o, _False, 1)
    SET_OBJ(r, o);
    return 0;
}

/* System */

static int System_print(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    for (int i = 0; i < ac; ++i) {
        vmvar *an = local_var(ctx, i);
        print_obj(ctx, an);
    }
    r->t = VAR_UNDEF;
    return 0;
}

static int System_println(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    for (int i = 0; i < ac; ++i) {
        vmvar *an = local_var(ctx, i);
        print_obj(ctx, an);
    }
    printf("\n");
    r->t = VAR_UNDEF;
    return 0;
}

int System(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, print, System_print, lex, 1)
    KL_SET_METHOD(o, println, System_println, lex, 1)
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
    o->is_sysobj = 1;
    vmvar *timer = alcvar(ctx, VAR_VOIDP, 0);
    timer->p = SystemTimer_init();
    array_set(ctx, o, 0, timer);
    KL_SET_METHOD(o, elapsed, SystemTimer_elapsed, lex, 1)
    KL_SET_METHOD(o, restart, SystemTimer_restart, lex, 1)
    SET_OBJ(r, o);
    return 0;
}

int SystemTimer(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, create, SystemTimer_create, lex, 0)
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
        return throw_system_exception(__LINE__, ctx, EXCEPT_DEAD_FIBER_CALLED, NULL);
    }
    f1->created = 0;

    int args;
    if (f1->yield == 0) {
        args = ac - 1;
        vmvar **aa = alloca(args * sizeof(vmvar *));
        for (int i = 1; i < ac; ++i) {
            aa[i-1] = local_var(ctx, i);
        }
        for (int i = ac - 2; i >= 0; --i) {
            push_var(ctx, aa[i], L0, __func__, __FILE__, __LINE__);
        }
    } else {
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
    DEF_ARG(a0, 0, VAR_FNC);

    vmvar *n0 = alcvar_initial(ctx);
    SHCOPY_VAR_TO(ctx, n0, a0);
    n0->f->created = 1;

    vmobj *o = alcobj(ctx);
    o->is_sysobj = 1;
    array_push(ctx, o, n0);
    KL_SET_PROPERTY_I(o, isFiber, 1)
    KL_SET_METHOD(o, resume, Fiber_resume, lex, 0)
    KL_SET_METHOD(o, isAlive, Fiber_isAlive, lex, 0)
    KL_SET_METHOD(o, reset, Fiber_reset, lex, 0)
    SET_OBJ(r, o);

    return 0;
}

int Fiber(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, create, Fiber_create, lex, 1)
    SET_OBJ(r, o);
    return 0;
}

/* Range for integer special */

static int iRange_reset(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    vmvar *e = a0->o->ary[0];
    vmvar *c = a0->o->ary[3];
    SHCOPY_VAR_TO(ctx, c, e);
    SET_OBJ(r, a0->o);
    return 0;
}

static int iRange_next(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    /* a0 should be the iRange object. */
    vmvar *a0 = local_var(ctx, 0);
    vmvar *e = a0->o->ary[3];
    SHCOPY_VAR_TO(ctx, r, e);
    if (e->t == VAR_INT64) {
        ++(e->i);
    }
    return 0;
}

static int iRange_begin(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    vmvar *e = a0->o->ary[0];
    if (e->t != VAR_INT64) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RANGE_ERROR, NULL);
    }
    SHCOPY_VAR_TO(ctx, r, e);
    return 0;
}

static int iRange_end(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    vmvar *e = a0->o->ary[1];
    if (e->t != VAR_INT64) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RANGE_ERROR, NULL);
    }
    SHCOPY_VAR_TO(ctx, r, e);
    return 0;
}

static int iRange_isEnded(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    /* a0 should be the iRange object. */
    vmvar *a0 = local_var(ctx, 0);
    vmvar *e = a0->o->ary[3];
    SHCOPY_VAR_TO(ctx, r, e);
    vmvar *end = a0->o->ary[1];
    if (end->t == VAR_INT64) {
        int exclusive = a0->o->ary[2]->i;
        int endi = end->i - exclusive + 1;
        SET_I64(r, endi < e->i);
    } else {
        SET_I64(r, 0);
    }
    return 0;
}

static int iRange_isEndExcluded(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    vmvar *e = a0->o->ary[2];
    SHCOPY_VAR_TO(ctx, r, e);
    return 0;
}

static int iRange_includes(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    DEF_ARG(a1, 1, VAR_INT64);
    vmvar *beg = a0->o->ary[0];
    vmvar *end = a0->o->ary[1];
    vmvar *ex = a0->o->ary[2];
    if (ex->t != VAR_INT64) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RANGE_ERROR, NULL);
    }
    if (beg->t == VAR_UNDEF && end->t == VAR_UNDEF) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RANGE_ERROR, NULL);
    }
    int excl = ex->i;
    int i = a1->i;
    if (beg->t == VAR_UNDEF) {
        int e = end->i - excl;
        SET_I64(r, (i <= e));
    } else if (end->t == VAR_UNDEF) {
        int b = beg->i;
        SET_I64(r, b <= i);
    } else {
        int b = beg->i;
        int e = end->i - excl;
        SET_I64(r, (b <= i && i <= e));
    }

    return 0;
}

static int iRange_toArray(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    vmvar *beg = a0->o->ary[0];
    vmvar *end = a0->o->ary[1];
    vmvar *ex = a0->o->ary[2];
    if (ex->t != VAR_INT64) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RANGE_ERROR, NULL);
    }
    if (beg->t == VAR_UNDEF || end->t == VAR_UNDEF) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RANGE_ERROR, NULL);
    }

    int bi = beg->i;
    int ei = end->i - ex->i;
    if (ei < bi) {
        int i = bi;
        bi = ei;
        ei = i;
    }
    vmobj *o = alcobj(ctx);
    for (int i = bi; i <= ei; ++i) {
        array_push(ctx, o, alcvar_int64(ctx, i, 0));
    }

    SET_OBJ(r, o);
    return 0;
}

static int iRange_sort(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    DEF_ARG(a1, 1, VAR_FNC);
    vmvar *beg = a0->o->ary[0];
    vmvar *end = a0->o->ary[1];
    vmvar *ex = a0->o->ary[2];
    if (ex->t != VAR_INT64) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RANGE_ERROR, NULL);
    }
    if (beg->t == VAR_UNDEF || end->t == VAR_UNDEF) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RANGE_ERROR, NULL);
    }

    int e = 0;
    int bi = beg->i;
    int excl = ex->i;
    int ei = end->i - excl;

    vmobj *a = alcobj(ctx);
    for (int i = bi; i <= ei; ++i) {
        array_push(ctx, a, alcvar_int64(ctx, i, 0));
    }

    vmfnc *f = a1->f;

    int pp = vstackp(ctx);
    push_var_f(ctx, f, L0, "iRange_sort", __FILE__, 0);
    push_var_o(ctx, a, L0, "iRange_sort", __FILE__, 0);

    vmvar *sort = hashmap_search(ctx->o, "sort");
    if (!sort || sort->t != VAR_FNC || !sort->f || !sort->f->f) {
        restore_vstackp(ctx, pp);
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL);
    }
    vmfnc *f1 = sort->f;

    vmfnc *callee = ctx->callee;
    ctx->callee = f1;
    e = ((vmfunc_t)(f1->f))(ctx, f1->lex, r, 2);
    ctx->callee = callee;

L0:;
    restore_vstackp(ctx, pp);
    return e;
}

int Range_create_i(vmctx *ctx, vmfrm *lex, vmvar *r, int *beg, int *end, int excl)
{
    vmvar *n0 = beg ? alcvar_int64(ctx, *beg, 0) : alcvar_initial(ctx);
    vmvar *n1 = end ? alcvar_int64(ctx, *end, 0) : alcvar_initial(ctx);
    vmvar *n2 = alcvar_int64(ctx, excl, 0);
    vmvar *n3 = alcvar_initial(ctx);
    SHCOPY_VAR_TO(ctx, n3, n0);

    vmobj *o = alcobj(ctx);
    o->is_sysobj = 1;
    array_push(ctx, o, n0);
    array_push(ctx, o, n1);
    array_push(ctx, o, n2);
    array_push(ctx, o, n3);
    KL_SET_PROPERTY_I(o, isRange, 1)
    KL_SET_METHOD(o, next, iRange_reset, lex, 0)
    KL_SET_METHOD(o, next, iRange_next, lex, 0)
    KL_SET_METHOD(o, begin, iRange_begin, lex, 0)
    KL_SET_METHOD(o, end, iRange_end, lex, 0)
    KL_SET_METHOD(o, isEnded, iRange_isEnded, lex, 0)
    KL_SET_METHOD(o, includes, iRange_includes, lex, 1)
    KL_SET_METHOD(o, isEndExcluded, iRange_isEndExcluded, lex, 0)
    KL_SET_METHOD(o, sort, iRange_sort, lex, 0)
    KL_SET_METHOD(o, toArray, iRange_toArray, lex, 0)
    SET_OBJ(r, o);

    return 0;
}

static int iRange_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG_OR_UNDEF(a0, 0, VAR_INT64);
    DEF_ARG_OR_UNDEF(a1, 1, VAR_INT64);
    DEF_ARG_OR_UNDEF(a2, 2, VAR_INT64);

    int beg = a0->i;
    int end = a1->i;
    int excl = a2->t == VAR_INT64 ? a2->i : 0;
    return Range_create_i(ctx, lex, r,
        a0->t == VAR_UNDEF ? NULL : &beg,
        a1->t == VAR_UNDEF ? NULL : &end,
        excl
    );
}

int Range_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG_ANY(a0, 0);
    DEF_ARG_ANY(a1, 1);
    if ((a0->t == VAR_INT64 || a0->t == VAR_UNDEF) && (a1->t == VAR_INT64 || a1->t == VAR_UNDEF)) {
        iRange_create(ctx, lex, r, ac);
    } else {
        return throw_system_exception(__LINE__, ctx, EXCEPT_NOT_IMPLEMENTED, NULL);
    }
    return 0;
}

int Range(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, create, Range_create, lex, 1)
    SET_OBJ(r, o);
    return 0;
}

/* Iterator */

static int IteratorArray_reset(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    vmvar *e = a0->o->ary[1];
    if (e->t == VAR_INT64) {
        e->i = 0;
    }
    SET_OBJ(r, a0->o);
    return 0;
}

static int IteratorArray_next(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    vmvar *e = a0->o->ary[1];
    if (e->t == VAR_INT64) {
        vmvar *n = a0->o->ary[0];
        int idx = e->i;
        if (n->t == VAR_OBJ && idx < n->o->idxsz) {
            vmvar *v = n->o->ary[idx];
            COPY_VAR_TO(ctx, r, v);
        } else {
            SET_I64(r, 0);
        }
        (e->i)++;
    } else {
        SET_I64(r, 0);
    }
    return 0;
}

static int IteratorArray_isEnded(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    vmvar *e = a0->o->ary[1];
    if (e->t == VAR_INT64) {
        vmvar *n = a0->o->ary[0];
        int idx = e->i;
        if (n->t == VAR_OBJ && idx <= n->o->idxsz) {
            SET_I64(r, 0);
        } else {
            SET_I64(r, 1);
        }
    } else {
        SET_I64(r, 1);
    }
    return 0;
}

static int IteratorObject_next(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmvar *a0 = local_var(ctx, 0);
    vmvar *n = a0->o->ary[2];
    vmvar *e = a0->o->ary[1];
    vmvar *k = NULL;
    vmvar *c = NULL;
    if (n && n->t == VAR_OBJ && e->t == VAR_INT64) {
        int idx = e->i;
        vmvar *keys = a0->o->ary[0];
        if (keys && keys->t == VAR_OBJ && idx < keys->o->idxsz) {
            k = keys->o->ary[idx];
            if (k && k->t == VAR_STR) {
                char *s = k->s->hd;
                c = hashmap_search(n->o, s);
            }
        }
    }

    if (k && c) {
        vmobj *o = alcobj(ctx);
        vmvar *v = alcvar_obj(ctx, o);
        array_push(ctx, o, k);
        array_push(ctx, o, c);
        COPY_VAR_TO(ctx, r, v);
    } else {
        SET_I64(r, 0);
    }
    (e->i)++;
    return 0;
}

int Iterator_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    if (!r || r->t != VAR_OBJ) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }

    vmvar *isRange = hashmap_search(r->o, "isRange");
    if (isRange) {
        vmvar *e = r->o->ary[0];
        vmvar *c = r->o->ary[3];
        SHCOPY_VAR_TO(ctx, c, e);
    } else {
        vmvar *n0 = alcvar_initial(ctx);
        SHCOPY_VAR_TO(ctx, n0, r);
        vmobj *o = alcobj(ctx);
        o->is_sysobj = 1;
        if (r->o->idxsz > 0) {
            array_push(ctx, o, n0);
            array_push(ctx, o, alcvar_int64(ctx, 0, 0));
            KL_SET_METHOD(o, next, IteratorArray_reset, lex, 0)
            KL_SET_METHOD(o, next, IteratorArray_next, lex, 0)
            KL_SET_METHOD(o, isEnded, IteratorArray_isEnded, lex, 0)
            SET_OBJ(r, o);
        } else {
            vmobj *keys = object_get_keys(ctx, n0->o);
            array_push(ctx, o, alcvar_obj(ctx, keys));
            array_push(ctx, o, alcvar_int64(ctx, 0, 0));
            array_push(ctx, o, n0);
            KL_SET_METHOD(o, next, IteratorArray_reset, lex, 0)
            KL_SET_METHOD(o, next, IteratorObject_next, lex, 0)
            KL_SET_METHOD(o, isEnded, IteratorArray_isEnded, lex, 0)
            SET_OBJ(r, o);
        }
    }

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

static int Integer_next(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG_INT(a0, 0);
    if (a0->t == VAR_INT64) {
        SET_I64(r, a0->i + 1);
    } else {
        BigZ b2 = BzFromInteger(1);
        r->t = VAR_BIG;
        r->bi = alcbgi_bigz(ctx, BzAdd(a0->bi->b, b2));
        BzFree(b2);
    }
    return 0;
}

static int digitch[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

static void Integer_toString_N(vmctx *ctx, vmstr *sv, int64_t iv, int n)
{
    if (iv >= n) {
        Integer_toString_N(ctx, sv, iv / n, n);
    }
    int r = iv % n;
    str_append_ch(ctx, sv, digitch[r]);
}

static int Integer_toString(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG_INT(a0, 0);
    DEF_ARG_OR_UNDEF(a1, 1, VAR_INT64);
    int radix = a1->t == VAR_INT64 ? a1->i : 10;
    r->t = VAR_STR;
    if (a0->t == VAR_INT64) {
        if (radix < 2 || 36 < radix) {
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, "Unsupported radix number");
        }
        switch (radix) {
        case 10: {
            char buf[32] = {0};
            sprintf(buf, "%" PRId64, a0->i);
            r->s = alcstr_str(ctx, buf);
            break;
        }
        case 16: {
            char buf[32] = {0};
            sprintf(buf, "%" PRIx64, a0->i);
            r->s = alcstr_str(ctx, buf);
            break;
        }
        default:
            r->s = alcstr_str(ctx, "");
            Integer_toString_N(ctx, r->s, a0->i, radix);
            break;
        }
    } else {
        char *buf = BzToString(a0->bi->b, radix, 0);
        r->s = alcstr_str(ctx, buf);
        BzFreeString(buf);
    }

    return 0;
}

static int Integer_toDouble(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG_INT(a0, 0);
    r->t = VAR_INT64;
    if (a0->t == VAR_INT64) {
        r->d = (double)(a0->i);
    } else {
        r->d = BzToDouble(a0->bi->b);
    }
    return 0;
}

int Integer_abs(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(v1, 0, VAR_INT64)
    SET_I64((r), v1->i < 0 ? -(v1->i) : v1->i);
    return 0;
}

extern int Integer_times(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Integer_upto(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Integer_downto(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);

int Integer(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->i = o;
    KL_SET_METHOD(o, toString, Integer_toString, lex, 2)
    KL_SET_METHOD(o, toDouble, Integer_toDouble, lex, 1)
    KL_SET_METHOD(o, times, Integer_times, lex, 2)
    KL_SET_METHOD(o, upto, Integer_upto, lex, 2)
    KL_SET_METHOD(o, downto, Integer_downto, lex, 2)
    KL_SET_METHOD(o, next, Integer_next, lex, 2)

    KL_SET_METHOD(o, acos, Math_acos, lex, 1)
    KL_SET_METHOD(o, asin, Math_asin, lex, 1)
    KL_SET_METHOD(o, atan, Math_atan, lex, 1)
    KL_SET_METHOD(o, atan2, Math_atan2, lex, 2)
    KL_SET_METHOD(o, cos, Math_cos, lex, 1)
    KL_SET_METHOD(o, sin, Math_sin, lex, 1)
    KL_SET_METHOD(o, tan, Math_tan, lex, 1)
    KL_SET_METHOD(o, cosh, Math_cosh, lex, 1)
    KL_SET_METHOD(o, sinh, Math_sinh, lex, 1)
    KL_SET_METHOD(o, tanh, Math_tanh, lex, 1)
    KL_SET_METHOD(o, exp, Math_exp, lex, 1)
    KL_SET_METHOD(o, ldexp, Math_ldexp, lex, 2)
    KL_SET_METHOD(o, log, Math_log, lex, 1)
    KL_SET_METHOD(o, log10, Math_log10, lex, 1)
    KL_SET_METHOD(o, pow, Math_pow, lex, 2)
    KL_SET_METHOD(o, sqrt, Math_sqrt, lex, 1)
    KL_SET_METHOD(o, ceil, Math_ceil, lex, 1)
    KL_SET_METHOD(o, abs, Integer_abs, lex, 1)
    KL_SET_METHOD(o, fabs, Math_fabs, lex, 1)
    KL_SET_METHOD(o, floor, Math_floor, lex, 1)
    KL_SET_METHOD(o, fmod, Math_fmod, lex, 2)
    KL_SET_METHOD(o, random, Math_random, lex, 0)
    SET_OBJ(r, o);
    return 0;
}

/* Double */

static int Double_toString(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_DBL);
    r->t = VAR_STR;
    char buf[32] = {0};
    sprintf(buf, "%g", a0->d);
    r->s = alcstr_str(ctx, buf);
    return 0;
}

static int Double_toInteger(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_DBL);
    r->t = VAR_INT64;
    r->i = (int64_t)(a0->d);
    return 0;
}

int Double(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->d = o;
    KL_SET_METHOD(o, toString, Double_toString, lex, 1)
    KL_SET_METHOD(o, toInt, Double_toInteger, lex, 1)
    KL_SET_METHOD(o, toInteger, Double_toInteger, lex, 1)
    KL_SET_METHOD(o, acos, Math_acos, lex, 1)
    KL_SET_METHOD(o, asin, Math_asin, lex, 1)
    KL_SET_METHOD(o, atan, Math_atan, lex, 1)
    KL_SET_METHOD(o, atan2, Math_atan2, lex, 2)
    KL_SET_METHOD(o, cos, Math_cos, lex, 1)
    KL_SET_METHOD(o, sin, Math_sin, lex, 1)
    KL_SET_METHOD(o, tan, Math_tan, lex, 1)
    KL_SET_METHOD(o, cosh, Math_cosh, lex, 1)
    KL_SET_METHOD(o, sinh, Math_sinh, lex, 1)
    KL_SET_METHOD(o, tanh, Math_tanh, lex, 1)
    KL_SET_METHOD(o, exp, Math_exp, lex, 1)
    KL_SET_METHOD(o, ldexp, Math_ldexp, lex, 2)
    KL_SET_METHOD(o, log, Math_log, lex, 1)
    KL_SET_METHOD(o, log10, Math_log10, lex, 1)
    KL_SET_METHOD(o, pow, Math_pow, lex, 2)
    KL_SET_METHOD(o, sqrt, Math_sqrt, lex, 1)
    KL_SET_METHOD(o, ceil, Math_ceil, lex, 1)
    KL_SET_METHOD(o, abs, Math_fabs, lex, 1)
    KL_SET_METHOD(o, fabs, Math_fabs, lex, 1)
    KL_SET_METHOD(o, floor, Math_floor, lex, 1)
    KL_SET_METHOD(o, fmod, Math_fmod, lex, 2)
    KL_SET_METHOD(o, random, Math_random, lex, 0)
    SET_OBJ(r, o);
    return 0;
}

/* String */

int String_length(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(sv0, 0, VAR_STR);
    SET_I64(r, sv0->s->len);
    return 0;
}

int String_trim(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(sv0, 0, VAR_STR);
    vmstr *s = str_dup(ctx, sv0->s);
    str_trim(ctx, s, " \t\r\n");
    SET_SV(r, s);
    return 0;
}

int String_subString(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(sv0, 0, VAR_STR);
    const char *str = sv0->s->hd;
    DEF_ARG(bv, 1, VAR_INT64);
    int beg = bv->i;
    DEF_ARG_OR_UNDEF(lv, 2, VAR_INT64);
    int len = lv->t == VAR_UNDEF ? -1 : lv->i;

    vmstr *sv = len < 0 ? alcstr_str(ctx, str + beg) : alcstr_str_len(ctx, str + beg, len);
    SET_SV(r, sv);
    return 0;
}

int String_splitByString(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(sv0, 0, VAR_STR);
    const char *str = sv0->s->hd;
    DEF_ARG(sv1, 1, VAR_STR);
    const char *cond = sv1->s->hd;

    if (!str) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RUNTIME_EXCEPTION, "No string value");
    }
    if (!cond) {
        cond = "";
    }

    vmobj *res = alcobj(ctx);
    if (cond[0] == 0) {
        const char *p = str;
        while (*p) {
            int b = g_utf8bytes[*p & 0xff];
            char buf[8] = {0};
            for (int i = 0; *p && i < b; ++i) {
                buf[i] = *p;
                ++p;
            }
            array_push(ctx, res, alcvar_str(ctx, buf));
        }
    } else {
        vmstr *sv = alcstr_str(ctx, "");
        int width = strlen(cond);
        const char *start = str;
        const char *p = strstr(str, cond);
        const char *end = start + strlen(str);
        while (p) {
            str_append(ctx, sv, start, p - start);
            array_push(ctx, res, alcvar_sv(ctx, sv));
            sv = alcstr_str(ctx, "");
            start = p + width;
            if (start < end) {
                p = strstr(start, cond);
            } else {
                p = NULL;
            }
        }
        if (start < end) {
            str_append_cp(ctx, sv, start);
        }
        array_push(ctx, res, alcvar_sv(ctx, sv));
    }

    SET_OBJ(r, res);
    return 0;
}

int String_replaceByString(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(sv0, 0, VAR_STR);
    const char *str = sv0->s->hd;
    DEF_ARG(sv1, 1, VAR_STR);
    const char *cond = sv1->s->hd;
    DEF_ARG(sv2, 2, VAR_STR);
    const char *newstr = sv2->s->hd;

    if (!str || !cond) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_RUNTIME_EXCEPTION, "No string value");
    }
    if (!newstr) {
        newstr = "";
    }

    vmstr *sv = alcstr_str(ctx, "");
    if (cond[0] == 0) {
        const char *p = str;
        while (*p) {
            char buf[2] = {*p, 0};
            str_append_cp(ctx, sv, buf);
            str_append_cp(ctx, sv, newstr);
            ++p;
        }
    } else {
        int width = strlen(cond);
        const char *start = str;
        const char *p = strstr(str, cond);
        const char *end = start + strlen(str);
        while (p) {
            str_append(ctx, sv, start, p - start);
            str_append_cp(ctx, sv, newstr);
            start = p + width;
            if (start < end) {
                p = strstr(start, cond);
            } else {
                p = NULL;
            }
        }
        if (start < end) {
            str_append_cp(ctx, sv, start);
        }
    }

    SET_SV(r, sv);
    return 0;
}

int String_toDouble(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    double d = strtod(a0->s->hd, NULL);
    SET_DBL(r, d);
    return 0;
}

int String_toInteger(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    DEF_ARG_OR_UNDEF(a1, 1, VAR_INT64);
    int radix = a1->t == VAR_INT64 ? a1->i : 10;

    int64_t i64 = strtoll(a0->s->hd, NULL, radix);
    SET_I64(r, i64);
    return 0;
}

int String_startsWith(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    DEF_ARG(a1, 1, VAR_STR);
    const char *s0 = a0->s->hd;
    int s0l = strlen(s0);
    const char *s1 = a1->s->hd;
    int s1l = strlen(s1);
    if (s0l < s1l) {
        SET_I64(r, 0);
    } else {
        SET_I64(r, strncmp(s0, s1, s1l));
    }
    return 0;
}

int String_endsWith(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    DEF_ARG(a1, 1, VAR_STR);
    const char *s0 = a0->s->hd;
    int s0l = strlen(s0);
    const char *s1 = a1->s->hd;
    int s1l = strlen(s1);
    if (s0l < s1l) {
        SET_I64(r, 0);
    } else {
        SET_I64(r, strcmp(s0 + s0l - s1l, s1));
    }
    return 0;
}

int String_find(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    DEF_ARG(a1, 1, VAR_STR);
    const char *s0 = a0->s->hd;
    const char *s1 = a1->s->hd;
    const char *p = strstr(s0, s1);
    if (p) {
        SET_I64(r, (int64_t)(p - s0));
    } else {
        SET_I64(r, -1);
    }
    return 0;
}

int String_parentPath(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    const char *s0 = a0->s->hd;
    const char *p = NULL;
    for (const char *pp = s0; *pp; ++pp) {
        if(*pp == '/' || *pp == '\\') {
            p = pp; // hold the last one.
        }
    }
    r->t = VAR_STR;
    if (p) {
        r->s = alcstr_str_len(ctx, s0, p - s0);
    } else {
        r->s = alcstr_str(ctx, "");
    }
    return 0;
}

int String_filename(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    const char *s0 = a0->s->hd;
    const char *p = s0;
    for (const char *pp = s0; *pp; ++pp) {
        if(*pp == '/' || *pp == '\\') {
            p = pp; // hold the last one.
        }
    }
    r->t = VAR_STR;
    r->s = alcstr_str(ctx, p + 1);
    return 0;
}

int String_stem(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    const char *s0 = a0->s->hd;
    const char *p = s0;
    const char *n = NULL;
    for (const char *pp = s0; *pp; ++pp) {
        if(*pp == '/' || *pp == '\\') {
            p = pp + 1; // hold the last one.
            n = NULL;
        }
        if(*pp == '.') {
            n = pp; // hold the last one.
        }
    }
    r->t = VAR_STR;
    if (n) {
        r->s = alcstr_str_len(ctx, p, n - p);
    } else {
        r->s = alcstr_str(ctx, p);
    }
    return 0;
}

int String_extension(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    const char *s0 = a0->s->hd;
    const char *n = NULL;
    for (const char *pp = s0; *pp; ++pp) {
        if(*pp == '/' || *pp == '\\') {
            n = NULL;
        }
        if(*pp == '.') {
            n = pp; // hold the last one.
        }
    }
    r->t = VAR_STR;
    if (n) {
        r->s = alcstr_str(ctx, n);
    } else {
        r->s = alcstr_str(ctx, "");
    }
    return 0;
}

extern int String_split(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int String_replace(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int String_each(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);

int String(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->s = o;
    KL_SET_METHOD(o, length, String_length, lex, 2)
    KL_SET_METHOD(o, trim, String_trim, lex, 2)
    KL_SET_METHOD(o, replace, String_replace, lex, 2)
    KL_SET_METHOD(o, split, String_split, lex, 2)
    KL_SET_METHOD(o, replaceByString, String_replaceByString, lex, 2)
    KL_SET_METHOD(o, splitByString, String_splitByString, lex, 2)
    KL_SET_METHOD(o, subString, String_subString, lex, 2)
    KL_SET_METHOD(o, toDouble, String_toDouble, lex, 1)
    KL_SET_METHOD(o, toInteger, String_toInteger, lex, 2)
    KL_SET_METHOD(o, toInt, String_toInteger, lex, 2)
    KL_SET_METHOD(o, startsWith, String_startsWith, lex, 2)
    KL_SET_METHOD(o, endsWith, String_endsWith, lex, 2)
    KL_SET_METHOD(o, parentPath, String_parentPath, lex, 1)
    KL_SET_METHOD(o, filename, String_filename, lex, 1)
    KL_SET_METHOD(o, stem, String_stem, lex, 1)
    KL_SET_METHOD(o, extension, String_extension, lex, 1)
    KL_SET_METHOD(o, find, String_find, lex, 2)
    KL_SET_METHOD(o, each, String_each, lex, 2)
    SET_OBJ(r, o);
    return 0;
}

/* Binary */

int Binary(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->b = o;
    SET_OBJ(r, o);
    return 0;
}

/* Array/Object */

static int Array_clone(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    r->t = VAR_OBJ;
    r->o = object_copy(ctx, a0->o);
    return 0;
}

static int Array_size(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    SET_I64(r, a0->o->idxsz);
    return 0;
}

static int Array_join(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_STR);
    SET_STR(r, "");
    vmobj *o = a0->o;
    int n = o->idxsz;
    if (n > 0) {
        add_v_v(ctx, r, r, o->ary[0]);
        for (int i = 1; i < n; ++i) {
            add_v_v(ctx, r, r, a1);
            add_v_v(ctx, r, r, o->ary[i]);
        }
    }
    return 0;
}

static int Array_unshift(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmobj *o = a0->o;
    for (int i = 1; i < ac; ++i) {
        vmvar *aa = local_var(ctx, i);
        array_unshift(ctx, o, copy_var(ctx, aa, 0));
    }
    return 0;
}

static int Array_shift(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG_OR_UNDEF(a1, 1, VAR_INT64);
    if (a1->t == VAR_UNDEF) {
        vmvar *v = array_shift(ctx, a0->o);
        SHCOPY_VAR_TO(ctx, r, v);
    } else {
        vmobj *o = array_shift_array(ctx, a0->o, a1->i);
        SET_OBJ(r, o);
    }
    return 0;
}

static int Array_push(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmobj *o = a0->o;
    for (int i = 1; i < ac; ++i) {
        vmvar *aa = local_var(ctx, i);
        array_push(ctx, o, copy_var(ctx, aa, 0));
    }
    return 0;
}

static int Array_pop(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG_OR_UNDEF(a1, 1, VAR_INT64);
    if (a1->t == VAR_UNDEF) {
        vmvar *v = array_pop(ctx, a0->o);
        SHCOPY_VAR_TO(ctx, r, v);
    } else {
        vmobj *o = array_pop_array(ctx, a0->o, a1->i);
        SET_OBJ(r, o);
    }
    return 0;
}

static int Array_flatten_impl(vmctx *ctx, vmvar *r, vmvar *a0, int level)
{
    if (100 < level) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_TOO_DEEP, NULL);
    }
    switch (a0->t) {
    case VAR_UNDEF: array_push(ctx, r->o, copy_var(ctx, a0, 0)); break;
    case VAR_INT64: array_push(ctx, r->o, copy_var(ctx, a0, 0)); break;
    case VAR_BIG:   array_push(ctx, r->o, copy_var(ctx, a0, 0)); break;
    case VAR_DBL:   array_push(ctx, r->o, copy_var(ctx, a0, 0)); break;
    case VAR_STR:   array_push(ctx, r->o, copy_var(ctx, a0, 0)); break;
    case VAR_BIN:   array_push(ctx, r->o, copy_var(ctx, a0, 0)); break;
    case VAR_FNC:   array_push(ctx, r->o, copy_var(ctx, a0, 0)); break;
    case VAR_OBJ: {
        vmobj *o = a0->o;
        for (int i = 0; i < o->idxsz; ++i) {
            int e = Array_flatten_impl(ctx, r, o->ary[i], level + 1);
            if (e != 0) {
                return e;
            }
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

static int Array_flatten(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    r->t = VAR_OBJ;
    r->o = alcobj(ctx);
    return Array_flatten_impl(ctx, r, a0, 0);
}

static int Array_reverse(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmobj *o = a0->o;
    vmobj *n = alcobj(ctx);
    int len = o->idxsz;
    for (int i = len - 1; i >= 0; --i) {
        array_push(ctx, n, copy_var(ctx, o->ary[i], 0));
    }
    SET_OBJ(r, n);
    return 0;
}

static int Object_keys(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmobj *keys = object_get_keys(ctx, a0->o);
    SET_OBJ(r, keys);
    return 0;
}

extern int Array_each(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_map(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_filter(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_reject(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_flatMap(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_findAll(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_reduce(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_all(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_any(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_partition(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_take(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_takeWhile(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_drop(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
extern int Array_dropWhile(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);

int Array(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->o = o;
    KL_SET_METHOD(o, each, Array_each, lex, 2)
    KL_SET_METHOD(o, push, Array_push, lex, 1)
    KL_SET_METHOD(o, pop, Array_pop, lex, 1)
    KL_SET_METHOD(o, unshift, Array_unshift, lex, 1)
    KL_SET_METHOD(o, shift, Array_shift, lex, 1)
    KL_SET_METHOD(o, map, Array_map, lex, 2)
    KL_SET_METHOD(o, collect, Array_map, lex, 2)
    KL_SET_METHOD(o, join, Array_join, lex, 2)
    KL_SET_METHOD(o, filter, Array_filter, lex, 2)
    KL_SET_METHOD(o, reject, Array_reject, lex, 2)
    KL_SET_METHOD(o, flatten, Array_flatten, lex, 2)
    KL_SET_METHOD(o, flatMap, Array_flatMap, lex, 2)
    KL_SET_METHOD(o, collectConcat, Array_flatMap, lex, 2)
    KL_SET_METHOD(o, findAll, Array_findAll, lex, 2)
    KL_SET_METHOD(o, select, Array_findAll, lex, 2)
    KL_SET_METHOD(o, reduce, Array_reduce, lex, 3)
    KL_SET_METHOD(o, inject, Array_reduce, lex, 3)
    KL_SET_METHOD(o, all, Array_all, lex, 2)
    KL_SET_METHOD(o, any, Array_any, lex, 2)
    KL_SET_METHOD(o, partition, Array_partition, lex, 2)
    KL_SET_METHOD(o, take, Array_take, lex, 2)
    KL_SET_METHOD(o, takeWhile, Array_takeWhile, lex, 2)
    KL_SET_METHOD(o, drop, Array_drop, lex, 2)
    KL_SET_METHOD(o, dropWhile, Array_dropWhile, lex, 2)
    KL_SET_METHOD(o, sort, Array_sort, lex, 2)
    KL_SET_METHOD(o, clone, Array_clone, lex, 1)
    KL_SET_METHOD(o, keys, Object_keys, lex, 1)
    KL_SET_METHOD(o, keySet, Object_keys, lex, 1)
    KL_SET_METHOD(o, size, Array_size, lex, 1)
    KL_SET_METHOD(o, length, Array_size, lex, 1)
    KL_SET_METHOD(o, reverse, Array_reverse, lex, 1)
    SET_OBJ(r, o);
    return 0;
}
