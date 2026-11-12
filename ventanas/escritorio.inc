; =============================================================================
;  Hispa Ventanas - el escritorio y las ventanas de ejemplo de la etapa 2
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Aspecto propio: fondo de mosaico (un azulejo de rombos) en azul petróleo,
;  barra superior marrón con filete ocre, ventanas de color crema con la
;  barra de título terracota (arena si no es la activa), marco marrón y
;  sombra dura.
;    esc_iniciar    prepara el mosaico y los textos y crea las ventanas
;    esc_pintar     pinta el fondo y la barra superior (con el recorte puesto)
;    esc_en_salir   EAX=x EBX=y -> CF=1 si el punto está en el botón «Salir»
;    esc_inv_salir  marca el botón para repintarlo
; =============================================================================

SALIR_AN    equ 64
SALIR_AL    equ 18
SALIR_Y     equ 4

esc_iniciar:
        pushad
        ; el mosaico: del diseño en texto a colores
        mov esi, mosaico_dib
        mov edi, mosaico
        mov ecx, 256
.m:
        lodsb
        mov ah, C_PETROLEO
        cmp al, '+'
        jne .m2
        mov ah, C_PETROLEO_CL
.m2:
        mov [edi], ah
        inc edi
        loop .m

        ; ---- textos de la ventana «Sistema» ----
        mov edi, t_pantalla
        mov esi, s_pantalla
        call pon_cad
        mov eax, [vid_ancho]
        call pon_num
        mov esi, s_por
        call pon_cad
        mov eax, [vid_alto]
        call pon_num
        mov esi, s_c16
        mov al, [vid_bits]
        cmp al, 4
        je .col
        mov esi, s_c256
        cmp al, 8
        je .col
        mov esi, s_c32k
        cmp al, 15
        je .col
        mov esi, s_c64k
        cmp al, 16
        je .col
        mov esi, s_c16m
.col:
        call pon_cad
        mov byte [edi], 0

        mov edi, t_video
        cmp byte [vid_tipo], 1
        je .vesa
        mov esi, s_vga
        call pon_cad
        jmp .vfin
.vesa:
        mov esi, s_vesa
        call pon_cad
        movzx eax, byte [vbe_ver+1]
        call pon_num
        mov al, '.'
        stosb
        movzx eax, byte [vbe_ver]
        call pon_num
        mov esi, s_lineal
        call pon_cad
.vfin:
        mov byte [edi], 0

        mov edi, t_dpmi
        mov esi, s_dpmi
        call pon_cad
        push edi
        mov ax, 0x0400
        int 0x31
        pop edi
        push eax
        movzx eax, ah
        call pon_num
        mov al, '.'
        stosb
        pop eax
        movzx eax, al
        call pon_num
        mov esi, s_dpmi2
        call pon_cad
        push edi
        mov ax, 0x0500
        mov edi, mem_info
        int 0x31
        pop edi
        mov eax, [mem_info]
        shr eax, 10
        call pon_num
        mov esi, s_kb
        call pon_cad
        mov byte [edi], 0

        ; ---- las ventanas (la última queda delante) ----
        mov eax, 60
        mov ebx, [vid_alto]
        sub ebx, 160
        mov ecx, 400
        mov edx, 150
        mov esi, tit_sistema
        mov edi, pinta_sistema
        call ges_crear
        mov eax, [vid_ancho]
        sub eax, 380
        mov ebx, 110
        mov ecx, 300
        mov edx, 200
        mov esi, tit_paleta
        mov edi, pinta_paleta
        call ges_crear
        mov eax, 30
        mov ebx, 50
        mov ecx, 440
        mov edx, 160
        mov esi, tit_bienvenida
        mov edi, pinta_bienvenida
        call ges_crear
        popad
        ret

; copia la cadena ESI (acabada en 0) a EDI, que avanza
pon_cad:
        lodsb
        test al, al
        jz .x
        stosb
        jmp pon_cad
.x:
        ret

; escribe EAX en decimal en EDI, que avanza
pon_num:
        push ebx
        push ecx
        push edx
        mov ebx, 10
        xor ecx, ecx
.d:
        xor edx, edx
        div ebx
        push edx
        inc ecx
        test eax, eax
        jnz .d
.e:
        pop eax
        add al, '0'
        stosb
        loop .e
        pop edx
        pop ecx
        pop ebx
        ret

esc_pintar:
        pushad
        xor eax, eax                ; el fondo
        mov ebx, BARRA_H
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        mov esi, mosaico
        call gdi_mosaico
        cmp dword [rec_y0], BARRA_H
        jge .x
        COLOR C_MARRON              ; la barra superior
        xor ebx, ebx
        mov edx, BARRA_H - 2
        call gdi_rect
        COLOR C_OCRE
        mov ebx, BARRA_H - 2
        mov edx, 2
        call gdi_rect
        COLOR C_CREMA
        mov byte [gdi_negrita], 1
        mov eax, 12
        mov ebx, 5
        mov esi, tit_barra
        call gdi_texto
        mov byte [gdi_negrita], 0
        ; el botón «Salir» (se oscurece mientras está pulsado)
        COLOR C_TERRACOTA
        cmp byte [sal_armado], 0
        je .b1
        COLOR C_OCRE
.b1:
        mov eax, [vid_ancho]
        sub eax, SALIR_AN + 12
        mov ebx, SALIR_Y
        mov ecx, SALIR_AN
        mov edx, SALIR_AL
        call gdi_rect
        COLOR C_CREMA
        call gdi_marco
        cmp byte [sal_armado], 0
        je .b2
        COLOR C_TINTA
.b2:
        add eax, (SALIR_AN - 5*8) / 2
        inc ebx
        mov esi, tit_salir
        call gdi_texto
.x:
        popad
        ret

esc_en_salir:
        push eax
        sub eax, [vid_ancho]
        add eax, SALIR_AN + 12
        js .no
        cmp eax, SALIR_AN
        jge .no
        cmp ebx, SALIR_Y
        jl .no
        cmp ebx, SALIR_Y + SALIR_AL
        jge .no
        pop eax
        stc
        ret
.no:
        pop eax
        clc
        ret

esc_inv_salir:
        pushad
        mov eax, [vid_ancho]
        sub eax, SALIR_AN + 12
        mov ebx, SALIR_Y
        mov ecx, SALIR_AN
        mov edx, SALIR_AL
        call ges_invalidar
        popad
        ret

; ---- pintores de las ventanas de ejemplo: EAX=x EBX=y ECX=ancho EDX=alto ----
; escribe varias líneas: ESI -> (dx, dy, color, negrita, texto, 0)..., 0xFF
esc_lineas:
        pushad
        mov ebp, eax
        mov edi, ebx
.l:
        cmp byte [esi], 0xFF
        je .x
        movzx eax, byte [esi]
        add eax, ebp
        movzx ebx, byte [esi+1]
        add ebx, edi
        mov dl, [esi+2]
        mov [gdi_color], dl
        mov dl, [esi+3]
        mov [gdi_negrita], dl
        add esi, 4
        call gdi_texto
.s:
        lodsb
        test al, al
        jnz .s
        jmp .l
.x:
        popad
        ret

pinta_bienvenida:
        mov esi, lin_bienvenida
        jmp esc_lineas

pinta_sistema:
        push esi
        mov esi, lin_sistema
        call esc_lineas
        pop esi
        push eax
        push ebx
        push esi
        COLOR C_TINTA
        add eax, 14
        add ebx, 10
        mov esi, t_pantalla
        call gdi_texto
        add ebx, 20
        mov esi, t_video
        call gdi_texto
        add ebx, 20
        mov esi, s_raton_si
        cmp byte [hay_rat], 0
        jne .r
        mov esi, s_raton_no
.r:
        call gdi_texto
        add ebx, 20
        mov esi, t_dpmi
        call gdi_texto
        pop esi
        pop ebx
        pop eax
        ret

pinta_paleta:
        pushad
        mov ebp, eax                ; origen
        mov edi, ebx
        ; los 16 colores de Hispa Ventanas
        xor esi, esi
.c:
        mov eax, esi
        and eax, 7
        imul eax, 34
        lea eax, [eax+ebp+12]
        mov ebx, esi
        shr ebx, 3
        imul ebx, 28
        lea ebx, [ebx+edi+10]
        mov ecx, 30
        mov edx, 24
        mov [gdi_color], esi
        call gdi_rect
        COLOR C_TINTA
        call gdi_marco
        inc esi
        cmp esi, 16
        jb .c
        ; un abanico de líneas
        xor esi, esi
.l:
        lea eax, [esi+2]
        mov [gdi_color], al
        lea eax, [ebp+12]
        lea ebx, [edi+162]
        mov ecx, esi
        imul ecx, 13
        lea ecx, [ecx+ebp+12]
        lea edx, [edi+74]
        call gdi_linea
        inc esi
        cmp esi, 11
        jb .l
        ; marcos uno dentro de otro
        xor esi, esi
.m:
        COLOR C_TERRACOTA
        test esi, 1
        jz .m2
        COLOR C_ANIL
.m2:
        mov eax, esi
        shl eax, 3
        mov ecx, 124
        sub ecx, eax
        sub ecx, eax
        lea eax, [eax+ebp+160]
        mov ebx, esi
        imul ebx, 6
        mov edx, 88
        sub edx, ebx
        sub edx, ebx
        lea ebx, [ebx+edi+74]
        call gdi_marco
        inc esi
        cmp esi, 5
        jb .m
        COLOR C_OCRE
        lea eax, [ebp+200]
        lea ebx, [edi+104]
        mov ecx, 44
        mov edx, 28
        call gdi_rect
        popad
        ret
