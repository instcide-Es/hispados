; PSPSH.COM - prueba de LOADFIX (tests/run_velocidad_tests.sh). Hace de
; intérprete (SHELL=A:\PSPSH.COM en CONFIG.SYS) para que los programas se
; carguen en los primeros 64 KB, cosa que con COMMAND.COM (que ocupa más) no
; pasa: ejecuta PSPTEST directamente («PSP=0xxx»), después con LOADFIX
; («PSP=1xxx»), dice el código de retorno que le llega y se para.
        cpu 386
        bits 16
        org 0x100
start:
        mov sp, pila
        mov bx, (fin - start + 0x100 + 15) / 16
        mov ah, 0x4A
        int 0x21
        mov si, s_ini
        call puts
        mov dx, p_psptest
        mov bx, cola1
        call ejecuta
        mov dx, p_loadfix
        mov bx, cola2
        call ejecuta
        mov si, s_ret
        call puts
        mov ah, 0x4D
        int 0x21
        xor ah, ah
        call puthex4
        call crlf
        ; otra vez sin LOADFIX: el relleno se ha liberado
        mov dx, p_psptest
        mov bx, cola1
        call ejecuta
        mov si, s_fin
        call puts
.para:
        hlt
        jmp .para

; DS:DX = programa, DS:BX = cola de órdenes
ejecuta:
        mov [pblk+2], bx
        mov [pblk+4], ds
        mov word [pblk], 0
        mov word [pblk+6], fcb
        mov [pblk+8], ds
        mov word [pblk+10], fcb
        mov [pblk+12], ds
        mov [g_sp], sp
        mov bx, pblk
        mov ax, 0x4B00
        int 0x21
        cli
        mov ax, cs
        mov ss, ax
        mov sp, [cs:g_sp]
        sti
        mov ds, ax
        mov es, ax
        cld
        jnc .r
        mov si, s_err
        call puts
.r:
        ret

%include "util.inc"

s_ini       db "PSPSH: sin LOADFIX y con LOADFIX", 13, 10, 0
s_ret       db "RET=", 0
s_err       db "PSPSH: no se ha podido ejecutar", 13, 10, 0
s_fin       db "PSPSH: fin", 13, 10, 0
p_psptest   db "A:\PSPTEST.COM", 0
p_loadfix   db "A:\HISPADOS\LOADFIX.COM", 0
cola1       db 8, " directo", 13
cola2       db 27, " A:\PSPTEST.COM con relleno", 13
g_sp        dw 0
pblk        times 14 db 0
fcb         times 40 db 0
u_lbuf      times 84 db 0
u_numbuf    times 16 db 0
            times 256 db 0
pila:
fin:
