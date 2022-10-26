
#include "../kir.h"
#include "parser.h"

#define IDT "    "
#define OP "%-10s"
#define REG "$%d(%d):%s"
#define R(x) i->r##x.index,i->r##x.level,(i->r##x.typestr ? i->r##x.typestr : "any")

#define disp_v(i, n) { \
    switch (i->r##n.t) { \
    case TK_VSINT: \
        printf("%" PRId64, i->r##n.i64, i->r##n.typestr); \
        break; \
    case TK_VUINT: \
        printf("%" PRIu64, i->r##n.u64, i->r##n.typestr); \
        break; \
    case TK_VAR: \
        printf(REG, R(n)); \
        break; \
    } \
} \
/**/

void disp_reg(kl_kir_inst *i, int idx)
{
    switch (idx) {
    case 1:
        if (i->r1.index < 0) {
            printf("(*r):%s", (i->r1.typestr ? i->r1.typestr : "any"));
        } else {
            printf(REG, R(1));
        }
        break;
    case 2:
        if (i->r2.index < 0) {
            printf("(*r):%s", (i->r2.typestr ? i->r2.typestr : "any"));
        } else {
            printf(REG, R(2));
        }
        break;
    case 3:
        if (i->r3.index < 0) {
            printf("(*r):%s", (i->r3.typestr ? i->r3.typestr : "any"));
        } else {
            printf(REG, R(3));
        }
        break;
    }
}

void disp_3op(const char *op, kl_kir_inst *i)
{
    printf(IDT OP, op);

    disp_reg(i, 1);
    printf(", ");

    disp_v(i, 2);
    printf(", ");
    disp_v(i, 3);

    printf("\n");
}

void disp_inst(kl_context *ctx, kl_kir_inst *i)
{
    switch (i->opcode) {
    case KIR_LOCAL:
        printf(IDT OP, "local");
        disp_v(i, 1);
        printf("\n");
        break;
    case KIR_MKFRM:
        printf(IDT OP, "mkfrm");
        disp_v(i, 1);
        printf("\n");
        break;
    case KIR_POPFRM:
        printf(IDT OP "\n", "popfrm");
        break;

    case KIR_SVSTKP:
        printf(IDT OP "\n", "savestkp");
        break;
    case KIR_PUSHARG:
        printf(IDT OP, "pusharg");
        disp_reg(i, 1);
        printf("\n");
        break;
    case KIR_CALL:
        printf(IDT OP, "call");
        disp_reg(i, 1);
        printf(", ");
        disp_reg(i, 2);
        printf("\n");
        break;
    case KIR_RSSTKP:
        printf(IDT OP "\n", "restorestkp");
        break;

    case KIR_RET:
        printf(IDT OP, "ret");
        if (i->r1.index >= 0) {
            disp_reg(i, 1);
        }
        printf("\n");
        break;

    case KIR_JMPIFF:
        printf(IDT OP, "jmpiff");
        disp_reg(i, 1);
        printf(", L%d\n", i->labelid);
        break;
    case KIR_JMP:
        printf(IDT OP, "jmp");
        printf("L%d\n", i->labelid);
        break;
    case KIR_LABEL:
        printf("  L%d:\n", i->labelid);
        break;

    case KIR_ADD:
        disp_3op("add", i);
        break;
    case KIR_SUB:
        disp_3op("sub", i);
        break;

    case KIR_LT:
        disp_3op("lt", i);
        break;

    }
}

void disp_func(kl_context *ctx, kl_kir_func *f)
{
    printf("func: %s\n", f->name);

    kl_kir_inst *i = f->head;
    while (i) {
        disp_inst(ctx, i);
        i = i->next;
    }

    printf("\n");
}

void disp_program(kl_context *ctx)
{
    if (!ctx->program) {
        return;
    }

    kl_kir_func *f = ctx->program->head;
    while (f) {
        disp_func(ctx, f);
        f = f->next;
    }
}
