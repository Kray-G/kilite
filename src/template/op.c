#ifndef KILITE_AMALGAMATION
#include "common.h"
#endif
extern double fmod(double, double);
extern double pow(double, double);
extern double fabs(double);

/* Bit NOT */

int bnot_v(vmctx *ctx, vmvar *r, vmvar *v)
{
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = (INT64_MIN + 1);
        break;
    case VAR_BOOL:
        r->t = VAR_INT64;
        r->i = v->i ? 0 : (INT64_MIN + 1);
        break;
    case VAR_DBL:
        r->t = VAR_INT64;
        r->i = ~((int64_t)(v->d));
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

/* NOT */

int not_v(vmctx *ctx, vmvar *r, vmvar *v)
{
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_BOOL;
        r->i = 1;
        break;
    case VAR_DBL:
        r->t = VAR_BOOL;
        r->i = (v->d < DBL_EPSILON) ? 1 : 0;
        break;
    case VAR_STR:
        r->t = VAR_BOOL;
        r->i = v->s->len == 0;
        break;
    case VAR_BIN:
        r->t = VAR_BOOL;
        r->i = v->bn->len == 0;
        break;
    case VAR_FNC:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    case VAR_OBJ: {
        vmvar *f = hashmap_search(v->o, "_False");
        if (f) {
            r->t = VAR_BOOL;
            r->i = f->t == VAR_INT64 ? f->i : 0;
        } else {
            r->t = VAR_BOOL;
            r->i = v->o->idxsz == 0;
        }
        break;
    }
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

/* ADD */

int add_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = i;
        break;
    case VAR_BOOL:
        r->t = VAR_INT64;
        r->i = v->i + i;
        break;
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = v->d + (double)i;
        break;
    case VAR_STR:
        r->t = VAR_STR;
        r->s = str_dup(ctx, v->s);
        str_append_i64(ctx, r->s, i);
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "+", __LINE__, r, v, i);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int add_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = i;
        break;
    case VAR_BOOL:
        r->t = VAR_INT64;
        r->i = i + v->i;
        break;
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = (double)i + v->d;
        break;
    case VAR_STR:
        r->t = VAR_STR;
        r->s = str_from_i64(ctx, i);
        str_append_str(ctx, r->s, v->s);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int add_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = v1->i;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = v1->d;
            break;
        case VAR_STR:
            r->t = VAR_STR;
            r->s = alcstr_str(ctx, v1->s->hd);
            break;
        case VAR_BIN:
            r->t = VAR_BIN;
            r->bn = v1->bn;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BOOL:
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = v1->i;
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = v0->i + v1->i;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = (double)v0->i + v1->d;
            break;
        case VAR_STR:
            r->t = VAR_STR;
            r->s = alcstr_str(ctx, "");
            str_append_i64(ctx, r->s, v0->i);
            str_append_str(ctx, r->s, v1->s);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzCopy(v0->bi->b));
            break;
        case VAR_BOOL:
            r->t = VAR_BIG;
            if (v1->i) {
                BigZ b2 = BzFromInteger(v1->i);
                r->bi->b = BzAdd(v0->bi->b, b2);
                BzFree(b2);
                bi_normalize(r);
            } else {
                r->bi = alcbgi_bigz(ctx, BzCopy(v0->bi->b));
            }
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = BzToDouble(v0->bi->b) + v1->d;
            break;
        case VAR_STR:
            r->t = VAR_STR;
            char *bs = BzToString(v0->bi->b, 10, 0);
            r->s = alcstr_str(ctx, bs);
            str_append_str(ctx, r->s, v1->s);
            BzFreeString(bs);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_DBL;
            r->d = v0->d;
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_DBL;
            r->d = v0->d + (double)v1->i;
            break;
        case VAR_BIG:
            r->t = VAR_DBL;
            r->d = v0->d + BzToDouble(v1->bi->b);
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = v0->d + v1->d;
            break;
        case VAR_STR:
            r->t = VAR_STR;
            r->s = str_from_dbl(ctx, &(v0->d));
            str_append_str(ctx, r->s, v1->s);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            break;
        case VAR_BOOL:
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            str_append_cp(ctx, r->s, v1->i ? "true" : "false");
            break;
        case VAR_INT64:
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            str_append_i64(ctx, r->s, v1->i);
            break;
        case VAR_BIG:
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            char *bs = BzToString(v1->bi->b, 10, 0);
            str_append_cp(ctx, r->s, bs);
            BzFreeString(bs);
            break;
        case VAR_DBL:
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            str_append_dbl(ctx, r->s, &(v1->d));
            break;
        case VAR_STR: {
            vmstr *s = str_dup(ctx, v0->s);
            str_append_str(ctx, s, v1->s);
            r->t = VAR_STR;
            r->s = s;
            break;
        }
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIN:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BIN;
            r->bn = v0->bn;
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_BIN;
            r->bn = bin_dup(ctx, v0->bn);
            bin_append_ch(ctx, r->bn, (uint8_t)v1->i);
            break;
        case VAR_BIN:
            r->t = VAR_BIN;
            r->bn = bin_dup(ctx, v0->bn);
            bin_append_bin(ctx, r->bn, v1->bn);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR(ctx, "+", __LINE__, r, v0, v1);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* SUB */

int sub_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = - i;
        break;
    case VAR_BOOL:
        r->t = VAR_INT64;
        r->d = v->i - i;
        break;
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = v->d - (double)i;
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "-", __LINE__, r, v, i);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int sub_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = i;
        break;
    case VAR_BOOL:
        r->t = VAR_INT64;
        r->d = i - v->i;
        break;
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = i - v->d;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int sub_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = - v1->i;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = - v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BOOL:
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = v1->i;
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = v0->i - v1->i;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = (double)v0->i - v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzNegate(v0->bi->b));
            break;
        case VAR_BOOL:
            r->t = VAR_BIG;
            if (v1->i) {
                BigZ b2 = BzFromInteger(v1->i);
                r->bi->b = BzSubtract(v0->bi->b, b2);
                BzFree(b2);
                bi_normalize(r);
            } else {
                r->bi = alcbgi_bigz(ctx, BzCopy(v0->bi->b));
            }
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = BzToDouble(v0->bi->b) - v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_DBL;
            r->d = v0->d;
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_DBL;
            r->d = v0->d - (double)v1->i;
            break;
        case VAR_BIG:
            r->t = VAR_DBL;
            r->d = v0->d - BzToDouble(v1->bi->b);
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = v0->d - v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR(ctx, "-", __LINE__, r, v0, v1);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* MUL */

int mul_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_BOOL:
        r->t = VAR_INT64;
        r->i = v->i * i;
        break;
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = v->d * (double)i;
        break;
    case VAR_STR:
        r->t = VAR_STR;
        r->s = str_dup(ctx, v->s);
        str_make_ntimes(ctx, r->s, i);
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "*", __LINE__, r, v, i);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int mul_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_BOOL:
        r->t = VAR_INT64;
        r->i = i * v->i;
        break;
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = (double)i * v->d;
        break;
    case VAR_STR:
        r->t = VAR_STR;
        r->s = str_dup(ctx, v->s);
        str_make_ntimes(ctx, r->s, i);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int mul_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = 0.0;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BOOL:
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_DBL;
            r->i = v0->i * v1->i;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = (double)v0->i * v1->d;
            break;
        case VAR_STR:
            r->t = VAR_STR;
            r->s = str_dup(ctx, v1->s);
            str_make_ntimes(ctx, r->s, v0->i);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_BOOL:
            if (v1->i) {
                r->t = VAR_BIG;
                r->bi = alcbgi_bigz(ctx, BzCopy(v0->bi->b));
            } else {
                r->t = VAR_INT64;
                r->i = 0;
            }
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = BzToDouble(v0->bi->b) * v1->d;
            break;
        default:
            /* Unsupported also for string because big int could be so big! */
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_DBL;
            r->d = 0.0;
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_DBL;
            r->d = v0->d * (double)v1->i;
            break;
        case VAR_BIG:
            r->t = VAR_DBL;
            r->d = v0->d * BzToDouble(v1->bi->b);
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = v0->d * v1->d;
            break;
        case VAR_STR:
            /* double value can be used as an int in some cases. */
            r->t = VAR_STR;
            r->s = str_dup(ctx, v1->s);
            str_make_ntimes(ctx, r->s, (int64_t)(v0->d));
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_STR;
            r->s = alcstr_str(ctx, "");
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_STR;
            if (v1->i > 0) {
                r->s = str_dup(ctx, v0->s);
                str_make_ntimes(ctx, r->s, v1->i);
            } else {
                r->s = alcstr_str(ctx, "");
            }
            break;
        case VAR_BIG:
            /* Unsupported because big int could be so big! */
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        case VAR_DBL:
            /* double value can be used as an int in some cases. */
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            str_make_ntimes(ctx, r->s, (int64_t)(v1->d));
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR(ctx, "*", __LINE__, r, v0, v1);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* DIV */

int div_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
    case VAR_DBL:
        if (i == 0) {
            return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
        }
        r->t = VAR_DBL;
        r->d = v->d / (double)i;
        break;
    case VAR_STR:
        /* Supported the integer value as a path name. */
        r->t = VAR_STR;
        r->s = str_dup(ctx, v->s);
        str_make_path_i64(ctx, r->s, i);
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "/", __LINE__, r, v, i);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int div_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
    case VAR_DBL:
        if (v->d < DBL_EPSILON) {
            return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
        }
        r->t = VAR_DBL;
        r->d = (double)i / v->d;
        break;
    case VAR_STR:
        /* Supported the integer value as a path name. */
        r->t = VAR_STR;
        r->s = str_dup(ctx, v->s);
        str_make_i64_path(ctx, i, r->s);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int div_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = 0.0;
            break;
        case VAR_STR:
            r->t = VAR_STR;
            r->s = str_dup(ctx, v1->s);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = (double)v0->i / v1->d;
            break;
        case VAR_STR:
            r->t = VAR_STR;
            r->s = alcstr_str(ctx, "");
            str_make_i64_path(ctx, v0->i, r->s);
            str_make_path(ctx, r->s, v1->s);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
        case VAR_DBL:
            if (v1->d < DBL_EPSILON) {
                return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
            }
            r->t = VAR_DBL;
            r->d = BzToDouble(v0->bi->b) / v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
        case VAR_INT64:
            if (v1->i == 0) {
                return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
            }
            r->t = VAR_DBL;
            r->d = v0->d / (double)v1->i;
            break;
        case VAR_BIG:
            r->t = VAR_DBL;
            r->d = v0->d / BzToDouble(v1->bi->b);
            break;
        case VAR_DBL:
            if (v1->d < DBL_EPSILON) {
                return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
            }
            r->t = VAR_DBL;
            r->d = v0->d / v1->d;
            break;
        case VAR_STR:
            /* double value can be used as an int in some cases. */
            r->t = VAR_STR;
            r->s = str_dup(ctx, v1->s);
            str_make_i64_path(ctx, (int64_t)(v0->d), r->s);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            break;
        case VAR_INT64:
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            str_make_path_i64(ctx, r->s, v1->i);
            break;
        case VAR_BIG:
            /* Unsupported because big int could be so big! */
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        case VAR_DBL:
            /* double value can be used as an int in some cases. */
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            str_make_path_i64(ctx, r->s, (int64_t)(v1->d));
            break;
        case VAR_STR: {
            vmstr *s = str_dup(ctx, v0->s);
            str_make_path(ctx, s, v1->s);
            r->t = VAR_STR;
            r->s = s;
            break;
        }
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR(ctx, "/", __LINE__, r, v0, v1);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* MOD */

int mod_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = fmod(v->d, (double)i);
        break;
    case VAR_STR:
        r->t = VAR_OBJ;
        r->o = alcobj(ctx);
        r->o->is_formatter = 1;
        hashmap_set(ctx, r->o, "_format", v);
        array_set(ctx, r->o, 0, alcvar_int64(ctx, i, 0));
        break;
    case VAR_OBJ:
        if (!v->o->is_formatter) {
            OP_CALL_SPECIAL_OPERATOR_I(ctx, "%", __LINE__, r, v, i);
        } else {
            r->t = VAR_OBJ;
            r->o = v->o;
            array_push(ctx, r->o, alcvar_int64(ctx, i, 0));
        }
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int mod_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
    case VAR_DBL:
        if (v->d < DBL_EPSILON) {
            return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
        }
        r->t = VAR_DBL;
        r->d = fmod((double)i, v->d);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int mod_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = 0.0;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = fmod((double)v0->i, v1->d);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
        case VAR_DBL:
            if (v1->d < DBL_EPSILON) {
                return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
            }
            r->t = VAR_DBL;
            r->d = fmod(BzToDouble(v0->bi->b), v1->d);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
        case VAR_INT64:
            if (v1->i == 0) {
                return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
            }
            r->t = VAR_DBL;
            r->d = fmod(v0->d, (double)v1->i);
            break;
        case VAR_BIG:
            r->t = VAR_DBL;
            r->d = fmod(v0->d, BzToDouble(v1->bi->b));
            break;
        case VAR_DBL:
            if (v1->d < DBL_EPSILON) {
                return throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL);
            }
            r->t = VAR_DBL;
            r->d = fmod(v0->d, v1->d);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
        case VAR_UNDEF: {
            r->t = VAR_OBJ;
            r->o = alcobj(ctx);
            r->o->is_formatter = 1;
            hashmap_set(ctx, r->o, "_format", v0);
            vmvar *v = alcvar_str(ctx, "((null))");
            array_push(ctx, r->o, v);
            break;
        }
        case VAR_BOOL:
        case VAR_INT64:
        case VAR_BIG:
        case VAR_DBL:
        case VAR_STR:
        case VAR_BIN:
        case VAR_OBJ:
            r->t = VAR_OBJ;
            r->o = alcobj(ctx);
            r->o->is_formatter = 1;
            hashmap_set(ctx, r->o, "_format", v0);
            array_push(ctx, r->o, v1);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_OBJ:
        if (!v0->o->is_formatter) {
            OP_CALL_SPECIAL_OPERATOR(ctx, "%", __LINE__, r, v0, v1);
        } else {
            r->t = VAR_OBJ;
            r->o = v0->o;
            if (v1->t == VAR_UNDEF) {
                vmvar *v = alcvar_str(ctx, "((null))");
                array_push(ctx, r->o, v);
            } else {
                array_push(ctx, r->o, v1);
            }
        }
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* POW */

int pow_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = pow(v->d, (double)i);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int pow_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 1;
        break;
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = pow((double)i, v->d);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int pow_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = 0.0;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 1;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = pow((double)v0->i, v1->d);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 1;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = pow(BzToDouble(v0->bi->b), v1->d);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 1;
        case VAR_INT64:
            r->t = VAR_DBL;
            r->d = pow(v0->d, (double)v1->i);
            break;
        case VAR_BIG:
            r->t = VAR_DBL;
            r->d = pow(v0->d, BzToDouble(v1->bi->b));
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = pow(v0->d, v1->d);
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* EQEQ */

int eqeq_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_BOOL;
        r->i = (i == 0);
        break;
    case VAR_DBL:
        r->t = VAR_BOOL;
        r->i = fabs(v->d - (double)i) < DBL_EPSILON;
        break;
    case VAR_STR: {
        char buf[32] = {0};
        sprintf(buf, "%" PRId64, i);
        r->t = VAR_BOOL;
        r->i = strcmp(v->s->hd, buf) == 0;
        break;
    }
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "==", __LINE__, r, v, i);
        break;
    default:
        r->t = VAR_BOOL;
        r->i = 0;
        break;
    }
    return 0;
}

int eqeq_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    return eqeq_v_i(ctx, r, v, i);
}

int eqeq_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_BOOL;
            r->i = v1->i == 0;
            break;
        case VAR_DBL:
            r->t = VAR_BOOL;
            r->i = fabs(v1->d) < DBL_EPSILON;
            break;
        default:
            r->t = VAR_BOOL;
            r->i = 0;
            break;
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BOOL;
            r->i = v0->i == 0;
            break;
        case VAR_DBL:
            r->t = VAR_BOOL;
            r->i = fabs((double)v0->i - v1->d) < DBL_EPSILON;
            break;
        case VAR_STR: {
            char buf[32] = {0};
            sprintf(buf, "%" PRId64, v0->i);
            r->t = VAR_BOOL;
            r->i = strcmp(v1->s->hd, buf) == 0;
            break;
        }
        default:
            r->t = VAR_BOOL;
            r->i = 0;
            break;
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_STR: {
            char *bs = BzToString(v0->bi->b, 10, 0);
            r->t = VAR_BOOL;
            r->i = strcmp(v1->s->hd, bs) == 0;
            BzFreeString(bs);
            break;
        }
        default:
            r->t = VAR_BOOL;
            r->i = 0;
            break;
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BOOL;
            r->i = v0->d < DBL_EPSILON;
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_BOOL;
            r->i = fabs(v0->d - (double)v1->i) < DBL_EPSILON;
            break;
        case VAR_BIG:
            r->t = VAR_BOOL;
            r->i = 0;
            break;
        case VAR_DBL:
            r->i = fabs(v0->d - v1->d) < DBL_EPSILON;
            break;
        case VAR_STR: {
            char buf[32] = {0};
            sprintf(buf, "%.16g", v0->d);
            r->t = VAR_BOOL;
            r->i = strcmp(v1->s->hd, buf) == 0;
            break;
        }
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BOOL;
            r->i = v0->s->len == 0;
            break;
        case VAR_BOOL:
            r->t = VAR_BOOL;
            r->i = strcmp(v0->s->hd, v1->i ? "true" : "false") == 0;
            break;
        case VAR_INT64: {
            char buf[32] = {0};
            sprintf(buf, "%" PRId64, v1->i);
            r->t = VAR_BOOL;
            r->i = strcmp(v0->s->hd, buf) == 0;
            break;
        }
        case VAR_BIG: {
            char *bs = BzToString(v1->bi->b, 10, 0);
            r->t = VAR_BOOL;
            r->i = strcmp(v0->s->hd, bs) == 0;
            BzFreeString(bs);
            break;
        }
        case VAR_DBL: {
            char buf[32] = {0};
            sprintf(buf, "%.16g", v1->d);
            r->t = VAR_BOOL;
            r->i = strcmp(v0->s->hd, buf) == 0;
            break;
        }
        case VAR_STR:
            r->t = VAR_BOOL;
            r->i = strcmp(v0->s->hd, v1->s->hd) == 0;
            break;
        case VAR_BIN:
        case VAR_OBJ:
        case VAR_FNC:
            r->t = VAR_BOOL;
            r->i = 0;
            break;
        }
        break;
    case VAR_BIN:
        r->t = VAR_BOOL;
        r->i = v1->t == VAR_BIN && v0->bn->len == v1->bn->len;
        if (r->i) {
            vmbin *b0 = v0->bn;
            vmbin *b1 = v1->bn;
            for (int i = 0, l = b0->len; i < l; ++i) {
                if (b0->s[i] != b1->s[i]) {
                    r->i = 0;
                    break;
                }
            }
        }
        break;
    case VAR_FNC:
        r->t = VAR_BOOL;
        r->i = v1->t == VAR_FNC && v0->f->f == v1->f->f;
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR(ctx, "==", __LINE__, r, v0, v1);
        break;
    }
    return e;
}

/* NEQ */

int neq_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    if (v->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "!=", __LINE__, r, v, i);
    } else {
        int e = eqeq_v_i(ctx, r, v, i);
        r->i = !(r->i);
        return e;
    }
    return 0;
}

int neq_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    return neq_v_i(ctx, r, v, i);
}

int neq_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    if (v0->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR(ctx, "!=", __LINE__, r, v0, v1);
    } else {
        int e = eqeq_v_v(ctx, r, v0, v1);
        r->i = !(r->i);
        return e;
    }
    return 0;
}

/* LT */

