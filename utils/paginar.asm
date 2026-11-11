; =============================================================================
;  Hispa-DOS PAGINAR.COM (MORE) - mostrar texto pantalla a pantalla
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  PAGINAR [archivo]       orden | PAGINAR       PAGINAR < archivo
;  En cada pantalla espera una tecla («-- Más --»); Esc o Q terminan.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

BUFSZ   equ 8192

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
        ; tamaño de la pantalla (BIOS)
        push es
        xor ax, ax
        mov es, ax
        mov al, [es:0x484]
        test al, al
        jnz .rows
        mov al, 24
.rows:
        mov [rows], al              ; filas - 1: líneas antes de parar
        mov ax, [es:0x44A]
        test ax, ax
        jnz .cols
        mov ax, 80
.cols:
        mov [cols], al
        pop es
        ; ¿archivo?
        mov word [handle], 0
        mov si, 0x81
.sk:
        lodsb
        cmp al, ' '
        je .sk
        cmp al, 9
        je .sk
        cmp al, 13
        je .go
        cmp al, '/'
        je usage
        mov di, fname
.fn:
        stosb
        lodsb
        cmp al, ' '
        ja .fn
        mov byte [di], 0
        mov dx, fname
        mov ax, 0x3D00
        int 0x21
        jnc .op
        mov si, msg_nofile
        call puts
        mov ax, 0x4C01
        int 0x21
.op:
        mov [handle], ax
.go:
        mov byte [line], 0
        mov byte [col], 0
.rd:
        mov bx, [handle]
        mov dx, buf
        mov cx, BUFSZ
        mov ah, 0x3F
        int 0x21
        jc .end
        test ax, ax
        jz .end
        mov cx, ax
        mov si, buf
.ch:
        lodsb
        cmp al, 0x1A
        je .end
        call out_char
        jc .end
        loop .ch
        jmp .rd
.end:
        mov ax, 0x4C00
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21

; escribir AL contando líneas y columnas; CF si el usuario sale
out_char:
        cmp al, 13
        je .cr
        cmp al, 10
        je .lf
        cmp al, 9
        je .tab
        call emit
        inc byte [col]
        mov ah, [col]
        cmp ah, [cols]
        jb .ok
        ; la línea ocupa toda la pantalla: la pantalla pasa a la siguiente
        mov byte [col], 0
        jmp .newline
.tab:
        mov al, ' '
        call emit
        inc byte [col]
        mov ah, [col]
        cmp ah, [cols]
        jae .ok
        test ah, 7
        jnz .tab
        jmp .ok
.cr:
        call emit
        mov byte [col], 0
        jmp .ok
.lf:
        call emit
        mov byte [col], 0
.newline:
        inc byte [line]
        mov ah, [line]
        cmp ah, [rows]
        jb .ok
        mov byte [line], 0
        ; «-- Más --» y una tecla del teclado (la entrada puede ser un archivo)
        push si
        push cx
        mov si, msg_more
        call puts_raw
        xor ah, ah
        int 0x16
        push ax
        mov si, msg_clear
        call puts_raw
        pop ax
        pop cx
        pop si
        cmp al, 27
        je .quit
        and al, 0xDF
        cmp al, 'Q'
        je .quit
.ok:
        clc
        ret
.quit:
        stc
        ret

; AL a la salida estándar
emit:
        push ax
        push bx
        push cx
        push dx
        mov [ch1], al
        mov dx, ch1
        mov cx, 1
        mov bx, 1
        mov ah, 0x40
        int 0x21
        pop dx
        pop cx
        pop bx
        pop ax
        ret
puts_raw:
        lodsb
        test al, al
        jz .r
        call emit
        jmp puts_raw
.r:
        ret

%include "util.inc"

msg_usage   db "Uso: PAGINAR [archivo]   o   orden | PAGINAR   o   PAGINAR < archivo", 13, 10
            db "Muestra el texto pantalla a pantalla; Esc o Q terminan.", 13, 10, 0
msg_nofile  db "No se encontró el archivo.", 13, 10, 0
msg_more    db "-- Más --", 0
msg_clear   db 13, "         ", 13, 0

        section .bss
bss_start:
rows        resb 1
cols        resb 1
line        resb 1
col         resb 1
ch1         resb 1
            resb 1
handle      resw 1
fname       resb 128
u_numbuf    resb 16
u_lbuf      resb 84
buf         resb BUFSZ
            resb 1024
stack_top:
prog_end:
