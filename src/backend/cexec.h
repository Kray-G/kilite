#ifndef KILITE_EXEC_H
#define KILITE_EXEC_H

#include "resolver.h"
#include "header.h"

typedef struct kl_opts {
    int mir;
    int bmir;
    int lazy_off;
    int out_stdout;
    int cctime;
    void *timer;
    const char *ext;
    const char *bext;
    const char **modules;
} kl_opts;

typedef int (*main_t)(int ac, char **av, char **ev);

extern int run(int *ret, const char *filename, const char *src, int ac, char **av, char **ev, kl_opts *opts);
extern int output(const char *fname, const char *src, int isbmir, const char *ext);

#endif /* KILITE_EXEC_H */
