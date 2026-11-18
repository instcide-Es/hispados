; =============================================================================
;  Hispa Ventanas - [HISPA-486] USER32 y GDI32 para los programas de Windows
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Fase 3b de Hispa-486 (docs/486/HISPA-486.md): el «Hola, mundo» de ventana.
;  Va detrás de win32.inc (el cargador, el despacho y KERNEL32), en la parte
;  alta de VENTANAS.COM. Sala limpia: hecho a partir de la documentación
;  pública del API Win32.
;
;  Cómo funciona:
;  - Una ventana de Windows ES una ventana de Hispa Ventanas cuyo
;    procedimiento es w32_ven_proc. En su ficha: V_D0 = el programa (su ficha
;    PE_*), V_D1 = su WndProc (dirección del programa), V_D2 = su clase
;    (WC_*), V_D3 = su estilo de Windows, V_D4 = su bloque (el título en la
;    página 437, GWL_USERDATA y los octetos de más de la clase), V_D5 =
;    banderas (W32V_*) y V_D6 = su estilo extendido. El HWND es el número de
;    la ficha (W32_HWND + 4 × número).
;  - Cada programa tiene un bloque (PE_W32) con sus clases, sus objetos de
;    GDI (plumas, pinceles y letras; los primeros, los de serie), sus
;    contextos de dispositivo y los temporizadores con función.
;  - LA LLAMADA DE VUELTA (pe_llamar, en win32.inc): de Hispa Ventanas al
;    WndProc. Solo desde la tarea del programa (w32_en_tarea). Lo que le llega
;    a w32_ven_proc desde otra tarea (el sistema, al mover la ventana o al
;    cambiar la activa) se apunta en la cola de su tarea con la marca
;    W32_DIFERIDO y se entrega dentro de GetMessageA (o del bucle de un
;    cuadro): así el WndProc nunca corre en la pila de otra tarea.
;  - Los mensajes de la cola son los de Hispa Ventanas; GetMessageA los
;    traduce a los de Windows (w32_traducir, w32_tecla). Los que pone el
;    programa con PostMessageA llevan la marca W32_CRUDO: ya son de Windows.
;    Lo que se inventa (WM_KEYUP, el WM_CHAR de TranslateMessage, lo mirado
;    con PM_NOREMOVE) espera en una cola pequeña del programa (PE_PEND).
;  - El dibujo: Hispa Ventanas tiene un solo contexto (dc.inc) y solo vale
;    hasta que se cede el turno, así que un HDC es una ficha (DC_*) con la
;    ventana, el recorte de lo que hay que pintar, los colores, la pluma y el
;    pincel; cada función de GDI abre el contexto de Hispa Ventanas, dibuja y
;    lo cierra (w32_dc_abrir ... dc_cerrar).
;  - Los colores: COLORREF (00BBGGRR) al cubo de 6×6×6 o a los grises de la
;    paleta de Hispa Ventanas (w32_color). La letra: la de Hispa Ventanas, de
;    8×16; los textos pasan de la página 1252 a la 437.
; =============================================================================

W32_DIFERIDO equ 0x40000000         ; (en M_MSG) enviado desde otra tarea
W32_CRUDO   equ 0x20000000          ; (en M_MSG) ya es un mensaje de Windows
W32_HWND    equ 0x100               ; HWND = esto + 4 × número de ficha
W32_HGDI    equ 0x1000              ; un objeto de GDI = esto + su número
W32_HDC     equ 0x2000              ; un HDC = esto + su número
W32V_SINAVISO equ 1                 ; se destruye sin avisar al programa
W32V_DESTRUYE equ 2                 ; se está destruyendo
W32V_MOSTRADA equ 4                 ; ya se ha mostrado una vez

; el bloque de una ventana (V_D4)
VB_TITULO   equ 0                   ; 256: el título, en la página 437
VB_USER     equ 256                 ; GWL_USERDATA
VB_SBV      equ 260                 ; (fase 3d) sus barras de desplazamiento (c_barra):
VB_SBH      equ 264                 ;   la vertical y la horizontal
VB_SIV      equ 268                 ;   y su SCROLLINFO (mín., máx., página, pos., arrastre)
VB_SIH      equ 288
VB_EXTRA    equ 308                 ; los octetos de más de la clase
; una clase
WC_MAX      equ 20
WC_VCLASE   equ 0                   ; la clase de Hispa Ventanas (C_PROC, C_FONDO...)
WC_USO      equ 8
WC_PROC     equ 12                  ; su WndProc (del programa)
WC_ESTILO   equ 16
WC_FONDO    equ 20                  ; hbrBackground
WC_CURSOR   equ 24
WC_ICONO    equ 28
WC_EXTRA    equ 32                  ; cbWndExtra
WC_MENU     equ 36                  ; lpszMenuName (del programa; 0: sin menú)
WC_NOMBRE   equ 40                  ; 39 letras y el 0
WC_TAM      equ 80
WC_EXTRA_MAX equ 64
; un objeto de GDI
GO_MAX      equ 128
GO_SERIE    equ 20                  ; los primeros: los de GetStockObject
GO_TIPO     equ 0                   ; 0 libre, 1 pluma, 2 pincel, 3 letra, 4 otro
GO_ESTILO   equ 1                   ; pluma: PS_* (5: ninguna); pincel: 0 liso, 1 ninguno
GO_ANCHO    equ 2
GO_COLOR    equ 4                   ; COLORREF
GO_TAM      equ 32
; un contexto de dispositivo
DC_MAX      equ 16
DC_USO      equ 0                   ; 0 libre, 1 GetDC, 2 BeginPaint
DC_VEN      equ 4
DC_X0       equ 8                   ; BeginPaint: lo que hay que pintar (pantalla)
DC_Y0       equ 12
DC_X1       equ 16
DC_Y1       equ 20
DC_CTEXTO   equ 24                  ; COLORREF
DC_CFONDO   equ 28
DC_MODO     equ 32                  ; 1 transparente, 2 opaco
DC_PLUMA    equ 36
DC_PINCEL   equ 40
DC_LETRA    equ 44
DC_PX       equ 48                  ; donde está la pluma
DC_PY       equ 52
DC_ALIN     equ 56                  ; SetTextAlign
; (fase 3d: win32g.inc)
DC_ROP2     equ 60                  ; SetROP2 (R2_*)
DC_BMP      equ 64                  ; (de memoria) el mapa elegido
DC_PALETA   equ 68                  ; la paleta elegida
DC_RELLENO  equ 72                  ; SetPolyFillMode (1 ALTERNATE, 2 WINDING)
DC_DX       equ 76                  ; el origen de la vista (viewport),
DC_DY       equ 80
DC_WX       equ 84                  ;   el de la ventana (window)
DC_WY       equ 88
DC_RECORTE  equ 92                  ; su recorte (un bloque RG_*; 0: ninguno)
DC_ESTIRA   equ 96                  ; SetStretchBltMode
DC_EXTRA    equ 100                 ; SetTextCharacterExtra
DC_GUARDA   equ 104                 ; lo guardado con SaveDC (lista)
DC_ARCO     equ 108                 ; SetArcDirection (0 o 1: al revés de las agujas)
DC_BRX      equ 112                 ; SetBrushOrgEx
DC_BRY      equ 116
DC_MAPA     equ 120                 ; SetMapMode (0: MM_TEXT)
DC_TAM      equ 128
; un temporizador con función
TP_MAX      equ 8
TP_VEN      equ 0
TP_ID       equ 4
TP_PROC     equ 8
TP_TAM      equ 12
; el bloque del programa (PE_W32)
WB_CLASES   equ 0
WB_OBJ      equ WB_CLASES + WC_MAX * WC_TAM
WB_DC       equ WB_OBJ + GO_MAX * GO_TAM
WB_TMP      equ WB_DC + DC_MAX * DC_TAM
; (fase 3c) un menú: sus órdenes (win32m.inc)
MN_MAX      equ 8
MI_MAX      equ 16
MI_FLAGS    equ 0                   ; MF_*
MI_ID       equ 4                   ; el número de la orden (o el HMENU del desplegable)
MI_TEXTO    equ 8                   ; 31 letras y el 0 (página 437)
MI_TAM      equ 40
MN_USO      equ 0
MN_N        equ 4
MN_ITEMS    equ 8
MN_TAM      equ MN_ITEMS + MI_MAX * MI_TAM
WB_MENUS    equ WB_TMP + TP_MAX * TP_TAM
WB_TAM      equ WB_MENUS + MN_MAX * MN_TAM

%if V_TAM != 128
%error "w32_ven y w32_hwnd cuentan con fichas de 128 octetos"
%endif

; los objetos de serie (GetStockObject): tipo, estilo y color
w32_serie:
        db 2, 0
        dd 0xFFFFFF                 ;  0 WHITE_BRUSH
        db 2, 0
        dd 0xC0C0C0                 ;  1 LTGRAY_BRUSH
        db 2, 0
        dd 0x808080                 ;  2 GRAY_BRUSH
        db 2, 0
        dd 0x404040                 ;  3 DKGRAY_BRUSH
        db 2, 0
        dd 0x000000                 ;  4 BLACK_BRUSH
        db 2, 1
        dd 0                        ;  5 NULL_BRUSH
        db 1, 0
        dd 0xFFFFFF                 ;  6 WHITE_PEN
        db 1, 0
        dd 0x000000                 ;  7 BLACK_PEN
        db 1, 5
        dd 0                        ;  8 NULL_PEN
        db 0, 0
        dd 0                        ;  9
        db 3, 0
        dd 0                        ; 10 OEM_FIXED_FONT
        db 3, 0
        dd 0                        ; 11 ANSI_FIXED_FONT
        db 3, 0
        dd 0                        ; 12 ANSI_VAR_FONT
        db 3, 0
        dd 0                        ; 13 SYSTEM_FONT
        db 3, 0
        dd 0                        ; 14 DEVICE_DEFAULT_FONT
        db 4, 0
        dd 0                        ; 15 DEFAULT_PALETTE
        db 3, 0
        dd 0                        ; 16 SYSTEM_FIXED_FONT
        db 3, 0
        dd 0                        ; 17 DEFAULT_GUI_FONT
W32_NSERIE  equ 18
WG_MAPA0    equ 18                  ; (fase 3d) el mapa de 1×1 de los contextos de memoria
; los colores del sistema (GetSysColor), los de Windows 95
w32_syscol:
        dd 0xC0C0C0, 0x808000, 0x800000, 0x808080, 0xC0C0C0, 0xFFFFFF, 0x000000
        dd 0x000000, 0x000000, 0xFFFFFF, 0xC0C0C0, 0xC0C0C0, 0x808080, 0x800000
        dd 0xFFFFFF, 0xC0C0C0, 0x808080, 0x808080, 0x000000, 0xC0C0C0, 0xFFFFFF
        dd 0x000000, 0xC0C0C0, 0x000000, 0xE1FFFF   ; 3DDKSHADOW, 3DLIGHT, INFOTEXT, INFOBK
W32_NSYSCOL equ 25
; teclas de WM_KEYDOWN de Hispa Ventanas (TC_*: el código de la tecla) y su
; tecla virtual de Windows (VK_*)
w32_vk:
        db 0x01, 0x1B, 0x0E, 0x08, 0x0F, 0x09, 0x1C, 0x0D
        db 0x3B, 0x70, 0x3C, 0x71, 0x3D, 0x72, 0x3E, 0x73, 0x3F, 0x74
        db 0x40, 0x75, 0x41, 0x76, 0x42, 0x77, 0x43, 0x78, 0x44, 0x79
        db 0x85, 0x7A, 0x86, 0x7B
        db 0x47, 0x24, 0x48, 0x26, 0x49, 0x21, 0x4B, 0x25, 0x4D, 0x27
        db 0x4F, 0x23, 0x50, 0x28, 0x51, 0x22, 0x52, 0x2D, 0x53, 0x2E
        db 0x73, 0x25, 0x74, 0x27, 0x75, 0x23, 0x77, 0x24
        db 0, 0
; GetSystemMetrics: número y valor (los de la pantalla, aparte)
w32_metricas:
        db 2, BD_AN, 3, BD_AN, 4, TIT_H + 1, 5, 1, 6, 1, 7, BORDE, 8, BORDE
        db 11, 32, 12, 32, 13, 32, 14, 32, 15, MENU_H, 19, 1
        db 28, MIN_AN, 29, MIN_AL, 30, BOT_NC, 31, BOT_NC, 32, BORDE, 33, BORDE
        db 34, MIN_AN, 35, MIN_AL, 43, 2
        db 20, BD_AN, 21, BD_AN, 45, 2, 46, 2, 49, 16, 50, 16, 9, BD_AN, 10, BD_AN
        db 0xFF
; ShowWindow: lo que pide (0 a 10) -> 0 ocultar, 1 mostrar, 2 minimizar,
; 3 maximizar, 4 mostrar y restaurar
w32_sw      db 0, 4, 2, 3, 1, 1, 2, 2, 1, 4, 1

; =============================================================================
;  Lo de dentro
; =============================================================================
; ---- EAX=HWND EDX=programa -> EAX=la ventana (0 y ZF=1 si no es suya). Vale
; también para sus controles (los de serie: V_PROC = w32_ctl_proc y de su
; tarea), que no llevan en V_D0...V_D7 lo de una ventana de Windows (ahí está
; lo de su clase de Hispa Ventanas): quien toque eso, que mire antes cuál es ----
w32_ven:
        sub eax, W32_HWND
        jb .no
        test al, 3
        jnz .no
        shr eax, 2
        cmp eax, MAX_VEN
        jae .no
        shl eax, 7
        add eax, [ven_tabla]
        cmp dword [eax+V_USO], 1
        jne .no
        cmp dword [eax+V_PROC], w32_ven_proc
        jne .ctl
        cmp [eax+V_D0], edx
        jne .no
        test eax, eax
        ret
.ctl:
        cmp dword [eax+V_PROC], w32_ctl_proc    ; (fase 3c) un control suyo:
        jne .no                                 ; de su tarea
        push ebx
        mov ebx, [edx+PE_TAREA]
        cmp [eax+V_TAREA], ebx
        pop ebx
        jne .no
        test eax, eax
        ret
.no:
        xor eax, eax
        ret
; ---- EAX=ventana -> EAX=su HWND (0: 0) ----
w32_hwnd:
        test eax, eax
        jz .x
        sub eax, [ven_tabla]
        shr eax, 7
        lea eax, [eax*4+W32_HWND]
.x:
        ret
; ---- EBP=ventana ESI=programa -> ZF=1 si sigue siendo suya ----
w32_viva:
        cmp dword [ebp+V_USO], 1
        jne .x
        cmp [ebp+V_D0], esi
.x:
        ret
; ---- EDX=programa -> ZF=1 si se está en su tarea de verdad (la que corre
; es la suya y la pila es la de esa tarea: el Administrador de tareas, al
; terminar un programa, se hace pasar por él) ----
w32_en_tarea:
        push eax
        mov eax, [edx+PE_TAREA]
        cmp eax, [tar_act]
        jne .x
        mov eax, [eax+T_PILA]
        cmp esp, eax
        jb .no
        add eax, PILA_TAM
        cmp esp, eax
        jae .no
        cmp eax, eax
.x:
        pop eax
        ret
.no:
        test esp, esp
        pop eax
        ret
; ---- memoria del montón para el programa EDX (ECX octetos -> EAX; CF=1 si
; no hay): queda apuntada a su tarea, y si lo terminan a la fuerza la suelta
; el Administrador de tareas (tar_soltar_de) ----
w32_pedir:
        call mem_pedir
        jc .x
        push ebx
        mov ebx, [edx+PE_TAREA]
        test ebx, ebx
        jz .s
        mov [eax-4], ebx
.s:
        pop ebx
        clc
.x:
        ret
; ---- (pe_abrir) el bloque del programa EDX, con los objetos de serie.
; CF=1 si no hay memoria ----
w32_iniciar:
        pushad
        mov ecx, WB_TAM
        call mem_pedir
        jc .x
        mov [edx+PE_W32], eax
        lea edi, [eax+WB_OBJ]
        mov esi, w32_serie
        mov ecx, W32_NSERIE
.b:
        lodsw
        mov [edi+GO_TIPO], ax       ; (el tipo y el estilo)
        lodsd
        mov [edi+GO_COLOR], eax
        add edi, GO_TAM
        dec ecx
        jnz .b
        call w32_dlg_clase          ; (fase 3c) la clase de los diálogos
        call wg_iniciar             ; (fase 3d) los objetos de GDI de más
        clc
.x:
        popad
        ret
; ---- COLORREF (EAX = 00BBGGRR) -> AL=el color de la paleta de Hispa
; Ventanas: el gris más cercano (232 a 255) o el del cubo de 6×6×6 (16 a 231);
; con el octeto alto a 1 (la macro PALETTEINDEX de Windows), el propio índice ----
w32_color:
        test eax, 0x01000000        ; PALETTEINDEX(n): ese color de la paleta
        jz .rgb                     ; (los 16 primeros, los de Hispa Ventanas)
        movzx eax, al
        ret
.rgb:
        push ebx
        push ecx
        push edx
        movzx ebx, al
        movzx ecx, ah
        shr eax, 16
        movzx edx, al
        cmp ebx, ecx
        jne .cubo
        cmp ebx, edx
        jne .cubo
        mov eax, 231                ; blanco: el del cubo (255)
        cmp ebx, 250
        jae .x
        lea eax, [ebx+5]
        mov bl, 11
        div bl
        movzx eax, al
        add eax, 232
        jmp .x
.cubo:
        lea eax, [ebx+25]
        mov bl, 51
        div bl
        movzx ebx, al
        imul ebx, 36
        lea eax, [ecx+25]
        mov cl, 51
        div cl
        movzx ecx, al
        imul ecx, 6
        lea eax, [edx+25]
        mov dl, 51
        div dl
        movzx eax, al
        lea eax, [eax+ebx+16]
        add eax, ecx
.x:
        pop edx
        pop ecx
        pop ebx
        ret
; ---- ¿son iguales los textos ESI y EDI, sin mirar mayúsculas? ZF=1 si sí ----
w32_cmp_i:
        push eax
        push esi
        push edi
.c:
        mov al, [esi]
        mov ah, [edi]
        inc esi
        inc edi
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 32
.c1:
        cmp ah, 'a'
        jb .c2
        cmp ah, 'z'
        ja .c2
        sub ah, 32
.c2:
        cmp al, ah
        jne .x
        test al, al
        jnz .c
.x:
        pop edi
        pop esi
        pop eax
        ret
; ---- EAX=nombre de clase (del programa; menos de 10000h: su átomo)
; EDX=programa -> EDI=la clase; CF=1 si no está ----
w32_clase:
        push eax
        push ecx
        push esi
        mov edi, [edx+PE_W32]
        cmp eax, 0x10000
        jae .nombre
        sub eax, 0xC000
        jb .no
        cmp eax, WC_MAX
        jae .no
        imul eax, WC_TAM
        add edi, eax
        cmp dword [edi+WC_USO], 0
        je .no
        jmp .si
.nombre:
        mov esi, eax
        add esi, [edx+PE_DELTA]
        mov ecx, WC_MAX
.b:
        cmp dword [edi+WC_USO], 0
        je .s
        push edi
        add edi, WC_NOMBRE
        call w32_cmp_i
        pop edi
        je .si
.s:
        add edi, WC_TAM
        dec ecx
        jnz .b
.no:
        pop esi
        pop ecx
        pop eax
        stc
        ret
.si:
        pop esi
        pop ecx
        pop eax
        clc
        ret
; ---- la cola de lo inventado: EDX=programa, EBX=HWND ECX=mensaje ESI=wParam
; EDI=lParam; w32_pend_fin lo pone al final y w32_pend_ini, el primero ----
w32_pend_fin:
        push eax
        mov eax, [edx+PE_NPEND]
        cmp eax, PE_PEND_MAX
        jae .x
        inc dword [edx+PE_NPEND]
        shl eax, 4
        lea eax, [edx+eax+PE_PEND]
        mov [eax], ebx
        mov [eax+4], ecx
        mov [eax+8], esi
        mov [eax+12], edi
.x:
        pop eax
        ret
w32_pend_ini:
        push eax
        push ecx
        push esi
        push edi
        mov eax, [edx+PE_NPEND]
        cmp eax, PE_PEND_MAX
        jb .cabe
        dec eax                     ; (llena: se pierde el último)
        mov [edx+PE_NPEND], eax
.cabe:
        inc dword [edx+PE_NPEND]
        mov ecx, eax
        shl ecx, 2                  ; los que hay, un sitio más allá
        jz .pon
        shl eax, 4
        lea esi, [edx+eax+PE_PEND-4]
        lea edi, [esi+16]
        std
        rep movsd
        cld
.pon:
        pop edi
        pop esi
        pop ecx
        mov [edx+PE_PEND], ebx
        mov [edx+PE_PEND+4], ecx
        mov [edx+PE_PEND+8], esi
        mov [edx+PE_PEND+12], edi
        pop eax
        ret
; ---- EBP=ventana ECX=número -> EDX=la función de ese temporizador (0: no
; tiene). ESI=programa ----
w32_tmp_proc:
        push edi
        push eax
        xor edx, edx
        mov edi, [esi+PE_W32]
        add edi, WB_TMP
        mov eax, TP_MAX
.b:
        cmp [edi+TP_VEN], ebp
        jne .s
        cmp [edi+TP_ID], ecx
        jne .s
        mov edx, [edi+TP_PROC]
        jmp .x
.s:
        add edi, TP_TAM
        dec eax
        jnz .b
.x:
        pop eax
        pop edi
        ret
; ---- ¿está ya en la cola de su tarea el mensaje EAX de la ventana EBP?
; ZF=1 si está ----
w32_en_cola:
        push ebx
        push ecx
        push esi
        push edi
        mov ebx, [ebp+V_TAREA]
        mov esi, [ebx+T_CAB]
        mov ecx, [ebx+T_N]
.b:
        test ecx, ecx
        jz .no
        mov edi, esi
        shl edi, 4
        lea edi, [ebx+edi+T_COLA]
        cmp [edi+M_VEN], ebp
        jne .s
        cmp [edi+M_MSG], eax
        je .x
.s:
        inc esi
        cmp esi, COLA_MAX
        jb .s2
        xor esi, esi
.s2:
        dec ecx
        jmp .b
.no:
        test esp, esp
.x:
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret

; =============================================================================
;  De los mensajes de Hispa Ventanas a los de Windows
; =============================================================================
; ---- una tecla: EAX=WM_KEYDOWN o WM_CHAR de Hispa Ventanas, ECX=su wParam
; -> EBX=la tecla virtual (0: no se sabe) ECX=el carácter que escribe (página
; 1252; 0: ninguno) EDX=el lParam. Cambia EAX y ESI ----
w32_tecla:
        cmp eax, WM_CHAR
        je .car
        cmp ecx, TC_CTRL
        jb .tc
        sub ecx, TC_CTRL            ; Ctrl+letra: la letra y su carácter de control
        lea ebx, [ecx+0x40]
        mov edx, 1
        ret
.tc:
        cmp ecx, 0x54               ; (fase 3d) F1 a F10 con Mayús., Ctrl o Alt
        jb .tc1
        cmp ecx, 0x71
        ja .tc0
        sub ecx, 0x54
        push eax
        push edx
        mov eax, ecx
        xor edx, edx
        mov ecx, 10
        div ecx
        lea ecx, [edx+0x3B]
        pop edx
        pop eax
        jmp .tc1
.tc0:
        cmp ecx, 0x87               ; F11 y F12 con Mayús., Ctrl o Alt
        jb .tc1
        cmp ecx, 0x8C
        ja .tc1
        sub ecx, 0x87
        and ecx, 1
        add ecx, 0x85
.tc1:
        mov edx, ecx
        shl edx, 16
        inc edx
        mov esi, w32_vk
.b:
        lodsw
        test al, al
        jz .nada
        cmp al, cl
        jne .b
        movzx ebx, ah
        xor ecx, ecx
        cmp ebx, 0x08               ; retroceso, tabulador, Intro y Esc escriben
        je .esc
        cmp ebx, 0x09
        je .esc
        cmp ebx, 0x0D
        je .esc
        cmp ebx, 0x1B
        jne .x
.esc:
        mov ecx, ebx
.x:
        ret
.nada:
        xor ebx, ebx
        xor ecx, ecx
        ret
.car:
        movzx ecx, cl
        mov ebx, ecx
        mov edx, 1
        cmp ecx, 'a'
        jb .c1
        cmp ecx, 'z'
        ja .c1
        sub ebx, 32
        ret
.c1:
        cmp ecx, ' '
        je .x
        cmp ecx, '0'
        jb .c2
        cmp ecx, '9'
        jbe .x
        cmp ecx, 'A'
        jb .c2
        cmp ecx, 'Z'
        jbe .x
.c2:
        xor ebx, ebx
        cmp ecx, 0x80
        jb .x
        movzx ecx, byte [w32_437+ecx-0x80]
        ret
; ---- los demás: EBP=ventana ESI=programa, EAX ECX EDX = el mensaje de Hispa
; Ventanas -> el de Windows; CF=1 si no es cosa del programa ----
w32_traducir:
        cmp eax, WM_MOUSEMOVE
        jb .nor
        cmp eax, WM_RBUTTONUP
        ja .nor
        mov [esi+PE_TEC_MOD], ecx   ; (fase 3d) Mayús., Ctrl, Alt (GetKeyState)
        and dword [esi+PE_TEC_MOD], TM_MAYUS | TM_CTRL | TM_ALT
        cmp eax, WM_MOUSEMOVE
        jne .r0
        call w32x_raton             ; el cursor de su clase
.r0:
        cmp eax, WM_LBUTTONDBLCLK   ; el doble clic, solo con CS_DBLCLKS
        jne .r1
        push edi
        mov edi, [ebp+V_D2]
        test byte [edi+WC_ESTILO], 8
        pop edi
        jnz .r1
        mov eax, WM_LBUTTONDOWN
.r1:
        cmp eax, WM_LBUTTONDOWN
        je .li
        cmp eax, WM_LBUTTONDBLCLK
        jne .r2
.li:
        or byte [esi+PE_BOT], 1
.r2:
        cmp eax, WM_LBUTTONUP
        jne .r3
        and byte [esi+PE_BOT], ~1
.r3:
        cmp eax, WM_RBUTTONDOWN
        jne .r4
        or byte [esi+PE_BOT], 2
.r4:
        cmp eax, WM_RBUTTONUP
        jne .r5
        and byte [esi+PE_BOT], ~2
.r5:
        push ebx
        mov ebx, ecx                ; las mayúsculas -> MK_SHIFT y MK_CONTROL
        mov ecx, [esi+PE_BOT]
        test bl, TM_MAYUS
        jz .r6
        or ecx, 4
.r6:
        test bl, TM_CTRL
        jz .r7
        or ecx, 8
.r7:
        pop ebx
        clc
        ret
.nor:
        cmp eax, WM_MOUSEWHEEL      ; [RUEDA]
        je .rueda
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_MOVE
        je .mov
        cmp eax, WM_TIMER
        je .tmp
        cmp eax, WM_COMMAND
        je .ord
        cmp eax, WM_VSCROLL         ; (fase 3d) de una barra de desplazamiento
        je .bar
        cmp eax, WM_HSCROLL
        je .bar
        cmp eax, WM_PAINT
        je .sin
        cmp eax, WM_CLOSE
        je .sin
        cmp eax, WM_EXITSIZEMOVE
        je .sin
        cmp eax, WM_ENTERMENULOOP
        je .sin
        cmp eax, WM_EXITMENULOOP
        je .sin
        stc
        ret
.sin:
        xor ecx, ecx
        xor edx, edx
        clc
        ret
.bar:
        jmp w32x_scroll
.ord:
        call w32x_tpm_orden         ; (fase 3d) lo espera TrackPopupMenu
        jc .tpm
        test edx, edx               ; de un control: lParam = su HWND
        jz .o1
        push eax
        mov eax, edx
        call w32_hwnd
        mov edx, eax
        pop eax
.o1:
        clc
        ret
.tmp:
        call w32_tmp_proc
        clc
        ret
; [RUEDA] ECX=pasos (positivo: hacia abajo), EDX=x,y en la zona -> wParam =
; delta (120 por paso, positivo: hacia arriba) << 16 | MK_*, lParam = x,y
; en la pantalla
.rueda:
        push eax
        push ebx
        push ecx
        push edx
        call ven_cli
        pop edx
        add ax, dx
        shr edx, 16
        add dx, bx
        shl edx, 16
        mov dx, ax
        pop ecx
        imul ecx, ecx, -120
        shl ecx, 16
        or ecx, [esi+PE_BOT]
        pop ebx
        pop eax
        clc
        ret
.tpm:
        stc
        ret
.tam:
        call w32x_colocar           ; (fase 3d) sus barras, a su sitio
        push eax                    ; lo que mide ahora su zona
        push ebx
        call w32_cli
        shl edx, 16
        mov dx, cx
        pop ebx
        pop eax
        xor ecx, ecx                ; SIZE_RESTORED, SIZE_MINIMIZED o SIZE_MAXIMIZED
        test dword [ebp+V_ESTADO], S_MINIM
        jz .t1
        inc ecx
        clc
        ret
.t1:
        test dword [ebp+V_ESTADO], S_MAXIM
        jz .t2
        mov ecx, 2
.t2:
        clc
        ret
.mov:
        push eax
        push ebx
        call ven_cli
        mov edx, ebx
        shl edx, 16
        mov dx, ax
        pop ebx
        pop eax
        xor ecx, ecx
        clc
        ret

; ---- llama al WndProc de la ventana EBP con el mensaje EAX, ECX, EDX (ya
; de Windows) -> EAX. Conserva lo demás ----
w32_proc:
        cmp dword [ebp+V_PROC], w32_ctl_proc    ; a un control: se traduce
        je w32_ctl_enviar
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov esi, ecx
        mov edi, edx
        mov ecx, eax
        mov eax, ebp
        call w32_hwnd
        mov ebx, eax
        mov edx, [ebp+V_D0]
        mov eax, [ebp+V_D1]
        call pe_llamar
        test eax, eax               ; (fase 3c) un diálogo cuyo DLGPROC dice que
        jnz .x                      ; no lo atiende: lo que haría Windows
        cmp dword [ebp+V_USO], 1
        jne .x
        cmp dword [ebp+V_PROC], w32_ven_proc
        jne .x
        cmp [ebp+V_D0], edx
        jne .x
        test byte [ebp+V_D5], W32V_DIALOGO
        jz .x
        mov eax, ecx
        mov ecx, esi
        mov edx, edi
        call w32_dlg_defecto
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; =============================================================================
;  El procedimiento (de Hispa Ventanas) de las ventanas de Windows
; =============================================================================
w32_ven_proc:
        mov esi, [ebp+V_D0]
        test esi, esi
        jnz .hay
        cmp eax, WM_CREATE          ; la crea CreateWindowExA: lo suyo, a la ficha
        jne .cero
        mov esi, w32_crea
        lea edi, [ebp+V_D0]
        mov ecx, 8
        rep movsd
.cero:
        xor eax, eax
        ret
.hay:
        test eax, W32_CRUDO
        jz .trad
        and eax, ~(W32_CRUDO | W32_DIFERIDO)
        jmp w32_proc
.trad:
        mov ebx, eax
        and eax, ~W32_DIFERIDO
        call w32x_tpm_mira          ; (fase 3d) la orden que espera TrackPopupMenu
        jc .cero
        cmp eax, WM_VSCROLL         ; (fase 3d) de una barra: lo que se ha
        je .bar                     ; apretado, en el byte alto de la posición
        cmp eax, WM_HSCROLL
        jne .nobar
.bar:
        test ecx, 0xFF000000
        jnz .nobar
        test edx, edx
        jz .nobar
        cmp dword [edx+V_CLASE], c_barra
        jne .nobar
        push edx
        mov edx, [edx+V_D4]
        and edx, 7
        shl edx, 24
        or ecx, edx
        pop edx
.nobar:
        cmp eax, WM_DESTROY
        je .destruir
        cmp dword [esi+PE_USO], 1
        jne .cero
        xchg edx, esi
        call w32_en_tarea
        xchg edx, esi
        jne .otra
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .tecla
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_KILLFOCUS
        je .sinfoco
        call w32_traducir
        jc .cero
        cmp eax, WM_TIMER
        jne w32_proc
        test edx, edx
        jz w32_proc
        mov eax, edx                ; un temporizador con función
        mov esi, ecx
        mov ecx, WM_TIMER
        jmp w32_proc_fun
; desde otra tarea: a la cola de la suya, para GetMessageA
.otra:
        cmp eax, WM_GETMINMAXINFO
        je .cero
        or eax, W32_DIFERIDO
        cmp eax, WM_SIZE | W32_DIFERIDO     ; (de estos, con uno basta: al
        je .uno                             ; entregarlo se mira cómo está)
        cmp eax, WM_MOVE | W32_DIFERIDO
        jne .pon
.uno:
        call w32_en_cola
        je .cero
.pon:
        call msg_poner
        jmp .cero
.foco:
        cmp dword [ebp+V_PADRE], 0
        jne .f1
        mov eax, 0x0006             ; WM_ACTIVATE, WA_ACTIVE
        mov ecx, 1
        xor edx, edx
        call w32_proc
        call w32_viva
        jne .cero
.f1:
        mov eax, WM_SETFOCUS
        xor ecx, ecx
        xor edx, edx
        jmp w32_proc
.sinfoco:
        mov eax, WM_KILLFOCUS
        xor ecx, ecx
        xor edx, edx
        call w32_proc
        call w32_viva
        jne .cero
        cmp dword [ebp+V_PADRE], 0
        jne .cero
        mov eax, 0x0006             ; WM_ACTIVATE, WA_INACTIVE
        xor ecx, ecx
        jmp w32_proc
; una tecla: WM_KEYDOWN, WM_CHAR (si escribe) y WM_KEYUP
.tecla:
        call w32x_tecla_mod         ; (fase 3d) las de mayúsculas, para GetKeyState
        push esi
        call w32_tecla
        pop esi
        mov [esi+PE_TEC_ULT], ecx   ; (fase 3d) para ToAscii
        push edx                    ; [esp+8] lParam
        push ecx                    ; [esp+4] el carácter
        push ebx                    ; [esp] la tecla
        mov eax, WM_KEYDOWN
        mov ecx, ebx
        call w32_proc
        call w32_viva
        jne .tf
        mov ecx, [esp+4]
        test ecx, ecx
        jz .t2
        mov eax, WM_CHAR
        mov edx, [esp+8]
        call w32_proc
        call w32_viva
        jne .tf
.t2:
        mov eax, 0x0101             ; WM_KEYUP
        mov ecx, [esp]
        mov edx, [esp+8]
        or edx, 0xC0000000
        call w32_proc
.tf:
        add esp, 12
        xor eax, eax
        ret
; se destruye: WM_DESTROY y WM_NCDESTROY al programa (si se puede: pe_llamar
; no llama si el programa ha acabado o si no es su tarea) y fuera lo suyo
.destruir:
        test byte [ebp+V_D5], W32V_SINAVISO
        jnz .limpia
        or byte [ebp+V_D5], W32V_DESTRUYE
        mov eax, WM_DESTROY
        xor ecx, ecx
        xor edx, edx
        call w32_proc
        mov eax, 0x0082             ; WM_NCDESTROY
        call w32_proc
.limpia:
        mov edi, [esi+PE_W32]
        test edi, edi
        jz .l3
        lea ebx, [edi+WB_DC]        ; sus contextos
        mov ecx, DC_MAX
.l1:
        cmp [ebx+DC_VEN], ebp
        jne .l1s
        mov dword [ebx+DC_USO], 0
.l1s:
        add ebx, DC_TAM
        dec ecx
        jnz .l1
        lea ebx, [edi+WB_TMP]       ; las funciones de sus temporizadores
        mov ecx, TP_MAX
.l2:
        cmp [ebx+TP_VEN], ebp
        jne .l2s
        mov dword [ebx+TP_VEN], 0
.l2s:
        add ebx, TP_TAM
        dec ecx
        jnz .l2
.l3:
        cmp dword [ebp+V_D7], 0     ; (fase 3c) su barra de menús y su menú
        je .l4
        cmp ebp, [menu_duena]
        jne .l3b
        call menu_cerrar
.l3b:
        mov eax, [ebp+V_MENU]
        mov dword [ebp+V_MENU], 0
        call mem_soltar
        test edi, edi
        jz .l4
        mov edx, esi
        mov eax, [ebp+V_D7]
        call w32_menu_soltar
.l4:
        mov eax, [ebp+V_D4]         ; y su bloque
        call mem_soltar
        xor eax, eax
        mov [ebp+V_D4], eax
        mov [ebp+V_TEXTO], eax
        ret

; ---- llama a la función EAX del programa de la ventana EBP con (HWND, ECX,
; ESI, la hora): la función de un temporizador ----
w32_proc_fun:
        push eax
        call tics
        imul edi, eax, 55
        mov eax, ebp
        call w32_hwnd
        mov ebx, eax
        pop eax
        mov edx, [ebp+V_D0]
        jmp pe_llamar

; =============================================================================
;  USER32
; =============================================================================
u_cero:
        xor eax, eax
        ret
u_uno:
        mov eax, 1
        ret
; ATOM RegisterClassA(WNDCLASSA*) y RegisterClassExA(WNDCLASSEXA*)
u_RegisterClassExA:
        mov esi, [ebp]
        add esi, 4
        jmp u_RegisterClassA.r
u_RegisterClassA:
        mov esi, [ebp]
.r:
        add esi, [edx+PE_DELTA]
        mov eax, [esi+36]           ; el nombre (un átomo no vale)
        cmp eax, 0x10000
        jb u_cero
        call w32_clase
        jnc .ya
        mov edi, [edx+PE_W32]
        xor ebx, ebx
.b:
        cmp dword [edi+WC_USO], 0
        je .hay
        add edi, WC_TAM
        inc ebx
        cmp ebx, WC_MAX
        jb .b
        mov dword [edx+PE_ERROR], 8 ; ERROR_NOT_ENOUGH_MEMORY
        jmp u_cero
.ya:
        mov dword [edx+PE_ERROR], 1410  ; ERROR_CLASS_ALREADY_EXISTS
        jmp u_cero
.hay:
        mov dword [edi+WC_VCLASE+C_PROC], w32_ven_proc
        mov dword [edi+WC_VCLASE+C_FONDO], SIN_FONDO | (CB_FOCO << 8)
        mov dword [edi+WC_USO], 1
        mov eax, [esi]
        mov [edi+WC_ESTILO], eax
        mov eax, [esi+4]
        mov [edi+WC_PROC], eax
        mov eax, [esi+20]
        mov [edi+WC_ICONO], eax
        mov eax, [esi+24]
        mov [edi+WC_CURSOR], eax
        mov eax, [esi+28]
        mov [edi+WC_FONDO], eax
        mov eax, [esi+32]
        mov [edi+WC_MENU], eax
        mov eax, [esi+12]
        cmp eax, WC_EXTRA_MAX
        jbe .e
        mov eax, WC_EXTRA_MAX
.e:
        mov [edi+WC_EXTRA], eax
        mov esi, [esi+36]
        add esi, [edx+PE_DELTA]
        add edi, WC_NOMBRE
        mov ecx, WC_TAM - WC_NOMBRE - 1
.c:
        lodsb
        stosb
        test al, al
        jz .f
        dec ecx
        jnz .c
        mov byte [edi], 0
.f:
        lea eax, [ebx+0xC000]
        ret
; BOOL UnregisterClassA(nombre, módulo): si no quedan ventanas suyas
u_UnregisterClassA:
        mov eax, [ebp]
        call w32_clase
        jc u_cero
        mov esi, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [esi+V_USO], 1
        jne .s
        cmp [esi+V_CLASE], edi
        je u_cero
.s:
        add esi, V_TAM
        dec ecx
        jnz .b
        mov dword [edi+WC_USO], 0
        jmp u_uno

; HWND CreateWindowExA(estilo extendido, clase, título, estilo, x, y, ancho,
; alto, madre, menú o número, módulo, parámetro)
WS_CHILD    equ 0x40000000
WS_POPUP    equ 0x80000000
WS_VISIBLE  equ 0x10000000
WS_CAPTION  equ 0x00C00000
WS_THICKFRAME equ 0x00040000
WS_MINIMIZEBOX equ 0x00020000
WS_MAXIMIZEBOX equ 0x00010000
CW_USEDEFAULT equ 0x80000000
u_CreateWindowExA:
        mov eax, [ebp+4]
        call w32_clase
        jnc .clase
        call w32c_clase             ; (fase 3c) ¿una de serie: BUTTON, EDIT...?
        jnc w32c_crear
        mov dword [edx+PE_ERROR], 1407  ; ERROR_CANNOT_FIND_WND_CLASS
        jmp u_cero
.clase:
        mov ecx, [edi+WC_EXTRA]
        add ecx, VB_EXTRA
        call w32_pedir
        jc u_cero
        mov ebx, eax                ; EBX = su bloque
        push edi
        mov esi, [ebp+8]
        mov edi, ebx
        mov ecx, 255
        call w32_texto
        pop edi
        mov [w32_crea], edx         ; lo que w32_ven_proc lleva a V_D0...V_D7
        mov eax, [edi+WC_PROC]
        mov [w32_crea+4], eax
        mov [w32_crea+8], edi
        mov eax, [ebp+12]
        mov [w32_crea+12], eax
        mov [w32_crea+16], ebx
        xor ecx, ecx
        mov [w32_crea+20], ecx
        mov [w32_crea+28], ecx
        mov ecx, [ebp]
        mov [w32_crea+24], ecx
        sub esp, P_TAM              ; la plantilla de Hispa Ventanas
        mov [esp+P_CLASE], edi
        mov [esp+P_TEXTO], ebx
        mov ecx, [ebp+36]
        mov [esp+P_ID], ecx
        mov ecx, E_HIJA | E_OCULTA
        test eax, WS_CHILD
        jnz .est
        mov ecx, E_OCULTA
        test eax, WS_POPUP          ; una emergente sin título: sin marco
        jz .tit
        mov esi, eax
        and esi, WS_CAPTION
        cmp esi, WS_CAPTION
        jne .est
.tit:
        or ecx, E_TITULO | E_CERRAR
        test eax, WS_MAXIMIZEBOX
        jz .t1
        or ecx, E_MAXIM
.t1:
        test eax, WS_MINIMIZEBOX
        jz .t2
        or ecx, E_MINIM
.t2:
        test eax, WS_THICKFRAME
        jz .est
        or ecx, E_TAMANO
.est:
        mov [esp+P_ESTILO], ecx
        mov eax, [ebp+24]           ; el tamaño (CW_USEDEFAULT: 5/8 de la pantalla)
        mov esi, [ebp+28]
        cmp eax, CW_USEDEFAULT
        jne .w1
        mov eax, [vid_ancho]
        imul eax, 5
        shr eax, 3
        mov esi, [vid_alto]
        imul esi, 5
        shr esi, 3
.w1:
        test eax, eax
        jns .w2
        xor eax, eax
.w2:
        test esi, esi
        jns .w3
        xor esi, esi
.w3:
        mov [esp+P_AN], eax
        mov [esp+P_AL], esi
        mov eax, [ebp+16]           ; y el sitio (CW_USEDEFAULT: en escalera)
        mov esi, [ebp+20]
        cmp eax, CW_USEDEFAULT
        jne .p1
        mov eax, [w32_cascada]
        inc dword [w32_cascada]
        and eax, 7
        imul eax, 24
        lea esi, [eax+BARRA_H+24]
        add eax, 48
        jmp .p2
.p1:
        test ecx, E_TITULO          ; (el título, debajo de la barra del escritorio)
        jz .p2
        cmp esi, BARRA_H
        jge .p2
        mov esi, BARRA_H
.p2:
        mov [esp+P_X], eax
        mov [esp+P_Y], esi
        xor eax, eax                ; la madre, si es hija
        test ecx, E_HIJA
        jz .m
        mov eax, [ebp+32]
        call w32_ven
        jz .falla
.m:
        mov esi, esp
        push ebp
        mov ebp, eax
        call ven_crear
        pop ebp
        test eax, eax
        jz .falla
        mov esi, eax                ; ESI = la ventana
        test dword [esi+V_ESTILO], E_HIJA   ; (fase 3c) su menú: el que se da
        jnz .sinmenu                        ; o el de su clase (de los recursos)
        mov eax, [ebp+36]
        test eax, eax
        jnz .hm
        mov ebx, [esi+V_D2]
        mov ebx, [ebx+WC_MENU]
        test ebx, ebx
        jz .sinmenu
        push esi
        call w32_menu_recurso
        pop esi
.hm:
        push esi
        call w32_menu
        pop esi
        jc .sinmenu
        mov [esi+V_D7], eax
        push ebp
        mov ebp, esi
        call w32_menu_hacer
        pop ebp
.sinmenu:
        call w32x_nueva             ; (fase 3d) sus barras y su icono
        mov eax, [esp+P_X]
        mov ecx, [esp+P_Y]
        add esp, P_TAM
        ; CREATESTRUCTA, en esta pila (el programa la ve con su puntero)
        push dword [ebp]            ; dwExStyle
        push dword [ebp+4]          ; lpszClass
        push dword [ebp+8]          ; lpszName
        push dword [ebp+12]         ; style
        push eax                    ; x
        push ecx                    ; y
        push dword [esi+V_AN]       ; cx
        push dword [esi+V_AL]       ; cy
        push dword [ebp+32]         ; hwndParent
        push dword [ebp+36]         ; hMenu
        push dword [ebp+40]         ; hInstance
        push dword [ebp+44]         ; lpCreateParams
        mov edi, esp
        sub edi, [edx+PE_DELTA]     ; EDI = la estructura, para el programa
        mov ebx, edx                ; EBX = el programa
        push ebp
        mov ebp, esi
        mov esi, ebx
        mov eax, 0x0081             ; WM_NCCREATE: si dice que no, no se crea
        xor ecx, ecx
        mov edx, edi
        call w32_proc
        call w32_viva
        jne .nada
        test eax, eax
        jnz .cr
        or byte [ebp+V_D5], W32V_SINAVISO
        call ven_destruir
        jmp .nada
.cr:
        mov eax, WM_CREATE          ; WM_CREATE: -1 la destruye
        xor ecx, ecx
        mov edx, edi
        call w32_proc
        call w32_viva
        jne .nada
        cmp eax, -1
        jne .hecha
        call ven_destruir
.nada:
        pop ebp
        add esp, 48
        jmp u_cero
.hecha:
        test dword [ebp+V_D3], WS_VISIBLE
        jz .h2
        mov eax, 5
        call w32_mostrar
        call w32_viva
        jne .nada
.h2:
        mov eax, ebp
        pop ebp
        add esp, 48
        jmp w32_hwnd
.falla:
        add esp, P_TAM
        mov eax, ebx
        call mem_soltar
        jmp u_cero

; (interna) ShowWindow: EBP=ventana ESI=programa EAX=lo que se pide (SW_*)
; -> EAX=distinto de 0 si antes se veía
w32_mostrar:
        push ebx
        push ecx
        push edx
        push edi
        mov edi, [ebp+V_ESTADO]
        and edi, S_VISIBLE
        cmp eax, 10
        jbe .t
        mov eax, 1
.t:
        movzx ebx, byte [w32_sw+eax]
        test ebx, ebx
        jnz .ver
        test edi, edi               ; ocultar
        jz .x
        mov eax, 0x0018             ; WM_SHOWWINDOW
        xor ecx, ecx
        xor edx, edx
        call w32_proc
        call w32_viva
        jne .x
        xor eax, eax
        call ven_mostrar
        jmp .x
.ver:
        test edi, edi
        jnz .v1
        mov eax, 0x0018             ; WM_SHOWWINDOW
        mov ecx, 1
        xor edx, edx
        call w32_proc
        call w32_viva
        jne .x
.v1:
        cmp ebx, 2
        jne .v2
        test edi, edi               ; minimizada: sin pasar delante
        jnz .v1b
        or dword [ebp+V_ESTADO], S_VISIBLE
.v1b:
        mov eax, 2
        call ven_mostrar
        jmp .x
.v2:
        mov eax, 1
        call ven_mostrar
        call w32_viva
        jne .x
        cmp ebx, 3
        jne .v3
        mov eax, 3
        call ven_mostrar
        jmp .v4
.v3:
        cmp ebx, 4
        jne .v4
        test dword [ebp+V_ESTADO], S_MINIM | S_MAXIM
        jz .v4
        mov eax, 4
        call ven_mostrar
.v4:
        call w32_viva
        jne .x
        test byte [ebp+V_D5], W32V_MOSTRADA ; la primera vez: su tamaño y su sitio
        jnz .x
        or byte [ebp+V_D5], W32V_MOSTRADA
        mov eax, WM_SIZE
        call w32_traducir
        call w32_proc
        call w32_viva
        jne .x
        mov eax, WM_MOVE
        call w32_traducir
        call w32_proc
.x:
        mov eax, edi
        pop edi
        pop edx
        pop ecx
        pop ebx
        ret
; BOOL ShowWindow(ventana, cómo)
u_ShowWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov ecx, [ebp+4]
        mov esi, edx
        mov ebp, eax
        cmp dword [ebp+V_PROC], w32_ctl_proc
        je .ctl
        mov eax, ecx
        jmp w32_mostrar
.ctl:
        mov ebx, [ebp+V_ESTADO]     ; un control: se ve o no se ve
        and ebx, S_VISIBLE
        xor eax, eax
        test ecx, ecx
        setnz al
        call ven_mostrar
        mov eax, ebx
        ret
; BOOL UpdateWindow(ventana): si tiene algo que pintar, WM_PAINT ya
u_UpdateWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        cmp dword [eax+V_PROC], w32_ctl_proc    ; (un control se pinta solo)
        je u_uno
        mov ebp, eax
        mov esi, edx
        test dword [ebp+V_ESTADO], S_NCSUCIO
        jz .n
        call nc_pintar
.n:
        mov eax, [ebp+V_UX0]
        cmp eax, [ebp+V_UX1]
        jge u_uno
        mov eax, WM_PAINT
        xor ecx, ecx
        xor edx, edx
        call w32_proc
        call w32_viva
        jne u_uno
        xor eax, eax                ; si no la ha pintado, se da por pintada
        mov [ebp+V_UX0], eax
        mov [ebp+V_UX1], eax
        jmp u_uno
; BOOL DestroyWindow(ventana)
u_DestroyWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov ebp, eax
        cmp dword [ebp+V_PROC], w32_ctl_proc
        je .d
        test byte [ebp+V_D5], W32V_DESTRUYE
        jnz u_cero
.d:
        call ven_destruir
        jmp u_uno
; BOOL IsWindow(ventana), IsWindowVisible, IsIconic, IsZoomed
u_IsWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        jmp u_uno
u_IsWindowVisible:
        mov ecx, S_VISIBLE
        jmp u_estado
u_IsIconic:
        mov ecx, S_MINIM
        jmp u_estado
u_IsZoomed:
        mov ecx, S_MAXIM
u_estado:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        test [eax+V_ESTADO], ecx
        jz u_cero
        jmp u_uno
; HWND GetParent(ventana)
u_GetParent:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov eax, [eax+V_PADRE]
        jmp w32_hwnd
; HWND SetFocus(ventana) -> la que lo tenía; HWND GetFocus()
u_SetFocus:
        call u_GetFocus
        push eax
        mov eax, [ebp]
        call w32_ven
        jz .x
        mov ebp, eax
        call ven_enfocar
.x:
        pop eax
        ret
u_GetFocus:
        mov eax, [foco]
        test eax, eax
        jz .x
        call w32_hwnd               ; (suya, o un control suyo)
        push eax
        call w32_ven
        pop eax
        jz u_cero
.x:
        ret

; (interna) lo que hace Windows con un mensaje que el programa no atiende.
; LRESULT DefWindowProcA(ventana, mensaje, wParam, lParam)
u_DefWindowProcA:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        cmp dword [eax+V_PROC], w32_ctl_proc
        je u_cero
        mov esi, eax                ; ESI = la ventana
        mov eax, [ebp+4]
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, 0x0014             ; WM_ERASEBKGND
        je .borrar
        cmp eax, 0x0081             ; WM_NCCREATE
        je u_uno
        cmp eax, 0x0080             ; (fase 3d) WM_SETICON y WM_GETICON
        je w32x_seticon
        cmp eax, 0x007F
        je w32x_geticon
        cmp eax, WM_SETTEXT
        je .ptexto
        cmp eax, WM_GETTEXT
        je .texto
        cmp eax, WM_MOUSEWHEEL      ; [RUEDA] a la madre, si es de Windows
        je .rueda
        cmp eax, 0x000E             ; WM_GETTEXTLENGTH
        jne u_cero
        mov esi, [esi+V_D4]
        jmp cad_largo
