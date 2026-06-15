#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/cpu.h"
#include "assembler.h"

// ─── label table ──────────────────────────────────────────────────
#define MAX_LABELS 256

typedef struct {
    char     name[32];
    uint32_t address;
} Label;

static Label  label_table[MAX_LABELS];
static int    label_count = 0;

static void label_table_clear(void) {
    label_count = 0;
}

static void label_add(const char *name, uint32_t address) {
    if (label_count >= MAX_LABELS) {
        fprintf(stderr, "Too many labels\n");
        return;
    }
    strncpy(label_table[label_count].name, name, 31);
    label_table[label_count].name[31] = '\0';
    label_table[label_count].address  = address;
    label_count++;
}

// Returns the address of a label, or -1 if not found
static int label_find(const char *name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(label_table[i].name, name) == 0)
            return (int)label_table[i].address;
    }
    return -1;
}

// ─── helpers ──────────────────────────────────────────────────────

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    return s;
}

static int parse_register(const char *s) {
    if (s[0] != 'R' && s[0] != 'r') return -1;
    int num = atoi(s + 1);
    if (num < 0 || num > 15) return -1;
    return num;
}

// Parse immediate: decimal, hex, or label name
static int parse_value(const char *s) {
    if (strncmp(s, "0x", 2) == 0 || strncmp(s, "0X", 2) == 0)
        return (int)strtol(s, NULL, 16);
    if (strcmp(s, "SP") == 0) return REG_SP;
    if (strcmp(s, "LR") == 0) return REG_LR;
    if (strcmp(s, "PC") == 0) return REG_PC;
    // if it starts with a digit it's a number
    if (isdigit((unsigned char)s[0])) return atoi(s);
    // otherwise treat as a label — look it up
    int addr = label_find(s);
    if (addr < 0) fprintf(stderr, "Undefined label: '%s'\n", s);
    return addr;
}

static void emit(Machine *m, uint32_t *addr, uint32_t instr) {
    m->memory[*addr + 0] = (instr)       & 0xFF;
    m->memory[*addr + 1] = (instr >>  8) & 0xFF;
    m->memory[*addr + 2] = (instr >> 16) & 0xFF;
    m->memory[*addr + 3] = (instr >> 24) & 0xFF;
    *addr += 4;
}

// ─── check if a line is a label definition ────────────────────────
// Returns the label name (without colon) if it is, NULL if not
static char *get_label(char *line) {
    char *trimmed = trim(line);
    char *colon   = strchr(trimmed, ':');
    if (!colon) return NULL;

    // make sure nothing comes before the colon except identifier chars
    for (char *p = trimmed; p < colon; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') return NULL;
    }

    *colon = '\0';   // cut off the colon
    return trimmed;  // return just the name
}

// ─── count how many instructions a line produces ──────────────────
// Labels and blank lines produce 0. Every instruction produces 1 (= 4 bytes).
static int instruction_size(char *line) {
    char copy[256];
    strncpy(copy, line, 255);
    copy[255] = '\0';

    char *trimmed = trim(copy);
    if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') return 0;

    // remove inline comments
    char *c = strchr(trimmed, ';'); if (c) *c = '\0';
    c = strchr(trimmed, '#'); if (c) *c = '\0';

    // if it ends with a colon it's a label
    if (get_label(trimmed)) return 0;

    // everything else is one instruction = 4 bytes
    return 4;
}

// ─── assemble one line (pass 2) ───────────────────────────────────
static int assemble_line(Machine *m, uint32_t *addr,
                          char *line, int line_num) {
    char copy[256];
    strncpy(copy, line, 255);
    copy[255] = '\0';

    char *trimmed = trim(copy);
    if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') return 0;

    // remove inline comments
    char *comment = strchr(trimmed, ';'); if (comment) *comment = '\0';
    comment = strchr(trimmed, '#'); if (comment) *comment = '\0';

    // skip label definitions — already handled in pass 1
    {
        char labelcopy[256];
        strncpy(labelcopy, trimmed, 255);
        if (get_label(labelcopy)) return 0;
    }

    // tokenize
    char *tokens[8];
    int   count = 0;
    char *work  = strdup(trimmed);
    char *tok   = strtok(work, " ,\t\n\r");
    while (tok && count < 8) {
        tokens[count++] = tok;
        tok = strtok(NULL, " ,\t\n\r");
    }
    if (count == 0) { free(work); return 0; }

    // uppercase mnemonic
    char mnemonic[16];
    strncpy(mnemonic, tokens[0], 15);
    mnemonic[15] = '\0';
    for (int i = 0; mnemonic[i]; i++)
        mnemonic[i] = toupper((unsigned char)mnemonic[i]);

    uint32_t instr = 0;
    int rd, ra, rb, imm;

    // TYPE R
    if (strcmp(mnemonic,"ADD")==0 || strcmp(mnemonic,"SUB")==0 ||
        strcmp(mnemonic,"MUL")==0 || strcmp(mnemonic,"DIV")==0 ||
        strcmp(mnemonic,"AND")==0 || strcmp(mnemonic,"OR") ==0 ||
        strcmp(mnemonic,"XOR")==0) {
        rd = parse_register(tokens[1]);
        ra = parse_register(tokens[2]);
        rb = parse_register(tokens[3]);
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
    else if (strcmp(mnemonic,"NOT")==0) {
        rd = parse_register(tokens[1]);
        ra = parse_register(tokens[2]);
        instr = ENCODE_R(OP_NOT, rd, ra, 0);
        emit(m, addr, instr);
    }
    // TYPE I
    else if (strcmp(mnemonic,"ADDI")==0) {
        rd  = parse_register(tokens[1]);
        ra  = parse_register(tokens[2]);
        imm = parse_value(tokens[3]);
        instr = ENCODE_I(OP_ADDI, rd, ra, imm);
        emit(m, addr, instr);
    }
    else if (strcmp(mnemonic,"LOAD")==0) {
        rd  = parse_register(tokens[1]);
        ra  = parse_register(tokens[2]);
        imm = parse_value(tokens[3]);
        instr = ENCODE_I(OP_LOAD, rd, ra, imm);
        emit(m, addr, instr);
    }
    else if (strcmp(mnemonic,"STORE")==0) {
        rd  = parse_register(tokens[1]);
        ra  = parse_register(tokens[2]);
        imm = parse_value(tokens[3]);
        instr = ENCODE_I(OP_STORE, rd, ra, imm);
        emit(m, addr, instr);
    }
    // BRANCHES — now accept label names
    else if (strcmp(mnemonic,"BEQ")==0 || strcmp(mnemonic,"BNE")==0 ||
             strcmp(mnemonic,"BLT")==0 || strcmp(mnemonic,"BGT")==0) {
        ra  = parse_register(tokens[1]);
        imm = parse_value(tokens[2]);
        uint8_t op = strcmp(mnemonic,"BEQ")==0 ? OP_BEQ :
                     strcmp(mnemonic,"BNE")==0 ? OP_BNE :
                     strcmp(mnemonic,"BLT")==0 ? OP_BLT : OP_BGT;
        instr = ENCODE_I(op, 0, ra, imm);
        emit(m, addr, instr);
    }
    // TYPE J — now accept label names
    else if (strcmp(mnemonic,"JMP")==0) {
        imm = parse_value(tokens[1]);
        instr = ENCODE_J(OP_JMP, imm);
        emit(m, addr, instr);
    }
    else if (strcmp(mnemonic,"CALL")==0) {
        imm = parse_value(tokens[1]);
        instr = ENCODE_J(OP_CALL, imm);
        emit(m, addr, instr);
    }
    // NO-OPERAND
    else if (strcmp(mnemonic,"RET") ==0) { emit(m, addr, ENCODE_R(OP_RET,  0,0,0)); }
    else if (strcmp(mnemonic,"HALT")==0) { emit(m, addr, ENCODE_R(OP_HALT, 0,0,0)); }
    else if (strcmp(mnemonic,"PUSH")==0) {
        ra = parse_register(tokens[1]);
        instr = ENCODE_R(OP_PUSH, 0, ra, 0);
        emit(m, addr, instr);
    }
    else if (strcmp(mnemonic,"POP")==0) {
        rd = parse_register(tokens[1]);
        instr = ENCODE_R(OP_POP, rd, 0, 0);
        emit(m, addr, instr);
    }
    else {
        fprintf(stderr, "Line %d: unknown mnemonic '%s'\n", line_num, mnemonic);
        free(work);
        return -1;
    }

    free(work);
    return 1;
}

// ─── two-pass assembly from an array of lines ─────────────────────
static int assemble_lines(Machine *m, char **lines, int line_count) {
    label_table_clear();

    // ── PASS 1: collect labels ────────────────────────────────────
    uint32_t addr = 0;
    for (int i = 0; i < line_count; i++) {
        char copy[256];
        strncpy(copy, lines[i], 255);

        char *label = get_label(trim(copy));
        if (label) {
            label_add(label, addr);  // record label at current address
        } else {
            addr += instruction_size(lines[i]);  // advance address
        }
    }

    // ── PASS 2: encode instructions ───────────────────────────────
    addr = 0;
    int count = 0;
    for (int i = 0; i < line_count; i++) {
        int result = assemble_line(m, &addr, lines[i], i + 1);
        if (result == -1) return -1;
        if (result ==  1) count++;
    }

    return count;
}

// ─── public API ───────────────────────────────────────────────────

int assemble_string(Machine *m, const char *source) {
    // split source into lines
    char  *copy   = strdup(source);
    char  *lines_buf[1024];
    int    count  = 0;
    char  *line   = strtok(copy, "\n");
    while (line && count < 1024) {
        lines_buf[count++] = line;
        line = strtok(NULL, "\n");
    }
    int result = assemble_lines(m, lines_buf, count);
    free(copy);
    return result;
}

int assemble_file(Machine *m, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return -1;
    }

    // read all lines into memory first
    char  *lines_buf[1024];
    int    count = 0;
    char   buf[256];
    while (fgets(buf, sizeof(buf), f) && count < 1024) {
        lines_buf[count++] = strdup(buf);
    }
    fclose(f);

    int result = assemble_lines(m, lines_buf, count);

    for (int i = 0; i < count; i++) free(lines_buf[i]);
    return result;
}