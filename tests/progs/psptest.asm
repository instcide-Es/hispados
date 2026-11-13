; PSPTEST.COM - prueba de LOADFIX (tests/run_velocidad_tests.sh): escribe el
; segmento de su PSP y sus argumentos, «PSP=xxxx ARGS=[...]», y acaba con el
; código de retorno 5.
        cpu 386
        bits 16
        org 0x100
start:
        mov si, s_psp
        call puts
        mov ax, cs
        call puthex4
        mov si, s_args
        call puts
        mov si, 0x81
.a:
        lodsb
        cmp al, 13
        je .f
        call putc
        jmp .a
.f:
        mov al, ']'
        call putc
        call crlf
        mov ax, 0x4C05
        int 0x21

%include "util.inc"

s_psp       db "PSP=", 0
s_args      db " ARGS=[", 0
u_lbuf      times 84 db 0
u_numbuf    times 16 db 0
