; =============================================================================
;  PISA.COM - programa de prueba de Hispa-8086: pisa la memoria de lo alto
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Llena de CCh toda su memoria a partir de 64 K por detrás de él, hasta el
;  final (PSP:2): ahí estaba la parte transitoria del intérprete, que al
;  volver tiene que leerse otra vez de COMMAND.COM.
; =============================================================================
        cpu 8086
        org 0x100
        mov bx, cs
        add bx, 0x1000
        mov dx, [2]
.s:
        cmp bx, dx
        jae .x
        mov es, bx
        xor di, di
        mov cx, 8
        mov ax, 0xCCCC
        rep stosw
        inc bx
        jmp .s
.x:
        mov dx, m_pisa
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
m_pisa      db "PISA: memoria pisada", 13, 10, "$"
