
#include "../kir.h"
#include "dispkir.h"

extern void escape_print(const char *);

#define IDT "    "
#define IDT2 "  "
#define OP "%-16s"
#define REG "$%d(%d):%s"
#define TYPS(x) (i->r##x.typestr ? i->r##x.typestr : typeidname(i->r##x.typeid))
#define R(x) i->r##x.index,i->r##x.level,TYPS(x)

#define disp_v(i, n) { \
    switch (i->r##n.t) { \
    case TK_VSINT: \
        printf("%" PRId64, i->r##n.i64); \
        break; \
    case TK_VBIGINT: \
        printf("%s", i->r##n.str); \
        break; \
    case TK_VDBL: \
        printf("%s", i->r##n.dbl); \
        break; \
    case TK_VSTR: \
        printf("\""); \
        escape_print(i->r##n.str); \
        printf("\""); \
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

void disp_mov(const char *op, kl_kir_inst *i)
{
    printf(IDT OP, op);
    disp_v(i, 1);
    printf(", ");
    if (i->r1.index < 0 && i->r2.index < 0) {
        printf("undefined\n");
    } else {
        disp_v(i, 2);
        printf("\n");
    }
}

void disp_pushsys(kl_kir_inst *i)
{
    printf(IDT OP, "pushsys");
    printf("*%d, ", i->r1.callcnt);
    disp_v(i, 1);
    printf("\n");
}

void disp_pusharg(kl_kir_inst *i)
{
    printf(IDT OP, i->r1.has_dot3 ? "pusharga" : "pusharg");
    printf("*%d, ", i->r1.callcnt);
    disp_v(i, 1);
    printf("\n");
}

void disp_call(const char *op, kl_kir_inst *i)
{
    printf(IDT OP, op);
    printf("*%d, ", i->r2.callcnt);
    disp_v(i, 1);
    printf(", ");
    disp_v(i, 2);
    if (i->r2.funcid > 0) {
        printf("(%s)", i->r2.name);
    }
    printf("\n");
}

void disp_inst(kl_kir_program *p, kl_kir_inst *i)
{
    if (i->disabled) {
        return;
    }

    switch (i->opcode) {
    case KIR_NOP:
        disp_0op("nop");
        break;

    case KIR_EXTERN:
        disp_2op("extern", i);
        break;

    case KIR_ALOCAL:
        disp_3op("local", i);
        break;
    case KIR_RLOCAL:
        disp_0op("reduce");
        break;
    case KIR_MKFRM:
        disp_3op("mkfrm", i);
        break;
    case KIR_POPFRM:
        disp_0op("popfrm");
        break;
    case KIR_SETARG:
        disp_2op("setarg", i);
        break;
    case KIR_SETARGL:
        disp_2op("setargl", i);
        break;
    case KIR_PURE:
        printf(IDT OP "\n", "purehook");
        break;

    case KIR_SVSTKP:
        printf(IDT OP, "savestkp");
        printf("*%" PRId64 "\n", i->r1.i64);
        break;
    case KIR_PUSHSYS:
        disp_pushsys(i);
        break;
    case KIR_PUSHARG:
        disp_pusharg(i);
        break;
    case KIR_CALL:
        disp_call("call", i);
        break;
    case KIR_RSSTKP:
        printf(IDT OP, "restorestkp");
        printf("*%" PRId64 "\n", i->r1.i64);
        break;
    case KIR_CHKEXCEPT:
        printf(IDT OP "L%d\n", "chkexcept", i->labelid);
        break;
    case KIR_YIELDC:
        printf(IDT OP "Y%d\n", "yieldcheck", i->labelid);
        break;

    case KIR_RET:
        printf(IDT OP "\n", "ret");
        break;
    case KIR_THROWE:
        printf(IDT OP, "throwe");
        disp_v(i, 1);
        printf(", L%d\n", i->labelid);
        break;
    case KIR_THROW:
        printf(IDT OP "L%d\n", "throw", i->labelid);
        break;
    case KIR_THROWX:
        printf(IDT OP "%s, L%d\n", "throwx", i->r1.str, i->labelid);
        break;
    case KIR_CATCH:
        disp_1op("catch", i);
        break;
    case KIR_RESUME:
        disp_1op("resume", i);
        break;
    case KIR_YIELD:
        printf(IDT OP "Y%d\n", "yield", i->labelid);
        break;

    case KIR_JMPIFT:
        printf(IDT OP, "jmpift");
        disp_v(i, 1);
        printf(", L%d\n", i->labelid);
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

    case KIR_NOT:
        disp_2op("not", i);
        break;
    case KIR_MOV:
        disp_mov("mov", i);
        break;
    case KIR_MOVA:
        disp_2op("mova", i);
        break;

    case KIR_ADD:
        disp_3op("add", i);
        break;
    case KIR_SUB:
        disp_3op("sub", i);
        break;
    case KIR_MUL:
        disp_3op("mul", i);
        break;
    case KIR_DIV:
        disp_3op("div", i);
        break;
    case KIR_MOD:
        disp_3op("mod", i);
        break;
    case KIR_POW:
        disp_3op("pow", i);
        break;
    case KIR_BNOT:
        disp_3op("bitnot", i);
        break;
    case KIR_BSHL:
        disp_3op("bitshift-l", i);
        break;
    case KIR_BSHR:
        disp_3op("bitshift-r", i);
        break;
    case KIR_BAND:
        disp_3op("bitand", i);
        break;
    case KIR_BOR:
        disp_3op("bitor", i);
        break;
    case KIR_BXOR:
        disp_3op("bitxor", i);
        break;

    case KIR_EQEQ:
        disp_3op("eqeq", i);
        break;
    case KIR_NEQ:
        disp_3op("neq", i);
        break;
    case KIR_LT:
        disp_3op("lt", i);
        break;
    case KIR_LE:
        disp_3op("le", i);
        break;
    case KIR_GT:
        disp_3op("gt", i);
        break;
    case KIR_GE:
        disp_3op("ge", i);
        break;
    case KIR_LGE:
        disp_3op("lge", i);
        break;
    case KIR_REGEQ:
        disp_3op("regeq", i);
        break;
    case KIR_REGNE:
        disp_3op("regne", i);
        break;

    case KIR_INC:
        disp_2op("inc", i);
        break;
    case KIR_INCP:
        disp_2op("incp", i);
        break;
    case KIR_DEC:
        disp_2op("dec", i);
        break;
    case KIR_DECP:
        disp_2op("decp", i);
        break;
    case KIR_MINUS:
        disp_2op("uminus", i);
        break;
    case KIR_CONV:
        disp_2op("conv", i);
        break;

    case KIR_NEWBIN:
        disp_1op("newbin", i);
        break;
    case KIR_NEWOBJ:
        disp_1op("newobj", i);
        break;
    case KIR_NEWREGEX:
        disp_3op("newregex", i);
        break;
    case KIR_SETBIN:
        disp_3op("setbin", i);
        break;
    case KIR_OBJCPY:
        disp_2op("objcopy", i);
        break;
    case KIR_MKSUPER:
        disp_2op("makesuper", i);
        break;
    case KIR_IDXFRM:
        disp_3op("idxfrm", i);
        break;
    case KIR_IDX:
        disp_3op("idx", i);
        break;
    case KIR_IDXL:
        disp_3op("idxl", i);
        break;
    case KIR_APLY:
        disp_3op("apply", i);
        break;
    case KIR_APLYL:
        disp_3op("applyl", i);
        break;
    case KIR_REMOVE:
        disp_2op("remove", i);
        break;
    case KIR_CHKMATCH:
        disp_2op("chkmatch", i);
        break;
    case KIR_CHKRANGE:
        disp_2op("chkrange", i);
        break;
    case KIR_CHKMATCHX:
        printf(IDT OP, "chkmatchx");
        disp_v(i, 1);
        printf(", ");
        disp_v(i, 2);
        printf(", L%d\n", i->labelid);
        break;
    case KIR_CHKRANGEX:
        printf(IDT OP, "chkrangex");
        disp_v(i, 1);
        printf(", ");
        disp_v(i, 2);
        printf(", L%d\n", i->labelid);
        break;

    case KIR_TYPE:
        disp_3op("type", i);
        break;
    case KIR_GETITER:
        disp_1op("getiter", i);
        break;
    case KIR_JMPIFNE:
        printf(IDT OP, "jmpifne");
        disp_v(i, 1);
        printf(", L%d\n", i->labelid);
        break;

    case KIR_SWITCHS:
        disp_2op("switch", i);
        break;
    case KIR_SWITCHE:
        disp_0op("switche");
        break;
    case KIR_CASEI:
        printf(IDT2 OP IDT2, "casei:");
        disp_v(i, 1);
        printf("\n");
        break;
    case KIR_CASEV:
        printf(IDT2 OP IDT2, "casev:");
        disp_v(i, 1);
        printf(", ");
        disp_v(i, 2);
        printf(", L%d\n", i->labelid);
        break;
    case KIR_DEFAULT:
        printf(IDT2 OP IDT2, "default:");
        printf("\n");
        break;

    case KIR_SWAP:
        disp_2op("swap", i);
        break;
    case KIR_SWAPA:
        disp_2op("swapa", i);
        break;
    case KIR_PUSH:
        disp_2op("push", i);
        break;
    case KIR_PUSHX:
        disp_2op("pushx", i);
        break;
    case KIR_PUSHN:
        disp_1op("pushn", i);
        break;
    case KIR_EXPAND:
        disp_2op("expand", i);
        break;

    case KIR_RANGEF:
        disp_3op("rangef", i);
        break;
    case KIR_RANGET:
        disp_3op("ranget", i);
        break;

    case KIR_ARYSIZE:
        disp_2op("arysize", i);
        break;
    }
}

void disp_func(kl_kir_program *p, kl_kir_func *f)
{
    printf("func%s: %s\n", f->is_pure ? "[*]" : "", f->funcname);

    kl_kir_inst *i = f->head;
    while (i) {
        disp_inst(p, i);
        i = i->next;
    }

    printf("\n");
}

void disp_program(kl_kir_program *p)
{
    if (!p) {
        return;
    }

    kl_kir_func *f = p->head;
    while (f) {
        disp_func(p, f);
        f = f->next;
    }
}
