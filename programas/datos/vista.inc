; =============================================================================
;  DATOS.HVP - lo que se ve: la barra, la rejilla, la ficha y el pie
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La rejilla: un encabezado con los nombres de los campos (el del orden, con
;  su flecha) y una fila por registro; el registro elegido va en petróleo y,
;  dentro de él, el campo elegido en ocre. La ficha: un papel con el registro
;  elegido, un campo por línea. Abajo, el pie de toda la familia: lo que hace
;  el botón que hay bajo el ratón o, si no, la cuenta de registros.
; =============================================================================

%macro VISTA_BSS 0
g_an        resd 1                  ; la rejilla: su ancho
g_y1        resd 1                  ;   y donde acaba
g_filas     resd 1                  ;   filas que se pintan
c_x         resd 1                  ; la columna que se pinta: su x,
c_an        resd 1                  ;   su ancho
c_i         resd 1                  ;   y su campo
f_an        resd 1                  ; la ficha: el ancho del papel
f_y1        resd 1
f_reg       resd 1
%endmacro

; las medidas, los topes y las barras de desplazamiento, al día
ajusta:
        pushad
        cmp dword [barra_v], 0
        je .x
        cmp byte [vista], 0
        jne .ficha
        mov eax, [cli_al]           ; cuántas filas caben
        sub eax, BR_PIE + BD_AN + GY0
        jns .a
        xor eax, eax
.a:
        xor edx, edx
        mov ecx, FIL_AL
        div ecx
        test eax, eax
        jnz .a1
        inc eax
.a1:
        mov [vis_fil], eax
        mov ecx, [n_vis]
        sub ecx, eax
        jns .a2
        xor ecx, ecx
.a2:
        cmp [top], ecx
        jbe .a3
        mov [top], ecx
.a3:
        mov edx, eax
        mov ebp, [barra_v]
        mov eax, BDM_PONER
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [top]
        HV msg_enviar
        mov eax, [n_cam]            ; y a lo ancho
        call col_x
        mov edx, [cli_an]
        sub edx, BD_AN
        sub eax, edx
        jns .b
        xor eax, eax
.b:
        cmp [xdesp], eax
        jbe .b1
        mov [xdesp], eax
.b1:
        mov ecx, eax
        mov ebp, [barra_h]
        mov eax, BDM_PONER
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [xdesp]
        HV msg_enviar
        jmp .x
.ficha:
        mov eax, [cli_al]           ; cuántos campos caben en el papel
        sub eax, BR_PIE + 12 + FCAM_Y
        jns .f
        xor eax, eax
.f:
        xor edx, edx
        mov ecx, FCAM_AL
        div ecx
        test eax, eax
        jnz .f1
        inc eax
.f1:
        mov [vis_cam], eax
        mov ecx, [n_cam]
        sub ecx, eax
        jns .f2
        xor ecx, ecx
.f2:
        cmp [ftop], ecx
        jbe .f3
        mov [ftop], ecx
.f3:
        mov edx, eax
        mov ebp, [barra_v]
        mov eax, BDM_PONER
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [ftop]
        HV msg_enviar
.x:
        popad
        ret

pintar:
        HV dc_pintar
        call pinta_barra
        call pinta_pie
        cmp byte [vista], 0
        jne .ficha
        call pinta_tabla
        jmp .cierra
.ficha:
        call pinta_ficha
.cierra:
        HV dc_cerrar
        jmp hecho

; ---- la barra de botones ----
pinta_barra:
        pushad
        mov ecx, [cli_an]
        mov edx, TB_AL
        call br_fondo
        mov edi, bt_buf             ; el botón de las tablas dice cuál se ve
        mov esi, t_bt_sin
        mov ebx, [tab]
        test ebx, ebx
        jz .n
        mov esi, t_bt_tabla
        call hvl_cad
        lea esi, [ebx+T_NOM]
.n:
        call hvl_cad
        mov byte [edi], 0
        mov edi, botones
        mov ebx, 4
        call br_fila
        popad
        ret
; lo que pregunta la barra común: EBP=1 en la vista que se ve y en el botón
; del filtro si lo hay
barra_estado:
        mov eax, [edi+12]
        cmp eax, O_FILTRAR
        jne .v
        cmp byte [f_on], 0
        je .x
        inc ebp
        ret
.v:
        sub eax, O_V_TABLA
        cmp eax, 1
        ja .x
        cmp al, [vista]
        jne .x
        inc ebp
.x:
        ret

; ---- el pie: la cuenta de registros, el filtro y el orden; a la derecha,
;      la suma del campo elegido si es un número ----
pinta_pie:
        pushad
        mov ecx, [cli_an]
        mov edx, [cli_al]
        call br_pie
        jc .x
        mov edi, pie_buf
        mov ebx, [tab]
        test ebx, ebx
        jnz .hay
        mov esi, t_p_sintab
        call hvl_cad
        jmp .pon
.hay:
        mov eax, [n_vis]
        cmp byte [f_on], 0
        je .c1
        call hvl_num
        mov esi, t_p_de
        call hvl_cad
.c1:
        mov eax, [n_reg]
        call hvl_num
        mov esi, t_p_reg
        cmp eax, 1
        jne .c2
        mov esi, t_p_reg1
.c2:
        call hvl_cad
        push ebx
        call rango
        jc .sinr
        mov al, ','
        stosb
        mov al, ' '
        stosb
        mov eax, edx
        sub eax, ebx
        inc eax
        call hvl_num
        mov esi, t_p_eleg
        call hvl_cad
.sinr:
        pop ebx
        cmp byte [f_on], 0
        je .o
        mov esi, t_p_filtro
        call hvl_cad
        call filtro_txt
.o:
        movzx eax, byte [ebx+T_ORDC]
        cmp eax, [n_cam]
        jae .pon
        mov esi, t_p_orden
        call hvl_cad
        push ebx
        call cam_ptr
        mov esi, ebx
        pop ebx
        call hvl_cad
        cmp byte [ebx+T_ORDD], 0
        je .pon
        mov esi, t_p_desc
        call hvl_cad
.pon:
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, BR_PIE - 3
        mov esi, pie_buf
        HV dc_texto
        ; la suma del campo elegido
        cmp dword [tab], 0
        je .x
        mov eax, [col]
        cmp byte [tipos+eax], T_NUM
        jne .x
        cmp dword [n_vis], 0
        je .x
        call suma_campo
        mov edi, pie_buf
        mov esi, t_p_suma
        call hvl_cad
        mov eax, [col]
        call cam_ptr
        mov esi, ebx
        call hvl_cad
        mov ax, ': '
        stosw
        mov eax, [suma_lo]
        mov edx, [suma_hi]
        mov ebx, [col]
        movzx ecx, byte [decs+ebx]
        call fmt_num64
        mov byte [edi], 0
        mov esi, pie_buf
        call largo
        shl ecx, 3
        mov eax, [cli_an]
        sub eax, ecx
        sub eax, 8
        mov ebx, [cli_al]
        sub ebx, BR_PIE - 3
        HV dc_texto
.x:
        popad
        ret

; (interna) el filtro, con palabras, a EDI: «Población contiene «alb» y
; Cuota es mayor que «10»»
filtro_txt:
        push esi
        mov esi, fc1
        call cond_txt
        cmp byte [f_dos], 0
        je .x
        mov esi, t_p_y
        cmp byte [f_dos], 1
        je .y
        mov esi, t_p_o
.y:
        call hvl_cad
        mov esi, fc2
        call cond_txt
.x:
        pop esi
        ret
; (interna) la condición ESI, con palabras, a EDI
cond_txt:
        push eax
        push ebx
        push ecx
        push esi
        mov ecx, esi
        mov eax, [ecx+FC_CAMPO]
        call cam_ptr
        mov esi, ebx
        call hvl_cad
        mov al, ' '
        stosb
        mov eax, [ecx+FC_OP]
        mov esi, [op_nom+eax*4]
        call hvl_cad
        cmp eax, 4
        je .x
        mov ax, ' «'
        stosw
        lea esi, [ecx+FC_TXT]
        mov ecx, 40
.c:
        lodsb
        test al, al
        jz .c1
        stosb
        dec ecx
        jnz .c
.c1:
        mov al, '»'
        stosb
.x:
        pop esi
        pop ecx
        pop ebx
        pop eax
        ret

; la suma del campo elegido en lo que se ve -> [suma_lo], [suma_hi]; se guarda
; hasta que cambie algo ([suma_ok])
suma_campo:
        cmp byte [suma_ok], 0
        jne .x
        pushad
        xor edi, edi                ; EBP:EDI=la suma
        xor ebp, ebp
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
        mov eax, [esi]
        cmp eax, VACIO
        je .s
        cdq
        add edi, eax
        adc ebp, edx
.s:
        inc ecx
        jmp .b
.f:
        mov [suma_lo], edi
        mov [suma_hi], ebp
        mov byte [suma_ok], 1
        popad
.x:
        ret

; EDX:EAX=número (con signo) con ECX decimales -> su texto en EDI, que
; avanza, con coma y puntos de millares
fmt_num64:
        pushad
        mov ebp, ecx
        test edx, edx
        jns .p
        not edx
        neg eax
        sbb edx, -1
        mov byte [edi], '-'
        inc edi
.p:
        mov esi, num_tmp
        xor ecx, ecx
.d:
        push ecx                    ; EDX:EAX entre 10, en dos pasos
        mov ecx, eax
        mov eax, edx
        xor edx, edx
        mov ebx, 10
        div ebx
        mov ebx, eax                ; la parte alta del cociente
        mov eax, ecx
        mov ecx, 10
        div ecx
        pop ecx
        add dl, '0'
        mov [esi+ecx], dl
        inc ecx
        mov edx, ebx
        or ebx, eax
        jnz .d
.pad:
        cmp ecx, ebp
        ja .e
        mov byte [esi+ecx], '0'
        inc ecx
        jmp .pad
.e:
        dec ecx
        mov al, [esi+ecx]
        stosb
        test ecx, ecx
        jz .f
        cmp ecx, ebp
        jb .e
        jne .e1
        mov al, ','
        stosb
        jmp .e
.e1:
        mov eax, ecx
        sub eax, ebp
        xor edx, edx
        push ecx
        mov ecx, 3
        div ecx
        pop ecx
        test edx, edx
        jnz .e
        mov al, '.'
        stosb
        jmp .e
.f:
        mov [esp], edi
        popad
        ret

; (interna) EAX=registro ECX=campo -> ESI=su texto para pintarlo (en vbuf, sin
; saltos de línea) y ECX=lo que mide (como mucho 120)
celda:
        push eax
        mov byte [fmt_mil], 1
        call reg_txt
        xor ecx, ecx
.b:
        mov al, [esi+ecx]
        test al, al
        jz .x
        cmp al, ' '
        jae .s
        mov byte [esi+ecx], ' '
.s:
        inc ecx
        cmp ecx, 120
        jb .b
.x:
        pop eax
        ret

; (interna) un mensaje centrado en la zona de los datos: ESI=texto EBX=y
centrado:
        pushad
        call largo
        shl ecx, 3
        mov eax, [cli_an]
        sub eax, BD_AN
        sub eax, ecx
        sar eax, 1
        jns .a
        xor eax, eax
.a:
        HV dc_texto
        popad
        ret

; ---- la rejilla ----
pinta_tabla:
        pushad
        mov eax, [cli_an]
        sub eax, BD_AN
        mov [g_an], eax
        mov eax, [cli_al]
        sub eax, BR_PIE + BD_AN
        mov [g_y1], eax
        COLOR C_ARENA               ; la esquina entre las dos barras
        mov eax, [g_an]
        mov ebx, [g_y1]
        mov ecx, BD_AN
        mov edx, BD_AN
        HV dc_rect
        xor eax, eax                ; y desde aquí, solo dentro de la rejilla
        mov ebx, TB_AL
        mov ecx, [g_an]
        mov edx, [g_y1]
        sub edx, TB_AL
        jle .x
        HV dc_cortar
        cmp dword [tab], 0
        jne .hay
        COLOR C_TINTA
        mov esi, t_sin_tab
        mov ebx, GY0 + 40
        call centrado
        mov esi, t_sin_tab2
        mov ebx, GY0 + 62
        call centrado
        jmp .x
.hay:
        COLOR C_ARENA               ; el encabezado
        xor eax, eax
        mov ebx, TB_AL
        mov ecx, [g_an]
        mov edx, CAB_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        mov ebx, TB_AL + CAB_AL - 1
        mov edx, 1
        HV dc_rect
        mov eax, [n_vis]            ; cuántas filas se pintan
        sub eax, [top]
        mov ecx, [vis_fil]
        inc ecx                     ; (y la que asoma a medias)
        cmp eax, ecx
        jbe .f0
        mov eax, ecx
.f0:
        mov [g_filas], eax
        test eax, eax
        jz .cols
        COLOR C_BLANCO              ; el papel de las filas
        imul edx, eax, FIL_AL
        xor eax, eax
        mov ebx, GY0
        mov ecx, [g_an]
        HV dc_rect
        COLOR C_PETROLEO            ; las filas elegidas
        xor esi, esi
.el:
        cmp esi, [g_filas]
        jae .rayas
        mov edx, [top]
        add edx, esi
        call fila_elegida
        jne .el1
        imul ebx, esi, FIL_AL
        add ebx, GY0
        xor eax, eax
        mov ecx, [g_an]
        mov edx, FIL_AL - 1
        HV dc_rect
.el1:
        inc esi
        jmp .el
.rayas:
        COLOR C_ARENA               ; las rayas entre filas
        xor esi, esi
.ry:
        cmp esi, [g_filas]
        jae .cols
        imul ebx, esi, FIL_AL
        add ebx, GY0 + FIL_AL - 1
        xor eax, eax
        mov ecx, [g_an]
        mov edx, 1
        HV dc_rect
        inc esi
        jmp .ry
.cols:
        mov dword [c_i], 0
        mov eax, [xdesp]
        neg eax
        mov [c_x], eax
.c:
        mov eax, [c_i]
        cmp eax, [n_cam]
        jae .fin
        call cam_ptr
        movzx eax, word [ebx+C_ANCHO]
        mov [c_an], eax
        add eax, [c_x]
        jle .csig                   ; a la izquierda de lo que se ve
        mov eax, [c_x]
        cmp eax, [g_an]
        jge .fin                    ; y esta, ya a la derecha
        call pinta_columna
.csig:
        mov eax, [c_an]
        add [c_x], eax
        inc dword [c_i]
        jmp .c
.fin:
        cmp dword [g_filas], 0
        jne .x
        COLOR C_TINTA
        mov esi, t_sin_reg
        cmp byte [f_on], 0
        je .v
        mov esi, t_sin_fil
.v:
        mov ebx, GY0 + 40
        call centrado
.x:
        popad
        ret

; (interna) la columna [c_i], que empieza en [c_x] y mide [c_an]
pinta_columna:
        pushad
        mov eax, [c_i]
        call cam_ptr                ; EBX=el campo
        ; el nombre (en negrita el campo elegido) y la flecha del orden
        COLOR C_TINTA
        mov ecx, [c_an]
        sub ecx, 8
        mov edi, [tab]
        movzx eax, byte [edi+T_ORDC]
        cmp eax, [c_i]
        jne .n0
        sub ecx, 12                 ; (sitio para la flecha)
.n0:
        sar ecx, 3
        jle .n3
        mov esi, ebx
        push ecx
        call largo
        mov eax, ecx
        pop ecx
        cmp ecx, eax
        jbe .n1
        mov ecx, eax
.n1:
        mov eax, [c_i]
        cmp eax, [col]
        jne .n2
        NEGRITA 1
.n2:
        mov eax, [c_x]
        add eax, 4
        push ebx
        mov ebx, TB_AL + 2
        HV dc_texto_n
        pop ebx
        NEGRITA 0
.n3:
        movzx eax, byte [edi+T_ORDC]
        cmp eax, [c_i]
        jne .n4
        mov esi, fl_sube
        cmp byte [edi+T_ORDD], 0
        je .fl
        mov esi, fl_baja
.fl:
        mov eax, [c_x]
        add eax, [c_an]
        sub eax, 13
        push ebx
        mov ebx, TB_AL + 7
        mov ecx, 8
        mov edx, 6
        HV dc_copiar_clave
        pop ebx
.n4:
        COLOR C_ARENA_OSC           ; la raya de la derecha
        mov eax, [c_x]
        add eax, [c_an]
        dec eax
        push ebx
        mov ebx, TB_AL
        mov ecx, 1
        imul edx, [g_filas], FIL_AL
        add edx, CAB_AL
        HV dc_rect
        pop ebx
        ; las celdas
        xor ebp, ebp                ; EBP=la fila
.f:
        cmp ebp, [g_filas]
        jae .x
        mov edx, [top]
        add edx, ebp                ; EDX=su sitio en la vista
        COLOR C_TINTA
        call fila_elegida
        jne .txt
        cmp edx, [sel]
        jne .clara
        mov eax, [c_i]
        cmp eax, [col]
        je .ocre
.clara:
        COLOR C_CREMA               ; fila elegida: letra clara
        jmp .txt
.ocre:
        COLOR C_OCRE                ; y su campo elegido, en ocre
        push edx
        mov eax, [c_x]
        imul ebx, ebp, FIL_AL
        add ebx, GY0
        mov ecx, [c_an]
        dec ecx
        mov edx, FIL_AL - 1
        HV dc_rect
        pop edx
        COLOR C_TINTA
.txt:
        mov esi, [vis]
        mov eax, [esi+edx*4]
        mov ecx, [c_i]
        call celda                  ; ESI=texto ECX=lo que mide
        test ecx, ecx
        jz .sig
        mov edx, [c_an]
        sub edx, 8
        sar edx, 3                  ; las letras que caben
        jle .sig
        mov eax, [c_x]
        add eax, 4
        cmp ecx, edx
        jbe .cabe
        mov ecx, edx
        jmp .pon
.cabe:
        mov edi, [c_i]              ; los números, a la derecha
        cmp byte [tipos+edi], T_NUM
        jne .pon
        mov eax, [c_x]
        add eax, [c_an]
        sub eax, 5
        lea edi, [ecx*8]
        sub eax, edi
.pon:
        imul ebx, ebp, FIL_AL
        add ebx, GY0 + 1
        HV dc_texto_n
.sig:
        inc ebp
        jmp .f
.x:
        popad
        ret
; las flechas del orden (8x6): de menor a mayor y de mayor a menor
fl_sube     db 255, 255, 255, 0, 0, 255, 255, 255
            db 255, 255, 0, 0, 0, 0, 255, 255
            db 255, 255, 0, 0, 0, 0, 255, 255
            db 255, 0, 0, 0, 0, 0, 0, 255
            db 255, 0, 0, 0, 0, 0, 0, 255
            db 0, 0, 0, 0, 0, 0, 0, 0
fl_baja     db 0, 0, 0, 0, 0, 0, 0, 0
            db 255, 0, 0, 0, 0, 0, 0, 255
            db 255, 0, 0, 0, 0, 0, 0, 255
            db 255, 255, 0, 0, 0, 0, 255, 255
            db 255, 255, 0, 0, 0, 0, 255, 255
            db 255, 255, 255, 0, 0, 255, 255, 255

; ---- la ficha ----
pinta_ficha:
        pushad
        cmp dword [tab], 0
        jne .hay
        COLOR C_TINTA
        mov esi, t_sin_tab
        mov ebx, GY0 + 40
        call centrado
        mov esi, t_sin_tab2
        mov ebx, GY0 + 62
        call centrado
        jmp .x
.hay:
        mov eax, [cli_an]           ; el papel
        sub eax, BD_AN + 2 * FX0
        cmp eax, 680
        jle .p1
        mov eax, 680
.p1:
        mov [f_an], eax
        mov edx, [cli_al]
        sub edx, BR_PIE + 8
        mov [f_y1], edx
        sub edx, FY0
        jle .x
        COLOR C_BLANCO
        mov eax, FX0
        mov ebx, FY0
        mov ecx, [f_an]
        HV dc_rect
        COLOR C_TINTA
        HV dc_marco
        ; arriba: la tabla y qué registro es
        NEGRITA 1
        mov esi, [tab]
        add esi, T_NOM
        mov eax, FX0 + 12
        mov ebx, FY0 + 8
        HV dc_texto
        NEGRITA 0
        mov edi, pie_buf
        mov esi, t_f_sin
        cmp dword [n_vis], 0
        je .t1
        mov esi, t_f_reg
        call hvl_cad
        mov eax, [sel]
        inc eax
        call hvl_num
        mov esi, t_p_de
        call hvl_cad
        mov eax, [n_vis]
        call hvl_num
        mov esi, t_nada
.t1:
        call hvl_cad
        mov byte [edi], 0
        mov esi, pie_buf
        call largo
        shl ecx, 3
        mov eax, FX0 - 12
        add eax, [f_an]
        sub eax, ecx
        mov ebx, FY0 + 8
        HV dc_texto
        COLOR C_ARENA_OSC
        mov eax, FX0 + 1
        mov ebx, FY0 + 30
        mov ecx, [f_an]
        sub ecx, 2
        mov edx, 1
        HV dc_rect
        cmp dword [n_vis], 0
        jne .reg
        COLOR C_TINTA
        mov esi, t_sin_reg
        cmp byte [f_on], 0
        je .v
        mov esi, t_sin_fil
.v:
        mov eax, FX0 + 12
        mov ebx, FCAM_Y + 8
        HV dc_texto
        jmp .x
.reg:
        mov eax, [sel]
        mov esi, [vis]
        mov eax, [esi+eax*4]
        mov [f_reg], eax
        mov ebp, [ftop]             ; EBP=el campo
.c:
        cmp ebp, [n_cam]
        jae .x
        mov eax, ebp
        sub eax, [ftop]
        cmp eax, [vis_cam]
        jae .x
        imul edi, eax, FCAM_AL
        add edi, FCAM_Y             ; EDI=su y
        COLOR C_TINTA
        cmp ebp, [col]
        jne .nom
        COLOR C_PETROLEO            ; el campo elegido
        mov eax, FX0 + 2
        mov ebx, edi
        mov ecx, [f_an]
        sub ecx, 4
        mov edx, FCAM_AL - 1
        HV dc_rect
        COLOR C_CREMA
.nom:
        mov eax, ebp                ; el nombre, arrimado a la derecha, y «:»
        call cam_ptr
        mov esi, ebx
        call largo
        shl ecx, 3
        mov eax, FX0 + 176
        sub eax, ecx
        lea ebx, [edi+2]
        HV dc_texto
        mov eax, FX0 + 176
        mov esi, t_dos_puntos
        HV dc_texto
        mov eax, [f_reg]            ; y el valor
        mov ecx, ebp
        call celda
        mov edx, [f_an]
        sub edx, 200
        sar edx, 3
        jle .s
        cmp ecx, edx
        jbe .s1
        mov ecx, edx
.s1:
        mov eax, FX0 + 192
        lea ebx, [edi+2]
        HV dc_texto_n
.s:
        inc ebp
        jmp .c
.x:
        popad
        ret
t_dos_puntos db ":", 0
