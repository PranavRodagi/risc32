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
