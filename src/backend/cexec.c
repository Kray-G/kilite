#include "cexec.h"
#include "../../mir/c2mir/c2mir.h"
#include "../../mir/mir-gen.h"
#include <stdio.h>
#include <string.h>

#include "../template/inc/platform.c"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

static int gen_executble(char *cmd)
{
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
     si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
 
    DWORD ret = CreateProcess(NULL, cmd, NULL, NULL, TRUE,
        CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (ret == 0) {
        return 1;
    }

    HANDLE h = pi.hProcess;
    CloseHandle(pi.hThread);
    WaitForSingleObject(h, INFINITE);
    DWORD code;
    GetExitCodeProcess(h, &code); 
    CloseHandle(h);

    return code;
}

#else

static int gen_executble(char *cmd)
{
    return system(cmd);
}

#endif

struct data {
    const char *p;
    const char *code;
};

static int getc_func(void *data)
{
    struct data *d = (struct data *)data;
    if (*(d->p)) {
        return *d->p++;
    }
    d->p = d->code;
    d->code = NULL;
    if (d->p && *(d->p)) {
        return *d->p++;
    }
    return EOF;
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
    struct data getc_data = { .code = src, .p = vmheader() };

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
        MIR_load_external(ctx, "printf", printf);
        MIR_load_external(ctx, "sprintf", sprintf);
        MIR_load_external(ctx, "SystemTimer_init", SystemTimer_init);
        MIR_load_external(ctx, "SystemTimer_restart_impl", SystemTimer_restart_impl);
        MIR_load_external(ctx, "SystemTimer_elapsed_impl", SystemTimer_elapsed_impl);
        MIR_load_external(ctx, "Math_random_impl", Math_random_impl);
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

int genexec(char *cmd)
{
    return gen_executble(cmd);
}
