; =============================================================================
;  Hispa-64 - ACPICA64.BIN, ACPI en el núcleo: la cabecera y las entradas
;  (acpi/hispa64/hispa64.h). Copyright (C) 2026 Carlos Sanz y contribuidores.
;  GPL-2.0 o posterior.
;
;  La cabecera (al principio, en 0xFFFFFFFFC0000000), para el núcleo
;  (castel/nucleo64/acpica64.cas): la firma, la versión, hasta dónde llega el
;  código (lo demás, datos), hasta dónde llega el archivo, hasta dónde lo que
;  empieza a cero, y la entrada.
;  La entrada: la llama el núcleo con la convención de C (f, a, b en rdi,
;  rsi, rdx; lo que devuelve en rax), con su cerrojo tomado; aquí se guardan
;  los registros que el código de Castel pueda usar y se alinea la pila.
;  castel0..castel5: llamar desde C a una función del núcleo de Castel, con
;  sus argumentos por la pila (el primero arriba; ella los quita al volver).
; =============================================================================
        bits 64
        default rel
        extern nucleo64_llamada, _ini_datos, _fin_imagen, _fin_bss
        global _inicio, castel0, castel1, castel2, castel4, castel5

        section .entrada progbits alloc exec
_inicio:
        db "HACPI64", 0             ; la firma
        dd 1                        ; la versión (H64_VERSION)
        dd 0
        dq _ini_datos               ; el código, hasta aquí (de solo leer)
        dq _fin_imagen              ; el archivo, hasta aquí
        dq _fin_bss                 ; lo que empieza a cero, hasta aquí
        dq entrada                  ; la entrada

entrada:
        push rbx
        push rbp
        push r12
        push r13
        push r14
        push r15
        mov rbp, rsp
        and rsp, -16
        cld
        call nucleo64_llamada
        mov rsp, rbp
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbp
        pop rbx
        ret

; llamar a una función de Castel: rdi la función y rsi, rdx, rcx, r8, r9 sus
; argumentos; se guardan los registros que C quiere intactos (Castel los
; puede usar) y la pila vuelve a donde estaba aunque la función quite de más
%macro GUARDA 0
        push rbx
        push rbp
        push r12
        push r13
        push r14
        push r15
        mov rbp, rsp
%endmacro
%macro VUELVE 0
        mov rsp, rbp
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbp
        pop rbx
        ret
%endmacro
castel0:
        GUARDA
        call rdi
        VUELVE
castel1:
        GUARDA
        push rsi
        call rdi
        VUELVE
castel2:
        GUARDA
        push rdx
        push rsi
        call rdi
        VUELVE
castel4:
        GUARDA
        push r8
        push rcx
        push rdx
        push rsi
        call rdi
        VUELVE
castel5:
        GUARDA
        push r9
        push r8
        push rcx
        push rdx
        push rsi
        call rdi
        VUELVE
