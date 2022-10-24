#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "lexer.h"
#include "error.h"

static const char *tpname[] = {
    "any",
    "int8",
    "int16",
    "int32",
    "int64",
    "uint8",
    "uint16",
    "uint32",
    "uint64",
    "bigint",
    "real",
    "string",
    "binary",
};
static const char *tkname[] = {
    "TK_EOF",
    "TK_UNKNOWN",
    "TK_VSINT",
    "TK_VUINT",
    "TK_VBIGINT",
    "TK_VDBL",
    "TK_VSTR",
    "TK_VBIN",
    "TK_CONST",
    "TK_LET",
    "TK_NEW",
    "TK_IMPORT",
    "TK_NAMESPACE",
    "TK_MODULE",
    "TK_CLASS",
    "TK_PRIVATE",
    "TK_PROTECTED",
    "TK_PUBLIC",
    "TK_MIXIN",
    "TK_FUNC",
    "TK_IF",
    "TK_ELSE",
    "TK_DO",
    "TK_WHILE",
    "TK_FOR",
    "TK_IN",
    "TK_RETURN",
    "TK_SWITCH",
    "TK_CASE",
    "TK_WHEN",
    "TK_BREAK",
    "TK_CONTINUE",
    "TK_DEFAULT",
    "TK_OTHERWISE",
    "TK_FALLTHROUGH",
    "TK_YIELD",
    "TK_TRY",
    "TK_CATCH",
    "TK_FINALLY",
    "TK_THROW",

    "TK_BNOT",
    "TK_NOT",

    "TK_EQ",
    "TK_ADDEQ",
    "TK_SUBEQ",
    "TK_MULEQ",
    "TK_DIVEQ",
    "TK_MODEQ",
    "TK_ANDEQ",
    "TK_OREQ",
    "TK_XOREQ",
    "TK_EXPEQ",
    "TK_LSHEQ",
    "TK_RSHEQ",
    "TK_LANDEQ",
    "TK_LOREQ",

    "TK_REGEQ",
    "TK_REGNE",
    "TK_EQEQ",
    "TK_NEQ",
    "TK_LT",
    "TK_LE",
    "TK_GT",
    "TK_GE",
    "TK_LGE",

    "TK_ADD",
    "TK_SUB",
    "TK_MUL",
    "TK_DIV",
    "TK_MOD",
    "TK_AND",
    "TK_OR",
    "TK_XOR",
    "TK_QES",
    "TK_EXP",
    "TK_LSH",
    "TK_RSH",
    "TK_LAND",
    "TK_LOR",

    "TK_COMMA",
    "TK_COLON",
    "TK_SEMICOLON",
    "TK_DOT",
    "TK_DOT2",
    "TK_DOT3",
    "TK_LSBR",
    "TK_RSBR",
    "TK_LLBR",
    "TK_RLBR",
    "TK_LXBR",
    "TK_RXBR",

    "TK_ARROW",
    "TK_DARROW",
    "TK_TYPEID",
    "TK_NAME",

    "TK_BLOCK",
    "TK_CONNECT",
    "TK_VAR",
    "TK_EXPR",
    "TK_CALL",
    "TK_IDX",
    "TK_BINEND",
};

#define LEXER_CHECK_12_TOK(ch2, tok1, tok12)\
    lexer_getch(l);\
    if (l->ch == ch2) {\
        lexer_getch(l);\
        l->toklen = 2;\
        return tok12;\
    }\
    l->toklen = 1;\
    return tok1;\
    /**/
#define LEXER_CHECK_123_TOK(ch2, ch3, tok1, tok12, tok123)\
    lexer_getch(l);\
    if (l->ch == ch2) {\
        lexer_getch(l);\
        if (l->ch == ch3) {\
            lexer_getch(l);\
            l->toklen = 3;\
            return tok123;\
        }\
        l->toklen = 2;\
        return tok12;\
    }\
    l->toklen = 1;\
    return tok1;\
    /**/
