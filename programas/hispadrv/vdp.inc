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
LIN_SPR     equ lin - spr           ; (de un punto de spr al mismo de lin)

; cuatro puntos: %1 tabla de pares de colores (la de la paleta: EBX =
; paleta x 512), %2 de máscaras, %3 %4 los dos octetos (en el orden en que se
; pintan), %5 desplazamiento en EDI. Gasta EDX, EBP
%macro PINTA4 5
        movzx edx, %4
        movzx ebp, %3
        mov dx, [%1+ebx+edx*2]
        shl edx, 16
        mov dx, [%1+ebx+ebp*2]
        push eax
        movzx eax, %4
        mov ax, [%2+eax*2]
        shl eax, 16
        mov ax, [%2+ebp*2]
        ; EDX = cuatro puntos, EAX = su máscara
        and edx, eax
        not eax
        and eax, [edi+%5]
        or eax, edx
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
        ; las máscaras de cada octeto de un dibujo: FFh en cada uno de sus
        ; dos puntos que no es transparente (el de los 4 bits altos primero;
        ; tab_mkr, al revés)
        xor ebx, ebx
.t:
        mov eax, ebx
        shr eax, 4
        mov edx, ebx
        and edx, 15
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
        mov al, [v_col]             ; (las tablas del fondo, enteras)
        not al
        mov [r_fondo_act], al
        not al
        call fondo_pon
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
        ; las sombras y los brillos: cada uno de los 512 colores, con cada
        ; componente (0-7) a la mitad (L/2) o a la mitad más la mitad del
        ; máximo ((L+7)/2); a cada color del equipo le toca el de uno de los
        ; de la consola que lo dan (todos se parecen)
        xor ebx, ebx
.s:
        xor ecx, ecx                ; ECX = el de sombra, EDX = el de brillo
        xor edx, edx
        xor esi, esi                ; ESI = el desplazamiento de la componente
.s1:
        mov eax, ebx
        push ecx
        mov ecx, esi
        shr eax, cl
        and eax, 7
        mov ebp, eax
        shr eax, 1
        shl eax, cl
        add ebp, 7
        shr ebp, 1
        shl ebp, cl
        pop ecx
        or ecx, eax
        or edx, ebp
        add esi, 3
        cmp esi, 9
        jb .s1
        movzx eax, byte [md_col9+ebx]
        mov cl, [md_col9+ecx]
        mov [t_sombra+eax], cl
        mov dl, [md_col9+edx]
        mov [t_brillo+eax], dl
        inc ebx
        cmp ebx, 512
        jb .s
        popad
        ret
; EBX=entrada AX=su valor (----BBB-GGG-RRR-): v_col y los pares de puntos de
; su paleta que la llevan (pal_px: el de los 4 bits altos primero; pal_pxr:
; al revés). Conserva todo menos EBP
vdp_col_pon:
        push eax
        push ecx
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
        mov ecx, ebx                ; la paleta: ECX = su tabla (x 512)
        and ecx, 0x30
        shl ecx, 5
        mov edx, ebx
        and edx, 15                 ; EDX = el número en la paleta
        push ebx
        ; los octetos con el color arriba: EDX*16 + k
        mov ebx, edx
        shl ebx, 4
        lea ebp, [ecx+ebx*2]
        mov ebx, 16
.p:
        mov [pal_px+ebp], al
        mov [pal_pxr+ebp+1], al
        test edx, edx
        jz .p1
        mov [pal_pxb+ebp], al
        mov [pal_pxbr+ebp+1], al
.p1:
        add ebp, 2
        dec ebx
        jnz .p
        ; y abajo: k*16 + EDX
        lea ebp, [ecx+edx*2]
        mov ebx, 16
.q:
        mov [pal_px+ebp+1], al
        mov [pal_pxr+ebp], al
        test edx, edx
        jz .q1
        mov [pal_pxb+ebp+1], al
        mov [pal_pxbr+ebp], al
.q1:
        add ebp, 32
        dec ebx
        jnz .q
        pop ebx
        pop edx
        pop ecx
        pop eax
        ret

; AL=el color del fondo (del equipo): en los puntos transparentes de
; pal_pxb y pal_pxbr (si ha cambiado)
fondo_pon:
        cmp al, [r_fondo_act]
        jne .si
        ret
.si:
        pushad
        mov [r_fondo_act], al
        xor ecx, ecx                ; ECX = paleta x 512
.p:
        xor ebx, ebx
.o:
        lea ebp, [ecx+ebx*2]
        test bl, 0xF0
        jnz .o1
        mov [pal_pxb+ebp], al
        mov [pal_pxbr+ebp+1], al
.o1:
        test bl, 0x0F
        jnz .o2
        mov [pal_pxb+ebp+1], al
        mov [pal_pxbr+ebp], al
.o2:
        inc ebx
        cmp ebx, 256
        jb .o
        add ecx, 512
        cmp ecx, 2048
        jb .p
        popad
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
        mov byte [v_sat_sucia], 1   ; (5 y 12: otra tabla de objetos)
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
        mov byte [v_sat_sucia], 1
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
        mov byte [v_sat_sucia], 1
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
        sub ebp, [r_sat]            ; ¿en la tabla de los objetos?
        cmp ebp, 0x280
        jae .sig
        mov byte [v_sat_sucia], 1
        jmp .sig
.cram:
        shr ebp, 1
        and ebp, 63
        and eax, 0x0EEE
        cmp [cram+ebp*2], ax        ; (igual que estaba: nada que hacer)
        je .sig
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
; puntos, en medio y con el borde del color del fondo). lin va ya en los
; colores del equipo
vdp_linea:
        pushad
        mov [r_linea], eax
        mov eax, [v_ancho]
        mov [r_ancho], eax
        ; el fondo (registro 7)
        movzx eax, byte [v_reg+7]
        and eax, 0x3F
        mov al, [v_col+eax]
        call fondo_pon
        mov ah, al
        mov ecx, eax
        shl eax, 16
        mov ax, cx
        mov [r_fondo4], eax
        mov edi, lin
        mov ecx, (320 + 2 * LIN_M) / 4
        rep stosd
        mov byte [r_sh], 0
        test byte [v_reg+1], 0x40   ; la imagen apagada: solo el fondo
        jz near .pasa
        mov dword [r_nalto], 0
        test byte [v_reg+12], 8     ; ¿sombras y brillos?
        jz .nsh
        mov byte [r_sh], 1
        mov edi, luz
        mov ecx, (320 + 2 * LIN_M) / 4
        xor eax, eax
        rep stosd
.nsh:
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
        ; ---- los objetos (los de prioridad baja van ya a lin) ----
        call objetos
        cmp byte [r_sh], 0
        je .ns1
        mov esi, lin
        mov edi, lin_b
        mov ecx, (320 + 2 * LIN_M) / 4
        rep movsd
.ns1:
        ; ---- lo de prioridad alta: B, A (y la ventana) ----
        call altos
        mov ah, 0xC0
        call objetos_pon
        cmp byte [r_sh], 0
        je .pasa
        call sombras
.pasa:
        ; a la fila de la imagen
        mov edi, [r_linea]
        imul edi, 320
        add edi, imagen
        mov esi, lin + LIN_M
        mov ecx, [r_ancho]
        shr ecx, 2
        cmp ecx, 80
        je .c
        mov eax, [r_fondo4]         ; (256: 32 de borde a cada lado)
        push ecx
        mov ecx, 8
        rep stosd
        pop ecx
        rep movsd
        mov ecx, 8
        rep stosd
        jmp .x
.c:
        rep movsd
.x:
        popad
        ret

; ---- un plano (r_plano: 0 A, 1 B; r_nt su tabla) entre r_x0 y r_x1: los
; dibujos de prioridad baja se pintan ya; los de alta se apuntan (r_altos)
plano:
        pushad
        call recorte
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
        mov ecx, eax
        and ecx, 7
        neg ecx
        mov esi, ecx                ; ESI = x en la pantalla de la primera casilla
        mov ecx, eax
        shr ecx, 3                  ; ECX = su columna en el plano
        mov eax, [r_cw]
        dec eax
        mov [r_cmask], eax
        ; las casillas que quedan a la izquierda de r_x0, saltadas
        mov eax, [r_x0]
        sub eax, esi
        jle .sx
        shr eax, 3
        add ecx, eax
        shl eax, 3
        add esi, eax
.sx:
        and ecx, [r_cmask]
        mov dword [r_colk], -1
        test byte [v_reg+11], 4     ; ¿desplazamiento vertical por columnas?
        setnz byte [r_porcol]
        jnz .t
        xor eax, eax                ; (si no, la fila de toda la línea)
        call fila_plano
.t:
        cmp byte [r_porcol], 0      ; por columnas de 16 puntos: la fila de la
        je .t0                      ; de esta casilla (si ha cambiado)
        mov eax, esi
        sar eax, 4
        jns .c0
        xor eax, eax
.c0:
        cmp eax, 19
        jbe .c1
        mov eax, 19
.c1:
        cmp eax, [r_colk]
        je .t0
        call fila_plano
.t0:
        mov edx, [r_filap]
        movzx edx, word [edx+ecx*2] ; la entrada (en el orden del 68000)
        xchg dl, dh
        test dh, 0x80               ; (sombras y brillos: dónde hay prioridad
        jz .nl                      ; alta, aunque sea transparente)
        cmp byte [r_sh], 0
        je .nl
        call marca_luz
.nl:
        mov eax, edx
        and eax, 0x7FF
        shl eax, 5
        test dh, 0x10               ; girada de arriba abajo
        jnz .nv
        add eax, [r_fy4]
        jmp .nv1
.nv:
        add eax, [r_fy4r]
.nv1:
        mov eax, [vram+eax]         ; los cuatro octetos de la fila
        test eax, eax
        jz .s                       ; (transparente del todo)
        mov ebx, edx
        shr ebx, 4
        and ebx, 0x600              ; la paleta (x 512)
        mov ebp, esi
        test dh, 0x80
        jnz .alto
        lea edi, [lin+LIN_M+esi]
        cmp dword [r_plano], 1      ; (el plano B, de prioridad baja: debajo
        je .b                       ; solo está el fondo)
        call casilla
        jmp .s
.b:
        call casilla_b
        jmp .s
.alto:                              ; prioridad alta: para luego
        call apunta_alto
.s:
        inc ecx
        and ecx, [r_cmask]
        add esi, 8
        cmp esi, [r_x1]
        jl .t
        popad
        ret

; (plano) EAX=columna de 16 puntos (0-19; con desplazamiento de toda la
; pantalla, 0): r_filap (la fila de la tabla), r_fy4 y r_fy4r (la línea
; dentro de la casilla, x 4, derecha y girada)
fila_plano:
        push eax
        push edx
        mov [r_colk], eax
        add eax, eax
        add eax, [r_plano]
        movzx eax, word [vsram+eax*2]
        add eax, [r_linea]
        and eax, [r_hmask]
        mov edx, eax
        and edx, 7
        shl edx, 2
        mov [r_fy4], edx
        xor edx, 28
        mov [r_fy4r], edx
        shr eax, 3
        imul eax, [r_cw]
        add eax, eax
        add eax, [r_nt]
        and eax, 0xFFFF
        add eax, vram
        mov [r_filap], eax
        pop edx
        pop eax
        ret

; el recorte de casilla: r_x0 y r_x1, salvo en los bordes de la pantalla
; (allí no hace falta: lin tiene margen)
recorte:
        push eax
        mov eax, [r_x0]
        test eax, eax
        jnz .a
        mov eax, -LIN_M
.a:
        mov [r_cx0], eax
        mov eax, [r_x1]
        cmp eax, [r_ancho]
        jb .b
        mov eax, 320 + LIN_M
.b:
        mov [r_cx1], eax
        pop eax
        ret

; (plano B, prioridad baja) una fila de una casilla, sin mirar lo de debajo:
; lo transparente es el color del fondo (pal_pxb). Como casilla, sin recorte
; (el plano B va de borde a borde)
casilla_b:
        cmp ebp, [r_cx0]
        jl casilla.recorta
        test edx, 0x0800
        jnz .girada
        movzx edx, ah
        movzx ebp, al
        mov dx, [pal_pxb+ebx+edx*2]
        shl edx, 16
        mov dx, [pal_pxb+ebx+ebp*2]
        mov [edi], edx
        rol eax, 16
        movzx edx, ah
        movzx ebp, al
        mov dx, [pal_pxb+ebx+edx*2]
        shl edx, 16
        mov dx, [pal_pxb+ebx+ebp*2]
        mov [edi+4], edx
        rol eax, 16
        ret
.girada:
        movzx edx, ah
        movzx ebp, al
        mov dx, [pal_pxbr+ebx+ebp*2]
        shl edx, 16
        mov bp, [pal_pxbr+ebx+edx*2]  ; (EBP: el índice ya no hace falta)
        mov dx, bp
        rol edx, 16
        mov [edi+4], edx
        rol eax, 16
        movzx edx, ah
        movzx ebp, al
        mov dx, [pal_pxbr+ebx+ebp*2]
        shl edx, 16
        mov bp, [pal_pxbr+ebx+edx*2]
        mov dx, bp
        rol edx, 16
        mov [edi], edx
        rol eax, 16
        ret

; pinta una fila de una casilla: EAX=los cuatro octetos, EBX=paleta (x 512),
; EDX=la entrada (bit 11: girada), EDI=en lin; solo entre r_cx0 y r_cx1
; (EBP=x). Conserva EAX, EBX, ECX, ESI, EDI
casilla:
        cmp ebp, [r_cx0]
        jl .recorta
        push ecx
        lea ecx, [ebp+8]
        cmp ecx, [r_cx1]
        pop ecx
        jg .recorta
        test edx, 0x0800
        jnz .girada
        PINTA4 pal_px, tab_mk, al, ah, 0
        rol eax, 16
        PINTA4 pal_px, tab_mk, al, ah, 4
        rol eax, 16
        ret
.girada:
        rol eax, 16
        PINTA4 pal_pxr, tab_mkr, ah, al, 0
        rol eax, 16
        PINTA4 pal_pxr, tab_mkr, ah, al, 4
        ret
.recorta:                           ; (de punto en punto, mirando los bordes)
        push ecx
        push esi
        push edx
        push edi
        mov esi, edx                ; ESI = la entrada (bit 11: girada)
        xor ecx, ecx                ; ECX = el punto (0-7)
.r3:
        cmp ebp, [r_cx0]
        jl .r5
        cmp ebp, [r_cx1]
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
        mov edx, ebx                ; (la paleta x 16)
        shr edx, 5
        add eax, edx
        mov al, [v_col+eax]
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
        pop edi
        pop edx
        pop esi
        pop ecx
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
        mov ecx, [r_cx0]
        mov [r_altos+edi+16], ecx
        mov ecx, [r_cx1]
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
        mov [r_cx0], eax
        mov eax, [r_altos+edi+20]
        mov [r_cx1], eax
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
        call recorte
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
        test edx, 0x8000
        jz .nl
        cmp byte [r_sh], 0
        je .nl
        call marca_luz
.nl:
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
        shr ebx, 4
        and ebx, 0x600
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

; ---- las listas de objetos de cada línea (una vez por cuadro, o si se ha
; escrito en su tabla): siguiendo los enlaces desde el 0, cada objeto se
; apunta en las líneas que toca (hasta 20 por línea con 320 puntos, 16 con
; 256; si hay más, la línea lo apunta y salen los primeros) ----
objetos_lista:
        pushad
        mov byte [v_sat_sucia], 0
        mov edi, r_lnum
        mov ecx, 256 / 4
        xor eax, eax
        rep stosd
        movzx ebx, byte [v_reg+5]   ; la tabla (en 320 puntos, sin el bit 0)
        and ebx, 0x7F
        mov ecx, 80
        mov edx, 20
        cmp dword [v_ancho], 320
        je .t
        mov cl, 64
        mov dl, 16
        jmp .t1
.t:
        and ebx, 0x7E
.t1:
        shl ebx, 9
        mov [r_sat], ebx
        mov [r_smaxn], edx
        mov [r_stotal], ecx
        xor esi, esi                ; ESI = el objeto
        xor ebp, ebp                ; EBP = cuántos se han mirado
.o:
        mov edi, esi
        shl edi, 3
        add edi, [r_sat]
        and edi, 0xFFFF             ; EDI = su entrada
        movzx eax, byte [vram+edi]
        shl eax, 8
        mov al, [vram+edi+1]
        and eax, 0x3FF
        sub eax, 128                ; EAX = y (primera línea)
        movzx ecx, byte [vram+edi+2]
        and ecx, 3
        inc ecx
        shl ecx, 3
        add ecx, eax                ; ECX = la de después de la última
        test eax, eax
        jns .a
        xor eax, eax
.a:
        cmp ecx, 240
        jbe .b
        mov ecx, 240
.b:
        cmp eax, ecx
        jae .sig
.l:
        movzx edx, byte [r_lnum+eax]
        cmp edx, [r_smaxn]
        jae .lleno
        imul ebx, eax, 20
        add ebx, edx
        push eax
        mov eax, esi
        mov [r_lobj+ebx], al
        pop eax
        inc byte [r_lnum+eax]
        jmp .l1
.lleno:
        mov byte [r_lmas+eax], 1
.l1:
        inc eax
        cmp eax, ecx
        jb .l
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

; ---- los objetos de la línea, a spr (índice de color, 40h si es de
; prioridad alta, 80h si hay algo); el primero de la lista gana ----
objetos:
        pushad
        mov dword [r_smin], 1000
        mov dword [r_smax], -1
        mov byte [r_sclases], 0
        cmp dword [r_linea], 0      ; las listas: al empezar el cuadro (o si
        je .rehaz                   ; se ha escrito en la tabla)
        cmp byte [v_sat_sucia], 0
        je .hecha
.rehaz:
        call objetos_lista
.hecha:
        mov eax, 320
        cmp dword [r_ancho], 320
        je .p
        mov eax, 256
.p:
        mov [r_spuntos], eax
        mov dword [r_sx0vis], 0
        mov ebx, [r_linea]
        cmp byte [r_lmas+ebx], 0    ; demasiados en esta línea
        je .nm
        or byte [v_st], 0x40
        mov byte [r_lmas+ebx], 0
.nm:
        movzx ebp, byte [r_lnum+ebx]
        test ebp, ebp
        jz .fin
        imul ebx, 20
        add ebx, r_lobj
.o:
        movzx esi, byte [ebx]
        inc ebx
        mov edi, esi
        shl edi, 3
        add edi, [r_sat]
        and edi, 0xFFFF             ; EDI = su entrada
        movzx eax, byte [vram+edi]
        shl eax, 8
        mov al, [vram+edi+1]
        and eax, 0x3FF
        sub eax, 128
        mov ecx, [r_linea]
        sub ecx, eax                ; ECX = la línea dentro del objeto
        push ebx
        push ebp
        call objeto
        pop ebp
        pop ebx
        cmp dword [r_spuntos], 0
        jle .fin
        dec ebp
        jnz .o
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
        mov dl, al
        and dl, 0x40
        add dl, 0x40                ; (40h baja, 80h alta)
        or [r_sclases], dl
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
        test dl, 0x40               ; (de prioridad baja: ya a lin; los altos
        jnz .n1                     ; van luego, encima de los planos altos)
        and edx, 0x3F
        cmp dl, 0x3E                ; (con sombras y brillos, 3Eh y 3Fh no se
        jae .n1op                  ; pintan: cambian lo de debajo)
.n1pi:
        mov dl, [v_col+edx]
        mov [edi+LIN_SPR], dl
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
        test dl, 0x40
        jnz .n2
        and edx, 0x3F
        cmp dl, 0x3E                ; (con sombras y brillos, 3Eh y 3Fh no se
        jae .n2op                  ; pintan: cambian lo de debajo)
.n2pi:
        mov dl, [v_col+edx]
        mov [edi+LIN_SPR], dl
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
.n1op:
        cmp byte [r_sh], 0
        je .n1pi
        jmp .n1
.n2op:
        cmp byte [r_sh], 0
        je .n2pi
        jmp .n2

; los puntos de los objetos a lin: AH=80h los de prioridad baja, C0h los de
; alta (y estos, al acabar, se borran de spr)
objetos_pon:
        pushad
        mov esi, [r_smin]
        mov ecx, [r_smax]
        cmp esi, ecx
        jg .x                       ; (ninguno en esta línea)
        mov bl, ah
        mov al, 0x40                ; ¿hay de esta clase?
        cmp bl, 0x80
        je .k
        mov al, 0x80
.k:
        test [r_sclases], al
        jz .f
        cmp esi, 0
        jge .a
        xor esi, esi
.a:
        cmp ecx, [r_ancho]
        jl .b
        mov ecx, [r_ancho]
        dec ecx
.b:
        and esi, ~3
        xor edx, edx
.c:
        cmp esi, ecx
        jg .f
        cmp dword [spr+LIN_M+esi], 0 ; (cuatro vacíos: siguiente)
        je .n4
        mov edi, 4
.c1:
        mov al, [spr+LIN_M+esi]
        mov dl, al
        and al, 0xC0
        cmp al, bl
        jne .n
        and edx, 0x3F
        cmp dl, 0x3E
        jae .op
.pi:
        mov dl, [v_col+edx]
        mov [lin+LIN_M+esi], dl
.n:
        inc esi
        dec edi
        jnz .c1
        jmp .c
.n4:
        add esi, 4
        jmp .c
.op:
        cmp byte [r_sh], 0
        je .pi
        jmp .n
.f:
        cmp bl, 0xC0
        jne .x
        cmp byte [r_sh], 0          ; (con sombras: los borra «sombras»)
        jne .x
        call spr_borra
.x:
        popad
        ret

; spr a cero (lo que han ocupado los objetos de la línea), para la siguiente
spr_borra:
        pushad
        mov edi, [r_smin]
        mov ecx, [r_smax]
        sub ecx, edi
        add ecx, 8
        lea edi, [spr+LIN_M+edi-4]
        xor eax, eax
        rep stosb
        popad
        ret

; ---- las sombras y los brillos (registro 12, bit 3) ----
; (plano y ventana) ESI=x de una casilla de prioridad alta: sus ocho puntos,
; dentro del recorte, a 1 en luz (lo que no es de prioridad alta en ningún
; plano sale a la mitad, también el fondo)
marca_luz:
        push eax
        push ecx
        push edi
        mov eax, esi
        cmp eax, [r_cx0]
        jge .a
        mov eax, [r_cx0]
.a:
        lea ecx, [esi+8]
        cmp ecx, [r_cx1]
        jle .b
        mov ecx, [r_cx1]
.b:
        sub ecx, eax
        jle .x
        lea edi, [luz+LIN_M+eax]
        mov al, 1
        rep stosb
.x:
        pop edi
        pop ecx
        pop eax
        ret

; la línea ya pintada (sin los puntos 3Eh y 3Fh de los objetos), a su luz:
; en cada punto, normal si algún plano es de prioridad alta y a la mitad si
; no; un objeto que se ve (de prioridad alta, o baja y sin un plano alto
; encima) cambia eso: el color 3Eh da un paso más de luz, el 3Fh deja a la
; mitad, los de prioridad alta y los colores 14 de cada paleta salen
; normales; los demás de prioridad baja, como lo de debajo. Luego spr a
; cero (spr_borra)
sombras:
        pushad
        xor esi, esi
.c:
        cmp esi, [r_ancho]
        jae .f
        movzx ebx, byte [luz+LIN_M+esi] ; EBX = 0 sombra, 1 normal, 2 brillo
        mov al, [spr+LIN_M+esi]
        test al, al
        jz .p
        mov dl, [lin+LIN_M+esi]
        test al, 0x40
        jnz .vis
        cmp dl, [lin_b+LIN_M+esi]   ; (de prioridad baja: si un plano alto lo
        jne .p                      ; ha tapado, no cuenta)
.vis:
        mov ah, al
        and ah, 0x3F
        cmp ah, 0x3F
        jne .k1
        xor ebx, ebx                ; sombra
        jmp .p
.k1:
        cmp ah, 0x3E
        jne .k2
        inc ebx                     ; un paso más de luz
        jmp .p
.k2:
        test al, 0x40
        jnz .nor
        and ah, 15
        cmp ah, 14
        jne .p
.nor:
        mov ebx, 1
.p:
        movzx edx, byte [lin+LIN_M+esi]
        cmp ebx, 1
        je .s
        jb .som
        mov dl, [t_brillo+edx]
        jmp .w
.som:
        mov dl, [t_sombra+edx]
.w:
        mov [lin+LIN_M+esi], dl
.s:
        inc esi
        jmp .c
.f:
        cmp dword [r_smin], 1000    ; (sin objetos en la línea)
        je .x
        call spr_borra
.x:
        popad
        ret

        align 4
tam_planos  dd 32, 64, 32, 128
