#ifndef KILITE_AMALGAMATION
#include "common.h"
#endif

static inline char str_head_ch(vmstr *vs)
{
    return (vs->len == 0) ? 0 : (vs->s[0]);
}

static inline char str_last_ch(vmstr *vs)
{
    return (vs->len == 0) ? 0 : (vs->s[vs->len-1]);
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
    int xlen = len + 1;
    if (xlen < vs->cap) {
        strncpy(vs->s + vs->len, s, l);
        vs->s[len] = 0;
        vs->len = len;
    } else {
        int cap = (xlen < STR_UNIT) ? STR_UNIT : ((xlen / STR_UNIT) * (STR_UNIT << 1));
        char *ns = (char *)calloc(cap, sizeof(char));
        strcpy(ns, vs->s);
        strncpy(ns + vs->len, s, l);
        ns[len] = 0;
        free(vs->s);
        vs->hd = vs->s = ns;
        vs->len = len;
        vs->cap = cap;
    }
    return vs;
}

void fprint_escape_str(vmstr *vs, FILE *fp)
{
    const char *s = vs->s;
    if (!s) {
        return;
    }

    while (*s) {
        if (*s == '\n') {
            fprintf(fp, "\\n");
            ++s;
            continue;
        }
        if (*s == '\r') {
            fprintf(fp, "\\r");
            ++s;
            continue;
        }
        if (*s == '\t') {
            fprintf(fp, "\\t");
            ++s;
            continue;
        }
        if (*s == '"' || *s == '\\') {
            fprintf(fp, "\\");
        }
        fprintf(fp, "%c", *s);
        s++;
    }
}

void print_escape_str(vmstr *vs)
{
    fprint_escape_str(vs, stdout);
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
    return str_append_impl(ctx, vs, s2->hd, s2->len);
}

vmstr *str_append_fmt(vmctx *ctx, vmstr *vs, const char *fmt, ...)
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

    va_list tmpa;
    va_start(tmpa, fmt);
    int l = vsnprintf(NULL, 0, fmt, tmpa);
    va_end(tmpa);

    va_list ap;
    va_start(ap, fmt);
    int len = vs->len + l;
    if (len < vs->cap) {
        vsnprintf(vs->s + vs->len, l + 1, fmt, ap);
        vs->len = len;
    } else {
        int cap = (len < STR_UNIT) ? STR_UNIT : ((len / STR_UNIT) * (STR_UNIT << 1));
        char *ns = (char *)calloc(cap, sizeof(char));
        strcpy(ns, vs->s);
        vsnprintf(ns + vs->len, l + 1, fmt, ap);
        free(vs->s);
        vs->hd = vs->s = ns;
        vs->len = len;
        vs->cap = cap;
    }

    va_end(ap);
    return vs;
}

vmstr *str_append_ch(vmctx *ctx, vmstr *vs, const char ch)
{
    char buf[2] = {ch, 0};
    return str_append_impl(ctx, vs, buf, 1);
}

vmstr *str_append_dbl(vmctx *ctx, vmstr *vs, double *d)
{
    char buf[256] = {0};
    sprintf(buf, "%.16g", *d);
    return str_append_impl(ctx, vs, buf, strlen(buf));
}

vmstr *str_append_fmt_dbl(vmctx *ctx, vmstr *vs, const char *fmt, double *d)
{
    char buf[256] = {0};
    sprintf(buf, fmt, *d);
    return str_append_impl(ctx, vs, buf, strlen(buf));
}

vmstr *str_append_i64(vmctx *ctx, vmstr *vs, int64_t i)
{
    return str_append_fmt(ctx, vs, "%" PRId64, i);
}

vmstr *str_clear(vmstr *vs)
{
    vs->s[0] = vs->hd[0] = 0;
    vs->len = 0;
    return vs;
}

vmstr *str_set(vmctx *ctx, vmstr *vs, const char *s, int len)
{
    vs->s[0] = vs->hd[0] = 0;
    vs->len = 0;
    str_append_impl(ctx, vs, s, len);
    return vs;
}

vmstr *str_set_cp(vmctx *ctx, vmstr *vs, const char *s)
{
    vs->s[0] = vs->hd[0] = 0;
    vs->len = 0;
    str_append_impl(ctx, vs, s, strlen(s));
    return vs;
}

vmstr *str_dup(vmctx *ctx, vmstr *vs)
{
    return alcstr_str(ctx, vs->hd);
}

vmstr *str_from_i64(vmctx *ctx, int64_t i)
{
    vmstr *vs = alcstr_str(ctx, NULL);
    return str_append_i64(ctx, vs, i);
}

vmstr *str_from_dbl(vmctx *ctx, double *d)
{
    vmstr *vs = alcstr_str(ctx, NULL);
    return str_append_dbl(ctx, vs, d);
}

vmstr *str_make_double(vmctx *ctx, vmstr *vs)
{
    int len = vs->len;
    int len2 = len * 2;
    if (len2 < vs->cap) {
        for (int i = 0; i < len; ++i) {
            vs->s[i+len] = vs->s[i];
        }
        vs->s[len2] = 0;
        vs->len = len2;
        return vs;
    }

    len = vs->len;
    char *str = (char *)calloc(len + 1, sizeof(char));
    strcpy(str, vs->s);
    str_append(ctx, vs, str, len);
    free(str);
    return vs;
}

vmstr *str_make_ntimes(vmctx *ctx, vmstr *vs, int n)
{
    switch (n) {
    case 0:
        return str_clear(vs);
    case 1:
        return vs;
    case 2:
        return str_make_double(ctx, vs);
    default:
        if (n < 0) {
            return str_clear(vs);
        }
        break;
    }

    char *str = (char *)calloc(vs->len + 1, sizeof(char));
    strcpy(str, vs->s);
    int len = vs->len;
    int l = 1;
    int l2 = 2;
    int ln = n;
    while (l2 < ln) {
        str_make_double(ctx, vs);
        l = l2;
        l2 *= 2;
    }
    int rn = ln - l;  /* must be positive. */
    for (int i = 0; i < rn; ++i) {
        str_append(ctx, vs, str, len);
    }
    free(str);
    return vs;
}

vmstr *str_make_path(vmctx *ctx, vmstr *v0, vmstr *v1)
{
    char *str = (char *)calloc(v1->len + 1, sizeof(char));
    strcpy(str, v1->s);

    str_rtrim(ctx, v0, "/");
    char *p = str;
    while (*p == '/') {
        p++;
    }
    str_append(ctx, v0, "/", 1);
    str_append_cp(ctx, v0, p);

    free(str);
    return v0;
}

vmstr *str_make_path_i64(vmctx *ctx, vmstr *v0, int64_t i)
{
    str_rtrim(ctx, v0, "/");
    char buf[32] = {0};
    snprintf(buf, 31, "/%" PRId64, i);
    return str_append(ctx, v0, buf, strlen(buf));
}

vmstr *str_make_i64_path(vmctx *ctx, int64_t i, vmstr *v0)
{
    char buf[32] = {0};
    snprintf(buf, 31, "%" PRId64 "/", i);

    int nlen = v0->len + strlen(buf);
    char *str = (char *)calloc(nlen + 1, sizeof(char));
    snprintf(str, nlen, "%s%s", buf, v0->s);
    str_clear(v0);
    str_append(ctx, v0, str, nlen);
    free(str);
    return v0;
}

vmstr *str_trim(vmctx *ctx, vmstr *vs, const char *ch)
{
    str_ltrim(ctx, vs, ch);
    str_rtrim(ctx, vs, ch);
    return vs;
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
