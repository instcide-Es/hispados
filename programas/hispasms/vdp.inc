; =============================================================================
;  Hispa-System - la imagen (el VDP de la consola), los puertos, los mandos y
;  el cuadro entero
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Escrito desde la documentación pública del VDP de la consola (315-5124 y
;  315-5246, que es el TMS9918 con un modo más, el «modo 4») y de sus
;  puertos:
;  - el modo 4: 16 KB de memoria de imagen; un fondo de 32x28 dibujos de 8x8
;    con 16 colores (cuatro planos), dos paletas, espejos y prioridad, con
;    desplazamiento horizontal (las dos filas de arriba pueden quedar fijas)
;    y vertical (las ocho columnas de la derecha pueden quedar fijas); 64
;    objetos de 8x8 u 8x16 (o al doble), ocho por línea como mucho (con la
;    bandera de «más de ocho») y la bandera de choque; la paleta de 32
;    colores de 6 bits (2 de rojo, 2 de verde, 2 de azul); la interrupción
;    de cuadro y la de línea (con su contador); 192 líneas;
;  - los modos del TMS9918 (los que usan los juegos de la consola anterior y
;    algunos de los primeros): gráfico I y II, texto y multicolor, con sus 32
;    objetos (cuatro por línea, el aviso del quinto) y su paleta fija;
;  - los puertos: 3Eh y 3Fh (memoria y patillas de los mandos), 7Eh y 7Fh
;    (contadores de línea y de punto), 7Fh también el sonido, BEh y BFh (el
;    VDP), DCh y DDh (los mandos). El botón de pausa da una NMI.
;  Se dibuja por líneas: cada línea se pinta de una vez al empezarla, con lo
;  que hay en ese momento, y luego corre el procesador sus 228 ciclos (lo
;  que cambie entonces vale desde la línea siguiente).
;
;    sms_cuadro      emula un cuadro (262 líneas, NTSC). Con [dibujar]=1 deja
;                    la imagen en «imagen»: 256x192, un octeto por punto con
;                    el color de la consola (0-63: bits 0-1 rojo, 2-3 verde,
;                    4-5 azul)
;    vdp_reiniciar   los registros como al encender
;    vdp_tablas      (una vez) las tablas para pasar los planos a puntos
;    sms_in, sms_out los puertos (z_in y z_out del procesador)
; =============================================================================

CICLOS_LINEA equ 228
LINEAS      equ 262

        align 4
; los colores del TMS9918, en los de la consola (0: el del fondo)
tms_pal     db 0x00, 0x00, 0x08, 0x0C, 0x10, 0x30, 0x01, 0x3C
            db 0x02, 0x03, 0x05, 0x0F, 0x04, 0x33, 0x15, 0x3F

vdp_tablas:
        pushad
        xor ebx, ebx                ; cada octeto: ocho puntos de 0 o 1 (bit 7
.o:                                 ; el de la izquierda), y al revés
        mov ecx, 8
        mov dl, bl
        lea edi, [tab_bits+ebx*8]
.b:
        xor eax, eax
        shl dl, 1
        adc eax, 0
        mov [edi], al
        mov [tab_bitsr+ebx*8+ecx-1], al
        inc edi
        dec ecx
        jnz .b
        inc ebx
        cmp ebx, 256
        jb .o
        popad
        ret

vdp_reiniciar:
        pushad
        mov edi, vdp_reg
        mov ecx, 16
        xor eax, eax
        rep stosb
        mov [vdp_dir], eax
        mov [vdp_cod], al
        mov [vdp_segundo], al
        mov [vdp_buf], al
        mov [vdp_st], al
        mov [vdp_linpend], al
        mov byte [vdp_reg+10], 0xFF
        mov byte [vdp_cnt], 0xFF
        mov [linea], eax
        popad
        ret

; la patilla de interrupción, según el estado y los registros 0 y 1
vdp_irq:
        push eax
        xor eax, eax
        test byte [vdp_st], 0x80
        jz .l
        test byte [vdp_reg+1], 0x20
        jz .l
        inc eax
.l:
        cmp byte [vdp_linpend], 0
        je .x
        test byte [vdp_reg], 0x10
        jz .x
        mov al, 1
.x:
        cmp al, [irq_linea]
        je .y
        mov [irq_linea], al
        mov byte [z_mira], 1
.y:
        pop eax
        ret

; =============================================================================
;  Los puertos: EDX=puerto, AL (conservan todo menos EAX y EBP)
; =============================================================================
sms_out:
        mov ebp, edx
        and ebp, 0xC1
        cmp ebp, 0x40
        jb .ctl
        cmp ebp, 0x80
        jb near psg_escribe
        cmp ebp, 0x80
        je .datos
        cmp ebp, 0x81
        je .control
        ret
.ctl:
        test ebp, 1
        jnz .io
        mov [mem_ctl], al
        ret
.io:
        mov [io_ctl], al
        ret
.datos:
        mov byte [vdp_segundo], 0
        push ebx
        mov ebx, [vdp_dir]
        mov [vdp_buf], al
        cmp byte [vdp_cod], 3
        je .paleta
        mov [vram+ebx], al
        jmp .sig
.paleta:
        and ebx, 31
        and al, 0x3F
        mov [cram+ebx], al
        mov ebx, [vdp_dir]
.sig:
        inc ebx
        and ebx, 0x3FFF
        mov [vdp_dir], ebx
        pop ebx
        ret
.control:
        cmp byte [vdp_segundo], 0
        jne .seg
        mov [vdp_primero], al
        mov byte [vdp_segundo], 1
        mov [vdp_dir], al           ; (el octeto bajo de la dirección, ya)
        ret
.seg:
        mov byte [vdp_segundo], 0
        push ebx
        mov bl, al
        shr bl, 6
        mov [vdp_cod], bl
        movzx ebx, al
        and ebx, 0x3F
        shl ebx, 8
        mov bl, [vdp_primero]
        mov [vdp_dir], ebx
        cmp byte [vdp_cod], 2
        je .registro
        ja .x
        cmp byte [vdp_cod], 0
        jne .x
        mov al, [vram+ebx]          ; leer: el búfer se llena ya
        mov [vdp_buf], al
        inc ebx
        and ebx, 0x3FFF
        mov [vdp_dir], ebx
        jmp .x
.registro:
        and eax, 0x0F
        mov bl, [vdp_primero]
        mov [vdp_reg+eax], bl
        call vdp_irq
.x:
        pop ebx
        ret

sms_in:
        mov ebp, edx
        and ebp, 0xC1
        cmp ebp, 0x40
        jb .ff
        cmp ebp, 0x41
        je .h
        jb .v
        cmp ebp, 0x80
        je .datos
        cmp ebp, 0x81
        je .estado
        cmp ebp, 0xC0
        je .dc
        jmp .dd
.ff:
        mov al, 0xFF
        ret
.v:                                 ; el contador de líneas (NTSC, 192 líneas:
        mov eax, [linea]            ; 00h-DAh y luego D5h-FFh)
        cmp eax, 0xDA
        jbe .v1
        sub eax, 6
.v1:
        ret
.h:                                 ; el de puntos: por dónde va la línea
        mov eax, CICLOS_LINEA
        sub eax, edi
        jns .h1
        xor eax, eax
.h1:
        cmp eax, CICLOS_LINEA
        jb .h2
        mov eax, CICLOS_LINEA - 1
.h2:
        imul eax, 3                 ; (171 por línea: 3/4 de ciclo)
        shr eax, 2
        ret
.datos:
        mov byte [vdp_segundo], 0
        push ebx
        mov ebx, [vdp_dir]
        mov ah, [vram+ebx]
        mov al, [vdp_buf]
        mov [vdp_buf], ah
        inc ebx
        and ebx, 0x3FFF
        mov [vdp_dir], ebx
        pop ebx
        ret
.estado:
        mov al, [vdp_st]
        test byte [vdp_reg], 0x04
        jz .e1
        or al, 0x1F                 ; (en el modo 4, los bits bajos no dicen nada)
.e1:
        mov byte [vdp_st], 0
        mov byte [vdp_linpend], 0
        mov byte [vdp_segundo], 0
        call vdp_irq
        ret
.dc:                                ; los mandos: 0 = pulsado
        push ebx
        mov al, [pad_estado]
        and al, 0x3F
        mov bl, [pad_estado+1]
        shl bl, 6
        or al, bl
        not al
        mov bl, [io_ctl]            ; TR del primero, si es salida
        test bl, 0x01
        jnz .dc1
        and al, ~0x20
        test bl, 0x10
        jz .dc1
        or al, 0x20
.dc1:
        pop ebx
        ret
.dd:
        push ebx
        mov al, [pad_estado+1]
        shr al, 2
        and al, 0x0F
        not al                      ; bits 4 (reinicio: suelto) y 5, a 1
        or al, 0xF0
        mov bl, [io_ctl]
        test bl, 0x02               ; TH del primero: si es salida, lo que se
        jnz .dd1                    ; le ha puesto (consola de fuera de Japón)
        and al, ~0x40
        test bl, 0x20
        jz .dd1
        or al, 0x40
.dd1:
        test bl, 0x08               ; TH del segundo
        jnz .dd2
        and al, ~0x80
        test bl, 0x80
        jz .dd2
        or al, 0x80
.dd2:
        test bl, 0x04               ; TR del segundo
        jnz .dd3
        and al, ~0x08
        test bl, 0x40
        jz .dd3
        or al, 0x08
.dd3:
        pop ebx
        ret

; =============================================================================
;  Un cuadro
; =============================================================================
sms_cuadro:
        pushad
        mov al, [pausa_tecla]       ; el botón de pausa: una NMI al pulsarlo
        cmp al, [pausa_ant]
        je .p
        mov [pausa_ant], al
        test al, al
        jz .p
        mov byte [nmi_pend], 1
.p:
        mov al, [vdp_reg+9]         ; el desplazamiento vertical vale para
        mov [vdp_vs], al            ; todo el cuadro
        xor eax, eax
        mov [linea], eax
.l:
        cmp dword [linea], 192
        jae .cnt
        call vdp_linea
.cnt:
        cmp dword [linea], 192      ; el contador de líneas: baja en 0-192 y
        ja .recarga                 ; al pasar de 0 avisa
        sub byte [vdp_cnt], 1
        jnc .c1
        mov byte [vdp_linpend], 1
.recarga:
        mov al, [vdp_reg+10]
        mov [vdp_cnt], al
.c1:
        cmp dword [linea], 0xC1     ; la interrupción de cuadro
        jne .c2
        or byte [vdp_st], 0x80
.c2:
        call vdp_irq
        mov edi, CICLOS_LINEA
        call z80_correr
        call psg_linea
        inc dword [linea]
        cmp dword [linea], LINEAS
        jb .l
        inc dword [cuadros]
        popad
        ret

; ---- una línea ([linea] < 192) ----
vdp_linea:
        pushad
        test byte [vdp_reg], 0x04
        jz near vdp_tms
        test byte [vdp_reg+1], 0x40
        jz near .apagada
        cmp byte [dibujar], 0
        je near .objetos
        ; ---- el fondo ----
        mov eax, [linea]
        movzx ebx, byte [vdp_reg+8] ; el desplazamiento horizontal
        test byte [vdp_reg], 0x40
        jz .h
        cmp eax, 16
        jae .h
        xor ebx, ebx
.h:
        mov ecx, ebx
        and ecx, 7
        lea edi, [lin_buf+ecx]      ; EDI: donde va el primer dibujo
        mov [r_fino], ecx
        shr ebx, 3
        neg ebx
        and ebx, 31                 ; EBX: la columna de la tabla del primero
        movzx edx, byte [vdp_reg+2]
        and edx, 0x0E
        shl edx, 10
        mov [r_nt], edx
        xor ebp, ebp                ; EBP: el dibujo (0-31) en la pantalla
.t:
        movzx eax, byte [vdp_vs]
        cmp ebp, 24                 ; las columnas de la derecha, fijas
        jb .v
        test byte [vdp_reg], 0x80
        jz .v
        xor eax, eax
.v:
        add eax, [linea]
.v1:
        cmp eax, 224
        jb .v2
        sub eax, 224
        jmp .v1
.v2:
        mov ecx, eax
        and ecx, 7                  ; la fila dentro del dibujo
        shr eax, 3
        shl eax, 5
        add eax, ebx
        shl eax, 1
        add eax, [r_nt]
        movzx edx, word [vram+eax]  ; la casilla
        test edx, 0x400             ; girado arriba-abajo
        jz .v3
        xor ecx, 7
.v3:
        mov eax, edx
        and eax, 0x1FF
        shl eax, 5
        lea esi, [vram+eax+ecx*4]   ; la fila del dibujo: sus cuatro planos
        push ebx
        push ebp
        mov ebp, tab_bits
        test edx, 0x200             ; girado izquierda-derecha
        jz .v4
        mov ebp, tab_bitsr
