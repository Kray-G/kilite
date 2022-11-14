#include "error.h"
#include <stdio.h>

int error_print(int out, const char *phase, int line, int pos, int len, const char *fmt, va_list ap)
{
    FILE *outf = out ? stdout : stderr;
    fprintf(outf, "Error: ");
    int r = vfprintf(outf, fmt, ap);
    if (line > 0) {
        fprintf(outf, " near the <%s>:%d", phase, line);
    } else {
        fprintf(outf, " in the <%s>", phase);
    }
    fprintf(outf, "\n");
    return r;
}
