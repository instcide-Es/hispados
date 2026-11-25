; =============================================================================
;  XCOPIAR.COM (XCOPY) - copia archivos y directorios enteros
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       XCOPIAR origen [destino] [/S]
;
;  El origen es un directorio (todo lo que tiene) o un patrón con comodines;
;  el destino, un directorio, que se crea si no existe (sin destino, el de
;  trabajo). Con /S copia también los directorios que cuelgan del origen, con
;  todo lo suyo, creándolos en el destino.
;
;  Es un XCOPIAR sencillo: no pregunta, y si el destino está dentro del origen se para cuando la ruta no da más.
;  Baja por los directorios con una rutina que se llama a sí misma (cada nivel
;  guarda en la pila su zona de búsquedas). Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100
%define SOLO_TEXTO

BUF_TAM     equ 8192

        jmp inicio
%include "util.inc"

inicio:
        cld
        mov si, 0x81
.arg:
        call salta_esp
        mov al, [si]
        cmp al, ' '
        jb .fin_args
        cmp al, '/'
        jne .ruta
        mov al, [si+1]
        call mayus
        cmp al, 'S'
        jne .uso
        mov byte [con_sub], 1
        add si, 2
        jmp .arg
.uso:
        mov si, m_uso
        call pon_cad
        mov ax, 0x4C01
        int 0x21
.ruta:
        mov di, o_ruta              ; la primera, el origen; la segunda, el
        cmp byte [o_ruta], 0        ; destino
        je .r
        mov di, d_ruta
.r:
        lodsb
        cmp al, ' '
        jbe .rf
        call mayus
        stosb
        jmp .r
.rf:
        mov byte [di], 0
        dec si
        jmp .arg
.fin_args:
        cmp byte [o_ruta], 0
        je .uso
        ; el origen: ¿un directorio, o un patrón?
        mov di, o_ruta
        call al_final
        mov al, [di-1]
        cmp al, '\'
        je .o_dir
        cmp al, ':'
        je .o_dir
        cmp al, '.'
        je .o_barra
        mov dx, dta
        DOS 0x1A
        mov si, o_ruta
        call con_comodin
        je .o_patron
        mov dx, o_ruta
        mov cx, 0x10
        DOS 0x4E
        jc .o_patron
        test byte [dta+21], 0x10
        jz .o_patron
.o_barra:
        mov al, '\'
        stosb
        mov byte [di], 0
.o_dir:
        mov si, t_todos
        mov di, patron
        call copia_txt
        jmp short .destino
.o_patron:
        mov si, o_ruta              ; se parte por la última barra
        mov bx, si
.p:
        lodsb
        test al, al
        jz .p2
        cmp al, '\'
        je .p1
        cmp al, ':'
        jne .p
.p1:
        mov bx, si
        jmp .p
.p2:
        mov si, bx
        mov di, patron
        call copia_txt
        mov byte [bx], 0
.destino:
        mov di, d_ruta              ; el destino: se crea, y acabado en barra
        call al_final
        cmp di, d_ruta
        je .listo
        mov al, [di-1]
        cmp al, '\'
        je .listo
        cmp al, ':'
        je .listo
        mov dx, d_ruta
        DOS 0x39                    ; (si ya existe, no pasa nada)
        mov al, '\'
        stosb
        mov byte [di], 0
.listo:
        mov di, o_ruta
        call al_final
        mov [o_fin], di
        mov di, d_ruta
        call al_final
        mov [d_fin], di
        call copia
        mov ax, [n]
        call pon_num
        mov si, m_copiados
        call pon_cad
        mov al, [mal]
        mov ah, 0x4C
        int 0x21

; copia lo que casa con el patrón del directorio o_ruta al d_ruta (acaban en
; [o_fin] y [d_fin]) y, con /S, baja a los directorios del origen
copia:
        sub sp, 44
        mov bp, sp
        push word [o_fin]
        push word [d_fin]
        mov dx, bp
        DOS 0x1A
        mov si, patron
        mov di, [o_fin]
        call copia_txt
        mov dx, o_ruta
        xor cx, cx
        DOS 0x4E
        jc .dirs
.uno:
        lea si, [bp+30]
        mov di, [o_fin]
        call copia_txt
        lea si, [bp+30]
        mov di, [d_fin]
        call copia_txt
        mov si, o_ruta
        call pon_cad
        call pon_nl
        call copia_arch
        DOS 0x4F
        jnc .uno
.dirs:
        cmp byte [con_sub], 0
        je .fin
        mov si, t_todos
        mov di, [o_fin]
        call copia_txt
        mov dx, o_ruta
        mov cx, 0x10
        DOS 0x4E
        jc .fin
.dir:
        test byte [bp+21], 0x10
        jz .sig
        cmp byte [bp+30], '.'
        je .sig
        mov di, [d_fin]             ; (que quepa la ruta)
        sub di, d_ruta
        cmp di, 60
        ja .sig
        lea si, [bp+30]             ; el directorio, en las dos rutas
        mov di, [o_fin]
        call copia_txt
        mov ax, '\'
        stosw
        dec di
        mov [o_fin], di
        lea si, [bp+30]
        mov di, [d_fin]
        call copia_txt
        mov dx, d_ruta
        DOS 0x39                    ; se crea en el destino
        mov ax, '\'
        stosw
        dec di
        mov [d_fin], di
        push bp
        call copia
        pop bp
        mov ax, [bp-4]              ; (los finales de este nivel, que están
        mov [d_fin], ax             ; guardados en la pila)
        mov ax, [bp-2]
        mov [o_fin], ax
        mov dx, bp
        DOS 0x1A
.sig:
        DOS 0x4F
        jnc .dir
.fin:
        pop word [d_fin]
        pop word [o_fin]
        add sp, 44
        ret

; copia el archivo o_ruta a d_ruta
copia_arch:
        mov dx, o_ruta
        mov al, 0
        DOS 0x3D
        jc .mal0
        mov [h1], ax
        mov dx, d_ruta
        xor cx, cx
        DOS 0x3C
        jnc .creado
        mov bx, [h1]
        push ax
        DOS 0x3E
        pop ax
        mov si, m_e_mismo           ; (32: está abierto, es el mismo origen)
        cmp ax, 32
        je .dice
.mal0:
        mov si, m_e_no
        jmp short .dice
.creado:
        mov [h2], ax
.trozo:
        mov bx, [h1]
        mov cx, BUF_TAM
        mov dx, buf
        DOS 0x3F
        jc .mal
        test ax, ax
        jz .hecho
        mov cx, ax
        mov bx, [h2]
        mov dx, buf
        DOS 0x40
        jc .mal
        cmp ax, cx
        je .trozo
        mov si, m_e_lleno
        jmp short .m1
.mal:
        mov si, m_e_no
.m1:
        call .cierra
.dice:
        call pon_cad
        mov byte [mal], 1
        ret
.hecho:
        inc word [n]
        mov bx, [h1]                ; la copia, con la fecha del original
        mov ax, 0x5700
        int 0x21
        jc .cierra
        mov bx, [h2]
        mov ax, 0x5701
        int 0x21
.cierra:
        mov bx, [h1]
        DOS 0x3E
        mov bx, [h2]
        DOS 0x3E
        ret

; DI -> el 0 del final del texto de DI
al_final:
        cmp byte [di], 0
        je .x
        inc di
        jmp al_final
.x:
        ret
; copia el texto de SI a DI (con su 0); DI queda en el 0
copia_txt:
        lodsb
        stosb
        test al, al
        jnz copia_txt
        dec di
        ret
; ¿tiene comodines el texto de SI? ZF=1 si los tiene
con_comodin:
        lodsb
        test al, al
        jz .no
        cmp al, '*'
        je .x
        cmp al, '?'
        jne con_comodin
.x:
        ret
.no:
        inc al
        ret

t_todos     db "*.*", 0
m_uso       db "XCOPIAR - copia archivos y directorios enteros", 10
            db "  XCOPIAR origen [destino] [/S]     /S: también los subdirectorios", 10, 0
m_copiados  db " archivos copiados.", 10, 0
m_e_no      db "  no se ha podido copiar.", 10, 0
m_e_lleno   db "  el disco está lleno.", 10, 0
m_e_mismo   db "  no se puede copiar sobre sí mismo.", 10, 0
con_sub     db 0
mal         db 0
sin_preguntar db 0
n           dw 0
o_ruta      times 100 db 0
d_ruta      times 100 db 0

        absolute $
            alignb 2
o_fin       resw 1
d_fin       resw 1
h1          resw 1
h2          resw 1
patron      resb 16
dta         resb 44
buf         resb BUF_TAM
