#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define NUM_REGISTERS   16
#define MEMORY_SIZE     65536

#define REG_KEY  12
#define REG_SP   13
#define REG_LR   14
#define REG_PC   15

#define FLAG_ZERO      0x01
#define FLAG_NEGATIVE  0x02
#define FLAG_CARRY     0x04
#define FLAG_OVERFLOW  0x08

#define MODE_USER    0
#define MODE_SECURE  1

#define TRACE_FILE "trace.log"

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

#define OP_HASH         0x30
#define OP_ENCRYPT      0x31
#define OP_DECRYPT      0x32
#define OP_VERIFY       0x33
#define OP_SECURE_ERASE 0x34
#define OP_RANDOM       0x35

// ─── encoding macros ──────────────────────────────────────────────
#define ENCODE_R(op, rd, ra, rb) \
    ((uint32_t)(op) << 24 | (uint32_t)(rd) << 20 | \
     (uint32_t)(ra) << 16 | (uint32_t)(rb) << 12)

#define ENCODE_I(op, rd, ra, imm) \
    ((uint32_t)(op) << 24 | (uint32_t)(rd) << 20 | \
     (uint32_t)(ra) << 16 | ((uint32_t)(imm) & 0xFFFF))

#define ENCODE_J(op, addr) \
    ((uint32_t)(op) << 24 | ((uint32_t)(addr) & 0xFFFFFF))


// ─── MPU ──────────────────────────────────────────────────────────
#define MPU_MAX_REGIONS  8

#define MPU_READ         0x01
#define MPU_WRITE        0x02
#define MPU_EXECUTE      0x04
#define MPU_USER         0x01
#define MPU_SECURE       0x02

typedef struct {
    uint32_t base;
    uint32_t limit;
    uint8_t  permissions;   // MPU_READ | MPU_WRITE | MPU_EXECUTE
    uint8_t  allowed_modes; // MPU_USER | MPU_SECURE
    uint8_t  enabled;
} MPU_Region;

// ─── structs ──────────────────────────────────────────────────────
typedef struct {
    uint32_t registers[NUM_REGISTERS];
    uint8_t  flags;
    uint8_t  privilege_mode;
    uint8_t  halted;
    uint64_t cycle_count;
} CPU;

typedef struct {
    CPU        cpu;
    uint8_t    memory[MEMORY_SIZE];
    MPU_Region mpu[MPU_MAX_REGIONS];
    uint8_t    mpu_enabled;
} Machine;

// ─── function declarations ────────────────────────────────────────
void machine_init(Machine *m);
void machine_step(Machine *m);
void machine_run(Machine *m);
void machine_dump(Machine *m);

void mpu_add_region(Machine *m, int index, uint32_t base,
                    uint32_t limit, uint8_t perms, uint8_t modes);
void mpu_enable(Machine *m, int on);

void tracer_open(void);
void tracer_close(void);
void tracer_enable(int on);

#endif