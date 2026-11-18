; =============================================================================
;  Hispa Ventanas - [HISPA-486] los controles de serie de Windows
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Fase 3c de Hispa-486 (docs/486/HISPA-486.md). Va detrás de win32u.inc, en
;  la parte alta de VENTANAS.COM. Sala limpia: hecho a partir de la
;  documentación pública del API Win32 y del formato PE (.rsrc).
;
;  LOS CONTROLES. Un control de Windows (BUTTON, EDIT, STATIC, LISTBOX) ES un
;  control de Hispa Ventanas (c_boton, c_edit, c_estatico, c_lista de
;  controles.inc), hijo de la ventana de Windows; el COMBOBOX (el sencillo:
;  una lista que se despliega) es una clase de aquí (c_w32combo) que abre un
;  menú emergente de Hispa Ventanas. En su ficha: V_PROC = w32_ctl_proc (que
;  pasa los mensajes al procedimiento de su clase) y V_TEXTO = un bloque del
;  montón con su texto (en la página 437); V_D0...V_D7 son de su clase de
;  Hispa Ventanas, así que su programa es el de su tarea (w32_ctl_prog). Su
;  HWND es como el de las demás.
;  - El ratón y el teclado los atiende el control, como en cualquier programa
;    de Hispa Ventanas (el foco, Tab entre los que tienen WS_TABSTOP, el
;    espacio y el Intro en los botones). El programa de Windows no ve esos
;    mensajes; le llega el aviso: el control avisa a su madre con WM_COMMAND
;    (ECX = id + aviso × 65536, EDX = el control), y w32_traducir pone en
;    lParam su HWND. Los avisos son los de Windows: BN_CLICKED (0),
;    LBN_SELCHANGE (1), LBN_DBLCLK (2), CBN_SELCHANGE (1) y EN_CHANGE (300h:
;    w32_ctl_proc mira si el cuadro de texto ha cambiado con cada mensaje).
;  - Lo que el control no quiere (una tecla, un carácter) sube a su madre,
;    como en Hispa Ventanas: la ventana de Windows recibe WM_KEYDOWN y WM_CHAR
;    aunque el foco esté en un botón.
;  - Los mensajes de Windows a un control (SendMessageA, SetWindowTextA...)
;    pasan por w32_ctl_enviar, que los traduce a los de Hispa Ventanas y
;    convierte los textos (página 1252 <-> 437; en los cuadros de varias
;    líneas, CR LF <-> LF).
; =============================================================================

WS_TABSTOP  equ 0x00010000
WS_DISABLED equ 0x08000000
WS_BORDER   equ 0x00800000
CT_TEXTO    equ 256                 ; el bloque del texto de un control (como poco)
CBO_MAX     equ 24                  ; un COMBOBOX: líneas,
CBO_TXT     equ 40                  ;   letras de cada una (con el 0)
CBO_MENU    equ CBO_MAX * CBO_TXT   ;   y, detrás, el desplegable que abre
CBO_TAM     equ CBO_MENU + CBO_MAX * (CBO_TXT + 2) + 4

c_w32combo  dd cbo_proc
            db C_BLANCO, CB_FOCO, 0, 0
; las clases de serie, en el orden de w32c_clases (y el átomo de cada una en
; las plantillas de diálogo: 80h BUTTON, 81h EDIT, 82h STATIC, 83h LISTBOX,
; 85h COMBOBOX)
w32c_nombres db "BUTTON", 0, "EDIT", 0, "STATIC", 0, "LISTBOX", 0, "COMBOBOX", 0, 0
w32c_clases dd c_boton, c_edit, c_estatico, c_lista, c_w32combo
w32c_atomos db 0x80, 0x81, 0x82, 0x83, 0x85

; ---- EAX=nombre de clase (del programa) EDX=programa -> EAX=cuál de las de
; serie (0 a 4); CF=1 si no es ninguna ----
w32c_clase:
        push ebx
        push esi
        push edi
        cmp eax, 0x10000
        jb .no
        mov esi, eax
        add esi, [edx+PE_DELTA]
        mov edi, w32c_nombres
        xor ebx, ebx
.b:
        cmp byte [edi], 0
        je .no
        call w32_cmp_i
        je .si
.s:
        inc edi
        cmp byte [edi-1], 0
        jne .s
        inc ebx
        jmp .b
.si:
        mov eax, ebx
        clc
        jmp .x
.no:
        stc
.x:
        pop edi
        pop esi
        pop ebx
        ret

; ---- hace un control: EAX=cuál (0 a 4) EBX=su estilo de Windows ESI=su texto
; (del programa; 0: ninguno) EDI=su madre (ventana) EDX=programa, y en w32c_p
; su sitio, su tamaño y su número -> EAX=el control (0: no se puede).
; Conserva lo demás ----
w32c_hacer:
        pushad
        mov dword [esp+28], 0
        mov ebp, eax                ; EBP = cuál
        ; su texto, en un bloque
        mov ecx, CT_TEXTO
        test esi, esi
        jz .t1
        push esi
        add esi, [edx+PE_DELTA]
        call cad_largo
        pop esi
        inc eax
        cmp eax, ecx
        jbe .t1
        mov ecx, eax
.t1:
        call w32_pedir
        jc .x
        mov [w32c_p+P_TEXTO], eax
        push edi
        mov edi, eax
        dec ecx
        call w32_texto
        pop edi
        cmp ebp, 2                  ; (fase 3d) un texto fijo: sin los «&» (con
        jne .t2                     ; «&&», uno), salvo con SS_NOPREFIX
        test ebx, 0x80
        jnz .t2
        push esi
        push edi
        mov esi, [w32c_p+P_TEXTO]
        mov edi, esi
.t3:
        lodsb
        cmp al, '&'
        jne .t4
        lodsb
.t4:
        stosb
        test al, al
        jnz .t3
        pop edi
        pop esi
.t2:
        ; el estilo
        mov ecx, E_HIJA
        test ebx, WS_VISIBLE
        jnz .e1
        or ecx, E_OCULTA
.e1:
        cmp ebp, 2                  ; (un texto fijo no toma el foco)
        je .e3
        test ebx, WS_TABSTOP
        jz .e3
        or ecx, E_TABULA
.e3:
        mov eax, ebx
        and eax, 0x0F
        test ebp, ebp
        jnz .noboton
        cmp eax, 7                  ; BS_GROUPBOX: un rótulo
        jne .b1
        mov ebp, 2
        and ecx, ~E_TABULA
        jmp .hecho
.b1:
        cmp eax, 1                  ; BS_DEFPUSHBUTTON
        jne .b2
        or ecx, BS_DEFECTO
.b2:
        cmp eax, 2                  ; casillas (también las de tres estados)
        je .b3
        cmp eax, 3
        je .b3
        cmp eax, 5
        je .b3
        cmp eax, 6
        jne .b4
.b3:
        or ecx, BS_CASILLA
.b4:
        cmp eax, 4                  ; opciones
        je .b5
        cmp eax, 9
        jne .hecho
.b5:
        or ecx, BS_OPCION
        jmp .hecho
.noboton:
        cmp ebp, 1
        jne .noedit
        test ebx, 4                 ; ES_MULTILINE
        jz .hecho
        or ecx, ES_MULTI
        jmp .hecho
.noedit:
        cmp ebp, 2
        jne .nostatic
        cmp eax, 2                  ; SS_RIGHT
        jne .hecho
        or ecx, SS_DERECHA
        jmp .hecho
.nostatic:
        cmp ebp, 4
        jne .hecho
        mov dword [w32c_p+P_AL], 24 ; (un COMBOBOX: cerrado mide una línea)
.hecho:
        mov [w32c_p+P_ESTILO], ecx
        mov eax, [w32c_clases+ebp*4]
        mov [w32c_p+P_CLASE], eax
        mov esi, w32c_p
        mov ebp, edi
        call ven_crear
        test eax, eax
        jz .falla
        mov dword [eax+V_PROC], w32_ctl_proc
        test ebx, WS_DISABLED
        jz .h1
        or dword [eax+V_ESTADO], S_INACTIVA
.h1:
        mov [esp+28], eax
.x:
        popad
        ret
.falla:
        mov eax, [w32c_p+P_TEXTO]
        call mem_soltar
        jmp .x

; ---- (u_CreateWindowExA) EAX=cuál de las clases de serie EBP=los argumentos
; EDX=programa -> EAX=su HWND ----
w32c_crear:
        push eax
        mov eax, [ebp+32]           ; la madre
        call w32_ven
        mov edi, eax
        pop eax
        jz u_cero
        mov ecx, [ebp+16]
        mov [w32c_p+P_X], ecx
        mov ecx, [ebp+20]
        mov [w32c_p+P_Y], ecx
        mov ecx, [ebp+24]
        mov [w32c_p+P_AN], ecx
        mov ecx, [ebp+28]
        mov [w32c_p+P_AL], ecx
        mov ecx, [ebp+36]
        mov [w32c_p+P_ID], ecx
        mov ebx, [ebp+12]
        mov esi, [ebp+8]
        call w32c_hacer
        jmp w32_hwnd

; =============================================================================
;  El procedimiento (de Hispa Ventanas) de un control de Windows: el de su
;  clase y, además, EN_CHANGE y su texto
; =============================================================================
w32_ctl_proc:
        test eax, W32_CRUDO         ; (PostMessageA: ya es de Windows)
        jnz .crudo
        mov esi, [ebp+V_CLASE]
        cmp eax, WM_DESTROY
        je .destruir
        cmp esi, c_edit
        je .edit
        jmp [esi+C_PROC]
.edit:
        push dword [ebp+V_D7]       ; ¿cambia con este mensaje?
        and dword [ebp+V_D7], ~1
        call ed_proc
        pop ebx
        cmp dword [ebp+V_USO], 1
        jne .e2
        cmp dword [ebp+V_PROC], w32_ctl_proc
        jne .e2
        test byte [ebp+V_D7], 1
        jz .e1
        push eax
        mov eax, 0x0300             ; EN_CHANGE
        call avisar
        pop eax
.e1:
        and ebx, 1
        or [ebp+V_D7], ebx
.e2:
        ret
.destruir:
        call [esi+C_PROC]
        mov eax, [ebp+V_TEXTO]
        mov dword [ebp+V_TEXTO], 0
        call mem_soltar
        xor eax, eax
        ret
.crudo:
        and eax, ~(W32_CRUDO | W32_DIFERIDO)
        ; (sigue)
; ---- un mensaje de Windows al control EBP: EAX, ECX=wParam, EDX=lParam (los
; punteros, del programa) -> EAX. Conserva lo demás ----
w32_ctl_enviar:
        pushad
        call w32_ctl_prog           ; ESI = el programa
        test esi, esi
        jz .cero
        mov ebx, [ebp+V_CLASE]      ; EBX = su clase
        mov edi, edx
        test edi, edi
        jz .p
        add edi, [esi+PE_DELTA]     ; EDI = lParam como puntero de aquí
.p:
        cmp eax, WM_SETTEXT
        je .ptexto
        cmp eax, WM_GETTEXT
        je .texto
        cmp eax, 0x000E             ; WM_GETTEXTLENGTH
        je .largo
        cmp ebx, c_boton
        je .boton
        cmp ebx, c_edit
        je .edit
        cmp ebx, c_lista
        je .lista
        cmp ebx, c_w32combo
        je .combo
.cero:
        xor eax, eax
.fin:
        mov [esp+28], eax
        popad
        ret
; WM_SETTEXT
.ptexto:
        cmp ebx, c_edit
        je .pt_edit
        cmp ebx, c_lista
        je .cero
        cmp ebx, c_w32combo
        je .cero
        mov edi, [ebp+V_TEXTO]
        test edi, edi
        jz .cero
        mov ecx, [edi-8]            ; (lo que mide su bloque, con la cabecera)
        sub ecx, 9
        xchg edx, esi
        call w32_texto
        call ven_invalidar
        mov eax, 1
        jmp .fin
.pt_edit:
        call .a_temp                ; EAX = el texto, en un bloque (0: no hay)
        test eax, eax
        jz .cero
        push eax
        mov edx, eax
        mov eax, WM_SETTEXT
        call ed_proc
        call ven_invalidar
        pop eax
        call mem_soltar
        mov eax, 0x0300             ; EN_CHANGE
        call avisar
        mov eax, 1
        jmp .fin
; (de w32_ctl_enviar) el texto del programa EDX (0: vacío), a un bloque nuevo y
; a la página 437 -> EAX (0: no hay memoria), ECX=sus letras
.a_temp:
        push esi
        push edi
        xor eax, eax
        test edi, edi
        jz .at1
        xchg esi, edi
        call cad_largo
        xchg esi, edi
.at1:
        lea ecx, [eax+1]
        call mem_pedir
        jc .at2
        push eax
        mov edi, eax
        dec ecx
        xchg edx, esi               ; (EDX = el programa, ESI = el texto)
        call w32_texto
        xchg edx, esi
        pop eax
        push eax
        mov esi, eax
        call cad_largo
        mov ecx, eax
        pop eax
.at2:
        pop edi
        pop esi
        ret
; WM_GETTEXT (ECX = sitio, EDI = búfer) y WM_GETTEXTLENGTH
.largo:
        xor edi, edi
        mov ecx, 0x7FFFFFFF
.texto:
        push ecx
        call w32_ctl_texto          ; ESI=su texto EBX=letras DL=1 si es de varias líneas
        pop ecx
        call w32_sacar
        jmp .fin
; BUTTON
.boton:
        cmp eax, 0x00F0             ; BM_GETCHECK
        je .b_marca
        cmp eax, 0x00F2             ; BM_GETSTATE
        je .b_marca
        cmp eax, 0x00F5             ; BM_CLICK
        je .b_clic
        cmp eax, 0x00F1             ; BM_SETCHECK
        jne .cero
        and ecx, 1
        mov eax, BM_PONER_MARCA
        call bot_proc
        jmp .cero
.b_marca:
        mov eax, BM_MARCA
        call bot_proc
        and eax, 1
        jmp .fin
.b_clic:
        call bot_clic
        jmp .cero
; EDIT
.edit:
        cmp eax, 0x00B1             ; EM_SETSEL
        je .e_sel
        cmp eax, 0x00B8             ; EM_GETMODIFY
        je .e_modif
        cmp eax, 0x00B9             ; EM_SETMODIFY
        je .e_pmodif
        cmp eax, 0x00BA             ; EM_GETLINECOUNT
        je .e_lineas
        cmp eax, 0x00C2             ; EM_REPLACESEL
        je .e_mete
        cmp eax, WM_CUT
        jb .cero
        cmp eax, WM_PASTE
        ja .cero
        call w32_ctl_proc           ; (por aquí: si cambia, EN_CHANGE)
        jmp .cero
.e_sel:
        mov eax, ecx
        mov ecx, [ebp+V_D1]
        cmp edx, ecx
        jbe .es1
        mov edx, ecx
.es1:
        cmp eax, ecx
        jbe .es2
        mov eax, ecx
.es2:
        call ed_seleccionar
        call ven_invalidar
        jmp .cero
.e_modif:
        mov eax, [ebp+V_D7]
        and eax, 1
        jmp .fin
.e_pmodif:
        and dword [ebp+V_D7], ~1
        and ecx, 1
        or [ebp+V_D7], ecx
        jmp .cero
.e_lineas:
        mov esi, [ebp+V_D0]
        mov ecx, [ebp+V_D1]
        mov eax, 1
.el1:
        test ecx, ecx
        jz .fin
        cmp byte [esi], 10
        jne .el2
        inc eax
.el2:
        inc esi
        dec ecx
        jmp .el1
.e_mete:
        call .a_temp
        test eax, eax
        jz .cero
        push eax
        mov esi, eax
        call ed_meter
        call ed_ver_cursor
        call ven_invalidar
        pop eax
        call mem_soltar
        mov eax, 0x0300
        call avisar
        jmp .cero
; LISTBOX
.lista:
        cmp eax, 0x0180             ; LB_ADDSTRING
        je .l_pon
        cmp eax, 0x0181             ; LB_INSERTSTRING (al final)
        je .l_pon
        cmp eax, 0x0184             ; LB_RESETCONTENT
        je .l_vacia
        cmp eax, 0x0186             ; LB_SETCURSEL
        je .l_psel
        cmp eax, 0x0188             ; LB_GETCURSEL
        je .l_sel
        cmp eax, 0x0189             ; LB_GETTEXT
        je .l_texto
        cmp eax, 0x018A             ; LB_GETTEXTLEN
        je .l_largo
        cmp eax, 0x018B             ; LB_GETCOUNT
        jne .menos
        mov eax, LM_CUENTA
        call lis_proc
        jmp .fin
.l_pon:
        call .a_temp
        test eax, eax
        jz .menos
        push eax
        mov edx, eax
        mov eax, LM_ANADIR
        call lis_proc
        call ven_invalidar
        xchg eax, [esp]
        call mem_soltar
        pop eax
        jmp .fin
.l_vacia:
        mov eax, LM_VACIAR
        call lis_proc
        call ven_invalidar
        jmp .cero
.l_psel:
        push ecx
        mov eax, LM_PONER_SELEC
        call lis_proc
        call ven_invalidar
        pop eax
        jmp .fin
.l_sel:
        mov eax, LM_SELEC
        call lis_proc
        jmp .fin
.l_largo:
        xor edi, edi
.l_texto:
        push edi
        mov eax, LM_CUENTA
        call lis_proc
        pop edi
        cmp ecx, eax
        jae .menos
        mov eax, LM_TEXTO
        push edi
        call lis_proc
        pop edi
        test eax, eax
        jz .menos
.saca:
        mov esi, eax
        call cad_largo
        mov ebx, eax
        mov ecx, 0x7FFFFFFF
        xor edx, edx
        call w32_sacar
        jmp .fin
.menos:
        mov eax, -1                 ; LB_ERR, CB_ERR
        jmp .fin
; COMBOBOX
.combo:
        mov ebx, [ebp+V_D0]
        test ebx, ebx
        jz .menos
        cmp eax, 0x0143             ; CB_ADDSTRING
        je .c_pon
        cmp eax, 0x0146             ; CB_GETCOUNT
        je .c_cuenta
        cmp eax, 0x0147             ; CB_GETCURSEL
        je .c_sel
        cmp eax, 0x0148             ; CB_GETLBTEXT
        je .c_texto
        cmp eax, 0x0149             ; CB_GETLBTEXTLEN
        je .c_largo
        cmp eax, 0x014B             ; CB_RESETCONTENT
        je .c_vacia
        cmp eax, 0x014E             ; CB_SETCURSEL
        jne .menos
        cmp ecx, [ebp+V_D1]
        jb .cp1
        mov ecx, -1
.cp1:
        mov [ebp+V_D2], ecx
        call ven_invalidar
        mov eax, ecx
        jmp .fin
.c_pon:
        mov eax, [ebp+V_D1]
        cmp eax, CBO_MAX
        jae .menos
        push eax
        imul eax, CBO_TXT
        lea edi, [ebx+eax]
        mov ecx, CBO_TXT - 1
        xchg edx, esi               ; (EDX = el programa, ESI = el texto)
        call w32_texto
        inc dword [ebp+V_D1]
        pop eax
        jmp .fin
.c_cuenta:
        mov eax, [ebp+V_D1]
        jmp .fin
.c_sel:
        mov eax, [ebp+V_D2]
        jmp .fin
.c_vacia:
        mov dword [ebp+V_D1], 0
        mov dword [ebp+V_D2], -1
        call ven_invalidar
        jmp .cero
.c_largo:
        xor edi, edi
.c_texto:
        cmp ecx, [ebp+V_D1]
        jae .menos
        imul ecx, CBO_TXT
        lea eax, [ebx+ecx]
        jmp .saca

; ---- EBP=control -> ESI=su programa: el de su tarea (0: ninguno) ----
w32_ctl_prog:
        push eax
        push ecx
        mov eax, [ebp+V_TAREA]
        mov esi, pe_tabla
        mov ecx, PE_MAX
.b:
        cmp dword [esi+PE_USO], 1
        jne .s
        cmp [esi+PE_TAREA], eax
        je .x
.s:
        add esi, PE_TAM
        dec ecx
        jnz .b
        xor esi, esi
.x:
        pop ecx
        pop eax
        ret
; ---- EBP=control -> ESI=su texto (página 437) EBX=sus letras DL=1 si es un
; cuadro de varias líneas (al sacarlo, CR delante de cada LF) ----
w32_ctl_texto:
        xor edx, edx
        xor ebx, ebx
        mov eax, [ebp+V_CLASE]
        cmp eax, c_edit
        je .edit
        cmp eax, c_lista
        je .nada
        cmp eax, c_w32combo
        je .combo
        mov esi, [ebp+V_TEXTO]
        test esi, esi
        jz .nada
.mide:
        call cad_largo
        mov ebx, eax
        ret
.edit:
        mov esi, [ebp+V_D0]
        mov ebx, [ebp+V_D1]
        test dword [ebp+V_ESTILO], ES_MULTI
        setnz dl
        test esi, esi
        jnz .x
.nada:
        mov esi, w32c_vacio
        xor ebx, ebx
.x:
        ret
.combo:
        mov eax, [ebp+V_D2]         ; el texto de la línea elegida
        cmp eax, [ebp+V_D1]
        jae .nada
        mov esi, [ebp+V_D0]
        test esi, esi
        jz .nada
        imul eax, CBO_TXT
        add esi, eax
        jmp .mide
w32c_vacio  db 0

; ---- saca un texto de aquí al programa: ESI=el texto (página 437) EBX=sus
; letras EDI=el búfer (ya como puntero de aquí; 0: solo se cuenta) ECX=su
; sitio (con el 0) DL=1: CR delante de cada LF -> EAX=letras (sin el 0) ----
w32_sacar:
        push ebx
        push ecx
        push esi
        push edi
        push ebp
        xor ebp, ebp                ; EBP = las que van
        dec ecx
        js .x
.c:
        test ebx, ebx
        jz .f
        cmp ebp, ecx
        jae .f
        lodsb
        dec ebx
        cmp al, 10
        jne .c1
        test dl, dl
        jz .c1
        lea eax, [ebp+2]            ; (los dos, o ninguno)
        cmp eax, ecx
        ja .f
        mov al, 13
        call .pon
        mov al, 10
.c1:
        test al, al
        jns .c2
        movzx eax, al
        mov al, [w32_437+eax-0x80]
.c2:
        call .pon
        jmp .c
.f:
        test edi, edi
        jz .x
        mov byte [edi], 0
.x:
        mov eax, ebp
        pop ebp
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret
.pon:
        test edi, edi
        jz .p1
        stosb
.p1:
        inc ebp
        ret

; =============================================================================
;  El COMBOBOX sencillo (CBS_DROPDOWNLIST): una caja con la línea elegida y una
;  flecha; un clic (o la flecha abajo con Alt... aquí, el espacio) abre un
;  menú emergente con las líneas. V_D0 = su bloque (las líneas y, detrás, el
;  desplegable), V_D1 = cuántas, V_D2 = la elegida (-1: ninguna)
; =============================================================================
cbo_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_LBUTTONDOWN
        je .abrir
        cmp eax, WM_LBUTTONDBLCLK
        je .abrir
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .car
        cmp eax, WM_SETFOCUS
        je .inv
        cmp eax, WM_KILLFOCUS
        je .inv
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        jne ven_defecto
        mov eax, [ebp+V_D0]
        call mem_soltar
        xor eax, eax
        ret
.crear:
        mov ecx, CBO_TAM
        call mem_pedir
        mov [ebp+V_D0], eax
        mov dword [ebp+V_D2], -1
        xor eax, eax
        ret
.inv:
        call ven_invalidar
        xor eax, eax
        ret
.car:
        cmp ecx, ' '
        jne ven_defecto
.abrir:
        mov ebx, [ebp+V_D0]
        test ebx, ebx
        jz .nada
        mov ecx, [ebp+V_D1]
        test ecx, ecx
        jz .nada
        lea edi, [ebx+CBO_MENU]     ; el desplegable: dw número, el texto y 0
        mov esi, ebx
        mov edx, 1
.a1:
        mov [edi], dx
        add edi, 2
        push esi
        call pon_cad
        pop esi
        mov byte [edi], 0
        inc edi
        add esi, CBO_TXT
        inc edx
        dec ecx
        jnz .a1
        mov word [edi], 0xFFFF
        lea esi, [ebx+CBO_MENU]
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        add ebx, [ebp+V_AL]
        xor ecx, ecx
        call ven_soltar             ; (el botón se soltará en el desplegable)
        call ctx_menu
.nada:
        xor eax, eax
        ret
.orden:
        dec ecx                     ; la línea elegida en el desplegable
        cmp ecx, [ebp+V_D1]
        jae .nada
.elige:
        mov [ebp+V_D2], ecx
        call ven_invalidar
        mov eax, 1                  ; CBN_SELCHANGE
        call avisar
        xor eax, eax
        ret
.tecla:
        mov eax, [ebp+V_D2]
        cmp ecx, TC_ABAJO
        je .t1
        cmp ecx, TC_ARRIBA
        jne ven_defecto
        dec eax
        js .nada
        jmp .t2
.t1:
        inc eax
        cmp eax, [ebp+V_D1]
        jae .nada
.t2:
        mov ecx, eax
        jmp .elige
.pintar:
        call dc_pintar
        call ven_cli
        mov esi, ecx                ; ancho
        mov edi, edx                ; alto
        COLOR C_BLANCO
        xor eax, eax
        xor ebx, ebx
        call dc_rect
        COLOR C_MARRON
        call dc_marco
        COLOR C_ARENA               ; el botón de la flecha
        lea eax, [esi-19]
        mov ebx, 1
        mov ecx, 18
        lea edx, [edi-2]
        call dc_rect
        COLOR C_MARRON
        lea eax, [esi-20]
        mov ecx, 1
        call dc_rect
        COLOR C_TINTA
        lea eax, [esi-14]           ; la flecha: un triángulo hacia abajo
        mov ebx, edi
        shr ebx, 1
        sub ebx, 2
        mov ecx, 7
.f1:
        mov edx, 1
        call dc_rect
        inc eax
        inc ebx
        sub ecx, 2
        jg .f1
        cmp ebp, [foco]             ; con el foco: la línea, resaltada
        jne .p1
        COLOR C_ANIL
        mov eax, 2
        mov ebx, 2
        lea ecx, [esi-23]
        lea edx, [edi-4]
        call dc_rect
        COLOR C_BLANCO
.p1:
        mov eax, [ebp+V_D2]
        cmp eax, [ebp+V_D1]
        jae .p2
        mov esi, [ebp+V_D0]
        test esi, esi
        jz .p2
        imul eax, CBO_TXT
        add esi, eax
        mov eax, 6
        lea ebx, [edi-16]
        sar ebx, 1
        call dc_texto
.p2:
        call dc_cerrar
        xor eax, eax
        ret

; =============================================================================
;  USER32: lo de los controles
; =============================================================================
; (interna) [EBP]=ventana, [EBP+4]=número -> EAX=esa hija suya (ZF=1: no está)
w32_hija:
        mov eax, [ebp]
        call w32_ven
        jz .x
        push ebp
        mov ecx, [ebp+4]
        mov ebp, eax
        call ven_hija_id
        pop ebp
        test eax, eax
.x:
        ret
; HWND GetDlgItem(ventana, número)
u_GetDlgItem:
        call w32_hija
        jmp w32_hwnd
; int GetDlgCtrlID(ventana)
u_GetDlgCtrlID:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov eax, [eax+V_ID]
        ret
; LRESULT SendDlgItemMessageA(ventana, número, mensaje, wParam, lParam)
u_SendDlgItemMessageA:
        call w32_hija
        jz u_cero
        mov ecx, [ebp+12]
        mov edx, [ebp+16]
        mov ebx, [ebp+8]
        mov ebp, eax
        mov eax, ebx
        jmp w32_proc
; BOOL SetDlgItemTextA(ventana, número, texto)
u_SetDlgItemTextA:
        call w32_hija
        jz u_cero
        mov edx, [ebp+8]
        mov ebp, eax
        mov eax, WM_SETTEXT
        xor ecx, ecx
        call w32_proc
        jmp u_uno
; UINT GetDlgItemTextA(ventana, número, búfer, sitio)
u_GetDlgItemTextA:
        call w32_hija
        jz u_cero
        mov edx, [ebp+8]
        mov ecx, [ebp+12]
        mov ebp, eax
        mov eax, WM_GETTEXT
        jmp w32_proc
; BOOL CheckDlgButton(ventana, número, marca) y UINT IsDlgButtonChecked
u_CheckDlgButton:
        call w32_hija
        jz u_cero
        mov ecx, [ebp+8]
        mov ebp, eax
        mov eax, 0x00F1
        xor edx, edx
        call w32_proc
        jmp u_uno
u_IsDlgButtonChecked:
        call w32_hija
        jz u_cero
        mov ebp, eax
        mov eax, 0x00F0
        xor ecx, ecx
        xor edx, edx
        jmp w32_proc
; BOOL CheckRadioButton(ventana, primero, último, el marcado)
u_CheckRadioButton:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, eax
        mov ebx, [ebp+4]
.b:
        cmp ebx, [ebp+8]
        jg u_uno
        push ebp
        mov ecx, ebx
        mov ebp, esi
        call ven_hija_id
        pop ebp
        test eax, eax
        jz .s
        xor ecx, ecx
        cmp ebx, [ebp+12]
        sete cl
        push ebp
        mov ebp, eax
        mov eax, 0x00F1
        xor edx, edx
        call w32_proc
        pop ebp
.s:
        inc ebx
        jmp .b
; BOOL SetDlgItemInt(ventana, número, valor, con signo)
u_SetDlgItemInt:
        call w32_hija
        jz u_cero
        mov esi, eax
        sub esp, 16
        mov edi, esp
        mov eax, [ebp+8]
        cmp dword [ebp+12], 0
        je .n
        test eax, eax
        jns .n
        mov byte [edi], '-'
        inc edi
        neg eax
.n:
        call pon_num
        mov byte [edi], 0
        mov ebp, esi
        mov eax, WM_SETTEXT
        xor ecx, ecx
        mov ebx, edx                ; (el texto está en esta pila: el programa
        mov edx, esp                ; lo ve con su puntero)
        sub edx, [ebx+PE_DELTA]
        call w32_proc
        add esp, 16
        jmp u_uno
; UINT GetDlgItemInt(ventana, número, BOOL *bien, con signo)
u_GetDlgItemInt:
        mov edi, [ebp+8]
        test edi, edi
        jz .s
        add edi, [edx+PE_DELTA]
        mov dword [edi], 0
.s:
        call w32_hija
        jz u_cero
        cmp dword [eax+V_PROC], w32_ctl_proc
        jne u_cero
        push ebp
        mov ebp, eax
        call w32_ctl_texto          ; ESI = su texto, EBX = sus letras
        pop ebp
        xor eax, eax                ; el número
        xor ecx, ecx                ; cifras
        xor edx, edx                ; 1: negativo
.e:
        test ebx, ebx
        jz .f
        cmp byte [esi], ' '
        jne .sig
        inc esi
        dec ebx
        jmp .e
.sig:
        cmp byte [esi], '-'
        jne .c
        cmp dword [ebp+12], 0
        je .f
        inc edx
        inc esi
        dec ebx
.c:
        test ebx, ebx
        jz .f
        push edx
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .cf
        imul eax, 10
        add eax, edx
        pop edx
        inc ecx
        inc esi
        dec ebx
        jmp .c
.cf:
        pop edx
.f:
        test ecx, ecx               ; sin cifras, o con algo detrás: mal
        jz u_cero
        test ebx, ebx
        jnz u_cero
        test edx, edx
        jz .p
        neg eax
.p:
        test edi, edi
        jz .x
        mov dword [edi], 1
.x:
        ret
; BOOL EnableWindow(ventana, sí o no) -> si estaba desactivada; BOOL
; IsWindowEnabled(ventana)
u_EnableWindow:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov ecx, [eax+V_ESTADO]
        and dword [eax+V_ESTADO], ~S_INACTIVA
        cmp dword [ebp+4], 0
        jne .e
        or dword [eax+V_ESTADO], S_INACTIVA
        cmp eax, [foco]             ; (no se queda con el foco)
        jne .e
        push eax
        push ecx
        mov ebp, eax
        xor al, al
        call ven_foco_sig
        pop ecx
        pop eax
.e:
        mov ebp, eax
        call ven_invalidar
        xor eax, eax
        test ecx, S_INACTIVA
        setnz al
        ret
u_IsWindowEnabled:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        test dword [eax+V_ESTADO], S_INACTIVA
        jnz u_cero
        jmp u_uno
; BOOL SetWindowPos(ventana, detrás de, x, y, ancho, alto, cómo): el sitio y
; el tamaño (SWP_NOSIZE 1, SWP_NOMOVE 2) y mostrar u ocultar (40h, 80h)
u_SetWindowPos:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        mov esi, eax                ; ESI = la ventana
        mov edi, [ebp+24]
        mov eax, edi
        and eax, 3
        cmp eax, 3
        je .ver
        mov eax, [esi+V_X]          ; donde está: una hija, dentro de su madre
        mov ebx, [esi+V_Y]
        mov ecx, [esi+V_PADRE]
        test ecx, ecx
        jz .a
        push ebp
        push eax
        push ebx
        mov ebp, ecx
        call ven_cli
        mov ecx, eax
        mov edx, ebx
        pop ebx
        pop eax
        pop ebp
        sub eax, ecx
        sub ebx, edx
.a:
        mov ecx, [esi+V_AN]
        mov edx, [esi+V_AL]
        test edi, 2
        jnz .b
        mov eax, [ebp+8]
        mov ebx, [ebp+12]
.b:
        test edi, 1
        jnz .c
        mov ecx, [ebp+16]
        mov edx, [ebp+20]
.c:
        push ebp
        mov ebp, esi
        call ven_mover
        pop ebp
.ver:
        test edi, 0xC0
        jz u_uno
        xor eax, eax
        test edi, 0x40
        setnz al
        mov ebp, esi
        call ven_mostrar
        jmp u_uno
