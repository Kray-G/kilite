#ifndef KILITE_KIR_H
#define KILITE_KIR_H

typedef enum kl_kir {
    KIR_UNKNOWN = 0,
    KIR_FUNC,
    KIR_LOCAL,

    KIR_ARG,
    KIR_CALL,
    KIR_GOTO,

    KIR_ADD,
    KIR_SUB,
    KIR_MUL,
    KIR_DIV,
    KIR_MOD,
    KIR_LT,
    KIR_GT,

} kl_kir;

#endif /* KILITE_KIR_H */
