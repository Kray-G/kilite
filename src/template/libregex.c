#ifndef KILITE_AMALGAMATION
#include "common.h"
#include "lib.h"
#endif

/* Regex */

#define RETTYPE_FOUND (1)
#define RETTYPE_OBJ (2)

#define REGEX_FLAGS_GLOBAL      (0x01)
#define REGEX_FLAGS_IGNORECASE  (0x02)
#define REGEX_FLAGS_EXTEND      (0x04)
#define REGEX_FLAGS_MULTILINE   (0x08)
#define REGEX_FLAGS_STICKY      (0x10)

#define is_regex_global(flags)  ((flags & REGEX_FLAGS_GLOBAL) == REGEX_FLAGS_GLOBAL)

typedef struct regex_pack_t {
    regex_t *reg;
    unsigned char *source;
    OnigRegion *region;
    int32_t flags;
} regex_pack_t;

static OnigOptionType make_onig_option(int32_t flags)
{
    OnigOptionType options = ONIG_OPTION_NONE;
    if (flags & REGEX_FLAGS_IGNORECASE) options |= ONIG_OPTION_IGNORECASE;
    if (flags & REGEX_FLAGS_EXTEND)     options |= ONIG_OPTION_EXTEND;
    if (flags & REGEX_FLAGS_MULTILINE)  options |= ONIG_OPTION_MULTILINE;
    return options;
}

static void *Regex_compile(const char *pattern, int32_t flags)
{
    regex_pack_t *r = (regex_pack_t *)calloc(1, sizeof(regex_pack_t));
    r->flags = flags;
    int rx = Regex_onig_new(&r->reg, pattern, make_onig_option(flags));
    if (rx != ONIG_NORMAL) {
        return NULL;
    }
    r->source = NULL;
    r->region = onig_region_new();

    return r;
}

void Regex_free(void *p)
{
    regex_pack_t *r = (regex_pack_t *)p;
    onig_region_free(r->region, 1);
    onig_free(r->reg);
    free(r->source);
    free(r);
}

#define Regexp(ro, rpack, rp) regex_pack_t *rp = NULL; vmvar *rpack = NULL; if (ro) { \
    if (0 < ro->o->idxsz && ro->o->ary[0]) { \
        rpack = ro->o->ary[0]; \
        if (rpack->t == VAR_VOIDP) { \
            rp = rpack->p; \
        } \
    } \
} \
/**/

#define ResetRegex(rx, rp, sv) { \
    const char *str = sv->s->hd; \
    KL_SET_PROPERTY_S(rx->o, source, str); \
    free(rp->source); \
    rp->source = calloc(strlen(str) + 2, sizeof(char)); \
    strcpy(rp->source, str); \
    KL_SET_PROPERTY_I(rx->o, lastIndex, 0); \
} \
/**/

int Regex_reset(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(rx, 0, VAR_OBJ);
    Regexp(rx, rpack, rp);
    if (rp) {
        DEF_ARG(sv, 1, VAR_STR);
        ResetRegex(rx, rp, sv);
    }

    return 0;
}

int Regex_setPosition(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(rx, 0, VAR_OBJ);
    Regexp(rx, rpack, rp);
    if (rp) {
        DEF_ARG(pos, 1, VAR_INT64);
        int p = pos->i;
        if (p < 0) {
            return throw_system_exception(__LINE__, ctx, EXCEPT_REGEX_ERROR, "Start position should be positive");
        }
        if (!rp->source) {
            return throw_system_exception(__LINE__, ctx, EXCEPT_REGEX_ERROR, "No target string");
        }
        if (strlen(rp->source) <= p) {
            return throw_system_exception(__LINE__, ctx, EXCEPT_REGEX_ERROR, "Longer than the target string");
        }
        KL_SET_PROPERTY_I(rx->o, lastIndex, p);
    }

    return 0;
}

