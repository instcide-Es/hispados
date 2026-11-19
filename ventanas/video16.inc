; =============================================================================
;  Hispa Ventanas - driver de pantalla, parte de modo real: elegir el modo
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Se hace antes de entrar en modo protegido porque las funciones de VBE
;  4F00h, 4F01h y 4F15h escriben en un búfer de memoria convencional.
;  Escrito a partir de las especificaciones públicas VBE 2.0/3.0 y VBE/DDC
;  1.1 y del formato EDID 1.3 de VESA.
;
;  [PANTALLA] Cómo se elige el modo (docs/VENTANAS.md, «La pantalla»):
;   1. vl_listar apunta en vl_tabla las resoluciones que da de verdad la
;      tarjeta (VESA con marco lineal, de 640x480 a 2560x1600) y, de cada
;      una, en qué colores: 256 (8 bits), 32.768 o 65.536 (15 o 16) y 16,7
;      millones (24 o 32). Configuración › Resolución enseña esta lista.
;   2. vl_edid lee el EDID de la pantalla (VBE/DDC, 4F15h): su resolución
;      nativa (la del primer «detailed timing»), en edid_an y edid_al.
;   3. La resolución: la pedida (opción /1024, /800, /640 o /MODO:AxB de la
;      línea de órdenes; si no, «Modo=» de [Pantalla] en VENTANAS.INI, que
;      ya ha leído vid_ini) si la tarjeta la da; si no se ha pedido ninguna
;      o no existe en este equipo, la automática (vl_auto): la nativa de la
;      pantalla si la tarjeta la da (hasta 1920x1200) o, si no, la mayor que
;      quepa en ella; sin EDID, 800x600 como siempre (un monitor de tubo
;      viejo puede no ver más).
;   4. Los colores: los pedidos (/8 /15 /16 /24 /32 o los de «Modo=») si
;      esa resolución los tiene (16 vale por 15 y 32 por 24 y al revés); si
;      no, 256 colores (lo más rápido) o, con /COLOR, los más que haya.
; =============================================================================

VL_MAX      equ 32                  ; resoluciones apuntadas, como mucho
VL_TAM      equ 16                  ; cada una: ancho, alto, colores (bits:
VL_AN       equ 0                   ;   0 8, 1 15, 2 16, 3 24, 4 32) y el
VL_AL       equ 2                   ;   número de modo de cada uno
VL_MASC     equ 4
VL_MODOS    equ 6                   ; 5 palabras

; Elige el modo. Deja vid_tipo (1 VESA, 0 VGA), vid_modo, vid_ancho,
; vid_alto, vid_paso, vid_bits, vid_octetos, vid_fis y las máscaras.
; CF=1: no hay ni VGA.
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
        call vl_listar
        call vl_edid
        mov si, [op_ancho]          ; ¿se ha pedido una resolución?
        test si, si
        jz .auto
        mov di, [op_alto]
        call vl_buscar
        jnc .res
.auto:
        mov byte [vl_autosi], 1
        call vl_auto
        jc .vga
.res:
        mov [vl_elegida], bx
        call vl_colores             ; -> CX=modo
        call vid_info_modo
        jc .vga
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
        mov byte [vid_tipo], 1
        mov ax, [modo_info+0x12]
        mov [vid_ancho], ax
        mov ax, [modo_info+0x14]
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

; (interna) CX=modo: 4F01h en modo_info y, si vale para Hispa Ventanas
; (admitido, gráfico, con marco lineal, un plano, 8 bits de paleta o color
; directo de 15, 16, 24 o 32), AL=bits (8, 15, 16, 24 o 32), BX=octetos por
; línea y DI=las máscaras (las «lineales» de VBE 3.0 si las hay). CF=1 si no
; vale. Conserva CX, SI y FS
vid_info_modo:
        push si
        push fs
        push cx
        mov ax, 0x4F01
        mov di, modo_info
        push cs
        pop es
        int 0x10
        pop cx
        pop fs
        pop si
        cmp ax, 0x004F
        jne .no
        mov ax, [modo_info]         ; admitido, gráfico y con marco lineal
        and ax, 0x0091
        cmp ax, 0x0091
        jne .no
        cmp byte [modo_info+0x18], 1
        jne .no
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
        jne .no
        clc
        ret
.directo:
        cmp ah, 6                   ; color directo
        jne .no
        mov ah, [di]                ; bits de rojo + verde + azul
        add ah, [di+2]
        add ah, [di+4]
        cmp al, 32
        je .d32
        mov al, ah
        cmp al, 15
        je .si
        cmp al, 16
        je .si
        cmp al, 24
        je .si
.no:
        stc
        ret
.d32:
        cmp ah, 24
        jne .no
.si:
        clc
        ret

; (interna) la lista de modos de la tarjeta, a vl_tabla (vl_n resoluciones)
vl_listar:
        mov word [vl_n], 0
        lfs si, [vbe_info+0x0E]     ; lista de modos
.m1:
        mov cx, [fs:si]
        add si, 2
        cmp cx, 0xFFFF
        je .x
        call vid_info_modo
        jc .m1
        mov dx, [modo_info+0x12]    ; de 640x480 a 2560x1600
        cmp dx, 640
        jb .m1
        cmp dx, 2560
        ja .m1
        mov dx, [modo_info+0x14]
        cmp dx, 480
        jb .m1
        cmp dx, 1600
        ja .m1
        call vl_bit                 ; AL=bits -> BX=su número (0-4)
        push si
        mov si, [modo_info+0x12]
        mov di, [modo_info+0x14]
        push bx
        call vl_buscar
        pop dx
        jnc .ya
        mov ax, [vl_n]              ; nueva
        cmp ax, VL_MAX
        jae .lleno
        inc word [vl_n]
        imul bx, ax, VL_TAM
        add bx, vl_tabla
        mov [bx+VL_AN], si
        mov [bx+VL_AL], di
        mov word [bx+VL_MASC], 0
.ya:
        mov si, dx                  ; (el primero de cada uno: el que vale)
        bts [bx+VL_MASC], si
        jc .lleno
        shl si, 1
        mov [bx+VL_MODOS+si], cx
.lleno:
        pop si
        jmp .m1
.x:
        ret

; (interna) AL=bits (8, 15, 16, 24, 32) -> BX=0-4
vl_bit:
        xor bx, bx
        cmp al, 8
        je .x
        inc bx
        cmp al, 15
        je .x
        inc bx
        cmp al, 16
        je .x
        inc bx
        cmp al, 24
        je .x
        inc bx
.x:
        ret

; (interna) SI=ancho DI=alto -> BX=su resolución en vl_tabla; CF=1 si no está
vl_buscar:
        push cx
        mov bx, vl_tabla
        mov cx, [vl_n]
        jcxz .no
.b:
        cmp [bx+VL_AN], si
        jne .s
        cmp [bx+VL_AL], di
        jne .s
        pop cx
        clc
        ret
.s:
        add bx, VL_TAM
        loop .b
.no:
        pop cx
        stc
        ret

; (interna) el EDID de la pantalla (VBE/DDC 4F15h, función 01h, bloque 0):
; su resolución nativa, a edid_an y edid_al (0 si no se sabe)
vl_edid:
        mov word [edid_an], 0
        mov word [edid_al], 0
        mov di, dos_buf
        mov cx, 64
        xor ax, ax
        rep stosw
        mov ax, 0x4F15
        mov bx, 0x0001
        xor cx, cx
        xor dx, dx
        mov di, dos_buf
        push cs
        pop es
        int 0x10
        push cs
        pop es
        cmp ax, 0x004F
        jne .x
        cmp dword [dos_buf], 0xFFFFFF00     ; la cabecera: 00 FF FF FF FF FF FF 00
        jne .x
        cmp dword [dos_buf+4], 0x00FFFFFF
        jne .x
        cmp word [dos_buf+54], 0    ; el primer «detailed timing» (reloj <> 0)
        je .x
        mov al, [dos_buf+58]        ; ancho: 8 bits bajos en 56, altos en 58 (4-7)
        mov ah, al
        shr ah, 4
        mov al, [dos_buf+56]
        mov bx, ax
        mov al, [dos_buf+61]        ; alto: en 59 y 61 (4-7)
        mov ah, al
        shr ah, 4
        mov al, [dos_buf+59]
        cmp bx, 640
        jb .x
        cmp ax, 480
        jb .x
        mov [edid_an], bx
        mov [edid_al], ax
.x:
        ret

; (interna) la resolución automática -> BX=en vl_tabla; CF=1 si no hay
; ninguna. Con EDID: la nativa, si la da la tarjeta y no pasa de 1920x1200;
; si no, la mayor (en puntos) que quepa en la nativa y en 1920x1200. Sin
; EDID (o si no cabe ninguna): 800x600, 640x480 o 1024x768, la primera que
; haya; si no, la primera de la lista
vl_auto:
        mov si, [edid_an]
        test si, si
        jz .sin
        mov di, [edid_al]
        cmp si, 1920
        ja .mayor
        cmp di, 1200
        ja .mayor
        call vl_buscar
        jnc .x
.mayor:
        cmp si, 1920
        jbe .m1
        mov si, 1920
.m1:
        cmp di, 1200
        jbe .m2
        mov di, 1200
.m2:
        xor eax, eax                ; EAX=los puntos de la mejor, DX=ella
        xor dx, dx
        mov bx, vl_tabla
        mov cx, [vl_n]
        jcxz .sin
.c:
        cmp [bx+VL_AN], si
        ja .cs
        cmp [bx+VL_AL], di
        ja .cs
        push eax
        push edx
        movzx eax, word [bx+VL_AN]
        movzx edx, word [bx+VL_AL]
        imul eax, edx
        mov ebp, eax
        pop edx
        pop eax
        cmp ebp, eax
        jbe .cs
        mov eax, ebp
        mov dx, bx
.cs:
        add bx, VL_TAM
        loop .c
        mov bx, dx
        test bx, bx
        jnz .si
.sin:
        mov si, 800
        mov di, 600
        call vl_buscar
        jnc .x
        mov si, 640
        mov di, 480
        call vl_buscar
        jnc .x
        mov si, 1024
        mov di, 768
        call vl_buscar
        jnc .x
        cmp word [vl_n], 0
        je .no
        mov bx, vl_tabla
.si:
        clc
.x:
        ret
.no:
        stc
        ret

; (interna) BX=resolución en vl_tabla -> CX=el modo con los colores que
; tocan: los pedidos (op_bpp, o su pareja: 15/16, 24/32) y, si no, 256
; colores o (con /COLOR) los más que haya
vl_colores:
        mov dx, [bx+VL_MASC]
        mov al, [op_bpp]
        test al, al
        jz .orden
        push bx
        call vl_bit
        mov si, bx
        pop bx
        bt dx, si
        jc .ya
        test si, si                 ; la pareja: 15/16 (1, 2) y 24/32 (3, 4)
        jz .orden
        cmp si, 2
        ja .p34
        xor si, 3
        jmp .p2
.p34:
        xor si, 7
.p2:
        bt dx, si
        jc .ya
.orden:
        mov di, vl_pref
        cmp byte [op_color], 0
        je .o1
        mov di, vl_pref_color
.o1:
        mov cx, 5
.o2:
        movzx si, byte [di]
        inc di
        bt dx, si
        jc .ya
        loop .o2
        xor si, si                  ; (no pasa: alguno hay)
.ya:
        shl si, 1
        mov cx, [bx+VL_MODOS+si]
        ret
vl_pref     db 0, 4, 3, 2, 1        ; 256 colores; si no, los más que haya
vl_pref_color db 4, 3, 2, 1, 0      ; /COLOR

; =============================================================================
;  [PANTALLA] «Modo=» de [Pantalla] en VENTANAS.INI (al lado de VENTANAS.COM)
;  y la opción /MODO: AxB, AxBxC (C: 8, 15, 16, 24 o 32 bits) o AUTO
; =============================================================================
; Lee VENTANAS.INI; si tiene «Modo=», lo pone en lo que no haya dicho la
; línea de órdenes (la resolución, los colores o los dos)
vid_ini:
        cmp byte [op_vga], 0
        jne .x
        mov es, [env_seg]           ; la ruta propia: tras el doble cero del entorno
        xor di, di
        xor al, al
.e1:
        mov cx, 0xFFFF
        repne scasb
        cmp [es:di], al
        jne .e1
        add di, 3
        mov si, ruta
        mov bx, si
.c1:
        mov al, [es:di]
        inc di
        mov [si], al
        inc si
        cmp al, '\'
        jne .c2
        mov bx, si
.c2:
        cmp si, ruta + 110
        jae .c3
        test al, al
        jnz .c1
.c3:
        push cs
        pop es
        mov di, bx
        mov si, vi_n_ini
        mov cx, 13
        rep movsb
        mov ax, 0x3D00
        mov dx, ruta
        int 0x21
        jc .x
        mov bx, ax
        mov ah, 0x3F
        mov cx, 2047
        mov dx, dos_buf
        int 0x21
        jnc .l
        xor ax, ax
.l:
        mov si, ax
        mov byte [dos_buf+si], 0
        mov ah, 0x3E
        int 0x21
        mov si, dos_buf
        xor dl, dl                  ; DL=1: dentro de [Pantalla]
.linea:
        lodsb
        test al, al
        jz .x
        cmp al, ' '
        jbe .linea
        dec si
        cmp al, '['
        jne .clave
        mov di, vi_s_pan
        call vi_igual
        setz dl
        jmp .salta
.clave:
        test dl, dl
        jz .salta
        mov di, vi_k_modo
        call vi_igual
        jne .salta
.k1:
        lodsb
        cmp al, ' '
        je .k1
        cmp al, '='
        jne .salta
.k2:
        lodsb
        cmp al, ' '
        je .k2
        dec si
        mov byte [vi_ini], 1
        call vid_modo_texto
        jmp .x
.salta:
        lodsb
        test al, al
        jz .x
        cmp al, 10
        jne .salta
        jmp .linea
.x:
        push cs
        pop es
        ret

; (interna) ¿empieza SI por DI (en minúsculas, acabado en 0)? ZF=1 si sí,
; y SI pasa detrás; si no, SI no cambia
vi_igual:
        push si
.c:
        mov al, [di]
        test al, al
        jz .si
        mov ah, [si]
        cmp ah, 'A'
        jb .c1
        cmp ah, 'Z'
        ja .c1
        or ah, 0x20
.c1:
        cmp al, ah
        jne .no
        inc si
        inc di
        jmp .c
.si:
        add sp, 2
        cmp al, al
        ret
.no:
        pop si
        ret

; SI=texto: «AUTO», «AxB» o «AxBxC». vi_ini=1: viene de VENTANAS.INI (no
; cambia lo que ya ha puesto la línea de órdenes). Lo que no se entiende,
; no cuenta (se queda la automática)
vid_modo_texto:
        mov di, vi_auto
        call vi_igual
        jne .num
        cmp byte [vi_ini], 0
        jne .x
        mov word [op_ancho], 0      ; /MODO:AUTO (la de Configuración, al probarla)
        mov byte [op_res], 1
        mov byte [op_bpp], 0
.x:
        ret
.num:
        call vi_num
        mov cx, ax
        cmp cx, 640
        jb .x
        cmp cx, 2560
        ja .x
        call vi_por
        jne .x
        call vi_num
        mov dx, ax
        cmp dx, 480
        jb .x
        cmp dx, 1600
        ja .x
        xor bx, bx
        call vi_por
        jne .pon
        call vi_num
        mov bx, ax
.pon:
        cmp byte [vi_ini], 0
        je .r
        cmp byte [op_res], 0        ; (de VENTANAS.INI: lo de la línea, manda)
        jne .b
        mov byte [vl_origen], 1
.r:
        mov [op_ancho], cx
        mov [op_alto], dx
        mov byte [op_res], 1
.b:
        test bx, bx
        jz .x
        cmp byte [vi_ini], 0
        je .b2
        cmp byte [op_bpp], 0
        jne .x
.b2:
        mov [op_bpp], bl
        ret

; (interna) SI=dígitos -> AX=número (SI avanza)
vi_num:
        xor ax, ax
.d:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .x
        imul ax, ax, 10
        xor bh, bh
        add ax, bx
        inc si
        jmp .d
.x:
        ret

; (interna) ¿una «x» (o «X», «*»)? ZF=1 y SI la pasa
vi_por:
        mov al, [si]
        or al, 0x20
        cmp al, 'x'
        je .si
        cmp al, '*' | 0x20
        jne .x
.si:
        inc si
        cmp al, al
.x:
        ret

vi_n_ini    db "VENTANAS.INI", 0
vi_s_pan    db "[pantalla]", 0
vi_k_modo   db "modo", 0
vi_auto     db "auto", 0
