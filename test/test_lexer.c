#include "../src/frontend/lexer.h"

// cl test_lexer.c ..\src\frontend\lexer.c ..\src\frontend\error.c
// gcc test_lexer.c ../src/frontend/lexer.c ../src/frontend/error.c

const char *teststr =
    "! = + - * / % & | ^ ? ** << >> && || += -= *= /= %= &= |= ^= **= <<= >>= &&= ||= =~ => !~ == != < <= > >= <=> "
    ", : ; . .. ... ( ) [ ] { } "
    "extern const var let new import namespace module class private protected public mixin func function if else do while for in return "
        "switch case when break continue default otherwise fallthrough yield try catch finally throw "
    "xconst xvar xlet xnew xmodule xclass xprivate xprotected xpublic xmixin xfunc xfunction xif xelse xdo xwhile xfor "
        "xin xreturn xswitch xcase xwhen xbreak xcontinue xdefault xotherwise xfallthrough xyield xtry xcatch xfinally xthrow "
    "constx varx letx newx modulex classx privatex protectedx publicx mixinx funcx functionx ifx elsex dox whilex forx "
        "inx returnx switchx casex whenx breakx continuex defaultx otherwisex fallthroughx yieldx tryx catchx finallyx throwx "
    "_throw _this_is_a_long_long_long_long_very_long_name _name_with_9876_number_012345"
;
const tk_token expected[] = {
    TK_NOT, TK_EQ, TK_ADD, TK_SUB, TK_MUL, TK_DIV, TK_MOD, TK_AND, TK_OR, TK_XOR, TK_QES, TK_EXP, TK_LSH, TK_RSH, TK_LAND, TK_LOR,
    TK_ADDEQ, TK_SUBEQ, TK_MULEQ, TK_DIVEQ, TK_MODEQ, TK_ANDEQ, TK_OREQ, TK_XOREQ, TK_EXPEQ, TK_LSHEQ, TK_RSHEQ, TK_LANDEQ,
    TK_LOREQ, TK_REGEQ, TK_DARROW, TK_REGNE, TK_EQEQ, TK_NEQ, TK_LT, TK_LE, TK_GT, TK_GE, TK_LGE,
    TK_COMMA, TK_COLON, TK_SEMICOLON, TK_DOT, TK_DOT2, TK_DOT3, TK_LSBR, TK_RSBR, TK_LLBR, TK_RLBR, TK_LXBR, TK_RXBR,
    TK_EXTERN, TK_CONST, TK_LET, TK_LET, TK_NEW, TK_IMPORT, TK_NAMESPACE, TK_MODULE, TK_CLASS, TK_PRIVATE, TK_PROTECTED, TK_PUBLIC, TK_MIXIN, TK_FUNC,
        TK_FUNC, TK_IF, TK_ELSE, TK_DO, TK_WHILE, TK_FOR, TK_IN, TK_RETURN, TK_SWITCH, TK_CASE, TK_WHEN, TK_BREAK, TK_CONTINUE,
        TK_DEFAULT, TK_OTHERWISE, TK_FALLTHROUGH, TK_YIELD, TK_TRY, TK_CATCH, TK_FINALLY, TK_THROW,
    TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME,
        TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME,
        TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME,
    TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME,
        TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME,
        TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME, TK_NAME,
    TK_NAME, TK_NAME, TK_NAME,
};

int main(void)
{
    kl_lexer *l = lexer_new_string(teststr);
    int errors = 0;
    int items = sizeof(expected) / sizeof(expected[0]);
    printf("Testing %d items\n", items);
    for (int i = 0; i < items; ++i) {
        tk_token tok = lexer_fetch(l);
        if (tok == EOF) {
            printf("END\n");
            break;
        }
        if (tok == expected[i]) {
            if (l->str[0]) {
                printf("[%3d] PASS: token(%s) for %s\n", i+1, tokenname(expected[i]), l->str);
            } else {
                printf("[%3d] PASS: token(%s)\n", i+1, tokenname(expected[i]));
            }
        } else {
            printf("[%3d] FAIL: token(%s), but actually it's %s.\n", i+1, tokenname(expected[i]), tokenname(tok));
            ++errors;
        }
    }
    printf("Testing ended with %d error(s)\n", errors);
    lexer_free(l);
    return 0;
}
