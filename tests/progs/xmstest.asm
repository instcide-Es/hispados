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
        call extra
        mov ax, 0x4C00
        int 0x21

; ---- casos límite de la especificación XMS 3.0 ----
; llamar al XMS con la estructura de movimiento mv
domove:
        mov si, mv
        mov ah, 0x0B
        call far [xms]
        ret
; rellenar buf con el patrón (AL = semilla)
pattern:
        mov di, buf
        mov cx, 4096
.p:
        stosb
        add al, 7
        loop .p
        ret
; mover buf (4096) -> handle AX, desplazamiento EDX
tohandle:
        mov dword [mv+0], 4096
        mov word [mv+4], 0
        mov word [mv+6], buf
        mov [mv+8], cs
        mov [mv+10], ax
        mov [mv+12], edx
        jmp domove
; mover handle AX, desplazamiento EDX -> buf (4096)
fromhandle:
        mov dword [mv+0], 4096
        mov [mv+4], ax
        mov [mv+6], edx
        mov word [mv+10], 0
        mov word [mv+12], buf
        mov [mv+14], cs
        jmp domove
; ¿buf tiene el patrón de semilla AL? ZF si sí
verify:
        mov si, buf
        mov cx, 4096
.c:
        cmp [si], al
        jne .r
        inc si
        add al, 7
        loop .c
        xor cx, cx
.r:
        ret

extra:
        ; 1) 0Fh: un bloque con otro detrás tiene que poder crecer (se muda)
        mov ah, 9
        mov dx, 64
        call far [xms]
        test ax, ax
        jz .f1
        mov [handle], dx
        mov ah, 9
        mov dx, 4
        call far [xms]
        test ax, ax
        jz .f1
        mov [handle2], dx
        mov al, 3
        call pattern
        mov ax, [handle]
        mov edx, 0xF000
        call tohandle
        test ax, ax
        jz .f1
        mov dx, [handle]
        mov bx, 128
        mov ah, 0x0F
        call far [xms]
        test ax, ax
        jz .f1
        mov ax, [handle]
        mov edx, 0xF000
        call fromhandle
        test ax, ax
        jz .f1
        mov al, 3
        call verify
        jnz .f1
        ; el bloque crecido llega hasta 128 KB
        mov al, 5
        call pattern
        mov ax, [handle]
        mov edx, 0x1F000
        call tohandle
        test ax, ax
        jz .f1
        mov si, m_re
        call puts
        jmp .t2
.f1:
        mov si, m_ref
        call puts
        mov al, bl
        xor ah, ah
        call hex4
        mov si, m_crlf
        call puts
.t2:
        ; 2) movimientos solapados dentro del mismo bloque
        mov al, 9
        call pattern
        mov ax, [handle]
        xor edx, edx
        call tohandle
        ; origen antes que destino (0 -> 800h, 4096 bytes)
        mov dword [mv+0], 4096
        mov ax, [handle]
        mov [mv+4], ax
        mov dword [mv+6], 0
        mov [mv+10], ax
        mov dword [mv+12], 0x800
        call domove
        test ax, ax
        jz .f2
        mov ax, [handle]
        mov edx, 0x800
        call fromhandle
        mov al, 9
        call verify
        jnz .f2
        ; destino antes que origen (800h -> 400h)
        mov dword [mv+0], 4096
        mov ax, [handle]
        mov [mv+4], ax
        mov dword [mv+6], 0x800
        mov [mv+10], ax
        mov dword [mv+12], 0x400
        call domove
        test ax, ax
        jz .f2
        mov ax, [handle]
        mov edx, 0x400
        call fromhandle
        mov al, 9
        call verify
        jnz .f2
        mov si, m_ov
        call puts
        jmp .t3
.f2:
        mov si, m_ovf
        call puts
.t3:
        ; 3) códigos de error del movimiento
        mov dword [mv+0], 4095          ; longitud impar: A7h
        mov ax, [handle]
        mov [mv+4], ax
        mov dword [mv+6], 0
        mov word [mv+10], 0
        mov word [mv+12], buf
        mov [mv+14], cs
        call domove
        cmp bl, 0xA7
        jne .f3
        mov dword [mv+0], 4096          ; desplazamiento de origen fuera: A4h
        mov dword [mv+6], 0x1FF00
        call domove
        cmp bl, 0xA4
        jne .f3
        mov dword [mv+6], 0xFFFFFFF0    ; desplazamiento + longitud dan la vuelta
        call domove
        test ax, ax
        jnz .f3
        cmp bl, 0xA4
        jne .f3
        mov word [mv+4], 0x1234         ; handle de origen malo: A3h
        mov dword [mv+6], 0
        call domove
        cmp bl, 0xA3
        jne .f3
        mov word [mv+4], 0              ; desplazamiento de destino fuera: A6h
        mov word [mv+6], buf
        mov [mv+8], cs
        mov ax, [handle]
        mov [mv+10], ax
        mov dword [mv+12], 0x20000
        call domove
        cmp bl, 0xA6
        jne .f3
        mov word [mv+10], 0x4321        ; handle de destino malo: A5h
        call domove
        cmp bl, 0xA5
        jne .f3
        mov si, m_er
        call puts
        jmp .t4
.f3:
        mov si, m_erf
        call puts
        mov al, bl
        xor ah, ah
        call hex4
        mov si, m_crlf
        call puts
.t4:
        ; 4) 08h y 0Ch solo cambian los registros de 16 bits que devuelven
        mov eax, 0x12340800
        mov edx, 0x56780000
        call far [xms]
        shr eax, 16
        cmp ax, 0x1234
        jne .f4
        shr edx, 16
        cmp dx, 0x5678
        jne .f4
        mov ebx, 0x9ABC0000
        mov edx, 0x13570000
        mov dx, [handle]
        mov ah, 0x0C
        call far [xms]
        shr ebx, 16
        cmp bx, 0x9ABC
        jne .f4
        shr edx, 16
        cmp dx, 0x1357
        jne .f4
        ; bloqueado: ni se libera (ABh) ni cambia de tamaño (ABh)
        mov dx, [handle]
        mov ah, 0x0A
        call far [xms]
        cmp bl, 0xAB
        jne .f4
        mov dx, [handle]
        mov bx, 256
        mov ah, 0x0F
        call far [xms]
        cmp bl, 0xAB
        jne .f4
        mov dx, [handle]
        mov ah, 0x0D
        call far [xms]
        mov si, m_rg
        call puts
        jmp .t5
.f4:
        mov si, m_rgf
        call puts
.t5:
        mov dx, [handle]
        mov ah, 0x0A
        call far [xms]
        mov dx, [handle2]
        mov ah, 0x0A
        call far [xms]
        ret
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
handle2 dw 0
m_re    db "XMS realloc OK", 13, 10, 0
m_ref   db "XMS realloc FAILED, error ", 0
m_ov    db "XMS overlap OK", 13, 10, 0
m_ovf   db "XMS overlap FAILED", 13, 10, 0
m_er    db "XMS errors OK", 13, 10, 0
m_erf   db "XMS errors FAILED, got ", 0
m_rg    db "XMS registers OK", 13, 10, 0
m_rgf   db "XMS registers FAILED", 13, 10, 0
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
