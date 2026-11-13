; =============================================================================
;  Hispa Ventanas - controles: botón, casilla, opción, texto, cuadro de
;  texto de una y de varias líneas, lista y barra de desplazamiento
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Cada control es una clase de ventana hija; se crean con ven_crear y una
;  plantilla (clase, texto, estilo, x, y, ancho, alto, id). Avisan a su madre
;  con WM_COMMAND: ECX = id + aviso*65536, EDX = el control.
;
;    c_boton     botón (BS_DEFECTO), casilla (BS_CASILLA) u opción (BS_OPCION)
;                BM_MARCA, BM_PONER_MARCA
;    c_estatico  texto fijo (SS_DERECHA, SS_HUNDIDO, SS_NEGRITA); el carácter
;                10 cambia de línea
;    c_edit      cuadro de texto; con ES_MULTI, de varias líneas y con barra
;                WM_SETTEXT (EDX=texto), WM_GETTEXT (ECX=tamaño, EDX=búfer),
;                WM_CUT, WM_COPY, WM_PASTE, EM_SEL_TODO, EM_MODIFICADO,
;                EM_PONER_MODIF, EM_LARGO
;                ed_poner  EBP=cuadro ESI=texto ECX=octetos (quita los CR)
;                ed_datos  EBP=cuadro -> ESI=texto ECX=octetos (líneas con LF)
;                ed_seleccionar  EBP=cuadro EAX=desde EDX=hasta (el cursor)
;    c_lista     lista con barra: LM_ANADIR, LM_VACIAR, LM_SELEC,
;                LM_PONER_SELEC, LM_TEXTO, LM_CUENTA; avisos LN_SELEC, LN_DOBLE
;  Portapapeles (texto):
;    pp_poner    ESI=texto ECX=octetos
;    pp_buf, pp_len   lo que hay
; =============================================================================

PP_MAX      equ 65536
ED_MAX_MULTI equ 60000
ED_MAX_UNA  equ 255
LISTA_MAX   equ 200
LISTA_TXT   equ 40
LISTA_FILA  equ 16

c_boton     dd bot_proc
            db C_CREMA, CB_FOCO, 0, 0
c_estatico  dd est_proc
            db C_CREMA, 0, 0, 0
c_edit      dd ed_proc
            db C_BLANCO, CB_FOCO, 0, 0
c_lista     dd lis_proc
            db C_BLANCO, CB_FOCO, 0, 0

; ---- utilidades ----
; ESI=cadena -> EAX=largo
cad_largo:
        xor eax, eax
.c:
        cmp byte [esi+eax], 0
        je .x
        inc eax
        jmp .c
.x:
        ret

; rombo lleno: EAX=x EBX=y del centro, ECX=radio
dc_rombo:
        pushad
        mov esi, eax
        mov edi, ecx
        mov ebp, ecx
        neg ebp                     ; de -radio a +radio
        add ebx, ebp
.f:
        mov eax, ebp
        test eax, eax
        jns .p
        neg eax
.p:
        mov ecx, edi
        sub ecx, eax
        mov eax, esi
        sub eax, ecx
        lea ecx, [ecx*2+1]
        mov edx, 1
        call dc_rect
        inc ebx
        inc ebp
        cmp ebp, edi
        jle .f
        popad
        ret

; (interna) EDX=lParam de un mensaje de ratón -> EAX=x EBX=y; CF=1 si el
; punto cae fuera de la ventana EBP
raton_xy:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        test eax, eax
        js .f
        cmp eax, [ebp+V_AN]
        jge .f
        test ebx, ebx
        js .f
        cmp ebx, [ebp+V_AL]
        jge .f
        clc
        ret
.f:
        stc
        ret

; (interna) avisa a la madre: WM_COMMAND con el id de EBP y el aviso EAX
avisar:
        push ecx
        push edx
        push ebp
        mov ecx, eax
        shl ecx, 16
        mov cx, [ebp+V_ID]
        mov edx, ebp
        mov ebp, [ebp+V_PADRE]
        mov eax, WM_COMMAND
        call msg_enviar
        pop ebp
        pop edx
        pop ecx
        ret

; ---- botón, casilla y opción ----
bot_proc:
        cmp eax, WM_PAINT
        je bot_pintar
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_CHAR
        je .car
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_SETFOCUS
        je .inv
        cmp eax, WM_KILLFOCUS
        je .inv
        cmp eax, BM_MARCA
        je .marca
        cmp eax, BM_PONER_MARCA
        jne ven_defecto
        mov [ebp+V_D0], ecx
        jmp .inv
.marca:
        mov eax, [ebp+V_D0]
        ret
.pulsa:
        mov dword [ebp+V_D1], 1
.inv:
        call ven_invalidar
        xor eax, eax
        ret
.mueve:
        cmp ebp, [captura]
        jne .nada
        call raton_xy
        setnc al
        movzx eax, al
        cmp eax, [ebp+V_D1]
        je .nada
        mov [ebp+V_D1], eax
        jmp .inv
.suelta:
        cmp dword [ebp+V_D1], 0
        je .nada
        mov dword [ebp+V_D1], 0
        call ven_invalidar
        jmp bot_clic
.car:
        cmp ecx, ' '
        jne ven_defecto
        jmp bot_clic
.tecla:
        cmp ecx, TC_INTRO
        jne ven_defecto
        test dword [ebp+V_ESTILO], BS_CASILLA | BS_OPCION
        jnz ven_defecto
        jmp bot_clic
.nada:
        xor eax, eax
        ret

bot_clic:
        test dword [ebp+V_ESTILO], BS_CASILLA
        jz .o
        xor dword [ebp+V_D0], 1
        call ven_invalidar
        jmp .avisa
.o:
        test dword [ebp+V_ESTILO], BS_OPCION
        jz .avisa
        mov esi, [ven_tabla]        ; las demás opciones de la madre, fuera
        mov ecx, MAX_VEN
.b:
        cmp dword [esi+V_USO], 1
        jne .s
        mov eax, [ebp+V_PADRE]
        cmp [esi+V_PADRE], eax
        jne .s
        cmp dword [esi+V_CLASE], c_boton
        jne .s
        test dword [esi+V_ESTILO], BS_OPCION
        jz .s
        cmp dword [esi+V_D0], 0
        je .s
        mov dword [esi+V_D0], 0
        xchg esi, ebp
        call ven_invalidar
        xchg esi, ebp
.s:
        add esi, V_TAM
        dec ecx
        jnz .b
        mov dword [ebp+V_D0], 1
        call ven_invalidar
.avisa:
        xor eax, eax
        call avisar
        xor eax, eax
        ret

bot_pintar:
        call dc_pintar
        call ven_cli
        mov esi, ecx                ; ancho
        mov edi, edx                ; alto
        test dword [ebp+V_ESTILO], BS_CASILLA | BS_OPCION
        jnz .marca
        COLOR C_ARENA
        cmp dword [ebp+V_D1], 0
        je .n1
        COLOR C_OCRE
.n1:
        xor eax, eax
        xor ebx, ebx
        call dc_rect
        cmp dword [ebp+V_D1], 0
        jne .n2
        COLOR C_BLANCO              ; luz arriba y a la izquierda
        mov eax, 1
        mov ebx, 1
        lea ecx, [esi-2]
        mov edx, 1
        call dc_rect
        mov ecx, 1
        lea edx, [edi-2]
        call dc_rect
        COLOR C_ARENA_OSC           ; y penumbra abajo y a la derecha
        lea eax, [esi-2]
        call dc_rect
        mov eax, 1
        lea ebx, [edi-2]
        lea ecx, [esi-2]
        mov edx, 1
        call dc_rect
.n2:
        COLOR C_MARRON
        xor eax, eax
        xor ebx, ebx
        mov ecx, esi
        mov edx, edi
        call dc_marco
        test dword [ebp+V_ESTILO], BS_DEFECTO
        jz .n3
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        call dc_marco
.n3:
        cmp ebp, [foco]
        jne .n4
        COLOR C_TERRACOTA
        mov eax, 3
        mov ebx, 3
        lea ecx, [esi-6]
        lea edx, [edi-6]
        call dc_marco
.n4:
        COLOR C_TINTA
        test dword [ebp+V_ESTADO], S_INACTIVA
        jz .n5
        COLOR C_ARENA_OSC
.n5:
        push esi
        mov esi, [ebp+V_TEXTO]
        call cad_largo
        pop ecx
        shl eax, 3
        sub ecx, eax
        sar ecx, 1
        mov eax, ecx
        lea ebx, [edi-16]
        sar ebx, 1
        add eax, [ebp+V_D1]         ; apretado: un punto abajo y a la derecha
        add ebx, [ebp+V_D1]
        call dc_texto
        jmp .fin
.marca:
        lea edi, [edi-14]
        sar edi, 1                  ; y del recuadro
        test dword [ebp+V_ESTILO], BS_OPCION
        jnz .opcion
        COLOR C_BLANCO
        cmp dword [ebp+V_D1], 0
        je .m1
        COLOR C_OCRE
.m1:
        xor eax, eax
        mov ebx, edi
        mov ecx, 14
        mov edx, 14
        call dc_rect
        COLOR C_MARRON
        call dc_marco
        cmp dword [ebp+V_D0], 0
        je .texto
        COLOR C_TERRACOTA
        mov eax, 3
        lea ebx, [edi+3]
        mov ecx, 8
        mov edx, 8
        call dc_rect
        jmp .texto
.opcion:
        COLOR C_MARRON              ; la opción es un rombo
        mov eax, 7
        lea ebx, [edi+7]
        mov ecx, 7
        call dc_rombo
        COLOR C_BLANCO
        cmp dword [ebp+V_D1], 0
        je .o1
        COLOR C_OCRE
.o1:
        mov ecx, 5
        call dc_rombo
        cmp dword [ebp+V_D0], 0
        je .texto
        COLOR C_TERRACOTA
        mov ecx, 3
        call dc_rombo
.texto:
        COLOR C_TINTA
        test dword [ebp+V_ESTADO], S_INACTIVA
        jz .tx1
        COLOR C_ARENA_OSC
.tx1:
        mov eax, 22
        lea ebx, [edi-1]
        mov esi, [ebp+V_TEXTO]
        call dc_texto
        cmp ebp, [foco]
        jne .fin
        call cad_largo
        lea ecx, [eax*8+4]
        COLOR C_TERRACOTA
        mov eax, 20
        add ebx, 16
        mov edx, 1
        call dc_rect
.fin:
        call dc_cerrar
        xor eax, eax
        ret

; ---- texto fijo ----
est_proc:
        cmp eax, WM_PAINT
        jne ven_defecto
        call dc_pintar
        call ven_cli
        mov edi, ecx                ; ancho
        xor ebx, ebx
        mov dword [est_m], 0        ; margen
        test dword [ebp+V_ESTILO], SS_HUNDIDO
        jz .t0
        COLOR C_BLANCO
        xor eax, eax
        call dc_rect
        COLOR C_MARRON
        call dc_marco
        mov dword [est_m], 6
        lea ebx, [edx-16]
        sar ebx, 1
.t0:
        COLOR C_TINTA
        test dword [ebp+V_ESTILO], SS_NEGRITA
        setnz byte [gdi_negrita]
        mov esi, [ebp+V_TEXTO]
        test esi, esi
        jz .fin
.linea:
        xor ecx, ecx                ; largo de la línea
.l1:
        mov al, [esi+ecx]
        test al, al
        jz .l2
        cmp al, 10
        je .l2
        inc ecx
        jmp .l1
.l2:
        mov eax, [est_m]
        test dword [ebp+V_ESTILO], SS_DERECHA
        jz .l3
        lea eax, [ecx*8]
        neg eax
        add eax, edi
        sub eax, [est_m]
.l3:
        call dc_texto_n
        add esi, ecx
        cmp byte [esi], 0
        je .fin
        inc esi
        add ebx, 18
        jmp .linea
.fin:
        mov byte [gdi_negrita], 0
        call dc_cerrar
        xor eax, eax
        ret

