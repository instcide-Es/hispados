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
;  1, 2, 4, 8 y 16 bits; /Decode al revés ([1 0 ...]); las máscaras
;  (/ImageMask), que se pintan con el color de relleno y dejan ver lo de
;  debajo (octeto IM_NADA); y la transparencia de una imagen, a todo o nada:
;  /SMask (se ve donde la opacidad pasa de la mitad), /Mask con otra imagen
;  y /Mask con los colores que no se pintan. Los datos, con los filtros de
;  pdf.inc.
;
;  Las JPEG (/DCTDecode) se descodifican con programas/comun/jpeg.inc, fila a
;  fila y ya reducidas al vuelo (1/2, 1/4, 1/8) si son grandes; lo que sale
;  se encoge a la medida de la miniatura promediando por superficie (cada
;  punto pesa lo que pisa). Su miniatura mide según lo que la imagen ocupe en
;  la página (dos puntos por punto tipográfico de su lado largo, hasta
;  IM_LADO_J: una página escaneada se lee), y menos si no hay memoria.
;  Valen las grises, las de color (YCbCr o RGB, según su APP14 de Adobe o el
;  /ColorTransform), las CMYK e YCCK (con su /Decode, que es quien dice si
;  van al revés) y las progresivas.
;
;  Lo demás (JPEG aritmético o de 12 bits, JPEG 2000, fax, JBIG2...) no: la
;  imagen se queda en su recuadro gris y en im_que queda el nombre de lo que
;  no se ha sabido, para el aviso.
;
;    im_xobjeto  ESI, EBP=una imagen (objeto con flujo); AL=el color de relleno
;              (para máscaras); [im_res]=los recursos; [im_lado]=lo que conviene
;              que mida su miniatura si es una JPEG -> EAX=la miniatura (dd
;              ancho, dd alto y los puntos); CF=1 y EAX=0 si no se sabe o no cabe
;    im_incrustada  lo mismo para una imagen metida en el contenido, con
;              filtros: ESI, EBP=su diccionario; [fl_p], [fl_n]=sus datos
;    im_hace   ESI, EBP=el diccionario de la imagen; [im_dat] y [im_ndat]=sus
;              datos ya sin filtros; AL=el color de relleno
;              -> EAX=la miniatura; CF=1 si no se sabe o no cabe
;    im_color  EAX=rojo EBX=verde ECX=azul (0-255) ESI=x EDI=y del punto (para
;              el tramado) -> AL=color de la paleta
; =============================================================================
IM_LADO     equ 640                 ; lo más que mide una miniatura, de lado
IM_LADO_J   equ 1280                ; y la de una JPEG que ocupe mucha página
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
im_lado     resd 1                  ; JPEG: lo que conviene que mida la miniatura
im_l        resd 1
im_obj      resd 2
im_mini     resd 1
im_alfa     resd 1                  ; 1: se guarda la opacidad, no el color (/SMask)
im_transp   resd 1                  ; puntos del bloque que son de un color que no se pinta
im_ck_n     resd 1                  ; /Mask [...]: de cuántos componentes, y entre qué valores
im_ck       resd 8
im_jopc     resd 1
im_jr       resd 1
im_jnc      resd 1
im_sw       resd 1                  ; lo que mide la JPEG ya reducida al vuelo
im_sh       resd 1
im_mtam     resd 1
im_mini_p   resd 1
im_fin0     resd 1
im_fin_aux  resd 1
im_hfila    resd 1                  ; una fila ya encogida a lo ancho
im_vacc     resd 1                  ; las sumas de la fila de la miniatura que se va llenando
im_vcap     resd 1
im_va       resd 1
im_dy       resd 1
im_ap       resd 1
im_hacc     resd 4
im_pal      resb 1024 + 8           ; la tabla de un Indexed
im_tmp      resb 40
im_que      resb 24                 ; el formato de la primera imagen que no se ha sabido
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
k_smask     db "SMask", 0
k_mask      db "Mask", 0
n_true      db "true", 0
; los espacios de color: nombre y componentes (0: Indexed; -1: no se sabe)
im_espacios db "DeviceGray", 0, 1, "G", 0, 1, "CalGray", 0, 1, "DeviceRGB", 0, 3, "RGB", 0, 3, "CalRGB", 0, 3
            db "Lab", 0, 3, "DeviceCMYK", 0, 4, "CMYK", 0, 4, "Indexed", 0, 0, "I", 0, 0, "ICCBased", 0, 5, 0
; el tramado ordenado de 4x4
im_bayer    db 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5
; los filtros de imagen que no se saben deshacer: su nombre y cómo se dice
im_filtros  db "JPXDecode", 0, "JPEG 2000", 0, "CCITTFaxDecode", 0, "fax CCITT", 0, "CCF", 0, "fax CCITT", 0
            db "JBIG2Decode", 0, "JBIG2", 0, "DCTDecode", 0, "JPEG", 0, "DCT", 0, "JPEG", 0, 0
; y las JPEG que no: por su error (JPEG_E_*)
im_jpeg_no  dd im_j_rota, im_j_rota, im_j_rota, im_j_rota, im_j_12, im_j_arit, im_j_sinp, im_j_jer
im_j_rota   db "JPEG dañado", 0
im_j_12     db "JPEG de 12 bits", 0
im_j_arit   db "JPEG aritmético", 0
im_j_sinp   db "JPEG sin pérdidas", 0
im_j_jer    db "JPEG jerárquico", 0

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
        jne .hecho
        cmp ecx, [im_nc]
        jb .comp
.hecho:
        ; ¿es de un color de los que no se pintan? (/Mask [...])
        cmp dword [im_ck_n], 0
        je .color
        xor ecx, ecx
.ck:
        mov eax, [im_sum+ecx*4]
        cmp eax, [im_ck+ecx*8]
        jb .color
        cmp eax, [im_ck+ecx*8+4]
        ja .color
        inc ecx
        cmp ecx, [im_ck_n]
        jb .ck
        inc dword [im_transp]
.color:
        call im_rgb
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

; (interna) las muestras de un punto, en im_sum (de 0 a 255; si es Indexed, su
; índice) -> EAX=rojo EBX=verde ECX=azul; en una máscara, EAX=255 si se pinta
im_rgb:
        push edx
        push esi
        push edi
        mov esi, im_sum
        cmp dword [im_ind], 0
        je .directo
        mov eax, [esi]              ; el índice, a su entrada de la tabla
        mul dword [im_nc]
        cmp eax, 1020
        jbe .i1
        mov eax, 1020
.i1:
        lea edi, [im_pal+eax]
        movzx eax, byte [edi]
        mov [esi], eax
        movzx eax, byte [edi+1]
        mov [esi+4], eax
        movzx eax, byte [edi+2]
        mov [esi+8], eax
        movzx eax, byte [edi+3]
        mov [esi+12], eax
        jmp .color
.directo:
        cmp dword [im_inv], 0       ; /Decode al revés, con varios componentes
        je .color
        cmp dword [im_nc], 1
        je .color
        xor dword [esi], 255
        xor dword [esi+4], 255
        xor dword [esi+8], 255
        xor dword [esi+12], 255
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
        mov edx, eax                ; ¿un gris? (el mayor y el menor, a 20 o menos: el
                                    ; papel de un escaneo, que nunca es blanco del todo,
                                    ; sale más limpio en gris que tramado con el cubo)
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
        cmp edx, 20
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
        mov [im_ck_n], eax
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
        call im_mira_decode
        ; /Mask [mín máx ...]: los colores que no se pintan (tal como vienen
        ; en los datos; aquí las muestras de menos de 8 bits van ya estiradas)
        cmp dword [im_mask], 0
        jne .ck_no
        mov edi, k_mask
        mov ebx, k_mask
        call im_clave
        jc .ck_no
        cmp esi, ebp
        jae .ck_no
        cmp byte [esi], '['
        jne .ck_no
        inc esi
        mov ebx, [im_nc]
        cmp dword [im_ind], 0
        je .ck0
        mov ebx, 1
.ck0:
        add ebx, ebx                ; EBX=cuántos números
        xor ecx, ecx
.ck1:
        call px_blancos
        call px_entero
        jc .ck_no
        cmp dword [im_ind], 0
        jne .ck3
        mov edx, [im_bpc]
        cmp edx, 8
        je .ck3
        ja .ck16
        cmp edx, 1
        jne .ck2a
        imul eax, 255
        jmp .ck3
.ck2a:
        cmp edx, 2
        jne .ck2b
        imul eax, 85
        jmp .ck3
.ck2b:
        imul eax, 17
        jmp .ck3
.ck16:
        shr eax, 8
.ck3:
        mov [im_ck+ecx*4], eax
        inc ecx
        cmp ecx, ebx
        jb .ck1
        shr ebx, 1
        mov [im_ck_n], ebx
.ck_no:
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
        mov dword [im_transp], 0
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
        mov eax, [im_transp]        ; la mitad o más, de un color que no se pinta
        test eax, eax
        jz .b2x
        add eax, eax
        cmp eax, ebx
        jb .b2x
        pop edi
        mov al, IM_NADA
        jmp .pon
.b2x:
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
        cmp dword [im_alfa], 0      ; (de una /SMask se guarda la opacidad, tal cual)
        jne .pon
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

; (interna) /Decode [1 0 ...]: los valores van al revés -> [im_inv]
im_mira_decode:
        push eax
        push ebx
        push esi
        push edi
        push ebp
        mov edi, k_decode
        mov ebx, k_d1
        call im_clave
        jc .x
        cmp esi, ebp
        jae .x
        cmp byte [esi], '['
        jne .x
        inc esi
        call px_blancos
        call px_num
        jc .x
        cmp eax, 0x8000
        jl .x
        mov dword [im_inv], 1
.x:
        pop ebp
        pop edi
        pop esi
        pop ebx
        pop eax
        ret

; (interna) ESI=cómo se llama lo que no se ha sabido pintar: si es la primera
; imagen de la página que no se sabe, queda en im_que para el aviso
im_pon_que:
        push eax
        push ecx
        push esi
        push edi
        cmp byte [im_que], 0
        jne .x
        mov edi, im_que
        mov ecx, 23
.c:
        lodsb
        test al, al
        jz .f
        stosb
        dec ecx
        jnz .c
.f:
        mov byte [edi], 0
.x:
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; -----------------------------------------------------------------------------
;  Una imagen entera: sus datos, su miniatura y su transparencia
; -----------------------------------------------------------------------------
im_xobjeto:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov [im_obj], esi
        mov [im_obj+4], ebp
        mov dword [im_alfa], 0
        call im_flujo
        jc .no
        mov [im_mini], eax
        call im_mascaras
        mov eax, [im_mini]
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

; (interna) ESI, EBP=una imagen (objeto con flujo) AL=relleno -> EAX=su
; miniatura; CF=1 si no se sabe (y, si es por el formato, queda dicho cuál)
im_flujo:
        push eax
        mov dword [fl_quiere_dct], 1
        push esi
        push ebp
        call pdf_flujo
        pop ebp
        pop esi
        mov dword [fl_quiere_dct], 0
        jc im_sin_flujo
im_sigue:
        mov [im_dat], eax
        mov [im_ndat], ecx
        pop eax
        cmp dword [fl_dct], 0
        jne im_hace_jpeg
        jmp im_hace
im_sin_flujo:
        pop eax
        push esi
        push edi
        mov eax, [fl_error]
        cmp eax, FE_MEMORIA
        jne .s1
        or dword [pg_avisos], AV_MEMORIA
.s1:
        cmp eax, FE_FILTRO
        jne .x
        ; el filtro que no se sabe: con su nombre de la calle, si lo tiene
        mov edi, im_filtros
.f:
        cmp byte [edi], 0
        je .tal_cual
        mov esi, fl_filtro
.f1:
        mov al, [esi]
        cmp al, [edi]
        jne .f_no
        inc esi
        inc edi
        test al, al
        jnz .f1
        mov esi, edi                ; es este: detrás va cómo se dice
        jmp .pon
.f_no:
        cmp byte [edi], 0           ; al siguiente par
        je .f_no1
        inc edi
        jmp .f_no
.f_no1:
        inc edi
.f_no2:
        cmp byte [edi], 0
        je .f_no3
        inc edi
        jmp .f_no2
.f_no3:
        inc edi
        jmp .f
.tal_cual:
        mov esi, fl_filtro
.pon:
        call im_pon_que
.x:
        pop edi
        pop esi
        xor eax, eax
        stc
        ret

im_incrustada:
        push eax
        mov dword [im_alfa], 0
        mov dword [fl_quiere_dct], 1
        push esi
        push ebp
        call pdf_filtros
        pop ebp
        pop esi
        mov dword [fl_quiere_dct], 0
        jc im_sin_flujo
        jmp im_sigue

; (interna) la transparencia de la imagen [im_obj], sobre su miniatura
; [im_mini]: /SMask (otra imagen, de grises: la opacidad) o /Mask (otra
; imagen, de un bit: se ve donde ella se pintaría). A todo o nada
im_mascaras:
        pushad
        mov esi, [im_obj]
        mov ebp, [im_obj+4]
        mov edi, k_smask
        call px_dic
        jc .mask
        call px_ref
        mov dword [im_alfa], 1
        call im_aux
        mov dword [im_alfa], 0
        jc .mask
        xor ebx, ebx
        call im_aplica
        jmp .x
.mask:
        mov esi, [im_obj]
        mov ebp, [im_obj+4]
        mov edi, k_mask
        call px_dic
        jc .x
        call px_ref
        cmp esi, ebp
        jae .x
        cmp byte [esi], '['         ; (por colores: eso ya lo ha mirado im_hace)
        je .x
        call im_aux
        jc .x
        mov ebx, 1
        call im_aplica
.x:
        popad
        ret

; (interna) ESI, EBP=la imagen que hace de máscara -> EAX=su miniatura, que
; queda de prestado en lo alto del almacén (la devuelve im_aplica); CF=1 si no
im_aux:
        push ebx
        push ecx
        push dword [im_lado]
        push dword [im_que]
        mov ebx, [im_mini]          ; no hace falta más fina que la imagen
        mov ecx, [ebx]
        cmp ecx, [ebx+4]
        jae .a1
        mov ecx, [ebx+4]
.a1:
        mov [im_lado], ecx
        mov ebx, [ar_fin]
        mov [im_fin_aux], ebx
        call ar_marca
        push eax
        xor eax, eax
        call im_flujo
        mov ebx, eax                ; (nada de lo que sigue toca CF)
        pop eax
        call ar_suelta
        mov eax, ebx
        pop dword [im_que]          ; (si la máscara no se sabe, no es para decirlo)
        pop dword [im_lado]
        pop ecx
        pop ebx
        ret

; (interna) EAX=la miniatura de la máscara EBX=0 opacidad (se ve con 128 o
; más), 1 máscara (se ve donde se pintaría): quita de [im_mini] lo que no se ve
im_aplica:
        pushad
        mov esi, eax
        mov edi, [im_mini]
        xor ebp, ebp                ; EBP=la fila
.y:
        cmp ebp, [edi+4]
        jae .fin
        mov eax, ebp                ; su fila de la máscara
        mul dword [esi+4]
        div dword [edi+4]
        mul dword [esi]
        lea eax, [esi+eax+8]
        mov [im_ap], eax
        xor ecx, ecx                ; ECX=la columna
.x:
        cmp ecx, [edi]
        jae .y1
        mov eax, ecx
        mul dword [esi]
        div dword [edi]
        add eax, [im_ap]
        mov al, [eax]
        test ebx, ebx
        jnz .m
        cmp al, 128
        jae .x1
        jmp .quita
.m:
        cmp al, IM_NADA
        jne .x1
.quita:
        mov eax, ebp
        mul dword [edi]
        add eax, ecx
        mov byte [edi+eax+8], IM_NADA
.x1:
        inc ecx
        jmp .x
.y1:
        inc ebp
        jmp .y
.fin:
        mov eax, [im_fin_aux]       ; la máscara ya no hace falta
        mov [ar_fin], eax
        popad
        ret

; -----------------------------------------------------------------------------
;  Las JPEG. ESI, EBP=el diccionario; [im_dat], [im_ndat]=el JPEG; AL=relleno
;  -> EAX=la miniatura; CF=1 si no se sabe o no cabe
; -----------------------------------------------------------------------------
im_hace_jpeg:
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
        mov [im_ck_n], eax
        mov eax, [ar_fin]
        mov [im_fin0], eax
        ; los componentes, como son (sus colores los pone el PDF); girados o
        ; no, lo que diga el APP14 de Adobe y, si no, el /ColorTransform
        mov eax, JPEG_COMP
        mov ebx, [fl_dct_tr]
        test ebx, ebx
        js .o1
        jnz .o_si
        or eax, JPEG_TR_NO
        jmp .o1
.o_si:
        or eax, JPEG_TR_SI
.o1:
        mov [im_jopc], eax
        mov esi, [im_dat]
        mov ecx, [im_ndat]
        call jpeg_medidas
        jc .mal_jpeg
        mov [im_w], eax
        mov [im_h], ebx
        mov [im_jnc], ecx
        mov [im_nc], ecx
        mov dword [im_bpc], 8
        ; del espacio de color del PDF solo importa si es Indexed (un
        ; componente que son índices): cuántos trae lo dice el JPEG
        mov edi, k_cspace
        mov ebx, k_cs1
        call im_clave
        jc .cs_ya
        call im_espacio
        jc .cs_no
        cmp dword [im_ind], 0
        je .cs_no
        cmp dword [im_jnc], 1
        je .cs_ya
.cs_no:
        mov dword [im_ind], 0
        mov eax, [im_jnc]
        mov [im_nc], eax
.cs_ya:
        call im_mira_decode
        mov eax, [im_lado]
        test eax, eax
        jnz .l0
        mov eax, IM_LADO
.l0:
        mov [im_l], eax
        mov dword [im_jr], 0
.mide:
        ; la reducción al vuelo: la mayor que deje el lado largo en [im_l] o más
        mov eax, [im_w]
        cmp eax, [im_h]
        jae .m1
        mov eax, [im_h]
.m1:
        xor ecx, ecx
.m2:
        cmp ecx, 3
        jae .m3
        mov edx, eax
        shr edx, 1
        cmp edx, [im_l]
        jb .m3
        mov eax, edx
        inc ecx
        jmp .m2
.m3:
        cmp ecx, [im_jr]
        jae .m4
        mov ecx, [im_jr]
.m4:
        mov [im_jr], ecx
        mov eax, [im_jopc]
        or eax, ecx
        mov esi, [im_dat]
        mov ecx, [im_ndat]
        call jpeg_medidas
        jc .mal_jpeg
        mov eax, [jp_sal_an]
        mov [im_sw], eax
        mov [im_tw], eax
        mov ebx, [jp_sal_al]
        mov [im_sh], ebx
        mov [im_th], ebx
        mov ecx, eax                ; ECX=el lado largo de lo que sale
        cmp ecx, ebx
        jae .t1
        mov ecx, ebx
.t1:
        cmp ecx, [im_l]
        jbe .t_ya
        mov ebx, ecx                ; se encoge: lado * [im_l] / largo, redondeado
        shr ebx, 1
        mul dword [im_l]
        add eax, ebx
        adc edx, 0
        div ecx
        test eax, eax
        jnz .t2
        inc eax
.t2:
        mov [im_tw], eax
        mov eax, [im_sh]
        mul dword [im_l]
        add eax, ebx
        adc edx, 0
        div ecx
        test eax, eax
        jnz .t3
        inc eax
