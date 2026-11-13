; =============================================================================
;  Hispa Ventanas - Centro de programas (grupos y accesos directos) y «Sistema»
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  La ventana del Centro tiene un icono por grupo; cada grupo se abre en su
;  propia ventana, con un icono por acceso directo. Un clic marca, doble clic
;  o Intro abre; las flechas cambian de icono y Supr borra. Un acceso directo
;  se lleva a otro grupo arrastrándolo a la ventana de ese grupo (o a su
;  icono en el Centro), o con «Mover a otro grupo...». El menú Archivo es el
;  mismo en todas y actúa sobre lo marcado en la ventana que lo tiene.
;
;  Un acceso directo abre un programa interno (los de «Accesorios»: una tarea
;  con su ventana, como siempre) o un programa de Hispa-DOS (.COM, .EXE o
;  .BAT): entonces Hispa Ventanas guarda su estado en CENTRO.INI, deja el
;  encargo al lanzador (ventanas.asm) y termina; el lanzador ejecuta el
;  programa y vuelve a cargar Hispa Ventanas.
;
;    centro_tarea    la entrada del Centro (se lanza al arrancar)
;    app_bucle       el bucle de mensajes de un programa: hasta WM_QUIT
;    dlg_lista       EBP=dueña EDI=título ESI=pregunta EBX=textos (uno cada
;                    EDX octetos) ECX=cuántos -> EAX=el elegido (-1: ninguno)
;  Todo lo demás es interno. Los datos (grupos, accesos) están en config.inc.
; =============================================================================

CEN_PASO    equ 104                 ; de un icono al siguiente
CEN_FILA    equ 76                  ; de una fila de iconos a la siguiente
INT_N       equ 4
BP_MAXDIR   equ 64                  ; subcarpetas que mira «Buscar programas»
BP_MAXCAND  equ 40

c_centro    dd vista_proc
            db C_CREMA, 0, 0, 0
c_grupo     dd vista_proc
            db C_CREMA, 0, 0, 0
c_sistema   dd sisinfo_proc
            db C_CREMA, 0, 0, 0

sisinfo_plant:
        PLANTILLA c_sistema, tit_sistema, E_TITULO | E_CERRAR | E_MINIM, 300, 330, 400, 150, 0

; los programas internos: nombre en CENTRO.INI, entrada de la tarea y
; variable con su ventana
int_tabla:
        dd ti_bloc, bloc_tarea, bloc_ven
        dd ti_calc, calc_tarea, calc_ven
        dd ti_reloj, reloj_tarea, reloj_ven
        dd ti_sistema, sisinfo_tarea, sisinfo_ven

menu_centro:
        db "&Archivo", 0
        dw 301
        db "&Abrir", 0
        dw 302
        db "&Nuevo acceso directo...", 0
        dw 303
        db "Nuevo &grupo...", 0
        dw 304
        db "&Propiedades...", 0
        dw 305
        db "&Mover a otro grupo...", 0
        dw 306
        db "&Borrar", 0
        dw 0
        db 0
        dw 307
        db "Bus&car programas...", 0
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

; diálogo de un acceso directo
ad_ventana:
        PLANTILLA c_dialogo, 0, E_DIALOGO, 0, 0, 484, 336, 0
ad_hijos:
        PLANTILLA c_estatico, t_ad_nombre, E_HIJA, 16, 18, 96, 18, 100
        PLANTILLA c_edit, 0, E_HIJA | E_TABULA, 116, 14, 348, 24, 110
        PLANTILLA c_estatico, t_ad_prog, E_HIJA, 16, 50, 96, 18, 101
        PLANTILLA c_edit, 0, E_HIJA | E_TABULA, 116, 46, 244, 24, 111
        PLANTILLA c_boton, t_ad_examinar, E_HIJA | E_TABULA, 368, 45, 96, 26, 120
        PLANTILLA c_estatico, t_ad_dir, E_HIJA, 16, 82, 96, 18, 102
        PLANTILLA c_edit, 0, E_HIJA | E_TABULA, 116, 78, 348, 24, 112
        PLANTILLA c_estatico, t_ad_param, E_HIJA, 16, 114, 96, 18, 103
        PLANTILLA c_edit, 0, E_HIJA | E_TABULA, 116, 110, 348, 24, 113
        PLANTILLA c_estatico, t_ad_icono, E_HIJA, 16, 142, 96, 18, 104
        PLANTILLA c_boton, t_ad_volver, E_HIJA | E_TABULA | BS_CASILLA, 16, 210, 340, 20, 114
        PLANTILLA c_boton, t_ad_pausa, E_HIJA | E_TABULA | BS_CASILLA, 16, 234, 340, 20, 115
        PLANTILLA c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 148, 266, 80, 26, ID_ACEPTAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, 240, 266, 80, 26, ID_CANCELAR
        dd 0
; diálogo de elegir de una lista
dl_ventana:
        PLANTILLA c_dialogo, 0, E_DIALOGO, 0, 0, 300, 250, 0
dl_hijos:
        PLANTILLA c_estatico, 0, E_HIJA, 16, 12, 264, 18, 100
        PLANTILLA c_lista, 0, E_HIJA | E_TABULA, 16, 34, 264, 132, 103
        PLANTILLA c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 62, 178, 80, 26, ID_ACEPTAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, 154, 178, 80, 26, ID_CANCELAR
        dd 0

bp_pat      dd t_bp_exe, t_bp_com, t_bp_bat
; programas que no son el principal de un juego (por cómo empieza el nombre)
bp_no       db "SETUP", 0, "INSTALL", 0, "INSTALAR", 0, "CONFIG", 0, "SETSOUND", 0
            db "SNDSETUP", 0, "UNINSTAL", 0, "README", 0, "LEEME", 0, "DOS4GW", 0
            db "CWSDPMI", 0, "DOS32A", 0, "PMODEW", 0, "UNIVBE", 0, 0

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

; ---- la tarea: la ventana del Centro y los grupos que estaban abiertos ----
centro_tarea:
        sub esp, P_TAM
        mov esi, esp
        mov dword [esi+P_CLASE], c_centro
        mov dword [esi+P_TEXTO], t_centro
        mov dword [esi+P_ESTILO], E_NORMAL
        mov eax, [cen_rect]
        mov [esi+P_X], eax
        mov eax, [cen_rect+4]
        mov [esi+P_Y], eax
        mov eax, [cen_rect+8]
        mov [esi+P_AN], eax
        mov eax, [cen_rect+12]
        mov [esi+P_AL], eax
        mov dword [esi+P_ID], 0
        call rect_acotar
        xor ebp, ebp
        call ven_crear
        mov [cen_ven], eax
        add esp, P_TAM
        call aj_iniciar             ; [Configuración] CONFIG.HVP aplica VENTANAS.INI
        mov ebx, [gr_tabla]
        mov ecx, MAX_GR
.g:
        cmp byte [ebx+GR_USO], 0
        je .s
        movzx edx, byte [ebx+GR_ESTADO]
        test edx, edx
        jz .s
        mov eax, ebx
        call grupo_abrir
        mov ebp, [ebx+GR_VEN]
        test ebp, ebp
        jz .s
        cmp edx, 2
        jb .s
        mov eax, edx                ; 2 minimizado, 3 maximizado
        call ven_mostrar
.s:
        add ebx, GR_TAM
        dec ecx
        jnz .g
        mov eax, [cen_activo]       ; delante, la que lo estaba
        cmp eax, MAX_GR
        jae .centro
        shl eax, 6
        add eax, [gr_tabla]
        mov ebp, [eax+GR_VEN]
        test ebp, ebp
        jz .centro
        test dword [ebp+V_ESTADO], S_MINIM
        jz .act
.centro:
        mov ebp, [cen_ven]
.act:
        call ven_activar
        jmp app_bucle

; (interna) deja el rectángulo de la plantilla ESI dentro de la pantalla
rect_acotar:
        push eax
        push ebx
        mov eax, [esi+P_AN]
        cmp eax, MIN_AN
        jge .a1
        mov eax, MIN_AN
.a1:
        cmp eax, [vid_ancho]
        jle .a2
        mov eax, [vid_ancho]
.a2:
        mov [esi+P_AN], eax
        mov ebx, [vid_ancho]
        sub ebx, eax
        mov eax, [esi+P_X]
        cmp eax, ebx
        jle .a3
        mov eax, ebx
.a3:
        test eax, eax
        jns .a4
        xor eax, eax
.a4:
        mov [esi+P_X], eax
        mov ebx, [vid_alto]
        sub ebx, BARRA_H
        mov eax, [esi+P_AL]
        cmp eax, MIN_AL
        jge .b1
        mov eax, MIN_AL
.b1:
        cmp eax, ebx
        jle .b2
        mov eax, ebx
.b2:
        mov [esi+P_AL], eax
        mov ebx, [vid_alto]
        sub ebx, eax
        mov eax, [esi+P_Y]
        cmp eax, ebx
        jle .b3
        mov eax, ebx
.b3:
        cmp eax, BARRA_H
        jge .b4
        mov eax, BARRA_H
.b4:
        mov [esi+P_Y], eax
        pop ebx
        pop eax
        ret

; (interna) abre la ventana del grupo EAX (o la trae delante)
grupo_abrir:
        pushad
        mov ebx, eax
        mov ebp, [ebx+GR_VEN]
        test ebp, ebp
        jz .nueva
        test dword [ebp+V_ESTADO], S_MINIM
        jz .a
        mov eax, 4
        call ven_mostrar
.a:
        call ven_activar
        jmp .x
.nueva:
        sub esp, P_TAM
        mov esi, esp
        mov dword [esi+P_CLASE], c_grupo
        lea eax, [ebx+GR_NOMBRE]
        mov [esi+P_TEXTO], eax
        mov dword [esi+P_ESTILO], E_NORMAL
        mov eax, [ebx+GR_X]
        mov [esi+P_X], eax
        mov eax, [ebx+GR_Y]
        mov [esi+P_Y], eax
        mov eax, [ebx+GR_AN]
        mov [esi+P_AN], eax
        mov eax, [ebx+GR_AL]
        mov [esi+P_AL], eax
        mov eax, ebx
        call gr_num
        add eax, 100
        mov [esi+P_ID], eax
        call rect_acotar
        xor ebp, ebp
        call ven_crear
        add esp, P_TAM
        mov [ebx+GR_VEN], eax
        mov byte [ebx+GR_ESTADO], 1
        test eax, eax
        jz .x
        mov ecx, [ebx+GR_SEL]       ; con el icono que estaba marcado
        mov [eax+V_D0], ecx
        mov ebp, eax                ; si no caben todos los iconos (ha entrado
        call vista_cuenta           ; un programa nuevo), la ventana se alarga
        add eax, [vis_cols]
        dec eax
        xor edx, edx
        div dword [vis_cols]
        imul eax, eax, CEN_FILA
        add eax, 8
        sub eax, [vis_alto]
        jle .x
        mov edx, [ebp+V_AL]
        add edx, eax
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        mov ecx, [vid_alto]         ; (sin salirse de la pantalla)
        sub ecx, ebx
        cmp edx, ecx
        jle .cabe
        mov edx, ecx
.cabe:
        mov ecx, [ebp+V_AN]
        call ven_mover
.x:
        popad
        ret

; ---- la vista de iconos (el Centro: V_ID=0; un grupo: V_ID=100+número) ----
; V_D0 = el icono marcado
; (interna) columnas que caben en la ventana EBP -> vis_cols (y vis_alto)
vista_cols:
        push eax
        push ebx
        push ecx
        push edx
        call ven_cli
        mov [vis_alto], edx
        mov eax, ecx
        sub eax, 12
        jns .a
        xor eax, eax
.a:
        xor edx, edx
        mov ecx, CEN_PASO
        div ecx
        test eax, eax
        jnz .b
        inc eax
.b:
        mov [vis_cols], eax
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; (interna) EBP=ventana ECX=posición -> EAX=el grupo o el acceso (0: no hay)
vista_ficha:
        push ebx
        push ecx
        push edx
        test ecx, ecx
        js .no
        mov edx, [ebp+V_ID]
        sub edx, 100
        js .grupos
        mov eax, [ac_tabla]
        mov ebx, MAX_AC
.a:
        cmp byte [eax+AC_USO], 0
        je .as
        cmp [eax+AC_GRUPO], dl
        jne .as
        dec ecx
        js .x
.as:
        add eax, AC_TAM
        dec ebx
        jnz .a
        jmp .no
.grupos:
        mov eax, [gr_tabla]
        mov ebx, MAX_GR
.g:
        cmp byte [eax+GR_USO], 0
        je .gs
        dec ecx
        js .x
.gs:
        add eax, GR_TAM
        dec ebx
        jnz .g
.no:
        xor eax, eax
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) -> EAX=cuántos iconos tiene la ventana EBP
vista_cuenta:
        push ecx
        call vista_cols
        xor ecx, ecx
.b:
        call vista_ficha
        test eax, eax
        jz .x
        inc ecx
        jmp .b
.x:
        mov eax, ecx
        pop ecx
        ret

; (interna) -> EAX=lo marcado en la ventana EBP (0: nada)
vista_sel:
        push ecx
        mov ecx, [ebp+V_D0]
        call vista_ficha
        pop ecx
        ret

; (interna) EAX=x EBX=y en la zona de EBP -> EAX=posición del icono (-1: ninguno)
vista_en:
        push ebx
        push ecx
        push edx
        push esi
        call vista_cols
        sub eax, 12
        js .no
        xor edx, edx
        mov ecx, CEN_PASO
        div ecx
        cmp eax, [vis_cols]
        jae .no
        mov esi, eax
        test ebx, ebx
        js .no
        mov eax, ebx
        xor edx, edx
        mov ecx, CEN_FILA
        div ecx
        imul eax, [vis_cols]
        add eax, esi
        mov ecx, eax
        push eax
        call vista_ficha
        test eax, eax
        pop eax
        jnz .x
.no:
        mov eax, -1
.x:
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) todas las vistas, a repintar
vistas_repintar:
        pushad
        mov edi, hvi_de             ; (y los iconos de los .HVP se leen otra vez)
        mov ecx, HVI_N
        xor eax, eax
        rep stosd
        mov ebp, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [ebp+V_USO], 1
        jne .s
        cmp dword [ebp+V_PROC], vista_proc
        jne .s
        call ven_invalidar
.s:
        add ebp, V_TAM
        dec ecx
        jnz .b
        popad
        ret

vista_proc:
        cmp eax, WM_PAINT
        je vista_pintar
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_LBUTTONUP
        je .suelta
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
        cmp eax, WM_DESTROY
        je .destruir
        jmp ven_defecto
.crear:
        mov dword [ebp+V_MENU], menu_centro
        mov dword [ebp+V_ICONO], ico_centro
        cmp dword [ebp+V_ID], 0
        je .nada
        mov dword [ebp+V_ICONO], ico_carpeta
        jmp .nada
.pulsa:
        push eax
        mov dword [arr_acc], 0
        call raton_xy
        jc .p9
        call vista_en
        test eax, eax
        js .p9
        mov [ebp+V_D0], eax
        call ven_invalidar
        cmp dword [ebp+V_ID], 0
        je .p1
        call vista_sel
        mov [arr_acc], eax          ; se puede arrastrar a otro grupo
.p1:
        pop eax
        cmp eax, WM_LBUTTONDBLCLK
        jne .nada
        mov dword [arr_acc], 0
        call vista_abrir
        jmp .nada
.p9:
        pop eax
        jmp .nada
.suelta:
        mov esi, [arr_acc]
        test esi, esi
        jz .nada
        mov dword [arr_acc], 0
        push ebp
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call ges_donde              ; ¿dónde se ha soltado?
        mov edx, ebp
        pop ebp
        cmp edx, ebp
        je .nada
        cmp dword [edx+V_PROC], vista_proc
        jne .nada
        mov eax, [edx+V_ID]
        sub eax, 100
        jns .mueve                  ; en la ventana de otro grupo
        cmp ecx, Z_CLIENTE          ; en el Centro: sobre el icono de un grupo
        jne .nada
        push ebp
        mov ebp, edx
        call ven_cli
        mov ecx, [rat_x]
        sub ecx, eax
        mov eax, ecx
        mov ecx, [rat_y]
        sub ecx, ebx
        mov ebx, ecx
        call vista_en
        mov ecx, eax
        call vista_ficha
        pop ebp
        test eax, eax
        jz .nada
        call gr_num
.mueve:
        cmp al, [esi+AC_GRUPO]
        je .nada
        call acc_mover
        jmp .nada
.tecla:
        push ecx
        call vista_cuenta
        mov edi, eax
        pop ecx
        mov eax, [ebp+V_D0]
        cmp ecx, TC_DER
        jne .t1
        inc eax
        cmp eax, edi
        jb .tm
        xor eax, eax
        jmp .tm
.t1:
        cmp ecx, TC_IZQ
        jne .t2
        dec eax
        jns .tm
        lea eax, [edi-1]
        test eax, eax
        jns .tm
        xor eax, eax
        jmp .tm
.t2:
        cmp ecx, TC_ABAJO
        jne .t3
        add eax, [vis_cols]
        cmp eax, edi
        jb .tm
        jmp .nada
.t3:
        cmp ecx, TC_ARRIBA
        jne .t4
        sub eax, [vis_cols]
        jns .tm
        jmp .nada
.t4:
        cmp ecx, TC_INTRO
        jne .t5
        call vista_abrir
        jmp .nada
.t5:
        cmp ecx, TC_SUPR
        jne .nada
        call vista_borrar
        jmp .nada
.tm:
        mov [ebp+V_D0], eax
.inv:
        call ven_invalidar
        jmp .nada
.orden:
        cmp ecx, 210
        je .salir
        cmp ecx, 220
        je .acerca
        cmp ecx, 301
        jne .o2
        call vista_abrir
        jmp .nada
.o2:
        cmp ecx, 302
        jne .o3
        call acc_nuevo_dlg
        jmp .nada
.o3:
        cmp ecx, 303
        jne .o4
        call grupo_nuevo_dlg
        jmp .nada
.o4:
        cmp ecx, 304
        jne .o5
        call vista_propiedades
        jmp .nada
.o5:
        cmp ecx, 305
        jne .o6
        call vista_mover
        jmp .nada
.o6:
        cmp ecx, 306
        jne .o7
        call vista_borrar
        jmp .nada
.o7:
        cmp ecx, 307
        jne .nada
        call buscar_programas
        jmp .nada
.acerca:
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        call dlg_mensaje
        jmp .nada
.cerrar:
        cmp dword [ebp+V_ID], 0
        jne ven_defecto             ; un grupo: se cierra sin más
.salir:
        mov esi, t_salir_preg
        mov edi, tit_barra
        mov eax, DM_ACEPTAR_CANCELAR
        call dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne .nada
        mov byte [salir], 1
        jmp .nada
.destruir:
        mov eax, [ebp+V_ID]
        sub eax, 100
        js .d1
        shl eax, 6
        add eax, [gr_tabla]
        lea edi, [eax+GR_X]         ; dónde estaba, para la próxima vez
        push eax
        call ven_rect_normal
        pop eax
        mov ecx, [ebp+V_D0]
        mov [eax+GR_SEL], ecx
        mov dword [eax+GR_VEN], 0
        mov byte [eax+GR_ESTADO], 0
        jmp .nada
.d1:
        mov dword [cen_ven], 0
.nada:
        xor eax, eax
        ret

vista_pintar:
        call dc_pintar
        call vista_cuenta
        mov [vis_n], eax
        cmp eax, [ebp+V_D0]         ; lo marcado, dentro de lo que hay
        ja .ok
        dec eax
        jns .ok0
        xor eax, eax
.ok0:
        mov [ebp+V_D0], eax
.ok:
        xor edi, edi
.i:
        mov ecx, edi
        call vista_ficha
        test eax, eax
        jz .fin
        mov ebx, eax
        mov esi, ico_carpeta        ; el icono y el nombre
        lea edx, [ebx+GR_NOMBRE]
        cmp dword [ebp+V_ID], 0
        je .i0
        movzx eax, byte [ebx+AC_ICONO]
        cmp eax, ICO_N
        jb .ia
        xor eax, eax
.ia:
        mov esi, [ico_tabla+eax*8]
        call hvp_icono              ; (un .HVP trae el suyo)
        lea edx, [ebx+AC_NOMBRE]
.i0:
        mov [vis_nom], edx
        mov eax, edi
        xor edx, edx
        div dword [vis_cols]        ; EAX=fila EDX=columna
        imul eax, CEN_FILA
        mov [vis_y], eax
        imul edx, CEN_PASO
        add edx, 12
        mov [vis_x], edx
        lea ebx, [eax+16]
        lea eax, [edx+(CEN_PASO-32)/2]
        call dc_icono
        mov esi, [vis_nom]          ; el nombre, centrado (hasta 14 letras)
        call cad_largo
        cmp eax, 14
        jbe .i1
        mov eax, 14
.i1:
        mov ecx, eax
        shl eax, 2
        neg eax
        add eax, [vis_x]
        add eax, CEN_PASO / 2
        mov ebx, [vis_y]
        add ebx, 58
        COLOR C_TINTA
        cmp edi, [ebp+V_D0]
        jne .i3
        push eax
        push ecx
        COLOR C_ARENA_OSC
        cmp ebp, [foco]
        jne .i2
        COLOR C_ANIL
.i2:
        sub eax, 4
        lea ecx, [ecx*8+8]
        mov edx, 18
        dec ebx
        call dc_rect
        inc ebx
        pop ecx
        pop eax
        COLOR C_BLANCO
.i3:
        call dc_texto_n
        inc edi
        jmp .i
.fin:
        COLOR C_ARENA_OSC
        mov eax, 16
        cmp dword [ebp+V_ID], 0
        jne .grupo
        mov ebx, [vis_alto]         ; el Centro: la pista, abajo
        sub ebx, 43
        mov esi, t_centro_pista
        call dc_texto
        jmp .x
.grupo:
        cmp dword [vis_n], 0        ; un grupo vacío lo dice
        jne .x
        mov ebx, 16
        mov esi, t_grupo_vacio
        call dc_texto
        mov ebx, 36
        mov esi, t_grupo_vacio2
        call dc_texto
.x:
        call dc_cerrar
        xor eax, eax
        ret

; (interna) abre lo marcado: un grupo (su ventana) o un acceso (el programa)
vista_abrir:
        push eax
        call vista_sel
        test eax, eax
        jz .x
        cmp dword [ebp+V_ID], 0
        jne .acc
        call grupo_abrir
        jmp .x
.acc:
        call acc_lanzar
.x:
        pop eax
        ret

; (interna) el acceso ESI pasa al grupo EAX
acc_mover:
        mov [esi+AC_GRUPO], al
        call vistas_repintar
        jmp cfg_guardar

; (interna) pregunta con el texto de cen_txt: ZF=1 si se acepta
cen_preguntar:
        push esi
        push edi
        mov esi, cen_txt
        mov edi, t_centro
        mov eax, DM_ACEPTAR_CANCELAR
        call dlg_mensaje
        cmp eax, ID_ACEPTAR
        pop edi
        pop esi
        ret

; (interna) aviso con el texto ESI
cen_aviso:
        push eax
        push edi
        mov edi, t_centro
        mov eax, DM_ACEPTAR
        call dlg_mensaje
        pop edi
        pop eax
        ret

; (interna) borra lo marcado (pregunta antes)
vista_borrar:
        pushad
        call vista_sel
        test eax, eax
        jz .x
        mov ebx, eax
        mov edi, cen_txt
        cmp dword [ebp+V_ID], 0
        je .grupo
        mov esi, t_borrar_acc
        call pon_cad
        lea esi, [ebx+AC_NOMBRE]
        call pon_cad
        mov esi, t_borrar_fin
        call pon_cad
        mov byte [edi], 0
        call cen_preguntar
        jne .x
        mov byte [ebx+AC_USO], 0
        jmp .hecho
.grupo:
        mov esi, t_borrar_gr
        call pon_cad
        lea esi, [ebx+GR_NOMBRE]
        call pon_cad
        mov esi, t_borrar_gr2
        call pon_cad
        mov byte [edi], 0
        call cen_preguntar
        jne .x
        push ebp
        mov ebp, [ebx+GR_VEN]
        test ebp, ebp
        jz .g1
        call ven_destruir
.g1:
        pop ebp
        mov eax, ebx
        call gr_num
        mov esi, [ac_tabla]
        mov ecx, MAX_AC
.g2:
        cmp [esi+AC_GRUPO], al
        jne .g3
        mov byte [esi+AC_USO], 0
.g3:
        add esi, AC_TAM
        dec ecx
        jnz .g2
        mov byte [ebx+GR_USO], 0
.hecho:
        call vistas_repintar
        call cfg_guardar
.x:
        popad
        ret

; (interna) «Propiedades»: de un acceso (el diálogo) o de un grupo (su nombre)
vista_propiedades:
        pushad
        call vista_sel
        test eax, eax
        jz .x
        mov ebx, eax
        cmp dword [ebp+V_ID], 0
        je .grupo
        mov edi, t_ad_prop
        call acc_dialogo
        test eax, eax
        jz .x
        jmp .hecho
.grupo:
        lea esi, [ebx+GR_NOMBRE]
        mov edi, cen_nom
        mov ecx, 27
        call cad_copiar
        mov esi, t_gr_preg
        mov edi, t_gr_renombrar
        push ebx
        mov ebx, cen_nom
        mov ecx, 28
        call dlg_pedir
        pop ebx
        cmp eax, 1
        jne .x
        cmp byte [cen_nom], 0
        je .x
        mov esi, cen_nom
        lea edi, [ebx+GR_NOMBRE]
        mov ecx, 27
        call cad_copiar
        mov ebp, [ebx+GR_VEN]       ; el título de su ventana
        test ebp, ebp
        jz .hecho
        mov esi, edi
        call ven_texto
.hecho:
        call vistas_repintar
        call cfg_guardar
.x:
        popad
        ret

; (interna) «Nuevo grupo...»
grupo_nuevo_dlg:
        pushad
        mov byte [cen_nom], 0
        mov esi, t_gr_preg
        mov edi, t_gr_nuevo
        mov ebx, cen_nom
        mov ecx, 28
        call dlg_pedir
        cmp eax, 1
        jne .x
        cmp byte [cen_nom], 0
        je .x
        call gr_nuevo
        test eax, eax
        jnz .hay
        mov esi, t_no_caben
        call cen_aviso
        jmp .x
.hay:
        lea edi, [eax+GR_NOMBRE]
        mov esi, cen_nom
        mov ecx, 27
        call cad_copiar
        call vistas_repintar
        call grupo_abrir
        call cfg_guardar
.x:
        popad
        ret

; (interna) -> EAX=número del grupo sobre el que actúa la ventana EBP: el
; suyo, o el marcado en el Centro (-1 y un aviso si no hay)
vista_grupo:
        mov eax, [ebp+V_ID]
        sub eax, 100
        jns .x
        call vista_sel
        test eax, eax
        jz .no
        jmp gr_num
.no:
        push esi
        mov esi, t_sin_grupo
        call cen_aviso
        pop esi
        mov eax, -1
.x:
        ret

; (interna) «Nuevo acceso directo...»
acc_nuevo_dlg:
        pushad
        call vista_grupo
        test eax, eax
        js .x
        mov edx, eax
        call ac_nuevo
        test eax, eax
        jnz .hay
        mov esi, t_no_caben
        call cen_aviso
        jmp .x
.hay:
        mov ebx, eax
        mov edi, t_ad_nuevo
        call acc_dialogo
        test eax, eax
        jnz .si
        mov byte [ebx+AC_USO], 0
        jmp .x
.si:
        call vistas_repintar
        movzx eax, byte [ebx+AC_GRUPO]
        shl eax, 6
        add eax, [gr_tabla]
        call grupo_abrir
        call cfg_guardar
.x:
        popad
        ret

; (interna) «Mover a otro grupo...»
vista_mover:
        pushad
        cmp dword [ebp+V_ID], 0
        je .no
        call vista_sel
        test eax, eax
        jz .no
        mov esi, eax
        mov ebx, [gr_tabla]         ; los nombres de los grupos, a la lista
        xor ecx, ecx
        xor edx, edx
.g:
        cmp byte [ebx+GR_USO], 0
        je .gs
        push esi
        imul edi, ecx, 28
        add edi, cen_lista
        lea esi, [ebx+GR_NOMBRE]
        push ecx
        mov ecx, 27
        call cad_copiar
        pop ecx
        pop esi
        mov [cen_mapa+ecx], dl
        inc ecx
.gs:
        add ebx, GR_TAM
        inc edx
        cmp edx, MAX_GR
        jb .g
        push esi
        mov esi, t_mover_preg
        mov edi, t_mover_tit
        mov ebx, cen_lista
        mov edx, 28
        call dlg_lista
        pop esi
        test eax, eax
        js .x
        movzx eax, byte [cen_mapa+eax]
        cmp al, [esi+AC_GRUPO]
        je .x
        call acc_mover
        jmp .x
.no:
        mov esi, t_mover_no
        call cen_aviso
.x:
        popad
        ret

; ---- abrir un acceso directo ----
; EBP=ventana EAX=acceso
acc_lanzar:
        pushad
        mov ebx, eax
        lea esi, [ebx+AC_PROG]
        cmp byte [esi], ':'
        jne .externo
        xor ecx, ecx                ; un programa interno
.i:
        mov edi, [int_tabla+ecx]
        call cad_igual_i
        je .ihay
        add ecx, 12
        cmp ecx, INT_N * 12
        jb .i
        mov esi, t_no_interno
        jmp .aviso
.ihay:
        mov eax, [int_tabla+ecx+8]
        mov ebp, [eax]
        test ebp, ebp
        jz .inuevo
        mov eax, 4
        call ven_mostrar
        call ven_activar
        jmp .x
.inuevo:
        mov eax, [int_tabla+ecx+4]
        call tar_crear
        jmp .x
.externo:
        call es_hvp                 ; un programa de Hispa Ventanas (módulo)
        jne .ex
        call hvp_abrir
        jmp .x
.ex:
        cmp byte [esi], 0
        jne .e0
        mov esi, t_no_programa
        jmp .aviso
.e0:
        cmp word [lz_seg], 0
        jne .e1
        mov esi, t_sin_lanzador
        jmp .aviso
.e1:
        xor al, al                  ; ¿está el programa?
        call dos_abrir
        jnc .e2
        mov esi, t_no_esta
        jmp .aviso
.e2:
        push ebx
        mov ebx, eax
        call dos_cerrar
        pop ebx
        cmp dword [bloc_ven], 0     ; ¿texto sin guardar en el Bloc de notas?
        je .e3
        push ebp
        mov ebp, [bloc_edit]
        mov eax, EM_MODIFICADO
        call msg_enviar
        pop ebp
        test eax, eax
        jz .e3
        mov esi, t_lanzar_bloc
        mov edi, t_centro
        mov eax, DM_ACEPTAR_CANCELAR
        call dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne .x
.e3:
        movzx edx, word [lz_seg]    ; el encargo, a la memoria del lanzador
        shl edx, 4
        add edx, [cero]
        lea esi, [ebx+AC_PROG]
        lea edi, [edx+lz_prog]
        mov ecx, 79
        call cad_copiar
        lea esi, [ebx+AC_PARAM]
        lea edi, [edx+lz_param]
        call cad_copiar
        lea edi, [edx+lz_dir]
        lea esi, [ebx+AC_DIR]
        cmp byte [esi], 0
        jne .e4
        lea esi, [ebx+AC_PROG]      ; sin carpeta: la del programa
        call dir_de_ruta
        jmp .e5
.e4:
        mov ecx, 67
        call cad_copiar
        mov esi, edi
        call cad_largo              ; (la barra final sobra, salvo en la raíz)
        cmp eax, 3
        jbe .e5
        cmp byte [edi+eax-1], '\'
        jne .e5
        mov byte [edi+eax-1], 0
.e5:
        mov al, [ebx+AC_BAND]
        and al, AB_VOLVER | AB_PAUSA
        mov [edx+lz_band], al
        mov byte [edx+lz_orden], 1
        mov byte [salir], 2
        jmp .x
.aviso:
        call cen_aviso
.x:
        popad
        ret

; (interna) ESI=ruta de un archivo -> EDI=su carpeta («C:\» o «C:\JUEGOS\X»)
dir_de_ruta:
        pushad
        xor ecx, ecx
        mov edx, -1
.b:
        mov al, [esi+ecx]
        test al, al
        jz .f
        cmp al, '\'
        jne .s
        mov edx, ecx
.s:
        inc ecx
        jmp .b
.f:
        mov ecx, edx
        test ecx, ecx
        js .vacio
        cmp ecx, 2
        ja .c
        inc ecx                     ; la raíz, con su barra
.c:
        cmp ecx, 67
        jbe .c2
        mov ecx, 67
.c2:
        call cad_copiar
        popad
        ret
.vacio:
        mov byte [edi], 0
        popad
        ret

; (interna) ESI=ruta o nombre de archivo -> EDI=un nombre para el acceso: el
; del archivo sin extensión, con la primera letra en mayúscula
nombre_de_ruta:
        pushad
        mov edx, esi
.b:
        lodsb
        test al, al
        jz .f
        cmp al, '\'
        je .m
        cmp al, ':'
        jne .b
.m:
        mov edx, esi
        jmp .b
.f:
        mov esi, edx
        mov ecx, 31
        xor edx, edx
.c:
        lodsb
        test al, al
        jz .x
        cmp al, '.'
        je .x
        test edx, edx
        jz .c1
        cmp al, 'A'
        jb .c1
        cmp al, 'Z'
        ja .c1
        add al, 0x20
.c1:
        stosb
        inc edx
        dec ecx
        jnz .c
.x:
        mov byte [edi], 0
        popad
        ret

; ---- el diálogo de un acceso directo ----
; EBP=dueña EBX=acceso EDI=título -> EAX=1 si se acepta (y el acceso cambia)
acc_dialogo:
        pushad
        sub esp, P_TAM
        mov edx, ebx
        mov eax, edi
        mov edi, esp
        mov esi, ad_ventana
        mov ecx, P_TAM / 4
        rep movsd
        mov [esp+P_TEXTO], eax
        mov esi, esp
        mov edi, ad_hijos
        mov ebx, ad_gancho
        call dlg_modal
        add esp, P_TAM
        cmp eax, ID_ACEPTAR
        sete al
        movzx eax, al
        mov [esp+28], eax
        popad
        ret

; (internas) el texto del control ECX del diálogo EBP: ponerlo (EDX=texto) y
; leerlo (EDX=búfer EBX=tamaño)
ad_poner_txt:
        push eax
        push ebp
        call ven_hija_id
        mov ebp, eax
        mov eax, WM_SETTEXT
        call msg_enviar
        pop ebp
        pop eax
        ret
ad_leer_txt:
        push eax
        push ecx
        push ebp
        call ven_hija_id
        mov ebp, eax
        mov eax, WM_GETTEXT
        mov ecx, ebx
        call msg_enviar
        pop ebp
        pop ecx
        pop eax
        ret

; V_D3 = el acceso, V_D5 = el icono elegido
ad_gancho:
        mov edi, [ebp+V_D3]
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, DM_INICIO
        je .inicio
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, 120
        je .examinar
        cmp ecx, ID_ACEPTAR
        jne .no
        ; Aceptar: sin programa no vale
        mov ecx, 111
        mov edx, cen_txt
        mov ebx, 80
        call ad_leer_txt
        cmp byte [cen_txt], 0
        je .si
        mov esi, cen_txt
        push edi
        lea edi, [edi+AC_PROG]
        mov ecx, 79
        call cad_copiar
        pop edi
        mov ecx, 110
        lea edx, [edi+AC_NOMBRE]
        mov ebx, 32
        call ad_leer_txt
        cmp byte [edx], 0
        jne .a1
        push edi                    ; sin nombre: el del programa
        mov esi, cen_txt
        mov edi, edx
        call nombre_de_ruta
        pop edi
.a1:
        mov ecx, 112
        lea edx, [edi+AC_DIR]
        mov ebx, 68
        call ad_leer_txt
        mov ecx, 113
        lea edx, [edi+AC_PARAM]
        mov ebx, 72
        call ad_leer_txt
        mov ecx, 114
        call ro_marca
        mov bl, al
        mov ecx, 115
        call ro_marca
        shl al, 1
        or al, bl
        mov [edi+AC_BAND], al
        mov eax, [ebp+V_D5]
        mov [edi+AC_ICONO], al
        jmp .no
.examinar:
        mov byte [cen_arch], 0
        push edi
        mov edi, t_ad_elegir
        mov ebx, cen_arch
        call dlg_archivo
        pop edi
        cmp eax, 1
        jne .si
        ; la ruta entera, en cen_txt: lo escrito, si ya lleva la unidad; si
        ; no, detrás de la carpeta en la que se ha quedado el diálogo
        mov edi, cen_txt
        cmp byte [cen_arch+1], ':'
        je .x3
        call dos_dir_actual
        cmp byte [cen_arch], '\'
        jne .x1
        add edi, 2                  ; «\RUTA»: solo la unidad
        jmp .x3
.x1:
        mov esi, cen_txt
        call cad_largo
        lea edi, [cen_txt+eax]
        cmp byte [edi-1], '\'
        je .x3
        mov al, '\'
        stosb
.x3:
        mov esi, cen_arch
        call pon_cad
        mov byte [edi], 0
        mov ecx, 111
        mov edx, cen_txt
        call ad_poner_txt
        mov esi, cen_txt            ; y su carpeta, la de trabajo
        mov edi, cen_arch
        call dir_de_ruta
        mov ecx, 112
        mov edx, cen_arch
        call ad_poner_txt
        mov ecx, 110                ; el nombre, si aún no lo tiene
        mov edx, cen_nom
        mov ebx, 32
        call ad_leer_txt
        cmp byte [cen_nom], 0
        jne .si
        mov esi, cen_txt
        mov edi, cen_nom
        call nombre_de_ruta
        mov ecx, 110
        mov edx, cen_nom
        call ad_poner_txt
        jmp .si
.inicio:
        mov ecx, 110
        lea edx, [edi+AC_NOMBRE]
        call ad_poner_txt
        mov ecx, 111
        lea edx, [edi+AC_PROG]
        call ad_poner_txt
        mov ecx, 112
        lea edx, [edi+AC_DIR]
        call ad_poner_txt
        mov ecx, 113
        lea edx, [edi+AC_PARAM]
        call ad_poner_txt
        movzx edx, byte [edi+AC_BAND]
        and edx, 1
        mov ecx, 114
        call ro_poner
        movzx edx, byte [edi+AC_BAND]
        shr edx, 1
        and edx, 1
        mov ecx, 115
        call ro_poner
        movzx eax, byte [edi+AC_ICONO]
        mov [ebp+V_D5], eax
        jmp .no
.pulsa:
        call raton_xy               ; ¿en la fila de iconos?
        jc .no
        cmp ebx, 160
        jl .no
        cmp ebx, 200
        jge .no
        sub eax, 24
        js .no
        xor edx, edx
        mov ecx, 36
        div ecx
        cmp eax, ICO_N
        jae .no
        mov [ebp+V_D5], eax
        mov eax, 16
        mov ebx, 158
        mov ecx, 448
        mov edx, 44
        call ven_invalidar_rect
        jmp .si
.pintar:
        call dc_pintar
        xor edi, edi
.pi:
        imul eax, edi, 36
        add eax, 26
        mov ebx, 164
        mov esi, [ico_tabla+edi*8]
        call dc_icono
        cmp edi, [ebp+V_D5]
        jne .pi2
        COLOR C_ANIL
        sub eax, 3
        sub ebx, 3
        mov ecx, 38
        mov edx, 38
        call dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        call dc_marco
.pi2:
        inc edi
        cmp edi, ICO_N
        jb .pi
        call dc_cerrar
.si:
        mov eax, 1
        ret
.no:
        xor eax, eax
        ret

; ---- elegir de una lista ----
; parámetro: dd pregunta, textos, cuántos, elegido, octetos de uno al siguiente
dlg_lista:
        pushad
        sub esp, 20 + P_TAM
        mov [esp], esi
        mov [esp+4], ebx
        mov [esp+8], ecx
        mov dword [esp+12], -1
        mov [esp+16], edx
        mov eax, edi
        lea edi, [esp+20]
        mov esi, dl_ventana
        mov ecx, P_TAM / 4
        rep movsd
        mov [esp+20+P_TEXTO], eax
        lea esi, [esp+20]
        mov edi, dl_hijos
        mov ebx, dl_gancho
        mov edx, esp
        call dlg_modal
        mov ebx, [esp+12]
        add esp, 20 + P_TAM
        cmp eax, ID_ACEPTAR
        je .ok
        mov ebx, -1
.ok:
        mov [esp+28], ebx
        popad
        ret

dl_gancho:
        mov edi, [ebp+V_D3]
        cmp eax, DM_INICIO
        je .inicio
        cmp eax, WM_COMMAND
        jne .no
        cmp cx, 103
        je .lista
        cmp ecx, ID_ACEPTAR
        jne .no
        push ebp
        mov ecx, 103
        call ven_hija_id
        mov ebp, eax
        mov eax, LM_SELEC
        call msg_enviar
        pop ebp
        mov [edi+12], eax
        test eax, eax
        js .si                      ; nada elegido: no se acepta
        jmp .no
.lista:
        shr ecx, 16
        cmp ecx, LN_DOBLE
        jne .no
        mov eax, WM_COMMAND
        mov ecx, ID_ACEPTAR
        xor edx, edx
        call msg_enviar
        jmp .si
.inicio:
        mov ecx, 100
        call ven_hija_id
        mov ebx, [edi]
        mov [eax+V_TEXTO], ebx
        mov ecx, 103
        call ven_hija_id
        push ebp
        mov ebp, eax
        mov edx, [edi+4]
        mov ebx, [edi+8]
.a:
        test ebx, ebx
        jz .af
        mov eax, LM_ANADIR
        call msg_enviar
        add edx, [edi+16]
        dec ebx
        jmp .a
.af:
        mov eax, LM_PONER_SELEC
        xor ecx, ecx
        call msg_enviar
        pop ebp
.no:
        xor eax, eax
        ret
.si:
        mov eax, 1
        ret

; ---- «Buscar programas...» ----
; Recorre las subcarpetas de una carpeta (C:\JUEGOS) y crea un acceso directo
; al programa principal de cada una: el que se llama como la carpeta; si no,
; el único .EXE (si no hay ninguno, el único .COM, y si no, el único .BAT),
; sin contar los de instalar y configurar (bp_no); y si hay varios, pregunta.
buscar_programas:
        pushad
        call vista_grupo
        test eax, eax
        js .x
        mov [bp_grupo], eax
        cmp byte [bp_dir], 0
        jne .p
        mov esi, t_bp_defecto       ; \JUEGOS de la unidad de Hispa Ventanas
        mov edi, bp_dir
        mov ecx, 63
        call cad_copiar
        mov al, [dir_propia]
        cmp byte [dir_propia+1], ':'
        jne .p
        mov [bp_dir], al
.p:
        mov esi, t_bp_preg
        mov edi, t_bp_tit
        mov ebx, bp_dir
        mov ecx, 64
        call dlg_pedir
        cmp eax, 1
        jne .x
        mov esi, bp_dir
        call cad_largo
        test eax, eax
        jz .x
        cmp byte [bp_dir+eax-1], '\'
        jne .p2
        mov byte [bp_dir+eax-1], 0
.p2:
        mov edi, cen_txt            ; primero, la lista de las subcarpetas
        mov esi, bp_dir
        call pon_cad
        mov esi, t_bp_todo
        call pon_cad
        mov byte [edi], 0
        mov edi, bp_dirs
        xor ebx, ebx
        mov esi, cen_txt
        mov ecx, 0x10
        call dos_primero
.d:
        jc .df
        test byte [dos_dta+0x15], 0x10
        jz .ds
        cmp byte [dos_dta+0x1E], '.'
        je .ds
        cmp ebx, BP_MAXDIR
        jae .df
        mov esi, dos_dta + 0x1E
        mov ecx, 12
        call cad_copiar
        add edi, 13
        inc ebx
.ds:
        call dos_siguiente
        jmp .d
.df:
        mov [bp_ndir], ebx
        mov edi, cen_arch           ; los programas de Hispa Ventanas (.HVP)
        mov esi, bp_dir             ; que haya en la carpeta misma
        call pon_cad
        mov word [edi], '\'
        mov ecx, 1
        mov edx, [bp_grupo]
        call hvp_buscar
        mov [bp_nuevos], eax
        xor ebx, ebx
.c:
        cmp ebx, [bp_ndir]
        jae .fin
        imul esi, ebx, 13
        add esi, bp_dirs
        call bp_carpeta
        inc ebx
        jmp .c
.fin:
        call vistas_repintar
        call cfg_guardar
        mov edi, cen_txt
        mov esi, t_bp_hecho
        call pon_cad
        mov eax, [bp_nuevos]
        call pon_num
        mov byte [edi], 0
        mov esi, cen_txt
        mov edi, t_bp_tit
        mov eax, DM_ACEPTAR
        call dlg_mensaje
.x:
        popad
        ret

; (interna) EDI = cen_txt con «carpeta\subcarpeta» y sigue
bp_ruta:
        mov edi, cen_txt
        mov esi, bp_dir
        call pon_cad
        mov al, '\'
        stosb
        mov esi, [bp_sub]
        jmp pon_cad

; (interna) la subcarpeta ESI: busca su programa principal y crea el acceso
bp_carpeta:
        pushad
        mov [bp_sub], esi
        xor ebx, ebx                ; candidatos: los .EXE, los .COM y los .BAT
        xor edx, edx
.t:
        call bp_ruta
        mov esi, [bp_pat+edx*4]
        call pon_cad
        mov byte [edi], 0
        mov esi, cen_txt
        xor ecx, ecx
        call dos_primero
.f:
        jc .ft
        cmp ebx, BP_MAXCAND
        jae .ft
        test byte [dos_dta+0x15], 0x18
        jnz .fs
        imul edi, ebx, 13
        add edi, cen_lista
        mov esi, dos_dta + 0x1E
        mov ecx, 12
        call cad_copiar
        mov [cen_mapa+ebx], dl
        inc ebx
.fs:
        call dos_siguiente
        jmp .f
.ft:
        inc edx
        cmp edx, 3
        jb .t
        test ebx, ebx
        jz .x
        mov [bp_n], ebx
        xor ecx, ecx                ; 1) el que se llama como la carpeta
.n:
        imul esi, ecx, 13
        add esi, cen_lista
        mov edi, [bp_sub]
        call bp_mismo
        je .elegido
        inc ecx
        cmp ecx, ebx
        jb .n
        xor edx, edx                ; 2) el único de su clase
.u:
        call bp_unico
        cmp eax, 1
        je .elegido
        ja .preguntar
        inc edx
        cmp edx, 3
        jb .u
        jmp .x
.preguntar:
        mov edi, cen_txt            ; 3) varios: que lo diga el usuario
        mov esi, t_bp_cual
        call pon_cad
        mov esi, [bp_sub]
        call pon_cad
        mov ax, ':'
        stosw
        mov esi, cen_txt
        mov edi, t_bp_tit
        mov ebx, cen_lista
        mov ecx, [bp_n]
        mov edx, 13
        call dlg_lista
        test eax, eax
        js .x
        mov ecx, eax
.elegido:
        call bp_ruta
        mov al, '\'
        stosb
        imul esi, ecx, 13
        add esi, cen_lista
        call pon_cad
        mov byte [edi], 0
        mov eax, [ac_tabla]         ; ¿ya hay un acceso a ese programa?
        mov edx, MAX_AC
.r:
        cmp byte [eax+AC_USO], 0
        je .rs
        lea esi, [eax+AC_PROG]
        mov edi, cen_txt
        call cad_igual_i
        je .x
.rs:
        add eax, AC_TAM
        dec edx
        jnz .r
        mov edx, [bp_grupo]
        call ac_nuevo
        test eax, eax
        jz .x
        mov ebx, eax
        lea edi, [ebx+AC_PROG]
        mov esi, cen_txt
        mov ecx, 79
        call cad_copiar
        lea edi, [ebx+AC_DIR]
        call dir_de_ruta
        mov esi, [bp_sub]
        lea edi, [ebx+AC_NOMBRE]
        call nombre_de_ruta
        mov byte [ebx+AC_ICONO], 1  ; el icono de juego
        inc dword [bp_nuevos]
.x:
        popad
        ret

; (interna) ZF=1 si el archivo ESI («DOOM.EXE») se llama como la carpeta EDI
bp_mismo:
        push eax
        push esi
        push edi
.c:
        mov al, [esi]
        mov ah, [edi]
        cmp al, '.'
        jne .a
        xor al, al
.a:
        cmp ah, '.'
        jne .b
        xor ah, ah
.b:
        cmp al, ah
        jne .x
        inc esi
        inc edi
        test al, al
        jnz .c
.x:
        pop edi
        pop esi
        pop eax
        ret

; (interna) EDX=clase (0 .EXE, 1 .COM, 2 .BAT) -> EAX=cuántos candidatos hay
; de esa clase sin contar los de instalar, ECX=el último de ellos
bp_unico:
        push ebx
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
.b:
        cmp ebx, [bp_n]
        jae .x
        cmp [cen_mapa+ebx], dl
        jne .s
        imul esi, ebx, 13
        add esi, cen_lista
        call bp_excluido
        je .s
        inc eax
        mov ecx, ebx
.s:
        inc ebx
        jmp .b
.x:
        pop edi
        pop esi
        pop ebx
        ret

; (interna) ZF=1 si el nombre ESI empieza como alguno de bp_no
bp_excluido:
        push eax
        push ecx
        push edi
        mov edi, bp_no
.t:
        cmp byte [edi], 0
        je .no
        xor ecx, ecx
.c:
        mov al, [edi+ecx]
        test al, al
        jz .si
        cmp al, [esi+ecx]
        jne .otra
        inc ecx
        jmp .c
.otra:
        inc edi
        cmp byte [edi-1], 0
        jne .otra
        jmp .t
.no:
        or edi, edi                 ; (EDI no es 0: ZF=0)
        jmp .x
.si:
        xor eax, eax                ; ZF=1
.x:
        pop edi
        pop ecx
        pop eax
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
