; =============================================================================
;  MIRADOR.HVP - el formato PNG
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Todas las clases de color (grises, color verdadero, paleta, y grises y
;  color con opacidad) con todas sus profundidades (1, 2, 4, 8 y 16 bits por
;  muestra; de las de 16 se toma el octeto de más peso), los cinco filtros,
;  el entrelazado Adam7, la paleta (PLTE) y la transparencia (tRNS: opacidad
;  por color de la paleta, o un color que es el transparente).
;
;  Cómo. Ni el archivo ni la imagen sin comprimir tienen que estar enteros en
;  memoria: el archivo se va pidiendo a trozos (mi_trozo: de la memoria, o
;  del disco) y los datos de los IDAT pasan, según llegan, por el
;  descompresor por tramos (programas/comun/desinfla_t.inc), al que se le
;  pide una fila cada vez. De una imagen sin entrelazar solo hay en memoria
;  dos filas (la que se hace y la de encima, que hace falta para los
;  filtros): un PNG de muchos megas se ve con unos 60 K, mida lo que mida.
;  Con Adam7 las filas no vienen en orden: se descomprime la imagen entera
;  a la memoria de trabajo (de uno a ocho octetos por punto), se deshacen los
;  filtros de las siete pasadas y cada fila se entrega juntando los puntos
;  que le tocan de cada una.
;  Se comprueban la suma (CRC-32) de cada trozo y la de zlib: si alguna no
;  cuadra o los datos se acaban antes, se enseña lo que haya y se avisa.
; =============================================================================
PNG_TROZO   equ 4096                ; lo que se le da al descompresor de cada vez

%macro PNG_BSS 0
            alignb 4
png_dat     resd 1
png_prof    resd 1                  ; bits por muestra
png_color   resd 1                  ; clase de color (0, 2, 3, 4, 6)
png_entre   resd 1                  ; con Adam7
png_bpp     resd 1                  ; octetos por punto, para los filtros (1 al menos)
png_bitsp   resd 1                  ; bits por punto
png_crudo   resd 1                  ; (Adam7) octetos de la imagen sin comprimir
png_validos resd 1                  ; los que de verdad han salido
png_b       resd 1                  ; (Adam7) la imagen sin comprimir
png_r       resd 1                  ; una fila con los puntos desempaquetados
png_e       resd 1                  ; lo mismo, de una pasada (y una fila de ceros)
png_npas    resd 1
png_trns    resd 1                  ; hay un color transparente
png_clave   resd 3                  ; cuál (16 bits por componente)
png_y       resd 1
png_dest    resd 1
png_pos     resd 1                  ; por dónde se va en el archivo: la cabecera del trozo
                                    ; siguiente o, dentro de un IDAT, sus datos
png_resto   resd 1                  ; octetos que quedan del IDAT de ahora
png_en_idat resd 1                  ; se está dentro de un IDAT (falta comprobar su suma)
png_crc_v   resd 1                  ; la suma que se lleva del trozo de ahora
png_sin_mas resd 1                  ; ya no hay más datos comprimidos
png_acabado resd 1                  ; el descompresor ha terminado (bien o mal)
png_ant     resd 1                  ; (sin Adam7) la fila de encima
png_act     resd 1                  ; y la que se hace, con su octeto de filtro delante
png_lista   resd 1                  ; octetos de la fila ya descomprimida en png_act (-1: no)
png_sig     resd 1                  ; la fila que toca entregar (-1: ya no hay más)
; de cada pasada: dónde empieza, ancho, alto y octetos por fila (sin el del filtro)
png_pd      resd 7
png_pw      resd 7
png_ph      resd 7
png_po      resd 7
png_crc_t   resd 256
png_sobra   resb 4
%endmacro

; las siete pasadas de Adam7: primera columna y fila, y cada cuántas
png_xs      dd 0, 4, 0, 2, 0, 1, 0
png_ys      dd 0, 0, 4, 0, 2, 0, 1
png_dx      dd 8, 8, 4, 4, 2, 2, 1
png_dy      dd 8, 8, 8, 4, 4, 2, 2

png_detecta:
        cmp ecx, 8 + 25
        jb .no
        cmp dword [esi], 0x474E5089
        jne .no
        cmp dword [esi+4], 0x0A1A0A0D
        jne .no
        clc
        ret
.no:
        stc
        ret

