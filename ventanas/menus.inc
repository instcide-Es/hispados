; =============================================================================
;  Hispa Ventanas - menús desplegables
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  La barra de menús de una ventana es una tabla (V_MENU apunta a ella):
;      db "&Archivo", 0             título («&» marca la letra de Alt+letra)
;        dw 101                     número de la orden (WM_COMMAND, ECX)
;        db "&Nuevo", 0             su texto
;        dw 0                       un número 0 es una raya de separación
;        db 0
;        dw 0xFFFF                  fin del desplegable
;      db "A&yuda", 0 ...
;      db 0                         fin de la barra
;  El desplegable es una ventana emergente del sistema (tarea 0). Mientras
;  está abierto, el ratón y el teclado son del menú: flechas, Intro y Esc.
;  Al elegir, la ventana recibe WM_COMMAND con ECX=número y EDX=0.
;
;    menu_abrir        EBP=ventana ECX=número de título
;    menu_cerrar
;  (del gestor) menu_pintar_barra, menu_barra_clic, menu_raton_pulsa,
;  menu_raton_mueve, menu_raton_suelta, menu_tecla, menu_letra
; =============================================================================

MENU_IT_H   equ 20
MENU_SEP_H  equ 8

c_menu      dd menu_proc
            db C_CREMA, 0, 0, 0

; (interna) ESI=texto -> EAX=caracteres sin contar «&»; ESI, tras el 0
menu_ancho_txt:
        xor eax, eax
.c:
        cmp byte [esi], 0
        je .x
        cmp byte [esi], '&'
        je .s
        inc eax
.s:
        inc esi
        jmp .c
.x:
        inc esi
        ret

; (interna) ESI=primera orden de un desplegable -> ESI, tras su final
menu_saltar:
        cmp word [esi], 0xFFFF
        je .x
        add esi, 2
.c:
        lodsb
        test al, al
        jnz .c
        jmp menu_saltar
.x:
        add esi, 2
        ret

; (interna) ECX=número de orden del desplegable abierto -> ESI (CF=1: no hay)
menu_orden:
        push eax
        push ecx
        mov esi, [menu_items]
.b:
        cmp word [esi], 0xFFFF
        je .no
        test ecx, ecx
        jz .si
        add esi, 2
.c:
        lodsb
        test al, al
        jnz .c
        dec ecx
        jmp .b
.si:
        pop ecx
        pop eax
        clc
        ret
.no:
        pop ecx
        pop eax
        stc
        ret

menu_pintar_barra:
        pushad
        COLOR C_PIEDRA
        mov eax, BORDE
        mov ebx, BORDE + TIT_H + 1
        mov ecx, [ebp+V_AN]
        sub ecx, 2*BORDE
        mov edx, MENU_H - 1
        call dc_rect
        COLOR C_MARRON
        mov ebx, BORDE + TIT_H + MENU_H
        mov edx, 1
        call dc_rect
        mov esi, [ebp+V_MENU]
        mov edi, BORDE + 6          ; x
        xor ebx, ebx                ; número de título
.t:
        cmp byte [esi], 0
        je .x
        push esi
        call menu_ancho_txt
        lea ecx, [eax*8+16]
        call menu_saltar
        xchg esi, [esp]
        COLOR C_TINTA
        cmp dword [menu_ven], 0
        je .n
        cmp ebp, [menu_duena]
        jne .n
        cmp ebx, [menu_idx]
        jne .n
        push ebx
        COLOR C_ANIL
        mov eax, edi
        mov ebx, BORDE + TIT_H + 2
        mov edx, MENU_H - 3
        call dc_rect
        pop ebx
        COLOR C_BLANCO
.n:
        push ebx
        lea eax, [edi+8]
        mov ebx, BORDE + TIT_H + 3
        call dc_texto_m
        pop ebx
        add edi, ecx
        pop esi
        inc ebx
        jmp .t
.x:
        popad
        ret

menu_abrir:
        pushad
        call menu_cerrar
        mov esi, [ebp+V_MENU]
        test esi, esi
        jz .x
        mov edx, BORDE + 6
.t:
        cmp byte [esi], 0
        je .x
        call menu_ancho_txt
        test ecx, ecx
        jz .enc
        lea edx, [edx+eax*8+16]
        call menu_saltar
        dec ecx
        jmp .t
.enc:
        mov [menu_items], esi
        xor edi, edi                ; ancho, alto y número de órdenes
        xor ebx, ebx
        xor ecx, ecx
.m:
        cmp word [esi], 0xFFFF
        je .mf
        add ebx, MENU_SEP_H
        cmp word [esi], 0
        je .m2
        add ebx, MENU_IT_H - MENU_SEP_H
.m2:
        add esi, 2
        call menu_ancho_txt
        cmp eax, edi
        jbe .m3
        mov edi, eax
.m3:
        inc ecx
        jmp .m
.mf:
        mov [menu_n], ecx
        mov dword [menu_plant+P_CLASE], c_menu
        mov dword [menu_plant+P_ESTILO], E_EMERGENTE
        lea ecx, [edi*8+34]
        mov [menu_plant+P_AN], ecx
        add ebx, 6
        mov [menu_plant+P_AL], ebx
        mov eax, [ebp+V_X]
        add eax, edx
        mov edx, [vid_ancho]
        sub edx, ecx
        sub edx, SOMBRA
        cmp eax, edx
        jle .m4
        mov eax, edx
.m4:
        mov [menu_plant+P_X], eax
        mov eax, [ebp+V_Y]
        add eax, BORDE + TIT_H + MENU_H
        mov [menu_plant+P_Y], eax
        mov [menu_duena], ebp
        mov eax, [esp+24]
        mov [menu_idx], eax
        mov dword [menu_sel], -1
        push ebp
        mov esi, menu_plant
        xor ebp, ebp
        call ven_crear
        pop ebp
        mov [menu_ven], eax
        test eax, eax
        jnz .ok
        mov dword [menu_duena], 0
.ok:
        or dword [ebp+V_ESTADO], S_NCSUCIO
        mov byte [actividad], 1
.x:
        popad
        ret

menu_cerrar:
        cmp dword [menu_ven], 0
        je .x
        push ebp
        mov ebp, [menu_ven]
        mov dword [menu_ven], 0
        call ven_destruir
        mov ebp, [menu_duena]
        mov dword [menu_duena], 0
        or dword [ebp+V_ESTADO], S_NCSUCIO
        mov byte [actividad], 1
        pop ebp
.x:
        ret

; el desplegable
menu_proc:
        cmp eax, WM_PAINT
        jne ven_defecto
        call dc_pintar
        call ven_cli
        mov ebp, ecx                ; ancho
        mov esi, [menu_items]
        xor edi, edi
        mov ebx, 2
.o:
        cmp word [esi], 0xFFFF
        je .f
        cmp word [esi], 0
        jne .o1
        COLOR C_ARENA_OSC           ; raya
        mov eax, 4
        add ebx, 3
        lea ecx, [ebp-8]
        mov edx, 1
        call dc_rect
        add ebx, MENU_SEP_H - 3
        jmp .o3
.o1:
        COLOR C_TINTA
        cmp edi, [menu_sel]
        jne .o2
        COLOR C_ANIL
        mov eax, 2
        lea ecx, [ebp-4]
        mov edx, MENU_IT_H
        call dc_rect
        COLOR C_BLANCO
.o2:
        mov eax, 16
        add ebx, 2
        add esi, 2
        call dc_texto_m
        sub esi, 2
        add ebx, MENU_IT_H - 2
.o3:
        add esi, 2
.o4:
        lodsb
        test al, al
        jnz .o4
        inc edi
        jmp .o
.f:
        call dc_cerrar
        xor eax, eax
        ret

; (interna) EAX=x EBX=y de la pantalla -> EAX=orden que hay ahí; -1 si es
; una raya o un hueco; CF=1 si el punto no está en el desplegable
menu_en:
        push ebx
        push ecx
        push edx
        push esi
        push ebp
        mov ebp, [menu_ven]
        sub eax, [ebp+V_X]
        js .fuera
        cmp eax, [ebp+V_AN]
        jge .fuera
        sub ebx, [ebp+V_Y]
        js .fuera
        cmp ebx, [ebp+V_AL]
        jge .fuera
        sub ebx, 3
        js .nada
        mov esi, [menu_items]
        xor ecx, ecx
.o:
        cmp word [esi], 0xFFFF
        je .nada
        mov edx, MENU_SEP_H
        cmp word [esi], 0
        je .o1
        mov edx, MENU_IT_H
.o1:
        cmp ebx, edx
        jb .este
        sub ebx, edx
        add esi, 2
.o2:
        lodsb
        test al, al
        jnz .o2
        inc ecx
        jmp .o
.este:
        cmp word [esi], 0
        je .nada
        mov eax, ecx
        jmp .si
.nada:
        mov eax, -1
.si:
        clc
        jmp .x
.fuera:
        stc
.x:
        pop ebp
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) EAX=orden elegida (o -1): la marca
menu_marcar:
        cmp eax, [menu_sel]
        je .x
        mov [menu_sel], eax
        push ebp
        mov ebp, [menu_ven]
        call ven_invalidar
        pop ebp
.x:
        ret

; (interna) da la orden marcada a la ventana y cierra el menú
menu_elegir:
        pushad
        mov ecx, [menu_sel]
        test ecx, ecx
        js .x
        call menu_orden
        jc .x
        movzx ecx, word [esi]
        mov ebp, [menu_duena]
        call menu_cerrar
        mov eax, WM_COMMAND
        xor edx, edx
        call msg_poner
.x:
        popad
        ret

; clic en la barra de menús de EBP, en EAX=x de la pantalla
menu_barra_clic:
        pushad
        sub eax, [ebp+V_X]
        mov esi, [ebp+V_MENU]
        mov edx, BORDE + 6
        xor ecx, ecx
.t:
        cmp byte [esi], 0
        je .cerrar
        push eax
        call menu_ancho_txt
        lea ebx, [eax*8+16]
        pop eax
        cmp eax, edx
        jl .cerrar
        add edx, ebx
        cmp eax, edx
        jl .este
        call menu_saltar
        inc ecx
        jmp .t
.este:
        cmp dword [menu_ven], 0
        je .abrir
        cmp ebp, [menu_duena]
        jne .abrir
        cmp ecx, [menu_idx]
        je .cerrar
.abrir:
        call menu_abrir
        jmp .x
.cerrar:
        call menu_cerrar
.x:
        popad
        ret

menu_raton_pulsa:
        pushad
        push eax
        call menu_en
        jc .fuera
        call menu_marcar
        pop eax
        jmp .x
.fuera:
        pop eax
        call ges_donde
        cmp ebp, [menu_duena]
        jne .cerrar
        cmp ecx, Z_MENU
        jne .cerrar
        call menu_barra_clic
        jmp .x
.cerrar:
        call menu_cerrar
.x:
        popad
        ret

menu_raton_mueve:
        pushad
        call menu_en
        jc .x
        call menu_marcar
.x:
        popad
        ret

menu_raton_suelta:
        pushad
        call menu_en
        jc .x
        call menu_marcar
        call menu_elegir
.x:
        popad
        ret

menu_tecla:
        pushad
        cmp al, 27
        je .cerrar
        cmp al, 13
        je .elegir
        test al, al
        jz .ext
        cmp al, 0xE0
        jne .x
.ext:
        cmp ah, TC_ABAJO
        je .abajo
        cmp ah, TC_ARRIBA
        je .arriba
        mov ebp, [menu_duena]
        mov ecx, [menu_idx]
        cmp ah, TC_DER
        je .der
        cmp ah, TC_IZQ
        jne .x
        dec ecx
        jns .cambia
        ; el último título
        mov esi, [ebp+V_MENU]
        xor ecx, ecx
.cu:
        cmp byte [esi], 0
        je .cu2
        call menu_ancho_txt
        call menu_saltar
        inc ecx
        jmp .cu
.cu2:
        dec ecx
        jmp .cambia
.der:
        inc ecx
        call menu_abrir
        cmp dword [menu_ven], 0
        jne .primera
        xor ecx, ecx
.cambia:
        call menu_abrir
.primera:
        cmp dword [menu_ven], 0
        je .x
        xor eax, eax
        call menu_marcar
        jmp .x
.abajo:
        mov edx, 1
        jmp .mueve
.arriba:
        mov edx, -1
.mueve:
        mov ecx, [menu_sel]
        mov edi, [menu_n]
        test edi, edi
        jz .x
        mov ebx, edi
.m1:
        add ecx, edx
        jns .m2
        lea ecx, [edi-1]
.m2:
        cmp ecx, edi
        jl .m3
        xor ecx, ecx
.m3:
        call menu_orden
        jc .x
        cmp word [esi], 0
        jne .m4
        dec ebx
        jnz .m1
        jmp .x
.m4:
        mov eax, ecx
        call menu_marcar
        jmp .x
.elegir:
        call menu_elegir
        jmp .x
.cerrar:
        call menu_cerrar
.x:
        popad
        ret

; Alt+letra (BL, en mayúscula) en la ventana EBP: CF=1 si abre un menú
menu_letra:
        pushad
        mov esi, [ebp+V_MENU]
        xor ecx, ecx
.t:
        cmp byte [esi], 0
        je .no
        mov edi, esi
.c:
        mov al, [edi]
        inc edi
        test al, al
        jz .sig
        cmp al, '&'
        jne .c
        mov al, [edi]
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 0x20
.c1:
        cmp al, bl
        je .si
        jmp .c
.sig:
        call menu_ancho_txt
        call menu_saltar
        inc ecx
        jmp .t
.si:
        call menu_abrir
        xor eax, eax
        call menu_marcar
        popad
        stc
        ret
.no:
        popad
        clc
        ret
