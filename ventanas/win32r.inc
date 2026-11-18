; =============================================================================
;  Hispa Ventanas - [HISPA-486] GDI32 de la fase 3d: regiones y recortes
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Una región (HRGN) es una lista de hasta RG_MAX rectángulos que no se pisan
;  (bloque RG_*, del montón). Las elípticas y las de esquinas redondeadas se
;  hacen por franjas; las de polígono, con su caja. Si una operación da más
;  rectángulos de los que caben, los últimos se juntan en su caja.
;  El recorte de un contexto (SelectClipRgn, IntersectClipRect...) es una
;  copia suya, en coordenadas del contexto (DC_RECORTE).
; =============================================================================

; ---- (interna) EDI=bloque: añade (EAX, EBX)-(ECX, EDX) si no está vacío ----
rg_anadir:
        cmp eax, ecx
        jge .x
        cmp ebx, edx
        jge .x
        push esi
        mov esi, [edi+RG_N]
        cmp esi, RG_MAX
        jb .cabe
        lea esi, [edi+RG_R+(RG_MAX-1)*16]   ; (lleno: a la caja del último)
        cmp eax, [esi]
        jge .a
        mov [esi], eax
.a:
        cmp ebx, [esi+4]
        jge .b
        mov [esi+4], ebx
.b:
        cmp ecx, [esi+8]
        jle .c
        mov [esi+8], ecx
.c:
        cmp edx, [esi+12]
        jle .d
        mov [esi+12], edx
.d:
        pop esi
        ret
.cabe:
        shl esi, 4
        lea esi, [edi+esi+RG_R]
        mov [esi], eax
        mov [esi+4], ebx
        mov [esi+8], ecx
        mov [esi+12], edx
        inc dword [edi+RG_N]
        pop esi
.x:
        ret
; ---- (interna) EDI := ESI ∩ EBX (bloques; EDI puede ser ESI o EBX) ----
rg_y:
        pushad
        push edi
        mov edi, wg_rgt
        mov dword [edi+RG_N], 0
        mov ebp, [esi+RG_N]
        lea esi, [esi+RG_R]
.a:
        test ebp, ebp
        jz .f
        push ebx
        mov ecx, [ebx+RG_N]
        lea ebx, [ebx+RG_R]
.b:
        jecxz .bs
        push ecx
        mov eax, [esi]
        cmp eax, [ebx]
        jge .1
        mov eax, [ebx]
.1:
        mov ecx, [esi+8]
        cmp ecx, [ebx+8]
        jle .2
        mov ecx, [ebx+8]
.2:
        mov edx, [esi+12]
        cmp edx, [ebx+12]
        jle .3
        mov edx, [ebx+12]
.3:
        push ebx
        mov ebx, [ebx+4]
        cmp ebx, [esi+4]
        jge .4
        mov ebx, [esi+4]
.4:
        call rg_anadir
        pop ebx
        pop ecx
        add ebx, 16
        dec ecx
        jmp .b
.bs:
        pop ebx
        add esi, 16
        dec ebp
        jmp .a
.f:
        pop edi
        mov esi, wg_rgt
        mov ecx, RG_TAM / 4
        rep movsd
        popad
        ret
; ---- (interna) EDI := ESI − EBX ----
rg_menos:
        pushad
        push edi
        mov edi, wg_rgt             ; empieza siendo ESI
        push edi
        mov ecx, RG_TAM / 4
        rep movsd
        pop edi
        mov ebp, [ebx+RG_N]
        lea ebx, [ebx+RG_R]
.q:
        test ebp, ebp
        jz .f
        ; wg_rgt := wg_rgt − el rectángulo EBX (por wg_rgu)
        push ebp
        mov esi, wg_rgt
        mov edi, wg_rgu
        mov dword [edi+RG_N], 0
        mov ebp, [esi+RG_N]
        lea esi, [esi+RG_R]
.r:
        test ebp, ebp
        jz .rf
        call .resta
        add esi, 16
        dec ebp
        jmp .r
.rf:
        mov esi, wg_rgu
        mov edi, wg_rgt
        mov ecx, RG_TAM / 4
        rep movsd
        pop ebp
        add ebx, 16
        dec ebp
        jmp .q
.f:
        pop edi
        mov esi, wg_rgt
        mov ecx, RG_TAM / 4
        rep movsd
        popad
        ret
.resta:                             ; añade a EDI el rectángulo ESI − EBX
        pushad
        mov eax, [esi]              ; ¿se tocan?
        cmp eax, [ebx+8]
        jge .entero
        mov eax, [esi+8]
        cmp eax, [ebx]
        jle .entero
        mov eax, [esi+4]
        cmp eax, [ebx+12]
        jge .entero
        mov eax, [esi+12]
        cmp eax, [ebx+4]
        jle .entero
        mov eax, [esi]              ; arriba
        mov ecx, [esi+8]
        mov edx, [ebx+4]
        mov [wg_t2], edx            ; (la franja del medio: de aquí...)
        cmp edx, [esi+4]
        jge .t1
        mov edx, [esi+4]
        mov [wg_t2], edx
.t1:
        push ebx
        mov ebx, [esi+4]
        call rg_anadir
        pop ebx
        mov edx, [ebx+12]           ; abajo
        mov [wg_t3], edx            ; (...hasta aquí)
        cmp edx, [esi+12]
        jle .t2
        mov edx, [esi+12]
        mov [wg_t3], edx
.t2:
        push ebx
        mov ebx, edx
        mov edx, [esi+12]
        call rg_anadir
        pop ebx
        mov eax, [esi]              ; la izquierda
        mov ecx, [ebx]
        push ebx
        mov ebx, [wg_t2]
        mov edx, [wg_t3]
        call rg_anadir
        pop ebx
        mov eax, [ebx+8]            ; la derecha
        mov ecx, [esi+8]
        mov ebx, [wg_t2]
        mov edx, [wg_t3]
        call rg_anadir
        popad
        ret
.entero:
        mov eax, [esi]
        mov ebx, [esi+4]
        mov ecx, [esi+8]
        mov edx, [esi+12]
        call rg_anadir
        popad
        ret
; ---- (interna) EDI := ESI ∪ EBX ----
rg_o:
        pushad
        push edi
        push esi
        mov esi, ebx
        mov edi, wg_rgv             ; wg_rgv := EBX − ESI
        mov ebx, [esp]
        call rg_menos
        pop esi
        pop edi
        push edi                    ; EDI := ESI y lo de wg_rgv
        cmp edi, esi
        je .c
        mov ecx, RG_TAM / 4
        rep movsd
.c:
        pop edi
        mov esi, wg_rgv
        mov ebp, [esi+RG_N]
        lea esi, [esi+RG_R]
.l:
        test ebp, ebp
        jz .x
        mov eax, [esi]
        mov ebx, [esi+4]
        mov ecx, [esi+8]
        mov edx, [esi+12]
        call rg_anadir
        add esi, 16
        dec ebp
        jmp .l
.x:
        popad
        ret
; ---- (interna) EDI := ESI xor EBX ----
rg_ox:
        pushad
        push edi
        push esi
        push ebx
        mov edi, wg_rgw             ; wg_rgw := ESI − EBX
        call rg_menos
        pop esi                     ; EDI := EBX − ESI
        pop ebx
        pop edi
        push edi
        push esi
        mov edi, wg_rgx
        call rg_menos
        pop esi
        pop edi
        mov esi, wg_rgw
        mov ebx, wg_rgx
        call rg_o
        popad
        ret
; ---- (interna) EDI=bloque -> EAX=NULLREGION (1), SIMPLEREGION (2) o
; COMPLEXREGION (3) ----
rg_tipo:
        mov eax, [edi+RG_N]
        cmp eax, 2
        jb .x
        mov eax, 2
.x:
        inc eax
        ret
