#include "../src/frontend/parser.h"
#include "../src/frontend/disp.h"
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

// cl test_parser.c ..\src\frontend\lexer.c ..\src\frontend\parser.c ..\src\frontend\error.c ..\src\frontend\disp.c
// gcc test_parser.c ../src/frontend/lexer.c ../src/frontend/parser.c ../src/frontend/error.c ../src/frontend/disp.c

extern const char *teststr;

int main(void)
{
    kl_lexer *l = lexer_new_string(teststr);
    // l->options |= LEXER_OPT_FETCH_NAME;
    kl_context *ctx = parser_new_context();
    // ctx->options |= PARSER_OPT_PHASE;

    int r = parse(ctx, l);
    dispast(ctx);

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

"func fib(n: int64, { a, b:x }, [y,z,,last]) : ()=>int64 {\n"
"    if (n < 2) {\n"
"        return n ? [,,a,,2,n,,3,,,] : { x, a: {x: 1, n, a: 2}, last, z, n};"
"    }\n"
"    return fib(n-1) + fib(n-2);\n"
"}\n"

// "func xxx(n: int64) : int64 {\n"
// "    let x;\n"
// "    const a = 1, b = 2;\n"
// "    let y = 11 + a, x = 10;\n"
// "    if (a) {}\n"
// "    else {x = 10;}\n"
// "    do {} while (0);\n"
// "}\n"

// "func yyy(n: ((int64, (int64, int64) => real) => int64, real) => real) : ()=>real {\n"
// "}\n"

// "func fibxx(n: int64) : int64 {\n"
// "    if (n < 3) {\n"
// "        return n;"
// "    }\n"
// "    return fibxx(n-1) + fibxx(n-2);\n"
// "}\n"
// "func f(x, y) {\n"
// "    x + y + x + <1,2,3,x+y>;\n"
// "    func f2(x, y) {\n"
// "         while (x < 100) {\n"
// "             x = (y + x) *2;\n"
// "         }\n"
// "    }\n"
// "}\n"

// "namespace {\n"
// "        func f2(x, y) {\n"
// "x + y + x;\n"
// "        }\n"
// "    namespace NS {\n"
// "        func fib(n, x: int16) {\n"
// "           return fib(n-1) + fib(n-2);\n"
// "        }\n"
// "        func some(s: string, n: int64) : string {\n"
// "   n + n * 2;\n"
// "return s + 92233720368547758089223372036854775808;\n"
// "        }\n"
// "        class classname(a) {\n"
// "            public some(s: string, b: int64, c: int64) : string {\n"
// "                a = b = 9223372036854775808;\n"
// "                s = a + (9223372036854775807 + 0x10) * f(f, 2*c) + \"aaa%{c+2}bbb\"\"+ccc\";\n"
// "\"\\041\\taaa\\uD867\\uDE3D\\u{29e3d}\"\"nnn\\uD83C\\uDF4E\";"
// "            }\n"
// "        }\n"
// "    }\n"
// "    {\n"
// "       3 + 2 + NS;\n"
// "    }\n"
// "}\n"
// "{\n"
// "    1 + 2 + 3;\n"
// "}\n"
;
