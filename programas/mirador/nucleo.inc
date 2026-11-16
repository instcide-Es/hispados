; =============================================================================
;  MIRADOR.HVP - el núcleo: la capa de formatos. Reconoce un archivo de imagen
;  por su contenido y lo entrega fila a fila, de arriba abajo, en rojo, verde
;  y azul (tres octetos por punto), con lo transparente ya mezclado con el
;  fondo. No sabe nada de Hispa Ventanas: se prueba aparte, en Linux, contra
;  Pillow (tests/tools/mirador_formatos.py).
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Cada formato es una línea de la tabla «mi_formatos» y un archivo .inc:
;      detecta   ESI=datos ECX=cuántos -> CF=0 si el archivo es de los suyos
;      abre      ESI=datos ECX=cuántos: lee la cabecera -> EAX=ME_* (0: bien)
;                y deja puestos im_an, im_al, im_clase, im_bits, im_ncol,
;                im_pal (si es de paleta), im_hay_alfa e im_alm (los octetos
;                de memoria de trabajo que necesita)
;      prepara   con la memoria de trabajo en [im_almp]: lo que haya que
;                descomprimir de una vez -> EAX=ME_*
;      fila      EAX=fila (se piden en orden, de la 0 en adelante, una vez
;                cada una) EDI=destino -> la fila, en la «clase» del formato
;                (IC_*: índices, gris, RGB, RGBA o gris con alfa); CF=1 si
;                esa fila ya no está (archivo cortado)
;      reduce    (0 si no sabe) EAX=1, 2 o 3: abre otra vez para entregar la
;                imagen a la mitad, un cuarto o un octavo (JPEG)
;  Pueden cambiar todos los registros. Si el archivo está cortado o dañado
;  pero algo se ve, ponen [im_aviso] a 1. Si la variante no se admite,
;  devuelven ME_VARIANTE con el texto que lo explica en [im_var].
;
;  Lo que usa el programa:
;    mi_abre     ESI=datos ECX=cuántos -> EAX=ME_*; con 0, las medidas y lo
;                demás en im_*, el formato en [im_fmt] y en [mi_mem] la
;                memoria de trabajo que hay que darle
;    mi_reduce   EAX=1, 2 o 3 -> CF=0 si el formato sabe entregar la imagen
;                ya reducida a 1/2, 1/4 o 1/8 (cambian im_an, im_al y mi_mem)
;    mi_prepara  EDI=la memoria de trabajo ([mi_mem] octetos) -> EAX=ME_*
;    mi_fila     EAX=fila EDI=destino (3 octetos por punto) -> CF=1 si no está
;  Lo transparente se mezcla con un ajedrezado de cuadros de 8 puntos, blanco
;  y gris claro ([mi_fondo]=0), o con el color liso [mi_fondo_rgb] (=1).
; =============================================================================
ME_OK           equ 0
ME_ROTO         equ 1               ; el archivo está dañado: no se puede ver nada
ME_VARIANTE     equ 2               ; una variante que no se admite ([im_var] dice cuál)
ME_MEMORIA      equ 3               ; (del programa) no hay memoria
ME_DESCONOCIDO  equ 4               ; no es de ningún formato que se conozca
ME_GRANDE       equ 5               ; medidas imposibles (más de 32.767 de lado o de 64 megapuntos)

IC_PALETA       equ 0               ; un índice de im_pal por punto
IC_GRIS         equ 1               ; un gris por punto
IC_RGB          equ 2               ; rojo, verde, azul
IC_RGBA         equ 3               ; rojo, verde, azul, opacidad
IC_GRISA        equ 4               ; gris, opacidad

F_DETECTA       equ 0
F_ABRE          equ 4
F_PREPARA       equ 8
F_FILA          equ 12
F_REDUCE        equ 16
F_NOMBRE        equ 20
F_TAM           equ 24

MI_LADO_MAX     equ 32767
MI_PUNTOS_MAX   equ 64 * 1024 * 1024

%macro NUCLEO_BSS 0
            alignb 4
im_dat      resd 1                  ; el archivo, entero en memoria
im_tam      resd 1
im_fmt      resd 1                  ; su línea de mi_formatos
im_an       resd 1
im_al       resd 1
im_clase    resd 1
im_bits     resd 1                  ; bits por punto en el archivo (para la información)
im_ncol     resd 1                  ; colores de su paleta (0: no tiene)
im_hay_alfa resd 1                  ; tiene partes transparentes
im_aviso    resd 1                  ; cortado o dañado: se ve una parte
im_var      resd 1
im_extra    resd 1                  ; un texto más para la información (0: nada)
im_orienta  resd 1                  ; cómo hay que ponerla para verla derecha (EXIF: de 1 a 8; 1: como viene)
im_camara   resd 1                  ; con qué cámara se hizo y cuándo (textos; 0: no lo dice)
im_tomada   resd 1
im_alm      resd 1
im_almp     resd 1
im_pal      resb 1024               ; rojo, verde, azul, opacidad
mi_mem      resd 1
mi_cruda    resd 1                  ; una fila como la da el formato
mi_fondo    resd 1
mi_fondo_rgb resd 1
mi_y        resd 1
mi_px       resd 1
mi_bg       resd 1
%endmacro

mi_formatos:
        dd png_detecta, png_abre, png_prepara, png_fila, 0, mi_n_png
        dd gif_detecta, gif_abre, gif_prepara, gif_fila, 0, mi_n_gif
        dd bmp_detecta, bmp_abre, bmp_prepara, bmp_fila, 0, mi_n_bmp
        dd jpg_detecta, jpg_abre, jpg_prepara, jpg_fila, jpg_reduce, mi_n_jpg
        dd pnm_detecta, pnm_abre, mi_nada, pnm_fila, 0, mi_n_pnm
        dd ico_detecta, ico_abre, ico_prepara, ico_fila, 0, mi_n_ico
        dd pcx_detecta, pcx_abre, mi_nada, pcx_fila, 0, mi_n_pcx
        dd tga_detecta, tga_abre, tga_prepara, tga_fila, 0, mi_n_tga
        dd 0
mi_n_png    db "PNG", 0
mi_n_gif    db "GIF", 0
mi_n_bmp    db "BMP", 0
mi_n_jpg    db "JPEG", 0
mi_n_pnm    db "PBM/PGM/PPM", 0
mi_n_ico    db "ICO", 0
mi_n_pcx    db "PCX", 0
mi_n_tga    db "TGA", 0

; cinco bits a ocho, como lo hace todo el mundo (v·255/31, sin redondear)
mi_cinco:
%assign i 0
%rep 32
        db i * 255 / 31
%assign i i + 1
%endrep

; EAX=cuatro octetos leídos de un archivo que los guarda al revés (PNG)
mi_be32:
        xchg al, ah
        rol eax, 16
        xchg al, ah
        ret

; (un «prepara» que no tiene nada que hacer)
mi_nada:
        xor eax, eax
        ret

; ¿valen las medidas [im_an] x [im_al]? CF=1 si no
mi_medidas_ok:
        push eax
        push edx
        mov eax, [im_an]
        dec eax
        cmp eax, MI_LADO_MAX
        jae .no
        mov eax, [im_al]
        dec eax
        cmp eax, MI_LADO_MAX
        jae .no
        mov eax, [im_an]
        mul dword [im_al]
        cmp eax, MI_PUNTOS_MAX
        ja .no
        pop edx
        pop eax
        clc
        ret
.no:
        pop edx
        pop eax
        stc
        ret

; desempaqueta puntos de menos de un octeto: ESI=octetos (el bit de más peso,
; el primer punto) EDI=destino (un octeto por punto) EDX=puntos CL=bits por
; punto (1, 2 o 4). Cambia EAX, EBX, EDX, ESI, EDI
mi_desempaqueta:
        test edx, edx
        jz .x
        mov ch, 8                   ; CH=bits que quedan en el octeto
        mov bl, 1
        shl bl, cl
        dec bl                      ; BL=la máscara de un punto
        mov ah, [esi]
        inc esi
.p:
        sub ch, cl
        mov al, ah
        xchg cl, ch
        shr al, cl
        xchg cl, ch
        and al, bl
        stosb
        dec edx
        jz .x
        test ch, ch
        jnz .p
        mov ch, 8
        mov ah, [esi]
        inc esi
        jmp .p
.x:
        ret

mi_abre:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov [im_dat], esi
        mov [im_tam], ecx
        xor eax, eax
        mov [im_fmt], eax
        mov [im_an], eax
        mov [im_al], eax
        mov [im_bits], eax
        mov [im_ncol], eax
        mov [im_hay_alfa], eax
        mov [im_aviso], eax
        mov [im_var], eax
        mov [im_extra], eax
        mov [im_camara], eax
        mov [im_tomada], eax
        mov dword [im_orienta], 1
        mov [im_alm], eax
        mov [im_clase], eax
        mov edi, im_pal             ; la paleta, negra y opaca
        mov ecx, 256
        mov eax, 0xFF000000
        rep stosd
        mov ebx, mi_formatos
