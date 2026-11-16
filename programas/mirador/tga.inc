; =============================================================================
;  MIRADOR.HVP - el formato TGA (Truevision Targa)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Con paleta (de 15, 16, 24 o 32 bits por color), en color directo de 15,
;  16, 24 y 32 bits (el de 32, con su opacidad si la cabecera dice que la
;  lleva) y en grises de 8 bits; sin comprimir y con tiradas (RLE); de abajo
;  arriba (lo corriente) o de arriba abajo, y de izquierda a derecha o al
;  revés. No: índices de 16 bits ni grises con opacidad (se dice).
;
;  El TGA no tiene firma: se reconoce porque su cabecera tiene sentido (por
;  eso es el último formato que se prueba). Sin comprimir, cada fila se toma
;  de su sitio; con RLE se descomprime entero a la memoria de trabajo.
; =============================================================================
%macro TGA_BSS 0
            alignb 4
tga_dat     resd 1
tga_fin     resd 1
tga_tipo    resd 1                  ; 1 paleta, 2 color, 3 grises
tga_rle     resd 1
tga_o       resd 1                  ; octetos por punto
tga_desc    resd 1
tga_px      resd 1                  ; los puntos, sin comprimir
tga_px_fin  resd 1
%endmacro

tga_t_var   db "un TGA con índices de 16 bits o con grises y opacidad", 0

tga_detecta:
        cmp ecx, 19
        jb .no
        cmp byte [esi+1], 1
        ja .no
        mov al, [esi+2]
        and al, ~8
        dec al
        cmp al, 2
        ja .no
        cmp byte [esi+2], 11
        ja .no
        mov al, [esi+16]
        cmp al, 8
        je .b
        cmp al, 15
        je .b
        cmp al, 16
        je .b
        cmp al, 24
        je .b
        cmp al, 32
        jne .no
.b:
        cmp word [esi+12], 0
        je .no
        cmp word [esi+14], 0
        je .no
        test byte [esi+17], 0xC0
        jnz .no
        mov al, [esi+2]
        and al, 7
        cmp al, 1                   ; con paleta: tiene que haberla
        jne .b1
        cmp byte [esi+1], 1
        jne .no
        jmp .b2
.b1:
        cmp al, 2                   ; color directo: no de 8 bits
        jne .b2
        cmp byte [esi+16], 8
        je .no
.b2:
        cmp byte [esi+1], 1         ; los colores de la paleta, de un tamaño que exista
        jne .si
        mov al, [esi+7]
        cmp al, 15
        je .si
        cmp al, 16
        je .si
        cmp al, 24
        je .si
        cmp al, 32
        jne .no
.si:
        clc
        ret
.no:
        stc
        ret

; (interna) ESI=un color de EBX octetos (2, 3 o 4), como va en el archivo ->
; EAX=rojo, verde, azul y, arriba, la opacidad que lleve (en los de 4)
tga_color:
        cmp ebx, 2
        jne .c3
        movzx eax, word [esi]
        push ecx
        push edx
        mov ecx, eax
        and ecx, 31
        movzx edx, byte [mi_cinco+ecx]      ; azul
        shl edx, 16
        mov ecx, eax
        shr ecx, 5
        and ecx, 31
        mov dh, [mi_cinco+ecx]              ; verde
        shr eax, 10
        and eax, 31
        mov dl, [mi_cinco+eax]              ; rojo
        mov eax, edx
        or eax, 0xFF000000
        pop edx
        pop ecx
        ret
.c3:
        push edx
        movzx eax, byte [esi]               ; azul
        shl eax, 16
        mov ah, [esi+1]
        mov al, [esi+2]
        mov edx, 0xFF000000
        cmp ebx, 4
        jne .c4
        movzx edx, byte [esi+3]
        shl edx, 24
.c4:
        or eax, edx
        pop edx
        ret

tga_abre:
        mov [tga_dat], esi
        lea eax, [esi+ecx]
        mov [tga_fin], eax
        movzx eax, word [esi+12]
        mov [im_an], eax
        movzx eax, word [esi+14]
        mov [im_al], eax
        call mi_medidas_ok
        jc .grande
        movzx eax, byte [esi+2]
        mov ebx, eax
        shr ebx, 3
        mov [tga_rle], ebx
        and eax, 7
        mov [tga_tipo], eax
        movzx eax, byte [esi+17]
        mov [tga_desc], eax
        movzx eax, byte [esi+16]
        mov [im_bits], eax
        add eax, 7
        shr eax, 3
        mov [tga_o], eax
        movzx edi, byte [esi]
        lea edi, [esi+edi+18]       ; EDI=detrás de la cabecera y del nombre
        cmp byte [esi+1], 1
        jne .sin_pal
        ; la paleta: se salta siempre; se lee si la imagen la usa
        movzx ecx, word [esi+5]     ; ECX=cuántos colores
        movzx ebx, byte [esi+7]
        add ebx, 7
        shr ebx, 3                  ; EBX=octetos de cada uno
        mov eax, ecx
        imul eax, ebx
        add eax, edi
        cmp eax, [tga_fin]
        ja .roto
        push eax
        cmp dword [tga_tipo], 1
        jne .p9
        mov [im_ncol], ecx
        movzx edx, word [esi+3]     ; EDX=el índice del primero
        push esi
        mov esi, edi
.p1:
        test ecx, ecx
        jz .p8
        cmp edx, 256
        jae .p8
        call tga_color
        or eax, 0xFF000000          ; (la opacidad de la paleta no se usa)
        mov [im_pal+edx*4], eax
        add esi, ebx
        inc edx
        dec ecx
        jmp .p1
.p8:
        pop esi
.p9:
        pop edi
.sin_pal:
        mov [tga_px], edi
        mov eax, [tga_fin]
        mov [tga_px_fin], eax
        cmp edi, eax
        ja .roto
        ; la clase
        mov eax, [tga_tipo]
        mov ebx, [tga_o]
        cmp eax, 1
        jne .c2
        mov dword [im_clase], IC_PALETA
        cmp ebx, 1
        jne .variante
        jmp .clase_ok
.c2:
        cmp eax, 3
        jne .c3
        mov dword [im_clase], IC_GRIS
        cmp ebx, 1
        jne .variante
        jmp .clase_ok
.c3:
        mov dword [im_clase], IC_RGB
        cmp ebx, 4
        jne .clase_ok
        test byte [tga_desc], 15    ; 32 bits: con opacidad si dice que la lleva
        jz .clase_ok
        mov dword [im_clase], IC_RGBA
        mov dword [im_hay_alfa], 1
.clase_ok:
        cmp dword [tga_rle], 0
        je .bien
        mov eax, [im_an]
        imul eax, [im_al]
        imul eax, ebx
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
        mov dword [im_var], tga_t_var
        mov eax, ME_VARIANTE
        ret

; ---- un TGA con RLE, entero, a la memoria de trabajo ----
tga_prepara:
        cmp dword [tga_rle], 0
        je .bien
        mov esi, [tga_px]
        mov edi, [im_almp]
        mov [tga_px], edi
        mov ebp, [im_an]
        imul ebp, [im_al]           ; EBP=puntos que faltan
        mov ebx, [tga_o]
        mov eax, ebp
        imul eax, ebx
        add eax, edi
        mov [tga_px_fin], eax
.paq:
        test ebp, ebp
        jz .bien
        cmp esi, [tga_fin]
        jae .corto
        movzx eax, byte [esi]
        inc esi
        mov edx, eax
        and edx, 127
        inc edx                     ; EDX=puntos del paquete
        cmp edx, ebp
        jbe .p1
        mov edx, ebp
.p1:
        test al, 0x80
        jz .tal
        mov eax, [tga_fin]          ; un punto, repetido
        sub eax, esi
        cmp eax, ebx
        jb .corto
        sub ebp, edx
.r1:
        mov ecx, ebx
        push esi
        rep movsb
        pop esi
        dec edx
        jnz .r1
        add esi, ebx
        jmp .paq
.tal:
        mov eax, [tga_fin]          ; puntos tal cual: los que haya enteros
        sub eax, esi
        push edx
        xor edx, edx
        div ebx
        pop edx
        cmp eax, edx
        jae .t1
        mov ecx, eax
        imul ecx, ebx
        sub ebp, eax
        rep movsb
        jmp .corto
.t1:
        sub ebp, edx
        mov ecx, edx
        imul ecx, ebx
        rep movsb
        jmp .paq
.corto:
        mov dword [im_aviso], 1     ; lo que falta, a cero
        mov ecx, ebp
        imul ecx, ebx
        xor al, al
        rep stosb
.bien:
        xor eax, eax
        ret

tga_fila:
        test byte [tga_desc], 0x20  ; ¿la primera fila es la de arriba?
        jnz .f1
        neg eax
        add eax, [im_al]
        dec eax
.f1:
        mov ebx, [tga_o]
        mov ecx, [im_an]
        imul ecx, ebx               ; ECX=octetos de una fila
        imul eax, ecx
        mov esi, [tga_px]
        add esi, eax
        lea eax, [esi+ecx]
        cmp eax, [tga_px_fin]
        ja .falta
        push edi
        cmp ebx, 1
        jne .color
        rep movsb
        jmp .hecha
.color:
        mov ecx, [im_an]
        mov edx, [im_clase]
.c1:
        call tga_color
        stosw
        shr eax, 16
        stosb
        cmp edx, IC_RGBA
        jne .c2
        mov al, ah
        stosb
.c2:
        add esi, ebx
        dec ecx
        jnz .c1
.hecha:
        pop edi
        test byte [tga_desc], 0x10  ; de derecha a izquierda: se le da la vuelta
        jz .bien
        mov ebx, 1                  ; EBX=octetos de un punto ya convertido
        mov eax, [im_clase]
        cmp eax, IC_RGB
        jb .v0
        mov ebx, 3
        cmp eax, IC_RGBA
        jne .v0
        inc ebx
.v0:
        mov esi, edi
        mov eax, [im_an]
        dec eax
        imul eax, ebx
        add edi, eax                ; ESI=el primero EDI=el último
.v1:
        cmp esi, edi
        jae .bien
        xor ecx, ecx
.v2:
        mov al, [esi+ecx]
        xchg al, [edi+ecx]
        mov [esi+ecx], al
        inc ecx
        cmp ecx, ebx
        jb .v2
        add esi, ebx
        sub edi, ebx
        jmp .v1
.bien:
        clc
        ret
.falta:
        stc
        ret
