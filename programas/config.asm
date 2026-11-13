; =============================================================================
;  CONFIG.HVP - Configuración de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Los ajustes del entorno, en una ventana con los apartados a la izquierda y
;  el apartado elegido a la derecha:
;    Pantalla       tema, dibujo y colores del fondo del escritorio, y un
;                   salvapantallas con su tiempo de espera
;    Ratón          velocidad del puntero, tiempo del doble clic (con una
;                   zona para probarlo) y botones intercambiados
;    Teclado        espera y velocidad de repetición (INT 16h, función 03h) y
;                   distribución española o estadounidense (se le pide al
;                   residente TECLADO, KEYB: INT 2Fh AX=AD80h)
;    Sonido         volumen y silencio (los mismos del altavoz de la barra)
;    Fecha y hora   las enseña y las cambia (INT 21h, 2Ah-2Dh)
;    Sistema        versión, procesador, memoria, pantalla, ratón, sonido
;  Lo que se cambia se aplica al momento y se guarda en VENTANAS.INI (al lado
;  de CONFIG.HVP; texto, formato en docs/VENTANAS.md): con «Guardar», al
;  cerrar la ventana o, solo, unos segundos después del último cambio.
;
;  Hispa Ventanas carga este programa al arrancar con el parámetro «/A»
;  (ventanas/ajustes.inc): lee VENTANAS.INI, aplica los ajustes y se queda en
;  marcha con la ventana oculta, para el salvapantallas; abrir el acceso
;  directo «Configuración» enseña la ventana y cerrarla la vuelve a ocultar.
;
;  Todo está hecho aquí: los controles propios (el selector de apartados, de
;  colores y de dibujos, el deslizador con su rombo, la zona de prueba del
;  doble clic), los dibujos del fondo, el salvapantallas, el icono y los
;  textos. Es un módulo .HVP (ventanas/hvp.inc).
; =============================================================================
%include "hvp.inc"

CLI_AN      equ 548                 ; la zona de la ventana
CLI_AL      equ 376
PX          equ 152                 ; donde empieza el apartado
PAN_AN      equ 388
SEC_AL      equ 34                  ; alto de cada apartado en el selector
N_SEC       equ 6
N_DIB       equ 10                  ; dibujos del fondo
N_TEMAS     equ 8
ID_SEC      equ 10
ID_FABRICA  equ 11
ID_GUARDAR  equ 12
ID_CERRAR   equ 13
ID_ESTADO   equ 14
ID_PAN      equ 100                 ; los controles del apartado: 100-159
ID_FIJO     equ 159                 ; (un texto que no cambia)
WM_MONTAR   equ WM_USER + 60
LOGO_AN     equ 176                 ; el letrero del salvapantallas
LOGO_AL     equ 44
INI_MAX     equ 2048
; tipos de las claves de VENTANAS.INI
T_NUM       equ 0
T_DIB       equ 1                   ; nombre de un dibujo
T_MS55      equ 2                   ; milisegundos <-> tics de reloj
T_MS250     equ 3                   ; milisegundos <-> 0-3 (250 ms cada uno)
T_DIST      equ 4                   ; SP o US

HVP_CABECERA "Configuración", entrada
        db "................................"
        db "................................"
        db "..1111111111111111111111111111.."
        db ".188888888888888888888888888881."
        db ".188888888888888888888888888881."
        db ".188888884888888888888888888881."
        db ".188888844488888888888888888881."
        db ".187777444447777777777777777781."
        db ".188888844488888888888888888881."
        db ".188888884888888888888888888881."
        db ".188888888888888888888888888881."
        db ".188888888888888888888888888881."
        db ".188888888888888888c88888888881."
        db ".18888888888888888ccc8888888881."
        db ".1877777777777777ccccc777777781."
        db ".18888888888888888ccc8888888881."
        db ".188888888888888888c88888888881."
        db ".188888888888888888888888888881."
        db ".188888888888888888888888888881."
        db ".188888888888a88888888888888881."
        db ".18888888888aaa8888888888888881."
        db ".1877777777aaaaa777777777777781."
        db ".18888888888aaa8888888888888881."
        db ".188888888888a88888888888888881."
        db ".188888888888888888888888888881."
        db ".188888888888888888888888888881."
        db "..1111111111111111111111111111.."
        db "...00000000000000000000000000..."
        db "................................"
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

; ---- clases ----
c_config    dd cfg_proc
            db C_CREMA, 0, 0, 0
c_selector  dd sel_proc
            db C_CREMA, CB_FOCO, 0, 0
c_desliza   dd des_proc
            db C_CREMA, CB_FOCO, 0, 0
c_prueba    dd pru_proc
            db C_BLANCO, 0, 0, 0
c_salva     dd sal_proc
            db C_TINTA, 0, 0, 0

%define EST HV_c_estatico
%define BOT HV_c_boton
%define EDT HV_c_edit
%define LIS HV_c_lista
H           equ E_HIJA
HT          equ E_HIJA | E_TABULA

cfg_plant:
        PLANTILLA c_config, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 0, 0, \
                  CLI_AN + 2*BORDE, CLI_AL + 2*BORDE + TIT_H + 1, 0

; lo que está siempre: los apartados, el estado y los botones de abajo
pl_fijos:
        PLANTILLA c_selector, 0, HT, 8, 8, 132, N_SEC * SEC_AL, ID_SEC
        PLANTILLA EST, t_est0, H, 8, 348, 246, 18, ID_ESTADO
        PLANTILLA BOT, t_fabrica, HT, 258, 344, 92, 24, ID_FABRICA
        PLANTILLA BOT, t_guardar, HT, 358, 344, 86, 24, ID_GUARDAR
        PLANTILLA BOT, t_cerrar, HT, 452, 344, 88, 24, ID_CERRAR
        dd 0

pl_pantalla:
        PLANTILLA EST, t_tema, H, PX, 40, 134, 18, ID_FIJO
        PLANTILLA LIS, 0, HT, PX, 60, 134, 84, 100
        PLANTILLA EST, l_dib, H, PX + 146, 40, 242, 18, 108
        PLANTILLA c_selector, 0, HT, PX + 146, 60, 240, 84, 101
        PLANTILLA EST, l_cf, H, PX, 154, PAN_AN, 18, 109
        PLANTILLA c_selector, 0, HT, PX, 174, 352, 24, 102
        PLANTILLA EST, l_cd, H, PX, 204, PAN_AN, 18, 110
        PLANTILLA c_selector, 0, HT, PX, 224, 352, 24, 103
        PLANTILLA BOT, t_salva, HT | BS_CASILLA, PX, 262, 176, 20, 104
        PLANTILLA c_desliza, 0, HT, PX + 178, 262, 124, 21, 105
        PLANTILLA EST, l_min, H, PX + 306, 264, 82, 18, 106
        PLANTILLA BOT, t_probar, HT, PX, 292, 80, 24, 107
        PLANTILLA EST, t_salva_nota, H, PX + 90, 296, 298, 18, ID_FIJO
        dd 0
; los selectores: id, cuántos, columnas, ancho y alto de la casilla, clase
; de dibujo (0 colores, 1 dibujos del fondo, 2 apartados)
sel_sec     dd ID_SEC, N_SEC, 1, 132, SEC_AL, 2
sel_dib     dd 101, N_DIB, 5, 48, 42, 1
sel_cf      dd 102, 16, 16, 22, 24, 0
sel_cd      dd 103, 16, 16, 22, 24, 0

pl_raton:
        PLANTILLA EST, t_r_vel, H, PX, 40, PAN_AN, 18, ID_FIJO
        PLANTILLA EST, t_lenta, H, PX, 64, 48, 18, ID_FIJO
        PLANTILLA c_desliza, 0, HT, PX + 50, 62, 190, 21, 110
        PLANTILLA EST, t_rapida, H, PX + 246, 64, 56, 18, ID_FIJO
        PLANTILLA EST, l_vel, H, PX + 306, 64, 82, 18, 111
        PLANTILLA EST, t_r_dbl, H, PX, 104, PAN_AN, 18, ID_FIJO
        PLANTILLA EST, t_corto, H, PX, 128, 48, 18, ID_FIJO
        PLANTILLA c_desliza, 0, HT, PX + 50, 126, 190, 21, 112
        PLANTILLA EST, t_largo, H, PX + 246, 128, 56, 18, ID_FIJO
        PLANTILLA EST, l_dbl, H, PX + 306, 128, 82, 18, 113
        PLANTILLA EST, t_r_prueba, H, PX, 172, 210, 18, ID_FIJO
        PLANTILLA c_prueba, 0, H, PX + 216, 160, 64, 42, 114
        PLANTILLA BOT, t_zurdo, HT | BS_CASILLA, PX, 222, 380, 20, 115
        PLANTILLA EST, t_zurdo_nota, H, PX, 248, PAN_AN, 40, ID_FIJO
        dd 0

pl_teclado:
        PLANTILLA EST, t_t_esp, H, PX, 40, PAN_AN, 18, ID_FIJO
        PLANTILLA EST, t_corta, H, PX, 64, 48, 18, ID_FIJO
        PLANTILLA c_desliza, 0, HT, PX + 50, 62, 190, 21, 120
        PLANTILLA EST, t_larga, H, PX + 246, 64, 56, 18, ID_FIJO
        PLANTILLA EST, l_esp, H, PX + 306, 64, 82, 18, 121
        PLANTILLA EST, t_t_rep, H, PX, 104, PAN_AN, 18, ID_FIJO
        PLANTILLA EST, t_lenta, H, PX, 128, 48, 18, ID_FIJO
        PLANTILLA c_desliza, 0, HT, PX + 50, 126, 190, 21, 122
        PLANTILLA EST, t_rapida, H, PX + 246, 128, 56, 18, ID_FIJO
        PLANTILLA EST, l_rep, H, PX + 306, 128, 82, 18, 123
        PLANTILLA EST, t_t_prueba, H, PX, 164, PAN_AN, 18, ID_FIJO
        PLANTILLA EDT, 0, HT, PX, 184, 380, 22, 124
        PLANTILLA EST, t_t_dist, H, PX, 226, PAN_AN, 18, ID_FIJO
        dd 0
pl_tecl_si:
        PLANTILLA BOT, t_espanola, HT | BS_OPCION, PX, 248, 130, 20, 125
        PLANTILLA BOT, t_eeuu, HT | BS_OPCION, PX + 140, 248, 170, 20, 126
        PLANTILLA EST, t_t_si, H, PX, 276, PAN_AN, 18, ID_FIJO
        dd 0
pl_tecl_no:
        PLANTILLA EST, t_t_no, H, PX, 248, PAN_AN, 40, ID_FIJO
        dd 0

pl_sonido:
        PLANTILLA EST, t_s_vol, H, PX, 40, PAN_AN, 18, ID_FIJO
        PLANTILLA c_desliza, 0, HT, PX, 62, 300, 21, 130
        PLANTILLA EST, l_vol, H, PX + 308, 64, 80, 18, 131
        PLANTILLA BOT, t_silencio, HT | BS_CASILLA, PX, 100, 150, 20, 132
        PLANTILLA EST, t_s_nota, H, PX, 140, PAN_AN, 40, ID_FIJO
        dd 0
pl_sonido_no:
        PLANTILLA EST, t_s_no, H, PX, 40, PAN_AN, 40, ID_FIJO
        dd 0

pl_fecha:
        PLANTILLA EST, l_fecha, H | SS_NEGRITA, PX, 40, PAN_AN, 18, 140
        PLANTILLA EST, l_hora, H | SS_NEGRITA, PX, 62, 120, 18, 141
        PLANTILLA EST, t_f_fecha, H, PX, 100, PAN_AN, 18, ID_FIJO
        PLANTILLA EST, t_f_dia, H, PX, 126, 36, 18, ID_FIJO
        PLANTILLA EDT, 0, HT, PX + 40, 122, 40, 22, 142
        PLANTILLA EST, t_f_mes, H, PX + 96, 126, 36, 18, ID_FIJO
        PLANTILLA EDT, 0, HT, PX + 136, 122, 40, 22, 143
        PLANTILLA EST, t_f_ano, H, PX + 192, 126, 36, 18, ID_FIJO
        PLANTILLA EDT, 0, HT, PX + 232, 122, 56, 22, 144
        PLANTILLA EST, t_f_hora, H, PX, 162, PAN_AN, 18, ID_FIJO
        PLANTILLA EST, t_f_h, H, PX, 188, 44, 18, ID_FIJO
        PLANTILLA EDT, 0, HT, PX + 48, 184, 40, 22, 145
        PLANTILLA EST, t_f_m, H, PX + 104, 188, 60, 18, ID_FIJO
        PLANTILLA EDT, 0, HT, PX + 168, 184, 40, 22, 146
        PLANTILLA EST, t_f_s, H, PX + 224, 188, 68, 18, ID_FIJO
        PLANTILLA EDT, 0, HT, PX + 296, 184, 40, 22, 147
        PLANTILLA BOT, t_f_poner, HT, PX, 226, 170, 24, 148
        PLANTILLA EST, l_res, H, PX + 180, 230, 208, 18, 149
        PLANTILLA EST, t_f_nota, H, PX, 272, PAN_AN, 18, ID_FIJO
        dd 0

pl_sistema:
        PLANTILLA EST, sis_txt, H, PX, 40, PAN_AN, 290, 150
        dd 0

listas      dd pl_fijos, pl_pantalla, pl_raton, pl_teclado, pl_tecl_si, pl_tecl_no
            dd pl_sonido, pl_sonido_no, pl_fecha, pl_sistema, 0
paneles     dd pan_pantalla, pan_raton, pan_teclado, pan_sonido, pan_fecha, pan_sistema
sel_pintores dd pin_color, pin_dibujo, pin_seccion
dib_tabla   dd dib_rombos, dib_liso, dib_puntos, dib_rayas, dib_cuadros
            dd dib_ladrillos, dib_olas, dib_damero, dib_trama, dib_zigzag

; ---- VENTANAS.INI: clave, variable, mínimo, máximo, tipo, sección que
; empieza con ella (0: sigue la misma). Con -1 en la variable no se escribe.
ini_tabla:
        dd k_dibujo, a_dibujo, 0, N_DIB - 1, T_DIB, s_pantalla
        dd k_cfondo, a_cfondo, 0, 15, T_NUM, 0
        dd k_cdibujo, a_cdibujo, 0, 15, T_NUM, 0
        dd k_salva, a_salva, 0, 1, T_NUM, 0
        dd k_minutos, a_minutos, 1, 60, T_NUM, 0
        dd k_vel, a_vel, 1, 6, T_NUM, s_raton
        dd k_doble, a_doble, 3, 18, T_MS55, 0
        dd k_zurdo, a_zurdo, 0, 1, T_NUM, 0
        dd k_espera, a_espera, 0, 3, T_MS250, s_teclado
        dd k_repet, a_repet, 0, 31, T_NUM, 0
        dd k_dist, a_dist, 0, 1, T_DIST, 0
        dd 0
INI_E       equ 24
; los ajustes de fábrica, en el orden de las variables a_*
fabrica     dd 0, C_PETROLEO, C_PETROLEO_CL, 0, 5, 2, 8, 0, -1, -1, -1
N_AJ        equ 11

k_cab       db "; Hispa Ventanas - ajustes del programa Configuración (CONFIG.HVP).", 13, 10
            db "; Se puede cambiar a mano con Hispa Ventanas cerrado (docs/VENTANAS.md).", 13, 10, 0
s_pantalla  db "[Pantalla]", 0
s_raton     db "[Raton]", 0
s_teclado   db "[Teclado]", 0
k_dibujo    db "Dibujo", 0
k_cfondo    db "ColorFondo", 0
k_cdibujo   db "ColorDibujo", 0
k_salva     db "Salvapantallas", 0
k_minutos   db "Minutos", 0
k_vel       db "Velocidad", 0
k_doble     db "DobleClic", 0
k_zurdo     db "Zurdo", 0
k_espera    db "Espera", 0
k_repet     db "Repeticion", 0
k_dist      db "Distribucion", 0
n_ini       db "VENTANAS.INI", 0

; ---- los dibujos del fondo ----
; el de fábrica: un azulejo de rombos (el mismo de ventanas/datos.inc)
rombos_dib:
        db "+......++......."
        db ".+.............+"
        db "..+...........+."
        db "...+.........+.."
        db "....+.......+..."
        db ".....+.....+...."
        db "......+...+....."
        db "+......+.+.....+"
        db "+.......+......+"
        db ".......+.+......"
        db "......+...+....."
        db ".....+.....+...."
        db "....+.......+..."
        db "...+.........+.."
        db "..+...........+."
        db ".+.....++......+"
ola         db 3, 4, 5, 5, 6, 5, 5, 4, 3, 2, 1, 1, 0, 1, 1, 2
dib_nombres dd n_d0, n_d1, n_d2, n_d3, n_d4, n_d5, n_d6, n_d7, n_d8, n_d9
n_d0        db "rombos", 0
n_d1        db "liso", 0
n_d2        db "puntos", 0
n_d3        db "rayas", 0
n_d4        db "cuadros", 0
n_d5        db "ladrillos", 0
n_d6        db "olas", 0
n_d7        db "damero", 0
n_d8        db "trama", 0
n_d9        db "zigzag", 0
; los temas: dibujo, color del fondo, color del dibujo
temas       db 0, C_PETROLEO, C_PETROLEO_CL
            db 2, C_TINTA, C_ANIL
            db 6, C_ANIL, C_TURQUESA
            db 9, C_OLIVA, C_OCRE
            db 0, C_GRANATE, C_TERRACOTA
            db 6, C_ARENA_OSC, C_ARENA
            db 4, C_TINTA, C_MARRON
            db 5, C_TERRACOTA, C_CREMA
tema_nombres dd n_t0, n_t1, n_t2, n_t3, n_t4, n_t5, n_t6, n_t7
n_t0        db "Hispa", 0
n_t1        db "Noche", 0
n_t2        db "Mar", 0
n_t3        db "Campo", 0
n_t4        db "Vino", 0
n_t5        db "Duna", 0
n_t6        db "Pizarra", 0
n_t7        db "Ladrillo", 0
col_nombres dd n_c0, n_c1, n_c2, n_c3, n_c4, n_c5, n_c6, n_c7
            dd n_c8, n_c9, n_c10, n_c11, n_c12, n_c13, n_c14, n_c15
n_c0        db "tinta", 0
n_c1        db "marrón", 0
n_c2        db "azul petróleo", 0
n_c3        db "petróleo claro", 0
n_c4        db "terracota", 0
n_c5        db "terracota claro", 0
n_c6        db "arena", 0
n_c7        db "arena oscura", 0
n_c8        db "crema", 0
n_c9        db "blanco", 0
n_c10       db "ocre", 0
n_c11       db "verde oliva", 0
n_c12       db "añil", 0
n_c13       db "granate", 0
n_c14       db "turquesa", 0
n_c15       db "piedra", 0
; repeticiones por segundo (x10) de cada código de la función 03h de INT 16h
ritmos      dw 300, 267, 240, 218, 200, 185, 171, 160, 150, 133, 120, 109, 100, 92, 86, 80
            dw 75, 67, 60, 55, 50, 46, 43, 40, 37, 33, 30, 27, 25, 23, 21, 20

; ---- textos ----
t_titulo    db "Configuración", 0
secc_nombres dd n_s0, n_s1, n_s2, n_s3, n_s4, n_s5
n_s0        db "Pantalla", 0
n_s1        db "Ratón", 0
n_s2        db "Teclado", 0
n_s3        db "Sonido", 0
n_s4        db "Fecha y hora", 0
n_s5        db "Sistema", 0
t_est0      db "Se aplica al momento.", 0
t_est1      db "Hay cambios sin guardar.", 0
t_est2      db "Guardado en VENTANAS.INI.", 0
t_est3      db "No se ha podido guardar.", 0
t_fabrica   db "De fábrica", 0
t_guardar   db "Guardar", 0
t_cerrar    db "Cerrar", 0
t_fab_preg  db "¿Poner todos los ajustes como vienen de fábrica?", 0
t_tema      db "Tema:", 0
t_dib       db "Dibujo del fondo: ", 0
t_cf        db "Color del fondo: ", 0
t_cd        db "Color del dibujo: ", 0
t_salva     db "Salvapantallas tras", 0
t_minuto    db " minuto", 0
t_probar    db "Probar", 0
t_salva_nota db "Se quita con el ratón o una tecla.", 0
t_salva_tit db "Salvapantallas", 0
t_logo      db "Hispa Ventanas", 0
t_r_vel     db "Velocidad del puntero:", 0
t_lenta     db "Lenta", 0
t_rapida    db "Rápida", 0
t_r_dbl     db "Doble clic: tiempo máximo entre los dos clics:", 0
t_corto     db "Corto", 0
t_largo     db "Largo", 0
t_ms        db " ms", 0
t_r_prueba  db "Pruebe aquí el doble clic:", 0
t_zurdo     db "Intercambiar los botones (para zurdos)", 0
t_zurdo_nota db "Con los botones intercambiados, el principal es", 10
            db "el botón derecho.", 0
t_t_esp     db "Espera antes de repetir una tecla:", 0
t_corta     db "Corta", 0
t_larga     db "Larga", 0
t_t_rep     db "Velocidad de repetición:", 0
t_por_s     db " por s", 0
t_t_prueba  db "Pruebe aquí (mantenga pulsada una tecla):", 0
t_t_dist    db "Distribución del teclado:", 0
t_espanola  db "Española", 0
t_eeuu      db "Estadounidense", 0
t_t_si      db "La cambia al momento el residente TECLADO.", 0
t_t_no      db "TECLADO (KEYB) no está cargado: ponga TECLADO SP", 10
            db "en AUTOEXEC.BAT para el teclado español.", 0
t_s_vol     db "Volumen:", 0
t_pct       db " %", 0
t_silencio  db "Silencio", 0
t_s_nota    db "También se cambia con el altavoz de la barra de", 10
            db "arriba.", 0
t_s_no      db "No hay driver de sonido.", 10
            db "Cargue antes HSPSND (está en \HISPADOS).", 0
t_f_fecha   db "Cambiar la fecha:", 0
t_f_dia     db "Día:", 0
t_f_mes     db "Mes:", 0
t_f_ano     db "Año:", 0
t_f_hora    db "Cambiar la hora:", 0
t_f_h       db "Hora:", 0
t_f_m       db "Minuto:", 0
t_f_s       db "Segundo:", 0
t_f_poner   db "Poner fecha y hora", 0
t_f_nota    db "Formato de España: día/mes/año y 24 horas.", 0
t_f_bien    db "Fecha y hora cambiadas.", 0
t_f_malf    db "La fecha no es válida.", 0
t_f_malh    db "La hora no es válida.", 0
t_de        db " de ", 0
dias        dd n_w0, n_w1, n_w2, n_w3, n_w4, n_w5, n_w6
n_w0        db "domingo", 0
n_w1        db "lunes", 0
n_w2        db "martes", 0
n_w3        db "miércoles", 0
n_w4        db "jueves", 0
n_w5        db "viernes", 0
n_w6        db "sábado", 0
meses       dd n_m1, n_m2, n_m3, n_m4, n_m5, n_m6, n_m7, n_m8, n_m9, n_m10, n_m11, n_m12
n_m1        db "enero", 0
n_m2        db "febrero", 0
n_m3        db "marzo", 0
n_m4        db "abril", 0
n_m5        db "mayo", 0
n_m6        db "junio", 0
n_m7        db "julio", 0
n_m8        db "agosto", 0
n_m9        db "septiembre", 0
n_m10       db "octubre", 0
n_m11       db "noviembre", 0
n_m12       db "diciembre", 0
t_i_dos     db "Sistema: Hispa-DOS (compatible con MS-DOS ", 0
t_i_cpu     db "Procesador:", 10, "  ", 0
t_i_386     db "80386", 0
t_i_486     db "80486 o compatible", 0
t_i_fam     db ", familia ", 0
t_i_conv    db "Memoria convencional: ", 0
t_i_ext     db "Memoria extendida: ", 0
t_i_libre   db "Memoria libre para programas (DPMI): ", 0
t_i_kb      db " KB", 0
t_i_rat_si  db "Ratón: driver INT 33h (HSPRATON)", 0
t_i_rat_no  db "Ratón: no hay (cargue HSPRATON)", 0
t_i_snd_si  db "Sonido: driver HSPSND", 0
t_i_snd_no  db "Sonido: no hay driver (HSPSND)", 0
t_i_tec_sp  db "Teclado: español (TECLADO)", 0
t_i_tec_us  db "Teclado: estadounidense (TECLADO)", 0
t_i_tec_no  db "Teclado: el de la BIOS (TECLADO no está cargado)", 0
t_i_pais    db "País: España (página de códigos 437)", 0

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        push edi
        mov edi, ini_ruta           ; VENTANAS.INI, al lado del módulo
        xor ebx, ebx
.r:
        lodsb
        stosb
        cmp al, '\'
        jne .r1
        mov ebx, edi
.r1:
        test al, al
        jnz .r
        test ebx, ebx
        jnz .r2
        mov ebx, ini_ruta
.r2:
        mov edi, ebx
        mov esi, n_ini
        call hvl_cad
        mov byte [edi], 0
        pop edi
        mov esi, edi
        mov al, 'A'
        call hvl_opcion
        setnc byte [residente]
        HV tics
        HV azar_semilla
        mov edx, eax                ; (el salvapantallas cuenta desde ahora)
        mov eax, AJ_ULT_ACT
        HV aj_poner
        mov ebx, listas             ; las clases de los controles
.cl:
        mov esi, [ebx]
        test esi, esi
        jz .cl2
        call hvl_clases
        add ebx, 4
        jmp .cl
.cl2:
        call de_fabrica
        call ini_leer
        call aplicar
        ; la ventana: centrada; oculta si se ha cargado al arrancar
        mov esi, cfg_plant
        mov edi, plant
        mov ecx, P_TAM / 4
        rep movsd
        HV pantalla
        sub eax, [plant+P_AN]
        sar eax, 1
        mov [plant+P_X], eax
        sub ebx, BARRA_H + SOMBRA
        sub ebx, [plant+P_AL]
        sar ebx, 1
        jns .y
        xor ebx, ebx
.y:
        add ebx, BARRA_H
        mov [plant+P_Y], ebx
        cmp byte [residente], 0
        je .v
        or dword [plant+P_ESTILO], E_OCULTA
.v:
        mov esi, plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        mov ebp, eax
        mov esi, pl_fijos
        HV ven_lista
        mov esi, sel_sec
        xor edx, edx
        call sel_ini
        mov ecx, 1
        mov edx, 1000
        HV tmp_poner
        HV app_bucle
.x:
        ret

; =============================================================================
;  La ventana
; =============================================================================
cfg_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_COMMAND
        je ordenes
        cmp eax, WM_TIMER
        je reloj
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_MONTAR
        je .montar
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_ICONO], hvp_icono
        jmp .hecho
.destruir:
        mov dword [ven], 0
        HV msg_salir
        jmp .hecho
.tecla:
        cmp ecx, TC_ESC
        je .esc
        HVJ ven_defecto
.esc:
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.cerrar:
        cmp byte [cambiado], 0
        je .c1
        call guardar
.c1:
        cmp byte [residente], 0
        jne .c2
        mov eax, WM_CLOSE
        HVJ ven_defecto
.c2:
        call panel_desmontar        ; se queda en marcha, oculta
        mov byte [pedido], 0
        xor eax, eax
        HV ven_mostrar
        jmp .hecho
.montar:
        mov byte [pedido], 0
        cmp byte [montado], 0
        jne .hecho
        test dword [ebp+V_ESTADO], S_VISIBLE
        jz .hecho
        call panel_montar
        cmp byte [cambiado], 0
        jne .m1
        mov ecx, ID_ESTADO
        mov esi, t_est0
        call texto_poner
.m1:
        mov ecx, ID_SEC
        call hija
        push ebp
        mov ebp, eax
        HV ven_enfocar
        pop ebp
        jmp .hecho
.pintar:
        HV dc_pintar
        mov eax, [montado]          ; (montado y pedido, a cero los dos)
        test ax, ax
        jnz .p1
        mov byte [pedido], 1        ; la primera vez que se ve: los controles
        mov eax, WM_MONTAR
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
.p1:
        NEGRITA 1
        COLOR C_TERRACOTA
        mov eax, PX
        mov ebx, 10
        mov ecx, [seccion]
        mov esi, [secc_nombres+ecx*4]
        HV dc_texto
        NEGRITA 0
        COLOR C_OCRE
        mov eax, PX
        mov ebx, 30
        mov ecx, PAN_AN
        mov edx, 2
        HV dc_rect
        COLOR C_ARENA
        mov eax, 145
        mov ebx, 8
        mov ecx, 1
        mov edx, 322
        HV dc_rect
        mov eax, 8
        mov ebx, 334
        mov ecx, CLI_AN - 16
        mov edx, 1
        HV dc_rect
        HV dc_cerrar
.hecho:
        xor eax, eax
        ret

; ---- el segundero: salvapantallas, guardar solo y lo que cambia a la vista ----
reloj:
        HV tics
        mov ebx, eax
        mov eax, AJ_ULT_ACT
        HV aj_leer
        cmp ebx, eax
        jae .t1
        mov eax, AJ_ULT_ACT         ; (a medianoche el reloj vuelve a cero)
        mov edx, ebx
        HV aj_poner
        mov eax, ebx
.t1:
        cmp byte [residente], 0
        je .t2
        cmp dword [a_salva], 0
        je .t2
        cmp dword [salva_ven], 0
        jne .t2
        mov edx, ebx
        sub edx, eax
        imul eax, [a_minutos], 1092 ; tics de reloj por minuto
        cmp edx, eax
        jb .t2
        call salva_abrir
.t2:
        cmp byte [cambiado], 0
        je .t3
        mov eax, ebx
        sub eax, [t_cambio]
        cmp eax, 55                 ; tres segundos sin tocar nada
        jb .t3
        call guardar
.t3:
        cmp byte [montado], 0
        je .x
        cmp dword [seccion], 4
        jne .t4
        call fecha_refrescar
        jmp .x
.t4:
        cmp dword [seccion], 3
        jne .x
        call sonido_refrescar
.x:
        xor eax, eax
        ret

; ---- los controles: ECX = id + aviso*65536, EDX = el control ----
ordenes:
        mov eax, ecx
        shr eax, 16
        movzx ecx, cx
        cmp ecx, ID_SEC
        jne .o1
        mov eax, [edx+V_D0]         ; otro apartado
        mov [seccion], eax
        call panel_desmontar
        call panel_montar
        jmp .hecho
.o1:
        cmp ecx, ID_FABRICA
        jne .o2
        mov esi, t_fab_preg
        mov edi, t_titulo
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne .hecho
        call de_fabrica
        mov ebx, 0x010B             ; el teclado, como lo deja la BIOS
        call tecl_ritmo
        call aplicar
        call panel_desmontar
        call panel_montar
        call cambio
        jmp .hecho
.o2:
        cmp ecx, ID_GUARDAR
        jne .o3
        call guardar
        jmp .hecho
