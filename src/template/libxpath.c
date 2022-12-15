#ifndef KILITE_AMALGAMATION
#include "common.h"
#include "lib.h"
#endif

/* XPath */

enum xpath_axis {
    XPATH_AXIS_UNKNOWN = 0,
    XPATH_AXIS_ANCESTOR,
    XPATH_AXIS_ANCESTOR_OR_SELF,
    XPATH_AXIS_ATTRIBUTE,
    XPATH_AXIS_CHILD,
    XPATH_AXIS_DESCENDANT,
    XPATH_AXIS_DESCENDANT_OR_SELF,
    XPATH_AXIS_FOLLOWING,
    XPATH_AXIS_FOLLOWING_SIBLING,
    XPATH_AXIS_NAMESPACE,
    XPATH_AXIS_PARENT,
    XPATH_AXIS_PRECEDING,
    XPATH_AXIS_PRECEDING_SIBLING,
    XPATH_AXIS_SELF,
    XPATH_AXIS_ROOT,
};

enum xpath_node_type {
    XPATH_NODE_ROOT,
    XPATH_NODE_ELEMENT,
    XPATH_NODE_ATTRIBUTE,
    XPATH_NODE_NAMESPACE,
    XPATH_NODE_TEXT,
    XPATH_NODE_SIGNIFICANT_WHITESPACE,
    XPATH_NODE_WHITESPACE,
    XPATH_NODE_PROCESSIGN_INSTRUCTION,
    XPATH_NODE_COMMENT,
    XPATH_NODE_ALL,
};

enum xpath_type {
    XPATH_TK_UNKNOWN,       // Unknown lexeme
    XPATH_TK_LOR,           // Operator 'or'
    XPATH_TK_LAND,          // Operator 'and'
    XPATH_TK_EQ,            // Operator '='
    XPATH_TK_NE,            // Operator '!='
    XPATH_TK_LT,            // Operator '<'
    XPATH_TK_LE,            // Operator '<='
    XPATH_TK_GT,            // Operator '>'
    XPATH_TK_GE,            // Operator '>='
    XPATH_TK_ADD,           // Operator '+'
    XPATH_TK_SUB,           // Operator '-'
    XPATH_TK_MUL,           // Operator '*'
    XPATH_TK_DIV,           // Operator 'div'
    XPATH_TK_MOD,           // Operator 'mod'
    XPATH_TK_UMINUS,        // Unary Operator '-'
    XPATH_TK_UNION,         // Operator '|'
    XPATH_TK_LAST_OPERATOR = XPATH_TK_UNION,

    XPATH_TK_DOTDOT,        // '..'
    XPATH_TK_COLONCOLON,    // '::'
    XPATH_TK_SLASHSLASH,    // Operator '//'
    XPATH_TK_NUM,           // Numeric Literal
    XPATH_TK_AXISNAME,      // AxisName

    XPATH_TK_NAME,          // NameTest, NodeType, FunctionName, AxisName, second part of VariableReference
    XPATH_TK_STR,           // String Literal
    XPATH_TK_EOF,           // End of the expression

    XPATH_TK_LP,            // '('
    XPATH_TK_RP,            // ')'
    XPATH_TK_LB,            // '['
    XPATH_TK_RB,            // ']'
    XPATH_TK_DOT,           // '.'
    XPATH_TK_AT,            // '@'
    XPATH_TK_COMMA,         // ','

    XPATH_TK_ASTA,          // '*' ... NameTest
    XPATH_TK_SLASH,         // '/' ... Operator '/'
    XPATH_TK_DOLLAR,        // '$' ... First part of VariableReference
};

const char *xpath_token_name[] = {
    "XPATH_TK_UNKNOWN",
    "XPATH_TK_LOR",
    "XPATH_TK_LAND",
    "XPATH_TK_EQ",
    "XPATH_TK_NE",
    "XPATH_TK_LT",
    "XPATH_TK_LE",
    "XPATH_TK_GT",
    "XPATH_TK_GE",
    "XPATH_TK_ADD",
    "XPATH_TK_SUB",
    "XPATH_TK_MUL",
    "XPATH_TK_DIV",
    "XPATH_TK_MOD",
    "XPATH_TK_UMINUS",
    "XPATH_TK_UNION",

    "XPATH_TK_DOTDOT",
    "XPATH_TK_COLONCOLON",
    "XPATH_TK_SLASHSLASH",
    "XPATH_TK_NUM",
    "XPATH_TK_AXISNAME",

    "XPATH_TK_NAME",
    "XPATH_TK_STR",
    "XPATH_TK_EOF",

    "XPATH_TK_LP",
    "XPATH_TK_RP",
    "XPATH_TK_LB",
    "XPATH_TK_RB",
    "XPATH_TK_DOT",
    "XPATH_TK_AT",
    "XPATH_TK_COMMA",

    "XPATH_TK_ASTA",
    "XPATH_TK_SLASH",
    "XPATH_TK_DOLLAR",
};    

typedef struct xpath_lexer {
    const char *expr;
    int exprlen;
    int curidx;
    char cur;
    int axis;
    int token;
    int prevtype;
    vmstr *name;
    vmstr *prefix;
    vmstr *str;
    vmstr *num;
    int function;
    int start;
    int prevend;
    int e;
    int stksz;
    int posinfo[256];
} xpath_lexer;

static vmvar *xpath_union_expr(vmctx *ctx, xpath_lexer *l);

#define is_xpath_whitespace(x) ((x) == ' ' || (x) == '\t' || (x) == '\r' || (x) == '\n')
#define is_ascii_digit(x) ((unsigned int)((x) - '0') <= 9)
#define is_ncname_char(x) (('a' <= (x) && (x) <= 'z') || ('Z' <= (x) && (x) <= 'Z') || ('0' <= (x) && (x) <= '9') || ((x) == '_') || ((x) == '-'))

static void next_char(xpath_lexer *l)
{
    l->curidx++;
    if (l->curidx < l->exprlen) {
        l->cur = l->expr[l->curidx];
    } else {
        l->cur = 0;
    }
}

static void skip_xpah_whitespace(xpath_lexer *l)
{
    while (is_xpath_whitespace(l->cur)) {
        next_char(l);
    }
}

static void set_index(xpath_lexer *l, int idx)
{
    l->curidx = idx - 1;
    next_char(l);
}

static int check_axis(xpath_lexer *l)
{
    l->token = XPATH_TK_AXISNAME;

    if      (strcmp(l->name->hd, "ancestor")           == 0) return XPATH_AXIS_ANCESTOR;
    else if (strcmp(l->name->hd, "ancestor-or-self")   == 0) return XPATH_AXIS_ANCESTOR_OR_SELF;
    else if (strcmp(l->name->hd, "attribute")          == 0) return XPATH_AXIS_ATTRIBUTE;
    else if (strcmp(l->name->hd, "child")              == 0) return XPATH_AXIS_CHILD;
    else if (strcmp(l->name->hd, "descendant")         == 0) return XPATH_AXIS_DESCENDANT;
    else if (strcmp(l->name->hd, "descendant-or-self") == 0) return XPATH_AXIS_DESCENDANT_OR_SELF;
    else if (strcmp(l->name->hd, "following")          == 0) return XPATH_AXIS_FOLLOWING;
    else if (strcmp(l->name->hd, "following-sibling")  == 0) return XPATH_AXIS_FOLLOWING_SIBLING;
    else if (strcmp(l->name->hd, "namespace")          == 0) return XPATH_AXIS_NAMESPACE;
    else if (strcmp(l->name->hd, "parent")             == 0) return XPATH_AXIS_PARENT;
    else if (strcmp(l->name->hd, "preceding")          == 0) return XPATH_AXIS_PRECEDING;
    else if (strcmp(l->name->hd, "preceding-sibling")  == 0) return XPATH_AXIS_PRECEDING_SIBLING;
    else if (strcmp(l->name->hd, "self")               == 0) return XPATH_AXIS_SELF;

    l->token = XPATH_TK_NAME; 
    return XPATH_AXIS_UNKNOWN;
}

static int check_operator(xpath_lexer *l, int asta)
{
    int optype;

    if (asta) {
        optype = XPATH_TK_MUL;
    } else {
        if ((strlen(l->prefix->hd) != 0) || (strlen(l->name->hd) > 3)) {
            return 0;
        }
        if      (strcmp(l->name->hd, "or")  == 0) optype = XPATH_TK_LOR;
        else if (strcmp(l->name->hd, "and") == 0) optype = XPATH_TK_LAND;
        else if (strcmp(l->name->hd, "div") == 0) optype = XPATH_TK_DIV;
        else if (strcmp(l->name->hd, "mod") == 0) optype = XPATH_TK_MOD;
        else return 0;
    }
    if (l->prevtype <= XPATH_TK_LAST_OPERATOR) {
        return 0;
    }
    switch (l->prevtype) {
    case XPATH_TK_SLASH:
    case XPATH_TK_SLASHSLASH:
    case XPATH_TK_AT:
    case XPATH_TK_COLONCOLON:
    case XPATH_TK_LP:
    case XPATH_TK_LB:
    case XPATH_TK_COMMA:
    case XPATH_TK_DOLLAR:
        return 0;
    }

    l->token = optype;
    return 1;
}

static int get_string(vmctx *ctx, xpath_lexer *l)
{
    int startidx = l->curidx + 1;
    int endch = l->cur;
    next_char(l);
    while (l->cur != endch && l->cur != 0) {
        next_char(l);
    }

    if (l->cur == 0) {
        return 0;
    }

    str_set(ctx, l->str, l->expr + startidx, l->curidx - startidx);
    next_char(l);
    return 1;
}

static int get_ncname(vmctx *ctx, xpath_lexer *l, vmstr *name)
{
    int startidx = l->curidx;
    while (is_ncname_char(l->cur)) {
        next_char(l);
    }

    str_set(ctx, name, l->expr + startidx, l->curidx - startidx);
    return 1;
}

static int get_number(vmctx *ctx, xpath_lexer *l)
{
    int startidx = l->curidx;
    while (is_ascii_digit(l->cur)) {
        next_char(l);
    }
    if (l->cur == '.') {
        next_char(l);
        while (is_ascii_digit(l->cur)) {
            next_char(l);
        }
    }
    if ((l->cur & (~0x20)) == 'E') {
        return 0;
    }

    str_set(ctx, l->num, l->expr + startidx, l->curidx - startidx);
    return 1;
}

static void next_token(vmctx *ctx, xpath_lexer *l)
{

    l->prevend = l->cur;
    l->prevtype = l->token;
    skip_xpah_whitespace(l);
    l->start = l->curidx;

    switch (l->cur) {
    case 0:   l->token = XPATH_TK_EOF; return;
    case '(': l->token = XPATH_TK_LP;     next_char(l); break;
    case ')': l->token = XPATH_TK_RP;     next_char(l); break;
    case '[': l->token = XPATH_TK_LB;     next_char(l); break;
    case ']': l->token = XPATH_TK_RB;     next_char(l); break;
    case '@': l->token = XPATH_TK_AT;     next_char(l); break;
    case ',': l->token = XPATH_TK_COMMA;  next_char(l); break;
    case '$': l->token = XPATH_TK_DOLLAR; next_char(l); break;

    case '.':
        next_char(l);
        if (l->cur == '.') {
            l->token = XPATH_TK_DOTDOT;
            next_char(l);
        } else if (is_ascii_digit(l->cur)) {
            l->curidx = l->start - 1;
            next_char(l);
            goto CASE_0;
        } else {
            l->token = XPATH_TK_DOT;
        }
        break;
    case ':':
        next_char(l);
        if (l->cur == ':') {
            l->token = XPATH_TK_COLONCOLON;
            next_char(l);
        } else {
            l->token = XPATH_TK_UNKNOWN;
        }
        break;
    case '*':
        l->token = XPATH_TK_ASTA;
        next_char(l);
        check_operator(l, 1);
        break;
    case '/':
        next_char(l);
        if (l->cur == '/') {
            l->token = XPATH_TK_SLASHSLASH;
            next_char(l);
        } else {
            l->token = XPATH_TK_SLASH;
        }
        break;
    case '|':
        l->token = XPATH_TK_UNION;
        next_char(l);
        break;
    case '+':
        l->token = XPATH_TK_ADD;
        next_char(l);
        break;
    case '-':
        l->token = XPATH_TK_SUB;
        next_char(l);
        break;
    case '=':
        l->token = XPATH_TK_EQ;
        next_char(l);
        break;
    case '!':
        next_char(l);
        if (l->cur == '=') {
            l->token = XPATH_TK_NE;
            next_char(l);
        } else {
            l->token = XPATH_TK_UNKNOWN;
        }
        break;
    case '<':
        next_char(l);
        if (l->cur == '=') {
            l->token = XPATH_TK_LE;
            next_char(l);
        } else {
            l->token = XPATH_TK_LT;
        }
        break;
    case '>':
        next_char(l);
        if (l->cur == '=') {
            l->token = XPATH_TK_GE;
            next_char(l);
        } else {
            l->token = XPATH_TK_GT;
        }
        break;
    case '"':
    case '\'':
        l->token = XPATH_TK_STR;
        if (!get_string(ctx, l)) {
            return;
        }
        break;
    case '0':
CASE_0:;
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        l->token = XPATH_TK_NUM;
        if (!get_number(ctx, l)) {
            return;
        }
        break;
    default:
        get_ncname(ctx, l, l->name);
        if (l->name->len > 0) {
            l->token = XPATH_TK_NAME;
            str_clear(l->prefix);
            l->function = 0;
            l->axis = XPATH_AXIS_UNKNOWN;
            int colon2 = 0;
            int savedidx = l->curidx;
            if (l->cur == ':') {
                next_char(l);
                if (l->cur == ':') {
                    next_char(l);
                    colon2 = 1;
                    set_index(l, savedidx);
                } else {
                    vmstr ncname;
                    get_ncname(ctx, l, &ncname);
                    if (ncname.len > 0) {
                        str_append_cp(ctx, l->prefix, l->name->hd);
                        str_set_cp(ctx, l->name, ncname.hd);
                        savedidx = l->curidx;
                        skip_xpah_whitespace(l);
                        l->function = (l->cur == '(');
                        set_index(l, savedidx);
                    } else if (l->cur == '*') {
                        next_char(l);
                        str_append_cp(ctx, l->prefix, l->name->hd);
                        str_set_cp(ctx, l->name, "*");
                    } else {
                        set_index(l, savedidx);
                    }
                }
            } else {
                skip_xpah_whitespace(l);
                if (l->cur == ':') {
                    next_char(l);
                    if (l->cur == ':') {
                        next_char(l);
                        colon2 = 1;
                    }
                    set_index(l, savedidx);
                } else {
                    l->function = (l->cur == '(');
                }
            }
            if (!check_operator(l, 0) && colon2) {
                l->axis = check_axis(l);
            }
        } else {
            l->token = XPATH_TK_UNKNOWN;
            next_char(l);
        }
        break;
    }
}

