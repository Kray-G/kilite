#ifndef KILITE_AMALGAMATION
#include "common.h"
#endif

static inline void bin_expand(vmbin *vs, int len)
{
    int cap = (len < BIN_UNIT) ? BIN_UNIT : ((len / BIN_UNIT) * (BIN_UNIT << 1));
    uint8_t *ns = (uint8_t *)calloc(cap, sizeof(uint8_t));
    memcpy(ns, vs->s, vs->len);
    free(vs->s);
    vs->hd = vs->s = ns;
    vs->cap = cap;
}

static inline vmbin *bin_append_impl(vmctx *ctx, vmbin *vs, const uint8_t *s, int l)
{
    if (vs->s < vs->hd) {
        memmove(vs->s, vs->hd, vs->len);
        vs->hd = vs->s;
    }
    int len = vs->len + l;
    if (len < vs->cap) {
        if (s) {
            memcpy(vs->s + vs->len, s, l);
        }
        vs->len = len;
    } else {
        int cap = (len < BIN_UNIT) ? BIN_UNIT : ((len / BIN_UNIT) * (BIN_UNIT << 1));
        uint8_t *ns = (uint8_t *)calloc(cap, sizeof(uint8_t));
        memcpy(ns, vs->s, vs->len);
        if (s) {
            memcpy(ns + vs->len, s, l);
        }
        free(vs->s);
        vs->hd = vs->s = ns;
        vs->len = len;
        vs->cap = cap;
    }
    return vs;
}

int bin_set_i(vmbin *vs, int idx, int64_t i)
{
    if (vs->cap <= idx) {
        bin_expand(vs, idx);
    }
    if (vs->len <= idx) {
        vs->len = idx + 1;
    }
    vs->s[idx] = (uint8_t)(i & 0xFF);
    return 1;
}

int bin_set_d(vmbin *vs, int idx, double *d)
{
    if (vs->cap <= idx) {
        bin_expand(vs, idx);
    }
    if (vs->len <= idx) {
        vs->len = idx + 1;
    }
    vs->s[idx] = (uint8_t)(((int)(*d)) & 0xFF);
    return 1;
}

int bin_set(vmbin *vs, int idx, vmvar *v)
{
    if (vs->cap <= idx) {
        bin_expand(vs, idx);
    }
    if (vs->len <= idx) {
        vs->len = idx + 1;
    }
    switch (v->t) {
    case VAR_UNDEF:
        vs->s[idx] = 0;
        break;
    case VAR_INT64:
        vs->s[idx] = (uint8_t)(v->i & 0xFF);
        break;
    case VAR_DBL:
        vs->s[idx] = (uint8_t)(((int)v->d) & 0xFF);
        break;
    case VAR_STR:
        vs->s[idx] = (uint8_t)(v->s->hd[0]);
        break;
    default:
        return 0;
    }
    return 1;
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
    uint8_t buf[2] = {ch, 0};
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

void fprint_bin(vmctx *ctx, vmbin *vs, FILE *fp)
{
    const uint8_t *p = vs->s;
    int len = vs->len;
    fprintf(fp, "<");
    if (len > 0) {
        fprintf(fp, "0x%02x", *p++);
        for (int i = 1; i < len; ++i) {
            fprintf(fp, ",0x%02x", *p++);
        }
    }
    fprintf(fp, ">");
}

void print_bin(vmctx *ctx, vmbin *vs)
{
    fprint_bin(ctx, vs, stdout);
}
