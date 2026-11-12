; =============================================================================
;  Hispa-DOS KEYB.COM - distribución de teclado española (residente)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  KEYB SP | KEYB ES   teclado español (ñ, ç, ¡, ¿, º, ª, acentos con teclas
;                      muertas ´ ` ^ ¨, AltGr: @ # | \ [ ] { } ~ ¬)
;  KEYB US             teclado estadounidense (el de la BIOS)
;  KEYB                muestra el teclado activo
;
;  Se engancha a INT 09h: traduce las teclas que cambian y pone el carácter
;  (página de códigos 437) en el búfer de la BIOS con INT 16h AH=05h.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

D_ACUTE     equ 0xF1
D_GRAVE     equ 0xF2
D_CIRC      equ 0xF3
D_DIAER     equ 0xF4

start:
        jmp init

old09       dd 0
old2f       dd 0
active      db 1
dead        db 0
e0          db 0                    ; último código fue E0h
altgr       db 0                    ; Alt derecho pulsado

; código de tecla, normal, Mayús, AltGr (0 = lo que dé la BIOS)
keytab:
        db 0x29, 0xA7, 0xA6, '\'        ; º ª \
        db 0x02, 0,    0,    '|'        ; 1 ! |
        db 0x03, 0,    '"',  '@'        ; 2 " @
        db 0x04, 0,    0xFA, '#'        ; 3 · #
        db 0x05, 0,    0,    '~'        ; 4 $ ~
        db 0x07, 0,    '&',  0xAA       ; 6 & ¬
        db 0x08, 0,    '/',  0          ; 7 /
        db 0x09, 0,    '(',  0          ; 8 (
        db 0x0A, 0,    ')',  0          ; 9 )
        db 0x0B, 0,    '=',  0          ; 0 =
        db 0x0C, 0x27, '?',  0          ; ' ?
        db 0x0D, 0xAD, 0xA8, 0          ; ¡ ¿
        db 0x1A, D_GRAVE, D_CIRC, '['   ; ` ^ [
        db 0x1B, '+',  '*',  ']'        ; + * ]
        db 0x27, 0xA4, 0xA5, 0          ; ñ Ñ
        db 0x28, D_ACUTE, D_DIAER, '{'  ; ´ ¨ {
        db 0x2B, 0x87, 0x80, '}'        ; ç Ç }
        db 0x56, '<',  '>',  0          ; < >
        db 0x33, ',',  ';',  0
        db 0x34, '.',  ':',  0
        db 0x35, '-',  '_',  0
        db 0

; vocales: código de tecla y, por acento, minúscula y mayúscula
;              a     e     i     o     u
vowel_scan  db 0x1E, 0x12, 0x17, 0x18, 0x16
acute_lo    db 0xA0, 0x82, 0xA1, 0xA2, 0xA3
acute_up    db 'A',  0x90, 'I',  'O',  'U'
grave_lo    db 0x85, 0x8A, 0x8D, 0x95, 0x97
grave_up    db 'A',  'E',  'I',  'O',  'U'
circ_lo     db 0x83, 0x88, 0x8C, 0x93, 0x96
circ_up     db 'A',  'E',  'I',  'O',  'U'
diaer_lo    db 0x84, 0x89, 0x8B, 0x94, 0x81
diaer_up    db 0x8E, 'E',  'I',  0x99, 0x9A
accent_ch   db 0x27, '`', '^', '"'

int2f:
        cmp ax, 0xAD80
        jne .chain
        mov al, 0xFF
        mov bx, 'HS'
        push cs
        pop es
        mov di, active
        iret
.chain:
        jmp far [cs:old2f]

int09:
        push ax
        push bx
        push cx
        push si
        push ds
        cmp byte [cs:active], 0
        je .chain
        in al, 0x60
        mov ah, al
        ; seguir AltGr (E0 38 / E0 B8) por nuestra cuenta
        cmp al, 0xE0
        jne .ne0
        mov byte [cs:e0], 1
        jmp .chain
.ne0:
        cmp byte [cs:e0], 0
        je .noe0
        mov byte [cs:e0], 0
        mov bl, al
        and bl, 0x7F
        cmp bl, 0x38
        jne .chain
        xor bl, bl
        test al, 0x80
        jnz .ag
        mov bl, 0x08
.ag:
        mov [cs:altgr], bl
        jmp .chain
.noe0:
        test al, 0x80               ; soltar tecla
        jnz .chain
        mov bx, 0x40
        mov ds, bx
        mov bl, [0x17]              ; Mayús, Ctrl, Alt, Bloq Mayús
        mov bh, [cs:altgr]
        test bl, 0x04               ; Ctrl sin AltGr: lo que diga la BIOS
        jz .noctrl
        test bh, bh
        jz .chain
.noctrl:
        test bl, 0x08               ; Alt izquierdo
        jz .noalt
        test bh, bh
        jz .chain
.noalt:
        ; buscar la tecla en la tabla
        mov si, keytab
.find:
        mov al, [cs:si]
        test al, al
        jz .notfound
        cmp al, ah
        je .found
        add si, 4
        jmp .find
.found:
        mov al, [cs:si+3]
        test bh, bh
        jnz .got
        mov al, [cs:si+1]
        mov cl, bl
        and cl, 3
        ; ñ y ç: Bloq Mayús también cuenta
        cmp ah, 0x27
        je .caps
        cmp ah, 0x2B
        jne .shift
.caps:
        test bl, 0x40
        jz .shift
        xor cl, 1
.shift:
        test cl, cl
        jz .got
        mov al, [cs:si+2]
.got:
        test al, al
        jz .notfound
        cmp al, D_ACUTE
        jb .plain
        ; tecla muerta: la siguiente tecla lleva el acento
        call flush_dead
        sub al, 0xF0
        mov [cs:dead], al
        jmp .eat
.plain:
        call flush_dead
        call emit
        jmp .eat
.notfound:
        cmp byte [cs:dead], 0
        je .chain
        ; Mayús, Ctrl, Alt y Bloq Mayús no deshacen la tecla muerta
        ; (´ y luego Mayús+E = É)
        cmp ah, 0x2A
        je .chain
        cmp ah, 0x36
        je .chain
        cmp ah, 0x1D
        je .chain
        cmp ah, 0x38
        je .chain
        cmp ah, 0x3A
        je .chain
        ; ¿vocal o espacio tras una tecla muerta?
        cmp ah, 0x39
        jne .vow
        call flush_dead
        jmp .eat
.vow:
        test bh, bh
        jnz .other
        mov si, vowel_scan
        xor cx, cx
.vl:
        cmp [cs:si], ah
        je .vowel
        inc si
        inc cx
        cmp cx, 5
        jb .vl
.other:
        call flush_dead
        jmp .chain
.vowel:
        ; mayúscula si Mayús xor Bloq Mayús
        mov al, bl
        and al, 3
        jz .nsh
        mov al, 1
.nsh:
        test bl, 0x40
        jz .ncaps
        xor al, 1
.ncaps:
        movzx ax, al
        movzx si, byte [cs:dead]
        dec si
        shl si, 1
        add si, ax                  ; (acento-1)*2 + mayúscula
        shl si, 1                   ; entradas de 2 bytes
        mov si, [cs:vtab+si]        ; tabla de esa forma
        mov bx, cx
        mov al, [cs:si+bx]
        mov byte [cs:dead], 0
        call emit
        jmp .eat
.eat:
        mov al, 0x20                ; fin de interrupción
        out 0x20, al
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        iret
.chain:
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        jmp far [cs:old09]

; AL = carácter, AH = código de tecla -> búfer de la BIOS
emit:
        push ax
        push cx
        mov cl, al
        mov ch, ah
        mov ah, 0x05
        int 0x16
        pop cx
        pop ax
        ret

; si hay una tecla muerta pendiente, escribir su acento
flush_dead:
        cmp byte [cs:dead], 0
        je .r
        push ax
        push bx
        movzx bx, byte [cs:dead]
        mov al, [cs:accent_ch+bx-1]
        mov ah, 0x28
        mov byte [cs:dead], 0
        call emit
        pop bx
        pop ax
.r:
        ret

vtab        dw acute_lo, acute_up, grave_lo, grave_up
            dw circ_lo, circ_up, diaer_lo, diaer_up

resident_end:

; =============================================================================
init:
        ; argumento
        mov si, 0x81
.sk:
        lodsb
        cmp al, ' '
        je .sk
        cmp al, 13
        je .show
        and al, 0xDF
        mov ah, [si]
        and ah, 0xDF
        cmp ax, 'SP'
        je .es
        cmp ax, 'ES'
        je .es
        cmp ax, 'US'
        je .us
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
.show:
        mov ax, 0xAD80
        int 0x2F
        cmp al, 0xFF
        jne .isus
        cmp byte [es:di], 0
        je .isus
        mov si, msg_cur_es
        call puts
        jmp .exit
.isus:
        mov si, msg_cur_us
        call puts
.exit:
        mov ax, 0x4C00
        int 0x21
.us:
        mov ax, 0xAD80
        int 0x2F
        cmp al, 0xFF
        jne .isus
        mov byte [es:di], 0
        jmp .isus
.es:
        mov ax, 0xAD80
        int 0x2F
        cmp al, 0xFF
        jne .install
        mov byte [es:di], 1
        mov si, msg_cur_es
        call puts
        jmp .exit
.install:
        push cs
        pop es
        mov ax, 0x3509
        int 0x21
        mov [old09], bx
        mov [old09+2], es
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov dx, int09
        mov ax, 0x2509
        int 0x21
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        mov si, msg_inst
        call puts
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, resident_end + 15
        shr dx, 4
        mov ax, 0x3100
        int 0x21

puts:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 2
        int 0x21
        jmp puts
.r:
        ret

msg_usage   db "Uso: KEYB [SP|ES|US]", 13, 10, 0
msg_inst    db "Teclado espa", 0xA4, "ol instalado.", 13, 10, 0
msg_cur_es  db "Teclado activo: espa", 0xA4, "ol (SP).", 13, 10, 0
msg_cur_us  db "Teclado activo: estadounidense (US).", 13, 10, 0