.o3:
        cmp ecx, ID_CERRAR
        jne .o4
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.o4:
        cmp ecx, ID_PAN
        jb .hecho
        ; ---- Pantalla ----
        cmp ecx, 100
        jne .p1
        cmp eax, LN_SELEC           ; un tema
        jne .hecho
        push ebp
        mov ebp, edx
        mov eax, LM_SELEC
        HV msg_enviar
        pop ebp
        cmp eax, N_TEMAS
        jae .hecho
        lea eax, [eax+eax*2]
        movzx edx, byte [temas+eax]
        mov [a_dibujo], edx
        movzx edx, byte [temas+eax+1]
        mov [a_cfondo], edx
        movzx edx, byte [temas+eax+2]
        mov [a_cdibujo], edx
        mov edx, [a_dibujo]
        mov ecx, 101
        call sel_poner
        mov edx, [a_cfondo]
        mov ecx, 102
        call sel_poner
        mov edx, [a_cdibujo]
        mov ecx, 103
        call sel_poner
        jmp .fondo
.p1:
        mov ebx, a_dibujo
        cmp ecx, 101
        je .p2
        mov ebx, a_cfondo
        cmp ecx, 102
        je .p2
        mov ebx, a_cdibujo
        cmp ecx, 103
        jne .p3
.p2:
        mov eax, [edx+V_D0]
        mov [ebx], eax
.fondo:
        call fondo_poner
        call pant_refrescar
        jmp .cambio
.p3:
        cmp ecx, 104
        jne .p4
        call marca_de
        mov [a_salva], eax
        jmp .cambio
.p4:
        cmp ecx, 105
        jne .p5
        mov eax, [edx+V_D0]
        mov [a_minutos], eax
        call pant_refrescar
        jmp .cambio
.p5:
        cmp ecx, 107
        jne .r1
        call salva_abrir
        jmp .hecho
        ; ---- Ratón ----
.r1:
        cmp ecx, 110
        jne .r2
        mov eax, [edx+V_D0]
        mov [a_vel], eax
        jmp .raton
.r2:
        cmp ecx, 112
        jne .r3
        mov eax, [edx+V_D0]
        mov [a_doble], eax
        jmp .raton
.r3:
        cmp ecx, 115
        jne .t1
        call marca_de
        mov [a_zurdo], eax
.raton:
        call raton_aplicar
        call raton_textos
        jmp .cambio
        ; ---- Teclado ----
.t1:
        cmp ecx, 120
        jne .t2
        mov eax, [edx+V_D0]
        mov [a_espera], eax
        jmp .tecl
.t2:
        cmp ecx, 122
        jne .t3
        mov eax, [edx+V_D0]
        mov [a_repet], eax
.tecl:
        call tecl_valores           ; (el otro deja de ser «no se toca»)
        mov [a_espera], eax
        mov [a_repet], ebx
        call tecl_aplicar
        call tecl_textos
        jmp .cambio
.t3:
        cmp ecx, 125
        jb .hecho
        cmp ecx, 126
        ja .s1
        sub ecx, 126
        neg ecx                     ; 125 española (1), 126 estadounidense (0)
        mov [a_dist], ecx
        call dist_aplicar
        jmp .cambio
        ; ---- Sonido ----
.s1:
        cmp ecx, 130
        jne .s2
        push dword [edx+V_D3]
        mov edx, [edx+V_D0]
        mov eax, AJ_VOLUMEN
        HV aj_poner
        call sonido_textos
        pop eax
        test eax, eax               ; arrastrando: la campanita, al soltar
        jnz .hecho                  ; (des_proc)
        HV snd_campana
        jmp .hecho
.s2:
        cmp ecx, 132
        jne .f1
        call marca_de
        mov edx, eax
        mov eax, AJ_SILENCIO
        HV aj_poner
        HV snd_campana              ; (al quitar el silencio; con él, no suena)
        jmp .hecho
        ; ---- Fecha y hora ----
.f1:
        cmp ecx, 148
        jne .hecho
        call fecha_poner
        jmp .hecho
.cambio:
        call cambio
.hecho:
        xor eax, eax
        ret

; =============================================================================
;  Utilidades de los controles (EBP no hace falta: van por [ven])
; =============================================================================
; ECX=id -> EAX=el control (0 si no está)
hija:
        push ebp
        mov ebp, [ven]
        HV ven_hija_id
        pop ebp
        ret

; ECX=id ESI=texto: lo pone y lo repinta
texto_poner:
        pushad
        call hija
        test eax, eax
        jz .x
        mov ebp, eax
        HV ven_texto
.x:
        popad
        ret

; ECX=id EDX=0/1: la marca de una casilla o de una opción
marca_poner:
        pushad
        call hija
        mov ebp, eax
        mov ecx, edx
        mov eax, BM_PONER_MARCA
        HV msg_enviar
        popad
        ret

; ECX=id -> EAX=0/1
marca_de:
        push ebp
        call hija
        mov ebp, eax
        mov eax, BM_MARCA
        HV msg_enviar
        pop ebp
        ret

; ECX=id EDX=valor: el elegido de un selector (o el valor de un deslizador)
sel_poner:
        pushad
        call hija
        test eax, eax
        jz .x
        mov ebp, eax
        mov [ebp+V_D0], edx
        HV ven_invalidar
.x:
        popad
        ret

; ESI=datos del selector (id, cuántos, columnas, ancho, alto, clase) EDX=elegido
sel_ini:
        pushad
        mov ecx, [esi]
        call hija
        test eax, eax
        jz .x
        mov ebp, eax
        mov [ebp+V_D0], edx
        lea edi, [ebp+V_D1]
        add esi, 4
        mov ecx, 5
        rep movsd
        HV ven_invalidar
.x:
        popad
        ret

; ECX=id EAX=mínimo EBX=máximo EDX=valor: un deslizador
des_poner:
        pushad
        call hija
        test eax, eax
        jz .x
        mov ebp, eax
        mov eax, [esp+28]
        mov [ebp+V_D1], eax
        mov [ebp+V_D2], ebx
        mov [ebp+V_D0], edx
        HV ven_invalidar
.x:
        popad
        ret

; ECX=id EAX=número: lo escribe en un cuadro de texto
ed_poner_num:
        pushad
        mov edi, tmp
        call hvl_num
        mov byte [edi], 0
        call hija
        mov ebp, eax
        mov eax, WM_SETTEXT
        mov edx, tmp
        HV msg_enviar
        popad
        ret

; ECX=id -> EAX=el número del cuadro de texto (-1 si no hay un número)
ed_leer_num:
        push ebp
        push ecx
        push edx
        push esi
        call hija
        mov ebp, eax
        mov byte [tmp], 0
        mov eax, WM_GETTEXT
        mov ecx, 12
        mov edx, tmp
        HV msg_enviar
        mov esi, tmp
.e:
        cmp byte [esi], ' '
        jne .d
        inc esi
        jmp .e
.d:
        mov al, [esi]
        sub al, '0'
        cmp al, 9
        ja .no
        call hvl_lee_num
        cmp byte [esi], ' '
        jbe .x
.no:
        or eax, -1
.x:
        pop esi
        pop edx
        pop ecx
        pop ebp
        ret

; (interna) avisa a la madre de EBP: WM_COMMAND con su id
avisar:
        pushad
        mov ecx, [ebp+V_ID]
        mov edx, ebp
        mov ebp, [ebp+V_PADRE]
        mov eax, WM_COMMAND
        HV msg_enviar
        popad
        ret

; algo ha cambiado: se guardará (ordenes, reloj)
cambio:
        push eax
        push ecx
        push esi
        mov byte [cambiado], 1
        HV tics
        mov [t_cambio], eax
        mov ecx, ID_ESTADO
        mov esi, t_est1
        call texto_poner
        pop esi
        pop ecx
        pop eax
        ret

; escribe VENTANAS.INI y lo dice abajo
guardar:
        pushad
        mov byte [cambiado], 0
        call ini_escribir
        mov esi, t_est2
        jnc .ok
        mov esi, t_est3
.ok:
        mov ecx, ID_ESTADO
        call texto_poner
        popad
        ret

; =============================================================================
;  Los apartados
; =============================================================================
panel_montar:
        pushad
        mov ebp, [ven]
        mov eax, [seccion]
        call [paneles+eax*4]
        mov byte [montado], 1
        mov ebp, [ven]
        HV ven_invalidar
        popad
        ret

; quita los controles del apartado (los de id 100 a 159)
panel_desmontar:
        pushad
        mov byte [montado], 0
        mov ecx, ID_PAN
.b:
        call hija
        test eax, eax
        jz .s
        mov ebp, eax
        HV ven_destruir
        jmp .b
.s:
        inc ecx
        cmp ecx, ID_PAN + 60
        jb .b
        popad
        ret

; ---- Pantalla ----
pan_pantalla:
        mov esi, pl_pantalla
        HV ven_lista
        mov ecx, 100                ; los temas
        call hija
        push ebp
        mov ebp, eax
        xor ebx, ebx
.t:
        mov edx, [tema_nombres+ebx*4]
        mov eax, LM_ANADIR
        HV msg_enviar
        inc ebx
        cmp ebx, N_TEMAS
        jb .t
        pop ebp
        mov esi, sel_dib
        mov edx, [a_dibujo]
        call sel_ini
        mov esi, sel_cf
        mov edx, [a_cfondo]
        call sel_ini
        mov esi, sel_cd
        mov edx, [a_cdibujo]
        call sel_ini
        mov ecx, 104
        mov edx, [a_salva]
        call marca_poner
        mov ecx, 105
        mov eax, 1
        mov ebx, 60
        mov edx, [a_minutos]
        call des_poner
        ; sigue: los textos y el tema que coincide

; los textos que dicen lo elegido, y en la lista el tema que es (si es uno)
pant_refrescar:
        pushad
        mov edi, l_dib
        mov esi, t_dib
        call hvl_cad
        mov eax, [a_dibujo]
        mov esi, [dib_nombres+eax*4]
        call hvl_cad
        mov byte [edi], 0
        mov ecx, 108
        mov esi, l_dib
        call texto_poner
        mov edi, l_cf
        mov esi, t_cf
        call hvl_cad
        mov eax, [a_cfondo]
        mov esi, [col_nombres+eax*4]
        call hvl_cad
        mov byte [edi], 0
        mov ecx, 109
        mov esi, l_cf
        call texto_poner
        mov edi, l_cd
        mov esi, t_cd
        call hvl_cad
        mov eax, [a_cdibujo]
        mov esi, [col_nombres+eax*4]
        call hvl_cad
        mov byte [edi], 0
        mov ecx, 110
        mov esi, l_cd
        call texto_poner
        mov edi, l_min
        mov eax, [a_minutos]
        call hvl_num
        mov esi, t_minuto
        call hvl_cad
        cmp eax, 1
        je .m1
        mov al, 's'
        stosb
.m1:
        mov byte [edi], 0
        mov ecx, 106
        mov esi, l_min
        call texto_poner
        mov ecx, 101                ; los dibujos se ven con los colores puestos
        mov edx, [a_dibujo]
        call sel_poner
        or edx, -1                  ; ¿es uno de los temas?
        xor ebx, ebx
.t:
        lea eax, [ebx+ebx*2]
        movzx ecx, byte [temas+eax]
        cmp ecx, [a_dibujo]
        jne .ts
        movzx ecx, byte [temas+eax+1]
        cmp ecx, [a_cfondo]
        jne .ts
        movzx ecx, byte [temas+eax+2]
        cmp ecx, [a_cdibujo]
        jne .ts
        mov edx, ebx
        jmp .tf
.ts:
        inc ebx
        cmp ebx, N_TEMAS
        jb .t
.tf:
        mov ecx, 100
        call hija
        mov ebp, eax
        mov eax, LM_SELEC
        HV msg_enviar
        cmp eax, edx
        je .x
        mov ecx, edx
        mov eax, LM_PONER_SELEC
        HV msg_enviar
.x:
        popad
        ret

; ---- Ratón ----
pan_raton:
        mov esi, pl_raton
        HV ven_lista
        mov ecx, 110
        mov eax, 1
        mov ebx, 6
        mov edx, [a_vel]
        call des_poner
        mov ecx, 112
        mov eax, 3
        mov ebx, 18
        mov edx, [a_doble]
        call des_poner
        mov ecx, 115
        mov edx, [a_zurdo]
        call marca_poner
        ; sigue: los textos
raton_textos:
        pushad
        mov edi, l_vel              ; «x1,5»
        mov al, 'x'
        stosb
        mov eax, [a_vel]
        shr eax, 1
        call hvl_num
        test byte [a_vel], 1
        jz .v1
        mov ax, ',5'
        stosw
.v1:
        mov byte [edi], 0
        mov ecx, 111
        mov esi, l_vel
        call texto_poner
        mov edi, l_dbl
        imul eax, [a_doble], 55
        call hvl_num
        mov esi, t_ms
        call hvl_cad
        mov byte [edi], 0
        mov ecx, 113
        mov esi, l_dbl
        call texto_poner
        popad
        ret

raton_aplicar:
        pushad
        mov eax, AJ_RAT_VEL
        mov edx, [a_vel]
        HV aj_poner
        mov eax, AJ_RAT_DOBLE
        mov edx, [a_doble]
        HV aj_poner
        mov eax, AJ_RAT_ZURDO
        mov edx, [a_zurdo]
        HV aj_poner
        popad
        ret

; ---- Teclado ----
pan_teclado:
        mov esi, pl_teclado
        HV ven_lista
        call tecl_valores
        mov edx, eax
        push ebx
        mov ecx, 120
        xor eax, eax
        mov ebx, 3
        call des_poner
        pop edx
        mov ecx, 122
        xor eax, eax
        mov ebx, 31
        call des_poner
        call keyb_ptr
        mov esi, pl_tecl_no
        test eax, eax
        jz .l
        mov esi, pl_tecl_si
.l:
        HV ven_lista
        test eax, eax
        jz tecl_textos
        movzx edx, byte [eax]       ; la distribución que hay puesta
        and edx, 1
        mov ecx, 125
        call marca_poner
        xor edx, 1
        mov ecx, 126
        call marca_poner
        ; sigue: los textos
tecl_textos:
        pushad
        call tecl_valores
        mov edi, l_esp
        inc eax
        imul eax, 250
        call hvl_num
        mov esi, t_ms
        call hvl_cad
        mov byte [edi], 0
        mov ecx, 121
        mov esi, l_esp
        call texto_poner
        mov edi, l_rep              ; «10,9 por s»
        neg ebx
        movzx eax, word [ritmos+62+ebx*2]
        xor edx, edx
        mov ecx, 10
        div ecx
        call hvl_num
        mov al, ','
        stosb
        mov eax, edx
        call hvl_num
        mov esi, t_por_s
        call hvl_cad
        mov byte [edi], 0
        mov ecx, 123
        mov esi, l_rep
        call texto_poner
        popad
        ret

; -> EAX=espera (0-3) EBX=velocidad (0-31): las puestas o, si no se han
; tocado, las de la BIOS (500 ms y 10,9 por segundo)
tecl_valores:
        mov eax, [a_espera]
        test eax, eax
        jns .e
        mov eax, 1
.e:
        mov ebx, [a_repet]
        test ebx, ebx
        jns .r
        mov ebx, 20
.r:
        ret

; da a la BIOS la espera y la velocidad de repetición, si se han tocado
tecl_aplicar:
        pushad
        mov eax, [a_espera]
        and eax, [a_repet]
        js .x                       ; las dos a -1: no se toca
        call tecl_valores
        mov bh, al
        neg bl
        add bl, 31
        call tecl_ritmo
.x:
        popad
        ret
; BH=espera (0-3) BL=código de velocidad (0 la más rápida)
tecl_ritmo:
        pushad
        mov eax, 0x0305
        int 0x16
        popad
        ret

; pone la distribución elegida, si se ha elegido una y está TECLADO
dist_aplicar:
        pushad
        mov edx, [a_dist]
        test edx, edx
        js .x
        call keyb_ptr
        test eax, eax
        jz .x
        mov [eax], dl
.x:
        popad
        ret

; -> EAX=puntero al octeto «española activa» del residente TECLADO (KEYB), o
; 0 si no está cargado. Se le pregunta en modo real (INT 2Fh AX=AD80h, que
; devuelve ES:DI) con la función 0300h del DPMI.
keyb_ptr:
        pushad
        mov dword [esp+28], 0
        mov edi, rmreg
        mov ecx, 13
        xor eax, eax
        rep stosd
        mov edi, rmreg
        mov word [edi+0x1C], 0xAD80
        mov eax, 0x0300
        mov ebx, 0x002F
        xor ecx, ecx
        int 0x31
        jc .x
        cmp byte [rmreg+0x1C], 0xFF
        jne .x
        cmp word [rmreg+0x10], 'HS'
        jne .x
        movzx esi, word [rmreg+0x22]
        shl esi, 4
        movzx eax, word [rmreg]
        add esi, eax
        mov eax, 0x0006             ; la base del segmento de datos
        mov ebx, ds
        int 0x31
        jc .x
        shl ecx, 16
        mov cx, dx
        sub esi, ecx
        mov [esp+28], esi
.x:
        popad
        ret

; ---- Sonido ----
pan_sonido:
        mov eax, AJ_HAY_SONIDO
        HV aj_leer
        mov esi, pl_sonido_no
        test eax, eax
        jz .l
        mov esi, pl_sonido
.l:
        HV ven_lista
        ; sigue: el volumen que hay puesto (también lo mira el segundero, por
        ; si se cambia con el altavoz de la barra)
sonido_refrescar:
        pushad
        mov ecx, 130
        call hija
        test eax, eax
        jz .x
        mov ebx, eax
        mov eax, AJ_VOLUMEN
        HV aj_leer
        test eax, eax
        jns .v
        mov eax, 100
.v:
        cmp dword [ebx+V_D2], 100   ; (recién creado: sin límites todavía)
        jne .pon
        cmp eax, [ebx+V_D0]
        je .m
.pon:
        mov edx, eax
        xor eax, eax
        mov ebx, 100
        call des_poner
        call sonido_textos
.m:
        mov eax, AJ_SILENCIO
        HV aj_leer
        mov edx, eax
        mov ecx, 132
        call marca_de
        cmp eax, edx
        je .x
        call marca_poner
.x:
        popad
        ret

sonido_textos:
        pushad
        mov ecx, 130
        call hija
        test eax, eax
        jz .x
        mov eax, [eax+V_D0]
        mov edi, l_vol
        call hvl_num
        mov esi, t_pct
        call hvl_cad
        mov byte [edi], 0
        mov ecx, 131
        mov esi, l_vol
        call texto_poner
.x:
        popad
        ret

; ---- Fecha y hora ----
pan_fecha:
        mov esi, pl_fecha
        HV ven_lista
        mov byte [l_res], 0
        mov ah, 0x2A
        int 0x21                    ; CX=año DH=mes DL=día
        movzx eax, dl
        push ecx
        mov ecx, 142
        call ed_poner_num
        movzx eax, dh
        mov ecx, 143
        call ed_poner_num
        pop eax
        movzx eax, ax
        mov ecx, 144
        call ed_poner_num
        mov ah, 0x2C
        int 0x21                    ; CH=hora CL=minuto DH=segundo
        mov ebx, ecx
        movzx eax, bh
        mov ecx, 145
        call ed_poner_num
        movzx eax, bl
        mov ecx, 146
        call ed_poner_num
        movzx eax, dh
        mov ecx, 147
        call ed_poner_num
        mov byte [ult_seg], 0xFF
        ; sigue: la fecha y la hora de arriba

; arriba, la fecha con todas las letras y la hora (si ha cambiado el segundo)
fecha_refrescar:
        pushad
        mov ah, 0x2C
        int 0x21
        cmp dh, [ult_seg]
        je .x
        mov [ult_seg], dh
        mov edi, l_hora
        movzx eax, ch
        call dos_cifras
        mov al, ':'
        stosb
        movzx eax, cl
        call dos_cifras
        mov al, ':'
        stosb
        movzx eax, dh
        call dos_cifras
        mov byte [edi], 0
        mov ah, 0x2A
        int 0x21                    ; AL=día de la semana (0 domingo)
        mov edi, l_fecha
        movzx ebx, al
        cmp ebx, 6
        ja .sd
        mov esi, [dias+ebx*4]
        call hvl_cad
        mov ax, ', '
        stosw
.sd:
        movzx eax, dl
        call hvl_num
        mov esi, t_de
        call hvl_cad
        movzx ebx, dh
        dec ebx
        cmp ebx, 11
        ja .sm
        mov esi, [meses+ebx*4]
        call hvl_cad
.sm:
        mov esi, t_de
        call hvl_cad
        movzx eax, cx
        call hvl_num
        mov byte [edi], 0
        mov ecx, 140
        mov esi, l_fecha
        call texto_poner
        mov ecx, 141
        mov esi, l_hora
        call texto_poner
.x:
        popad
        ret

; EAX (0-99) con dos cifras en EDI, que avanza
dos_cifras:
        push eax
        push edx
        xor edx, edx
        push ecx
        mov ecx, 10
        div ecx
        pop ecx
        add al, '0'
        stosb
        mov al, dl
        add al, '0'
        stosb
        pop edx
        pop eax
        ret

; «Poner fecha y hora»: lo de los seis cuadros, a Hispa-DOS (y al reloj del
; equipo, que es donde lo guarda)
fecha_poner:
        pushad
        mov esi, t_f_malf
        mov ecx, 142
        call ed_leer_num
        mov ebx, eax                ; BL=día
        cmp eax, 31
        ja .di
        mov ecx, 143
        call ed_leer_num
        cmp eax, 12
        ja .di
        mov bh, al                  ; BH=mes
        mov ecx, 144
        call ed_leer_num
        cmp eax, 1980
        jb .di
        cmp eax, 2099
        ja .di
        mov ecx, eax
        mov edx, ebx
        mov ah, 0x2B
        int 0x21
        test al, al
        jnz .di
        mov esi, t_f_malh
        mov ecx, 145
        call ed_leer_num
        cmp eax, 23
        ja .di
        mov bh, al
        mov ecx, 146
        call ed_leer_num
        cmp eax, 59
        ja .di
        mov bl, al
        mov ecx, 147
        call ed_leer_num
        cmp eax, 59
        ja .di
        mov ecx, ebx
        mov dh, al
        xor dl, dl
        mov ah, 0x2D
        int 0x21
        test al, al
        jnz .di
        mov esi, t_f_bien
.di:
        mov edi, l_res
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        mov ecx, 149
        mov esi, l_res
        call texto_poner
        mov byte [ult_seg], 0xFF
        call fecha_refrescar
        popad
        ret

; ---- Sistema ----
pan_sistema:
        pushad
        mov edi, sis_txt
        mov eax, AJ_TXT_VERSION     ; «Hispa Ventanas 0.3» (la primera línea)
        HV aj_leer
        mov esi, eax
.v:
        lodsb
        cmp al, 10
        jbe .v2
        stosb
        jmp .v
.v2:
        mov al, 10
        stosb
        mov esi, t_i_dos
        call hvl_cad
        mov ah, 0x30
        int 0x21
        mov ebx, eax
        movzx eax, bl
        call hvl_num
        mov al, '.'
        stosb
        movzx eax, bh
        call dos_cifras
        mov ax, ')' | (10 << 8)
        stosw
        mov esi, t_i_cpu
        call hvl_cad
        call cpu_texto
        mov al, 10
        stosb
        mov esi, t_i_conv
        call hvl_cad
        int 0x12
        movzx eax, ax
        call num_miles
        call .kb
        mov eax, 0xE801             ; memoria por encima de 1 MB
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        int 0x15
        jc .sinext
        test ax, ax
        jnz .e1
        mov eax, ecx
        mov ebx, edx
.e1:
        movzx eax, ax
        movzx ebx, bx
        shl ebx, 6
        add eax, ebx
        jz .sinext
        mov esi, t_i_ext
        call hvl_cad
        call num_miles
        call .kb
.sinext:
        push edi
        mov edi, rmreg
        mov eax, 0x0500
        int 0x31
        pop edi
        jc .sinlibre
        mov esi, t_i_libre
        call hvl_cad
        mov eax, [rmreg]
        shr eax, 10
        call num_miles
        call .kb
.sinlibre:
        mov eax, AJ_TXT_PANTALLA
        call .linea
        mov eax, AJ_TXT_VIDEO
        call .linea
        mov eax, AJ_HAY_RATON
        HV aj_leer
        mov esi, t_i_rat_no
        test eax, eax
        jz .r
        mov esi, t_i_rat_si
.r:
        call .texto
        mov eax, AJ_HAY_SONIDO
        HV aj_leer
        mov esi, t_i_snd_no
        test eax, eax
        jz .s
        mov esi, t_i_snd_si
.s:
        call .texto
        call keyb_ptr
        mov esi, t_i_tec_no
        test eax, eax
        jz .t
        mov esi, t_i_tec_us
        test byte [eax], 1
        jz .t
        mov esi, t_i_tec_sp
.t:
        call .texto
        mov esi, t_i_pais
        call hvl_cad
        mov byte [edi], 0
        popad
        mov esi, pl_sistema
        HV ven_lista
        ret
.linea:
        HV aj_leer
        mov esi, eax
.texto:
        call hvl_cad
        mov al, 10
        stosb
        ret
.kb:
        mov esi, t_i_kb
        jmp .texto

; EAX en decimal con el punto de los millares, en EDI (que avanza)
num_miles:
        push eax
        push ebx
        push edx
        cmp eax, 1000
        jb .u
        xor edx, edx
        mov ebx, 1000
        div ebx
        call num_miles
        mov byte [edi], '.'
        inc edi
        mov eax, edx
        xor edx, edx
        mov ebx, 100
        div ebx
        add al, '0'
        stosb
        mov eax, edx
        call dos_cifras
        jmp .x
.u:
        call hvl_num
.x:
        pop edx
        pop ebx
        pop eax
        ret

; el nombre del procesador, en EDI (que avanza)
cpu_texto:
        pushad
        pushfd                      ; ¿se puede cambiar el bit ID? (hay CPUID)
        pop eax
        mov ecx, eax
        xor eax, 0x00200000
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        xor eax, ecx
        test eax, 0x00200000
        jnz .cpuid
        mov eax, ecx                ; ¿y el bit AC? (486)
        xor eax, 0x00040000
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        xor eax, ecx
        mov esi, t_i_386
        test eax, 0x00040000
        jz .t
        mov esi, t_i_486
.t:
        call hvl_cad
        jmp .x
.cpuid:
        mov eax, 0x80000000
        db 0x0F, 0xA2               ; cpuid
        cmp eax, 0x80000004
        jb .marca
        mov ebp, edi                ; el nombre comercial (48 letras)
        mov edi, tmp
        mov esi, 0x80000002
.n:
        mov eax, esi
        db 0x0F, 0xA2
        stosd
        mov eax, ebx
        stosd
        mov eax, ecx
        stosd
        mov eax, edx
        stosd
        inc esi
        cmp esi, 0x80000004
        jbe .n
        mov byte [edi], 0
        mov edi, ebp
        mov esi, tmp
.e:
        cmp byte [esi], ' '
        jne .e2
        inc esi
        jmp .e
.e2:
        cmp byte [esi], 0
        je .marca
        mov ecx, 46
.c:
        lodsb
        test al, al
        jz .x
        stosb
        dec ecx
        jnz .c
        jmp .x
.marca:
        xor eax, eax
        db 0x0F, 0xA2
        mov eax, ebx
        stosd
        mov eax, edx
        stosd
        mov eax, ecx
        stosd
        mov esi, t_i_fam
        call hvl_cad
        mov eax, 1
        db 0x0F, 0xA2
        shr eax, 8
        and eax, 15
        call hvl_num
.x:
        mov [esp], edi
        popad
        ret

; =============================================================================
;  Aplicar los ajustes y el fondo del escritorio
; =============================================================================
de_fabrica:
        pushad
        mov esi, fabrica
        mov edi, a_dibujo
        mov ecx, N_AJ
        rep movsd
        popad
        ret

aplicar:
        call fondo_poner
        call raton_aplicar
        call tecl_aplicar
        jmp dist_aplicar

; el azulejo de 16x16 con el dibujo y los colores elegidos, al escritorio
fondo_poner:
        pushad
        mov edi, azulejo
        mov ecx, [a_dibujo]
        xor ebx, ebx
.f:
        xor eax, eax
.c:
        push eax
        call dib_punto
        stosb
        pop eax
        inc eax
        cmp eax, 16
        jb .c
        inc ebx
        cmp ebx, 16
        jb .f
        mov esi, azulejo
        HV esc_fondo
        popad
        ret

; ECX=dibujo EAX=x EBX=y (0-15) -> AL=el color de ese punto
dib_punto:
        push edx
        call [dib_tabla+ecx*4]
        mov al, [a_cfondo]
        jnz .x
        mov al, [a_cdibujo]
.x:
        pop edx
        ret
; cada dibujo: EAX=x EBX=y -> ZF=1 si el punto es del dibujo (usan EDX)
dib_rombos:
        mov edx, ebx
        shl edx, 4
        cmp byte [rombos_dib+edx+eax], '+'
        ret
dib_liso:
        or edx, -1
        ret
dib_puntos:
        mov edx, eax
        xor edx, ebx
        and edx, 7
        jnz .x
        test al, 3
.x:
        ret
dib_rayas:
        lea edx, [eax+ebx]
        and edx, 7
        shr edx, 1                  ; (0 y 1: la raya, de dos puntos)
        ret
dib_cuadros:
        test al, 7
        jz .x
        test bl, 7
.x:
        ret
dib_ladrillos:
        test bl, 7
        jz .x
        mov edx, ebx
        and edx, 8
        cmp edx, eax
.x:
        ret
dib_olas:
        mov edx, ebx
        and edx, 7
        cmp dl, [ola+eax]
        ret
dib_damero:
        mov edx, eax
        xor edx, ebx
        and edx, 8
        ret
dib_trama:
        mov edx, eax
        xor edx, ebx
        and edx, 1
        ret
dib_zigzag:
        mov edx, ebx
        cmp edx, 8
        jb .z
        neg edx
        add edx, 15
.z:
        add edx, eax
        and edx, 7
        shr edx, 1
        ret

; =============================================================================
;  VENTANAS.INI
; =============================================================================
ini_leer:
        pushad
        mov esi, ini_ruta
        mov edi, ini_buf
        mov ecx, INI_MAX
        call hvl_leer
        mov esi, ini_buf
.linea:
        mov al, [esi]
        test al, al
        jz .fin
        cmp al, ' '
        ja .l1
        inc esi
        jmp .linea
.l1:
        cmp al, ';'
        je .salta
        cmp al, '['
        je .salta
        mov ebx, ini_tabla
.t:
        mov edi, [ebx]
        test edi, edi
        jz .salta
        mov edx, esi
.c:
        mov al, [edi]
        test al, al
        jz .fk
        mov ah, [edx]
        or ax, 0x2020               ; sin mirar mayúsculas (son letras)
        cmp al, ah
        jne .otra
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
        jne .otra
        lea esi, [edx+1]
        call ini_valor
        jmp .salta
.otra:
        add ebx, INI_E
        jmp .t
.salta:
        mov al, [esi]
        test al, al
        jz .fin
        inc esi
        cmp al, 10
        jne .salta
        jmp .linea
.fin:
        popad
        ret

; (interna) EBX=la clave ESI=su valor en texto: si vale, a la variable
ini_valor:
        pushad
