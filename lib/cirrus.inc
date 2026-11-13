; =============================================================================
;  cirrus.inc - [CIRRUS] los Cirrus Logic CS4206/CS4207/CS4208 de los Mac
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Lo usan HSPSND y AUDICION. Escrito desde cero con lo que se sabe
;  públicamente de estos códecs (hojas de Cirrus Logic de la familia
;  CS4206/CS4207/CS4208 y la norma HD Audio de Intel), contrastado con el
;  comportamiento de los sistemas libres en estos Mac (sin copiar código).
;
;  Lo que hace falta en un Mac (subsistema del códec 106Bxxxx):
;   - los amplificadores de los altavoces y de los auriculares van fuera del
;     códec y se encienden con patillas GPIO del propio códec; el firmware
;     de Apple los deja apagados. CS4206/CS4207: altavoces por la GPIO 3
;     (08h) y auriculares por la GPIO 1 (02h); en el MacBook Pro 10,1 y en
;     los iMac de 2011-2013, los auriculares por la GPIO 2 (04h). CS4208
;     (MacBook Air de 2013 en adelante): altavoces por la GPIO 0 (01h).
;   - el widget de proceso del fabricante (nodo 11h en CS4206/7, 24h en
;     CS4208) tiene que estar en marcha y con sus coeficientes: de él
;     dependen los convertidores. Tras un reinicio del enlace (CRST) queda
;     como al encender.
;   - CS4208 del MacBook Air 6,x: la configuración de los pines que trae no
;     dice cuál es el altavoz (12h) ni los auriculares (10h).
;
;  Quien lo incluye pone: hda_verb (BL nodo, AX verbo de 12 bits, CL dato),
;  hda_verb4 (BL nodo, AL verbo de 4 bits, CX dato) y las variables cr_kind,
;  cr_dev (dw), cr_ssid (dw), cr_gspk, cr_ghp, cr_gm y cr_vpw (db).
; =============================================================================

; EAX = identificador del códec (parámetro 00h del nodo 0), EDX = subsistema
; del grupo de audio (F20h), CL = las GPIO que tiene (máscara) -> cr_kind
; (0 no es de esos, 1 CS4206/7, 2 CS4208), cr_dev, cr_ssid (la parte baja
; del subsistema: 5600h...), cr_gspk y cr_ghp (el bit de cada amplificador)
; y cr_gm (los dos, solo los que tenga)
cr_ids:
        push eax
        push si
        mov byte [cr_kind], 0
        mov byte [cr_gspk], 0
        mov byte [cr_ghp], 0
        mov byte [cr_gm], 0
        mov [cr_dev], ax
        mov [cr_ssid], dx
        push edx
        shr edx, 16
        cmp dx, 0x106B              ; Apple
        pop edx
        jne .r
        cmp eax, 0x10134206
        je .c420x
        cmp eax, 0x10134207
        je .c420x
        cmp eax, 0x10134208
        jne .r
        mov byte [cr_kind], 2
        mov byte [cr_gspk], 0x01
        jmp .m
.c420x:
        mov byte [cr_kind], 1
        mov byte [cr_gspk], 0x08
        mov byte [cr_ghp], 0x02
        mov si, cr_gpio23
.t:
        cmp word [si], 0
        je .m
        cmp dx, [si]
        je .g23
        add si, 2
        jmp .t
.g23:
        mov byte [cr_ghp], 0x04
.m:
        mov al, [cr_gspk]
        or al, [cr_ghp]
        and al, cl
        mov [cr_gm], al
.r:
        pop si
        pop eax
        ret
; los que llevan los auriculares en la GPIO 2: MacBook Pro 10,1; iMac 12,1,
; 12,2 y 14,1
cr_gpio23   dw 0x2800, 0x0900, 0x2000, 0x0600, 0

; puesta en marcha del widget de proceso (si cr_kind lo pide) -> cr_vpw =
; su nodo (0 si no hay nada que hacer)
cr_start:
        pusha
        mov byte [cr_vpw], 0
        mov si, cr_seq420x
        mov dl, 0x11
        cmp byte [cr_kind], 1
        je .l
        mov si, cr_seq4208
        mov dl, 0x24
        cmp byte [cr_kind], 2
        jne .r
.l:
        mov [cr_vpw], dl
.n:
        mov bl, [si]
        test bl, bl
        jz .r
        mov ax, [si+2]
        mov cx, [si+4]
        cmp byte [si+1], 0
        jne .cf
        call hda_verb
        jmp .s
.cf:
        push cx
        mov cx, ax
        mov al, 0x05                ; índice del coeficiente
        call hda_verb4
        pop cx
        mov al, 0x04                ; y su valor
        call hda_verb4
.s:
        add si, 6
        jmp .n
.r:
        popa
        ret
; las secuencias: nodo, tipo (0: verbo de 12 bits y dato; 1: coeficiente,
; índice y valor) y dos palabras; nodo 0 = fin
cr_seq420x:
        db 0x01, 0
        dw 0x705, 0x00              ; grupo de audio en D0
        db 0x11, 0
        dw 0x703, 0x01              ; widget de proceso en marcha
        db 0x11, 1
        dw 0x0006, 0x9999           ; corrección de las revisiones C0-C2:
        db 0x11, 1                  ; se abre, se ajusta el 17h y el SPDIF...
        dw 0x0017, 0xA412
        db 0x11, 1
        dw 0x0001, 0x0009
        db 0x07, 0
        dw 0x705, 0x00              ; (receptor y emisor SPDIF en D0)
        db 0x08, 0
        dw 0x705, 0x00
        db 0x11, 1
        dw 0x0017, 0x2412           ; ... y se cierra
        db 0x11, 1
        dw 0x0008, 0x0000
        db 0x11, 1
        dw 0x0006, 0x0000
        db 0x11, 1
        dw 0x0003, 0x146A           ; DAC: rampa suave, silencio si falla la
                                    ; FIFO, filtro de paso alto, sin autoincremento
        db 0x11, 1
        dw 0x0002, 0x000A           ; ADC: rampas
        db 0x11, 1
        dw 0x0004, 0x0007           ; pitido por los tres DAC
        db 0
cr_seq4208:
        db 0x01, 0
        dw 0x705, 0x00
        db 0x24, 0
        dw 0x703, 0x01
        db 0x24, 1
        dw 0x0033, 0x0001
        db 0x24, 1
        dw 0x0034, 0x1C01
        db 0

; BL = pin, EAX = su configuración por defecto (F1Ch) -> EAX, la que vale
; (CS4208 del MacBook Air 6,1 y 6,2: 12h altavoz, 10h auriculares)
cr_pincfg:
        cmp byte [cr_kind], 2
        jne .r
        cmp word [cr_ssid], 0x7100
        je .m
        cmp word [cr_ssid], 0x7200
        jne .r
.m:
        cmp bl, 0x12
        jne .h
        mov eax, 0x90100110         ; fijo, interno: altavoz
        ret
.h:
        cmp bl, 0x10
        jne .r
        mov eax, 0x032B4020         ; conector: auriculares
.r:
        ret
