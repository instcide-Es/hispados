; =============================================================================
;  Hispa-DOS MOVER.COM (MOVE) - mover archivos y renombrar carpetas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  MOVER [/Y | /-Y] origen destino
;     origen   archivo(s) (admite comodines) o una carpeta (se renombra)
;     destino  carpeta o nombre nuevo
;     /Y       no pregunta antes de sobrescribir; /-Y sí pregunta (normal)
;  En la misma unidad se renombra (no se copia nada); entre unidades se copia
;  (con fecha y hora) y se borra el original.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

BUFSZ   equ 32768

start:
        mov sp, stack_top
        ; las variables sin valor inicial a cero (DOS no las limpia)
        cld
        mov di, bss_start
        mov cx, stack_top - bss_start
        xor al, al
        rep stosb
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        ; argumentos: opciones y dos nombres
        mov si, 0x81
        mov di, src
        call next_arg
        jc usage
        mov di, dst
        call next_arg
        jc usage
        mov di, extra
        call next_arg
        jnc usage
        ; ¿el origen tiene comodines?
        mov si, src
        call has_wild
        mov [wild], al
        ; ¿el destino es una carpeta que existe?
        mov dx, dst
        mov ax, 0x4300
        int 0x21
        jc .nodir
        test cl, 0x10
        jz .nodir
        mov byte [dst_isdir], 1
.nodir:
        ; «X:», «X:\» y «ruta\» también son carpetas (el raíz no tiene atributos)
        mov si, dst
.lc:
        lodsb
        cmp byte [si], 0
        jne .lc
        cmp al, ':'
        je .isd
        cmp al, '\'
        jne .nisd
.isd:
        mov byte [dst_isdir], 1
.nisd:
        ; varios archivos solo a una carpeta
        cmp byte [wild], 0
        je .one
        cmp byte [dst_isdir], 0
        jne .one
        mov si, msg_multi
        call puts
        mov ax, 0x4C01
        int 0x21
.one:
        ; carpeta del origen (prefijo hasta la última \ o :)
        mov si, src
        mov di, sdir
        call dir_prefix
        ; buscar
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov dx, src
        mov cx, 0x16                ; también carpetas (solo sin comodines)
        mov ah, 0x4E
        int 0x21
        jnc .found
        mov si, msg_nf
        call puts
        mov ax, 0x4C01
        int 0x21
.found:
        cmp byte [dta+0x1E], '.'
        je .next
        test byte [dta+0x15], 0x10
        jz .file
        cmp byte [wild], 0          ; carpetas: solo por su nombre
        jne .next
.file:
        inc word [count]
        call one
.next:
        mov ah, 0x4F
        int 0x21
        jnc .found
.end:
        mov al, [errors]
        mov ah, 0x4C
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21

; mover el elemento de la DTA
one:
        ; origen completo
        mov si, sdir
        mov di, fsrc
        call strcpy
        mov si, dta+0x1E
        call strcpy
        ; destino
        mov si, dst
        mov di, fdst
        call strcpy
        cmp byte [dst_isdir], 0
        je .named
        cmp byte [di-1], '\'
        je .sl
        cmp byte [di-1], ':'
        je .sl
        mov byte [di], '\'
        inc di
.sl:
        mov si, dta+0x1E
        call strcpy
.named:
        ; mensaje «origen => destino»
        mov si, fsrc
        call puts
        mov si, msg_arrow
        call puts
        mov si, fdst
        call puts
        ; ¿origen y destino son el mismo archivo? (si se «sobrescribiera»,
        ; se borraría el único ejemplar)
        push ds
        pop es
        mov si, fsrc
        mov di, tn1
        mov ah, 0x60
        int 0x21
        jc .nsame
        mov si, fdst
        mov di, tn2
        mov ah, 0x60
        int 0x21
        jc .nsame
        mov si, tn1
        mov di, tn2
.cmp:
        lodsb
        cmp al, [di]
        jne .nsame
        inc di
        test al, al
        jnz .cmp
        mov byte [errors], 1
        mov si, msg_same
        jmp puts
.nsame:
        ; ¿existe el destino?
        mov dx, fdst
        mov ax, 0x4300
        int 0x21
        jc .go
        test cl, 0x10
        jnz .fail                   ; una carpeta con ese nombre
        cmp byte [opt_y], 0
        jne .over
        mov si, msg_ask
        call puts
        call ask_yes
        jc .skip
.over:
        mov dx, fdst
        mov ax, 0x4301
        xor cx, cx
        int 0x21
        mov ah, 0x41
        int 0x21
.go:
        ; misma unidad: renombrar
        call same_drive
        jne .copy
        mov dx, fsrc
        mov di, fdst
        push ds
        pop es
        mov ah, 0x56
        int 0x21
        jc .fail
        jmp .ok
.copy:
        test byte [dta+0x15], 0x10
        jnz .fail                   ; una carpeta no se mueve a otra unidad
        call copy_file
        jc .fail
        mov dx, fsrc
        mov ax, 0x4301
        xor cx, cx
        int 0x21
        mov ah, 0x41
        int 0x21
