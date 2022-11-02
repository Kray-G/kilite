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
#else
#define INLINE inline
int printf(const char *, ...);
#ifdef WIN32
int _snprintf(char *, int, const char *, ...);
#define snprintf _snprintf
#else
int snprintf(char *, int, const char *, ...);
#endif
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

#define FRM_STACK_SIZE (1024)
#define VAR_STACK_SIZE (1024*16)
#define ALC_UNIT (1024)
#define ALC_UNIT_FRM (1024)
#define TICK_UNIT (1024*64)
#define STR_UNIT (32)
#define HASH_SIZE (23)
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

#define push_frm(ctx, m) do { if ((ctx)->fstksz <= (ctx)->fstkp) { /* TODO: stack overflow */ } (((ctx)->fstk)[((ctx)->fstkp)++] = (m)); } while (0)
#define pop_frm(ctx) (--((ctx)->fstkp))

#define alloc_var(ctx, n) do { if ((ctx)->vstksz <= ((ctx)->vstkp + n)) { /* TODO: stack overflow */ } (((ctx)->vstkp) += (n)); } while (0)
#define vstackp(ctx) ((ctx)->vstkp)
#define push_var(ctx, v) do { if ((ctx)->vstksz <= (ctx)->vstkp) { /* TODO: stack overflow */ } (((ctx)->vstk)[((ctx)->vstkp)++].a = (v)); } while (0)
#define pop_var(ctx) (--((ctx)->vstkp))
#define top_var(ctx) (((ctx)->vstk)[((ctx)->vstkp) - 1].a)
#define arg_var(ctx, n) (((ctx)->vstk)[(ctx)->vstkp - ((n) + 1)].a)
#define reduce_vstackp(ctx, n) (((ctx)->vstkp) -= (n))
#define restore_vstackp(ctx, p) (((ctx)->vstkp) = (p))
#define init_var(v) ((v)->t = VAR_INT64, (v)->i = 0)
#define local_var(ctx, n) (&(((ctx)->vstk)[(ctx)->vstkp - ((n) + 1)]))
#define local_var_index(n) ((ctx)->vstkp - ((n) + 1))

/***************************************************************************
 * VM Variable
*/

#define IS_VMINT(x) ((x) <= VAR_BIG)
typedef enum vartype {
    VAR_INT64 = 0x00,
    VAR_BIG,
    VAR_DBL,
    VAR_STR,
    VAR_FNC,
    VAR_OBJ,
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

typedef struct vmhsh {
    struct vmhsh *prv;  /* The link to the previous item in alive list. */
    struct vmhsh *liv;  /* The link to the next item in alive list. */
    struct vmhsh *nxt;  /* The link to the next item in free list. */
    struct vmhsh *chn;  /* The link in allocated object list */

    int32_t flags;
    int32_t sz;
    struct vmvar *map;
} vmhsh;

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
    vmhsh *h;           /* The hashmap from string to object */
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

    vmvar *except;  /* Current exception that was thrown. */

    struct {
        vmvar var;
        vmfnc fnc;
        vmfrm frm;
        vmstr str;
        vmbgi bgi;
        vmhsh hsh;
    } alc;
    struct {
        int var;
        int fnc;
        int frm;
        int str;
        int bgi;
        int hsh;
    } cnt;
    struct {
        int var;
        int fnc;
        int frm;
        int str;
        int bgi;
        int hsh;
    } fre;
} vmctx;

INLINE vmctx *initialize(void);
INLINE void finalize(vmctx *ctx);
INLINE void setup_context(vmctx *ctx);

INLINE vmfnc *alcfnc(vmctx *ctx, void *f, vmfrm *lex, int args);
INLINE void pbakfnc(vmctx *ctx, vmfnc *p);
INLINE vmfrm *alcfrm(vmctx *ctx, int args);
INLINE void pbakfrm(vmctx *ctx, vmfrm *p);
INLINE vmstr *alcstr_str(vmctx *ctx, const char *s);
INLINE void pbakstr(vmctx *ctx, vmstr *p);
INLINE vmbgi *alcbgi_bigz(vmctx *ctx, BigZ bz);
INLINE void pbakbgi(vmctx *ctx, vmbgi *p);
INLINE vmhsh *alchsh(vmctx *ctx);
INLINE void pbakhsh(vmctx *ctx, vmhsh *p);
INLINE vmvar *alcvar(vmctx *ctx, vartype t, int hold);
INLINE vmvar *alcvar_initial(vmctx *ctx);
INLINE vmvar *alcvar_fnc(vmctx *ctx, vmfnc *f);
INLINE vmvar *alcvar_int64(vmctx *ctx, int64_t i, int hold);
INLINE vmvar *alcvar_str(vmctx *ctx, const char *s);
INLINE vmvar *alcvar_bgistr(vmctx *ctx, const char *s, int radix);
INLINE void pbakvar(vmctx *ctx, vmvar *p);
INLINE vmvar *copy_var(vmctx *ctx, vmvar *src, int hold);
INLINE void copy_var_to(vmctx *ctx, vmvar *dst, vmvar *src);

INLINE void initialize_allocators(vmctx *ctx);
INLINE void mark_and_sweep(vmctx *ctx);
INLINE void count(vmctx *ctx);

INLINE void bi_initialize(void);
INLINE void bi_finalize(void);
INLINE vmbgi *bi_copy(vmctx *ctx, vmbgi *src);
INLINE void bi_print(vmbgi *b);
INLINE void bi_str(char *buf, int max, vmbgi *b);

INLINE vmstr *str_dup(vmctx *ctx, vmstr *vs);
INLINE vmstr *str_from_i64(vmctx *ctx, int64_t i);
INLINE vmstr *str_from_dbl(vmctx *ctx, double d);
INLINE vmstr *str_make_double(vmctx *ctx, vmstr *vs);
INLINE vmstr *str_make_ntimes(vmctx *ctx, vmstr *vs, int n);
INLINE vmstr *str_append(vmctx *ctx, vmstr *vs, const char *s, int len);
INLINE vmstr *str_append_cp(vmctx *ctx, vmstr *vs, const char *s);
INLINE vmstr *str_append_str(vmctx *ctx, vmstr *vs, vmstr *s2);
INLINE vmstr *str_append_i64(vmctx *ctx, vmstr *vs, int64_t i);
INLINE vmstr *str_append_dbl(vmctx *ctx, vmstr *vs, double d);
INLINE vmstr *str_make_path(vmctx *ctx, vmstr *v0, vmstr *v1);
INLINE vmstr *str_make_path_i64(vmctx *ctx, vmstr *v0, int64_t i);
INLINE vmstr *str_make_i64_path(vmctx *ctx, int64_t i, vmstr *v0);
INLINE vmstr *str_trim(vmctx *ctx, vmstr *vs, const char *ch);
INLINE vmstr *str_ltrim(vmctx *ctx, vmstr *vs, const char *ch);
INLINE vmstr *str_rtrim(vmctx *ctx, vmstr *vs, const char *ch);

INLINE void hashmap_print(vmhsh *hsh);
INLINE void hashmap_objprint(vmhsh *hsh);
INLINE vmhsh *hashmap_create(vmhsh *h, int sz);
INLINE vmhsh *hashmap_set(vmctx *ctx, vmhsh *hsh, const char *s, vmvar *v);
INLINE vmhsh *hashmap_remove(vmctx *ctx, vmhsh *hsh, const char *s);
INLINE vmvar *hashmap_search(vmhsh *hsh, const char *s);
INLINE vmhsh *hashmap_copy(vmctx *ctx, vmhsh *h);

INLINE int run_global(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);

INLINE int throw_system_exception(vmctx *ctx, int id);
INLINE int throw_exception(vmctx *ctx, vmvar *e);

INLINE int add_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE int add_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE int add_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);

INLINE int sub_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE int sub_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE int sub_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);

INLINE int mul_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE int mul_i_v(vmctx *ctx, vmvar *, int64_t ir, vmvar *v);
INLINE int mul_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);

INLINE int div_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE int div_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE int div_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);

INLINE int mod_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE int mod_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE int mod_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);

INLINE int eqeq_v_i(vmctx *ctx, vmvar *r, vmvar *v, int64_t i);
INLINE int eqeq_i_v(vmctx *ctx, vmvar *r, int64_t i, vmvar *v);
INLINE int eqeq_v_v(vmctx *ctx, vmvar *r, vmvar *v0, vmvar *v1);


/* System Exceptions */
enum {
    EXCEPT_DIVIDE_BY_ZERO = 1,
    EXCEPT_UNSUPPORTED_OPERATION = 1,
};

/* Operator macros */

/* Copy Variable */

#define SET_I64(dst, v) { (dst)->t = VAR_INT64; (dst)->i  = (v);                  }
#define SET_DBL(dst, v) { (dst)->t = VAR_DBL;   (dst)->d  = (v);                  }
#define SET_BIG(dst, v) { (dst)->t = VAR_BIG;   (dst)->bi = (v);                  }
#define SET_STR(dst, v) { (dst)->t = VAR_STR;   (dst)->s  = alcstr_str(ctx, (v)); }
#define SET_FNC(dst, v) { (dst)->t = VAR_FNC;   (dst)->f  = (v);                  }
#define SET_OBJ(dst, v) { (dst)->t = VAR_OBJ;   (dst)->a  = (v);                  }

#define COPY_VAR_TO(ctx, dst, src) { \
    switch ((src)->t) { \
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
        (dst)->h = (src)->h; \
        break; \
    default: \
        /* Error */ \
        (dst)->t = VAR_INT64; \
        (dst)->i = 0; \
        break; \
    } \
} \
/**/

#define SET_ARGVAR(idx, alc) { \
    if (ac > idx) { \
        vmvar *a##idx = arg_var(ctx, alc); \
        COPY_VAR_TO(ctx, n##idx, a##idx); \
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

/* Hashmap control */

#define OP_APPLY(ctx, r, v, str) { \
    (r)->a = NULL; \
    vmvar *t1 = ((v)->a) ? (v)->a : (v); \
    if ((t1)->t == VAR_OBJ) { \
        if (!((t1)->h)) { \
            (r)->t = VAR_INT64; \
            (r)->i = 0; \
        } else { \
            vmvar *t2 = hashmap_search((t1)->h, str); \
            COPY_VAR_TO(ctx, (r), t2); \
        } \
    } else { \
        (r)->t = VAR_INT64; \
        (r)->i = 0; \
    } \
} \
/**/

#define OP_APPLYL(ctx, r, v, str) { \
    vmvar *t1 = ((v)->a) ? (v)->a : (v); \
    if ((t1)->t != VAR_OBJ) { \
        (t1)->t = VAR_OBJ; \
    } \
    vmvar *t2 = NULL; \
    if (!(t1)->h) { \
        (t1)->h = alchsh(ctx); \
        hashmap_create((t1)->h, HASH_SIZE); \
    } else { \
        t2 = hashmap_search((t1)->h, str); \
    } \
    if (!t2) { \
        t2 = alcvar_int64(ctx, 0, 0); \
        (t1)->h = hashmap_set(ctx, (t1)->h, str, t2); \
    } \
    r->t = VAR_OBJ; \
    r->a = t2; \
} \
/**/

/* Increment/Decrement */

#define OP_INC(ctx, r, v) { \
    if ((v)->t == VAR_INT64) { \
        (r)->i = ++((v)->i); \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_INCP(ctx, r, v) { \
    if ((v)->t == VAR_INT64) { \
        (r)->i = ((v)->i)++; \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_DEC(ctx, r, v) { \
    if ((v)->t == VAR_INT64) { \
        (r)->i = --((v)->i); \
    } else { \
        /* TODO */ \
    } \
} \
/**/

#define OP_DECP(ctx, r, v) { \
    if ((v)->t == VAR_INT64) { \
        (r)->i = ((v)->i)--; \
    } else { \
        /* TODO */ \
    } \
} \
/**/

/* ADD */

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
        e = throw_system_exception(ctx, EXCEPT_DIVIDE_BY_ZERO); \
    } \
    (r)->t = VAR_DBL; \
    (r)->d = ((double)(i0)) / (i1); \
} \
/**/

#define OP_DIV_B_I(ctx, r, v0, i1) { \
    if (i1 < DBL_EPSILON) { \
        e = throw_system_exception(ctx, EXCEPT_DIVIDE_BY_ZERO); \
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

#define OP_MOD_I_I(ctx, r, i0, i1) { \
    if (i1 < DBL_EPSILON) { \
        e = throw_system_exception(ctx, EXCEPT_DIVIDE_BY_ZERO); \
    } \
    (r)->t = VAR_INT64; \
    (r)->i = (i0) % (i1); \
} \
/**/

#define OP_MOD_B_I(ctx, r, v0, i1) { \
    if (i1 < DBL_EPSILON) { \
        e = throw_system_exception(ctx, EXCEPT_DIVIDE_BY_ZERO); \
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
            (r)->t = VAR_DBL; \
            (r)->d = ((double)(BzToDouble((v0)->bi->b))) / (BzToDouble((v1)->bi->b)); \
        } else { \
            e = mod_v_v(ctx, r, v0, v1); \
        } \
    } else { \
        e = mod_v_v(ctx, r, v0, v1); \
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
        /* TODO */ \
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
        /* TODO */ \
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
            /* TODO */ \
        } \
    } else { \
            /* TODO */ \
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
        /* TODO */ \
    } \
} \
/**/

#define OP_LT_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        OP_LT_I_I(ctx, r, i0, (v1)->i) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_LT_I_B(ctx, r, i0, v1) \
    } else { \
        /* TODO */ \
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
            /* TODO */ \
        } \
    } else { \
            /* TODO */ \
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
        /* TODO */ \
    } \
} \
/**/

#define OP_LE_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        OP_LE_I_I(ctx, r, i0, (v1)->i) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_LE_I_B(ctx, r, i0, v1) \
    } else { \
        /* TODO */ \
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
            /* TODO */ \
        } \
    } else { \
            /* TODO */ \
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
        /* TODO */ \
    } \
} \
/**/

#define OP_LGE_I_V(ctx, r, i0, v1) { \
    if ((v1)->t == VAR_INT64) { \
        OP_LGE_I_I(ctx, r, i0, (v1)->i) \
    } else if ((v1)->t == VAR_BIG) { \
        OP_LGE_I_B(ctx, r, i0, v1) \
    } else { \
        /* TODO */ \
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
            /* TODO */ \
        } \
    } else { \
            /* TODO */ \
    } \
} \
/**/

#endif /* KILITE_TEMPLATE_HEADER_H */
