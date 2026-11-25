; =============================================================================
;  PLUMA.COM - el procesador de textos de Hispa-8086
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       PLUMA [/W] [documento]
;
;  Para un PC o un XT con 256 K y cualquier tarjeta de vídeo en 80 columnas
;  (MDA, Hercules, CGA, EGA, VGA). Se escribe sin pulsar Intro al final de cada
;  línea: las líneas se parten solas por los espacios. Negrita, subrayado y
;  cursiva se ven en la pantalla con brillo, subrayado e inverso (no con
;  colores); los párrafos pueden ir a la izquierda, centrados, a la derecha o
;  justificados, y con sangría. El documento puede ser mayor que la memoria
;  (lo que no cabe va al disco a trozos). Deshacer, bloques, buscar y
;  reemplazar sin mirar tildes ni mayúsculas, contar palabras, la vista de la
;  página, imprimir en Epson, IBM o en texto (con la ñ y las tildes bien), y
;  exportar a texto (de DOS o UTF-8) y a RTF. Abre texto y archivos de
;  WordStar (/W, o lo nota solo).
;
;  Las teclas: las de siempre en español (F1 la ayuda, Esc o F10 el menú) y,
;  para quien las tenga en los dedos, las de WordStar (^E ^X ^S ^D, ^K, ^Q,
;  ^O, ^P...). Todo está en docs/8086/PLUMA.md.
;
;  Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

TROZO       equ 4096                ; un trozo de los que van al disco
TROZO_BITS     equ 12
MARGEN      equ 4096                ; lo que se tiene a cada lado del cursor
UB_TAM      equ 2048                ; para deshacer
CLIP_TAM    equ 4096                ; un bloque copiado (más, va al disco)
PB_TAM      equ 2048                ; un párrafo, al imprimir
FILA0       equ 2
NFILAS      equ 22
PILA        equ 1024

CODN        equ 0x02                ; ^B negrita (los de WordStar)
CODS        equ 0x13                ; ^S subrayado
CODC        equ 0x19                ; ^Y cursiva
C_SANG      equ 0x14                ; un paso de sangría (de 5 columnas)
C_CEN       equ 0x1C                ; párrafo centrado
C_DER       equ 0x1D                ; a la derecha
C_JUS       equ 0x1E                ; justificado
C_EURO      equ 0xD5                ; € en la página 858

; la cabecera de un documento .PLU (128 octetos; ver PLUMA.md)
CAB_VER     equ 8
CAB_MIZQ    equ 9
CAB_ANCHO   equ 10
CAB_SANG1   equ 11
CAB_LARGO   equ 12
CAB_MSUP    equ 13
CAB_MINF    equ 14
CAB_PAG1    equ 15
CAB_IMPR    equ 16
CAB_HOJAS   equ 17
CAB_ENC     equ 32
CAB_PIE     equ 80
CAB_TAM     equ 128

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

inicio:
        cld
        mov ax, [2]                 ; ¿hay 64 K para el programa? (lo que
        mov bx, cs                  ; tiene, desde su segmento: se miraba el
        sub ax, bx                  ; final de la memoria, y con poca, la pila
        cmp ax, 0x1000              ; quedaba fuera de la RAM)
        jb .poca
        mov sp, 0xFFFE              ; (por si DOS no lo deja arriba del todo)
        mov word [bfin], 0xFFFE - PILA
        mov ax, [bfin]
        sub ax, BUF
        cmp ax, 4 * TROZO + 2 * MARGEN
        jae .mem
.poca:
        mov dx, m_poca
        DOS 0x09
        mov ax, 0x4C01
        int 0x21
.mem:
        mov di, bss_ini             ; las variables, a cero
        mov cx, BUF - bss_ini
        xor al, al
        rep stosb
        mov byte [insertar], 1
        mov word [bfin], 0xFFFE - PILA
        mov ax, 0x2523              ; Ctrl+C y los errores de disco, nuestros
        mov dx, int_23
        int 0x21
        mov ax, 0x2524
        mov dx, int_24
        int 0x21
        call video
        call tablas
        ; la línea de órdenes: [/W] [nombre]
        mov si, 0x81
.p:
        lodsb
        cmp al, ' '
        je .p
        cmp al, 9
        je .p
        cmp al, '/'
        jne .p1
        lodsb
        and al, 0xDF
        cmp al, 'W'
        jne .p
        mov byte [forzar_ws], 1
        jmp .p
.p1:
        cmp al, 13
        jbe .sin
        mov di, nombre
.p2:
        stosb
        lodsb
        cmp al, ' '
        ja .p2
        mov byte [di], 0
        call mayusculas_nombre
        call con_extension
.sin:
        call doc_vacio
        cmp byte [nombre], 0
        je bucle
        call carga
        mov byte [forzar_ws], 0

; ---- una tecla tras otra ----
bucle:
        call asegura
        call normaliza
        mov ah, 1                   ; si hay teclas esperando, primero ellas
        int 0x16
        jnz .t
        call pinta