#define LEXER_CHECK_12_13_TOK(ch2, ch3, tok1, tok12, tok13)\
    lexer_getch(l);\
    if (l->ch == ch2) {\
        lexer_getch(l);\
        l->toklen = 2;\
        return tok12;\
    }\
    if (l->ch == ch3) {\
        lexer_getch(l);\
        l->toklen = 2;\
        return tok13;\
    }\
    l->toklen = 1;\
    return tok1;\
    /**/
#define LEXER_CHECK_12_13_14_TOK(ch2, ch3, ch4, tok1, tok12, tok13, tok14)\
    lexer_getch(l);\
    if (l->ch == ch2) {\
        lexer_getch(l);\
        l->toklen = 2;\
        return tok12;\
    }\
    if (l->ch == ch3) {\
        lexer_getch(l);\
        l->toklen = 2;\
        return tok13;\
    }\
    if (l->ch == ch4) {\
        lexer_getch(l);\
        l->toklen = 2;\
        return tok14;\
    }\
    l->toklen = 1;\
    return tok1;\
    /**/
#define LEXER_CHECK_12_13_123_TOK(ch2, ch3, tok1, tok12, tok13, tok123)\
    lexer_getch(l);\
    if (l->ch == ch2) {\
        lexer_getch(l);\
        if (l->ch == ch3) {\
            lexer_getch(l);\
            l->toklen = 3;\
            return tok123;\
        }\
        l->toklen = 2;\
        return tok12;\
    }\
    if (l->ch == ch3) {\
        lexer_getch(l);\
        l->toklen = 2;\
        return tok13;\
    }\
    l->toklen = 2;\
    return tok1;\
    /**/
#define LEXER_CHECK_12_13_123_134_TOK(ch2, ch3, ch4, tok1, tok12, tok13, tok123, tok134)\
    lexer_getch(l);\
    if (l->ch == ch2) {\
        lexer_getch(l);\
        if (l->ch == ch3) {\
            lexer_getch(l);\
            l->toklen = 3;\
            return tok123;\
        }\
        l->toklen = 2;\
        return tok12;\
    }\
    if (l->ch == ch3) {\
        lexer_getch(l);\
        if (l->ch == ch4) {\
            lexer_getch(l);\
            l->toklen = 3;\
            return tok134;\
        }\
        l->toklen = 2;\
        return tok13;\
    }\
    l->toklen = 1;\
    return tok1;\
    /**/

static int lexer_error(const char *phase, kl_lexer *l, int line, int pos, int len, const char *fmt, ...)
{
    l->errors++;
    va_list ap;
    va_start(ap, fmt);
    int r = error_print(phase, line, pos, len, fmt, ap);
    va_end(ap);
    return r;
}

const char *typeidname(int tid)
{
    if (tid < sizeof(tpname)/sizeof(tpname[0])) {
        return tpname[tid];
    }
    return "TK_TUNKNOWN";
}

const char *tokenname(int tok)
{
    if (tok < sizeof(tkname)/sizeof(tkname[0])) {
        return tkname[tok];
    }
    return "TK_UNKNOWN";
}

kl_lexer *lexer_new_file(const char *f)
{
    kl_lexer *l = (kl_lexer *)calloc(1, sizeof(kl_lexer));
    l->s = l->p = NULL;
    l->f = (FILE *)fopen(f, "rb");
    l->ch = ' ';
    l->tok = EOF;
    l->line = 1;
    l->pos = 0;
    l->tokline = 1;
    l->tokpos = 0;
    l->toklen = 1;
    return l;
}

kl_lexer *lexer_new_string(const char *s)
{
    kl_lexer *l = (kl_lexer *)calloc(1, sizeof(kl_lexer));
    l->s = l->p = strdup(s);
    l->f = NULL;
    l->ch = ' ';
    l->tok = EOF;
    l->line = 1;
    l->pos = 0;
    l->tokline = 1;
    l->tokpos = 0;
    l->toklen = 1;
    return l;
}

