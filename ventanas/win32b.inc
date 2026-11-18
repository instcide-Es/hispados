; =============================================================================
;  Hispa Ventanas - [HISPA-486] GDI32 de la fase 3d: mapas de bits y DIB
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un mapa de bits (HBITMAP) es un octeto por punto con los colores de la
;  paleta de Hispa Ventanas (o 0 y 1, si es monocromo); los de hasta
;  MAPA_MONTON octetos, del montón; los demás, de un bloque de DPMI apuntado
;  en el programa (PE_BLQS), que se suelta al acabar. Los DIB de
;  CreateDIBSection tienen además sus puntos en su formato (los que escribe
;  el programa): al pintar en ellos con GDI se pasan al mapa y vuelven
;  (wg_dib_a_mapa, wg_mapa_a_dib); al copiarlos se leen directamente.
;    wg_blt      la copia de bloques de todo (BitBlt, StretchBlt, PatBlt,
;                StretchDIBits...): con estiramiento, espejo y ROP3
;    wg_dib_desc ESI=BITMAPINFO EBX=puntos ECX=uso EDI=ficha DB_* -> CF=1 si
;                no se sabe leer
;    wg_dib_fila EDI=ficha EBX=fila (de arriba abajo) -> ESI=wg_fila (la
;                paleta de Hispa Ventanas)
; =============================================================================

; EAX: los octetos (a, b, c, x) -> (c, b, a, 0): de RGBQUAD a COLORREF y
; al revés (sin bswap, que es del 486)
%macro VUELTA_RGB 0
        push edx
        movzx edx, al
        shl edx, 16
        mov dh, ah
        shr eax, 16
        mov dl, al
        mov eax, edx
        pop edx
%endmacro

; ---- memoria para un mapa: ECX=octetos EDX=programa -> EAX=puntero
; EBX=mango (0: del montón); CF=1 si no hay. Puesta a cero ----
wg_memoria:
        cmp ecx, MAPA_MONTON
        ja .dpmi
        call w32_pedir
        jc .x
        xor ebx, ebx
.x:
        ret
.dpmi:
        push ecx
        push edi
        lea edi, [edx+PE_BLQS]
        mov ebx, WG_BLQ_MAX
.b:
        cmp dword [edi], 0
        je .hay
        add edi, 4
        dec ebx
        jnz .b
        pop edi
        pop ecx
        stc
        ret
.hay:
        push edx
        call mem_bloque             ; ECX -> EAX, EDX=mango
        mov ebx, edx
        pop edx
        jc .no
        mov [edi], ebx
        push eax
        mov edi, eax
        push ecx
        xor eax, eax
        shr ecx, 2
        inc ecx
        rep stosd
        pop ecx
        pop eax
        pop edi
        pop ecx
        clc
        ret
.no:
        pop edi
        pop ecx
        stc
        ret
; ---- EAX=puntero EBX=mango EDX=programa: la suelta ----
wg_memoria_soltar:
        test ebx, ebx
        jnz .dpmi
        jmp mem_soltar
.dpmi:
        pushad
        lea edi, [edx+PE_BLQS]
        mov ecx, WG_BLQ_MAX
.b:
        cmp [edi], ebx
        je .s
        add edi, 4
        loop .b
        jmp .x
.s:
        mov dword [edi], 0
        mov edx, ebx
        call mem_bloque_soltar
.x:
        popad
        ret
; ---- (pe_limpiar) suelta los bloques de DPMI del programa EDX ----
wg_limpiar:
        pushad
        lea edi, [edx+PE_BLQS]
        mov ecx, WG_BLQ_MAX
.b:
        mov edx, [edi]
        test edx, edx
        jz .s
        mov dword [edi], 0
        call mem_bloque_soltar
.s:
        add edi, 4
        loop .b
        popad
        ret
; ---- un mapa nuevo del programa EDX: ECX=ancho EBX=alto AL=1 monocromo ->
; EAX=el objeto ESI=su ficha; CF=1 si no hay sitio ----
wg_mapa_nuevo:
        push ecx
        push ebx
        push edi
        test ecx, ecx
        jle .uno
        test ebx, ebx
        jg .t
.uno:
        mov ecx, 1                  ; (0 × 0: un punto monocromo, como Windows)
        mov ebx, 1
        mov al, 1
.t:
        cmp ecx, 8192
        ja .no
        cmp ebx, 8192
        ja .no
        mov edi, ecx
        imul edi, ebx
        cmp edi, 0x2000000
        ja .no
        push eax
        call g_nuevo                ; ESI=ficha EAX=objeto
        mov edi, eax
        pop eax
        jc .no
        mov byte [esi+GO_TIPO], GT_MAPA
        and al, 1
        mov [esi+GO_ESTILO], al
        mov word [esi+GO_ANCHO], 8
        test al, al
        jz .c
        mov word [esi+GO_ANCHO], 1
.c:
        mov [esi+GO_B], ecx
        mov [esi+GO_C], ebx
        mov dword [esi+GO_E], 0
        imul ecx, ebx
        push ebx
        call wg_memoria
        mov [esi+GO_D], ebx
        pop ebx
        jc .sin
        mov [esi+GO_A], eax
        mov eax, edi
        pop edi
        pop ebx
        pop ecx
        clc
        ret
.sin:
        mov byte [esi+GO_TIPO], 0
.no:
        pop edi
        pop ebx
        pop ecx
        xor eax, eax
        stc
        ret
; ---- suelta lo de la ficha ESI (un objeto) del programa EDX ----
wg_obj_soltar:
        pushad
        movzx ecx, byte [esi+GO_TIPO]
        cmp ecx, GT_MAPA
        jne .r
        mov eax, [esi+GO_A]
        mov ebx, [esi+GO_D]
        call wg_memoria_soltar
        mov edi, [esi+GO_E]
        test edi, edi
        jz .x
        mov eax, [edi+DB_BITS]
        mov ebx, [edi+DB_MANGO]
        call wg_memoria_soltar
        mov eax, edi
        call mem_soltar
        jmp .x
.r:
        cmp ecx, GT_REGION
        je .bloque
        cmp ecx, GT_PALETA
        je .bloque
        cmp ecx, GT_ICONO
        je .bloque
        cmp ecx, GT_CURSOR
        je .bloque
        cmp ecx, GT_PINCEL
        jne .x
        cmp byte [esi+GO_ESTILO], 3
        jne .x
.bloque:
        test byte [esi+GO_ESTILO], 0x80 ; (de serie: no)
        jnz .x
        mov eax, [esi+GO_A]
        call mem_soltar
.x:
        mov byte [esi+GO_TIPO], 0
        popad
        ret

; =============================================================================
;  Los DIB
; =============================================================================
; ---- ESI=BITMAPINFO EBX=sus puntos (de Hispa Ventanas) ECX=uso (0 RGB, 1
; índices de la paleta del contexto [wg_dc]) EDI=la ficha que se rellena;
; CF=1 si no es de los que se saben leer ----
wg_dib_desc:
        pushad
        mov [edi+DB_BITS], ebx
        mov eax, [esi+4]
        test eax, eax
        jle .no
        cmp eax, 8192
        ja .no
        mov [edi+DB_AN], eax
        mov eax, [esi+8]
        mov dword [edi+DB_ABAJO], 1
        test eax, eax
        jns .h
        neg eax
        mov dword [edi+DB_ABAJO], 0
.h:
        test eax, eax
        jz .no
        mov [edi+DB_AL], eax
        mov [edi+DB_N], eax
        mov dword [edi+DB_INI], 0
        movzx eax, word [esi+14]
        mov [edi+DB_BPP], eax
        mov edx, [esi+16]           ; BI_RGB, o BI_BITFIELDS en 16 y 32
        mov dword [edi+DB_565], 0
        test edx, edx
        jz .c
        cmp edx, 3
        jne .no
        cmp eax, 16
        jb .no
        mov ebp, [esi]
        cmp dword [esi+ebp+4], 0x7E0    ; (la máscara del verde: 5-6-5)
        jne .c
        mov dword [edi+DB_565], 1
.c:
        cmp eax, 1
        je .bpp
        cmp eax, 4
        je .bpp
        cmp eax, 8
        je .bpp
        cmp eax, 16
        je .bpp
        cmp eax, 24
        je .bpp
        cmp eax, 32
        jne .no
.bpp:
        mov edx, [edi+DB_AN]        ; el paso: de 4 en 4 octetos
        imul edx, eax
        add edx, 31
        shr edx, 5
        shl edx, 2
        mov [edi+DB_PASO], edx
        mov dword [edi+DB_INV], 0
        cmp eax, 8
        ja .x
        mov edx, [esi+32]           ; los colores: biClrUsed o todos
        mov ebp, 1
        push ecx
        mov cl, al
        shl ebp, cl
        pop ecx
        test edx, edx
        jz .n
        cmp edx, ebp
        jbe .n2
.n:
        mov edx, ebp
.n2:
        mov ebp, [esi]
        add ebp, esi                ; EBP = la tabla
        push dword [wg_mono]
        mov byte [wg_mono], 0
        xor ebx, ebx
.col:
        cmp ebx, edx
        jae .cf
        test ecx, ecx
        jnz .pal
        mov eax, [ebp+ebx*4]        ; RGBQUAD: B, G, R
        VUELTA_RGB
        jmp .pon
.pal:
        movzx eax, word [ebp+ebx*2]
        or eax, 0x01000000
        push edi
        mov edi, [wg_dc]
        push edx
        mov edx, [wg_prog]
        call wg_colref
        pop edx
        pop edi
.pon:
        mov [edi+DB_COLORES+ebx*4], eax
        call wg_idx
        mov [edi+DB_XLAT+ebx], al
        inc ebx
        jmp .col
.cf:
        cmp ebx, 256                ; (los que no tiene: negro)
        jae .cf2
        mov dword [edi+DB_COLORES+ebx*4], 0
        xor eax, eax
        call wg_idx
        mov [edi+DB_XLAT+ebx], al
        inc ebx
        jmp .cf
.cf2:
        pop eax
        mov [wg_mono], al
.x:
        popad
        clc
        ret
.no:
        popad
        stc
        ret
; ---- (interna) EDI=ficha EBX=fila de arriba abajo -> ESI=dónde empieza en
; sus puntos (CF=1 si no la tiene) ----
wg_dib_dir:
        push eax
        push edx
        mov eax, ebx
        cmp dword [edi+DB_ABAJO], 0
        je .a
        mov eax, [edi+DB_AL]        ; de abajo arriba: la línea AL−1−fila
        dec eax
        sub eax, ebx
.a:
        sub eax, [edi+DB_INI]
        jb .no
        cmp eax, [edi+DB_N]
        jae .no
        imul eax, [edi+DB_PASO]
        mov esi, [edi+DB_BITS]
        add esi, eax
        pop edx
        pop eax
        clc
        ret
.no:
        pop edx
        pop eax
        stc
        ret
; ---- EDI=ficha EBX=fila -> ESI=wg_fila con sus colores (de la paleta de
; Hispa Ventanas) ----
wg_dib_fila:
        pushad
        mov ecx, [edi+DB_AN]
        cmp ecx, WG_FILA
        jbe .w
        mov ecx, WG_FILA
.w:
        call wg_dib_dir
        jnc .hay
        push edi                    ; (fuera del DIB: negro)
        mov edi, wg_fila
        xor eax, eax
        rep stosb
        pop edi
        jmp .x
.hay:
        push dword [wg_mono]
        mov byte [wg_mono], 0
        mov ebp, wg_fila
        xor edx, edx                ; x
        mov eax, [edi+DB_BPP]
        cmp eax, 8
        je .b8
        cmp eax, 4
        je .b4
        cmp eax, 1
        je .b1
        cmp eax, 16
        je .b16
        cmp eax, 24
        je .b24
.b32:
        mov eax, [esi]
        add esi, 4
        call .bgr
        dec ecx
        jnz .b32
        jmp .f
.b24:
        mov eax, [esi]
        add esi, 3
        call .bgr
        dec ecx
        jnz .b24
        jmp .f
.b16:
        movzx eax, word [esi]
        add esi, 2
        push ecx
        push ebx
        cmp dword [edi+DB_565], 0
        jne .b565
        mov ebx, eax                ; 5-5-5: xRRRRRGG GGGBBBBB
        shr ebx, 10
        and ebx, 31
        lea ebx, [ebx*8]
        mov ecx, eax
        shr ecx, 5
        and ecx, 31
        shl ecx, 3
        and eax, 31
        shl eax, 3
        jmp .b16c
.b565:
        mov ebx, eax
        shr ebx, 11
        and ebx, 31
        shl ebx, 3
        mov ecx, eax
        shr ecx, 5
        and ecx, 63
        shl ecx, 2
        and eax, 31
        shl eax, 3
.b16c:
        shl eax, 16                 ; 00BBGGRR
        shl ecx, 8
        or eax, ecx
        or eax, ebx
        call wg_idx
        mov [ebp], al
        inc ebp
        pop ebx
        pop ecx
        dec ecx
        jnz .b16
        jmp .f
.b8:
        movzx eax, byte [esi]
        inc esi
        mov al, [edi+DB_XLAT+eax]
        mov [ebp], al
        inc ebp
        dec ecx
        jnz .b8
        jmp .f
.b4:
        movzx eax, byte [esi]
        test edx, 1
        jz .b4a
        inc esi
        and eax, 15
        jmp .b4b
.b4a:
        shr eax, 4
.b4b:
        mov al, [edi+DB_XLAT+eax]
        mov [ebp], al
        inc ebp
        inc edx
        dec ecx
        jnz .b4
        jmp .f
.b1:
        movzx eax, byte [esi]
        push ecx
        mov ecx, edx
        and ecx, 7
        shl eax, cl
        pop ecx
        shr eax, 7
        and eax, 1
        mov al, [edi+DB_XLAT+eax]
        mov [ebp], al
        inc ebp
        inc edx
        test edx, 7
        jnz .b1s
        inc esi
.b1s:
        dec ecx
        jnz .b1
.f:
        pop eax
        mov [wg_mono], al
.x:
        popad
        mov esi, wg_fila
        ret
.bgr:                               ; EAX = B, G, R (y algo) -> a la fila
        VUELTA_RGB                  ; -> 00BBGGRR
        call wg_idx
        mov [ebp], al
        inc ebp
        ret
; ---- (interna) EDI=ficha: DB_CERCA, el color suyo más cercano a cada uno
; de la paleta (para los DIB de 1, 4 y 8 bits) ----
wg_dib_cerca:
        cmp dword [edi+DB_INV], 0
        jne .r
        pushad
        mov ecx, [edi+DB_BPP]
        mov ebp, 1
        shl ebp, cl                 ; los que tiene
        xor esi, esi                ; el de la paleta
.p:
        movzx eax, byte [pal_rgb+esi*4]
        mov [wg_t0], eax
        movzx eax, byte [pal_rgb+esi*4+1]
        mov [wg_t1], eax
        movzx eax, byte [pal_rgb+esi*4+2]
        mov [wg_t2], eax
        or ebx, -1                  ; la menor distancia
        xor edx, edx                ; el mejor
        xor ecx, ecx
.c:
        cmp ecx, ebp
        jae .cf
        push edx
        movzx eax, byte [edi+DB_COLORES+ecx*4]
        sub eax, [wg_t0]
        imul eax, eax
        mov edx, eax
        movzx eax, byte [edi+DB_COLORES+ecx*4+1]
        sub eax, [wg_t1]
        imul eax, eax
        add edx, eax
        movzx eax, byte [edi+DB_COLORES+ecx*4+2]
        sub eax, [wg_t2]
        imul eax, eax
        add eax, edx
        pop edx
        cmp eax, ebx
        jae .cs
        mov ebx, eax
        mov edx, ecx
.cs:
        inc ecx
        jmp .c
.cf:
        mov [edi+DB_CERCA+esi], dl
        inc esi
        cmp esi, 256
        jb .p
        mov dword [edi+DB_INV], 1
        popad
.r:
        ret
; ---- (interna) la fila ESI (colores de la paleta, ECX puntos) al formato del
; DIB EDI, en EBP ----
wg_dib_escribir:
        pushad
        xor edx, edx
.l:
        cmp edx, ecx
        jae .x
        mov al, [esi+edx]
        push ecx
        mov ecx, edx
        call wg_dib_punto
        pop ecx
        inc edx
        jmp .l
.x:
        popad
        ret
; ---- (interna) EDI=ficha EBP=una fila de sus puntos ECX=x AL=color de la
; paleta: lo pone en su formato ----
wg_dib_punto:
        pushad
        movzx eax, al
        mov ebx, [edi+DB_BPP]
        cmp ebx, 8
        jbe .ind
        mov eax, [pal_rgb+eax*4]    ; R, G, B
        cmp ebx, 16
        je .w16
        lea edx, [ecx*2+ecx]
        cmp ebx, 32
        jne .w24
        lea edx, [ecx*4]
.w24:
        mov [ebp+edx+2], al         ; -> B, G, R
        mov [ebp+edx+1], ah
        shr eax, 16
        mov [ebp+edx], al
        cmp ebx, 32
        jne .x
        mov byte [ebp+edx+3], 0
        jmp .x
.w16:
        movzx ebx, al               ; R
        movzx edx, ah               ; G
        shr eax, 16
        and eax, 0xFF               ; B
        shr eax, 3
        shr ebx, 3
        cmp dword [edi+DB_565], 0
        jne .w565
        shl ebx, 10
        shr edx, 3
        shl edx, 5
        jmp .w16c
.w565:
        shl ebx, 11
        shr edx, 2
        shl edx, 5
.w16c:
        or eax, ebx
        or eax, edx
        mov [ebp+ecx*2], ax
        jmp .x
.ind:
        call wg_dib_cerca
        mov al, [edi+DB_CERCA+eax]
        cmp ebx, 8
        jne .i4
        mov [ebp+ecx], al
        jmp .x
.i4:
        cmp ebx, 4
        jne .i1
        mov edx, ecx
        shr edx, 1
        and al, 15
        test ecx, 1
        jnz .i4b
        shl al, 4
        and byte [ebp+edx], 0x0F
        or [ebp+edx], al
        jmp .x
.i4b:
        and byte [ebp+edx], 0xF0
        or [ebp+edx], al
        jmp .x
.i1:
        mov edx, ecx
        shr edx, 3
        and ecx, 7
        mov ah, 0x80
        shr ah, cl
        test al, 1
        jz .i1c
        or [ebp+edx], ah
        jmp .x
.i1c:
        not ah
        and [ebp+edx], ah
.x:
        popad
        ret
; ---- ESI=un mapa con DIB: sus puntos -> el mapa ----
wg_dib_a_mapa:
        pushad
        mov edi, [esi+GO_E]
        mov ebp, [esi+GO_A]
        xor ebx, ebx
.f:
        cmp ebx, [edi+DB_AL]
        jae .x
        push esi
        call wg_dib_fila
        mov ecx, [edi+DB_AN]
        push edi
        mov edi, ebp
        rep movsb
        pop edi
        pop esi
        add ebp, [esi+GO_B]
        inc ebx
        jmp .f
.x:
        popad
        ret
; ---- ESI=un mapa con DIB: el mapa -> sus puntos (solo los que han
; cambiado: los demás conservan su color exacto) ----
wg_mapa_a_dib:
        pushad
        mov edi, [esi+GO_E]
        mov edx, [esi+GO_A]
        xor ebx, ebx
.f:
        cmp ebx, [edi+DB_AL]
        jae .x
        push esi
        call wg_dib_dir
        mov ebp, esi
        pop esi
        jc .s
        push esi
        call wg_dib_fila            ; (lo que había, ya en la paleta)
        xor ecx, ecx
.p:
        cmp ecx, [edi+DB_AN]
        jae .pf
        cmp ecx, WG_FILA
        jae .pf
        mov al, [edx+ecx]
        cmp al, [wg_fila+ecx]
        je .ps
        call wg_dib_punto
.ps:
        inc ecx
        jmp .p
.pf:
        pop esi
.s:
        add edx, [esi+GO_B]
        inc ebx
        jmp .f
.x:
        popad
        ret

; =============================================================================
;  La copia de bloques
; =============================================================================
; Entrada (variables): wg_b_dx, wg_b_dy, wg_b_dw, wg_b_dh el destino (en la
; superficie; negativos: al revés); wg_b_sx... wg_b_sh el origen (en sus
; puntos); wg_b_src 0 sin origen, 1 una matriz (wg_b_ptr, wg_b_paso, de
; wg_b_an × wg_b_al), 2 un DIB (wg_b_dib); wg_b_rop la tabla de la ROP3;
; wg_b_xlat lo que se hace con cada color del origen (wg_b_xid = 1: nada).
; Si la ROP mira el pincel, wg_ppincel ya hecho.
wg_blt:
        pushad
        xor eax, eax
        mov [wg_b_fx], eax
        mov [wg_b_fy], eax
        mov eax, [wg_b_dw]
        test eax, eax
        jns .1
        add [wg_b_dx], eax
        neg eax
        mov [wg_b_dw], eax
        xor byte [wg_b_fx], 1
.1:
        mov eax, [wg_b_dh]
        test eax, eax
        jns .2
        add [wg_b_dy], eax
        neg eax
        mov [wg_b_dh], eax
        xor byte [wg_b_fy], 1
.2:
        mov eax, [wg_b_sw]
        test eax, eax
        jns .3
        add [wg_b_sx], eax
        neg eax
        mov [wg_b_sw], eax
        xor byte [wg_b_fx], 1
.3:
        mov eax, [wg_b_sh]
        test eax, eax
        jns .4
        add [wg_b_sy], eax
        neg eax
        mov [wg_b_sh], eax
        xor byte [wg_b_fy], 1
.4:
        cmp dword [wg_b_dw], 0
        je .x
        cmp dword [wg_b_dh], 0
        je .x
        cmp dword [wg_b_src], 0
        je .5
        cmp dword [wg_b_sw], 0
        je .x
        cmp dword [wg_b_sh], 0
        je .x
.5:
        ; lo que se mira de la ROP
        movzx eax, byte [wg_b_rop]
        mov ecx, eax
        shr ecx, 4
        xor ecx, eax
        and ecx, 0x0F
        mov [wg_b_usap], ecx
        mov ecx, eax
        shr ecx, 2
        xor ecx, eax
        and ecx, 0x33
        mov [wg_b_usas], ecx
        ; la caja de la región: las columnas que hacen falta
        mov eax, 0x7FFFFFFF
        mov ebx, 0x80000000
        mov esi, reg
        mov ecx, [reg_n]
.cj:
        jecxz .cjx
        cmp eax, [esi]
        jle .cj1
        mov eax, [esi]
.cj1:
        cmp ebx, [esi+8]
        jge .cj2
        mov ebx, [esi+8]
.cj2:
        add esi, 16
        dec ecx
        jmp .cj
.cjx:
        sub eax, [wg_b_dx]
        jns .c0
        xor eax, eax
.c0:
        sub ebx, [wg_b_dx]
        cmp ebx, [wg_b_dw]
        jle .c1
        mov ebx, [wg_b_dw]
.c1:
        mov [wg_b_c0], eax
        sub ebx, eax
        jle .x
        mov [wg_b_nc], ebx
        mov eax, [wg_b_sw]          ; ¿se estira de ancho?
        cmp eax, [wg_b_dw]
        setne al
        movzx eax, al
        mov [wg_b_estira], eax
.filas:
        mov dword [wg_b_ultf], -1
        xor ebp, ebp                ; la fila del destino
.f:
        cmp ebp, [wg_b_dh]
        jae .x
        mov ebx, [wg_b_dy]
        add ebx, ebp                ; EBX = la y en la superficie
        cmp ebx, 0
        jl .fs
        cmp ebx, [sup_alto]
        jge .x
        xor esi, esi
        cmp dword [wg_b_src], 0
        je .fr
        mov eax, ebp                ; la fila del origen
        cmp byte [wg_b_fy], 0
        je .f1
        neg eax
        add eax, [wg_b_dh]
        dec eax
.f1:
        lea eax, [eax*2+1]
        imul dword [wg_b_sh]
        mov ecx, [wg_b_dh]
        add ecx, ecx
        idiv ecx
        add eax, [wg_b_sy]
        cmp eax, [wg_b_al]
        jae .fs
        call wg_b_fila              ; EAX=fila -> ESI
.fr:
        mov [wg_b_fp], esi
        call wg_b_tramos            ; la fila EBX del destino
.fs:
        inc ebp
        jmp .f
.x:
        popad
        ret
; (de wg_blt) EAX=fila del origen -> ESI=sus puntos
wg_b_fila:
        cmp dword [wg_b_src], 2
        je .dib
        mov esi, eax
        imul esi, [wg_b_paso]
        add esi, [wg_b_ptr]
        ret
.dib:
        cmp eax, [wg_b_ultf]        ; (la de antes: ya está)
        je .y
        mov [wg_b_ultf], eax
        push ebx
        push edi
        mov ebx, eax
        mov edi, [wg_b_dib]
        call wg_dib_fila
        pop edi
        pop ebx
.y:
        mov esi, wg_fila
        ret
; (de wg_blt) los tramos de la fila EBX que caen en la región
wg_b_tramos:
        pushad
        mov esi, reg
        mov ebp, [reg_n]
.l:
        test ebp, ebp
        jz .x
        cmp ebx, [esi+4]
        jl .s
        cmp ebx, [esi+12]
        jge .s
        mov eax, [wg_b_dx]
        mov ecx, eax
        add ecx, [wg_b_dw]
        cmp eax, [esi]
        jge .a
        mov eax, [esi]
.a:
        cmp ecx, [esi+8]
        jle .b
        mov ecx, [esi+8]
.b:
        mov edx, [wg_b_dx]          ; (y en las columnas calculadas)
        add edx, [wg_b_c0]
        cmp eax, edx
        jge .c
        mov eax, edx
.c:
        add edx, [wg_b_nc]
        cmp ecx, edx
        jle .d
        mov ecx, edx
.d:
        cmp eax, ecx
        jge .s
        push esi
        call wg_b_tramo
        pop esi
.s:
        add esi, 16
        dec ebp
        jmp .l
.x:
        popad
        ret
; (de wg_blt) el tramo [EAX, ECX) de la fila EBX
wg_b_tramo:
        pushad
        mov edi, ebx
        imul edi, [sup_ancho]
        add edi, [sup_ptr]
        add edi, eax
        mov edx, eax
        sub edx, [wg_b_dx]
        sub edx, [wg_b_c0]          ; EDX = la columna (en wg_b_cols)
        sub ecx, eax
        mov [wg_t6], eax            ; la x
        mov esi, [wg_b_fp]
        cmp dword [wg_b_src], 0
        je .gen
        cmp byte [wg_b_rop], 0xCC   ; SRCCOPY tal cual, sin colores: deprisa
        jne .gen
        cmp dword [wg_b_xid], 0
        je .cc
.cc1:
        call wg_b_col
        test eax, eax
        js .cc2
        mov al, [esi+eax]
        mov [edi], al
.cc2:
        inc edi
        inc edx
        dec ecx
        jnz .cc1
        jmp .x
.cc:
        call wg_b_col
        test eax, eax
        js .cc4
        movzx eax, byte [esi+eax]
        mov al, [wg_b_xlat+eax]
        mov [edi], al
.cc4:
        inc edi
        inc edx
        dec ecx
        jnz .cc
        jmp .x
.gen:
        push ecx
        xor eax, eax                ; S
        cmp dword [wg_b_src], 0
        je .g1
        call wg_b_col
        test eax, eax
        js .gs
        movzx eax, byte [esi+eax]
        mov al, [wg_b_xlat+eax]
        call wg_rgb
.g1:
        push esi
        mov esi, eax                ; ESI = S
        xor eax, eax
        cmp dword [wg_b_usap], 0    ; P
        je .g2
        push ebx
        mov eax, [wg_t6]
        call wg_pincel_en
        pop ebx
        call wg_rgb
.g2:
        push ebx
        mov ebx, eax
        mov al, [edi]
        call wg_rgb                 ; D
        movzx ecx, byte [wg_b_rop]
        call wg_rop3
        call wg_idx
        mov [edi], al
        pop ebx
        pop esi
.gs:
        pop ecx
        inc edi
        inc edx
        inc dword [wg_t6]
        dec ecx
        jnz .gen
.x:
        popad
        ret
; (de wg_b_tramo) EDX=la columna del destino (desde wg_b_c0) -> EAX=la del
; origen (−1: fuera)
wg_b_col:
        push edx
        mov eax, edx
        add eax, [wg_b_c0]
        cmp byte [wg_b_fx], 0
        je .1
        neg eax
        add eax, [wg_b_dw]
        dec eax
.1:
        cmp dword [wg_b_estira], 0
        je .c
        push ebx
        lea eax, [eax*2+1]          ; el centro del punto, estirado
        imul dword [wg_b_sw]
        mov ebx, [wg_b_dw]
        add ebx, ebx
        idiv ebx
        pop ebx
.c:
        add eax, [wg_b_sx]
        cmp eax, [wg_b_an]
        jb .x
        or eax, -1
.x:
        pop edx
        ret
; ---- EAX=x EBX=y (superficie) -> AL=el color del pincel preparado ahí ----
wg_pincel_en:
        cmp dword [wg_pm], 0
        jg .no_liso
        mov al, [wg_pt]
        ret
.no_liso:
        push ecx
        push edx
        mov edx, ebx
        sub edx, [wg_by]
        and edx, 7
        mov ecx, eax
        sub ecx, [wg_bx]
        and ecx, 7
        cmp dword [wg_pm], 1
        jne .dib
        mov al, 0x80
        shr al, cl
        test [wg_pmask+edx], al
        mov al, [wg_pt]
        jnz .x
        mov al, [wg_bk]
        jmp .x
.dib:
        lea eax, [edx*8+ecx]
        mov al, [wg_patbuf+eax]
.x:
        pop edx
        pop ecx
        ret
; ---- prepara wg_b_xlat para un origen de mapa (ESI=su ficha; 0: la
; pantalla) y el destino abierto: de monocromo a color, 0 = el texto y 1 =
; el fondo del destino; de color a monocromo, 1 lo que es del color de
; fondo del origen (EBX = ese color, ya en la paleta) ----
wg_b_colores:
        pushad
        mov dword [wg_b_xid], 1
        xor ecx, ecx
.i:
        mov [wg_b_xlat+ecx], cl
        inc cl
        jnz .i
        xor eax, eax
        test esi, esi
        jz .c
        mov al, [esi+GO_ESTILO]
        and al, 1
.c:
        cmp al, [wg_mono]
        je .x
        mov dword [wg_b_xid], 0
        test al, al
        jz .a_mono
        mov al, [wg_txt]            ; de monocromo a color
        mov [wg_b_xlat], al
        mov al, [wg_bk]
        mov [wg_b_xlat+1], al
        jmp .x
.a_mono:
        xor ecx, ecx                ; de color a monocromo
.m:
        cmp cl, bl
        sete al
        mov [wg_b_xlat+ecx], al
        inc cl
        jnz .m
.x:
        popad
        ret

; =============================================================================
;  Las funciones de los mapas de bits
; =============================================================================
; HDC CreateCompatibleDC(HDC)
g_CreateCompatibleDC:
        push ebp
        xor ebp, ebp
        mov ecx, 3
        call w32_dc_nuevo
        pop ebp
        jc u_cero
        ret
; BOOL DeleteDC(HDC)
g_DeleteDC:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        call wg_dc_soltar
        jmp u_uno
; HBITMAP CreateCompatibleBitmap(HDC, ancho, alto)
g_CreateCompatibleBitmap:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        xor al, al                  ; (monocromo si el contexto lo es)
        cmp dword [edi+DC_USO], 3
        jne .c
        push eax
        mov eax, [edi+DC_BMP]
        call w32_obj
        pop eax
        jc .c
        mov al, [esi+GO_ESTILO]
        and al, 1
.c:
        mov ecx, [ebp+4]
        mov ebx, [ebp+8]
        call wg_mapa_nuevo
        jc u_cero
        ret
; HBITMAP CreateBitmap(ancho, alto, planos, bits, puntos)
g_CreateBitmap:
        mov eax, [ebp+12]
        cmp eax, 1
        sete al
        mov ecx, [ebp]
        mov ebx, [ebp+4]
        call wg_mapa_nuevo
        jc u_cero
        mov edi, [ebp+16]
        test edi, edi
        jz .x
        add edi, [edx+PE_DELTA]
        call wg_mapa_bits           ; los puntos
.x:
        ret
; HBITMAP CreateBitmapIndirect(BITMAP*)
g_CreateBitmapIndirect:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        movzx eax, word [esi+18]
        cmp eax, 1
        sete al
        mov ecx, [esi+4]
        mov ebx, [esi+8]
        push esi
        call wg_mapa_nuevo
        pop edi
        jc u_cero
        mov edi, [edi+20]
        test edi, edi
        jz .x
        add edi, [edx+PE_DELTA]
        call wg_mapa_bits
.x:
        ret
; (interna) ESI=un mapa EDI=sus puntos en el formato de Windows: los
; monocromos, de 1 bit con las filas de 2 en 2 octetos; los de color, de 8
wg_mapa_bits:
        pushad
        mov ebp, [esi+GO_A]
        mov edx, [esi+GO_B]
        test byte [esi+GO_ESTILO], 1
        jnz .mono
        mov ecx, edx
        inc ecx
        and ecx, ~1
        mov ebx, [esi+GO_C]
.c:
        push ecx
        push esi
        push edi
        mov esi, edi
        mov edi, ebp
        mov ecx, edx
        rep movsb
        pop edi
        pop esi
        pop ecx
        add edi, ecx
        add ebp, edx
        dec ebx
        jnz .c
        jmp .x
.mono:
        mov ebx, [esi+GO_C]
        mov ecx, edx
        add ecx, 15
        shr ecx, 4
        add ecx, ecx                ; los octetos de una fila
.m:
        push ecx
        xor ecx, ecx
.m1:
        cmp ecx, edx
        jae .m2
        mov eax, ecx
        shr eax, 3
        movzx eax, byte [edi+eax]
        push ecx
        and ecx, 7
        shl eax, cl
        pop ecx
        shr eax, 7
        and al, 1
        mov [ebp+ecx], al
        inc ecx
        jmp .m1
.m2:
        pop ecx
        add edi, ecx
        add ebp, edx
        dec ebx
        jnz .m
.x:
        popad
        ret
; LONG GetBitmapBits(HBITMAP, octetos, puntos) y SetBitmapBits(HBITMAP, octetos,
; puntos): los de 8 bits, tal cual
g_GetBitmapBits:
        mov al, 1
        jmp g_bitmapbits
g_SetBitmapBits:
        mov al, 0
g_bitmapbits:
        push eax
        mov eax, [ebp]
        call w32_obj
        pop eax
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_MAPA
        jne u_cero
        test byte [esi+GO_ESTILO], 1
        jnz u_cero
        mov ecx, [esi+GO_B]
        imul ecx, [esi+GO_C]
        cmp ecx, [ebp+4]
        jbe .n
        mov ecx, [ebp+4]
.n:
        mov edi, [ebp+8]
        add edi, [edx+PE_DELTA]
        mov esi, [esi+GO_A]
        test al, al
        jnz .g
        xchg esi, edi
.g:
        mov eax, ecx
        rep movsb
        ret
; HBITMAP CreateDIBSection(HDC, BITMAPINFO*, uso, void** puntos, sección, desde)
g_CreateDIBSection:
        mov eax, [ebp]
        call w32_dc
        jnc .dc
        xor edi, edi
.dc:
        mov [wg_dc], edi
        mov [wg_prog], edx
        call wg_tablas
        mov ecx, DB_TAM
        call w32_pedir
        jc u_cero
        mov edi, eax                ; EDI = su ficha
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov ecx, [ebp+8]
        xor ebx, ebx
        mov byte [wg_mono], 0
        call wg_dib_desc
        jc .no
        mov ecx, [edi+DB_PASO]
        imul ecx, [edi+DB_AL]
        call wg_memoria             ; sus puntos (los ve el programa)
        jc .no
        mov [edi+DB_BITS], eax
        mov [edi+DB_MANGO], ebx
        mov ecx, [edi+DB_AN]
        mov ebx, [edi+DB_AL]
        xor al, al
        push edi
        call wg_mapa_nuevo
        pop edi
        jc .no2
        mov [esi+GO_E], edi
        mov ecx, [edi+DB_BPP]
        mov [esi+GO_ANCHO], cx
        mov ecx, [ebp+12]
        test ecx, ecx
        jz .x
        add ecx, [edx+PE_DELTA]
        mov ebx, [edi+DB_BITS]
        sub ebx, [edx+PE_DELTA]
        mov [ecx], ebx
.x:
        ret
.no2:
        push eax
        mov eax, [edi+DB_BITS]
        mov ebx, [edi+DB_MANGO]
        call wg_memoria_soltar
        pop eax
.no:
        mov eax, edi
        call mem_soltar
        jmp u_cero
; (interna) prepara wg_dibd con el BITMAPINFO [EBP+ESI] y los puntos
; [EBP+EBX] (del programa) y el uso [EBP+ECX] -> CF=1 si no vale
wg_dib_arg:
        push esi
        push ebx
        push ecx
        mov esi, [ebp+esi]
        add esi, [edx+PE_DELTA]
        mov ebx, [ebp+ebx]
        add ebx, [edx+PE_DELTA]
        mov ecx, [ebp+ecx]
        and ecx, 1
        mov edi, wg_dibd
        call wg_dib_desc
        pop ecx
        pop ebx
        pop esi
        ret
; int SetDIBitsToDevice(HDC, xd, yd, ancho, alto, xo, yo, primera, líneas,
;                       puntos, BITMAPINFO*, uso)
g_SetDIBitsToDevice:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        push edi
        mov esi, 40
        mov ebx, 36
        mov ecx, 44
        call wg_dib_arg
        pop edi
        jc .no
        mov eax, [ebp+28]           ; la primera línea y cuántas
        mov [wg_dibd+DB_INI], eax
        mov eax, [ebp+32]
        mov [wg_dibd+DB_N], eax
        mov eax, [ebp+4]
        add eax, [wg_ox]
        mov [wg_b_dx], eax
        mov eax, [ebp+8]
        add eax, [wg_oy]
        mov [wg_b_dy], eax
        mov eax, [ebp+12]
        mov [wg_b_dw], eax
        mov [wg_b_sw], eax
        mov eax, [ebp+16]
        mov [wg_b_dh], eax
        mov [wg_b_sh], eax
        mov eax, [ebp+20]
        mov [wg_b_sx], eax
        mov eax, [ebp+24]           ; (la y, desde abajo si es de abajo arriba)
        cmp dword [wg_dibd+DB_ABAJO], 0
        je .y
        neg eax
        add eax, [wg_dibd+DB_AL]
        sub eax, [ebp+16]
.y:
        mov [wg_b_sy], eax
        mov eax, 0xCC
        call wg_dib_blt
        call wg_cerrar
        mov eax, [ebp+32]
        ret
.no:
        call wg_cerrar
        jmp u_cero
; (interna) copia el DIB wg_dibd con lo de wg_b_*; AL=ROP3
wg_dib_blt:
        mov [wg_b_rop], al
        mov dword [wg_b_src], 2
        mov dword [wg_b_dib], wg_dibd
        mov eax, [wg_dibd+DB_AN]
        mov [wg_b_an], eax
        mov eax, [wg_dibd+DB_AL]
        mov [wg_b_al], eax
        push esi
        xor esi, esi                ; (los colores ya son de la paleta)
        call wg_b_colores
        pop esi
        cmp byte [wg_mono], 0       ; a monocromo: claro u oscuro
        je .b
        pushad
        mov dword [wg_b_xid], 0
        xor ecx, ecx
.m:
        mov byte [wg_mono], 0
        mov al, cl
        call wg_rgb
        mov byte [wg_mono], 1
        call wg_idx
        mov [wg_b_xlat+ecx], al
        inc cl
        jnz .m
        popad
.b:
        call wg_prop_pincel
        jmp wg_blt
; (interna) si la ROP wg_b_rop mira el pincel, lo prepara (el del contexto)
wg_prop_pincel:
        push eax
        push ecx
        movzx eax, byte [wg_b_rop]
        mov ecx, eax
        shr ecx, 4
        xor ecx, eax
        test ecx, 0x0F
        jz .x
        mov edi, [wg_dc]
        mov eax, [edi+DC_PINCEL]
        call wg_ppincel
        jnc .x
        mov eax, [wg_bk]            ; (sin pincel: como uno del color de fondo)
        mov [wg_pt], eax
        mov dword [wg_pm], 0
.x:
        pop ecx
        pop eax
        ret
; int StretchDIBits(HDC, xd, yd, anchod, altod, xo, yo, anchoo, altoo, puntos,
;                   BITMAPINFO*, uso, ROP)
g_StretchDIBits:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        push edi
        mov esi, 40
        mov ebx, 36
        mov ecx, 44
        call wg_dib_arg
        pop edi
        jc .no
        mov eax, [ebp+4]
        add eax, [wg_ox]
        mov [wg_b_dx], eax
        mov eax, [ebp+8]
        add eax, [wg_oy]
        mov [wg_b_dy], eax
        mov eax, [ebp+12]
        mov [wg_b_dw], eax
        mov eax, [ebp+16]
        mov [wg_b_dh], eax
        mov eax, [ebp+20]
        mov [wg_b_sx], eax
        mov eax, [ebp+28]
        mov [wg_b_sw], eax
        mov eax, [ebp+32]
        mov [wg_b_sh], eax
        mov eax, [ebp+24]
        cmp dword [wg_dibd+DB_ABAJO], 0
        je .y
        neg eax
        add eax, [wg_dibd+DB_AL]
        sub eax, [ebp+32]
.y:
        mov [wg_b_sy], eax
        mov eax, [ebp+48]
        shr eax, 16
        call wg_dib_blt
        call wg_cerrar
        mov eax, [ebp+32]
        ret
.no:
        call wg_cerrar
        jmp u_cero
; int SetDIBits(HDC, HBITMAP, primera, líneas, puntos, BITMAPINFO*, uso)
g_SetDIBits:
        mov eax, [ebp+4]
        call w32_obj
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_MAPA
        jne u_cero
        mov eax, [ebp]
        call w32_dc
        jnc .dc
        xor edi, edi
.dc:
        mov [wg_dc], edi
        mov [wg_prog], edx
        call wg_tablas
        push esi
        mov esi, 20
        mov ebx, 16
        mov ecx, 24
        mov byte [wg_mono], 0
        call wg_dib_arg
        pop esi
        jc u_cero
        mov eax, [ebp+8]
        mov [wg_dibd+DB_INI], eax
        mov eax, [ebp+12]
        mov [wg_dibd+DB_N], eax
        mov edi, wg_dibd
        mov ebp, [esi+GO_A]         ; fila a fila, al mapa
        xor ebx, ebx
.f:
        cmp ebx, [esi+GO_C]
        jae .x
        cmp ebx, [edi+DB_AL]
        jae .x
        push esi
        push ebx
        call wg_dib_dir
        pop ebx
        pop esi
        jc .s
        push esi
        call wg_dib_fila
        mov ecx, [edi+DB_AN]
        mov eax, [esp]
        cmp ecx, [eax+GO_B]
        jbe .c
        mov ecx, [eax+GO_B]
.c:
        test byte [eax+GO_ESTILO], 1
        jnz .mono
        push edi
        mov edi, ebp
        rep movsb
        pop edi
        jmp .c2
.mono:
        push edi
        mov edi, ebp
.m:
        lodsb
        push ecx
        mov byte [wg_mono], 0
        call wg_rgb
        mov byte [wg_mono], 1
        call wg_idx
        mov byte [wg_mono], 0
        pop ecx
        stosb
        loop .m
        pop edi
.c2:
        pop esi
.s:
        add ebp, [esi+GO_B]
        inc ebx
        jmp .f
.x:
        cmp dword [esi+GO_E], 0     ; (si es de un DIB, a sus puntos)
        je .y
        call wg_mapa_a_dib
.y:
        mov eax, [wg_dibd+DB_N]
        ret
; int GetDIBits(HDC, HBITMAP, primera, líneas, puntos, BITMAPINFO*, uso)
g_GetDIBits:
        mov eax, [ebp+4]
        call w32_obj
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_MAPA
        jne u_cero
        mov [wg_prog], edx
        call wg_tablas
        mov edi, [ebp+20]
        add edi, [edx+PE_DELTA]     ; EDI = el BITMAPINFO
        cmp dword [ebp+16], 0
        jne .leer
        mov eax, [esi+GO_B]         ; sin puntos: solo la cabecera
        mov [edi+4], eax
        mov eax, [esi+GO_C]
        mov [edi+8], eax
        mov word [edi+12], 1
        movzx eax, word [esi+GO_ANCHO]
        cmp word [edi+14], 0
        jne .b
        mov [edi+14], ax
.b:
        mov dword [edi+16], 0
        movzx eax, word [edi+14]
        imul eax, [esi+GO_B]
        add eax, 31
        shr eax, 5
        shl eax, 2
        imul eax, [esi+GO_C]
        mov [edi+20], eax
        mov eax, [esi+GO_C]
        ret
.leer:
        movzx eax, word [edi+14]    ; 1, 4 y 8 bits: la tabla de colores
        cmp eax, 8
        ja .t
        mov ecx, 1
        xchg ecx, eax
        shl eax, cl
        mov ecx, eax
        mov [edi+32], eax
        add edi, [edi]
        xor ebx, ebx
.t1:
        cmp ebx, ecx
        jae .t2
        mov eax, [pal_rgb+ebx*4]    ; 8 bits: la paleta de Hispa Ventanas
        cmp ecx, 256
        je .t3
        mov eax, [wg_pal20+ebx*4]   ; 4 bits: los 16 de serie
        cmp ecx, 16
        je .t3
        mov eax, ebx                ; 1 bit: negro y blanco
        neg eax
        and eax, 0xFFFFFF
.t3:
        VUELTA_RGB                  ; RGB -> RGBQUAD (B, G, R, 0)
        mov [edi+ebx*4], eax
        inc ebx
        jmp .t1
.t2:
        mov edi, [ebp+20]
        add edi, [edx+PE_DELTA]
.t:
        push esi
        mov esi, edi
        mov ebx, [ebp+16]
        add ebx, [edx+PE_DELTA]
        mov edi, wg_dibd
        xor ecx, ecx
        mov byte [wg_mono], 0
        call wg_dib_desc
        pop esi
        jc u_cero
        mov eax, [ebp+8]
        mov [wg_dibd+DB_INI], eax
        mov eax, [ebp+12]
        mov [wg_dibd+DB_N], eax
        cmp dword [esi+GO_E], 0
        je .f0
        call wg_dib_a_mapa
.f0:
        mov edx, [esi+GO_A]
        xor ebx, ebx
.f:
        cmp ebx, [esi+GO_C]
        jae .x
        cmp ebx, [edi+DB_AL]
        jae .x
        push esi
        call wg_dib_dir
        mov ebp, esi
        pop esi
        jc .s
        push esi
        mov ecx, [esi+GO_B]
        cmp ecx, [edi+DB_AN]
        jbe .c
        mov ecx, [edi+DB_AN]
.c:
        test byte [esi+GO_ESTILO], 1
        mov esi, edx
        jz .e
        push edi                    ; (monocromo: 0 negro, 1 blanco)
        mov edi, wg_fila
        push ecx
.mo:
        lodsb
        test al, al
        mov al, 232
        jz .mo1
        mov al, 231
.mo1:
        stosb
        loop .mo
        pop ecx
        pop edi
        mov esi, wg_fila
.e:
        call wg_dib_escribir
        pop esi
.s:
        add edx, [esi+GO_B]
        inc ebx
        jmp .f
.x:
        mov eax, [wg_dibd+DB_N]
        ret
; HBITMAP CreateDIBitmap(HDC, BITMAPINFOHEADER*, CBM_INIT, puntos,
;                        BITMAPINFO*, uso)
g_CreateDIBitmap:
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov ecx, [esi+4]
        mov ebx, [esi+8]
        test ebx, ebx
        jns .h
        neg ebx
.h:
        xor al, al
        call wg_mapa_nuevo
        jc u_cero
        test dword [ebp+8], 4       ; CBM_INIT
        jz .x
        push eax
        sub esp, 28                 ; SetDIBits(HDC, mapa, 0, alto, puntos, bmi, uso)
        mov ecx, [ebp]
        mov [esp], ecx
        mov [esp+4], eax
        mov dword [esp+8], 0
        mov [esp+12], ebx
        mov ecx, [ebp+12]
        mov [esp+16], ecx
        mov ecx, [ebp+16]
        mov [esp+20], ecx
        mov ecx, [ebp+20]
        mov [esp+24], ecx
        push ebp
        lea ebp, [esp+4]
        call g_SetDIBits
        pop ebp
        add esp, 28
        pop eax
.x:
        ret
; (interna) EBX=DIB empaquetado (BITMAPINFO y detrás sus puntos; de Hispa
; Ventanas) EDX=programa -> EAX=un mapa con él (0: no se puede)
wg_mapa_de_dib:
        mov ecx, [ebx+4]
        mov eax, [ebx+8]
        push ebx
        mov ebx, eax
        test ebx, ebx
        jns .h
        neg ebx
.h:
        xor al, al
        call wg_mapa_nuevo
        pop ebx
        jc .no
        push eax
        mov esi, [ebx]              ; dónde empiezan los puntos
        add esi, ebx
        movzx ecx, word [ebx+14]
        cmp ecx, 8
        ja .p
        mov eax, [ebx+32]
        test eax, eax
        jnz .p1
        mov eax, 1
        shl eax, cl
.p1:
        lea esi, [esi+eax*4]
.p:
        cmp dword [ebx+16], 3       ; (BI_BITFIELDS: las tres máscaras)
        jne .p2
        add esi, 12
.p2:
        sub esp, 28                 ; SetDIBits(0, mapa, 0, alto, puntos, bmi, 0)
        mov dword [esp], 0
        mov eax, [esp+28]
        mov [esp+4], eax
        mov dword [esp+8], 0
        mov eax, [ebx+8]
        test eax, eax
        jns .p3
        neg eax
.p3:
        mov [esp+12], eax
        sub esi, [edx+PE_DELTA]
        mov [esp+16], esi
        sub ebx, [edx+PE_DELTA]
        mov [esp+20], ebx
        mov dword [esp+24], 0
        push ebp
        lea ebp, [esp+4]
        call g_SetDIBits
        pop ebp
        add esp, 28
        pop eax
        ret
.no:
        xor eax, eax
        ret
; HBITMAP LoadBitmapA(módulo, nombre)
u_LoadBitmapA:
        cmp dword [ebp], 0
        je u_cero                   ; (los OBM_* de serie: no hay)
        mov eax, 2                  ; RT_BITMAP
        mov ebx, [ebp+4]
        call w32_recurso
        jc u_cero
        mov ebx, esi
        call wg_tablas
        mov [wg_prog], edx
        mov dword [wg_dc], 0
        jmp wg_mapa_de_dib
; BOOL BitBlt(HDC, x, y, ancho, alto, HDC origen, xo, yo, ROP)
g_BitBlt:
        sub esp, 44                 ; como StretchBlt con el mismo tamaño
        mov esi, ebp
        mov edi, esp
        mov ecx, 8
        rep movsd
        mov eax, [ebp+12]
        mov [esp+32], eax
        mov eax, [ebp+16]
        mov [esp+36], eax
        mov eax, [ebp+32]
        mov [esp+40], eax
        push ebp
        lea ebp, [esp+4]
        call g_StretchBlt
        pop ebp
        add esp, 44
        ret
; BOOL StretchBlt(HDC, x, y, ancho, alto, HDC origen, xo, yo, anchoo, altoo, ROP)
g_StretchBlt:
        mov dword [wg_b_tmp], 0
        mov dword [wg_b_mapa], 0
        mov [wg_prog], edx
        call wg_tablas
        mov eax, [ebp+40]
        shr eax, 16
        mov [wg_b_rop], al
        mov dword [wg_b_src], 0
        mov ecx, eax                ; ¿mira el origen?
        shr ecx, 2
        xor ecx, eax
        test ecx, 0x33
        jz .abrir
        mov eax, [ebp+32]
        mov [wg_b_sw], eax
        mov eax, [ebp+36]
        mov [wg_b_sh], eax
        mov eax, [ebp+20]           ; el origen: un mapa, o una ventana
        call w32_dc
        jc u_cero
        mov eax, [edi+DC_CFONDO]    ; (su color de fondo, para pasar a monocromo)
        call wg_colref
        mov [wg_b_sfondo], eax
        mov eax, [edi+DC_DX]        ; (sus coordenadas lógicas)
        sub eax, [edi+DC_WX]
        add eax, [ebp+24]
        mov [wg_b_sx], eax
        mov eax, [edi+DC_DY]
        sub eax, [edi+DC_WY]
        add eax, [ebp+28]
        mov [wg_b_sy], eax
        mov dword [wg_b_src], 1
        cmp dword [edi+DC_USO], 3
        jne .pantalla
        mov eax, [edi+DC_BMP]
        call w32_obj
        jc u_cero
        mov [wg_b_mapa], esi
        mov eax, [esi+GO_A]
        mov [wg_b_ptr], eax
        mov eax, [esi+GO_B]
        mov [wg_b_an], eax
        mov [wg_b_paso], eax
        mov eax, [esi+GO_C]
        mov [wg_b_al], eax
        cmp dword [esi+GO_E], 0     ; un DIB: se lee de sus puntos
        je .abrir
        mov eax, [esi+GO_E]
        mov [wg_b_dib], eax
        mov dword [wg_b_src], 2
        mov dword [wg_b_ultf], -1
        jmp .abrir
.pantalla:
        ; de una ventana: lo que hay en la pantalla, copiado antes aparte
        push ebp
        mov ebp, [edi+DC_VEN]
        call w32_cli                ; EAX, EBX = el origen de su zona
        pop ebp
        add [wg_b_sx], eax
        add ebx, [wg_b_sy]
        mov eax, [wg_b_sx]
        mov ecx, [wg_b_sw]          ; el trozo, del derecho
        test ecx, ecx
        jns .p1
        add eax, ecx
        neg ecx
.p1:
        mov edx, [wg_b_sh]
        test edx, edx
        jns .p2
        add ebx, edx
        neg edx
.p2:
        call wg_b_de_pantalla
        jc u_cero
        xor eax, eax                ; (y se lee desde su esquina)
        cmp dword [wg_b_sw], 0
        jge .p3
        mov eax, [wg_b_an]
.p3:
        mov [wg_b_sx], eax
        xor eax, eax
        cmp dword [wg_b_sh], 0
        jge .p4
        mov eax, [wg_b_al]
.p4:
        mov [wg_b_sy], eax
        mov edx, [wg_prog]
.abrir:
        mov eax, [ebp]              ; el destino
        call wg_abrir
        jc .mal
        mov eax, [ebp+4]
        add eax, [wg_ox]
        mov [wg_b_dx], eax
        mov eax, [ebp+8]
        add eax, [wg_oy]
        mov [wg_b_dy], eax
        mov eax, [ebp+12]
        mov [wg_b_dw], eax
        mov eax, [ebp+16]
        mov [wg_b_dh], eax
        cmp dword [wg_b_src], 0
        je .b
        mov eax, [wg_b_sfondo]
        call wg_idx_de_origen
        mov ebx, eax
        mov esi, [wg_b_mapa]
        call wg_b_colores
        cmp dword [wg_b_src], 2     ; (de un DIB: ya en la paleta; como mucho,
        jne .b                      ;  a monocromo)
        cmp byte [wg_mono], 0
        je .b
        pushad
        mov dword [wg_b_xid], 0
        xor ecx, ecx
.m:
        mov byte [wg_mono], 0
        mov al, cl
        call wg_rgb
        mov byte [wg_mono], 1
        call wg_idx
        mov [wg_b_xlat+ecx], al
        inc cl
        jnz .m
        popad
.b:
        ; el mismo mapa de origen y de destino: se copia antes aparte
        cmp dword [wg_b_src], 1
        jne .c
        cmp dword [wg_b_tmp], 0
        jne .c
        mov eax, [wg_b_ptr]
        cmp eax, [sup_ptr]
        jne .c
        call wg_b_aparte
        jc .f
.c:
        call wg_prop_pincel
        call wg_blt
.f:
        call wg_b_soltar
        call wg_cerrar
        jmp u_uno
.mal:
        call wg_b_soltar
        jmp u_cero
; (interna) copia aparte el rectángulo EAX, EBX, ECX × EDX de la pantalla
; (lo de fuera, negro) y deja wg_b_* apuntando a la copia
wg_b_de_pantalla:
        pushad
        mov [wg_b_an], ecx
        mov [wg_b_paso], ecx
        mov [wg_b_al], edx
        mov [wg_t4], eax
        mov [wg_t5], ebx
        imul ecx, edx
        mov edx, [wg_prog]
        call wg_memoria
        jc .no
        mov [wg_b_tmp], eax
        mov [wg_b_tmpm], ebx
        mov [wg_b_ptr], eax
        mov edi, eax
        xor ebp, ebp                ; la fila
.f:
        cmp ebp, [wg_b_al]
        jae .x
        mov ebx, [wg_t5]
        add ebx, ebp
        xor ecx, ecx
.c:
        cmp ecx, [wg_b_an]
        jae .fs
        mov eax, [wg_t4]
        add eax, ecx
        mov dl, 0
        cmp eax, 0
        jl .p
        cmp eax, [sup_ancho]
        jge .p
        cmp ebx, 0
        jl .p
        cmp ebx, [sup_alto]
        jge .p
        push ebx
        imul ebx, [sup_ancho]
        add ebx, [sup_ptr]
        mov dl, [ebx+eax]
        pop ebx
.p:
        mov [edi], dl
        inc edi
        inc ecx
        jmp .c
.fs:
        inc ebp
        jmp .f
.x:
        popad
        clc
        ret
.no:
        popad
        stc
        ret
; (interna) EAX=RGB -> el color en la paleta (sin mirar el destino)
wg_idx_de_origen:
        push dword [wg_mono]
        mov byte [wg_mono], 0
        call wg_idx
        pop ecx
        mov [wg_mono], cl
        ret
; (interna) copia aparte el origen de wg_b_* (el rectángulo que se lee):
; lo que hay en wg_b_ptr pasa a un bloque nuevo (wg_b_tmp)
wg_b_aparte:
        pushad
        mov ecx, [wg_b_paso]
        imul ecx, [wg_b_al]
        mov edx, [wg_prog]
        call wg_memoria
        jc .no
        mov [wg_b_tmp], eax
        mov [wg_b_tmpm], ebx
        mov edi, eax
        mov esi, [wg_b_ptr]
        mov [wg_b_ptr], eax
        rep movsb
        popad
        clc
        ret
.no:
        popad
        stc
        ret
; (interna) suelta lo de wg_b_aparte
wg_b_soltar:
        pushad
        mov eax, [wg_b_tmp]
        test eax, eax
        jz .x
        mov ebx, [wg_b_tmpm]
        mov edx, [wg_prog]
        call wg_memoria_soltar
        mov dword [wg_b_tmp], 0
.x:
        popad
        ret
; BOOL PatBlt(HDC, x, y, ancho, alto, ROP)
g_PatBlt:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov eax, [ebp+20]
        shr eax, 16
        mov [wg_b_rop], al
        mov dword [wg_b_src], 0
        mov eax, [ebp+4]
        add eax, [wg_ox]
        mov [wg_b_dx], eax
        mov eax, [ebp+8]
        add eax, [wg_oy]
        mov [wg_b_dy], eax
        mov eax, [ebp+12]
        mov [wg_b_dw], eax
        mov eax, [ebp+16]
        mov [wg_b_dh], eax
        call wg_prop_pincel
        call wg_blt
        call wg_cerrar
        jmp u_uno
; COLORREF GetPixel(HDC, x, y)
g_GetPixel:
        mov eax, [ebp]
        call wg_abrir
        jc g_mal
        mov eax, [ebp+4]
        add eax, [wg_ox]
        mov ebx, [ebp+8]
        add ebx, [wg_oy]
        mov esi, reg                ; (solo lo que se ve)
        mov ecx, [reg_n]
.l:
        jecxz .no
        cmp eax, [esi]
        jl .s
        cmp eax, [esi+8]
        jge .s
        cmp ebx, [esi+4]
        jl .s
        cmp ebx, [esi+12]
        jge .s
        imul ebx, [sup_ancho]
        add ebx, [sup_ptr]
        mov al, [ebx+eax]
        call wg_rgb
        push eax
        call wg_cerrar
        pop eax
        ret
.s:
        add esi, 16
        dec ecx
        jmp .l
.no:
        call wg_cerrar
        jmp g_mal
