; =============================================================================
;  MIRADOR.HVP - el formato BMP (Windows y OS/2)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Cabeceras de OS/2 1.x (12 octetos, paleta de tres octetos por color), de
;  OS/2 2.x (de 16 a 64) y de Windows 3, 4 y 5 (40, 52, 56, 108 y 124). De 1,
;  4 y 8 bits con paleta; de 16, 24 y 32 en color directo (los de 16 y 32,
;  con las máscaras que digan, y con su opacidad si la llevan). Comprimidos
;  con RLE8 y RLE4. De abajo arriba (lo corriente) y de arriba abajo (alto
;  negativo). No: los que llevan dentro un JPEG o un PNG, ni el Huffman de
;  OS/2 (se dice).
;
;  Sin comprimir, cada fila se toma de su sitio en el archivo. Uno con RLE se
;  descomprime entero a la memoria de trabajo (un octeto por punto).
; =============================================================================
%macro BMP_BSS 0
            alignb 4
bmp_dat     resd 1
bmp_fin     resd 1
bmp_px      resd 1                  ; dónde empiezan los puntos
bmp_bits    resd 1
bmp_comp    resd 1
bmp_paso    resd 1                  ; octetos de una fila en el archivo
bmp_abajo   resd 1                  ; 1: la primera fila del archivo es la de arriba
bmp_x       resd 1
bmp_y       resd 1
; de cada componente (rojo, verde, azul, opacidad): máscara, desplazamiento, máximo
bmp_can     resd 12
%endmacro

bmp_t_comp  db "un BMP con una compresión que Mirador no conoce (JPEG, PNG o Huffman)", 0
bmp_t_bits  db "un BMP con un número de bits por punto que no existe", 0

bmp_detecta:
        cmp ecx, 26
        jb .no
        cmp word [esi], 'BM'
        jne .no
        clc
        ret
.no:
        stc
        ret

bmp_abre:
        mov [bmp_dat], esi
        lea eax, [esi+ecx]
        mov [bmp_fin], eax
        mov edx, [esi+14]           ; EDX=tamaño de la cabecera
        lea eax, [edx+14]
        cmp eax, ecx
        ja .roto
        xor eax, eax
        mov [bmp_comp], eax
        mov [bmp_abajo], eax
        mov dword [im_ncol], 0
        cmp edx, 12
        jne .win
        movzx eax, word [esi+18]    ; la de OS/2 1.x
        mov [im_an], eax
        movzx eax, word [esi+20]
        mov [im_al], eax
        movzx eax, word [esi+24]
        mov [bmp_bits], eax
        jmp .medidas
.win:
        cmp edx, 16
        jb .roto
        cmp edx, 1024
        ja .roto
        mov eax, [esi+18]
        mov [im_an], eax
        mov eax, [esi+22]
        test eax, eax
        jns .w1
        neg eax
        mov dword [bmp_abajo], 1
.w1:
        mov [im_al], eax
        movzx eax, word [esi+28]
        mov [bmp_bits], eax
        cmp edx, 20
        jb .medidas
        mov eax, [esi+30]
        mov [bmp_comp], eax
        cmp edx, 36
        jb .medidas
        mov eax, [esi+46]
        mov [im_ncol], eax
.medidas:
        call mi_medidas_ok
        jc .grande
        mov eax, [bmp_bits]
        mov [im_bits], eax
        cmp eax, 1
        je .b_ok
        cmp eax, 4
        je .b_ok
        cmp eax, 8
        je .b_ok
        cmp eax, 16
        je .b_ok
        cmp eax, 24
        je .b_ok
        cmp eax, 32
        je .b_ok
        mov dword [im_var], bmp_t_bits
        jmp .variante
.b_ok:
        mov ebx, [bmp_comp]         ; la compresión, con los bits que le tocan
        test ebx, ebx
        jz .c_ok
        cmp ebx, 1
        jne .c2
        cmp eax, 8
        je .c_ok
        jmp .c_no
.c2:
        cmp ebx, 2
        jne .c3
        cmp eax, 4
        je .c_ok
        jmp .c_no
.c3:
        cmp ebx, 3
        je .c36
        cmp ebx, 6
        jne .c_no
.c36:
        cmp eax, 16
        je .c_ok
        cmp eax, 32
        je .c_ok
.c_no:
        mov dword [im_var], bmp_t_comp
        jmp .variante
.c_ok:
        ; las máscaras del color directo
        mov edi, bmp_can
        xor eax, eax
        mov ecx, 12
        rep stosd
        mov dword [bmp_can], 0x7C00
        mov dword [bmp_can+12], 0x03E0
        mov dword [bmp_can+24], 0x001F
        cmp dword [bmp_bits], 16
        je .m1
        mov dword [bmp_can], 0x00FF0000
        mov dword [bmp_can+12], 0x0000FF00
        mov dword [bmp_can+24], 0x000000FF
.m1:
        lea edi, [edx+14]           ; EDI=dónde va la paleta
        mov ebx, [bmp_comp]
        cmp ebx, 3
        je .m2
        cmp ebx, 6
        jne .m5
.m2:
        mov eax, [bmp_dat]          ; las máscaras: detrás de los 40 de siempre
        add eax, 54 + 12
        cmp eax, [bmp_fin]
        ja .roto
        mov eax, [esi+54]
        mov [bmp_can], eax
        mov eax, [esi+58]
        mov [bmp_can+12], eax
        mov eax, [esi+62]
        mov [bmp_can+24], eax
        cmp edx, 40
        jne .m3
        add edi, 12                 ; (fuera de la cabecera)
        cmp ebx, 6
        jne .m5
        add edi, 4
        jmp .m4
.m3:
        cmp edx, 56
        jb .m5
.m4:
        mov eax, [bmp_dat]
        add eax, 70
        cmp eax, [bmp_fin]
        ja .roto
        mov eax, [esi+66]
        mov [bmp_can+36], eax
.m5:
        ; de cada máscara, dónde empieza y cuánto vale llena
        mov ebx, bmp_can
        mov ecx, 4
.mk:
        push ecx
        mov eax, [ebx]
        test eax, eax
        jz .mk9
        bsf ecx, eax
        shr eax, cl
        xor ebp, ebp                ; EBP=bits seguidos
.mk1:
        test al, 1
        jz .mk2
        shr eax, 1
        inc ebp
        cmp ebp, 32
        jb .mk1
.mk2:
        cmp ebp, 8                  ; con más de ocho, los ocho de arriba
        jbe .mk3
        lea ecx, [ecx+ebp-8]
        mov ebp, 8
.mk3:
        mov [ebx+4], ecx
        mov ecx, ebp
        mov eax, 1
        shl eax, cl
        dec eax
        mov [ebx+8], eax
.mk9:
        pop ecx
        add ebx, 12
        dec ecx
        jnz .mk
        ; ---- la clase y la paleta ----
        mov eax, [bmp_bits]
        cmp eax, 8
        jbe .paleta
        mov dword [im_clase], IC_RGB
        mov dword [im_ncol], 0
        xor ebp, ebp                ; EBP=octetos de paleta que hay delante
        cmp eax, 24
        je .datos
        cmp dword [bmp_can+44], 0
        je .datos
        mov dword [im_clase], IC_RGBA
        mov dword [im_hay_alfa], 1
        jmp .datos
.paleta:
        mov dword [im_clase], IC_PALETA
        mov ecx, eax
        mov eax, 1
        shl eax, cl                 ; EAX=los colores que caben
        mov ecx, [im_ncol]
        test ecx, ecx
        jz .p1
        cmp ecx, eax
        jbe .p2
.p1:
        mov ecx, eax
.p2:
        mov ebx, 4                  ; EBX=octetos por color
        cmp edx, 12
        jne .p3
        mov ebx, 3
.p3:
        mov eax, [bmp_fin]          ; (los que haya de verdad en el archivo)
        sub eax, [bmp_dat]
        sub eax, edi
        jbe .roto
        push edx
        xor edx, edx
        div ebx
        pop edx
        cmp ecx, eax
        jbe .p4
        mov ecx, eax
.p4:
        mov [im_ncol], ecx
        mov ebp, ecx
        imul ebp, ebx
        push esi
        add esi, edi
        xor eax, eax
.p5:
        test ecx, ecx
        jz .p6
        mov dl, [esi+2]
        mov [im_pal+eax*4], dl
        mov dl, [esi+1]
        mov [im_pal+eax*4+1], dl
        mov dl, [esi]
        mov [im_pal+eax*4+2], dl
        add esi, ebx
        inc eax
        dec ecx
        jmp .p5
.p6:
        pop esi
.datos:
        add ebp, edi                ; lo primero que puede ser un punto
        mov eax, [esi+10]
        cmp eax, ebp
        jae .d1
        mov eax, ebp                ; (hay quien deja esto a cero)
.d1:
        mov ecx, [bmp_fin]
        sub ecx, [bmp_dat]
        cmp eax, ecx
        ja .roto
        add eax, [bmp_dat]
        mov [bmp_px], eax
        mov eax, [im_an]
        imul eax, [bmp_bits]
        add eax, 31
        shr eax, 5
        shl eax, 2
        mov [bmp_paso], eax
        mov eax, [bmp_comp]
        dec eax
        cmp eax, 1
        ja .bien
        mov eax, [im_an]            ; con RLE: toda la imagen, a un octeto por punto
        imul eax, [im_al]
        mov [im_alm], eax
.bien:
        xor eax, eax
        ret
.roto:
        mov eax, ME_ROTO
        ret
.grande:
        mov eax, ME_GRANDE
        ret
.variante:
        mov eax, ME_VARIANTE
        ret

; ---- un BMP con RLE8 o RLE4, entero, a la memoria de trabajo: las filas en
;      el orden del archivo ----
bmp_prepara:
        mov eax, [bmp_comp]
        dec eax
        cmp eax, 1
        ja .bien
        mov edi, [im_almp]
        mov ecx, [im_alm]
        xor al, al
        rep stosb
        xor eax, eax
        mov [bmp_x], eax
        mov [bmp_y], eax
        mov esi, [bmp_px]
.par:
        mov eax, [bmp_fin]          ; (con signo: el relleno de un trozo impar
        sub eax, esi                ; puede haber pasado del final)
        cmp eax, 2
        jl .corto
        movzx ecx, byte [esi]
        movzx eax, byte [esi+1]
        add esi, 2
        test ecx, ecx
        jz .escape
        ; una tirada: ECX puntos del color AL (o de sus dos mitades, alternadas)
        cmp dword [bmp_comp], 2
        je .t4
.t8:
        call .pon
        dec ecx
        jnz .t8
        jmp .par
.t4:
        mov ah, al
        shr al, 4
        and ah, 15
.t4b:
        call .pon
        xchg al, ah
        dec ecx
        jnz .t4b
        jmp .par
.escape:
        test eax, eax               ; fin de la fila
        jnz .e1
        mov dword [bmp_x], 0
        inc dword [bmp_y]
        jmp .par
.e1:
        cmp eax, 1                  ; fin de la imagen
        je .bien
        cmp eax, 2                  ; un salto
        jne .abs
        mov eax, [bmp_fin]
        sub eax, esi
        cmp eax, 2
        jb .corto
        movzx eax, byte [esi]
        add [bmp_x], eax
        movzx eax, byte [esi+1]
        add [bmp_y], eax
        add esi, 2
        jmp .par
.abs:
        ; EAX puntos tal cual (rellenos hasta un número par de octetos)
        mov ecx, eax
        cmp dword [bmp_comp], 2
        je .a4
        mov edx, [bmp_fin]
        sub edx, esi
        cmp edx, ecx
        jb .corto
        mov edx, ecx
.a8:
        lodsb
        call .pon
        dec ecx
        jnz .a8
        test dl, 1
        jz .par
        inc esi
        jmp .par
.a4:
        lea ebx, [ecx+1]
        shr ebx, 1                  ; EBX=octetos
        mov edx, [bmp_fin]
        sub edx, esi
        cmp edx, ebx
        jb .corto
        push ebx
.a4b:
        mov al, [esi]
        shr al, 4
        call .pon
        dec ecx
        jz .a4c
        lodsb
        and al, 15
        call .pon
        dec ecx
        jnz .a4b
        dec esi
.a4c:
        inc esi
        pop ebx
        test bl, 1
        jz .par
        inc esi
        jmp .par
.corto:
        mov eax, [bmp_y]            ; se acaba sin decir «fin»: si falta imagen, cortado
        inc eax
        cmp eax, [im_al]
        jae .bien
        mov dword [im_aviso], 1
.bien:
        xor eax, eax
        ret
; (interna) el punto AL, donde toca; lo que se sale de la imagen no se pone
.pon:
        push ebx
        mov ebx, [bmp_x]
        cmp ebx, [im_an]
        jae .p9
        mov ebx, [bmp_y]
        cmp ebx, [im_al]
        jae .p9
        imul ebx, [im_an]
        add ebx, [bmp_x]
        add ebx, [im_almp]
        mov [ebx], al
.p9:
        inc dword [bmp_x]
        pop ebx
        ret

bmp_fila:
        cmp dword [bmp_abajo], 0    ; EAX=la fila del archivo
        jne .f1
        neg eax
        add eax, [im_al]
        dec eax
.f1:
        mov ecx, [bmp_comp]
        dec ecx
        cmp ecx, 1
        ja .sin
        imul eax, [im_an]           ; con RLE: ya está hecha
        mov esi, [im_almp]
        add esi, eax
        mov ecx, [im_an]
        rep movsb
        clc
        ret
.sin:
        imul eax, [bmp_paso]
        mov esi, [bmp_px]
        add esi, eax
        mov eax, [im_an]            ; ¿está entera en el archivo?
        imul eax, [bmp_bits]
        add eax, 7
        shr eax, 3
        add eax, esi
        cmp eax, [bmp_fin]
        ja .falta
        cmp esi, [bmp_px]
        jb .falta
        mov eax, [bmp_bits]
        cmp eax, 8
        jb .bits
        je .ocho
        cmp eax, 24
        je .v24
        cmp eax, 16
        je .v16
        ; ---- 32 bits ----
        mov ebp, [im_an]
.v32:
        lodsd
        call .color
        dec ebp
        jnz .v32
        clc
        ret
.v16:
        mov ebp, [im_an]
.v16b:
        xor eax, eax
        lodsw
        call .color
        dec ebp
        jnz .v16b
        clc
        ret
.v24:
        mov ecx, [im_an]
.v24b:
        mov al, [esi+2]
        stosb
        mov al, [esi+1]
        stosb
        mov al, [esi]
        stosb
        add esi, 3
        dec ecx
        jnz .v24b
        clc
        ret
.ocho:
        mov ecx, [im_an]
        rep movsb
        clc
        ret
.bits:
        mov cl, al
        mov edx, [im_an]
        call mi_desempaqueta
        clc
        ret
.falta:
        stc
        ret
; (interna) EAX=un punto de 16 o 32 bits: sus componentes, por las máscaras
.color:
        push esi
        mov esi, eax
        mov ebx, bmp_can
        mov edx, 3
        cmp dword [im_clase], IC_RGBA
        jne .co1
        inc edx
.co1:
        push edx
        mov eax, esi
        and eax, [ebx]
        mov ecx, [ebx+4]
        shr eax, cl
        mov ecx, [ebx+8]
        cmp ecx, 255
        je .co2
        test ecx, ecx
        jz .co2
        imul eax, 255
        xor edx, edx
        div ecx
.co2:
        stosb
        pop edx
        add ebx, 12
        dec edx
        jnz .co1
        pop esi
        ret
