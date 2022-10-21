#ifndef KILITE_ERROR_H
#define KILITE_ERROR_H

#include "common.h"
#include <stdarg.h>

int error_print(const char *phase, int line, int pos, int len, const char *fmt, va_list ap);

#endif /* KILITE_ERROR_H */
