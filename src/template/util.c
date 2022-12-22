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
    case VAR_BOOL:
        fprintf(fp, "%s", v->i ? "true" : "false");
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

#define XU  (0x01)
#define XL  (0x02)
#define XD  (0x04)
#define XS  (0x08)
#define XP  (0x10)
#define XC  (0x20)
#define XX  (0x40)
#define XB  (0x80)

#define util_ctype_lookup(__c)  (util_ctype[(int)(__c)])
#define x_isalpha(__c)          (util_ctype_lookup(__c)&(XU|XL))
#define x_isupper(__c)          ((util_ctype_lookup(__c)&(XU|XL))==XU)
#define x_islower(__c)          ((util_ctype_lookup(__c)&(XU|XL))==XL)
#define x_isdigit(__c)          (util_ctype_lookup(__c)&XD)
#define x_isxdigit(__c)         (util_ctype_lookup(__c)&(XX|XD))
#define x_isspace(__c)          (util_ctype_lookup(__c)&XS)
#define x_ispunct(__c)          (util_ctype_lookup(__c)&XP)
#define x_isalnum(__c)          (util_ctype_lookup(__c)&(XU|XL|XD))
#define x_isprint(__c)          (util_ctype_lookup(__c)&(XP|XU|XL|XD|XB))
#define x_isgraph(__c)          (util_ctype_lookup(__c)&(XP|XU|XL|XD))
#define x_iscntrl(__c)          (util_ctype_lookup(__c)&XC)

#define UTIL_CTYPE_DATA_000_127 \
    XC,     XC,     XC,     XC,     XC,     XC,     XC,     XC, \
    XC,     XC|XS,  XC|XS,  XC|XS,  XC|XS,  XC|XS,  XC,     XC, \
    XC,     XC,     XC,     XC,     XC,     XC,     XC,     XC, \
    XC,     XC,     XC,     XC,     XC,     XC,     XC,     XC, \
    XS|XB,  XP,     XP,     XP,     XP,     XP,     XP,     XP, \
    XP,     XP,     XP,     XP,     XP,     XP,     XP,     XP, \
    XD,     XD,     XD,     XD,     XD,     XD,     XD,     XD, \
    XD,     XD,     XP,     XP,     XP,     XP,     XP,     XP, \
    XP,     XU|XX,  XU|XX,  XU|XX,  XU|XX,  XU|XX,  XU|XX,  XU, \
    XU,     XU,     XU,     XU,     XU,     XU,     XU,     XU, \
    XU,     XU,     XU,     XU,     XU,     XU,     XU,     XU, \
    XU,     XU,     XU,     XP,     XP,     XP,     XP,     XP, \
    XP,     XL|XX,  XL|XX,  XL|XX,  XL|XX,  XL|XX,  XL|XX,  XL, \
    XL,     XL,     XL,     XL,     XL,     XL,     XL,     XL, \
    XL,     XL,     XL,     XL,     XL,     XL,     XL,     XL, \
    XL,     XL,     XL,     XP,     XP,     XP,     XP,     XC

static const unsigned char util_ctype_b[1 + 256] = {
    0,
    UTIL_CTYPE_DATA_000_127,
    /* all zero in remaining area. */
};

const unsigned char* util_ctype = util_ctype_b + 1;

int c_isalpha(int c)  { return x_isalpha(c);  }
int c_isupper(int c)  { return x_isupper(c);  }
int c_islower(int c)  { return x_islower(c);  }
int c_isdigit(int c)  { return x_isdigit(c);  }
int c_isxdigit(int c) { return x_isxdigit(c); }
int c_isspace(int c)  { return x_isspace(c);  }
int c_ispunct(int c)  { return x_ispunct(c);  }
int c_isalnum(int c)  { return x_isalnum(c);  }
int c_isprint(int c)  { return x_isprint(c);  }
int c_isgraph(int c)  { return x_isgraph(c);  }
int c_iscntrl(int c)  { return x_iscntrl(c);  }
