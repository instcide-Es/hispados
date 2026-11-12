; =============================================================================
;  Hispa Ventanas - ratón y cursor
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  El ratón llega por INT 33h (HSPRATON u otro driver) y por interrupciones:
;  el driver llama a rm_raton (ventanas.asm, en modo real) en cada movimiento
;  y en cada cambio de los botones izquierdo y derecho (función 0Ch), y esa rutina lo apunta
;  en una cola que el sistema vacía en cada vuelta (eventos). La posición la
;  lleva Hispa Ventanas, en puntos de pantalla, con los contadores de
;  movimiento del driver.
;  El cursor se dibuja por software en la superficie, guardando lo que tapa:
;    rat_iniciar    prepara el dibujo del cursor, lo centra y pide los avisos
;    rat_quitar     deja de recibir avisos del driver
;    rat_mover      EAX=x EBX=y: nueva posición (acotada a la pantalla)
;    cur_ocultar    repone lo que había debajo
;    cur_mostrar    guarda lo que hay debajo y dibuja el cursor en rat_x, rat_y
;    cur_x, cur_y, cur_puesto   dónde está dibujado, si lo está
; =============================================================================

CUR_ANCHO   equ 12
CUR_ALTO    equ 18

rat_iniciar:
        pushad
        mov esi, cursor_dib         ; el diseño en texto, a colores
        mov edi, cur_img
        mov ecx, CUR_ANCHO * CUR_ALTO
.c:
        lodsb
        mov ah, CLAVE
        cmp al, '.'
        je .p
        mov ah, C_TINTA
        cmp al, 'X'
        je .p
        mov ah, C_OCRE
.p:
        mov [edi], ah
        inc edi
        loop .c
        mov eax, [vid_ancho]
        shr eax, 1
        mov [rat_x], eax
        mov eax, [vid_alto]
        shr eax, 1
        mov [rat_y], eax
        cmp byte [hay_rat], 0
        je .x
        mov eax, 0x000B             ; contadores de movimiento, a cero
        int 0x33
        mov dword [rmcs+0x1C], 0x000C
        mov dword [rmcs+0x18], 0x1F ; movimiento y los botones izquierdo y derecho
        mov dword [rmcs+0x14], rm_raton
        mov bl, 0x33
        call rm_int
.x:
        popad
        ret

rat_quitar:
        cmp byte [hay_rat], 0
        je .x
        pushad
        mov dword [rmcs+0x1C], 0x000C
        mov dword [rmcs+0x18], 0
        mov dword [rmcs+0x14], 0
        mov bl, 0x33
        call rm_int
        popad
.x:
        ret

rat_mover:
        test eax, eax
        jns .a
        xor eax, eax
.a:
        cmp eax, [vid_ancho]
        jl .b
        mov eax, [vid_ancho]
        dec eax
.b:
        test ebx, ebx
        jns .c
        xor ebx, ebx
.c:
        cmp ebx, [vid_alto]
        jl .d
        mov ebx, [vid_alto]
        dec ebx
.d:
        mov [rat_x], eax
        mov [rat_y], ebx
        ret

cur_ocultar:
        cmp byte [cur_puesto], 0
        je .x
        pushad
        call gdi_recorte_total
        mov eax, [cur_x]
        mov ebx, [cur_y]
        mov ecx, CUR_ANCHO
        mov edx, CUR_ALTO
        mov esi, cur_fondo
        call gdi_copiar
        mov byte [cur_puesto], 0
        popad
.x:
        ret

cur_mostrar:
        cmp byte [hay_rat], 0
        je .x
        pushad
        call gdi_recorte_total
        mov eax, [rat_x]
        mov ebx, [rat_y]
        mov [cur_x], eax
        mov [cur_y], ebx
        mov ecx, CUR_ANCHO
        mov edx, CUR_ALTO
        mov edi, cur_fondo
        call gdi_leer
        mov esi, cur_img
        call gdi_copiar_clave
        mov byte [cur_puesto], 1
        popad
.x:
        ret
