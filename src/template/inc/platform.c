
#ifndef __MIRC__

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__GNUC__)
#include <stdint.h>

#if defined(_MSC_VER)
#define MATH_SYSTIME_DEFINED
#include <stdio.h>
#include <windows.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")
HCRYPTPROV gmathh = (HCRYPTPROV)NULL;

typedef struct systemtimer_t {
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
} systemtimer_t;

void Math_initialize(void)
{
    if (!CryptAcquireContext(&gmathh,
                                NULL,
                                NULL,
                                PROV_RSA_FULL,
                                CRYPT_SILENT)) {
        if (GetLastError() != (DWORD)NTE_BAD_KEYSET) {
            gmathh = (HCRYPTPROV)NULL;
            return;
        }
        if (!CryptAcquireContext(&gmathh,
                                    NULL,
                                    NULL,
                                    PROV_RSA_FULL,
                                    CRYPT_SILENT | CRYPT_NEWKEYSET)) {
            gmathh = (HCRYPTPROV)NULL;
            return;
        }
    }
}

void Math_finalize(void)
{
    if (gmathh)
        CryptReleaseContext(gmathh, 0);
}

uint32_t Math_random_impl(void)
{
    uint32_t result;
    CryptGenRandom(gmathh, sizeof(result), (BYTE*)&result);
    return result;
}

void *SystemTimer_init(void)
{
    systemtimer_t *v = (systemtimer_t *)calloc(1, sizeof(systemtimer_t));
    QueryPerformanceFrequency(&(v->freq));
    QueryPerformanceCounter(&(v->start));
    return v;
}

void SystemTimer_restart_impl(void *p)
{
    systemtimer_t *v = (systemtimer_t *)p;
    QueryPerformanceCounter(&(v->start));
}

double SystemTimer_elapsed_impl(void *p)
{
    systemtimer_t *v = (systemtimer_t *)p;
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    return (double)(end.QuadPart - (v->start).QuadPart) / (v->freq).QuadPart;
}
#endif

#else
#define MATH_SYSTIME_DEFINED

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#if defined(_WIN32) || defined(_WIN64)
#else
static FILE* gmathh = NULL;
#endif

typedef struct timer_ {
    struct timeval s;
} systemtimer_t;

void Math_initialize(void)
{
    #if defined(_WIN32) || defined(_WIN64)
    srand((unsigned int)time(NULL));
    #else
    gmathh = fopen("/dev/urandom", "rb");
    #endif
}

void Math_finalize(void)
{
    #if defined(_WIN32) || defined(_WIN64)
    #else
    if (gmathh)
        fclose(gmathh);
    #endif
}

uint32_t Math_random_impl(void)
{
    #if defined(_WIN32) || defined(_WIN64)
    int c = rand() % 16;
    uint32_t result = 1;
    while (c--) result *= rand();
    return result;
    #else
    uint32_t result;
    fread(&result, sizeof(result), 1, gmathh);
    return result;
    #endif
}

void *SystemTimer_init(void)
{
    systemtimer_t *v = (systemtimer_t *)calloc(1, sizeof(systemtimer_t));
    gettimeofday(&(v->s), NULL);
    return v;
}

void SystemTimer_restart_impl(void *p)
{
    systemtimer_t *v = (systemtimer_t *)p;
    gettimeofday(&(v->s), NULL);
}

double SystemTimer_elapsed_impl(void *p)
{
    systemtimer_t *v = (systemtimer_t *)p;
    struct timeval e;
    gettimeofday(&e, NULL);
    return (e.tv_sec - (v->s).tv_sec) + (e.tv_usec - (v->s).tv_usec) * 1.0e-6;
}

#endif

#endif  /* !__MIRC__ */

#if !defined(MATH_SYSTIME_DEFINED)

#include <stdint.h>
#include <stddef.h>

#ifndef __MIRC__
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#else
#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC ((clock_t)1000)
#endif
extern void srand(unsigned);
extern int rand(void);
extern void *calloc(size_t, size_t);
typedef long clock_t;
extern clock_t clock(void);
#endif

typedef struct timer_ {
    clock_t s;
} systemtimer_t;

void Math_initialize(void)
{
    srand((unsigned int)time(NULL));
}

void Math_finalize(void)
{
}

uint32_t Math_random_impl(void)
{
    int c = rand() % 16;
    uint32_t result = 1;
    while (c--) result *= rand();
    return result;
}

void *SystemTimer_init(void)
{
    systemtimer_t *v = (systemtimer_t *)calloc(1, sizeof(systemtimer_t));
    v->s = clock();
    return v;
}

void SystemTimer_restart_impl(void *p)
{
    systemtimer_t *v = (systemtimer_t *)p;
    v->s = clock();
}

double SystemTimer_elapsed_impl(void *p)
{
    systemtimer_t *v = (systemtimer_t *)p;
    clock_t e = clock();
    return (double)(e - (v->s)) / CLOCKS_PER_SEC;
}

#endif

#ifndef __MIRC__
#include "../lib.h"

void Regex_initialize(void)
{
    OnigEncoding use_encs[] = { ONIG_ENCODING_UTF8 };
    onig_initialize(use_encs, sizeof(use_encs)/sizeof(use_encs[0]));
}

void Regex_finalize(void)
{
    onig_end();
}

int Regex_onig_new(void *reg, const char *pattern, int option)
{
    return onig_new(reg, pattern, pattern + strlen((char*)pattern), option, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT, NULL);
}

int Regex_get_region_numregs(OnigRegion *region)
{
    return region->num_regs;
}

int Regex_get_region_beg(OnigRegion *region, int i)
{
    return region->beg[i];
}

int Regex_get_region_end(OnigRegion *region, int i)
{
    return region->end[i];
}

const char *mz_zip_fileinfo_filename(mz_zip_file *file_info)
{
    return file_info->filename;
}

int64_t mz_zip_fileinfo_uncompsize(mz_zip_file *file_info)
{
    return file_info->uncompressed_size;
}

int64_t mz_zip_fileinfo_compsize(mz_zip_file *file_info)
{
    return file_info->compressed_size;
}

uint32_t mz_zip_fileinfo_crc(mz_zip_file *file_info)
{
    return file_info->crc;
}

int mz_zip_fileinfo_comp_method(mz_zip_file *file_info)
{
    return file_info->compression_method;
}

int mz_zip_fileinfo_zip64(mz_zip_file *file_info)
{
    return file_info->zip64;
}

int mz_zip_fileinfo_crypt(mz_zip_file *file_info)
{
    return file_info->flag & MZ_ZIP_FLAG_ENCRYPTED;
}

int mz_zip_fileinfo_aes_bit(mz_zip_file *file_info)
{
    switch (file_info->aes_encryption_mode) {
    case MZ_AES_ENCRYPTION_MODE_128:
        return 128;
    case MZ_AES_ENCRYPTION_MODE_192:
        return 192;
    case MZ_AES_ENCRYPTION_MODE_256:
        return 256;
    }
    return 0;
}

void mz_zip_timeinfo(mz_zip_file *file_info, int type, int *year, int *mon, int *mday, int *hour, int *min, int *sec)
{
    struct tm t = {0};
    switch (type) {
    case TIMEINFO_TYPE_MODIFIED:
        mz_zip_time_t_to_tm(file_info->modified_date, &t);
        break;
    case TIMEINFO_TYPE_ACCESSED:
        mz_zip_time_t_to_tm(file_info->accessed_date, &t);
        break;
    case TIMEINFO_TYPE_CREATION:
        mz_zip_time_t_to_tm(file_info->creation_date, &t);
        break;
    default:
        return;
    }
    *year = t.tm_year + 1900;
    *mon  = t.tm_mon;
    *mday = t.tm_mday;
    *hour = t.tm_hour;
    *min  = t.tm_min;
    *sec  = t.tm_sec;
}

time_t mz_zip_make_time(int year, int mon, int mday, int hour, int min, int sec)
{
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon  = mon;
    t.tm_mday = mday;
    t.tm_hour = hour;
    t.tm_min  = min;
    t.tm_sec  = sec;
    return mktime(&t);
}

void mz_zip_timeinfo_raw(time_t time, int *year, int *mon, int *mday, int *hour, int *min, int *sec)
{
    struct tm t = {0};
    mz_zip_time_t_to_tm(time, &t);
    *year = t.tm_year + 1900;
    *mon  = t.tm_mon;
    *mday = t.tm_mday;
    *hour = t.tm_hour;
    *min  = t.tm_min;
    *sec  = t.tm_sec;
}

mz_zip_file *mz_alloc_file_info(const char *filename, int64_t t, int method, int flag, int aes)
{
    mz_zip_file *file_info = (mz_zip_file *)calloc(1, sizeof(mz_zip_file));
    file_info->filename = filename;
    file_info->modified_date = (time_t)t;
    file_info->version_madeby = MZ_VERSION_MADEBY;
    file_info->compression_method = method;
    file_info->flag = flag;
    if (aes) {
        file_info->aes_version = MZ_AES_VERSION;
    }
    return file_info;
}

void mz_free_file_info(mz_zip_file *file_info)
{
    free(file_info);
}

FILE *get_stdio_stdin(void)
{
    return stdin;
}

FILE *get_stdio_stdout(void)
{
    return stdout;
}

FILE *get_stdio_stderr(void)
{
    return stderr;
}

#endif
