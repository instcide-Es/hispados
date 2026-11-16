; =============================================================================
;  MIRADOR.HVP - los iconos y cursores de Windows (.ICO, .CUR)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un .ICO lleva varias imágenes del mismo icono, de distintos tamaños y
;  colores: se enseña la mayor (y, de las iguales, la de más colores). Cada
;  una es un BMP sin su primera cabecera, de 1, 4, 8, 24 o 32 bits, con una
;  máscara de un bit detrás que dice qué puntos son transparentes (en los de
;  32 bits manda la opacidad de cada punto); o, en los modernos, un PNG
;  entero, que se le pasa al de los PNG.
; =============================================================================
%macro ICO_BSS 0
            alignb 4
ico_png     resd 1                  ; 1: lo que hay dentro es un PNG
ico_bits    resd 1
ico_px      resd 1                  ; los puntos
ico_masc    resd 1                  ; la máscara
ico_paso    resd 1
ico_paso_m  resd 1
ico_fin     resd 1
ico_mejor   resd 1
ico_mejor_p resd 1
ico_mejor_n resd 1
%endmacro

ico_t_var   db "un icono con una clase de imagen que Mirador no conoce", 0

; (interna) ESI=archivo ECX=tamaño EBX=una entrada de la lista -> CF=0 si lo
; que dice está dentro del archivo y empieza como un BMP o un PNG; EDX=dónde
ico_mira:
        mov edx, [ebx+12]
        cmp edx, ecx
        jae .no
        mov eax, ecx
        sub eax, edx
        cmp eax, 40
        jb .no
        cmp dword [esi+edx], 0x474E5089
        je .si
        mov eax, [esi+edx]
        cmp eax, 40
        jb .no
        cmp eax, 124
        ja .no
.si:
        clc
        ret
.no:
        stc
        ret

ico_detecta:
        cmp ecx, 6 + 16 + 40
        jb .no
        cmp word [esi], 0
        jne .no
        movzx eax, word [esi+2]
        dec eax
        cmp eax, 1
        ja .no
        movzx eax, word [esi+4]
        test eax, eax
        jz .no
        shl eax, 4
        add eax, 6
        cmp eax, ecx
        ja .no
        lea ebx, [esi+6]
        call ico_mira
        ret
.no:
        stc
        ret

ico_abre:
        lea eax, [esi+ecx]
        mov [ico_fin], eax
        movzx ebp, word [esi+4]     ; EBP=cuántas hay
        lea ebx, [esi+6]
        mov dword [ico_mejor], 0
        mov dword [ico_mejor_p], 0
.e:
        call ico_mira
        jc .sig
        ; sus medidas de verdad, de lo que lleva dentro
        cmp dword [esi+edx], 0x474E5089
        jne .e_bmp
        mov eax, [esi+edx+20]       ; (un PNG: ancho y alto, al revés)
        call mi_be32
        mov edi, eax
        mov eax, [esi+edx+16]
        call mi_be32
        push edx
        mov edx, 32
        jmp .e_vale
.e_bmp:
        mov eax, [esi+edx+4]
        mov edi, [esi+edx+8]
        sar edi, 1
        push edx
        movzx edx, word [esi+edx+14]
.e_vale:
        ; EAX=ancho EDI=alto EDX=bits: ¿es la mayor?
        cmp eax, 4096
        ja .e_no
        cmp edi, 4096
        ja .e_no
        test eax, eax
        jle .e_no
        test edi, edi
        jle .e_no
        imul eax, edi
        shl eax, 6
        add eax, edx
        cmp eax, [ico_mejor]
        jbe .e_no
        mov [ico_mejor], eax
        pop edx
        mov [ico_mejor_p], edx
        mov eax, [ebx+8]
        mov [ico_mejor_n], eax
        jmp .sig
.e_no:
        pop edx
.sig:
        add ebx, 16
        dec ebp
        jnz .e
        mov edx, [ico_mejor_p]
        test edx, edx
        jz .roto
        mov dword [ico_png], 0
        cmp dword [esi+edx], 0x474E5089
        jne .bmp
        mov dword [ico_png], 1      ; un PNG: lo que quede de archivo, para él
        add esi, edx
        sub ecx, edx
        cmp ecx, [ico_mejor_n]
        jbe .p1
        mov ecx, [ico_mejor_n]
.p1:
        call png_detecta
        jc .roto
        jmp png_abre
.bmp:
        add esi, edx                ; ESI=la cabecera de dentro
        mov eax, [esi+4]
        mov [im_an], eax
        mov eax, [esi+8]
        sar eax, 1
        mov [im_al], eax
        call mi_medidas_ok
        jc .roto
        cmp dword [esi+16], 0       ; sin comprimir
        jne .variante
        movzx eax, word [esi+14]
        mov [ico_bits], eax
        mov [im_bits], eax
        cmp eax, 1
        je .b_ok
        cmp eax, 4
        je .b_ok
        cmp eax, 8
        je .b_ok
        cmp eax, 24
        je .b_ok
        cmp eax, 32
        jne .variante
.b_ok:
        mov dword [im_clase], IC_RGBA
        mov dword [im_hay_alfa], 1
        mov edi, [esi]
        add edi, esi                ; EDI=la paleta
        cmp eax, 8
        ja .sin_pal
        mov ecx, eax
        mov eax, 1
        shl eax, cl
        mov ecx, [esi+32]           ; (los colores que dice, si dice)
        test ecx, ecx
        jz .c1
        cmp ecx, eax
        jbe .c2
.c1:
        mov ecx, eax
.c2:
        mov [im_ncol], ecx
        xor eax, eax
.c3:
        lea edx, [edi+4]
        cmp edx, [ico_fin]
        ja .roto
        mov dl, [edi+2]
        mov [im_pal+eax*4], dl
        mov dl, [edi+1]
        mov [im_pal+eax*4+1], dl
        mov dl, [edi]
        mov [im_pal+eax*4+2], dl
        add edi, 4
        inc eax
        dec ecx
        jnz .c3
.sin_pal:
        mov [ico_px], edi
        mov eax, [im_an]
        imul eax, [ico_bits]
        add eax, 31
        shr eax, 5
        shl eax, 2
        mov [ico_paso], eax
        imul eax, [im_al]
        add eax, edi
        mov [ico_masc], eax
        mov eax, [im_an]
        add eax, 31
        shr eax, 5
        shl eax, 2
        mov [ico_paso_m], eax
        xor eax, eax
        ret
.roto:
        mov eax, ME_ROTO
        ret
.variante:
        mov dword [im_var], ico_t_var
        mov eax, ME_VARIANTE
        ret

ico_prepara:
        cmp dword [ico_png], 0
        jne png_prepara
        xor eax, eax
        ret

ico_fila:
        cmp dword [ico_png], 0
        jne png_fila
        neg eax                     ; de abajo arriba
        add eax, [im_al]
        dec eax
        mov ebp, eax                ; EBP=la fila del archivo
        imul eax, [ico_paso]
        mov esi, [ico_px]
        add esi, eax
        mov eax, [im_an]
        imul eax, [ico_bits]
        add eax, 7
        shr eax, 3
        add eax, esi
        cmp eax, [ico_fin]
        ja .falta
        mov eax, [ico_bits]
        cmp eax, 8
        ja .directo
        ; con paleta: los índices, al final del destino, y de ahí a su color
        push edi
        mov edx, [im_an]
        lea edi, [edi+edx*2]
        add edi, edx
        cmp eax, 8
        je .i8
        mov cl, al
        push edi
        call mi_desempaqueta
        pop esi
        jmp .i1
.i8:
        mov ecx, edx
        push edi
        rep movsb
        pop esi
.i1:
        pop edi
        mov ecx, [im_an]
.i2:
        movzx eax, byte [esi]
        inc esi
        mov eax, [im_pal+eax*4]
        stosd
        dec ecx
        jnz .i2
        jmp .mascara
.directo:
        mov ecx, [im_an]
        cmp eax, 32
        je .d32
.d24:
        mov al, [esi+2]
        stosb
        mov al, [esi+1]
        stosb
        mov al, [esi]
        stosb
        mov al, 255
        stosb
        add esi, 3
        dec ecx
        jnz .d24
        jmp .mascara
.d32:
        mov al, [esi+2]
        stosb
        mov al, [esi+1]
        stosb
        mov al, [esi]
        stosb
        mov al, [esi+3]
        stosb
        add esi, 4
        dec ecx
        jnz .d32
        clc                         ; (con 32 bits manda su opacidad, no la máscara)
        ret
.mascara:
        ; un bit a 1 en la máscara: ese punto no se ve
        mov eax, [im_an]
        shl eax, 2
        sub edi, eax                ; EDI=otra vez el principio de la fila
        mov esi, ebp
        imul esi, [ico_paso_m]
        add esi, [ico_masc]
        mov eax, [im_an]
        add eax, 7
        shr eax, 3
        add eax, esi
        cmp eax, [ico_fin]          ; (sin máscara en el archivo: todo opaco)
        ja .bien
        xor ecx, ecx
.m1:
        mov eax, ecx                ; (el punto 0 es el bit 7)
        shr eax, 3
        movzx eax, byte [esi+eax]
        mov edx, ecx
        not edx
        and edx, 7
        bt eax, edx
        jnc .m2
        mov byte [edi+ecx*4+3], 0
.m2:
        inc ecx
        cmp ecx, [im_an]
        jb .m1
.bien:
        clc
        ret
.falta:
        stc
        ret