vmobj *make_group_object(vmctx *ctx, const char *str, regex_pack_t *rp)
{
    vmobj *group = alcobj(ctx);
    int num = Regex_get_region_numregs(rp->region);
    for (int i = 0; i < num; ++i) {
        vmobj *item = alcobj(ctx);
        int b = Regex_get_region_beg(rp->region, i);
        int e = Regex_get_region_end(rp->region, i);
        KL_SET_PROPERTY_I(item, begin, b);
        KL_SET_PROPERTY_I(item, end, e);
        vmstr *s = alcstr_str(ctx, "");
        str_append(ctx, s, str + b, e - b);
        KL_SET_PROPERTY_SV(item, string, s);
        array_push(ctx, group, alcvar_obj(ctx, item));
    }
    return group;
}

int Regex_find_impl(vmctx *ctx, vmvar *r, vmobj *o, regex_pack_t *rp, int rettype, int ret)
{
    const unsigned char *str = rp->source;
    if (!str) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_REGEX_ERROR, "Uninitialized string source");
    }
    int len = strlen((char*)str);
    int index = hashmap_getint(o, "lastIndex", 0);
    if (index < 0 || len <= index) {
        KL_SET_PROPERTY_I(o, lastIndex, 0);
        SET_I64(r, rettype == RETTYPE_OBJ ? 0 : (ret == 1 ? 0 : 1));
        return 0;
    }

    int sticky = rp->flags & REGEX_FLAGS_STICKY;
    onig_region_clear(rp->region);
    const unsigned char *end = str + len;
    int reg = sticky
        ? onig_match(rp->reg, str, end, str + index, rp->region, ONIG_OPTION_NONE)
        : onig_search(rp->reg, str, end, str + index, end, rp->region, ONIG_OPTION_NONE);
    if (reg == ONIG_MISMATCH) {
        SET_I64(r, rettype == RETTYPE_OBJ ? 0 : (ret == 1 ? 0 : 1));
        return 0;
    }
    int searched = Regex_get_region_end(rp->region, 0);
    KL_SET_PROPERTY_I(o, lastIndex, searched);

    vmobj *group = make_group_object(ctx, str, rp);
    if (rettype == RETTYPE_OBJ) {
        SET_OBJ(r, group);
    } else {
        KL_SET_PROPERTY_O(o, group, group);
        SET_I64(r, ret);
    }
    return 0;
}

int Regex_find(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(rx, 0, VAR_OBJ);
    Regexp(rx, rpack, rp);
    return Regex_find_impl(ctx, r, rx->o, rp, RETTYPE_FOUND, 1);
}

int Regex_find_eq_ne_hook(vmctx *ctx, vmfrm *lex, vmvar *r, int ac, int rettype, int ret)
{
    vmvar *rx = NULL, *sv = NULL;
    DEF_ARG2(a0, 0, VAR_STR, VAR_OBJ);
    DEF_ARG2(a1, 1, VAR_STR, VAR_OBJ);
    if (a0->t == VAR_OBJ && a1->t == VAR_STR) {
        rx = a0;
        sv = a1;
    } else if (a0->t == VAR_STR && a1->t == VAR_OBJ) {
        rx = a1;
        sv = a0;
    }
    Regexp(rx, rpack, rp);
    if (!rp) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, "No Regex object");
    }

    if (!rp->source || strcmp(rp->source, sv->s->hd) != 0) {
        ResetRegex(rx, rp, sv);
    }
    return Regex_find_impl(ctx, r, rx->o, rp, rettype, ret);
}

int Regex_find_eq(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    return Regex_find_eq_ne_hook(ctx, lex, r, ac, RETTYPE_OBJ, 0);
}

int Regex_find_ne(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    return Regex_find_eq_ne_hook(ctx, lex, r, ac, RETTYPE_FOUND, 0);
}

int Regex_matches(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(rx, 0, VAR_OBJ);
    Regexp(rx, rpack, rp);

    const unsigned char *str = rp->source;
    if (!str) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_REGEX_ERROR, "Uninitialized string source");
    }
    int len = strlen((char*)str);

    onig_region_clear(rp->region);
    const unsigned char *end = str + len;
    int rr = onig_match(rp->reg, str, end, str, rp->region, ONIG_OPTION_NONE);
    if (rr == ONIG_MISMATCH) {
        SET_I64(r, 0);
        return 0;
    }
    if (len != rr) {
        SET_I64(r, 0);
        return 0;
    }

    vmobj *group = make_group_object(ctx, str, rp);
    KL_SET_PROPERTY_O(rx->o, group, group);

    SET_I64(r, 1);
    return 0;
}

