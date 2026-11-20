; WINSIM.COM - hace con el ratón lo que Windows 3.x (pruebas de HSPRATON)
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  1. Avisa de que «Windows arranca»: INT 2Fh AX=1605h (DX=0: modo 386
;     mejorado, DI=030Ah: versión 3.10, ES:BX=DS:SI=0, CX=0). Si alguien
;     contesta CX distinto de 0, Windows no seguiría: se dice y se acaba.
;  2. Usa el ratón como el MOUSE.DRV de Windows («Microsoft o IBM PS/2»):
;     INT 15h C205h (inicializar, paquetes de 3 bytes), C203h (resolución),
;     C207h (su rutina), C200h BH=1 (activar), y cuenta los paquetes que la
;     BIOS le entrega por la IRQ 12 hasta que se pulsa una tecla (o 15 s).
;  3. Lo deja como lo deja Windows al salir (C200h BH=0, C207h con 0:0) y
;     avisa de que «Windows termina»: INT 2Fh AX=1606h.
;  Escribe «WINSIM: ...» con lo que ha pasado.
        cpu 386
        bits 16
        org 0x100

        mov dx, m_ini
        call puts
        xor bx, bx
        mov es, bx
        xor si, si                  ; (DS:SI: aquí DS = CS; nadie lo mira)
        xor cx, cx
        xor dx, dx
        mov di, 0x030A
        mov ax, 0x1605
        int 0x2F
        push cs
        pop ds
        mov [cx1605], cx
        mov ax, cx
        call phex
        mov dx, m_crlf
        call puts
        cmp word [cx1605], 0
        je .bios
        mov dx, m_no
        call puts
        mov ax, 0x4C01
        int 0x21
.bios:
        ; el ratón, por la BIOS, como MOUSE.DRV
        mov dx, m_wsim
        call puts
        mov ax, 0xC205
        mov bh, 3
        int 0x15
        mov al, 0x05
        call paso
        mov ax, 0xC203
        mov bh, 2
        int 0x15
        mov al, 0x03
        call paso
        push cs
        pop es
        mov bx, rutina
        mov ax, 0xC207
        int 0x15
        mov al, 0x07
        call paso
        mov ax, 0xC200
        mov bh, 1
        int 0x15
        mov al, 0x00
        call paso
        mov dx, m_listo
        call puts
        ; esperar una tecla (por la consola del DOS: también el puerto serie)
        ; o 15 s
        xor ax, ax
        mov es, ax
        mov bx, [es:0x46C]
.w:
        mov ah, 0x0B
        int 0x21
        test al, al
        jnz .tecla
        mov ax, [es:0x46C]
        sub ax, bx
        cmp ax, 15 * 18
        jb .w
        jmp .fin
.tecla:
        mov ah, 0x08
        int 0x21
.fin:
        mov dx, m_paq
        call puts
        mov ax, [npaq]
        call pdec
        mov dx, m_mov
        call puts
        mov ax, [movx]
        call pdec
        mov dx, m_crlf
        call puts
        ; como lo deja Windows al salir
        mov ax, 0xC200
        xor bh, bh
        int 0x15
        xor bx, bx
        mov es, bx
        mov ax, 0xC207
        int 0x15
        mov dx, m_fin
        call puts
        xor cx, cx
        xor dx, dx
        mov ax, 0x1606
        int 0x2F
        mov dx, m_adios
        call puts
        mov ax, 0x4C00
        int 0x21

; resultado de una función C2xxh (AL = su número): «C2xxh=bien» o el error
paso:
        pushf
        push ax
        mov dx, m_c2
        call puts
        pop ax
        push ax
        call phex2
        pop ax
        popf
        jc .mal
        mov dx, m_bien
        jmp puts
.mal:
        push ax
        mov dx, m_mal
        call puts
        pop ax
        mov al, ah
        call phex2
        mov dx, m_sp
        jmp puts

; la rutina que llama la BIOS con cada paquete: [sp+4+6] estado, +4 x, +2 y
rutina:
        push bp
        mov bp, sp
        push ax
        inc word [cs:npaq]
        mov al, [bp+10]             ; x (con el signo en el bit 4 del estado)
        xor ah, ah
        test byte [bp+12], 0x10
        jz .p
        mov ah, 0xFF
.p:
        add [cs:movx], ax
        pop ax
        pop bp
        retf

puts:
        mov ah, 0x09
        int 0x21
        ret
phex:                               ; AX en hexadecimal (4 cifras)
        push ax
        mov al, ah
        call phex2
        pop ax
phex2:                              ; AL en hexadecimal
        push ax
        shr al, 4
        call .c
        pop ax
.c:
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .o
        add al, 7
.o:
        mov dl, al
        mov ah, 0x02
        int 0x21
        ret
pdec:                               ; AX en decimal
        mov bx, 10
        xor cx, cx
.d:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d
.e:
        pop dx
        add dl, '0'
        mov ah, 0x02
        int 0x21
        loop .e
        ret

cx1605      dw 0
npaq        dw 0
movx        dw 0
m_ini       db "WINSIM: Windows arranca (INT 2Fh 1605h) -> CX=$"
m_no        db "WINSIM: alguien no deja arrancar a Windows", 13, 10, "$"
m_c2        db "C2$"
m_wsim      db "WINSIM: $"
m_bien      db "h=bien ", "$"
m_mal       db "h=MAL AH=$"
m_sp        db " $"
m_listo     db 13, 10, "WINSIM: ratón de la BIOS activado; mueva el ratón y pulse una tecla", 13, 10, "$"
m_paq       db "WINSIM: paquetes de la BIOS=$"
m_mov       db " movimiento X=$"
m_fin       db "WINSIM: Windows termina (INT 2Fh 1606h)", 13, 10, "$"
m_adios     db "WINSIM: fin", 13, 10, "$"
m_crlf      db 13, 10, "$"
