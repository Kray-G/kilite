#include "frontend/parser.h"
#include "frontend/mkkir.h"
#include "frontend/opt/type.h"
#include "frontend/dispast.h"
#include "backend/cexec.h"
#include "backend/dispkir.h"
#include "backend/translate.h"

#define PROGNAME "kilite"
#define VER_MAJOR "0"
#define VER_MINOR "0"
#define VER_PATCH "1"

#define OPT_ERROR (1)
#define OPT_ERROR_USAGE (2)
#define OPT_PRINT_HELP (3)
#define OPT_DISPLAY_VERSION (4)

#if defined(_WIN32) || defined(_WIN64)
#define alloca _alloca
#define unlink _unlink
#define SEP '\\'
#else
#include <unistd.h>
#define SEP '/'
#endif

extern void *SystemTimer_init(void);
extern void SystemTimer_restart_impl(void *p);
extern double SystemTimer_elapsed_impl(void *p);
#define timer_init SystemTimer_init
#define timer_restart SystemTimer_restart_impl
#define timer_elapsed SystemTimer_elapsed_impl
#define SHOW_TIMER(msg) { \
    if (opts.cctime) { \
        printf(">> %-25s: %f\n", msg, timer_elapsed(ctx->timer)); \
        timer_restart(ctx->timer); \
    } \
} \
/**/

typedef struct kl_argopts {
    int out_bmir;
    int out_mir;
    int out_kir;
    int out_lib;
    int out_csrc;
    int out_cfull;
    int out_ast;
    int out_stdout;
    int in_stdin;
    int out_src;
    int disable_pure;
    int error_stdout;
    int error_limit;
    int print_result;
    int lazy_off;
    int verbose;
    int argstart;
    int cctime;
    int cc;
    const char *ccname;
    const char *ccopt;
    const char *ext;
    const char *file;
} kl_argopts;

typedef struct clcmd {
    int optch;
    char *cc;
    char *opt;
    char *args;
    char *outf;
    char *libpathopt;
    char *libname;
    char *link;
} clcmd;

static clcmd cclist[] = {
    { .cc = "dummy" },
    #if defined(_WIN32) || defined(_WIN64)
    { .optch = '/', .cc = "cl", .opt = "O2", .args = "/MT /nologo", .outf = "/Fe",
        .libpathopt = "/link /LIBPATH:", .link = "kilite.lib" },
    { .optch = '-', .cc = "gcc", .opt = "O3", .args = "", .outf = "-o ",
        .libpathopt = "-L", .link = "-lkilite -lcrypt32" },
    #else
    { .optch = '-', .cc = "gcc", .opt = "O3", .args = "", .outf = "-o ",
        .libpathopt = "-L", .link = "-lkilite -lm" },
    #endif
};

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

