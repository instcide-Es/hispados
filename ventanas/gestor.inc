; =============================================================================
;  Hispa Ventanas - gestor de ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Lleva la lista de ventanas, su orden (cuál tapa a cuál), la «zona sucia»
;  (lo que hay que volver a pintar) y el arrastre con el ratón.
;
;  Cómo se pinta, sin parpadeo: nada se dibuja directamente en la pantalla.
;  Quien cambia algo marca un rectángulo como sucio (ges_invalidar); una vez
;  por vuelta, ges_actualizar pinta en la superficie solo esa zona, de atrás
;  adelante (fondo, ventanas en su orden y, encima, el cursor), y el driver la
;  lleva entera a la pantalla.
;
;    ges_crear         EAX=x EBX=y ECX=ancho EDX=alto ESI=título EDI=pintor
;                      (queda delante y activa)
;    ges_invalidar     EAX=x EBX=y ECX=ancho EDX=alto
;    ges_actualizar    pinta lo sucio y mueve el cursor
;    ges_raton_mueve / ges_raton_pulsa / ges_raton_suelta   EAX=x EBX=y
;    ges_tecla         AX=tecla (AL carácter, AH código)
;  El pintor de una ventana (lo que será WM_PAINT) recibe EAX=x EBX=y
;  ECX=ancho EDX=alto de su zona interior, con el recorte ya puesto.
; =============================================================================

; ---- una ventana ----
VEN_X       equ 0
VEN_Y       equ 4
VEN_AN      equ 8
VEN_AL      equ 12
VEN_TITULO  equ 16
VEN_PINTA   equ 20
VEN_TAM     equ 24
MAX_VEN     equ 8

; ---- medidas del aspecto ----
BORDE       equ 2                   ; marco
TIT_H       equ 22                  ; barra de título
SOMBRA      equ 5                   ; sombra, abajo y a la derecha
BARRA_H     equ 28                  ; barra superior del escritorio

ges_iniciar:
        xor eax, eax
        xor ebx, ebx
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        jmp ges_invalidar

ges_crear:
        pushad
        mov ebp, [n_ven]
        cmp ebp, MAX_VEN
        jae .x
        push eax
        mov eax, ebp
        mov [orden+ebp], al
        imul ebp, VEN_TAM
        add ebp, ventanas
        pop eax
        mov [ebp+VEN_X], eax
        mov [ebp+VEN_Y], ebx
        mov [ebp+VEN_AN], ecx
        mov [ebp+VEN_AL], edx
        mov [ebp+VEN_TITULO], esi
        mov [ebp+VEN_PINTA], edi
        inc dword [n_ven]
.x:
        popad
        ret

ges_invalidar:
        pushad
        add ecx, eax
        add edx, ebx
        mov esi, [suc_x0]
        cmp esi, [suc_x1]
        jl .une
        mov [suc_x0], eax
        mov [suc_y0], ebx
        mov [suc_x1], ecx
        mov [suc_y1], edx
        jmp .x
.une:
        cmp eax, [suc_x0]
        jge .a
        mov [suc_x0], eax
.a:
        cmp ebx, [suc_y0]
        jge .b
        mov [suc_y0], ebx
.b:
        cmp ecx, [suc_x1]
        jle .c
        mov [suc_x1], ecx
.c:
        cmp edx, [suc_y1]
        jle .x
        mov [suc_y1], edx
.x:
        popad
        ret

; (interna) ESI=número de ventana: todo lo que ocupa, con la sombra
ges_inv_ven:
        pushad
        imul esi, VEN_TAM
        add esi, ventanas
        mov eax, [esi+VEN_X]
        mov ebx, [esi+VEN_Y]
        mov ecx, [esi+VEN_AN]
        mov edx, [esi+VEN_AL]
        add ecx, SOMBRA
        add edx, SOMBRA
        call ges_invalidar
        popad
        ret

ges_actualizar:
        pushad
        mov eax, [suc_x0]
        cmp eax, [suc_x1]
        jl .hay
        cmp byte [hay_rat], 0
        je .x
        cmp byte [cur_puesto], 0
        je .hay
        mov eax, [rat_x]
        cmp eax, [cur_x]
        jne .hay
        mov eax, [rat_y]
        cmp eax, [cur_y]
        je .x
.hay:
        mov eax, [cur_x]            ; dónde estaba el cursor
        mov [act_x], eax
        mov eax, [cur_y]
        mov [act_y], eax
        mov al, [cur_puesto]
        mov [act_puesto], al
        call cur_ocultar
        mov eax, [suc_x0]
        cmp eax, [suc_x1]
        jge .cursor
        call ges_pintar
.cursor:
        call cur_mostrar
        ; a la pantalla: la zona pintada y el cursor, el de antes y el de ahora
        mov eax, [suc_x0]
        mov ecx, [suc_x1]
        cmp eax, ecx
        jge .p2
        mov ebx, [suc_y0]
        mov edx, [suc_y1]
        call vid_presentar
.p2:
        cmp byte [act_puesto], 0
        je .p3
        mov eax, [act_x]
        mov ebx, [act_y]
        lea ecx, [eax+CUR_ANCHO]
        lea edx, [ebx+CUR_ALTO]
        call vid_presentar
.p3:
        cmp byte [cur_puesto], 0
        je .p4
        mov eax, [cur_x]
        mov ebx, [cur_y]
        lea ecx, [eax+CUR_ANCHO]
        lea edx, [ebx+CUR_ALTO]
        call vid_presentar
.p4:
        xor eax, eax
        mov [suc_x0], eax
        mov [suc_x1], eax
.x:
        popad
        ret

; (interna) pinta la zona sucia en la superficie
ges_pintar:
        mov eax, [suc_x0]
        mov ebx, [suc_y0]
        mov ecx, [suc_x1]
        mov edx, [suc_y1]
        call gdi_fijar_recorte
        call esc_pintar
        xor edi, edi
.v:
        cmp edi, [n_ven]
        jae .f
        movzx esi, byte [orden+edi]
        imul esi, VEN_TAM
        add esi, ventanas
        lea eax, [edi+1]
        cmp eax, [n_ven]
        sete al                     ; la de delante es la activa
        call ges_dibujar
        inc edi
        jmp .v
.f:
        jmp gdi_recorte_total

; (interna) dibuja la ventana ESI; AL distinto de 0 = es la activa
ges_dibujar:
        pushad
        mov [gd_activa], al
        mov eax, [esi+VEN_X]
        mov ebx, [esi+VEN_Y]
        mov ecx, [esi+VEN_AN]
        mov edx, [esi+VEN_AL]
        mov [gd_x], eax
        mov [gd_y], ebx
        mov [gd_an], ecx
        mov [gd_al], edx
        ; ¿toca la zona que se está pintando?
        cmp eax, [rec_x1]
        jge .x
        cmp ebx, [rec_y1]
        jge .x
        lea edi, [eax+ecx+SOMBRA]
        cmp edi, [rec_x0]
        jle .x
        lea edi, [ebx+edx+SOMBRA]
        cmp edi, [rec_y0]
        jle .x
        ; sombra
        COLOR C_TINTA
        add eax, SOMBRA
        add ebx, edx
        mov edx, SOMBRA
        call gdi_rect
        mov eax, [gd_x]
        add eax, [gd_an]
        mov ebx, [gd_y]
        add ebx, SOMBRA
        mov ecx, SOMBRA
        mov edx, [gd_al]
        sub edx, SOMBRA
        call gdi_rect
        ; marco
        COLOR C_MARRON
        mov eax, [gd_x]
        mov ebx, [gd_y]
        mov ecx, [gd_an]
        mov edx, [gd_al]
        call gdi_rect
        ; barra de título
        COLOR C_ARENA
        cmp byte [gd_activa], 0
        je .t1
        COLOR C_TERRACOTA
.t1:
        add eax, BORDE
        add ebx, BORDE
        sub ecx, 2*BORDE
        mov edx, TIT_H
        call gdi_rect
        ; interior
        COLOR C_CREMA
        add ebx, TIT_H + 1
        mov edx, [gd_al]
        sub edx, 2*BORDE + TIT_H + 1
        call gdi_rect
        mov [gd_cy], ebx
        mov [gd_cal], edx
        ; lo demás, sin salirse de la ventana
        push dword [rec_x0]
        push dword [rec_y0]
        push dword [rec_x1]
        push dword [rec_y1]
        mov ebx, [gd_y]
        add ebx, BORDE
        mov edx, [gd_al]
        sub edx, 2*BORDE
        call gdi_cortar
        ; el rombo de la barra de título
        COLOR C_ARENA_OSC
        cmp byte [gd_activa], 0
        je .t2
        COLOR C_OCRE
.t2:
        xor edi, edi
.rombo:
        mov eax, edi
        sub eax, 4
        jns .r1
        neg eax
.r1:
        mov ecx, 4
        sub ecx, eax
        mov eax, [gd_x]
        add eax, BORDE + 13
        sub eax, ecx
        lea ecx, [ecx*2+1]
        mov ebx, [gd_y]
        add ebx, BORDE + TIT_H/2 - 4
        add ebx, edi
        mov edx, 1
        call gdi_rect
        inc edi
        cmp edi, 9
        jb .rombo
        ; el título
        COLOR C_MARRON
        mov byte [gdi_negrita], 0
        cmp byte [gd_activa], 0
        je .t3
        COLOR C_CREMA
        mov byte [gdi_negrita], 1
.t3:
        mov eax, [gd_x]
        add eax, BORDE + 26
        mov ebx, [gd_y]
        add ebx, BORDE + (TIT_H - 16) / 2
        push esi
        mov esi, [esi+VEN_TITULO]
        call gdi_texto
        pop esi
        mov byte [gdi_negrita], 0
        ; el contenido
        mov eax, [gd_x]
        add eax, BORDE
        mov ebx, [gd_cy]
        mov ecx, [gd_an]
        sub ecx, 2*BORDE
        mov edx, [gd_cal]
        call gdi_cortar
        cmp dword [esi+VEN_PINTA], 0
        je .sinpintor
        call [esi+VEN_PINTA]
.sinpintor:
        mov byte [gdi_negrita], 0
        pop dword [rec_y1]
        pop dword [rec_x1]
        pop dword [rec_y0]
        pop dword [rec_x0]
.x:
        popad
        ret

; (interna) EDI=puesto en el orden: esa ventana pasa delante
ges_activar:
        pushad
        mov ecx, [n_ven]
        dec ecx
        cmp edi, ecx
        jae .x
        movzx esi, byte [orden+ecx]
        call ges_inv_ven
        movzx esi, byte [orden+edi]
        call ges_inv_ven
        mov al, [orden+edi]
.d:
        mov ah, [orden+edi+1]
        mov [orden+edi], ah
        inc edi
        cmp edi, ecx
        jb .d
        mov [orden+ecx], al
.x:
        popad
        ret

; (interna) lleva la ventana ESI (puntero) a EAX=x EBX=y, sin dejar que se
; pierda: la barra de título queda siempre al alcance del ratón
ges_colocar:
        pushad
        mov ecx, 32
        sub ecx, [esi+VEN_AN]
        cmp eax, ecx
        jge .a
        mov eax, ecx
.a:
        mov ecx, [vid_ancho]
        sub ecx, 32
        cmp eax, ecx
        jle .b
        mov eax, ecx
.b:
        cmp ebx, BARRA_H
        jge .c
        mov ebx, BARRA_H
.c:
        mov ecx, [vid_alto]
        sub ecx, BORDE + TIT_H
        cmp ebx, ecx
        jle .d
        mov ebx, ecx
.d:
        cmp eax, [esi+VEN_X]
        jne .mueve
        cmp ebx, [esi+VEN_Y]
        je .x
.mueve:
        push eax
        push ebx
        mov eax, [esi+VEN_X]
        mov ebx, [esi+VEN_Y]
        mov ecx, [esi+VEN_AN]
        mov edx, [esi+VEN_AL]
        add ecx, SOMBRA
        add edx, SOMBRA
        call ges_invalidar          ; donde estaba
        pop ebx
        pop eax
        mov [esi+VEN_X], eax
        mov [esi+VEN_Y], ebx
        call ges_invalidar          ; donde está
.x:
        popad
        ret

; (interna) -> ESI=puntero a la ventana activa (CF=1 si no hay ninguna)
ges_la_activa:
        mov esi, [n_ven]
        test esi, esi
        stc
        jz .x
        movzx esi, byte [orden+esi-1]
        imul esi, VEN_TAM
        add esi, ventanas
        clc
.x:
        ret

ges_raton_pulsa:
        pushad
        call esc_en_salir
        jnc .ven
        mov byte [sal_armado], 1
        call esc_inv_salir
        jmp .x
.ven:
        mov edi, [n_ven]
.b:
        dec edi
        js .x
        movzx esi, byte [orden+edi]
        imul esi, VEN_TAM
        add esi, ventanas
        mov ecx, eax
        sub ecx, [esi+VEN_X]
        js .b
        cmp ecx, [esi+VEN_AN]
        jge .b
        mov edx, ebx
        sub edx, [esi+VEN_Y]
        js .b
        cmp edx, [esi+VEN_AL]
        jge .b
        call ges_activar            ; clic en una ventana: pasa delante
        cmp edx, BORDE + TIT_H
        jge .x
        mov byte [arrastre], 1      ; en la barra de título: empieza el arrastre
        mov [arr_dx], ecx
        mov [arr_dy], edx
.x:
        popad
        ret

ges_raton_mueve:
        cmp byte [arrastre], 0
        je .x
        pushad
        call ges_la_activa
        jc .y
        sub eax, [arr_dx]
        sub ebx, [arr_dy]
        call ges_colocar
.y:
        popad
.x:
        ret

ges_raton_suelta:
        mov byte [arrastre], 0
        cmp byte [sal_armado], 0
        je .x
        mov byte [sal_armado], 0
        call esc_inv_salir
        call esc_en_salir
        jnc .x
        mov byte [salir], 1
.x:
        ret

ges_tecla:
        pushad
        cmp al, 27                  ; Esc
        je .salir
        cmp al, 9                   ; Tab: la ventana del fondo pasa delante
        je .tab
        test al, al
        jz .ext
        cmp al, 0xE0
        jne .x
.ext:
        cmp ah, 0x6B                ; Alt+F4
        je .salir
        xor ecx, ecx
        xor edx, edx
        cmp ah, 0x48
        jne .t1
        mov edx, -8
.t1:
        cmp ah, 0x50
        jne .t2
        mov edx, 8
.t2:
        cmp ah, 0x4B
        jne .t3
        mov ecx, -8
.t3:
        cmp ah, 0x4D
        jne .t4
        mov ecx, 8
.t4:
        mov eax, ecx
        or eax, edx
        jz .x
        call ges_la_activa          ; flechas: mueven la ventana activa
        jc .x
        mov eax, [esi+VEN_X]
        add eax, ecx
        mov ebx, [esi+VEN_Y]
        add ebx, edx
        call ges_colocar
        jmp .x
.tab:
        cmp dword [n_ven], 2
        jb .x
        xor edi, edi
        call ges_activar
        jmp .x
.salir:
        mov byte [salir], 1
.x:
        popad
        ret