/*
 * UnionExpr ::= PathExpr ('|' PathExpr)*
 * PathExpr ::= LocationPath | FilterExpr (('/' | '//') RelativeLocationPath )?
 * FilterExpr ::= PrimaryExpr Predicate*
 * PrimaryExpr ::= Literal | Number | VariableReference | '(' Expr ')' | FunctionCall
 * FunctionCall ::= FunctionName '(' (Expr (',' Expr)* )? ')'
 * 
 * LocationPath ::= RelativeLocationPath | '/' RelativeLocationPath? | '//' RelativeLocationPath
 * RelativeLocationPath ::= Step (('/' | '//') Step)*
 * Step ::= '.' | '..' | (AxisName '::' | '@')? NodeTest Predicate*
 * NodeTest ::= NameTest | ('comment' | 'text' | 'node') '(' ')' | 'processing-instruction' '('  Literal? ')'
 * NameTest ::= '*' | NCName ':' '*' | QName
 * Predicate ::= '[' Expr ']'
 * 
 * Expr   ::= OrExpr
 * OrExpr ::= AndExpr ('or' AndExpr)*
 * AndExpr ::= EqualityExpr ('and' EqualityExpr)*
 * EqualityExpr ::= RelationalExpr (('=' | '!=') RelationalExpr)*
 * RelationalExpr ::= AdditiveExpr (('<' | '>' | '<=' | '>=') AdditiveExpr)*
 * AdditiveExpr ::= MultiplicativeExpr (('+' | '-') MultiplicativeExpr)*
 * MultiplicativeExpr ::= UnaryExpr (('*' | 'div' | 'mod') UnaryExpr)*
 * UnaryExpr ::= ('-')* UnionExpr
 */

static int xpath_operator_priority[] = {
    /* XPATH_TK_UNKNOWN */ 0,
    /* XPATH_TK_LOR     */ 1,
    /* XPATH_TK_LAND    */ 2,
    /* XPATH_TK_EQ      */ 3,
    /* XPATH_TK_NE      */ 3,
    /* XPATH_TK_LT      */ 4,
    /* XPATH_TK_LE      */ 4,
    /* XPATH_TK_GT      */ 4,
    /* XPATH_TK_GE      */ 4,
    /* XPATH_TK_ADD     */ 5,
    /* XPATH_TK_SUB     */ 5,
    /* XPATH_TK_MUL     */ 6,
    /* XPATH_TK_DIV     */ 6,
    /* XPATH_TK_MOD     */ 6,
    /* XPATH_TK_UMINUS  */ 7,
    /* XPATH_TK_UNION   */ 8,
};

static void set_error(int line, xpath_lexer *l, int e)
{
    printf("XPath error at %d\n", line);
    l->e = e;
}

static vmvar *xpath_make_predicate(vmctx *ctx, vmvar *node, vmvar *condition, int reverse)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, isPredicate, 1);
    array_push(ctx, n, node);
    array_push(ctx, n, condition);
    array_push(ctx, n, alcvar_int64(ctx, reverse, 0));
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_axis(vmctx *ctx, int axis, int nodetype, vmvar *prefix, vmvar *name)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, isAxis, 1);
    array_push(ctx, n, alcvar_int64(ctx, axis, 0));
    array_push(ctx, n, alcvar_int64(ctx, nodetype, 0));
    array_push(ctx, n, prefix);
    array_push(ctx, n, name);
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_variable(vmctx *ctx, vmvar *prefix, vmvar *name)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, isVariable, 1);
    array_push(ctx, n, prefix);
    array_push(ctx, n, name);
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_function(vmctx *ctx, vmvar *prefix, vmvar *name, vmobj *args)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, isFunctionCall, 1);
    array_push(ctx, n, prefix);
    array_push(ctx, n, name);
    array_push(ctx, n, args ? alcvar_obj(ctx, args) : NULL);
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_binop(vmctx *ctx, int op, vmvar *lhs, vmvar *rhs)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, isExpr, 1);
    array_push(ctx, n, alcvar_int64(ctx, op, 0));
    array_push(ctx, n, lhs);
    array_push(ctx, n, rhs);
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_step(vmctx *ctx, vmvar *lhs, vmvar *rhs)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, isStep, 1);
    array_push(ctx, n, lhs);
    array_push(ctx, n, rhs);
    return alcvar_obj(ctx, n);
}

