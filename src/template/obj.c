#ifndef KILITE_AMALGAMATION
#include "common.h"
#endif

static vmobj *hashmap_extend(vmctx *ctx, vmobj *obj);

static inline unsigned int hashcode(const char *s, int hsz)
{
	unsigned int h = (int)*s;
	if (h) for (++s ; *s; ++s) {
        h = (h << 5) - h + (int)*s;
    }
    return h % hsz;
}

static void hashmap_fprint_indent(int indent, FILE *fp)
{
    for (int i = 0; i < indent; ++i) {
        fprintf(fp, "    ");
    }
}

static void hashmap_objfprint_impl(vmobj *obj, int indent, FILE *fp)
{
    if (obj->is_checked) {
        fprintf(fp, "(...cycle...)");
        return;
    }
    obj->is_checked = 1;

    int idt = indent >= 0;
    int lsz = obj->idxsz - 1;
    if (lsz >= 0) {
        fprintf(fp, "[");
        for (int i = 0; i <= lsz; ++i) {
            vmvar *v = obj->ary[i];
            if (!v) {
                if (i < lsz) fprintf(fp, "null, ");
                else         fprintf(fp, "null");
            } else {
                switch (v->t) {
                case VAR_UNDEF:
                    fprintf(fp, "null");
                    if (i < lsz) fprintf(fp, ", ");
                    break;
                case VAR_BOOL:
                    fprintf(fp, "%s", v->i ? "true" : "false");
                    if (i < lsz) fprintf(fp, ", ");
                    break;
                case VAR_INT64:
                    fprintf(fp, "%" PRId64, v->i);
                    if (i < lsz) fprintf(fp, ", ");
                    break;
                case VAR_DBL:
                    fprintf(fp, "%.16g", v->d);
                    if (i < lsz) fprintf(fp, ", ");
                    break;
                case VAR_BIG: {
                    char *bs = BzToString(v->bi->b, 10, 0);
                    fprintf(fp, "%s", bs);
                    BzFreeString(bs);
                    if (i < lsz) fprintf(fp, ", ");
                    break;
                }
                case VAR_STR:
                    fprintf(fp, "\"");
                    fprint_escape_str(v->s, fp);
                    fprintf(fp, "\"");
                    if (i < lsz) fprintf(fp, ", ");
                    break;
                case VAR_FNC:
                    fprintf(fp, "func(%p)", v->f);
                    if (i < lsz) fprintf(fp, ", ");
                    break;
                case VAR_OBJ:
                    if (idt) {
                        fprintf(fp, "\n");
                        hashmap_fprint_indent(indent + 1, fp);
                    }
                    hashmap_objfprint_impl(v->o, idt ? indent + 1 : -1, fp);
                    if (i < lsz) {
                        fprintf(fp, ", ");
                    } else if (idt) {
                        fprintf(fp, "\n");
                        hashmap_fprint_indent(indent, fp);
                    }
                    break;
                }
            }
        }
        fprintf(fp, "]");
    }
    int hsz = obj->hsz;
    if (hsz <= 0) {
        if (lsz < 0) {
            fprintf(fp, "{}");
        }
    } else {
        if (idt && lsz > 0) fprintf(fp, "\n");
        int count = 0;
        vmvar *map = obj->map;
        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(map[i]);
            if (IS_HASHITEM_EXIST(v)) {
                vmvar *va = v->a;
                // Function information seems not to be needed for users, so now it was made hidden.
                if (va && va->t != VAR_FNC) {
                    count++;
                }
            }
        }
        if (idt) fprintf(fp, "{\n");
        else     fprintf(fp, "{ ");
        int c = 0;
        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(map[i]);
            if (IS_HASHITEM_EXIST(v)) {
                if (v->k) {
                    vmvar *va = v->a;
                    if (!va) {
                        hashmap_fprint_indent(idt ? indent + 1 : -1, fp);
                        fprintf(fp, "\"%s\": null", v->k);
                    } else if (va->t != VAR_FNC) {
                        ++c;
                        hashmap_fprint_indent(idt ? indent + 1 : -1, fp);
                        fprintf(fp, "\"%s\": ", v->k);
                        switch (va->t) {
                        case VAR_UNDEF:
                            fprintf(fp, "null");
                            break;
                        case VAR_BOOL:
                            fprintf(fp, "%s", va->i ? "true" : "false");
                            break;
                        case VAR_INT64:
                            fprintf(fp, "%" PRId64, va->i);
                            break;
                        case VAR_DBL:
                            fprintf(fp, "%.16g", va->d);
                            break;
                        case VAR_BIG: {
                            char *bs = BzToString(va->bi->b, 10, 0);
                            fprintf(fp, "%s", bs);
                            BzFreeString(bs);
                            break;
                        }
                        case VAR_STR:
                            fprintf(fp, "\"");
                            fprint_escape_str(va->s, fp);
                            fprintf(fp, "\"");
                            break;
                        case VAR_OBJ:
                            hashmap_objfprint_impl(va->o, idt ? indent + 1 : -1, fp);
                            break;
                        }
                    }
                    if (idt) {
                        if (c < count) fprintf(fp, ",\n");
                        else           fprintf(fp, "\n");
                    } else {
                        if (c < count) fprintf(fp, ", ");
                        else           fprintf(fp, " ");
                    }
                }
            }
        }
        if (idt) {
            hashmap_fprint_indent(indent, fp);
        }
        fprintf(fp, "}");
    }

    obj->is_checked = 0;
}

void hashmap_objfprint(vmctx *ctx, vmobj *obj, FILE *fp)
{
    if (obj->is_formatter) {
        vmstr *s = format(ctx, obj);
        if (s) {
            fprintf(fp, "%s", s->s);
        }
    } else {
        vmvar *f = hashmap_search(obj, "_False");
        if (f) {
            fprintf(fp, "%s", (f->t == VAR_INT64 && f->i == 0) ? "true" : "false");
        } else {
            hashmap_objfprint_impl(obj, -1, fp);
        }
    }
}

void hashmap_objprint(vmctx *ctx, vmobj *obj)
{
    hashmap_objfprint(ctx, obj, stdout);
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
        if (v->k) {
            printf("[%d], key(%s) => var(%p)", (int)v->i, v->k, v->a);
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

int hashmap_getint(vmobj *o, const char *optname, int def)
{
    vmvar *v = hashmap_search(o, optname);
    if (!v || v->t != VAR_INT64) return def;
    return (int)v->i;
}

const char *hashmap_getstr(vmobj *o, const char *optname)
{
    vmvar *v = hashmap_search(o, optname);
    if (!v || v->t != VAR_STR) return NULL;
    return v->s->hd;
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
            if (strcmp(v->k, s) == 0) {
                /* if the key string has been already registered, overwrite it. */
                v->i = hc;
                v->a = vs;
                return obj;
            }
        } else if (IS_HASHITEM_EMPTY(v) || IS_HASHITEM_REMVD(v)) {
            /* The value can be registered to the place where is either empty or removed. */
            HASHITEM_EXIST(v);
            v->i = hc;
            v->k = vmconst_str(ctx, s);
            v->a = vs;
            if (strcmp(s, "_False") == 0) {
                obj->is_false = 1;
            }
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
    if (hsz > 0) {
        unsigned int h = hashcode(s, hsz);
        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(obj->map[h]);
            if (IS_HASHITEM_EXIST(v)) {
                if (strcmp(v->k, s) == 0) {
                    v->i = 0;
                    v->k = NULL;
                    v->a = NULL;
                    HASHITEM_REMVD(v);
                    if (strcmp(s, "_False") == 0) {
                        obj->is_false = 0;
                    }
                    return obj;
                }
            }
            ++h;
            if (h >= hsz) {
                h = 0;
            }
        }
    }
    return obj;
}

vmvar *hashmap_search(vmobj *obj, const char *s)
{
    if (!obj || !obj->map) {
        return NULL;
    }

    int hsz = obj->hsz;
    if (hsz > 0) {
        unsigned int h = hashcode(s, hsz);
        unsigned int hc = h;
        vmvar *map = obj->map;
        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(obj->map[h]);
            if (IS_HASHITEM_EMPTY(v)) {
                return NULL;
            }
            if (IS_HASHITEM_EXIST(v)) {
                if (v->i == hc && strcmp(v->k, s) == 0) {
                    return v->a;
                }
            }
            ++h;
            if (h >= hsz) {
                h = 0;
            }
        }
    }
    return NULL;
}

static vmobj *hashmap_extend_n(vmctx *ctx, vmobj *obj, int n)
{
    if (!obj->map) {
        return hashmap_create(obj, n);
    }
    int hsz = obj->hsz;
    vmvar *map = obj->map;

    obj->hsz = n;
    obj->map = (vmvar *)calloc(obj->hsz, sizeof(vmvar));
    for (int i = 0; i < hsz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            hashmap_set(ctx, obj, v->k, v->a);
        }
    }
    free(map);

    return obj;
}

static vmobj *hashmap_extend(vmctx *ctx, vmobj *obj)
{
    if (!obj->map) {
        return hashmap_create(obj, HASH_SIZE);
    }

    return hashmap_extend_n(ctx, obj, (obj->hsz << 1) + 1);
}

vmobj *hashmap_copy(vmctx *ctx, vmobj *src)
{
    vmobj *obj = alcobj(ctx);
    int hsz = src->hsz;
    if (hsz > 0) {
        hashmap_create(obj, hsz);
        vmvar *map = src->map;
        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(map[i]);
            if (IS_HASHITEM_EXIST(v)) {
                hashmap_set(ctx, obj, v->k, v->a);
            }
        }
    }

    return obj;
}

vmobj *hashmap_append(vmctx *ctx, vmobj *obj, vmobj *src)
{
    int hsz = src->hsz;
    if (hsz > 0) {
        int count = 0;
        vmvar *map = src->map;
        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(map[i]);
            if (IS_HASHITEM_EXIST(v)) {
                count++;
            }
        }
        int cur = obj->hsz;
        for (int i = 0; i < cur; ++i) {
            vmvar *v = &(obj->map[i]);
            if (IS_HASHITEM_EXIST(v)) {
                count++;
            }
        }
        if (cur < count) {
            hashmap_extend_n(ctx, obj, (count << 1) + 1);
        }

        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(map[i]);
            if (IS_HASHITEM_EXIST(v)) {
                hashmap_set(ctx, obj, v->k, v->a);
            }
        }
    }

    return obj;
}

