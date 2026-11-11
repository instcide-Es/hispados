; =============================================================================
;  Hispa-DOS ATRIBUTO.COM (ATTRIB) - ver y cambiar los atributos de archivos
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  ATRIBUTO [+R|-R] [+A|-A] [+S|-S] [+H|-H] [[unidad:][ruta]archivo] [/S]
;     R solo lectura, A archivo, S sistema, H oculto
;     /S  también en las subcarpetas
; =============================================================================
        cpu 386
        bits 16
        org 0x100

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
        ; argumentos
        mov si, 0x81
.arg:
        lodsb
        cmp al, ' '
        je .arg
        cmp al, 9
        je .arg
        cmp al, 13
        je .argd
        cmp al, '+'
        je .plus
        cmp al, '-'
        je .minus
        cmp al, '/'
        je .opt
        ; nombre de archivo
        cmp byte [pat], 0
        jne usage
        mov di, pat
.n:
        call upcase
        stosb
        lodsb
        cmp al, ' '
        jbe .ne
        cmp al, '/'
        jne .n
.ne:
        dec si
        mov byte [di], 0
        jmp .arg
.plus:
        call attr_bit
        jc usage
        or [set_bits], al
        jmp .arg
.minus:
        call attr_bit
        jc usage
        or [clr_bits], al
        jmp .arg
.opt:
        lodsb
        call upcase
        cmp al, 'S'
        jne usage
        mov byte [opt_s], 1
        jmp .arg
.argd:
        cmp byte [pat], 0
        jne .havepat
        mov dword [pat], '*.*'
.havepat:
        ; separar carpeta y máscara
        mov si, pat
        mov di, path
        mov bx, di
.sp:
        lodsb
        mov [di], al
        inc di
        test al, al
        jz .spd
        cmp al, '\'
        je .sps
        cmp al, ':'
        jne .sp
.sps:
        mov bx, di
        jmp .sp
.spd:
        ; máscara = lo que sigue a la carpeta
        mov si, bx
        mov di, mask
.mk:
        lodsb
        stosb
        test al, al
        jnz .mk
        mov byte [bx], 0
        mov di, bx
        mov [pend], di
        ; ¿el nombre era una carpeta sin comodines? -> sus archivos no; ella
        call walk
        cmp word [found], 0
        jne .ok
        mov si, msg_nf
        call puts
        mov ax, 0x4C01
        int 0x21
.ok:
        mov ax, 0x4C00
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21

; AL = letra tras + o - (en [SI]) -> AL = bit; CF si no vale
attr_bit:
        lodsb
        call upcase
        mov ah, al
        mov al, 1
        cmp ah, 'R'
        je .ok
        mov al, 0x20
        cmp ah, 'A'
        je .ok
        mov al, 4
        cmp ah, 'S'
        je .ok
        mov al, 2
        cmp ah, 'H'
        je .ok
        stc
        ret
.ok:
        clc
        ret

; path (hasta pend) + mask: archivos; con /S, las subcarpetas
walk:
        push bp
        sub sp, 44
        mov bp, sp
        mov dx, bp
        mov ah, 0x1A
        int 0x21
        ; archivos
        mov di, [pend]
        mov si, mask
        call strcpy
        mov dx, path
        mov cx, 0x27
        mov ah, 0x4E
        int 0x21
        jc .subs
.f:
        call one
        mov ah, 0x4F
        int 0x21
        jnc .f
.subs:
        cmp byte [opt_s], 0
        je .r
        mov di, [pend]
        mov dword [di], '*.*'
        mov dx, path
        mov cx, 0x16
        mov ah, 0x4E
        int 0x21
        jc .r
.d:
        test byte [bp+0x15], 0x10
        jz .dn
        cmp byte [bp+0x1E], '.'
        je .dn
        ; entrar en la subcarpeta
        mov di, [pend]
        push di
        lea si, [bp+0x1E]
        call strcpy
        mov byte [di], '\'
        inc di
        mov [pend], di
        cmp di, path + 200
        jae .back
        call walk
.back:
        pop di
        mov [pend], di
        mov dx, bp                  ; la DTA de este nivel
        mov ah, 0x1A
        int 0x21
.dn:
        mov ah, 0x4F
        int 0x21
        jnc .d
.r:
        add sp, 44
        pop bp
        ret

; el archivo de la DTA [bp]: cambiar y/o mostrar
one:
        inc word [found]
        ; ruta completa del archivo
        mov di, [pend]
        lea si, [bp+0x1E]
        call strcpy
        mov al, [bp+0x15]
        mov ah, [set_bits]
        or ah, [clr_bits]
        jz .show
        ; cambiar
        or al, [set_bits]
        mov ah, [clr_bits]
        not ah
        and al, ah
        and al, 0x27
        movzx cx, al
        mov dx, path
        mov ax, 0x4301
        int 0x21
        jnc .r
        mov si, msg_err
        call puts
        mov si, path
        call puts
        jmp crlf
.show:
        ; «A    SHR     C:\RUTA\NOMBRE»
        mov bl, al
        mov al, ' '
        test bl, 0x20
        jz .a
        mov al, 'A'
.a:
        call putc
        mov si, s_sp4
        call puts
        mov al, ' '
        test bl, 4
        jz .s
        mov al, 'S'
.s:
        call putc
        mov al, ' '
        test bl, 2
        jz .h
        mov al, 'H'
.h:
        call putc
        mov al, ' '
        test bl, 1
        jz .ro
        mov al, 'R'
.ro:
        call putc
        mov si, s_sp5
        call puts
        ; ruta completa (INT 21h 60h)
        push es
        push ds
        pop es
        mov si, path
        mov di, full
        mov ah, 0x60
        int 0x21
        pop es
        mov si, full
        jnc .p
        mov si, path
.p:
        call puts
        call crlf
.r:
        ret

; DI <- SI; DI queda sobre el cero
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

msg_usage   db "Uso: ATRIBUTO [+R|-R] [+A|-A] [+S|-S] [+H|-H] [archivo] [/S]", 13, 10
            db "  R solo lectura   A archivo   S sistema   H oculto", 13, 10
            db "  /S  también en las subcarpetas", 13, 10, 0
msg_nf      db "No se encontró el archivo.", 13, 10, 0
msg_err     db "No se pueden cambiar los atributos de ", 0
s_sp4       db "    ", 0
s_sp5       db "     ", 0

        section .bss
bss_start:
set_bits    resb 1
clr_bits    resb 1
opt_s       resb 1
            resb 1
found       resw 1
pend        resw 1
pat         resb 128
mask        resb 16
path        resb 260
full        resb 128
u_numbuf    resb 16
u_lbuf      resb 84
            resb 2048
stack_top:
prog_end:
