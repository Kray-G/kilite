#ifndef KILITE_TEMPLATE_HEADER_H
#define KILITE_TEMPLATE_HEADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <float.h>

extern BigZ i64maxp1;
extern BigZ i64minm1;

// printf("%s:%d -> %s\n", __FILE__, __LINE__, __func__);

#ifndef __MIRC__
#define INLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define xprintf printf
#define xsprintf sprintf
#else
#define INLINE inline
int xprintf(const char *, ...);
int xsprintf(const char *, const char *, ...);
int64_t strtoll(const char*, char**, int);
void *malloc(size_t);
void *calloc(size_t, size_t);
void *memset(void *, int, size_t);
void free(void *);
char *strcpy(char *s1, const char *s2);
int strcmp(const char *s1, const char *s2);
#ifndef NULL
#define NULL ((void*)0)
#endif
#endif

/* System Exceptions */
enum {
    EXCEPT_EXCEPTION = 0,
    EXCEPT_RUNTIME_EXCEPTION,
    EXCEPT_STACK_OVERFLOW,
    EXCEPT_DIVIDE_BY_ZERO,
    EXCEPT_UNSUPPORTED_OPERATION,
    EXCEPT_METHOD_MISSING,
    EXCEPT_NO_MATCHING_PATTERN,
    EXCEPT_TOO_FEW_ARGUMENTS,
    EXCEPT_TYPE_MISMATCH,
    EXCEPT_MAX,
};

#define FRM_STACK_SIZE (1024)
#define VAR_STACK_SIZE (1024*16)
#define ALC_UNIT (1024)
#define ALC_UNIT_FRM (1024)
#define TICK_UNIT (1024*64)
#define STR_UNIT (32)
#define HASH_SIZE (23)
#define ARRAY_UNIT (64)
#define HASHITEM_EMPTY(h) ((h)->hasht = 0x00)
#define HASHITEM_EXIST(h) ((h)->hasht = 0x01)
#define HASHITEM_REMVD(h) ((h)->hasht = 0x02)
#define IS_HASHITEM_EMPTY(h) ((h)->hasht == 0x00)
#define IS_HASHITEM_EXIST(h) ((h)->hasht == 0x01)
#define IS_HASHITEM_REMVD(h) ((h)->hasht == 0x02)
#define VARS_MIN_IN_FRAME (32)
#define GC_CHECK(ctx) do { if (--((ctx)->tick) == 0) mark_and_sweep(ctx); } while(0)

#define MARK(obj) ((obj)->flags |= 0x01)
#define HOLD(obj) ((obj)->flags |= 0x02)
#define UNMARK(obj) ((obj)->flags &= 0xFE)
#define UNHOLD(obj) ((obj)->flags &= 0xFD)
#define IS_MARKED(obj) (((obj)->flags & 0x01) == 0x01)
#define IS_HELD(obj) (((obj)->flags & 0x02) == 0x02)

#define push_frm(ctx, m, label, func, file, line) do { \
    if ((ctx)->fstksz <= (ctx)->fstkp) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
    (((ctx)->fstk)[((ctx)->fstkp)++] = (m)); \
} while (0)
/**/
#define pop_frm(ctx) (--((ctx)->fstkp))

#define alloc_var(ctx, n, label, func, file, line) do { \
    if ((ctx)->vstksz <= ((ctx)->vstkp + n)) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
    (((ctx)->vstkp) += (n)); \
} while (0) \
/**/
#define vstackp(ctx) ((ctx)->vstkp)
#define push_var_def(ctx, v, label, func, file, line, pushcode) \
    do { \
            if ((ctx)->vstksz <= (ctx)->vstkp) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        vmvar *px = &(((ctx)->vstk)[((ctx)->vstkp)++]); \
        pushcode \
    } while (0) \
/**/
#define push_var(ctx, v, label, func, file, line)   push_var_def(ctx, v, label, func, file, line, { SHCOPY_VAR_TO(ctx, px, v); })
#define push_var_i(ctx, v, label, func, file, line) push_var_def(ctx, v, label, func, file, line, { px->t = VAR_INT64; px->i = (v); })
#define push_var_b(ctx, v, label, func, file, line) push_var_def(ctx, v, label, func, file, line, { px->t = VAR_BIG; px->bi = alcbgi_bigz(ctx, BzFromString((v), 10, BZ_UNTIL_END)); })
#define push_var_d(ctx, v, label, func, file, line) push_var_def(ctx, v, label, func, file, line, { px->t = VAR_DBL; px->d = (v); })
#define push_var_s(ctx, v, label, func, file, line) push_var_def(ctx, v, label, func, file, line, { px->t = VAR_STR; px->s = alcstr_str(ctx, v); })
#define push_var_sys(ctx, v, fn, label, func, file, line) \
    if (v->t == VAR_OBJ && v->o->is_sysobj) { \
        if ((ctx)->vstksz <= (ctx)->vstkp) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        vmvar *px = &(((ctx)->vstk)[((ctx)->vstkp)++]); \
        SHCOPY_VAR_TO(ctx, px, v); \
        ++fn; \
    } \
/**/
#define push_var_a(ctx, v, fn, label, func, file, line) \
    if ((v)->t == VAR_OBJ) { \
        int idxsz = (v)->o->idxsz; \
        fn += idxsz - 1;\
        if ((ctx)->vstksz <= ((ctx)->vstkp + idxsz)) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        for (int i = idxsz - 1; i >= 0; --i) { \
            vmvar *px = &(((ctx)->vstk)[((ctx)->vstkp)++]); \
            vmvar *item = (v)->o->ary[i]; \
            if (item) {\
                SHCOPY_VAR_TO(ctx, px, item); \
            } else { \
                px->t = VAR_INT64; px->i = 0; \
            } \
        } \
    }\
/**/
#define pop_var(ctx) (--((ctx)->vstkp))
#define reduce_vstackp(ctx, n) (((ctx)->vstkp) -= (n))
#define restore_vstackp(ctx, p) (((ctx)->vstkp) = (p))
#define init_var(v) ((v)->t = VAR_UNDEF)
#define local_var(ctx, n) (&(((ctx)->vstk)[(ctx)->vstkp - ((n) + 1)]))
#define local_var_index(n) ((ctx)->vstkp - ((n) + 1))

#define KL_CAST_TO_I64(ctx, v) { \
    if ((v)->t == VAR_UNDEF) { \
        (v)->t = VAR_INT64; \
        (v)->i = 0; \
    } else if ((v)->t == VAR_DBL) { \
        (v)->t = VAR_INT64; \
        (v)->i = (int64_t)((v)->d); \
    } else if ((v)->t != VAR_INT64) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH); \
    } \
} \
/**/

#define KL_CAST_TO_DBL(ctx, v) { \
    if ((v)->t == VAR_UNDEF) { \
        (v)->t = VAR_DBL; \
        (v)->d = 0.0; \
    } else if ((v)->t == VAR_INT64) { \
        (v)->t = VAR_DBL; \
        (v)->d = (double)((v)->i); \
    } else if ((v)->t != VAR_DBL) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH); \
    } \
} \
/**/

#define KL_SET_METHOD(o, name, fname, args) \
    hashmap_set(ctx, o, #name, alcvar_fnc(ctx, alcfnc(ctx, fname, NULL, #name, args))); \
/**/

/***************************************************************************
 * VM Variable
*/

#define IS_VMINT(x) ((x) <= VAR_BIG)
typedef enum vartype {
    VAR_UNDEF = 0x00,
    VAR_INT64,
    VAR_BIG,
    VAR_DBL,
    VAR_STR,
    VAR_BIN,
    VAR_OBJ,
    VAR_FNC,
    VAR_VOIDP,
} vartype;

struct vmctx;
struct vmvar;
struct vmfnc;
struct vmfrm;

typedef struct vmbgi {
    struct vmbgi *prv;  /* The link to the previous item in alive list. */
    struct vmbgi *liv;  /* The link to the next item in alive list. */
    struct vmbgi *nxt;  /* The link to the next item in free list. */
    struct vmbgi *chn;  /* The link in allocated object list */

    int32_t flags;
    BigZ b;
} vmbgi;

typedef struct vmstr {
    struct vmstr *prv;  /* The link to the previous item in alive list. */
    struct vmstr *liv;  /* The link to the next item in alive list. */
    struct vmstr *nxt;  /* The link to the next item in free list. */
    struct vmstr *chn;  /* The link in allocated object list */

    int32_t flags;
    int cap;
    int len;
    char *s;
    char *hd;
} vmstr;

typedef struct vmobj {
    struct vmobj *prv;  /* The link to the previous item in alive list. */
    struct vmobj *liv;  /* The link to the next item in alive list. */
    struct vmobj *nxt;  /* The link to the next item in free list. */
    struct vmobj *chn;  /* The link in allocated object list */

    int32_t flags;
    int32_t is_sysobj;  /* This is the mark for the system object and automatically passed to the function. */
    int32_t is_formatter;
    int64_t idxsz;
    int64_t asz;
    int64_t hsz;
    struct vmvar **ary; /* Array holder */
    struct vmvar *map;  /* Hashmap holder */
} vmobj;

typedef struct vmvar {
    struct vmvar *prv;  /* The link to the previous item in alive list. */
    struct vmvar *liv;  /* The link to the next item in alive list. */
    struct vmvar *nxt;  /* The link to the next item in free list. */
    struct vmvar *chn;  /* The link in allocated object list */

    int32_t flags;
    int32_t hasht;
    vartype t;
    int64_t i;
    double d;
    vmbgi *bi;
    vmstr *s;
    vmobj *o;           /* The hashmap from string to object */
    void *p;            /* almighty holder */
    struct vmfnc *f;
    struct vmvar *a;    /* an object */
} vmvar;

typedef int (*vmfunc_t)(struct vmctx *ctx, struct vmfrm *lex, struct vmvar *r, int ac);
typedef struct vmfnc {
    struct vmfnc *prv;  /* The link to the previous item in alive list. */
    struct vmfnc *liv;  /* The link to the next item in alive list. */
    struct vmfnc *nxt;  /* The link to the next item in free list. */
    struct vmfnc *chn;  /* The link in allocated object list */

    int32_t flags;
    int32_t args;
    int64_t n;          /* The minimum of n */
    const char *name;   /* function name */
    void *f;            /* function pointer */
    struct vmfrm *frm;  /* the funtion frame holding arguments */
    struct vmfrm *lex;  /* the lexical frame bound with this function */
} vmfnc;

typedef struct vmfrm {
    struct vmfrm *prv;  /* The link to the previous item in alive list. */
    struct vmfrm *liv;  /* The link to the next item in alive list. */
    struct vmfrm *nxt;  /* The link to the next item in free list. */
    struct vmfrm *chn;  /* The link in allocated object list */

    int32_t flags;
    int32_t vars;
    struct vmvar **v;
    struct vmfrm *lex;  /* chain to a lexical frame */
} vmfrm;

/***************************************************************************
 * VM Context
*/
typedef struct vmctx {
    int tick;
    int sweep;
    int gccnt;
    int verbose;
    int print_result;

    int vstksz;
    int vstkp;
    vmvar *vstk;

    int fstksz;
    int fstkp;
    vmfrm **fstk;

    vmfnc *methodmissing;   /* Global methodMissing method. */
    vmfnc *callee;          /* Callee function to manage a pure function. */
    vmvar *except;          /* Current exception that was thrown. */
    int exceptl;            /* The line where the exception occurred. */

    struct {
        vmvar var;
        vmfnc fnc;
        vmfrm frm;
        vmstr str;
        vmbgi bgi;
        vmobj obj;
    } alc;
    struct {
        int var;
        int fnc;
        int frm;
        int str;
        int bgi;
        int obj;
    } cnt;
    struct {
        int var;
        int fnc;
        int frm;
        int str;
        int bgi;
        int obj;
    } fre;
} vmctx;

/* Operator macros */

/* call function */
#define CALL(f1, lex, r, ac) { \
    vmfnc *callee = ctx->callee; \
    ctx->callee = (f1); \
    e = ((vmfunc_t)((f1)->f))(ctx, lex, (r), (ac)); \
    ctx->callee = callee; \
} \
/**/

/* Check if it's a function, exception. */

#define CHECK_CALL(ctx, v, label, r, ac, func, file, line) { \
    if ((v)->t == VAR_FNC) { \
        CALL((v)->f, ((v)->f)->lex, r, ac) \
    } else { \
        if (ctx->methodmissing) { \
             { push_var_s(ctx, "<global>", label, func, file, line); } \
            CALL((ctx->methodmissing), (ctx->methodmissing)->lex, r, ac + 1) \
        } else { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_METHOD_MISSING); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/
#define CHECK_EXCEPTION(label, func, file, line) if (e) { \
    exception_addtrace(ctx, ctx->except, func, file, line); \
    goto label; \
} \
/**/
#define THROW_EXCEPTION(ctx, v, label, func, file, line) { \
    SHCOPY_VAR_TO(ctx, ctx->except, v); \
    exception_addtrace(ctx, ctx->except, func, file, line); \
    e = 1; \
    goto label; \
} \
/**/
#define THROW_CURRENT(ctx, label, func, file, line) { \
    exception_addtrace(ctx, ctx->except, func, file, line); \
    e = 1; \
    goto label; \
} \
/**/
#define CATCH_EXCEPTION(ctx, v) { \
    e = 0; \
    SHCOPY_VAR_TO(ctx, v, ctx->except); \
} \
/**/

#define CHKMATCH_I64(v, vi, label, func, file, line) { \
    if ((v)->t != VAR_INT64 || (v)->i != (vi)) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_NO_MATCHING_PATTERN); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define CHKMATCH_DBL(v, vd, label, func, file, line) { \
    if ((v)->t != VAR_DBL || DBL_EPSILON <= ((v)->d - (vd))) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_NO_MATCHING_PATTERN); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define CHKMATCH_STR(v, i, label, func, file, line) { \
    if ((v)->t != VAR_STR || strcmp((v)->s->s, (vs)) != 0) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_NO_MATCHING_PATTERN); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

