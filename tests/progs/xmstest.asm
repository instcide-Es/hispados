; XMSTEST.COM - prueba del gestor XMS: versión, memoria libre, reservar,
; copiar ida y vuelta, comparar, bloquear y liberar.
        cpu 386
        bits 16
        org 0x100
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne nox
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        push cs
        pop es
        mov ah, 0
        call far [xms]
        mov si, m_ver
        call puts
        call hex4
        mov si, m_free
        call puts
        mov ah, 8
        call far [xms]
        push dx
        movzx eax, ax
        call dec32
        mov si, m_tot
        call puts
        pop ax
        movzx eax, ax
        call dec32
        mov si, m_crlf
        call puts
        ; reservar 64 KB
        mov ah, 9
        mov dx, 64
        call far [xms]
        test ax, ax
        jz fail
        mov [handle], dx
        ; patrón en buf y copia a XMS (desplazamiento 1000h)
        mov di, buf
        mov cx, 4096
        xor al, al
.p:
        stosb
        add al, 7
        loop .p
        mov dword [mv+0], 4096
        mov word [mv+4], 0
        mov word [mv+6], buf
        mov [mv+8], cs
        mov ax, [handle]
        mov [mv+10], ax
        mov dword [mv+12], 0x1000
        mov si, mv
        mov ah, 0x0B
        call far [xms]
        test ax, ax
        jz fail
        ; borrar buf y traerlo de vuelta
        mov di, buf
        mov cx, 4096
        xor al, al
        rep stosb
        mov ax, [handle]
        mov [mv+4], ax
        mov dword [mv+6], 0x1000
        mov word [mv+10], 0
        mov word [mv+12], buf
        mov [mv+14], cs
        mov si, mv
        mov ah, 0x0B
        call far [xms]
        test ax, ax
        jz fail
        ; comparar
        mov si, buf
        mov cx, 4096
        xor al, al
.c:
        cmp [si], al
        jne fail
        inc si
        add al, 7
        loop .c
        ; bloquear y mostrar la dirección
        mov dx, [handle]
        mov ah, 0x0C
        call far [xms]
        test ax, ax
        jz fail
        push bx
        mov si, m_lock
        call puts
        mov ax, dx
        call hex4
        pop ax
        call hex4
        mov si, m_crlf
        call puts
        mov dx, [handle]
        mov ah, 0x0D
        call far [xms]
        mov dx, [handle]
        mov ah, 0x0A
        call far [xms]
        test ax, ax
        jz fail
        mov si, m_ok
        call puts
        mov ax, 0x4C00
        int 0x21
fail:
        mov si, m_fail
        call puts
        mov al, bl
        xor ah, ah
        call hex4
        mov si, m_crlf
        call puts
        mov ax, 0x4C01
        int 0x21
nox:
        mov si, m_nox
        call puts
        mov ax, 0x4C02
        int 0x21

hex4:
        push cx
        mov cx, 4
.h:
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
        loop .h
        pop cx
        ret
dec32:
        mov ebx, 10
        xor cx, cx
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        ret
putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret
puts:
        lodsb
        test al, al
        jz .r
        call putc
        jmp puts
.r:
        ret
xms     dd 0
handle  dw 0
mv      times 16 db 0
m_ver   db "XMS version ", 0
m_free  db "  largest free KB: ", 0
m_tot   db "  total free KB: ", 0
m_lock  db "Locked at linear ", 0
m_ok    db "XMS test OK", 13, 10, 0
m_fail  db "XMS test FAILED, error ", 0
m_nox   db "No XMS driver", 13, 10, 0
m_crlf  db 13, 10, 0
buf     times 4096 db 0
