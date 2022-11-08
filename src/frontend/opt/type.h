#ifndef KILITE_OPT_TYPE_H
#define KILITE_OPT_TYPE_H

#include "../error.h"
#include "../parser.h"

extern void update_ast_type(kl_context *ctx);
extern int check_pure_function(kl_context *ctx, kl_stmt *stmt);

#endif /* KILITE_OPT_TYPE_H */
