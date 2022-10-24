#ifndef KILITE_TEMPLATE_HEADER_H
#define KILITE_TEMPLATE_HEADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "lib/bigz.h"

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
#define NULL ((void*)0)
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

typedef int (*vmfunc_t)(struct vmctx *ctx, struct vmfrm *lex, struct vmvar **r, int ac);
typedef struct vmfnc {
    struct vmfnc *prv;  /* The link to the previous item in alive list. */
    struct vmfnc *liv;  /* The link to the next item in alive list. */
    struct vmfnc *nxt;  /* The link to the next item in free list. */
    struct vmfnc *chn;  /* The link in allocated object list */

    int32_t flags;
    int32_t args;
    vmfunc_t f;         /* function prototype have to be fixed */
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
INLINE vmstr *str_append(vmctx *ctx, vmstr *vs, const char *s, int len);
INLINE vmstr *str_append_cp(vmctx *ctx, vmstr *vs, const char *s);
INLINE vmstr *str_append_str(vmctx *ctx, vmstr *vs, vmstr *s2);
INLINE vmstr *str_ltrim(vmctx *ctx, vmstr *vs, const char *ch);
INLINE vmstr *str_rtrim(vmctx *ctx, vmstr *vs, const char *ch);

INLINE void hashmap_print(vmhsh *hsh);
INLINE vmhsh *hashmap_create(vmhsh *h, int sz);
INLINE vmhsh *hashmap_set(vmctx *ctx, vmhsh *hsh, const char *s, vmvar *v);
INLINE vmhsh *hashmap_remove(vmctx *ctx, vmhsh *hsh, const char *s);
INLINE vmvar *hashmap_search(vmhsh *hsh, const char *s);

INLINE void run_global(vmctx *ctx);

#include "op.h"

#endif /* KILITE_TEMPLATE_HEADER_H */
