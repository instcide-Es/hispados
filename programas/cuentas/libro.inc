; =============================================================================
;  Cuentas - el libro: varias hojas en un mismo archivo
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un libro tiene de una a NHOJAS hojas, cada una con su nombre, que se ven
;  como pestañas abajo, a la izquierda de la barra de desplazamiento.
;
;  Las tablas de la hoja (cel_off, cel_fmt, el almacén: 900 K) no dan para
;  tener ocho hojas a la vez, así que solo la hoja ACTIVA está en ellas. Las
;  demás se guardan «plegadas»: cada una, en un bloque de memoria aparte, tal
;  como se escribiría en el archivo .HOJ (sus líneas A, H, P, G, C...). Al
;  cambiar de hoja, la activa se pliega (hoja_a_bloque: se escribe con las
;  mismas rutinas que al guardar, pero a la memoria) y la otra se despliega
;  (bloque_a_hoja: se lee con lee_hoj, como al abrir). Con la hoja van su
;  cursor y su vista (la línea «V»), sus anchos, sus paneles y su gráfico. El
;  diario de deshacer es de la hoja activa: al cambiar de hoja se vacía.
;
;  Las fórmulas pueden pedir celdas de otra hoja (Hoja2!A1, Hoja2!A1:B5).
;  Para dar el valor de una celda de una hoja plegada sin desplegarla, la
;  primera vez que se le pide algo se le hace un ÍNDICE (idx_haz): de cada
;  celda con algo, su número, dónde está su texto dentro del bloque y la
;  ficha donde el cálculo apunta su valor (como las del almacén). El texto
;  de cada celda se acaba entonces con un 0 en vez de su salto de línea (el
;  cálculo lo quiere así); al desplegar la hoja o al guardarla se deshace.
;  cel_puntero y cel_cache (hoja.inc) miran [ev_hoja]: si no es la activa,
;  buscan aquí.
;
;  En el archivo: detrás de la cabecera, cada hoja empieza con «S nombre», y
;  «L n» dice cuál estaba a la vista. Un libro de una sola hoja con el nombre
;  de siempre («Hoja1») no lleva ni una ni otra: es un .HOJ como los de
;  antes, y los de antes se abren igual.
; =============================================================================
NHOJAS      equ 8                   ; hojas de un libro, como mucho
NOM_HOJA    equ 16                  ; letras del nombre de una hoja (con el 0)
TAB_MAS     equ 22                  ; ancho de la pestaña «+»
IDX_TAM     equ 6 + CV_TAM          ; una entrada del índice: dw celda, dd texto, la ficha

t_hoja      db "Hoja", 0
t_lib_llena db "Un libro no puede tener más de ocho hojas.", 0
t_lib_una   db "Un libro tiene que tener al menos una hoja.", 0
t_lib_mem   db "No queda memoria para cambiar de hoja.", 10
            db "Guarde el libro y cierre algún programa.", 0
t_lib_borra db "¿Eliminar esta hoja con todo lo que tiene?", 10
            db "No se podrá deshacer.", 0
t_lib_nom   db "Nombre de la hoja", 0
t_q_lib_nom db "Nombre de la hoja (sin espacios):", 0
t_lib_nom_mal db "Ese nombre no vale. Tiene que empezar por una letra, llevar solo", 10
            db "letras, cifras o «_» (quince como mucho), no ser el de una celda", 10
            db "(como A1) ni el de otra hoja del libro.", 0

; el libro, vacío: una sola hoja, «Hoja1», sin nada
libro_vacio:
        pushad
        xor ebx, ebx
.b:
        call lib_suelta
        inc ebx
        cmp ebx, NHOJAS
        jb .b
        mov dword [lib_n], 1
        mov dword [lib_act], 0
        mov dword [ev_hoja], 0
        mov dword [lib_quiere], 0
        mov edi, lib_nom
        mov esi, t_hoja
        call hvl_cad
        mov word [edi], '1'
        call hoja_vacia
        popad
        ret
; suelta la memoria de todas las hojas plegadas (al cerrar)
libro_suelta:
        pushad
        xor ebx, ebx
.b:
        call lib_suelta
        inc ebx
        cmp ebx, NHOJAS
        jb .b
        popad
        ret
; (interna) suelta la memoria de la hoja EBX (su bloque y su índice)
lib_suelta:
        pushad
        call idx_suelta
        mov edx, [lib_mango+ebx*4]
        cmp dword [lib_blq+ebx*4], 0
        je .x
        HV mem_bloque_soltar
        mov dword [lib_blq+ebx*4], 0
.x:
        popad
        ret

; EAX=hoja -> ESI=su nombre
lib_nombre:
        mov esi, eax
        shl esi, 4
        add esi, lib_nom
        ret

; pliega la hoja activa: la escribe en un bloque de memoria. CF=1 si no hay
; memoria (y la hoja sigue como estaba)
hoja_a_bloque:
        pushad
        mov ebx, [lib_act]
        mov byte [esc_modo], 1      ; primero, solo para saber cuánto ocupa
        mov dword [esc_total], 0
        mov dword [esc_n], 0
        call hoja_escribe
        call esc_vuelca
        mov ecx, [esc_total]
        add ecx, 4
        HV mem_bloque
        jc .no
        mov ebx, [lib_act]
        mov [lib_blq+ebx*4], eax
        mov [lib_mango+ebx*4], edx
        mov [esc_mem_p], eax
        mov byte [esc_modo], 2      ; y ahora, de verdad
        mov dword [esc_total], 0
        call hoja_escribe
        call esc_vuelca
        mov edi, [esc_mem_p]
        mov byte [edi], 0
        mov byte [esc_modo], 0
        popad
        clc
        ret
.no:
        mov byte [esc_modo], 0
        popad
        stc
        ret

; despliega la hoja EAX (que estaba plegada) en las tablas, en vez de la
; que hubiera, y pasa a ser la activa
bloque_a_hoja:
        pushad
        mov [lib_act], eax
        mov [ev_hoja], eax
        mov ebx, eax
        call idx_suelta             ; (su texto, otra vez con saltos de línea)
        push dword [moneda_txt]     ; (la moneda es del libro, no de la hoja)
        push dword [moneda_txt+4]
        call hoja_vacia
        pop dword [moneda_txt+4]
        pop dword [moneda_txt]
        mov byte [hv_hay], 0
        mov esi, [lib_blq+ebx*4]
        test esi, esi
        jz .vacia
        push ebx
        call lee_hoj
        pop ebx
        call lib_suelta
.vacia:
        call limites
        call al_principio
        cmp byte [hv_hay], 0        ; el cursor y la vista, donde estaban
        je .x
        mov eax, [hv_c]
        cmp eax, NCOL
        jae .x
        mov ebx, [hv_f]
        cmp ebx, NFIL
        jae .x
        mov [cur_col], eax
        mov [sel_c2], eax
        mov [cur_fil], ebx
        mov [sel_f2], ebx
        mov eax, [hv_dc]
        cmp eax, NCOL
        jae .x
        cmp eax, [inm_c]
        jb .x
        mov ebx, [hv_df]
        cmp ebx, NFIL
        jae .x
        cmp ebx, [inm_f]
        jb .x
        mov [desp_col], eax
        mov [desp_fil], ebx
.x:
        popad
        ret

; cambia a la hoja EAX
cambia_hoja:
        cmp eax, [lib_n]
        jae .x
        cmp eax, [lib_act]
        je .x
        call hoja_a_bloque
        jc lib_sin_memoria
        call bloque_a_hoja
        jmp lib_repinta
.x:
        ret
lib_sin_memoria:
        mov esi, t_lib_mem
        jmp aviso
; (interna) después de cambiar algo del libro: las pestañas y la barra de
; abajo en su sitio, y todo a repintar
lib_repinta:
        cmp dword [barra_v], 0
        je .x
        call cuenta_cols
        call pon_hijas
        call a_la_vista
.x:
        jmp invalida

; ---- Insertar > Hoja nueva: detrás de la última, y pasa a ser la activa ----
hoja_nueva:
        mov esi, t_lib_llena
        cmp dword [lib_n], NHOJAS
        jae aviso
        call hoja_a_bloque
        jc lib_sin_memoria
        pushad
        mov ebx, 1                  ; su nombre: «HojaN», con el primer número
.n:                                 ; que no esté cogido
        mov edi, lib_nom_tmp
        mov esi, t_hoja
        call hvl_cad
        mov eax, ebx
        call hvl_num
        mov byte [edi], 0
        mov esi, lib_nom_tmp
        call lib_busca
        jc .libre
        inc ebx
        jmp .n
.libre:
        mov eax, [lib_n]
        inc dword [lib_n]
        mov [lib_act], eax
        mov [ev_hoja], eax
        call lib_nombre
        mov edi, esi
        mov esi, lib_nom_tmp
        mov ecx, NOM_HOJA
        rep movsb
        push dword [moneda_txt]
        push dword [moneda_txt+4]
        call hoja_vacia
        pop dword [moneda_txt+4]
        pop dword [moneda_txt]
        call al_principio
        mov byte [modif], 1
        popad
        jmp lib_repinta

