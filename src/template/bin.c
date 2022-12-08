#ifndef KILITE_AMALGAMATION
#include "common.h"
#endif

static inline vmbin *bin_append_impl(vmctx *ctx, vmbin *vs, const uint8_t *s, int l)
{
    if (vs->s < vs->hd) {
        memmove(vs->s, vs->hd, vs->len);
        vs->hd = vs->s;
    }
    int len = vs->len + l;
    if (len < vs->cap) {
        memcpy(vs->s + vs->len, s, l);
        vs->len = len;
    } else {
        int cap = (len < BIN_UNIT) ? BIN_UNIT : ((len / BIN_UNIT) * (BIN_UNIT << 1));
        char *ns = (uint8_t *)calloc(cap, sizeof(uint8_t));
        memcpy(ns, vs->s, vs->len);
        memcpy(ns + vs->len, s, l);
        free(vs->s);
        vs->hd = vs->s = ns;
        vs->len = len;
        vs->cap = cap;
    }
    return vs;
}

vmbin *bin_append(vmctx *ctx, vmbin *vs, const uint8_t *s, int len)
{
    return bin_append_impl(ctx, vs, s, len);
}

vmbin *bin_append_bin(vmctx *ctx, vmbin *vs, vmbin *s2)
{
    return bin_append_impl(ctx, vs, s2->hd, s2->len);
}

vmbin *bin_append_ch(vmctx *ctx, vmbin *vs, const uint8_t ch)
{
    char buf[2] = {ch, 0};
    return bin_append_impl(ctx, vs, buf, 1);
}

vmbin *bin_clear(vmbin *vs)
{
    vs->s[0] = 0;
    vs->len = 0;
    return vs;
}

vmbin *bin_dup(vmctx *ctx, vmbin *vs)
{
    return alcbin_bin(ctx, vs->hd, vs->len);
}
