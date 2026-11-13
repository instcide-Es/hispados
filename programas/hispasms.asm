; =============================================================================
;  HISPASMS.HVP - Hispa-System, la segunda consola de 8 bits de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un emulador compatible con los cartuchos de la Master System (volcados en
;  archivos .SMS). Todo el código es propio, escrito a partir de la
;  documentación pública del hardware (el Z80, el VDP 315-5124/5246 y el
;  SN76489): no lleva nada del fabricante de la consola ni de otros
;  emuladores, y las ROM comerciales no van en el proyecto (quien tenga sus
;  cartuchos usa sus volcados). Con él va un juego propio, Buzo (BUZO.SMS),
;  que se abre solo si está al lado.
;
;  Las piezas (programas/hispasms/):
;    z80.inc    el procesador (Z80, con lo no documentado), con sus ciclos
;    vdp.inc    la imagen, línea a línea, los puertos y los mandos
;    psg.inc    el sonido: tres tonos y un ruido
;    mapas.inc  los cartuchos (mapeador de Sega y de Codemasters) y su RAM
;    datos.inc  las variables de la consola
;  y aquí, lo que la une a Hispa Ventanas: la ventana y el menú, el reloj
;  (59,92 cuadros por segundo, saltándose el dibujo de alguno si el equipo
;  no llega), el teclado y los mandos de HSPJOY, el sonido por HSPSND (en
;  trozos de la medida de la tarjeta, sin esperar nunca) y los archivos (la
;  ROM y la partida guardada, .SAV, de los cartuchos con pila).
;
;  Teclas: flechas = cruceta, Z = botón 1, X = botón 2, Intro = el botón de
;  pausa de la consola; P para el emulador y F2 reinicia. Mandos (si está
;  HSPJOY): el del jugador 1 y el del jugador 2.
;
;  Parámetros del acceso directo: el nombre de una ROM, que se abre al
;  entrar, y:
;    /M=n   el mapeador: 1 el de Sega, 2 el de Codemasters (si no, se mira)
;    /1     tamaño x1 aunque la pantalla sea grande
;  y para las pruebas:
;    /P     paso a paso: la consola solo avanza con cada tecla (seis cuadros
;           con el botón pulsado y seis sin él; Espacio, treinta sin botones)
;    /T=n   corre n cuadros a toda velocidad y apunta el resultado (tiempo,
;           suma de la imagen y de la memoria) en HISPASMS.LOG; con /V,
;           pintando cada cuadro en la ventana
; =============================================================================
%include "hvp.inc"

EST_AL      equ 18                  ; alto de la barra de estado
TICS_SEG    equ 1193182             ; tics del reloj (el contador 8254) por segundo
TICS_CUADRO equ 19913               ; y por cuadro (59,92 por segundo)
ROM_MAX     equ 4 * 1024 * 1024 + 512
ROM_MENOS   equ 8192                ; una ROM más pequeña no es de la consola
SRAM_TAM    equ 0x8000              ; la RAM del cartucho: dos bancos de 16 KB
TROZO_MAX   equ 8192                ; octetos de un trozo de sonido, como mucho
; botones del mando, como los lee la consola (y el de pausa, aparte)
B_ARRIBA    equ 0x01
B_ABAJO     equ 0x02
B_IZQ       equ 0x04
B_DER       equ 0x08
B_1         equ 0x10
B_2         equ 0x20
B_PAUSA     equ 0x80
; HSPJOY (docs/MANDOS.md)
HJD_TAM     equ 0x04
HJD_TABLA   equ 0x06
HJD_J1E     equ 0x0D
HJD_J2E     equ 0x0E
HJD_OPC     equ 0x12
HJS_FLAGS   equ 1
HJS_NBTN    equ 3
HJS_EJES    equ 8
HJS_CRUZ    equ 14
HJS_BTN     equ 16

HVP_CABECERA "Hispa-System", entrada
        db "................................"
        db "................................"
        db "................................"
        db "...............00..............."
        db "...............00..............."
        db "...............00..............."
        db "...............00..............."
        db "...............00..............."
        db "...............00..............."
        db "...00000000000000000000000000..."
        db "..0077777777777777777777777700.."
        db "..07ffffffffffffffffffffffff70.."
        db "..0777777777777777777777777770.."
        db "..0777700077777777777777777770.."
        db "..0777708077777777777777ccc770.."
        db "..0770008000777777777777ccc770.."
        db "..0770888880777777744477ccc770.."
        db "..0770008000777777744477777770.."
        db "..0777708077777777744477777770.."
        db "..0777700077777777777777777770.."
        db "..0777777777777777777777777770.."
        db "..0777777777777777777777777770.."
        db "..0077777777777777777777777700.."
        db "...00000000000000000000000000..."
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

c_sms       dd sms_proc
            db C_TINTA, CB_FOCO, 0, 0
plant:
        PLANTILLA c_sms, titulo, E_NORMAL, 140, 30, 256 + 2*BORDE, \
                  192 + EST_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

menu:
        db "&Archivo", 0
        dw 101
        db "&Abrir ROM...", 0
        dw 102
        db "&Buzo (el juego de muestra)", 0
        dw 0
        db 0
        dw 103
        db "&Reiniciar (F2)", 0
        dw 104
        db "&Pausa (P)", 0
        dw 0
        db 0
        dw 109
        db "&Salir", 0
        dw 0xFFFF
        db "&Tamaño", 0
        dw 111
        db "Tamaño x&1", 0
        dw 112
        db "Tamaño x&2", 0
        dw 0xFFFF
        db "&Sonido", 0
        dw 121
        db "Poner o quitar el &sonido", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw 131
        db "&Teclas y mandos...", 0
        dw 132
        db "&Acerca de Hispa-System...", 0
        dw 0xFFFF
        db 0

titulo      db "Hispa-System", 0
t_abrir     db "Abrir ROM (.SMS)", 0
t_buzo      db "BUZO.SMS", 0
t_sin       db "Sin cartucho. Archivo, Abrir ROM...", 0
t_pausa     db "Pausa - ", 0
t_cs        db " c/s", 0
t_sinson    db "  sin sonido", 0
t_noabre    db "No se ha podido abrir el archivo.", 0
t_mala      db "Este archivo no es una ROM de la consola (.SMS).", 0
t_nomem     db "No hay memoria para un cartucho tan grande.", 0
t_nosnd     db "No hay sonido: falta HSPSND o no ha encontrado tarjeta.", 0
t_tec_tit   db "Teclas y mandos", 0
t_tec       db "Flechas: la cruceta.    Z: botón 1.    X: botón 2.", 10
            db "Intro: el botón de pausa de la consola.", 10
            db "P: parar el emulador.    F2: reiniciar la consola.", 10
            db 10
            db "Con HSPJOY cargado valen los mandos del jugador 1 y del 2:", 10
            db "cruceta o palanca y los botones 1 y 2; la pausa de la", 10
            db "consola es el 3 o el 4, o del 7 al 10 si el mando tiene más.", 0
