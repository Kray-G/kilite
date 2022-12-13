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

#define FILE_TEXT   (0x01)
#define FILE_BINARY (0x02)
#define FILE_READ   (0x10)
#define FILE_WRITE  (0x20)
#define FILE_APPEND (0x40)

#ifndef __MIRC__

#include "inc/lib/onig/include/oniguruma.h"

#include "inc/lib/minizip/include/mz.h"
#include "inc/lib/minizip/include/mz_compat.h"
#include "inc/lib/minizip/include/mz_crypt.h"
#include "inc/lib/minizip/include/mz_os.h"
#include "inc/lib/minizip/include/mz_strm.h"
#include "inc/lib/minizip/include/mz_strm_buf.h"
#include "inc/lib/minizip/include/mz_strm_mem.h"
#include "inc/lib/minizip/include/mz_strm_os.h"
#include "inc/lib/minizip/include/mz_strm_pkcrypt.h"
#include "inc/lib/minizip/include/mz_strm_zlib.h"
#include "inc/lib/minizip/include/mz_zip.h"
#include "inc/lib/minizip/include/mz_zip_rw.h"
#include "inc/lib/minizip/include/unzip.h"
#include "inc/lib/minizip/include/zip.h"
#if defined(_WIN32) || defined(_WIN64)
#include "inc/lib/minizip/include/mz_strm_wzaes.h"
#endif

#else

/* oniguruma */

#define ONIG_OPTION_NONE          0U
#define ONIG_OPTION_DEFAULT       ONIG_OPTION_NONE
#define ONIG_ENCODING_UTF8        (&OnigEncodingUTF8)

#define ONIG_NORMAL                0
#define ONIG_VALUE_IS_NOT_SET      1
#define ONIG_MISMATCH             -1
#define ONIG_NO_SUPPORT_CONFIG    -2
#define ONIG_ABORT                -3

#define ONIG_OPTION_NONE                 0U
#define ONIG_OPTION_IGNORECASE           1U
#define ONIG_OPTION_EXTEND               (ONIG_OPTION_IGNORECASE         << 1)
#define ONIG_OPTION_MULTILINE            (ONIG_OPTION_EXTEND             << 1)
#define ONIG_OPTION_SINGLELINE           (ONIG_OPTION_MULTILINE          << 1)
#define ONIG_OPTION_FIND_LONGEST         (ONIG_OPTION_SINGLELINE         << 1)
#define ONIG_OPTION_FIND_NOT_EMPTY       (ONIG_OPTION_FIND_LONGEST       << 1)
#define ONIG_OPTION_NEGATE_SINGLELINE    (ONIG_OPTION_FIND_NOT_EMPTY     << 1)
#define ONIG_OPTION_DONT_CAPTURE_GROUP   (ONIG_OPTION_NEGATE_SINGLELINE  << 1)
#define ONIG_OPTION_CAPTURE_GROUP        (ONIG_OPTION_DONT_CAPTURE_GROUP << 1)

typedef void regex_t;
typedef void OnigRegion;
typedef unsigned char OnigUChar;
typedef unsigned int OnigOptionType;

void onig_free(regex_t *);
OnigRegion* onig_region_new(void);
void onig_region_free(OnigRegion* region, int free_self);
void onig_region_clear(OnigRegion* region);
int onig_search(regex_t *, const OnigUChar* str, const OnigUChar* end, const OnigUChar* start, const OnigUChar* range, OnigRegion* region, OnigOptionType option);
int onig_match(regex_t *, const OnigUChar* str, const OnigUChar* end, const OnigUChar* at, OnigRegion* region, OnigOptionType option);

/* minizip */

typedef void mz_zip_file;
typedef int64_t time_t;

typedef int32_t (*mz_zip_reader_overwrite_cb)(void *handle, void *userdata, mz_zip_file *file_info, const char *path);

void*   mz_zip_reader_create(void **handle);
int32_t mz_zip_reader_open_file(void *handle, const char *path);
void    mz_zip_reader_delete(void **handle);
int32_t mz_zip_reader_goto_first_entry(void *handle);
int32_t mz_zip_reader_goto_next_entry(void *handle);
int32_t mz_zip_reader_locate_entry(void *handle, const char *filename, uint8_t ignore_case);
int32_t mz_zip_reader_entry_get_info(void *handle, mz_zip_file **file_info);
int32_t mz_zip_reader_entry_is_dir(void *handle);
void    mz_zip_reader_set_password(void *handle, const char *password);
int32_t mz_zip_reader_entry_save_buffer_length(void *handle);
int32_t mz_zip_reader_entry_save_buffer(void *handle, void *buf, int32_t len);
void    mz_zip_reader_set_overwrite_cb(void *handle, void *userdata, mz_zip_reader_overwrite_cb cb);
int32_t mz_zip_reader_entry_save_file(void *handle, const char *path);

void*   mz_zip_writer_create(void **handle);
void    mz_zip_writer_delete(void **handle);
int32_t mz_zip_writer_open_file(void *handle, const char *path, int64_t disk_size, uint8_t append);
int32_t mz_zip_writer_add_path(void *handle, const char *path, const char *root_path, uint8_t include_path, uint8_t recursive);
int32_t mz_zip_writer_add_buffer(void *handle, void *buf, int32_t len, mz_zip_file *file_info);
void    mz_zip_writer_set_aes(void *handle, uint8_t aes);
void    mz_zip_writer_set_compress_method(void *handle, uint16_t compress_method);
void    mz_zip_writer_set_compress_level(void *handle, int16_t compress_level);
void    mz_zip_writer_set_password(void *handle, const char *password);

