#ifndef KILITE_AMALGAMATION
#include "common.h"
#endif

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

vmfrm *get_lex(vmfrm* lex, int c)
{
    while (--c > 0) {
        lex = lex->lex;
    }
    return lex;
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

void fprint_obj(vmctx *ctx, vmvar *v, FILE *fp)
{
    switch (v->t) {
    case VAR_UNDEF:
        fprintf(fp, "null");
        break;
    case VAR_INT64:
        fprintf(fp, "%" PRId64, v->i);
        break;
    case VAR_DBL:
        fprintf(fp, "%.16g", v->d);
        break;
    case VAR_BIG: {
        char *bs = BzToString(v->bi->b, 10, 0);
        fprintf(fp, "%s", bs);
        BzFreeString(bs);
        break;
    }
    case VAR_STR:
        fprintf(fp, "%s", v->s->hd);
        break;
    case VAR_BIN:
        fprint_bin(ctx, v->bn, fp);
        break;
    case VAR_OBJ:
        hashmap_objfprint(ctx, v->o, fp);
        break;
    case VAR_FNC:
        fprintf(fp, "<function>");
        break;
    default:
        fprintf(fp, "<UNKNOWN>");
        break;
    }
}

void print_obj(vmctx *ctx, vmvar *v)
{
    fprint_obj(ctx, v, stdout);
}
