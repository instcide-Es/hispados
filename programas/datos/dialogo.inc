; =============================================================================
;  DATOS.HVP - los diálogos: registros, campos, tablas, filtro, buscar,
;  resumen y las bases (nueva, abrir, copia)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El diálogo de un registro se hace sobre la marcha, con un cuadro por campo
;  de la tabla (hasta 32, en dos columnas): los nombres de los campos no son
;  controles, los pinta el gancho, para no gastar ventanas (Hispa Ventanas
;  tiene 160 para todos los programas). Al aceptar, el gancho valida cada
;  valor por su tipo; si uno no vale lo dice, deja el cursor en él y el
;  diálogo sigue abierto con todo lo escrito.
; =============================================================================

%macro DLG_BSS 0
re_dest     resd 1                  ; el registro que se cambia (-1: nuevo)
rp_filas    resd 1                  ; el diálogo del registro: filas,
rp_ed_an    resd 1                  ;   ancho de los cuadros
rp_cols     resd 1                  ;   y columnas
dp_x        resd 1
dp_y        resd 1
dp_an       resd 1
dp_al       resd 1
memo_i      resd 1
dc_i        resd 1                  ; el campo que se cambia (-1: uno nuevo;
dc_reft     resd 1                  ;   -2: el primero de una tabla nueva)
dc_refc     resd 1
tn_sel      resd 1
tl_sel      resd 1
fl_campo    resd 1
fl_op       resd 1
el_sel      resd 1
el_modo     resd 1
el_tab      resd 1
el_excl     resd 1
lista_orig  resd 1
et_n        resd 1
vo_tab      resd 1
vo_sel      resd 1
vo_col      resd 1
vo_f        resb 1
            alignb 4
dc_tipo     resb 1
dc_dec      resb 1
            alignb 4
dc_nom      resb 24
dc_dectxt   resb 8
dc_ref_txt  resb 64
tn_nom      resb 28
fl_txt      resb TXT_MAX + 4
tit_buf     resb 40
ruta_old    resb 128
lin_buf     resb 96
res_buf     resb 1200
dp_buf      resb (3 + 3 * MAX_CAM) * P_TAM + 8
%endmacro

LAB_AN      equ 164                 ; el sitio del nombre de cada campo
RP_FIL      equ 26                  ; de un campo al siguiente

; ---- diálogo: un campo ----
dlg_campo:
        PLANTILLA HV_c_dialogo, t_d_campo, E_DIALOGO, 0, 0, 440, 330, 0
        PLANTILLA HV_c_estatico, t_l_nombre, E_HIJA, 16, 18, 100, 18, 100
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 120, 14, 200, 24, 110
        PLANTILLA HV_c_estatico, t_l_tipo, E_HIJA, 16, 50, 100, 18, 101
        PLANTILLA HV_c_boton, t_tipo0, E_HIJA | E_TABULA | BS_OPCION, 120, 48, 300, 20, 120
        PLANTILLA HV_c_boton, t_tipo1x, E_HIJA | E_TABULA | BS_OPCION, 120, 70, 300, 20, 121
        PLANTILLA HV_c_boton, t_tipo2x, E_HIJA | E_TABULA | BS_OPCION, 120, 92, 300, 20, 122
        PLANTILLA HV_c_boton, t_tipo3x, E_HIJA | E_TABULA | BS_OPCION, 120, 114, 300, 20, 123
        PLANTILLA HV_c_boton, t_tipo4x, E_HIJA | E_TABULA | BS_OPCION, 120, 136, 300, 20, 124
        PLANTILLA HV_c_estatico, t_l_dec, E_HIJA, 16, 170, 100, 18, 102
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 120, 166, 40, 24, 111
        PLANTILLA HV_c_estatico, t_l_dec2, E_HIJA, 170, 170, 260, 18, 103
        PLANTILLA HV_c_estatico, t_l_ref, E_HIJA, 16, 202, 100, 18, 104
        PLANTILLA HV_c_estatico, dc_ref_txt, E_HIJA | SS_HUNDIDO, 120, 200, 200, 22, 105
        PLANTILLA HV_c_boton, t_b_elegir, E_HIJA | E_TABULA, 326, 198, 96, 26, 130
        PLANTILLA HV_c_estatico, t_l_ref2, E_HIJA, 120, 226, 310, 18, 106
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 120, 262, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 220, 262, 90, 26, ID_CANCELAR
        dd 0
; ---- diálogo: una tabla nueva ----
dlg_tnueva:
        PLANTILLA HV_c_dialogo, t_d_tnueva, E_DIALOGO, 0, 0, 380, 300, 0
        PLANTILLA HV_c_estatico, t_l_plant, E_HIJA, 16, 14, 340, 18, 100
        PLANTILLA HV_c_lista, 0, E_HIJA | E_TABULA, 16, 36, 220, 116, 60
        PLANTILLA HV_c_estatico, t_l_nombre, E_HIJA, 16, 168, 70, 18, 101
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 90, 164, 200, 24, 110
        PLANTILLA HV_c_estatico, t_l_tn2, E_HIJA, 90, 192, 280, 18, 102
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 90, 226, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 190, 226, 90, 26, ID_CANCELAR
        dd 0
; ---- diálogo: las tablas de la base ----
dlg_tablas:
        PLANTILLA HV_c_dialogo, t_d_tablas, E_DIALOGO, 0, 0, 420, 290, 0
        PLANTILLA HV_c_lista, 0, E_HIJA | E_TABULA, 16, 16, 240, 220, 60
        PLANTILLA HV_c_boton, t_b_ver, E_HIJA | E_TABULA | BS_DEFECTO, 270, 16, 130, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_b_tnueva, E_HIJA | E_TABULA, 270, 50, 130, 26, 3
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 270, 84, 130, 26, ID_CANCELAR
        dd 0
; ---- diálogo: el filtro (el valor va primero: es lo que se escribe casi
;      siempre, con el campo elegido y «contiene» ya puestos) ----
dlg_filtro:
        PLANTILLA HV_c_dialogo, t_d_filtro, E_DIALOGO, 0, 0, 470, 300, 0
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 232, 166, 216, 24, 110
        PLANTILLA HV_c_estatico, t_l_fcampo, E_HIJA, 16, 14, 200, 18, 100
        PLANTILLA HV_c_lista, 0, E_HIJA | E_TABULA, 16, 36, 200, 196, 60
        PLANTILLA HV_c_estatico, t_l_fcond, E_HIJA, 232, 14, 200, 18, 101
        PLANTILLA HV_c_lista, 0, E_HIJA | E_TABULA, 232, 36, 216, 84, 61
        PLANTILLA HV_c_estatico, t_l_fvalor, E_HIJA, 232, 144, 200, 18, 102
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 232, 206, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 330, 206, 90, 26, ID_CANCELAR
        PLANTILLA HV_c_boton, t_b_sinfiltro, E_HIJA | E_TABULA, 232, 238, 188, 26, 3
        dd 0
; ---- diálogo: un texto largo ----
dlg_memo:
        PLANTILLA HV_c_dialogo, t_d_memo, E_DIALOGO, 0, 0, 500, 330, 0
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA | ES_MULTI, 12, 12, 472, 240, 110
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 150, 262, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 250, 262, 90, 26, ID_CANCELAR
        dd 0
; ---- diálogo: elegir de una lista (un valor de otra tabla, una tabla, un
;      campo) ----
dlg_elige:
        PLANTILLA HV_c_dialogo, t_d_elige, E_DIALOGO, 0, 0, 380, 330, 0
        PLANTILLA HV_c_lista, 0, E_HIJA | E_TABULA, 12, 12, 352, 240, 60
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 90, 262, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 190, 262, 90, 26, ID_CANCELAR
        dd 0
; ---- diálogo: las etiquetas ----
dlg_etiq:
        PLANTILLA HV_c_dialogo, t_d_etiq, E_DIALOGO, 0, 0, 440, 330, 0
        PLANTILLA HV_c_estatico, t_l_etiq, E_HIJA, 16, 12, 410, 54, 100
        PLANTILLA HV_c_estatico, t_l_et1, E_HIJA, 16, 76, 70, 18, 101
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 90, 72, 330, 24, 110
        PLANTILLA HV_c_estatico, t_l_et2, E_HIJA, 16, 106, 70, 18, 102
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 90, 102, 330, 24, 111
        PLANTILLA HV_c_estatico, t_l_et3, E_HIJA, 16, 136, 70, 18, 103
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 90, 132, 330, 24, 112
        PLANTILLA HV_c_estatico, t_l_et4, E_HIJA, 16, 166, 70, 18, 104
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 90, 162, 330, 24, 113
        PLANTILLA HV_c_boton, t_b_imprimir, E_HIJA | E_TABULA | BS_DEFECTO, 90, 256, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 190, 256, 90, 26, ID_CANCELAR
        dd 0

t_d_campo   db "Campo", 0
t_d_tnueva  db "Nueva tabla", 0
t_d_tablas  db "Tablas de la base", 0
t_d_filtro  db "Filtrar", 0
t_d_memo    db "Texto largo", 0
t_d_elige   db "Elegir", 0
t_d_etiq    db "Imprimir etiquetas", 0
t_d_reg_n   db "Registro nuevo", 0
t_d_reg_c   db "Cambiar el registro", 0
t_l_nombre  db "Nombre:", 0
t_l_tipo    db "Tipo:", 0
t_tipo1x    db "Número (cantidades, precios, años)", 0
t_tipo2x    db "Fecha (día/mes/año)", 0
t_tipo3x    db "Sí o no (una casilla)", 0
t_tipo4x    db "Texto largo (notas, varias líneas)", 0
t_l_dec     db "Decimales:", 0
t_l_dec2    db "(solo en los números: de 0 a 3)", 0
t_l_ref     db "Viene de:", 0
t_l_ref2    db "(la tabla de la que se elige el valor)", 0
t_b_elegir  db "Elegir...", 0
t_ref_no    db "(de ninguna)", 0
t_ref_sep   db " > ", 0
t_ref_tit   db "¿De qué tabla viene?", 0
t_ref_tit2  db "¿De qué campo de esa tabla?", 0
t_ref_ning  db "(de ninguna: se escribe a mano)", 0
t_l_plant   db "¿De qué va a ser la tabla?", 0
t_l_tn2     db "(vacío: el de la lista)", 0
t_tabla_def db "Tabla", 0
t_b_ver     db "Ver", 0
t_b_tnueva  db "Nueva tabla...", 0
t_b_sinfiltro db "Quitar el filtro", 0
t_b_escribir db "Escribir...", 0
t_b_puntos  db "...", 0
t_b_imprimir db "Imprimir", 0
t_l_fcampo  db "Campo:", 0
t_l_fcond   db "Condición:", 0
t_l_fvalor  db "Valor:", 0
t_l_etiq    db "Una etiqueta por registro a la vista, en dos", 10
            db "columnas. En cada línea, los números de sus campos", 10
            db "(están abajo), separados por espacios.", 0
t_l_et1     db "Línea 1:", 0
t_l_et2     db "Línea 2:", 0
t_l_et3     db "Línea 3:", 0
t_l_et4     db "Línea 4:", 0
t_e_cnombre db "Escriba el nombre del campo.", 0
t_e_cdup    db "Ya hay un campo con ese nombre en la tabla.", 0
t_e_cdec    db "Los decimales tienen que ser un número de 0 a 3.", 0
t_e_num     db "tiene que ser un número, como 12 o 3,50", 10, "(como mucho, nueve cifras).", 0
t_e_fecha   db "tiene que ser una fecha que exista, así: 16/11/2026.", 0
t_e_largo   db "no puede tener más de 100 letras.", 0
t_e_fnum    db "Para comparar con este campo escriba un número, como 12 o 3,50.", 0
t_e_ffecha  db "Para comparar con este campo escriba una fecha, así: 16/11/2026.", 0
t_e_fsino   db "Para comparar con este campo escriba «sí» o «no».", 0
t_e_fvacio  db "Escriba el texto que tienen que contener.", 0
t_e_reftab  db "Para eso hace falta otra tabla en la base.", 0
t_r_campo   db "Campo «", 0
t_r_en      db "), en ", 0
t_r_regs    db " registros a la vista:", 10, 10, 0
t_r_con     db "Con valor:   ", 0
t_r_sin     db "Vacíos:      ", 0
t_r_suma    db "Suma:        ", 0
t_r_media   db "Media:       ", 0
t_r_mayor   db "Mayor:       ", 0
t_r_menor   db "Menor:       ", 0
t_r_prim    db "La primera:  ", 0
t_r_ult     db "La última:   ", 0
t_r_si      db "Que sí:      ", 0
t_r_no      db "Que no:      ", 0
t_res_tit   db "Resumen", 0
t_i_tit     db "Datos de la base", 0
t_i_arch    db "Archivo: ", 0
t_i_tablas  db "Tablas:", 10, 0
t_i_ninguna db "   (ninguna)", 10, 0
t_i_ocupa   db "Ocupa ", 0
t_i_de      db " K de los ", 0
t_i_k       db " K que caben en la memoria de Datos.", 0
t_i_reg     db " registros, ", 0
t_i_cam     db " campos", 0

; las clases de los diálogos fijos (una vez, al empezar)
dlg_clases:
        mov esi, dlg_campo
        call hvl_clases
        mov esi, dlg_tnueva
        call hvl_clases
        mov esi, dlg_tablas
        call hvl_clases
        mov esi, dlg_filtro
        call hvl_clases
        mov esi, dlg_memo
        call hvl_clases
        mov esi, dlg_elige
        call hvl_clases
        mov esi, dlg_etiq
        call hvl_clases
        ret

; ESI=plantilla del diálogo (y detrás, sus controles) EBX=gancho EBP=dueña
; -> EAX=el botón con el que se cierra
dialogo_de:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        lea edi, [esi+P_TAM]
        xor edx, edx
        HV dlg_modal
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
; lo mismo, de la ventana del programa
dialogo:
        push ebp
        mov ebp, [ven]
        call dialogo_de
        pop ebp
        ret

; (internas, para los ganchos; EBP=diálogo) el control ECX -> EBP
g_hija:
        HV ven_hija_id
        mov ebp, eax
        ret
; pone el texto EDX en el control ECX del diálogo EBP
g_pon_txt:
        pushad
        call g_hija
        mov eax, WM_SETTEXT
        HV msg_enviar
        popad
        ret
; lee el texto del control ECX en EDX (EBX octetos)
g_lee_txt:
        pushad
        call g_hija
        mov ecx, ebx
        mov eax, WM_GETTEXT
        HV msg_enviar
        popad
        ret
; marca (EDX=1) o desmarca el botón ECX
g_pon_marca:
        pushad
        call g_hija
        mov ecx, edx
        mov eax, BM_PONER_MARCA
        HV msg_enviar
        popad
        ret
; -> EAX=1 si el botón ECX está marcado
g_marca:
        pushad
        call g_hija
        mov eax, BM_MARCA
        HV msg_enviar
        mov [esp+28], eax
        popad
        ret
; añade el texto EDX a la lista ECX
g_lista_mas:
        pushad
        call g_hija
        mov eax, LM_ANADIR
        HV msg_enviar
        popad
        ret
; elige el EDX de la lista ECX
g_lista_pon:
        pushad
        call g_hija
        mov ecx, edx
        mov eax, LM_PONER_SELEC
        HV msg_enviar
        popad
        ret
; -> EAX=el elegido de la lista ECX (-1: ninguno)
g_lista_sel:
        pushad
        call g_hija
        mov eax, LM_SELEC
        HV msg_enviar
        mov [esp+28], eax
        popad
        ret
; arregla el Tab de la lista ECX. La lista de Hispa Ventanas (c_lista) pisa el
; estado de las mayúsculas antes de dejar el Tab al procedimiento por defecto,
; y el foco salta hacia atrás o hacia delante según cuántos elementos tenga.
; Aquí se le pone delante un procedimiento que atiende el Tab con el estado
; de verdad y le deja todo lo demás
g_lista_tab:
        pushad
        call g_hija
        mov eax, [ebp+V_PROC]
        cmp eax, lista_proc
        je .x
        mov [lista_orig], eax
        mov dword [ebp+V_PROC], lista_proc
.x:
        popad
        ret
lista_proc:
        cmp eax, WM_KEYDOWN
        jne .suyo
        cmp ecx, TC_TAB
        jne .suyo
        HVJ ven_defecto
.suyo:
        jmp [lista_orig]
; el foco, al control ECX
g_foco:
        pushad
        call g_hija
        HV ven_enfocar
        popad
        ret
; aviso desde un gancho: ESI=texto (EBP=el diálogo)
g_aviso:
        pushad
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret
; (para los ganchos con listas, la 60 y la 61) ¿es un doble clic o un Intro
; en una de ellas? Entonces es como pulsar Aceptar
g_doble:
        cmp eax, WM_COMMAND
        jne .x
        cmp cx, 60
        je .l
        cmp cx, 61
        jne .x
.l:
        mov eax, ecx
        shr eax, 16
        cmp eax, LN_DOBLE
        mov eax, WM_COMMAND
        jne .x
        mov ecx, ID_ACEPTAR
        xor edx, edx
        HV msg_enviar
        mov eax, WM_NULL
.x:
        ret

; =============================================================================
;  Los registros
; =============================================================================
; EAX=campo -> EDI=su búfer de edición
ebuf_ptr:
        mov edi, eax
        shl edi, 11
        add edi, [ebufs]
        ret

reg_mas:
        call hay_tabla
        jc .x
        mov eax, -1
        mov edx, -1
        call reg_edita
.x:
        ret
reg_camb:
        call reg_elegido
        jc .x
        mov edx, eax
        call reg_edita
.x:
        ret
; Duplicar: un registro nuevo que empieza con lo que tiene el elegido
reg_dup:
        call reg_elegido
        jc .x
        mov edx, -1
        call reg_edita
.x:
        ret
reg_borra:
        call reg_elegido
        jc .x
        mov esi, t_q_borrar
        call pregunta
        jne .x
        call reg_quita
        call guarda
        call inval
.x:
        ret

; el diálogo de un registro: empieza con lo que tiene el registro EAX (-1: en
; blanco) y, al aceptar, queda en el registro EDX (-1: uno nuevo)
reg_edita:
        pushad
        mov [re_dest], edx
        xor ecx, ecx
.f:
        cmp ecx, [n_cam]
        jae .f9
        push eax
        mov eax, ecx
        call ebuf_ptr
        pop eax
        mov byte [edi], 0
        cmp eax, -1
        je .fs
        push eax
        mov esi, [idx]
        mov esi, [esi+eax*4]
        mov eax, ecx
        call val_ptr
        mov al, [tipos+ecx]
        cmp al, T_SINO
        jne .ft
        mov al, [esi]
        mov [edi], al
        jmp .fp
.ft:
        mov dl, [decs+ecx]
        mov byte [fmt_mil], 0
        call val_txt
.fp:
        pop eax
.fs:
        inc ecx
        jmp .f
.f9:
        call reg_plant
        mov esi, dp_buf
        mov ebx, reg_gancho
        call dialogo
        cmp eax, ID_ACEPTAR
        jne .x
        mov eax, [re_dest]
        call reg_pon
        jc .lleno
        call guarda
        mov eax, [re_dest]          ; y queda elegido
        cmp eax, -1
        jne .s
        mov eax, [n_reg]
        dec eax
.s:
        mov [sel_reg], eax
        call sel_repon
        call inval
.x:
        popad
        ret
.lleno:
        mov esi, t_e_lleno
        mov ebx, [tab]
        cmp dword [ebx+T_NREG], MAX_REG
        jb .l1
        mov esi, t_e_maxreg
.l1:
        call aviso
        jmp .x

; (interna) una plantilla en EDI, que avanza: EAX=clase (su número en la
; interfaz) ESI=texto EBX=estilo ECX=id; el sitio, en dp_x, dp_y, dp_an, dp_al
dp_pon:
        push eax
        mov eax, [hv_api+eax*4]
        stosd
        mov eax, esi
        stosd
        mov eax, ebx
        stosd
        mov eax, [dp_x]
        stosd
        mov eax, [dp_y]
        stosd
        mov eax, [dp_an]
        stosd
        mov eax, [dp_al]
        stosd
        mov eax, ecx
        stosd
        pop eax
        ret

; (interna) EAX=campo -> EAX=x EBX=y de su línea en el diálogo del registro
rp_sitio:
        push ecx
        push edx
        xor edx, edx
        div dword [rp_filas]        ; EAX=columna EDX=fila
        mov ecx, [rp_ed_an]
        add ecx, LAB_AN + 24 + 12
        imul eax, ecx
        add eax, 12
        imul ebx, edx, RP_FIL
        add ebx, 12
        pop edx
        pop ecx
        ret

; (interna) hace en dp_buf las plantillas del diálogo del registro: por cada
; campo, un cuadro de texto (id 100 + campo), una casilla si es de sí o no, o
; un botón «Escribir...» (200 + campo) si es un texto largo; y un botón
; «...» (300 + campo) en los que vienen de otra tabla
reg_plant:
        pushad
        mov eax, [n_cam]
        mov dword [rp_cols], 1
        mov dword [rp_ed_an], 300
        cmp eax, 14
        jbe .u
        mov dword [rp_cols], 2
        mov dword [rp_ed_an], 176
        inc eax
        shr eax, 1
.u:
        mov [rp_filas], eax
        mov edi, dp_buf
        ; la ventana del diálogo
        mov eax, [rp_ed_an]
        add eax, LAB_AN + 24 + 12
        imul eax, [rp_cols]
        add eax, 12 + 2 * BORDE
        mov [dp_an], eax
        imul eax, [rp_filas], RP_FIL
        add eax, 12 + 8 + 26 + 14 + TIT_H + 2 * BORDE + 1
        mov [dp_al], eax
        xor eax, eax
        mov [dp_x], eax
        mov [dp_y], eax
        mov eax, HV_c_dialogo
        mov esi, t_d_reg_n
        cmp dword [re_dest], -1
        je .t
        mov esi, t_d_reg_c
.t:
        mov ebx, E_DIALOGO
        xor ecx, ecx
        call dp_pon
        xor ebp, ebp                ; EBP=el campo
.c:
        cmp ebp, [n_cam]
        jae .bot
        mov eax, ebp
        call rp_sitio
        add eax, LAB_AN
        mov [dp_x], eax
        mov [dp_y], ebx
        mov eax, [rp_ed_an]
        mov [dp_an], eax
        mov dword [dp_al], 22
        lea ecx, [ebp+100]
        mov al, [tipos+ebp]
        cmp al, T_SINO
        je .sn
        cmp al, T_MEMO
        je .memo
        mov eax, HV_c_edit
        xor esi, esi
        mov ebx, E_HIJA | E_TABULA
        call dp_pon
        mov eax, ebp                ; ¿viene de otra tabla? su botón
        push ebx
        call cam_ptr
        cmp byte [ebx+C_REFT], 0
        pop ebx
        je .cs
        mov eax, [rp_ed_an]
        add eax, 2
        add [dp_x], eax
        mov dword [dp_an], 30
        mov eax, HV_c_boton
        mov esi, t_b_puntos
        mov ebx, E_HIJA
        lea ecx, [ebp+300]
        call dp_pon
        jmp .cs
.sn:
        mov eax, HV_c_boton
        mov esi, t_si
        mov ebx, E_HIJA | E_TABULA | BS_CASILLA
        call dp_pon
        jmp .cs
.memo:
        mov eax, HV_c_boton
        mov esi, t_b_escribir
        mov ebx, E_HIJA | E_TABULA
        mov dword [dp_an], 110
        lea ecx, [ebp+200]
        call dp_pon
.cs:
        inc ebp
        jmp .c
.bot:
        imul eax, [rp_filas], RP_FIL
        add eax, 12 + 8
        mov [dp_y], eax
        mov dword [dp_x], 12 + LAB_AN
        mov dword [dp_an], 90
        mov dword [dp_al], 26
        mov eax, HV_c_boton
        mov esi, t_aceptar
        mov ebx, E_HIJA | E_TABULA | BS_DEFECTO
        mov ecx, ID_ACEPTAR
        call dp_pon
        add dword [dp_x], 100
        mov esi, t_cancelar
        mov ebx, E_HIJA | E_TABULA
        mov ecx, ID_CANCELAR
        call dp_pon
        xor eax, eax
        stosd
        popad
        ret

; el gancho del diálogo del registro
reg_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_PAINT
        je .pinta
        cmp eax, WM_USER + 99
        je .foco
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        je .acepta
        cmp ecx, 200
        jb .no
        cmp ecx, 200 + MAX_CAM
        jb .memo
        cmp ecx, 300
        jb .no
        cmp ecx, 300 + MAX_CAM
        jb .lista
.no:
        xor eax, eax
        ret
.foco:                              ; al cambiar un registro, el cursor
        mov ecx, [col]              ; empieza en el campo que estaba elegido
        cmp ecx, [n_cam]
        jae .no
        cmp byte [tipos+ecx], T_MEMO
        jne .f1
        add ecx, 100
.f1:
        add ecx, 100
        call g_foco
        jmp .no
.ini:
        cmp dword [re_dest], -1     ; (se pide con un mensaje a la cola: el
        je .i0                      ; diálogo pone su foco después de esto)
        mov eax, WM_USER + 99
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
.i0:
        xor ebx, ebx
.i:
        cmp ebx, [n_cam]
        jae .no
        mov eax, ebx
        call ebuf_ptr
        mov edx, edi
        lea ecx, [ebx+100]
        mov al, [tipos+ebx]
        cmp al, T_MEMO
        je .is
        cmp al, T_SINO
        jne .it
        movzx edx, byte [edi]
        call g_pon_marca
        jmp .is
.it:
        call g_pon_txt
.is:
        inc ebx
        jmp .i
.pinta:                             ; los nombres de los campos
        HV dc_pintar
        COLOR C_TINTA
        xor ecx, ecx
.p:
        cmp ecx, [n_cam]
        jae .p9
        mov eax, ecx
        call cam_ptr
        mov esi, ebx
        mov edi, lin_buf
        call hvl_cad
        mov word [edi], ':'
        mov eax, ecx
        call rp_sitio
        add ebx, 3
        mov esi, lin_buf
        push ecx
        HV dc_texto
        pop ecx
        inc ecx
        jmp .p
.p9:
        HV dc_cerrar
        mov eax, 1
        ret
.memo:                              ; un texto largo: su propio diálogo
        sub ecx, 200
        mov [memo_i], ecx
        mov esi, dlg_memo
        mov ebx, memo_gancho
        call dialogo_de
        mov eax, 1
        ret
.lista:                             ; un valor de otra tabla
        sub ecx, 300
        mov ebx, ecx
        call elige_valor            ; -> ESI=el texto elegido; CF=1 si nada
        jc .l9
        mov edx, esi
        lea ecx, [ebx+100]
        call g_pon_txt
        call g_foco
.l9:
        mov eax, 1
        ret
.acepta:
        ; lo escrito, a los búferes; y de ahí al registro, valor a valor
        xor ebx, ebx
.a:
        cmp ebx, [n_cam]
        jae .a9
        mov eax, ebx
        call ebuf_ptr
        lea ecx, [ebx+100]
        mov al, [tipos+ebx]
        cmp al, T_MEMO
        je .as
        cmp al, T_SINO
        jne .at
        call g_marca
        mov [edi], al
        jmp .as
.at:
        mov edx, edi
        push ebx
        mov ebx, 256
        call g_lee_txt
        pop ebx
.as:
        inc ebx
        jmp .a
.a9:
        mov edx, [rbuf]
        add edx, 2                  ; EDX=donde va el valor
        xor ebx, ebx
.m:
        cmp ebx, [n_cam]
        jae .m9
        mov eax, ebx
        call ebuf_ptr
        mov esi, edi
        mov edi, edx
        mov al, [tipos+ebx]
        cmp al, T_SINO
        jne .mt
        mov al, [esi]
        stosb
        jmp .ms
.mt:
        mov ah, [decs+ebx]
        cmp al, T_TEXTO
        jne .mv
        call largo
        cmp ecx, TXT_MAX
        mov ecx, t_e_largo
        ja .mal
.mv:
        push edx
        mov dl, al
        mov dh, ah
        call txt_val
        pop edx
        jnc .ms
        mov ecx, t_e_num
        cmp byte [tipos+ebx], T_NUM
        je .mal
        mov ecx, t_e_fecha
        jmp .mal
.ms:
        mov edx, edi
        inc ebx
        jmp .m
.m9:
        mov eax, edx
        mov esi, [rbuf]
        sub eax, esi
        mov [esi], ax
        xor eax, eax                ; todo vale: el diálogo se cierra
        ret
.mal:                               ; ECX=lo que pasa EBX=el campo
        mov edi, res_buf
        mov al, '«'
        stosb
        mov eax, ebx
        push ebx
        call cam_ptr
        mov esi, ebx
        pop ebx
        call hvl_cad
        mov ax, '» '
        stosw
        mov esi, ecx
        call hvl_cad
        mov byte [edi], 0
        mov esi, res_buf
        call g_aviso
        lea ecx, [ebx+100]
        call g_foco
        mov eax, 1                  ; (atendido: el diálogo sigue abierto)
        ret

; el gancho del texto largo: el campo [memo_i]
memo_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        mov eax, [memo_i]
        call ebuf_ptr
        mov edx, edi
        mov ebx, MEMO_MAX + 1
        mov ecx, 110
        call g_lee_txt
        jmp .no
.ini:
        mov eax, [memo_i]
        call ebuf_ptr
        mov edx, edi
        mov ecx, 110
        call g_pon_txt
.no:
        xor eax, eax
        ret

; =============================================================================
;  Elegir de una lista
; =============================================================================
; el gancho de dlg_elige: llena la lista según [el_modo] (0: los valores del
; campo [dc_refc] de la tabla [el_tab]; 1: las tablas, menos la que se ve, y
; delante «de ninguna»; 2: los campos de la tabla [el_tab]) y deja el
; elegido en [el_sel]
elige_gancho:
        call g_doble
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        mov ecx, 60
        call g_lista_sel
        mov [el_sel], eax
        test eax, eax
        jns .no
        mov eax, 1                  ; (sin elegir nada no se acepta)
        ret
.no:
        xor eax, eax
        ret
.ini:
        mov ecx, 60
        call g_lista_tab
        cmp dword [el_modo], 1
        je .tablas
        cmp dword [el_modo], 2
        je .campos
        ; los valores: del campo de la otra tabla, sin los vacíos (hasta 200,
        ; lo que admite una lista)
        mov eax, [el_tab]
        call ver_otra
        xor ebx, ebx
        xor edi, edi
.v:
        cmp ebx, [n_reg]
        jae .v9
        cmp edi, 200
        jae .v9
        push ecx
        mov eax, ebx
        mov ecx, [dc_refc]
        call celda
        mov byte [esi+39], 0
        pop ecx
        cmp byte [esi], 0
        je .vs
        mov edx, esi
        call g_lista_mas
        inc edi
.vs:
        inc ebx
        jmp .v
.v9:
        call ver_vuelve
        jmp .sel0
.tablas:
        mov edx, t_ref_ning
        call g_lista_mas
        xor ebx, ebx
.t:
        mov eax, [arena]
        cmp ebx, [eax+H_NTAB]
        jae .sel0
        cmp ebx, [el_excl]
        je .ts
        mov eax, ebx
        push ebx
        call tab_ptr
        lea edx, [ebx+T_NOM]
        pop ebx
        call g_lista_mas
.ts:
        inc ebx
        jmp .t
.campos:
        mov eax, [el_tab]
        call tab_ptr
        movzx edi, byte [ebx+T_NCAM]
        lea edx, [ebx+T_CAB]
.c:
        test edi, edi
        jz .sel0
        call g_lista_mas
        add edx, C_TAM
        dec edi
        jmp .c
.sel0:
        xor edx, edx
        call g_lista_pon
        jmp .no

; (interna) pone como tabla «que se ve» la EAX solo para leerla, sin filtro;
; después hay que volver a la de verdad con ver_vuelve
ver_otra:
        pushad
        mov ecx, [tab_i]
        mov [vo_tab], ecx
        mov ecx, [sel]
        mov [vo_sel], ecx
        mov ecx, [col]
        mov [vo_col], ecx
        mov cl, [f_on]
        mov [vo_f], cl
        mov byte [f_on], 0
        mov [tab_i], eax
        call tab_ptr
        mov [tab], ebx
        call indexa
        popad
        ret
ver_vuelve:
        pushad
        mov eax, [vo_tab]
        mov [tab_i], eax
        call tab_ptr
        mov [tab], ebx
        mov al, [vo_f]
        mov [f_on], al
        mov eax, [vo_col]
        mov [col], eax
        call indexa
        mov eax, [vo_sel]
        mov [sel], eax
        popad
        ret

; EBX=campo (que viene de otra tabla) EBP=diálogo: la lista de sus valores
; -> ESI=el elegido (texto, en lin_buf); CF=1 si no se elige
elige_valor:
        push eax
        push ebx
        push ecx
        push edx
        push edi
        mov eax, ebx
        call cam_ptr
        movzx eax, byte [ebx+C_REFT]
        dec eax
        js .no
        mov [el_tab], eax
        movzx eax, byte [ebx+C_REFC]
        mov [dc_refc], eax
        mov dword [el_modo], 0
        mov dword [el_sel], -1
        mov esi, dlg_elige
        mov ebx, elige_gancho
        call dialogo_de
        cmp eax, ID_ACEPTAR
        jne .no
        ; el texto del elegido: el mismo recorrido que hizo la lista
        mov eax, [el_tab]
        call ver_otra
        xor ebx, ebx
        mov edi, [el_sel]
        mov byte [lin_buf], 0
.v:
        cmp ebx, [n_reg]
        jae .v9
        mov eax, ebx
        mov ecx, [dc_refc]
        mov byte [fmt_mil], 0
        call reg_txt
        cmp byte [esi], 0
        je .vs
        dec edi
        jns .vs
        mov edi, lin_buf
        mov ecx, 95
.cp:
        lodsb
        stosb
        test al, al
        jz .v9
        dec ecx
        jnz .cp
        mov byte [edi], 0
        jmp .v9
.vs:
        inc ebx
        jmp .v
.v9:
        call ver_vuelve
        mov esi, lin_buf
        clc
        jmp .x
.no:
        stc
.x:
        pop edi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; =============================================================================
;  Los campos
; =============================================================================
; (interna) el texto de «viene de» del diálogo del campo, en dc_ref_txt
ref_txt:
        pushad
        mov edi, dc_ref_txt
        mov esi, t_ref_no
        mov eax, [dc_reft]
        dec eax
        js .p
        call tab_ptr
        lea esi, [ebx+T_NOM]
        call hvl_cad
        mov esi, t_ref_sep
        call hvl_cad
        mov eax, [dc_refc]
        shl eax, 5
        lea esi, [ebx+eax+T_CAB]
        mov byte [dc_ref_txt+24], 0
.p:
        call hvl_cad
        mov byte [edi], 0
        popad
        ret

campo_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, 130
        je .ref
        cmp ecx, ID_ACEPTAR
        je .acepta
.no:
        xor eax, eax
        ret
.ini:
        mov ecx, 110
        mov edx, dc_nom
        call g_pon_txt
        movzx ecx, byte [dc_tipo]
        add ecx, 120
        mov edx, 1
        call g_pon_marca
        mov edi, dc_dectxt
        movzx eax, byte [dc_dec]
        call hvl_num
        mov byte [edi], 0
        mov ecx, 111
        mov edx, dc_dectxt
        call g_pon_txt
        jmp .no
.ref:                               ; de qué tabla y de qué campo viene
        mov eax, [arena]            ; (de la propia tabla, no; si es el primer
        mov ecx, [eax+H_NTAB]       ; campo de una tabla nueva, vale cualquiera)
        mov edx, [tab_i]
        cmp dword [dc_i], -2
        jne .r0
        mov edx, -1
        inc ecx
.r0:
        mov [el_excl], edx
        cmp ecx, 2
        jae .r1
        mov esi, t_e_reftab
        call g_aviso
        jmp .si
.r1:
        mov dword [el_modo], 1
        mov dword [el_sel], -1
        mov dword [dlg_elige+P_TEXTO], t_ref_tit
        mov esi, dlg_elige
        mov ebx, elige_gancho
        call dialogo_de
        cmp eax, ID_ACEPTAR
        jne .r9
        mov eax, [el_sel]
        dec eax                     ; (el primero es «de ninguna»)
        jns .r2
        mov dword [dc_reft], 0
        jmp .r8
.r2:
        cmp eax, [el_excl]          ; la lista no lleva la tabla que se ve
        jb .r3
        inc eax
.r3:
        mov [el_tab], eax
        mov dword [el_modo], 2
        mov dword [el_sel], -1
        mov dword [dlg_elige+P_TEXTO], t_ref_tit2
        mov esi, dlg_elige
        mov ebx, elige_gancho
        call dialogo_de
        cmp eax, ID_ACEPTAR
        jne .r9
        mov eax, [el_tab]
        inc eax
        mov [dc_reft], eax
        mov eax, [el_sel]
        mov [dc_refc], eax
.r8:
        call ref_txt
        pushad
        mov ecx, 105
        call g_hija
        mov esi, dc_ref_txt
        HV ven_texto
        HV ven_invalidar
        popad
        mov ecx, ID_ACEPTAR         ; (y con Intro, se acepta)
        call g_foco
.r9:
        mov dword [dlg_elige+P_TEXTO], t_d_elige
.si:
        mov eax, 1
        ret
.acepta:
        mov ecx, 110
        mov edx, dc_nom
        mov ebx, 20
        call g_lee_txt
        mov esi, dc_nom
        call vacio
        mov esi, t_e_cnombre
        mov ecx, 110
        je .mal
        ; ¿otro campo de la tabla se llama igual?
        cmp dword [dc_i], -2
        je .tipo
        xor ebx, ebx
.d:
        cmp ebx, [n_cam]
        jae .tipo
        cmp ebx, [dc_i]
        je .ds
        push ebx
        mov eax, ebx
        call cam_ptr
        mov esi, ebx
        pop ebx
        call largo
        mov edx, ecx
        mov edi, esi
        mov esi, dc_nom
        call largo
        call cmp_txt
        test eax, eax
        mov esi, t_e_cdup
        mov ecx, 110
        jz .mal
.ds:
        inc ebx
        jmp .d
.tipo:
        xor ebx, ebx
.t:
        lea ecx, [ebx+120]
        call g_marca
        test eax, eax
        jnz .t1
        inc ebx
        cmp ebx, 5
        jb .t
        xor ebx, ebx
.t1:
        mov [dc_tipo], bl
        mov ecx, 111
        mov edx, dc_dectxt
        mov ebx, 8
        call g_lee_txt
        mov esi, dc_dectxt
        xor eax, eax
        call vacio
        je .dec
        xor dl, dl
        call lee_num
        mov esi, t_e_cdec
        mov ecx, 111
        jc .mal
        cmp eax, 3
        ja .mal
.dec:
        mov [dc_dec], al
        cmp byte [dc_tipo], T_NUM
        je .no
        mov byte [dc_dec], 0
        jmp .no
.mal:
        call g_aviso
        call g_foco
        mov eax, 1
        ret

; (interna) el diálogo del campo, sobre dc_*; ZF=1 si se acepta
campo_dlg:
        call ref_txt
        mov esi, dlg_campo
        mov ebx, campo_gancho
        call dialogo
        cmp eax, ID_ACEPTAR
        ret

; (interna) la ficha del campo de dc_* a EDI (C_TAM octetos); EAX=el ancho
; de su columna
dc_a_ficha:
        pushad
        push eax
        mov ecx, C_TAM
        xor al, al
        push edi
        rep stosb
        pop edi
        mov esi, dc_nom
        mov ecx, 19
        push edi
.n:
        lodsb
        test al, al
        jz .n1
        stosb
        dec ecx
        jnz .n
.n1:
        pop edi
        mov al, [dc_tipo]
        mov [edi+C_TIPO], al
        mov al, [dc_dec]
        mov [edi+C_DEC], al
        pop eax
        mov [edi+C_ANCHO], ax
        cmp byte [dc_tipo], T_TEXTO ; solo un texto puede venir de otra tabla
        jne .x
        mov eax, [dc_reft]
        mov [edi+C_REFT], al
        mov eax, [dc_refc]
        mov [edi+C_REFC], al
.x:
        popad
        ret

; (interna) nv = los campos de la tabla, tal como están
nv_copia:
        pushad
        mov ecx, [n_cam]
        mov [nv_n], ecx
        mov esi, [tab]
        add esi, T_CAB
        mov edi, nv_cam
        shl ecx, 3
        rep movsd
        xor eax, eax
.o:
        mov [nv_org+eax], al
        inc eax
        cmp eax, MAX_CAM
        jb .o
        popad
        ret
; (interna) rehace la tabla con nv, la guarda y repinta. Los campos de otras
; tablas que venían de uno de esta lo siguen hasta su sitio nuevo (o dejan de
; venir de él si se ha quitado). CF=1 (y lo dice) si no cabe
nv_aplica:
        pushad
        call rehaz
        jnc .ok
        mov esi, t_e_lleno
        call aviso
        popad
        stc
        ret
.ok:
        mov edx, [tab_i]
        inc edx
        mov ebx, [arena]
        xor eax, eax
.t:
        cmp eax, [ebx+H_NTAB]
        jae .t9
        push ebx
        call tab_ptr
        movzx ecx, byte [ebx+T_NCAM]
        lea esi, [ebx+T_CAB]
        pop ebx
.c:
        test ecx, ecx
        jz .t1
        cmp [esi+C_REFT], dl
        jne .c1
        push eax
        mov al, [esi+C_REFC]
        xor edi, edi
.b:
        cmp edi, [nv_n]
        jae .b0
        cmp [nv_org+edi], al
        je .b1
        inc edi
        jmp .b
.b0:
        mov byte [esi+C_REFT], 0
        xor edi, edi
.b1:
        mov eax, edi
        mov [esi+C_REFC], al
        pop eax
.c1:
        add esi, C_TAM
        dec ecx
        jmp .c
.t1:
        inc eax
        jmp .t
.t9:
        call guarda
        call sel_ver
        call inval
        popad
        clc
        ret

campo_mas:
        call hay_tabla
        jc .x
        cmp dword [n_cam], MAX_CAM
        jb .cabe
        mov esi, t_e_maxcam
        jmp aviso
.cabe:
        xor eax, eax
        mov [dc_nom], al
        mov [dc_tipo], al
        mov [dc_dec], al
        mov [dc_reft], eax
        mov dword [dc_i], -1
        call campo_dlg
        jne .x
        call nv_copia
        mov edi, [nv_n]
        mov byte [nv_org+edi], 0xFF
        shl edi, 5
        add edi, nv_cam
        movzx eax, byte [dc_tipo]
        movzx eax, word [an_tipo+eax*2]
        call dc_a_ficha
        inc dword [nv_n]
        call nv_aplica
        jc .x
        mov eax, [n_cam]
        dec eax
        mov [col], eax
        call sel_ver
        call inval
.x:
        ret

campo_camb:
        call hay_tabla
        jc .x
        mov eax, [col]
        mov [dc_i], eax
        call cam_ptr
        mov esi, ebx
        mov edi, dc_nom
        call copia
        mov al, [ebx+C_TIPO]
        mov [dc_tipo], al
        mov al, [ebx+C_DEC]
        mov [dc_dec], al
        movzx eax, byte [ebx+C_REFT]
        mov [dc_reft], eax
        movzx eax, byte [ebx+C_REFC]
        mov [dc_refc], eax
        call campo_dlg
        jne .x
        mov eax, [col]
        call cam_ptr
        mov al, [dc_tipo]           ; ¿cambia el tipo habiendo datos?
        mov ah, [dc_dec]
        cmp al, [ebx+C_TIPO]
        jne .q
        cmp ah, [ebx+C_DEC]
        je .pon
.q:
        cmp dword [n_reg], 0
        je .pon
        mov esi, t_q_ctipo
        call pregunta
        jne .x
.pon:
        call nv_copia
        mov edi, [col]
        shl edi, 5
        add edi, nv_cam
        movzx eax, word [ebx+C_ANCHO]
        call dc_a_ficha
        call nv_aplica
.x:
        ret

campo_quita:
        call hay_tabla
        jc .x
        cmp dword [n_cam], 1
        ja .hay
        mov esi, t_e_uncampo
        jmp aviso
.hay:
        mov esi, t_q_cquita
        call pregunta
        jne .x
        call nv_copia
        mov eax, [col]              ; los de detrás, un sitio adelante
.b:
        lea ebx, [eax+1]
        cmp ebx, [nv_n]
        jae .f
        mov [nv_org+eax], bl
        mov esi, ebx
        shl esi, 5
        add esi, nv_cam
        lea edi, [esi-C_TAM]
        mov ecx, C_TAM / 4
        rep movsd
        inc eax
        jmp .b
.f:
        dec dword [nv_n]
        call nv_aplica
.x:
        ret

campo_izq:
        mov eax, [col]
        dec eax
        js campo_der.x
        jmp campo_cambia
campo_der:
        mov eax, [col]
        lea ebx, [eax+1]
        cmp ebx, [n_cam]
        jae .x
        call campo_cambia
        ret
.x:
        ret
; (interna) cambia de sitio el campo EAX con el siguiente; el elegido se va
; con el suyo
campo_cambia:
        call hay_tabla
        jc .x
        push eax
        call nv_copia
        mov [nv_org+eax], al
        inc byte [nv_org+eax]
        mov [nv_org+eax+1], al
        mov esi, eax
        shl esi, 5
        add esi, nv_cam             ; ESI=el primero; detrás, el segundo
        mov ecx, C_TAM / 4
.c:
        mov edx, [esi]
        xchg edx, [esi+C_TAM]
        mov [esi], edx
        add esi, 4
        dec ecx
        jnz .c
        pop eax
        cmp eax, [col]
        jne .a
        inc dword [col]
        jmp .b
.a:
        mov [col], eax
.b:
        call nv_aplica
.x:
        ret

; =============================================================================
;  Las tablas
; =============================================================================
tablas:
        mov eax, [arena]
        cmp dword [eax+H_NTAB], 0
        je tabla_dlg_nueva
        mov dword [tl_sel], -1
        mov esi, dlg_tablas
        mov ebx, tablas_gancho
        call dialogo
        cmp eax, 3
        je tabla_dlg_nueva
        cmp eax, ID_ACEPTAR
        jne .x
        mov eax, [tl_sel]
        test eax, eax
        js .x
        cmp eax, [tab_i]
        je .x
        call tab_pon
        call inval
.x:
        ret
tablas_gancho:
        call g_doble
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, 16
        jae .no
        push ecx
        mov ecx, 60
        call g_lista_sel
        mov [tl_sel], eax
        pop ecx
.no:
        xor eax, eax
        ret
.ini:
        mov ecx, 60
        call g_lista_tab
        xor ebx, ebx
.t:
        mov eax, [arena]
        cmp ebx, [eax+H_NTAB]
        jae .t9
        mov eax, ebx
        push ebx
        call tab_ptr
        mov edi, lin_buf
        lea esi, [ebx+T_NOM]
        call hvl_cad
        mov ax, ' ('
        stosw
        mov eax, [ebx+T_NREG]
        call hvl_num
        mov ax, ')'
        stosw
        pop ebx
        mov edx, lin_buf
        mov ecx, 60
        call g_lista_mas
        inc ebx
        jmp .t
.t9:
        mov ecx, 60
        mov edx, [tab_i]
        call g_lista_pon
        jmp .no

; Nueva tabla: de una plantilla o en blanco (y entonces se pide su primer
; campo)
tabla_dlg_nueva:
        mov eax, [arena]
        cmp dword [eax+H_NTAB], MAX_TAB
        jb .cabe
        mov esi, t_e_maxtab
        jmp aviso
.cabe:
        mov byte [tn_nom], 0
        mov esi, dlg_tnueva
        mov ebx, tnueva_gancho
        call dialogo
        cmp eax, ID_ACEPTAR
        jne .x
        mov eax, [tn_sel]
        lea eax, [eax+eax*2]
        lea ebx, [plantillas+eax*4] ; EBX=la plantilla: nombre, campos, fichas
        mov esi, tn_nom
        call vacio
        jne .nom
        mov esi, [ebx]
        cmp dword [ebx+4], 0
        jne .nom1
        mov esi, t_tabla_def
.nom1:
        mov edi, tn_nom
        call copia
.nom:
        mov ecx, [ebx+4]
        test ecx, ecx
        jnz .plant
        xor eax, eax                ; en blanco: su primer campo
        mov [dc_nom], al
        mov [dc_tipo], al
        mov [dc_dec], al
        mov [dc_reft], eax
        mov dword [dc_i], -2
        call campo_dlg
        jne .x
        mov edi, nv_cam
        movzx eax, byte [dc_tipo]
        movzx eax, word [an_tipo+eax*2]
        call dc_a_ficha
        mov dword [nv_n], 1
        jmp .haz
.plant:
        mov [nv_n], ecx
        mov esi, [ebx+8]
        mov edi, nv_cam
        shl ecx, 3
        rep movsd
.haz:
        mov esi, tn_nom
        call tabla_nueva
        jnc .ok
        mov esi, t_e_lleno
        jmp aviso
.ok:
        call guarda
        call inval
.x:
        ret
tnueva_gancho:
        call g_doble
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        mov ecx, 60
        call g_lista_sel
        test eax, eax
        jns .a
        xor eax, eax
.a:
        mov [tn_sel], eax
        mov ecx, 110
        mov edx, tn_nom
        mov ebx, 24
        call g_lee_txt
.no:
        xor eax, eax
        ret
.ini:
        mov ecx, 60
        call g_lista_tab
        xor ebx, ebx
.p:
        lea eax, [ebx+ebx*2]
        mov edx, [plantillas+eax*4]
        mov ecx, 60
        call g_lista_mas
        inc ebx
        cmp ebx, N_PLANT
        jb .p
        mov ecx, 60
        xor edx, edx
        call g_lista_pon
        jmp .no

tabla_nombre:
        call hay_tabla
        jc .x
        mov esi, [tab]
        add esi, T_NOM
        mov edi, tn_nom
        call copia
        mov ebp, [ven]
        mov esi, t_nombre_q
        mov edi, t_nombre_tit
        mov ebx, tn_nom
        mov ecx, 24
        HV dlg_pedir
        test eax, eax
        jz .x
        mov esi, tn_nom
        call vacio
        je .x
        mov edi, [tab]
        add edi, T_NOM
        call copia
        call guarda
        call inval
.x:
        ret

tabla_quita:
        call hay_tabla
        jc .x
        mov esi, t_q_tborra
        call pregunta
        jne .x
        call tabla_borra
        call guarda
        call inval
.x:
        ret

; =============================================================================
;  Filtrar y buscar
; =============================================================================
filtrar:
        call hay_tabla
        jc .x
        mov esi, dlg_filtro
        mov ebx, filtro_gancho
        call dialogo
        cmp eax, 3
        je sin_filtro
        cmp eax, ID_ACEPTAR
        jne .x
        mov byte [f_on], 1
        call sel_guarda
        call filtra
        call sel_repon
        call inval
.x:
        ret
sin_filtro:
        cmp byte [f_on], 0
        je .x
        mov byte [f_on], 0
        call sel_guarda
        call filtra
        call sel_repon
        call inval
.x:
        ret
filtro_gancho:
        call g_doble
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        je .acepta
.no:
        xor eax, eax
        ret
.ini:
        mov ecx, 60
        call g_lista_tab
        mov ecx, 61
        call g_lista_tab
        xor ebx, ebx                ; los campos
.c:
        cmp ebx, [n_cam]
        jae .c9
        mov eax, ebx
        push ebx
        call cam_ptr
        mov edx, ebx
        pop ebx
        mov ecx, 60
        call g_lista_mas
        inc ebx
        jmp .c
.c9:
        xor ebx, ebx                ; las condiciones
.o:
        mov edx, [op_nom+ebx*4]
        mov ecx, 61
        call g_lista_mas
        inc ebx
        cmp ebx, 5
        jb .o
        mov edx, [col]              ; lo de ahora, o el campo elegido
        xor ebx, ebx
        cmp byte [f_on], 0
        je .p
        mov edx, [f_campo]
        mov ebx, [f_op]
        push edx
        mov ecx, 110
        mov edx, f_txt
        call g_pon_txt
        pop edx
.p:
        mov ecx, 60
        call g_lista_pon
        mov ecx, 61
        mov edx, ebx
        call g_lista_pon
        jmp .no
.acepta:
        mov ecx, 60
        call g_lista_sel
        test eax, eax
        jns .a1
        xor eax, eax
.a1:
        mov [fl_campo], eax
        mov ecx, 61
        call g_lista_sel
        test eax, eax
        jns .a2
        xor eax, eax
.a2:
        mov [fl_op], eax
        mov ecx, 110
        mov edx, fl_txt
        mov ebx, TXT_MAX + 1
        call g_lee_txt
        xor edi, edi                ; EDI=el valor, si es número o fecha
        cmp dword [fl_op], 4
        je .vale
        mov esi, fl_txt
        cmp dword [fl_op], 0
        jne .comp
        call vacio
        mov esi, t_e_fvacio
        je .mal
        jmp .vale
.comp:
        mov ebx, [fl_campo]
        mov al, [tipos+ebx]
        cmp al, T_NUM
        jne .f
        mov dl, [decs+ebx]
        call lee_num
        mov edi, eax
        mov esi, t_e_fnum
        jc .mal
        jmp .vale
.f:
        cmp al, T_FECHA
        jne .s
        call lee_fecha
        mov edi, eax
        mov esi, t_e_ffecha
        jc .mal
        jmp .vale
.s:
        cmp al, T_SINO
        jne .vale
        call lee_sino
        movzx edi, al
        mov esi, t_e_fsino
        jc .mal
.vale:
        mov [f_val], edi
        mov eax, [fl_campo]
        mov [f_campo], eax
        mov eax, [fl_op]
        mov [f_op], eax
        mov esi, fl_txt
        mov edi, f_txt
        call copia
        call largo
        mov [f_len], ecx
        jmp .no
.mal:
        call g_aviso
        mov ecx, 110
        call g_foco
        mov eax, 1
        ret

buscar:
        call hay_tabla
        jc .x
        mov ebp, [ven]
        mov esi, t_buscar_q
        mov edi, t_buscar_tit
        mov ebx, busca_txt
        mov ecx, 60
        HV dlg_pedir
        test eax, eax
        jz .x
        call buscar_sig
.x:
        ret
; busca el texto de busca_txt desde el registro siguiente al elegido (y da la
; vuelta), en todos los campos, como se ven
buscar_sig:
        call hay_tabla
        jc .x
        mov esi, busca_txt
        call vacio
        je buscar
        call largo
        mov edx, ecx                ; EDX=lo que mide lo buscado
        mov ebp, 1                  ; EBP=cuántos más allá del elegido
.r:
        cmp ebp, [n_vis]
        ja .no
        mov eax, [sel]
        add eax, ebp
        cmp eax, [n_vis]
        jb .r1
        sub eax, [n_vis]
.r1:
        mov ebx, eax                ; EBX=su sitio en la vista
        mov esi, [vis]
        mov eax, [esi+eax*4]
        xor ecx, ecx
.c:
        cmp ecx, [n_cam]
        jae .rs
        push ecx
        call celda
        mov edi, busca_txt
        call contiene
        pop ecx
        jnc .si
        inc ecx
        jmp .c
.rs:
        inc ebp
        jmp .r
.si:
        mov [sel], ebx
        mov [col], ecx
        mov byte [suma_ok], 0
        call sel_ver
        call inval
.x:
        ret
.no:
        mov esi, t_no_esta
        jmp aviso

; =============================================================================
;  El resumen del campo elegido y los datos de la base
; =============================================================================
; (interna) una línea del resumen: ESI=rótulo, y el número EDX:EAX con ECX
; decimales
res_lin:
        call hvl_cad
        call fmt_num64
        mov al, 10
        stosb
        ret
; (interna) lo mismo con una fecha en EAX
res_fec:
        call hvl_cad
        call fmt_fecha
        mov al, 10
        stosb
        ret

resumen:
        call hay_tabla
        jc .x0
        pushad
        mov edi, res_buf
        mov esi, t_r_campo
        call hvl_cad
        mov eax, [col]
        call cam_ptr
        mov esi, ebx
        call hvl_cad
        mov ax, '» '
        stosw
        mov al, '('
        stosb
        movzx eax, byte [ebx+C_TIPO]
        mov esi, [tipo_nom+eax*4]
        call hvl_cad
        mov esi, t_r_en
        call hvl_cad
        mov eax, [n_vis]
        call hvl_num
        mov esi, t_r_regs
        call hvl_cad
        push edi
        ; el recorrido: cuántos con valor, la suma, el mayor y el menor
        xor ebp, ebp                ; EBP=con valor
        mov dword [suma_lo], 0
        mov dword [suma_hi], 0
        mov byte [suma_ok], 0
        mov edi, 0x80000000         ; EDI=el mayor
        mov ebx, 0x7FFFFFFF         ; EBX=el menor
        xor ecx, ecx
.b:
        cmp ecx, [n_vis]
        jae .f
        mov esi, [vis]
        mov eax, [esi+ecx*4]
        mov esi, [idx]
        mov esi, [esi+eax*4]
        mov eax, [col]
        call val_ptr
        mov dl, [tipos+eax]
        cmp dl, T_NUM
        je .n
        cmp dl, T_FECHA
        je .d
        cmp dl, T_SINO
        je .s
        cmp word [esi], 0
        je .sig
        inc ebp
        jmp .sig
.s:
        cmp byte [esi], 0
        je .sig
        inc ebp
        jmp .sig
.d:
        mov eax, [esi]
        test eax, eax
        jz .sig
        jmp .mm
.n:
        mov eax, [esi]
        cmp eax, VACIO
        je .sig
        cdq
        add [suma_lo], eax
        adc [suma_hi], edx
.mm:
        inc ebp
        cmp eax, edi
        jle .m1
        mov edi, eax
.m1:
        cmp eax, ebx
        jge .sig
        mov ebx, eax
.sig:
        inc ecx
        jmp .b
.f:
        mov esi, edi                ; ESI=el mayor EBX=el menor
        pop edi
        mov eax, [col]
        movzx ecx, byte [decs+eax]
        mov dl, [tipos+eax]
        cmp dl, T_SINO
        jne .t1
        push esi
        mov eax, ebp
        xor edx, edx
        mov esi, t_r_si
        call res_lin
        mov eax, [n_vis]
        sub eax, ebp
        xor edx, edx
        mov esi, t_r_no
        call res_lin
        pop esi
        jmp .pon
.t1:
        push esi
        push edx
        mov eax, ebp
        xor edx, edx
        push ecx
        xor ecx, ecx
        mov esi, t_r_con
        call res_lin
        mov eax, [n_vis]
        sub eax, ebp
        xor edx, edx
        mov esi, t_r_sin
        call res_lin
        pop ecx
        pop edx
        pop esi
        test ebp, ebp
        jz .pon
        cmp dl, T_FECHA
        jne .t2
        mov eax, ebx
        push esi
        mov esi, t_r_prim
        call res_fec
        pop eax
        mov esi, t_r_ult
        call res_fec
        jmp .pon
.t2:
        cmp dl, T_NUM
        jne .pon
        push esi
        mov eax, [suma_lo]
        mov edx, [suma_hi]
        mov esi, t_r_suma
        call res_lin
        call media                  ; EDX:EAX=la media, ECX=sus decimales
        push ecx
        mov esi, t_r_media
        call res_lin
        pop ecx
        mov eax, [col]
        movzx ecx, byte [decs+eax]
        pop eax
        cdq
        mov esi, t_r_mayor
        call res_lin
        mov eax, ebx
        cdq
        mov esi, t_r_menor
        call res_lin
.pon:
        mov byte [edi-1], 0         ; (sin el último salto)
        mov ebp, [ven]
        mov esi, res_buf
        mov edi, t_res_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
.x0:
        ret

; (interna) la media: la suma ([suma_lo], [suma_hi]) entre EBP, con dos
; decimales por lo menos, redondeada -> EDX:EAX y ECX=sus decimales
media:
        push ebx
        push esi
        push edi
        mov eax, [suma_lo]
        mov edx, [suma_hi]
        xor edi, edi                ; EDI=1 si es negativa
        test edx, edx
        jns .p
        not edx
        neg eax
        sbb edx, -1
        inc edi
.p:
        mov ebx, [col]
        movzx ecx, byte [decs+ebx]
.d:
        cmp ecx, 2                  ; hasta dos decimales
        jae .d1
        push ecx
        mov ecx, eax                ; EDX:EAX por 10
        mov eax, edx
        mov ebx, 10
        mul ebx
        xchg eax, ecx
        mul ebx
        add edx, ecx
        pop ecx
        inc ecx
        jmp .d
.d1:
        mov ebx, ebp                ; más medio divisor, para redondear
        shr ebx, 1
        add eax, ebx
        adc edx, 0
        mov esi, eax                ; EDX:EAX entre EBP
        mov eax, edx
        xor edx, edx
        div ebp
        mov ebx, eax
        mov eax, esi
        div ebp
        mov edx, ebx
        test edi, edi
        jz .x
        not edx
        neg eax
        sbb edx, -1
.x:
        pop edi
        pop esi
        pop ebx
        ret

; Datos de la base: su archivo, sus tablas y lo que ocupa
info:
        pushad
        mov edi, res_buf
        mov esi, t_i_arch
        call hvl_cad
        mov esi, ruta_bdd
        call hvl_cad
        mov ax, 0x0A0A
        stosw
        mov esi, t_i_tablas
        call hvl_cad
        mov edx, [arena]
        cmp dword [edx+H_NTAB], 0
        jne .hay
        mov esi, t_i_ninguna
        call hvl_cad
.hay:
        xor ecx, ecx
.t:
        cmp ecx, [edx+H_NTAB]
        jae .t9
        mov eax, ecx
        call tab_ptr
        mov eax, '    '
        stosd
        dec edi
        lea esi, [ebx+T_NOM]
        call hvl_cad
        mov ax, ': '
        stosw
        mov eax, [ebx+T_NREG]
        call hvl_num
        mov esi, t_i_reg
        call hvl_cad
        movzx eax, byte [ebx+T_NCAM]
        call hvl_num
        mov esi, t_i_cam
        call hvl_cad
        mov al, 10
        stosb
        inc ecx
        jmp .t
.t9:
        mov al, 10
        stosb
        mov esi, t_i_ocupa
        call hvl_cad
        mov eax, [fin]
        sub eax, edx
        add eax, 1023
        shr eax, 10
        call hvl_num
        mov esi, t_i_de
        call hvl_cad
        mov eax, [arena_fin]
        sub eax, edx
        shr eax, 10
        call hvl_num
        mov esi, t_i_k
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        mov esi, res_buf
        mov edi, t_i_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; =============================================================================
;  Las bases: nueva, abrir y guardar una copia
; =============================================================================
; el título de la ventana: «Datos - SOCIOS.BDD»
pon_titulo:
        pushad
        mov edi, tit_buf
        mov esi, t_titulo
        call hvl_cad
        mov eax, ' - '
        stosd
        dec edi
        mov esi, ruta_bdd           ; el nombre, sin la ruta
        mov edx, esi
.b:
        lodsb
        test al, al
        jz .b1
        cmp al, '\'
        jne .b
        mov edx, esi
        jmp .b
.b1:
        mov esi, edx
        mov ecx, 14
.c:
        lodsb
        stosb
        test al, al
        jz .c1
        dec ecx
        jnz .c
        mov byte [edi], 0
.c1:
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov esi, tit_buf
        HV ven_texto
        or dword [ebp+V_ESTADO], S_NCSUCIO
        HV ven_invalidar
.x:
        popad
        ret

; (interna) pone la extensión ESI a nom_tmp si no lleva ninguna
pon_ext:
        push eax
        push edx
        push edi
        push esi
        mov esi, nom_tmp
        xor edx, edx
.e:
        lodsb
        test al, al
        jz .e2
        cmp al, '.'
        jne .e1
        inc edx
.e1:
        cmp al, '\'
        jne .e
        xor edx, edx
        jmp .e
.e2:
        lea edi, [esi-1]
        pop esi
        test edx, edx
        jnz .x
        call hvl_cad
        mov byte [edi], 0
.x:
        pop edi
        pop edx
        pop eax
        ret

; (interna) el diálogo de elegir un archivo: EDI=título, el nombre en nom_tmp
; -> ZF=1 si se cancela
pide_archivo:
        push ebx
        push ebp
        mov ebp, [ven]
        mov ebx, nom_tmp
        HV dlg_archivo
        test eax, eax
        pop ebp
        pop ebx
        ret

base_nueva:
        mov esi, t_nom_def
        mov edi, nom_tmp
        call copia
        mov edi, t_nueva_tit
        call pide_archivo
        jz .x
        call guarda_si              ; la de ahora, a salvo
        mov esi, ruta_bdd
        mov edi, ruta_old
        call copia
        mov esi, nom_tmp
        call pon_ruta
        mov esi, ruta_bdd           ; ¿ya hay un archivo con ese nombre?
        xor al, al
        HV dos_abrir
        jc .libre
        mov ebx, eax
        HV dos_cerrar
        mov esi, t_q_existe
        call pregunta
        je .libre
        mov esi, ruta_old
        call pon_ruta
        ret
.libre:
        call base_vacia
        call guarda
        call apunta_ini
        call pon_titulo
        call inval
        call tabla_dlg_nueva
.x:
        ret

base_abrir:
        mov byte [nom_tmp], 0
        mov edi, t_abrir_tit
        call pide_archivo
        jz .x
        call guarda_si
        mov esi, ruta_bdd
        mov edi, ruta_old
        call copia
        mov esi, nom_tmp
        call pon_ruta
        mov byte [aviso_ini], 0
        call abre
        test eax, eax
        jz .ok
        mov esi, t_e_abrir          ; no vale: se vuelve a la de antes
        call aviso
        mov esi, ruta_old
        call pon_ruta
        call abre
        jmp .fin
.ok:
        call apunta_ini
        cmp byte [aviso_ini], 0
        je .fin
        mov esi, t_recupera
        call aviso
        call guarda
.fin:
        mov byte [aviso_ini], 0
        call pon_titulo
        call inval
.x:
        ret

; Guardar una copia: la base, entera, en otro archivo (se sigue trabajando
; en la de siempre)
base_copia:
        call guarda                 ; (y de paso, tamaño y suma al día)
        mov esi, t_nada
        mov edi, nom_tmp
        call copia
        mov edi, t_copia_tit
        call pide_archivo
        jz .x
        mov esi, t_ext_bdd
        call pon_ext
        mov esi, nom_tmp
        call escribe_base
        mov esi, t_copia_ok
        jnc .d
        mov esi, t_e_guardar
.d:
        call aviso
.x:
        ret
