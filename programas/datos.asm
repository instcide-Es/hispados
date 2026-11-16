; =============================================================================
;  DATOS.HVP - Datos, la base de datos sencilla de la familia de oficina
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un gestor de fichas para una asociación, un taller o una casa: los socios,
;  los clientes, el inventario, la biblioteca, la agenda. Una BASE es un
;  archivo .BDD con una o varias TABLAS; cada tabla tiene sus CAMPOS (texto,
;  número, fecha, sí o no y texto largo) y sus REGISTROS. Lo que hace:
;    - tablas nuevas a partir de plantillas (Socios, Clientes, Inventario,
;      Biblioteca, Agenda) o en blanco; añadir, cambiar, quitar y mover campos;
;    - dos vistas de la misma tabla: la REJILLA (columnas que se ensanchan
;      arrastrando la raya del encabezado) y la FICHA (un registro, campo a
;      campo); añadir, cambiar, duplicar y borrar registros, con validación;
;    - ordenar por un campo (clic en su encabezado), buscar un texto, filtrar
;      por una condición y el resumen de un campo (suma, media, mayor, menor);
;    - exportar a .CSV (para Cuentas), importar un .CSV como tabla nueva,
;      imprimir el listado y etiquetas de dos columnas (puerto paralelo);
;    - un campo puede «venir de otra tabla»: su valor se elige de una lista.
;
;  LOS LÍMITES (se dicen cuando se alcanzan): 16 tablas por base, 32 campos
;  por tabla, 8.000 registros por tabla, 100 letras en un texto y 2.000 en un
;  texto largo, nueve cifras en un número (con hasta tres decimales) y, entre
;  todo, lo que quepa en la memoria que se consigue al abrir (2 MB; 1 MB o
;  448 K si el equipo tiene poca).
;
;  LAS TECLAS: flechas, RePág, AvPág, Inicio y Fin para moverse; Ins añade,
;  Intro cambia y Supr borra el registro; F4 pasa de la rejilla a la ficha;
;  F3 busca el siguiente; F5, el resumen; F6, las tablas; Ctrl+U base nueva,
;  Ctrl+A abrir, Ctrl+P imprimir, Ctrl+B buscar y Ctrl+F filtrar.
;
;  LA MEMORIA. Toda la base está en memoria, en un bloque de memoria extendida
;  (mem_bloque), con la misma forma que en el archivo: así guardar es escribir
;  el bloque y abrir es leerlo y comprobarlo. Los registros son de largo
;  variable (un texto ocupa lo que mide), de modo que varios miles de fichas
;  caben en poco sitio. Cambiar un registro es abrir o cerrar un hueco en el
;  bloque (base.inc: «hueco») y volver a hacer el índice de la tabla que se ve.
;
;  EL ARCHIVO .BDD (versión 1; los números, con el octeto bajo delante):
;    Cabecera (32 octetos)
;       0  "HBDD"            4  dd versión (1)
;       8  dd tamaño de todo el archivo
;      12  dd suma de comprobación de lo que va detrás de la cabecera
;      16  dd cuántas tablas (hasta 16)      20  dd la tabla que se veía
;      24  8 octetos a cero
;    Detrás, las tablas, una tras otra. Cada tabla:
;       0  dd tamaño de la tabla entera (esta cabecera, campos y registros)
;       4  dd cuántos registros
;       8  db cuántos campos (1 a 32)
;       9  db campo por el que se ordena (255: como se escribieron)
;      10  db 1 si el orden es de mayor a menor
;      11  db la vista: 0 rejilla, 1 ficha
;      12  4 octetos a cero       16  el nombre (23 letras y un 0)
;      40  los campos, 32 octetos cada uno:
;             0  el nombre (19 letras y un 0)
;            20  db tipo: 0 texto, 1 número, 2 fecha, 3 sí o no, 4 texto largo
;            21  db decimales del número (0 a 3)
;            22  dw ancho de su columna en la rejilla, en puntos
;            24  db tabla de la que viene + 1 (0: no viene de ninguna)
;            25  db y el campo de esa tabla       26  6 octetos a cero
;       y detrás los registros. Cada registro:
;             dw su tamaño (con estos dos octetos)
;             y un valor por campo, en su orden:
;               texto y texto largo   dw largo y las letras (página 437)
;               número                dd entero con signo, multiplicado por
;                                     10^decimales; 80000000h = vacío
;               fecha                 dd aaaammdd; 0 = vacía
;               sí o no               db 0 o 1
;
;  GUARDAR SIN SUSTOS. La base se guarda sola cada vez que cambia. Se escribe
;  entera en un archivo temporal (.$$$); solo cuando está completo, el archivo
;  anterior pasa a llamarse .ANT y el temporal toma el nombre .BDD. Si se va
;  la luz a medias, al abrir se usa el .BDD si está entero (tamaño y suma) y,
;  si no, el temporal o la copia .ANT: nunca queda una base a medio escribir.
;
;  Es un módulo .HVP (ventanas/hvp.inc) del grupo «Productividad». Las partes:
;  datos/base.inc (la base en memoria y el archivo), datos/vista.inc (lo que
;  se pinta), datos/dialogo.inc (los diálogos) y datos/salida.inc (.CSV e
;  impresora).
; =============================================================================
%include "hvp.inc"

MAX_TAB     equ 16                  ; tablas de una base
MAX_CAM     equ 32                  ; campos de una tabla
MAX_REG     equ 8000                ; registros de una tabla
TXT_MAX     equ 100                 ; letras de un texto
MEMO_MAX    equ 2000                ; y de un texto largo
EB_TAM      equ 2048                ; el búfer de edición de un campo
REG_MAX     equ 2 + MAX_CAM * (2 + MEMO_MAX)   ; lo más que ocupa un registro
RB_TAM      equ 66048               ; el búfer donde se monta un registro

; ---- el archivo ----
H_FIRMA     equ 0
H_VER       equ 4
H_TAM       equ 8
H_SUMA      equ 12
H_NTAB      equ 16
H_ACT       equ 20
H_CAB       equ 32
T_TAM       equ 0
T_NREG      equ 4
T_NCAM      equ 8
T_ORDC      equ 9
T_ORDD      equ 10
T_VISTA     equ 11
T_NOM       equ 16
T_CAB       equ 40
C_NOM       equ 0
C_TIPO      equ 20
C_DEC       equ 21
C_ANCHO     equ 22
C_REFT      equ 24
C_REFC      equ 25
C_TAM       equ 32
T_TEXTO     equ 0
T_NUM       equ 1
T_FECHA     equ 2
T_SINO      equ 3
T_MEMO      equ 4
VACIO       equ 0x80000000          ; un número sin escribir

; ---- la ventana ----
TB_AL       equ 30                  ; la barra de botones
CAB_AL      equ 20                  ; el encabezado de la rejilla
GY0         equ TB_AL + CAB_AL      ; la primera fila
FIL_AL      equ 18
FX0         equ 12                  ; la ficha: su papel
FY0         equ TB_AL + 8
FCAM_Y      equ FY0 + 40            ; el primer campo de la ficha
FCAM_AL     equ 20
AN_MIN      equ 24                  ; lo más estrecha y lo más ancha que
AN_MAX      equ 600                 ; puede ser una columna
ID_BV       equ 901                 ; las barras de desplazamiento
ID_BH       equ 902

; ---- órdenes ----
O_NUEVA     equ 101
O_ABRIR     equ 102
O_COPIA     equ 103
O_IMPORTAR  equ 104
O_EXPORTAR  equ 105
O_IMPRIMIR  equ 106
O_SALIR     equ 107
O_ETIQ      equ 108
O_R_MAS     equ 111
O_R_CAMB    equ 112
O_R_BORRA   equ 113
O_BUSCAR    equ 114
O_SIGUE     equ 115
O_R_DUP     equ 116
O_V_TABLA   equ 121
O_V_FICHA   equ 122
O_FILTRAR   equ 123
O_SINFILTRO equ 124
O_ORD_AZ    equ 125
O_ORD_ZA    equ 126
O_ORD_NO    equ 127
O_ORDENAR   equ 128
O_ANT       equ 129
O_SIG       equ 130
O_TABLAS    equ 131
O_T_NUEVA   equ 132
O_T_NOMBRE  equ 133
O_T_BORRA   equ 134
O_C_MAS     equ 135
O_C_CAMB    equ 136
O_C_QUITA   equ 137
O_C_IZQ     equ 138
O_C_DER     equ 139
O_RESUMEN   equ 141
O_INFO      equ 142
O_ACERCA    equ 151

HVP_CABECERA "Datos", entrada, 4
        db "................................"
        db "................................"
        db "....000000000000000000000000...."
        db "...0222222222222222222222220...."
        db "...0222222222222222222222220...."
        db "...0222222222222222222222220...."
        db "...0000000000000000000000000...."
        db "...0666666606666666606666660...."
        db "...0666666606666666606666660...."
        db "...0000000000000000000000000...."
        db "...0999999909999999909999990...."
        db "...0977779909777779909777990...."
        db "...0999999909999999909999990...."
        db "...0000000000000000000000000...."
        db "...0aaaaaaa0aaaaaaaa0aaaaaa0...."
        db "...0a0000aa0a00000aa0a000aa0...."
        db "...0aaaaaaa0aaaaaaaa0aaaaaa0...."
        db "...0000000000000000000000000...."
        db "...0999999909999999909999990...."
        db "...0977799909777777909779990...."
        db "...0999999909999999909999990...."
        db "...0000000000000000000000000...."
        db "...0999999909999999909999990...."
        db "...0977779909777999909777790...."
        db "...0999999909999999909999990...."
        db "...0000000000000000000000000...."
        db "...0999999909999999909999990...."
        db "...0999999909999999909999990...."
        db "....000000000000000000000000...."
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

c_dat       dd dat_proc
            db C_CREMA, 0, 0, 0
dat_plant:
        PLANTILLA c_dat, t_titulo, E_NORMAL, 10, 32, 780, 556, 0
hijos:
        PLANTILLA HV_c_barra, 0, E_HIJA, 0, 0, BD_AN, 50, ID_BV
        PLANTILLA HV_c_barra, 0, E_HIJA | BDS_HORIZ, 0, 0, 50, BD_AN, ID_BH
        dd 0

dat_menu:
        db "&Archivo", 0
        dw O_NUEVA
        db "&Nueva base...          Ctrl+U", 0
        dw O_ABRIR
        db "&Abrir base...          Ctrl+A", 0
        dw O_COPIA
        db "&Guardar una copia como...", 0
        dw 0
        db 0
        dw O_IMPORTAR
        db "I&mportar una tabla de un .CSV...", 0
        dw O_EXPORTAR
        db "&Exportar a .CSV para Cuentas...", 0
        dw 0
        db 0
        dw O_IMPRIMIR
        db "&Imprimir el listado    Ctrl+P", 0
        dw O_ETIQ
        db "Imprimir e&tiquetas...", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw O_R_MAS
        db "&Añadir un registro...     Ins", 0
        dw O_R_CAMB
        db "&Cambiar el registro...  Intro", 0
        dw O_R_DUP
        db "&Duplicar el registro...", 0
        dw O_R_BORRA
        db "&Borrar el registro       Supr", 0
        dw 0
        db 0
        dw O_BUSCAR
        db "B&uscar...              Ctrl+B", 0
        dw O_SIGUE
        db "Buscar el &siguiente        F3", 0
        dw 0xFFFF
        db "&Ver", 0
        dw O_V_TABLA
        db "En &tabla                   F4", 0
        dw O_V_FICHA
        db "En &ficha                   F4", 0
        dw O_ANT
        db "Registro &anterior       RePág", 0
        dw O_SIG
        db "Registro &siguiente      AvPág", 0
        dw 0
        db 0
        dw O_FILTRAR
        db "Fi&ltrar...             Ctrl+F", 0
        dw O_SINFILTRO
        db "&Quitar el filtro", 0
        dw 0
        db 0
        dw O_ORD_AZ
        db "Ordenar de &menor a mayor", 0
        dw O_ORD_ZA
        db "Ordenar de ma&yor a menor", 0
        dw O_ORD_NO
        db "Sin &ordenar (como se escribieron)", 0
        dw 0xFFFF
        db "&Tabla", 0
        dw O_TABLAS
        db "&Elegir tabla...            F6", 0
        dw O_T_NUEVA
        db "&Nueva tabla...", 0
        dw O_T_NOMBRE
        db "Cambiar el nom&bre de la tabla...", 0
        dw O_T_BORRA
        db "Borrar la &tabla", 0
        dw 0
        db 0
        dw O_C_MAS
        db "&Añadir un campo...", 0
        dw O_C_CAMB
        db "&Cambiar el campo...", 0
        dw O_C_QUITA
        db "&Quitar el campo", 0
        dw O_C_IZQ
        db "Mover el campo hacia &delante", 0
        dw O_C_DER
        db "Mover el campo hacia de&trás", 0
        dw 0xFFFF
        db "&Herramientas", 0
        dw O_RESUMEN
        db "&Resumen del campo...       F5", 0
        dw O_INFO
        db "&Datos de la base...", 0
        dw 0xFFFF
        db "&?", 0
        dw O_ACERCA
        db "&Acerca de Datos...", 0
        dw 0xFFFF
        db 0

; ---- los botones de la barra (programas/comun/barra.inc): x, ancho, dibujo,
;      orden, nombre y clase ----
            align 4
botones     dd 8, 24, ic_nuevo, O_NUEVA, n_nueva, BT_ICONO
            dd 34, 24, ic_abrir, O_ABRIR, n_abrir, BT_ICONO
            dd 68, 24, ic_mas, O_R_MAS, n_mas, BT_ICONO
            dd 94, 24, ic_lapiz, O_R_CAMB, n_camb, BT_ICONO
            dd 120, 24, ic_borrar, O_R_BORRA, n_borra, BT_ICONO
            dd 154, 24, ic_buscar, O_BUSCAR, n_buscar, BT_ICONO
            dd 180, 24, ic_filtro, O_FILTRAR, n_filtrar, BT_ICONO
            dd 206, 24, ic_orden, O_ORDENAR, n_ordenar, BT_ICONO
            dd 240, 24, ic_rejilla, O_V_TABLA, n_vtabla, BT_ICONO
            dd 266, 24, ic_ficha, O_V_FICHA, n_vficha, BT_ICONO
            dd 292, 24, ic_ant, O_ANT, n_ant, BT_ICONO
            dd 318, 24, ic_sig, O_SIG, n_sig, BT_ICONO
            dd 352, 24, ic_hoja, O_EXPORTAR, n_exportar, BT_ICONO
            dd 378, 24, ic_imprimir, O_IMPRIMIR, n_imprimir, BT_ICONO
            dd 412, 24, ic_sigma, O_RESUMEN, n_resumen, BT_ICONO
            dd 446, 240, bt_buf, O_TABLAS, n_tablas, BT_TEXTO
            dd 0, 0, 0, 0, 0, 0
n_nueva     db "Nueva base de datos", 0
n_abrir     db "Abrir otra base de datos", 0
n_mas       db "Añadir un registro", 0
n_camb      db "Cambiar el registro elegido", 0
n_borra     db "Borrar el registro elegido", 0
n_buscar    db "Buscar un texto en cualquier campo", 0
n_filtrar   db "Filtrar: ver solo los registros que cumplen una condición", 0
n_ordenar   db "Ordenar por el campo elegido (otra vez: al revés)", 0
n_vtabla    db "Ver la tabla entera, en filas y columnas", 0
n_vficha    db "Ver la ficha: un registro, campo a campo", 0
n_ant       db "Registro anterior", 0
n_sig       db "Registro siguiente", 0
n_exportar  db "Exportar lo que se ve a un archivo .CSV, para Cuentas", 0
n_imprimir  db "Imprimir el listado de lo que se ve", 0
n_resumen   db "Resumen del campo elegido: suma, media, mayor y menor", 0
n_tablas    db "Elegir otra tabla de la base o hacer una nueva", 0

; ---- iconos propios (con la forma de los de barra.inc) ----
ic_filtro:
        db "................"
        db "................"
        db ".00000000000000."
        db ".0cccccccccccc0."
        db "..0cccccccccc0.."
        db "...0cccccccc0..."
        db "....0cccccc0...."
        db ".....0cccc0....."
        db "......0cc0......"
        db "......0cc0......"
        db "......0cc0......"
        db "......0cc0......"
        db "......0c0......."
        db "......00........"
        db "................"
        db "................"
ic_orden:
        db "................"
        db "..00........0..."
        db ".0..0.......0..."
        db ".0..0.......0..."
        db ".0000.......0..."
        db ".0..0.......0..."
        db ".0..0.......0..."
        db "............0..."
        db ".0000.......0..."
        db "....0.......0..."
        db "...0......0.0.0."
        db "..0........000.."
        db ".0..........0..."
        db ".0000..........."
        db "................"
        db "................"
ic_rejilla:
        db "................"
        db "................"
        db ".00000000000000."
        db ".02222022220220."
        db ".00000000000000."
        db ".09999099990990."
        db ".09999099990990."
        db ".00000000000000."
        db ".09999099990990."
        db ".09999099990990."
        db ".00000000000000."
        db ".09999099990990."
        db ".09999099990990."
        db ".00000000000000."
        db "................"
        db "................"
ic_ficha:
        db "................"
        db "................"
        db ".00000000000000."
        db ".09999999999990."
        db ".09222299999990."
        db ".09999999999990."
        db ".09770977777790."
        db ".09999999999990."
        db ".09770977777790."
        db ".09999999999990."
        db ".09770977779990."
        db ".09999999999990."
        db ".09999999999990."
        db ".00000000000000."
        db "................"
        db "................"
ic_ant:
        db "................"
        db "................"
        db "................"
        db ".........00....."
        db ".......0020....."
        db ".....002220....."
        db "...00222220....."
        db "..022222220....."
        db "...00222220....."
        db ".....002220....."
        db ".......0020....."
        db ".........00....."
        db "................"
        db "................"
        db "................"
        db "................"
ic_sig:
        db "................"
        db "................"
        db "................"
        db ".....00........."
        db ".....0200......."
        db ".....022200....."
        db ".....02222200..."
        db ".....022222220.."
        db ".....02222200..."
        db ".....022200....."
        db ".....0200......."
        db ".....00........."
        db "................"
        db "................"
        db "................"
        db "................"

; ---- las plantillas de las tablas nuevas ----
%macro CAMPO 4                      ; nombre, tipo, decimales, ancho
%%a:    db %1
        times 20 - ($ - %%a) db 0
        db %2, %3
        dw %4
        times 8 db 0
%endmacro
N_PLANT     equ 6
            align 4
plantillas  dd tp_socios, 8, pc_socios
            dd tp_clientes, 7, pc_clientes
            dd tp_invent, 4, pc_invent
            dd tp_biblio, 5, pc_biblio
            dd tp_agenda, 5, pc_agenda
            dd tp_blanco, 0, 0
tp_socios   db "Socios", 0
tp_clientes db "Clientes", 0
tp_invent   db "Inventario", 0
tp_biblio   db "Biblioteca", 0
tp_agenda   db "Agenda", 0
tp_blanco   db "En blanco", 0
pc_socios:
        CAMPO "Nombre", T_TEXTO, 0, 136
        CAMPO "Apellidos", T_TEXTO, 0, 176
        CAMPO "NIF", T_TEXTO, 0, 88
        CAMPO "Teléfono", T_TEXTO, 0, 96
        CAMPO "Dirección", T_TEXTO, 0, 200
        CAMPO "Población", T_TEXTO, 0, 136
        CAMPO "Fecha de alta", T_FECHA, 0, 112
        CAMPO "Cuota pagada", T_SINO, 0, 104
pc_clientes:
        CAMPO "Nombre", T_TEXTO, 0, 200
        CAMPO "NIF", T_TEXTO, 0, 88
        CAMPO "Teléfono", T_TEXTO, 0, 96
        CAMPO "Correo", T_TEXTO, 0, 176
        CAMPO "Dirección", T_TEXTO, 0, 200
        CAMPO "Población", T_TEXTO, 0, 136
        CAMPO "Notas", T_MEMO, 0, 200
pc_invent:
        CAMPO "Artículo", T_TEXTO, 0, 240
        CAMPO "Cantidad", T_NUM, 0, 80
        CAMPO "Precio", T_NUM, 2, 96
        CAMPO "Lugar", T_TEXTO, 0, 160
pc_biblio:
        CAMPO "Título", T_TEXTO, 0, 240
        CAMPO "Autor", T_TEXTO, 0, 176
        CAMPO "Año", T_NUM, 0, 56
        CAMPO "Prestado a", T_TEXTO, 0, 160
        CAMPO "Fecha", T_FECHA, 0, 96
pc_agenda:
        CAMPO "Nombre", T_TEXTO, 0, 200
        CAMPO "Teléfono", T_TEXTO, 0, 96
        CAMPO "Correo", T_TEXTO, 0, 176
        CAMPO "Cumpleaños", T_FECHA, 0, 96
        CAMPO "Notas", T_MEMO, 0, 200
; el ancho con el que nace la columna de un campo nuevo, por su tipo
an_tipo     dw 160, 88, 96, 64, 200

; ---- textos ----
t_titulo    db "Datos", 0
t_aceptar   db "Aceptar", 0
t_cancelar  db "Cancelar", 0
t_si        db "Sí", 0
t_no        db "No", 0
t_nada      db 0
t_ext_bdd   db ".BDD", 0
t_ext_csv   db ".CSV", 0
t_bt_tabla  db "Tabla: ", 0
t_bt_sin    db "Tablas...", 0
t_tipo0     db "Texto", 0
t_tipo1     db "Número", 0
t_tipo2     db "Fecha", 0
t_tipo3     db "Sí o no", 0
t_tipo4     db "Texto largo", 0
            align 4
tipo_nom    dd t_tipo0, t_tipo1, t_tipo2, t_tipo3, t_tipo4
t_sin_tab   db "Esta base todavía no tiene tablas.", 0
t_sin_tab2  db "Pulse el botón «Tablas...» de la barra para hacer la primera.", 0
t_sin_reg   db "La tabla está vacía. Pulse Ins (o el botón «+») para añadir el primer registro.", 0
t_sin_fil   db "Ningún registro cumple el filtro.", 0
t_p_sintab  db "Sin tablas", 0
t_p_reg     db " registros", 0
t_p_reg1    db " registro", 0
t_p_de      db " de ", 0
t_p_filtro  db "   Filtro: ", 0
t_p_orden   db "   Orden: ", 0
t_p_desc    db " (de mayor a menor)", 0
t_p_suma    db "Suma de ", 0
t_f_reg     db "Registro ", 0
t_f_sin     db "Sin registros", 0
t_op0       db "contiene", 0
t_op1       db "es igual a", 0
t_op2       db "es mayor que", 0
t_op3       db "es menor que", 0
t_op4       db "está vacío", 0
            align 4
op_nom      dd t_op0, t_op1, t_op2, t_op3, t_op4
t_e_mem     db "No hay memoria bastante para abrir Datos.", 0
t_e_guardar db "No se ha podido guardar la base en el disco.", 10
            db "Compruebe que el disco no está lleno ni protegido.", 0
t_e_abrir   db "No se ha podido abrir esa base: el archivo no está,", 10
            db "no es una base de Datos o está estropeado.", 0
t_recupera  db "El archivo de la base estaba a medias (¿un corte de luz?).", 10
            db "Se ha recuperado la última copia entera.", 0
t_e_sintab  db "Antes hay que hacer una tabla: pulse «Tablas...».", 0
t_e_sinreg  db "No hay ningún registro elegido.", 0
t_e_lleno   db "No cabe más: la base ha llenado la memoria que tiene Datos.", 10
            db "Borre lo que no haga falta o reparta los datos en dos bases.", 0
t_e_maxreg  db "Una tabla no puede tener más de 8.000 registros.", 0
t_e_maxtab  db "Una base no puede tener más de 16 tablas.", 0
t_e_maxcam  db "Una tabla no puede tener más de 32 campos.", 0
t_e_uncampo db "Una tabla tiene que tener al menos un campo.", 0
t_q_borrar  db "¿Borrar este registro? No se puede deshacer.", 0
t_q_tborra  db "¿Borrar la tabla entera, con todos sus registros?", 10
            db "No se puede deshacer.", 0
t_q_cquita  db "¿Quitar este campo? Se borra lo que haya escrito en él", 10
            db "en todos los registros. No se puede deshacer.", 0
t_q_ctipo   db "Al cambiar el tipo del campo, los valores que no se puedan", 10
            db "pasar al tipo nuevo se quedarán vacíos. ¿Seguir?", 0
t_q_existe  db "Ya hay un archivo con ese nombre. ¿Poner la base nueva en su lugar?", 0
t_nueva_tit db "Nueva base: nombre del archivo", 0
t_abrir_tit db "Abrir una base de datos", 0
t_copia_tit db "Guardar una copia de la base", 0
t_copia_ok  db "La copia se ha guardado.", 0
t_nom_def   db "NUEVA.BDD", 0
t_buscar_tit db "Buscar", 0
t_buscar_q  db "Texto que se busca (en cualquier campo):", 0
t_no_esta   db "No se ha encontrado ese texto.", 0
t_nombre_tit db "Nombre de la tabla", 0
t_nombre_q  db "Nombre nuevo de la tabla:", 0
t_acerca_tit db "Acerca de Datos", 0
t_acerca    db "Datos", 10
            db "Fichas y tablas para la asociación, el taller o la casa:", 10
            db "socios, clientes, inventario, biblioteca, agenda.", 10, 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores.", 10
            db "GPL-2.0 o posterior.", 0

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        mov [param], edi
        mov dword [arr_col], -1
        mov edi, ruta_def           ; la base de siempre: DATOS.BDD, al lado
        mov eax, 'BDD'
        call hvl_ruta
        mov edi, ruta_ini           ; y dónde se apunta la última que se abrió
        mov eax, 'INI'
        call hvl_ruta
        mov esi, hijos
        call hvl_clases
        call dlg_clases
        mov eax, [hv_api + HV_dos_buf * 4]
        mov [dbuf], eax
        call haz_pliega
        call hoy_leer
        call memoria
        jnc .hay
        xor ebp, ebp
        mov esi, t_e_mem
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret
.hay:
        call base_inicial
        HV pantalla                 ; EAX=ancho EBX=alto: que quepa
        sub eax, 16
        cmp eax, 780
        jae .a
        mov [dat_plant+P_AN], eax
        mov dword [dat_plant+P_X], 8
.a:
        sub ebx, BARRA_H + 12
        cmp ebx, 556
        jae .b
        mov [dat_plant+P_AL], ebx
        mov dword [dat_plant+P_Y], BARRA_H + 4
.b:
        mov esi, dat_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        cmp byte [aviso_ini], 0     ; (lo que hubiera que decir al abrir)
        je .bucle
        mov esi, t_recupera
        call aviso
.bucle:
        HV app_bucle
.x:
        call guarda_si
        mov edx, [blq_mango]
        HV mem_bloque_soltar
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
dat_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_KEYDOWN
        je tecla
        cmp eax, WM_LBUTTONDOWN
        je raton
        cmp eax, WM_MOUSEMOVE
        je raton_mueve
        cmp eax, WM_LBUTTONUP
        je raton_suelta
        cmp eax, WM_LBUTTONDBLCLK
        je doble
        cmp eax, WM_MOUSEWHEEL
        je rueda
        cmp eax, WM_VSCROLL
        je .bv
        cmp eax, WM_HSCROLL
        je .bh
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_ENTERMENULOOP   ; se abre un menú: el ratón ya no está
        je .menu                    ; sobre ningún botón
        cmp eax, WM_GETMINMAXINFO
        jne .nmin
        mov eax, 700 + 240 * 65536
        ret
.nmin:
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], dat_menu
        mov dword [ebp+V_ICONO], hvp_icono
        call pon_titulo
        mov esi, hijos
        HV ven_lista
        mov ecx, ID_BV
        HV ven_hija_id
        mov [barra_v], eax
        mov ecx, ID_BH
        HV ven_hija_id
        mov [barra_h], eax
        push ebp
        mov ebp, eax
        mov eax, BDM_LINEA
        mov ecx, 16
        HV msg_enviar
        pop ebp
.tam:
        mov ebp, [ven]
        test ebp, ebp
        jz hecho
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        call coloca
        call inval
        jmp hecho
.bv:
        cmp byte [vista], 0
        jne .bvf
        mov [top], ecx
        jmp .bvx
.bvf:
        mov [ftop], ecx
.bvx:
        call inval
        jmp hecho
.bh:
        mov [xdesp], ecx
        call inval
        jmp hecho
.menu:
        cmp dword [br_sobre], 0
        je hecho
        mov dword [br_sobre], 0
        call inval
        jmp hecho
.destruir:
        mov dword [ven], 0
        HV msg_salir
hecho:
        xor eax, eax
        ret

; (interna) las dos barras de desplazamiento, en su sitio: la vertical a la
; derecha y la horizontal (solo en la rejilla) encima del pie
coloca:
        pushad
        cmp dword [barra_v], 0
        je .x
        mov ebp, [barra_h]
        xor eax, eax
        cmp byte [vista], 0
        jne .oc
        inc eax
.oc:
        push eax
        HV ven_mostrar
        pop esi                     ; ESI=1 si hay barra horizontal
        mov eax, [cli_an]
        sub eax, BD_AN
        mov ebx, TB_AL
        mov ecx, BD_AN
        mov edx, [cli_al]
        sub edx, TB_AL + BR_PIE
        test esi, esi
        jz .v
        sub edx, BD_AN
.v:
        test edx, edx
        jg .v1
        mov edx, 1
.v1:
        mov ebp, [barra_v]
        HV ven_mover
        test esi, esi
        jz .x
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, BR_PIE + BD_AN
        mov ecx, [cli_an]
        sub ecx, BD_AN
        mov edx, BD_AN
        mov ebp, [barra_h]
        HV ven_mover
.x:
        popad
        ret

; la ventana, a repintar (y antes, las barras y los topes al día)
inval:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        call ajusta
        HV ven_invalidar
.x:
        popad
        ret

; mensaje de aviso: ESI=texto
aviso:
        pushad
        mov ebp, [ven]
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret
; pregunta: ESI=texto -> ZF=1 si se acepta
pregunta:
        pushad
        mov ebp, [ven]
        mov edi, t_titulo
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        popad
        ret
; CF=1 (y lo dice) si la base no tiene ninguna tabla
hay_tabla:
        cmp dword [tab], 0
        jne .si
        push esi
        mov esi, t_e_sintab
        call aviso
        pop esi
        stc
        ret
.si:
        clc
        ret
; -> EAX=el registro elegido; CF=1 (y lo dice) si no hay ninguno
reg_elegido:
        call hay_tabla
        jc .x
        mov eax, [sel]
        cmp eax, [n_vis]
        jae .no
        push ebx
        mov ebx, [vis]
        mov eax, [ebx+eax*4]
        pop ebx
        clc
.x:
        ret
.no:
        push esi
        mov esi, t_e_sinreg
        call aviso
        pop esi
        stc
        ret

; ---- las órdenes (del menú, de la barra y de las teclas): ECX=número ----
orden:
        mov dword [br_sobre], 0     ; (el pie ya no habla del botón)
        call orden_haz
        jmp hecho
orden_haz:
        cmp ecx, O_NUEVA
        je base_nueva
        cmp ecx, O_ABRIR
        je base_abrir
        cmp ecx, O_COPIA
        je base_copia
        cmp ecx, O_IMPORTAR
        je importar
        cmp ecx, O_EXPORTAR
        je exportar
        cmp ecx, O_IMPRIMIR
        je imprimir
        cmp ecx, O_ETIQ
        je etiquetas
        cmp ecx, O_R_MAS
        je reg_mas
        cmp ecx, O_R_CAMB
        je reg_camb
        cmp ecx, O_R_DUP
        je reg_dup
        cmp ecx, O_R_BORRA
        je reg_borra
        cmp ecx, O_BUSCAR
        je buscar
        cmp ecx, O_SIGUE
        je buscar_sig
        cmp ecx, O_V_TABLA
        je .vt
        cmp ecx, O_V_FICHA
        je .vf
        cmp ecx, O_ANT
        je .ant
        cmp ecx, O_SIG
        je .sig
        cmp ecx, O_FILTRAR
        je filtrar
        cmp ecx, O_SINFILTRO
        je sin_filtro
        cmp ecx, O_ORD_AZ
        je .oaz
        cmp ecx, O_ORD_ZA
        je .oza
        cmp ecx, O_ORD_NO
        je .ono
        cmp ecx, O_ORDENAR
        je .oboton
        cmp ecx, O_TABLAS
        je tablas
        cmp ecx, O_T_NUEVA
        je tabla_dlg_nueva
        cmp ecx, O_T_NOMBRE
        je tabla_nombre
        cmp ecx, O_T_BORRA
        je tabla_quita
        cmp ecx, O_C_MAS
        je campo_mas
        cmp ecx, O_C_CAMB
        je campo_camb
        cmp ecx, O_C_QUITA
        je campo_quita
        cmp ecx, O_C_IZQ
        je campo_izq
        cmp ecx, O_C_DER
        je campo_der
        cmp ecx, O_RESUMEN
        je resumen
        cmp ecx, O_INFO
        je info
        cmp ecx, O_ACERCA
        je .acerca
        cmp ecx, O_SALIR
        jne .x
        mov ebp, [ven]
        HV ven_destruir
.x:
        ret
.vt:
        xor eax, eax
        jmp vista_pon
.vf:
        mov eax, 1
        jmp vista_pon
.ant:
        mov eax, -1
        jmp sel_mueve
.sig:
        mov eax, 1
        jmp sel_mueve
.oaz:
        xor edx, edx
        jmp .opon
.oza:
        mov edx, 1
.opon:
        mov eax, [col]
        jmp ordenar_por
.ono:
        mov eax, 0xFF
        xor edx, edx
        jmp ordenar_por
.oboton:
        mov eax, [col]
        jmp ordenar_clic
.acerca:
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret

; pone la vista EAX (0 rejilla, 1 ficha); se recuerda con la tabla
vista_pon:
        mov [vista], al
        mov ebx, [tab]
        test ebx, ebx
        jz .v
        cmp [ebx+T_VISTA], al
        je .v
        mov [ebx+T_VISTA], al
        mov byte [sucio], 1
.v:
        call coloca
        call sel_ver
        jmp inval

; ordena por el campo EAX (255: sin orden), EDX=1 de mayor a menor
ordenar_por:
        call hay_tabla
        jc .x
        mov ebx, [tab]
        mov [ebx+T_ORDC], al
        mov [ebx+T_ORDD], dl
        mov byte [sucio], 1
        call sel_guarda
        call filtra
        call sel_repon
        call inval
.x:
        ret
; un clic en el encabezado del campo EAX (o el botón «Ordenar»): la primera
; vez, de menor a mayor; otra vez sobre el mismo, al revés
ordenar_clic:
        cmp dword [tab], 0
        je ordenar_por
        mov ebx, [tab]
        xor edx, edx
        cmp [ebx+T_ORDC], al
        jne ordenar_por
        mov dl, [ebx+T_ORDD]
        xor dl, 1
        jmp ordenar_por

; ---- teclado ----
tecla:
        mov eax, O_NUEVA
        cmp ecx, TC_CTRL + 21       ; Ctrl+U
        je .ord
        mov eax, O_ABRIR
        cmp ecx, TC_CTRL + 1        ; Ctrl+A
        je .ord
        mov eax, O_IMPRIMIR
        cmp ecx, TC_CTRL + 16       ; Ctrl+P
        je .ord
        mov eax, O_BUSCAR
        cmp ecx, TC_CTRL + 2        ; Ctrl+B
        je .ord
        mov eax, O_FILTRAR
        cmp ecx, TC_CTRL + 6        ; Ctrl+F
        je .ord
        mov eax, O_SIGUE
        cmp ecx, TC_F1 + 2          ; F3
        je .ord
        mov eax, O_RESUMEN
        cmp ecx, TC_F1 + 4          ; F5
        je .ord
        mov eax, O_TABLAS
        cmp ecx, TC_F1 + 5          ; F6
        je .ord
        mov eax, O_R_CAMB
        cmp ecx, TC_INTRO
        je .ord
        mov eax, O_R_BORRA
        cmp ecx, TC_SUPR
        je .ord
        mov eax, O_R_MAS
        cmp ecx, TC_INS
        je .ord
        cmp ecx, TC_F1 + 3          ; F4: la otra vista
        jne .mov
        mov eax, O_V_FICHA
        cmp byte [vista], 0
        je .ord
        mov eax, O_V_TABLA
.ord:
        mov ecx, eax
        jmp orden
.mov:
        cmp ecx, TC_CTRL_INICIO
        je .ini
        cmp ecx, TC_CTRL_FIN
        je .fin
        cmp byte [vista], 0
        jne .ficha
        cmp ecx, TC_INICIO
        je .ini
        cmp ecx, TC_FIN
        je .fin
        mov eax, 1
        cmp ecx, TC_ABAJO
        je .sel
        mov eax, -1
        cmp ecx, TC_ARRIBA
        je .sel
        mov eax, [vis_fil]
        cmp ecx, TC_AVPAG
        je .sel
        neg eax
        cmp ecx, TC_REPAG
        je .sel
        mov eax, 1
        cmp ecx, TC_DER
        je .col
        cmp ecx, TC_TAB
        je .col
        mov eax, -1
        cmp ecx, TC_IZQ
        je .col
        jmp hecho
.ficha:
        mov eax, 1                  ; en la ficha, arriba y abajo van de campo
        cmp ecx, TC_ABAJO           ; en campo, y las páginas, de registro
        je .col
        cmp ecx, TC_TAB
        je .col
        mov eax, -1
        cmp ecx, TC_ARRIBA
        je .col
        mov eax, 1
        cmp ecx, TC_AVPAG
        je .sel
        cmp ecx, TC_DER
        je .sel
        mov eax, -1
        cmp ecx, TC_REPAG
        je .sel
        cmp ecx, TC_IZQ
        je .sel
        mov eax, -MAX_CAM
        cmp ecx, TC_INICIO
        je .col
        mov eax, MAX_CAM
        cmp ecx, TC_FIN
        je .col
        jmp hecho
.ini:
        mov eax, -MAX_REG
        jmp .sel
.fin:
        mov eax, MAX_REG
.sel:
        call sel_mueve
        jmp hecho
.col:
        call col_mueve
        jmp hecho

; mueve la elección EAX registros
sel_mueve:
        pushad
        add eax, [sel]
        mov ecx, [n_vis]
        dec ecx
        cmp eax, ecx
        jle .a
        mov eax, ecx
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        mov [sel], eax
        call sel_ver
        call inval
        popad
        ret
; mueve el campo elegido EAX sitios
col_mueve:
        pushad
        add eax, [col]
        mov ecx, [n_cam]
        dec ecx
        cmp eax, ecx
        jle .a
        mov eax, ecx
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        mov [col], eax
        mov byte [suma_ok], 0
        call sel_ver
        call inval
        popad
        ret
; que el registro y el campo elegidos queden a la vista
sel_ver:
        pushad
        mov eax, [sel]
        cmp eax, [top]
        jae .a
        mov [top], eax
.a:
        sub eax, [vis_fil]
        inc eax
        cmp eax, [top]
        jle .b
        mov [top], eax
.b:
        mov eax, [col]              ; en la ficha: el campo
        cmp eax, [ftop]
        jae .c
        mov [ftop], eax
.c:
        sub eax, [vis_cam]
        inc eax
        cmp eax, [ftop]
        jle .d
        mov [ftop], eax
.d:
        mov eax, [col]              ; en la rejilla: la columna
        call col_x
        mov ebx, eax
        mov eax, [col]
        inc eax
        call col_x                  ; EBX=donde empieza EAX=donde acaba
        cmp ebx, [xdesp]
        jae .e
        mov [xdesp], ebx
.e:
        mov ecx, [cli_an]
        sub ecx, BD_AN
        sub eax, ecx
        cmp eax, ebx                ; (una más ancha que la ventana: su principio)
        jle .f
        mov eax, ebx
.f:
        cmp eax, [xdesp]
        jle .x
        mov [xdesp], eax
.x:
        popad
        ret

; recuerda qué registro está elegido, para volver a él cuando cambie el orden
sel_guarda:
        push eax
        push ebx
        mov dword [sel_reg], -1
        mov eax, [sel]
        cmp eax, [n_vis]
        jae .x
        mov ebx, [vis]
        mov eax, [ebx+eax*4]
        mov [sel_reg], eax
.x:
        pop ebx
        pop eax
        ret
; y lo vuelve a elegir (si sigue a la vista)
sel_repon:
        pushad
        mov eax, [sel_reg]
        mov ebx, [vis]
        xor ecx, ecx
.b:
        cmp ecx, [n_vis]
        jae .x
        cmp [ebx+ecx*4], eax
        je .es
        inc ecx
        jmp .b
.es:
        mov [sel], ecx
.x:
        call sel_ver
        popad
        ret

rueda:
        mov eax, ecx
        cmp byte [vista], 0
        jne .f
        lea eax, [eax+eax*2]        ; tres filas por paso
        add eax, [top]
        jns .a
        xor eax, eax
.a:
        mov [top], eax
        call inval
        jmp hecho
.f:
        call sel_mueve
        jmp hecho

; ---- el ratón se mueve: arrastra el ancho de una columna, o dice el nombre
;      del botón que tiene debajo; sobre la raya de un encabezado, el cursor
;      de ensanchar ----
raton_mueve:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp dword [arr_col], -1
        je .libre
        sub eax, [arr_x0]           ; el ancho nuevo
        add eax, [arr_an0]
        cmp eax, AN_MIN
        jge .m1
        mov eax, AN_MIN
.m1:
        cmp eax, AN_MAX
        jle .m2
        mov eax, AN_MAX
.m2:
        mov ecx, eax
        mov eax, [arr_col]
        call cam_ptr
        mov [ebx+C_ANCHO], cx
        call inval
        jmp hecho
.libre:
        xor edi, edi
        cmp ebx, TB_AL
        jae .n
        test ebx, ebx
        js .n
        mov esi, botones
        call br_boton_en
        jc .n
        mov edi, [esi+16]
.n:
        push eax
        push ebx
        call br_sobre_pon
        je .mismo
        call inval
.mismo:
        pop ebx
        pop eax
        call raya_en                ; ¿sobre la raya de un encabezado?
        mov eax, CUR_FLECHA
        jc .c
        mov eax, CUR_HORIZ
.c:
        cmp eax, [cur_ahora]
        je hecho
        mov [cur_ahora], eax
        mov ebp, [ven]
        HV cur_forma
        jmp hecho

; (interna) EAX=x EBX=y: ¿es la raya derecha del encabezado de una columna?
; -> CF=0 y ECX=la columna
raya_en:
        push eax
        push edx
        push esi
        cmp byte [vista], 0
        jne .no
        cmp dword [tab], 0
        je .no
        cmp ebx, TB_AL
        jl .no
        cmp ebx, GY0
        jge .no
        mov esi, eax
        add esi, [xdesp]            ; ESI=x en la tabla
        xor ecx, ecx
.b:
        cmp ecx, [n_cam]
        jae .no
        lea eax, [ecx+1]
        call col_x                  ; donde acaba la columna ECX
        mov edx, esi
        sub edx, eax
        add edx, 4
        cmp edx, 8
        jb .si
        inc ecx
        jmp .b
.si:
        clc
        jmp .x
.no:
        stc
.x:
        pop esi
        pop edx
        pop eax
        ret

; (interna) EAX=x: la columna de la rejilla que hay ahí -> ECX; CF=1 si ninguna
col_en:
        push eax
        push edx
        push esi
        mov esi, eax
        add esi, [xdesp]
        xor ecx, ecx
.b:
        cmp ecx, [n_cam]
        jae .no
        lea eax, [ecx+1]
        call col_x
        cmp esi, eax
        jb .si
        inc ecx
        jmp .b
.si:
        clc
        jmp .x
.no:
        stc
.x:
        pop esi
        pop edx
        pop eax
        ret

; ---- ratón: EDX=x + y*65536 ----
raton:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl .barra
        cmp dword [tab], 0
        je hecho
        cmp byte [vista], 0
        jne .ficha
        cmp ebx, GY0
        jge .fila
        ; el encabezado: en la raya, arrastrar el ancho; si no, ordenar
        call raya_en
        jc .ord
        mov [arr_col], ecx
        mov [arr_x0], eax
        mov eax, ecx
        call cam_ptr
        movzx eax, word [ebx+C_ANCHO]
        mov [arr_an0], eax
        mov ebp, [ven]
        HV ven_capturar
        jmp hecho
.ord:
        call col_en
        jc hecho
        mov [col], ecx
        mov byte [suma_ok], 0
        mov eax, ecx
        call ordenar_clic
        jmp hecho
.fila:
        call fila_en
        jc hecho
        mov [sel], edx
        call col_en
        jc .f1
        mov [col], ecx
        mov byte [suma_ok], 0
.f1:
        call inval
        jmp hecho
.ficha:
        call campo_en
        jc hecho
        mov [col], ecx
        mov byte [suma_ok], 0
        call inval
        jmp hecho
.barra:
        mov esi, botones
        call br_boton_en
        jc hecho
        mov dword [br_sobre], 0     ; (pulsado: su nombre ya no hace falta)
        mov ecx, [esi+12]
        jmp orden

; (interna) EBX=y: la fila de la rejilla -> EDX=su sitio en la vista; CF=1
fila_en:
        push eax
        push ecx
        mov eax, ebx
        sub eax, GY0
        js .no
        xor edx, edx
        mov ecx, FIL_AL
        div ecx
        cmp eax, [vis_fil]
        jae .no
        add eax, [top]
        cmp eax, [n_vis]
        jae .no
        mov edx, eax
        clc
        jmp .x
.no:
        stc
.x:
        pop ecx
        pop eax
        ret
; (interna) EBX=y: el campo de la ficha -> ECX; CF=1 si ninguno
campo_en:
        push eax
        push edx
        mov eax, ebx
        sub eax, FCAM_Y
        js .no
        xor edx, edx
        mov ecx, FCAM_AL
        div ecx
        cmp eax, [vis_cam]
        jae .no
        add eax, [ftop]
        cmp eax, [n_cam]
        jae .no
        mov ecx, eax
        clc
        jmp .x
.no:
        stc
.x:
        pop edx
        pop eax
        ret

; se suelta el botón: si se arrastraba una columna, su ancho queda guardado
raton_suelta:
        cmp dword [arr_col], -1
        je hecho
        mov dword [arr_col], -1
        mov ebp, [ven]
        HV ven_soltar
        call guarda
        call inval
        jmp hecho

doble:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl raton                    ; en la barra: otro clic
        cmp dword [tab], 0
        je hecho
        cmp byte [vista], 0
        jne .ficha
        cmp ebx, GY0
        jl raton                    ; en el encabezado: otro clic
        call fila_en
        jc hecho
        mov [sel], edx
        call col_en
        jc .a
        mov [col], ecx
.a:
        mov ecx, O_R_CAMB
        jmp orden
.ficha:
        call campo_en
        jc hecho
        mov [col], ecx
        mov ecx, O_R_CAMB
        jmp orden

%include "base.inc"
%include "vista.inc"
%include "dialogo.inc"
%include "salida.inc"
%include "barra.inc"
%include "hvplib.inc"

        section .bss
            BARRA_BSS
ven         resd 1
barra_v     resd 1
barra_h     resd 1
param       resd 1                  ; los parámetros con los que se abrió
dbuf        resd 1                  ; el búfer de paso de Hispa Ventanas
blq_mango   resd 1                  ; el bloque de memoria y sus partes:
ebufs       resd 1                  ;   los búferes de edición (uno por campo)
rbuf        resd 1                  ;   el registro que se monta
idx         resd 1                  ;   dónde está cada registro de la tabla
vis         resd 1                  ;   los registros que se ven, en su orden
arena       resd 1                  ;   la base (como en el archivo)
arena_fin   resd 1                  ;   hasta dónde llega el bloque
fin         resd 1                  ;   y hasta dónde, la base
tab         resd 1                  ; la tabla que se ve (0: no hay)
tab_i       resd 1                  ; y su número
n_cam       resd 1
n_reg       resd 1
n_vis       resd 1
sel         resd 1                  ; el registro elegido (su sitio en la vista)
sel_reg     resd 1
top         resd 1                  ; la primera fila que se ve
col         resd 1                  ; el campo elegido
xdesp       resd 1                  ; lo que está corrida la rejilla
ftop        resd 1                  ; el primer campo que se ve en la ficha
vis_fil     resd 1                  ; filas que caben
vis_cam     resd 1                  ; campos que caben en la ficha
cli_an      resd 1
cli_al      resd 1
arr_col     resd 1                  ; la columna que se ensancha (-1: ninguna)
arr_x0      resd 1
arr_an0     resd 1
cur_ahora   resd 1
hoy         resd 1                  ; aaaammdd
ord_c       resd 1
f_campo     resd 1                  ; el filtro: campo,
f_op        resd 1                  ;   comparación (0 contiene ... 4 vacío),
f_val       resd 1                  ;   el valor (número o fecha)
f_len       resd 1                  ;   y lo que mide el texto
suma_lo     resd 1                  ; la suma del campo elegido (64 bits)
suma_hi     resd 1
vista       resb 1                  ; 0: la rejilla; 1: la ficha
sucio       resb 1                  ; hay algo sin guardar (anchos, orden)
ord_d       resb 1
f_on        resb 1
fmt_mil     resb 1                  ; los números, con puntos de millares
suma_ok     resb 1
aviso_ini   resb 1
            alignb 4
rl          resb RL_TAM
tipos       resb MAX_CAM            ; el tipo y los decimales de cada campo
decs        resb MAX_CAM
pliega      resb 256                ; sin mayúsculas ni acentos, para comparar
f_txt       resb TXT_MAX + 4
busca_txt   resb 64
num_tmp     resb 32
bt_buf      resb 40
pie_buf     resb 200
nom_tmp     resb 128
ruta_bdd    resb 128
ruta_tmp    resb 128
ruta_ant    resb 128
ruta_def    resb 128
ruta_ini    resb 128
vbuf        resb MEMO_MAX + 104
            BASE_BSS
            VISTA_BSS
            DLG_BSS
            SAL_BSS
HVP_FIN
