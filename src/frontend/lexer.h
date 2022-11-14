#ifndef KILITE_LEXER_H
#define KILITE_LEXER_H

#include "../kir.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/*
 * Defnitions
 */
#define LEXER_STRBUF_SZ (1024)

/*
 * Structure of lexical analyzer.
 */
typedef struct kl_lexer {
    const char *filename;
    FILE *f;
    char *s;
    char *p;
    char *precode;
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
    int error_stdout;
    tk_token tok;           //  Current token.
    tk_token unfetch;       //  Buffer for unfetch.
    tk_typeid typeid;       //  Type ID
    uint32_t options;

    int64_t i64;
    double dbl;
    char str[LEXER_STRBUF_SZ];
} kl_lexer;

#define LEXER_OPT_FETCH_NAME (0x01)

/*
 * Public functions of lexical analyzer.
 */
extern const char *tokenname(int tok);
extern const char *typeidname(int tid);
extern kl_lexer *lexer_new_file(const char *f);
extern kl_lexer *lexer_new_string(const char *s);
extern void lexer_free(kl_lexer *l);
extern tk_token lexer_fetch(kl_lexer *l);
extern void lexer_unfetch(kl_lexer *l, tk_token prev);

#endif /* KILITE_LEXER_H */
