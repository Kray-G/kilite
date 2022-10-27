#include "common.h"

vmstr *str_dup(vmctx *ctx, vmstr *vs)
{
    return alcstr_str(ctx, vs->hd);
}

static inline vmstr *str_append_impl(vmctx *ctx, vmstr *vs, const char *s, int l)
{
    if (vs->s < vs->hd) {
        char *sp = vs->s;
        char *hp = vs->hd;
        while (*hp) {
            *sp++ = *hp++;
        }
        *sp = 0;
        vs->hd = vs->s;
    }
    int len = vs->len + l;
    if (len < vs->cap) {
        strcpy(vs->s + vs->len, s);
        vs->len = len;
    } else {
        int cap = (len < STR_UNIT) ? STR_UNIT : ((len / STR_UNIT) * (STR_UNIT << 1));
        char *ns = (char *)calloc(cap, sizeof(char));
        strcpy(ns, vs->s);
        strcpy(ns + vs->len, s);
        free(vs->s);
        vs->hd = vs->s = ns;
        vs->len = len;
        vs->cap = cap;
    }
    return vs;
}

vmstr *str_append(vmctx *ctx, vmstr *vs, const char *s, int len)
{
    return str_append_impl(ctx, vs, s, len);
}

vmstr *str_append_cp(vmctx *ctx, vmstr *vs, const char *s)
{
    return str_append_impl(ctx, vs, s, strlen(s));
}

vmstr *str_append_str(vmctx *ctx, vmstr *vs, vmstr *s2)
{
    return str_append_impl(ctx, vs, s2->s, s2->len);
}

vmstr *str_ltrim(vmctx *ctx, vmstr *vs, const char *ch)
{
    if (!ch || *ch == 0) {
        return vs;
    }
    for ( ; *vs->hd; ++vs->hd) {
        int matched = 0;
        const char *c = ch;
        for ( ; *c; ++c) {
            if (*(vs->hd) == *c) {
                matched = 1;
                break;
            }
        }
        if (matched == 0) {
            break;
        }
        vs->len--;
    }
    return vs;
}

vmstr *str_rtrim(vmctx *ctx, vmstr *vs, const char *ch)
{
    if (!ch || *ch == 0) {
        return vs;
    }
    char *p = vs->hd + vs->len - 1; 
    for ( ; vs->hd < p; --p) {
        int matched = 0;
        const char *c = ch;
        for ( ; *c; ++c) {
            if (*p == *c) {
                matched = 1;
                break;
            }
        }
        if (matched == 0) {
            break;
        }
        vs->len--;
    }
    *(p + 1) = 0;
    return vs;
}
