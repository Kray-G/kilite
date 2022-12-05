
#ifndef __MIRC__

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__GNUC__)
#include <stdint.h>

#if defined(_MSC_VER)
#define MATH_SYSTIME_DEFINED
#include <windows.h>
#pragma comment(lib, "advapi32.lib")
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
extern void srand(unsigned);
extern int rand(void);
extern void *calloc(size_t, size_t);
typedef long clock_t;
extern clock_t clock(void);
#define CLOCKS_PER_SEC  ((clock_t)1000)
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
