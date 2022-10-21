#include "error.h"
#include <stdio.h>

int error_print(const char *phase, int line, int pos, int len, const char *fmt, va_list ap)
{
    if (line > 0) {
        fprintf(stderr, "[%s,%d:%d-%d] ", phase, line, pos, pos+len);
    } else {
        fprintf(stderr, "[%s] ", phase);
    }
    int r = vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    return r;
}