/* Copy Variable */

#define MAKE_SUPER(ctx, dst, src) { vmobj *o = hashmap_copy_method(ctx, src->o); (dst)->t = VAR_OBJ; (dst)->o = o; }

#define SET_UNDEF(dst)  { (dst)->t = VAR_UNDEF;                                   }
#define SET_I64(dst, v) { (dst)->t = VAR_INT64; (dst)->i  = (v);                  }
#define SET_DBL(dst, v) { (dst)->t = VAR_DBL;   (dst)->d  = (v);                  }
#define SET_BIG(dst, v) { (dst)->t = VAR_BIG;   (dst)->bi = alcbgi_bigz(ctx, BzFromString((v), 10, BZ_UNTIL_END)); }
#define SET_STR(dst, v) { (dst)->t = VAR_STR;   (dst)->s  = alcstr_str(ctx, (v)); }
#define SET_FNC(dst, v) { (dst)->t = VAR_FNC;   (dst)->f  = (v);                  }
#define SET_OBJ(dst, v) { (dst)->t = VAR_OBJ;   (dst)->o  = (v);                  }

#define SHCOPY_VAR_TO(ctx, dst, src) { \
    switch ((src)->t) { \
    case VAR_UNDEF: \
        (dst)->t = VAR_UNDEF; \
        break; \
    case VAR_INT64: \
        (dst)->t = VAR_INT64; \
        (dst)->i = (src)->i; \
        break; \
    case VAR_DBL: \
        (dst)->t = VAR_DBL; \
        (dst)->d = (src)->d; \
        break; \
    case VAR_BIG: \
        (dst)->t = VAR_BIG; \
        (dst)->bi = (src)->bi; \
        break; \
    case VAR_STR: \
        (dst)->t = VAR_STR; \
        (dst)->s = (src)->s; \
        break; \
    case VAR_FNC: \
        (dst)->t = VAR_FNC; \
        (dst)->f = (src)->f; \
        break; \
    case VAR_OBJ: \
        (dst)->t = VAR_OBJ; \
        (dst)->o = (src)->o; \
        break; \
    default: \
        /* Error */ \
        (dst)->t = VAR_INT64; \
        (dst)->i = 0; \
        break; \
    } \
} \
/**/

#define COPY_VAR_TO(ctx, dst, src) { \
    switch ((src)->t) { \
    case VAR_UNDEF: \
        (dst)->t = VAR_UNDEF; \
        break; \
    case VAR_INT64: \
        (dst)->t = VAR_INT64; \
        (dst)->i = (src)->i; \
        break; \
    case VAR_DBL: \
        (dst)->t = VAR_DBL; \
        (dst)->d = (src)->d; \
        break; \
    case VAR_BIG: \
        (dst)->t = VAR_BIG; \
        (dst)->bi = bi_copy(ctx, (src)->bi); \
        break; \
    case VAR_STR: \
        (dst)->t = VAR_STR; \
        (dst)->s = str_dup(ctx, (src)->s); \
        break; \
    case VAR_FNC: \
        (dst)->t = VAR_FNC; \
        (dst)->f = (src)->f; \
        break; \
    case VAR_OBJ: \
        (dst)->t = VAR_OBJ; \
        (dst)->o = (src)->o; \
        break; \
    default: \
        /* Error */ \
        (dst)->t = VAR_INT64; \
        (dst)->i = 0; \
        break; \
    } \
} \
/**/

#define SHMOVE_VAR_TO(ctx, dst, src) { \
    SHCOPY_VAR_TO(ctx, dst, src) \
    (src) = NULL; \
} \
/**/

#define MOVE_VAR_TO(ctx, dst, src) { \
    COPY_VAR_TO(ctx, dst, src) \
    (src) = NULL; \
} \
/**/

#define OBJCOPY(ctx, dst, src) { \
    (dst)->t = VAR_OBJ; \
    (dst)->o = object_copy(ctx, (src)->o); \
} \
/**/

#define SET_ARGVAR(idx, alc) { \
    if (idx < ac) { \
        vmvar *a##idx = local_var(ctx, (idx + alc)); \
        SHCOPY_VAR_TO(ctx, n##idx, a##idx); \
    } else { \
        init_var(n##idx); \
    } \
} \
/**/

/* Conditional Jump */

#define OP_JMP_IF_TRUE(r, label) { \
    if ((r)->t == VAR_INT64) { \
        if ((r)->i) goto label; \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_JMP_IF_FALSE(r, label) { \
    if ((r)->t == VAR_INT64) { \
        if ((r)->i == 0) goto label; \
    } else { \
        /* TODO */ \
    } \
} \
/**/

/* Object control */

#define OP_HASH_APPLY_OBJ(ctx, r, t1, str) { \
    if ((t1)->o) { \
        vmvar *t2 = hashmap_search((t1)->o, str); \
        if (!t2) { \
            (r)->t = VAR_UNDEF; \
        } else { \
            COPY_VAR_TO(ctx, (r), t2); \
        } \
    } else { \
        (r)->t = VAR_UNDEF; \
    } \
} \
/**/

#define OP_HASH_APPLY(ctx, r, v, str) { \
    vmvar *t1 = ((v)->a) ? (v)->a : (v); \
    if ((t1)->t == VAR_OBJ) { \
        OP_HASH_APPLY_OBJ(ctx, r, t1, str) \
    } else { \
        (r)->t = VAR_UNDEF; \
    } \
} \
/**/

#define OP_HASH_APPLYL_OBJ(ctx, r, t1, str) { \
    vmvar *t2 = hashmap_search((t1)->o, str); \
    if (!t2) { \
        t2 = alcvar_int64(ctx, 0, 0); \
        (t1)->o = hashmap_set(ctx, (t1)->o, str, t2); \
    } \
    (r)->t = VAR_OBJ; \
    (r)->a = t2; \
} \
/**/

#define OP_HASH_APPLYL(ctx, r, v, str) { \
    vmvar *t1 = ((v)->a) ? (v)->a : (v); \
    if ((t1)->t != VAR_OBJ) { \
        (t1)->t = VAR_OBJ; \
        if (!(t1)->o) { \
            (t1)->o = alcobj(ctx); \
        } \
    } \
    OP_HASH_APPLYL_OBJ(ctx, r, t1, str) \
} \
/**/

#define OP_ARRAY_REF_IDXFRM(ctx, r, v, idx) { \
    if ((v)->t == VAR_OBJ) { \
        int ii = idx; \
        if (ii < 0) { \
            do { ii += (v)->o->idxsz; } while (ii < 0); \
        } \
        if (0 <= ii && ii < (v)->o->idxsz) { \
            vmvar *na = alcvar_obj(ctx, alcobj(ctx)); \
            int idxsz = (v)->o->idxsz; \
            for (int i = ii; i < idxsz; ++i) { \
                array_push(ctx, na->o, (v)->o->ary[i]); \
            } \
            SHCOPY_VAR_TO(ctx, r, na); \
        } else { \
            (r)->t = VAR_UNDEF; \
        } \
    } else { \
        (r)->t = VAR_UNDEF; \
    } \
} \
/**/

#define OP_ARRAY_REF_I(ctx, r, v, idx) { \
    if ((v)->t == VAR_OBJ) { \
        int ii = idx; \
        if (ii < 0) { \
            do { ii += (v)->o->idxsz; } while (ii < 0); \
        } \
        if (0 <= ii && ii < (v)->o->idxsz && (v)->o->ary[ii]) { \
            COPY_VAR_TO(ctx, r, (v)->o->ary[ii]); \
        } else { \
            (r)->t = VAR_UNDEF; \
        } \
    } else { \
        (r)->t = VAR_UNDEF; \
    } \
} \
/**/

#define OP_ARRAY_REF(ctx, r, v, iv) { \
    switch ((iv)->t) { \
    case VAR_INT64: { \
        int64_t i = (iv)->i; \
        OP_ARRAY_REF_I(ctx, r, v, i) \
        break; \
    } \
    case VAR_DBL: { \
        int64_t i = (int64_t)iv->d; \
        OP_ARRAY_REF_I(ctx, r, v, i) \
        break; \
    } \
    case VAR_STR: { \
        OP_HASH_APPLY_OBJ(ctx, r, v, (iv)->s->s) \
        break; \
    } \
    default: \
        /* TODO: operator[] */ \
        break; \
    } \
} \
/**/

#define OP_ARRAY_REFL_CHKV(ctx, t1, v) \
    vmvar *t1 = ((v)->a) ? (v)->a : (v); \
    if ((t1)->t != VAR_OBJ) { \
        (t1)->t = VAR_OBJ; \
        if (!(t1)->o) { \
            (t1)->o = alcobj(ctx); \
        } \
    } \
/**/

#define OP_ARRAY_REFL_I(ctx, r, v, idx) { \
    OP_ARRAY_REFL_CHKV(ctx, t1, v) \
    int ii = idx; \
    if (ii < 0) { \
        do { ii += (t1)->o->idxsz; } while (ii < 0); \
    } \
    vmvar *t2 = NULL; \
    if (0 <= ii && ii < (t1)->o->asz) { \
        t2 = (t1)->o->ary[ii]; \
    } \
    if (!t2) { \
        t2 = alcvar_initial(ctx); \
        array_set(ctx, (t1)->o, ii, t2); \
    } \
    (r)->t = VAR_OBJ; \
    (r)->a = t2; \
} \
/**/

#define OP_ARRAY_REFL_S(ctx, r, v, s) { \
    OP_ARRAY_REFL_CHKV(ctx, t1, v) \
    OP_HASH_APPLYL_OBJ(ctx, r, t1, s) \
} \
/**/

#define OP_ARRAY_REFL(ctx, r, v, iv) { \
    switch (iv->t) { \
    case VAR_INT64: { \
        int64_t i = iv->i; \
        OP_ARRAY_REFL_I(ctx, r, v, i) \
        break; \
    } \
    case VAR_DBL: { \
        int64_t i = (int64_t)iv->d; \
        OP_ARRAY_REFL_I(ctx, r, v, i) \
        break; \
    } \
    case VAR_STR: { \
        OP_ARRAY_REFL_S(ctx, r, v, (iv)->s->s) \
        break; \
    } \
    default: \
        /* TODO: operator[] */ \
        break; \
    } \
} \
/**/

/* Increment/Decrement */

#define OP_INC_SAME_I(ctx, r) { \
    if ((r)->i < INT64_MAX) { \
        ++((r)->i); \
    } else { \
        BigZ b2 = BzFromInteger(1); \
        BigZ bi = BzFromInteger((r)->i); \
        (r)->t = VAR_BIG; \
        (r)->bi = alcbgi_bigz(ctx, BzAdd(bi, b2)); \
        BzFree(bi); \
        BzFree(b2); \
    } \
} \
/**/

#define OP_INC_SAME(ctx, r) { \
    if ((r)->t == VAR_INT64) { \
        OP_INC_SAME_I(ctx, r) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_INCP_SAME(ctx, r) { \
    if ((r)->t == VAR_INT64) { \
        OP_INC_SAME_I(ctx, r) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_DEC_SAME_I(ctx, r) { \
    if (INT64_MIN < (r)->i) { \
        --((r)->i); \
    } else { \
        BigZ b2 = BzFromInteger(1); \
        BigZ bi = BzFromInteger((r)->i); \
        (r)->t = VAR_BIG; \
        (r)->bi = alcbgi_bigz(ctx, BzAdd(bi, b2)); \
        BzFree(bi); \
        BzFree(b2); \
    } \
} \

#define OP_DEC_SAME(ctx, r) { \
    if ((r)->t == VAR_INT64) { \
        OP_DEC_SAME_I(ctx, r) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_DECP_SAME(ctx, r) { \
    if ((r)->t == VAR_INT64) { \
        OP_DEC_SAME_I(ctx, r) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_INC(ctx, r, v) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        OP_INC_SAME_I(ctx, t1) \
        SHCOPY_VAR_TO(ctx, r, t1) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_INCP(ctx, r, v) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        (r)->t = VAR_INT64; \
        (r)->i = ((t1)->i); \
        OP_INC_SAME_I(ctx, t1) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_DEC(ctx, r, v) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        OP_DEC_SAME_I(ctx, t1) \
        SHCOPY_VAR_TO(ctx, r, t1) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_DECP(ctx, r, v) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        (r)->t = VAR_INT64; \
        (r)->i = ((t1)->i); \
        OP_DEC_SAME_I(ctx, t1) \
    } else { \
        /* TODO */ \
    } \
} \
/**/

/* Unary minus */

#define OP_UMINUS(ctx, r, v) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        (r)->t = VAR_INT64; \
        (r)->i = -((t1)->i); \
    } else if ((t1)->t == VAR_DBL) { \
        (r)->t = VAR_DBL; \
        (r)->d = -((t1)->d); \
    } else { \
        /* TODO */ \
    } \
} \
/**/

/* ADD */

#define OP_NADD_I_I(e, n, r, i0, i1, label) { \
    if ((i0) >= 0) { \
        if (((i1) <= 0) || ((i0) <= (INT64_MAX - (i1)))) { \
            (r) = (i0) + (i1); \
        } else { \
            if (*e == 0 || n < *e) *e = n; \
        } \
    } else if (((i1) >= 0) || ((i0) >= (INT64_MIN - (i1))))  { \
        (r) = (i0) + (i1); \
    } else { \
            if (*e == 0 || n < *e) *e = n; \
    } \
    if (*e > 0) goto label; \
} \
/**/

#define OP_ADD_I_I(ctx, r, i0, i1) { \
    if ((i0) >= 0) { \
        if (((i1) <= 0) || ((i0) <= (INT64_MAX - (i1)))) { \
            (r)->t = VAR_INT64; \
            (r)->i = (i0) + (i1); \
        } else { \
            BigZ b2 = BzFromInteger(i1); \
            BigZ bi = BzFromInteger(i0); \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzAdd(bi, b2)); \
            BzFree(bi); \
            BzFree(b2); \
            bi_normalize(r); \
        } \
    } else if (((i1) >= 0) || ((i0) >= (INT64_MIN - (i1))))  { \
        (r)->t = VAR_INT64; \
        (r)->i = (i0) + (i1); \
    } else { \
        BigZ b2 = BzFromInteger(i1); \
        BigZ bi = BzFromInteger(i0); \
        (r)->t = VAR_BIG; \
        (r)->bi = alcbgi_bigz(ctx, BzAdd(bi, b2)); \
        BzFree(bi); \
        BzFree(b2); \
        bi_normalize(r); \
    } \
} \
/**/

#define OP_ADD_B_I(ctx, r, v0, i1) { \
    BigZ bi = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAdd((v0)->bi->b, bi)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_ADD_I_B(ctx, r, i0, v1) { \
    BigZ bi = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAdd(bi, (v1)->bi->b)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_ADD_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_ADD_I_I(ctx, r, i0, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_ADD_B_I(ctx, r, v0, i1) \
    } else { \
        e = add_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_ADD_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_ADD_I_I(ctx, r, i0, i1) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_ADD_I_B(ctx, r, i0, v1) \
    } else { \
        e = add_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_ADD(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_ADD_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_ADD_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzAdd((v0)->bi->b, (v1)->bi->b)); \
            bi_normalize(r); \
        } else { \
            e = add_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = add_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

/* SUB */

#define OP_NSUB_I_I(e, n, r, i0, i1, label) { \
    if ((i0) >= 0) { \
        if (((i1) >= 0) || ((i0) <= (INT64_MAX + (i1)))) { \
            (r) = (i0) - (i1); \
        } else { \
            if (*e == 0 || n < *e) *e = n; \
        } \
    } else if (((i1) <= 0) || ((i0) >= (INT64_MIN + (i1)))) { \
        (r) = (i0) - (i1); \
    } else { \
        if (*e == 0 || n < *e) *e = n; \
    } \
    if (*e > 0) goto label; \
} \
/**/

#define OP_SUB_I_I(ctx, r, i0, i1) { \
    if ((i0) >= 0) { \
        if (((i1) >= 0) || ((i0) <= (INT64_MAX + (i1)))) { \
            (r)->t = VAR_INT64; \
            (r)->i = (i0) - (i1); \
        } else { \
            BigZ b2 = BzFromInteger(i1); \
            BigZ bi = BzFromInteger(i0); \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzSubtract(bi, b2)); \
            BzFree(bi); \
            BzFree(b2); \
            bi_normalize(r); \
        } \
    } else if (((i1) <= 0) || ((i0) >= (INT64_MIN + (i1)))) { \
        (r)->t = VAR_INT64; \
        (r)->i = (i0) - (i1); \
    } else { \
        BigZ b2 = BzFromInteger(i1); \
        BigZ bi = BzFromInteger(i0); \
        (r)->t = VAR_BIG; \
        (r)->bi = alcbgi_bigz(ctx, BzSubtract(bi, b2)); \
        BzFree(bi); \
        BzFree(b2); \
        bi_normalize(r); \
    } \
} \
/**/

#define OP_SUB_B_I(ctx, r, v0, i1) { \
    BigZ bi = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzSubtract((v0)->bi->b, bi)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_SUB_I_B(ctx, r, i0, v1) { \
    BigZ bi = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzSubtract(bi, (v1)->bi->b)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_SUB_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_SUB_I_I(ctx, r, i0, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_SUB_B_I(ctx, r, v0, i1) \
    } else { \
        e = sub_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_SUB_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_SUB_I_I(ctx, r, i0, i1) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_SUB_I_B(ctx, r, i0, v1) \
    } else { \
        e = sub_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_SUB(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_SUB_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_SUB_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzSubtract((v0)->bi->b, (v1)->bi->b)); \
        } else { \
            e = sub_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = sub_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

/* MUL */

#define OP_NMUL_I_I(e, n, r, i0, i1, label) { \
    if ((i0) == 0 || (i1) == 0) { \
        (r) = 0; \
    } else if ((i1) == -1) { \
        if ((i0) == INT64_MIN) { \
            if (*e == 0 || n < *e) *e = n; \
        } else { \
            (r) = -(i0); \
        } \
    } else if (((i0) > 0 && (i1) > 0) || ((i0) < 0 && (i1) < 0)) { \
        if ((i0) > (INT64_MAX)/(i1)) { \
            if (*e == 0 || n < *e) *e = n; \
        } else { \
            (r) = (i0) * (i1); \
        } \
    } else if ((i0) < (INT64_MIN)/(i1)) { \
        if (*e == 0 || n < *e) *e = n; \
    } else { \
        (r) = (i0) * (i1); \
    } \
    if (*e > 0) goto label; \
} \
/**/

#define OP_MUL_I_I(ctx, r, i0, i1) { \
    if ((i0) == 0 || (i1) == 0) { \
        (r)->t = VAR_INT64; \
        (r)->i = 0; \
    } else if ((i1) == -1) { \
        if ((i0) == INT64_MIN) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzCopy(i64maxp1)); \
        } else { \
            (r)->t = VAR_INT64; \
            (r)->i = -(i0); \
        } \
    } else if (((i0) > 0 && (i1) > 0) || ((i0) < 0 && (i1) < 0)) { \
        if ((i0) > (INT64_MAX)/(i1)) { \
            BigZ b2 = BzFromInteger(i1); \
            BigZ bi = BzFromInteger(i0); \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzMultiply(bi, b2)); \
            BzFree(bi); \
            BzFree(b2); \
        } else { \
            (r)->t = VAR_INT64; \
            (r)->i = (i0) * (i1); \
        } \
    } else if ((i0) < (INT64_MIN)/(i1)) { \
        BigZ b2 = BzFromInteger(i1); \
        BigZ bi = BzFromInteger(i0); \
        (r)->t = VAR_BIG; \
        (r)->bi = alcbgi_bigz(ctx, BzMultiply(bi, b2)); \
        BzFree(bi); \
        BzFree(b2); \
    } else { \
        (r)->t = VAR_INT64; \
        (r)->i = (i0) * (i1); \
    } \
} \
/**/

#define OP_MUL_B_I(ctx, r, v0, i1) { \
    BigZ bi = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzMultiply((v0)->bi->b, bi)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_MUL_I_B(ctx, r, i0, v1) { \
    BigZ bi = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzMultiply(bi, (v1)->bi->b)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_MUL_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_MUL_I_I(ctx, r, i0, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_MUL_B_I(ctx, r, v0, i1) \
    } else { \
        e = mul_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_MUL_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_MUL_I_I(ctx, r, i0, i1) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_MUL_I_B(ctx, r, i0, v1) \
    } else { \
        e = mul_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_MUL(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_MUL_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_MUL_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzMultiply((v0)->bi->b, (v1)->bi->b)); \
        } else { \
            e = mul_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = mul_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

/* DIV */

#define OP_DIV_I_I(ctx, r, i0, i1) { \
    if (i1 < DBL_EPSILON) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO); \
    } \
    (r)->t = VAR_DBL; \
    (r)->d = ((double)(i0)) / (i1); \
} \
/**/

#define OP_DIV_B_I(ctx, r, v0, i1) { \
    if (i1 < DBL_EPSILON) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO); \
    } \
    (r)->t = VAR_DBL; \
    (r)->d = ((double)(BzToDouble((v0)->bi->b))) / (i1); \
} \
/**/

#define OP_DIV_I_B(ctx, r, i0, v1) { \
    (r)->t = VAR_DBL; \
    (r)->d = ((double)(i0)) / (BzToDouble((v1)->bi->b)); \
} \
/**/

#define OP_DIV_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_DIV_I_I(ctx, r, i0, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_DIV_B_I(ctx, r, v0, i1) \
    } else { \
        e = div_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_DIV_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_DIV_I_I(ctx, r, i0, i1) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_DIV_I_B(ctx, r, i0, v1) \
    } else { \
        e = div_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_DIV(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_DIV_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_DIV_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            (r)->t = VAR_DBL; \
            (r)->d = ((double)(BzToDouble((v0)->bi->b))) / (BzToDouble((v1)->bi->b)); \
        } else { \
            e = div_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = div_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

/* MOD */

#define OP_NMOD_I_I(e, n, r, i0, i1, label) { \
    (r) = (i0) % (i1); \
} \
/**/

#define OP_MOD_I_I(ctx, r, i0, i1) { \
    if (i1 < DBL_EPSILON) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO); \
    } \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) % (i1); \
} \
/**/

#define OP_MOD_B_I(ctx, r, v0, i1) { \
    if (i1 < DBL_EPSILON) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO); \
    } \
    BigZ rx; \
    BigZ b1 = BzFromInteger(i1); \
    BigZ q = BzDivide((v0)->bi->b, b1, &rx); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, rx); \
    BzFree(b1); \
    BzFree(q); \
    bi_normalize(r); \
} \
/**/

#define OP_MOD_I_B(ctx, r, i0, v1) { \
    BigZ rx; \
    BigZ b0 = BzFromInteger(i0); \
    BigZ q = BzDivide(b0, (v1)->bi->b, &rx); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, rx); \
    BzFree(b0); \
    BzFree(q); \
    bi_normalize(r); \
} \
/**/

#define OP_MOD_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_MOD_I_I(ctx, r, i0, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_MOD_B_I(ctx, r, v0, i1) \
    } else { \
        e = mod_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_MOD_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_MOD_I_I(ctx, r, i0, i1) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_MOD_I_B(ctx, r, i0, v1) \
    } else { \
        e = mod_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_MOD(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_MOD_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_MOD_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            BigZ rx; \
            BigZ q = BzDivide((v0)->bi->b, (v1)->bi->b, &rx); \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, rx); \
            BzFree(q); \
            bi_normalize(r); \
        } else { \
            e = mod_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = mod_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

/* POW */

#define OP_NPOW_I_I(e, n, r, ip0, ip1, label) { \
    (r) = 1; \
    for (int i = 0; i < ip1; ++i) { \
        OP_NMUL_I_I(e, n, r, r, ip0, label); \
    } \
} \
/**/

#define OP_POW_I_I(ctx, r, ip0, ip1) { \
    (r)->t = VAR_INT64; \
    (r)->i = 1; \
    for (int i = 0; i < ip1; ++i) { \
        OP_MUL_V_I(ctx, (r), (r), (ip0)); \
    } \
} \
/**/

#define OP_POW_B_I(ctx, r, vp0, ip1) { \
    BigZ rx = BzPow((vp0)->bi->b, ip1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, rx); \
    bi_normalize(r); \
} \
/**/

#define OP_POW_I_B(ctx, r, i0, v1) { \
    e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION); \
} \
/**/

#define OP_POW_V_I(ctx, r, vp0, ip1) { \
    if ((vp0)->t == VAR_INT64) { \
        int64_t ip0 = (vp0)->i; \
        OP_POW_I_I(ctx, r, ip0, ip1) \
    } else if ((vp0)->t == VAR_BIG) { \
        OP_POW_B_I(ctx, r, vp0, ip1) \
    } else { \
        e = pow_v_i(ctx, r, vp0, ip1); \
    } \
} \
/**/

#define OP_POW_I_V(ctx, r, ip0, vp1) { \
    if ((vp1)->t == VAR_INT64) { \
        int64_t ip1 = (vp1)->i; \
        OP_POW_I_I(ctx, r, ip0, ip1) \
    } else if ((vp1)->t == VAR_BIG) { \
        OP_POW_I_B(ctx, r, ip0, vp1) \
    } else { \
        e = pow_i_v(ctx, r, ip0, vp1); \
    } \
} \
/**/

#define OP_POW(ctx, r, vp0, vp1) { \
    if ((vp0)->t == VAR_INT64) { \
        int64_t ip0 = (vp0)->i; \
        OP_POW_I_V(ctx, r, ip0, vp1) \
    } else if ((vp0)->t == VAR_BIG) { \
        if ((vp1)->t = VAR_INT64) { \
            int64_t ip1 = (vp1)->i; \
            OP_POW_B_I(ctx, r, vp0, ip1) \
        } else if ((vp1)->t = VAR_BIG) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION); \
        } else { \
            e = pow_v_v(ctx, r, vp0, vp1); \
        } \
    } else { \
        e = pow_v_v(ctx, r, vp0, vp1); \
    } \
} \
/**/

/* EQEQ */

#define OP_EQEQ_I_I(ctx, r, i0, i1) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) == (i1); \
} \
/**/

#define OP_EQEQ_B_I(ctx, r, v0, i1) { \
    (r)->t = VAR_INT64; \
    (r)->i = 0; \
} \
/**/

#define OP_EQEQ_I_B(ctx, r, i0, v1) { \
    (r)->t = VAR_INT64; \
    (r)->i = 0; \
} \
/**/

#define OP_EQEQ_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        OP_EQEQ_I_I(ctx, r, (v0)->i, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_EQEQ_B_I(ctx, r, v0, i1) \
    } else { \
        e = eqeq_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_EQEQ_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        OP_EQEQ_I_I(ctx, r, i0, (v1)->i) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_EQEQ_I_B(ctx, r, i0, v1) \
    } else { \
        e = eqeq_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_EQEQ(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_EQEQ_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_EQEQ_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c == BZ_EQ); \
        } else { \
            e = eqeq_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = eqeq_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

/* NEQ */

#define OP_NEQ_I_I(ctx, r, i0, i1) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) != (i1); \
} \
/**/

#define OP_NEQ_B_I(ctx, r, v0, i1) { \
    (r)->t = VAR_INT64; \
    (r)->i = 1; \
} \
/**/

#define OP_NEQ_I_B(ctx, r, i0, v1) { \
    (r)->t = VAR_INT64; \
    (r)->i = 1; \
} \
/**/

#define OP_NEQ_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        OP_NEQ_I_I(ctx, r, (v0)->i, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        (r)->t = VAR_INT64; \
        (r)->i = 1; \
    } else { \
        e = neq_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_NEQ_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        OP_NEQ_I_I(ctx, r, i0, (v1)->i) \
    } else if ((v1)->t == VAR_BIG) { \
        (r)->t = VAR_INT64; \
        (r)->i = 1; \
    } else { \
        e = neq_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_NEQ(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_NEQ_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            (r)->t = VAR_INT64; \
            (r)->i = 1; \
        } else if ((v1)->t = VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c != BZ_EQ); \
        } else { \
            e = neq_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = neq_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

/* LT */

#define OP_LT_I_I(ctx, r, i0, i1) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) < (i1); \
} \
/**/

#define OP_LT_B_I(ctx, r, v0, i1) { \
    BigZ b1 = BzFromInteger(i1); \
    BzCmp c = BzCompare((v0)->bi->b, b1); \
    BzFree(b1); \
    (r)->t = VAR_INT64; \
    (r)->i = (c == BZ_LT); \
} \
/**/

#define OP_LT_I_B(ctx, r, i0, v1) { \
    BigZ b0 = BzFromInteger(i0); \
    BzCmp c = BzCompare(b0, (v1)->bi->b); \
    BzFree(b0); \
    (r)->t = VAR_INT64; \
    (r)->i = (c == BZ_LT); \
} \
/**/

#define OP_LT_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        OP_LT_I_I(ctx, r, (v0)->i, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_LT_B_I(ctx, r, v0, i1) \
    } else { \
        e = lt_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_LT_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        OP_LT_I_I(ctx, r, i0, (v1)->i) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_LT_I_B(ctx, r, i0, v1) \
    } else { \
        e = lt_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_LT(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_LT_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_LT_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c == BZ_LT); \
        } else { \
            e = lt_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = lt_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

/* LE */

#define OP_LE_I_I(ctx, r, i0, i1) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) <= (i1); \
} \
/**/

#define OP_LE_B_I(ctx, r, v0, i1) { \
    BigZ b1 = BzFromInteger(i1); \
    BzCmp c = BzCompare((v0)->bi->b, b1); \
    BzFree(b1); \
    (r)->t = VAR_INT64; \
    (r)->i = (c != BZ_GT); \
} \
/**/

#define OP_LE_I_B(ctx, r, i0, v1) { \
    BigZ b0 = BzFromInteger(i0); \
    BzCmp c = BzCompare(b0, (v1)->bi->b); \
    BzFree(b0); \
    (r)->t = VAR_INT64; \
    (r)->i = (c != BZ_GT); \
} \
/**/

#define OP_LE_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        OP_LE_I_I(ctx, r, (v0)->i, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_LE_B_I(ctx, r, v0, i1) \
    } else { \
        e = le_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_LE_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        OP_LE_I_I(ctx, r, i0, (v1)->i) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_LE_I_B(ctx, r, i0, v1) \
    } else { \
        e = le_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_LE(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_LE_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_LE_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c != BZ_GT); \
        } else { \
            e = le_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = le_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

/* GT */

#define OP_GT_I_I(ctx, r, i0, i1) { OP_LT_I_I(ctx, r, i1, i0) }
#define OP_GT_B_I(ctx, r, v0, i1) { OP_LT_I_B(ctx, r, i1, v0) }
#define OP_GT_I_B(ctx, r, i0, v1) { OP_LT_B_I(ctx, r, v1, i0) }
#define OP_GT_V_I(ctx, r, v0, i1) { OP_LT_I_V(ctx, r, i1, v0) }
#define OP_GT_I_V(ctx, r, i0, v1) { OP_LT_V_I(ctx, r, v1, i0) }
#define OP_GT(ctx, r, v0, v1) { OP_LT(ctx, r, v1, v0) }

/* GE */

#define OP_GE_I_I(ctx, r, i0, i1) { OP_LE_I_I(ctx, r, i1, i0) }
#define OP_GE_B_I(ctx, r, v0, i1) { OP_LE_I_B(ctx, r, i1, v0) }
#define OP_GE_I_B(ctx, r, i0, v1) { OP_LE_B_I(ctx, r, v1, i0) }
#define OP_GE_V_I(ctx, r, v0, i1) { OP_LE_I_V(ctx, r, i1, v0) }
#define OP_GE_I_V(ctx, r, i0, v1) { OP_LE_V_I(ctx, r, v1, i0) }
#define OP_GE(ctx, r, v0, v1) { OP_LE(ctx, r, v1, v0) }

/* LGE */

#define OP_LGE_I_I(ctx, r, i0, i1) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) == (i1) ? 0 : ((i0) < (i1) ? -1 : 1); \
} \
/**/

#define OP_LGE_B_I(ctx, r, v0, i1) { \
    BigZ b1 = BzFromInteger(i1); \
    BzCmp c = BzCompare((v0)->bi->b, b1); \
    BzFree(b1); \
    (r)->t = VAR_INT64; \
    (r)->i = (c == BZ_EQ) ? 0 : ((c == BZ_LT) ? -1 : 1); \
} \
/**/

#define OP_LGE_I_B(ctx, r, i0, v1) { \
    BigZ b0 = BzFromInteger(i0); \
    BzCmp c = BzCompare(b0, (v1)->bi->b); \
    BzFree(b0); \
    (r)->t = VAR_INT64; \
    (r)->i = (c == BZ_EQ) ? 0 : ((c == BZ_LT) ? -1 : 1); \
} \
/**/

#define OP_LGE_V_I(ctx, r, v0, i1) { \
    if ((v0)->t == VAR_INT64) { \
        OP_LGE_I_I(ctx, r, (v0)->i, i1) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_LGE_B_I(ctx, r, v0, i1) \
    } else { \
        e = lge_v_i(ctx, r, v0, i1); \
    } \
} \
/**/

#define OP_LGE_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        OP_LGE_I_I(ctx, r, i0, (v1)->i) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_LGE_I_B(ctx, r, i0, v1) \
    } else { \
        e = lge_i_v(ctx, r, i0, v1); \
    } \
} \
/**/

#define OP_LGE(ctx, r, v0, v1) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_LGE_I_V(ctx, r, i0, v1) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t = VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_LGE_B_I(ctx, r, v0, i1) \
        } else if ((v1)->t = VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c == BZ_EQ) ? 0 : ((c == BZ_LT) ? -1 : 1); \
        } else { \
            e = lge_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = lge_v_v(ctx, r, v0, v1); \
    } \
} \
/**/