int Regex_splitOf(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(rx, 0, VAR_OBJ);
    Regexp(rx, rpack, rp);
    DEF_ARG(sv1, 1, VAR_STR);
    const char *str = sv1->s->hd;
    if (!str) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_REGEX_ERROR, "No string to split");
    }

    int index = 0;
    int len = strlen(str);
    const unsigned char *end = str + len;
    vmobj *res = alcobj(ctx);
    vmstr *sv = alcstr_str(ctx, "");

    while (index < len) {
        onig_region_clear(rp->region);
        int rx = onig_search(rp->reg, str + index, end, str + index, end, rp->region, ONIG_OPTION_NONE);
        if (rx == ONIG_MISMATCH) {
            str_append_cp(ctx, sv, str + index);
            array_push(ctx, res, alcvar_sv(ctx, sv));
            break;
        }
        int found_start = Regex_get_region_beg(rp->region, 0);
        int found_end = Regex_get_region_end(rp->region, 0);
        if (found_start == found_end) {
            str_append(ctx, sv, str + index, 1);
            array_push(ctx, res, alcvar_str(ctx, sv->s));
            str_clear(sv);
            ++index;
        } else {
            str_append(ctx, sv, str + index, found_start);
            array_push(ctx, res, alcvar_str(ctx, sv->s));
            str_clear(sv);
            index += found_end;
        }
    }

    SET_OBJ(r, res);
    return 0;
}

static void append_with_replace(vmctx *ctx, regex_pack_t *rp, vmstr *sv, const char *str, int index, const char *newstr, int sn)
{
    #define KL_BUFSIZ (1020)
    #define KL_IS_REPL_CH(c) ((('0' <= (c) && (c) <= '9') || (c) == '&' || (c) == '$'))
    #define KL_APPEND_STR(sv, buf, p, c) \
        buf[p++] = c; \
        if (KL_BUFSIZ <= p) { \
            buf[p] = 0; \
            str_append_cp(ctx, sv, buf); \
            p = 0; \
        } \
    /**/
    char buf[KL_BUFSIZ+4] = {0};
    int p = 0;
    int nr = Regex_get_region_numregs(rp->region);
    int last = sn - 1;
    for (int i = 0; i < sn; ++i) {
        if (i < last && newstr[i] == '$' && KL_IS_REPL_CH(newstr[i+1])) {
            ++i;
            if (newstr[i] == '$') {
                KL_APPEND_STR(sv, buf, p, '$');
            } else {
                int n = (newstr[i] == '&') ? 0 : (newstr[i] - '0');
                if (n < nr) {
                    int b = index + Regex_get_region_beg(rp->region, n);
                    int e = index + Regex_get_region_end(rp->region, n);
                    for (int j = b; j < e; ++j) {
                        KL_APPEND_STR(sv, buf, p, str[j]);
                    }
                } else {
                    /* not appending any string */;
                }
            }
        } else {
            KL_APPEND_STR(sv, buf, p, newstr[i]);
        }
    }
    #undef KL_APPEND_STR
    #undef KL_IS_REPL_CH
    #undef KL_BUFSIZ
    if (p > 0) {
        buf[p] = 0;
        str_append_cp(ctx, sv, buf);
    }
}

int Regex_replaceOf(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(rx, 0, VAR_OBJ);
    Regexp(rx, rpack, rp);
    DEF_ARG(sv1, 1, VAR_STR);
    const char *str = sv1->s->hd;
    DEF_ARG(sv2, 2, VAR_STR);
    const char *newstr = sv2->s->hd;

    if (!str || !newstr) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_REGEX_ERROR, "No string to replace");
    }
    int sn = strlen(newstr);
    int index = 0;
    int len = strlen(str);
    const unsigned char *end = str + len;
    vmstr *sv = alcstr_str(ctx, "");

    while (index < len) {
        onig_region_clear(rp->region);
        int rx = onig_search(rp->reg, str + index, end, str + index, end, rp->region, ONIG_OPTION_NONE);
        if (rx == ONIG_MISMATCH) {
            str_append_cp(ctx, sv, str + index);
            break;
        }
        int found_start = Regex_get_region_beg(rp->region, 0);
        int found_end = Regex_get_region_end(rp->region, 0);
        if (found_start == found_end) {
            str_append(ctx, sv, str + index, 1);
            append_with_replace(ctx, rp, sv, str, index, newstr, sn);
            ++index;
        } else {
            str_append(ctx, sv, str + index, found_start);
            append_with_replace(ctx, rp, sv, str, index, newstr, sn);
            index += found_end;
        }
    }

    SET_SV(r, sv);
    return 0;
}

