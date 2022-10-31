#include "common.h"

// c2m -c alloc.c gc.c init.c main.c util.c bigi.c str.c hash.c lib\bign.c lib\bigz.c
// timecmd c2m alloc.bmir gc.bmir init.bmir main.bmir util.bmir bigi.bmir hash.bmir str.bmir bign.bmir bigz.bmir sample.c -eg
// cl -O2 -Fesample.exe alloc.c gc.c init.c main.c util.c bigi.c str.c hash.c lib\bign.c lib\bigz.c sample.c

// c2m -c alloc.c gc.c init.c main.c util.c bigi.c str.c hash.c lib/bign.c lib/bigz.c
// time c2m alloc.bmir gc.bmir init.bmir main.bmir util.bmir bigi.bmir hash.bmir str.bmir bign.bmir bigz.bmir sample.c -eg
// cl -O2 -Fesample.exe alloc.c gc.c init.c main.c util.c bigi.c str.c hash.c lib/bign.c lib/bigz.c sample.c

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

typedef int64_t (*fib_t)(int64_t n);
int64_t fib(int64_t n)
{
    if (n < 2) {
        return n;
    }
    return fib(n-2) + fib(n-1);
}

void run_global(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmfrm *global = alcfrm(ctx, 16);
    push_frm(ctx, global);

    int c = 38;

    /* Pattern 1 */
    {
        vmfnc *f1 = alcfnc(ctx, func_fib2, global, 1);
        vmvar *l1 = alcvar_fnc(ctx, f1);
        global->v[0] = l1;
        global->v[1] = alcvar_int64(ctx, c, 0);

        int p = vstackp(ctx);
        push_var(ctx, global->v[1]);
        ((vmfunc_t)(f1->f))(ctx, f1->lex, r, 1);
        restore_vstackp(ctx, p);

        if (r->t == VAR_INT64) {
            printf("fib(%d) = %lld\n", c, r->i);
        } else if (r->t == VAR_BIG) {
            char s1[1024] = {0};
            bi_str(s1, 1024, r->bi);
            printf("fib(%d) = %s\n", c, s1);
        }
        pbakvar(ctx, r);
    }

    /* Pattern 2 */
    // {
    //     vmfnc *f1 = alcfnc(ctx, fib, global, 1);
    //     vmvar *l1 = alcvar_fnc(ctx, f1);
    //     global->v[0] = l1;

    //     vmvar r = alcvar(ctx, VAR_INT64, 1);
    //     r->i = ((fib_t)(f1->f))(c);
    //     printf("fib(%d) = %lld\n", c, r->i);
    //     pbakvar(ctx, r);
    // }

    pop_frm(ctx);
}
