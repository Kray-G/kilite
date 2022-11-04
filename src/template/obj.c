#include "common.h"

static vmobj *hashmap_extend(vmctx *ctx, vmobj *obj);

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

static void hashmap_objprint_impl(vmobj *obj, int indent)
{
    int sz = obj->sz;
    vmvar *map = obj->map;
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
                    hashmap_objprint_impl(v->a->o, indent + 1);
                    break;
                }
            }
        }
    }
    hashmap_print_indent(indent);
    printf("}%s\n", indent == 0 ? "" : ",");
}

void hashmap_objprint(vmobj *obj)
{
    hashmap_objprint_impl(obj, 0);
}

void hashmap_print(vmobj *obj)
{
    printf("---------\n");
    int sz = obj->sz;
    vmvar *map = obj->map;
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

vmobj *hashmap_create(vmobj *h, int sz)
{
    h->map = (vmvar *)calloc(sz, sizeof(vmvar));
    h->sz = sz;
    return h;
}

vmobj *hashmap_set(vmctx *ctx, vmobj *obj, const char *s, vmvar *vs)
{
    if (!obj->map) {
        hashmap_create(obj, HASH_SIZE);
    }
    int sz = obj->sz;
    unsigned int h = hashcode(s, sz);
    unsigned int hc = h;
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(obj->map[h]);
        if (IS_HASHITEM_EXIST(v)) {
            if (strcmp(v->s->s, s) == 0) {
                /* if the key string has been already registered, overwrite it. */
                v->i = hc;
                v->a = vs;
                return obj;
            }
        } else if (IS_HASHITEM_EMPTY(v) || IS_HASHITEM_REMVD(v)) {
            /* The value can be registered to the place where is either empty or removed. */
            HASHITEM_EXIST(v);
            v->i = hc;
            v->s = alcstr_str(ctx, s);
            v->a = vs;
            return obj;
        }
        ++h;
        if (h >= sz) {
            h = 0;
        }
    }

    obj = hashmap_extend(ctx, obj);
    return hashmap_set(ctx, obj, s, vs);
}

vmobj *hashmap_remove(vmctx *ctx, vmobj *obj, const char *s)
{
    if (!obj->map) {
        return obj;
    }

    int sz = obj->sz;
    unsigned int h = hashcode(s, sz);
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(obj->map[h]);
        if (IS_HASHITEM_EXIST(v)) {
            if (strcmp(v->s->s, s) == 0) {
                v->i = 0;
                v->s = NULL;
                v->a = NULL;
                HASHITEM_REMVD(v);
                return obj;
            }
        }
        ++h;
        if (h >= sz) {
            h = 0;
        }
    }
    return obj;
}

vmvar *hashmap_search(vmobj *obj, const char *s)
{
    if (!obj->map) {
        return NULL;
    }

    int sz = obj->sz;
    unsigned int h = hashcode(s, sz);
    unsigned int hc = h;
    vmvar *map = obj->map;
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(obj->map[h]);
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

static vmobj *hashmap_extend(vmctx *ctx, vmobj *obj)
{
    if (!obj->map) {
        return hashmap_create(obj, HASH_SIZE);
    }
    int sz = obj->sz;
    vmvar *map = obj->map;

    obj->sz = (obj->sz << 1) + 1;
    obj->map = (vmvar *)calloc(obj->sz, sizeof(vmvar));
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            hashmap_set(ctx, obj, v->s->s, v->a);
        }
    }

    return obj;
}

vmobj *hashmap_copy(vmctx *ctx, vmobj *h)
{
    vmobj *obj = alcobj(ctx);
    hashmap_create(obj, HASH_SIZE);
    int sz = h->sz;
    vmvar *map = h->map;

    obj->sz = sz;
    obj->map = (vmvar *)calloc(sz, sizeof(vmvar));
    for (int i = 0; i < sz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            hashmap_set(ctx, obj, v->s->s, v->a);
        }
    }

    return obj;
}
