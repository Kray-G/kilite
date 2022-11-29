#ifndef KILITE_KIR_H
#define KILITE_KIR_H

#include <stdio.h>
#include <stdint.h>

#define SHOW_BIGINT(b) do {char *bs = BzToString(b, 10, 0); printf("%s\n", bs); BzFreeString(bs);} while(0)
// printf("%s:%d -> %s\n", __FILE__, __LINE__, __func__);

/*
 * Available tokens in the Kilite language.
 */
typedef enum tk_typeid {
    // Types
    TK_TANY = 0,            //  <any type>
    TK_TUNDEF,              //  undefined
    TK_TSINT64,             //  int64
    TK_TBIGINT,             //  bigint
    TK_TDBL,                //  real
    TK_TSTR,                //  string
    TK_TBIN,                //  binary
    TK_TOBJ,                //  object or array
    TK_TFUNC,               //  function

    TK_TCLASSBASE,          //  The typename value of class will be started from this value.
} tk_typeid;

/* '*' means that the implementation of parsing has been done. */
typedef enum tk_token {
    TK_EOF = 0,
    TK_UNKNOWN,

    // Literals
    TK_VSINT,               //  * Integer value as signed 64 bit.
    TK_VBIGINT,             //  * Big Integer value
    TK_VDBL,                //  * Double precision value
    TK_VSTR,                //  * String literal
    TK_VBIN,                //  * Binary literal
    TK_VARY,                //  * Array literal.
    TK_VOBJ,                //  * Object literal.
    TK_VKV,                 //  * Key value literal.

    // Keywords
    TK_EXTERN,              //    extern
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
    TK_NATIVE,              //    native
    TK_SWITCH,              //    switch
    TK_CASE,                //    case
    TK_DEFAULT,             //    default
    TK_IF,                  //  * if
    TK_ELSE,                //  * else
    TK_DO,                  //  * do
    TK_WHILE,               //  * while
    TK_FOR,                 //  * for
    TK_IN,                  //    in
    TK_FORIN,               //    for-in
    TK_RETURN,              //  * return
    TK_WHEN,                //    when
    TK_BREAK,               //    break
    TK_CONTINUE,            //    continue
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
    TK_POWEQ,               //  * **=
    TK_LSHEQ,               //  * <<=
    TK_RSHEQ,               //  * >>=
    TK_LANDEQ,              //  * &&=
    TK_LOREQ,               //  * ||=
    TK_NULLCEQ,             //  * ??=
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
    TK_POW,                 //  * **
    TK_LSH,                 //  * <<
    TK_RSH,                 //  * >>
    TK_LAND,                //  * &&
    TK_LOR,                 //  * ||
    TK_NULLC,               //  * ??
    TK_INC,                 //  * ++x
    TK_INCP,                //  * x++ (postfix)
    TK_DEC,                 //  * --x
    TK_DECP,                //  * x-- (postfix)

    // Punctuations
    TK_COMMA,               //  ,
    TK_COLON,               //  :
    TK_DCOLON,              //  ::
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
    TK_LABEL,               //  The label statement.
    TK_BLOCK,               //  For block.
    TK_CONNECT,             //  For connected expressions.
    TK_VAR,                 //  For variable.
    TK_MINUS,               //  Unary minus.
    TK_EXPR,                //  For expression type.
    TK_CALL,                //  For function call.
    TK_IDX,                 //  For array index reference.
    TK_TYPENODE,            //  The node will represent the type.
    TK_MKSUPER,             //  The mark of making `super` object.
    TK_BINEND,              //  The mark for the end of a binary literal.
    TK_RANGE2,              //  The range object with dot2.
    TK_RANGE3,              //  The range object with dot3.
    TK_COMMENT1,            //  1 line comment.
    TK_COMMENTX,            //  Multi lines comment.

    // Specialized operation
    TK_ARYSIZE,
} tk_token;

/* This is a temporary IR structure only used to convert from AST to C code. */
typedef enum kl_kir {
    KIR_UNKNOWN = 0,
    KIR_NOP,
    /*
     * The code for function frames.
     *  either KIR_MKFRM or KIR_LOCAL could be used, but those are exclusive.
     */
    KIR_EXTERN,     //  <name>                  ;   load the external function in C.
    KIR_FUNC,       //  <name>                  ;   just an entry point of the funtion.
    KIR_ALOCAL,     //  <n>                     ;   allocate <n> local vars on stack, without creating a frame.
    KIR_RLOCAL,     //  -                       ;   reduce the stack to make the size back.
    KIR_MKFRM,      //  <n>                     ;   create a frame of this function with <n> local vars, and push it.
    KIR_POPFRM,     //  -                       ;   pop the frame. this is needed only when the frame has been created.
    KIR_SETARG,     //  <n>, <idx>              ;   The argument <idx> will be set to the vriable <n>.
    KIR_SETARGL,    //  <n>, <idx>              ;   The arguments from <idx> will be set to the vriable <n>.
    KIR_PURE,       //  -                       ;   Output a pure function code if possible.

    /*
     * Function call, which should work like this.
     *  -> save vstackp
     *  -> push arguments
     *  -> call function
     *  -> restore vstackp
     */
    KIR_SVSTKP,     //  -                       ;   save the current stack pointer.
    KIR_PUSHSYS,    //  <r>                     ;   push <r> as an argument when it's system object.
    KIR_PUSHARG,    //  <r>                     ;   push <r> as an argument.
    KIR_CALL,       //  <r1>, <r2>              ;   <r1>  <-  call <r2>
    KIR_CHKEXCEPT,  //  -                       ;   check exception and throw it if exists after function call.
    KIR_RSSTKP,     //  -                       ;   restore the saved stack pointer.

    /*
     * Return from a function.
     */
    KIR_RET,        //  -                       ;   return from the function.
    KIR_THROWE,     //  <r1>                    ;   throw <r1>.
    KIR_THROW,      //  -                       ;   throw.
    KIR_CATCH,      //  <r1>                    ;   <r1>  <- catch.
    KIR_YIELDC,     //  ...                     ;   transfer yield condition if it's yield.
    KIR_YIELD,      //  <n>                     ;   yield with the number <n>.
    KIR_RESUME,     //  <r1>                    ;   <r1> <- resume.

    /*
     * Conditional and unconditional Jump.
     */
    KIR_JMPIFT,     //  <r1>, <label>           ;   if <r1> is true, it will jump to the label.
    KIR_JMPIFF,     //  <r1>, <label>           ;   if <r1> is false, it will jump to the label.
    KIR_JMP,        //  <label>                 ;   always jump to the label.
    KIR_LABEL,      //  <label>:                ;   The label location.

    /*
     * Operators.
     *  <r*> is the value with a type.
     */
    KIR_NOT,        //  <r1>, <r2>              ;   <r1>  <-  !<r2>
    KIR_MOV,        //  <r1>, <r2>              ;   <r1>  <-  <r2>
    KIR_MOVA,       //  <r1>, <r2>              ;   *<r1> <-  <r2>
    KIR_ADD,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> + <r3>
    KIR_SUB,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> - <r3>
    KIR_MUL,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> * <r3>
    KIR_DIV,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> / <r3>
    KIR_MOD,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> % <r3>
    KIR_POW,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> ** <r3>
    KIR_BSHL,       //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> << <r3>
    KIR_BSHR,       //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> >> <r3>
    KIR_BNOT,       //  <r1>, <r2>              ;   <r1>  <-  ~<r2>
    KIR_BAND,       //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> & <r3>
    KIR_BOR,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> | <r3>
    KIR_BXOR,       //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> ^ <r3>

    KIR_EQEQ,       //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> == <r3>
    KIR_NEQ,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> != <r3>
    KIR_LT,         //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> < <r3>
    KIR_LE,         //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> <= <r3>
    KIR_GT,         //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> > <r3>
    KIR_GE,         //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> => <r3>
    KIR_LGE,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> <=> <r3>

    KIR_INC,        //  <r1>, <r2>              ;   <r1>  <-  inc <r2>
    KIR_INCP,       //  <r1>, <r2>              ;   <r1>  <-  <r2>,  inc <r2>
    KIR_DEC,        //  <r1>, <r2>              ;   <r1>  <-  dec <r2>
    KIR_DECP,       //  <r1>, <r2>              ;   <r1>  <-  <r2>,  dec <r2>
    KIR_MINUS,      //  <r1>, -<r2>             ;   <r1>  <-  -<r2>

    KIR_NEWOBJ,     //  <r1>                    ;   <r1>  <-  new obj
    KIR_OBJCPY,     //  <r1>, <r2>              ;   <r1>  <-  <r2> as copy object.
    KIR_MKSUPER,    //  <r1>                    ;   <r1>  <-  `super` object
    KIR_IDXFRM,     //  <r1>, <r2>, <idx>       ;   <r1>  <-  <r2>[idx..]
    KIR_IDX,        //  <r1>, <r2>, <idx>       ;   <r1>  <-  <r2>[<idx>]
    KIR_IDXL,       //  <r1>, <r2>, <idx>       ;   <r1>  <-  <r2>[<idx>]
    KIR_APLY,       //  <r1>, <r2>, <str>       ;   <r1>  <-  <r2>.<str>
    KIR_APLYL,      //  <r1>, <r2>, <str>       ;   <r1>  <-  &<r2>.<str>
    KIR_REMOVE,     //  <r1>, <str>             ;   <r1>.remove(<str>)
    KIR_CHKMATCH,   //  <r1>, <r2>              ;   if <r1> != <r2>, throw NoMatchingPatternException.
    KIR_CHKRANGE,   //  <r1>, <r2>              ;   if !<r1>.includes(<r2>), throw NoMatchingPatternException.

    KIR_TYPE,       //  <r1>, <r2>, n           ;   <r1>  <-  true if <r2> type == n
    KIR_GETITER,    //  <r1>                    ;   <r1>  <-  get_iterator(<r1>).
    KIR_JMPIFNE,    //  <r1>, <label>           ;   !<r1>.isEnded() will make it jump to the label.

    KIR_SWITCHS,    //  <r1>, <r2>              ;   <r1>  <-  <r2>, and switch <r1>.
    KIR_SWITCHE,    //                          ;   the end of switch.
    KIR_CASEI,      //  <n>                     ;   switch case label <n> as an integer.
    KIR_CASEV,      //  <r1>, <r2>, <label>     ;   <r1> == <r2>, then goto label.
    KIR_DEFAULT,    //                          ;   default label in switch.

    /* Special instructions */
    KIR_SWAP,       //  <r1>, <r2>              ;   <r1>  <-> <r2>
    KIR_SWAPA,      //  <r1>, <r2>              ;   &<r1> <-> &<r2>
    KIR_RANGEF,     //  <r1>, <r2>, <r3>        ;   <r1>  <-  new Range(<r2>, <r3>, true)
    KIR_RANGET,     //  <r1>, <r2>, <r3>        ;   <r1>  <-  new Range(<r2>, <r3>, false)
    KIR_ARYSIZE,    //  <r1>, <r2>              ;   <r1>  <-  <r2>.size()
} kl_kir;

/* These should be the same as the one in the template/header.h */
typedef enum vartype {
    VAR_UNDEF = 0x00,
    VAR_INT64,
    VAR_BIG,
    VAR_DBL,
    VAR_STR,
    VAR_BIN,
    VAR_OBJ,
    VAR_FNC,
    VAR_ARY,
    VAR_DEF,
    VAR_VOIDP,
} vartype;

typedef struct kl_kir_opr {
    int t;                      //  The type of this value.
    int index;                  //  The variable index.
    int level;                  //  The lexical frame level.
    int args;                   //  The argument count when calling the function.
    int funcid;                 //  The function id to call.
    int recursive;              //  1 if the recursive call.
    int has_dot3;               //  This operand has 3 dots, it means
                                //      the variable should be an array of rest elements, or
                                //      the variable has to be expanded to multiple aruguments for function call.
    int callcnt;                //  The call count number to identify the call.
    int prevent;                //  Prevent an assignment to the variable.
    int64_t i64;
    const char *dbl;
    const char *str;
    tk_typeid typeid;           //  The value of the type.
    const char *typestr;        //  The type name as a string.
    const char *name;           //  The name if necessary.
} kl_kir_opr;

typedef struct kl_kir_inst {
    int disabled;
    kl_kir opcode;
    const char *funcname;
    const char *filename;
    const char *str;
    int labelid;
    int catchid;
    int gcable;
    int line;
    int pos;
    kl_kir_opr r1, r2, r3;

    struct kl_kir_inst *next;
    struct kl_kir_inst *chn;
} kl_kir_inst;

typedef struct kl_kir_func {
    const char *name;
    const char *funcname;
    int is_global;
    int is_pure;
    int argcount;
    int has_frame;
    int has_dot3;
    int funcid;
    int funcend;
    int yield;
    int vars;
    int line;
    int pos;
    kl_kir_inst *head;
    kl_kir_inst *last;

    struct kl_kir_func *next;
    struct kl_kir_func *chn;
} kl_kir_func;

typedef struct kl_kir_program {
    kl_kir_func *head;
    kl_kir_func *last;
    int verbose;
    int print_result;

    struct kl_kir_inst *ichn;
    struct kl_kir_func *fchn;
} kl_kir_program;

#endif /* KILITE_KIR_H */
