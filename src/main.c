#include <stdio.h>
#include <string.h>
#include "../include/cpu.h"
#include "../tools/assembler.h"
#include "../tools/debugger.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.asm> [-t]\n", argv[0]);
        fprintf(stderr, "  -t   enable execution tracer (writes trace.log)\n");
        return 1;
    }

    // check for -t flag
    int tracing = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0)
            tracing = 1;
    }

    Machine m;
    machine_init(&m);

    if (strstr(argv[1], "security") || strstr(argv[1], "encrypt")) {
        m.cpu.privilege_mode = MODE_SECURE;
        printf("Running in SECURE mode\n");
    }

    int instructions = assemble_file(&m, argv[1]);
    if (instructions < 0) return 1;
    printf("Assembled %d instructions from %s\n\n", instructions, argv[1]);

    if (tracing) {
        tracer_open();
        tracer_enable(1);
        printf("Tracing enabled — output: trace.log\n\n");
    }

    debugger_start(&m);

    if (tracing) {
        tracer_close();
        printf("Trace written to trace.log\n");
    }

    return 0;
}