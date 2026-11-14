; =============================================================================
;  HISPANES.HVP - Hispa-Nes, la consola de 8 bits de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un emulador compatible con los cartuchos de la NES (volcados en formato
;  iNES, archivos .NES). Todo el código es propio, escrito a partir de la
;  documentación pública del hardware: no lleva nada del fabricante de la
;  consola ni de otros emuladores, y las ROM comerciales no van en el
;  proyecto (quien tenga sus cartuchos usa sus volcados). Con él va un juego
;  propio, Cometas (HISPA1.NES), que se abre solo si está en su carpeta: NES,
;  al lado del módulo (allí van los cartuchos; Abrir ROM empieza en la última
;  carpeta, apuntada en HISPANES.INI). Al abrir un cartucho empieza en pausa,
;  con un letrero, hasta una tecla, un clic o un botón del mando; y se para
;  sola (callada) con un menú abierto, un diálogo, sin el foco o minimizada.
;
;  Las piezas (programas/hispanes/):
;    cpu.inc    el procesador (6502 sin modo decimal), con sus ciclos
;    ppu.inc    la imagen, línea a línea, los registros y los mandos
;    apu.inc    el sonido: dos canales de pulsos, triángulo, ruido y muestras
;    mapas.inc  los cartuchos (0, 1, 2, 3, 4, 7 y 66) y el formato iNES
;    datos.inc  las variables de la consola
;  y aquí, lo que la une a Hispa Ventanas: la ventana y el menú, el reloj
;  (60 cuadros por segundo, saltándose el dibujo de alguno si el equipo no
;  llega), el teclado y los mandos de HSPJOY, el sonido por HSPSND y los
;  archivos (la ROM y la partida guardada, .SAV, de los cartuchos con pila).
;
;  Teclas: flechas = cruceta, X = A, Z = B, Intro = Start, Mayús = Select;
;  P pone o quita la pausa y F2 reinicia. Mandos (si está HSPJOY): el del jugador 1 y el del
;  jugador 2; con cuatro botones o menos son A, B, Select y Start.
;
;  Parámetros del acceso directo: el nombre de una ROM, que se abre al
;  entrar, y para las pruebas:
;    /P     paso a paso: la consola solo avanza con cada tecla (seis cuadros
;           con el botón pulsado y seis sin él; Espacio, treinta sin botones)
;    /T=n   corre n cuadros a toda velocidad y apunta el resultado (tiempo,
;           suma de la imagen y de la memoria) en HISPANES.LOG; con /V,
;           pintando cada cuadro en la ventana
;    /1     tamaño x1 aunque la pantalla sea grande
; =============================================================================
%include "hvp.inc"

EST_AL      equ 18                  ; alto de la barra de estado
TICS_SEG    equ 1193182             ; tics del reloj (el contador 8254) por segundo
TICS_CUADRO equ 19854               ; y por cuadro (60,1 por segundo)
TICS_MUESTRA equ 54                 ; y por muestra de sonido (22.050 Hz)
ROM_MAX     equ 3 * 1024 * 1024
SND_MAX     equ 4096                ; muestras que caben esperando a HSPSND
; botones del mando, como los lee la consola
B_A         equ 0x01
B_B         equ 0x02
B_SELECT    equ 0x04
B_START     equ 0x08
B_ARRIBA    equ 0x10
B_ABAJO     equ 0x20
B_IZQ       equ 0x40
B_DER       equ 0x80
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

HVP_CABECERA "Hispa-Nes", entrada
        db "................................"
        db "................................"
        db "...............00..............."
        db "...............00..............."
        db "...............00..............."
        db "...............00..............."
        db "...............00..............."
        db "...............00..............."
        db "...............00..............."
        db ".000000000000000000000000000000."
        db ".0ffffffffffffffffffffffffffff0."
        db ".0f88888888888888888888888888f0."
        db ".0f8880008888ddddddd888888888f0."
        db ".0f8880f08888ddddddd888888888f0."
        db ".0f8880f0888888888888ddd8ddd8f0."
        db ".0f0000f000088888888d55dd55ddf0."
        db ".0f0fffffff088888888dddddddddf0."
        db ".0f0000f000081118111dddddddddf0."
        db ".0f8880f0888811181118ddd8ddd8f0."
        db ".0f8880f088888888888888888888f0."
        db ".0f77700077777777777777777777f0."
        db ".0f77777777777777777777777777f0."
        db ".0ffffffffffffffffffffffffffff0."
        db ".000000000000000000000000000000."
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

c_nes       dd nes_proc
            db C_TINTA, CB_FOCO, 0, 0
plant:
        PLANTILLA c_nes, titulo, E_NORMAL, 140, 30, 256 + 2*BORDE, \
                  240 + EST_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

