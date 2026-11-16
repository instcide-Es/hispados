; =============================================================================
;  Cuentas - lo que se ve: las medidas de la ventana, la barra de botones, la
;  barra de fórmulas, los rótulos, las celdas (con su formato y con el texto
;  largo que se desborda sobre las celdas vacías de su derecha), lo elegido,
;  el pie y las barras de desplazamiento
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  De arriba abajo, la ventana lleva: la barra de botones (dos filas, de
;  barra.inc), la barra de fórmulas (la celda activa y su contenido tal como
;  se tecleó), los rótulos de las columnas, la rejilla (con los rótulos de
;  las filas a la izquierda y la barra vertical a la derecha), la barra
;  horizontal y el pie. La rejilla ocupa todo lo que deja lo demás: crece con
;  la ventana.
;
;  Las columnas pueden tener anchos distintos, así que la x de una columna se
;  saca sumando los anchos de las que hay desde la primera que se ve
;  ([desp_col]); las filas son todas igual de altas.
; =============================================================================

; las medidas de la ventana: [cli_an], [cli_al]; la rejilla va de (ROT_AN,
; GY0) a ([gx1], [gy1]); [vis_fil] filas y [vis_col] columnas enteras a la vista
medidas:
        pushad
        mov ebp, [ven]
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        sub ecx, BD_AN
        cmp ecx, ROT_AN + 8
        jge .a
        mov ecx, ROT_AN + 8
.a:
        mov [gx1], ecx
        sub edx, BR_PIE + BD_AN
        cmp edx, GY0 + CEL_AL
        jge .b
        mov edx, GY0 + CEL_AL
.b:
        mov [gy1], edx
        mov eax, edx
        sub eax, GY0
        xor edx, edx
        mov ecx, CEL_AL
        div ecx
        mov [vis_fil], eax
        mov eax, [desp_fil]         ; (al crecer la ventana, que no quede un
        add eax, [vis_fil]          ; hueco debajo de la última fila)
        sub eax, NFIL
        jle .c
        sub [desp_fil], eax
        jns .c
        mov dword [desp_fil], 0
.c:
        call cuenta_cols
        popad
        ret
; [vis_col]: cuántas columnas enteras se ven desde [desp_col] (una al menos)
cuenta_cols:
        pushad
        mov ecx, [desp_col]
        mov eax, ROT_AN
        xor ebx, ebx
.b:
        cmp ecx, NCOL
        jae .f
        movzx edx, word [col_an+ecx*2]
        add eax, edx
        cmp eax, [gx1]
        ja .f
        inc ebx
        inc ecx
        jmp .b
.f:
        test ebx, ebx
        jnz .g
        inc ebx
.g:
        mov [vis_col], ebx
        popad
        ret

; ECX=columna -> EAX=su x en la ventana (negativa si queda a la izquierda)
col_x:
        push ebx
        push ecx
        push edx
        mov eax, ROT_AN
        mov ebx, [desp_col]
.b:
        cmp ebx, ecx
        je .x
        ja .atras
        movzx edx, word [col_an+ebx*2]
        add eax, edx
        inc ebx
        jmp .b
.atras:
        dec ebx
        movzx edx, word [col_an+ebx*2]
        sub eax, edx
        jmp .b
.x:
        pop edx
        pop ecx
        pop ebx
        ret
; EDX=fila -> EBX=su y en la ventana
fil_y:
        mov ebx, edx
        sub ebx, [desp_fil]
        imul ebx, CEL_AL
        add ebx, GY0
        ret
; EAX=x de la ventana -> ECX=la columna que hay ahí (de las que se ven hacia
; la derecha; a la izquierda de la rejilla, la primera que se ve)
x_col:
        push ebx
        push edx
        mov ecx, [desp_col]
        mov ebx, ROT_AN
.b:
        cmp ecx, NCOL - 1
        jae .x
        movzx edx, word [col_an+ecx*2]
        add ebx, edx
        cmp eax, ebx
        jl .x
        inc ecx
        jmp .b
.x:
        pop edx
        pop ebx
        ret
; EBX=y de la ventana -> EDX=la fila que hay ahí
y_fil:
        push eax
        mov eax, ebx
        sub eax, GY0
        jns .a
        xor eax, eax
.a:
        xor edx, edx
        push ecx
        mov ecx, CEL_AL
        div ecx
        pop ecx
        add eax, [desp_fil]
        cmp eax, NFIL - 1
        jbe .b
        mov eax, NFIL - 1
.b:
        mov edx, eax
        pop eax
        ret
; EAX=x de la ventana -> ECX=la columna cuyo borde derecho está ahí (a tres
; puntos como mucho); CF=1 si no hay ninguno
borde_col:
        push ebx
        push edx
        mov ecx, [desp_col]
        mov ebx, ROT_AN
.b:
        cmp ecx, NCOL
        jae .no
        movzx edx, word [col_an+ecx*2]
        add ebx, edx
        mov edx, eax
        sub edx, ebx
        add edx, 3
        cmp edx, 6
        jbe .si
        cmp ebx, eax
        jg .no
        inc ecx
        jmp .b
.si:
        pop edx
        pop ebx
        clc
        ret
.no:
        pop edx
        pop ebx
        stc
        ret

; mueve la vista lo justo para que se vea la celda ECX=columna EDX=fila
a_la_vista_de:
        pushad
        cmp edx, [desp_fil]
        jae .f1
        mov [desp_fil], edx
.f1:
        mov eax, [desp_fil]
        add eax, [vis_fil]
        cmp edx, eax
        jb .c0
        mov eax, edx
        sub eax, [vis_fil]
        inc eax
        jns .f2
        xor eax, eax
.f2:
        mov [desp_fil], eax
.c0:
        cmp ecx, [desp_col]
        jae .c1
        mov [desp_col], ecx
.c1:
        cmp ecx, [desp_col]         ; mientras su borde derecho no quepa, una
        jbe .x                      ; columna más allá
        call col_x
        movzx ebx, word [col_an+ecx*2]
        add eax, ebx
        cmp eax, [gx1]
        jle .x
        inc dword [desp_col]
        jmp .c1
.x:
        call cuenta_cols
        call pon_barras
        popad
        ret
; ...la esquina de lo elegido que se mueve (o la celda activa, si es una sola)
a_la_vista:
        push ecx
        push edx
        mov ecx, [sel_c2]
        mov edx, [sel_f2]
        call a_la_vista_de
        pop edx
        pop ecx
        ret

; las barras de desplazamiento, al día
pon_barras:
        pushad
        mov ebp, [barra_v]
        test ebp, ebp
        jz .x
        mov ecx, NFIL
        sub ecx, [vis_fil]
        jns .a
        xor ecx, ecx
.a:
        mov edx, [vis_fil]
        mov eax, BDM_PONER
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [desp_fil]
        HV msg_enviar
        mov ebp, [barra_h]
        mov ecx, NCOL - 1
        mov edx, 1
        mov eax, BDM_PONER
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [desp_col]
        HV msg_enviar
.x:
        popad
        ret

; =============================================================================
;  Cómo se ve una celda
; =============================================================================
; ECX=columna EDX=fila -> [disp]=el texto que se ve (entero: quien pinta lo
; recorta), [disp_tipo] (0 vacía, 1 número, 2 texto, 3 error), [disp_alin]
; (1 izquierda, 2 centro, 3 derecha) y [disp_fmt]=su formato. Los números,
; con su formato, tienen que caber en la columna (si no, almohadillas)
celda_ver:
        pushad
        mov byte [disp], 0
        mov byte [disp_tipo], 0
        call cel_formato
        mov [disp_fmt], eax
        movzx ebx, word [col_an+ecx*2]
        sub ebx, 6
        jns .a
        xor ebx, ebx
.a:
        shr ebx, 3
        mov [nf_cars], ebx
        call cel_puntero
        cmp byte [esi], 0
        je .alin
        call valor_celda
        cmp byte [ev_err], 0
        jne .err
        cmp byte [cv_tipo], 2
        je .txt
        mov ebx, [disp_fmt]
        mov edi, disp
        call fmt_celda
        mov byte [disp_tipo], 1
        jmp .alin
.err:
        mov byte [disp_tipo], 3
        mov eax, t_err_err
        cmp byte [ev_err], EV_CIRC
        jne .e1
        mov eax, t_err_circ
.e1:
        cmp byte [ev_err], EV_REF
        jne .cp
        mov eax, t_err_ref
        jmp .cp
.txt:
        mov byte [disp_tipo], 2
.cp:
        mov esi, eax
        mov edi, disp
        mov ecx, CAD_MAX
.c:
        lodsb
        stosb
        test al, al
        jz .alin
        dec ecx
        jnz .c
        mov byte [edi], 0
.alin:
        mov eax, [disp_fmt]
        shr eax, F_ALIN_D
        and eax, 3
        jnz .x
        inc eax                     ; sin alineación propia: los textos, a la
        cmp byte [disp_tipo], 1     ; izquierda, y los números, a la derecha
        jne .x
        mov eax, 3
.x:
        mov [disp_alin], eax
        popad
        ret

; el valor de la celda ECX=columna EDX=fila -> EDX:EAX, [cv_tipo], [ev_err]
; (como calc_celda). El cálculo va con su propia pila: una fórmula pide
; otras, y la pila de la tarea es pequeña. Conserva los demás registros
valor_celda:
        push ebx
        push ecx
        push esi
        push edi
        push ebp
        mov [pila_ant], esp
        mov esp, pila_calc + PILA_CALC
        mov dword [ev_pila_min], pila_calc + 2048
        call calc_celda
        mov esp, [pila_ant]
        pop ebp
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret

; =============================================================================
;  Pintar
; =============================================================================
pintar:
        call calc_olvida            ; cada vez que se pinta se calcula de nuevo
        mov ebp, [ven]
        HV dc_pintar
        call medidas
        call sel_rect
        mov eax, [r_c1]
        xor eax, [r_c2]
        mov ebx, [r_f1]
        xor ebx, [r_f2]
        or eax, ebx
        mov [sel_varias], eax       ; distinto de 0: hay más de una celda elegida
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_formato
        mov [act_fmt], eax
        call pinta_botones
        call pinta_formula
        call pinta_pie
        COLOR C_ARENA               ; el hueco entre las dos barras de desplazamiento
        mov eax, [gx1]
        mov ebx, [gy1]
        mov ecx, BD_AN
        mov edx, BD_AN
        HV dc_rect
        xor eax, eax                ; de aquí abajo, solo los rótulos y la rejilla
        mov ebx, HY0
        mov ecx, [gx1]
        mov edx, [gy1]
        sub edx, HY0
        HV dc_cortar
        call pinta_rotulos
        mov eax, ROT_AN
        mov ebx, GY0
        mov ecx, [gx1]
        sub ecx, eax
        mov edx, [gy1]
        sub edx, ebx
        HV dc_cortar
        call pinta_celdas
        HV dc_cerrar
        jmp hecho

; ---- la barra de botones ----
pinta_botones:
        pushad
        mov ecx, [cli_an]
        mov edx, TB_AL
        call br_fondo
        mov edi, botones1
        mov ebx, 4
        call br_fila
        mov edi, botones2
        mov ebx, 32
        call br_fila
        popad
        ret
; lo que pregunta la barra común de cada botón (EDI): EBP=1 si está puesto
; ahora, según el formato de la celda activa
barra_estado:
        xor ebp, ebp
        mov eax, [edi+12]
        mov edx, [act_fmt]
        mov ecx, F_NEG
        cmp eax, O_NEG
        je .bit
        mov ecx, F_CUR
        cmp eax, O_CUR
        je .bit
        mov ecx, edx
        shr ecx, F_ALIN_D
        and ecx, 3
        add ecx, O_IZQ - 1
        cmp eax, O_IZQ
        jb .otros
        cmp eax, O_DER
        ja .otros
.igual:
        cmp eax, ecx
        jne .x
        inc ebp
.x:
        ret
.otros:
        mov ecx, edx
        shr ecx, F_NUM_D
        and ecx, 7
        cmp eax, O_MONEDA
        jne .o1
        cmp ecx, FN_MONEDA
        jmp .z
.o1:
        cmp eax, O_PORC
        jne .o2
        cmp ecx, FN_PORC
        jmp .z
.o2:
        mov ecx, edx
        shr ecx, F_COLOR_D
        and ecx, 7
        add ecx, O_COLOR
        cmp eax, O_COLOR
        jb .x
        cmp eax, O_COLOR + 3
        ja .x
        jmp .igual
.z:
        jne .x
        inc ebp
        ret
.bit:
        test edx, ecx
        jz .x
        inc ebp
        ret

