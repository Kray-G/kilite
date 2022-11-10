#include "cexec.h"
#include "../../mir/c2mir/c2mir.h"
#include "../../mir/mir-gen.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
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

struct data {
    const char *p;
};

static int getc_func(void *data)
{
    struct data *d = (struct data *)data;
    if (!*d->p) return EOF;
    return *d->p++;
}

static FILE *open_output_file(const char *fname, kl_opts *opts, int ismir)
{
    if (opts->out_stdout && ismir) {
        return stdout;
    }
    char buf[256] = {0};
    char *p = buf;
    for (int i = 0; i < 240; ++i) {
        if (*fname && *fname != '.') {
            *p++ = *fname++;
        }
    }
    const char *ext = ismir ? (opts->ext ? opts->ext : ".mir") : (opts->bext ? opts->bext : ".bmir");
    if (*ext != '.') {
        *p++ = '.';
    }
    strcpy(p, ext);
    return fopen(buf, ismir ? "w" : "wb");
}

static void load_additional_modules(MIR_context_t ctx, const char **modules)
{
    while (*modules) {
        FILE *f = fopen(*modules, "rb");
        if (f) {
            MIR_read(ctx, f);
            fclose(f);
        }
        ++modules;
    }
}

static MIR_item_t load_main_modules(MIR_context_t ctx)
{
    MIR_item_t main_func = NULL;
    for (MIR_module_t module = DLIST_HEAD(MIR_module_t, *MIR_get_module_list(ctx));
            module != NULL;
            module = DLIST_NEXT(MIR_module_t, module)) {
        for (MIR_item_t func = DLIST_HEAD(MIR_item_t, module->items);
                func != NULL;
                func = DLIST_NEXT(MIR_item_t, func)) {
            if (func->item_type == MIR_func_item && strcmp(func->u.func->name, "main") == 0) {
                main_func = func;
            }
        }
        MIR_load_module(ctx, module);
    }
    return main_func;
}

int run(int *ret, const char *fname, const char *src, int ac, char **av, char **ev, kl_opts *opts)
{
    open_std_libs();
    int r = 1, lazy = 1;
    MIR_context_t ctx = MIR_init();
    struct c2mir_options options = {0};
    struct data getc_data = { .p = src };

    // options.verbose_p = 1;
    options.message_file = stderr;

    FILE *outf = NULL;
    if (opts) {
        if (opts->mir || opts->bmir) {
            options.asm_p = opts->mir;
            options.object_p = opts->bmir;
            outf = open_output_file(fname, opts, options.asm_p);
        }
        if (opts->lazy) {
            lazy = opts->lazy;
        }
    }
    c2mir_init(ctx);
    if (!c2mir_compile(ctx, &options, getc_func, &getc_data, fname, outf)) {
        printf("Compile error\n");
        goto END;
    }

    if (options.asm_p || options.object_p) {
        r = 0;  /* successful */
    } else {
        if (opts->modules) {
            load_additional_modules(ctx, opts->modules);
        }
        MIR_load_external(ctx, "SystemTimer_init", SystemTimer_init);
        MIR_load_external(ctx, "SystemTimer_restart_impl", SystemTimer_restart_impl);
        MIR_load_external(ctx, "SystemTimer_elapsed_impl", SystemTimer_elapsed_impl);
        MIR_load_external(ctx, "Math_random_impl", Math_random_impl);
        MIR_load_external(ctx, "_putchar", putchar);
        MIR_item_t main_func = load_main_modules(ctx);
        if (main_func == NULL || main_func->addr == NULL) {
            printf("Can't find the 'main'\n");
            goto END;
        }

        Math_initialize();
        MIR_gen_init(ctx, 1);
        MIR_link(ctx, lazy ? MIR_set_lazy_gen_interface : MIR_set_gen_interface, import_resolver);
        main_t fun_addr = (main_t)(main_func->addr);
        int rc = fun_addr(ac, av, ev);
        MIR_gen_finish(ctx);
        Math_finalize();
        if (ret) *ret = rc;
        r = 0;  /* successful */
    }

END:
    if (outf) fclose(outf);
    c2mir_finish(ctx);
    MIR_finish(ctx);
    close_std_libs();
    return r;
}

int output(const char *fname, const char *src, int isbmir, const char *ext)
{
    kl_opts opts = {0};
    if (isbmir) {
        opts.bmir = 1;
        opts.bext = ext;
    } else {
        opts.mir = 1;
        opts.ext = ext;
        if (!ext) {
            opts.out_stdout = 1;
        }
    }
    return run(NULL, fname, src, 0, NULL, NULL, &opts);
}