menu:
        db "&Archivo", 0
        dw 101
        db "&Abrir ROM...", 0
        dw 102
        db "&Cometas (el juego de muestra)", 0
        dw 0
        db 0
        dw 103
        db "&Reiniciar (F2)", 0
        dw 104
        db "&Pausa o continuar (P)", 0
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
        db "&Acerca de Hispa-Nes...", 0
        dw 0xFFFF
        db 0

titulo      db "Hispa-Nes", 0
t_abrir     db "Abrir ROM (.NES)", 0
t_h1        db "NES\HISPA1.NES", 0   ; (en su carpeta, al lado del módulo)
t_viejo     db "HISPA1.NES", 0         ; (donde iba antes: al lado del módulo)
t_sub       db "NES", 0
t_clave     db "Carpeta=", 0
t_letrero1  db "En pausa", 0
t_letrero2  db "Pulse una tecla o haga clic", 0
t_sin       db "Sin cartucho. Archivo, Abrir ROM...", 0
t_pausa     db "Pausa - ", 0
t_cs        db " c/s", 0
t_sinson    db "  sin sonido", 0
t_noabre    db "No se ha podido abrir el archivo.", 0
t_mala      db "Este archivo no es una ROM en formato iNES (.NES).", 0
t_nomem     db "No hay memoria para un cartucho tan grande.", 0
t_mapa1     db "Este cartucho usa un circuito (el número ", 0
t_mapa2     db ") que", 10, "Hispa-Nes todavía no conoce.", 0
t_nosnd     db "No hay sonido: falta HSPSND o no ha encontrado tarjeta.", 0
t_tec_tit   db "Teclas y mandos", 0
t_tec       db "Flechas: la cruceta.    X: botón A.    Z: botón B.", 10
            db "Intro: Start.    Mayúsculas: Select.", 10
            db "P: pausa (o seguir).    F2: reiniciar la consola.", 10
            db "En pausa, cualquier tecla, un clic o un botón del mando sigue.", 10
            db 10
            db "Con HSPJOY cargado valen los mandos del jugador 1 y del 2:", 10
            db "cruceta o palanca y los botones 1 y 2 (A y B); Select y Start", 10
            db "son el 3 y el 4, o el 7 y el 8 (9 y 10) si el mando tiene más.", 0
t_ac_tit    db "Acerca de Hispa-Nes", 0
t_acerca    db "Hispa-Nes: emulador propio, compatible con los cartuchos", 10
            db "de la NES volcados en formato iNES (.NES).", 10
            db 10
            db "Hecho desde la documentación pública del hardware. Las ROM", 10
            db "comerciales no van con Hispa-DOS: use los volcados de sus", 10
            db "propios cartuchos. Cometas (HISPA1.NES) es un juego nuestro.", 10
            db 10
            db "Cartuchos: 0, 1, 2, 3, 4, 7 y 66. Partidas con pila: .SAV", 0
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
teclas      db 0x2D, B_A,  0x2C, B_B,  0x36, B_SELECT,  0x2A, B_SELECT,  0x1C, B_START
            db 0x48, B_ARRIBA,  0x50, B_ABAJO,  0x4B, B_IZQ,  0x4D, B_DER
N_TECLAS    equ ($ - teclas) / 2

; los 64 colores de la consola en rojo, verde y azul (tools/hispanes_paleta.py)
        align 4
nes_rgb:
        db 0x6F, 0x6F, 0x6F,  0x00, 0x31, 0xC3,  0x18, 0x0A, 0xE9,  0x55, 0x00, 0xDF    ; 00h
        db 0x83, 0x00, 0xA7,  0x99, 0x00, 0x4C,  0x93, 0x00, 0x00,  0x73, 0x25, 0x00    ; 04h
        db 0x3E, 0x4A, 0x00,  0x00, 0x65, 0x00,  0x00, 0x71, 0x00,  0x00, 0x6B, 0x07    ; 08h
        db 0x00, 0x54, 0x74,  0x00, 0x00, 0x00,  0x00, 0x00, 0x00,  0x00, 0x00, 0x00    ; 0Ch
        db 0xB5, 0xB5, 0xB5,  0x00, 0x68, 0xFF,  0x49, 0x39, 0xFF,  0x94, 0x11, 0xFF    ; 10h
        db 0xCE, 0x00, 0xFD,  0xEB, 0x07, 0x89,  0xE3, 0x2A, 0x00,  0xBA, 0x59, 0x00    ; 14h
        db 0x77, 0x86, 0x00,  0x29, 0xA9, 0x00,  0x00, 0xB8, 0x00,  0x00, 0xB0, 0x36    ; 18h
        db 0x00, 0x93, 0xBB,  0x00, 0x00, 0x00,  0x00, 0x00, 0x00,  0x00, 0x00, 0x00    ; 1Ch
        db 0xFF, 0xFF, 0xFF,  0x52, 0xB5, 0xFF,  0x97, 0x88, 0xFF,  0xDF, 0x64, 0xFF    ; 20h
        db 0xFF, 0x54, 0xFF,  0xFF, 0x5D, 0xD4,  0xFF, 0x7B, 0x50,  0xFF, 0xA6, 0x00    ; 24h
        db 0xC3, 0xD2, 0x00,  0x7A, 0xF3, 0x00,  0x3B, 0xFF, 0x00,  0x19, 0xFA, 0x86    ; 28h
        db 0x22, 0xDE, 0xFF,  0x57, 0x57, 0x57,  0x00, 0x00, 0x00,  0x00, 0x00, 0x00    ; 2Ch
        db 0xFF, 0xFF, 0xFF,  0xBA, 0xE1, 0xFF,  0xD5, 0xCF, 0xFF,  0xF2, 0xC1, 0xFF    ; 30h
        db 0xFF, 0xBB, 0xFF,  0xFF, 0xBE, 0xED,  0xFF, 0xCA, 0xB9,  0xFF, 0xDB, 0x8E    ; 34h
        db 0xE7, 0xEC, 0x77,  0xC9, 0xFA, 0x7D,  0xB1, 0xFF, 0x9D,  0xA5, 0xFD, 0xCE    ; 38h
        db 0xA8, 0xF2, 0xFF,  0xBC, 0xBC, 0xBC,  0x00, 0x00, 0x00,  0x00, 0x00, 0x00    ; 3Ch

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        push edi
        mov edi, ruta_log           ; HISPANES.LOG, al lado del módulo
        mov eax, 'LOG'
        call hvl_ruta
        mov edi, ruta_ini           ; HISPANES.INI: la carpeta de Abrir ROM
        mov eax, 'INI'
        call hvl_ruta
        mov edi, ruta_h1            ; y HISPA1.NES: la carpeta y el nombre
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
        mov ecx, edx                ; la carpeta del módulo, con la barra: a
        sub ecx, ruta_h1           ; la ruta vieja del juego propio y a la
        mov esi, ruta_h1           ; carpeta de los cartuchos (NES)
        mov edi, ruta_vieja
        rep movsb
        mov esi, t_viejo
        call hvl_cad
        mov byte [edi], 0
        mov ecx, edx
        sub ecx, ruta_h1
        mov esi, ruta_h1
        mov edi, dir_roms
        rep movsb
        mov esi, t_sub
        call hvl_cad
        mov byte [edi], 0
        mov edi, edx
        mov esi, t_h1
        call hvl_cad
        mov byte [edi], 0
.c2:
        call carpeta_lee            ; la última de Abrir ROM (HISPANES.INI)
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
        ; el cartucho del principio: el de los parámetros o Cometas
        mov esi, nom_tmp
        cmp byte [esi], 0
        je .propio
        call rom_abrir
        jmp .abierto
.propio:
        mov al, 1                   ; (si no está, no pasa nada)
        call abre_propio
.abierto:
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
        setnz al
        cmp al, [corriendo]
        je .igual
        mov [corriendo], al         ; arranca o se para (pausa, menú abierto,
        test al, al                 ; sin el foco, minimizada...)
        jz .para
        call al_empezar             ; sigue: el reloj empieza ahora
        jmp .igual
.para:
        call silencio               ; parada: sin sonido (y sin repetir trozos)
.igual:
        test al, al
        jnz .emula
        call pausa_mando            ; en pausa con un mando: se mira con un
        mov edi, msg                ; temporizador si aprieta un botón; parada:
        HV msg_tomar                ; a esperar un mensaje
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
        call nes_cuadro
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
        cmp byte [foco], 0          ; sin el foco (otra ventana, un diálogo)
        je .no
        cmp byte [en_menu], 0       ; con un menú suyo abierto
        jne .no
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
        imul edx, eax, 240
        add edx, EST_AL + 2*BORDE + TIT_H + 1 + MENU_H
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
nes_proc:
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
        cmp eax, WM_LBUTTONDOWN
        je .clic
        cmp eax, WM_ENTERMENULOOP
        je .menu_si
        cmp eax, WM_EXITMENULOOP
        je .menu_no
        HVJ ven_defecto
.clic:
        call despierta              ; en pausa: un clic en la imagen, a jugar
        jmp hecho
.menu_si:
        mov byte [en_menu], 1       ; con el menú abierto, quieta y callada
        jmp hecho
.menu_no:
        mov byte [en_menu], 0       ; (y luego, como estaba)
        jmp hecho
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
        call despierta              ; en pausa: cualquier tecla, a jugar (y
                                    ; vale también para la consola)
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
        call despierta
        cmp al, 'X'
        jne .l1
        mov eax, B_A
        jmp .boton
.l1:
        cmp al, 'Z'
        jne .l2
        mov eax, B_B
        jmp .boton
.l2:
        cmp byte [a_pasos], 0
        je hecho
        cmp al, 'S'                 ; (paso a paso: Select no es una tecla)
        jne .l3
        mov eax, B_SELECT
        jmp .boton
.l3:
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
        call abrir_dlg
        jmp hecho
.o2:
        cmp ecx, 102
        jne .o3
        xor eax, eax
        call abre_propio
        jmp hecho
.o3:
        cmp ecx, 103
        jne .o4
.reinicia:
        cmp byte [cargada], 0
        je hecho
        call nes_reset
        mov byte [pausa], 0
        call al_empezar
        jmp .estado
.o4:
        cmp ecx, 104
        jne .o9
.pausa:
        cmp byte [cargada], 0
        je hecho
        mov al, [pausa]
        xor al, 1
        call pausa_pon
        jmp hecho
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
        mov dword [snd_n], 0
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
        call silencio               ; (el diálogo no deja correr la consola)
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
hecho:
        xor eax, eax
        ret

; =============================================================================
;  La pausa: al abrir un cartucho, con P o con Archivo, Pausa o continuar. Se
;  sigue con una tecla, un clic en la imagen o un botón del mando. Aparte, la
;  consola se para sola (sin letrero) con un menú suyo abierto, sin el foco
;  o minimizada, y sigue como estaba al volver (corre y el bucle)
; =============================================================================
; AL=1 en pausa, 0 a jugar
pausa_pon:
        pushad
        cmp al, [pausa]
        je .x
        mov [pausa], al
        test al, al
        jz .pinta
        call botones_mando          ; los que ya están apretados no la quitan
        mov [joy_ant], al
.pinta:
        mov ebp, [ven]              ; el letrero y la barra de estado
        test ebp, ebp
        jz .x
        HV ven_invalidar
.x:
        popad
        ret

; una tecla o un clic: si está en pausa (la de verdad, no paso a paso), sigue
despierta:
        cmp byte [pausa], 0
        je .x
        cmp byte [cargada], 0
        je .x
        cmp byte [a_pasos], 0
        jne .x
        push eax
        xor al, al
        call pausa_pon
        pop eax
.x:
        ret

; (bucle, parado) en pausa con un mando: ¿un botón recién apretado? Entonces
; sigue. Mientras tanto, un temporizador (cada tic de 55 ms) despierta el
; bucle para mirarlo otra vez sin gastar el procesador; si no hace falta, se
; quita
PAUSA_TMP   equ 7
pausa_mando:
        pushad
        call .mira
        jc .x
        cmp byte [tmp_puesto], 0
        je .x
        mov byte [tmp_puesto], 0
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov ecx, PAUSA_TMP
        HV tmp_quitar
.x:
        popad
        ret
; -> CF=1 si toca mirar el mando (y ya se ha mirado)
.mira:
        pushad
        cmp dword [joy], 0
        je .no
        cmp byte [pausa], 0
        je .no
        cmp byte [cargada], 0
        je .no
        cmp byte [a_pasos], 0
        jne .no
        cmp byte [foco], 0
        je .no
        cmp byte [en_menu], 0
        jne .no
        mov eax, [ven]
        test eax, eax
        jz .no
        test dword [eax+V_ESTADO], S_MINIM
        jnz .no
        cmp byte [tmp_puesto], 0
        jne .m1
        mov byte [tmp_puesto], 1
        mov ebp, eax
        mov ecx, PAUSA_TMP
        mov edx, 55
        HV tmp_poner
.m1:
        call botones_mando
        mov ah, [joy_ant]
        mov [joy_ant], al
        not ah
        and al, ah
        jz .si
        xor al, al
        call pausa_pon
.si:
        popad
        stc
        ret
.no:
        popad
        clc
        ret

; -> AL=los botones (no la cruceta) de los dos mandos de HSPJOY, juntos
botones_mando:
        push esi
        push ebx
        xor ebx, ebx
        mov esi, [joy]
        test esi, esi
        jz .x
        movzx eax, byte [esi+HJD_J1E]
        call mandos_lee.uno
        mov bl, al
        movzx eax, byte [esi+HJD_J2E]
        call mandos_lee.uno
        or bl, al
        and bl, B_A | B_B | B_SELECT | B_START
.x:
        mov al, bl
        pop ebx
        pop esi
        ret

; (con el contexto abierto) en pausa, el letrero en medio de la imagen
LETRERO_AN  equ 27 * 8 + 24
LETRERO_AL  equ 44
letrero:
        pushad
        cmp byte [pausa], 0
        je .x
        cmp byte [a_pasos], 0
        jne .x
        mov eax, [escala]
        shl eax, 8
        sub eax, LETRERO_AN
        sar eax, 1
        add eax, [img_x]
        imul ebx, [escala], 240
        sub ebx, LETRERO_AL
        sar ebx, 1
        add ebx, [img_y]
        mov ecx, LETRERO_AN
        mov edx, LETRERO_AL
        COLOR C_CREMA
        HV dc_rect
        COLOR C_TINTA
        HV dc_marco
        push eax
        push ebx
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        COLOR C_OCRE
        HV dc_marco
        pop ebx
        pop eax
        COLOR C_TINTA
        push eax
        push ebx
        add eax, (LETRERO_AN - 8 * 8) / 2
        add ebx, 5
        NEGRITA 1
        mov esi, t_letrero1
        HV dc_texto
        NEGRITA 0
        pop ebx
        pop eax
        add eax, 12
        add ebx, 23
        mov esi, t_letrero2
        HV dc_texto