; ---- barra de desplazamiento (vertical; la dibuja y la atiende su dueño) ----
; Para todas: EDX=alto de la barra, ESI=posición, EDI=máximo, ECX=página
; (interna) -> EAX=dónde empieza el pulgar (desde arriba), ECX=su largo
bd_pulgar:
        push ebx
        push edx
        push ebp
        lea ebp, [edx-2*BD_AN]      ; el carril
        mov eax, ebp
        imul eax, ecx
        lea ebx, [edi+ecx]
        xor edx, edx
        test ebx, ebx
        jz .p0
        div ebx
.p0:
        cmp eax, 16
        jae .p1
        mov eax, 16
.p1:
        cmp eax, ebp
        jbe .p2
        mov eax, ebp
.p2:
        mov ecx, eax                ; largo
        mov eax, ebp
        sub eax, ecx
        imul eax, esi
        xor edx, edx
        test edi, edi
        jz .p3
        div edi
.p3:
        add eax, BD_AN
        pop ebp
        pop edx
        pop ebx
        ret

; EAX=x EBX=y (en la ventana) y lo común: la dibuja
bd_pintar:
        pushad
        COLOR C_PIEDRA
        mov ecx, BD_AN
        call dc_rect
        COLOR C_ARENA
        mov edx, BD_AN
        call dc_rect                ; flecha de arriba
        COLOR C_MARRON
        call dc_marco
        mov edx, [esp+20]
        call dc_marco               ; el borde de toda la barra
        COLOR C_TINTA
        xor ebp, ebp
.fa:
        push eax
        push ebx
        add eax, 8
        sub eax, ebp
        lea ebx, [ebx+ebp+6]
        lea ecx, [ebp*2+1]
        mov edx, 1
        call dc_rect
        pop ebx
        pop eax
        inc ebp
        cmp ebp, 4
        jb .fa
        mov edx, [esp+20]
        push ebx
        lea ebx, [ebx+edx-BD_AN]    ; flecha de abajo
        COLOR C_ARENA
        mov ecx, BD_AN
        mov edx, BD_AN
        call dc_rect
        COLOR C_MARRON
        call dc_marco
        COLOR C_TINTA
        xor ebp, ebp
.fb:
        push eax
        push ebx
        lea eax, [eax+ebp+5]
        lea ebx, [ebx+ebp+6]
        mov ecx, 7
        sub ecx, ebp
        sub ecx, ebp
        mov edx, 1
        call dc_rect
        pop ebx
        pop eax
        inc ebp
        cmp ebp, 4
        jb .fb
        pop ebx
        test edi, edi
        jz .x
        mov edx, [esp+20]           ; el pulgar
        mov ecx, [esp+24]
        push eax
        call bd_pulgar
        add ebx, eax
        pop eax
        mov edx, ecx
        mov ecx, BD_AN
        COLOR C_ARENA
        call dc_rect
        COLOR C_MARRON
        call dc_marco
.x:
        popad
        ret

; EBX=y dentro de la barra y lo común -> EAX=1 una línea arriba, 2 una
; abajo, 3 una página arriba, 4 una abajo, 5 se ha agarrado el pulgar
bd_donde:
        push ecx
        mov eax, 1
        cmp ebx, BD_AN
        jl .x
        lea eax, [edx-BD_AN]
        cmp ebx, eax
        mov eax, 2
        jge .x
        call bd_pulgar
        cmp ebx, eax
        jge .d1
        mov eax, 3
        jmp .x
.d1:
        add ecx, eax
        cmp ebx, ecx
        jl .d2
        mov eax, 4
        jmp .x
.d2:
        sub eax, ebx
        neg eax
        mov [bd_agarre], eax
        mov eax, 5
.x:
        pop ecx
        ret

; EBX=y dentro de la barra (arrastrando el pulgar) y lo común -> EAX=posición
bd_pos_de:
        push ebx
        push ecx
        push edx
        push ebp
        lea ebp, [edx-2*BD_AN]
        call bd_pulgar
        sub ebp, ecx                ; recorrido del pulgar
        mov eax, ebx
        sub eax, [bd_agarre]
        sub eax, BD_AN
        jns .a
        xor eax, eax
.a:
        test ebp, ebp
        jle .cero
        imul eax, edi
        xor edx, edx
        div ebp
        cmp eax, edi
        jbe .x
        mov eax, edi
        jmp .x
.cero:
        xor eax, eax
.x:
        pop ebp
        pop edx
        pop ecx
        pop ebx
        ret

; ---- portapapeles ----
pp_poner:
        pushad
        cmp dword [pp_buf], 0
        jne .t
        push ecx
        mov ecx, PP_MAX
        call mem_pedir
        pop ecx
        jc .x
        mov [pp_buf], eax
.t:
        cmp ecx, PP_MAX
        jbe .c
        mov ecx, PP_MAX
.c:
        mov [pp_len], ecx
        mov edi, [pp_buf]
        rep movsb
.x:
        popad
        ret

; ---- cuadro de texto ----
; V_D0 búfer, V_D1 largo, V_D2 capacidad, V_D3 cursor, V_D4 el otro extremo
; de la selección, V_D5 primera línea a la vista, V_D6 primera columna,
; V_D7 bit 0 modificado, bit 1 arrastrando el pulgar, bit 2 seleccionando
ed_ini_linea:
        push esi
        mov esi, [ebp+V_D0]
.b:
        test eax, eax
        jz .x
        cmp byte [esi+eax-1], 10
        je .x
        dec eax
        jmp .b
.x:
        pop esi
        ret

ed_fin_linea:
        push esi
        mov esi, [ebp+V_D0]
.b:
        cmp eax, [ebp+V_D1]
        jae .x
        cmp byte [esi+eax], 10
        je .x
        inc eax
        jmp .b
.x:
        pop esi
        ret

; EAX=posición -> EAX=su número de línea
ed_num_linea:
        push esi
        push ecx
        push edx
        mov esi, [ebp+V_D0]
        mov ecx, eax
        xor eax, eax
        xor edx, edx
.b:
        cmp edx, ecx
        jae .x
        cmp byte [esi+edx], 10
        jne .s
        inc eax
.s:
        inc edx
        jmp .b
.x:
        pop edx
        pop ecx
        pop esi
        ret

; EAX=línea -> EAX=dónde empieza (el final del texto si no hay tantas)
ed_pos_linea:
        push esi
        push ecx
        mov esi, [ebp+V_D0]
        mov ecx, eax
        xor eax, eax
.b:
        test ecx, ecx
        jz .x
        cmp eax, [ebp+V_D1]
        jae .x
        cmp byte [esi+eax], 10
        jne .s
        dec ecx
.s:
        inc eax
        jmp .b
.x:
        pop ecx
        pop esi
        ret

; medidas: ed_an, ed_al, ed_cols, ed_filas, ed_tx, ed_ty
ed_geo:
        pushad
        call ven_cli
        mov [ed_an], ecx
        mov [ed_al], edx
        mov dword [ed_tx], 4
        test dword [ebp+V_ESTILO], ES_MULTI
        jz .una
        sub ecx, 8 + BD_AN
        jg .m1
        mov ecx, 8
.m1:
        shr ecx, 3
        mov [ed_cols], ecx
        sub edx, 6
        cmp edx, 16
        jge .m2
        mov edx, 16
.m2:
        shr edx, 4
        mov [ed_filas], edx
        mov dword [ed_ty], 3
        jmp .x
.una:
        sub ecx, 8
        jg .u1
        mov ecx, 8
.u1:
        shr ecx, 3
        mov [ed_cols], ecx
        mov dword [ed_filas], 1
        sub edx, 16
        sar edx, 1
        mov [ed_ty], edx
.x:
        popad
        ret

; quita del texto de EAX a EDX
ed_quitar:
        pushad
        mov ecx, [ebp+V_D1]
        sub ecx, edx
        mov esi, [ebp+V_D0]
        lea edi, [esi+eax]
        add esi, edx
        rep movsb
        sub edx, eax
        sub [ebp+V_D1], edx
        or dword [ebp+V_D7], 1
        popad
        ret

; quita lo seleccionado. CF=1 si había algo (cambia EAX y EDX)
ed_borrar_sel:
        mov eax, [ebp+V_D3]
        mov edx, [ebp+V_D4]
        cmp eax, edx
        je .no
        jb .o
        xchg eax, edx
.o:
        call ed_quitar
        mov [ebp+V_D3], eax
        mov [ebp+V_D4], eax
        stc
        ret
.no:
        clc
        ret

; (interna) AL=carácter que se quiere meter -> CF=1 si no vale
ed_filtro:
        cmp al, 13
        je .no
        test al, al
        jz .no
        cmp al, 10
        jne .t
        test dword [ebp+V_ESTILO], ES_MULTI
        jz .no
        clc
        ret
.t:
        cmp al, 9
        jne .si
        mov al, ' '
.si:
        clc
        ret
.no:
        stc
        ret

; mete en el cursor ECX octetos de ESI (en lugar de lo seleccionado)
ed_meter:
        pushad
        call ed_borrar_sel
        xor ebx, ebx                ; cuántos caben
        mov edx, [ebp+V_D2]
        sub edx, [ebp+V_D1]
        push esi
        push ecx
.c:
        test ecx, ecx
        jz .cf
        lodsb
        dec ecx
        call ed_filtro
        jc .c
        cmp ebx, edx
        jae .cf
        inc ebx
        jmp .c
.cf:
        pop ecx
        pop esi
        test ebx, ebx
        jz .x
        push esi                    ; hueco: lo de detrás, EBX más allá
        mov ecx, [ebp+V_D1]
        sub ecx, [ebp+V_D3]
        mov esi, [ebp+V_D0]
        add esi, [ebp+V_D1]
        dec esi
        lea edi, [esi+ebx]
        std
        rep movsb
        cld
        pop esi
        mov edi, [ebp+V_D0]
        add edi, [ebp+V_D3]
        mov edx, ebx
.p:
        test edx, edx
        jz .pf
        lodsb
        call ed_filtro
        jc .p
        stosb
        dec edx
        jmp .p
.pf:
        add [ebp+V_D1], ebx
        add ebx, [ebp+V_D3]
        mov [ebp+V_D3], ebx
        mov [ebp+V_D4], ebx
        or dword [ebp+V_D7], 1
.x:
        popad
        ret

ed_poner:
        pushad
        xor eax, eax
        mov [ebp+V_D1], eax
        mov [ebp+V_D3], eax
        mov [ebp+V_D4], eax
        call ed_meter
        xor eax, eax
        mov [ebp+V_D3], eax
        mov [ebp+V_D4], eax
        mov [ebp+V_D5], eax
        mov [ebp+V_D6], eax
        and dword [ebp+V_D7], ~1
        call ven_invalidar
        popad
        ret

ed_datos:
        mov esi, [ebp+V_D0]
        mov ecx, [ebp+V_D1]
        ret

ed_seleccionar:
        mov [ebp+V_D4], eax
        mov [ebp+V_D3], edx
        call ed_ver_cursor
        jmp ven_invalidar

; (interna) lo seleccionado, al portapapeles
ed_copiar:
        pushad
        mov eax, [ebp+V_D3]
        mov edx, [ebp+V_D4]
        cmp eax, edx
        je .x
        jb .o
        xchg eax, edx
.o:
        mov esi, [ebp+V_D0]
        add esi, eax
        mov ecx, edx
        sub ecx, eax
        call pp_poner
.x:
        popad
        ret

ed_pegar:
        pushad
        mov esi, [pp_buf]
        mov ecx, [pp_len]
        test esi, esi
        jz .x
        call ed_meter
.x:
        popad
        ret

; EAX=posición -> EAX=la misma columna de la línea de arriba / de abajo
ed_arriba:
        push ebx
        push ecx
        push edx
        mov ebx, eax
        call ed_ini_linea
        mov ecx, ebx
        sub ecx, eax
        test eax, eax
        jz .no
        lea edx, [eax-1]
        mov eax, edx
        call ed_ini_linea
        mov ebx, edx
        sub ebx, eax
        cmp ecx, ebx
        jbe .c
        mov ecx, ebx
.c:
        add eax, ecx
        jmp .x
.no:
        mov eax, ebx
.x:
        pop edx
        pop ecx
        pop ebx
        ret

ed_abajo:
        push ebx
        push ecx
        push edx
        mov ebx, eax
        call ed_ini_linea
        mov ecx, ebx
        sub ecx, eax
        mov eax, ebx
        call ed_fin_linea
        cmp eax, [ebp+V_D1]
        jae .no
        inc eax
        mov edx, eax
        call ed_fin_linea
        sub eax, edx
        cmp ecx, eax
        jbe .c
        mov ecx, eax
.c:
        lea eax, [edx+ecx]
        jmp .x
.no:
        mov eax, ebx
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; deja el cursor a la vista
ed_ver_cursor:
        pushad
        call ed_geo
        xor eax, eax
        test dword [ebp+V_ESTILO], ES_MULTI
        jz .a0
        mov eax, [ebp+V_D3]
        call ed_num_linea
.a0:
        cmp eax, [ebp+V_D5]
        jge .a
        mov [ebp+V_D5], eax
.a:
        mov ebx, [ebp+V_D5]
        add ebx, [ed_filas]
        cmp eax, ebx
        jl .b
        sub eax, [ed_filas]
        inc eax
        mov [ebp+V_D5], eax
.b:
        mov eax, [ebp+V_D3]
        mov ebx, eax
        call ed_ini_linea
        sub ebx, eax
        cmp ebx, [ebp+V_D6]
        jge .c
        mov [ebp+V_D6], ebx
.c:
        mov eax, [ebp+V_D6]
        add eax, [ed_cols]
        cmp ebx, eax
        jl .d
        mov eax, ebx
        sub eax, [ed_cols]
        inc eax
        mov [ebp+V_D6], eax
.d:
        popad
        ret

; (interna) lo común de la barra del cuadro: EAX=x EBX=y EDX=alto ESI=posición
; EDI=máximo ECX=página
ed_barra:
        call ed_geo
        mov eax, [ebp+V_D1]
        call ed_num_linea
        inc eax
        sub eax, [ed_filas]
        jns .a
        xor eax, eax
.a:
        mov esi, [ebp+V_D5]
        cmp eax, esi
        jae .b
        mov eax, esi
.b:
        mov edi, eax
        mov ecx, [ed_filas]
        mov eax, [ed_an]
        sub eax, BD_AN + 1
        mov ebx, 1
        mov edx, [ed_al]
        sub edx, 2
        ret

; (interna) EAX=x EBX=y en el cuadro -> EAX=posición del texto que cae ahí
ed_pos_de_xy:
        push ebx
        push ecx
        push edx
        call ed_geo
        sub ebx, [ed_ty]
        jns .a
        xor ebx, ebx
.a:
        shr ebx, 4
        add ebx, [ebp+V_D5]
        sub eax, [ed_tx]
        add eax, 4
        jns .b
        xor eax, eax
.b:
        shr eax, 3
        add eax, [ebp+V_D6]
        mov ecx, eax                ; columna
        mov eax, ebx
        call ed_pos_linea
        mov edx, eax
        call ed_fin_linea
        add edx, ecx
        cmp edx, eax
        jae .x
        mov eax, edx
.x:
        pop edx
        pop ecx
        pop ebx
        ret

ed_proc:
        cmp eax, WM_PAINT
        je ed_pintar
        cmp eax, WM_MOUSEWHEEL      ; [TAMAÑO] la rueda: tres líneas por paso
        je .rueda
        cmp eax, WM_CHAR
        je .car
        cmp eax, WM_KEYDOWN
        je ed_tecla
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_SETFOCUS
        je .inv
        cmp eax, WM_KILLFOCUS
        je .inv
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        cmp eax, WM_SETTEXT
        je .ptexto
        cmp eax, WM_GETTEXT
        je .ttexto
        cmp eax, WM_COPY
        je .copiar
        cmp eax, WM_CUT
        je .cortar
        cmp eax, WM_PASTE
        je .pegar
        cmp eax, EM_SEL_TODO
        je .todo
        cmp eax, EM_LARGO
        je .largo
        cmp eax, EM_MODIFICADO
        je .modif
        cmp eax, EM_PONER_MODIF
        jne ven_defecto
        and dword [ebp+V_D7], ~1
        and ecx, 1
        or [ebp+V_D7], ecx
        xor eax, eax
        ret
.modif:
        mov eax, [ebp+V_D7]
        and eax, 1
        ret
.largo:
        mov eax, [ebp+V_D1]
        ret
.crear:
        mov ecx, ED_MAX_UNA
        test dword [ebp+V_ESTILO], ES_MULTI
        jz .c1
        mov ecx, ED_MAX_MULTI
.c1:
        mov [ebp+V_D2], ecx
        add ecx, 4
        call mem_pedir
        mov [ebp+V_D0], eax
        test eax, eax
        jnz .c2
        mov dword [ebp+V_D2], 0
.c2:
        mov esi, [ebp+V_TEXTO]      ; texto inicial
        test esi, esi
        jz .nada
        call cad_largo
        mov ecx, eax
        call ed_poner
.nada:
        xor eax, eax
        ret
.destruir:
        mov eax, [ebp+V_D0]
        call mem_soltar
        xor eax, eax
        ret
.ptexto:
        mov esi, edx
        call cad_largo
        mov ecx, eax
        call ed_poner
        xor eax, eax
        ret
.ttexto:
        mov edi, edx
        mov esi, [ebp+V_D0]
        mov eax, [ebp+V_D1]
        dec ecx
        js .nada
        cmp eax, ecx
        jbe .t1
        mov eax, ecx
.t1:
        mov ecx, eax
        rep movsb
        mov byte [edi], 0
        ret
.todo:
        mov dword [ebp+V_D4], 0
        mov eax, [ebp+V_D1]
        mov [ebp+V_D3], eax
        jmp .inv
.copiar:
        call ed_copiar
        jmp .nada
.cortar:
        call ed_copiar
        call ed_borrar_sel
        jmp .cambio
.pegar:
        call ed_pegar
        jmp .cambio
.car:
        mov [ed_car], cl
        mov esi, ed_car
        mov ecx, 1
        call ed_meter
.cambio:
        call ed_ver_cursor
.inv:
        call ven_invalidar
        xor eax, eax
        ret
.rueda:
        test dword [ebp+V_ESTILO], ES_MULTI
        jz ven_defecto
        push ecx
        call ed_barra
        pop eax
        imul eax, 3
        add eax, [ebp+V_D5]
        jns .r1
        xor eax, eax
.r1:
        cmp eax, edi
        jle .r2
        mov eax, edi
.r2:
        mov [ebp+V_D5], eax
        jmp .inv
.pulsa:
        call raton_xy
        test dword [ebp+V_ESTILO], ES_MULTI
        jz .p1
        mov ecx, [ebp+V_AN]
        sub ecx, BD_AN + 1
        cmp eax, ecx
        jge .barra
.p1:
        call ed_pos_de_xy
        mov [ebp+V_D3], eax
        test byte [tec_mayus], TM_MAYUS
        jnz .p2
        mov [ebp+V_D4], eax
.p2:
        or dword [ebp+V_D7], 4
        jmp .inv
.barra:
        dec ebx                     ; (la barra empieza en y=1)
        push ebx
        call ed_barra
        pop ebx
        call bd_donde
        mov ebx, [ebp+V_D5]
        cmp eax, 1
        jne .b2
        dec ebx
.b2:
        cmp eax, 2
        jne .b3
        inc ebx
.b3:
        cmp eax, 3
        jne .b4
        sub ebx, ecx
.b4:
        cmp eax, 4
        jne .b5
        add ebx, ecx
.b5:
        cmp eax, 5
        jne .b6
        or dword [ebp+V_D7], 2
.b6:
        test ebx, ebx
        jns .b7
        xor ebx, ebx
.b7:
        cmp ebx, edi
        jle .b8
        mov ebx, edi
.b8:
        mov [ebp+V_D5], ebx
        jmp .inv
.mueve:
        test dword [ebp+V_D7], 2
        jnz .mpulgar
        test dword [ebp+V_D7], 4
        jz .nada
        call raton_xy
        call ed_pos_de_xy
        cmp eax, [ebp+V_D3]
        je .nada
        mov [ebp+V_D3], eax
        jmp .cambio
.mpulgar:
        call raton_xy
        dec ebx
        push ebx
        call ed_barra
        pop ebx
        call bd_pos_de
        cmp eax, [ebp+V_D5]
        je .nada
        mov [ebp+V_D5], eax
        jmp .inv
.suelta:
        and dword [ebp+V_D7], ~6
        xor eax, eax
        ret

ed_tecla:
        mov eax, [ebp+V_D3]
        cmp ecx, TC_IZQ
        je .izq
        cmp ecx, TC_DER
        je .der
        cmp ecx, TC_INICIO
        je .inicio
        cmp ecx, TC_FIN
        je .fin
        cmp ecx, TC_CTRL_INICIO
        je .cinicio
        cmp ecx, TC_CTRL_FIN
        je .cfin
        cmp ecx, TC_SUPR
        je .supr
        cmp ecx, TC_RETRO
        je .retro
        cmp ecx, TC_CTRL + 3
        je .copiar
        cmp ecx, TC_CTRL_INS
        je .copiar
        cmp ecx, TC_CTRL + 24
        je .cortar
        cmp ecx, TC_CTRL + 22
        je .pegar
        cmp ecx, TC_CTRL + 1
        je .todo
        cmp ecx, TC_INS
        je .ins
        test dword [ebp+V_ESTILO], ES_MULTI
        jz .defecto
        cmp ecx, TC_ARRIBA
        je .arriba
        cmp ecx, TC_ABAJO
        je .abajo
        cmp ecx, TC_REPAG
        je .repag
        cmp ecx, TC_AVPAG
        je .avpag
        cmp ecx, TC_INTRO
        jne .defecto
        mov ecx, 10
        mov eax, WM_CHAR
        jmp ed_proc
.defecto:
        mov eax, WM_KEYDOWN
        jmp ven_defecto
.izq:
        test eax, eax
        jz .mov
        dec eax
        jmp .mov
.der:
        cmp eax, [ebp+V_D1]
        jae .mov
        inc eax
        jmp .mov
.inicio:
        call ed_ini_linea
        jmp .mov
.fin:
        call ed_fin_linea
        jmp .mov
.cinicio:
        xor eax, eax
        jmp .mov
.cfin:
        mov eax, [ebp+V_D1]
        jmp .mov
.arriba:
        call ed_arriba
        jmp .mov
.abajo:
        call ed_abajo
        jmp .mov
.repag:
        call ed_geo
        mov ecx, [ed_filas]
.rp:
        call ed_arriba
        dec ecx
        jnz .rp
        jmp .mov
.avpag:
        call ed_geo
        mov ecx, [ed_filas]
.ap:
        call ed_abajo
        dec ecx
        jnz .ap
.mov:
        mov [ebp+V_D3], eax
        test dl, TM_MAYUS           ; con Mayús se va seleccionando
        jnz .listo
        mov [ebp+V_D4], eax
.listo:
        call ed_ver_cursor
        call ven_invalidar
        xor eax, eax
        ret
.supr:
        test dl, TM_MAYUS
        jnz .cortar
        call ed_borrar_sel
        jc .listo
        mov eax, [ebp+V_D3]
        cmp eax, [ebp+V_D1]
        jae .listo
        lea edx, [eax+1]
        call ed_quitar
        jmp .listo
.retro:
        call ed_borrar_sel
        jc .listo
        mov eax, [ebp+V_D3]
        test eax, eax
        jz .listo
        mov edx, eax
        dec eax
        call ed_quitar
        mov [ebp+V_D3], eax
        mov [ebp+V_D4], eax
        jmp .listo
.ins:
        test dl, TM_MAYUS
        jz .defecto
.pegar:
        call ed_pegar
        jmp .listo
.copiar:
        call ed_copiar
        jmp .listo
.cortar:
        call ed_copiar
        call ed_borrar_sel
        jmp .listo
.todo:
        mov dword [ebp+V_D4], 0
        mov eax, [ebp+V_D1]
        mov [ebp+V_D3], eax
        jmp .listo

ed_pintar:
        call dc_pintar
        call ed_geo
        COLOR C_MARRON
        xor eax, eax
        xor ebx, ebx
        mov ecx, [ed_an]
        mov edx, [ed_al]
        call dc_marco
        test dword [ebp+V_ESTILO], ES_MULTI
        jz .sinbarra
        call ed_barra
        call bd_pintar
.sinbarra:
        mov eax, [ed_tx]
        dec eax
        mov ebx, 1
        mov ecx, [ed_cols]
        lea ecx, [ecx*8+2]
        mov edx, [ed_al]
        sub edx, 2
        call dc_cortar
        mov eax, [ebp+V_D3]         ; lo seleccionado: de ed_s0 a ed_s1
        mov edx, [ebp+V_D4]
        cmp eax, edx
        jbe .o
        xchg eax, edx
.o:
        mov [ed_s0], eax
        mov [ed_s1], edx
        mov eax, [ebp+V_D5]
        call ed_pos_linea
        mov [ed_p], eax
        mov dword [ed_fila], 0
.fila:
        mov eax, [ed_fila]
        cmp eax, [ed_filas]
        jae .fin
        shl eax, 4
        add eax, [ed_ty]
        mov [ed_y], eax
        mov eax, [ed_p]
        call ed_fin_linea
        mov edi, eax                ; fin de la línea
        mov esi, [ed_p]
        add esi, [ebp+V_D6]
        mov [ed_pri], esi           ; primera posición a la vista
        mov ecx, edi
        sub ecx, esi
        jle .sel
        cmp ecx, [ed_cols]
        jle .t1
        mov ecx, [ed_cols]
.t1:
        COLOR C_TINTA
        add esi, [ebp+V_D0]
        mov eax, [ed_tx]
        mov ebx, [ed_y]
        call dc_texto_n
.sel:
        mov eax, [ed_s0]
        cmp eax, [ed_pri]
        jge .s1
        mov eax, [ed_pri]
.s1:
        mov edx, [ed_s1]
        lea ecx, [edi+1]            ; (el salto de línea también se marca)
        cmp edx, ecx
        jle .s2
        mov edx, ecx
.s2:
        mov ecx, [ed_pri]
        add ecx, [ed_cols]
        cmp edx, ecx
        jle .s3
        mov edx, ecx
.s3:
        cmp eax, edx
        jge .cursor
        mov [ed_a], eax
        mov [ed_b], edx
        mov ecx, edx
        sub ecx, eax
        shl ecx, 3
        sub eax, [ed_pri]
        shl eax, 3
        add eax, [ed_tx]
        mov ebx, [ed_y]
        mov edx, 16
        COLOR C_ANIL
        call dc_rect
        mov ecx, [ed_b]             ; y su texto, en blanco
        cmp ecx, edi
        jle .s4
        mov ecx, edi
.s4:
        sub ecx, [ed_a]
        jle .cursor
        mov esi, [ebp+V_D0]
        add esi, [ed_a]
        COLOR C_BLANCO
        call dc_texto_n
.cursor:
        cmp ebp, [foco]
        jne .sig
        mov eax, [ebp+V_D3]
        cmp eax, [ed_p]
        jb .sig
        cmp eax, edi
        ja .sig
        sub eax, [ed_pri]
        js .sig
        cmp eax, [ed_cols]
        jg .sig
        shl eax, 3
        add eax, [ed_tx]
        dec eax
        mov ebx, [ed_y]
        mov ecx, 2
        mov edx, 16
        COLOR C_TERRACOTA
        call dc_rect
.sig:
        cmp edi, [ebp+V_D1]
        jae .fin
        inc edi
        mov [ed_p], edi
        inc dword [ed_fila]
        jmp .fila
.fin:
        call dc_cerrar
        xor eax, eax
        ret

; ---- lista ----
; V_D0 textos (LISTA_TXT octetos cada uno), V_D1 cuántos, V_D2 el elegido,
; V_D3 el primero a la vista, V_D4 arrastrando el pulgar
; (interna) -> ECX=filas a la vista; lo común de la barra: EAX=x EBX=y
; EDX=alto ESI=posición EDI=máximo (0: no hace falta barra)
lis_barra:
        call ven_cli
        mov eax, ecx
        sub eax, BD_AN + 1
        lea ecx, [edx-4]
        shr ecx, 4
        mov edi, [ebp+V_D1]
        sub edi, ecx
        jns .a
        xor edi, edi
.a:
        mov esi, [ebp+V_D3]
        cmp esi, edi
        jbe .b
        mov esi, edi
        mov [ebp+V_D3], esi
.b:
        mov ebx, 1
        sub edx, 2
        ret

; (interna) deja el elegido a la vista
lis_ver:
        pushad
        call lis_barra
        mov eax, [ebp+V_D2]
        test eax, eax
        js .x
        cmp eax, [ebp+V_D3]
        jge .a
        mov [ebp+V_D3], eax
.a:
        mov ebx, [ebp+V_D3]
        add ebx, ecx
        cmp eax, ebx
        jl .x
        sub eax, ecx
        inc eax
        mov [ebp+V_D3], eax
.x:
        popad
        ret

lis_proc:
        cmp eax, WM_PAINT
        je lis_pintar
        cmp eax, WM_MOUSEWHEEL      ; [TAMAÑO] la rueda: tres filas por paso
        je .rueda
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_SETFOCUS
        je .inv
        cmp eax, WM_KILLFOCUS
        je .inv
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        cmp eax, LM_ANADIR
        je .anadir
        cmp eax, LM_VACIAR
        je .vaciar
        cmp eax, LM_SELEC
        je .selec
        cmp eax, LM_PONER_SELEC
        je .psel
        cmp eax, LM_TEXTO
        je .texto
        cmp eax, LM_CUENTA
        jne ven_defecto
        mov eax, [ebp+V_D1]
        ret
.selec:
        mov eax, [ebp+V_D2]
        ret
.texto:
        mov eax, ecx
        imul eax, LISTA_TXT
        add eax, [ebp+V_D0]
        ret
.crear:
        mov ecx, LISTA_MAX * LISTA_TXT
        call mem_pedir
        mov [ebp+V_D0], eax
        mov dword [ebp+V_D2], -1
        xor eax, eax
        ret
.destruir:
        mov eax, [ebp+V_D0]
        call mem_soltar
        xor eax, eax
        ret
.vaciar:
        xor eax, eax
        mov [ebp+V_D1], eax
        mov [ebp+V_D3], eax
        mov dword [ebp+V_D2], -1
        jmp .inv
.anadir:
        mov eax, [ebp+V_D1]
        cmp eax, LISTA_MAX
        jae .nada
        cmp dword [ebp+V_D0], 0
        je .nada
        imul edi, eax, LISTA_TXT
        add edi, [ebp+V_D0]
        mov esi, edx
        mov ecx, LISTA_TXT - 1
.a1:
        lodsb
        test al, al
        jz .a2
        stosb
        dec ecx
        jnz .a1
.a2:
        mov byte [edi], 0
        inc dword [ebp+V_D1]
        call ven_invalidar
        mov eax, [ebp+V_D1]
        dec eax
        ret
.psel:
        mov [ebp+V_D2], ecx
        call lis_ver
.inv:
        call ven_invalidar
.nada:
        xor eax, eax
        ret
.pulsa:
        push eax
        call raton_xy
        mov [lis_x], eax
        mov [lis_y], ebx
        call lis_barra
        test edi, edi
        jz .item
        cmp [lis_x], eax
        jl .item
        pop eax
        mov ebx, [lis_y]
        dec ebx
        call bd_donde
        mov ebx, [ebp+V_D3]
        cmp eax, 1
        jne .b2
        dec ebx
.b2:
        cmp eax, 2
        jne .b3
        inc ebx
.b3:
        cmp eax, 3
        jne .b4
        sub ebx, ecx
.b4:
        cmp eax, 4
        jne .b5
        add ebx, ecx
.b5:
        cmp eax, 5
        jne .b6
        mov dword [ebp+V_D4], 1
.b6:
        test ebx, ebx
        jns .b7
        xor ebx, ebx
.b7:
        cmp ebx, edi
        jle .b8
        mov ebx, edi
.b8:
        mov [ebp+V_D3], ebx
        jmp .inv
.item:
        mov eax, [lis_y]
        sub eax, 2
        js .i9
        shr eax, 4
        add eax, [ebp+V_D3]
        cmp eax, [ebp+V_D1]
        jae .i9
        mov [ebp+V_D2], eax
        call ven_invalidar
        mov eax, LN_SELEC
        call avisar
        pop eax
        cmp eax, WM_LBUTTONDBLCLK
        jne .nada
        mov eax, LN_DOBLE
        call avisar
        jmp .nada
.i9:
        pop eax
        jmp .nada
.mueve:
        cmp dword [ebp+V_D4], 0
        je .nada
        call raton_xy
        mov [lis_y], ebx
        call lis_barra
        mov ebx, [lis_y]
        dec ebx
        call bd_pos_de
        cmp eax, [ebp+V_D3]
        je .nada
        mov [ebp+V_D3], eax
        jmp .inv
.suelta:
        mov dword [ebp+V_D4], 0
        jmp .nada
.rueda:
        push ecx
        call lis_barra
        pop eax
        imul eax, 3
        add eax, [ebp+V_D3]
        jns .r1
        xor eax, eax
.r1:
        cmp eax, edi
        jle .r2
        mov eax, edi
.r2:
        mov [ebp+V_D3], eax
        jmp .inv
.tecla:
        mov eax, [ebp+V_D2]
        mov edx, [ebp+V_D1]
        test edx, edx
        jz .defecto
        cmp ecx, TC_ARRIBA
        jne .t1
        dec eax
        jmp .tm
.t1:
        cmp ecx, TC_ABAJO
        jne .t2
        inc eax
        jmp .tm
.t2:
        cmp ecx, TC_INICIO
        jne .t3
        xor eax, eax
        jmp .tm
.t3:
        cmp ecx, TC_FIN
        jne .t4
        lea eax, [edx-1]
        jmp .tm
.t4:
        cmp ecx, TC_REPAG
        jne .t5
        sub eax, 8
        jmp .tm
.t5:
        cmp ecx, TC_AVPAG
        jne .t6
        add eax, 8
        jmp .tm
.t6:
        cmp ecx, TC_INTRO
        jne .defecto
        test eax, eax
        js .defecto
        mov eax, LN_DOBLE
        call avisar
        jmp .nada
.defecto:
        mov eax, WM_KEYDOWN
        jmp ven_defecto
.tm:
        test eax, eax
        jns .tm1
        xor eax, eax
.tm1:
        cmp eax, edx
        jl .tm2
        lea eax, [edx-1]
.tm2:
        cmp eax, [ebp+V_D2]
        je .nada
        mov [ebp+V_D2], eax
        call lis_ver
        call ven_invalidar
        mov eax, LN_SELEC
        call avisar
        jmp .nada

lis_pintar:
        call dc_pintar
        call ven_cli
        mov [lis_x], ecx            ; ancho
        COLOR C_MARRON
        xor eax, eax
        xor ebx, ebx
        call dc_marco
        call lis_barra
        mov [lis_y], ecx            ; filas
        test edi, edi
        jz .sinbarra
        mov ecx, [lis_y]
        call bd_pintar
        sub dword [lis_x], BD_AN
.sinbarra:
        mov eax, 2
        mov ebx, 2
        mov ecx, [lis_x]
        sub ecx, 4
        mov edx, [lis_y]
        shl edx, 4
        call dc_cortar
        mov edi, [ebp+V_D3]         ; el que se pinta
        mov ebx, 2
.f:
        cmp edi, [ebp+V_D1]
        jae .fin
        mov eax, edi
        sub eax, [ebp+V_D3]
        cmp eax, [lis_y]
        jae .fin
        COLOR C_TINTA
        cmp edi, [ebp+V_D2]
        jne .f2
        COLOR C_ARENA_OSC
        cmp ebp, [foco]
        jne .f1
        COLOR C_ANIL
.f1:
        mov eax, 2
        mov ecx, [lis_x]
        mov edx, LISTA_FILA
        call dc_rect
        COLOR C_BLANCO
.f2:
        imul esi, edi, LISTA_TXT
        add esi, [ebp+V_D0]
        mov eax, 6
        call dc_texto
        add ebx, LISTA_FILA
        inc edi
        jmp .f
.fin:
        call dc_cerrar
        xor eax, eax
        ret
