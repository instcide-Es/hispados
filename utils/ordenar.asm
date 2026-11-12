; =============================================================================
;  Hispa-DOS ORDENAR.COM (SORT) - ordenar líneas de texto
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  ORDENAR [/R] [/+n] [archivo]     (sin archivo: la entrada estándar)
;     /R   orden inverso
;     /+n  ordenar a partir de la columna n
;  Hasta 64 KB de texto, como el SORT de MS-DOS, y 24000 líneas; si hay más,
;  lo avisa (por la salida de errores) y termina con ERRORLEVEL 1. No
;  distingue mayúsculas de minúsculas.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

MAXLINES equ 24000
OBUFSZ   equ 2048

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
        mov word [col], 0
        mov si, 0x81
.arg:
        lodsb
        cmp al, ' '
        je .arg
        cmp al, 9
        je .arg
        cmp al, 13
        je .argd
        cmp al, '/'
        jne .file
        lodsb
        cmp al, '+'
        jne .r
        call parse_num
        jc usage
        test ax, ax
        jz .arg
        dec ax
        mov [col], ax
        jmp .arg
.r:
        call upcase
        cmp al, 'R'
        jne usage
        mov byte [opt_r], 1
        jmp .arg
.file:
        mov di, fname
.fn:
        stosb
        lodsb
        cmp al, ' '
        ja .fn
        dec si
        mov byte [di], 0
        jmp .arg
.argd:
        mov word [handle], 0
        cmp byte [fname], 0
        je .mem
        mov dx, fname
        mov ax, 0x3D00
        int 0x21
        jnc .op
        mov si, msg_nofile
        call eputs
        mov ax, 0x4C01
        int 0x21
.op:
        mov [handle], ax
.mem:
        ; un bloque de 64 KB para el texto
        mov bx, 0x1000
        mov ah, 0x48
        int 0x21
        jnc .gotm
        mov si, msg_nomem
        call eputs
        mov ax, 0x4C01
        int 0x21
.gotm:
        mov [tseg], ax
        ; leer todo
        xor di, di                  ; usado
.rd:
        mov cx, 0xFFF0
        sub cx, di
        jz .full
        cmp cx, 0x4000
        jbe .c
        mov cx, 0x4000
.c:
        push ds
        mov bx, [handle]
        mov dx, di
        mov ds, [tseg]
        mov ah, 0x3F
        int 0x21
        pop ds
        jc .rdd
        test ax, ax
        jz .rdd
        add di, ax
        jmp .rd
.full:
        ; el aviso, a la salida de errores: la normal puede ser un archivo
        mov byte [trunc], 1
        mov si, msg_big
        call eputs
.rdd:
        mov [tlen], di
        ; partir en líneas: cada una termina en 0
        push es
        mov es, [tseg]
        xor si, si
        xor bx, bx                  ; número de líneas
.ln:
        cmp si, [tlen]
        jae .lnd
        cmp byte [es:si], 0x1A      ; Ctrl+Z tras el último salto: no es
        je .lnd                     ; una línea vacía más
        cmp bx, MAXLINES
        jb .lnok
        mov byte [many], 1          ; no caben más líneas: se avisa luego
        jmp .lnd
.lnok:
        push bx
        shl bx, 1
        mov [lines+bx], si
        pop bx
        inc bx
.lc:
        cmp si, [tlen]
        jae .lce
        mov al, [es:si]
        cmp al, 0x1A
        jne .l1
        mov [tlen], si              ; fin de texto
        jmp .lce
.l1:
        cmp al, 10
        je .lf
        cmp al, 13
        jne .ln1
        mov byte [es:si], 0
.ln1:
        inc si
        jmp .lc
.lf:
        mov byte [es:si], 0
        inc si
        jmp .ln
.lce:
        ; la última línea sin salto: terminarla (el bloque es de 64 KB
        ; enteros y el texto no pasa de FFF0h, así que siempre cabe)
        mov byte [es:si], 0
.lnd:
        pop es
        ; la última «línea» vacía tras el último salto no cuenta
        mov [nlines], bx
        cmp byte [many], 0
        je .sort
        mov byte [trunc], 1
        mov si, msg_many
        call eputs
.sort:
        ; ordenar (Shell)
        call shell_sort
        ; escribir
        xor bx, bx
.w:
        cmp bx, [nlines]
        jae .end
        push bx
        shl bx, 1
        mov si, [lines+bx]
        call write_line
        pop bx
        inc bx
        jmp .w
.end:
        call flush_out
        mov al, [trunc]             ; 1 si no se pudo ordenar todo
        mov ah, 0x4C
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21

