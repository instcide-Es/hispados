; =============================================================================
;  Hispa Ventanas - [HISPA-486] USER32 de la fase 3d, COMDLG32, ADVAPI32 y
;  SHELL32
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Lo que piden los programas sencillos de la época además de lo de las fases
;  3b y 3c: las barras de desplazamiento de las ventanas (WS_VSCROLL y
;  WS_HSCROLL: barras c_barra de Hispa Ventanas, hijas de la ventana, que se
;  le quitan a su zona), el cursor de escribir (caret), el estado del
;  teclado, los aceleradores, los iconos y los cursores (de los recursos o de
;  serie; el icono de la ventana va a su botón de la barra de tareas), el
;  portapapeles, los menús emergentes (TrackPopupMenu), los rectángulos y
;  las coordenadas; los diálogos comunes (abrir y guardar con el de Hispa
;  Ventanas, elegir color y letra), y el registro y SHELL32, que no hay (sus
;  funciones dicen que no). Sala limpia: a partir de la documentación pública.
; =============================================================================

W32_HESC    equ 0x0FF0              ; el HWND del escritorio (GetDesktopWindow)
W32_HCUR    equ 0x3000              ; un cursor de serie: esto + su forma (CUR_*)
GT_ACEL     equ 10                  ; (GO_TIPO) una tabla de aceleradores
WS_VSCROLL_ equ 0x00200000
WS_HSCROLL_ equ 0x00100000

; =============================================================================
;  Las barras de desplazamiento
; =============================================================================
; ---- EBP=ventana -> EAX, EBX el origen de su zona (superficie), ECX, EDX
; lo que mide para el programa (sin sus barras de desplazamiento) ----
w32_cli:
        call ven_cli
        cmp dword [ebp+V_PROC], w32_ven_proc
        jne .x
        push esi
        mov esi, [ebp+V_D4]
        test esi, esi
        jz .y
        cmp dword [esi+VB_SBV], 0
        je .h
        sub ecx, BD_AN
        jns .h
        xor ecx, ecx
.h:
        cmp dword [esi+VB_SBH], 0
        je .y
        sub edx, BD_AN
        jns .y
        xor edx, edx
.y:
        pop esi
.x:
        ret
; ---- (CreateWindowExA) ESI=la ventana nueva EDX=programa: sus barras y el
; icono de su botón ----
w32x_nueva:
        pushad
        mov ebp, esi
        mov eax, [ebp+V_D2]
        mov eax, [eax+WC_ICONO]
        call w32x_icono_barra
        mov eax, [ebp+V_D3]
        test eax, WS_VSCROLL_
        jz .h
        xor ebx, ebx
        call w32x_barra_crear
.h:
        mov eax, [ebp+V_D3]
        test eax, WS_HSCROLL_
        jz .x
        mov ebx, 1
        call w32x_barra_crear
.x:
        call w32x_colocar
        popad
        ret
; ---- EBP=ventana EBX=0 vertical, 1 horizontal: le hace esa barra ----
w32x_barra_crear:
        pushad
        mov edi, [ebp+V_D4]
        cmp dword [edi+VB_SBV+ebx*4], 0
        jne .x
        sub esp, P_TAM
        mov dword [esp+P_CLASE], c_barra
        mov dword [esp+P_TEXTO], 0
        mov eax, E_HIJA
        test ebx, ebx
        jz .e
        or eax, BDS_HORIZ
.e:
        mov [esp+P_ESTILO], eax
        xor eax, eax
        mov [esp+P_X], eax
        mov [esp+P_Y], eax
        mov dword [esp+P_AN], BD_AN
        mov dword [esp+P_AL], BD_AN
        mov [esp+P_ID], eax
        mov esi, esp
        push ebx
        call ven_crear
        pop ebx
        add esp, P_TAM
        test eax, eax
        jz .x
        mov [edi+VB_SBV+ebx*4], eax
        imul ecx, ebx, 20           ; de 0 a 100, como Windows
        lea ecx, [edi+VB_SIV+ecx]
        mov dword [ecx], 0
        mov dword [ecx+4], 100
        mov dword [ecx+8], 0
        mov dword [ecx+12], 0
        mov dword [ecx+16], 0
        call w32x_sb_sync
.x:
        popad
        ret
; ---- EBP=ventana: pone sus barras en su sitio ----
w32x_colocar:
        cmp dword [ebp+V_PROC], w32_ven_proc
        jne .r
        pushad
        mov edi, [ebp+V_D4]
        test edi, edi
        jz .x
        call ven_cli                ; ECX, EDX = lo que mide su zona
        mov esi, ecx
        mov ebx, edx
        cmp dword [edi+VB_SBV], 0
        je .h
        push ebp
        mov ebp, [edi+VB_SBV]
        lea eax, [esi-BD_AN]
        push ebx
        mov edx, ebx
        cmp dword [edi+VB_SBH], 0
        je .v1
        sub edx, BD_AN
.v1:
        xor ebx, ebx
        mov ecx, BD_AN
        call w32x_mover
        pop ebx
        pop ebp
.h:
        cmp dword [edi+VB_SBH], 0
        je .x
        push ebp
        mov ebp, [edi+VB_SBH]
        mov ecx, esi
        cmp dword [edi+VB_SBV], 0
        je .h1
        sub ecx, BD_AN
.h1:
        xor eax, eax
        sub ebx, BD_AN
        mov edx, BD_AN
        call w32x_mover
        pop ebp
.x:
        popad
.r:
        ret
; (interna) mueve la ventana EBP si no está ya ahí (EAX..EDX como ven_mover)
w32x_mover:
        test ecx, ecx
        jns .a
        xor ecx, ecx
.a:
        test edx, edx
        jns .b
        xor edx, edx
.b:
        jmp ven_mover
; ---- EBP=ventana EBX=0 o 1: su barra, como dice su SCROLLINFO ----
w32x_sb_sync:
        pushad
        mov edi, [ebp+V_D4]
        mov eax, [edi+VB_SBV+ebx*4]
        test eax, eax
        jz .x
        imul esi, ebx, 20
        lea esi, [edi+VB_SIV+esi]
        mov ebp, eax
        call w32x_sb_poner
.x:
        popad
        ret
; ---- EBP=una barra (c_barra) ESI=su SCROLLINFO (mín., máx., página, pos.) ----
w32x_sb_poner:
        pushad
        mov ecx, [esi+4]            ; la posición más alta: máx − mín − (página − 1)
        sub ecx, [esi]
        mov edx, [esi+8]
        test edx, edx
        jz .p
        dec edx
        sub ecx, edx
.p:
        test ecx, ecx
        jns .p1
        xor ecx, ecx
.p1:
        mov edx, [esi+8]
        mov eax, BDM_PONER
        call msg_enviar
        mov ecx, [esi+12]
        sub ecx, [esi]
        mov eax, BDM_POS
        call msg_enviar
        popad
        ret
; ---- (w32_traducir) WM_VSCROLL o WM_HSCROLL de una barra: EAX, ECX
; (posición | lo apretado << 24), EDX=la barra; EBP=ventana -> los de
; Windows ----
w32x_scroll:
        push ebx
        push esi
        push edi
        mov ebx, ecx
        shr ebx, 24                 ; lo apretado -> SB_*
        movzx ebx, byte [w32x_sb_cod+ebx]
        and ecx, 0xFFFFFF
        mov esi, [ebp+V_D4]
        xor edi, edi                ; EDI = lParam
        cmp edx, [esi+VB_SBV]
        je .v
        cmp edx, [esi+VB_SBH]
        je .h
        add ecx, [edx+V_D6]         ; un control SCROLLBAR: su mínimo
        push eax
        mov eax, edx
        call w32_hwnd
        mov edi, eax
        pop eax
        jmp .f
.v:
        lea esi, [esi+VB_SIV]
        jmp .s
.h:
        lea esi, [esi+VB_SIH]
.s:
        add ecx, [esi]
        mov [esi+16], ecx           ; nTrackPos
        cmp ebx, 5                  ; (al arrastrar no cambia la posición)
        je .f
        mov [esi+12], ecx
.f:
        shl ecx, 16
        or ecx, ebx
        mov edx, edi
        pop edi
        pop esi
        pop ebx
        clc
        ret
; lo apretado (V_D4 de c_barra) -> SB_*: 0 la rueda (SB_THUMBPOSITION),
; 1 y 2 las flechas, 3 y 4 la pista, 5 el pulgar (SB_THUMBTRACK)
w32x_sb_cod db 4, 0, 1, 2, 3, 5, 4, 4
; (interna) EAX=HWND [EBP+4]=la barra (SB_HORZ, SB_VERT, SB_CTL) EDX=programa
; -> ESI=su SCROLLINFO (mín., máx., página, pos., arrastre) EDI=la barra
; (c_barra; 0: no hay) EBX=la ventana; CF=1 si no vale. SB_CTL: en una
; copia (w32x_si), que w32x_ctl_guardar devuelve al control
w32x_si_de:
        call w32_ven
        jz .no
        mov ebx, eax
        mov ecx, [ebp+4]
        cmp ecx, 2
        je .ctl
        cmp dword [ebx+V_PROC], w32_ven_proc
        jne .no
        cmp ecx, 1
        ja .no
        xor ecx, 1                  ; (SB_HORZ 0 -> la 2.ª, SB_VERT 1 -> la 1.ª)
        mov esi, [ebx+V_D4]
        mov edi, [esi+VB_SBV+ecx*4]
        imul ecx, 20
        lea esi, [esi+VB_SIV+ecx]
        clc
        ret
.ctl:
        cmp dword [ebx+V_CLASE], c_barra
        jne .no
        mov edi, ebx
        mov esi, w32x_si
        mov eax, [edi+V_D6]
        mov [esi], eax
        mov eax, [edi+V_D7]
        mov [esi+4], eax
        mov eax, [edi+V_D2]
        mov [esi+8], eax
        mov eax, [edi+V_D0]
        add eax, [edi+V_D6]
        mov [esi+12], eax
        mov [esi+16], eax
        clc
        ret
.no:
        stc
        ret
; (interna) tras cambiar ESI: a la barra EDI (y si es un control, su mínimo y
; su máximo)
w32x_si_fin:
        test edi, edi
        jz .x
        cmp esi, w32x_si
        jne .p
        mov eax, [esi]
        mov [edi+V_D6], eax
        mov eax, [esi+4]
        mov [edi+V_D7], eax
.p:
        push ebp
        mov ebp, edi
        call w32x_sb_poner
        pop ebp
.x:
        ret
; (interna) la ventana EBX no tenía esa barra: se le hace (Windows la saca al
; darle un recorrido) y se le avisa de que su zona ha cambiado
w32x_si_nueva:
        test edi, edi
        jnz .x
        cmp dword [ebx+V_PROC], w32_ven_proc
        jne .x
        push ebp
        push ebx
        mov ebx, [ebp+4]
        xor ebx, 1
        mov ebp, [esp]
        call w32x_barra_crear
        call w32x_colocar
        mov eax, WM_SIZE | W32_DIFERIDO
        xor ecx, ecx
        xor edx, edx
        call msg_poner
        pop ebx
        pop ebp
        mov eax, [ebp]
        call w32x_si_de
.x:
        ret
; int SetScrollInfo(ventana, barra, SCROLLINFO*, repintar) -> la posición
u_SetScrollInfo:
        mov eax, [ebp]
        call w32x_si_de
        jc u_cero
        call w32x_si_nueva
        mov ebx, [ebp+8]
        add ebx, [edx+PE_DELTA]
        mov ecx, [ebx+4]            ; fMask
        test ecx, 1                 ; SIF_RANGE
        jz .p
        mov eax, [ebx+8]
        mov [esi], eax
        mov eax, [ebx+12]
        mov [esi+4], eax
.p:
        test ecx, 2                 ; SIF_PAGE
        jz .o
        mov eax, [ebx+16]
        mov [esi+8], eax
.o:
        test ecx, 4                 ; SIF_POS
        jz .f
        mov eax, [ebx+20]
        mov [esi+12], eax
.f:
        call w32x_si_acotar
        call w32x_si_fin
        mov eax, [esi+12]
        ret
; (interna) la posición de ESI, dentro de su recorrido
w32x_si_acotar:
        push eax
        push ebx
        mov eax, [esi+4]
        mov ebx, [esi+8]
        test ebx, ebx
        jz .a
        dec ebx
        sub eax, ebx
.a:
        cmp eax, [esi]
        jge .b
        mov eax, [esi]
.b:
        cmp [esi+12], eax
        jle .c
        mov [esi+12], eax
.c:
        mov eax, [esi]
        cmp [esi+12], eax
        jge .d
        mov [esi+12], eax