; ---- la barra de fórmulas: la celda activa y su contenido tal como se
;      tecleó (o lo que se está tecleando) ----
pinta_formula:
        pushad
        COLOR C_ARENA
        xor eax, eax
        mov ebx, TB_AL
        mov ecx, [cli_an]
        mov edx, BARRA
        HV dc_rect
        COLOR C_ARENA_OSC
        mov ebx, TB_AL + BARRA - 1
        mov edx, 1
        HV dc_rect
        mov eax, REF_AN
        mov ebx, TB_AL
        mov ecx, 1
        mov edx, BARRA
        HV dc_rect
        COLOR C_BLANCO              ; el contenido, sobre blanco
        mov eax, REF_AN + 4
        mov ebx, TB_AL + 2
        mov ecx, [cli_an]
        sub ecx, REF_AN + 8
        mov edx, BARRA - 5
        HV dc_rect
        mov edi, ref_txt            ; «B7»
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call ref_nombre
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 6
        mov ebx, TB_AL + 3
        mov esi, ref_txt
        HV dc_texto
        mov ebp, [cli_an]           ; las letras que caben
        sub ebp, REF_AN + 16
        sar ebp, 3
        jle .x
        cmp byte [editando], 0
        jne .edita
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_puntero
        xor ecx, ecx
.l:
        cmp byte [esi+ecx], 0
        je .lf
        inc ecx
        cmp ecx, ebp
        jb .l
.lf:
        mov eax, REF_AN + 8
        mov ebx, TB_AL + 3
        HV dc_texto_n
        jmp .x
.edita:
        mov eax, REF_AN + 8
        mov ebx, TB_AL + 3
        call pinta_edt
.x:
        popad
        ret

; (interna) lo que se está tecleando, en EAX=x EBX=y, en EBP letras como
; mucho, con su cursor (si no cabe entero, el trozo donde está el cursor)
pinta_edt:
        pushad
        mov edx, [edt_pos]
        xor edi, edi                ; la primera letra que se ve
        cmp edx, ebp
        jb .a
        mov edi, edx
        sub edi, ebp
        inc edi
.a:
        lea esi, [edt_buf+edi]
        xor ecx, ecx
.l:
        cmp byte [esi+ecx], 0
        je .lf
        inc ecx
        cmp ecx, ebp
        jb .l
.lf:
        COLOR C_TINTA
        HV dc_texto_n
        sub edx, edi                ; el cursor: una raya
        lea eax, [eax+edx*8]
        inc ebx
        mov ecx, 2
        mov edx, 14
        HV dc_rect
        popad
        ret

; ---- el pie: el nombre del botón que hay bajo el ratón o, si no, lo
;      elegido: de un rango, su suma, su promedio y cuántos números tiene ----
pinta_pie:
        pushad
        mov ecx, [cli_an]
        mov edx, [cli_al]
        call br_pie
        jc .x
        mov edi, pie_txt
        cmp byte [editando], 0
        je .n
        mov esi, t_pie_edita
        call hvl_cad
        jmp .pon
.n:
        cmp dword [sel_varias], 0
        jne .rango
        mov ecx, [cur_col]          ; una celda: qué es
        mov edx, [cur_fil]
        call ref_nombre
        mov esi, t_pie_vacia
        call cel_puntero_q
        jz .p1
        mov esi, t_pie_formula
        cmp al, '='
        je .p1
        call celda_ver
        mov esi, t_pie_numero
        cmp byte [disp_tipo], 1
        je .p1
        mov esi, t_pie_texto
.p1:
        call hvl_cad
        jmp .pon
.rango:
        mov ecx, [r_c1]
        mov edx, [r_f1]
        call ref_nombre
        mov al, ':'
        stosb
        mov ecx, [r_c2]
        mov edx, [r_f2]
        call ref_nombre
        call suma_rango             ; [sr_n], [sr_s], [sr_mal]
        cmp dword [sr_n], 0
        je .pon
        cmp byte [sr_mal], 0
        jne .pon
        mov dword [nf_cars], 24
        mov esi, t_pie_suma
        call hvl_cad
        mov eax, [sr_s]
        mov edx, [sr_s+4]
        call .num
        mov esi, t_pie_promedio
        call hvl_cad
        mov ebx, [sr_s]             ; la suma entre cuántos
        mov ecx, [sr_s+4]
        mov eax, [sr_n]
        mov edx, CU_ESC
        mul edx
        push edi
        mov byte [ev_err], 0
        call ev_div
        pop edi
        cmp byte [ev_err], 0
        jne .cu
        call .num
.cu:
        mov esi, t_pie_cuenta
        call hvl_cad
        mov eax, [sr_n]
        call hvl_num
.pon:
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, BR_PIE - 3
        mov esi, pie_txt
        HV dc_texto
.x:
        popad
        ret
.num:
        call num_fmt
.nz:
        cmp byte [edi], 0
        je .nf
        inc edi
        jmp .nz
.nf:
        ret
; (interna) la celda ECX, EDX: AL=su primera letra y ZF=1 si está vacía
cel_puntero_q:
        push esi
        call cel_puntero
        mov al, [esi]
        pop esi
        test al, al
        ret

; la suma de los números de lo elegido ([r_*], recortado a lo escrito) ->
; [sr_s], cuántos son [sr_n] y [sr_mal]=1 si no cabe o hay un error
suma_rango:
        pushad
        xor eax, eax
        mov [sr_n], eax
        mov [sr_s], eax
        mov [sr_s+4], eax
        mov [sr_mal], al
        mov edx, [r_f1]
.f:
        cmp edx, [lim_f]
        jae .x
        mov ecx, [r_c1]
.c:
        cmp ecx, [lim_c]
        jae .sf
        call cel_puntero
        cmp byte [esi], 0
        je .s
        push edx
        call valor_celda
        mov ebx, edx
        pop edx
        cmp byte [ev_err], 0
        jne .mal
        cmp byte [cv_tipo], 1
        jne .s
        inc dword [sr_n]
        add [sr_s], eax
        adc [sr_s+4], ebx
        jno .s
.mal:
        mov byte [sr_mal], 1
.s:
        inc ecx
        cmp ecx, [r_c2]
        jbe .c
.sf:
        inc edx
        cmp edx, [r_f2]
        jbe .f
.x:
        popad
        ret

; ---- los rótulos de las columnas y de las filas ----
pinta_rotulos:
        pushad
        COLOR C_PIEDRA              ; el fondo de los rótulos y la esquina
        xor eax, eax                ; (pulsarla lo elige todo)
        mov ebx, HY0
        mov ecx, [gx1]
        mov edx, [gy1]
        HV dc_rect
        mov ecx, [desp_col]
        mov eax, ROT_AN
.col:
        cmp ecx, NCOL
        jae .colf
        cmp eax, [gx1]
        jge .colf
        movzx edx, word [col_an+ecx*2]
        push ecx
        push eax
        push edx
        COLOR C_PIEDRA
        cmp ecx, [r_c1]             ; las de lo elegido, más claras
        jb .c1
        cmp ecx, [r_c2]
        ja .c1
        COLOR C_ARENA
.c1:
        mov ecx, edx
        mov ebx, HY0
        mov edx, ROT_AL
        HV dc_rect
        COLOR C_ARENA_OSC           ; la raya que la separa de la siguiente
        add eax, ecx
        dec eax
        mov ecx, 1
        HV dc_rect
        mov ecx, [esp+8]
        mov edi, ref_txt
        call col_nombre
        mov byte [edi], 0
        sub edi, ref_txt            ; el nombre, centrado (si cabe)
        shl edi, 3
        mov eax, [esp]
        sub eax, edi
        jle .c2
        sar eax, 1
        add eax, [esp+4]
        mov ebx, HY0 + 1
        COLOR C_TINTA
        mov esi, ref_txt
        HV dc_texto
.c2:
        pop edx
        pop eax
        pop ecx
        add eax, edx
        inc ecx
        jmp .col
.colf:
        cmp eax, [gx1]              ; a la derecha de la última columna
        jge .filas
        COLOR C_CREMA
        mov ebx, HY0
        mov ecx, [gx1]
        sub ecx, eax
        mov edx, [gy1]
        HV dc_rect
.filas:
        mov edx, [desp_fil]
        mov ebx, GY0
.fil:
        cmp edx, NFIL
        jae .filf
        cmp ebx, [gy1]
        jge .filf
        push edx
        COLOR C_PIEDRA
        cmp edx, [r_f1]
        jb .f1
        cmp edx, [r_f2]
        ja .f1
        COLOR C_ARENA
.f1:
        xor eax, eax
        mov ecx, ROT_AN
        mov edx, CEL_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        push ebx
        add ebx, CEL_AL - 1
        mov edx, 1
        HV dc_rect
        pop ebx
        mov eax, [esp]
        inc eax
        mov edi, ref_txt
        call hvl_num
        mov byte [edi], 0
        sub edi, ref_txt            ; el número, arrimado a la derecha
        shl edi, 3
        mov eax, ROT_AN - 4
        sub eax, edi
        inc ebx
        COLOR C_TINTA
        mov esi, ref_txt
        HV dc_texto
        dec ebx
        pop edx
        add ebx, CEL_AL
        inc edx
        jmp .fil
.filf:
        popad
        ret

; ---- la rejilla: el fondo, lo elegido, las rayas, lo que hay en cada celda,
;      el marco de lo elegido y lo que se está tecleando ----
pinta_celdas:
        pushad
        COLOR C_BLANCO
        mov eax, ROT_AN
        mov ebx, GY0
        mov ecx, [gx1]
        mov edx, [gy1]
        HV dc_rect
        ; el rectángulo de lo elegido, en la ventana (recortado a lo que se ve)
        mov ecx, [r_c1]
        call col_x
        call .recx
        mov [s_x0], eax
        mov ecx, [r_c2]
        inc ecx
        call col_x
        call .recx
        mov [s_x1], eax
        mov edx, [r_f1]
        call fil_y
        call .recy
        mov [s_y0], ebx
        mov edx, [r_f2]
        inc edx
        call fil_y
        call .recy
        mov [s_y1], ebx
        cmp dword [sel_varias], 0
        je .rayas
        COLOR C_PETROLEO            ; varias celdas: en petróleo, menos la activa
        mov eax, [s_x0]
        mov ebx, [s_y0]
        mov ecx, [s_x1]
        sub ecx, eax
        jle .rayas
        mov edx, [s_y1]
        sub edx, ebx
        jle .rayas
        HV dc_rect
        COLOR C_BLANCO
        mov ecx, [cur_col]
        call col_x
        movzx ecx, word [col_an+ecx*2]
        mov edx, [cur_fil]
        call fil_y
        mov edx, CEL_AL
        cmp ebx, GY0 - CEL_AL
        jl .rayas
        cmp ebx, [gy1]
        jg .rayas
        HV dc_rect
.rayas:
        COLOR C_ARENA_OSC
        mov ecx, [desp_col]         ; las verticales
        mov eax, ROT_AN
.v:
        cmp eax, [gx1]
        jg .vf
        push ecx
        mov ebx, GY0
        mov ecx, 1
        mov edx, [gy1]
        HV dc_rect
        pop ecx
        cmp ecx, NCOL
        jae .vf
        movzx edx, word [col_an+ecx*2]
        add eax, edx
        inc ecx
        jmp .v
.vf:
        mov ebx, GY0                ; y las horizontales
.h:
        cmp ebx, [gy1]
        jg .hf
        mov eax, ROT_AN
        mov ecx, [gx1]
        mov edx, 1
        HV dc_rect
        add ebx, CEL_AL
        jmp .h
.hf:
        mov edx, [desp_fil]         ; lo que hay escrito, fila a fila
        mov ebx, GY0
.fila:
        cmp edx, NFIL
        jae .marco
        cmp ebx, [gy1]
        jge .marco
        call pinta_fila
        add ebx, CEL_AL
        inc edx
        jmp .fila
.marco:
        COLOR C_TINTA               ; el marco de lo elegido: dos puntos de grueso
        mov eax, [s_x0]
        mov ebx, [s_y0]
        mov ecx, [s_x1]
        sub ecx, eax
        jle .edita
        mov edx, [s_y1]
        sub edx, ebx
        jle .edita
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
.edita:
        cmp byte [editando], 0
        je .x
        mov ecx, [cur_col]          ; lo que se teclea, sobre la celda (y sobre
        call col_x                  ; las de su derecha, si no cabe)
        cmp eax, ROT_AN
        jl .x
        mov edx, [cur_fil]
        call fil_y
        cmp ebx, GY0
        jl .x
        movzx ecx, word [col_an+ecx*2]
        xor edx, edx
.el:
        cmp byte [edt_buf+edx], 0
        je .elf
        inc edx
        jmp .el
.elf:
        lea edx, [edx*8+16]         ; el ancho que pide lo tecleado
        cmp edx, ecx
        jbe .e1
        mov ecx, edx
.e1:
        mov edx, [gx1]
        sub edx, eax
        cmp ecx, edx
        jle .e2
        mov ecx, edx
.e2:
        cmp ecx, 24
        jl .x
        COLOR C_BLANCO
        mov edx, CEL_AL
        HV dc_rect
        COLOR C_TINTA
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
        mov ebp, ecx
        sub ebp, 6
        sar ebp, 3
        add eax, 2
        call pinta_edt
.x:
        popad
        ret
; (internas) recortan una x o una y a los bordes de la rejilla, con dos
; puntos de sobra (para que el marco de lo que se sale no se vea)
.recx:
        cmp eax, ROT_AN - 2
        jge .rx1
        mov eax, ROT_AN - 2
.rx1:
        push ebx
        mov ebx, [gx1]
        add ebx, 2
        cmp eax, ebx
        jle .rx2
        mov eax, ebx
.rx2:
        pop ebx
        ret
.recy:
        cmp ebx, GY0 - 2
        jge .ry1
        mov ebx, GY0 - 2
.ry1:
        push eax
        mov eax, [gy1]
        add eax, 2
        cmp ebx, eax
        jle .ry2
        mov ebx, eax
.ry2:
        pop eax
        ret

; (interna) lo escrito en la fila EDX, que está en la y EBX. Empieza por la
; última celda con algo que quede a la izquierda de lo que se ve: si es un
; texto largo, se desborda hasta aquí
pinta_fila:
        pushad
        mov esi, edx
        shl esi, 6                  ; ESI=la primera celda de la fila
        mov ecx, [desp_col]
.atras:
        test ecx, ecx
        jz .desde
        dec ecx
        lea edi, [esi+ecx]
        cmp dword [cel_off+edi*4], 0
        je .atras
        jmp .ya
.desde:
        mov ecx, [desp_col]
.ya:
        call col_x                  ; EAX=la x de la columna ECX
.c:
        cmp ecx, NCOL
        jae .x
        cmp eax, [gx1]
        jge .x
        lea edi, [esi+ecx]
        cmp dword [cel_off+edi*4], 0
        je .s
        call pinta_celda
.s:
        movzx edi, word [col_an+ecx*2]
        add eax, edi
        inc ecx
        jmp .c
.x:
        popad
        ret

; (interna) la celda ECX=columna EDX=fila, que está en EAX=x EBX=y
pinta_celda:
        pushad
        mov [pc_x], eax
        mov [pc_y], ebx
        call celda_ver
        xor edi, edi                ; EDI=las letras que tiene
.l:
        cmp byte [disp+edi], 0
        je .lf
        inc edi
        jmp .l
.lf:
        test edi, edi
        jz .x
        ; los colores: lo elegido (menos la celda activa), en claro sobre petróleo
        mov eax, [disp_fmt]
        shr eax, F_COLOR_D
        and eax, 7
        mov al, [colores+eax]
        mov ah, C_BLANCO
        cmp dword [sel_varias], 0
        je .col
        cmp ecx, [r_c1]
        jb .col
        cmp ecx, [r_c2]
        ja .col
        cmp edx, [r_f1]
        jb .col
        cmp edx, [r_f2]
        ja .col
        cmp ecx, [cur_col]
        jne .sel
        cmp edx, [cur_fil]
        je .col
.sel:
        mov al, C_CREMA
        mov ah, C_PETROLEO
.col:
        mov [tc_fg], al
        mov [tc_bg], ah
        movzx ebp, word [col_an+ecx*2]
        mov [pc_an], ebp
        sub ebp, 6                  ; EBP=las letras que caben en la celda
        jns .c1
        xor ebp, ebp
.c1:
        shr ebp, 3
        cmp edi, ebp
        jbe .cabe
        cmp byte [disp_tipo], 1     ; un número no se desborda (ni se corta:
        je .corta                   ; ya viene con almohadillas)
        cmp dword [disp_alin], 1
        jne .corta
        ; un texto largo: sobre las celdas vacías de su derecha
        mov esi, edx
        shl esi, 6
        mov eax, [pc_x]
        add eax, [pc_an]            ; EAX=hasta dónde llega
.des:
        lea ebx, [edi*8+6]          ; (solo hasta donde le hace falta)
        add ebx, [pc_x]
        cmp eax, ebx
        jge .desf
        inc ecx
        cmp ecx, NCOL
        jae .desf
        cmp eax, [gx1]
        jge .desf
        lea ebx, [esi+ecx]
        cmp dword [cel_off+ebx*4], 0
        jne .desf
        movzx ebx, word [col_an+ecx*2]
        add eax, ebx
        jmp .des
.desf:
        sub eax, [pc_x]
        cmp eax, [pc_an]
        je .corta
        mov ecx, eax                ; tapa las rayas que cruza
        dec ecx
        mov ebp, eax
        sub ebp, 6
        shr ebp, 3
        movzx eax, byte [tc_bg]
        HV dc_color
        mov eax, [pc_x]
        inc eax
        mov ebx, [pc_y]
        inc ebx
        mov edx, CEL_AL - 1
        HV dc_rect
.corta:
        cmp edi, ebp
        jbe .cabe
        mov edi, ebp
.cabe:
        test edi, edi
        jz .x
        mov eax, [pc_x]             ; dónde empieza, según la alineación
        mov ecx, [disp_alin]
        cmp ecx, 1
        jne .a2
        add eax, 3
        jmp .pinta
.a2:
        lea ebx, [edi*8]
        mov edx, [pc_an]
        sub edx, ebx
        cmp ecx, 2
        jne .a3
        sar edx, 1
        add eax, edx
        jmp .pinta
.a3:
        lea eax, [eax+edx-3]
.pinta:
        mov ebx, [pc_y]
        inc ebx
        mov ecx, edi
        mov esi, disp
        mov edx, [disp_fmt]
        mov byte [tc_neg], 0
        test edx, F_NEG
        jz .p1
        mov byte [tc_neg], 1
.p1:
        test edx, F_CUR
        jz .p2
        call texto_cursiva
        jmp .x
.p2:
        push eax
        movzx eax, byte [tc_fg]
        HV dc_color
        movzx eax, byte [tc_neg]
        HV dc_negrita
        pop eax
        HV dc_texto_n
        NEGRITA 0
.x:
        popad
        ret

; un texto en cursiva: ESI=texto ECX=letras EAX=x EBX=y, con los colores
; [tc_fg] sobre [tc_bg] y, con [tc_neg], en negrita. Se dibuja aparte, con la
; fuente del sistema y las filas de arriba corridas a la derecha (las de
; abajo, a la izquierda), y se copia a la ventana
texto_cursiva:
        pushad
        cmp ecx, TC_MAX
        jbe .a
        mov ecx, TC_MAX
.a:
        mov [tc_n], ecx
        lea edx, [ecx*8+4]
        mov [tc_an], edx
        mov edi, tc_buf
        mov ecx, edx
        shl ecx, 4
        mov al, [tc_bg]
        rep stosb
        xor ebp, ebp                ; la letra
.l:
        cmp ebp, [tc_n]
        jae .f
        movzx eax, byte [esi+ebp]
        shl eax, 4
        add eax, [hv_api+HV_fuente8x16*4]
        xor edx, edx                ; la fila
.r:
        mov bl, [eax+edx]
        mov ecx, 15                 ; lo que se corre esta fila: de 3 a 0
        sub ecx, edx
        shr ecx, 2
        mov edi, edx
        imul edi, [tc_an]
        lea edi, [edi+ebp*8]
        lea edi, [edi+ecx+tc_buf]
        mov ch, 8
        mov cl, [tc_fg]
.b:
        shl bl, 1
        jnc .nb
        mov [edi], cl
        cmp byte [tc_neg], 0
        je .nb
        mov [edi+1], cl
.nb:
        inc edi
        dec ch
        jnz .b
        inc edx
        cmp edx, 16
        jb .r
        inc ebp
        jmp .l
.f:
        mov eax, [esp+28]
        dec eax
        mov ebx, [esp+16]
        mov ecx, [tc_an]
        mov edx, 16
        mov esi, tc_buf
        HV dc_copiar
        popad
        ret

        section .bss
            alignb 4
cli_an      resd 1                  ; la zona de la ventana
cli_al      resd 1
gx1         resd 1                  ; donde acaba la rejilla (y empiezan las barras)
gy1         resd 1
vis_col     resd 1                  ; columnas y filas enteras que se ven
vis_fil     resd 1
sel_varias  resd 1                  ; hay más de una celda elegida
act_fmt     resd 1                  ; el formato de la celda activa
s_x0        resd 1                  ; lo elegido, en la ventana
s_y0        resd 1
s_x1        resd 1
s_y1        resd 1
pc_x        resd 1                  ; pinta_celda
pc_y        resd 1
pc_an       resd 1
disp_fmt    resd 1                  ; celda_ver: el formato,
disp_alin   resd 1                  ; la alineación
sr_n        resd 1                  ; suma_rango
sr_s        resd 2
tc_n        resd 1                  ; texto_cursiva
tc_an       resd 1
pila_ant    resd 1
disp_tipo   resb 1                  ; y la clase de lo que hay
sr_mal      resb 1
tc_fg       resb 1
tc_bg       resb 1
tc_neg      resb 1
            alignb 4
disp        resb CAD_MAX + 5        ; cómo se ve la celda
ref_txt     resb 16
pie_txt     resb 200
TC_MAX      equ 250
tc_buf      resb (TC_MAX * 8 + 4) * 16
PILA_CALC   equ 65536
pila_calc   resb PILA_CALC          ; la pila del cálculo
        section .text
