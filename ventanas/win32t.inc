; =============================================================================
;  Hispa Ventanas - [HISPA-486] GDI32 de la fase 3d: las letras y el texto
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Una letra de Windows (CreateFontA) es la de Hispa Ventanas (8×16, página
;  437) a otro tamaño: alto de la celda y ancho de cada carácter (es de paso
;  fijo); cada punto de la letra se estira (el más cercano). Con negrita,
;  cursiva, subrayado y tachado. Los textos pasan de la página 1252 (o de
;  UTF-16, las funciones W) a la 437.
;    wg_pletra   EAX=letra: la prepara (wg_fh, wg_fw...)
;    wg_texto    EAX=x EBX=y (superficie) ESI=texto (437) ECX=letras, del
;                color [wg_c]; [wg_tdx] = los avances (o 0)
; =============================================================================

; ---- EAX=letra (objeto) del programa [wg_prog]: la prepara ----
wg_pletra:
        pushad
        mov edx, [wg_prog]
        mov dword [wg_fh], 16
        mov dword [wg_fw], 8
        mov dword [wg_fb], 0
        mov dword [wg_ff], 0
        call w32_obj
        jc .x
        cmp byte [esi+GO_TIPO], GT_LETRA
        jne .x
        mov eax, [esi+GO_A]
        test eax, eax
        jz .x                       ; (las de serie: 8×16)
        mov [wg_fh], eax
        mov eax, [esi+GO_B]
        mov [wg_fw], eax
        cmp dword [esi+GO_C], 600
        setae al
        movzx eax, al
        mov [wg_fb], eax
        mov eax, [esi+GO_D]
        mov [wg_ff], eax
.x:
        mov eax, [wg_fh]            ; la línea de base y las tablas
        imul eax, 13
        shr eax, 4
        mov [wg_fasc], eax
        xor ecx, ecx
.r:
        mov eax, ecx                ; fila de la celda -> fila de la letra
        shl eax, 4
        xor edx, edx
        div dword [wg_fh]
        mov [wg_rmap+ecx], al
        inc ecx
        cmp ecx, [wg_fh]
        jb .r
        xor ecx, ecx
.c:
        mov eax, ecx                ; columna -> el bit de la letra
        shl eax, 3
        xor edx, edx
        div dword [wg_fw]
        mov dl, 0x80
        push ecx
        mov cl, al
        shr dl, cl
        pop ecx
        mov [wg_cmap+ecx], dl
        inc ecx
        cmp ecx, [wg_fw]
        jb .c
        popad
        ret
; ---- la letra del contexto EDI, preparada ----
wg_pletra_dc:
        push eax
        mov eax, [edi+DC_LETRA]
        call wg_pletra
        pop eax
        ret
; ---- (interna) lo que avanza la letra I-ésima: ECX=I -> EAX ----
wg_avance:
        mov eax, [wg_tdx]
        test eax, eax
        jz .fijo
        mov eax, [eax+ecx*4]
        ret
.fijo:
        mov eax, [wg_fw]
        add eax, [wg_fextra]
        ret
; ---- ESI=texto ECX=letras -> EAX=lo que mide de ancho ----
wg_ancho:
        push ecx
        push edx
        xor edx, edx
.l:
        dec ecx
        js .x
        push ecx
        call wg_avance
        pop ecx
        add edx, eax
        jmp .l
.x:
        mov eax, edx
        pop edx
        pop ecx
        ret
; ---- EAX=x EBX=y ESI=texto ECX=letras: lo escribe del color [wg_c] ----
wg_texto:
        pushad
        push dword [wg_rop]
        mov dword [wg_rop], 13      ; (el texto no usa la ROP2)
        xor ebp, ebp                ; la letra I
.l:
        cmp ebp, [esp+4+24]         ; (el ECX de pushad)
        jae .x
        movzx edx, byte [esi+ebp]
        push eax
        call wg_letra
        pop eax
        mov ecx, ebp
        push eax
        call wg_avance
        mov ecx, eax
        pop eax
        test byte [wg_ff], 2        ; subrayado
        jz .t
        push ebx
        push ecx
        mov edi, [wg_fasc]
        inc edi
        cmp edi, [wg_fh]
        jb .u1
        mov edi, [wg_fh]
        dec edi
.u1:
        add ebx, edi
        add ecx, eax
        call wg_tramo_c
        pop ecx
        pop ebx
.t:
        test byte [wg_ff], 4        ; tachado
        jz .s
        push ebx
        push ecx
        mov edi, [wg_fasc]
        shr edi, 1
        add edi, [wg_fasc]
        shr edi, 1
        add ebx, edi
        add ecx, eax
        call wg_tramo_c
        pop ecx
        pop ebx
.s:
        add eax, ecx
        inc ebp
        jmp .l
.x:
        pop dword [wg_rop]
        popad
        ret
; (de wg_texto) EDX=carácter EAX=x EBX=y (los de control, en blanco)
wg_letra:
        cmp edx, 32
        jb .r
        pushad
        shl edx, 4
        lea esi, [fuente8x16+edx]
        xor ebp, ebp                ; la fila
.f:
        cmp ebp, [wg_fh]
        jae .x
        movzx ecx, byte [wg_rmap+ebp]
        movzx edx, byte [esi+ecx]
        cmp dword [wg_fb], 0
        je .n
        mov ecx, edx
        shr ecx, 1
        or edx, ecx
.n:
        test edx, edx
        jz .sig
        mov edi, eax                ; la x de la fila (la cursiva, inclinada)
        test byte [wg_ff], 1
        jz .c0
        push eax
        push edx
        mov eax, [wg_fh]
        dec eax
        sub eax, ebp
        imul eax, [wg_fw]
        mov ecx, [wg_fh]
        add ecx, ecx
        xor edx, edx
        div ecx
        add edi, eax
        pop edx
        pop eax
.c0:
        xor ecx, ecx
.c:
        cmp ecx, [wg_fw]
        jae .sig
        test dl, [wg_cmap+ecx]
        jz .c1
        push eax
        push ebx
        lea eax, [edi+ecx]
        add ebx, ebp
        call wg_punto
        pop ebx
        pop eax
.c1:
        inc ecx
        jmp .c
.sig:
        inc ebp
        jmp .f
.x:
        popad
.r:
        ret
; ---- copia a EDI el texto ESI (1252) de ECX letras, en la página 437 ----
wg_a437:
        pushad
.c:
        dec ecx
        js .x
        lodsb
        test al, al
        jns .p
        movzx eax, al
        mov al, [w32_1252+eax-0x80]
.p:
        stosb
        jmp .c
.x:
        popad
        ret
; ---- lo mismo, de UTF-16 ----
wg_w437:
        pushad
.c:
        dec ecx
        js .x
        lodsw
        call wg_u16_1252
        test al, al
        jns .p
        movzx eax, al
        mov al, [w32_1252+eax-0x80]
.p:
        stosb
        jmp .c
.x:
        popad
        ret
; ---- AX=carácter UTF-16 -> AL=el de la página 1252 ('?' si no está) ----
wg_u16_1252:
        cmp ax, 0x100
        jb .x
        push esi
        push ecx
        mov esi, wg_t1252
        mov ecx, 32
.b:
        cmp ax, [esi]
        je .si
        add esi, 2
        dec ecx
        jnz .b
        mov al, '?'
        jmp .f
.si:
        sub esi, wg_t1252
        shr esi, 1
        lea eax, [esi+0x80]
.f:
        pop ecx
        pop esi
.x:
        ret
; de 80h a 9Fh en la página 1252, en UTF-16
wg_t1252:
        dw 0x20AC, 0xFFFF, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021
        dw 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFF, 0x017D, 0xFFFF
        dw 0xFFFF, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014
        dw 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFF, 0x017E, 0x0178

; =============================================================================
;  Las letras (objetos)
; =============================================================================
; HFONT CreateFontA(alto, ancho, escape, orientación, peso, cursiva,
;                   subrayado, tachado, juego, precisión, recorte, calidad,
;                   paso y familia, nombre)
g_CreateFontA:
        sub esp, 60                 ; se hace su LOGFONTA y como la otra
        mov edi, esp
        mov eax, [ebp]
        stosd
        mov eax, [ebp+4]
        stosd
        mov eax, [ebp+8]
        stosd
        mov eax, [ebp+12]
        stosd
        mov eax, [ebp+16]
        stosd
        mov al, [ebp+20]
        stosb
        mov al, [ebp+24]
        stosb
        mov al, [ebp+28]
        stosb
        mov al, [ebp+32]
        stosb
        mov al, [ebp+36]
        stosb
        mov al, [ebp+40]
        stosb
        mov al, [ebp+44]
        stosb
        mov al, [ebp+48]
        stosb
        mov esi, [ebp+52]
        mov ecx, 31
        test esi, esi
        jz .n0
        add esi, [edx+PE_DELTA]
.n:
        lodsb
        stosb
        test al, al
        jz .n1
        dec ecx
        jnz .n
.n0:
        mov byte [edi], 0
.n1:
        mov esi, esp
        call wg_letra_nueva
        add esp, 60
        ret
; HFONT CreateFontIndirectA(LOGFONTA*)
g_CreateFontIndirectA:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
; (interna) ESI=LOGFONTA (de Hispa Ventanas) -> EAX=la letra (0: no caben)
wg_letra_nueva:
        push esi
        call g_nuevo
        mov edi, esi
        pop esi
        jc .no
        mov byte [edi+GO_TIPO], GT_LETRA
        mov ecx, [esi]              ; lfHeight -> el alto de la celda
        mov [edi+GO_E], ecx
        test ecx, ecx
        jnz .h1
        mov ecx, 16
        jmp .h3
.h1:
        jns .h3
        neg ecx                     ; (negativo: el de las letras, sin el
        shl ecx, 4                  ;  hueco de arriba)
        add ecx, 6
        push eax
        push edx
        mov eax, ecx
        xor edx, edx
        mov ecx, 13
        div ecx
        mov ecx, eax
        pop edx
        pop eax
.h3:
        cmp ecx, 14                 ; (cerca del de siempre: el de siempre, que se
        jb .h4                      ;  ve mejor)
        cmp ecx, 18
        ja .h4
        mov ecx, 16
.h4:
        cmp ecx, 12                 ; (más pequeña no se lee)
        jae .h5
        mov ecx, 12
.h5:
        cmp ecx, 128
        jbe .h6
        mov ecx, 128
.h6:
        mov [edi+GO_A], ecx
        mov ebx, [esi+4]            ; lfWidth (0: la mitad del alto)
        mov [edi+GO_F], ebx
        test ebx, ebx
        jg .w1
        mov ebx, ecx
        shr ebx, 1
.w1:
        cmp ebx, 5
        jae .w2
        mov ebx, 5
.w2:
        cmp ebx, 64
        jbe .w3
        mov ebx, 64
.w3:
        mov [edi+GO_B], ebx
        mov ecx, [esi+16]           ; lfWeight
        test ecx, ecx
        jnz .p
        mov ecx, 400
.p:
        mov [edi+GO_C], ecx
        xor ecx, ecx
        cmp byte [esi+20], 0
        je .f1
        or ecx, 1
.f1:
        cmp byte [esi+21], 0
        je .f2
        or ecx, 2
.f2:
        cmp byte [esi+22], 0
        je .f3
        or ecx, 4
.f3:
        mov [edi+GO_D], ecx
        cmp dword [edi+GO_A], 16    ; la de siempre: como las de serie
        jne .x
        cmp dword [edi+GO_B], 8
        jne .x
        cmp dword [edi+GO_C], 600
        jae .x
        test ecx, ecx
        jnz .x
        mov dword [edi+GO_A], 0
.x:
        ret
.no:
        xor eax, eax
        ret
; (interna) EAX=letra -> rellena el LOGFONTA EDI (de Hispa Ventanas)
wg_letra_logfont:
        pushad
        push edi
        mov ecx, 15
        push eax
        xor eax, eax
        rep stosd
        pop eax
        pop edi
        call wg_pletra
        mov eax, [wg_fh]
        neg eax
        mov ecx, [wg_fasc]
        add ecx, [wg_fh]
        shr ecx, 1
        call w32_obj
        jc .s
        cmp byte [esi+GO_TIPO], GT_LETRA
        jne .s
        cmp dword [esi+GO_E], 0
        je .s
        mov eax, [esi+GO_E]
.s:
        mov [edi], eax              ; lfHeight
        mov eax, [wg_fw]
        mov [edi+4], eax
        mov eax, 400
        cmp dword [wg_fb], 0
        je .p
        mov eax, 700
.p:
        mov [edi+16], eax
        mov al, [wg_ff]
        mov ah, al
        and ah, 1
        mov [edi+20], ah
        mov ah, al
        shr ah, 1
        and ah, 1
        mov [edi+21], ah
        shr al, 2
        and al, 1
        mov [edi+22], al
        mov byte [edi+23], 0        ; ANSI_CHARSET
        mov byte [edi+27], 0x31     ; FIXED_PITCH | FF_MODERN
        lea edi, [edi+28]
        mov esi, wg_t_letra
        call pon_cad
        mov byte [edi], 0
        popad
        ret
wg_t_letra  db "Hispa", 0
; ---- EDI=TEXTMETRICA (de Hispa Ventanas): la letra preparada ----
wg_metricas:
        pushad
        mov ebx, edi
        xor eax, eax
        mov ecx, 14
        rep stosd
        mov eax, [wg_fh]
        mov [ebx], eax              ; tmHeight
        mov ecx, [wg_fasc]
        mov [ebx+4], ecx            ; tmAscent
        sub eax, ecx
        mov [ebx+8], eax            ; tmDescent
        mov eax, [wg_fh]
        imul eax, 3
        shr eax, 4
        mov [ebx+12], eax           ; tmInternalLeading
        mov eax, [wg_fw]
        mov [ebx+20], eax           ; tmAveCharWidth
        mov [ebx+24], eax           ; tmMaxCharWidth
        mov eax, 400
        cmp dword [wg_fb], 0
        je .p
        mov eax, 700
.p:
        mov [ebx+28], eax           ; tmWeight
        mov dword [ebx+36], 96
        mov dword [ebx+40], 96
        mov dword [ebx+44], 0x2020FF20  ; primera, última, por defecto y de corte
        mov al, [wg_ff]
        mov ah, al
        and ah, 1
        mov [ebx+48], ah
        mov ah, al
        shr ah, 1
        and ah, 1
        mov [ebx+49], ah
        shr al, 2
        and al, 1
        mov [ebx+50], al
        mov byte [ebx+51], 0x30     ; FF_MODERN, paso fijo
        popad
        ret
; BOOL GetTextMetricsA(HDC, TEXTMETRICA*)
g_GetTextMetricsA:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        call wg_pfuera
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        call wg_metricas
        jmp u_uno
; (interna) prepara la letra del contexto EDI sin abrirlo
wg_pfuera:
        mov [wg_prog], edx
        mov eax, [edi+DC_EXTRA]
        mov [wg_fextra], eax
        mov dword [wg_tdx], 0
        jmp wg_pletra_dc
; BOOL GetTextExtentPoint32A(HDC, texto, letras, SIZE*)
g_GetTextExtentPoint32A:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        call wg_pfuera
        mov ecx, [ebp+8]
        mov esi, [ebp+4]
        call wg_ancho
.pon:
        mov edi, [ebp+12]
        add edi, [edx+PE_DELTA]
        mov [edi], eax
        mov eax, [wg_fh]
        mov [edi+4], eax
        jmp u_uno
; BOOL GetTextExtentPoint32W(HDC, texto, letras, SIZE*)
g_GetTextExtentPoint32W:
        jmp g_GetTextExtentPoint32A
; BOOL GetTextExtentExPointA(HDC, texto, letras, ancho, int* caben, int* cada,
;                            SIZE*)
g_GetTextExtentExPointA:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        call wg_pfuera
        mov edi, [ebp+20]           ; los anchos acumulados
        test edi, edi
        jz .a
        add edi, [edx+PE_DELTA]
.a:
        xor ebx, ebx                ; cuántas caben
        xor esi, esi                ; el ancho hasta ahora
        xor ecx, ecx
.l:
        cmp ecx, [ebp+8]
        jae .f
        push ecx
        call wg_avance
        pop ecx
        add esi, eax
        test edi, edi
        jz .b
        mov [edi+ecx*4], esi
.b:
        cmp esi, [ebp+12]
        jg .c
        lea ebx, [ecx+1]
.c:
        inc ecx
        jmp .l
.f:
        mov edi, [ebp+16]
        test edi, edi
        jz .g
        add edi, [edx+PE_DELTA]
        mov [edi], ebx
.g:
        mov eax, esi
        mov edi, [ebp+24]
        add edi, [edx+PE_DELTA]
        mov [edi], eax
        mov eax, [wg_fh]
        mov [edi+4], eax
        jmp u_uno
; BOOL GetCharWidth32A(HDC, primera, última, int*) (y GetCharWidthA)
g_GetCharWidth32A:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        call wg_pfuera
        mov edi, [ebp+12]
        add edi, [edx+PE_DELTA]
        mov ecx, [ebp+8]
        sub ecx, [ebp+4]
        jb u_cero
        inc ecx
        mov eax, [wg_fw]
        rep stosd
        jmp u_uno
; BOOL GetCharABCWidthsA(HDC, primera, última, ABC*)
g_GetCharABCWidthsA:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        call wg_pfuera
        mov edi, [ebp+12]
        add edi, [edx+PE_DELTA]
        mov ecx, [ebp+8]
        sub ecx, [ebp+4]
        jb u_cero
        inc ecx
.l:
        xor eax, eax
        stosd
        mov eax, [wg_fw]
        stosd
        xor eax, eax
        stosd
        loop .l
        jmp u_uno
; int GetTextFaceA(HDC, sitio, búfer)
g_GetTextFaceA:
        mov edi, [ebp+8]
        test edi, edi
        jz .n
        add edi, [edx+PE_DELTA]
        mov esi, wg_t_letra
        call pon_cad
        mov byte [edi], 0
.n:
        mov eax, 5
        ret
; int SetTextCharacterExtra(HDC, extra) y GetTextCharacterExtra(HDC)
g_SetTextCharacterExtra:
        mov ecx, DC_EXTRA
        jmp g_poner
g_GetTextCharacterExtra:
        mov ecx, DC_EXTRA
        jmp g_leer
g_GetTextAlign:
        mov ecx, DC_ALIN
        jmp g_leer

; =============================================================================
;  Escribir
; =============================================================================
; (interna) escribe en el contexto abierto EDI el texto ESI (ECX letras,
; ya en la 437) en EAX, EBX (lógicas, con la alineación del contexto). Con el
; fondo si es opaco. Con TA_UPDATECP, desde donde está la pluma y la mueve
wg_escribir:
        pushad
        call wg_pletra_dc
        push eax
        call wg_ancho
        mov ebp, eax                ; EBP = lo que mide
        pop eax
        mov edx, [edi+DC_ALIN]
        test edx, 1                 ; TA_UPDATECP
        jz .a
        mov eax, [edi+DC_PX]
        mov ebx, [edi+DC_PY]
.a:
        push eax
        mov eax, edx
        and eax, 6
        cmp eax, 6                  ; TA_CENTER
        jne .a1
        mov eax, ebp
        shr eax, 1
        sub [esp], eax
        jmp .a3
.a1:
        cmp eax, 2                  ; TA_RIGHT
        jne .a3
        sub [esp], ebp
.a3:
        pop eax
        test edx, 1
        jz .b
        push eax                    ; (la pluma: al final, o al principio si
        mov ecx, edx                ;  es a la derecha; centrado, no se mueve)
        and ecx, 6
        cmp ecx, 6
        je .a5
        cmp ecx, 2
        je .a4
        add eax, ebp
.a4:
        mov [edi+DC_PX], eax
.a5:
        pop eax
.b:
        mov ecx, edx
        and ecx, 24
        cmp ecx, 24                 ; TA_BASELINE
        jne .b1
        sub ebx, [wg_fasc]
        jmp .b2
