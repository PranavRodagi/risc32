#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "../include/cpu.h"

void debugger_start(Machine *m);
void disassemble(Machine *m, uint32_t addr);

#endif