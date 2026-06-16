; encrypt_verify.asm
; Demonstrates the full security workflow:
;   1. Write a message to memory
;   2. Hash it for integrity verification
;   3. Encrypt it
;   4. Decrypt it
;   5. Hash the decrypted result
;   6. VERIFY the two hashes match
;   7. SECURE_ERASE the message
;
; Memory layout:
;   0x0200 — message (4 bytes)
;   0x0300 — hash before encryption
;   0x0304 — hash after decryption
;
; R12 = crypto key (set at start)

        ADDI R12, R12, 0x1234   ; set key

        ; write message to 0x0200
        ADDI R0, R0, 0x0200     ; R0 = message address
        ADDI R1, R1, 0xBEEF     ; message value
        STORE R1, R0, 0         ; memory[0x0200] = 0xBEEF

        ; hash original message → store at 0x0300
        HASH R4, R0, 4          ; R4 = hash(memory[0x0200..0x0203])
        ADDI R5, R5, 0x0300
        STORE R4, R5, 0         ; memory[0x0300] = original hash

        ; encrypt the message
        ENCRYPT R0, 4           ; encrypt memory[0x0200..0x0203]

        ; decrypt it back
        DECRYPT R0, 4           ; decrypt memory[0x0200..0x0203]

        ; hash decrypted message → store at 0x0304
        HASH R4, R0, 4          ; R4 = hash(memory[0x0200..0x0203])
        ADDI R5, R5, 4          ; R5 = 0x0304
        STORE R4, R5, 0         ; memory[0x0304] = decrypted hash

        ; verify both hashes match
        ADDI R8, R8, 0x0300     ; R8 = address of original hash
        ADDI R9, R9, 0x0304     ; R9 = address of decrypted hash
        VERIFY R8, R9, 4        ; compare 4 bytes — sets ZERO flag if equal

        ; secure erase the message
        SERASE R0, 4

        HALT