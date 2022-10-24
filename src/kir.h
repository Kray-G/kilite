#ifndef KILITE_KIR_H
#define KILITE_KIR_H

typedef enum kl_kir {
    KIR_UNKNOWN = 0,
    KIR_FUNC,
    KIR_LOCAL,

    /*
     * Function call, which should work like this.
     *  -> save vstackp
     *  -> push arguments
     *  -> call function
     *  -> restore vstackp
     */
    KIR_SVSTKP,
    KIR_ARG,
    KIR_CALL,
    KIR_RSSTKP,

    /*
     * Return from a function.
     */
    KIR_RET,

    /*
     * Conditional and unconditional Jump.
     */
    KIR_TEST,
    KIR_JMPIFF,
    KIR_JMP,

    /*
     * Operators.
     */
    KIR_ADD,
    KIR_SUB,
    KIR_MUL,
    KIR_DIV,
    KIR_MOD,
    KIR_LT,
    KIR_GT,

} kl_kir;

#endif /* KILITE_KIR_H */
