# RISC-32 Architecture Reference

This document describes the complete design of the RISC-32 virtual CPU platform —
the architectural decisions made, the tradeoffs considered, and the reasoning
behind every major component. It is written for someone who wants to understand
not just what was built, but why.

---

## Table of Contents

1. [Design Philosophy](#design-philosophy)
2. [Architecture Overview](#architecture-overview)
3. [Register Model](#register-model)
4. [Memory Model](#memory-model)
5. [Instruction Set Architecture](#instruction-set-architecture)
6. [Instruction Encoding](#instruction-encoding)
7. [Deterministic Timing Model](#deterministic-timing-model)
8. [Security Architecture](#security-architecture)
9. [Toolchain](#toolchain)
10. [Design Decisions and Tradeoffs](#design-decisions-and-tradeoffs)

---

## Design Philosophy

RISC-32 was designed around three principles that informed every decision:

**Determinism over performance.** Every instruction has a fixed, defined cycle cost.
There are no caches, no branch predictors, no out-of-order execution. A program's
execution time is fully predictable before it runs. This is the core requirement
of real-time embedded systems — a motor controller, a sensor fusion loop, a
safety-critical interrupt handler must all complete within a known time bound.

**Security as architecture, not afterthought.** Cryptographic operations are
first-class instructions. Key material lives in a dedicated register inaccessible
to unprivileged code. Privilege modes are enforced at the hardware level. These
decisions mirror what real secure microcontrollers implement — ARM TrustZone,
RISC-V Physical Memory Protection, TPM secure enclaves.

**Simplicity that teaches.** Every component is implemented at the level of
abstraction that reveals how it actually works. The assembler is a two-pass
text processor. The debugger is a command loop around a single step function.
The CPU is a switch statement over decoded instruction fields. Nothing is hidden.

---

## Architecture Overview

RISC-32 is a 32-bit load/store RISC architecture. Its defining characteristics:

- **Fixed 32-bit instruction width.** Every instruction is exactly 4 bytes.
  Fetch cost is constant and predictable.
- **16 general-purpose registers.** Each 32 bits wide. Four have designated
  roles (SP, LR, PC, KEY) by convention and hardware enforcement.
- **64KB flat address space.** Byte-addressable. Word accesses are 4-byte
  aligned. Memory is modeled as a single `uint8_t[65536]` array.
- **Little-endian byte order.** The least significant byte of a word lives
  at the lowest address. Consistent with ARM Cortex-M.
- **Two privilege modes.** USER and SECURE. Security instructions are
  restricted to SECURE mode. Violations halt the CPU with a fault.
- **Load/store memory model.** Arithmetic operates only on registers.
  To add two values in memory, they must first be loaded into registers,
  operated on, then stored back. This is the defining characteristic of
  RISC design and the reason cycle counting stays clean.

---

## Register Model

| Register | Name | Role |
|---|---|---|
| R0–R11 | General purpose | Arithmetic, logic, temporaries, arguments |
| R12 | KEY | Cryptographic key register. Hardware-protected in SECURE mode. Never exposed to general-purpose instructions during crypto operations. |
| R13 | SP | Stack pointer. Initialized to 0xFFFC. Grows downward. Decremented before write on PUSH, incremented before read on POP. |
| R14 | LR | Link register. CALL saves the return address here. RET reads it to restore PC. |
| R15 | PC | Program counter. Always holds the address of the next instruction to fetch. Advancing PC by 4 after fetch means branches and calls write their target directly into PC. |

### Why 16 registers

The register count determines how many bits each instruction must spend on
register fields. With 16 registers, each field is 4 bits. A three-register
instruction (opcode + Rd + Ra + Rb) costs 8 + 4 + 4 + 4 = 20 bits, leaving
12 bits for immediate values or flags — a comfortable budget in a 32-bit word.

32 registers (RISC-V style) would require 5 bits per field, tightening the
encoding budget. 8 registers would free bits but make register allocation
difficult for real programs — crypto operations alone need several temporaries.
16 is the ARM Cortex-M answer to the same tradeoff.

### The KEY register

R12 is reserved for cryptographic key material. ENCRYPT and DECRYPT use it
implicitly — they never take a key argument. This means:

- Key material never passes through general-purpose registers where it could
  be observed or leaked by unprivileged code
- A program cannot call ENCRYPT with an arbitrary key — it must first load
  the key into R12 through a privileged operation
- This mirrors ARM TrustZone's concept of secure-world-only resources

---

## Memory Model

Memory is a flat 64KB byte array. Address space layout by convention:

```
0x0000 – 0x00FF   Reserved (reset vector, interrupt table — future use)
0x0100 – 0x1FFF   Program text (instructions)
0x2000 – 0xDFFF   Data / heap
0xE000 – 0xFEFF   Secure memory region (future MPU enforcement)
0xFF00 – 0xFFFF   Stack (grows downward from 0xFFFC)
```

### Little-endian layout

A 32-bit value `0x12345678` stored at address `0x0100`:

```
Address   Value
0x0100    0x78   <- least significant byte
0x0101    0x56
0x0102    0x34
0x0103    0x12   <- most significant byte
```

Fetch assembles an instruction from four consecutive bytes:

```c
uint32_t instruction =
     (uint32_t)memory[pc + 0]
  | ((uint32_t)memory[pc + 1] << 8)
  | ((uint32_t)memory[pc + 2] << 16)
  | ((uint32_t)memory[pc + 3] << 24);
```

### Stack behavior

SP starts at `0xFFFC`. PUSH writes then decrements. POP increments then reads.
The stack grows downward — standard for ARM, x86, RISC-V, and most architectures.
Stack and heap growing toward each other from opposite ends of memory is the
universal solution to not knowing at compile time how much of each you need.

---

## Instruction Set Architecture

### Arithmetic

| Mnemonic | Operation | Cycles |
|---|---|---|
| ADD Rd, Ra, Rb | Rd = Ra + Rb | 1 |
| SUB Rd, Ra, Rb | Rd = Ra - Rb | 1 |
| MUL Rd, Ra, Rb | Rd = Ra x Rb | 3 |
| DIV Rd, Ra, Rb | Rd = Ra / Rb | 5 |
| ADDI Rd, Ra, imm | Rd = Ra + imm | 1 |

MUL costs 3 cycles and DIV costs 5 because multiplication and division are
genuinely more expensive in real hardware. A single-cycle multiplier requires
significantly more silicon area than an adder. DIV is typically implemented
as a shift-and-subtract loop. The cycle costs reflect this reality.

Division by zero halts the CPU. This is a hardware fault — the same behavior
as ARM's divide-by-zero trap and x86's #DE exception.

### Logic

| Mnemonic | Operation | Cycles |
|---|---|---|
| AND Rd, Ra, Rb | Rd = Ra & Rb | 1 |
| OR Rd, Ra, Rb | Rd = Ra OR Rb | 1 |
| XOR Rd, Ra, Rb | Rd = Ra ^ Rb | 1 |
| NOT Rd, Ra | Rd = ~Ra | 1 |

All single-cycle — bitwise operations are the simplest thing an ALU does.

### Memory

| Mnemonic | Operation | Cycles |
|---|---|---|
| LOAD Rd, Ra, imm | Rd = memory[Ra + imm] | 2 |
| STORE Rd, Ra, imm | memory[Ra + imm] = Rd | 2 |
| PUSH Ra | memory[SP] = Ra; SP -= 4 | 2 |
| POP Rd | SP += 4; Rd = memory[SP] | 2 |

Memory operations cost 2 cycles — one to compute the address, one to
perform the access. Real memory systems have much higher latency (cache
misses can cost hundreds of cycles), but the 2-cycle model captures the
key principle that memory access is more expensive than register access.

### Control Flow

| Mnemonic | Operation | Cycles |
|---|---|---|
| JMP addr | PC = addr | 2 |
| CALL addr | LR = PC; PC = addr | 3 |
| RET | PC = LR | 3 |
| HALT | halted = 1 | 1 |
| BEQ Ra, imm | if Z: PC = Ra + imm | 2 |
| BNE Ra, imm | if !Z: PC = Ra + imm | 2 |
| BLT Ra, imm | if N: PC = Ra + imm | 2 |
| BGT Ra, imm | if !N && !Z: PC = Ra + imm | 2 |

CALL costs one extra cycle over JMP because it must save the return address
to LR before jumping. RET costs the same as CALL — it reads LR and writes PC.

Branches use base-plus-offset addressing: PC = Ra + imm. Setting Ra to R0
(which is zero at program start) gives absolute addressing. Using a non-zero
Ra gives relative addressing. This single mechanism covers both cases.

### Security Instructions

| Mnemonic | Operation | Cycles |
|---|---|---|
| HASH Rd, Ra, len | Rd = fnv1a(memory[Ra .. Ra+len]) | 4 + len |
| ENCRYPT Ra, len | xor_cipher(memory[Ra .. Ra+len], R12) | 8 + len |
| DECRYPT Ra, len | xor_cipher(memory[Ra .. Ra+len], R12) | 8 + len |
| VERIFY Ra, Rb, len | constant_time_compare(mem[Ra], mem[Rb], len) | 4 + len |
| SERASE Ra, len | volatile_zero(memory[Ra .. Ra+len]) | 2 + len |
| RANDOM Rd | Rd = lcg_random() | 2 |

Security instructions have variable cycle costs that scale with data length.
This is intentional and accurate — hashing 1024 bytes takes longer than
hashing 4 bytes. The cost model makes timing analysis of security operations
tractable.

**ENCRYPT and DECRYPT require SECURE mode.** Calling them from USER mode
triggers a security fault and halts the CPU. This is privilege enforcement
at the hardware level.

**VERIFY uses constant-time comparison.** A naive comparison short-circuits
at the first differing byte — an attacker can measure execution time and
deduce how many bytes matched, leaking information about a secret. The
constant-time implementation accumulates differences with bitwise OR and
never branches on the comparison result:

```c
uint8_t diff = 0;
for (uint32_t i = 0; i < length; i++)
    diff |= a[i] ^ b[i];   // never exits early
return diff == 0;
```

This is the same technique used in OpenSSL, mbedTLS, and every serious
cryptographic library.

**SERASE uses volatile writes.** The C compiler is permitted to optimize
away writes to memory it believes will never be read again. A naive zeroing
loop over sensitive data can be silently eliminated. Marking the pointer
volatile forces every write to actually occur:

```c
volatile uint8_t *p = data;
for (uint32_t i = 0; i < length; i++)
    p[i] = 0;
```

This is a real vulnerability class — CVE entries exist for exactly this
optimization removing security-critical zeroing in production code.

---

## Instruction Encoding

Every instruction is exactly 32 bits. Three formats:

### Type R — register operations

```
 31      24 23    20 19    16 15    12 11       0
+----------+--------+--------+--------+----------+
|  opcode  |   Rd   |   Ra   |   Rb   |  unused  |
|  8 bits  | 4 bits | 4 bits | 4 bits | 12 bits  |
+----------+--------+--------+--------+----------+
```

### Type I — immediate operations

```
 31      24 23    20 19    16 15                 0
+----------+--------+--------+--------------------+
|  opcode  |   Rd   |   Ra   |     immediate      |
|  8 bits  | 4 bits | 4 bits |      16 bits       |
+----------+--------+--------+--------------------+
```

### Type J — jump operations

```
 31      24 23                                   0
+----------+------------------------------------+
|  opcode  |            address                  |
|  8 bits  |            24 bits                  |
+----------+------------------------------------+
```

### Decoding

Field extraction uses shift and mask — the universal tool for working with
packed binary formats:

```c
uint8_t  opcode = (instruction >> 24) & 0xFF;
uint8_t  rd     = (instruction >> 20) & 0xF;
uint8_t  ra     = (instruction >> 16) & 0xF;
uint8_t  rb     = (instruction >> 12) & 0xF;
uint16_t imm    =  instruction        & 0xFFFF;
uint32_t addr   =  instruction        & 0xFFFFFF;
```

Shift moves the target field to bit position zero. Mask zeroes everything
above it. The result is a plain integer used directly as an array index,
an operand, or an address. Encoding is the mirror operation — shift fields
left into position and OR them together.

---

## Deterministic Timing Model

Every instruction has a fixed cycle cost defined at design time. The CPU
maintains a uint64_t cycle_count that increments by the exact cost of
each instruction executed. Given any program, its total cycle count is
computable before running it.

### Why this matters

Real-time systems make timing guarantees. A flight control system must
respond to sensor input within a bounded time. A motor controller must
update PWM signals at a precise frequency. An automotive safety system
must complete its computation before a hard deadline.

Worst-Case Execution Time (WCET) analysis answers the question: what is
the longest possible time this program can take? WCET is only tractable
on deterministic architectures — caches, branch predictors, and
out-of-order execution make WCET analysis on modern desktop processors
extraordinarily difficult.

RISC-32 is designed to be WCET-analyzable by construction. There are no
variable-latency operations, no caches, no speculation. The cycle cost
of any program is the sum of the cycle costs of its instructions.

### Example

```asm
ADDI R1, R1, 5      ; 1 cycle
ADDI R2, R2, 1      ; 1 cycle
loop:
ADD  R0, R0, R2     ; 1 cycle  |
SUB  R3, R0, R1     ; 1 cycle  | x 5 iterations
BEQ  R5, halt       ; 2 cycles |
JMP  loop           ; 2 cycles | (skipped on last iteration)
halt:
HALT                ; 1 cycle

Total: 2 + (4 x 6) + 4 + 1 = 31 cycles. Always exactly 31.
```

---

## Security Architecture

### Threat model

RISC-32's security model addresses a specific threat: unprivileged code
attempting to access cryptographic key material or perform cryptographic
operations without authorization. This maps to real embedded threats —
a compromised application trying to exfiltrate device keys, or malicious
firmware attempting to bypass secure boot verification.

### Privilege modes

Two modes of execution:

**USER mode** — default. General-purpose instructions execute normally.
Security instructions that touch key material (ENCRYPT, DECRYPT) are
forbidden. Attempting them triggers a security fault.

**SECURE mode** — elevated privilege. All instructions permitted.
Key register R12 is accessible. Cryptographic operations execute normally.

Mode is stored in cpu.privilege_mode and checked before executing
security instructions. Future extensions would add a privileged instruction
to transition between modes with proper authentication.

### Key isolation

R12 is designated the KEY register by architectural convention. ENCRYPT
and DECRYPT use it implicitly — their encoding contains no key field.
This has two consequences:

1. The key cannot be specified inline in an instruction. An attacker
   who controls the instruction stream cannot choose an arbitrary key.
2. User-mode code cannot read R12 to extract the key (future MPU
   extension would enforce this in hardware).

This is the same principle as ARM TrustZone's secure-world-only memory
and the reason hardware security modules (HSMs) never expose raw key
material to software.

### Constant-time operations

VERIFY is implemented with a constant-time comparison. Timing side-channel
attacks on naive comparisons are a real and exploited vulnerability class.
The fix — accumulate differences without branching — is standard practice
in cryptographic implementations and is implemented here at the instruction
level rather than relying on software discipline.

### Secure erasure

SERASE guarantees memory zeroing using volatile writes, preventing the
compiler from optimizing away the erasure of sensitive data. This addresses
a real compiler optimization that has caused security vulnerabilities in
production code.

### Security workflow

The intended pattern for handling sensitive data:

```asm
; 1. load key into R12 (secure mode only)
ADDI R12, R12, KEY_VALUE

; 2. write sensitive data to memory
STORE R1, R0, 0

; 3. hash for integrity verification
HASH R4, R0, LENGTH

; 4. encrypt for confidentiality
ENCRYPT R0, LENGTH

; 5. decrypt when needed
DECRYPT R0, LENGTH

; 6. verify integrity after decrypt
HASH R5, R0, LENGTH
VERIFY R4_ADDR, R5_ADDR, 4

; 7. erase when done
SERASE R0, LENGTH
```

---

## Toolchain

### Assembler

A two-pass assembler that translates RISC-32 assembly text into binary
loaded directly into machine memory.

**Pass 1** scans the file collecting label definitions and their addresses.
A label is any identifier followed by a colon. Its address is the byte
offset of the next instruction after it.

**Pass 2** encodes instructions, resolving label references using the
table built in pass 1. Forward references — labels used before they are
defined — are handled correctly because pass 1 always runs first.

The two-pass design is how every assembler from the 1950s to today works.
GNU Assembler (GAS), NASM, and LLVM's MC layer all use this approach.

Assembly syntax:

```asm
; This is a comment
label:
    MNEMONIC Rd, Ra, Rb     ; Type R
    MNEMONIC Rd, Ra, imm    ; Type I — imm can be decimal, 0xHEX, or label
    MNEMONIC addr           ; Type J — addr can be 0xHEX or label
```

### Disassembler

The reverse of the assembler. Reads 4 bytes from memory, extracts fields
using shift and mask, and prints the human-readable mnemonic with operands.
Built into the debugger and displayed automatically during step execution.

### Debugger

An interactive REPL (Read-Eval-Print Loop) wrapping the CPU's step function.

```
dbg [0x0000]> s          step one instruction
dbg [0x0004]> r          run until halt or breakpoint
dbg [0x0048]> b 0x0018   set breakpoint at address
dbg [0x0048]> m 0x0200   inspect memory
dbg [0x0048]> d          dump all registers
dbg [0x0048]> p          disassemble next 8 instructions
dbg [0x0048]> reset      reset CPU, preserve memory
dbg [0x0048]> q          quit
```

Breakpoints are stored as an array of addresses. Before each step, the
debugger checks whether the current PC matches any breakpoint address.
If so, it returns control to the command loop. This is how GDB implements
software breakpoints — in real hardware debuggers, breakpoints temporarily
replace the target instruction with a trap instruction.

The disassembly window shows an arrow at the current PC and displays
several instructions ahead — the same view provided by GDB's layout asm
and most embedded IDE debuggers.

---

## Design Decisions and Tradeoffs

### Why not a stack machine

Stack machines (JVM bytecode, CPython bytecode, Forth) use an implicit
operand stack instead of named registers. They are simpler to implement
at the core but create problems for security-oriented design: passing
key material to a crypto instruction requires stack discipline rather
than a hardware-enforced register. Named registers make the security
model explicit and auditable.

### Why not variable-length instructions

Variable-length encodings (x86, ARM Thumb) pack more information into
fewer bytes but complicate the decoder. Fetch cost varies per instruction,
making cycle counting messier. Fixed 32-bit instructions mean fetch is
always one word read — constant cost, simple decoder, clean timing model.

### Why XOR cipher instead of AES

AES is the correct production choice. A correct, side-channel-resistant
AES implementation is approximately 500 lines of carefully written C with
hardware acceleration. The purpose of ENCRYPT and DECRYPT in RISC-32 is
to demonstrate the architectural interface — implicit key register, privilege
enforcement, fixed cycle cost — not to provide production cryptography.
The cipher algorithm is replaceable; the interface is the design.

### Why FNV-1a instead of SHA-256

Same reasoning. FNV-1a is a non-cryptographic hash — it is fast,
deterministic, and simple to implement. A real firmware integrity check
would use SHA-256 or SHA-3. The HASH instruction demonstrates hardware
hash acceleration, memory-region hashing interface, and integration with
the VERIFY workflow. The algorithm is an implementation detail.

### Why 64KB of memory

Large enough to hold real programs with data structures, small enough
to allocate as a stack variable without concern. The 16-bit address space
also means LOAD/STORE with a 16-bit immediate can directly address any
location — no high/low address pair needed for most programs.

### Why cycle_count is uint64_t

A 32-bit counter overflows after approximately 4 billion cycles. At a
hypothetical 100MHz clock, that is 40 seconds. A 64-bit counter overflows
after 1.8 x 10^19 cycles — effectively never. Timing infrastructure
should never be the source of subtle bugs.

---

*RISC-32 was designed and implemented as a summer project to learn computer
architecture, embedded systems, C, and security-oriented design from first
principles. Every component was built from scratch. Every design decision
was made after evaluating alternatives.*
