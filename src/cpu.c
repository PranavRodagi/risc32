
#include <stdio.h>
#include <stdarg.h>
#include "../include/cpu.h"
 
// ─── tracer state ─────────────────────────────────────────────────
static FILE *trace_file    = NULL;
static int   trace_enabled = 0;
 
void tracer_open(void) {
    trace_file = fopen(TRACE_FILE, "w");
    if (!trace_file)
        fprintf(stderr, "Warning: could not open trace.log\n");
}
 
void tracer_close(void) {
    if (trace_file) {
        fclose(trace_file);
        trace_file = NULL;
    }
}
 
void tracer_enable(int on) {
    trace_enabled = on;
}
 
static void trace_write(const char *fmt, ...) {
    if (!trace_enabled || !trace_file) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(trace_file, fmt, args);
    va_end(args);
}
 
static uint32_t fnv1a_hash(uint8_t *data, uint32_t length) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}
 
static void xor_cipher(uint8_t *data, uint32_t length, uint32_t key) {
    uint8_t key_bytes[4] = {
        (key)       & 0xFF,
        (key >> 8)  & 0xFF,
        (key >> 16) & 0xFF,
        (key >> 24) & 0xFF
    };
    for (uint32_t i = 0; i < length; i++)
        data[i] ^= key_bytes[i % 4];
}
 
static int constant_time_compare(uint8_t *a, uint8_t *b, uint32_t length) {
    uint8_t diff = 0;
    for (uint32_t i = 0; i < length; i++)
        diff |= a[i] ^ b[i];
    return diff == 0;
}
 
static void secure_zero(uint8_t *data, uint32_t length) {
    volatile uint8_t *p = data;
    for (uint32_t i = 0; i < length; i++)
        p[i] = 0;
}
 
static uint32_t rng_state = 12345;
 
static uint32_t lcg_random(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}
 
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
 
    // snapshot before execution
    uint32_t reg_before[NUM_REGISTERS];
    for (int i = 0; i < NUM_REGISTERS; i++)
        reg_before[i] = m->cpu.registers[i];
    uint8_t flags_before = m->cpu.flags;
 
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
    uint32_t jaddr  = addr;
 
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
        case OP_HASH: {
            uint32_t base   = reg[ra];
            uint32_t length = (uint32_t)imm;
            if (base + length > MEMORY_SIZE) { m->cpu.halted = 1; return; }
            reg[rd] = fnv1a_hash(mem + base, length);
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 4 + length;
            break;
        }
        case OP_ENCRYPT: {
            if (m->cpu.privilege_mode != MODE_SECURE) {
                printf("[SECURITY FAULT] ENCRYPT called from USER mode\n");
                m->cpu.halted = 1; return;
            }
            uint32_t base   = reg[ra];
            uint32_t length = (uint32_t)imm;
            if (base + length > MEMORY_SIZE) { m->cpu.halted = 1; return; }
            xor_cipher(mem + base, length, reg[REG_KEY]);
            m->cpu.cycle_count += 8 + length;
            break;
        }
        case OP_DECRYPT: {
            if (m->cpu.privilege_mode != MODE_SECURE) {
                printf("[SECURITY FAULT] DECRYPT called from USER mode\n");
                m->cpu.halted = 1; return;
            }
            uint32_t base   = reg[ra];
            uint32_t length = (uint32_t)imm;
            if (base + length > MEMORY_SIZE) { m->cpu.halted = 1; return; }
            xor_cipher(mem + base, length, reg[REG_KEY]);
            m->cpu.cycle_count += 8 + length;
            break;
        }
        case OP_VERIFY: {
            uint32_t base_a = reg[ra];
            uint32_t base_b = reg[rb];
            uint32_t length = (uint32_t)imm;
            if (base_a + length > MEMORY_SIZE || base_b + length > MEMORY_SIZE) {
                m->cpu.halted = 1; return;
            }
            int equal = constant_time_compare(mem + base_a, mem + base_b, length);
            m->cpu.flags = 0;
            if (equal) m->cpu.flags |= FLAG_ZERO;
            m->cpu.cycle_count += 4 + length;
            break;
        }
        case OP_SECURE_ERASE: {
            uint32_t base   = reg[ra];
            uint32_t length = (uint32_t)imm;
            if (base + length > MEMORY_SIZE) { m->cpu.halted = 1; return; }
            secure_zero(mem + base, length);
            m->cpu.cycle_count += 2 + length;
            break;
        }
        case OP_RANDOM:
            reg[rd] = lcg_random();
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 2;
            break;
        default:
            m->cpu.halted = 1;
            break;
    }
 
    // TRACE
    if (trace_enabled && trace_file) {
        trace_write("[cycle=%06lu] 0x%04X  ", m->cpu.cycle_count, pc);
 
        switch (opcode) {
            case OP_ADD:          trace_write("ADD  R%d, R%d, R%d",  rd, ra, rb);  break;
            case OP_SUB:          trace_write("SUB  R%d, R%d, R%d",  rd, ra, rb);  break;
            case OP_MUL:          trace_write("MUL  R%d, R%d, R%d",  rd, ra, rb);  break;
            case OP_DIV:          trace_write("DIV  R%d, R%d, R%d",  rd, ra, rb);  break;
            case OP_AND:          trace_write("AND  R%d, R%d, R%d",  rd, ra, rb);  break;
            case OP_OR:           trace_write("OR   R%d, R%d, R%d",  rd, ra, rb);  break;
            case OP_XOR:          trace_write("XOR  R%d, R%d, R%d",  rd, ra, rb);  break;
            case OP_NOT:          trace_write("NOT  R%d, R%d",        rd, ra);      break;
            case OP_ADDI:         trace_write("ADDI R%d, R%d, %d",   rd, ra, imm); break;
            case OP_LOAD:         trace_write("LOAD R%d, R%d, %d",   rd, ra, imm); break;
            case OP_STORE:        trace_write("STORE R%d, R%d, %d",  rd, ra, imm); break;
            case OP_JMP:          trace_write("JMP  0x%04X",          jaddr);       break;
            case OP_CALL:         trace_write("CALL 0x%04X",          jaddr);       break;
            case OP_RET:          trace_write("RET");                               break;
            case OP_HALT:         trace_write("HALT");                              break;
            case OP_BEQ:          trace_write("BEQ  R%d, 0x%04X",    ra, imm);     break;
            case OP_BNE:          trace_write("BNE  R%d, 0x%04X",    ra, imm);     break;
            case OP_BLT:          trace_write("BLT  R%d, 0x%04X",    ra, imm);     break;
            case OP_BGT:          trace_write("BGT  R%d, 0x%04X",    ra, imm);     break;
            case OP_PUSH:         trace_write("PUSH R%d",             ra);          break;
            case OP_POP:          trace_write("POP  R%d",             rd);          break;
            case OP_HASH:         trace_write("HASH R%d, R%d, %d",   rd, ra, imm); break;
            case OP_ENCRYPT:      trace_write("ENCRYPT R%d, %d",      ra, imm);     break;
            case OP_DECRYPT:      trace_write("DECRYPT R%d, %d",      ra, imm);     break;
            case OP_SECURE_ERASE: trace_write("SERASE R%d, %d",       ra, imm);     break;
            case OP_RANDOM:       trace_write("RANDOM R%d",            rd);          break;
            default:              trace_write("??? (0x%02X)",          opcode);      break;
        }
 
        int any_change = 0;
        for (int i = 0; i < NUM_REGISTERS; i++) {
            if (m->cpu.registers[i] != reg_before[i]) {
                trace_write("  R%d: 0x%08X -> 0x%08X",
                    i, reg_before[i], m->cpu.registers[i]);
                any_change = 1;
            }
        }
 
        if (m->cpu.flags != flags_before) {
            trace_write("  flags: %s%s%s%s -> %s%s%s%s",
                (flags_before & FLAG_ZERO)     ? "Z" : "-",
                (flags_before & FLAG_NEGATIVE) ? "N" : "-",
                (flags_before & FLAG_CARRY)    ? "C" : "-",
                (flags_before & FLAG_OVERFLOW) ? "V" : "-",
                (m->cpu.flags & FLAG_ZERO)     ? "Z" : "-",
                (m->cpu.flags & FLAG_NEGATIVE) ? "N" : "-",
                (m->cpu.flags & FLAG_CARRY)    ? "C" : "-",
                (m->cpu.flags & FLAG_OVERFLOW) ? "V" : "-");
        }
 
        if (!any_change)
            trace_write("  (no register change)");
 
        trace_write("\n");
    }
}
 
void machine_run(Machine *m) {
    while (!m->cpu.halted) {
        machine_step(m);
    }
}