static int32_t make_pattern_string(vmctx *ctx, const char *pattern, const char *flags)
{
    int32_t fl = 0x00;
    const char *f = flags;
    while (*f) {
        if (*f == 'g') fl |= REGEX_FLAGS_GLOBAL;
        if (*f == 'i') fl |= REGEX_FLAGS_IGNORECASE;
        if (*f == 'm') fl |= REGEX_FLAGS_MULTILINE;
        if (*f == 'x') fl |= REGEX_FLAGS_EXTEND;
        if (*f == 'y') fl |= REGEX_FLAGS_STICKY;
        ++f;
    }
    return fl;
}

int Regex_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(patv, 0, VAR_STR);
    const char *pattern = patv->s->hd;
    DEF_ARG_OR_UNDEF(flagsv, 1, VAR_STR);

    vmobj *o = alcobj(ctx);
    o->is_sysobj = 1;
    vmvar *rpack = alcvar(ctx, VAR_VOIDP, 0);
    vmstr *sv = alcstr_str(ctx, pattern);
    int32_t flags = make_pattern_string(ctx, pattern, flagsv->t == VAR_STR ? flagsv->s->hd : "");

    rpack->p = Regex_compile(sv->s, flags);
    rpack->freep = Regex_free;
    array_push(ctx, o, rpack);

    KL_SET_METHOD(o, reset, Regex_reset, lex, 2);
    KL_SET_METHOD(o, setPosition, Regex_setPosition, lex, 2);
    KL_SET_METHOD(o, matches, Regex_matches, lex, 2);
    KL_SET_METHOD(o, find, Regex_find, lex, 2);
    KL_SET_METHOD(o, find_eq, Regex_find_eq, lex, 2);
    KL_SET_METHOD(o, find_ne, Regex_find_ne, lex, 2);
    KL_SET_METHOD(o, splitOf, Regex_splitOf, lex, 2);
    KL_SET_METHOD(o, replaceOf, Regex_replaceOf, lex, 2);
    KL_SET_PROPERTY_I(o, isRegex, 1);
    KL_SET_PROPERTY_I(o, lastIndex, 0);
    KL_SET_PROPERTY_I(o, global, (flags & REGEX_FLAGS_GLOBAL) ? 1 : 0);
    KL_SET_PROPERTY_I(o, ignoreCase, (flags & REGEX_FLAGS_IGNORECASE) ? 1 : 0);
    KL_SET_PROPERTY_I(o, multiline, (flags & REGEX_FLAGS_MULTILINE) ? 1 : 0);
    KL_SET_PROPERTY_I(o, extend, (flags & REGEX_FLAGS_MULTILINE) ? 1 : 0);
    KL_SET_PROPERTY_I(o, sticky, (flags & REGEX_FLAGS_STICKY) ? 1 : 0);
    KL_SET_PROPERTY_SV(o, pattern, sv);
    SET_OBJ(r, o);
    return 0;
}

int Regex(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->regex = alcfnc(ctx, Regex_create, lex, "create", 2);
    KL_SET_METHOD_F(o, create, ctx->regex);
    ctx->regeq = alcfnc(ctx, Regex_find_eq, lex, "=~", 2);
    KL_SET_METHOD_F(o, =~, ctx->regeq);
    ctx->regne = alcfnc(ctx, Regex_find_ne, lex, "!~", 2);
    KL_SET_METHOD_F(o, !~, ctx->regne);
    SET_OBJ(r, o);
    return 0;
}
