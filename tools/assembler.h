#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "../include/cpu.h"

// Assembles a .asm file and loads it into machine memory starting at addr 0
// Returns the number of instructions written, or -1 on error
int assemble_file(Machine *m, const char *filename);

// Assembles a string directly (useful for testing)
int assemble_string(Machine *m, const char *source);

#endif