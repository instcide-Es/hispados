; =============================================================================
;  PASAR.COM - pasa archivos por el puerto serie (XMODEM)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Lo que más se echa de menos al restaurar un PC antiguo es meter y sacar
;  archivos (docs/8086/ESTUDIO.md). PASAR lo hace por el puerto serie, con un
;  cable cruzado («null modem») hasta otro ordenador, antiguo o moderno (con
;  un adaptador de USB a serie), y con el protocolo XMODEM, que tiene
;  cualquier programa de terminal: Tera Term, minicom, HyperTerminal, o
;  «sx» y «rx» en Linux. En el otro lado no hace falta nada nuestro.
;
;       PASAR ENVIAR archivo     lo manda (el otro lado, «recibir XMODEM»)
;       PASAR RECIBIR archivo    lo recibe (el otro lado, «enviar XMODEM»)
;  Opciones:  /2          por COM2 (si no, COM1)
;             /V:número   la velocidad: de 300 a 115200 (si no, 9600)
;  Siempre 8 bits, sin paridad, 1 bit de parada. Esc lo deja.
;
;  XMODEM manda el archivo en bloques de 128 octetos, cada uno con su número y
;  su suma de comprobación (un CRC de 16 bits si el que recibe lo pide con
;  una «C»; si no, una suma de 8); el que recibe contesta «bien» o «otra
;  vez». El último bloque se rellena con el octeto 1Ah, que al recibir se
;  quita del final: un archivo que acabe de verdad en 1Ah los pierde.
;
;  El puerto se maneja directamente (el chip 8250) y sin interrupciones: en
;  XMODEM solo habla uno cada vez. Los plazos se miden con el reloj de la
;  BIOS (18,2 tics por segundo). Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

SOH         equ 0x01
EOT         equ 0x04
ACK         equ 0x06
NAK         equ 0x15
CAN         equ 0x18
RELLENO     equ 0x1A
INTENTOS    equ 10
SEGUNDO         equ 18                  ; tics de un segundo

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro
%macro DI_TXT 1
        mov dx, %1
        DOS 9
%endmacro

inicio:
        cld
        ; ---- lo que se pide ----
        mov si, 0x81
        call palabra                ; ENVIAR o RECIBIR (vale la primera letra)
        mov al, [pal]
        and al, 0xDF
        mov [modo], al
        cmp al, 'E'
        je .m
        cmp al, 'R'
        je .m
        jmp uso
.m:
        mov word [vel], 9600
        mov word [vel+2], 0
        mov word [base], 0x3F8
.arg:
        call palabra
        cmp byte [pal], 0
        je .listo
        cmp byte [pal], '/'
        je .op
        cmp byte [nombre], 0        ; el archivo (el primero que no es opción)
        jne .arg
        push si
        mov si, pal
        mov di, nombre
.cn:
        lodsb
        stosb
        test al, al
        jnz .cn
        pop si
        jmp .arg
.op:
        mov al, [pal+1]
        cmp al, '2'
        jne .op1
        mov word [base], 0x2F8
        jmp .arg
.op1:
        and al, 0xDF
        cmp al, 'V'
        je .vel
        jmp uso
.vel:
        push si                     ; /V:9600
        mov si, pal + 2
        cmp byte [si], ':'
        jne .v1
        inc si
.v1:
        xor ax, ax
        xor dx, dx
.v2:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .v3
        push bx                     ; DX:AX por 10, más la cifra
        mov cx, dx
        mov bx, 10
        mul bx
        push ax
        mov ax, cx
        mov cx, dx
        mul bx
        add cx, ax
        pop ax
        mov dx, cx
        pop bx
        xor bh, bh
        add ax, bx
        adc dx, 0
        inc si
        jmp .v2
.v3:
        pop si
        mov [vel], ax
        mov [vel+2], dx
        jmp .arg
.listo:
        cmp byte [nombre], 0
        jne .hay
        jmp uso
.hay:
        ; el divisor del chip: 115200 entre la velocidad
        mov cx, [vel]
        mov ax, [vel+2]
        test ax, ax
        jz .d1
        cmp ax, 1                   ; (solo 115200 pasa de 65535)
        jne .mal_vel
        cmp cx, 0xC200
        jne .mal_vel
        mov ax, 1
        jmp short .d2
.d1:
        cmp cx, 300
        jb .mal_vel
        mov dx, 1                   ; 115200 = 0001:C200h
        mov ax, 0xC200
        div cx
.d2:
        mov [divisor], ax
        jmp short .puerto
.mal_vel:
        DI_TXT m_e_vel
        jmp fin_mal
.puerto:
        mov dx, [base]              ; 8 bits, sin paridad, 1 de parada
        add dx, 3
        mov al, 0x80
        out dx, al
        mov dx, [base]
        mov ax, [divisor]
        out dx, al
        inc dx
        mov al, ah
        out dx, al
        add dx, 2
        mov al, 0x03
        out dx, al
        inc dx                      ; DTR y RTS puestos
        out dx, al
        mov dx, [base]
        inc dx
        xor al, al                  ; sin interrupciones
        out dx, al
        call vacia
        cmp byte [modo], 'E'
        jne recibir
        jmp enviar

; =============================================================================
;  Recibir
; =============================================================================
recibir:
        mov dx, nombre
        xor cx, cx
        DOS 0x3C
        jnc .creado
        DI_TXT m_e_crear
        jmp fin_mal
.creado:
        mov [arch], ax
        DI_TXT m_espera_r
        mov byte [con_crc], 1
        mov byte [bloque], 1
        mov byte [hay_ant], 0
        mov word [veces], 0
.pide:
        ; se pide el primer bloque: con «C» (CRC) cuatro veces; luego, con NAK
        ; (suma sencilla), por si el otro lado es de los antiguos
        mov al, 'C'
        cmp byte [con_crc], 0
        jne .p1
        mov al, NAK
.p1:
        call pon
        mov cx, 3 * SEGUNDO
        call lee
        jnc .cabecera
        inc word [veces]
        cmp word [veces], 4
        jne .p2
        mov byte [con_crc], 0
.p2:
        cmp word [veces], 4 + INTENTOS
        jb .pide
        jmp r_nadie
.otro:
        mov cx, 10 * SEGUNDO
        call lee
        jnc .cabecera
        jmp r_nadie
.cabecera:
        cmp al, EOT
        jne .c1
        jmp .fin
.c1:
        cmp al, CAN
        jne .c2
        jmp r_cancela
.c2:
        cmp al, SOH
        jne .otro                   ; (ruido: a por el siguiente)
        ; un bloque: número, su complemento, 128 octetos y la suma
        mov cx, SEGUNDO
        call lee
        jc .repite
        mov [b_num], al
        mov cx, SEGUNDO
        call lee
        jc .repite
        mov [b_neg], al
        mov di, datos
        mov word [resto], 128
.d:
        mov cx, SEGUNDO
        call lee
        jc .repite
        stosb
        dec word [resto]
        jnz .d
        mov cx, SEGUNDO
        call lee
        jc .repite
        mov bh, al
        cmp byte [con_crc], 0
        je .suma
        mov cx, SEGUNDO
        call lee
        jc .repite
        mov bl, al                  ; BX = el CRC recibido
        call crc16
        cmp ax, bx
        jne .repite
        jmp short .numero
.suma:
        call suma8
        cmp al, bh
        jne .repite
.numero:
        mov al, [b_num]
        mov ah, al
        not ah
        cmp ah, [b_neg]
        jne .repite
        cmp al, [bloque]
        je .nuevo
        mov ah, [bloque]            ; ¿el de antes otra vez? (se perdió nuestro
        dec ah                      ; «bien»): se da por bueno y se sigue
        cmp al, ah
        je .bien
        mov al, CAN                 ; uno que no toca: no hay arreglo
        call pon
        call pon
        jmp r_cancela
