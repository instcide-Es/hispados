; =============================================================================
;  Hispa Ventanas - constantes de la interfaz (mensajes, estilos, fichas)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Los mensajes llevan los nombres y los números documentados de la API de
;  Windows (es interfaz); los propios de cada control van desde WM_USER.
;  Convenio de registros de toda la capa (docs/VENTANAS.md):
;    EBP = ventana (puntero a su ficha)   EAX = mensaje o resultado
;    ECX = wParam                         EDX = lParam
; =============================================================================

; ---- mensajes ----
WM_NULL         equ 0x0000
WM_CREATE       equ 0x0001
WM_DESTROY      equ 0x0002
WM_MOVE         equ 0x0003
WM_SIZE         equ 0x0005
WM_SETFOCUS     equ 0x0007
WM_KILLFOCUS    equ 0x0008
WM_SETTEXT      equ 0x000C
WM_GETTEXT      equ 0x000D
WM_PAINT        equ 0x000F
WM_CLOSE        equ 0x0010
WM_QUIT         equ 0x0012
WM_KEYDOWN      equ 0x0100
WM_CHAR         equ 0x0102
WM_COMMAND      equ 0x0111
WM_TIMER        equ 0x0113
WM_MOUSEMOVE    equ 0x0200
WM_LBUTTONDOWN  equ 0x0201
WM_LBUTTONUP    equ 0x0202
WM_LBUTTONDBLCLK equ 0x0203
WM_RBUTTONDOWN  equ 0x0204          ; botón derecho (solo en la zona de la ventana)
WM_CUT          equ 0x0300
WM_COPY         equ 0x0301
WM_PASTE        equ 0x0302
WM_USER         equ 0x0400

; propios de los controles
BM_MARCA        equ WM_USER+1       ; botón: -> 1 si está marcado
BM_PONER_MARCA  equ WM_USER+2       ; ECX=0/1
EM_SEL_TODO     equ WM_USER+10      ; cuadro de texto: seleccionarlo todo
EM_MODIFICADO   equ WM_USER+11      ; -> 1 si se ha cambiado
EM_PONER_MODIF  equ WM_USER+12      ; ECX=0/1
EM_LARGO        equ WM_USER+13      ; -> octetos de texto
LM_ANADIR       equ WM_USER+20      ; lista: EDX=texto -> índice
LM_VACIAR       equ WM_USER+21
LM_SELEC        equ WM_USER+22      ; -> índice elegido (-1: ninguno)
LM_PONER_SELEC  equ WM_USER+23      ; ECX=índice
LM_TEXTO        equ WM_USER+24      ; ECX=índice -> puntero al texto
LM_CUENTA       equ WM_USER+25
DM_INICIO       equ WM_USER+40      ; diálogo: ya están creados los controles

; avisos de los controles: WM_COMMAND con ECX = id + aviso*65536, EDX=control
LN_SELEC        equ 1               ; lista: ha cambiado el elegido
LN_DOBLE        equ 2               ; lista: doble clic o Intro

; respuestas de los diálogos (los números de IDOK, IDCANCEL, IDYES, IDNO)
ID_ACEPTAR      equ 1
ID_CANCELAR     equ 2
ID_SI           equ 6
ID_NO           equ 7
; tipos de dlg_mensaje
DM_ACEPTAR      equ 0
DM_ACEPTAR_CANCELAR equ 1
DM_SI_NO_CANCELAR equ 3

; ---- teclas de WM_KEYDOWN (ECX): las que no escriben un carácter ----
TC_ESC          equ 0x01
TC_RETRO        equ 0x0E
TC_TAB          equ 0x0F
TC_INTRO        equ 0x1C
TC_F1           equ 0x3B
TC_F10          equ 0x44
TC_INICIO       equ 0x47
TC_ARRIBA       equ 0x48
TC_REPAG        equ 0x49
TC_IZQ          equ 0x4B
TC_DER          equ 0x4D
TC_FIN          equ 0x4F
TC_ABAJO        equ 0x50
TC_AVPAG        equ 0x51
TC_INS          equ 0x52
TC_SUPR         equ 0x53
TC_CTRL_IZQ     equ 0x73
TC_CTRL_DER     equ 0x74
TC_CTRL_FIN     equ 0x75
TC_CTRL_INICIO  equ 0x77
TC_CTRL_INS     equ 0x92
TC_CTRL         equ 0x100           ; + letra (1 = A ... 26 = Z): Ctrl+letra
; estado de las teclas de mayúsculas (EDX de WM_KEYDOWN y ECX del ratón)
TM_MAYUS        equ 3
TM_CTRL         equ 4
TM_ALT          equ 8

