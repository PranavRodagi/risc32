# RISC-32 Virtual CPU

A custom 32-bit RISC processor emulator built from scratch in C.
Designed as a deep-dive into computer architecture, embedded systems,
and security-oriented ISA design.

## What this is

RISC-32 is a virtual microcontroller platform with realistic architectural
decisions and a full toolchain. It is not a toy emulator — it is a
miniature embedded platform built to understand how real CPUs work.

Architecturally analogous to ARM Cortex-M:
- 16 general-purpose 32-bit registers (R0–R15)
- Fixed 32-bit instruction encoding
- Load/store memory model
- Little-endian byte order
- Deterministic cycle-accurate execution

## Features

### CPU Core
- Full fetch-decode-execute pipeline
- 16 registers including dedicated SP, LR, PC, and crypto key register
- Flag register (Zero, Negative, Carry, Overflow)
- User and Secure privilege modes
- Cycle-accurate execution — every instruction has a defined cost

### Instruction Set
| Category | Instructions |
|---|---|
| Arithmetic | ADD, SUB, MUL, DIV, ADDI |
| Logic | AND, OR, XOR, NOT |
| Memory | LOAD, STORE, PUSH, POP |
| Control flow | JMP, CALL, RET, HALT |
| Branches | BEQ, BNE, BLT, BGT |
| Security | HASH, ENCRYPT, DECRYPT, VERIFY, SECURE_ERASE *(in progress)* |

### Toolchain
- **Assembler** — two-pass assembler with label support
- **Disassembler** — binary to human-readable assembly
- **Interactive debugger** — step execution, breakpoints, memory inspector

## Project structure
risc32/

├── include/

│   └── cpu.h          # CPU data structures, opcodes, encoding macros

├── src/

│   ├── cpu.c          # Fetch-decode-execute engine

│   └── main.c         # Entry point

├── tools/

│   ├── assembler.c/h  # Two-pass assembler

│   └── debugger.c/h   # Interactive debugger and disassembler

└── tests/

└── count.asm      # Example: loop counting 0 to 5

## Building

Requires GCC and Make (Linux or WSL).

```bash
gcc src/main.c src/cpu.c tools/assembler.c tools/debugger.c \
    -I include -I tools -o risc32
```

## Running the debugger

```bash
./risc32
```

Debugger commands:

| Command | Action |
|---|---|
| `s` | Step one instruction |
| `r` | Run until halt or breakpoint |
| `d` | Dump all registers |
| `p` | Disassemble next 8 instructions |
| `m <addr>` | Inspect memory at hex address |
| `b <addr>` | Set breakpoint |
| `bl` | List breakpoints |
| `bc <n>` | Clear breakpoint n |
| `reset` | Reset CPU, preserve memory |
| `q` | Quit |

## Example program

```asm
; count.asm — counts R0 from 0 to 5
        ADDI R1, R1, 5     ; R1 = limit
        ADDI R2, R2, 1     ; R2 = increment
loop:
        ADD  R0, R0, R2    ; R0++
        SUB  R3, R0, R1    ; compare
        BEQ  R5, halt      ; if equal, done
        JMP  loop
halt:
        HALT
```
Assembled 7 instructions

cycles: 31   (deterministic — always exactly 31)

R0: 5

## Deterministic timing

Every instruction has a fixed cycle cost:

| Instruction | Cycles | Rationale |
|---|---|---|
| ADD, SUB, logic | 1 | Single ALU operation |
| MUL | 3 | Multi-cycle multiply |
| DIV | 5 | Iterative division |
| LOAD, STORE | 2 | Memory access penalty |
| JMP, branches | 2 | PC update cost |
| CALL, RET | 3 | Link register save/restore |

Given any program, you can predict its exact cycle count before running it.
This makes RISC-32 suitable for real-time reasoning — a core requirement
in embedded and robotics systems.

## Security architecture

RISC-32 includes hardware-inspired security features:

- **R12 (KEY register)** — dedicated cryptographic key storage,
  inaccessible in user mode
- **Privilege modes** — USER and SECURE execution contexts,
  analogous to ARM TrustZone
- **Security ISA** — hardware-accelerated HASH, ENCRYPT, DECRYPT,
  VERIFY, and SECURE_ERASE instructions *(in progress)*

## Background and motivation

Built as a summer project to learn computer architecture, embedded systems,
and C from the ground up. Every design decision — register count, instruction
encoding, memory model, security model — was made deliberately after
evaluating tradeoffs.

Design decisions documented in commit history.

## Roadmap

- [x] CPU core — fetch, decode, execute
- [x] Deterministic cycle counting
- [x] Two-pass assembler with labels
- [x] Interactive debugger with breakpoints
- [x] Security instruction set (HASH, ENCRYPT, DECRYPT)
- [x] Execution tracer and WCET analyzer
- [x] Memory protection unit
- [ ] Virtual peripherals (UART, SPI)
- [ ] RTOS-inspired scheduler