.d:
        mov eax, [esi+12]
        mov [esi+16], eax
        pop ebx
        pop eax
        ret
; BOOL GetScrollInfo(ventana, barra, SCROLLINFO*)
u_GetScrollInfo:
        mov eax, [ebp]
        call w32x_si_de
        jc u_cero
        mov ebx, [ebp+8]
        add ebx, [edx+PE_DELTA]
        mov ecx, [ebx+4]
        test ecx, 1
        jz .p
        mov eax, [esi]
        mov [ebx+8], eax
        mov eax, [esi+4]
        mov [ebx+12], eax
.p:
        test ecx, 2
        jz .o
        mov eax, [esi+8]
        mov [ebx+16], eax
.o:
        test ecx, 4
        jz .t
        mov eax, [esi+12]
        mov [ebx+20], eax
.t:
        test ecx, 16                ; SIF_TRACKPOS
        jz .x
        mov eax, [esi+16]
        mov [ebx+24], eax
.x:
        jmp u_uno
; int SetScrollPos(ventana, barra, pos., repintar) -> la de antes
u_SetScrollPos:
        mov eax, [ebp]
        call w32x_si_de
        jc u_cero
        mov eax, [ebp+8]
        xchg eax, [esi+12]
        push eax
        call w32x_si_acotar
        call w32x_si_fin
        pop eax
        ret
; int GetScrollPos(ventana, barra)
u_GetScrollPos:
        mov eax, [ebp]
        call w32x_si_de
        jc u_cero
        mov eax, [esi+12]
        ret
; BOOL SetScrollRange(ventana, barra, mín., máx., repintar)
u_SetScrollRange:
        mov eax, [ebp]
        call w32x_si_de
        jc u_cero
        mov eax, [ebp+8]
        cmp eax, [ebp+12]
        je .c
        call w32x_si_nueva
.c:
        mov eax, [ebp+8]
        mov [esi], eax
        mov eax, [ebp+12]
        mov [esi+4], eax
        mov dword [esi+8], 0
        call w32x_si_acotar
        call w32x_si_fin
        jmp u_uno
; BOOL GetScrollRange(ventana, barra, int* mín., int* máx.)
u_GetScrollRange:
        mov eax, [ebp]
        call w32x_si_de
        jc u_cero
        mov edi, [ebp+8]
        add edi, [edx+PE_DELTA]
        mov eax, [esi]
        mov [edi], eax
        mov edi, [ebp+12]
        add edi, [edx+PE_DELTA]
        mov eax, [esi+4]
        mov [edi], eax
        jmp u_uno

; =============================================================================
;  El cursor de escribir (caret): uno por programa, invertido, sin parpadear
; =============================================================================
; ---- EDX=programa: lo invierte en la pantalla ----
w32x_caret_xor:
        pushad
        mov ebp, [edx+PE_CAR_VEN]
        call w32x_caret_vale
        jc .x
        call wg_tablas
        mov byte [wg_mono], 0
        mov dword [wg_rop], 6       ; R2_NOT
        xor al, al
        call reg_ventana
        call w32_cli
        mov [wg_dox], eax
        mov [wg_doy], ebx
        add ecx, eax
        add edx, ebx
        call reg_cortar
        call dc_origen
        mov edx, [esp+20]           ; (el programa)
        mov eax, [wg_dox]
        add eax, [edx+PE_CAR_X]
        mov ecx, eax
        add ecx, [edx+PE_CAR_W]
        mov ebx, [wg_doy]
        add ebx, [edx+PE_CAR_Y]
        mov esi, [edx+PE_CAR_H]
.f:
        test esi, esi
        jz .c
        call wg_tramo_c
        inc ebx
        dec esi
        jmp .f
.c:
        call gdi_recorte_total
        mov edx, [esp+20]
        xor byte [edx+PE_CAR_DIB], 1
.x:
        popad
        ret
; ---- EBP=ventana EDX=programa: CF=1 si no es una ventana suya ----
w32x_caret_vale:
        test ebp, ebp
        jz .no
        cmp dword [ebp+V_USO], 1
        jne .no
        cmp dword [ebp+V_PROC], w32_ven_proc
        jne .no
        cmp [ebp+V_D0], edx
        jne .no
        clc
        ret
.no:
        stc
        ret
; BOOL CreateCaret(ventana, mapa, ancho, alto)
u_CreateCaret:
        call w32x_caret_quitar
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov [edx+PE_CAR_VEN], eax
        mov eax, [ebp+8]
        test eax, eax
        jnz .w
        mov eax, 2
.w:
        mov [edx+PE_CAR_W], eax
        mov eax, [ebp+12]
        test eax, eax
        jnz .h
        mov eax, 16
.h:
        mov [edx+PE_CAR_H], eax
        mov dword [edx+PE_CAR_N], 1 ; (se crea escondido)
        mov dword [edx+PE_CAR_DIB], 0
        jmp u_uno
; BOOL DestroyCaret(void)
u_DestroyCaret:
        call w32x_caret_quitar
        jmp u_uno
w32x_caret_quitar:
        cmp dword [edx+PE_CAR_DIB], 0
        je .q
        call w32x_caret_xor
.q:
        mov dword [edx+PE_CAR_VEN], 0
        mov dword [edx+PE_CAR_DIB], 0
        ret
; BOOL ShowCaret(ventana) y HideCaret(ventana)
u_ShowCaret:
        cmp dword [edx+PE_CAR_VEN], 0
        je u_cero
        cmp dword [edx+PE_CAR_N], 0
        je u_uno
        dec dword [edx+PE_CAR_N]
        jnz u_uno
        cmp dword [edx+PE_CAR_DIB], 0
        jne u_uno
        call w32x_caret_xor
        jmp u_uno
u_HideCaret:
        cmp dword [edx+PE_CAR_VEN], 0
        je u_cero
        cmp dword [edx+PE_CAR_DIB], 0
        je .n
        call w32x_caret_xor
.n:
        inc dword [edx+PE_CAR_N]
        jmp u_uno
; BOOL SetCaretPos(x, y) y GetCaretPos(POINT*)
u_SetCaretPos:
        cmp dword [edx+PE_CAR_DIB], 0
        je .p
        call w32x_caret_xor
.p:
        mov eax, [ebp]
        mov [edx+PE_CAR_X], eax
        mov eax, [ebp+4]
        mov [edx+PE_CAR_Y], eax
        cmp dword [edx+PE_CAR_VEN], 0
        je u_uno
        cmp dword [edx+PE_CAR_N], 0
        jne u_uno
        call w32x_caret_xor
        jmp u_uno
u_GetCaretPos:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        mov eax, [edx+PE_CAR_X]
        mov [edi], eax
        mov eax, [edx+PE_CAR_Y]
        mov [edi+4], eax
        jmp u_uno
u_GetCaretBlinkTime:
        mov eax, 530
        ret
; ---- (BeginPaint) EBP=ventana EDX=programa: si el caret está en ella, se
; quita mientras se pinta; (EndPaint) se vuelve a poner ----
w32x_caret_antes:
        cmp [edx+PE_CAR_VEN], ebp
        jne .x
        cmp dword [edx+PE_CAR_DIB], 0
        je .x
        call w32x_caret_xor
.x:
        ret
w32x_caret_despues:
        cmp [edx+PE_CAR_VEN], ebp
        jne .x
        cmp dword [edx+PE_CAR_N], 0
        jne .x
        cmp dword [edx+PE_CAR_DIB], 0
        jne .x
        call w32x_caret_xor
.x:
        ret

; =============================================================================
;  El teclado
; =============================================================================
; (interna) EAX=tecla virtual EBX=estado (TM_*) -> EAX=su estado como
; GetKeyState (bit 15: apretada; bit 0: activada)
w32x_tecla_est:
        push ecx
        xor ecx, ecx
        cmp eax, 0x10               ; VK_SHIFT
        je .may
        cmp eax, 0xA0
        je .may
        cmp eax, 0xA1
        je .may
        cmp eax, 0x11               ; VK_CONTROL
        je .ctrl
        cmp eax, 0xA2
        je .ctrl
        cmp eax, 0xA3
        je .ctrl
        cmp eax, 0x12               ; VK_MENU
        je .alt
        cmp eax, 0xA4
        je .alt
        cmp eax, 0xA5
        je .alt
        push ebx
        mov ebx, [cero]
        mov bl, [ebx+0x417]         ; las de activar, de la BIOS
        cmp eax, 0x14               ; VK_CAPITAL
        jne .t1
        shr bl, 6
        jmp .act
.t1:
        cmp eax, 0x90               ; VK_NUMLOCK
        jne .t2
        shr bl, 5
        jmp .act
.t2:
        cmp eax, 0x91               ; VK_SCROLL
        jne .t3
        shr bl, 4
.act:
        and ebx, 1
        mov ecx, ebx
.t3:
        pop ebx
        jmp .x
.may:
        test bl, TM_MAYUS
        jmp .dn
.ctrl:
        test bl, TM_CTRL
        jmp .dn
.alt:
        test bl, TM_ALT
.dn:
        jz .x
        or ecx, 0xFFFF8000
.x:
        mov eax, ecx
        pop ecx
        ret
; ---- (w32_tomar, w32_ven_proc) una tecla del programa ESI: EAX=WM_KEYDOWN
; o WM_CHAR de Hispa Ventanas, ECX=su tecla, EDX=las de mayúsculas ----
w32x_tecla_mod:
        push edx
        and edx, TM_MAYUS | TM_CTRL | TM_ALT
        cmp eax, WM_KEYDOWN
        jne .p
        cmp ecx, TC_CTRL            ; (Ctrl+letra)
        jb .p
        or edx, TM_CTRL
.p:
        mov [esi+PE_TEC_MOD], edx
        pop edx
        ret
; SHORT GetKeyState(tecla): como estaba con el último mensaje del teclado
u_GetKeyState:
        mov eax, [ebp]
        cmp eax, 1                  ; los botones del ratón
        je .bot
        cmp eax, 2
        je .bot
        mov ebx, [edx+PE_TEC_MOD]
        jmp w32x_tecla_est
.bot:
        mov ecx, eax
        dec ecx
        xor eax, eax
        bt dword [edx+PE_BOT], ecx
        jnc .x
        or eax, 0xFFFF8000
.x:
        ret
; SHORT GetAsyncKeyState(tecla): como está ahora
u_GetAsyncKeyState:
        mov eax, [ebp]
        movzx ebx, byte [tec_mayus]
        jmp w32x_tecla_est
; BOOL GetKeyboardState(BYTE[256])
u_GetKeyboardState:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        push edi
        mov ecx, 64
        xor eax, eax
        rep stosd
        pop edi
        mov ebx, [edx+PE_TEC_MOD]
        mov esi, w32x_vks
.l:
        movzx eax, byte [esi]
        test eax, eax
        jz .b
        inc esi
        push eax
        call w32x_tecla_est
        mov ecx, eax
        pop eax
        test ecx, ecx
        jns .s
        or byte [edi+eax], 0x80
.s:
        and cl, 1
        or [edi+eax], cl
        jmp .l
.b:
        test byte [edx+PE_BOT], 1
        jz .b2
        mov byte [edi+1], 0x80
.b2:
        test byte [edx+PE_BOT], 2
        jz u_uno
        mov byte [edi+2], 0x80
        jmp u_uno
w32x_vks    db 0x10, 0x11, 0x12, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x14, 0x90, 0x91, 0
; int ToAscii(tecla, código, estado, WORD* carácter, banderas): el carácter
; de la tecla que se acaba de tomar
u_ToAscii:
        mov eax, [edx+PE_TEC_ULT]
        cmp dword [ebp], 0x20       ; (VK_SPACE: un espacio)
        jne .c
        mov eax, ' '
.c:
        test eax, eax
        jz u_cero
        mov edi, [ebp+12]
        add edi, [edx+PE_DELTA]
        mov [edi], ax
        jmp u_uno
; UINT MapVirtualKeyA(código, tipo): de tecla virtual a carácter (2)
u_MapVirtualKeyA:
        mov eax, [ebp]
        cmp dword [ebp+4], 2
        jne u_cero
        cmp eax, ' '
        je .x
        cmp eax, '0'
        jb u_cero
        cmp eax, '9'
        jbe .x
        cmp eax, 'A'
        jb u_cero
        cmp eax, 'Z'
        ja u_cero
.x:
        ret
; SHORT VkKeyScanA(carácter) -> la tecla virtual (y 1 en el byte alto: Mayús.)
u_VkKeyScanA:
        movzx eax, byte [ebp]
        cmp eax, 'a'
        jb .1
        cmp eax, 'z'
        ja .1
        sub eax, 32
        ret
.1:
        cmp eax, 'A'
        jb .2
        cmp eax, 'Z'
        ja .2
        or eax, 0x100
        ret
.2:
        cmp eax, ' '
        je .x
        cmp eax, '0'
        jb .no
        cmp eax, '9'
        jbe .x
.no:
        or eax, -1
.x:
        ret

; =============================================================================
;  Los aceleradores: una tabla (objeto GT_ACEL) con las entradas como en los
;  recursos: banderas, tecla, orden y relleno (2 octetos cada uno)
; =============================================================================
; HACCEL LoadAcceleratorsA(módulo, nombre)
u_LoadAcceleratorsA:
        mov eax, 9                  ; RT_ACCELERATOR
        mov ebx, [ebp+4]
        call w32_recurso
        jc u_cero
        shr ecx, 3
        jz u_cero
w32x_acel_nueva:                    ; ESI=entradas ECX=cuántas -> EAX
        push ecx
        push esi
        lea ecx, [ecx*8+4]
        call w32_pedir
        pop esi
        pop ecx
        jc u_cero
        mov edi, eax
        mov [edi], ecx
        push edi
        add edi, 4
        shl ecx, 1
        rep movsd
        pop edi
        push edi
        call g_nuevo
        pop edi
        jc .no
        mov byte [esi+GO_TIPO], GT_ACEL
        mov [esi+GO_A], edi
        ret
.no:
        mov eax, edi
        call mem_soltar
        jmp u_cero
; HACCEL CreateAcceleratorTableA(ACCEL*, n): ACCEL = BYTE, WORD, WORD
u_CreateAcceleratorTableA:
        mov ecx, [ebp+4]
        test ecx, ecx
        jle u_cero
        cmp ecx, 256
        jbe .n
        mov ecx, 256
.n:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        mov edi, w32x_buf
        push ecx
.c:
        movzx eax, byte [esi]
        stosw
        mov ax, [esi+2]
        stosw
        mov ax, [esi+4]
        stosw
        xor eax, eax
        stosw
        add esi, 6
        loop .c
        pop ecx
        mov esi, w32x_buf
        jmp w32x_acel_nueva
; BOOL DestroyAcceleratorTable(HACCEL)
u_DestroyAcceleratorTable:
        mov eax, [ebp]
        call w32_obj
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_ACEL
        jne u_cero
        mov eax, [esi+GO_A]
        call mem_soltar
        mov byte [esi+GO_TIPO], 0
        jmp u_uno
; int CopyAcceleratorTableA(HACCEL, ACCEL*, n)
u_CopyAcceleratorTableA:
        mov eax, [ebp]
        call w32_obj
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_ACEL
        jne u_cero
        mov esi, [esi+GO_A]
        mov eax, [esi]
        mov edi, [ebp+4]
        test edi, edi
        jz .x
        add edi, [edx+PE_DELTA]
        mov ecx, [ebp+8]
        cmp ecx, eax
        jbe .n
        mov ecx, eax
.n:
        mov eax, ecx
        add esi, 4
.c:
        jecxz .x
        mov bl, [esi]
        mov [edi], bl
        mov bx, [esi+2]
        mov [edi+2], bx
        mov bx, [esi+4]
        mov [edi+4], bx
        add esi, 8
        add edi, 6
        dec ecx
        jmp .c
.x:
        ret
; int TranslateAcceleratorA(ventana, HACCEL, MSG*): si la tecla es de la
; tabla, WM_COMMAND (con 1 en la palabra alta) a la ventana
u_TranslateAcceleratorA:
        mov eax, [ebp]
        mov [w32x_hwnd], eax
        mov eax, [ebp+4]
        call w32_obj
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_ACEL
        jne u_cero
        mov edi, [ebp+8]
        add edi, [edx+PE_DELTA]
        mov eax, [edi+4]            ; el mensaje
        mov ebx, [edi+8]            ; la tecla (o el carácter)
        xor ecx, ecx                ; ECX = 1 si es de tecla virtual
        cmp eax, WM_KEYDOWN
        je .v
        cmp eax, 0x0104             ; WM_SYSKEYDOWN
        je .v
        cmp eax, WM_CHAR
        jne u_cero
        jmp .m
.v:
        inc ecx
.m:
        mov eax, [edx+PE_TEC_MOD]   ; las de mayúsculas, como FSHIFT/FCONTROL/FALT
        xor edi, edi
        test al, TM_MAYUS
        jz .m1
        or edi, 4
.m1:
        test al, TM_CTRL
        jz .m2
        or edi, 8
.m2:
        test al, TM_ALT
        jz .m3
        or edi, 16
.m3:
        mov esi, [esi+GO_A]
        mov eax, [esi]
        add esi, 4
.b:
        test eax, eax
        jz u_cero
        movzx ebp, word [esi]       ; (EBP ya no hace falta)
        test ebp, 1                 ; FVIRTKEY
        jz .asc
        test ecx, ecx
        jz .sig
        cmp bx, [esi+2]
        jne .sig
        and ebp, 4 | 8 | 16
        cmp ebp, edi
        jne .sig
        jmp .si
.asc:
        test ecx, ecx
        jnz .sig
        cmp bx, [esi+2]
        jne .sig
.si:
        movzx ecx, word [esi+4]     ; la orden
        or ecx, 0x10000
        mov eax, [w32x_hwnd]
        call w32_ven
        jz u_cero
        mov ebp, eax
        mov eax, WM_COMMAND
        xor edx, edx
        call w32_proc
        jmp u_uno
.sig:
        add esi, 8
        dec eax
        jmp .b
; =============================================================================
;  Iconos y cursores (objetos GT_ICONO y GT_CURSOR, con sus dibujos IC_*)
; =============================================================================
; ---- ESI=icono de Hispa Ventanas (32×32 a 4 bits, 3 = transparente) EDX=
; programa -> EAX=un icono con él (0: no hay sitio) ----
w32x_icono_hispa:
        push esi
        mov ecx, IC_TAM
        call w32_pedir
        pop esi
        jc .no
        mov edi, eax
        push edi
        push esi
        add edi, IC_4B              ; el de la barra: tal cual
        mov ecx, 512 / 4
        rep movsd
        pop esi
        pop edi
        push edi
        mov ecx, 512                ; el de 32×32: un color por punto
.c:
        lodsb
        mov ah, al
        and al, 15
        cmp al, 3
        jne .c1
        mov al, CLAVE
.c1:
        stosb
        mov al, ah
        shr al, 4
        cmp al, 3
        jne .c2
        mov al, CLAVE
.c2:
        stosb
        loop .c
        pop edi
        call w32x_ic_cursor
        call w32x_ic_objeto
        ret
.no:
        xor eax, eax
        ret
; (interna) EDI=dibujos: el de 16×18 para el cursor, de cada 2×2 el primero
; que no sea transparente
w32x_ic_cursor:
        pushad
        lea ebx, [edi+IC_CUR]
        push edi
        mov edi, ebx
        mov ecx, CUR_ANCHO * CUR_ALTO
        mov al, CLAVE
        rep stosb
        pop edi
        xor edx, edx                ; fila
.f:
        xor ecx, ecx
.c:
        mov esi, edx
        shl esi, 6                  ; (2·fila) × 32
        lea esi, [esi+ecx*2]
        add esi, edi
        mov al, [esi]
        cmp al, CLAVE
        jne .p
        mov al, [esi+1]
        cmp al, CLAVE
        jne .p
        mov al, [esi+32]
        cmp al, CLAVE
        jne .p
        mov al, [esi+33]
.p:
        mov esi, edx
        imul esi, CUR_ANCHO
        add esi, ecx
        mov [ebx+esi], al
        inc ecx
        cmp ecx, 16
        jb .c
        inc edx
        cmp edx, 16
        jb .f
        popad
        ret
; (interna) EDI=dibujos EDX=programa -> EAX=el objeto (GT_ICONO; 0: no)
w32x_ic_objeto:
        push esi
        push edi
        call g_nuevo
        pop edi
        jc .no
        mov byte [esi+GO_TIPO], GT_ICONO
        mov [esi+GO_A], edi
        mov dword [esi+GO_D], CUR_PROG
        pop esi
        ret
.no:
        mov eax, edi
        call mem_soltar
        pop esi
        xor eax, eax
        ret
; ---- (interna) un icono o un cursor de los recursos: EAX=tipo del grupo (14
; o 12) EBX=nombre EDX=programa -> EAX=el objeto (0: no está) ----
w32x_ic_recurso:
        push eax
        call w32_recurso
        pop ecx
        jc .no
        movzx ebp, word [esi+4]     ; GRPICONDIR: cuántos; entradas de 14
        test ebp, ebp
        jz .no
        add esi, 6
        xor edi, edi                ; el mejor: el de 32 de ancho con más colores
        xor ebx, ebx                ; (su número)
        mov dword [w32x_ic_bits], -1
.b:
        movzx eax, byte [esi]       ; ancho (0: 256)
        cmp ecx, 12                 ; (de cursor: ancho y alto en palabras)
        jne .b1
        movzx eax, word [esi]
.b1:
        cmp eax, 32                 ; los puntos: 32 de ancho, mil más; y sus bits
        sete al
        movzx eax, al
        imul eax, 1000
        movzx edx, word [esi+6]
        add eax, edx
        cmp eax, [w32x_ic_bits]
        jle .s
        cmp dword [w32x_ic_bits], -1
        je .toma
.toma:
        mov edi, esi
        mov [w32x_ic_bits], eax
        movzx ebx, word [esi+12]
.s:
        add esi, 14
        dec ebp
        jnz .b
        mov eax, 3                  ; RT_ICON o RT_CURSOR
        cmp ecx, 12
        jne .r
        mov eax, 1
.r:
        mov edx, [wg_prog]
        push ecx
        call w32_recurso
        pop ecx
        jc .no
        mov [w32x_hot], dword 0
        cmp ecx, 12                 ; un cursor: el punto caliente delante
        jne .d
        mov eax, [esi]
        mov [w32x_hot], eax
        add esi, 4
.d:
        call w32x_ic_dib            ; ESI=el DIB (con su máscara) -> EAX
        test eax, eax
        jz .no
        cmp ecx, 12
        jne .x
        push eax                    ; un cursor: su punto caliente, a la mitad
        call w32_obj
        mov byte [esi+GO_TIPO], GT_CURSOR
        movzx eax, word [w32x_hot]
        shr eax, 1
        mov [esi+GO_B], eax
        movzx eax, word [w32x_hot+2]
        shr eax, 1
        mov [esi+GO_C], eax
        pop eax
.x:
        ret
.no:
        xor eax, eax
        ret
; ---- (interna) ESI=el DIB de un icono (BITMAPINFOHEADER con el doble de
; alto, colores, puntos y la máscara de 1 bit) -> EAX=un icono (0: no) ----
w32x_ic_dib:
        pushad
        mov edx, [wg_prog]
        cmp dword [esi], 40
        jne .no
        mov ecx, IC_TAM
        call w32_pedir
        jc .no
        mov [w32x_ic_p], eax
        push esi                    ; la cabecera y los colores, aparte, con el
        mov edi, w32x_bmi           ; alto de verdad (la mitad)
        mov ecx, (40 + 1024) / 4
        rep movsd
        pop esi
        mov eax, [w32x_bmi+8]
        sar eax, 1
        mov [w32x_bmi+8], eax
        mov dword [w32x_bmi+16], 0
        lea ebx, [esi+40]           ; los puntos: tras la cabecera y los colores
        movzx ecx, word [esi+14]
        cmp ecx, 8
        ja .p
        mov eax, [esi+32]
        test eax, eax
        jnz .p1
        mov eax, 1
        shl eax, cl
.p1:
        lea ebx, [ebx+eax*4]
.p:
        mov edi, wg_dibd
        mov byte [wg_mono], 0
        mov dword [wg_dc], 0
        mov esi, w32x_bmi
        xor ecx, ecx
        call wg_dib_desc
        jc .no2
        mov eax, [wg_dibd+DB_PASO]  ; la máscara, detrás de los puntos
        imul eax, [wg_dibd+DB_AL]
        add eax, [wg_dibd+DB_BITS]
        mov [w32x_ic_masc], eax
        mov eax, [wg_dibd+DB_AN]
        add eax, 31
        shr eax, 5
        shl eax, 2
        mov [w32x_ic_mpaso], eax
        ; 32 × 32: cada punto, el más cercano del original
        xor ebp, ebp                ; fila
.f:
        mov ebx, ebp
        imul ebx, [wg_dibd+DB_AL]
        shr ebx, 5
        mov edi, wg_dibd
        call wg_dib_fila            ; ESI = la fila (de arriba abajo)
        mov eax, [wg_dibd+DB_AL]    ; su máscara (de abajo arriba)
        dec eax
        sub eax, ebx
        imul eax, [w32x_ic_mpaso]
        add eax, [w32x_ic_masc]
        mov edx, eax
        xor ecx, ecx
.c:
        mov eax, ecx
        imul eax, [wg_dibd+DB_AN]
        shr eax, 5
        push ecx
        mov ecx, eax
        and ecx, 7
        mov bl, 0x80
        shr bl, cl
        mov ecx, eax
        shr ecx, 3
        test [edx+ecx], bl
        pop ecx
        mov bl, CLAVE
        jnz .pt
        mov bl, [esi+eax]
        cmp bl, CLAVE               ; (el de la clave, al gris de al lado)
        jne .pt
        dec bl
.pt:
        mov eax, ebp
        shl eax, 5
        add eax, ecx
        add eax, [w32x_ic_p]
        mov [eax], bl
        inc ecx
        cmp ecx, 32
        jb .c
        inc ebp
        cmp ebp, 32
        jb .f
        mov edi, [w32x_ic_p]
        call w32x_ic_4b
        call w32x_ic_cursor
        mov edx, [wg_prog]
        call w32x_ic_objeto
        mov [esp+28], eax
        popad
        ret
.no2:
        mov eax, [w32x_ic_p]
        call mem_soltar
.no:
        popad
        xor eax, eax
        ret
; (interna) EDI=dibujos: el de la barra (a 4 bits, los 16 colores de Hispa
; Ventanas, 3 = transparente) desde el de 32×32
w32x_ic_4b:
        pushad
        lea ebx, [edi+IC_4B]
        xor ecx, ecx
.p:
        movzx eax, byte [edi+ecx]
        mov dl, 3
        cmp al, CLAVE
        je .pon
        call w32x_cerca16
        mov dl, al
.pon:
        mov eax, ecx
        shr eax, 1
        test ecx, 1
        jnz .alto
        mov [ebx+eax], dl
        jmp .s
.alto:
        shl dl, 4
        or [ebx+eax], dl
.s:
        inc ecx
        cmp ecx, 1024
        jb .p
        popad
        ret
; (interna) AL=color de la paleta -> AL=el más cercano de los 16 de Hispa
; Ventanas (sin el 3, que es el transparente)
w32x_cerca16:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        movzx eax, al
        mov eax, [pal_rgb+eax*4]
        or edi, -1
        xor esi, esi
        xor ecx, ecx
.c:
        cmp ecx, 3
        je .s
        mov ebx, [pal_rgb+ecx*4]
        push eax
        call wg_distancia
        mov edx, eax
        pop eax
        cmp edx, edi
        jae .s
        mov edi, edx
        mov esi, ecx
.s:
        inc ecx
        cmp ecx, 16
        jb .c
        mov eax, esi
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
; (interna) los iconos de serie (IDI_*: 32512 a 32517) -> el de Hispa Ventanas
w32x_ico_serie:
        dd ico_programa, ico_ventana, ico_ventana, ico_ventana, ico_ventana, ico_programa
; HICON LoadIconA(módulo, nombre)
u_LoadIconA:
        cmp dword [ebp], 0
        je .serie
        mov eax, 14                 ; RT_GROUP_ICON
        mov ebx, [ebp+4]
        jmp w32x_ic_recurso         ; (con módulo: solo los suyos)
.serie:
        mov eax, [ebp+4]
        sub eax, 32512
        cmp eax, 6
        jb .s
        xor eax, eax
.s:
        mov ecx, [edx+PE_SICO+eax*4]    ; (uno por programa, la primera vez)
        test ecx, ecx
        jz .nuevo
        mov eax, ecx
.x:
        ret
.nuevo:
        push eax
        mov esi, [w32x_ico_serie+eax*4]
        call w32x_icono_hispa
        pop ecx
        mov [edx+PE_SICO+ecx*4], eax
        ret
; HCURSOR LoadCursorA(módulo, nombre)
u_LoadCursorA:
        cmp dword [ebp], 0
        je .serie
        mov eax, 12                 ; RT_GROUP_CURSOR
        mov ebx, [ebp+4]
        jmp w32x_ic_recurso         ; (con módulo: solo los suyos)
.serie:
        mov eax, [ebp+4]
        mov esi, w32x_cur_serie
.b:
        cmp word [esi], 0
        je .flecha
        cmp ax, [esi]
        je .si
        add esi, 4
        jmp .b
.si:
        movzx eax, word [esi+2]
        add eax, W32_HCUR
.x:
        ret
.flecha:
        mov eax, W32_HCUR + CUR_FLECHA
        ret
; los cursores de serie (IDC_*) y su forma
w32x_cur_serie:
        dw 32512, CUR_FLECHA, 32513, CUR_TEXTO, 32514, CUR_ESPERA, 32515, CUR_CRUZ
        dw 32642, CUR_DIAG1, 32643, CUR_DIAG2, 32644, CUR_HORIZ, 32645, CUR_VERT
        dw 32646, CUR_CRUZ, 32640, CUR_CRUZ, 32650, CUR_ESPERA, 0
; HANDLE LoadImageA(módulo, nombre, tipo, ancho, alto, LR_*)
u_LoadImageA:
        mov eax, [ebp+8]
        test eax, eax
        jz u_LoadBitmapA
        cmp eax, 1
        je u_LoadIconA
        cmp eax, 2
        je u_LoadCursorA
        jmp u_cero
; BOOL DestroyIcon(HICON) y DestroyCursor(HCURSOR)
u_DestroyIcon:
        mov eax, [ebp]
        call w32_obj
        jc u_uno
        cmp byte [esi+GO_TIPO], GT_ICONO
        je .s
        cmp byte [esi+GO_TIPO], GT_CURSOR
        jne u_cero
.s:
        mov ecx, PE_SICO            ; (los de serie se quedan)
.c:
        cmp [edx+ecx], eax
        je u_uno
        add ecx, 4
        cmp ecx, PE_SICO + 24
        jb .c
        call wg_obj_soltar
        jmp u_uno
; HICON CopyIcon(HICON)
u_CopyIcon:
        mov eax, [ebp]
        ret
; BOOL DrawIcon(HDC, x, y, HICON) y DrawIconEx(HDC, x, y, HICON, ancho, alto,
; paso, pincel, DI_*)
u_DrawIcon:
        mov dword [w32x_ic_w], 32
        mov dword [w32x_ic_h], 32
        jmp w32x_dibicono
u_DrawIconEx:
        mov eax, [ebp+16]
        test eax, eax
        jnz .w
        mov eax, 32
.w:
        mov [w32x_ic_w], eax
        mov eax, [ebp+20]
        test eax, eax
        jnz .h
        mov eax, 32
.h:
        mov [w32x_ic_h], eax
w32x_dibicono:
        mov eax, [ebp+12]
        call w32_obj
        jc u_cero
        cmp byte [esi+GO_TIPO], GT_ICONO
        je .si
        cmp byte [esi+GO_TIPO], GT_CURSOR
        jne u_cero
.si:
        mov esi, [esi+GO_A]
        mov eax, [ebp]
        call wg_abrir
        jc u_cero
        mov dword [wg_rop], 13
        mov eax, [ebp+4]            ; dónde, en la superficie
        add eax, [wg_ox]
        mov [wg_t7], eax
        mov eax, [ebp+8]
        add eax, [wg_oy]
        mov [wg_t6], eax
        xor ebp, ebp                ; la fila (los argumentos ya no hacen falta)
.f:
        cmp ebp, [w32x_ic_h]
        jae .x
        xor ecx, ecx
.c:
        cmp ecx, [w32x_ic_w]
        jae .fs
        mov eax, ebp                ; el punto del de 32 × 32
        shl eax, 5
        xor edx, edx
        div dword [w32x_ic_h]
        shl eax, 5
        mov ebx, eax
        mov eax, ecx
        shl eax, 5
        xor edx, edx
        div dword [w32x_ic_w]
        add ebx, eax
        movzx eax, byte [esi+ebx]
        cmp al, CLAVE
        je .cs
        cmp byte [wg_mono], 0
        je .pon
        mov byte [wg_mono], 0
        call wg_rgb
        mov byte [wg_mono], 1
        call wg_idx
.pon:
        mov [wg_c], eax
        push ecx
        mov eax, [wg_t7]
        add eax, ecx
        mov ebx, [wg_t6]
        add ebx, ebp
        call wg_punto
        pop ecx
.cs:
        inc ecx
        jmp .c
.fs:
        inc ebp
        jmp .f
.x:
        call wg_cerrar
        jmp u_uno
; ---- (interna) EAX=HICON EBP=ventana EDX=programa: el icono de su botón de
; la barra de tareas ----
w32x_icono_barra:
        push esi
        call w32_obj
        jc .x
        cmp byte [esi+GO_TIPO], GT_ICONO
        jne .x
        mov esi, [esi+GO_A]
        add esi, IC_4B
        mov [ebp+V_ICONO], esi
.x:
        pop esi
        ret
; HCURSOR SetCursor(HCURSOR) -> el de antes
u_SetCursor:
        mov eax, [ebp]
        xchg eax, [edx+PE_CURSOR]
        push eax
        mov eax, [ebp]
        mov ebp, [edx+PE_RVEN]      ; la ventana del programa con el ratón
        call w32x_poner_cursor
        pop eax
        ret
; HCURSOR GetCursor(void)
u_GetCursor:
        mov eax, [edx+PE_CURSOR]
        ret
; ---- (interna) EAX=HCURSOR EBP=ventana EDX=programa: el cursor sobre su zona
w32x_poner_cursor:
        pushad
        test ebp, ebp
        jz .x
        cmp dword [ebp+V_USO], 1
        jne .x
        cmp eax, W32_HCUR
        jb .obj
        sub eax, W32_HCUR
        cmp eax, CUR_N
        jae .x
        jmp .pon
.obj:
        call w32_obj
        jc .x
        cmp byte [esi+GO_TIPO], GT_CURSOR
        je .c
        cmp byte [esi+GO_TIPO], GT_ICONO
        jne .x
.c:
        push esi                    ; su dibujo, a la forma CUR_PROG
        mov esi, [esi+GO_A]
        add esi, IC_CUR
        mov edi, cur_imgs + CUR_PROG * CUR_IMG
        mov ecx, CUR_IMG
        rep movsb
        pop esi
        mov eax, [esi+GO_B]
        mov [cur_tabla+CUR_PROG*CT_TAM+12], eax
        mov eax, [esi+GO_C]
        mov [cur_tabla+CUR_PROG*CT_TAM+16], eax
        mov dword [cur_forma_v], -1 ; (para que se vea el dibujo nuevo)
        mov eax, CUR_PROG
.pon:
        call cur_forma
.x:
        popad
        ret
; ---- (w32_traducir) WM_MOUSEMOVE en la ventana EBP del programa ESI: el
; cursor de su clase ----
w32x_raton:
        mov [esi+PE_RVEN], ebp
        cmp dword [ebp+V_PROC], w32_ven_proc
        jne .x
        pushad
        mov edx, esi
        mov eax, [ebp+V_D2]
        mov eax, [eax+WC_CURSOR]
        test eax, eax
        jz .y
        call w32x_poner_cursor
.y:
        popad
.x:
        ret
; (u_DefWindowProcA) WM_SETICON y WM_GETICON: ESI=ventana
w32x_seticon:
        mov eax, [ebp+12]
        mov ebp, esi
        push dword [ebp+V_D2]
        call w32x_icono_barra
        pop eax
        mov eax, [eax+WC_ICONO]
        ret
w32x_geticon:
        mov eax, [esi+V_D2]
        mov eax, [eax+WC_ICONO]
        ret
; DWORD GetClassLongA(ventana, cuál) y SetClassLongA(ventana, cuál, valor)
u_GetClassLongA:
        xor ebx, ebx
        jmp w32x_clase_long
u_SetClassLongA:
        mov ebx, 1
w32x_clase_long:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        cmp dword [eax+V_PROC], w32_ven_proc
        jne u_cero
        mov esi, eax
        mov edi, [eax+V_D2]
        mov eax, [ebp+4]
        mov ecx, WC_ICONO
        cmp eax, -14                ; GCL_HICON
        je .c
        cmp eax, -34                ; GCL_HICONSM
        je .c
        mov ecx, WC_CURSOR
        cmp eax, -12                ; GCL_HCURSOR
        je .c
        mov ecx, WC_FONDO
        cmp eax, -10                ; GCL_HBRBACKGROUND
        je .c
        mov ecx, WC_ESTILO
        cmp eax, -26                ; GCL_STYLE
        je .c
        mov ecx, WC_PROC
        cmp eax, -24                ; GCL_WNDPROC
        jne u_cero
.c:
        mov eax, [edi+ecx]
        test ebx, ebx
        jz .x
        push eax
        mov eax, [ebp+8]
        mov [edi+ecx], eax
        cmp ecx, WC_ICONO
        jne .y
        mov ebp, esi
        call w32x_icono_barra
.y:
        pop eax
.x:
        ret

; =============================================================================
;  El portapapeles (el de Hispa Ventanas: texto)
; =============================================================================
u_OpenClipboard:
        jmp u_uno
u_EmptyClipboard:
        mov dword [pp_len], 0
        jmp u_uno
; HANDLE SetClipboardData(formato, HGLOBAL): CF_TEXT y CF_OEMTEXT
u_SetClipboardData:
        mov eax, [ebp]
        cmp eax, 1
        je .t
        cmp eax, 7
        jne .x
.t:
        mov esi, [ebp+4]
        test esi, esi
        jz .x
        add esi, [edx+PE_DELTA]
        call cad_largo
        mov ecx, eax
        cmp ecx, PP_MAX
        jbe .n
        mov ecx, PP_MAX
.n:
        push ecx
        push esi
        call mem_pedir
        pop esi
        pop ecx
        jc .x
        mov ebx, eax
        mov edi, eax
        jecxz .p
.c:
        lodsb                       ; (sin los retornos de carro; a la 437)
        cmp al, 13
        je .s
        test al, al
        jns .c1
        cmp dword [ebp], 7
        je .c1
        movzx eax, al
        mov al, [w32_1252+eax-0x80]
.c1:
        stosb
.s:
        loop .c
.p:
        mov ecx, edi
        sub ecx, ebx
        mov esi, ebx
        call pp_poner
        mov eax, ebx
        call mem_soltar
.x:
        mov eax, [ebp+4]
        ret
; HANDLE GetClipboardData(formato): un bloque como los de GlobalAlloc
u_GetClipboardData:
        mov eax, [ebp]
        cmp eax, 1
        je .t
        cmp eax, 7
        jne u_cero
.t:
        mov ecx, [pp_len]
        mov esi, [pp_buf]
        test esi, esi
        jz u_cero
        mov eax, [edx+PE_PP]        ; (el de la vez anterior, fuera)
        call mem_soltar
        lea ecx, [ecx*2+16+1]
        push esi
        call w32_pedir
        pop esi
        jc u_cero
        mov [edx+PE_PP], eax
        lea edi, [eax+16]
        mov ecx, [pp_len]
        jecxz .f
.c:
        lodsb                       ; LF -> CR LF; a la 1252
        cmp al, 10
        jne .c1
        mov byte [edi], 13
        inc edi
.c1:
        test al, al
        jns .c2
        cmp dword [ebp], 7
        je .c2
        movzx eax, al
        mov al, [w32_437+eax-0x80]
.c2:
        stosb
        loop .c
.f:
        mov byte [edi], 0
        mov eax, [edx+PE_PP]
        mov ecx, edi
        sub ecx, eax
        sub ecx, 15
        mov [eax+4], ecx            ; (lo que lee GlobalSize)
        add eax, 16
        sub eax, [edx+PE_DELTA]
        ret
; BOOL IsClipboardFormatAvailable(formato) e int CountClipboardFormats(void)
u_IsClipboardFormatAvailable:
        mov eax, [ebp]
        cmp eax, 1
        je u_CountClipboardFormats
        cmp eax, 7
        jne u_cero
u_CountClipboardFormats:
        xor eax, eax
        cmp dword [pp_len], 0
        setne al
        ret
; UINT EnumClipboardFormats(formato)
u_EnumClipboardFormats:
        cmp dword [ebp], 0
        jne u_cero
        jmp u_CountClipboardFormats

; =============================================================================
;  Menús emergentes
; =============================================================================
; BOOL TrackPopupMenu(HMENU, TPM_*, x, y, 0, ventana, RECT*): con el menú
; emergente de Hispa Ventanas; la orden llega con WM_COMMAND (con
; TPM_RETURNCMD, se espera y se devuelve)
u_TrackPopupMenu:
        mov eax, [ebp+4]
        mov [w32x_tpm_fl], eax
        mov eax, [ebp+8]
        mov [w32x_tpm_x], eax
        mov eax, [ebp+12]
        mov [w32x_tpm_y], eax
        mov eax, [ebp]
        call w32_menu
        jc u_cero
        mov eax, [ebp+20]
        call w32_ven
        jz u_cero
        mov [w32x_tpm_ven], eax
        mov edi, w32x_buf           ; las órdenes, como las de ctx_menu
        mov ecx, [esi+MN_N]
        lea esi, [esi+MN_ITEMS]
        xor ebx, ebx                ; las grises
        xor ebp, ebp
.c:
        jecxz .f
        test dword [esi+MI_FLAGS], MF_RAYA
        jz .o
        xor eax, eax
        stosw
        stosb
        jmp .s
.o:
        mov eax, [esi+MI_ID]
        stosw
        test dword [esi+MI_FLAGS], 3    ; MF_GRAYED, MF_DISABLED
        jz .o1
        bts ebx, ebp
.o1:
        push esi
        lea esi, [esi+MI_TEXTO]
        test dword [esi-MI_TEXTO+MI_FLAGS], 8   ; MF_CHECKED
        jz .o2
        mov al, '*'
        stosb
        mov al, ' '
        stosb
.o2:
        call pon_cad
        mov al, 0
        stosb
        pop esi
.s:
        add esi, MI_TAM
        inc ebp
        dec ecx
        jmp .c
.f:
        mov word [edi], 0xFFFF
        mov ecx, ebx
        mov eax, [w32x_tpm_x]       ; x, y: de la pantalla
        mov ebx, [w32x_tpm_y]
        mov esi, w32x_buf
        mov ebp, [w32x_tpm_ven]
        mov dword [w32x_tpm_res], 0
        test dword [w32x_tpm_fl], 0x100 ; TPM_RETURNCMD: se espera la orden
        setnz byte [w32x_tpm_esp]
        push edx
        call ctx_menu
        pop edx
        cmp byte [w32x_tpm_esp], 0
        je u_uno
.e:                                 ; (los mensajes de la tarea, también los del
        cmp dword [menu_ven], 0     ;  menú, que es suyo)
        je .ef
        push edx
        sub esp, M_TAM
        mov edi, esp
        call msg_tomar
        test eax, eax
        jnz .d
        call msg_salir              ; WM_QUIT: para el bucle de fuera
        add esp, M_TAM
        pop edx
        jmp .ef
.d:
        call msg_despachar
        add esp, M_TAM
        pop edx
        jmp .e
.ef:                                ; (la orden llega tras cerrarse: lo que
        push edx                    ;  quede en la cola, ya)
        sub esp, M_TAM
.ef1:
        mov edi, esp
        call msg_mirar
        jc .ef2
        call msg_despachar
        jmp .ef1
.ef2:
        add esp, M_TAM
        pop edx
        mov byte [w32x_tpm_esp], 0
        mov eax, [w32x_tpm_res]
        ret
; (w32_ven_proc) EAX=mensaje ECX EDX EBP=ventana: si es la orden que espera
; TrackPopupMenu (llega desde el menú, en otra tarea), se apunta; CF=1
w32x_tpm_mira:
        cmp eax, WM_COMMAND
        jne .no
        cmp byte [w32x_tpm_esp], 0
        je .no
        cmp ebp, [w32x_tpm_ven]
        jne .no
        test edx, edx
        jnz .no
        push ecx
        movzx ecx, cx
        mov [w32x_tpm_res], ecx
        pop ecx
        stc
        ret
.no:
        clc
        ret
; (w32_traducir, WM_COMMAND) ¿lo espera TrackPopupMenu? CF=1 si se lo queda
w32x_tpm_orden:
        cmp byte [w32x_tpm_esp], 0
        je .no
        cmp ebp, [w32x_tpm_ven]
        jne .no
        test edx, edx
        jnz .no
        movzx ecx, cx
        mov [w32x_tpm_res], ecx
        stc
        ret
.no:
        clc
        ret

; =============================================================================
;  Rectángulos y coordenadas
; =============================================================================
; (interna) [EBP] = un RECT* del programa -> EDI
w32x_rect:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        ret
u_SetRect:
        call w32x_rect
        lea esi, [ebp+4]
        movsd
        movsd
        movsd
        movsd
        jmp u_uno
u_SetRectEmpty:
        call w32x_rect
        xor eax, eax
        stosd
        stosd
        stosd
        stosd
        jmp u_uno
u_CopyRect:
        call w32x_rect
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        movsd
        movsd
        movsd
        movsd
        jmp u_uno
u_InflateRect:
        call w32x_rect
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        sub [edi], eax
        sub [edi+4], ebx
        add [edi+8], eax
        add [edi+12], ebx
        jmp u_uno
u_OffsetRect:
        call w32x_rect
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        add [edi], eax
        add [edi+4], ebx
        add [edi+8], eax
        add [edi+12], ebx
        jmp u_uno
u_IsRectEmpty:
        call w32x_rect
        mov eax, [edi]
        cmp eax, [edi+8]
        jge u_uno
        mov eax, [edi+4]
        cmp eax, [edi+12]
        jge u_uno
        jmp u_cero
u_EqualRect:
        call w32x_rect
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov ecx, 4
        repe cmpsd
        jne u_cero
        jmp u_uno
u_PtInRect:
        call w32x_rect
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        cmp eax, [edi]
        jl u_cero
        cmp eax, [edi+8]
        jge u_cero
        cmp ebx, [edi+4]
        jl u_cero
        cmp ebx, [edi+12]
        jge u_cero
        jmp u_uno
; BOOL IntersectRect(RECT* destino, RECT* a, RECT* b)
u_IntersectRect:
        call w32x_dos_rect
        mov eax, [esi]
        cmp eax, [ebx]
        jge .1
        mov eax, [ebx]
.1:
        mov [w32x_r], eax
        mov eax, [esi+4]
        cmp eax, [ebx+4]
        jge .2
        mov eax, [ebx+4]
.2:
        mov [w32x_r+4], eax
        mov eax, [esi+8]
        cmp eax, [ebx+8]
        jle .3
        mov eax, [ebx+8]
.3:
        mov [w32x_r+8], eax
        mov eax, [esi+12]
        cmp eax, [ebx+12]
        jle .4
        mov eax, [ebx+12]
.4:
        mov [w32x_r+12], eax
        mov eax, [w32x_r]
        cmp eax, [w32x_r+8]
        jge .vacio
        mov eax, [w32x_r+4]
        cmp eax, [w32x_r+12]
        jge .vacio
        mov esi, w32x_r
        movsd
        movsd
        movsd
        movsd
        jmp u_uno
.vacio:
        xor eax, eax
        stosd
        stosd
        stosd
        stosd
        jmp u_cero
; (interna) EDI=destino ESI=a EBX=b
w32x_dos_rect:
        call w32x_rect
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov ebx, [ebp+8]
        add ebx, [edx+PE_DELTA]
        ret
; BOOL UnionRect(RECT* destino, RECT* a, RECT* b)
u_UnionRect:
        call w32x_dos_rect
        mov eax, [esi]              ; (uno vacío: el otro)
        cmp eax, [esi+8]
        jge .solo_b
        mov eax, [esi+4]
        cmp eax, [esi+12]
        jge .solo_b
        mov eax, [ebx]
        cmp eax, [ebx+8]
        jge .solo_a
        mov eax, [ebx+4]
        cmp eax, [ebx+12]
        jge .solo_a
        mov eax, [esi]
        cmp eax, [ebx]
        jle .1
        mov eax, [ebx]
.1:
        mov [edi], eax
        mov eax, [esi+4]
        cmp eax, [ebx+4]
        jle .2
        mov eax, [ebx+4]
.2:
        mov [edi+4], eax
        mov eax, [esi+8]
        cmp eax, [ebx+8]
        jge .3
        mov eax, [ebx+8]
.3:
        mov [edi+8], eax
        mov eax, [esi+12]
        cmp eax, [ebx+12]
        jge .4
        mov eax, [ebx+12]
.4:
        mov [edi+12], eax
        jmp u_uno
.solo_b:
        mov esi, ebx
.solo_a:
        movsd
        movsd
        movsd
        movsd
        jmp u_uno
; BOOL SubtractRect(RECT* destino, RECT* a, RECT* b): a, si b no lo corta
; entero de un lado
u_SubtractRect:
        call w32x_dos_rect
        push edi
        movsd
        movsd
        movsd
        movsd
        pop edi
        jmp u_uno
; (interna) EAX=HWND -> EAX, EBX el origen de su zona en la pantalla; CF=1
; si no vale (el escritorio y 0: 0, 0)
w32x_origen:
        test eax, eax
        jz .esc
        cmp eax, W32_HESC
        je .esc
        call w32_ven
        jz .no
        push ebp
        push ecx
        push edx
        mov ebp, eax
        call ven_cli
        pop edx
        pop ecx
        pop ebp
        clc
        ret
.esc:
        xor eax, eax
        xor ebx, ebx
        clc
        ret
.no:
        stc
        ret
; BOOL ClientToScreen(ventana, POINT*) y ScreenToClient(ventana, POINT*)
u_ClientToScreen:
        mov eax, [ebp]
        call w32x_origen
        jc u_cero
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        add [edi], eax
        add [edi+4], ebx
        jmp u_uno
u_ScreenToClient:
        mov eax, [ebp]
        call w32x_origen
        jc u_cero
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        sub [edi], eax
        sub [edi+4], ebx
        jmp u_uno
; int MapWindowPoints(de, a, POINT*, n)
u_MapWindowPoints:
        mov eax, [ebp]
        call w32x_origen
        jc u_cero
        mov esi, eax
        mov edi, ebx
        mov eax, [ebp+4]
        call w32x_origen
        jc u_cero
        sub esi, eax                ; lo que se suma
        sub edi, ebx
        mov ebx, [ebp+8]
        add ebx, [edx+PE_DELTA]
        mov ecx, [ebp+12]
.l:
        test ecx, ecx
        jle .x
        add [ebx], esi
        add [ebx+4], edi
        add ebx, 8
        dec ecx
        jmp .l
.x:
        mov eax, edi
        shl eax, 16
        mov ax, si
        ret
; BOOL GetCursorPos(POINT*) y SetCursorPos(x, y)
u_GetCursorPos:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        mov eax, [rat_x]
        mov [edi], eax
        mov eax, [rat_y]
        mov [edi+4], eax
        jmp u_uno
; HWND WindowFromPoint(POINT): la del programa que esté ahí (o 0)
u_WindowFromPoint:
        mov eax, [ebp]
        mov ebx, [ebp+4]
        push edx
        call ges_donde              ; -> EBP ventana, EDI hija
        pop edx
        test edi, edi
        jz .v
        mov ebp, edi
.v:
        mov eax, ebp
        call w32_hwnd
        push eax
        call w32_ven
        pop eax
        jz u_cero
        ret
; HWND ChildWindowFromPoint(ventana, POINT)
u_ChildWindowFromPoint:
        mov eax, [ebp]
        ret
; BOOL AdjustWindowRect(RECT*, estilo, menú) y AdjustWindowRectEx(..., ext)
u_AdjustWindowRect:
u_AdjustWindowRectEx:
        call w32x_rect
        mov eax, [ebp+4]
        test eax, WS_CHILD
        jnz .x
        test eax, WS_CAPTION
        jz .x
        sub dword [edi], BORDE
        add dword [edi+8], BORDE
        sub dword [edi+4], BORDE + TIT_H + 1
        add dword [edi+12], BORDE
        cmp dword [ebp+8], 0
        je .x
        sub dword [edi+4], MENU_H
.x:
        jmp u_uno
; HWND SetCapture(ventana), BOOL ReleaseCapture(void), HWND GetCapture(void)
u_SetCapture:
        mov eax, [ebp]
        xchg eax, [edx+PE_CAPT]
        ret
u_ReleaseCapture:
        mov dword [edx+PE_CAPT], 0
        jmp u_uno
u_GetCapture:
        mov eax, [edx+PE_CAPT]
        ret
; HWND GetDesktopWindow(void)
u_GetDesktopWindow:
        mov eax, W32_HESC
        ret
; HWND GetForegroundWindow(void) y GetActiveWindow(void): la activa, si es suya
u_GetActiveWindow:
        mov eax, [activa]
        call w32_hwnd
        push eax
        call w32_ven
        pop eax
        jz u_cero
        ret
; HWND SetForegroundWindow(ventana), SetActiveWindow y BringWindowToTop
u_SetActiveWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        push eax
        mov ebp, eax
        call ven_activar
        pop eax
        jmp u_uno
; BOOL GetWindowPlacement(ventana, WINDOWPLACEMENT*)
u_GetWindowPlacement:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        mov dword [edi], 44
        mov dword [edi+4], 0
        mov ecx, 1
        test dword [eax+V_ESTADO], S_MINIM
        jz .a
        mov ecx, 2
.a:
        test dword [eax+V_ESTADO], S_MAXIM
        jz .b
        mov ecx, 3
.b:
        mov [edi+8], ecx
        xor ecx, ecx
        mov [edi+12], ecx
        mov [edi+16], ecx
        mov [edi+20], ecx
        mov [edi+24], ecx
        mov ecx, [eax+V_X]
        mov [edi+28], ecx
        add ecx, [eax+V_AN]
        mov [edi+36], ecx
        mov ecx, [eax+V_Y]
        mov [edi+32], ecx
        add ecx, [eax+V_AL]
        mov [edi+40], ecx
        jmp u_uno
; BOOL SetWindowPlacement(ventana, WINDOWPLACEMENT*)
u_SetWindowPlacement:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        push esi
        push edx
        mov ebp, eax
        mov eax, [esi+28]
        mov ebx, [esi+32]
        mov ecx, [esi+36]
        sub ecx, eax
        mov edx, [esi+40]
        sub edx, ebx
        call ven_mover
        pop edx
        pop esi
        mov eax, [esi+8]
        mov esi, edx
        call w32_mostrar
        jmp u_uno
; HWND FindWindowA(clase, título): solo entre las ventanas del programa
u_FindWindowA:
        mov esi, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [esi+V_USO], 1
        jne .s
        cmp dword [esi+V_PROC], w32_ven_proc
        jne .s
        cmp [esi+V_D0], edx
        jne .s
        cmp dword [esi+V_PADRE], 0
        jne .s
        mov edi, [ebp]              ; la clase
        test edi, edi
        jz .t
        cmp edi, 0x10000
        jb .s
        add edi, [edx+PE_DELTA]
        push esi
        mov esi, [esi+V_D2]
        add esi, WC_NOMBRE
        call w32_cmp_i
        pop esi
        jne .s
.t:
        mov edi, [ebp+4]            ; el título
        test edi, edi
        jz .si
        push esi
        push edi
        mov edi, w32x_buf
        mov esi, [esp]
        push ecx
        mov ecx, 255
        call w32_texto
        pop ecx
        pop edi
        pop esi
        push esi
        push edi
        mov esi, [esi+V_D4]
        mov edi, w32x_buf
        call w32_cmp_i
        pop edi
        pop esi
        jne .s
.si:
        mov eax, esi
        jmp w32_hwnd
.s:
        add esi, V_TAM
        dec ecx
        jnz .b
        jmp u_cero
; BOOL WinHelpA(...): no hay ayuda de Windows
u_WinHelpA:
        jmp u_cero
; DWORD GetMessagePos(void) y LONG GetMessageTime(void)
u_GetMessagePos:
        mov eax, [rat_y]
        shl eax, 16
        mov ax, [rat_x]
        ret
u_GetMessageTime:
        call tics
        imul eax, eax, 55
        ret
; BOOL CheckMenuRadioItem(HMENU, primera, última, la marcada, MF_*)
u_CheckMenuRadioItem:
        mov ecx, [ebp+4]
.l:
        cmp ecx, [ebp+8]
        jg u_uno
        push ecx
        sub esp, 12
        mov eax, [ebp]
        mov [esp], eax
        mov [esp+4], ecx
        xor eax, eax
        cmp ecx, [ebp+12]
        jne .n
        mov eax, 8                  ; MF_CHECKED
.n:
        or eax, [ebp+16]
        and eax, 0x408              ; (MF_BYPOSITION y MF_CHECKED)
        mov [esp+8], eax
        push ebp
        lea ebp, [esp+4]
        push edx
        call u_CheckMenuItem
        pop edx
        pop ebp
        add esp, 12
        pop ecx
        inc ecx
        jmp .l
; BOOL MapDialogRect(diálogo, RECT*): dos puntos por unidad
u_MapDialogRect:
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        shl dword [edi], 1
        shl dword [edi+4], 1
        shl dword [edi+8], 1
        shl dword [edi+12], 1
        jmp u_uno
; int MessageBoxIndirectA(MSGBOXPARAMSA*)
u_MessageBoxIndirectA:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        sub esp, 16
        mov eax, [esi+4]
        mov [esp], eax
        mov eax, [esi+12]
        mov [esp+4], eax
        mov eax, [esi+16]
        mov [esp+8], eax
        mov eax, [esi+20]
        mov [esp+12], eax
        push ebp
        lea ebp, [esp+4]
        call u_MessageBoxA
        pop ebp
        add esp, 16
        ret
; BOOL SystemParametersInfoA(qué, n, void*, guardar)
u_SystemParametersInfoA:
        mov edi, [ebp+8]
        test edi, edi
        jz u_cero
        add edi, [edx+PE_DELTA]
        mov eax, [ebp]
        cmp eax, 0x30               ; SPI_GETWORKAREA
        jne .nc
        xor eax, eax
        mov [edi], eax
        mov eax, [area_y0]
        mov [edi+4], eax
        mov eax, [vid_ancho]
        mov [edi+8], eax
        mov eax, [area_y1]
        mov [edi+12], eax
        jmp u_uno
.nc:
        cmp eax, 0x29               ; SPI_GETNONCLIENTMETRICS
        jne .lf
        mov dword [edi+4], 1        ; bordes, barras, título, menú y letras
        mov dword [edi+8], BD_AN
        mov dword [edi+12], BD_AN
        mov dword [edi+16], BOT_NC
        mov dword [edi+20], TIT_H
        mov eax, W32_HGDI + 13
        push edi
        add edi, 24
        call wg_letra_logfont
        pop edi
        mov dword [edi+84], BOT_NC
        mov dword [edi+88], TIT_H
        mov dword [edi+152], BOT_NC
        mov dword [edi+156], MENU_H
        mov esi, edi
        lea edi, [esi+92]
        call .lf3
        lea edi, [esi+160]
        call .lf3
        lea edi, [esi+220]
        call .lf3
        lea edi, [esi+280]
        call .lf3
        jmp u_uno
.lf3:
        push esi
        mov eax, W32_HGDI + 13
        call wg_letra_logfont
        pop esi
        ret
.lf:
        cmp eax, 0x1F               ; SPI_GETICONTITLELOGFONT
        jne .o
        mov eax, W32_HGDI + 13
        call wg_letra_logfont
        jmp u_uno
.o:
        mov dword [edi], 0          ; lo demás: 0
        jmp u_uno
; int GetClassNameA(ventana, búfer, sitio)
u_GetClassNameA:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, w32x_t_estatico
        cmp dword [eax+V_PROC], w32_ven_proc
        jne .c
        mov esi, [eax+V_D2]
        add esi, WC_NOMBRE
.c:
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        mov ecx, [ebp+8]
        dec ecx
        js u_cero
        xor eax, eax
.l:
        cmp eax, ecx
        jae .f
        mov bl, [esi+eax]
        test bl, bl
        jz .f
        mov [edi+eax], bl
        inc eax
        jmp .l
.f:
        mov byte [edi+eax], 0
        ret
w32x_t_estatico db "STATIC", 0
; BOOL ScrollWindow(...), int ScrollWindowEx(...), BOOL ScrollDC(...): se
; repinta toda la zona
u_ScrollWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov ebp, eax
        call ven_invalidar
        jmp u_uno
u_ScrollWindowEx:
        call u_ScrollWindow
        mov eax, 2                  ; SIMPLEREGION
        ret
u_ScrollDC:
        jmp u_uno
; BOOL RedrawWindow(ventana, RECT*, región, RDW_*)
u_RedrawWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        test dword [ebp+12], 1      ; RDW_INVALIDATE
        jz .u
        push eax
        push ebp
        mov ebp, eax
        call ven_invalidar
        pop ebp
        pop eax
.u:
        test dword [ebp+12], 0x100  ; RDW_UPDATENOW
        jz u_uno
        jmp u_UpdateWindow
; HWND GetWindow(ventana, GW_*): la primera hija (GW_CHILD) o la siguiente
; hermana (GW_HWNDNEXT)
u_GetWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov ecx, [ebp+4]
        mov esi, eax                ; ESI = desde dónde se busca
        mov ebx, eax                ; EBX = la madre de las que valen
        cmp ecx, 5
        je .hija
        cmp ecx, 2
        jne u_cero
        mov ebx, [eax+V_PADRE]
        add esi, V_TAM
        jmp .b
.hija:
        mov esi, [ven_tabla]
.b:
        mov eax, [ven_tabla]
        add eax, MAX_VEN * V_TAM
        cmp esi, eax
        jae u_cero
        cmp dword [esi+V_USO], 1
        jne .s
        cmp [esi+V_PADRE], ebx
        jne .s
        mov eax, esi
        call w32_hwnd
        push eax
        call w32_ven
        pop eax
        jnz .x
.s:
        add esi, V_TAM
        jmp .b
.x:
        ret
; BOOL IsChild(madre, hija)
u_IsChild:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov ebx, eax
        mov eax, [ebp+4]
        call w32_ven
        jz u_cero
.l:
        mov eax, [eax+V_PADRE]
        test eax, eax
        jz u_cero
        cmp eax, ebx
        je u_uno
        jmp .l
; BOOL EnumChildWindows(madre, función, lParam)
u_EnumChildWindows:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov ebx, eax
        mov esi, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [esi+V_USO], 1
        jne .s
        cmp [esi+V_PADRE], ebx
        jne .s
        pushad
        mov eax, esi
        call w32_hwnd
        mov ebx, eax                ; (EBX, ECX: los argumentos)
        mov ecx, [ebp+8]
        mov eax, [ebp+4]
        call pe_llamar
        test eax, eax
        popad
        jz u_uno
.s:
        add esi, V_TAM
        dec ecx
        jnz .b
        jmp u_uno
; LRESULT DefDlgProcA(diálogo, mensaje, wParam, lParam)
u_DefDlgProcA:
        jmp u_DefWindowProcA
; BOOL LockWindowUpdate(ventana), GetQueueStatus...: nada que hacer
u_nada1:
        jmp u_uno

; ---- (interna) de la página 1252: ¿es letra? AL -> ZF=0 si lo es ----
w32x_es_letra:
        cmp al, 'A'
        jb .no
        cmp al, 'Z'
        jbe .si
        cmp al, 'a'
        jb .no
        cmp al, 'z'
        jbe .si
        cmp al, 0xC0
        jb .no
        cmp al, 0xD7
        je .no
        cmp al, 0xF7
        je .no
.si:
        or al, al
        jnz .x
        inc al                      ; (nunca: AL no es 0)
.x:
        ret
.no:
        cmp al, al
        ret
; (interna) AL -> AL en mayúscula (EBX=1) o en minúscula (EBX=0), 1252
w32x_caso:
        call w32x_es_letra
        jz .x
        test ebx, ebx
        jz .min
        cmp al, 'a'
        jb .x
        cmp al, 0xDF                ; (ß no tiene)
        je .x
        cmp al, 0xE0
        jae .m1
        cmp al, 'z'
        ja .x
.m1:
        sub al, 32
        ret
.min:
        cmp al, 'Z'
        jbe .n1
        cmp al, 0xC0
        jb .x
        cmp al, 0xDF
        jae .x
.n1:
        add al, 32
.x:
        ret
; LPSTR CharUpperA(texto o carácter) y CharLowerA
u_CharUpperA:
        mov ebx, 1
        jmp w32x_char_caso
u_CharLowerA:
        xor ebx, ebx
w32x_char_caso:
        mov eax, [ebp]
        cmp eax, 0x10000
        jae .t
        call w32x_caso
        movzx eax, al
        ret
.t:
        mov esi, eax
        add esi, [edx+PE_DELTA]
.c:
        mov al, [esi]
        test al, al
        jz .x
        call w32x_caso
        mov [esi], al
        inc esi
        jmp .c
.x:
        mov eax, [ebp]
        ret
; DWORD CharUpperBuffA(texto, n) y CharLowerBuffA
u_CharUpperBuffA:
        mov ebx, 1
        jmp w32x_buff_caso
u_CharLowerBuffA:
        xor ebx, ebx
w32x_buff_caso:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        mov ecx, [ebp+4]
        mov edx, ecx
.c:
        test ecx, ecx
        jle .x
        mov al, [esi]
        call w32x_caso
        mov [esi], al
        inc esi
        dec ecx
        jmp .c
.x:
        mov eax, edx
        ret
; LPSTR CharNextA(p) y CharPrevA(inicio, p)
u_CharNextA:
        mov eax, [ebp]
        mov esi, eax
        add esi, [edx+PE_DELTA]
        cmp byte [esi], 0
        je .x
        inc eax
.x:
        ret
u_CharPrevA:
        mov eax, [ebp+4]
        cmp eax, [ebp]
        jbe .x
        dec eax
.x:
        ret
; BOOL IsCharAlphaA(c), IsCharAlphaNumericA(c), IsCharUpperA(c), IsCharLowerA(c)
u_IsCharAlphaA:
        mov al, [ebp]
        call w32x_es_letra
        jz u_cero
        jmp u_uno
u_IsCharAlphaNumericA:
        mov al, [ebp]
        cmp al, '0'
        jb u_IsCharAlphaA
        cmp al, '9'
        jbe u_uno
        jmp u_IsCharAlphaA
u_IsCharUpperA:
        mov al, [ebp]
        call w32x_es_letra
        jz u_cero
        mov ah, al
        mov ebx, 1
        call w32x_caso
        cmp al, ah
        je u_uno
        jmp u_cero
u_IsCharLowerA:
        mov al, [ebp]
        call w32x_es_letra
        jz u_cero
        mov ah, al
        xor ebx, ebx
        call w32x_caso
        cmp al, ah
        je u_uno
        jmp u_cero

; =============================================================================
;  COMDLG32: abrir, guardar, color y letra
; =============================================================================
OFN_OVERWRITEPROMPT equ 2
OFN_NOCHANGEDIR equ 8
OFN_FILEMUSTEXIST equ 0x1000
; BOOL GetOpenFileNameA(OPENFILENAMEA*) y GetSaveFileNameA(OPENFILENAMEA*):
; el diálogo de archivos de Hispa Ventanas
c_GetOpenFileNameA:
        xor eax, eax
        jmp c_archivo
c_GetSaveFileNameA:
        mov eax, 1
c_archivo:
        mov [w32x_guardar], eax
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        mov [w32x_ofn], esi
        ; la carpeta de ahora, para volver a ella (siempre)
        mov edi, w32x_dir0
        call dos_dir_actual
        ; la carpeta de empezar
        mov eax, [esi+44]
        test eax, eax
        jz .d
        push esi
        mov esi, eax
        mov edi, w32x_buf
        mov ecx, 120
        call w32_texto
        mov esi, w32x_buf
        call dos_ir_carpeta
        pop esi
.d:
        ; el filtro: el patrón del elegido (el primero, si son varios)
        mov dword [w32x_patron], t_todos
        mov eax, [esi+12]
        test eax, eax
        jz .p
        add eax, [edx+PE_DELTA]
        mov ecx, [esi+24]           ; nFilterIndex (desde 1)
        test ecx, ecx
        jnz .f1
        inc ecx
.f1:
        dec ecx                     ; salta 2·n textos (nombre y patrón)
        lea ecx, [ecx*2+1]
.f2:
        cmp byte [eax], 0
        je .p                       ; (no hay tantos: todos)
.f3:
        dec ecx
        jz .f4
.f5:
        inc eax
        cmp byte [eax-1], 0
        jne .f5
        jmp .f2
.f4:
        mov edi, w32x_pat
        mov ecx, 15
.f6:
        mov bl, [eax]
        test bl, bl
        jz .f7
        cmp bl, ';'
        je .f7
        mov [edi], bl
        inc eax
        inc edi
        dec ecx
        jnz .f6
.f7:
        mov byte [edi], 0
        mov dword [w32x_patron], w32x_pat
.p:
        ; el nombre de empezar: lo que traiga lpstrFile (sin la carpeta)
        mov edi, w32x_nombre
        mov byte [edi], 0
        mov eax, [esi+28]
        test eax, eax
        jz .t
        push esi
        mov esi, eax
        mov ecx, 79
        mov edi, w32x_buf
        call w32_texto
        mov esi, w32x_buf           ; (desde la última barra)
        mov edi, esi
.n1:
        lodsb
        test al, al
        jz .n2
        cmp al, '\'
        je .n3
        cmp al, ':'
        jne .n1
.n3:
        mov edi, esi
        jmp .n1
.n2:
        mov esi, edi
        mov edi, w32x_nombre
        mov ecx, 79
.n4:
        lodsb
        stosb
        test al, al
        jz .n5
        loop .n4
        mov byte [edi], 0
.n5:
        pop esi
.t:
        ; el título
        mov edi, w32x_titulo
        mov eax, [esi+48]
        test eax, eax
        jz .t1
        push esi
        mov esi, eax
        mov ecx, 60
        call w32_texto
        pop esi
        jmp .t2
.t1:
        push esi
        mov esi, w32x_t_abrir
        cmp dword [w32x_guardar], 0
        je .t3
        mov esi, w32x_t_guardar
.t3:
        call pon_cad
        mov byte [edi], 0
        pop esi
.t2:
        ; el diálogo
        mov eax, [esi+4]
        call w32_ven
        mov ebp, eax
        mov eax, [w32x_patron]
        xchg eax, [da_patron]
        push eax
        mov edi, w32x_titulo
        mov ebx, w32x_nombre
        push edx
        call dlg_archivo
        pop edx
        pop dword [da_patron]
        test eax, eax
        jz .no
        mov esi, [w32x_ofn]
        ; con la extensión de serie si no trae ninguna
        mov eax, [esi+60]
        test eax, eax
        jz .e
        mov edi, w32x_nombre
.e1:
        mov bl, [edi]
        test bl, bl
        jz .e2
        cmp bl, '.'
        je .e
        inc edi
        jmp .e1
.e2:
        mov byte [edi], '.'
        inc edi
        push esi
        mov esi, eax                ; (w32_texto ya le suma PE_DELTA)
        mov ecx, 3
        call w32_texto
        pop esi
.e:
        ; la ruta entera: el nombre tal cual si ya la lleva (C:..., \...);
        ; si no, la carpeta de ahora y el nombre
        mov edi, w32x_ruta
        cmp byte [w32x_nombre+1], ':'
        je .r1
        cmp byte [w32x_nombre], '\'
        je .r1
        call dos_dir_actual
        mov edi, w32x_ruta
.r0:
        cmp byte [edi], 0           ; EDI al final
        je .r2
        inc edi
        jmp .r0
.r2:
        cmp byte [edi-1], '\'
        je .r1
        mov byte [edi], '\'
        inc edi
.r1:
        push esi
        mov esi, w32x_nombre
        call pon_cad
        mov byte [edi], 0
        mov ebx, w32x_ruta          ; EBX = dónde empieza el nombre (tras la
        mov esi, ebx                ; última barra o los dos puntos)
.r3:
        lodsb
        test al, al
        jz .r4
        cmp al, '\'
        je .r5
        cmp al, ':'
        jne .r3
.r5:
        mov ebx, esi
        jmp .r3
.r4:
        pop esi
        ; ¿existe? (abrir: tiene que existir; guardar: ¿reemplazarlo?)
        push esi
        mov esi, w32x_ruta
        mov ecx, 0x37
        call dos_primero
        pop esi
        jc .noexiste
        cmp dword [w32x_guardar], 0
        je .dar
        test dword [esi+52], OFN_OVERWRITEPROMPT
        jz .dar
        push ebx
        push esi
        mov ebp, [esi+4]
        mov eax, ebp
        call w32_ven
        mov ebp, eax
        mov esi, w32x_t_reemp
        mov edi, w32x_titulo
        mov eax, DM_SI_NO
        push edx
        call dlg_mensaje
        pop edx
        pop esi
        pop ebx
        cmp eax, ID_SI
        jne .no
        jmp .dar
.noexiste:
        cmp dword [w32x_guardar], 0
        jne .dar
        test dword [esi+52], OFN_FILEMUSTEXIST
        jz .dar
        push esi
        mov eax, [esi+4]
        call w32_ven
        mov ebp, eax
        mov esi, w32x_t_noesta
        mov edi, w32x_titulo
        mov eax, DM_ACEPTAR
        push edx
        call dlg_mensaje
        pop edx
        pop esi
        jmp .no
.dar:
        ; a lpstrFile (en la 1252), con nFileOffset y nFileExtension
        mov edi, [esi+28]
        test edi, edi
        jz .ft
        add edi, [edx+PE_DELTA]
        mov ecx, [esi+32]           ; nMaxFile (con el cero)
        dec ecx
        jle .no
        push esi
        mov esi, w32x_ruta
.d1:
        lodsb
        test al, al
        jz .d3
        jns .d2
        movzx eax, al
        mov al, [w32_437+eax-0x80]
.d2:
        stosb
        dec ecx
        jnz .d1
.d3:
        mov byte [edi], 0
        pop esi
        mov eax, ebx
        sub eax, w32x_ruta
        mov [esi+56], ax            ; nFileOffset
        mov edi, ebx                ; nFileExtension: tras el último punto
        xor ecx, ecx
.x1:
        mov al, [edi]
        test al, al
        jz .x2
        inc edi
        cmp al, '.'
        jne .x1
        mov ecx, edi
        jmp .x1
.x2:
        test ecx, ecx
        jnz .x3
        mov ecx, edi
.x3:
        sub ecx, w32x_ruta
        mov [esi+58], cx
.ft:
        mov edi, [esi+36]           ; lpstrFileTitle
        test edi, edi
        jz .vuelta
        add edi, [edx+PE_DELTA]
        push esi
        mov esi, w32x_nombre
        call pon_cad
        mov byte [edi], 0
        pop esi
.vuelta:
        mov esi, w32x_dir0          ; la carpeta de trabajo, que en Hispa
        call dos_ir_carpeta         ; Ventanas es de todos los programas,
        jmp u_uno                   ; queda como estaba (lpstrFile lleva la ruta)
.no:
        mov esi, w32x_dir0          ; (cancelado: la carpeta de antes)
        call dos_ir_carpeta
        jmp u_cero
w32x_t_abrir    db "Abrir", 0
w32x_t_guardar  db "Guardar como", 0
w32x_t_reemp    db "Ese archivo ya existe. ", 168, "Quiere reemplazarlo?", 0
w32x_t_noesta   db "No se encuentra ese archivo.", 0
; short GetFileTitleA(ruta, búfer, sitio)
c_GetFileTitleA:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        mov edi, esi
.n1:
        lodsb
        test al, al
        jz .n2
        cmp al, '\'
        je .n3
        cmp al, ':'
        jne .n1
.n3:
        mov edi, esi
        jmp .n1
.n2:
        mov esi, edi
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        mov ecx, [ebp+8]
.c:
        dec ecx
        jle .f
        lodsb
        stosb
        test al, al
        jnz .c
        xor eax, eax
        ret
.f:
        mov byte [edi], 0
        xor eax, eax
        ret
; DWORD CommDlgExtendedError(void) y lo que no hay (PrintDlgA...)
c_CommDlgExtendedError:
        jmp u_cero
; BOOL ChooseColorA(CHOOSECOLORA*): una lista con los 16 colores de siempre
c_ChooseColorA:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        mov [w32x_ofn], esi
        mov dword [w32x_lista], w32x_colores
        mov dword [w32x_lista_n], 16
        mov eax, [esi+12]           ; el que había: el más cercano
        call w32x_color_cerca
        mov [w32x_sel], eax
        mov dword [w32x_casillas], 0
        mov edi, w32x_t_color
        mov eax, [esi+4]
        call w32x_lista_dlg
        jc u_cero
        mov esi, [w32x_ofn]
        mov eax, [w32x_sel]
        mov eax, [w32x_col_rgb+eax*4]
        mov [esi+12], eax
        jmp u_uno
; (interna) EAX=RGB -> EAX=el de w32x_col_rgb más cercano
w32x_color_cerca:
        push ebx
        push ecx
        push edx
        push esi
        or esi, -1
        xor edx, edx
        xor ecx, ecx
.c:
        mov ebx, [w32x_col_rgb+ecx*4]
        push eax
        call wg_distancia
        mov ebx, eax
        pop eax
        cmp ebx, esi
        jae .s
        mov esi, ebx
        mov edx, ecx
.s:
        inc ecx
        cmp ecx, 16
        jb .c
        mov eax, edx
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
w32x_col_rgb:
        dd 0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0x808080
        dd 0xC0C0C0, 0x0000FF, 0x00FF00, 0x00FFFF, 0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF
w32x_colores:
        db "Negro", 0, "Granate", 0, "Verde", 0, "Oliva", 0, "Azul marino", 0, "Morado", 0
        db "Verde azulado", 0, "Gris", 0, "Plata", 0, "Rojo", 0, "Lima", 0, "Amarillo", 0
        db "Azul", 0, "Fucsia", 0, "Aguamarina", 0, "Blanco", 0
w32x_t_color db "Color", 0
; BOOL ChooseFontA(CHOOSEFONTA*): una lista de tamaños, negrita y cursiva
c_ChooseFontA:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        mov [w32x_ofn], esi
        mov dword [w32x_lista], w32x_tams
        mov dword [w32x_lista_n], 10
        mov dword [w32x_sel], 2     ; (10 puntos)
        mov dword [w32x_casillas], 0
        mov edi, [esi+12]           ; CF_INITTOLOGFONTSTRUCT: lo que trae
        test dword [esi+20], 0x40
        jz .d
        test edi, edi
        jz .d
        add edi, [edx+PE_DELTA]
        mov eax, [edi]              ; lfHeight -> puntos
        test eax, eax
        jns .h
        neg eax
.h:
        imul eax, 72
        add eax, 48
        xor edx, edx
        mov ecx, 96
        div ecx
        mov edx, [wg_prog]
        xor ecx, ecx
.b:
        movzx ebx, byte [w32x_tam_n+ecx]
        cmp eax, ebx
        jbe .b1
        inc ecx
        cmp ecx, 9
        jb .b
.b1:
        mov [w32x_sel], ecx
        cmp dword [edi+16], 600
        jb .i
        or dword [w32x_casillas], 1
.i:
        cmp byte [edi+20], 0
        je .d
        or dword [w32x_casillas], 2
.d:
        mov dword [w32x_con_casillas], 1
        mov edi, w32x_t_letra
        mov eax, [esi+4]
        call w32x_lista_dlg
        mov dword [w32x_con_casillas], 0
        jc u_cero
        mov esi, [w32x_ofn]
        mov edi, [esi+12]
        test edi, edi
        jz u_uno
        add edi, [edx+PE_DELTA]
        mov ecx, [w32x_sel]
        movzx eax, byte [w32x_tam_n+ecx]
        push eax
        imul eax, 10
        mov [esi+16], eax           ; iPointSize
        pop eax
        imul eax, 96
        add eax, 36
        xor edx, edx
        mov ecx, 72
        div ecx
        neg eax
        mov [edi], eax              ; lfHeight
        mov dword [edi+4], 0
        mov eax, 400
        test byte [w32x_casillas], 1
        jz .n
        mov eax, 700
.n:
        mov [edi+16], eax
        mov al, [w32x_casillas]
        shr al, 1
        and al, 1
        mov [edi+20], al
        mov edx, [wg_prog]
        jmp u_uno
w32x_tams   db "8", 0, "9", 0, "10", 0, "11", 0, "12", 0, "14", 0, "16", 0, "18", 0
            db "24", 0, "36", 0
w32x_tam_n  db 8, 9, 10, 11, 12, 14, 16, 18, 24, 36
w32x_t_letra db "Letra (tama", 164, "o)", 0
; ---- (interna) un diálogo con una lista (w32x_lista, w32x_lista_n textos;
; el elegido en w32x_sel) y, con w32x_con_casillas, «Negrita» y «Cursiva»
; (w32x_casillas). EDI=título EAX=HWND de la dueña -> CF=1 si se cancela ----
w32x_lista_dlg:
        pushad
        push edx
        call w32_ven
        mov ebp, eax
        sub esp, P_TAM
        mov esi, w32x_dlg_v
        push edi
        mov edi, esp
        add edi, 4
        mov ecx, P_TAM / 4
        rep movsd
        pop edi
        mov [esp+P_TEXTO], edi
        mov esi, esp
        mov edi, w32x_dlg_h
        cmp dword [w32x_con_casillas], 0
        jne .h
        mov edi, w32x_dlg_h2
.h:
        mov ebx, w32x_dlg_gancho
        xor edx, edx
        call dlg_modal
        add esp, P_TAM
        pop edx
        cmp eax, ID_ACEPTAR
        jne .no
        popad
        clc
        ret
.no:
        popad
        stc
        ret
w32x_dlg_v:
        PLANTILLA c_dialogo, 0, E_DIALOGO, 0, 0, 260, 276, 0
w32x_dlg_h:
        PLANTILLA c_lista, 0, E_HIJA | E_TABULA, 16, 12, 140, 186, 103
        PLANTILLA c_boton, w32x_t_neg, E_HIJA | E_TABULA | BS_CASILLA, 166, 12, 84, 24, 104
        PLANTILLA c_boton, w32x_t_cur, E_HIJA | E_TABULA | BS_CASILLA, 166, 42, 84, 24, 105
        PLANTILLA c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 30, 206, 90, 26, ID_ACEPTAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, 136, 206, 90, 26, ID_CANCELAR
        dd 0
w32x_dlg_h2:
        PLANTILLA c_lista, 0, E_HIJA | E_TABULA, 16, 12, 228, 186, 103
        PLANTILLA c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 30, 206, 90, 26, ID_ACEPTAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, 136, 206, 90, 26, ID_CANCELAR
        dd 0
w32x_t_neg  db "Negrita", 0
w32x_t_cur  db "Cursiva", 0
w32x_dlg_gancho:
        cmp eax, DM_INICIO
        je .inicio
        cmp eax, WM_COMMAND
        jne .no
        cmp cx, 103
        jne .ac
        shr ecx, 16                 ; doble clic: aceptar
        cmp ecx, LN_DOBLE
        jne .no
        mov eax, WM_COMMAND
        mov ecx, ID_ACEPTAR
        xor edx, edx
        call msg_enviar
        jmp .si
.ac:
        cmp ecx, ID_ACEPTAR
        jne .no
        push ebp                    ; lo elegido
        mov ecx, 103
        call ven_hija_id
        mov ebp, eax
        mov eax, LM_SELEC
        call msg_enviar
        pop ebp
        test eax, eax
        js .c
        mov [w32x_sel], eax
.c:
        mov dword [w32x_casillas], 0
        mov ecx, 104
        call .casilla
        jz .c2
        or dword [w32x_casillas], 1
.c2:
        mov ecx, 105
        call .casilla
        jz .no
        or dword [w32x_casillas], 2
        jmp .no
.inicio:
        push ebp
        mov ecx, 103
        call ven_hija_id
        mov ebp, eax
        mov esi, [w32x_lista]
        mov ecx, [w32x_lista_n]
.l:
        push ecx
        mov eax, LM_ANADIR
        mov edx, esi
        call msg_enviar
        call cad_largo
        lea esi, [esi+eax+1]
        pop ecx
        loop .l
        mov eax, LM_PONER_SELEC
        mov ecx, [w32x_sel]
        call msg_enviar
        pop ebp
        cmp dword [w32x_con_casillas], 0   ; las casillas: solo para la letra
        je .no
        mov ecx, 104
        mov ebx, 1
        call .ver
        mov ecx, 105
        mov ebx, 2
        call .ver
.no:
        xor eax, eax
        ret
.si:
        mov eax, 1
        ret
.ver:                               ; ECX=la casilla EBX=su bit
        push ebp
        call ven_hija_id
        mov ebp, eax
        xor ecx, ecx
        test [w32x_casillas], ebx
        setnz cl
        mov eax, BM_PONER_MARCA
        call msg_enviar
        pop ebp
        ret
.casilla:
        push ebp
        call ven_hija_id
        mov ebp, eax
        mov eax, BM_MARCA
        call msg_enviar
        pop ebp
        test eax, eax
        ret

; =============================================================================
;  ADVAPI32 (el registro: no hay) y SHELL32 (lo justo)
; =============================================================================
a_no_esta:
        mov eax, 2                  ; ERROR_FILE_NOT_FOUND
        ret
a_no_puede:
        mov eax, 5                  ; ERROR_ACCESS_DENIED
        ret
a_no_hay_mas:
        mov eax, 259                ; ERROR_NO_MORE_ITEMS
        ret
a_GetUserNameA:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        mov esi, wg_t_letra         ; «Hispa»
        call pon_cad
        mov byte [edi], 0
        mov edi, [ebp+4]
        test edi, edi
        jz u_uno
        add edi, [edx+PE_DELTA]
        mov dword [edi], 6
        jmp u_uno
; HINSTANCE ShellExecuteA(...): no se puede (2: no encontrado)
s_ShellExecuteA:
        mov eax, 2
        ret
; int ShellAboutA(ventana, título, más, icono): un MessageBoxA
s_ShellAboutA:
        sub esp, 16
        mov eax, [ebp]
        mov [esp], eax
        mov eax, [ebp+8]
        test eax, eax
        jnz .t
        mov eax, [ebp+4]
.t:
        mov [esp+4], eax
        mov eax, [ebp+4]
        mov [esp+8], eax
        mov dword [esp+12], 0
        push ebp
        lea ebp, [esp+4]
        call u_MessageBoxA
        pop ebp
        add esp, 16
        jmp u_uno