.b1:
        cmp ecx, 8                  ; TA_BOTTOM
        jne .b2
        sub ebx, [wg_fh]
.b2:
        add eax, [wg_ox]
        add ebx, [wg_oy]
        cmp dword [edi+DC_MODO], 2  ; opaco: el fondo
        jne .t
        push eax
        push ecx
        mov ecx, [wg_bk]
        mov [wg_c], ecx
        lea ecx, [eax+ebp]
        mov edx, [wg_fh]
        push dword [wg_rop]
        mov dword [wg_rop], 13
.f:
        call wg_tramo_c
        inc ebx
        dec edx
        jnz .f
        pop dword [wg_rop]
        sub ebx, [wg_fh]
        pop ecx
        pop eax
.t:
        mov ecx, [wg_txt]
        mov [wg_c], ecx
        mov ecx, [esp+24]           ; (el ECX de pushad)
        call wg_texto
        popad
        ret
; (interna) del programa EDX: el texto ESI (1252, ECX letras; -1: hasta el 0)
; a wg_tbuf -> ECX=letras
wg_tomar_texto:
        add esi, [edx+PE_DELTA]
        test ecx, ecx
        jns .n
        push eax
        call cad_largo
        mov ecx, eax
        pop eax
.n:
        cmp ecx, WG_TBUF
        jbe .c
        mov ecx, WG_TBUF
.c:
        push edi
        mov edi, wg_tbuf
        call wg_a437
        pop edi
        ret
; lo mismo, de UTF-16
wg_tomar_textow:
        add esi, [edx+PE_DELTA]
        test ecx, ecx
        jns .n
        xor ecx, ecx
.b:
        cmp word [esi+ecx*2], 0
        je .n
        inc ecx
        jmp .b
.n:
        cmp ecx, WG_TBUF
        jbe .c
        mov ecx, WG_TBUF
.c:
        push edi
        mov edi, wg_tbuf
        call wg_w437
        pop edi
        ret
; BOOL TextOutA(HDC, x, y, texto, letras)
g_TextOutA:
        mov esi, [ebp+12]
        mov ecx, [ebp+16]
        call wg_tomar_texto
g_textout:
        mov eax, [ebp]
        push ecx
        call wg_abrir
        pop ecx
        jc u_cero
        mov dword [wg_tdx], 0
        mov eax, [edi+DC_EXTRA]
        mov [wg_fextra], eax
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        mov esi, wg_tbuf
        call wg_escribir
        call wg_cerrar
        jmp u_uno
; BOOL TextOutW(HDC, x, y, texto, letras)
g_TextOutW:
        mov esi, [ebp+12]
        mov ecx, [ebp+16]
        call wg_tomar_textow
        jmp g_textout
; BOOL ExtTextOutA(HDC, x, y, opciones, RECT*, texto, letras, int* avances)
g_ExtTextOutA:
        mov esi, [ebp+20]
        mov ecx, [ebp+24]
        test esi, esi
        jnz .t
        xor ecx, ecx
        jmp g_exttextout
.t:
        call wg_tomar_texto
        jmp g_exttextout
g_ExtTextOutW:
        mov esi, [ebp+20]
        mov ecx, [ebp+24]
        test esi, esi
        jnz .t
        xor ecx, ecx
        jmp g_exttextout
.t:
        call wg_tomar_textow
g_exttextout:
        mov eax, [ebp]
        push ecx
        call wg_abrir
        pop ecx
        jc u_cero
        mov eax, [edi+DC_EXTRA]
        mov [wg_fextra], eax
        mov eax, [ebp+28]           ; los avances
        test eax, eax
        jz .d
        add eax, [edx+PE_DELTA]
.d:
        mov [wg_tdx], eax
        mov esi, [ebp+16]           ; el rectángulo
        test esi, esi
        jz .sinr
        add esi, [edx+PE_DELTA]
        test dword [ebp+12], 2      ; ETO_OPAQUE: el fondo, siempre
        jz .r1
        pushad
        mov eax, [wg_bk]
        mov [wg_c], eax
        mov eax, [esi]
        mov ecx, [esi+8]
        mov ebx, [esi+4]
        mov edx, [esi+12]
        add eax, [wg_ox]
        add ecx, [wg_ox]
        add ebx, [wg_oy]
        add edx, [wg_oy]
        push dword [wg_rop]
        mov dword [wg_rop], 13
.r0:
        cmp ebx, edx
        jge .r0x
        call wg_tramo_c
        inc ebx
        jmp .r0
.r0x:
        pop dword [wg_rop]
        popad
.r1:
        test dword [ebp+12], 4      ; ETO_CLIPPED: dentro del rectángulo
        jz .sinr
        pushad
        mov eax, [esi]
        mov ecx, [esi+8]
        mov ebx, [esi+4]
        mov edx, [esi+12]
        add eax, [wg_ox]
        add ecx, [wg_ox]
        add ebx, [wg_oy]
        add edx, [wg_oy]
        call reg_cortar
        popad
.sinr:
        test ecx, ecx
        jz .f
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        mov esi, wg_tbuf
        test dword [ebp+12], 2      ; (con ETO_OPAQUE, el fondo ya está)
        jz .e
        push dword [edi+DC_MODO]
        mov dword [edi+DC_MODO], 1
        call wg_escribir
        pop dword [edi+DC_MODO]
        jmp .f
.e:
        call wg_escribir
.f:
        call wg_cerrar
        jmp u_uno
; LONG TabbedTextOutA(HDC, x, y, texto, letras, n, int* paradas, origen)
g_TabbedTextOutA:
        mov esi, [ebp+12]
        mov ecx, [ebp+16]
        call wg_tomar_texto
        mov eax, [ebp]
        push ecx
        call wg_abrir
        pop ecx
        jc u_cero
        call wg_pletra_dc
        call wg_tabular             ; (los tabuladores, a espacios)
        mov dword [wg_tdx], 0
        mov eax, [edi+DC_EXTRA]
        mov [wg_fextra], eax
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        mov esi, wg_tbuf
        push ecx
        call wg_escribir
        pop ecx
        call wg_cerrar
        mov esi, wg_tbuf
        call wg_ancho
        mov edx, [wg_fh]
        shl edx, 16
        mov dx, ax
        mov eax, edx
        ret
; DWORD GetTabbedTextExtentA(HDC, texto, letras, n, int* paradas)
g_GetTabbedTextExtentA:
        mov esi, [ebp+4]
        mov ecx, [ebp+8]
        call wg_tomar_texto
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        push ecx
        call wg_pfuera
        pop ecx
        call wg_tabular
        mov esi, wg_tbuf
        call wg_ancho
        mov edx, [wg_fh]
        shl edx, 16
        mov dx, ax
        mov eax, edx
        ret
; (interna) en wg_tbuf (ECX letras), los tabuladores pasan a espacios hasta
; la siguiente parada de 8 letras -> ECX
wg_tabular:
        push eax
        push esi
        push edi
        mov esi, wg_tbuf
        mov edi, wg_tbuf2
        xor eax, eax                ; la columna
.c:
        dec ecx
        js .f
        mov ah, [esi]
        inc esi
        cmp ah, 9
        je .tab
        mov [edi], ah
        inc edi
        inc al
        jmp .c
.tab:
        mov byte [edi], ' '
        inc edi
        inc al
        test al, 7
        jnz .tab
        jmp .c
.f:
        mov ecx, edi
        sub ecx, wg_tbuf2
        cmp ecx, WG_TBUF
        jbe .g
        mov ecx, WG_TBUF
.g:
        push ecx
        mov esi, wg_tbuf2
        mov edi, wg_tbuf
        rep movsb
        pop ecx
        pop edi
        pop esi
        pop eax
        ret

; =============================================================================
;  DrawTextA
; =============================================================================
DT_CENTER   equ 1
DT_RIGHT    equ 2
DT_VCENTER  equ 4
DT_BOTTOM   equ 8
DT_WORDBREAK equ 0x10
DT_SINGLELINE equ 0x20
DT_EXPANDTABS equ 0x40
DT_TABSTOP  equ 0x80
DT_NOCLIP   equ 0x100
DT_CALCRECT equ 0x400
DT_NOPREFIX equ 0x800
DT_PATH_ELLIPSIS equ 0x4000
DT_END_ELLIPSIS equ 0x8000
; int DrawTextA(HDC, texto, letras o -1, RECT*, formato) -> el alto
u_DrawTextA:
        mov esi, [ebp+4]
        test esi, esi
        jz u_cero
        mov ecx, [ebp+8]
        call wg_tomar_texto
        jmp wg_dibtexto
; int DrawTextW(HDC, texto, letras o -1, RECT*, formato)
u_DrawTextW:
        mov esi, [ebp+4]
        test esi, esi
        jz u_cero
        mov ecx, [ebp+8]
        call wg_tomar_textow
        jmp wg_dibtexto
; int DrawTextExA(HDC, texto, letras, RECT*, formato, DRAWTEXTPARAMS*)
u_DrawTextExA:
        jmp u_DrawTextA
wg_dibtexto:
        mov [wg_dt_n], ecx
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        call wg_pfuera
        mov [wg_dt_dc], edi
        mov ebx, [ebp+12]
        add ebx, [edx+PE_DELTA]
        mov [wg_dt_rect], ebx
        mov eax, [ebp+16]
        mov [wg_dt_fmt], eax
        mov ecx, 8                  ; las paradas de los tabuladores
        test eax, DT_TABSTOP
        jz .ts
        movzx ecx, ah
        test ecx, ecx
        jnz .ts
        mov ecx, 8
.ts:
        imul ecx, [wg_fw]
        mov [wg_dt_tab], ecx
        ; 1) los «&»: fuera, y se apunta qué letra va subrayada
        mov esi, wg_tbuf
        mov edi, wg_tbuf
        mov ecx, [wg_dt_n]
        mov dword [wg_dt_sub], -1
.p:
        dec ecx
        js .p9
        lodsb
        test dword [wg_dt_fmt], DT_NOPREFIX
        jnz .p8
        cmp al, '&'
        jne .p8
        test ecx, ecx
        jz .p
        cmp byte [esi], '&'
        jne .p1
        inc esi
        dec ecx
        jmp .p8
.p1:
        mov eax, edi
        sub eax, wg_tbuf
        mov [wg_dt_sub], eax
        jmp .p
.p8:
        stosb
        jmp .p
.p9:
        sub edi, wg_tbuf
        mov [wg_dt_n], edi
        ; 2) las líneas: inicio y letras de cada una (en wg_fil_l, wg_fil_r)
        call wg_dt_lineas           ; -> wg_dt_nl, el ancho más largo en EAX
        mov ecx, [wg_dt_nl]
        imul ecx, [wg_fh]
        mov [wg_dt_alto], ecx
        mov ebx, [wg_dt_rect]
        test dword [wg_dt_fmt], DT_CALCRECT
        jz .dib
        add eax, [ebx]
        mov [ebx+8], eax
        mov eax, [ebx+4]
        add eax, ecx
        mov [ebx+12], eax
        mov eax, [wg_dt_alto]
        ret
.dib:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov ebx, [wg_dt_rect]
        test dword [wg_dt_fmt], DT_NOCLIP
        jnz .y
        mov eax, [ebx]
        mov ecx, [ebx+8]
        mov edx, [ebx+12]
        mov ebx, [ebx+4]
        add eax, [wg_ox]
        add ecx, [wg_ox]
        add ebx, [wg_oy]
        add edx, [wg_oy]
        call reg_cortar
        mov ebx, [wg_dt_rect]
.y:
        mov eax, [ebx+4]            ; la y de la primera línea
        test dword [wg_dt_fmt], DT_SINGLELINE
        jz .y2
        test dword [wg_dt_fmt], DT_VCENTER
        jz .y1
        mov eax, [ebx+12]
        sub eax, [ebx+4]
        sub eax, [wg_fh]
        sar eax, 1
        add eax, [ebx+4]
.y1:
        test dword [wg_dt_fmt], DT_BOTTOM
        jz .y2
        mov eax, [ebx+12]
        sub eax, [wg_fh]
.y2:
        mov [wg_dt_y], eax
        push dword [edi+DC_ALIN]
        mov dword [edi+DC_ALIN], 0
        xor ebp, ebp                ; la línea
.l:
        cmp ebp, [wg_dt_nl]
        jae .fin
        mov esi, [wg_fil_l+ebp*4]   ; su inicio y sus letras, a wg_tbuf2
        mov ecx, [wg_fil_r+ebp*4]
        push ecx
        push esi
        add esi, wg_tbuf
        push edi
        mov edi, wg_tbuf2
        rep movsb
        pop edi
        pop esi
        pop ecx
        call wg_dt_expandir         ; (tabuladores) wg_tbuf2 -> wg_tbuf3, ECX
        call wg_dt_puntos           ; los «...» si no cabe
        push esi
        mov esi, wg_tbuf3
        call wg_ancho
        pop esi
        mov ebx, [wg_dt_rect]
        mov edx, eax
        mov eax, [ebx]
        test dword [wg_dt_fmt], DT_CENTER
        jz .x1
        mov eax, [ebx+8]
        sub eax, [ebx]
        sub eax, edx
        sar eax, 1
        add eax, [ebx]
.x1:
        test dword [wg_dt_fmt], DT_RIGHT
        jz .x2
        mov eax, [ebx+8]
        sub eax, edx
.x2:
        mov ebx, [wg_dt_y]
        push esi
        mov esi, wg_tbuf3
        call wg_escribir
        pop esi
        mov edx, [wg_dt_sub]        ; la letra subrayada, si es de esta línea
        sub edx, esi
        jb .s
        cmp edx, [wg_fil_r+ebp*4]
        jae .s
        pushad
        imul edx, [wg_fw]
        add eax, edx
        add eax, [wg_ox]
        add ebx, [wg_oy]
        add ebx, [wg_fasc]
        inc ebx
        lea ecx, [eax]
        add ecx, [wg_fw]
        mov edx, [wg_txt]
        mov [wg_c], edx
        call wg_tramo_c
        popad
.s:
        mov eax, [wg_fh]
        add [wg_dt_y], eax
        inc ebp
        jmp .l
.fin:
        pop dword [edi+DC_ALIN]
        call wg_cerrar
        mov eax, [wg_dt_alto]
        ret
; (interna) parte wg_tbuf (wg_dt_n letras) en líneas: wg_fil_l = el inicio,
; wg_fil_r = sus letras; wg_dt_nl = cuántas -> EAX=el ancho de la más larga
wg_dt_lineas:
        pushad
        xor ebp, ebp                ; líneas
        xor esi, esi                ; por dónde va
        mov dword [wg_t0], 0        ; el ancho más largo
.l:
        cmp ebp, WG_FILAS
        jae .x
        mov [wg_fil_l+ebp*4], esi
        mov edi, esi                ; EDI = el fin de la línea
        mov edx, -1                 ; EDX = el último espacio
        xor ebx, ebx                ; EBX = el ancho hasta ahí
.c:
        cmp edi, [wg_dt_n]
        jae .fin
        movzx eax, byte [wg_tbuf+edi]
        test dword [wg_dt_fmt], DT_SINGLELINE
        jnz .c2
        cmp al, 13
        je .salto
        cmp al, 10
        je .salto
.c2:
        cmp al, ' '
        jne .c3
        mov edx, edi
.c3:
        call .mide                  ; EAX = lo que avanza
        test dword [wg_dt_fmt], DT_WORDBREAK
        jz .c4
        test dword [wg_dt_fmt], DT_SINGLELINE
        jnz .c4
        push ebx
        add ebx, eax
        mov ecx, [wg_dt_rect]
        mov ecx, [ecx+8]
        push eax
        mov eax, [wg_dt_rect]
        sub ecx, [eax]
        pop eax
        cmp ebx, ecx
        pop ebx
        jle .c4
        cmp edx, esi                ; no cabe: se parte en el último espacio
        jl .c4                      ; (sin espacio: la palabra entera)
        mov edi, edx
        mov eax, edx
        sub eax, esi
        mov [wg_fil_r+ebp*4], eax
        call .ancho_hasta
        lea esi, [edx+1]
        inc ebp
        jmp .l
.c4:
        add ebx, eax
        inc edi
        jmp .c
.salto:
        mov eax, edi
        sub eax, esi
        mov [wg_fil_r+ebp*4], eax
        call .ancho_hasta
        inc edi
        cmp byte [wg_tbuf+edi-1], 13
        jne .s2
        cmp edi, [wg_dt_n]
        jae .s2
        cmp byte [wg_tbuf+edi], 10
        jne .s2
        inc edi
.s2:
        mov esi, edi
        inc ebp
        cmp esi, [wg_dt_n]
        jb .l
        jmp .x
.fin:
        mov eax, edi
        sub eax, esi
        mov [wg_fil_r+ebp*4], eax
        call .ancho_hasta
        inc ebp
.x:
        mov [wg_dt_nl], ebp
        mov eax, [wg_t0]
        mov [esp+28], eax
        popad
        ret
.mide:                              ; AL = letra, EBX = el ancho ya -> EAX
        cmp al, 9
        jne .m1
        test dword [wg_dt_fmt], DT_EXPANDTABS
        jz .m1
        push edx
        mov eax, ebx
        xor edx, edx
        div dword [wg_dt_tab]
        mov eax, [wg_dt_tab]
        sub eax, edx
        pop edx
        ret
.m1:
        mov eax, [wg_fw]
        add eax, [wg_fextra]
        ret
.ancho_hasta:                       ; el ancho de la línea EBP, al más largo
        pushad
        mov edi, [wg_fil_l+ebp*4]
        mov ecx, [wg_fil_r+ebp*4]
        xor ebx, ebx
.a:
        dec ecx
        js .a2
        movzx eax, byte [wg_tbuf+edi]
        call .mide
        add ebx, eax
        inc edi
        jmp .a
.a2:
        cmp ebx, [wg_t0]
        jle .a3
        mov [wg_t0], ebx
.a3:
        popad
        ret
; (interna) wg_tbuf2 (ECX letras) -> wg_tbuf3, con los tabuladores en
; espacios (DT_EXPANDTABS) -> ECX
wg_dt_expandir:
        push eax
        push ebx
        push edx
        push esi
        push edi
        mov esi, wg_tbuf2
        mov edi, wg_tbuf3
        xor ebx, ebx                ; el ancho
.c:
        dec ecx
        js .f
        lodsb
        cmp al, 9
        jne .p
        test dword [wg_dt_fmt], DT_EXPANDTABS
        jz .p
.t:
        mov byte [edi], ' '
        inc edi
        add ebx, [wg_fw]
        mov eax, ebx
        xor edx, edx
        div dword [wg_dt_tab]
        test edx, edx
        jnz .t
        jmp .c
.p:
        stosb
        add ebx, [wg_fw]
        jmp .c
.f:
        mov ecx, edi
        sub ecx, wg_tbuf3
        cmp ecx, WG_TBUF
        jbe .g
        mov ecx, WG_TBUF
.g:
        pop edi
        pop esi
        pop edx
        pop ebx
        pop eax
        ret
; (interna) DT_END_ELLIPSIS y DT_PATH_ELLIPSIS: si wg_tbuf3 (ECX letras) no
; cabe en el rectángulo, se corta y se le ponen «...» -> ECX
wg_dt_puntos:
        test dword [wg_dt_fmt], DT_END_ELLIPSIS | DT_PATH_ELLIPSIS
        jz .x
        push eax
        push ebx
        push edx
        mov ebx, [wg_dt_rect]
        mov edx, [ebx+8]
        sub edx, [ebx]              ; EDX = el ancho que hay
        mov eax, [wg_fw]
        add eax, [wg_fextra]
        mov ebx, eax
        imul eax, ecx
        cmp eax, edx
        jle .y
        mov eax, edx                ; caben EDX / ancho letras, con los «...»
        push edx
        xor edx, edx
        div ebx
        pop edx
        sub eax, 3
        jns .p
        xor eax, eax
.p:
        mov ecx, eax
        mov dword [wg_tbuf3+ecx], '...'
        add ecx, 3
.y:
        pop edx
        pop ebx
        pop eax
.x:
        ret
