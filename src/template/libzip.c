#ifndef KILITE_AMALGAMATION
#include "common.h"
#include "lib.h"
#endif

/* Zip */

#define ZIP_MODE_READ   (1)
#define ZIP_MODE_WRITE  (2)

typedef struct overwrite_status_t {
    int overwrite;
    int skipped;
} overwrite_status_t;

static vmobj *Zip_get_zip_object(vmobj *o)
{
    vmvar *holder = hashmap_search(o, "zip");
    if (!holder || holder->t != VAR_OBJ) return NULL;
    return holder->o;
}

static const char *Zip_get_zipname(vmobj *o)
{
    vmvar *holder = hashmap_search(o, "name");
    if (!holder || holder->t != VAR_STR) return NULL;
    return holder->s->s;
}

static int Zip_get_int(vmobj *o, const char *optname, int def)
{
    vmvar *v = hashmap_search(o, optname);
    if (!v || v->t != VAR_INT64) return def;
    return (int)v->i;
}

static const char *Zip_get_str(vmobj *o, const char *optname)
{
    vmvar *v = hashmap_search(o, optname);
    if (!v || v->t != VAR_STR) return NULL;
    return v->s->s;
}

static int Zip_get_fileinfo(void *reader, const char *password, const char *zipfile, const char *filename, mz_zip_file **file_info)
{
    int err = MZ_OK;
    if (password) {
        mz_zip_reader_set_password(reader, password);
    }
    err = mz_zip_reader_open_file(reader, zipfile);
    if (err != MZ_OK) {
        return err;
    }
    err = mz_zip_reader_locate_entry(reader, filename, 1);
    if (err != MZ_OK) {
        return err;
    }
    err = mz_zip_reader_entry_get_info(reader, file_info);
    if (err != MZ_OK) {
        return err;
    }
    return MZ_OK;
}

static vmvar *Zip_extract_impl(const char *zipname, const char *filename, const char *password, const char *outfile, int isbin)
{
    ;
}

static int Zip_extract_entry(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ)             // entry object
    vmobj *zip = Zip_get_zip_object(a0->o);
    DEF_ARG_OR_UNDEF(a1, 1, VAR_OBJ)    // options
    const char *filename = Zip_get_str(a0->o, "filename");
    const char *password = a1->t == VAR_OBJ ? Zip_get_str(a1->o, "password") : NULL;
    if (!password) {
        password = Zip_get_str(zip, "password");
    }
    int bin = a1->t == VAR_OBJ ? Zip_get_int(a1->o, "binary", 0) : 0;

//     int r = extract_impl(args, filename, password, 1, 1, binary, obj, ctx, NULL);
//     if (r) {
//         return r;
//     }

    return 0;
}

static int Zip_extract_entry_to(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    return 0;
}

static vmobj *Zip_setup_entry(vmctx *ctx, vmfrm *lex, vmobj *zip, void *reader, mz_zip_file *file_info)
{
    int is_dir = mz_zip_reader_entry_is_dir(reader) == MZ_OK;
    int crypt = mz_zip_fileinfo_crypt(file_info);
    int year, mon, mday, hour, min, sec;
    mz_zip_timeinfo(file_info, TIMEINFO_TYPE_MODIFIED, &year, &mon, &mday, &hour, &min, &sec);

    vmobj *entry = alcobj(ctx);
    KL_SET_PROPERTY_O(entry, zip, zip);
    KL_SET_METHOD(entry, read, Zip_extract_entry, lex, 1);
    KL_SET_METHOD(entry, extract, Zip_extract_entry, lex, 1);
    KL_SET_METHOD(entry, extractTo, Zip_extract_entry_to, lex, 1);
    KL_SET_PROPERTY_I(entry, isDirectory, is_dir);
    KL_SET_PROPERTY_I(entry, isEncrypted, crypt);
    KL_SET_PROPERTY_I(entry, size, mz_zip_fileinfo_uncompsize(file_info));
    KL_SET_PROPERTY_I(entry, compsize, mz_zip_fileinfo_compsize(file_info));
    KL_SET_PROPERTY_I(entry, crc32, mz_zip_fileinfo_crc(file_info));
    KL_SET_PROPERTY_S(entry, filename, mz_zip_fileinfo_filename(file_info));
    switch (mz_zip_fileinfo_comp_method(file_info)) {
    case MZ_COMPRESS_METHOD_DEFLATE:
        KL_SET_PROPERTY_S(entry, method, "deflate");
        break;
    case MZ_COMPRESS_METHOD_STORE:
        KL_SET_PROPERTY_S(entry, method, "store");
        break;
    case MZ_COMPRESS_METHOD_BZIP2:
        KL_SET_PROPERTY_S(entry, method, "bzip2");
        break;
    case MZ_COMPRESS_METHOD_LZMA:
        KL_SET_PROPERTY_S(entry, method, "lzma");
        break;
    case MZ_COMPRESS_METHOD_AES:
        KL_SET_PROPERTY_S(entry, method, "aes");
        break;
    default:
        KL_SET_PROPERTY_S(entry, method, "unknown");
        break;
    }

    vmobj *time = alcobj(ctx);
    KL_SET_PROPERTY_I(time, year, (year < 1900) ? (year + 1900) : year);
    KL_SET_PROPERTY_I(time, month, mon + 1);
    KL_SET_PROPERTY_I(time, day, mday);
    KL_SET_PROPERTY_I(time, hour, hour);
    KL_SET_PROPERTY_I(time, minute, min);
    KL_SET_PROPERTY_I(time, second, sec);
    KL_SET_PROPERTY_O(entry, time, time);
    return entry;
}

static int Zip_entries(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ)     // zip object
    vmobj *zip = a0->o;
    const char *zipname = Zip_get_zipname(zip);

    mz_zip_file *file_info = NULL;
    int16_t level = 0;
    int32_t err = MZ_OK;
    int crypt = 0;
    void *reader = NULL;

    mz_zip_reader_create(&reader);
    err = mz_zip_reader_open_file(reader, zipname);
    if (err != MZ_OK) {
        mz_zip_reader_delete(&reader);
        char buf[256] = {0};
        snprintf(buf, 240, "Cannot open file: %s", zipname);
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, buf);
    }

    err = mz_zip_reader_goto_first_entry(reader);
    if (err != MZ_OK && err != MZ_END_OF_LIST) {
        mz_zip_reader_delete(&reader);
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, NULL);
    }

    vmobj *files = alcobj(ctx);
    do {
        err = mz_zip_reader_entry_get_info(reader, &file_info);
        if (err != MZ_OK) {
            break;
        }

        vmobj *entry = Zip_setup_entry(ctx, lex, zip, reader, file_info);
        array_push(ctx, files, alcvar_obj(ctx, entry));

        err = mz_zip_reader_goto_next_entry(reader);
        if (err != MZ_OK && err != MZ_END_OF_LIST) {
            break;
        }
    } while (err == MZ_OK);

    mz_zip_reader_delete(&reader);
    if (err != MZ_OK && err != MZ_END_OF_LIST) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, NULL);
    }

    SET_OBJ(r, files);
    return 0;
}

static int Zip_find(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ)     // zip object
    vmobj *zip = a0->o;
    DEF_ARG(a1, 1, VAR_STR)     // filename
    const char *filename = a1->s->s;
    const char *zipname = Zip_get_zipname(zip);

    mz_zip_file *file_info = NULL;
    int32_t err = MZ_OK;
    int crypt = 0;
    void *reader = NULL;

    mz_zip_reader_create(&reader);
    err = Zip_get_fileinfo(reader, NULL, zipname, filename, &file_info);
    if (err != MZ_OK) {
        mz_zip_reader_delete(&reader);
        char buf[256] = {0};
        snprintf(buf, 240, "Cannot read the entry: %s", filename);
        return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, buf);
    }

    vmobj *entry = Zip_setup_entry(ctx, lex, zip, reader, file_info);
    mz_zip_reader_delete(&reader);

    SET_OBJ(r, entry)
    return 0;
}

static int Zip_set_password(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ)     // zip object
    vmobj *o = a0->o;
    DEF_ARG(a1, 1, VAR_STR)     // password
    KL_SET_PROPERTY_S(o, password, a1->s->s);
    return 0;
}

static int Zip_set_overwrite(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ)     // zip object
    vmobj *o = a0->o;
    DEF_ARG(a1, 1, VAR_INT64)   // overwrite flag
    KL_SET_PROPERTY_I(o, overwrite, a1->i);
    return 0;
}

static int Zip_open(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR)     // zipname
    DEF_ARG_OR_UNDEF(a1, 1, VAR_INT64)   // mode

    const char *zipname = a0->s->s;
    int mode = a1->t == VAR_UNDEF ? ZIP_MODE_READ : a1->i;

    vmobj *o = alcobj(ctx);
    KL_SET_PROPERTY_S(o, name, zipname);
    KL_SET_PROPERTY_I(o, isReadable, (mode & ZIP_MODE_READ) == ZIP_MODE_READ);
    KL_SET_PROPERTY_I(o, isWritable, (mode & ZIP_MODE_WRITE) == ZIP_MODE_WRITE);
    KL_SET_METHOD(o, entries, Zip_entries, lex, 1);
    // KL_SET_METHOD(o, "read", Zip_extract, lex, 1);
    // KL_SET_METHOD(o, "extract", Zip_extract, lex, 1);
    // KL_SET_METHOD(o, "extractTo", Zip_extract_to, lex, 1);
    KL_SET_METHOD(o, "find", Zip_find, lex, 1);
    // KL_SET_METHOD(o, "addFile", Zip_add_file, lex, 1);
    // KL_SET_METHOD(o, "addString", Zip_add_buffer, lex, 1);
    KL_SET_METHOD(o, setPassword, Zip_set_password, lex, 1);
    KL_SET_METHOD(o, setOverwrite, Zip_set_overwrite, lex, 1);
    o->is_sysobj = 1;
    SET_OBJ(r, o);

    return 0;
}

/* Creating Zip file directly. */
// static int Zip_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
// {

// }

int Zip(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, open, Zip_open, lex, 1);
    KL_SET_METHOD(o, create, Zip_open, lex, 1);
    // KL_SET_METHOD(o, zip, Zip_create, lex, 1)
    SET_OBJ(r, o);
    return 0;
}
