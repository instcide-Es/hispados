; =============================================================================
;  Hispa Ventanas - [BARRA] el escritorio de Hispa-486: barra de tareas abajo
;  y menú «Inicio» (docs/486/HISPA-486.md, fase 4; docs/VENTANAS.md)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Este archivo va en la «parte alta» de VENTANAS.COM (ventanas.asm): solo
;  código y datos de 32 bits. En los demás archivos solo hay ganchos de una o
;  dos líneas, marcados con [BARRA].
;
;  Pedido de Carlos: «en 486 se echa de menos la barra de inicio abajo», «todas
;  las aplicaciones en el menú de inicio para que quede el escritorio
;  despejado» y «en 386 no hace falta poner ese menú de inicio, porque así
;  respeta la estética de Win 3.11». Por eso hay dos escritorios:
;    - el de Hispa-386, el de siempre (barra arriba, Centro de programas
;      abierto, iconos de lo minimizado abajo): aquí no cambia nada;
;    - el de Hispa-486: se pide con la opción /486 de VENTANAS o con la clave
;      «Escritorio=486» de VENTANAS.INI (op_486 = 1). No hay barra arriba: el
;      nombre va en la cabecera del menú Inicio; «Salir», dentro del menú; el
;      volumen, la red, la batería y la hora, a la derecha de la barra de
;      abajo. El Centro de programas no se abre al entrar (queda oculto: se
;      abre desde Inicio) y las ventanas minimizadas van a su botón.
;      Los accesos de los grupos con «Escritorio=1» en CENTRO.INI salen como
;      iconos grandes en el escritorio (no en el menú Inicio): un clic (o
;      Intro, con el marcado) los abre. Lo pide la prueba de uso con la
;      biblioteca (docs/486/USABILIDAD-NAVEGADOR.md, M11): un icono
;      «Biblioteca» que abre el Navegador.
;  Es una función equivalente a la de otros sistemas de la época con el
;  aspecto de Hispa Ventanas (marrón, ocre, terracota y el rombo): no copia
;  el dibujo de ninguno.
;
;  La barra es una ventana de primer nivel sin título, de la tarea del
;  sistema, que bt_orden mantiene encima de todas las que tienen título (los
;  menús y lo emergente quedan encima de ella). No guarda estado de las
;  ventanas: bt_vigilar, una vez por vuelta del bucle del sistema, mira qué
;  ventanas hay y cómo están (una «firma» de cada una) y repinta si algo ha
;  cambiado. Así el gestor no tiene que avisar de nada.
;
;    bt_iniciar      (principal) el área de trabajo; lee «Escritorio=486» y,
;                    en Hispa-486, crea la barra y cambia el escritorio
;    bt_vigilar      (bucle del sistema) la barra, al día; lo pendiente del menú
;    bt_orden        (orden_poner) EBP=ventana que acaba de pasar delante
;    bt_area         (ven_mostrar) -> EAX=x EBX=y ECX=ancho EDX=alto de trabajo
;    bt_colocar      (ven_crear) EDI=ventana nueva: que no nazca tras la barra
;    bt_centro_ini   (centro_tarea) el Centro, oculto; los grupos, cerrados
;    bt_centro_cerrar (vista_proc) EBP=el Centro: se esconde -> CF=1
;    bt_tecla        (ctx_tecla) AX=tecla: Ctrl+Esc y las del menú -> CF=1
;    bt_clic_fuera   (ctx_ctrl_clic) BL, BH=botones: un clic fuera lo cierra
;    bt_esc_inv      (vistas_repintar) los iconos del escritorio, a repintar
; =============================================================================

BT_H        equ 28                  ; alto de la barra (como la de arriba)
BT_MAX      equ 32                  ; ventanas con botón (MAX_TOPE)
BT_BUF      equ 4096
BT_INI_X    equ 4                   ; el botón «Inicio»
BT_INI_Y    equ 5
BT_INI_AN   equ 76
BT_INI_AL   equ 18
BT_X0       equ BT_INI_X + BT_INI_AN + 8    ; el primer botón de ventana
BT_BOT_Y    equ 4
BT_BOT_AL   equ 21
BT_BOT_MAX  equ 160                 ; ancho de un botón, como mucho...
BT_BOT_MIN  equ 30                  ; ...y como poco (solo el icono)
BT_DESP     equ SALIR_AN + 12 - 8   ; lo que se corre la bandeja al no haber «Salir»
INI_MAX      equ MAX_GR + 8          ; órdenes del menú Inicio
INI_SMAX      equ MAX_AC              ; programas de un submenú
MI_CAB      equ 26                  ; la cabecera del menú
MI_FR       equ 40                  ; [FRANJA] la franja de la izquierda con «Hispa-486»
; [ICONOS] el alto de una orden (mi_h: 22, con un icono de 16 y aire; 38 con
; los iconos grandes, de 32), en bt_mi_tallas
MI_SEP      equ 8                   ; una raya
INI_P_CERRAR equ 1                  ; lo pendiente: cerrar el menú,
INI_P_ORDEN equ 2                   ;   una orden del menú,
INI_P_PROG  equ 3                   ;   un programa del submenú
; los iconos del escritorio de Hispa-486 (los accesos de los grupos con
; «Escritorio=1»): en columnas desde arriba a la izquierda, cada uno en una
; casilla con el dibujo y el nombre debajo.
; [ICONOS] Desde la 0.9.28 el dibujo y la casilla van con el tamaño de los
; iconos de Configuración (los mismos de los grupos del Centro: ico_px,
; cen_paso, cen_fila) y el nombre con la letra normal: antes eran 64x64 con
; el nombre a doble tamaño en una etiqueta, y en un portátil de 1440x900 a
; 800x600 ampliado cada icono se comía la pantalla.
BE_MAX      equ 8                   ; iconos, como mucho
BE_X0       equ 12                  ; la primera casilla
BE_Y0       equ 12
BE_ALMAS    equ 16                  ; la casilla: el ancho de cen_paso y el alto de cen_fila y esto
BE_ESPERA   equ 18                  ; tics (1 s) en los que otro clic no abre otra vez

c_barra_t   dd bt_proc
            db SIN_FONDO, 0, 0, 0
c_inicio    dd ini_proc
            db C_CREMA, 0, 0, 0

bt_k_escr   db "escritorio", 0
bt_n_ini    db "VENTANAS.INI", 0
bt_t_inicio db "Inicio", 0
bt_t_vacio  db "(vacío)", 0
bt_t_mas    db "Más...", 0
; las órdenes fijas del menú Inicio: texto e icono
ini_cmds    dd bt_t_config, ico_herramienta
            dd bt_t_arch, ico_disco
            dd bt_t_tareas, ico_programa
            dd bt_t_centro, ico_centro
            dd bt_t_salir, ico_apagar
bt_t_config db "Configuración", 0
bt_t_arch   db "Administrador de archivos", 0
bt_t_tareas db "Administrador de tareas", 0
bt_t_centro db "Centro de programas", 0
bt_t_salir  db "Salir de Hispa Ventanas...", 0
; el menú del botón derecho en la barra (un desplegable de menus.inc)
bt_l_ctx:
        dw CE_CASCADA
        db "Ventanas en c&ascada", 0
        dw CE_MOSAICO
        db "Ventanas en m&osaico", 0
        dw 0
        db 0
        dw CE_TAREAS
        db "Administrador de &tareas...", 0
        dw 0
        db 0
        dw CE_CONFIG
        db "&Configuración...", 0
        dw 0xFFFF

; =============================================================================
;  Arranque
; =============================================================================
bt_iniciar:
        pushad
        mov dword [area_y0], BARRA_H
        mov eax, [vid_alto]
        mov [area_y1], eax
        cmp byte [op_demo], 0
        jne .x
        cmp byte [op_486], 0
        jne .si
        call bt_ini_leer            ; VENTANAS.INI: «Escritorio=486»
        cmp byte [op_486], 0
        je .x
.si:
        call bt_crear
.x:
        popad
        ret

; (interna) el escritorio de Hispa-486: el área de trabajo, el escritorio sin
; la barra de arriba y la barra de tareas (al arrancar y, [ESTILO], al
; cambiarlo en Configuración)
bt_crear:
        pushad
        mov dword [area_y0], 0
        mov eax, [vid_alto]
        sub eax, BT_H
        mov [area_y1], eax
        call bt_edicion_leer        ; [FRANJA] el nombre de la edición
        mov ebp, [escritorio]       ; el escritorio, con el procedimiento de
        mov dword [ebp+V_PROC], bt_esc_proc ; Hispa-486 (sin barra arriba)
        call ven_invalidar
        mov dword [ms_grupo], -1
        mov dword [mi_sel], -1
        sub esp, P_TAM
        mov esi, esp
        mov dword [esi+P_CLASE], c_barra_t
        xor eax, eax
        mov [esi+P_TEXTO], eax
        mov [esi+P_ESTILO], eax
        mov [esi+P_X], eax
        mov [esi+P_ID], eax
        mov eax, [area_y1]
        mov [esi+P_Y], eax
        mov eax, [vid_ancho]
        mov [esi+P_AN], eax
        mov dword [esi+P_AL], BT_H
        xor ebp, ebp
        call ven_crear
        add esp, P_TAM
        mov [bt_ven], eax
        popad
        ret

; (interna) busca «Escritorio=486» en VENTANAS.INI (de la carpeta de
; VENTANAS.COM): si está, op_486 = 1. La clave la pone la instalación de
; Hispa-486 (o quien quiera, a mano); Configuración la conserva al guardar
bt_ini_leer:
        pushad
        call dir_propia_poner
        mov edi, cfg_ruta
        mov esi, dir_propia
        call pon_cad
        mov esi, bt_n_ini
        call pon_cad
        mov byte [edi], 0
        mov esi, cfg_ruta
        xor al, al
        call dos_abrir
        jc .x
        mov ebx, eax
        mov edi, bt_buf
        mov ecx, BT_BUF - 1
        call dos_leer
        jnc .l
        xor eax, eax
.l:
        call dos_cerrar
        mov byte [bt_buf+eax], 0
        mov esi, bt_buf
.linea:
        mov al, [esi]
        test al, al
        jz .x
        cmp al, ' '
        ja .l1
        inc esi
        jmp .linea
.l1:
        mov edi, bt_k_escr
        mov edx, esi
.c:
        mov al, [edi]
        test al, al
        jz .fk
        mov ah, [edx]
        or ah, 0x20                 ; sin mirar mayúsculas (son letras)
        cmp al, ah
        jne .salta
        inc edi
        inc edx
        jmp .c
.fk:
        cmp byte [edx], ' '
        jne .fk2
        inc edx
        jmp .fk
.fk2:
        cmp byte [edx], '='
        jne .salta
.fk3:
        inc edx
        cmp byte [edx], ' '
        je .fk3
        cmp word [edx], '48'
        jne .salta
        cmp byte [edx+2], '6'
        jne .salta
        cmp byte [edx+3], ' '
        ja .salta
        mov byte [op_486], 1
        jmp .x
.salta:
        mov al, [esi]
        test al, al
        jz .x
        inc esi
        cmp al, 10
        jne .salta
        jmp .linea
.x:
        popad
        ret