t_ac_tit    db "Acerca de Hispa-System", 0
t_acerca    db "Hispa-System: emulador propio, compatible con los", 10
            db "cartuchos de la Master System volcados en archivos .SMS.", 10
            db 10
            db "Hecho desde la documentación pública del hardware. Las ROM", 10
            db "comerciales no van con Hispa-DOS: use los volcados de sus", 10
            db "propios cartuchos. Buzo (BUZO.SMS) es un juego nuestro.", 10
            db 10
            db "Mapeadores de Sega y de Codemasters. Partidas con pila: .SAV", 0
k_cuadros   db "Cuadros=", 0
k_ms        db "Milisegundos=", 0
k_cps       db "CuadrosPorSegundo=", 0
k_modo      db "Modo=", 0
k_modo0     db "sin pintar", 0
k_modo1     db "ventana x", 0
k_imagen    db "Imagen=", 0
k_ram       db "RAM=", 0
k_estado    db "Estado=", 0
k_texto     db "Texto=", 0
k_mapa      db "Mapa=", 0

; teclas -> botones: código de la tecla y botón
        align 4
teclas      db 0x2C, B_1,  0x2D, B_2,  0x1C, B_PAUSA
            db 0x48, B_ARRIBA,  0x50, B_ABAJO,  0x4B, B_IZQ,  0x4D, B_DER
N_TECLAS    equ ($ - teclas) / 2

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        push edi
        mov edi, ruta_log           ; HISPASMS.LOG, al lado del módulo
        mov eax, 'LOG'
        call hvl_ruta
        mov edi, ruta_buzo          ; y BUZO.SMS: la carpeta y el nombre
        xor edx, edx
.c:
        lodsb
        stosb
        cmp al, '\'
        jne .c1
        mov edx, edi
.c1:
        test al, al
        jnz .c
        test edx, edx
        jz .c2
        mov edi, edx
        mov esi, t_buzo
        call hvl_cad
        mov byte [edi], 0
.c2:
        pop edi
        ; los parámetros
        mov esi, edi
        mov al, 'P'
        call hvl_opcion
        setnc byte [a_pasos]
        mov esi, edi
        mov al, 'V'
        call hvl_opcion
        setnc byte [a_ventana]
        mov esi, edi
        mov al, '1'
        call hvl_opcion
        setnc byte [a_uno]
        mov esi, edi
        mov al, 'T'
        call hvl_opcion
        jc .st
        call hvl_lee_num
        mov [a_prueba], eax
.st:
        mov esi, edi
        mov al, 'M'
        call hvl_opcion
        jc .sm
        call hvl_lee_num
        cmp eax, 2
        ja .sm
        mov [map_forzar], eax
.sm:
        mov esi, edi                ; el nombre de la ROM: lo primero que no
.n0:                                ; empiece por «/»
        lodsb
        cmp al, ' '
        je .n0
        test al, al
        jz .sn
        cmp al, '/'
        jne .n2
.n1:
        lodsb                       ; (una opción: se salta)
        test al, al
        jz .sn
        cmp al, ' '
        jne .n1
        jmp .n0
.n2:
        mov edi, nom_tmp
        mov ecx, 78
.n3:
        stosb
        lodsb
        cmp al, ' '
        jbe .n4
        dec ecx
        jnz .n3
.n4:
        mov byte [edi], 0
.sn:
        call cero_busca
        call paleta_haz
        call mandos_busca
        mov eax, [hv_api + HV_tec_mapa * 4]
        mov [p_mapa], eax
        mov eax, AJ_HAY_SONIDO
        HV aj_leer
        mov [snd_hay], al
        mov [snd_si], al
        call snd_busca
        mov eax, [a_prueba]
        or al, [a_pasos]
        jz .s1
        mov byte [snd_si], 0        ; en las pruebas, callado
.s1:
        call snd_ajusta
        ; el tamaño: x2 si la pantalla da para ello
        HV pantalla
        mov ecx, 1
        cmp byte [a_uno], 0
        jne .t1
        cmp eax, 800
        jb .t1
        cmp ebx, 600
        jb .t1
        inc ecx
.t1:
        push eax
        mov eax, ecx
        call tam_ventana            ; -> ECX=ancho EDX=alto de la ventana
        pop eax
        mov [plant+P_AN], ecx
        mov [plant+P_AL], edx
        sub eax, ecx
        shr eax, 1
        mov [plant+P_X], eax
        mov esi, plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz near bucle.x
        ; el cartucho del principio: el de los parámetros o Buzo
        mov esi, nom_tmp
        cmp byte [esi], 0
        jne .abre
        mov esi, ruta_buzo
        mov byte [callado], 1       ; (si no está, no pasa nada)
.abre:
        call rom_abrir
        mov byte [callado], 0
        cmp dword [a_prueba], 0
        je bucle
        call prueba

; ---- el bucle: los mensajes de la ventana y, entre medias, la consola ----
bucle:
        mov edi, msg
        HV msg_mirar
        jc .nada
.msg:
        cmp dword [msg+M_MSG], WM_QUIT
        je .x
        HV msg_despachar
        jmp bucle
.nada:
        call corre
        jnz .emula
        mov edi, msg                ; parada: a esperar un mensaje
        HV msg_tomar
        jmp .msg
.emula:
        call reloj
        mov edx, eax
        sub edx, [t_sig]
        js .cede                    ; todavía no toca otro cuadro
        cmp edx, TICS_CUADRO * 5
        jb .a
        mov [t_sig], eax            ; muy atrasado (un diálogo, el disco...):
        xor edx, edx                ; se empieza a contar otra vez
.a:
        add dword [t_sig], TICS_CUADRO
        mov byte [dibujar], 1
        cmp edx, TICS_CUADRO        ; va con retraso: este cuadro no se pinta
        jb .p                       ; (tres seguidos como mucho)
        cmp byte [saltados], 3
        jae .p
        inc byte [saltados]
        mov byte [dibujar], 0
        jmp .q
.p:
        mov byte [saltados], 0
.q:
        call mandos_lee
        call sms_cuadro
        call sonido_manda
        inc dword [n_emu]
        cmp byte [dibujar], 0
        je .r
        inc dword [n_pint]
        call presenta
.r:
        call cada_segundo
.cede:
        HV ceder
        jmp bucle
.x:
        ret

; ZF=0 si la consola tiene que correr
corre:
        cmp byte [cargada], 0
        je .no
        cmp byte [pausa], 0
        jne .no
        cmp byte [a_pasos], 0
        jne .no
        mov eax, [ven]
        test eax, eax
        jz .no
        test dword [eax+V_ESTADO], S_MINIM
        jnz .no
        or eax, 1
        ret
.no:
        xor eax, eax
        ret

; EAX=escala (1 o 2) -> ECX=ancho EDX=alto de la ventana entera
tam_ventana:
        mov ecx, eax
        shl ecx, 8
        add ecx, 2*BORDE
        imul edx, eax, 192
        add edx, EST_AL + 2*BORDE + TIT_H + 1 + MENU_H
        ret

; AL=botones (con B_PAUSA) -> el mando 1 y el botón de pausa de la consola
pon_pad:
        push eax
        mov ah, al
        and al, 0x3F
        mov [pad_estado], al
        shr ah, 7
        mov [pausa_tecla], ah
        pop eax
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
sms_proc:
        cmp eax, WM_PAINT
        je near pintar
        cmp eax, WM_KEYDOWN
        je near .tecla
        cmp eax, WM_CHAR
        je near .letra
        cmp eax, WM_COMMAND
        je near .orden
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_KILLFOCUS
        je .sinfoco
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], menu
        mov dword [ebp+V_ICONO], hvp_icono
        jmp hecho
.destruir:
        call rom_cerrar
        call mandos_suelta
        mov dword [ven], 0
        HV msg_salir
        jmp hecho
.foco:
        mov byte [foco], 1
        HVJ ven_defecto
.sinfoco:
        mov byte [foco], 0
        HVJ ven_defecto
.tam:
        HV ven_invalidar
        mov eax, WM_SIZE
        HVJ ven_defecto
.tecla:
        cmp ecx, 0x3C               ; F2
        je .reinicia
        mov eax, ecx
        mov esi, teclas
        mov ecx, N_TECLAS
.t1:
        cmp al, [esi]
        je .t2
        add esi, 2
        dec ecx
        jnz .t1
        jmp hecho
.t2:
        movzx eax, byte [esi+1]
        jmp .boton
.letra:
        mov eax, ecx
        cmp al, 'a'
        jb .l0
        cmp al, 'z'
        ja .l0
        sub al, 0x20
.l0:
        cmp al, 'P'
        je .pausa
        cmp al, 'Z'
        jne .l1
        mov eax, B_1
        jmp .boton
.l1:
        cmp al, 'X'
        jne .l2
        mov eax, B_2
        jmp .boton
.l2:
        cmp byte [a_pasos], 0
        je hecho
        cmp al, ' '
        jne hecho
        xor eax, eax                ; treinta cuadros sin tocar nada
        mov ecx, 30
        xor edx, edx
        call pasos
        jmp hecho
; una tecla de la consola por mensaje: AL=botón
.boton:
        cmp byte [a_pasos], 0
        je .blando
        mov ecx, 6                  ; paso a paso: seis cuadros con el botón
        mov edx, 6                  ; y seis sin él
        call pasos
        jmp hecho
.blando:
        mov ecx, N_TECLAS - 1       ; (por si el equipo no avisa de las teclas
.b1:                                ; que se sueltan: vale unos cuadros)
        cmp al, [teclas+ecx*2+1]
        jne .b2
        mov byte [blandas+ecx], 8
.b2:
        dec ecx
        jns .b1
        jmp hecho
.orden:
        cmp ecx, 101
        jne .o2
        call sram_guarda
        mov ebx, nom_tmp
        mov byte [ebx], 0
        mov edi, t_abrir
        HV dlg_archivo
        test eax, eax
        jz hecho
        mov esi, nom_tmp
        call rom_abrir
        jmp hecho
.o2:
        cmp ecx, 102
        jne .o3
        mov esi, ruta_buzo
        call rom_abrir
        jmp hecho
.o3:
        cmp ecx, 103
        jne .o4
.reinicia:
        cmp byte [cargada], 0
        je hecho
        call sms_reiniciar
        mov byte [pausa], 0
        call al_empezar
        jmp .estado
.o4:
        cmp ecx, 104
        jne .o9
.pausa:
        cmp byte [cargada], 0
        je hecho
        xor byte [pausa], 1
.estado:
        call estado_cambia
        jmp hecho
.o9:
        cmp ecx, 109
        jne .o11
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp hecho
.o11:
        cmp ecx, 111
        jb hecho
        cmp ecx, 112
        ja .o21
        lea eax, [ecx-110]          ; el tamaño: la ventana, a la medida
        call tam_ventana
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        HV ven_mover
        HV ven_invalidar
        jmp hecho
.o21:
        cmp ecx, 121
        jne .o31
        cmp byte [snd_hay], 0
        jne .s1
        mov esi, t_nosnd
        mov edi, titulo
        jmp .dlg
.s1:
        xor byte [snd_si], 1
        call snd_ajusta
        jmp .estado
.o31:
        mov esi, t_tec
        mov edi, t_tec_tit
        cmp ecx, 131
        je .dlg
        mov esi, t_acerca
        mov edi, t_ac_tit
        cmp ecx, 132
        jne hecho
.dlg:
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
hecho:
        xor eax, eax
        ret

; (paso a paso) AL=botones: ECX cuadros con ellos y EDX sin ellos, y se pinta
pasos:
        pushad
        cmp byte [cargada], 0
        je .x
        mov byte [dibujar], 1
        call pon_pad
.a:
        test ecx, ecx
        jz .b
        call sms_cuadro
        dec ecx
        jmp .a
.b:
        xor eax, eax
        call pon_pad
.c:
        test edx, edx
        jz .d
        call sms_cuadro
        dec edx
        jmp .c
.d:
        mov dword [psg_n], 0
        call presenta
.x:
        popad
        ret

; la barra de estado ha cambiado
estado_cambia:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        HV ven_cli
        xor eax, eax
        lea ebx, [edx-EST_AL]
        mov edx, EST_AL
        HV ven_invalidar_rect
.x:
        popad
        ret

; =============================================================================
;  La pantalla
; =============================================================================
; (interna) las medidas: la zona, la escala y dónde va la imagen
medidas:
        pushad
        mov ebp, [ven]
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        sub edx, EST_AL
        mov eax, 1
        cmp ecx, 512
        jb .e
        cmp edx, 384
        jb .e
        inc eax
