#include "common.h"

// ..\..\bin\c2m -c alloc.c gc.c init.c main.c util.c bigi.c str.c obj.c op.c lib\bign.c lib\bigz.c
// timecmd ..\..\bin\c2m alloc.bmir gc.bmir init.bmir main.bmir util.bmir bigi.bmir obj.bmir op.bmir str.bmir bign.bmir bigz.bmir sample.c -eg
// cl -O2 -Fesample.exe alloc.c gc.c init.c main.c util.c bigi.c str.c obj.c op.c lib\bign.c lib\bigz.c sample.c

// ../../bin/c2m -c alloc.c gc.c init.c main.c util.c bigi.c str.c obj.c op.c lib/bign.c lib/bigz.c
// time ../../bin/c2m alloc.bmir gc.bmir init.bmir main.bmir util.bmir bigi.bmir obj.bmir op.bmir str.bmir bign.bmir bigz.bmir sample.c -eg
// cl -O2 -Fesample.exe alloc.c gc.c init.c main.c util.c bigi.c str.c obj.c op.c lib/bign.c lib/bigz.c sample.c

void setup_context(vmctx *ctx)
{
    ctx->print_result = 0;
    ctx->verbose = 0;
}

int func_fib(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    GC_CHECK(ctx);

    // vmfrm *frm = alcfrm(ctx, 1);
    // push_frm(ctx, frm);

    /* Local variable(s) */
    int p, e = 0;
    alloc_var(ctx, 4);
    vmvar *n0 = local_var(ctx, 0);
    vmvar *n1 = local_var(ctx, 1);
    vmvar *n2 = local_var(ctx, 2);
    vmvar *n3 = local_var(ctx, 3);
    SET_ARGVAR(0, 4);   // Why it is 4 is because 4 spaces are allocated for local variables.
    // SET_ARGVAR(1, 4);
    // SET_ARGVAR(2, 4);
    // SET_ARGVAR(3, 4);

    OP_LT_V_I(ctx, n1, n0, 2);
    OP_JMP_IF_FALSE(n1, COND2);

COND1:;
    GC_CHECK(ctx);
    COPY_VAR_TO(ctx, r, n0);
    goto END;

COND2:;
    vmfnc *f1 = lex->v[0]->f;
    GC_CHECK(ctx);

    OP_SUB_V_I(ctx, n1, n0, 2);

    p = vstackp(ctx);
    push_var(ctx, n1);
    e = ((vmfunc_t)(f1->f))(ctx, f1->lex, n2, 1);
    restore_vstackp(ctx, p);
    if (e) goto END;

    OP_SUB_V_I(ctx, n1, n0, 1);

    p = vstackp(ctx);
    push_var(ctx, n1);
    e = ((vmfunc_t)(f1->f))(ctx, f1->lex, n3, 1);
    restore_vstackp(ctx, p);
    if (e) goto END;

    OP_ADD(ctx, r, n2, n3);

    goto END;

END:
    reduce_vstackp(ctx, 4);
    /* This frame can be released because this function is not being a lexical scope. */
    // pop_frm(ctx);
    // pbakfrm(ctx, frm);
    return e;
}

/* function:fib */
int func_fib2(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    GC_CHECK(ctx);
    int p, e = 0;
    const int allocated_local = 5;
    alloc_var(ctx, 5);
    vmvar *n0 = local_var(ctx, 0);
    vmvar *n1 = local_var(ctx, 1);
    vmvar *n2 = local_var(ctx, 2);
    vmvar *n3 = local_var(ctx, 3);
    vmvar *n4 = local_var(ctx, 4);
    SET_ARGVAR(0, 5);

    vmfnc *f1 = (lex->v[0])->f;

    OP_LT_V_I(ctx, n1, n0, 2);
    OP_JMP_IF_FALSE(n1, L3);
L2:;
    GC_CHECK(ctx);
    COPY_VAR_TO(ctx, r, n0);
    goto L1;
L3:;
L4:;
    GC_CHECK(ctx);
    p = vstackp(ctx);
    OP_SUB_V_I(ctx, n2, n0, 2);
    push_var(ctx, n2);
    e = ((vmfunc_t)(f1->f))(ctx, lex, n1, 1);
    restore_vstackp(ctx, p);
    if (e) goto L1;
    p = vstackp(ctx);
    OP_SUB_V_I(ctx, n4, n0, 1);
    push_var(ctx, n4);
    e = ((vmfunc_t)(f1->f))(ctx, lex, n3, 1);
    restore_vstackp(ctx, p);
    if (e) goto L1;
    OP_ADD(ctx, r, n1, n3);
L1:;
    reduce_vstackp(ctx, allocated_local);
    return e;
}

typedef int (*fib_t)(vmctx *ctx, vmfrm *lex, int64_t *r, int64_t n);
int fib(vmctx *ctx, vmfrm *lex, int64_t *r, int64_t n)
{
    int e = 0;
    if (n < 2) {
        *r = n;
        return e;
    }
    int64_t r1 = 0;
    e = fib(ctx, lex, &r1, n-2);
    if (e) return e;
    int64_t r2 = 0;
    e = fib(ctx, lex, &r2, n-1);
    if (e) return e;
    *r = r1 + r2;
    return e;
}

