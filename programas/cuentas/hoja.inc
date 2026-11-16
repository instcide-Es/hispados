; =============================================================================
;  Cuentas - la hoja: el almacén de las celdas, deshacer, los rangos (copiar,
;  pegar, rellenar, borrar, ordenar), insertar y eliminar filas y columnas,
;  el ajuste de las referencias de las fórmulas y buscar
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La hoja tiene NCOL x NFIL celdas (64 x 1.000), pero casi todas están
;  vacías, así que el texto de cada una no tiene un sitio fijo: va en un
;  ALMACÉN, una detrás de otra. De cada celda se guarda:
;       cel_off   dónde está su texto en el almacén (0: vacía)
;       cel_fmt   su formato (una palabra: los bits F_* de calculo.inc)
;  y en el almacén, delante de cada texto: dw el número de su celda, db lo
;  que cabe en ese hueco (con el 0 final). Un texto que crece y no cabe en
;  su hueco se escribe al final, y el hueco viejo queda muerto; cuando el
;  almacén se llena se COMPACTA: se recorre de principio a fin y los huecos
;  vivos (los que su celda aún señala) se arriman al principio.
;
;  Deshacer: antes de cada cambio se copia la hoja entera (lo que haya de
;  almacén y las dos tablas) a una copia; deshacer y rehacer la intercambian
;  con la hoja. Es un solo paso, pero vale para todo: teclear, pegar,
;  ordenar, eliminar filas, abrir formato...
;
;  Las referencias. Al pegar o rellenar, las fórmulas se copian «corridas»:
;  A1 pasa a ser la celda que está a la misma distancia del sitio nuevo, y
;  lo que lleva «$» ($A$1, A$1, $A1) no se mueve. Al insertar o eliminar
;  filas o columnas se corrigen todas las fórmulas de la hoja para que sigan
;  señalando a las mismas celdas; lo que señalaba a una celda eliminada
;  queda como «#REF», y un rango se encoge. Lo hace ajusta_formula.
; =============================================================================
%if NCOL != 64
%error "el número de una celda se hace con desplazamientos: NCOL tiene que ser 64"
%endif
ALM_TAM     equ 524288              ; el almacén de textos
CLIP_TAM    equ 262144              ; lo copiado (formato y texto de cada celda)
PP_TAM      equ 60000               ; lo que cabe en el portapapeles de todos
AN_NORMAL   equ 88                  ; ancho de una columna, si no se cambia
AN_MIN      equ 8
AN_MAX      equ 480

; ECX=columna EDX=fila -> ESI=el texto de la celda (acabado en 0; si está
; vacía, un texto vacío). Conserva los demás registros
cel_puntero:
        push eax
        mov eax, edx
        shl eax, 6
        add eax, ecx
        mov esi, [cel_off+eax*4]
        pop eax
        test esi, esi
        jz .v
        add esi, almacen
        ret
.v:
        mov esi, t_nada
        ret

; ECX=columna EDX=fila -> EAX=su formato
cel_formato:
        mov eax, edx
        shl eax, 6
        add eax, ecx
        movzx eax, word [cel_fmt+eax*2]
        ret

; pone el texto ESI (acabado en 0; como mucho CEL_MAX-1 letras) en la celda
; ECX=columna EDX=fila. Si no queda sitio ni compactando, pone [llena] y
; CF=1. Conserva los registros
cel_pon:
        pushad
        mov ebx, edx
        shl ebx, 6
        add ebx, ecx                ; EBX=el número de la celda
        mov edi, cp_buf             ; (el texto puede venir del almacén, que
        xor ecx, ecx                ; se puede compactar: primero, aparte)
.l:
        mov al, [esi+ecx]
        mov [edi+ecx], al
        test al, al
        jz .lf
        inc ecx
        cmp ecx, CEL_MAX - 1
        jb .l
        mov byte [edi+ecx], 0
.lf:
        test ecx, ecx
        jnz .hay
        mov dword [cel_off+ebx*4], 0
        jmp .ok
.hay:
        mov edi, [cel_off+ebx*4]
        test edi, edi
        jz .nueva
        movzx eax, byte [almacen+edi-1]
        cmp ecx, eax                ; ¿cabe donde estaba?
        jb .copia
.nueva:
        mov edi, [alm_fin]
        lea eax, [edi+ecx+4]
        cmp eax, ALM_TAM
        jbe .sitio
        call alm_compacta
        mov edi, [alm_fin]
        lea eax, [edi+ecx+4]
        cmp eax, ALM_TAM
        ja .llena
.sitio:
        mov [alm_fin], eax
        mov [almacen+edi], bx
        lea eax, [ecx+1]
        mov [almacen+edi+2], al
        add edi, 3
        mov [cel_off+ebx*4], edi
        mov eax, ebx                ; hasta dónde hay algo escrito
        shr eax, 6
        inc eax
        cmp eax, [lim_f]
        jbe .l1
        mov [lim_f], eax
.l1:
        mov eax, ebx
        and eax, NCOL - 1
        inc eax
        cmp eax, [lim_c]
        jbe .copia
        mov [lim_c], eax
.copia:
        add edi, almacen
        mov esi, cp_buf
        inc ecx
        rep movsb
.ok:
        popad
        clc
        ret
.llena:
        mov byte [llena], 1
        popad
        stc
        ret

; arrima al principio del almacén los huecos vivos
alm_compacta:
        pushad
        xor esi, esi                ; por dónde se lee
        xor edi, edi                ; y por dónde se escribe
.b:
        cmp esi, [alm_fin]
        jae .f
        movzx ebx, word [almacen+esi]
        movzx ecx, byte [almacen+esi+2]
        add ecx, 3
        lea eax, [esi+3]
        cmp [cel_off+ebx*4], eax    ; ¿su celda sigue señalando aquí?
        jne .muerto
        lea eax, [edi+3]
        mov [cel_off+ebx*4], eax
        push ecx
        push esi
        add esi, almacen
        add edi, almacen
        rep movsb
        sub edi, almacen
        pop esi
        pop ecx
.muerto:
        add esi, ecx
        jmp .b
.f:
        mov [alm_fin], edi
        popad
        ret

; pone en cada hueco vivo el número de su celda (después de mover celdas de
; sitio: ordenar, insertar, eliminar)
alm_indices:
        pushad
        xor ebx, ebx
.b:
        mov eax, [cel_off+ebx*4]
        test eax, eax
        jz .s
        mov [almacen+eax-3], bx
.s:
        inc ebx
        cmp ebx, NCEL
        jb .b
        popad
        ret

; la hoja, vacía
hoja_vacia:
        pushad
        mov edi, cel_off
        mov ecx, NCEL
        xor eax, eax
        rep stosd
        mov edi, cel_fmt
        mov ecx, NCEL / 2
        rep stosd
        mov edi, col_an
        mov ecx, NCOL
        mov ax, AN_NORMAL
        rep stosw
        xor eax, eax
        mov [alm_fin], eax
        mov [lim_c], eax
        mov [lim_f], eax
        mov dword [moneda_txt], 'EUR'
        mov dword [moneda_txt+4], 0
        mov byte [des_hay], 0
        mov byte [llena], 0
        call calc_olvida
        popad
        ret

; hasta dónde hay algo escrito, mirando la hoja: [lim_c] columnas y [lim_f]
; filas
limites:
        pushad
        xor ebx, ebx
        xor ebp, ebp
        xor esi, esi
.b:
        cmp dword [cel_off+esi*4], 0
        je .s
        mov eax, esi
        shr eax, 6
        inc eax
        cmp eax, ebp
        jbe .c
        mov ebp, eax
.c:
        mov eax, esi
        and eax, NCOL - 1
        inc eax
        cmp eax, ebx
        jbe .s
        mov ebx, eax
.s:
        inc esi
        cmp esi, NCEL
        jb .b
        mov [lim_c], ebx
        mov [lim_f], ebp
        popad
        ret

; =============================================================================
;  Deshacer y rehacer
; =============================================================================
; guarda la hoja como está: se llama antes de cada cambio
instantanea:
        pushad
        mov esi, cel_off
        mov edi, des_off
        mov ecx, NCEL
        rep movsd
        mov esi, cel_fmt
        mov edi, des_fmt
        mov ecx, NCEL / 2
        rep movsd
        mov esi, col_an
        mov edi, des_an
        mov ecx, NCOL / 2
        rep movsd
        mov ecx, [alm_fin]
        mov [des_fin], ecx
        add ecx, 3
        shr ecx, 2
        mov esi, almacen
        mov edi, des_alm
        rep movsd
        mov eax, [moneda_txt]
        mov [des_mon], eax
        mov eax, [moneda_txt+4]
        mov [des_mon+4], eax
        mov eax, [lim_c]
        mov [des_lim], eax
        mov eax, [lim_f]
        mov [des_lim+4], eax
        mov byte [des_hay], 1
        mov byte [des_hecho], 0
        mov byte [modif], 1
        popad
        ret
deshacer:
        cmp byte [des_hay], 0
        je des_cambia.x
        cmp byte [des_hecho], 0
        jne des_cambia.x
        mov byte [des_hecho], 1
        jmp des_cambia
rehacer:
        cmp byte [des_hay], 0
        je des_cambia.x
        cmp byte [des_hecho], 0
        je des_cambia.x
        mov byte [des_hecho], 0
; la hoja y la copia se cambian la una por la otra
des_cambia:
        pushad
        mov esi, cel_off
        mov edi, des_off
        mov ecx, NCEL
        call .cambia
        mov esi, cel_fmt
        mov edi, des_fmt
        mov ecx, NCEL / 2
        call .cambia
        mov esi, col_an
        mov edi, des_an
        mov ecx, NCOL / 2
        call .cambia
        mov ecx, [alm_fin]
        cmp ecx, [des_fin]
        jae .m
        mov ecx, [des_fin]
.m:
        add ecx, 3
        shr ecx, 2
        mov esi, almacen
        mov edi, des_alm
        call .cambia
        mov esi, alm_fin
        mov edi, des_fin
        mov ecx, 1
        call .cambia
        mov esi, moneda_txt
        mov edi, des_mon
        mov ecx, 2
        call .cambia
        mov esi, lim_c              ; (lim_c y lim_f van seguidas)
        mov edi, des_lim
        mov ecx, 2
        call .cambia
        mov byte [modif], 1
        popad
        call invalida
.x:
        ret
.cambia:
        jecxz .cx
.cb:
        mov eax, [esi]
        xchg eax, [edi]
        mov [esi], eax
        add esi, 4
        add edi, 4
        dec ecx
        jnz .cb
.cx:
        ret

; =============================================================================
;  Lo elegido
; =============================================================================
; el rango elegido, de la esquina de arriba a la izquierda a la de abajo a la
; derecha -> [r_c1], [r_f1], [r_c2], [r_f2]
sel_rect:
        push eax
        push ebx
        mov eax, [cur_col]
        mov ebx, [sel_c2]
        cmp eax, ebx
        jbe .a
        xchg eax, ebx
.a:
        mov [r_c1], eax
        mov [r_c2], ebx
        mov eax, [cur_fil]
        mov ebx, [sel_f2]
        cmp eax, ebx
        jbe .b
        xchg eax, ebx
.b:
        mov [r_f1], eax
        mov [r_f2], ebx
        pop ebx
        pop eax
        ret
; lo recorta a donde hay algo escrito (una columna entera son mil filas, y
; casi todas vacías), sin dejarlo en menos de una celda
rect_recorta:
        push eax
        mov eax, [lim_c]
        dec eax
        cmp eax, [r_c1]
        jge .a
        mov eax, [r_c1]
.a:
        cmp eax, [r_c2]
        jge .b
        mov [r_c2], eax
.b:
        mov eax, [lim_f]
        dec eax
        cmp eax, [r_f1]
        jge .c
        mov eax, [r_f1]
.c:
        cmp eax, [r_f2]
        jge .d
        mov [r_f2], eax
.d:
        pop eax
        ret

; después de un cambio: si la hoja se ha llenado, lo dice; y a repintar
fin_cambio:
        cmp byte [llena], 0
        je invalida
        mov byte [llena], 0
        push esi
        mov esi, t_llena
        call aviso
        pop esi
        jmp invalida

; ---- borrar lo elegido (el contenido; el formato se queda) ----
borrar_rango:
        pushad
        call sel_rect
        call rect_recorta
        call instantanea
        xor eax, eax
        call .borra
        popad
        jmp invalida
; (interna) vacía el rango [r_*]; con EAX distinto de 0, también el formato
.borra:
        mov edx, [r_f1]
.f:
        mov ecx, [r_c1]
.c:
        mov ebx, edx
        shl ebx, 6
        add ebx, ecx
        mov dword [cel_off+ebx*4], 0
        test eax, eax
        jz .s
        mov word [cel_fmt+ebx*2], 0
.s:
        inc ecx
        cmp ecx, [r_c2]
        jbe .c
        inc edx
        cmp edx, [r_f2]
        jbe .f
        ret

; =============================================================================
;  Copiar, cortar y pegar
; =============================================================================
; Lo copiado se guarda aquí con su formato (para pegarlo en la hoja tal cual,
; corriendo las referencias) y además va, como texto, al portapapeles de
; todos los programas: lo que se ve en cada celda, con tabuladores entre las
; columnas y un salto de línea entre las filas. El portapapeles se alcanza a
; través de un cuadro de texto escondido (el «puente»), como hace Pluma.
; CF=1 si no se ha podido copiar
copiar:
        pushad
        call sel_rect
        call rect_recorta
        mov eax, [r_c1]
        mov [clip_c0], eax
        mov eax, [r_c2]
        sub eax, [r_c1]
        inc eax
        mov [clip_cols], eax
        mov eax, [r_f1]
        mov [clip_f0], eax
        mov eax, [r_f2]
        sub eax, [r_f1]
        inc eax
        mov [clip_fils], eax
        mov edi, clip_buf
        mov ebx, clip_txt
        mov edx, [r_f1]
.f:
        mov ecx, [r_c1]
.c:
        cmp edi, clip_buf + CLIP_TAM - CEL_MAX - 4
        jae .grande
        call cel_formato
        stosw
        call cel_puntero
.t:
        lodsb
        stosb
        test al, al
        jnz .t
        call celda_ver              ; y lo que se ve, al texto de todos
        mov esi, disp
.v:
        lodsb
        test al, al
        jz .vf
        cmp ebx, clip_txt + PP_TAM - 4
        jae .v
        mov [ebx], al
        inc ebx
        jmp .v
.vf:
        inc ecx
        cmp ecx, [r_c2]
        ja .ff
        mov byte [ebx], 9
        inc ebx
        jmp .c
.ff:
        inc edx
        cmp edx, [r_f2]
        ja .fin
        mov byte [ebx], 10
        inc ebx
        jmp .f
.fin:
        cmp ebx, clip_txt           ; (que no vaya vacío: no se copiaría nada)
        jne .fin1
        mov byte [ebx], ' '
        inc ebx
.fin1:
        mov byte [ebx], 0
        mov ebp, [puente]
        mov eax, WM_SETTEXT
        mov edx, clip_txt
        HV msg_enviar
        mov eax, WM_GETTEXT         ; (como lo deja el cuadro: así se reconoce
        mov ecx, PP_TAM + 1         ; luego al pegar)
        mov edx, clip_txt
        HV msg_enviar
        mov eax, EM_SEL_TODO
        HV msg_enviar
        mov eax, WM_COPY
        HV msg_enviar
        mov byte [clip_hay], 1
        mov byte [clip_corte], 0
        popad
        clc
        ret
.grande:
        mov byte [clip_hay], 0
        mov esi, t_clip_grande
        call aviso
        popad
        stc
        ret

cortar:
        call copiar
        jc .x
        pushad
        call instantanea
        mov eax, 1
        call borrar_rango.borra
        mov byte [clip_corte], 1
        popad
        call invalida
.x:
        ret

pegar:
        pushad
        mov ebp, [puente]
        mov eax, WM_SETTEXT
        mov edx, t_nada
        HV msg_enviar
        mov eax, WM_PASTE
        HV msg_enviar
        mov eax, WM_GETTEXT
        mov ecx, PP_TAM + 1
        mov edx, pega_txt
        HV msg_enviar
        cmp byte [pega_txt], 0
        je .x
        call sel_rect
        cmp byte [clip_hay], 0
        je .fuera
        mov esi, pega_txt           ; ¿es lo último que copió Cuentas?
        mov edi, clip_txt
.cmp:
        mov al, [esi]
        cmp al, [edi]
        jne .fuera
        inc esi
        inc edi
        test al, al
        jnz .cmp
        call instantanea
        xor eax, eax
        mov [pg_i], eax
        mov [pg_j], eax
        mov eax, [clip_cols]
        imul eax, [clip_fils]
        cmp eax, 1
        jne .bloque
        mov edx, [r_f1]             ; una celda sola: a todas las elegidas
.uf:
        mov ecx, [r_c1]
.uc:
        mov esi, clip_buf
        call pega_una
        inc ecx
        cmp ecx, [r_c2]
        jbe .uc
        inc edx
        cmp edx, [r_f2]
        jbe .uf
        jmp .hecho
.bloque:
        mov esi, clip_buf
.bf:
        mov dword [pg_i], 0
.bc:
        mov ecx, [r_c1]
        add ecx, [pg_i]
        mov edx, [r_f1]
        add edx, [pg_j]
        call pega_una
        inc dword [pg_i]
        mov eax, [pg_i]
        cmp eax, [clip_cols]
        jb .bc
        inc dword [pg_j]
        mov eax, [pg_j]
        cmp eax, [clip_fils]
        jb .bf
        mov eax, [clip_cols]        ; lo pegado queda elegido
        mov ebx, [clip_fils]
        call .elige
.hecho:
        popad
        jmp fin_cambio
.x:
        popad
        ret
; un texto de otro programa: los tabuladores separan las columnas y los
; saltos de línea, las filas
.fuera:
        call instantanea
        mov esi, pega_txt
        mov ecx, [r_c1]
        mov edx, [r_f1]
        mov edi, aj_buf
        mov ebx, ecx                ; la columna más a la derecha
.e:
        lodsb
        cmp al, 13
        je .e
        cmp al, 9
        je .ecol
        cmp al, 10
        je .efil
        test al, al
        jz .efin
        cmp edi, aj_buf + CEL_MAX - 1
        jae .e
        stosb
        jmp .e
.ecol:
        call .epon
        inc ecx
        jmp .e
.efil:
        call .epon
        mov ecx, [r_c1]
        inc edx
        jmp .e
.efin:
        cmp edi, aj_buf             ; (un salto de línea al final no es otra fila)
        jne .ef1
        cmp ecx, [r_c1]
        jne .ef1
        cmp edx, [r_f1]
        je .ef1
        dec edx
        jmp .ef2
.ef1:
        call .epon
.ef2:
        mov eax, ebx
        sub eax, [r_c1]
        inc eax
        mov ebx, edx
        sub ebx, [r_f1]
        inc ebx
        call .elige
        jmp .hecho
.epon:
        mov byte [edi], 0
        mov edi, aj_buf
        cmp ecx, NCOL
        jae .ep1
        cmp edx, NFIL
        jae .ep1
        push esi
        mov esi, aj_buf
        call cel_pon
        pop esi
        cmp ecx, ebx
        jbe .ep1
        mov ebx, ecx
.ep1:
        ret
; (interna) elige EAX columnas y EBX filas desde la esquina del rango
.elige:
        mov ecx, [r_c1]
        mov [cur_col], ecx
        lea ecx, [ecx+eax-1]
        cmp ecx, NCOL - 1
        jbe .g1
        mov ecx, NCOL - 1
.g1:
        mov [sel_c2], ecx
        mov edx, [r_f1]
        mov [cur_fil], edx
        lea edx, [edx+ebx-1]
        cmp edx, NFIL - 1
        jbe .g2
        mov edx, NFIL - 1
.g2:
        mov [sel_f2], edx
        ret

; pega en la celda ECX=columna EDX=fila lo copiado que hay en ESI (dw
; formato, texto), que venía de la celda [pg_i], [pg_j] de lo copiado; ESI
; queda detrás. Las fórmulas se corren lo que haya de un sitio al otro
; (menos si se cortó: lo cortado se lleva sus fórmulas como estaban)
pega_una:
        push eax
        push ebx
        push edi
        movzx ebx, word [esi]
        add esi, 2
        cmp ecx, NCOL
        jae .salta
        cmp edx, NFIL
        jae .salta
        mov eax, edx
        shl eax, 6
        add eax, ecx
        mov [cel_fmt+eax*2], bx
        cmp byte [esi], '='
        jne .tal
        cmp byte [clip_corte], 0
        jne .tal
        mov eax, ecx
        sub eax, [clip_c0]
        sub eax, [pg_i]
        mov [aj_dc], eax
        mov eax, edx
        sub eax, [clip_f0]
        sub eax, [pg_j]
        mov [aj_df], eax
        mov dword [aj_modo], 0
        mov edi, aj_buf
        call ajusta_formula
        push esi
        mov esi, aj_buf
        call cel_pon
        pop esi
        jmp .salta
.tal:
        call cel_pon
.salta:
        lodsb
        test al, al
        jnz .salta
        pop edi
        pop ebx
        pop eax
        ret

; ---- rellenar: EAX=0 hacia abajo (la primera fila de lo elegido, a las
;      demás), 1 hacia la derecha (la primera columna). Con una sola fila (o
;      columna) elegida, se copia la de encima (o la de la izquierda) ----
rellenar:
        pushad
        mov ebp, eax
        call sel_rect
        test ebp, ebp
        jnz .der
        mov eax, [r_f1]
        cmp eax, [r_f2]
        jne .a1
        test eax, eax
        jz .nada
        dec dword [r_f1]
.a1:
        call instantanea
        mov ecx, [r_c1]
.ac:
        mov edx, [r_f1]
        inc edx
.af:
        mov eax, ecx
        mov ebx, [r_f1]
        call copia_celda
        inc edx
        cmp edx, [r_f2]
        jbe .af
        inc ecx
        cmp ecx, [r_c2]
        jbe .ac
        jmp .fin
.der:
        mov eax, [r_c1]
        cmp eax, [r_c2]
        jne .d1
        test eax, eax
        jz .nada
        dec dword [r_c1]
.d1:
        call instantanea
        mov edx, [r_f1]
.df:
        mov ecx, [r_c1]
        inc ecx
.dc:
        mov eax, [r_c1]
        mov ebx, edx
        call copia_celda
        inc ecx
        cmp ecx, [r_c2]
        jbe .dc
        inc edx
        cmp edx, [r_f2]
        jbe .df
.fin:
        popad
        jmp fin_cambio
.nada:
        popad
        ret

; copia la celda EAX=columna EBX=fila a la ECX=columna EDX=fila, con su
; formato y corriendo las referencias de su fórmula. Conserva los registros
copia_celda:
        pushad
        mov esi, ebx
        shl esi, 6
        add esi, eax
        mov edi, edx
        shl edi, 6
        add edi, ecx
        mov bp, [cel_fmt+esi*2]
        mov [cel_fmt+edi*2], bp
        mov esi, [cel_off+esi*4]
        test esi, esi
        jnz .hay
        mov dword [cel_off+edi*4], 0
        jmp .x
.hay:
        add esi, almacen
        cmp byte [esi], '='
        jne .pon
        push ecx
        push edx
        sub ecx, eax
        mov [aj_dc], ecx
        sub edx, ebx
        mov [aj_df], edx
        pop edx
        pop ecx
        mov dword [aj_modo], 0
        mov edi, aj_buf
        call ajusta_formula
        mov esi, aj_buf
.pon:
        call cel_pon
.x:
        popad
        ret

; =============================================================================
;  Las referencias de una fórmula
; =============================================================================
; copia la fórmula ESI a EDI (CEL_MAX octetos) cambiando sus referencias,
; según [aj_modo]:
;   0  corridas [aj_dc] columnas y [aj_df] filas (lo que lleva «$», no)
;   1  se han insertado [aj_n] filas en la fila [aj_pos]       3  columnas
;   2  se han eliminado [aj_n] filas desde la fila [aj_pos]    4  columnas
; Lo que cae fuera de la hoja o señalaba a algo eliminado queda «#REF».
; Los textos entre comillas y los nombres de las funciones no se tocan
ajusta_formula:
        pushad
        lea eax, [edi+CEL_MAX-1]
        mov [aj_fin], eax
.b:
        mov al, [esi]
        test al, al
        jz .fin
        cmp al, '"'
        je .cad
        cmp al, '$'
        je .ref
        mov ah, al
        and ah, 0xDF
        cmp ah, 'A'
        jb .otro
        cmp ah, 'Z'
        ja .otro
.ref:
        push esi
        call lee_ref                ; ECX=columna EDX=fila AL=los «$»
        jc .nombre
        mov ah, [esi]               ; detrás, ni letra ni «(»: es una celda
        cmp ah, '('
        je .nombre
        and ah, 0xDF
        cmp ah, 'A'
        jb .celda
        cmp ah, 'Z'
        jbe .nombre
.celda:
        add esp, 4
        call aj_una
        jmp .b
.nombre:
        pop esi                     ; un nombre: tal cual
.id:
        mov al, [esi]
        cmp al, '.'
        je .idp
        cmp al, '$'
        je .idp
        cmp al, '0'
        jb .b
        cmp al, '9'
        jbe .idp
        mov ah, al
        and ah, 0xDF
        cmp ah, 'A'
        jb .b
        cmp ah, 'Z'
        ja .b
.idp:
        call aj_pon
        inc esi
        jmp .id
.cad:
        call aj_pon
        inc esi
.c1:
        mov al, [esi]
        test al, al
        jz .fin
        call aj_pon
        inc esi
        cmp al, '"'
        jne .c1
        jmp .b
.otro:
        call aj_pon
        inc esi
        jmp .b
.fin:
        mov byte [edi], 0
        popad
        ret

; (interna) AL, al destino (si cabe)
aj_pon:
        cmp edi, [aj_fin]
        jae .x
        mov [edi], al
        inc edi
.x:
        ret

; (interna) una referencia ya leída (ECX, EDX, AL=sus «$»; ESI detrás): la
; escribe cambiada. Al insertar o eliminar, un rango (A1:B7) va entero
aj_una:
        movzx eax, al
        mov [aj_b1], eax
        cmp dword [aj_modo], 0
        jne .estr
        test al, 1
        jnz .a1
        add ecx, [aj_dc]
.a1:
        test al, 2
        jnz aj_pon_ref
        add edx, [aj_df]
        jmp aj_pon_ref
.estr:
        mov [aj_c1], ecx
        mov [aj_f1], edx
        cmp byte [esi], ':'
        jne .sola
        push esi
        inc esi
        call lee_ref
        jc .nopar
        add esp, 4
        movzx eax, al
        mov [aj_b2], eax
        mov [aj_c2], ecx
        mov [aj_f2], edx
        jmp aj_rango
.nopar:
        pop esi
.sola:
        mov ecx, [aj_c1]
        mov edx, [aj_f1]
        cmp dword [aj_modo], 2
        ja .col
        mov ebx, edx
        call aj_coord
        mov edx, ebx
        jmp .s2
.col:
        mov ebx, ecx
        call aj_coord
        mov ecx, ebx
.s2:
        jc aj_pon_almo
        mov eax, [aj_b1]
        jmp aj_pon_ref

; (interna) la fila o la columna EBX, después de insertar o eliminar -> EBX;
; CF=1 si era de las eliminadas
aj_coord:
        mov eax, [aj_pos]
        test byte [aj_modo], 1
        jz .eli
        cmp ebx, eax
        jb .ok
        add ebx, [aj_n]
.ok:
        clc
        ret
.eli:
        cmp ebx, eax
        jb .ok
        add eax, [aj_n]
        cmp ebx, eax
        jb .fuera
        sub ebx, [aj_n]
        clc
        ret
.fuera:
        stc
        ret

; (interna) un rango, de [aj_c1],[aj_f1] a [aj_c2],[aj_f2]: al eliminar, si
; se le quita un trozo se encoge; si se le quita todo, «#REF»
aj_rango:
        cmp dword [aj_modo], 2
        ja .col
        mov ecx, [aj_f1]
        mov edx, [aj_f2]
        call .par
        jc aj_pon_almo
        mov [aj_f1], ecx
        mov [aj_f2], edx
        jmp .pon
.col:
        mov ecx, [aj_c1]
        mov edx, [aj_c2]
        call .par
        jc aj_pon_almo
        mov [aj_c1], ecx
        mov [aj_c2], edx
.pon:
        mov ecx, [aj_c1]
        mov edx, [aj_f1]
        mov eax, [aj_b1]
        call aj_pon_ref
        mov al, ':'
        call aj_pon
        mov ecx, [aj_c2]
        mov edx, [aj_f2]
        mov eax, [aj_b2]
        jmp aj_pon_ref
; ECX=de EDX=a -> como quedan; CF=1 si no queda nada
.par:
        test byte [aj_modo], 1
        jz .eli
        mov ebx, ecx
        call aj_coord
        mov ecx, ebx
        mov ebx, edx
        call aj_coord
        mov edx, ebx
        clc
        ret
.eli:
        cmp ecx, edx
        ja .suelto                  ; (escrito del revés: cada una por su lado)
        mov ebx, ecx
        call aj_coord
        jnc .e1
        mov ebx, [aj_pos]           ; empezaba en lo eliminado: en lo que sigue
.e1:
        mov ecx, ebx
        mov ebx, edx
        call aj_coord
        jnc .e2
        mov ebx, [aj_pos]           ; acababa en lo eliminado: en lo de antes
        dec ebx
.e2:
        mov edx, ebx
        cmp ecx, edx
        jg .nada
        clc
        ret
.suelto:
        mov ebx, ecx
        call aj_coord
        jc .nada
        mov ecx, ebx
        mov ebx, edx
        call aj_coord
        jc .nada
        mov edx, ebx
        clc
        ret
.nada:
        stc
        ret

; (interna) escribe la referencia ECX=columna EDX=fila EAX=sus «$»; si se
; sale de la hoja, «#REF»
aj_pon_ref:
        cmp ecx, NCOL
        jae aj_pon_almo
        cmp edx, NFIL
        jae aj_pon_almo
        push ebx
        push esi
        mov ebx, eax
        test bl, 1
        jz .c
        mov al, '$'
        call aj_pon
.c:
        push edi
        mov edi, aj_tmp
        call col_nombre
        test bl, 2
        jz .f
        mov al, '$'
        stosb
.f:
        lea eax, [edx+1]
        call hvl_num
        mov byte [edi], 0
        pop edi
        mov esi, aj_tmp
.p:
        lodsb
        test al, al
        jz .x
        call aj_pon
        jmp .p
.x:
        pop esi
        pop ebx
        ret
aj_pon_almo:
        push esi
        mov esi, t_err_ref
.p:
        lodsb
        test al, al
        jz .x
        call aj_pon
        jmp .p
.x:
        pop esi
        ret

; ECX=columna -> sus letras (A, B... Z, AA, AB...) en EDI, que avanza
col_nombre:
        push eax
        push edx
        mov eax, ecx
        cmp eax, 26
        jb .una
        xor edx, edx
        push ecx
        mov ecx, 26
        div ecx
        pop ecx
        add al, 'A' - 1
        stosb
        mov eax, edx
.una:
        add al, 'A'
        stosb
        pop edx
        pop eax
        ret
; ECX=columna EDX=fila -> «B7» en EDI, que avanza
ref_nombre:
        push eax
        call col_nombre
        lea eax, [edx+1]
        call hvl_num
        pop eax
        ret

; corrige todas las fórmulas de la hoja (con [aj_modo], [aj_pos] y [aj_n])
aj_todas:
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
        call ajusta_formula
        mov edi, aj_buf             ; ¿ha cambiado?
.c:
        mov al, [esi]
        cmp al, [edi]
        jne .dif
        inc esi
        inc edi
        test al, al
        jnz .c
        jmp .s
.dif:
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

; =============================================================================
;  Insertar y eliminar filas y columnas (las de lo elegido)
; =============================================================================
; EAX=0 insertar, 1 eliminar
filas_orden:
        pushad
        mov ebp, eax
        call sel_rect
        mov eax, [r_f1]
        mov [aj_pos], eax
        mov ebx, [r_f2]
        sub ebx, eax
        inc ebx
        mov [aj_n], ebx             ; EBX=cuántas
        test ebp, ebp
        jnz .eli
        mov esi, NFIL               ; ¿caben? Las últimas tienen que estar vacías
        sub esi, ebx
        shl esi, 6
.v:
        cmp dword [cel_off+esi*4], 0
        jne no_cabe
        inc esi
        cmp esi, NCEL
        jb .v
        call instantanea
        mov ecx, NFIL               ; las de debajo, hacia abajo (de atrás adelante)
        sub ecx, ebx
        sub ecx, [aj_pos]
        shl ecx, 6
        push ecx
        mov esi, NFIL
        sub esi, ebx
        shl esi, 6
        lea edi, [cel_off+NCEL*4-4]
        lea esi, [cel_off+esi*4-4]
        std
        rep movsd
        pop ecx
        mov esi, NFIL
        sub esi, ebx
        shl esi, 6
        lea edi, [cel_fmt+NCEL*2-2]
        lea esi, [cel_fmt+esi*2-2]
        rep movsw
        cld
        mov edi, [aj_pos]           ; y las nuevas, vacías
        shl edi, 6
        mov ecx, ebx
        shl ecx, 6
        push ecx
        push edi
        lea edi, [cel_off+edi*4]
        xor eax, eax
        rep stosd
        pop edi
        pop ecx
        lea edi, [cel_fmt+edi*2]
        rep stosw
        mov dword [aj_modo], 1
        jmp .fin
.eli:
        call instantanea
        mov ecx, NFIL               ; las de debajo, hacia arriba
        sub ecx, ebx
        sub ecx, [aj_pos]
        shl ecx, 6
        mov edi, [aj_pos]
        shl edi, 6
        mov esi, ebx
        shl esi, 6
        add esi, edi
        push ecx
        push esi
        push edi
        lea esi, [cel_off+esi*4]
        lea edi, [cel_off+edi*4]
        rep movsd
        pop edi
        pop esi
        pop ecx
        lea esi, [cel_fmt+esi*2]
        lea edi, [cel_fmt+edi*2]
        rep movsw
        mov edi, NFIL               ; y las últimas, vacías
        sub edi, ebx
        shl edi, 6
        mov ecx, ebx
        shl ecx, 6
        push ecx
        push edi
        lea edi, [cel_off+edi*4]
        xor eax, eax
        rep stosd
        pop edi
        pop ecx
        lea edi, [cel_fmt+edi*2]
        rep stosw
        mov dword [aj_modo], 2
.fin:
        call alm_indices
        call aj_todas
        call limites
        popad
        jmp fin_cambio
no_cabe:
        mov esi, t_no_cabe
        call aviso
        popad
        ret

; EAX=0 insertar, 1 eliminar
cols_orden:
        pushad
        mov ebp, eax
        call sel_rect
        mov eax, [r_c1]
        mov [aj_pos], eax
        mov ebx, [r_c2]
        sub ebx, eax
        inc ebx
        mov [aj_n], ebx             ; EBX=cuántas
        test ebp, ebp
        jnz .hazlo
        xor edx, edx                ; ¿caben? Las últimas columnas, vacías
.v:
        mov ecx, NCOL
        sub ecx, ebx
.v1:
        mov eax, edx
        shl eax, 6
        add eax, ecx
        cmp dword [cel_off+eax*4], 0
        jne no_cabe
        inc ecx
        cmp ecx, NCOL
        jb .v1
        inc edx
        cmp edx, NFIL
        jb .v
.hazlo:
        call instantanea
        xor edx, edx                ; fila a fila
.f:
        mov esi, edx
        shl esi, 6
        test ebp, ebp
        jnz .e
        mov ecx, NCOL - 1           ; insertar: de la última a la primera
.i1:
        mov eax, ecx
        sub eax, ebx
        cmp eax, [aj_pos]
        jl .i2
        lea edi, [esi+eax]
        push dword [cel_off+edi*4]
        mov di, [cel_fmt+edi*2]
        lea eax, [esi+ecx]
        mov [cel_fmt+eax*2], di
        pop dword [cel_off+eax*4]
        dec ecx
        jmp .i1
.i2:
        mov ecx, [aj_pos]           ; y las nuevas, vacías
        mov edi, ebx
.i3:
        lea eax, [esi+ecx]
        mov dword [cel_off+eax*4], 0
        mov word [cel_fmt+eax*2], 0
        inc ecx
        dec edi
        jnz .i3
        jmp .sig
.e:
        mov ecx, [aj_pos]           ; eliminar: de la primera a la última
.e1:
        lea eax, [ecx+ebx]
        cmp eax, NCOL
        jae .e2
        lea edi, [esi+eax]
        push dword [cel_off+edi*4]
        mov di, [cel_fmt+edi*2]
        lea eax, [esi+ecx]
        mov [cel_fmt+eax*2], di
        pop dword [cel_off+eax*4]
        inc ecx
        jmp .e1
.e2:
        cmp ecx, NCOL
        jae .sig
        lea eax, [esi+ecx]
        mov dword [cel_off+eax*4], 0
        mov word [cel_fmt+eax*2], 0
        inc ecx
        jmp .e2
.sig:
        inc edx
        cmp edx, NFIL
        jb .f
        test ebp, ebp               ; los anchos de las columnas, lo mismo
        jnz .ae
        mov ecx, NCOL - 1
.a1:
        mov eax, ecx
        sub eax, ebx
        cmp eax, [aj_pos]
        jl .a2
        mov di, [col_an+eax*2]
        mov [col_an+ecx*2], di
        dec ecx
        jmp .a1
.a2:
        mov dword [aj_modo], 3
        jmp .fin
.ae:
        mov ecx, [aj_pos]
.a3:
        lea eax, [ecx+ebx]
        cmp eax, NCOL
        jae .a4
        mov di, [col_an+eax*2]
        mov [col_an+ecx*2], di
        inc ecx
        jmp .a3
.a4:
        cmp ecx, NCOL
        jae .a5
        mov word [col_an+ecx*2], AN_NORMAL
        inc ecx
        jmp .a4
.a5:
        mov dword [aj_modo], 4
.fin:
        call alm_indices
        call aj_todas
        call limites
        popad
        jmp fin_cambio

; =============================================================================
;  Ordenar las filas de lo elegido por la columna de la celda activa
; =============================================================================
; EAX=0 de menor a mayor, 1 de mayor a menor. Primero los números, luego los
; textos (sin mirar las mayúsculas) y, al final siempre, las vacías. Las
; filas se mueven enteras (lo que cae dentro de lo elegido), con su formato;
; las fórmulas no se corrigen: van como están escritas.
ordenar:
        pushad
        mov [or_desc], eax
        call sel_rect
        call rect_recorta
        mov ebp, [r_f2]
        sub ebp, [r_f1]
        inc ebp                     ; EBP=cuántas filas
        cmp ebp, 2
        jae .hay
        mov esi, t_ordenar_elija
        call aviso
        popad
        ret
.hay:
        call instantanea
        xor ebx, ebx                ; la clave de cada fila
.k:
        mov [or_fila+ebx*2], bx
        mov ecx, [cur_col]
        mov edx, [r_f1]
        add edx, ebx
        call valor_celda
        mov cl, 1                   ; 0 número, 1 texto (o error), 2 vacía
        cmp byte [ev_err], 0
        jne .k1
        mov cl, 2
        cmp byte [cv_tipo], 0
        je .k1
        mov cl, 1
        cmp byte [cv_tipo], 1
        jne .k1
        mov cl, 0
        mov [or_val+ebx*8], eax
        mov [or_val+ebx*8+4], edx
.k1:
        mov [or_clase+ebx], cl
        inc ebx
        cmp ebx, ebp
        jb .k
        mov ebx, 1                  ; por inserción (no cambia el orden de las iguales)
.s:
        cmp ebx, ebp
        jae .mover
        movzx esi, word [or_fila+ebx*2]
        mov edi, ebx
.s1:
        test edi, edi
        jz .s2
        movzx eax, word [or_fila+edi*2-2]
        mov edx, esi
        call .compara               ; ¿la de antes va detrás?
        jle .s2
        mov ax, [or_fila+edi*2-2]
        mov [or_fila+edi*2], ax
        dec edi
        jmp .s1
.s2:
        mov [or_fila+edi*2], si
        inc ebx
        jmp .s
.mover:
        ; las filas, a un sitio aparte (el de los valores apuntados, que
        ; ahora no vale) y de vuelta en su orden
        xor ebx, ebx
.m1:
        mov esi, [r_f1]
        add esi, ebx
        shl esi, 6
        mov edi, ebx
        shl edi, 6
        mov ecx, [r_c1]
        add esi, ecx
        add edi, ecx
.m2:
        mov eax, [cel_off+esi*4]
        mov [cv_val+edi*4], eax
        mov ax, [cel_fmt+esi*2]
        mov [cv_val+NCEL*4+edi*2], ax
        inc esi
        inc edi
        inc ecx
        cmp ecx, [r_c2]
        jbe .m2
        inc ebx
        cmp ebx, ebp
        jb .m1
        xor ebx, ebx
.m3:
        mov edi, [r_f1]
        add edi, ebx
        shl edi, 6
        movzx esi, word [or_fila+ebx*2]
        shl esi, 6
        mov ecx, [r_c1]
        add esi, ecx
        add edi, ecx
.m4:
        mov eax, [cv_val+esi*4]
        mov [cel_off+edi*4], eax
        mov ax, [cv_val+NCEL*4+esi*2]
        mov [cel_fmt+edi*2], ax
        inc esi
        inc edi
        inc ecx
        cmp ecx, [r_c2]
        jbe .m4
        inc ebx
        cmp ebx, ebp
        jb .m3
        call alm_indices
        popad
        jmp invalida
; (interna) compara las filas EAX y EDX (contadas desde la primera de lo
; elegido) -> las banderas de «cmp»: mayor si EAX va detrás de EDX
.compara:
        push ebx
        push ecx
        push esi
        push edi
        mov bl, [or_clase+eax]
        mov bh, [or_clase+edx]
        cmp bl, 2                   ; las vacías, al final siempre
        je .cv
        cmp bh, 2
        jne .c0
        mov eax, -1
        jmp .cf
.c0:
        cmp bl, bh
        jne .cclase
        test bl, bl
        jnz .ctexto
        mov ecx, [or_val+eax*8+4]   ; dos números
        cmp ecx, [or_val+edx*8+4]
        jg .cmayor
        jl .cmenor
        mov ecx, [or_val+eax*8]
        cmp ecx, [or_val+edx*8]
        ja .cmayor
        jb .cmenor
.cigual:
        xor eax, eax
        jmp .cf
.cv:
        cmp bh, 2
        je .cigual
        mov eax, 1
        jmp .cf
.cclase:
        ja .cmayor
        jmp .cmenor
.ctexto:
        push edx                    ; dos textos: como se ven
        mov ecx, [cur_col]
        mov edx, [r_f1]
        add edx, eax
        call celda_ver
        mov esi, disp
        mov edi, or_txt
.ct1:
        lodsb
        stosb
        test al, al
        jnz .ct1
        pop edx
        add edx, [r_f1]
        call celda_ver
        mov esi, or_txt
        mov edi, disp
        call cad_cmp
        jmp .cdir
.cmayor:
        mov eax, 1
        jmp .cdir
.cmenor:
        mov eax, -1
.cdir:
        cmp dword [or_desc], 0
        je .cf
        neg eax
.cf:
        pop edi
        pop esi
        pop ecx
        pop ebx
        cmp eax, 0
        ret

; =============================================================================
;  Buscar: la celda siguiente (por filas) cuyo contenido lleva el texto
;  [busca_txt], sin mirar las mayúsculas
; =============================================================================
buscar_sig:
        pushad
        cmp byte [busca_txt], 0
        je .x
        mov ebx, [cur_fil]
        shl ebx, 6
        add ebx, [cur_col]
        mov ebp, NCEL
.b:
        inc ebx
        cmp ebx, NCEL
        jb .b1
        xor ebx, ebx
.b1:
        mov esi, [cel_off+ebx*4]
        test esi, esi
        jz .s
        add esi, almacen
.c:
        cmp byte [esi], 0           ; ¿está aquí, o más adelante?
        je .s
        xor ecx, ecx
.c1:
        mov al, [busca_txt+ecx]
        test al, al
        jz .esta
        call may_car
        mov ah, al
        mov al, [esi+ecx]
        call may_car
        cmp al, ah
        jne .c2
        inc ecx
        jmp .c1
.c2:
        inc esi
        jmp .c
.s:
        dec ebp
        jnz .b
        mov esi, t_no_esta
        call aviso
.x:
        popad
        ret
.esta:
        mov eax, ebx
        and eax, NCOL - 1
        mov [cur_col], eax
        mov [sel_c2], eax
        shr ebx, 6
        mov [cur_fil], ebx
        mov [sel_f2], ebx
        call a_la_vista
        popad
        jmp invalida

        section .bss
            alignb 4
alm_fin     resd 1                  ; lo que hay usado del almacén
lim_c       resd 1                  ; hasta dónde hay algo escrito: columnas
lim_f       resd 1                  ; y filas (detrás de lim_c)
r_c1        resd 1                  ; el rango con el que se trabaja
r_f1        resd 1
r_c2        resd 1
r_f2        resd 1
des_fin     resd 1                  ; la copia de deshacer: su alm_fin,
des_lim     resd 2                  ; sus límites
des_mon     resd 2                  ; y su moneda
clip_c0     resd 1                  ; lo copiado: de dónde,
clip_f0     resd 1
clip_cols   resd 1                  ; y cuánto
clip_fils   resd 1
pg_i        resd 1                  ; al pegar: la celda de lo copiado que toca
pg_j        resd 1
aj_modo     resd 1                  ; ajusta_formula: qué hay que hacer
aj_dc       resd 1
aj_df       resd 1
aj_pos      resd 1
aj_n        resd 1
aj_fin      resd 1
aj_c1       resd 1
aj_f1       resd 1
aj_c2       resd 1
aj_f2       resd 1
aj_b1       resd 1
aj_b2       resd 1
or_desc     resd 1
des_hay     resb 1                  ; hay una copia para deshacer
des_hecho   resb 1                  ; y ya se ha deshecho (se puede rehacer)
clip_hay    resb 1
clip_corte  resb 1                  ; lo copiado se cortó
llena       resb 1                  ; no ha cabido algo en el almacén
            alignb 4
aj_tmp      resb 16
aj_buf      resb CEL_MAX + 4
cp_buf      resb CEL_MAX + 4
or_txt      resb 260
busca_txt   resb 44
or_fila     resw NFIL
or_clase    resb NFIL
            alignb 4
or_val      resd 2 * NFIL
col_an      resw NCOL               ; el ancho de cada columna, en puntos
des_an      resw NCOL
cel_off     resd NCEL
des_off     resd NCEL
cel_fmt     resw NCEL
des_fmt     resw NCEL
clip_txt    resb PP_TAM + 4
pega_txt    resb PP_TAM + 4
clip_buf    resb CLIP_TAM
almacen     resb ALM_TAM
des_alm     resb ALM_TAM
        section .text
