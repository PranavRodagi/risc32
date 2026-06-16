#include <stdio.h>
#include "../include/cpu.h"

// ─── FNV-1a hash ──────────────────────────────────────────────────
// A real non-cryptographic hash. Fast, simple, deterministic.
// Used here to demonstrate hardware hash acceleration concept.
static uint32_t fnv1a_hash(uint8_t *data, uint32_t length) {
    uint32_t hash = 2166136261u;   // FNV offset basis
    for (uint32_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 16777619u;         // FNV prime
    }
    return hash;
}

// ─── XOR stream cipher ────────────────────────────────────────────
// Encrypts/decrypts memory in place using R12 as the key.
// XOR is its own inverse: encrypt and decrypt are identical operations.
// Real hardware uses AES — the interface concept is the same.
static void xor_cipher(uint8_t *data, uint32_t length, uint32_t key) {
    // expand the 32-bit key into 4 bytes
    uint8_t key_bytes[4] = {
        (key)       & 0xFF,
        (key >> 8)  & 0xFF,
        (key >> 16) & 0xFF,
        (key >> 24) & 0xFF
    };
    for (uint32_t i = 0; i < length; i++)
        data[i] ^= key_bytes[i % 4];   // cycle through key bytes
}

// ─── constant-time comparison ─────────────────────────────────────
// CRITICAL: this must not short-circuit.
// A naive strcmp stops at the first difference — an attacker can
// measure how long it takes and deduce how many bytes matched.
// Constant-time means the loop always runs to completion regardless
// of where the first difference is.
static int constant_time_compare(uint8_t *a, uint8_t *b, uint32_t length) {
    uint8_t diff = 0;
    for (uint32_t i = 0; i < length; i++)
        diff |= a[i] ^ b[i];   // accumulate differences without branching
    return diff == 0;           // 1 if equal, 0 if not
}

// ─── guaranteed memory zeroing ────────────────────────────────────
// CRITICAL: a naive memset to zero can be optimized away by the
// compiler if it thinks the memory is never read again.
// volatile forces every write to actually happen.
static void secure_zero(uint8_t *data, uint32_t length) {
    volatile uint8_t *p = data;
    for (uint32_t i = 0; i < length; i++)
        p[i] = 0;
}

// ─── linear congruential random number generator ──────────────────
static uint32_t rng_state = 12345;   // seed

static uint32_t lcg_random(void) {
    rng_state = rng_state * 1664525u + 1013904223u;   // Numerical Recipes constants
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
        case OP_HASH: {
            // HASH Rd, Ra, len
            // Rd = fnv1a(memory[Ra .. Ra + imm])
            uint32_t base   = reg[ra];
            uint32_t length = (uint32_t)imm;
            if (base + length > MEMORY_SIZE) {
                m->cpu.halted = 1;   // memory bounds violation
                return;
            }
            reg[rd] = fnv1a_hash(mem + base, length);
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 4 + length;   // cost scales with data size
            break;
        }

        case OP_ENCRYPT: {
            // ENCRYPT Ra, len
            // encrypt memory[Ra .. Ra+imm] using R12 as key
            if (m->cpu.privilege_mode != MODE_SECURE) {
                printf("[SECURITY FAULT] ENCRYPT called from USER mode\n");
                m->cpu.halted = 1;
                return;
            }
            uint32_t base   = reg[ra];
            uint32_t length = (uint32_t)imm;
            if (base + length > MEMORY_SIZE) {
                m->cpu.halted = 1;
                return;
            }
            uint32_t key = reg[REG_KEY];   // always from R12, never exposed
            xor_cipher(mem + base, length, key);
            m->cpu.cycle_count += 8 + length;
            break;
        }

        case OP_DECRYPT: {
            // DECRYPT Ra, len
            // decrypt memory[Ra .. Ra+imm] using R12 as key
            if (m->cpu.privilege_mode != MODE_SECURE) {
                printf("[SECURITY FAULT] DECRYPT called from USER mode\n");
                m->cpu.halted = 1;
                return;
            }
            uint32_t base   = reg[ra];
            uint32_t length = (uint32_t)imm;
            if (base + length > MEMORY_SIZE) {
                m->cpu.halted = 1;
                return;
            }
            uint32_t key = reg[REG_KEY];
            xor_cipher(mem + base, length, key);   // XOR is its own inverse
            m->cpu.cycle_count += 8 + length;
            break;
        }

        case OP_VERIFY: {
            // VERIFY Ra, Rb, len
            // constant-time compare memory[Ra..+imm] vs memory[Rb..+imm]
            // sets ZERO flag if equal, clears it if not
            uint32_t base_a = reg[ra];
            uint32_t base_b = reg[rb];
            uint32_t length = (uint32_t)imm;
            if (base_a + length > MEMORY_SIZE ||
                base_b + length > MEMORY_SIZE) {
                m->cpu.halted = 1;
                return;
            }
            int equal = constant_time_compare(mem + base_a,
                                              mem + base_b, length);
            m->cpu.flags = 0;
            if (equal) m->cpu.flags |= FLAG_ZERO;
            m->cpu.cycle_count += 4 + length;   // always same cost — no timing leak
            break;
        }

        case OP_SECURE_ERASE: {
            // SECURE_ERASE Ra, len
            // guaranteed zero memory[Ra .. Ra+imm]
            uint32_t base   = reg[ra];
            uint32_t length = (uint32_t)imm;
            if (base + length > MEMORY_SIZE) {
                m->cpu.halted = 1;
                return;
            }
            secure_zero(mem + base, length);
            m->cpu.cycle_count += 2 + length;
            break;
        }

        case OP_RANDOM: {
            // RANDOM Rd
            // Rd = random 32-bit value
            reg[rd] = lcg_random();
            update_flags(&m->cpu, reg[rd]);
            m->cpu.cycle_count += 2;
            break;
        }
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