.nuevo:
        ; el bloque de antes ya se puede escribir entero (el último no: hay
        ; que quitarle el relleno); este se queda esperando
        call escribe_ant
        jc r_disco
        mov si, datos
        mov di, anterior
        mov cx, 64
        rep movsw
        mov byte [hay_ant], 1
        inc byte [bloque]
        add word [total], 128
        adc word [total+2], 0
        mov al, '.'
        call pon_car
.bien:
        mov al, ACK
        call pon
        jmp .otro
.repite:
        call vacia
        mov al, NAK
        call pon
        jmp .otro
.fin:
        mov al, ACK
        call pon
        ; el último bloque, sin el relleno del final
        cmp byte [hay_ant], 0
        je .cierra
        mov cx, 128
        mov bx, anterior + 127
.q:
        cmp byte [bx], RELLENO
        jne .q1
        dec bx
        loop .q
.q1:
        sub word [total], 128
        sbb word [total+2], 0
        add [total], cx
        adc word [total+2], 0
        jcxz .cierra
        mov bx, [arch]
        mov dx, anterior
        DOS 0x40
        jc r_disco
        cmp ax, cx
        jne r_disco
.cierra:
        mov bx, [arch]
        DOS 0x3E
        DI_TXT m_recibido
        jmp fin_cuenta
r_disco:
        mov al, CAN
        call pon
        call pon
        DI_TXT m_e_disco
        jmp short r_borra
r_cancela:
        DI_TXT m_e_cancela
        jmp short r_borra
r_nadie:
        DI_TXT m_e_nadie
r_borra:
        mov bx, [arch]              ; lo que haya llegado a medias no vale
        DOS 0x3E
        mov dx, nombre
        DOS 0x41
        jmp fin_mal
; (interna) escribe el bloque anterior, si lo hay. CF=1 si falla el disco
escribe_ant:
        cmp byte [hay_ant], 0
        je .x
        mov bx, [arch]
        mov cx, 128
        mov dx, anterior
        DOS 0x40
        jc .x
        cmp ax, 128
        je .x
        stc
        ret
.x:
        ret

; =============================================================================
;  Enviar
; =============================================================================
enviar:
        mov dx, nombre
        mov al, 0
        DOS 0x3D
        jnc .abierto
        DI_TXT m_e_abrir
        jmp fin_mal
.abierto:
        mov [arch], ax
        DI_TXT m_espera_e
        ; el que recibe empieza: «C» si quiere CRC, NAK si quiere la suma
        mov word [veces], 0
.espera:
        mov cx, 6 * SEGUNDO
        call lee
        jc .e1
        cmp al, 'C'
        je .crc
        cmp al, NAK
        je .sum
        cmp al, CAN
        jne .espera
        jmp e_cancela
.e1:
        inc word [veces]
        cmp word [veces], 10        ; un minuto
        jb .espera
        jmp e_nadie
.crc:
        mov byte [con_crc], 1
        jmp short .va
.sum:
        mov byte [con_crc], 0
.va:
        mov byte [bloque], 1
.bloque:
        mov bx, [arch]              ; 128 octetos del archivo (los que falten,
        mov cx, 128                 ; de relleno)
        mov dx, datos
        DOS 0x3F
        jnc .leido
        jmp e_disco
.leido:
        test ax, ax
        jnz .l1
        jmp .eot
.l1:
        add [total], ax
        adc word [total+2], 0
        mov cx, 128
        sub cx, ax
        jcxz .manda
        mov di, datos
        add di, ax
        mov al, RELLENO
        rep stosb
.manda:
        mov word [veces], 0
.otra:
        mov al, SOH
        call pon
        mov al, [bloque]
        call pon
        not al
        call pon
        mov si, datos
        mov cx, 128
.d:
        lodsb
        call pon
        loop .d
        cmp byte [con_crc], 0
        je .s8
        call crc16
        xchg al, ah
        call pon
        mov al, ah
        call pon
        jmp short .resp
.s8:
        call suma8
        call pon
.resp:
        mov cx, 10 * SEGUNDO
        call lee
        jc .mal
        cmp al, ACK
        je .ok
        cmp al, CAN
        jne .mal
        jmp e_cancela
.mal:
        inc word [veces]
        cmp word [veces], INTENTOS
        jb .otra
        jmp e_nadie
.ok:
        inc byte [bloque]
        mov al, '.'
        call pon_car
        jmp .bloque
.eot:
        mov word [veces], 0
.e2:
        mov al, EOT
        call pon
        mov cx, 5 * SEGUNDO
        call lee
        jc .e3
        cmp al, ACK
        je .hecho
.e3:
        inc word [veces]
        cmp word [veces], 5
        jb .e2
.hecho:
        mov bx, [arch]
        DOS 0x3E
        DI_TXT m_enviado
        jmp fin_cuenta
e_disco:
        mov al, CAN
        call pon
        call pon
        DI_TXT m_e_disco
        jmp short e_cierra
e_cancela:
        DI_TXT m_e_cancela
        jmp short e_cierra
e_nadie:
        DI_TXT m_e_nadie
e_cierra:
        mov bx, [arch]
        DOS 0x3E
        jmp fin_mal

; =============================================================================
;  Comunes
; =============================================================================
; « N octetos.» y fin
fin_cuenta:
        mov ax, [total]
        mov dx, [total+2]
        call pon_num
        DI_TXT m_octetos
        mov ax, 0x4C00
        int 0x21
uso:
        DI_TXT m_uso
fin_mal:
        mov ax, 0x4C01
        int 0x21

; manda AL por el puerto. Conserva los registros
pon:
        push ax
        push dx
        mov ah, al
        mov dx, [base]
        add dx, 5
.e:
        in al, dx                   ; ¿puede salir otro?
        test al, 0x20
        jz .e
        mov dx, [base]
        mov al, ah
        out dx, al
        pop dx
        pop ax
        ret
; espera un octeto del puerto, CX tics como mucho -> AL. CF=1 si no llega (o
; se pulsa Esc, que además lo deja todo). Conserva los demás registros
lee:
        push bx
        push dx
        push es
        mov ax, 0x40
        mov es, ax
        mov bx, [es:0x6C]           ; cuándo se empezó a esperar
.e:
        mov dx, [base]
        add dx, 5
        in al, dx
        test al, 0x01
        jnz .hay
        mov ax, [es:0x6C]
        sub ax, bx
        cmp ax, cx
        jb .e1
        stc
        jmp short .x
.e1:
        test al, 3                  ; de vez en cuando, ¿han pulsado Esc?
        jnz .e
        mov ah, 1
        int 0x16
        jz .e
        xor ah, ah
        int 0x16
        cmp al, 27
        jne .e
        mov al, CAN
        call pon
        call pon
        DI_TXT m_e_esc
        cmp byte [modo], 'R'
        jne .es1
        jmp r_borra
.es1:
        jmp e_cierra
.hay:
        mov dx, [base]
        in al, dx
        clc
.x:
        pop es
        pop dx
        pop bx
        ret
; tira lo que haya llegado (medio segundo sin que llegue nada)
vacia:
        push ax
        push cx
.v:
        mov cx, SEGUNDO / 2
        call lee
        jnc .v
        pop cx
        pop ax
        ret
; el CRC de 16 bits (el de XMODEM: polinomio 1021h) de los 128 octetos de
; `datos` -> AX. Conserva los demás registros
crc16:
        push bx
        push cx
        push dx
        push si
        mov si, datos
        mov cx, 128
        xor dx, dx
