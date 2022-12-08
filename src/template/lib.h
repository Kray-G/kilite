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

#include "inc/lib/minizip/include/mz.h"
#include "inc/lib/minizip/include/mz_compat.h"
#include "inc/lib/minizip/include/mz_crypt.h"
#include "inc/lib/minizip/include/mz_os.h"
#include "inc/lib/minizip/include/mz_strm.h"
#include "inc/lib/minizip/include/mz_strm_buf.h"
#include "inc/lib/minizip/include/mz_strm_mem.h"
#include "inc/lib/minizip/include/mz_strm_os.h"
#include "inc/lib/minizip/include/mz_strm_pkcrypt.h"
#include "inc/lib/minizip/include/mz_strm_wzaes.h"
#include "inc/lib/minizip/include/mz_strm_zlib.h"
#include "inc/lib/minizip/include/mz_zip.h"
#include "inc/lib/minizip/include/mz_zip_rw.h"
#include "inc/lib/minizip/include/unzip.h"
#include "inc/lib/minizip/include/zip.h"

#else

typedef void mz_zip_file;

void*   mz_zip_reader_create(void **handle);
int32_t mz_zip_reader_open_file(void *handle, const char *path);
void    mz_zip_reader_delete(void **handle);
int32_t mz_zip_reader_goto_first_entry(void *handle);
int32_t mz_zip_reader_goto_next_entry(void *handle);
int32_t mz_zip_reader_locate_entry(void *handle, const char *filename, uint8_t ignore_case);
int32_t mz_zip_reader_entry_get_info(void *handle, mz_zip_file **file_info);
int32_t mz_zip_reader_entry_is_dir(void *handle);
void    mz_zip_reader_set_password(void *handle, const char *password);

#define MZ_OK                           (0)  /* zlib */
#define MZ_STREAM_ERROR                 (-1) /* zlib */
#define MZ_DATA_ERROR                   (-3) /* zlib */
#define MZ_MEM_ERROR                    (-4) /* zlib */
#define MZ_BUF_ERROR                    (-5) /* zlib */
#define MZ_VERSION_ERROR                (-6) /* zlib */

#define MZ_END_OF_LIST                  (-100)
/* MZ_COMPRESS */
#define MZ_COMPRESS_METHOD_STORE        (0)
#define MZ_COMPRESS_METHOD_DEFLATE      (8)
#define MZ_COMPRESS_METHOD_BZIP2        (12)
#define MZ_COMPRESS_METHOD_LZMA         (14)
#define MZ_COMPRESS_METHOD_ZSTD         (93)
#define MZ_COMPRESS_METHOD_XZ           (95)
#define MZ_COMPRESS_METHOD_AES          (99)

#endif

#define TIMEINFO_TYPE_MODIFIED (1)
#define TIMEINFO_TYPE_ACCESSED (2)
#define TIMEINFO_TYPE_CREATION (3)

const char *mz_zip_fileinfo_filename(mz_zip_file *file_info);
int64_t mz_zip_fileinfo_uncompsize(mz_zip_file *file_info);
int64_t mz_zip_fileinfo_compsize(mz_zip_file *file_info);
uint32_t mz_zip_fileinfo_crc(mz_zip_file *file_info);
int mz_zip_fileinfo_comp_method(mz_zip_file *file_info);
int mz_zip_fileinfo_crypt(mz_zip_file *file_info);
void mz_zip_timeinfo(mz_zip_file *file_info, int type, int *year, int *mon, int *mday, int *hour, int *min, int *sec);

#endif /* KILITE_TEMPLATE_LIB_H */
