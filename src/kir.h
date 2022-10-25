#ifndef KILITE_KIR_H
#define KILITE_KIR_H

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
    KIR_ARG,        //  <r>                     ;   push <r> as an argument.
    KIR_CALL,       //  <r>                     ;   call <r>
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

    /*
     * Operators.
     *  <r*> is the value with a type.
     */
    KIR_MOV,        //  <r1>, <r2>              ;   <r1> = <r2>
    KIR_ADD,        //  <r1>, <r2>, <r3>        ;   <r1> = <r2> + <r3>
    KIR_SUB,        //  <r1>, <r2>, <r3>        ;   <r1> = <r2> - <r3>
    KIR_MUL,        //  <r1>, <r2>, <r3>        ;   <r1> = <r2> * <r3>
    KIR_DIV,        //  <r1>, <r2>, <r3>        ;   <r1> = <r2> / <r3>
    KIR_MOD,        //  <r1>, <r2>, <r3>        ;   <r1> = <r2> % <r3>
    KIR_LT,         //  <r1>, <r2>, <r3>        ;   <r1> = <r2> < <r3>
    KIR_GT,         //  <r1>, <r2>, <r3>        ;   <r1> = <r2> > <r3>

} kl_kir;

typedef struct kl_kir_opr {
    int index;                  //  The variable index.
    int level;                  //  The lexical frame level.
    int type;                   //  The value of the type.
    const char *typestr;        //  The type name as a string.
} kl_kir_opr;

typedef struct kl_kir_inst {
    kl_kir opcode;
    const char *name;
    int labelid;
    int num;
    kl_kir_opr r1, r2, r3;
} kl_kir_inst;

typedef struct kl_kir_func {
    int sz;
    kl_kir_inst *list;
} kl_kir_func;

#endif /* KILITE_KIR_H */