static int gen_executble(char *cmd)
{
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
 
    DWORD ret = CreateProcess(NULL, cmd, NULL, NULL, FALSE,
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

char* get_actual_exe_path(void)
{
    static char result[260] = {0};
    if (result[0] == 0) {
        char* p;
        int len = GetModuleFileNameA(NULL, result, 256);
        if (len > 0) {
            p = strrchr(result, '\\');
            if (p) *p = 0;
        }
    }
    return result;
}

#else

static int gen_executble(char *cmd)
{
    return system(cmd);
}

char* get_actual_exe_path(void)
{
    static char result[260] = {0};
    if (result[0] == 0) {
        char* p;
        readlink("/proc/self/exe", result, 256);
        p = strrchr(result, '/');
        if (p) *p = 0;
    }
    return result;
}

#endif

static void version(void)
{
    printf(PROGNAME " version %s.%s.%s\n", VER_MAJOR, VER_MINOR, VER_PATCH);
}

static void usage(void)
{
    printf("Usage: " PROGNAME " -[hvcxSX]\n");
    printf("Main Options:\n");
    printf("    -h                  Display this help.\n");
    printf("    -v, --version       Display the version number.\n");
    printf("    -c                  Output .bmir for library.\n");
    printf("    -x                  Execute the code and print the result.\n");
    printf("    -S                  Output .mir code.\n");
    printf("    -X                  Generate an executable. (need another compiler)\n");
    printf("    --stdout            Change the distination of the output to stdout.\n");
    printf("    --verbose           Show some infrmation when running.\n");
    printf("    --disable-pure      Disable the code optimization for a pure function.\n");
    printf("    --lazy-off          Disable lazy code generation mode.\n");
    printf("    --ext=<ext>         Change the extension of the output file.\n");
    printf("\n");
    printf("Show Process:\n");
    printf("    --cctime            Display various time in compilation.\n");
    printf("    --ast               Output AST.\n");
    printf("    --kir               Output internal temporary compiled code.\n");
    printf("    --csrc              Output the C code of the script code.\n");
    printf("    --cfull             Output the full C code to build the executable.\n");
    printf("\n");
    printf("Compiler Options:\n");
    printf("    --cc=<cc>           Change the compiler to make an executable.\n");
    printf("    --ccopt=<level>     Change the compiler optimization level like 'O3'.\n");
    printf("\n");
    printf("Error Control:\n");
    printf("    --error-stdout      Output error messages to stdout instead of stderr.\n");
    printf("    --error-limit=<n>   Change the limitation of errors. (default: 100)\n");
}

static int parse_long_options_with_iparam(int ac, char **av, int *i, const char *name, int *param)
{
    int len = strlen(name);
    if (strncmp(av[*i], name, len) == 0) {
        if (av[*i][len] == '=') {
            (*param) = strtol(av[*i] + len + 1, NULL, 0);
        } else if (++(*i) < ac) {
            (*param) = strtol(av[*i], NULL, 0);
        }
        return 1;
    }
    return 0;
}

static int parse_long_options_with_sparam(int ac, char **av, int *i, const char *name, const char **param)
{
    int len = strlen(name);
    if (strncmp(av[*i], name, len) == 0) {
        if (av[*i][len] == '=') {
            (*param) = av[*i] + len + 1;
        } else if (++(*i) < ac) {
            (*param) = av[*i];
        }
        return 1;
    }
    return 0;
}

static int parse_long_options(int ac, char **av, int *i, kl_argopts *opts)
{
    if (strcmp(av[*i], "--version") == 0) {
        return OPT_DISPLAY_VERSION;
    } else if (strcmp(av[*i], "--verbose") == 0) {
        opts->verbose = 1;
        opts->cctime = 1;
    } else if (strcmp(av[*i], "--ast") == 0) {
        opts->out_src = 1;
        opts->out_ast = 1;
    } else if (strcmp(av[*i], "--kir") == 0) {
        opts->out_src = 1;
        opts->out_kir = 1;
    } else if (strcmp(av[*i], "--makelib") == 0) {
        opts->out_src = 1;
        opts->out_lib = 1;
        opts->disable_pure = 1;
    } else if (strcmp(av[*i], "--csrc") == 0) {
        opts->out_src = 1;
        opts->out_csrc = 1;
    } else if (strcmp(av[*i], "--cfull") == 0) {
        opts->out_src = 1;
        opts->out_cfull = 1;
    } else if (strcmp(av[*i], "--lazy-off") == 0) {
        opts->lazy_off = 1;
    } else if (strcmp(av[*i], "--stdout") == 0) {
        opts->out_stdout = 1;
    } else if (strcmp(av[*i], "--cctime") == 0) {
        opts->cctime = 1;
    } else if (strcmp(av[*i], "--disable-pure") == 0) {
        opts->disable_pure = 1;
    } else if (strcmp(av[*i], "--error-stdout") == 0) {
        opts->error_stdout = 1;
    } else if (parse_long_options_with_sparam(ac, av, i, "--ccopt", &(opts->ccopt))) {
        return 0;
    } else if (parse_long_options_with_sparam(ac, av, i, "--cc", &(opts->ccname))) {
        opts->cc = 0;
        int count = sizeof(cclist) / sizeof(cclist[0]);
        for (int i = 1; i < count; ++i) {
            if (strcmp(cclist[i].cc, opts->ccname) == 0) {
                opts->cc = i;
                break;
            }
        }
        if (opts->cc == 0) {
            fprintf(stderr, "Error unsupported compiler: %s\n", opts->ccname);
            fprintf(stderr, "    supported: %s", cclist[1].cc);
            for (int i = 2; i < count; ++i) {
                fprintf(stderr, ", %s", cclist[i].cc);
            }
            fprintf(stderr, "\n");
            return OPT_ERROR;
        }
        return 0;
    } else if (parse_long_options_with_iparam(ac, av, i, "--error-limit", &(opts->error_limit))) {
        return 0;
    } else if (parse_long_options_with_sparam(ac, av, i, "--ext", &(opts->ext))) {
        return 0;
    } else {
        fprintf(stderr, "Error unknown option: %s\n", av[*i]);
        return OPT_ERROR_USAGE;
    }
    return 0;
}

static int parse_arg_options(int ac, char **av, kl_argopts *opts)
{
    if (ac < 1) {
        return OPT_ERROR_USAGE;
    }

    int r;
    for (int i = 1; i < ac; ++i) {
        if (av[i][0] == '-') {
            if (av[i][1] == '-') {
                switch (av[i][2]) {
                case '\0':
                    opts->in_stdin = 1;
                    break;
                default:
                    if ((r = parse_long_options(ac, av, &i, opts)) != 0) {
                        return r;
                    }
                    break;
                }
                continue;
            }
            for (int j = 1; av[i][j] > 0; ++j) {
                switch (av[i][j]) {
                case 'S':
                    opts->out_src = 1;
                    break;
                case 'c':
                    opts->out_bmir = 1;
                    break;
                case 'x':
                    opts->print_result = 1;
                    break;
                case 'X':
                    if (!opts->cc) {
                        opts->cc = 1;
                    }
                    break;
                case 'v':
                    return OPT_DISPLAY_VERSION;
                case 'h':
                    return OPT_PRINT_HELP;
                default:
                    fprintf(stderr, "Error unknown option: -%c\n", av[i][j]);
                    return OPT_ERROR_USAGE;
                }
            }
        } else {
            opts->file = av[i];
            opts->argstart = i;
            break;
        }
    }
    if (opts->out_src) {
        opts->out_mir = !(opts->out_ast || opts->out_kir || opts->out_csrc || opts->out_cfull);
    }

    return 0;
}

void output_source(FILE *f, int cfull, int print_result, int verbose, const char *s)
{
    if (cfull) {
        fprintf(f, "#define KILITE_AMALGAMATION\n");
        fprintf(f, "#define _PRINTF_H_\n");
        fprintf(f, "%s", vmheader());
    }
    fprintf(f, "%s\n", s);
    if (cfull) {
        fprintf(f, "void setup_context(vmctx *ctx)\n{\n");
        fprintf(f, "    ctx->print_result = %d;\n", print_result);
        fprintf(f, "    ctx->verbose = %d;\n", verbose);
        fprintf(f, "}\n\n");
        fprintf(f, "void finalize_context(vmctx *ctx)\n{\n");
        fprintf(f, "    finalize(ctx);\n");
        fprintf(f, "}\n\n");
    }
}

int make_executable(kl_argopts *opts, const char *s)
{
    const char *exepath = get_actual_exe_path();
    const char *temppath = getenv("TEMP");
    if (!temppath) {
        temppath = ".";
    }
    int i = 0, j = 0;
    char srcf[128] = {0};
    char name[128] = {0};
    for ( ; i < 120; ++i) {
        int ch = opts->file[i];
        if (ch == 0) {
            break;
        }
        if (ch == SEP) {
            j = i + 1;
        }
    }
    for (i = 0; i < 120; ++i) {
        int ch = opts->file[i+j];
        if (ch == 0) {
            break;
        }
        name[i] = srcf[i] = ch;
        if (ch == '.') {
            ++i;
            break;
        }
    }
    if (i == 0) {
        return 1;
    }

    srcf[i] = 'c';
    #if defined(_WIN32) || defined(_WIN64)
    name[i] = 'e'; name[++i] = 'x'; name[++i] = 'e';
    #else
    name[i-1] = 0;
    #endif

    int tlen = strlen(temppath);
    int fnamelen = (tlen + i) * 2;
    char *fname = (char *)calloc(fnamelen + 2, sizeof(char));
    snprintf(fname, fnamelen, "%s%c%s", temppath, SEP, srcf);

    int elen = strlen(exepath);
    int plen = strlen(cclist[opts->cc].libpathopt);
    int llen = strlen(cclist[opts->cc].link);
    int cmdlen = (elen * 2 + tlen + i + llen + plen) * 3;
    char *cmd = (char *)calloc(cmdlen + 2, sizeof(char));

    FILE *fp = fopen(fname, "w");
    output_source(fp, /* cfull */ 1, /* print_result */ 0, /* verbose */ 0, s);
    fclose(fp);

    int r = 0;
    char ccopt[32] = {0};
    const char *optlevel = opts->ccopt ? opts->ccopt : cclist[opts->cc].opt;
    if (optlevel && optlevel[0] != 0) {
        snprintf(ccopt, 30, "%c%s", cclist[opts->cc].optch, optlevel);
    }

    printf("Temp path: %s\n", temppath);
    #if defined(_WIN32) || defined(_WIN64)
    snprintf(cmd, cmdlen, "%s %s %s %s%s \"%s\\%s\" %s\"%s\" %s",
        cclist[opts->cc].cc, ccopt, cclist[opts->cc].args,
        cclist[opts->cc].outf, name,
        temppath, srcf,
        cclist[opts->cc].libpathopt, exepath,
        cclist[opts->cc].link);
    #else
    snprintf(cmd, cmdlen, "%s %s %s %s%s %s/%s %s%s %s",
        cclist[opts->cc].cc, ccopt, cclist[opts->cc].args, cclist[opts->cc].outf,
        name, temppath, srcf,
        cclist[opts->cc].libpathopt, exepath,
        cclist[opts->cc].link);
    #endif
    printf("Command: %s\n", cmd);
    r = gen_executble(cmd);
    if (r == 0) {
        printf("Creating executable successful: %s\n", name);
    } else {
        printf("Creating executable failed.\n");
    }

    unlink(fname);
    free(cmd);
    free(fname);

    return r;
}

int main(int ac, char **av)
{
    int ri = 1;
    char *s = NULL;
    kl_argopts opts = {0};
    switch (parse_arg_options(ac, av, &opts)) {
    case OPT_ERROR:
        return 1;
    case OPT_ERROR_USAGE:
        usage();
        return 1;
    case OPT_PRINT_HELP:
        usage();
        return 0;
    case OPT_DISPLAY_VERSION:
        version();
        return 0;
    }

    kl_lexer *l = lexer_new_file(opts.in_stdin ? NULL : opts.file);
    l->precode = "let $$;"
                /* `$$` is a program argument passed by a user. */
                /* This must be a 1st variable because compiler is expecting it's an index 0 variable.*/
        "const undefined; const null = undefined;"
        "extern True; extern False;"
        "extern System; extern SystemTimer; extern Math; extern Fiber; extern Range; extern Regex;"
        "extern RuntimeException();"
        "extern Integer; extern Double; extern String; extern Binary; extern Array;"
        "const Object = Array;"
        "extern File; extern XmlDom; extern Zip;"
        "\n";

    kl_context *ctx = parser_new_context();
    l->filename = ctx->filename = opts.file ? opts.file : "stdin";
    if (opts.out_lib) {
        ctx->options |= PARSER_OPT_MAKELIB;
    }
    if (opts.disable_pure) {
        ctx->options |= PARSER_OPT_DISABLE_PURE;
    }
    if (opts.error_stdout) {
        l->error_stdout = 1;
        ctx->options |= PARSER_OPT_ERR_STDOUT;
    }
    if (opts.error_limit > 0) {
        ctx->error_limit = opts.error_limit;
    }

    ctx->timer = timer_init();
    int r = parse(ctx, l);
    SHOW_TIMER("Parsing source code");
    if (r > 0) {
        goto END;
    }
    update_ast_type(ctx);
    if (opts.out_src && opts.out_ast) {
        disp_ast(ctx);
        goto END;
    }
    r = make_kir(ctx);
    SHOW_TIMER("Generating KIR");
    if (r > 0) {
        goto END;
    }
    if (opts.out_src && opts.out_kir) {
        disp_program(ctx->program);
        goto END;
    }
    ctx->program->print_result = opts.print_result;
    ctx->program->verbose = opts.verbose;
    if (opts.out_src && (opts.out_csrc || opts.out_cfull)) {
        s = translate(ctx->program, TRANS_SRC);
        SHOW_TIMER("Translating from KIR to C");
        output_source(stdout, opts.out_cfull, opts.print_result, opts.verbose, s);
        goto END;
    } else if (opts.cc) {
        s = translate(ctx->program, TRANS_SRC);
        SHOW_TIMER("Translating from KIR to C");
        if (make_executable(&opts, s) != 0) {
            fprintf(stderr, "Error: compilation failed, check your compiler path.\n");
        }
        goto END;
    } else if (opts.out_lib) {
        s = translate(ctx->program, TRANS_LIB);
        SHOW_TIMER("Translating from KIR to C");
        printf("%s\n", s);
        goto END;
    }

    s = translate(ctx->program, TRANS_FULL);
    SHOW_TIMER("Translating from KIR to C");
    if (opts.out_mir || opts.out_bmir) {
        ri = output(opts.file, s, opts.out_bmir, (opts.out_stdout || opts.out_lib) ? NULL : (opts.out_mir ? ".mir" : ".bmir"));
        goto END;
    }

    /* run the code */
    if (!opts.out_src) {
        char kilite[384] = {0};
        snprintf(kilite, 380, "%s%ckilite.bmir", get_actual_exe_path(), SEP);
        const char *modules[] = {
            kilite,
            NULL,   /* must be ended by NULL */
        };
        kl_opts runopts = {
            .modules = modules,
            .timer = ctx->timer,
            .cctime = opts.cctime,
            .lazy_off = opts.lazy_off,
        };
        run(&ri, opts.file, s, ac - opts.argstart, av + opts.argstart, NULL, &runopts);
    }

END:
    SHOW_TIMER("Finalization");
    if (s) free(s);
    free_context(ctx);
    lexer_free(l);
    return ri;
}
