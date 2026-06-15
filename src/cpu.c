#include <stdio.h>
#include "../include/cpu.h"

static void update_flags(CPU *cpu, uint32_t result) {
    cpu->flags = 0;
    if (result == 0)
        cpu->flags |= FLAG_ZERO;
    if (result & 0x80000000)
        cpu->flags |= FLAG_NEGATIVE;
}

static void write_word(uint8_t *memory, uint32_t addr, uint32_t value) {
    memory[addr + 0] = (value)       & 0xFF;
    memory[addr + 1] = (value >> 8)  & 0xFF;
    memory[addr + 2] = (value >> 16) & 0xFF;
    memory[addr + 3] = (value >> 24) & 0xFF;
}

static uint32_t read_word(uint8_t *memory, uint32_t addr) {
    return  (uint32_t)memory[addr + 0]
         | ((uint32_t)memory[addr + 1] << 8)
         | ((uint32_t)memory[addr + 2] << 16)
         | ((uint32_t)memory[addr + 3] << 24);
}

void machine_init(Machine *m) {
    for (int i = 0; i < NUM_REGISTERS; i++)
        m->cpu.registers[i] = 0;
    m->cpu.flags          = 0;
    m->cpu.privilege_mode = MODE_USER;
    m->cpu.halted         = 0;
    m->cpu.cycle_count    = 0;
    for (int i = 0; i < MEMORY_SIZE; i++)
        m->memory[i] = 0;
    m->cpu.registers[REG_SP] = MEMORY_SIZE - 4;
}

void machine_dump(Machine *m) {
    printf("\n=== CPU STATE ===\n");
    printf("cycles: %lu\n", m->cpu.cycle_count);
    printf("flags:  %s%s%s%s\n",
        (m->cpu.flags & FLAG_ZERO)     ? "Z" : "-",
        (m->cpu.flags & FLAG_NEGATIVE) ? "N" : "-",
        (m->cpu.flags & FLAG_CARRY)    ? "C" : "-",
        (m->cpu.flags & FLAG_OVERFLOW) ? "V" : "-");
    printf("mode:   %s\n",
        m->cpu.privilege_mode == MODE_SECURE ? "SECURE" : "USER");
    printf("halted: %s\n", m->cpu.halted ? "YES" : "NO");
    printf("\n");
    for (int i = 0; i < NUM_REGISTERS; i++) {
        if      (i == REG_KEY) printf("R%02d [KEY]: 0x%08X\n", i, m->cpu.registers[i]);
        else if (i == REG_SP)  printf("R%02d [SP]:  0x%08X\n", i, m->cpu.registers[i]);
        else if (i == REG_LR)  printf("R%02d [LR]:  0x%08X\n", i, m->cpu.registers[i]);
        else if (i == REG_PC)  printf("R%02d [PC]:  0x%08X\n", i, m->cpu.registers[i]);
        else                   printf("R%02d:       0x%08X\n", i, m->cpu.registers[i]);
    }
    printf("=================\n\n");
}

void machine_step(Machine *m) {
    if (m->cpu.halted) return;

    // FETCH
    uint32_t pc          = m->cpu.registers[REG_PC];
    uint32_t instruction = read_word(m->memory, pc);
    m->cpu.registers[REG_PC] += 4;

    // DECODE
    uint8_t  opcode = (instruction >> 24) & 0xFF;
    uint8_t  rd     = (instruction >> 20) & 0xF;
    uint8_t  ra     = (instruction >> 16) & 0xF;
    uint8_t  rb     = (instruction >> 12) & 0xF;
    uint16_t imm    = (instruction)       & 0xFFFF;
    uint32_t addr   = (instruction)       & 0xFFFFFF;

    uint32_t *reg = m->cpu.registers;
    uint8_t  *mem = m->memory;

    // EXECUTE
    switch (opcode) {
        case OP_ADD:
            reg[rd] = reg[ra] + reg[rb];
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 1;
            break;
        case OP_SUB:
            reg[rd] = reg[ra] - reg[rb];
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 1;
            break;
        case OP_MUL:
            reg[rd] = reg[ra] * reg[rb];
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 3;
            break;
        case OP_DIV:
            if (reg[rb] == 0) { m->cpu.halted = 1; return; }
            reg[rd] = reg[ra] / reg[rb];
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 5;
            break;
        case OP_AND:
            reg[rd] = reg[ra] & reg[rb];
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 1;
            break;
        case OP_OR:
            reg[rd] = reg[ra] | reg[rb];
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 1;
            break;
        case OP_XOR:
            reg[rd] = reg[ra] ^ reg[rb];
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 1;
            break;
        case OP_NOT:
            reg[rd] = ~reg[ra];
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 1;
            break;
        case OP_ADDI:
            reg[rd] = reg[ra] + (uint32_t)imm;
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 1;
            break;
        case OP_LOAD:
            reg[rd] = read_word(mem, reg[ra] + imm);
            m->cpu.cycle_count += 2;
            break;
        case OP_STORE:
            write_word(mem, reg[ra] + imm, reg[rd]);
            m->cpu.cycle_count += 2;
            break;
        case OP_JMP:
            reg[REG_PC] = addr;
            m->cpu.cycle_count += 2;
            break;
        case OP_CALL:
            reg[REG_LR] = reg[REG_PC];
            reg[REG_PC] = addr;
            m->cpu.cycle_count += 3;
            break;
        case OP_RET:
            reg[REG_PC] = reg[REG_LR];
            m->cpu.cycle_count += 3;
            break;
        case OP_HALT:
            m->cpu.halted = 1;
            m->cpu.cycle_count += 1;
            break;
        case OP_BEQ:
            if (m->cpu.flags & FLAG_ZERO)
                reg[REG_PC] = reg[ra] + imm;
            m->cpu.cycle_count += 2;
            break;
        case OP_BNE:
            if (!(m->cpu.flags & FLAG_ZERO))
                reg[REG_PC] = reg[ra] + imm;
            m->cpu.cycle_count += 2;
            break;
        case OP_BLT:
            if (m->cpu.flags & FLAG_NEGATIVE)
                reg[REG_PC] = reg[ra] + imm;
            m->cpu.cycle_count += 2;
            break;
        case OP_BGT:
            if (!(m->cpu.flags & FLAG_NEGATIVE) && !(m->cpu.flags & FLAG_ZERO))
                reg[REG_PC] = reg[ra] + imm;
            m->cpu.cycle_count += 2;
            break;
        case OP_PUSH:
            write_word(mem, reg[REG_SP], reg[ra]);
            reg[REG_SP] -= 4;
            m->cpu.cycle_count += 2;
            break;
        case OP_POP:
            reg[REG_SP] += 4;
            reg[rd] = read_word(mem, reg[REG_SP]);
            m->cpu.cycle_count += 2;
            break;
        default:
            m->cpu.halted = 1;
            break;
    }
}

void machine_run(Machine *m) {
    uint64_t limit = 1000;
    while (!m->cpu.halted && m->cpu.cycle_count < limit) {
        machine_step(m);
    }
    if (!m->cpu.halted) {
        printf("WARNING: hit cycle limit -- CPU never halted\n");
        printf("PC at limit: 0x%08X\n", m->cpu.registers[REG_PC]);
    }
}