.o:
        lodsb
        xor dh, al
        mov bx, 8
.b:
        shl dx, 1
        jnc .n
        xor dx, 0x1021
.n:
        dec bx
        jnz .b
        loop .o
        mov ax, dx
        pop si
        pop dx
        pop cx
        pop bx
        ret
; la suma de 8 bits de los 128 octetos de `datos` -> AL
suma8:
        push cx
        push si
        mov si, datos
        mov cx, 128
        xor ah, ah
.s:
        lodsb
        add ah, al
        loop .s
        mov al, ah
        pop si
        pop cx
        ret
; la palabra siguiente de DS:SI (hasta un espacio o el final) -> pal, acabada
; en 0; SI avanza
palabra:
        push ax
        push di
        mov di, pal
.e:
        cmp byte [si], ' '
        jne .c
        inc si
        jmp .e
.c:
        mov al, [si]
        cmp al, ' '
        jbe .f
        cmp di, pal + 78
        jae .f
        stosb
        inc si
        jmp .c
.f:
        mov byte [di], 0
        pop di
        pop ax
        ret
; escribe AL en la pantalla
pon_car:
        push ax
        push dx
        mov dl, al
        DOS 2
        pop dx
        pop ax
        ret
; escribe DX:AX en decimal
pon_num:
        push bx
        push cx
        push si
        mov bx, 10
        xor cx, cx
.d:
        push ax
        mov ax, dx
        xor dx, dx
        div bx
        mov si, ax
        pop ax
        div bx
        push dx
        inc cx
        mov dx, si
        mov si, ax
        or si, dx
        jnz .d
.c:
        pop ax
        add al, '0'
        call pon_car
        loop .c
        pop si
        pop cx
        pop bx
        ret

m_uso       db "PASAR - pasa archivos por el puerto serie (XMODEM)", 13, 10
            db "  PASAR ENVIAR archivo    lo manda", 13, 10
            db "  PASAR RECIBIR archivo   lo recibe", 13, 10
            db "Opciones: /2 por COM2 (si no, COM1); /V:9600 la velocidad", 13, 10
            db "(de 300 a 115200). 8 bits, sin paridad, 1 bit de parada.", 13, 10
            db "En el otro ordenador: cualquier programa de terminal con XMODEM.", 13, 10, "$"
m_espera_e  db "Enviando. Ponga el otro ordenador a recibir con XMODEM (Esc lo deja).", 13, 10, "$"
m_espera_r  db "Recibiendo. Ponga el otro ordenador a enviar con XMODEM (Esc lo deja).", 13, 10, "$"
m_enviado   db 13, 10, "Enviado: $"
m_recibido  db 13, 10, "Recibido: $"
m_octetos   db " octetos.", 13, 10, "$"
m_e_vel     db "Esa velocidad no vale: de 300 a 115200.", 13, 10, "$"
m_e_abrir   db "No existe ese archivo.", 13, 10, "$"
m_e_crear   db "No se puede crear el archivo.", 13, 10, "$"
m_e_disco   db 13, 10, "Error de disco: se deja.", 13, 10, "$"
m_e_nadie   db 13, 10, "El otro ordenador no contesta: se deja.", 13, 10, "$"
m_e_cancela db 13, 10, "El otro ordenador lo ha cancelado.", 13, 10, "$"
m_e_esc     db 13, 10, "Se deja (Esc).", 13, 10, "$"

modo        db 0                    ; «E» o «R»
con_crc     db 0
bloque      db 0                    ; el número del bloque que toca
hay_ant     db 0
b_num       db 0
b_neg       db 0
            align 2
base        dw 0                    ; el puerto del chip
divisor     dw 0
vel         dw 0, 0
arch        dw 0
veces       dw 0
resto       dw 0
total       dw 0, 0
nombre      times 80 db 0
pal         times 80 db 0
datos       times 128 db 0
anterior    times 128 db 0
