; =============================================================================
;  TECLADO.HVP - Teclado, la herramienta de taller del teclado de Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Una ventana con un TECLADO DIBUJADO (distribución española). Al pulsar una
;  tecla física, se ENCIENDE la tecla dibujada que le corresponde (cambia de
;  color) y se APAGA al soltarla. Además, SIEMPRE enseña, en un rótulo bien
;  visible, el código de rastreo de la ÚLTIMA tecla (en hexadecimal, con make/
;  break y el prefijo E0 si lo lleva) — como TECLA.COM, pero gráfico. Así, una
;  tecla rara (p. ej. un Ctrl izquierdo con un código no estándar) quizá no
;  encienda ninguna tecla dibujada, pero su CÓDIGO sí se ve, y con eso se puede
;  remapear.
;
;  REMAPEO: abajo hay una zona donde el usuario (a) ve el ORIGEN (el código de
;  la última tecla pulsada), (b) elige el DESTINO (Ctrl, Alt, Mayús, Esc, Tab,
;  Intro, o la próxima tecla que pulse) y (c) «Guardar»: escribe/actualiza
;  \HISPADOS\TECLAS.INI con la línea «origen=destino» en hexadecimal (el formato
;  de docs/TECLADO.md). Si ya había una línea para ese origen, la reemplaza; con
;  «Quitar» la borra. Tras guardar avisa de que hay que REINICIAR (KEYB lee
;  TECLAS.INI al arrancar).
;
;  CÓMO VE EL CÓDIGO CRUDO: Hispa Ventanas reenvía a esta ventana (suscrita con
;  tec_suscribir en WM_CREATE) cada código de rastreo tal como llega del
;  hardware, como el mensaje WM_TECLA_CRUDA (ECX = make | break 0x80 | E0 0x100;
;  ver defs.inc). El gancho de INT 15h/4Fh lo ve ANTES del remapeo de KEYB, así
;  que aquí aparece el código de hardware de verdad, sin cocer.
;
;  Es un módulo .HVP (ventanas/hvp.inc), accesorio del grupo «Accesorios».
; =============================================================================
%include "hvp.inc"

; ---- medidas ----
TKH         equ 22                  ; alto de una tecla dibujada
BTN_H       equ 22                  ; alto de un botón del panel
; marcas de fin de tabla y campos de un registro (teclas y botones, 12 octetos)
K_X         equ 0                   ; dw x
K_Y         equ 2                   ; dw y
K_W         equ 4                   ; dw ancho
K_SCAN      equ 6                   ; db código de rastreo (o acción, en botones)
K_E0        equ 7                   ; db 1 si es tecla extendida (E0)
K_LBL       equ 8                   ; dd rótulo (acabado en 0)
K_TAM       equ 12
%macro KT 6                         ; x, y, ancho, scancode, e0, rótulo
        dw %1, %2, %3
        db %4, %5
        dd %6
%endmacro

; ---- acciones de los botones del panel ----
BA_CTRL     equ 1
BA_ALT      equ 2
BA_MAYUS    equ 3
BA_ESC      equ 4
BA_TAB      equ 5
BA_INTRO    equ 6
BA_TECLA    equ 7                   ; el destino será la próxima tecla pulsada
BA_GUARDAR  equ 8
BA_QUITAR   equ 9

; ---- órdenes del menú ----
O_SALIR     equ 101
O_ACERCA    equ 121

HVP_CABECERA "Teclado", entrada, 1
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "...ffffffffffffffffffffffffff..."
        db "...f888888888888888888888888f..."
        db "...f808808808808808808808808f..."
        db "...f888888888888888888888888f..."
        db "...f888888888888888888888888f..."
        db "...f808808808808808808808808f..."
        db "...f888888888888888888888888f..."
        db "...f888888888888888888888888f..."
        db "...f808808808808808808808808f..."
        db "...f888888888888888888888888f..."
        db "...f888888888888888888888888f..."
        db "...f808808808808808808808808f..."
        db "...f888888888888888888888888f..."
        db "...f888888888888888888888888f..."
        db "...f888888000000000000888888f..."
        db "...f888888888888888888888888f..."
        db "...f888888888888888888888888f..."
        db "...ffffffffffffffffffffffffff..."
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

; ---- clase de la ventana y plantilla ----
c_teclado   dd tec_proc
            db C_ARENA, CB_FOCO, 0, 0
tec_plant:
        PLANTILLA c_teclado, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 40, 36, 640, 312, 0

; ---- la barra de menús ----
tec_menu:
        db "&Archivo", 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw O_ACERCA
        db "&Acerca de Teclado...", 0
        dw 0xFFFF
        db 0

; =============================================================================
;  El teclado dibujado (distribución española). Cada KT: x, y, ancho, make, E0.
; =============================================================================
teclas:
        ; fila de función
        KT   8, 34, 30, 0x01, 0, l_esc
        KT  72, 34, 30, 0x3B, 0, l_f1
        KT 104, 34, 30, 0x3C, 0, l_f2
        KT 136, 34, 30, 0x3D, 0, l_f3
        KT 168, 34, 30, 0x3E, 0, l_f4
        KT 208, 34, 30, 0x3F, 0, l_f5
        KT 240, 34, 30, 0x40, 0, l_f6
        KT 272, 34, 30, 0x41, 0, l_f7
        KT 304, 34, 30, 0x42, 0, l_f8
        KT 344, 34, 30, 0x43, 0, l_f9
        KT 376, 34, 30, 0x44, 0, l_f10
        KT 408, 34, 30, 0x57, 0, l_f11
        KT 440, 34, 30, 0x58, 0, l_f12
        ; fila de números
        KT   8, 60, 30, 0x29, 0, l_grado
        KT  40, 60, 30, 0x02, 0, l_1
        KT  72, 60, 30, 0x03, 0, l_2
        KT 104, 60, 30, 0x04, 0, l_3
        KT 136, 60, 30, 0x05, 0, l_4
        KT 168, 60, 30, 0x06, 0, l_5
        KT 200, 60, 30, 0x07, 0, l_6
        KT 232, 60, 30, 0x08, 0, l_7
        KT 264, 60, 30, 0x09, 0, l_8
        KT 296, 60, 30, 0x0A, 0, l_9
        KT 328, 60, 30, 0x0B, 0, l_0
        KT 360, 60, 30, 0x0C, 0, l_apos
        KT 392, 60, 30, 0x0D, 0, l_iexcl
        KT 424, 60, 62, 0x0E, 0, l_retro
        ; bloque de edición (teclas extendidas E0)
        KT 508, 60, 36, 0x47, 1, l_inicio
        KT 548, 60, 36, 0x4F, 1, l_fin
        KT 588, 60, 36, 0x49, 1, l_repag
        KT 508, 86, 36, 0x53, 1, l_supr
        KT 548, 86, 36, 0x52, 1, l_ins
        KT 588, 86, 36, 0x51, 1, l_avpag
        ; fila QWERTY
        KT   8, 86, 46, 0x0F, 0, l_tab
        KT  56, 86, 30, 0x10, 0, l_q
        KT  88, 86, 30, 0x11, 0, l_w
        KT 120, 86, 30, 0x12, 0, l_e
        KT 152, 86, 30, 0x13, 0, l_r
        KT 184, 86, 30, 0x14, 0, l_t
        KT 216, 86, 30, 0x15, 0, l_y
        KT 248, 86, 30, 0x16, 0, l_u
        KT 280, 86, 30, 0x17, 0, l_i
        KT 312, 86, 30, 0x18, 0, l_o
        KT 344, 86, 30, 0x19, 0, l_p
        KT 376, 86, 30, 0x1A, 0, l_tilde
        KT 408, 86, 30, 0x1B, 0, l_mas
        ; fila de casa
        KT   8, 112, 54, 0x3A, 0, l_bloq
        KT  64, 112, 30, 0x1E, 0, l_a
        KT  96, 112, 30, 0x1F, 0, l_s
        KT 128, 112, 30, 0x20, 0, l_d
        KT 160, 112, 30, 0x21, 0, l_f
        KT 192, 112, 30, 0x22, 0, l_g
        KT 224, 112, 30, 0x23, 0, l_h
        KT 256, 112, 30, 0x24, 0, l_j
        KT 288, 112, 30, 0x25, 0, l_k
        KT 320, 112, 30, 0x26, 0, l_l
        KT 352, 112, 30, 0x27, 0, l_enye
        KT 384, 112, 30, 0x28, 0, l_acento
        KT 416, 112, 30, 0x2B, 0, l_cced
        KT 448, 112, 40, 0x1C, 0, l_intro
        ; fila de mayúsculas
        KT   8, 138, 40, 0x2A, 0, l_mayus
        KT  50, 138, 30, 0x56, 0, l_menor
        KT  82, 138, 30, 0x2C, 0, l_z
        KT 114, 138, 30, 0x2D, 0, l_x
        KT 146, 138, 30, 0x2E, 0, l_c
        KT 178, 138, 30, 0x2F, 0, l_v
        KT 210, 138, 30, 0x30, 0, l_b
        KT 242, 138, 30, 0x31, 0, l_n
        KT 274, 138, 30, 0x32, 0, l_m
        KT 306, 138, 30, 0x33, 0, l_coma
        KT 338, 138, 30, 0x34, 0, l_punto
        KT 370, 138, 30, 0x35, 0, l_guion
        KT 402, 138, 86, 0x36, 0, l_mayus
        KT 548, 138, 36, 0x48, 1, l_arr_up
        ; fila inferior
        KT   8, 164, 44, 0x1D, 0, l_ctrl
        KT  54, 164, 38, 0x5B, 1, l_win
        KT  94, 164, 38, 0x38, 0, l_alt
        KT 134, 164, 170, 0x39, 0, l_espacio
        KT 306, 164, 44, 0x38, 1, l_altgr
        KT 352, 164, 38, 0x5C, 1, l_win
        KT 392, 164, 38, 0x5D, 1, l_menu
        KT 432, 164, 44, 0x1D, 1, l_ctrl
        KT 508, 164, 36, 0x4B, 1, l_arr_lf
        KT 548, 164, 36, 0x50, 1, l_arr_dn
        KT 588, 164, 36, 0x4D, 1, l_arr_rt
        dw 0xFFFF                    ; fin

; =============================================================================
;  Los botones del panel de remapeo (misma estructura; K_SCAN = acción)
; =============================================================================
botones:
        KT   8, 216, 64, BA_CTRL,   0, lb_ctrl
        KT  78, 216, 56, BA_ALT,    0, lb_alt
        KT 140, 216, 74, BA_MAYUS,  0, lb_mayus
        KT 220, 216, 56, BA_ESC,    0, lb_esc
        KT 282, 216, 56, BA_TAB,    0, lb_tab
        KT 344, 216, 64, BA_INTRO,  0, lb_intro
        KT 414, 216, 136, BA_TECLA, 0, lb_tecla
        KT   8, 246, 180, BA_GUARDAR, 0, lb_guardar
        KT 196, 246, 160, BA_QUITAR,  0, lb_quitar
        dw 0xFFFF

; ---- nombres de los códigos de destino conocidos (código, nombre, 0; ... 0) ----
dst_nombres:
        db 0x1D
        db "Ctrl", 0
        db 0x38
        db "Alt", 0
        db 0x2A
        db "May", 0xA3, "s", 0       ; Mayús (ú = 0xA3 en CP437)
        db 0x36
        db "May", 0xA3, "s", 0
        db 0x01
        db "Esc", 0
        db 0x0F
        db "Tab", 0
        db 0x1C
        db "Intro", 0
        db 0x3A
        db "Bloq", 0
        db 0x39
        db "Espacio", 0
        db 0x0E
        db "Retro", 0
        db 0

; ---- rótulos de las teclas (todo en español; las flechas, con los glifos de
;      la fuente 0x18..0x1B, que no existen en el texto fuente) ----
l_esc   db "Esc", 0
l_f1    db "F1", 0
l_f2    db "F2", 0
l_f3    db "F3", 0
l_f4    db "F4", 0
l_f5    db "F5", 0
l_f6    db "F6", 0
l_f7    db "F7", 0
l_f8    db "F8", 0
l_f9    db "F9", 0
l_f10   db "F10", 0
l_f11   db "F11", 0
l_f12   db "F12", 0
l_grado db 0xA7, 0                   ; º (ordinal masculino)
l_1     db "1", 0
l_2     db "2", 0
l_3     db "3", 0
l_4     db "4", 0
l_5     db "5", 0
l_6     db "6", 0
l_7     db "7", 0
l_8     db "8", 0
l_9     db "9", 0
l_0     db "0", 0
l_apos  db "'", 0
l_iexcl db 0xAD, 0                   ; ¡
l_retro db "Retro", 0
l_tab   db "Tab", 0
l_q     db "Q", 0
l_w     db "W", 0
l_e     db "E", 0
l_r     db "R", 0
l_t     db "T", 0
l_y     db "Y", 0
l_u     db "U", 0
l_i     db "I", 0
l_o     db "O", 0
l_p     db "P", 0
l_tilde db "`", 0
l_mas   db "+", 0
l_bloq  db "Bloq", 0
l_a     db "A", 0
l_s     db "S", 0
l_d     db "D", 0
l_f     db "F", 0
l_g     db "G", 0
l_h     db "H", 0
l_j     db "J", 0
l_k     db "K", 0
l_l     db "L", 0
l_enye  db 0xA5, 0                   ; Ñ
l_acento db "Ac", 0                  ; tecla muerta del acento/diéresis
l_cced  db 0x80, 0                   ; Ç
l_intro db "Intro", 0
l_mayus db "May", 0xA3, "s", 0       ; Mayús
l_menor db "<", 0
l_z     db "Z", 0
l_x     db "X", 0
l_c     db "C", 0
l_v     db "V", 0
l_b     db "B", 0
l_n     db "N", 0
l_m     db "M", 0
l_coma  db ",", 0
l_punto db ".", 0
l_guion db "-", 0
l_ctrl  db "Ctrl", 0
l_win   db "Win", 0
l_alt   db "Alt", 0
l_espacio db "Espacio", 0
l_altgr db "AltGr", 0
l_menu  db "Men", 0xA3, 0           ; Menú
l_inicio db "Inic", 0
l_fin   db "Fin", 0
l_repag db "RePg", 0
l_supr  db "Supr", 0
l_ins   db "Ins", 0
l_avpag db "AvPg", 0
l_arr_up db 0x18, 0                  ; flecha arriba
l_arr_dn db 0x19, 0                  ; flecha abajo
l_arr_rt db 0x1A, 0                  ; flecha derecha
l_arr_lf db 0x1B, 0                  ; flecha izquierda

; ---- rótulos de los botones (el 0x1A es la flecha «→») ----
lb_ctrl    db 0x1A, " Ctrl", 0
lb_alt     db 0x1A, " Alt", 0
lb_mayus   db 0x1A, " May", 0xA3, "s", 0
lb_esc     db 0x1A, " Esc", 0
lb_tab     db 0x1A, " Tab", 0
lb_intro   db 0x1A, " Intro", 0
lb_tecla   db "Destino: pr", 0xA2, "xima tecla", 0   ; próxima
lb_guardar db "Guardar en TECLAS.INI", 0
lb_quitar  db "Quitar este remapeo", 0

; ---- textos ----
t_titulo    db "Teclado", 0
t_ultima    db "Última tecla: ", 0
t_ninguna   db "(ninguna)", 0
t_e0        db "E0 ", 0
t_soltar    db " (soltar)", 0
t_pulsar    db " (pulsar)", 0
t_origen    db "Origen del remapeo: ", 0
t_destino   db "Destino: ", 0
t_guion2    db "---", 0
t_pulse_dst db "Pulse la tecla destino...", 0
t_reinicia  db "Remapeo guardado en \HISPADOS\TECLAS.INI.", 10
            db "Reinicie para que el cambio surta efecto.", 0
t_quitado   db "Remapeo quitado de \HISPADOS\TECLAS.INI.", 10
            db "Reinicie para que el cambio surta efecto.", 0
t_faltan    db "Primero pulse la tecla de ORIGEN y elija un DESTINO.", 0
t_falta_org db "Primero pulse la tecla de ORIGEN que quiere quitar.", 0
t_noguardar db "No se ha podido escribir \HISPADOS\TECLAS.INI.", 0
t_acerca    db "Teclado", 10
            db "Herramienta de taller del teclado.", 10
            db "Al pulsar una tecla, enciende la tecla dibujada y enseña su", 10
            db "código (make/break y E0). Permite asignar un remapeo que se", 10
            db "guarda en \HISPADOS\TECLAS.INI y que KEYB aplica al arrancar.", 10
            db "TECLA.COM da lo mismo en modo texto.", 0
ruta_ini    db "\HISPADOS\TECLAS.INI", 0

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        mov esi, tec_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
tec_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_TECLA_CRUDA
        je en_crudo
        cmp eax, WM_LBUTTONDOWN
        je en_clic
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], tec_menu
        mov dword [ebp+V_ICONO], hvp_icono
        HV tec_suscribir            ; EBP=la ventana: recibe el código crudo
        jmp hecho
.cerrar:
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        xor ebp, ebp
        HV tec_suscribir            ; nadie suscrito: el gancho deja de encolar
        mov dword [ven], 0
        HV msg_salir
        jmp hecho

hecho:
        xor eax, eax
        ret

; ---- las órdenes del menú ----
orden:
        cmp ecx, O_SALIR
        je .salir
        cmp ecx, O_ACERCA
        jne hecho
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        jmp hecho
.salir:
        mov ebp, [ven]
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp hecho

; =============================================================================
;  Pintar: el teclado dibujado y el panel de remapeo
; =============================================================================
pintar:
        mov ebp, [ven]
        HV dc_pintar                ; borra el fondo (C_ARENA) de lo sucio

        ; --- el rótulo de la última tecla (siempre visible, arriba) ---
        mov edi, txt
        mov esi, t_ultima
        call pon_cad
        cmp byte [ult_valido], 0
        jne .uv
        mov esi, t_ninguna
        call pon_cad
        jmp .uf
.uv:
        cmp byte [ult_e0], 0
        je .ue
        mov esi, t_e0
        call pon_cad
.ue:
        mov al, [ult_make]
        call pon_hex2
        cmp byte [ult_break], 0
        je .umk
        mov esi, t_soltar
        jmp .uput
.umk:
        mov esi, t_pulsar
.uput:
        call pon_cad
.uf:
        mov byte [edi], 0
        COLOR C_TINTA
        NEGRITA 1
        mov eax, 8
        mov ebx, 8
        mov esi, txt
        HV dc_texto
        NEGRITA 0

        ; --- el teclado: cada tecla, encendida o no ---
        mov esi, teclas
        xor edi, edi                ; índice (para estado_teclas)
.kl:
        cmp word [esi+K_X], 0xFFFF
        je .kfin
        push esi
        push edi
        ; color de fondo según el estado
        movzx eax, byte [estado_teclas+edi]
        test eax, eax
        jz .apagada
        COLOR C_TURQUESA            ; encendida
        jmp .fondo
.apagada:
        COLOR C_CREMA
.fondo:
        movzx eax, word [esi+K_X]
        movzx ebx, word [esi+K_Y]
        movzx ecx, word [esi+K_W]
        mov edx, TKH
        HV dc_rect
        COLOR C_TINTA               ; el marco
        movzx eax, word [esi+K_X]
        movzx ebx, word [esi+K_Y]
        movzx ecx, word [esi+K_W]
        mov edx, TKH
        HV dc_marco
        movzx eax, word [esi+K_X]   ; el rótulo
        add eax, 4
        movzx ebx, word [esi+K_Y]
        add ebx, 3
        mov esi, [esi+K_LBL]
        HV dc_texto
        pop edi
        pop esi
        add esi, K_TAM
        inc edi
        jmp .kl
.kfin:
        call panel
        jmp hecho

; ---- el panel de remapeo (origen, destino y botones) ----
panel:
        ; origen
        mov edi, txt
        mov esi, t_origen
        call pon_cad
        cmp byte [org_valido], 0
        jne .ov
        mov esi, t_guion2
        call pon_cad
        jmp .of
.ov:
        cmp byte [org_e0], 0
        je .oe
        mov esi, t_e0
        call pon_cad
.oe:
        mov al, [org_make]
        call pon_hex2
.of:
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 8
        mov ebx, 194
        mov esi, txt
        HV dc_texto
        ; destino
        mov edi, txt
        mov esi, t_destino
        call pon_cad
        cmp byte [dst_valido], 0
        jne .dv
        mov esi, t_guion2
        call pon_cad
        jmp .df
.dv:
        mov al, [dst_make]
        call pon_hex2
        mov byte [edi], ' '
        inc edi
        call pon_nombre_dest
.df:
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 300
        mov ebx, 194
        mov esi, txt
        HV dc_texto
        ; aviso de «pulse destino»
        cmp byte [cap_dst], 0
        je .bot
        COLOR C_GRANATE
        mov eax, 470
        mov ebx, 194
        mov esi, t_pulse_dst
        HV dc_texto
.bot:
        ; los botones
        mov esi, botones
.bl:
        cmp word [esi+K_X], 0xFFFF
        je .x
        ; ¿resaltado? (destino elegido, o captura de tecla activa)
        movzx eax, byte [esi+K_SCAN]
        call boton_activo           ; AL!=0 -> resaltado
        test al, al
        jz .normal
        COLOR C_OCRE
        jmp .relleno
.normal:
        COLOR C_PIEDRA
.relleno:
        movzx eax, word [esi+K_X]
        movzx ebx, word [esi+K_Y]
        movzx ecx, word [esi+K_W]
        mov edx, BTN_H
        HV dc_rect
        COLOR C_TINTA
        movzx eax, word [esi+K_X]
        movzx ebx, word [esi+K_Y]
        movzx ecx, word [esi+K_W]
        mov edx, BTN_H
        HV dc_marco
        movzx eax, word [esi+K_X]
        add eax, 6
        movzx ebx, word [esi+K_Y]
        add ebx, 3
        push esi                    ; dc_* conservan ESI, pero aquí se machaca
        mov esi, [esi+K_LBL]
        HV dc_texto
        pop esi
        add esi, K_TAM
        jmp .bl
.x:
        ret

; ---- ¿el botón de acción AL debe salir resaltado? -> AL=1 sí, 0 no ----
boton_activo:
        cmp al, BA_TECLA
        jne .nt
        mov al, [cap_dst]
        ret
.nt:
        push ebx
        call codigo_de_accion       ; AL -> AL = código de destino (0 si ninguno)
        test al, al
        jz .no
        cmp byte [dst_valido], 0
        je .no
        cmp al, [dst_make]
        jne .no
        mov al, 1
        pop ebx
        ret
.no:
        xor al, al
        pop ebx
        ret

; ---- código de destino de una acción de botón (0 si no fija un destino) ----
codigo_de_accion:
        cmp al, BA_CTRL
        jne .n1
        mov al, 0x1D
        ret
.n1:
        cmp al, BA_ALT
        jne .n2
        mov al, 0x38
        ret
.n2:
        cmp al, BA_MAYUS
        jne .n3
        mov al, 0x2A
        ret
.n3:
        cmp al, BA_ESC
        jne .n4
        mov al, 0x01
        ret
.n4:
        cmp al, BA_TAB
        jne .n5
        mov al, 0x0F
        ret
.n5:
        cmp al, BA_INTRO
        jne .n6
        mov al, 0x1C
        ret
.n6:
        xor al, al
        ret

; =============================================================================
;  WM_TECLA_CRUDA: ECX = make | break 0x80 | E0 0x100
; =============================================================================
en_crudo:
        mov edx, ecx
        and edx, 0x7F               ; DL = make puro
        mov [ult_make], dl
        test cl, 0x80
        setnz [ult_break]
        mov eax, ecx
        shr eax, 8
        and eax, 1                  ; AL = E0
        mov [ult_e0], al
        mov byte [ult_valido], 1
        mov bl, al                  ; BL = E0 (se conserva)
        ; encender/apagar la tecla dibujada que corresponde
        push ecx
        mov esi, teclas
        xor ecx, ecx                ; índice
.fl:
        cmp word [esi+K_X], 0xFFFF
        je .nf
        mov al, [esi+K_SCAN]
        cmp al, dl
        jne .fn
        mov al, [esi+K_E0]
        cmp al, bl
        jne .fn
        mov al, [ult_break]         ; soltar -> 0, pulsar -> 1
        xor al, 1
        mov [estado_teclas+ecx], al
        ; (no se corta: una misma tecla sale una vez en la tabla)
.fn:
        add esi, K_TAM
        inc ecx
        jmp .fl
.nf:
        pop ecx
        ; fijar origen o destino (solo al PULSAR)
        test byte [ult_break], 1
        jnz .pinta
        cmp byte [cap_dst], 0
        je .org
        mov [dst_make], dl          ; el destino es un make de 2 cifras (sin E0)
        mov byte [dst_valido], 1
        mov byte [cap_dst], 0
        jmp .pinta
.org:
        mov [org_make], dl
        mov [org_e0], bl
        mov byte [org_valido], 1
.pinta:
        mov ebp, [ven]
        HV ven_invalidar
        jmp hecho

; =============================================================================
;  WM_LBUTTONDOWN: EDX = x (palabra baja) + y (palabra alta), zona cliente
; =============================================================================
en_clic:
        mov eax, edx
        and eax, 0xFFFF
        mov [clic_x], eax
        mov eax, edx
        shr eax, 16
        and eax, 0xFFFF
        mov [clic_y], eax
        mov esi, botones
.bl:
        cmp word [esi+K_X], 0xFFFF
        je .x
        movzx eax, word [esi+K_X]
        mov ecx, [clic_x]
        cmp ecx, eax
        jl .bn
        movzx edx, word [esi+K_W]
        add eax, edx
        cmp ecx, eax
        jge .bn
        movzx eax, word [esi+K_Y]
        mov ecx, [clic_y]
        cmp ecx, eax
        jl .bn
        add eax, BTN_H
        cmp ecx, eax
        jge .bn
        movzx eax, byte [esi+K_SCAN]
        call hacer_accion
        jmp .x
.bn:
        add esi, K_TAM
        jmp .bl
.x:
        jmp hecho

; ---- ejecutar la acción AL de un botón ----
hacer_accion:
        push eax                    ; la acción original
        call codigo_de_accion       ; AL -> código de destino (0 si no lo fija)
        test al, al
        jz .nofijo
        mov [dst_make], al
        mov byte [dst_valido], 1
        mov byte [cap_dst], 0
        add esp, 4                  ; descartar la acción guardada
        jmp .refresca
.nofijo:
        pop eax
        cmp al, BA_TECLA
        jne .n1
        mov byte [cap_dst], 1       ; el destino será la próxima tecla pulsada
        jmp .refresca
.n1:
        cmp al, BA_GUARDAR
        jne .n2
        call accion_guardar
        jmp .refresca
.n2:
        cmp al, BA_QUITAR
        jne .refresca
        call accion_quitar
.refresca:
        mov ebp, [ven]
        HV ven_invalidar
        ret

; ---- «Guardar»: escribe/actualiza la línea del origen en TECLAS.INI ----
accion_guardar:
        cmp byte [org_valido], 0
        je .faltan
        cmp byte [dst_valido], 0
        je .faltan
        mov byte [modo_guardar], 1
        call reescribe_ini
        jc .error
        mov ebp, [ven]
        mov esi, t_reinicia
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret
.faltan:
        mov ebp, [ven]
        mov esi, t_faltan
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret
.error:
        mov ebp, [ven]
        mov esi, t_noguardar
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret

; ---- «Quitar»: borra la línea del origen en TECLAS.INI ----
accion_quitar:
        cmp byte [org_valido], 0
        je .faltan
        mov byte [modo_guardar], 0
        call reescribe_ini
        jc .error
        mov ebp, [ven]
        mov esi, t_quitado
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret
.faltan:
        mov ebp, [ven]
        mov esi, t_falta_org
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret
.error:
        mov ebp, [ven]
        mov esi, t_noguardar
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret

; ---- reescribe \HISPADOS\TECLAS.INI quitando la línea del origen actual y,
;      si modo_guardar=1, añadiendo «origen=destino». CF=1 si no se puede ----
reescribe_ini:
        call build_org_txt          ; org_txt = «xx» o «E0xx»
        mov esi, ruta_ini           ; leer lo que hay (vacío si no existe)
        mov edi, buf_in
        mov ecx, 4095
        call hvl_leer
        mov esi, buf_in
        mov edi, buf_out
.rl:
        cmp byte [esi], 0
        je .fin
        call parse_origen           ; AL=make AH=e0 CF (ESI intacto)
        jc .copia
        cmp al, [org_make]
        jne .copia
        mov bl, ah
        cmp bl, [org_e0]
        jne .copia
        call salta_linea            ; misma tecla: se quita esa línea
        jmp .rl
.copia:
        call copia_linea
        jmp .rl
.fin:
        cmp byte [modo_guardar], 0
        je .escribe
        cmp edi, buf_out            ; ¿hace falta un salto antes de añadir?
        je .nueva
        cmp byte [edi-1], 10
        je .nueva
        mov byte [edi], 13
        inc edi
        mov byte [edi], 10
        inc edi
.nueva:
        mov esi, org_txt            ; «origen=destino» + CR LF
        call pon_cad
        mov byte [edi], '='
        inc edi
        mov al, [dst_make]
        call pon_hex2
        mov byte [edi], 13
        inc edi
        mov byte [edi], 10
        inc edi
.escribe:
        mov ecx, edi
        sub ecx, buf_out
        mov esi, ruta_ini
        mov edx, buf_out
        call hvl_escribir           ; CF=1 si falla
        ret

; ---- org_txt = el origen en texto («xx» o «E0xx») ----
build_org_txt:
        mov edi, org_txt
        cmp byte [org_e0], 0
        je .n
        mov byte [edi], 'E'
        inc edi
        mov byte [edi], '0'
        inc edi
.n:
        mov al, [org_make]
        call pon_hex2
        mov byte [edi], 0
        ret

; ---- parse_origen: ESI=principio de línea -> AL=make AH=E0, CF=1 si la línea
;      no define un origen (en blanco, comentario o sin cifras). ESI intacto ----
parse_origen:
        push ebx
        push ecx
        push edx
        mov ebx, esi
.sp:
        mov al, [ebx]
        cmp al, ' '
        je .adv
        cmp al, 9
        jne .first
.adv:
        inc ebx
        jmp .sp
.first:
        test al, al
        jz .no
        cmp al, 13
        je .no
        cmp al, 10
        je .no
        cmp al, ';'
        je .no
        cmp al, '#'
        je .no
        xor edx, edx
        xor ecx, ecx
.hx:
        mov al, [ebx]
        cmp al, 'a'
        jb .nl
        cmp al, 'f'
        ja .nl
        sub al, 0x20
.nl:
        cmp al, '0'
        jb .end
        cmp al, '9'
        jbe .dig
        cmp al, 'A'
        jb .end
        cmp al, 'F'
        ja .end
        sub al, 'A' - 10
        jmp .put
.dig:
        sub al, '0'
.put:
        shl edx, 4
        and al, 0x0F
        or dl, al
        inc ebx
        inc ecx
        cmp ecx, 4
        jb .hx
.end:
        test ecx, ecx
        jz .no
        xor ah, ah
        cmp ecx, 4
        jne .noext
        mov al, dh                  ; 4 cifras: el octeto alto ha de ser E0
        cmp al, 0xE0
        jne .no
        mov ah, 1
.noext:
        mov al, dl
        and al, 0x7F
        clc
        jmp .x
.no:
        stc
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; ---- copia la línea en ESI (hasta el salto incluido, o el 0) a EDI ----
copia_linea:
        push eax
.c:
        mov al, [esi]
        test al, al
        jz .x
        inc esi
        mov [edi], al
        inc edi
        cmp al, 10
        jne .c
.x:
        pop eax
        ret

; ---- salta la línea en ESI (hasta el salto incluido, o el 0) ----
salta_linea:
        push eax
.c:
        mov al, [esi]
        test al, al
        jz .x
        inc esi
        cmp al, 10
        jne .c
.x:
        pop eax
        ret

; =============================================================================
;  Helpers de texto
; =============================================================================
; copia ESI (acabada en 0) a EDI sin el 0; EDI avanza (como hvl_cad)
pon_cad:
        push eax
.c:
        lodsb
        test al, al
        jz .x
        stosb
        jmp .c
.x:
        pop eax
        ret

; escribe AL en 2 cifras hexadecimales (mayúsculas) en EDI, que avanza
pon_hex2:
        push eax
        push ebx
        mov bl, al
        shr al, 4
        call .nib
        mov al, bl
        and al, 0x0F
        call .nib
        pop ebx
        pop eax
        ret
.nib:
        and al, 0x0F
        cmp al, 10
        jb .d
        add al, 'A' - 10
        jmp .s
.d:
        add al, '0'
.s:
        mov [edi], al
        inc edi
        ret

; añade en EDI « (Nombre)» del código de destino AL, si se conoce
pon_nombre_dest:
        push esi
        push eax
        mov esi, dst_nombres
.l:
        mov al, [esi]
        test al, al
        jz .x
        cmp al, [dst_make]
        je .hit
        inc esi
.sk:
        lodsb
        test al, al
        jnz .sk
        jmp .l
.hit:
        inc esi
        mov byte [edi], '('
        inc edi
        call pon_cad
        mov byte [edi], ')'
        inc edi
.x:
        pop eax
        pop esi
        ret

%include "hvplib.inc"

        section .bss
        alignb 4
ven         resd 1
clic_x      resd 1
clic_y      resd 1
ult_valido  resb 1
ult_make    resb 1
ult_e0      resb 1
ult_break   resb 1
org_valido  resb 1
org_make    resb 1
org_e0      resb 1
dst_valido  resb 1
dst_make    resb 1
cap_dst     resb 1
modo_guardar resb 1
            alignb 4
estado_teclas resb 128
org_txt     resb 8
txt         resb 192
buf_in      resb 4096
buf_out     resb 4352

HVP_FIN