void lexer_free(kl_lexer *l)
{
    if (l->f) {
        fclose(l->f);
        l->f = NULL;
    }
    if (l->s) {
        free(l->s);
    }
    free(l);
}

static inline void lexer_getch(kl_lexer *l)
{
    if (l->f) {
        l->ch = fgetc(l->f);
    } else if (l->p) {
        if (*(l->p)) {
            l->ch = *(l->p);
            ++(l->p);
        } else {
            l->ch = EOF;
        }
    } else {
        l->ch = EOF;
    }
    if (l->ch == '\n') {
        l->line++;
        l->pos = 0;
    } else {
        l->pos++;
    }
}

static inline int is_whitespace(int ch)
{
    return isspace(ch);
}

static inline int is_name_head(int ch)
{
    return isalpha(ch) || ch == '_';
}

static inline int is_name_char(int ch)
{
    return isalnum(ch) || ch == '_';
}

static inline int isbdigit(int ch)
{
    return ch == '0' || ch == '1';
}

static inline int is_2num_char(int ch)
{
    return isbdigit(ch) || ch == '_';
}

static inline int isodigit(int ch)
{
    return ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' || ch == '5' || ch == '6' || ch == '7';
}

static inline int is_8num_char(int ch)
{
    return isodigit(ch) || ch == '_';
}

static inline int is_10num_char(int ch)
{
    return isdigit(ch) || ch == '_';
}

static inline int is_16num_char(int ch)
{
    return isxdigit(ch) || ch == '_';
}

static inline int set_type(kl_lexer *l, tk_token tok, tk_typeid tid)
{
    l->type = tid;
    return tok;
}

static inline int check_keyword(kl_lexer *l)
{
    int hc = l->str[0];
    switch (hc) {
    case 'b':
        if (strcmp(l->str, "binary") == 0) return set_type(l, TK_TYPEID, TK_TBIN);
        if (strcmp(l->str, "break") == 0) return TK_BREAK;
        if (strcmp(l->str, "bigint") == 0) return set_type(l, TK_TYPEID, TK_TBIGINT);
        break;
    case 'c':
        if (strcmp(l->str, "const") == 0) return TK_CONST;
        if (strcmp(l->str, "catch") == 0) return TK_CATCH;
        if (strcmp(l->str, "class") == 0) return TK_CLASS;
        if (strcmp(l->str, "case") == 0) return TK_CASE;
        if (strcmp(l->str, "continue") == 0) return TK_CONTINUE;
        break;
    case 'd':
        if (strcmp(l->str, "default") == 0) return TK_DEFAULT;
        if (strcmp(l->str, "do") == 0) return TK_DO;
        break;
    case 'e':
        if (strcmp(l->str, "else") == 0) return TK_ELSE;
        break;
    case 'f':
        if (strcmp(l->str, "func") == 0) return TK_FUNC;
        if (strcmp(l->str, "function") == 0) return TK_FUNC;
        if (strcmp(l->str, "for") == 0) return TK_FOR;
        if (strcmp(l->str, "fallthrough") == 0) return TK_FALLTHROUGH;
        if (strcmp(l->str, "finally") == 0) return TK_FINALLY;
        break;
    case 'i':
        if (strcmp(l->str, "int8") == 0) return set_type(l, TK_TYPEID, TK_TSINT8);
        if (strcmp(l->str, "int16") == 0) return set_type(l, TK_TYPEID, TK_TSINT16);
        if (strcmp(l->str, "int32") == 0) return set_type(l, TK_TYPEID, TK_TSINT32);
        if (strcmp(l->str, "int64") == 0) return set_type(l, TK_TYPEID, TK_TSINT64);
        if (strcmp(l->str, "if") == 0) return TK_IF;
        if (strcmp(l->str, "in") == 0) return TK_IN;
        if (strcmp(l->str, "import") == 0) return TK_IMPORT;
        break;
    case 'l':
        if (strcmp(l->str, "let") == 0) return TK_LET;
        break;
    case 'm':
        if (strcmp(l->str, "module") == 0) return TK_MODULE;
        if (strcmp(l->str, "mixin") == 0) return TK_MIXIN;
        break;
    case 'n':
        if (strcmp(l->str, "new") == 0) return TK_NEW;
        if (strcmp(l->str, "namespace") == 0) return TK_NAMESPACE;
        break;
    case 'o':
        if (strcmp(l->str, "otherwise") == 0) return TK_OTHERWISE;
        break;
    case 'p':
        if (strcmp(l->str, "private") == 0) return TK_PRIVATE;
        if (strcmp(l->str, "protected") == 0) return TK_PROTECTED;
        if (strcmp(l->str, "public") == 0) return TK_PUBLIC;
        break;
    case 'r':
        if (strcmp(l->str, "real") == 0) return set_type(l, TK_TYPEID, TK_TDBL);
        if (strcmp(l->str, "return") == 0) return TK_RETURN;
        break;
    case 's':
        if (strcmp(l->str, "string") == 0) return set_type(l, TK_TYPEID, TK_TSTR);
        if (strcmp(l->str, "switch") == 0) return TK_SWITCH;
        break;
    case 't':
        if (strcmp(l->str, "try") == 0) return TK_TRY;
        if (strcmp(l->str, "throw") == 0) return TK_THROW;
        break;
    case 'u':
        if (strcmp(l->str, "uint8") == 0) return set_type(l, TK_TYPEID, TK_TUINT8);
        if (strcmp(l->str, "uint16") == 0) return set_type(l, TK_TYPEID, TK_TUINT16);
        if (strcmp(l->str, "uint32") == 0) return set_type(l, TK_TYPEID, TK_TUINT32);
        if (strcmp(l->str, "uint64") == 0) return set_type(l, TK_TYPEID, TK_TUINT64);
        break;
    case 'v':
        if (strcmp(l->str, "var") == 0) return TK_LET;
        break;
    case 'w':
        if (strcmp(l->str, "while") == 0) return TK_WHILE;
        if (strcmp(l->str, "when") == 0) return TK_WHEN;
        break;
    case 'y':
        if (strcmp(l->str, "yield") == 0) return TK_YIELD;
        break;
    default:
        ;
    }
    return TK_NAME;
}

