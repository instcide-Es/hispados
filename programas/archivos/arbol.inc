; =============================================================================
;  ARCHIVOS.HVP - el árbol: las unidades y sus carpetas, desplegables
;  Las filas van seguidas, como se ven (nodos): cada una con su nivel (0 las
;  unidades); desplegar una mete detrás sus carpetas, plegarla las quita.
; =============================================================================

; las unidades que hay: A: (y B: si hay dos disqueteras), y de C: a Z: las
; que dice DOS (4408h: fijas o extraíbles; un CD es extraíble)
unidades_buscar:
        pushad
        mov dword [n_nodos], 0
        call d_disqueteras
        mov [n_disq], eax
        xor ebx, ebx                ; la unidad
.u:
        mov al, bl
        call d_unidad
        jc .s
        mov edx, eax                ; 1 fija
        cmp ebx, 1
        ja .c
        jb .a
        cmp dword [n_disq], 2       ; B: solo con dos disqueteras
        jb .s
.a:
        xor edx, edx                ; disquete
        jmp .pon
.c:
        cmp edx, 1
        je .pon
        mov edx, 2                  ; CD o extraíble
.pon:
        mov edi, [n_nodos]
        cmp edi, MAX_NODOS
        jae .x
        shl edi, 4
        add edi, nodos
        lea eax, [ebx+'A']
        mov [edi+N_NOM], al
        mov word [edi+N_NOM+1], ':'
        mov byte [edi+N_NIVEL], 0
        mov byte [edi+N_BAND], NB_UNIDAD
        mov [edi+N_UTIPO], dl
        inc dword [n_nodos]
.s:
        inc ebx
        cmp ebx, 26
        jb .u
.x:
        mov dword [sel_nodo], 0
        mov dword [desp_a], 0
        popad
        ret

; EAX=índice -> EAX=puntero a su fila
nodo_ptr:
        shl eax, 4
        add eax, nodos
        ret

; EAX=índice EDI=destino: su ruta («C:\» o «C:\A\B»)
arbol_ruta:
        pushad
        mov ebx, eax                ; los de arriba, de abajo arriba, en la pila
        xor ecx, ecx
        mov edx, eax
        call nodo_ptr
        movzx esi, byte [eax+N_NIVEL]
.s:
        push edx
        inc ecx
        test esi, esi
        jz .es
.b:
        dec edx
        js .es
        mov eax, edx
        call nodo_ptr
        movzx eax, byte [eax+N_NIVEL]
        cmp eax, esi
        jae .b
        mov esi, eax
        jmp .s
.es:
        ; de arriba abajo: «C:» y los nombres con «\»
        pop eax
        dec ecx
        call nodo_ptr
        mov ax, [eax+N_NOM]
        stosw
        mov al, '\'
        stosb
        test ecx, ecx
        jz .f
.n:
        pop eax
        call nodo_ptr
        mov esi, eax
        call hvl_cad
        dec ecx
        jz .f
        mov al, '\'
        stosb
        jmp .n
.f:
        mov byte [edi], 0
        popad
        ret

; EAX=índice: lee sus carpetas y las mete detrás -> CF=1 (EAX=error) si no
; se puede leer
arbol_desplegar:
        pushad
        mov [ad_i], eax
        mov edi, tmp2
        call arbol_ruta
        mov esi, tmp2
        mov edi, tmp_ad
        mov ebx, t_patron
        call ruta_unir
        call crit_olvidar
        mov esi, tmp_ad
        mov ecx, 0x16
        mov edi, dta_a
        call d_primero
        jnc .hay
        cmp eax, 0x12
        je .vacia
        cmp eax, 2
        je .vacia
        mov [esp+28], eax
        popad
        stc
        ret
.hay:
        xor ebx, ebx                ; los nombres, a ad_noms (ordenados)
.e:
        test byte [dta_a+DTA_ATR], 0x10
        jz .sig
        cmp byte [dta_a+DTA_NOMBRE], '.'
        je .sig
        cmp ebx, 255
        jae .fin
        ; se mete en su sitio (por orden alfabético)
        mov ecx, ebx
.o:
        test ecx, ecx
        jz .o2
        imul esi, ecx, 13           ; (13 octetos cada uno: el de antes)
        add esi, ad_noms - 13
        mov edi, dta_a + DTA_NOMBRE
        xchg esi, edi
        call cad_cmp
        xchg esi, edi
        test eax, eax
        jns .o2                     ; (el nuevo va detrás)
        lea edi, [esi+13]
        push ecx
        mov ecx, 13
        rep movsb
        pop ecx
        dec ecx
        jmp .o
.o2:
        imul edi, ecx, 13
        add edi, ad_noms
        mov esi, dta_a + DTA_NOMBRE
        push ecx
        mov ecx, 12
        call copiar_n
        pop ecx
        inc ebx
.sig:
        mov edi, dta_a
        call d_siguiente
        jnc .e
.fin:
        test ebx, ebx
        jz .vacia
        ; hueco detrás del nodo
        mov eax, [n_nodos]
        add eax, ebx
        cmp eax, MAX_NODOS
        jbe .cabe
        mov ebx, MAX_NODOS
        sub ebx, [n_nodos]
        jle .vacia
.cabe:
        mov esi, [n_nodos]          ; de atrás adelante: [i+1, n) -> + ebx
        shl esi, 4
        add esi, nodos - 1
        mov edi, ebx
        shl edi, 4
        add edi, esi
        mov ecx, [n_nodos]
        sub ecx, [ad_i]
        dec ecx
        shl ecx, 4
        std
        rep movsb
        cld
        add [n_nodos], ebx
        mov eax, [ad_i]
        call nodo_ptr
        or byte [eax+N_BAND], NB_ABIERTO
        and byte [eax+N_BAND], ~NB_VACIO
        mov dl, [eax+N_NIVEL]
        inc dl
        lea edi, [eax+NODO_TAM]
        mov esi, ad_noms
        mov ecx, ebx
.m:
        push ecx
        mov ecx, 12
        call copiar_n
        pop ecx
        mov [edi+N_NIVEL], dl
        mov byte [edi+N_BAND], 0
        mov byte [edi+N_UTIPO], 0
        add esi, 13
        add edi, NODO_TAM
        dec ecx
        jnz .m
        mov eax, [sel_nodo]         ; lo de detrás se ha corrido
        cmp eax, [ad_i]
        jbe .bien
        add [sel_nodo], ebx
        jmp .bien
.vacia:
        mov eax, [ad_i]
        call nodo_ptr
        or byte [eax+N_BAND], NB_ABIERTO | NB_VACIO
.bien:
        call arbol_medidas
        call arbol_inv
        popad
        clc
        ret

; EAX=índice: quita lo que tiene debajo
arbol_plegar:
        pushad
        mov ebx, eax
        call nodo_ptr
        and byte [eax+N_BAND], ~NB_ABIERTO
        mov dl, [eax+N_NIVEL]
        lea ecx, [ebx+1]            ; hasta dónde llega lo suyo
.b:
        cmp ecx, [n_nodos]
        jae .h
        mov eax, ecx
        call nodo_ptr
        cmp [eax+N_NIVEL], dl
        jbe .h
        inc ecx
        jmp .b
.h:
        mov edx, ecx
        sub edx, ebx
        dec edx                     ; cuántos se quitan
        jz .x
        mov eax, ecx
        call nodo_ptr
        mov esi, eax
        lea eax, [ebx+1]
        call nodo_ptr
        mov edi, eax
        push ecx
        neg ecx
        add ecx, [n_nodos]
        shl ecx, 4
        rep movsb
        pop ecx
        sub [n_nodos], edx
        mov eax, [sel_nodo]
        cmp eax, ebx
        jbe .x
        cmp eax, ecx
        jae .detras
        mov [sel_nodo], ebx         ; lo elegido estaba dentro: el plegado
        jmp .x
.detras:
        sub [sel_nodo], edx
.x:
        call arbol_medidas
        call arbol_inv
        popad
        ret

; EAX=índice: lo despliega o lo pliega (y si no se puede leer, lo dice)
arbol_alternar:
        pushad
        cmp eax, [n_nodos]
        jae .x
        mov ebx, eax
        call nodo_ptr
        test byte [eax+N_BAND], NB_ABIERTO
        jz .d
        mov eax, ebx
        call arbol_plegar
        jmp .x
.d:
        mov eax, ebx
        call arbol_desplegar
        jnc .x
        call err_texto
        mov edi, tmp_ruta
        push esi
        mov eax, ebx
        call arbol_ruta
        pop esi
        mov edi, t_e_unidad
        call aviso_ruta
        cmp eax, ID_REINT
        jne .x
        mov eax, ebx
        call arbol_alternar
.x:
        popad
        ret

; ESI=ruta EAX=1 desplegando lo que haga falta (0: solo lo que ya se ve)
; -> EAX=el nodo de la ruta, o el más hondo de los que se encuentran, y CF=1
; si no se ha llegado hasta ella
arbol_buscar:
        pushad
        mov [ab_desp], eax
        mov al, [esi]
        xor ecx, ecx                ; la unidad
.u:
        cmp ecx, [n_nodos]
        jae .no
        mov edi, ecx
        shl edi, 4
        add edi, nodos
        cmp byte [edi+N_NIVEL], 0
        jne .us
        cmp [edi+N_NOM], al
        je .hu
.us:
        inc ecx
        jmp .u
.hu:
        mov [ab_i], ecx
        add esi, 3                  ; tras «C:\»
.t:
        cmp byte [esi], 0
        je .si
        ; el trozo, a ab_nom
        mov edi, ab_nom
        mov ecx, 12
.c:
        lodsb
        cmp al, '\'
        je .c2
        test al, al
        jz .c1
        stosb
        loop .c
        jmp .c2
.c1:
        dec esi
.c2:
        mov byte [edi], 0
        mov eax, [ab_i]
        call nodo_ptr
        test byte [eax+N_BAND], NB_ABIERTO
        jnz .ab
        cmp dword [ab_desp], 0
        je .falta
        mov eax, [ab_i]
        call arbol_desplegar
        jc .falta
.ab:
        mov eax, [ab_i]             ; su hija con ese nombre
        call nodo_ptr
        mov dl, [eax+N_NIVEL]
        mov ecx, [ab_i]
.h:
        inc ecx
        cmp ecx, [n_nodos]
        jae .falta
        mov eax, ecx
        call nodo_ptr
        cmp [eax+N_NIVEL], dl
        jbe .falta
        mov bl, dl
        inc bl
        cmp [eax+N_NIVEL], bl
        jne .h
        push esi
        mov esi, eax
        mov edi, ab_nom
        call igual
        pop esi
        jne .h
        mov [ab_i], ecx
        jmp .t
.si:
        mov eax, [ab_i]
        mov [esp+28], eax
        popad
        clc
        ret
.falta:
        mov eax, [ab_i]
        mov [esp+28], eax
        popad
        stc
        ret
.no:
        mov dword [esp+28], 0
        popad
        stc
        ret

; el árbol, a la carpeta de la ventana: despliega lo que haga falta y la
; deja elegida y a la vista
arbol_seguir:
        pushad
        mov esi, ruta
        mov eax, 1
        call arbol_buscar
        mov [sel_nodo], eax
        jc .v                       ; (la carpeta que se ve, desplegada)
        mov ebx, eax
        call nodo_ptr
        test byte [eax+N_BAND], NB_ABIERTO
        jnz .v
        mov eax, ebx
        call arbol_desplegar
.v:
        call arbol_ver_sel
        call arbol_inv
        popad
        ret

; ESI=ruta de una carpeta que ha cambiado: si se ve desplegada, se vuelve a
; leer; si no, se olvida que no tenía carpetas
arbol_recargar:
        pushad
        xor eax, eax
        call arbol_buscar
        jc .x
        mov ebx, eax
        call nodo_ptr
        and byte [eax+N_BAND], ~NB_VACIO
        test byte [eax+N_BAND], NB_ABIERTO
        jz .x
        mov eax, ebx
        call arbol_plegar
        mov eax, ebx
        call arbol_desplegar
.x:
        call arbol_seguir
        popad
        ret
arbol_recargar_actual:
        push esi
        mov esi, ruta
        call arbol_recargar
        pop esi
        ret

; ---- la ventana ----
arbol_medidas:
        pushad
        mov ebp, [v_arbol]
        test ebp, ebp
        jz .x
        HV ven_cli
        mov [a_an], ecx
        mov [a_al], edx
        mov eax, [n_nodos]
        imul eax, FILA_A
        sub eax, edx
        jns .m
        xor eax, eax
.m:
        mov [a_max], eax
        cmp [desp_a], eax
        jle .d
        mov [desp_a], eax
.d:
        mov ebp, [v_barra_a]
        test eax, eax
        jz .sinb
        mov dword [a_barra], 1
        mov eax, [a_an]
        sub eax, BD_AN
        xor ebx, ebx
        mov ecx, BD_AN
        mov edx, [a_al]
        HV ven_mover
        mov eax, BDM_PONER
        mov ecx, [a_max]
        mov edx, [a_al]
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [desp_a]
        HV msg_enviar
        mov eax, 1
        HV ven_mostrar
        jmp .x
.sinb:
        mov dword [a_barra], 0
        xor eax, eax
        HV ven_mostrar
.x:
        popad
        ret

arbol_inv:
        push ebp
        mov ebp, [v_arbol]
        test ebp, ebp
        jz .x
        HV ven_invalidar
.x:
        pop ebp
        ret

; que lo elegido se vea
arbol_ver_sel:
        pushad
        call arbol_medidas
        mov eax, [sel_nodo]
        imul eax, FILA_A
        sub eax, [desp_a]
        jns .a
        add [desp_a], eax
        jmp .p
.a:
        add eax, FILA_A
        sub eax, [a_al]
        jle .x
        add [desp_a], eax
.p:
        call arbol_medidas
.x:
        popad
        ret

; EDX=x,y -> EAX=nodo (-1: ninguno) y ECX=1 si es en su cuadradito
arbol_en:
        push edx
        xor ecx, ecx
        mov eax, edx
        sar eax, 16
        add eax, [desp_a]
        js .no
        push edx
        xor edx, edx
        push ecx
        mov ecx, FILA_A
        div ecx
        pop ecx
        pop edx
        cmp eax, [n_nodos]
        jae .no
        push eax
        call nodo_ptr
        movzx eax, byte [eax+N_NIVEL]
        imul eax, SANGRIA
        add eax, 2
        movsx edx, dx
        sub edx, eax
        cmp edx, -2
        jl .n
        cmp edx, 12
        jg .n
        inc ecx
.n:
        pop eax
        jmp .x
.no:
        mov eax, -1
.x:
        pop edx
        ret

arbol_proc:
        cmp eax, WM_PAINT
        je arbol_pintar
        cmp eax, WM_LBUTTONDOWN
        je arbol_pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je arbol_doble
        cmp eax, WM_MOUSEMOVE
        je arbol_mueve
        cmp eax, WM_LBUTTONUP
        je arbol_suelta
        cmp eax, WM_RBUTTONDOWN
        je arbol_der
        cmp eax, WM_KEYDOWN
        je arbol_tecla
        cmp eax, WM_CHAR
        je arbol_letra
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
        mov dword [panel_foco], 1
        mov dword [ult_panel], 1
        HV ven_invalidar
        jmp hecho
.sinfoco:
        mov dword [panel_foco], 0
        HV ven_invalidar
        jmp hecho
.barra:
        mov [desp_a], ecx
        HV ven_invalidar
        jmp hecho
.rueda:
        cmp dword [a_barra], 0
        je hecho
        mov ebp, [v_barra_a]
        mov eax, WM_MOUSEWHEEL
        imul ecx, ecx, 3
        HV msg_enviar
        jmp hecho

arbol_pintar:
        HV dc_pintar
        mov eax, [desp_a]
        xor edx, edx
        mov ecx, FILA_A
        div ecx
        mov [pa_i], eax
.f:
        mov eax, [pa_i]
        cmp eax, [n_nodos]
        jae .fin
        imul ebx, eax, FILA_A
        sub ebx, [desp_a]
        cmp ebx, [a_al]
        jge .fin
        mov [pa_y], ebx
        call nodo_ptr
        mov esi, eax
        movzx eax, byte [esi+N_NIVEL]
        imul eax, SANGRIA
        add eax, 4
        mov [pa_x], eax
        ; el cuadradito (si puede tener carpetas)
        test byte [esi+N_BAND], NB_VACIO
        jnz .ico
        COLOR C_BLANCO
        mov ebx, [pa_y]
        add ebx, 4
        mov ecx, 9
        mov edx, 9
        HV dc_rect
        COLOR C_ARENA_OSC
        HV dc_marco
        COLOR C_TINTA
        add eax, 2
        add ebx, 4
        mov ecx, 5
        mov edx, 1
        HV dc_rect
        test byte [esi+N_BAND], NB_ABIERTO
        jnz .ico
        add eax, 2
        sub ebx, 2
        mov ecx, 1
        mov edx, 5
        HV dc_rect
.ico:
        ; el icono
        mov eax, TI_CARP
        test byte [esi+N_BAND], NB_UNIDAD
        jz .i1
        movzx eax, byte [esi+N_UTIPO]
        add eax, IC_UNIDAD
.i1:
        shl eax, 8
        push esi
        lea esi, [ico16+eax]
        mov eax, [pa_x]
        add eax, 12
        mov ebx, [pa_y]
        inc ebx
        mov ecx, 16
        mov edx, 16
        HV dc_copiar_clave
        pop esi
        ; el nombre (una unidad: «Disco (C:)»)
        mov edi, pa_txt
        test byte [esi+N_BAND], NB_UNIDAD
        jz .nc
        call unidad_nombre
        jmp .nt
.nc:
        push esi
        call hvl_cad
        pop esi
.nt:
        mov byte [edi], 0
        mov eax, [pa_x]
        add eax, 32
        mov ebx, [pa_y]
        inc ebx
        mov esi, pa_txt
        call largo
        mov ecx, eax
        mov eax, [pa_x]
        add eax, 32
        COLOR C_TINTA
        mov edx, [pa_i]
        cmp edx, [sel_nodo]
        jne .t
        push eax
        push ecx
        COLOR C_ARENA_OSC
        cmp dword [panel_foco], 1
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
        cmp dword [arr_dtipo], 1    ; el destino de lo que se arrastra
        jne .s
        mov edx, [pa_i]
        cmp edx, [arr_di]
        jne .s
        COLOR C_TERRACOTA
        sub eax, 4
        lea ecx, [ecx*8+8]
        sub ebx, 2
        mov edx, 18
        HV dc_marco
.s:
        inc dword [pa_i]
        jmp .f
.fin:
        HV dc_cerrar
        jmp hecho

; ESI=nodo de una unidad: «Disquete (A:)» en EDI, que avanza
unidad_nombre:
        push eax
        push esi
        movzx eax, byte [esi+N_UTIPO]
        mov esi, [nom_unid+eax*4]
        cmp eax, 2                  ; extraíble: CD si se ha visto que lo es
        jne .n
        mov eax, [esp]
        movzx eax, byte [eax+N_NOM]
        cmp byte [es_cd+eax-'A'], 0
        jne .n
        mov esi, t_un2b
.n:
        call hvl_cad
        mov esi, [esp]
        mov ax, ' ('
        stosw
        mov ax, [esi+N_NOM]
        stosw
        mov al, ')'
        stosb
        pop esi
        pop eax
        ret

arbol_pulsa:
        cmp dword [ocupado], 0
        jne hecho
        call arbol_en
        test eax, eax
        js hecho
        test ecx, ecx
        jz .nodo
        call arbol_alternar         ; el cuadradito
        jmp hecho
.nodo:
        call arbol_abrir_nodo
        jc hecho
        mov [arr_x0], edx
        mov [arr_ai], eax
        mov dword [arr_pend], 1
        mov dword [arr_origen], 1
        HV ven_capturar
        jmp hecho

; EAX=nodo: la lista enseña esa carpeta
arbol_abrir_nodo:
        pushad
        mov [sel_nodo], eax
        call arbol_inv
        mov edi, tmp
        call arbol_ruta
        mov esi, tmp
        call ir_a
        jnc .x
        call arbol_seguir           ; (no se ha podido: vuelve a la de antes)
        popad
        stc
        ret
.x:
        popad
        clc
        ret

arbol_doble:
        cmp dword [ocupado], 0
        jne hecho
        call arbol_en
        test eax, eax
        js hecho
        test ecx, ecx
        jnz hecho                   ; (el cuadradito ya ha hecho lo suyo)
        call arbol_alternar
        jmp hecho

arbol_der:
        cmp dword [ocupado], 0
        jne hecho
        HV ven_enfocar
        call arbol_en
        test eax, eax
        js hecho
        mov [sel_nodo], eax
        call arbol_inv
        jmp hecho

arbol_mueve:
        cmp dword [arr_origen], 1
        jne hecho
        jmp arr_mueve

arbol_suelta:
        cmp dword [arr_origen], 1
        jne hecho
        HV ven_soltar
        cmp dword [arr_activo], 0
        je .x
        call arr_soltar
        jmp hecho
.x:
        mov dword [arr_origen], 0
        jmp hecho

arbol_tecla:
        cmp dword [ocupado], 0
        jne hecho
        mov eax, [sel_nodo]
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
        call tecla_orden
        jnc hecho
        mov eax, WM_KEYDOWN
        HVJ ven_defecto
.intro:
        call arbol_alternar
        jmp hecho
.abajo:
        inc eax
        jmp .ir
.arriba:
        dec eax
        jmp .ir
.inicio:
        xor eax, eax
        jmp .ir
.fin:
        mov eax, [n_nodos]
        dec eax
        jmp .ir
.avpag:
        call .pag
        add eax, ecx
        jmp .ir
.repag:
        call .pag
        sub eax, ecx
        jmp .ir
.pag:
        push eax
        push edx
        mov eax, [a_al]
        xor edx, edx
        mov ecx, FILA_A
        div ecx
        mov ecx, eax
        dec ecx
        jg .p1
        mov ecx, 1
.p1:
        pop edx
        pop eax
        ret
.der:                               ; desplegar o a la primera de dentro
        mov ebx, eax
        call nodo_ptr
        test byte [eax+N_BAND], NB_ABIERTO
        jnz .d1
        mov eax, ebx
        call arbol_alternar
        jmp hecho
.d1:
        test byte [eax+N_BAND], NB_VACIO
        jnz hecho
        lea eax, [ebx+1]
        jmp .ir
.izq:                               ; plegar o a la de arriba
        mov ebx, eax
        call nodo_ptr
        test byte [eax+N_BAND], NB_ABIERTO
        jz .i1
        test byte [eax+N_BAND], NB_VACIO
        jnz .i1
        mov eax, ebx
        call arbol_plegar
        jmp hecho
.i1:
        mov dl, [eax+N_NIVEL]
        test dl, dl
        jz hecho
        mov eax, ebx
.i2:
        dec eax
        js hecho
        push eax
        call nodo_ptr
        cmp [eax+N_NIVEL], dl
        pop eax
        jae .i2
.ir:
        test eax, eax
        jns .ir1
        xor eax, eax
.ir1:
        cmp eax, [n_nodos]
        jb .ir2
        mov eax, [n_nodos]
        dec eax
        js hecho
.ir2:
        cmp eax, [sel_nodo]
        je hecho
        call arbol_abrir_nodo
        jmp hecho

arbol_letra:
        cmp dword [ocupado], 0
        jne hecho
        mov eax, [sel_nodo]
        cmp ecx, '+'
        je .mas
        cmp ecx, '-'
        je .menos
        HVJ ven_defecto
.mas:
        call nodo_ptr
        test byte [eax+N_BAND], NB_ABIERTO
        jnz hecho
        mov eax, [sel_nodo]
        call arbol_alternar
        jmp hecho
.menos:
        call nodo_ptr
        test byte [eax+N_BAND], NB_ABIERTO
        jz hecho
        mov eax, [sel_nodo]
        call arbol_plegar
        jmp hecho

        section .bss
            alignb 4
n_nodos     resd 1
sel_nodo    resd 1
desp_a      resd 1
n_disq      resd 1
a_an        resd 1
a_al        resd 1
a_max       resd 1
a_barra     resd 1
ad_i        resd 1
ab_i        resd 1
ab_desp     resd 1
pa_i        resd 1
pa_x        resd 1
pa_y        resd 1
ult_panel   resd 1
es_cd       resb 32
ab_nom      resb 16
pa_txt      resb 48
tmp_ad      resb 128
dta_a       resb DTA_TAM
ad_noms     resb 256 * 13
nodos       resb MAX_NODOS * NODO_TAM
        section .text
