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
    KIR_ADD_II,
    KIR_ADD_VI,
    KIR_ADD_IV,

    KIR_SUB,
    KIR_SUB_II,
    KIR_SUB_VI,
    KIR_SUB_IV,

    KIR_MUL,
    KIR_MUL_II,
    KIR_MUL_VI,
    KIR_MUL_IV,

    KIR_DIV,
    KIR_DIV_II,
    KIR_DIV_VI,
    KIR_DIV_IV,

    KIR_MOD,
    KIR_MOD_II,
    KIR_MOD_VI,
    KIR_MOD_IV,

    KIR_LT,
    KIR_LT_II,
    KIR_LT_VI,
    KIR_LT_IV,

    KIR_GT,
    KIR_GT_II,
    KIR_GT_VI,
    KIR_GT_IV,

} kl_kir;

#endif /* KILITE_KIR_H */
