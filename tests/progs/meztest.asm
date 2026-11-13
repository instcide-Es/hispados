; MEZTEST.COM - prueba del mezclador por fuentes de HSPSND (interfaz 09h-0Fh)
; y de los registros del mezclador de la Sound Blaster (tests/run_mezclador_tests.sh).
;   MEZTEST [orden ...]   cada orden, por este orden, y al final el estado
;     Vf:n   volumen de la fuente f (0-6) a n (0-100)
;     Mf:x   silencio (x = 0/1)       Sf:x  «Sonar»       Gf:x  «Grabar»
;     Bf:n   balance (0-100)          An:v  ajuste n a v
;     F      valores de fábrica       N     marca «guardado» (función 0Fh)
;     Wrr:vv escribe vv (hex) en el registro rr (hex) del mezclador de la
;            Sound Blaster en 220h (como un juego: puertos 224h y 225h)
;     C      lee y enseña los registros 30h-47h del mezclador (CT1745)
;     P      lee y enseña los registros 22h, 30h-35h (lo que ve un juego)
; Escribe «MEZ ...», una línea por fuente («F1 vol=90 cas=0 bal=50»), los
; ajustes y «CT 30=..» si se pide.
        cpu 386
        bits 16
        org 0x100

start:
        mov ax, 0xD700
        int 0x2F
        cmp al, 0xFF
        je .hay
        mov si, m_no
        call puts
        mov ax, 0x4C01
        int 0x21
.hay:
        mov [drv], bx
        mov [drv+2], es
        push cs
        pop es
        mov ah, 0
        call far [drv]
        mov [tipo], al
        mov [ver], bx
        mov si, 0x81
.l:
        lodsb
        cmp al, 13
        je .fin
        cmp al, ' '
        je .l
        and al, 0xDF
        cmp al, 'V'
        je o_vol
        cmp al, 'M'
        je o_mudo
        cmp al, 'S'
        je o_sonar
        cmp al, 'G'
        je o_grabar
        cmp al, 'B'
        je o_bal
        cmp al, 'A'
        je o_aj
        cmp al, 'F'
        je o_fab
        cmp al, 'N'
        je o_guard
        cmp al, 'W'
        je o_esc
        cmp al, 'C'
        je o_ct
        cmp al, 'P'
        je o_pro
        jmp .l
.fin:
        call estado
        mov ax, 0x4C00
        int 0x21

; f:n -> BL = f, AX = n (decimal)
dos_num:
        call dec
        mov bl, al
        inc si                      ; «:»
        call dec
        ret
dec:
        xor ax, ax
.d:
        movzx cx, byte [si]
        sub cl, '0'
        jb .r
        cmp cl, 9
        ja .r
        imul ax, ax, 10
        add ax, cx
        inc si
        jmp .d
.r:
        ret
hexn:
        xor ax, ax
.h:
        mov cl, [si]
        sub cl, '0'
        jb .r
        cmp cl, 9
        jbe .k
        and cl, 0xDF
        sub cl, 7
        cmp cl, 10
        jb .r
        cmp cl, 15
        ja .r
.k:
        shl ax, 4
        or al, cl
        inc si
        jmp .h
.r:
        ret

o_vol:
        call dos_num                ; BL = fuente, AX = volumen
        mov dl, bl
        mov bl, al
        mov al, dl
        mov bh, 0xFF
        mov cl, 0xFF
        mov ah, 0x0B
        call far [drv]
        jmp start.l

o_mudo:
        mov dl, 1
        jmp casilla
o_sonar:
        mov dl, 2
        jmp casilla
o_grabar:
        mov dl, 4
casilla:
        push dx
        call dos_num
        pop dx
        mov [num], ax
        mov [fuente], bl
        mov al, bl
        mov ah, 0x0A
        call far [drv]              ; AH = casillas
        mov bh, ah
        or bh, dl
        cmp word [num], 0
        jne .p
        not dl
        and bh, dl
.p:
        mov al, [fuente]
        mov bl, 0xFF
        mov cl, 0xFF
        mov ah, 0x0B
        call far [drv]
        jmp start.l

o_bal:
        call dos_num
        mov cl, al
        mov al, bl
        mov bx, 0xFFFF
        mov ah, 0x0B
        call far [drv]
        jmp start.l

o_aj:
        call dos_num
        xchg al, bl
        mov ah, 0x0D
        call far [drv]
        jmp start.l

o_fab:
        mov ax, 0x0E00
        call far [drv]
        jmp start.l

o_guard:
        mov ax, 0x0F01
        call far [drv]
        jmp start.l

o_esc:
        call hexn
        mov bl, al
        inc si
        call hexn
        mov bh, al
        mov dx, 0x224
        mov al, bl
        out dx, al
        inc dx
        mov al, bh
        out dx, al
        jmp start.l

o_ct:
        push si
        mov si, m_ct
        call puts
        mov bl, 0x30
.c:
        call regsb
        inc bl
        cmp bl, 0x48
        jb .c
        mov si, m_nl
        call puts
        pop si
        jmp start.l
o_pro:
        push si
        mov si, m_pro
        call puts
        mov bl, 0x22
        call regsb
        mov bl, 0x04
        call regsb
        mov bl, 0x26
        call regsb
        mov bl, 0x30
.p:
        call regsb
        inc bl
        cmp bl, 0x36
        jb .p
        mov si, m_nl
        call puts
        pop si
        jmp start.l
; BL = registro: « rr=vv»
regsb:
        mov al, ' '
        call putc
        mov al, bl
        call hex2
        mov al, '='
        call putc
        mov dx, 0x224
        mov al, bl
        out dx, al
        inc dx
        in al, dx
        call hex2
        ret

; ---- el estado ----
estado:
        mov si, m_mez
        call puts
        mov ax, [ver]
        call pdec
        mov si, m_tarj
        call puts
        movzx ax, byte [tipo]
        call pdec
        cmp word [ver], 4
        jb .vieja
        mov ah, 0x09
        call far [drv]
        mov [c_ax], ax
        mov [c_bx], bx
        mov [c_cx], cx
        mov [c_dx], dx
        mov si, m_fu
        call puts
        mov al, [c_ax]
        call hex2
        mov si, m_hw
        call puts
        mov al, [c_ax+1]
        call hex2
        mov si, m_aj
        call puts
        mov ax, [c_bx]
        call hex4
        mov si, m_gr
        call puts
        mov al, [c_cx]
        call hex2
        mov si, m_so
        call puts
        mov al, [c_dx]
        call hex2
        mov ax, 0x0F00
        call far [drv]
        push ax
        mov si, m_cam
        call puts
        pop ax
        movzx ax, al
        call pdec
        mov si, m_nl
        call puts
        xor di, di
.f:
        mov si, m_f
        call puts
        mov ax, di
        call pdec
        mov ax, di
        mov ah, 0x0A
        call far [drv]
        mov [c_ax], ax
        mov [c_bx], bx
        mov si, m_vol
        call puts
        movzx ax, byte [c_ax]
        call pdec
        mov si, m_cas
        call puts
        movzx ax, byte [c_ax+1]
        call pdec
        mov si, m_bal
        call puts
        movzx ax, byte [c_bx]
        call pdec
        mov si, m_nl
        call puts
        inc di
        cmp di, 7
        jb .f
        mov si, m_ajs
        call puts
        xor di, di
.a:
        mov al, ' '
        call putc
        mov ax, di
        call pdec
        mov al, '='
        call putc
        mov ax, di
        mov ah, 0x0C
        call far [drv]
        movzx ax, al
        call pdec
        inc di
        cmp di, 6
        jb .a
        mov si, m_nl
        call puts
        ret
.vieja:
        mov si, m_nl
        jmp puts

; ---- salida ----
puts:
        lodsb
        test al, al
        jz .r
        call putc
        jmp puts
.r:
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
hex4:
        push ax
        mov al, ah
        call hex2
        pop ax
hex2:
        push ax
        shr al, 4
        call .n
        pop ax
        push ax
        and al, 15
        call .n
        pop ax
        ret
.n:
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp putc
pdec:
        push ax
        push bx
        push cx
        push dx
        mov bx, 10
        xor cx, cx
.l:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .l
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop dx
        pop cx
        pop bx
        pop ax
        ret

m_no    db "MEZTEST: HSPSND no está cargado", 13, 10, 0
m_mez   db "MEZ versión ", 0
m_tarj  db " tarjeta ", 0
m_fu    db " fuentes=", 0
m_hw    db " tarjeta=", 0
m_aj    db " ajustes=", 0
m_gr    db " grabar=", 0
m_so    db " sonar=", 0
m_cam   db " cambios=", 0
m_f     db "F", 0
m_vol   db " vol=", 0
m_cas   db " cas=", 0
m_bal   db " bal=", 0
m_ajs   db "AJ", 0
m_ct    db "CT", 0
m_pro   db "SBP", 0
m_nl    db 13, 10, 0
drv     dd 0
tipo    db 0
ver     dw 0
num     dw 0
fuente  db 0
c_ax    dw 0
c_bx    dw 0
c_cx    dw 0
c_dx    dw 0