; un PNG se puede ver siempre sin tenerlo entero en memoria
png_tramos:
        clc
        ret

png_abre:
        mov [png_dat], esi
        cmp dword [esi+12], 'IHDR'
        jne .roto
        mov eax, [esi+16]
        call mi_be32
        mov [im_an], eax
        mov eax, [esi+20]
        call mi_be32
        mov [im_al], eax
        call mi_medidas_ok
        jc .grande
        movzx eax, byte [esi+24]
        mov [png_prof], eax
        movzx ebx, byte [esi+25]
        mov [png_color], ebx
        cmp word [esi+26], 0        ; ni otra compresión ni otros filtros
        jne .roto
        movzx ecx, byte [esi+28]
        cmp ecx, 1
        ja .roto
        mov [png_entre], ecx
        ; la profundidad que admite cada clase de color, y sus muestras por punto
        mov ecx, 1                  ; ECX=muestras
        mov dword [im_clase], IC_GRIS
        cmp ebx, 0
        je .p_cualquiera
        mov dword [im_clase], IC_PALETA
        cmp ebx, 3
        je .p_hasta8
        mov ecx, 3
        mov dword [im_clase], IC_RGB
        cmp ebx, 2
        je .p_8o16
        mov ecx, 2
        mov dword [im_clase], IC_GRISA
        cmp ebx, 4
        je .p_8o16
        mov ecx, 4
        mov dword [im_clase], IC_RGBA
        cmp ebx, 6
        je .p_8o16
        jmp .roto
.p_hasta8:
        cmp eax, 16
        je .roto
.p_cualquiera:
        cmp eax, 1
        je .p_ok
        cmp eax, 2
        je .p_ok
        cmp eax, 4
        je .p_ok
.p_8o16:
        cmp eax, 8
        je .p_ok
        cmp eax, 16
        jne .roto
.p_ok:
        imul eax, ecx
        mov [png_bitsp], eax
        mov [im_bits], eax
        add eax, 7
        shr eax, 3
        mov [png_bpp], eax
        cmp ebx, 4
        jb .a1
        mov dword [im_hay_alfa], 1
.a1:
        call png_crc_tabla
        ; ---- los trozos de delante de los datos: la paleta y la
        ;      transparencia; y de todos, su suma ----
        xor eax, eax
        mov [png_trns], eax
        mov dword [png_pos], 8      ; (el primero es IHDR: su suma también se comprueba)
        call png_suma_trozo
        mov dword [png_pos], 33
.t:
        mov eax, [png_pos]
        mov ecx, 8
        call mi_trozo
        jc .roto                    ; se acaba el archivo sin datos
        mov eax, [esi]
        call mi_be32
        mov ebx, eax                ; EBX=octetos del trozo
        mov edx, [esi+4]            ; EDX=su nombre
        cmp edx, 'IDAT'
        je .idat
        cmp edx, 'IEND'
        je .roto
        mov ecx, 768                ; la paleta y la transparencia caben siempre en poco
        cmp edx, 'PLTE'
        je .t1
        mov ecx, 256
        cmp edx, 'tRNS'
        jne .t3
.t1:
        cmp ecx, ebx
        jbe .t2
        mov ecx, ebx
.t2:
        mov eax, [png_pos]
        add eax, 8
        call mi_trozo
        jc .t3                      ; (cortado: como si no estuviera)
        push ebx
        call png_un_trozo
        pop ebx
.t3:
        call png_suma_trozo         ; su suma; y al siguiente
        jnc .t
        jmp .roto                   ; (no cabe en lo que queda de archivo)
.idat:
        ; las pasadas y lo que ocupa la imagen sin comprimir
        mov dword [png_npas], 1
        mov eax, [im_an]
        mov [png_pw], eax
        mov eax, [im_al]
        mov [png_ph], eax
        cmp dword [png_entre], 0
        je .pas
        mov dword [png_npas], 7
        xor ebx, ebx
.ad:
        xor edx, edx                ; ancho de la pasada: (ancho - x0 + dx - 1) / dx
        mov eax, [im_an]
        sub eax, [png_xs+ebx*4]
        jbe .ad1
        add eax, [png_dx+ebx*4]
        dec eax
        div dword [png_dx+ebx*4]
        mov edx, eax
