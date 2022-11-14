#include "common.h"

static vmobj *hashmap_extend(vmctx *ctx, vmobj *obj);

static inline unsigned int hashcode(const char *s, int hsz)
{
	unsigned int h = (int)*s;
	if (h) for (++s ; *s; ++s) {
        h = (h << 5) - h + (int)*s;
    }
    return h % hsz;
}

static void hashmap_print_indent(int indent)
{
    for (int i = 0; i < indent; ++i) {
        printf("    ");
    }
}

static void hashmap_objprint_impl(vmobj *obj, int indent)
{
    int idt = indent >= 0;
    int lsz = obj->idxsz - 1;
    if (lsz >= 0) {
        printf("[");
        for (int i = 0; i <= lsz; ++i) {
            vmvar *v = obj->ary[i];
            if (!v) {
                if (i < lsz) printf("undefined, ");
                else         printf("undefined");
            } else {
                switch (v->t) {
                case VAR_UNDEF:
                    printf("undefined");
                    if (i < lsz) printf(", ");
                    break;
                case VAR_INT64:
                    printf("%lld", v->i);
                    if (i < lsz) printf(", ");
                    break;
                case VAR_DBL:
                    printf("%f", v->d);
                    if (i < lsz) printf(", ");
                    break;
                case VAR_BIG: {
                    char *bs = BzToString(v->bi->b, 10, 0);
                    printf("%s", bs);
                    BzFreeString(bs);
                    if (i < lsz) printf(", ");
                    break;
                }
                case VAR_STR:
                    printf("\"");
                    print_escape_str(v->s);
                    printf("\"");
                    if (i < lsz) printf(", ");
                    break;
                case VAR_FNC:
                    printf("func(%p)", v->f);
                    if (i < lsz) printf(", ");
                    break;
                case VAR_OBJ:
                    if (idt) {
                        printf("\n");
                        hashmap_print_indent(indent + 1);
                    }
                    hashmap_objprint_impl(v->o, idt ? indent + 1 : -1);
                    if (i < lsz) {
                        printf(", ");
                    } else if (idt) {
                        printf("\n");
                        hashmap_print_indent(indent);
                    }
                    break;
                }
            }
        }
        printf("]");
    }
    int hsz = obj->hsz;
    if (hsz <= 0) {
        if (lsz < 0) {
            printf("{}");
        }
    } else {
        if (idt && lsz > 0) printf("\n");
        int count = 0;
        vmvar *map = obj->map;
        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(map[i]);
            if (IS_HASHITEM_EXIST(v)) {
                count++;
            }
        }
        if (idt) printf("{\n");
        else     printf("{ ");
        int c = 0;
        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(map[i]);
            if (IS_HASHITEM_EXIST(v)) {
                ++c;
                if (v->s && v->s->s) {
                    hashmap_print_indent(idt ? indent + 1 : -1);
                    printf("\"%s\": ", v->s->s);
                    vmvar *va = v->a;
                    switch (va->t) {
                    case VAR_INT64:
                        printf("%lld", va->i);
                        break;
                    case VAR_DBL:
                        printf("%f", va->d);
                        break;
                    case VAR_BIG: {
                        char *bs = BzToString(va->bi->b, 10, 0);
                        printf("%s", bs);
                        BzFreeString(bs);
                        break;
                    }
                    case VAR_STR:
                        printf("\"");
                        print_escape_str(va->s);
                        printf("\"");
                        break;
                    case VAR_FNC:
                        printf("func(%p)", va->f);
                        break;
                    case VAR_OBJ:
                        hashmap_objprint_impl(va->o, idt ? indent + 1 : -1);
                        break;
                    }
                    if (idt) {
                        if (c < count) printf(",\n");
                        else           printf("\n");
                    } else {
                        if (c < count) printf(", ");
                        else           printf(" ");
                    }
                }
            }
        }
        if (idt) {
            hashmap_print_indent(indent);
        }
        printf("}");
    }
}

void hashmap_objprint(vmctx *ctx, vmobj *obj)
{
    if (obj->is_formatter) {
        vmstr *s = format(ctx, obj);
        if (s) {
            printf("%s", s->s);
        }
    } else {
        hashmap_objprint_impl(obj, -1);
    }
}

