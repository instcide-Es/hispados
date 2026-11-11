; =============================================================================
;  Hispa-DOS ARBOL.COM (TREE) - árbol de carpetas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  ARBOL [unidad:][ruta] [/F] [/A]
;     /F  también los archivos de cada carpeta
;     /A  con caracteres ASCII (+---, \---, |) en lugar de líneas
; =============================================================================
        cpu 386
        bits 16
        org 0x100

MAXDEPTH equ 24

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
        je .opt
        cmp byte [start_path], 0
        jne usage
        mov di, start_path
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
.opt:
        lodsb
        call upcase
        cmp al, 'F'
        jne .oa
        mov byte [opt_f], 1
        jmp .arg
.oa:
        cmp al, 'A'
        jne usage
        mov byte [opt_a], 1
        jmp .arg
.argd:
        cmp byte [opt_a], 0
        je .lines
        mov dword [s_tee], '+---'
        mov dword [s_end], '\---'
        mov dword [s_bar], '|   '
.lines:
        cmp byte [start_path], 0
        jne .hp
        mov word [start_path], '.'
.hp:
        ; ruta completa de la carpeta de partida
        push ds
        pop es
        mov si, start_path
        mov di, path
        mov ah, 0x60
        int 0x21
        jnc .tn
        mov si, msg_bad
        call puts
        mov ax, 0x4C01
        int 0x21
.tn:
        ; cabecera: etiqueta y número de serie
        mov al, [path]
        mov [s_vol], al
        mov [drv_letter], al
        mov si, msg_head
        call puts
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov dx, s_vol
        mov cx, 8
        mov ah, 0x4E
        int 0x21
        jc .nolab
        mov si, dta+0x1E
.lab:
        lodsb
        test al, al
        jz .labe
        cmp al, '.'
        je .lab
        call putc
        jmp .lab
.nolab:
        mov si, msg_nolab
        call puts
.labe:
        call crlf
        ; número de serie (IOCTL 440Dh/66h)
        mov bl, [drv_letter]
        sub bl, 'A' - 1
        mov dx, mid
        mov ax, 0x440D
        mov cx, 0x0866
        int 0x21
        jc .noser
        mov si, msg_ser
        call puts
        mov ax, [mid+4]
        call hex4
        mov al, '-'
        call putc
        mov ax, [mid+2]
        call hex4
        call crlf
.noser:
        ; la raíz del árbol
        mov si, path
        call puts
        call crlf
        ; path termina en \ para añadir nombres
        mov di, path
.e:
        cmp byte [di], 0
        je .ee
        inc di
        jmp .e
.ee:
        cmp byte [di-1], '\'
        je .es
        mov byte [di], '\'
        inc di
.es:
        mov [pend], di
        mov word [pfx_end], prefix
        mov byte [prefix], 0
        call level
        cmp word [ndirs], 0
        jne .ok
        cmp byte [opt_f], 0
        jne .ok
        mov si, msg_nosub
        call puts
.ok:
        mov ax, 0x4C00
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21

; una carpeta (path hasta pend): sus archivos (/F) y sus subcarpetas
level:
        push bp
        sub sp, 54
        mov bp, sp
        mov word [bp+44], 0             ; subcarpetas
        mov dx, bp
        mov ah, 0x1A
        int 0x21
        ; contar las subcarpetas
        call find_all
        jc .count_d
.c:
        call is_subdir
        jc .cn
        inc word [bp+44]
.cn:
        mov ah, 0x4F
        int 0x21
        jnc .c
.count_d:
        ; archivos (/F)
        cmp byte [opt_f], 0
        je .dirs
        call find_all
        jc .fdone
        mov byte [bp+48], 0             ; ¿ha habido alguno?
.f:
        test byte [bp+0x15], 0x18       ; ni carpetas ni etiqueta
        jnz .fn
        mov byte [bp+48], 1
        mov si, prefix
        call puts
        mov si, s_bar
        cmp word [bp+44], 0
        jne .fb
        mov si, s_sp4
.fb:
        call puts
        lea si, [bp+0x1E]
        call puts
        call crlf
.fn:
        mov ah, 0x4F
        int 0x21
        jnc .f
        cmp byte [bp+48], 0
        je .fdone
        ; línea en blanco tras los archivos
        mov si, prefix
        call puts
        cmp word [bp+44], 0
        je .fbl
        mov si, s_bar
        call puts
.fbl:
        call crlf
.fdone:
.dirs:
        ; las subcarpetas, una a una
        cmp word [bp+44], 0
        je .r
        mov word [bp+46], 0             ; índice
        call find_all
        jc .r
.d:
        call is_subdir
        jc .dn
        inc word [ndirs]
        inc word [bp+46]
        ; «├───NOMBRE» o «└───NOMBRE» (la última)
        mov si, prefix
        call puts
        mov ax, [bp+46]
        cmp ax, [bp+44]
        mov si, s_tee
        jne .t
        mov si, s_end
.t:
        call puts
        lea si, [bp+0x1E]
        call puts
        call crlf
        ; prefijo de los hijos: «│   » o «    »
        mov di, [pfx_end]
        push di
        mov si, s_bar
        mov ax, [bp+46]
        cmp ax, [bp+44]
        jne .pb
        mov si, s_sp4
.pb:
        call strcpy
        mov [pfx_end], di
        ; ruta de la subcarpeta
        mov di, [pend]
        push di
        lea si, [bp+0x1E]
        call strcpy
        mov byte [di], '\'
        inc di
        mov [pend], di
        inc byte [depth]
        cmp byte [depth], MAXDEPTH
        jae .back
        call level
.back:
        dec byte [depth]
        pop di
        mov [pend], di
        pop di
        mov [pfx_end], di
        mov byte [di], 0
        mov dx, bp
        mov ah, 0x1A
        int 0x21
.dn:
        mov ah, 0x4F
        int 0x21
        jnc .d
.r:
        add sp, 54
        pop bp
        ret

; buscar «*.*» en la carpeta actual del recorrido (DTA en [bp]) -> CF
find_all:
        mov di, [pend]
        mov dword [di], '*.*'
        mov dx, path
        mov cx, 0x16
        mov ah, 0x4E
        int 0x21
        ret
; ¿la entrada de la DTA [bp] es una subcarpeta (no . ni ..)? CF si no
is_subdir:
        test byte [bp+0x15], 0x10
        jz .no
        cmp byte [bp+0x1E], '.'
        je .no
        clc
        ret
.no:
        stc
        ret

strcpy:
        lodsb
        mov [di], al
        test al, al
        jz .e
        inc di
        jmp strcpy
.e:
        ret
hex4:
        push cx
        mov cx, 4
.l:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .l
        pop cx
        ret

%include "util.inc"

msg_usage   db "Uso: ARBOL [unidad:][ruta] [/F] [/A]", 13, 10
            db "  /F  también los archivos   /A  caracteres ASCII", 13, 10, 0
msg_bad     db "Ruta no válida.", 13, 10, 0
msg_head    db "Listado de carpetas del volumen ", 0
msg_nolab   db "(sin etiqueta)", 0
msg_ser     db "El número de serie del volumen es ", 0
msg_nosub   db "No hay subcarpetas.", 13, 10, 0
s_vol       db "A:\*.*", 0
; líneas (CP437) y su versión ASCII se eligen al empezar
s_tee       db 0xC3, 0xC4, 0xC4, 0xC4, 0
s_end       db 0xC0, 0xC4, 0xC4, 0xC4, 0
s_bar       db 0xB3, "   ", 0
s_sp4       db "    ", 0

        section .bss
bss_start:
opt_f       resb 1
opt_a       resb 1
depth       resb 1
drv_letter  resb 1
ndirs       resw 1
pend        resw 1
pfx_end     resw 1
mid         resb 26
start_path  resb 128
path        resb 260
prefix      resb MAXDEPTH*4 + 4
dta         resb 64
u_numbuf    resb 16
u_lbuf      resb 84
            resb 4096
stack_top:
prog_end:
