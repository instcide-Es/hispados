; =============================================================================
;  HISPADRV.HVP - Hispa-Drive, la consola de 16 bits de Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un emulador de la consola de 16 bits de 1988 (la Mega Drive: los
;  cartuchos volcados en archivos .MD, .BIN, .GEN o .SMD). Todo el código es
;  propio, escrito a partir de la documentación pública del hardware (el
;  68000, el VDP, el Z80, el YM2612 y el SN76489): no lleva nada del
;  fabricante de la consola ni de otros emuladores, y las ROM comerciales no
;  van en el proyecto (quien tenga sus cartuchos usa sus volcados). Con él va
;  una ROM propia, la prueba de la consola (PRUEBA.MD, en la carpeta MD, al
;  lado del módulo, donde van los cartuchos; Abrir ROM empieza en la última
;  carpeta, en HISPADRV.INI). Al abrir un cartucho empieza en pausa, con un
;  letrero, hasta una tecla, un clic o un botón del mando; y se para sola
;  (callada) con un menú abierto, un diálogo, sin el foco o minimizada.
;
;  Las piezas (programas/hispadrv/):
;    m68k.inc   el procesador principal (68000); sus instrucciones las hace
;               tools/gen68k.py (m68k_gen.inc, en la compilación)
;    vdp.inc    la imagen: los puertos, las DMA y el dibujo de cada línea
;    bus.inc    el cartucho (y su RAM con pila), la memoria, los mandos y el
;               bus del Z80
;    md.inc     un cuadro: las líneas, las interrupciones y el reparto del
;               tiempo
;    ym.inc     el sonido FM (YM2612; sus tablas, de tools/genym.py)
;    datos.inc  las variables
;  y de programas/comun/, las de Hispa-System: z80.inc (el Z80 del sonido) y
;  psg.inc (el SN76489). Aquí, lo que la une a Hispa Ventanas: la ventana y
;  el menú, el reloj (59,92 cuadros por segundo, o 49,70 en PAL, saltándose
;  el dibujo de alguno si el equipo no llega), el teclado y los mandos de
;  HSPJOY, el sonido por HSPSND (en trozos de la medida de la tarjeta, sin
;  esperar nunca) y los archivos (la ROM y la partida guardada, .SRM).
;
;  Corre también en Hispa-Pentium, con HVPRUN (docs/PENTIUM.md): lo nota
;  como Hispa-Snes (aj_leer AJ_TXT_VERSION da 0); entonces el reloj es el
;  fino de HVPRUN (aj_leer 20) y no hay puertos, HSPJOY ni sonido.
;
;  Teclas: flechas = cruceta, Z/X/C = A/B/C, A/S/D = X/Y/Z, Intro = Start,
;  M = Mode; P pone o quita la pausa del emulador y F2 reinicia. Mandos (si
;  está HSPJOY): el del jugador 1 y el del jugador 2.
;
;  Parámetros del acceso directo: el nombre de una ROM, que se abre al
;  entrar, y:
;    /6     mando de seis botones (si no, el de HISPADRV.INI; de tres)
;    /R=n   la región: 1 Japón, 2 América, 3 Europa (si no, la de la ROM)
;    /1     tamaño x1 aunque la pantalla sea grande
;  y para las pruebas:
;    /P     paso a paso: la consola solo avanza con cada tecla (seis cuadros
;           con el botón pulsado y seis sin él; Espacio, treinta sin botones)
;    /T=n   corre n cuadros a toda velocidad y apunta el resultado (tiempo,
;           suma de la imagen y de la memoria) en HISPADRV.LOG; con /V,
;           pintando cada cuadro en la ventana
; =============================================================================
%include "hvp.inc"

