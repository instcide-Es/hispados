; =============================================================================
;  Hispa-DOS KEYB.COM - distribución de teclado española (residente)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  KEYB SP | KEYB ES   teclado español (ñ, ç, ¡, ¿, º, ª, acentos con teclas
;                      muertas ´ ` ^ ¨, AltGr: @ # | \ [ ] { } ~ ¬)
;  KEYB US             teclado estadounidense (el de la BIOS)
;  KEYB                muestra el teclado activo
;  KEYB ... /MAC       teclas que no tienen los portátiles de Apple:
;                        Ctrl+Retroceso = Supr      Alt+Retroceso = Insertar
;                        Alt+izquierda  = Inicio    Alt+derecha   = Fin
;                        Alt+arriba     = RePág     Alt+abajo     = AvPág
;                      (Alt es la tecla Opción). Se activa sola si el equipo
;                      es de Apple (fabricante de la tabla SMBIOS tipo 1).
;  KEYB ... /NOMAC     las quita
;
;  Se engancha al aviso de teclado de la BIOS (INT 15h, AH=4Fh: la BIOS lo
;  llama desde INT 09h con cada código de tecla): traduce las teclas que
;  cambian y pone el carácter (página de códigos 437) en el búfer de la BIOS
;  con INT 16h AH=05h; las demás las deja pasar. No lee el puerto del teclado:
;  si lo leyera antes que la BIOS, las teclas de dos códigos (flechas, Inicio,
;  Fin...) podían llegar repetidas.
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
soft        db 0                    ; la tecla llegó por INT 2Fh AX=AD81h
active      db 1                    ; distribución española activa
mac         db 0                    ; (active+1) teclas de sustitución del Mac
dead        db 0
insdn       db 0                    ; Alt+Retroceso sigue pulsado (no repetir)
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
        cmp ax, 0xAD81
        je .soft
        jmp far [cs:old2f]
; código de tecla que no viene del 8042 (HSPUSB en equipos sin controlador
; de teclado): BL = código -> AX = 4B01h atendida, 4B00h sin cambios
.soft:
        push ax
        push bx
        push cx
        push si
        push ds
        mov byte [cs:soft], 1
        mov al, bl
        jmp int09.have

int09:
        cmp ah, 0x4F                ; ¿aviso de tecla? AL=código
        je .tecla
        jmp far [cs:old09]
.tecla:
        push ax
        push bx
        push cx
        push si
        push ds
.have:
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
        jne .macnav
        xor bl, bl
        test al, 0x80
        jnz .ag
        mov bl, 0x08
.ag:
        mov [cs:altgr], bl
        jmp .chain
; Mac: Alt + flecha gris (E0 4B/4D/48/50) = Inicio / Fin / RePág / AvPág
.macnav:
        cmp byte [cs:mac], 0
        je .chain
        test al, 0x80
        jnz .chain
        mov bx, 0x40
        mov ds, bx
        mov bl, [0x17]
        and bl, 0x0C
        cmp bl, 0x08                ; Alt (cualquiera de los dos) sin Ctrl
        jne .chain
        mov ah, 0x47                ; izquierda -> Inicio
        cmp al, 0x4B
        je .macput
        mov ah, 0x4F                ; derecha -> Fin
        cmp al, 0x4D
        je .macput
        mov ah, 0x49                ; arriba -> RePág
        cmp al, 0x48
        je .macput
        mov ah, 0x51                ; abajo -> AvPág
        cmp al, 0x50
        je .macput
        jmp .chain
; la tecla gris de código AH al búfer (AX = xxE0h: INT 16h AH=10h la da así
; y AH=00h como xx00h, igual que la tecla de verdad); la original no pasa
.macput:
        and byte [0x96], 0xFD       ; la BIOS ya vio el E0h: que lo olvide
        mov al, 0xE0
        call flush_dead
        call emit
        jmp .eat
.noe0:
        cmp byte [cs:mac], 0
        je .nomac
        mov bl, al
        and bl, 0x7F
        cmp bl, 0x0E                ; Retroceso
        jne .nomac
        mov bx, 0x40
        mov ds, bx
        test al, 0x80
        jz .bsdn
        mov byte [cs:insdn], 0
        jmp .chain
.bsdn:
        mov bl, [0x17]
        and bl, 0x0C
        mov ah, 0x53                ; Ctrl+Retroceso = Supr
        cmp bl, 0x04
        je .macput
        cmp bl, 0x08                ; Alt+Retroceso = Insertar
        jne .bsno
        mov ah, 0x52
        cmp byte [cs:insdn], 0      ; el indicador cambia una vez por
        jne .macput                 ; pulsación, no con la repetición
        mov byte [cs:insdn], 1
        xor byte [0x17], 0x80
        jmp .macput
.bsno:
        mov ah, al
.nomac:
        cmp byte [cs:active], 0     ; distribución de EE. UU.: nada más
        je .chain
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
        cmp byte [cs:soft], 0
        jne .seat
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        push bp                     ; acarreo a 0: la BIOS no hace nada más
        mov bp, sp
        and byte [bp+6], 0xFE
        pop bp
        iret
.chain:
        cmp byte [cs:soft], 0
        jne .schain
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        jmp far [cs:old09]
.seat:
        mov byte [cs:soft], 0
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        mov ax, 0x4B01              ; atendida
        iret
.schain:
        mov byte [cs:soft], 0
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        mov ax, 0x4B00              ; sin cambios: la pone quien la envía
        iret

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
        ; argumentos: [SP|ES|US] [/MAC|/NOMAC]
        mov si, 0x81
.tok:
        lodsb
        cmp al, ' '
        je .tok
        cmp al, 9
        je .tok
        cmp al, 13
        je .parsed
        cmp al, '/'
        je .sw
        and al, 0xDF
        mov ah, [si]
        and ah, 0xDF
        inc si
        mov bl, 1
        cmp ax, 'SP'
        je .lay
        cmp ax, 'ES'
        je .lay
        mov bl, 2
        cmp ax, 'US'
        jne .usage
.lay:
        mov [lay], bl
        jmp .delim
.sw:
        mov bl, 1
        mov ax, [si]
        and ax, 0xDFDF
        cmp ax, 'NO'
        jne .sw1
        add si, 2
        mov bl, 2
.sw1:
        mov ax, [si]
        and ax, 0xDFDF
        cmp ax, 'MA'
        jne .usage
        mov al, [si+2]
        and al, 0xDF
        cmp al, 'C'
        jne .usage
        add si, 3
        mov [macopt], bl
.delim:
        mov al, [si]                ; detrás, espacio, otra opción o el final
        cmp al, ' '
        je .tok
        cmp al, 9
        je .tok
        cmp al, 13
        je .tok
        cmp al, '/'
        je .tok
.usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
.parsed:
        mov ax, 0xAD80              ; ¿ya residente? -> ES:DI = active, mac
        int 0x2F
        cmp al, 0xFF
        jne .nores
        ; ---- residente: cambiar lo que se pida y decir cómo queda ----
        mov al, [lay]
        test al, al
        jz .r1
        and al, 1                   ; SP = 1, US = 0
        mov [es:di], al
.r1:
        mov al, [macopt]
        test al, al
        jz .r2
        and al, 1                   ; /MAC = 1, /NOMAC = 0
        mov [es:di+1], al
.r2:
        mov si, msg_cur_us
        cmp byte [es:di], 0
        je .r3
        mov si, msg_cur_es
.r3:
        call puts
        mov si, msg_mac
        cmp byte [es:di+1], 0
        jne .r4
        cmp byte [macopt], 2
        jne .exit
        mov si, msg_nomac
.r4:
        call puts
.exit:
        mov ax, 0x4C00
        int 0x21
        ; ---- sin instalar ----
.nores:
        mov al, [macopt]
        cmp al, 2
        je .nomac
        cmp al, 1
        je .simac
        cmp byte [lay], 0           ; «KEYB» a secas solo informa
        je .nomac
        call detect_mac
        mov [mac], al
        jmp .nomac
.simac:
        mov byte [mac], 1
.nomac:
        mov al, [lay]
        and al, 1
        mov [active], al
        or al, [mac]
        jnz .install
        mov si, msg_cur_us          ; nada que dejar residente
        call puts
        cmp byte [macopt], 2
        jne .exit
        mov si, msg_nomac
        call puts
        jmp .exit
.install:
        push cs
        pop es
        mov ax, 0x3515
        int 0x21
        mov [old09], bx
        mov [old09+2], es
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov dx, int09
        mov ax, 0x2515
        int 0x21
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        mov si, msg_inst
        cmp byte [active], 0
        jne .i1
        mov si, msg_cur_us
.i1:
        call puts
        cmp byte [mac], 0
        je .i2
        mov si, msg_mac
        call puts
.i2:
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

; ---- ¿es un equipo de Apple? ----
; Busca la cabecera de SMBIOS («_SM_», «_SM3_» o la antigua «_DMI_») en
; F0000h-FFFFFh, recorre la tabla hasta la estructura de tipo 1 (información
; del sistema) y mira si el fabricante contiene «Apple». La tabla puede estar
; por encima del primer megabyte: se lee con INT 15h AH=87h.
; -> AL = 1 si lo es
SM_BUF      equ 1024
detect_mac:
        push es
        mov ax, 0xF000
        mov es, ax
        xor di, di
.scan:
        mov eax, [es:di]
        cmp eax, '_SM_'
        je .sm2
        cmp byte [es:di+4], '_'
        jne .next
        cmp eax, '_SM3'
        je .sm3
        cmp eax, '_DMI'
        je .dmi
.next:
        add di, 16
        jnz .scan
        jmp .no
.sm2:
        mov eax, [es:di+0x18]       ; dirección de la tabla
        movzx ecx, word [es:di+0x16] ; y lo que ocupa
        jmp .walk
.sm3:
        cmp dword [es:di+0x14], 0   ; por encima de 4 GB: no se alcanza
        jne .next
        mov eax, [es:di+0x10]
        mov ecx, [es:di+0x0C]
        jmp .walk
.dmi:
        mov eax, [es:di+8]
        movzx ecx, word [es:di+6]
.walk:
        mov [sm_addr], eax
        add ecx, eax
        mov [sm_end], ecx
        mov word [sm_n], 500        ; tope de estructuras
.st:
        mov eax, [sm_addr]
        cmp eax, [sm_end]
        jae .no
        dec word [sm_n]
        jz .no
        call sm_read
        jc .no
        mov word [smbuf+SM_BUF], 0  ; tope: las búsquedas acaban aquí
        mov si, smbuf
        mov al, [si]                ; tipo
        cmp al, 127                 ; fin de la tabla
        je .no
        movzx bx, byte [si+1]       ; longitud de la parte fija
        cmp bx, 4
        jb .no
        cmp bx, SM_BUF-2
        jae .no
        cmp al, 1
        je .type1
        ; saltar la estructura: tras la parte fija, cadenas hasta dos ceros
        add si, bx
.e1:
        cmp si, smbuf+SM_BUF-1
        jae .no
        cmp word [si], 0
        je .e2
        inc si
        jmp .e1
.e2:
        add si, 2
        sub si, smbuf
        movzx eax, si
        add [sm_addr], eax
        jmp .st
.type1:
        cmp bx, 5
        jb .no
        movzx cx, byte [si+4]       ; n.º de la cadena del fabricante
        jcxz .no
        add si, bx
.s1:
        dec cx
        jz .s3
.s2:
        lodsb                       ; saltar una cadena
        test al, al
        jnz .s2
        cmp byte [si], 0            ; no hay tantas
        je .no
        jmp .s1
.s3:
        cmp byte [si], 0
        je .no
        cmp dword [si], 'Appl'
        jne .s4
        cmp byte [si+4], 'e'
        je .yes
.s4:
        inc si
        jmp .s3
.yes:
        mov al, 1
        pop es
        ret
.no:
        xor al, al
        pop es
        ret

; copiar SM_BUF octetos de la memoria física [sm_addr] a smbuf -> CF si falla
sm_read:
        push es
        push si
        push di
        push cx
        mov eax, [sm_addr]
        cmp eax, 0x100000 - SM_BUF
        ja .high
        ; por debajo del megabyte: se copia sin más
        push ds
        mov si, ax
        and si, 0x000F
        shr eax, 4
        push ds
        pop es
        mov ds, ax
        mov di, smbuf
        mov cx, SM_BUF
        cld
        rep movsb
        pop ds
        clc
        jmp .r
        ; por encima (los equipos con EFI): movimiento de bloques de la BIOS
.high:
        call a20_test
        mov [sm_a20], al
        mov eax, [sm_addr]
        mov [sm_gdt+0x12], ax
        shr eax, 16
        mov [sm_gdt+0x14], al
        mov [sm_gdt+0x17], ah
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, smbuf
        mov [sm_gdt+0x1A], ax
        shr eax, 16
        mov [sm_gdt+0x1C], al
        mov [sm_gdt+0x1F], ah
        push cs
        pop es
        mov si, sm_gdt
        mov cx, SM_BUF/2
        mov ah, 0x87
        int 0x15
        pushf
        ; hay BIOS (las del AT) que dejan la línea A20 cerrada al volver: si
        ; estaba abierta (HSPXMS, Windows) se vuelve a abrir
        cmp byte [sm_a20], 0
        je .a2
        call a20_test
        test al, al
        jnz .a2
        call a20_reopen
.a2:
        popf
.r:
        pop cx
        pop di
        pop si
        pop es
        ret

; -> AL = 1 si la línea A20 está abierta (0000:0500 y FFFF:0510 son sitios
; distintos), 0 si está cerrada
a20_test:
        push ds
        push es
        push si
        push di
        pushf
        cli
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov si, 0x0500
        mov di, 0x0510
        mov al, [si]
        mov ah, [es:di]
        push ax
        mov byte [si], 0x55
        mov byte [es:di], 0xAA
        cmp byte [si], 0x55
        pop ax
        mov [es:di], ah
        mov [si], al
        mov al, 0
        jne .c
        mov al, 1
.c:
        popf
        pop di
        pop si
        pop es
        pop ds
        ret

; abrir la línea A20: por el driver XMS si lo hay; si no (o si no basta),
; por el puerto 92h
a20_reopen:
        push bx
        push es
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .port
        mov ax, 0x4310
        int 0x2F
        mov [sm_xms], bx
        mov [sm_xms+2], es
        mov ah, 3
        call far [sm_xms]
        call a20_test
        test al, al
        jnz .r
.port:
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al
.r:
        pop es
        pop bx
        ret

sm_a20      db 0
sm_xms      dd 0
lay         db 0                    ; 1 = SP/ES, 2 = US
macopt      db 0                    ; 1 = /MAC, 2 = /NOMAC
sm_addr     dd 0
sm_end      dd 0
sm_n        dw 0
sm_gdt      times 16 db 0
            dw 0xFFFF, 0            ; origen
            db 0, 0x93, 0, 0
            dw 0xFFFF, 0            ; destino
            db 0, 0x93, 0, 0
            times 16 db 0

msg_usage   db "Uso: TECLADO [SP|ES|US] [/MAC|/NOMAC]", 13, 10
            db "  SP o ES  teclado espa", 0xA4, "ol        US  teclado estadounidense", 13, 10
            db "  /MAC     teclas que faltan en los port", 0xA0, "tiles de Apple (Alt = Opci", 0xA2, "n):", 13, 10
            db "           Ctrl+Retroceso = Supr, Alt+Retroceso = Insertar,", 13, 10
            db "           Alt+izquierda/derecha = Inicio/Fin, Alt+arriba/abajo = RePág/AvPág", 13, 10
            db "           (se activan solas si el equipo es de Apple)", 13, 10
            db "  /NOMAC   las quita", 13, 10, 0
msg_inst    db "Teclado espa", 0xA4, "ol instalado.", 13, 10, 0
msg_cur_es  db "Teclado activo: espa", 0xA4, "ol (SP).", 13, 10, 0
msg_cur_us  db "Teclado activo: estadounidense (US).", 13, 10, 0
msg_mac     db "Teclado de Mac: Ctrl+Retroceso = Supr, Alt+flechas = Inicio/Fin/RePág/AvPág,", 13, 10
            db "Alt+Retroceso = Insertar", 13, 10, 0
msg_nomac   db "Teclado de Mac: desactivado.", 13, 10, 0

smbuf:                              ; SM_BUF + 2 octetos, detrás del programa