int32_t  mz_os_rename(const char *source_path, const char *target_path);
int32_t  mz_os_unlink(const char *path);
int32_t  mz_os_file_exists(const char *path);
int64_t  mz_os_get_file_size(const char *path);
int32_t  mz_os_get_file_date(const char *path, time_t *modified_date, time_t *accessed_date, time_t *creation_date);
int32_t  mz_os_set_file_date(const char *path, time_t modified_date, time_t accessed_date, time_t creation_date);
int32_t  mz_os_is_dir(const char *path);
int32_t  mz_os_make_dir(const char *path);
int32_t  mz_os_is_symlink(const char *path);
int32_t  mz_os_make_symlink(const char *path, const char *target_path);
int32_t  mz_os_read_symlink(const char *path, char *target_path, int32_t max_target_path);

/* MZ_ERROR */
#define MZ_OK                           (0)  /* zlib */
#define MZ_STREAM_ERROR                 (-1) /* zlib */
#define MZ_DATA_ERROR                   (-3) /* zlib */
#define MZ_MEM_ERROR                    (-4) /* zlib */
#define MZ_BUF_ERROR                    (-5) /* zlib */
#define MZ_VERSION_ERROR                (-6) /* zlib */

#define MZ_END_OF_LIST                  (-100)
#define MZ_END_OF_STREAM                (-101)

#define MZ_PARAM_ERROR                  (-102)
#define MZ_FORMAT_ERROR                 (-103)
#define MZ_INTERNAL_ERROR               (-104)
#define MZ_CRC_ERROR                    (-105)
#define MZ_CRYPT_ERROR                  (-106)
#define MZ_EXIST_ERROR                  (-107)
#define MZ_PASSWORD_ERROR               (-108)
#define MZ_SUPPORT_ERROR                (-109)
#define MZ_HASH_ERROR                   (-110)
#define MZ_OPEN_ERROR                   (-111)
#define MZ_CLOSE_ERROR                  (-112)
#define MZ_SEEK_ERROR                   (-113)
#define MZ_TELL_ERROR                   (-114)
#define MZ_READ_ERROR                   (-115)
#define MZ_WRITE_ERROR                  (-116)
#define MZ_SIGN_ERROR                   (-117)
#define MZ_SYMLINK_ERROR                (-118)

/* MZ_COMPRESS */
#define MZ_COMPRESS_METHOD_STORE        (0)
#define MZ_COMPRESS_METHOD_DEFLATE      (8)
#define MZ_COMPRESS_METHOD_BZIP2        (12)
#define MZ_COMPRESS_METHOD_LZMA         (14)
#define MZ_COMPRESS_METHOD_ZSTD         (93)
#define MZ_COMPRESS_METHOD_XZ           (95)
#define MZ_COMPRESS_METHOD_AES          (99)

/* MZ_ZIP_FLAG */
#define MZ_ZIP_FLAG_ENCRYPTED           (1 << 0)
#define MZ_ZIP_FLAG_LZMA_EOS_MARKER     (1 << 1)
#define MZ_ZIP_FLAG_DEFLATE_MAX         (1 << 1)
#define MZ_ZIP_FLAG_DEFLATE_NORMAL      (0)
#define MZ_ZIP_FLAG_DEFLATE_FAST        (1 << 2)
#define MZ_ZIP_FLAG_DEFLATE_SUPER_FAST  (MZ_ZIP_FLAG_DEFLATE_FAST | \
                                         MZ_ZIP_FLAG_DEFLATE_MAX)
#define MZ_ZIP_FLAG_DATA_DESCRIPTOR     (1 << 3)
#define MZ_ZIP_FLAG_UTF8                (1 << 11)
#define MZ_ZIP_FLAG_MASK_LOCAL_INFO     (1 << 13)

#endif

extern int Regex_onig_new(void *reg, const char *pattern, int option);
extern int Regex_get_region_numregs(OnigRegion *region);
extern int Regex_get_region_beg(OnigRegion *region, int i);
extern int Regex_get_region_end(OnigRegion *region, int i);

#define TIMEINFO_TYPE_MODIFIED (1)
#define TIMEINFO_TYPE_ACCESSED (2)
#define TIMEINFO_TYPE_CREATION (3)

#define MZ_READONLY_ERROR               (-120)
#define MZ_WRITEONLY_ERROR              (-121)
#define MZ_CONTENT_ERROR                (-122)

extern const char *mz_zip_fileinfo_filename(mz_zip_file *file_info);
extern int64_t mz_zip_fileinfo_uncompsize(mz_zip_file *file_info);
extern int64_t mz_zip_fileinfo_compsize(mz_zip_file *file_info);
extern uint32_t mz_zip_fileinfo_crc(mz_zip_file *file_info);
extern int mz_zip_fileinfo_comp_method(mz_zip_file *file_info);
extern int mz_zip_fileinfo_crypt(mz_zip_file *file_info);
extern int mz_zip_fileinfo_aes_bit(mz_zip_file *file_info);
extern int mz_zip_fileinfo_zip64(mz_zip_file *file_info);
extern void mz_zip_timeinfo(mz_zip_file *file_info, int type, int *year, int *mon, int *mday, int *hour, int *min, int *sec);
extern void mz_zip_timeinfo_raw(time_t time, int *year, int *mon, int *mday, int *hour, int *min, int *sec);
extern time_t mz_zip_make_time(int year, int mon, int mday, int hour, int min, int sec);
extern mz_zip_file *mz_alloc_file_info(const char *filename, time_t t, int method, int flag, int aes);
extern void mz_free_file_info(mz_zip_file *file_info);

#endif /* KILITE_TEMPLATE_LIB_H */
