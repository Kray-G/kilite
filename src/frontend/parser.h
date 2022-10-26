#ifndef KILITE_PARSER_H
#define KILITE_PARSER_H

#include "../kir.h"
#include "lexer.h"

#define HASHSIZE (23)

typedef struct kl_conststr {
    char *str;
    struct kl_conststr *next;       //  For hashtable.
    struct kl_conststr *chn;        //  For memory allocation control.
} kl_conststr;

struct kl_expr;
struct kl_stmt;

typedef struct kl_symbol {
    const char *name;               //  Symbol's name like a function name, a class name, a variable name, or somthing.
    int has_func;                   //  If 1, this symbol's scope has an internal function. If 0, this func doesn't need a frame.
    int is_callable;                //  The callable symbol, which is like TK_FUNC, TK_CLASS, or something.
    int is_recursive;               //  This reference symbol could be recursive call. ref->is_callable should be 1.
    int count;                      //  Variable counter in this scope.
    int level;                      //  The level of lexical scope, which means a distance from the scope having the 'ref' symbol.
    int index;                      //  The index of this symbol.
    int idxmax;                     //  The current index max.
    tk_typeid symtype;              //  TK_VAR, TK_NAMESPACE, TK_CLASS, TK_MODULE, TK_FUNC, TK_PRIVATE, TK_PROTECTED, or TK_PUBLIC.
    tk_typeid type;                 //  Return type if this is a function. The variable type if this is a variable.

    struct kl_expr *base;           //  Base class expression when it's TK_CLASS.
    struct kl_expr *args;           //  Constructor or function's arguments.
    struct kl_stmt *body;           //  Class/module/function body.

    struct kl_symbol *scope;        //  Lexical scope chain.
    struct kl_symbol *method;       //  Method symbol chain in the current scope when it's TK_NAMESPACE, TK_CLASS, or TK_MODULE.
    struct kl_symbol *next;         //  Symbol chain in the current namespace.

    struct kl_expr *typ;            //  Type expression.
    struct kl_symbol *ref;          //  Reference to the symbol defined earlier.
    struct kl_symbol *chn;          //  For memory allocation control.
} kl_symbol;

typedef struct kl_nsstack {
    const char *name;               //  The name which is the base of stack.
    int scopetype;                  //  The scope type of the namespace.
    kl_symbol *list;                //  Symbol list in this namespace stack.
    struct kl_nsstack *prev;        //  Stack of namespace, which is from the top to the bottom.
    struct kl_nsstack *chn;         //  For memory allocation control.
} kl_nsstack;

typedef struct kl_expr {
    tk_token nodetype;              //  Operator type like TK_ADD, the value type like TK_VSINT, or TK_LET for variable.
    tk_typeid typeid;               //  Type of this node.
    kl_symbol *sym;                 //  The symbol of this node.
    union {
        int64_t i64;
        uint64_t u64;
        double dbl;
        const char *str;
        const char *big;
    } val;

    struct kl_expr *lhs;            //  Left hand side.
    struct kl_expr *rhs;            //  Right hand side.
    struct kl_expr *xhs;            //  Additional expr for ternary expression.
    struct kl_expr *chn;            //  For memory allocation control.
} kl_expr;

typedef struct kl_stmt {
    tk_token nodetype;              //  Statement type like TK_IF or somthing.
    tk_typeid typeid;               //  Type of this node.
    kl_symbol *sym;                 //  The symbol of this node.
    struct kl_expr *e1, *e2, *e3;   //  Statement can hold at least 3 expressions for conditions.
    struct kl_stmt *s1, *s2, *s3;   //  Statement can hold at least 3 statements inside.
    struct kl_stmt *next;           //  The pointer to next statement.
    struct kl_stmt *chn;            //  For memory allocation control.
} kl_stmt;

typedef struct kl_context {
    int errors;                     //  Total error count.
    int options;                    //  Options for parser.
    int in_decl;                    //  Parsing declaration.
    int in_lvalue;                  //  Parsing l-value.
    kl_stmt *head;                  //  The head of object tree.
    kl_nsstack *ns;                 //  Namespace stack.
    kl_symbol *scope;               //  The symbol of a current scope.
    kl_symbol *global;              //  The symbol of the global scope.
    kl_conststr *hash[HASHSIZE];    //  Hashtable of constant string.

    int labelid;                    //  Current max value of label index.
    kl_kir_program *program;        //  KIR output program.

    kl_symbol *symchn;              //  For memory allocation control.
    kl_expr *exprchn;               //  For memory allocation control.
    kl_stmt *stmtchn;               //  For memory allocation control.
    kl_nsstack *nsstchn;            //  For memory allocation control.
} kl_context;

extern unsigned int hash(const char *s);
extern char *const_str(kl_context *ctx, const char *phase, int line, int pos, int len, const char *str);
extern kl_context *parser_new_context(void);
extern kl_stmt *copy_tree(kl_context *ctx, kl_stmt *src);
extern int parse(kl_context *ctx, kl_lexer *l);
extern void free_context(kl_context *ctx);

#define PARSER_OPT_PHASE (0x01)

#endif /* KILITE_PARSER_H */
