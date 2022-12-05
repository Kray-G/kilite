#include <stdio.h>
#include <time.h>

int main(int ac, char **av)
{
    printf("#ifdef __MIRC__\n");
    printf("#ifndef CLOCKS_PER_SEC\n");
    printf("#define CLOCKS_PER_SEC ((clock_t)%d)\n", CLOCKS_PER_SEC);
    printf("#endif\n");
    printf("#endif\n");
    return 0;
}