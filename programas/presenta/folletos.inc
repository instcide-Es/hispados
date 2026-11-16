; =============================================================================
;  folletos.inc - imprimir folletos y notas en Presentaciones
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  «Imprimir» (una diapositiva por página, girada para llenar la hoja) es la
;  orden del editor de láminas. Aquí están las otras: folletos de 2, 3 (con
;  renglones al lado para apuntar) o 6 diapositivas por hoja, y las notas del
;  orador (la diapositiva arriba y sus notas debajo). Cada hoja se compone en
;  hoja_buf, un A4 de pie: cada diapositiva se dibuja con el motor, se reduce
;  (lam_reduce) y se pega en su sitio con un filete; las notas las escribe el
;  motor directamente en la hoja (lam_destino y lam_pinta_obj con una copia de
;  la ficha de la caja de las notas). La hoja va a la impresora por la misma
;  vía que todo (imp_prepara e imp_pagina del editor de láminas).
; =============================================================================
; de cada modo: cuántas por hoja, su ancho y su alto, y dónde va cada una
            align 4
fo_2        dd 2, 640, 480, 77, 60, 77, 600
fo_3        dd 3, 320, 240, 50, 80, 50, 420, 50, 760
fo_6        dd 6, 320, 240, 50, 80, 424, 80, 50, 420, 424, 420, 50, 760, 424, 760
fo_n        dd 1, 640, 480, 77, 50

folleto2:
        mov esi, fo_2
        jmp folleto
folleto3:
        mov esi, fo_3
        jmp folleto
folleto6:
        mov esi, fo_6
        jmp folleto
imp_notas:
        mov esi, fo_n
; ESI=el modo: imprime toda la presentación así
folleto:
        pushad
        mov [fo_modo], esi
        call fin_edicion
        mov ecx, HOJA_AN * HOJA_AL + 8  ; la hoja y la diapositiva reducida: la
        mov ebx, hoja_buf               ; memoria se pide ahora y se devuelve al acabar
        xor al, al
        call lam_bloque
        jc .sin
        mov ecx, [esi+4]
        imul ecx, [esi+8]
        mov ebx, esc_buf
        call lam_bloque
        jc .sin
        call imp_abre
        jc .mal
        mov edi, [hoja_buf]
        mov ecx, HOJA_AN
        mov edx, HOJA_AL
        call imp_prepara
        xor ebp, ebp                ; EBP=la diapositiva
.hoja:
        cmp ebp, [lam_pags]
        jae .fin
        mov edi, [hoja_buf]           ; una hoja en blanco
        mov ecx, HOJA_AN * HOJA_AL
        mov al, C_BLANCO
        rep stosb
        mov dword [fo_k], 0
.dia:
        mov esi, [fo_modo]
        mov eax, [fo_k]
        cmp eax, [esi]
        jae .imprime
        cmp ebp, [lam_pags]
        jae .imprime
        mov ecx, ebp
        call lam_pinta
        cmp byte [lam_sin_mem], 0
        jne .sin_c
        mov edi, [esc_buf]            ; reducida...
        mov ecx, [esi+4]
        mov edx, [esi+8]
        mov ebx, [suave]
        call lam_reduce
        mov eax, [fo_k]             ; ...y a su sitio
        mov ebx, [esi+16+eax*8]
        mov eax, [esi+12+eax*8]
        call hoja_pega
        cmp esi, fo_3
        jne .n1
        call hoja_renglones
.n1:
        cmp esi, fo_n
        jne .n2
        call hoja_notas
.n2:
        inc ebp
        inc dword [fo_k]
        jmp .dia
.imprime:
        call imp_pagina
        jc .mal
        jmp .hoja
.fin:
        call imp_lista
        jc .mal
        call imp_cierra
        mov esi, t_impreso
.dice:
        mov byte [esc_sucio], 1     ; (esc_buf se ha usado para reducir)
        call pre_suelta
        call aviso
        popad
        ret
.mal:
        call imp_cierra
        mov esi, t_e_imprimir
        jmp .dice
.sin_c:
        call imp_cierra
.sin:
        mov esi, t_sin_mem
        jmp .dice

; (interna) pega en la hoja, en (EAX, EBX), lo que hay en esc_buf (ECX x EDX),
; con un filete negro alrededor
hoja_pega:
        pushad
        mov esi, [esc_buf]
        mov ebp, edx                ; las filas
        imul ebx, HOJA_AN
        mov edi, [hoja_buf]
        add edi, ebx
        add edi, eax
        mov edx, edi                ; (EDX=su esquina en la hoja)
.f:
        push ecx
        push edi
        rep movsb
        pop edi
        pop ecx
        add edi, HOJA_AN
        dec ebp
        jnz .f
        mov ebp, [esp+20]           ; (el alto)
        mov al, C_TINTA
        lea edi, [edx-HOJA_AN-1]    ; el filete: arriba y abajo
        push ecx
        add ecx, 2
        rep stosb
        pop ecx
        mov edi, ebp
        imul edi, HOJA_AN
        lea edi, [edx+edi-1]
        push ecx
        add ecx, 2
        rep stosb
        pop ecx
        mov edi, edx                ; y a los lados
.l:
        mov [edi-1], al
        mov [edi+ecx], al
        add edi, HOJA_AN
        dec ebp
        jnz .l
        popad
        ret

; (interna) los renglones para apuntar, a la derecha de la diapositiva que se
; acaba de pegar en (EAX, EBX)
hoja_renglones:
        pushad
        add ebx, 30
        imul ebx, HOJA_AN
        mov edi, [hoja_buf]
        lea edi, [edi+ebx+400]
        mov edx, 7
.r:
        push edi
        mov ecx, 344
        mov al, C_TINTA
        rep stosb
        pop edi
        add edi, 32 * HOJA_AN
        dec edx
        jnz .r
        popad
        ret

; (interna) las notas de la diapositiva EBP, debajo de ella: las escribe el
; motor en la hoja, con una copia de la ficha de su caja puesta en su sitio
hoja_notas:
        pushad
        mov ecx, ebp
        call notas_de_pag
        test eax, eax
        js .x
        mov edi, ficha_tmp
        mov ecx, LO_TAM
        rep movsb
        mov esi, ficha_tmp
        mov dword [esi+LO_X], 77
        mov dword [esi+LO_Y], 560
        mov dword [esi+LO_AN], 640
        mov dword [esi+LO_AL], 520
        mov byte [esi+LO_TINTA], C_TINTA
        mov byte [esi+LO_FONDO], LAM_NADA
        mov byte [esi+LO_BORDE], LAM_NADA
        mov byte [esi+LO_LETRA], 9
        mov byte [esi+LO_CUERPO], 2 ; 18 puntos: que se lea de pie
        mov byte [esi+LO_ESTILO], 0
        mov byte [esi+LO_ALIN], 0
        mov byte [esi+LO_BAND], 0
        mov edi, [hoja_buf]
        mov ecx, HOJA_AN
        mov edx, HOJA_AL
        call lam_destino
        call lam_pinta_obj
        xor edi, edi
        call lam_destino
.x:
        popad
        ret
