; E820SIM.SYS - mapa de memoria de pega para tests/run_xmshueco_tests.sh.
; Contesta a INT 15h E820h con un mapa como el del MacBook Air 2012: la BIOS
; reserva 2 MB en 512 MB (y 4 KB en 768 MB), con las entradas desordenadas,
; una repetida dentro de otra y un trozo por encima de 4 GB. E801h dice que
; hay 1 GB seguido (lo que creía HSPXMS antes de mirar el mapa).
; Para QEMU con -m 1G (la memoria «reservada» existe de verdad: la prueba
; solo mira que ningún bloque XMS caiga en los huecos).
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
        cpu 386
        bits 16
        org 0
cab:
        dd 0xFFFFFFFF
        dw 0x8000
        dw estrategia
        dw interrupcion
        db "E820SIM$"
peticion dd 0
old15    dd 0

; base (dq), largo (dq), tipo (dd)
%macro ENT 3
        dq %1, %2
        dd %3
%endmacro
mapa:
        ENT 0x00000000, 0x0009FC00, 1
        ENT 0x0009FC00, 0x00000400, 2
        ENT 0x20200000, 0x0FE04000, 1       ; 514 MB - 768 MB + 16 KB
        ENT 0x000F0000, 0x00010000, 2
        ENT 0x00100000, 0x1FF00000, 1       ; 1 MB - 512 MB
        ENT 0x20000000, 0x00200000, 2       ; el hueco del Mac: 512 MB, 2 MB
        ENT 0x01000000, 0x00100000, 1       ; repetida (dentro de la segunda)
        ENT 0x30004000, 0x00001000, 2       ; 4 KB reservados
        ENT 0x30005000, 0x0FFDB000, 1       ; hasta 1 GB - 128 KB
        ENT 0x3FFE0000, 0x00020000, 2
        ENT 0xFFFC0000, 0x00040000, 2
        ENT 0x100000000, 0x10000000, 1      ; por encima de 4 GB: no cuenta
NMAPA   equ ($ - mapa) / 20

int15:
        cmp eax, 0xE820
        jne .e801
        cmp edx, 0x534D4150
        jne .sigue
        cmp ebx, NMAPA
        jae .mal
        push ds
        push si
        push di
        push cx
        push cs
        pop ds
        imul si, bx, 20
        add si, mapa
        mov cx, 20
        cld
        rep movsb
        pop cx
        pop di
        pop si
        pop ds
        inc ebx
        cmp ebx, NMAPA
        jb .no0
        xor ebx, ebx
.no0:
        mov eax, 0x534D4150
        mov ecx, 20
.bien:
        push bp
        mov bp, sp
        and byte [bp+6], 0xFE
        pop bp
        iret
.mal:
        push bp
        mov bp, sp
        or byte [bp+6], 1
        pop bp
        mov ah, 0x86
        iret
.e801:
        cmp ax, 0xE801
        jne .sigue
        mov ax, 0x3C00              ; 1-16 MB
        mov bx, 0x3EFE              ; bloques de 64 KB hasta 1 GB - 128 KB
        mov cx, ax
        mov dx, bx
        jmp .bien
.sigue:
        jmp far [cs:old15]

estrategia:
        mov [cs:peticion], bx
        mov [cs:peticion+2], es
        retf

interrupcion:
        pushad
        push ds
        push es
        push cs
        pop ds
        les bx, [peticion]
        cmp byte [es:bx+2], 0
        je init
        mov word [es:bx+3], 0x0100
fin:
        pop es
        pop ds
        popad
        retf
fin_res:

init:
        push es
        mov ax, 0x3515
        int 0x21
        mov [old15], bx
        mov [old15+2], es
        pop es
        mov dx, int15
        mov ax, 0x2515
        int 0x21
        mov dx, msg
        mov ah, 9
        int 0x21
        les bx, [peticion]
        mov word [es:bx+0x0E], fin_res
        mov [es:bx+0x10], cs
        mov word [es:bx+3], 0x0100
        jmp fin

msg     db "E820SIM: mapa de memoria de pega con huecos en 512 MB y 768 MB", 13, 10, "$"
