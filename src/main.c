#include "frontend/parser.h"
#include "frontend/mkkir.h"
#include "frontend/dispast.h"
#include "backend/cexec.h"
#include "backend/dispkir.h"
#include "backend/translate.h"

typedef struct kl_argopts {
    int out_bmir;
    int out_mir;
    int out_kir;
    int out_csrc;
    int out_cfull;
    int out_ast;
    int out_stdout;
    int in_stdin;
    int out_src;
    int verbose;
    const char *ext;
    const char *file;
} kl_argopts;

void parse_arg_options(int ac, char **av, kl_argopts *opts)
{
    for (int i = 1; i < ac; ++i) {
        if (av[i][0] == '-') {
            switch (av[i][1]) {
            case '-':
                switch (av[i][2]) {
                case '\0':
                    opts->in_stdin = 1;
                    break;
                default:
                    if (strcmp(av[i], "--ast") == 0) {
                        opts->out_ast = 1;
                    } else if (strcmp(av[i], "--kir") == 0) {
                        opts->out_kir = 1;
                    } else if (strcmp(av[i], "--csrc") == 0) {
                        opts->out_csrc = 1;
                    } else if (strcmp(av[i], "--cfull") == 0) {
                        opts->out_cfull = 1;
                    } else if (strcmp(av[i], "--stdout") == 0) {
                        opts->out_stdout = 1;
                    } else if (strcmp(av[i], "--verbose") == 0) {
                        opts->verbose = 1;
                    } else if (strcmp(av[i], "--ext") == 0) {
                        if (++i < ac) {
                            opts->ext = av[i];
                        }
                    }
                    break;
                }
                break;
            case 'S':
                opts->out_src = 1;
                break;
            case 'c':
                opts->out_bmir = 1;
                break;
            }
        } else {
            opts->file = av[i];
        }
    }
    if (opts->out_src) {
        opts->out_mir = !(opts->out_ast || opts->out_kir || opts->out_csrc);
    }
}

int main(int ac, char **av)
{
    int ri = 1;
    char *s = NULL;
    kl_argopts opts = {0};
    parse_arg_options(ac, av, &opts);

    kl_lexer *l = lexer_new_file(opts.in_stdin ? NULL : opts.file);
    kl_context *ctx = parser_new_context();

    int r = parse(ctx, l);
    if (r > 0) {
        goto END;
    }
    if (opts.out_src && opts.out_ast) {
        disp_ast(ctx);
    }
    make_kir(ctx);
    if (opts.out_src && opts.out_kir) {
        disp_program(ctx->program);
    }
    ctx->program->verbose = opts.verbose;
    if (opts.out_src && (opts.out_csrc || opts.out_cfull)) {
        s = translate(ctx->program, opts.out_cfull ? TRANS_FULL : TRANS_SRC);
        printf("%s\n", s);
    } else {
        s = translate(ctx->program, TRANS_FULL);
    }
    if (opts.out_mir || opts.out_bmir) {
        ri = output(opts.file, s, opts.out_bmir, opts.out_stdout ? NULL : (opts.out_mir ? ".mir" : ".bmir"));
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
