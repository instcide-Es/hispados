; =============================================================================
;  Hispa Ventanas - Centro de programas (el lanzador) y «Sistema»
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Una ventana con un icono por programa: un clic lo marca, doble clic o
;  Intro lo abre (o lo trae delante si ya está abierto); las flechas cambian
;  de icono. Cada programa es una tarea (tar_crear) con su bucle de mensajes.
;    centro_tarea    la entrada del Centro (se lanza al arrancar)
;    app_bucle       el bucle de mensajes de un programa: hasta WM_QUIT
; =============================================================================

CEN_PASO    equ 104                 ; de un icono al siguiente
CEN_N       equ 4

c_centro    dd centro_proc
            db C_CREMA, 0, 0, 0
c_sistema   dd sisinfo_proc
            db C_CREMA, 0, 0, 0

centro_plant:
        PLANTILLA c_centro, t_centro, E_NORMAL, 36, 52, 452, 190, 0
sisinfo_plant:
        PLANTILLA c_sistema, tit_sistema, E_TITULO | E_CERRAR | E_MINIM, 300, 330, 400, 150, 0

; los programas: icono, nombre, entrada de la tarea, variable con su ventana
centro_tabla:
        dd ico_bloc, t_bloc, bloc_tarea, bloc_ven
        dd ico_calc, t_calc, calc_tarea, calc_ven
        dd ico_reloj, t_reloj, reloj_tarea, reloj_ven
        dd ico_ventana, tit_sistema, sisinfo_tarea, sisinfo_ven

menu_centro:
        db "&Programas", 0
        dw 201
        db "&Bloc de notas", 0
        dw 202
        db "&Calculadora", 0
        dw 203
        db "&Reloj", 0
        dw 204
        db "&Sistema", 0
        dw 0
        db 0
        dw 210
        db "Salir a &Hispa-DOS", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw 220
        db "&Acerca de Hispa Ventanas...", 0
        dw 0xFFFF
        db 0

app_bucle:
        sub esp, M_TAM
        mov edi, esp
.l:
        call msg_tomar
        test eax, eax
        jz .x
        call msg_despachar
        jmp .l
.x:
        add esp, M_TAM
        ret

centro_tarea:
        mov esi, centro_plant
        xor ebp, ebp
        call ven_crear
        jmp app_bucle

; abre el programa ECX (o lo trae delante si ya está abierto)
centro_lanzar:
        pushad
        cmp ecx, CEN_N
        jae .x
        shl ecx, 4
        mov eax, [centro_tabla+ecx+12]
        mov ebp, [eax]
        test ebp, ebp
        jz .nuevo
        mov eax, 4
        call ven_mostrar
        call ven_activar
        jmp .x
.nuevo:
        mov eax, [centro_tabla+ecx+8]
        call tar_crear
.x:
        popad
        ret

centro_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_SETFOCUS
        je .inv
        cmp eax, WM_KILLFOCUS
        je .inv
        cmp eax, WM_CLOSE
        je .cerrar
        jmp ven_defecto
.crear:
        mov dword [ebp+V_MENU], menu_centro
        mov dword [ebp+V_ICONO], ico_centro
        jmp .nada
.pulsa:
        push eax
        call raton_xy
        sub eax, 12
        js .p9
        xor edx, edx
        mov ecx, CEN_PASO
        div ecx
        cmp eax, CEN_N
        jae .p9
        cmp ebx, 92
        jae .p9
        mov [ebp+V_D0], eax
        call ven_invalidar
        mov ecx, eax
        pop eax
        cmp eax, WM_LBUTTONDBLCLK
        jne .nada
        call centro_lanzar
        jmp .nada
.p9:
        pop eax
        jmp .nada
.tecla:
        mov eax, [ebp+V_D0]
        cmp ecx, TC_DER
        jne .t1
        inc eax
        cmp eax, CEN_N
        jb .tm
        xor eax, eax
        jmp .tm
.t1:
        cmp ecx, TC_IZQ
        jne .t2
        dec eax
        jns .tm
        mov eax, CEN_N - 1
.tm:
        mov [ebp+V_D0], eax
.inv:
        call ven_invalidar
        jmp .nada
.t2:
        cmp ecx, TC_INTRO
        jne .nada
        mov ecx, eax
        call centro_lanzar
        jmp .nada
.orden:
        cmp ecx, 210
        je .cerrar
        cmp ecx, 220
        je .acerca
        sub ecx, 201
        call centro_lanzar
        jmp .nada
.acerca:
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        call dlg_mensaje
        jmp .nada
.cerrar:
        mov esi, t_salir_preg
        mov edi, tit_barra
        mov eax, DM_ACEPTAR_CANCELAR
        call dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne .nada
        mov byte [salir], 1
.nada:
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        xor edi, edi
.i:
        mov eax, edi
        imul eax, CEN_PASO
        add eax, 12 + (CEN_PASO - 32) / 2
        mov ebx, 16
        mov edx, edi
        shl edx, 4
        mov esi, [centro_tabla+edx]
        call dc_icono
        mov esi, [centro_tabla+edx+4]
        call cad_largo
        mov ecx, eax
        shl eax, 2
        neg eax
        mov edx, edi
        imul edx, CEN_PASO
        lea eax, [eax+edx+12+CEN_PASO/2]
        mov ebx, 58
        COLOR C_TINTA
        cmp edi, [ebp+V_D0]
        jne .i2
        push eax
        push ecx
        COLOR C_ARENA_OSC
        cmp ebp, [foco]
        jne .i1
        COLOR C_ANIL
.i1:
        sub eax, 4
        lea ecx, [ecx*8+8]
        mov edx, 18
        dec ebx
        call dc_rect
        inc ebx
        pop ecx
        pop eax
        COLOR C_BLANCO
.i2:
        call dc_texto
        inc edi
        cmp edi, CEN_N
        jb .i
        COLOR C_ARENA_OSC
        mov eax, 16
        mov ebx, 100
        mov esi, t_centro_pista
        call dc_texto
        call dc_cerrar
        xor eax, eax
        ret

; ---- «Sistema»: lo que ha encontrado Hispa Ventanas ----
sisinfo_tarea:
        mov esi, sisinfo_plant
        xor ebp, ebp
        call ven_crear
        mov [sisinfo_ven], eax
        jmp app_bucle

sisinfo_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_DESTROY
        jne ven_defecto
        mov dword [sisinfo_ven], 0
        call msg_salir
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        call ven_cli
        xor eax, eax
        xor ebx, ebx
        call pinta_sistema
        call dc_cerrar
        xor eax, eax
        ret
