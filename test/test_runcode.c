#include "../src/backend/cexec.h"

// cl -I ../mir -O2 test_runcode.c ..\src\backend\cexec.c ..\src\backend\resolver.c ..\bin\mir_static.lib
// gcc -I ../mir -O2 test_runcode.c ../src/backend/cexec.c ../src/backend/resolver.c ../bin/mir_static.a

int main(void)
{
    const char *modules[] = {
        "fib.bmir",
        NULL,   /* must be ended by NULL */
    };
    kl_opts opts = {
        .mir = 0,
        .bmir = 0,
        .lazy = 1,
        .modules = modules,
    };
    int r = 0;
    return run_code(&r, "fib.c",
            // "int fib(int n)\n"
            // "{\n"
            // "    if (n < 2) return n;\n"
            // "    return fib(n-2) + fib(n-1);\n"
            // "}\n"
            "int printf(const char *, ...);\n"
            "int main(void)\n"
            "{\n"
            "   printf(\"okay! fib(38) = %lld\\n\", fib(38));\n"
            "   return 100;\n"
            "}\n"
            "\n",
        0, NULL, NULL,
        &opts
    );
}
