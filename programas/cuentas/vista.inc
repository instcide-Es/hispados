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
;  Las columnas pueden tener anchos distintos y las filas, altos distintos,
;  así que la x de una columna (y la y de una fila) se saca sumando los
;  anchos de las que hay desde la primera que se ve ([desp_col], [desp_fil]).
;
;  Con filas o columnas INMOVILIZADAS ([inm_f], [inm_c]: las primeras de la
;  hoja), esas están siempre en su sitio y las demás se mueven detrás, desde
;  ([gx0], [gy0]). La rejilla se pinta entonces en cuatro ZONAS, cada una
;  recortada a su sitio: ver zona_pon.
;
;  Una celda COMBINADA con las de su derecha (F_COMB en su formato) se pinta
;  a lo ancho de todas, y las que tapa no se pintan.
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
        call cuenta_cols
        mov eax, [desp_fil]         ; (al crecer la ventana, que no quede un
        add eax, [vis_fil]          ; hueco debajo de la última fila)
        sub eax, NFIL
        jle .c
        sub [desp_fil], eax
        mov eax, [inm_f]
        cmp [desp_fil], eax
        jge .c0
        mov [desp_fil], eax
.c0:
        call cuenta_cols
.c:
        popad
        ret
; lo que se ve, al día: dónde empiezan las columnas y las filas que se mueven
; ([gx0], [gy0]: detrás de las inmovilizadas) y cuántas enteras se ven desde
; [desp_col] y [desp_fil] ([vis_col], [vis_fil]: una al menos). Hay que
; llamarla cuando cambia un ancho, un alto, lo inmovilizado o la vista
cuenta_cols:
        pushad
        mov eax, ROT_AN
        xor ecx, ecx
.i:
        cmp ecx, [inm_c]
        jae .if
        movzx edx, word [col_an+ecx*2]
        add eax, edx
        inc ecx
        jmp .i
.if:
        mov [gx0], eax
        mov ecx, [desp_col]
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
        mov eax, GY0
        xor ecx, ecx
.j:
        cmp ecx, [inm_f]
        jae .jf
        movzx edx, byte [fil_al+ecx]
        add eax, edx
        inc ecx
        jmp .j
.jf:
        mov [gy0], eax
        mov ecx, [desp_fil]
        xor ebx, ebx
.r:
        cmp ecx, NFIL
        jae .rf
        movzx edx, byte [fil_al+ecx]
        add eax, edx
        cmp eax, [gy1]
        ja .rf
        inc ebx
        inc ecx
        jmp .r
.rf:
        test ebx, ebx
        jnz .rg
        inc ebx
.rg:
        mov [vis_fil], ebx
        popad
        ret

; ECX=columna -> EAX=su x en la ventana. Las inmovilizadas están siempre en
; su sitio; las demás, detrás, contando desde la primera que se ve (y las
; que quedan a su izquierda salen con una x menor: no se ven)
col_x:
        push ebx
        push ecx
        push edx
        mov eax, ROT_AN
        xor ebx, ebx
        cmp ecx, [inm_c]
        jb .b
        mov eax, [gx0]
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
; ...y la x donde acaba (su borde derecho)
col_x1:
        push edx
        call col_x
        movzx edx, word [col_an+ecx*2]
        add eax, edx
        pop edx
        ret
; EDX=fila -> EBX=su y en la ventana (lo mismo, con las filas)
fil_y:
        push eax
        push ecx
        mov ebx, GY0
        xor ecx, ecx
        cmp edx, [inm_f]
        jb .b
        mov ebx, [gy0]
        mov ecx, [desp_fil]
.b:
        cmp ecx, edx
        je .x
        ja .atras
        movzx eax, byte [fil_al+ecx]
        add ebx, eax
        inc ecx
        jmp .b
.atras:
        dec ecx
        movzx eax, byte [fil_al+ecx]
        sub ebx, eax
        jmp .b
.x:
        pop ecx
        pop eax
        ret
; ...y la y donde acaba
fil_y1:
        push eax
        call fil_y
        movzx eax, byte [fil_al+edx]
        add ebx, eax
        pop eax
        ret
; EDX=fila -> EAX=su alto, en puntos
fil_alto:
        movzx eax, byte [fil_al+edx]
        ret
; EAX=x de la ventana -> ECX=la columna que hay ahí (de las que se ven hacia
; la derecha; a la izquierda de la rejilla, la primera que se ve)
x_col:
        push ebx
        push edx
        cmp eax, [gx0]
        jl .fijas
.mueve:
        mov ecx, [desp_col]
        mov ebx, [gx0]
.b:
        cmp ecx, NCOL - 1
        jae .x
        movzx edx, word [col_an+ecx*2]
        add ebx, edx
        cmp eax, ebx
        jl .x
        inc ecx
        jmp .b
.fijas:
        cmp dword [inm_c], 0
        je .mueve
        xor ecx, ecx
        mov ebx, ROT_AN
.fb:
        lea edx, [ecx+1]
        cmp edx, [inm_c]
        jae .x
        movzx edx, word [col_an+ecx*2]
        add ebx, edx
        cmp eax, ebx
        jl .x
        inc ecx
        jmp .fb
.x:
        pop edx
        pop ebx
        ret
; EBX=y de la ventana -> EDX=la fila que hay ahí
y_fil:
        push eax
        push ecx
        cmp ebx, [gy0]
        jl .fijas
.mueve:
        mov edx, [desp_fil]
        mov eax, [gy0]
.b:
        cmp edx, NFIL - 1
        jae .x
        movzx ecx, byte [fil_al+edx]
        add eax, ecx
        cmp ebx, eax
        jl .x
        inc edx
        jmp .b
.fijas:
        cmp dword [inm_f], 0
        je .mueve
        xor edx, edx
        mov eax, GY0
.fb:
        lea ecx, [edx+1]
        cmp ecx, [inm_f]
        jae .x
        movzx ecx, byte [fil_al+edx]
        add eax, ecx
        cmp ebx, eax
        jl .x
        inc edx
        jmp .fb
.x:
        pop ecx
        pop eax
        ret
; EAX=x de la ventana -> ECX=la columna cuyo borde derecho está ahí (a tres
; puntos como mucho); CF=1 si no hay ninguno
borde_col:
        push ebx
        push edx
        push esi
        xor ecx, ecx                ; primero las inmovilizadas, luego las demás
        mov ebx, ROT_AN
        mov esi, [inm_c]
        call .busca
        jnc .x
        mov ecx, [desp_col]
        mov ebx, [gx0]
        mov esi, NCOL
        call .busca
.x:
        pop esi
        pop edx
        pop ebx
        ret
; (interna) de la columna ECX (que está en la x EBX) a la ESI, sin llegar
.busca:
        cmp ecx, esi
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
        jmp .busca
.si:
        clc
        ret
.no:
        stc
        ret
; EBX=y de la ventana -> EDX=la fila cuyo borde de abajo está ahí (a tres
; puntos como mucho); CF=1 si no hay ninguno
borde_fil:
        push eax
        push ecx
        push esi
        xor edx, edx
        mov eax, GY0
        mov esi, [inm_f]
        call .busca
        jnc .x
        mov edx, [desp_fil]
        mov eax, [gy0]
        mov esi, NFIL
        call .busca
.x:
        pop esi
        pop ecx
        pop eax
        ret
.busca:
        cmp edx, esi
        jae .no
        movzx ecx, byte [fil_al+edx]
        add eax, ecx
        mov ecx, ebx
        sub ecx, eax
        add ecx, 3
        cmp ecx, 6
        jbe .si
        cmp eax, ebx
        jg .no
        inc edx
        jmp .busca
.si:
        clc
        ret
.no:
        stc
        ret

; ---- las celdas combinadas: la primera (el «ancla») lleva en su formato
;      cuántas de su derecha tapa (F_COMB); las tapadas no se ven ----
; ECX=columna EDX=fila -> EAX=cuántas tapa (sin salirse de la hoja)
comb_n:
        push ebx
        mov eax, edx
        shl eax, 6
        add eax, ecx
        mov eax, [cel_fmt+eax*4]
        shr eax, F_COMB_D
        and eax, 63
        mov ebx, NCOL - 1
        sub ebx, ecx
        cmp eax, ebx
        jbe .x
        mov eax, ebx
.x:
        pop ebx
        ret
; ECX=columna EDX=fila -> EAX=cuántas celdas de su derecha son de su misma
; combinación (0: ninguna, o no está combinada)
comb_r:
        push ebx
        push esi
        mov esi, ecx                ; hacia atrás, hasta dar con un ancla
.b:
        push ecx
        mov ecx, esi
        call comb_n
        pop ecx
        test eax, eax
        jnz .hay
        test esi, esi
        jz .no
        dec esi
        jmp .b
.hay:
        add eax, esi                ; EAX=la última que tapa
        sub eax, ecx
        jns .x
.no:
        xor eax, eax
.x:
        pop esi
        pop ebx
        ret
; ECX=columna EDX=fila: si está tapada por una combinación, ECX=su ancla
comb_ancla:
        push eax
        push esi
        mov esi, ecx
.b:
        test esi, esi
        jz .x
        dec esi
        push ecx
        mov ecx, esi
        call comb_n
        pop ecx
        test eax, eax
        jz .b
        add eax, esi
        cmp eax, ecx
        jb .x
        mov ecx, esi
.x:
        pop esi
        pop eax
        ret
; ECX=columna EDX=fila -> EAX=su ancho, con las que tapa
cel_ancho:
        push ebx
        push ecx
        push edx
        call comb_n
        mov ebx, eax
        movzx eax, word [col_an+ecx*2]
.b:
        test ebx, ebx
        jz .x
        inc ecx
        movzx edx, word [col_an+ecx*2]
        add eax, edx
        dec ebx
        jmp .b
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; mueve la vista lo justo para que se vea la celda ECX=columna EDX=fila (las
; inmovilizadas se ven siempre)
a_la_vista_de:
        pushad
        call cuenta_cols
        cmp edx, [inm_f]
        jb .c0
        cmp edx, [desp_fil]
        jae .f1
        mov [desp_fil], edx
        jmp .c0
.f1:
        mov eax, [desp_fil]
        add eax, [vis_fil]
        cmp edx, eax
        jb .c0
        mov eax, [gy1]              ; no cabe por abajo: la primera que se ve
        sub eax, [gy0]              ; será la más alta que la deje entera
        mov esi, edx
.f2:
        movzx ebx, byte [fil_al+esi]
        sub eax, ebx
        js .f3
        dec esi
        cmp esi, [inm_f]
        jge .f2
.f3:
        inc esi
        cmp esi, edx
        jbe .f4
        mov esi, edx
.f4:
        mov [desp_fil], esi
.c0:
        cmp ecx, [inm_c]
        jb .x
        cmp ecx, [desp_col]
        jae .c1
        mov [desp_col], ecx
.c1:
        cmp ecx, [desp_col]         ; mientras su borde derecho no quepa, una
        jbe .x                      ; columna más allá
        call col_x1
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

; las barras de desplazamiento, al día (cuentan desde la primera columna y
; la primera fila que se mueven)
pon_barras:
        pushad
        mov ebp, [barra_v]
        test ebp, ebp
        jz .x
        mov ecx, NFIL
        sub ecx, [vis_fil]
        sub ecx, [inm_f]
        jns .a
        xor ecx, ecx
.a:
        mov edx, [vis_fil]
        mov eax, BDM_PONER
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [desp_fil]
        sub ecx, [inm_f]
        HV msg_enviar
        mov ebp, [barra_h]
        mov ecx, NCOL - 1
        sub ecx, [inm_c]
        mov edx, 1
        mov eax, BDM_PONER
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [desp_col]
        sub ecx, [inm_c]
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
        call cel_ancho              ; (con las que tapa, si está combinada)
        mov ebx, eax
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
        mov eax, [lib_act]          ; (la celda es de la hoja activa)
        mov [ev_hoja], eax
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
        mov ebp, [ven]              ; (lo calculado sigue valiendo mientras no
                                    ; cambie la hoja: ver «invalida»)
        HV dc_pintar
        call medidas
        cmp byte [vp_on], 0
        jne pinta_prelim
        call sel_rect
        call sel_una
        setne al
        movzx eax, al
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
        call pinta_tabs
        call pinta_rotulos
        xor esi, esi                ; las cuatro zonas de la rejilla (si no hay nada
.z:                                 ; inmovilizado, solo la última tiene algo)
        call zona_pon
        jc .zs
        call pinta_zona
.zs:
        inc esi
        cmp esi, 4
        jb .z
        mov eax, ROT_AN             ; y, sobre toda ella, las rayas de lo
        mov ebx, GY0                ; inmovilizado y lo que se está tecleando
        mov ecx, [gx1]
        sub ecx, eax
        mov edx, [gy1]
        sub edx, ebx
        call recorta
        call pinta_inm
        call pinta_edita
        HV dc_cerrar
        jmp hecho

; recorta el dibujo de la ventana al rectángulo EAX=x EBX=y ECX=ancho
; EDX=alto. Los recortes de un contexto de dibujo se van sumando (cada uno
; quita más), así que para recortar a otro sitio se cierra el contexto y se
; abre otro, de toda la ventana. Conserva los registros
recorta:
        pushad
        HV dc_cerrar
        mov ebp, [ven]
        HV dc_abrir
        popad
        HV dc_cortar
        ret

; ¿lo elegido es una sola celda? ZF=1 si sí. Conserva los registros
sel_una:
        push eax
        mov eax, [cur_col]
        cmp eax, [sel_c2]
        jne .x
        mov eax, [cur_fil]
        cmp eax, [sel_f2]
.x:
        pop eax
        ret

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
        COLOR C_TINTA               ; (delante de las muestras del fondo)
        mov eax, FON_X
        mov ebx, 35
        mov esi, t_b_fondo
        HV dc_texto
        popad
        ret
; lo que pregunta la barra común de cada botón (EDI): EBP=1 si está puesto
; ahora, según el formato de la celda activa
barra_estado:
        xor ebp, ebp
        mov eax, [edi+12]
        cmp eax, O_PRELIM
        jne .np
        movzx ebp, byte [vp_on]
        ret
.np:
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
        jbe .igual
        sub eax, O_FONDO            ; el fondo: el de su botón
        cmp eax, N_FONDOS
        jae .x
        movzx eax, byte [fondos+eax]
        shr edx, F_FONDO_D
        and edx, 15
        cmp eax, edx
        jne .x
        inc ebp
        ret
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
        call pinta_edt_sel
        COLOR C_TINTA
        sub edx, edi                ; el cursor: una raya
        lea eax, [eax+edx*8]
        inc ebx
        mov ecx, 2
        mov edx, 14
        HV dc_rect
        popad
        ret

; (interna) las letras elegidas de lo que se teclea, en petróleo con letra
; clara: EAX=x EBX=y EDI=la primera letra que se ve ECX=cuántas se ven
pinta_edt_sel:
        pushad
        mov esi, eax
        mov ebp, ecx
        call edt_sel                ; EAX=la primera ECX=cuántas
        jz .x
        lea edx, [eax+ecx]
        cmp eax, edi                ; lo que de ellas se ve
        jae .a1
        mov eax, edi
.a1:
        add ebp, edi
        cmp edx, ebp
        jbe .a2
        mov edx, ebp
.a2:
        sub edx, eax
        jle .x
        mov ecx, edx
        mov edx, eax
        sub edx, edi
        lea esi, [esi+edx*8]        ; ESI=su x
        COLOR C_PETROLEO
        push eax
        push ecx
        mov eax, esi
        shl ecx, 3
        mov edx, 16
        HV dc_rect
        pop ecx
        pop eax
        COLOR C_CREMA
        mov edx, esi
        lea esi, [edt_buf+eax]
        mov eax, edx
        HV dc_texto_n
.x:
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
        cmp byte [vp_on], 0
        je .nv
        call vp_pie
        jmp .pon
.nv:
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

; ---- los rótulos de las columnas y de las filas: primero los de las
;      inmovilizadas (si las hay) y luego los de las que se mueven ----
pinta_rotulos:
        pushad
        xor eax, eax
        mov ebx, HY0
        mov ecx, [gx1]
        mov edx, [gy1]
        sub edx, ebx
        call recorta
        COLOR C_PIEDRA              ; el fondo de los rótulos y la esquina
        HV dc_rect                  ; (pulsarla lo elige todo)
        mov eax, [gx0]              ; las columnas que se mueven
        mov ebx, HY0
        mov ecx, [gx1]
        sub ecx, eax
        jle .cf
        mov edx, [gy1]
        sub edx, ebx
        call recorta
        mov ecx, [desp_col]
        mov esi, NCOL
        call .cols
        cmp eax, [gx1]              ; a la derecha de la última columna
        jge .cf
        COLOR C_CREMA
        mov ebx, HY0
        mov ecx, [gx1]
        sub ecx, eax
        mov edx, [gy1]
        HV dc_rect
.cf:
        cmp dword [inm_c], 0        ; y las inmovilizadas
        je .filas
        mov eax, ROT_AN
        mov ebx, HY0
        mov ecx, [gx0]
        sub ecx, eax
        mov edx, ROT_AL
        call recorta
        xor ecx, ecx
        mov esi, [inm_c]
        call .cols
.filas:
        xor eax, eax                ; las filas que se mueven
        mov ebx, [gy0]
        mov ecx, ROT_AN
        mov edx, [gy1]
        sub edx, ebx
        jle .ff
        call recorta
        mov edx, [desp_fil]
        mov esi, NFIL
        call .fils
.ff:
        cmp dword [inm_f], 0
        je .x
        xor eax, eax
        mov ebx, GY0
        mov ecx, ROT_AN
        mov edx, [gy0]
        sub edx, ebx
        call recorta
        xor edx, edx
        mov ebx, GY0
        mov esi, [inm_f]
        call .fils
.x:
        popad
        ret
; (interna) los rótulos de las columnas, de la ECX (en la x EAX) a la ESI
; (sin llegar) o hasta el borde -> EAX=la x donde acaban
.cols:
        cmp ecx, esi
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
        push esi
        mov esi, ref_txt
        HV dc_texto
        pop esi
.c2:
        pop edx
        pop eax
        pop ecx
        add eax, edx
        inc ecx
        jmp .cols
.colf:
        ret
; (interna) los rótulos de las filas, de la EDX (en la y EBX) a la ESI
.fils:
        cmp edx, esi
        jae .filf
        cmp ebx, [gy1]
        jge .filf
        push edx
        movzx edi, byte [fil_al+edx]        ; EDI=su alto
        COLOR C_PIEDRA
        cmp edx, [r_f1]
        jb .f1
        cmp edx, [r_f2]
        ja .f1
        COLOR C_ARENA
.f1:
        xor eax, eax
        mov ecx, ROT_AN
        mov edx, edi
        HV dc_rect
        COLOR C_ARENA_OSC
        push ebx
        lea ebx, [ebx+edi-1]
        mov edx, 1
        HV dc_rect
        pop ebx
        cmp edi, 14                 ; (en una fila muy baja no cabe el número)
        jb .f2
        mov eax, [esp]
        inc eax
        push edi
        mov edi, ref_txt
        call hvl_num
        mov byte [edi], 0
        sub edi, ref_txt            ; el número, arrimado a la derecha y abajo
        shl edi, 3
        mov eax, ROT_AN - 4
        sub eax, edi
        pop edi
        push ebx
        lea ebx, [ebx+edi-17]
        COLOR C_TINTA
        push esi
        mov esi, ref_txt
        HV dc_texto
        pop esi
        pop ebx
.f2:
        pop edx
        add ebx, edi
        inc edx
        jmp .fils
.filf:
        ret

; ---- las zonas de la rejilla. Con filas y columnas inmovilizadas son
;      cuatro: la esquina (filas y columnas quietas), la de arriba (filas
;      quietas), la de la izquierda (columnas quietas) y la que se mueve.
;      Cada una se pinta igual, recortada a su sitio ----
; prepara la zona ESI (bit 0: sus columnas se mueven; bit 1: sus filas) y
; recorta el dibujo a ella. CF=1 si no tiene nada
zona_pon:
        pushad
        test esi, 1
        jnz .cm
        mov eax, [inm_c]            ; columnas quietas: de la 0 a la última
        test eax, eax               ; inmovilizada
        jz .nada
        mov [z_cn], eax
        xor eax, eax
        mov [z_c0], eax
        mov [z_cb], eax
        mov dword [z_xa], ROT_AN
        mov eax, [gx0]
        jmp .c1
.cm:
        mov eax, [desp_col]
        mov [z_c0], eax
        mov eax, [inm_c]
        mov [z_cb], eax
        mov dword [z_cn], NCOL
        mov eax, [gx0]
        mov [z_xa], eax
        mov eax, [gx1]
.c1:
        cmp eax, [gx1]
        jle .c2
        mov eax, [gx1]
.c2:
        mov [z_xb], eax
        cmp eax, [z_xa]
        jle .nada
        test esi, 2
        jnz .fm
        mov eax, [inm_f]
        test eax, eax
        jz .nada
        mov [z_fn], eax
        xor eax, eax
        mov [z_f0], eax
        mov [z_fb], eax
        mov dword [z_ya], GY0
        mov eax, [gy0]
        jmp .f1
.fm:
        mov eax, [desp_fil]
        mov [z_f0], eax
        mov eax, [inm_f]
        mov [z_fb], eax
        mov dword [z_fn], NFIL
        mov eax, [gy0]
        mov [z_ya], eax
        mov eax, [gy1]
.f1:
        cmp eax, [gy1]
        jle .f2
        mov eax, [gy1]
.f2:
        mov [z_yb], eax
        cmp eax, [z_ya]
        jle .nada
        call zona_corta
        popad
        clc
        ret
.nada:
        popad
        stc
        ret
; recorta el dibujo a la zona
zona_corta:
        pushad
        mov eax, [z_xa]
        mov ebx, [z_ya]
        mov ecx, [z_xb]
        sub ecx, eax
        mov edx, [z_yb]
        sub edx, ebx
        call recorta
        popad
        ret

; ---- una zona: el fondo, lo elegido, las rayas, lo que hay en cada celda y
;      el marco de lo elegido ----
pinta_zona:
        pushad
        COLOR C_BLANCO
        mov eax, [z_xa]
        mov ebx, [z_ya]
        mov ecx, [z_xb]
        sub ecx, eax
        mov edx, [z_yb]
        sub edx, ebx
        HV dc_rect
        ; lo elegido que cae en esta zona, en la ventana (recortado a ella)
        mov byte [s_hay], 0
        mov ecx, [r_c1]
        cmp ecx, [z_cb]
        jae .s1
        mov ecx, [z_cb]
.s1:
        mov eax, [r_c2]
        cmp eax, [z_cn]
        jb .s2
        mov eax, [z_cn]
        dec eax
.s2:
        cmp ecx, eax
        jg .rayas
        push eax
        call col_x
        call .recx
        mov [s_x0], eax
        pop ecx
        call col_x1
        call .recx
        mov [s_x1], eax
        mov edx, [r_f1]
        cmp edx, [z_fb]
        jae .s3
        mov edx, [z_fb]
.s3:
        mov eax, [r_f2]
        cmp eax, [z_fn]
        jb .s4
        mov eax, [z_fn]
        dec eax
.s4:
        cmp edx, eax
        jg .rayas
        push eax
        call fil_y
        call .recy
        mov [s_y0], ebx
        pop edx
        call fil_y1
        call .recy
        mov [s_y1], ebx
        mov eax, [s_x1]
        cmp eax, [s_x0]
        jle .rayas
        mov eax, [s_y1]
        cmp eax, [s_y0]
        jle .rayas
        mov byte [s_hay], 1
        cmp dword [sel_varias], 0
        je .rayas
        COLOR C_PETROLEO            ; varias celdas: en petróleo, menos la activa
        mov eax, [s_x0]
        mov ebx, [s_y0]
        mov ecx, [s_x1]
        sub ecx, eax
        mov edx, [s_y1]
        sub edx, ebx
        HV dc_rect
        mov ecx, [cur_col]          ; (si la activa es de esta zona)
        cmp ecx, [z_cb]
        jb .rayas
        cmp ecx, [z_cn]
        jae .rayas
        mov edx, [cur_fil]
        cmp edx, [z_fb]
        jb .rayas
        cmp edx, [z_fn]
        jae .rayas
        COLOR C_BLANCO
        call col_x
        mov esi, eax
        call cel_ancho
        mov ecx, eax
        mov eax, esi
        call fil_y
        movzx edx, byte [fil_al+edx]
        cmp ebx, GY0 - AL_MAX
        jl .rayas
        cmp ebx, [gy1]
        jg .rayas
        HV dc_rect
.rayas:
        COLOR C_ARENA_OSC
        mov ecx, [z_c0]             ; las verticales
        mov eax, [z_xa]
.v:
        cmp eax, [z_xb]
        jg .vf
        push ecx
        mov ebx, [z_ya]
        mov ecx, 1
        mov edx, [gy1]
        HV dc_rect
        pop ecx
        cmp ecx, [z_cn]
        jae .vf
        movzx edx, word [col_an+ecx*2]
        add eax, edx
        inc ecx
        jmp .v
.vf:
        mov edx, [z_f0]             ; y las horizontales
        mov ebx, [z_ya]
.h:
        cmp ebx, [z_yb]
        jg .hf
        push edx
        mov eax, [z_xa]
        mov ecx, [gx1]
        mov edx, 1
        HV dc_rect
        pop edx
        cmp edx, [z_fn]
        jae .hf
        movzx eax, byte [fil_al+edx]
        add ebx, eax
        inc edx
        jmp .h
.hf:
        mov edx, [z_f0]             ; lo que hay escrito, fila a fila
        mov ebx, [z_ya]
.fila:
        cmp edx, [z_fn]
        jae .marco
        cmp ebx, [z_yb]
        jge .marco
        call pinta_fila
        movzx eax, byte [fil_al+edx]
        add ebx, eax
        inc edx
        jmp .fila
.marco:
        cmp byte [s_hay], 0
        je .senala
        COLOR C_TINTA               ; el marco de lo elegido: dos puntos de grueso
        mov eax, [s_x0]
        mov ebx, [s_y0]
        mov ecx, [s_x1]
        sub ecx, eax
        mov edx, [s_y1]
        sub edx, ebx
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
.senala:
        cmp byte [editando], 0
        je .x
        cmp byte [pt_activo], 0     ; lo señalado para la fórmula: un marco terracota
        je .x
        COLOR C_TERRACOTA
        mov ecx, [pt_c1]
        cmp ecx, [pt_c2]
        jbe .p1
        mov ecx, [pt_c2]
.p1:
        call col_x
        call .recx
        mov esi, eax
        mov ecx, [pt_c1]
        cmp ecx, [pt_c2]
        jae .p2
        mov ecx, [pt_c2]
.p2:
        call col_x1
        call .recx
        mov edi, eax
        mov edx, [pt_f1]
        cmp edx, [pt_f2]
        jbe .p3
        mov edx, [pt_f2]
.p3:
        call fil_y
        call .recy
        mov ebp, ebx
        mov edx, [pt_f1]
        cmp edx, [pt_f2]
        jae .p4
        mov edx, [pt_f2]
.p4:
        call fil_y1
        call .recy
        mov edx, ebx
        sub edx, ebp
        jle .x
        mov ecx, edi
        sub ecx, esi
        jle .x
        mov eax, esi
        mov ebx, ebp
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
.x:
        popad
        ret
; (internas) recortan una x o una y a los bordes de la zona, con dos puntos
; de sobra (para que el marco de lo que se sale no se vea)
.recx:
        push ebx
        mov ebx, [z_xa]
        sub ebx, 2
        cmp eax, ebx
        jge .rx1
        mov eax, ebx
.rx1:
        mov ebx, [z_xb]
        add ebx, 2
        cmp eax, ebx
        jle .rx2
        mov eax, ebx
.rx2:
        pop ebx
        ret
.recy:
        push eax
        mov eax, [z_ya]
        sub eax, 2
        cmp ebx, eax
        jge .ry1
        mov ebx, eax
.ry1:
        mov eax, [z_yb]
        add eax, 2
        cmp ebx, eax
        jle .ry2
        mov ebx, eax
.ry2:
        pop eax
        ret

; ---- las rayas que separan lo inmovilizado de lo que se mueve ----
pinta_inm:
        pushad
        COLOR C_TINTA
        cmp dword [inm_c], 0
        je .f
        mov eax, [gx0]
        mov ebx, GY0
        mov ecx, 1
        mov edx, [gy1]
        HV dc_rect
.f:
        cmp dword [inm_f], 0
        je .x
        mov eax, ROT_AN
        mov ebx, [gy0]
        mov ecx, [gx1]
        mov edx, 1
        HV dc_rect
.x:
        popad
        ret

; ---- lo que se teclea, sobre la celda (y sobre las de su derecha, si no
;      cabe) ----
pinta_edita:
        pushad
        cmp byte [editando], 0
        je .x
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call col_x
        cmp eax, ROT_AN
        jl .x
        cmp ecx, [inm_c]            ; (una celda que se mueve y ha quedado
        jb .e0                      ; debajo de las inmovilizadas no se ve)
        cmp eax, [gx0]
        jl .x
.e0:
        call fil_y
        cmp ebx, GY0
        jl .x
        cmp edx, [inm_f]
        jb .e00
        cmp ebx, [gy0]
        jl .x
.e00:
        push eax
        movzx eax, byte [fil_al+edx]        ; (en una fila alta, abajo: donde
        sub eax, CEL_AL             ; está lo escrito)
        jle .e01
        add ebx, eax
.e01:
        call cel_ancho
        mov ecx, eax
        pop eax
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

; (interna) lo escrito en la fila EDX, que está en la y EBX, dentro de la
; zona. Empieza por la última celda con algo (o combinada) que quede a la
; izquierda de lo que se ve: si es un texto largo, se desborda hasta aquí
pinta_fila:
        pushad
        mov [pf_y], ebx
        movzx eax, byte [fil_al+edx]
        mov [pf_al], eax
        cmp eax, CEL_AL             ; una fila baja: lo escrito no puede
        jae .nr                     ; salirse de ella
        push edx
        mov ecx, [z_xb]
        mov edx, eax
        dec edx
        mov eax, [z_xa]
        sub ecx, eax
        inc ebx
        mov esi, [z_yb]             ; (ni de la zona)
        sub esi, ebx
        cmp edx, esi
        jle .nr0
        mov edx, esi
.nr0:
        mov esi, [z_ya]
        sub esi, ebx
        jle .nr1
        add ebx, esi
        sub edx, esi
.nr1:
        test edx, edx
        jg .nr2
        pop edx
        jmp .bordes
.nr2:
        call recorta
        pop edx
        mov ebx, [pf_y]
.nr:
        mov esi, edx
        shl esi, 6                  ; ESI=la primera celda de la fila
        mov ecx, [z_c0]
.atras:
        cmp ecx, [z_cb]
        jbe .desde
        dec ecx
        lea edi, [esi+ecx]
        cmp dword [cel_off+edi*4], 0
        jne .ya
        test dword [cel_fmt+edi*4], F_COMB
        jz .atras
        jmp .ya
.desde:
        mov ecx, [z_c0]
.ya:
        call col_x                  ; EAX=la x de la columna ECX
.c:
        cmp ecx, [z_cn]
        jae .bordes
        cmp eax, [z_xb]
        jge .bordes
        lea edi, [esi+ecx]
        mov ebp, [cel_fmt+edi*4]
        test ebp, F_FONDO | F_COMB
        jnz .p
        cmp dword [cel_off+edi*4], 0
        je .s
.p:
        call pinta_celda
.s:
        shr ebp, F_COMB_D           ; detrás de ella y de las que tapa
        and ebp, 63
.s1:
        movzx edi, word [col_an+ecx*2]
        add eax, edi
        inc ecx
        sub ebp, 1
        jc .c
        cmp ecx, [z_cn]
        jb .s1
        jmp .c
.bordes:
        cmp dword [pf_al], CEL_AL
        jae .b00
        call zona_corta
.b00:
        mov edx, [esp+20]           ; (la fila)
        mov esi, edx
        shl esi, 6
        mov ebx, [pf_y]
        mov ecx, [z_c0]             ; los bordes de las celdas que se ven (y el
        cmp ecx, [z_cb]             ; de la derecha de la que queda justo antes)
        jbe .b0
        dec ecx
.b0:
        mov dword [pf_resto], 0     ; (de una combinada solo cuenta la raya de
        test ecx, ecx               ; la derecha de su última celda)
        jz .b01
        dec ecx
        call comb_r
        inc ecx
        mov [pf_resto], eax
.b01:
        call col_x
.b:
        cmp ecx, [z_cn]
        jae .bx
        cmp eax, [z_xb]
        jge .bx
        movzx edi, word [col_an+ecx*2]
        lea edx, [esi+ecx]
        mov edx, [cel_fmt+edx*4]
        mov ebp, edx
        shr ebp, F_COMB_D
        and ebp, 63
        jz .br1
        mov [pf_resto], ebp
        jmp .br2
.br1:
        cmp dword [pf_resto], 0
        je .br2
        dec dword [pf_resto]
.br2:
        cmp dword [pf_resto], 0
        je .br3
        and edx, ~F_BDER
.br3:
        test edx, F_BABA | F_BDER
        jz .bs
        COLOR C_TINTA
        push ecx
        test edx, F_BABA
        jz .b1
        push ebx
        push edx
        add ebx, [pf_al]
        lea ecx, [edi+1]
        mov edx, 1
        HV dc_rect
        pop edx
        pop ebx
.b1:
        test edx, F_BDER
        jz .b2
        push eax
        add eax, edi
        mov ecx, 1
        mov edx, [pf_al]
        inc edx
        HV dc_rect
        pop eax
.b2:
        pop ecx
.bs:
        add eax, edi
        inc ecx
        jmp .b
.bx:
        popad
        ret

; (interna) la celda ECX=columna EDX=fila, que está en EAX=x EBX=y: su fondo
; (si tiene color, o tapa a otras) y lo que hay escrito
pinta_celda:
        pushad
        mov [pc_x], eax
        mov [pc_y], ebx
        call cel_ancho
        mov [pc_an], eax
        call celda_ver
        ; los colores: lo elegido (menos la celda activa), en claro sobre petróleo
        mov eax, [disp_fmt]
        mov ebx, eax
        shr eax, F_COLOR_D
        and eax, 7
        mov al, [colores+eax]
        shr ebx, F_FONDO_D
        and ebx, 15
        jnz .f0
        mov bl, C_BLANCO
.f0:
        mov ah, bl
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
        test dword [disp_fmt], F_FONDO | F_COMB
        jz .txt
        push ecx                    ; el fondo: por dentro de las rayas
        push edx
        movzx eax, byte [tc_bg]
        HV dc_color
        mov eax, [pc_x]
        inc eax
        mov ebx, [pc_y]
        inc ebx
        mov ecx, [pc_an]
        dec ecx
        mov edx, [pf_al]
        dec edx
        jle .f1
        HV dc_rect
.f1:
        pop edx
        pop ecx
.txt:
        xor edi, edi                ; EDI=las letras que tiene
.l:
        cmp byte [disp+edi], 0
        je .lf
        inc edi
        jmp .l
.lf:
        test edi, edi
        jz .x
        mov ebp, [pc_an]
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
        test dword [disp_fmt], F_COMB       ; (ni una combinada)
        jnz .corta
        ; un texto largo: sobre las celdas vacías (y sin color) de su derecha
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
        cmp ecx, [z_cn]
        jae .desf
        cmp eax, [z_xb]
        jge .desf
        lea ebx, [esi+ecx]
        cmp dword [cel_off+ebx*4], 0
        jne .desf
        test dword [cel_fmt+ebx*4], F_FONDO | F_COMB
        jnz .desf
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
        mov edx, [pf_al]
        dec edx
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
        mov ebx, [pc_y]             ; abajo de la celda (en una fila de alto
        add ebx, [pf_al]            ; normal, un punto por debajo de la raya)
        sub ebx, 17
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
gx0         resd 1                  ; donde empiezan las columnas que se mueven
gy0         resd 1                  ; y las filas (detrás de las inmovilizadas)
inm_c       resd 1                  ; columnas inmovilizadas (las primeras)
inm_f       resd 1                  ; y filas
z_c0        resd 1                  ; la zona que se pinta: su primera columna,
z_cb        resd 1                  ; la primera que le toca (aunque no se vea),
z_cn        resd 1                  ; y la siguiente a la última;
z_xa        resd 1                  ; de qué x a cuál;
z_xb        resd 1
z_f0        resd 1                  ; y lo mismo, de las filas
z_fb        resd 1
z_fn        resd 1
z_ya        resd 1
z_yb        resd 1
pf_y        resd 1                  ; pinta_fila: su y,
pf_al       resd 1                  ; su alto
pf_resto    resd 1                  ; y lo que queda de una combinada
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
s_hay       resb 1                  ; lo elegido cae en la zona que se pinta
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
