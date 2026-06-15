; count.asm
ADDI R1, R1, 5     ; R1 = 5
ADDI R2, R2, 1     ; R2 = 1

; 0x0008 — loop start
ADD  R0, R0, R2    ; R0 = R0 + 1
SUB  R3, R0, R1    ; R3 = R0 - R1, sets ZERO flag when equal
BEQ  R5, 0x0018    ; if zero: jump to HALT at 0x0018
JMP  0x0008        ; else loop back

; 0x0018 — HALT
HALT