; =============================================================================
;  ETIQUETA.COM (LABEL) - el nombre del disco
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       ETIQUETA [unidad:]             dice cómo se llama el disco
;       ETIQUETA [unidad:] nombre      le pone ese nombre (hasta 11 letras)
;       ETIQUETA [unidad:] /B          le quita el nombre
;
;  La etiqueta es una entrada del directorio raíz con el atributo 08h. Se
;  busca y se cambia leyendo y escribiendo los sectores de la raíz (INT 25h y
;  26h); dónde están lo dice el núcleo (INT 21h/E8h con AL=5). Si el sector
;  de arranque lleva también el nombre (los de DOS 4 en adelante y los de
;  FORMATEA), se pone al día. Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100
%define SOLO_TEXTO

        jmp inicio
%include "util.inc"

inicio:
        cld
        call pide_nucleo
        DOS 0x19
        mov [unidad], al
        mov si, 0x81
        call salta_esp
        cmp byte [si+1], ':'
        jne .nom
        mov al, [si]
        call mayus
        sub al, 'A'
        mov [unidad], al
        add si, 2
        call salta_esp
.nom:
        mov di, nombre              ; el nombre: once letras, en mayúsculas
        mov cx, 11
.n:
        lodsb
        cmp al, ' '
        jb .nf
        jcxz .n
        call mayus
        stosb
        dec cx
        mov byte [hay], 1
        jmp .n
.nf:
        cmp word [nombre], '/B'
        jne .medidas
        cmp byte [nombre+2], 0
        jne .medidas
        mov byte [hay], 2           ; quitarla
.medidas:
        mov ax, 0xE805
        mov dl, [unidad]
        mov di, med
        int 0x21
        jnc .raiz
        mov si, m_e_unidad
        jmp error
.raiz:
        ; se recorre la raíz: la etiqueta que haya y el primer hueco
        mov ax, [med+8]
        mov [sec_n], ax
        mov cx, [med+10]
.sector:
        push cx
        mov dx, [sec_n]
        mov bx, sec
        call lee
        pop cx
        jc .mal_leer
        mov bx, sec
.ent:
        mov al, [bx]
        test al, al
        jz .hueco
        cmp al, 0xE5
        je .hueco
        mov al, [bx+11]
        cmp al, 0x0F                ; (un trozo de nombre largo no lo es)
        je .sig
        test al, 0x08
        jz .sig
        jmp esta
.hueco:
        cmp word [hueco_sec], 0
        jne .h1
        mov ax, [sec_n]
        mov [hueco_sec], ax
        mov [hueco_off], bx
.h1:
        cmp byte [bx], 0            ; (detrás del primer 0 no hay nada)
        je sin
.sig:
        add bx, 32
        cmp bx, sec + 512
        jb .ent
        inc word [sec_n]
        loop .sector
        jmp sin
.mal_leer:
        mov si, m_e_leer
        jmp error

; ---- no tiene etiqueta ----
sin:
        cmp byte [hay], 1
        je .pon
        mov si, m_sin
        call pon_cad
        jmp fin
.pon:
        mov ax, [hueco_sec]
        test ax, ax
        jnz .p1
        mov si, m_e_lleno
        jmp error
.p1:
        mov [sec_n], ax
        mov dx, ax
        mov bx, sec
        call lee
        jc sin.mal
        mov bx, [hueco_off]
        mov di, bx                  ; una entrada nueva, limpia
        mov cx, 32
        xor al, al
        rep stosb
        mov byte [bx+11], 0x08
        DOS 0x2A                    ; con la fecha de hoy
        sub cx, 1980
        mov ax, cx
        mov cl, 9
        shl ax, cl
        mov cl, dh
        xor ch, ch
        push dx
        mov dx, cx
        mov cl, 5
        shl dx, cl
        or ax, dx
        pop dx
        xor dh, dh
        or ax, dx
        mov [bx+24], ax
        jmp short pon_nombre
.mal:
        mov si, m_e_leer
        jmp error

; ---- la tiene: está en BX, dentro del sector [sec_n] ----
esta:
        cmp byte [hay], 0
        jne .cambia
        mov si, m_es
        call pon_cad
        mov cx, 11
.c:
        mov al, [bx]
        call pon_car
        inc bx
        loop .c
        call pon_nl
        jmp fin
.cambia:
        cmp byte [hay], 1
        je pon_nombre
        mov byte [bx], 0xE5         ; quitarla
        mov si, t_sin_nombre
        mov di, nombre
        mov cx, 11
        rep movsb
        jmp short escribe
pon_nombre:
        mov di, bx
        mov si, nombre
        mov cx, 11
.c:
        lodsb
        test al, al
        jnz .c1
        mov al, ' '
.c1:
        stosb
        loop .c
        push di                     ; (nombre, también relleno con espacios)
        mov si, bx
        mov di, nombre
        mov cx, 11
        rep movsb
        pop di
escribe:
        mov dx, [sec_n]
        mov bx, sec
        mov al, [unidad]
        mov cx, 1
        int 0x26
        pop dx
        jnc .arr
        mov si, m_e_escribir
        jmp error
.arr:
        ; el sector de arranque, si lleva el nombre
        xor dx, dx
        mov bx, sec
        call lee
        jc .hecho
        cmp word [sec+11], 512
        jne .hecho
        cmp byte [sec+38], 0x29
        jne .hecho
        mov si, nombre
        mov di, sec + 43
        mov cx, 11
        rep movsb
        xor dx, dx
        mov bx, sec
        mov al, [unidad]
        mov cx, 1
        int 0x26
        pop dx
.hecho:
        mov si, m_hecho
        call pon_cad
fin:
        mov ax, 0x4C00
        int 0x21
error:
        call pon_cad
        mov ax, 0x4C01
        int 0x21
; lee el sector DX de la unidad en BX. CF=1 si falla
lee:
        mov al, [unidad]
        mov cx, 1
        int 0x25
        pop dx
        cld
        ret

m_es        db "El disco se llama ", 0
m_sin       db "El disco no tiene etiqueta.", 10
            db "ETIQUETA [unidad:] nombre se la pone; ETIQUETA [unidad:] /B se la quita.", 10, 0
m_hecho     db "Hecho.", 10, 0
m_e_unidad  db "La unidad no está preparada.", 10, 0
m_e_leer    db "No se puede leer el disco.", 10, 0
m_e_escribir db "No se puede escribir en el disco.", 10, 0
m_e_lleno   db "El directorio raíz está lleno.", 10, 0
t_sin_nombre db "NO NAME    "
unidad      db 0
hay         db 0                    ; 0 solo mirar, 1 poner, 2 quitar
sin_preguntar db 0
hueco_sec   dw 0
hueco_off   dw 0
sec_n       dw 0
nombre      times 12 db 0

        absolute $
            alignb 2
med         resw 9                  ; las medidas de la unidad (E8h/5)
sec         resb 512
