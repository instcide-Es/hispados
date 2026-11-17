; =============================================================================
;  Hispa Ventanas - [HISPA-486] los menús, los recursos (.rsrc) y los diálogos
;  de plantilla de los programas de Windows
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Fase 3c de Hispa-486 (docs/486/HISPA-486.md). Va detrás de win32c.inc, en
;  la parte alta de VENTANAS.COM. Sala limpia: hecho a partir de la
;  documentación pública del API Win32 y del formato PE (.rsrc, las plantillas
;  de menú y de diálogo).
;
;  LOS MENÚS. Un HMENU es una ficha del programa (MN_*: hasta MN_MAX menús de
;  hasta MI_MAX órdenes cada uno, con su texto ya en la página 437). SetMenu
;  hace con ellas la tabla que quiere la barra de menús de Hispa Ventanas
;  (menus.inc: V_MENU) en un bloque del montón, y la rehace cada vez que el
;  programa cambia algo (AppendMenuA, CheckMenuItem, EnableMenuItem). La
;  barra, los desplegables, Alt+letra y las flechas son los de Hispa Ventanas;
;  al elegir, la ventana recibe WM_COMMAND con el número de la orden. La marca
;  de CheckMenuItem es un signo delante del texto; las de gris se le dicen al
;  desplegable al abrirse (w32_menu_abierto, desde menu_abrir). En V_D7 de la
;  ventana, su HMENU.
;
;  LOS RECURSOS. w32_recurso busca en la sección .rsrc del programa (tipo,
;  nombre o número, y el primer idioma que haya). LoadStringA, LoadMenuA,
;  LoadIconA y LoadCursorA (solo se sabe si están), FindResourceA y los
;  diálogos salen de ahí; los textos, de UTF-16 a la página 1252.
;
;  LOS DIÁLOGOS. Un diálogo de plantilla es una ventana de Windows (de la
;  clase «#32770», la 0 de cada programa) con la marca W32V_DIALOGO: su
;  «WndProc» es el DLGPROC del programa y, si este devuelve 0, se hace aquí lo
;  que haría DefDlgProc (w32_dlg_defecto: el fondo, WM_CLOSE y Esc = IDCANCEL,
;  Intro = el botón por defecto). Sus controles salen de la plantilla (las
;  medidas, en unidades de diálogo: con la letra de 8×16, dos puntos cada
;  una). DialogBoxParamA se queda en un bucle de mensajes propio, con la dueña
;  desactivada, hasta EndDialog; CreateDialogParamA vuelve enseguida.
; =============================================================================
W32_HMENU   equ 0x3000              ; un HMENU = esto + su número
MF_GRIS     equ 3                   ; MF_GRAYED | MF_DISABLED
MF_MARCA    equ 8                   ; MF_CHECKED
MF_POPUP    equ 0x10
MF_RAYA     equ 0x800               ; MF_SEPARATOR
MF_POSICION equ 0x400               ; MF_BYPOSITION
W32V_DIALOGO equ 8                  ; (V_D5) es un diálogo: su WndProc es un DLGPROC
W32V_FIN    equ 16                  ;   EndDialog
VB_DLGRES   equ VB_EXTRA + 12       ; lo que dijo EndDialog (tras DWL_USER)
DLG_EXTRA   equ 16

; ---- EAX=HMENU EDX=programa -> ESI=su ficha; CF=1 si no vale ----
w32_menu:
        push eax
        sub eax, W32_HMENU
        jb .no
        cmp eax, MN_MAX
        jae .no
        imul eax, MN_TAM
        mov esi, [edx+PE_W32]
        lea esi, [esi+eax+WB_MENUS]
        cmp dword [esi+MN_USO], 0
        je .no
        pop eax
        clc
        ret
.no:
        pop eax
        stc
        ret
; HMENU CreateMenu() y CreatePopupMenu()
u_CreateMenu:
        mov esi, [edx+PE_W32]
        add esi, WB_MENUS
        xor eax, eax
.b:
        cmp dword [esi+MN_USO], 0
        je .hay
        add esi, MN_TAM
        inc eax
        cmp eax, MN_MAX
        jb .b
        jmp u_cero
.hay:
        mov dword [esi+MN_USO], 1
        mov dword [esi+MN_N], 0
        add eax, W32_HMENU
        ret
; (interna) una orden más para el menú ESI del programa EDX: EAX=banderas
; EBX=número (o el HMENU de su desplegable) ECX=su texto (del programa; 0:
; ninguno) -> CF=1 si no cabe
w32_menu_pon:
        push edi
        push ecx
        mov edi, [esi+MN_N]
        cmp edi, MI_MAX
        jae .no
        imul edi, MI_TAM
        lea edi, [esi+edi+MN_ITEMS]
        mov [edi+MI_FLAGS], eax
        mov [edi+MI_ID], ebx
        mov byte [edi+MI_TEXTO], 0
        test eax, MF_RAYA | 0x104   ; (ni las rayas ni las dibujadas llevan texto)
        jnz .hecho
        test ecx, ecx
        jz .hecho
        push esi
        push edi
        mov esi, ecx
        add edi, MI_TEXTO
        mov ecx, MI_TAM - MI_TEXTO - 1
        call w32_texto
        pop edi
        pop esi
        push edi                    ; (el tabulador del atajo: un espacio)
        add edi, MI_TEXTO
.t:
        cmp byte [edi], 0
        je .tf
        cmp byte [edi], 9
        jne .t1
        mov byte [edi], ' '
.t1:
        inc edi
        jmp .t
.tf:
        pop edi
.hecho:
        inc dword [esi+MN_N]
        pop ecx
        pop edi
        clc
        ret
.no:
        pop ecx
        pop edi
        stc
        ret
; BOOL AppendMenuA(menú, banderas, número o desplegable, texto)
u_AppendMenuA:
        mov eax, [ebp]
        call w32_menu
        jc u_cero
        mov eax, [ebp+4]
        mov ebx, [ebp+8]
        mov ecx, [ebp+12]
        call w32_menu_pon
        jc u_cero
        call w32_menus_rehacer
        jmp u_uno
; (interna) EAX=HMENU EDX=programa: lo suelta, con sus desplegables
w32_menu_soltar:
        pushad
        call w32_menu
        jc .x
        mov dword [esi+MN_USO], 0
        mov ecx, [esi+MN_N]
        lea ebx, [esi+MN_ITEMS]
.b:
        test ecx, ecx
        jz .x
        test dword [ebx+MI_FLAGS], MF_POPUP
        jz .s
        mov eax, [ebx+MI_ID]
        call w32_menu_soltar
.s:
        add ebx, MI_TAM
        dec ecx
        jmp .b
.x:
        popad
        ret
; BOOL DestroyMenu(menú)
u_DestroyMenu:
        mov eax, [ebp]
        call w32_menu
        jc u_cero
        call w32_menu_soltar
        jmp u_uno
; (interna) busca la orden: EAX=menú EBX=número o posición ECX=banderas
; (MF_BYPOSITION o por número: entonces, también en sus desplegables)
; EDX=programa -> EDI=la orden; CF=1 si no está
w32_menu_orden:
        push eax
        push ecx
        push esi
        call w32_menu
        jc .no
        test ecx, MF_POSICION
        jz .num
        cmp ebx, [esi+MN_N]
        jae .no
        imul edi, ebx, MI_TAM
        lea edi, [esi+edi+MN_ITEMS]
        jmp .si
.num:
        mov ecx, [esi+MN_N]
        lea edi, [esi+MN_ITEMS]
.b:
        test ecx, ecx
        jz .no
        test dword [edi+MI_FLAGS], MF_POPUP
        jnz .sub
        cmp [edi+MI_ID], ebx
        je .si
.s:
        add edi, MI_TAM
        dec ecx
        jmp .b
.sub:
        push edi
        push ecx
        mov eax, [edi+MI_ID]
        xor ecx, ecx
        call w32_menu_orden
        pop ecx
        jc .sub2
        add esp, 4
        jmp .si
.sub2:
        pop edi
        jmp .s
.si:
        pop esi
        pop ecx
        pop eax
        clc
        ret
.no:
        pop esi
        pop ecx
        pop eax
        stc
        ret
; DWORD CheckMenuItem(menú, orden, MF_CHECKED o no) -> cómo estaba (-1: no está)
u_CheckMenuItem:
        mov esi, MF_MARCA
        jmp u_menu_cambia
; BOOL EnableMenuItem(menú, orden, MF_GRAYED o no) -> cómo estaba
u_EnableMenuItem:
        mov esi, MF_GRIS
u_menu_cambia:
        mov eax, [ebp]
        mov ebx, [ebp+4]
        mov ecx, [ebp+8]
        call w32_menu_orden
        jc .no
        mov eax, [edi+MI_FLAGS]
        and ecx, esi
        not esi
        and [edi+MI_FLAGS], esi
        or [edi+MI_FLAGS], ecx
        not esi
        and eax, esi
        push eax
        call w32_menus_rehacer
        pop eax
        ret
.no:
        mov eax, -1
        ret
; UINT GetMenuState(menú, orden, por número o por posición)
u_GetMenuState:
        mov eax, [ebp]
        mov ebx, [ebp+4]
        mov ecx, [ebp+8]
        call w32_menu_orden
        jc u_menu_cambia.no
        mov eax, [edi+MI_FLAGS]
        and eax, MF_GRIS | MF_MARCA | MF_POPUP | MF_RAYA
        ret
; int GetMenuItemCount(menú)
u_GetMenuItemCount:
        mov eax, [ebp]
        call w32_menu
        jc u_menu_cambia.no
        mov eax, [esi+MN_N]
        ret
; HMENU GetSubMenu(menú, posición)
u_GetSubMenu:
        mov eax, [ebp]
        mov ebx, [ebp+4]
        mov ecx, MF_POSICION
        call w32_menu_orden
        jc u_cero
        test dword [edi+MI_FLAGS], MF_POPUP
        jz u_cero
        mov eax, [edi+MI_ID]
        ret
; HMENU GetMenu(ventana)
u_GetMenu:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        cmp dword [eax+V_PROC], w32_ctl_proc
        je u_cero
        mov eax, [eax+V_D7]
        ret
; BOOL SetMenu(ventana, menú o NULL) y BOOL DrawMenuBar(ventana)
u_SetMenu:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        cmp dword [eax+V_PROC], w32_ctl_proc
        je u_cero
        mov ebx, eax
        mov eax, [ebp+4]
        test eax, eax
        jz .pon
        call w32_menu
        jc u_cero
.pon:
        mov ebp, ebx
        mov [ebp+V_D7], eax
        call w32_menu_hacer
        jmp u_uno
u_DrawMenuBar:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        cmp dword [eax+V_PROC], w32_ctl_proc
        je u_cero
        mov ebp, eax
        call w32_menu_hacer
        jmp u_uno

; ---- (interna) rehace la barra de las ventanas del programa EDX que la
; tienen (ha cambiado un menú) ----
w32_menus_rehacer:
        pushad
        mov ebp, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [ebp+V_USO], 1
        jne .s
        cmp dword [ebp+V_PROC], w32_ven_proc
        jne .s
        cmp [ebp+V_D0], edx
        jne .s
        cmp dword [ebp+V_D7], 0
        je .s
        call w32_menu_hacer
.s:
        add ebp, V_TAM
        dec ecx
        jnz .b
        popad
        ret

; ---- (interna) hace la barra de menús de la ventana EBP (de Windows) del
; programa EDX con su HMENU (V_D7; 0: se queda sin barra). Si gana o pierde
; la barra, sus hijas se quedan donde estaban dentro de su zona ----
w32_menu_hacer:
        pushad
        cmp ebp, [menu_duena]       ; (con un menú suyo abierto: se cierra)
        jne .c
        call menu_cerrar
.c:
        xor edi, edi                ; EDI = la tabla nueva (0: ninguna)
        mov eax, [ebp+V_D7]
        test eax, eax
        jz .cambia
        call w32_menu
        jc .cambia
        ; lo que puede ocupar: 36 por orden (de la barra y de los desplegables)
        mov ecx, [esi+MN_N]
        mov ebx, ecx
        lea edi, [esi+MN_ITEMS]
.m:
        test ecx, ecx
        jz .mf
        test dword [edi+MI_FLAGS], MF_POPUP
        jz .ms
        push esi
        mov eax, [edi+MI_ID]
        call w32_menu
        jc .m1
        add ebx, [esi+MN_N]
.m1:
        pop esi
.ms:
        add edi, MI_TAM
        dec ecx
        jmp .m
.mf:
        imul ecx, ebx, 36
        add ecx, 4
        call w32_pedir
        mov edi, eax
        jc .cambia
        push edi
        push edx
        call w32_menu_tabla
        pop edx
        pop edi
.cambia:
        mov eax, [ebp+V_MENU]
        mov [ebp+V_MENU], edi
        test eax, eax
        setnz bl
        call mem_soltar
        test edi, edi
        setnz bh
        or dword [ebp+V_ESTADO], S_NCSUCIO
        mov byte [actividad], 1
        cmp bl, bh
        je .x
        ; gana (o pierde) la barra: las hijas, MENU_H más abajo (o más arriba)
        mov eax, MENU_H
        test bh, bh
        jnz .h
        neg eax
.h:
        mov esi, [ven_tabla]
        mov ecx, MAX_VEN
.hb:
        cmp dword [esi+V_USO], 1
        jne .hs
        mov ebx, [esi+V_PADRE]
.hp:
        test ebx, ebx
        jz .hs
        cmp ebx, ebp
        je .hm
        mov ebx, [ebx+V_PADRE]
        jmp .hp
.hm:
        add [esi+V_Y], eax
.hs:
        add esi, V_TAM
        dec ecx
        jnz .hb
        call ven_invalidar
        test byte [ebp+V_D5], W32V_MOSTRADA ; y su zona mide otra cosa
        jz .x
        mov esi, edx
        mov eax, WM_SIZE
        call w32_traducir
        call w32_proc
.x:
        popad
        ret
; (de w32_menu_hacer) escribe en EDI la tabla de la barra del menú ESI.
; Cambia todos los registros menos EBP
w32_menu_tabla:
        mov ecx, [esi+MN_N]
        lea ebx, [esi+MN_ITEMS]
.t:
        test ecx, ecx
        jz .fin
        lea esi, [ebx+MI_TEXTO]     ; el título (vacío no puede ser)
        cmp byte [esi], 0
        jne .t1
        mov al, ' '
        stosb
.t1:
        call pon_cad
        xor al, al
        stosb
        test dword [ebx+MI_FLAGS], MF_POPUP
        jnz .desp
        mov eax, [ebx+MI_ID]        ; una orden en la barra: ella sola debajo
        stosw
        lea esi, [ebx+MI_TEXTO]
        call pon_cad
        xor al, al
        stosb
        jmp .cierra
.desp:
        mov eax, [ebx+MI_ID]
        mov edx, [ebp+V_D0]
        call w32_menu
        jc .cierra
        push ebx
        push ecx
        mov ecx, [esi+MN_N]
        lea ebx, [esi+MN_ITEMS]
        xor ah, ah                  ; AH = 1 si alguna lleva marca
        push ecx
        push ebx
.d0:
        test ecx, ecx
        jz .d1
        test dword [ebx+MI_FLAGS], MF_MARCA
        jz .d0s
        mov ah, 1
.d0s:
        add ebx, MI_TAM
        dec ecx
        jmp .d0
.d1:
        pop ebx
        pop ecx
.d:
        test ecx, ecx
        jz .df
        mov edx, [ebx+MI_ID]
        test dword [ebx+MI_FLAGS], MF_POPUP ; (un desplegable dentro de otro: no)
        jz .d2
        mov edx, 0xFFF0
.d2:
        test dword [ebx+MI_FLAGS], MF_RAYA
        jnz .raya
        cmp byte [ebx+MI_TEXTO], 0
        je .raya
        test dx, dx
        jz .raya
        mov [edi], dx
        add edi, 2
        test ah, ah
        jz .d3
        mov al, ' '
        test dword [ebx+MI_FLAGS], MF_MARCA
        jz .d2b
        mov al, '*'                 ; el signo de la marca
.d2b:
        stosb
        mov al, ' '
        stosb
.d3:
        lea esi, [ebx+MI_TEXTO]
        call pon_cad
        xor al, al
        stosb
        jmp .ds
.raya:
        mov word [edi], 0
        mov byte [edi+2], 0
        add edi, 3
.ds:
        add ebx, MI_TAM
        dec ecx
        jmp .d
.df:
        pop ecx
        pop ebx
.cierra:
        mov ax, 0xFFFF
        stosw
        add ebx, MI_TAM
        dec ecx
        jmp .t
.fin:
        xor al, al
        stosb
        ret

; ---- (menu_abrir) se ha abierto el desplegable [menu_idx] de la barra de la
; ventana EBP: si es de un programa de Windows, cuáles van en gris ----
w32_menu_abierto:
        pushad
        cmp dword [ebp+V_PROC], w32_ven_proc
        jne .x
        mov eax, [ebp+V_D7]
        test eax, eax
        jz .x
        mov edx, [ebp+V_D0]
        cmp dword [edx+PE_USO], 1
        jne .x
        mov ebx, [menu_idx]
        mov ecx, MF_POSICION
        call w32_menu_orden
        jc .x
        xor ebx, ebx                ; EBX = las de gris (un bit cada una)
        test dword [edi+MI_FLAGS], MF_POPUP
        jnz .desp
        test dword [edi+MI_FLAGS], MF_GRIS
        setnz bl
        jmp .pon
.desp:
        mov eax, [edi+MI_ID]
        call w32_menu
        jc .x
        mov ecx, [esi+MN_N]
        lea edi, [esi+MN_ITEMS]
        xor eax, eax
.b:
        cmp eax, ecx
        jae .pon
        test dword [edi+MI_FLAGS], MF_GRIS
        jz .s
        bts ebx, eax
.s:
        add edi, MI_TAM
        inc eax
        jmp .b
.pon:
        mov [ctx_gris], ebx
        mov byte [menu_gris], 1
.x:
        popad
        ret

; =============================================================================
;  Los recursos (.rsrc)
; =============================================================================
; ---- busca un recurso del programa EDX: EAX=tipo (un número) EBX=nombre (un
; número, o un texto del programa) -> ESI=el recurso ECX=sus octetos (y en
; w32_rec_ent, su ficha en la tabla); CF=1 si no está ----
w32_recurso:
        push eax
        push ebx
        push edi
        push ebp
        mov edi, [edx+PE_IMG]
        mov ecx, [edi+0x3C]
        cmp dword [edi+ecx+0x74], 3 ; (¿llega el directorio a los recursos?)
        jb .no
        mov ecx, [edi+ecx+0x88]
        test ecx, ecx
        jz .no
        cmp ecx, [edx+PE_IMGTAM]
        jae .no
        lea ebp, [edi+ecx]          ; EBP = la raíz
        mov esi, ebp                ; 1.º el tipo
        call .num
        jc .no
        test eax, eax
        jns .no
        and eax, 0x7FFFFFFF
        lea esi, [ebp+eax]
        mov eax, ebx                ; 2.º el nombre
        cmp eax, 0x10000
        jae .n2
        call .num
        jmp .n3
.n2:
        call .nom
.n3:
        jc .no
        test eax, eax
        jns .dato
        and eax, 0x7FFFFFFF
        lea esi, [ebp+eax]
        movzx ecx, word [esi+12]    ; 3.º el idioma: el primero
        add cx, [esi+14]
        jz .no
        mov eax, [esi+20]
        test eax, eax
        js .no
.dato:
        lea esi, [ebp+eax]
        mov [w32_rec_ent], esi
        mov ecx, [esi+4]
        mov eax, [esi]
        call pe_rva
        jc .no
        mov esi, eax
        pop ebp
        pop edi
        pop ebx
        pop eax
        clc
        ret
.no:
        pop ebp
        pop edi
        pop ebx
        pop eax
        stc
        ret
; (de w32_recurso) en el directorio ESI, la entrada de número EAX -> EAX=lo
; que dice (el bit 31: otro directorio); CF=1 si no está
.num:
        push ecx
        push edi
        movzx ecx, word [esi+12]
        add cx, [esi+14]
        lea edi, [esi+16]
.nb:
        test ecx, ecx
        jz .nno
        cmp [edi], eax
        je .nsi
        add edi, 8
        dec ecx
        jmp .nb
.nsi:
        mov eax, [edi+4]
        pop edi
        pop ecx
        clc
        ret
.nno:
        pop edi
        pop ecx
        stc
        ret
; (de w32_recurso) lo mismo, por nombre: EAX=texto (del programa); en el
; directorio están en UTF-16 con su largo delante. Sin mirar mayúsculas
.nom:
        push ebx
        push ecx
        push edx
        push edi
        add eax, [edx+PE_DELTA]
        mov ebx, eax                ; EBX = el nombre que se busca
        movzx ecx, word [esi+12]
        lea edi, [esi+16]
.mb:
        test ecx, ecx
        jz .mno
        mov eax, [edi]
        test eax, eax
        jns .ms
        and eax, 0x7FFFFFFF
        add eax, ebp                ; su largo y sus letras
        push ecx
        push esi
        movzx ecx, word [eax]
        lea esi, [eax+2]
        xor edx, edx
.mc:
        cmp edx, ecx
        jae .mf
        mov al, [ebx+edx]
        mov ah, [esi+edx*2]
        cmp al, 'a'
        jb .m1
        cmp al, 'z'
        ja .m1
        sub al, 32
.m1:
        cmp ah, 'a'
        jb .m2
        cmp ah, 'z'
        ja .m2
        sub ah, 32
.m2:
        cmp al, ah
        jne .mdif
        inc edx
        jmp .mc
.mf:
        cmp byte [ebx+edx], 0       ; (y que no siga)
.mdif:
        pop esi
        pop ecx
        je .msi
.ms:
        add edi, 8
        dec ecx
        jmp .mb
.msi:
        mov eax, [edi+4]
        pop edi
        pop edx
        pop ecx
        pop ebx
        clc
        ret
.mno:
        pop edi
        pop edx
        pop ecx
        pop ebx
        stc
        ret

; ---- (interna) copia a EDI el texto UTF-16 de ESI (acabado en 0), en la
; página 1252 y con su 0; ECX letras como mucho. ESI queda tras su final ----
w32_u16:
        push eax
.c:
        lodsw
        test ax, ax
        jz .f
        test ecx, ecx
        jz .c
        test ah, ah
        jz .p
        mov al, '?'
.p:
        stosb
        dec ecx
        jmp .c
.f:
        mov byte [edi], 0
        pop eax
        ret
; ---- (interna) salta en ESI un «texto o número» de una plantilla: 0 (nada),
; FFFFh y un número, o un texto UTF-16 ----
w32_sz_saltar:
        cmp word [esi], 0xFFFF
        jne .t
        add esi, 4
        ret
.t:
        add esi, 2
        cmp word [esi-2], 0
        jne .t
        ret

; int LoadStringA(módulo, número, búfer, sitio): los textos van de 16 en 16
u_LoadStringA:
        mov eax, 6                  ; RT_STRING
        mov ebx, [ebp+4]
        shr ebx, 4
        inc ebx
        call w32_recurso
        jc u_cero
        mov ecx, [ebp+4]
        and ecx, 15
.s:
        test ecx, ecx
        jz .es
        movzx eax, word [esi]
        lea esi, [esi+eax*2+2]
        dec ecx
        jmp .s
.es:
        movzx ecx, word [esi]       ; ECX = sus letras
        add esi, 2
        mov edi, [ebp+8]
        add edi, [edx+PE_DELTA]
        mov ebx, [ebp+12]
        dec ebx
        js u_cero
        xor eax, eax
.c:
        cmp eax, ecx
        jae .f
        cmp eax, ebx
        jae .f
        mov dx, [esi+eax*2]
        test dh, dh
        jz .c1
        mov dl, '?'
.c1:
        mov [edi+eax], dl
        inc eax
        jmp .c
.f:
        mov byte [edi+eax], 0
        ret
; HICON LoadIconA(módulo, cuál) y HCURSOR LoadCursorA(módulo, cuál): sin
; módulo, los de serie; con él, los del programa (si están). El icono y el
; cursor que se ven son los de Hispa Ventanas
u_LoadIconA:
        mov eax, 14                 ; RT_GROUP_ICON
        mov edi, 0x40000
        jmp u_carga
u_LoadCursorA:
        mov eax, 12                 ; RT_GROUP_CURSOR
        mov edi, 0x30000
u_carga:
        mov ebx, [ebp+4]
        cmp dword [ebp], 0
        je .serie
        call w32_recurso
        jc u_cero
.serie:
        movzx eax, bx
        or eax, edi
        ret
; HMENU LoadMenuA(módulo, nombre)
u_LoadMenuA:
        mov ebx, [ebp+4]
; (interna) EBX=nombre de un menú de los recursos EDX=programa -> EAX=su
; HMENU (0: no está, o no caben más)
w32_menu_recurso:
        mov eax, 4                  ; RT_MENU
        call w32_recurso
        jc u_cero
        movzx eax, word [esi+2]     ; (la cabecera: versión y lo que la sigue)
        lea esi, [esi+eax+4]
; (interna) lee un menú de la plantilla ESI (sus órdenes, hasta la que lleva
; MF_END) -> EAX=su HMENU (0: no caben más); ESI queda detrás
w32_menu_leer:
        push esi
        call u_CreateMenu
        pop esi
        push eax                    ; [ESP] = el menú
.i:
        movzx ebx, word [esi]       ; las banderas
        add esi, 2
        xor ecx, ecx
        test ebx, MF_POPUP
        jnz .txt
        movzx ecx, word [esi]       ; su número
        add esi, 2
.txt:
        sub esp, 64                 ; su texto, aquí (el menú queda en [ESP+64])
        mov edi, esp
        push ecx
        mov ecx, 63
        call w32_u16
        pop ecx
        test ebx, MF_POPUP
        jz .pon
        push ebx
        call w32_menu_leer          ; su desplegable
        pop ebx
        mov ecx, eax
        test eax, eax
        jnz .pon
        and ebx, ~MF_POPUP
.pon:
        mov eax, [esp+64]
        test eax, eax
        jz .sig
        push esi
        call w32_menu
        mov eax, ebx
        and eax, ~0x80
        test eax, MF_POPUP
        jnz .p1
        cmp byte [esp+4], 0         ; sin texto: una raya
        jne .p1
        or eax, MF_RAYA
.p1:
        push ebx
        mov ebx, ecx
        lea ecx, [esp+8]            ; (el texto está en esta pila: se da como
        sub ecx, [edx+PE_DELTA]     ; puntero del programa)
        call w32_menu_pon
        pop ebx
        pop esi
.sig:
        add esp, 64
        test ebx, 0x80              ; MF_END: la última
        jz .i
        pop eax
        ret

; ---- KERNEL32 (se hacen aquí en vez de en HW32): los recursos a mano ----
; HRSRC FindResourceA(módulo, nombre, tipo) -> su ficha en la tabla
k_FindResourceA:
        mov eax, [ebp+8]
        cmp eax, 0x10000
        jae u_cero
        mov ebx, [ebp+4]
        call w32_recurso
        jc u_cero
        mov eax, [w32_rec_ent]
        sub eax, [edx+PE_DELTA]
        ret
; HGLOBAL LoadResource(módulo, HRSRC) -> donde está; LPVOID LockResource(eso)
k_LoadResource:
        mov eax, [ebp+4]
        test eax, eax
        jz u_cero
        add eax, [edx+PE_DELTA]
        mov eax, [eax]
        add eax, [edx+PE_BASE]
        ret
k_LockResource:
        mov eax, [ebp]
        ret
; DWORD SizeofResource(módulo, HRSRC)
k_SizeofResource:
        mov eax, [ebp+4]
        test eax, eax
        jz u_cero
        add eax, [edx+PE_DELTA]
        mov eax, [eax+4]
        ret

; =============================================================================
;  Los diálogos de plantilla
; =============================================================================
t_dlg_clase db "#32770", 0
; ---- (w32_iniciar) la clase de los diálogos del programa EDX: la 0 ----
w32_dlg_clase:
        pushad
        mov edi, [edx+PE_W32]
        mov dword [edi+WC_VCLASE+C_PROC], w32_ven_proc
        mov dword [edi+WC_VCLASE+C_FONDO], SIN_FONDO | (CB_FOCO << 8)
        mov dword [edi+WC_USO], 1
        mov dword [edi+WC_EXTRA], DLG_EXTRA
        mov esi, t_dlg_clase
        add edi, WC_NOMBRE
        call pon_cad
        mov byte [edi], 0
        popad
        ret

; ---- (w32_proc) el DLGPROC del diálogo EBP ha devuelto 0 al mensaje EAX
; (ECX, EDX): lo que hace Windows por él -> EAX. Conserva lo demás ----
w32_dlg_defecto:
        cmp eax, WM_CLOSE
        je .cancelar
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, 0x0014             ; WM_ERASEBKGND
        je .borrar
        cmp eax, WM_PAINT
        je .pintar
.cero:
        xor eax, eax
        ret
.tecla:
        cmp ecx, 0x1B               ; Esc: cancelar
        je .cancelar
        cmp ecx, 0x0D               ; Intro: el botón por defecto (o IDOK)
        jne .cero
        push ecx
        push edx
        push esi
        mov ecx, 1
        mov esi, [ven_tabla]
        mov edx, MAX_VEN
.b:
        cmp dword [esi+V_USO], 1
        jne .s
        cmp [esi+V_PADRE], ebp
        jne .s
        cmp dword [esi+V_CLASE], c_boton
        jne .s
        test dword [esi+V_ESTILO], BS_DEFECTO
        jz .s
        test dword [esi+V_ESTADO], S_INACTIVA
        jnz .s
        mov ecx, [esi+V_ID]
        jmp .orden
.s:
        add esi, V_TAM
        dec edx
        jnz .b
        jmp .orden
.cancelar:
        push ecx
        push edx
        push esi
        mov ecx, 2
.orden:
        mov eax, WM_COMMAND
        xor edx, edx
        call w32_proc
        pop esi
        pop edx
        pop ecx
        xor eax, eax
        ret
.borrar:
        pushad
        mov eax, ecx
        mov edx, [ebp+V_D0]
        call w32_dc_abrir
        jc .b2
        COLOR C_CREMA
        call ven_cli
        xor eax, eax
        xor ebx, ebx
        call dc_rect
        call dc_cerrar
.b2:
        popad
        mov eax, 1
        ret
.pintar:
        pushad
        mov edx, [ebp+V_D0]
        call w32_empezar_pintar
        jc .p2
        mov dword [edi+DC_USO], 0
.p2:
        popad
        xor eax, eax
        ret

; ---- (interna) hace el diálogo: EBP=los argumentos (módulo, plantilla,
; dueña, DLGPROC, parámetro) EDX=programa -> EAX=el diálogo (la ventana; 0:
; no se puede), ya con sus controles, WM_INITDIALOG enviado y a la vista ----
w32_dlg_crear:
        mov eax, 5                  ; RT_DIALOG
        mov ebx, [ebp+4]
        call w32_recurso
        jc u_cero
        cmp dword [esi], 0xFFFF0001 ; (las plantillas extendidas: no)
        je u_cero
        mov [w32d_tpl], esi
        mov [w32d_arg], ebp
        mov ecx, VB_EXTRA + DLG_EXTRA
        call w32_pedir
        jc u_cero
        mov ebx, eax                ; EBX = su bloque
        add esi, 18                 ; el menú y la clase (no se miran) y el título
        call w32_sz_saltar
        call w32_sz_saltar
        sub esp, 256
        mov edi, esp
        mov ecx, 255
        call w32_u16
        mov eax, [w32d_tpl]
        test byte [eax], 0x40       ; DS_SETFONT: el tamaño y el nombre de la letra
        jz .sl
        add esi, 2
        call w32_sz_saltar
.sl:
        mov [w32d_sig], esi
        mov esi, esp                ; (de la 1252 a la 437)
        sub esi, [edx+PE_DELTA]
        mov edi, ebx
        mov ecx, 255
        call w32_texto
        add esp, 256
        mov edi, [edx+PE_W32]       ; EDI = la clase de los diálogos
        mov [w32_crea], edx
        mov eax, [ebp+12]
        mov [w32_crea+4], eax
        mov [w32_crea+8], edi
        mov esi, [w32d_tpl]
        mov eax, [esi]
        mov [w32_crea+12], eax
        mov [w32_crea+16], ebx
        mov dword [w32_crea+20], W32V_DIALOGO
        xor eax, eax
        mov [w32_crea+24], eax
        mov [w32_crea+28], eax
        sub esp, P_TAM
        mov [esp+P_CLASE], edi
        mov [esp+P_TEXTO], ebx
        mov [esp+P_ID], eax
        mov dword [esp+P_ESTILO], E_TITULO | E_CERRAR | E_OCULTA
        movsx eax, word [esi+14]    ; el ancho y el alto de su zona, y el marco
        lea eax, [eax*2+2*BORDE]
        mov [esp+P_AN], eax
        mov ecx, [vid_ancho]
        sub ecx, eax
        sar ecx, 1
        mov [esp+P_X], ecx
        movsx eax, word [esi+16]
        lea eax, [eax*2+2*BORDE+TIT_H+1]
        mov [esp+P_AL], eax
        mov ecx, [vid_alto]
        sub ecx, eax
        sar ecx, 1
        cmp ecx, BARRA_H
        jge .y
        mov ecx, BARRA_H
.y:
        mov [esp+P_Y], ecx
        mov esi, esp
        push ebp
        xor ebp, ebp
        call ven_crear
        pop ebp
        add esp, P_TAM
        test eax, eax
        jnz .hecha
        mov eax, ebx
        call mem_soltar
        jmp u_cero
.hecha:
        mov edi, eax                ; EDI = el diálogo
        ; sus controles
        mov esi, [w32d_tpl]
        movzx ecx, word [esi+8]
        mov esi, [w32d_sig]
.ctl:
        test ecx, ecx
        jz .listos
        add esi, 3                  ; (cada uno, alineado a 4)
        and esi, ~3
        mov ebx, [esi]              ; EBX = su estilo
        movsx eax, word [esi+8]
        add eax, eax
        mov [w32c_p+P_X], eax
        movsx eax, word [esi+10]
        add eax, eax
        mov [w32c_p+P_Y], eax
        movsx eax, word [esi+12]
        add eax, eax
        mov [w32c_p+P_AN], eax
        movsx eax, word [esi+14]
        add eax, eax
        mov [w32c_p+P_AL], eax
        movsx eax, word [esi+16]
        mov [w32c_p+P_ID], eax
        add esi, 18
        sub esp, 256
        mov dword [w32d_tipo], -1
        cmp word [esi], 0xFFFF      ; su clase: por número...
        jne .cnom
        mov al, [esi+2]
        add esi, 4
        xor ebp, ebp
.ca:
        cmp al, [w32c_atomos+ebp]
        je .csi
        inc ebp
        cmp ebp, 5
        jb .ca
        jmp .ctit
.cnom:                              ; ...o por nombre
        push edi
        push ecx
        lea edi, [esp+8]
        mov ecx, 60
        call w32_u16
        push esi
        lea esi, [esp+12]
        mov edi, w32c_nombres
        xor ebp, ebp
.cn:
        cmp byte [edi], 0
        je .cnf
        call w32_cmp_i
        je .cnsi
.cn2:
        inc edi
        cmp byte [edi-1], 0
        jne .cn2
        inc ebp
        jmp .cn
.cnsi:
        mov [w32d_tipo], ebp
.cnf:
        pop esi
        pop ecx
        pop edi
        jmp .ctit
.csi:
        mov [w32d_tipo], ebp
.ctit:
        mov byte [esp], 0           ; su texto (o el número de un recurso: nada)
        cmp word [esi], 0xFFFF
        jne .ct
        add esi, 4
        jmp .cex
.ct:
        push edi
        push ecx
        lea edi, [esp+8]
        mov ecx, 255
        call w32_u16
        pop ecx
        pop edi
.cex:
        movzx eax, word [esi]       ; y lo que traiga de más
        lea esi, [esi+eax+2]
        mov eax, [w32d_tipo]
        test eax, eax
        js .csig
        push esi
        mov esi, esp
        add esi, 4
        sub esi, [edx+PE_DELTA]
        call w32c_hacer
        pop esi
.csig:
        add esp, 256
        dec ecx
        jmp .ctl
.listos:
        ; WM_INITDIALOG: wParam = el primer control donde para el tabulador
        mov ebp, edi
        mov esi, edx                ; ESI = el programa
        call w32_dlg_primero        ; EBX = ese control (0: ninguno)
        mov eax, ebx
        call w32_hwnd
        mov ecx, eax
        mov edx, [w32d_arg]
        mov edx, [edx+16]
        mov eax, 0x0110
        call w32_proc
        call w32_viva
        jne u_cero
        test byte [ebp+V_D5], W32V_FIN  ; (EndDialog ya en WM_INITDIALOG)
        jnz .x
        push eax
        mov eax, 1
        call ven_mostrar
        pop eax
        call w32_viva
        jne u_cero
        test eax, eax               ; si ha dicho que sí, el foco a ese control
        jz .x
        call w32_dlg_primero
        test ebx, ebx
        jz .x
        push ebp
        mov ebp, ebx
        call ven_enfocar
        pop ebp
.x:
        mov eax, ebp
        ret
; (interna) EBP=diálogo -> EBX=su primer control donde para el tabulador (0)
w32_dlg_primero:
        push ecx
        mov ebx, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [ebx+V_USO], 1
        jne .s
        cmp [ebx+V_PADRE], ebp
        jne .s
        test dword [ebx+V_ESTILO], E_TABULA
        jz .s
        test dword [ebx+V_ESTADO], S_INACTIVA
        jz .x
.s:
        add ebx, V_TAM
        dec ecx
        jnz .b
        xor ebx, ebx
.x:
        pop ecx
        ret

; HWND CreateDialogParamA(módulo, plantilla, dueña, DLGPROC, parámetro)
u_CreateDialogParamA:
        call w32_dlg_crear
        jmp w32_hwnd
; int DialogBoxParamA(módulo, plantilla, dueña, DLGPROC, parámetro): hasta
; EndDialog, en un bucle de mensajes propio y con la dueña desactivada
u_DialogBoxParamA:
        mov eax, [ebp+8]
        call w32_ven
        push eax                    ; [ESP] = la dueña (0: ninguna)
        call w32_dlg_crear
        test eax, eax
        jnz .hay
        pop eax
        mov eax, -1
        ret
.hay:
        mov ebp, eax                ; EBP = el diálogo, ESI = el programa
        mov ebx, [esp]
        test ebx, ebx
        jz .sd
        or dword [ebx+V_ESTADO], S_INACTIVA
.sd:
        push esi                    ; [ESP] = el programa (y [ESP+4], la dueña)
        sub esp, M_TAM
        mov edi, esp
.b:
        mov esi, [esp+M_TAM]
        call w32_viva
        jne .fuera
        test byte [ebp+V_D5], W32V_FIN
        jnz .fin
        call msg_tomar
        test eax, eax
        jnz .d
        call msg_salir              ; WM_QUIT: es para el bucle de fuera
        jmp .fin
.d:
        call msg_despachar
        jmp .b
.fin:
        mov ebx, [ebp+V_D4]
        mov ebx, [ebx+VB_DLGRES]
        add esp, M_TAM + 4
        pop edi                     ; la dueña, otra vez en marcha
        test edi, edi
        jz .f1
        and dword [edi+V_ESTADO], ~S_INACTIVA
.f1:
        call ven_destruir
        test edi, edi
        jz .f2
        cmp dword [edi+V_USO], 1
        jne .f2
        mov ebp, edi
        call ven_activar
.f2:
        mov eax, ebx
        ret
.fuera:                             ; (lo han destruido desde dentro)
        add esp, M_TAM + 4
        pop edi
        test edi, edi
        jz .f3
        and dword [edi+V_ESTADO], ~S_INACTIVA
.f3:
        mov eax, -1
        ret
; BOOL EndDialog(diálogo, lo que devuelve DialogBoxParamA)
u_EndDialog:
        mov eax, [ebp]
        call w32_ven
        jz u_cero
        cmp dword [eax+V_PROC], w32_ctl_proc
        je u_cero
        test byte [eax+V_D5], W32V_DIALOGO
        jz u_cero
        mov ecx, [ebp+4]
        mov ebx, [eax+V_D4]
        mov [ebx+VB_DLGRES], ecx
        or byte [eax+V_D5], W32V_FIN
        mov byte [actividad], 1
        jmp u_uno