.e:
        mov [escala], eax
        mov ebx, eax
        shl ebx, 8
        sub ecx, ebx
        sar ecx, 1
        jns .x1
        xor ecx, ecx
.x1:
        mov [img_x], ecx
        imul ebx, eax, 192
        sub edx, ebx
        sar edx, 1
        jns .y1
        xor edx, edx
.y1:
        mov [img_y], edx
        popad
        ret

pintar:
        call medidas
        HV dc_pintar
        cmp byte [cargada], 0
        jne .img
        COLOR C_ARENA
        mov eax, 12
        mov ebx, 12
        mov esi, t_sin
        HV dc_texto
        jmp .barra
.img:
        call img_copia
.barra:
        COLOR C_ARENA
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, EST_AL
        mov ecx, [cli_an]
        mov edx, EST_AL
        HV dc_rect
        COLOR C_TINTA
        call estado_texto
        mov eax, 6
        inc ebx
        mov esi, txt_est
        HV dc_texto
        HV dc_cerrar
        jmp hecho

; el texto de la barra de estado, en txt_est
estado_texto:
        pushad
        mov edi, txt_est
        cmp byte [cargada], 0
        jne .a
        mov esi, titulo
        call hvl_cad
        jmp .f
.a:
        cmp byte [pausa], 0
        je .b
        mov esi, t_pausa
        call hvl_cad
.b:
        mov esi, [rom_nom]
        call hvl_cad
        cmp byte [a_pasos], 0
        jne .f
        cmp byte [pausa], 0
        jne .s
        mov word [edi], '  '
        add edi, 2
        mov eax, [cps]
        call hvl_num
        mov esi, t_cs
        call hvl_cad
        mov eax, [cps_pint]
        cmp eax, [cps]
        je .s
        mov word [edi], ' ('        ; no llega: cuántos se pintan
        add edi, 2
        call hvl_num
        mov al, ')'
        stosb
.s:
        cmp byte [snd_si], 0
        jne .f
        mov esi, t_sinson
        call hvl_cad
.f:
        mov byte [edi], 0
        popad
        ret

; el cuadro, a la ventana (fuera de WM_PAINT)
presenta:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        test dword [ebp+V_ESTADO], S_MINIM
        jnz .x
        call medidas
        HV dc_abrir
        call img_copia
        HV dc_cerrar
.x:
        popad
        ret

; (interna, con el contexto abierto) pasa «imagen» a los colores de Hispa
; Ventanas, al doble si toca, y la dibuja
img_copia:
        pushad
        mov esi, imagen
        mov edi, pant
        xor eax, eax
        cmp dword [escala], 2
        je .doble
        mov ecx, 256 * 192 / 4
.u:
        mov al, [esi]
        mov dl, [col_host+eax]
        mov al, [esi+1]
        mov dh, [col_host+eax]
        mov [edi], dx
        mov al, [esi+2]
        mov dl, [col_host+eax]
        mov al, [esi+3]
        mov dh, [col_host+eax]
        mov [edi+2], dx
        add esi, 4
        add edi, 4
        dec ecx
        jnz .u
        mov ecx, 256
        mov edx, 192
        jmp .copia
.doble:
        mov ebx, 192
.fila:
        mov ecx, 128
.d:
        mov al, [esi]
        mov dx, [col_doble+eax*2]
        mov [edi], dx
        mov al, [esi+1]
        mov dx, [col_doble+eax*2]
        mov [edi+2], dx
        add esi, 2
        add edi, 4
        dec ecx
        jnz .d
        push esi                    ; y la misma fila otra vez debajo
        lea esi, [edi-512]
        mov ecx, 128
        rep movsd
        pop esi
        dec ebx
        jnz .fila
        mov ecx, 512
        mov edx, 384
.copia:
        mov eax, [img_x]
        mov ebx, [img_y]
        mov esi, pant
        HV dc_copiar
        popad
        ret

; cada color de la consola (6 bits: rojo, verde y azul de dos bits; cada
; nivel, 0, 85, 170 o 255), al más parecido de la paleta de Hispa Ventanas
paleta_haz:
        pushad
        mov ebp, [hv_api + HV_pal_rgb * 4]
        xor ebx, ebx
.c:
        mov dword [mejor_d], 0x7FFFFFFF
        xor ecx, ecx
.p:
        xor edi, edi                ; la distancia: suma de los cuadrados
        xor esi, esi
.k:
        push ecx
        lea ecx, [esi+esi]          ; la componente ESI: bits 2*ESI y siguiente
        mov eax, ebx
        shr eax, cl
        and eax, 3
        imul eax, 85
        pop ecx
        lea edx, [esi+ecx*4]
        movzx edx, byte [ebp+edx]
        sub eax, edx
        imul eax, eax
        add edi, eax
        inc esi
        cmp esi, 3
        jb .k
        cmp edi, [mejor_d]
        jae .s
        mov [mejor_d], edi
        mov [col_host+ebx], cl
        mov ch, cl
        mov [col_doble+ebx*2], cx
        xor ch, ch
.s:
        inc ecx
        cmp ecx, 256
        jb .p
        inc ebx
        cmp ebx, 64
        jb .c
        popad
        ret

; =============================================================================
;  El reloj: los tics del 8254 (1.193.182 por segundo) desde que se arrancó
; =============================================================================
; El tic de la BIOS (18,2 por segundo) es muy basto para 60 cuadros: se
; afina con lo que lleva contado el contador 0, que es el que lo produce. El
; contador puede estar en modo 3 (onda cuadrada: baja de dos en dos y da dos
; vueltas por tic) o en modo 2: se pregunta con la orden de «leer atrás».
; -> EAX (da la vuelta cada hora: solo valen las diferencias)
reloj:
        push ebx
        push ecx
        push edx
.otra:
        mov ebx, [cero]
        mov ecx, [ebx+0x46C]
        mov al, 0xC2                ; estado y cuenta del contador 0
        out 0x43, al
        in al, 0x40
        mov dl, al                  ; el estado: bit 7 la salida, bits 1-3 el modo
        in al, 0x40
        mov ah, al
        in al, 0x40
        xchg al, ah                 ; AX = la cuenta
        cmp ecx, [ebx+0x46C]
        jne .otra
        movzx eax, ax
        neg eax
        add eax, 0x10000            ; lo que lleva contado
        and eax, 0xFFFF
        mov dh, dl
        and dh, 0x06
        cmp dh, 0x06
        jne .m2
        shr eax, 1                  ; modo 3: media vuelta por cada mitad
        test dl, 0x80
        jnz .m2
        add eax, 0x8000
.m2:
        shl ecx, 16
        add eax, ecx
        mov edx, eax                ; nunca hacia atrás (el tic de la BIOS
        sub edx, [reloj_ult]        ; llega un pelo después de la vuelta)
        jns .ok
        cmp edx, -0x20000
        jl .ok
        mov eax, [reloj_ult]
.ok:
        mov [reloj_ult], eax
        pop edx
        pop ecx
        pop ebx
        ret

; el puntero a la dirección lineal 0 (para los datos de la BIOS y HSPJOY)
cero_busca:
        pushad
        mov ebx, ds
        mov edi, desc
        mov eax, 0x000B
        int 0x31
        mov eax, [desc+2]
        and eax, 0x00FFFFFF
        movzx ebx, byte [desc+7]
        shl ebx, 24
        or eax, ebx
        neg eax
        mov [cero], eax
        popad
        ret

; una vez por segundo: los cuadros que salen y la partida guardada
cada_segundo:
        pushad
        call reloj
        mov edx, eax
        sub edx, [t_seg]
        cmp edx, TICS_SEG
        jb .x
        mov [t_seg], eax
        mov eax, [n_emu]
        mov ebx, [n_pint]
        mov dword [n_emu], 0
        mov dword [n_pint], 0
        cmp eax, [cps]
        jne .cambia
        cmp ebx, [cps_pint]
        je .igual
.cambia:
        mov [cps], eax
        mov [cps_pint], ebx
        call estado_cambia
.igual:
        call sram_vigila
        cmp byte [hubo_boton], 0    ; se está jugando (quizá solo con el
        je .x                       ; mando): que no salte el salvapantallas
        mov byte [hubo_boton], 0
        HV tics
        mov edx, eax
        mov eax, AJ_ULT_ACT
        HV aj_poner
.x:
        popad
        ret

; después de abrir, reiniciar o seguir: el reloj empieza ahora
al_empezar:
        pushad
        call reloj
        mov [t_sig], eax
        mov [t_seg], eax
        xor eax, eax
        mov [n_emu], eax
        mov [n_pint], eax
        mov [snd_n], eax
        mov [psg_n], eax
        mov dword [cps], 60
        mov dword [cps_pint], 60
        popad
        ret

; =============================================================================
;  Los mandos: teclado y HSPJOY
; =============================================================================
mandos_lee:
        pushad
        xor ebx, ebx                ; BL = jugador 1, BH = jugador 2
        cmp byte [foco], 0
        je .mandos
        mov esi, [p_mapa]
        cmp byte [esi+0x38], 0      ; con Alt se va a los menús
        jne .mandos
        xor ecx, ecx
.t:
        movzx eax, byte [teclas+ecx*2]
        cmp byte [esi+eax], 0
        je .t1
        or bl, [teclas+ecx*2+1]
        mov byte [mapa_vale], 1
.t1:
        cmp byte [blandas+ecx], 0
        je .t2
        dec byte [blandas+ecx]
        cmp byte [mapa_vale], 0
        jne .t2
        or bl, [teclas+ecx*2+1]
.t2:
        inc ecx
        cmp ecx, N_TECLAS
        jb .t
.mandos:
        mov esi, [joy]
        test esi, esi
        jz .fin
        movzx eax, byte [esi+HJD_J1E]
        call .uno
        or bl, al
        movzx eax, byte [esi+HJD_J2E]
        call .uno
        or bh, al
.fin:
        mov al, bl
        or al, bh
        or [hubo_boton], al
        mov al, bh                  ; la pausa: la de cualquiera de los dos
        and al, B_PAUSA
        or bl, al
        mov al, bl                  ; arriba y abajo a la vez, o izquierda y
        call .contra                ; derecha: ninguno
        call pon_pad
        mov al, bh
        call .contra
        and al, 0x3F
        mov [pad_estado+1], al
        popad
        ret
.contra:
        mov ah, al
        and ah, B_ARRIBA | B_ABAJO
        cmp ah, B_ARRIBA | B_ABAJO
        jne .c1
        and al, ~(B_ARRIBA | B_ABAJO)
.c1:
        mov ah, al
        and ah, B_IZQ | B_DER
        cmp ah, B_IZQ | B_DER
        jne .c2
        and al, ~(B_IZQ | B_DER)
.c2:
        ret
; (interna) EAX=número del mando (FFh: no hay) -> AL=sus botones
.uno:
        push ecx
        push edx
        push edi
        cmp eax, 6
        jae .u0
        movzx ecx, byte [esi+HJD_TAM]
        imul eax, ecx
        movzx ecx, word [esi+HJD_TABLA]
        add eax, ecx
        mov edi, [joy_seg]
        add edi, eax                ; EDI = la entrada del mando
        test byte [edi+HJS_FLAGS], 1
        jz .u0
        xor eax, eax
        mov cl, [edi+HJS_CRUZ]      ; la cruceta: arriba, abajo, izq., der.
        and cl, 0x0F                ; (en los mismos bits que la consola)
        or al, cl
        cmp byte [edi+HJS_EJES], 64 ; la palanca
        ja .e1
        or al, B_IZQ
.e1:
        cmp byte [edi+HJS_EJES], 192
        jb .e2
        or al, B_DER
.e2:
        cmp byte [edi+HJS_EJES+1], 64
        ja .e3
        or al, B_ARRIBA
.e3:
        cmp byte [edi+HJS_EJES+1], 192
        jb .e4
        or al, B_ABAJO
.e4:
        movzx edx, word [edi+HJS_BTN]
        test dl, 0x01
        jz .b1
        or al, B_1
.b1:
        test dl, 0x02
        jz .b2
        or al, B_2
.b2:
        cmp byte [edi+HJS_NBTN], 4
        ja .mas
        test dl, 0x0C               ; el 3 o el 4: la pausa
        jz .u9
        or al, B_PAUSA
        jmp .u9
.mas:
        test dl, 0x04               ; con más botones: el 3 y el 4 son otra
        jz .m1                      ; vez el 2 y el 1, y la pausa del 7 al 10
        or al, B_2
.m1:
        test dl, 0x08
        jz .m2
        or al, B_1
.m2:
        test edx, 0x03C0
        jz .u9
        or al, B_PAUSA
        jmp .u9
.u0:
        xor eax, eax
.u9:
        pop edi
        pop edx
        pop ecx
        ret

; ¿está HSPJOY? (INT 2Fh AX=D900h, en modo real)
mandos_busca:
        pushad
        mov edi, rmcs
        mov ecx, 50 / 2
        xor eax, eax
        rep stosw
        mov dword [rmcs+0x1C], 0xD900
        mov edi, rmcs
        mov ebx, 0x002F
        xor ecx, ecx
        mov eax, 0x0300
        int 0x31
        jc .x
        cmp byte [rmcs+0x1C], 0xFF
        jne .x
        cmp word [rmcs+0x10], 0x484A
        jne .x
        movzx eax, word [rmcs+0x22] ; ES:DI -> los datos
        shl eax, 4
        add eax, [cero]
        mov [joy_seg], eax
        movzx edx, word [rmcs]
        add eax, edx
        mov [joy], eax
        mov dl, [eax+HJD_OPC]       ; que mire los mandos en cada tic
        mov [joy_opc], dl
        or byte [eax+HJD_OPC], 1
.x:
        popad
        ret

mandos_suelta:
        push eax
        push edx
        mov eax, [joy]
        test eax, eax
        jz .x
        mov dl, [joy_opc]
        and dl, 1
        jnz .x
        and byte [eax+HJD_OPC], ~1
.x:
        pop edx
        pop eax
        ret

; =============================================================================
;  El sonido: las muestras de la consola, a HSPSND
; =============================================================================
; El núcleo deja en psg_buf las muestras del cuadro (16 bits con signo, un
; canal, a [psg_hz] por segundo). Aquí se pasan al formato de la tarjeta y se
; juntan hasta tener un trozo de la medida que la tarjeta toca de una vez
; (así HSPSND no lo completa con silencio): 2.048 tramas de 16 bits en
; estéreo a 48.000 Hz con HD Audio y AC'97, 1.024 de 16 bits y un canal a
; 22.050 Hz con una Sound Blaster 16 y 4.096 muestras de 8 bits con las
; anteriores. Antes de mandarlo se pregunta a HSPSND si cabe sin esperar (su
; función 08h; con un HSPSND anterior, se lleva la cuenta con el reloj, como
; Hispa-Nes): si no cabe, se queda para la vuelta siguiente, y si se juntan
; más de dos trozos, lo más viejo se tira. La ventana nunca espera.
sonido_manda:
        pushad
        mov ecx, [psg_n]
        mov dword [psg_n], 0
        cmp byte [snd_si], 0
        je near .x
        test ecx, ecx
        jz .manda
        mov eax, ecx                ; ¿cabe? (si no, fuera el trozo más viejo)
        imul eax, [snd_trama]
        add eax, [snd_n]
        mov edx, [snd_bytes]
        imul edx, 3
        cmp eax, edx
        jbe .cabe
        call .quita
.cabe:
        mov esi, psg_buf
        mov edi, snd_buf
        add edi, [snd_n]
        mov al, [snd_fmt]
.c:
        mov dx, [esi]
        add esi, 2
        cmp al, 3
        je .est
        test al, al
        jz .ocho
        mov [edi], dx               ; 16 bits, un canal
        add edi, 2
        jmp .s
.est:
        mov [edi], dx               ; 16 bits, estéreo
        mov [edi+2], dx
        add edi, 4
        jmp .s
.ocho:
        mov dl, dh                  ; 8 bits sin signo
        add dl, 0x80
        mov [edi], dl
        inc edi
.s:
        dec ecx
        jnz .c
        sub edi, snd_buf
        mov [snd_n], edi
.manda:
        mov eax, [snd_n]
        cmp eax, [snd_bytes]
        jb .x
        call snd_cabe
        jc .x
        mov esi, snd_buf
        mov ecx, [snd_bytes]
        mov edx, [snd_hz]
        mov al, [snd_fmt]
        HV snd_pcm2
        call .quita
        jmp .manda
.x:
        popad
        ret
; (interna) quita el primer trozo del búfer
.quita:
        push ecx
        push esi
        push edi
        mov esi, snd_buf
        mov edi, esi
        add esi, [snd_bytes]
        mov ecx, [snd_n]
        sub ecx, [snd_bytes]
        jae .q1
        xor ecx, ecx
.q1:
        mov [snd_n], ecx
        rep movsb
        pop edi
        pop esi
        pop ecx
        ret

; ¿cabe otro trozo en la cola de HSPSND sin esperar? -> CF=1 si no
snd_cabe:
        pushad
        cmp dword [hsp_ver], 3
        jb .reloj
        mov eax, 0x0800             ; HSPSND, función 08h: cuántos caben
        HV snd_orden
        jc .no
        test al, al
        jz .no
        jmp .si
.reloj:                             ; un HSPSND anterior: por el reloj, como
        call reloj                  ; Hispa-Nes (sin más de tres sonando)
        xor ecx, ecx
        xor ebx, ebx
.c:
        mov edx, [snd_fin+ebx*4]
        sub edx, eax
        jle .c1
        cmp edx, TICS_SEG
        ja .c1
        inc ecx
.c1:
        inc ebx
        cmp ebx, 4
        jb .c
        cmp ecx, 3
        jae .no
        mov edx, [snd_ult]          ; este empieza cuando acabe el anterior
        mov ebx, edx
        sub ebx, eax
        jle .ya
        cmp ebx, TICS_SEG
        jbe .d
.ya:
        mov edx, eax                ; (o ahora, si no sonaba nada)
.d:
        add edx, [snd_tics]
        mov [snd_ult], edx
        mov ebx, [snd_i]
        mov [snd_fin+ebx*4], edx
        inc ebx
        and ebx, 3
        mov [snd_i], ebx
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; ¿qué tarjeta lleva HSPSND? -> el formato y la medida de los trozos
snd_busca:
        pushad
        cmp byte [snd_hay], 0
        je .x
        xor edx, edx
        HV snd_info                 ; -> EAX=tarjeta ESI=nombre EDX=versión
        mov [hsp_ver], edx
        test eax, eax
        jg .t
        mov byte [snd_hay], 0
        mov byte [snd_si], 0
        jmp .x
.t:
        mov dword [snd_hz], 48000   ; HD Audio y AC'97 (y las que vengan)
        mov dword [snd_bloque], 2048
        mov byte [snd_fmt], 3
        mov dword [snd_trama], 4
        cmp eax, 3
        jae .f
        mov dword [snd_hz], 22050
        mov dword [snd_bloque], 1024 ; Sound Blaster 16
        mov byte [snd_fmt], 1
        mov dword [snd_trama], 2
        cmp eax, 2
        je .f
        mov dword [snd_bloque], 4096 ; las de 8 bits
        mov byte [snd_fmt], 0
        mov dword [snd_trama], 1
.f:
        mov eax, [snd_bloque]
        imul eax, [snd_trama]
        mov [snd_bytes], eax
        mov eax, [snd_bloque]       ; lo que dura un trozo, en tics del reloj
        mov ecx, TICS_SEG
        mul ecx
        div dword [snd_hz]
        mov [snd_tics], eax