; DS:SI = mensaje ASCIIZ -> salida de errores (no se redirige con > ni |)
eputs:
        mov dx, si
        xor cx, cx
.l:
        cmp byte [si], 0
        je .w
        inc si
        inc cx
        jmp .l
.w:
        mov bx, 2
        mov ah, 0x40
        int 0x21
        ret

; escribir la línea [tseg]:SI y CR LF. Pasa por un búfer: con dos llamadas
; a DOS por línea, ordenar 9000 líneas a un archivo tardaba casi un minuto.
write_line:
        push es
        mov es, [tseg]
.l:
        mov al, [es:si]
        test al, al
        jz .e
        inc si
        call out_byte
        jmp .l
.e:
        pop es
        mov al, 13
        call out_byte
        mov al, 10
; AL -> búfer de salida (se vacía al llenarse)
out_byte:
        mov di, [opos]
        mov [obuf+di], al
        inc word [opos]
        cmp word [opos], OBUFSZ
        jb out_ret
; vaciar el búfer en la salida estándar
flush_out:
        mov cx, [opos]
        jcxz out_ret
        mov dx, obuf
        mov bx, 1
        mov ah, 0x40
        int 0x21
        mov word [opos], 0
out_ret:
        ret

; ordenar lines[0..nlines) (Shell, huecos n/2, n/4...)
shell_sort:
        mov ax, [nlines]
        shr ax, 1
        mov [gap], ax
.g:
        cmp word [gap], 0
        je .r
        mov cx, [gap]               ; i
.i:
        cmp cx, [nlines]
        jae .gn
        mov bx, cx
        shl bx, 1
        mov ax, [lines+bx]
        mov [tmp], ax
        mov dx, cx                  ; j
.j:
        cmp dx, [gap]
        jb .put
        mov bx, dx
        sub bx, [gap]
        shl bx, 1
        mov si, [lines+bx]          ; lines[j-gap]
        mov di, [tmp]
        call compare                ; CF = lines[j-gap] > tmp
        jnc .put
        mov ax, [lines+bx]
        mov bx, dx
        shl bx, 1
        mov [lines+bx], ax
        sub dx, [gap]
        jmp .j
.put:
        mov bx, dx
        shl bx, 1
        mov ax, [tmp]
        mov [lines+bx], ax
        inc cx
        jmp .i
.gn:
        shr word [gap], 1
        jmp .g
.r:
        ret

; ¿línea SI va detrás de la línea DI? (con /R al revés) -> CF
compare:
        push cx
        push dx
        push es
        mov es, [tseg]
        ; columna de inicio
        mov cx, [col]
.cs:
        jcxz .cd
        cmp byte [es:si], 0
        je .cs1
        inc si
.cs1:
        cmp byte [es:di], 0
        je .cs2
        inc di
.cs2:
        loop .cs
.cd:
.l:
        mov al, [es:si]
        call fold
        mov ah, al
        mov al, [es:di]
        call fold
        cmp ah, al
        jne .diff
        test al, al
        jz .eq
        inc si
        inc di
        jmp .l
.diff:
        ; AH (de SI) frente a AL (de DI)
        cmp byte [opt_r], 0
        jne .rev
        cmp al, ah                  ; CF si AL < AH, es decir SI > DI
        jmp .out
.rev:
        cmp ah, al                  ; al revés: CF si SI < DI
        jmp .out
.eq:
        clc
.out:
        pop es
        pop dx
        pop cx
        ret

; AL -> mayúscula (también ñ y vocales con tilde en CP437, a su vocal)
fold:
        call upcase
        cmp al, 0xA4                ; ñ
        jne .r
        mov al, 0xA5
.r:
        ret

%include "util.inc"

msg_usage   db "Uso: ORDENAR [/R] [/+n] [archivo]", 13, 10
            db "  /R   orden inverso   /+n  desde la columna n", 13, 10, 0
msg_nofile  db "No se encontró el archivo.", 13, 10, 0
msg_nomem   db "No hay memoria suficiente.", 13, 10, 0
msg_big     db "El texto tiene más de 64 KB: se ordena solo el principio.", 13, 10, 0
msg_many    db "El texto tiene más de 24000 líneas: se ordena solo el principio.", 13, 10, 0

        section .bss
bss_start:
opt_r       resb 1
trunc       resb 1
many        resb 1
            resb 1
col         resw 1
handle      resw 1
tseg        resw 1
tlen        resw 1
nlines      resw 1
gap         resw 1
tmp         resw 1
opos        resw 1
obuf        resb OBUFSZ
fname       resb 128
u_numbuf    resb 16
u_lbuf      resb 84
lines       resw MAXLINES
            resb 1024
stack_top:
prog_end:
