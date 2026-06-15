#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/cpu.h"
#include "assembler.h"

// ─── helpers ──────────────────────────────────────────────────────

// Trim leading whitespace, return pointer into the string
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    return s;
}

// Parse "R0"..."R15" and return the register number, or -1 on error
static int parse_register(const char *s) {
    if (s[0] != 'R' && s[0] != 'r') return -1;
    int num = atoi(s + 1);
    if (num < 0 || num > 15) return -1;
    return num;
}

// Parse a number: decimal (10), hex (0x1F), or register alias (SP, LR, PC)
static int parse_immediate(const char *s) {
    if (strncmp(s, "0x", 2) == 0 || strncmp(s, "0X", 2) == 0)
        return (int)strtol(s, NULL, 16);
    if (strcmp(s, "SP") == 0) return REG_SP;
    if (strcmp(s, "LR") == 0) return REG_LR;
    if (strcmp(s, "PC") == 0) return REG_PC;
    return atoi(s);
}

// Write a 32-bit instruction into memory little-endian
static void emit(Machine *m, uint32_t *addr, uint32_t instr) {
    m->memory[*addr + 0] = (instr)       & 0xFF;
    m->memory[*addr + 1] = (instr >>  8) & 0xFF;
    m->memory[*addr + 2] = (instr >> 16) & 0xFF;
    m->memory[*addr + 3] = (instr >> 24) & 0xFF;
    *addr += 4;
}

// ─── assemble one line ────────────────────────────────────────────
// Returns 1 on success, 0 on empty/comment, -1 on error
static int assemble_line(Machine *m, uint32_t *addr,
                          char *line, int line_num) {
    line = trim(line);

    // skip blank lines and comments
    if (*line == '\0' || *line == ';' || *line == '#') return 0;

    // remove inline comments
    char *comment = strchr(line, ';');
    if (comment) *comment = '\0';
    comment = strchr(line, '#');
    if (comment) *comment = '\0';

    // tokenize: split on spaces and commas
    char *tokens[8];
    int   count = 0;
    char *copy  = strdup(line);
    char *tok   = strtok(copy, " ,\t\n\r");
    while (tok && count < 8) {
        tokens[count++] = tok;
        tok = strtok(NULL, " ,\t\n\r");
    }

    if (count == 0) { free(copy); return 0; }

    // uppercase the mnemonic for case-insensitive matching
    char mnemonic[16];
    strncpy(mnemonic, tokens[0], 15);
    mnemonic[15] = '\0';
    for (int i = 0; mnemonic[i]; i++)
        mnemonic[i] = toupper((unsigned char)mnemonic[i]);

    // ── instruction encoding ──────────────────────────────────────
    uint32_t instr = 0;
    int rd, ra, rb, imm;

    // TYPE R — three register instructions
    if (strcmp(mnemonic, "ADD") == 0 || strcmp(mnemonic, "SUB") == 0 ||
        strcmp(mnemonic, "MUL") == 0 || strcmp(mnemonic, "DIV") == 0 ||
        strcmp(mnemonic, "AND") == 0 || strcmp(mnemonic, "OR")  == 0 ||
        strcmp(mnemonic, "XOR") == 0) {
        if (count < 4) { fprintf(stderr, "Line %d: %s needs 3 registers\n", line_num, mnemonic); free(copy); return -1; }
        rd = parse_register(tokens[1]);
        ra = parse_register(tokens[2]);
        rb = parse_register(tokens[3]);
        if (rd < 0 || ra < 0 || rb < 0) { fprintf(stderr, "Line %d: bad register\n", line_num); free(copy); return -1; }
        uint8_t op = strcmp(mnemonic,"ADD")==0 ? OP_ADD :
                     strcmp(mnemonic,"SUB")==0 ? OP_SUB :
                     strcmp(mnemonic,"MUL")==0 ? OP_MUL :
                     strcmp(mnemonic,"DIV")==0 ? OP_DIV :
                     strcmp(mnemonic,"AND")==0 ? OP_AND :
                     strcmp(mnemonic,"OR") ==0 ? OP_OR  :
                     strcmp(mnemonic,"XOR")==0 ? OP_XOR : 0;
        instr = ENCODE_R(op, rd, ra, rb);
        emit(m, addr, instr);
    }

    // NOT — two register
    else if (strcmp(mnemonic, "NOT") == 0) {
        if (count < 3) { fprintf(stderr, "Line %d: NOT needs 2 registers\n", line_num); free(copy); return -1; }
        rd = parse_register(tokens[1]);
        ra = parse_register(tokens[2]);
        instr = ENCODE_R(OP_NOT, rd, ra, 0);
        emit(m, addr, instr);
    }

    // TYPE I — register + immediate
    else if (strcmp(mnemonic, "ADDI") == 0) {
        if (count < 4) { fprintf(stderr, "Line %d: ADDI needs Rd, Ra, imm\n", line_num); free(copy); return -1; }
        rd  = parse_register(tokens[1]);
        ra  = parse_register(tokens[2]);
        imm = parse_immediate(tokens[3]);
        instr = ENCODE_I(OP_ADDI, rd, ra, imm);
        emit(m, addr, instr);
    }

    else if (strcmp(mnemonic, "LOAD") == 0) {
        if (count < 4) { fprintf(stderr, "Line %d: LOAD needs Rd, Ra, imm\n", line_num); free(copy); return -1; }
        rd  = parse_register(tokens[1]);
        ra  = parse_register(tokens[2]);
        imm = parse_immediate(tokens[3]);
        instr = ENCODE_I(OP_LOAD, rd, ra, imm);
        emit(m, addr, instr);
    }

    else if (strcmp(mnemonic, "STORE") == 0) {
        if (count < 4) { fprintf(stderr, "Line %d: STORE needs Rd, Ra, imm\n", line_num); free(copy); return -1; }
        rd  = parse_register(tokens[1]);
        ra  = parse_register(tokens[2]);
        imm = parse_immediate(tokens[3]);
        instr = ENCODE_I(OP_STORE, rd, ra, imm);
        emit(m, addr, instr);
    }

    // BRANCHES — Rd is unused, Ra is base, imm is offset
    else if (strcmp(mnemonic, "BEQ") == 0 || strcmp(mnemonic, "BNE") == 0 ||
             strcmp(mnemonic, "BLT") == 0 || strcmp(mnemonic, "BGT") == 0) {
        if (count < 3) { fprintf(stderr, "Line %d: branch needs Ra, imm\n", line_num); free(copy); return -1; }
        ra  = parse_register(tokens[1]);
        imm = parse_immediate(tokens[2]);
        uint8_t op = strcmp(mnemonic,"BEQ")==0 ? OP_BEQ :
                     strcmp(mnemonic,"BNE")==0 ? OP_BNE :
                     strcmp(mnemonic,"BLT")==0 ? OP_BLT : OP_BGT;
        instr = ENCODE_I(op, 0, ra, imm);
        emit(m, addr, instr);
    }

    // TYPE J — jump and call
    else if (strcmp(mnemonic, "JMP") == 0) {
        if (count < 2) { fprintf(stderr, "Line %d: JMP needs address\n", line_num); free(copy); return -1; }
        imm = parse_immediate(tokens[1]);
        instr = ENCODE_J(OP_JMP, imm);
        emit(m, addr, instr);
    }

    else if (strcmp(mnemonic, "CALL") == 0) {
        if (count < 2) { fprintf(stderr, "Line %d: CALL needs address\n", line_num); free(copy); return -1; }
        imm = parse_immediate(tokens[1]);
        instr = ENCODE_J(OP_CALL, imm);
        emit(m, addr, instr);
    }

    // NO-OPERAND instructions
    else if (strcmp(mnemonic, "RET")  == 0) { emit(m, addr, ENCODE_R(OP_RET,  0,0,0)); }
    else if (strcmp(mnemonic, "HALT") == 0) { emit(m, addr, ENCODE_R(OP_HALT, 0,0,0)); }
    else if (strcmp(mnemonic, "PUSH") == 0) {
        if (count < 2) { fprintf(stderr, "Line %d: PUSH needs a register\n", line_num); free(copy); return -1; }
        ra = parse_register(tokens[1]);
        instr = ENCODE_R(OP_PUSH, 0, ra, 0);
        emit(m, addr, instr);
    }
    else if (strcmp(mnemonic, "POP") == 0) {
        if (count < 2) { fprintf(stderr, "Line %d: POP needs a register\n", line_num); free(copy); return -1; }
        rd = parse_register(tokens[1]);
        instr = ENCODE_R(OP_POP, rd, 0, 0);
        emit(m, addr, instr);
    }

    else {
        fprintf(stderr, "Line %d: unknown mnemonic '%s'\n", line_num, mnemonic);
        free(copy);
        return -1;
    }

    free(copy);
    return 1;
}

// ─── public API ───────────────────────────────────────────────────

int assemble_string(Machine *m, const char *source) {
    char *copy = strdup(source);
    uint32_t addr = 0;
    int line_num  = 1;
    int count     = 0;

    char *line = strtok(copy, "\n");
    while (line) {
        int result = assemble_line(m, &addr, line, line_num++);
        if (result == -1) { free(copy); return -1; }
        if (result == 1)  count++;
        line = strtok(NULL, "\n");
    }
    free(copy);
    return count;
}

int assemble_file(Machine *m, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return -1;
    }

    uint32_t addr    = 0;
    int      line_num = 1;
    int      count    = 0;
    char     line[256];

    while (fgets(line, sizeof(line), f)) {
        int result = assemble_line(m, &addr, line, line_num++);
        if (result == -1) { fclose(f); return -1; }
        if (result == 1)  count++;
    }

    fclose(f);
    return count;
}