; =============================================================================
;  [ESTILO] Pasar de un escritorio al otro sin salir de Hispa Ventanas
;  (Configuración › Escritorio › «Estilo del escritorio»; pedido de Carlos,
;  18/11/2026: «pasar de menú de inicio a menú de Ventanas»). Configuración
;  lo pide por la interfaz (esc_imagen, EAX=4) y se hace aquí, en la vuelta
;  siguiente del bucle del sistema (bt_vigilar), como lo demás de la barra:
;    - a la barra de tareas: el área de trabajo sin la barra de arriba, el
;      escritorio de Hispa-486, la barra; el Centro y sus grupos, ocultos
;      (como al entrar en Hispa-486: todo está en el menú Inicio);
;    - al clásico: se cierran el menú Inicio y las ventanitas de la bandeja,
;      se quita la barra, vuelven la barra de arriba y el escritorio de
;      siempre (lo minimizado, en iconos), y el Centro y los grupos que se
;      escondieron, a la vista.
;  En los dos casos, las ventanas maximizadas se ajustan al área nueva, las
;  que quedarían debajo de una barra se corren lo justo y la que estaba
;  delante (Configuración) sigue delante.
; =============================================================================
; (esc_imagen, EAX=3) -> EAX=0 el clásico, 1 el de la barra de tareas (el que
; se va a poner, si ya se ha pedido otro)
bt_estilo_leer:
        movzx eax, byte [bt_est_pend]
        dec eax
        jns .r
        movzx eax, byte [op_486]
.r:
        clc
        ret

; (esc_imagen, EAX=4) EDX=0 el clásico, 1 el de la barra de tareas -> CF=0 y
; EAX=0 (se pone en la vuelta siguiente); CF=1 y EAX=1 si no se puede (la
; demostración, /DEMO, no tiene ninguno de los dos)
bt_estilo_pedir:
        cmp byte [op_demo], 0
        jne .no
        cmp edx, 1
        ja .no
        lea eax, [edx+1]
        mov [bt_est_pend], al
        mov byte [actividad], 1     ; (que la vuelta no se duerma)
        xor eax, eax
        clc
        ret
.no:
        mov eax, 1
        stc
        ret

; (bt_vigilar) si se ha pedido el otro escritorio, se pone
bt_estilo:
        cmp byte [bt_est_pend], 0
        jne .si
        ret
.si:
        pushad
        movzx eax, byte [bt_est_pend]
        mov byte [bt_est_pend], 0
        dec eax
        cmp al, [op_486]
        je .x
        mov ebx, [activa]           ; la de delante, que siga delante
        push ebx
        push eax
        ; las ventanitas de la bandeja (salen junto a una barra u otra)
        mov ebp, [vol_ven]
        test ebp, ebp
        jz .v1
        call ven_destruir
.v1:
        mov ebp, [wf_ven]
        test ebp, ebp
        jz .v2
        call ven_destruir
.v2:
        pop eax
        test eax, eax
        jz .clasico
        ; ---- a la barra de tareas ----
        mov byte [op_486], 1
        call bt_crear
        mov ebp, [cen_ven]          ; el Centro y sus grupos, ocultos
        call bt_est_ocultar
        mov edi, [gr_tabla]
        mov ecx, MAX_GR
.g:
        mov ebp, [edi+GR_VEN]
        call bt_est_ocultar
        add edi, GR_TAM
        dec ecx
        jnz .g
        jmp .comun
.clasico:
        call ini_cerrar             ; el menú Inicio, si estaba abierto
        mov dword [ini_pend], 0
        mov ebp, [bt_ven]
        mov dword [bt_ven], 0
        mov dword [bt_n], 0
        test ebp, ebp
        jz .c1
        call ven_destruir
.c1:
        mov byte [op_486], 0
        mov dword [area_y0], BARRA_H
        mov eax, [vid_alto]
        mov [area_y1], eax
        mov ebp, [escritorio]       ; el escritorio de siempre: la barra de
        mov dword [ebp+V_PROC], esc_proc    ; arriba y los iconos de lo minimizado
        call ven_invalidar
        mov ebp, [cen_ven]          ; el Centro y los grupos que se escondieron
        call bt_est_mostrar
        mov edi, [gr_tabla]
        mov ecx, MAX_GR
.h:
        mov ebp, [edi+GR_VEN]
        call bt_est_mostrar
        add edi, GR_TAM
        dec ecx
        jnz .h
.comun:
        call bt_est_acomodar
        pop ebp                     ; la que estaba delante, otra vez delante
        test ebp, ebp
        jz .x
        cmp dword [ebp+V_USO], 1
        jne .x
        call ven_visible
        jc .x
        call ven_activar
.x:
        popad
        ret

; (interna) EBP=ventana (0: ninguna): si se ve, se esconde
bt_est_ocultar:
        test ebp, ebp
        jz .r
        test dword [ebp+V_ESTADO], S_VISIBLE
        jz .r
        push eax
        xor eax, eax
        call ven_mostrar
        pop eax
.r:
        ret

; (interna) EBP=ventana (0: ninguna): si está escondida, a la vista
bt_est_mostrar:
        test ebp, ebp
        jz .r
        test dword [ebp+V_ESTADO], S_VISIBLE
        jnz .r
        push eax
        mov eax, 1
        call ven_mostrar
        pop eax
.r:
        ret

; (interna) las ventanas con título, dentro del área de trabajo nueva: las
; maximizadas, a toda ella; las demás, sin quedar debajo de una barra
bt_est_acomodar:
        pushad
        mov ebp, [ven_tabla]
        mov ecx, MAX_VEN
.v:
        cmp dword [ebp+V_USO], 1
        jne .s
        cmp dword [ebp+V_PADRE], 0
        jne .s
        test dword [ebp+V_ESTILO], E_TITULO
        jz .s
        test dword [ebp+V_ESTILO], E_EMERGENTE | E_HIJA
        jnz .s
        cmp ebp, [escritorio]
        je .s
        push ecx
        test dword [ebp+V_ESTADO], S_MAXIM
        jz .n
        call bt_area
        call ven_mover
        jmp .s1
.n:
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        mov esi, [area_y1]          ; lo más abajo que puede empezar
        sub esi, edx
        cmp ebx, esi
        jle .n1
        mov ebx, esi
.n1:
        cmp ebx, [area_y0]          ; y lo más arriba
        jge .n2
        mov ebx, [area_y0]
.n2:
        cmp ebx, [ebp+V_Y]
        je .s1
        call ven_mover
.s1:
        pop ecx
.s:
        add ebp, V_TAM
        dec ecx
        jnz .v
        popad
        ret

; =============================================================================
;  Ganchos del gestor y del Centro
; =============================================================================
; -> el área de trabajo: EAX=x EBX=y ECX=ancho EDX=alto (maximizar)
bt_area:
        xor eax, eax
        mov ebx, [area_y0]
        mov ecx, [vid_ancho]
        mov edx, [area_y1]
        sub edx, ebx
        ret

; EBP acaba de ponerse la última del orden (delante): si tiene título, pasa
; a estar justo debajo de la barra de tareas (y de lo que la barra tenga
; encima: los menús y lo emergente)
bt_orden:
        cmp dword [bt_ven], 0
        je .r
        test dword [ebp+V_ESTILO], E_TITULO
        jz .r
        pushad
        mov edx, [bt_ven]
        mov ecx, [n_orden]
        dec ecx                     ; donde está EBP
        xor esi, esi
.b:
        cmp esi, ecx
        jae .x
        cmp [orden+esi*4], edx
        je .d
        inc esi
        jmp .b
.d:
        cmp ecx, esi
        jbe .p
        mov eax, [orden+ecx*4-4]
        mov [orden+ecx*4], eax
        dec ecx
        jmp .d
.p:
        mov [orden+esi*4], ebp
.x:
        popad
.r:
        ret

; EDI=ventana que se está creando (ya con su sitio): en Hispa-486, una con
; título que caería detrás de la barra de tareas sube lo justo
bt_colocar:
        cmp dword [bt_ven], 0
        je .r
        test dword [edi+V_ESTILO], E_HIJA | E_EMERGENTE
        jnz .r
        test dword [edi+V_ESTILO], E_TITULO
        jz .r
        push eax
        mov eax, [area_y1]
        sub eax, [edi+V_AL]
        jns .a
        xor eax, eax
.a:
        cmp [edi+V_Y], eax
        jle .b
        mov [edi+V_Y], eax
.b:
        pop eax
.r:
        ret

; (centro_tarea, con la ventana del Centro recién creada) en Hispa-486 el
; escritorio empieza despejado: el Centro queda oculto y no se abre ningún
; grupo (todo está en el menú Inicio)
bt_centro_ini:
        cmp byte [op_486], 0
        je .r
        pushad
        mov ebx, [gr_tabla]
        mov ecx, MAX_GR
.g:
        mov byte [ebx+GR_ESTADO], 0
        add ebx, GR_TAM
        dec ecx
        jnz .g
        mov dword [cen_activo], -1
        mov ebp, [cen_ven]
        test ebp, ebp
        jz .x
        xor eax, eax
        call ven_mostrar
.x:
        popad
.r:
        ret

; (vista_proc, WM_CLOSE del Centro) en Hispa-486 cerrar el Centro lo esconde
; (para salir está «Salir» en el menú Inicio) -> CF=1 si ya está hecho
bt_centro_cerrar:
        cmp byte [op_486], 0
        je .no
        push eax
        xor eax, eax
        call ven_mostrar
        pop eax
        stc
        ret
.no:
        clc
        ret

; (interna) el Centro de programas, a la vista y delante
bt_centro_abrir:
        pushad
        mov ebp, [cen_ven]
        test ebp, ebp
        jz .x
        test dword [ebp+V_ESTADO], S_INACTIVA   ; (tiene un cuadro abierto)
        jnz .x
        test dword [ebp+V_ESTADO], S_VISIBLE
        jnz .vis
        mov eax, 1
        call ven_mostrar
        jmp .x
.vis:
        test dword [ebp+V_ESTADO], S_MINIM
        jz .a
        mov eax, 4
        call ven_mostrar
.a:
        call ven_activar
.x:
        popad
        ret

; =============================================================================
;  El escritorio de Hispa-486: el fondo entero, sin barra arriba, y los
;  iconos de los accesos de los grupos con «Escritorio=1»
; =============================================================================
bt_esc_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_LBUTTONUP
        je .nada
        cmp eax, WM_CONTEXTMENU
        je .menu
        cmp eax, WM_COMMAND
        jne esc_proc                ; (el temporizador de la hora, el botón derecho)
        cmp ecx, CE_CENTRO
        jne esc_proc
        call bt_centro_abrir
.nada:
        xor eax, eax
        ret
.pulsa:
        ; un clic (no hace falta el doble: la gente mayor se lía con él) en
        ; un icono lo marca y lo abre; el segundo clic de un doble clic no
        ; abre otro (be_lanzar)
        call be_lista
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        call be_en
        test eax, eax
        js .nada
        mov ecx, eax
        cmp ecx, [be_sel]
        jne .p0
        cmp byte [be_marca], 0
        jne .p1
.p0:
        mov [be_sel], ecx
        mov byte [be_marca], 1      ; [ICONOS] desde ahora se ve la marca
        call bt_esc_inv
.p1:
        call be_lanzar
        jmp .nada
.menu:
        cmp edx, -1
        jne .m1
        mov dword [ctx_px], 16
        mov dword [ctx_py], 16
.m1:
        mov esi, ctx_l_esc
        mov eax, -1
        xor ecx, ecx
        call ctx_menu
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        xor eax, eax
        xor ebx, ebx
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        call fon_pintar             ; [FONDO] la imagen o el mosaico (fondo.inc)
        call be_pintar              ; los iconos del escritorio, encima del fondo
        call dc_cerrar
        xor eax, eax
        ret

; (vistas_repintar: han cambiado los accesos) los iconos del escritorio de
; Hispa-486, a repintar
bt_esc_inv:
        cmp dword [bt_ven], 0
        je .r
        push ebp
        mov ebp, [escritorio]
        test ebp, ebp
        jz .x
        call ven_invalidar
.x:
        pop ebp
.r:
        ret

; (interna) los accesos que van en el escritorio -> be_acc, be_n
be_lista:
        pushad
        xor edi, edi
        mov ebx, [ac_tabla]
        test ebx, ebx
        jz .f
        cmp dword [gr_tabla], 0
        je .f
        mov ecx, MAX_AC
.a:
        cmp byte [ebx+AC_USO], 0
        je .s
        test byte [ebx+AC_BAND2], AB2_OCULTO
        jnz .s
        movzx eax, byte [ebx+AC_GRUPO]
        cmp eax, MAX_GR
        jae .s
        imul eax, GR_TAM
        add eax, [gr_tabla]
        cmp byte [eax+GR_USO], 0
        je .s
        test byte [eax+GR_BAND], GRB_ESCRITORIO
        jz .s
        mov [be_acc+edi*4], ebx
        inc edi
        cmp edi, BE_MAX
        jae .f
.s:
        add ebx, AC_TAM
        dec ecx
        jnz .a
.f:
        mov [be_n], edi
        popad
        ret

; (interna) -> EAX=casillas que caben de arriba abajo (1 como poco)
be_filas:
        push ecx
        push edx
        mov eax, [area_y1]
        sub eax, BE_Y0
        jns .a
        xor eax, eax
.a:
        xor edx, edx
        mov ecx, [cen_fila]
        add ecx, BE_ALMAS
        div ecx
        test eax, eax
        jnz .x
        inc eax
.x:
        pop edx
        pop ecx
        ret

; (interna) ECX=icono -> EAX=x EBX=y de su casilla
be_pos:
        push edx
        call be_filas
        mov ebx, eax
        mov eax, ecx
        xor edx, edx
        div ebx                     ; EAX=columna EDX=fila
        imul eax, [cen_paso]
        add eax, BE_X0
        mov ebx, [cen_fila]
        add ebx, BE_ALMAS
        imul edx, ebx
        add edx, BE_Y0
        mov ebx, edx
        pop edx
        ret

; (interna) EAX=x EBX=y en el escritorio -> EAX=el icono que hay ahí (-1:
; ninguno). Vale toda la casilla menos un margen (el dibujo y la etiqueta)
be_en:
        push ecx
        push edx
        push esi
        push edi
        mov esi, eax
        mov edi, ebx
        xor ecx, ecx
.b:
        cmp ecx, [be_n]
        jae .no
        call be_pos
        mov edx, esi
        sub edx, eax
        sub edx, 4
        js .s
        push eax
        mov eax, [cen_paso]
        sub eax, 8
        cmp edx, eax
        pop eax
        jae .s
        mov edx, edi
        sub edx, ebx
        js .s
        push eax
        mov eax, [cen_fila]
        add eax, BE_ALMAS - 4
        cmp edx, eax
        pop eax
        jae .s
        mov eax, ecx
        jmp .x
.s:
        inc ecx
        jmp .b
.no:
        mov eax, -1
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        ret

; (interna) ECX=icono: abre su acceso (en una tarea, como el menú Inicio).
; No lo abre otra vez si se acaba de abrir (el segundo clic de un doble
; clic, BE_ESPERA) ni si es un programa de Windows que ya está en marcha: lo
; trae delante (en el equipo de la biblioteca, un solo Navegador)
be_lanzar:
        pushad
        cmp ecx, [be_n]
        jae .x
        call tics
        mov edx, eax
        sub eax, [be_tic]
        cmp dword [be_tic], 0
        je .t
        cmp eax, BE_ESPERA
        jb .x
.t:
        mov [be_tic], edx
        mov ebx, [be_acc+ecx*4]
        ; ¿un programa de Windows suyo ya en marcha?
        mov edx, pe_tabla
        mov ecx, PE_MAX
.p:
        cmp dword [edx+PE_USO], 1
        jne .ps
        lea esi, [edx+PE_RUTA]
        lea edi, [ebx+AC_PROG]
        call cad_igual_i
        je .ya
.ps:
        add edx, PE_TAM
        dec ecx
        jnz .p
        mov [bt_lz_acc], ebx
        mov eax, bt_lz_tarea
        call tar_crear
.x:
        popad
        ret
.ya:
        ; su ventana (de primer nivel, con título y a la vista), delante; si
        ; aún no tiene (se está cargando), nada
        mov eax, [edx+PE_TAREA]
        mov ebp, [ven_tabla]
        mov ecx, MAX_VEN
.v:
        cmp dword [ebp+V_USO], 1
        jne .vs
        cmp [ebp+V_TAREA], eax
        jne .vs
        cmp dword [ebp+V_PADRE], 0
        jne .vs
        test dword [ebp+V_ESTILO], E_TITULO
        jz .vs
        test dword [ebp+V_ESTADO], S_VISIBLE
        jz .vs
        call bt_pulsar.trae0
        jmp .x
.vs:
        add ebp, V_TAM
        dec ecx
        jnz .v
        jmp .x

; (interna, con el escritorio en un dc_pintar) los iconos: [ICONOS] el
; dibujo del tamaño puesto en Configuración (ico_px) y el nombre debajo, con
; la letra normal, centrado y partido en dos líneas si es largo (como en los
; grupos del Centro: vis_partir, vis_linea), en blanco con sombra para que
; se lea sobre la imagen de fondo; la marca (anil) solo en el marcado y solo
; después de un clic o de una flecha (be_marca)
be_pintar:
        pushad
        call be_lista
        mov eax, [be_n]
        test eax, eax
        jz .x
        cmp [be_sel], eax
        jb .s0
        dec eax
        mov [be_sel], eax
.s0:
        mov byte [vis_forzar2], 1   ; (dos líneas, como en un grupo)
        xor edi, edi
.i:
        cmp edi, [be_n]
        jae .fin
        mov ecx, edi
        call be_pos
        mov [be_sx], eax
        mov [be_sy], ebx
        mov [vis_x], eax            ; (para vis_linea)
        mov ebx, [be_acc+edi*4]
        movzx eax, byte [ebx+AC_ICONO]
        cmp eax, ICO_N
        jb .ia
        xor eax, eax
.ia:
        mov esi, [ico_tabla+eax*8]
        call hvp_icono              ; (un .HVP trae el suyo)
        mov eax, [cen_paso]
        sub eax, [ico_px]
        shr eax, 1
        add eax, [be_sx]
        mov ebx, [be_sy]
        add ebx, [cen_icoy]
        call dc_icono_t
        mov ebx, [be_acc+edi*4]
        lea esi, [ebx+AC_NOMBRE]
        call cad_largo
        call vis_partir
        mov ebx, [be_sy]
        add ebx, [cen_etqy]
        cmp byte [be_marca], 0
        je .sombra
        cmp edi, [be_sel]
        jne .sombra
        mov ecx, [vis_n1]           ; la marca: como la de los grupos
        call vis_ancho
        mov edx, eax
        mov ecx, [vis_n2]
        call vis_ancho
        cmp eax, edx
        jae .m1
        mov eax, edx
.m1:
        lea ecx, [eax+8]
        shr eax, 1
        neg eax
        add eax, [be_sx]
        mov edx, [cen_paso]
        shr edx, 1
        lea eax, [eax+edx-4]
        mov edx, 18
        cmp dword [vis_n2], 0
        je .m2
        add edx, VIS_RENGLON
.m2:
        COLOR C_ANIL
        dec ebx
        call dc_rect
        inc ebx
        COLOR C_BLANCO
        xor eax, eax
        call be_nombre
        jmp .sig
.sombra:
        COLOR C_TINTA               ; la sombra, un punto abajo a la derecha
        mov eax, 1
        call be_nombre
        COLOR C_BLANCO
        xor eax, eax
        call be_nombre
.sig:
        inc edi
        jmp .i
.fin:
        mov byte [vis_forzar2], 0
.x:
        popad
        ret

; (interna) el nombre partido por vis_partir, a la altura EBX, corrido EAX
; puntos a la derecha y abajo, con el color puesto
be_nombre:
        pushad
        mov edx, [vis_x]
        push edx
        add [vis_x], eax
        add ebx, eax
        mov esi, [vis_t1]
        mov ecx, [vis_n1]
        call vis_linea
        mov ecx, [vis_n2]
        test ecx, ecx
        jz .x
        add ebx, VIS_RENGLON
        mov esi, [vis_t2]
        call vis_linea
.x:
        pop dword [vis_x]
        popad
        ret

; (bt_tecla, sin el menú abierto) AX=tecla: sin ninguna ventana activa (en el
; escritorio), las flechas cambian de icono e Intro abre el marcado -> CF=1
be_tecla:
        cmp dword [activa], 0
        jne .no
        cmp dword [foco], 0
        jne .no
        pushad
        mov byte [be_hecho], 0
        call be_lista
        mov ecx, [be_n]
        test ecx, ecx
        jz .x
        mov ecx, [be_sel]
        cmp al, 13
        jne .e
        mov byte [be_hecho], 1
        call be_lanzar
        jmp .x
.e:
        test al, al
        jz .ext
        cmp al, 0xE0
        jne .x
.ext:
        push eax
        call be_filas
        mov edx, eax                ; EDX=filas
        pop eax
        cmp ah, TC_ABAJO
        jne .e1
        inc ecx
        jmp .pon
.e1:
        cmp ah, TC_ARRIBA
        jne .e2
        dec ecx
        jmp .pon
.e2:
        cmp ah, TC_DER
        jne .e3
        add ecx, edx
        jmp .pon
.e3:
        cmp ah, TC_IZQ
        jne .x
        sub ecx, edx
.pon:
        mov byte [be_hecho], 1
        cmp byte [be_marca], 0      ; [ICONOS] la primera flecha enseña la marca
        jne .pon2
        mov byte [be_marca], 1
        mov ecx, [be_sel]
.pon2:
        test ecx, ecx
        js .x
        cmp ecx, [be_n]
        jae .x
        mov [be_sel], ecx
        call bt_esc_inv
.x:
        popad
        cmp byte [be_hecho], 1
        cmc
        ret
.no:
        clc
        ret

; =============================================================================
;  Qué ventanas tienen botón
; =============================================================================
; (interna) EBP=ventana -> CF=0 si es de primer nivel, con título y a la vista
; (aunque esté minimizada)
bt_basica:
        cmp dword [ebp+V_USO], 1
        jne .no
        cmp dword [ebp+V_PADRE], 0
        jne .no
        test dword [ebp+V_ESTADO], S_VISIBLE
        jz .no
        test dword [ebp+V_ESTILO], E_TITULO
        jz .no
        test dword [ebp+V_ESTILO], E_EMERGENTE | E_HIJA
        jnz .no
        cmp ebp, [escritorio]
        je .no
        cmp dword [ebp+V_TEXTO], apa_tit    ; el cuadro «Salir»
        je .no
        clc
        ret
.no:
        stc
        ret

; (interna) EBP=ventana -> CF=0 si le toca un botón en la barra: las que se
; pueden minimizar (las ventanas principales) y, de un programa que no tiene
; ninguna así (uno de Windows que solo saca un cuadro, por ejemplo), la
; primera suya. Los cuadros de diálogo de un programa con ventana no salen
bt_vale:
        push eax
        push ecx
        push esi
        call bt_basica
        jc .no
        test dword [ebp+V_ESTILO], E_MINIM
        jnz .si
        mov eax, [ebp+V_TAREA]
        cmp eax, [tar_tabla]        ; (lo del sistema: el volumen)
        je .no
        mov esi, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp esi, ebp
        je .s
        cmp [esi+V_TAREA], eax
        jne .s
        xchg esi, ebp
        call bt_basica
        xchg esi, ebp
        jc .s
        test dword [esi+V_ESTILO], E_MINIM
        jnz .no
        cmp esi, ebp
        jb .no
.s:
        add esi, V_TAM
        dec ecx
        jnz .b
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

; (interna) EBP=ventana -> CF=1 si se ve como «la de delante»: es la activa,
; o está esperando a un cuadro suyo que es el activo
bt_delante:
        push eax
        cmp ebp, [activa]
        je .si
        test dword [ebp+V_ESTADO], S_INACTIVA
        jz .no
        mov eax, [activa]
        test eax, eax
        jz .no
        mov eax, [eax+V_TAREA]
        cmp eax, [ebp+V_TAREA]
        jne .no
.si:
        pop eax
        stc
        ret
.no:
        pop eax
        clc
        ret

; (interna) EBP=ventana -> EAX=la firma de lo que enseña su botón
bt_firmar:
        push ebx
        push esi
        mov eax, [ebp+V_ESTADO]
        and eax, S_MINIM
        call bt_delante
        jnc .f1
        or eax, 1
.f1:
        xor eax, [ebp+V_ICONO]
        mov esi, [ebp+V_TEXTO]
        test esi, esi
        jz .x
        mov ebx, 40
.t:
        movzx edx, byte [esi]
        test edx, edx
        jz .x
        imul eax, eax, 31
        add eax, edx
        inc esi
        dec ebx
        jnz .t
.x:
        pop esi
        pop ebx
        ret

; (interna) las medidas de la barra: bt_x1 (donde empieza la bandeja),
; bt_an_bot (de un botón al siguiente) y bt_vis (los que caben)
bt_medidas:
        push eax
        push ecx
        push edx
        mov eax, [vid_ancho]
        sub eax, [vol_x]
        add eax, BT_DESP - 24 - 12  ; (a la izquierda del icono de red)
        mov [bt_x1], eax
        sub eax, BT_X0
        jns .a
        xor eax, eax
.a:
        mov ecx, [bt_n]
        test ecx, ecx
        jnz .b
        inc ecx
.b:
        push eax
        xor edx, edx
        div ecx
        cmp eax, BT_BOT_MAX
        jbe .c
        mov eax, BT_BOT_MAX
.c:
        cmp eax, BT_BOT_MIN
        jae .d
        mov eax, BT_BOT_MIN
.d:
        mov [bt_an_bot], eax
        mov ecx, eax
        pop eax
        xor edx, edx
        div ecx
        cmp eax, [bt_n]
        jbe .e
        mov eax, [bt_n]
.e:
        mov [bt_vis], eax
        pop edx
        pop ecx
        pop eax
        ret

; (interna) EAX=x en la barra -> ECX=el botón de ventana que hay ahí (CF=1:
; ninguno)
bt_boton_en:
        push eax
        push edx
        call bt_medidas
        sub eax, BT_X0
        js .no
        xor edx, edx
        div dword [bt_an_bot]
        cmp eax, [bt_vis]
        jae .no
        mov ecx, [bt_an_bot]
        sub ecx, 3
        cmp edx, ecx
        jae .no
        mov ecx, eax
        pop edx
        pop eax
        clc
        ret
.no:
        pop edx
        pop eax
        stc
        ret

; =============================================================================
;  Una vez por vuelta del bucle del sistema
; =============================================================================
bt_vigilar:
        call bt_estilo              ; [ESTILO] el otro escritorio, si se ha pedido
        cmp dword [bt_ven], 0
        je .r
        pushad
        ; una ventana oculta no puede ser la activa (el Centro, oculto, es la
        ; dueña de los avisos de los programas que se abren desde el menú)
        mov ebp, [activa]
        test ebp, ebp
        jz .a1
        call ven_visible
        jnc .a1
        call ges_desactivar
        call ges_activar_otra
.a1:
        call ini_pendiente
        call wf_vigilar             ; [WIFI] cómo está la red (y lo que tarda)
        ; la tecla de Windows (la BIOS no la da: se mira en tec_mapa)
        mov al, [tec_mapa+0x5B]
        or al, [tec_mapa+0x5C]
        cmp al, [bt_win_ant]
        je .w
        mov [bt_win_ant], al
        test al, al
        jz .w
        call ev_act
        cmp dword [menu_ven], 0
        jne .w
        call ini_alternar
        cmp dword [mi_ven], 0       ; (como con Ctrl+Esc: la primera, marcada)
        je .w
        mov dword [mi_sel], 0
.w:
        xor edx, edx                ; ¿ha cambiado algo?
        ; fuera de la lista las que ya no están
        xor esi, esi
        xor edi, edi
.q:
        cmp esi, [bt_n]
        jae .qf
        mov ebp, [bt_lista+esi*4]
        call bt_vale
        jc .qs
        mov [bt_lista+edi*4], ebp
        mov eax, [bt_firma+esi*4]
        mov [bt_firma+edi*4], eax
        inc edi
        inc esi
        jmp .q
.qs:
        inc edx
        inc esi
        jmp .q
.qf:
        mov [bt_n], edi
        ; al final, las nuevas
        mov ebp, [ven_tabla]
        mov ecx, MAX_VEN
.n:
        call bt_vale
        jc .ns
        xor esi, esi
.n1:
        cmp esi, [bt_n]
        jae .n2
        cmp [bt_lista+esi*4], ebp
        je .ns
        inc esi
        jmp .n1
.n2:
        cmp esi, BT_MAX
        jae .ns
        mov [bt_lista+esi*4], ebp
        mov dword [bt_firma+esi*4], -1
        inc dword [bt_n]
        inc edx
.ns:
        add ebp, V_TAM
        dec ecx
        jnz .n
        ; lo que enseña cada una
        xor esi, esi
.f:
        cmp esi, [bt_n]
        jae .ff
        mov ebp, [bt_lista+esi*4]
        push edx
        call bt_firmar
        pop edx
        cmp eax, [bt_firma+esi*4]
        je .fs
        mov [bt_firma+esi*4], eax
        inc edx
.fs:
        inc esi
        jmp .f
.ff:
        ; la bandeja: hora, batería, red, volumen
        push edx
        call barra_firma            ; (gasta EDX)
        pop edx
        shl eax, 3
        xor al, [red_hay]
        xor eax, [wf_firma]         ; [WIFI] el icono de red
        mov ebx, [vol_mudo]
        and ebx, 1
        shl ebx, 1
        xor eax, ebx
        movzx ebx, byte [snd_hay]
        shl ebx, 2
        xor eax, ebx
        mov ebx, [vol_x]
        shl ebx, 24
        xor eax, ebx
        cmp eax, [bt_fbandeja]
        je .b1
        mov [bt_fbandeja], eax
        inc edx
.b1:
        test edx, edx
        jz .x
        mov ebp, [bt_ven]
        call ven_invalidar
.x:
        popad
.r:
        ret

; =============================================================================
;  La barra
; =============================================================================
bt_proc:
        cmp eax, WM_PAINT
        je bt_pintar
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_RBUTTONUP
        je ctx_def_rup
        cmp eax, WM_CONTEXTMENU
        je .ctx
        cmp eax, WM_COMMAND
        je .orden
.nada:
        xor eax, eax
        ret
.orden:
        cmp ecx, CE_CENTRO
        jne ctx_esc_orden
        call bt_centro_abrir
        jmp .nada
.ctx:
        mov eax, [area_y1]          ; el menú, encima de la barra (sin taparla)
        sub eax, SOMBRA
        mov [ctx_py], eax
        cmp edx, -1
        je .menu
        movsx eax, dx
        call bt_boton_en
        jc .menu
        mov ebp, [bt_lista+ecx*4]   ; en un botón: el menú de esa ventana
        mov eax, [ctx_px]
        mov ebx, [ctx_py]
        call ctx_sistema
        jmp .nada
.menu:
        mov esi, bt_l_ctx
        mov eax, -1
        xor ecx, ecx
        call ctx_menu
        jmp .nada
.pulsa:
        mov edi, eax
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp eax, BT_INI_X + BT_INI_AN
        jge .p1
        call ini_alternar           ; «Inicio» (hasta la esquina: no hay que afinar)
        jmp .nada
.p1:
        push eax
        sub eax, BT_DESP
        call wf_en_icono            ; [WIFI] el icono de red: su ventanita
        jc .red
        call vol_en_icono           ; el altavoz: abre o cierra el volumen
        pop eax
        jnc .p2
        cmp edi, WM_LBUTTONDBLCLK   ; con doble clic, el Mezclador
        je .mez
        call vol_alternar
        call bt_vol_colocar
        jmp .nada
.mez:
        cmp dword [vol_ven], 0      ; (el primer clic abrió la ventanita)
        je .mz
        call vol_alternar
.mz:
        mov eax, 1
        call mez_abrir
        jmp .nada
.p2:
        call bt_boton_en
        jc .nada
        mov ebp, [bt_lista+ecx*4]
        call bt_pulsar
        jmp .nada
.red:
        pop eax
        call wf_alternar
        jmp .nada

; (interna) la ventanita del volumen, encima de la barra (nace arriba)
bt_vol_colocar:
        pushad
        mov ebp, [vol_ven]
        test ebp, ebp
        jz .x
        mov eax, [ebp+V_X]
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        mov ebx, [area_y1]
        sub ebx, edx
        sub ebx, SOMBRA + 3
        call ven_mover
.x:
        popad
        ret

; (interna) un clic en el botón de la ventana EBP: minimizada, se recupera;
; si ya está delante, se minimiza; si no, pasa delante
bt_pulsar:
        pushad
        test dword [ebp+V_ESTADO], S_MINIM
        jz .vista
.recupera:
        mov eax, 4
        call ven_mostrar
        jmp .cuadro
; (be_lanzar) EBP=ventana: delante (recuperada si está minimizada), o su
; cuadro si tiene uno abierto; sin minimizarla si ya lo está
.trae0:
        pushad
        test dword [ebp+V_ESTADO], S_MINIM
        jnz .recupera
        jmp .trae
.vista:
        call bt_delante
        jnc .trae
        test dword [ebp+V_ESTADO], S_INACTIVA   ; (con un cuadro abierto, no)
        jnz .x
        test dword [ebp+V_ESTILO], E_MINIM
        jz .x
        mov eax, 2
        call ven_mostrar
        jmp .x
.trae:
        test dword [ebp+V_ESTADO], S_INACTIVA
        jnz .cuadro
        call ven_activar
        jmp .x
.cuadro:
        test dword [ebp+V_ESTADO], S_INACTIVA   ; la que espera a un cuadro:
        jz .x                                   ; delante, el cuadro
        mov eax, [ebp+V_TAREA]
        call tar_activar
.x:
        popad
        ret

; (interna) ESI=icono de 32x32 (a 4 bits por punto, como los lee dc_icono)
; EAX=x EBX=y: lo dibuja a mitad de tamaño (de cada cuatro puntos, el primero
; que no sea transparente)
bt_icono16:
        pushad
        mov edi, bt_ico16
        xor edx, edx                ; fila
.f:
        xor ecx, ecx                ; columna
.c:
        mov ebx, edx
        shl ebx, 5
        add ebx, ecx
        mov al, [esi+ebx]           ; fila 2f: los puntos 2c (bajo) y 2c+1 (alto)
        mov ah, [esi+ebx+16]        ; fila 2f+1
        mov bl, al
        and bl, 0x0F
        cmp bl, 3
        jne .pon
        mov bl, ah
        shr bl, 4
        cmp bl, 3
        jne .pon
        mov bl, al
        shr bl, 4
        cmp bl, 3
        jne .pon
        mov bl, ah
        and bl, 0x0F
        cmp bl, 3
        jne .pon
        mov bl, CLAVE
.pon:
        mov [edi], bl
        inc edi
        inc ecx
        cmp ecx, 16
        jb .c
        inc edx
        cmp edx, 16
        jb .f
        popad
        push ecx
        push edx
        push esi
        mov ecx, 16
        mov edx, 16
        mov esi, bt_ico16
        call dc_copiar_clave
        pop esi
        pop edx
        pop ecx
        ret

; [ICONOS] (ico_talla_poner) el menú Inicio con los iconos grandes: de 32 y
; cada orden más alta; con los normales y los pequeños, de 16, como siempre
bt_mi_tallas:
        mov dword [mi_h], 22
        mov dword [mi_ico], 16
        mov dword [mi_txy], 0
        cmp dword [ico_talla], 2
        jne .x
        mov dword [mi_h], 38
        mov dword [mi_ico], 32
        mov dword [mi_txy], 8
.x:
        ret
mi_h        dd 22
mi_ico      dd 16
mi_txy      dd 0

; (interna) ESI=icono EAX=x EBX=y: en el menú Inicio, del tamaño de mi_ico
bt_icono_mi:
        cmp dword [mi_ico], 32
        je dc_icono
        jmp bt_icono16

; (interna) un rombo pequeño (la marca de Hispa Ventanas) con el centro en
; EAX, EBX, del color puesto
bt_rombo:
        pushad
        mov esi, eax
        mov edi, ebx
        xor ebp, ebp
.l:
        mov eax, esi
        sub eax, ebp
        lea ecx, [ebp*2+1]
        mov edx, 1
        lea ebx, [edi+ebp-5]
        call dc_rect
        mov ebx, edi
        sub ebx, ebp
        add ebx, 5
        call dc_rect
        inc ebp
        cmp ebp, 6
        jb .l
        popad
        ret

bt_pintar:
        call bt_medidas
        call dc_pintar
        COLOR C_OCRE                ; el filete, arriba
        xor eax, eax
        xor ebx, ebx
        mov ecx, [vid_ancho]
        mov edx, 2
        call dc_rect
        COLOR C_MARRON
        mov ebx, 2
        mov edx, BT_H - 2
        call dc_rect
        ; ---- «Inicio» (se aclara mientras el menú está abierto) ----
        COLOR C_TERRACOTA
        cmp dword [mi_ven], 0
        je .i1
        COLOR C_OCRE
.i1:
        mov eax, BT_INI_X
        mov ebx, BT_INI_Y
        mov ecx, BT_INI_AN
        mov edx, BT_INI_AL
        call dc_rect
        COLOR C_CREMA
        call dc_marco
        COLOR C_OCRE
        cmp dword [mi_ven], 0
        je .i2
        COLOR C_MARRON
.i2:
        mov eax, BT_INI_X + 12
        mov ebx, BT_INI_Y + 9
        call bt_rombo
        COLOR C_CREMA
        cmp dword [mi_ven], 0
        je .i3
        COLOR C_TINTA
.i3:
        mov byte [gdi_negrita], 1
        mov eax, BT_INI_X + 22
        mov ebx, BT_INI_Y + 1
        mov esi, bt_t_inicio
        call dc_texto
        mov byte [gdi_negrita], 0
        ; ---- un botón por ventana ----
        xor edi, edi
.b:
        cmp edi, [bt_vis]
        jae .bf
        mov ebp, [bt_lista+edi*4]
        mov eax, edi
        imul eax, [bt_an_bot]
        add eax, BT_X0
        mov ebx, BT_BOT_Y
        mov ecx, [bt_an_bot]
        sub ecx, 3
        mov edx, BT_BOT_AL
        COLOR C_ARENA               ; detrás: arena
        test dword [ebp+V_ESTADO], S_MINIM
        jz .b1
        COLOR C_ARENA_OSC           ; minimizada: arena oscura
.b1:
        call bt_delante
        jnc .b2
        COLOR C_CREMA               ; delante: crema, con una raya terracota
.b2:
        call dc_rect
        call bt_delante
        jnc .b3
        COLOR C_TERRACOTA
        push ebx
        push edx
        add ebx, BT_BOT_AL - 2
        mov edx, 2
        call dc_rect
        pop edx
        pop ebx
.b3:
        mov esi, [ebp+V_ICONO]
        test esi, esi
        jnz .b4
        mov esi, ico_ventana
.b4:
        add eax, 3
        add ebx, 2
        call bt_icono16
        mov esi, [ebp+V_TEXTO]
        test esi, esi
        jz .b6
        sub ecx, 3 + 16 + 4 + 3     ; lo que queda para el título
        js .b6
        shr ecx, 3
        jz .b6
        push eax
        call cad_largo
        cmp eax, ecx
        jae .b5
        mov ecx, eax
.b5:
        pop eax
        add eax, 16 + 4
        COLOR C_TINTA
        call dc_texto_n
.b6:
        inc edi
        jmp .b
.bf:
        ; ---- la bandeja: red, volumen, batería y hora (los mismos dibujos de
        ; la barra de arriba, corridos al sitio que deja «Salir») ----
        COLOR C_ARENA_OSC
        mov eax, [bt_x1]
        add eax, 4
        mov ebx, 7
        mov ecx, 1
        mov edx, BT_H - 12
        call dc_rect
        add dword [dc_ox], BT_DESP
        inc dword [dc_oy]
        call vol_pintar_icono
        call bat_pintar_icono
        call wf_pintar_icono        ; [WIFI] el icono de red, con su estado
        call rel_pintar
        call dc_cerrar
        xor eax, eax
        ret

; =============================================================================
;  El menú Inicio
; =============================================================================
ini_alternar:
        cmp dword [mi_ven], 0
        je ini_abrir
ini_cerrar:
        pushad
        mov ebp, [ms_ven]
        mov dword [ms_ven], 0
        test ebp, ebp
        jz .c1
        call ven_destruir
.c1:
        mov ebp, [mi_ven]
        mov dword [mi_ven], 0
        test ebp, ebp
        jz .c2
        call ven_destruir
.c2:
        mov dword [ms_grupo], -1
        mov dword [mi_sel], -1
        mov byte [ini_nivel], 0
        call bt_inv_inicio
        popad
        ret

; (interna) el botón «Inicio», a repintar
bt_inv_inicio:
        pushad
        mov ebp, [bt_ven]
        test ebp, ebp
        jz .x
        xor eax, eax
        xor ebx, ebx
        mov ecx, BT_X0
        mov edx, BT_H
        call ven_invalidar_rect
.x:
        popad
        ret

; (interna) ECX=orden del menú -> ESI=su texto EDX=su icono
ini_texto:
        push eax
        mov eax, [mi_dato+ecx*4]
        cmp byte [mi_tipo+ecx], 1
        jne .cmd
        lea esi, [eax+GR_NOMBRE]
        mov edx, ico_carpeta
        pop eax
        ret
.cmd:
        mov esi, [ini_cmds+eax*8]
        mov edx, [ini_cmds+eax*8+4]
        pop eax
        ret

ini_abrir:
        pushad
        cmp dword [bt_ven], 0
        je .x
        cmp dword [mi_ven], 0
        jne .x
        call menu_cerrar
        ; las órdenes: un grupo del Centro por línea, una raya y las fijas
        xor edi, edi
        mov ebx, [gr_tabla]
        mov ecx, MAX_GR
.g:
        cmp byte [ebx+GR_USO], 0
        je .gs
        test byte [ebx+GR_BAND], GRB_ESCRITORIO ; (los del escritorio, no:
        jnz .gs                     ; sus accesos ya están a la vista)
        mov byte [mi_tipo+edi], 1
        mov [mi_dato+edi*4], ebx
        inc edi
.gs:
        add ebx, GR_TAM
        dec ecx
        jnz .g
        test edi, edi
        jz .f0
        mov byte [mi_tipo+edi], 0
        inc edi
.f0:
        xor eax, eax
.f:
        cmp eax, 4                  ; antes de «Salir», otra raya
        jne .f1
        mov byte [mi_tipo+edi], 0
        inc edi
.f1:
        mov byte [mi_tipo+edi], 2
        mov [mi_dato+edi*4], eax
        inc edi
        inc eax
        cmp eax, 5
        jb .f
        mov [mi_n], edi
        ; dónde va cada una, el alto y el texto más largo
        xor ecx, ecx
        mov ebx, MI_CAB + 2
        mov edi, 14                 ; («Hispa Ventanas», en la cabecera)
.m:
        cmp ecx, [mi_n]
        jae .mf
        mov [mi_y+ecx*4], ebx
        cmp byte [mi_tipo+ecx], 0
        jne .m1
        add ebx, MI_SEP
        jmp .m2
.m1:
        add ebx, [mi_h]
        call ini_texto
        call cad_largo
        cmp eax, edi
        jbe .m2
        mov edi, eax
.m2:
        inc ecx
        jmp .m
.mf:
        add ebx, 2 + 2              ; (y el filete de la emergente)
        lea ecx, [edi*8 + 6 + 6 + 20 + 2]
        add ecx, [mi_ico]           ; [ICONOS]
        lea eax, [ecx-2]
        mov [mi_an], eax
        add ecx, MI_FR              ; [FRANJA] y la franja, a la izquierda
        cmp ebx, MI_FR_MIN          ; (alta como para que quepa el nombre)
        jge .alto
        mov ebx, MI_FR_MIN
.alto:
        call ini_franja_hacer       ; [FRANJA] (EBX=el alto)
        sub esp, P_TAM
        mov esi, esp
        mov dword [esi+P_CLASE], c_inicio
        mov dword [esi+P_TEXTO], 0
        mov dword [esi+P_ESTILO], E_EMERGENTE
        mov dword [esi+P_X], 2
        mov eax, [area_y1]
        sub eax, ebx
        sub eax, SOMBRA
        jns .y
        xor eax, eax
