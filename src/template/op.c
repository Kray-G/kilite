#include "common.h"

/* Make exception */

int throw_system_exception(vmctx *ctx, int id)
{
    /* TODO */
    return 1;
}

int throw_exception(vmctx *ctx, vmvar *e)
{
    /* TODO */
    return 1;
}

/* ADD */

int add_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = v->d + (double)i;
        break;
    case VAR_STR:
        r->t = VAR_STR;
        r->s = str_dup(ctx, v->s);
        str_append_i64(ctx, r->s, i);
        break;
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

int add_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = (double)i + v->d;
        break;
    case VAR_STR:
        r->t = VAR_STR;
        r->s = str_from_i64(ctx, i);
        str_append_str(ctx, r->s, v->s);
        break;
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

int add_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    /* This should care about what can't be cared by *_i_v/*_v_i method. */
    switch (v0->t) {
    case VAR_BIG:
        switch (v1->t) {
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
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
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
            r->s = str_from_dbl(ctx, v0->d);
            str_append_str(ctx, r->s, v1->s);
            break;
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
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
            str_append_i64(ctx, r->s, v1->d);
            break;
        case VAR_STR: {
            vmstr *s = str_dup(ctx, v0->s);
            str_append_str(ctx, s, v1->s);
            r->t = VAR_STR;
            r->s = s;
            break;
        }
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

/* SUB */

int sub_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = v->d - (double)i;
        break;
    case VAR_STR:
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

int sub_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = i - v->d;
        break;
    case VAR_STR:
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

int sub_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
     /* This should care about what can't be cared by *_i_v/*_v_i method. */
    switch (v0->t) {
    case VAR_BIG:
        switch (v1->t) {
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = BzToDouble(v0->bi->b) - v1->d;
            break;
        case VAR_STR:
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_BIG:
            r->t = VAR_DBL;
            r->d = v0->d - BzToDouble(v1->bi->b);
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = v0->d - v1->d;
            break;
        case VAR_STR:
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
        case VAR_BIG:
        case VAR_DBL:
        case VAR_STR:
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

/* MUL */

int mul_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = v->d * (double)i;
        break;
    case VAR_STR:
        r->t = VAR_STR;
        r->s = str_dup(ctx, v->s);
        str_make_ntimes(ctx, r->s, i);
        break;
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

int mul_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_DBL:
        r->t = VAR_DBL;
        r->d = (double)i * v->d;
        break;
    case VAR_STR:
        r->t = VAR_STR;
        r->s = str_dup(ctx, v->s);
        str_make_ntimes(ctx, r->s, i);
        break;
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

int mul_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
     /* This should care about what can't be cared by *_i_v/*_v_i method. */
    switch (v0->t) {
    case VAR_BIG:
        switch (v1->t) {
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = BzToDouble(v0->bi->b) * v1->d;
            break;
        case VAR_STR:
            /* Unsupported because big int could be so big! */
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
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
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
        case VAR_BIG:
            /* Unsupported because big int could be so big! */
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        case VAR_DBL:
            /* double value can be used as an int in some cases. */
            r->t = VAR_STR;
            r->s = str_dup(ctx, v0->s);
            str_make_ntimes(ctx, r->s, (int64_t)(v1->d));
            break;
        case VAR_STR:
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

/* DIV */

int div_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_DBL:
        if (i == 0) {
            return throw_system_exception(ctx, EXCEPT_DIVIDE_BY_ZERO);
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
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

int div_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    /* v's type should not be INT and BIGINT. */
    switch (v->t) {
    case VAR_DBL:
        if (i == 0) {
            return throw_system_exception(ctx, EXCEPT_DIVIDE_BY_ZERO);
        }
        r->t = VAR_DBL;
        r->d = v->d / (double)i;
        break;
    case VAR_STR:
        /* Supported the integer value as a path name. */
        r->t = VAR_STR;
        r->s = str_dup(ctx, v->s);
        str_make_i64_path(ctx, i, r->s);
        break;
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

int div_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
     /* This should care about what can't be cared by *_i_v/*_v_i method. */
    switch (v0->t) {
    case VAR_BIG:
        switch (v1->t) {
        case VAR_DBL:
            if (v1->d < DBL_EPSILON) {
                return throw_system_exception(ctx, EXCEPT_DIVIDE_BY_ZERO);
            }
            r->t = VAR_DBL;
            r->d = BzToDouble(v0->bi->b) / v1->d;
            break;
        case VAR_STR:
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_DBL:
        switch (v1->t) {
        case VAR_BIG:
            r->t = VAR_DBL;
            r->d = v0->d / BzToDouble(v1->bi->b);
            break;
        case VAR_DBL:
            r->t = VAR_DBL;
            r->d = v0->d * v1->d;
            break;
        case VAR_STR:
            /* double value can be used as an int in some cases. */
            r->t = VAR_STR;
            r->s = str_dup(ctx, v1->s);
            str_make_i64_path(ctx, (int64_t)(v0->d), r->s);
            break;
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_STR:
        switch (v1->t) {
        case VAR_BIG:
            /* Unsupported because big int could be so big! */
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
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
        case VAR_FNC:
        case VAR_OBJ:
            return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
        }
        break;
    case VAR_FNC:
    case VAR_OBJ:
        return throw_system_exception(ctx, EXCEPT_UNSUPPORTED_OPERATION);
    }
    return 0;
}

/* MOD */

int mod_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    return 0;
}

int mod_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    return 0;
}

int mod_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    return 0;
}

/* EQEQ */

int eqeq_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i)
{
    return 0;
}

int eqeq_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v)
{
    return 0;
}

int eqeq_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1)
{
    return 0;
}
