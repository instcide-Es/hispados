; =============================================================================
;  Hispa-DOS BUSCAR.COM (FIND) - buscar un texto en archivos o en la entrada
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  BUSCAR [/V] [/C] [/N] [/I] "texto" [archivo ...]
;     /V  las líneas que NO contienen el texto
;     /C  solo cuántas líneas
;     /N  con el número de línea
;     /I  sin distinguir mayúsculas y minúsculas
;  Sin archivos lee la entrada estándar (TYPE x | BUSCAR "texto").
;  Código de salida: 0 encontrado, 1 no encontrado, 2 error.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

LINEMAX equ 1024
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
        mov byte [result], 1
        mov si, 0x81
        ; opciones y el texto entre comillas
.arg:
        lodsb
        cmp al, ' '
        je .arg
        cmp al, 9
        je .arg
        cmp al, 13
        je usage
        cmp al, '/'
        jne .q
        lodsb
        call upcase
        cmp al, 'V'
        jne .o1
        mov byte [opt_v], 1
        jmp .arg
.o1:
        cmp al, 'C'
        jne .o2
        mov byte [opt_c], 1
        jmp .arg
.o2:
        cmp al, 'N'
        jne .o3
        mov byte [opt_n], 1
        jmp .arg
.o3:
        cmp al, 'I'
        jne usage
        mov byte [opt_i], 1
        jmp .arg
.q:
        cmp al, '"'
        jne usage
        mov di, text
.t:
        lodsb
        cmp al, 13
        je usage
        cmp al, '"'
        jne .tc
        cmp byte [si], '"'          ; "" dentro del texto = una comilla
        jne .td
        inc si
.tc:
        stosb
        jmp .t
.td:
        mov byte [di], 0
        mov ax, di
        sub ax, text
        mov [tlen], ax
        ; las opciones también valen detrás del texto, como en MS-DOS
        ; (FIND "texto" /C archivo)
        push si
.po:
        lodsb
        cmp al, 13
        je .pod
        cmp al, '/'
        jne .po
        lodsb
        call upcase
        mov bx, opt_v
        cmp al, 'V'
        je .pos
        mov bx, opt_c
        cmp al, 'C'
        je .pos
        mov bx, opt_n
        cmp al, 'N'
        je .pos
        mov bx, opt_i
        cmp al, 'I'
        jne usage
.pos:
        mov byte [bx], 1
        jmp .po
.pod:
        pop si
        cmp byte [opt_i], 0
        je .files
        push si
        mov si, text
        call str_upper
        pop si
        ; archivos (o la entrada estándar)
.files:
        mov [argp], si
        mov byte [nfiles], 0
.fl:
        mov si, [argp]
.fs:
        lodsb
        cmp al, ' '
        je .fs
        cmp al, 9
        je .fs
        cmp al, 13
        je .fend
        cmp al, '/'                 ; una opción (ya apuntada): saltarla
        jne .f1
        inc si
        jmp .fs
.f1:
        mov di, fname
.fn:
        call upcase
        stosb
        lodsb
        cmp al, ' '
        jbe .fne
        cmp al, '/'
        jne .fn
.fne:
        dec si
        mov byte [di], 0
        mov [argp], si
        inc byte [nfiles]
        mov dx, fname
        mov ax, 0x3D00
        int 0x21
        jnc .open
        mov si, msg_nofile
        call puts
        mov si, fname
        call puts
        call crlf
        mov byte [result], 2
        jmp .fl
.open:
        mov [handle], ax
        call crlf
        mov si, s_dashes
        call puts
        mov si, fname
        call puts
        call scan
        mov bx, [handle]
        mov ah, 0x3E
        int 0x21
        jmp .fl
.fend:
        cmp byte [nfiles], 0
        jne .done
        mov word [handle], 0        ; entrada estándar
        mov byte [stdin], 1
        call scan
.done:
        mov al, [result]
        mov ah, 0x4C
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C02
        int 0x21

; recorrer las líneas de [handle]
scan:
        mov dword [lineno], 0
        mov dword [hits], 0
        mov word [bpos], 0
        mov word [blen], 0
        mov byte [ateof], 0
        cmp byte [opt_c], 0
        jne .l
        cmp byte [stdin], 0
        jne .l
        call crlf
