#include "common.h"

#define DEF_COUNT(type, idx)\
    void count_##idx(vmctx *ctx)\
    {\
        printf(#idx " allocated = %d, free = %d\n", ctx->cnt.idx, ctx->fre.idx);\
    }\
/**/

DEF_COUNT(vmstr, str)
DEF_COUNT(vmbgi, bgi)
DEF_COUNT(vmobj, obj)
DEF_COUNT(vmfrm, frm)
DEF_COUNT(vmfnc, fnc)
DEF_COUNT(vmvar, var)

void count(vmctx *ctx)
{
    count_str(ctx);
    count_bgi(ctx);
    count_obj(ctx);
    count_frm(ctx);
    count_fnc(ctx);
    count_var(ctx);
}

int get_min2(int a0, int a1)
{
    return a0 < a1 ? a0 : a1;
}

int get_min3(int a0, int a1, int a2)
{
    return a0 < a1 ? get_min2(a0, a2) : get_min2(a1, a2);
}

int get_min4(int a0, int a1, int a2, int a3)
{
    return a0 < a1 ? get_min3(a0, a2, a3) : get_min3(a1, a2, a3);
}

int get_min5(int a0, int a1, int a2, int a3, int a4)
{
    return a0 < a1 ? get_min4(a0, a2, a3, a4) : get_min4(a1, a2, a3, a4);
}

void print_obj(vmvar *v)
{
    switch (v->t) {
    case VAR_INT64:
        printf("%lld", v->i);
        break;
    case VAR_DBL:
        printf("%f", v->d);
        break;
    case VAR_BIG: {
        char *bs = BzToString(v->bi->b, 10, 0);
        printf("%s", bs);
        BzFreeString(bs);
        break;
    }
    case VAR_STR:
        printf("%s", v->s->s);
        break;
    case VAR_OBJ:
        hashmap_objprint(v->o);
        break;
    case VAR_FNC:
        printf("<function>");
        break;
    default:
        printf("<UNKNOWN>");
        break;
    }
}

int print(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    for (int i = 0; i < ac; ++i) {
        vmvar *an = local_var(ctx, i);
        print_obj(an);
    }
}

int println(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    for (int i = 0; i < ac; ++i) {
        vmvar *an = local_var(ctx, i);
        print_obj(an);
    }
    printf("\n");
}