void hashmap_print(vmobj *obj)
{
    printf("---------\n");
    int hsz = obj->hsz;
    vmvar *map = obj->map;
    for (int i = 0; i < hsz; ++i) {
        vmvar *v = &(map[i % hsz]);
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

vmobj *hashmap_create(vmobj *obj, int hsz)
{
    obj->map = (vmvar *)calloc(hsz, sizeof(vmvar));
    obj->hsz = hsz;
    return obj;
}

vmobj *hashmap_set(vmctx *ctx, vmobj *obj, const char *s, vmvar *vs)
{
    if (!obj->map) {
        hashmap_create(obj, HASH_SIZE);
    }
    int hsz = obj->hsz;
    unsigned int h = hashcode(s, hsz);
    unsigned int hc = h;
    for (int i = 0; i < hsz; ++i) {
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
        if (h >= hsz) {
            h = 0;
        }
    }

    obj = hashmap_extend(ctx, obj);
    return hashmap_set(ctx, obj, s, vs);
}

vmobj *hashmap_remove(vmctx *ctx, vmobj *obj, const char *s)
{
    if (!obj || !obj->map) {
        return obj;
    }

    int hsz = obj->hsz;
    unsigned int h = hashcode(s, hsz);
    for (int i = 0; i < hsz; ++i) {
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
        if (h >= hsz) {
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

    int hsz = obj->hsz;
    unsigned int h = hashcode(s, hsz);
    unsigned int hc = h;
    vmvar *map = obj->map;
    for (int i = 0; i < hsz; ++i) {
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
        if (h >= hsz) {
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
    int hsz = obj->hsz;
    vmvar *map = obj->map;

    obj->hsz = (obj->hsz << 1) + 1;
    obj->map = (vmvar *)calloc(obj->hsz, sizeof(vmvar));
    for (int i = 0; i < hsz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            hashmap_set(ctx, obj, v->s->s, v->a);
        }
    }

    return obj;
}

vmobj *hashmap_copy(vmctx *ctx, vmobj *src)
{
    vmobj *obj = alcobj(ctx);
    hashmap_create(obj, HASH_SIZE);
    int hsz = src->hsz;
    vmvar *map = src->map;

    obj->hsz = hsz;
    obj->map = (vmvar *)calloc(hsz, sizeof(vmvar));
    for (int i = 0; i < hsz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            hashmap_set(ctx, obj, v->s->s, v->a);
        }
    }

    return obj;
}

vmobj *hashmap_copy_method(vmctx *ctx, vmobj *src)
{
    vmobj *obj = alcobj(ctx);
    hashmap_create(obj, HASH_SIZE);
    int hsz = src->hsz;
    vmvar *map = src->map;

    obj->hsz = hsz;
    obj->map = (vmvar *)calloc(hsz, sizeof(vmvar));
    for (int i = 0; i < hsz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            if (v->s && v->s->s) {
                vmvar *va = v->a;
                if (va->t == VAR_FNC) {
                    vmvar *nv = alcvar_fnc(ctx, v->a->f);
                    hashmap_set(ctx, obj, v->s->s, nv);
                }
            }
        }
    }

    return obj;
}

vmobj *array_create(vmobj *obj, int asz)
{
    if (asz < ARRAY_UNIT) asz = ARRAY_UNIT;
    obj->ary = (vmvar **)calloc(asz, sizeof(vmvar*));
    obj->asz = asz;
    obj->idxsz = 0;
    return obj;
}

static vmobj *array_extend(vmctx *ctx, vmobj *obj, int64_t idx)
{
    int newasz = ARRAY_UNIT * ((idx / ARRAY_UNIT) + 1) * 2;
    if (!obj->ary) {
        return array_create(obj, newasz);
    }
    int asz = obj->asz;
    vmvar **ary = obj->ary;
    obj->ary = (vmvar **)calloc(newasz, sizeof(vmvar*));
    for (int i = 0; i < asz; ++i) {
        obj->ary[i] = ary[i];
    }
    obj->asz = newasz;

    return obj;
}

vmobj *array_set(vmctx *ctx, vmobj *obj, int64_t idx, vmvar *vs)
{
    int asz = obj->asz;
    if (asz <= idx) {
        array_extend(ctx, obj, idx + 1);
        obj->idxsz = idx + 1;
    } else if (obj->idxsz <= idx) {
        obj->idxsz = idx + 1;
    }

    obj->ary[idx] = vs;
    return obj;
}

vmobj *array_push(vmctx *ctx, vmobj *obj, vmvar *vs)
{
    int64_t idx = obj->idxsz;
    int asz = obj->asz;
    if (asz <= idx) {
        array_extend(ctx, obj, idx + 1);
        obj->idxsz = idx + 1;
    } else if (obj->idxsz <= idx) {
        obj->idxsz = idx + 1;
    }

    obj->ary[idx] = vs;
    return obj;
}

vmobj *object_copy(vmctx *ctx, vmobj *src)
{
    vmobj *obj = hashmap_copy(ctx, src);
    int idxsz = src->idxsz;
    int asz = obj->asz;
    if (asz < idxsz) {
        array_extend(ctx, obj, idxsz);
    }
    for (int i = 0; i < idxsz; ++i) {
        obj->ary[i] = src->ary[i];
    }
    return obj;
}
