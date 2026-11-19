; =============================================================================
;  Hispa-DOS UNIDADES.COM - qué es cada letra de unidad
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  UNIDADES     enseña, letra por letra, qué es cada unidad: disquete, el
;               disco duro interno (la C:, aunque esté reservada porque aún
;               no tiene partición), otras particiones, el CD/DVD, las
;               memorias USB y de cuál se ha arrancado. Y qué reparto de
;               letras está puesto (LETRAS= de CONFIG.SYS).
;
;  Se lo pregunta al núcleo: INT 21h AX=3368h y AX=3369h, propias de
;  Hispa-DOS (docs/UNIDADES.md).
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        cld
        mov si, msg_title
        call puts
        xor bx, bx
        xor cx, cx
        mov ax, 0x3368
        int 0x21
        cmp bx, 'HD'
        je .ok
        mov si, msg_viejo
        call puts
        mov ax, 0x4C01
        int 0x21
.ok:
        mov [k_flags], cl
        mov byte [drv], 0
.l:
        mov dl, [drv]
        xor bx, bx
        mov ax, 0x3369
        int 0x21
        test al, al
        jz .n
        mov [u_tipo], al
        mov [u_band], ah
        mov [u_disco], cl
        mov [u_part], ch
        mov [u_mb], dx
        call linea
.n:
        inc byte [drv]
        cmp byte [drv], 26
        jb .l
        call crlf
        mov si, msg_normal
        test byte [k_flags], 2
        jnz .p
        mov si, msg_clasico
.p:
        call puts
        mov ax, 0x4C00
        int 0x21

; una línea: «  C:  Disco duro interno (disco 2), FAT16, 63 MB»
linea:
        mov si, msg_sp
        call puts
        mov al, [drv]
        add al, 'A'
        call putc
        mov si, msg_dp
        call puts
        mov al, [u_tipo]
        cmp al, 1
        jne .n1
        mov si, msg_disq
        jmp .fin
.n1:
        cmp al, 3
        jne .n3
        mov si, msg_cd
        jmp .fin
.n3:
        cmp al, 4
        jne .n4
        mov si, msg_usbdrv
        jmp .tam
.n4:
        cmp al, 5
        jne .n5
        mov si, msg_drv
        jmp .tam
.n5:
        ; un disco de la BIOS: memoria USB, el interno u otro
        mov si, msg_usb
        test byte [u_band], 4
        jnz .d
        mov si, msg_int
        test byte [u_band], 2
        jnz .d
        mov si, msg_otro
.d:
        call puts
        mov si, msg_disco
        call puts
        movzx eax, byte [u_disco]
        sub al, 0x7F
        call putnum
        mov al, ')'
        call putc
        cmp byte [u_tipo], 6
        jne .part
        mov si, msg_reserva
        jmp .fin
.part:
        mov si, msg_fat12
        mov al, [u_part]
        cmp al, 0x01
        je .t
        mov si, msg_fat32
        cmp al, 0x0B
        je .t
        cmp al, 0x0C
        je .t
        mov si, msg_fat16
.t:
.tam:
        call puts
        mov si, msg_coma
        call puts
        movzx eax, word [u_mb]
        call putnum
        mov si, msg_mb
.fin:
        call puts
        test byte [u_band], 1
        jz .r
        mov si, msg_arr
        call puts
.r:
        jmp crlf

%include "util.inc"

msg_title   db "Unidades de Hispa-DOS", 13, 10, 0
msg_viejo   db "Este núcleo no sabe decir qué es cada unidad.", 13, 10, 0
msg_sp      db "  ", 0
msg_dp      db ":  ", 0
msg_disq    db "Disquete", 0
msg_cd      db "Lector de CD/DVD", 0
msg_usbdrv  db "Memoria USB (HSPUSB)", 0
msg_drv     db "Unidad de un controlador (disco RAM...)", 0
msg_usb     db "Memoria USB", 0
msg_int     db "Disco duro interno", 0
msg_otro    db "Disco duro", 0
msg_disco   db " (disco ", 0
msg_reserva db ": RESERVADA, aún sin partición de DOS (PARTIR o INSTALAR)", 0
msg_fat12   db ", FAT12", 0
msg_fat16   db ", FAT16", 0
msg_fat32   db ", FAT32", 0
msg_coma    db ", ", 0
msg_mb      db " MB", 0
msg_arr     db "  <- de aquí ha arrancado Hispa-DOS", 0
msg_normal  db "Reparto de letras: el normal (C: el disco duro interno; el CD/DVD, la", 13, 10
            db "primera libre detrás de las particiones; las memorias USB, de la X: hacia", 13, 10
            db "abajo). Se cambia en CONFIG.SYS: LETRAS=CD:R,USB:U o LETRAS=CLASICO.", 13, 10, 0
msg_clasico db "Reparto de letras: el clásico de MS-DOS (LETRAS=CLASICO en CONFIG.SYS):", 13, 10
            db "todas seguidas desde la C:, en el orden de la BIOS.", 13, 10, 0

k_flags     db 0
drv         db 0
u_tipo      db 0
u_band      db 0
u_disco     db 0
u_part      db 0
u_mb        dw 0
u_numbuf    times 16 db 0
u_lbuf      db 16, 0
            times 18 db 0
