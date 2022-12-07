#ifndef KILITE_AMALGAMATION
#include "common.h"
#include "lib.h"
#endif

/* TinyZip */

static int zip_error(vmctx *ctx, int err, const char *text)
{
    if (text && (err == ZIP_EOPNFILE || err == ZIP_ENOFILE)) {
        char buf[256] = {0};
        switch (err) {
        case ZIP_EOPNFILE:
            sprintf("Cannot open file: %s", text);
            break;
        case ZIP_ENOFILE:
            sprintf("File not found: %s", text);
            break;
        }
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, buf);
    }

    const char *msg = NULL;
    switch (err) {
    case ZIP_ENOINIT:     msg = "Not initialized";                              break;
    case ZIP_EINVENTNAME: msg = "Invalid entry name";                           break;
    case ZIP_ENOENT:      msg = "Entry not found";                              break;
    case ZIP_EINVMODE:    msg = "Invalid zip mode";                             break;
    case ZIP_EINVLVL:     msg = "Invalid compression level";                    break;
    case ZIP_ENOSUP64:    msg = "No zip 64 support";                            break;
    case ZIP_EMEMSET:     msg = "Memset error";                                 break;
    case ZIP_EWRTENT:     msg = "Cannot write data to entry";                   break;
    case ZIP_ETDEFLINIT:  msg = "Cannot initialize tdefl compressor";           break;
    case ZIP_EINVIDX:     msg = "Invalid index";                                break;
    case ZIP_ENOHDR:      msg = "Header not found";                             break;
    case ZIP_ETDEFLBUF:   msg = "Cannot flush tdefl buffer";                    break;
    case ZIP_ECRTHDR:     msg = "Cannot create entry header";                   break;
    case ZIP_EWRTHDR:     msg = "Cannot write entry header";                    break;
    case ZIP_EWRTDIR:     msg = "Cannot write to central dir";                  break;
    case ZIP_EOPNFILE:    msg = "Cannot open file";                             break;
    case ZIP_EINVENTTYPE: msg = "Invalid entry type";                           break;
    case ZIP_EMEMNOALLOC: msg = "Extracting data using no memory allocation";   break;
    case ZIP_ENOFILE:     msg = "File not found";                               break;
    case ZIP_ENOPERM:     msg = "No permission";                                break;
    case ZIP_EOOMEM:      msg = "Out of memory";                                break;
    case ZIP_EINVZIPNAME: msg = "Invalid zip archive name";                     break;
    case ZIP_EMKDIR:      msg = "Make dir error";                               break;
    case ZIP_ESYMLINK:    msg = "Symlink error";                                break;
    case ZIP_ECLSZIP:     msg = "Close archive error";                          break;
    case ZIP_ECAPSIZE:    msg = "Capacity size too small";                      break;
    case ZIP_EFSEEK:      msg = "Fseek error";                                  break;
    case ZIP_EFREAD:      msg = "Fread error";                                  break;
    case ZIP_EFWRITE:     msg = "Fwrite error";                                 break;
    default:
        msg = "Unknown error";
        break;
    }
    return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, msg);
}

static int TinyZip_entry_close(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    int ret = zip_entry_close(z);
    if (ret < 0) {
        return zip_error(ctx, ret, NULL);
    }

    return 0;
}

static int TinyZip_entry_name(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    const char *name = zip_entry_name(z);
    if (!name) {
        return zip_error(ctx, ZIP_ENOFILE, NULL);
    }
    SET_STR(r, name)
    return 0;
}

static int TinyZip_entry_index(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    ssize_t ret = zip_entry_index(z);
    if (ret < 0) {
        return zip_error(ctx, ret, NULL);
    }
    SET_I64(r, ret)
    return 0;
}

static int TinyZip_entry_isdir(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    int ret = zip_entry_isdir(z);
    if (ret < 0) {
        return zip_error(ctx, ret, NULL);
    }
    SET_I64(r, ret)
    return 0;
}

static int TinyZip_entry_size(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    unsigned long long ret = zip_entry_size(z);
    SET_I64(r, ret)
    return 0;
}

static int TinyZip_entry_uncomp_size(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    unsigned long long ret = zip_entry_uncomp_size(z);
    SET_I64(r, ret)
    return 0;
}

static int TinyZip_entry_comp_size(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    unsigned long long ret = zip_entry_comp_size(z);
    SET_I64(r, ret)
    return 0;
}

static int TinyZip_entry_crc32(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    unsigned int ret = zip_entry_crc32(z);
    SET_I64(r, ret)
    return 0;
}

static int TinyZip_entry_write(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG2(a1, 1, VAR_STR, VAR_BIN);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    void *p = NULL;
    int len = 0;
    if (a1->t == VAR_STR){
        p = a1->s->s;
        len = a1->s->len;
    }
    if (!p || len == 0) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "No zip data to write");
    }
    int ret = zip_entry_write(z, p, len);
    if (ret < 0) {
        return zip_error(ctx, ret, NULL);
    }
    SET_I64(r, ret)
    return 0;
}

static int TinyZip_entry_fwrite(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_STR);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    const char *filename = a1->s->s;
    int ret = zip_entry_fwrite(z, filename);
    if (ret < 0) {
        return zip_error(ctx, ret, filename);
    }
    SET_I64(r, ret)
    return 0;
}

static int TinyZip_entry_read(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }

    unsigned long long size = zip_entry_size(z);
    if (size < 0) {
        return zip_error(ctx, size, NULL);
    }

    char *buf = calloc(sizeof(char), size);
    int ret = zip_entry_noallocread(z, (void *)buf, size);
    r->t = VAR_STR;
    r->s = alcstr_allocated_str(ctx, buf, size);
    if (ret < 0) {
        return zip_error(ctx, ret, NULL);
    }
    return 0;
}

static int TinyZip_entry_fread(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_STR);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_UNDEF && a0->o->ary[0]->a) {
        z = a0->o->ary[0]->a->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    const char *filename = a1->s->s;
    int ret = zip_entry_fread(z, filename);
    if (ret < 0) {
        return zip_error(ctx, ret, filename);
    }
    SET_I64(r, ret)
    return 0;
}

static void set_entry_methods(vmctx *ctx, vmfrm *lex, vmobj *entry)
{
    KL_SET_METHOD(entry, close, TinyZip_entry_close, lex, 1)
    KL_SET_METHOD(entry, name, TinyZip_entry_name, lex, 1)
    KL_SET_METHOD(entry, index, TinyZip_entry_index, lex, 1)
    KL_SET_METHOD(entry, isdir, TinyZip_entry_isdir, lex, 1)
    KL_SET_METHOD(entry, size, TinyZip_entry_size, lex, 1)
    KL_SET_METHOD(entry, uncompSize, TinyZip_entry_uncomp_size, lex, 1)
    KL_SET_METHOD(entry, compSize, TinyZip_entry_comp_size, lex, 1)
    KL_SET_METHOD(entry, crc32, TinyZip_entry_crc32, lex, 1)
    KL_SET_METHOD(entry, write, TinyZip_entry_write, lex, 1)
    KL_SET_METHOD(entry, writeTo, TinyZip_entry_fwrite, lex, 1)
    KL_SET_METHOD(entry, read, TinyZip_entry_read, lex, 1)
    KL_SET_METHOD(entry, readTo, TinyZip_entry_fread, lex, 1)
}

static int TinyZip_entry_open(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_STR);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_VOIDP) {
        z = a0->o->ary[0]->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    const char *entryname = a1->s->s;
    int ret = zip_entry_open(z, entryname);
    if (ret < 0) {
        return zip_error(ctx, ret, entryname);
    }

    vmobj *o = alcobj(ctx);
    vmvar *zip = alcvar_initial(ctx);
    zip->a = a0->o->ary[0];
    array_set(ctx, o, 0, zip);
    set_entry_methods(ctx, lex, o);
    SET_OBJ(r, o);
    o->is_sysobj = 1;
    return 0;
}

static int TinyZip_entry_openbyindex(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_INT64);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_VOIDP) {
        z = a0->o->ary[0]->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    int index = a1->i;
    int ret = zip_entry_openbyindex(z, index);
    if (ret < 0) {
        return zip_error(ctx, ret, NULL);
    }

    vmobj *o = alcobj(ctx);
    vmvar *zip = alcvar_initial(ctx);
    zip->a = a0->o->ary[0];
    array_set(ctx, o, 0, zip);
    set_entry_methods(ctx, lex, o);
    SET_OBJ(r, o);
    o->is_sysobj = 1;
    return 0;
}

static void zip_close_hook(void *p)
{
    zip_close(p);
}

static int TinyZip_close(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_VOIDP) {
        z = a0->o->ary[0]->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    zip_close_hook(z);
    vmvar *v = a0->o->ary[0];
    v->p = NULL;
    v->freep = NULL;
    return 0;
}

static int TinyZip_entries_total(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_VOIDP) {
        z = a0->o->ary[0]->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    int ret = zip_entries_total(z);
    if (ret < 0) {
        return zip_error(ctx, ret, NULL);
    }

    SET_I64(r, ret);
    return 0;
}

static int TinyZip_is64(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    struct zip_t *z = NULL;
    if (0 < a0->o->idxsz && a0->o->ary[0]->t == VAR_VOIDP) {
        z = a0->o->ary[0]->p;
    }
    if (!z) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Zip invalid state");
    }
    int ret = zip_is64(z);
    if (ret < 0) {
        return zip_error(ctx, ret, NULL);
    }

    SET_I64(r, ret);
    return 0;
}

static int TinyZip_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    DEF_ARG2(a1, 1, VAR_STR, VAR_OBJ);

    const char *zipname = a0->s->s;
    int ret;
    if (a1->t == VAR_STR) {
        const char *filenames[] = { a1->s->s };
        ret = zip_create(zipname, filenames, 1);
    } else {
        vmobj *o = a1->o;
        int n = o->idxsz;
        const char **filenames = alloca(n);
        for (int i = 0; i < n; ++i) {
            if (o->ary[i] && o->ary[i]->t == VAR_STR) {
                filenames[i] = o->ary[i]->s->s;
            } else {
                return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, "Invalid filename");
            }
        }
        ret = zip_create(zipname, filenames, n);
    }
    if (ret < 0) {
        return zip_error(ctx, ret, NULL);
    }

    SET_I64(r, ret);
    return 0;
}

static int TinyZip_open(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    DEF_ARG_OR_UNDEF(a1, 1, VAR_INT64);
    DEF_ARG_OR_UNDEF(a2, 2, VAR_STR)
    const char *zipname = a0->s->s;
    int level = a1->t == VAR_UNDEF ? ZIP_DEFAULT_COMPRESSION_LEVEL : (int)a1->i;
    char mode = a2->t == VAR_UNDEF ? 'r' : a2->s->s[0];

    vmvar *zip = alcvar(ctx, VAR_VOIDP, 0);
    zip->p = zip_open(zipname, level, mode);
    if (!zip->p) {
        char buf[256] = {0};
        sprintf(buf, "Cannot open file: %s", zipname);
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, buf);
    }
    zip->freep = zip_close_hook;

    vmobj *o = alcobj(ctx);
    array_set(ctx, o, 0, zip);
    KL_SET_METHOD(o, close, TinyZip_close, lex, 1)
    KL_SET_METHOD(o, entry, TinyZip_entry_open, lex, 1)
    KL_SET_METHOD(o, entryByIndex, TinyZip_entry_openbyindex, lex, 1)
    KL_SET_METHOD(o, entryCount, TinyZip_entries_total, lex, 1)
    KL_SET_METHOD(o, is64, TinyZip_is64, lex, 1)
    SET_OBJ(r, o);
    o->is_sysobj = 1;
    return 0;
}

int TinyZip(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, open, TinyZip_open, lex, 1);
    KL_SET_METHOD(o, create, TinyZip_open, lex, 1);
    KL_SET_METHOD(o, zip, TinyZip_create, lex, 1)
    SET_OBJ(r, o);
    return 0;
}