.ad1:
        mov [png_pw+ebx*4], edx
        xor edx, edx
        mov eax, [im_al]
        sub eax, [png_ys+ebx*4]
        jbe .ad2
        add eax, [png_dy+ebx*4]
        dec eax
        div dword [png_dy+ebx*4]
        mov edx, eax
.ad2:
        mov [png_ph+ebx*4], edx
        inc ebx
        cmp ebx, 7
        jb .ad
.pas:
        xor ebx, ebx
        xor edi, edi                ; EDI=lo que llevan las anteriores
.pa:
        mov [png_pd+ebx*4], edi
        mov eax, [png_pw+ebx*4]
        imul eax, [png_bitsp]
        add eax, 7
        shr eax, 3
        mov [png_po+ebx*4], eax
        cmp dword [png_pw+ebx*4], 0 ; (una pasada sin puntos no tiene ni filas)
        je .pa1
        inc eax
        imul eax, [png_ph+ebx*4]
        add edi, eax
.pa1:
        inc ebx
        cmp ebx, [png_npas]
        jb .pa
        mov [png_crudo], edi
        ; la memoria de trabajo: la imagen entera (Adam7) o dos filas; dos
        ; filas de hasta ocho octetos por punto; y la del descompresor
        cmp dword [png_entre], 0
        jne .m1
        mov edi, [png_po]
        lea edi, [edi*2+32]
.m1:
        mov eax, [im_an]
        shl eax, 4
        lea eax, [eax+edi+64+DST_MEM+16]
        mov [im_alm], eax
        mov dword [im_buf_n], PNG_TROZO
        ; con un color transparente, cada punto lleva además su opacidad
        cmp dword [png_trns], 0
        je .bien
        mov dword [im_hay_alfa], 1
        cmp dword [im_clase], IC_GRIS
        jne .tr1
        mov dword [im_clase], IC_GRISA
.tr1:
        cmp dword [im_clase], IC_RGB
        jne .bien
        mov dword [im_clase], IC_RGBA
.bien:
        xor eax, eax
        ret
.roto:
        mov eax, ME_ROTO
        ret
.grande:
        mov eax, ME_GRANDE
        ret

; (interna) un trozo de delante de los datos: EDX=su nombre ESI=sus datos
; ECX=cuántos hay (PLTE: hasta 768; tRNS: hasta 256)
png_un_trozo:
        cmp edx, 'PLTE'
        jne .t_trns
        mov eax, ecx
        xor edx, edx
        mov ecx, 3
        div ecx
        cmp eax, 256
        jbe .tp1
        mov eax, 256
.tp1:
        mov [im_ncol], eax
        mov ecx, eax
        xor eax, eax
.tp2:
        test ecx, ecx
        jz .t_x
        mov dl, [esi]
        mov [im_pal+eax*4], dl
        mov dl, [esi+1]
        mov [im_pal+eax*4+1], dl
        mov dl, [esi+2]
        mov [im_pal+eax*4+2], dl
        add esi, 3
        inc eax
        dec ecx
        jmp .tp2
.t_trns:
        mov eax, [png_color]
        cmp eax, 3
        jne .tt1
        xor eax, eax                ; con paleta: la opacidad de cada color
.tt0b:
        test ecx, ecx
        jz .t_x
        mov dl, [esi+eax]
        mov [im_pal+eax*4+3], dl
        cmp dl, 255
        je .tt0c
        mov dword [im_hay_alfa], 1
.tt0c:
        inc eax
        dec ecx
        jmp .tt0b
.tt1:
        test eax, eax               ; grises: el gris transparente
        jnz .tt2
        cmp ecx, 2
        jb .t_x
        movzx edx, word [esi]
        xchg dl, dh
        mov [png_clave], edx
        mov dword [png_trns], 1
        ret
.tt2:
        cmp eax, 2                  ; color: el color transparente
        jne .t_x
        cmp ecx, 6
        jb .t_x
        xor eax, eax
.tt3:
        movzx edx, word [esi+eax*2]
        xchg dl, dh
        mov [png_clave+eax*4], edx
        inc eax
        cmp eax, 3
        jb .tt3
        mov dword [png_trns], 1
.t_x:
        ret

; (interna) el trozo cuya cabecera está en [png_pos]: comprueba su suma
; (CRC-32 del nombre y los datos; si no cuadra, se avisa) y deja [png_pos] en
; el siguiente. CF=1 si el trozo no cabe en lo que queda de archivo ([png_pos]
; no cambia). Los datos se piden de MI_MINI en MI_MINI: valen también antes
; de tener el búfer del disco
png_suma_trozo:
        pushad
        mov eax, [png_pos]
        mov ecx, 8
        call mi_trozo
        jc .no
        mov eax, [esi]
        call mi_be32
        mov ebx, eax                ; EBX=octetos de datos
        mov edx, [im_tam]           ; ¿caben los datos y la suma?
        sub edx, [png_pos]
        sub edx, 8
        cmp ebx, edx
        ja .no
        sub edx, ebx
        cmp edx, 4
        jb .no
        mov ebp, [png_pos]          ; EBP=dónde empieza el siguiente
        lea ebp, [ebp+ebx+12]
        add esi, 4                  ; la suma empieza por el nombre
        mov ecx, 4
        mov eax, 0xFFFFFFFF
        call png_crc
        mov edi, [png_pos]
        add edi, 8                  ; EDI=por dónde se va
.d:
        test ebx, ebx
        jz .fin
        mov ecx, ebx
        cmp ecx, MI_MINI
        jbe .d1
        mov ecx, MI_MINI
.d1:
        push eax
        mov eax, edi
        call mi_trozo
        pop eax
        jc .mal                     ; (el disco falla: no se puede saber)
        call png_crc
        add edi, ecx
        sub ebx, ecx
        jmp .d
.fin:
        not eax
        mov edx, eax
        mov eax, edi
        mov ecx, 4
        call mi_trozo
        jc .mal
        mov eax, [esi]
        call mi_be32
        cmp eax, edx
        je .bien
.mal:
        mov dword [im_aviso], 1
.bien:
        mov [png_pos], ebp
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; EAX=la suma (CRC-32) hasta ahora (se empieza con 0FFFFFFFFh y al final se
; le da la vuelta a todos los bits) ESI=datos ECX=cuántos -> EAX. Conserva lo
; demás. Antes, una vez, png_crc_tabla
png_crc:
        test ecx, ecx
        jz .x
        push ebx
        push ecx
        push esi
        xor ebx, ebx
.b:
        mov bl, [esi]
        inc esi
        xor bl, al
        shr eax, 8
        xor eax, [png_crc_t+ebx*4]
        dec ecx
        jnz .b
        pop esi
        pop ecx
        pop ebx
.x:
        ret
png_crc_tabla:
        pushad
        xor ebx, ebx
.t:
        mov eax, ebx
        mov ecx, 8
.t1:
        shr eax, 1
        jnc .t2
        xor eax, 0xEDB88320
.t2:
        dec ecx
        jnz .t1
        mov [png_crc_t+ebx*4], eax
        inc ebx
        cmp ebx, 256
        jb .t
        popad
        ret

png_prepara:
        ; el reparto de la memoria: la imagen (Adam7) o dos filas; dos filas
        ; desempaquetadas; y la del descompresor
        mov edi, [im_almp]
        cmp dword [png_entre], 0
        jne .r1
        lea eax, [edi+1]            ; (cada búfer: un octeto para el filtro y la fila)
        mov [png_ant], eax
        mov eax, [png_po]
        lea edi, [edi+eax+16]
        mov [png_act], edi
        lea edi, [edi+eax+16]
        jmp .r2
.r1:
        mov [png_b], edi
        add edi, [png_crudo]
.r2:
        add edi, 3
        and edi, ~3
        mov [png_r], edi
        mov edx, [im_an]
        lea edi, [edi+edx*8+16]
        mov [png_e], edi
        lea edi, [edi+edx*8+16]
        mov eax, 1                  ; los IDAT llevan dentro un flujo zlib
        call dst_empieza
        xor eax, eax
        mov [png_resto], eax
        mov [png_en_idat], eax
        mov [png_sin_mas], eax
        mov [png_acabado], eax
        mov [png_sig], eax
        mov dword [png_lista], -1
        cmp dword [png_entre], 0
        jne .adam
        ; ---- sin entrelazar: la fila de encima de la primera son ceros; y
        ;      la primera se descomprime ya, para saber si hay imagen ----
        mov edi, [png_ant]
        mov ecx, [png_po]
        xor al, al
        rep stosb
        mov edi, [png_act]
        mov ecx, [png_po]
        inc ecx
        call png_infla
        mov [png_lista], eax
        test eax, eax
        jz .roto
        xor eax, eax
        ret
.adam:
        ; ---- con Adam7: la imagen entera, de una vez ----
        mov edi, [png_b]
        mov ecx, [png_crudo]
        call png_infla
        test eax, eax
        jz .roto
        mov [png_validos], eax
        mov ecx, [png_crudo]        ; lo que no ha salido, a cero
        sub ecx, eax
        jbe .d3
        mov dword [im_aviso], 1
        mov edi, [png_b]
        add edi, eax
        xor al, al
        rep stosb
        jmp .d4
.d3:
        call png_remata             ; (ha salido toda: las sumas del final)
.d4:
        ; ---- los filtros, fuera: pasada a pasada y fila a fila ----
        xor ebx, ebx                ; EBX=la pasada
.pas:
        cmp dword [png_pw+ebx*4], 0
        je .pas9
        mov edi, [png_e]            ; la fila de encima de la primera: ceros
        mov ecx, [png_po+ebx*4]
        xor al, al
        rep stosb
        mov edx, [png_e]            ; EDX=la fila de encima
        mov esi, [png_b]
        add esi, [png_pd+ebx*4]     ; ESI=la fila (con su octeto de filtro)
        mov ebp, [png_ph+ebx*4]
.fil:
        test ebp, ebp
        jz .pas9
        movzx eax, byte [esi]
        inc esi
        mov ecx, [png_po+ebx*4]
        push ebx
        push ebp
        call png_filtro
        pop ebp
        pop ebx
        mov edx, esi
        add esi, [png_po+ebx*4]
        dec ebp
        jmp .fil
.pas9:
        inc ebx
        cmp ebx, [png_npas]
        jb .pas
        xor eax, eax
        ret
.roto:
        mov eax, ME_ROTO
        ret

; (interna) EDI=destino ECX=cuántos octetos sin comprimir se quieren -> EAX=
; los que han salido (menos si los datos se acaban o están rotos: se avisa).
; Conserva los demás registros
png_infla:
        push ebx
        push ecx
        push edx
        push edi
        xor ebx, ebx                ; EBX=los que van
.l:
        test ecx, ecx
        jz .x
        cmp dword [png_acabado], 0
        jne .x
        call dst_sigue              ; EAX=escritos EDX=por qué para
        add ebx, eax
        add edi, eax
        sub ecx, eax
        cmp edx, DST_HECHO
        je .x
        cmp edx, DST_HAMBRE
        jne .fin
        call png_mas                ; otro trozo de los IDAT (o que ya no hay)
        jmp .l
.fin:
        mov dword [png_acabado], 1
        cmp edx, DST_FIN
        jne .mal
        call png_fin_idat           ; la suma del último IDAT (si se ha leído entero)...
        cmp dword [dst_suma], 0     ; ...y la de zlib: si no cuadra, puede haber basura
        je .x
.mal:
        mov dword [im_aviso], 1
.x:
        mov eax, ebx
        pop edi
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) le da al descompresor el trozo siguiente de los datos de los
; IDAT (hasta PNG_TROZO octetos), o le dice que no hay más. Conserva los registros
png_mas:
        pushad
        cmp dword [png_sin_mas], 0
        jne .nada
.otra:
        cmp dword [png_resto], 0
        jne .da
        call png_fin_idat           ; (el IDAT que se acaba: su suma)
        mov eax, [png_pos]          ; la cabecera del trozo siguiente
        mov ecx, 8
        call mi_trozo
        jc .nada
        mov eax, [esi]
        call mi_be32
        mov ebx, eax                ; EBX=sus octetos
        mov edx, [esi+4]
        cmp edx, 'IEND'
        je .nada
        cmp edx, 'IDAT'
        je .idat
        call png_suma_trozo         ; otro que no es de datos: su suma, y al siguiente
        jc .nada
        jmp .otra
.idat:
        mov ecx, [im_tam]           ; (cortado: lo que haya)
        sub ecx, [png_pos]
        sub ecx, 8
        cmp ebx, ecx
        jbe .i1
        mov ebx, ecx
.i1:
        mov [png_resto], ebx
        add esi, 4                  ; su suma empieza por el nombre
        mov ecx, 4
        mov eax, 0xFFFFFFFF
        call png_crc
        mov [png_crc_v], eax
        mov dword [png_en_idat], 1
        add dword [png_pos], 8
        jmp .otra
