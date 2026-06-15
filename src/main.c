#include <stdio.h>
#include "../include/cpu.h"
#include "../tools/assembler.h"

int main() {
    Machine m;
    machine_init(&m);

    int instructions = assemble_file(&m, "tests/count.asm");
    if (instructions < 0) return 1;
    printf("Assembled %d instructions\n\n", instructions);

    machine_run(&m);
    machine_dump(&m);

    printf("R0 = %u  (expected 5)\n", m.cpu.registers[0]);
    printf("cycles = %lu\n", m.cpu.cycle_count);

    return 0;
}