.v4:
        movzx eax, byte [esi+3]
        mov ebx, [ebp+eax*8]
        mov ecx, [ebp+eax*8+4]
        shl ebx, 1
        shl ecx, 1
        movzx eax, byte [esi+2]
        or ebx, [ebp+eax*8]
        or ecx, [ebp+eax*8+4]
        shl ebx, 1
        shl ecx, 1
        movzx eax, byte [esi+1]
        or ebx, [ebp+eax*8]
        or ecx, [ebp+eax*8+4]
        shl ebx, 1
        shl ecx, 1
        movzx eax, byte [esi]
        or ebx, [ebp+eax*8]
        or ecx, [ebp+eax*8+4]
        test edx, 0x1000            ; delante de los objetos: +40h donde no es 0
        jz .v5
        call .delante
.v5:
        test edx, 0x800             ; la segunda paleta
        jz .v6
        or ebx, 0x10101010
        or ecx, 0x10101010
.v6:
        mov [edi], ebx
        mov [edi+4], ecx
        add edi, 8
        pop ebp
        pop ebx
        inc ebx
        and ebx, 31
        inc ebp
        cmp ebp, 32
        jb .t
        mov ecx, [r_fino]           ; lo que se ha salido por la derecha, a la
        mov esi, lin_buf + 256      ; izquierda
        mov edi, lin_buf
        rep movsb
.objetos:
        call vdp_objetos4
        cmp byte [dibujar], 0
        je .x
        test byte [vdp_reg], 0x20   ; la columna de la izquierda, tapada
        jz .pinta
        mov al, [vdp_reg+7]
        and al, 0x0F
        or al, 0x10
        mov ah, al
        mov [lin_buf], ax
        mov [lin_buf+2], ax
        mov [lin_buf+4], ax
        mov [lin_buf+6], ax
.pinta:
        mov edi, [linea]            ; con los colores de la paleta
        shl edi, 8
        add edi, imagen
        mov esi, lin_buf
        mov ecx, 256
        xor eax, eax
.p:
        mov al, [esi]
        and al, 0x1F
        mov al, [cram+eax]
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .p
.x:
        popad
        ret
.apagada:                           ; la imagen apagada: el color del borde
        cmp byte [dibujar], 0
        je .x
        movzx eax, byte [vdp_reg+7]
        and eax, 0x0F
        mov al, [cram+16+eax]
        mov ah, al
        mov edx, eax
        shl eax, 16
        mov ax, dx
        mov edi, [linea]
        shl edi, 8
        add edi, imagen
        mov ecx, 64
        rep stosd
        jmp .x
; (interna) EBX:ECX = los ocho puntos: los que no son 0, con 40h
.delante:
        push edx
        mov edx, ebx
        call .d4
        or ebx, eax
        mov edx, ecx
        call .d4
        or ecx, eax
        pop edx
        ret
.d4:                                ; EDX = cuatro puntos -> EAX = 40h en los que no son 0
        xor eax, eax
        test dl, dl
        jz .d1
        or eax, 0x40
.d1:
        test dh, dh
        jz .d2
        or eax, 0x4000
.d2:
        shr edx, 16
        test dl, dl
        jz .d3
        or eax, 0x400000
.d3:
        test dh, dh
        jz .d5
        or eax, 0x40000000
.d5:
        ret

; ---- los objetos del modo 4 en la línea [linea] ----
vdp_objetos4:
        pushad
        movzx esi, byte [vdp_reg+5]
        and esi, 0x7E
        shl esi, 7                  ; la tabla de los objetos
        mov ecx, 8                  ; el alto
        test byte [vdp_reg+1], 0x02
        jz .a1
        mov ecx, 16
.a1:
        test byte [vdp_reg+1], 0x01
        jz .a2
        shl ecx, 1
.a2:
        mov [r_alto], ecx
        xor ebx, ebx                ; el objeto
        xor ebp, ebp                ; cuántos en la línea
.e:
        movzx eax, byte [vram+esi+ebx]
        cmp eax, 0xD0               ; el final de la lista
        je .dib
        mov edx, [linea]
        sub edx, eax
        dec edx
        and edx, 0xFF               ; la fila del objeto
        cmp edx, [r_alto]
        jae .s
        cmp ebp, 8
        jb .e1
        or byte [vdp_st], 0x40      ; más de ocho
        jmp .dib
.e1:
        shl edx, 8
        or edx, ebx
        mov [r_obj+ebp*4], edx
        inc ebp
.s:
        inc ebx
        cmp ebx, 64
        jb .e
.dib:
        test ebp, ebp
        jz near .x
        mov [r_nobj], ebp
        mov edi, spr_buf
        mov ecx, 256 / 4
        xor eax, eax
        rep stosd
        xor ebp, ebp
.o:
        mov edx, [r_obj+ebp*4]
        movzx ebx, dl               ; el objeto
        shr edx, 8                  ; su fila
        movzx eax, byte [vram+esi+0x80+ebx*2]   ; x
        test byte [vdp_reg], 0x08
        jz .o1
        sub eax, 8
.o1:
        mov [r_x], eax
        movzx eax, byte [vram+esi+0x81+ebx*2]   ; el dibujo
        test byte [vdp_reg+6], 0x04
        jz .o2
        or eax, 0x100
.o2:
        test byte [vdp_reg+1], 0x01
        jz .o3
        shr edx, 1
.o3:
        test byte [vdp_reg+1], 0x02
        jz .o4
        and eax, 0x1FE
        cmp edx, 8
        jb .o4
        inc eax
        sub edx, 8
.o4:
        shl eax, 5
        lea eax, [vram+eax+edx*4]
        push esi
        push ebp
        mov esi, eax
        movzx eax, byte [esi+3]     ; los ocho puntos, en r_px
        mov ebx, [tab_bits+eax*8]
        mov ecx, [tab_bits+eax*8+4]
        shl ebx, 1
        shl ecx, 1
        movzx eax, byte [esi+2]
        or ebx, [tab_bits+eax*8]
        or ecx, [tab_bits+eax*8+4]
        shl ebx, 1
        shl ecx, 1
        movzx eax, byte [esi+1]
        or ebx, [tab_bits+eax*8]
        or ecx, [tab_bits+eax*8+4]
        shl ebx, 1
        shl ecx, 1
        movzx eax, byte [esi]
        or ebx, [tab_bits+eax*8]
        or ecx, [tab_bits+eax*8+4]
        mov [r_px], ebx
        mov [r_px+4], ecx
        mov edx, [r_x]
        xor ecx, ecx                ; el punto (0-7)
        mov ebp, 1                  ; cuántas veces cada uno
        test byte [vdp_reg+1], 0x01
        jz .p
        inc ebp
.p:
        movzx eax, byte [r_px+ecx]
        mov ebx, ebp
.p1:
        test eax, eax
        jz .p3
        cmp edx, 256
        jae .p3                     ; (fuera: a la izquierda o a la derecha)
        cmp byte [spr_buf+edx], 0
        je .p2
        or byte [vdp_st], 0x20      ; choque
        jmp .p3
.p2:
        mov byte [spr_buf+edx], 1
        test byte [lin_buf+edx], 0x40
        jnz .p3
        or al, 0x10
        mov [lin_buf+edx], al
        and al, 0x0F
.p3:
        inc edx
        dec ebx
        jnz .p1
        inc ecx
        cmp ecx, 8
        jb .p
        pop ebp
        pop esi
        inc ebp
        cmp ebp, [r_nobj]
        jb .o
.x:
        popad
        ret

; =============================================================================
;  Los modos del TMS9918 (sin el bit del modo 4). Entra con pushad hecho
; =============================================================================
vdp_tms:
        test byte [vdp_reg+1], 0x40
        jz near .apagada
        mov eax, [linea]
        mov edi, [linea]
        shl edi, 8
        add edi, imagen
        mov [r_dest], edi
        cmp byte [dibujar], 0
        je near .objetos
        movzx ebx, byte [vdp_reg+7]
        and ebx, 0x0F
        mov bl, [tms_pal+ebx]
        mov [r_fondo], bl
        test byte [vdp_reg+1], 0x10
        jnz near .texto
        test byte [vdp_reg+1], 0x08
        jnz near .multi
        ; ---- gráfico I y II ----
        movzx edx, byte [vdp_reg+2]
        and edx, 0x0F
        shl edx, 10
        mov ecx, eax
        shr ecx, 3
        shl ecx, 5
        add edx, ecx                ; EDX: la fila de la tabla de nombres
        xor ebp, ebp
.g:
        movzx ebx, byte [vram+edx+ebp]  ; el nombre
        test byte [vdp_reg], 0x02
        jnz .g2
        mov ecx, ebx                ; gráfico I: el color, cada ocho nombres
        shr ecx, 3
        movzx esi, byte [vdp_reg+3]
        shl esi, 6
        mov cl, [vram+esi+ecx]
        movzx esi, byte [vdp_reg+4]
        and esi, 7
        shl esi, 11
        lea esi, [esi+ebx*8]
        mov eax, [linea]
        and eax, 7
        mov ch, [vram+esi+eax]      ; CH: los puntos, CL: los colores
        jmp .gpinta
.g2:
        mov eax, [linea]            ; gráfico II: un tercio de la pantalla
        shr eax, 6                  ; por cada 256 nombres
        shl eax, 8
        add ebx, eax
        push edx
        movzx eax, byte [vdp_reg+4]
        and eax, 3
        shl eax, 8
        or eax, 0xFF
        and eax, ebx
        shl eax, 3
        movzx esi, byte [vdp_reg+4]
        and esi, 4
        shl esi, 11
        add esi, eax
        mov eax, [linea]
        and eax, 7
        add esi, eax
        mov ch, [vram+esi]
        movzx eax, byte [vdp_reg+3]
        and eax, 0x7F
        shl eax, 3
        or eax, 7
        and eax, ebx
        shl eax, 3
        movzx esi, byte [vdp_reg+3]
        and esi, 0x80
        shl esi, 6
        add esi, eax
        mov eax, [linea]
        and eax, 7
        add esi, eax
        mov cl, [vram+esi]
        pop edx
.gpinta:
        mov esi, 8
        call tms_puntos
        inc ebp
        cmp ebp, 32
        jb .g
        jmp .objetos
.texto:                             ; 40 columnas de 6 puntos
        mov ecx, 8
        call tms_borde
        movzx edx, byte [vdp_reg+2]
        and edx, 0x0F
        shl edx, 10
        mov ecx, eax
        shr ecx, 3
        imul ecx, 40
        add edx, ecx
        xor ebp, ebp
.tx:
        movzx ebx, byte [vram+edx+ebp]
        movzx esi, byte [vdp_reg+4]
        and esi, 7
        shl esi, 11
        lea esi, [esi+ebx*8]
        mov eax, [linea]
        and eax, 7
        mov ch, [vram+esi+eax]
        mov cl, [vdp_reg+7]
        mov esi, 6
        call tms_puntos
        inc ebp
        cmp ebp, 40
        jb .tx
        mov ecx, 8
        call tms_borde
        jmp .x                      ; (en el modo de texto no hay objetos)
.multi:                             ; bloques de 4x4
        movzx edx, byte [vdp_reg+2]
        and edx, 0x0F
        shl edx, 10
        mov ecx, eax
        shr ecx, 3
        shl ecx, 5
        add edx, ecx
        xor ebp, ebp
.mc:
        movzx ebx, byte [vram+edx+ebp]
        movzx esi, byte [vdp_reg+4]
        and esi, 7
        shl esi, 11
        lea esi, [esi+ebx*8]
        mov eax, [linea]
        mov ecx, eax
        shr ecx, 3
        and ecx, 3
        shl ecx, 1
        shr eax, 2
        and eax, 1
        add ecx, eax
        mov cl, [vram+esi+ecx]      ; arriba-izquierda y derecha: los dos nibbles
        mov ch, 0xF0
        mov esi, 8
        call tms_puntos
        inc ebp
        cmp ebp, 32
        jb .mc
.objetos:
        call vdp_objetos_tms
.x:
        popad
        ret
.apagada:
        cmp byte [dibujar], 0
        je .x
        movzx eax, byte [vdp_reg+7]
        and eax, 0x0F
        mov al, [tms_pal+eax]
        mov edi, [linea]
        shl edi, 8
        add edi, imagen
        mov ecx, 256
        rep stosb
        jmp .x

; (interna) ESI puntos (8 o 6) de CH (bit 7 el primero) con los colores de
; CL (alto: el 1, bajo: el 0), a [r_dest]
tms_puntos:
        push eax
        push ebx
        push edi
        mov edi, [r_dest]
.p:
        mov al, cl
        shl ch, 1
        jc .uno
        and eax, 0x0F
        jmp .c
.uno:
        shr al, 4
        and eax, 0x0F
.c:
        test eax, eax
        jz .fondo
        mov al, [tms_pal+eax]
        jmp .w
.fondo:
        mov al, [r_fondo]
.w:
        mov [edi], al
        inc edi
        dec esi
        jnz .p
        mov [r_dest], edi
        pop edi
        pop ebx
        pop eax
        ret
; (interna) ECX puntos del color del fondo
tms_borde:
        push eax
        push edi
        mov edi, [r_dest]
        mov al, [r_fondo]
        rep stosb
        mov [r_dest], edi
        pop edi
        pop eax
        ret

; los 32 objetos del TMS9918: cuatro por línea; el quinto, avisa
vdp_objetos_tms:
        pushad
        test byte [vdp_reg+1], 0x10 ; (en el de texto no hay)
        jnz near .x
        movzx esi, byte [vdp_reg+5]
        and esi, 0x7F
        shl esi, 7
        mov ecx, 8
        test byte [vdp_reg+1], 0x02
        jz .a1
        mov ecx, 16
.a1:
        test byte [vdp_reg+1], 0x01
        jz .a2
        shl ecx, 1
.a2:
        mov [r_alto], ecx
        xor ebx, ebx
        xor ebp, ebp
.e:
        movzx eax, byte [vram+esi+ebx*4]
        cmp eax, 0xD0
        je .dib
        mov edx, [linea]
        sub edx, eax
        dec edx
        and edx, 0xFF
        cmp edx, [r_alto]
        jae .s
        cmp ebp, 4
        jb .e1
        test byte [vdp_st], 0x40    ; el quinto: su número
        jnz .dib
        mov al, [vdp_st]
        and al, 0xE0
        or al, 0x40
        or al, bl
        mov [vdp_st], al
        jmp .dib
.e1:
        shl edx, 8
        or edx, ebx
        mov [r_obj+ebp*4], edx
        inc ebp
.s:
        inc ebx
        cmp ebx, 32
        jb .e
.dib:
        test ebp, ebp
        jz near .x
        mov [r_nobj], ebp
        mov edi, spr_buf
        mov ecx, 256 / 4
        xor eax, eax
        rep stosd
        xor ebp, ebp
.o:
        mov edx, [r_obj+ebp*4]
        movzx ebx, dl
        shr edx, 8
        movzx eax, byte [vram+esi+ebx*4+1]  ; x
        movzx ecx, byte [vram+esi+ebx*4+3]  ; el color
        test cl, 0x80               ; 32 puntos a la izquierda
        jz .o1
        sub eax, 32
.o1:
        mov [r_x], eax
        and ecx, 0x0F
        mov [r_col], ecx
        movzx eax, byte [vram+esi+ebx*4+2]  ; el dibujo
        test byte [vdp_reg+1], 0x01
        jz .o2
        shr edx, 1
.o2:
        movzx ebx, byte [vdp_reg+6]
        and ebx, 7
        shl ebx, 11
        mov ecx, 8                  ; los puntos del objeto
        test byte [vdp_reg+1], 0x02
        jz .o3
        and eax, 0xFC
        mov ecx, 16
.o3:
        shl eax, 3
        add ebx, eax
        add ebx, edx
        mov ah, [vram+ebx]          ; AH: los ocho de la izquierda, AL: los de
        mov al, [vram+ebx+16]       ; la derecha (en los de 16)
        push esi
        push ebp
        mov esi, ecx
        mov edx, [r_x]
        mov ebp, 1
        test byte [vdp_reg+1], 0x01
        jz .p
        inc ebp
.p:
        shl ax, 1
        setc cl
        mov ebx, ebp
.p1:
        test cl, cl
        jz .p3
        cmp edx, 256
        jae .p3
        cmp byte [spr_buf+edx], 0
        je .p2
        or byte [vdp_st], 0x20
        jmp .p3
.p2:
        mov byte [spr_buf+edx], 1
        cmp byte [dibujar], 0
        je .p3
        push eax
        mov eax, [r_col]
        test eax, eax
        jz .p4
        mov al, [tms_pal+eax]
        mov ch, al
        mov eax, [linea]
        shl eax, 8
        add eax, edx
        mov [imagen+eax], ch
.p4:
        pop eax
.p3:
        inc edx
        dec ebx
        jnz .p1
        dec esi
        jnz .p
        pop ebp
        pop esi
        inc ebp
        cmp ebp, [r_nobj]
        jb .o
.x:
        popad
        ret
