#include "cexec.h"
#include "../../submodules/mir/c2mir/c2mir.h"
#include "../../submodules/mir/mir-gen.h"
#include <stdio.h>
#include <string.h>

#include "../template/lib/zip.h"
#include "../template/inc/platform.c"
#define timer_init SystemTimer_init
#define timer_restart SystemTimer_restart_impl
#define timer_elapsed SystemTimer_elapsed_impl
#define SHOW_TIMER(msg) { \
    if (opts->cctime) { \
        printf(">> %-25s: %f\n", msg, timer_elapsed(opts->timer)); \
        timer_restart(opts->timer); \
    } \
} \
/**/

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

void load_additional_methods(MIR_context_t ctx)
{
    MIR_load_external(ctx, "printf", (void *)printf);
    MIR_load_external(ctx, "sprintf", (void *)sprintf);
    MIR_load_external(ctx, "SystemTimer_init", (void *)SystemTimer_init);
    MIR_load_external(ctx, "SystemTimer_restart_impl", (void *)SystemTimer_restart_impl);
    MIR_load_external(ctx, "SystemTimer_elapsed_impl", (void *)SystemTimer_elapsed_impl);
    MIR_load_external(ctx, "Math_random_impl", (void *)Math_random_impl);

    MIR_load_external(ctx, "zip_strerror", (void *)zip_strerror);
    MIR_load_external(ctx, "zip_open", (void *)zip_open);
    MIR_load_external(ctx, "zip_close", (void *)zip_close);
    MIR_load_external(ctx, "zip_is64", (void *)zip_is64);
    MIR_load_external(ctx, "zip_entry_open", (void *)zip_entry_open);
    MIR_load_external(ctx, "zip_entry_opencasesensitive", (void *)zip_entry_opencasesensitive);
    MIR_load_external(ctx, "zip_entry_openbyindex", (void *)zip_entry_openbyindex);
    MIR_load_external(ctx, "zip_entry_close", (void *)zip_entry_close);
    MIR_load_external(ctx, "zip_entry_name", (void *)zip_entry_name);
    MIR_load_external(ctx, "zip_entry_index", (void *)zip_entry_index);
    MIR_load_external(ctx, "zip_entry_isdir", (void *)zip_entry_isdir);
    MIR_load_external(ctx, "zip_entry_size", (void *)zip_entry_size);
    MIR_load_external(ctx, "zip_entry_uncomp_size", (void *)zip_entry_uncomp_size);
    MIR_load_external(ctx, "zip_entry_comp_size", (void *)zip_entry_comp_size);
    MIR_load_external(ctx, "zip_entry_crc32", (void *)zip_entry_crc32);
    MIR_load_external(ctx, "zip_entry_write", (void *)zip_entry_write);
    MIR_load_external(ctx, "zip_entry_fwrite", (void *)zip_entry_fwrite);
    MIR_load_external(ctx, "zip_entry_read", (void *)zip_entry_read);
    MIR_load_external(ctx, "zip_entry_noallocread", (void *)zip_entry_noallocread);
    MIR_load_external(ctx, "zip_entry_fread", (void *)zip_entry_fread);
    MIR_load_external(ctx, "zip_entries_total", (void *)zip_entries_total);
    MIR_load_external(ctx, "zip_entries_delete", (void *)zip_entries_delete);
    MIR_load_external(ctx, "zip_stream_open", (void *)zip_stream_open);
    MIR_load_external(ctx, "zip_stream_copy", (void *)zip_stream_copy);
    MIR_load_external(ctx, "zip_stream_close", (void *)zip_stream_close);
    MIR_load_external(ctx, "zip_create", (void *)zip_create);
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
    SHOW_TIMER("Run init");
    if (!c2mir_compile(ctx, &options, getc_func, &getc_data, fname, outf)) {
        fprintf(stderr, "Compile error\n");
        goto END;
    }
    SHOW_TIMER("Compilation from C");

    if (options.asm_p || options.object_p) {
        r = 0;  /* successful */
    } else {
        if (opts->modules) {
            load_additional_modules(ctx, opts->modules);
        }
        load_additional_methods(ctx);
        MIR_item_t main_func = load_main_modules(ctx);
        if (main_func == NULL || main_func->addr == NULL) {
            fprintf(stderr, "Can't find the 'main'\n");
            goto END;
        }
        SHOW_TIMER("Load modules");

        Math_initialize();
        MIR_gen_init(ctx, 1);
        MIR_link(ctx, lazy ? MIR_set_lazy_gen_interface : MIR_set_gen_interface, import_resolver);
        main_t fun_addr = (main_t)(main_func->addr);
        int rc = fun_addr(ac, av, ev);
        MIR_gen_finish(ctx);
        Math_finalize();
        if (ret) *ret = rc;
        r = 0;  /* successful */
        SHOW_TIMER("Run the program");
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
