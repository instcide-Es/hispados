; =============================================================================
;  ARCHIVOS.HVP - lo que se hace con los archivos: abrir, copiar, mover,
;  eliminar, cambiar el nombre, carpeta nueva, propiedades y buscar; la
;  ventana de progreso y arrastrar y soltar
; =============================================================================
BUF_COPIA   equ 16384
W_NIV       equ 32                  ; carpetas, una dentro de otra, que se recorren
R_BIEN      equ 0                   ; lo que devuelven las operaciones de uno
R_OMITIDO   equ 1
R_CANCELA   equ 2

; -> EAX = 1 si la orden es para el árbol (el último que tuvo el foco), 2 la lista
objetivo:
        mov eax, [ult_panel]
        cmp eax, 1
        je .x
        mov eax, 2
.x:
        ret

; lo elegido (en la lista o el árbol), a fuentes -> CF=1 si no hay nada
fuentes_poner:
        pushad
        mov dword [n_fuentes], 0
        call objetivo
        cmp eax, 1
        jne .lista
        mov eax, [sel_nodo]
        cmp eax, [n_nodos]
        jae .no
        push eax
        call nodo_ptr
        test byte [eax+N_BAND], NB_UNIDAD
        pop eax
        jnz .no
        mov edi, fuentes
        call arbol_ruta
        mov dword [n_fuentes], 1
        jmp .si
.lista:
        xor ecx, ecx
        mov edi, fuentes
.l:
        cmp ecx, [n_ents]
        jae .lf
        mov eax, ecx
        shl eax, 5
        test byte [ents+eax+E_MARCA], 1
        jz .ls
        cmp dword [n_fuentes], PP_MAX
        jae .muchos
        mov eax, ecx
        call ent_ruta
        add edi, RUTA_TAM
        inc dword [n_fuentes]
.ls:
        inc ecx
        jmp .l
.muchos:
        mov esi, t_e_muchos
        call aviso
.lf:
        cmp dword [n_fuentes], 0
        je .no
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; =============================================================================
;  Copiar, cortar y pegar
; =============================================================================
o_copiar:
        xor eax, eax
        jmp o_pp
o_cortar:
        mov eax, 1
o_pp:
        call fuentes_poner
        jc .x
        mov [pp_cortar], eax
        mov esi, fuentes
        mov edi, pp_rutas
        mov ecx, [n_fuentes]
        mov [pp_n], ecx
        imul ecx, RUTA_TAM / 4
        rep movsd
        call pp_marcar_cortados
        call lista_inv
.x:
        ret

; en la lista, lo cortado sale más claro
pp_marcar_cortados:
        pushad
        xor ecx, ecx
.e:
        cmp ecx, [n_ents]
        jae .x
        mov ebx, ecx
        shl ebx, 5
        and byte [ents+ebx+E_MARCA], ~2
        cmp dword [pp_cortar], 0
        je .s
        mov eax, ecx
        mov edi, tmp2
        call ent_ruta
        mov esi, pp_rutas
        mov edx, [pp_n]
.p:
        test edx, edx
        jz .s
        mov edi, tmp2
        call igual
        je .si
        add esi, RUTA_TAM
        dec edx
        jmp .p
.si:
        or byte [ents+ebx+E_MARCA], 2
.s:
        inc ecx
        jmp .e
.x:
        popad
        ret

o_pegar:
        cmp dword [pp_n], 0
        je .x
        call objetivo
        cmp eax, 1
        jne .l
        mov eax, [sel_nodo]
        mov edi, op_dest
        call arbol_ruta
        jmp .h
.l:
        cmp dword [modo_busca], 0
        jne .x
        mov esi, ruta
        mov edi, op_dest
        mov ecx, RUTA_TAM - 1
        call copiar_n
.h:
        mov esi, pp_rutas
        mov edi, fuentes
        mov ecx, [pp_n]
        mov [n_fuentes], ecx
        imul ecx, RUTA_TAM / 4
        rep movsd
        mov eax, [pp_cortar]
        call operacion
        cmp dword [pp_cortar], 0    ; lo cortado ya no está donde estaba
        je .x
        mov dword [pp_n], 0
        mov dword [pp_cortar], 0
.x:
        ret

; =============================================================================
;  La operación: fuentes (n_fuentes) a op_dest; EAX=0 copiar, 1 mover
; =============================================================================
operacion:
        pushad
        mov [op_mover], eax
        mov dword [op_todos], 0
        mov dword [op_hechos], 0
        call crit_olvidar
        ; una carpeta no puede ir dentro de sí misma
        mov esi, fuentes
        mov ecx, [n_fuentes]
.d:
        push ecx
        call d_atr_leer
        mov eax, ecx
        pop ecx
        jc .ds
        test al, 0x10
        jz .ds
        mov edi, op_dest
        call ruta_dentro
        jne .ds
        mov esi, t_e_dentro
        call aviso
        jmp .x
.ds:
        add esi, RUTA_TAM
        loop .d
        ; ¿todo a la misma carpeta en la que está, moviendo? no se hace nada
        mov eax, t_moviendo
        cmp dword [op_mover], 0
        jne .t
        mov eax, t_copiando
.t:
        mov [op_verbo], eax
        call progreso_abrir
        ; lo que hay que hacer: octetos y archivos (para la barra)
        mov dword [pr_total], 0
        mov dword [pr_nt], 0
        mov esi, t_contando
        mov edi, pr_l1
        mov ecx, 90
        call copiar_n
        mov byte [pr_l2], 0
        mov esi, fuentes
        mov ecx, [n_fuentes]
.c:
        call contar_uno
        jc .fin
        add esi, RUTA_TAM
        loop .c
        mov dword [pr_hecho], 0
        mov dword [pr_n], 0
        ; uno a uno
        mov esi, fuentes
        mov ecx, [n_fuentes]
.u:
        push ecx
        push esi
        call op_uno
        pop esi
        pop ecx
        cmp eax, R_CANCELA
        je .fin
        add esi, RUTA_TAM
        loop .u
.fin:
        call progreso_cerrar
        ; la lista y el árbol, al día; lo que ha llegado aquí, elegido
        call refrescar
        mov esi, op_dest
        mov edi, ruta
        call igual
        jne .r
        cmp dword [modo_busca], 0
        jne .r
        cmp dword [op_hechos], 0
        je .r
        xor eax, eax
        call sel_todos
        mov esi, op_nombres
        mov ecx, [op_hechos]
        cmp ecx, 64
        jbe .m
        mov ecx, 64
.m:
        push esi
        call lista_buscar_nombre
        jc .m1
        mov [foco_i], eax
        shl eax, 5
        or byte [ents+eax+E_MARCA], 1
.m1:
        pop esi
        add esi, 13
        loop .m
        call lista_ver_foco
        call lista_inv
        call estado_poner
.r:
        mov esi, op_dest
        call arbol_recargar
        cmp dword [op_mover], 0
        je .x
        mov esi, fuentes            ; (la carpeta de donde se ha movido)
        mov edi, tmp
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov esi, tmp
        call ruta_subir
        call arbol_recargar
.x:
        popad
        ret

; (interna) ESI=una fuente: suma lo que hay que copiar -> CF=1 si se cancela
contar_uno:
        pushad
        cmp dword [op_mover], 0     ; mover en la misma unidad: cada uno cuenta 1
        je .cop
        mov al, [esi]
        cmp al, [op_dest]
        jne .cop
        inc dword [pr_total]
        inc dword [pr_nt]
        jmp .ok
.cop:
        call d_atr_leer
        jc .ok                      ; (ya se dirá al copiar)
        test cl, 0x10
        jnz .dir
        push esi
        mov al, 0                   ; su tamaño: lo dice la búsqueda
        mov ecx, 0x27
        mov edi, dta_w
        call d_primero
        pop esi
        jc .ok
        mov eax, [dta_w+DTA_TAMANO]
        add [pr_total], eax
        inc dword [pr_nt]
        jmp .ok
.dir:
        mov ebx, cb_contar
        call recorrer
        jc .no
.ok:
        call progreso_paso
        jc .no
        popad
        clc
        ret
.no:
        popad
        stc
        ret
cb_contar:
        cmp eax, 1
        jne .x
        mov eax, [edx+DTA_TAMANO]
        add [pr_total], eax
        inc dword [pr_nt]
.x:
        jmp progreso_paso

; (interna) ESI=una fuente: la copia o la mueve a op_dest -> EAX=R_*
op_uno:
        mov [op_src], esi
        mov esi, [op_src]           ; el nombre y el destino
        call ruta_nombre
        mov esi, eax
        mov edi, op_nom
        mov ecx, 12
        call copiar_n
        ; ¿a la misma carpeta? (mover: nada; copiar: con otro nombre)
        mov esi, [op_src]
        mov edi, tmp
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov esi, tmp
        call ruta_subir
        mov edi, op_dest
        call igual
        jne .otra
        mov eax, R_OMITIDO
        cmp dword [op_mover], 0
        jne .x
        call nombre_copia
.otra:
        mov edi, op_dst
        mov esi, op_dest
        mov ebx, op_nom
        call ruta_unir
        mov esi, op_dst
        call largo
        cmp eax, 66
        jbe .l
        mov esi, t_e_larga
        call aviso
        mov eax, R_OMITIDO
        jmp .x
.l:
        mov esi, [op_src]
        mov edi, op_dst
        cmp dword [op_mover], 0
        je .copia
        mov al, [esi]
        cmp al, [edi]
        jne .cruzado
        call mover_uno              ; en la misma unidad
        jmp .hecho
.cruzado:
        mov dword [op_fallos], 0
        call copiar_uno
        cmp eax, R_BIEN
        jne .x
        cmp dword [op_fallos], 0    ; (si algo no se copió, no se borra nada)
        jne .x
        mov esi, [op_src]
        call borrar_uno
        jmp .hecho
.copia:
        call copiar_uno
.hecho:
        cmp eax, R_BIEN
        jne .x
        mov edx, [op_hechos]        ; el nombre, para elegirlo luego
        cmp edx, 64
        jae .h
        imul edi, edx, 13
        add edi, op_nombres
        mov esi, op_nom
        mov ecx, 12
        call copiar_n
.h:
        inc dword [op_hechos]
        xor eax, eax
.x:
        ret

; (interna) op_nom: un nombre que no esté en op_dest («NOMBR~1.TXT»)
nombre_copia:
        pushad
        mov esi, op_nom
        mov edi, nc_base            ; la base (hasta 6) y la extensión
        mov ecx, 6
.b:
        lodsb
        test al, al
        jz .b1
        cmp al, '.'
        je .b2
        dec ecx
        js .b
        stosb
        jmp .b
.b1:
        dec esi
.b2:
        mov byte [edi], 0
        mov edi, nc_ext
        dec esi
        mov ecx, 4
        call copiar_n               ; «.TXT» (o nada)
        cmp byte [nc_ext], '.'
        je .n
        mov byte [nc_ext], 0
.n:
        mov ebx, 1
.p:
        mov edi, op_nom
        mov esi, nc_base
        call hvl_cad
        mov al, '~'
        stosb
        mov eax, ebx
        call hvl_num
        mov esi, nc_ext
        call hvl_cad
        mov byte [edi], 0
        mov edi, tmp
        mov esi, op_dest
        push ebx
        mov ebx, op_nom
        call ruta_unir
        pop ebx
        mov esi, tmp
        call d_atr_leer
        jc .x                       ; no existe: ese
        inc ebx
        cmp ebx, 10
        jne .p1
        mov byte [nc_base+5], 0     ; (dos cifras: una letra menos)
.p1:
        cmp ebx, 99
        jbe .p
.x:
        call crit_olvidar
        popad
        ret

; (interna) ESI=origen EDI=destino: copia un archivo o una carpeta entera
copiar_uno:
        mov [cu_src], esi
        mov [cu_dst], edi
        call d_atr_leer
        jnc .a
        mov ecx, eax                ; (no se puede leer: lo dice copiar_archivo)
        xor ecx, ecx
.a:
        test cl, 0x10
        jnz .dir
        jmp copiar_archivo
.dir:
        ; la carpeta de destino (si ya está, se juntan)
        mov esi, [cu_dst]
        call crear_dir_dest
        cmp eax, R_BIEN
        jne .x
        mov esi, [cu_src]
        call largo
        mov [cu_lsrc], eax
        mov ebx, cb_copiar
        call recorrer
        mov eax, R_BIEN
        jnc .x
        mov eax, R_CANCELA
.x:
        ret
; recorrer: cada archivo, a su sitio; cada carpeta, creada
cb_copiar:
        push eax
        mov esi, [cu_dst]           ; el destino: cu_dst + lo que sigue a cu_src
        mov edi, cu_otro
        call hvl_cad
        mov esi, w_ruta
        add esi, [cu_lsrc]
        call hvl_cad
        mov byte [edi], 0
        pop eax
        cmp eax, 3
        je .bien
        cmp eax, 2
        je .dir
        mov esi, w_ruta
        mov edi, cu_otro
        call copiar_archivo
        jmp .r
.dir:
        mov esi, cu_otro
        call crear_dir_dest
.r:
        cmp eax, R_CANCELA
        je .no
        cmp eax, R_OMITIDO
        jne .bien
        inc dword [op_fallos]
.bien:
        clc
        ret
.no:
        stc
        ret

; (interna) ESI=carpeta de destino: la crea si no está -> EAX=R_*
crear_dir_dest:
        pushad
.o:
        call d_atr_leer
        jc .crea
        test cl, 0x10
        jnz .bien
        mov eax, 0x50               ; (es un archivo)
        jmp .mal
.crea:
        call crit_olvidar
        call d_crear_dir
        jnc .bien
.mal:
        mov edi, t_no_crear
        call preguntar_error
        cmp eax, ID_REINT
        je .o
        mov edx, R_OMITIDO
        cmp eax, ID_OMITIR
        je .r
        mov edx, R_CANCELA
        jmp .r
.bien:
        xor edx, edx
.r:
        mov [esp+28], edx
        popad
        ret

; (interna) ESI=archivo de origen EDI=destino -> EAX=R_*
copiar_archivo:
        pushad
        mov [ca_src], esi
        mov [ca_dst], edi
        ; los textos de la ventana de progreso
        call ruta_nombre
        mov edi, pr_l1
        mov esi, [op_verbo]
        call hvl_cad
        mov esi, eax
        call hvl_cad
        mov byte [edi], 0
        mov edi, pr_l2
        mov esi, t_a_
        call hvl_cad
        mov esi, [ca_dst]
        mov ecx, edi
        mov edi, tmp
        push ecx
        mov ecx, RUTA_TAM - 1
        call copiar_n
        pop edi
        mov esi, tmp
        call ruta_subir
        call hvl_cad
        mov byte [edi], 0
        call progreso_texto
.otra:
        call crit_olvidar
        mov esi, [ca_src]
        call d_atr_leer
        jc .err_src
        mov [ca_atr], ecx
        xor al, al
        HV dos_abrir
        jc .err_src
        mov [ca_hs], eax
        mov ebx, eax
        call d_fecha_leer
        mov [ca_hora], ecx
        mov [ca_fecha], edx
        ; ¿ya está el destino?
        mov esi, [ca_dst]
        call d_atr_leer
        jc .crea
        test cl, 0x10
        jz .existe
        mov eax, 0x50
        jmp .err_cerrar
.existe:
        cmp dword [op_todos], 0
        jne .encima
        push ecx
        call preguntar_encima
        pop ecx
        cmp eax, ID_TODOS
        jne .e1
        mov dword [op_todos], 1
        jmp .encima
.e1:
        cmp eax, ID_SI
        je .encima
        mov ebx, [ca_hs]
        HV dos_cerrar
        mov edx, R_OMITIDO
        cmp eax, ID_NO
        je .r
        mov edx, R_CANCELA
        jmp .r
.encima:
        test cl, 7                  ; solo lectura, oculto o sistema: fuera
        jz .crea
        push ecx
        xor ecx, ecx
        mov esi, [ca_dst]
        call d_atr_poner
        pop ecx
.crea:
        call crit_olvidar
        mov esi, [ca_dst]
        HV dos_crear
        jc .err_cerrar
        mov [ca_hd], eax
        ; los datos, a trozos
.t:
        mov ebx, [ca_hs]
        mov edi, buf_copia
        mov ecx, BUF_COPIA
        HV dos_leer
        jc .err_ambos
        test eax, eax
        jz .acabado
        mov ecx, eax
        mov ebx, [ca_hd]
        mov esi, buf_copia
        HV dos_escribir
        jc .err_escr
        cmp eax, ecx
        jne .lleno
        add [pr_hecho], ecx
        call progreso_paso
        jc .cancelado
        cmp ecx, BUF_COPIA
        je .t
.acabado:
        mov ebx, [ca_hd]
        mov ecx, [ca_hora]
        mov edx, [ca_fecha]
        call d_fecha_poner
        HV dos_cerrar
        mov ebx, [ca_hs]
        HV dos_cerrar
        mov ecx, [ca_atr]           ; los atributos, los mismos
        and ecx, 0x27
        test ecx, 0x07
        jz .sin
        mov esi, [ca_dst]
        call d_atr_poner
.sin:
        inc dword [pr_n]
        mov edx, R_BIEN
        jmp .r
.lleno:
        mov eax, 0x1D
.err_escr:
.err_ambos:
        push eax
        mov ebx, [ca_hd]
        HV dos_cerrar
        mov esi, [ca_dst]           ; lo que se ha quedado a medias, fuera
        call d_borrar
        pop eax
.err_cerrar:
        push eax
        mov ebx, [ca_hs]
        HV dos_cerrar
        pop eax
.err_src:
        mov esi, [ca_src]
        mov edi, t_no_copia
        cmp dword [op_mover], 0
        je .e2
        mov edi, t_no_mover
.e2:
        call preguntar_error_nom
        cmp eax, ID_REINT
        je .otra
        mov edx, R_OMITIDO
        cmp eax, ID_OMITIR
        je .r
        mov edx, R_CANCELA
        jmp .r
.cancelado:
        mov ebx, [ca_hd]
        HV dos_cerrar
        mov ebx, [ca_hs]
        HV dos_cerrar
        mov esi, [ca_dst]
        call d_borrar
        mov edx, R_CANCELA
.r:
        call crit_olvidar
        mov [esp+28], edx
        popad
        ret

; (interna) ESI=origen EDI=destino, en la misma unidad: cambiarlo de sitio
; (un archivo, con su nombre; una carpeta, se crea en el destino, se le pasa
; todo lo de dentro y se quita la de antes) -> EAX=R_*
mover_uno:
        mov [cu_src], esi
        mov [cu_dst], edi
        call d_atr_leer
        jc .archivo
        test cl, 0x10
        jnz .dir
.archivo:
        jmp mover_archivo
.dir:
        mov esi, [cu_dst]
        call crear_dir_dest
        cmp eax, R_BIEN
        jne .x
        mov esi, [cu_src]
        call largo
        mov [cu_lsrc], eax
        mov dword [op_fallos], 0
        mov ebx, cb_mover
        call recorrer
        mov eax, R_CANCELA
        jc .x
        cmp dword [op_fallos], 0
        mov eax, R_OMITIDO
        jne .x
        mov esi, [cu_src]
        call quitar_dir
.x:
        ret
cb_mover:
        push eax
        mov esi, [cu_dst]
        mov edi, cu_otro
        call hvl_cad
        mov esi, w_ruta
        add esi, [cu_lsrc]
        call hvl_cad
        mov byte [edi], 0
        pop eax
        cmp eax, 2
        je .dir
        cmp eax, 3
        je .sale
        mov esi, w_ruta
        mov edi, cu_otro
        call mover_archivo
        jmp .r
.dir:
        mov esi, cu_otro
        call crear_dir_dest
        jmp .r
.sale:
        cmp dword [op_fallos], 0
        jne .bien
        mov esi, w_ruta             ; la carpeta de antes, ya vacía
        call quitar_dir
.r:
        cmp eax, R_CANCELA
        je .no
        cmp eax, R_OMITIDO
        jne .bien
        inc dword [op_fallos]
.bien:
        clc
        ret
.no:
        stc
        ret

; (interna) ESI=origen EDI=destino (misma unidad) -> EAX=R_*
mover_archivo:
        pushad
        mov [ca_src], esi
        mov [ca_dst], edi
        call ruta_nombre
        mov edi, pr_l1
        mov esi, [op_verbo]
        call hvl_cad
        mov esi, eax
        call hvl_cad
        mov byte [edi], 0
        call progreso_texto
.otra:
        call crit_olvidar
        mov esi, [ca_dst]           ; ¿ya está?
        call d_atr_leer
        jc .mueve
        test cl, 0x10
        mov eax, 0x50
        jnz .err
        cmp dword [op_todos], 0
        jne .encima
        call preguntar_encima
        cmp eax, ID_TODOS
        jne .e1
        mov dword [op_todos], 1
        jmp .encima
.e1:
        cmp eax, ID_SI
        je .encima
        mov edx, R_OMITIDO
        cmp eax, ID_NO
        je .r
        mov edx, R_CANCELA
        jmp .r
.encima:
        xor ecx, ecx
        mov esi, [ca_dst]
        call d_atr_poner
        call d_borrar
.mueve:
        call crit_olvidar
        mov esi, [ca_src]
        mov edi, [ca_dst]
        call d_renombrar
        jc .err
        inc dword [pr_n]
        inc dword [pr_hecho]
        call progreso_paso
        mov edx, R_BIEN
        jnc .r
        mov edx, R_CANCELA
        jmp .r
.err:
        mov esi, [ca_src]
        mov edi, t_no_mover
        call preguntar_error_nom
        cmp eax, ID_REINT
        je .otra
        mov edx, R_OMITIDO
        cmp eax, ID_OMITIR
        je .r
        mov edx, R_CANCELA
.r:
        call crit_olvidar
        mov [esp+28], edx
        popad
        ret

; (interna) ESI=carpeta vacía: la quita (si no se puede, lo dice) -> EAX=R_*
quitar_dir:
        pushad
.o:
        call crit_olvidar
        call d_atr_leer             ; (una carpeta de solo lectura u oculta)
        jc .q
        test cl, 7
        jz .q
        xor ecx, ecx
        call d_atr_poner
.q:
        call d_quitar_dir
        jnc .bien
        mov edi, t_no_borrar
        call preguntar_error_nom
        cmp eax, ID_REINT
        je .o
        mov edx, R_OMITIDO
        cmp eax, ID_OMITIR
        je .r
        mov edx, R_CANCELA
        jmp .r
.bien:
        xor edx, edx
.r:
        mov [esp+28], edx
        popad
        ret

; (interna) ESI=archivo o carpeta: lo borra (una carpeta, con todo) -> EAX=R_*
borrar_uno:
        mov [cu_src], esi
        call d_atr_leer
        jc borrar_archivo
        test cl, 0x10
        jz borrar_archivo
        mov dword [op_fallos], 0
        mov ebx, cb_borrar
        call recorrer
        mov eax, R_CANCELA
        jc .x
        mov eax, R_OMITIDO
        cmp dword [op_fallos], 0
        jne .x
        mov esi, [cu_src]
        call quitar_dir
.x:
        ret
cb_borrar:
        cmp eax, 2
        je .bien
        mov esi, w_ruta
        cmp eax, 3
        je .dir
        call borrar_archivo
        jmp .r
.dir:
        cmp dword [op_fallos], 0
        jne .bien
        call quitar_dir
.r:
        cmp eax, R_CANCELA
        je .no
        cmp eax, R_OMITIDO
        jne .bien
        inc dword [op_fallos]
.bien:
        clc
        ret
.no:
        stc
        ret

; (interna) ESI=archivo: fuera (aunque sea de solo lectura) -> EAX=R_*
borrar_archivo:
        pushad
        mov [ca_src], esi
        call ruta_nombre
        mov edi, pr_l1
        mov esi, t_eliminando
        call hvl_cad
        mov esi, eax
        call hvl_cad
        mov byte [edi], 0
        call progreso_texto
.o:
        call crit_olvidar
        mov esi, [ca_src]
        call d_atr_leer
        jc .b
        test cl, 7
        jz .b
        xor ecx, ecx
        call d_atr_poner
.b:
        call d_borrar
        jc .err
        inc dword [pr_n]
        call progreso_paso
        mov edx, R_BIEN
        jnc .r
        mov edx, R_CANCELA
        jmp .r
.err:
        mov edi, t_no_borrar
        call preguntar_error_nom
        cmp eax, ID_REINT
        je .o
        mov edx, R_OMITIDO
        cmp eax, ID_OMITIR
        je .r
        mov edx, R_CANCELA
.r:
        call crit_olvidar
        mov [esp+28], edx
        popad
        ret

; ---- preguntas de las operaciones ----
; EAX=error ESI=ruta EDI=«No se ha podido copiar »: con el nombre y el
; motivo; Reintentar, Omitir o Cancelar -> EAX
preguntar_error_nom:
        push ebx
        push esi
        push edi
        call err_texto
        mov ebx, esi                ; el motivo
        mov esi, [esp+4]
        call ruta_nombre
        mov edx, eax
        mov esi, edi
        mov edi, msg_txt
        call hvl_cad
        mov esi, t_comillas1
        call hvl_cad
        mov esi, edx
        call hvl_cad
        mov esi, t_comillas2
        call hvl_cad
        mov esi, t_dos_puntos
        call hvl_cad
        mov esi, ebx
        call hvl_cad
        mov byte [edi], 0
        mov esi, msg_txt
        mov edi, bot_omitir
        call preguntar
        pop edi
        pop esi
        pop ebx
        ret
; EAX=error EDI=texto de delante (con su fin de línea): el motivo detrás
preguntar_error:
        push ebx
        push esi
        push edi
        call err_texto
        mov ebx, esi
        mov esi, edi
        mov edi, msg_txt
        call hvl_cad
        mov esi, ebx
        call hvl_cad
        mov byte [edi], 0
        mov esi, msg_txt
        mov edi, bot_omitir
        call preguntar
        pop edi
        pop esi
        pop ebx
        ret
; [ca_dst] ya existe: ¿encima? -> EAX = ID_SI, ID_NO, ID_TODOS o ID_CANCELAR
preguntar_encima:
        push esi
        push edi
        mov esi, [ca_dst]
        call ruta_nombre
        mov edi, msg_txt
        mov esi, t_sobrescr1
        call hvl_cad
        mov esi, eax
        call hvl_cad
        mov esi, t_sobrescr2
        call hvl_cad
        mov byte [edi], 0
        mov esi, msg_txt
        mov edi, bot_sobre
        mov dword [pg_tit], t_t_sobre
        call preguntar
        pop edi
        pop esi
        ret

; =============================================================================
;  Recorrer una carpeta con todo lo que tiene dentro
; =============================================================================
; ESI=carpeta EBX=función: la llama con EAX=1 por cada archivo, 2 al entrar en
; una carpeta (antes de mirar dentro) y 3 al salir de ella; w_ruta = la ruta
; de lo que se mira y EDX = su DTA (atributos, tamaño, nombre). Si la función
; devuelve CF=1, se para -> CF=1. Las carpetas de dentro que no se pueden
; leer se saltan
recorrer:
        pushad
        mov [w_fn], ebx
        mov edi, w_ruta
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov dword [w_n], 0
        call .abrir
.b:
        mov ebx, [w_n]
        cmp byte [w_hecho+ebx], 0
        jne .sube
        imul edx, ebx, DTA_TAM
        add edx, w_dtas
        test byte [edx+DTA_ATR], 8
        jnz .sig
        cmp byte [edx+DTA_NOMBRE], '.'
        je .sig
        ; la ruta, con el nombre
        mov esi, w_ruta
        call largo
        lea edi, [w_ruta+eax]
        cmp byte [edi-1], '\'
        je .r1
        mov al, '\'
        stosb
.r1:
        lea esi, [edx+DTA_NOMBRE]
        call hvl_cad
        mov byte [edi], 0
        sub edi, w_ruta
        cmp edi, 78
        ja .corta
        test byte [edx+DTA_ATR], 0x10
        jnz .dir
        mov eax, 1
        call .fn
        jc .para
.corta:
        call .cortar
        jmp .sig
.dir:
        mov eax, 2
        call .fn
        jc .para
        mov ebx, [w_n]
        inc ebx
        cmp ebx, W_NIV
        jae .corta
        mov [w_n], ebx
        call .abrir
        jmp .b
.sig:
        mov ebx, [w_n]
        imul edi, ebx, DTA_TAM
        add edi, w_dtas
        call d_siguiente
        jnc .b
        mov byte [w_hecho+ebx], 1
        jmp .b
.sube:
        mov ebx, [w_n]
        test ebx, ebx
        jz .fin
        mov eax, 3                  ; (w_ruta es la carpeta de la que se sale)
        imul edx, ebx, DTA_TAM
        add edx, w_dtas
        call .fn
        jc .para
        dec dword [w_n]
        call .cortar
        jmp .sig
.fin:
        popad
        clc
        ret
.para:
        popad
        stc
        ret
.fn:
        pushad
        call [w_fn]
        popad
        ret
.cortar:                            ; w_ruta, hasta la carpeta del nivel w_n
        push eax
        mov eax, [w_n]
        mov eax, [w_largo+eax*4]
        mov byte [w_ruta+eax], 0
        pop eax
        ret
.abrir:                             ; la carpeta w_ruta, en el nivel w_n
        pushad
        mov ebx, [w_n]
        mov esi, w_ruta
        call largo
        mov [w_largo+ebx*4], eax
        mov byte [w_hecho+ebx], 0
        mov edi, w_patron
        mov ebx, t_patron
        call ruta_unir
        mov ebx, [w_n]
        imul edi, ebx, DTA_TAM
        add edi, w_dtas
        mov esi, w_patron
        mov ecx, 0x16
        call d_primero
        jnc .ab
        mov byte [w_hecho+ebx], 1
.ab:
        popad
        ret

; =============================================================================
;  La ventana de progreso (con «Cancelar»); mientras está, la del
;  Administrador no atiende al ratón ni al teclado
; =============================================================================
progreso_abrir:
        pushad
        mov dword [ocupado], 1
        mov dword [cancelar], 0
        mov dword [pr_hecho], 0
        mov dword [pr_total], 0
        mov dword [pr_n], 0
        mov dword [pr_nt], 0
        mov byte [pr_l1], 0
        mov byte [pr_l2], 0
        HV pantalla
        sub eax, [prog_plant+P_AN]
        sar eax, 1
        mov [prog_plant+P_X], eax
        sub ebx, [prog_plant+P_AL]
        sar ebx, 1
        mov [prog_plant+P_Y], ebx
        mov esi, prog_plant
        xor ebp, ebp
        HV ven_crear
        mov [v_prog], eax
        mov ebp, [ven]
        or dword [ebp+V_ESTADO], S_INACTIVA
        mov ebp, [v_prog]
        test ebp, ebp
        jz .x
        mov [dlg_dueno], ebp
        HV ven_activar
        HV tics
        mov [pr_tic], eax
        call bombear
.x:
        popad
        ret

progreso_cerrar:
        pushad
        mov ebp, [ven]
        and dword [ebp+V_ESTADO], ~S_INACTIVA
        mov [dlg_dueno], ebp
        mov ebp, [v_prog]
        test ebp, ebp
        jz .a
        mov dword [v_prog], 0
        HV ven_destruir
.a:
        mov ebp, [ven]
        HV ven_activar
        mov dword [ocupado], 0
        mov ebp, [v_lista]
        cmp dword [ult_panel], 1
        jne .f
        mov ebp, [v_arbol]
.f:
        HV ven_enfocar
        cmp dword [salir_pend], 0   ; (se pidió salir mientras tanto)
        je .x
        HV msg_salir
.x:
        popad
        ret

; si ha pasado un tic de reloj: se repinta y se atiende a lo demás -> CF=1 si
; se ha pulsado «Cancelar»
progreso_paso:
        push eax
        HV tics
        cmp eax, [pr_tic]
        je .x
        mov [pr_tic], eax
        call progreso_texto
        call bombear
.x:
        pop eax
        cmp dword [cancelar], 0
        jne .no
        clc
        ret
.no:
        stc
        ret

; los textos han cambiado: a repintar
progreso_texto:
        push ebp
        mov ebp, [v_prog]
        test ebp, ebp
        jz .x
        HV ven_invalidar
.x:
        pop ebp
        ret

; se atiende a los mensajes que hay y a las demás tareas, y se vuelve
bombear:
        pushad
.l:
        mov edi, msg_b
        HV msg_mirar
        jc .f
        cmp dword [msg_b+M_MSG], WM_QUIT
        jne .d
        mov dword [salir_pend], 1
        mov dword [cancelar], 1
        jmp .l
.d:
        HV msg_despachar
        jmp .l
.f:
        HV ceder
        popad
        ret

prog_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_COMMAND
        je .cancelar
        cmp eax, WM_CLOSE
        je .cancelar
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CREATE
        jne .nc
        mov esi, prog_hijos
        HV ven_lista
        jmp hecho
.nc:
        HVJ ven_defecto
.tecla:
        cmp ecx, TC_ESC
        jne hecho
.cancelar:
        mov dword [cancelar], 1
        jmp hecho
.pintar:
        HV dc_pintar
        HV ven_cli
        mov [pr_an], ecx
        COLOR C_TINTA
        mov eax, 16
        mov ebx, 12
        mov esi, pr_l1
        sub ecx, 32
        shr ecx, 3
        mov edi, ecx
        call texto_corto
        mov ebx, 32
        mov esi, pr_l2
        call texto_corto
        ; la barra
        mov eax, 16
        mov ebx, 56
        mov ecx, [pr_an]
        sub ecx, 32
        mov edx, 16
        COLOR C_BLANCO
        HV dc_rect
        COLOR C_MARRON
        HV dc_marco
        mov eax, [pr_hecho]
        mov ecx, [pr_total]
        test ecx, ecx
        jz .t
        cmp eax, ecx
        jbe .b1
        mov eax, ecx
.b1:
        mov edx, [pr_an]
        sub edx, 36
        mul edx
        div ecx
        test eax, eax
        jz .t
        mov ecx, eax
        COLOR C_ANIL
        mov eax, 18
        mov ebx, 58
        mov edx, 12
        HV dc_rect
.t:
        ; «3 de 10 archivos»
        cmp dword [pr_nt], 0
        je .f
        mov edi, pr_l3
        mov eax, [pr_n]
        call pon_miles
        mov esi, t_de
        call hvl_cad
        mov eax, [pr_nt]
        call pon_miles
        mov esi, t_archivos_
        call hvl_cad
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 16
        mov ebx, 80
        mov esi, pr_l3
        HV dc_texto
.f:
        HV dc_cerrar
        jmp hecho

; =============================================================================
;  Eliminar
; =============================================================================
o_eliminar:
        call fuentes_poner
        jc .x
        call crit_olvidar
        ; la pregunta
        mov edi, msg_txt
        cmp dword [n_fuentes], 1
        jne .varios
        mov esi, t_borrar1
        call hvl_cad
        mov esi, fuentes
        call ruta_nombre
        mov esi, eax
        call hvl_cad
        mov esi, t_borrar2
        call hvl_cad
        mov esi, fuentes
        call d_atr_leer
        jc .p
        test cl, 0x10
        jz .p
        mov esi, t_borrar_c
        call hvl_cad
        jmp .p
.varios:
        mov esi, t_borrar_n1
        call hvl_cad
        mov eax, [n_fuentes]
        call hvl_num
        mov esi, t_borrar_n2
        call hvl_cad
        mov esi, t_borrar_c
        call hvl_cad
.p:
        mov byte [edi], 0
        mov esi, msg_txt
        mov edi, bot_borrar
        mov dword [pg_tit], t_t_borrar
        call preguntar
        cmp eax, ID_SI
        jne .x
        call progreso_abrir
        mov eax, [n_fuentes]
        mov [pr_total], eax
        mov esi, fuentes
        mov ecx, [n_fuentes]
.u:
        push ecx
        push esi
        call borrar_uno
        pop esi
        pop ecx
        inc dword [pr_hecho]
        cmp eax, R_CANCELA
        je .fin
        add esi, RUTA_TAM
        loop .u
