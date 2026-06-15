#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/cpu.h"
#include "debugger.h"

// ─── breakpoints ──────────────────────────────────────────────────
#define MAX_BREAKPOINTS 16

static uint32_t breakpoints[MAX_BREAKPOINTS];
static int      breakpoint_count = 0;

static void breakpoint_add(uint32_t addr) {
    if (breakpoint_count >= MAX_BREAKPOINTS) {
        printf("Breakpoint list full\n");
        return;
    }
    breakpoints[breakpoint_count++] = addr;
    printf("Breakpoint set at 0x%04X\n", addr);
}

static int breakpoint_hit(uint32_t addr) {
    for (int i = 0; i < breakpoint_count; i++)
        if (breakpoints[i] == addr) return 1;
    return 0;
}

static void breakpoint_list(void) {
    if (breakpoint_count == 0) {
        printf("No breakpoints set\n");
        return;
    }
    for (int i = 0; i < breakpoint_count; i++)
        printf("  [%d] 0x%04X\n", i, breakpoints[i]);
}

static void breakpoint_clear(int index) {
    if (index < 0 || index >= breakpoint_count) {
        printf("Invalid breakpoint index\n");
        return;
    }
    // shift remaining breakpoints down
    for (int i = index; i < breakpoint_count - 1; i++)
        breakpoints[i] = breakpoints[i + 1];
    breakpoint_count--;
    printf("Breakpoint %d removed\n", index);
}

// ─── disassembler ─────────────────────────────────────────────────
// reads 4 bytes from memory at addr and prints human-readable form
void disassemble(Machine *m, uint32_t addr) {
    // read instruction little-endian
    uint32_t instr =  (uint32_t)m->memory[addr + 0]
                   | ((uint32_t)m->memory[addr + 1] << 8)
                   | ((uint32_t)m->memory[addr + 2] << 16)
                   | ((uint32_t)m->memory[addr + 3] << 24);

    uint8_t  opcode = (instr >> 24) & 0xFF;
    uint8_t  rd     = (instr >> 20) & 0xF;
    uint8_t  ra     = (instr >> 16) & 0xF;
    uint8_t  rb     = (instr >> 12) & 0xF;
    uint16_t imm    =  instr        & 0xFFFF;
    uint32_t jaddr  =  instr        & 0xFFFFFF;

    printf("0x%04X: %08X  ", addr, instr);

    switch (opcode) {
        case OP_ADD:  printf("ADD  R%d, R%d, R%d",  rd, ra, rb);  break;
        case OP_SUB:  printf("SUB  R%d, R%d, R%d",  rd, ra, rb);  break;
        case OP_MUL:  printf("MUL  R%d, R%d, R%d",  rd, ra, rb);  break;
        case OP_DIV:  printf("DIV  R%d, R%d, R%d",  rd, ra, rb);  break;
        case OP_AND:  printf("AND  R%d, R%d, R%d",  rd, ra, rb);  break;
        case OP_OR:   printf("OR   R%d, R%d, R%d",  rd, ra, rb);  break;
        case OP_XOR:  printf("XOR  R%d, R%d, R%d",  rd, ra, rb);  break;
        case OP_NOT:  printf("NOT  R%d, R%d",        rd, ra);      break;
        case OP_ADDI: printf("ADDI R%d, R%d, %d",   rd, ra, imm); break;
        case OP_LOAD: printf("LOAD R%d, R%d, %d",   rd, ra, imm); break;
        case OP_STORE:printf("STORE R%d, R%d, %d",  rd, ra, imm); break;
        case OP_JMP:  printf("JMP  0x%04X",          jaddr);       break;
        case OP_CALL: printf("CALL 0x%04X",          jaddr);       break;
        case OP_RET:  printf("RET");                               break;
        case OP_HALT: printf("HALT");                              break;
        case OP_BEQ:  printf("BEQ  R%d, 0x%04X",    ra, imm);     break;
        case OP_BNE:  printf("BNE  R%d, 0x%04X",    ra, imm);     break;
        case OP_BLT:  printf("BLT  R%d, 0x%04X",    ra, imm);     break;
        case OP_BGT:  printf("BGT  R%d, 0x%04X",    ra, imm);     break;
        case OP_PUSH: printf("PUSH R%d",             ra);          break;
        case OP_POP:  printf("POP  R%d",             rd);          break;
        default:      printf("??? (unknown opcode 0x%02X)", opcode); break;
    }
    printf("\n");
}

// ─── print a window of instructions around the current PC ─────────
static void disassemble_window(Machine *m, int count) {
    uint32_t pc = m->cpu.registers[REG_PC];
    for (int i = 0; i < count; i++) {
        uint32_t addr = pc + (i * 4);
        if (addr >= MEMORY_SIZE - 4) break;
        // mark current instruction with an arrow
        if (addr == pc) printf("→ ");
        else            printf("  ");
        disassemble(m, addr);
    }
}

// ─── memory dump ──────────────────────────────────────────────────
static void dump_memory(Machine *m, uint32_t addr, int count) {
    printf("\nMemory at 0x%04X:\n", addr);
    for (int i = 0; i < count; i += 16) {
        printf("  0x%04X: ", addr + i);
        // hex bytes
        for (int j = 0; j < 16 && (i + j) < count; j++)
            printf("%02X ", m->memory[addr + i + j]);
        // ASCII representation
        printf(" | ");
        for (int j = 0; j < 16 && (i + j) < count; j++) {
            uint8_t b = m->memory[addr + i + j];
            printf("%c", (b >= 32 && b < 127) ? b : '.');
        }
        printf("\n");
    }
}

// ─── run until breakpoint or halt ─────────────────────────────────
static void run_until_break(Machine *m) {
    while (!m->cpu.halted) {
        uint32_t pc = m->cpu.registers[REG_PC];
        if (breakpoint_hit(pc)) {
            printf("Breakpoint hit at 0x%04X\n", pc);
            return;
        }
        machine_step(m);
    }
    if (m->cpu.halted)
        printf("CPU halted after %lu cycles\n", m->cpu.cycle_count);
}

// ─── print help ───────────────────────────────────────────────────
static void print_help(void) {
    printf("\nRISC-32 Debugger commands:\n");
    printf("  s              step one instruction\n");
    printf("  r              run until halt or breakpoint\n");
    printf("  d              dump all registers\n");
    printf("  p              show next 5 instructions (disassemble)\n");
    printf("  m <addr>       show 64 bytes of memory at hex address\n");
    printf("  b <addr>       set breakpoint at hex address\n");
    printf("  bl             list all breakpoints\n");
    printf("  bc <index>     clear breakpoint by index\n");
    printf("  reset          reset the CPU (memory preserved)\n");
    printf("  q              quit\n\n");
}

// ─── the main debugger loop ───────────────────────────────────────
void debugger_start(Machine *m) {
    printf("\n");
    printf("╔══════════════════════════════╗\n");
    printf("║     RISC-32 Debugger v1.0    ║\n");
    printf("╚══════════════════════════════╝\n");
    printf("Type 'h' for help\n\n");

    // show starting state
    disassemble_window(m, 5);

    char input[64];

    while (1) {
        printf("\ndbg [0x%04X]> ", m->cpu.registers[REG_PC]);
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

        // strip newline
        input[strcspn(input, "\n")] = '\0';

        // parse command
        if (strcmp(input, "s") == 0) {
            // ── step ──────────────────────────────────────────────
            if (m->cpu.halted) {
                printf("CPU is halted. Reset to continue.\n");
                continue;
            }
            uint32_t pc_before = m->cpu.registers[REG_PC];
            printf("Executing: ");
            disassemble(m, pc_before);
            machine_step(m);
            printf("cycles: %lu  flags: %s%s%s%s\n",
                m->cpu.cycle_count,
                (m->cpu.flags & FLAG_ZERO)     ? "Z" : "-",
                (m->cpu.flags & FLAG_NEGATIVE) ? "N" : "-",
                (m->cpu.flags & FLAG_CARRY)    ? "C" : "-",
                (m->cpu.flags & FLAG_OVERFLOW) ? "V" : "-");
            if (!m->cpu.halted)
                disassemble_window(m, 3);

        } else if (strcmp(input, "r") == 0) {
            // ── run ───────────────────────────────────────────────
            if (m->cpu.halted) {
                printf("CPU is halted. Reset to continue.\n");
                continue;
            }
            run_until_break(m);
            disassemble_window(m, 3);

        } else if (strcmp(input, "d") == 0) {
            // ── dump ──────────────────────────────────────────────
            machine_dump(m);

        } else if (strcmp(input, "p") == 0) {
            // ── disassemble window ────────────────────────────────
            disassemble_window(m, 8);

        } else if (strncmp(input, "m ", 2) == 0) {
            // ── memory dump ───────────────────────────────────────
            uint32_t addr = (uint32_t)strtol(input + 2, NULL, 16);
            dump_memory(m, addr, 64);

        } else if (strncmp(input, "b ", 2) == 0) {
            // ── set breakpoint ────────────────────────────────────
            uint32_t addr = (uint32_t)strtol(input + 2, NULL, 16);
            breakpoint_add(addr);

        } else if (strcmp(input, "bl") == 0) {
            // ── list breakpoints ──────────────────────────────────
            breakpoint_list();

        } else if (strncmp(input, "bc ", 3) == 0) {
            // ── clear breakpoint ──────────────────────────────────
            int index = atoi(input + 3);
            breakpoint_clear(index);

        } else if (strcmp(input, "reset") == 0) {
            // ── reset CPU ─────────────────────────────────────────
            // reset registers and flags but keep memory
            for (int i = 0; i < NUM_REGISTERS; i++)
                m->cpu.registers[i] = 0;
            m->cpu.registers[REG_SP] = MEMORY_SIZE - 4;
            m->cpu.flags          = 0;
            m->cpu.halted         = 0;
            m->cpu.cycle_count    = 0;
            printf("CPU reset. Memory preserved.\n");
            disassemble_window(m, 5);

        } else if (strcmp(input, "h") == 0) {
            print_help();

        } else if (strcmp(input, "q") == 0) {
            printf("Goodbye.\n");
            break;

        } else if (strlen(input) > 0) {
            printf("Unknown command '%s'. Type 'h' for help.\n", input);
        }
    }
}