; =============================================================================
;  HSPRATON.COM - driver de ratón (INT 33h) de Hispa-DOS
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Uso:  HSPRATON [/BIOS | /DIRECTO] [/SIMPLE] [/INFO]     (en AUTOEXEC.BAT)
;
;    (sin opciones)  prueba primero la BIOS (INT 15h C2xxh) y, si no ofrece
;                    ratón, habla directamente con el controlador 8042
;    /BIOS           solo por la BIOS
;    /DIRECTO        solo directamente por el 8042 (puerto auxiliar, IRQ 12)
;    /SIMPLE         sin sondeos (rueda, Synaptics): reinicio y activación
;    /INFO           diagnóstico en pantalla; con el driver ya cargado enseña
;                    lo que se anotó al cargar y los contadores en vivo
;
;  Ratón PS/2 y touchpads de portátil (Synaptics, ALPS, Elantech...), que tras
;  el reinicio se comportan como un ratón PS/2 normal (movimiento relativo).
;
;  Vía BIOS: la BIOS atiende la IRQ 12 y nos llama con cada paquete.
;  Vía directa (según las especificaciones públicas del 8042 de IBM PS/2 y del
;  protocolo del ratón PS/2): con el teclado en pausa (ADh/AEh) se activa el
;  puerto auxiliar (A8h), se pone en el byte de órdenes la interrupción del
;  auxiliar (20h/60h; solo si hace falta y sin tocar los bits del teclado),
;  se reinicia el dispositivo (FFh -> AAh 00h), F6h, frecuencia y resolución,
;  y se activa (F4h). La IRQ 12 (INT 74h) pasa a ser nuestra: solo lee del
;  puerto 60h los bytes marcados como del auxiliar (bit 5 del estado), así
;  que no quita teclas al teclado, ni a la emulación USB de la BIOS, ni a
;  HSPUSB (órdenes D2h/D3h). Paquetes de 3 bytes (4 con IntelliMouse; la
;  rueda no se entrega), sincronizados por el bit 3 del primer byte y por
;  tiempo. Todos los plazos van por el reloj de la BIOS: en un PC sin 8042
;  se avisa y no se cuelga.
;
;  Los ratones USB llegan por HSPUSB.SYS: como paquetes PS/2 (controlador
;  8042) o, si no hay ratón PS/2, por la entrada privada INT 33h AX=48D0h.
;  Cursor por software en los modos de texto y en el modo 13h (320x200x256).
;  Funciones: 00h-0Fh, 10h, 13h-1Fh, 20h-24h, 26h, 2Ah.
;  Privadas: AX=48D0h (movimiento de HSPUSB), 48D2h (¿está?), 48D3h (ES:BX =
;  datos del diagnóstico).
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
in10        db 0                 ; dentro de INT 10h: no dibujar
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

; ---- vía directa (8042) ----
old74       dd 0
pk_len      db 3                    ; bytes por paquete (4 = IntelliMouse)
pk_n        db 0                    ; bytes recibidos del paquete en curso
pk_buf      db 0, 0, 0, 0
pk_tick     dw 0                    ; reloj de la BIOS en el último byte
ack_wait    db 0                    ; se espera el FAh de una orden F4h

; ---- datos del diagnóstico (/INFO; INT 33h AX=48D3h -> ES:BX) ----
info:
i_via       db 0                    ; 0 ninguna, 1 BIOS, 2 directa
i_dev       db 0                    ; 1 ratón PS/2, 2 IntelliMouse, 3 Synaptics
i_have      dw 0                    ; qué datos de abajo valen (H_xxx)
i_eq        db 0                    ; INT 11h: bit del ratón
i_biosstep  db 0                    ; FFh bien; si no, función C2xxh que falló
i_biosah    db 0                    ; su código de error
i_st64      db 0                    ; estado del 8042 al empezar
i_ctr0      db 0                    ; byte de órdenes: antes
i_ctr1      db 0                    ; y después
i_auxt      db 0                    ; prueba del puerto auxiliar (A9h)
i_dirres    db 0                    ; vía directa: 0 bien, o el motivo
i_rst       db 0, 0, 0              ; respuesta al reinicio (FA AA 00)
i_id        db 0                    ; identificador (F2h)
i_syn       db 0, 0, 0              ; identificación Synaptics
i_pic       db 0, 0                 ; máscaras de los PIC al acabar
c_irq       dw 0                    ; IRQ 12 recibidas
c_bytes     dw 0                    ; bytes del auxiliar
c_pkts      dw 0                    ; paquetes completos
c_bad       dw 0                    ; bytes y paquetes descartados
c_kbd       dw 0                    ; IRQ 12 con un byte que era del teclado
c_rst       dw 0                    ; reinicios del dispositivo (AA 00)
INFO_LEN    equ $ - info

H_CTR0      equ 0x0001
H_CTR1      equ 0x0002
H_AUXT      equ 0x0004
H_RST0      equ 0x0008
H_RST1      equ 0x0010
H_RST2      equ 0x0020
H_ID        equ 0x0040
H_SYN       equ 0x0080
H_BIOS      equ 0x0100              ; se probó la vía BIOS
H_DIR       equ 0x0200              ; se probó la vía directa
H_NOIRQ     equ 0x0400              ; el dispositivo contesta, la IRQ 12 no

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
        jne .n3
        mov ax, 0x5253
        iret
