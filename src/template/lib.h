#ifndef KILITE_TEMPLATE_LIB_H
#define KILITE_TEMPLATE_LIB_H

/* check argument's type if it's specified */
#define DEF_ARG(a0, n, type) \
    if (ac < (n+1)) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TOO_FEW_ARGUMENTS, NULL); \
    } \
    vmvar *a0 = local_var(ctx, n); \
    if (a0->t != type) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL); \
    } \
/**/
#define DEF_ARG2(a0, n, type1, type2) \
    if (ac < (n+1)) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TOO_FEW_ARGUMENTS, NULL); \
    } \
    vmvar *a0 = local_var(ctx, n); \
    if (a0->t != type1 && a0->t != type2) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL); \
    } \
/**/
#define DEF_ARG_ANY(a0, n) \
    vmvar *a0 = (ac > n) ? local_var(ctx, n) : alcvar_initial(ctx); \
/**/
#define DEF_ARG_INT(a0, n) \
    if (ac < (n+1)) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TOO_FEW_ARGUMENTS, NULL); \
    } \
    vmvar *a0 = local_var(ctx, n); \
    if (a0->t != VAR_INT64 && a0->t != VAR_BIG) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL); \
    } \
/**/
#define DEF_ARG_OR_UNDEF(a0, n, type) \
    vmvar *a0 = (ac > n) ? local_var(ctx, n) : alcvar_initial(ctx); \
    if (a0->t != type && a0->t != VAR_UNDEF) { \
        return throw_system_exception(__LINE__, ctx, EXCEPT_TYPE_MISMATCH, NULL); \
    } \
/**/

#endif /* KILITE_TEMPLATE_LIB_H */