; ---- Edición > Eliminar hoja: la activa, preguntando antes ----
hoja_elimina:
        mov esi, t_lib_una
        cmp dword [lib_n], 1
        jbe aviso
        pushad
        mov ebp, [ven]
        mov esi, t_lib_borra
        mov edi, t_cuentas
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne .x
        mov ebx, [lib_act]          ; EBX=la que se va
        lea eax, [ebx+1]            ; pasa a verse la siguiente (o la de antes,
        cmp eax, [lib_n]            ; si era la última)
        jb .a
        lea eax, [ebx-1]
.a:
        call bloque_a_hoja
        mov ecx, ebx                ; y las de detrás, un puesto antes
.c:
        lea eax, [ecx+1]
        cmp eax, [lib_n]
        jae .cf
        call lib_copia
        inc ecx
        jmp .c
.cf:
        mov dword [lib_blq+ecx*4], 0
        mov dword [lib_idx+ecx*4], 0
        dec dword [lib_n]
        mov eax, [lib_act]
        cmp eax, ebx
        jb .d
        dec eax
        mov [lib_act], eax
        mov [ev_hoja], eax
.d:
        mov byte [modif], 1
        popad
        jmp lib_repinta
.x:
        popad
        ret
; (interna) la hoja EAX (su nombre y su memoria), al puesto ECX
lib_copia:
        pushad
        mov esi, eax
        shl esi, 4
        add esi, lib_nom
        mov edi, ecx
        shl edi, 4
        add edi, lib_nom
        push ecx
        mov ecx, NOM_HOJA
        rep movsb
        pop ecx
        mov edx, [lib_blq+eax*4]
        mov [lib_blq+ecx*4], edx
        mov edx, [lib_mango+eax*4]
        mov [lib_mango+ecx*4], edx
        mov edx, [lib_idx+eax*4]
        mov [lib_idx+ecx*4], edx
        mov edx, [lib_idx_m+eax*4]
        mov [lib_idx_m+ecx*4], edx
        mov edx, [lib_idx_n+eax*4]
        mov [lib_idx_n+ecx*4], edx
        mov dl, [lib_fin+eax]
        mov [lib_fin+ecx], dl
        mov dl, [lib_des+eax]
        mov [lib_des+ecx], dl
        popad
        ret

; ---- Formato > Mover la hoja: EAX=-1 a la izquierda, 1 a la derecha ----
hoja_mueve:
        pushad
        mov ebx, [lib_act]
        add eax, ebx                ; EAX=con cuál se cambia
        cmp eax, [lib_n]
        jae .x                      ; (también si sale negativa)
        mov ecx, NHOJAS             ; a través de un puesto de sobra
        call lib_copia              ; la otra, al de sobra
        xchg eax, ebx
        mov ecx, ebx
        call lib_copia              ; la activa, al sitio de la otra
        mov ecx, eax
        mov eax, NHOJAS
        call lib_copia              ; y la otra, al de la activa
        mov [lib_act], ebx
        mov [ev_hoja], ebx
        mov byte [modif], 1
        call calc_olvida            ; (las hojas han cambiado de número)
        popad
        jmp lib_repinta
.x:
        popad
        ret

; ---- Formato > Nombre de la hoja (o doble clic en su pestaña) ----
hoja_renombra:
        pushad
        mov eax, [lib_act]
        call lib_nombre
        mov edi, lib_nom_tmp
        mov ecx, NOM_HOJA
        rep movsb
        mov ebp, [ven]
        mov esi, t_q_lib_nom
        mov edi, t_lib_nom
        mov ebx, lib_nom_tmp
        mov ecx, NOM_HOJA - 1
        HV dlg_pedir
        test eax, eax
        jz .x
        mov esi, lib_nom_tmp
        call nom_vale
        jc .mal
        mov eax, [lib_act]
        call lib_nombre
        push esi
        mov edi, lib_nom_ant        ; (el de antes: lo llevan las fórmulas)
        mov ecx, NOM_HOJA
        rep movsb
        pop edi
        mov esi, lib_nom_tmp
        mov ecx, NOM_HOJA
        rep movsb
        mov byte [modif], 1
        ; las fórmulas de todo el libro que la nombran (Hoja2!A1) pasan a
        ; decir el nombre nuevo: hoja por hoja, desplegando cada una
        call des_vacia
        call ren_hoja
        mov ebp, [lib_act]          ; EBP=la que se veía
        xor ebx, ebx
.h:
        cmp ebx, ebp
        je .hs
        call hoja_a_bloque
        jc .hf
        mov eax, ebx
        call bloque_a_hoja
        call ren_hoja
.hs:
        inc ebx
        cmp ebx, [lib_n]
        jb .h
.hf:
        cmp ebp, [lib_act]
        je .hx
        call hoja_a_bloque
        jc .hx
        mov eax, ebp
        call bloque_a_hoja
.hx:
        popad
        jmp lib_repinta
.mal:
        mov esi, t_lib_nom_mal
        call aviso
.x:
        popad
        ret

; (interna) corrige las fórmulas de la hoja activa: donde nombran la hoja
; lib_nom_ant, ponen lib_nom_tmp
ren_hoja:
        pushad
        xor ebx, ebx
.b:
        mov eax, [cel_off+ebx*4]
        test eax, eax
        jz .s
        cmp byte [almacen+eax], '='
        jne .s
        lea esi, [almacen+eax]
        mov edi, aj_buf
        call ren_formula
        jnc .s
        mov ecx, ebx
        and ecx, NCOL - 1
        mov edx, ebx
        shr edx, 6
        mov esi, aj_buf
        call cel_pon
.s:
        inc ebx
        cmp ebx, NCEL
        jb .b
        popad
        ret
; (interna) copia la fórmula ESI a EDI (CEL_MAX octetos) cambiando el nombre
; de la hoja; CF=1 si ha cambiado algo. Los textos entre comillas no se tocan
ren_formula:
        pushad
        lea ebp, [edi+CEL_MAX-1]
        mov byte [ren_cambio], 0
.b:
        mov al, [esi]
        test al, al
        jz .fin
        cmp al, '"'
        je .cad
        call .es
        jne .otro
        xor ecx, ecx                ; un nombre (o un número): ¿sigue un «!»?
.m:
        mov al, [esi+ecx]
        call .es
        jne .mf
        inc ecx
        jmp .m
.mf:
        cmp byte [esi+ecx], '!'
        jne .ident
        push esi                    ; ¿es el de la hoja?
        push ecx
        mov ebx, lib_nom_ant
.c:
        jecxz .cf
        mov al, [esi]
        call may_car
        mov ah, al
        mov al, [ebx]
        call may_car
        cmp al, ah
        jne .cno
        inc esi
        inc ebx
        dec ecx
        jmp .c
.cf:
        cmp byte [ebx], 0
.cno:
        pop ecx
        pop esi
        jne .ident
        add esi, ecx                ; sí: el nuevo, en su sitio
        mov ebx, lib_nom_tmp
.w:
        mov al, [ebx]
        test al, al
        jz .wf
        call .pon
        inc ebx
        jmp .w
.wf:
        mov byte [ren_cambio], 1
        jmp .b
.ident:
        mov al, [esi]
        call .pon
        inc esi
        dec ecx
        jnz .ident
        jmp .b
.cad:
        call .pon
        inc esi
        mov al, [esi]
        test al, al
        jz .fin
        cmp al, '"'
        jne .cad
.otro:
        call .pon
        inc esi
        jmp .b
.fin:
        mov byte [edi], 0
        popad
        cmp byte [ren_cambio], 1    ; (CF=1 si no ha cambiado nada...
        cmc                         ; ...y al revés)
        ret
; (internas) AL, al destino (si cabe); y ZF=1 si AL puede ser de un nombre
.pon:
        cmp edi, ebp
        jae .px
        mov [edi], al
        inc edi
.px:
        ret
.es:
        cmp al, '_'
        je .ex
        cmp al, '0'
        jb .eno
        cmp al, '9'
        jbe .esi
        push eax
        and al, 0xDF
        cmp al, 'A'
        jb .eno2
        cmp al, 'Z'
        ja .eno2
        pop eax
.esi:
        cmp al, al
.ex:
        ret
.eno2:
        pop eax
.eno:
        or esp, esp                 ; (ZF=0)
        ret

; ¿vale ESI como nombre de la hoja activa? CF=1 si no: vacío, largo, con
; letras raras, igual que una celda o que otra hoja
nom_vale:
        pushad
        mov al, [esi]
        call may_car
        cmp al, 'A'
        jb .no
        cmp al, 'Z'
        ja .no
        xor ecx, ecx
.l:
        mov al, [esi+ecx]
        test al, al
        jz .lf
        cmp al, '_'
        je .ls
        cmp al, '0'
        jb .no
        cmp al, '9'
        jbe .ls
        and al, 0xDF
        cmp al, 'A'
        jb .no
        cmp al, 'Z'
        ja .no
.ls:
        inc ecx
        cmp ecx, NOM_HOJA
        jb .l
        jmp .no
.lf:
        push esi                    ; ¿es una celda (B7), o empieza como una
        call lee_ref                ; (B7_x)? Al corregir las fórmulas se
        mov al, [esi]               ; tomaría por ella
        pop esi
        jc .u
        and al, 0xDF
        cmp al, 'A'
        jb .no
        cmp al, 'Z'
        ja .no
.u:
        call lib_busca              ; ¿y otra hoja?
        jc .si
        cmp eax, [lib_act]
        jne .no
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; busca la hoja que se llama como ESI (acabado en 0), sin mirar las
; mayúsculas -> EAX=cuál; CF=1 si no hay ninguna. Conserva los demás registros
lib_busca:
        push ebx
        push ecx
        push edi
        xor ebx, ebx
.b:
        cmp ebx, [lib_n]
        jae .no
        mov edi, ebx
        shl edi, 4
        add edi, lib_nom
        xor ecx, ecx
.c:
        mov al, [esi+ecx]
        call may_car
        mov ah, al
        mov al, [edi+ecx]
        call may_car
        cmp al, ah
        jne .s
        inc ecx
        test al, al
        jnz .c
        mov eax, ebx
        pop edi
        pop ecx
        pop ebx
        clc
        ret
.s:
        inc ebx
        jmp .b
.no:
        pop edi
        pop ecx
        pop ebx
        stc
        ret

; lo que pide el cálculo para «Hoja2!A1»: ESI=un nombre ECX=sus letras ->
; EAX=la hoja que se llama así; CF=1 si no la hay. Conserva los demás registros
hoja_busca:
        push ebx
        push edx
        push edi
        xor ebx, ebx
.b:
        cmp ebx, [lib_n]
        jae .no
        mov edi, ebx
        shl edi, 4
        add edi, lib_nom
        xor edx, edx
.c:
        cmp edx, ecx
        jae .fin
        mov al, [esi+edx]
        call may_car
        mov ah, al
        mov al, [edi+edx]
        call may_car
        cmp al, ah
        jne .s
        inc edx
        jmp .c
.fin:
        cmp byte [edi+edx], 0       ; (y el de la hoja no sigue)
        jne .s
        mov eax, ebx
        pop edi
        pop edx
        pop ebx
        clc
        ret
.s:
        inc ebx
        jmp .b
.no:
        pop edi
        pop edx
        pop ebx
        stc
        ret

; =============================================================================
;  El libro, en el archivo .HOJ
; =============================================================================
; escribe las hojas del libro (detrás de la cabecera), con las rutinas esc_*
libro_escribe:
        pushad
        cmp dword [lib_n], 1        ; (una sola hoja y con el nombre de siempre:
        jne .varias                 ; un archivo como los de antes)
        mov esi, lib_nom
        mov edi, t_hoja1
        mov ecx, 6
        repe cmpsb
        jne .varias
        call hoja_escribe
        popad
        ret
.varias:
        mov al, 'L'                 ; la que está a la vista
        call esc_car
        mov al, ' '
        call esc_car
        mov eax, [lib_act]
        inc eax
        call esc_num
        call esc_nl
        xor ebx, ebx
.h:
        mov al, 'S'
        call esc_car
        mov al, ' '
        call esc_car
        mov eax, ebx
        call lib_nombre
        call esc_cad
        call esc_nl
        cmp ebx, [lib_act]
        jne .plegada
        call hoja_escribe
        jmp .sig
.plegada:
        call idx_suelta             ; (su texto, con los saltos de línea)
        mov esi, [lib_blq+ebx*4]
        test esi, esi
        jz .sig
        call esc_cad
.sig:
        inc ebx
        cmp ebx, [lib_n]
        jb .h
        popad
        ret
t_hoja1     db "Hoja1", 0

; lee un libro: ESI=el texto del archivo .HOJ, acabado en 0 (se puede
; cambiar). La primera hoja queda en las tablas y las demás, plegadas; si el
; archivo dice que se veía otra, se cambia a ella
libro_lee:
        mov dword [lib_quiere], 0
        mov ebx, -1                 ; EBX=la hoja que se lee (-1: aún ninguna)
        mov [ll_ini], esi           ; donde empieza lo de la primera
.lin:
        cmp word [esi], 'S '        ; (al principio de una línea)
        jne .sig
        test ebx, ebx
        js .primera
        call .cierra                ; acaba la hoja de antes
.primera:
        inc ebx
        cmp ebx, NHOJAS
        jae .fin
        mov [lib_n], ebx
        inc dword [lib_n]
        add esi, 2                  ; su nombre
        mov eax, ebx
        push esi
        call lib_nombre
        mov edi, esi
        pop esi
        mov ecx, NOM_HOJA - 1
.nom:
        mov al, [esi]
        cmp al, 32
        jbe .nomf
        stosb
        inc esi
        dec ecx
        jnz .nom
.nomf:
        mov byte [edi], 0
        call .eol
        test ebx, ebx
        jz .lin
        mov [ll_ini], esi           ; (la primera se lee desde el principio del
        jmp .lin                    ; archivo: delante lleva la moneda)
.sig:
        cmp byte [esi], 0
        je .fin
        call .eol
        jmp .lin
.fin:
        test ebx, ebx               ; un archivo sin «S»: una sola hoja
        jns .f1
        mov esi, [ll_ini]
        jmp lee_hoj
.f1:
        cmp ebx, NHOJAS             ; (las que pasan de ocho no se leen)
        jae .f2
        call .cierra
.f2:
        mov eax, [lib_quiere]
        dec eax
        js .x
        mov dword [lib_act], 0
        mov dword [ev_hoja], 0
        cmp eax, [lib_n]
        jae .x
        test eax, eax
        jz .x
        push eax
        call limites
        pop eax
        call hoja_a_bloque
        jc .x
        call bloque_a_hoja
.x:
        ret
; (interna) ESI, al principio de la línea siguiente (o al 0 del final)
.eol:
        mov al, [esi]
        test al, al
        jz .ex
        inc esi
        cmp al, 10
        jne .eol
.ex:
        ret
; (interna) lo leído de la hoja EBX va de [ll_ini] a ESI: la primera, a las
; tablas; las demás, cada una a su bloque. Conserva ESI y EBX
.cierra:
        push esi
        push ebx
        mov al, [esi]               ; (el texto de la hoja, acabado en 0 un momento)
        push eax
        mov byte [esi], 0
        test ebx, ebx
        jnz .c1
        mov esi, [ll_ini]
        call lee_hoj
        jmp .c2
.c1:
        mov ecx, esi
        sub ecx, [ll_ini]
        push ecx
        add ecx, 4
        HV mem_bloque
        pop ecx
        jc .c2
        mov ebx, [esp+4]
        mov [lib_blq+ebx*4], eax
        mov [lib_mango+ebx*4], edx
        mov edi, eax
        mov esi, [ll_ini]
        rep movsb
        mov byte [edi], 0
.c2:
        pop eax
        pop ebx
        pop esi
        mov [esi], al
        ret

; =============================================================================
;  Las celdas de una hoja plegada (para las fórmulas que las piden)
; =============================================================================
; hace el índice de la hoja plegada EBX, si no lo tiene: de cada línea «C
; celda formato texto» con texto, una entrada. CF=1 si no se puede
idx_haz:
        cmp dword [lib_idx+ebx*4], 0
        jne .ya
        pushad
        mov esi, [lib_blq+ebx*4]
        test esi, esi
        jz .no
        mov dword [ix_n], 0         ; primero se cuentan
        mov byte [ix_pon], 0
        call .recorre
        mov ecx, [ix_n]
        imul ecx, IDX_TAM
        add ecx, IDX_TAM
        HV mem_bloque
        jc .no
        mov ebx, [esp+16]
        mov [lib_idx+ebx*4], eax
        mov [lib_idx_m+ebx*4], edx
        mov [ix_p], eax
        mov edi, eax                ; (las fichas, de ninguna tanda)
        mov ecx, [ix_n]
        imul ecx, IDX_TAM
        xor al, al
        rep stosb
        mov eax, [ix_n]
        mov [lib_idx_n+ebx*4], eax
        mov [ix_h], ebx
        mov byte [lib_fin+ebx], 13
        mov esi, [lib_blq+ebx*4]
        mov byte [ix_pon], 1
        call .recorre
        mov ebx, [ix_h]             ; ¿han quedado en orden? (si no, se buscan
        mov byte [lib_des+ebx], 0   ; una a una)
        mov esi, [lib_idx+ebx*4]
        mov ecx, [lib_idx_n+ebx*4]
.ord:
        cmp ecx, 2
        jb .ordf
        mov ax, [esi]
        cmp ax, [esi+IDX_TAM]
        jae .des
        add esi, IDX_TAM
        dec ecx
        jmp .ord
.des:
        mov byte [lib_des+ebx], 1
.ordf:
        popad
.ya:
        clc
        ret
.no:
        popad
        stc
        ret
; (interna) recorre las líneas del bloque ESI; con [ix_pon], apunta cada
; celda con texto en [ix_p] y acaba su texto con un 0
.recorre:
        mov al, [esi]
        test al, al
        jz .rx
        cmp word [esi], 'C '
        jne .rsig
        add esi, 2
        call lee_ref
        jc .rsig
        cmp ecx, NCOL
        jae .rsig
        cmp edx, NFIL
        jae .rsig
        shl edx, 6
        add edx, ecx                ; EDX=el número de la celda
        call hvl_lee_num            ; (su formato)
        cmp byte [esi], ' '
        jne .rsig
        inc esi
        cmp byte [esi], 32          ; sin texto: no cuenta
        jb .rsig
        cmp byte [ix_pon], 0
        jne .rpon
        inc dword [ix_n]
        jmp .rsig
.rpon:
        mov edi, [ix_p]
        mov [edi], dx
        mov [edi+2], esi
        add dword [ix_p], IDX_TAM
.rt:
        inc esi                     ; hasta el final de su texto
        cmp byte [esi], 32
        jae .rt
        mov al, [esi]
        test al, al
        jz .rx
        mov byte [esi], 0           ; (era un 13, o un 10 si la línea no traía 13:
        inc esi                     ; entonces todas se dejarán luego con un 10)
        cmp al, 10
        jne .rsig
        mov ebx, [ix_h]
        mov byte [lib_fin+ebx], 10
        jmp .recorre
.rsig:
        mov al, [esi]               ; a la línea siguiente
        test al, al
        jz .rx
        inc esi
        cmp al, 10
        jne .rsig
        jmp .recorre
.rx:
        ret

; deshace el índice de la hoja EBX (si lo tiene): el texto de cada celda
; vuelve a acabar con su salto de línea
idx_suelta:
        pushad
        mov esi, [lib_idx+ebx*4]
        test esi, esi
        jz .x
        mov ecx, [lib_idx_n+ebx*4]
        jecxz .f
.b:
        mov edi, [esi+2]
.t:
        cmp byte [edi], 0
        je .tf
        inc edi
        jmp .t
.tf:
        mov al, [lib_fin+ebx]
        mov [edi], al
        add esi, IDX_TAM
        dec ecx
        jnz .b
.f:
        mov edx, [lib_idx_m+ebx*4]
        HV mem_bloque_soltar
        mov dword [lib_idx+ebx*4], 0
.x:
        popad
        ret

; la celda ECX=columna EDX=fila de la hoja plegada [ev_hoja] -> ESI=su texto
; (si está vacía, o no hay tal hoja, un texto vacío) y en [ix_ult] su entrada
; del índice (para cel_cache). Conserva los demás registros
idx_puntero:
        push eax
        push ebx
        push ecx
        push edx
        push edi
        mov dword [ix_ult], 0
        mov ebx, [ev_hoja]
        cmp ebx, [lib_n]
        jae .v
        call idx_haz
        jc .v
        shl edx, 6
        add edx, ecx                ; EDX=el número de la celda
        mov esi, [lib_idx+ebx*4]
        cmp byte [lib_des+ebx], 0
        jne .lento
        xor eax, eax                ; a medias: están en orden (las escribe
        mov ecx, [lib_idx_n+ebx*4]  ; hoja_escribe, de la primera a la última)
.b:
        cmp eax, ecx
        jae .v
        lea edi, [eax+ecx]
        shr edi, 1
        mov ebx, edi
        imul ebx, IDX_TAM
        cmp dx, [esi+ebx]
        je .esta
        ja .der
        mov ecx, edi
        jmp .b
.der:
        lea eax, [edi+1]
        jmp .b
.lento:
        mov ebx, [ev_hoja]          ; (un archivo hecho a mano podría traerlas
        mov ecx, [lib_idx_n+ebx*4]  ; desordenadas: una a una)
        xor ebx, ebx
.l:
        jecxz .v
        cmp dx, [esi+ebx]
        je .esta
        add ebx, IDX_TAM
        dec ecx
        jmp .l
.esta:
        add esi, ebx
        mov [ix_ult], esi
        mov esi, [esi+2]
        jmp .x
.v:
        mov esi, t_nada
.x:
        pop edi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; =============================================================================
;  Las pestañas
; =============================================================================
; lo que ocupan las pestañas: [tab_x1] (sin pasar de lo que deja la barra de
; desplazamiento, que se queda con 120 puntos al menos)
tab_mide:
        pushad
        xor eax, eax
        xor ebx, ebx
.b:
        call tab_ancho
        add eax, ecx
        inc ebx
        cmp ebx, [lib_n]
        jb .b
        add eax, TAB_MAS + 4
        mov ecx, [gx1]
        sub ecx, 120
        jns .a
        xor ecx, ecx
.a:
        cmp eax, ecx
        jle .c
        mov eax, ecx
.c:
        mov [tab_x1], eax
        popad
        ret
; (interna) ECX=el ancho de la pestaña de la hoja EBX
tab_ancho:
        push esi
        mov esi, ebx
        shl esi, 4
        add esi, lib_nom
        xor ecx, ecx
.l:
        cmp byte [esi+ecx], 0
        je .f
        inc ecx
        jmp .l
.f:
        lea ecx, [ecx*8+14]
        pop esi
        ret

; pinta las pestañas: la de la hoja activa, en blanco (como el papel); las
; demás, en arena; y al final, la de añadir una hoja («+»)
pinta_tabs:
        pushad
        xor eax, eax
        mov ebx, [gy1]
        mov ecx, [tab_x1]
        mov edx, BD_AN
        test ecx, ecx
        jle .x
        call recorta
        COLOR C_ARENA
        HV dc_rect
        xor ebp, ebp                ; EBP=la hoja
        xor edi, edi                ; EDI=su x
.h:
        mov ebx, ebp
        call tab_ancho
        COLOR C_ARENA_OSC
        cmp ebp, [lib_act]
        jne .h1
        COLOR C_BLANCO
.h1:
        mov eax, edi
        mov ebx, [gy1]
        mov edx, BD_AN
        HV dc_rect
        COLOR C_TINTA               ; (una raya la separa de la siguiente)
        push eax
        push ecx
        lea eax, [eax+ecx-1]
        mov ecx, 1
        HV dc_rect
        pop ecx
        pop eax
        push ecx
        mov esi, ebp
        shl esi, 4
        add esi, lib_nom
        add eax, 7
        cmp ebp, [lib_act]
        jne .h2
        NEGRITA 1
.h2:
        HV dc_texto
        NEGRITA 0
        pop ecx
        add edi, ecx
        inc ebp
        cmp ebp, [lib_n]
        jb .h
        COLOR C_CREMA               ; la de añadir
        mov eax, edi
        mov ebx, [gy1]
        mov ecx, TAB_MAS
        mov edx, BD_AN
        HV dc_rect
        COLOR C_TINTA
        push eax
        add eax, TAB_MAS - 1
        mov ecx, 1
        HV dc_rect
        pop eax
        add eax, 7
        mov esi, t_tab_mas
        HV dc_texto
.x:
        popad
        ret
t_tab_mas   db "+", 0

; EAX=x EBX=y de la ventana -> EAX=la hoja de la pestaña que hay ahí, o
; [lib_n] si es la de añadir; CF=1 si ahí no hay ninguna
tab_en:
        push ebx
        push ecx
        push edx
        sub ebx, [gy1]              ; (la fila de las pestañas: debajo de la rejilla)
        cmp ebx, BD_AN
        jae .no
        cmp eax, [tab_x1]
        jae .no
        mov edx, eax
        xor eax, eax
        xor ebx, ebx
.b:
        call tab_ancho
        add eax, ecx
        cmp edx, eax
        jb .si
        inc ebx
        cmp ebx, [lib_n]
        jb .b
        add eax, TAB_MAS
        cmp edx, eax
        jae .no
.si:
        mov eax, ebx
        pop edx
        pop ecx
        pop ebx
        clc
        ret
.no:
        pop edx
        pop ecx
        pop ebx
        stc
        ret

        section .bss
            alignb 4
lib_n       resd 1                  ; cuántas hojas tiene el libro
lib_act     resd 1                  ; cuál es la activa (la que está en las tablas)
lib_quiere  resd 1                  ; al abrir: la que el archivo dice que se veía (desde 1)
tab_x1      resd 1                  ; donde acaban las pestañas (y empieza la barra)
ll_ini      resd 1                  ; libro_lee: donde empieza la hoja que se lee
esc_total   resd 1                  ; al escribir a la memoria: lo escrito
esc_mem_p   resd 1                  ; y por dónde va
hv_c        resd 1                  ; la línea «V» de la hoja leída: el cursor
hv_f        resd 1
hv_dc       resd 1                  ; y la vista
hv_df       resd 1
ix_n        resd 1                  ; idx_haz: cuántas celdas,
ix_p        resd 1                  ; por dónde va
ix_ult      resd 1                  ; idx_puntero: la entrada de la última celda pedida
ix_h        resd 1                  ; idx_haz: la hoja
lib_blq     resd NHOJAS + 1         ; el texto de cada hoja plegada (0: no tiene, o
lib_mango   resd NHOJAS + 1         ; es la activa) y el mango de su memoria
lib_idx     resd NHOJAS + 1         ; su índice (0: sin hacer),
lib_idx_m   resd NHOJAS + 1         ; el mango de su memoria
lib_idx_n   resd NHOJAS + 1         ; y cuántas celdas tiene
lib_nom     resb (NHOJAS + 1) * NOM_HOJA    ; los nombres (y un puesto de sobra, para mover)
lib_nom_tmp resb NOM_HOJA + 4
lib_nom_ant resb NOM_HOJA + 4       ; al cambiar un nombre: el de antes
esc_modo    resb 1                  ; a dónde escriben las rutinas esc_*: 0 al archivo,
                                    ; 1 a ningún sitio (solo se cuenta), 2 a la memoria
hv_hay      resb 1                  ; la hoja leída traía su línea «V»
lib_fin     resb NHOJAS + 1         ; con qué acababa el texto de cada celda (13 o 10)
lib_des     resb NHOJAS + 1         ; el índice de la hoja no está en orden
ix_pon      resb 1
ren_cambio  resb 1
        section .text
