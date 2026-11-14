; =============================================================================
;  ARCHIVOS.HVP - la lista: lo que hay en la carpeta (leer, ordenar, elegir)
;  y su ventana (vista Iconos y vista Detalles, ratón, teclado, arrastrar)
; =============================================================================

; ESI=carpeta: lee lo que hay (sin «.» ni «..» ni la etiqueta del disco) en
; ents -> CF=1 y EAX=error si no se puede (antes de tocar la lista, si es en
; el primero)
leer_carpeta:
        pushad
        mov edi, tmp2
        mov ebx, t_patron
        call ruta_unir
        mov esi, tmp2
        mov ecx, 0x16
        mov edi, dta_l
        call d_primero
        jnc .hay
        cmp eax, 0x12               ; (sin nada: vacía)
        je .vacia
        cmp eax, 2
        je .vacia
        mov [esp+28], eax
        popad
        stc
        ret
.vacia:
        mov dword [n_ents], 0
        mov dword [lista_rota], 0
        popad
        clc
        ret
.hay:
        mov dword [lista_rota], 1
        xor ebx, ebx                ; cuántas
        mov edi, ents
.e:
        mov al, [dta_l+DTA_ATR]
        test al, 8                  ; la etiqueta del disco, no
        jnz .sig
        cmp byte [dta_l+DTA_NOMBRE], '.'
        je .sig
        cmp ebx, MAX_ENT
        jae .fin
        push edi
        mov esi, dta_l + DTA_NOMBRE
        mov ecx, 12
        call copiar_n               ; E_NOM (13)
        mov [edi+E_ATR], al
        mov byte [edi+E_MARCA], 0
        mov eax, [dta_l+DTA_TAMANO]
        mov [edi+E_TAM], eax
        mov eax, [dta_l+DTA_HORA]   ; (hora y fecha)
        mov [edi+E_HORA], eax
        mov dword [edi+E_DIR], 0
        call tipo_poner
        pop edi
        add edi, ENT_TAM
        inc ebx
.sig:
        push edi
        mov edi, dta_l
        call d_siguiente
        pop edi
        jnc .e
.fin:
        mov [n_ents], ebx
        mov dword [lista_rota], 0
        popad
        clc
        ret

; EDI=entrada (con el nombre y los atributos): su tipo
tipo_poner:
        pushad
        mov byte [edi+E_TIPO], TI_CARP
        test byte [edi+E_ATR], 0x10
        jnz .x
        mov byte [edi+E_TIPO], TI_DESC
        lea esi, [edi+E_NOM]        ; la extensión: detrás del punto
.p:
        lodsb
        test al, al
        jz .x
        cmp al, '.'
        jne .p
        mov ebx, tab_ext
.t:
        cmp byte [ebx], 0
        je .x
        xor ecx, ecx
.c:
        mov al, [ebx+ecx]
        mov ah, [esi+ecx]
        test al, al                 ; (extensión de dos letras: «ME», «MD»)
        jz .c2
        cmp al, ah
        jne .s
        inc ecx
        cmp ecx, 3
        jb .c
        cmp byte [esi+3], 0
        jne .s
        jmp .si
.c2:
        test ah, ah
        jnz .s
.si:
        mov al, [ebx+3]
        mov [edi+E_TIPO], al
        jmp .x
.s:
        add ebx, 4
        jmp .t
.x:
        popad
        ret

; =============================================================================
;  Ordenar (las carpetas, siempre delante)
; =============================================================================
; (interna) ESI, EDI: dos entradas -> EAX <0, 0 o >0
ent_cmp:
        push ebx
        push ecx
        push edx
        mov al, [esi+E_ATR]
        mov ah, [edi+E_ATR]
        and ax, 0x1010
        cmp al, ah
        je .misma
        mov eax, -1                 ; la carpeta, delante
        test byte [esi+E_ATR], 0x10
        jnz .x
        mov eax, 1
        jmp .x
.misma:
        mov ecx, [orden]
        cmp ecx, 1
        jne .o2
        test byte [esi+E_ATR], 0x10 ; (las carpetas, por el nombre)
        jnz .nombre
        mov eax, [esi+E_TAM]
        cmp eax, [edi+E_TAM]
        jmp .comp
.o2:
        cmp ecx, 2
        jne .o3
        push esi
        push edi
        call .ext
        mov ebx, eax
        mov esi, edi
        call .ext
        mov esi, ebx
        mov edi, eax
        call cad_cmp
        pop edi
        pop esi
        test eax, eax
        jnz .inv
        jmp .nombre
.o3:
        cmp ecx, 3
        jne .nombre
        mov eax, [esi+E_HORA]
        rol eax, 16                 ; fecha:hora
        mov edx, [edi+E_HORA]
        rol edx, 16
        cmp eax, edx
.comp:
        mov eax, 0
        je .nombre
        mov eax, -1
        jb .inv
        mov eax, 1
        jmp .inv
.nombre:
        call cad_cmp
.inv:
        cmp dword [inverso], 0
        je .x
        neg eax
.x:
        pop edx
        pop ecx
        pop ebx
        ret
.ext:                               ; ESI=entrada -> EAX=su extensión
        lea eax, [esi+E_NOM]
.e1:
        cmp byte [eax], 0
        je .e2
        cmp byte [eax], '.'
        je .e3
        inc eax
        jmp .e1
.e3:
        inc eax
.e2:
        ret

; ESI, EDI: dos textos -> EAX <0, 0 o >0
cad_cmp:
        push ecx
        push esi
        push edi
.c:
        movzx eax, byte [esi]
        movzx ecx, byte [edi]
        sub eax, ecx
        jnz .x
        test ecx, ecx
        jz .x
        inc esi
        inc edi
        jmp .c
.x:
        pop edi
        pop esi
        pop ecx
        ret

; ordena ents (Shell)
ordenar:
        pushad
        mov ebx, [n_ents]
        mov ebp, ebx                ; el salto
.h:
        shr ebp, 1
        test ebp, ebp
        jz .x
        mov ecx, ebp                ; i
.i:
        cmp ecx, ebx
        jae .h
        mov esi, ecx                ; la que se coloca, aparte
        shl esi, 5
        add esi, ents
        mov edi, ent_aux
        push ecx
        mov ecx, 8
        rep movsd
        pop ecx
        mov edx, ecx                ; j
.j:
        cmp edx, ebp
        jb .pon
        mov esi, ent_aux
        mov edi, edx
        sub edi, ebp
        shl edi, 5
        add edi, ents
        call ent_cmp
        test eax, eax
        jns .pon
        mov esi, edi                ; ents[j] = ents[j-salto]
        mov edi, edx
        shl edi, 5
        add edi, ents
        push ecx
        mov ecx, 8
        rep movsd
        pop ecx
        sub edx, ebp
        jmp .j
.pon:
        mov edi, edx
        shl edi, 5
        add edi, ents
        mov esi, ent_aux
        push ecx
        mov ecx, 8
        rep movsd
        pop ecx
        inc ecx
        jmp .i
.x:
        popad
        ret

; ordena otra vez y deja el foco donde estaba
ordenar_conservar:
        pushad
        mov eax, [foco_i]
        mov byte [tmp], 0
        cmp eax, [n_ents]
        jae .o
        shl eax, 5
        lea esi, [ents+eax+E_NOM]
        mov edi, tmp
        mov ecx, 12
        call copiar_n
.o:
        call ordenar
        mov esi, tmp
        call lista_buscar_nombre
        jc .x
        mov [foco_i], eax
        mov [ancla], eax
.x:
        call lista_ver_foco
        call lista_inv
        popad
        ret

; ESI=nombre -> EAX=su índice (CF=1 si no está)
lista_buscar_nombre:
        push ecx
        push edi
        xor ecx, ecx
.b:
        cmp ecx, [n_ents]
        jae .no
        mov edi, ecx
        shl edi, 5
        add edi, ents + E_NOM
        call igual
        je .si
        inc ecx
        jmp .b
.si:
        mov eax, ecx
        pop edi
        pop ecx
        clc
        ret
.no:
        pop edi
        pop ecx
        stc
        ret

; ESI=nombre: lo marca (solo él) y lo pone a la vista
lista_marcar_nombre:
        pushad
        call lista_buscar_nombre
        jc .x
        call sel_uno
        call lista_ver_foco
        call lista_inv
        call estado_poner
.x:
        popad
        ret

; vuelve a leer la carpeta: conserva lo elegido y el foco (por los nombres)
refrescar:
        pushad
        cmp dword [modo_busca], 0
        jne .x
        ; los nombres elegidos, a sel_nombres
        mov edi, sel_nombres
        xor ecx, ecx
        xor edx, edx
.g:
        cmp ecx, [n_ents]
        jae .g2
        mov esi, ecx
        shl esi, 5
        add esi, ents
        test byte [esi+E_MARCA], 1
        jz .gs
        cmp edx, 64
        jae .gs
        push ecx
        mov ecx, 12
        call copiar_n
        pop ecx
        add edi, 13
        inc edx
.gs:
        inc ecx
        jmp .g
.g2:
        mov [n_selnom], edx
        mov eax, [foco_i]
        mov byte [tmp], 0
        cmp eax, [n_ents]
        jae .f
        shl eax, 5
        lea esi, [ents+eax+E_NOM]
        mov edi, tmp
        mov ecx, 12
        call copiar_n
.f:
        call crit_olvidar
        mov esi, ruta
        call leer_carpeta
        jnc .l
        mov dword [n_ents], 0
.l:
        call ordenar
        mov esi, sel_nombres
        mov ecx, [n_selnom]
.s:
        test ecx, ecx
        jz .s2
        call lista_buscar_nombre
        jc .s1
        shl eax, 5
        or byte [ents+eax+E_MARCA], 1
.s1:
        add esi, 13
        dec ecx
        jmp .s
.s2:
        mov dword [foco_i], 0
        mov esi, tmp
        call lista_buscar_nombre
        jc .s3
        mov [foco_i], eax
        mov [ancla], eax
.s3:
        call pp_marcar_cortados
        call libre_mirar
        call lista_medidas
        call lista_inv
        call estado_poner
.x:
        popad
        ret

; =============================================================================
;  Lo elegido
; =============================================================================
; EAX=índice: solo él, elegido, y el foco ahí
sel_uno:
        pushad
        mov ecx, [n_ents]
        mov edi, ents
.l:
        test ecx, ecx
        jz .f
        and byte [edi+E_MARCA], ~1
        add edi, ENT_TAM
        dec ecx
        jmp .l
.f:
        cmp eax, [n_ents]
        jae .x
        mov [foco_i], eax
        mov [ancla], eax
        shl eax, 5
        or byte [ents+eax+E_MARCA], 1
.x:
        popad
        ret

; EAX=0 ninguno, 1 todos, 2 al revés
sel_todos:
        pushad
        mov ecx, [n_ents]
        mov edi, ents
.l:
        test ecx, ecx
        jz .f
        cmp eax, 1
        je .t
        jb .n
        xor byte [edi+E_MARCA], 1
        jmp .s
.t:
        or byte [edi+E_MARCA], 1
        jmp .s
.n:
        and byte [edi+E_MARCA], ~1
.s:
        add edi, ENT_TAM
        dec ecx
        jmp .l
.f:
        call lista_inv
        call estado_poner
        popad
        ret

; elige de [ancla] a EAX (y el foco en EAX); lo demás, no
sel_rango:
        pushad
        cmp eax, [n_ents]
        jae .x
        mov [foco_i], eax
        mov ebx, [ancla]
        cmp ebx, [n_ents]
        jb .a
        mov ebx, eax
        mov [ancla], eax
.a:
        cmp ebx, eax
        jbe .o
        xchg ebx, eax
.o:
        xor ecx, ecx
        mov edi, ents
.l:
        cmp ecx, [n_ents]
        jae .x
        and byte [edi+E_MARCA], ~1
        cmp ecx, ebx
        jb .s
        cmp ecx, eax
        ja .s
        or byte [edi+E_MARCA], 1
.s:
        add edi, ENT_TAM
        inc ecx
        jmp .l
.x:
        popad
        ret

; -> EAX = cuántos están elegidos; EDX = el primero (-1 si ninguno)
n_elegidos:
        push ecx
        push esi
        xor eax, eax
        or edx, -1
        xor ecx, ecx
        mov esi, ents
.l:
        cmp ecx, [n_ents]
        jae .x
        test byte [esi+E_MARCA], 1
        jz .s
        inc eax
        test edx, edx
        jns .s
        mov edx, ecx
.s:
        add esi, ENT_TAM
        inc ecx
        jmp .l
.x:
        pop esi
        pop ecx
        ret

; EAX=índice EDI=destino: su ruta completa
ent_ruta:
        push ebx
        push esi
        push eax
        shl eax, 5
        lea ebx, [ents+eax+E_NOM]
        mov esi, [ents+eax+E_DIR]
        test esi, esi
        jnz .d
        mov esi, ruta
.d:
        call ruta_unir
        pop eax
        pop esi
        pop ebx
        ret

; el texto de la barra de estado
estado_poner:
        pushad
        mov edi, st_izq
        call n_elegidos
        test eax, eax
        jnz .sel
        mov eax, [n_ents]
        call pon_miles
        mov esi, t_objetos
        cmp eax, 1
        jne .o
        mov esi, t_objeto
.o:
        call hvl_cad
        ; y lo que ocupan los archivos
        xor ecx, ecx
        xor edx, edx
        mov esi, ents
.t:
        cmp ecx, [n_ents]
        jae .t2
        add edx, [esi+E_TAM]
        add esi, ENT_TAM
        inc ecx
        jmp .t
.t2:
        jmp .tam
.sel:
        push eax
        call pon_miles
        mov esi, t_selecc
        cmp eax, 1
        jne .s1
        mov esi, t_selec1
.s1:
        call hvl_cad
        pop eax
        xor ecx, ecx
        xor edx, edx
        mov esi, ents
.u:
        cmp ecx, [n_ents]
        jae .u2
        test byte [esi+E_MARCA], 1
        jz .u1
        add edx, [esi+E_TAM]
.u1:
        add esi, ENT_TAM
        inc ecx
        jmp .u
.u2:
        cmp dword [modo_busca], 0   ; (de una búsqueda: dónde está el del foco)
        je .tam
        mov eax, [foco_i]
        cmp eax, [n_ents]
        jae .tam
        mov esi, t_coma_sp
        call hvl_cad
        shl eax, 5
        mov esi, [ents+eax+E_DIR]
        test esi, esi
        jz .tam
        call hvl_cad
        jmp .fin
.tam:
        mov esi, t_coma_sp
        call hvl_cad
        mov eax, edx
        call pon_miles
        mov esi, t_octetos
        call hvl_cad
.fin:
        mov byte [edi], 0
        call estado_repintar
        popad
        ret

; =============================================================================
;  La ventana de la lista
; =============================================================================
; las medidas: columnas, filas y la barra de desplazamiento
lista_medidas:
        pushad
        mov ebp, [v_lista]
        test ebp, ebp
        jz .x
        HV ven_cli
        mov [l_an], ecx
        mov [l_al], edx
        xor ebx, ebx                ; arriba: la cabecera de las columnas
        cmp dword [vista], 0
        je .v
        mov ebx, CAB_D
.v:
        mov [l_cab], ebx
        sub edx, ebx
        jns .v1
        xor edx, edx
.v1:
        mov [l_pag], edx            ; lo que se ve, de alto
        mov dword [l_barra], 0
        call .cuenta
        cmp eax, [l_pag]
        jle .cabe
        mov dword [l_barra], 1
        call .cuenta
.cabe:
        mov [l_todo], eax
        sub eax, [l_pag]            ; lo más que se desplaza
        jns .m
        xor eax, eax
.m:
        mov [l_max], eax
        cmp [desp_l], eax
        jle .d
        mov [desp_l], eax
.d:
        cmp dword [desp_l], 0
        jge .d1
        mov dword [desp_l], 0
.d1:
        mov ebp, [v_barra_l]
        cmp dword [l_barra], 0
        je .sinb
        mov eax, [l_an]
        sub eax, BD_AN
        mov ebx, [l_cab]
        mov ecx, BD_AN
        mov edx, [l_pag]
        HV ven_mover
        mov eax, BDM_PONER
        mov ecx, [l_max]
        mov edx, [l_pag]
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [desp_l]
        HV msg_enviar
        mov eax, 1
        HV ven_mostrar
        jmp .x
.sinb:
        xor eax, eax
        HV ven_mostrar
.x:
        popad
        ret
.cuenta:                            ; -> EAX = el alto de todo; [l_cols]
        mov ecx, [l_an]
        cmp dword [l_barra], 0
        je .c1
        sub ecx, BD_AN
.c1:
        mov [l_ancho], ecx
        cmp dword [vista], 0
        jne .det
        mov eax, ecx
        xor edx, edx
        mov ecx, CEL_AN
        div ecx
        cmp eax, 1
        jae .c2
        mov eax, 1
.c2:
        mov [l_cols], eax
        mov ecx, eax
        mov eax, [n_ents]
        add eax, ecx
        dec eax
        xor edx, edx
        div ecx
        imul eax, CEL_AL
        ret
.det:
        mov dword [l_cols], 1
        mov eax, [n_ents]
        imul eax, FILA_D
        ret

lista_inv:
        push ebp
        mov ebp, [v_lista]
        test ebp, ebp
        jz .x
        HV ven_invalidar
.x:
        pop ebp
        ret

; EAX=índice -> EAX=x EBX=y (en la ventana) de su casilla o fila
lista_xy:
        push ecx
        push edx
        cmp dword [vista], 0
        jne .det
        xor edx, edx
        div dword [l_cols]
        imul ebx, eax, CEL_AL
        imul eax, edx, CEL_AN
        jmp .y
.det:
        imul ebx, eax, FILA_D
        xor eax, eax
.y:
        sub ebx, [desp_l]
        add ebx, [l_cab]
        pop edx
        pop ecx
        ret

; EDX=x,y (de la ventana) -> EAX=índice (-1: el fondo; -2: la cabecera, con
; ECX=la columna)
lista_en:
        push ebx
        push edx
        movsx ebx, dx               ; x
        sar edx, 16                 ; y
        cmp edx, [l_cab]
        jge .z
        mov eax, ebx
        call col_en
        mov ecx, eax
        mov eax, -2
        jmp .x
.z:
        sub edx, [l_cab]
        add edx, [desp_l]
        cmp ebx, [l_ancho]
        jge .no
        test ebx, ebx
        js .no
        cmp dword [vista], 0
        jne .det
        mov eax, edx
        xor edx, edx
        push ecx
        mov ecx, CEL_AL
        div ecx
        imul eax, [l_cols]
        push eax
        mov eax, ebx
        xor edx, edx
        mov ecx, CEL_AN
        div ecx
        cmp eax, [l_cols]
        pop edx
        pop ecx
        jae .no
        add eax, edx
        jmp .hay
.det:
        mov eax, edx
        xor edx, edx
        push ecx
        mov ecx, FILA_D
        div ecx
        pop ecx
.hay:
        cmp eax, [n_ents]
        jb .x
.no:
        mov eax, -1
.x:
        pop edx
        pop ebx
        ret

; las columnas de la vista Detalles: dónde empieza cada una
; -> col_x: nombre, tamaño (su borde derecho), tipo, fecha, fin
col_medir:
        pushad
        mov ebx, 100                ; el tipo (o, de una búsqueda, la carpeta)
        mov ecx, 104                ; y lo menos que se le deja al nombre
        cmp dword [modo_busca], 0
        je .b
        mov ebx, 132
        mov ecx, 120
.b:
        mov edx, 136                ; la fecha con la hora, si cabe
        mov dword [col_hora], 1
        mov eax, [l_ancho]
        sub eax, 84
        sub eax, ebx
        sub eax, edx
        cmp eax, 120
        jge .n
        mov edx, 92                 ; si no, sin la hora
        mov dword [col_hora], 0
        add eax, 136 - 92
.n:
        cmp eax, ecx
        jge .n1
        mov eax, ecx
.n1:
        mov dword [col_x], 0
        mov [col_x+4], eax          ; tamaño
        add eax, 84
        mov [col_x+8], eax          ; tipo (o carpeta)
        add eax, ebx
        mov [col_x+12], eax         ; fecha
        add eax, edx
        mov [col_x+16], eax
        popad
        ret
; EAX=x -> EAX=columna (0-3)
col_en:
        push ecx
        call col_medir
        mov ecx, 3
.b:
        cmp eax, [col_x+ecx*4]
        jge .x
        dec ecx
        jnz .b
.x:
        mov eax, ecx
        pop ecx
        ret

; que el foco se vea
lista_ver_foco:
        pushad
        call lista_medidas
        mov eax, [foco_i]
        cmp eax, [n_ents]
        jae .x
        call lista_xy               ; EBX = y en la ventana
        sub ebx, [l_cab]
        mov ecx, FILA_D
        cmp dword [vista], 0
        jne .a
        mov ecx, CEL_AL
.a:
        test ebx, ebx
        jns .b
        add [desp_l], ebx
        jmp .p
.b:
        add ebx, ecx
        sub ebx, [l_pag]
        jle .x
        add [desp_l], ebx
.p:
        call lista_medidas
        call lista_inv
.x:
        popad
        ret

; ---- el procedimiento ----
lista_proc:
        cmp eax, WM_PAINT
        je lista_pintar
        cmp eax, WM_LBUTTONDOWN
        je lista_pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je lista_doble
        cmp eax, WM_MOUSEMOVE
        je lista_mueve
        cmp eax, WM_LBUTTONUP
        je lista_suelta
        cmp eax, WM_RBUTTONDOWN
        je lista_der
        cmp eax, WM_KEYDOWN
        je lista_tecla
        cmp eax, WM_CHAR
        je lista_letra
        cmp eax, WM_VSCROLL
        je .barra
        cmp eax, WM_MOUSEWHEEL
        je .rueda
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_KILLFOCUS
        je .sinfoco
        HVJ ven_defecto
.foco:
        mov dword [panel_foco], 2
        mov dword [ult_panel], 2
        HV ven_invalidar
        jmp hecho
.sinfoco:
        mov dword [panel_foco], 0
        HV ven_invalidar
        jmp hecho
.barra:
        mov [desp_l], ecx
        HV ven_invalidar
        jmp hecho
.rueda:
        cmp dword [l_barra], 0
        je hecho
        mov ebp, [v_barra_l]
        mov eax, WM_MOUSEWHEEL
        imul ecx, ecx, 3
        HV msg_enviar
        jmp hecho

; ---- pintar ----
lista_pintar:
        HV dc_pintar
        cmp dword [vista], 0
        jne .det
        ; vista Iconos
        mov eax, [desp_l]           ; las filas que se ven
        xor edx, edx
        mov ecx, CEL_AL
        div ecx
        imul eax, [l_cols]
        mov [pi_i], eax
.i:
        mov eax, [pi_i]
        cmp eax, [n_ents]
        jae .fin
        call lista_xy
        cmp ebx, [l_al]
        jge .fin
        mov [pi_x], eax
        mov [pi_y], ebx
        mov esi, [pi_i]
        shl esi, 5
        add esi, ents
        mov [pi_e], esi
        ; el icono
        movzx eax, byte [esi+E_TIPO]
        shl eax, 10
        lea esi, [ico32+eax]
        mov eax, [pi_x]
        add eax, (CEL_AN - 32) / 2
        mov ebx, [pi_y]
        add ebx, 4
        mov ecx, 32
        mov edx, 32
        HV dc_copiar_clave
        ; el nombre, centrado debajo
        mov esi, [pi_e]
        call largo
        mov ecx, eax
        shl eax, 2
        neg eax
        add eax, [pi_x]
        add eax, CEL_AN / 2
        mov ebx, [pi_y]
        add ebx, 40
        call pinta_nombre
        inc dword [pi_i]
        jmp .i
.det:
        call col_medir
        call cabecera_pintar
        mov eax, [desp_l]
        xor edx, edx
        mov ecx, FILA_D
        div ecx
        mov [pi_i], eax
.d:
        mov eax, [pi_i]
        cmp eax, [n_ents]
        jae .fin
        call lista_xy
        cmp ebx, [l_al]
        jge .fin
        mov [pi_y], ebx
        mov esi, [pi_i]
        shl esi, 5
        add esi, ents
        mov [pi_e], esi
        call fila_pintar
        inc dword [pi_i]
        jmp .d
.fin:
        cmp dword [n_ents], 0       ; vacía: lo dice
        jne .f2
        COLOR C_ARENA_OSC
        mov eax, 12
        mov ebx, [l_cab]
        add ebx, 10
        mov esi, t_vacia
        cmp dword [modo_busca], 0
        je .f1
        mov esi, t_nada
.f1:
        HV dc_texto
.f2:
        HV dc_cerrar
        jmp hecho

; (interna) el nombre de [pi_e] en EAX, EBX (ECX letras), con lo elegido
; resaltado y el destino de arrastrar enmarcado
pinta_nombre:
        pushad
        mov esi, [pi_e]
        mov edi, [pi_i]
        COLOR C_TINTA
        test byte [esi+E_MARCA], 2  ; cortado: más claro
        jz .c
        COLOR C_ARENA_OSC
.c:
        test byte [esi+E_MARCA], 1
        jz .t
        push eax
        push ecx
        COLOR C_ARENA_OSC
        cmp dword [panel_foco], 2
        jne .r
        COLOR C_ANIL
.r:
        sub eax, 3
        lea ecx, [ecx*8+6]
        dec ebx
        mov edx, 18
        HV dc_rect
        inc ebx
        pop ecx
        pop eax
        COLOR C_BLANCO
.t:
        HV dc_texto_n
        cmp edi, [foco_i]           ; el foco: un marco de puntos
        jne .d
        cmp dword [panel_foco], 2
        jne .d
        sub eax, 4
        lea ecx, [ecx*8+8]
        sub ebx, 2
        mov edx, 20
        call marco_puntos
        add eax, 4
        add ebx, 2
.d:
        cmp dword [arr_dtipo], 2    ; el destino de lo que se arrastra
        jne .x
        cmp edi, [arr_di]
        jne .x
        COLOR C_TERRACOTA
        mov eax, [pi_x]
        mov ebx, [pi_y]
        mov ecx, CEL_AN
        mov edx, CEL_AL
        cmp dword [vista], 0
        je .m
        mov ecx, [l_ancho]
        mov edx, FILA_D
.m:
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
.x:
        popad
        ret

; EAX, EBX, ECX, EDX: un marco de puntos (el foco)
marco_puntos:
        pushad
        COLOR C_TINTA
        mov esi, eax
        mov edi, ebx
        xor ebp, ebp
.h:
        cmp ebp, [esp+24]           ; (ECX: el ancho)
        jge .v0
        lea eax, [esi+ebp]
        mov ebx, edi
        HV dc_punto
        add ebx, [esp+20]           ; (EDX: el alto)
        dec ebx
        HV dc_punto
        add ebp, 2
        jmp .h
.v0:
        xor ebp, ebp
.v:
        cmp ebp, [esp+20]
        jge .x
        mov eax, esi
        lea ebx, [edi+ebp]
        HV dc_punto
        add eax, [esp+24]
        dec eax
        HV dc_punto
        add ebp, 2
        jmp .v
.x:
        popad
        ret

; EAX=x EBX=y ESI=texto ECX=letras como mucho: lo escribe (sin pasarse)
texto_corto:
        push eax
        push ecx
        call largo
        cmp eax, ecx
        jae .c
        mov ecx, eax
.c:
        mov eax, [esp+4]
        HV dc_texto_n
        pop ecx
        pop eax
        ret

; (interna) la cabecera de las columnas
cabecera_pintar:
        pushad
        COLOR C_ARENA
        xor eax, eax
        xor ebx, ebx
        mov ecx, [l_an]
        mov edx, CAB_D
        HV dc_rect
        COLOR C_ARENA_OSC
        mov ebx, CAB_D - 1
        mov edx, 1
        HV dc_rect
        xor ecx, ecx
.c:
        COLOR C_ARENA_OSC
        mov eax, [col_x+ecx*4]
        test ecx, ecx
        jz .t
        dec eax
        xor ebx, ebx
        push ecx
        mov ecx, 1
        mov edx, CAB_D
        HV dc_rect
        pop ecx
        inc eax
.t:
        COLOR C_TINTA
        mov esi, [cab_txt+ecx*4]
        cmp ecx, 2
        jne .t1
        cmp dword [modo_busca], 0
        je .t1
        mov esi, t_c_dondes
.t1:
        add eax, 6
        mov ebx, 2
        cmp ecx, [orden]
        jne .t2
        NEGRITA 1
.t2:
        HV dc_texto
        NEGRITA 0
        cmp ecx, [orden]            ; el orden: un triangulito
        jne .s
        push eax
        call largo
        lea eax, [eax*8]
        add eax, [esp]
        add eax, 8
        call triangulo
        pop eax
.s:
        inc ecx
        cmp ecx, 4
        jb .c
        popad
        ret
            align 4
cab_txt     dd t_c_nombre, t_c_tam, t_c_tipo, t_c_fecha

; (interna) EAX=x: un triangulito (arriba: de menor a mayor; abajo: al revés)
triangulo:
        pushad
        COLOR C_MARRON
        mov esi, eax
        xor edi, edi
.l:
        mov eax, esi
        add eax, edi
        mov ebx, 11
        sub ebx, edi
        cmp dword [inverso], 0
        je .a
        mov ebx, 7
        add ebx, edi
.a:
        mov ecx, 9
        sub ecx, edi
        sub ecx, edi
        jle .x
        mov edx, 1
        HV dc_rect
        inc edi
        jmp .l
.x:
        popad
        ret

; (interna) una fila de la vista Detalles ([pi_e], [pi_y])
fila_pintar:
        pushad
        mov esi, [pi_e]
        test byte [esi+E_MARCA], 1  ; elegida: toda la fila
        jz .i
        COLOR C_ARENA_OSC
        cmp dword [panel_foco], 2
        jne .r
        COLOR C_ANIL
.r:
        xor eax, eax
        mov ebx, [pi_y]
        mov ecx, [l_ancho]
        mov edx, FILA_D
        HV dc_rect
.i:
        movzx eax, byte [esi+E_TIPO]
        shl eax, 8
        lea esi, [ico16+eax]
        mov eax, 3
        mov ebx, [pi_y]
        inc ebx
        mov ecx, 16
        mov edx, 16
        HV dc_copiar_clave
        ; el color del texto
        mov esi, [pi_e]
        COLOR C_TINTA
        test byte [esi+E_MARCA], 2
        jz .c1
        COLOR C_ARENA_OSC
.c1:
        test byte [esi+E_MARCA], 1
        jz .c2
        COLOR C_BLANCO
.c2:
        ; el nombre
        mov eax, 22
        mov ebx, [pi_y]
        inc ebx
        mov ecx, [col_x+4]
        sub ecx, 26
        shr ecx, 3
        call texto_corto
        ; el tamaño (los archivos), a la derecha
        test byte [esi+E_ATR], 0x10
        jnz .tipo
        mov edi, pi_txt
        mov eax, [esi+E_TAM]
        call pon_miles
        mov byte [edi], 0
        push esi
        mov esi, pi_txt
        call largo
        shl eax, 3
        neg eax
        add eax, [col_x+8]
        sub eax, 8
        HV dc_texto
        pop esi
.tipo:
        ; el tipo (o, de una búsqueda, la carpeta)
        mov eax, [col_x+8]
        add eax, 6
        mov ecx, [col_x+12]
        sub ecx, eax
        shr ecx, 3
        cmp dword [modo_busca], 0
        je .t1
        push esi
        mov esi, [esi+E_DIR]
        test esi, esi
        jz .t0
        call texto_corto
.t0:
        pop esi
        jmp .fecha
.t1:
        call tipo_texto
        push esi
        mov esi, pi_txt
        call texto_corto
        pop esi
.fecha:
        ; la fecha y la hora: «dd/mm/aaaa hh:mm»
        mov edi, pi_txt
        call pon_fecha
        mov byte [edi], 0
        push esi
        mov eax, [col_x+12]
        add eax, 6
        mov esi, pi_txt
        HV dc_texto
        pop esi
        ; el foco
        mov eax, [pi_i]
        cmp eax, [foco_i]
        jne .d
        cmp dword [panel_foco], 2
        jne .d
        xor eax, eax
        mov ebx, [pi_y]
        mov ecx, [l_ancho]
        mov edx, FILA_D
        call marco_puntos
.d:
        cmp dword [arr_dtipo], 2
        jne .x
        mov eax, [pi_i]
        cmp eax, [arr_di]
        jne .x
        COLOR C_TERRACOTA
        xor eax, eax
        mov ebx, [pi_y]
        mov ecx, [l_ancho]
        mov edx, FILA_D
        HV dc_marco
.x:
        popad
        ret

; ESI=entrada -> pi_txt = su tipo («Texto», «Archivo XYZ»)
tipo_texto:
        pushad
        mov edi, pi_txt
        movzx eax, byte [esi+E_TIPO]
        push esi
        mov esi, [nom_tipos+eax*4]
        call hvl_cad
        pop esi
        cmp al, TI_DESC
        jne .x
        lea esi, [esi+E_NOM]
.p:
        lodsb
        test al, al
        jz .x
        cmp al, '.'
        jne .p
        mov al, ' '
        stosb
        call hvl_cad
.x:
        mov byte [edi], 0
        popad
        ret

; ESI=entrada: su fecha y hora en EDI («14/11/2026 06:57»), que avanza
pon_fecha:
        push eax
        push edx
        movzx edx, word [esi+E_FECHA]
        mov eax, edx
        and eax, 31
        call dos_cifras
        mov al, '/'
        stosb
        mov eax, edx
        shr eax, 5
        and eax, 15
        call dos_cifras
        mov al, '/'
        stosb
        mov eax, edx
        shr eax, 9
        add eax, 1980
        call hvl_num
        cmp dword [col_hora], 0
        je .x
        mov al, ' '
        stosb
        movzx edx, word [esi+E_HORA]
        mov eax, edx
        shr eax, 11
        call dos_cifras
        mov al, ':'
        stosb
        mov eax, edx
        shr eax, 5
        and eax, 63
        call dos_cifras
.x:
        pop edx
        pop eax
        ret
dos_cifras:
        push edx
        xor edx, edx
        push ecx
        mov ecx, 10
        div ecx
        pop ecx
        add al, '0'
        stosb
        mov al, dl
        add al, '0'
        stosb
        pop edx
        ret

; ---- el ratón ----
lista_pulsa:
        cmp dword [ocupado], 0
        jne hecho
        call lista_en
        cmp eax, -2
        je .cab
        mov [arr_x0], edx
        mov dword [arr_pend], 0
        mov dword [arr_origen], 2
        HV ven_capturar
        test eax, eax
        js .fondo
        mov ebx, ecx                ; las mayúsculas
        test ebx, TM_MAYUS
        jnz .rango
        test ebx, TM_CTRL
        jnz .cambia
        shl eax, 5
        test byte [ents+eax+E_MARCA], 1
        jz .nuevo
        shr eax, 5
        mov [foco_i], eax           ; ya elegido: puede ser para arrastrarlo
        mov [ancla], eax
        mov dword [arr_pend], 2     ; (al soltar sin arrastrar: solo él)
        jmp .fin
.nuevo:
        shr eax, 5
        call sel_uno
        mov dword [arr_pend], 1
        jmp .fin
.rango:
        call sel_rango
        jmp .fin
.cambia:
        mov [foco_i], eax
        mov [ancla], eax
        shl eax, 5
        xor byte [ents+eax+E_MARCA], 1
        jmp .fin
.fondo:
        test ecx, TM_MAYUS | TM_CTRL
        jnz .fin
        xor eax, eax
        call sel_todos
.fin:
        call lista_inv
        call estado_poner
        jmp hecho
.cab:                               ; la cabecera: ordenar por esa columna
        lea ecx, [ecx+O_ORD_NOM]
        call o_orden
        jmp hecho

lista_doble:
        cmp dword [ocupado], 0
        jne hecho
        call lista_en
        test eax, eax
        js hecho
        call sel_uno
        call lista_inv
        call estado_poner
        call o_abrir
        jmp hecho

lista_der:                          ; el botón derecho: lo que hay debajo, elegido
        cmp dword [ocupado], 0
        jne hecho
        HV ven_enfocar
        call lista_en
        test eax, eax
        js .fondo
        mov ebx, eax
        shl ebx, 5
        test byte [ents+ebx+E_MARCA], 1
        jnz .ya
        call sel_uno
.ya:
        mov [foco_i], eax
        jmp .x
.fondo:
        xor eax, eax
        call sel_todos
.x:
        call lista_inv
        call estado_poner
        jmp hecho

lista_mueve:
        cmp dword [arr_origen], 2
        jne hecho
        jmp arr_mueve

lista_suelta:
        cmp dword [arr_origen], 2
        jne hecho
        HV ven_soltar
        cmp dword [arr_activo], 0
        je .clic
        call arr_soltar
        jmp hecho
.clic:
        mov dword [arr_origen], 0
        cmp dword [arr_pend], 2     ; un clic en uno ya elegido: solo él
        jne hecho
        mov eax, [foco_i]
        call sel_uno
        call lista_inv
        call estado_poner
        jmp hecho

; ---- el teclado ----
lista_tecla:
        cmp dword [ocupado], 0
        jne hecho
        mov [lt_mayus], edx
        mov eax, [foco_i]
        mov ebx, [l_cols]
        cmp dword [vista], 0
        je .v
        mov ebx, 1
.v:
        cmp ecx, TC_ABAJO
        je .abajo
        cmp ecx, TC_ARRIBA
        je .arriba
        cmp ecx, TC_DER
        je .der
        cmp ecx, TC_IZQ
        je .izq
        cmp ecx, TC_INICIO
        je .inicio
        cmp ecx, TC_FIN
        je .fin
        cmp ecx, TC_AVPAG
        je .avpag
        cmp ecx, TC_REPAG
        je .repag
        cmp ecx, TC_INTRO
        je .intro
        call tecla_orden            ; Supr, F2, Ctrl+C...
        jnc hecho
        mov eax, WM_KEYDOWN
        HVJ ven_defecto             ; (Tab y lo demás)
.intro:
        call o_abrir
        jmp hecho
.abajo:
        add eax, ebx
        jmp .ir
.arriba:
        sub eax, ebx
        jns .ir
        xor eax, eax
        jmp .ir
.der:
        cmp dword [vista], 0
        jne hecho
        inc eax
        jmp .ir
.izq:
        cmp dword [vista], 0
        jne hecho
        test eax, eax
        jz .ir
        dec eax
        jmp .ir
.inicio:
        xor eax, eax
        jmp .ir
.fin:
        mov eax, [n_ents]
        dec eax
        jmp .ir
.avpag:
        call .pagina
        add eax, ecx
        jmp .ir
.repag:
        call .pagina
        sub eax, ecx
        jns .ir
        xor eax, eax
        jmp .ir
.pagina:                            ; -> ECX = lo que es una página
        push eax
        push edx
        mov eax, [l_pag]
        xor edx, edx
        mov ecx, FILA_D
        cmp dword [vista], 0
        jne .p1
        mov ecx, CEL_AL
.p1:
        div ecx
        cmp eax, 1
        jae .p2
        mov eax, 1
.p2:
        imul eax, ebx
        mov ecx, eax
        pop edx
        pop eax
        ret
.ir:
        cmp dword [n_ents], 0
        je hecho
        cmp eax, [n_ents]
        jb .ir1
        mov eax, [n_ents]
        dec eax
.ir1:
        test dword [lt_mayus], TM_MAYUS
        jnz .rango
        test dword [lt_mayus], TM_CTRL
        jnz .solofoco
        call sel_uno
        jmp .visto
.rango:
        call sel_rango
        jmp .visto
.solofoco:
        mov [foco_i], eax
.visto:
        call lista_ver_foco
        call lista_inv
        call estado_poner
        jmp hecho

; una letra: al siguiente que empieza por ella; Ctrl+Espacio elige o deja
lista_letra:
        cmp dword [ocupado], 0
        jne hecho
        cmp ecx, ' '
        jne .l
        HV tec_estado
        test eax, TM_CTRL
        jz hecho
        mov eax, [foco_i]
        cmp eax, [n_ents]
        jae hecho
        mov [ancla], eax
        shl eax, 5
        xor byte [ents+eax+E_MARCA], 1
        call lista_inv
        call estado_poner
        jmp hecho
.l:
        cmp ecx, ' '
        jbe hecho
        mov eax, ecx
        call mayus
        mov ah, al
        mov ecx, [n_ents]
        test ecx, ecx
        jz hecho
        mov edx, [foco_i]
.b:
        inc edx
        cmp edx, [n_ents]
        jb .b1
        xor edx, edx
.b1:
        mov ebx, edx
        shl ebx, 5
        mov al, [ents+ebx+E_NOM]
        cmp al, ah
        je .hay
        dec ecx
        jnz .b
        jmp hecho
.hay:
        mov eax, edx
        call sel_uno
        call lista_ver_foco
        call lista_inv
        call estado_poner
        jmp hecho

        section .bss
            alignb 4
n_ents      resd 1
foco_i      resd 1
ancla       resd 1
desp_l      resd 1
l_an        resd 1
l_al        resd 1
l_cab       resd 1
l_pag       resd 1
l_todo      resd 1
l_max       resd 1
l_barra     resd 1
l_ancho     resd 1
l_cols      resd 1
col_x       resd 5
col_hora    resd 1
pi_i        resd 1
pi_x        resd 1
pi_y        resd 1
pi_e        resd 1
lt_mayus    resd 1
n_selnom    resd 1
pi_txt      resb 64
dta_l       resb DTA_TAM
ent_aux     resb ENT_TAM
sel_nombres resb 64 * 13
ents        resb MAX_ENT * ENT_TAM
        section .text