.x:
        popad
        ret

; sin sonido: lo juntado, fuera, y HSPSND parado (HD Audio, si no, repetiría
; sus últimos trozos mientras la consola está quieta)
silencio:
        pushad
        xor eax, eax
        mov [apu_n], eax
        mov [snd_n], eax
        mov [snd_ult], eax
        mov edi, snd_fin
        mov ecx, 4
        rep stosd
        cmp byte [snd_si], 0
        je .x
        mov eax, 0x0300             ; HSPSND, función 03h: parar
        HV snd_orden
.x:
        popad
        ret

; =============================================================================
;  Las carpetas: los cartuchos van en NES, al lado del módulo; Abrir ROM
;  empieza en la última carpeta de la que se abrió uno (apuntada en el .INI)
; =============================================================================
; el juego propio: en su carpeta o, si no está, donde iba antes. AL=1 callado
abre_propio:
        pushad
        mov byte [callado], 1
        mov esi, ruta_h1
        call rom_abrir
        cmp byte [rom_ok], 0
        jne .x
        mov [callado], al
        mov esi, ruta_vieja
        call rom_abrir
.x:
        mov byte [callado], 0
        popad
        ret

; Archivo, Abrir ROM: el diálogo, en la última carpeta (si ya no está, en la
; de los cartuchos); luego la carpeta de trabajo vuelve a ser la de antes
abrir_dlg:
        pushad
        call silencio
        mov edi, dir_ant
        HV dos_dir_actual
        mov esi, dir_ult
        HV dos_ir_carpeta
        jnc .en
        mov esi, dir_roms
        HV dos_ir_carpeta
.en:
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov byte [ebx], 0
        mov edi, t_abrir
        HV dlg_archivo
        test eax, eax
        jz .vuelve
        mov esi, nom_tmp            ; (con la carpeta del diálogo)
        call rom_abrir
        cmp byte [rom_ok], 0
        je .vuelve
        mov esi, rom_ruta           ; la carpeta del cartucho: la última
        mov edi, dir_tmp
        mov ecx, [rom_nom]
        sub ecx, esi
        dec ecx                     ; (sin la barra del final...
        cmp ecx, 2
        ja .c
        inc ecx                     ; ...salvo en la raíz: «A:\»)
.c:
        rep movsb
        mov byte [edi], 0
        mov esi, dir_tmp
        mov edi, dir_ult
        call cad_igual
        je .vuelve
        call cad0
        call carpeta_guarda
.vuelve:
        mov esi, dir_ant
        HV dos_ir_carpeta
        popad
        ret

; ESI, EDI: dos cadenas -> ZF=1 si son iguales
cad_igual:
        push eax
        push esi
        push edi
.c:
        mov al, [esi]
        cmp al, [edi]
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

; copia ESI a EDI con su 0
cad0:
        push edi
        call hvl_cad
        mov byte [edi], 0
        pop edi
        ret

; dir_ult: la del .INI («Carpeta=») o, si no hay, la de los cartuchos
carpeta_lee:
        pushad
        mov esi, dir_roms
        mov edi, dir_ult
        call cad0
        mov esi, ruta_ini
        mov edi, txt_ini
        mov ecx, 128
        call hvl_leer
        test eax, eax
        jz .x
        mov esi, txt_ini
        mov edi, t_clave
        call hvl_clave
        jc .x
        cmp byte [esi], ' '
        jb .x
        mov edi, dir_ult
        mov ecx, 79
.c:
        lodsb
        cmp al, ' '
        jb .f
        stosb
        dec ecx
        jnz .c
.f:
        mov byte [edi], 0
.x:
        popad
        ret

; apunta dir_ult en el .INI
carpeta_guarda:
        pushad
        mov edi, txt_ini
        mov esi, t_clave
        call hvl_cad
        mov esi, dir_ult
        call hvl_cad
        mov ax, 0x0A0D
        stosw
        mov ecx, edi
        mov edx, txt_ini
        sub ecx, edx
        mov esi, ruta_ini
        call hvl_escribir
        popad
        ret

; (paso a paso) AL=botones: ECX cuadros con ellos y EDX sin ellos, y se pinta
pasos:
        pushad
        cmp byte [cargada], 0
        je .x
        mov byte [dibujar], 1
        mov [pad_estado], al
.a:
        test ecx, ecx
        jz .b
        call nes_cuadro
        dec ecx
        jmp .a
.b:
        mov byte [pad_estado], 0
.c:
        test edx, edx
        jz .d
        call nes_cuadro
        dec edx
        jmp .c