int lt_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_BOOL;
        r->i = 0 < i;
        break;
    case VAR_BIG:
        r->t = VAR_BOOL;
        r->i = BzGetSign(v->bi->b) == BZ_MINUS;
        break;
    case VAR_DBL:
        r->t = VAR_BOOL;
        r->i = v->d < (double)i;
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "<", __LINE__, r, v, i);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int lt_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_BOOL;
        r->i = i < 0;
        break;
    case VAR_BIG:
        r->t = VAR_BOOL;
        r->i = BzGetSign(v->bi->b) == BZ_PLUS;
        break;
    case VAR_DBL:
        r->t = VAR_BOOL;
        r->i = (double)i < v->d;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int lt_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_BOOL;
            r->i = 0 < v1->i;
            break;
        case VAR_DBL:
            r->t = VAR_BOOL;
            r->i = DBL_EPSILON <= fabs(v1->d);
            break;
        case VAR_BIG:
            r->t = VAR_BOOL;
            r->i = BzGetSign(v1->bi->b) == BZ_PLUS;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BOOL;
            r->i = v0->i < 0;
            break;
        case VAR_DBL:
            r->t = VAR_BOOL;
            r->i = (double)v0->i < v1->d;
            break;
        case VAR_BIG:
            r->t = VAR_BOOL;
            r->i = BzGetSign(v1->bi->b) == BZ_PLUS;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BOOL;
            r->i = BzGetSign(v1->bi->b) == BZ_MINUS;
            break;
        case VAR_INT64:
            r->t = VAR_BOOL;
            r->i = BzGetSign(v1->bi->b) == BZ_MINUS;
            break;
        case VAR_DBL:
            r->t = VAR_BOOL;
            r->i = BzToDouble(v0->bi->b) < v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BOOL;
            r->i = v0->d < 0;
            break;
        case VAR_BOOL:
        case VAR_INT64:
            r->t = VAR_BOOL;
            r->i = v0->d < (double)v1->i;
            break;
        case VAR_BIG:
            r->t = VAR_BOOL;
            r->i = v0->d < BzToDouble(v1->bi->b);
            break;
        case VAR_DBL:
            r->i = v0->d < v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
        case VAR_STR:
            r->t = VAR_BOOL;
            r->i = strcmp(v0->s->hd, v1->s->hd) == -1;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR(ctx, "<", __LINE__, r, v0, v1);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* LE */

int le_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    if (v->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "<=", __LINE__, r, v, i);
    } else {
        int e = lt_i_v(ctx, r, i, v);
        r->i = !(r->i);
        return e;
    }
    return 0;
}

int le_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    int e = lt_v_i(ctx, r, v, i);
    r->i = !(r->i);
    return e;
}

int le_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    if (v0->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR(ctx, "<=", __LINE__, r, v0, v1);
    } else {
        int e = lt_v_v(ctx, r, v1, v0);
        r->i = !(r->i);
        return e;
    }
    return 0;
}

/* GT */

int gt_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    if (v->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR_I(ctx, ">", __LINE__, r, v, i);
    } else {
        return lt_i_v(ctx, r, i, v);
    }
    return 0;
}

int gt_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    return lt_v_i(ctx, r, v, i);
}

int gt_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    if (v0->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR(ctx, ">", __LINE__, r, v0, v1);
    } else {
        return lt_v_v(ctx, r, v1, v0);
    }
    return 0;
}

/* GE */

int ge_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    if (v->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR_I(ctx, ">=", __LINE__, r, v, i);
    } else {
        return le_i_v(ctx, r, i, v);
    }
    return 0;
}

int ge_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    return le_v_i(ctx, r, v, i);
}

int ge_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    if (v0->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR(ctx, ">=", __LINE__, r, v0, v1);
    } else {
        return le_v_v(ctx, r, v1, v0);
    }
    return 0;
}

/* LGE */

int lge_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    if (v->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "<=>", __LINE__, r, v, i);
    } else {
        int e = eqeq_v_i(ctx, r, v, i);
        if (r->i) {
            r->i = 0;
            return e;
        }
        e = lt_v_i(ctx, r, v, i);
        if (r->i) {
            r->i = -1;
            return e;
        }
        r->i = 1;
        return e;
    }
    return 0;
}

int lge_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    int e = eqeq_i_v(ctx, r, i, v);
    if (r->i) {
        r->i = 0;
        return e;
    }
    e = lt_i_v(ctx, r, i, v);
    if (r->i) {
        r->i = -1;
        return e;
    }
    r->i = 1;
    return e;
}

int lge_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    if (v0->t == VAR_OBJ) {
        OP_CALL_SPECIAL_OPERATOR(ctx, "<=>", __LINE__, r, v0, v1);
    } else {
        int e = eqeq_v_v(ctx, r, v0, v1);
        if (r->i) {
            r->i = 0;
            return e;
        }
        e = lt_v_v(ctx, r, v0, v1);
        if (r->i) {
            r->i = -1;
            return e;
        }
        r->i = 1;
        return e;
    }
    return 0;
}

