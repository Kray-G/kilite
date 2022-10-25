#include "header.h"

// c2m -c alloc.c gc.c init.c main.c util.c bigi.c str.c hash.c lib\bign.c lib\bigz.c
// timecmd c2m alloc.bmir gc.bmir init.bmir main.bmir util.bmir bigi.bmir hash.bmir str.bmir bign.bmir bigz.bmir sample.c -eg
// cl -O2 -Fesample.exe alloc.c gc.c init.c main.c util.c bigi.c str.c hash.c lib\bign.c lib\bigz.c sample.c

int func_fib(vmctx *ctx, vmfrm *lex, vmvar **r, int ac)
{
    GC_CHECK(ctx);

    // vmfrm *frm = alcfrm(ctx, 1);
    // push_frm(ctx, frm);

    /* Local variable(s) */
    int p, e = 0;
    alloc_var(ctx, 4);
    vmvar *n0 = local_var(ctx, 0);
    vmvar *t1 = local_var(ctx, 1);
    vmvar *t2 = local_var(ctx, 2);
    vmvar *t3 = local_var(ctx, 3);
    if (ac > 0) {
        vmvar *a0 = arg_var(ctx, 4);    // Why it is 4 is because 4 spaces are allocated for local variables.
        COPY_VAR_TO(ctx, n0, a0);
    } else {
        init_var(n0);
    }

    OP_LT_V_I(t1, n0, 2);
    OP_JMP_IF_FALSE(t1, COND2);

COND1:;
    GC_CHECK(ctx);
    COPY_VAR_TO(ctx, *r, n0);
    goto END;

COND2:;
    vmfnc *f1 = lex->v[0]->f;
    GC_CHECK(ctx);

    OP_SUB_V_I(ctx, t1, n0, 2);

    p = vstackp(ctx);
    push_var(ctx, t1);
    e = (f1->f)(ctx, f1->lex, &t2, 1);
    restore_vstackp(ctx, p);
    if (e) goto END;

    OP_SUB_V_I(ctx, t1, n0, 1);

    p = vstackp(ctx);
    push_var(ctx, t1);
    e = (f1->f)(ctx, f1->lex, &t3, 1);
    restore_vstackp(ctx, p);
    if (e) goto END;

    OP_ADD(ctx, *r, t2, t3);

    goto END;

END:
    reduce_vstackp(ctx, 4);
    /* This frame can be released because this function is not being a lexical scope. */
    // pop_frm(ctx);
    // pbakfrm(ctx, frm);
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

void run_global(vmctx *ctx)
{
    vmfrm *global = alcfrm(ctx, 16);
    push_frm(ctx, global);

    int c = 38;

    /* Pattern 1 */
    {
        vmfnc *f1 = alcfnc(ctx, func_fib, global, 1);
        vmvar *l1 = alcvar_fnc(ctx, f1);
        global->v[0] = l1;
        global->v[1] = alcvar_int64(ctx, c, 0);

        vmvar *r = alcvar(ctx, VAR_INT64, 1);
        int p = vstackp(ctx);
        push_var(ctx, global->v[1]);
        (f1->f)(ctx, f1->lex, &r, 1);
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

    //     vmvar *r = alcvar(ctx, VAR_INT64, 1);
    //     r->i = ((fib_t)(f1->f))(c);
    //     printf("fib(%d) = %lld\n", c, r->i);
    //     pbakvar(ctx, r);
    // }

    pop_frm(ctx);
}