.b:
        cmp dword [ebx], 0
        je .ninguno
        mov esi, [im_dat]
        mov ecx, [im_tam]
        push ebx
        call [ebx+F_DETECTA]
        pop ebx
        jnc .es
        add ebx, F_TAM
        jmp .b
.ninguno:
        mov eax, ME_DESCONOCIDO
        jmp .x
.es:
        mov [im_fmt], ebx
        mov esi, [im_dat]
        mov ecx, [im_tam]
        call [ebx+F_ABRE]
        test eax, eax
        jnz .x
        call mi_medidas_ok
        mov eax, ME_GRANDE
        jc .x
        call mi_pon_mem
        xor eax, eax
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) la memoria de trabajo: la del formato y una fila cruda
mi_pon_mem:
        mov eax, [im_alm]
        add eax, 7
        and eax, ~3
        mov [im_alm], eax
        mov edx, [im_an]
        lea eax, [eax+edx*4+64]
        mov [mi_mem], eax
        ret

mi_reduce:
        pushad
        mov ebx, [im_fmt]
        cmp dword [ebx+F_REDUCE], 0
        je .no
        call [ebx+F_REDUCE]
        jc .no
        call mi_medidas_ok
        jc .no
        call mi_pon_mem
        popad
        clc
        ret
.no:
        popad
        stc
        ret

mi_prepara:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov [im_almp], edi
        add edi, [im_alm]
        mov [mi_cruda], edi
        mov ebx, [im_fmt]
        call [ebx+F_PREPARA]
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

mi_fila:
        pushad
        mov [mi_y], eax
        mov edi, [mi_cruda]
        mov ebx, [im_fmt]
        call [ebx+F_FILA]
        jc .mal
        mov esi, [mi_cruda]
        mov edi, [esp]              ; (el EDI de quien llama)
        mov ecx, [im_an]
        mov eax, [im_clase]
        cmp eax, IC_RGB
        je .rgb
        cmp eax, IC_GRIS
        je .gris
        xor ebx, ebx                ; EBX=x
        cmp eax, IC_RGBA
        je .rgba
        cmp eax, IC_GRISA
        je .grisa
.pal:
        movzx eax, byte [esi+ebx]
        mov eax, [im_pal+eax*4]
        call mi_pon
        inc ebx
        cmp ebx, ecx
        jb .pal
        jmp .bien
.rgb:
        lea ecx, [ecx+ecx*2]
        rep movsb
        jmp .bien
.gris:
        lodsb
        stosb
        stosb
        stosb
        dec ecx
        jnz .gris
        jmp .bien
.rgba:
        mov eax, [esi+ebx*4]
        call mi_pon
        inc ebx
        cmp ebx, ecx
        jb .rgba
        jmp .bien
.grisa:
        movzx eax, byte [esi+ebx*2]
        movzx edx, byte [esi+ebx*2+1]
        shl edx, 24
        imul eax, 0x010101
        or eax, edx
        call mi_pon
        inc ebx
        cmp ebx, ecx
        jb .grisa
.bien:
        popad
        clc
        ret
.mal:
        popad
        stc
        ret

; (interna) EAX=un punto (rojo, verde, azul, opacidad) EBX=su columna: lo
; escribe en EDI (tres octetos), mezclado con el fondo si no es opaco
mi_pon:
        cmp eax, 0xFF000000
        jb .mezcla
        stosw
        shr eax, 16
        stosb
        ret
.mezcla:
        push ecx
        push edx
        push esi
        mov [mi_px], eax
        mov eax, [mi_fondo_rgb]
        cmp dword [mi_fondo], 0
        jne .f
        mov eax, ebx                ; el ajedrezado
        mov edx, [mi_y]
        shr eax, 3
        shr edx, 3
        xor eax, edx
        test al, 1
        mov eax, 0x00FFFFFF
        jz .f
        mov eax, 0x00CCCCCC
.f:
        mov [mi_bg], eax
        movzx ecx, byte [mi_px+3]   ; ECX=opacidad
        xor esi, esi
.c:
        movzx eax, byte [mi_px+esi]
        imul eax, ecx
        movzx edx, byte [mi_bg+esi]
        push ecx
        xor ecx, 255
        imul edx, ecx
        pop ecx
        lea eax, [eax+edx+127]      ; y entre 255, redondeando
        mov edx, eax
        shr edx, 8
        lea eax, [eax+edx+1]
        shr eax, 8
        stosb
        inc esi
        cmp esi, 3
        jb .c
        pop esi
        pop edx
        pop ecx
        ret
