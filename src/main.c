#include <stdio.h>
#include "../include/cpu.h"

// helper: write a 32-bit instruction into memory at a given address
static void write_instruction(Machine *m, uint32_t addr, uint32_t instr) {
    m->memory[addr + 0] = (instr)       & 0xFF;
    m->memory[addr + 1] = (instr >> 8)  & 0xFF;
    m->memory[addr + 2] = (instr >> 16) & 0xFF;
    m->memory[addr + 3] = (instr >> 24) & 0xFF;
}

int main() {
    Machine m;
    machine_init(&m);

    // ── write a small program into memory ─────────────────────────
    // This program:
    //   1. loads two constants into R0 and R1
    //   2. adds them and puts result in R2
    //   3. multiplies R2 by R1 and puts result in R3
    //   4. halts
    //
    // Expected: R0=10, R1=20, R2=30, R3=600, cycles=6

    uint32_t pc = 0x0000;

    // ADDI R0, R0, 10    → R0 = 0 + 10 = 10
    write_instruction(&m, pc, ENCODE_I(OP_ADDI, 0, 0, 10));
    pc += 4;

    // ADDI R1, R1, 20    → R1 = 0 + 20 = 20
    write_instruction(&m, pc, ENCODE_I(OP_ADDI, 1, 1, 20));
    pc += 4;

    // ADD R2, R0, R1     → R2 = 10 + 20 = 30
    write_instruction(&m, pc, ENCODE_R(OP_ADD, 2, 0, 1));
    pc += 4;

    // MUL R3, R2, R1     → R3 = 30 * 20 = 600
    write_instruction(&m, pc, ENCODE_R(OP_MUL, 3, 2, 1));
    pc += 4;

    // HALT
    write_instruction(&m, pc, ENCODE_R(OP_HALT, 0, 0, 0));

    // ── run until halted ──────────────────────────────────────────
    machine_run(&m);

    // ── show results ──────────────────────────────────────────────
    machine_dump(&m);

    printf("R0  = %u  (expected 10)\n",  m.cpu.registers[0]);
    printf("R1  = %u  (expected 20)\n",  m.cpu.registers[1]);
    printf("R2  = %u  (expected 30)\n",  m.cpu.registers[2]);
    printf("R3  = %u  (expected 600)\n", m.cpu.registers[3]);
    printf("cycles = %lu  (expected 6)\n", m.cpu.cycle_count);

    return 0;
}