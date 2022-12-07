#ifndef KILITE_TEMPLATE_LIB_H
#define KILITE_TEMPLATE_LIB_H

/* check argument's type if it's specified */
#define DEF_ARG(a0, n, type) \
    if (ac < (n+1)) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TOO_FEW_ARGUMENTS, NULL); \
    } \
    vmvar *a0 = local_var(ctx, n); \
    if (a0->t != type) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL); \
    } \
/**/
#define DEF_ARG2(a0, n, type1, type2) \
    if (ac < (n+1)) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TOO_FEW_ARGUMENTS, NULL); \
    } \
    vmvar *a0 = local_var(ctx, n); \
    if (a0->t != type1 && a0->t != type2) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL); \
    } \
/**/
#define DEF_ARG_ANY(a0, n) \
    vmvar *a0 = (ac > n) ? local_var(ctx, n) : alcvar_initial(ctx); \
/**/
#define DEF_ARG_INT(a0, n) \
    if (ac < (n+1)) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TOO_FEW_ARGUMENTS, NULL); \
    } \
    vmvar *a0 = local_var(ctx, n); \
    if (a0->t != VAR_INT64 && a0->t != VAR_BIG) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL); \
    } \
/**/
#define DEF_ARG_OR_UNDEF(a0, n, type) \
    vmvar *a0 = (ac > n) ? local_var(ctx, n) : alcvar_initial(ctx); \
    if (a0->t != type && a0->t != VAR_UNDEF) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL); \
    } \
/**/

#ifndef __MIRC__

#ifndef KILITE_AMALGAMATION
#include "lib/miniz.h"
#include "lib/zip.h"
#endif

#else

#define ZIP_EXPORT

typedef long ssize_t; /* byte count or error */

#define ZIP_DEFAULT_COMPRESSION_LEVEL 6

#define ZIP_ENOINIT -1      // not initialized
#define ZIP_EINVENTNAME -2  // invalid entry name
#define ZIP_ENOENT -3       // entry not found
#define ZIP_EINVMODE -4     // invalid zip mode
#define ZIP_EINVLVL -5      // invalid compression level
#define ZIP_ENOSUP64 -6     // no zip 64 support
#define ZIP_EMEMSET -7      // memset error
#define ZIP_EWRTENT -8      // cannot write data to entry
#define ZIP_ETDEFLINIT -9   // cannot initialize tdefl compressor
#define ZIP_EINVIDX -10     // invalid index
#define ZIP_ENOHDR -11      // header not found
#define ZIP_ETDEFLBUF -12   // cannot flush tdefl buffer
#define ZIP_ECRTHDR -13     // cannot create entry header
#define ZIP_EWRTHDR -14     // cannot write entry header
#define ZIP_EWRTDIR -15     // cannot write to central dir
#define ZIP_EOPNFILE -16    // cannot open file
#define ZIP_EINVENTTYPE -17 // invalid entry type
#define ZIP_EMEMNOALLOC -18 // extracting data using no memory allocation
#define ZIP_ENOFILE -19     // file not found
#define ZIP_ENOPERM -20     // no permission
#define ZIP_EOOMEM -21      // out of memory
#define ZIP_EINVZIPNAME -22 // invalid zip archive name
#define ZIP_EMKDIR -23      // make dir error
#define ZIP_ESYMLINK -24    // symlink error
#define ZIP_ECLSZIP -25     // close archive error
#define ZIP_ECAPSIZE -26    // capacity size too small
#define ZIP_EFSEEK -27      // fseek error
#define ZIP_EFREAD -28      // fread error
#define ZIP_EFWRITE -29     // fwrite error

struct zip_t;

/* Normal interface */
extern ZIP_EXPORT const char *zip_strerror(int errnum);
extern ZIP_EXPORT struct zip_t *zip_open(const char *zipname, int level, char mode);
extern ZIP_EXPORT void zip_close(struct zip_t *zip);
extern ZIP_EXPORT int zip_is64(struct zip_t *zip);
extern ZIP_EXPORT int zip_entry_open(struct zip_t *zip, const char *entryname);
extern ZIP_EXPORT int zip_entry_opencasesensitive(struct zip_t *zip, const char *entryname);
extern ZIP_EXPORT int zip_entry_openbyindex(struct zip_t *zip, size_t index);
extern ZIP_EXPORT int zip_entry_close(struct zip_t *zip);
extern ZIP_EXPORT const char *zip_entry_name(struct zip_t *zip);
extern ZIP_EXPORT ssize_t zip_entry_index(struct zip_t *zip);
extern ZIP_EXPORT int zip_entry_isdir(struct zip_t *zip);
extern ZIP_EXPORT unsigned long long zip_entry_size(struct zip_t *zip);
extern ZIP_EXPORT unsigned long long zip_entry_uncomp_size(struct zip_t *zip);
extern ZIP_EXPORT unsigned long long zip_entry_comp_size(struct zip_t *zip);
extern ZIP_EXPORT unsigned int zip_entry_crc32(struct zip_t *zip);
extern ZIP_EXPORT int zip_entry_write(struct zip_t *zip, const void *buf, size_t bufsize);
extern ZIP_EXPORT int zip_entry_fwrite(struct zip_t *zip, const char *filename);
extern ZIP_EXPORT ssize_t zip_entry_read(struct zip_t *zip, void **buf, size_t *bufsize);
extern ZIP_EXPORT ssize_t zip_entry_noallocread(struct zip_t *zip, void *buf, size_t bufsize);
extern ZIP_EXPORT int zip_entry_fread(struct zip_t *zip, const char *filename);
extern ZIP_EXPORT ssize_t zip_entries_total(struct zip_t *zip);
extern ZIP_EXPORT ssize_t zip_entries_delete(struct zip_t *zip, char *const entries[], size_t len);
extern ZIP_EXPORT struct zip_t *zip_stream_open(const char *stream, size_t size, int level, char mode);
extern ZIP_EXPORT ssize_t zip_stream_copy(struct zip_t *zip, void **buf, size_t *bufsize);
extern ZIP_EXPORT void zip_stream_close(struct zip_t *zip);
extern ZIP_EXPORT int zip_create(const char *zipname, const char *filenames[], size_t len);

/* Use a callback function */
extern ZIP_EXPORT int zip_entry_extract(struct zip_t *zip, size_t (*on_extract)(void *arg, uint64_t offset, const void *data, size_t size), void *arg);
extern ZIP_EXPORT int zip_stream_extract(const char *stream, size_t size, const char *dir, int (*on_extract)(const char *filename, void *arg), void *arg);
extern ZIP_EXPORT int zip_extract(const char *zipname, const char *dir, int (*on_extract_entry)(const char *filename, void *arg), void *arg);

#endif

#endif /* KILITE_TEMPLATE_LIB_H */