.d:
        mov dword [apu_n], 0
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
        cmp edx, 480
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
        imul ebx, eax, 240
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
        call letrero
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
        mov ecx, 256 * 240 / 4
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
        mov edx, 240
        jmp .copia
.doble:
        mov ebx, 240
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
        mov edx, 480
.copia:
        mov eax, [img_x]
        mov ebx, [img_y]
        mov esi, pant
        HV dc_copiar
        popad
        ret

; cada color de la consola, al más parecido de la paleta de Hispa Ventanas
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
        lea eax, [ebx+ebx*2]
        add eax, esi
        movzx eax, byte [nes_rgb+eax]
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
        mov [apu_n], eax
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
        mov al, bl                  ; arriba y abajo a la vez, o izquierda y
        call .contra                ; derecha: ninguno
        mov [pad_estado], al
        mov al, bh
        call .contra
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
        and cl, 0x0F
        shl cl, 4
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
        or al, B_A
.b1:
        test dl, 0x02
        jz .b2
        or al, B_B
.b2:
        cmp byte [edi+HJS_NBTN], 4
        ja .mas
        test dl, 0x04
        jz .b3
        or al, B_SELECT
.b3:
        test dl, 0x08
        jz .u9
        or al, B_START
        jmp .u9
.mas:
        test dl, 0x04               ; con más botones: el 3 y el 4 son otra
        jz .m1                      ; vez B y A, y Select y Start los de en medio
        or al, B_B
.m1:
        test dl, 0x08
        jz .m2
        or al, B_A
.m2:
        test edx, 0x0140            ; 7 o 9
        jz .m3
        or al, B_SELECT
.m3:
        test edx, 0x0280            ; 8 o 10
        jz .u9
        or al, B_START
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
;  El sonido: las muestras del cuadro, a HSPSND
; =============================================================================
; HSPSND toca trozos, uno detrás de otro (tiene sitio para cuatro), y si no
; le cabe otro hace esperar a quien se lo manda. La ventana no puede
; esperar: aquí se lleva la cuenta de cuándo acaba cada trozo mandado y solo
; se manda otro si quedan como mucho dos sonando; si no, ese se pierde.
; Cada trozo tiene que ser de la medida que la tarjeta toca de una vez (si
; no, HSPSND lo completa con silencio y se oye entrecortado): 1.024 muestras
; con una Sound Blaster 16 y hasta 900 con AC'97 y HD Audio. Con una Sound
; Blaster de 8 bits el trozo entero serían 4.096, más de lo que se le puede
; dar de una vez: suena a saltos, y por eso ahí empieza sin sonido.
sonido_manda:
        pushad
        mov ecx, [apu_n]
        mov dword [apu_n], 0
        cmp byte [snd_si], 0
        je near .x
        mov edi, [snd_n]
        lea eax, [edi+ecx]
        cmp eax, SND_MAX
        jbe .cabe
        xor edi, edi                ; (no se ha podido mandar: lo viejo, fuera)
.cabe:
        lea eax, [edi+ecx]
        mov [snd_n], eax
        add edi, snd_buf
        mov esi, apu_buf
        rep movsb
        mov eax, [snd_n]
        cmp eax, [snd_bloque]
        jb near .x
        call reloj
        xor ecx, ecx                ; ¿cuántos trozos siguen sonando?
        xor ebx, ebx
.c:
        mov edx, [snd_fin+ebx*4]
        sub edx, eax
        jle .c1
        cmp edx, TICS_SEG           ; (más de un segundo: es de otra vuelta)
        ja .c1
        inc ecx
.c1:
        inc ebx
        cmp ebx, 4
        jb .c
        cmp ecx, 3
        jae .x
        mov edx, [snd_ult]          ; este empieza cuando acabe el anterior
        mov ebx, edx
        sub ebx, eax
        jle .ya
        cmp ebx, TICS_SEG
        jbe .d
.ya:
        mov edx, eax                ; (o ahora, si no sonaba nada)
.d:
        imul ebx, [snd_bloque], TICS_MUESTRA
        add edx, ebx
        mov [snd_ult], edx
        mov ebx, [snd_i]
        mov [snd_fin+ebx*4], edx
        inc ebx
        and ebx, 3
        mov [snd_i], ebx
        mov esi, snd_buf
        mov ecx, [snd_bloque]
        mov edx, APU_HZ
        xor eax, eax                ; 8 bits, un canal
        HV snd_pcm
        mov esi, snd_buf            ; lo que sobra, al principio
        mov edi, esi
        add esi, [snd_bloque]
        mov ecx, [snd_n]
        sub ecx, [snd_bloque]
        mov [snd_n], ecx
        rep movsb
.x:
        popad
        ret