; ---- estilos de ventana ----
E_HIJA          equ 0x0001          ; dentro de la zona de su madre
E_TITULO        equ 0x0002          ; marco y barra de título
E_CERRAR        equ 0x0004          ; botón de cerrar
E_MAXIM         equ 0x0008          ; botón de maximizar
E_MINIM         equ 0x0010          ; botón de minimizar
E_TAMANO        equ 0x0020          ; se cambia de tamaño por los bordes
E_EMERGENTE     equ 0x0040          ; sin título, con un filete (menús)
E_TABULA        equ 0x0080          ; el foco para aquí con Tab
E_OCULTA        equ 0x0100          ; se crea sin mostrar
E_SINFOCO       equ 0x0200          ; no toma el foco aunque se haga clic
E_NORMAL        equ E_TITULO | E_CERRAR | E_MAXIM | E_MINIM | E_TAMANO
E_DIALOGO       equ E_TITULO | E_CERRAR
; estilos de cada control (desde el bit 16)
BS_CASILLA      equ 0x10000         ; botón: casilla de marcar
BS_OPCION       equ 0x20000         ; botón: opción (una del grupo)
BS_DEFECTO      equ 0x40000         ; botón: el que responde a Intro
SS_DERECHA      equ 0x10000         ; texto: pegado a la derecha
SS_HUNDIDO      equ 0x20000         ; texto: en un recuadro blanco
SS_NEGRITA      equ 0x40000
ES_MULTI        equ 0x10000         ; cuadro de texto de varias líneas

; ---- estado de una ventana ----
S_VISIBLE       equ 0x01
S_MINIM         equ 0x02
S_MAXIM         equ 0x04
S_INACTIVA      equ 0x08            ; no recibe ratón ni teclado
S_NCSUCIO       equ 0x10            ; hay que repintar el marco

; ---- ficha de una ventana ----
V_USO           equ 0
V_CLASE         equ 4
V_PROC          equ 8
V_PADRE         equ 12
V_TAREA         equ 16
V_ESTILO        equ 20
V_ESTADO        equ 24
V_X             equ 28              ; en la pantalla, con el marco
V_Y             equ 32
V_AN            equ 36
V_AL            equ 40
V_TEXTO         equ 44
V_ID            equ 48
V_MENU          equ 52
V_ICONO         equ 56
V_UX0           equ 60              ; lo que hay que repintar (en la pantalla)
V_UY0           equ 64
V_UX1           equ 68
V_UY1           equ 72
V_RX            equ 76              ; dónde estaba antes de maximizar
V_RY            equ 80
V_RAN           equ 84
V_RAL           equ 88
V_FOCO          equ 92              ; la hija que tenía el foco
V_D0            equ 96              ; datos de la clase
V_D1            equ 100
V_D2            equ 104
V_D3            equ 108
V_D4            equ 112
V_D5            equ 116
V_D6            equ 120
V_D7            equ 124
V_TAM           equ 128
MAX_VEN         equ 160
MAX_TOPE        equ 32

; ---- clase de ventana: dd procedimiento; db fondo, banderas, 0, 0 ----
C_PROC          equ 0
C_FONDO         equ 4               ; color con que se borra (SIN_FONDO: no)
C_BAND          equ 5
SIN_FONDO       equ 0xFF
CB_FOCO         equ 1               ; toma el foco con un clic