.da:
        mov ecx, [png_resto]
        cmp ecx, PNG_TROZO
        jbe .d1
        mov ecx, PNG_TROZO
.d1:
        mov eax, [png_pos]
        call mi_trozo
        jc .nada                    ; (el disco falla)
        mov eax, [png_crc_v]
        call png_crc
        mov [png_crc_v], eax
        add [png_pos], ecx
        sub [png_resto], ecx
        call dst_entrada
        popad
        ret
.nada:
        mov dword [png_sin_mas], 1
        xor ecx, ecx
        call dst_entrada
        popad
        ret

; (interna) si se ha acabado de leer un IDAT, comprueba su suma (va detrás de
; sus datos) y deja [png_pos] en el trozo siguiente. Conserva los registros
png_fin_idat:
        cmp dword [png_en_idat], 0
        je .x
        cmp dword [png_resto], 0
        jne .x
        mov dword [png_en_idat], 0
        pushad
        mov eax, [png_pos]
        mov ecx, 4
        call mi_trozo
        jc .f1                      ; (cortado justo ahí: de eso ya se avisa)
        mov eax, [esi]
        call mi_be32
        mov edx, [png_crc_v]
        not edx
        cmp eax, edx
        je .f1
        mov dword [im_aviso], 1
.f1:
        add dword [png_pos], 4
        popad
.x:
        ret

; (interna) ya han salido todos los octetos de la imagen: lo que queda del
; flujo (su final y la suma de zlib) y de los trozos (sus sumas, hasta IEND).
; Que sobren datos comprimidos no importa. Conserva los registros
png_remata:
        pushad
        cmp dword [png_acabado], 0
        jne .trozos
        mov edi, png_sobra
        mov ecx, 1
        call png_infla
        cmp dword [png_acabado], 0
        je .x                       ; (sobran datos: se deja estar)
.trozos:
        cmp dword [png_en_idat], 0  ; (a medio IDAT: no se puede seguir la cuenta)
        jne .x
.t:
        mov eax, [png_pos]
        mov ecx, 8
        call mi_trozo
        jc .x
        cmp dword [esi+4], 'IEND'
        je .x
        call png_suma_trozo
        jnc .t
.x:
        popad
        ret

; (interna) deshace el filtro EAX de la fila ESI (ECX octetos), con la de
; encima en EDX. Conserva ESI y EDX. Un filtro que no existe: se deja como
; está y se avisa
png_filtro:
        test ecx, ecx
        jz .x
        cmp eax, 4
        jbe .f0
        mov dword [im_aviso], 1
        ret
.f0:
        test eax, eax
        jz .x
        push esi
        push edx
        mov ebx, [png_bpp]
        xor edi, edi                ; EDI=el octeto
        cmp eax, 2
        je .arriba
        cmp eax, 3
        je .media
        cmp eax, 4
        je .paeth
.izq:                               ; 1: más el de su izquierda
        cmp edi, ebx
        jb .i1
        mov ebp, esi
        sub ebp, ebx
        mov al, [ebp+edi]
        add [esi+edi], al
.i1:
        inc edi
        cmp edi, ecx
        jb .izq
        jmp .fin
.arriba:                            ; 2: más el de encima
        mov al, [edx+edi]
        add [esi+edi], al
        inc edi
        cmp edi, ecx
        jb .arriba
        jmp .fin
.media:                             ; 3: más la media de los dos
        xor eax, eax
        cmp edi, ebx
        jb .m1
        mov ebp, esi
        sub ebp, ebx
        movzx eax, byte [ebp+edi]
.m1:
        movzx ebp, byte [edx+edi]
        add eax, ebp
        shr eax, 1
        add [esi+edi], al
        inc edi
        cmp edi, ecx
        jb .media
        jmp .fin
.paeth:                             ; 4: más el que más se parezca a a + b - c
        push ecx
        xor eax, eax                ; EAX=a (izquierda) ECX=c (arriba a la izquierda)
        xor ecx, ecx
        cmp edi, ebx
        jb .p1
        mov ebp, esi
        sub ebp, ebx
        movzx eax, byte [ebp+edi]
        mov ebp, edx
        sub ebp, ebx
        movzx ecx, byte [ebp+edi]
