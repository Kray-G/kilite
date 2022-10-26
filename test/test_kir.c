#include "../src/frontend/parser.h"
#include "../src/frontend/mkkir.h"
#include "../src/frontend/dispkir.h"

// cl test_kir.c ..\src\frontend\lexer.c ..\src\frontend\parser.c ..\src\frontend\error.c ..\src\frontend\dispkir.c ..\src\frontend\mkkir.c
// gcc test_kir.c ../src/frontend/lexer.c ../src/frontend/parser.c ../src/frontend/error.c ../src/frontend/dispkir.c ../src/frontend/mkkir.c

extern const char *teststr;

int main(void)
{
    kl_lexer *l = lexer_new_string(teststr);
    // l->options |= LEXER_OPT_FETCH_NAME;
    kl_context *ctx = parser_new_context();
    // ctx->options |= PARSER_OPT_PHASE;

    int r = parse(ctx, l);
    make_kir(ctx);
    disp_program(ctx);

    free_context(ctx);
    lexer_free(l);

    return 0;
}

const char *teststr =
"func fib(n: int64) : int64 {\n"
"    if (n < 2) {\n"
"        return n;"
"    }\n"
"    return fib(n-1) + fib(n-2);\n"
"}\n"

"func fib2(n) {\n"
"    func internal(n) {}\n"
"    if (n < 2) {\n"
"        return n;"
"    }\n"
"    return fib2(n-1) + fib2(n-2);\n"
"}\n"

"fib(38);\n"
;
