; =============================================================================
;  HISPASNS.HVP - Hispa-Snes, la consola de 16 bits de Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un emulador de la consola de 16 bits de 1990 (sus cartuchos, volcados en
;  archivos .SFC o .SMC). Todo el código es propio, escrito a partir de la
;  documentación pública del hardware (el 65C816, la PPU, el DMA, el SPC700
;  y el DSP del sonido): no lleva nada del fabricante de la consola ni de
;  otros emuladores, y las ROM comerciales no van en el proyecto (quien tenga
;  sus cartuchos usa sus volcados). Los cartuchos van en su carpeta, SNES,
;  al lado del módulo (Abrir ROM empieza en la última carpeta: HISPASNS.INI).
;  Al abrir un cartucho empieza en pausa, con un letrero, hasta una tecla, un
;  clic o un botón del mando; y se para sola (callada) con un menú abierto,
;  un diálogo, sin el foco o minimizada.
;
;  Las piezas (programas/hispasnes/):
;    cpu.inc      el 65C816
;    memoria.inc  leer y escribir su memoria
;    bus.inc      los registros, el DMA y el HDMA, los mandos y el reloj
;    ppu.inc      la imagen
;    spc.inc      el procesador del sonido (SPC700) y sus temporizadores
;    dsp.inc      el sonido: 8 voces, eco y ruido
;    mapas.inc    los cartuchos (LoROM, HiROM, ExHiROM) y su RAM
;    datos.inc    las variables
;  y aquí, lo que la une a Hispa Ventanas: la ventana y el menú, el reloj
;  (60 cuadros por segundo, 50 los de Europa; si el equipo no llega se deja
;  de pintar alguno, solo o con «Pintar 1 de cada N»), el teclado y los mandos
;  de HSPJOY, el sonido por HSPSND (en trozos de la medida de la tarjeta,
;  sin esperar nunca) y los archivos (la ROM y la partida guardada, .SRM).
;
;  También corre en Hispa-Pentium, con HVPRUN (docs/PENTIUM.md): allí no hay
;  modo real ni puertos (se mira con aj_leer: sin texto de versión es
;  HVPRUN), así que no busca HSPJOY, va sin sonido y toma el reloj fino de
;  HVPRUN (aj_leer 20, nuevo en HVPRUN para esto) en vez del 8254.
;
;  Teclas: flechas = cruceta, Z = B, X = A, A = Y, S = X, Q = L, W = R,
;  Intro = Start, Espacio = Select; P pone o quita la pausa y F2 reinicia.
;  Mandos (si está HSPJOY): el del jugador 1 y el del jugador 2.
;
;  Parámetros del acceso directo: el nombre de una ROM, que se abre al
;  entrar, y:
;    /1     tamaño x1 aunque la pantalla sea grande
;    /S=n   pintar 1 de cada n cuadros (1-4; si no, solo los que dé tiempo)
;  y para las pruebas:
;    /P     paso a paso: la consola solo avanza con cada tecla (seis cuadros
;           con el botón pulsado y seis sin él; Espacio, treinta sin botones)
;    /T=n   corre n cuadros a toda velocidad y apunta el resultado (tiempo,
;           suma de la imagen y de la memoria) en HISPASNS.LOG; con /V,
;           pintando cada cuadro en la ventana
; =============================================================================
%include "hvp.inc"

EST_AL      equ 18                  ; alto de la barra de estado
JUEGO_AN    equ 256                 ; [PANTALLA COMPLETA] el cuadro de la consola
JUEGO_AL    equ 224
FULL_MAX    equ 3                   ; escala entera máxima (la medida de «pant»)
TICS_SEG    equ 1193182             ; tics del reloj (el contador 8254) por segundo
TICS_NTSC   equ 19854               ; y por cuadro: 60,10 por segundo
TICS_PAL    equ 23860               ;   o 50,01 (los de Europa)
ROM_MAX     equ 6 * 1024 * 1024 + 512
ROM_MENOS   equ 0x8000              ; una ROM más pequeña no es de la consola
TROZO_MAX   equ 8192                ; octetos de un trozo de sonido, como mucho
AJ_RELOJ    equ 20                  ; (HVPRUN) aj_leer: el reloj fino
; los botones del mando, como los lee la consola ($4218)
B_B         equ 0x8000
B_Y         equ 0x4000
B_SELECT    equ 0x2000
B_START     equ 0x1000
B_ARRIBA    equ 0x0800
B_ABAJO     equ 0x0400
B_IZQ       equ 0x0200
B_DER       equ 0x0100
B_A         equ 0x0080
B_X         equ 0x0040
B_L         equ 0x0020
B_R         equ 0x0010
B_BOTONES   equ 0xF0F0              ; (lo que no es la cruceta)
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

HVP_CABECERA "Hispa-Snes", entrada
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "......00000000000000000000......"
        db "....008888888888888888888800...."
        db "...08888888888888888888888880..."
        db "..0888888888888888888888888880.."
        db ".088880088888888888888888888880."
        db ".088880088888888888888888cc8880."
        db ".088000000888888888888888cc8880."
        db ".088000000888888888888ee888ee80."
        db ".088880088888877888778ee888ee80."
        db ".088880088888888888888888998880."
        db ".088888888888888888888888998880."
        db "..0888888888880000088888888880.."
        db "...08888888880.....08888888880.."
        db "....000000000.......000000000..."
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
        db "................................"
HVP_CODIGO

c_snes      dd snes_proc
            db C_TINTA, CB_FOCO, 0, 0
plant:
        PLANTILLA c_snes, titulo, E_NORMAL, 120, 20, 256 + 2*BORDE, \
                  224 + EST_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

menu:
        db "&Archivo", 0
        dw 101
        db "&Abrir ROM...", 0
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
        db "&Imagen", 0
        dw 111
        db "Tamaño x&1", 0
        dw 112
        db "Tamaño x&2", 0
        dw 0
        db 0
        dw 113
pc_marca    db ' '                  ; [PANTALLA COMPLETA] la marca (* si activa)
        db " &Pantalla completa (Alt+Intro)", 0
        dw 0
        db 0
        dw 140
salto_m0    db '*'
        db " Pintar los que &dé tiempo", 0
        dw 141
salto_m1    db ' '
        db " Pintar &todos", 0
        dw 142
salto_m2    db ' '
        db " Pintar 1 de cada &2", 0
        dw 143
salto_m3    db ' '
        db " Pintar 1 de cada &3", 0
        dw 144
salto_m4    db ' '
        db " Pintar 1 de cada &4", 0
        dw 0xFFFF
        db "&Sonido", 0
        dw 121
        db "Poner o quitar el &sonido", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw 131
        db "&Teclas y mandos...", 0
        dw 132
        db "&Acerca de Hispa-Snes...", 0
        dw 0xFFFF
        db 0

titulo      db "Hispa-Snes", 0
t_abrir     db "Abrir ROM (.SFC, .SMC)", 0
t_sub       db "SNES", 0
t_clave     db "Carpeta=", 0
t_clave2    db "Saltar=", 0
t_letrero1  db "En pausa", 0
t_letrero2  db "Pulse una tecla o haga clic", 0
t_sin       db "Sin cartucho. Archivo, Abrir ROM...", 0
t_pausa     db "Pausa - ", 0
t_cs        db " c/s", 0
t_sinson    db "  sin sonido", 0
t_noabre    db "No se ha podido abrir el archivo.", 0
t_mala      db "Este archivo no es una ROM de la consola (.SFC o .SMC).", 0
t_nomem     db "No hay memoria para un cartucho tan grande.", 0
t_nosnd     db "No hay sonido: falta HSPSND o no ha encontrado tarjeta.", 0
t_nosnd2    db "Aquí no hay sonido todavía (Hispa-Pentium).", 0
t_chip1     db "Este cartucho lleva ", 0
t_chip2     db ",", 10, "que Hispa-Snes todavía no emula: puede que no", 10
            db "arranque o que se vea o suene mal.", 0
t_chip_dsp  db "un coprocesador matemático (DSP)", 0
t_chip_gsu  db "un procesador de gráficos (GSU)", 0
t_chip_obc  db "un chip para los objetos (OBC1)", 0
t_chip_sa1  db "un segundo procesador (SA-1)", 0
t_chip_sdd  db "un descompresor (S-DD1)", 0
t_chip_rtc  db "un reloj (S-RTC)", 0
t_chip_otro db "un chip especial", 0
        align 4
