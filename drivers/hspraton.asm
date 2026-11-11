; =============================================================================
;  HSPRATON.COM - driver de ratón (INT 33h) de Hispa-DOS
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Uso:  HSPRATON        instalar (en AUTOEXEC.BAT)
;
;  Ratón PS/2 a través de los servicios de la BIOS (INT 15h C2xxh). Los ratones
;  USB llegan por HSPUSB.SYS: como paquetes PS/2 (controlador 8042) o, si no
;  hay ratón PS/2, por la entrada privada INT 33h AX=48D0h.
;  Cursor por software en los modos de texto y en el modo 13h (320x200x256).
;  Funciones: 00h-0Fh, 10h, 13h-1Fh, 20h-24h, 26h, 2Ah.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        jmp init

; ---- estado (lo que guardan las funciones 16h/17h) ----
state:
mx          dw 0                    ; posición en coordenadas virtuales
my          dw 0
minx        dw 0
maxx        dw 639
miny        dw 0
maxy        dw 199
cflag       dw -1                   ; 0 = visible
btn         dw 0
mickx       dw 0                    ; contadores de la función 0Bh
micky       dw 0
remx        dw 0
remy        dw 0
ratx        dw 8                    ; mickeys por 8 puntos
raty        dw 16
prs_cnt     dw 0, 0, 0
prs_x       dw 0, 0, 0
prs_y       dw 0, 0, 0
rel_cnt     dw 0, 0, 0
rel_x       dw 0, 0, 0
rel_y       dw 0, 0, 0
umask       dw 0
uhand       dd 0
smask       dw 0x77FF               ; cursor de texto: máscaras
cmask       dw 0x7700
hotx        dw 0
hoty        dw 0
gmask       dw 0x3FFF, 0x1FFF, 0x0FFF, 0x07FF, 0x03FF, 0x01FF, 0x00FF, 0x007F
            dw 0x003F, 0x001F, 0x01FF, 0x10FF, 0x30FF, 0xF87F, 0xF87F, 0xFC3F
            dw 0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7C00, 0x7E00, 0x7F00
            dw 0x7F80, 0x7C00, 0x6C00, 0x4600, 0x0600, 0x0300, 0x0300, 0x0000
sens        dw 50, 50, 50
page        dw 0
disabled    db 0
            db 0
STATE_LEN   equ $ - state

; ---- variables internas ----
old33       dd 0
old10       dd 0
ps2         db 0                    ; hay ratón PS/2
in10        db 0                    ; dentro de INT 10h: no dibujar
busy        db 0
drawn       db 0                    ; 1 = texto, 2 = gráfico
vmode       db 0xFF
cols        dw 80
dseg        dw 0xB800
doff        dw 0                    ; texto: desplazamiento de la celda
dsave       dw 0                    ; texto: carácter y atributo debajo
gx          dw 0                    ; gráfico: esquina del cursor
gy          dw 0
gsave       times 256 db 0

dmask:  ; máscaras por defecto (para el reinicio)
            dw 0x3FFF, 0x1FFF, 0x0FFF, 0x07FF, 0x03FF, 0x01FF, 0x00FF, 0x007F
            dw 0x003F, 0x001F, 0x01FF, 0x10FF, 0x30FF, 0xF87F, 0xF87F, 0xFC3F
            dw 0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7C00, 0x7E00, 0x7F00
            dw 0x7F80, 0x7C00, 0x6C00, 0x4600, 0x0600, 0x0300, 0x0300, 0x0000

; =============================================================================
;  INT 33h
; =============================================================================
int33:
        cmp ax, 0x48D0              ; entrada privada: movimiento de HSPUSB
        je feed
        cmp ax, 0x48D2              ; ¿está HSPRATON? -> AX = 5253h ('RS')
        jne .n
        mov ax, 0x5253
        iret
.n:
        cmp ax, 0x2A
        ja .old
        push bx
        mov bx, ax
        shl bx, 1
        mov bx, [cs:ftab+bx]
        mov [cs:fjmp], bx
        pop bx
        push ds
        push cs
        pop ds
        cld
        call [fjmp]
        pop ds
        iret
.old:
        iret
fjmp        dw 0

ftab        dw f00, f01, f02, f03, f04, f05, f06, f07
            dw f08, f09, f0a, f0b, f0c, fnop, fnop, f0f
            dw f10, fnop, fnop, fnop, f14, f15, f16, f17
            dw fbad, fbad, f1a, f1b, fnop, f1d, f1e, f1f
            dw f20, f21, fnop, f23, f24, fnop, f26, fnop
            dw fnop, fnop, f2a

fnop:
        ret
fbad:
        mov ax, 0xFFFF
        ret

; 00h: reiniciar
f00:
        call reset_all
        mov ax, 0xFFFF
        mov bx, 2
        ret
; 21h: reinicio por software
f21:
        call reset_all
        mov ax, 0xFFFF
        mov bx, 2
        ret

reset_all:
        call hide_now
        mov word [cflag], -1
        call mode_info
        mov ax, [maxx]
        inc ax
        shr ax, 1
        mov [mx], ax
        mov ax, [maxy]
        inc ax
        shr ax, 1
        mov [my], ax
        xor ax, ax
        mov [mickx], ax
        mov [micky], ax
        mov [remx], ax
        mov [remy], ax
        mov [umask], ax
        mov [uhand], ax
        mov [uhand+2], ax
        mov [hotx], ax
        mov [hoty], ax
        mov [page], ax
        mov [disabled], al
        mov word [ratx], 8
        mov word [raty], 16
        mov word [smask], 0x77FF
        mov word [cmask], 0x7700
        push es
        push cs
        pop es
        mov di, prs_cnt
        mov cx, 18
        rep stosw
        mov si, dmask
        mov di, gmask
        mov cx, 32
        rep movsw
        pop es
        ret

; límites por defecto según el modo de vídeo de la BIOS
mode_info:
        push es
        push ax
        push bx
        xor ax, ax
        mov es, ax
        mov al, [es:0x449]
        mov [vmode], al
        mov bx, [es:0x44A]
        mov [cols], bx
        mov word [dseg], 0xB800
        cmp al, 7
        jne .c
        mov word [dseg], 0xB000
.c:
        mov word [minx], 0
        mov word [maxx], 639
        mov word [miny], 0
        mov ax, 199
        mov bl, [vmode]
        cmp bl, 3
        jbe .text
        cmp bl, 7
        je .text
        cmp bl, 0x0F
        je .h350
        cmp bl, 0x10
        je .h350
        cmp bl, 0x11
        je .h480
        cmp bl, 0x12
        je .h480
        jmp .set
.text:
        movzx ax, byte [es:0x484]   ; filas - 1
        test ax, ax
        jnz .t
        mov ax, 24
.t:
        inc ax
        shl ax, 3
        dec ax
        jmp .set
.h350:
        mov ax, 349
        jmp .set
.h480:
        mov ax, 479
.set:
        mov [maxy], ax
        pop bx
        pop ax
        pop es
        ret

; 01h: mostrar
f01:
        cmp word [cflag], 0
        jge .r
        inc word [cflag]
        jnz .r
        call show_now
.r:
        ret
; 02h: ocultar
f02:
        cmp word [cflag], 0
        jne .d
        call hide_now
.d:
        dec word [cflag]
        ret
; 10h: ocultar en una zona (se oculta sin más)
f10:
        jmp f02

; 03h: estado
f03:
        mov bx, [btn]
        mov cx, [mx]
        mov dx, [my]
        ret
; 04h: colocar
f04:
        call hide_now
        mov [mx], cx
        mov [my], dx
        call clip
        call show_now
        ret

; 05h / 06h: pulsaciones y liberaciones del botón BX
f05:
        mov si, prs_cnt
        jmp btninfo
f06:
        mov si, rel_cnt
btninfo:
        cmp bx, 2
        ja .bad
        shl bx, 1
        mov ax, [btn]
        push ax
        mov ax, [si+bx]
        mov word [si+bx], 0
        mov cx, [si+bx+6]
        mov dx, [si+bx+12]
        mov bx, ax
        pop ax
        ret
.bad:
        mov ax, [btn]
        xor bx, bx
        ret

; 07h / 08h: límites
f07:
        cmp cx, dx
        jle .o
        xchg cx, dx
.o:
        mov [minx], cx
        mov [maxx], dx
        jmp relimit
f08:
        cmp cx, dx
        jle .o
        xchg cx, dx
.o:
        mov [miny], cx
        mov [maxy], dx
relimit:
        call hide_now
        call clip
        call show_now
        ret

; 09h: cursor gráfico (BX, CX = punto activo; ES:DX = máscaras)
f09:
        call hide_now
        mov [hotx], bx
        mov [hoty], cx
        push ds
        push es
        push es
        push cs
        pop es
        pop ds
        mov si, dx
        mov di, gmask
        mov cx, 32
        rep movsw
        pop es
        pop ds
        call show_now
        ret
; 0Ah: cursor de texto (BX = 0 software: CX y DX máscaras)
f0a:
        test bx, bx
        jnz .r
        call hide_now
        mov [smask], cx
        mov [cmask], dx
        call show_now
.r:
        ret
; 0Bh: contadores de movimiento
f0b:
        xor cx, cx
        xor dx, dx
        xchg cx, [mickx]
        xchg dx, [micky]
        ret
; 0Ch: rutina del usuario
f0c:
        mov [umask], cx
        mov [uhand], dx
        mov [uhand+2], es
        ret
; 14h: intercambiar la rutina del usuario
f14:
        xchg cx, [umask]
        xchg dx, [uhand]
        push ax
        mov ax, es
        xchg ax, [uhand+2]
        mov es, ax
        pop ax
        ret
; 0Fh: proporción mickeys/puntos
f0f:
        test cx, cx
        jz .y
        mov [ratx], cx
.y:
        test dx, dx
        jz .r
        mov [raty], dx
.r:
        ret
; 15h-17h: guardar y restaurar el estado
f15:
        mov bx, STATE_LEN
        ret
f16:
        push es
        mov di, dx
        mov si, state
        mov cx, STATE_LEN
        rep movsb
        pop es
        ret
f17:
        call hide_now
        push ds
        push es
        push es
        push cs
        pop es
        pop ds
        mov si, dx
        mov di, state
        mov cx, STATE_LEN
        rep movsb
        pop es
        pop ds
        cmp word [cflag], 0
        jne .r
        call show_now
.r:
        ret
; 1Ah / 1Bh: sensibilidad
f1a:
        mov [sens], bx
        mov [sens+2], cx
        mov [sens+4], dx
        ret
f1b:
        mov bx, [sens]
        mov cx, [sens+2]
        mov dx, [sens+4]
        ret
; 1Dh / 1Eh: página de vídeo
f1d:
        mov [page], bx
        ret
f1e:
        mov bx, [page]
        ret
; 1Fh / 20h: desactivar y activar
f1f:
        call hide_now
        mov byte [disabled], 1
        mov ax, 0x1F
        les bx, [old33]
        ret
f20:
        mov byte [disabled], 0
        ret
; 23h: idioma (6 = español)
f23:
        mov bx, 6
        ret
; 24h: versión 8.20, ratón PS/2
f24:
        mov bx, 0x0820
        mov cx, 0x0400
        ret
; 26h: máximos
f26:
        xor bx, bx
        mov cx, [maxx]
        mov dx, [maxy]
        ret
; 2Ah: estado del cursor
f2a:
        mov ax, [cflag]
        mov bx, [hotx]
        mov cx, [hoty]
        mov dx, 4
        ret

; mantener la posición dentro de los límites
clip:
        mov ax, [mx]
        cmp ax, [minx]
        jge .a
        mov ax, [minx]
.a:
        cmp ax, [maxx]
        jle .b
        mov ax, [maxx]
.b:
        mov [mx], ax
        mov ax, [my]
        cmp ax, [miny]
        jge .c
        mov ax, [miny]
.c:
        cmp ax, [maxy]
        jle .d
        mov ax, [maxy]
.d:
        mov [my], ax
        ret

; =============================================================================
;  Movimiento: CX = dx, DX = dy (mickeys, positivo hacia abajo), BX = botones
; =============================================================================
motion:
        cmp byte [disabled], 0
        jne .r
        cmp byte [busy], 0
        jne .r
        mov byte [busy], 1
        mov word [ev], 0
        ; contadores y posición
        add [mickx], cx
        add [micky], dx
        mov ax, cx
        or ax, dx
        jz .nomove
        or word [ev], 1
        call hide_now
        ; x
        mov ax, cx
        shl ax, 3
        add ax, [remx]
        cwd
        idiv word [ratx]
        mov [remx], dx
        add [mx], ax
        ; y
        mov ax, [mickd]
        shl ax, 3
        add ax, [remy]
        cwd
        idiv word [raty]
        mov [remy], dx
        add [my], ax
        call clip
.nomove:
        ; botones
        mov ax, [btn]
        xor ax, bx
        jz .nob
        mov [chg], ax
        mov [btn], bx
        xor si, si                  ; botón
        mov di, 2                   ; bit del suceso "pulsado"
.bl:
        mov ax, 1
        mov cx, si
        shl ax, cl
        test [chg], ax
        jz .bn
        push si
        shl si, 1
        test [btn], ax
        jz .rel
        inc word [prs_cnt+si]
        mov ax, [mx]
        mov [prs_x+si], ax
        mov ax, [my]
        mov [prs_y+si], ax
        or [ev], di
        jmp .bp
.rel:
        inc word [rel_cnt+si]
        mov ax, [mx]
        mov [rel_x+si], ax
        mov ax, [my]
        mov [rel_y+si], ax
        mov ax, di
        shl ax, 1
        or [ev], ax
.bp:
        pop si
.bn:
        shl di, 2
        inc si
        cmp si, 3
        jb .bl
.nob:
        cmp word [cflag], 0
        jne .user
        call show_now
.user:
        ; rutina del usuario
        mov ax, [ev]
        and ax, [umask]
        jz .done
        cmp dword [uhand], 0
        je .done
        mov bx, [btn]
        mov cx, [mx]
        mov dx, [my]
        mov si, [mickx]
        mov di, [micky]
        push ds
        call far [uhand]
        pop ds
.done:
        mov byte [busy], 0
.r:
        ret
ev          dw 0
chg         dw 0
mickd       dw 0

; entrada privada de HSPUSB: BX botones, CX dx, DX dy (hacia abajo)
feed:
        push ds
        pusha
        push es
        push cs
        pop ds
        mov [mickd], dx
        call motion
        pop es
        popa
        pop ds
        mov ax, 0x48D1
        iret

; rutina que llama la BIOS (INT 15h C207h) con cada paquete PS/2
ps2_cb:
        push bp
        mov bp, sp
        pusha
        push ds
        push es
        push cs
        pop ds
        mov bl, [bp+12]             ; estado
        test bl, 0xC0               ; desbordamiento: se descarta
        jnz .r
        movzx cx, byte [bp+10]      ; x
        test bl, 0x10
        jz .x
        or ch, 0xFF
.x:
        movzx dx, byte [bp+8]       ; y (positivo hacia arriba)
        test bl, 0x20
        jz .y
        or dh, 0xFF
.y:
        neg dx
        mov [mickd], dx
        and bx, 7
        call motion
.r:
        pop es
        pop ds
        popa
        pop bp
        retf

; =============================================================================
;  Cursor
; =============================================================================
show_now:
        cmp byte [drawn], 0
        jne .r
        cmp word [cflag], 0
        jne .r
        cmp byte [in10], 0
        jne .r
        push es
        pusha
        xor ax, ax
        mov es, ax
        mov al, [es:0x449]
        cmp al, [vmode]
        je .m
        call mode_info
        call clip
.m:
        mov al, [vmode]
        cmp al, 3
        jbe .text
        cmp al, 7
        je .text
        cmp al, 0x13
        je .g13
        jmp .out
.text:
        ; celda: columna = x * columnas / 640, fila = y / 8
        mov ax, [mx]
        mul word [cols]
        mov bx, 640
        div bx
        mov bx, ax
        mov ax, [my]
        shr ax, 3
        mul word [cols]
        add ax, bx
        shl ax, 1
        xor bx, bx
        mov es, bx
        add ax, [es:0x44E]          ; página activa
        mov [doff], ax
        mov di, ax
        mov es, [dseg]
        mov ax, [es:di]
        mov [dsave], ax
        and ax, [smask]
        xor ax, [cmask]
        mov [es:di], ax
        mov byte [drawn], 1
        jmp .out
.g13:
        mov ax, [mx]
        shr ax, 1
        sub ax, [hotx]
        mov [gx], ax
        mov ax, [my]
        sub ax, [hoty]
        mov [gy], ax
        mov ax, 0xA000
        mov es, ax
        xor bx, bx                  ; fila
.gr:
        mov ax, [gy]
        add ax, bx
        cmp ax, 200
        jae .gnr                    ; fuera (también negativo)
        imul di, ax, 320
        mov si, bx
        shl si, 1
        mov dx, [gmask+si]          ; máscara de pantalla
        mov bp, [gmask+32+si]       ; máscara del cursor
        xor cx, cx                  ; columna
.gc:
        mov ax, [gx]
        add ax, cx
        cmp ax, 320
        jae .gnc
        push di
        add di, ax
        mov al, [es:di]
        mov si, bx
        shl si, 4
        add si, cx
        mov [gsave+si], al
        test dx, 0x8000
        jnz .k
        xor al, al
.k:
        test bp, 0x8000
        jz .w
        xor al, 0x0F
.w:
        mov [es:di], al
        pop di
.gnc:
        shl dx, 1
        shl bp, 1
        inc cx
        cmp cx, 16
        jb .gc
.gnr:
        inc bx
        cmp bx, 16
        jb .gr
        mov byte [drawn], 2
.out:
        popa
        pop es
.r:
        ret

hide_now:
        cmp byte [drawn], 0
        je .r
        push es
        pusha
        cmp byte [drawn], 2
        je .g
        mov es, [dseg]
        mov di, [doff]
        mov ax, [dsave]
        mov [es:di], ax
        jmp .d
.g:
        mov ax, 0xA000
        mov es, ax
        xor bx, bx
.gr:
        mov ax, [gy]
        add ax, bx
        cmp ax, 200
        jae .gnr
        imul di, ax, 320
        xor cx, cx
.gc:
        mov ax, [gx]
        add ax, cx
        cmp ax, 320
        jae .gnc
        push di
        add di, ax
        mov si, bx
        shl si, 4
        add si, cx
        mov al, [gsave+si]
        mov [es:di], al
        pop di
.gnc:
        inc cx
        cmp cx, 16
        jb .gc
.gnr:
        inc bx
        cmp bx, 16
        jb .gr
.d:
        mov byte [drawn], 0
        popa
        pop es
.r:
        ret

; =============================================================================
;  INT 10h: quitar el cursor mientras la BIOS escribe en pantalla
; =============================================================================
int10:
        cmp ah, 0x0F                ; consultas que no tocan la pantalla
        je .old
        cmp ah, 0x03
        je .old
        cmp ah, 0x1A
        je .old
        push ds
        push cs
        pop ds
        inc byte [in10]
        call hide_now
        pop ds
        pushf
        call far [cs:old10]
        push ds
        push ax
        push cs
        pop ds
        dec byte [in10]
        call show_now               ; (si cambió el modo, show_now lo detecta)
        pop ax
        pop ds
        iret
.old:
        jmp far [cs:old10]

resident_end:

; =============================================================================
;  Instalación
; =============================================================================
init:
        mov si, msg_title
        call puts
        ; ¿ya instalado?
        mov ax, 0x48D2
        xor bx, bx
        mov es, bx
        cmp dword [es:0x33*4], 0
        je .new
        int 0x33
        cmp ax, 0x5253
        jne .new
        mov si, msg_already
        call puts
        mov ax, 0x4C01
        int 0x21
.new:
        ; ratón PS/2 de la BIOS
        int 0x11
        test al, 4                  ; dispositivo señalador
        jz .nops2
        mov ax, 0xC205              ; inicializar, paquetes de 3 bytes
        mov bh, 3
        int 0x15
        jc .nops2
        mov ax, 0xC203              ; resolución: 4 puntos/mm
        mov bh, 2
        int 0x15
        push cs
        pop es
        mov bx, ps2_cb
        mov ax, 0xC207
        int 0x15
        jc .nops2
        mov ax, 0xC200              ; activar
        mov bh, 1
        int 0x15
        jc .nops2
        mov byte [ps2], 1
        mov si, msg_ps2
        call puts
        jmp .vec
.nops2:
        mov si, msg_nops2
        call puts
.vec:
        mov ax, 0x3533
        int 0x21
        mov [old33], bx
        mov [old33+2], es
        mov ax, 0x3510
        int 0x21
        mov [old10], bx
        mov [old10+2], es
        mov dx, int33
        mov ax, 0x2533
        int 0x21
        mov dx, int10
        mov ax, 0x2510
        int 0x21
        call reset_all
        ; liberar el entorno y quedar residente
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

msg_title   db "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)", 13, 10, 0
msg_ps2     db "Ratón PS/2 preparado.", 13, 10, 0
msg_nops2   db "No hay ratón PS/2: se usará un ratón USB (HSPUSB.SYS) si lo hay.", 13, 10, 0
msg_already db "HSPRATON ya está instalado.", 13, 10, 0