EST_AL      equ 18                  ; alto de la barra de estado
JUEGO_AN    equ 320                 ; [PANTALLA COMPLETA] el cuadro de la consola
JUEGO_AL    equ 224
FULL_MAX    equ 3                   ; escala entera máxima (la medida de «pant»)
TICS_SEG    equ 1193182             ; tics del reloj (el contador 8254) por segundo
TICS_CUADRO equ 19913               ; y por cuadro (59,92 por segundo)
TICS_PAL    equ 24006
AJ_RELOJ    equ 20                  ; (HVPRUN) aj_leer: el reloj fino               ; (en PAL: 49,70 por segundo)
ROM_MAX     equ 4 * 1024 * 1024 + 512
ROM_MENOS   equ 512                 ; una ROM más pequeña no es de la consola
SRAM_TAM    equ 0x10000             ; la RAM del cartucho: 64 KB
TROZO_MAX   equ 8192                ; octetos de un trozo de sonido, como mucho
BLANDA_N    equ 8                   ; cuadros que vale una tecla sin mapa
BLANDA_PENT equ 32                  ;   (en Hispa-Pentium: hasta que repite,
BLANDA_REP  equ 4                   ;    y las repeticiones, 25 por segundo)
; botones del mando (pad_md)
B_ARRIBA    equ 0x001
B_ABAJO     equ 0x002
B_IZQ       equ 0x004
B_DER       equ 0x008
B_B         equ 0x010
B_C         equ 0x020
B_A         equ 0x040
B_START     equ 0x080
B_Z         equ 0x100
B_Y         equ 0x200
B_X         equ 0x400
B_MODE      equ 0x800
B_BOTONES   equ 0xFF0               ; los que no son la cruceta
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

HVP_CABECERA "Hispa-Drive", entrada
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "....000000000......000000000...."
        db "...07777777770000007777777770..."
        db "..0777777777777777777777777770.."
        db ".077700077777777777777777777770."
        db ".077708077777777777777774477770."
        db ".077008007777777777777744447770."
        db ".078888888777700007744774477770."
        db ".077008007777777777744447777770."
        db ".077708077777777777774477447770."
        db ".077700077777777777777774444770."
        db ".077777777777777777777777447770."
        db ".077777777700000000777777777770."
        db "..0777777770........0777777770.."
        db "...0777770............0777770..."
        db "....00000..............00000...."
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

c_md        dd md_proc
            db C_TINTA, CB_FOCO, 0, 0
plant:
        PLANTILLA c_md, titulo, E_NORMAL, 140, 30, 320 + 2*BORDE, \
                  224 + EST_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

menu:
        db "&Archivo", 0
        dw 101
        db "&Abrir ROM...", 0
        dw 102
        db "&La prueba de la consola", 0
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
        dw 0
        db 0
        dw 113
pc_marca    db ' '                  ; [PANTALLA COMPLETA] la marca (* si activa)
        db " &Pantalla completa (Alt+Intro)", 0
        dw 0xFFFF
        db "&Mando", 0
        dw 141
m3_marca    db '*'
        db " Mando de &3 botones", 0
        dw 142
m6_marca    db ' '
        db " Mando de &6 botones", 0
        dw 0xFFFF
        db "&Sonido", 0
        dw 121
        db "Poner o quitar el &sonido", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw 131
        db "&Teclas y mandos...", 0
        dw 132
        db "&Acerca de Hispa-Drive...", 0
        dw 0xFFFF
        db 0

titulo      db "Hispa-Drive", 0
t_abrir     db "Abrir ROM (.MD, .BIN, .GEN, .SMD)", 0
t_buzo      db "MD\PRUEBA.MD", 0    ; (en su carpeta, al lado del módulo)
t_sub       db "MD", 0
t_clave     db "Carpeta=", 0
t_clave6    db "Mando=", 0
t_letrero1  db "En pausa", 0
t_letrero2  db "Pulse una tecla o haga clic", 0
t_sin       db "Sin cartucho. Archivo, Abrir ROM...", 0
t_pausa     db "Pausa - ", 0
t_cs        db " c/s", 0
t_sinson    db "  sin sonido", 0
t_noabre    db "No se ha podido abrir el archivo.", 0
t_mala      db "Este archivo no es una ROM de la consola (.MD, .BIN, .GEN o .SMD).", 0
t_nomem     db "No hay memoria para un cartucho tan grande.", 0
t_nosnd     db "No hay sonido: falta HSPSND o no ha encontrado tarjeta.", 0
t_tec_tit   db "Teclas y mandos", 0
t_tec       db "Flechas: la cruceta.    Intro: Start.", 10
            db "Z, X, C: los botones A, B y C.", 10
            db "A, S, D: los botones X, Y y Z (mando de seis).    M: Mode.", 10
            db "P: pausa del emulador (o seguir).    F2: reiniciar.", 10
            db "En pausa, cualquier tecla, un clic o un botón del mando sigue.", 10
            db 10
            db "Con HSPJOY cargado valen los mandos del jugador 1 y del 2:", 10
            db "botón 1 A, 2 B, 3 C; con más de cuatro, 4 X, 5 Y, 6 Z, 9 Mode", 10
            db "y Start el 7, el 8 o el 10 (con cuatro o menos, Start es el 4).", 10
            db 10
            db "Algunos juegos antiguos no van con el mando de seis botones:", 10
            db "si uno no responde bien, pruebe con el de tres (menú Mando).", 0
t_ac_tit    db "Acerca de Hispa-Drive", 0
t_acerca    db "Hispa-Drive: emulador propio de la consola de 16 bits", 10
            db "de 1988 (la Mega Drive: cartuchos .MD, .BIN, .GEN, .SMD).", 10
            db 10
            db "Hecho desde la documentación pública del hardware. Las ROM", 10
            db "comerciales no van con Hispa-DOS: use los volcados de sus", 10
            db "propios cartuchos. PRUEBA.MD es nuestra.", 10
            db 10
            db "Partidas de los cartuchos con pila: .SRM, al lado de la ROM.", 0
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
k_mapa      db "Region=", 0

; teclas -> botones: código de la tecla y botón
        align 4
teclas      db 0x2C, 0x2D, 0x2E, 0x1E, 0x1F, 0x20, 0x1C, 0x32
            db 0x48, 0x50, 0x4B, 0x4D
N_TECLAS    equ $ - teclas
            align 2
tec_bot     dw B_A, B_B, B_C, B_X, B_Y, B_Z, B_START, B_MODE
            dw B_ARRIBA, B_ABAJO, B_IZQ, B_DER

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        ; ¿Hispa Ventanas o HVPRUN (Hispa-Pentium)? Como Hispa-Snes: HVPRUN
        ; no tiene texto de versión
        mov eax, AJ_TXT_VERSION
        HV aj_leer
        test eax, eax
        setz [pentium]
        push edi
        mov edi, ruta_log           ; HISPADRV.LOG, al lado del módulo
        mov eax, 'LOG'
        call hvl_ruta
        mov edi, ruta_ini           ; HISPADRV.INI: la carpeta de Abrir ROM
        mov eax, 'INI'
        call hvl_ruta
        mov edi, ruta_buzo          ; y la prueba: la carpeta y el nombre
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
        mov ecx, edx                ; la carpeta de los cartuchos: MD, al
        sub ecx, ruta_buzo          ; lado del módulo
        mov esi, ruta_buzo
        mov edi, dir_roms
        rep movsb
        mov esi, t_sub
        call hvl_cad
        mov byte [edi], 0
        mov edi, edx
        mov esi, t_buzo
        call hvl_cad
        mov byte [edi], 0
.c2:
        call carpeta_lee            ; la última de Abrir ROM (HISPADRV.INI)
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
        mov al, '6'
        call hvl_opcion
        jc .s6
        mov byte [mando6], 1
.s6:
        mov esi, edi
        mov al, 'T'
        call hvl_opcion
        jc .st
        call hvl_lee_num
        mov [a_prueba], eax
.st:
        mov esi, edi
        mov al, 'R'
        call hvl_opcion
        jc .sm
        call hvl_lee_num
        cmp eax, 3
        ja .sm
        mov al, [.reg+eax]
        mov [region_forzar], al
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
        call mando_marca
        cmp byte [pentium], 0
        jne .sp
        call cero_busca             ; (en DOS: la BIOS y HSPJOY)
        call mandos_busca
.sp:
        call paleta_haz
        call md_rutinas
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
        ; el cartucho del principio: el de los parámetros (si no, nada)
        mov esi, nom_tmp
        cmp byte [esi], 0
        je .abierto
        call rom_abrir
.abierto:
        cmp dword [a_prueba], 0
        je bucle
        call prueba
        jmp bucle
.reg:   db 0, 0x01, 0x81, 0xC1      ; (la región + 1: Japón, América, Europa)

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
        mov ecx, TICS_CUADRO
        cmp byte [region], 0xC0
        jne .a0
        mov ecx, TICS_PAL
.a0:
        lea ebx, [ecx*5]
        cmp edx, ebx
        jb .a
        mov [t_sig], eax            ; muy atrasado (un diálogo, el disco...):
        xor edx, edx                ; se empieza a contar otra vez
.a:
        add [t_sig], ecx
        mov byte [dibujar], 1
        cmp edx, ecx                ; va con retraso: este cuadro no se pinta
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
        call md_cuadro
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
        imul ecx, eax, 320
        add ecx, 2*BORDE
        imul edx, eax, 224
        add edx, EST_AL + 2*BORDE + TIT_H + 1 + MENU_H
        ret

; AX=botones -> el mando 1
pon_pad:
        mov [pad_md], ax
        ret

; las marcas del menú Mando
mando_marca:
        mov byte [m3_marca], '*'
        mov byte [m6_marca], ' '
        mov al, [mando6]
        mov [seis_md], al
        mov [seis_md+1], al
        test al, al
        jz .x
        mov byte [m3_marca], ' '
        mov byte [m6_marca], '*'
.x:
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
md_proc:
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
        cmp ecx, TC_INTRO           ; Alt+Intro: entra o sale de pantalla completa
        jne .tnpc
        test dl, TM_ALT
        jz .tnpc
        call pc_alterna
        jmp hecho
.tnpc:
        cmp ecx, TC_ESC             ; Esc: si está en pantalla completa, a ventana
        jne .tnesc                  ; (en ventana no cierra el emulador)
        cmp byte [pantc], 0
        je hecho
        call pc_salir
        jmp hecho
.tnesc:
        cmp ecx, 0x3C               ; F2
        je .reinicia
        call despierta              ; en pausa: cualquier tecla, a jugar (y
                                    ; vale también para la consola)
        xor esi, esi
.t1:
        cmp cl, [teclas+esi]
        je .t2
        inc esi
        cmp esi, N_TECLAS
        jb .t1
        jmp hecho
.t2:
        movzx eax, word [tec_bot+esi*2]
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
        xor esi, esi                ; las letras de los botones
.l1:
        cmp al, [.letras+esi]
        je .l3
        inc esi
        cmp esi, 8
        jb .l1
        jmp .l2
.letras: db "ZXCASD", 13, "M"
.l3:
        movzx eax, word [tec_bot+esi*2]
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
        cmp ax, [tec_bot+ecx*2]
        jne .b2
        mov dl, BLANDA_N
        cmp byte [pentium], 0
        je .b3
        ; en Hispa-Pentium no hay mapa: una tecla que se deja pulsada se
        ; repite a los 0,5 s y luego 25 veces por segundo. La primera vale
        ; hasta que empieza a repetirse; las repetidas, poco (si se suelta,
        ; el botón se suelta enseguida)
        mov dl, BLANDA_PENT
        cmp byte [blandas+ecx], 0
        je .b3
        mov dl, BLANDA_REP
.b3:
        mov [blandas+ecx], dl
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
.o14:
        sub ecx, 141                ; Mando: de tres o de seis botones
        cmp cl, [mando6]
        je hecho
        mov [mando6], cl
        call mando_marca
        call carpeta_guarda
        jmp hecho
.o3:
        cmp ecx, 141
        je .o14
        cmp ecx, 142
        je .o14
        cmp ecx, 103
        jne .o4
.reinicia:
        cmp byte [cargada], 0
        je hecho
        call md_reiniciar
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
        cmp ecx, 113                ; Pantalla completa (entrar o salir)
        jne .o11v
        call pc_alterna
        jmp hecho
.o11v:
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
        mov [joy_ant], ax
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
        mov dx, [joy_ant]
        mov [joy_ant], ax
        not dx
        and ax, dx
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
        mov ebx, eax
        movzx eax, byte [esi+HJD_J2E]
        call mandos_lee.uno
        or ebx, eax
        and ebx, B_BOTONES
