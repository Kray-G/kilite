
#if defined(_WIN32) || defined(_WIN64)

#include <stdint.h>
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

#else

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
FILE* gmathh = NULL;

typedef struct timer_ {
    struct timeval s;
} systemtimer_t;

void Math_initialize(void)
{
    gmathh = fopen("/dev/urandom", "rb");
}

void Math_finalize(void)
{
    if (gmathh)
        fclose(gmathh);
}

uint32_t Math_random_impl(void)
{
    uint32_t result;
    fread(&result, sizeof(result), 1, gmathh);
    return result;
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
