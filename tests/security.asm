; security.asm
; Demonstrates the security instruction set
;
; Memory layout:
;   0x0200 — plaintext message (8 bytes)
;   0x0300 — will hold hash of plaintext
;   0x0400 — encrypted copy
;
; R12 (KEY register) is set directly before entering secure mode

        ; set up key in R12
        ADDI R12, R12, 0xABCD

        ; write a test message into memory at 0x0200
        ; "RISC32!!" as 4-byte words
        ADDI R0, R0, 0x0200     ; R0 = base address
        ADDI R1, R1, 0x5249     ; "RI"
        STORE R1, R0, 0         ; memory[0x0200] = 0x5249
        ADDI R1, R1, 0x1001     ; change R1
        ADDI R1, R0, 0          ; restore base

        ; hash the message
        HASH R4, R0, 8          ; R4 = hash(memory[0x0200..0x0207])

        ; store hash at 0x0300
        ADDI R5, R5, 0x0300
        STORE R4, R5, 0

        ; generate a random number
        RANDOM R6               ; R6 = random value

        ; secure erase the message
        SERASE R0, 8            ; zero memory[0x0200..0x0207]

        HALT