.x:
        mov eax, ebx
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
        imul eax, [escala], 320
        sub eax, LETRERO_AN
        sar eax, 1
        add eax, [img_x]
        imul ebx, [escala], 224
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
        mov [psg_n], eax
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
;  Las carpetas: los cartuchos van en SMS, al lado del módulo; Abrir ROM
;  empieza en la última carpeta de la que se abrió uno (apuntada en el .INI)
; =============================================================================
; la prueba de la consola (PRUEBA.MD, en la carpeta de los cartuchos). AL=1 callado
abre_propio:
        pushad
        mov [callado], al
        mov esi, ruta_buzo
        call rom_abrir
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
        mov ecx, 150
        call hvl_leer
        test eax, eax
        jz .x
        mov esi, txt_ini            ; Mando=6: el de seis botones
        mov edi, t_clave6
        call hvl_clave
        jc .m
        cmp byte [esi], '6'
        jne .m
        mov byte [mando6], 1
.m:
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
        mov esi, t_clave6
        call hvl_cad
        mov al, '3'
        cmp byte [mando6], 0
        je .m
        mov al, '6'
.m:
        stosb
        mov ax, 0x0A0D
        stosw
        mov ecx, edi
        mov edx, txt_ini
        sub ecx, edx
        mov esi, ruta_ini
        call hvl_escribir
        popad
        ret

; (paso a paso) AX=botones: ECX cuadros con ellos y EDX sin ellos, y se pinta
pasos:
        pushad
        cmp byte [cargada], 0
        je .x
        mov byte [dibujar], 1
        call pon_pad
.a:
        test ecx, ecx
        jz .b
        call md_cuadro
        dec ecx
        jmp .a
.b:
        xor eax, eax
        call pon_pad
.c:
        test edx, edx
        jz .d
        call md_cuadro
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
        cmp byte [pantc], 0         ; en pantalla completa, otras medidas
        jne medidas_full
        pushad
        mov ebp, [ven]
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        sub edx, EST_AL
        mov eax, 1
        cmp ecx, 640
        jb .e
        cmp edx, 448
        jb .e
        inc eax
.e:
        mov [escala], eax
        imul ebx, eax, 320
        sub ecx, ebx
        sar ecx, 1
        jns .x1
        xor ecx, ecx
.x1:
        mov [img_x], ecx
        imul ebx, eax, 224
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
        cmp byte [pantc], 0         ; en pantalla completa no hay barra de estado
        jne .cierra
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
.cierra:
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

; (interna, con el contexto abierto) dibuja «imagen» (ya en los colores de
; Hispa Ventanas), al doble si toca
img_copia:
        cmp byte [pantc], 0         ; en pantalla completa, el escalado entero
        jne img_copia_full
        pushad
        mov esi, imagen
        cmp dword [escala], 2
        je .doble
        mov ecx, 320                ; x1: tal cual
        mov edx, 224
        jmp .copia
.doble:
        mov edi, pant
        mov ebx, 224
.fila:
        mov ecx, 160
.d:
        mov al, [esi]               ; dos puntos, cada uno dos veces
        mov ah, al
        mov dl, [esi+1]
        mov dh, dl
        mov [edi], ax
        mov [edi+2], dx
        add esi, 2
        add edi, 4
        dec ecx
        jnz .d
        push esi                    ; y la misma fila otra vez debajo
        lea esi, [edi-640]
        mov ecx, 160
        rep movsd
        pop esi
        dec ebx
        jnz .fila
        mov esi, pant
        mov ecx, 640
        mov edx, 448
.copia:
        mov eax, [img_x]
        mov ebx, [img_y]
        HV dc_copiar
        popad
        ret

; cada uno de los 512 colores de la consola (3 bits de azul, de verde y de
; rojo: BBBGGGRRR; cada nivel, de 0 a 252 de 36 en 36), al más parecido de
; la paleta de Hispa Ventanas: md_col9. Y col_host, la paleta de la pantalla
; completa (emufull.inc), que aquí no cambia nada: «imagen» ya va en colores
; de Ventanas
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
        lea ecx, [esi+esi*2]        ; la componente ESI (0 rojo, 1 verde, 2 azul)
        mov eax, ebx
        shr eax, cl
        and eax, 7
        imul eax, 36
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
        mov [md_col9+ebx], cl
.s:
        inc ecx
        cmp ecx, 256
        jb .p
        inc ebx
        cmp ebx, 512
        jb .c
        xor eax, eax
.h:
        mov [col_host+eax], al
        inc eax
        cmp eax, 256
        jb .h
        call vdp_colores
        popad
        ret

; =============================================================================
;  El reloj: los tics del 8254 (1.193.182 por segundo) desde que se arrancó
; =============================================================================
; El tic de la BIOS (18,2 por segundo) es muy basto para 60 cuadros: se
; afina con lo que lleva contado el contador 0, que es el que lo produce. El
; contador puede estar en modo 3 (onda cuadrada: baja de dos en dos y da dos
; vueltas por tic) o en modo 2: se pregunta con la orden de «leer atrás».
; -> EAX (da la vuelta cada hora: solo valen las diferencias). En
; Hispa-Pentium, los tics del núcleo (int 80h, la 4), pasados a lo mismo
reloj:
        push ebx
        push ecx
        push edx
        cmp byte [pentium], 0
        jne .pent
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
.pent:
        mov eax, AJ_RELOJ           ; el reloj fino de HVPRUN (aj_leer 20, en
        HV aj_leer                  ; tics del 8254, como el de aquí)
        test eax, eax
        jnz .p1
        HV tics                     ; (un HVPRUN de antes: los de la BIOS)
        shl eax, 16
.p1:
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
        xor ebx, ebx                ; EBX = jugador 1, EDI = jugador 2
        xor edi, edi
        cmp byte [foco], 0
        je .mandos
        mov esi, [p_mapa]
        cmp byte [esi+0x38], 0      ; con Alt se va a los menús
        jne .mandos
        xor ecx, ecx
.t:
        movzx eax, byte [teclas+ecx]
        cmp byte [esi+eax], 0
        je .t1
        or bx, [tec_bot+ecx*2]
        mov byte [mapa_vale+ecx], 1 ; (esta tecla sí sale en el mapa)
.t1:
        cmp byte [blandas+ecx], 0   ; las que no salen en el mapa (en algunos
        je .t2                      ; equipos, las flechas; en Hispa-Pentium,
        dec byte [blandas+ecx]      ; todas): unos cuadros por cada vez que
        cmp byte [mapa_vale+ecx], 0 ; llega la tecla
        jne .t2
        or bx, [tec_bot+ecx*2]
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
        or ebx, eax
        movzx eax, byte [esi+HJD_J2E]
        call .uno
        or edi, eax
.fin:
        mov eax, ebx
        or eax, edi
        or [hubo_boton], al
        or [hubo_boton], ah
        mov eax, ebx                ; arriba y abajo a la vez, o izquierda y
        call .contra                ; derecha: ninguno
        call pon_pad
        mov eax, edi
        call .contra
        mov [pad_md+2], ax
        popad
        ret
.contra:
        mov edx, eax
        and edx, B_ARRIBA | B_ABAJO
        cmp edx, B_ARRIBA | B_ABAJO
        jne .c1
        and eax, ~(B_ARRIBA | B_ABAJO)
.c1:
        mov edx, eax
        and edx, B_IZQ | B_DER
        cmp edx, B_IZQ | B_DER
        jne .c2
        and eax, ~(B_IZQ | B_DER)
.c2:
        ret
; (interna) EAX=número del mando (FFh: no hay) -> EAX=sus botones (pad_md)
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
        test dl, 0x01               ; 1 A, 2 B, 3 C
        jz .b1
        or eax, B_A
.b1:
        test dl, 0x02
        jz .b2
        or eax, B_B
.b2:
        test dl, 0x04
        jz .b3
        or eax, B_C
.b3:
        cmp byte [edi+HJS_NBTN], 4
        ja .mas
        test dl, 0x08               ; con cuatro o menos: el 4, Start
        jz .u9
        or eax, B_START
        jmp .u9
.mas:
        test dl, 0x08               ; con más: 4 X, 5 Y, 6 Z, 9 Mode y Start
        jz .m1                      ; el 7, el 8 o el 10
        or eax, B_X
.m1:
        test dl, 0x10
        jz .m2
        or eax, B_Y
.m2:
        test dl, 0x20
        jz .m3
        or eax, B_Z
.m3:
        test edx, 0x100
        jz .m4
        or eax, B_MODE
.m4:
        test edx, 0x2C0
        jz .u9
        or eax, B_START
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
        call ym_frecuencia
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
        mov byte [rom_ok], 0
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
        call md_tam_buf
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
        call md_cargar
        call md_arrancar
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
        mov eax, 'SRM'
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
        mov byte [sram_usada], 1    ; (había partida: se guarda si cambia)
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
        call md_cuadro
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
        movzx eax, byte [region]
        shr eax, 6
        call hvl_num
        call .nl
        mov esi, k_imagen
        call hvl_cad
        mov esi, imagen
        mov ecx, 320 * 224
        call .suma
        call .nl
        mov esi, k_ram
        call hvl_cad
        mov esi, md_ram
        mov ecx, 0x10000
        call .suma
        call .nl
        mov esi, k_estado
        call hvl_cad
        movzx eax, byte [sram+4]
        call hvl_num
        call .nl
        mov esi, k_texto            ; lo que la ROM de prueba haya escrito
        call hvl_cad                ; (en FF0000h: «HDRV» y el texto; la RAM
        cmp dword [md_ram], 0x52564844 ; va por palabras al revés)
        jne .sintexto
        mov edx, 4
        mov ecx, 300
.t:
        mov eax, edx
        xor eax, 1
        mov al, [md_ram+eax]
        inc edx
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

%include "m68k.inc"
%include "m68k_gen.inc"
%include "vdp.inc"
%include "bus.inc"
%include "md.inc"
%include "ym.inc"
%include "ym_tablas.inc"
%include "z80.inc"
%include "psg.inc"
%include "hvplib.inc"
%include "emufull.inc"              ; [PANTALLA COMPLETA] compartido con Hispa-Nes
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
; [PANTALLA COMPLETA] estado y lo guardado de la ventana para devolverla
pantc       resd 1                  ; 1 = en pantalla completa
pc_n        resd 1                  ; la escala, para el escalado entero
pc_estilo   resd 1                  ; V_ESTILO de antes de entrar
pc_menu     resd 1                  ; su V_MENU
pc_x        resd 1                  ; y su colocación
pc_y        resd 1
pc_an       resd 1
pc_al       resd 1
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
mapa_vale   resb N_TECLAS
hubo_boton  resb 1
sram_pend   resb 1
blandas     resb N_TECLAS
col_host    resb 256                ; (la pantalla completa: aquí, igual)
nom_tmp     resb 80
dir_roms    resb 80                 ; la carpeta de los cartuchos (SMS)
dir_ult     resb 80                 ; la última de Abrir ROM
dir_ant     resb 80                 ; la de trabajo, mientras está el diálogo
dir_tmp     resb 80
ruta_ini    resb 80
txt_ini     resb 160
en_menu     resb 1                  ; un menú suyo está abierto
corriendo   resb 1                  ; la consola corría en la vuelta anterior
pentium     resb 1                  ; 1: en Hispa-Pentium (HVPRUN)
mando6      resb 1                  ; 1: el mando de seis botones
            alignb 2
joy_ant     resw 1                  ; (en pausa) los botones de los mandos
rom_ok      resb 1                  ; rom_abrir ha podido
tmp_puesto  resb 1                  ; el temporizador de mirar el mando en pausa
ruta_tmp    resb 160
rom_ruta    resb 160
sav_ruta    resb 160
ruta_log    resb 80
ruta_buzo   resb 96
txt_est     resb 160
txt_log     resb 512
            alignb 4
snd_buf     resb TROZO_MAX * 3 + 16
sram_copia  resb SRAM_TAM           ; lo que hay en el .SRM
sram_vista  resb SRAM_TAM           ; lo que había hace un segundo
pant        resb 960 * 672          ; (JUEGO_AN*FULL_MAX por JUEGO_AL*FULL_MAX)
HVP_FIN
