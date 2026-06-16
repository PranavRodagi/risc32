#include <stdio.h>
#include "../include/cpu.h"
#include "../tools/assembler.h"
#include "../tools/debugger.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.asm>\n", argv[0]);
        fprintf(stderr, "Example: %s tests/count.asm\n", argv[0]);
        return 1;
    }

    Machine m;
    machine_init(&m);

    // security.asm needs secure mode
    if (argc >= 2) {
        const char *filename = argv[1];
        // check if filename contains "security"
        if (strstr(filename, "security")) {
            m.cpu.privilege_mode = MODE_SECURE;
            printf("Running in SECURE mode\n");
        }
    }

    int instructions = assemble_file(&m, argv[1]);
    if (instructions < 0) return 1;
    printf("Assembled %d instructions from %s\n\n", instructions, argv[1]);

    debugger_start(&m);
    return 0;
}