static int is_node_type(xpath_lexer *l)
{
    return l->prefix->len == 0 && (
        strcmp(l->name->hd, "node") == 0 || strcmp(l->name->hd, "text") == 0 ||
        strcmp(l->name->hd, "processing-instruction") == 0 || strcmp(l->name->hd, "comment") == 0
    );
}

static int is_step(int token)
{
    return
        token == XPATH_TK_DOT      ||
        token == XPATH_TK_DOTDOT   ||
        token == XPATH_TK_AT       ||
        token == XPATH_TK_AXISNAME ||
        token == XPATH_TK_ASTA     ||
        token == XPATH_TK_NAME
    ;
}

static int is_primary_expr(xpath_lexer *l)
{
    return
        l->token == XPATH_TK_STR ||
        l->token == XPATH_TK_NUM ||
        l->token == XPATH_TK_DOLLAR ||
        l->token == XPATH_TK_LP ||
        (l->token == XPATH_TK_NAME && l->function && !is_node_type(l))
    ;
}

static int is_reverse_axis(int axis)
{
    return
        axis == XPATH_AXIS_ANCESTOR         || axis == XPATH_AXIS_PRECEDING ||
        axis == XPATH_AXIS_ANCESTOR_OR_SELF || axis == XPATH_AXIS_PRECEDING_SIBLING
    ;
}

static int xpath_make_node_type(int axis)
{
    return axis == XPATH_AXIS_ATTRIBUTE ? XPATH_NODE_ATTRIBUTE :
        (axis == XPATH_AXIS_NAMESPACE ? XPATH_NODE_NAMESPACE : XPATH_NODE_ELEMENT);
}

static void push_posinfo(xpath_lexer *l, int startch, int endch)
{
    l->posinfo[l->stksz++] = startch; 
    l->posinfo[l->stksz++] = endch; 
}

static void pop_posinfo(xpath_lexer *l)
{
    l->stksz -= 2;
}

static int xpath_check_node_test(vmctx *ctx, xpath_lexer *l, int axis, int *node_type, vmstr *node_prefix, vmstr *node_name)
{
    switch (l->token) {
    case XPATH_TK_NAME:
        if (l->function && is_node_type(l)) {
            str_clear(node_prefix);
            str_clear(node_name);
            if      (strcmp(l->name->hd, "comment") == 0) *node_type = XPATH_NODE_COMMENT;
            else if (strcmp(l->name->hd, "text")    == 0) *node_type = XPATH_NODE_TEXT;
            else if (strcmp(l->name->hd, "node")    == 0) *node_type = XPATH_NODE_ALL;
            else                                          *node_type = XPATH_NODE_PROCESSIGN_INSTRUCTION;
            next_token(ctx, l);
            if (l->token != XPATH_TK_LP) {
                set_error(__LINE__, l, 1); /* Exception */
                return 0;
            }
            next_token(ctx, l);

            if (*node_type == XPATH_NODE_PROCESSIGN_INSTRUCTION) {
                if (l->token != XPATH_TK_RP) {
                    if (l->token != XPATH_TK_STR) {
                        set_error(__LINE__, l, 1); /* Exception */
                        return 0;
                    }
                    str_clear(node_prefix);
                    str_set_cp(ctx, node_name, l->str->hd);
                    next_token(ctx, l);
                }
            }

            if (l->token != XPATH_TK_RP) {
                set_error(__LINE__, l, 1); /* Exception */
                return 0;
            }
            next_token(ctx, l);
        } else {
            str_set_cp(ctx, node_prefix, l->prefix->hd);
            str_set_cp(ctx, node_name, l->name->hd);
            *node_type = xpath_make_node_type(axis);
            next_token(ctx, l);
            if (strcmp(node_name->hd, "*") == 0) {
                str_clear(node_name);
            }
        }
        break;
    case XPATH_TK_ASTA:
        str_clear(node_prefix);
        str_clear(node_name);
        *node_type = xpath_make_node_type(axis);
        next_token(ctx, l);
        break;
    default :
        set_error(__LINE__, l, 1); /* Exception */
    }
    return 1;
}

static vmvar *xpath_node_test(vmctx *ctx, xpath_lexer *l, int axis) {
    int node_type;
    vmstr *node_prefix = alcstr_str(ctx, "");
    vmstr *node_name = alcstr_str(ctx, "");

    vmvar *n = NULL;
    int startch = l->start;
    if (xpath_check_node_test(ctx, l, axis, &node_type, node_prefix, node_name)) {
        push_posinfo(l, startch, l->prevend);
        n = xpath_make_axis(ctx, axis, node_type, alcvar_str(ctx, node_prefix->hd), alcvar_str(ctx, node_name->hd));
        pop_posinfo(l);
    }

    pbakstr(ctx, node_name);
    pbakstr(ctx, node_prefix);
    return n;
}

