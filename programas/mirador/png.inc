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
;  Cómo: los trozos IDAT se juntan en la memoria de trabajo y se descomprimen
;  de una vez con programas/comun/desinfla.inc (que trabaja de memoria a
;  memoria); después se deshacen los filtros de todas las filas, en el sitio.
;  Una fila se entrega desempaquetando la suya; con Adam7, juntando los
;  puntos que le tocan de cada una de las siete pasadas. Por eso un PNG
;  necesita en memoria la imagen entera como va en el archivo (de uno a ocho
;  octetos por punto), además de sus datos comprimidos.
;  Se comprueban la suma (CRC-32) de cada trozo y la de zlib: si alguna no
;  cuadra o los datos se acaban antes, se enseña lo que haya y se avisa.
; =============================================================================
%macro PNG_BSS 0
            alignb 4
png_dat     resd 1
png_fin     resd 1
png_prof    resd 1                  ; bits por muestra
png_color   resd 1                  ; clase de color (0, 2, 3, 4, 6)
png_entre   resd 1                  ; con Adam7
png_bpp     resd 1                  ; octetos por punto, para los filtros (1 al menos)
png_bitsp   resd 1                  ; bits por punto
png_idat    resd 1                  ; octetos de todos los IDAT
png_crudo   resd 1                  ; octetos de la imagen sin comprimir
png_validos resd 1                  ; los que de verdad han salido
png_filas   resd 1                  ; (sin Adam7) filas enteras que han salido
png_b       resd 1                  ; la imagen sin comprimir
png_r       resd 1                  ; una fila con los puntos desempaquetados
png_e       resd 1                  ; lo mismo, de una pasada (y una fila de ceros)
png_npas    resd 1
png_trns    resd 1                  ; hay un color transparente
png_clave   resd 3                  ; cuál (16 bits por componente)
png_y       resd 1
png_dest    resd 1
; de cada pasada: dónde empieza, ancho, alto y octetos por fila (sin el del filtro)
png_pd      resd 7
png_pw      resd 7
png_ph      resd 7
png_po      resd 7
png_crc_t   resd 256
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

; (interna) recorre los trozos desde el que sigue a IHDR y llama a EBX con
; cada uno: ESI=sus datos ECX=cuántos (los que haya en el archivo) EAX=su
; nombre. Se para en IEND o donde se acabe el archivo
png_trozos:
        mov edx, [png_dat]
        add edx, 33
.t:
        mov eax, [png_fin]
        sub eax, edx
        cmp eax, 8
        jb .x
        sub eax, 8                  ; EAX=lo que queda detrás de su cabecera
        push eax
        mov eax, [edx]
        call mi_be32
        mov ecx, eax
        pop eax
        cmp ecx, eax
        jbe .t1
        mov ecx, eax                ; (cortado: lo que haya)
.t1:
        mov eax, [edx+4]
        cmp eax, 'IEND'
        je .x
        lea esi, [edx+8]
        push ecx
        push edx
        push ebx
        call ebx
        pop ebx
        pop edx
        pop ecx
        add edx, ecx
        add edx, 12
        cmp edx, [png_fin]          ; (si no cabe ni la suma, el siguiente no existe)
        jb .t
.x:
        ret

png_abre:
        mov [png_dat], esi
        lea eax, [esi+ecx]
        mov [png_fin], eax
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
        call png_sumas
        ; los trozos: la paleta, la transparencia y cuánto ocupan los datos
        xor eax, eax
        mov [png_idat], eax
        mov [png_trns], eax
        mov ebx, .trozo
        call png_trozos
        cmp dword [png_idat], 0
        je .roto
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
        mov eax, [im_an]
        shl eax, 4                  ; dos filas de hasta ocho octetos por punto
        add eax, 64
        add eax, edi
        add eax, [png_idat]
        mov [im_alm], eax
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
; (interna) un trozo, al abrir
.trozo:
        cmp eax, 'IDAT'
        jne .t_plte
        add [png_idat], ecx
        ret
.t_plte:
        cmp eax, 'PLTE'
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
        cmp eax, 'tRNS'
        jne .t_x
        mov eax, [png_color]
        cmp eax, 3
        jne .tt1
        cmp ecx, 256                ; con paleta: la opacidad de cada color
        jbe .tt0
        mov ecx, 256
.tt0:
        xor eax, eax
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

; comprueba la suma (CRC-32) de cada trozo que esté entero: si alguna no
; cuadra el archivo se ha estropeado por el camino; se ve igual, con aviso
png_sumas:
        pushad
        xor ebx, ebx                ; la tabla del CRC-32 (polinomio EDB88320)
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
        mov esi, [png_dat]
        add esi, 8
.c:
        mov eax, [png_fin]
        sub eax, esi
        cmp eax, 12
        jb .x
        sub eax, 12
        push eax
        mov eax, [esi]
        call mi_be32
        mov ecx, eax
        pop eax
        cmp ecx, eax                ; (cortado: de eso ya se avisa en otro sitio)
        ja .x
        mov edx, [esi+4]            ; EDX=su nombre
        add ecx, 4                  ; la suma es del nombre y los datos
        add esi, 4
        mov eax, 0xFFFFFFFF
        xor ebx, ebx
.b:
        mov bl, [esi]
        inc esi
        xor bl, al
        shr eax, 8
        xor eax, [png_crc_t+ebx*4]
        dec ecx
        jnz .b
        not eax
        mov ebx, eax
        mov eax, [esi]
        call mi_be32
        add esi, 4
        cmp eax, ebx
        je .c1
        mov dword [im_aviso], 1
.c1:
        cmp edx, 'IEND'
        jne .c
.x:
        popad
        ret

png_prepara:
        ; el reparto de la memoria: los IDAT, la imagen y dos filas
        mov edi, [im_almp]
        mov eax, edi
        add eax, [png_idat]
        mov [png_b], eax
        add eax, [png_crudo]
        add eax, 3
        and eax, ~3
        mov [png_r], eax
        mov edx, [im_an]
        lea eax, [eax+edx*8+16]
        mov [png_e], eax
        mov ebx, .copia             ; los IDAT, uno detrás de otro
        call png_trozos
        mov esi, [im_almp]
        mov ecx, [png_idat]
        mov edi, [png_b]
        mov edx, [png_crudo]
        call desinfla_zlib
        jnc .d1
        cmp dword [dsf_error], DSF_E_LLENO  ; (que sobren datos no importa)
        je .d2
        mov dword [im_aviso], 1
        jmp .d2
.d1:
        cmp dword [dsf_suma], 0     ; la suma de zlib no cuadra: puede haber basura
        je .d2
        mov dword [im_aviso], 1
.d2:
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
.d3:
        mov eax, [png_validos]      ; (sin Adam7) las filas que han salido enteras
        xor edx, edx
        mov ecx, [png_po]
        inc ecx
        div ecx
        mov [png_filas], eax
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
; (interna) un trozo, al preparar: los IDAT, a la memoria (EDI avanza: lo
; conserva png_trozos porque no lo toca)
.copia:
        cmp eax, 'IDAT'
        jne .c9
        rep movsb
.c9:
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
        cmp eax, [png_filas]
        jae .falta
        mov ecx, [png_po]
        inc ecx
        imul eax, ecx
        mov esi, [png_b]
        lea esi, [esi+eax+1]
        cmp dword [png_prof], 8     ; de 8 o 16 bits: tal como está
        jae .convierte
        mov ecx, [im_an]
        mov edi, [png_r]
        call png_expande
        mov esi, edi
        jmp .convierte
.falta:
        stc
        ret
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
