#ifndef KILITE_TRANSLATE_H
#define KILITE_TRANSLATE_H

#include "../kir.h"

#define TRANS_SRC (0)
#define TRANS_LIB (1)
#define TRANS_DEBUG (2)
extern char *translate(kl_kir_program *p, int mode);

#endif /* KILITE_TRANSLATE_H */