.rueda:                             ; [RUEDA]
        mov ecx, [ebp+8]
        mov edx, [ebp+12]
        mov ebp, [esi+V_PADRE]
        test ebp, ebp
        jz u_cero
        cmp dword [ebp+V_PROC], w32_ven_proc
        jne u_cero
        jmp w32_proc
.cerrar:
        test byte [esi+V_D5], W32V_DESTRUYE
        jnz u_cero
        mov ebp, esi
        call ven_destruir
        jmp u_cero
.pintar:
        mov ebp, esi
        call w32_empezar_pintar
        jc u_cero
        mov dword [edi+DC_USO], 0
        jmp u_cero
.borrar:
        mov eax, [ebp+8]
        mov ebp, esi
        jmp w32_borrar
.ptexto:
        mov eax, [ebp+12]
        mov ebp, esi
        call w32_poner_titulo
        jmp u_uno
.texto:
        mov edi, [ebp+12]
        mov ecx, [ebp+8]
        mov ebp, esi
        jmp w32_leer_titulo

; (interna) EBP=ventana EAX=HDC EDX=programa: borra la zona con el pincel de
; su clase -> EAX=1 si la ha borrado
w32_borrar:
        call wg_abrir
        jc u_cero
        mov esi, [ebp+V_D2]
        mov eax, [esi+WC_FONDO]
        call wg_ppincel
        jc .no
        call w32_cli
        mov eax, [wg_dox]
        add ecx, eax
        mov ebx, [wg_doy]
        add edx, ebx
.f:
        cmp ebx, edx
        jge .s
        call wg_tramo
        inc ebx
        jmp .f
.s:
        call wg_cerrar
        jmp u_uno
.no:
        call wg_cerrar
        jmp u_cero
; (interna) BeginPaint: EBP=ventana EDX=programa -> EDI=el contexto (con lo
; que había que pintar, que queda dado por pintado) EAX=1 si el programa no
; ha borrado el fondo; CF=1 si no quedan contextos
w32_empezar_pintar:
        mov ecx, 2
        call w32_dc_nuevo
        jc .x
        push eax                    ; el HDC
        push edx
        mov eax, [ebp+V_UX0]
        mov ebx, [ebp+V_UY0]
        mov ecx, [ebp+V_UX1]
        mov edx, [ebp+V_UY1]
        cmp eax, ecx
        jl .hay
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
.hay:
        mov [edi+DC_X0], eax
        mov [edi+DC_Y0], ebx
        mov [edi+DC_X1], ecx
        mov [edi+DC_Y1], edx
        xor esi, esi
        mov [ebp+V_UX0], esi
        mov [ebp+V_UX1], esi
        mov esi, [ven_tabla]        ; las hijas que caen ahí también se repintan
        push edi
        mov edi, MAX_VEN
.h:
        cmp dword [esi+V_USO], 1
        jne .hs
        cmp [esi+V_PADRE], ebp
        jne .hs
        xchg esi, ebp
        call ven_inv_p
        xchg esi, ebp
.hs:
        add esi, V_TAM
        dec edi
        jnz .h
        pop edi
        mov ecx, [esp+4]            ; el HDC
        mov eax, 0x0014             ; WM_ERASEBKGND
        xor edx, edx
        call w32_proc
        pop edx
        pop ecx
        test eax, eax
        setz al
        movzx eax, al
        clc
.x:
        ret
; HDC BeginPaint(ventana, PAINTSTRUCT*)
u_BeginPaint:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]     ; ESI = PAINTSTRUCT
        mov ebp, eax
        call w32x_caret_antes       ; (fase 3d) el caret, fuera mientras se pinta
        push esi
        call w32_empezar_pintar     ; EDI=contexto EAX=fErase ECX=HDC
        pop esi
        jc u_cero
        push eax
        push ecx
        push edi
        mov edi, esi
        mov ecx, 16
        xor eax, eax
        rep stosd
        pop edi
        pop dword [esi]             ; hdc
        pop dword [esi+4]           ; fErase
        call ven_cli                ; EAX, EBX = el origen de su zona
        mov ecx, [edi+DC_X0]        ; rcPaint, en coordenadas de la zona
        sub ecx, eax
        jns .a
        xor ecx, ecx
.a:
        mov [esi+8], ecx
        mov ecx, [edi+DC_Y0]
        sub ecx, ebx
        jns .b
        xor ecx, ecx
.b:
        mov [esi+12], ecx
        mov ecx, [edi+DC_X1]
        sub ecx, eax
        jns .c
        xor ecx, ecx
.c:
        mov [esi+16], ecx
        mov ecx, [edi+DC_Y1]
        sub ecx, ebx
        jns .d
        xor ecx, ecx
.d:
        mov [esi+20], ecx
        mov eax, [esi]
        ret
; BOOL EndPaint(ventana, PAINTSTRUCT*)
u_EndPaint:
        mov eax, [ebp]
        call w32_ven
        jz .s
        push ebp
        mov ebp, eax
        call w32x_caret_despues     ; (fase 3d) el caret, otra vez
        pop ebp
.s:
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov eax, [esi]
        jmp u_ReleaseDC.s
; HDC GetDC(ventana) y int ReleaseDC(ventana, HDC)
u_GetDC:
        mov eax, [ebp]
        test eax, eax               ; (fase 3d) la pantalla: el escritorio
        jz .esc
        cmp eax, W32_HESC
        je .esc
        call w32_ven
        jz u_cero
        jmp .v
.esc:
        mov eax, [escritorio]
.v:
        mov ebp, eax
        mov ecx, 1
        call w32_dc_nuevo
        ret
u_ReleaseDC:
        mov eax, [ebp+4]
.s:
        call w32_dc
        jc u_cero
        mov dword [edi+DC_USO], 0
        jmp u_uno

; BOOL GetMessageA(MSG*, ventana, del, al): aquí cede el turno el programa
u_GetMessageA:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        xor al, al
        jmp w32_tomar
; BOOL PeekMessageA(MSG*, ventana, del, al, PM_REMOVE o no): sin esperar (si
; no hay nada, antes de volver deja pasar una vez a los demás)
u_PeekMessageA:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        mov al, 1
        call w32_tomar
        jc u_cero
        test byte [ebp+16], 1
        jnz u_uno
        cmp dword [edi+4], WM_QUIT  ; no se quita: vuelve a la cola
        jne .p
        mov dword [edx+PE_QUIT], 1
        jmp u_uno
.p:
        mov ebx, [edi]
        mov ecx, [edi+4]
        mov esi, [edi+8]
        mov edi, [edi+12]
        call w32_pend_ini
        jmp u_uno
; (interna) el siguiente mensaje del programa EDX, a EDI (MSG, ya como
; puntero de Hispa Ventanas). AL=0: espera (cede el turno); 1: no espera
; -> CF=1 si no hay ninguno; EAX=0 si es WM_QUIT. Conserva EBP, EDX y EDI
w32_tomar:
        push ebp
        push ebx
        push esi
        sub esp, M_TAM + 4          ; [esp] = cómo; [esp+4] = el de Hispa Ventanas
        movzx eax, al
        mov [esp], eax
.l:
        cmp dword [edx+PE_NPEND], 0 ; 1.º lo inventado
        je .cola
        dec dword [edx+PE_NPEND]
        lea esi, [edx+PE_PEND]
        movsd
        movsd
        movsd
        movsd
        sub edi, 16
        push edi
        lea edi, [edx+PE_PEND]
        mov ecx, (PE_PEND_MAX - 1) * 4
        rep movsd
        pop edi
        jmp .lleno
.cola:
        cmp dword [edx+PE_QUIT], 0  ; PostQuitMessage: cuando la cola se vacía
        je .mirar
        mov ebx, [edx+PE_TAREA]
        cmp dword [ebx+T_N], 0
        jne .mirar
        mov dword [edx+PE_QUIT], 0
        mov ecx, [edx+PE_QCOD]
.salir:
        xor eax, eax
        mov [edi], eax
        mov dword [edi+4], WM_QUIT
        mov [edi+8], ecx
        mov [edi+12], eax
        jmp .lleno
.mirar:
        push edi
        lea edi, [esp+8]
        call msg_mirar
        pop edi
        jc .nada
        mov ebp, [esp+4+M_VEN]
        mov eax, [esp+4+M_MSG]
        mov ecx, [esp+4+M_W]
        mov ebx, [esp+4+M_L]
        test ebp, ebp
        jz .sinven
        cmp dword [ebp+V_PROC], w32_ven_proc    ; de otra ventana de la tarea
        jne .desp                               ; (un cuadro): a ella
        cmp [ebp+V_D0], edx
        jne .desp
        test eax, W32_DIFERIDO      ; enviado desde otra tarea: al WndProc
        jnz .desp
        test eax, W32_CRUDO
        jnz .crudo
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .tecla
        mov esi, edx
        mov edx, ebx
        call w32_traducir
        mov ebx, edx
        mov edx, esi
        jc .l
        jmp .pon
.crudo:
        and eax, ~W32_CRUDO
.pon:
        mov [edi+4], eax
        mov [edi+8], ecx
        mov [edi+12], ebx
        mov eax, ebp
        call w32_hwnd
        mov [edi], eax
.lleno:
        call tics                   ; la hora y dónde está el ratón
        imul eax, eax, 55
        mov [edi+16], eax
        mov eax, [rat_x]
        mov [edi+20], eax
        mov eax, [rat_y]
        mov [edi+24], eax
        xor eax, eax
        cmp dword [edi+4], WM_QUIT
        setne al
        clc
        jmp .sal
.tecla:
        push esi
        mov esi, edx
        mov edx, ebx
        call w32x_tecla_mod         ; (fase 3d) las de mayúsculas, para GetKeyState
        mov edx, esi
        pop esi
        push edx
        call w32_tecla              ; EBX=tecla ECX=carácter EDX=lParam
        mov esi, edx
        pop edx
        mov eax, ebp
        call w32_hwnd
        mov [edi], eax
        mov dword [edi+4], WM_KEYDOWN
        mov [edi+8], ebx
        mov [edi+12], esi
        mov [edx+PE_TEC_CAR], ecx   ; (para TranslateMessage)
        mov [edx+PE_TEC_ULT], ecx   ; (fase 3d: y para ToAscii)
        mov [edx+PE_TEC_VEN], eax
        mov [edx+PE_TEC_L], esi
        push edi                    ; y detrás, que se suelta
        mov edi, esi
        or edi, 0xC0000000
        mov esi, ebx
        mov ebx, eax
        mov ecx, 0x0101             ; WM_KEYUP
        call w32_pend_fin
        pop edi
        jmp .lleno
.sinven:
        test eax, W32_CRUDO         ; PostMessageA sin ventana
        jnz .crudo
        cmp eax, WM_QUIT            ; (el WM_QUIT de msg_salir)
        jne .l
        xor ecx, ecx
        jmp .salir
.desp:
        push edi
        lea edi, [esp+8]
        call msg_despachar
        pop edi
        jmp .l
.nada:
        cmp dword [esp], 0
        jne .n1
        call tar_ceder
        jmp .l
.n1:
        cmp dword [esp], 2
        je .no
        mov dword [esp], 2
        call ceder
        jmp .l
.no:
        stc
.sal:
        lea esp, [esp+M_TAM+4]
        pop esi
        pop ebx
        pop ebp
        ret
; BOOL TranslateMessage(MSG*): el WM_CHAR de la tecla que se acaba de tomar
u_TranslateMessage:
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]
        cmp dword [esi+4], WM_KEYDOWN
        jne u_cero
        mov eax, [edx+PE_TEC_CAR]
        test eax, eax
        jz u_cero
        mov ebx, [esi]
        cmp ebx, [edx+PE_TEC_VEN]
        jne u_cero
        mov dword [edx+PE_TEC_CAR], 0
        mov ecx, WM_CHAR
        mov esi, eax
        mov edi, [edx+PE_TEC_L]
        call w32_pend_ini
        jmp u_uno
; LRESULT DispatchMessageA(MSG*): al WndProc de su ventana (o a la función
; del temporizador)
u_DispatchMessageA:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        mov eax, [edi]
        call w32_ven
        jz u_cero
        mov ebp, eax
        mov esi, edx                ; ESI = el programa
        mov eax, [edi+4]
        mov ecx, [edi+8]
        mov edx, [edi+12]
        cmp dword [ebp+V_PROC], w32_ctl_proc    ; (a un control)
        je w32_proc
        cmp eax, WM_TIMER
        jne .n
        test edx, edx
        jz .n
        mov eax, edx
        mov esi, ecx
        mov ecx, WM_TIMER
        jmp w32_proc_fun
.n:
        cmp eax, WM_PAINT
        jne w32_proc
        call w32_proc
        call w32_viva
        jne .x
        xor ecx, ecx                ; si no la ha pintado, se da por pintada
        mov [ebp+V_UX0], ecx
        mov [ebp+V_UX1], ecx
.x:
        ret
; void PostQuitMessage(código)
u_PostQuitMessage:
        mov eax, [ebp]
        mov [edx+PE_QCOD], eax
        mov dword [edx+PE_QUIT], 1
        mov byte [actividad], 1
        ret
; BOOL PostMessageA(ventana, mensaje, wParam, lParam): a la cola (sin ventana,
; al programa)
u_PostMessageA:
        mov eax, [ebp]
        test eax, eax
        jz .m
        call w32_ven
        jz u_cero
.m:
        mov ebx, [edx+PE_TAREA]
        cmp dword [ebx+T_N], COLA_MAX
        jae u_cero
        mov ecx, [ebp+8]
        mov edx, [ebp+12]
        mov ebp, [ebp+4]
        xchg eax, ebp
        or eax, W32_CRUDO
        call msg_poner_en
        jmp u_uno
; LRESULT SendMessageA(ventana, mensaje, wParam, lParam): a su WndProc, ya
u_SendMessageA:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov ecx, [ebp+8]
        mov edx, [ebp+12]
        mov ebx, [ebp+4]
        mov ebp, eax
        mov eax, ebx
        jmp w32_proc

; BOOL GetClientRect(ventana, RECT*) y GetWindowRect(ventana, RECT*)
u_GetClientRect:
        mov eax, [ebp]
        test eax, eax               ; (fase 3d) el escritorio: la pantalla
        jz u_GetWindowRect.esc
        cmp eax, W32_HESC
        je u_GetWindowRect.esc
        call w32_ven
        jz u_cero
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        mov ebp, eax
        call w32_cli
        xor eax, eax
        stosd
        stosd
        mov eax, ecx
        stosd
        mov eax, edx
        stosd
        jmp u_uno
u_GetWindowRect:
        mov eax, [ebp]
        test eax, eax               ; (fase 3d) el escritorio: la pantalla
        jz .esc
        cmp eax, W32_HESC
        je .esc
        call w32_ven
        jz u_cero
        jmp .v
.esc:
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        xor eax, eax
        stosd
        stosd
        mov eax, [vid_ancho]
        stosd
        mov eax, [vid_alto]
        stosd
        jmp u_uno
.v:
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        mov ecx, [eax+V_X]
        mov [edi], ecx
        add ecx, [eax+V_AN]
        mov [edi+8], ecx
        mov ecx, [eax+V_Y]
        mov [edi+4], ecx
        add ecx, [eax+V_AL]
        mov [edi+12], ecx
        jmp u_uno
; BOOL InvalidateRect(ventana, RECT* o NULL, borrar)
u_InvalidateRect:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, [ebp+4]
        mov ebp, eax
        test esi, esi
        jnz .r
        call ven_invalidar
        jmp u_uno
.r:
        add esi, [edx+PE_DELTA]
        mov eax, [esi]
        mov ebx, [esi+4]
        mov ecx, [esi+8]
        sub ecx, eax
        mov edx, [esi+12]
        sub edx, ebx
        call ven_invalidar_rect
        jmp u_uno
; BOOL ValidateRect(ventana, RECT*): todo queda dado por pintado
u_ValidateRect:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        xor ecx, ecx
        mov [eax+V_UX0], ecx
        mov [eax+V_UX1], ecx
        jmp u_uno
; BOOL MoveWindow(ventana, x, y, ancho, alto, repintar)
u_MoveWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, eax
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        mov ecx, [ebp+12]
        mov edx, [ebp+16]
        mov ebp, esi
        test dword [ebp+V_ESTILO], E_TITULO
        jz .m
        cmp ebx, BARRA_H
        jge .m
        mov ebx, BARRA_H
.m:
        call ven_mover
        jmp u_uno
; (interna) EBP=ventana EAX=texto (del programa) EDX=programa: su título
w32_poner_titulo:
        push ecx
        push esi
        push edi
        mov esi, eax
        mov edi, [ebp+V_D4]
        mov ecx, 255
        call w32_texto
        mov esi, [ebp+V_D4]
        call ven_texto
        pop edi
        pop esi
        pop ecx
        ret
; (interna) EBP=ventana EDI=búfer (del programa) ECX=su tamaño EDX=programa
; -> EAX=letras copiadas (de la página 437 a la 1252)
w32_leer_titulo:
        xor eax, eax
        test ecx, ecx
        jle .x
        test edi, edi
        jz .x
        add edi, [edx+PE_DELTA]
        mov esi, [ebp+V_D4]
.c:
        dec ecx
        jz .f
        movzx ebx, byte [esi+eax]
        test ebx, ebx
        jz .f
        cmp ebx, 0x80
        jb .p
        mov bl, [w32_437+ebx-0x80]
.p:
        mov [edi+eax], bl
        inc eax
        jmp .c
.f:
        mov byte [edi+eax], 0
.x:
        ret
; BOOL SetWindowTextA(ventana, texto)
u_SetWindowTextA:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov ecx, [ebp+4]
        mov ebp, eax
        cmp dword [ebp+V_PROC], w32_ctl_proc
        je .ctl
        mov eax, ecx
        call w32_poner_titulo
        jmp u_uno
.ctl:
        mov edx, ecx
        mov eax, WM_SETTEXT
        xor ecx, ecx
        call w32_proc
        jmp u_uno
; int GetWindowTextA(ventana, búfer, tamaño) y GetWindowTextLengthA(ventana)
u_GetWindowTextA:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov edi, [ebp+4]
        mov ecx, [ebp+8]
        mov ebp, eax
        cmp dword [ebp+V_PROC], w32_ctl_proc
        jne w32_leer_titulo
        mov edx, edi
        mov eax, WM_GETTEXT
        jmp w32_proc
u_GetWindowTextLengthA:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        cmp dword [eax+V_PROC], w32_ctl_proc
        je .ctl
        mov esi, [eax+V_D4]
        jmp cad_largo
.ctl:
        mov ebp, eax
        mov eax, 0x000E             ; WM_GETTEXTLENGTH
        xor ecx, ecx
        xor edx, edx
        jmp w32_proc
; LONG GetWindowLongA(ventana, cuál) y SetWindowLongA(ventana, cuál, valor)
u_GetWindowLongA:
        xor ebx, ebx
        jmp u_long
u_SetWindowLongA:
        mov ebx, 1
u_long:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, eax                ; ESI = la ventana
        mov ecx, [ebp+4]
        cmp dword [esi+V_PROC], w32_ctl_proc
        je .ctl
        mov edi, [esi+V_D4]
        add edi, VB_USER
        cmp ecx, -21                ; GWL_USERDATA
        je .dir
        lea edi, [esi+V_D1]
        cmp ecx, -4                 ; GWL_WNDPROC
        je .dir
        lea edi, [esi+V_ID]
        cmp ecx, -12                ; GWL_ID
        je .dir
        lea edi, [esi+V_D6]
        cmp ecx, -20                ; GWL_EXSTYLE
        je .dir
        cmp ecx, -16                ; GWL_STYLE
        je .estilo
        cmp ecx, -6                 ; GWL_HINSTANCE
        je .modulo
        cmp ecx, -8                 ; GWL_HWNDPARENT
        je .madre
        test ecx, ecx               ; los octetos de más de la clase
        js u_cero
        mov edi, [esi+V_D2]
        mov eax, [edi+WC_EXTRA]
        sub eax, 4
        cmp ecx, eax
        jg u_cero
        mov edi, [esi+V_D4]
        lea edi, [edi+ecx+VB_EXTRA]
.dir:
        mov eax, [edi]
        test ebx, ebx
        jz .x
        mov ecx, [ebp+8]
        mov [edi], ecx
.x:
        ret
.estilo:
        mov eax, [esi+V_D3]
        and eax, ~WS_VISIBLE
        test dword [esi+V_ESTADO], S_VISIBLE
        jz .e1
        or eax, WS_VISIBLE
.e1:
        test ebx, ebx
        jz .x
        mov ecx, [ebp+8]
        mov [esi+V_D3], ecx
        ret
.modulo:
        mov eax, [edx+PE_BASE]
        ret
.madre:
        mov eax, [esi+V_PADRE]
        jmp w32_hwnd
; de un control: su número, su madre, el módulo y un estilo aproximado
.ctl:
        lea edi, [esi+V_ID]
        cmp ecx, -12
        je .dir
        cmp ecx, -6
        je .modulo
        cmp ecx, -8
        je .madre
        cmp ecx, -16
        jne u_cero
        mov eax, WS_CHILD
        test dword [esi+V_ESTADO], S_VISIBLE
        jz .c1
        or eax, WS_VISIBLE
.c1:
        test dword [esi+V_ESTADO], S_INACTIVA
        jz .c2
        or eax, WS_DISABLED
.c2:
        mov ecx, [esi+V_ESTILO]
        test ecx, E_TABULA
        jz .c3
        or eax, WS_TABSTOP
.c3:
        mov edi, [esi+V_CLASE]
        cmp edi, c_boton
        jne .c6
        test ecx, BS_DEFECTO
        jz .c4
        or eax, 1
.c4:
        test ecx, BS_CASILLA
        jz .c5
        or eax, 3
.c5:
        test ecx, BS_OPCION
        jz .x
        or eax, 9
        ret
.c6:
        cmp edi, c_edit
        jne .x
        test ecx, ES_MULTI
        jz .x
        or eax, 4
        ret

; UINT SetTimer(ventana, número, milisegundos, función) y KillTimer(ventana,
; número). Sin ventana no hay temporizador
u_SetTimer:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, eax                ; ESI = la ventana
        mov ecx, [ebp+4]
        call u_tmp_quitar           ; (la función que tuviera ese número)
        mov eax, [ebp+12]
        test eax, eax
        jz .t
        mov edi, [edx+PE_W32]
        add edi, WB_TMP
        mov ebx, TP_MAX
.b:
        cmp dword [edi+TP_VEN], 0
        je .hay
        add edi, TP_TAM
        dec ebx
        jnz .b
        jmp u_cero
.hay:
        mov [edi+TP_VEN], esi
        mov [edi+TP_ID], ecx
        mov [edi+TP_PROC], eax
.t:
        mov edx, [ebp+8]
        mov ebp, esi
        call tmp_poner
        mov eax, ecx
        test eax, eax
        jnz .x
        inc eax
.x:
        ret
u_KillTimer:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, eax
        mov ecx, [ebp+4]
        call u_tmp_quitar
        mov ebp, esi
        call tmp_quitar
        jmp u_uno
; (interna) quita la función del temporizador ECX de la ventana ESI
u_tmp_quitar:
        push edi
        push ebx
        mov edi, [edx+PE_W32]
        add edi, WB_TMP
        mov ebx, TP_MAX
.b:
        cmp [edi+TP_VEN], esi
        jne .s
        cmp [edi+TP_ID], ecx
        jne .s
        mov dword [edi+TP_VEN], 0
.s:
        add edi, TP_TAM
        dec ebx
        jnz .b
        pop ebx
        pop edi
        ret

; int GetSystemMetrics(cuál)
u_GetSystemMetrics:
        mov ecx, [ebp]
        mov eax, [vid_ancho]        ; SM_CXSCREEN, SM_CXFULLSCREEN
        test ecx, ecx
        jz .x
        cmp ecx, 16
        je .x
        mov eax, [vid_alto]         ; SM_CYSCREEN
        cmp ecx, 1
        je .x
        sub eax, BARRA_H + TIT_H + 1    ; SM_CYFULLSCREEN
        cmp ecx, 17
        je .x
        mov esi, w32_metricas
.b:
        lodsw
        cmp al, 0xFF
        je u_cero
        cmp al, cl
        jne .b
        movzx eax, ah
.x:
        ret
; [RUEDA] SystemParametersInfoA: con la de la fase 3d (win32x.inc)
; DWORD GetSysColor(cuál) y HBRUSH GetSysColorBrush(cuál)
u_GetSysColor:
        mov eax, [ebp]
        cmp eax, W32_NSYSCOL
        jae u_cero
        mov eax, [w32_syscol+eax*4]
        ret
u_GetSysColorBrush:
        mov eax, [ebp]
        cmp eax, W32_NSYSCOL
        jae u_cero
        inc eax
        ret
; int wsprintfA(búfer, formato, ...): %d %i %u %x %X %c %s %%, con ancho,
; ceros a la izquierda y «l» (los argumentos los quita el programa)
; (fase 3d) int wvsprintfA(búfer, formato, va_list)
u_wvsprintfA:
        mov ebx, [ebp+8]
        add ebx, [edx+PE_DELTA]
        jmp u_wsprintfA.va
u_wsprintfA:
        lea ebx, [ebp+8]            ; EBX = el argumento siguiente
.va:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov eax, [edx+PE_DELTA]
        mov [w32_wsp_delta], eax
        push edi
.c:
        lodsb
        test al, al
        jz .f
        cmp al, '%'
        je .p
.pon:
        stosb
        jmp .c
.p:
        xor ecx, ecx                ; ECX = el ancho, DL = con qué se rellena
        mov dl, ' '
        lodsb
        cmp al, '-'
        jne .p0
        lodsb
.p0:
        cmp al, '0'
        jne .an
        mov dl, '0'
.an:
        cmp al, '0'
        jb .mod
        cmp al, '9'
        ja .mod
        imul ecx, 10
        movzx eax, al
        lea ecx, [ecx+eax-'0']
        lodsb
        jmp .an
.mod:
        cmp al, 'l'
        je .salta
        cmp al, 'h'
        jne .conv
.salta:
        lodsb
.conv:
        test al, al
        jz .f
        cmp al, 'c'
        jne .c1
        mov eax, [ebx]
        add ebx, 4
        jmp .pon
.c1:
        cmp al, 's'
        jne .c2
        push esi
        mov esi, [ebx]
        add ebx, 4
        test esi, esi
        jz .s2
        add esi, [w32_wsp_delta]
        call cad_largo              ; (a la derecha, con el ancho)
        sub ecx, eax
        jle .s1
        mov al, ' '
        rep stosb
.s1:
        lodsb
        test al, al
        jz .s2
        stosb
        jmp .s1
.s2:
        pop esi
        jmp .c
.c2:
        mov dh, 0                   ; DH = la letra de las cifras de más de 9
        cmp al, 'u'
        je .u
        cmp al, 'd'
        je .d
        cmp al, 'i'
        je .d
        mov dh, 'a' - 10
        cmp al, 'x'
        je .x
        mov dh, 'A' - 10
        cmp al, 'X'
        jne .pon                    ; («%%» y lo demás: tal cual)
.x:
        mov eax, [ebx]
        add ebx, 4
        push esi
        mov esi, 16
        jmp .num
.d:
        mov eax, [ebx]
        test eax, eax
        jns .u
        neg eax
        mov byte [edi], '-'
        inc edi
        dec ecx
        jmp .u1
.u:
        mov eax, [ebx]
.u1:
        add ebx, 4
        push esi
        mov esi, 10
.num:
        mov [w32_wsp], dx
        push ebx
        xor ebx, ebx                ; EBX = cifras (en la pila, al revés)
.n1:
        xor edx, edx
        div esi
        cmp dl, 10
        jb .n2
        add dl, [w32_wsp+1]
        jmp .n3
.n2:
        add dl, '0'
.n3:
        push edx
        inc ebx
        test eax, eax
        jnz .n1
        sub ecx, ebx
        jle .n4
        mov al, [w32_wsp]
        rep stosb
.n4:
        pop eax
        stosb
        dec ebx
        jnz .n4
        pop ebx
        pop esi
        jmp .c
.f:
        mov byte [edi], 0
        mov eax, edi
        pop edi
        sub eax, edi
        ret

; =============================================================================
;  GDI32
; =============================================================================
; COLORREF SetTextColor(HDC, color), SetBkColor(HDC, color), int SetBkMode(HDC,
; modo) y UINT SetTextAlign(HDC, cómo): devuelven lo de antes
g_SetTextColor:
        mov ecx, DC_CTEXTO
        jmp g_poner
g_SetBkColor:
        mov ecx, DC_CFONDO
        jmp g_poner
g_SetBkMode:
        mov ecx, DC_MODO
        jmp g_poner
g_SetTextAlign:
        mov ecx, DC_ALIN
g_poner:
        mov eax, [ebp]
        call w32_dc
        jc g_mal
        mov eax, [ebp+4]
        xchg eax, [edi+ecx]
        ret
g_mal:
        or eax, -1                  ; CLR_INVALID
        ret
; COLORREF GetTextColor(HDC) y GetBkColor(HDC)
g_GetTextColor:
        mov ecx, DC_CTEXTO
        jmp g_leer
g_GetBkColor:
        mov ecx, DC_CFONDO
g_leer:
        mov eax, [ebp]
        call w32_dc
        jc g_mal
        mov eax, [edi+ecx]
        ret
; HGDIOBJ GetStockObject(cuál)
g_GetStockObject:
        mov eax, [ebp]
        cmp eax, W32_NSERIE
        jae u_cero
        add eax, W32_HGDI
        call w32_obj
        jc u_cero
        ret
; BOOL MoveToEx(HDC, x, y, POINT* donde estaba) y LineTo(HDC, x, y)
g_MoveToEx:
        mov eax, [ebp]
        call w32_dc
        jc u_cero
        mov esi, [ebp+12]
        test esi, esi
        jz .m
        add esi, [edx+PE_DELTA]
        mov eax, [edi+DC_PX]
        mov [esi], eax
        mov eax, [edi+DC_PY]
        mov [esi+4], eax
.m:
        mov eax, [ebp+4]
        mov [edi+DC_PX], eax
        mov eax, [ebp+8]
        mov [edi+DC_PY], eax
        jmp u_uno
; de la página 437 (80h a FFh) a la 1252 (tools/mk1252.py); [GLIFOS] la 437 de Ventanas,
; con Á, Ó, €... en códigos que no se pintaban (tools/hispa437.py)
w32_437:
        db 0xC7, 0xFC, 0xE9, 0xE2, 0xE4, 0xE0, 0xE5, 0xE7, 0xEA, 0xEB, 0xE8, 0xEF, 0xEE, 0xEC, 0xC4, 0xC5
        db 0xC9, 0xE6, 0xC6, 0xF4, 0xF6, 0xF2, 0xFB, 0xF9, 0xFF, 0xD6, 0xDC, 0xA2, 0xA3, 0xA5, 0x3F, 0x83
        db 0xE1, 0xED, 0xF3, 0xFA, 0xF1, 0xD1, 0xAA, 0xBA, 0xBF, 0x3F, 0xAC, 0xBD, 0xBC, 0xA1, 0xAB, 0xBB
        db 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0xC0, 0xC1, 0xC2, 0xC3, 0x3F, 0x3F, 0x3F, 0x3F, 0xC8, 0xCA, 0x3F
        db 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0xCB, 0xCC, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0xCD
        db 0xCE, 0xCF, 0xD2, 0xD3, 0xD4, 0xD5, 0xD9, 0xDA, 0xDB, 0x3F, 0x3F, 0x3F, 0x3F, 0xE3, 0xF5, 0x3F
        db 0x3F, 0xDF, 0x8C, 0x3F, 0x3F, 0x9C, 0xB5, 0xA9, 0xAE, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F
        db 0x3F, 0xB1, 0x3F, 0x3F, 0x80, 0x85, 0xF7, 0x3F, 0xB0, 0x3F, 0xB7, 0x3F, 0x3F, 0xB2, 0x3F, 0xA0