.l:
        call getline
        jc .end
        inc dword [lineno]
        call match                  ; CF = contiene el texto
        mov al, 0
        adc al, 0
        xor al, [opt_v]
        jz .l
        inc dword [hits]
        mov byte [result], 0
        cmp byte [opt_c], 0
        jne .l
        cmp byte [opt_n], 0
        je .p
        mov al, '['
        call putc
        mov eax, [lineno]
        call putnum
        mov al, ']'
        call putc
.p:
        mov si, line
        call puts
        call crlf
        jmp .l
.end:
        cmp byte [opt_c], 0
        je .r
        cmp byte [stdin], 0
        jne .cnt
        mov al, ':'
        call putc
        mov al, ' '
        call putc
.cnt:
        mov eax, [hits]
        call putnum
        call crlf
.r:
        ret

; leer una línea de [handle] -> line (ASCIIZ, sin CR LF); CF al final
getline:
        cmp byte [ateof], 0         ; tras Ctrl+Z no se lee más
        je .go
        stc
        ret
.go:
        mov di, line
        xor cx, cx                  ; caracteres
        mov byte [gotany], 0
.l:
        mov bx, [bpos]
        cmp bx, [blen]
        jb .have
        ; llenar el búfer
        mov bx, [handle]
        mov dx, buf
        mov cx, BUFSZ
        mov ah, 0x3F
        int 0x21
        jc .eof
        test ax, ax
        jz .eof
        mov [blen], ax
        mov word [bpos], 0
        mov cx, di
        sub cx, line
        jmp .l
.have:
        mov al, [buf+bx]
        inc word [bpos]
        mov byte [gotany], 1
        cmp al, 10
        je .eol
        cmp al, 13
        je .l
        cmp al, 0x1A                ; fin de texto
        je .ctrlz
        mov cx, di
        sub cx, line
        cmp cx, LINEMAX - 1
        jae .l                      ; las líneas muy largas se cortan
        stosb
        jmp .l
.eol:
        mov byte [di], 0
        clc
        ret
.ctrlz:
        mov byte [ateof], 1
.eof:
        mov byte [di], 0
        mov word [blen], 0
        mov word [bpos], 0
        cmp di, line                ; última línea sin salto
        jne .last
        stc
        ret
.last:
        clc
        ret

; ¿line contiene text? -> CF = 1 si sí
match:
        mov si, line
        cmp word [tlen], 0          ; el texto vacío no está en ninguna línea
        je .no                      ; (FIND /V /C "" cuenta las líneas)
.o:
        cmp byte [si], 0
        je .no
        push si
        mov di, text
        mov cx, [tlen]
.c:
        mov al, [si]
        test al, al
        jz .nc
        cmp byte [opt_i], 0
        je .cs
        call upcase_ext
.cs:
        cmp al, [di]
        jne .nc
        inc si
        inc di
        loop .c
        pop si
.yes:
        stc
        ret
.nc:
        pop si
        inc si
        jmp .o
.no:
        clc
        ret

; SI: a mayúsculas (también ñ, vocales con tilde de CP437)
str_upper:
.l:
        mov al, [si]
        test al, al
        jz .r
        call upcase_ext
        mov [si], al
        inc si
        jmp .l
.r:
        ret
upcase_ext:
        call upcase
        cmp al, 0xA4                ; ñ -> Ñ
        jne .r
        mov al, 0xA5
.r:
        ret

%include "util.inc"

msg_usage   db "Uso: BUSCAR [/V] [/C] [/N] [/I] ", 34, "texto", 34, " [archivo ...]", 13, 10
            db "  /V  líneas que no lo contienen   /C  solo cuántas", 13, 10
            db "  /N  con el número de línea       /I  sin distinguir mayúsculas", 13, 10, 0
msg_nofile  db "No se encontró el archivo ", 0
s_dashes    db "---------- ", 0

        section .bss
bss_start:
opt_v       resb 1
opt_c       resb 1
opt_n       resb 1
opt_i       resb 1
stdin       resb 1
result      resb 1
nfiles      resb 1
gotany      resb 1
ateof       resb 1
            resb 1
tlen        resw 1
argp        resw 1
handle      resw 1
bpos        resw 1
blen        resw 1
lineno      resd 1
hits        resd 1
text        resb 128
fname       resb 128
line        resb LINEMAX
u_numbuf    resb 16
u_lbuf      resb 84
buf         resb BUFSZ
            resb 1024
stack_top:
prog_end:
