#ifndef KILITE_TEMPLATE_HEADER_H
#define KILITE_TEMPLATE_HEADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <float.h>

extern BigZ i64maxp1;
extern BigZ i64minm1;
extern const char g_utf8bytes[];

// printf("%s:%d -> %s\n", __FILE__, __LINE__, __func__);

#ifndef __MIRC__
#define INLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#if defined(_WIN32) || defined(_WIN64)
#if defined(_MSC_VER)
#define alloca _alloca
#endif
#endif
#else
#ifndef PRId64
#define PRId64 "lld"
#endif
#ifndef PRIx64
#define PRIx64 "llx"
#endif
#define INLINE inline
typedef void FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
extern FILE *get_stdio_stdin(void);
extern FILE *get_stdio_stdout(void);
extern FILE *get_stdio_stderr(void);

extern FILE *fopen(const char *, const char *);
extern int fclose(FILE *);
extern int fprintf(FILE *, const char *, ...);
extern int printf(const char *, ...);
extern int sprintf(const char *, const char *, ...);
extern int64_t strtoll(const char*, char**, int);
extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void *memset(void *, int, size_t);
extern void *memcpy(void *, const void *, size_t);
extern void free(void *);
extern char *strcpy(char *, const char *);
extern char *strncpy(char *, const char *, size_t);
extern char *strstr(const char *, const char *);
extern int strcmp(const char *, const char *);
#ifndef NULL
#define NULL ((void*)0)
#endif
#endif

#define FLOW_EXCEPTION (1)
#define FLOW_YIELD (2)

/* System Exceptions */
enum {
    EXCEPT_EXCEPTION = 0,
    EXCEPT_NOT_IMPLEMENTED,
    EXCEPT_RUNTIME_EXCEPTION,
    EXCEPT_STACK_OVERFLOW,
    EXCEPT_DIVIDE_BY_ZERO,
    EXCEPT_UNSUPPORTED_OPERATION,
    EXCEPT_METHOD_MISSING,
    EXCEPT_NO_MATCHING_PATTERN,
    EXCEPT_TOO_FEW_ARGUMENTS,
    EXCEPT_TYPE_MISMATCH,
    EXCEPT_INVALID_FIBER_STATE,
    EXCEPT_DEAD_FIBER_CALLED,
    EXCEPT_RANGE_ERROR,
    EXCEPT_OUT_OF_RANGE_ERROR,
    EXCEPT_TOO_DEEP,
    EXCEPT_XML_ERROR,
    EXCEPT_FILE_ERROR,
    EXCEPT_ZIP_ERROR,
    EXCEPT_REGEX_ERROR,
    EXCEPT_MAX,
};

#define FRM_STACK_SIZE (1024)
#define VAR_STACK_SIZE (1024*16)
#define ALC_UNIT (1024)
#define ALC_UNIT_FRM (1024)
#define TICK_UNIT (1024*64)
#define STR_UNIT (64)
#define BIN_UNIT (64)
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

/***************************************************************************
 * Basic structures
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
    VAR_STRREF,
    VAR_BINREF,
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

typedef struct vmbin {
    struct vmbin *prv;  /* The link to the previous item in alive list. */
    struct vmbin *liv;  /* The link to the next item in alive list. */
    struct vmbin *nxt;  /* The link to the next item in free list. */
    struct vmbin *chn;  /* The link in allocated object list */

    int32_t flags;
    int cap;
    int len;
    uint8_t *s;
    uint8_t *hd;
} vmbin;

typedef struct vmobj {
    struct vmobj *prv;  /* The link to the previous item in alive list. */
    struct vmobj *liv;  /* The link to the next item in alive list. */
    struct vmobj *nxt;  /* The link to the next item in free list. */
    struct vmobj *chn;  /* The link in allocated object list */

    int32_t flags;
    int32_t is_sysobj;  /* This is the mark for the system object and automatically passed to the function. */
    int32_t is_formatter;
    int64_t is_false;
    int64_t idxsz;
    int64_t asz;
    int64_t hsz;
    struct vmvar **ary; /* Array holder */
    struct vmvar *map;  /* Hashmap holder */
} vmobj;

typedef void (*freep_func)(void *);
typedef struct vmvar {
    struct vmvar *prv;  /* The link to the previous item in alive list. */
    struct vmvar *liv;  /* The link to the next item in alive list. */
    struct vmvar *nxt;  /* The link to the next item in free list. */
    struct vmvar *chn;  /* The link in allocated object list */

    int32_t flags;
    int32_t hasht;
    int32_t push;
    vartype t;
    int64_t i;
    double d;
    vmbgi *bi;
    vmbin *bn;
    vmstr *s;
    vmobj *o;           /* The hashmap from string to object */
    void *p;            /* almighty holder */
    freep_func freep;   /* if set this, freep(p) will be called instead of free(). */
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
    int created;        /* 1 means it's right after created. */
    int yield;          /* The next position at the next call after yield. */
    int varcnt;         /* The number of variables this should hold. */
    struct vmfnc *yfnc; /* The resumed funtion after yield. */
    struct vmfrm *frm;  /* The funtion frame holding arguments */
    struct vmvar **vars;/* Hold the local condition when yield was done. */
    struct vmfrm *lex;  /* The lexical frame bound with this function */
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
 * Context
*/
typedef struct vmctx {
    int tick;
    int sweep;
    int gccnt;
    int verbose;
    int print_result;
    const char *msgbuf;     /* Temporary used for the exception message, etc. */

    int vstksz;
    int vstkp;
    vmvar *vstk;

    int fstksz;
    int fstkp;
    vmfrm **fstk;

    const char *lastapply;  /* For a class methodMissing method. */
    vmobj *hostObject;      /* For a class methodMissing method. */
    vmfnc *methodmissing;   /* For a global methodMissing method. */
    vmfnc *callee;          /* Callee function to manage a pure function. */
    vmvar *except;          /* Current exception that was thrown. */
    int exceptl;            /* The line where the exception occurred. */

    vmobj *args;            /* Holder of the program arguments. */
    vmobj *i;               /* Special object for integer. */
    vmobj *d;               /* Special object for double. */
    vmobj *s;               /* Special object for string. */
    vmobj *b;               /* Special object for binary. */
    vmobj *o;               /* Special object for object/array. */

    vmfnc *regex;           /* The function to create a Regex object. */
    vmfnc *regeq;           /* The function to compare Regex objects with `=~`. */
    vmfnc *regne;           /* The function to compare Regex objects with `!~`. */

    vmfrm *frm;             /* Reference to the global frame. */

    struct {
        vmvar var;
        vmfnc fnc;
        vmfrm frm;
        vmstr str;
        vmbin bin;
        vmbgi bgi;
        vmobj obj;
    } alc;
    struct {
        int var;
        int fnc;
        int frm;
        int str;
        int bin;
        int bgi;
        int obj;
    } cnt;
    struct {
        int var;
        int fnc;
        int frm;
        int str;
        int bin;
        int bgi;
        int obj;
    } fre;
} vmctx;

/***************************************************************************
 * Setup arguments
*/

#define SETUP_PROGRAM_ARGS(nv) { \
    SET_OBJ(nv, ctx->args); \
    ctx->args = NULL; \
} \
/**/

/***************************************************************************
 * Stack operations
*/

#define push_frm(ctx, e, frm, label, func, file, line) do { \
    if ((ctx)->fstksz <= (ctx)->fstkp) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW, "The frame stack was overflow"); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
    (((ctx)->fstk)[((ctx)->fstkp)++] = (frm)); \
} while (0)
/**/
#define pop_frm(ctx) (--((ctx)->fstkp))

#define alloc_var(ctx, n, label, func, file, line) do { \
    if ((ctx)->vstksz <= ((ctx)->vstkp + n)) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
    int ntp = (ctx)->vstkp + (n); \
    for (int i = (ctx)->vstkp; i < ntp; ++i) { \
        (ctx)->vstk[i].t = VAR_UNDEF; \
    } \
    ((ctx)->vstkp) = ntp; \
} while (0) \
/**/
#define vstackp(ctx) ((ctx)->vstkp)
#define push_var_def(ctx, label, func, file, line, pushcode) \
    do { \
        if ((ctx)->vstksz <= (ctx)->vstkp) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        vmvar *px = &(((ctx)->vstk)[((ctx)->vstkp)++]); \
        pushcode \
    } while (0) \
/**/
#define push_var(ctx, v, label, func, file, line)   push_var_def(ctx, label, func, file, line, { SHCOPY_VAR_TO(ctx, px, v); })
#define push_var_i(ctx, v, label, func, file, line) push_var_def(ctx, label, func, file, line, { px->t = VAR_INT64; px->i = (v); })
#define push_var_b(ctx, v, label, func, file, line) push_var_def(ctx, label, func, file, line, { px->t = VAR_BIG; px->bi = alcbgi_bigz(ctx, BzFromString((v), 10, BZ_UNTIL_END)); })
#define push_var_d(ctx, v, label, func, file, line) push_var_def(ctx, label, func, file, line, { px->t = VAR_DBL; px->d = (v); })
#define push_var_s(ctx, v, label, func, file, line) push_var_def(ctx, label, func, file, line, { px->t = VAR_STR; px->s = alcstr_str(ctx, v); })
#define push_var_n(ctx, v, label, func, file, line) push_var_def(ctx, label, func, file, line, { px->t = VAR_BIN; px->s = (v); })
#define push_var_f(ctx, v, label, func, file, line) push_var_def(ctx, label, func, file, line, { px->t = VAR_FNC; px->f = (v); })
#define push_var_o(ctx, v, label, func, file, line) push_var_def(ctx, label, func, file, line, { px->t = VAR_OBJ; px->o = (v); })
#define push_var_sys(ctx, v, fn, label, func, file, line) \
    if (v->t == VAR_OBJ) { \
        ctx->hostObject = v->o; \
        if (v->o->is_sysobj) { \
            if ((ctx)->vstksz <= (ctx)->vstkp) { \
                e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW, NULL); \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
            vmvar *px = &(((ctx)->vstk)[((ctx)->vstkp)++]); \
            SHCOPY_VAR_TO(ctx, px, v); \
            ++fn; \
        } \
    } \
    if (v->push) { \
        ++fn; \
        v->push = 0; \
    } \
/**/
#define push_var_a(ctx, v, fn, label, func, file, line) \
    if ((v)->t == VAR_OBJ) { \
        int idxsz = (v)->o->idxsz; \
        fn += idxsz - 1;\
        if ((ctx)->vstksz <= ((ctx)->vstkp + idxsz)) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_STACK_OVERFLOW, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        for (int i = idxsz - 1; i >= 0; --i) { \
            vmvar *px = &(((ctx)->vstk)[((ctx)->vstkp)++]); \
            vmvar *item = (v)->o->ary[i]; \
            if (item) { \
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

/***************************************************************************
 * Useful macros
*/

#define KL_CAST_TO_I64(ctx, v) { \
    if ((v)->t == VAR_UNDEF) { \
        (v)->t = VAR_INT64; \
        (v)->i = 0; \
    } else if ((v)->t == VAR_DBL) { \
        (v)->t = VAR_INT64; \
        (v)->i = (int64_t)((v)->d); \
    } else if ((v)->t != VAR_INT64) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, "Can't cast to integer"); \
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
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, "Can't cast to double"); \
    } \
} \
/**/

#define KL_SET_PROPERTY(o, name, vs) \
    hashmap_set(ctx, o, #name, vs); \
/**/

#define KL_SET_PROPERTY_O(o, name, obj) \
    hashmap_set(ctx, o, #name, alcvar_obj(ctx, obj)); \
/**/

#define KL_SET_PROPERTY_S(o, name, str) \
    hashmap_set(ctx, o, #name, alcvar_str(ctx, str)); \
/**/

#define KL_SET_PROPERTY_SV(o, name, sv) \
    hashmap_set(ctx, o, #name, alcvar_sv(ctx, sv)); \
/**/

#define KL_SET_PROPERTY_I(o, name, i64) \
    hashmap_set(ctx, o, #name, alcvar_int64(ctx, i64, 0)); \
/**/

#define KL_SET_METHOD(o, name, fname, lex, args) \
    hashmap_set(ctx, o, #name, alcvar_fnc(ctx, alcfnc(ctx, fname, lex, #name, args))); \
/**/

#define KL_SET_METHOD_F(o, name, f) \
    hashmap_set(ctx, o, #name, alcvar_fnc(ctx, f)); \
/**/


/***************************************************************************
 * Operation macros
*/

#define EXTERN_FUNC(name, vn) { \
    int name(vmctx *ctx, vmfrm *lex, vmvar *r, int ac); \
    vmfnc *f = alcfnc(ctx, name, frm, #name, 0); \
    SET_FNC(vn, f); \
} \
/**/

#define EXTERN_OBJECT(name, vn) { \
    int name(vmctx *ctx, vmfrm *lex, vmvar *r, int ac); \
    vmfnc *f = alcfnc(ctx, name, frm, #name, 0); \
    e = ((vmfunc_t)(f->f))(ctx, frm, vn, 0); \
} \
/**/

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
    const char *lastapply = ctx->lastapply; \
    ctx->lastapply = NULL; \
    vmobj *host = ctx->hostObject; \
    ctx->hostObject = NULL; \
    if ((v)->t == VAR_FNC) { \
        CALL((v)->f, ((v)->f)->lex, r, ac) \
    } else { \
        int done = 0; \
        if (host && lastapply) { \
            vmvar *mm = hashmap_search(host, "methodMissing"); \
            if (mm && mm->t == VAR_FNC) { \
                { push_var_s(ctx, lastapply, label, func, file, line); } \
                { push_var_o(ctx, host, label, func, file, line); } \
                CALL((mm->f), (mm->f)->lex, r, ac + 2) \
                done = 1; \
            } \
        } \
        if (!done) { \
            if (ctx->methodmissing) { \
                { push_var_s(ctx, "<global>", label, func, file, line); } \
                CALL((ctx->methodmissing), (ctx->methodmissing)->lex, r, ac + 1) \
            } else { \
                e = throw_system_exception(__LINE__, ctx, EXCEPT_METHOD_MISSING, NULL); \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } \
} \
/**/

#define CHECK_EXCEPTION(label, func, file, line) if (e == FLOW_EXCEPTION) { \
    exception_addtrace(ctx, ctx->except, func, file, line); \
    goto label; \
} \
/**/

#define THROW_EXCEPTION(ctx, v, label, func, file, line) { \
    SHCOPY_VAR_TO(ctx, ctx->except, v); \
    exception_addtrace(ctx, ctx->except, func, file, line); \
    e = FLOW_EXCEPTION; \
    goto label; \
} \
/**/

#define RETHROW(ctx, label, func, file, line) { \
    e = FLOW_EXCEPTION; \
    goto label; \
} \
/**/

#define GET_ITERATOR(ctx, lex, v, label, func, file, line) { \
    Iterator_create(ctx, lex, v, 0); \
} \
/**/

#define OP_JMP_IF_NOTEND(ctx, lex, v, label, func, file, line) { \
    vmvar *isend = hashmap_search(v->o, "isEnded"); \
    if (isend && isend->t == VAR_FNC) { \
        int pp = vstackp(ctx); \
        vmfnc *f = isend->f; \
        vmvar rx; \
        int ad0 = 0; \
        push_var_sys(ctx, v, ad0, label, func, file, line); \
        CALL(f, f->lex, &rx, ad0) \
        restore_vstackp(ctx, pp); \
        if (rx.t == VAR_UNDEF || (rx.t == VAR_INT64 && rx.i == 0)) goto label; \
    } else { \
        /* This is treated as the end. */ \
    } \
} \
/**/

#define SAVEN(i, nv) SHCOPY_VAR_TO(ctx, f->vars[i], nv)

#define RESUME_HOOK(ctx, ac, alc, label, func, file, line) { \
    if (yieldno > 0 && ctx->callee->yfnc) { \
        if (ac > 0) { \
            vmvar *a = local_var(ctx, alc); \
            push_var(ctx, a, label, func, file, line); \
        } \
        int pp = vstackp(ctx); \
        vmfnc *f = ctx->callee->yfnc; \
        CALL(f, f->lex, &yy, ac) \
        restore_vstackp(ctx, pp); \
        if (e == FLOW_YIELD) { \
            SHCOPY_VAR_TO(ctx, (r), &yy); \
            goto YEND; \
        } \
    } \
} \
/**/

#define RESUME(ctx, vn, ac, alc, label, func, file, line) { \
    vn = alcvar_initial(ctx); \
    vmvar *aa = local_var(ctx, alc); \
    SHCOPY_VAR_TO(ctx, vn, aa); \
} \
/**/

#define RESUME_SETUP(ynum, vn, label, func, file, line) if (yieldno == ynum) { \
    SHCOPY_VAR_TO(ctx, vn, &yy); \
    CHECK_EXCEPTION(label, func, file, line); \
} \
/**/

#define YIELD_FRM(ctx, cur, ynum, total, copyblock) { \
    vmfnc *f = ctx->callee; \
    f->yield = ynum; \
    f->frm = cur; \
    if (f->vars == NULL) { \
        f->vars = (vmvar**)calloc(total, sizeof(vmvar*)); \
        for (int i = 0; i < total; ++i) { \
            f->vars[i] = alcvar_initial(ctx); \
        } \
    } \
    f->varcnt = total; \
    copyblock; \
    e = FLOW_YIELD; \
    goto YEND;\
} \
/**/

#define YIELD(ctx, ynum, total, copyblock) { \
    vmfnc *f = ctx->callee; \
    f->yield = ynum; \
    if (f->vars == NULL) { \
        f->vars = (vmvar**)calloc(total, sizeof(vmvar*)); \
        for (int i = 0; i < total; ++i) { \
            f->vars[i] = alcvar_initial(ctx); \
        } \
    } \
    f->varcnt = total; \
    copyblock; \
    e = FLOW_YIELD; \
    goto YEND;\
} \
/**/

#define CHECK_YIELD_FRM(ctx, ret, cur, fnc, ynum, total, copyblock) \
    if (e == FLOW_YIELD) { \
        vmfnc *f = ctx->callee; \
        f->yield = ynum; \
        f->yfnc = fnc; \
        f->frm = cur; \
        if (f->vars == NULL) { \
            f->vars = (vmvar**)calloc(total, sizeof(vmvar*)); \
            for (int i = 0; i < total; ++i) { \
                f->vars[i] = alcvar_initial(ctx); \
            } \
        } \
        f->varcnt = total; \
        copyblock; \
        if (r != ret) { \
            SHCOPY_VAR_TO(ctx, (r), (ret)); \
        } \
        goto YEND;\
    } \
/**/

#define CHECK_YIELD(ctx, ret, fnc, ynum, total, copyblock) \
    if (e == FLOW_YIELD) { \
        vmfnc *f = ctx->callee; \
        f->yield = ynum; \
        f->yfnc = fnc; \
        if (f->vars == NULL) { \
            f->vars = (vmvar**)calloc(total, sizeof(vmvar*)); \
            for (int i = 0; i < total; ++i) { \
                f->vars[i] = alcvar_initial(ctx); \
            } \
        } \
        f->varcnt = total; \
        copyblock; \
        if (r != ret) { \
            SHCOPY_VAR_TO(ctx, (r), (ret)); \
        } \
        goto YEND;\
    } \
/**/

#define CATCH_EXCEPTION(ctx, v) { \
    e = 0; \
    SHCOPY_VAR_TO(ctx, v, ctx->except); \
} \
/**/

#define CHKMATCH_I64(v, vi, label, func, file, line) { \
    if ((v)->t != VAR_INT64 || (v)->i != (vi)) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_NO_MATCHING_PATTERN, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define CHKMATCH_DBL(v, vd, label, func, file, line) { \
    if ((v)->t != VAR_DBL || DBL_EPSILON <= ((v)->d - (vd))) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_NO_MATCHING_PATTERN, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define CHKMATCH_STR(v, i, label, func, file, line) { \
    if ((v)->t != VAR_STR || strcmp((v)->s->s, (vs)) != 0) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_NO_MATCHING_PATTERN, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

/* Copy Variable */

#define MAKE_SUPER(ctx, dst, src) { vmobj *o = hashmap_copy_method(ctx, src->o); (dst)->t = VAR_OBJ; (dst)->o = o; }

#define MAKE_RANGE_I_I(ctx, dst, r1, r2, excl, label, func, file, line) { \
    int beg = r1; \
    int end = r2; \
    Range_create_i(ctx, NULL, dst, &beg, &end, excl); \
} \
/**/

#define MAKE_RANGE_I_N(ctx, dst, r1, excl, label, func, file, line) { \
    int beg = r1; \
    Range_create_i(ctx, NULL, dst, &beg, NULL, excl); \
} \
/**/

#define MAKE_RANGE_N_I(ctx, dst, r2, excl, label, func, file, line) { \
    int end = r2; \
    Range_create_i(ctx, NULL, dst, NULL, &end, excl); \
} \
/**/

#define MAKE_RANGE(ctx, dst, r1, r2, excl, label, func, file, line) { \
    int pp = vstackp(ctx); \
    vmfnc *f = alcfnc(ctx, Range_create, NULL, "Range.create", 3); \
    { push_var_i(ctx, excl, label, func, file, line); } \
    { push_var(ctx, r2, label, func, file, line); } \
    { push_var(ctx, r1, label, func, file, line); } \
    CALL(f, NULL, dst, 3) \
    restore_vstackp(ctx, pp); \
} \
/**/

#define CHECK_RANGE(ctx, r1, r2, label, func, file, line) { \
    int pp = vstackp(ctx); \
    vmvar *includes = hashmap_search(r1->o, "includes"); \
    { push_var(ctx, r2, label, func, file, line); } \
    vmvar *px = &(((ctx)->vstk)[((ctx)->vstkp)++]); \
    SHCOPY_VAR_TO(ctx, px, r1); \
    CHECK_CALL(ctx, includes, label, r1, 2, func, file, line) \
    restore_vstackp(ctx, pp); \
    if (r1->t != VAR_INT64 || r1->i == 0) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_NO_MATCHING_PATTERN, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define SET_UNDEF(dst)  { (dst)->t = VAR_UNDEF;                                     }
#define SET_I64(dst, v) { (dst)->t = VAR_INT64; (dst)->i  = (v);                    }
#define SET_DBL(dst, v) { (dst)->t = VAR_DBL;   (dst)->d  = (v);                    }
#define SET_BIG(dst, v) { (dst)->t = VAR_BIG;   (dst)->bi = alcbgi_bigz(ctx, BzFromString((v), 10, BZ_UNTIL_END)); }
#define SET_STR(dst, v) { (dst)->t = VAR_STR;   (dst)->s  = alcstr_str(ctx, (v));   }
#define SET_SV(dst, v)  { (dst)->t = VAR_STR;   (dst)->s  = (v);                    }
#define SET_BIN(dst, v) { (dst)->t = VAR_BIN;   (dst)->bn = (v);                    }
#define SET_FNC(dst, v) { (dst)->t = VAR_FNC;   (dst)->f  = (v);                    }
#define SET_OBJ(dst, v) { (dst)->t = VAR_OBJ;   (dst)->o  = (v);                    }
#define SET_BIN_DATA(ctx, bin, idx, v, label, func, file, line) { \
    /* bin should be always binary here by compiler */ \
    if (!bin_set(bin->bn, idx, v)) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/
#define SET_BIN_DATA_I(ctx, bin, idx, v) { \
    /* bin should be always binary here by compiler */ \
    bin_set_i(bin->bn, idx, v); \
} \
/**/
#define SET_BIN_DATA_D(ctx, bin, idx, v) { \
    /* bin should be always binary here by compiler */ \
    double dv = v; \
    bin_set_d(bin->bn, idx, &dv); \
} \
/**/

#define SET_APPLY_I(ctx, r, iv, label, func, file, line) { \
    vmvar *dst = (r)->a; \
    if ((dst)->t == VAR_STRREF) { \
        vmstr *s = (dst)->s; \
        int ii = (int)((dst)->i); \
        if (ii < 0) { \
            do { ii += s->len; } while (ii < 0); \
        } \
        if (ii < s->len) { \
            s->s[ii] = iv; \
        } else { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_OUT_OF_RANGE_ERROR, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        SET_I64(r, iv) \
        (r)->a = NULL; \
    } else if ((dst)->t == VAR_BINREF) { \
        vmbin *bn = (dst)->bn; \
        int ii = (int)((dst)->i); \
        if (ii < 0) { \
            do { ii += bn->len; } while (ii < 0); \
        } \
        bin_set_i(bn, ii, iv); \
        SET_I64(r, iv) \
        (r)->a = NULL; \
    } else { \
        SET_I64(dst, iv) \
        SHMOVE_VAR_TO(ctx, (r), (r)->a) \
    } \
} \
/**/

#define SET_APPLY_D(ctx, r, dv, label, func, file, line) { \
    vmvar *dst = (r)->a; \
    if ((dst)->t == VAR_STRREF) { \
        vmstr *s = (dst)->s; \
        int ii = (int)((dst)->i); \
        if (ii < 0) { \
            do { ii += s->len; } while (ii < 0); \
        } \
        if (ii < s->len) { \
            s->s[ii] = (int)dv; \
        } else { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_OUT_OF_RANGE_ERROR, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        SET_I64(r, (int)dv) \
        (r)->a = NULL; \
    } else if ((dst)->t == VAR_BINREF) { \
        vmbin *bn = (dst)->bn; \
        int ii = (int)((dst)->i); \
        if (ii < 0) { \
            do { ii += bn->len; } while (ii < 0); \
        } \
        double dvv = dv; \
        bin_set_d(bn, ii, &dvv); \
        SET_I64(r, (int)dv) \
        (r)->a = NULL; \
    } else { \
        SET_DBL(dst, dv) \
        SHMOVE_VAR_TO(ctx, (r), (r)->a) \
    } \
} \
/**/

#define SET_APPLY_S(ctx, r, str, label, func, file, line) { \
    vmvar *dst = (r)->a; \
    if ((dst)->t == VAR_STRREF) { \
        vmstr *s = (dst)->s; \
        int ii = (int)((dst)->i); \
        if (ii < 0) { \
            do { ii += s->len; } while (ii < 0); \
        } \
        if (ii < s->len) { \
            s->s[ii] = (int)(str[0]); \
        } else { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_OUT_OF_RANGE_ERROR, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        SET_I64(r, (int)(str[0])) \
        (r)->a = NULL; \
    } else if ((dst)->t == VAR_BINREF) { \
        vmbin *bn = (dst)->bn; \
        int ii = (int)((dst)->i); \
        if (ii < 0) { \
            do { ii += bn->len; } while (ii < 0); \
        } \
        bin_set_i(bn, ii, str[0]); \
        SET_I64(r, (int)(str[0])) \
        (r)->a = NULL; \
    } else { \
        SET_STR(dst, str) \
        SHMOVE_VAR_TO(ctx, (r), (r)->a) \
    } \
} \
/**/

#define SET_APPLY_F(ctx, r, f, label, func, file, line) { \
    vmvar *dst = (r)->a; \
    if ((dst)->t == VAR_STRREF || (dst)->t == VAR_BINREF) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } else { \
        SET_FNC(dst, f) \
        SHMOVE_VAR_TO(ctx, (r), (r)->a) \
    } \
} \
/**/

#define SET_APPLY(ctx, r, v, label, func, file, line) { \
    if ((v)->t == VAR_INT64) { \
        int64_t i = (v)->i; \
        SET_APPLY_I(ctx, r, i, label, func, file, line); \
    } else if ((v)->t == VAR_DBL) { \
        double d = (v)->d; \
        SET_APPLY_D(ctx, r, d, label, func, file, line); \
    } else if ((v)->t == VAR_STR) { \
        const char *str = (v)->s->s; \
        SET_APPLY_S(ctx, r, str, label, func, file, line); \
    } else { \
        vmvar *dst = (r)->a; \
        if ((dst)->t == VAR_STRREF || (dst)->t == VAR_BINREF) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } else if ((v)->t == VAR_FNC) { \
            SET_FNC(dst, (v)->f) \
            SHMOVE_VAR_TO(ctx, (r), (r)->a) \
        } else { \
            COPY_VAR_TO(ctx, dst, v) \
            SHMOVE_VAR_TO(ctx, (r), (r)->a) \
        } \
    } \
} \
/**/

#define VALUE_PUSHN(ctx, r, label, func, file, line) { \
    if ((r)->t == VAR_OBJ) { \
        array_push(ctx, (r)->o, NULL); \
    } else if ((r)->t == VAR_BIN) { \
    } else { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define VALUE_PUSH(ctx, r, v, label, func, file, line) { \
    if ((r)->t == VAR_OBJ) { \
        array_push(ctx, (r)->o, v); \
    } else if ((r)->t == VAR_BIN) { \
        if ((v)->t == VAR_INT64) { \
            bin_append_ch(ctx, r->bn, (uint8_t)((v)->i)); \
        } else if ((v)->t == VAR_DBL) { \
            bin_append_ch(ctx, r->bn, (uint8_t)((v)->d)); \
        } else if ((v)->t == VAR_STR) { \
            bin_append_ch(ctx, r->bn, (uint8_t)((v)->s->s[0])); \
        } else { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } else { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define VALUE_PUSH_I(ctx, r, i, label, func, file, line) { \
    if ((r)->t == VAR_OBJ) { \
        array_push(ctx, (r)->o, alcvar_int64(ctx, i, 0)); \
    } else if ((r)->t == VAR_BIN) { \
        bin_append_ch(ctx, r->bn, (uint8_t)i); \
    } else { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define VALUE_PUSH_D(ctx, r, d, label, func, file, line) { \
    if ((r)->t == VAR_OBJ) { \
        double dbl = d; \
        array_push(ctx, (r)->o, alcvar_double(ctx, &dbl)); \
    } else if ((r)->t == VAR_BIN) { \
        bin_append_ch(ctx, (r)->bn, (uint8_t)d); \
    } else { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define VALUE_PUSH_S(ctx, r, str, label, func, file, line) { \
    if ((r)->t == VAR_OBJ) { \
        array_push(ctx, (r)->o, alcvar_str(ctx, str)); \
    } else if ((r)->t == VAR_BIN) { \
        bin_append(ctx, (r)->bn, str, strlen(str)); \
    } else { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define VALUE_PUSHX(ctx, r, v, label, func, file, line) { \
    if ((r)->t == VAR_OBJ) { \
        if ((v)->t == VAR_OBJ) { \
            vmobj *o = (v)->o; \
            for (int i = 0; i < o->idxsz; ++i) { \
                array_push(ctx, (r)->o, o->ary[i]); \
            } \
        } else if ((v)->t == VAR_BIN) { \
            vmbin *bn = (v)->bn; \
            for (int i = 0; i < bn->len; ++i) { \
                array_push(ctx, (r)->o, alcvar_int64(ctx, bn->hd[i], 0)); \
            } \
        } else if ((v)->t == VAR_STR) { \
            vmstr *s = (v)->s; \
            for (int i = 0; i < s->len; ++i) { \
                array_push(ctx, (r)->o, alcvar_int64(ctx, s->hd[i], 0)); \
            } \
        } else { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } else if ((r)->t == VAR_BIN) { \
        if ((v)->t == VAR_OBJ) { \
            vmobj *o = (v)->o; \
            for (int i = 0; i < o->idxsz; ++i) { \
                vmvar *aryi = o->ary[i]; \
                if (aryi) { \
                    if (aryi->t == VAR_INT64) { \
                        bin_append_ch(ctx, (r)->bn, (uint8_t)aryi->i); \
                    } else if (aryi->t == VAR_DBL) { \
                        bin_append_ch(ctx, (r)->bn, (uint8_t)(aryi->d)); \
                    } else if (aryi->t == VAR_STR) { \
                        bin_append_ch(ctx, (r)->bn, (uint8_t)aryi->s->s[0]); \
                    } else { \
                        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
                        exception_addtrace(ctx, ctx->except, func, file, line); \
                        goto label; \
                    } \
                } else { \
                    bin_append_ch(ctx, (r)->bn, 0); \
                } \
            } \
        } else if ((v)->t == VAR_BIN) { \
            bin_append_bin(ctx, (r)->bn, (v)->bn); \
        } else if ((v)->t == VAR_STR) { \
            vmstr *s = (v)->s; \
            bin_append(ctx, (r)->bn, s->hd, s->len); \
        } else { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } else { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define VALUE_PUSHX_S(ctx, r, str, label, func, file, line) { \
    if ((r)->t == VAR_OBJ) { \
        const char *s = str; \
        while (*s != 0) { \
            array_push(ctx, (r)->o, *s); \
        } \
    } else if ((r)->t == VAR_BIN) { \
        bin_append(ctx, (r)->bn, str, strlen(str)); \
    } else { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define VALUE_EXPAND(ctx, r, v, label, func, file, line) { \
    if ((r)->t == VAR_OBJ && (v)->t == VAR_OBJ) { \
        hashmap_append(ctx, (r)->o, (v)->o); \
    } else { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

#define SHCOPY_VAR_TO(ctx, dst, src) { \
    if (!src) { \
        (dst)->t = VAR_UNDEF; \
    } else switch ((src)->t) { \
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
    case VAR_BIN: \
        (dst)->t = VAR_BIN; \
        (dst)->bn = (src)->bn; \
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
    if (!src) { \
        (dst)->t = VAR_UNDEF; \
    } else switch ((src)->t) { \
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
    case VAR_BIN: \
        (dst)->t = VAR_BIN; \
        (dst)->bn = (src)->bn; \
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

#define SET_ARGVAR(vn, idx, alc) { \
    if (idx < ac) { \
        vmvar *aa = local_var(ctx, (idx + alc)); \
        SHCOPY_VAR_TO(ctx, n##vn, aa); \
    } else { \
        init_var(n##vn); \
    } \
} \
/**/

#define SET_ARGVAR_TYPE(vn, idx, alc, type) { \
    if (idx < ac) { \
        vmvar *aa = local_var(ctx, (idx + alc)); \
        if (aa->t != type) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL); \
            goto END; \
        } \
        SHCOPY_VAR_TO(ctx, n##vn, aa); \
    } else { \
        init_var(n##vn); \
    } \
} \
/**/

/* Conditional Jump */

#define OP_JMP_IF_TRUE(r, label) { \
    switch ((r)->t) { \
    case VAR_INT64: \
        if ((r)->i) goto label; \
        break; \
    case VAR_DBL: \
        if ((r)->d >= DBL_EPSILON) goto label; \
        break; \
    case VAR_BIG: \
        goto label; \
    case VAR_STR: \
        if ((r)->s->len > 0) goto label; \
        break; \
    case VAR_BIN: \
        if ((r)->bn->len > 0) goto label; \
        break; \
    case VAR_FNC: \
        goto label; \
    case VAR_OBJ: \
        if (!((r)->o->is_false)) goto label; \
        break; \
    default: \
        /* TODO */ \
        break; \
    } \
} \
/**/

#define OP_JMP_IF_FALSE(r, label) { \
    switch ((r)->t) { \
    case VAR_UNDEF: \
        goto label; \
    case VAR_INT64: \
        if ((r)->i == 0) goto label; \
        break; \
    case VAR_DBL: \
        if ((r)->d < DBL_EPSILON) goto label; \
        break; \
    case VAR_BIG: \
        break; \
    case VAR_STR: \
        if ((r)->s->len == 0) goto label; \
        break; \
    case VAR_BIN: \
        if ((r)->bn->len == 0) goto label; \
        break; \
    case VAR_FNC: \
        break; \
    case VAR_OBJ: \
        if ((r)->o->is_false) goto label; \
        break; \
    default: \
        /* TODO */ \
        break; \
    } \
} \
/**/

/* Object control */

#define OP_HASH_APPLY_OBJ(ctx, r, t1, str) { \
    ctx->lastapply = str; \
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
    vmvar *t1 = (v); \
    vmvar *t2 = NULL; \
    int done = 0; \
    switch ((t1)->t) { \
    case VAR_INT64: \
    case VAR_BIG: \
        t2 = hashmap_search(ctx->i, str); \
        break; \
    case VAR_DBL: \
        t2 = hashmap_search(ctx->d, str); \
        break; \
    case VAR_STR: \
        t2 = hashmap_search(ctx->s, str); \
        break; \
    case VAR_BIN: \
        t2 = hashmap_search(ctx->b, str); \
        break; \
    case VAR_OBJ: \
        ctx->lastapply = str; \
        if (t1->o) { \
            t2 = hashmap_search(t1->o, str); \
            if (t2) { \
                COPY_VAR_TO(ctx, (r), t2); \
                done = 1; \
            } \
        } \
        if (!done) { \
            t2 = hashmap_search(ctx->o, str); \
        } \
        break; \
    default: \
        break; \
    } \
    if (!done) { \
        if (t2 && t2->t == VAR_FNC) { \
            push_var(ctx, t1, END/* dummy */, "", "", 0); \
            (r)->t = VAR_FNC; \
            (r)->f = t2->f; \
            t1->push = 1; \
        } else { \
            (r)->t = VAR_UNDEF; \
        } \
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
    if ((v)->t == VAR_STR) { \
        int ii = idx; \
        int xlen = (v)->s->len; \
        if (ii < 0) { \
            do { ii += xlen; } while (ii < 0); \
        } \
        if (0 <= ii && ii < xlen && (v)->s->s[ii]) { \
            SET_I64(r, (v)->s->s[ii]); \
        } else { \
            (r)->t = VAR_UNDEF; \
        } \
    } else if ((v)->t == VAR_BIN) { \
        int ii = idx; \
        int xlen = (v)->bn->len; \
        if (ii < 0) { \
            do { ii += xlen; } while (ii < 0); \
        } \
        if (0 <= ii && ii < xlen && (v)->bn->s[ii]) { \
            SET_I64(r, (v)->bn->s[ii]); \
        } else { \
            (r)->t = VAR_UNDEF; \
        } \
    } else if ((v)->t == VAR_OBJ) { \
        int ii = idx; \
        int xlen = (v)->o->idxsz; \
        if (ii < 0) { \
            do { ii += xlen; } while (ii < 0); \
        } \
        if (0 <= ii && ii < xlen && (v)->o->ary[ii]) { \
            COPY_VAR_TO(ctx, r, (v)->o->ary[ii]); \
        } else { \
            (r)->t = VAR_UNDEF; \
        } \
    } else if (idx == 0) { \
        COPY_VAR_TO(ctx, r, v); \
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
    if ((v)->t == VAR_STR) { \
        (r)->t = VAR_STRREF; \
        (r)->i = idx; \
        (r)->s = (v)->s; \
        (r)->a = r; \
    } else if ((v)->t == VAR_BIN) { \
        (r)->t = VAR_BINREF; \
        (r)->i = idx; \
        (r)->bn = (v)->bn; \
        (r)->a = r; \
    } else { \
        OP_ARRAY_REFL_CHKV(ctx, t1, v) \
        int ii = idx; \
        if (ii < 0) { \
            do { ii += (t1)->o->idxsz; } while (ii < 0); \
        } \
        vmvar *t2 = NULL; \
        if (0 <= ii && ii < (t1)->o->idxsz) { \
            t2 = (t1)->o->ary[ii]; \
        } \
        if (!t2) { \
            t2 = alcvar_initial(ctx); \
            array_set(ctx, (t1)->o, ii, t2); \
        } \
        (r)->t = VAR_OBJ; \
        (r)->a = t2; \
    } \
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

#define OP_INC_SAME(ctx, r, label, func, file, line) { \
    if ((r)->t == VAR_INT64) { \
        OP_INC_SAME_I(ctx, r) \
    } else { \
        e = inc_v(ctx, r); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_INCP_SAME(ctx, r, label, func, file, line) { \
    if ((r)->t == VAR_INT64) { \
        OP_INC_SAME_I(ctx, r) \
    } else { \
        e = inc_v(ctx, r); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
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

#define OP_DEC_SAME(ctx, r, label, func, file, line) { \
    if ((r)->t == VAR_INT64) { \
        OP_DEC_SAME_I(ctx, r) \
    } else { \
        e = dec_v(ctx, r); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_DECP_SAME(ctx, r, label, func, file, line) { \
    if ((r)->t == VAR_INT64) { \
        OP_DEC_SAME_I(ctx, r) \
    } else { \
        e = dec_v(ctx, r); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_INC(ctx, r, v, label, func, file, line) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        OP_INC_SAME_I(ctx, t1) \
        SHCOPY_VAR_TO(ctx, r, t1) \
    } else { \
        e = inc_v(ctx, t1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        COPY_VAR_TO(ctx, r, t1) \
    } \
} \
/**/

#define OP_INCP(ctx, r, v, label, func, file, line) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        (r)->t = VAR_INT64; \
        (r)->i = ((t1)->i); \
        OP_INC_SAME_I(ctx, t1) \
    } else { \
        COPY_VAR_TO(ctx, r, t1) \
        e = inc_v(ctx, t1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_DEC(ctx, r, v, label, func, file, line) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        OP_DEC_SAME_I(ctx, t1) \
        SHCOPY_VAR_TO(ctx, r, t1) \
    } else { \
        e = dec_v(ctx, t1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
        COPY_VAR_TO(ctx, r, t1) \
    } \
} \
/**/

#define OP_DECP(ctx, r, v, label, func, file, line) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        (r)->t = VAR_INT64; \
        (r)->i = ((t1)->i); \
        OP_DEC_SAME_I(ctx, t1) \
    } else { \
        COPY_VAR_TO(ctx, r, t1) \
        e = dec_v(ctx, t1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* Unary operations */

#define OP_UMINUS(ctx, r, v, label, func, file, line) { \
    vmvar *t1 = (v); \
    if ((t1)->t == VAR_INT64) { \
        (r)->t = VAR_INT64; \
        (r)->i = -((t1)->i); \
    } else if ((t1)->t == VAR_DBL) { \
        (r)->t = VAR_DBL; \
        (r)->d = -((t1)->d); \
    } else { \
        e = uminus_v(ctx, r, t1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_CONV(ctx, r, v, label, func, file, line) { \
    vmvar *t1 = (v); \
    e = uconv_v(ctx, r, t1); \
    if (e == FLOW_EXCEPTION) { \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
} \
/**/

/* Bit NOT */

#define OP_BNOT_I(ctx, r, i0, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = ~(i0); \
} \
/**/

#define OP_BNOT_B(ctx, r, v0, label, func, file, line) { \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzNot((v0)->bi->b)); \
} \
/**/

#define OP_BNOT_V(ctx, r, v0, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_BNOT_I(ctx, r, (v0)->i, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_BNOT_B(ctx, r, v0, label, func, file, line) \
    } else { \
        e = bnot_v(ctx, r, v0); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* NOT */

#define OP_NOT_I(ctx, r, i0, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0 == 0) ? 1 : 0; \
} \
/**/

#define OP_NOT_V(ctx, r, v0, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_NOT_I(ctx, r, i0, label, func, file, line) \
    } else { \
        e = not_v(ctx, r, v0); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
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

#define OP_ADD_I_I(ctx, r, i0, i1, label, func, file, line) { \
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

#define OP_ADD_B_I(ctx, r, v0, i1, label, func, file, line) { \
    BigZ bi = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAdd((v0)->bi->b, bi)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_ADD_I_B(ctx, r, i0, v1, label, func, file, line) { \
    BigZ bi = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAdd(bi, (v1)->bi->b)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_ADD_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_ADD_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_ADD_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = add_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_ADD_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_ADD_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_ADD_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = add_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_ADD(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_ADD_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_ADD_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzAdd((v0)->bi->b, (v1)->bi->b)); \
            bi_normalize(r); \
        } else { \
            e = add_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = add_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
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

#define OP_SUB_I_I(ctx, r, i0, i1, label, func, file, line) { \
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

#define OP_SUB_B_I(ctx, r, v0, i1, label, func, file, line) { \
    BigZ bi = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzSubtract((v0)->bi->b, bi)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_SUB_I_B(ctx, r, i0, v1, label, func, file, line) { \
    BigZ bi = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzSubtract(bi, (v1)->bi->b)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_SUB_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_SUB_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_SUB_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = sub_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_SUB_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_SUB_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_SUB_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = sub_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_SUB(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_SUB_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_SUB_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzSubtract((v0)->bi->b, (v1)->bi->b)); \
        } else { \
            e = sub_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = sub_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
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

#define OP_MUL_I_I(ctx, r, i0, i1, label, func, file, line) { \
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

#define OP_MUL_B_I(ctx, r, v0, i1, label, func, file, line) { \
    BigZ bi = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzMultiply((v0)->bi->b, bi)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_MUL_I_B(ctx, r, i0, v1, label, func, file, line) { \
    BigZ bi = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzMultiply(bi, (v1)->bi->b)); \
    BzFree(bi); \
    bi_normalize(r); \
} \
/**/

#define OP_MUL_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_MUL_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_MUL_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = mul_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_MUL_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_MUL_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_MUL_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = mul_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_MUL(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_MUL_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_MUL_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzMultiply((v0)->bi->b, (v1)->bi->b)); \
        } else { \
            e = mul_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = mul_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* DIV */

#define OP_DIV_I_I(ctx, r, i0, i1, label, func, file, line) { \
    if (i1 == 0) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
    (r)->t = VAR_DBL; \
    (r)->d = ((double)(i0)) / (i1); \
} \
/**/

#define OP_DIV_B_I(ctx, r, v0, i1, label, func, file, line) { \
    if (i1 == 0) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
    (r)->t = VAR_DBL; \
    (r)->d = ((double)(BzToDouble((v0)->bi->b))) / (i1); \
} \
/**/

#define OP_DIV_I_B(ctx, r, i0, v1, label, func, file, line) { \
    (r)->t = VAR_DBL; \
    (r)->d = ((double)(i0)) / (BzToDouble((v1)->bi->b)); \
} \
/**/

#define OP_DIV_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_DIV_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_DIV_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = div_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_DIV_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_DIV_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_DIV_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = div_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_DIV(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_DIV_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_DIV_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            (r)->t = VAR_DBL; \
            (r)->d = ((double)(BzToDouble((v0)->bi->b))) / (BzToDouble((v1)->bi->b)); \
        } else { \
            e = div_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = div_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* MOD */

#define OP_NMOD_I_I(e, n, r, i0, i1, label) { \
    (r) = (i0) % (i1); \
} \
/**/

#define OP_MOD_I_I(ctx, r, i0, i1, label, func, file, line) { \
    if (i1 < DBL_EPSILON) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
    } \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) % (i1); \
} \
/**/

#define OP_MOD_B_I(ctx, r, v0, i1, label, func, file, line) { \
    if (i1 < DBL_EPSILON) { \
        e = throw_system_exception(__LINE__, ctx, EXCEPT_DIVIDE_BY_ZERO, NULL); \
        exception_addtrace(ctx, ctx->except, func, file, line); \
        goto label; \
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

#define OP_MOD_I_B(ctx, r, i0, v1, label, func, file, line) { \
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

#define OP_MOD_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_MOD_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_MOD_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = mod_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_MOD_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        int64_t i1 = (v1)->i; \
        OP_MOD_I_I(ctx, r, i0, i1, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_MOD_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = mod_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_MOD(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_MOD_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_MOD_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            BigZ rx; \
            BigZ q = BzDivide((v0)->bi->b, (v1)->bi->b, &rx); \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, rx); \
            BzFree(q); \
            bi_normalize(r); \
        } else { \
            e = mod_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = mod_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
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

#define OP_POW_I_I(ctx, r, ip0, ip1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = 1; \
    for (int i = 0; i < ip1; ++i) { \
        OP_MUL_V_I(ctx, (r), (r), (ip0), label, func, file, line); \
    } \
} \
/**/

#define OP_POW_B_I(ctx, r, vp0, ip1, label, func, file, line) { \
    BigZ rx = BzPow((vp0)->bi->b, ip1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, rx); \
    bi_normalize(r); \
} \
/**/

#define OP_POW_I_B(ctx, r, i0, v1, label, func, file, line) { \
    e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
    exception_addtrace(ctx, ctx->except, func, file, line); \
    goto label; \
} \
/**/

#define OP_POW_V_I(ctx, r, vp0, ip1, label, func, file, line) { \
    if ((vp0)->t == VAR_INT64) { \
        int64_t ip0 = (vp0)->i; \
        OP_POW_I_I(ctx, r, ip0, ip1, label, func, file, line) \
    } else if ((vp0)->t == VAR_BIG) { \
        OP_POW_B_I(ctx, r, vp0, ip1, label, func, file, line) \
    } else { \
        e = pow_v_i(ctx, r, vp0, ip1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_POW_I_V(ctx, r, ip0, vp1, label, func, file, line) { \
    if ((vp1)->t == VAR_INT64) { \
        int64_t ip1 = (vp1)->i; \
        OP_POW_I_I(ctx, r, ip0, ip1, label, func, file, line) \
    } else if ((vp1)->t == VAR_BIG) { \
        OP_POW_I_B(ctx, r, ip0, vp1, label, func, file, line) \
    } else { \
        e = pow_i_v(ctx, r, ip0, vp1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_POW(ctx, r, vp0, vp1, label, func, file, line) { \
    if ((vp0)->t == VAR_INT64) { \
        int64_t ip0 = (vp0)->i; \
        OP_POW_I_V(ctx, r, ip0, vp1, label, func, file, line) \
    } else if ((vp0)->t == VAR_BIG) { \
        if ((vp1)->t = VAR_INT64) { \
            int64_t ip1 = (vp1)->i; \
            OP_POW_B_I(ctx, r, vp0, ip1, label, func, file, line) \
        } else if ((vp1)->t = VAR_BIG) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } else { \
            e = pow_v_v(ctx, r, vp0, vp1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = pow_v_v(ctx, r, vp0, vp1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* EQEQ */

#define OP_EQEQ_I_I(ctx, r, i0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) == (i1); \
} \
/**/

#define OP_EQEQ_B_I(ctx, r, v0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = 0; \
} \
/**/

#define OP_EQEQ_I_B(ctx, r, i0, v1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = 0; \
} \
/**/

#define OP_EQEQ_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_EQEQ_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_EQEQ_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = eqeq_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_EQEQ_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_EQEQ_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_EQEQ_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = eqeq_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_EQEQ(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_EQEQ_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_EQEQ_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c == BZ_EQ); \
        } else { \
            e = eqeq_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = eqeq_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* NEQ */

#define OP_NEQ_I_I(ctx, r, i0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) != (i1); \
} \
/**/

#define OP_NEQ_B_I(ctx, r, v0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = 1; \
} \
/**/

#define OP_NEQ_I_B(ctx, r, i0, v1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = 1; \
} \
/**/

#define OP_NEQ_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_NEQ_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        (r)->t = VAR_INT64; \
        (r)->i = 1; \
    } else { \
        e = neq_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_NEQ_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_NEQ_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        (r)->t = VAR_INT64; \
        (r)->i = 1; \
    } else { \
        e = neq_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_NEQ(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_NEQ_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            (r)->t = VAR_INT64; \
            (r)->i = 1; \
        } else if ((v1)->t == VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c != BZ_EQ); \
        } else { \
            e = neq_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = neq_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* LT */

#define OP_LT_I_I(ctx, r, i0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) < (i1); \
} \
/**/

#define OP_LT_B_I(ctx, r, v0, i1, label, func, file, line) { \
    BigZ b1 = BzFromInteger(i1); \
    BzCmp c = BzCompare((v0)->bi->b, b1); \
    BzFree(b1); \
    (r)->t = VAR_INT64; \
    (r)->i = (c == BZ_LT); \
} \
/**/

#define OP_LT_I_B(ctx, r, i0, v1, label, func, file, line) { \
    BigZ b0 = BzFromInteger(i0); \
    BzCmp c = BzCompare(b0, (v1)->bi->b); \
    BzFree(b0); \
    (r)->t = VAR_INT64; \
    (r)->i = (c == BZ_LT); \
} \
/**/

#define OP_LT_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_LT_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_LT_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = lt_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_LT_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_LT_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_LT_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = lt_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_LT(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_LT_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_LT_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c == BZ_LT); \
        } else { \
            e = lt_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = lt_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* LE */

#define OP_LE_I_I(ctx, r, i0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) <= (i1); \
} \
/**/

#define OP_LE_B_I(ctx, r, v0, i1, label, func, file, line) { \
    BigZ b1 = BzFromInteger(i1); \
    BzCmp c = BzCompare((v0)->bi->b, b1); \
    BzFree(b1); \
    (r)->t = VAR_INT64; \
    (r)->i = (c != BZ_GT); \
} \
/**/

#define OP_LE_I_B(ctx, r, i0, v1, label, func, file, line) { \
    BigZ b0 = BzFromInteger(i0); \
    BzCmp c = BzCompare(b0, (v1)->bi->b); \
    BzFree(b0); \
    (r)->t = VAR_INT64; \
    (r)->i = (c != BZ_GT); \
} \
/**/

#define OP_LE_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_LE_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_LE_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = le_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_LE_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_LE_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_LE_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = le_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_LE(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_LE_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_LE_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c != BZ_GT); \
        } else { \
            e = le_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = le_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* GT */

#define OP_GT_I_I(ctx, r, i0, i1, label, func, file, line) { OP_LT_I_I(ctx, r, i1, i0, label, func, file, line) }
#define OP_GT_B_I(ctx, r, v0, i1, label, func, file, line) { OP_LT_I_B(ctx, r, i1, v0, label, func, file, line) }
#define OP_GT_I_B(ctx, r, i0, v1, label, func, file, line) { OP_LT_B_I(ctx, r, v1, i0, label, func, file, line) }
#define OP_GT_V_I(ctx, r, v0, i1, label, func, file, line) { OP_LT_I_V(ctx, r, i1, v0, label, func, file, line) }
#define OP_GT_I_V(ctx, r, i0, v1, label, func, file, line) { OP_LT_V_I(ctx, r, v1, i0, label, func, file, line) }
#define OP_GT(ctx, r, v0, v1, label, func, file, line) { OP_LT(ctx, r, v1, v0, label, func, file, line) }

/* GE */

#define OP_GE_I_I(ctx, r, i0, i1, label, func, file, line) { OP_LE_I_I(ctx, r, i1, i0, label, func, file, line) }
#define OP_GE_B_I(ctx, r, v0, i1, label, func, file, line) { OP_LE_I_B(ctx, r, i1, v0, label, func, file, line) }
#define OP_GE_I_B(ctx, r, i0, v1, label, func, file, line) { OP_LE_B_I(ctx, r, v1, i0, label, func, file, line) }
#define OP_GE_V_I(ctx, r, v0, i1, label, func, file, line) { OP_LE_I_V(ctx, r, i1, v0, label, func, file, line) }
#define OP_GE_I_V(ctx, r, i0, v1, label, func, file, line) { OP_LE_V_I(ctx, r, v1, i0, label, func, file, line) }
#define OP_GE(ctx, r, v0, v1, label, func, file, line) { OP_LE(ctx, r, v1, v0, label, func, file, line) }

/* LGE */

#define OP_LGE_I_I(ctx, r, i0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) == (i1) ? 0 : ((i0) < (i1) ? -1 : 1); \
} \
/**/

#define OP_LGE_B_I(ctx, r, v0, i1, label, func, file, line) { \
    BigZ b1 = BzFromInteger(i1); \
    BzCmp c = BzCompare((v0)->bi->b, b1); \
    BzFree(b1); \
    (r)->t = VAR_INT64; \
    (r)->i = (c == BZ_EQ) ? 0 : ((c == BZ_LT) ? -1 : 1); \
} \
/**/

#define OP_LGE_I_B(ctx, r, i0, v1, label, func, file, line) { \
    BigZ b0 = BzFromInteger(i0); \
    BzCmp c = BzCompare(b0, (v1)->bi->b); \
    BzFree(b0); \
    (r)->t = VAR_INT64; \
    (r)->i = (c == BZ_EQ) ? 0 : ((c == BZ_LT) ? -1 : 1); \
} \
/**/

#define OP_LGE_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_LGE_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_LGE_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = lge_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_LGE_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_LGE_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_LGE_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = lge_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_LGE(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_LGE_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_LGE_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            BzCmp c = BzCompare((v0)->bi->b, (v1)->bi->b); \
            (r)->t = VAR_INT64; \
            (r)->i = (c == BZ_EQ) ? 0 : ((c == BZ_LT) ? -1 : 1); \
        } else { \
            e = lge_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = lge_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* Bit Shift-L */

#define OP_BSHL_I_I(ctx, r, i0, i1, label, func, file, line) { \
    if (i0 > 0 && i1 > 0) { \
        if ((i1 >= 64) || (i0 > (INT64_MAX >> i1))) { \
            BigZ b0 = BzFromInteger(i0); \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzAsh(b0, i1)); \
            BzFree(b0); \
        } else { \
            (r)->t = VAR_INT64; \
            (r)->i = (i0) << (i1); \
        } \
    } else { \
        (r)->t = VAR_INT64; \
        (r)->i = (i0) << (i1); \
    } \
} \
/**/

#define OP_BSHL_B_I(ctx, r, v0, i1, label, func, file, line) { \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAsh((v0)->bi->b, i1)); \
} \
/**/

#define OP_BSHL_I_B(ctx, r, i0, v1, label, func, file, line) { \
    e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
    exception_addtrace(ctx, ctx->except, func, file, line); \
    goto label; \
} \
/**/

#define OP_BSHL_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_BSHL_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_BSHL_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = bshl_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BSHL_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_BSHL_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_BSHL_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = bshl_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BSHL(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_BSHL_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_BSHL_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } else { \
            e = bshl_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = bshl_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* Bit Shift-R */

#define OP_BSHR_I_I(ctx, r, i0, i1, label, func, file, line) { \
    if (i0 > 0 && i1 < 0) { \
        if ((i1 >= 64) || (i0 > (INT64_MAX >> -(i1)))) { \
            BigZ b0 = BzFromInteger(i0); \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzAsh(b0, -(i1))); \
            BzFree(b0); \
        } else { \
            (r)->t = VAR_INT64; \
            (r)->i = (i0) >> (i1); \
        } \
    } else { \
        (r)->t = VAR_INT64; \
        (r)->i = (i0) >> (i1); \
    } \
} \
/**/

#define OP_BSHR_B_I(ctx, r, v0, i1, label, func, file, line) { \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAsh((v0)->bi->b, -(i1))); \
} \
/**/

#define OP_BSHR_I_B(ctx, r, i0, v1, label, func, file, line) { \
    e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
    exception_addtrace(ctx, ctx->except, func, file, line); \
    goto label; \
} \
/**/

#define OP_BSHR_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_BSHR_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_BSHR_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = bshr_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BSHR_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_BSHR_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_BSHR_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = bshr_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BSHR(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_BSHR_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_BSHR_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            e = throw_system_exception(__LINE__, ctx, EXCEPT_UNSUPPORTED_OPERATION, NULL); \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } else { \
            e = bshr_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = bshr_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* Bit AND */

#define OP_BAND_I_I(ctx, r, i0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) & (i1); \
} \
/**/

#define OP_BAND_B_I(ctx, r, v0, i1, label, func, file, line) { \
    BigZ b1 = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAnd((v0)->bi->b, b1)); \
    BzFree(b1); \
} \
/**/

#define OP_BAND_I_B(ctx, r, i0, v1, label, func, file, line) { \
    BigZ b0 = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzAnd(b0, (v1)->bi->b)); \
    BzFree(b0); \
} \
/**/

#define OP_BAND_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_BAND_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_BAND_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = band_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BAND_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_BAND_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_BAND_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = band_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BAND(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_BAND_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_BAND_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzAnd((v0)->bi->b, (v1)->bi->b)); \
        } else { \
            e = band_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = band_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* Bit OR */

#define OP_BOR_I_I(ctx, r, i0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) | (i1); \
} \
/**/

#define OP_BOR_B_I(ctx, r, v0, i1, label, func, file, line) { \
    BigZ b1 = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzOr((v0)->bi->b, b1)); \
    BzFree(b1); \
} \
/**/

#define OP_BOR_I_B(ctx, r, i0, v1, label, func, file, line) { \
    BigZ b0 = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzOr(b0, (v1)->bi->b)); \
    BzFree(b0); \
} \
/**/

#define OP_BOR_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_BOR_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_BOR_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = bor_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BOR_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_BOR_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_BOR_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = bor_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BOR(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_BOR_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_BOR_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzOr((v0)->bi->b, (v1)->bi->b)); \
        } else { \
            e = bor_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = bor_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

/* Bit XOR */

#define OP_BXOR_I_I(ctx, r, i0, i1, label, func, file, line) { \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) ^ (i1); \
} \
/**/

#define OP_BXOR_B_I(ctx, r, v0, i1, label, func, file, line) { \
    BigZ b1 = BzFromInteger(i1); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzXor((v0)->bi->b, b1)); \
    BzFree(b1); \
} \
/**/

#define OP_BXOR_I_B(ctx, r, i0, v1, label, func, file, line) { \
    BigZ b0 = BzFromInteger(i0); \
    (r)->t = VAR_BIG; \
    (r)->bi = alcbgi_bigz(ctx, BzXor(b0, (v1)->bi->b)); \
    BzFree(b0); \
} \
/**/

#define OP_BXOR_V_I(ctx, r, v0, i1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        OP_BXOR_I_I(ctx, r, (v0)->i, i1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        OP_BXOR_B_I(ctx, r, v0, i1, label, func, file, line) \
    } else { \
        e = bor_v_i(ctx, r, v0, i1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BXOR_I_V(ctx, r, i0, v1, label, func, file, line) { \
    if ((v1)->t == VAR_INT64) { \
        OP_BXOR_I_I(ctx, r, i0, (v1)->i, label, func, file, line) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_BXOR_I_B(ctx, r, i0, v1, label, func, file, line) \
    } else { \
        e = bor_i_v(ctx, r, i0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

#define OP_BXOR(ctx, r, v0, v1, label, func, file, line) { \
    if ((v0)->t == VAR_INT64) { \
        int64_t i0 = (v0)->i; \
        OP_BXOR_I_V(ctx, r, i0, v1, label, func, file, line) \
    } else if ((v0)->t == VAR_BIG) { \
        if ((v1)->t == VAR_INT64) { \
            int64_t i1 = (v1)->i; \
            OP_BXOR_B_I(ctx, r, v0, i1, label, func, file, line) \
        } else if ((v1)->t == VAR_BIG) { \
            (r)->t = VAR_BIG; \
            (r)->bi = alcbgi_bigz(ctx, BzXor((v0)->bi->b, (v1)->bi->b)); \
        } else { \
            e = bor_v_v(ctx, r, v0, v1); \
            if (e == FLOW_EXCEPTION) { \
                exception_addtrace(ctx, ctx->except, func, file, line); \
                goto label; \
            } \
        } \
    } else { \
        e = bor_v_v(ctx, r, v0, v1); \
        if (e == FLOW_EXCEPTION) { \
            exception_addtrace(ctx, ctx->except, func, file, line); \
            goto label; \
        } \
    } \
} \
/**/

INLINE extern vmctx *initialize(void);
INLINE extern void finalize(vmctx *ctx);
INLINE extern void setup_context(vmctx *ctx);
INLINE extern void finalize_context(vmctx *ctx);
INLINE extern void Math_initialize(void);
INLINE extern void Math_finalize(void);
INLINE extern void Regex_initialize(void);
INLINE extern void Regex_finalize(void);

INLINE extern vmfnc *alcfnc(vmctx *ctx, void *f, vmfrm *lex, const char *name, int args);
INLINE extern void pbakfnc(vmctx *ctx, vmfnc *p);
INLINE extern vmfrm *alcfrm(vmctx *ctx, vmfrm *lex, int args);
INLINE extern void pbakfrm(vmctx *ctx, vmfrm *p);
INLINE extern vmstr *alcstr_allocated_str(vmctx *ctx, char *s, int alloclen);
INLINE extern vmstr *alcstr_str(vmctx *ctx, const char *s);
INLINE extern vmstr *alcstr_str_len(vmctx *ctx, const char *s, int len);
INLINE extern void pbakstr(vmctx *ctx, vmstr *p);
INLINE extern vmbin *alcbin_allocated_bin(vmctx *ctx, uint8_t *s, int alloclen);
INLINE extern vmbin *alcbin_bin(vmctx *ctx, const uint8_t *s, int len);
INLINE extern vmbin *alcbin(vmctx *ctx);
INLINE extern void pbakbin(vmctx *ctx, vmbin *p);
INLINE extern vmbgi *alcbgi_bigz(vmctx *ctx, BigZ bz);
INLINE extern void pbakbgi(vmctx *ctx, vmbgi *p);
INLINE extern vmobj *alcobj(vmctx *ctx);
INLINE extern void pbakobj(vmctx *ctx, vmobj *p);
INLINE extern vmvar *alcvar(vmctx *ctx, vartype t, int hold);
INLINE extern vmvar *alcvar_initial(vmctx *ctx);
INLINE extern vmvar *alcvar_obj(vmctx *ctx, vmobj *o);
INLINE extern vmvar *alcvar_fnc(vmctx *ctx, vmfnc *f);
INLINE extern vmvar *alcvar_int64(vmctx *ctx, int64_t i, int hold);
INLINE extern vmvar *alcvar_double(vmctx *ctx, double *d);
INLINE extern vmvar *alcvar_str(vmctx *ctx, const char *s);
INLINE extern vmvar *alcvar_sv(vmctx *ctx, vmstr *sv);
INLINE extern vmvar *alcvar_bin(vmctx *ctx, const uint8_t *s, int len);
INLINE extern vmvar *alcvar_bgistr(vmctx *ctx, const char *s, int radix);
INLINE extern void pbakvar(vmctx *ctx, vmvar *p);
INLINE extern vmvar *copy_var(vmctx *ctx, vmvar *src, int hold);
INLINE extern void copy_var_to(vmctx *ctx, vmvar *dst, vmvar *src);

INLINE extern void initialize_allocators(vmctx *ctx);
INLINE extern void mark_and_sweep(vmctx *ctx);
INLINE extern void count(vmctx *ctx);
INLINE extern vmfrm *get_lex(vmfrm* lex, int c);
INLINE extern int get_min2(int a0, int a1);
INLINE extern int get_min3(int a0, int a1, int a2);
INLINE extern int get_min4(int a0, int a1, int a2, int a3);
INLINE extern int get_min5(int a0, int a1, int a2, int a3, int a4);
INLINE extern void print_obj(vmctx *ctx, vmvar *v);
INLINE extern void fprint_obj(vmctx *ctx, vmvar *v, FILE *fp);
INLINE extern vmstr *format(vmctx *ctx, vmobj *v);

INLINE extern void bi_initialize(void);
INLINE extern void bi_finalize(void);
INLINE extern void bi_normalize(vmvar *v);
INLINE extern vmbgi *bi_copy(vmctx *ctx, vmbgi *src);
INLINE extern void bi_print(vmbgi *b);
INLINE extern void bi_str(char *buf, int max, vmbgi *b);

INLINE extern void print_escape_str(vmstr *vs);
INLINE extern void fprint_escape_str(vmstr *vs, FILE *fp);
INLINE extern vmstr *str_clear(vmstr *vs);
INLINE extern vmstr *str_dup(vmctx *ctx, vmstr *vs);
INLINE extern vmstr *str_from_i64(vmctx *ctx, int64_t i);
INLINE extern vmstr *str_from_dbl(vmctx *ctx, double *d);
INLINE extern vmstr *str_make_double(vmctx *ctx, vmstr *vs);
INLINE extern vmstr *str_make_ntimes(vmctx *ctx, vmstr *vs, int n);
INLINE extern vmstr *str_append(vmctx *ctx, vmstr *vs, const char *s, int len);
INLINE extern vmstr *str_append_ch(vmctx *ctx, vmstr *vs, const char ch);
INLINE extern vmstr *str_append_cp(vmctx *ctx, vmstr *vs, const char *s);
INLINE extern vmstr *str_append_str(vmctx *ctx, vmstr *vs, vmstr *s2);
INLINE extern vmstr *str_append_fmt(vmctx *ctx, vmstr *vs, const char *fmt, ...);
INLINE extern vmstr *str_append_fmt_dbl(vmctx *ctx, vmstr *vs, const char *fmt, double *d);
INLINE extern vmstr *str_append_i64(vmctx *ctx, vmstr *vs, int64_t i);
INLINE extern vmstr *str_append_dbl(vmctx *ctx, vmstr *vs, double *d);
INLINE extern vmstr *str_make_path(vmctx *ctx, vmstr *v0, vmstr *v1);
INLINE extern vmstr *str_make_path_i64(vmctx *ctx, vmstr *v0, int64_t i);
INLINE extern vmstr *str_make_i64_path(vmctx *ctx, int64_t i, vmstr *v0);
INLINE extern vmstr *str_trim(vmctx *ctx, vmstr *vs, const char *ch);
INLINE extern vmstr *str_ltrim(vmctx *ctx, vmstr *vs, const char *ch);
INLINE extern vmstr *str_rtrim(vmctx *ctx, vmstr *vs, const char *ch);

INLINE extern int bin_set_i(vmbin *vs, int idx, int64_t i);
INLINE extern int bin_set_d(vmbin *vs, int idx, double *d);
INLINE extern int bin_set(vmbin *vs, int idx, vmvar *v);
INLINE extern vmbin *bin_append(vmctx *ctx, vmbin *vs, const uint8_t *s, int len);
INLINE extern vmbin *bin_append_bin(vmctx *ctx, vmbin *vs, vmbin *s2);
INLINE extern vmbin *bin_append_ch(vmctx *ctx, vmbin *vs, const uint8_t ch);
INLINE extern vmbin *bin_clear(vmbin *vs);
INLINE extern vmbin *bin_dup(vmctx *ctx, vmbin *vs);
INLINE extern void print_bin(vmctx *ctx, vmbin *vs);
INLINE extern void fprint_bin(vmctx *ctx, vmbin *vs, FILE *fp);

INLINE extern void hashmap_print(vmobj *obj);
INLINE extern void hashmap_objprint(vmctx *ctx, vmobj *obj);
INLINE extern void hashmap_objfprint(vmctx *ctx, vmobj *obj, FILE *fp);
INLINE extern int hashmap_getint(vmobj *o, const char *optname, int def);
INLINE extern const char *hashmap_getstr(vmobj *o, const char *optname);
INLINE extern vmobj *hashmap_create(vmobj *h, int sz);
INLINE extern vmobj *hashmap_set(vmctx *ctx, vmobj *obj, const char *s, vmvar *v);
INLINE extern vmobj *hashmap_remove(vmctx *ctx, vmobj *obj, const char *s);
INLINE extern vmvar *hashmap_search(vmobj *obj, const char *s);
INLINE extern vmobj *hashmap_append(vmctx *ctx, vmobj *obj, vmobj *src);
INLINE extern vmobj *hashmap_copy(vmctx *ctx, vmobj *h);
INLINE extern vmobj *hashmap_copy_method(vmctx *ctx, vmobj *src);
INLINE extern vmobj *array_create(vmobj *obj, int asz);
INLINE extern vmobj *array_set(vmctx *ctx, vmobj *obj, int64_t idx, vmvar *vs);
INLINE extern vmobj *array_unshift(vmctx *ctx, vmobj *obj, vmvar *vs);
INLINE extern vmvar *array_shift(vmctx *ctx, vmobj *obj);
INLINE extern vmobj *array_shift_array(vmctx *ctx, vmobj *obj, int n);
INLINE extern vmobj *array_push(vmctx *ctx, vmobj *obj, vmvar *vs);
INLINE extern vmvar *array_pop(vmctx *ctx, vmobj *obj);
INLINE extern vmobj *array_pop_array(vmctx *ctx, vmobj *obj, int n);
INLINE extern vmobj *object_copy(vmctx *ctx, vmobj *src);
INLINE extern vmobj *object_get_keys(vmctx *ctx, vmobj *src);

INLINE extern int run_global(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
INLINE extern int throw_system_exception(int line, vmctx *ctx, int id, const char *msg);
INLINE extern int exception_addtrace(vmctx *ctx, vmvar *e, const char *funcname, const char *filename, int linenum);
INLINE extern int exception_printtrace(vmctx *ctx, vmvar *e);
INLINE extern int exception_uncaught(vmctx *ctx, vmvar *e);
INLINE extern int Fiber_yield(vmctx *ctx, int yield);

INLINE extern int bnot_v(vmctx *ctx, vmvar *r, vmvar *v);
INLINE extern int not_v(vmctx *ctx, vmvar *r, vmvar *v);
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

INLINE extern int band_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int band_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int band_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int bor_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int bor_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int bor_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int bxor_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE extern int bxor_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE extern int bxor_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);
INLINE extern int inc_v(vmctx *ctx, vmvar *v);
INLINE extern int dec_v(vmctx *ctx, vmvar *v);
INLINE extern int uminus_v(vmctx *ctx, vmvar *r, vmvar *v);

INLINE extern int iRange_create_i(vmctx *ctx, vmfrm *lex, vmvar *r, int *beg, int *end, int excl);
INLINE extern int Range_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);
INLINE extern int Iterator_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);

#endif /* KILITE_TEMPLATE_HEADER_H */
