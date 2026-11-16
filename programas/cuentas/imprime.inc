; =============================================================================
;  Cuentas - la impresora: como texto, «como se ve» y la vista preliminar
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Sale lo elegido (si es más de una celda) o toda la hoja escrita, por el
;  puerto paralelo (LPT1) con la BIOS, INT 17h, de dos maneras:
;
;  COMO TEXTO (Ctrl+P): rápido, con la letra de la impresora. Una fila por
;  línea, cada columna con el ancho que tiene en la pantalla (en letras) y
;  un espacio entre columnas, cada celda alineada como se ve; las negritas y
;  las cursivas, con los códigos ESC/P (ESC E / ESC F, ESC 4 / ESC 5).
;
;  COMO SE VE: punto por punto, en el modo gráfico de las impresoras de
;  agujas (ESC/P: «ESC * 5», 72 puntos por pulgada, 576 de ancho, en tiras
;  de 8 puntos de alto; el mismo modelo que Pluma). Salen los anchos de las
;  columnas tal cual, las alineaciones, las negritas y las cursivas, los
;  bordes y, si se pide (Ver > Rejilla al imprimir), la rejilla, punteada.
;
;  En los dos casos la hoja se reparte en PÁGINAS a lo alto y a lo ancho:
;  las columnas que caben en el ancho del papel forman una «tira» y las
;  filas que caben en el alto, un «tramo»; cada página es un tramo de una
;  tira, y salen primero todas las de la primera tira, hacia abajo, y luego
;  las de la siguiente, hacia la derecha. Cada página lleva una cabecera con
;  el nombre del archivo, la fecha y «Página N de M».
;
;  La VISTA PRELIMINAR enseña una página a la mitad de tamaño, hecha con
;  las mismas rutinas que «como se ve»: lo que se ve es lo que saldrá.
;
;  Cómo se hace una página «como se ve»: no se guarda entera (serían 440 K).
;  Se va pintando por TROZOS en ib_buf (la cabecera, cada fila de celdas y
;  la raya que cierra la última), y cada trozo se da a quien lo quiere
;  ([ip_saca]): la impresora (ig_come, que junta las filas de puntos de ocho
;  en ocho y las manda) o la vista preliminar (vp_come, que las encoge a la
;  mitad). En ib_buf cada punto es un color de la paleta; los que llevan
;  IB_TRAMA (la rejilla, los fondos) salen en el papel TRAMADOS, más o menos
;  espesos según lo oscuro que sea el color; los que llevan IB_RAYADO (las
;  barras y los sectores de los gráficos), con un rayado distinto para cada
;  color; y los demás (las letras, los bordes), lisos.
;
;  Las rutinas ib_* pintan donde diga [ib_base]: en ib_buf, o en el LIENZO,
;  un mapa más grande (576 x 400) donde se dibuja el gráfico (grafico.inc)
;  y, a la mitad de tamaño, la página de la vista preliminar.
; =============================================================================
IMP_AN      equ 80                  ; letras de una línea de la impresora
IMP_FIL     equ 58                  ; filas de la hoja en una página de texto
IB_AN       equ 576                 ; puntos de ancho del papel, «como se ve»
IB_AL       equ 128                 ; filas de puntos de un trozo, como mucho
IG_PAG      equ 760                 ; puntos de alto que se usan de cada página
IG_CAB      equ 26                  ; lo que ocupa la cabecera
IB_TRAMA    equ 0x80                ; (con el color) este punto sale tramado
IB_RAYADO   equ 0x40                ; (con el color) y este, con el rayado de su color
LZ_AL       equ 400                 ; filas del lienzo (un gráfico, o media página)
VP_M        equ 12                  ; vista preliminar: el margen del papel, a los lados
VP_MV       equ 4                   ; y arriba y abajo (para que quepa entera en la ventana)
VP_Y        equ TB_AL + 3           ; donde empieza el papel
VP_AN       equ IB_AN / 2
VP_AL       equ IG_PAG / 2

; =============================================================================
;  El reparto en páginas
; =============================================================================
; prepara la impresión: EAX=0 como texto, 1 como se ve. Deja el rango en
; [ip_c1]..., las tiras de columnas en ip_bcol, los tramos de filas en
; ip_tfil y cuántas páginas salen en [ip_n]. CF=1 si la hoja está vacía
ip_prepara:
        pushad
        mov [ip_modo], eax
        call sel_rect
        call sel_una
        jne .rango
        xor eax, eax                ; una celda sola: toda la hoja
        mov [r_c1], eax
        mov [r_f1], eax
        mov dword [r_c2], NCOL - 1
        mov dword [r_f2], NFIL - 1
.rango:
        call rect_recorta
        cmp dword [lim_f], 0
        jne .hay
        popad
        stc
        ret
.hay:
        mov esi, r_c1
        mov edi, ip_c1
        mov ecx, 4
        rep movsd
        mov dword [ip_lim_an], IMP_AN + 1   ; (el espacio de detrás de la última no cuenta)
        mov dword [ip_lim_al], IMP_FIL
        cmp dword [ip_modo], 0
        je .tiras
        mov dword [ip_lim_an], IB_AN - 1    ; (y la raya que cierra por la derecha)
        mov dword [ip_lim_al], IG_PAG - IG_CAB - 1
.tiras:
        xor ebx, ebx                ; EBX=cuántas tiras van
        mov ecx, [ip_c1]
.tira:
        mov [ip_bcol+ebx], cl
        inc ebx
        xor esi, esi                ; ESI=lo que lleva de ancho la tira
.tc:
        call ip_ancho
        add esi, eax
        cmp esi, [ip_lim_an]
        jbe .cabe
        cmp cl, [ip_bcol+ebx-1]     ; (una columna sola cabe siempre)
        jne .tira
.cabe:
        inc ecx
        cmp ecx, [ip_c2]
        jbe .tc
        mov [ip_bcol+ebx], cl
        mov [ip_nb], ebx
        xor ebx, ebx                ; y los tramos de filas
        mov edx, [ip_f1]
.tramo:
        mov [ip_tfil+ebx*2], dx
        inc ebx
        xor esi, esi
.tf:
        mov eax, 1
        cmp dword [ip_modo], 0
        je .tf1
        call fil_alto
.tf1:
        add esi, eax
        cmp esi, [ip_lim_al]
        jbe .fcabe
        cmp dx, [ip_tfil+ebx*2-2]
        jne .tramo
.fcabe:
        inc edx
        cmp edx, [ip_f2]
        jbe .tf
        mov [ip_tfil+ebx*2], dx
        mov [ip_nt], ebx
        imul ebx, [ip_nb]
        mov [ip_n], ebx
        popad
        clc
        ret
; (interna) lo que ocupa la columna ECX: en letras (con el espacio que la
; separa de la siguiente) o en puntos
ip_ancho:
        movzx eax, word [col_an+ecx*2]
        cmp dword [ip_modo], 0
        jne .x
        shr eax, 3
        jnz .a
        inc eax
.a:
        inc eax
.x:
        ret

; la página EAX (desde 0): sus columnas [ipb_c1]..[ipb_c2], sus filas
; [ipb_f1]..[ipb_f2] y lo que miden de ancho sus columnas [ipb_an] (puntos)
ip_pag_pon:
        pushad
        mov [ipb_pag], eax
        xor edx, edx
        div dword [ip_nt]           ; EAX=la tira EDX=el tramo
        movzx ecx, byte [ip_bcol+eax]
        mov [ipb_c1], ecx
        movzx ebx, byte [ip_bcol+eax+1]
        dec ebx
        mov [ipb_c2], ebx
        xor esi, esi
.an:
        movzx eax, word [col_an+ecx*2]
        add esi, eax
        inc ecx
        cmp ecx, ebx
        jbe .an
        mov [ipb_an], esi
        movzx ecx, word [ip_tfil+edx*2]
        mov [ipb_f1], ecx
        movzx ecx, word [ip_tfil+edx*2+2]
        dec ecx
        mov [ipb_f2], ecx
        popad
        ret

; los tres textos de la cabecera de la página [ipb_pag]: el nombre del
; archivo (sin la carpeta) en ip_t_nom, la fecha en ip_t_fecha y «Página N
; de M» en ip_t_pag; y lo que miden, en letras: [ip_l_nom], [ip_l_pag]
ip_cab_txt:
        pushad
        mov esi, nombre             ; desde la última «\» o «:»
        mov ebx, esi
.n:
        lodsb
        test al, al
        jz .nf
        cmp al, '\'
        je .n1
        cmp al, ':'
        jne .n
.n1:
        mov ebx, esi
        jmp .n
.nf:
        mov esi, ebx
        cmp byte [esi], 0
        jne .n2
        mov esi, t_sin_nombre
.n2:
        mov edi, ip_t_nom
        mov ecx, 30
.n3:
        lodsb
        test al, al
        jz .n4
        stosb
        dec ecx
        jnz .n3
.n4:
        mov byte [edi], 0
        sub edi, ip_t_nom
        mov [ip_l_nom], edi
        call hoy_serie
        mov edi, ip_t_fecha
        call fecha_txt
        mov byte [edi], 0
        mov edi, ip_t_pag
        mov esi, t_ip_pagina
        call hvl_cad
        mov eax, [ipb_pag]
        inc eax
        call hvl_num
        mov esi, t_ip_de
        call hvl_cad
        mov eax, [ip_n]
        call hvl_num
        mov byte [edi], 0
        sub edi, ip_t_pag
        mov [ip_l_pag], edi
        popad
        ret
t_ip_pagina db "Página ", 0
t_ip_de     db " de ", 0

; ¿hay impresora, y está encendida? CF=1 si no
imp_hay:
        push eax
        push ebx
        mov eax, 0                  ; ¿hay puerto paralelo? (INT 11h)
        mov bl, 0x11
        call bios
        test ah, 0xC0
        jz .no
        mov eax, 0x0200             ; ¿y la impresora está encendida?
        mov bl, 0x17
        call bios
        test ah, 0x28
        jnz .no
        test ah, 0x10
        jz .no
        pop ebx
        pop eax
        clc
        ret
.no:
        pop ebx
        pop eax
        stc
        ret
; manda a la impresora ECX octetos de ESI; CF=1 si falla
imp_manda:
        lodsb
        call imp_car
        jc .x
        dec ecx
        jnz imp_manda
.x:
        ret
; lo que se dice al acabar: que ha salido, que no se ha podido, o que no
; había nada
imp_dicho:
        mov esi, t_impreso
        jmp aviso
imp_mal:
        mov esi, t_e_imprimir
        jmp aviso
imp_vacia:
        mov esi, t_imp_vacia
        jmp aviso

; =============================================================================
;  Imprimir como texto
; =============================================================================
imprimir:
        pushad
        xor eax, eax
        call ip_prepara
        jc .vacia
        call imp_hay
        jc .mal
        mov esi, it_ini             ; la impresora, como recién encendida
        mov ecx, 2
        call imp_manda
        jc .mal
        xor eax, eax
.pag:
        call ip_pag_pon
        call it_cabecera
        jc .mal
        mov edx, [ipb_f1]
.fila:
        call it_fila
        jc .mal
        inc edx
        cmp edx, [ipb_f2]
        jbe .fila
        mov al, 12                  ; y la hoja, fuera
        call imp_car
        jc .mal
        mov eax, [ipb_pag]
        inc eax
        cmp eax, [ip_n]
        jb .pag
        call imp_lista
        jc .mal
        popad
        jmp imp_dicho
.mal:
        popad
        jmp imp_mal
.vacia:
        popad
        jmp imp_vacia
it_ini      db 27, '@'

; (interna) la línea, en blanco y sin formato
it_limpia:
        push eax
        push ecx
        push edi
        mov edi, imp_lin
        mov ecx, IMP_AN
        mov al, ' '
        rep stosb
        mov edi, imp_at
        mov ecx, IMP_AN
        xor al, al
        rep stosb
        pop edi
        pop ecx
        pop eax
        ret

; (interna) la cabecera de la página: el nombre a la izquierda, la fecha en
; el centro y «Página N de M» a la derecha; y una línea en blanco. CF=1 si
; la impresora falla
it_cabecera:
        pushad
        call ip_cab_txt
        call it_limpia
        mov esi, ip_t_nom
        mov edi, imp_lin
        call hvl_cad
        mov esi, ip_t_fecha
        mov edi, imp_lin + (IMP_AN - 10) / 2
        call hvl_cad
        mov esi, ip_t_pag
        mov edi, imp_lin + IMP_AN
        sub edi, [ip_l_pag]
        call hvl_cad
        call it_manda
        jc .x
        call it_limpia
        call it_manda
.x:
        popad
        ret

; (interna) la fila EDX de la hoja, de la columna [ipb_c1] a la [ipb_c2]. El
; texto largo sigue sobre las celdas vacías de su derecha; lo que pasa del
; ancho del papel no sale. CF=1 si la impresora falla
it_fila:
        pushad
        call it_limpia
        xor ebx, ebx                ; EBX=la letra de la línea donde empieza la columna
        mov ecx, [ipb_c1]
.col:
        call comb_n                 ; (una combinada: con las que tapa, que
        mov esi, eax                ; no pasan de la tira)
        add eax, ecx
        cmp eax, [ipb_c2]
        jbe .c0
        mov esi, [ipb_c2]
        sub esi, ecx
.c0:
        mov [it_comb], esi
        mov edi, ecx
        mov ebp, -1                 ; EBP=el ancho de la celda, en letras
.an:
        movzx eax, word [col_an+edi*2]
        shr eax, 3
        jnz .an1
        inc eax
.an1:
        lea ebp, [ebp+eax+1]
        inc edi
        dec esi
        jns .an
        call celda_ver
        xor eax, eax
.lg:
        cmp byte [disp+eax], 0
        je .lgf
        inc eax
        jmp .lg
.lgf:
        test eax, eax               ; (vacía: se queda lo que haya llegado de
        jz .sig                     ; la izquierda)
        push ecx
        mov edi, ebx                ; limpia su sitio
        mov ecx, ebp
.lim:
        cmp edi, IMP_AN
        jae .limf
        mov byte [imp_lin+edi], ' '
        mov byte [imp_at+edi], 0
        inc edi
        dec ecx
        jnz .lim
.limf:
        pop ecx
        mov edi, ebx                ; dónde empieza, según la alineación
        mov esi, ebp
        sub esi, eax
        jle .pon
        cmp dword [disp_alin], 1
        je .pon
        cmp dword [disp_alin], 2
        jne .der
        shr esi, 1
.der:
        add edi, esi
.pon:
        mov esi, disp
        mov ah, [disp_fmt]          ; (F_NEG y F_CUR son los dos bits de abajo)
        and ah, F_NEG | F_CUR
.p:
        lodsb
        test al, al
        jz .sig
        cmp edi, IMP_AN
        jae .sig
        mov [imp_lin+edi], al
        mov [imp_at+edi], ah
        inc edi
        jmp .p
.sig:
        lea ebx, [ebx+ebp+1]
        inc ecx
        add ecx, [it_comb]
        cmp ecx, [ipb_c2]
        jbe .col
        call it_manda
        popad
        ret

; (interna) manda la línea imp_lin (sin los espacios del final), con las
; negritas y las cursivas que dice imp_at, y el salto de línea. CF=1 si falla
it_manda:
        pushad
        mov ecx, IMP_AN
.fin:
        cmp byte [imp_lin+ecx-1], ' '
        jne .m0
        dec ecx
        jnz .fin
.m0:
        xor esi, esi
        xor bl, bl                  ; BL=el formato que tiene puesto la impresora
        jecxz .nl
.m:
        mov al, [imp_lin+esi]
        mov ah, [imp_at+esi]
        call it_formato
        jc .mal
        call imp_car
        jc .mal
        inc esi
        dec ecx
        jnz .m
.nl:
        xor ah, ah                  ; la línea acaba sin formato
        call it_formato
        jc .mal
        mov al, 13
        call imp_car
        jc .mal
        mov al, 10
        call imp_car
        jc .mal
        popad
        clc
        ret
.mal:
        popad
        stc
        ret
; (interna) pone en la impresora el formato AH (bit 0 negrita, bit 1
; cursiva): solo lo que cambia respecto al que tiene, BL. Conserva AL
it_formato:
        push eax
        mov bh, ah
        xor bh, bl                  ; BH=lo que cambia
        mov bl, ah
        test bh, F_NEG
        jz .k
        mov al, 27
        call imp_car
        jc .x
        mov al, 'E'
        test bl, F_NEG
        jnz .n1
        mov al, 'F'
.n1:
        call imp_car
        jc .x
.k:
        test bh, F_CUR
        jz .ok
        mov al, 27
        call imp_car
        jc .x
        mov al, '4'
        test bl, F_CUR
        jnz .k1
        mov al, '5'
.k1:
        call imp_car
        jc .x
.ok:
        clc
.x:
        pop eax
        ret

; =============================================================================
;  Pintar en el trozo (ib_buf): rectángulos y letras
; =============================================================================
; el trozo, en blanco: [ib_al] filas
ib_limpia:
        pushad
        mov edi, [ib_base]
        mov ecx, [ib_al]
        imul ecx, IB_AN
        mov al, C_BLANCO
        rep stosb
        popad
        ret
; un rectángulo: EAX=x EBX=y ECX=ancho EDX=alto, del color [ib_c]. Lo que
; se sale del trozo (o pasa de [ib_cx1]) no se pinta
ib_rect:
        pushad
        test eax, eax
        jns .a
        add ecx, eax
        xor eax, eax
.a:
        lea esi, [eax+ecx]
        cmp esi, [ib_cx1]
        jle .b
        mov ecx, [ib_cx1]
        sub ecx, eax
.b:
        test ecx, ecx
        jle .x
        test ebx, ebx
        jns .c
        add edx, ebx
        xor ebx, ebx
.c:
        lea esi, [ebx+edx]
        cmp esi, [ib_al]
        jle .d
        mov edx, [ib_al]
        sub edx, ebx
.d:
        test edx, edx
        jle .x
        imul ebx, IB_AN
        add ebx, [ib_base]
        lea edi, [ebx+eax]
        mov al, [ib_c]
.f:
        push ecx
        push edi
        rep stosb
        pop edi
        pop ecx
        add edi, IB_AN
        dec edx
        jnz .f
.x:
        popad
        ret
; un texto: ESI=texto ECX=letras EAX=x EBX=y, del color [tc_fg]; con
; [tc_neg], en negrita (cada punto, repetido a su derecha), y con [ib_cur],
; en cursiva (las filas de arriba, corridas a la derecha: como texto_cursiva)
ib_texto:
        pushad
        mov [ib_tx], eax
        mov [ib_ty], ebx
        xor ebp, ebp                ; EBP=la letra
.l:
        cmp ebp, [esp+24]
        jae .x
        mov esi, [esp+4]
        movzx eax, byte [esi+ebp]
        shl eax, 4
        add eax, [hv_api+HV_fuente8x16*4]
        xor edx, edx                ; EDX=la fila de la letra
.r:
        mov ebx, [ib_ty]
        add ebx, edx
        cmp ebx, [ib_al]            ; (fuera del trozo, por arriba o por abajo)
        jae .rs
        imul ebx, IB_AN
        mov edi, [ib_tx]
        lea edi, [edi+ebp*8]        ; EDI=la x del primer punto de la fila
        cmp byte [ib_cur], 0
        je .rc
        mov ecx, 15
        sub ecx, edx
        shr ecx, 2
        lea edi, [edi+ecx-1]
.rc:
        mov ch, 8
        mov cl, [eax+edx]
.b:
        shl cl, 1
        jnc .nb
        call .pon
        cmp byte [tc_neg], 0
        je .nb
        inc edi
        call .pon
        dec edi
.nb:
        inc edi
        dec ch
        jnz .b
.rs:
        inc edx
        cmp edx, 16
        jb .r
        inc ebp
        jmp .l
.x:
        popad
        ret
.pon:
        test edi, edi
        js .px
        cmp edi, [ib_cx1]
        jge .px
        push eax
        push esi
        mov esi, [ib_base]
        add esi, ebx
        mov al, [tc_fg]
        mov [esi+edi], al
        pop esi
        pop eax
.px:
        ret
; el contorno de un rectángulo: EAX=x EBX=y ECX=ancho EDX=alto, de [ib_c]
ib_marco:
        pushad
        push edx
        mov edx, 1
        call ib_rect
        pop edx
        push ebx
        lea ebx, [ebx+edx-1]
        push edx
        mov edx, 1
        call ib_rect
        pop edx
        pop ebx
        mov esi, ecx
        mov ecx, 1
        call ib_rect
        lea eax, [eax+esi-1]
        call ib_rect
        popad
        ret
; un punto: EAX=x EBX=y, de [ib_c] (si cae dentro). Conserva los registros
ib_punto:
        cmp eax, [ib_cx1]
        jae .x
        cmp ebx, [ib_al]
        jae .x
        push edi
        push edx
        imul edi, ebx, IB_AN
        add edi, [ib_base]
        mov dl, [ib_c]
        mov [edi+eax], dl
        pop edx
        pop edi
.x:
        ret
; una raya punteada (un punto sí y otro no): horizontal, desde EAX=x EBX=y,
; de ECX puntos; o vertical, de EDX
ib_puntos_h:
        pushad
.l:
        test ecx, ecx
        jle .x
        test al, 1
        jnz .s
        call ib_punto
.s:
        inc eax
        dec ecx
        jmp .l
.x:
        popad
        ret
ib_puntos_v:
        pushad
.l:
        test edx, edx
        jle .x
        test bl, 1
        jnz .s
        call ib_punto
.s:
        inc ebx
        dec edx
        jmp .l
.x:
        popad
        ret
; una línea de (EAX, EBX) a (ECX, EDX), de [ib_c]
ib_linea:
        pushad
        mov esi, ecx
        sub esi, eax                ; ESI=lo que avanza a lo ancho (sin signo)
        mov ebp, 1
        jns .a
        neg esi
        neg ebp
.a:
        mov [il_sx], ebp
        mov edi, edx
        sub edi, ebx                ; EDI=y a lo alto
        mov ebp, 1
        jns .b
        neg edi
        neg ebp
.b:
        mov [il_sy], ebp
        mov ebp, esi
        sub ebp, edi                ; EBP=el error que se lleva
.l:
        call ib_punto
        cmp eax, ecx
        jne .m
        cmp ebx, edx
        je .x
.m:
        push edx
        lea edx, [ebp+ebp]
        mov [il_e2], edx
        mov edx, edi
        neg edx
        cmp [il_e2], edx
        pop edx
        jle .n
        sub ebp, edi
        add eax, [il_sx]
.n:
        cmp [il_e2], esi
        jge .l
        add ebp, esi
        add ebx, [il_sy]
        jmp .l
.x:
        popad
        ret
; un texto de abajo arriba (girado un cuarto de vuelta): ESI=texto
; ECX=letras, desde EAX=x EBX=y, del color [ib_c]
ib_texto_v:
        pushad
        xor ebp, ebp                ; EBP=la letra
.l:
        cmp ebp, [esp+24]
        jae .x
        mov esi, [esp+4]
        movzx edi, byte [esi+ebp]
        shl edi, 4
        add edi, [hv_api+HV_fuente8x16*4]
        xor edx, edx                ; EDX=la fila de la letra: va a lo ancho
.r:
        mov cl, [edi+edx]
        xor ch, ch                  ; CH=la columna: va hacia arriba
.b:
        shl cl, 1
        jnc .nb
        mov eax, [esp+28]
        add eax, edx
        mov ebx, [esp+16]
        movzx esi, ch
        sub ebx, esi
        lea esi, [ebp*8]
        sub ebx, esi
        call ib_punto
.nb:
        inc ch
        cmp ch, 8
        jb .b
        inc edx
        cmp edx, 16
        jb .r
        inc ebp
        jmp .l
.x:
        popad
        ret

; =============================================================================
;  Una página «como se ve», trozo a trozo
; =============================================================================
; hace la página EAX y da cada trozo a [ip_saca] (que devuelve CF=1 si no
; puede seguir). CF=1 si se ha parado
ipg_pagina:
        pushad
        call ip_pag_pon
        mov dword [ib_base], ib_buf
        mov dword [ib_py], 0
        call ipg_cabecera
        call .saca
        jc .mal
        mov edx, [ipb_f1]
.fila:
        mov byte [ipg_cierra], 0
        call ipg_fila
        call .saca
        jc .mal
        inc edx
        cmp edx, [ipb_f2]
        jbe .fila
        mov byte [ipg_cierra], 1    ; y la raya de debajo de la última fila
        call ipg_fila
        call .saca
        jc .mal
        popad
        clc
        ret
.mal:
        popad
        stc
        ret
.saca:
        call [ip_saca]
        jc .sx
        mov eax, [ib_al]
        add [ib_py], eax            ; (CF=0: no pasa de 760)
.sx:
        ret

; (interna) el trozo de la cabecera: los tres textos y una raya debajo
ipg_cabecera:
        pushad
        call ip_cab_txt
        mov dword [ib_al], IG_CAB
        mov dword [ib_cx1], IB_AN
        call ib_limpia
        mov byte [tc_fg], C_TINTA
        mov byte [tc_neg], 0
        mov byte [ib_cur], 0
        xor eax, eax
        mov ebx, 2
        mov esi, ip_t_nom
        mov ecx, [ip_l_nom]
        call ib_texto
        mov eax, (IB_AN - 80) / 2
        mov esi, ip_t_fecha
        mov ecx, 10
        call ib_texto
        mov ecx, [ip_l_pag]
        mov eax, ecx
        shl eax, 3
        neg eax
        add eax, IB_AN
        mov esi, ip_t_pag
        call ib_texto
        mov byte [ib_c], C_TINTA
        xor eax, eax
        mov ebx, 20
        mov ecx, IB_AN
        mov edx, 1
        call ib_rect
        popad
        ret

; (interna) ECX=columna -> EAX=su x en la página (negativa si queda a la
; izquierda de la tira)
ipg_x:
        push ebx
        push edx
        xor eax, eax
        mov ebx, [ipb_c1]
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
        pop ebx
        ret

; (interna) el trozo de la fila EDX, como pinta_fila en la ventana: la
; rejilla (si se pide), lo escrito (empezando por la última celda con algo
; que quede a la izquierda de la tira: si es un texto largo, llega hasta
; aquí) y los bordes. La raya de arriba del trozo es la de debajo de la fila
; de encima, igual que en la pantalla. Con [ipg_cierra], EDX es la fila
; siguiente a la última y el trozo es solo esa raya, que cierra la página
ipg_fila:
        pushad
        mov [ipg_f], edx
        mov eax, 1
        cmp byte [ipg_cierra], 0
        jne .al
        call fil_alto
.al:
        mov [ib_al], eax
        call ib_limpia
        mov eax, [ipb_an]
        inc eax
        mov [ib_cx1], eax
        cmp byte [imp_rej], 0
        je .celdas
        mov byte [ib_c], C_ARENA_OSC | IB_TRAMA
        xor eax, eax
        xor ebx, ebx
        mov ecx, [ipb_an]
        inc ecx
        mov edx, 1
        call ib_rect
        mov ecx, [ipb_c1]           ; las verticales: la de la izquierda de cada
        xor eax, eax                ; columna y la que cierra por la derecha
.rv:
        push ecx
        mov ecx, 1
        mov edx, [ib_al]
        call ib_rect
        pop ecx
        cmp ecx, [ipb_c2]
        ja .celdas
        movzx edx, word [col_an+ecx*2]
        add eax, edx
        inc ecx
        jmp .rv
.celdas:
        cmp byte [ipg_cierra], 0
        jne .bordes
        mov edx, [ipg_f]
        mov esi, edx
        shl esi, 6                  ; ESI=la primera celda de la fila
        mov ecx, [ipb_c1]
.atras:
        test ecx, ecx
        jz .desde
        dec ecx
        lea edi, [esi+ecx]
        cmp dword [cel_off+edi*4], 0
        jne .ya
        test dword [cel_fmt+edi*4], F_COMB
        jz .atras
        jmp .ya
.desde:
        mov ecx, [ipb_c1]
.ya:
        call ipg_x                  ; EAX=la x de la columna ECX
.c:
        cmp ecx, [ipb_c2]
        ja .bordes
        lea edi, [esi+ecx]
        mov ebp, [cel_fmt+edi*4]
        test ebp, F_FONDO | F_COMB
        jnz .p
        cmp dword [cel_off+edi*4], 0
        je .s
.p:
        call ipg_celda
.s:
        shr ebp, F_COMB_D           ; detrás de ella y de las que tapa
        and ebp, 63
.s1:
        movzx edi, word [col_an+ecx*2]
        add eax, edi
        inc ecx
        sub ebp, 1
        jc .c
        cmp ecx, NCOL
        jb .s1
        jmp .c
.bordes:
        mov byte [ib_c], C_TINTA
        mov ecx, [ipb_c1]           ; desde la columna de antes de la tira: su
        mov dword [ipg_xx], 0       ; raya de la derecha es la x 0 de la página
        mov dword [ipg_resto], 0
        test ecx, ecx
        jz .b
        dec ecx
        movzx edi, word [col_an+ecx*2]
        sub [ipg_xx], edi
.b:
        cmp ecx, [ipb_c2]
        ja .x
        movzx edi, word [col_an+ecx*2]      ; EDI=el ancho de la columna
        cmp byte [ipg_cierra], 0
        jne .b1
        mov edx, [ipg_f]
        call comb_r                 ; (de una combinada solo cuenta la raya de
        mov [ipg_resto], eax        ; la derecha de su última celda)
        call cel_formato
        cmp dword [ipg_resto], 0
        jne .b1
        test eax, F_BDER            ; su raya de la derecha
        jz .b1
        mov eax, [ipg_xx]
        add eax, edi
        xor ebx, ebx
        push ecx
        mov ecx, 1
        mov edx, [ib_al]
        call ib_rect
        pop ecx
.b1:
        mov edx, [ipg_f]            ; y de la celda de encima: su raya de abajo
        test edx, edx               ; y el último punto de la de su derecha
        jz .b3
        dec edx
        call cel_formato
        mov ebp, eax
        test ebp, F_BABA
        jz .b2
        mov eax, [ipg_xx]
        xor ebx, ebx
        push ecx
        lea ecx, [edi+1]
        mov edx, 1
        call ib_rect
        pop ecx
.b2:
        mov edx, [ipg_f]
        dec edx
        push ebp
        call comb_r                 ; (ECX, EDX: la celda de encima)
        pop ebp
        test eax, eax
        jnz .b3
        test ebp, F_BDER
        jz .b3
        mov eax, [ipg_xx]
        add eax, edi
        xor ebx, ebx
        push ecx
        mov ecx, 1
        mov edx, 1
        call ib_rect
        pop ecx
.b3:
        add [ipg_xx], edi
        inc ecx
        jmp .b
.x:
        popad
        ret

; (interna) la celda ECX=columna EDX=fila, que está en la x EAX del trozo:
; como pinta_celda en la ventana (las mismas cuentas, para que salga igual)
ipg_celda:
        pushad
        mov [pc_x], eax
        call cel_ancho
        mov [pc_an], eax
        call celda_ver
        mov eax, [disp_fmt]
        shr eax, F_COLOR_D
        and eax, 7
        mov al, [colores+eax]
        mov [tc_fg], al
        ; el fondo: tramado, más o menos espeso según su color; el que es
        ; muy oscuro sale liso, y la letra sobre él, en blanco (se lee mejor)
        mov byte [ipg_fondo], 0
        test dword [disp_fmt], F_FONDO | F_COMB
        jz .txt
        mov ebx, [disp_fmt]
        shr ebx, F_FONDO_D
        and ebx, 15
        mov al, C_BLANCO            ; (sin color, pero tapa a otras: en blanco,
        jz .f1                      ; sin las rayas de la rejilla)
        mov al, bl
        cmp byte [ig_nivel+ebx], 10
        jb .f0
        mov byte [tc_fg], C_BLANCO | IB_RAYADO      ; (un blanco que tapa: ver vp_come)
        jmp .f1
.f0:
        or al, IB_TRAMA
        mov byte [ipg_fondo], 1
.f1:
        mov [ib_c], al
        push ecx
        push edx
        mov eax, [pc_x]
        inc eax
        mov ebx, 1
        mov ecx, [pc_an]
        dec ecx
        mov edx, [ib_al]
        dec edx
        call ib_rect
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
        cmp byte [disp_tipo], 1     ; un número no se desborda
        je .corta
        cmp dword [disp_alin], 1
        jne .corta
        test dword [disp_fmt], F_COMB
        jnz .corta
        mov esi, edx                ; un texto largo: sobre las celdas vacías
        shl esi, 6                  ; de su derecha (aunque sean de otra tira:
        mov eax, [pc_x]             ; lo que no cabe en el papel se recorta)
        add eax, [pc_an]            ; EAX=hasta dónde llega
.des:
        lea ebx, [edi*8+6]
        add ebx, [pc_x]
        cmp eax, ebx
        jge .desf
        inc ecx
        cmp ecx, NCOL
        jae .desf
        cmp eax, [ipb_an]
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
        mov ecx, eax                ; tapa las rayas de la rejilla que cruza
        dec ecx
        mov ebp, eax
        sub ebp, 6
        shr ebp, 3
        mov byte [ib_c], C_BLANCO
        mov eax, [pc_x]
        inc eax
        mov ebx, 1
        mov edx, [ib_al]
        dec edx
        call ib_rect
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
        mov ebx, [ib_al]            ; abajo de la celda, como en la ventana
        sub ebx, 17
        cmp byte [ipg_fondo], 0     ; sobre un fondo tramado, lo escrito va en
        je .p0                      ; un hueco en blanco: si no, en el papel no
                                    ; se leería
        push eax
        push ebx
        mov byte [ib_c], C_BLANCO | IB_RAYADO
        dec eax
        lea ecx, [edi*8+2]
        mov edx, 16
        call ib_rect
        pop ebx
        pop eax
.p0:
        mov ecx, edi
        mov esi, disp
        mov edx, [disp_fmt]
        test edx, F_NEG
        setnz [tc_neg]
        test edx, F_CUR
        setnz [ib_cur]
        call ib_texto
.x:
        popad
        ret

; =============================================================================
;  Imprimir como se ve
; =============================================================================
imprimir_g:
        pushad
        mov eax, 1
        call ip_prepara
        jc .vacia
        call imp_hay
        jc .mal
        mov esi, ig_ini             ; ESC @ y el avance de línea, a 8/72
        mov ecx, 5
        call imp_manda
        jc .mal
        mov dword [ip_saca], ig_come
        xor eax, eax
.pag:
        call ig_empieza
        call ipg_pagina
        jc .mal
        call ig_acaba
        jc .mal
        mov eax, [ipb_pag]
        inc eax
        cmp eax, [ip_n]
        jb .pag
        call imp_lista
        jc .mal
        popad
        jmp imp_dicho
.mal:
        popad
        jmp imp_mal
.vacia:
        popad
        jmp imp_vacia
ig_ini      db 27, '@', 27, 'A', 8
ig_graf     db 27, '*', 5, IB_AN & 0xFF, IB_AN >> 8

; una página nueva: la tira de ocho filas de puntos, en blanco
ig_empieza:
        mov dword [ig_fila], 0
        mov dword [ig_y], 0
; (interna) la tira, en blanco
ig_limpia:
        pushad
        mov edi, ig_banda
        mov ecx, 8 * IB_AN
        mov al, C_BLANCO
        rep stosb
        popad
        ret
; el final de la página: lo que quede en la tira, y la hoja fuera. CF=1 si
; la impresora falla
ig_acaba:
        call ig_tira
        jc .x
        mov al, 12
        call imp_car
.x:
        ret
; recibe un trozo (ib_buf, [ib_al] filas): sus filas de puntos, a la tira; y
; cada ocho, a la impresora. CF=1 si falla
ig_come:
        pushad
        mov esi, [ib_base]
        mov edx, [ib_al]
.f:
        mov edi, [ig_fila]
        imul edi, IB_AN
        add edi, ig_banda
        mov ecx, IB_AN
        rep movsb
        inc dword [ig_fila]
        cmp dword [ig_fila], 8
        jb .f1
        call ig_tira
        jc .mal
.f1:
        dec edx
        jnz .f
        popad
        clc
        ret
.mal:
        popad
        stc
        ret
; manda la tira (si tiene algún punto) y avanza el papel; la deja en blanco.
; Cada columna de la tira es un octeto: el punto de arriba, el bit 7. Un
; punto sale si no es blanco; y los que llevan IB_TRAMA, según una trama de
; 4 x 4: de los dieciséis puntos de cada cuadro salen los que diga lo oscuro
; que es su color (ig_nivel). CF=1 si la impresora falla
ig_tira:
        pushad
        cmp dword [ig_fila], 0
        je .bien
        xor ebx, ebx                ; EBX=la columna
        xor ebp, ebp                ; EBP: distinto de 0 si hay algún punto
.col:
        xor al, al
        mov ah, 0x80
        lea esi, [ig_banda+ebx]
        mov edi, [ig_y]             ; EDI=la fila de la página
.pt:
        movzx edx, byte [esi]
        test dl, IB_TRAMA
        jnz .trama
        test dl, IB_RAYADO
        jnz .rayado
        cmp dl, C_BLANCO
        je .p1
.si:
        or al, ah
        jmp .p1
.rayado:
        and edx, 15                 ; el rayado de su color: ocho filas de ocho
        mov ecx, edi                ; puntos, que se repiten
        and ecx, 7
        mov dl, [ig_rayados+edx*8+ecx]
        mov ecx, ebx
        and ecx, 7
        shl dl, cl
        test dl, 0x80
        jnz .si
        jmp .p1
.trama:
        and edx, 15
        mov dl, [ig_nivel+edx]      ; cuántos de los dieciséis
        mov ecx, edi
        and ecx, 3
        shl ecx, 2
        push ebx
        and ebx, 3
        add ecx, ebx
        pop ebx
        cmp dl, [ig_trama+ecx]
        ja .si
.p1:
        add esi, IB_AN
        inc edi
        shr ah, 1
        jnz .pt
        mov [ig_cols+ebx], al
        or ebp, eax
        inc ebx
        cmp ebx, IB_AN
        jb .col
        test ebp, 0xFF
        jz .avanza
        mov esi, ig_graf            ; ESC * 5 y cuántas columnas
        mov ecx, 5
        call imp_manda
        jc .mal
        mov esi, ig_cols
        mov ecx, IB_AN
        call imp_manda
        jc .mal
.avanza:
        mov al, 13
        call imp_car
        jc .mal
        mov al, 10
        call imp_car
        jc .mal
        mov dword [ig_fila], 0
        add dword [ig_y], 8
        call ig_limpia
.bien:
        popad
        clc
        ret
.mal:
        popad
        stc
        ret
; la trama: en qué orden se van llenando los dieciséis puntos de un cuadro
ig_trama    db 0, 8, 2, 10
            db 12, 4, 14, 6
            db 3, 11, 1, 9
            db 15, 7, 13, 5
; y cuántos se llenan con cada color de la paleta (tinta, marrón, petróleo,
; petróleo claro, terracota, terracota clara, arena, arena oscura, crema,
; blanco, ocre, oliva, añil, granate, turquesa, piedra). Con 8, una raya de
; un punto de gruesa sale punteada: así va la rejilla (en arena oscura)
ig_nivel    db 16, 14, 12, 10, 8, 5, 4, 8, 1, 0, 3, 6, 12, 11, 4, 2
; los rayados de los rellenos de los gráficos, uno por color de la paleta:
; los de las ocho series (petróleo, terracota, ocre, oliva, añil, granate,
; turquesa y marrón) son todos distintos; los demás colores salen lisos
ig_rayados  db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF   ;  0 tinta
            db 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55   ;  1 marrón: ajedrez
            db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF   ;  2 petróleo: liso
            db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
            db 0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88   ;  4 terracota: rayas inclinadas
            db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
            db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
            db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
            db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
            db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   ;  9 blanco
            db 0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00   ; 10 ocre: puntos
            db 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00   ; 11 oliva: rayas tumbadas
            db 0xFF, 0x88, 0x88, 0x88, 0xFF, 0x88, 0x88, 0x88   ; 12 añil: cuadrícula
            db 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88   ; 13 granate: rayas de pie
            db 0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11   ; 14 turquesa: inclinadas al revés
            db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF

; =============================================================================
;  La vista preliminar
; =============================================================================
; la orden: se pone o se quita
prelim:
        cmp byte [vp_on], 0
        jne vp_sal
        mov eax, 1
        call ip_prepara
        jc imp_vacia
        mov byte [vp_on], 1
        mov dword [vp_pag], 0
        xor eax, eax                ; (las barras de desplazamiento, escondidas)
        jmp vp_barras
; vuelve a la hoja
vp_sal:
        mov byte [vp_on], 0
        mov eax, 1
; (interna) enseña (EAX=1) o esconde (0) las barras de desplazamiento
vp_barras:
        pushad
        mov ebp, [barra_v]
        test ebp, ebp
        jz .x
        push eax
        HV ven_mostrar
        pop eax
        mov ebp, [barra_h]
        HV ven_mostrar
.x:
        popad
        jmp inval
; pasa EAX páginas (con signo), sin salirse
vp_pasa:
        add eax, [vp_pag]
        js .x
        cmp eax, [ip_n]
        jae .x
        mov [vp_pag], eax
        call inval
.x:
        ret

; recibe un trozo: sus puntos, a la mitad de tamaño, al mapa de la página
; (el lienzo). De cada cuatro puntos queda uno: el de una letra o un borde
; antes que el de un fondo tramado, y cualquiera antes que el blanco del
; papel (el blanco de una letra sobre un fondo oscuro lleva IB_RAYADO, que
; en el papel no pone punto, y aquí cuenta como una letra)
vp_come:
        pushad
        mov esi, ib_buf
        mov edx, [ib_py]            ; EDX=la fila de la página
        mov ebp, [ib_al]
.f:
        mov edi, edx
        shr edi, 1
        cmp edi, VP_AL
        jae .x
        imul edi, VP_AN
        add edi, lienzo
        xor ecx, ecx
.p:
        mov al, [esi+ecx]
        cmp al, C_BLANCO
        je .s
        mov ebx, ecx
        shr ebx, 1
        test al, IB_TRAMA
        jz .pon
        cmp byte [edi+ebx], C_BLANCO
        jne .s
.pon:
        mov [edi+ebx], al
.s:
        inc ecx
        cmp ecx, IB_AN
        jb .p
        add esi, IB_AN
        inc edx
        dec ebp
        jnz .f
.x:
        popad
        clc
        ret

; pinta la ventana con la vista preliminar (desde «pintar», ya con las
; medidas): la barra de botones, el pie y, en lo demás, el papel
pinta_prelim:
        mov eax, 1                  ; (el reparto en páginas, al día)
        call ip_prepara
        jnc .hay
        call vp_sal                 ; la hoja se ha quedado vacía: fuera
        HV dc_cerrar
        jmp hecho
.hay:
        mov eax, [ip_n]
        dec eax
        cmp eax, [vp_pag]
        jae .p0
        mov [vp_pag], eax
.p0:
        call pinta_botones
        call pinta_pie
        xor eax, eax
        mov ebx, TB_AL
        mov ecx, [cli_an]
        mov edx, [cli_al]
        sub edx, TB_AL + BR_PIE
        HV dc_cortar
        COLOR C_ARENA_OSC
        HV dc_rect
        mov eax, [cli_an]           ; el papel, centrado, con su sombra
        sub eax, VP_AN + 2 * VP_M
        sar eax, 1
        mov [vp_x], eax
        COLOR C_TINTA
        add eax, 3
        mov ebx, VP_Y + 3
        mov ecx, VP_AN + 2 * VP_M
        mov edx, VP_AL + 2 * VP_MV
        HV dc_rect
        COLOR C_BLANCO
        sub eax, 3
        sub ebx, 3
        HV dc_rect
        mov edi, lienzo
        mov ecx, VP_AN * VP_AL
        mov al, C_BLANCO
        rep stosb
        mov dword [ip_saca], vp_come
        mov eax, [vp_pag]
        call ipg_pagina
        mov esi, lienzo             ; (las marcas de la trama, fuera)
        mov ecx, VP_AN * VP_AL
.m:
        and byte [esi], 15
        inc esi
        dec ecx
        jnz .m
        mov eax, [vp_x]
        add eax, VP_M
        mov ebx, VP_Y + VP_MV
        mov ecx, VP_AN
        mov edx, VP_AL
        mov esi, lienzo
        HV dc_copiar
        HV dc_cerrar
        jmp hecho

; el pie de la vista preliminar, en EDI: «Vista preliminar: página 1 de 4...»
vp_pie:
        mov esi, t_vp1
        call hvl_cad
        mov eax, [vp_pag]
        inc eax
        call hvl_num
        mov esi, t_ip_de
        call hvl_cad
        mov eax, [ip_n]
        call hvl_num
        mov esi, t_vp2
        jmp hvl_cad
t_vp1       db "Vista preliminar: página ", 0
t_vp2       db "    Av Pág y Re Pág: otra página    Esc: volver", 0

; la rejilla al imprimir: se pone o se quita (se guarda con la hoja)
rejilla:
        xor byte [imp_rej], 1
        mov byte [modif], 1
        call rej_pon
        jmp inval
; el menú dice si la rejilla sale o no
rej_pon:
        push eax
        mov ax, [t_rej_no]
        cmp byte [imp_rej], 0
        je .p
        mov ax, [t_rej_si]
.p:
        mov [m_rej], ax
        pop eax
        ret
t_rej_no    db "no"
t_rej_si    db "sí"

        section .bss
            alignb 4
ip_modo     resd 1                  ; 0 como texto, 1 como se ve
ip_c1       resd 1                  ; lo que se imprime (en el orden de r_c1...)
ip_f1       resd 1
ip_c2       resd 1
ip_f2       resd 1
ip_lim_an   resd 1                  ; lo que cabe en una página: de ancho
ip_lim_al   resd 1                  ; y de alto
ip_nb       resd 1                  ; cuántas tiras de columnas,
ip_nt       resd 1                  ; cuántos tramos de filas
ip_n        resd 1                  ; y cuántas páginas
ipb_pag     resd 1                  ; la página que se hace
ipb_c1      resd 1                  ; sus columnas
ipb_c2      resd 1
ipb_f1      resd 1                  ; sus filas
ipb_f2      resd 1
ipb_an      resd 1                  ; el ancho de sus columnas, en puntos
ip_l_nom    resd 1                  ; la cabecera: las letras del nombre
ip_l_pag    resd 1                  ; y las de «Página N de M»
ip_saca     resd 1                  ; a quién se da cada trozo
ib_al       resd 1                  ; el trozo: sus filas de puntos,
ib_py       resd 1                  ; la fila de la página donde empieza
ib_cx1      resd 1                  ; y hasta qué x se pinta
ib_base     resd 1                  ; dónde pintan las rutinas ib_*
il_sx       resd 1                  ; ib_linea
il_sy       resd 1
il_e2       resd 1
ib_tx       resd 1
ib_ty       resd 1
ipg_f       resd 1                  ; ipg_fila: la fila
ipg_resto   resd 1                  ; y lo que queda de una combinada
it_comb     resd 1                  ; it_fila: las celdas que tapa la que se escribe
ipg_xx      resd 1
ig_fila     resd 1                  ; filas de puntos que hay en la tira
ig_y        resd 1                  ; la fila de la página donde empieza la tira
vp_pag      resd 1                  ; vista preliminar: la página que se ve
vp_x        resd 1
vp_on       resb 1                  ; se ve la vista preliminar
imp_rej     resb 1                  ; la rejilla sale al imprimir como se ve
ib_c        resb 1                  ; el color de ib_rect
ib_cur      resb 1                  ; ib_texto: en cursiva
ipg_cierra  resb 1                  ; ipg_fila: solo la raya que cierra
ipg_fondo   resb 1                  ; ipg_celda: tiene un fondo tramado
            alignb 4
ip_t_nom    resb 32
ip_t_fecha  resb 16
ip_t_pag    resb 32
ip_bcol     resb NCOL + 4           ; la primera columna de cada tira (y el final)
ip_tfil     resw NFIL + 2           ; la primera fila de cada tramo (y el final)
imp_lin     resb IMP_AN + 4         ; una línea de texto
imp_at      resb IMP_AN + 4         ; y el formato de cada letra
ig_cols     resb IB_AN              ; la tira, ya en octetos de la impresora
ig_banda    resb 8 * IB_AN
ib_buf      resb IB_AN * IB_AL
lienzo      resb IB_AN * LZ_AL      ; el lienzo: el gráfico, o la página de la vista preliminar
        section .text
