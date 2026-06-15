#include <stdio.h>
#include "../include/cpu.h"
#include "../tools/assembler.h"
#include "../tools/debugger.h"

int main() {
    Machine m;
    machine_init(&m);

    int instructions = assemble_file(&m, "tests/count.asm");
    if (instructions < 0) return 1;
    printf("Assembled %d instructions\n", instructions);

    debugger_start(&m);
    return 0;
}