#ifndef KILITE_EXEC_H
#define KILITE_EXEC_H

#include "resolver.h"
#include <stdio.h>

typedef struct kl_opts {
    int mir;
    int bmir;
    int lazy;
    const char **modules;
} kl_opts;

typedef int (*main_t)(void);

extern int run_code(int *ret, const char *filename, const char *src, kl_opts *opts);

#endif /* KILITE_EXEC_H */