.x:
        popad
        ret

; con el sonido puesto o quitado: el núcleo hace muestras o no
snd_ajusta:
        pushad
        xor eax, eax
        cmp byte [snd_si], 0
        je .a
        mov eax, [snd_hz]
.a:
        mov [psg_hz], eax
        call psg_frecuencia
        mov dword [psg_n], 0
        mov dword [snd_n], 0
        popad
        ret

; =============================================================================
;  Los archivos: la ROM y la partida guardada
; =============================================================================
; ESI=nombre de la ROM: la abre y enciende la consola. Si no puede, lo dice
; (salvo con [callado]) y sigue lo que había
rom_abrir:
        pushad
        mov edi, ruta_tmp           ; la ruta entera (el .SAV va al lado y la
        call ruta_entera            ; carpeta de trabajo puede cambiar luego)
        mov esi, ruta_tmp
        xor al, al
        HV dos_abrir
        jc near .noabre
        mov ebx, eax
        xor edx, edx                ; lo que mide: el puntero, al final
        mov al, 2
        HV dos_puntero
        jc near .mala
        mov [tam_nuevo], eax
        cmp eax, ROM_MENOS
        jb near .mala
        cmp eax, ROM_MAX
        ja .grande
        xor edx, edx
        xor al, al
        HV dos_puntero
        mov eax, [tam_nuevo]
        call sms_tam_buf
        mov ecx, eax
        HV mem_bloque
        jnc .mem
.grande:
        mov esi, t_nomem
        jmp .error
.mem:
        mov [blq_nuevo], eax
        mov [mango_nuevo], edx
        mov edi, eax
        mov ecx, [tam_nuevo]
        HV dos_leer
        jc .corta
        cmp eax, [tam_nuevo]
        je .leida
.corta:
        mov edx, [mango_nuevo]
        HV mem_bloque_soltar
        jmp .mala
.leida:
        HV dos_cerrar
        ; ya está: fuera el cartucho anterior (su partida, guardada)
        call rom_cerrar
        mov eax, [blq_nuevo]
        mov [rom], eax
        mov eax, [tam_nuevo]
        mov [rom_tam], eax
        mov eax, [mango_nuevo]
        mov [rom_mango], eax
        call sms_cargar
        call sms_arrancar
        mov esi, ruta_tmp           ; su nombre (sin la carpeta) y su .SAV
        mov edi, rom_ruta
        mov edx, edi
.n:
        lodsb
        stosb
        cmp al, '\'
        jne .n1
        mov edx, edi
.n1:
        test al, al
        jnz .n
        mov [rom_nom], edx
        mov esi, rom_ruta
        mov edi, sav_ruta
        mov eax, 'SAV'
        call hvl_ruta
        call sram_lee
        mov byte [cargada], 1
        mov byte [pausa], 0
        call al_empezar
        mov ebp, [ven]
        HV ven_invalidar
        cmp byte [a_pasos], 0
        je .x
        xor eax, eax                ; paso a paso: un segundo para arrancar
        mov ecx, 60
        xor edx, edx
        call pasos
        jmp .x
.mala:
        HV dos_cerrar
        mov esi, t_mala
        jmp .error2
.noabre:
        mov esi, t_noabre
        jmp .error2
.error:
        HV dos_cerrar
.error2:
        cmp byte [callado], 0
        jne .x
        mov ebp, [ven]
        mov edi, titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
.x:
        popad
        ret

; quita el cartucho: guarda su partida y suelta su memoria
rom_cerrar:
        pushad
        cmp byte [cargada], 0
        je .x
        call sram_guarda
        mov byte [cargada], 0
        mov edx, [rom_mango]
        HV mem_bloque_soltar
.x:
        popad
        ret

; ESI=nombre -> en EDI (80), con unidad y carpeta
ruta_entera:
        pushad
        cmp byte [esi+1], ':'
        je .tal
        push edi
        HV dos_dir_actual           ; «A:\CARPETA»
        pop edi
        cmp byte [esi], '\'
        jne .rel
        add edi, 2                  ; desde la raíz: solo falta la unidad
        jmp .tal
.rel:
        xor al, al
        mov ecx, 80
        repne scasb
        dec edi
        cmp byte [edi-1], '\'
        je .tal
        mov al, '\'
        stosb
.tal:
        mov ecx, 79
.c:
        lodsb
        cmp al, 'a'                 ; (en mayúsculas, como los enseña DOS)
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 0x20
.c1:
        stosb
        test al, al
        jz .x
        dec ecx
        jnz .c
        mov byte [edi], 0
.x:
        popad
        ret

; la partida guardada, a la RAM del cartucho (si hay archivo; si no, a cero)
sram_lee:
        pushad
        mov byte [sram_pend], 0
        mov dword [sram_seg], 0
        mov edi, sram
        mov ecx, SRAM_TAM / 4
        xor eax, eax
        rep stosd
        mov esi, sav_ruta
        xor al, al
        HV dos_abrir
        jc .x
        mov ebx, eax
        mov edi, sram
        mov ecx, SRAM_TAM
        HV dos_leer
        HV dos_cerrar
.x:
        mov esi, sram
        mov edi, sram_copia
        mov ecx, SRAM_TAM / 4
        rep movsd
        mov esi, sram
        mov edi, sram_vista
        mov ecx, SRAM_TAM / 4
        rep movsd
        popad
        ret

; la guarda si el juego usa la RAM del cartucho y ha cambiado desde la
; última vez
sram_guarda:
        pushad
        mov byte [sram_pend], 0
        mov dword [sram_seg], 0
        cmp byte [cargada], 0
        je .x
        cmp byte [sram_usada], 0
        je .x
        mov esi, sram
        mov edi, sram_copia
        mov ecx, SRAM_TAM / 4
        repe cmpsd
        je .x
        mov esi, sram
        mov edi, sram_copia
        mov ecx, SRAM_TAM / 4
        rep movsd
        mov esi, sav_ruta
        mov edx, sram
        mov ecx, SRAM_TAM
        call hvl_escribir
.x:
        popad
        ret

; (cada segundo) se guarda sola dos segundos después del último cambio, o a
; los treinta si el juego no para de escribir
sram_vigila:
        cmp byte [sram_usada], 0
        je .x
        pushad
        mov esi, sram               ; ¿ha cambiado en este segundo?
        mov edi, sram_vista
        mov ecx, SRAM_TAM / 4
        repe cmpsd
        je .quieta
        mov esi, sram
        mov edi, sram_vista
        mov ecx, SRAM_TAM / 4
        rep movsd
        popad
        mov byte [sram_pend], 2
        inc dword [sram_seg]
        cmp dword [sram_seg], 30
        jb .x
        jmp sram_guarda
.quieta:
        popad
        cmp byte [sram_pend], 0
        je .x
        dec byte [sram_pend]
        jz sram_guarda
.x:
        ret

; =============================================================================
;  La prueba (/T=n): n cuadros a toda velocidad y el resultado, a un archivo
; =============================================================================
prueba:
        pushad
        cmp byte [cargada], 0
        je near .x
        call reloj
        mov [t_seg], eax
        mov ecx, [a_prueba]
.c:
        mov byte [dibujar], 1
        xor eax, eax
        call pon_pad
        call sms_cuadro
        mov dword [psg_n], 0
        cmp byte [a_ventana], 0
        je .c1
        call presenta
        HV ceder
.c1:
        dec ecx
        jnz .c
        call reloj
        sub eax, [t_seg]
        mov ebx, eax                ; EBX = tics que ha tardado
        call presenta
        mov edi, txt_log
        mov esi, k_cuadros
        call hvl_cad
        mov eax, [a_prueba]
        call hvl_num
        call .nl
        mov esi, k_ms
        call hvl_cad
        mov eax, ebx
        mov ecx, 1000
        mul ecx
        mov ecx, TICS_SEG
        div ecx
        call hvl_num
        call .nl
        mov esi, k_cps
        call hvl_cad
        mov eax, [a_prueba]         ; cuadros x tics por segundo / tics
        mov ecx, TICS_SEG
        mul ecx
        test ebx, ebx
        jnz .d
        inc ebx
.d:
        div ebx
        call hvl_num
        call .nl
        mov esi, k_modo
        call hvl_cad
        mov esi, k_modo0
        cmp byte [a_ventana], 0
        je .m
        mov esi, k_modo1
        call hvl_cad
        mov eax, [escala]
        call hvl_num
        jmp .m1
.m:
        call hvl_cad
.m1:
        call .nl
        mov esi, k_mapa
        call hvl_cad
        mov eax, [map_tipo]
        call hvl_num
        call .nl
        mov esi, k_imagen
        call hvl_cad
        mov esi, imagen
        mov ecx, 256 * 192
        call .suma
        call .nl
        mov esi, k_ram
        call hvl_cad
        mov esi, ram
        mov ecx, 0x2000
        call .suma
        call .nl
        mov esi, k_estado
        call hvl_cad
        movzx eax, byte [sram]
        call hvl_num
        call .nl
        mov esi, k_texto            ; lo que la ROM de prueba haya escrito
        call hvl_cad
        mov eax, [sram]
        shr eax, 8
        cmp eax, 0x61B0DE
        jne .sintexto
        mov esi, sram + 4
        mov ecx, 200
.t:
        lodsb
        test al, al
        jz .sintexto
        cmp al, 10
        jne .t1
        mov al, '|'
.t1:
        cmp al, ' '
        jb .t2
        stosb
.t2:
        dec ecx
        jnz .t
.sintexto:
        call .nl
        mov ecx, edi
        sub ecx, txt_log
        mov esi, ruta_log
        mov edx, txt_log
        call hvl_escribir
        call al_empezar
.x:
        popad
        ret
.nl:
        mov ax, 0x0A0D
        stosw
        ret
; (interna) la suma de ECX octetos desde ESI, en hexadecimal, a EDI
.suma:
        xor edx, edx
        xor eax, eax
.s1:
        lodsb
        rol edx, 5
        add edx, eax
        dec ecx
        jnz .s1
        mov ecx, 8
.s2:
        rol edx, 4
        mov al, dl
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .s3
        add al, 7
.s3:
        stosb
        dec ecx
        jnz .s2
        ret

%include "z80.inc"
%include "vdp.inc"
%include "psg.inc"
%include "mapas.inc"
%include "hvplib.inc"
%include "datos.inc"
            alignb 4
ven         resd 1
cero        resd 1
p_mapa      resd 1                  ; el mapa de teclas pulsadas de Hispa Ventanas
joy         resd 1                  ; los datos de HSPJOY (0: no está)
joy_seg     resd 1                  ;   y su segmento
a_prueba    resd 1
escala      resd 1
cli_an      resd 1
cli_al      resd 1
img_x       resd 1
img_y       resd 1
t_sig       resd 1                  ; cuándo toca el cuadro siguiente
t_seg       resd 1
reloj_ult   resd 1
n_emu       resd 1                  ; cuadros en este segundo, y pintados
n_pint      resd 1
cps         resd 1
cps_pint    resd 1
mejor_d     resd 1
rom_mango   resd 1
rom_nom     resd 1
blq_nuevo   resd 1
mango_nuevo resd 1
tam_nuevo   resd 1
hsp_ver     resd 1                  ; la versión de la interfaz de HSPSND
snd_hz      resd 1                  ; el formato de la tarjeta: frecuencia,
snd_bloque  resd 1                  ;   tramas de cada trozo,
snd_trama   resd 1                  ;   octetos de cada trama
snd_bytes   resd 1                  ;   y de cada trozo
snd_tics    resd 1                  ; lo que dura un trozo, en tics del reloj
snd_n       resd 1                  ; octetos juntados en snd_buf
snd_i       resd 1
snd_ult     resd 1
snd_fin     resd 4
sram_seg    resd 1
msg         resb M_TAM
rmcs        resb 52
desc        resb 8
a_pasos     resb 1
a_ventana   resb 1
a_uno       resb 1
cargada     resb 1
pausa       resb 1
foco        resb 1
saltados    resb 1
callado     resb 1
snd_hay     resb 1
snd_si      resb 1
snd_fmt     resb 1                  ; 0: 8 bits, 1: 16 bits, 3: 16 bits estéreo
joy_opc     resb 1
mapa_vale   resb 1
hubo_boton  resb 1
sram_pend   resb 1
blandas     resb N_TECLAS
col_host    resb 64
            alignb 2
col_doble   resw 64
nom_tmp     resb 80
ruta_tmp    resb 160
rom_ruta    resb 160
sav_ruta    resb 160
ruta_log    resb 80
ruta_buzo   resb 96
txt_est     resb 160
txt_log     resb 512
            alignb 4
snd_buf     resb TROZO_MAX * 3 + 16
sram_copia  resb SRAM_TAM           ; lo que hay en el .SAV
sram_vista  resb SRAM_TAM           ; lo que había hace un segundo
pant        resb 512 * 384
HVP_FIN
