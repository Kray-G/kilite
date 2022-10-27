#include <stdio.h>

int main(int ac, char **av)
{
    if (ac < 2) {
        printf("Usage: makecstr [file]\n");
        return 1;
    }
    const char *file = av[1];
    FILE *f = fopen(file, "r");
    if (!f) {
        printf("File open failed.\n");
        return 1;
    }

    int ch;
    printf("static const char *header = \"");
    while ((ch = fgetc(f)) != EOF) {
        if (ch == '"') {
            printf("\\\"");
        } else if (ch == '\\') {
            printf("\\\\");
        } else if (ch == '\t') {
            printf("\\t");
        } else if (ch == '\r') {
            /* skip */
        } else if (ch == '\n') {
            printf("\\n\"\n\"");
        } else {
            printf("%c", ch);
        }
    }
    printf("\\n\";\n\n");
    printf("const char *vmheader(void)\n");
    printf("{\n");
    printf("    return header;\n");
    printf("}\n\n");
    fclose(f);

    return 0;
}