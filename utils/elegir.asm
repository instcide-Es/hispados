; =============================================================================
;  Hispa-DOS ELEGIR.COM (CHOICE) - preguntar en los archivos por lotes
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  ELEGIR [/C[:]teclas] [/N] [/S] [/T[:]c,nn] [texto]
;     /C  teclas válidas (por omisión SN)
;     /N  no mostrar las teclas ([S,N]?)
;     /S  distinguir mayúsculas y minúsculas
;     /T  responder c solo, pasados nn segundos (0-99)
;  ERRORLEVEL = posición de la tecla elegida (1 la primera); 255 si error.
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
        ; Ctrl+C: terminar con ERRORLEVEL 0, como el CHOICE de MS-DOS
        mov dx, on_break
        mov ax, 0x2523
        int 0x21
        mov word [keys], 'SN'
        mov byte [keys+2], 0
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
        jne .text
        lodsb
        call upcase
        cmp al, 'C'
        je .optc
        cmp al, 'N'
        je .optn
        cmp al, 'S'
        je .opts
        cmp al, 'T'
        je .optt
        jmp usage
.optc:
        cmp byte [si], ':'
        jne .c1
        inc si
.c1:
        mov di, keys
.c2:
        lodsb
        cmp al, ' '
        jbe .c3
        cmp al, '/'
        je .c3
        stosb
        cmp di, keys + 40
        jb .c2
.c3:
        dec si
        mov byte [di], 0
        cmp di, keys
        je usage
        jmp .arg
.optn:
        mov byte [opt_n], 1
        jmp .arg
.opts:
        mov byte [opt_s], 1
        jmp .arg
.optt:
        cmp byte [si], ':'
        jne .t1
        inc si
.t1:
        lodsb
        mov [tdef], al
        cmp byte [si], ','
        jne usage
        inc si
        call parse_num
        jc usage
        cmp ax, 99
        ja usage
        mov [tsec], ax
        mov byte [opt_t], 1
        jmp .arg
.text:
        ; el resto de la línea es el texto
        dec si
        mov di, text
.tx:
        lodsb
        cmp al, 13
        je .txe
        stosb
        jmp .tx
.txe:
        mov byte [di], 0
.argd:
        ; sin /S, las teclas en mayúsculas
        cmp byte [opt_s], 0
        jne .k
        mov si, keys
.ku:
        mov al, [si]
        test al, al
        jz .k
        call upcase
        mov [si], al
        inc si
        jmp .ku
.k:
        ; la tecla por omisión de /T tiene que ser una de las válidas
        cmp byte [opt_t], 0
        je .show
        mov al, [tdef]
        cmp byte [opt_s], 0
        jne .td
        call upcase
        mov [tdef], al
.td:
        call key_index
        jc usage
.show:
        mov si, text
        call puts
        cmp byte [opt_n], 0
        jne .wait
        ; «[S,N]?»
        mov al, '['
        call putc
        mov si, keys
.sk:
        lodsb
        call putc
        cmp byte [si], 0
        je .ske
        mov al, ','
        call putc
        jmp .sk
.ske:
        mov al, ']'
        call putc
        mov al, '?'
        call putc
.wait:
        ; plazo en ticks (18,2 por segundo)
        cmp byte [opt_t], 0
        je .key
        mov ax, [tsec]
        mov cx, 182
        mul cx
        mov cx, 10
        div cx
        mov [tticks], ax
        push es
        xor bx, bx
        mov es, bx
        mov ax, [es:0x46C]
        pop es
        mov [t0], ax
.poll:
        mov ah, 0x0B                ; ¿hay tecla?
        int 0x21
        test al, al
        jnz .key
        push es
        xor bx, bx
        mov es, bx
        mov ax, [es:0x46C]
        pop es
        sub ax, [t0]
        cmp ax, [tticks]
        jb .poll
        mov al, [tdef]
        jmp .got
.key:
        mov ah, 0x08
        int 0x21
        test al, al
        jnz .k2
        mov ah, 0x08                ; tecla extendida: se descarta
        int 0x21
        jmp .bad
.k2:
        cmp byte [opt_s], 0
        jne .got
        call upcase
.got:
        call key_index
        jnc .ok
.bad:
        mov al, 7                   ; pitido
        call putc
        cmp byte [opt_t], 0
        je .key
        jmp .poll
.ok:
        push bx
        call putc
        call crlf
        pop ax
        mov ah, 0x4C
        int 0x21

on_break:
        mov ax, 0x4C00
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4CFF
        int 0x21

; AL en keys -> BL = posición (1...); CF si no está
key_index:
        mov si, keys
        mov bl, 1
.l:
        mov ah, [si]
        test ah, ah
        jz .no
        cmp ah, al
        je .yes
        inc si
        inc bl
        jmp .l
.no:
        stc
        ret
.yes:
        clc
        ret

%include "util.inc"

msg_usage   db "Uso: ELEGIR [/C[:]teclas] [/N] [/S] [/T[:]c,nn] [texto]", 13, 10
            db "  /C teclas válidas (SN)   /N sin mostrarlas   /S distinguir mayúsculas", 13, 10
            db "  /T responder c pasados nn segundos", 13, 10
            db "ERRORLEVEL = posición de la tecla elegida.", 13, 10, 0

        section .bss
bss_start:
opt_n       resb 1
opt_s       resb 1
opt_t       resb 1
tdef        resb 1
tsec        resw 1
tticks      resw 1
t0          resw 1
keys        resb 44
text        resb 130
u_numbuf    resb 16
u_lbuf      resb 84
            resb 512
stack_top:
