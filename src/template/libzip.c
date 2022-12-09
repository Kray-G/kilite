#ifndef KILITE_AMALGAMATION
#include "common.h"
#include "lib.h"
#endif

/* Zip */

#define ZIP_OPTION_I(param, opts, zip, def) \
    int64_t param = opts->t == VAR_OBJ ? Zip_get_int(opts->o, #param, def) : def; \
    if (!param) param = Zip_get_int(zip, #param, def); \
/**/
#define ZIP_OPTION_S(param, opts, zip) \
    const char *param = opts->t == VAR_OBJ ? Zip_get_str(opts->o, #param) : NULL; \
    if (!param) param = Zip_get_str(zip, #param); \
/**/
#define ZIP_OPTION_V(param, opts) \
    vmvar *param = opts->t == VAR_OBJ ? hashmap_search(opts->o, #param) : NULL; \
/**/

typedef struct overwrite_status_t {
    int overwrite;
    int skipped;
} overwrite_status_t;

static int zip_error(vmctx *ctx, int err, const char *arg)
{
    char buf[256] = {0};
    switch (err) {
    case MZ_STREAM_ERROR:
        snprintf(buf, 240, "Stream error");
        break;
    case MZ_DATA_ERROR:
        snprintf(buf, 240, "Data error");
        break;
    case MZ_MEM_ERROR:
        snprintf(buf, 240, "Memory error");
        break;
    case MZ_BUF_ERROR:
        snprintf(buf, 240, "Buffer error");
        break;
    case MZ_VERSION_ERROR:
        snprintf(buf, 240, "Version error");
        break;
    case MZ_END_OF_LIST:
        if (arg) {
            snprintf(buf, 240, "Entry not found: %s", arg);
        } else {
            snprintf(buf, 240, "Entry not found");
        }
        break;
    case MZ_END_OF_STREAM:
        snprintf(buf, 240, "End of stream");
        break;
    case MZ_PARAM_ERROR:
        snprintf(buf, 240, "Parameter error");
        break;
    case MZ_FORMAT_ERROR:
        snprintf(buf, 240, "Format error");
        break;
    case MZ_INTERNAL_ERROR:
        snprintf(buf, 240, "Internal error");
        break;
    case MZ_CRC_ERROR:
        snprintf(buf, 240, "CRC error");
        break;
    case MZ_CRYPT_ERROR:
        snprintf(buf, 240, "Crypt error");
        break;
    case MZ_EXIST_ERROR:
        if (arg) {
            snprintf(buf, 240, "File already exists: %s", arg);
        } else {
            snprintf(buf, 240, "File already exists");
        }
        break;
    case MZ_PASSWORD_ERROR:
        snprintf(buf, 240, "Password error");
        break;
    case MZ_SUPPORT_ERROR:
        snprintf(buf, 240, "Support error");
        break;
    case MZ_HASH_ERROR:
        snprintf(buf, 240, "Hash error");
        break;
    case MZ_OPEN_ERROR:
        if (arg) {
            snprintf(buf, 240, "Cannot open file: %s", arg);
        } else {
            snprintf(buf, 240, "Cannot open file");
        }
        break;
    case MZ_CLOSE_ERROR:
        snprintf(buf, 240, "Close error");
        break;
    case MZ_SEEK_ERROR:
        snprintf(buf, 240, "Seek error");
        break;
    case MZ_TELL_ERROR:
        snprintf(buf, 240, "Tell error");
        break;
    case MZ_READ_ERROR:
        snprintf(buf, 240, "Read error");
        break;
    case MZ_WRITE_ERROR:
        snprintf(buf, 240, "Writ error");
        break;
    case MZ_SIGN_ERROR:
        snprintf(buf, 240, "Sign error");
        break;
    case MZ_SYMLINK_ERROR:
        snprintf(buf, 240, "Symlink error");
        break;
    case MZ_READONLY_ERROR:
        snprintf(buf, 240, "Read only mode");
        break;
    case MZ_WRITEONLY_ERROR:
        snprintf(buf, 240, "Write only mode");
        break;
    case MZ_CONTENT_ERROR:
        snprintf(buf, 240, "Invalid content");
        break;
    default:
        snprintf(buf, 240, "Unknown error");
        break;
    }
    return throw_system_exception(__LINE__, ctx, EXCEPT_ZIP_ERROR, buf);
}

static vmobj *Zip_get_zip_object(vmobj *o)
{
    vmvar *holder = hashmap_search(o, "zip");
    if (!holder || holder->t != VAR_OBJ) return NULL;
    return holder->o;
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

static const char *Zip_get_zipname(vmobj *o)
{
    return Zip_get_str(o, "name");
}

static inline int isReadable(int mode)
{
    return (mode & FILE_READ) == FILE_READ;
}

static inline int isWritable(int mode)
{
    return (mode & FILE_WRITE) == FILE_WRITE || (mode & FILE_APPEND) == FILE_APPEND;
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

static int32_t extract_file_overwrite_cb(void *handle, void *userdata, mz_zip_file *file_info, const char *path)
{
    overwrite_status_t *status = (overwrite_status_t *)userdata;
    if (status->overwrite == 0) {
        status->skipped = MZ_EXIST_ERROR;
        return MZ_EXIST_ERROR;
    }
    status->skipped = 0;
    return MZ_OK;
}

static int32_t Zip_extract_impl(vmctx *ctx, vmvar *r, const char *zipname, const char *filename, const char *password, int overwrite, int skip, const char *outfile, int isbin)
{
    mz_zip_file *file_info = NULL;
    int32_t err = MZ_OK;
    void *reader = NULL;
    overwrite_status_t status = { .overwrite = overwrite };
    /*
     * overwrite: if the file exists, overwrite it.
     * skip:      if the file exists, skip it.
     *
     *  Note that those parameters are independent because a user have to select which one definitely.
     *  For example, if you don't want to overwrite it but you don't set also the skip parameter,
     *  skipping some files should be handled as an error.
     */

    mz_zip_reader_create(&reader);
    mz_zip_reader_set_overwrite_cb(reader, (void*)&status, extract_file_overwrite_cb);

    err = Zip_get_fileinfo(reader, password, zipname, filename, &file_info);
    if (err != MZ_OK) {
        mz_zip_reader_delete(&reader);
        return err;
    }

    if (mz_zip_reader_entry_is_dir(reader) == MZ_OK) {
        mz_zip_reader_delete(&reader);
        return MZ_OK;
    }

    if (outfile) {
        err = mz_zip_reader_entry_save_file(reader, outfile);
        if (!skip && status.skipped) {
            err = status.skipped;
        }
        mz_zip_reader_delete(&reader);
        return err;
    }

    int32_t buf_size = (int32_t)mz_zip_reader_entry_save_buffer_length(reader);
    uint8_t *buf = (char *)calloc(buf_size + 1, sizeof(uint8_t));
    err = mz_zip_reader_entry_save_buffer(reader, buf, buf_size);
    if (!skip && status.skipped) {
        err = status.skipped;
    }
    if (err != MZ_OK) {
        free(buf);
        mz_zip_reader_delete(&reader);
        return err;
    }

    if (isbin) {
        vmbin *bin = alcbin_allocated_bin(ctx, buf, buf_size);
        SET_BIN(r, bin);
    } else {
        vmstr *str = alcstr_allocated_str(ctx, buf, buf_size + 1);
        SET_VMSTR(r, str);
    }

    mz_zip_reader_delete(&reader);
    return MZ_OK;
}

static int Zip_add_file_impl(vmctx *ctx, const char *zipname, int aes, uint16_t method, int level, int64_t disk_size, uint8_t append, const char *password, const char *filename)
{
    int32_t err = MZ_OK;
    void *writer = NULL;

    mz_zip_writer_create(&writer);

    mz_zip_writer_set_compress_method(writer, method);
    mz_zip_writer_set_compress_level(writer, method == MZ_COMPRESS_METHOD_STORE ? 0 : level);
    if (password) {
        mz_zip_writer_set_password(writer, password);
    }

    err = mz_zip_writer_open_file(writer, zipname, disk_size, append);
    if (err != MZ_OK) {
        mz_zip_writer_delete(&writer);
        return err;
    }

    mz_zip_writer_set_aes(writer, aes);
    err = mz_zip_writer_add_path(writer, filename, NULL, 1, 1);
    mz_zip_writer_delete(&writer);

    return err;
}

static int Zip_add_buffer_impl(vmctx *ctx, const char *zipname, int aes, uint16_t method, int level, int64_t disk_size, uint8_t append, const char *password, const char *filename, vmvar *v)
{
    int32_t err = MZ_OK;
    void *writer = NULL;

    mz_zip_writer_create(&writer);

    mz_zip_writer_set_compress_method(writer, method);
    mz_zip_writer_set_compress_level(writer, method == MZ_COMPRESS_METHOD_STORE ? 0 : level);
    if (password) {
        mz_zip_writer_set_password(writer, password);
    }

    err = mz_zip_writer_open_file(writer, zipname, disk_size, append);
    if (err != MZ_OK) {
        mz_zip_writer_delete(&writer);
        return err;
    }

    void *buf = NULL;
    int32_t len = 0;
    if (v->t == VAR_STR) {
        buf = v->s->s;
        len = v->s->len;
    } else if (v->t == VAR_BIN) {
        buf = v->bn->s;
        len = v->bn->len;
    } else {
        return MZ_CONTENT_ERROR;
    }

    mz_zip_file* file_info = mz_alloc_file_info(filename, time(NULL), method, MZ_ZIP_FLAG_UTF8, aes);
    err = mz_zip_writer_add_buffer(writer, buf, len, file_info);
    mz_free_file_info(file_info);
    mz_zip_writer_delete(&writer);

    return err;
}

static int Zip_extract_entry(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(entry, 0, VAR_OBJ)          // entry object
    vmobj *zip = Zip_get_zip_object(entry->o);
    const char *zipname = Zip_get_zipname(zip);
    int mode = Zip_get_int(zip, "mode", 0);
    if (!isReadable(mode)) {
        return zip_error(ctx, MZ_WRITEONLY_ERROR, NULL);
    }

    const char *filename = Zip_get_str(entry->o, "filename");
    DEF_ARG_OR_UNDEF(opts, 1, VAR_OBJ)  // options
    ZIP_OPTION_S(password, opts, zip);
    ZIP_OPTION_I(isbin, opts, zip, 0);

    int32_t err = Zip_extract_impl(ctx, r, zipname, filename, password, 1, 1, NULL, isbin);
    if (err != MZ_OK) {
        return zip_error(ctx, err, filename);
    }

    return 0;
}

static int Zip_extract_entry_to(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(entry, 0, VAR_OBJ)          // entry object
    vmobj *zip = Zip_get_zip_object(entry->o);
    const char *zipname = Zip_get_zipname(zip);
    int mode = Zip_get_int(zip, "mode", 0);
    if (!isReadable(mode)) {
        return zip_error(ctx, MZ_WRITEONLY_ERROR, NULL);
    }

    const char *filename = Zip_get_str(entry->o, "filename");
    DEF_ARG(outfv, 1, VAR_STR)          // output file's name
    const char *outfile = outfv->s->s;
    DEF_ARG_OR_UNDEF(opts, 2, VAR_OBJ)  // options
    ZIP_OPTION_S(password, opts, zip);
    ZIP_OPTION_I(isbin, opts, zip, 0);
    ZIP_OPTION_I(overwrite, opts, zip, 0);
    ZIP_OPTION_I(skip, opts, zip, 0);

    int32_t err = Zip_extract_impl(ctx, r, zipname, filename, password, overwrite, skip, outfile, isbin);
    if (err != MZ_OK) {
        return zip_error(ctx, err, filename);
    }

    return 0;
}

static vmobj *Zip_setup_entry(vmctx *ctx, vmfrm *lex, vmobj *zip, void *reader, mz_zip_file *file_info)
{
    int year, mon, mday, hour, min, sec;
    mz_zip_timeinfo(file_info, TIMEINFO_TYPE_MODIFIED, &year, &mon, &mday, &hour, &min, &sec);
    int aesbit = mz_zip_fileinfo_aes_bit(file_info);

    vmobj *entry = alcobj(ctx);
    entry->is_sysobj = 1;
    KL_SET_PROPERTY_O(entry, zip, zip);
    KL_SET_METHOD(entry, read, Zip_extract_entry, lex, 1);
    KL_SET_METHOD(entry, extract, Zip_extract_entry, lex, 1);
    KL_SET_METHOD(entry, extractTo, Zip_extract_entry_to, lex, 1);
    KL_SET_PROPERTY_I(entry, isDirectory, mz_zip_reader_entry_is_dir(reader) == MZ_OK);
    KL_SET_PROPERTY_I(entry, isEncrypted, mz_zip_fileinfo_crypt(file_info));
    KL_SET_PROPERTY_I(entry, isZip64, mz_zip_fileinfo_zip64(file_info));
    KL_SET_PROPERTY_I(entry, size, mz_zip_fileinfo_uncompsize(file_info));
    KL_SET_PROPERTY_I(entry, compsize, mz_zip_fileinfo_compsize(file_info));
    KL_SET_PROPERTY_I(entry, crc32, mz_zip_fileinfo_crc(file_info));
    KL_SET_PROPERTY_S(entry, filename, mz_zip_fileinfo_filename(file_info));
    KL_SET_PROPERTY_S(entry, filename, mz_zip_fileinfo_filename(file_info));
    if (aesbit) {
        KL_SET_PROPERTY_I(entry, aesBit, aesbit);
    }
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
    int mode = Zip_get_int(zip, "mode", 0);
    if (!isReadable(mode)) {
        return zip_error(ctx, MZ_WRITEONLY_ERROR, NULL);
    }

    mz_zip_file *file_info = NULL;
    int16_t level = 0;
    int32_t err = MZ_OK;
    int crypt = 0;
    void *reader = NULL;

    mz_zip_reader_create(&reader);
    err = mz_zip_reader_open_file(reader, zipname);
    if (err != MZ_OK) {
        mz_zip_reader_delete(&reader);
        return zip_error(ctx, err, zipname);
    }

    err = mz_zip_reader_goto_first_entry(reader);
    if (err != MZ_OK && err != MZ_END_OF_LIST) {
        mz_zip_reader_delete(&reader);
        return zip_error(ctx, err, zipname);
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
        return zip_error(ctx, err, zipname);
    }

    SET_OBJ(r, files);
    return 0;
}

static int Zip_extract(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(zipv, 0, VAR_OBJ)           // zip object
    vmobj *zip = zipv->o;
    const char *zipname = Zip_get_zipname(zip);
    int mode = Zip_get_int(zip, "mode", 0);
    if (!isReadable(mode)) {
        return zip_error(ctx, MZ_WRITEONLY_ERROR, NULL);
    }

    DEF_ARG(filev, 1, VAR_STR)          // target filename
    const char *filename = filev->s->s;
    DEF_ARG_OR_UNDEF(opts, 2, VAR_OBJ)  // options
    ZIP_OPTION_S(password, opts, zip);
    ZIP_OPTION_I(isbin, opts, zip, 0);

    int32_t err = Zip_extract_impl(ctx, r, zipname, filename, password, 1, 1, NULL, isbin);
    if (err != MZ_OK) {
        return zip_error(ctx, err, filename);
    }

    return 0;
}

static int Zip_extract_to(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(zipv, 0, VAR_OBJ)           // zip object
    vmobj *zip = zipv->o;
    const char *zipname = Zip_get_zipname(zip);
    int mode = Zip_get_int(zip, "mode", 0);
    if (!isReadable(mode)) {
        return zip_error(ctx, MZ_WRITEONLY_ERROR, NULL);
    }

    DEF_ARG(filev, 1, VAR_STR)          // target filename
    const char *filename = filev->s->s;
    DEF_ARG(outfv, 2, VAR_STR)          // output file's name
    const char *outfile = outfv->s->s;
    DEF_ARG_OR_UNDEF(opts, 3, VAR_OBJ)  // options
    ZIP_OPTION_S(password, opts, zip);
    ZIP_OPTION_I(isbin, opts, zip, 0);
    ZIP_OPTION_I(overwrite, opts, zip, 0);
    ZIP_OPTION_I(skip, opts, zip, 0);

    int32_t err = Zip_extract_impl(ctx, r, zipname, filename, password, overwrite, skip, outfile, isbin);
    if (err != MZ_OK) {
        return zip_error(ctx, err, filename);
    }

    return 0;
}

static int Zip_find(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(zipv, 0, VAR_OBJ)           // zip object
    vmobj *zip = zipv->o;
    const char *zipname = Zip_get_zipname(zip);
    int mode = Zip_get_int(zip, "mode", 0);
    if (!isReadable(mode)) {
        return zip_error(ctx, MZ_WRITEONLY_ERROR, NULL);
    }

    DEF_ARG(filev, 1, VAR_STR)          // target filename
    const char *filename = filev->s->s;
    mz_zip_file *file_info = NULL;
    int32_t err = MZ_OK;
    int crypt = 0;
    void *reader = NULL;

    mz_zip_reader_create(&reader);
    err = Zip_get_fileinfo(reader, NULL, zipname, filename, &file_info);
    if (err != MZ_OK) {
        mz_zip_reader_delete(&reader);
        return zip_error(ctx, err, filename);
    }

    vmobj *entry = Zip_setup_entry(ctx, lex, zip, reader, file_info);
    mz_zip_reader_delete(&reader);

    SET_OBJ(r, entry)
    return 0;
}

static int Zip_add_file(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(zipv, 0, VAR_OBJ)           // zip object
    vmobj *zip = zipv->o;
    const char *zipname = Zip_get_zipname(zip);
    int added = Zip_get_int(zip, "added", 0);
    int mode = Zip_get_int(zip, "mode", 0);
    if (!isWritable(mode)) {
        return zip_error(ctx, MZ_READONLY_ERROR, NULL);
    }

    DEF_ARG(filev, 1, VAR_STR)          // target filename
    const char *filename = filev->s->s;
    DEF_ARG_OR_UNDEF(opts, 2, VAR_OBJ)  // options
    ZIP_OPTION_S(password, opts, zip);
    ZIP_OPTION_S(method, opts, zip);
    ZIP_OPTION_I(aes, opts, zip, 0);
    ZIP_OPTION_I(diskSize, opts, zip, 0);
    ZIP_OPTION_I(level, opts, zip, 6);

    uint16_t use_method = MZ_COMPRESS_METHOD_DEFLATE;
    if (method) {
        if (strcmp(method, "store") == 0) {
            use_method = MZ_COMPRESS_METHOD_STORE;
        }
    }

    int append = (added > 0) || ((mode & FILE_APPEND) == FILE_APPEND);
    int32_t err = Zip_add_file_impl(ctx, zipname, aes, use_method, level, diskSize, append, password, filename);
    if (err != MZ_OK) {
        return zip_error(ctx, err, filename);
    }

    KL_SET_PROPERTY_I(zip, added, added + 1);
    return 0;
}

static int Zip_add_buffer(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(zipv, 0, VAR_OBJ)               // zip object
    vmobj *zip = zipv->o;
    const char *zipname = Zip_get_zipname(zip);
    int added = Zip_get_int(zip, "added", 0);
    int mode = Zip_get_int(zip, "mode", 0);
    if (!isWritable(mode)) {
        return zip_error(ctx, MZ_READONLY_ERROR, NULL);
    }

    DEF_ARG(filev, 1, VAR_STR)          // target filename
    const char *filename = filev->s->s;
    DEF_ARG_OR_UNDEF(opts, 2, VAR_OBJ)      // options
    ZIP_OPTION_V(content, opts);
    ZIP_OPTION_S(password, opts, zip);
    ZIP_OPTION_S(method, opts, zip);
    ZIP_OPTION_I(aes, opts, zip, 0);
    ZIP_OPTION_I(diskSize, opts, zip, 0);
    ZIP_OPTION_I(level, opts, zip, 6);

    uint16_t use_method = MZ_COMPRESS_METHOD_DEFLATE;
    if (method) {
        if (strcmp(method, "store") == 0) {
            use_method = MZ_COMPRESS_METHOD_STORE;
        }
    }

    int append = (added > 0) || ((mode & FILE_APPEND) == FILE_APPEND);
    int32_t err = Zip_add_buffer_impl(ctx, zipname, aes, use_method, level, diskSize, append, password, filename, content);
    if (err != MZ_OK) {
        return zip_error(ctx, err, filename);
    }

    KL_SET_PROPERTY_I(zip, added, added + 1);
    return 0;
}

static int Zip_set_password(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(zipv, 0, VAR_OBJ)           // zip object
    DEF_ARG(password, 1, VAR_STR)       // password
    KL_SET_PROPERTY_S(zipv->o, password, password->s->s);
    return 0;
}

static int Zip_set_overwrite(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(zipv, 0, VAR_OBJ)           // zip object
    DEF_ARG(overwrite, 1, VAR_INT64)    // overwrite flag
    KL_SET_PROPERTY_I(zipv->o, overwrite, overwrite->i);
    return 0;
}

static int Zip_set_skip(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(zipv, 0, VAR_OBJ)           // zip object
    DEF_ARG(skip, 1, VAR_INT64)         // skip flag
    KL_SET_PROPERTY_I(zipv->o, skip, skip->i);
    return 0;
}

static int Zip_open(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR)             // zipname
    DEF_ARG_OR_UNDEF(a1, 1, VAR_INT64)  // mode

    const char *zipname = a0->s->s;
    int mode = a1->t == VAR_UNDEF ? FILE_READ : a1->i;

    vmobj *o = alcobj(ctx);
    o->is_sysobj = 1;
    KL_SET_PROPERTY_S(o, name, zipname);
    KL_SET_PROPERTY_I(o, mode, mode);
    KL_SET_PROPERTY_I(o, added, 0);
    KL_SET_METHOD(o, entries, Zip_entries, lex, 1);
    KL_SET_METHOD(o, read, Zip_extract, lex, 1);
    KL_SET_METHOD(o, extract, Zip_extract, lex, 1);
    KL_SET_METHOD(o, extractTo, Zip_extract_to, lex, 1);
    KL_SET_METHOD(o, find, Zip_find, lex, 1);
    KL_SET_METHOD(o, addFile, Zip_add_file, lex, 1);
    KL_SET_METHOD(o, addString, Zip_add_buffer, lex, 1);
    KL_SET_METHOD(o, setPassword, Zip_set_password, lex, 1);
    KL_SET_METHOD(o, setOverwrite, Zip_set_overwrite, lex, 1);
    KL_SET_METHOD(o, setSkip, Zip_set_skip, lex, 1);
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
