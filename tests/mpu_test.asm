; mpu_test.asm
; Tests the MPU by attempting to access secure memory from user mode
;
; Memory layout enforced by MPU:
;   0x0000-0x1FFF  code region    execute only   user+secure
;   0x2000-0xDFFF  data region    read+write     user+secure
;   0xE000-0xFEFF  secure region  read+write     secure only
;
; This program:
;   1. writes a value to normal data memory (should succeed)
;   2. reads it back (should succeed)
;   3. attempts to read secure memory (should fault)

        ADDI R0, R0, 0x2000     ; R0 = data region address
        ADDI R1, R1, 0xBEEF     ; R1 = test value
        STORE R1, R0, 0         ; write to data region — permitted
        LOAD  R2, R0, 0         ; read back — permitted

        ; now attempt to access secure memory from user mode
        ADDI R3, R3, 0xE000     ; R3 = secure region address
        LOAD  R4, R3, 0         ; READ secure memory — should fault here

        ; this should never execute
        HALT