.n3:
        cmp ax, 0x48D3              ; datos del diagnóstico -> ES:BX
        jne .n
        push cs
        pop es
        mov bx, info
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
        ; INT 33h solo cambia los registros en los que devuelve algo: SI y
        ; DI (que varias funciones usan para copiar) se conservan aquí
        push ds
        push si
        push di
        push cs
        pop ds
        cld
        call [fjmp]
        pop di
        pop si
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
        push cx
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
        pop cx
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
        push cx
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
        pop cx
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
        push cx
        mov di, dx
        mov si, state
        mov cx, STATE_LEN
        rep movsb
        pop cx
        ret
f17:
        call hide_now
        push cx
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
        pop cx
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
        inc word [c_pkts]
        mov bl, [bp+12]             ; estado
        mov cl, [bp+10]             ; x
        mov dl, [bp+8]              ; y (positivo hacia arriba)
        call packet
        pop es
        pop ds
        popa
        pop bp
        retf

; paquete PS/2: BL = estado, CL = x, DL = y (positivo hacia arriba)
packet:
        test bl, 0xC0               ; desbordamiento: se descarta
        jnz .bad
        xor ch, ch
        test bl, 0x10
        jz .x
        mov ch, 0xFF
.x:
        xor dh, dh
        test bl, 0x20
        jz .y
        mov dh, 0xFF
.y:
        neg dx
        mov [mickd], dx
        and bx, 7
        call motion
        ret
.bad:
        inc word [c_bad]
        ret

; -----------------------------------------------------------------------------
;  IRQ 12 (INT 74h) en la vía directa. Solo se lee del puerto 60h lo que el
;  8042 marca como del auxiliar (bit 5 del estado): lo demás es del teclado
;  (o una tecla que inyecta HSPUSB con D2h) y lo recogerá INT 09h.
; -----------------------------------------------------------------------------
irq12:
        pusha
        push ds
        push es
        push cs
        pop ds
        inc word [c_irq]
        in al, 0x64
        test al, 1
        jz .eoi
        test al, 0x20
        jnz .aux
        inc word [c_kbd]
        jmp .eoi
.aux:
        in al, 0x60
        inc word [c_bytes]
        ; un hueco largo entre dos bytes de un paquete: se había roto
        xor bx, bx
        mov es, bx
        mov bx, [es:0x46C]
        mov cx, bx
        sub cx, [pk_tick]
        mov [pk_tick], bx
        cmp byte [pk_n], 0
        je .first
        cmp cx, 2
        jbe .store
        inc word [c_bad]
        mov byte [pk_n], 0
.first:
        cmp byte [ack_wait], 0
        je .data
        cmp al, 0xFE                ; «repita» o error: se sigue esperando
        je .eoi
        cmp al, 0xFC
        je .eoi
        mov byte [ack_wait], 0
        cmp al, 0xFA                ; el «recibido» de nuestra orden F4h
        je .eoi
.data:
        test al, 8                  ; el primer byte siempre lleva el bit 3
        jnz .store
        inc word [c_bad]
        jmp .eoi
.store:
        movzx bx, byte [pk_n]
        mov [pk_buf+bx], al
        inc bx
        mov [pk_n], bl
        cmp bl, 2
        jne .len
        cmp word [pk_buf], 0x00AA   ; AA 00: el dispositivo se ha reiniciado
        jne .len                    ; (p. ej. al volver de una suspensión)
        mov byte [pk_n], 0
        mov byte [pk_len], 3
        call rearm
        jmp .eoi
.len:
        cmp bl, [pk_len]
        jb .eoi
        mov byte [pk_n], 0
        inc word [c_pkts]
        mov al, 0x20                ; fin de interrupción a los dos PIC
        out 0xA0, al
        out 0x20, al
        mov bl, [pk_buf]
        mov cl, [pk_buf+1]
        mov dl, [pk_buf+2]
        call packet
        jmp .ret
.eoi:
        mov al, 0x20
        out 0xA0, al
        out 0x20, al
.ret:
        pop es
        pop ds
        popa
        iret

; volver a activar el dispositivo (F4h); el FAh llegará por la IRQ 12.
; Esperas acotadas: se está dentro de una interrupción.
rearm:
        xor cx, cx
.w1:
        in al, 0x64
        test al, 2
        loopnz .w1
        mov al, 0xD4
        out 0x64, al
        xor cx, cx
.w2:
        in al, 0x64
        test al, 2
        loopnz .w2
        mov al, 0xF4
        out 0x60, al
        mov byte [ack_wait], 1
        inc word [c_rst]
        ret

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
        cld
        mov si, msg_title
        call puts
        call parse
        jnc .pok
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
.pok:
        cmp byte [o_help], 0
        je .nohelp
        mov si, msg_usage
        call puts
        mov ax, 0x4C00
        int 0x21
.nohelp:
        ; ¿ya instalado?
        mov ax, 0x48D2
        xor bx, bx
        mov es, bx
        cmp dword [es:0x33*4], 0
        je .new
        int 0x33
        cmp ax, 0x5253
        jne .new
        cmp byte [o_info], 0
        je .alr
        ; /INFO con el driver cargado: sus datos y sus contadores
        xor bx, bx
        mov ax, 0x48D3
        int 0x33
        test bx, bx
        jz .alr
        push ds
        push es
        pop ds
        mov si, bx
        push cs
        pop es
        mov di, info
        mov cx, INFO_LEN
        cld
        rep movsb
        pop ds
        call show_info
.alr:
        mov si, msg_already
        call puts
        mov ax, 0x4C01
        int 0x21
.new:
        sti
        cld
        int 0x11
        shr al, 2
        and al, 1
        mov [i_eq], al
        in al, 0x64
        mov [i_st64], al
        cmp byte [o_via], 2
        je .dir
        call bios_init
        jnc .ready
        cmp byte [o_via], 1
        je .ready
.dir:
        call dir_init
.ready:
        cmp byte [o_info], 0
        je .noinfo
        call show_info
.noinfo:
        call report
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

; -----------------------------------------------------------------------------
;  Opciones de la línea de órdenes. CF = 1 si alguna no se entiende.
; -----------------------------------------------------------------------------
parse:
        mov si, 0x81
.l:
        lodsb
        cmp al, ' '
        je .l
        cmp al, 9
        je .l
        cmp al, 13
        je .ok
        test al, al
        jz .ok
        cmp al, '/'
        je .opt
        cmp al, '-'
        je .opt
.bad:
        stc
        ret
.ok:
        clc
        ret
.opt:
        mov di, opts
.t:
        cmp byte [di], 0
        je .bad
        push si
.c:
        mov ah, [di]
        test ah, ah
        jz .end
        lodsb
        cmp al, 'a'
        jb .u
        cmp al, 'z'
        ja .u
        sub al, 32
.u:
        inc di
        cmp al, ah
        je .c
.sk:
        cmp byte [di], 0
        je .next
        inc di
        jmp .sk
.end:
        mov al, [si]                ; tras la opción: fin, espacio u otra
        cmp al, ' '
        je .hit
        cmp al, 9
        je .hit
        cmp al, 13
        je .hit
        cmp al, '/'
        je .hit
        test al, al
        jz .hit
.next:
        pop si
        add di, 2
        jmp .t
.hit:
        pop ax
        mov al, [di+1]
        cmp al, 1
        jne .h2
        mov byte [o_info], 1
.h2:
        cmp al, 2
        jne .h3
        mov byte [o_via], 1
.h3:
        cmp al, 3
        jne .h4
        mov byte [o_via], 2
.h4:
        cmp al, 4
        jne .h5
        mov byte [o_simple], 1
.h5:
        cmp al, 5
        jne .l
        mov byte [o_help], 1
        jmp .l

opts        db "INFO", 0, 1
            db "BIOS", 0, 2
            db "DIRECTO", 0, 3
            db "DIRECTA", 0, 3
            db "SIMPLE", 0, 4
            db "?", 0, 5
            db 0
o_info      db 0
o_via       db 0                    ; 0 automática, 1 BIOS, 2 directa
o_simple    db 0
o_help      db 0

; -----------------------------------------------------------------------------
;  Vía BIOS (INT 15h C2xxh). CF = 0 si el ratón queda activado.
;  No se mira el bit de INT 11h (hay BIOS que no lo ponen) y se reintenta: en
;  máquinas reales la primera orden al ratón falla a menudo (AH = 04h).
; -----------------------------------------------------------------------------
bios_init:
        or word [i_have], H_BIOS
        call aux_quiet
        mov byte [i_biosstep], 0x05
        mov byte [tries], 3
.rst:
        mov ax, 0xC205              ; inicializar, paquetes de 3 bytes
        mov bh, 3
        int 0x15
        jnc .rok
        mov [i_biosah], ah
        dec byte [tries]
        jnz .rst
        stc
        ret
.rok:
        cmp byte [o_simple], 0
        jne .conf
        ; ¿touchpad Synaptics? Cuatro «resolución 0» y una petición de estado:
        ; un Synaptics contesta con su identificación (47h en el centro)
        mov byte [tries], 4
.s1:
        mov ax, 0xC203
        mov bh, 0
        int 0x15
        jc .conf
        dec byte [tries]
        jnz .s1
        mov ax, 0xC206
        mov bh, 0
        int 0x15
        jc .conf
        mov [i_syn], bl
        mov [i_syn+1], cl
        mov [i_syn+2], dl
        or word [i_have], H_SYN
.conf:
        mov ax, 0xC203              ; resolución: 4 puntos/mm
        mov bh, 2
        int 0x15
        mov byte [i_biosstep], 0x07
        push cs
        pop es
        mov bx, ps2_cb
        mov ax, 0xC207
        int 0x15
        jc .fail
        mov byte [i_biosstep], 0x00
        mov byte [tries], 3
.ren:
        mov ax, 0xC200              ; activar
        mov bh, 1
        int 0x15
        jnc .eok
        mov [i_biosah], ah
        dec byte [tries]
        jnz .ren
        ; no se activa: quitar nuestra rutina
        xor bx, bx
        mov es, bx
        mov ax, 0xC207
        int 0x15
        stc
        ret
.fail:
        mov [i_biosah], ah
        stc
        ret
.eok:
        mov byte [i_biosstep], 0xFF
        mov byte [i_via], 1
        call set_dev
        clc
        ret

; qué dispositivo es, por lo que contestó
set_dev:
        cmp byte [i_dev], 0
        jne .syn
        mov byte [i_dev], 1
.syn:
        test word [i_have], H_SYN
        jz .r
        cmp byte [i_syn+1], 0x47
        jne .r
        mov byte [i_dev], 3
.r:
        ret

; -----------------------------------------------------------------------------
;  Plazos por el reloj de la BIOS (0040:006C, 18,2 tics por segundo), con un
;  tope de vueltas por si el reloj estuviera parado.
; -----------------------------------------------------------------------------
tmo_set:                            ; CX = tics
        push eax
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        mov [tmo_t0], ax
        mov [tmo_n], cx
        movzx eax, cx
        inc eax
        shl eax, 20
        mov [tmo_cnt], eax
        pop es
        pop eax
        ret
tmo_chk:                            ; CF = 1: plazo vencido
        push ax
        push es
        dec dword [tmo_cnt]
        jz .exp
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        sub ax, [tmo_t0]
        cmp ax, [tmo_n]
        ja .exp
        clc
        jmp .r
.exp:
        mov dword [tmo_cnt], 1
        stc
.r:
        pop es
        pop ax
        ret
pause:                              ; CX = tics
        call tmo_set
.l:
        call tmo_chk
        jnc .l
        ret
tmo_t0      dw 0
tmo_n       dw 0
tmo_cnt     dd 0

; -----------------------------------------------------------------------------
;  Controlador 8042
; -----------------------------------------------------------------------------
k_wait_in:                          ; esperar a que acepte (0,5 s); CF si no
        push cx
        mov cx, 9
        call tmo_set
        pop cx
.l:
        in al, 0x64
        test al, 2
        jz .ok
        call tmo_chk
        jnc .l
        ret
.ok:
        clc
        ret
k_cmd:                              ; orden AL al controlador (puerto 64h)
        push ax
        mov ah, al
        call k_wait_in
        jc .r
        mov al, ah
        out 0x64, al
        clc
.r:
        pop ax
        ret
k_dat:                              ; dato AL (puerto 60h)
        push ax
        mov ah, al
        call k_wait_in
        jc .r
        mov al, ah
        out 0x60, al
        clc
.r:
        pop ax
        ret
k_setctr:                           ; escribir el byte de órdenes AL
        push ax
        mov al, 0x60
        call k_cmd
        pop ax
        jc .r
        call k_dat
.r:
        ret
k_read:                             ; respuesta del controlador en CX tics:
        call tmo_set                ; AL = dato; CF si no llega
.l:
        in al, 0x64
        test al, 1
        jnz .got
        call tmo_chk
        jnc .l
        ret
.got:
        in al, 0x60
        clc
        ret
; orden AL al controlador y su respuesta (AL; CF si no llega). Con la IRQ 1
; tapada ese momento: hay controladores que la levantan con sus respuestas y
; se las llevaría INT 09h.
k_query:
        push bx
        push cx
        mov bl, al
        cli
        in al, 0x21
        mov bh, al
        or al, 0x02
        out 0x21, al
        sti
        mov al, bl
        call k_cmd
        jc .r
        mov cx, 9
        call k_read
.r:
        pushf
        push ax
        cli
        in al, 0x21
        and al, 0xFD
        and bh, 0x02
        or al, bh
        out 0x21, al
        pop ax
        popf
        pop cx
        pop bx
        ret
; Callar al ratón antes de pedirle nada a la BIOS: tras Ctrl+Alt+Supr el
; ratón sigue mandando movimientos (lo dejamos activado) y la BIOS, al
; reiniciarlo, recibe un trozo de paquete en vez de FAh; la de VirtualBox se
; detiene entonces con «FATAL: Mouse reset returned ...». Orden F5h (no
; enviar) por D4h y fuera lo pendiente del auxiliar.
aux_quiet:
        in al, 0x64
        cmp al, 0xFF                ; sin controlador de teclado
        je .r
        mov al, 0xD4
        call k_cmd
        jc .r
        mov al, 0xF5
        call k_dat
        jc .r
        push cx
        mov cx, 2
        call pause                  ; que llegue su FAh (y lo que quedara)
        pop cx
        call k_flush
.r:
        ret
k_flush:                            ; tirar lo que haya pendiente del auxiliar
        push cx                     ; (lo del teclado es de INT 09h)
        mov cx, 32
.l:
        cli
        in al, 0x64
        test al, 1
        jz .r
        and al, 0x21
        cmp al, 0x21
        jne .p
        in al, 0x60
.p:
        sti
        push cx
        mov cx, 1
        call pause
        pop cx
        loop .l
.r:
        sti
        pop cx
        ret

; byte del dispositivo auxiliar en CX tics: AL; CF si no llega. Lo que no
; venga marcado como del auxiliar (bit 5 del estado) no se toca: es una tecla
; (p. ej. inyectada por HSPUSB o por la BIOS) y la recoge INT 09h.
m_read:
        call tmo_set
.l:
        cli
        in al, 0x64
        and al, 0x21
        cmp al, 0x21
        jne .w
        in al, 0x60
        sti
        clc
        ret
.w:
        sti
        call tmo_chk
        jnc .l
        ret
; orden AL al dispositivo auxiliar (D4h); CF = 0 si contesta FAh
m_send:
        push ax
        push bx
        push cx
        mov bl, al
        mov bh, 3
        mov byte [m_last], 0xFF
.try:
        mov al, 0xD4
        call k_cmd
        jc .fail
        mov al, bl
        call k_dat
        jc .fail
        mov byte [m_skip], 8
.rd:
        mov cx, 10
        call m_read
        jc .fail
        mov [m_last], al
        cmp al, 0xFA
        je .ok
        cmp al, 0xFE                ; «repita» o error
        je .again
        cmp al, 0xFC
        je .again
        dec byte [m_skip]           ; restos de antes: se saltan
        jnz .rd
        jmp .fail
.again:
        dec bh
        jnz .try
.fail:
        stc
        jmp .r
.ok:
        clc
.r:
        pop cx
        pop bx
        pop ax                      ; (AX y los indicadores se conservan)
        ret
; orden AL con argumento AH
m_send2:
        call m_send
        jc .r
        xchg al, ah
        call m_send
        xchg al, ah
.r:
        ret
m_last      db 0xFF                 ; última respuesta (FFh: ninguna)
m_skip      db 0
tries       db 0
ctr_raw     db 0                    ; byte de órdenes leído
ctr_set     db 0                    ; lo hemos cambiado
pic2        db 0

; -----------------------------------------------------------------------------
;  Vía directa. CF = 0 si el dispositivo queda activado; si no, i_dirres dice
;  por qué y todo queda como estaba.
;    1 no hay 8042   2 el 8042 no responde   3 nada contesta en el auxiliar
;    4 el dispositivo no se deja activar
; -----------------------------------------------------------------------------
dir_init:
        or word [i_have], H_DIR
        mov byte [i_dirres], 1
        in al, 0x64
        cmp al, 0xFF                ; bus al aire: no hay controlador
        jne .have
        stc
        ret
.have:
        mov cx, 4                   ; que pase la suelta de la tecla Intro
        call pause
        mov byte [i_dirres], 2
        ; teclado en pausa mientras se habla con el ratón (las teclas se
        ; quedan en el teclado y salen después)
        mov al, 0xAD
        call k_cmd
        jnc .ad
        ret
.ad:
        mov cx, 2                   ; lo que ya hubiera del teclado es de INT 09h
        call tmo_set
.dr:
        in al, 0x64
        test al, 1
        jz .dq
        call tmo_chk
        jnc .dr
.dq:
        ; IRQ 12 tapada: las respuestas del ratón se leen aquí, por sondeo
        cli
        in al, 0xA1
        mov [pic2], al
        or al, 0x10
        out 0xA1, al
        sti
        call k_flush
        mov al, 0x20                ; leer el byte de órdenes
        call k_query
        jc .fail
        mov [ctr_raw], al
        and al, 0xEF
        mov [i_ctr0], al
        mov [i_ctr1], al
        or word [i_have], H_CTR0
        mov al, 0xA8                ; activar el puerto auxiliar
        call k_cmd
        jc .fail
        ; interrupción del auxiliar y su reloj. Solo se escribe si hace falta
        ; y si lo leído es creíble (bits reservados 3 y 7 a cero): los demás
        ; bits (teclado, traducción) no se tocan.
        mov al, [ctr_raw]
        test al, 0x88
        jnz .ctrok
        mov ah, al
        or al, 0x02
        and al, 0xDF
        cmp al, ah
        je .ctrok
        call k_setctr
        jc .fail
        mov byte [ctr_set], 1
        and al, 0xEF
        mov [i_ctr1], al
.ctrok:
        or word [i_have], H_CTR1
        cmp byte [o_info], 0
        je .noa9
        mov al, 0xA9                ; prueba del puerto auxiliar (00h = bien)
        call k_query
        jc .noa9
        mov [i_auxt], al
        or word [i_have], H_AUXT
.noa9:
        mov byte [i_dirres], 3
        call k_flush
        ; reinicio: FAh, y al acabar la autoprueba AAh 00h
        mov byte [tries], 2
.rst:
        mov al, 0xFF
        call m_send
        pushf
        mov ah, [m_last]
        mov [i_rst], ah
        or word [i_have], H_RST0
        popf
        jc .rnext
        mov byte [m_skip], 4
.raa:
        mov cx, 45                  ; hasta 2,5 s
        call m_read
        jc .rdone
        mov [i_rst+1], al
        or word [i_have], H_RST1
        cmp al, 0xAA
        je .rid
        dec byte [m_skip]
        jnz .raa
        jmp .rdone
.rid:
        mov cx, 10
        call m_read
        jc .rdone
        mov [i_rst+2], al
        or word [i_have], H_RST2
        jmp .rdone
.rnext:
        call k_flush
        dec byte [tries]
        jnz .rst
.rdone:
        ; valores por omisión (y sin enviar datos). Si esto se acepta hay
        ; dispositivo, aunque el reinicio no haya ido como debe.
        mov al, 0xF6
        call m_send
        jc .fail
        cmp byte [o_simple], 0
        jne .conf
        ; ¿IntelliMouse? Frecuencias 200, 100, 80 y pedir el identificador
        mov ax, 0xC8F3
        call m_send2
        jc .syn
        mov ax, 0x64F3
        call m_send2
        jc .syn
        mov ax, 0x50F3
        call m_send2
        jc .syn
        mov al, 0xF2
        call m_send
        jc .syn
        mov cx, 10
        call m_read
        jc .syn
        mov [i_id], al
        or word [i_have], H_ID
        cmp al, 3
        jne .syn
        mov byte [pk_len], 4
        mov byte [i_dev], 2
.syn:
        ; ¿touchpad Synaptics? Cuatro «resolución 0» y petición de estado
        mov byte [tries], 4
.s1:
        mov ax, 0x00E8
        call m_send2
        jc .conf
        dec byte [tries]
        jnz .s1
        mov al, 0xE9
        call m_send
        jc .conf
        mov cx, 10
        call m_read
        jc .conf
        mov [i_syn], al
        mov cx, 10
        call m_read
        jc .conf
        mov [i_syn+1], al
        mov cx, 10
        call m_read
        jc .conf
        mov [i_syn+2], al
        or word [i_have], H_SYN
.conf:
        mov ax, 0x64F3              ; 100 paquetes por segundo
        call m_send2
        mov ax, 0x02E8              ; 4 puntos por milímetro
        call m_send2
        mov al, 0xE6                ; escala 1:1
        call m_send
        call set_dev
        ; nuestra IRQ 12
        mov ax, 0x3574
        int 0x21
        mov [old74], bx
        mov [old74+2], es
        mov dx, irq12
        mov ax, 0x2574
        int 0x21
        call k_flush
        cmp byte [ctr_set], 0
        je .ae
        mov al, [i_ctr1]            ; el mismo, con el teclado en marcha
        call k_setctr
.ae:
        call kbd_on
        cli
        mov byte [pk_n], 0
        in al, 0x21                 ; cascada del segundo PIC abierta
        and al, 0xFB
        out 0x21, al
        in al, 0xA1                 ; IRQ 12 abierta
        and al, 0xEF
        out 0xA1, al
        sti
        ; activar. El FAh ya llega por la IRQ 12: así se sabe que funciona
        mov byte [i_dirres], 4
        mov byte [tries], 3
.en:
        mov byte [ack_wait], 1
        mov al, 0xD4
        call k_cmd
        jc .enr
        mov al, 0xF4
        call k_dat
        jc .enr
        mov cx, 10
        call tmo_set
.ew:
        cmp byte [ack_wait], 0
        je .eok
        call tmo_chk
        jnc .ew
        ; no ha llegado por la IRQ 12: ¿está esperando en el controlador?
        cli
        in al, 0x64
        and al, 0x21
        cmp al, 0x21
        jne .enx
        in al, 0x60
        sti
        cmp al, 0xFA
        jne .enr
        or word [i_have], H_NOIRQ   ; contesta, pero sin interrupción
        jmp .eok
.enx:
        sti
.enr:
        dec byte [tries]
        jnz .en
        ; no se deja activar: deshacer
        mov byte [ack_wait], 0
        cli
        in al, 0xA1
        and al, 0xEF
        mov ah, [pic2]
        and ah, 0x10
        or al, ah
        out 0xA1, al
        sti
        push ds
        lds dx, [old74]
        mov ax, 0x2574
        int 0x21
        pop ds
        cmp byte [ctr_set], 0
        je .unr
        mov al, [ctr_raw]
        and al, 0xEF
        call k_setctr
        mov al, [i_ctr0]
        mov [i_ctr1], al
.unr:
        stc
        ret
.eok:
        mov byte [ack_wait], 0
        mov byte [i_dirres], 0
        mov byte [i_via], 2
        clc
        ret
.fail:
        ; dejarlo todo como estaba
        cmp byte [ctr_set], 0
        je .f1
        mov al, [ctr_raw]
        call k_setctr
        mov al, [i_ctr0]
        mov [i_ctr1], al
.f1:
        call kbd_on
        cli
        in al, 0xA1
        and al, 0xEF
        mov ah, [pic2]
        and ah, 0x10
        or al, ah
        out 0xA1, al
        sti
        stc
        ret

; volver a poner en marcha el teclado (AEh): pase lo que pase se envía
kbd_on:
        call k_wait_in
        mov al, 0xAE
        out 0x64, al
        call k_wait_in
        ret

; -----------------------------------------------------------------------------
;  Mensajes
; -----------------------------------------------------------------------------
report:
        cmp byte [i_via], 1
        jne .n1
        mov si, msg_vbios
        call puts
        jmp .dev
