; functions.asm
; Demonstrates CALL, RET, PUSH, POP — the function call convention
;
; Calls a function that doubles a value:
;   input:  R0 = value to double
;   output: R0 = value * 2
;
; Calls a function that squares a value:
;   input:  R0 = value to square
;   output: R0 = value * value
;
; Stack is used to save/restore registers across calls

        ADDI R0, R0, 6      ; R0 = 6
        CALL double         ; R0 = 12
        CALL double         ; R0 = 24
        CALL square         ; R0 = 576
        HALT

; ── double(R0) ────────────────────────────────────────────────────
; R0 = R0 * 2
; uses ADD — no registers clobbered beyond R0
double:
        ADD  R0, R0, R0     ; R0 = R0 + R0
        RET

; ── square(R0) ────────────────────────────────────────────────────
; R0 = R0 * R0
; saves R1 on stack since it uses it as a temp
square:
        PUSH R1             ; save R1
        ADD  R1, R0, R6     ; R1 = R0  (copy input, R6 is zero)
        MUL  R0, R1, R1     ; R0 = R1 * R1
        POP  R1             ; restore R1
        RET