.t:
        call lee_tecla
        mov word [mensaje], 0
        mov bl, [vert]
        mov [vert_ant], bl
        mov byte [vert], 0
        ; un grupo nuevo para deshacer, salvo si se siguen escribiendo letras
        mov bl, 0
        cmp al, ' '
        jb .g
        mov bl, 1
        cmp [era_letra], bl
        je .g1
.g:
        inc byte [u_grupo]
        mov byte [u_roto], 0
.g1:
        mov [era_letra], bl
        test al, al
        jz .ext
        cmp al, 0xE0
        je .ext
        cmp al, 32
        jae .letra
        mov bl, al
        mov bh, 0
        shl bx, 1
        call [t_ctrl+bx]
        jmp bucle
.letra:
        call escribe_letra
        jmp bucle
.ext:
        mov bx, t_ext
.b:
        cmp byte [bx], 0
        je bucle
        cmp ah, [bx]
        je .e
        add bx, 3
        jmp .b
.e:
        call [bx+1]
        jmp bucle

int_23:
        iret
int_24:
        mov al, 3                   ; (fallar: lo dice Pluma)
        iret

lee_tecla:
        xor ah, ah
        int 0x16
        ret

nada:
        ret

; ---- las teclas ----
%macro TECLA 2
        db %1
        dw %2
%endmacro
t_ctrl:
        dw nada, palabra_izq, reajusta, avpag             ; ^@ ^A ^B ^C
        dw derecha, arriba, palabra_der, suprime          ; ^D ^E ^F ^G
        dw retroceso, tabulador, ayuda, pre_k             ; ^H ^I ^J ^K
        dw buscar_sig, intro, intro_queda, pre_o          ; ^L ^M ^N ^O
        dw pre_p, pre_q, repag, izquierda                 ; ^P ^Q ^R ^S
        dw borra_palabra, deshacer, cambia_ins, arriba    ; ^T ^U ^V ^W
        dw abajo, borra_linea, deshacer, menu_principal   ; ^X ^Y ^Z Esc
        dw nada, nada, nada, nada
t_ext:
        TECLA 0x48, arriba
        TECLA 0x50, abajo
        TECLA 0x4B, izquierda
        TECLA 0x4D, derecha
        TECLA 0x47, inicio_linea
        TECLA 0x4F, fin_linea
        TECLA 0x49, repag
        TECLA 0x51, avpag
        TECLA 0x73, palabra_izq
        TECLA 0x74, palabra_der
        TECLA 0x77, doc_inicio
        TECLA 0x75, doc_fin
        TECLA 0x84, doc_inicio
        TECLA 0x76, doc_fin
        TECLA 0x52, cambia_ins
        TECLA 0x53, suprime
        TECLA 0x3B, ayuda           ; F1
        TECLA 0x3C, guardar         ; F2
        TECLA 0x3D, buscar          ; F3
        TECLA 0x3E, copiar          ; F4
        TECLA 0x3F, cortar          ; F5
        TECLA 0x40, pegar           ; F6
        TECLA 0x41, marcar          ; F7
        TECLA 0x42, reemplazar      ; F8
        TECLA 0x43, vista           ; F9
        TECLA 0x44, menu_principal  ; F10
        TECLA 0x31, negrita         ; Alt+N
        TECLA 0x1F, subrayado       ; Alt+S
        TECLA 0x2E, cursiva         ; Alt+C
        TECLA 0x10, quita_formato   ; Alt+Q
        TECLA 0x17, al_izq          ; Alt+I
        TECLA 0x32, al_centro       ; Alt+M
        TECLA 0x20, al_der          ; Alt+D
        TECLA 0x24, al_just         ; Alt+J
        TECLA 0x22, sangria         ; Alt+G
        TECLA 0x19, salto_pagina    ; Alt+P
        TECLA 0x14, acento          ; Alt+T
        TECLA 0x12, euro            ; Alt+E (AltGr+E en el teclado español)
        TECLA 0x2F, cambia_codigos  ; Alt+V
        TECLA 0x2D, salir           ; Alt+X
        TECLA 0x0E, deshacer        ; Alt+Retroceso
        db 0

; las de WordStar que van tras ^K, ^Q, ^O y ^P: la letra y lo que hace
t_k:    db "BKCVYHSDXQRWP", 0
        dw marca_ini, marca_fin, ws_copia, ws_mueve, cortar, desmarca
        dw guardar, guardar_salir, guardar_salir, salir_sin, inserta_archivo
        dw escribe_bloque, imprimir
t_q:    db "SDRCFAYBKEXL", 0
        dw inicio_linea, fin_linea, doc_inicio, doc_fin, buscar, reemplazar
        dw borra_fin_linea, va_marca_ini, va_marca_fin, pant_arriba, pant_abajo
        dw buscar_sig
t_o:    db "CJDLRGP", 0
        dw al_centro, al_just, cambia_codigos, pide_mizq, pide_ancho, sangria
        dw vista
t_p:    db "BSYL", 0
        dw negrita, subrayado, cursiva, salto_pagina

pre_k:
        mov si, m_pre_k
        mov bx, t_k
        jmp short prefijo
pre_q:
        mov si, m_pre_q
        mov bx, t_q
        jmp short prefijo
pre_o:
        mov si, m_pre_o
        mov bx, t_o
        jmp short prefijo
pre_p:
        mov si, m_pre_p
        mov bx, t_p
; enseña las teclas que siguen (como el menú de ayuda de WordStar) y hace la
; que se pulse (la letra sola, o con Ctrl)
prefijo:
        push bx
        call pinta_abajo
        call lee_tecla
        pop bx
        cmp al, 27
        je .x
        cmp al, 32
        jae .l
        add al, 0x40
.l:
        and al, 0xDF
        mov si, bx
        xor cx, cx
.b:
        cmp byte [si], 0
        je .x
        cmp al, [si]
        je .e
        inc si
        inc cx
        jmp .b
.e:
        inc si                      ; (el 0 del final de las letras)
.f:
        cmp byte [si-1], 0
        je .g
        inc si
        jmp .f
.g:
        shl cx, 1
        add si, cx
        jmp [si]
.x:
        ret

; =============================================================================
;  Moverse
; =============================================================================
; AL es un código de formato o de párrafo (no se ve ni ocupa)? CF=1 si lo es
es_codigo:
        cmp al, 32
        jae .no
        call de_clase
        test ah, 0xC0
        jz .no
        stc
        ret
.no:
        clc
        ret
; AL es de los que forman palabras? CF=1 si lo es
es_de_palabra:
        cmp al, '0'
        jb .no
        cmp al, '9'
        jbe .si
        cmp al, 'A'
        jb .no
        cmp al, 0xA8                ; ¿ ¡ « »
        je .no
        cmp al, 0xAD
        je .no
        cmp al, 0xAE
        je .no
        cmp al, 0xAF
        je .no
        cmp al, 0x80
        jae .si
        cmp al, 'Z'
        jbe .si
        cmp al, 'a'
        jb .no
        cmp al, 'z'
        ja .no
.si:
        stc
        ret
.no:
        clc
        ret
; la letra de antes del cursor (AL) y la de después (AH); 0 si no hay
alrededor:
        xor ax, ax
        cmp word [pre], 0
        je .a
        mov si, [g_ini]
        mov al, [si-1]
.a:
        cmp word [post], 0
        je .x
        mov si, [pstart]
        mov ah, [si]
.x:
        ret

; un octeto a la izquierda / a la derecha (CF=1 si no hay)
atras1:
        cmp word [pre], 0
        jne .s
        call trae_de_A
        jc .x
.s:
        mov ax, [pre]
        dec ax
        call mueve_hueco
        mov byte [filas_ok], 0
        clc
.x:
        ret
adelante1:
        cmp word [post], 0
        jne .s
        call trae_de_B
        jc .x
.s:
        mov ax, [pre]
        inc ax
        call mueve_hueco
        mov byte [filas_ok], 0
        clc
.x:
        ret

izquierda:
        call limpia_pareja
.c:
        call atras1
        jc .x
        mov si, [pstart]
        mov al, [si]
        call es_codigo
        jc .c                       ; (los códigos se pasan de largo)
.x:
        ret
derecha:
        call limpia_pareja
.c:
        cmp word [post], 0
        jne .h
        call trae_de_B
        jc .x
.h:
        mov si, [pstart]
        mov al, [si]
        push ax
        call adelante1
        pop ax
        jc .x
        call es_codigo
        jc .c
.x:
        ret

; el cursor no se queda delante de los códigos de un párrafo; y una pareja
; de códigos iguales sin nada dentro, cuando se deja, se quita
normaliza:
        call alrededor
        test ah, ah
        jz .x
        xchg al, ah
        call de_clase
        test ah, 0x40
        jz .x
        cmp word [pre], 0
        je .mueve
        call alrededor
        cmp al, 0x0A
        je .mueve
        cmp al, 0x0C
        je .mueve
        call de_clase
        test ah, 0x40
        jz .x
.mueve:
        call adelante1
        jnc normaliza
.x:
        ret
limpia_pareja:
        call alrededor
        cmp al, ah
        jne .x
        test al, al
        jz .x
        call de_clase
        test ah, 0x80
        jz .x
        call borra_atras
        mov cx, 1
        call borra_delante
.x:
        ret

; la columna que se quiere al subir y bajar
col_vertical:
        cmp byte [vert_ant], 0
        jne .x
        mov al, [cur_col]
        mov [col_des], al
.x:
        mov byte [vert], 1
        ret
arriba:
        call limpia_pareja
        call filas_al_dia
        call col_vertical
        mov bl, [cur_fila]
        test bl, bl
        jnz .f
        call linea_atras
        jc .x
        mov byte [filas_ok], 0
        mov byte [sin_ajuste], 1
        call filas_al_dia
        mov byte [sin_ajuste], 0
        mov bl, 1
