; =============================================================================
;  Hispa Ventanas - gestor de ventanas (el USER propio)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Lleva las fichas de las ventanas (defs.inc), el orden de las de primer
;  nivel (cuál tapa a cuál), la activa, el foco del teclado, la captura del
;  ratón, la zona no cliente (marco, barra de título con sus botones, barra de
;  menús) y el reparto del ratón y del teclado.
;
;  Cómo se pinta: nadie dibuja en la pantalla. Quien cambia algo marca la zona
;  (ven_invalidar...); cada tarea pinta sus ventanas cuando recibe WM_PAINT,
;  solo en su región visible (dc.inc), sobre la superficie; y una vez por
;  vuelta el sistema lleva a la pantalla lo que ha cambiado (ges_presentar).
;
;  Para los programas (conservan los registros salvo EAX si devuelven algo):
;    ven_crear       ESI=plantilla EBP=madre (0: ninguna) -> EAX=ventana
;    ven_lista       ESI=plantillas seguidas, acabadas en dd 0; EBP=madre
;    ven_destruir    EBP
;    ven_mover       EBP EAX=x EBX=y ECX=ancho EDX=alto (hijas: en la zona
;                    de la madre)
;    ven_mostrar     EBP EAX=0 ocultar, 1 mostrar, 2 minimizar, 3 maximizar,
;                    4 restaurar
;    ven_activar     EBP: delante y activa
;    ven_enfocar     EBP: le da el foco del teclado
;    ven_capturar    EBP: el ratón es suyo hasta que se suelte el botón
;    ven_soltar
;    ven_invalidar   EBP: toda su zona, a repintar
;    ven_invalidar_rect  EBP EAX=x EBX=y ECX=ancho EDX=alto (de su zona)
;    ven_texto       EBP ESI=texto (se guarda el puntero) y se repinta
;    ven_cli         EBP -> EAX=x EBX=y (pantalla) ECX=ancho EDX=alto de su zona
;    ven_hija_id     EBP=madre ECX=id -> EAX=ventana hija (0 si no hay)
;    ven_defecto     procedimiento por defecto (DefWindowProc)
;  Del sistema: ges_iniciar, ges_presentar, ges_raton_mueve, ges_raton_pulsa,
;  ges_raton_suelta (EAX=x EBX=y), ges_raton_der, ges_tecla (AX=tecla de
;  INT 16h).
; =============================================================================

ges_iniciar:
        pushad
        mov ecx, MAX_VEN * V_TAM
        call mem_pedir
        mov [ven_tabla], eax
        popad
        ret

; ---- medidas y consultas ----
ven_cli:
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        test dword [ebp+V_ESTILO], E_TITULO
        jz .e
        add eax, BORDE
        add ebx, BORDE + TIT_H + 1
        sub ecx, 2*BORDE
        sub edx, 2*BORDE + TIT_H + 1
        cmp dword [ebp+V_MENU], 0
        je .f
        add ebx, MENU_H
        sub edx, MENU_H
        jmp .f
.e:
        test dword [ebp+V_ESTILO], E_EMERGENTE
        jz .f
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
.f:
        test ecx, ecx
        jns .g
        xor ecx, ecx
.g:
        test edx, edx
        jns .h
        xor edx, edx
.h:
        ret

; CF=1 si EBP no se ve (ella o una antepasada, oculta o minimizada)
ven_visible:
        push eax
        mov eax, ebp
.l:
        test dword [eax+V_ESTADO], S_VISIBLE
        jz .no
        test dword [eax+V_ESTADO], S_MINIM
        jnz .no
        mov eax, [eax+V_PADRE]
        test eax, eax
        jnz .l
        pop eax
        clc
        ret
.no:
        pop eax
        stc
        ret

; -> EAX=la ventana de primer nivel a la que pertenece EBP
ven_tope:
        mov eax, ebp
.l:
        cmp dword [eax+V_PADRE], 0
        je .x
        mov eax, [eax+V_PADRE]
        jmp .l
.x:
        ret

ven_hija_id:
        push ebx
        push edx
        mov eax, [ven_tabla]
        mov edx, MAX_VEN
.b:
        cmp dword [eax+V_USO], 1
        jne .s
        cmp [eax+V_PADRE], ebp
        jne .s
        cmp [eax+V_ID], ecx
        je .x
.s:
        add eax, V_TAM
        dec edx
        jnz .b
        xor eax, eax
.x:
        pop edx
        pop ebx
        ret

; ---- marcar para repintar ----
; (interna) EBP, (EAX, EBX)-(ECX, EDX) en la pantalla: lo que caiga en su zona
ven_inv_p:
        pushad
        call ven_visible
        jc .x
        mov esi, eax
        mov edi, ebx
        push ecx
        push edx
        call ven_cli
        add ecx, eax
        add edx, ebx
        cmp eax, esi
        jge .a
        mov eax, esi
.a:
        cmp ebx, edi
        jge .b
        mov ebx, edi
.b:
        pop edi
        pop esi
        cmp ecx, esi
        jle .c
        mov ecx, esi
.c:
        cmp edx, edi
        jle .d
        mov edx, edi
.d:
        cmp eax, ecx
        jge .x
        cmp ebx, edx
        jge .x
        mov esi, [ebp+V_UX0]
        cmp esi, [ebp+V_UX1]
        jl .une
        mov [ebp+V_UX0], eax
        mov [ebp+V_UY0], ebx
        mov [ebp+V_UX1], ecx
        mov [ebp+V_UY1], edx
        jmp .act
.une:
        cmp eax, [ebp+V_UX0]
        jge .u1
        mov [ebp+V_UX0], eax
.u1:
        cmp ebx, [ebp+V_UY0]
        jge .u2
        mov [ebp+V_UY0], ebx
.u2:
        cmp ecx, [ebp+V_UX1]
        jle .u3
        mov [ebp+V_UX1], ecx
.u3:
        cmp edx, [ebp+V_UY1]
        jle .act
        mov [ebp+V_UY1], edx
.act:
        mov byte [actividad], 1
.x:
        popad
        ret

ven_invalidar:
        pushad
        call ven_cli
        add ecx, eax
        add edx, ebx
        call ven_inv_p
        popad
        ret

ven_invalidar_rect:
        pushad
        mov esi, eax
        mov edi, ebx
        push ecx
        push edx
        call ven_cli
        add eax, esi
        add ebx, edi
        pop edx
        pop ecx
        add ecx, eax
        add edx, ebx
        call ven_inv_p
        popad
        ret

; (interna) marco y zona interior de una ventana de primer nivel
ven_inv_toda:
        or dword [ebp+V_ESTADO], S_NCSUCIO
        mov byte [actividad], 1
        jmp ven_invalidar

; (interna) (EAX, EBX)-(ECX, EDX) de la pantalla ha quedado al descubierto
ges_exponer:
        pushad
        mov ebp, [escritorio]
        call ven_inv_p
        xor esi, esi
.l:
        cmp esi, [n_orden]
        jae .x
        mov ebp, [orden+esi*4]
        inc esi
        mov edi, [ebp+V_ESTADO]
        and edi, S_VISIBLE | S_MINIM
        cmp edi, S_VISIBLE
        jne .l
        mov edi, [ebp+V_X]
        cmp edi, ecx
        jge .l
        add edi, [ebp+V_AN]
        add edi, SOMBRA
        cmp edi, eax
        jle .l
        mov edi, [ebp+V_Y]
        cmp edi, edx
        jge .l
        add edi, [ebp+V_AL]
        add edi, SOMBRA
        cmp edi, ebx
        jle .l
        or dword [ebp+V_ESTADO], S_NCSUCIO
        call ven_inv_p
        jmp .l
.x:
        mov byte [actividad], 1
        popad
        ret

; (interna) lo que ocupa EBP (con la sombra) queda al descubierto
ges_exponer_ven:
        pushad
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        lea ecx, [eax+ecx+SOMBRA]
        lea edx, [ebx+edx+SOMBRA]
        call ges_exponer
        popad
        ret

; ---- el orden de las ventanas de primer nivel ----
orden_quitar:
        pushad
        xor esi, esi
.b:
        cmp esi, [n_orden]
        jae .x
        cmp [orden+esi*4], ebp
        je .q
        inc esi
        jmp .b
.q:
        dec dword [n_orden]
.d:
        cmp esi, [n_orden]
        jae .x
        mov eax, [orden+esi*4+4]
        mov [orden+esi*4], eax
        inc esi
        jmp .d
.x:
        popad
        ret

orden_poner:
        push eax
        mov eax, [n_orden]
        cmp eax, MAX_TOPE
        jae .x
        mov [orden+eax*4], ebp
        inc dword [n_orden]
.x:
        pop eax
        ret

; ---- crear y destruir ----
ven_crear:
        pushad
        mov dword [esp+28], 0
        mov edi, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [edi+V_USO], 0
        je .hay
        add edi, V_TAM
        dec ecx
        jnz .b
        jmp .x
.hay:
        test dword [esi+P_ESTILO], E_HIJA
        jnz .h0
        cmp dword [n_orden], MAX_TOPE
        jae .x
.h0:
        push edi
        mov ecx, V_TAM / 4
        xor eax, eax
        rep stosd
        pop edi
        mov eax, [esi+P_CLASE]
        mov [edi+V_CLASE], eax
        mov eax, [eax+C_PROC]
        mov [edi+V_PROC], eax
        mov [edi+V_PADRE], ebp
        mov eax, [tar_act]
        mov [edi+V_TAREA], eax
        mov eax, [esi+P_TEXTO]
        mov [edi+V_TEXTO], eax
        mov eax, [esi+P_ID]
        mov [edi+V_ID], eax
        mov eax, [esi+P_ESTILO]
        mov [edi+V_ESTILO], eax
        mov eax, [esi+P_AN]
        mov [edi+V_AN], eax
        mov eax, [esi+P_AL]
        mov [edi+V_AL], eax
        xor eax, eax
        xor ebx, ebx
        test dword [esi+P_ESTILO], E_HIJA
        jz .c1
        call ven_cli                ; (de la madre)
.c1:
        add eax, [esi+P_X]
        add ebx, [esi+P_Y]
        mov [edi+V_X], eax
        mov [edi+V_Y], ebx
        mov dword [edi+V_USO], 1
        mov ebp, edi
        mov [esp+28], ebp
        test dword [ebp+V_ESTILO], E_HIJA
        jnz .c2
        call orden_poner
.c2:
        mov eax, WM_CREATE
        xor ecx, ecx
        mov edx, esi
        call msg_enviar
        test dword [ebp+V_ESTILO], E_OCULTA
        jnz .x
        mov eax, 1
        call ven_mostrar
.x:
        popad
        ret

ven_lista:
        push eax
        push esi
.l:
        cmp dword [esi], 0
        je .x
        call ven_crear
        add esi, P_TAM
        jmp .l
.x:
        pop esi
        pop eax
        ret

ven_destruir:
        pushad
        test ebp, ebp
        jz .x
        cmp dword [ebp+V_USO], 1
        jne .x
        cmp ebp, [menu_duena]
        jne .m
        call menu_cerrar
.m:
        mov esi, [ven_tabla]        ; las hijas, antes
        mov ecx, MAX_VEN
.h:
        cmp dword [esi+V_USO], 1
        jne .hs
        cmp [esi+V_PADRE], ebp
        jne .hs
        push ebp
        mov ebp, esi
        call ven_destruir
        pop ebp
.hs:
        add esi, V_TAM
        dec ecx
        jnz .h
        mov eax, WM_DESTROY
        xor ecx, ecx
        xor edx, edx
        call msg_enviar
        call tmp_quitar_de
        call msg_purgar
        cmp ebp, [captura]
        jne .c1
        mov dword [captura], 0
.c1:
        cmp ebp, [foco]
        jne .c2
        mov dword [foco], 0
.c2:
        call ven_tope
        cmp [eax+V_FOCO], ebp
        jne .c3
        mov dword [eax+V_FOCO], 0
.c3:
        cmp ebp, [arr_ven]
        jne .c4
        mov byte [arrastre], 0
        mov dword [arr_ven], 0
.c4:
        test dword [ebp+V_ESTILO], E_HIJA
        jnz .hija
        call orden_quitar
        test dword [ebp+V_ESTADO], S_MINIM
        jz .c5
        call esc_inv_iconos
.c5:
        call ven_visible
        jc .c6
        call ges_exponer_ven
.c6:
        mov dword [ebp+V_USO], 0
        cmp ebp, [activa]
        jne .x
        mov dword [activa], 0
        call ges_activar_otra
        jmp .x
.hija:
        call ven_visible
        jc .h2
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        mov ecx, [ebp+V_AN]
        add ecx, eax
        mov edx, [ebp+V_AL]
        add edx, ebx
        push ebp
        mov ebp, [ebp+V_PADRE]
        call ven_inv_p
        pop ebp
.h2:
        mov dword [ebp+V_USO], 0
.x:
        popad
        ret

; (interna) destruye las ventanas de primer nivel de la tarea EBX
ges_destruir_de_tarea:
        pushad
        mov ebp, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [ebp+V_USO], 1
        jne .s
        cmp [ebp+V_TAREA], ebx
        jne .s
        cmp dword [ebp+V_PADRE], 0
        jne .s
        call ven_destruir
.s:
        add ebp, V_TAM
        dec ecx
        jnz .b
        popad
        ret

; ---- activa y foco ----
ven_activar:
        pushad
        call ven_tope
        mov ebp, eax
        cmp ebp, [escritorio]
        je .x
        call menu_cerrar
        cmp ebp, [activa]
        je .x
        call ges_desactivar
        mov eax, [n_orden]
        cmp [orden+eax*4-4], ebp
        je .a2
        call orden_quitar           ; delante de todas
        call orden_poner
        call ven_inv_toda
.a2:
        mov [activa], ebp
        or dword [ebp+V_ESTADO], S_NCSUCIO
        mov eax, [ebp+V_FOCO]
        test eax, eax
        jz .a3
        cmp dword [eax+V_USO], 1
        jne .a3
        mov ebp, eax
.a3:
        mov [foco], ebp
        mov eax, WM_SETFOCUS
        xor ecx, ecx
        xor edx, edx
        call msg_enviar
        mov byte [actividad], 1
.x:
        popad
        ret

; (interna) la activa deja de serlo
ges_desactivar:
        pushad
        mov ebp, [foco]
        mov dword [foco], 0
        mov eax, WM_KILLFOCUS
        xor ecx, ecx
        xor edx, edx
        call msg_enviar
        mov ebp, [activa]
        test ebp, ebp
        jz .x
        or dword [ebp+V_ESTADO], S_NCSUCIO
        mov dword [activa], 0
        mov byte [actividad], 1
.x:
        popad
        ret

; (interna) activa la ventana con título que esté más arriba
ges_activar_otra:
        pushad
        mov esi, [n_orden]
.b:
        dec esi
        js .x
        mov ebp, [orden+esi*4]
        test dword [ebp+V_ESTILO], E_TITULO
        jz .b
        mov eax, [ebp+V_ESTADO]
        and eax, S_VISIBLE | S_MINIM
        cmp eax, S_VISIBLE
        jne .b
        call ven_activar
.x:
        popad
        ret

; (sistema) la ventana del fondo pasa delante (Ctrl+Tab)
ges_siguiente:
        pushad
        xor esi, esi
.b:
        cmp esi, [n_orden]
        jae .x
        mov ebp, [orden+esi*4]
        inc esi
        test dword [ebp+V_ESTILO], E_TITULO
        jz .b
        mov eax, [ebp+V_ESTADO]
        and eax, S_VISIBLE | S_MINIM | S_INACTIVA
        cmp eax, S_VISIBLE
        jne .b
        call ven_activar
.x:
        popad
        ret

ven_enfocar:
        pushad
        cmp ebp, [foco]
        je .x
        call ven_tope
        mov esi, eax
        xor edi, edi
        cmp esi, ebp
        je .t
        mov edi, ebp
.t:
        mov [esi+V_FOCO], edi
        cmp esi, [activa]
        jne .x
        push ebp
        mov ebp, [foco]
        mov eax, WM_KILLFOCUS
        xor ecx, ecx
        xor edx, edx
        call msg_enviar
        pop ebp
        mov [foco], ebp
        mov eax, WM_SETFOCUS
        call msg_enviar
.x:
        popad
        ret

ven_capturar:
        mov [captura], ebp
        ret
ven_soltar:
        mov dword [captura], 0
        ret

; (interna) EBP=hija con el foco, AL=0 siguiente / 1 anterior: el foco pasa
; a la hermana más próxima donde para el tabulador
ven_foco_sig:
        pushad
        mov edx, V_TAM
        test al, al
        jz .d
        neg edx
.d:
        mov esi, ebp
        mov ecx, MAX_VEN
        mov eax, [ven_tabla]
        lea ebx, [eax+MAX_VEN*V_TAM]
.b:
        add esi, edx
        cmp esi, eax
        jae .b1
        lea esi, [ebx-V_TAM]
.b1:
        cmp esi, ebx
        jb .b2
        mov esi, eax
.b2:
        cmp dword [esi+V_USO], 1
        jne .s
        mov edi, [ebp+V_PADRE]
        cmp [esi+V_PADRE], edi
        jne .s
        test dword [esi+V_ESTILO], E_TABULA
        jz .s
        mov edi, [esi+V_ESTADO]
        and edi, S_VISIBLE | S_INACTIVA
        cmp edi, S_VISIBLE
        jne .s
        mov ebp, esi
        call ven_enfocar
        jmp .x
.s:
        dec ecx
        jnz .b
.x:
        popad
        ret

; ---- mostrar, mover ----
ven_mostrar:
        pushad
        test dword [ebp+V_ESTILO], E_HIJA
        jnz .hija
        cmp eax, 1
        jb .ocultar
        je .mostrar
        cmp eax, 3
        jb .minimizar
        je .maximizar
        ; restaurar
        test dword [ebp+V_ESTADO], S_MINIM
        jz .r2
        and dword [ebp+V_ESTADO], ~S_MINIM
        call esc_inv_iconos
        jmp .m2
.r2:
        test dword [ebp+V_ESTADO], S_MAXIM
        jz .x
        call ges_exponer_ven
        and dword [ebp+V_ESTADO], ~S_MAXIM
        mov eax, [ebp+V_RX]
        mov ebx, [ebp+V_RY]
        mov ecx, [ebp+V_RAN]
        mov edx, [ebp+V_RAL]
        call ven_mover
        jmp .x
.maximizar:
        test dword [ebp+V_ESTADO], S_MAXIM
        jnz .x
        mov eax, [ebp+V_X]
        mov [ebp+V_RX], eax
        mov eax, [ebp+V_Y]
        mov [ebp+V_RY], eax
        mov eax, [ebp+V_AN]
        mov [ebp+V_RAN], eax
        mov eax, [ebp+V_AL]
        mov [ebp+V_RAL], eax
        call ges_exponer_ven
        or dword [ebp+V_ESTADO], S_MAXIM
        xor eax, eax
        mov ebx, BARRA_H
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        sub edx, BARRA_H
        call ven_mover
        jmp .x
.minimizar:
        test dword [ebp+V_ESTADO], S_MINIM
        jnz .x
        cmp ebp, [menu_duena]
        jne .mi1
        call menu_cerrar
.mi1:
        or dword [ebp+V_ESTADO], S_MINIM
        call ges_exponer_ven
        call esc_inv_iconos
        jmp .otra
.ocultar:
        test dword [ebp+V_ESTADO], S_VISIBLE
        jz .x
        and dword [ebp+V_ESTADO], ~S_VISIBLE
        xor eax, eax
        mov [ebp+V_UX0], eax
        mov [ebp+V_UX1], eax
        call ges_exponer_ven
.otra:
        cmp ebp, [activa]
        jne .x
        call ges_desactivar
        call ges_activar_otra
        jmp .x
.mostrar:
        or dword [ebp+V_ESTADO], S_VISIBLE
.m2:
        call ven_inv_toda
        test dword [ebp+V_ESTILO], E_TITULO
        jz .x
        call ven_activar
        jmp .x
.hija:
        test eax, eax
        jnz .hm
        test dword [ebp+V_ESTADO], S_VISIBLE
        jz .x
        call ven_inv_madre
        and dword [ebp+V_ESTADO], ~S_VISIBLE
        jmp .x
.hm:
        or dword [ebp+V_ESTADO], S_VISIBLE
        call ven_invalidar
.x:
        popad
        ret

; (interna) la zona de la madre donde está la hija EBP, a repintar
ven_inv_madre:
        pushad
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        mov ecx, [ebp+V_AN]
        add ecx, eax
        mov edx, [ebp+V_AL]
        add edx, ebx
        mov ebp, [ebp+V_PADRE]
        call ven_inv_p
        popad
        ret

ven_mover:
        pushad
        test dword [ebp+V_ESTILO], E_HIJA
        jz .tope
        call ven_inv_madre
        push ecx
        push edx
        mov esi, eax
        mov edi, ebx
        push ebp
        mov ebp, [ebp+V_PADRE]
        call ven_cli
        pop ebp
        add eax, esi
        add ebx, edi
        pop edx
        pop ecx
        jmp .pon
.tope:
        call ven_visible
        jc .pon
        call ges_exponer_ven
.pon:
        mov esi, eax
        sub esi, [ebp+V_X]
        mov edi, ebx
        sub edi, [ebp+V_Y]
        call ven_desplazar
        mov [ebp+V_X], eax
        mov [ebp+V_Y], ebx
        cmp ecx, [ebp+V_AN]
        jne .tam
        cmp edx, [ebp+V_AL]
        je .mismo
.tam:
        mov [ebp+V_AN], ecx
        mov [ebp+V_AL], edx
        call ven_cli
        shl edx, 16
        mov dx, cx
        mov eax, WM_SIZE
        xor ecx, ecx
        call msg_enviar
.mismo:
        mov eax, WM_MOVE
        xor ecx, ecx
        xor edx, edx
        call msg_enviar
        test dword [ebp+V_ESTILO], E_HIJA
        jnz .h2
        call ven_inv_toda
        jmp .x
.h2:
        call ven_invalidar
.x:
        popad
        ret

; (interna) mueve las descendientes de EBP (ESI, EDI) puntos
ven_desplazar:
        pushad
        mov eax, esi
        or eax, edi
        jz .x
        mov ebx, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [ebx+V_USO], 1
        jne .s
        cmp [ebx+V_PADRE], ebp
        jne .s
        add [ebx+V_X], esi
        add [ebx+V_Y], edi
        push ebp
        mov ebp, ebx
        call ven_desplazar
        pop ebp
.s:
        add ebx, V_TAM
        dec ecx
        jnz .b
.x:
        popad
        ret

ven_texto:
        mov [ebp+V_TEXTO], esi
        test dword [ebp+V_ESTILO], E_TITULO
        jz ven_invalidar
        or dword [ebp+V_ESTADO], S_NCSUCIO
        mov byte [actividad], 1
        ret

; ---- procedimiento por defecto ----
ven_defecto:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .sube
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        call dc_cerrar
        xor eax, eax
        ret
.cerrar:
        call ven_destruir
        xor eax, eax
        ret
.tecla:
        cmp dword [ebp+V_PADRE], 0
        je .nada
        cmp ecx, TC_TAB
        jne .sube
        xor eax, eax
        test dl, TM_MAYUS
        setnz al
        call ven_foco_sig
.nada:
        xor eax, eax
        ret
.sube:
        mov ebp, [ebp+V_PADRE]      ; lo que no quiere una hija, a su madre
        jmp msg_enviar

; ---- la zona no cliente ----
; botones de la barra de título, de derecha a izquierda: estilo y zona
nc_botones  db E_CERRAR, Z_CERRAR, E_MAXIM, Z_MAXIM, E_MINIM, Z_MINIM, 0

nc_pintar:
        pushad
        and dword [ebp+V_ESTADO], ~S_NCSUCIO
        test dword [ebp+V_ESTILO], E_TITULO | E_EMERGENTE
        jz .x
        mov al, 1
        call reg_ventana
        cmp dword [reg_n], 0
        je .x
        call reg_a_pres
        call cur_ocultar
        mov eax, [ebp+V_X]
        mov [dc_ox], eax
        mov eax, [ebp+V_Y]
        mov [dc_oy], eax
        mov byte [gdi_negrita], 0
        mov esi, [ebp+V_AN]
        mov edi, [ebp+V_AL]
        test dword [ebp+V_ESTADO], S_MAXIM
        jnz .ss
        COLOR C_TINTA               ; sombra
        mov eax, SOMBRA
        mov ebx, edi
        mov ecx, esi
        mov edx, SOMBRA
        call dc_rect
        mov eax, esi
        mov ebx, SOMBRA
        mov ecx, SOMBRA
        lea edx, [edi-SOMBRA]
        call dc_rect
.ss:
        COLOR C_MARRON
        test dword [ebp+V_ESTILO], E_TITULO
        jnz .normal
        xor eax, eax                ; emergente: un filete
        xor ebx, ebx
        mov ecx, esi
        mov edx, edi
        call dc_marco
        jmp .x
.normal:
        xor eax, eax                ; marco y línea bajo el título
        xor ebx, ebx
        mov ecx, esi
        mov edx, BORDE
        call dc_rect
        lea ebx, [edi-BORDE]
        call dc_rect
        mov ebx, BORDE + TIT_H
        mov edx, 1
        call dc_rect
        xor ebx, ebx
        mov ecx, BORDE
        mov edx, edi
        call dc_rect
        lea eax, [esi-BORDE]
        call dc_rect
        COLOR C_ARENA               ; barra de título
        cmp ebp, [activa]
        jne .t1
        COLOR C_TERRACOTA
.t1:
        mov eax, BORDE
        mov ebx, BORDE
        lea ecx, [esi-2*BORDE]
        mov edx, TIT_H
        call dc_rect
        cmp dword [ebp+V_MENU], 0
        je .sinmenu
        call menu_pintar_barra
.sinmenu:
        ; los botones
        lea edi, [esi-BORDE-2-BOT_NC]
        mov esi, nc_botones
.bt:
        movzx edx, byte [esi]
        test edx, edx
        jz .btf
        test [ebp+V_ESTILO], edx
        jz .bts
        movzx edx, byte [esi+1]
        call nc_boton
        sub edi, BOT_NC + 2
.bts:
        add esi, 2
        jmp .bt
.btf:
        ; el título, sin pisar los botones
        mov eax, BORDE
        mov ebx, BORDE
        lea ecx, [edi+BOT_NC-BORDE]
        mov edx, TIT_H
        call dc_cortar
        COLOR C_ARENA_OSC
        cmp ebp, [activa]
        jne .t2
        COLOR C_OCRE
.t2:
        xor edi, edi                ; el rombo
.rombo:
        mov eax, edi
        sub eax, 4
        jns .r1
        neg eax
.r1:
        mov ecx, 4
        sub ecx, eax
        mov eax, BORDE + 13
        sub eax, ecx
        lea ecx, [ecx*2+1]
        lea ebx, [edi+BORDE+TIT_H/2-4]
        mov edx, 1
        call dc_rect
        inc edi
        cmp edi, 9
        jb .rombo
        COLOR C_MARRON
        cmp ebp, [activa]
        jne .t3
        COLOR C_CREMA
        mov byte [gdi_negrita], 1
.t3:
        mov eax, BORDE + 26
        mov ebx, BORDE + (TIT_H - 16) / 2
        mov esi, [ebp+V_TEXTO]
        test esi, esi
        jz .t4
        call dc_texto
.t4:
        mov byte [gdi_negrita], 0
.x:
        call dc_cerrar
        popad
        ret

; (interna) dibuja el botón de la zona EDX en x=EDI de la barra de título
nc_boton:
        pushad
        mov esi, edx
        COLOR C_CREMA
        cmp byte [nc_apretado], 0
        je .b1
        cmp ebp, [arr_ven]
        jne .b1
        cmp edx, [nc_zona]
        jne .b1
        COLOR C_OCRE
.b1:
        mov eax, edi
        mov ebx, BORDE + 2
        mov ecx, BOT_NC
        mov edx, BOT_NC
        call dc_rect
        COLOR C_MARRON
        call dc_marco
        cmp esi, Z_MAXIM
        je .max
        cmp esi, Z_MINIM
        je .min
        COLOR C_GRANATE             ; cerrar: un aspa
        lea eax, [edi+5]
        mov ebx, BORDE + 2 + 5
        lea ecx, [edi+12]
        mov edx, BORDE + 2 + 12
        call dc_linea
        inc eax
        inc ecx
        call dc_linea
        mov ebx, BORDE + 2 + 12
        mov edx, BORDE + 2 + 5
        call dc_linea
        dec eax
        dec ecx
        call dc_linea
        jmp .x
.min:
        lea eax, [edi+4]            ; minimizar: una raya abajo
        mov ebx, BORDE + 2 + 11
        mov ecx, 10
        mov edx, 3
        call dc_rect
        jmp .x
.max:
        test dword [ebp+V_ESTADO], S_MAXIM
        jnz .rest
        lea eax, [edi+4]            ; maximizar: un recuadro
        mov ebx, BORDE + 2 + 4
        mov ecx, 10
        mov edx, 10
        call dc_marco
        mov edx, 3
        call dc_rect
        jmp .x
.rest:
        lea eax, [edi+7]            ; restaurar: dos recuadros
        mov ebx, BORDE + 2 + 3
        mov ecx, 8
        mov edx, 8
        call dc_marco
        mov edx, 2
        call dc_rect
        COLOR C_CREMA
        lea eax, [edi+4]
        mov ebx, BORDE + 2 + 7
        mov edx, 8
        call dc_rect
        COLOR C_MARRON
        call dc_marco
        mov edx, 2
        call dc_rect
.x:
        popad
        ret

; ---- qué hay en un punto ----
; EAX=x EBX=y -> EBP=ventana de primer nivel (o el escritorio), ECX=zona y,
; si es Z_CLIENTE, EDI=la ventana (hija) que está debajo
ges_donde:
        push edx
        push esi
        mov esi, [n_orden]
.b:
        dec esi
        js .esc
        mov ebp, [orden+esi*4]
        mov ecx, [ebp+V_ESTADO]
        and ecx, S_VISIBLE | S_MINIM
        cmp ecx, S_VISIBLE
        jne .b
        mov ecx, eax
        sub ecx, [ebp+V_X]
        js .b
        cmp ecx, [ebp+V_AN]
        jge .b
        mov edx, ebx
        sub edx, [ebp+V_Y]
        js .b
        cmp edx, [ebp+V_AL]
        jge .b
        test dword [ebp+V_ESTILO], E_TITULO
        jz .cli
        test dword [ebp+V_ESTILO], E_TAMANO
        jz .tit
        test dword [ebp+V_ESTADO], S_MAXIM
        jnz .tit
        xor esi, esi                ; ¿en un borde?
        cmp ecx, AGARRE
        jge .b1
        or esi, 1
.b1:
        mov edi, [ebp+V_AN]
        sub edi, ecx
        cmp edi, AGARRE
        jg .b2
        or esi, 2
.b2:
        cmp edx, AGARRE
        jge .b3
        or esi, 4
.b3:
        mov edi, [ebp+V_AL]
        sub edi, edx
        cmp edi, AGARRE
        jg .b4
        or esi, 8
.b4:
        test esi, esi
        jz .tit
        test esi, 12                ; cerca de una esquina: en diagonal
        jz .b6
        cmp ecx, 16
        jge .b5
        or esi, 1
.b5:
        mov edi, [ebp+V_AN]
        sub edi, ecx
        cmp edi, 16
        jg .b8
        or esi, 2
        jmp .b8
.b6:
        cmp edx, 16
        jge .b7
        or esi, 4
.b7:
        mov edi, [ebp+V_AL]
        sub edi, edx
        cmp edi, 16
        jg .b8
        or esi, 8
.b8:
        lea ecx, [esi+Z_BORDE]
        jmp .x
.tit:
        cmp edx, BORDE + TIT_H
        jge .menu
        cmp edx, BORDE + 2
        jl .titulo
        cmp edx, BORDE + 2 + BOT_NC
        jge .titulo
        mov edi, [ebp+V_AN]
        sub edi, BORDE + 2 + BOT_NC
        mov esi, nc_botones
.bt:
        movzx edx, byte [esi]
        test edx, edx
        jz .titulo
        test [ebp+V_ESTILO], edx
        jz .bts
        cmp ecx, edi
        jl .btn
        lea edx, [edi+BOT_NC]
        cmp ecx, edx
        jge .btn
        movzx ecx, byte [esi+1]
        jmp .x
.btn:
        sub edi, BOT_NC + 2
.bts:
        add esi, 2
        jmp .bt
.titulo:
        mov ecx, Z_TITULO
        jmp .x
.menu:
        cmp dword [ebp+V_MENU], 0
        je .cli
        cmp edx, BORDE + TIT_H + 1 + MENU_H
        jge .cli
        mov ecx, Z_MENU
        jmp .x
.esc:
        mov ebp, [escritorio]
.cli:
        push eax
        push ebx
        mov esi, eax
        mov edi, ebx
        call ven_cli                ; ¿dentro de la zona interior?
        sub esi, eax
        sub edi, ebx
        pop ebx
        pop eax
        cmp esi, ecx
        jae .fuera
        cmp edi, edx
        jae .fuera
        mov edi, ebp
        ; la hija más profunda que tenga el punto
.hijas:
        mov esi, [ven_tabla]
        mov ecx, MAX_VEN
.h:
        cmp dword [esi+V_USO], 1
        jne .hs
        cmp [esi+V_PADRE], edi
        jne .hs
        test dword [esi+V_ESTADO], S_VISIBLE
        jz .hs
        mov edx, eax
        sub edx, [esi+V_X]
        js .hs
        cmp edx, [esi+V_AN]
        jge .hs
        mov edx, ebx
        sub edx, [esi+V_Y]
        js .hs
        cmp edx, [esi+V_AL]
        jge .hs
        mov edi, esi
        jmp .hijas
.hs:
        add esi, V_TAM
        dec ecx
        jnz .h
        mov ecx, Z_CLIENTE
        jmp .x
.fuera:
        mov ecx, Z_NADA
.x:
        pop esi
        pop edx
        ret

; (interna) pone en la cola de la ventana EDI el mensaje de ratón EAX, con
; la posición dentro de su zona (x en la palabra baja de EDX, y en la alta)
ges_msg_raton:
        pushad
        mov ebp, edi
        push eax
        call ven_cli
        mov edx, [rat_y]
        sub edx, ebx
        shl edx, 16
        mov ecx, [rat_x]
        sub ecx, eax
        mov dx, cx
        pop eax
        movzx ecx, byte [tec_mayus]
        call msg_poner
        popad
        ret

; ---- el ratón ----
ges_raton_pulsa:
        pushad
        cmp dword [menu_ven], 0
        je .nm
        call menu_raton_pulsa
        jmp .x
.nm:
        mov edi, [captura]
        test edi, edi
        jnz .post
        call ges_donde
        mov esi, ecx
        test dword [ebp+V_ESTADO], S_INACTIVA
        jnz .x
        cmp ebp, [escritorio]
        je .zona
        test dword [ebp+V_ESTILO], E_TITULO
        jz .zona
        call ven_activar
.zona:
        mov ecx, esi
        cmp ecx, Z_NADA
        je .x
        cmp ecx, Z_CLIENTE
        je .cliente
        cmp ecx, Z_TITULO
        je .titulo
        cmp ecx, Z_MENU
        je .menu
        mov [arr_ven], ebp
        cmp ecx, Z_BORDE
        jae .borde
        mov [nc_zona], ecx          ; un botón de la barra: queda apretado
        mov byte [arrastre], 3
        mov byte [nc_apretado], 1
        or dword [ebp+V_ESTADO], S_NCSUCIO
        jmp .x
.borde:
        sub ecx, Z_BORDE
        mov [arr_bordes], cl
        mov byte [arrastre], 2
        jmp .x
.titulo:
        cmp byte [rat_doble], 0
        je .t1
        test dword [ebp+V_ESTILO], E_MAXIM
        jz .x
        call ges_alternar_max
        jmp .x
.t1:
        test dword [ebp+V_ESTADO], S_MAXIM
        jnz .x
        mov [arr_ven], ebp
        mov byte [arrastre], 1
        sub eax, [ebp+V_X]
        sub ebx, [ebp+V_Y]
        mov [arr_dx], eax
        mov [arr_dy], ebx
        jmp .x
.menu:
        call menu_barra_clic
        jmp .x
.cliente:
        test dword [edi+V_ESTADO], S_INACTIVA
        jnz .x
        mov esi, [edi+V_CLASE]
        test byte [esi+C_BAND], CB_FOCO
        jz .c1
        test dword [edi+V_ESTILO], E_SINFOCO
        jnz .c1
        mov ebp, edi
        call ven_enfocar
.c1:
        mov [captura], edi
.post:
        mov eax, WM_LBUTTONDOWN
        cmp byte [rat_doble], 0
        je .p1
        mov eax, WM_LBUTTONDBLCLK
.p1:
        call ges_msg_raton
.x:
        popad
        ret

; el botón derecho (BH=1 apretado, 0 soltado): WM_RBUTTONDOWN a la ventana
; que hay debajo, que se queda con el ratón hasta que se suelta; entonces
; recibe WM_RBUTTONUP
ges_raton_der:
        pushad
        test bh, bh
        jz .sube
        mov eax, [menu_ven]
        or eax, [captura]
        jnz .x
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call ges_donde
        cmp ecx, Z_CLIENTE
        jne .x
        mov eax, [ebp+V_ESTADO]
        or eax, [edi+V_ESTADO]
        test al, S_INACTIVA
        jnz .x
        mov eax, WM_RBUTTONDOWN
        call ges_msg_raton
        mov [captura], edi
        mov byte [cap_der], 1
        jmp .x
.sube:
        cmp byte [cap_der], 0
        je .x
        mov byte [cap_der], 0
        mov edi, [captura]
        test edi, edi
        jz .x
        cmp byte [rat_boton], 0     ; (con el izquierdo apretado, la captura es suya)
        jne .x
        mov dword [captura], 0
        mov eax, WM_RBUTTONUP
        call ges_msg_raton
.x:
        popad
        ret

; (interna) EBP: maximiza o restaura
ges_alternar_max:
        push eax
        mov eax, 3
        test dword [ebp+V_ESTADO], S_MAXIM
        jz .m
        mov eax, 4
.m:
        call ven_mostrar
        pop eax
        ret

; (interna) deja la posición EAX, EBX de una ventana de ancho ECX donde la
; barra de título quede al alcance del ratón
ges_acotar:
        push edx
        mov edx, 32
        sub edx, ecx
        cmp eax, edx
        jge .a
        mov eax, edx
.a:
        mov edx, [vid_ancho]
        sub edx, 32
        cmp eax, edx
        jle .b
        mov eax, edx
.b:
        cmp ebx, BARRA_H
        jge .c
        mov ebx, BARRA_H
.c:
        mov edx, [vid_alto]
        sub edx, BORDE + TIT_H
        cmp ebx, edx
        jle .d
        mov ebx, edx
.d:
        pop edx
        ret

ges_raton_mueve:
        pushad
        mov cl, [arrastre]
        cmp cl, 1
        je .mover
        cmp cl, 2
        je .tam
        cmp cl, 3
        je .boton
        cmp dword [menu_ven], 0
        je .nm
        call menu_raton_mueve
        jmp .x
.nm:
        mov edi, [captura]
        test edi, edi
        jnz .post
        call ges_donde
        cmp ecx, Z_CLIENTE
        jne .x
        test dword [ebp+V_ESTADO], S_INACTIVA
        jnz .x
        test dword [edi+V_ESTADO], S_INACTIVA
        jnz .x
.post:
        mov eax, WM_MOUSEMOVE
        call ges_msg_raton
        jmp .x
.mover:
        mov ebp, [arr_ven]
        sub eax, [arr_dx]
        sub ebx, [arr_dy]
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        call ges_acotar
        cmp eax, [ebp+V_X]
        jne .m1
        cmp ebx, [ebp+V_Y]
        je .x
.m1:
        call ven_mover
        jmp .x
.tam:
        mov ebp, [arr_ven]
        mov esi, [ebp+V_X]
        mov edi, [ebp+V_Y]
        mov ecx, esi
        add ecx, [ebp+V_AN]
        mov edx, edi
        add edx, [ebp+V_AL]
        test byte [arr_bordes], 1
        jz .t2
        mov esi, eax
        lea eax, [ecx-MIN_AN]
        cmp esi, eax
        jle .t2
        mov esi, eax
.t2:
        mov eax, [esp+28]
        test byte [arr_bordes], 2
        jz .t3
        lea ecx, [eax+1]
        lea eax, [esi+MIN_AN]
        cmp ecx, eax
        jge .t3
        mov ecx, eax
.t3:
        test byte [arr_bordes], 4
        jz .t4
        mov edi, ebx
        cmp edi, BARRA_H
        jge .t3b
        mov edi, BARRA_H
.t3b:
        lea eax, [edx-MIN_AL]
        cmp edi, eax
        jle .t4
        mov edi, eax
.t4:
        test byte [arr_bordes], 8
        jz .t5
        lea edx, [ebx+1]
        lea eax, [edi+MIN_AL]
        cmp edx, eax
        jge .t5
        mov edx, eax
.t5:
        mov eax, esi
        mov ebx, edi
        sub ecx, eax
        sub edx, ebx
        cmp eax, [ebp+V_X]
        jne .t6
        cmp ebx, [ebp+V_Y]
        jne .t6
        cmp ecx, [ebp+V_AN]
        jne .t6
        cmp edx, [ebp+V_AL]
        je .x
.t6:
        call ven_mover
        jmp .x
.boton:
        call ges_donde              ; ¿sigue encima del botón?
        xor al, al
        cmp ebp, [arr_ven]
        jne .b1
        cmp ecx, [nc_zona]
        jne .b1
        inc al
.b1:
        cmp al, [nc_apretado]
        je .x
        mov [nc_apretado], al
        mov ebp, [arr_ven]
        or dword [ebp+V_ESTADO], S_NCSUCIO
        mov byte [actividad], 1
.x:
        popad
        ret

ges_raton_suelta:
        pushad
        mov cl, [arrastre]
        mov byte [arrastre], 0
        cmp cl, 3
        je .boton
        test cl, cl
        jnz .x
        cmp dword [menu_ven], 0
        je .nm
        call menu_raton_suelta
        jmp .x
.nm:
        mov edi, [captura]
        test edi, edi
        jz .x
        mov dword [captura], 0
        mov eax, WM_LBUTTONUP
        call ges_msg_raton
        jmp .x
.boton:
        mov ebp, [arr_ven]
        cmp byte [nc_apretado], 0
        je .x
        mov byte [nc_apretado], 0
        or dword [ebp+V_ESTADO], S_NCSUCIO
        mov byte [actividad], 1
        mov ecx, [nc_zona]
        cmp ecx, Z_MAXIM
        je .max
        cmp ecx, Z_MINIM
        je .min
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        call msg_poner
        jmp .x
.max:
        call ges_alternar_max
        jmp .x
.min:
        mov eax, 2
        call ven_mostrar
.x:
        popad
        ret

; ---- el teclado ----
; letras de las teclas con Alt (código de tecla 10h a 32h)
alt_letras  db "QWERTYUIOP", 0, 0, 0, 0, "ASDFGHJKL", 0, 0, 0, 0, 0, "ZXCVBNM"

ges_tecla:
        pushad
        cmp dword [menu_ven], 0
        je .nm
        call menu_tecla
        jmp .x
.nm:
        mov ebp, [activa]
        test al, al
        jnz .normal
        cmp ah, 0x6B                ; Alt+F4: cerrar la ventana activa
        jne .n1
        test ebp, ebp
        jz .x
        test dword [ebp+V_ESTADO], S_INACTIVA
        jnz .x
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        call msg_poner
        jmp .x
.n1:
        cmp ah, 0xA5                ; Alt+Tab o Ctrl+Tab: otra ventana
        je .sig
        cmp ah, 0x94
        jne .n2
.sig:
        call ges_siguiente
        jmp .x
.n2:
        test ebp, ebp
        jz .x
        cmp dword [ebp+V_MENU], 0
        je .normal
        test dword [ebp+V_ESTADO], S_INACTIVA
        jnz .normal
        cmp ah, TC_F10
        jne .n3
        xor ecx, ecx
        call menu_abrir
        mov dword [menu_sel], 0
        jmp .x
.n3:
        cmp ah, 0x10                ; Alt+letra: el menú que la tenga marcada
        jb .normal
        cmp ah, 0x32
        ja .normal
        movzx ebx, ah
        mov bl, [alt_letras+ebx-0x10]
        test bl, bl
        jz .normal
        call menu_letra
        jc .x
.normal:
        mov ebp, [foco]
        test ebp, ebp
        jnz .f1
        mov ebp, [activa]
        test ebp, ebp
        jz .x
.f1:
        push eax
        call ven_tope
        test dword [eax+V_ESTADO], S_INACTIVA
        pop eax
        jnz .x
        movzx edx, byte [tec_mayus]
        movzx ecx, ah
        test al, al
        jz .abajo
        cmp al, 0xE0
        jne .car
        test ah, ah
        jnz .abajo
.car:
        cmp al, 32
        jae .char
        mov ecx, TC_INTRO           ; teclas de control
        cmp al, 13
        je .abajo
        mov ecx, TC_TAB
        cmp al, 9
        je .abajo
        mov ecx, TC_ESC
        cmp al, 27
        je .abajo
        mov ecx, TC_RETRO
        cmp al, 8
        je .abajo
        movzx ecx, al
        add ecx, TC_CTRL
.abajo:
        mov eax, WM_KEYDOWN
        call msg_poner
        jmp .x
.char:
        movzx ecx, al
        mov eax, WM_CHAR
        call msg_poner
.x:
        popad
        ret

; ---- a la pantalla ----
; (sistema) una vez por vuelta: recoloca el cursor y lleva a la pantalla lo
; que ha cambiado en la superficie
ges_presentar:
        pushad
        mov eax, [pres_x0]
        cmp eax, [pres_x1]
        jl .hay
        cmp byte [hay_rat], 0
        je .x
        cmp byte [cur_puesto], 0
        je .hay
        mov eax, [rat_x]
        cmp eax, [cur_x]
        jne .hay
        mov eax, [rat_y]
        cmp eax, [cur_y]
        je .x
.hay:
        call cur_ocultar
        call cur_mostrar
        mov eax, [pres_x0]
        mov ecx, [pres_x1]
        cmp eax, ecx
        jge .p2
        mov ebx, [pres_y0]
        mov edx, [pres_y1]
        call vid_presentar
.p2:
        cmp byte [pan_puesto], 0    ; donde estaba el cursor en la pantalla
        je .p3
        mov eax, [pan_x]
        mov ebx, [pan_y]
        lea ecx, [eax+CUR_ANCHO]
        lea edx, [ebx+CUR_ALTO]
        call vid_presentar
.p3:
        mov al, [cur_puesto]
        mov [pan_puesto], al
        test al, al
        jz .p4
        mov eax, [cur_x]
        mov ebx, [cur_y]
        mov [pan_x], eax
        mov [pan_y], ebx
        lea ecx, [eax+CUR_ANCHO]
        lea edx, [ebx+CUR_ALTO]
        call vid_presentar
.p4:
        xor eax, eax
        mov [pres_x0], eax
        mov [pres_x1], eax
        call gdi_recorte_total
.x:
        popad
        ret
