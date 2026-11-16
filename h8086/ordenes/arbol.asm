; =============================================================================
;  ARBOL.COM (ÁRBOL, TREE) - los directorios de un disco, en árbol
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       ARBOL [unidad:][directorio] [/A]
;
;  Enseña los directorios que cuelgan del que se diga (o de la raíz de la
;  unidad de trabajo), cada nivel dos espacios más adentro. Con /A, también
;  los archivos de cada uno.
;
;  Se baja por los directorios con una rutina que se llama a sí misma: cada
;  nivel guarda en la pila su zona de búsquedas (44 octetos) y la ruta se va
;  alargando y acortando en un solo sitio. Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100
%define SOLO_TEXTO

        jmp inicio
%include "util.inc"

inicio:
        cld
        mov di, ruta
        mov si, 0x81
.arg:
        call salta_esp
        mov al, [si]
        cmp al, ' '
        jb .fin_args
        cmp al, '/'
        jne .dir
        mov al, [si+1]
        call mayus
        cmp al, 'A'
        jne .uso
        mov byte [archivos], 1
        add si, 2
        jmp .arg
.uso:
        mov si, m_uso
        call pon_cad
        mov ax, 0x4C01
        int 0x21
.dir:
        lodsb
        cmp al, ' '
        jbe .df
        call mayus
        stosb
        jmp .dir
.df:
        dec si
        jmp .arg
.fin_args:
        cmp di, ruta                ; sin nada: la raíz
        jne .hay
        mov al, '\'
        stosb
.hay:
        mov al, [di-1]              ; acabada en barra
        cmp al, '\'
        je .barra
        cmp al, ':'
        je .barra
        mov al, '\'
        stosb
.barra:
        mov byte [di], 0
        mov si, ruta
        call pon_cad
        call pon_nl
        mov word [nivel], 1
        call baja
        mov ax, [n_dir]
        call pon_num
        mov si, m_dirs
        call pon_cad
        mov ax, 0x4C00
        int 0x21

; enseña lo que hay en el directorio de `ruta` (que acaba en barra, en DI) y
; baja a cada uno de sus directorios
baja:
        sub sp, 44                  ; la zona de búsquedas de este nivel
        mov bp, sp
        push di
        mov dx, bp
        DOS 0x1A
        mov si, t_todos
        push di
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop di
        mov dx, ruta
        mov cx, 0x10
        DOS 0x4E
        jc .fin
.uno:
        cmp byte [bp+30], '.'       ; («.» y «..» no cuentan)
        je .sig
        test byte [bp+21], 0x10
        jnz .es_dir
        cmp byte [archivos], 0
        je .sig
        call sangra
        lea si, [bp+30]
        call pon_cad
        call pon_nl
        jmp short .sig
.es_dir:
        inc word [n_dir]
        call sangra
        mov al, '['
        call pon_car
        lea si, [bp+30]
        call pon_cad
        mov al, ']'
        call pon_car
        call pon_nl
        cmp word [nivel], 16        ; (la ruta no da para más)
        jae .sig
        push di                     ; la ruta, con él; y adentro
        push bp
        lea si, [bp+30]
.n:
        lodsb
        test al, al
        jz .nf
        stosb
        jmp .n
.nf:
        mov al, '\'
        stosb
        inc word [nivel]
        call baja
        dec word [nivel]
        pop bp
        pop di
        mov dx, bp                  ; (las búsquedas, otra vez las nuestras)
        DOS 0x1A
.sig:
        DOS 0x4F
        jnc .uno
.fin:
        pop di
        add sp, 44
        ret
; dos espacios por nivel
sangra:
        push cx
        mov cx, [nivel]
        shl cx, 1
        mov al, ' '
.s:
        call pon_car
        loop .s
        pop cx
        ret

t_todos     db "*.*", 0
m_uso       db "ARBOL - los directorios de un disco", 10
            db "  ARBOL [unidad:][directorio] [/A]     /A: también los archivos", 10, 0
m_dirs      db " directorios", 10, 0
archivos    db 0
sin_preguntar db 0
nivel       dw 0
n_dir       dw 0

        absolute $
ruta        resb 300