.e:
        cmp byte [esi], ' '
        jne .e2
        inc esi
        jmp .e
.e2:
        mov ecx, [ebx+16]
        cmp ecx, T_DIST
        je .dist
        cmp ecx, T_DIB
        jne .num
        xor eax, eax                ; el nombre de un dibujo
.d:
        mov edi, [dib_nombres+eax*4]
        mov edx, esi
.dc:
        mov cl, [edi]
        test cl, cl
        jz .df
        mov ch, [edx]
        or ch, 0x20
        cmp cl, ch
        jne .ds
        inc edi
        inc edx
        jmp .dc
.df:
        cmp byte [edx], ' '
        jbe .pon
.ds:
        inc eax
        cmp eax, N_DIB
        jb .d
        mov ecx, T_NUM              ; (o su número)
.num:
        mov al, [esi]
        sub al, '0'
        cmp al, 9
        ja .x
        call hvl_lee_num
        cmp ecx, T_MS55
        jne .n1
        add eax, 27
        xor edx, edx
        mov ecx, 55
        div ecx
        jmp .pon
.n1:
        cmp ecx, T_MS250
        jne .pon
        add eax, 125
        xor edx, edx
        mov ecx, 250
        div ecx
        dec eax
        jmp .pon
.dist:
        mov ax, [esi]
        or ax, 0x2020
        cmp ax, 'us'
        mov edx, 0
        je .dp
        inc edx
        cmp ax, 'sp'
        je .dp
        cmp ax, 'es'
        jne .x
.dp:
        mov eax, edx
.pon:
        cmp eax, [ebx+8]
        jl .x
        cmp eax, [ebx+12]
        jg .x
        mov edx, [ebx+4]
        mov [edx], eax
.x:
        popad
        ret

; escribe VENTANAS.INI -> CF=1 si no se puede
ini_escribir:
        pushad
        mov edi, ini_buf
        mov esi, k_cab
        call hvl_cad
        mov ebx, ini_tabla
.e:
        cmp dword [ebx], 0
        je .fin
        mov esi, [ebx+20]
        test esi, esi
        jz .s
        mov ax, 0x0A0D
        stosw
        call hvl_cad
        stosw
.s:
        mov edx, [ebx+4]
        mov eax, [edx]
        test eax, eax
        js .sig
        mov esi, [ebx]
        call hvl_cad
        mov byte [edi], '='
        inc edi
        mov ecx, [ebx+16]
        cmp ecx, T_DIB
        jne .t2
        mov esi, [dib_nombres+eax*4]
        call hvl_cad
        jmp .nl
.t2:
        cmp ecx, T_DIST
        jne .t3
        mov word [edi], 'US'
        test eax, eax
        jz .t2b
        mov word [edi], 'SP'
.t2b:
        add edi, 2
        jmp .nl
.t3:
        cmp ecx, T_MS55
        jne .t4
        imul eax, 55
.t4:
        cmp ecx, T_MS250
        jne .t5
        inc eax
        imul eax, 250
.t5:
        call hvl_num
.nl:
        mov ax, 0x0A0D
        stosw
.sig:
        add ebx, INI_E
        jmp .e
.fin:
        mov ecx, edi
        mov edx, ini_buf
        sub ecx, edx
        mov esi, ini_ruta
        call hvl_escribir
        popad
        ret

; =============================================================================
;  Controles propios
; =============================================================================
; ---- el selector: una rejilla de casillas de las que se elige una ----
; V_D0 la elegida, V_D1 cuántas, V_D2 columnas, V_D3 y V_D4 ancho y alto de
; la casilla, V_D5 qué se dibuja (sel_pintores), V_D6 tiene el foco
sel_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_KILLFOCUS
        je .sinfoco
        HVJ ven_defecto
.foco:
        mov dword [ebp+V_D6], 1
        jmp .inv
.sinfoco:
        mov dword [ebp+V_D6], 0
.inv:
        HV ven_invalidar
.nada:
        xor eax, eax
        ret
.pulsa:
        movsx eax, dx
        sar edx, 16
        mov ecx, edx
        or edx, eax
        js .nada
        xor edx, edx
        div dword [ebp+V_D3]
        cmp eax, [ebp+V_D2]
        jae .nada
        mov ebx, eax
        mov eax, ecx
        xor edx, edx
        div dword [ebp+V_D4]
        imul eax, [ebp+V_D2]
        add eax, ebx
.pon:
        cmp eax, [ebp+V_D1]
        jae .nada
        cmp eax, [ebp+V_D0]
        je .nada
        mov [ebp+V_D0], eax
        HV ven_invalidar
        call avisar
        jmp .nada
.tecla:
        mov eax, [ebp+V_D0]
        cmp ecx, TC_DER
        jne .t1
        inc eax
        jmp .pon
.t1:
        cmp ecx, TC_IZQ
        jne .t2
        dec eax
        jmp .pon
.t2:
        cmp ecx, TC_ABAJO
        jne .t3
        add eax, [ebp+V_D2]
        jmp .pon
.t3:
        cmp ecx, TC_ARRIBA
        jne .t4
        sub eax, [ebp+V_D2]
        jmp .pon
.t4:
        mov eax, WM_KEYDOWN
        HVJ ven_defecto
.pintar:
        HV dc_pintar
        xor edi, edi
.c:
        cmp edi, [ebp+V_D1]
        jae .fin
        mov eax, edi
        xor edx, edx
        div dword [ebp+V_D2]
        mov ebx, eax
        imul ebx, [ebp+V_D4]
        mov eax, edx
        imul eax, [ebp+V_D3]
        mov ecx, [ebp+V_D5]
        call [sel_pintores+ecx*4]
        inc edi
        jmp .c
.fin:
        HV dc_cerrar
        xor eax, eax
        ret

; los pintores: EAX=x EBX=y de la casilla, EDI=cuál es, EBP=el selector
; (-> EAX=el color del recuadro de la elegida: añil con el foco, tinta sin él)
sel_color:
        mov eax, C_TINTA
        cmp dword [ebp+V_D6], 0
        je .x
        mov eax, C_ANIL
.x:
        HV dc_color
        ret

; un color de la paleta
pin_color:
        pushad
        cmp edi, [ebp+V_D0]
        jne .n
        push eax
        call sel_color
        pop eax
        mov ecx, 22
        mov edx, 24
        HV dc_rect
        COLOR C_BLANCO
        add eax, 2
        add ebx, 2
        mov ecx, 18
        mov edx, 20
        HV dc_rect
        add eax, 2
        add ebx, 2
        mov ecx, 14
        mov edx, 16
        jmp .c
.n:
        COLOR C_ARENA_OSC
        inc eax
        add ebx, 2
        mov ecx, 20
        mov edx, 20
        HV dc_rect
        inc eax
        inc ebx
        mov ecx, 18
        mov edx, 18
.c:
        push eax
        mov eax, edi
        HV dc_color
        pop eax
        HV dc_rect
        popad
        ret

; un dibujo del fondo, con los colores elegidos (dos azulejos por lado)
pin_dibujo:
        pushad
        cmp edi, [ebp+V_D0]
        jne .n
        push eax
        call sel_color
        pop eax
        add eax, 3
        mov ecx, 42
        mov edx, 42
        HV dc_rect
        COLOR C_CREMA
        add eax, 2
        add ebx, 2
        mov ecx, 38
        mov edx, 38
        HV dc_rect
        sub eax, 5
        sub ebx, 2
.n:
        push eax
        push ebx
        mov ecx, edi
        mov edi, vista
        xor ebx, ebx
.f:
        xor edx, edx
.c:
        push ebx
        mov eax, edx
        and eax, 15
        and ebx, 15
        call dib_punto
        stosb
        pop ebx
        inc edx
        cmp edx, 32
        jb .c
        inc ebx
        cmp ebx, 32
        jb .f
        pop ebx
        pop eax
        add eax, 8
        add ebx, 5
        mov ecx, 32
        mov edx, 32
        mov esi, vista
        HV dc_copiar_clave
        popad
        ret

; un apartado: un rombo y su nombre; el elegido, en añil
pin_seccion:
        pushad
        mov esi, C_TINTA            ; el color del texto
        mov edx, C_ARENA_OSC        ; y el del rombo
        cmp edi, [ebp+V_D0]
        jne .n
        COLOR C_ANIL
        mov ecx, 132
        mov edx, SEC_AL - 2
        HV dc_rect
        cmp dword [ebp+V_D6], 0
        je .sf
        COLOR C_OCRE                ; con el foco, un filete ocre
        HV dc_marco
.sf:
        mov esi, C_CREMA
        mov edx, C_OCRE
.n:
        push eax
        mov eax, edx
        HV dc_color
        pop eax
        add eax, 16
        add ebx, 16
        mov ecx, 6
        HV dc_rombo
        push eax
        mov eax, esi
        HV dc_color
        pop eax
        add eax, 14
        sub ebx, 8
        mov esi, [secc_nombres+edi*4]
        cmp edi, [ebp+V_D0]
        jne .t
        NEGRITA 1
.t:
        HV dc_texto
        NEGRITA 0
        popad
        ret

; ---- el deslizador: una guía y un rombo que se lleva con el ratón o con
; las flechas. V_D0 el valor, V_D1 el mínimo, V_D2 el máximo, V_D3 se está
; arrastrando, V_D6 tiene el foco
DES_M       equ 10                  ; margen de la guía a cada lado
des_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_KILLFOCUS
        je .sinfoco
        HVJ ven_defecto
.foco:
        mov dword [ebp+V_D6], 1
        jmp .inv
.sinfoco:
        mov dword [ebp+V_D6], 0
        jmp .s0
.suelta:
        cmp dword [ebp+V_D3], 0
        je .inv
        cmp dword [ebp+V_ID], 130   ; el del volumen: al soltarlo suena la
        jne .s0                     ; campanita con el volumen nuevo
        HV snd_campana
.s0:
        mov dword [ebp+V_D3], 0
.inv:
        HV ven_invalidar
.nada:
        xor eax, eax
        ret
.pulsa:
        mov dword [ebp+V_D3], 1
        jmp .porx
.mueve:
        cmp dword [ebp+V_D3], 0
        je .nada
.porx:
        movsx eax, dx               ; del sitio del ratón al valor
        sub eax, DES_M
        jns .x1
        xor eax, eax
.x1:
        mov ecx, [ebp+V_AN]
        sub ecx, 2*DES_M
        cmp eax, ecx
        jle .x2
        mov eax, ecx
.x2:
        mov ebx, [ebp+V_D2]
        sub ebx, [ebp+V_D1]
        imul eax, ebx
        mov edx, ecx
        shr edx, 1
        add eax, edx
        xor edx, edx
        div ecx
        add eax, [ebp+V_D1]
.pon:
        cmp eax, [ebp+V_D1]
        jge .p1
        mov eax, [ebp+V_D1]
.p1:
        cmp eax, [ebp+V_D2]
        jle .p2
        mov eax, [ebp+V_D2]
.p2:
        cmp eax, [ebp+V_D0]
        je .nada
        mov [ebp+V_D0], eax
        HV ven_invalidar
        call avisar
        jmp .nada
.tecla:
        mov ebx, [ebp+V_D2]         ; el paso: 1, o un veinteavo si son muchos
        sub ebx, [ebp+V_D1]
        mov eax, ebx
        xor edx, edx
        mov ebx, 20
        div ebx
        mov ebx, eax
        cmp ebx, 2
        jae .k0
        mov ebx, 1
.k0:
        mov eax, [ebp+V_D0]
        cmp ecx, TC_DER
        jne .k1
        add eax, ebx
        jmp .pon
.k1:
        cmp ecx, TC_IZQ
        jne .k2
        sub eax, ebx
        jmp .pon
.k2:
        cmp ecx, TC_INICIO
        jne .k3
        mov eax, [ebp+V_D1]
        jmp .pon
.k3:
        cmp ecx, TC_FIN
        jne .k4
        mov eax, [ebp+V_D2]
        jmp .pon
.k4:
        HV tec_estado               ; (EDX se ha perdido: las mayúsculas, otra vez)
        mov edx, eax
        mov eax, WM_KEYDOWN
        HVJ ven_defecto
.pintar:
        HV dc_pintar
        mov edi, [ebp+V_AN]
        sub edi, 2*DES_M            ; largo de la guía
        COLOR C_ARENA_OSC
        mov eax, DES_M
        mov ebx, 9
        mov ecx, edi
        mov edx, 3
        HV dc_rect
        mov ebx, 5                  ; los topes
        mov ecx, 1
        mov edx, 11
        HV dc_rect
        add eax, edi
        HV dc_rect
        mov eax, [ebp+V_D0]         ; el rombo, donde toca
        sub eax, [ebp+V_D1]
        imul eax, edi
        mov ebx, [ebp+V_D2]
        sub ebx, [ebp+V_D1]
        jnz .r
        inc ebx
.r:
        xor edx, edx
        div ebx
        add eax, DES_M
        mov ebx, 10
        COLOR C_MARRON
        mov ecx, 9
        HV dc_rombo
        COLOR C_TERRACOTA
        cmp dword [ebp+V_D6], 0
        je .r2
        COLOR C_ANIL
.r2:
        mov ecx, 7
        HV dc_rombo
        COLOR C_CREMA
        mov ecx, 2
        HV dc_rombo
        HV dc_cerrar
        xor eax, eax
        ret

; ---- la zona de prueba del doble clic: el rombo cambia de color ----
pru_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_LBUTTONDBLCLK
        je .doble
        HVJ ven_defecto
.doble:
        xor dword [ebp+V_D0], 1
        HV ven_invalidar
        xor eax, eax
        ret
.pintar:
        HV dc_pintar
        COLOR C_MARRON
        xor eax, eax
        xor ebx, ebx
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        HV dc_marco
        COLOR C_OCRE
        cmp dword [ebp+V_D0], 0
        je .c
        COLOR C_ANIL
.c:
        mov eax, ecx
        shr eax, 1
        mov ebx, edx
        shr ebx, 1
        mov ecx, 14
        HV dc_rombo
        COLOR C_CREMA
        mov ecx, 5
        HV dc_rombo
        HV dc_cerrar
        xor eax, eax
        ret

; =============================================================================
;  El salvapantallas: la pantalla en negro y un letrero que se pasea. Es una
;  ventana con título que ocupa toda la pantalla (el título queda fuera, por
;  arriba): al ser la activa, las teclas le llegan a ella. Se quita con
;  cualquier movimiento del ratón, clic o tecla.
; =============================================================================
salva_abrir:
        pushad
        cmp dword [salva_ven], 0
        jne .x
        mov edi, plant2
        mov dword [edi+P_CLASE], c_salva
        mov dword [edi+P_TEXTO], t_salva_tit
        mov dword [edi+P_ESTILO], E_TITULO
        mov dword [edi+P_X], -BORDE
        mov dword [edi+P_Y], -(BORDE + TIT_H + 1)
        HV pantalla
        mov [salva_an], eax
        mov [salva_al], ebx
        add eax, 2*BORDE
        add ebx, 2*BORDE + TIT_H + 1
        mov [edi+P_AN], eax
        mov [edi+P_AL], ebx
        mov dword [edi+P_ID], 0
        mov esi, edi
        xor ebp, ebp
        HV ven_crear
.x:
        popad
        ret

sal_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        cmp eax, WM_KEYDOWN
        je .nada
        cmp eax, WM_CHAR
        je .nada
        HVJ ven_defecto
.crear:
        mov [salva_ven], ebp
        mov eax, [salva_an]
        sub eax, LOGO_AN
        HV azar
        mov [salva_x], eax
        mov eax, [salva_al]
        sub eax, LOGO_AL
        HV azar
        mov [salva_y], eax
        mov dword [salva_dx], 2
        mov dword [salva_dy], 1
        HV tics
        mov [salva_t0], eax
        mov byte [salva_listo], 0
        mov byte [salva_hora], 0
        mov ecx, 1
        mov edx, 110
        HV tmp_poner
        jmp .nada
.destruir:
        mov dword [salva_ven], 0
.nada:
        xor eax, eax
        ret
.reloj:
        HV tics
        mov ebx, eax
        mov eax, AJ_ULT_ACT
        HV aj_leer
        cmp byte [salva_listo], 0
        jne .r1
        mov edx, ebx                ; medio segundo de margen al empezar
        sub edx, [salva_t0]
        cmp edx, 9
        jb .mueve
        mov [salva_base], eax
        mov byte [salva_listo], 1
        jmp .mueve
.r1:
        cmp eax, [salva_base]
        je .mueve
        HV ven_destruir             ; ratón o teclado: se acabó
        jmp .nada
.mueve:
        mov eax, ebx                ; la hora del letrero, una vez por segundo
        sub eax, [salva_th]
        cmp eax, 18
        jb .m0
        mov [salva_th], ebx
        mov byte [salva_hora], 0
.m0:
        call .inv
        mov eax, [salva_x]
        add eax, [salva_dx]
        js .bx
        mov edx, [salva_an]
        sub edx, LOGO_AN
        cmp eax, edx
        jle .mx
.bx:
        neg dword [salva_dx]
        mov eax, [salva_x]
.mx:
        mov [salva_x], eax
        mov eax, [salva_y]
        add eax, [salva_dy]
        js .by
        mov edx, [salva_al]
        sub edx, LOGO_AL
        cmp eax, edx
        jle .my
.by:
        neg dword [salva_dy]
        mov eax, [salva_y]
.my:
        mov [salva_y], eax
        call .inv
        jmp .nada
.inv:
        mov eax, [salva_x]
        mov ebx, [salva_y]
        mov ecx, LOGO_AN
        mov edx, LOGO_AL
        HV ven_invalidar_rect
        ret
.pintar:
        HV dc_pintar
        cmp byte [salva_hora], 0
        jne .p1
        mov ah, 0x2C
        int 0x21
        mov edi, salva_txt
        movzx eax, ch
        call dos_cifras
        mov al, ':'
        stosb
        movzx eax, cl
        call dos_cifras
        mov byte [edi], 0
        mov byte [salva_hora], 1
.p1:
        mov eax, [salva_x]
        mov ebx, [salva_y]
        add eax, 22
        add ebx, 22
        COLOR C_OCRE
        mov ecx, 20
        HV dc_rombo
        COLOR C_TERRACOTA
        mov ecx, 12
        HV dc_rombo
        COLOR C_TINTA
        mov ecx, 5
        HV dc_rombo
        COLOR C_CREMA
        NEGRITA 1
        add eax, 30
        sub ebx, 18
        mov esi, t_logo
        HV dc_texto
        NEGRITA 0
        COLOR C_ARENA
        add ebx, 20
        mov esi, salva_txt
        HV dc_texto
        HV dc_cerrar
        xor eax, eax
        ret

%include "hvplib.inc"

; =============================================================================
;  Variables (a cero al cargar)
; =============================================================================
        section .bss
ven         resd 1                  ; la ventana
seccion     resd 1                  ; el apartado que se ve
t_cambio    resd 1                  ; tics del último cambio sin guardar
; los ajustes, en el orden de «fabrica»
a_dibujo    resd 1                  ; dibujo del fondo (0 a N_DIB-1)
a_cfondo    resd 1                  ; colores del fondo y del dibujo (0-15)
a_cdibujo   resd 1
a_salva     resd 1                  ; salvapantallas: 0/1
a_minutos   resd 1                  ; y cuántos minutos espera
a_vel       resd 1                  ; ratón: velocidad en mitades (2 = normal)
a_doble     resd 1                  ; doble clic, en tics de reloj
a_zurdo     resd 1
a_espera    resd 1                  ; teclado: 0-3 (-1: no se toca)
a_repet     resd 1                  ; 0-31, 31 lo más rápido (-1: no se toca)
a_dist      resd 1                  ; 1 española, 0 estadounidense (-1: no se toca)
; el salvapantallas
salva_ven   resd 1
salva_an    resd 1
salva_al    resd 1
salva_x     resd 1
salva_y     resd 1
salva_dx    resd 1
salva_dy    resd 1
salva_t0    resd 1
salva_th    resd 1
salva_base  resd 1
montado     resb 1                  ; los controles del apartado están puestos
pedido      resb 1                  ; (va detrás de «montado»: se miran juntos)
residente   resb 1                  ; cargado al arrancar («/A»): no se cierra
cambiado    resb 1
ult_seg     resb 1
salva_listo resb 1
salva_hora  resb 1
            alignb 4
plant       resb P_TAM
plant2      resb P_TAM
rmreg       resb 0x34
azulejo     resb 256
vista       resb 32 * 32
ini_ruta    resb 100
tmp         resb 64
salva_txt   resb 8
l_dib       resb 48
l_cf        resb 48
l_cd        resb 48
l_min       resb 16
l_vel       resb 12
l_dbl       resb 12
l_esp       resb 12
l_rep       resb 16
l_vol       resb 12
l_fecha     resb 48
l_hora      resb 12
l_res       resb 40
sis_txt     resb 800
ini_buf     resb INI_MAX + 16
HVP_FIN
