; =============================================================================
;  Hispa-Drive - un cuadro de la consola: las líneas, las interrupciones y
;  el reparto del tiempo entre el 68000, el Z80, la imagen y el sonido
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Una línea son 3.420 tics del reloj de la consola (53,69 MHz en NTSC):
;  488,57 ciclos del 68000 (entre 7) y 228 del Z80 (entre 15); 262 líneas por
;  cuadro (313 en PAL), de las que se ven 224 (o 240 en PAL). En cada línea:
;  el contador de la interrupción de línea (registro 10), la de cuadro al
;  empezar la 224, el 68000 un rato (el borde), la imagen de la línea, el
;  resto del 68000, el Z80 (si tiene el bus y no está en reinicio) y el
;  sonido de la línea.
;
;    md_cuadro       un cuadro entero (con [dibujar]=1 pinta la imagen)
; =============================================================================

C68_BORDE   equ 104                 ; lo que corre el 68000 antes de pintar

md_cuadro:
        pushad
        mov eax, 262                ; las líneas y las que se ven
        mov ecx, 224
        cmp byte [region], 0xC0
        jne .n
        mov eax, 313
        test byte [v_reg+1], 0x08
        jz .n
        mov ecx, 240
.n:
        mov [v_lineas], eax
        mov [v_alto], ecx
        xor ebx, ebx                ; EBX = la línea
.l:
        mov [v_linea], ebx
        ; la interrupción de línea: el contador baja en las líneas que se
        ; ven (y la de después); fuera, se carga con el registro 10
        cmp ebx, [v_alto]
        ja .h1
        dec byte [v_hcnt]
        jns .h2
        mov al, [v_reg+10]
        mov [v_hcnt], al
        mov byte [v_hint], 1
        call vdp_irq
        jmp .h2
.h1:
        mov al, [v_reg+10]
        mov [v_hcnt], al
.h2:
        ; la de cuadro (y la del Z80, una línea)
        cmp ebx, [v_alto]
        jne .v1
        or byte [v_st], 0x80
        mov byte [v_vint], 1
        call vdp_irq
        mov byte [irq_linea], 1
        jmp .v2
.v1:
        mov byte [irq_linea], 0
.v2:
        ; los ciclos del 68000 de esta línea: 488 o 489 (4 de cada 7)
        mov ecx, 488
        add byte [v_frac], 4
        cmp byte [v_frac], 7
        jb .c1
        sub byte [v_frac], 7
        inc ecx
.c1:
        mov [v_c68], ecx
        mov dword [v_cic0], 0
        mov eax, C68_BORDE
        call m68k_correr
        cmp ebx, [v_alto]
        jae .np
        cmp byte [dibujar], 0
        je .np
        mov eax, ebx
        call vdp_linea
.np:
        mov dword [v_cic0], C68_BORDE
        mov eax, [v_c68]
        sub eax, C68_BORDE
        call m68k_correr
        mov dword [v_cic0], 0
        mov eax, [v_c68]
        add [md_ciclos], eax
        ; el Z80
        cmp byte [z80_reset], 0
        jne .z
        cmp byte [z80_bus], 0
        jne .z
        mov edi, 228
        call z80_correr
.z:
        ; el sonido
        call md_sonido_linea
        inc ebx
        cmp ebx, [v_lineas]
        jb .l
        inc dword [cuadros]
        popad
        ret

; las muestras de una línea: las del SN76489 (psg_linea) y, sumadas encima,
; las del YM2612
md_sonido_linea:
        push eax
        push ecx
        mov ecx, [psg_n]
        call psg_linea
        mov eax, [psg_n]
        sub eax, ecx
        call ym_linea               ; ECX = la primera, EAX = cuántas
        pop ecx
        pop eax
        ret