static inline int get_name(kl_lexer *l)
{
    char buf[LEXER_STRBUF_SZ] = { l->ch };
    lexer_getch(l);
    if (is_name_char(l->ch)) {
        buf[1] = l->ch;
        int i = 2;
        for ( ; i < (LEXER_STRBUF_SZ-8); ++i) {
            lexer_getch(l);
            if (!is_name_char(l->ch)) {
                break;
            }
            buf[i] = l->ch;
        }
        l->toklen = i;
    } else {
        l->toklen = 1;
    }
    strcpy(l->str, buf);
    return check_keyword(l);
}

static inline int get_10fixnum(kl_lexer *l, char *buf, int i, int max)
{
    while (is_10num_char(l->ch)) {
        if (l->ch != '_') {
            buf[i++] = l->ch;
        }
        lexer_getch(l);
        if (max <= i) {
            break;
        }
    }
    return i;
}

static inline int get_real(kl_lexer *l, char *buf, int i, int max)
{
    buf[i++] = '.';
    lexer_getch(l);
    i = get_10fixnum(l, buf, i, max);
    if (i < max && (l->ch == 'e' || l->ch == 'E')) {
        buf[i++] = l->ch;
        lexer_getch(l);
        if (i < max && (l->ch == '+' || l->ch == '-')) {
            buf[i++] = l->ch;
            lexer_getch(l);
        }
        i = get_10fixnum(l, buf, i, max);
    }
    buf[i] = 0;
    l->dbl = strtod(buf, NULL);
    return TK_VDBL;
}

static inline int generate_int_value(kl_lexer *l, char *buf, int radix)
{
    errno = 0;
    int64_t i64 = strtoll(buf, NULL, radix);
    if (errno != ERANGE && errno != EINVAL) {
        l->i64 = i64;
        return TK_VSINT;
    }
    errno = 0;
    uint64_t u64 = strtoull(buf, NULL, radix);
    if (errno != ERANGE && errno != EINVAL) {
        l->u64 = u64;
        return TK_VUINT;
    }
    strcpy(l->str, buf);
    return TK_VBIGINT;
}

static inline int get_integer(kl_lexer *l, char *buf, int i, int max, int (*is_xnum_char)(int), int radix)
{
    while (is_xnum_char(l->ch)) {
        if (l->ch != '_') {
            buf[i++] = l->ch;
        }
        lexer_getch(l);
        if (max <= i) {
            break;
        }
    }
    buf[i] = 0;
    return generate_int_value(l, buf, radix);
}

static inline int get_number(kl_lexer *l)
{
    #define TOKEN_BUFSIZE (1023)
    char buf[TOKEN_BUFSIZE+1] = { 0 };
    if (l->ch == '0') {
        lexer_getch(l);
        if (l->ch == '.') {
            buf[0] = '0';
            return get_real(l, buf, 1, TOKEN_BUFSIZE);
        }
        if (l->ch == 'x' || l->ch == 'X') {
            buf[0] = '0';
            buf[1] = 'x';
            lexer_getch(l);
            return get_integer(l, buf, 2, TOKEN_BUFSIZE, is_16num_char, 0);
        }
        if (l->ch == 'b' || l->ch == 'B') {
            return get_integer(l, buf, 0, TOKEN_BUFSIZE, is_2num_char, 2);
        }
        buf[0] = '0';
        return get_integer(l, buf, 1, TOKEN_BUFSIZE, is_8num_char, 8);
    }
    /* l->ch must be '1' .. '9' */
    buf[0] = l->ch;
    lexer_getch(l);
    int i = get_10fixnum(l, buf, 1, TOKEN_BUFSIZE);
    if (l->ch == '.') {
        return get_real(l, buf, i, TOKEN_BUFSIZE);
    }
    buf[i] = 0;
    return generate_int_value(l, buf, 0);
    #undef TOKEN_BUFSIZE
}

static inline uint64_t surrogatepair2codepoint(uint64_t h, uint64_t l)
{
    return (h - 0xD800) * 0x400 + (l - 0xDC00) + 0x10000;
}

static inline unsigned char *codepoint2utf8(unsigned char buf[], uint64_t cp)
{
    if (cp < 0x80) {
        buf[0] = cp;
        buf[1] = '\0';
    } else if (cp < 0x800) {
        buf[0] = cp >> 6 | 0xc0;
        buf[1] = (cp & 0x3f) | 0x80;
        buf[2] = '\0';
    } else if (cp < 0xd800 || (0xdfff < cp && cp < 0x10000)) {
        buf[0] = cp >> 12 | 0xe0;
        buf[1] = (cp >> 6 & 0x3f) | 0x80;
        buf[2] = (cp & 0x3f) | 0x80;
        buf[3] = '\0';
    } else if (cp < 0x10ffff) {
        buf[0] = cp >> 18 | 0xf0;
        buf[1] = (cp >> 12 & 0x3f) | 0x80;
        buf[2] = (cp >> 6 & 0x3f) | 0x80;
        buf[3] = (cp & 0x3f) | 0x80;
        buf[4] = '\0';
    }
    return buf;
}

static inline int append_unicode(char *buf, int i, int max, kl_lexer *l)
{
    int c1, c2, c3, c4, code;
    lexer_getch(l);
    c1 = l->ch;
    if (c1 == '{') {
        int i = 0;
        char b[16] = {0};
        lexer_getch(l);
        while (i < 16 && l->ch != '}' && l->ch != EOF) {
            if (!isxdigit(l->ch)) {
                lexer_error("Compile", l, l->line, l->pos - 2, 5, "invalid code point");
                lexer_getch(l);
                return i;
            }
            b[i++] = l->ch;
            lexer_getch(l);
        }
        code = strtol(b, NULL, 16);
    } else {
        const int pos = 3, len = 6;
        if (!isxdigit(c1)) {
            lexer_error("Compile", l, l->line, l->pos - (pos + 0), len, "invalid unicode number");
            lexer_getch(l);
            return i;
        }
        lexer_getch(l);
        c2 = l->ch;
        if (!isxdigit(c2)) {
            lexer_error("Compile", l, l->line, l->pos - (pos + 1), len, "invalid unicode number");
            return i;
        }
        lexer_getch(l);
        c3 = l->ch;
        if (!isxdigit(c3)) {
            lexer_error("Compile", l, l->line, l->pos - (pos + 3), len, "invalid unicode number");
            return i;
        }
        lexer_getch(l);
        c4 = l->ch;
        if (!isxdigit(c4)) {
            lexer_error("Compile", l, l->line, l->pos - (pos + 4), len, "invalid unicode number");
            return i;
        }
        char b[] = { c1, c2, c3, c4, 0 };
        code = strtol(b, NULL, 16);
        if (0xD800 <= code && code <= 0xDBFF) {
            const int pos = 7, len = 6;
            lexer_getch(l);
            if (l->ch != '\\') {
                lexer_error("Compile", l, l->line, l->pos - (pos + 0), len + 0, "invalid surrogate pair number");
                return i;
            }
            lexer_getch(l);
            if (l->ch != 'u') {
                lexer_error("Compile", l, l->line, l->pos - (pos + 1), len + 1, "invalid surrogate pair number");
                return i;
            }
            lexer_getch(l);
            c1 = l->ch;
            if (!isxdigit(c1)) {
                lexer_error("Compile", l, l->line, l->pos - (pos + 2), len + 3, "invalid surrogate pair number");
                return i;
            }
            lexer_getch(l);
            c2 = l->ch;
            if (!isxdigit(c2)) {
                lexer_error("Compile", l, l->line, l->pos - (pos + 3), len + 4, "invalid surrogate pair number");
                return i;
            }
            lexer_getch(l);
            c3 = l->ch;
            if (!isxdigit(c3)) {
                lexer_error("Compile", l, l->line, l->pos - (pos + 4), len + 5, "invalid surrogate pair number");
                return i;
            }
            lexer_getch(l);
            c4 = l->ch;
            if (!isxdigit(c4)) {
                lexer_error("Compile", l, l->line, l->pos - (pos + 5), len + 6, "invalid surrogate pair number");
                return i;
            }
            char b[] = { c1, c2, c3, c4, 0 };
            int code2 = strtol(b, NULL, 16);
            code = surrogatepair2codepoint(code, code2);
        }
    }

    unsigned char utf8[16] = {0};
    codepoint2utf8(utf8, code);
    for (unsigned char *c = utf8; i < max && *c != 0; ++c) {
        buf[i++] = *c;
    }

    return i;
}

static inline int translate_escape(kl_lexer *l)
{
    switch (l->ch) {
    case 'a':  return '\a';
    case 'b':  return '\b';
    case 'e':  return '\x1b';
    case 'f':  return '\f';
    case 'n':  return '\n';
    case 'r':  return '\r';
    case 't':  return '\t';
    case 'v':  return '\f';
    case '"':  return '\"';
    case '\'': return '\'';
    case '?':  return '?'; 
    case '1':  return '\1';
    case '2':  return '\2';
    case '3':  return '\3';
    case '4':  return '\4';
    case '5':  return '\5';
    case '6':  return '\6';
    case '7':  return '\7';
    case '\\': return '\\';
    case '0': { //  octal number
        int c1, c2;
        lexer_getch(l);
        c1 = l->ch;
        if (!isodigit(c1)) {
            lexer_error("Compile", l, l->line, l->pos - 2, 3, "invalid octal number");
            lexer_getch(l);
            return 0;
        }
        lexer_getch(l);
        c2 = l->ch;
        if (!isodigit(c2)) {
            lexer_error("Compile", l, l->line, l->pos - 3, 3, "invalid octal number");
            return 0;
        }
        char b[] = { '0', c1, c2, 0 };
        return strtol(b, NULL, 8);
    }
    case 'x': { //  hexadecimal number
        int c1, c2;
        lexer_getch(l);
        c1 = l->ch;
        if (!isxdigit(c1)) {
            lexer_error("Compile", l, l->line, l->pos - 2, 3, "invalid hexadecimal number");
            lexer_getch(l);
            return 0;
        }
        lexer_getch(l);
        c2 = l->ch;
        if (!isxdigit(c2)) {
            lexer_error("Compile", l, l->line, l->pos - 3, 3, "invalid hexadecimal number");
            return 0;
        }
        char b[] = { c1, c2, 0 };
        return strtol(b, NULL, 16);
    }
    default:
        break;
    }
    return l->ch;
}

