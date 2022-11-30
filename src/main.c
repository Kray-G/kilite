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

#define OPT_ERROR_USAGE (1)
#define OPT_PRINT_HELP (2)
#define OPT_DISPLAY_VERSION (3)

typedef struct kl_argopts {
    int out_bmir;
    int out_mir;
    int out_kir;
    int out_lib;
    int out_csrc;
    int out_cfull;
    int out_cdebug;
    int out_ast;
    int out_stdout;
    int in_stdin;
    int out_src;
    int disable_pure;
    int error_stdout;
    int error_limit;
    int print_result;
    int verbose;
    const char *ext;
    const char *file;
} kl_argopts;

static void version(void)
{
    printf(PROGNAME " version %s.%s.%s\n", VER_MAJOR, VER_MINOR, VER_PATCH);
}

static void usage(void)
{
    printf("Usage: " PROGNAME " -[hvcxS]\n");
    printf("Main options:\n");
    printf("    -h                  Display this help.\n");
    printf("    -v, --version       Display the version number.\n");
    printf("    -c                  Output .bmir for library.\n");
    printf("    -x                  Execute the code and print the result.\n");
    printf("    -S                  Output .mir code.\n");
    printf("    --ast               Output AST.\n");
    printf("    --kir               Output low level compiled code.\n");
    printf("    --csrc              Output the C code of the script code.\n");
    printf("    --cfull             Output the full C code to build the executable.\n");
    printf("    --cdebug            Output the full C code to debug the script code.\n");
    printf("    --stdout            Change the distination of the output to stdout.\n");
    printf("    --verbose           Show some infrmation when running.\n");
    printf("    --disable-pure      Disable the code optimization for a pure function.\n");
    printf("    --ext <ext>         Change the extension of the output file.\n");
    printf("    --error-stdout      Output error messages to stdout instead of stderr.\n");
    printf("    --error-limit <n>   Change the limitation of errors. (default: 100)\n");
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
    } else if (strcmp(av[*i], "--cdebug") == 0) {
        opts->out_src = 1;
        opts->out_cdebug = 1;
    } else if (strcmp(av[*i], "--stdout") == 0) {
        opts->out_stdout = 1;
    } else if (strcmp(av[*i], "--disable-pure") == 0) {
        opts->disable_pure = 1;
    } else if (strcmp(av[*i], "--error-stdout") == 0) {
        opts->error_stdout = 1;
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
        }
    }
    if (opts->out_src) {
        opts->out_mir = !(opts->out_ast || opts->out_kir || opts->out_csrc || opts->out_cdebug || opts->out_cfull);
    }

    return 0;
}

int main(int ac, char **av)
{
    int ri = 1;
    char *s = NULL;
    kl_argopts opts = {0};
    switch (parse_arg_options(ac, av, &opts)) {
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
    l->precode = "const undefined; const null = undefined;"
        "extern True; extern False;"
        "extern System; extern SystemTimer; extern Math; extern Fiber; extern Range;"
        "extern RuntimeException();"
        "extern Integer; extern Double; extern String; extern Binary; extern Array;"
        "const Object = Array;"
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

    int r = parse(ctx, l);
    if (r > 0) {
        goto END;
    }
    update_ast_type(ctx);
    if (opts.out_src && opts.out_ast) {
        disp_ast(ctx);
        goto END;
    }
    r = make_kir(ctx);
    if (r > 0) {
        goto END;
    }
    if (opts.out_src && opts.out_kir) {
        disp_program(ctx->program);
        goto END;
    }
    ctx->program->print_result = opts.print_result;
    ctx->program->verbose = opts.verbose;
    if (opts.out_src && (opts.out_csrc || opts.out_cdebug || opts.out_cfull)) {
        s = translate(ctx->program, opts.out_cdebug ? TRANS_DEBUG : TRANS_SRC);
        if (opts.out_cfull) {
            printf("#define _PRINTF_H_\n");
        }
        if (opts.out_cdebug || opts.out_cfull) {
            printf("%s", vmheader());
        }
        printf("%s\n", s);
        if (opts.out_cdebug || opts.out_cfull) {
            printf("void setup_context(vmctx *ctx)\n{\n");
            printf("    ctx->print_result = %d;\n", opts.out_cdebug ? 1 : opts.print_result);
            printf("    ctx->verbose = %d;\n", opts.verbose);
            printf("}\n\n");
            if (opts.out_cdebug) {
                printf("void _putchar(char ch) { putchar(ch); }\n");
            }
            printf("uint32_t Math_random_impl(void) { return 0; }\n");
            printf("void *SystemTimer_init(void) { return NULL; }\n");
            printf("void SystemTimer_restart_impl(void *p) {}\n");
            printf("double SystemTimer_elapsed_impl(void *p) { return 0.0; }\n");
        }
        goto END;
    } else {
        s = translate(ctx->program, opts.out_lib ? TRANS_LIB : TRANS_FULL);
    }

    if (opts.out_lib) {
        printf("%s\n", s);
        goto END;
    }
    if (opts.out_mir || opts.out_bmir) {
        ri = output(opts.file, s, opts.out_bmir, (opts.out_stdout || opts.out_lib) ? NULL : (opts.out_mir ? ".mir" : ".bmir"));
        goto END;
    }

    /* run the code */
    if (!opts.out_src) {
        const char *modules[] = {
            "kilite.bmir",
            NULL,   /* must be ended by NULL */
        };
        kl_opts runopts = {
            .modules = modules,
        };
        run(&ri, opts.file, s, 0, NULL, NULL, &runopts);
    }

END:
    if (s) free(s);
    free_context(ctx);
    lexer_free(l);
    return ri;
}