INLINE extern vmctx *initialize(void);
INLINE extern void finalize(vmctx *ctx);
INLINE extern void setup_context(vmctx *ctx);

INLINE extern vmfnc *alcfnc(vmctx *ctx, void *f, vmfrm *lex, const char *name, int args);
INLINE extern void pbakfnc(vmctx *ctx, vmfnc *p);
INLINE extern vmfrm *alcfrm(vmctx *ctx, int args);
INLINE extern void pbakfrm(vmctx *ctx, vmfrm *p);
INLINE extern vmstr *alcstr_str(vmctx *ctx, const char *s);
INLINE extern void pbakstr(vmctx *ctx, vmstr *p);
INLINE extern vmbgi *alcbgi_bigz(vmctx *ctx, BigZ bz);
INLINE extern void pbakbgi(vmctx *ctx, vmbgi *p);
INLINE extern vmobj *alcobj(vmctx *ctx);
INLINE extern void pbakobj(vmctx *ctx, vmobj *p);
INLINE extern vmvar *alcvar(vmctx *ctx, vartype t, int hold);
INLINE extern vmvar *alcvar_initial(vmctx *ctx);
INLINE extern vmvar *alcvar_obj(vmctx *ctx, vmobj *o);
INLINE extern vmvar *alcvar_fnc(vmctx *ctx, vmfnc *f);
INLINE extern vmvar *alcvar_int64(vmctx *ctx, int64_t i, int hold);
INLINE extern vmvar *alcvar_str(vmctx *ctx, const char *s);
INLINE extern vmvar *alcvar_bgistr(vmctx *ctx, const char *s, int radix);
INLINE extern void pbakvar(vmctx *ctx, vmvar *p);
INLINE extern vmvar *copy_var(vmctx *ctx, vmvar *src, int hold);
INLINE extern void copy_var_to(vmctx *ctx, vmvar *dst, vmvar *src);

INLINE extern void initialize_allocators(vmctx *ctx);
INLINE extern void mark_and_sweep(vmctx *ctx);
INLINE extern void count(vmctx *ctx);
INLINE extern int get_min2(int a0, int a1);
INLINE extern int get_min3(int a0, int a1, int a2);
INLINE extern int get_min4(int a0, int a1, int a2, int a3);
INLINE extern int get_min5(int a0, int a1, int a2, int a3, int a4);
INLINE extern void print_obj(vmctx *ctx, vmvar *v);
INLINE extern vmstr *format(vmctx *ctx, vmobj *v);

INLINE extern void bi_initialize(void);
INLINE extern void bi_finalize(void);
INLINE extern vmbgi *bi_copy(vmctx *ctx, vmbgi *src);
INLINE extern void bi_print(vmbgi *b);
INLINE extern void bi_str(char *buf, int max, vmbgi *b);

INLINE extern void print_escape_str(vmstr *vs);
INLINE extern vmstr *str_dup(vmctx *ctx, vmstr *vs);
INLINE extern vmstr *str_from_i64(vmctx *ctx, int64_t i);
INLINE extern vmstr *str_from_dbl(vmctx *ctx, double d);
INLINE extern vmstr *str_make_double(vmctx *ctx, vmstr *vs);
INLINE extern vmstr *str_make_ntimes(vmctx *ctx, vmstr *vs, int n);
INLINE extern vmstr *str_append(vmctx *ctx, vmstr *vs, const char *s, int len);
INLINE extern vmstr *str_append_ch(vmctx *ctx, vmstr *vs, const char ch);
INLINE extern vmstr *str_append_cp(vmctx *ctx, vmstr *vs, const char *s);
INLINE extern vmstr *str_append_str(vmctx *ctx, vmstr *vs, vmstr *s2);
INLINE extern vmstr *str_append_fmt(vmctx *ctx, vmstr *vs, const char *fmt, ...);
INLINE extern vmstr *str_append_fmt_dbl(vmctx *ctx, vmstr *vs, const char *fmt, double *d);
INLINE extern vmstr *str_append_i64(vmctx *ctx, vmstr *vs, int64_t i);
INLINE extern vmstr *str_append_dbl(vmctx *ctx, vmstr *vs, double d);
INLINE extern vmstr *str_make_path(vmctx *ctx, vmstr *v0, vmstr *v1);
INLINE extern vmstr *str_make_path_i64(vmctx *ctx, vmstr *v0, int64_t i);
INLINE extern vmstr *str_make_i64_path(vmctx *ctx, int64_t i, vmstr *v0);
INLINE extern vmstr *str_trim(vmctx *ctx, vmstr *vs, const char *ch);
INLINE extern vmstr *str_ltrim(vmctx *ctx, vmstr *vs, const char *ch);
INLINE extern vmstr *str_rtrim(vmctx *ctx, vmstr *vs, const char *ch);

INLINE extern void hashmap_print(vmobj *obj);
INLINE extern void hashmap_objprint(vmctx *ctx, vmobj *obj);
INLINE extern vmobj *hashmap_create(vmobj *h, int sz);
INLINE extern vmobj *hashmap_set(vmctx *ctx, vmobj *obj, const char *s, vmvar *v);
INLINE extern vmobj *hashmap_remove(vmctx *ctx, vmobj *obj, const char *s);
INLINE extern vmvar *hashmap_search(vmobj *obj, const char *s);
INLINE extern vmobj *hashmap_copy(vmctx *ctx, vmobj *h);
INLINE extern vmobj *hashmap_copy_method(vmctx *ctx, vmobj *src);
INLINE extern vmobj *array_create(vmobj *obj, int asz);
INLINE extern vmobj *array_set(vmctx *ctx, vmobj *obj, int64_t idx, vmvar *vs);
INLINE extern vmobj *array_push(vmctx *ctx, vmobj *obj, vmvar *vs);
INLINE extern vmobj *object_copy(vmctx *ctx, vmobj *src);

INLINE extern int run_global(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
INLINE extern int throw_system_exception(int line, vmctx *ctx, int id);
INLINE extern int exception_addtrace(vmctx *ctx, vmvar *e, const char *funcname, const char *filename, int linenum);
INLINE extern int exception_printtrace(vmctx *ctx, vmvar *e);
INLINE extern int exception_uncaught(vmctx *ctx, vmvar *e);

INLINE extern int add_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int add_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int add_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int sub_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int sub_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int sub_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int mul_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int mul_i_v(vmctx *ctx, vmvar *, int64_t ir, vmvar *v);
INLINE extern int mul_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int div_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int div_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int div_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int mod_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int mod_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int mod_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int eqeq_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int eqeq_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int eqeq_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int neq_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int neq_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int neq_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int lt_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int lt_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int lt_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int le_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int le_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int le_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int gt_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int gt_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int gt_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int ge_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int ge_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int ge_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int lge_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int lge_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int lge_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);

#endif /* KILITE_TEMPLATE_HEADER_H */