/*
    * String parser state behavior.
    state 0:    returns '(' and state -> 1
    state 1:    returns "... , if finding '"' then returns str and state -> 5, if finding '%{' returns str and state -> 2
    state 2:    returns '+' and state -> 3
    state 3:    returns '(' and state -> 9
    state 4:    returns '+' and state -> 1
    state 5:    returns ')' and state -> 0
    state 9:    parsing normally with counting '{', and if finding '}' and counted == 0 then returns ')' and state -> 4
*/
static int get_string(kl_lexer *l)
{
    #define STRING_BUFSIZE (LEXER_STRBUF_SZ-1)
    int i = 0;
    char buf[STRING_BUFSIZE+1] = {0};
    switch (l->strstate) {
    case 1:
        while (i < STRING_BUFSIZE && l->ch != EOF && l->ch != l->strch) {
            if (l->ch == '\\') {
                lexer_getch(l);
                if (l->ch == 'u') {
                    i = append_unicode(buf, i, STRING_BUFSIZE, l);
                    lexer_getch(l);
                    continue;
                } else {
                    l->ch = translate_escape(l);
                }
            } else if (l->ch == '%') {
                lexer_getch(l);
                if (l->ch == '{') {
                    l->strstate = 2;
                    strcpy(l->str, buf);
                    lexer_getch(l);
                    return TK_VSTR;
                }
                buf[i++] = '%';
            }
            buf[i++] = l->ch;
            lexer_getch(l);
        }
        /* l->ch must be same as l->strch */
        l->strstate = 5;
        strcpy(l->str, buf);
        return TK_VSTR;

    case 2:
        l->strstate = 3;
        return TK_ADD;

    case 3:
        l->strstate = 9;
        return TK_LSBR;

    case 4:
        l->strstate = 1;
        return TK_ADD;

    case 5:
        l->strstate = 0;
        lexer_getch(l);
        return TK_RSBR;
    }

    return TK_EOF;
    #undef STRING_BUFSIZE
}

