#include "cexec.h"
#include "../../submodules/mir/c2mir/c2mir.h"
#include "../../submodules/mir/mir-gen.h"
#include <stdio.h>
#include <string.h>

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
    MIR_load_external(ctx, "get_stdio_stdin", (void *)get_stdio_stdin);
    MIR_load_external(ctx, "get_stdio_stdout", (void *)get_stdio_stdout);
    MIR_load_external(ctx, "get_stdio_stderr", (void *)get_stdio_stderr);
    MIR_load_external(ctx, "printf", (void *)printf);
    MIR_load_external(ctx, "fprintf", (void *)fprintf);
    MIR_load_external(ctx, "sprintf", (void *)sprintf);
    MIR_load_external(ctx, "fopen", (void *)fopen);
    MIR_load_external(ctx, "fclose", (void *)fclose);
    MIR_load_external(ctx, "SystemTimer_init", (void *)SystemTimer_init);
    MIR_load_external(ctx, "SystemTimer_restart_impl", (void *)SystemTimer_restart_impl);
    MIR_load_external(ctx, "SystemTimer_elapsed_impl", (void *)SystemTimer_elapsed_impl);
    MIR_load_external(ctx, "Math_random_impl", (void *)Math_random_impl);
    MIR_load_external(ctx, "Math_initialize", (void *)Math_initialize);
    MIR_load_external(ctx, "Math_finalize", (void *)Math_finalize);
    MIR_load_external(ctx, "Regex_initialize", (void *)Regex_initialize);
    MIR_load_external(ctx, "Regex_finalize", (void *)Regex_finalize);

    /* Regex */
    MIR_load_external(ctx, "onig_initialize", (void *)onig_initialize);
    MIR_load_external(ctx, "onig_new", (void *)onig_new);
    MIR_load_external(ctx, "onig_free", (void *)onig_free);
    MIR_load_external(ctx, "onig_region_new", (void *)onig_region_new);
    MIR_load_external(ctx, "onig_region_free", (void *)onig_region_free);
    MIR_load_external(ctx, "onig_region_clear", (void *)onig_region_clear);
    MIR_load_external(ctx, "onig_search", (void *)onig_search);
    MIR_load_external(ctx, "onig_match", (void *)onig_match);

    MIR_load_external(ctx, "Regex_onig_new", (void *)Regex_onig_new);
    MIR_load_external(ctx, "Regex_get_region_numregs", (void *)Regex_get_region_numregs);
    MIR_load_external(ctx, "Regex_get_region_beg", (void *)Regex_get_region_beg);
    MIR_load_external(ctx, "Regex_get_region_end", (void *)Regex_get_region_end);

    /* File/Zip */
    MIR_load_external(ctx, "mz_os_rename", (void *)mz_os_rename);
    MIR_load_external(ctx, "mz_os_unlink", (void *)mz_os_unlink);
    MIR_load_external(ctx, "mz_os_file_exists", (void *)mz_os_file_exists);
    MIR_load_external(ctx, "mz_os_get_file_size", (void *)mz_os_get_file_size);
    MIR_load_external(ctx, "mz_os_get_file_date", (void *)mz_os_get_file_date);
    MIR_load_external(ctx, "mz_os_set_file_date", (void *)mz_os_set_file_date);
    MIR_load_external(ctx, "mz_os_is_dir", (void *)mz_os_is_dir);
    MIR_load_external(ctx, "mz_os_make_dir", (void *)mz_os_make_dir);
    MIR_load_external(ctx, "mz_os_is_symlink", (void *)mz_os_is_symlink);
    MIR_load_external(ctx, "mz_os_make_symlink", (void *)mz_os_make_symlink);
    MIR_load_external(ctx, "mz_os_read_symlink", (void *)mz_os_read_symlink);

    MIR_load_external(ctx, "mz_zip_reader_create", (void *)mz_zip_reader_create);
    MIR_load_external(ctx, "mz_zip_reader_open_file", (void *)mz_zip_reader_open_file);
    MIR_load_external(ctx, "mz_zip_reader_delete", (void *)mz_zip_reader_delete);
    MIR_load_external(ctx, "mz_zip_reader_goto_first_entry", (void *)mz_zip_reader_goto_first_entry);
    MIR_load_external(ctx, "mz_zip_reader_goto_next_entry", (void *)mz_zip_reader_goto_next_entry);
    MIR_load_external(ctx, "mz_zip_reader_locate_entry", (void *)mz_zip_reader_locate_entry);
    MIR_load_external(ctx, "mz_zip_reader_entry_get_info", (void *)mz_zip_reader_entry_get_info);
    MIR_load_external(ctx, "mz_zip_reader_entry_is_dir", (void *)mz_zip_reader_entry_is_dir);
    MIR_load_external(ctx, "mz_zip_reader_set_password", (void *)mz_zip_reader_set_password);
    MIR_load_external(ctx, "mz_zip_reader_entry_save_buffer_length", (void *)mz_zip_reader_entry_save_buffer_length);
    MIR_load_external(ctx, "mz_zip_reader_entry_save_buffer", (void *)mz_zip_reader_entry_save_buffer);
    MIR_load_external(ctx, "mz_zip_reader_set_overwrite_cb", (void *)mz_zip_reader_set_overwrite_cb);
    MIR_load_external(ctx, "mz_zip_reader_entry_save_file", (void *)mz_zip_reader_entry_save_file);

    MIR_load_external(ctx, "mz_zip_writer_create", (void *)mz_zip_writer_create);
    MIR_load_external(ctx, "mz_zip_writer_delete", (void *)mz_zip_writer_delete);
    MIR_load_external(ctx, "mz_zip_writer_open_file", (void *)mz_zip_writer_open_file);
    MIR_load_external(ctx, "mz_zip_writer_add_path", (void *)mz_zip_writer_add_path);
    MIR_load_external(ctx, "mz_zip_writer_add_buffer", (void *)mz_zip_writer_add_buffer);
    MIR_load_external(ctx, "mz_zip_writer_set_aes", (void *)mz_zip_writer_set_aes);
    MIR_load_external(ctx, "mz_zip_writer_set_compress_method", (void *)mz_zip_writer_set_compress_method);
    MIR_load_external(ctx, "mz_zip_writer_set_compress_level", (void *)mz_zip_writer_set_compress_level);
    MIR_load_external(ctx, "mz_zip_writer_set_password", (void *)mz_zip_writer_set_password);

    MIR_load_external(ctx, "mz_zip_fileinfo_filename", (void *)mz_zip_fileinfo_filename);
    MIR_load_external(ctx, "mz_zip_fileinfo_uncompsize", (void *)mz_zip_fileinfo_uncompsize);
    MIR_load_external(ctx, "mz_zip_fileinfo_compsize", (void *)mz_zip_fileinfo_compsize);
    MIR_load_external(ctx, "mz_zip_fileinfo_crc", (void *)mz_zip_fileinfo_crc);
    MIR_load_external(ctx, "mz_zip_fileinfo_comp_method", (void *)mz_zip_fileinfo_comp_method);
    MIR_load_external(ctx, "mz_zip_fileinfo_crypt", (void *)mz_zip_fileinfo_crypt);
    MIR_load_external(ctx, "mz_zip_fileinfo_zip64", (void *)mz_zip_fileinfo_zip64);
    MIR_load_external(ctx, "mz_zip_fileinfo_aes_bit", (void *)mz_zip_fileinfo_aes_bit);
    MIR_load_external(ctx, "mz_zip_timeinfo", (void *)mz_zip_timeinfo);
    MIR_load_external(ctx, "mz_zip_timeinfo_raw", (void *)mz_zip_timeinfo_raw);
    MIR_load_external(ctx, "mz_zip_make_time", (void *)mz_zip_make_time);
    MIR_load_external(ctx, "mz_alloc_file_info", (void *)mz_alloc_file_info);
    MIR_load_external(ctx, "mz_free_file_info", (void *)mz_free_file_info);
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
        if (opts->lazy_off) {
            lazy = 0;
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

        MIR_gen_init(ctx, 1);
        MIR_link(ctx, lazy ? MIR_set_lazy_gen_interface : MIR_set_gen_interface, import_resolver);
        SHOW_TIMER("Link modules");
        main_t fun_addr = (main_t)(main_func->addr);
        int rc = fun_addr(ac, av, ev);
        SHOW_TIMER("Run the program");
        MIR_gen_finish(ctx);
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