; ¿qué tarjeta lleva HSPSND? (INT 2Fh AX=D700h y su función 0, en modo real)
snd_busca:
        pushad
        mov dword [snd_bloque], 1024
        cmp byte [snd_hay], 0
        je .x
        call .limpia
        mov dword [rmcs+0x1C], 0xD700
        mov ebx, 0x002F
        mov eax, 0x0300
        int 0x31
        jc .x
        cmp byte [rmcs+0x1C], 0xFF
        jne .x
        mov ax, [rmcs+0x10]         ; ES:BX = su punto de entrada
        mov dx, [rmcs+0x22]
        call .limpia
        mov [rmcs+0x2A], ax
        mov [rmcs+0x2C], dx
        xor ebx, ebx
        mov eax, 0x0301
        int 0x31
        jc .x
        mov al, [rmcs+0x1C]         ; 1 Sound Blaster, 2 SB16, 3 AC'97, 4 HD Audio
        cmp al, 2
        je .x
        mov dword [snd_bloque], 900
        cmp al, 1
        jne .x
        mov dword [snd_bloque], 2048
        mov byte [snd_si], 0
.x:
        popad
        ret
.limpia:
        push eax
        mov edi, rmcs
        mov ecx, 50 / 2
        xor eax, eax
        rep stosw
        mov edi, rmcs
        xor ecx, ecx
        pop eax
        ret

; =============================================================================
;  Los archivos: la ROM y la partida guardada
; =============================================================================
; ESI=nombre de la ROM: la abre y enciende la consola. Si no puede, lo dice
; (salvo con [callado]) y sigue lo que había
rom_abrir:
        pushad
        mov byte [rom_ok], 0
        mov edi, ruta_tmp           ; la ruta entera (el .SAV va al lado y la
        call ruta_entera            ; carpeta de trabajo puede cambiar luego)
        mov esi, ruta_tmp
        xor al, al
        HV dos_abrir
        jc near .noabre
        mov ebx, eax
        mov edi, cab
        mov ecx, 16
        HV dos_leer
        jc near .mala
        cmp eax, 16
        jne near .mala
        ; (la cabecera se mira sin tocar el cartucho que hay puesto)
        mov esi, rom_prg
        mov edi, rom_ant
        mov ecx, ROM_VARS
        rep movsd
        mov eax, [map_ini]
        mov [map_ant], eax
        mov esi, cab
        call nes_cabecera
        jnc .cab
        push eax
        call .repone
        pop eax
        test eax, eax
        jz near .mala
        jmp .mapa
.cab:
        mov eax, [rom_prg_n]
        shl eax, 13
        mov [tam_prg], eax
        mov ecx, [rom_chr_n]
        shl ecx, 10
        add ecx, eax
        mov [tam_rom], ecx
        cmp ecx, ROM_MAX
        ja .grande
        add ecx, 16
        HV mem_bloque
        jnc .mem
.grande:
        call .repone
        mov esi, t_nomem
        jmp .error
.mem:
        mov [blq_nuevo], eax
        mov [mango_nuevo], edx
        cmp dword [rom_entren], 0
        je .se
        mov edi, entren
        mov ecx, 512
        HV dos_leer
.se:
        mov edi, [blq_nuevo]
        mov ecx, [tam_rom]
        HV dos_leer
        jc .corta
        cmp eax, [tam_rom]
        je .leida
.corta:
        mov edx, [mango_nuevo]
        HV mem_bloque_soltar
        call .repone
        jmp .mala
.leida:
        HV dos_cerrar
        ; ya está: fuera el cartucho anterior (su partida, guardada)
        mov esi, rom_prg            ; (rom_cerrar necesita los datos del viejo)
        mov edi, rom_nueva
        mov ecx, ROM_VARS
        rep movsd
        call .repone
        call rom_cerrar
        mov esi, rom_nueva
        mov edi, rom_prg
        mov ecx, ROM_VARS
        rep movsd
        mov eax, [map_nuevo]
        mov [map_ini], eax
        mov eax, [blq_nuevo]
        mov [rom_prg], eax
        add eax, [tam_prg]
        mov [rom_chr], eax
        mov eax, [mango_nuevo]
        mov [rom_mango], eax
        call nes_arrancar
        cmp dword [rom_entren], 0
        je .se2
        mov esi, entren
        mov edi, sram + 0x1000
        mov ecx, 512
        rep movsb
.se2:
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
        mov byte [rom_ok], 1
        mov byte [pausa], 0
        call al_empezar
        mov ebp, [ven]
        HV ven_invalidar
        cmp dword [a_prueba], 0     ; (/T: la prueba, enseguida)
        jne .x
        xor eax, eax                ; un segundo para arrancar (el título)
        mov ecx, 60
        xor edx, edx
        call pasos
        cmp byte [a_pasos], 0
        jne .x
        mov byte [joy_ant], 0       ; y en pausa, hasta una tecla, un clic o
        mov al, 1                   ; un botón del mando
        call pausa_pon
        mov byte [joy_ant], 0       ; (un botón ya apretado también vale)
        call al_empezar
        jmp .x
