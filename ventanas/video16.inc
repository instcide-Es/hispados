; =============================================================================
;  Hispa Ventanas - driver de pantalla, parte de modo real: elegir el modo
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Se hace antes de entrar en modo protegido porque las funciones de VBE
;  4F00h y 4F01h escriben en un búfer de memoria convencional.
;  Escrito a partir de la especificación pública VBE 2.0/3.0 de VESA.
; =============================================================================

; Elige el modo: VESA con marco lineal a la resolución pedida (op_ancho) y, si
; no hay, 800x600; si tampoco, VGA 640x480 de 16 colores.
; Deja vid_tipo (1 VESA, 0 VGA), vid_modo, vid_ancho, vid_alto, vid_paso,
; vid_bits, vid_octetos, vid_fis y las máscaras. CF=1: no hay ni VGA.
vid_detectar:
        cmp byte [op_vga], 0
        jne .vga
        mov dword [vbe_info], 'VBE2'
        mov ax, 0x4F00
        mov di, vbe_info
        int 0x10
        cmp ax, 0x004F
        jne .vga
        cmp dword [vbe_info], 'VESA'
        jne .vga
        mov ax, [vbe_info+4]
        mov [vbe_ver], ax
        cmp ax, 0x0200
        jb .vga
.otra:
        mov ax, [op_ancho]
        mov bx, 768
        cmp ax, 1024
        je .res
        mov bx, 600
        cmp ax, 800
        je .res
        mov bx, 480
.res:
        mov [op_alto], bx
        mov word [vid_puntos], 0
        lfs si, [vbe_info+0x0E]     ; lista de modos
.m1:
        mov cx, [fs:si]
        add si, 2
        cmp cx, 0xFFFF
        je .m9
        push si
        push fs
        push cx
        mov ax, 0x4F01
        mov di, modo_info
        int 0x10
        pop cx
        pop fs
        pop si
        cmp ax, 0x004F
        jne .m1
        mov ax, [modo_info]         ; admitido, gráfico y con marco lineal
        and ax, 0x0091
        cmp ax, 0x0091
        jne .m1
        mov ax, [modo_info+0x12]
        cmp ax, [op_ancho]
        jne .m1
        mov ax, [modo_info+0x14]
        cmp ax, [op_alto]
        jne .m1
        cmp byte [modo_info+0x18], 1
        jne .m1
        mov di, modo_info+0x1F      ; máscaras (las «lineales» en VBE 3.0)
        mov bx, [modo_info+0x10]
        cmp word [vbe_ver], 0x0300
        jb .v2
        cmp word [modo_info+0x32], 0
        je .v2
        mov di, modo_info+0x36
        mov bx, [modo_info+0x32]
.v2:
        mov al, [modo_info+0x19]    ; bits por punto
        mov ah, [modo_info+0x1B]    ; modelo de memoria
        cmp ah, 4                   ; puntos empaquetados: solo 8 bits
        jne .directo
        cmp al, 8
        jne .m1
        mov dx, 50
        jmp .puntos
.directo:
        cmp ah, 6                   ; color directo
        jne .m1
        mov ah, [di]                ; bits de rojo + verde + azul
        add ah, [di+2]
        add ah, [di+4]
        cmp al, 32
        je .d32
        mov al, ah
        mov dx, 10
        cmp al, 15
        je .dc
        mov dx, 20
        cmp al, 16
        je .dc
        mov dx, 30
        cmp al, 24
        je .dc
        jmp .m1
.d32:
        cmp ah, 24
        jne .m1
        mov dx, 40
.dc:
        cmp byte [op_color], 0
        je .puntos
        add dx, 60
.puntos:
        cmp al, [op_bpp]
        jne .p2
        add dx, 200
.p2:
        cmp dx, [vid_puntos]
        jbe .m1
        mov [vid_puntos], dx
        mov [vid_modo], cx
        mov [vid_bits], al
        mov [vid_paso], bx
        mov al, [modo_info+0x19]
        add al, 7
        shr al, 3
        mov [vid_octetos], al
        mov eax, [modo_info+0x28]
        mov [vid_fis], eax
        mov eax, [di]
        mov [vid_masc], eax
        mov ax, [di+4]
        mov [vid_masc+4], ax
        jmp .m1
.m9:
        cmp word [vid_puntos], 0
        jne .vesa
        cmp word [op_ancho], 800    ; no hay: probar con 800x600
        je .vga
        mov word [op_ancho], 800
        jmp .otra
.vesa:
        mov byte [vid_tipo], 1
        mov ax, [op_ancho]
        mov [vid_ancho], ax
        mov ax, [op_alto]
        mov [vid_alto], ax
        push cs
        pop es
        clc
        ret
.vga:
        push cs
        pop es
        mov ax, 0x1A00              ; ¿hay VGA?
        int 0x10
        cmp al, 0x1A
        jne .no
        cmp bl, 7
        je .sivga
        cmp bl, 8
        jne .no
.sivga:
        mov byte [vid_tipo], 0
        mov word [vid_ancho], 640
        mov word [vid_alto], 480
        mov word [vid_paso], 80
        mov byte [vid_bits], 4
        mov byte [vid_octetos], 1
        clc
        ret
.no:
        stc
        ret