.y:
        mov [esi+P_Y], eax
        mov [esi+P_AN], ecx
        mov [esi+P_AL], ebx
        mov dword [esi+P_ID], 0
        xor ebp, ebp
        call ven_crear
        add esp, P_TAM
        mov [mi_ven], eax
        test eax, eax
        jz .x
        mov dword [mi_sel], -1
        mov dword [ms_grupo], -1
        mov byte [ini_nivel], 0
        mov ebp, eax
        mov ecx, 1                  ; el temporizador que abre el submenú del
        mov edx, 110                ; grupo sobre el que se para el ratón
        call tmp_poner
        call bt_inv_inicio
.x:
        popad
        ret

; (interna) cierra el submenú
ini_sub_cerrar:
        pushad
        mov ebp, [ms_ven]
        mov dword [ms_ven], 0
        mov dword [ms_grupo], -1
        mov byte [ini_nivel], 0
        test ebp, ebp
        jz .x
        call ven_destruir
        mov ebp, [mi_ven]
        test ebp, ebp
        jz .x
        call ven_invalidar
.x:
        popad
        ret

; (interna) ECX=orden del menú (un grupo): abre su submenú con los programas
ini_sub_abrir:
        pushad
        cmp ecx, [ms_grupo]
        je .x
        call ini_sub_cerrar
        cmp byte [mi_tipo+ecx], 1
        jne .x
        mov [ms_grupo], ecx
        mov eax, [mi_dato+ecx*4]
        mov [ms_gr], eax
        call gr_num
        mov edx, eax                ; DL=número del grupo
        ; sus accesos directos (los que se ven en su ventana) y el más largo
        xor edi, edi
        mov ebp, 7                  ; («(vacío)»)
        mov ebx, [ac_tabla]
        mov ecx, MAX_AC
.a:
        cmp byte [ebx+AC_USO], 0
        je .as
        cmp [ebx+AC_GRUPO], dl
        jne .as
        test byte [ebx+AC_BAND2], AB2_OCULTO
        jnz .as
        mov [ms_it+edi*4], ebx
        inc edi
        lea esi, [ebx+AC_NOMBRE]
        call cad_largo
        cmp eax, ebp
        jbe .as
        mov ebp, eax
.as:
        add ebx, AC_TAM
        dec ecx
        jnz .a
        test edi, edi
        jnz .hay
        mov dword [ms_it], 0
        inc edi
.hay:
        mov byte [ms_mas], 0
        lea eax, [ebp*8 + 6 + 6 + 8]
        add eax, [mi_ico]           ; [ICONOS]
        mov [ms_colan], eax
        ; filas que caben de arriba abajo y columnas que hacen falta
        mov eax, [area_y1]
        sub eax, SOMBRA + 8
        xor edx, edx
        mov ecx, [mi_h]
        div ecx
        test eax, eax
        jnz .f1
        inc eax
.f1:
        mov ebx, eax                ; EBX=filas, como mucho
        lea eax, [edi+ebx-1]
        xor edx, edx
        div ebx
        mov ecx, eax                ; ECX=columnas
        ; las columnas que caben a lo ancho (al lado del menú o, si no, encima)
        mov eax, [vid_ancho]
        sub eax, SOMBRA + 2
        xor edx, edx
        div dword [ms_colan]
        test eax, eax
        jnz .f2
        inc eax
.f2:
        cmp ecx, eax
        jbe .f3
        mov ecx, eax                ; no caben todos: la última, «Más...»
        mov eax, ecx
        imul eax, ebx
        mov edi, eax
        mov byte [ms_mas], 1
.f3:
        mov [ms_n], edi
        lea eax, [edi+ecx-1]        ; las filas, repartidas
        xor edx, edx
        div ecx
        mov [ms_filas], eax
        mov ebx, eax
        imul ebx, [mi_h]
        add ebx, 4 + 2              ; EBX=alto
        imul ecx, [ms_colan]
        add ecx, 2                  ; ECX=ancho
        mov ebp, [mi_ven]
        sub esp, P_TAM
        mov esi, esp
        mov dword [esi+P_CLASE], c_inicio
        mov dword [esi+P_TEXTO], 0
        mov dword [esi+P_ESTILO], E_EMERGENTE
        mov eax, [ebp+V_X]
        add eax, [ebp+V_AN]
        sub eax, 3
        mov edx, [vid_ancho]
        sub edx, ecx
        sub edx, SOMBRA
        cmp eax, edx
        jle .x1
        mov eax, edx
.x1:
        test eax, eax
        jns .x2
        xor eax, eax
.x2:
        mov [esi+P_X], eax
        mov eax, [ms_grupo]
        mov eax, [mi_y+eax*4]
        add eax, [ebp+V_Y]
        dec eax
        mov edx, [area_y1]
        sub edx, ebx
        sub edx, SOMBRA
        cmp eax, edx
        jle .y1
        mov eax, edx
.y1:
        test eax, eax
        jns .y2
        xor eax, eax
.y2:
        mov [esi+P_Y], eax
        mov [esi+P_AN], ecx
        mov [esi+P_AL], ebx
        mov dword [esi+P_ID], 1
        xor ebp, ebp
        call ven_crear
        add esp, P_TAM
        mov [ms_ven], eax
        mov dword [ms_sel], -1
        test eax, eax
        jnz .inv
        mov dword [ms_grupo], -1
.inv:
        mov ebp, [mi_ven]
        call ven_invalidar
.x:
        popad
        ret

; (interna) EBX=y en la zona del menú -> EAX=la orden que hay ahí (-1: una
; raya, la cabecera o nada)
ini_orden_en:
        push ecx
        xor ecx, ecx
.b:
        cmp ecx, [mi_n]
        jae .no
        cmp byte [mi_tipo+ecx], 0
        je .s
        mov eax, [mi_y+ecx*4]
        cmp ebx, eax
        jl .s
        add eax, [mi_h]
        cmp ebx, eax
        jl .si
.s:
        inc ecx
        jmp .b
.si:
        mov eax, ecx
        pop ecx
        ret
.no:
        mov eax, -1
        pop ecx
        ret

; (interna) EAX=x EBX=y en la zona del submenú -> EAX=el programa (-1: nada)
ini_prog_en:
        push ecx
        push edx
        sub ebx, 2
        js .no
        test eax, eax
        js .no
        xor edx, edx
        div dword [ms_colan]
        mov ecx, eax                ; la columna
        mov eax, ebx
        xor edx, edx
        push ebx
        mov ebx, [mi_h]
        div ebx
        pop ebx
        cmp eax, [ms_filas]
        jae .no
        imul ecx, [ms_filas]
        add eax, ecx
        cmp eax, [ms_n]
        jae .no
        pop edx
        pop ecx
        ret
.no:
        mov eax, -1
        pop edx
        pop ecx
        ret

; el procedimiento del menú y del submenú (EBP = ms_ven: el submenú)
ini_proc:
        cmp eax, WM_PAINT
        je ini_pintar
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_TIMER
        je .timer
.nada:
        xor eax, eax
        ret
.timer:
        cmp ebp, [mi_ven]
        jne .nada
        cmp byte [ini_nivel], 0
        jne .nada
        mov ecx, [mi_sel]
        test ecx, ecx
        js .nada
        cmp ecx, [ms_grupo]
        je .nada
        call tics
        sub eax, [ini_tic]
        cmp eax, 5                  ; un cuarto de segundo parado en la orden
        jb .nada
        cmp byte [mi_tipo+ecx], 1
        je .t1
        call ini_sub_cerrar
        jmp .nada
.t1:
        call ini_sub_abrir
        jmp .nada
.mueve:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebp, [ms_ven]
        je .msub
        cmp eax, MI_FR              ; [FRANJA] (en la franja no hay órdenes)
        jl .nada
        call ini_orden_en
        test eax, eax
        js .nada
        cmp byte [ini_nivel], 0
        jne .m0
        cmp eax, [mi_sel]
        je .nada
.m0:
        mov [mi_sel], eax
        mov byte [ini_nivel], 0
        mov dword [ms_sel], -1
        call tics
        mov [ini_tic], eax
        jmp .inv2
.msub:
        call ini_prog_en
        test eax, eax
        js .nada
        cmp byte [ini_nivel], 1
        jne .m1
        cmp eax, [ms_sel]
        je .nada
.m1:
        mov [ms_sel], eax
        mov byte [ini_nivel], 1
        mov eax, [ms_grupo]
        mov [mi_sel], eax
.inv2:
        call ini_invalidar
        jmp .nada
.pulsa:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebp, [ms_ven]
        je .psub
        cmp eax, MI_FR              ; [FRANJA]
        jl .nada
        call ini_orden_en
        test eax, eax
        js .nada
        mov [mi_sel], eax
        mov byte [ini_nivel], 0
        mov ecx, eax
        cmp byte [mi_tipo+ecx], 1
        jne .pcmd
        call ini_sub_abrir          ; un grupo: su submenú, ya
        jmp .inv2
.pcmd:
        mov dword [ini_pend], INI_P_ORDEN
        mov [ini_dato], ecx
        jmp .nada
.psub:
        call ini_prog_en
        test eax, eax
        js .nada
        mov dword [ini_pend], INI_P_PROG
        mov [ini_dato], eax
        jmp .nada

; (interna) el menú y el submenú, a repintar
ini_invalidar:
        push ebp
        mov ebp, [mi_ven]
        test ebp, ebp
        jz .a
        call ven_invalidar
.a:
        mov ebp, [ms_ven]
        test ebp, ebp
        jz .b
        call ven_invalidar
.b:
        pop ebp
        ret

; (interna) el triángulo que dice «aquí hay un submenú», en EAX, EBX
ini_flecha:
        pushad
        xor esi, esi
.l:
        lea ecx, [esi+1]
        mov edx, 1
        push eax
        push ebx
        add ebx, esi
        call dc_rect
        add ebx, 8
        sub ebx, esi
        sub ebx, esi
        call dc_rect
        pop ebx
        pop eax
        inc esi
        cmp esi, 4
        jb .l
        mov ecx, 5
        mov edx, 1
        add ebx, 4
        call dc_rect
        popad
        ret

ini_pintar:
        cmp ebp, [ms_ven]
        je sub_pintar
        call dc_pintar
        call ini_franja             ; [FRANJA] y lo demás, a su derecha
        add dword [dc_ox], MI_FR
        ; la cabecera: el nombre de la casa, que ya no está en la barra de arriba
        COLOR C_MARRON
        xor eax, eax
        xor ebx, ebx
        mov ecx, [mi_an]
        mov edx, MI_CAB - 2
        call dc_rect
        COLOR C_OCRE
        mov ebx, MI_CAB - 2
        mov edx, 2
        call dc_rect
        mov eax, 13
        mov ebx, 12
        call bt_rombo
        COLOR C_CREMA
        mov byte [gdi_negrita], 1
        mov eax, 26
        mov ebx, 4
        mov esi, tit_barra
        call dc_texto
        mov byte [gdi_negrita], 0
        xor edi, edi
.o:
        cmp edi, [mi_n]
        jae .f
        mov ebx, [mi_y+edi*4]
        cmp byte [mi_tipo+edi], 0
        jne .o1
        COLOR C_ARENA_OSC           ; una raya
        mov eax, 4
        add ebx, 3
        mov ecx, [mi_an]
        sub ecx, 8
        mov edx, 1
        call dc_rect
        jmp .o9
.o1:
        mov ecx, edi
        call ini_texto              ; ESI=texto EDX=icono
        push edx
        cmp edi, [mi_sel]
        jne .o2
        COLOR C_ANIL                ; la marcada (con el submenú abierto y el
        cmp byte [ini_nivel], 0     ; teclado en él, en arena oscura)
        je .o1b
        COLOR C_ARENA_OSC
.o1b:
        mov eax, 2
        mov ecx, [mi_an]
        sub ecx, 4
        mov edx, [mi_h]
        call dc_rect
.o2:
        pop edx
        push esi
        mov esi, edx
        mov eax, 6
        add ebx, 3
        call bt_icono_mi            ; [ICONOS] de 16 o, con los grandes, de 32
        pop esi
        COLOR C_TINTA
        cmp edi, [mi_sel]
        jne .o3
        COLOR C_BLANCO
.o3:
        mov eax, [mi_ico]
        add eax, 6 + 6
        add ebx, [mi_txy]
        call dc_texto
        cmp byte [mi_tipo+edi], 1
        jne .o9
        mov eax, [mi_an]
        sub eax, 14
        add ebx, 3
        call ini_flecha
.o9:
        inc edi
        jmp .o
.f:
        sub dword [dc_ox], MI_FR
        call dc_cerrar
        xor eax, eax
        ret

; =============================================================================
;  [FRANJA] La franja del menú Inicio: a la izquierda, de arriba abajo, con
;  un degradado propio (ámbar arriba, terracota oscuro abajo, con la trama
;  de Bayer para que no se vean escalones con 256 colores) y el nombre de la
;  edición («Hispa-486», de EDICION.INI; sin ella, «Hispa-386») girado: se
;  lee de abajo arriba, a doble tamaño y en negrita, «Hispa» en crema y lo
;  de detrás del guion en blanco; abajo, el disquete. Pedido de Carlos: que
;  el menú diga qué sistema es, como Hispa-Pentium, al estilo de su época.
;  Se dibuja en bt_fr_buf al abrir el menú (ini_abrir) y al pintar se copia
;  de una vez.
; =============================================================================
MI_FR_MIN   equ 200                 ; el menú, alto al menos esto (cabe el nombre)
MI_FR_MAXAL equ 1600
fr_paradas  db 5, 3, 0,  5, 2, 0,  4, 1, 0,  3, 1, 0,  2, 0, 0   ; rojo, verde, azul (0-5) de arriba abajo
fr_bayer    db 0, 8, 2, 10,  12, 4, 14, 6,  3, 11, 1, 9,  15, 7, 13, 5
bt_n_edic   db "EDICION.INI", 0
bt_k_nombre db "nombre", 0
bt_ed_def   db "Hispa-386", 0

; (bt_crear) el nombre de la edición: «Nombre=» de EDICION.INI (al lado de
; VENTANAS.COM; la pone la instalación de Hispa-486) -> bt_edicion
bt_edicion_leer:
        pushad
        mov esi, bt_ed_def
        mov edi, bt_edicion
        mov ecx, 15
        call cad_copiar
        call dir_propia_poner
        mov edi, cfg_ruta
        mov esi, dir_propia
        call pon_cad
        mov esi, bt_n_edic
        call pon_cad
        mov byte [edi], 0
        mov esi, cfg_ruta
        xor al, al
        call dos_abrir
        jc .x
        mov ebx, eax
        mov edi, bt_buf
        mov ecx, 1023
        call dos_leer
        jnc .l
        xor eax, eax
.l:
        call dos_cerrar
        mov byte [bt_buf+eax], 0
        mov esi, bt_buf
.linea:
        mov edi, bt_k_nombre
        mov edx, esi
.c:
        mov al, [edi]
        test al, al
        jz .fk
        mov ah, [edx]
        or ah, 0x20
        cmp al, ah
        jne .salta
        inc edi
        inc edx
        jmp .c
.fk:
        cmp byte [edx], '='
        jne .salta
        inc edx
        mov edi, bt_edicion
        mov ecx, 15
.v:
        mov al, [edx]
        cmp al, ' '
        jb .vf
        mov [edi], al
        inc edi
        inc edx
        dec ecx
        jnz .v
.vf:
        mov byte [edi], 0
        cmp edi, bt_edicion
        jne .x
        mov esi, bt_ed_def          ; (vacío: el de siempre)
        mov ecx, 15
        call cad_copiar
        jmp .x
.salta:
        mov al, [esi]
        test al, al
        jz .x
        inc esi
        cmp al, 10
        jne .salta
        jmp .linea
.x:
        popad
        ret

; (ini_abrir) EBX=el alto del menú: la franja, a bt_fr_buf
ini_franja_hacer:
        pushad
        cmp ebx, MI_FR_MAXAL
        jbe .a
        mov ebx, MI_FR_MAXAL
.a:
        mov [bt_fr_al], ebx
        mov edi, bt_fr_buf
        xor edx, edx                ; EDX=fila
.f:
        mov eax, edx                ; dónde va, de 0 a 64 (4 tramos de 16)
        shl eax, 6
        push edx
        xor edx, edx
        div dword [bt_fr_al]
        pop edx
        mov ebp, eax
        shr ebp, 4                  ; EBP=tramo
        and eax, 15                 ; EAX=cuánto del siguiente
        xor ecx, ecx                ; ECX=columna
.c:
        mov esi, edx
        and esi, 3
        shl esi, 2
        mov ebx, ecx
        and ebx, 3
        add esi, ebx
        movzx ebx, byte [fr_bayer+esi]
        mov esi, ebp
        cmp eax, ebx
        jbe .p
        inc esi
.p:
        cmp esi, 4
        jbe .p2
        mov esi, 4
.p2:
        imul esi, 3
        movzx ebx, byte [fr_paradas+esi]    ; 16 + 36r + 6g + b
        imul ebx, 36
        movzx esi, byte [fr_paradas+esi+1]
        imul esi, 6
        add ebx, esi
        add ebx, 16
        cmp byte [vid_bits], 4      ; (en la VGA de 16 colores, terracota lisa)
        jne .p3
        mov ebx, C_TERRACOTA
.p3:
        mov [edi], bl
        inc edi
        inc ecx
        cmp ecx, MI_FR
        jb .c
        inc edx
        cmp edx, [bt_fr_al]
        jb .f
        ; un filete oscuro a la derecha
        mov edi, bt_fr_buf + MI_FR - 1
        mov ecx, [bt_fr_al]
.r:
        mov byte [edi], C_MARRON
        add edi, MI_FR
        loop .r
        ; el nombre, de abajo arriba: la sombra (marrón) y encima la letra
        mov esi, bt_edicion
        call cad_largo
        mov ecx, eax
        mov ebp, [bt_fr_al]
        sub ebp, 34                 ; (debajo, el disquete)
        mov eax, 1                  ; (la sombra, un punto a la derecha y abajo)
        mov byte [fr_color], C_MARRON
        mov byte [fr_claro], C_MARRON
        call fr_texto
        xor eax, eax
        mov byte [fr_color], C_CREMA
        mov byte [fr_claro], C_BLANCO
        call fr_texto
        popad
        ret

; (interna) ESI=texto ECX=letras EBP=dónde empieza abajo EAX=corrimiento:
; el texto girado (de abajo arriba), a doble tamaño y en negrita, en
; bt_fr_buf; hasta el guion con fr_color y desde él con fr_claro
fr_texto:
        pushad
        mov [fr_corre], eax
        mov dl, [fr_color]
        mov [fr_ahora], dl
.ch:
        test ecx, ecx
        jz .x
        movzx edi, byte [esi]
        cmp edi, '-'
        jne .ch1
        mov dl, [fr_claro]
        mov [fr_ahora], dl
.ch1:
        shl edi, 4
        add edi, fuente8x16
        xor ebx, ebx                ; EBX=fila de la letra (0-15)
.fila:
        movzx edx, byte [edi+ebx]
        mov eax, edx                ; negrita: y un punto a la derecha
        shr eax, 1
        or edx, eax
        push ecx
        xor ecx, ecx                ; ECX=columna (0-7)
.col:
        test edx, 0x80
        jz .cs
        ; (columna c, fila r) -> x = 4 + 2r, y = EBP - 2(c+1)
        pushad
        lea eax, [ebx*2+4]
        add eax, [fr_corre]
        mov edx, ebp
        lea ecx, [ecx*2+2]
        sub edx, ecx
        add edx, [fr_corre]
        call fr_2x2
        popad
.cs:
        shl edx, 1
        inc ecx
        cmp ecx, 8
        jb .col
        pop ecx
        inc ebx
        cmp ebx, 16
        jb .fila
        inc esi
        sub ebp, 16
        dec ecx
        jmp .ch
.x:
        popad
        ret

; (interna) EAX=x EDX=y: un cuadrado de 2x2 con fr_ahora (si cabe)
fr_2x2:
        test edx, edx
        js .x
        lea ecx, [edx+1]
        cmp ecx, [bt_fr_al]
        jae .x
        cmp eax, MI_FR - 2
        ja .x
        imul edx, MI_FR
        add edx, eax
        mov cl, [fr_ahora]
        mov [bt_fr_buf+edx], cl
        mov [bt_fr_buf+edx+1], cl
        mov [bt_fr_buf+edx+MI_FR], cl
        mov [bt_fr_buf+edx+MI_FR+1], cl
.x:
        ret

; (ini_pintar, con el contexto abierto) la franja y el disquete
ini_franja:
        pushad
        xor eax, eax
        xor ebx, ebx
        mov ecx, MI_FR
        mov edx, [bt_fr_al]
        mov esi, bt_fr_buf
        call dc_copiar
        mov esi, ico_disco
        mov eax, (MI_FR - 1 - 16) / 2
        mov ebx, [bt_fr_al]
        sub ebx, 24
        call bt_icono16
        popad
        ret
fr_color    db 0
fr_claro    db 0
fr_ahora    db 0
            align 4
fr_corre    dd 0

; (interna) ECX=programa del submenú -> ESI=su texto; EBX=el acceso (0: no
; es un acceso: «(vacío)» o «Más...»)
sub_texto:
        mov ebx, [ms_it+ecx*4]
        cmp byte [ms_mas], 0
        je .n
        push eax
        mov eax, [ms_n]
        dec eax
        cmp ecx, eax
        pop eax
        jne .n
        xor ebx, ebx
        mov esi, bt_t_mas
        ret
.n:
        mov esi, bt_t_vacio
        test ebx, ebx
        jz .x
        lea esi, [ebx+AC_NOMBRE]
.x:
        ret

sub_pintar:
        call dc_pintar
        xor edi, edi
.o:
        cmp edi, [ms_n]
        jae .f
        mov eax, edi
        xor edx, edx
        div dword [ms_filas]        ; EAX=columna EDX=fila
        imul eax, [ms_colan]
        imul edx, [mi_h]
        lea ebx, [edx+2]
        mov ecx, edi
        push eax
        push ebx
        call sub_texto              ; ESI=texto EBX=acceso
        mov edx, ebx
        pop ebx
        pop eax
        cmp edi, [ms_sel]
        jne .o2
        test edx, edx               ; («(vacío)» no se marca)
        jnz .o1
        cmp byte [ms_mas], 0
        je .o2
.o1:
        COLOR C_ANIL
        push eax
        push edx
        add eax, 2
        mov ecx, [ms_colan]
        sub ecx, 4
        mov edx, [mi_h]
        call dc_rect
        pop edx
        pop eax
.o2:
        test edx, edx
        jz .o4
        push esi                    ; el icono del acceso (un .HVP trae el suyo)
        push eax
        movzx eax, byte [edx+AC_ICONO]
        cmp eax, ICO_N
        jb .o3
        xor eax, eax
.o3:
        mov esi, [ico_tabla+eax*8]
        pop eax
        push ebx
        mov ebx, edx
        call hvp_icono
        pop ebx
        add eax, 6
        add ebx, 3
        call bt_icono_mi            ; [ICONOS]
        sub eax, 6
        sub ebx, 3
        pop esi
.o4:
        COLOR C_TINTA
        test edx, edx
        jnz .o5
        cmp byte [ms_mas], 0
        jne .o5
        COLOR C_ARENA_OSC           ; «(vacío)», en gris
        jmp .o6
.o5:
        cmp edi, [ms_sel]
        jne .o6
        COLOR C_BLANCO
.o6:
        add eax, [mi_ico]
        add eax, 6 + 6
        add ebx, 3
        add ebx, [mi_txy]
        call dc_texto
        inc edi
        jmp .o
.f:
        call dc_cerrar
        xor eax, eax
        ret

; ---- el teclado ----
; (ctx_tecla, de ges_tecla) AX=tecla de INT 16h -> CF=1 si era para el menú
; Inicio: Ctrl+Esc lo abre; abierto, todas son suyas
bt_tecla:
        cmp dword [bt_ven], 0
        je .no
        cmp dword [mi_ven], 0
        jne .mia
        cmp al, 27
        jne be_tecla                ; (las teclas del escritorio)
        test byte [tec_mayus], TM_CTRL
        jz be_tecla
        call ini_abrir
        cmp dword [mi_ven], 0
        je .si
        mov dword [mi_sel], 0
        jmp .si
.mia:
        pushad
        call ini_tecla
        popad
.si:
        stc
        ret
.no:
        clc
        ret

; (interna) EAX=-1 o +1: marca la orden anterior o la siguiente (sin rayas)
ini_mover:
        pushad
        mov ecx, [mi_sel]
        mov edx, [mi_n]
.b:
        add ecx, eax
        jns .b1
        mov ecx, [mi_n]
        dec ecx
.b1:
        cmp ecx, [mi_n]
        jb .b2
        xor ecx, ecx
.b2:
        cmp byte [mi_tipo+ecx], 0
        jne .hay
        dec edx
        jnz .b
        jmp .x
.hay:
        mov [mi_sel], ecx
        cmp ecx, [ms_grupo]
        je .x
        call ini_sub_cerrar
.x:
        popad
        ret

; (interna) AL=letra, ECX=por dónde se empieza, EDX=cuántas hay, EBX=0 el
; menú / 1 el submenú -> EAX=la siguiente que empieza por esa letra (-1: no)
ini_letra:
        push esi
        push edi
        push ebp
        mov edi, edx
        mov ah, al
        or ah, 0x20
        mov ebp, ebx
.b:
        test edi, edi
        jz .no
        dec edi
        inc ecx
        cmp ecx, edx
        jb .b1
        xor ecx, ecx
.b1:
        test ebp, ebp
        jnz .sub
        cmp byte [mi_tipo+ecx], 0
        je .b
        push edx
        call ini_texto
        pop edx
        jmp .mira
.sub:
        push ebx
        call sub_texto
        test ebx, ebx
        pop ebx
        jnz .mira
        cmp byte [ms_mas], 0
        je .b
.mira:
        mov al, [esi]
        or al, 0x20
        cmp al, ah
        jne .b
        mov eax, ecx
        jmp .x
.no:
        mov eax, -1
.x:
        pop ebp
        pop edi
        pop esi
        ret

; (interna) AX=tecla, con el menú abierto
ini_tecla:
        cmp al, 27
        jne .t1
        test byte [tec_mayus], TM_CTRL
        jnz .cerrar
        cmp byte [ini_nivel], 0
        je .cerrar
        call ini_sub_cerrar
        jmp .inv
.cerrar:
        mov dword [ini_pend], INI_P_CERRAR
        ret
.t1:
        cmp al, 13
        je .intro
        test al, al
        jz .ext
        cmp al, 0xE0
        je .ext
        cmp al, 32
        jbe .x
        ; una letra: la siguiente orden que empieza por ella
        xor ebx, ebx
        mov ecx, [mi_sel]
        mov edx, [mi_n]
        cmp byte [ini_nivel], 0
        je .l1
        inc ebx
        mov ecx, [ms_sel]
        mov edx, [ms_n]
.l1:
        call ini_letra
        test eax, eax
        js .x
        test ebx, ebx
        jnz .l2
        mov [mi_sel], eax
        cmp eax, [ms_grupo]
        je .inv
        call ini_sub_cerrar
        jmp .inv
.l2:
        mov [ms_sel], eax
        jmp .inv
.ext:
        cmp ah, 0x6B                ; Alt+F4
        je .cerrar
        cmp byte [ini_nivel], 0
        jne .sub
        cmp ah, TC_ARRIBA
        jne .e1
        mov eax, -1
        call ini_mover
        jmp .inv
.e1:
        cmp ah, TC_ABAJO
        jne .e2
        mov eax, 1
        call ini_mover
        jmp .inv
.e2:
        cmp ah, TC_DER
        jne .x
.entra:
        mov ecx, [mi_sel]           ; a la derecha (o Intro) en un grupo: dentro
        test ecx, ecx
        js .x
        cmp byte [mi_tipo+ecx], 1
        jne .x
        call ini_sub_abrir
        cmp dword [ms_ven], 0
        je .x
        mov byte [ini_nivel], 1
        mov dword [ms_sel], 0
        jmp .inv
.intro:
        cmp byte [ini_nivel], 0
        jne .isub
        mov ecx, [mi_sel]
        test ecx, ecx
        js .x
        cmp byte [mi_tipo+ecx], 1
        je .entra
        mov dword [ini_pend], INI_P_ORDEN
        mov [ini_dato], ecx
        ret
.isub:
        mov eax, [ms_sel]
        test eax, eax
        js .x
        mov dword [ini_pend], INI_P_PROG
        mov [ini_dato], eax
        ret
.sub:
        mov ecx, [ms_sel]
        cmp ah, TC_ARRIBA
        jne .s1
        dec ecx
        jns .spon
        mov ecx, [ms_n]
        dec ecx
        jmp .spon
.s1:
        cmp ah, TC_ABAJO
        jne .s2
        inc ecx
        cmp ecx, [ms_n]
        jb .spon
        xor ecx, ecx
        jmp .spon
.s2:
        cmp ah, TC_DER
        jne .s3
        add ecx, [ms_filas]
        cmp ecx, [ms_n]
        jb .spon
        jmp .x
.s3:
        cmp ah, TC_IZQ
        jne .x
        sub ecx, [ms_filas]
        jns .spon
        call ini_sub_cerrar         ; desde la primera columna: fuera
        jmp .inv
.spon:
        mov [ms_sel], ecx
.inv:
        call ini_invalidar
.x:
        ret

; ---- el ratón fuera del menú ----
; (ctx_ctrl_clic, de eventos) BL=botón izquierdo BH=derecho, como llegan: si
; se aprieta uno fuera del menú Inicio (y del botón «Inicio», que lo cierra
; él), el menú se cierra; el clic sigue su camino
bt_clic_fuera:
        cmp dword [mi_ven], 0
        je .r
        push eax
        mov al, bl
        or al, bh
        jz .p
        mov al, [rat_boton]
        or al, [rat_der]
        jnz .p
        pushad
        mov eax, [rat_x]
        mov ebx, [rat_y]
        mov ebp, [mi_ven]
        call .dentro
        jc .q
        mov ebp, [ms_ven]
        test ebp, ebp
        jz .f1
        call .dentro
        jc .q
.f1:
        cmp ebx, [area_y1]          ; el botón «Inicio»
        jl .f2
        cmp eax, BT_INI_X + BT_INI_AN
        jl .q
.f2:
        mov dword [ini_pend], INI_P_CERRAR
.q:
        popad
.p:
        pop eax
.r:
        ret
; EAX, EBX dentro de la ventana EBP (con algo de margen) -> CF=1
.dentro:
        mov ecx, eax
        sub ecx, [ebp+V_X]
        add ecx, 2
        js .dn
        mov edx, [ebp+V_AN]
        add edx, 4
        cmp ecx, edx
        jge .dn
        mov ecx, ebx
        sub ecx, [ebp+V_Y]
        add ecx, 2
        js .dn
        mov edx, [ebp+V_AL]
        add edx, 4
        cmp ecx, edx
        jge .dn
        stc
        ret
.dn:
        clc
        ret

; ---- lo que se ha elegido se hace en la vuelta siguiente del bucle del
; sistema (no dentro del procedimiento de una ventana que hay que destruir)
ini_pendiente:
        mov eax, [ini_pend]
        test eax, eax
        jz .r
        mov dword [ini_pend], 0
        mov ecx, [ini_dato]
        cmp dword [mi_ven], 0
        je .r
        cmp eax, INI_P_CERRAR
        je ini_cerrar
        cmp eax, INI_P_PROG
        je .prog
        cmp ecx, [mi_n]
        jae .r
        cmp byte [mi_tipo+ecx], 2
        jne .r
        mov ecx, [mi_dato+ecx*4]
        call ini_cerrar
        cmp ecx, 1
        jb .config
        je arch_abrir
        cmp ecx, 3
        jb tar_abrir
        je bt_centro_abrir
        jmp apa_pedir
.config:
        xor edx, edx
        jmp ctx_config
.prog:
        cmp dword [ms_ven], 0
        je .r
        cmp ecx, [ms_n]
        jae .r
        push ebx
        push esi
        call sub_texto              ; EBX=el acceso (0: «(vacío)» o «Más...»)
        mov eax, ebx
        pop esi
        pop ebx
        test eax, eax
        jnz .lanza
        cmp byte [ms_mas], 0
        je .r                       ; «(vacío)»: no hace nada (el menú sigue)
        call ini_cerrar
        ; «Más...»: la ventana del grupo, con todos (la abre el Centro, que es
        ; el dueño de las ventanas de los grupos: «Abrir» sobre su icono)
        pushad
        mov ebp, [cen_ven]
        test ebp, ebp
        jz .m9
        xor ecx, ecx                ; qué icono hace el grupo en el Centro
        mov ebx, [gr_tabla]
.m1:
        cmp ebx, [ms_gr]
        jae .m2
        cmp byte [ebx+GR_USO], 0
        je .m1s
        inc ecx
.m1s:
        add ebx, GR_TAM
        jmp .m1
.m2:
        mov [ebp+V_D0], ecx
        mov eax, WM_COMMAND
        mov ecx, 301
        xor edx, edx
        call msg_poner
.m9:
        popad
.r:
        ret
.lanza:
        call ini_cerrar
        mov [bt_lz_acc], eax
        mov eax, bt_lz_tarea        ; en una tarea suya: abrir un programa lee
        call tar_crear              ; del disco y puede sacar un aviso
        ret

; la tarea que abre el acceso directo elegido en el menú Inicio (como un
; doble clic en su icono; los avisos tienen de dueña al Centro, como siempre)
bt_lz_tarea:
        mov eax, [bt_lz_acc]
        mov ebp, [cen_ven]
        call acc_lanzar
        ret