chip_nom    dd 0, t_chip_dsp, t_chip_gsu, t_chip_obc, t_chip_sa1, t_chip_sdd, t_chip_rtc, t_chip_otro
t_tec_tit   db "Teclas y mandos", 0
t_tec       db "Flechas: la cruceta.   Z: B.   X: A.   A: Y.   S: X.", 10
            db "Q: L.   W: R.   Intro: Start.   Espacio: Select.", 10
            db "P: pausa del emulador (o seguir).    F2: reiniciar.", 10
            db "En pausa, cualquier tecla, un clic o un botón del mando sigue.", 10
            db 10
            db "Con HSPJOY cargado valen los mandos del jugador 1 y del 2:", 10
            db "cruceta o palanca; botones 1 B, 2 A, 3 Y, 4 X, 5 L, 6 R,", 10
            db "7 o 9 Select, 8 o 10 Start (con 4 botones o menos: 3 Select", 10
            db "y 4 Start).", 0
t_ac_tit    db "Acerca de Hispa-Snes", 0
t_acerca    db "Hispa-Snes: emulador propio de la consola de 16 bits", 10
            db "de 1990, para los cartuchos volcados en archivos .SFC", 10
            db "o .SMC.", 10
            db 10
            db "Hecho desde la documentación pública del hardware. Las ROM", 10
            db "comerciales no van con Hispa-DOS: use los volcados de sus", 10
            db "propios cartuchos.", 10
            db 10
            db "LoROM, HiROM y ExHiROM. Partidas con pila: .SRM al lado.", 10
            db "Sin chips especiales todavía (GSU, SA-1, DSP...).", 0
k_cuadros   db "Cuadros=", 0
k_ms        db "Milisegundos=", 0
k_cps       db "CuadrosPorSegundo=", 0
k_modo      db "Modo=", 0
k_modo0     db "sin pintar", 0
k_modo1     db "ventana x", 0
k_imagen    db "Imagen=", 0
k_ram       db "RAM=", 0
k_titulo    db "Titulo=", 0
k_mapa      db "Mapa=", 0
k_texto     db "Texto=", 0

; teclas -> botones: código de la tecla y botón (16 bits)
        align 4
teclas      dw 0x2C, B_B,  0x2D, B_A,  0x1E, B_Y,  0x1F, B_X
            dw 0x10, B_L,  0x11, B_R,  0x1C, B_START,  0x39, B_SELECT
            dw 0x48, B_ARRIBA,  0x50, B_ABAJO,  0x4B, B_IZQ,  0x4D, B_DER
N_TECLAS    equ ($ - teclas) / 4
; (las letras, por WM_CHAR: en HVPRUN las teclas de letra llegan así)
letras_bot  db 'Z', 0,  'X', 1,  'A', 2,  'S', 3,  'Q', 4,  'W', 5,  ' ', 7
N_LETRAS    equ ($ - letras_bot) / 2

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        push edi
        mov edi, ruta_log           ; HISPASNS.LOG, al lado del módulo
        mov eax, 'LOG'
        call hvl_ruta
        mov edi, ruta_ini           ; HISPASNS.INI: la carpeta de Abrir ROM
        mov eax, 'INI'
        call hvl_ruta
        mov edi, dir_roms           ; la carpeta de los cartuchos: SNES,
        xor edx, edx                ; al lado del módulo
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
        mov esi, t_sub
        call hvl_cad
        mov byte [edi], 0
.c2:
        ; ¿Hispa Ventanas o HVPRUN (Hispa-Pentium)?
        mov eax, AJ_TXT_VERSION
        HV aj_leer
        test eax, eax
        setz [en_hvprun]
        call carpeta_lee            ; la última de Abrir ROM (HISPASNS.INI)
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
        mov al, 'S'
        call hvl_opcion
        jc .ss
        call hvl_lee_num
        cmp eax, 4
        ja .ss
        mov [salto], eax
        call salto_marca
.ss:
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
        cmp byte [en_hvprun], 0
        jne .sinreal
        call cero_busca
        call mandos_busca
.sinreal:
        call paleta_haz
        call ppu_tablas
        mov eax, [hv_api + HV_tec_mapa * 4]
        mov [p_mapa], eax
        mov eax, AJ_HAY_SONIDO
        HV aj_leer
        mov [snd_hay], al
        mov [snd_si], al
        cmp byte [en_hvprun], 0
        je .sh
        mov byte [snd_hay], 0
        mov byte [snd_si], 0
.sh:
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
        mov esi, nom_tmp
        cmp byte [esi], 0
        je .sinrom
        call rom_abrir
.sinrom:
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
        mov ecx, [tics_cuadro]
        lea ebx, [ecx*4+ecx]
        cmp edx, ebx
        jb .a
        mov [t_sig], eax            ; muy atrasado (un diálogo, el disco...):
        xor edx, edx                ; se empieza a contar otra vez
.a:
        add [t_sig], ecx
        mov byte [dibujar], 1
        cmp dword [salto], 0
        je .auto
        cmp dword [salto], 1
        je .p
        inc dword [salto_n]         ; 1 de cada N
        mov eax, [salto_n]
        cmp eax, [salto]
        jb .no
        mov dword [salto_n], 0
        jmp .p
.auto:
        cmp edx, ecx                ; va con retraso: este cuadro no se pinta
        jb .p                       ; (tres seguidos como mucho)
        cmp byte [saltados], 3
        jae .p
        inc byte [saltados]
.no:
        mov byte [dibujar], 0
        jmp .q
.p:
        mov byte [saltados], 0
.q:
        call mandos_lee
        call snes_cuadro
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
        imul edx, eax, 224
        add edx, EST_AL + 2*BORDE + TIT_H + 1 + MENU_H
        ret

; las órdenes de pintar del menú «Imagen»: la marca en la que está puesta
salto_marca:
        push eax
        push ecx
        mov byte [salto_m0], ' '
        mov byte [salto_m1], ' '
        mov byte [salto_m2], ' '
        mov byte [salto_m3], ' '
        mov byte [salto_m4], ' '
        mov eax, [salto]
        mov ecx, [salto_marcas+eax*4]
        mov byte [ecx], '*'
        pop ecx
        pop eax
        ret
        align 4
salto_marcas dd salto_m0, salto_m1, salto_m2, salto_m3, salto_m4

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
snes_proc:
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
        mov eax, ecx
        mov esi, teclas
        mov ecx, N_TECLAS
.t1:
        cmp ax, [esi]
        je .t2
        add esi, 4
        dec ecx
        jnz .t1
        jmp hecho
.t2:
        movzx eax, word [esi+2]
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
        cmp byte [a_pasos], 0       ; (paso a paso: Espacio, treinta cuadros)
        je .l1
        cmp al, ' '
        jne .l1
        xor eax, eax
        mov ecx, 30
        xor edx, edx
        call pasos
        jmp hecho
.l1:
        mov esi, letras_bot
        mov ecx, N_LETRAS
.l2:
        cmp al, [esi]
        je .l3
        add esi, 2
        dec ecx
        jnz .l2
        jmp hecho
.l3:
        movzx eax, byte [esi+1]     ; el número de la tecla en «teclas»
        movzx eax, word [teclas+eax*4+2]
; una tecla de la consola por mensaje: AX=botón
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
        cmp ax, [teclas+ecx*4+2]
        jne .b2
        mov byte [blandas+ecx], 8
.b2:
        dec ecx
        jns .b1
        jmp hecho
.orden:
        cmp ecx, 101
        jne .o3
        call sram_guarda
        call abrir_dlg
        jmp hecho
.o3:
        cmp ecx, 103
        jne .o4
.reinicia:
        cmp byte [cargada], 0
        je hecho
        call snes_reiniciar
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
        jne .o14
        cmp byte [snd_hay], 0
        jne .s1
        mov esi, t_nosnd
        cmp byte [en_hvprun], 0
        je .s0
        mov esi, t_nosnd2
.s0:
        mov edi, titulo
        jmp .dlg
.s1:
        xor byte [snd_si], 1
        call snd_ajusta
        jmp .estado
.o14:
        cmp ecx, 140                ; Imagen: cuántos cuadros se pintan
        jb .o31
        cmp ecx, 144
        ja .o31
        sub ecx, 140
        mov [salto], ecx
        mov dword [salto_n], 0
        call salto_marca
        call carpeta_guarda
        jmp hecho
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

; -> AX=los botones (no la cruceta) de los dos mandos de HSPJOY, juntos
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
        mov eax, [escala]
        shl eax, 8
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
        mov [snd_sal_n], eax
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
;  Las carpetas: los cartuchos van en SNES, al lado del módulo; Abrir ROM
;  empieza en la última carpeta de la que se abrió uno (apuntada en el .INI)
; =============================================================================
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

; del .INI: dir_ult («Carpeta=»; si no hay, la de los cartuchos) y [salto]
; («Saltar=»)
carpeta_lee:
        pushad
        mov esi, dir_roms
        mov edi, dir_ult
        call cad0
        mov esi, ruta_ini
        mov edi, txt_ini
        mov ecx, 200
        call hvl_leer
        test eax, eax
        jz .x
        mov byte [txt_ini+eax], 0
        mov esi, txt_ini
        mov edi, t_clave2
        call hvl_clave
        jc .c0
        call hvl_lee_num
        cmp eax, 4
        ja .c0
        mov [salto], eax
        call salto_marca
.c0:
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

; apunta dir_ult y [salto] en el .INI
carpeta_guarda:
        pushad
        mov edi, txt_ini
        mov esi, t_clave
        call hvl_cad
        mov esi, dir_ult
        call hvl_cad
        mov ax, 0x0A0D
        stosw
        mov esi, t_clave2
        call hvl_cad
        mov eax, [salto]
        call hvl_num
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
        mov [pad], ax
.a:
        test ecx, ecx
        jz .b
        call snes_cuadro
        dec ecx
        jmp .a
.b:
        mov word [pad], 0
.c:
        test edx, edx
        jz .d
        call snes_cuadro
        dec edx
        jmp .c
.d:
        mov dword [snd_sal_n], 0
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
        cmp ecx, 512
        jb .e
        cmp edx, 448
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

; (interna, con el contexto abierto) «imagen» (ya con los colores de Hispa
; Ventanas), al doble si toca, a la ventana
img_copia:
        cmp byte [pantc], 0         ; en pantalla completa, el escalado entero
        jne img_copia_full
        pushad
        cmp dword [escala], 2
        je .doble
        mov eax, [img_x]
        mov ebx, [img_y]
        mov ecx, 256
        mov edx, 224
        mov esi, imagen
        HV dc_copiar
        popad
        ret
.doble:
        mov esi, imagen
        mov edi, pant
        mov ebx, 224
.fila:
        mov ecx, 128
.d:
        movzx eax, byte [esi]       ; dos puntos, cada uno dos veces
        mov ah, al
        movzx edx, byte [esi+1]
        mov dh, dl
        shl edx, 16
        or eax, edx
        mov [edi], eax
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
        mov eax, [img_x]
        mov ebx, [img_y]
        mov ecx, 512
        mov edx, 448
        mov esi, pant
        HV dc_copiar
        popad
        ret

; lut15: cada color de 15 bits de la consola, al más parecido de los 256 de
; Hispa Ventanas (se busca para 4 bits por componente, 4.096, y se reparte);
; y col_host, la identidad (para lib/emufull.inc)
paleta_haz:
        pushad
        mov ebp, [hv_api + HV_pal_rgb * 4]
        xor ebx, ebx                ; EBX = el color de 12 bits
.c:
        mov dword [mejor_d], 0x7FFFFFFF
        xor ecx, ecx
.p:
        xor edi, edi                ; la distancia: suma de los cuadrados
        xor esi, esi
.k:
        push ecx
        lea ecx, [esi*4]
        mov eax, ebx
        shr eax, cl
        and eax, 15
        imul eax, 17
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
        mov [lut12+ebx], cl
.s:
        inc ecx
        cmp ecx, 256
        jb .p
        inc ebx
        cmp ebx, 4096
        jb .c
        ; los 32.768: (rojo 0-4, verde 5-9, azul 10-14) -> 4 bits cada uno
        xor ebx, ebx
.r:
        mov eax, ebx
        and eax, 0x1F
        shr eax, 1
        mov edx, ebx
        shr edx, 6
        and edx, 0x0F
        shl edx, 4
        or eax, edx
        mov edx, ebx
        shr edx, 11
        and edx, 0x0F
        shl edx, 8
        or eax, edx
        mov al, [lut12+eax]
        mov [lut15+ebx], al
        inc ebx
        cmp ebx, 32768
        jb .r
        xor eax, eax
.i:
        mov [col_host+eax], al
        inc eax
        cmp eax, 256
        jb .i
        popad
        ret

