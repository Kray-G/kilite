#ifndef KILITE_ERROR_H
#define KILITE_ERROR_H

#include <stdarg.h>

extern int error_print(int out, const char *phase, int line, int pos, int len, const char *fmt, va_list ap);

#endif /* KILITE_ERROR_H */
