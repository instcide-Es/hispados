; =============================================================================
;  DISCO81.COM - prueba: un disco duro de mentira sin medio (residente)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Engancha INT 13h y contesta por un disco 80h que existe (tiene geometría y
;  AH=15h dice «disco duro») pero que no se puede leer: error 04h, «sector no
;  encontrado». Es lo que hace la BIOS de un MacBook Air con su lector de
;  tarjetas vacío. Sirve para probar que el núcleo, al volver a mirar los
;  discos (INT 21h AH=0Dh), no saca el aviso de error crítico por él.
;  Lo usa tests/run_criterr_tests.sh, arrancando de disquete (sin disco duro).
; =============================================================================
        cpu 386
        org 0x100
        jmp init
old13   dd 0
int13:
        cmp dl, 0x80
        je .mio
        jmp far [cs:old13]
.mio:
        push bp
        mov bp, sp
        cmp ah, 0x08
        je .geo
        cmp ah, 0x15
        je .tipo
        test ah, ah
        jz .bien
        mov ah, 0x04                ; todo lo demás: sector no encontrado
        or byte [bp+6], 1
        pop bp
        iret
.geo:
        mov cx, 0x3F3F              ; 64 cilindros, 63 sectores
        mov dx, 0x0F01              ; 16 cabezas, un disco
        xor ax, ax
.bien:
        xor ah, ah
        and byte [bp+6], 0xFE
        pop bp
        iret
.tipo:
        mov ah, 3
        mov cx, 0
        mov dx, 0xFC00
        and byte [bp+6], 0xFE
        pop bp
        iret
fin_res:
init:
        mov ax, 0x3513
        int 0x21
        mov [old13], bx
        mov [old13+2], es
        mov dx, int13
        mov ax, 0x2513
        int 0x21
        mov dx, m
        mov ah, 9
        int 0x21
        mov dx, fin_res + 15
        shr dx, 4
        mov ax, 0x3100
        int 0x21
m       db "disco-de-mentira-puesto", 13, 10, "$"