; =============================================================================
;  El reloj: los tics del 8254 (1.193.182 por segundo) desde que se arrancó
; =============================================================================
; El tic de la BIOS (18,2 por segundo) es muy basto para 60 cuadros: se
; afina con lo que lleva contado el contador 0, que es el que lo produce. El
; contador puede estar en modo 3 (onda cuadrada: baja de dos en dos y da dos
; vueltas por tic) o en modo 2: se pregunta con la orden de «leer atrás».
; En HVPRUN no hay puertos: el reloj fino de HVPRUN (aj_leer 20; si no lo
; tiene, los tics de 18,2 por segundo).
; -> EAX (da la vuelta cada hora: solo valen las diferencias)
reloj:
        cmp byte [en_hvprun], 0
        jne .hvprun
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
.hvprun:
        mov eax, AJ_RELOJ
        HV aj_leer
        test eax, eax
        jnz .h1
        HV tics                     ; (un HVPRUN de antes: los tics de la BIOS)
        shl eax, 16
.h1:
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
        cmp word [hubo_boton], 0    ; se está jugando (quizá solo con el
        je .x                       ; mando): que no salte el salvapantallas
        mov word [hubo_boton], 0
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
        mov [snd_sal_n], eax
        mov dword [cps], 60
        mov dword [cps_pint], 60
        popad
        ret

; =============================================================================
;  Los mandos: teclado y HSPJOY
; =============================================================================
mandos_lee:
        pushad
        xor ebx, ebx                ; EBX = jugador 1, EBP = jugador 2
        xor ebp, ebp
        cmp byte [foco], 0
        je .mandos
        mov esi, [p_mapa]
        cmp byte [esi+0x38], 0      ; con Alt se va a los menús
        jne .mandos
        xor ecx, ecx
.t:
        movzx eax, word [teclas+ecx*4]
        cmp byte [esi+eax], 0
        je .t1
        or bx, [teclas+ecx*4+2]
        mov byte [mapa_vale], 1
.t1:
        cmp byte [blandas+ecx], 0
        je .t2
        dec byte [blandas+ecx]
        cmp byte [mapa_vale], 0
        jne .t2
        or bx, [teclas+ecx*4+2]
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
        or ebp, eax
.fin:
        mov eax, ebx
        or eax, ebp
        or [hubo_boton], ax
        mov eax, ebx                ; arriba y abajo a la vez, o izquierda y
        call .contra                ; derecha: ninguno
        mov [pad], ax
        mov eax, ebp
        call .contra
        mov [pad+2], ax
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
; (interna) EAX=número del mando (FFh: no hay) -> EAX=sus botones
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
        test cl, 1
        jz .k1
        or eax, B_ARRIBA
.k1:
        test cl, 2
        jz .k2
        or eax, B_ABAJO
.k2:
        test cl, 4
        jz .k3
        or eax, B_IZQ
.k3:
        test cl, 8
        jz .k4
        or eax, B_DER
.k4:
        cmp byte [edi+HJS_EJES], 64 ; la palanca
        ja .e1
        or eax, B_IZQ
.e1:
        cmp byte [edi+HJS_EJES], 192
        jb .e2
        or eax, B_DER
.e2:
        cmp byte [edi+HJS_EJES+1], 64
        ja .e3
        or eax, B_ARRIBA
.e3:
        cmp byte [edi+HJS_EJES+1], 192
        jb .e4
        or eax, B_ABAJO
.e4:
        movzx edx, word [edi+HJS_BTN]
        cmp byte [edi+HJS_NBTN], 4
        ja .mas
        mov ecx, 4                  ; 4 o menos: B, A, Select, Start
        mov edi, botones4
        jmp .b
.mas:
        mov ecx, 10                 ; más: B A Y X L R Select Start Select Start
        mov edi, botones10
.b:
        shr edx, 1
        jnc .b1
        or ax, [edi]
.b1:
        add edi, 2
        dec ecx
        jnz .b
        jmp .u9
.u0:
        xor eax, eax
.u9:
        pop edi
        pop edx
        pop ecx
        ret
botones4    dw B_B, B_A, B_SELECT, B_START
botones10   dw B_B, B_A, B_Y, B_X, B_L, B_R, B_SELECT, B_START, B_SELECT, B_START

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
;  El sonido: las muestras del DSP, a HSPSND
; =============================================================================
; El núcleo deja en snd_sal las muestras del cuadro (16 bits con signo, en
; estéreo si la tarjeta lo es, a [snd_hz] por segundo). Aquí se pasan al
; formato de la tarjeta y se juntan hasta tener un trozo de la medida que la
; tarjeta toca de una vez: 2.048 tramas de 16 bits en estéreo a 48.000 Hz con
; HD Audio y AC'97, 1.024 de 16 bits y un canal a 22.050 Hz con una Sound
; Blaster 16 y 4.096 muestras de 8 bits con las anteriores. Antes de
; mandarlo se pregunta a HSPSND si cabe sin esperar (su función 08h): si no
; cabe, se queda para la vuelta siguiente, y si se juntan más de dos
; trozos, lo más viejo se tira. La ventana nunca espera.
sonido_manda:
        pushad
        mov ecx, [snd_sal_n]
        mov dword [snd_sal_n], 0
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
        mov esi, snd_sal
        mov edi, snd_buf
        add edi, [snd_n]
        mov al, [snd_fmt]
        cmp al, 3
        je .est
        cmp al, 1
        je .m16
.c8:                                ; 8 bits sin signo
        mov dx, [esi]
        add esi, 2
        mov dl, dh
        add dl, 0x80
        mov [edi], dl
        inc edi
        dec ecx
        jnz .c8
        jmp .hecho
.m16:
        shl ecx, 1                  ; 16 bits, un canal
        rep movsb
        jmp .hecho
.est:
        shl ecx, 2                  ; 16 bits, estéreo
        rep movsb
.hecho:
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
.reloj:                             ; un HSPSND anterior: por el reloj (sin
        call reloj                  ; más de tres sonando)
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
        mov [snd_hz_sal], eax
        xor eax, eax
        cmp byte [snd_fmt], 3
        sete al
        mov [snd_estereo], al
        mov dword [snd_sal_n], 0
        mov dword [snd_n], 0
        popad
        ret

; =============================================================================
;  Los archivos: la ROM y la partida guardada
; =============================================================================
; ESI=nombre de la ROM: la abre y enciende la consola. Si no puede, lo dice
; y sigue lo que había
rom_abrir:
        pushad
        mov byte [rom_ok], 0
        mov edi, ruta_tmp           ; la ruta entera (el .SRM va al lado y la
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
        call snes_tam_buf
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
        call cart_cargar
        mov eax, TICS_NTSC
        cmp dword [cart_pal], 0
        je .tc
        mov eax, TICS_PAL
.tc:
        mov [tics_cuadro], eax
        call snes_arrancar
        mov esi, ruta_tmp           ; su nombre (sin la carpeta) y su .SRM
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
        ; un chip que no se emula: se avisa
        mov eax, [cart_chip]
        test eax, eax
        jz .sinchip
        mov edi, txt_log
        mov esi, t_chip1
        call hvl_cad
        mov esi, [chip_nom+eax*4]
        call hvl_cad
        mov esi, t_chip2
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        mov esi, txt_log
        mov edi, titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
.sinchip:
        xor eax, eax                ; un segundo para arrancar (el título)
        mov ecx, 60
        xor edx, edx
        call pasos
        cmp byte [a_pasos], 0
        jne .x
        mov word [joy_ant], 0       ; y en pausa, hasta una tecla, un clic o
        mov al, 1                   ; un botón del mando
        call pausa_pon
        mov word [joy_ant], 0       ; (un botón ya apretado también vale)
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

; la partida guardada, a la RAM del cartucho (si hay archivo)
sram_lee:
        pushad
        mov byte [sram_pend], 0
        mov dword [sram_seg], 0
        mov ecx, [sram_tam]
        test ecx, ecx
        jz .x
        mov esi, sav_ruta
        xor al, al
        HV dos_abrir
        jc .x
        mov ebx, eax
        mov edi, sram
        mov ecx, [sram_tam]
        HV dos_leer
        HV dos_cerrar
.x:
        mov esi, sram
        mov edi, sram_copia
        mov ecx, SRAM_MAX / 4
        rep movsd
        mov esi, sram
        mov edi, sram_vista
        mov ecx, SRAM_MAX / 4
        rep movsd
        popad
        ret

; la guarda si el cartucho tiene RAM con pila y ha cambiado desde la
; última vez
sram_guarda:
        pushad
        mov byte [sram_pend], 0
        mov dword [sram_seg], 0
        cmp byte [cargada], 0
        je .x
        mov ecx, [sram_tam]
        test ecx, ecx
        jz .x
        shr ecx, 2
        mov esi, sram
        mov edi, sram_copia
        repe cmpsd
        je .x
        mov ecx, [sram_tam]
        shr ecx, 2
        mov esi, sram
        mov edi, sram_copia
        rep movsd
        mov esi, sav_ruta
        mov edx, sram
        mov ecx, [sram_tam]
        call hvl_escribir
.x:
        popad
        ret

; (cada segundo) se guarda sola dos segundos después del último cambio, o a
; los treinta si el juego no para de escribir
sram_vigila:
        cmp dword [sram_tam], 0
        je .x
        pushad
        mov ecx, [sram_tam]
        shr ecx, 2
        mov esi, sram               ; ¿ha cambiado en este segundo?
        mov edi, sram_vista
        repe cmpsd
        je .quieta
        mov ecx, [sram_tam]
        shr ecx, 2
        mov esi, sram
        mov edi, sram_vista
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
        mov word [pad], 0
        call snes_cuadro
        mov dword [snd_sal_n], 0
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
        mov esi, k_titulo
        call hvl_cad
        mov esi, cart_titulo
        call hvl_cad
        call .nl
        mov esi, k_mapa
        call hvl_cad
        mov eax, [cart_mapa]
        call hvl_num
        call .nl
        mov esi, k_imagen
        call hvl_cad
        mov esi, imagen
        mov ecx, 256 * 224
        call .suma
        call .nl
        mov esi, k_ram
        call hvl_cad
        mov esi, wram
        mov ecx, 0x20000
        call .suma
        call .nl
        mov esi, k_texto            ; lo que la ROM de prueba haya escrito
        call hvl_cad                ; en su RAM («HSN» y el texto)
        cmp dword [sram], 0x4E5348
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
%include "memoria.inc"
%include "bus.inc"
%include "ppu.inc"
%include "spc.inc"
%include "dsp.inc"
%include "mapas.inc"
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
tics_cuadro resd 1                  ; tics del reloj por cuadro (NTSC o PAL)
salto       resd 1                  ; 0: los que dé tiempo; N: 1 de cada N
salto_n     resd 1
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
joy_ant     resw 1                  ; (en pausa) los botones de los mandos
hubo_boton  resw 1
a_pasos     resb 1
a_ventana   resb 1
a_uno       resb 1
cargada     resb 1
pausa       resb 1
foco        resb 1
saltados    resb 1
snd_hay     resb 1
snd_si      resb 1
snd_fmt     resb 1                  ; 0: 8 bits, 1: 16 bits, 3: 16 bits estéreo
joy_opc     resb 1
mapa_vale   resb 1
sram_pend   resb 1
en_hvprun   resb 1                  ; 1: dentro de HVPRUN (Hispa-Pentium)
en_menu     resb 1                  ; un menú suyo está abierto
corriendo   resb 1                  ; la consola corría en la vuelta anterior
rom_ok      resb 1                  ; rom_abrir ha podido
tmp_puesto  resb 1                  ; el temporizador de mirar el mando en pausa
blandas     resb N_TECLAS
col_host    resb 256                ; (la identidad: «imagen» ya va en sus colores)
nom_tmp     resb 80
dir_roms    resb 80                 ; la carpeta de los cartuchos (SNES)
dir_ult     resb 80                 ; la última de Abrir ROM
dir_ant     resb 80                 ; la de trabajo, mientras está el diálogo
dir_tmp     resb 80
ruta_ini    resb 80
txt_ini     resb 220
ruta_tmp    resb 160
rom_ruta    resb 160
sav_ruta    resb 160
ruta_log    resb 80
txt_est     resb 160
txt_log     resb 512
            alignb 4
lut12       resb 4096
snd_buf     resb TROZO_MAX * 3 + 16
sram_copia  resb SRAM_MAX           ; lo que hay en el .SRM
sram_vista  resb SRAM_MAX           ; lo que había hace un segundo
pant        resb 768 * 672          ; (JUEGO_AN*FULL_MAX por JUEGO_AL*FULL_MAX)
HVP_FIN
