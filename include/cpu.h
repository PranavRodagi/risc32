#ifndef CPU_H 
#define CPU_H

#include <stdint.h> //allows for fixed integer widths

#define NUM_REGISTERS 16
#define MEMORY_SIZE 65534 //equates to 64KB RAM

#define REG_KEY 12 //R12: crypto key register
#define REG_SP 13 //R13: stack pointer
#define REG_LR 14 //R14: link register (holds return address)
#define REG_PC 15 //R15: program counter

#define FLAG_ZERO 0x01 //last result was zero
#define FLAG_NEGATIVE 0x02 //last result was negative
#define FLAG_CARRY 0x04 //last operation produced a carry
#define FLAG_OVERFLOW 0x08 //last operation overflowed

#define MODE_USER 0
#define MODE_SECURE 1

// ─── opcodes ──────────────────────────────────────────────────────
#define OP_ADD   0x01
#define OP_SUB   0x02
#define OP_MUL   0x03
#define OP_DIV   0x04
#define OP_AND   0x05
#define OP_OR    0x06
#define OP_XOR   0x07
#define OP_NOT   0x08
#define OP_ADDI  0x09
#define OP_LOAD  0x0A
#define OP_STORE 0x0B
#define OP_JMP   0x0C
#define OP_CALL  0x0D
#define OP_RET   0x0E
#define OP_HALT  0x0F
#define OP_BEQ   0x10
#define OP_BNE   0x11
#define OP_BLT   0x12
#define OP_BGT   0x13
#define OP_PUSH  0x20
#define OP_POP   0x21

// ─── security opcodes ─────────────────────────────────────────────
#define OP_HASH         0x30   // R1 = hash(memory[R0 .. R0+imm])
#define OP_ENCRYPT      0x31   // encrypt memory[R0 .. R0+imm] using R12
#define OP_DECRYPT      0x32   // decrypt memory[R0 .. R0+imm] using R12
#define OP_VERIFY       0x33   // constant-time compare mem[R0..+imm] vs mem[R1..+imm]
#define OP_SECURE_ERASE 0x34   // guaranteed zero memory[R0 .. R0+imm]
#define OP_RANDOM       0x35   // Rd = random 32-bit value               

// security instructions use TYPE I format
// HASH    Rd, Ra, len   → Rd = hash(memory[Ra .. Ra+len])
// ENCRYPT Ra, len       → encrypt memory[Ra .. Ra+len] with R12
// DECRYPT Ra, len       → decrypt memory[Ra .. Ra+len] with R12
// VERIFY  Ra, Rb, len   → constant-time compare, result in flags
// SECURE_ERASE Ra, len  → zero memory[Ra .. Ra+len]
// RANDOM  Rd            → Rd = random value

// ─── helper: encode instructions into memory ──────────────────────
//use these in main.c to write test programs directly
#define ENCODE_R(op, rd, ra, rb) \
    ((uint32_t)(op) << 24 | (uint32_t)(rd) << 20 | \
     (uint32_t)(ra) << 16 | (uint32_t)(rb) << 12)

#define ENCODE_I(op, rd, ra, imm) \
    ((uint32_t)(op) << 24 | (uint32_t)(rd) << 20 | \
     (uint32_t)(ra) << 16 | ((uint32_t)(imm) & 0xFFFF))

#define ENCODE_J(op, addr) \
    ((uint32_t)(op) << 24 | ((uint32_t)(addr) & 0xFFFFFF))

typedef struct {
    uint32_t registers[NUM_REGISTERS]; //R0-15
    uint8_t flags; //zero/negative/carry/overflow
    uint8_t privilege_mode; //user or secure
    uint8_t halted; //1 = CPU has stopped
    uint64_t cycle_count;
} CPU;

typedef struct {
    CPU cpu;
    uint8_t memory [MEMORY_SIZE];
} Machine;

void machine_init(Machine *m);
void machine_step(Machine *m);
void machine_run(Machine *m);
void machine_dump(Machine *m);
//prints the CPU state for debugging

#endif