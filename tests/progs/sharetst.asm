; SHARETST.COM - prueba del SHARE del núcleo: INT 2Fh 1000h, modos de
; compartición al abrir y bloqueos de zonas (INT 21h 5Ch)
        cpu 386
        bits 16
        org 0x100

        ; 1) ¿SHARE instalado?
        mov ax, 0x1000
        int 0x2F
        mov dx, m_noshare
        cmp al, 0xFF
        jne .p1
        mov dx, m_share
.p1:
        call say
        ; 2) crear T.DAT con 100 octetos
        mov dx, fname
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc fail
        mov bx, ax
        mov dx, buf
        mov cx, 100
        mov ah, 0x40
        int 0x21
        mov ah, 0x3E
        int 0x21
        ; 3) abrir sin dejar a nadie (12h) y otra vez para leer (40h): violación
        mov ax, 0x3D12
        call op
        jc fail
        mov [h1], ax
        mov ax, 0x3D40
        call op
        mov dx, m_sh_ok
        jnc .p3
        cmp ax, 0x20
        jne .p3
        mov dx, m_sh_den
.p3:
        call say
        ; 4) cerrar la primera: ahora sí se puede
        mov bx, [h1]
        mov ah, 0x3E
        int 0x21
        mov ax, 0x3D40
        call op
        mov dx, m_reopen_bad
        jc .p4
        mov bx, ax
        mov ah, 0x3E
        int 0x21
        mov dx, m_reopen_ok
.p4:
        call say
        ; 5) modo compatibilidad dos veces: se permite
        mov ax, 0x3D02
        call op
        jc fail
        mov [h1], ax
        mov ax, 0x3D02
        call op
        mov dx, m_compat_bad
        jc .p5
        mov bx, ax
        mov ah, 0x3E
        int 0x21
        mov dx, m_compat_ok
.p5:
        call say
        mov bx, [h1]
        mov ah, 0x3E
        int 0x21
        ; 6) bloqueos: h2 bloquea 0-9, h3 pide 5-14 (choque), h2 desbloquea, h3 sí
        mov ax, 0x3D42
        call op
        jc fail
        mov [h2], ax
        mov ax, 0x3D42
        call op
        jc fail
        mov [h3], ax
        mov bx, [h2]
        xor dx, dx
        mov di, 10
        mov al, 0
        call lk
        mov dx, m_lk1_bad
        jc .p6
        mov dx, m_lk1_ok
.p6:
        call say
        mov bx, [h3]
        mov dx, 5
        mov di, 10
        mov al, 0
        call lk
        mov dx, m_lk2_bad
        jnc .p7
        cmp ax, 0x21
        jne .p7
        mov dx, m_lk2_ok
.p7:
        call say
        mov bx, [h2]
        xor dx, dx
        mov di, 10
        mov al, 1
        call lk
        mov bx, [h3]
        mov dx, 5
        mov di, 10
        mov al, 0
        call lk
        mov dx, m_lk3_bad
        jc .p8
        mov dx, m_lk3_ok
.p8:
        call say
        ; cerrar (libera los bloqueos) y borrar
        mov bx, [h2]
        mov ah, 0x3E
        int 0x21
        mov bx, [h3]
        mov ah, 0x3E
        int 0x21
        mov dx, fname
        mov ah, 0x41
        int 0x21
        mov dx, m_end
        call say
        mov ax, 0x4C00
        int 0x21
fail:
        mov dx, m_fail
        call say
        mov ax, 0x4C01
        int 0x21

; AX = 3Dxxh -> abrir T.DAT
op:
        mov dx, fname
        int 0x21
        ret
; AL = 0/1, BX = handle, DX = inicio, DI = longitud
lk:
        mov ah, 0x5C
        xor cx, cx
        xor si, si
        int 0x21
        ret
say:
        mov ah, 9
        int 0x21
        ret

h1      dw 0
h2      dw 0
h3      dw 0
fname   db "T.DAT", 0
m_share      db "SHARE: instalado", 13, 10, "$"
m_noshare    db "SHARE: no instalado", 13, 10, "$"
m_sh_den     db "Compartir: denegado (20h)", 13, 10, "$"
m_sh_ok      db "Compartir: NO denegado", 13, 10, "$"
m_reopen_ok  db "Tras cerrar: se abre", 13, 10, "$"
m_reopen_bad db "Tras cerrar: NO se abre", 13, 10, "$"
m_compat_ok  db "Compatibilidad: dos aperturas", 13, 10, "$"
m_compat_bad db "Compatibilidad: denegada", 13, 10, "$"
m_lk1_ok     db "Bloqueo 1: concedido", 13, 10, "$"
m_lk1_bad    db "Bloqueo 1: denegado", 13, 10, "$"
m_lk2_ok     db "Bloqueo 2: choque (21h)", 13, 10, "$"
m_lk2_bad    db "Bloqueo 2: sin choque", 13, 10, "$"
m_lk3_ok     db "Bloqueo 3: concedido tras desbloquear", 13, 10, "$"
m_lk3_bad    db "Bloqueo 3: denegado", 13, 10, "$"
m_end        db "Fin de la prueba de SHARE", 13, 10, "$"
m_fail       db "Error inesperado", 13, 10, "$"
buf     times 100 db 'x'
