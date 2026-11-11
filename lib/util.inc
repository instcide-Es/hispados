; =============================================================================
;  HispaDOS - rutinas comunes de las utilidades (.COM)
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
; =============================================================================

; AL -> pantalla
putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret

; DS:SI = cadena ASCIIZ
puts:
        push ax
        push si
.l:
        lodsb
        test al, al
        jz .d
        call putc
        jmp .l
.d:
        pop si
        pop ax
        ret

crlf:
        push ax
        mov al, 13
        call putc
        mov al, 10
        call putc
        pop ax
        ret

; EAX sin relleno
putnum:
        push cx
        mov cl, 0
        call putnum_w
        pop cx
        ret

; EAX con separador de miles, alineado a la derecha en CL columnas
putnum_w:
        push eax
        push ebx
        push ecx
        push edx
        push si
        push di
        mov di, u_numbuf + 15
        mov byte [di], 0
        mov ebx, 10
        xor ch, ch
.l:
        xor edx, edx
        div ebx
        add dl, '0'
        cmp ch, 3
        jne .nc
        dec di
        mov byte [di], ','
        mov ch, 0
.nc:
        dec di
        mov [di], dl
        inc ch
        test eax, eax
        jnz .l
        mov ax, u_numbuf + 15
        sub ax, di
        movzx cx, cl
        sub cx, ax
        jle .p
.pad:
        mov al, ' '
        call putc
        loop .pad
.p:
        mov si, di
        call puts
        pop di
        pop si
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; EAX en hexadecimal con 4 cifras (AX)
puthex4:
        push ax
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
        pop ax
        ret

; esperar una tecla -> AL (sin eco)
getkey:
        mov ah, 0x08
        int 0x21
        ret

; leer una línea -> u_line (ASCIIZ), CX = longitud
readline:
        push dx
        push bx
        mov byte [u_lbuf], 80
        mov dx, u_lbuf
        mov ah, 0x0A
        int 0x21
        movzx cx, byte [u_lbuf+1]
        mov bx, cx
        mov byte [u_lbuf+2+bx], 0
        call crlf
        pop bx
        pop dx
        ret
u_line  equ u_lbuf + 2

upcase:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 32
.r:
        ret

; DS:SI = texto -> EAX = número decimal, SI avanza; CF si no hay cifras
parse_num:
        push ebx
        push edx
        xor eax, eax
        xor ebx, ebx
        mov dl, 0
.l:
        mov bl, [si]
        sub bl, '0'
        jb .e
        cmp bl, 9
        ja .e
        imul eax, eax, 10
        add eax, ebx
        inc si
        mov dl, 1
        jmp .l
.e:
        cmp dl, 1
        pop edx
        pop ebx
        ret                 ; CF=1 si DL=0 (sin cifras)

; ¿S/Y? -> CF=0 si la respuesta es sí
ask_yes:
        call readline
        mov al, [u_line]
        call upcase
        cmp al, 'Y'
        je .y
        cmp al, 'S'
        je .y
        stc
        ret
.y:
        clc
        ret
