; =============================================================================
;  ATRIBUTO.COM (ATTRIB) - los atributos de los archivos
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       ATRIBUTO [+R|-R] [+A|-A] [+S|-S] [+H|-H] archivos
;
;  R: solo se puede leer.  A: cambiado desde la última copia.
;  S: del sistema.         H: oculto (DIR no lo enseña).
;  Sin signos, enseña los que tiene cada archivo. Valen los comodines.
;  Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100
%define SOLO_TEXTO

        jmp inicio
%include "util.inc"

inicio:
        cld
        mov dx, dta
        DOS 0x1A
        mov si, 0x81
.arg:
        call salta_esp
        mov al, [si]
        cmp al, ' '
        jb .fin_args
        cmp al, '+'
        je .signo
        cmp al, '-'
        je .signo
        mov di, patron              ; los archivos
.n:
        lodsb
        cmp al, ' '
        jbe .nf
        stosb
        jmp .n
.nf:
        mov byte [di], 0
        dec si
        jmp .arg
.signo:
        mov ah, al
        mov al, [si+1]
        call mayus
        add si, 2
        mov bl, 0x01
        cmp al, 'R'
        je .s1
        mov bl, 0x20
        cmp al, 'A'
        je .s1
        mov bl, 0x04
        cmp al, 'S'
        je .s1
        mov bl, 0x02
        cmp al, 'H'
        jne .uso
.s1:
        cmp ah, '+'
        jne .s2
        or [pon], bl
        jmp .arg
.s2:
        or [quita], bl
        jmp .arg
.uso:
        mov si, m_uso
        call pon_cad
        mov ax, 0x4C01
        int 0x21
.fin_args:
        cmp byte [patron], 0
        je .uso
        mov dx, patron
        mov cx, 0x06                ; también los ocultos y los del sistema
        DOS 0x4E
        jnc .uno
        mov si, m_no_hay
        call pon_cad
        mov ax, 0x4C01
        int 0x21
.uno:
        mov si, patron              ; su ruta: la del patrón, con su nombre
        mov di, ruta
        mov bx, di
.r:
        lodsb
        stosb
        cmp al, '\'
        je .r1
        cmp al, ':'
        jne .r2
.r1:
        mov bx, di
.r2:
        test al, al
        jnz .r
        mov di, bx
        mov si, dta + 30
.r3:
        lodsb
        stosb
        test al, al
        jnz .r3
        mov al, [dta+21]
        mov ah, [pon]
        or ah, [quita]
        jz .ensena
        or al, [pon]
        mov ah, [quita]
        not ah
        and al, ah
        mov cl, al
        xor ch, ch
        mov dx, ruta
        mov al, 1
        DOS 0x43
        jnc .sig
        mov si, m_e_no
        call pon_cad
        mov si, ruta
        call pon_cad
        call pon_nl
        jmp short .sig
.ensena:
        mov bx, letras              ; «A  SHR  NOMBRE»
.l:
        mov ah, [bx]
        test ah, ah
        jz .l2
        mov al, ' '
        test [dta+21], ah
        jz .l1
        mov al, [bx+1]
.l1:
        call pon_car
        add bx, 2
        jmp .l
.l2:
        mov al, ' '
        call pon_car
        call pon_car
        mov si, ruta
        call pon_cad
        call pon_nl
.sig:
        DOS 0x4F
        jc .fin
        jmp .uno
.fin:
        mov ax, 0x4C00
        int 0x21

letras      db 0x20, 'A', 0x04, 'S', 0x02, 'H', 0x01, 'R', 0
m_uso       db "ATRIBUTO - los atributos de los archivos", 10
            db "  ATRIBUTO [+R|-R] [+A|-A] [+S|-S] [+H|-H] archivos", 10
            db "R: solo leer. A: cambiado. S: del sistema. H: oculto.", 10
            db "Sin signos, enseña los que tienen.", 10, 0
m_no_hay    db "No existe ese archivo.", 10, 0
m_e_no      db "No se puede cambiar ", 0
pon         db 0
quita       db 0
sin_preguntar db 0
patron      times 82 db 0

        absolute $
dta         resb 44
ruta        resb 100
