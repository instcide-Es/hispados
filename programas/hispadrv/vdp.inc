; =============================================================================
;  Hispa-Drive - la imagen: el VDP de la consola (el sucesor del de la de 8
;  bits: 64 KB de VRAM, dos planos con desplazamiento, la ventana y 80
;  objetos)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escrito desde la documentación pública del VDP (los registros, los
;  puertos de datos y de control, las tres DMA, las interrupciones de línea
;  y de cuadro, los contadores H y V):
;    - el puerto de control: un registro (10xR RRRR DDDD DDDD) o una orden
;      en dos palabras (CD1 CD0 A13-A0, luego 0000 0000 CD5-CD2 00 A15 A14);
;      CD = 1 VRAM, 3 CRAM, 5 VSRAM para escribir; 0, 8 y 4 para leer; con
;      CD5 (y el bit 4 del registro 1), una DMA: copia del 68000 (registro
;      23, bit 7 a 0), relleno (10) o copia de la VRAM (11);
;    - la imagen: 320 o 256 puntos (registro 12), 224 líneas; el plano B, el
;      plano A (o la ventana, registros 17 y 18, donde toque) y los objetos,
;      cada uno con dos prioridades: fondo, B, A, objetos, B alto, A alto,
;      objetos altos. Desplazamiento horizontal de toda la pantalla, por
;      filas de 8 o por línea (la tabla del registro 13); vertical de toda la
;      pantalla o por columnas de 16 puntos (la VSRAM).
;  Cada línea se pinta de una vez (en md_cuadro, después de dejar correr al
;  68000 el borde de la línea), a lin: índices de la CRAM (0-63), que se
;  pasan al final a los colores del equipo con v_col (al que le da los
;  colores quien llama, en md_col9: los 512 de la consola).
;
;    vdp_arrancar    la memoria del VDP a cero y sus tablas
;    vdp_reiniciar   los registros
;    vdp_linea       EAX=línea: la pinta en la fila de «imagen»
;    vdp_l8...       los puertos (las rutinas lentas del 68000)
; =============================================================================

LIN_M       equ 32                  ; margen de lin a cada lado
R_ALTOS     equ 192                 ; casillas de prioridad alta por línea

; cuatro puntos: %1 tabla de puntos, %2 de máscaras, %3 %4 los dos octetos
; (en el orden en que se pintan), %5 desplazamiento en EDI. Gasta ECX, EBP
%macro PINTA4 5
        movzx ecx, %4
        movzx ebp, %3
        mov cx, [%1+ecx*2]
        shl ecx, 16
        mov cx, [%1+ebp*2]
        push eax
        movzx eax, %4
        mov ax, [%2+eax*2]
        shl eax, 16
        mov ax, [%2+ebp*2]
        ; ECX = cuatro puntos, EAX = su máscara
        or ecx, [r_pal4+ebx]        ; (la paleta en los cuatro)
        and ecx, eax
        not eax
        and eax, [edi+%5]
        or eax, ecx
        mov [edi+%5], eax
        pop eax
%endmacro

vdp_arrancar:
        pushad
        mov edi, vram
        mov ecx, 0x10000 / 4
        xor eax, eax
        rep stosd
        mov edi, cram
        mov ecx, (64 * 2 + 80) / 4
        rep stosd
        ; las tablas: cada octeto de un dibujo, en dos puntos (el de los 4
        ; bits altos primero) y su máscara (FFh donde no es transparente)
        xor ebx, ebx
.t:
        mov eax, ebx
        shr eax, 4
        mov edx, ebx
        and edx, 15
        mov [tab_px+ebx*2], al
        mov [tab_px+ebx*2+1], dl
        mov [tab_pxr+ebx*2], dl     ; (girado: el de los bits bajos primero)
        mov [tab_pxr+ebx*2+1], al
        xor ecx, ecx
        test al, al
        jz .t1
        mov cl, 0xFF
.t1:
        test dl, dl
        jz .t2
        mov ch, 0xFF
.t2:
        mov [tab_mk+ebx*2], cx
        xchg cl, ch
        mov [tab_mkr+ebx*2], cx
        inc ebx
        cmp ebx, 256
        jb .t
        call vdp_colores
        popad
        ret

; los 64 colores de la CRAM, a los del equipo (tras cambiar md_col9)
vdp_colores:
        pushad
        xor ebx, ebx
.c:
        movzx eax, word [cram+ebx*2]
        call vdp_col_pon
        inc ebx
        cmp ebx, 64
        jb .c
        popad
        ret
; EBX=entrada AX=su valor (----BBB-GGG-RRR-)
vdp_col_pon:
        push eax
        push edx
        movzx edx, ax
        shr edx, 1                  ; -> 9 bits: BBBGGGRRR
        mov eax, edx
        and eax, 7
        mov ebp, edx
        shr ebp, 1
        and ebp, 0x38
        or eax, ebp
        mov ebp, edx
        shr ebp, 2
        and ebp, 0x1C0
        or eax, ebp
        mov al, [md_col9+eax]
        mov [v_col+ebx], al
        pop edx
        pop eax
        ret

vdp_reiniciar:
        pushad
        mov edi, v_reg
        mov ecx, 32 / 4
        xor eax, eax
        rep stosd
        mov [v_dir], eax
        mov [v_cod], al
        mov [v_pend], al
        mov [v_relleno], al
        mov [v_vint], al
        mov [v_hint], al
        mov [v_st], al
        mov [v_buf], ax
        mov byte [v_hcnt], 0
        mov dword [v_ancho], 256
        popad
        ret

; el nivel de interrupción que se pide al 68000 (6 cuadro, 4 línea)
vdp_irq:
        push eax
        xor eax, eax
        cmp byte [v_hint], 0
        je .a
        test byte [v_reg+0], 0x10
        jz .a
        mov al, 4
.a:
        cmp byte [v_vint], 0
        je .b
        test byte [v_reg+1], 0x20
        jz .b
        mov al, 6
.b:
        cmp al, [m_ipl]
        je .x
        mov [m_ipl], al
        cmp al, 0                   ; (una nueva: que el 68000 la mire ya)
        je .x
        cmp byte [m_corre], 0
        je .x
        call m_pide_mirar
.x:
        pop eax
        ret

; el 68000 acepta la interrupción EAX: se quita la que era
md_irq_ack:
        cmp eax, 6
        jne .h
        mov byte [v_vint], 0
        jmp vdp_irq
.h:
        cmp eax, 4
        jne .x
        mov byte [v_hint], 0
        jmp vdp_irq
.x:
        ret

; =============================================================================
;  Los puertos (desde el 68000): EDX=dirección
; =============================================================================
vdp_l16:
        mov ebp, edx
        and ebp, 0x1E
        cmp ebp, 4
        jb .datos
        cmp ebp, 8
        jb .estado
        cmp ebp, 0x10
        jb .hv
        mov eax, 0xFFFF
        ret
.datos:
        jmp vdp_lee_datos
.estado:
        jmp vdp_estado
.hv:
        jmp vdp_hv
vdp_l8:
        push edx
        and edx, ~1
        call vdp_l16
        pop edx
        test edx, 1
        jnz .x
        shr eax, 8
.x:
        and eax, 0xFF
        ret

vdp_e8:
        mov ebp, edx
        and ebp, 0x1F
        cmp ebp, 0x10
        jb .v
        cmp ebp, 0x18
        jae .x
        test ebp, 1
        jz .x
        jmp psg_escribe
.v:
        push eax                    ; (un octeto: la palabra, con él dos veces)
        mov ah, al
        call vdp_e16
        pop eax
.x:
        ret
vdp_e16:
        mov ebp, edx
        and ebp, 0x1F
        cmp ebp, 4
        jb vdp_esc_datos
        cmp ebp, 8
        jb vdp_esc_control
        cmp ebp, 0x10
        jb .x
        cmp ebp, 0x18
        jae .x
        jmp psg_escribe             ; (el octeto bajo)
.x:
        ret

; ---- el estado ----
vdp_estado:
        mov byte [v_pend], 0
        movzx eax, byte [v_st]      ; 80h cuadro, 40h demasiados, 20h choque
        and byte [v_st], 0x9F       ; (al leerlo se quitan los dos de objetos)
        or eax, 0x3600              ; (la FIFO, vacía)
        mov ebp, [v_linea]
        cmp ebp, [v_alto]
        jae .vb
        test byte [v_reg+1], 0x40
        jnz .novb
.vb:
        or al, 0x08
.novb:
        push eax
        call ciclos_linea
        cmp eax, 420                ; (el borde: el final de la línea)
        pop eax
        jb .nohb
        or al, 0x04
.nohb:
        cmp byte [region], 0xC0
        jne .x
        or al, 0x01
.x:
        ret

; -> EAX=los ciclos del 68000 que van de esta línea (0-487)
ciclos_linea:
        mov eax, [m_presup]
        sub eax, edi
        cmp byte [m_corre], 0
        jne .s
        xor eax, eax
.s:
        add eax, [v_cic0]
        cmp eax, 487
        jbe .x
        mov eax, 487
.x:
        ret

; ---- los contadores (V en el octeto alto, H en el bajo) ----
vdp_hv:
        mov eax, [v_linea]
        cmp byte [region], 0xC0
        je .pal
        cmp eax, 0xEA               ; NTSC: 00-EA y luego E5-FF
        jbe .v
        sub eax, 6
        jmp .v
.pal:
        cmp eax, 0x102              ; PAL: 00-102 y luego CA-FF
        jbe .v
        sub eax, 0x39
.v:
        and eax, 0xFF
        shl eax, 8
        push eax
        call ciclos_linea           ; H: en 320 puntos, 00-B6 y luego E4-FF
        imul eax, 210
        shr eax, 9
        cmp eax, 0xB6
        jbe .h
        add eax, 0x2D
.h:
        and eax, 0xFF
        pop ebp
        or eax, ebp
        ret

; ---- el puerto de control ----
vdp_esc_control:
        push eax
        push ecx
        push edx
        movzx eax, ax
        cmp byte [v_pend], 0
        jne .segunda
        mov ecx, eax
        and ecx, 0xC000
        cmp ecx, 0x8000
        je .registro
        mov byte [v_pend], 1        ; la primera palabra de una orden
        mov ecx, eax
        and ecx, 0x3FFF
        and dword [v_dir], 0xC000
        or [v_dir], ecx
        shr eax, 14
        and byte [v_cod], 0x3C
        or [v_cod], al
        jmp .x
.registro:
        mov ecx, eax                ; un registro: R (5 bits) y D (8)
        shr ecx, 8
        and ecx, 0x1F
        cmp ecx, 24
        jae .x
        mov ah, [v_reg+ecx]
        mov [v_reg+ecx], al
        cmp ecx, 1
        ja .r12
        call vdp_irq                ; (0 y 1: las interrupciones)
        jmp .x
.r12:
        cmp ecx, 12
        jne .x
        mov dword [v_ancho], 256
        test al, 0x81
        jz .x
        mov dword [v_ancho], 320
        jmp .x
.segunda:
        mov byte [v_pend], 0
        mov ecx, eax
        and ecx, 3
        shl ecx, 14
        and dword [v_dir], 0x3FFF
        or [v_dir], ecx
        shr eax, 2
        and al, 0x3C
        and byte [v_cod], 0x03
        or [v_cod], al
        test byte [v_cod], 0x20     ; ¿una DMA?
        jz .lee
        test byte [v_reg+1], 0x10
        jz .lee
        mov al, [v_reg+23]
        test al, 0x80
        jz .dma68
        test al, 0x40
        jnz .copia
        mov byte [v_relleno], 1     ; relleno: con la siguiente escritura de datos
        jmp .x
.dma68:
        call vdp_dma68
        jmp .x
.copia:
        call vdp_dma_copia
        jmp .x
.lee:
        mov al, [v_cod]             ; para leer: el búfer, ya
        and al, 0x0F
        jnz .x
.x:
        pop edx
        pop ecx
        pop eax
        ret

; lo que dice la DMA: -> ECX=cuántas (0 = 65536)
vdp_dma_largo:
        movzx ecx, byte [v_reg+20]
        shl ecx, 8
        mov cl, [v_reg+19]
        test ecx, ecx
        jnz .x
        mov ecx, 0x10000
.x:
        ret
vdp_dma_fin:                        ; (la longitud queda a 0)
        mov byte [v_reg+19], 0
        mov byte [v_reg+20], 0
        ret

; DMA del 68000 a la VRAM, la CRAM o la VSRAM
vdp_dma68:
        pushad
        call vdp_dma_largo
        movzx edx, byte [v_reg+23]  ; la fuente: en palabras
        and edx, 0x7F
        shl edx, 8
        mov dl, [v_reg+22]
        shl edx, 8
        mov dl, [v_reg+21]
        shl edx, 1                  ; (en octetos)
        mov ebx, ecx                ; los ciclos que el 68000 se queda quieto
        shl ebx, 1                  ; (unos 2 por palabra fuera de la imagen;
        mov eax, [v_linea]          ; muchos más dentro)
        cmp eax, [v_alto]
        jae .rapida
        test byte [v_reg+1], 0x40
        jz .rapida
        imul ebx, 12
.rapida:
        cmp byte [m_corre], 0
        je .c
        sub [esp], ebx              ; (EDI, en la pila de PUSHAD)
.c:
        LEE16
        call vdp_esc_dato           ; AX al sitio de [v_dir] (y avanza)
        mov eax, edx                ; la fuente avanza dentro de su trozo de 128 KB
        add edx, 2
        and edx, 0x1FFFF
        and eax, 0xFE0000
        or edx, eax
        dec ecx
        jnz .c
        shr edx, 1                  ; la fuente, como quede
        mov [v_reg+21], dl
        mov [v_reg+22], dh
        call vdp_dma_fin
        popad
        ret

; relleno: AX=lo escrito (ya va en la VRAM): el octeto alto, a v_dir XOR 1,
; tantas veces como diga la longitud
vdp_dma_relleno:
        pushad
        mov byte [v_relleno], 0
        call vdp_dma_largo
        mov bl, ah
        mov edx, [v_dir]
        movzx esi, byte [v_reg+15]
        mov al, [v_cod]
        and al, 0x0F
        cmp al, 1
        jne .otra
.c:
        mov edi, edx
        xor edi, 1
        and edi, 0xFFFF
        mov [vram+edi], bl
        add edx, esi
        dec ecx
        jnz .c
        jmp .f
.otra:                              ; (CRAM o VSRAM: la palabra entera)
        call vdp_esc_dato
        dec ecx
        jnz .otra
        mov edx, [v_dir]
.f:
        and edx, 0xFFFF
        mov [v_dir], edx
        call vdp_dma_fin
        popad
        ret

; copia dentro de la VRAM (de octeto en octeto)
vdp_dma_copia:
        pushad
        call vdp_dma_largo
        movzx esi, byte [v_reg+22]
        shl esi, 8
        mov ebx, esi
        mov bl, [v_reg+21]
        mov esi, ebx
        mov edx, [v_dir]
        movzx edi, byte [v_reg+15]
.c:
        and esi, 0xFFFF
        and edx, 0xFFFF
        mov al, [vram+esi]
        mov [vram+edx], al
        inc esi
        add edx, edi
        dec ecx
        jnz .c
        and edx, 0xFFFF
        mov [v_dir], edx
        mov eax, esi
        mov [v_reg+21], al
        mov [v_reg+22], ah
        call vdp_dma_fin
        popad
        ret

; ---- el puerto de datos ----
vdp_esc_datos:
        mov byte [v_pend], 0
        call vdp_esc_dato
        cmp byte [v_relleno], 0
        je .x
        jmp vdp_dma_relleno
.x:
        ret

; AX -> donde dice la orden (VRAM, CRAM o VSRAM), y la dirección avanza
; (conserva todo menos EBP)
vdp_esc_dato:
        push ecx
        push eax
        mov ebp, [v_dir]
        mov cl, [v_cod]
        and cl, 0x0F
        cmp cl, 1
        je .vram
        cmp cl, 3
        je .cram
        cmp cl, 5
        je .vsram
        jmp .sig
.vram:
        and ebp, 0xFFFF
        test ebp, 1                 ; (impar: los octetos al revés)
        jz .v1
        xchg al, ah
.v1:
        and ebp, 0xFFFE
        mov [vram+ebp], ah
        mov [vram+ebp+1], al
        jmp .sig
.cram:
        shr ebp, 1
        and ebp, 63
        and eax, 0x0EEE
        mov [cram+ebp*2], ax
        push ebx
        mov ebx, ebp
        call vdp_col_pon
        pop ebx
        jmp .sig
.vsram:
        shr ebp, 1
        and ebp, 63
        cmp ebp, 40
        jae .sig
        and eax, 0x07FF
        mov [vsram+ebp*2], ax
.sig:
        movzx ecx, byte [v_reg+15]
        add ecx, [v_dir]
        and ecx, 0xFFFF
        mov [v_dir], ecx
        pop eax
        pop ecx
        ret

vdp_lee_datos:
        mov byte [v_pend], 0
        push ecx
        mov ebp, [v_dir]
        mov cl, [v_cod]
        and cl, 0x0F
        xor eax, eax
        cmp cl, 0
        je .vram
        cmp cl, 4
        je .vsram
        cmp cl, 8
        je .cram
        jmp .sig
.vram:
        and ebp, 0xFFFE
        mov ah, [vram+ebp]
        mov al, [vram+ebp+1]
        jmp .sig
.vsram:
        shr ebp, 1
        and ebp, 63
        cmp ebp, 40
        jae .sig
        mov ax, [vsram+ebp*2]
        jmp .sig
.cram:
        shr ebp, 1
        and ebp, 63
        mov ax, [cram+ebp*2]
.sig:
        movzx ecx, byte [v_reg+15]
        add ecx, [v_dir]
        and ecx, 0xFFFF
        mov [v_dir], ecx
        pop ecx
        ret

; =============================================================================
;  El dibujo de una línea
; =============================================================================
; EAX=línea (0 a v_alto-1): se pinta en imagen (320 por fila; con 256
; puntos, en medio y con el borde del color del fondo)
vdp_linea:
        pushad
        mov [r_linea], eax
        mov eax, [v_ancho]
        mov [r_ancho], eax
        ; el fondo (registro 7)
        movzx eax, byte [v_reg+7]
        and eax, 0x3F
        mov ah, al
        mov ecx, eax
        shl eax, 16
        mov ax, cx
        mov edi, lin
        mov ecx, (320 + 2 * LIN_M) / 4
        rep stosd
        test byte [v_reg+1], 0x40   ; la imagen apagada: solo el fondo
        jz near .pasa
        mov dword [r_nalto], 0
        ; ---- el plano B ----
        movzx eax, byte [v_reg+4]
        and eax, 7
        shl eax, 13
        mov [r_nt], eax
        mov dword [r_plano], 1
        mov dword [r_x0], 0
        mov eax, [r_ancho]
        mov [r_x1], eax
        call plano
        ; ---- el plano A y la ventana ----
        call ventana_cols           ; -> r_wx0, r_wx1: dónde va la ventana
        movzx eax, byte [v_reg+2]
        and eax, 0x38
        shl eax, 10
        mov [r_nt], eax
        mov dword [r_plano], 0
        mov eax, [r_wx0]            ; A a la izquierda de la ventana...
        test eax, eax
        jz .a1
        mov dword [r_x0], 0
        mov [r_x1], eax
        call plano
.a1:
        mov eax, [r_wx1]            ; ...y a su derecha
        cmp eax, [r_ancho]
        jae .a2
        mov [r_x0], eax
        mov eax, [r_ancho]
        mov [r_x1], eax
        call plano
.a2:
        mov eax, [r_wx0]
        cmp eax, [r_wx1]
        jae .a3
        call ventana
.a3:
        ; ---- los objetos de prioridad baja ----
        call objetos
        mov ah, 0x80
        call objetos_pon
        ; ---- lo de prioridad alta: B, A (y la ventana) ----
        call altos
        mov ah, 0xC0
        call objetos_pon
.pasa:
        ; a la fila de la imagen, en los colores del equipo
        mov edi, [r_linea]
        imul edi, 320
        add edi, imagen
        mov esi, lin + LIN_M
        mov ecx, [r_ancho]
        cmp ecx, 320
        je .c
        movzx eax, byte [v_reg+7]   ; (256: 32 de borde a cada lado)
        and eax, 0x3F
        mov al, [v_col+eax]
        mov ah, al
        mov edx, eax
        shl eax, 16
        mov ax, dx
        push ecx
        mov ecx, 8
        rep stosd
        pop ecx
.c:
        xor eax, eax
        xor edx, edx
        shr ecx, 2
.c1:
        mov al, [esi]
        mov dl, [esi+1]
        mov bl, [v_col+eax]
        mov bh, [v_col+edx]
        mov al, [esi+2]
        mov dl, [esi+3]
        mov [edi], bx
        mov bl, [v_col+eax]
        mov bh, [v_col+edx]
        mov [edi+2], bx
        add esi, 4
        add edi, 4
        dec ecx
        jnz .c1
        cmp dword [r_ancho], 320
        je .x
        movzx eax, byte [v_reg+7]
        and eax, 0x3F
        mov al, [v_col+eax]
        mov ah, al
        mov edx, eax
        shl eax, 16
        mov ax, dx
        mov ecx, 8
        rep stosd
.x:
        popad
        ret

; ---- un plano (r_plano: 0 A, 1 B; r_nt su tabla) entre r_x0 y r_x1: los
; dibujos de prioridad baja se pintan ya; los de alta se apuntan (r_altos)
plano:
        pushad
        ; el tamaño (registro 16): 32, 64 o 128 casillas
        movzx eax, byte [v_reg+16]
        mov ecx, eax
        and eax, 3
        mov eax, [tam_planos+eax*4]
        mov [r_cw], eax             ; ancho, en casillas
        shr ecx, 4
        and ecx, 3
        mov ecx, [tam_planos+ecx*4]
        shl ecx, 3
        dec ecx
        mov [r_hmask], ecx          ; alto en puntos - 1
        ; el desplazamiento horizontal (tabla del registro 13)
        movzx edx, byte [v_reg+13]
        and edx, 0x3F
        shl edx, 10
        mov eax, [r_linea]
        mov cl, [v_reg+11]
        and cl, 3
        jz .h0
        cmp cl, 2
        jne .h1
        and eax, ~7                 ; por filas de 8
        jmp .h1
.h0:
        xor eax, eax                ; toda la pantalla
.h1:
        shl eax, 2
        add edx, eax
        mov eax, [r_plano]
        add edx, eax
        add edx, eax
        and edx, 0xFFFF
        movzx eax, byte [vram+edx]
        shl eax, 8
        mov al, [vram+edx+1]
        neg eax
        mov ecx, [r_cw]
        shl ecx, 3
        dec ecx
        and eax, ecx                ; EAX = x del plano en el punto 0
        mov [r_px], eax
        ; las casillas, de 8 en 8 puntos
        mov ecx, eax
        and ecx, 7
        neg ecx                     ; ECX = x en la pantalla de la primera
        mov [r_sx], ecx
.tile:
        mov ecx, [r_sx]
        cmp ecx, [r_x1]
        jge near .fin
        lea eax, [ecx+8]
        cmp eax, [r_x0]
        jle near .sig
        ; el desplazamiento vertical: de toda la pantalla o de su columna
        mov eax, [r_plano]
        test byte [v_reg+11], 4
        jz .v0
        mov edx, ecx
        test edx, edx
        jns .v1
        xor edx, edx
.v1:
        shr edx, 4
        cmp edx, 19
        jbe .v2
        mov edx, 19
.v2:
        lea eax, [eax+edx*2]
.v0:
        movzx eax, word [vsram+eax*2]
        add eax, [r_linea]
        and eax, [r_hmask]
        mov ebx, eax                ; EBX = y en el plano
        ; la entrada de la tabla: fila y columna
        shr eax, 3
        imul eax, [r_cw]
        mov edx, [r_px]
        shr edx, 3
        mov ebp, [r_cw]
        dec ebp
        and edx, ebp
        add eax, edx
        add eax, eax
        add eax, [r_nt]
        and eax, 0xFFFF
        movzx edx, byte [vram+eax]
        shl edx, 8
        mov dl, [vram+eax+1]        ; EDX = la entrada
        and ebx, 7
        test edx, 0x1000            ; girado de arriba abajo
        jz .nv
        xor ebx, 7
.nv:
        mov eax, edx
        and eax, 0x7FF
        shl eax, 5
        lea eax, [eax+ebx*4]
        mov eax, [vram+eax]         ; los cuatro octetos de la fila
        test eax, eax
        jz .sig                     ; (transparente del todo)
        mov ebx, edx
        shr ebx, 9
        and ebx, 0x30               ; la paleta
        mov ebp, [r_sx]
        test edx, 0x8000
        jnz .alto
        lea edi, [lin+LIN_M+ebp]
        call casilla
        jmp .sig
.alto:                              ; prioridad alta: para luego
        call apunta_alto
.sig:
        add dword [r_sx], 8
        add dword [r_px], 8
        jmp .tile
.fin:
        popad
        ret

; pinta una fila de una casilla: EAX=los cuatro octetos, EBX=paleta (0-48),
; EDX=la entrada (bit 11: girada), EDI=en lin; solo entre r_x0 y r_x1 (EBP=x)
casilla:
        cmp ebp, [r_x0]
        jl .recorta
        lea ecx, [ebp+8]
        cmp ecx, [r_x1]
        jg .recorta
        test edx, 0x0800
        jnz .girada
        PINTA4 tab_px, tab_mk, al, ah, 0
        shr eax, 16
        PINTA4 tab_px, tab_mk, al, ah, 4
        ret
.girada:
        rol eax, 16
        PINTA4 tab_pxr, tab_mkr, ah, al, 0
        shr eax, 16
        PINTA4 tab_pxr, tab_mkr, ah, al, 4
        ret
.recorta:                           ; (de punto en punto, mirando los bordes)
        push esi
        push edx
        mov esi, edx                ; ESI = la entrada (bit 11: girada)
        xor ecx, ecx                ; ECX = el punto (0-7)
.r3:
        cmp ebp, [r_x0]
        jl .r5
        cmp ebp, [r_x1]
        jge .r6
        push ecx
        push eax
        test esi, 0x0800
        jz .r7
        xor ecx, 7
.r7:
        mov edx, ecx
        shr ecx, 1
        shl ecx, 3
        shr eax, cl
        test edx, 1
        jnz .r4
        shr eax, 4
.r4:
        and eax, 15
        jz .r8
        or eax, ebx
        mov [edi], al
.r8:
        pop eax
        pop ecx
.r5:
        inc edi
        inc ebp
        inc ecx
        cmp ecx, 8
        jb .r3
.r6:
        pop edx
        pop esi
        ret

; apunta una casilla de prioridad alta (EAX, EBX, EBP y EDX como para
; casilla, y el recorte de ahora)
apunta_alto:
        push edi
        push ecx
        mov edi, [r_nalto]
        cmp edi, R_ALTOS
        jae .x
        inc dword [r_nalto]
        shl edi, 5
        mov [r_altos+edi], eax
        mov [r_altos+edi+4], ebx
        mov [r_altos+edi+8], ebp
        mov [r_altos+edi+12], edx
        mov ecx, [r_x0]
        mov [r_altos+edi+16], ecx
        mov ecx, [r_x1]
        mov [r_altos+edi+20], ecx
.x:
        pop ecx
        pop edi
        ret

; lo de prioridad alta apuntado por los planos (B primero, luego A y la
; ventana: en el orden en que se apuntaron)
altos:
        pushad
        xor esi, esi
.c:
        cmp esi, [r_nalto]
        jae .x
        mov edi, esi
        shl edi, 5
        mov eax, [r_altos+edi+16]
        mov [r_x0], eax
        mov eax, [r_altos+edi+20]
        mov [r_x1], eax
        mov eax, [r_altos+edi]
        mov ebx, [r_altos+edi+4]
        mov ebp, [r_altos+edi+8]
        mov edx, [r_altos+edi+12]
        lea edi, [lin+LIN_M+ebp]
        call casilla
        inc esi
        jmp .c
.x:
        popad
        ret

; ---- la ventana: r_wx0 y r_wx1 (registros 17 y 18) ----
ventana_cols:
        push eax
        push ecx
        mov dword [r_wx0], 0
        mov dword [r_wx1], 0
        mov al, [v_reg+18]          ; ¿la línea entera? (de arriba o de abajo)
        mov ecx, eax
        and ecx, 0x1F
        shl ecx, 3
        test al, 0x80
        jnz .abajo
        cmp [r_linea], ecx
        jb .toda
        jmp .cols
.abajo:
        cmp [r_linea], ecx
        jae .toda
.cols:
        mov al, [v_reg+17]          ; las columnas: de la izquierda o la derecha
        mov ecx, eax
        and ecx, 0x1F
        shl ecx, 4
        cmp ecx, [r_ancho]
        jbe .c1
        mov ecx, [r_ancho]
.c1:
        test al, 0x80
        jnz .der
        mov [r_wx1], ecx
        jmp .x
.der:
        mov [r_wx0], ecx
        mov ecx, [r_ancho]
        mov [r_wx1], ecx
        jmp .x
.toda:
        mov ecx, [r_ancho]
        mov [r_wx1], ecx
.x:
        pop ecx
        pop eax
        ret

; pinta la ventana (sin desplazamiento) entre r_wx0 y r_wx1
ventana:
        pushad
        mov eax, [r_wx0]
        mov [r_x0], eax
        mov eax, [r_wx1]
        mov [r_x1], eax
        movzx edx, byte [v_reg+3]   ; su tabla: 64 casillas de ancho con 320
        mov ecx, 64                 ; puntos (32 con 256)
        and edx, 0x3C
        cmp dword [r_ancho], 320
        je .w
        mov cl, 32
        movzx edx, byte [v_reg+3]
        and edx, 0x3E
.w:
        shl edx, 10
        mov eax, [r_linea]
        shr eax, 3
        imul eax, ecx
        add eax, eax
        add edx, eax                ; EDX = su fila
        mov [r_nt], edx
        mov esi, [r_wx0]
        and esi, ~7
.c:
        cmp esi, [r_wx1]
        jge .x
        mov eax, esi
        shr eax, 2                  ; (dos octetos por casilla)
        add eax, [r_nt]
        and eax, 0xFFFF
        movzx edx, byte [vram+eax]
        shl edx, 8
        mov dl, [vram+eax+1]
        mov ebx, [r_linea]
        and ebx, 7
        test edx, 0x1000
        jz .nv
        xor ebx, 7
.nv:
        mov eax, edx
        and eax, 0x7FF
        shl eax, 5
        lea eax, [eax+ebx*4]
        mov eax, [vram+eax]
        test eax, eax
        jz .s
        mov ebx, edx
        shr ebx, 9
        and ebx, 0x30
        mov ebp, esi
        test edx, 0x8000
        jnz .alto
        lea edi, [lin+LIN_M+ebp]
        call casilla
        jmp .s
.alto:
        call apunta_alto
.s:
        add esi, 8
        jmp .c
.x:
        popad
        ret

; ---- los objetos de la línea, a spr (índice de color, 40h si es de
; prioridad alta, 80h si hay algo); el primero de la lista gana ----
objetos:
        pushad
        mov dword [r_smin], 1000
        mov dword [r_smax], -1
        movzx ebx, byte [v_reg+5]   ; la tabla (en 320 puntos, sin el bit 0)
        and ebx, 0x7F
        mov ecx, 80                 ; cuántos, por línea y puntos por línea
        mov edx, 20
        mov eax, 320
        cmp dword [r_ancho], 320
        je .t
        mov cl, 64
        mov dl, 16
        mov eax, 256
        jmp .t1
.t:
        and ebx, 0x7E
.t1:
        shl ebx, 9
        mov [r_sat], ebx
        mov [r_smaxn], edx
        mov [r_spuntos], eax
        mov [r_stotal], ecx
        mov dword [r_sn], 0
        mov dword [r_sx0vis], 0
        xor esi, esi                ; ESI = el objeto (empieza en el 0)
        xor ebp, ebp                ; EBP = cuántos se han mirado
.o:
        mov eax, esi
        shl eax, 3
        add eax, [r_sat]
        and eax, 0xFFFF
        mov edi, eax                ; EDI = su entrada
        movzx eax, byte [vram+edi]
        shl eax, 8
        mov al, [vram+edi+1]
        and eax, 0x3FF
        sub eax, 128                ; y
        mov ecx, [r_linea]
        sub ecx, eax                ; ECX = la línea dentro del objeto
        js .sig
        movzx edx, byte [vram+edi+2]
        mov eax, edx
        and eax, 3                  ; alto - 1, en casillas
        inc eax
        shl eax, 3
        cmp ecx, eax
        jae .sig
        ; está en la línea
        mov eax, [r_sn]
        cmp eax, [r_smaxn]
        jb .cabe
        or byte [v_st], 0x40        ; demasiados en la línea
        jmp .fin
.cabe:
        inc dword [r_sn]
        push esi
        push ebp
        call objeto                 ; EDI=entrada ECX=línea dentro
        pop ebp
        pop esi
        cmp dword [r_spuntos], 0
        jle .fin
.sig:
        movzx esi, byte [vram+edi+3] ; el siguiente de la lista
        and esi, 0x7F
        jz .fin
        inc ebp
        cmp esi, [r_stotal]
        jae .fin
        cmp ebp, [r_stotal]
        jb .o
.fin:
        popad
        ret

; un objeto: EDI=su entrada en la VRAM, ECX=la línea dentro de él
objeto:
        pushad
        movzx eax, byte [vram+edi+2]
        mov edx, eax
        shr edx, 2
        and edx, 3                  ; ancho - 1
        and eax, 3                  ; alto - 1
        mov [r_ow], edx
        mov [r_oh], eax
        movzx ebx, byte [vram+edi+4]
        shl ebx, 8
        mov bl, [vram+edi+5]        ; EBX = la entrada del dibujo
        movzx eax, byte [vram+edi+6]
        shl eax, 8
        mov al, [vram+edi+7]
        and eax, 0x1FF
        sub eax, 128                ; x
        mov [r_ox], eax
        ; la máscara: un objeto con x=0 (sin restar) tapa los demás de la
        ; línea si antes ha habido otro con x distinta
        cmp eax, -128
        jne .nm
        cmp dword [r_sx0vis], 0
        je .nm0
        mov dword [r_spuntos], 0
        jmp .x
.nm:
        mov dword [r_sx0vis], 1
.nm0:
        ; la fila de casillas y la línea dentro de ella
        test ebx, 0x1000
        jz .nv
        mov eax, [r_oh]             ; girado de arriba abajo
        inc eax
        shl eax, 3
        dec eax
        sub eax, ecx
        mov ecx, eax
.nv:
        mov eax, ecx
        shr eax, 3                  ; la fila
        and ecx, 7
        mov [r_ofy], ecx
        mov [r_ofila], eax
        mov eax, ebx
        shr eax, 9
        and eax, 0x30
        test ebx, 0x8000
        jz .pb
        or eax, 0x40
.pb:
        or eax, 0x80
        mov [r_opal], eax
        ; las casillas de izquierda a derecha
        xor esi, esi
.c:
        cmp esi, [r_ow]
        ja .x
        mov eax, [r_spuntos]        ; (el límite de puntos por línea)
        sub eax, 8
        mov [r_spuntos], eax
        jl .x
        mov eax, esi                ; la columna (girado: al revés)
        test ebx, 0x0800
        jz .nh
        mov eax, [r_ow]
        sub eax, esi
.nh:
        mov edx, [r_oh]
        inc edx
        imul eax, edx
        add eax, [r_ofila]
        add eax, ebx
        and eax, 0x7FF
        shl eax, 5
        mov edx, [r_ofy]
        lea eax, [eax+edx*4]
        mov eax, [vram+eax]
        test eax, eax
        jz .s
        mov edx, esi
        shl edx, 3
        add edx, [r_ox]             ; EDX = x en la pantalla
        cmp edx, -8
        jle .s
        cmp edx, [r_ancho]
        jge .s
        call objeto_casilla
.s:
        inc esi
        jmp .c
.x:
        popad
        ret

; ocho puntos de un objeto: EAX=los cuatro octetos, EDX=x, EBX=la entrada
; (bit 11: girada)
objeto_casilla:
        pushad
        cmp edx, [r_smin]
        jge .m1
        mov [r_smin], edx
.m1:
        lea ecx, [edx+7]
        cmp ecx, [r_smax]
        jle .m2
        mov [r_smax], ecx
.m2:
        lea edi, [spr+LIN_M+edx]
        mov esi, 8
        test ebx, 0x0800
        jz .c
        ; girada: los octetos y sus mitades al revés
        mov ecx, eax
        and eax, 0x0F0F0F0F
        shl eax, 4
        and ecx, 0xF0F0F0F0
        shr ecx, 4
        or eax, ecx
        rol eax, 16
        xchg al, ah
        mov ecx, eax
        shr ecx, 16
        xchg cl, ch
        shl ecx, 16
        mov cx, ax
        mov eax, ecx
.c:
        mov edx, eax                ; el punto: los 4 bits altos del octeto bajo
        shr edx, 4
        and edx, 15
        jz .n1
        cmp byte [edi], 0
        jne .ch1
        or edx, [r_opal]
        mov [edi], dl
        jmp .n1
.ch1:
        or byte [v_st], 0x20        ; (dos objetos en el mismo punto)
.n1:
        inc edi
        mov edx, eax
        and edx, 15
        jz .n2
        cmp byte [edi], 0
        jne .ch2
        or edx, [r_opal]
        mov [edi], dl
        jmp .n2
.ch2:
        or byte [v_st], 0x20
.n2:
        inc edi
        shr eax, 8
        sub esi, 2
        jnz .c
        popad
        ret

; los puntos de los objetos a lin: AH=80h los de prioridad baja, C0h los de
; alta (y estos, al acabar, se borran de spr)
objetos_pon:
        pushad
        mov esi, [r_smin]
        cmp esi, 0
        jge .a
        xor esi, esi
.a:
        mov ecx, [r_smax]
        cmp ecx, [r_ancho]
        jl .b
        mov ecx, [r_ancho]
        dec ecx
.b:
        mov bl, ah
.c:
        cmp esi, ecx
        jg .f
        mov al, [spr+LIN_M+esi]
        mov dl, al
        and dl, 0xC0
        cmp dl, bl
        jne .n
        and al, 0x3F
        mov [lin+LIN_M+esi], al
.n:
        inc esi
        jmp .c
.f:
        cmp bl, 0xC0
        jne .x
        mov edi, spr                ; (a cero para la siguiente)
        mov ecx, (320 + 2 * LIN_M) / 4
        xor eax, eax
        rep stosd
.x:
        popad
        ret

        align 4
tam_planos  dd 32, 64, 32, 128
; la paleta repetida en los cuatro octetos (r_pal4+0, +16, +32, +48)
r_pal4      times 4 db 0
            times 12 db 0
            times 4 db 0x10
            times 12 db 0
            times 4 db 0x20
            times 12 db 0
            times 4 db 0x30
            times 12 db 0
