; =============================================================================
;  Hispa-DOS CONFRED.COM (IPCONFIG) - la configuración de la red
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  CONFRED              la enseña (si todavía no hay, la pide: DHCP o la de
;                       \HISPADOS\RED.INI)
;  CONFRED /RENOVAR     pedirla otra vez por DHCP (/RENEW)
;  CONFRED /LIBERAR     olvidarla (/RELEASE): la próxima orden de red la pide
;  Necesita HSPRED (u otro packet driver). Ver docs/RED.md.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        cld
        mov di, red_bss
        mov cx, bss_fin - red_bss
        xor al, al
        rep stosb
        ; opciones
        mov si, 0x81
.o:
        lodsb
        cmp al, ' '
        je .o
        cmp al, 9
        je .o
        cmp al, 13
        je .go
        cmp al, '/'
        jne uso
        lodsb
        cmp al, '?'
        je ayuda
        call upcase
        cmp al, 'R'
        je .r
        cmp al, 'L'
        je .lib
        cmp al, 'A'                 ; /ALL (como en Windows): lo mismo
        je .salta
        jmp uso
.r:
        ; /RENOVAR, /RENEW o /RELEASE (en inglés)
        lodsb
        call upcase
        cmp al, 'E'
        jne uso
        lodsb
        call upcase
        cmp al, 'N'
        je .ren
        cmp al, 'L'
        jne uso
.lib:
        mov byte [o_liberar], 1
        jmp .salta
.ren:
        mov byte [o_renovar], 1
.salta:
        lodsb
        cmp al, ' '
        je .o
        cmp al, 13
        je .go
        cmp al, '/'
        je .sl
        jmp .salta
.sl:
        dec si
        jmp .o
.go:
        call red_abrir
        jc error
        cmp byte [o_liberar], 0
        je .nl
        call red_olvidar
        mov si, m_liberada
        call puts
        call crlf
        xor al, al
        jmp red_salir
.nl:
        mov al, [o_renovar]
        test al, al
        jz .cf
        mov si, m_pidiendo
        call puts
.cf:
        call red_config
        jc error
        ; enseñarla
        mov si, m_titulo
        call puts
        mov si, m_tarjeta
        call puts
        mov si, r_nombre
        call puts
        mov si, m_int
        call puts
        mov al, [r_vec]
        call hex2
        mov al, 'h'
        call putc
        mov al, ')'
        call putc
        call crlf
        mov si, m_mac
        call puts
        mov si, my_mac
        mov cx, 6
.m:
        lodsb
        call hex2
        dec cx
        jz .me
        mov al, ':'
        call putc
        jmp .m
.me:
        call crlf
        mov si, m_ip
        mov eax, [my_ip]
        call linea_ip
        mov si, m_mascara
        mov eax, [my_mask]
        call linea_ip
        mov si, m_puerta
        mov eax, [my_gw]
        call linea_ip
        mov si, m_dns
        mov eax, [my_dns]
        call linea_ip
        mov si, m_origen
        call puts
        test byte [my_cfg], 2
        jnz .dhcp
        mov si, m_amano
        call puts
        call crlf
        jmp .fin
.dhcp:
        mov si, m_pordhcp
        call puts
        mov eax, [my_cfg+18]
        call ip_poner
        mov eax, [my_cfg+22]
        test eax, eax
        jz .nl2
        mov si, m_concesion
        call puts
        call putnum
        mov si, m_segundos
        call puts
.nl2:
        mov al, ')'
        call putc
        call crlf
        mov si, m_fecha
        call puts
        movzx eax, byte [my_cfg+26]
        call putnum
        mov al, '/'
        call putc
        movzx eax, byte [my_cfg+27]
        call putnum
        mov al, '/'
        call putc
        movzx eax, word [my_cfg+28]
        call putnum2
        call crlf
.fin:
        xor al, al
        jmp red_salir

; SI = rótulo, EAX = IP («-» si es 0)
linea_ip:
        call puts
        test eax, eax
        jz .no
        call ip_poner
        jmp crlf
.no:
        mov al, '-'
        call putc
        jmp crlf

; EAX sin separador de miles (el año)
putnum2:
        push eax
        push ebx
        push edx
        push cx
        mov ebx, 10
        xor cx, cx
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop cx
        pop edx
        pop ebx
        pop eax
        ret

hex2:
        push ax
        shr al, 4
        call .d
        pop ax
        push ax
        and al, 15
        call .d
        pop ax
        ret
.d:
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp putc

error:
        call puts
        call crlf
        mov al, 1
        jmp red_salir

uso:
        mov si, m_uso
        call puts
        mov ax, 0x4C01
        int 0x21
ayuda:
        mov si, m_uso
        call puts
        mov ax, 0x4C00
        int 0x21

%include "util.inc"
%include "red.inc"

m_uso       db "Muestra y renueva la configuración de la red (IP, máscara, puerta, DNS).", 13, 10
            db 13, 10
            db "CONFRED (IPCONFIG) [/RENOVAR | /LIBERAR]", 13, 10
            db 13, 10
            db "  (sin nada)  la enseña; si todavía no hay, la pide (DHCP o RED.INI)", 13, 10
            db "  /RENOVAR    pedirla otra vez al servidor DHCP (también /RENEW)", 13, 10
            db "  /LIBERAR    olvidarla (también /RELEASE)", 13, 10
            db 13, 10
            db "La configuración está en \HISPADOS\RED.INI de la unidad de arranque:", 13, 10
            db "DHCP=SI (lo normal) o DHCP=NO con IP=, MASCARA=, PUERTA= y DNS=.", 13, 10
            db "Hace falta el controlador de la tarjeta: HSPRED (u otro packet driver).", 13, 10
            db "Otras órdenes de red: PING y DESCARGAR.", 13, 10, 0
m_pidiendo  db "Pidiendo la configuración por DHCP...", 13, 10, 0
m_liberada  db "Configuración olvidada (la próxima orden de red la pedirá otra vez).", 0
m_titulo    db 13, 10, "Configuración de la red", 13, 10, 13, 10, 0
m_tarjeta   db "  Tarjeta . . . . . . . : ", 0
m_int       db " (INT ", 0
m_mac       db "  Dirección MAC . . . . : ", 0
m_ip        db "  Dirección IP  . . . . : ", 0
m_mascara   db "  Máscara de subred . . : ", 0
m_puerta    db "  Puerta de enlace  . . : ", 0
m_dns       db "  Servidor DNS  . . . . : ", 0
m_origen    db "  Obtenida  . . . . . . : ", 0
m_amano     db "a mano (RED.INI)", 0
m_pordhcp   db "por DHCP (servidor ", 0
m_concesion db ", concesión de ", 0
m_segundos  db " s", 0
m_fecha     db "  Fecha . . . . . . . . : ", 0

        section .bss
o_renovar   resb 1
o_liberar   resb 1
u_numbuf    resb 16
u_lbuf      resb 84
bss_fin:
