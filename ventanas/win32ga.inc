; =============================================================================
;  Hispa Ventanas - [HISPA-486] GDI32 de la fase 3d: las funciones
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Los objetos (plumas, pinceles, letras...), las figuras (Rectangle,
;  Ellipse, RoundRect, Arc, Pie, Chord, Polygon, Polyline, PolyBezier...),
;  el estado del contexto (ROP2, orígenes, SaveDC...), las paletas y
;  GetDeviceCaps; y de USER32, lo que pinta (FillRect, FrameRect, DrawEdge...).
; =============================================================================

; ---- (w32_iniciar) lo de la fase 3d en el bloque del programa EDX ----
wg_iniciar:
        pushad
        call wg_tablas
        mov esi, [edx+PE_W32]
        lea esi, [esi+WB_OBJ+WG_MAPA0*GO_TAM]   ; el mapa de 1×1 de los contextos
        mov byte [esi+GO_TIPO], GT_MAPA         ; de memoria
        mov byte [esi+GO_ESTILO], 1
        mov word [esi+GO_ANCHO], 1
        lea eax, [esi+GO_F]
        mov [esi+GO_A], eax
        mov dword [esi+GO_B], 1
        mov dword [esi+GO_C], 1
        mov esi, [edx+PE_W32]                   ; la paleta de serie
        lea esi, [esi+WB_OBJ+15*GO_TAM]
        mov byte [esi+GO_TIPO], GT_PALETA
        mov byte [esi+GO_ESTILO], 0x80
        mov dword [esi+GO_A], wg_pal20_blq
        popad
        ret
wg_pal20_blq dd 20                  ; (los 20 colores de serie, como bloque)
             dd 0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0xC0C0C0
             dd 0xC0DCC0, 0xF0CAA6, 0xF0FBFF, 0xA4A0A0, 0x808080, 0x0000FF, 0x00FF00, 0x00FFFF
             dd 0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF
; (interna) un objeto nuevo del programa EDX -> ESI=su ficha (a cero) EAX=el
; objeto; CF=1 si no caben más
g_nuevo:
        mov esi, [edx+PE_W32]
        add esi, WB_OBJ + GO_SERIE * GO_TAM
        mov eax, GO_SERIE
.b:
        cmp byte [esi+GO_TIPO], 0
        je .hay
        add esi, GO_TAM
        inc eax
        cmp eax, GO_MAX
        jb .b
        xor eax, eax
        stc
        ret
.hay:
        push edi
        push eax
        push ecx
        mov edi, esi
        xor eax, eax
        mov ecx, GO_TAM / 4
        rep stosd
        pop ecx
        pop eax
        pop edi
        add eax, W32_HGDI
        clc
        ret

; =============================================================================
;  Plumas y pinceles
; =============================================================================
; HPEN CreatePen(estilo, ancho, color)
g_CreatePen:
        mov ebx, [ebp]
        mov ecx, [ebp+4]
        mov eax, [ebp+8]
g_pluma_nueva:                      ; EBX=estilo ECX=ancho EAX=color
        push eax
        call g_nuevo
        pop edi
        jc u_cero
        mov byte [esi+GO_TIPO], GT_PLUMA
        and bl, 15
        mov [esi+GO_ESTILO], bl
        mov [esi+GO_ANCHO], cx
        mov [esi+GO_COLOR], edi
        ret
; HPEN CreatePenIndirect(LOGPEN*)
g_CreatePenIndirect:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        mov ebx, [esi]
        mov ecx, [esi+4]
        mov eax, [esi+12]
        jmp g_pluma_nueva
; HPEN ExtCreatePen(estilo, ancho, LOGBRUSH*, n, estilos)
g_ExtCreatePen:
        mov esi, [ebp+8]
        add esi, [edx+PE_DELTA]
        mov ebx, [ebp]
        mov ecx, [ebp+4]
        mov eax, [esi+4]
        jmp g_pluma_nueva
; HBRUSH CreateSolidBrush(color)
g_CreateSolidBrush:
        mov eax, [ebp]
g_pincel_liso:
        push eax
        call g_nuevo
        pop ecx
        jc u_cero
        mov byte [esi+GO_TIPO], GT_PINCEL
        mov [esi+GO_COLOR], ecx
        ret
; HBRUSH CreateHatchBrush(rayado, color)
g_CreateHatchBrush:
        mov eax, [ebp+4]
        mov ecx, [ebp]
g_pincel_rayado:
        push ecx
        call g_pincel_liso
        pop ecx
        test eax, eax
        jz .x
        mov byte [esi+GO_ESTILO], 2
        mov [esi+GO_A], ecx
.x:
        ret
; HBRUSH CreatePatternBrush(HBITMAP)
g_CreatePatternBrush:
        mov eax, [ebp]
g_pincel_mapa:                      ; EAX=mapa
        call w32_obj
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_MAPA
        jne u_cero
        mov ebx, esi
        mov ecx, 64
        call w32_pedir
        jc u_cero
        mov edi, eax                ; 8×8 de su esquina (repetido si es menor)
        push ebx
        xor ebp, ebp
.f:
        mov eax, ebp
        xor edx, edx
        div dword [ebx+GO_C]
        mov esi, edx
        imul esi, [ebx+GO_B]
        add esi, [ebx+GO_A]
        xor ecx, ecx
.c:
        mov eax, ecx
        xor edx, edx
        div dword [ebx+GO_B]
        mov al, [esi+edx]
        stosb
        inc ecx
        cmp ecx, 8
        jb .c
        inc ebp
        cmp ebp, 8
        jb .f
        pop ebx
        sub edi, 64
        mov edx, [wg_prog]
        push edi
        xor eax, eax
        call g_pincel_liso
        pop edi
        test eax, eax
        jz .no
        mov byte [esi+GO_ESTILO], 3
        mov [esi+GO_A], edi
        mov cl, [ebx+GO_ESTILO]
        and ecx, 1
        mov [esi+GO_B], ecx
        ret
.no:
        mov eax, edi
        call mem_soltar
        jmp u_cero
; HBRUSH CreateBrushIndirect(LOGBRUSH*)
g_CreateBrushIndirect:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        mov eax, [esi+4]
        mov ecx, [esi]
        test ecx, ecx
        jz g_pincel_liso
        cmp ecx, 1
        je .nada
        cmp ecx, 2
        jne .p
        mov ecx, [esi+8]
        jmp g_pincel_rayado
.p:
        mov eax, [esi+8]
        cmp ecx, 3                  ; BS_PATTERN: un mapa
        je g_pincel_mapa
        cmp ecx, 5                  ; BS_DIBPATTERN(PT): un DIB empaquetado
        je .dib
        cmp ecx, 6
        jne u_cero
.dib:
        add eax, [edx+PE_DELTA]
        jmp g_pincel_dib
.nada:
        mov eax, W32_HGDI + 5       ; NULL_BRUSH
        ret
; HBRUSH CreateDIBPatternBrushPt(DIB, uso) y CreateDIBPatternBrush(HGLOBAL, uso)
g_CreateDIBPatternBrushPt:
        mov eax, [ebp]
        add eax, [edx+PE_DELTA]
g_pincel_dib:                       ; EAX=el DIB (de Hispa Ventanas)
        mov ebx, eax
        mov dword [wg_dc], 0
        call wg_tablas
        call wg_mapa_de_dib
        test eax, eax
        jz u_cero
        push eax
        call g_pincel_mapa
        pop ecx
        push eax
        mov eax, ecx                ; (el mapa ya no hace falta)
        call w32_obj
        jc .x
        call wg_obj_soltar
.x:
        pop eax
        ret

; =============================================================================
;  Elegir, borrar y mirar objetos
; =============================================================================
; HGDIOBJ SelectObject(HDC, objeto) -> el que había de su tipo
g_SelectObject:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [ebp+4]
        call w32_obj
        jc u_cero
        movzx ecx, byte [esi+GO_TIPO]
        cmp ecx, GT_LETRA
        jbe .tres
        cmp ecx, GT_MAPA
        je .mapa
        cmp ecx, GT_REGION
        jne u_cero
        mov dword [ebp+8], 5        ; una región: SelectClipRgn  (no: argumentos)
        jmp g_SelectClipRgn
.tres:
        cmp ecx, GT_PLUMA
        jb u_cero
        xchg eax, [edi+ecx*4+DC_PLUMA-4]
        ret
.mapa:
        cmp dword [edi+DC_USO], 3   ; solo a los de memoria
        jne u_cero
        xchg eax, [edi+DC_BMP]
        ret
; BOOL DeleteObject(objeto): los de serie no se borran; un mapa elegido en
; un contexto, tampoco; lo demás elegido se cambia por el de serie
g_DeleteObject:
        mov eax, [ebp]
        call w32_obj
        jc u_cero
        cmp eax, W32_HGDI + GO_SERIE
        jb u_uno
        mov edi, [edx+PE_W32]
        add edi, WB_DC
        mov ecx, DC_MAX
.d:
        cmp dword [edi+DC_USO], 0
        je .ds
        cmp [edi+DC_BMP], eax
        je u_cero
        cmp [edi+DC_PLUMA], eax
        jne .d1
        mov dword [edi+DC_PLUMA], W32_HGDI + 7
.d1:
        cmp [edi+DC_PINCEL], eax
        jne .d2
        mov dword [edi+DC_PINCEL], W32_HGDI + 0
.d2:
        cmp [edi+DC_LETRA], eax
        jne .d3
        mov dword [edi+DC_LETRA], W32_HGDI + 13
.d3:
        cmp [edi+DC_PALETA], eax
        jne .ds
        mov dword [edi+DC_PALETA], W32_HGDI + 15
.ds:
        add edi, DC_TAM
        loop .d
        call wg_obj_soltar
        jmp u_uno
; BOOL UnrealizeObject(objeto)
g_UnrealizeObject:
        jmp u_uno
; DWORD GetObjectType(objeto)
g_GetObjectType:
        mov eax, [ebp]
        call w32_dc
        jnc .dc
        call w32_obj
        jc u_cero
        movzx eax, byte [esi+GO_TIPO]
        movzx eax, byte [wg_tipos+eax]
        ret
.dc:
        mov eax, 3                  ; OBJ_DC
        cmp dword [edi+DC_USO], 3
        jne .x
        mov eax, 10                 ; OBJ_MEMDC
.x:
        ret
wg_tipos    db 0, 1, 2, 6, 5, 7, 8, 5, 0, 0  ; GT_* -> OBJ_*
; HGDIOBJ GetCurrentObject(HDC, tipo OBJ_*)
g_GetCurrentObject:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [ebp+4]
        mov ecx, DC_PLUMA
        cmp eax, 1
        je .x
        mov ecx, DC_PINCEL
        cmp eax, 2
        je .x
        mov ecx, DC_PALETA
        cmp eax, 5
        je .x
        mov ecx, DC_LETRA
        cmp eax, 6
        je .x
        mov ecx, DC_BMP
        cmp eax, 7
        jne u_cero
.x:
        mov eax, [edi+ecx]
        ret
; int GetObjectA(objeto, octetos, búfer) -> los octetos escritos
g_GetObjectA:
        mov eax, [ebp]
        call w32_obj
        jc u_cero
        mov edi, [ebp+8]
        test edi, edi
        jz .tam
        add edi, [edx+PE_DELTA]
.tam:
        mov ebx, edi                ; EBX = el búfer (0: solo el tamaño)
        movzx ecx, byte [esi+GO_TIPO]
        cmp ecx, GT_PLUMA
        je .pluma
        cmp ecx, GT_PINCEL
        je .pincel
        cmp ecx, GT_LETRA
        je .letra
        cmp ecx, GT_MAPA
        je .mapa
        cmp ecx, GT_PALETA
        je .paleta
        jmp u_cero
.pluma:
        mov eax, 16
        test ebx, ebx
        jz .x
        movzx ecx, byte [esi+GO_ESTILO]
        mov [ebx], ecx
        movzx ecx, word [esi+GO_ANCHO]
        mov [ebx+4], ecx
        mov dword [ebx+8], 0
        mov ecx, [esi+GO_COLOR]
        mov [ebx+12], ecx
        jmp .x
.pincel:
        mov eax, 12
        test ebx, ebx
        jz .x
        movzx ecx, byte [esi+GO_ESTILO]
        mov [ebx], ecx
        mov ecx, [esi+GO_COLOR]
        mov [ebx+4], ecx
        mov ecx, [esi+GO_A]
        mov [ebx+8], ecx
        jmp .x
.letra:
        test ebx, ebx
        jz .l
        push eax
        mov eax, [ebp]
        mov edi, ebx
        call wg_letra_logfont
        pop eax
.l:
        mov eax, 60
        jmp .x
.paleta:
        mov eax, 2
        test ebx, ebx
        jz .x
        mov ecx, [esi+GO_A]
        mov ecx, [ecx]
        mov [ebx], cx
        jmp .x
.mapa:
        mov eax, 24
        mov edi, [esi+GO_E]
        test edi, edi
        jz .m1
        cmp dword [ebp+4], 84       ; un DIB: también DIBSECTION
        jb .m1
        mov eax, 84
.m1:
        test ebx, ebx
        jz .x
        push eax
        mov dword [ebx], 0          ; BITMAP
        mov eax, [esi+GO_B]
        mov [ebx+4], eax
        mov eax, [esi+GO_C]
        mov [ebx+8], eax
        movzx ecx, word [esi+GO_ANCHO]
        imul eax, ecx, 0
        mov eax, [esi+GO_B]
        imul eax, ecx
        add eax, 15
        shr eax, 4
        add eax, eax
        mov [ebx+12], eax           ; bmWidthBytes
        mov word [ebx+16], 1
        mov [ebx+18], cx
        xor eax, eax
        test edi, edi
        jz .m2
        mov eax, [edi+DB_BITS]
        sub eax, [edx+PE_DELTA]
        mov ecx, [edi+DB_PASO]
        mov [ebx+12], ecx
.m2:
        mov [ebx+20], eax
        pop eax
        cmp eax, 84
        jb .x
        mov dword [ebx+24], 40      ; BITMAPINFOHEADER
        mov ecx, [edi+DB_AN]
        mov [ebx+28], ecx
        mov ecx, [edi+DB_AL]
        cmp dword [edi+DB_ABAJO], 0
        jne .m3
        neg ecx
.m3:
        mov [ebx+32], ecx
        mov word [ebx+36], 1
        mov ecx, [edi+DB_BPP]
        mov [ebx+38], cx
        xor ecx, ecx
        mov [ebx+40], ecx
        mov ecx, [edi+DB_PASO]
        imul ecx, [edi+DB_AL]
        mov [ebx+44], ecx
        xor ecx, ecx
        mov [ebx+48], ecx
        mov [ebx+52], ecx
        mov [ebx+56], ecx
        mov [ebx+60], ecx
        mov [ebx+64], ecx
        mov [ebx+68], ecx
        mov [ebx+72], ecx
        mov [ebx+76], ecx
        mov [ebx+80], ecx
.x:
        ret

; =============================================================================
;  Las figuras
; =============================================================================
; (interna) abre el contexto [EBP] y prepara su pluma y su pincel; CF=1 si no
; vale
wg_abrir_fig:
        mov eax, [ebp]
        call wg_abrir
        jc .x
        mov byte [wg_filtro], 0
        mov byte [wg_ultimo], 0
        mov eax, [edi+DC_PLUMA]
        call wg_ppluma
        setnc [wg_con_pluma]
        mov eax, [edi+DC_PINCEL]
        call wg_ppincel
        setnc [wg_con_pincel]
        clc
.x:
        ret
; (interna) ESI=4 números (lógicas) -> EAX..EDX en la superficie, en orden
wg_caja_sup:
        call wg_rect_orden
        add eax, [wg_ox]
        add ecx, [wg_ox]
        add ebx, [wg_oy]
        add edx, [wg_oy]
        ret
; (interna) sin pluma, la figura es un punto menor por la derecha y abajo
wg_sin_pluma:
        cmp byte [wg_con_pluma], 0
        jne .x
        dec ecx
        dec edx
.x:
        ret
; BOOL Rectangle(HDC, l, t, r, b)
g_Rectangle:
        call wg_abrir_fig
        jc u_cero
        lea esi, [ebp+4]
        call wg_caja_sup
        call wg_sin_pluma
        cmp byte [wg_con_pincel], 0
        je .p
        push ebx
.f:
        cmp ebx, edx
        jge .f1
        call wg_tramo
        inc ebx
        jmp .f
.f1:
        pop ebx
.p:
        cmp byte [wg_con_pluma], 0
        je .x
        dec ecx
        dec edx
        call wg_marco_pluma
.x:
        call wg_cerrar
        jmp u_uno
; (interna) el marco de (EAX, EBX)-(ECX, EDX) (los dos dentro) con la pluma
wg_marco_pluma:
        pushad
        mov esi, ecx
        mov edi, edx
        mov ecx, esi                ; arriba
        mov edx, ebx
        call wg_linea
        mov eax, esi                ; derecha
        mov edx, edi
        call wg_linea
        mov ebx, edi                ; abajo
        mov ecx, [esp+28]
        call wg_linea
        mov eax, ecx                ; izquierda
        mov edx, [esp+16]
        call wg_linea
        popad
        ret
; BOOL Ellipse(HDC, l, t, r, b)
g_Ellipse:
        call wg_abrir_fig
        jc u_cero
        lea esi, [ebp+4]
        call wg_caja_sup
        call wg_sin_pluma
        call wg_filas_elipse
        call wg_figura
        call wg_cerrar
        jmp u_uno
; BOOL RoundRect(HDC, l, t, r, b, ancho, alto)
g_RoundRect:
        call wg_abrir_fig
        jc u_cero
        mov eax, [ebp+20]
        mov [wg_t4], eax
        mov eax, [ebp+24]
        mov [wg_t5], eax
        lea esi, [ebp+4]
        call wg_caja_sup
        call wg_sin_pluma
        call wg_filas_redondo
        call wg_figura
        call wg_cerrar
        jmp u_uno
; (interna) Arc, Pie y Chord: AL = el filtro (1 sector, 2 cuerda), AH = 1 sin
; relleno. Las líneas del centro (Pie) o de la cuerda (Chord), después
wg_arco:
        push eax
        call wg_abrir_fig
        pop eax
        jc .x
        mov [wg_filtro], al
        test ah, ah
        jz .r
        mov byte [wg_con_pincel], 0
.r:
        mov eax, [ebp+20]           ; los puntos de inicio y de fin
        mov ebx, [ebp+24]
        mov ecx, [ebp+28]
        mov edx, [ebp+32]
        mov esi, [edi+DC_ARCO]      ; (AD_CLOCKWISE: al revés)
        cmp esi, 2
        jne .r1
        xchg eax, ecx
        xchg ebx, edx
.r1:
        add eax, [wg_ox]
        add ecx, [wg_ox]
        add ebx, [wg_oy]
        add edx, [wg_oy]
        mov [wg_t0], eax
        mov [wg_t1], ebx
        mov [wg_t2], ecx
        mov [wg_t3], edx
        lea esi, [ebp+4]
        call wg_caja_sup
        call wg_sin_pluma
        call wg_sector
        call wg_filas_elipse
        call wg_figura
        clc
.x:
        ret
; BOOL Arc(HDC, l, t, r, b, xi, yi, xf, yf)
g_Arc:
        mov ax, 0x0101
        call wg_arco
        jc u_cero
        call wg_cerrar
        jmp u_uno
; BOOL Pie(HDC, l, t, r, b, xi, yi, xf, yf)
g_Pie:
        mov ax, 0x0001
        call wg_arco
        jc u_cero
        cmp byte [wg_con_pluma], 0
        je .x
        mov byte [wg_filtro], 0
        mov byte [wg_ultimo], 1
        mov eax, [wg_cx2]
        sar eax, 1
        mov ebx, [wg_cy2]
        sar ebx, 1
        mov ecx, [wg_px0]
        mov edx, [wg_py0]
        call wg_linea
        mov ecx, [wg_px1]
        mov edx, [wg_py1]
        call wg_linea
.x:
        call wg_cerrar
        jmp u_uno
; BOOL Chord(HDC, l, t, r, b, xi, yi, xf, yf)
g_Chord:
        mov ax, 0x0002
        call wg_arco
        jc u_cero
        cmp byte [wg_con_pluma], 0
        je .x
        mov byte [wg_filtro], 0
        mov byte [wg_ultimo], 1
        mov eax, [wg_px0]
        mov ebx, [wg_py0]
        mov ecx, [wg_px1]
        mov edx, [wg_py1]
        call wg_linea
.x:
        call wg_cerrar
        jmp u_uno
; BOOL LineTo(HDC, x, y)
g_LineTo:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov byte [wg_ultimo], 0
        mov eax, [edi+DC_PLUMA]
        call wg_ppluma
        jc .f
        mov eax, [edi+DC_PX]
        mov ebx, [edi+DC_PY]
        mov ecx, [ebp+4]
        mov edx, [ebp+8]
        add eax, [wg_ox]
        add ebx, [wg_oy]
        add ecx, [wg_ox]
        add edx, [wg_oy]
        call wg_linea
.f:
        mov eax, [ebp+4]
        mov [edi+DC_PX], eax
        mov eax, [ebp+8]
        mov [edi+DC_PY], eax
        call wg_cerrar
        jmp u_uno
; BOOL GetCurrentPositionEx(HDC, POINT*)
g_GetCurrentPositionEx:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov eax, [edi+DC_PX]
        mov [esi], eax
        mov eax, [edi+DC_PY]
        mov [esi+4], eax
        jmp u_uno
; ---- (interna) los puntos ESI (lógicas, del programa) ECX: a wg_pts, en la
; superficie (como mucho WG_PUNTOS) -> ECX ----
wg_tomar_puntos:
        push eax
        push edi
        add esi, [edx+PE_DELTA]
        cmp ecx, WG_PUNTOS
        jbe .n
        mov ecx, WG_PUNTOS
.n:
        push ecx
        mov edi, wg_pts
.l:
        dec ecx
        js .x
        lodsd
        add eax, [wg_ox]
        stosd
        lodsd
        add eax, [wg_oy]
        stosd
        jmp .l
.x:
        pop ecx
        pop edi
        pop eax
        ret
; ---- (interna) las líneas de los ECX puntos de wg_pts desde el ESI-ésimo
; (cerrada si EBX=1) ----
wg_quebrada:
        pushad
        cmp ecx, 2
        jb .x
        lea edi, [wg_pts+esi*8]
        mov ebp, ecx
        dec ebp
.l:
        mov eax, [edi]
        mov ebx, [edi+4]
        mov ecx, [edi+8]
        mov edx, [edi+12]
        call wg_linea
        add edi, 8
        dec ebp
        jnz .l
        cmp dword [esp+16], 0       ; (el EBX de pushad)
        je .u
        mov eax, [edi]
        mov ebx, [edi+4]
        mov esi, [esp+4]
        mov ecx, [wg_pts+esi*8]
        mov edx, [wg_pts+esi*8+4]
        call wg_linea
        jmp .x
.u:
        cmp byte [wg_ultimo], 0
        je .x
        mov eax, [edi]
        mov ebx, [edi+4]
        call wg_ppunto
.x:
        popad
        ret
; BOOL Polyline(HDC, POINT*, n)
g_Polyline:
        call wg_abrir_fig
        jc u_cero
        cmp byte [wg_con_pluma], 0
        je .x
        mov esi, [ebp+4]
        mov ecx, [ebp+8]
        call wg_tomar_puntos
        xor esi, esi
        xor ebx, ebx
        call wg_quebrada
.x:
        call wg_cerrar
        jmp u_uno
; BOOL PolylineTo(HDC, POINT*, n): desde donde está la pluma, y la mueve
g_PolylineTo:
        call wg_abrir_fig
        jc u_cero
        mov ecx, [ebp+8]
        test ecx, ecx
        jz .x
        cmp ecx, WG_PUNTOS - 1
        jbe .n
        mov ecx, WG_PUNTOS - 1
.n:
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov eax, [esi+ecx*8-8]      ; (la pluma: al último)
        mov ebx, [esi+ecx*8-4]
        push eax
        push ebx
        mov eax, [edi+DC_PX]
        add eax, [wg_ox]
        mov [wg_pts], eax
        mov eax, [edi+DC_PY]
        add eax, [wg_oy]
        mov [wg_pts+4], eax
        push edi
        mov edi, wg_pts + 8
        push ecx
.l:
        lodsd
        add eax, [wg_ox]
        stosd
        lodsd
        add eax, [wg_oy]
        stosd
        loop .l
        pop ecx
        pop edi
        inc ecx
        cmp byte [wg_con_pluma], 0
        je .m
        xor esi, esi
        xor ebx, ebx
        call wg_quebrada
.m:
        pop dword [edi+DC_PY]
        pop dword [edi+DC_PX]
.x:
        call wg_cerrar
        jmp u_uno
; BOOL PolyPolyline(HDC, POINT*, DWORD* cuántos, n)
g_PolyPolyline:
        call wg_abrir_fig
        jc u_cero
        cmp byte [wg_con_pluma], 0
        je .x
        call wg_tomar_varios        ; wg_pts, wg_pcnt, wg_pn
        xor esi, esi                ; el primer punto de cada una
        xor ebp, ebp
.l:
        cmp ebp, [wg_pn]
        jae .x
        mov ecx, [wg_pcnt+ebp*4]
        xor ebx, ebx
        call wg_quebrada
        add esi, ecx
        inc ebp
        jmp .l
.x:
        call wg_cerrar
        jmp u_uno
; (interna) PolyPolygon y PolyPolyline: los puntos [EBP+4] y los cuántos
; [EBP+8] ([EBP+12] figuras) a wg_pts, wg_pcnt y wg_pn
wg_tomar_varios:
        mov esi, [ebp+8]
        add esi, [edx+PE_DELTA]
        mov ecx, [ebp+12]
        cmp ecx, 64
        jbe .n
        mov ecx, 64
.n:
        xor ebx, ebx                ; los puntos de todas
        xor edi, edi
.c:
        cmp edi, ecx
        jae .c2
        mov eax, [esi+edi*4]
        mov [wg_pcnt+edi*4], eax
        add ebx, eax
        inc edi
        jmp .c
.c2:
        mov [wg_pn], ecx
        mov ecx, ebx
        mov esi, [ebp+4]
        push ebp
        call wg_tomar_puntos
        pop ebp
        mov edi, [wg_dc]
        ret
; BOOL Polygon(HDC, POINT*, n)
g_Polygon:
        call wg_abrir_fig
        jc u_cero
        mov esi, [ebp+4]
        mov ecx, [ebp+8]
        call wg_tomar_puntos
        mov [wg_pcnt], ecx
        mov dword [wg_pn], 1
        jmp wg_poligonos
; BOOL PolyPolygon(HDC, POINT*, int* cuántos, n)
g_PolyPolygon:
        call wg_abrir_fig
        jc u_cero
        call wg_tomar_varios
; (interna) rellena y bordea los polígonos de wg_pts, wg_pcnt, wg_pn
wg_poligonos:
        cmp byte [wg_con_pincel], 0
        je .borde
        call wg_rellenar_pol
.borde:
        cmp byte [wg_con_pluma], 0
        je .x
        xor esi, esi
        xor ebp, ebp
.l:
        cmp ebp, [wg_pn]
        jae .x
        mov ecx, [wg_pcnt+ebp*4]
        mov ebx, 1
        call wg_quebrada
        add esi, ecx
        inc ebp
        jmp .l
.x:
        call wg_cerrar
        jmp u_uno
; (interna) el relleno de los polígonos, fila a fila (ALTERNATE o WINDING)
wg_rellenar_pol:
        pushad
        mov ecx, [wg_pn]            ; los puntos que hay de verdad
        xor eax, eax
        xor ebx, ebx
.t:
        cmp ebx, ecx
        jae .t2
        add eax, [wg_pcnt+ebx*4]
        inc ebx
        jmp .t
.t2:
        cmp eax, WG_PUNTOS
        ja .x
        test eax, eax
        jz .x
        mov [wg_t7], eax
        mov eax, 0x7FFFFFFF         ; de arriba abajo
        mov ebx, 0x80000000
        xor ecx, ecx
.y:
        cmp ecx, [wg_t7]
        jae .y2
        mov edx, [wg_pts+ecx*8+4]
        cmp edx, eax
        jge .y1
        mov eax, edx
.y1:
        cmp edx, ebx
        jle .y3
        mov ebx, edx
.y3:
        inc ecx
        jmp .y
.y2:
        mov [wg_t5], ebx            ; la última
        mov ebp, eax                ; la fila
.fila:
        cmp ebp, [wg_t5]
        jg .x
        cmp ebp, 0                  ; (lo que no se ve, ni se mira)
        jl .fs
        cmp ebp, [sup_alto]
        jge .x
        call wg_cortes              ; -> wg_cx (X2 y sentido), wg_ncx
        call wg_cortes_tramos
.fs:
        inc ebp
        jmp .fila
.x:
        popad
        ret
; (interna) los cortes de la fila EBP con los lados de todos los polígonos:
; wg_cxs (X2 en dobles y el sentido), wg_ncx
wg_cortes:
        pushad
        mov dword [wg_ncx], 0
        lea eax, [ebp*2+1]
        mov [wg_t6], eax            ; Yc
        xor esi, esi                ; el primer punto de la figura
        xor edi, edi                ; la figura
.f:
        cmp edi, [wg_pn]
        jae .x
        mov ecx, [wg_pcnt+edi*4]
        xor ebx, ebx                ; el lado
.l:
        cmp ebx, ecx
        jae .fs
        lea eax, [esi+ebx]
        lea edx, [ebx+1]
        cmp edx, ecx
        jb .l1
        xor edx, edx
.l1:
        add edx, esi
        call .corte
        inc ebx
        jmp .l
.fs:
        add esi, ecx
        inc edi
        jmp .f
.x:
        popad
        ret
.corte:                             ; el lado de los puntos EAX a EDX
        pushad
        mov ecx, [wg_pts+edx*8]     ; x1, y1
        mov edx, [wg_pts+edx*8+4]
        mov esi, [wg_pts+eax*8]     ; x0, y0
        mov edi, [wg_pts+eax*8+4]
        cmp edi, edx
        je .cx
        mov ebp, 1                  ; el sentido
        lea eax, [edi+edi]
        lea ebx, [edx+edx]
        cmp eax, ebx
        jl .c1
        xchg eax, ebx
        neg ebp
.c1:
        cmp [wg_t6], eax
        jl .cx
        cmp [wg_t6], ebx
        jge .cx
        mov ebx, edx                ; X2 = 2·x0 + (Yc − 2·y0)·(x1 − x0)/(y1 − y0)
        sub ebx, edi
        mov eax, [wg_t6]
        sub eax, edi
        sub eax, edi
        sub ecx, esi
        imul ecx
        idiv ebx
        lea eax, [eax+esi*2]
        mov ecx, [wg_ncx]
        cmp ecx, 128
        jae .cx
        mov [wg_cxs+ecx*8], eax
        mov [wg_cxs+ecx*8+4], ebp
        inc dword [wg_ncx]
.cx:
        popad
        ret
; (interna) los tramos de la fila EBP según los cortes (ordenados aquí)
wg_cortes_tramos:
        pushad
        mov ecx, [wg_ncx]
        mov esi, 1                  ; por inserción
.o:
        cmp esi, ecx
        jae .od
        mov eax, [wg_cxs+esi*8]
        mov edx, [wg_cxs+esi*8+4]
        mov edi, esi
.o1:
        test edi, edi
        jz .o2
        cmp [wg_cxs+edi*8-8], eax
        jle .o2
        mov ebx, [wg_cxs+edi*8-8]
        mov [wg_cxs+edi*8], ebx
        mov ebx, [wg_cxs+edi*8-4]
        mov [wg_cxs+edi*8+4], ebx
        dec edi
        jmp .o1
.o2:
        mov [wg_cxs+edi*8], eax
        mov [wg_cxs+edi*8+4], edx
        inc esi
        jmp .o
.od:
        mov ebx, ebp                ; la fila
        mov edi, [wg_dc]
        cmp dword [edi+DC_RELLENO], 2
        je .wind
        xor esi, esi
.a:
        lea eax, [esi+1]
        cmp eax, ecx
        jae .x
        mov eax, [wg_cxs+esi*8]
        push ecx
        mov ecx, [wg_cxs+esi*8+8]
        sar eax, 1
        sar ecx, 1
        call wg_tramo
        pop ecx
        add esi, 2
        jmp .a
.wind:
        xor esi, esi
        xor edx, edx                ; las vueltas
.w:
        cmp esi, ecx
        jae .x
        mov eax, edx
        add edx, [wg_cxs+esi*8+4]
        test eax, eax
        jnz .w1
        test edx, edx
        jz .ws
        mov ebp, [wg_cxs+esi*8]     ; empieza
        jmp .ws
.w1:
        test edx, edx
        jnz .ws
        push ecx
        mov eax, ebp
        mov ecx, [wg_cxs+esi*8]
        sar eax, 1
        sar ecx, 1
        call wg_tramo
        pop ecx
.ws:
        inc esi
        jmp .w
.x:
        popad
        ret
; BOOL PolyBezier(HDC, POINT*, n) y PolyBezierTo(HDC, POINT*, n): cada tramo,
; en 16 líneas
g_PolyBezier:
        xor bl, bl
        jmp g_bezier
g_PolyBezierTo:
        mov bl, 1
g_bezier:
        mov [wg_t7], bl
        call wg_abrir_fig
        jc u_cero
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov ecx, [ebp+8]
        xor edi, edi                ; los puntos hechos, en wg_pts
        cmp byte [wg_t7], 0
        je .p0
        mov ebx, [wg_dc]            ; (To: desde la pluma)
        mov eax, [ebx+DC_PX]
        mov [wg_bz], eax
        mov eax, [ebx+DC_PY]
        mov [wg_bz+4], eax
        jmp .seg
.p0:
        dec ecx
        js .fin
        lodsd
        mov [wg_bz], eax
        lodsd
        mov [wg_bz+4], eax
.seg:
        mov eax, [wg_bz]
        add eax, [wg_ox]
        mov [wg_pts+edi*8], eax
        mov eax, [wg_bz+4]
        add eax, [wg_oy]
        mov [wg_pts+edi*8+4], eax
        inc edi
.s:
        cmp ecx, 3
        jb .fin
        sub ecx, 3
        push ecx
        push esi
        mov ecx, 6                  ; P1, P2, P3
        push edi
        mov edi, wg_bz + 8
        rep movsd
        pop edi
        mov ebx, 1                  ; t = i/16
.t:
        cmp edi, WG_PUNTOS - 1
        jae .ts
        push ebx
        mov eax, 16                 ; a = (16−i)³, b = 3(16−i)²i, c = 3(16−i)i², d = i³
        sub eax, ebx
        mov [wg_t0], eax
        imul eax, eax
        imul eax, [wg_t0]
        mov [wg_t1], eax            ; a
        mov eax, [wg_t0]
        imul eax, eax
        imul eax, ebx
        imul eax, 3
        mov [wg_t2], eax            ; b
        mov eax, ebx
        imul eax, ebx
        imul eax, [wg_t0]
        imul eax, 3
        mov [wg_t3], eax            ; c
        mov eax, ebx
        imul eax, ebx
        imul eax, ebx
        mov [wg_t4], eax            ; d
        xor edx, edx
        call .comp
        add eax, [wg_ox]
        mov [wg_pts+edi*8], eax
        mov edx, 4
        call .comp
        add eax, [wg_oy]
        mov [wg_pts+edi*8+4], eax
        inc edi
        pop ebx
        inc ebx
        cmp ebx, 16
        jbe .t
.ts:
        mov eax, [wg_bz+24]         ; P3 pasa a ser P0
        mov [wg_bz], eax
        mov eax, [wg_bz+28]
        mov [wg_bz+4], eax
        pop esi
        add esi, 24
        pop ecx
        jmp .s
.fin:
        cmp byte [wg_con_pluma], 0
        je .m
        mov ecx, edi
        xor esi, esi
        xor ebx, ebx
        call wg_quebrada
.m:
        cmp byte [wg_t7], 0
        je .x
        mov ebx, [wg_dc]
        mov eax, [wg_bz]
        mov [ebx+DC_PX], eax
        mov eax, [wg_bz+4]
        mov [ebx+DC_PY], eax
.x:
        call wg_cerrar
        jmp u_uno
.comp:                              ; EDX = 0 (x) o 4 (y) -> EAX
        mov eax, [wg_bz+edx]
        imul eax, [wg_t1]
        push eax
        mov eax, [wg_bz+edx+8]
        imul eax, [wg_t2]
        add [esp], eax
        mov eax, [wg_bz+edx+16]
        imul eax, [wg_t3]
        add [esp], eax
        mov eax, [wg_bz+edx+24]
        imul eax, [wg_t4]
        add eax, [esp]
        add esp, 4
        add eax, 2048
        sar eax, 12
        ret
; COLORREF SetPixel(HDC, x, y, color) y BOOL SetPixelV(HDC, x, y, color)
g_SetPixel:
        call g_pixel
        jc g_mal
        ret
g_SetPixelV:
        call g_pixel
        jc u_cero
        jmp u_uno
g_pixel:
        mov eax, [ebp]
        call wg_abrir
        jc .x
        mov eax, [ebp+12]
        call wg_col
        mov [wg_c], eax
        mov dword [wg_rop], 13
        mov eax, [ebp+4]
        add eax, [wg_ox]
        mov ebx, [ebp+8]
        add ebx, [wg_oy]
        call wg_punto
        mov al, [wg_c]
        call wg_rgb
        push eax
        call wg_cerrar
        pop eax
        clc
.x:
        ret

; =============================================================================
;  El estado del contexto
; =============================================================================
; int SetROP2(HDC, ROP2), GetROP2(HDC), GetBkMode(HDC), Set/GetPolyFillMode,
; Set/GetStretchBltMode, Set/GetArcDirection, Set/GetMapMode
g_SetROP2:
        mov ecx, DC_ROP2
        jmp g_poner
g_GetROP2:
        mov ecx, DC_ROP2
        jmp g_leer
g_GetBkMode:
        mov ecx, DC_MODO
        jmp g_leer
g_SetPolyFillMode:
        mov ecx, DC_RELLENO
        jmp g_poner
g_GetPolyFillMode:
        mov ecx, DC_RELLENO
        jmp g_leer
g_SetStretchBltMode:
        mov ecx, DC_ESTIRA
        jmp g_poner
g_GetStretchBltMode:
        mov ecx, DC_ESTIRA
        jmp g_leer
g_SetArcDirection:
        mov ecx, DC_ARCO
        jmp g_poner_1
g_GetArcDirection:
        mov ecx, DC_ARCO
        jmp g_leer_1
g_SetMapMode:
        mov ecx, DC_MAPA
        jmp g_poner_1
g_GetMapMode:
        mov ecx, DC_MAPA
        jmp g_leer_1
; (los que empiezan en 0 y valen 1)
g_poner_1:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [ebp+4]
        xchg eax, [edi+ecx]
        test eax, eax
        jnz .x
        inc eax
.x:
        ret
g_leer_1:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [edi+ecx]
        test eax, eax
        jnz .x
        inc eax
.x:
        ret
; (interna) [EBP+ECX] = un POINT* (del programa) donde se deja EAX, EBX
wg_dar_punto:
        mov esi, [ebp+ecx]
        test esi, esi
        jz .x
        add esi, [edx+PE_DELTA]
        mov [esi], eax
        mov [esi+4], ebx
.x:
        ret
; BOOL SetViewportOrgEx(HDC, x, y, POINT*) y los demás de los orígenes
g_SetViewportOrgEx:
        mov esi, DC_DX
        jmp g_org_poner
g_SetWindowOrgEx:
        mov esi, DC_WX
g_org_poner:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [ebp+4]
        xchg eax, [edi+esi]
        mov ebx, [ebp+8]
        xchg ebx, [edi+esi+4]
        mov ecx, 12
        call wg_dar_punto
        jmp u_uno
g_OffsetViewportOrgEx:
        mov esi, DC_DX
        jmp g_org_mover
g_OffsetWindowOrgEx:
        mov esi, DC_WX
g_org_mover:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [edi+esi]
        mov ebx, [edi+esi+4]
        mov ecx, [ebp+4]
        add [edi+esi], ecx
        mov ecx, [ebp+8]
        add [edi+esi+4], ecx
        mov ecx, 12
        call wg_dar_punto
        jmp u_uno
g_GetViewportOrgEx:
        mov esi, DC_DX
        jmp g_org_leer
g_GetWindowOrgEx:
        mov esi, DC_WX
g_org_leer:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [edi+esi]
        mov ebx, [edi+esi+4]
        mov ecx, 4
        call wg_dar_punto
        jmp u_uno
; BOOL SetViewportExtEx/SetWindowExtEx(HDC, x, y, SIZE*): (sin escalas: 1, 1)
g_SetViewportExtEx:
        mov eax, 1
        mov ebx, 1
        mov ecx, 12
        call wg_dar_punto
        jmp u_uno
g_GetViewportExtEx:
        mov eax, 1
        mov ebx, 1
        mov ecx, 4
        call wg_dar_punto
        jmp u_uno
; BOOL DPtoLP(HDC, POINT*, n) y LPtoDP(HDC, POINT*, n)
g_DPtoLP:
        mov bl, 1
        jmp g_dplp
g_LPtoDP:
        xor bl, bl
g_dplp:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [edi+DC_DX]
        sub eax, [edi+DC_WX]
        mov ecx, [edi+DC_DY]
        sub ecx, [edi+DC_WY]
        test bl, bl
        jz .s
        neg eax
        neg ecx
.s:
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov edx, [ebp+8]
.l:
        test edx, edx
        jle u_uno
        add [esi], eax
        add [esi+4], ecx
        add esi, 8
        dec edx
        jmp .l
; BOOL SetBrushOrgEx(HDC, x, y, POINT*) y GetBrushOrgEx(HDC, POINT*)
g_SetBrushOrgEx:
        mov esi, DC_BRX
        jmp g_org_poner
g_GetBrushOrgEx:
        mov esi, DC_BRX
        jmp g_org_leer
; int SaveDC(HDC) -> el nivel
g_SaveDC:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov ecx, DC_TAM + 4
        call w32_pedir
        jc u_cero
        mov ebx, eax
        push edi
        mov esi, edi
        lea edi, [ebx+4]
        mov ecx, DC_TAM / 4
        rep movsd
        pop edi
        mov eax, [edi+DC_GUARDA]
        mov [ebx], eax
        mov [edi+DC_GUARDA], ebx
        mov esi, [edi+DC_RECORTE]   ; su recorte, copiado
        mov dword [ebx+4+DC_RECORTE], 0
        test esi, esi
        jz .n
        mov ecx, RG_TAM
        call w32_pedir
        jc .n
        mov [ebx+4+DC_RECORTE], eax
        push edi
        mov edi, eax
        mov ecx, RG_TAM / 4
        rep movsd
        pop edi
.n:
        xor eax, eax                ; el nivel: cuántos guardados hay
        mov ebx, [edi+DC_GUARDA]
.c:
        test ebx, ebx
        jz .x
        inc eax
        mov ebx, [ebx]
        jmp .c
.x:
        ret
; BOOL RestoreDC(HDC, nivel): negativo, desde el último
g_RestoreDC:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        xor ecx, ecx                ; los que hay
        mov ebx, [edi+DC_GUARDA]
.c:
        test ebx, ebx
        jz .c2
        inc ecx
        mov ebx, [ebx]
        jmp .c
.c2:
        mov eax, [ebp+4]
        test eax, eax
        jns .p
        add eax, ecx
        inc eax
.p:
        test eax, eax
        jle u_cero
        cmp eax, ecx
        ja u_cero
        sub ecx, eax                ; se sueltan ECX y se repone el siguiente
.s:
        mov ebx, [edi+DC_GUARDA]
        mov eax, [ebx]
        mov [edi+DC_GUARDA], eax
        test ecx, ecx
        jz .repon
        push eax
        mov eax, [ebx+4+DC_RECORTE]
        call mem_soltar
        mov eax, ebx
        call mem_soltar
        pop eax
        dec ecx
        jmp .s
.repon:
        mov eax, [edi+DC_RECORTE]
        call mem_soltar
        push dword [edi+DC_GUARDA]
        push dword [edi+DC_USO]
        push dword [edi+DC_VEN]
        lea esi, [ebx+4]
        push edi
        mov ecx, DC_TAM / 4
        rep movsd
        pop edi
        pop dword [edi+DC_VEN]
        pop dword [edi+DC_USO]
        pop dword [edi+DC_GUARDA]
        mov eax, ebx
        call mem_soltar
        jmp u_uno
; int GetDeviceCaps(HDC, cuál)
g_GetDeviceCaps:
        mov eax, [ebp]
        call w32_dc
        jnc .dc
        xor edi, edi
.dc:
        mov ecx, [ebp+4]
        mov eax, [vid_ancho]
        cmp ecx, 8                  ; HORZRES
        je .x
        cmp ecx, 118                ; DESKTOPHORZRES
        je .x
        mov eax, [vid_alto]
        cmp ecx, 10                 ; VERTRES
        je .x
        cmp ecx, 117
        je .x
        mov eax, [vid_ancho]        ; HORZSIZE y VERTSIZE (mm, a 96 ppp)
        imul eax, 254
        mov ebx, 960
        cmp ecx, 4
        je .div
        mov eax, [vid_alto]
        imul eax, 254
        cmp ecx, 6
        je .div
        mov eax, 8                  ; BITSPIXEL (1: un mapa monocromo)
        cmp ecx, 12
        jne .t
        test edi, edi
        jz .x
        cmp dword [edi+DC_USO], 3
        jne .x
        push eax
        mov eax, [edi+DC_BMP]
        call w32_obj
        pop eax
        jc .x
        test byte [esi+GO_ESTILO], 1
        jz .x
        mov eax, 1
        ret
.t:
        mov esi, wg_caps
.b:
        lodsw
        cmp ax, 0xFFFF
        je u_cero
        cmp ax, cx
        lodsw
        jne .b
        cwde
        ret
.div:
        xor edx, edx
        div ebx
.x:
        ret
; GetDeviceCaps: número y valor
wg_caps:
        dw 0, 0x400, 2, 1, 14, 1, 16, -1, 18, -1, 22, 0, 24, 256
        dw 28, 0xFF, 30, 0xFE, 32, 0xFF, 34, 0, 36, 1, 38, 0x2B89
        dw 40, 36, 42, 36, 44, 51, 88, 96, 90, 96, 104, 256, 106, 20
        dw 108, 18, 116, 60, 119, 0
        dw 0xFFFF
; BOOL GdiFlush(void) y DWORD GdiSetBatchLimit(n)
g_GdiFlush:
        jmp u_uno

; =============================================================================
;  Las paletas
; =============================================================================
; HPALETTE CreatePalette(LOGPALETTE*)
g_CreatePalette:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        movzx ecx, word [esi+2]
        cmp ecx, 256
        jbe .n
        mov ecx, 256
.n:
        push ecx
        push esi
        lea ecx, [ecx*4+4]
        call w32_pedir
        pop esi
        pop ecx
        jc u_cero
        mov edi, eax
        mov [edi], ecx
        push edi
        add esi, 4
        add edi, 4
        push ecx
.c:
        lodsd
        and eax, 0xFFFFFF           ; PALETTEENTRY: R, G, B (y banderas)
        stosd
        loop .c
        pop ecx
        pop edi
        push edi
        call g_nuevo
        pop edi
        jc .no
        mov byte [esi+GO_TIPO], GT_PALETA
        mov [esi+GO_A], edi
        ret
.no:
        mov eax, edi
        call mem_soltar
        jmp u_cero
; HPALETTE SelectPalette(HDC, paleta, fondo)
g_SelectPalette:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [ebp+4]
        call w32_obj
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_PALETA
        jne u_cero
        xchg eax, [edi+DC_PALETA]
        ret
; UINT RealizePalette(HDC): los colores no cambian (se usa el más cercano)
g_RealizePalette:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [edi+DC_PALETA]
        call w32_obj
        jc u_cero
        mov eax, [esi+GO_A]
        mov eax, [eax]
        ret
; UINT GetPaletteEntries(paleta, desde, n, PALETTEENTRY*) y SetPaletteEntries
g_GetPaletteEntries:
        mov bl, 1
        jmp g_entradas
g_SetPaletteEntries:
        xor bl, bl
g_entradas:
        mov eax, [ebp]
        call w32_obj
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_PALETA
        jne u_cero
        mov esi, [esi+GO_A]
        mov ecx, [ebp+8]
        mov eax, [ebp+4]
        cmp eax, [esi]
        jae u_cero
        add eax, ecx
        cmp eax, [esi]
        jbe .n
        mov ecx, [esi]
        sub ecx, [ebp+4]
.n:
        mov edi, [ebp+12]
        test edi, edi
        jz .tam
        add edi, [edx+PE_DELTA]
        mov eax, [ebp+4]
        lea esi, [esi+eax*4+4]
        test bl, bl
        jnz .c
        xchg esi, edi
.c:
        mov eax, ecx
        rep movsd
        ret
.tam:
        mov eax, [esi]
        ret
; UINT GetNearestPaletteIndex(paleta, color)
g_GetNearestPaletteIndex:
        mov eax, [ebp]
        call w32_obj
        jc g_mal
        cmp byte [esi+GO_TIPO], GT_PALETA
        jne g_mal
        mov esi, [esi+GO_A]
        mov ecx, [esi]
        mov ebx, [ebp+4]
        or edi, -1                  ; la menor distancia
        xor ebp, ebp
        xor edx, edx
.l:
        cmp edx, ecx
        jae .x
        push ecx
        push edx
        mov eax, [esi+4+edx*4]
        call wg_distancia
        pop edx
        pop ecx
        cmp eax, edi
        jae .s
        mov edi, eax
        mov ebp, edx
.s:
        inc edx
        jmp .l
.x:
        mov eax, ebp
        ret
; (interna) la distancia (al cuadrado) entre los RGB EAX y EBX
wg_distancia:
        push ebx
        push ecx
        push edx
        push esi
        xor esi, esi
        mov ecx, 3
.c:
        movzx edx, al
        push ebx
        movzx ebx, bl
        sub edx, ebx
        pop ebx
        imul edx, edx
        add esi, edx
        shr eax, 8
        shr ebx, 8
        loop .c
        mov eax, esi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
; UINT GetSystemPaletteEntries(HDC, desde, n, PALETTEENTRY*): la de Hispa
; Ventanas
g_GetSystemPaletteEntries:
        mov edi, [ebp+12]
        test edi, edi
        jz .t
        add edi, [edx+PE_DELTA]
        mov eax, [ebp+4]
        mov ecx, [ebp+8]
        add ecx, eax
        cmp ecx, 256
        jbe .n
        mov ecx, 256
.n:
        sub ecx, eax
        jbe u_cero
        lea esi, [pal_rgb+eax*4]
        mov eax, ecx
.c:
        movsd
        and dword [edi-4], 0xFFFFFF
        loop .c
        ret
.t:
        mov eax, 256
        ret
; COLORREF GetNearestColor(HDC, color)
g_GetNearestColor:
        mov eax, [ebp]
        call w32_dc
        jc g_mal
        call wg_tablas
        mov byte [wg_mono], 0
        mov eax, [ebp+4]
        call wg_colref
        call wg_idx
        jmp wg_rgb
; UINT GetSystemPaletteUse(HDC) y SetSystemPaletteUse(HDC, uso)
g_GetSystemPaletteUse:
        jmp u_uno

; =============================================================================
;  USER32: lo que pinta
; =============================================================================
; int FillRect(HDC, RECT*, pincel)
u_FillRect:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov eax, [ebp+8]
        call wg_ppincel
        jc .f
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        call wg_caja_sup
.l:
        cmp ebx, edx
        jge .f
        call wg_tramo
        inc ebx
        jmp .l
.f:
        call wg_cerrar
        jmp u_uno
; int FrameRect(HDC, RECT*, pincel): de un punto
u_FrameRect:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov eax, [ebp+8]
        call wg_ppincel
        jc .f
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        call wg_rect_orden
        push ebp
        mov edi, ecx
        mov ebp, edx
        mov ecx, 1
        mov edx, 1
        call wg_marco_px
        pop ebp
.f:
        call wg_cerrar
        jmp u_uno
; BOOL InvertRect(HDC, RECT*)
u_InvertRect:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov dword [wg_rop], 6
        mov dword [wg_pm], 0
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        call wg_caja_sup
.l:
        cmp ebx, edx
        jge .f
        call wg_tramo
        inc ebx
        jmp .l
.f:
        call wg_cerrar
        jmp u_uno
; BOOL DrawFocusRect(HDC, RECT*): un marco de puntos sueltos, invertidos
u_DrawFocusRect:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov dword [wg_rop], 6
        mov eax, W32_HGDI + 7       ; (una pluma de puntos)
        call wg_ppluma
        mov dword [wg_pbits], 1
        mov dword [wg_plen], 2
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        call wg_caja_sup
        dec ecx
        dec edx
        mov byte [wg_ultimo], 0
        push dword [edi+DC_MODO]
        mov dword [edi+DC_MODO], 1
        call wg_marco_pluma
        pop dword [edi+DC_MODO]
        call wg_cerrar
        jmp u_uno
; BOOL DrawEdge(HDC, RECT*, borde, BF_*)
BDR_RAISEDOUTER equ 1
BDR_SUNKENOUTER equ 2
BDR_RAISEDINNER equ 4
BDR_SUNKENINNER equ 8
u_DrawEdge:
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov eax, [ebp+12]
        mov [wg_de_bf], eax
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov [wg_t7], esi
        call wg_caja_sup
        mov [wg_de], eax
        mov [wg_de+4], ebx
        mov [wg_de+8], ecx
        mov [wg_de+12], edx
        mov edi, [ebp+8]
        test edi, BDR_RAISEDOUTER | BDR_SUNKENOUTER
        jz .i
        mov eax, 22 + 1             ; elevado: COLOR_3DLIGHT y COLOR_3DDKSHADOW
        mov ebx, 21 + 1
        test edi, BDR_SUNKENOUTER
        jz .o1
        mov eax, 16 + 1             ; hundido: COLOR_3DSHADOW y COLOR_3DHILIGHT
        mov ebx, 20 + 1
.o1:
        call wg_canto
.i:
        test edi, BDR_RAISEDINNER | BDR_SUNKENINNER
        jz .m
        mov eax, 20 + 1             ; el de dentro: 3DHILIGHT y 3DSHADOW
        mov ebx, 16 + 1
        test edi, BDR_SUNKENINNER
        jz .i1
        mov eax, 21 + 1             ; hundido: 3DDKSHADOW y 3DLIGHT
        mov ebx, 22 + 1
.i1:
        call wg_canto
.m:
        test dword [wg_de_bf], 0x800    ; BF_MIDDLE: COLOR_3DFACE
        jz .a
        mov eax, 15 + 1
        call wg_ppincel
        jc .a
        mov eax, [wg_de]
        mov ecx, [wg_de+8]
        mov ebx, [wg_de+4]
.ml:
        cmp ebx, [wg_de+12]
        jge .a
        call wg_tramo
        inc ebx
        jmp .ml
.a:
        test dword [wg_de_bf], 0x2000   ; BF_ADJUST: lo que queda dentro
        jz .f
        mov esi, [wg_t7]
        mov eax, [wg_de]
        sub eax, [wg_ox]
        mov [esi], eax
        mov eax, [wg_de+4]
        sub eax, [wg_oy]
        mov [esi+4], eax
        mov eax, [wg_de+8]
        sub eax, [wg_ox]
        mov [esi+8], eax
        mov eax, [wg_de+12]
        sub eax, [wg_oy]
        mov [esi+12], eax
.f:
        call wg_cerrar
        jmp u_uno
; (interna) un canto de DrawEdge: EAX=pincel de arriba e izquierda, EBX=el
; de abajo y derecha; wg_de queda un punto más estrecho
wg_canto:
        pushad
        push ebx
        call wg_ppincel
        jc .ab
        test byte [wg_de_bf], 2     ; BF_TOP
        jz .iz
        mov eax, [wg_de]
        mov ecx, [wg_de+8]
        mov ebx, [wg_de+4]
        call wg_tramo
.iz:
        test byte [wg_de_bf], 1     ; BF_LEFT
        jz .ab
        mov eax, [wg_de]
        mov ebx, [wg_de+4]
        mov edx, [wg_de+12]
        call wg_columna
.ab:
        pop eax
        call wg_ppincel
        jc .x
        test byte [wg_de_bf], 8     ; BF_BOTTOM
        jz .de
        mov eax, [wg_de]
        mov ecx, [wg_de+8]
        mov ebx, [wg_de+12]
        dec ebx
        call wg_tramo
.de:
        test byte [wg_de_bf], 4     ; BF_RIGHT
        jz .x
        mov eax, [wg_de+8]
        dec eax
        mov ebx, [wg_de+4]
        mov edx, [wg_de+12]
        call wg_columna
.x:
        test byte [wg_de_bf], 1
        jz .x1
        inc dword [wg_de]
.x1:
        test byte [wg_de_bf], 2
        jz .x2
        inc dword [wg_de+4]
.x2:
        test byte [wg_de_bf], 4
        jz .x3
        dec dword [wg_de+8]
.x3:
        test byte [wg_de_bf], 8
        jz .x4
        dec dword [wg_de+12]
.x4:
        popad
        ret
; (interna) la columna EAX de [EBX, EDX) con el pincel
wg_columna:
        pushad
        lea ecx, [eax+1]
.l:
        cmp ebx, edx
        jge .x
        call wg_tramo
        inc ebx
        jmp .l
.x:
        popad
        ret