/* Bit AND */

int band_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_DBL:
        r->t = VAR_INT64;
        r->i = (int64_t)v->d & i;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int band_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_DBL:
        r->t = VAR_INT64;
        r->d = i & (int64_t)v->d;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int band_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = - v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL:
            r->t = VAR_INT64;
            r->d = v0->i & (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL: {
            BigZ b1 = BzFromInteger((int64_t)v0->d);
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzAnd((v0)->bi->b, b1));
            BzFree(b1);
            bi_normalize(r);
            break;
        }
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d & v1->i;
            break;
        case VAR_BIG: {
            BigZ b0 = BzFromInteger((int64_t)v1->d);
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzAnd(b0, (v1)->bi->b));
            BzFree(b0);
            bi_normalize(r);
            break;
        }
        case VAR_DBL:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d & (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* Bit OR */

int bor_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = i;
        break;
    case VAR_DBL:
        r->t = VAR_INT64;
        r->i = (int64_t)v->d | i;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int bor_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = i;
        break;
    case VAR_DBL:
        r->t = VAR_INT64;
        r->d = i | (int64_t)v->d;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int bor_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = v1->i;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = - v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = v0->i;
            break;
        case VAR_DBL:
            r->t = VAR_INT64;
            r->d = v0->i | (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzCopy((v0)->bi->b));
            break;
        case VAR_DBL: {
            BigZ b1 = BzFromInteger((int64_t)v0->d);
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzOr((v0)->bi->b, b1));
            BzFree(b1);
            bi_normalize(r);
            break;
        }
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d;
            break;
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d | v1->i;
            break;
        case VAR_BIG: {
            BigZ b0 = BzFromInteger((int64_t)v1->d);
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzOr(b0, (v1)->bi->b));
            BzFree(b0);
            bi_normalize(r);
            break;
        }
        case VAR_DBL:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d | (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* Bit XOR */

int bxor_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_DBL:
        r->t = VAR_INT64;
        r->i = (int64_t)v->d ^ i;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int bxor_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_DBL:
        r->t = VAR_INT64;
        r->i = i ^ (int64_t)v->d;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int bxor_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->i = v0->i ^ (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL:
            r->t = VAR_INT64;
            r->i = v0->i ^ (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_DBL: {
            BigZ b1 = BzFromInteger((int64_t)v0->d);
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzXor((v0)->bi->b, b1));
            BzFree(b1);
            bi_normalize(r);
            break;
        }
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d ^ v1->i;
            break;
        case VAR_BIG: {
            BigZ b0 = BzFromInteger((int64_t)v1->d);
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzXor(b0, (v1)->bi->b));
            BzFree(b0);
            bi_normalize(r);
            break;
        }
        case VAR_DBL:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d ^ (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* Bit Shift-L */

int bshl_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_DBL:
        r->t = VAR_INT64;
        r->i = (int64_t)v->d << i;
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR_I(ctx, "<<", __LINE__, r, v, i);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int bshl_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
    case VAR_DBL:
        r->t = VAR_INT64;
        r->i = i;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int bshl_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_INT64:
        case VAR_DBL:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = v0->i;
            break;
        case VAR_DBL:
            r->t = VAR_INT64;
            r->d = v0->i << (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzCopy((v0)->bi->b));
            break;
        case VAR_DBL: {
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzAsh((v0)->bi->b, (int64_t)v0->d));
            break;
        }
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d;
            break;
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d << v1->i;
            break;
        case VAR_BIG: {
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        case VAR_DBL:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d << (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR(ctx, "<<", __LINE__, r, v0, v1);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* Bit Shift-R */

int bshr_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_DBL:
        r->t = VAR_INT64;
        r->i = (int64_t)v->d >> i;
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR_I(ctx, ">>", __LINE__, r, v, i);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int bshr_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_UNDEF:
    case VAR_DBL:
        r->t = VAR_INT64;
        r->i = i;
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

int bshr_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    int e = 0;
    switch (v0->t) {
    case VAR_UNDEF:
        switch (v1->t) {
        case VAR_INT64:
        case VAR_DBL:
            r->t = VAR_INT64;
            r->i = 0;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_INT64:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = v0->i;
            break;
        case VAR_DBL:
            r->t = VAR_INT64;
            r->d = v0->i >> (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_BIG:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzCopy((v0)->bi->b));
            break;
        case VAR_DBL: {
            r->t = VAR_BIG;
            r->bi = alcbgi_bigz(ctx, BzAsh((v0)->bi->b, -((int64_t)v0->d)));
            bi_normalize(r);
            break;
        }
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_UNDEF:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d;
            break;
        case VAR_INT64:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d >> v1->i;
            break;
        case VAR_BIG: {
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        case VAR_DBL:
            r->t = VAR_INT64;
            r->i = (int64_t)v0->d >> (int64_t)v1->d;
            break;
        default:
            return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
        }
        break;
    case VAR_OBJ:
        OP_CALL_SPECIAL_OPERATOR(ctx, ">>", __LINE__, r, v0, v1);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return e;
}

/* INC */

int inc_v(vmctx *ctx, vmvar *v)
{
    switch (v->t) {
    case VAR_UNDEF:
        v->t = VAR_INT64;
        v->i = 1;
        break;
    case VAR_DBL:
        v->d += 1.0;
        break;
    case VAR_BIG: {
        BigZ b2 = BzFromInteger(1);
        BigZ b1 = v->bi->b;
        v->bi->b = BzAdd(b1, b2);
        BzFree(b1);
        BzFree(b2);
        bi_normalize(v);
        break;
    }
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

/* DEC */

int dec_v(vmctx *ctx, vmvar *v)
{
    switch (v->t) {
    case VAR_UNDEF:
        v->i = -1;
        break;
    case VAR_DBL:
        v->d -= 1.0;
        break;
    case VAR_BIG: {
        BigZ b2 = BzFromInteger(1);
        BigZ b1 = v->bi->b;
        v->bi->b = BzSubtract(b1, b2);
        BzFree(b1);
        BzFree(b2);
        bi_normalize(v);
        break;
    }
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

/* Unary minux */

int uminus_v(vmctx *ctx, vmvar *r, vmvar *v)
{
    switch (v->t) {
    case VAR_UNDEF:
        r->t = VAR_INT64;
        r->i = 0;
        break;
    case VAR_BIG:
        r->t = VAR_BIG;
        r->bi = alcbgi_bigz(ctx, BzNegate(v->bi->b));
        bi_normalize(r);
        break;
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}

/* Unary conversion */

int uconv_v(vmctx *ctx, vmvar *r, vmvar *v)
{
    switch (v->t) {
    case VAR_INT64:
        r->t = VAR_STR;
        r->s = alcstr_str(ctx, "0");
        r->s->hd[0] = (int)v->i;
        break;
    case VAR_BIN: {
        char *buf = alloca(v->bn->len + 1);
        for (int i = 0; i < v->bn->len; ++i) {
            buf[i] = v->bn->s[i];
        }
        buf[v->bn->len] = 0;
        r->t = VAR_STR;
        r->s = alcstr_str(ctx, buf);
        break;
    }
    case VAR_STR:
        r->t = VAR_OBJ;
        r->o = alcobj(ctx);
        for (int i = 0; i < v->s->len; ++i) {
            array_push(ctx, r->o, alcvar_int64(ctx, v->s->hd[i], 0));
        }
        break;
    case VAR_OBJ: {
        vmobj *obj = v->o;
        vmvar **ary = obj->ary;
        r->t = VAR_STR;
        if (obj->idxsz > 0) {
            char *buf = alloca(obj->idxsz + 1);
            for (int i = 0; i < obj->idxsz; ++i) {
                if (ary[i] && ary[i]->t == VAR_INT64) {
                    buf[i] = ary[i]->i;
                } else {
                    buf[i] = ' ';
                }
            }
            buf[obj->idxsz] = 0;
            r->s = alcstr_str(ctx, buf);
        } else {
            r->s = alcstr_str(ctx, "");
        }
        break;
    }
    default:
        return throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL);
    }
    return 0;
}