.p1:
        movzx ebp, byte [edx+edi]   ; EBP=b (arriba)
        push ebx
        push edx
        push esi
        mov ebx, ebp                ; pa = |b - c|
        sub ebx, ecx
        mov edx, eax                ; pb = |a - c|
        sub edx, ecx
        lea esi, [ebx+edx]          ; pc = |a + b - 2c|
        test ebx, ebx
        jns .p2
        neg ebx
.p2:
        test edx, edx
        jns .p3
        neg edx
.p3:
        test esi, esi
        jns .p4
        neg esi
.p4:
        cmp ebx, edx                ; a, si pa <= pb y pa <= pc
        ja .p5
        cmp ebx, esi
        jbe .p7
.p5:
        mov eax, ebp                ; b, si pb <= pc
        cmp edx, esi
        jbe .p7
        mov eax, ecx                ; y si no, c
.p7:
        pop esi
        pop edx
        pop ebx
        pop ecx
        add [esi+edi], al
        inc edi
        cmp edi, ecx
        jb .paeth
.fin:
        pop edx
        pop esi
.x:
        ret

; (interna) ESI=una fila del archivo (ya sin filtro) ECX=sus puntos EDI=
; destino: las muestras, a un octeto cada una si son de menos (sin escalar);
; las de 8 y 16 bits se copian. Conserva EDI
png_expande:
        push edi
        mov eax, [png_prof]
        cmp eax, 8
        jae .copia
        mov edx, ecx
        mov cl, al
        call mi_desempaqueta
        pop edi
        ret
.copia:
        imul ecx, [png_bpp]
        rep movsb
        pop edi
        ret

png_fila:
        mov [png_y], eax
        mov [png_dest], edi
        cmp dword [png_entre], 0
        jne .adam
        ; ---- sin entrelazar: las filas se descomprimen según se piden (en
        ;      orden); cada una necesita la de encima, para su filtro ----
        cmp eax, [png_sig]
        jne .falta
        mov ecx, [png_po]
        inc ecx
        mov eax, [png_lista]        ; (la primera ya se descomprimió al preparar)
        cmp eax, -1
        jne .tengo
        mov edi, [png_act]
        call png_infla
.tengo:
        mov dword [png_lista], -1
        cmp eax, ecx
        je .entera
        mov dword [im_aviso], 1     ; a medias o ninguna: esta fila y las de debajo faltan
        mov dword [png_sig], -1
.falta:
        stc
        ret
.entera:
        inc dword [png_sig]
        mov esi, [png_act]
        movzx eax, byte [esi]
        inc esi
        mov ecx, [png_po]
        mov edx, [png_ant]
        call png_filtro             ; (conserva ESI y EDX)
        mov eax, [png_act]          ; la de ahora pasa a ser la de encima: se cambian
        inc eax                     ; los papeles de los dos búferes (la de encima
        dec edx                     ; apunta a los datos; la de ahora, a su octeto
        mov [png_ant], eax          ; de filtro, uno antes)
        mov [png_act], edx
        mov eax, [png_sig]          ; tras la última: lo que quede por comprobar
        cmp eax, [im_al]
        jb .f1
        call png_remata
.f1:
        cmp dword [png_prof], 8     ; de 8 o 16 bits: tal como está
        jae .convierte
        mov ecx, [im_an]
        mov edi, [png_r]
        call png_expande
        mov esi, edi
        jmp .convierte
.adam:
        ; de cada pasada que tenga esta fila, sus puntos, cada uno a su columna
        mov ebp, 1                  ; EBP=octetos de un punto desempaquetado
        cmp dword [png_prof], 8
        jb .a0
        mov ebp, [png_bpp]
.a0:
        xor ebx, ebx
.a1:
        cmp dword [png_pw+ebx*4], 0
        je .a9
        mov eax, [png_y]
        sub eax, [png_ys+ebx*4]
        jb .a9
        xor edx, edx
        div dword [png_dy+ebx*4]
        test edx, edx
        jnz .a9
        cmp eax, [png_ph+ebx*4]
        jae .a9
        mov ecx, [png_po+ebx*4]     ; EAX=la fila dentro de la pasada
        inc ecx
        imul eax, ecx
        mov esi, [png_b]
        add esi, [png_pd+ebx*4]
        lea esi, [esi+eax+1]
        mov ecx, [png_pw+ebx*4]
        mov edi, [png_e]
        push ebx
        call png_expande
        pop ebx
        mov esi, edi
        mov edi, [png_xs+ebx*4]
        imul edi, ebp
        add edi, [png_r]
        mov edx, [png_dx+ebx*4]
        dec edx
        imul edx, ebp               ; EDX=lo que se salta entre punto y punto
        mov eax, [png_pw+ebx*4]
.a2:
        mov ecx, ebp
        rep movsb
        add edi, edx
        dec eax
        jnz .a2
.a9:
        inc ebx
        cmp ebx, 7
        jb .a1
        mov esi, [png_r]
.convierte:
        ; ---- ESI=las muestras de la fila: a la clase del formato ----
        mov edi, [png_dest]
        mov ecx, [im_an]
        mov eax, [png_color]
        cmp eax, 3
        je .paleta
        cmp eax, 2
        je .color
        cmp eax, 6
        je .color_a
        cmp eax, 4
        je .gris_a
        ; ---- grises ----
        mov ebx, [png_prof]
        cmp ebx, 16
        je .g16
        mov edx, 1                  ; por cuánto se multiplica: 255, 85, 17 o 1
        cmp ebx, 8
        je .g1
        mov edx, 17
        cmp ebx, 4
        je .g1
        mov edx, 85
        cmp ebx, 2
        je .g1
        mov edx, 255
.g1:
        movzx eax, byte [esi]
        inc esi
        mov ebx, eax
        imul eax, edx
        stosb
        cmp dword [png_trns], 0
        je .g2
        cmp ebx, [png_clave]
        setne al
        neg al
        stosb
.g2:
        dec ecx
        jnz .g1
        clc
        ret
.g16:
        movzx eax, word [esi]
        add esi, 2
        stosb
        cmp dword [png_trns], 0
        je .g17
        xchg al, ah
        cmp eax, [png_clave]
        setne al
        neg al
        stosb
.g17:
        dec ecx
        jnz .g16
        clc
        ret
.paleta:
        rep movsb
        clc
        ret
.gris_a:
        cmp dword [png_prof], 16
        je .ga16
        add ecx, ecx
        rep movsb
        clc
        ret
.ga16:
        mov al, [esi]
        stosb
        mov al, [esi+2]
        stosb
        add esi, 4
        dec ecx
        jnz .ga16
        clc
        ret
.color_a:
        cmp dword [png_prof], 16
        je .ca16
        shl ecx, 2
        rep movsb
        clc
        ret
.ca16:
        mov al, [esi]
        stosb
        mov al, [esi+2]
        stosb
        mov al, [esi+4]
        stosb
        mov al, [esi+6]
        stosb
        add esi, 8
        dec ecx
        jnz .ca16
        clc
        ret
.color:
        cmp dword [png_prof], 16
        je .c16
        cmp dword [png_trns], 0
        jne .c8t
        lea ecx, [ecx+ecx*2]
        rep movsb
        clc
        ret
.c8t:
        xor edx, edx                ; ¿es el color transparente?
        movzx eax, byte [esi]
        cmp eax, [png_clave]
        jne .c8u
        movzx eax, byte [esi+1]
        cmp eax, [png_clave+4]
        jne .c8u
        movzx eax, byte [esi+2]
        cmp eax, [png_clave+8]
        jne .c8u
        inc edx
.c8u:
        movsb
        movsb
        movsb
        mov al, dl                  ; (0 si lo era; 255 si no)
        dec al
        stosb
        dec ecx
        jnz .c8t
        clc
        ret
.c16:
        xor edx, edx
        cmp dword [png_trns], 0
        je .c16u
        xor ebx, ebx
.c16t:
        movzx eax, word [esi+ebx*2]
        xchg al, ah
        cmp eax, [png_clave+ebx*4]
        jne .c16u
        inc ebx
        cmp ebx, 3
        jb .c16t
        inc edx
.c16u:
        mov al, [esi]
        stosb
        mov al, [esi+2]
        stosb
        mov al, [esi+4]
        stosb
        add esi, 6
        cmp dword [png_trns], 0
        je .c16v
        mov al, dl                  ; EDX=1 si era el transparente
        dec al
        stosb
.c16v:
        dec ecx
        jnz .c16
        clc
        ret
