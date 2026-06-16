; fibonacci.asm
; Computes the first 8 Fibonacci numbers and stores them in memory
; starting at address 0x0200
;
; F(0)=0, F(1)=1, F(2)=1, F(3)=2, F(4)=3, F(5)=5, F(6)=8, F(7)=13
;
; Registers:
;   R0 = F(n-2)  previous previous value
;   R1 = F(n-1)  previous value
;   R2 = F(n)    current value
;   R3 = memory write pointer
;   R4 = counter
;   R5 = limit (8)

        ADDI R3, R3, 0x0200     ; R3 = base address
        ADDI R5, R5, 8          ; R5 = limit (8 numbers)

        ; store F(0) = 0
        STORE R0, R3, 0
        ADDI R3, R3, 4

        ; store F(1) = 1
        ADDI R1, R1, 1
        STORE R1, R3, 0
        ADDI R3, R3, 4

        ; R4 = 2 (we've stored 2 values already)
        ADDI R4, R4, 2

loop:
        ADD  R2, R0, R1         ; R2 = F(n-2) + F(n-1)
        STORE R2, R3, 0         ; store to memory
        ADDI R3, R3, 4          ; advance pointer

        ; shift: R0 = R1, R1 = R2
        ADD  R0, R1, R6         ; R0 = R1 (R6 is zero)
        ADD  R1, R2, R6         ; R1 = R2

        ; increment counter
        ADDI R4, R4, 1

        ; check if done
        SUB  R7, R4, R5         ; R7 = counter - limit
        BEQ  R6, done           ; if zero, done
        JMP  loop

done:
        HALT