.n1:
        cmp byte [i_via], 2
        jne .none
        mov si, msg_vdir
        call puts
.dev:
        call pdev
        mov si, msg_ready
        cmp byte [i_dev], 3
        jne .p
        mov si, msg_readyt
.p:
        call puts
        test word [i_have], H_NOIRQ
        jz .r
        mov si, msg_noirq
        call puts
.r:
        ret
.none:
        test word [i_have], H_BIOS
        jz .nb
        mov si, msg_nobios
        call puts
        mov al, [i_biosstep]
        call phex
        mov si, msg_nobios2
        call puts
        mov al, [i_biosah]
        call phex
        mov si, msg_hdot
        call puts
.nb:
        test word [i_have], H_DIR
        jz .nd
        mov si, msg_nodir
        call puts
        call pdirres
        mov si, msg_dot
        call puts
.nd:
        mov si, msg_nops2
        call puts
        ret

pdirres:
        movzx bx, byte [i_dirres]
        cmp bx, 4
        jbe .ok
        xor bx, bx
.ok:
        shl bx, 1
        mov si, [dirtab+bx]
        call puts
        ret
dirtab      dw msg_d0, msg_d1, msg_d2, msg_d3, msg_d4

; nombre del dispositivo
pdev:
        cmp byte [i_dev], 2
        je .im
        cmp byte [i_dev], 3
        je .syn
        mov si, msg_dps2
        call puts
        ret
.im:
        mov si, msg_dim
        call puts
        ret
.syn:
        mov si, msg_dsyn
        call puts
        movzx ax, byte [i_syn+2]    ; versión: mayor.menor
        and al, 0x0F
        call pdec
        mov al, '.'
        call pc
        movzx ax, byte [i_syn]
        call pdec
        ret

; diagnóstico (/INFO)
show_info:
        mov si, mi_head
        call puts
        ; BIOS
        mov si, mi_bios
        call puts
        mov si, mi_si
        cmp byte [i_eq], 0
        jne .eq
        mov si, mi_no
.eq:
        call puts
        mov si, mi_c2
        call puts
        test word [i_have], H_BIOS
        jnz .b1
        mov si, mi_untried
        call puts
        jmp .b9
.b1:
        cmp byte [i_biosstep], 0xFF
        jne .b2
        mov si, mi_good
        call puts
        jmp .b9
.b2:
        mov si, mi_failc2
        call puts
        mov al, [i_biosstep]
        call phex
        mov si, mi_ah
        call puts
        mov al, [i_biosah]
        call phex
        mov al, 'h'
        call pc
.b9:
        call crlf
        ; 8042
        mov si, mi_8042
        call puts
        mov al, [i_st64]
        call phex
        mov si, mi_ctr
        call puts
        mov al, [i_ctr0]
        mov dx, H_CTR0
        call pval
        mov si, mi_arrow
        call puts
        mov al, [i_ctr1]
        mov dx, H_CTR1
        call pval
        mov si, mi_auxt
        call puts
        mov al, [i_auxt]
        mov dx, H_AUXT
        call pval
        call crlf
        ; vía directa
        mov si, mi_dir
        call puts
        test word [i_have], H_DIR
        jnz .d1
        mov si, mi_untried
        call puts
        jmp .d9
.d1:
        call pdirres
.d9:
        call crlf
        mov si, mi_rst
        call puts
        mov al, [i_rst]
        mov dx, H_RST0
        call pval
        mov al, ' '
        call pc
        mov al, [i_rst+1]
        mov dx, H_RST1
        call pval
        mov al, ' '
        call pc
        mov al, [i_rst+2]
        mov dx, H_RST2
        call pval
        mov si, mi_id
        call puts
        mov al, [i_id]
        mov dx, H_ID
        call pval
        mov si, mi_syn
        call puts
        mov al, [i_syn]
        mov dx, H_SYN
        call pval
        mov al, ' '
        call pc
        mov al, [i_syn+1]
        mov dx, H_SYN
        call pval
        mov al, ' '
        call pc
        mov al, [i_syn+2]
        mov dx, H_SYN
        call pval
        call crlf
        ; PIC y vector
        mov si, mi_pic
        call puts
        in al, 0x21
        call phex
        mov si, mi_pic2
        call puts
        in al, 0xA1
        call phex
        mov si, mi_v74
        call puts
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x74*4+2]
        mov bx, [es:0x74*4]
        pop es
        push bx
        push ax
        mov al, ah
        call phex
        pop ax
        call phex
        mov al, ':'
        call pc
        pop ax
        push ax
        mov al, ah
        call phex
        pop ax
        call phex
        call crlf
        ; en uso
        mov si, mi_use
        call puts
        mov si, mi_vnone
        cmp byte [i_via], 1
        jb .u
        mov si, mi_vbios
        je .u
        mov si, mi_vdir
.u:
        call puts
        cmp byte [i_via], 0
        je .u9
        call pdev
        mov si, mi_p3
        cmp byte [i_dev], 2
        jne .u2
        mov si, mi_p4
.u2:
        call puts
        test word [i_have], H_NOIRQ
        jz .u9
        mov si, mi_noirq
        call puts
.u9:
        call crlf
        ; contadores
        mov si, mi_c1
        call puts
        mov ax, [c_irq]
        call pdec
        mov si, mi_c2b
        call puts
        mov ax, [c_bytes]
        call pdec
        mov si, mi_c3
        call puts
        mov ax, [c_pkts]
        call pdec
        mov si, mi_c4
        call puts
        mov ax, [c_bad]
        call pdec
        mov si, mi_c5
        call puts
        mov ax, [c_kbd]
        call pdec
        mov si, mi_c6
        call puts
        mov ax, [c_rst]
        call pdec
        call crlf
        mov si, mi_foot
        call puts
        ret

; AL en hexadecimal si el dato vale (DX = su bit en i_have); si no, «--»
pval:
        test [i_have], dx
        jnz phex
        mov al, '-'
        call pc
        mov al, '-'
        jmp pc
phex:
        push ax
        shr al, 4
        call .n
        pop ax
        and al, 0x0F
.n:
        add al, '0'
        cmp al, '9'
        jbe pc
        add al, 7
pc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret
pdec:
        push bx
        push cx
        push dx
        xor cx, cx
        mov bx, 10
.d:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d
.p:
        pop ax
        add al, '0'
        call pc
        loop .p
        pop dx
        pop cx
        pop bx
        ret
crlf:
        mov al, 13
        call pc
        mov al, 10
        jmp pc

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
msg_vbios   db "Vía BIOS (INT 15h). ", 0
msg_vdir    db "Vía directa (8042, IRQ 12). ", 0
msg_dps2    db "Ratón PS/2", 0
msg_dim     db "Ratón PS/2 con rueda (IntelliMouse)", 0
msg_dsyn    db "Touchpad Synaptics ", 0
msg_ready   db " preparado.", 13, 10, 0
msg_readyt  db " preparado (funciona como ratón PS/2).", 13, 10, 0
msg_noirq   db "Aviso: el dispositivo contesta, pero su interrupción (IRQ 12) no llega.", 13, 10, 0
msg_nobios  db "La BIOS no ofrece ratón PS/2 (INT 15h C2", 0
msg_nobios2 db "h, AH=", 0
msg_hdot    db "h).", 13, 10, 0
msg_nodir   db "Vía directa: ", 0
msg_dot     db ".", 13, 10, 0
msg_d0      db "bien", 0
msg_d1      db "este PC no tiene controlador 8042", 0
msg_d2      db "el controlador 8042 no responde", 0
msg_d3      db "nada contesta en el puerto auxiliar del 8042", 0
msg_d4      db "el dispositivo no se deja activar", 0
msg_nops2   db "No hay ratón PS/2: se usará un ratón USB (HSPUSB.SYS) si lo hay.", 13, 10, 0
msg_already db "HSPRATON ya está instalado.", 13, 10, 0
msg_usage   db "Uso: HSPRATON [/BIOS | /DIRECTO] [/SIMPLE] [/INFO]", 13, 10
            db "  (sin opciones) prueba la BIOS y, si no ofrece ratón, el 8042 directamente", 13, 10
            db "  /BIOS     solo por la BIOS (INT 15h)", 13, 10
            db "  /DIRECTO  solo directamente por el controlador 8042 (IRQ 12)", 13, 10
            db "  /SIMPLE   sin sondeos (rueda, Synaptics): solo reinicio y activación", 13, 10
            db "  /INFO     diagnóstico en pantalla (también con el driver ya cargado)", 13, 10, 0
mi_head     db "--- HSPRATON /INFO ---", 13, 10, 0
mi_bios     db "BIOS:    INT 11h ratón=", 0
mi_si       db "sí", 0
mi_no       db "no", 0
mi_c2       db "  INT 15h C2xxh: ", 0
mi_untried  db "no probada", 0
mi_good     db "bien", 0
mi_failc2   db "fallo en C2", 0
mi_ah       db "h, AH=", 0
mi_8042     db "8042:    estado=", 0
mi_ctr      db "  órdenes=", 0
mi_arrow    db " y luego ", 0
mi_auxt     db "  prueba aux=", 0
mi_dir      db "Directa: ", 0
mi_rst      db "         reinicio=", 0
mi_id       db "  id=", 0
mi_syn      db "  identif=", 0
mi_pic      db "PIC:     21h=", 0
mi_pic2     db "  A1h=", 0
mi_v74      db "  INT 74h=", 0
mi_use      db "En uso:  ", 0
mi_vnone    db "ningún ratón PS/2", 0
mi_vbios    db "vía BIOS, ", 0
mi_vdir     db "vía directa, ", 0
mi_p3       db ", paquetes de 3 bytes", 0
mi_p4       db ", paquetes de 4 bytes", 0
mi_noirq    db ", SIN IRQ 12", 0
mi_c1       db "Cuenta:  IRQ12=", 0
mi_c2b      db "  bytes=", 0
mi_c3       db "  paquetes=", 0
mi_c4       db "  descartes=", 0
mi_c5       db "  teclado=", 0
mi_c6       db "  reinicios=", 0
mi_foot     db "----------------------", 13, 10, 0