.t3:
        mov [im_th], eax
.t_ya:
        ; ¿cabe? La miniatura (arriba), y abajo la memoria de jpeg.inc, una
        ; fila encogida y las sumas de una fila de la miniatura
        mov eax, [im_tw]
        mul dword [im_th]
        lea ecx, [eax+8+3]
        and ecx, ~3
        mov [im_mtam], ecx
        mov eax, [im_tw]
        imul eax, [jp_sal_nc]
        lea eax, [eax+eax*4+16]
        add eax, [jp_pide]
        jc .no_cabe
        add eax, ecx
        jc .no_cabe
        mov edx, [im_fin0]
        sub edx, [ar_tope]
        jb .no_cabe
        cmp eax, edx
        jbe .cabe
.no_cabe:
        ; no cabe: si lo gordo son los coeficientes de un progresivo, más
        ; reducción; si no, una miniatura más pequeña
        mov eax, [jp_pide]
        cmp eax, [im_mtam]
        jbe .menos
        cmp dword [im_jr], 3
        jae .menos
        inc dword [im_jr]
        jmp .mide
.menos:
        cmp dword [im_l], 80
        jbe .ultimo
        shr dword [im_l], 1
        jmp .mide
.ultimo:
        cmp dword [im_jr], 3
        jae .sin_memoria
        inc dword [im_jr]
        jmp .mide
.cabe:
        mov eax, [im_fin0]
        sub eax, [im_mtam]
        mov [ar_fin], eax
        mov [im_mini_p], eax
        mov edi, eax
        mov eax, [im_tw]
        stosd
        mov eax, [im_th]
        stosd
        mov ecx, [im_tw]            ; lo que no llegue a salir deja ver lo de debajo
        imul ecx, [im_th]
        mov al, IM_NADA
        rep stosb
        mov ecx, [im_tw]
        imul ecx, [jp_sal_nc]
        call ar_pide
        jc .sin_memoria
        mov [im_hfila], eax
        shl ecx, 2
        call ar_pide
        jc .sin_memoria
        mov [im_vacc], eax
        mov edi, eax
        shr ecx, 2
        xor eax, eax
        rep stosd
        mov ecx, [jp_pide]
        call ar_pide
        jc .sin_memoria
        mov edi, eax
        mov edx, [jp_pide]
        mov esi, [im_dat]
        mov ecx, [im_ndat]
        mov eax, [im_jopc]
        or eax, [im_jr]
        call jpeg_abre
        jc .mal_jpeg
        ; fila a fila: se encoge a lo ancho y se reparte entre las filas de
        ; la miniatura que pisa (de alto, cada fila de la JPEG mide [im_th] y
        ; cada una de la miniatura [im_sh]: así no hay quebrados)
        mov eax, [im_sh]
        mov [im_vcap], eax
        mov dword [im_dy], 0
.fila:
        call jpeg_fila
        jc .fin_filas
        call im_j_ancho
        mov eax, [im_th]
        mov [im_va], eax
.v:
        mov ebx, [im_va]
        cmp ebx, [im_vcap]
        jbe .v1
        mov ebx, [im_vcap]
.v1:
        sub [im_va], ebx
        sub [im_vcap], ebx
        mov esi, [im_hfila]
        mov edi, [im_vacc]
        mov ecx, [im_tw]
        imul ecx, [jp_sal_nc]
.v2:
        movzx eax, byte [esi]
        inc esi
        imul eax, ebx
        add [edi], eax
        add edi, 4
        dec ecx
        jnz .v2
        cmp dword [im_vcap], 0
        jne .v3
        call im_j_saca
        inc dword [im_dy]
        mov eax, [im_sh]
        mov [im_vcap], eax
.v3:
        cmp dword [im_va], 0
        jne .v
        jmp .fila
.fin_filas:
        test eax, eax
        jz .bien
        cmp dword [im_dy], 0        ; rota o cortada: si ha salido algo, se
        je .mal_jpeg                ; enseña, y la página lleva su aviso
        or dword [pg_avisos], AV_ROTO
.bien:
        mov eax, [im_mini_p]
        clc
        jmp .x
.sin_memoria:
        or dword [pg_avisos], AV_MEMORIA
        jmp .no
.mal_jpeg:
        cmp eax, JPEG_E_MEMORIA
        je .sin_memoria
        cmp eax, 8
        ja .no
        mov esi, [im_jpeg_no+eax*4-4]
        call im_pon_que
.no:
        mov eax, [im_fin0]
        mov [ar_fin], eax
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

; (interna) ESI=una fila de la JPEG ([im_sw] puntos) -> [im_hfila], encogida
; a [im_tw] puntos: cada punto de la JPEG mide [im_tw] y cada uno de los que
; salen [im_sw]; cada cual pone lo que pisa
im_j_ancho:
        pushad
        mov edi, [im_hfila]
        mov ebp, [jp_sal_nc]
        mov eax, [im_sw]
        cmp eax, [im_tw]
        jne .escala
        mov ecx, eax                ; (del mismo ancho: tal cual)
        imul ecx, ebp
        rep movsb
        popad
        ret
.escala:
        xor eax, eax
        mov [im_hacc], eax
        mov [im_hacc+4], eax
        mov [im_hacc+8], eax
        mov [im_hacc+12], eax
        mov edx, [im_sw]            ; EDX=lo que le falta al punto que se llena
        mov ecx, [im_sw]            ; ECX=puntos que quedan de la fila
.px:
        mov ebx, [im_tw]            ; EBX=lo que queda de este punto por repartir
.h:
        push ebx
        cmp ebx, edx
        jbe .h1
        mov ebx, edx
.h1:
        sub edx, ebx
        push ecx
        xor ecx, ecx
.hc:
        movzx eax, byte [esi+ecx]
        imul eax, ebx
        add [im_hacc+ecx*4], eax
        inc ecx
        cmp ecx, ebp
        jb .hc
        pop ecx
        mov eax, ebx
        pop ebx
        sub ebx, eax
        test edx, edx
        jnz .h2
        push ecx                    ; lleno: su media, y al siguiente
        push ebx
        xor ecx, ecx
.he:
        mov eax, [im_sw]
        shr eax, 1
        add eax, [im_hacc+ecx*4]
        xor edx, edx
        div dword [im_sw]
        stosb
        mov dword [im_hacc+ecx*4], 0
        inc ecx
        cmp ecx, ebp
        jb .he
        pop ebx
        pop ecx
        mov edx, [im_sw]
.h2:
        test ebx, ebx
        jnz .h
        add esi, ebp
        dec ecx
        jnz .px
        popad
        ret

; (interna) la fila [im_dy] de la miniatura, de las sumas (que deja a cero):
; la media, a rojo, verde y azul y de ahí a la paleta
im_j_saca:
        pushad
        mov eax, [im_dy]
        cmp eax, [im_th]
        jae .x
        mul dword [im_tw]
        add eax, [im_mini_p]
        lea edi, [eax+8]
        mov esi, [im_vacc]
        mov dword [im_x], 0
.p:
        xor ecx, ecx
.c:
        mov eax, [im_sh]
        shr eax, 1
        add eax, [esi]
        mov dword [esi], 0
        add esi, 4
        xor edx, edx
        div dword [im_sh]
        mov [im_sum+ecx*4], eax
        inc ecx
        cmp ecx, [jp_sal_nc]
        jb .c
        push esi
        push edi
        call im_rgb
        cmp dword [im_alfa], 0      ; (de una /SMask se guarda la opacidad)
        jne .g
        mov esi, [im_x]
        mov edi, [im_dy]
        call im_color
        cmp al, IM_NADA
        jne .g
        dec al
.g:
        pop edi
        pop esi
        stosb
        inc dword [im_x]
        mov eax, [im_x]
        cmp eax, [im_tw]
        jb .p
.x:
        popad
        ret