static tk_token lexer_fetch_token(kl_lexer *l)
{
    l->str[0] = 0;

    if (l->strstate != 9 && l->strstate > 0) {
        return get_string(l);
    }

    if (is_name_head(l->ch)) {
        return get_name(l);
    }

    switch (l->ch) {
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        return get_number(l);
    case '"': case '\'':
        l->strch = l->ch;
        l->strstate = 1;
        l->lbrcount = 0;
        lexer_getch(l);
        return TK_LSBR;
    case '=':
        LEXER_CHECK_12_13_14_TOK('=', '~', '>', TK_EQ, TK_EQEQ, TK_REGEQ, TK_DARROW)
    case '!':
        LEXER_CHECK_12_13_TOK('=', '~', TK_NOT, TK_NEQ, TK_REGNE)
    case '+':
        LEXER_CHECK_12_TOK('=', TK_ADD, TK_ADDEQ)
    case '-':
        LEXER_CHECK_12_13_TOK('=', '>', TK_SUB, TK_SUBEQ, TK_ARROW)
    case '*':
        LEXER_CHECK_12_13_123_TOK('*', '=', TK_MUL, TK_EXP, TK_MULEQ, TK_EXPEQ)
    case '/':
        LEXER_CHECK_12_TOK('=', TK_DIV, TK_DIVEQ)
    case '%':
        LEXER_CHECK_12_TOK('=', TK_MOD, TK_MODEQ)
    case '&':
        LEXER_CHECK_12_13_123_TOK('&', '=', TK_AND, TK_LAND, TK_ANDEQ, TK_LANDEQ)
    case '|':
        LEXER_CHECK_12_13_123_TOK('|', '=', TK_OR, TK_LOR, TK_OREQ, TK_LOREQ)
    case '^':
        LEXER_CHECK_12_TOK('=', TK_XOR, TK_XOREQ)
    case '<':
        LEXER_CHECK_12_13_123_134_TOK('<', '=', '>', TK_LT, TK_LSH, TK_LE, TK_LSHEQ, TK_LGE)
    case '>':
        LEXER_CHECK_12_13_123_TOK('>', '=', TK_GT, TK_RSH, TK_GE, TK_RSHEQ)
    case '?':
        lexer_getch(l);
        return TK_QES;
    case '~':
        lexer_getch(l);
        return TK_BNOT;
    case ',':
        lexer_getch(l);
        return TK_COMMA;
    case ':':
        lexer_getch(l);
        return TK_COLON;
    case ';':
        lexer_getch(l);
        return TK_SEMICOLON;
    case '.':
        LEXER_CHECK_123_TOK('.', '.', TK_DOT, TK_DOT2, TK_DOT3);
    case '(':
        lexer_getch(l);
        return TK_LSBR;
    case ')':
        lexer_getch(l);
        return TK_RSBR;
    case '[':
        lexer_getch(l);
        return TK_LLBR;
    case ']':
        lexer_getch(l);
        return TK_RLBR;
    case '{':
        if (l->strstate == 9) {
            l->lbrcount++;
        }
        lexer_getch(l);
        return TK_LXBR;
    case '}':
        if (l->strstate == 9) {
            if (l->lbrcount == 0) {
                lexer_getch(l);
                l->strstate = 4;
                return TK_RSBR;
            }
            l->lbrcount--;
        }
        lexer_getch(l);
        return TK_RXBR;
    case EOF:
        return TK_EOF;
    }

    return TK_UNKNOWN;
}

void lexer_unfetch(kl_lexer *l, tk_token prev)
{
    l->unfetch = l->tok;
    l->tok = prev;
}

tk_token lexer_fetch(kl_lexer *l)
{
    if (l->unfetch) {
        l->tok = l->unfetch;
        l->unfetch = 0;
        return l->tok;
    }

    while (is_whitespace(l->ch)) {
        lexer_getch(l); // skip a whitespace.
    }

    l->tokline = l->line;
    l->tokpos = l->pos;
    l->tok = lexer_fetch_token(l);
    if ((l->options & LEXER_OPT_FETCH_NAME) == LEXER_OPT_FETCH_NAME) {
        if (l->tok == TK_EOF) {
            fprintf(stderr, "kl_lexer: EOF\n");
        } else if (l->str[0]) {
            fprintf(stderr, "kl_lexer: fetched(%s) with \"%s\"\n", tkname[l->tok], l->str);
        } else if (l->tok == TK_VSINT) {
            fprintf(stderr, "kl_lexer: fetched(%s) with %" PRId64 "\n", tkname[l->tok], l->i64);
        } else if (l->tok == TK_VUINT) {
            fprintf(stderr, "kl_lexer: fetched(%s) with %" PRIu64 "\n", tkname[l->tok], l->u64);
        } else if (l->tok == TK_VDBL) {
            fprintf(stderr, "kl_lexer: fetched(%s) with %f\n", tkname[l->tok], l->dbl);
        } else {
            fprintf(stderr, "kl_lexer: fetched(%s)\n", tkname[l->tok]);
        }
    }
    if (l->binmode && l->tok == TK_GT) {
        l->binmode = 0;
        l->tok = TK_BINEND;
    }
    return l->tok;
}
