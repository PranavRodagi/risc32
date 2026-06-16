#include <stdio.h>
#include <string.h>
#include "../include/cpu.h"
#include "../tools/assembler.h"
#include "../tools/debugger.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.asm> [-t]\n", argv[0]);
        return 1;
    }

    int tracing = 0;
    for (int i = 2; i < argc; i++)
        if (strcmp(argv[i], "-t") == 0) tracing = 1;

    Machine m;
    machine_init(&m);

    // initialize MPU regions for all programs
    // region 0: code — execute only, user+secure
    mpu_add_region(&m, 0, 0x0000, 0x1FFF,
                   MPU_EXECUTE,
                   MPU_USER | MPU_SECURE);

    // region 1: data — read+write, user+secure
    mpu_add_region(&m, 1, 0x2000, 0xDFFF,
                   MPU_READ | MPU_WRITE,
                   MPU_USER | MPU_SECURE);

    // region 2: secure memory — read+write, secure only
    mpu_add_region(&m, 2, 0xE000, 0xFEFF,
                   MPU_READ | MPU_WRITE,
                   MPU_SECURE);

    // region 3: stack — read+write, user+secure
    mpu_add_region(&m, 3, 0xFF00, 0xFFFF,
                   MPU_READ | MPU_WRITE,
                   MPU_USER | MPU_SECURE);

    // security programs get secure mode + MPU enabled
    if (strstr(argv[1], "security") || strstr(argv[1], "encrypt")) {
        m.cpu.privilege_mode = MODE_SECURE;
        mpu_enable(&m, 1);
        printf("Running in SECURE mode with MPU enabled\n");
    }

    // mpu_test runs in user mode with MPU enabled
    if (strstr(argv[1], "mpu")) {
        mpu_enable(&m, 1);
        printf("MPU enabled — running in USER mode\n");
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