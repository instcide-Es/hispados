; =============================================================================
;  Hispa Ventanas - regiones de recorte y contexto de dispositivo
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Una ventana solo dibuja en su «región visible»: su zona menos lo que tapan
;  las ventanas que tiene encima, como lista de rectángulos. El contexto de
;  dispositivo (uno, el de la ventana que está pintando) es esa región más el
;  origen de la zona de la ventana: las funciones dc_* reciben coordenadas de
;  la ventana y repiten la primitiva de gdi.inc en cada rectángulo visible.
;
;    dc_abrir     EBP=ventana: contexto de toda su zona interior (GetDC)
;    dc_pintar    EBP=ventana: solo lo que hay que repintar; borra el fondo
;                 con el color de la clase y da la zona por pintada (BeginPaint)
;    dc_cerrar    (EndPaint / ReleaseDC)
;    dc_cortar    EAX=x EBX=y ECX=ancho EDX=alto: estrecha el contexto
;    dc_rect, dc_marco, dc_mosaico, dc_texto, dc_texto_m, dc_punto, dc_linea,
;    dc_copiar_clave, dc_copiar, dc_caracter   como las gdi_*, en coordenadas
;                 de ventana
;    dc_icono     EAX=x EBX=y ESI=icono (32x32 en texto, ver iconos.inc)
;    dc_texto_n   EAX=x EBX=y ESI=texto ECX=caracteres
;  Estado: gdi_color (macro COLOR) y gdi_negrita.
;
;  (internas) reg, reg_n: la región en curso (x0, y0, x1, y1 por rectángulo)
;    reg_ventana  EBP=ventana AL=0 zona interior, 1 todo (marco y sombra)
;    reg_anadir, reg_cortar, reg_restar   EAX=x0 EBX=y0 ECX=x1 EDX=y1
;    pres_unir    lo mismo: zona que hay que llevar a la pantalla
; =============================================================================

REG_MAX     equ 128

reg_anadir:
        cmp eax, ecx
        jge .x
        cmp ebx, edx
        jge .x
        push esi
        mov esi, [reg_n]
        cmp esi, REG_MAX
        jae .y
        shl esi, 4
        mov [reg+esi], eax
        mov [reg+esi+4], ebx
        mov [reg+esi+8], ecx
        mov [reg+esi+12], edx
        inc dword [reg_n]
.y:
        pop esi
.x:
        ret

; deja de cada rectángulo lo que cae dentro de (EAX, EBX)-(ECX, EDX)
reg_cortar:
        pushad
        mov esi, reg
        mov edi, reg
        mov ebp, [reg_n]
        mov dword [reg_n], 0
.l:
        test ebp, ebp
        jz .x
        dec ebp
        push eax
        push ebx
        push ecx
        push edx
        cmp eax, [esi]
        jge .a
        mov eax, [esi]
.a:
        cmp ebx, [esi+4]
        jge .b
        mov ebx, [esi+4]
.b:
        cmp ecx, [esi+8]
        jle .c
        mov ecx, [esi+8]
.c:
        cmp edx, [esi+12]
        jle .d
        mov edx, [esi+12]
.d:
        cmp eax, ecx
        jge .s
        cmp ebx, edx
        jge .s
        mov [edi], eax
        mov [edi+4], ebx
        mov [edi+8], ecx
        mov [edi+12], edx
        add edi, 16
        inc dword [reg_n]
.s:
        pop edx
        pop ecx
        pop ebx
        pop eax
        add esi, 16
        jmp .l
.x:
        popad
        ret

; quita de la región el rectángulo (EAX, EBX)-(ECX, EDX): cada rectángulo
; que lo toca se parte en hasta cuatro
reg_restar:
        pushad
        cmp eax, ecx
        jge .x
        cmp ebx, edx
        jge .x
        mov [rr_x0], eax
        mov [rr_y0], ebx
        mov [rr_x1], ecx
        mov [rr_y1], edx
        mov ecx, [reg_n]            ; la lista, a un lado
        mov ebp, ecx
        shl ecx, 2
        mov esi, reg
        mov edi, reg2
        rep movsd
        mov dword [reg_n], 0
        mov esi, reg2
.l:
        test ebp, ebp
        jz .x
        dec ebp
        mov eax, [esi]
        mov ebx, [esi+4]
        mov ecx, [esi+8]
        mov edx, [esi+12]
        add esi, 16
        cmp eax, [rr_x1]
        jge .entero
        cmp ecx, [rr_x0]
        jle .entero
        cmp ebx, [rr_y1]
        jge .entero
        cmp edx, [rr_y0]
        jg .parte
.entero:
        call reg_anadir
        jmp .l
.parte:
        cmp ebx, [rr_y0]            ; lo de encima
        jge .p1
        push edx
        mov edx, [rr_y0]
        call reg_anadir
        pop edx
        mov ebx, [rr_y0]
.p1:
        cmp edx, [rr_y1]            ; lo de debajo
        jle .p2
        push ebx
        mov ebx, [rr_y1]
        call reg_anadir
        pop ebx
        mov edx, [rr_y1]
.p2:
        cmp eax, [rr_x0]            ; a la izquierda
        jge .p3
        push ecx
        mov ecx, [rr_x0]
        call reg_anadir
        pop ecx
.p3:
        cmp ecx, [rr_x1]            ; a la derecha
        jle .l
        mov eax, [rr_x1]
        call reg_anadir
        jmp .l
.x:
        popad
        ret

; (interna) EBP=ventana: quita de la región lo que ocupa (con su sombra)
reg_restar_ven:
        pushad
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        mov ecx, [ebp+V_AN]
        add ecx, eax
        mov edx, [ebp+V_AL]
        add edx, ebx
        call reg_restar
        test dword [ebp+V_ESTADO], S_MAXIM
        jnz .x
        mov eax, ecx                ; la sombra de la derecha
        add ebx, SOMBRA
        add ecx, SOMBRA
        call reg_restar
        mov eax, [ebp+V_X]          ; y la de abajo
        add eax, SOMBRA
        mov ebx, edx
        add edx, SOMBRA
        call reg_restar
.x:
        popad
        ret

reg_ventana:
        pushad
        mov dword [reg_n], 0
        call ven_visible
        jc .x
        cmp byte [esp+28], 0
        jne .todo
        call ven_cli                ; la zona interior...
        add ecx, eax
        add edx, ebx
        call reg_anadir
        mov esi, ebp
.p:
        mov esi, [esi+V_PADRE]      ; ...dentro de las de sus antepasadas
        test esi, esi
        jz .pantalla
        push ebp
        mov ebp, esi
        call ven_cli
        pop ebp
        add ecx, eax
        add edx, ebx
        call reg_cortar
        jmp .p
.todo:
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        mov ecx, [ebp+V_AN]
        add ecx, eax
        mov edx, [ebp+V_AL]
        add edx, ebx
        call reg_anadir
        test dword [ebp+V_ESTADO], S_MAXIM
        jnz .pantalla
        mov eax, ecx
        add ebx, SOMBRA
        add ecx, SOMBRA
        call reg_anadir
        mov eax, [ebp+V_X]
        add eax, SOMBRA
        mov ebx, edx
        add edx, SOMBRA
        call reg_anadir
.pantalla:
        xor eax, eax
        xor ebx, ebx
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        call reg_cortar
        ; fuera lo que tapan las ventanas de encima
        call ven_tope
        xor esi, esi
        cmp eax, [escritorio]
        je .enc
.busca:
        cmp esi, [n_orden]
        jae .x
        cmp [orden+esi*4], eax
        lea esi, [esi+1]
        jne .busca
.enc:
        cmp esi, [n_orden]
        jae .x
        mov ebp, [orden+esi*4]
        inc esi
        mov eax, [ebp+V_ESTADO]
        and eax, S_VISIBLE | S_MINIM
        cmp eax, S_VISIBLE
        jne .enc
        call reg_restar_ven
        jmp .enc
.x:
        popad
        ret

; la zona que hay que llevar a la pantalla
pres_unir:
        push esi
        mov esi, [pres_x0]
        cmp esi, [pres_x1]
        jl .une
        mov [pres_x0], eax
        mov [pres_y0], ebx
        mov [pres_x1], ecx
        mov [pres_y1], edx
        jmp .x
.une:
        cmp eax, [pres_x0]
        jge .a
        mov [pres_x0], eax
.a:
        cmp ebx, [pres_y0]
        jge .b
        mov [pres_y0], ebx
.b:
        cmp ecx, [pres_x1]
        jle .c
        mov [pres_x1], ecx
.c:
        cmp edx, [pres_y1]
        jle .x
        mov [pres_y1], edx
.x:
        pop esi
        ret

; (interna) toda la región en curso habrá que presentarla
reg_a_pres:
        pushad
        mov esi, reg
        mov edi, [reg_n]
.l:
        test edi, edi
        jz .x
        dec edi
        mov eax, [esi]
        mov ebx, [esi+4]
        mov ecx, [esi+8]
        mov edx, [esi+12]
        call pres_unir
        add esi, 16
        jmp .l
.x:
        popad
        ret

dc_abrir:
        pushad
        xor al, al
        call reg_ventana
        jmp dc_listo

dc_pintar:
        pushad
        xor al, al
        call reg_ventana
        mov eax, [ebp+V_UX0]
        mov ebx, [ebp+V_UY0]
        mov ecx, [ebp+V_UX1]
        mov edx, [ebp+V_UY1]
        call reg_cortar
        xor esi, esi
        mov [ebp+V_UX0], esi
        mov [ebp+V_UX1], esi
        ; las hijas que caen en la zona también se repintan
        mov esi, [ven_tabla]
        mov edi, MAX_VEN
.h:
        cmp dword [esi+V_USO], 1
        jne .hs
        cmp [esi+V_PADRE], ebp
        jne .hs
        xchg esi, ebp
        call ven_inv_p
        xchg esi, ebp
.hs:
        add esi, V_TAM
        dec edi
        jnz .h
        call dc_origen
        mov esi, [ebp+V_CLASE]      ; el fondo
        mov al, [esi+C_FONDO]
        cmp al, SIN_FONDO
        je dc_listo.sin
        mov [gdi_color], al
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        sub eax, [dc_ox]
        sub ebx, [dc_oy]
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        call dc_rect
        jmp dc_listo.sin
dc_listo:
        call dc_origen
.sin:
        mov byte [gdi_negrita], 0
        popad
        ret

; (interna) origen del contexto = zona interior de EBP; fuera el cursor
dc_origen:
        call ven_cli
        mov [dc_ox], eax
        mov [dc_oy], ebx
        cmp dword [reg_n], 0
        je .x
        call reg_a_pres
        call cur_ocultar
.x:
        ret

dc_cerrar:
        jmp gdi_recorte_total

dc_cortar:
        pushad
        add eax, [dc_ox]
        add ebx, [dc_oy]
        add ecx, eax
        add edx, ebx
        call reg_cortar
        popad
        ret

; ---- las primitivas, una vez por cada rectángulo visible ----
%macro DCFN 2
dc_%1:
        push dword %2
        jmp dc_cada
%endmacro
        DCFN rect, gdi_rect
        DCFN marco, gdi_marco
        DCFN mosaico, gdi_mosaico
        DCFN texto, gdi_texto
        DCFN texto_m, gdi_texto_m
        DCFN texto_n, gdi_texto_n
        DCFN punto, gdi_punto
        DCFN copiar_clave, gdi_copiar_clave
        DCFN copiar, gdi_copiar
        DCFN caracter, gdi_caracter
        DCFN linea0, gdi_linea

dc_cada:
        xchg ebp, [esp]             ; EBP=primitiva
        pushad
        add eax, [dc_ox]
        add ebx, [dc_oy]
        mov edi, reg
        push dword [reg_n]
.l:
        cmp dword [esp], 0
        je .f
        dec dword [esp]
        push eax
        push ebx
        push ecx
        push edx
        mov eax, [edi]
        mov ebx, [edi+4]
        mov ecx, [edi+8]
        mov edx, [edi+12]
        call gdi_fijar_recorte
        pop edx
        pop ecx
        pop ebx
        pop eax
        push edi
        mov edi, [esp+8]            ; (el EDI de quien llama)
        call ebp
        pop edi
        add edi, 16
        jmp .l
.f:
        add esp, 4
        popad
        pop ebp
        ret

dc_linea:
        push ecx
        push edx
        add ecx, [dc_ox]
        add edx, [dc_oy]
        call dc_linea0
        pop edx
        pop ecx
        ret

; (el icono está a 4 bits por punto, el de la izquierda en los bajos, y el
; color 3 es el transparente: lo deja así tools/icopack.py al compilar)
dc_icono:
        pushad
        mov edi, ico_tmp
        mov ecx, 16 * 32
.c:
        lodsb
        mov ah, al
        and al, 0x0F
        cmp al, 3
        jne .c1
        mov al, CLAVE
.c1:
        stosb
        mov al, ah
        shr al, 4
        cmp al, 3
        jne .c2
        mov al, CLAVE
.c2:
        stosb
        dec ecx
        jnz .c
        popad
        push ecx
        push edx
        push esi
        mov ecx, 32
        mov edx, 32
        mov esi, ico_tmp
        call dc_copiar_clave
        pop esi
        pop edx
        pop ecx
        ret

; ---- añadidos a la capa de dibujo ----
; texto de menú: «&» no se escribe y subraya la letra que le sigue
gdi_texto_m:
        pushad
        xor edi, edi
.c:
        movzx edx, byte [esi]
        inc esi
        test edx, edx
        jz .x
        cmp dl, '&'
        jne .c1
        inc edi
        jmp .c
.c1:
        call gdi_caracter
        test edi, edi
        jz .c2
        xor edi, edi
        push ebx
        push ecx
        add ebx, 14
        mov ecx, 8
        mov edx, 1
        call gdi_rect
        pop ecx
        pop ebx
.c2:
        add eax, 8
        jmp .c
.x:
        popad
        ret

; texto de ECX caracteres (sin 0 final)
gdi_texto_n:
        pushad
.c:
        test ecx, ecx
        jle .x
        movzx edx, byte [esi]
        inc esi
        call gdi_caracter
        add eax, 8
        dec ecx
        jmp .c
.x:
        popad
        ret