/* function:fact */
typedef int64_t (*fact_t)(int64_t *e, int64_t a0);
int64_t nfact(int64_t *e, int64_t a0)
{
    int64_t r;
    int64_t n = a0;
    int64_t n0 = 0;
    int64_t n1 = 0;
    int64_t n2 = 0;
    fact_t f1 = nfact;

    n0 = a0;
    n1 = (n0 == 0);
    if (!n1) goto L3;

L2:;
    r = 1;
    goto L1;

L3:;
L4:;
    OP_NSUB_I_I(e, n, n2, n0, 1, L1)
    n1 = f1(e, n2);
    OP_NMUL_I_I(e, n, r, n0, n1, L1)

L1:;
    return r;
}


int fact(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    GC_CHECK(ctx);
    int e = 0;

    const int allocated_local = 3;
    alloc_var(ctx, 3);
    CHECK_EXCEPTION(L1);
    vmvar *n0 = local_var(ctx, 0);
    vmvar *n1 = local_var(ctx, 1);
    vmvar *n2 = local_var(ctx, 2);
    SET_ARGVAR(0, 3);

    if (n0->t == VAR_INT64 && (ctx->callee->n == 0 || n0->i < ctx->callee->n)) {
        int64_t error = 0;
        int64_t ret = nfact(&error, n0->i);
        if (error > 0) {
            ctx->callee->n = error;
            goto L10;
        }
        SET_I64((r), ret);
        goto L1;
    }

L10:;
    vmfnc *f1 = ((lex->v[0]))->f;

    OP_EQEQ_V_I(ctx, n1, n0, 0);
    OP_JMP_IF_FALSE(n1, L3);

L2:;
    SET_I64((r), 1);
    goto L1;

L3:;
L4:;
    int p0 = vstackp(ctx);
    OP_SUB_V_I(ctx, n2, n0, 1);
    push_var(ctx, n2);
    CHECK_EXCEPTION(L1);
    vmfnc *callee = ctx->callee;
    ctx->callee = f1;
    e = ((vmfunc_t)(f1->f))(ctx, lex, n1, 1);
    ctx->callee = callee;
    restore_vstackp(ctx, p0);
    CHECK_EXCEPTION(L1);
    OP_MUL(ctx, (r), n0, n1);

L1:;
    reduce_vstackp(ctx, allocated_local);
    return e;
}

int run_global(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmfrm *global = alcfrm(ctx, 16);
    push_frm(ctx, global);

    int c = 38;

    /* Pattern 1 */
    // {
    //     vmfnc *f1 = alcfnc(ctx, func_fib2, global, 1);
    //     vmvar *l1 = alcvar_fnc(ctx, f1);
    //     global->v[0] = l1;
    //     global->v[1] = alcvar_int64(ctx, c, 0);

    //     int p = vstackp(ctx);
    //     push_var(ctx, global->v[1]);
    //     ((vmfunc_t)(f1->f))(ctx, f1->lex, r, 1);
    //     restore_vstackp(ctx, p);

    //     if (r->t == VAR_INT64) {
    //         printf("fib(%d) = %lld\n", c, r->i);
    //     } else if (r->t == VAR_BIG) {
    //         char s1[1024] = {0};
    //         bi_str(s1, 1024, r->bi);
    //         printf("fib(%d) = %s\n", c, s1);
    //     }
    //     pbakvar(ctx, r);
    // }

    /* Pattern 2 */
    // {
    //     vmfnc *f1 = alcfnc(ctx, fib, global, 1);
    //     vmvar *l1 = alcvar_fnc(ctx, f1);
    //     global->v[0] = l1;

    //     vmvar *r = alcvar(ctx, VAR_INT64, 1);
    //     ((fib_t)(f1->f))(ctx, lex, &(r->i), c);
    //     printf("fib(%d) = %lld\n", c, r->i);
    //     pbakvar(ctx, r);
    // }


    /* Fact */
    {
        vmfnc *f1 = alcfnc(ctx, fact, global, 1);
        vmvar *l1 = alcvar_fnc(ctx, f1);
        global->v[0] = l1;
        vmvar *n0 = alcvar_int64(ctx, 500, 1);

        vmvar *r = alcvar(ctx, VAR_INT64, 1);
        push_var(ctx, n0);
        vmfnc *callee = ctx->callee;
        ctx->callee = f1;
        ((vmfunc_t)(f1->f))(ctx, f1->lex, r, 1);
        ctx->callee = callee;
        if (r->t == VAR_BIG) {
            char *bs = BzToString(r->bi->b, 10, 0);
            printf("%s\n", bs);
            BzFreeString(bs);
        }

        pbakvar(ctx, r);
    }
    pop_frm(ctx);
}