.fin:
        call progreso_cerrar
        ; si se ha borrado la carpeta que se ve (desde el árbol), a la de arriba
        mov esi, fuentes
        mov edi, ruta
        call ruta_dentro
        jne .r
        mov edi, tmp
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov esi, tmp
        call ruta_subir
        call ir_a
.r:
        call refrescar
        mov esi, fuentes
        mov edi, tmp
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov esi, tmp
        call ruta_subir
        call arbol_recargar
.x:
        ret

; =============================================================================
;  Cambiar el nombre y carpeta nueva
; =============================================================================
o_nombre:
        call fuentes_poner
        jc .x
        cmp dword [n_fuentes], 1
        jne .x
        mov esi, fuentes
        call ruta_nombre
        mov esi, eax
        mov edi, nb_nombre
        mov ecx, 12
        call copiar_n
        mov edi, nb_antes
        call copiar_n
.pide:
        mov ebp, [ven]
        mov esi, t_p_nombre
        mov edi, t_t_nombre
        mov ebx, nb_nombre
        mov ecx, 13
        HV dlg_pedir
        test eax, eax
        jz .x
        mov esi, nb_nombre
        call nombre_valido
        jnc .v
        mov esi, t_e_nombre
        call aviso
        jmp .pide
.v:
        mov edi, nb_antes
        call igual
        je .x
        mov esi, fuentes            ; la ruta nueva: la misma carpeta
        mov edi, nb_padre
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov esi, nb_padre
        call ruta_subir
        mov edi, op_dst
        mov ebx, nb_nombre
        call ruta_unir
        call crit_olvidar
        mov esi, fuentes
        mov edi, op_dst
        call d_renombrar
        jnc .hecho
        mov edi, t_no_nombre
        call err_texto
        mov ebx, esi
        mov esi, edi
        mov edi, msg_txt
        call hvl_cad
        mov esi, ebx
        call hvl_cad
        mov byte [edi], 0
        mov esi, msg_txt
        call aviso
        jmp .x
.hecho:
        ; si era la carpeta que se ve (o una de encima), a la de su nombre nuevo
        mov esi, fuentes
        mov edi, ruta
        call ruta_dentro
        jne .lista
        mov esi, fuentes            ; la ruta nueva: op_dst y lo que seguía
        call largo
        mov ecx, eax
        mov edi, tmp2
        mov esi, op_dst
        call hvl_cad
        lea esi, [ruta+ecx]
        call hvl_cad
        mov byte [edi], 0
        mov esi, tmp2
        mov edi, tmp_ruta2
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov esi, tmp_ruta2
        call ir_a
        jmp .arbol
.lista:
        call refrescar
        mov esi, nb_nombre
        call lista_marcar_nombre
.arbol:
        mov esi, nb_padre
        call arbol_recargar
        call pp_marcar_cortados
.x:
        ret

o_nueva:
        call objetivo
        cmp eax, 1
        jne .l
        mov eax, [sel_nodo]
        mov edi, nv_padre
        call arbol_ruta
        jmp .n
.l:
        cmp dword [modo_busca], 0
        jne .x
        mov esi, ruta
        mov edi, nv_padre
        mov ecx, RUTA_TAM - 1
        call copiar_n
.n:
        ; un nombre que no esté: NUEVA, NUEVA2...
        mov ebx, 1
.b:
        mov edi, nb_nombre
        mov esi, t_nueva_def
        call hvl_cad
        cmp ebx, 1
        je .b1
        mov eax, ebx
        call hvl_num
.b1:
        mov byte [edi], 0
        mov edi, op_dst
        mov esi, nv_padre
        push ebx
        mov ebx, nb_nombre
        call ruta_unir
        pop ebx
        mov esi, op_dst
        call d_atr_leer
        jc .pide
        inc ebx
        cmp ebx, 99
        jb .b
.pide:
        call crit_olvidar
        mov ebp, [ven]
        mov esi, t_p_nueva
        mov edi, t_t_nueva
        mov ebx, nb_nombre
        mov ecx, 13
        HV dlg_pedir
        test eax, eax
        jz .x
        mov esi, nb_nombre
        call nombre_valido
        jnc .v
        mov esi, t_e_nombre
        call aviso
        jmp .pide
.v:
        mov edi, op_dst
        mov esi, nv_padre
        mov ebx, nb_nombre
        call ruta_unir
        call crit_olvidar
        mov esi, op_dst
        call d_crear_dir
        jnc .hecho
        mov edi, t_no_crear
        call err_texto
        mov ebx, esi
        mov esi, edi
        mov edi, msg_txt
        call hvl_cad
        mov esi, ebx
        call hvl_cad
        mov byte [edi], 0
        mov esi, msg_txt
        call aviso
        jmp .x
.hecho:
        mov esi, nv_padre
        mov edi, ruta
        call igual
        jne .a
        call refrescar
        mov esi, nb_nombre
        call lista_marcar_nombre
.a:
        mov esi, nv_padre
        call arbol_recargar
.x:
        ret

; =============================================================================
;  Abrir y editar
; =============================================================================
o_abrir:
        call objetivo
        cmp eax, 1
        jne .l
        mov eax, [sel_nodo]
        jmp arbol_alternar
.l:
        call n_elegidos
        test eax, eax
        jz .x
        mov eax, [foco_i]           ; el del foco, si está elegido; si no, el primero
        cmp eax, [n_ents]
        jae .p
        mov ebx, eax
        shl ebx, 5
        test byte [ents+ebx+E_MARCA], 1
        jnz .h
.p:
        mov eax, edx
.h:
        mov [foco_i], eax
        mov edi, ab_ruta
        call ent_ruta
        shl eax, 5
        lea esi, [ents+eax]
        movzx eax, byte [esi+E_TIPO]
        cmp eax, TI_CARP
        jne .arch
        mov esi, ab_ruta
        jmp ir_a
.arch:
        mov edi, ab_ruta
        cmp eax, TI_PROG
        je lanzar_dos
        cmp eax, TI_ORD
        je lanzar_dos
        mov esi, n_bloc
        cmp eax, TI_TEXTO
        je abrir_con_int
        cmp eax, TI_MOD
        je .mod
        mov ebx, n_pintura
        cmp eax, TI_IMAGEN
        je .progs
        mov ebx, n_sonidos
        cmp eax, TI_SONIDO
        je .progs
        cmp eax, TI_CART
        je .cart
        ; no se sabe: ¿en el Bloc de notas?
        mov ebp, [ven]
        mov esi, t_e_noabre
        mov edi, t_titulo
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne .x
        mov esi, n_bloc
        mov edi, ab_ruta
        jmp abrir_con_int
.mod:
        mov esi, ab_ruta
        xor edi, edi
        jmp abrir_con
.progs:
        mov esi, d_progs
        jmp .con
.cart:
        mov ebx, n_hispanes
        mov esi, ab_ruta
        call largo
        cmp byte [esi+eax-3], 'N'
        je .c1
        mov ebx, n_hispasms
.c1:
        mov esi, d_hisp
.con:
        mov edi, ab_prog
        call ruta_unir
        mov esi, ab_prog
        mov edi, ab_ruta
        jmp abrir_con
.x:
        ret

o_editar:
        call objetivo
        cmp eax, 1
        je .x
        call n_elegidos
        test eax, eax
        jz .x
        mov eax, [foco_i]
        cmp eax, [n_ents]
        jae .x
        mov ebx, eax
        shl ebx, 5
        test byte [ents+ebx+E_ATR], 0x10
        jnz .x
        mov edi, ab_ruta
        call ent_ruta
        mov esi, n_bloc
        jmp abrir_con_int
.x:
        ret

; ESI=programa (un .HVP con su ruta) EDI=parámetros: si está, se abre
abrir_con:
        pushad
        xor al, al
        HV dos_abrir
        jc abrir_falta
        mov ebx, eax
        HV dos_cerrar
        popad
abrir_con_int:                      ; (un programa interno: «:BLOC»)
        pushad
        call crit_olvidar
        mov ebp, [ven]
        xor edx, edx
        mov ecx, PA_VOLVER
        xor ebx, ebx
        HV prog_abrir
        popad
        ret
abrir_falta:
        call crit_olvidar
        mov ebx, esi
        mov edi, msg_txt
        mov esi, t_e_falta
        call hvl_cad
        mov esi, ebx
        call hvl_cad
        mov byte [edi], 0
        mov esi, msg_txt
        call aviso
        popad
        ret

; EDI=ruta de un .COM, .EXE o .BAT: se ejecuta como un acceso directo (Hispa
; Ventanas sale a Hispa-DOS y, al acabar, vuelve y abre esto otra vez)
lanzar_dos:
        pushad
        call ini_guardar            ; (la carpeta y lo marcado, para volver)
        mov esi, mi_ruta
        mov edi, ab_reabrir
        call hvl_cad
        mov esi, t_param_r
        call hvl_cad
        mov byte [edi], 0
        mov esi, ab_ruta
        call largo
        mov ecx, PA_VOLVER
        cmp byte [esi+eax-1], 'T'   ; un .BAT: esperar una tecla al acabar
        jne .c
        or ecx, PA_PAUSA
.c:
        mov ebp, [ven]
        xor edi, edi
        xor edx, edx
        mov ebx, ab_reabrir
        HV prog_abrir
        popad
        ret

; =============================================================================
;  Propiedades
; =============================================================================
o_propiedades:
        call objetivo
        cmp eax, 1
        jne .l
        mov eax, [sel_nodo]
        call nodo_ptr
        test byte [eax+N_BAND], NB_UNIDAD
        jz .l
        mov esi, eax
        jmp prop_unidad
.l:
        call fuentes_poner
        jc .x
        call crit_olvidar
        mov dword [pr_atr], 0
        mov dword [pc_dirs], 0
        mov dword [pr_total], 0
        mov dword [pr_nt], 0
        ; los atributos del primero (y su fecha, si es uno)
        mov esi, fuentes
        call d_atr_leer
        jc .x
        and ecx, 0x37
        mov [pr_atr], ecx
        ; lo que ocupan (las carpetas, con lo de dentro)
        mov dword [pc_hay_dir], 0
        mov esi, fuentes
        mov ecx, [n_fuentes]
.c:
        push ecx
        call d_atr_leer
        mov eax, ecx
        pop ecx
        jc .cs
        test al, 0x10
        jz .ca
        inc dword [pc_hay_dir]
        inc dword [pc_dirs]
        push ecx
        mov ebx, cb_prop
        call recorrer
        pop ecx
        jmp .cs
.ca:
        push ecx
        push esi
        mov ecx, 0x27
        mov edi, dta_w
        call d_primero
        pop esi
        pop ecx
        jc .cs
        mov eax, [dta_w+DTA_TAMANO]
        add [pr_total], eax
        inc dword [pr_nt]
        mov eax, [dta_w+DTA_HORA]
        mov [pc_fh], eax
.cs:
        add esi, RUTA_TAM
        loop .c
        call crit_olvidar
        ; los textos
        mov edi, pc_v_nom
        cmp dword [n_fuentes], 1
        jne .varios
        mov esi, fuentes
        call ruta_nombre
        mov esi, eax
        call hvl_cad
        mov byte [edi], 0
        mov edi, pc_v_tipo          ; el tipo
        mov esi, t_ti0
        test byte [pr_atr], 0x10
        jnz .t1
        mov esi, fuentes
        call ruta_nombre
        mov esi, eax
        push edi
        mov edi, ent_aux
        mov ecx, 12
        call copiar_n
        mov eax, [pr_atr]
        mov [ent_aux+E_ATR], al
        call tipo_poner
        mov esi, edi
        call tipo_texto
        pop edi
        mov esi, pi_txt
.t1:
        call hvl_cad
        mov byte [edi], 0
        jmp .donde
.varios:
        mov eax, [n_fuentes]
        call hvl_num
        mov esi, t_varios
        call hvl_cad
        mov byte [edi], 0
        mov edi, pc_v_tipo
        mov byte [edi], 0
.donde:
        mov esi, fuentes            ; la carpeta
        mov edi, pc_v_donde
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov esi, edi
        call ruta_subir
        ; el tamaño
        mov edi, pc_v_tam
        mov eax, [pr_total]
        call pon_miles
        mov esi, t_octetos
        call hvl_cad
        mov byte [edi], 0
        ; lo que contiene (si hay carpetas)
        mov edi, pc_v_cont
        mov byte [edi], 0
        cmp dword [pc_hay_dir], 0
        je .f
        mov eax, [pr_nt]
        call pon_miles
        mov esi, t_archivos_
        call hvl_cad
        mov esi, t_coma_sp
        call hvl_cad
        mov eax, [pc_dirs]
        call pon_miles
        mov esi, t_carpetas_
        call hvl_cad
        mov byte [edi], 0
.f:
        mov edi, pc_v_fecha         ; la fecha (de uno)
        mov byte [edi], 0
        cmp dword [n_fuentes], 1
        jne .dlg
        test byte [pr_atr], 0x10
        jnz .dlg
        mov eax, [pc_fh]
        mov [ent_aux+E_HORA], eax
        mov esi, ent_aux
        call pon_fecha
        mov byte [edi], 0
.dlg:
        ; el diálogo
        mov dword [pc_tabla], pc_filas
        mov esi, pc_dlg_plant
        mov edi, pc_ctl
        call prop_controles
        mov ebp, [ven]
        mov esi, pc_dlg_plant
        mov edi, pc_ctl
        mov ebx, prop_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        ; los atributos que han cambiado, a todos
        mov eax, [pr_atr_nuevo]
        xor eax, [pr_atr]
        and eax, 0x27
        jz .x
        mov [pc_cambia], eax
        mov esi, fuentes
        mov ecx, [n_fuentes]
.a:
        push ecx
        call crit_olvidar
        call d_atr_leer
        jc .an
        mov eax, [pc_cambia]
        not eax
        and ecx, eax
        mov eax, [pr_atr_nuevo]
        and eax, [pc_cambia]
        or ecx, eax
        and ecx, 0x27
        call d_atr_poner
        jnc .as
.an:
        mov edi, t_no_atr
        call err_texto
        mov ebx, esi
        mov esi, edi
        mov edi, msg_txt
        call hvl_cad
        mov esi, ebx
        call hvl_cad
        mov byte [edi], 0
        mov esi, msg_txt
        call aviso
        pop ecx
        jmp .r
.as:
        pop ecx
        add esi, RUTA_TAM
        loop .a
.r:
        call refrescar
.x:
        ret
cb_prop:
        cmp eax, 2
        jne .a
        inc dword [pc_dirs]
        ret
.a:
        cmp eax, 1
        jne .x
        mov eax, [edx+DTA_TAMANO]
        add [pr_total], eax
        inc dword [pr_nt]
.x:
        clc
        ret

; ESI=nodo de una unidad: sus propiedades (espacio total, libre y ocupado)
prop_unidad:
        mov edi, pc_v_nom
        call unidad_nombre
        mov byte [edi], 0
        mov al, [esi+N_NOM]
        sub al, 'A'
        call crit_olvidar
        call d_libre
        jnc .l
        call err_texto
        mov edi, tmp_ruta
        mov ax, [esi+N_NOM]
        mov [edi], ax
        mov byte [edi+2], 0
        mov edi, t_e_unidad
        mov esi, t_e_preparada
        call aviso_ruta
        ret
.l:
        mov [pc_libre], eax
        mov [pc_totalk], edx
        mov edi, pc_v_tipo
        mov eax, edx
        call pon_kb
        mov byte [edi], 0
        mov edi, pc_v_donde
        mov eax, [pc_libre]
        call pon_kb
        mov byte [edi], 0
        mov edi, pc_v_tam
        mov eax, [pc_totalk]
        sub eax, [pc_libre]
        call pon_kb
        mov byte [edi], 0
        mov byte [pc_v_cont], 0
        mov byte [pc_v_fecha], 0
        mov dword [pc_tabla], pc_filas_u
        mov esi, pc_dlg_plant
        mov edi, pc_ctl
        call prop_controles_unidad
        mov ebp, [ven]
        mov esi, pc_dlg_plant
        mov edi, pc_ctl
        xor ebx, ebx
        xor edx, edx
        HV dlg_modal
        ret
t_usado     db "Ocupado:", 0
t_a_        db "a ", 0

; (interna) las plantillas del diálogo de propiedades: ESI=la del diálogo
; EDI=las de los controles
prop_controles:
        pushad
        call prop_base
        ; los atributos
        mov eax, [pc_y]
        add eax, 6
        mov [pc_y], eax
        mov edx, [hv_api + HV_c_estatico * 4]
        mov ecx, t_p_atr
        mov ebx, 16
        mov esi, 300
        push dword SS_NEGRITA
        call pc_uno
        add dword [pc_y], 22
        mov ebp, t_a_ro
        mov ebx, 24
        mov ecx, 31
        call pc_casilla
        mov ebp, t_a_oc
        mov ebx, 180
        mov ecx, 32
        call pc_casilla
        add dword [pc_y], 24
        mov ebp, t_a_si
        mov ebx, 24
        mov ecx, 33
        call pc_casilla
        mov ebp, t_a_ar
        mov ebx, 180
        mov ecx, 34
        call pc_casilla
        add dword [pc_y], 34
        call pc_botones
        popad
        ret
prop_controles_unidad:
        pushad
        call prop_base
        add dword [pc_y], 10
        call pc_botones
        popad
        ret
; (interna) la plantilla del diálogo y las filas de texto
prop_base:
        mov eax, [hv_api + HV_c_dialogo * 4]
        mov [esi+P_CLASE], eax
        mov dword [esi+P_TEXTO], t_t_props
        mov dword [esi+P_ESTILO], E_DIALOGO
        mov dword [esi+P_AN], 400
        mov dword [esi+P_ID], 0
        mov [pc_pl], esi
        mov [pc_p], edi
        mov dword [pc_y], 14
        mov esi, [pc_tabla]
.f:
        mov ecx, [esi]
        test ecx, ecx
        jz .x
        mov eax, [esi+4]
        cmp byte [eax], 0           ; (las filas vacías no salen)
        je .s
        push esi
        mov edx, [hv_api + HV_c_estatico * 4]
        mov ebx, 16
        mov esi, 90
        push dword SS_NEGRITA
        call pc_uno
        mov esi, [esp]
        mov ecx, [esi+4]
        mov ebx, 110
        mov esi, 270
        push dword 0
        call pc_uno
        pop esi
        add dword [pc_y], 20
.s:
        add esi, 8
        jmp .f
.x:
        ret
; (interna) EDX=clase ECX=texto EBX=x ESI=ancho [pila]=estilo: un control
; en pc_y
pc_uno:
        mov edi, [pc_p]
        mov [edi+P_CLASE], edx
        mov [edi+P_TEXTO], ecx
        mov eax, [esp+4]
        or eax, E_HIJA
        mov [edi+P_ESTILO], eax
        mov [edi+P_X], ebx
        mov eax, [pc_y]
        mov [edi+P_Y], eax
        mov [edi+P_AN], esi
        mov dword [edi+P_AL], 18
        mov dword [edi+P_ID], 90
        add dword [pc_p], P_TAM
        ret 4
; (interna) EBP=texto EBX=x ECX=id: una casilla en pc_y
pc_casilla:
        mov edi, [pc_p]
        mov eax, [hv_api + HV_c_boton * 4]
        mov [edi+P_CLASE], eax
        mov [edi+P_TEXTO], ebp
        mov dword [edi+P_ESTILO], E_HIJA | E_TABULA | BS_CASILLA
        mov [edi+P_X], ebx
        mov eax, [pc_y]
        mov [edi+P_Y], eax
        mov dword [edi+P_AN], 150
        mov dword [edi+P_AL], 20
        mov [edi+P_ID], ecx
        add dword [pc_p], P_TAM
        ret
; (interna) Aceptar y Cancelar, y el alto del diálogo
pc_botones:
        mov edi, [pc_p]
        mov eax, [hv_api + HV_c_boton * 4]
        mov [edi+P_CLASE], eax
        mov dword [edi+P_TEXTO], t_aceptar
        mov dword [edi+P_ESTILO], E_HIJA | E_TABULA | BS_DEFECTO
        mov dword [edi+P_X], 176
        mov eax, [pc_y]
        mov [edi+P_Y], eax
        mov dword [edi+P_AN], 100
        mov dword [edi+P_AL], 24
        mov dword [edi+P_ID], ID_ACEPTAR
        add edi, P_TAM
        mov eax, [hv_api + HV_c_boton * 4]
        mov [edi+P_CLASE], eax
        mov dword [edi+P_TEXTO], t_cancelar
        mov dword [edi+P_ESTILO], E_HIJA | E_TABULA
        mov dword [edi+P_X], 284
        mov eax, [pc_y]
        mov [edi+P_Y], eax
        mov dword [edi+P_AN], 100
        mov dword [edi+P_AL], 24
        mov dword [edi+P_ID], ID_CANCELAR
        add edi, P_TAM
        mov dword [edi], 0
        mov eax, [pc_y]
        add eax, 24 + 14 + TIT_H + 2 * BORDE
        mov edi, [pc_pl]
        mov [edi+P_AL], eax
        ret
            align 4
pc_filas    dd t_p_nom, pc_v_nom, t_p_tipo, pc_v_tipo, t_p_donde, pc_v_donde
            dd t_p_tam, pc_v_tam, t_p_cont, pc_v_cont, t_p_fecha, pc_v_fecha, 0
pc_filas_u  dd t_p_nom, pc_v_nom, t_p_total, pc_v_tipo, t_p_libre, pc_v_donde
            dd t_usado, pc_v_tam, 0

; el gancho: las casillas, al empezar y al aceptar
prop_gancho:
        cmp eax, DM_INICIO
        je .inicio
        cmp eax, WM_COMMAND
        jne .no
        cmp cx, ID_ACEPTAR
        jne .no
        xor eax, eax                ; se leen
        mov [pr_atr_nuevo], eax
        mov ebx, pc_bits
        mov ecx, 31
.l:
        push ecx
        HV ven_hija_id
        pop ecx
        test eax, eax
        jz .ls
        push ebp
        mov ebp, eax
        mov eax, BM_MARCA
        HV msg_enviar
        pop ebp
        test eax, eax
        jz .ls
        movzx eax, byte [ebx]
        or [pr_atr_nuevo], eax
.ls:
        inc ebx
        inc ecx
        cmp ecx, 35
        jb .l
.no:
        xor eax, eax
        ret
.inicio:
        mov ebx, pc_bits
        mov ecx, 31
.i:
        push ecx
        HV ven_hija_id
        pop ecx
        test eax, eax
        jz .is
        movzx edx, byte [ebx]
        push ecx
        xor ecx, ecx
        test [pr_atr], edx
        setnz cl
        push ebp
        mov ebp, eax
        mov eax, BM_PONER_MARCA
        HV msg_enviar
        pop ebp
        pop ecx
.is:
        inc ebx
        inc ecx
        cmp ecx, 35
        jb .i
        xor eax, eax
        ret
pc_bits     db 1, 2, 4, 0x20

; =============================================================================
;  Buscar
; =============================================================================
o_buscar:
        cmp byte [busca_pat], 0
        jne .p
        mov dword [busca_pat], '*.*'
.p:
        mov ebp, [ven]
        mov esi, t_p_buscar
        mov edi, t_t_buscar
        mov ebx, busca_pat
        mov ecx, 16
        HV dlg_pedir
        test eax, eax
        jz .x
        mov esi, busca_pat          ; en mayúsculas
.m:
        mov al, [esi]
        test al, al
        jz .m1
        call mayus
        mov [esi], al
        inc esi
        jmp .m
.m1:
        cmp byte [busca_pat], 0
        je .x
        cmp dword [modo_busca], 0   ; desde la carpeta que se ve
        jne buscar_otra_vez
        mov esi, ruta
        mov edi, busca_raiz
        mov ecx, RUTA_TAM - 1
        call copiar_n
        jmp buscar_otra_vez
.x:
        ret
buscar_otra_vez:
        call progreso_abrir
        mov edi, pr_l1
        mov esi, t_buscando
        call hvl_cad
        mov esi, busca_raiz
        call hvl_cad
        mov byte [edi], 0
        call progreso_texto
        mov dword [n_ents], 0
        mov dword [pool_n], 0
        mov dword [pool_ult], 0
        call crit_olvidar
        mov esi, busca_raiz
        mov ebx, cb_buscar
        call recorrer
        call progreso_cerrar
        mov esi, busca_raiz
        mov edi, ruta
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov dword [modo_busca], 1
        call ordenar
        xor eax, eax
        mov [foco_i], eax
        mov [ancla], eax
        mov [desp_l], eax
        call dir_poner
        call lista_medidas
        call lista_inv
        call estado_poner
        mov ebp, [v_lista]
        HV ven_enfocar
.x:
        ret
cb_buscar:
        cmp eax, 3
        je .bien
        mov [cbb_dta], edx
        cmp eax, 2                  ; la carpeta en la que se entra, en la ventana
        jne .c
        push edx
        mov edi, pr_l2
        mov esi, w_ruta
        mov ecx, 90
        call copiar_n
        call progreso_texto
        pop edx
.c:
        lea esi, [edx+DTA_NOMBRE]
        mov edi, busca_pat
        call coincide
        jne .sig
        mov ebx, [n_ents]
        cmp ebx, MAX_ENT
        jae .lleno
        ; su carpeta, en el depósito (la misma que la del anterior, si lo es)
        mov eax, [w_n]
        mov ecx, [w_largo+eax*4]
        call pool_carpeta
        jc .lleno
        mov edi, ebx
        shl edi, 5
        add edi, ents
        mov [edi+E_DIR], eax
        mov edx, [cbb_dta]
        lea esi, [edx+DTA_NOMBRE]
        push edi
        mov ecx, 12
        call copiar_n
        pop edi
        mov al, [edx+DTA_ATR]
        mov [edi+E_ATR], al
        mov byte [edi+E_MARCA], 0
        mov eax, [edx+DTA_TAMANO]
        mov [edi+E_TAM], eax
        mov eax, [edx+DTA_HORA]
        mov [edi+E_HORA], eax
        call tipo_poner
        inc dword [n_ents]
.sig:
        jmp progreso_paso
.lleno:
        stc
        ret
.bien:
        clc
        ret

; (interna) ECX=largo de la carpeta en w_ruta -> EAX=su texto en el depósito
pool_carpeta:
        pushad
        mov esi, [pool_ult]         ; ¿la misma que la última?
        test esi, esi
        jz .nueva
        mov edi, w_ruta
        push ecx
        repe cmpsb
        pop ecx
        jne .nueva
        cmp byte [esi], 0
        jne .nueva
        mov eax, [pool_ult]
        jmp .si
.nueva:
        mov eax, [pool_n]
        lea edx, [eax+ecx+1]
        cmp edx, POOL_TAM
        ja .no
        lea edi, [pool+eax]
        mov [pool_ult], edi
        mov esi, w_ruta
        push ecx
        rep movsb
        pop ecx
        mov byte [edi], 0
        mov [pool_n], edx
        mov eax, [pool_ult]
.si:
        mov [esp+28], eax
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; =============================================================================
;  Arrastrar y soltar
; =============================================================================
; EBP=la ventana de donde se arrastra EDX=x,y (en ella)
arr_mueve:
        cmp dword [arr_activo], 0
        jne .va
        cmp dword [arr_pend], 0
        je hecho
        movsx eax, dx               ; ¿se ha movido bastante? (5 puntos)
        movsx ebx, word [arr_x0]
        sub eax, ebx
        jns .a1
        neg eax
.a1:
        mov ecx, eax
        mov eax, edx
        sar eax, 16
        mov ebx, [arr_x0]
        sar ebx, 16
        sub eax, ebx
        jns .a2
        neg eax
.a2:
        add eax, ecx
        cmp eax, 5
        jl hecho
        mov dword [arr_activo], 1
        mov dword [arr_dtipo], 0
        or dword [arr_di], -1
.va:
        push edx
        HV ven_cli                  ; -> EAX, EBX: su esquina en la pantalla
        pop edx
        movsx ecx, dx
        add eax, ecx
        mov ecx, edx
        sar ecx, 16
        add ebx, ecx
        call arr_destino
        jmp hecho

; (interna) EAX, EBX: el punto de la pantalla -> arr_dtipo (0 nada, 1 un
; nodo del árbol, 2 una carpeta de la lista, 3 la carpeta que se ve),
; arr_di y arr_dest
arr_destino:
        pushad
        mov [ad_sx], eax
        mov [ad_sy], ebx
        mov dword [ad_t], 0
        or dword [ad_di], -1
        ; ¿en el árbol?
        mov ebp, [v_arbol]
        call .dentro
        jc .lista
        call arbol_en
        test eax, eax
        js .fin
        mov [ad_di], eax
        mov edi, arr_dest
        call arbol_ruta
        call arr_valido
        jc .fin
        mov dword [ad_t], 1
        jmp .fin
.lista:
        mov ebp, [v_lista]
        call .dentro
        jc .fin
        call lista_en
        cmp eax, -2
        je .fin
        test eax, eax
        js .fondo
        mov ebx, eax
        shl ebx, 5
        test byte [ents+ebx+E_ATR], 0x10
        jz .fondo
        cmp dword [arr_origen], 2   ; (lo que se arrastra no es destino)
        jne .l1
        test byte [ents+ebx+E_MARCA], 1
        jnz .fondo
.l1:
        mov [ad_di], eax
        mov edi, arr_dest
        call ent_ruta
        call arr_valido
        jc .fin
        mov dword [ad_t], 2
        jmp .fin
.fondo:
        or dword [ad_di], -1
        cmp dword [modo_busca], 0
        jne .fin
        mov esi, ruta
        mov edi, arr_dest
        mov ecx, RUTA_TAM - 1
        call copiar_n
        call arr_valido
        jc .fin
        mov dword [ad_t], 3
.fin:
        mov eax, [ad_t]
        mov ebx, [ad_di]
        cmp eax, [arr_dtipo]
        jne .cambia
        cmp ebx, [arr_di]
        je .estado
.cambia:
        mov [arr_dtipo], eax
        mov [arr_di], ebx
        call arbol_inv
        call lista_inv
.estado:
        ; la barra de estado: «Mover a ...» o «Copiar a ...»
        cmp dword [arr_dtipo], 0
        jne .e1
        call estado_poner
        jmp .x
.e1:
        mov edi, st_izq
        mov esi, t_copiar_a
        call arr_mover_si
        jnc .e2
        mov esi, t_mover_a
.e2:
        call hvl_cad
        mov esi, arr_dest
        call hvl_cad
        mov byte [edi], 0
        call estado_repintar
.x:
        popad
        ret
.dentro:                            ; EBP=ventana: el punto, en su zona -> EDX
        HV ven_cli
        mov esi, [ad_sx]
        sub esi, eax
        js .no
        cmp esi, ecx
        jge .no
        mov edi, [ad_sy]
        sub edi, ebx
        js .no
        cmp edi, edx
        jge .no
        mov edx, edi
        shl edx, 16
        mov dx, si
        clc
        ret
.no:
        stc
        ret

; (interna) ¿se puede soltar en arr_dest? CF=1 si no (es lo mismo que se
; arrastra, está dentro o es donde ya está)
arr_valido:
        pushad
        cmp dword [arr_origen], 1
        jne .lista
        mov eax, [arr_ai]           ; del árbol: esa carpeta
        mov edi, tmp2
        call arbol_ruta
        mov esi, tmp2
        mov edi, arr_dest
        call ruta_dentro
        je .no
        call ruta_subir
        jc .no
        call igual
        je .no
        jmp .si
.lista:
        cmp dword [modo_busca], 0
        jne .si
        mov esi, ruta               ; de la lista: no a la misma carpeta
        mov edi, arr_dest
        call igual
        je .no
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; (interna) -> CF=1 si soltar ahora es mover (misma unidad y sin Ctrl)
arr_mover_si:
        push eax
        HV tec_estado
        test eax, TM_CTRL
        jnz .copia
        mov al, [ruta]
        cmp dword [arr_origen], 1
        jne .u
        push edi
        mov eax, [arr_ai]
        mov edi, tmp2
        call arbol_ruta
        pop edi
        mov al, [tmp2]
.u:
        cmp al, [arr_dest]
        jne .copia
        pop eax
        stc
        ret
.copia:
        pop eax
        clc
        ret

; se suelta: se copia o se mueve
arr_soltar:
        pushad
        mov eax, [arr_dtipo]
        mov [ad_t], eax
        call arr_mover_si
        setc al
        movzx eax, al
        mov [ad_mover], eax
        call arr_limpiar
        cmp dword [ad_t], 0
        je .x
        call fuentes_poner
        jc .x
        mov esi, arr_dest
        mov edi, op_dest
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov eax, [ad_mover]
        call operacion
.x:
        call estado_poner
        popad
        ret

arr_cancelar:
        pushad
        mov ebp, [v_lista]
        cmp dword [arr_origen], 1
        jne .s
        mov ebp, [v_arbol]
.s:
        HV ven_soltar
        call arr_limpiar
        call estado_poner
        popad
        ret

arr_limpiar:
        mov dword [arr_activo], 0
        mov dword [arr_pend], 0
        mov dword [arr_origen], 0
        mov dword [arr_dtipo], 0
        or dword [arr_di], -1
        call arbol_inv
        jmp lista_inv

        section .bss
            alignb 4
n_fuentes   resd 1
pp_n        resd 1
pp_cortar   resd 1
op_mover    resd 1
op_todos    resd 1
op_hechos   resd 1
op_fallos   resd 1
op_verbo    resd 1
op_src      resd 1
cu_src      resd 1
cu_dst      resd 1
cu_lsrc     resd 1
ca_src      resd 1
ca_dst      resd 1
ca_atr      resd 1
ca_hs       resd 1
ca_hd       resd 1
ca_hora     resd 1
ca_fecha    resd 1
w_fn        resd 1
w_n         resd 1
w_largo     resd W_NIV
pr_hecho    resd 1
pr_total    resd 1
pr_n        resd 1
pr_nt       resd 1
pr_tic      resd 1
pr_an       resd 1
pr_atr      resd 1
pr_atr_nuevo resd 1
cancelar    resd 1
salir_pend  resd 1
dlg_dueno   resd 1
modo_busca  resd 1
pool_n      resd 1
pool_ult    resd 1
cbb_dta     resd 1
pc_dirs     resd 1
pc_hay_dir  resd 1
pc_fh       resd 1
pc_cambia   resd 1
pc_libre    resd 1
pc_totalk   resd 1
pc_y        resd 1
pc_p        resd 1
pc_pl       resd 1
pc_tabla    resd 1
arr_activo  resd 1
arr_pend    resd 1
arr_origen  resd 1
arr_x0      resd 1
arr_ai      resd 1
arr_dtipo   resd 1
arr_di      resd 1
ad_sx       resd 1
ad_sy       resd 1
ad_t        resd 1
ad_di       resd 1
ad_mover    resd 1
msg_b       resb M_TAM
w_hecho     resb W_NIV
w_ruta      resb 128
w_patron    resb 128
cu_otro     resb 128
op_dest     resb 128
op_dst      resb 128
op_nom      resb 16
nc_base     resb 16
nc_ext      resb 8
nb_nombre   resb 16
nb_antes    resb 16
nb_padre    resb 128
nv_padre    resb 128
ab_ruta     resb 128
ab_prog     resb 128
ab_reabrir  resb 128
arr_dest    resb 128
busca_pat   resb 20
busca_raiz  resb 128
pr_l1       resb 100
pr_l2       resb 100
pr_l3       resb 64
pc_v_nom    resb 64
tmp_ruta2   resb 128
pc_v_tipo   resb 64
pc_v_donde  resb 96
pc_v_tam    resb 48
pc_v_cont   resb 64
pc_v_fecha  resb 32
pc_dlg_plant resb P_TAM
pc_ctl      resb P_TAM * 24
op_nombres  resb 64 * 13
dta_w       resb DTA_TAM
w_dtas      resb W_NIV * DTA_TAM
fuentes     resb PP_MAX * RUTA_TAM
pp_rutas    resb PP_MAX * RUTA_TAM
buf_copia   resb BUF_COPIA
pool        resb POOL_TAM
        section .text
