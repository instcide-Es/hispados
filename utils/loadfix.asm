; =============================================================================
;  Hispa-DOS LOADFIX.COM - cargar un programa por encima de los primeros 64 KB
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  LOADFIX [unidad:][ruta]programa [argumentos]
;
;  Algunos programas antiguos (los comprimidos con ciertas versiones de
;  EXEPACK, y algunos juegos) fallan si se cargan en los primeros 64 KB de la
;  memoria: «Packed file corrupt», cuelgues o basura en la pantalla. Con
;  DOS=HIGH queda tanta memoria baja libre que es fácil que un programa caiga
;  ahí. LOADFIX ocupa con un bloque de relleno lo que quede libre por debajo
;  de 1000h:0000, ejecuta el programa (que así se carga por encima) y, cuando
;  acaba, libera el relleno y devuelve su mismo código de retorno
;  (ERRORLEVEL).
;
;  Cómo: encoge su propio bloque, y mientras el primer bloque libre empiece
;  por debajo del segmento 1000h lo reserva (INT 21h 48h) hasta 1000h; después
;  INT 21h 4B00h y, al volver, INT 21h 49h de cada relleno.
;  Códigos de retorno propios: 255 si no se ha podido ejecutar el programa.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

LIMITE  equ 0x1000              ; primer segmento admitido para el programa
MAXREL  equ 16                  ; bloques de relleno como mucho

start:
        mov sp, stack_top
        cld
        mov di, bss_start
        mov cx, stack_top - bss_start
        xor al, al
        rep stosb
        ; quedarse solo con lo que ocupa LOADFIX
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        mov si, 0x81
        call skip_sp
        cmp byte [si], 13
        je usage
        cmp word [si], '/?'
        je usage
        call ej_buscar
        jc not_found
        mov [args], si
        call rellenar
        mov si, [args]
        call ej_lanzar
        pushf
        push ax
        call liberar
        pop ax
        popf
        jc exec_err
        mov ah, 0x4C                ; el código de retorno del programa
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C00
        int 0x21
not_found:
        mov si, msg_nf
        call puts
        mov si, ej_nombre
        call puts
        call crlf
        mov ax, 0x4CFF
        int 0x21
exec_err:
        mov si, msg_nomem
        cmp ax, 8
        je .m
        mov si, msg_exec
.m:
        call puts
        mov si, ej_ruta
        call puts
        call crlf
        mov ax, 0x4CFF
        int 0x21

skip_sp:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        jne .r
.s:
        inc si
        jmp skip_sp
.r:
        ret

; ---- ocupar la memoria libre que haya por debajo de LIMITE ----
rellenar:
        push es
        ; primer ajuste desde abajo mientras se rellena (y se restaura luego)
        mov ax, 0x5800
        int 0x21
        jc .sin
        mov [estrategia], ax
        mov byte [hay_estr], 1
        mov ax, 0x5801
        xor bx, bx
        int 0x21
.sin:
.l:
        cmp word [nrel], MAXREL
        jae .fin
        mov bx, 1
        mov ah, 0x48
        int 0x21
        jc .fin                     ; no queda memoria: se intenta igualmente
        cmp ax, LIMITE
        jae .arriba
        ; el bloque libre más bajo empieza en AX: ocuparlo hasta LIMITE (o
        ; entero, si acaba antes)
        mov di, [nrel]
        shl di, 1
        mov [rellenos + di], ax
        inc word [nrel]
        mov es, ax
        mov dx, LIMITE
        sub dx, ax                  ; lo que hace falta
        mov bx, 0xFFFF
        mov ah, 0x4A
        int 0x21                    ; falla: BX = lo máximo que puede crecer
        cmp bx, dx
        jbe .t
        mov bx, dx
.t:
        mov ah, 0x4A
        int 0x21
        jmp .l
.arriba:
        mov es, ax
        mov ah, 0x49
        int 0x21
.fin:
        cmp byte [hay_estr], 0
        je .r
        mov bx, [estrategia]
        mov ax, 0x5801
        int 0x21
.r:
        pop es
        ret

liberar:
        push es
        mov cx, [nrel]
        mov si, rellenos
.l:
        jcxz .r
        lodsw
        mov es, ax
        mov ah, 0x49
        int 0x21
        dec cx
        jmp .l
.r:
        pop es
        ret

%include "util.inc"
%include "ejecutar.inc"

msg_usage   db "Carga un programa por encima de los primeros 64 KB de la memoria y lo", 13, 10
            db "ejecuta.", 13, 10, 13, 10
            db "LOADFIX [unidad:][ruta]programa [argumentos]", 13, 10, 13, 10
            db "Sirve para los programas antiguos que, cargados muy abajo en la memoria,", 13, 10
            db "fallan con «Packed file corrupt», se cuelgan o llenan la pantalla de", 13, 10
            db "basura. Por ejemplo:  LOADFIX JUEGO.EXE /S", 13, 10
            db "Se puede combinar con VELOCIDAD:  VELOCIDAD 386 LOADFIX JUEGO.EXE", 13, 10, 0
msg_nf      db "LOADFIX: no se encuentra el programa ", 0
msg_nomem   db "LOADFIX: no hay memoria suficiente para ejecutar ", 0
msg_exec    db "LOADFIX: no se ha podido ejecutar ", 0

prog_end_data:
        absolute prog_end_data
bss_start:
args        resw 1
nrel        resw 1
rellenos    resw MAXREL
estrategia  resw 1
hay_estr    resb 1
u_lbuf      resb 84
u_numbuf    resb 16
            EJ_BSS
            alignb 2
            resb 384
stack_top:
prog_end:
