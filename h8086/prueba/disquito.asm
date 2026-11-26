; =============================================================================
;  DISQUITO.SYS - un controlador de bloque «de quitar y poner», para la prueba
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Hecho con lo que cuentan los manuales de MS-DOS sobre los controladores
;  de dispositivo (la cabecera, las peticiones y sus campos). Lleva dentro
;  dos discos de 20 K (el Makefile se los pega detrás, ya formateados) y hace
;  como una disquetera: un disco puesto, que se cambia por el otro con una
;  escritura de control (INT 21h/4405h, «1» o «2»). Entonces, a la pregunta
;  «¿han cambiado el disco?» (orden 1) contesta que sí, y a «sus medidas»
;  (orden 2) da las del nuevo. Atiende también:
;       3  lectura de control: «DISQUITO: disco N, preguntas P, medidas M»
;       12 escritura de control: cambia el disco
;       15 ¿se puede quitar? (sí)
;       19 la IOCTL genérica: en el primer octeto de los datos, el disco
;       13, 14 abrir y cerrar (no hace nada)
; =============================================================================
        cpu 8086
        org 0
SECTORES    equ 40                  ; los de cada disco (de 512 octetos)

        dd -1
        dw 0x4840                   ; de bloque: IOCTL, quitar y poner, genérica
        dw estrategia
        dw interrupcion
        db 1, "DISQUIT"
peticion    dd 0
disco       db 0                    ; el puesto (0 o 1)
cambiado    db 0
preguntas   dw 0
medidas     dw 0
bpbs        dw imagenes + 11        ; (la BPB, la del primer sector del disco)

estrategia:
        mov [cs:peticion], bx
        mov [cs:peticion+2], es
        retf
interrupcion:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push ds
        push es
        push cs
        pop ds
        cld
        les bx, [peticion]
        mov word [es:bx+3], 0x0100
        mov al, [es:bx+2]
        cmp al, 0
        je iniciar
        cmp al, 1
        je pregunta
        cmp al, 2
        je dame_bpb
        cmp al, 3
        je control_lee
        cmp al, 4
        je leer
        cmp al, 8
        je escribir
        cmp al, 9
        je escribir
        cmp al, 12
        je control_escribe
        cmp al, 19
        je generica
fin:
        pop es
        pop ds
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        retf

iniciar:
        mov byte [es:bx+13], 1      ; una unidad
        mov word [es:bx+14], final  ; me quedo con los dos discos
        mov [es:bx+16], cs
        mov word [es:bx+18], bpbs
        mov [es:bx+20], cs
        mov al, [es:bx+22]
        add al, 'A'
        mov [m_letra], al
        mov dx, m_hola
        mov ah, 9
        int 0x21
        jmp fin

pregunta:                           ; 1: ¿han cambiado el disco?
        inc word [preguntas]
        mov al, 1                   ; el mismo
        cmp byte [cambiado], 0
        je .x
        mov al, 0xFF                ; otro
        mov byte [cambiado], 0
.x:
        mov [es:bx+14], al
        jmp fin

dame_bpb:                           ; 2: las medidas del disco puesto
        inc word [medidas]
        call base
        mov ax, si
        add ax, 11
        mov [es:bx+18], ax
        mov [es:bx+20], cs
        mov [bpbs], ax
        jmp fin

; SI = el principio del disco puesto (CS:SI)
base:
        mov si, imagenes
        cmp byte [disco], 0
        je .x
        add si, SECTORES * 512
.x:
        ret

; DX:CX -> ES:DI / DS:SI de los sectores pedidos (CX = cuántos octetos)
sectores:
        mov ax, [es:bx+20]          ; el primero
        mov cx, [es:bx+18]          ; cuántos
        mov dx, ax
        add dx, cx
        cmp dx, SECTORES
        ja .mal
        mov dx, 512
        mul dx
        call base
        add si, ax
        mov ax, cx
        mov dx, 512
        mul dx
        mov cx, ax
        clc
        ret
.mal:
        mov word [es:bx+18], 0
        mov word [es:bx+3], 0x8108  ; sector no encontrado
        stc
        ret

leer:
        call sectores
        jc .x
        les di, [es:bx+14]
        rep movsb
.x:
        jmp fin

escribir:
        call sectores
        jc .x
        push ds
        mov di, si
        lds si, [es:bx+14]
        push cs
        pop es
        rep movsb
        pop ds
.x:
        jmp fin

control_lee:                        ; 3: el texto, con los números
        mov al, [disco]
        add al, '1'
        mov [m_disco], al
        mov ax, [preguntas]
        mov di, m_preg
        call cifras
        mov ax, [medidas]
        mov di, m_medidas
        call cifras
        mov cx, [es:bx+18]
        cmp cx, m_estado_fin - m_estado
        jbe .c
        mov cx, m_estado_fin - m_estado
.c:
        mov [es:bx+18], cx
        les di, [es:bx+14]
        mov si, m_estado
        rep movsb
        jmp fin
; AX (hasta 999) en tres cifras en DI
cifras:
        mov cl, 100
        div cl
        add al, '0'
        mov [di], al
        mov al, ah
        aam
        add ax, '00'
        mov [di+1], ah
        mov [di+2], al
        ret

control_escribe:                    ; 12: «1» o «2», el disco que se pone
        push ds
        lds si, [es:bx+14]
        mov al, [si]
        pop ds
        sub al, '1'
        cmp al, 1
        ja .x
        cmp al, [disco]
        je .x
        mov [disco], al
        mov byte [cambiado], 1
.x:
        jmp fin

generica:                           ; 19: el disco puesto, en los datos
        les di, [es:bx+19]
        mov al, [disco]
        add al, '1'
        stosb
        jmp fin

m_hola      db "DISQUITO: dos discos de 20 K en la unidad "
m_letra     db "?:", 13, 10, "$"
m_estado    db "DISQUITO: disco "
m_disco     db "1, preguntas "
m_preg      db "000, medidas "
m_medidas   db "000"
m_estado_fin:
            align 16
imagenes:                           ; (aquí, los dos discos)
final       equ imagenes + 2 * SECTORES * 512
