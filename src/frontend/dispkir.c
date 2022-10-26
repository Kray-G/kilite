
#include "../kir.h"
#include "parser.h"

#define IDT "    "
#define OP "%-10s"
#define REG "$%d(%d):%s"
#define TYPS(x) (i->r##x.typestr ? i->r##x.typestr : typeidname(i->r##x.typeid))
#define R(x) i->r##x.index,i->r##x.level,TYPS(x)

#define disp_v(i, n) { \
    switch (i->r##n.t) { \
    case TK_VSINT: \
        printf("%" PRId64 ":%s", i->r##n.i64, TYPS(n)); \
        break; \
    case TK_VUINT: \
        printf("%" PRIu64 ":%s", i->r##n.u64, TYPS(n)); \
        break; \
    case TK_VAR: \
        if (i->r##n.index < 0) { \
            printf("(*r):%s", TYPS(n)); \
        } else { \
            printf(REG, R(n)); \
        } \
        break; \
    case TK_FUNC: \
        printf("%s:%s", i->r##n.name, TYPS(n)); \
        break; \
    } \
} \
/**/

void disp_0op(const char *op)
{
    printf(IDT OP "\n", op);
}

void disp_1op(const char *op, kl_kir_inst *i)
{
    printf(IDT OP, op);
    disp_v(i, 1);
    printf("\n");
}

void disp_2op(const char *op, kl_kir_inst *i)
{
    printf(IDT OP, op);
    disp_v(i, 1);
    printf(", ");
    disp_v(i, 2);
    printf("\n");
}

void disp_3op(const char *op, kl_kir_inst *i)
{
    printf(IDT OP, op);
    disp_v(i, 1);
    printf(", ");
    disp_v(i, 2);
    printf(", ");
    disp_v(i, 3);
    printf("\n");
}

void disp_inst(kl_context *ctx, kl_kir_inst *i)
{
    switch (i->opcode) {
    case KIR_ALOCAL:
        disp_1op("local", i);
        break;
    case KIR_RLOCAL:
        disp_0op("reduce");
        break;
    case KIR_MKFRM:
        disp_1op("mkfrm", i);
        break;
    case KIR_POPFRM:
        disp_0op("popfrm");
        break;

    case KIR_SVSTKP:
        printf(IDT OP "\n", "savestkp");
        break;
    case KIR_PUSHARG:
        disp_1op("pusharg", i);
        break;
    case KIR_CALL:
        disp_2op("call", i);
        break;
    case KIR_RSSTKP:
        printf(IDT OP "\n", "restorestkp");
        break;

    case KIR_RET:
        printf(IDT OP, "ret");
        if (i->r1.index >= 0) {
            disp_v(i, 1);
        }
        printf("\n");
        break;

    case KIR_JMPIFF:
        printf(IDT OP, "jmpiff");
        disp_v(i, 1);
        printf(", L%d\n", i->labelid);
        break;
    case KIR_JMP:
        printf(IDT OP "L%d\n", "jmp", i->labelid);
        break;
    case KIR_LABEL:
        printf("  L%d:\n", i->labelid);
        break;

    case KIR_MOV:
        disp_2op("mov", i);
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
