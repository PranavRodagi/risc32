; count.asm — now with labels
; Counts R0 from 0 to 5

        ADDI R1, R1, 5     ; R1 = 5 (limit)
        ADDI R2, R2, 1     ; R2 = 1 (increment)

loop:
        ADD  R0, R0, R2    ; R0 = R0 + 1
        SUB  R3, R0, R1    ; R3 = R0 - R1
        BEQ  R5, halt      ; if equal jump to halt
        JMP  loop          ; else loop back

halt:
        HALT