.f:
        dec bl
        mov bh, 0
        jmp va_fila
.x:
        ret
abajo:
        call limpia_pareja
        call filas_al_dia
        call col_vertical
        mov bl, [cur_fila]
        inc bl
        cmp bl, NFILAS
        jb .f
        call linea_adelante
        jc .x
        mov byte [filas_ok], 0
        mov byte [sin_ajuste], 1
        call filas_al_dia
        mov byte [sin_ajuste], 0
        mov bl, NFILAS - 1
.f:
        mov bh, 0
        jmp va_fila
.x:
        ret
; el cursor a la fila BX, en la columna col_des (o al final, si es corta)
va_fila:
        call rehace_fila
        jc .x
        mov bl, [col_des]
        mov bh, 0
        shl bx, 1
        mov ax, [fpos+bx]
        cmp ax, 0xFFFF
        jne .pon
        mov ax, [k_ultp]            ; pasada del final: al final de verdad
        test ax, ax
        jnz .pon
.c:
        mov ax, [fpos+bx]           ; (o en la última casilla con algo)
        cmp ax, 0xFFFF
        jne .pon
        sub bx, 2
        jae .c
        mov ax, [l_p]
.pon:
        call w_de
        call mueve_hueco
        mov byte [filas_ok], 0
.x:
        ret
repag:
        call limpia_pareja
        mov dx, linea_atras
        jmp short pagina
avpag:
        call limpia_pareja
        mov dx, linea_adelante
; la pantalla entera arriba o abajo (DX), y el cursor en la misma fila
pagina:
        call filas_al_dia
        call col_vertical
        mov al, [cur_fila]
        mov [cur_fila_ant], al
        mov cx, NFILAS - 1
.c:
        push cx
        push dx
        call dx
        pop dx
        pop cx
        loop .c
        mov byte [filas_ok], 0
        mov byte [sin_ajuste], 1
        call filas_al_dia
        mov byte [sin_ajuste], 0
        mov bl, [cur_fila_ant]
        mov bh, 0
.b:
        mov di, bx                  ; (la última fila con algo)
        mov cl, 3
        shl di, cl
        cmp byte [filas+di+7], 0
        jne .v
        dec bx
        jns .b
        xor bx, bx
.v:
        jmp va_fila
pant_arriba:
        call filas_al_dia
        xor bx, bx
        jmp va_fila
pant_abajo:
        call filas_al_dia
        mov bx, NFILAS - 1
        jmp pagina.b
inicio_linea:
        call limpia_pareja
        call filas_al_dia
        mov bl, [cur_fila]
        mov bh, 0
        mov cl, 3
        shl bx, cl
        mov ax, [filas+bx]
        call mueve_hueco
        mov byte [filas_ok], 0
        ret
fin_linea:
        call limpia_pareja
        call filas_al_dia
        mov byte [col_des], 79
        mov bl, [cur_fila]
        mov bh, 0
        jmp va_fila
palabra_izq:
        call izquierda
.a:
        call alrededor
        test al, al
        jz .x
        call es_de_palabra
        jc .b
        call izquierda
        jmp .a
.b:
        call alrededor
        call es_de_palabra
        jnc .x
        call izquierda
        jmp .b
.x:
        ret
palabra_der:
.a:
        call alrededor
        test ah, ah
        jz .x
        mov al, ah
        call es_de_palabra
        jnc .b
        call derecha
        jmp .a
.b:
        call alrededor
        mov al, ah
        cmp al, ' '
        jne .x
        call derecha
        jmp .b
.x:
        ret
doc_inicio:
        call limpia_pareja
        xor ax, ax
        xor dx, dx
        jmp short va_pos
doc_fin:
        call limpia_pareja
        call total32
; el cursor a la posición DX:AX, y la pantalla a su alrededor
va_pos:
        call ir_a
        mov byte [centrar], 1
        mov byte [filas_ok], 0
        ret
va_marca_ini:
        mov ax, [b_ini]
        mov dx, [b_ini+2]
        jmp va_pos
va_marca_fin:
        mov ax, [b_fin]
        mov dx, [b_fin+2]
        jmp va_pos

; =============================================================================
;  Escribir y borrar
; =============================================================================
escribe_letra:
        cmp byte [insertar], 0
        jne .ins
        push ax                     ; sobrescribir: se quita la que hay (si
        call alrededor              ; es una letra)
        cmp ah, ' '
        jb .s
        mov cx, 1
        call borra_delante
.s:
        pop ax
.ins:
        jmp inserta_al
tabulador:
        mov al, 9
        jmp inserta_al
euro:
        mov al, C_EURO
        jmp inserta_al
cambia_ins:
        xor byte [insertar], 1
        jmp forma_cursor
cambia_codigos:
        xor byte [ver_cod], 1
        mov byte [filas_ok], 0
        ret
reajusta:
        mov word [mensaje], m_reajusta
        ret

; el formato donde está el cursor: AL (bits de negrita, subrayado, cursiva)
estado_cursor:
        mov ax, [pre]
        call inicio_parrafo
        mov si, ax
        add si, BUF
        mov cx, [pre]
        sub cx, ax
        mov bl, 0
        jcxz .x
.c:
        lodsb
        call de_clase
        test ah, 0x80
        jz .s
        and ah, 7
        xor bl, ah
.s:
        loop .c
.x:
        mov al, bl
        ret
; los códigos de las letras de los bits de AL, en DI (hasta 3)
codigos_de:
        mov ah, al
        test ah, 1
        jz .s
        mov al, CODN
        stosb
.s:
        test ah, 2
        jz .c
        mov al, CODS
        stosb
.c:
        test ah, 4
        jz .x
        mov al, CODC
        stosb
.x:
        ret
; Intro: el párrafo se parte. El nuevo se queda con la alineación y la
; sangría del de antes, y el formato que se llevaba sigue en él
intro:
        call estado_cursor
        mov [i_est], al
        mov di, tmp
        call codigos_de             ; lo que estaba puesto se cierra...
        mov al, 0x0A
        stosb
        mov ax, [pre]               ; ...los códigos del párrafo, otra vez...
        call inicio_parrafo
        mov si, ax
        add si, BUF
.p:
        cmp si, [g_ini]
        jae .p1
        lodsb
        push ax
        call de_clase
        test ah, 0x40
        pop ax
        jz .p1
        stosb
        cmp di, tmp + 12
        jb .p
.p1:
        mov al, [i_est]             ; ...y se vuelve a abrir
        call codigos_de
        mov cx, di
        mov si, tmp
        sub cx, si
        jmp inserta
intro_queda:
        call cursor32
        push dx
        push ax
        call intro
        pop ax
        pop dx
        jmp ir_a
; quita los códigos de párrafo que haya delante del cursor (al juntar dos)
quita_prefijo:
        call alrededor
        mov al, ah
        test al, al
        jz .x
        call de_clase
        test ah, 0x40
        jz .x
        mov cx, 1
        call borra_delante
        jmp quita_prefijo
.x:
        ret
retroceso:
        call limpia_pareja
.c:
        call alrededor
        test al, al
        jnz .h
        cmp word [lenA], 0          ; (¿de verdad no hay nada antes?)
        jne .h0
        cmp word [lenA+2], 0
        je .x
.h0:
        call trae_de_A
        jc .x
        jmp .c
.h:
        call es_codigo
        jnc .b
        call atras1                 ; los códigos no se borran: se pasan
        jmp .c
.b:
        push ax
        call borra_atras
        pop ax
        cmp al, 0x0A
        je quita_prefijo
        cmp al, 0x0C
        je quita_prefijo
.x:
        ret
suprime:
        call limpia_pareja
.c:
        cmp word [post], 0
        jne .h
        call trae_de_B
        jc .x
.h:
        mov si, [pstart]
        mov al, [si]
        call es_codigo
        jnc .b
        call adelante1
        jmp .c
.b:
        push ax
        mov cx, 1
        call borra_delante
        pop ax
        cmp al, 0x0A
        je quita_prefijo
        cmp al, 0x0C
        je quita_prefijo
.x:
        ret
borra_palabra:
.a:
        call alrededor
        mov al, ah
        test al, al
        jz .x
        call es_codigo
        jnc .a1
        call adelante1
        jmp .a
.a1:
        call es_de_palabra
        jnc .b
        mov cx, 1
        call borra_delante
        jmp .a
.b:
        call alrededor
        cmp ah, ' '
        jne .x
        mov cx, 1
        call borra_delante
        jmp .b
.x:
        ret
; ^Y: la fila entera (con su fin de párrafo, si lo tiene)
borra_linea:
        call filas_al_dia
        mov bl, [cur_fila]
        mov bh, 0
        push bx
        call rehace_fila
        pop bx
        jc .x
        mov cl, 3
        shl bx, cl
        mov ax, [filas+bx]
        call mueve_hueco
        mov ax, [m_fin]
        call w_de
        sub ax, [pre]
        mov cx, ax
        call borra_delante
        jmp quita_prefijo
.x:
        ret
; ^QY: desde el cursor hasta el final de la fila
borra_fin_linea:
        call filas_al_dia
        mov bl, [cur_fila]
        mov bh, 0
        call rehace_fila
        jc .x
        mov ax, [m_fin]
        cmp byte [m_ult], 0
        je .n
        mov ax, [k_ultp]
.n:
        call w_de
        sub ax, [pre]
        jbe .x
        mov cx, ax
        jmp borra_delante
.x:
        ret

; Alt+T: la letra de antes del cursor cambia a la siguiente de su familia
; (a á à ä â, n ñ, c ç, ? ¿...): así se escriben las mayúsculas con tilde y
; todo lo que no tenga el teclado
acento:
        call alrededor
        test al, al
        jz .x
        mov si, familias
.b:
        mov di, si                  ; DI = la primera de la familia
.c:
        mov ah, [si]
        inc si
        test ah, ah
        jz .fam
        cmp ah, al
        jne .c
        mov al, [si]                ; la siguiente (o la primera)
        test al, al
        jnz .pon
        mov al, [di]
.pon:
        push ax
        call borra_atras
        pop ax
        jmp inserta_al
.fam:
        cmp byte [si], 0
        jne .b
.x:
        jmp pita
familias:
        db 'a', 0xA0, 0x85, 0x84, 0x83, 0, 'e', 0x82, 0x8A, 0x89, 0x88, 0
        db 'i', 0xA1, 0x8D, 0x8B, 0x8C, 0, 'o', 0xA2, 0x95, 0x94, 0x93, 0
        db 'u', 0xA3, 0x97, 0x81, 0x96, 0, 'A', 0xB5, 0xB7, 0x8E, 0xB6, 0
        db 'E', 0x90, 0xD4, 0xD3, 0xD2, 0, 'I', 0xD6, 0xDE, 0xD8, 0xD7, 0
        db 'O', 0xE0, 0xE3, 0x99, 0xE2, 0, 'U', 0xE9, 0xEB, 0x9A, 0xEA, 0
        db 'n', 0xA4, 0, 'N', 0xA5, 0, 'c', 0x87, 0, 'C', 0x80, 0
        db '?', 0xA8, 0, '!', 0xAD, 0, '<', 0xAE, 0, '>', 0xAF, 0
        db '.', 0xFA, 0, 'l', '.', 0xFA, 0, 'E', C_EURO, 0, 0

; Alt+P: un salto de página
salto_pagina:
        mov al, 0x0C
        jmp inserta_al

; =============================================================================
;  El formato de las letras: negrita, subrayado, cursiva
; =============================================================================
negrita:
        mov al, CODN
        jmp short formato
subrayado:
        mov al, CODS
        jmp short formato
cursiva:
        mov al, CODC
; sin bloque: lo que se escriba a continuación (o se deja de poner, si el
; cursor está justo antes del código que lo cierra). Con bloque: todo él
formato:
        mov [f_cod], al
        call bloque
        jc .sin
        mov byte [f_forzar], 0xFF
        call formato_bloque
        jmp desmarca
.sin:
        call alrededor
        cmp ah, [f_cod]
        jne .par
        jmp adelante1               ; se sale de él
.par:
        mov al, [f_cod]
        mov [tmp], al
        mov [tmp+1], al
        mov si, tmp
        mov cx, 2
        call inserta
        jmp atras1                  ; el cursor, entre los dos
; AL = código: su bit (en AL)
bit_de:
        call de_clase
        mov al, ah
        and al, 7
        ret
; todo el bloque con el formato [f_cod]; si ya empezaba con él, se le quita
formato_bloque:
        call fija_bloque
        call ir_a
        call estado_cursor
        mov [f_est], al
        mov al, [f_cod]
        call bit_de
        mov [f_bit], al
        mov ah, [f_est]
        and ah, al                  ; ¿ya lo tiene?
        mov al, 0
        jnz .d
        mov al, 1
.d:
        cmp byte [f_forzar], 0xFF
        je .d1
        mov al, [f_forzar]
.d1:
        mov [f_quiere], al
        mov al, [f_est]             ; f_orig: como estaba; f_ahora: como está
        and al, [f_bit]
        jz .o
        mov al, 1
.o:
        mov [f_orig], al
        mov [f_ahora], al
        call .iguala
.c:
        call cursor32
        cmp dx, [b_fin+2]
        jb .s
        ja .fin
        cmp ax, [b_fin]
        jae .fin
.s:
        call alrededor
        cmp ah, [f_cod]
        jne .t
        mov cx, 1                   ; los que había dentro, fuera
        call borra_delante
        xor byte [f_orig], 1
        jmp .c
.t:
        cmp ah, 0x0A
        je .p
        cmp ah, 0x0C
        je .p
        call adelante1
        jc .fin
        jmp .c
.p:
        call adelante1              ; otro párrafo: se empieza sin nada
        mov byte [f_orig], 0
        mov byte [f_ahora], 0
        call normaliza
        call cursor32
        cmp dx, [b_fin+2]
        jb .p1
        ja .fin
        cmp ax, [b_fin]
        jae .fin
.p1:
        call .iguala
        jmp .c
.fin:
        mov al, [f_orig]            ; detrás, como estaba
        mov [f_quiere], al
        jmp .iguala
.iguala:
        mov al, [f_ahora]
        cmp al, [f_quiere]
        je .i
        xor byte [f_ahora], 1
        mov al, [f_cod]
        jmp inserta_al
.i:
        ret
; Alt+Q: quitar la negrita, el subrayado y la cursiva del bloque
quita_formato:
        call bloque
        jc .x
        mov byte [f_forzar], 0
        mov al, CODN
        call .uno
        mov al, CODS
        call .uno
        mov al, CODC
        call .uno
        jmp desmarca
.uno:
        mov [f_cod], al
        jmp formato_bloque
.x:
        jmp pita

; =============================================================================
;  El formato de los párrafos: alineación y sangría
; =============================================================================
al_izq:
        mov al, 0
        jmp short alinea
al_centro:
        mov al, C_CEN
        jmp short alinea
al_der:
        mov al, C_DER
        jmp short alinea
al_just:
        mov al, C_JUS
alinea:
        mov [a_cod], al
        mov word [a_hace], .uno
        jmp por_parrafos
.uno:
        ; quita la alineación que tenga y pone la nueva
.c:
        call alrededor
        mov al, ah
        call de_clase
        test ah, 0x40
        jz .p
        cmp ah, 0x44
        je .s
        mov cx, 1
        call borra_delante
        dec word [a_delta]
        jmp .c
.s:
        call adelante1
        jmp .c
.p:
        mov al, [a_cod]
        test al, al
        jz .x
        inc word [a_delta]
        jmp inserta_al
.x:
        ret
; Alt+G: un paso más de sangría (de 0 a 4, y vuelta a 0)
sangria:
        mov word [a_hace], .uno
        jmp por_parrafos
.uno:
        xor bx, bx
.c:
        call alrededor
        mov al, ah
        call de_clase
        test ah, 0x40
        jz .p
        cmp ah, 0x44
        jne .s
        push bx
        mov cx, 1
        call borra_delante
        pop bx
        inc bx
        dec word [a_delta]
        jmp .c
.s:
        push bx
        call adelante1
        pop bx
        jmp .c
.p:
        inc bx
        cmp bx, 5
        jb .q
        xor bx, bx
.q:
        mov cx, bx
        jcxz .x
.r:
        push cx
        mov al, C_SANG
        call inserta_al
        pop cx
        inc word [a_delta]
        loop .r
.x:
        ret
; hace [a_hace] con el principio del párrafo del cursor (o con todos los del
; bloque), y deja el cursor donde estaba
por_parrafos:
        mov word [a_delta], 0
        call bloque
        jc .uno
        call fija_bloque
        call ir_a
.otro:
        call .este
        ; al siguiente párrafo, si sigue dentro del bloque
.b:
        call alrededor
        test ah, ah
        jz .fin
        push ax
        call adelante1
        pop ax
        cmp ah, 0x0A
        je .nuevo
        cmp ah, 0x0C
        jne .b
.nuevo:
        call cursor32
        cmp dx, [b_fin+2]
        jb .otro
        ja .fin
        cmp ax, [b_fin]
        jb .otro
.fin:
        jmp desmarca
.uno:
        call cursor32
        push dx
        push ax
        call .este
        pop ax
        pop dx
        add ax, [a_delta]
        adc dx, 0
        cmp word [a_delta], 0
        jge .v
        dec dx                      ; (a_delta es negativo)
.v:
        jmp ir_a
.este:
        mov ax, [pre]
        call inicio_parrafo
        call mueve_hueco
        jmp [a_hace]

; =============================================================================
;  Bloques
; =============================================================================
; el bloque: DX:AX a CX:BX. CF=1 si no hay
bloque:
        mov ax, [b_ini]
        mov dx, [b_ini+2]
        mov bx, [b_fin]
        mov cx, [b_fin+2]
        cmp byte [marcando], 0
        je .o
        push ax
        push dx
        call cursor32
        mov bx, ax
        mov cx, dx
        pop dx
        pop ax
.o:
        cmp dx, cx                  ; en orden
        jb .ok
        ja .da
        cmp ax, bx
        jb .ok
        je .no
.da:
        xchg ax, bx
        xchg dx, cx
.ok:
        clc
        ret
.no:
        stc
        ret
; deja el bloque quieto (si se estaba marcando) y en orden: DX:AX su principio
fija_bloque:
        call bloque
        mov [b_ini], ax
        mov [b_ini+2], dx
        mov [b_fin], bx
        mov [b_fin+2], cx
        mov byte [marcando], 0
        ret
desmarca:
        xor ax, ax
        mov [b_ini], ax
        mov [b_ini+2], ax
        mov [b_fin], ax
        mov [b_fin+2], ax
        mov byte [marcando], 0
        mov byte [filas_ok], 0
        ret
; F7: empieza a marcar (y lo que se mueva el cursor, se marca); otra vez F7,
; se deja de marcar
marcar:
        cmp byte [marcando], 0
        je .emp
        jmp fija_bloque
.emp:
        call cursor32
        mov [b_ini], ax
        mov [b_ini+2], dx
        mov byte [marcando], 1
        ret
marca_ini:
        call cursor32
        mov [b_ini], ax
        mov [b_ini+2], dx
        mov byte [marcando], 0
        ret
marca_fin:
        call cursor32
        mov [b_fin], ax
        mov [b_fin+2], dx
        mov byte [marcando], 0
        ret
; lo que mide el bloque, en DX:AX (después de fija_bloque)
mide_bloque:
        mov ax, [b_fin]
        mov dx, [b_fin+2]
        sub ax, [b_ini]
        sbb dx, [b_ini+2]
        ret
; F4: copiarlo (a la memoria; si es grande, a un archivo)
copiar:
        call bloque
        jc .no
        call fija_bloque
        call al_portapapeles
        jmp desmarca
.no:
        jmp pita
al_portapapeles:
        call mide_bloque
        mov [clip_n], ax
        mov [clip_n+2], dx
        push ax
        push dx
        mov ax, [b_ini]
        mov dx, [b_ini+2]
        call lee_desde
        pop dx
        pop ax
        test dx, dx
        jnz .disco
        cmp ax, CLIP_TAM
        ja .disco
        mov byte [clip_f], 0
        mov cx, ax
        mov di, CLIP
        jcxz .x
.c:
        call lee_letra
        stosb
        loop .c
.x:
        ret
.disco:
        mov byte [clip_f], 1
        mov bx, hP
        call abre_tmp
        jc .mal
        mov bx, [hP]
        call sal_a
        mov ax, [clip_n]
        mov dx, [clip_n+2]
        call copia_letras
        call sal_fin
        ret
.mal:
        mov word [clip_n], 0
        mov word [clip_n+2], 0
        ret
; copia DX:AX letras de lo que se está leyendo a la salida
copia_letras:
.c:
        mov cx, ax
        or cx, dx
        jz .x
        push ax
        push dx
        call lee_letra
        jc .y
        call sal_letra
        pop dx
        pop ax
        sub ax, 1
        sbb dx, 0
        jmp .c
.y:
        pop dx
        pop ax
.x:
        ret
; F5 (y ^KY): cortarlo
cortar:
        call bloque
        jc copiar.no
        call fija_bloque
        call al_portapapeles
borra_bloque:
        mov ax, [b_ini]
        mov dx, [b_ini+2]
        call ir_a
        call mide_bloque
.c:
        mov cx, 0x8000
        test dx, dx
        jnz .k
        cmp ax, cx
        jae .k
        mov cx, ax
.k:
        jcxz .x
        sub ax, cx
        sbb dx, 0
        push ax
        push dx
        call borra_delante
        pop dx
        pop ax
        jmp .c
.x:
        call desmarca
        mov byte [centrar], 1
        ret
; F6: pegar lo copiado donde está el cursor
pegar:
        cmp byte [clip_f], 0
        jne .disco
        mov si, CLIP
        mov cx, [clip_n]
        jmp inserta
.disco:
        mov bx, [hP]
        xor cx, cx
        xor dx, dx
        mov ax, 0x4200
        int 0x21
        mov ax, [clip_n]
        mov dx, [clip_n+2]
.c:
        mov cx, 512
        test dx, dx
        jnz .k
        cmp ax, cx
        jae .k
        mov cx, ax
.k:
        jcxz .x
        sub ax, cx
        sbb dx, 0
        push ax
        push dx
        push cx
        mov bx, [hP]
        mov dx, sec
        DOS 0x3F
        pop cx
        jc .y
        mov si, sec
        call inserta
.y:
        pop dx
        pop ax
        jnc .c
.x:
        mov byte [centrar], 1
        ret
; ^KC: copiar el bloque donde está el cursor
ws_copia:
        call bloque
        jc .x
        call fija_bloque
        call al_portapapeles
        call desmarca
        jmp pegar
.x:
        ret
; ^KV: moverlo donde está el cursor
ws_mueve:
        call bloque
        jc .x
        call fija_bloque
        call cursor32               ; (no dentro de él)
        cmp dx, [b_ini+2]
        jb .f
        ja .d
        cmp ax, [b_ini]
        jbe .f
.d:
        cmp dx, [b_fin+2]
        ja .tras
        jb .no
        cmp ax, [b_fin]
        jb .no
.tras:
        push ax                     ; detrás: al borrarlo, el sitio se corre
        push dx
        call mide_bloque
        mov bx, ax
        mov cx, dx
        pop dx
        pop ax
        sub ax, bx
        sbb dx, cx
        jmp short .h
.f:
.h:
        push ax
        push dx
        call al_portapapeles
        call borra_bloque
        pop dx
        pop ax
        call ir_a
        jmp pegar
.no:
        call pita
.x:
        ret

; ---- dónde se guarda el cursor ----
pon_cursor:
        mov ah, 2
        xor bh, bh
        int 0x10
        ret
pita:
        mov ax, 0x0E07
        int 0x10
        ret

%include "dialogo.inc"
%include "texto.inc"
%include "pantalla.inc"
%include "archivo.inc"
%include "imprime.inc"
%include "letras.inc"
%include "variables.inc"