.mapa:
        HV dos_cerrar
        mov edi, txt_dlg
        mov esi, t_mapa1
        call hvl_cad
        mov eax, [mapa_malo]
        call hvl_num
        mov esi, t_mapa2
        call hvl_cad
        mov byte [edi], 0
        mov esi, txt_dlg
        jmp .error2
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
; (interna) los datos del cartucho, como estaban
.repone:
        mov eax, [rom_mapa]
        mov [mapa_malo], eax
        mov eax, [map_ini]
        mov [map_nuevo], eax
        mov eax, [map_ant]
        mov [map_ini], eax
        mov esi, rom_ant
        mov edi, rom_prg
        mov ecx, ROM_VARS
        rep movsd
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

; la partida guardada, a la RAM del cartucho (si tiene pila y hay archivo)
sram_lee:
        pushad
        mov byte [sram_sucia], 0
        mov byte [sram_pend], 0
        cmp dword [rom_bateria], 0
        je .x
        mov esi, sav_ruta
        xor al, al
        HV dos_abrir
        jc .x
        mov ebx, eax
        mov edi, sram
        mov ecx, 0x2000
        HV dos_leer
        HV dos_cerrar
.x:
        mov esi, sram
        mov edi, sram_copia
        mov ecx, 0x2000 / 4
        rep movsd
        popad
        ret

; la guarda si ha cambiado desde la última vez
sram_guarda:
        pushad
        mov byte [sram_sucia], 0
        mov byte [sram_pend], 0
        mov dword [sram_seg], 0
        cmp byte [cargada], 0
        je .x
        cmp dword [rom_bateria], 0
        je .x
        mov esi, sram
        mov edi, sram_copia
        mov ecx, 0x2000 / 4
        repe cmpsd
        je .x
        mov esi, sram
        mov edi, sram_copia
        mov ecx, 0x2000 / 4
        rep movsd
        mov esi, sav_ruta
        mov edx, sram
        mov ecx, 0x2000
        call hvl_escribir
.x:
        popad
        ret

; (cada segundo) se guarda sola dos segundos después del último cambio, o a
; los treinta si el juego no para de escribir
sram_vigila:
        cmp dword [rom_bateria], 0
        je .x
        cmp byte [sram_sucia], 0
        je .quieta
        mov byte [sram_sucia], 0
        mov byte [sram_pend], 2
        inc dword [sram_seg]
        cmp dword [sram_seg], 30
        jb .x
        jmp sram_guarda
.quieta:
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
        mov word [pad_estado], 0
        call nes_cuadro
        mov dword [apu_n], 0
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
        mov eax, [rom_mapa]
        call hvl_num
        call .nl
        mov esi, k_imagen
        call hvl_cad
        mov esi, imagen
        mov ecx, 256 * 240
        call .suma
        call .nl
        mov esi, k_ram
        call hvl_cad
        mov esi, ram
        mov ecx, 0x800
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

%include "cpu.inc"
%include "ppu.inc"
%include "apu.inc"
%include "mapas.inc"
%include "hvplib.inc"
%include "datos.inc"
ROM_VARS    equ 8                   ; rom_prg ... rom_entren (dd seguidos)
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
tam_prg     resd 1
tam_rom     resd 1
mapa_malo   resd 1
map_ant     resd 1
map_nuevo   resd 1
rom_ant     resd ROM_VARS
rom_nueva   resd ROM_VARS
snd_n       resd 1
snd_bloque  resd 1                  ; muestras de cada trozo, según la tarjeta
snd_i       resd 1
snd_ult     resd 1
snd_fin     resd 4
sram_seg    resd 1
msg         resb M_TAM
rmcs        resb 52
desc        resb 8
cab         resb 16
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
joy_opc     resb 1
mapa_vale   resb 1
hubo_boton  resb 1
sram_pend   resb 1
blandas     resb N_TECLAS
col_host    resb 64
            alignb 2
col_doble   resw 64
nom_tmp     resb 80
dir_roms    resb 80                 ; la carpeta de los cartuchos (NES)
dir_ult     resb 80                 ; la última de Abrir ROM
dir_ant     resb 80                 ; la de trabajo, mientras está el diálogo
dir_tmp     resb 80
ruta_ini    resb 80
ruta_vieja  resb 96
txt_ini     resb 160
en_menu     resb 1                  ; un menú suyo está abierto
corriendo   resb 1                  ; la consola corría en la vuelta anterior
joy_ant     resb 1                  ; (en pausa) los botones de los mandos
rom_ok      resb 1                  ; rom_abrir ha podido
tmp_puesto  resb 1                  ; el temporizador de mirar el mando en pausa
ruta_tmp    resb 160
rom_ruta    resb 160
sav_ruta    resb 160
ruta_log    resb 80
ruta_h1     resb 96
txt_est     resb 160
txt_dlg     resb 160
txt_log     resb 512
entren      resb 512
snd_buf     resb SND_MAX + 16
sram_copia  resb 0x2000
pant        resb 512 * 480
HVP_FIN
