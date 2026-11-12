; =============================================================================
;  Hispa Ventanas - cuadros de diálogo modales
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un diálogo modal es una ventana de la clase c_dialogo con sus controles;
;  mientras está abierto, su dueña no recibe ratón ni teclado y la tarea se
;  queda dentro de dlg_modal, en un bucle de mensajes propio (las demás
;  tareas siguen su curso). Intro da la orden por defecto, Esc y el botón de
;  cerrar dan ID_CANCELAR; cualquier botón con id menor que 16 lo termina.
;
;    dlg_mensaje  EBP=dueña ESI=texto (10 = otra línea) EDI=título
;                 EAX=DM_ACEPTAR, DM_ACEPTAR_CANCELAR o DM_SI_NO_CANCELAR
;                 -> EAX=ID_ACEPTAR, ID_CANCELAR, ID_SI o ID_NO
;    dlg_pedir    EBP=dueña ESI=pregunta EDI=título EBX=búfer (con el texto
;                 inicial) ECX=tamaño -> EAX=1 si se acepta (texto en el búfer)
;    dlg_archivo  EBP=dueña EDI=título EBX=búfer del nombre (80 octetos)
;                 -> EAX=1 si se acepta: lista los archivos y las carpetas
;                 de la carpeta actual y deja cambiar de carpeta
;    dlg_modal    EBP=dueña ESI=plantilla del diálogo EDI=plantillas de sus
;                 controles (acabadas en dd 0; 0 si los crea el gancho)
;                 EBX=gancho (0: ninguno) EDX=parámetro -> EAX=respuesta
;                 El gancho recibe todos los mensajes del diálogo antes que
;                 el procedimiento normal (y DM_INICIO al empezar) y devuelve
;                 EAX distinto de 0 si ya lo ha atendido. V_D3 = parámetro.
;    dlg_terminar EBP=diálogo EAX=respuesta
; =============================================================================

c_dialogo   dd dlg_proc
            db C_CREMA, 0, 0, 0

dlg_proc:
        cmp dword [ebp+V_D1], 0
        je .sin
        pushad
        call [ebp+V_D1]
        test eax, eax
        popad
        jnz .hecho
.sin:
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CLOSE
        jne ven_defecto
.cancelar:
        mov ecx, ID_CANCELAR
.manda:
        mov eax, WM_COMMAND
        xor edx, edx
        jmp dlg_proc
.tecla:
        cmp ecx, TC_ESC
        je .cancelar
        cmp ecx, TC_INTRO
        jne .hecho
        mov ecx, [ebp+V_D4]
        jmp .manda
.orden:
        cmp ecx, 16                 ; (los avisos de los controles no cuentan)
        jae .hecho
        mov [ebp+V_D0], ecx
.hecho:
        xor eax, eax
        ret

dlg_terminar:
        mov [ebp+V_D0], eax
        ret

dlg_modal:
        pushad
        sub esp, P_TAM              ; la plantilla, con su sitio en la pantalla
        mov eax, edi
        mov edi, esp
        mov ecx, P_TAM / 4
        rep movsd
        mov edi, eax
        mov eax, [vid_ancho]
        sub eax, [esp+P_AN]
        sar eax, 1
        mov [esp+P_X], eax
        mov eax, [vid_alto]
        sub eax, [esp+P_AL]
        sar eax, 1
        mov [esp+P_Y], eax
        or dword [esp+P_ESTILO], E_OCULTA
        mov esi, ebp                ; la dueña, fuera de juego
        test esi, esi
        jz .sd
        or dword [esi+V_ESTADO], S_INACTIVA
.sd:
        push esi
        lea esi, [esp+4]
        xor ebp, ebp
        call ven_crear
        pop esi
        add esp, P_TAM
        test eax, eax
        jz .falla
        mov ebp, eax
        mov [ebp+V_D1], ebx
        mov [ebp+V_D2], esi
        mov [ebp+V_D3], edx
        mov dword [ebp+V_D4], ID_ACEPTAR
        test edi, edi
        jz .sh
        mov esi, edi
        call ven_lista
.sh:
        mov eax, DM_INICIO
        xor ecx, ecx
        xor edx, edx
        call msg_enviar
        mov eax, 1
        call ven_mostrar
        mov esi, [ven_tabla]        ; el foco, al primer control
        mov ecx, MAX_VEN
.pf:
        cmp dword [esi+V_USO], 1
        jne .ps
        cmp [esi+V_PADRE], ebp
        jne .ps
        test dword [esi+V_ESTILO], E_TABULA
        jz .ps
        xchg esi, ebp
        call ven_enfocar
        xchg esi, ebp
        jmp .bucle
.ps:
        add esi, V_TAM
        dec ecx
        jnz .pf
.bucle:
        sub esp, M_TAM
        mov edi, esp
.b:
        cmp dword [ebp+V_D0], 0
        jne .fin
        call msg_tomar
        test eax, eax
        jnz .d
        call msg_salir              ; WM_QUIT: es para el bucle de fuera
        mov dword [ebp+V_D0], ID_CANCELAR
        jmp .fin
.d:
        call msg_despachar
        jmp .b
.fin:
        add esp, M_TAM
        mov ebx, [ebp+V_D0]
        mov esi, [ebp+V_D2]
        test esi, esi
        jz .f2
        and dword [esi+V_ESTADO], ~S_INACTIVA
.f2:
        call ven_destruir
        test esi, esi
        jz .f3
        mov ebp, esi
        call ven_activar
.f3:
        mov [esp+28], ebx
        popad
        ret
.falla:
        test esi, esi
        jz .fa
        and dword [esi+V_ESTADO], ~S_INACTIVA
.fa:
        mov dword [esp+28], ID_CANCELAR
        popad
        ret

; ---- mensaje ----
; parámetro: dd texto, tipo, ancho de la zona, líneas
dlg_mensaje:
        pushad
        sub esp, 16 + P_TAM
        mov [esp], esi
        mov [esp+4], eax
        xor ebx, ebx                ; líneas y la más larga
        xor edx, edx
.l0:
        inc ebx
        xor ecx, ecx
.l1:
        lodsb
        test al, al
        jz .l2
        cmp al, 10
        je .l2
        inc ecx
        jmp .l1
.l2:
        cmp ecx, edx
        jbe .l3
        mov edx, ecx
.l3:
        test al, al
        jnz .l0
        mov [esp+12], ebx
        lea edx, [edx*8+48]
        mov ecx, 220
        cmp dword [esp+4], DM_SI_NO_CANCELAR
        jne .a1
        mov ecx, 310
.a1:
        cmp edx, ecx
        jae .a2
        mov edx, ecx
.a2:
        mov [esp+8], edx
        lea esi, [esp+16]
        mov dword [esi+P_CLASE], c_dialogo
        mov [esi+P_TEXTO], edi
        mov dword [esi+P_ESTILO], E_DIALOGO
        add edx, 2*BORDE
        mov [esi+P_AN], edx
        imul ebx, 18
        add ebx, 16 + 12 + 26 + 14 + 2*BORDE + TIT_H + 1
        mov [esi+P_AL], ebx
        mov dword [esi+P_ID], 0
        xor edi, edi
        mov ebx, dm_gancho
        mov edx, esp
        call dlg_modal
        add esp, 16 + P_TAM
        mov [esp+28], eax
        popad
        ret

; botones de cada tipo: id y texto, acabado en 0
dm_botones:
        dd ID_ACEPTAR, t_aceptar, 0, 0, 0, 0, 0
        dd ID_ACEPTAR, t_aceptar, ID_CANCELAR, t_cancelar, 0, 0, 0
        dd 0, 0, 0, 0, 0, 0, 0
        dd ID_SI, t_si, ID_NO, t_no, ID_CANCELAR, t_cancelar, 0

dm_gancho:
        cmp eax, DM_INICIO
        jne .no
        mov edi, [ebp+V_D3]
        sub esp, P_TAM
        mov esi, esp
        mov dword [esi+P_CLASE], c_estatico
        mov eax, [edi]
        mov [esi+P_TEXTO], eax
        mov dword [esi+P_ESTILO], E_HIJA
        mov dword [esi+P_X], 24
        mov dword [esi+P_Y], 16
        mov eax, [edi+8]
        sub eax, 48
        mov [esi+P_AN], eax
        mov eax, [edi+12]
        imul eax, 18
        mov [esi+P_AL], eax
        mov dword [esi+P_ID], 0
        call ven_crear
        mov ebx, [edi+4]            ; los botones, centrados
        imul ebx, 28
        add ebx, dm_botones
        mov eax, [ebx]
        mov [ebp+V_D4], eax         ; el primero es el de Intro
        xor ecx, ecx
.n:
        cmp dword [ebx+ecx*8], 0
        je .n2
        inc ecx
        jmp .n
.n2:
        imul ecx, 90
        sub ecx, 10
        mov eax, [edi+8]
        sub eax, ecx
        sar eax, 1
        mov edx, [edi+12]
        imul edx, 18
        add edx, 16 + 12
        mov dword [esi+P_CLASE], c_boton
        mov dword [esi+P_ESTILO], E_HIJA | E_TABULA | BS_DEFECTO
        mov [esi+P_Y], edx
        mov dword [esi+P_AN], 80
        mov dword [esi+P_AL], 26
.b:
        cmp dword [ebx], 0
        je .bf
        mov [esi+P_X], eax
        mov ecx, [ebx]
        mov [esi+P_ID], ecx
        mov ecx, [ebx+4]
        mov [esi+P_TEXTO], ecx
        push eax
        call ven_crear
        pop eax
        mov dword [esi+P_ESTILO], E_HIJA | E_TABULA
        add eax, 90
        add ebx, 8
        jmp .b
.bf:
        add esp, P_TAM
.no:
        xor eax, eax
        ret

; ---- pedir un texto ----
; parámetro: dd pregunta, búfer, tamaño
dp_ventana:
        PLANTILLA c_dialogo, 0, E_DIALOGO, 0, 0, 340, 150, 0
dp_hijos:
        PLANTILLA c_estatico, 0, E_HIJA, 16, 14, 304, 18, 100
        PLANTILLA c_edit, 0, E_HIJA | E_TABULA, 16, 40, 304, 24, 101
        PLANTILLA c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 84, 80, 80, 26, ID_ACEPTAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, 176, 80, 80, 26, ID_CANCELAR
        dd 0

dlg_pedir:
        pushad
        sub esp, 12 + P_TAM
        mov [esp], esi
        mov [esp+4], ebx
        mov [esp+8], ecx
        mov eax, edi
        lea edi, [esp+12]
        mov esi, dp_ventana
        mov ecx, P_TAM / 4
        rep movsd
        mov [esp+12+P_TEXTO], eax
        lea esi, [esp+12]
        mov edi, dp_hijos
        mov ebx, dp_gancho
        mov edx, esp
        call dlg_modal
        add esp, 12 + P_TAM
        cmp eax, ID_ACEPTAR
        sete al
        movzx eax, al
        mov [esp+28], eax
        popad
        ret

dp_gancho:
        mov edi, [ebp+V_D3]
        cmp eax, DM_INICIO
        je .inicio
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        mov ecx, 101                ; el texto, al búfer de quien pregunta
        call ven_hija_id
        mov ebp, eax
        mov eax, WM_GETTEXT
        mov ecx, [edi+8]
        mov edx, [edi+4]
        call msg_enviar
        jmp .no
.inicio:
        mov ecx, 100
        call ven_hija_id
        mov ebx, [edi]
        mov [eax+V_TEXTO], ebx
        mov ecx, 101
        call ven_hija_id
        mov ebp, eax
        mov eax, WM_SETTEXT
        mov edx, [edi+4]
        call msg_enviar
        mov eax, EM_SEL_TODO
        call msg_enviar
.no:
        xor eax, eax
        ret

; ---- elegir un archivo ----
; parámetro: el búfer del nombre. V_D5 del diálogo: texto «Carpeta: ...»
da_ventana:
        PLANTILLA c_dialogo, 0, E_DIALOGO, 0, 0, 356, 300, 0
da_hijos:
        PLANTILLA c_estatico, t_nombre, E_HIJA, 16, 12, 320, 18, 100
        PLANTILLA c_edit, 0, E_HIJA | E_TABULA, 16, 32, 320, 24, 101
        PLANTILLA c_estatico, 0, E_HIJA, 16, 62, 320, 18, 102
        PLANTILLA c_lista, 0, E_HIJA | E_TABULA, 16, 84, 320, 132, 103
        PLANTILLA c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 92, 228, 80, 26, ID_ACEPTAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, 184, 228, 80, 26, ID_CANCELAR
        dd 0

dlg_archivo:
        pushad
        sub esp, P_TAM
        mov eax, edi
        mov edi, esp
        mov esi, da_ventana
        mov ecx, P_TAM / 4
        rep movsd
        mov [esp+P_TEXTO], eax
        mov esi, esp
        mov edi, da_hijos
        mov edx, ebx
        mov ebx, da_gancho
        call dlg_modal
        add esp, P_TAM
        cmp eax, ID_ACEPTAR
        sete al
        movzx eax, al
        mov [esp+28], eax
        popad
        ret

da_gancho:
        cmp eax, DM_INICIO
        je .inicio
        cmp eax, WM_DESTROY
        je .destruir
        cmp eax, WM_COMMAND
        jne .no
        cmp cx, 103
        je .lista
        cmp ecx, ID_ACEPTAR
        jne .no
        ; Aceptar: el nombre del cuadro de texto
        mov edi, [ebp+V_D3]
        push ebp
        mov ecx, 101
        call ven_hija_id
        mov ebp, eax
        mov eax, WM_GETTEXT
        mov ecx, 80
        mov edx, edi
        call msg_enviar
        pop ebp
        cmp byte [edi], 0
        je .si                      ; vacío: no se acepta
        cmp byte [edi], '['
        jne .no
        mov esi, edi
        call da_entrar
        jmp .si
.lista:
        shr ecx, 16
        mov esi, edx                ; la lista
        push ebp
        mov ebp, esi
        mov eax, LM_SELEC
        call msg_enviar
        mov edi, eax
        test eax, eax
        js .l9
        push ecx
        mov ecx, eax
        mov eax, LM_TEXTO
        call msg_enviar
        pop ecx
        mov esi, eax                ; el texto elegido
        pop ebp
        cmp ecx, LN_DOBLE
        je .doble
        cmp byte [esi], '['
        je .si
        call da_al_cuadro
        jmp .si
.doble:
        cmp byte [esi], '['
        jne .d2
        call da_entrar
        jmp .si
.d2:
        call da_al_cuadro
        mov eax, WM_COMMAND
        mov ecx, ID_ACEPTAR
        xor edx, edx
        call msg_enviar
        jmp .si
.l9:
        pop ebp
        jmp .si
.destruir:
        mov eax, [ebp+V_D5]
        call mem_soltar
        jmp .no
.inicio:
        mov ecx, 96
        call mem_pedir
        mov [ebp+V_D5], eax
        call da_llenar
        mov esi, [ebp+V_D3]
        call da_al_cuadro
.no:
        xor eax, eax
        ret
.si:
        mov eax, 1
        ret

; (interna) pone el texto ESI en el cuadro del nombre del diálogo EBP
da_al_cuadro:
        pushad
        mov ecx, 101
        call ven_hija_id
        mov ebp, eax
        mov eax, WM_SETTEXT
        mov edx, esi
        call msg_enviar
        mov eax, EM_SEL_TODO
        call msg_enviar
        popad
        ret

; (interna) entra en la carpeta ESI=«[NOMBRE]» y vuelve a llenar la lista
da_entrar:
        pushad
        sub esp, 16
        mov edi, esp
        inc esi
.c:
        lodsb
        cmp al, ']'
        je .f
        test al, al
        jz .f
        stosb
        jmp .c
.f:
        mov byte [edi], 0
        mov esi, esp
        call dos_cambiar_dir
        add esp, 16
        call da_llenar
        popad
        ret

; (interna) llena la lista del diálogo EBP con la carpeta actual
da_llenar:
        pushad
        mov edi, [ebp+V_D5]
        test edi, edi
        jz .x
        mov esi, t_carpeta
        call pon_cad
        call dos_dir_actual
        mov ecx, 102
        call ven_hija_id
        mov esi, [ebp+V_D5]
        push ebp
        mov ebp, eax
        call ven_texto
        pop ebp
        mov ecx, 103
        call ven_hija_id
        mov ebp, eax                ; la lista
        mov eax, LM_VACIAR
        call msg_enviar
        mov esi, t_todos
        mov ecx, 0x10
        call dos_primero
.b:
        jc .x
        sub esp, 16
        mov edi, esp
        mov esi, dos_dta + 0x1E
        test byte [dos_dta+0x15], 0x10
        jz .n
        cmp word [esi], '.'         ; la propia carpeta no se lista
        je .s
        mov al, '['
        stosb
        call pon_cad
        mov al, ']'
        stosb
        jmp .n2
.n:
        call pon_cad
.n2:
        mov byte [edi], 0
        mov eax, LM_ANADIR
        mov edx, esp
        call msg_enviar
.s:
        add esp, 16
        call dos_siguiente
        jmp .b
.x:
        popad
        ret
