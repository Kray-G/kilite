#ifndef KILITE_KIR_H
#define KILITE_KIR_H

#include <stdint.h>

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

typedef enum kl_kir {
    KIR_UNKNOWN = 0,
    /*
     * The code for function frames.
     *  either KIR_MKFRM or KIR_LOCAL could be used, but those are exclusive.
     */
    KIR_FUNC,       //  <name>                  ;   just an entry point of the funtion.   
    KIR_LOCAL,      //  <n>                     ;   allocate <n> local vars on stack, without creating a frame.
    KIR_MKFRM,      //  <n>                     ;   create a frame of this function with <n> local vars, and push it.
    KIR_POPFRM,     //  -                       ;   pop the frame. this is needed only when the frame has been created.

    /*
     * Function call, which should work like this.
     *  -> save vstackp
     *  -> push arguments
     *  -> call function
     *  -> restore vstackp
     */
    KIR_SVSTKP,     //  -                       ;   save the current stack pointer.
    KIR_PUSHARG,    //  <r>                     ;   push <r> as an argument.
    KIR_CALL,       //  <r1>, <r2>              ;   <r1>  <-  call <r2>
    KIR_RSSTKP,     //  -                       ;   restore the saved stack pointer.

    /*
     * Return from a function.
     */
    KIR_RET,        //  -                       ;   return from the function.

    /*
     * Conditional and unconditional Jump.
     */
    KIR_JMPIFF,     //  <r1>, <label>           ;   if <r1> is false, it will jump to the label.
    KIR_JMP,        //  <label>                 ;   always jump to the label.
    KIR_LABEL,      //  <label>:                ;   The label location.

    /*
     * Operators.
     *  <r*> is the value with a type.
     */
    KIR_MOV,        //  <r1>, <r2>              ;   <r1>  <-  <r2>
    KIR_ADD,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> + <r3>
    KIR_SUB,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> - <r3>
    KIR_MUL,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> * <r3>
    KIR_DIV,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> / <r3>
    KIR_MOD,        //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> % <r3>
    KIR_LT,         //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> < <r3>
    KIR_GT,         //  <r1>, <r2>, <r3>        ;   <r1>  <-  <r2> > <r3>

    KIR_APLY,       //  <r1>, <r2>, <str>       ;   <r1>  <-  <r2>.<str>

} kl_kir;

typedef struct kl_kir_opr {
    int t;                      //  The type of this value.
    int index;                  //  The variable index.
    int level;                  //  The lexical frame level.
    int64_t i64;
    int64_t u64;
    tk_typeid typeid;           //  The value of the type.
    const char *typestr;        //  The type name as a string.
} kl_kir_opr;

typedef struct kl_kir_inst {
    kl_kir opcode;
    const char *str;
    int labelid;
    kl_kir_opr r1, r2, r3;

    struct kl_kir_inst *next;
} kl_kir_inst;

typedef struct kl_kir_func {
    const char *name;
    int vars;
    kl_kir_inst *head;
    kl_kir_inst *last;

    struct kl_kir_func *next;
} kl_kir_func;

typedef struct kl_kir_program {
    kl_kir_func *head;
    kl_kir_func *last;
} kl_kir_program;

#endif /* KILITE_KIR_H */