; ---- (interna) EDI=bloque -> EAX..EDX su caja (0 si está vacío) ----
rg_caja:
        push esi
        push ebp
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        mov ebp, [edi+RG_N]
        test ebp, ebp
        jz .x
        lea esi, [edi+RG_R]
        mov eax, [esi]
        mov ebx, [esi+4]
        mov ecx, [esi+8]
        mov edx, [esi+12]
.l:
        dec ebp
        jz .x
        add esi, 16
        cmp eax, [esi]
        jle .1
        mov eax, [esi]
.1:
        cmp ebx, [esi+4]
        jle .2
        mov ebx, [esi+4]
.2:
        cmp ecx, [esi+8]
        jge .3
        mov ecx, [esi+8]
.3:
        cmp edx, [esi+12]
        jge .l
        mov edx, [esi+12]
        jmp .l
.x:
        pop ebp
        pop esi
        ret
; ---- una región nueva del programa EDX -> EAX=el objeto EDI=su bloque
; (vacío); CF=1 si no hay sitio ----
rg_nueva:
        call g_nuevo
        jc .no
        push eax
        mov ecx, RG_TAM
        call w32_pedir
        mov edi, eax
        pop eax
        jc .no
        mov byte [esi+GO_TIPO], GT_REGION
        mov [esi+GO_A], edi
        clc
        ret
.no:
        xor eax, eax
        stc
        ret
; ---- EAX=región EDX=programa -> EDI=su bloque; CF=1 si no lo es ----
rg_de:
        push esi
        call w32_obj
        jc .x
        cmp byte [esi+GO_TIPO], GT_REGION
        jne .no
        mov edi, [esi+GO_A]
        clc
.x:
        pop esi
        ret
.no:
        stc
        pop esi
        ret

; HRGN CreateRectRgn(l, t, r, b) y CreateRectRgnIndirect(RECT*)
g_CreateRectRgnIndirect:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        jmp g_rect_rgn
g_CreateRectRgn:
        mov esi, ebp
g_rect_rgn:
        push esi
        call rg_nueva
        pop esi
        jc u_cero
        push eax
        call wg_rect_orden
        call rg_anadir
        pop eax
        ret
; (interna) ESI=RECT -> EAX..EDX con izquierda < derecha y arriba < abajo
wg_rect_orden:
        mov eax, [esi]
        mov ebx, [esi+4]
        mov ecx, [esi+8]
        mov edx, [esi+12]
        cmp eax, ecx
        jle .a
        xchg eax, ecx
.a:
        cmp ebx, edx
        jle .b
        xchg ebx, edx
.b:
        ret
; HRGN CreateEllipticRgn(l, t, r, b), CreateEllipticRgnIndirect(RECT*) y
; CreateRoundRectRgn(l, t, r, b, ancho, alto): por franjas
g_CreateEllipticRgnIndirect:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        jmp g_eli_rgn
g_CreateEllipticRgn:
        mov esi, ebp
g_eli_rgn:
        mov [wg_prog], edx
        call wg_rect_orden
        call wg_filas_elipse
        jmp wg_rgn_filas
g_CreateRoundRectRgn:
        mov [wg_prog], edx
        mov esi, ebp
        mov eax, [ebp+16]
        mov [wg_t4], eax
        mov eax, [ebp+20]
        mov [wg_t5], eax
        call wg_rect_orden
        call wg_filas_redondo
; (interna) una región con las filas de wg_fil_* en RG_MAX franjas
wg_rgn_filas:
        mov edx, [wg_prog]
        call rg_nueva
        jc u_cero
        push eax
        mov ecx, [wg_fil_n]
        mov ebp, RG_MAX
        cmp ecx, ebp
        jae .n
        mov ebp, ecx
.n:
        xor esi, esi                ; la franja
.f:
        cmp esi, ebp
        jae .x
        mov eax, esi                ; de y0 = i·n/f a y1 = (i+1)·n/f
        imul eax, [wg_fil_n]
        xor edx, edx
        div ebp
        mov ebx, eax
        lea eax, [esi+1]
        imul eax, [wg_fil_n]
        xor edx, edx
        div ebp
        mov edx, eax
        mov ecx, ebx                ; la fila del medio
        add ecx, edx
        shr ecx, 1
        mov eax, [wg_fil_l+ecx*4]
        mov ecx, [wg_fil_r+ecx*4]
        add ebx, [wg_fil_y0]
        add edx, [wg_fil_y0]
        call rg_anadir
        inc esi
        jmp .f
.x:
        pop eax
        mov edx, [wg_prog]
        ret
; HRGN CreatePolygonRgn(POINT*, n, modo): su caja
g_CreatePolygonRgn:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        mov ecx, [ebp+4]
        test ecx, ecx
        jle u_cero
        mov eax, [esi]
        mov ebx, [esi+4]
        mov [wg_t0], eax
        mov [wg_t1], ebx
        mov [wg_t2], eax
        mov [wg_t3], ebx
.l:
        mov eax, [esi]
        mov ebx, [esi+4]
        cmp eax, [wg_t0]
        jge .1
        mov [wg_t0], eax
.1:
        cmp eax, [wg_t2]
        jle .2
        mov [wg_t2], eax
.2:
        cmp ebx, [wg_t1]
        jge .3
        mov [wg_t1], ebx
.3:
        cmp ebx, [wg_t3]
        jle .4
        mov [wg_t3], ebx
.4:
        add esi, 8
        loop .l
        call rg_nueva
        jc u_cero
        push eax
        mov eax, [wg_t0]
        mov ebx, [wg_t1]
        mov ecx, [wg_t2]
        mov edx, [wg_t3]
        call rg_anadir
        pop eax
        ret
; int CombineRgn(destino, uno, otro, modo)
g_CombineRgn:
        mov eax, [ebp]
        call rg_de
        jc u_cero
        push edi
        mov eax, [ebp+4]
        call rg_de
        mov esi, edi
        pop edi
        jc u_cero
        mov eax, [ebp+12]
        cmp eax, 5                  ; RGN_COPY
        jne .dos
        push edi
        mov ecx, RG_TAM / 4
        rep movsd
        pop edi
        jmp rg_tipo
.dos:
        push edi
        push eax
        mov eax, [ebp+8]
        call rg_de
        mov ebx, edi
        pop eax
        pop edi
        jc u_cero
        cmp eax, 1
        jne .o
        call rg_y
        jmp rg_tipo
.o:
        cmp eax, 2
        jne .xo
        call rg_o
        jmp rg_tipo
.xo:
        cmp eax, 3
        jne .m
        call rg_ox
        jmp rg_tipo
.m:
        cmp eax, 4
        jne u_cero
        call rg_menos
        jmp rg_tipo
; BOOL SetRectRgn(región, l, t, r, b)
g_SetRectRgn:
        mov eax, [ebp]
        call rg_de
        jc u_cero
        mov dword [edi+RG_N], 0
        lea esi, [ebp+4]
        call wg_rect_orden
        call rg_anadir
        jmp u_uno
; int OffsetRgn(región, x, y)
g_OffsetRgn:
        mov eax, [ebp]
        call rg_de
        jc u_cero
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        call rg_mover
        jmp rg_tipo
; (interna) mueve la región EDI EAX, EBX
rg_mover:
        push ecx
        push esi
        mov ecx, [edi+RG_N]
        lea esi, [edi+RG_R]
.l:
        jecxz .x
        add [esi], eax
        add [esi+4], ebx
        add [esi+8], eax
        add [esi+12], ebx
        add esi, 16
        dec ecx
        jmp .l
.x:
        pop esi
        pop ecx
        ret
; int GetRgnBox(región, RECT*)
g_GetRgnBox:
        mov eax, [ebp]
        call rg_de
        jc u_cero
        push edx
        call rg_caja
        mov esi, [esp]
        mov esi, [ebp+4]
        pop ebp                     ; (EBP = el programa, un momento)
        add esi, [ebp+PE_DELTA]
        mov [esi], eax
        mov [esi+4], ebx
        mov [esi+8], ecx
        mov [esi+12], edx
        jmp rg_tipo
; BOOL PtInRegion(región, x, y)
g_PtInRegion:
        mov eax, [ebp]
        call rg_de
        jc u_cero
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        mov ecx, [edi+RG_N]
        lea esi, [edi+RG_R]
.l:
        jecxz .no
        cmp eax, [esi]
        jl .s
        cmp eax, [esi+8]
        jge .s
        cmp ebx, [esi+4]
        jl .s
        cmp ebx, [esi+12]
        jl u_uno
.s:
        add esi, 16
        dec ecx
        jmp .l
.no:
        jmp u_cero
; BOOL RectInRegion(región, RECT*)
g_RectInRegion:
        mov eax, [ebp]
        call rg_de
        jc u_cero
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        call wg_rect_orden
        mov ebp, [edi+RG_N]
        lea esi, [edi+RG_R]
.l:
        test ebp, ebp
        jz u_cero
        cmp eax, [esi+8]
        jge .s
        cmp ecx, [esi]
        jle .s
        cmp ebx, [esi+12]
        jge .s
        cmp edx, [esi+4]
        jg u_uno
.s:
        add esi, 16
        dec ebp
        jmp .l
; BOOL EqualRgn(una, otra): la misma caja y los mismos rectángulos
g_EqualRgn:
        mov eax, [ebp]
        call rg_de
        jc u_cero
        mov esi, edi
        mov eax, [ebp+4]
        call rg_de
        jc u_cero
        mov ecx, [esi+RG_N]
        cmp ecx, [edi+RG_N]
        jne u_cero
        shl ecx, 2
        add esi, RG_R
        add edi, RG_R
        repe cmpsd
        jne u_cero
        jmp u_uno

; ---- (interna) pinta la región ESI (bloque) en el contexto abierto con el
; pincel preparado ----
wg_pintar_rg:
        pushad
        mov ebp, [esi+RG_N]
        add esi, RG_R
.r:
        test ebp, ebp
        jz .x
        mov eax, [esi]
        mov ecx, [esi+8]
        add eax, [wg_ox]
        add ecx, [wg_ox]
        mov ebx, [esi+4]
        mov edx, [esi+12]
        add ebx, [wg_oy]
        add edx, [wg_oy]
.f:
        cmp ebx, edx
        jge .s
        call wg_tramo
        inc ebx
        jmp .f
.s:
        add esi, 16
        dec ebp
        jmp .r
.x:
        popad
        ret
; BOOL FillRgn(HDC, región, pincel) y PaintRgn(HDC, región)
g_FillRgn:
        mov eax, [ebp+8]
        jmp g_pintar_rgn
g_PaintRgn:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [edi+DC_PINCEL]
g_pintar_rgn:
        push eax
        mov eax, [ebp+4]
        call rg_de
        mov esi, edi
        pop ebx
        jc u_cero
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov eax, ebx
        call wg_ppincel
        jc .f
        call wg_pintar_rg
.f:
        call wg_cerrar
        jmp u_uno
; BOOL InvertRgn(HDC, región)
g_InvertRgn:
        mov eax, [ebp+4]
        call rg_de
        jc u_cero
        mov esi, edi
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov dword [wg_rop], 6       ; R2_NOT
        mov dword [wg_pm], 0
        call wg_pintar_rg
        call wg_cerrar
        jmp u_uno
; BOOL FrameRgn(HDC, región, pincel, ancho, alto): el borde de cada rectángulo
g_FrameRgn:
        mov eax, [ebp+4]
        call rg_de
        jc u_cero
        mov esi, edi
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov eax, [ebp+8]
        call wg_ppincel
        jc .f
        push ebp
        mov ecx, [ebp+12]
        mov edx, [ebp+16]
        mov ebp, [esi+RG_N]
        add esi, RG_R
.r:
        test ebp, ebp
        jz .rf
        pushad
        mov eax, [esi]
        mov ebx, [esi+4]
        mov edi, [esi+8]
        mov ebp, [esi+12]
        call wg_marco_px
        popad
        add esi, 16
        dec ebp
        jmp .r
.rf:
        pop ebp
.f:
        call wg_cerrar
        jmp u_uno
; (interna) el marco de (EAX, EBX)-(EDI, EBP) (lógicas) con el pincel, de ECX
; de ancho y EDX de alto
wg_marco_px:
        add eax, [wg_ox]
        add edi, [wg_ox]
        add ebx, [wg_oy]
        add ebp, [wg_oy]
        push ecx                    ; arriba y abajo
        mov esi, edx
.ar:
        test esi, esi
        jz .ab0
        push eax
        mov ecx, edi
        call wg_tramo
        push ebx
        mov ebx, ebp
        sub ebx, esi
        call wg_tramo
        pop ebx
        pop eax
        inc ebx
        dec esi
        jmp .ar
.ab0:
        sub ebx, edx
        pop ecx
        push ebx                    ; los lados
.l:
        cmp ebx, ebp
        jge .x
        push ecx
        push eax
        mov esi, ecx
        lea ecx, [eax+esi]
        call wg_tramo
        mov ecx, edi
        sub edi, esi
        mov eax, edi
        add edi, esi
        call wg_tramo
        pop eax
        pop ecx
        inc ebx
        jmp .l
.x:
        pop ebx
        ret

; =============================================================================
;  El recorte de un contexto
; =============================================================================
; (interna) EDI=contexto: su recorte (el que tenga, o uno de todo) en
; wg_rgc; CF=1 si no tenía
wg_recorte_tomar:
        push ecx
        push esi
        push edi
        mov esi, [edi+DC_RECORTE]
        mov edi, wg_rgc
        test esi, esi
        jz .todo
        mov ecx, RG_TAM / 4
        rep movsd
        pop edi
        pop esi
        pop ecx
        clc
        ret
.todo:
        mov dword [edi+RG_N], 1
        mov dword [edi+RG_R], -32768
        mov dword [edi+RG_R+4], -32768
        mov dword [edi+RG_R+8], 32767
        mov dword [edi+RG_R+12], 32767
        pop edi
        pop esi
        pop ecx
        stc
        ret
; (interna) EDI=contexto: wg_rgc pasa a ser su recorte -> EAX=el tipo
wg_recorte_poner:
        push esi
        push edi
        push ecx
        mov esi, [edi+DC_RECORTE]
        test esi, esi
        jnz .hay
        push edi
        mov ecx, RG_TAM
        call w32_pedir
        pop edi
        jc .x
        mov [edi+DC_RECORTE], eax
        mov esi, eax
.hay:
        mov edi, esi
        mov esi, wg_rgc
        mov ecx, RG_TAM / 4
        rep movsd
.x:
        mov edi, wg_rgc
        call rg_tipo
        pop ecx
        pop edi
        pop esi
        ret
; int SelectClipRgn(HDC, región o 0)
g_SelectClipRgn:
        mov ecx, 5                  ; (RGN_COPY)
        jmp g_clip_rgn
; int ExtSelectClipRgn(HDC, región o 0, modo)
g_ExtSelectClipRgn:
        mov ecx, [ebp+8]
g_clip_rgn:
        mov [wg_t7], ecx
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov eax, [ebp+4]
        test eax, eax
        jnz .r
        cmp dword [wg_t7], 5        ; ninguna: sin recorte
        jne u_cero
        mov eax, [edi+DC_RECORTE]
        call mem_soltar
        mov dword [edi+DC_RECORTE], 0
        mov eax, 2                  ; SIMPLEREGION
        ret
.r:
        push edi
        call rg_de
        mov esi, edi
        pop edi
        jc u_cero
        push edi
        mov edi, wg_rgd             ; (una copia de la región)
        mov ecx, RG_TAM / 4
        rep movsd
        pop edi
        mov eax, [wg_t7]
        cmp eax, 5
        jne .op
        mov esi, wg_rgd
        push edi
        mov edi, wg_rgc
        mov ecx, RG_TAM / 4
        rep movsd
        pop edi
        jmp wg_recorte_poner
.op:
        call wg_recorte_tomar
        push edi
        mov esi, wg_rgc
        mov ebx, wg_rgd
        mov edi, wg_rgc
        cmp eax, 1
        jne .o2
        call rg_y
        jmp .of
.o2:
        cmp eax, 2
        jne .o3
        call rg_o
        jmp .of
.o3:
        cmp eax, 3
        jne .o4
        call rg_ox
        jmp .of
.o4:
        call rg_menos
.of:
        pop edi
        jmp wg_recorte_poner
; int IntersectClipRect(HDC, l, t, r, b) y ExcludeClipRect(HDC, l, t, r, b)
g_IntersectClipRect:
        mov bl, 1
        jmp g_clip_rect
g_ExcludeClipRect:
        mov bl, 4
g_clip_rect:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        push ebx
        lea esi, [ebp+4]
        call wg_rect_orden          ; (a coordenadas del contexto)
        mov esi, [edi+DC_DX]
        sub esi, [edi+DC_WX]
        add eax, esi
        add ecx, esi
        mov esi, [edi+DC_DY]
        sub esi, [edi+DC_WY]
        add ebx, esi
        add edx, esi
        push edi
        mov edi, wg_rgd
        mov dword [edi+RG_N], 0
        call rg_anadir
        pop edi
        call wg_recorte_tomar
        pop eax
        push edi
        mov esi, wg_rgc
        mov ebx, wg_rgd
        mov edi, wg_rgc
        cmp al, 1
        jne .m
        call rg_y
        jmp .f
.m:
        call rg_menos
.f:
        pop edi
        jmp wg_recorte_poner
; int OffsetClipRgn(HDC, x, y)
g_OffsetClipRgn:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov edi, [edi+DC_RECORTE]
        test edi, edi
        jz .x
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        call rg_mover
        jmp rg_tipo
.x:
        mov eax, 2
        ret
; int GetClipBox(HDC, RECT*): en lógicas
g_GetClipBox:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        push edi
        call wg_dc_caja             ; EAX..EDX: todo lo del contexto
        mov edi, [esp]
        mov edi, [edi+DC_RECORTE]
        test edi, edi
        jz .c
        push eax
        push ebx
        push ecx
        push edx
        call rg_caja
        cmp eax, [esp+12]
        jge .1
        mov eax, [esp+12]
.1:
        cmp ebx, [esp+8]
        jge .2
        mov ebx, [esp+8]
.2:
        cmp ecx, [esp+4]
        jle .3
        mov ecx, [esp+4]
.3:
        cmp edx, [esp]
        jle .4
        mov edx, [esp]
.4:
        add esp, 16
.c:
        pop edi
        mov esi, [edi+DC_DX]        ; a lógicas
        sub esi, [edi+DC_WX]
        sub eax, esi
        sub ecx, esi
        mov esi, [edi+DC_DY]
        sub esi, [edi+DC_WY]
        sub ebx, esi
        sub edx, esi
        mov esi, [ebp+4]
        push edx
        mov edx, [wg_prog]
        add esi, [edx+PE_DELTA]
        pop edx
        mov [esi], eax
        mov [esi+4], ebx
        mov [esi+8], ecx
        mov [esi+12], edx
        cmp eax, ecx
        jge .vacio
        cmp ebx, edx
        jge .vacio
        mov eax, 2
        cmp dword [edi+DC_RECORTE], 0
        je .y
        mov esi, [edi+DC_RECORTE]
        cmp dword [esi+RG_N], 1
        jbe .y
        mov eax, 3
.y:
        ret
.vacio:
        mov eax, 1
        ret
; (interna) EDI=contexto -> EAX..EDX = lo que mide (su zona o su mapa), en
; coordenadas del contexto
wg_dc_caja:
        push esi
        push ebp
        mov [wg_prog], edx
        xor eax, eax
        xor ebx, ebx
        cmp dword [edi+DC_USO], 3
        jne .v
        push eax
        mov eax, [edi+DC_BMP]
        call w32_obj
        pop eax
        mov ecx, [esi+GO_B]
        mov edx, [esi+GO_C]
        jmp .x
.v:
        mov ebp, [edi+DC_VEN]
        call w32_cli
        xor eax, eax
        xor ebx, ebx
.x:
        pop ebp
        pop esi
        ret
; int GetClipRgn(HDC, región) -> 1 si tiene recorte (copiado), 0 si no
g_GetClipRgn:
        mov eax, [ebp]
        call w32_dc
        jc g_mal
        mov esi, [edi+DC_RECORTE]
        test esi, esi
        jz u_cero
        mov eax, [ebp+4]
        call rg_de
        jc g_mal
        mov ecx, RG_TAM / 4
        rep movsd
        jmp u_uno
; BOOL PtVisible(HDC, x, y) y RectVisible(HDC, RECT*)
g_PtVisible:
        jmp u_uno
g_RectVisible:
        jmp u_uno

; =============================================================================
;  USER32: lo que hay que repintar
; =============================================================================
; BOOL InvalidateRgn(ventana, región, borrar): su caja
u_InvalidateRgn:
        mov eax, [ebp+4]
        test eax, eax
        jz u_InvalidateRect
        call rg_de
        jc u_cero
        call rg_caja
        sub esp, 16
        mov [esp], eax
        mov [esp+4], ebx
        mov [esp+8], ecx
        mov [esp+12], edx
        mov edx, [wg_prog]
        mov eax, esp
        sub eax, [edx+PE_DELTA]
        mov [ebp+4], eax
        call u_InvalidateRect
        add esp, 16
        ret
; BOOL ValidateRgn(ventana, región)
u_ValidateRgn:
        jmp u_ValidateRect
; int GetUpdateRgn(ventana, región, borrar)
u_GetUpdateRgn:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, eax
        mov eax, [ebp+4]
        call rg_de
        jc u_cero
        mov dword [edi+RG_N], 0
        push ebp
        mov ebp, esi
        call w32_sucio              ; EAX..EDX: lo que hay que repintar (zona)
        pop ebp
        call rg_anadir
        jmp rg_tipo
; BOOL GetUpdateRect(ventana, RECT*, borrar)
u_GetUpdateRect:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        push ebp
        mov ebp, eax
        call w32_sucio
        pop ebp
        mov esi, [ebp+4]
        test esi, esi
        jz .r
        push eax
        mov eax, [wg_prog]
        add esi, [eax+PE_DELTA]
        pop eax
        mov [esi], eax
        mov [esi+4], ebx
        mov [esi+8], ecx
        mov [esi+12], edx
.r:
        cmp eax, ecx
        jl u_uno
        jmp u_cero
; (interna) EBP=ventana -> EAX..EDX lo que hay que repintar, en coordenadas de
; su zona (vacío: 0) (conserva EDX... no: EDX es la salida; ESI se respeta)
w32_sucio:
        push esi
        mov esi, [ebp+V_UX0]
        cmp esi, [ebp+V_UX1]
        jl .hay
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        pop esi
        ret
.hay:
        call ven_cli
        mov ecx, [ebp+V_UX1]
        sub ecx, eax
        mov edx, [ebp+V_UY1]
        sub edx, ebx
        mov esi, [ebp+V_UX0]
        sub esi, eax
        mov eax, esi
        mov esi, [ebp+V_UY0]
        sub esi, ebx
        mov ebx, esi
        test eax, eax
        jns .a
        xor eax, eax
.a:
        test ebx, ebx
        jns .b
        xor ebx, ebx
.b:
        pop esi
        ret
