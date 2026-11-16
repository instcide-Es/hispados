; =============================================================================
;  imagen.inc - Lector: las imágenes de un PDF, ya reducidas y en la paleta
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Al componer la página (contenido.inc), de cada imagen se hace una
;  «miniatura»: un mapa de un octeto por punto, ya con los colores de la
;  paleta de Hispa Ventanas (el cubo de 6x6x6 con tramado ordenado, o la
;  escala de grises) y reducida, si es grande, promediando bloques de puntos
;  (a no más de IM_LADO de lado). Queda en lo alto del almacén hasta la página
;  siguiente; quien pinta solo tiene que estirarla o encogerla a su sitio.
;
;  Sabe: DeviceGray, DeviceRGB, DeviceCMYK (y sus parientes CalGray, CalRGB e
;  ICCBased de 1, 3 o 4 componentes), Indexed sobre cualquiera de ellos, de
;  1, 2, 4, 8 y 16 bits; /Decode [1 0] en las de un componente; y las
;  máscaras (/ImageMask), que se pintan con el color de relleno y dejan ver
;  lo de debajo (octeto IM_NADA). Los datos, con los filtros de pdf.inc. Lo
;  demás (JPEG, JPEG 2000, fax, JBIG2, Separation...) no: CF=1 y la imagen
;  se queda en su recuadro gris.
;
;    im_hace   ESI, EBP=el diccionario de la imagen; [im_dat] y [im_ndat]=sus
;              datos ya sin filtros; AL=el color de relleno (para máscaras)
;              -> EAX=la miniatura (dd ancho, dd alto y los puntos); CF=1 si
;              no se sabe o no cabe
;    im_color  EAX=rojo EBX=verde ECX=azul (0-255) ESI=x EDI=y del punto (para
;              el tramado) -> AL=color de la paleta
; =============================================================================
IM_LADO     equ 640                 ; lo más que mide una miniatura, de lado
IM_NADA     equ 0xFF                ; un punto que deja ver lo de debajo

%macro IMAGEN_BSS 0
            alignb 4
im_dat      resd 1
im_ndat     resd 1
im_dic      resd 2
im_w        resd 1
im_h        resd 1
im_bpc      resd 1
im_nc       resd 1                  ; componentes por punto (del espacio base, si es Indexed)
im_fila     resd 1                  ; octetos de una fila
im_ind      resd 1                  ; 1: Indexed
im_mask     resd 1
im_inv      resd 1
im_paso     resd 1
im_tw       resd 1
im_th       resd 1
im_relleno  resd 1
im_sum      resd 4
im_x        resd 1
im_y        resd 1
im_prof     resd 1
im_res      resd 2                  ; los recursos (para espacios de color con nombre)
im_pal      resb 1024 + 8           ; la tabla de un Indexed
im_tmp      resb 40
%endmacro

k_width     db "Width", 0
k_w1        db "W", 0
k_height    db "Height", 0
k_h1        db "H", 0
k_bpc1      db "BPC", 0
k_cspace    db "ColorSpace", 0
k_cs1       db "CS", 0
k_imask     db "ImageMask", 0
k_im1       db "IM", 0
k_decode    db "Decode", 0
k_d1        db "D", 0
n_true      db "true", 0
; los espacios de color: nombre y componentes (0: Indexed; -1: no se sabe)
im_espacios db "DeviceGray", 0, 1, "G", 0, 1, "CalGray", 0, 1, "DeviceRGB", 0, 3, "RGB", 0, 3, "CalRGB", 0, 3
            db "Lab", 0, 3, "DeviceCMYK", 0, 4, "CMYK", 0, 4, "Indexed", 0, 0, "I", 0, 0, "ICCBased", 0, 5, 0
; el tramado ordenado de 4x4
im_bayer    db 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5

; (interna) la clave EDI o, si no está, la EBX, del diccionario de la imagen
; -> ESI, EBP en su valor; CF=1 si no está ninguna
im_clave:
        mov esi, [im_dic]
        mov ebp, [im_dic+4]
        call px_dic
        jnc .si
        mov edi, ebx
        mov esi, [im_dic]
        call px_dic
        jc .x
.si:
        call px_ref
        clc
.x:
        ret

; (interna) ESI, EBP=un espacio de color -> [im_nc], [im_ind] (y im_pal); CF=1
; si no se sabe
im_espacio:
        inc dword [im_prof]
        cmp dword [im_prof], 4
        ja .no
        call px_blancos
        cmp esi, ebp
        jae .no
        cmp byte [esi], '['
        jne .nombre
        inc esi
        call px_blancos
.nombre:
        cmp esi, ebp
        jae .no
        cmp byte [esi], '/'
        jne .no
        mov ebx, im_espacios
.b:
        cmp byte [ebx], 0
        je .recurso
        mov edi, ebx
        call px_es_nombre
        pushf