; ---- plantilla de ven_crear ----
P_CLASE         equ 0
P_TEXTO         equ 4
P_ESTILO        equ 8
P_X             equ 12
P_Y             equ 16
P_AN            equ 20
P_AL            equ 24
P_ID            equ 28
P_TAM           equ 32
; PLANTILLA clase, texto, estilo, x, y, ancho, alto, id
%macro PLANTILLA 8
        dd %1, %2, %3, %4, %5, %6, %7, %8
%endmacro

; ---- mensaje sacado de la cola (msg_tomar) ----
M_VEN           equ 0
M_MSG           equ 4
M_W             equ 8
M_L             equ 12
M_TAM           equ 16

; ---- medidas del aspecto ----
BORDE           equ 2               ; marco
TIT_H           equ 22              ; barra de título
SOMBRA          equ 5               ; sombra, abajo y a la derecha
BARRA_H         equ 28              ; barra superior del escritorio
MENU_H          equ 20              ; barra de menús de una ventana
AGARRE          equ 4               ; borde que se agarra para cambiar el tamaño
BOT_NC          equ 18              ; botones de la barra de título
MIN_AN          equ 140             ; tamaño mínimo de una ventana
MIN_AL          equ 70
BD_AN           equ 16              ; ancho de la barra de desplazamiento

; ---- zonas de una ventana (ges_donde) ----
Z_NADA          equ 0
Z_CLIENTE       equ 1
Z_TITULO        equ 2
Z_CERRAR        equ 3
Z_MAXIM         equ 4
Z_MINIM         equ 5
Z_MENU          equ 6
Z_BORDE         equ 16              ; + 1 izquierda, 2 derecha, 4 arriba, 8 abajo

; ---- los 16 colores de Hispa Ventanas (la paleta, en datos.inc) ----
C_TINTA         equ 0
C_MARRON        equ 1
C_PETROLEO      equ 2
C_PETROLEO_CL   equ 3
C_TERRACOTA     equ 4
C_TERRACOTA_CL  equ 5
C_ARENA         equ 6
C_ARENA_OSC     equ 7
C_CREMA         equ 8
C_BLANCO        equ 9
C_OCRE          equ 10
C_OLIVA         equ 11
C_ANIL          equ 12
C_GRANATE       equ 13
C_TURQUESA      equ 14
C_PIEDRA        equ 15
CLAVE           equ 0xFF            ; «transparente» en dc_copiar_clave

; ---- [Configuración] ajustes del entorno: aj_leer y aj_poner (ajustes.inc) ----
; EAX=número del ajuste (y EDX=valor en aj_poner). Los que se pueden cambiar:
AJ_RAT_VEL      equ 0               ; velocidad del ratón, en mitades (2 = normal)
AJ_RAT_DOBLE    equ 1               ; doble clic: tics de reloj como mucho (8)
AJ_RAT_ZURDO    equ 2               ; 1: botones del ratón intercambiados
AJ_VOLUMEN      equ 3               ; 0-100 (-1: no se sabe); se le da a HSPSND
AJ_SILENCIO     equ 4               ; 0/1
AJ_ULT_ACT      equ 5               ; tics del último movimiento, clic o tecla
AJ_N_RW         equ 6
; solo se leen (un octeto):
AJ_VID_BITS     equ 6               ; bits por punto de la pantalla
AJ_VID_TIPO     equ 7               ; 1 VESA, 0 VGA de 16 colores
AJ_HAY_RATON    equ 8
AJ_HAY_SONIDO   equ 9               ; HSPSND está y ha encontrado tarjeta
AJ_N_OCT        equ 10
; solo se leen (-> EAX=puntero a un texto acabado en 0):
AJ_TXT_PANTALLA equ 10              ; "Pantalla: 800 x 600, 256 colores"
AJ_TXT_VIDEO    equ 11              ; "Vídeo: VESA 3.0 con marco lineal"
AJ_TXT_DPMI     equ 12              ; "DPMI 0.90 (32 bits); memoria libre: ..."
AJ_TXT_VERSION  equ 13              ; "Hispa Ventanas 0.3" (hasta el carácter 10)
AJ_N            equ 14
