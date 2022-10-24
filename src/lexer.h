#ifndef KILITE_LEXER_H
#define KILITE_LEXER_H

#include "common.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/*
 * Defnitions
 */
#define LEXER_STRBUF_SZ (1024)

/*
 * Available tokens in the Kilite language.
 */
typedef enum tk_typeid {
    // Types
    TK_TANY = 0,            //  <any type>
    TK_TSINT8,              //  int8
    TK_TSINT16,             //  int16
    TK_TSINT32,             //  int32
    TK_TSINT64,             //  int64
    TK_TUINT8,              //  uint8
    TK_TUINT16,             //  uint16
    TK_TUINT32,             //  uint32
    TK_TUINT64,             //  uint64
    TK_TBIGINT,             //  bigint
    TK_TDBL,                //  real
    TK_TSTR,                //  string
    TK_TBIN,                //  binary

    TK_TCLASSBASE,          //  The typename value of class will be started from this value.
} tk_typeid;

/* '*' means that the implementation of parsing has been done. */
typedef enum tk_token {
    TK_EOF = 0,
    TK_UNKNOWN,

    // Literals
    TK_VSINT,               //  * Integer value as signed 64 bit.
    TK_VUINT,               //  * Integer value as unsigned 64 bit, only when it's over the range of signed 64 bit.
    TK_VBIGINT,             //  * Big Integer value
    TK_VDBL,                //  * Double precision value
    TK_VSTR,                //  * String literal
    TK_VBIN,                //  * Binary literal

    // Keywords
    TK_CONST,               //  * const
    TK_LET,                 //  * let/var, and var is same as let. So it's different from JavaScript.
    TK_NEW,                 //    new
    TK_IMPORT,              //    import
    TK_NAMESPACE,           //  * namespace
    TK_MODULE,              //    module
    TK_CLASS,               //  * class
    TK_PRIVATE,             //  * private
    TK_PROTECTED,           //  * protected
    TK_PUBLIC,              //  * public
    TK_MIXIN,               //    mixin
    TK_FUNC,                //  * func/function, which one you can choose as you like.
    TK_IF,                  //  * if
    TK_ELSE,                //  * else
    TK_DO,                  //  * do
    TK_WHILE,               //  * while
    TK_FOR,                 //    for
    TK_IN,                  //    in
    TK_RETURN,              //  * return
    TK_SWITCH,              //    switch
    TK_CASE,                //    case
    TK_WHEN,                //    when
    TK_BREAK,               //    break
    TK_CONTINUE,            //    continue
    TK_DEFAULT,             //    default
    TK_OTHERWISE,           //    otherwise
    TK_FALLTHROUGH,         //    fallthrough
    TK_YIELD,               //    yield
    TK_TRY,                 //    try
    TK_CATCH,               //    catch
    TK_FINALLY,             //    finally
    TK_THROW,               //    throw

    // Operators
        // Unary operators
    TK_BNOT,                //  * ~
    TK_NOT,                 //  * !
        // Assignment
    TK_EQ,                  //  * =
    TK_ADDEQ,               //  * +=
    TK_SUBEQ,               //  * -=
    TK_MULEQ,               //  * *=
    TK_DIVEQ,               //  * /=
    TK_MODEQ,               //  * %=
    TK_ANDEQ,               //  * &=
    TK_OREQ,                //  * |=
    TK_XOREQ,               //  * ^=
    TK_EXPEQ,               //  * **=
    TK_LSHEQ,               //  * <<=
    TK_RSHEQ,               //  * >>=
    TK_LANDEQ,              //  * &&=
    TK_LOREQ,               //  * ||=
        // Comparison
    TK_REGEQ,               //    =~
    TK_REGNE,               //    !~
    TK_EQEQ,                //  * ==
    TK_NEQ,                 //  * !=
    TK_LT,                  //  * <
    TK_LE,                  //  * <=
    TK_GT,                  //  * >
    TK_GE,                  //  * >=
    TK_LGE,                 //  * <=>
        // binary operators
    TK_ADD,                 //  * +
    TK_SUB,                 //  * -
    TK_MUL,                 //  * *
    TK_DIV,                 //  * /
    TK_MOD,                 //  * %
    TK_AND,                 //  * &
    TK_OR,                  //  * |
    TK_XOR,                 //  * ^
    TK_QES,                 //  * ?
    TK_EXP,                 //  * **
    TK_LSH,                 //  * <<
    TK_RSH,                 //  * >>
    TK_LAND,                //  * &&
    TK_LOR,                 //  * ||

    // Punctuations
    TK_COMMA,               //  ,
    TK_COLON,               //  :
    TK_SEMICOLON,           //  ;
    TK_DOT,                 //  .
    TK_DOT2,                //  ..
    TK_DOT3,                //  ...
    TK_LSBR,                //  (
    TK_RSBR,                //  )
    TK_LLBR,                //  [
    TK_RLBR,                //  ]
    TK_LXBR,                //  {
    TK_RXBR,                //  }

    // Others
    TK_ARROW,               //  ->
    TK_DARROW,              //  =>
    TK_TYPEID,              //  Typename in tk_typeid.
    TK_NAME,                //  <name>

    // For extra keywords in parsing.
    TK_BLOCK,               //  For block.
    TK_CONNECT,             //  For connected expressions.
    TK_VAR,                 //  For variable.
    TK_EXPR,                //  For expression type.
    TK_CALL,                //  For function call.
    TK_IDX,                 //  For array index reference.
    TK_TYPENODE,            //  The node will represent the type.
    TK_BINEND,              //  The mark for the end of a binary literal.
} tk_token;

/*
 * Structure of lexical analyzer.
 */
typedef struct kl_lexer {
    FILE *f;
    char *s;
    char *p;
    int errors;             //  Error count.
    int line;
    int pos;
    int tokline;            //  Position of token.
    int tokpos;             //  Position of token.
    int toklen;             //  Length of token.
    int ch;                 //  Pre fetched character.
    int strch;              //  Either " or ' as a strting char of a string.
    int strstate;           //  The state during parsing string.
    int lbrcount;           //  The '{' count during parsing string.
    int binmode;            //  If 1, it's parsing a binary.
    tk_token tok;           //  Current token.
    tk_token unfetch;       //  Buffer for unfetch.
    tk_typeid type;         //  Type value
    uint32_t options;

    int64_t i64;
    uint64_t u64;
    double dbl;
    char str[LEXER_STRBUF_SZ];
} kl_lexer;

#define LEXER_OPT_FETCH_NAME (0x01)

/*
 * Public functions of lexical analyzer.
 */
const char *tokenname(int tok);
const char *typeidname(int tid);
kl_lexer *lexer_new_file(const char *f);
kl_lexer *lexer_new_string(const char *s);
void lexer_free(kl_lexer *l);
tk_token lexer_fetch(kl_lexer *l);
void lexer_unfetch(kl_lexer *l, tk_token prev);

#endif /* KILITE_LEXER_H */
