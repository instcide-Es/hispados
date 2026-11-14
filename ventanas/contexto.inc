; =============================================================================
;  Hispa Ventanas - [CONTEXTO] el botón derecho: menús contextuales
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Este archivo va en la «parte alta» de VENTANAS.COM (ventanas.asm): solo
;  código y datos de 32 bits. Pedido de Carlos: «No tenemos botón secundario
;  en Ventanas».
;
;  El menú emergente es el mismo desplegable de menus.inc (la misma clase,
;  las mismas variables menu_*), abierto donde está el ratón en lugar de
;  debajo de un título de la barra, con ctx_activo = 1. Se maneja igual
;  (flechas, Intro, Esc, la letra subrayada, el ratón; fuera de él se cierra)
;  y puede llevar órdenes en gris (ctx_gris: un bit por orden).
;
;  Cómo llega: al soltar el botón derecho sobre una ventana, ven_defecto
;  (lo que no atiende la ventana) le manda WM_CONTEXTMENU (ECX = la ventana,
;  EDX = x + y*65536 de la pantalla); una hija que no lo quiere lo pasa a su
;  madre. Con el teclado (Mayús+F10 o la tecla de menú), WM_CONTEXTMENU va a
;  la ventana con el foco con EDX = -1. Quien quiere un menú lo abre con
;  ctx_menu; la orden elegida le llega como WM_COMMAND (ECX = número), igual
;  que las de la barra de menús. Una ventana de primer nivel que no lo
;  atiende, con el teclado, abre el menú de la ventana (el de la barra de
;  título).
;
;    ctx_menu        (interfaz 84) EBP=dueña ESI=órdenes (como un desplegable:
;                    dw número, db texto, 0 ... dw 0xFFFF) EAX=x EBX=y de la
;                    pantalla (EAX=-1: donde se ha pedido) ECX=las que van en
;                    gris (bit n = la orden n, contando las rayas)
;    ctx_sistema     EBP=ventana EAX=x EBX=y (-1: bajo el título): Restaurar,
;                    Minimizar, Maximizar y Cerrar, las que tenga
;    ctx_raton_der   (gestor, ges_raton_der) el botón derecho, BH=1/0
;    ctx_ctrl_clic   (eventos) Ctrl+clic izquierdo = clic derecho (BL, BH)
;    ctx_tecla       (ges_tecla) Mayús+F10 -> CF=1
;    ctx_tecla_menu  (eventos) la tecla de menú del teclado (tec_mapa 5Dh)
;    ctx_def_rup, ctx_def_menu   (ven_defecto) WM_RBUTTONUP y WM_CONTEXTMENU
;    ctx_escritorio, ctx_esc_orden  (esc_proc) el menú del escritorio
;    ctx_vista, ctx_vista_orden     (vista_proc) iconos y fondo del Centro y
;                    de los grupos
;    ctx_edit, ctx_ed_orden         (ed_proc) los cuadros de texto
;    ctx_tinta, ctx_filtrar, ctx_saltable, ctx_letra, ctx_cerrado
;                    (menus.inc) el gris, las letras y el cierre
; =============================================================================

SC_RESTAURAR equ 0xF120             ; las órdenes del menú de la ventana (los
SC_MINIMIZAR equ 0xF020             ; números de SC_* de Windows)
SC_MAXIMIZAR equ 0xF030
SC_CERRAR   equ 0xF060
CE_CONFIG   equ 0xE101              ; el escritorio
CE_FONDO    equ 0xE102
CE_MEZ      equ 0xE103
CE_CASCADA  equ 0xE104
CE_MOSAICO  equ 0xE105
CE_ARCH     equ 0xE106              ; [ARCHIVOS] el Administrador de archivos
CT_CORTAR   equ 0xE201              ; los cuadros de texto
CT_COPIAR   equ 0xE202
CT_PEGAR    equ 0xE203
CT_BORRAR   equ 0xE204
CT_TODO     equ 0xE205
CV_NOMBRE   equ 310                 ; el Centro y los grupos (las demás, las
CV_ORGANIZAR equ 311                ; del menú Archivo: 301-308)
WM_APARTADO equ WM_USER + 61        ; (a CONFIG.HVP) ECX=apartado que enseña
CASC_PASO   equ 26

; ---- las listas (el formato de un desplegable de menus.inc) ----
ctx_l_acc:                          ; un acceso directo
        dw 301
        db "&Abrir", 0
        dw 0
        db 0
        dw 304
        db "&Propiedades...", 0
        dw CV_NOMBRE
        db "Cambiar &nombre...", 0
        dw 305
        db "&Mover a otro grupo...", 0
        dw 0
        db 0
        dw 306
        db "&Eliminar", 0
        dw 0xFFFF
ctx_l_grupo:                        ; un grupo, en el Centro
        dw 301
        db "&Abrir", 0
        dw 0
        db 0
        dw 304
        db "&Propiedades...", 0
        dw CV_NOMBRE
        db "Cambiar &nombre...", 0
        dw 0
        db 0
        dw 306
        db "&Eliminar", 0
        dw 0xFFFF
ctx_l_fgrupo:                       ; el fondo de la ventana de un grupo
        dw 302
        db "Nuevo &acceso directo...", 0
        dw 307
        db "&Buscar programas...", 0
        dw 0
        db 0
        dw CV_ORGANIZAR
        db "&Organizar iconos", 0
        dw 0xFFFF
ctx_l_fcentro:                      ; el fondo del Centro
        dw 303
        db "Nuevo &grupo...", 0
        dw 307
        db "&Buscar programas...", 0
        dw 0xFFFF
ctx_l_esc:                          ; el escritorio
        dw CE_CONFIG
        db "&Configuración...", 0
        dw CE_FONDO
        db "&Fondo del escritorio...", 0
        dw CE_MEZ
        db "&Mezclador...", 0
        dw CE_ARCH
        db "Administrador de a&rchivos...", 0
        dw 0
        db 0
        dw CE_CASCADA
        db "Ventanas en c&ascada", 0
        dw CE_MOSAICO
        db "Ventanas en m&osaico", 0
        dw 0xFFFF
ctx_l_edit:                         ; un cuadro de texto (no tiene deshacer)
        dw CT_CORTAR
        db "Cor&tar           Ctrl+X", 0
        dw CT_COPIAR
        db "&Copiar           Ctrl+C", 0
        dw CT_PEGAR
        db "&Pegar            Ctrl+V", 0
        dw CT_BORRAR
        db "&Borrar           Supr", 0
        dw 0
        db 0
        dw CT_TODO
        db "&Seleccionar todo Ctrl+A", 0
        dw 0xFFFF
t_ctx_rest  db "&Restaurar", 0
t_ctx_min   db "Mi&nimizar", 0
t_ctx_max   db "Ma&ximizar", 0
t_ctx_cer   db "&Cerrar           Alt+F4", 0
t_ctx_nom   db "Nombre nuevo del acceso directo:", 0
t_ctx_ren   db "Cambiar el nombre", 0

; =============================================================================
;  El menú emergente
; =============================================================================
ctx_menu:
        pushad
        mov [ctx_gris_q], ecx
        mov [ctx_qx], eax
        mov [ctx_qy], ebx
        mov al, [ctx_q_sis]
        mov [ctx_q_sis2], al
        mov byte [ctx_q_sis], 0
        call menu_cerrar
        cmp dword [ctx_qx], -1
        jne .p
        mov eax, [ctx_px]
        mov [ctx_qx], eax
        mov eax, [ctx_py]
        mov [ctx_qy], eax
.p:
        mov [menu_items], esi
        xor edi, edi                ; ancho, alto y número de órdenes
        xor ebx, ebx
        xor ecx, ecx
.m:
        cmp word [esi], 0xFFFF
        je .mf
        add ebx, MENU_SEP_H
        cmp word [esi], 0
        je .m2
        add ebx, MENU_IT_H - MENU_SEP_H
.m2:
        add esi, 2
        call menu_ancho_txt
        cmp eax, edi
        jbe .m3
        mov edi, eax
.m3:
        inc ecx
        jmp .m
.mf:
        test ecx, ecx
        jz .x
        mov [menu_n], ecx
        mov dword [menu_plant+P_CLASE], c_menu
        mov dword [menu_plant+P_ESTILO], E_EMERGENTE
        lea ecx, [edi*8+34]
        mov [menu_plant+P_AN], ecx
        add ebx, 6
        mov [menu_plant+P_AL], ebx
        mov eax, [ctx_qx]           ; si no cabe a la derecha del punto, a su
        mov edx, [vid_ancho]        ; izquierda (y si tampoco, pegado al borde)
        call ctx_acotar
        mov [menu_plant+P_X], eax
        mov eax, [ctx_qy]           ; lo mismo, arriba y abajo
        mov ecx, ebx
        mov edx, [vid_alto]
        call ctx_acotar
        mov [menu_plant+P_Y], eax
        mov [menu_duena], ebp
        mov dword [menu_idx], -1
        mov dword [menu_sel], -1
        mov eax, [ctx_gris_q]
        mov [ctx_gris], eax
        mov al, [ctx_q_sis2]
        mov [ctx_sis], al
        mov byte [ctx_activo], 1
        push ebp
        mov esi, menu_plant
        xor ebp, ebp
        call ven_crear
        pop ebp
        mov [menu_ven], eax
        test eax, eax
        jnz .ok
        mov dword [menu_duena], 0
        mov byte [ctx_activo], 0
        mov byte [ctx_sis], 0
        jmp .ok2
.ok:
        mov eax, WM_ENTERMENULOOP   ; a la dueña, como los de la barra
        call menu_avisar
.ok2:
        mov byte [actividad], 1
.x:
        popad
        ret

; (interna) EAX=posición ECX=tamaño EDX=tamaño de la pantalla -> EAX, dentro
ctx_acotar:
        push ebx
        lea ebx, [eax+ecx+SOMBRA]
        cmp ebx, edx
        jle .a
        sub eax, ecx                ; al otro lado del punto
        jns .a
        mov eax, edx
        sub eax, ecx
        sub eax, SOMBRA
.a:
        test eax, eax
        jns .x
        xor eax, eax
.x:
        pop ebx
        ret

; (menu_cerrar) se ha cerrado el desplegable o el emergente
ctx_cerrado:
        mov byte [ctx_activo], 0
        mov byte [ctx_sis], 0
        mov dword [ctx_gris], 0
        ret

; (interna) ECX=número de orden -> CF=1 si va en gris
ctx_es_gris:
        cmp byte [ctx_activo], 0
        je .no
        cmp ecx, 32
        jae .no
        bt [ctx_gris], ecx
        ret
.no:
        clc
        ret

; (menu_proc) el color de la orden EDI: tinta, o gris si no se puede elegir
ctx_tinta:
        COLOR C_TINTA
        push ecx
        mov ecx, edi
        call ctx_es_gris
        pop ecx
        jnc .x
        COLOR C_ARENA_OSC
.x:
        ret

; (menu_marcar) EAX=orden que se quiere marcar -> -1 si va en gris
ctx_filtrar:
        test eax, eax
        js .x
        push ecx
        mov ecx, eax
        call ctx_es_gris
        pop ecx
        jnc .x
        mov eax, -1
.x:
        ret

; (menu_tecla, flechas) ESI=orden ECX=su número -> ZF=1 si no se puede
; marcar (una raya o una en gris)
ctx_saltable:
        cmp word [esi], 0
        je .x
        call ctx_es_gris
        jnc .si
        cmp eax, eax                ; ZF=1
        ret
.si:
        test esp, esp               ; ZF=0
.x:
        ret

; (menu_tecla) AL=carácter: la orden que tiene esa letra subrayada se elige
; (vale también en los desplegables de la barra)
ctx_letra:
        pushad
        call ctx_mayuscula
        mov bl, al
        mov esi, [menu_items]
        xor ecx, ecx
.o:
        cmp word [esi], 0xFFFF
        je .x
        cmp word [esi], 0
        je .sig
        call ctx_es_gris
        jc .sig
        lea edi, [esi+2]
.c:
        mov al, [edi]
        inc edi
        test al, al
        jz .sig
        cmp al, '&'
        jne .c
        mov al, [edi]
        call ctx_mayuscula
        cmp al, bl
        jne .sig
        mov eax, ecx
        call menu_marcar
        call menu_elegir
        jmp .x
.sig:
        add esi, 2
.s2:
        lodsb
        test al, al
        jnz .s2
        inc ecx
        jmp .o
.x:
        popad
        ret

; (interna) AL -> AL en mayúscula (también las vocales con tilde y la eñe de
; la página 437)
ctx_mayuscula:
        cmp al, 'a'
        jb .t
        cmp al, 'z'
        ja .t
        sub al, 0x20
        ret
.t:
        cmp al, 0xA4                ; ñ
        jne .t1
        mov al, 0xA5
        ret
.t1:
        cmp al, 0xA0                ; á
        jne .t2
        mov al, 'A'
.t2:
        cmp al, 0x82                ; é
        jne .t3
        mov al, 'E'
.t3:
        cmp al, 0xA1                ; í
        jne .t4
        mov al, 'I'
.t4:
        cmp al, 0xA2                ; ó
        jne .t5
        mov al, 'O'
.t5:
        cmp al, 0xA3                ; ú
        jne .x
        mov al, 'U'
.x:
        ret

; =============================================================================
;  El menú de la ventana (el de la barra de título)
; =============================================================================
; (interna) empieza una lista en ctx_lista
ctx_lista_ini:
        mov dword [ctx_lp], ctx_lista
        mov dword [ctx_lgris], 0
        mov dword [ctx_ln], 0
        mov word [ctx_lista], 0xFFFF
        ret

; (interna) ECX=número (0: una raya) ESI=texto AL=1 si va en gris
ctx_lista_pon:
        pushad
        mov edi, [ctx_lp]
        mov [edi], cx
        add edi, 2
        test cx, cx
        jnz .t
        mov byte [edi], 0
        inc edi
        jmp .f
.t:
        movsb
        cmp byte [esi-1], 0
        jne .t
.f:
        mov word [edi], 0xFFFF
        mov [ctx_lp], edi
        test al, al
        jz .ng
        mov ecx, [ctx_ln]
        bts [ctx_lgris], ecx
.ng:
        inc dword [ctx_ln]
        popad
        ret

ctx_sistema:
        pushad
        mov [ctx_sx], eax
        mov [ctx_sy], ebx
        call ctx_lista_ini
        mov edx, [ebp+V_ESTADO]
        mov ebx, [ebp+V_ESTILO]
        test ebx, E_MAXIM | E_MINIM
        jz .s1
        mov ecx, SC_RESTAURAR       ; Restaurar: si está minimizada o maximizada
        mov esi, t_ctx_rest
        test edx, S_MINIM | S_MAXIM
        setz al
        call ctx_lista_pon
.s1:
        test ebx, E_MINIM
        jz .s2
        mov ecx, SC_MINIMIZAR
        mov esi, t_ctx_min
        test edx, S_MINIM
        setnz al
        call ctx_lista_pon
.s2:
        test ebx, E_MAXIM
        jz .s3
        mov ecx, SC_MAXIMIZAR       ; (una minimizada se puede maximizar)
        mov esi, t_ctx_max
        mov eax, edx
        and eax, S_MINIM | S_MAXIM
        cmp eax, S_MAXIM
        sete al
        call ctx_lista_pon
.s3:
        test ebx, E_CERRAR
        jz .s4
        cmp dword [ctx_ln], 0
        je .s3b
        xor ecx, ecx
        xor al, al
        call ctx_lista_pon
.s3b:
        mov ecx, SC_CERRAR
        mov esi, t_ctx_cer
        xor al, al
        call ctx_lista_pon
.s4:
        cmp dword [ctx_ln], 0
        je .x
        mov eax, [ctx_sx]
        mov ebx, [ctx_sy]
        cmp eax, -1
        jne .m
        mov eax, [ebp+V_X]          ; debajo del título, a la izquierda
        add eax, BORDE
        mov ebx, [ebp+V_Y]
        add ebx, BORDE + TIT_H
.m:
        mov byte [ctx_q_sis], 1
        mov esi, ctx_lista
        mov ecx, [ctx_lgris]
        call ctx_menu
.x:
        popad
        ret

; (menu_elegir) ECX=orden del menú de la ventana EBP: se hace ya, como los
; botones de la barra de título
ctx_sis_orden:
        pushad
        cmp ecx, SC_CERRAR
        jne .s1
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        call msg_poner
        jmp .x
.s1:
        cmp ecx, SC_MINIMIZAR
        jne .s2
        mov eax, 2
        call ven_mostrar
        jmp .x
.s2:
        cmp ecx, SC_MAXIMIZAR
        jne .s3
        test dword [ebp+V_ESTADO], S_MINIM
        jz .s2b
        mov eax, 4
        call ven_mostrar
.s2b:
        mov eax, 3
        call ven_mostrar
        jmp .x
.s3:
        cmp ecx, SC_RESTAURAR
        jne .x
        mov eax, 4
        call ven_mostrar
        test dword [ebp+V_ESTADO], S_MAXIM
        jz .x
        call ven_mostrar            ; (estaba minimizada y maximizada)
.x:
        popad
        ret

; =============================================================================
;  El ratón y el teclado
; =============================================================================
; el botón derecho (BH=1 apretado, 0 soltado). Con un menú abierto: dentro,
; marca (al soltar se elige); fuera, lo cierra y sigue. En la barra de título
; (o sus botones), el menú de la ventana. En la zona de una ventana,
; WM_RBUTTONDOWN a la que hay debajo (que se activa), que se queda con el
; ratón hasta que se suelta y recibe WM_RBUTTONUP (ven_defecto lo convierte
; en WM_CONTEXTMENU)
ctx_raton_der:
        pushad
        test bh, bh
        jz .sube
        mov eax, [rat_x]
        mov ebx, [rat_y]
        cmp dword [menu_ven], 0
        je .nm
        call menu_en
        jc .fuera
        call menu_marcar
        mov byte [ctx_der_menu], 1
        jmp .x
.fuera:
        call menu_cerrar
        mov eax, [rat_x]
        mov ebx, [rat_y]
.nm:
        cmp dword [captura], 0
        jne .x
        cmp byte [arrastre], 0
        jne .x
        call ges_donde
        test dword [ebp+V_ESTADO], S_INACTIVA
        jnz .x
        cmp ecx, Z_CLIENTE
        je .cliente
        cmp ecx, Z_TITULO
        jb .x
        cmp ecx, Z_MINIM
        ja .x
        call ven_activar            ; el título o uno de sus botones
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call ctx_sistema
        mov byte [ctx_der_menu], 1
        jmp .x
.cliente:
        test dword [edi+V_ESTADO], S_INACTIVA
        jnz .x
        cmp ebp, [escritorio]
        je .c1
        test dword [ebp+V_ESTILO], E_TITULO
        jz .c1
        cmp ebp, [activa]
        je .c1
        call ven_activar
.c1:
        mov eax, WM_RBUTTONDOWN
        call ges_msg_raton
        mov [captura], edi
        mov byte [cap_der], 1
        jmp .x
.sube:
        cmp byte [ctx_der_menu], 0
        je .s1
        mov byte [ctx_der_menu], 0
        cmp dword [menu_ven], 0
        je .x
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call menu_raton_suelta      ; encima de una orden: esa
        jmp .x
.s1:
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

; (eventos) BL=botón izquierdo BH=derecho, ya con los botones de los zurdos:
; un clic izquierdo que empieza con Ctrl apretado es un clic derecho hasta
; que se suelta (los Mac y los touchpads de un botón). Se quita en
; Configuración (AJ_CTRL_DER)
ctx_ctrl_clic:
        cmp byte [ctx_ctrl], 0
        jne .dentro
        test bl, bl
        jz .x
        cmp byte [rat_boton], 0     ; (el izquierdo ya estaba apretado)
        jne .x
        test byte [tec_mayus], TM_CTRL
        jz .x
        cmp byte [aj_ctrl_no], 0
        jne .x
        mov byte [ctx_ctrl], 1
.dentro:
        test bl, bl
        jnz .m
        mov byte [ctx_ctrl], 0
.m:
        or bh, bl
        xor bl, bl
.x:
        ret

; (ges_tecla) AX=tecla: Mayús+F10 pide el menú contextual -> CF=1
ctx_tecla:
        test al, al
        jnz .no
        cmp ah, 0x5D
        jne .no
        call ctx_teclado
        stc
        ret
.no:
        clc
        ret

; (eventos) la tecla de menú (la de la lista, junto a Ctrl: código E0 5Dh),
; que la BIOS no da como tecla: se mira en tec_mapa
ctx_tecla_menu:
        pushad
        mov al, [tec_mapa+0x5D]
        cmp al, [ctx_tmenu]
        je .x
        mov [ctx_tmenu], al
        test al, al
        jz .x
        call ev_act
        cmp dword [menu_ven], 0
        je .abre
        call menu_cerrar
        jmp .x
.abre:
        call ctx_teclado
.x:
        popad
        ret

; (interna) WM_CONTEXTMENU con EDX=-1 a la ventana con el foco (o la activa,
; o el escritorio); el menú saldrá en su zona, arriba a la izquierda
ctx_teclado:
        pushad
        mov ebp, [foco]
        test ebp, ebp
        jnz .t
        mov ebp, [activa]
        test ebp, ebp
        jnz .t
        mov ebp, [escritorio]
.t:
        call ven_tope
        test dword [eax+V_ESTADO], S_INACTIVA
        jnz .x
        call ven_cli
        add eax, 8
        add ebx, 8
        mov [ctx_px], eax
        mov [ctx_py], ebx
        mov eax, WM_CONTEXTMENU
        mov ecx, ebp
        mov edx, -1
        call msg_poner
.x:
        popad
        ret

; (ven_defecto) WM_RBUTTONUP que no ha querido la ventana: WM_CONTEXTMENU
; con el punto en la pantalla
ctx_def_rup:
        movsx esi, dx
        sar edx, 16
        mov edi, edx
        call ven_cli
        add eax, esi
        add ebx, edi
        mov [ctx_px], eax
        mov [ctx_py], ebx
        shl ebx, 16
        mov bx, ax
        mov edx, ebx
        mov ecx, ebp
        mov eax, WM_CONTEXTMENU
        call msg_enviar
        xor eax, eax
        ret

; (ven_defecto) WM_CONTEXTMENU que no ha querido la ventana: la hija lo
; pasa a su madre; la de primer nivel, si es del teclado, abre su menú
ctx_def_menu:
        cmp dword [ebp+V_PADRE], 0
        je .tope
        mov ebp, [ebp+V_PADRE]
        jmp msg_enviar
.tope:
        cmp edx, -1
        jne .x
        test dword [ebp+V_ESTILO], E_TITULO
        jz .x
        mov eax, -1
        call ctx_sistema
.x:
        xor eax, eax
        ret

; =============================================================================
;  El escritorio
; =============================================================================
; EDX = el punto (o -1): en un icono de una ventana minimizada, su menú;
; en lo demás, el del escritorio
ctx_escritorio:
        cmp edx, -1
        jne .raton
        mov dword [ctx_px], 16
        mov dword [ctx_py], BARRA_H + 4
        jmp .menu
.raton:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        mov ecx, [vid_alto]
        sub ecx, ICO_ALTO
        cmp ebx, ecx
        jl .menu
        sub eax, 16
        js .menu
        xor edx, edx
        mov ecx, ICO_PASO
        div ecx
        mov ecx, eax
        call esc_minimizada
        jc .menu
        mov ebp, esi
        mov eax, [ctx_px]
        mov ebx, [ctx_py]
        call ctx_sistema
        jmp .x
.menu:
        mov esi, ctx_l_esc
        mov eax, -1
        xor ecx, ecx
        call ctx_menu
.x:
        xor eax, eax
        ret

ctx_esc_orden:
        cmp ecx, CE_CONFIG
        jne .e1
        xor edx, edx
        call ctx_config
        jmp .x
.e1:
        cmp ecx, CE_FONDO
        jne .e2
        mov edx, 1
        call ctx_config
        jmp .x
.e2:
        cmp ecx, CE_MEZ
        jne .e3
        xor eax, eax
        call mez_abrir
        jmp .x
.e3:
        cmp ecx, CE_CASCADA
        jne .e4
        call ctx_cascada
        jmp .x
.e4:
        cmp ecx, CE_MOSAICO
        jne .e5
        call ctx_mosaico
        jmp .x
.e5:
        cmp ecx, CE_ARCH
        jne .x
        call arch_abrir
.x:
        xor eax, eax
        ret

; (interna) abre Configuración (CONFIG.HVP, que ya está en marcha con la
; ventana oculta); EDX=1: en el apartado Pantalla (el fondo)
ctx_config:
        pushad
        mov edi, ctx_acc + AC_PROG
        mov esi, dir_propia
        call pon_cad
        mov esi, tn_config
        call pon_cad
        mov byte [edi], 0
        mov dword [ctx_acc+AC_PARAM], 0
        mov ebx, ctx_acc
        mov ebp, [cen_ven]          ; (si no se puede cargar, el aviso sale aquí)
        call hvp_abrir
        test edx, edx
        jz .x
        lea esi, [ctx_acc+AC_PROG]  ; su ventana: que enseñe el apartado
        mov edx, hvp_tabla
        mov ecx, HVP_MAX
.b:
        cmp dword [edx+HS_USO], 1
        jne .s
        lea edi, [edx+HS_RUTA]
        call cad_igual_i
        je .hay
.s:
        add edx, HS_TAM
        dec ecx
        jnz .b
        jmp .x
.hay:
        mov eax, [edx+HS_TAREA]
        mov ebp, [ven_tabla]
        mov ecx, MAX_VEN
.v:
        cmp dword [ebp+V_USO], 1
        jne .vs
        cmp [ebp+V_TAREA], eax
        jne .vs
        cmp dword [ebp+V_PADRE], 0
        jne .vs
        mov eax, WM_APARTADO
        xor ecx, ecx                ; 0: Pantalla
        xor edx, edx
        call msg_poner
        jmp .x
.vs:
        add ebp, V_TAM
        dec ecx
        jnz .v
.x:
        popad
        ret

; (interna) EBP=ventana de primer nivel -> CF=1 si no se organiza (oculta,
; minimizada, sin título, un menú, el escritorio)
ctx_organizable:
        push eax
        mov eax, [ebp+V_ESTADO]
        and eax, S_VISIBLE | S_MINIM
        cmp eax, S_VISIBLE
        jne .no
        test dword [ebp+V_ESTILO], E_TITULO
        jz .no
        test dword [ebp+V_ESTILO], E_EMERGENTE | E_HIJA
        jnz .no
        cmp ebp, [escritorio]
        je .no
        pop eax
        clc
        ret
.no:
        pop eax
        stc
        ret

; (interna) EBP: si está maximizada, la restaura
ctx_sin_max:
        test dword [ebp+V_ESTADO], S_MAXIM
        jz .x
        push eax
        mov eax, 4
        call ven_mostrar
        pop eax
.x:
        ret

; (interna) EBP -> EAX=ancho mínimo EDX=alto mínimo (WM_GETMINMAXINFO)
ctx_minimo:
        push ebx
        push ecx
        push esi
        push edi
        push ebp
        mov eax, WM_GETMINMAXINFO
        xor ecx, ecx
        xor edx, edx
        call msg_enviar
        movzx edx, ax
        shr eax, 16
        xchg eax, edx
        cmp eax, MIN_AN
        jae .a
        mov eax, MIN_AN
.a:
        cmp edx, MIN_AL
        jae .x
        mov edx, MIN_AL
.x:
        pop ebp
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret

; las ventanas, una detrás de otra, en escalera (la de delante, la última;
; no cambia cuál está delante)
ctx_cascada:
        pushad
        xor esi, esi
        xor edi, edi
.b:
        cmp esi, [n_orden]
        jae .x
        mov ebp, [orden+esi*4]
        inc esi
        call ctx_organizable
        jc .b
        call ctx_sin_max
        imul eax, edi, CASC_PASO
        add eax, 8
        lea ebx, [eax+BARRA_H]
        lea ecx, [ebx+120]          ; (si se sale por abajo, vuelve arriba)
        cmp ecx, [vid_alto]
        jle .c0
        xor edi, edi
        mov eax, 8
        mov ebx, BARRA_H + 8
.c0:
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        test dword [ebp+V_ESTILO], E_TAMANO
        jz .m
        push eax                    ; las que cambian de tamaño, dentro de la
        add eax, ecx                ; pantalla
        sub eax, [vid_ancho]
        jle .c1
        sub ecx, eax
.c1:
        mov eax, ebx
        add eax, edx
        sub eax, [vid_alto]
        jle .c2
        sub edx, eax
.c2:
        push edx
        call ctx_minimo
        cmp ecx, eax
        jge .c3
        mov ecx, eax
.c3:
        pop eax
        cmp eax, edx
        jge .c4
        mov eax, edx
.c4:
        mov edx, eax
        pop eax
.m:
        call ven_mover
        inc edi
        jmp .b
.x:
        popad
        ret

; las ventanas, una al lado de otra, repartiendo la pantalla: las que no
; cambian de tamaño solo se ponen en su sitio
ctx_mosaico:
        pushad
        xor ecx, ecx                ; cuántas
        xor esi, esi
.n:
        cmp esi, [n_orden]
        jae .n2
        mov ebp, [orden+esi*4]
        inc esi
        call ctx_organizable
        jc .n
        inc ecx
        jmp .n
.n2:
        test ecx, ecx
        jz .x
        mov [ctx_mn], ecx
        mov eax, 1                  ; columnas: la raíz, por arriba
.c:
        mov edx, eax
        imul edx, eax
        cmp edx, ecx
        jae .c2
        inc eax
        jmp .c
.c2:
        mov [ctx_mcols], eax
        mov eax, ecx                ; filas
        add eax, [ctx_mcols]
        dec eax
        xor edx, edx
        div dword [ctx_mcols]
        mov [ctx_mfilas], eax
        mov eax, [vid_ancho]
        xor edx, edx
        div dword [ctx_mcols]
        mov [ctx_man], eax
        mov eax, [vid_alto]
        sub eax, BARRA_H
        xor edx, edx
        div dword [ctx_mfilas]
        mov [ctx_mal], eax
        xor esi, esi
        xor edi, edi                ; la casilla
.b:
        cmp esi, [n_orden]
        jae .x
        mov ebp, [orden+esi*4]
        inc esi
        call ctx_organizable
        jc .b
        call ctx_sin_max
        mov eax, edi
        xor edx, edx
        div dword [ctx_mcols]       ; EAX=fila EDX=columna
        mov ebx, eax
        imul ebx, [ctx_mal]
        add ebx, BARRA_H
        mov eax, edx
        imul eax, [ctx_man]
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        test dword [ebp+V_ESTILO], E_TAMANO
        jz .m
        push eax
        call ctx_minimo
        mov ecx, [ctx_man]
        cmp ecx, eax
        jge .d1
        mov ecx, eax
.d1:
        mov eax, [ctx_mal]
        cmp eax, edx
        jl .d2
        mov edx, eax
.d2:
        pop eax
.m:
        call ven_mover
        inc edi
        jmp .b
.x:
        popad
        ret

; =============================================================================
;  El Centro y los grupos
; =============================================================================
; (vista_proc) WM_CONTEXTMENU: EDX=punto de la pantalla (-1: del teclado).
; En un icono: lo marca y su menú; en el fondo, el del fondo
ctx_vista:
        cmp edx, -1
        je .tecl
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        push eax
        push ebx
        call ven_cli
        mov esi, eax
        mov edi, ebx
        pop ebx
        pop eax
        sub eax, esi
        sub ebx, edi
        call vista_en
        test eax, eax
        js .fondo
        mov [ebp+V_D0], eax
        call ven_invalidar
        jmp .icono
.tecl:
        call vista_sel
        test eax, eax
        jz .fondo
        call vista_cols             ; el menú, en el icono marcado
        mov eax, [ebp+V_D0]
        xor edx, edx
        div dword [vis_cols]
        imul eax, CEN_FILA
        sub eax, [ebp+V_D2]
        add eax, 44
        imul edx, CEN_PASO
        add edx, 12 + CEN_PASO / 2
        push eax
        push edx
        call ven_cli
        pop edx
        pop ecx
        add eax, edx
        add ebx, ecx
        mov [ctx_px], eax
        mov [ctx_py], ebx
.icono:
        mov esi, ctx_l_grupo
        cmp dword [ebp+V_ID], 0
        je .m
        mov esi, ctx_l_acc
        jmp .m
.fondo:
        mov esi, ctx_l_fcentro
        cmp dword [ebp+V_ID], 0
        je .m
        mov esi, ctx_l_fgrupo
.m:
        mov eax, -1
        xor ecx, ecx
        call ctx_menu
        xor eax, eax
        ret

; (vista_proc) las órdenes que no tiene el menú Archivo
ctx_vista_orden:
        cmp ecx, CV_NOMBRE
        je ctx_renombrar
        cmp ecx, CV_ORGANIZAR
        je ctx_organizar
        ret

; «Cambiar nombre»: de un acceso, pide el nombre nuevo; de un grupo, lo
; mismo que sus Propiedades
ctx_renombrar:
        pushad
        call vista_sel
        test eax, eax
        jz .x
        mov ebx, eax
        cmp dword [ebp+V_ID], 0
        jne .acc
        call vista_propiedades
        jmp .x
.acc:
        lea esi, [ebx+AC_NOMBRE]
        mov edi, ctx_nom
        mov ecx, 31
        call cad_copiar
        mov esi, t_ctx_nom
        mov edi, t_ctx_ren
        push ebx
        mov ebx, ctx_nom
        mov ecx, 32
        call dlg_pedir
        pop ebx
        cmp eax, 1
        jne .x
        cmp byte [ctx_nom], 0
        je .x
        mov esi, ctx_nom
        lea edi, [ebx+AC_NOMBRE]
        mov ecx, 31
        call cad_copiar
        test byte [ebx+AC_BAND2], AB2_AUTO  ; uno de la carpeta del grupo: ya
        jz .h                               ; no se quita solo
        or byte [ebx+AC_BAND2], AB2_MANO
.h:
        call vistas_repintar
        call cfg_guardar
.x:
        popad
        ret

; «Organizar iconos»: los accesos del grupo, por orden alfabético (se
; guarda en CENTRO.INI)
ctx_organizar:
        pushad
        mov eax, [ebp+V_ID]
        sub eax, 100
        js .x
        mov dl, al
.pasada:
        xor dh, dh
        mov esi, [ac_tabla]
        xor edi, edi
        mov ecx, MAX_AC
.b:
        cmp byte [esi+AC_USO], 0
        je .s
        cmp [esi+AC_GRUPO], dl
        jne .s
        test edi, edi
        jz .pon
        call ctx_cmp_nombre
        jbe .pon
        call ctx_cambiar
        mov dh, 1
.pon:
        mov edi, esi
.s:
        add esi, AC_TAM
        dec ecx
        jnz .b
        test dh, dh
        jnz .pasada
        mov dword [ebp+V_D0], 0
        call vistas_repintar
        call cfg_guardar
.x:
        popad
        ret

; (interna) EDI, ESI=accesos: compara sus nombres sin mirar mayúsculas
; (CF y ZF como cmp: «ja» si el de EDI va detrás)
ctx_cmp_nombre:
        push eax
        push ebx
        push ecx
        xor ecx, ecx
.c:
        mov al, [edi+AC_NOMBRE+ecx]
        call ctx_mayuscula
        mov bl, al
        mov al, [esi+AC_NOMBRE+ecx]
        call ctx_mayuscula
        cmp bl, al
        jne .x
        test al, al
        jz .x
        inc ecx
        cmp ecx, 32
        jb .c
        cmp eax, eax
.x:
        pop ecx
        pop ebx
        pop eax
        ret

; (interna) intercambia los accesos EDI y ESI
ctx_cambiar:
        pushad
        mov ecx, AC_TAM / 4
.c:
        mov eax, [esi]
        xchg eax, [edi]
        mov [esi], eax
        add esi, 4
        add edi, 4
        dec ecx
        jnz .c
        popad
        ret

; =============================================================================
;  Los cuadros de texto
; =============================================================================
; (ed_proc) WM_CONTEXTMENU: toma el foco y su menú, con lo que no se puede
; hacer en gris
ctx_edit:
        call ven_enfocar
        xor ecx, ecx
        mov eax, [ebp+V_D3]
        cmp eax, [ebp+V_D4]
        jne .g1
        or ecx, 1011b               ; sin nada seleccionado: Cortar, Copiar, Borrar
.g1:
        cmp dword [pp_len], 0
        jne .g2
        or ecx, 100b                ; el portapapeles vacío: Pegar
.g2:
        cmp dword [ebp+V_D1], 0
        jne .g3
        or ecx, 100000b             ; sin texto: Seleccionar todo
.g3:
        mov esi, ctx_l_edit
        mov eax, -1
        call ctx_menu
        xor eax, eax
        ret

; (ed_proc) WM_COMMAND: las órdenes de su menú (lo demás, a ven_defecto)
ctx_ed_orden:
        cmp ecx, CT_CORTAR
        je ed_proc.cortar
        cmp ecx, CT_COPIAR
        je ed_proc.copiar
        cmp ecx, CT_PEGAR
        je ed_proc.pegar
        cmp ecx, CT_TODO
        je ed_proc.todo
        cmp ecx, CT_BORRAR
        jne ven_defecto
        call ed_borrar_sel
        jmp ed_proc.cambio

; (aj_leer, aj_poner) AJ_CTRL_DER: 1 si Ctrl+clic es el botón derecho
ctx_aj_leer:
        movzx eax, byte [aj_ctrl_no]
        xor eax, 1
        ret
ctx_aj_poner:
        test edx, edx
        setz [aj_ctrl_no]
        ret