.s:
        inc ebx
        cmp byte [ebx-1], 0
        jne .s
        popf
        je .es
        inc ebx
        jmp .b
.es:
        movzx eax, byte [ebx]
        test eax, eax
        jz .indexado
        cmp eax, 5
        je .icc
        mov [im_nc], eax
        jmp .si
.icc:
        ; [/ICCBased flujo]: los componentes, en /N del flujo
        call px_salta
        call px_blancos
        call px_ref
        mov edi, k_n
        call px_dic
        jc .no
        call px_ref
        call px_entero
        jc .no
        cmp eax, 1
        je .icc1
        cmp eax, 3
        je .icc1
        cmp eax, 4
        jne .no
.icc1:
        mov [im_nc], eax
        jmp .si
.indexado:
        ; [/Indexed base mayor tabla]
        cmp dword [im_ind], 0
        jne .no
        call px_salta
        call px_blancos
        push esi
        push ebp
        call px_ref
        call im_espacio             ; la base (deja im_nc)
        pop ebp
        pop esi
        jc .no
        cmp dword [im_ind], 0
        jne .no
        mov dword [im_ind], 1
        call px_salta_valor
        call px_blancos
        call px_salta_valor         ; el mayor índice (no hace falta: se mira la tabla)
        call px_blancos
        call px_ref
        cmp esi, ebp
        jae .no
        push edi
        mov edi, im_pal             ; la tabla, a cero y luego lo que traiga
        mov ecx, 256
        xor eax, eax
        rep stosd
        pop edi
        mov al, [esi]
        cmp al, '('
        je .cad
        cmp al, '<'
        jne .no
        lea eax, [esi+1]
        cmp eax, ebp
        jae .no
        cmp byte [eax], '<'
        je .flujo
.cad:
        mov edi, im_pal
        mov ecx, 1024
        call px_cadena
        jmp .si
.flujo:
        call ar_marca
        push eax
        call pdf_flujo
        jc .f_no
        cmp ecx, 1024
        jbe .f1
        mov ecx, 1024
.f1:
        mov esi, eax
        mov edi, im_pal
        rep movsb
        pop eax
        call ar_suelta
        jmp .si
.f_no:
        pop eax
        call ar_suelta
        jmp .no
.recurso:
        ; un nombre propio: el espacio de color de ese nombre en los recursos
        mov edi, im_tmp
        mov ecx, 36
        call px_copia_nombre
        mov esi, [im_res]
        mov ebp, [im_res+4]
        test esi, esi
        jz .no
        mov edi, k_cspace
        call px_dic
        jc .no
        call px_ref
        mov edi, im_tmp
        call px_dic
        jc .no
        call px_ref
        call im_espacio
        jc .no
.si:
        dec dword [im_prof]
        clc
        ret
.no:
        dec dword [im_prof]
        stc
        ret
; (interna) ECX=x EDX=y -> EAX=rojo EBX=verde ECX=azul (0-255); en una
; máscara, EAX=255 si ese punto se pinta y 0 si no
im_punto:
        push edx
        push esi
        push edi
        mov eax, edx
        mul dword [im_fila]
        mov esi, eax
        add eax, [im_fila]
        cmp eax, [im_ndat]
        ja .fuera                   ; (datos cortados: en blanco)
        add esi, [im_dat]
        mov eax, ecx
        mov edi, [im_nc]
        cmp dword [im_ind], 0
        je .c0
        mov edi, 1
.c0:
        mul edi
        mov edx, eax                ; EDX=la primera muestra del punto
        xor ecx, ecx                ; ECX=el componente
.comp:
        push ecx
        push edx
        add edx, ecx
        mov ecx, [im_bpc]
        cmp ecx, 8
        jb .bits
        je .ocho
        movzx eax, byte [esi+edx*2] ; 16 bits: el octeto alto
        jmp .v
.ocho:
        movzx eax, byte [esi+edx]
        jmp .v
.bits:
        mov eax, edx
        mul ecx                     ; el bit donde empieza
        mov edx, eax
        shr edx, 3
        movzx edx, byte [esi+edx]
        and eax, 7
        add eax, ecx
        mov edi, ecx
        mov ecx, 8
        sub ecx, eax
        shr edx, cl                 ; la muestra, abajo
        mov ecx, edi
        mov eax, 1
        shl eax, cl
        dec eax
        and eax, edx
        cmp dword [im_ind], 0
        jne .v                      ; (un índice no se estira)
        cmp ecx, 1                  ; a 0-255: x 255, x 85 o x 17
        jne .b2
        imul eax, 255
        jmp .v
.b2:
        cmp ecx, 2
        jne .b4
        imul eax, 85
        jmp .v
.b4:
        imul eax, 17
.v:
        pop edx
        pop ecx
        mov [im_sum+ecx*4], eax
        inc ecx
        cmp dword [im_ind], 0
        jne .indice
        cmp ecx, [im_nc]
        jb .comp
        mov esi, im_sum
        jmp .color
.indice:
        ; el índice, a su entrada de la tabla
        mul dword [im_nc]
        cmp eax, 1020
        jbe .i1
        mov eax, 1020
.i1:
        lea esi, [im_pal+eax]
        movzx eax, byte [esi]
        movzx ebx, byte [esi+1]
        movzx ecx, byte [esi+2]
        movzx edx, byte [esi+3]
        mov esi, im_sum
        mov [esi], eax
        mov [esi+4], ebx
        mov [esi+8], ecx
        mov [esi+12], edx
.color:
        mov eax, [esi]
        mov edi, [im_nc]
        cmp edi, 3
        je .rgb
        ja .cmyk
        cmp dword [im_inv], 0       ; un componente: gris (o máscara)
        je .g1
        xor eax, 255
.g1:
        cmp dword [im_mask], 0
        je .g2
        xor eax, 255                ; en una máscara se pinta donde hay 0
.g2:
        mov ebx, eax
        mov ecx, eax
        jmp .x
.rgb:
        mov ebx, [esi+4]
        mov ecx, [esi+8]
        jmp .x
.cmyk:
        mov edi, 255                ; rojo = (255 - cian)(255 - negro) / 255...
        sub edi, [esi+12]
        mov eax, 255
        sub eax, [esi+8]
        imul eax, edi
        mov ecx, 255
        xor edx, edx
        div ecx
        push eax                    ; azul
        mov eax, 255
        sub eax, [esi+4]
        imul eax, edi
        xor edx, edx
        div ecx
        mov ebx, eax                ; verde
        mov eax, 255
        sub eax, [esi]
        imul eax, edi
        xor edx, edx
        div ecx
        pop ecx
        jmp .x
.fuera:
        mov eax, 255
        cmp dword [im_mask], 0
        je .f1
        xor eax, eax
.f1:
        mov ebx, eax
        mov ecx, eax
.x:
        pop edi
        pop esi
        pop edx
        ret

im_color:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov edx, eax                ; ¿un gris? (el mayor y el menor, a menos de 11)
        cmp ebx, edx
        jbe .m1
        mov edx, ebx
.m1:
        cmp ecx, edx
        jbe .m2
        mov edx, ecx
.m2:
        push edx
        mov edx, eax
        cmp ebx, edx
        jae .m3
        mov edx, ebx
.m3:
        cmp ecx, edx
        jae .m4
        mov edx, ecx
.m4:
        neg edx
        add edx, [esp]
        add esp, 4
        cmp edx, 10
        ja .cubo
        add eax, ebx
        add eax, ecx
        xor edx, edx
        mov ebx, 3
        div ebx
        cmp eax, 248
        jae .blanco
        cmp eax, 5
        jbe .negro
        add eax, 5
        xor edx, edx
        mov ebx, 11
        div ebx
        cmp eax, 23
        jbe .g1
        mov eax, 23
.g1:
        add eax, 232
        jmp .x
.blanco:
        mov eax, 231
        jmp .x
.negro:
        mov eax, 16
        jmp .x
.cubo:
        ; cada componente, de 0 a 5; el umbral del redondeo, según el sitio
        and esi, 3
        and edi, 3
        movzx esi, byte [im_bayer+edi*4+esi]
        imul esi, 255
        shr esi, 4
        add esi, 8
        mov edi, 255
        lea eax, [eax+eax*4]
        add eax, esi
        xor edx, edx
        div edi
        push eax
        lea eax, [ebx+ebx*4]
        add eax, esi
        xor edx, edx
        div edi
        mov ebx, eax
        lea eax, [ecx+ecx*4]
        add eax, esi
        xor edx, edx
        div edi
        mov ecx, eax
        pop eax
        imul eax, 36
        imul ebx, 6
        add eax, ebx
        add eax, ecx
        add eax, 16
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

im_hace:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        movzx eax, al
        mov [im_relleno], eax
        mov [im_dic], esi
        mov [im_dic+4], ebp
        xor eax, eax
        mov [im_ind], eax
        mov [im_mask], eax
        mov [im_inv], eax
        mov [im_prof], eax
        mov edi, k_width
        mov ebx, k_w1
        call im_clave
        jc .no
        call px_entero
        jc .no
        mov [im_w], eax
        mov edi, k_height
        mov ebx, k_h1
        call im_clave
        jc .no
        call px_entero
        jc .no
        mov [im_h], eax
        cmp dword [im_w], 1
        jl .no
        cmp dword [im_h], 1
        jl .no
        cmp dword [im_w], 20000
        ja .no
        cmp dword [im_h], 20000
        ja .no
        mov dword [im_bpc], 8
        mov edi, k_bpc
        mov ebx, k_bpc1
        call im_clave
        jc .b1
        call px_entero
        jc .b1
        mov [im_bpc], eax
.b1:
        mov edi, k_imask
        mov ebx, k_im1
        call im_clave
        jc .m1
        mov edi, n_true
        call px_es_palabra
        jne .m1
        mov dword [im_mask], 1
        mov dword [im_bpc], 1
        mov dword [im_nc], 1
        jmp .dec
.m1:
        mov edi, k_cspace
        mov ebx, k_cs1
        call im_clave
        jc .no
        call im_espacio
        jc .no
.dec:
        mov eax, [im_bpc]
        cmp eax, 1
        je .bpc_ok
        cmp eax, 2
        je .bpc_ok
        cmp eax, 4
        je .bpc_ok
        cmp eax, 8
        je .bpc_ok
        cmp eax, 16
        jne .no
.bpc_ok:
        ; /Decode [1 0] en un componente: al revés
        mov edi, k_decode
        mov ebx, k_d1
        call im_clave
        jc .d1
        cmp esi, ebp
        jae .d1
        cmp byte [esi], '['
        jne .d1
        inc esi
        call px_blancos
        call px_num
        jc .d1
        cmp eax, 0x8000
        jl .d1
        mov dword [im_inv], 1
.d1:
        mov eax, [im_nc]            ; los octetos de una fila
        cmp dword [im_ind], 0
        je .f0
        mov eax, 1
.f0:
        imul eax, [im_bpc]
        mul dword [im_w]
        add eax, 7
        shr eax, 3
        mov [im_fila], eax
        ; cuánto se reduce: que ningún lado pase de IM_LADO
        mov eax, [im_w]
        cmp eax, [im_h]
        jae .p0
        mov eax, [im_h]
.p0:
        add eax, IM_LADO - 1
        xor edx, edx
        mov ebx, IM_LADO
        div ebx
        mov [im_paso], eax
        mov ebx, eax
        mov eax, [im_w]
        add eax, ebx
        dec eax
        xor edx, edx
        div ebx
        mov [im_tw], eax
        mov eax, [im_h]
        add eax, ebx
        dec eax
        xor edx, edx
        div ebx
        mov [im_th], eax
        ; el sitio, en lo alto del almacén
        mul dword [im_tw]
        lea ecx, [eax+8+3]
        and ecx, ~3
        mov eax, [ar_fin]
        sub eax, ecx
        cmp eax, [ar_tope]
        jb .no
        mov [ar_fin], eax
        mov edi, eax
        push eax
        mov eax, [im_tw]
        stosd
        mov eax, [im_th]
        stosd
        ; cada punto de la miniatura: la media de su bloque
        mov dword [im_y], 0
.fy:
        mov dword [im_x], 0
.fx:
        push edi
        xor esi, esi                ; ESI, EDI, EBP=sumas de rojo, verde y azul
        xor edi, edi
        xor ebp, ebp
        push dword 0                ; cuántos puntos
        mov edx, [im_y]
        imul edx, [im_paso]
        mov ebx, [im_paso]          ; EBX=filas que quedan del bloque
.by:
        cmp edx, [im_h]
        jae .b_fin
        mov ecx, [im_x]
        imul ecx, [im_paso]
        push ebx
        mov ebx, [im_paso]
.bx:
        cmp ecx, [im_w]
        jae .bx_fin
        push ebx
        push ecx
        call im_punto
        add esi, eax
        add edi, ebx
        add ebp, ecx
        pop ecx
        pop ebx
        inc dword [esp+4]
        inc ecx
        dec ebx
        jnz .bx
.bx_fin:
        pop ebx
        inc edx
        dec ebx
        jnz .by
.b_fin:
        pop ebx                     ; EBX=cuántos
        test ebx, ebx
        jnz .b1x
        inc ebx
.b1x:
        mov eax, ebp
        xor edx, edx
        div ebx
        mov ecx, eax
        mov eax, edi
        xor edx, edx
        div ebx
        push eax
        mov eax, esi
        xor edx, edx
        div ebx
        pop ebx
        pop edi
        cmp dword [im_mask], 0
        je .col
        cmp eax, 128                ; máscara: pintado o no
        mov al, IM_NADA
        jb .pon
        mov al, [im_relleno]
        jmp .pon
.col:
        push esi
        push edi
        mov esi, [im_x]
        mov edi, [im_y]
        call im_color
        pop edi
        pop esi
        cmp al, IM_NADA
        jne .pon
        dec al
.pon:
        stosb
        inc dword [im_x]
        mov eax, [im_x]
        cmp eax, [im_tw]
        jb .fx
        inc dword [im_y]
        mov eax, [im_y]
        cmp eax, [im_th]
        jb .fy
        pop eax
        clc
        jmp .x
.no:
        xor eax, eax
        stc
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