vmobj *hashmap_copy_method(vmctx *ctx, vmobj *src)
{
    vmobj *obj = alcobj(ctx);
    int hsz = src->hsz;
    if (hsz > 0) {
        hashmap_create(obj, hsz);
        vmvar *map = src->map;
        for (int i = 0; i < hsz; ++i) {
            vmvar *v = &(map[i]);
            if (IS_HASHITEM_EXIST(v)) {
                if (v->k) {
                    vmvar *va = v->a;
                    if (va->t == VAR_FNC) {
                        vmvar *nv = alcvar_fnc(ctx, v->a->f);
                        hashmap_set(ctx, obj, v->k, nv);
                    }
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

vmobj *array_unshift(vmctx *ctx, vmobj *obj, vmvar *vs)
{
    int64_t idx = obj->idxsz;
    int asz = obj->asz;
    if (asz <= idx) {
        array_extend(ctx, obj, idx + 1);
        obj->idxsz = idx + 1;
    } else if (obj->idxsz <= idx) {
        obj->idxsz = idx + 1;
    }
    for (int i = idx; i > 0; --i) {
        obj->ary[i] = obj->ary[i - 1];
    }

    obj->ary[0] = vs;
    return obj;
}

vmvar *array_shift(vmctx *ctx, vmobj *obj)
{
    int64_t idx = obj->idxsz;
    vmvar *r = NULL;
    if (idx > 0) {
        r = obj->ary[0];
        for (int i = 1; i < idx; ++i) {
            obj->ary[i - 1] = obj->ary[i];
        }
        obj->idxsz--;
    }
    return r;
}

vmobj *array_shift_array(vmctx *ctx, vmobj *obj, int n)
{
    int64_t idx = obj->idxsz;
    vmobj *o = alcobj(ctx);
    if (idx > 0) {
        int nx = n < idx ? n : idx;
        for (int i = nx - 1; i >= 0; --i) {
            array_push(ctx, o, obj->ary[i]);
        }
        for (int i = nx; i < idx; ++i) {
            obj->ary[i - nx] = obj->ary[i];
        }
        obj->idxsz -= nx;
    }
    return o;
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

vmvar *array_pop(vmctx *ctx, vmobj *obj)
{
    int64_t idx = obj->idxsz;
    vmvar *r = NULL;
    if (idx > 0) {
        r = obj->ary[idx - 1];
        obj->idxsz--;
    }
    return r;
}

vmobj *array_pop_array(vmctx *ctx, vmobj *obj, int n)
{
    int64_t idx = obj->idxsz;
    vmobj *o = alcobj(ctx);
    if (idx > 0) {
        int nx = n < idx ? n : idx;
        for (int i = nx; i >= 1; --i) {
            array_push(ctx, o, obj->ary[idx - i]);
        }
        obj->idxsz -= nx;
    }
    return o;
}

vmobj *array_remove_obj(vmobj *obj, vmobj *rmv)
{
    int n = obj->idxsz;
    for (int i = 0, j = 0; i < n; ++i) {
        vmvar *c = obj->ary[i];
        if (c && c->t == VAR_OBJ && (c->o != rmv)) {
            if (j < i) {
                obj->ary[j] = obj->ary[i];
            }
            ++j;
        } else {
            obj->idxsz--;
        }
    }
    return obj;
}

int array_replace_obj(vmctx *ctx, vmobj *obj, vmobj *obj1, vmobj *obj2)
{
    int n = obj->idxsz;
    for (int i = 0; i < n; ++i) {
        vmvar *c = obj->ary[i];
        if (c && c->t == VAR_OBJ && (c->o == obj1)) {
            obj->ary[i] = alcvar_obj(ctx, obj2);
            return i;
        }
    }
    return -1;
}

int array_insert_before_obj(vmctx *ctx, vmobj *obj, vmobj *key, vmobj* ins)
{
    int64_t idx = obj->idxsz;
    int asz = obj->asz;
    if (asz <= idx) {
        array_extend(ctx, obj, idx + 1);
        obj->idxsz = idx + 1;
    } else if (obj->idxsz <= idx) {
        obj->idxsz = idx + 1;
    }
    int p = -1;
    int n = obj->idxsz;
    for (int i = 0; i < n; ++i) {
        vmvar *c = obj->ary[i];
        if (c && c->t == VAR_OBJ && (c->o == key)) {
            p = i;
            break;
        }
    }
    if (p < 0) {
        return 0;
    }
    for (int i = n - 1; p < i; --i) {
        obj->ary[i] = obj->ary[i-1];
    }
    obj->ary[p] = alcvar_obj(ctx, ins);
    return 1;
}

int array_insert_after_obj(vmctx *ctx, vmobj *obj, vmobj *key, vmobj* ins)
{
    int n = obj->idxsz;
    int64_t idx = obj->idxsz;
    int asz = obj->asz;
    if (asz <= idx) {
        array_extend(ctx, obj, idx + 1);
        obj->idxsz = idx + 1;
    } else if (obj->idxsz <= idx) {
        obj->idxsz = idx + 1;
    }
    int p = -1;
    for (int i = 0; i < n; ++i) {
        vmvar *c = obj->ary[i];
        if (c && c->t == VAR_OBJ && (c->o == key)) {
            p = i + 1;
            break;
        }
    }
    if (p < 0) {
        return 0;
    }
    for (int i = n - 1; p < i; --i) {
        obj->ary[i] = obj->ary[i-1];
    }

    obj->ary[p] = alcvar_obj(ctx, ins);
    return 1;
}

vmobj *object_copy(vmctx *ctx, vmobj *src)
{
    vmobj *obj = hashmap_copy(ctx, src);
    int idxsz = src->idxsz;
    int asz = obj->asz;
    if (asz < idxsz) {
        array_extend(ctx, obj, idxsz);
    }
    obj->idxsz = idxsz;
    for (int i = 0; i < idxsz; ++i) {
        if (src->ary[i]) {
            obj->ary[i] = copy_var(ctx, src->ary[i], 0);
        } else {
            obj->ary[i] = NULL;
        }
    }
    return obj;
}

static int compkeys(const void * n1, const void * n2)
{
    return strcmp((*(vmvar **)n1)->s->hd, (*(vmvar **)n2)->s->hd);
}

vmobj *object_get_keys(vmctx *ctx, vmobj *src)
{
    vmobj *obj = alcobj(ctx);
    int hsz = src->hsz;
    vmvar *map = src->map;

    for (int i = 0; i < hsz; ++i) {
        vmvar *v = &(map[i]);
        if (IS_HASHITEM_EXIST(v)) {
            if (v->k) {
                array_push(ctx, obj, alcvar_str(ctx, v->k));
            }
        }
    }

    qsort(obj->ary, obj->idxsz, sizeof(vmvar *), compkeys);
    return obj;
}
