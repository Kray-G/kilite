#include "common.h"

static vmhsh *hashmap_extend(vmctx *ctx, vmhsh *hsh);

static inline unsigned int hashcode(const char *s, int sz)
{
	unsigned int h = (int)*s;
	if (h) for (++s ; *s; ++s) {
        h = (h << 5) - h + (int)*s;
    }
    return h % sz;
}

static void hashmap_print_indent(int indent)
{
    for (int i = 0; i < indent; ++i) {
        printf("    ");
    }
}

static void hashmap_objprint_impl(vmhsh *hsh, int indent)
{
    int sz = hsh->sz;
    vmvar *map = hsh->map;
    printf("{\n");
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            if (v->s && v->s->s) {
                hashmap_print_indent(indent + 1);
                printf("\"%s\": ", v->s->s);
                switch (v->a->t) {
                case VAR_INT64:
                    printf("%lld\n", v->a->i);
                    break;
                case VAR_DBL:
                    printf("%f\n", v->a->d);
                    break;
                case VAR_BIG: {
                    char *bs = BzToString(v->a->bi->b, 10, 0);
                    printf("%s\n", bs);
                    BzFreeString(bs);
                    break;
                }
                case VAR_STR:
                    printf("%s\n", v->a->s);
                    break;
                case VAR_FNC:
                    printf("func(%p)\n", v->a->f);
                    break;
                case VAR_OBJ:
                    hashmap_objprint_impl(v->a->h, indent + 1);
                    break;
                }
            }
        }
    }
    hashmap_print_indent(indent);
    printf("}%s\n", indent == 0 ? "" : ",");
}

void hashmap_objprint(vmhsh *hsh)
{
    hashmap_objprint_impl(hsh, 0);
}

void hashmap_print(vmhsh *hsh)
{
    printf("---------\n");
    int sz = hsh->sz;
    vmvar *map = hsh->map;
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(map[i % sz]);
        if (IS_HASHITEM_REMVD(v)) {
            printf("REMOVED ");
        } else if (IS_HASHITEM_EXIST(v)) {
            printf("EXISTS  ");
        } else if (IS_HASHITEM_EMPTY(v)) {
            printf("EMPTY   ");
        }
        if (v->s && v->s->s) {
            printf("[%d], key(%s) => var(%p)", (int)v->i, v->s->s, v->a);
        }
        printf("\n");
    }
}

vmhsh *hashmap_create(vmhsh *h, int sz)
{
    h->map = (vmvar *)calloc(sz, sizeof(vmvar));
    h->sz = sz;
    return h;
}

vmhsh *hashmap_set(vmctx *ctx, vmhsh *hsh, const char *s, vmvar *vs)
{
    if (!hsh->map) {
        hashmap_create(hsh, HASH_SIZE);
    }
    int sz = hsh->sz;
    unsigned int h = hashcode(s, sz);
    unsigned int hc = h;
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(hsh->map[h]);
        if (IS_HASHITEM_EXIST(v)) {
            if (strcmp(v->s->s, s) == 0) {
                /* if the key string has been already registered, overwrite it. */
                v->i = hc;
                v->a = vs;
                return hsh;
            }
        } else if (IS_HASHITEM_EMPTY(v) || IS_HASHITEM_REMVD(v)) {
            /* The value can be registered to the place where is either empty or removed. */
            HASHITEM_EXIST(v);
            v->i = hc;
            v->s = alcstr_str(ctx, s);
            v->a = vs;
            return hsh;
        }
        ++h;
        if (h >= sz) {
            h = 0;
        }
    }

    hsh = hashmap_extend(ctx, hsh);
    return hashmap_set(ctx, hsh, s, vs);
}

vmhsh *hashmap_remove(vmctx *ctx, vmhsh *hsh, const char *s)
{
    int sz = hsh->sz;
    unsigned int h = hashcode(s, sz);
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(hsh->map[h]);
        if (IS_HASHITEM_EXIST(v)) {
            if (strcmp(v->s->s, s) == 0) {
                v->i = 0;
                v->s = NULL;
                v->a = NULL;
                HASHITEM_REMVD(v);
                return hsh;
            }
        }
        ++h;
        if (h >= sz) {
            h = 0;
        }
    }
    return hsh;
}

vmvar *hashmap_search(vmhsh *hsh, const char *s)
{
    int sz = hsh->sz;
    unsigned int h = hashcode(s, sz);
    unsigned int hc = h;
    vmvar *map = hsh->map;
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(hsh->map[h]);
        if (IS_HASHITEM_EMPTY(v)) {
            return NULL;
        }
        if (IS_HASHITEM_EXIST(v)) {
            if (v->i == hc && strcmp(v->s->s, s) == 0) {
                return v->a;
            }
        }
        ++h;
        if (h >= sz) {
            h = 0;
        }
    }
    return NULL;
}

static vmhsh *hashmap_extend(vmctx *ctx, vmhsh *hsh)
{
    if (!hsh->map) {
        return hashmap_create(hsh, HASH_SIZE);
    }
    int sz = hsh->sz;
    vmvar *map = hsh->map;

    hsh->sz = (hsh->sz << 1) + 1;
    hsh->map = (vmvar *)calloc(hsh->sz, sizeof(vmvar));
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            hashmap_set(ctx, hsh, v->s->s, v->a);
        }
    }

    return hsh;
}

vmhsh *hashmap_copy(vmctx *ctx, vmhsh *h)
{
    vmhsh *hsh = alchsh(ctx);
    hashmap_create(hsh, HASH_SIZE);
    int sz = h->sz;
    vmvar *map = h->map;

    hsh->sz = sz;
    hsh->map = (vmvar *)calloc(sz, sizeof(vmvar));
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            hashmap_set(ctx, hsh, v->s->s, v->a);
        }
    }

    return hsh;
}