static vmvar *xpath_process_expr(vmctx *ctx, xpath_lexer *l, int prec)
{
    int op = XPATH_TK_UNKNOWN;
    vmvar *n = NULL;

    // Unary operators
    if (l->token == XPATH_TK_SUB) {
        op = XPATH_TK_UMINUS;
        int opprec = xpath_operator_priority[op];
        next_token(ctx, l);
        vmvar *n2 =  xpath_process_expr(ctx, l, opprec);
        if (!n2) return NULL;
        n = xpath_make_binop(ctx, op, n2, NULL);
    } else {
        n = xpath_union_expr(ctx, l);
        if (!n) return NULL;
    }

    // Binary operators
    for ( ; ; ) {
        op = (l->token <= XPATH_TK_LAST_OPERATOR) ? l->token : XPATH_TK_UNKNOWN;
        int opprec = xpath_operator_priority[op];
        if (opprec <= prec) {
            break;
        }

        next_token(ctx, l);
        vmvar *n2 =  xpath_process_expr(ctx, l, opprec);
        if (!n2) return NULL;
        n = xpath_make_binop(ctx, op, n, n2);
    }

    return n;
}

static vmvar *xpath_expr(vmctx *ctx, xpath_lexer *l)
{
    return xpath_process_expr(ctx, l, 0);
}

static vmvar *xpath_predicate(vmctx *ctx, xpath_lexer *l)
{
    if (l->token != XPATH_TK_LB) {
        set_error(__LINE__, l, 1); /* Exception */
        return NULL;
    }
    next_token(ctx, l);
    vmvar *n = xpath_expr(ctx, l);
    if (!n) return NULL;
    if (l->token != XPATH_TK_RB) {
        set_error(__LINE__, l, 1); /* Exception */
        return NULL;
    }
    next_token(ctx, l);
    return n;
}

static vmvar *xpath_step(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    if (l->token == XPATH_TK_DOT) {
        next_token(ctx, l);
        n = xpath_make_axis(ctx, XPATH_AXIS_SELF, XPATH_NODE_ALL, NULL, NULL);
    } else if (l->token == XPATH_TK_DOTDOT) {
        next_token(ctx, l);
        n = xpath_make_axis(ctx, XPATH_AXIS_PARENT, XPATH_NODE_ALL, NULL, NULL);
    } else {
        int axis;
        switch (l->token) {
        case XPATH_TK_AXISNAME:
            axis = l->axis;
            next_token(ctx, l);
            next_token(ctx, l);
            break;
        case XPATH_TK_AT:
            axis = XPATH_AXIS_ATTRIBUTE;
            next_token(ctx, l);
            break;
        case XPATH_TK_NAME:
        case XPATH_TK_ASTA:
            axis = XPATH_AXIS_CHILD;
            break;
        default:
            set_error(__LINE__, l, 1); /* Exception */
            return NULL;
        }
        n = xpath_node_test(ctx, l, axis);
        if (!n) return NULL;

        while (l->token == XPATH_TK_LB) {
            vmvar *n2 = xpath_predicate(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_predicate(ctx, n, n2, is_reverse_axis(axis));
        }
    }

    return n;
}

static vmvar *xpath_relative_location_path(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = xpath_step(ctx, l);
    if (!n) return NULL;
    if (l->token == XPATH_TK_SLASH) {
        next_token(ctx, l);
        vmvar *n2 = xpath_relative_location_path(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_step(ctx, n, n2);
    } else if (l->token == XPATH_TK_SLASHSLASH) {
        next_token(ctx, l);
        vmvar *n2 = xpath_relative_location_path(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_step(ctx, n,
            xpath_make_step(ctx,
                xpath_make_axis(ctx, XPATH_AXIS_DESCENDANT_OR_SELF, XPATH_NODE_ALL, NULL, NULL), n2
            )
        );
    }
    return n;
}

static vmvar *xpath_location_path(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    if (l->token == XPATH_TK_SLASH) {
        next_token(ctx, l);
        n = xpath_make_axis(ctx, XPATH_AXIS_ROOT, XPATH_NODE_ALL, NULL, NULL);
        if (is_step(l->token)) {
            vmvar *n2 = xpath_relative_location_path(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_step(ctx, n, n2);
        }
    } else if (l->token == XPATH_TK_SLASHSLASH) {
        next_token(ctx, l);
        vmvar *n2 = xpath_relative_location_path(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_step(ctx,
            xpath_make_axis(ctx, XPATH_AXIS_ROOT, XPATH_NODE_ALL, NULL, NULL),
            xpath_make_step(ctx,
                xpath_make_axis(ctx, XPATH_AXIS_DESCENDANT_OR_SELF, XPATH_NODE_ALL, NULL, NULL), n2
            )
        );
    } else {
        n = xpath_relative_location_path(ctx, l);
        if (!n) return NULL;
    }
    return n;
}

static vmvar *xpath_function_call(vmctx *ctx, xpath_lexer *l)
{
    vmobj *args = alcobj(ctx);
    vmvar *name = alcvar_str(ctx, l->name->hd);
    vmvar *prefix = alcvar_str(ctx, l->prefix->hd);
    int startch = l->start;

    if (l->token != XPATH_TK_NAME) {
        set_error(__LINE__, l, 1); /* Exception */
        return NULL;
    }
    next_token(ctx, l);
    if (l->token != XPATH_TK_LP) {
        set_error(__LINE__, l, 1); /* Exception */
        return NULL;
    }
    next_token(ctx, l);

    if (l->token != XPATH_TK_RP) {
        for ( ; ; ) {
            array_push(ctx, args, xpath_expr(ctx, l));
            if (l->token != XPATH_TK_COMMA) {
                if (l->token != XPATH_TK_RP) {
                    set_error(__LINE__, l, 1); /* Exception */
                    return NULL;
                }
                break;
            }
            next_token(ctx, l);
        }
    }

    next_token(ctx, l);
    push_posinfo(l, startch, l->prevend);
    vmvar *n = xpath_make_function(ctx, prefix, name, args);
    pop_posinfo(l);
    return n;
}

static vmvar *xpath_primary_expr(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    switch (l->token) {
    case XPATH_TK_STR:
        n = alcvar_str(ctx, l->str->hd);
        next_token(ctx, l);
        break;
    case XPATH_TK_NUM:
        n = alcvar_str(ctx, l->num->hd);
        next_token(ctx, l);
        break;
    case XPATH_TK_DOLLAR: {
        int startch = l->start;
        next_token(ctx, l);
        if (l->token != XPATH_TK_NAME) {
            set_error(__LINE__, l, 1); /* Exception */
            return NULL;
        }
        push_posinfo(l, startch, l->curidx);
        n = xpath_make_variable(ctx, alcvar_str(ctx, l->prefix->hd), alcvar_str(ctx, l->name->hd));
        pop_posinfo(l);
        next_token(ctx, l);
        break;
    }
    case XPATH_TK_LP:
        next_token(ctx, l);
        n = xpath_expr(ctx, l);
        if (l->token != XPATH_TK_RP) {
            set_error(__LINE__, l, 1); /* Exception */
            return NULL;
        }
        next_token(ctx, l);
        break;
    default:
        n = xpath_function_call(ctx, l);
        break;
    }
    return n;
}

static vmvar *xpath_filter_expr(vmctx *ctx, xpath_lexer *l)
{
    int startch = l->start;
    vmvar *n = xpath_primary_expr(ctx, l);
    int endch = l->prevend;

    while (n && l->token == XPATH_TK_LB) {
        push_posinfo(l, startch, endch);
        vmvar *n2 = xpath_predicate(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_predicate(ctx, n, n2, 0);
        pop_posinfo(l);
    }

    return n;
}

static vmvar *xpath_path_expr(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    if (is_primary_expr(l)) {
        int startch = l->start;
        n = xpath_filter_expr(ctx, l);
        if (!n) return NULL;
        int endch = l->prevend;

        if (l->token == XPATH_TK_SLASH) {
            next_token(ctx, l);
            push_posinfo(l, startch, endch);
            vmvar *n2 = xpath_relative_location_path(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_step(ctx, n, n2);
            pop_posinfo(l);
        } else if (l->token == XPATH_TK_SLASHSLASH) {
            next_token(ctx, l);
            push_posinfo(l, startch, endch);
            vmvar *n2 = xpath_relative_location_path(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_step(ctx, n,
                xpath_make_step(ctx,
                    xpath_make_axis(ctx, XPATH_AXIS_DESCENDANT_OR_SELF, XPATH_NODE_ALL, NULL, NULL), n2
                )
            );
            pop_posinfo(l);
        }
    } else {
        n = xpath_location_path(ctx, l);
    }
    return n;
}

static vmvar *xpath_union_expr(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = xpath_path_expr(ctx, l);
    while (n && l->token == XPATH_TK_UNION) {
        next_token(ctx, l);
        vmvar *n2 = xpath_path_expr(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_binop(ctx, XPATH_TK_UNION, n, n2);
    }
    return n;
}

static vmvar *xpath_parse(vmctx *ctx, xpath_lexer *l)
{
    return xpath_expr(ctx, l);
}

static int xpath_compile(vmctx *ctx, vmvar *r, const char *expr)
{
    xpath_lexer lexer = {
        .name = alcstr_str(ctx, ""),
        .prefix = alcstr_str(ctx, ""),
        .str = alcstr_str(ctx, ""),
        .num = alcstr_str(ctx, ""),
        .expr = expr,
        .exprlen = strlen(expr),
        .token = XPATH_TK_UNKNOWN,
        .curidx = -1,
    };
    next_char(&lexer);
    next_token(ctx, &lexer);
    vmvar *root = xpath_parse(ctx, &lexer);
    SHCOPY_VAR_TO(ctx, r, root);
    return lexer.e;
}

static int XPath_compile(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    return xpath_compile(ctx, r, a0->s->hd);
}

static int XPath_opName(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_INT64);
    switch (a0->i) {
    case XPATH_TK_LOR:    SET_STR(r, "or");      break;
    case XPATH_TK_LAND:   SET_STR(r, "and");     break;
    case XPATH_TK_EQ:     SET_STR(r, "eq");      break;
    case XPATH_TK_NE:     SET_STR(r, "ne");      break;
    case XPATH_TK_LT:     SET_STR(r, "lt");      break;
    case XPATH_TK_LE:     SET_STR(r, "le");      break;
    case XPATH_TK_GT:     SET_STR(r, "gt");      break;
    case XPATH_TK_GE:     SET_STR(r, "ge");      break;
    case XPATH_TK_ADD:    SET_STR(r, "add");     break;
    case XPATH_TK_SUB:    SET_STR(r, "sub");     break;
    case XPATH_TK_MUL:    SET_STR(r, "mul");     break;
    case XPATH_TK_DIV:    SET_STR(r, "div");     break;
    case XPATH_TK_MOD:    SET_STR(r, "mod");     break;
    case XPATH_TK_UMINUS: SET_STR(r, "uminus");  break;
    case XPATH_TK_UNION:  SET_STR(r, "union");   break;
    default:              SET_STR(r, "UNKNOWN"); break;
    }
    return 0;
}

static int XPath_axisName(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_INT64);
    switch (a0->i) {
    case XPATH_AXIS_ANCESTOR:           SET_STR(r, "ancestor");           break;
    case XPATH_AXIS_ANCESTOR_OR_SELF:   SET_STR(r, "ancestor-or-self");   break;
    case XPATH_AXIS_ATTRIBUTE:          SET_STR(r, "attribute");          break;
    case XPATH_AXIS_CHILD:              SET_STR(r, "child");              break;
    case XPATH_AXIS_DESCENDANT:         SET_STR(r, "descendant");         break;
    case XPATH_AXIS_DESCENDANT_OR_SELF: SET_STR(r, "descendant-or-self"); break;
    case XPATH_AXIS_FOLLOWING:          SET_STR(r, "following");          break;
    case XPATH_AXIS_FOLLOWING_SIBLING:  SET_STR(r, "following-sibling");  break;
    case XPATH_AXIS_NAMESPACE:          SET_STR(r, "namespace");          break;
    case XPATH_AXIS_PARENT:             SET_STR(r, "parent");             break;
    case XPATH_AXIS_PRECEDING:          SET_STR(r, "preceding");          break;
    case XPATH_AXIS_PRECEDING_SIBLING:  SET_STR(r, "preceding-sibling");  break;
    case XPATH_AXIS_SELF:               SET_STR(r, "self");               break;
    case XPATH_AXIS_ROOT:               SET_STR(r, "root");               break;
    default:                            SET_STR(r, "UNKNOWN");            break;
    }
    return 0;
}

static int XPath_nodeTypeName(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_INT64);
    switch (a0->i) {
    case XPATH_NODE_ROOT:                   SET_STR(r, "root");                   break;
    case XPATH_NODE_ELEMENT:                SET_STR(r, "element");                break;
    case XPATH_NODE_ATTRIBUTE:              SET_STR(r, "attribute");              break;
    case XPATH_NODE_NAMESPACE:              SET_STR(r, "namespace");              break;
    case XPATH_NODE_TEXT:                   SET_STR(r, "text");                   break;
    case XPATH_NODE_SIGNIFICANT_WHITESPACE: SET_STR(r, "significant-whitespace"); break;
    case XPATH_NODE_WHITESPACE:             SET_STR(r, "whitespace");             break;
    case XPATH_NODE_PROCESSIGN_INSTRUCTION: SET_STR(r, "processing-instruction"); break;
    case XPATH_NODE_COMMENT:                SET_STR(r, "comment");                break;
    case XPATH_NODE_ALL:                    SET_STR(r, "all");                    break;
    default:                                SET_STR(r, "UNKNOWN");                break;
    }
    return 0;
}

int XPath(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, create, XPath_compile, lex, 1);
    KL_SET_METHOD(o, compile, XPath_compile, lex, 1);
    KL_SET_METHOD(o, opName, XPath_opName, lex, 1);
    KL_SET_METHOD(o, axisName, XPath_axisName, lex, 1);
    KL_SET_METHOD(o, nodeTypeName, XPath_nodeTypeName, lex, 1);
    SET_OBJ(r, o);
    return 0;
}
