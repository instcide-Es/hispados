; =============================================================================
;  MIRADOR.HVP - el formato PCX (PC Paintbrush): el de los programas de DOS
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  - un plano de un bit: blanco y negro;
;  - dos, tres o cuatro planos de un bit: hasta 16 colores (los de la EGA),
;    con la paleta de la cabecera;
;  - un plano de 2 o de 4 bits: 4 o 16 colores, con la paleta de la cabecera;
;  - un plano de 8 bits: 256 colores con la paleta del final del archivo (o
;    256 grises, si no la lleva);
;  - tres planos de 8 bits: color verdadero.
;  Con las tiradas de siempre (RLE) o sin comprimir. Se descomprime fila a
;  fila, según se piden: en memoria solo hay una fila con sus planos.
; =============================================================================
%macro PCX_BSS 0
            alignb 4
pcx_p       resd 1                  ; por dónde se va leyendo
pcx_fin     resd 1
pcx_bits    resd 1
pcx_planos  resd 1
pcx_opl     resd 1                  ; octetos por fila de un plano
pcx_rle     resd 1
pcx_rep     resd 1                  ; lo que queda de una tirada a medias
pcx_val     resd 1
%endmacro

pcx_t_var   db "un PCX con una combinación de planos y bits que Mirador no conoce", 0

pcx_detecta:
        cmp ecx, 129
        jb .no
        cmp byte [esi], 10
        jne .no
        cmp byte [esi+1], 5
        ja .no
        cmp byte [esi+2], 1
        ja .no
        mov al, [esi+3]
        cmp al, 1
        je .b
        cmp al, 2
        je .b
        cmp al, 4
        je .b
        cmp al, 8
        jne .no
.b:
        mov al, [esi+65]
        dec al
        cmp al, 3
        ja .no
        mov ax, [esi+8]
        cmp ax, [esi+4]
        jb .no
        mov ax, [esi+10]
        cmp ax, [esi+6]
        jb .no
        cmp word [esi+66], 0
        je .no
        clc
        ret
.no:
        stc
        ret

pcx_abre:
        lea eax, [esi+ecx]
        mov [pcx_fin], eax
        lea eax, [esi+128]
        mov [pcx_p], eax
        movzx eax, word [esi+8]
        movzx edx, word [esi+4]
        sub eax, edx
        inc eax
        mov [im_an], eax
        movzx eax, word [esi+10]
        movzx edx, word [esi+6]
        sub eax, edx
        inc eax
        mov [im_al], eax
        call mi_medidas_ok
        jc .grande
        movzx eax, byte [esi+2]
        mov [pcx_rle], eax
        movzx eax, byte [esi+3]
        mov [pcx_bits], eax
        movzx ebx, byte [esi+65]
        mov [pcx_planos], ebx
        movzx edx, word [esi+66]
        mov [pcx_opl], edx
        mov dword [pcx_rep], 0
        imul eax, ebx
        mov [im_bits], eax
        mov eax, [im_an]            ; ¿caben los puntos en lo que dice que mide un plano?
        imul eax, [pcx_bits]
        add eax, 7
        shr eax, 3
        cmp eax, edx
        ja .roto
        imul edx, ebx
        mov [im_alm], edx           ; una fila con todos sus planos
        mov eax, [pcx_bits]
        mov dword [im_clase], IC_PALETA
        cmp eax, 8
        je .ocho
        cmp eax, 1
        je .uno
        cmp ebx, 1                  ; 2 o 4 bits: un solo plano
        jne .variante
.pal16:
        mov ecx, [im_bits]
        mov eax, 1
        shl eax, cl
        mov [im_ncol], eax
        mov ecx, eax
        lea edx, [esi+16]           ; la paleta de la cabecera
        xor eax, eax
.p1:
        mov bl, [edx]
        mov [im_pal+eax*4], bl
        mov bl, [edx+1]
        mov [im_pal+eax*4+1], bl
        mov bl, [edx+2]
        mov [im_pal+eax*4+2], bl
        add edx, 3
        inc eax
        dec ecx
        jnz .p1
        jmp .bien
.uno:
        cmp ebx, 1
        jne .pal16
        mov dword [im_pal+4], 0xFFFFFFFF    ; el 0, negro; el 1, blanco
        mov dword [im_ncol], 2
        jmp .bien
.ocho:
        cmp ebx, 3
        je .rgb
        cmp ebx, 1
        jne .variante
        ; 256 colores: la paleta, al final, detrás de un 12
        mov dword [im_clase], IC_GRIS
        mov eax, [pcx_fin]
        sub eax, 769
        cmp eax, [pcx_p]
        jb .sin_paleta
        cmp byte [eax], 12
        jne .sin_paleta
        mov [pcx_fin], eax          ; (lo de la imagen acaba ahí)
        mov dword [im_clase], IC_PALETA
        mov dword [im_ncol], 256
        lea edx, [eax+1]
        xor eax, eax
.p2:
        mov bl, [edx]
        mov [im_pal+eax*4], bl
        mov bl, [edx+1]
        mov [im_pal+eax*4+1], bl
        mov bl, [edx+2]
        mov [im_pal+eax*4+2], bl
        add edx, 3
        inc eax
        cmp eax, 256
        jb .p2
        jmp .bien
.sin_paleta:
        cmp byte [esi+1], 5         ; uno de la versión 5 sin su paleta: le falta el final
        jb .bien
        mov dword [im_aviso], 1
        jmp .bien
.rgb:
        mov dword [im_clase], IC_RGB
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
        mov dword [im_var], pcx_t_var
        mov eax, ME_VARIANTE
        ret

pcx_fila:
        push edi
        ; ---- la fila, con todos sus planos, a la memoria de trabajo ----
        mov edi, [im_almp]
        mov ecx, [pcx_opl]
        imul ecx, [pcx_planos]
        mov esi, [pcx_p]
        mov edx, ecx                ; EDX=para saber si ha salido algo
        mov ebx, [pcx_rep]
        mov al, [pcx_val]
.o:
        test ebx, ebx
        jz .lee
        stosb                       ; la tirada que venía
        dec ebx
        dec ecx
        jnz .o
        jmp .llena
.lee:
        cmp esi, [pcx_fin]
        jae .corta
        lodsb
        cmp dword [pcx_rle], 0
        je .tal
        mov ah, al
        and ah, 0xC0
        cmp ah, 0xC0
        jne .tal
        movzx ebx, al               ; dos bits a 1: los otros seis, cuántas veces
        and ebx, 0x3F
        cmp esi, [pcx_fin]
        jae .corta
        lodsb
        jmp .o
.tal:
        stosb
        dec ecx
        jnz .lee
.llena:
        mov [pcx_rep], ebx
        mov [pcx_val], al
        mov [pcx_p], esi
        jmp .convierte
.corta:
        mov [pcx_p], esi
        mov dword [pcx_rep], 0
        cmp ecx, edx                ; ni un octeto de esta fila: no está
        je .falta
        mov dword [im_aviso], 1
        xor al, al                  ; a medias: lo que falta, a cero
        rep stosb
.convierte:
        pop edi
        mov esi, [im_almp]
        mov eax, [pcx_bits]
        cmp eax, 8
        je .c8
        cmp dword [pcx_planos], 1
        jne .planos
        mov cl, al
        mov edx, [im_an]
        call mi_desempaqueta
        clc
        ret
.c8:
        mov ecx, [im_an]
        cmp dword [pcx_planos], 3
        je .c24
        rep movsb
        clc
        ret
.c24:
        mov edx, [pcx_opl]
.c24b:
        mov al, [esi]
        stosb
        mov al, [esi+edx]
        stosb
        mov al, [esi+edx*2]
        stosb
        inc esi
        dec ecx
        jnz .c24b
        clc
        ret
.planos:
        ; cada plano da un bit del índice: el primero, el de menos peso
        xor ecx, ecx                ; ECX=x
.pl1:
        mov ebx, [pcx_planos]
        xor eax, eax                ; EAX=el índice
        mov edx, ecx
        shr edx, 3
        mov ebp, [pcx_opl]
        imul ebp, ebx
        add edx, ebp                ; (desde el último plano hacia el primero)
.pl2:
        sub edx, [pcx_opl]
        movzx ebp, byte [esi+edx]
        push ecx
        not ecx
        and ecx, 7
        shr ebp, cl
        pop ecx
        and ebp, 1
        lea eax, [ebp+eax*2]
        dec ebx
        jnz .pl2
        stosb
        inc ecx
        cmp ecx, [im_an]
        jb .pl1
        clc
        ret
.falta:
        pop edi
        stc
        ret
