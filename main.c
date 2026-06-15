#include <stdio.h> //uses the input/output library
#include <stdint.h> //allows for setting integer type widths

int main() {
    uint32_t instruction = 0x12345678;
    uint8_t opcode = (instruction >> 24) & 0xFF;
    uint8_t rd = (instruction >> 20) & 0xF;
    uint8_t ra = (instruction >> 16) & 0xF;
    uint16_t imm = instruction & 0xFFFF;

    printf("instruction: 0x%08X\n", instruction); 
    printf("opcode:      0x%02X\n", opcode); 
    printf("ra:          %d\n", ra);
    printf("imm:         0x%04X\n", imm);

    return 0;
}