.ok:
        mov si, msg_ok
        jmp puts
.skip:
        mov si, msg_skip
        jmp puts
.fail:
        mov byte [errors], 1
        mov si, msg_fail
        jmp puts

; ¿fsrc y fdst en la misma unidad? -> ZF
same_drive:
        mov si, fsrc
        call drive_of
        mov bl, al
        mov si, fdst
        call drive_of
        cmp al, bl
        ret
; SI = ruta -> AL = unidad (A=0)
drive_of:
        cmp byte [si+1], ':'
        jne .cur
        mov al, [si]
        call upcase
        sub al, 'A'
        ret
.cur:
        mov ah, 0x19
        int 0x21
        ret

; copiar fsrc a fdst con la fecha y la hora -> CF
copy_file:
        mov dx, fsrc
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov [hs], ax
        mov dx, fdst
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .cl
        mov [hd], ax
.l:
        mov bx, [hs]
        mov dx, buf
        mov cx, BUFSZ
        mov ah, 0x3F
        int 0x21
        jc .both
        test ax, ax
        jz .eof
        mov cx, ax
        mov bx, [hd]
        mov ah, 0x40
        int 0x21
        jc .both
        cmp ax, cx
        jne .both
        jmp .l
.eof:
        mov bx, [hs]
        mov ax, 0x5700
        int 0x21
        mov bx, [hd]
        mov ax, 0x5701
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        clc
        ret
.both:
        mov bx, [hd]
        mov ah, 0x3E
        int 0x21
        mov dx, fdst
        mov ah, 0x41
        int 0x21
.cl:
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        stc
.r:
        ret

; siguiente argumento (las opciones /Y y /-Y se anotan) -> DI; CF si no hay
next_arg:
.sk:
        lodsb
        cmp al, ' '
        je .sk
        cmp al, 9
        je .sk
        cmp al, 13
        je .none
        cmp al, '/'
        jne .w
        lodsb
        cmp al, '-'
        jne .y
        lodsb
        call upcase
        cmp al, 'Y'
        jne .badopt
        mov byte [opt_y], 0
        jmp .sk
.y:
        call upcase
        cmp al, 'Y'
        jne .badopt
        mov byte [opt_y], 1
        jmp .sk
.badopt:
        jmp usage
.w:
        call upcase
        stosb
        lodsb
        cmp al, ' '
        je .e
        cmp al, 9
        je .e
        cmp al, 13
        jne .w
        dec si
.e:
        mov byte [di], 0
        clc
        ret
.none:
        dec si
        stc
        ret

; SI = texto -> AL = 1 si tiene * o ?
has_wild:
        xor al, al
.l:
        mov ah, [si]
        inc si
        test ah, ah
        jz .r
        cmp ah, '*'
        je .y
        cmp ah, '?'
        jne .l
.y:
        mov al, 1
.r:
        ret

; SI = ruta -> DI = carpeta (hasta la última \ o :, incluida)
dir_prefix:
        mov bx, di
.l:
        lodsb
        mov [di], al
        inc di
        test al, al
        jz .e
        cmp al, '\'
        je .s
        cmp al, ':'
        jne .l
.s:
        mov bx, di
        jmp .l
.e:
        mov byte [bx], 0
        ret

; DI <- SI (ASCIIZ); DI queda sobre el cero
strcpy:
        lodsb
        mov [di], al
        test al, al
        jz .e
        inc di
        jmp strcpy
.e:
        ret

%include "util.inc"

msg_usage   db "Uso: MOVER [/Y | /-Y] origen destino", 13, 10
            db "Mueve archivos (admite comodines) o renombra una carpeta.", 13, 10
            db "  /Y   no pregunta antes de sobrescribir", 13, 10, 0
msg_nf      db "No se encontró el archivo.", 13, 10, 0
msg_arrow   db " => ", 0
msg_ok      db " [correcto]", 13, 10, 0
msg_fail    db " [error]", 13, 10, 0
msg_skip    db " [no se mueve]", 13, 10, 0
msg_same    db " [es el mismo archivo: no se mueve]", 13, 10, 0
msg_ask     db 13, 10, "El archivo ya existe. ¿Sobrescribir? (S/N) ", 0
msg_multi   db "Para mover varios archivos, el destino tiene que ser una carpeta.", 13, 10, 0

        section .bss
bss_start:
opt_y       resb 1
wild        resb 1
dst_isdir   resb 1
errors      resb 1
count       resw 1
hs          resw 1
hd          resw 1
src         resb 128
dst         resb 128
extra       resb 128
sdir        resb 128
fsrc        resb 160
fdst        resb 160
tn1         resb 160
tn2         resb 160
dta         resb 128
u_numbuf    resb 16
u_lbuf      resb 84
buf         resb BUFSZ
            resb 512
stack_top:
prog_end:
