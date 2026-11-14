; =============================================================================
;  ARCHIVOS.HVP - el Administrador de archivos de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Pedido de Carlos: «En ventanas, ¿tenemos explorador de archivos?». Una
;  ventana (redimensionable, maximizable; recuerda su sitio y su tamaño) con
;  barra de menús (Archivo, Edición, Ver, Ayuda), la barra de dirección (la
;  ruta, que se puede escribir, y el botón «Subir»), a la izquierda el árbol
;  de las unidades (A:, B: si hay, C:, D:..., el CD) con sus carpetas, a la
;  derecha lo que hay en la carpeta (vista Iconos, con un icono por tipo, o
;  Detalles: nombre, tamaño, tipo y fecha, ordenable por columna) y abajo la
;  barra de estado (objetos, tamaño, espacio libre).
;
;  Selección con clic, Mayús+clic, Ctrl+clic (si Ctrl+clic no hace de botón
;  derecho: Configuración > Ratón) o Ctrl+Espacio, y Ctrl+E (o Ctrl+A) para
;  todo. Copiar, cortar y pegar (Ctrl+C, X, V), arrastrar y soltar en la
;  lista y entre el árbol y la lista (en la misma unidad, mueve; a otra,
;  copia; con Ctrl, copia siempre), eliminar (Supr, pregunta antes), cambiar
;  el nombre (F2), carpeta nueva, propiedades (los atributos se cambian) y
;  buscar (* y ?, en la carpeta y sus subcarpetas). Lo largo sale con una
;  ventana de progreso que se puede cancelar, y pregunta antes de escribir
;  encima (Sí, No, Sí a todo, Cancelar). Los errores de disco (disquete sin
;  poner, protegido contra escritura, disco lleno) se dicen con palabras.
;
;  Abrir (doble clic o Intro): una carpeta entra; un .COM, .EXE o .BAT se
;  ejecuta como los accesos directos (Hispa Ventanas deja la memoria al
;  programa y vuelve, y el Administrador se abre otra vez en la misma
;  carpeta); un texto, en el Bloc de notas; un .BMP, en Pintura; un .WAV, en
;  el Reproductor de sonido; un .NES, en Hispa-Nes; un .SMS, en Hispa-System;
;  un .HVP, el programa. Usa la interfaz de [ARCHIVOS] (rm_interrup,
;  dos_buf, dos_crit y prog_abrir; docs/VENTANAS.md).
;
;  Todo es propio: el diseño, los iconos y los textos. Va en el grupo
;  «Principal» del Centro de programas (el 3 de la cabecera).
;
;  Parámetros: /R (lo pone él mismo al volver de un programa: se abre donde
;  estaba, con lo que se abrió marcado).
;  Guarda en ARCHIVOS.INI (al lado del módulo) la ventana, la vista, el
;  orden, el ancho del árbol y la carpeta.
; =============================================================================
%include "hvp.inc"

HVP_CABECERA "Administrador de archivos", entrada, 3
        db "................................"
        db "................................"
        db "..1111111111111111111111111111.."
        db "..1666666666666666666666666661.."
        db "..1611111111111111111111111161.."
        db "..1617777777777777777777777161.."
        db "..161711111.......11111111.161.."
        db "..16171aaaa1111111aaaaaaa1.161.."
        db "..16171aaaaaaaaaaaaaaaaaa1.1617."
        db "..1617111111111111111111117161.."
        db "..1616666666666666666666666161.."
        db "..1616666666611111166666666161.."
        db "..161666666661aaaa166666666161.."
        db "..1616666666611111166666666161.."
        db "..1611111111111111111111111161.."
        db "..1617777777777777777777777161.."
        db "..161711111.......11111111.161.."
        db "..16171cccc1111111ccccccc1.161.."
        db "..16171cccccccccccccccccc1.1617."
        db "..1617111111111111111111117161.."
        db "..1616666666666666666666666161.."
        db "..1616666666611111166666666161.."
        db "..161666666661aaaa166666666161.."
        db "..1616666666611111166666666161.."
        db "..1611111111111111111111111161.."
        db "..1666666666666666666666666661.."
        db "..1111111111111111111111111111.."
        db "..11.....................11...7."
        db "..11.....................11...7."
        db "...777777777777777777777777777.."
        db "................................"
        db "................................"
HVP_CODIGO

; ---- medidas ----
DIR_H       equ 28                  ; la barra de dirección
EST_H       equ 20                  ; la barra de estado
SEP         equ 5                   ; el separador entre el árbol y la lista
FILA_A      equ 18                  ; una fila del árbol
FILA_D      equ 18                  ; una fila de la vista Detalles
CAB_D       equ 20                  ; la cabecera de las columnas
CEL_AN      equ 104                 ; una casilla de la vista Iconos
CEL_AL      equ 62
SANGRIA     equ 14                  ; lo que entra cada nivel del árbol
MAX_ENT     equ 1000                ; lo que se ve de una carpeta
ENT_TAM     equ 32
E_NOM       equ 0                   ; 13: «NOMBRE.EXT»
E_ATR       equ 13
E_TIPO      equ 14
E_MARCA     equ 15                  ; bit 0 elegido, bit 1 cortado
E_TAM       equ 16
E_HORA      equ 20
E_FECHA     equ 22
E_DIR       equ 24                  ; (búsqueda) su carpeta; 0: la de la ventana
MAX_NODOS   equ 600                 ; filas del árbol
NODO_TAM    equ 16
N_NOM       equ 0                   ; 13 (en una unidad, «A:»)
N_NIVEL     equ 13
N_BAND      equ 14
N_UTIPO     equ 15                  ; una unidad: 0 disquete, 1 disco, 2 CD o extraíble
NB_ABIERTO  equ 1
NB_VACIO    equ 2                   ; ya se sabe que no tiene carpetas
NB_UNIDAD   equ 4
PP_MAX      equ 512                 ; lo que se copia o se mueve de una vez
RUTA_TAM    equ 80
POOL_TAM    equ 32768               ; las carpetas de los resultados de buscar
TI_CARP     equ 0                   ; los tipos (y sus iconos)
TI_PROG     equ 1
TI_ORD      equ 2
TI_TEXTO    equ 3
TI_IMAGEN   equ 4
TI_SONIDO   equ 5
TI_CART     equ 6
TI_MOD      equ 7
TI_DESC     equ 8
TI_N        equ 9
IC_UNIDAD   equ 9                   ; (los pequeños) 9, 10 y 11: las unidades
ID_DIR      equ 20                  ; los controles
ID_SUBIR    equ 21
ID_ARBOL    equ 22
ID_LISTA    equ 23
ID_BARRA_A  equ 24
ID_BARRA_L  equ 25
ID_TODOS    equ 8                   ; (respuestas de los diálogos propios)
ID_REINT    equ 4
ID_OMITIR   equ 5
; las órdenes
O_ABRIR     equ 101
O_EDITAR    equ 102
O_NUEVA     equ 103
O_BUSCAR    equ 104
O_SALIR     equ 105
O_SUBIR     equ 106
O_IR        equ 107                 ; (la ruta escrita)
O_CORTAR    equ 111
O_COPIAR    equ 112
O_PEGAR     equ 113
O_ELIMINAR  equ 114
O_NOMBRE    equ 115
O_PROPIED   equ 116
O_TODO      equ 117
O_INVERTIR  equ 118
O_ICONOS    equ 121
O_DETALLES  equ 122
O_ACTUAL    equ 123
O_ORD_NOM   equ 131                 ; 131-134: por nombre, tamaño, tipo y fecha
O_ORD_INV   equ 135
O_DESPLEGAR equ 141
O_AYUDA     equ 151
O_ACERCA    equ 152
O_ESC       equ 160                 ; (Esc: deja de arrastrar o vuelve la dirección)
TC_F2       equ 0x3C
PA_VOLVER   equ 1                   ; (prog_abrir) volver a Hispa Ventanas al acabar
PA_PAUSA    equ 2                   ;   y esperar una tecla
TC_F3       equ 0x3D
TC_F5       equ 0x3F
TC_F7       equ 0x41

c_archivos  dd arc_proc
            db SIN_FONDO, 0, 0, 0
c_arbol     dd arbol_proc
            db C_BLANCO, CB_FOCO, 0, 0
c_lista_a   dd lista_proc
            db C_BLANCO, CB_FOCO, 0, 0
c_progreso  dd prog_proc
            db C_CREMA, 0, 0, 0
arc_plant:
        PLANTILLA c_archivos, t_titulo, E_NORMAL, 60, 40, 620, 420, 0
prog_plant:
        PLANTILLA c_progreso, t_progreso, E_TITULO | E_CERRAR, 0, 0, 420, 170, 0
; los hijos (se colocan en disponer)
hijos:
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 0, 0, 100, 20, ID_DIR
        PLANTILLA HV_c_boton, t_subir, E_HIJA | E_TABULA, 0, 0, 64, 20, ID_SUBIR
        PLANTILLA c_arbol, 0, E_HIJA | E_TABULA, 0, 0, 100, 100, ID_ARBOL
        PLANTILLA c_lista_a, 0, E_HIJA | E_TABULA, 0, 0, 100, 100, ID_LISTA
        dd 0
barra_a_pl:
        PLANTILLA HV_c_barra, 0, E_HIJA | E_OCULTA, 0, 0, BD_AN, 50, ID_BARRA_A
        dd 0
barra_l_pl:
        PLANTILLA HV_c_barra, 0, E_HIJA | E_OCULTA, 0, 0, BD_AN, 50, ID_BARRA_L
        dd 0
prog_hijos:
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA | BS_DEFECTO, 158, 108, 100, 24, ID_CANCELAR
        dd 0

arc_menu:
        db "Archivo", 0
        dw O_ABRIR
        db "&Abrir                 Intro", 0
        dw O_EDITAR
        db "&Editar (Bloc de notas)", 0
        dw O_NUEVA
        db "&Nueva carpeta...      F7", 0
        dw 0
        db 0
        dw O_ELIMINAR
        db "E&liminar...           Supr", 0
        dw O_NOMBRE
        db "&Cambiar nombre...     F2", 0
        dw O_PROPIED
        db "&Propiedades...", 0
        dw 0
        db 0
        dw O_BUSCAR
        db "&Buscar...             F3", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw O_CORTAR
        db "Cor&tar                Ctrl+X", 0
        dw O_COPIAR
        db "&Copiar                Ctrl+C", 0
        dw O_PEGAR
        db "&Pegar                 Ctrl+V", 0
        dw 0
        db 0
        dw O_TODO
        db "&Seleccionar todo      Ctrl+E", 0
        dw O_INVERTIR
        db "&Invertir la selección", 0
        dw 0xFFFF
        db "&Ver", 0
        dw O_ICONOS
mv_iconos   db "  &Iconos", 0
        dw O_DETALLES
mv_detalles db "  &Detalles", 0
        dw 0
        db 0
        dw O_ORD_NOM
mv_o0       db "  Por &nombre", 0
        dw O_ORD_NOM + 1
mv_o1       db "  Por &tamaño", 0
        dw O_ORD_NOM + 2
mv_o2       db "  Por ti&po", 0
        dw O_ORD_NOM + 3
mv_o3       db "  Por &fecha", 0
        dw O_ORD_INV
mv_inv      db "  Orden &inverso", 0
        dw 0
        db 0
        dw O_ACTUAL
        db "&Actualizar            F5", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw O_AYUDA
        db "&Cómo se usa...", 0
        dw O_ACERCA
        db "&Acerca del Administrador de archivos...", 0
        dw 0xFFFF
        db 0
mv_marcas   dd mv_iconos, mv_detalles, mv_o0, mv_o1, mv_o2, mv_o3, mv_inv

; los menús contextuales
ctx_ent:                            ; un archivo o una carpeta (en la lista)
        dw O_ABRIR
        db "&Abrir", 0
        dw O_EDITAR
        db "&Editar", 0
        dw 0
        db 0
        dw O_CORTAR
        db "Cor&tar         Ctrl+X", 0
        dw O_COPIAR
        db "&Copiar         Ctrl+C", 0
        dw O_PEGAR
        db "&Pegar          Ctrl+V", 0
        dw 0
        db 0
        dw O_ELIMINAR
        db "E&liminar       Supr", 0
        dw O_NOMBRE
        db "Cambiar &nombre F2", 0
        dw 0
        db 0
        dw O_PROPIED
        db "P&ropiedades", 0
        dw 0xFFFF
ctx_fondo:                          ; el fondo de la lista
        dw O_ICONOS
        db "Ver &iconos", 0
        dw O_DETALLES
        db "Ver &detalles", 0
        dw 0
        db 0
        dw O_ORD_NOM
        db "Ordenar por &nombre", 0
        dw O_ORD_NOM + 1
        db "Ordenar por &tamaño", 0
        dw O_ORD_NOM + 2
        db "Ordenar por ti&po", 0
        dw O_ORD_NOM + 3
        db "Ordenar por &fecha", 0
        dw 0
        db 0
        dw O_PEGAR
        db "&Pegar          Ctrl+V", 0
        dw O_NUEVA
        db "Nueva &carpeta...", 0
        dw O_TODO
        db "&Seleccionar todo", 0
        dw O_ACTUAL
        db "&Actualizar     F5", 0
        dw 0xFFFF
ctx_nodo:                           ; una carpeta o una unidad del árbol
        dw O_DESPLEGAR
        db "&Desplegar o plegar", 0
        dw O_NUEVA
        db "Nueva &carpeta...", 0
        dw 0
        db 0
        dw O_CORTAR
        db "Cor&tar         Ctrl+X", 0
        dw O_COPIAR
        db "&Copiar         Ctrl+C", 0
        dw O_PEGAR
        db "&Pegar          Ctrl+V", 0
        dw 0
        db 0
        dw O_ELIMINAR
        db "E&liminar       Supr", 0
        dw O_NOMBRE
        db "Cambiar &nombre F2", 0
        dw 0
        db 0
        dw O_PROPIED
        db "P&ropiedades", 0
        dw 0xFFFF

            align 4
; las órdenes y quién las hace
tab_orden   dd O_ABRIR, o_abrir, O_EDITAR, o_editar, O_NUEVA, o_nueva, O_BUSCAR, o_buscar
            dd O_SALIR, o_salir, O_SUBIR, o_subir, O_IR, o_ir, O_CORTAR, o_cortar
            dd O_COPIAR, o_copiar, O_PEGAR, o_pegar, O_ELIMINAR, o_eliminar
            dd O_NOMBRE, o_nombre, O_PROPIED, o_propiedades, O_TODO, o_todo
            dd O_INVERTIR, o_invertir, O_ICONOS, o_iconos, O_DETALLES, o_detalles
            dd O_ACTUAL, o_actualizar, O_ORD_NOM, o_orden, O_ORD_NOM + 1, o_orden
            dd O_ORD_NOM + 2, o_orden, O_ORD_NOM + 3, o_orden, O_ORD_INV, o_inverso
            dd O_DESPLEGAR, o_desplegar, O_AYUDA, o_ayuda, O_ACERCA, o_acerca
            dd O_ESC, o_esc, 0
; las teclas que son una orden (en la lista y en el árbol)
tab_tecla   dd TC_SUPR, O_ELIMINAR, TC_F2, O_NOMBRE, TC_F3, O_BUSCAR, TC_F5, O_ACTUAL, TC_F7, O_NUEVA
            dd TC_CTRL + 3, O_COPIAR, TC_CTRL + 24, O_CORTAR, TC_CTRL + 22, O_PEGAR
            dd TC_CTRL_INS, O_COPIAR, TC_INS, O_PEGAR
            dd TC_CTRL + 5, O_TODO, TC_CTRL + 1, O_TODO, TC_CTRL + 2, O_BUSCAR
            dd TC_RETRO, O_SUBIR, TC_ESC, O_ESC, 0

; las extensiones conocidas: tres letras y el tipo
tab_ext     db "COM", TI_PROG, "EXE", TI_PROG, "BAT", TI_ORD
            db "TXT", TI_TEXTO, "INI", TI_TEXTO, "LOG", TI_TEXTO, "CFG", TI_TEXTO
            db "ASM", TI_TEXTO, "INC", TI_TEXTO, "DIZ", TI_TEXTO, "NFO", TI_TEXTO
            db "LST", TI_TEXTO, "CSV", TI_TEXTO, "ME", 0, TI_TEXTO, "1ST", TI_TEXTO
            db "MD", 0, TI_TEXTO, "BMP", TI_IMAGEN, "WAV", TI_SONIDO
            db "NES", TI_CART, "SMS", TI_CART, "HVP", TI_MOD, 0
            align 4
nom_tipos   dd t_ti0, t_ti1, t_ti2, t_ti3, t_ti4, t_ti5, t_ti6, t_ti7, t_ti8
t_ti0       db "Carpeta", 0
t_ti1       db "Programa", 0
t_ti2       db "Por lotes", 0
t_ti3       db "Texto", 0
t_ti4       db "Imagen", 0
t_ti5       db "Sonido", 0
t_ti6       db "Cartucho", 0
t_ti7       db "Módulo", 0
t_ti8       db "Archivo", 0
nom_unid    dd t_un0, t_un1, t_un2
t_un0       db "Disquete", 0
t_un1       db "Disco", 0
t_un2       db "CD", 0
t_un2b      db "Extraíble", 0

t_titulo    db "Administrador de archivos", 0
t_progreso  db "Administrador de archivos", 0
t_direccion db "Dirección", 0
t_subir     db "Subir", 0
t_cancelar  db "Cancelar", 0
t_aceptar   db "Aceptar", 0
t_si        db "Sí", 0
t_no        db "No", 0
t_si_todos  db "Sí a todo", 0
t_reint     db "Reintentar", 0
t_omitir    db "Omitir", 0
t_patron    db "*.*", 0
t_c_nombre  db "Nombre", 0
t_c_tam     db "Tamaño", 0
t_c_tipo    db "Tipo", 0
t_c_fecha   db "Fecha", 0
t_c_dondes  db "Carpeta", 0
t_vacia     db "(Esta carpeta está vacía.)", 0
t_nada      db "(No se ha encontrado nada.)", 0
t_objetos   db " objetos", 0
t_objeto    db " objeto", 0
t_selecc    db " seleccionados", 0
t_selec1    db " seleccionado", 0
t_octetos   db " octetos", 0
t_kb        db " KB", 0
t_libre     db "Libre: ", 0
t_de        db " de ", 0
t_coma_sp   db ", ", 0
t_resultados db "Búsqueda de ", 0
t_en        db " en ", 0
t_mover_a   db "Mover a ", 0
t_copiar_a  db "Copiar a ", 0
t_copiando  db "Copiando ", 0
t_moviendo  db "Moviendo ", 0
t_eliminando db "Eliminando ", 0
t_buscando  db "Buscando en ", 0
t_contando  db "Contando lo que hay que copiar...", 0
t_archivos_ db " archivos", 0
t_hechos    db "Hecho: ", 0
t_e_protegido db "El disco está protegido contra escritura.", 0
t_e_preparada db "La unidad no está preparada: ¿falta el disquete o el CD?", 0
t_e_disco   db "No se ha podido leer o escribir el disco.", 0
t_e_nohay   db "No se encuentra el archivo.", 0
t_e_noruta  db "No se encuentra la carpeta.", 0
t_e_acceso  db "No se permite: puede ser de solo lectura, una carpeta que no", 10
            db "está vacía, un nombre que ya existe o un CD.", 0
t_e_lleno   db "El disco está lleno.", 0
t_e_existe  db "Ya hay un archivo con ese nombre.", 0
t_e_actual  db "No se puede quitar la carpeta en la que se está.", 0
t_e_compartido db "El archivo está abierto en otro programa.", 0
t_e_nombre  db "Ese nombre no vale en Hispa-DOS: hasta 8 letras, un punto y", 10
            db "3 letras más, sin espacios ni los signos  \ / : * ? " ,'"', " < > | + , ; = [ ]", 0
t_e_dentro  db "No se puede poner una carpeta dentro de sí misma.", 0
t_e_misma   db "El destino es la misma carpeta en la que ya está.", 0
t_e_muchos  db "Hay demasiados elementos: se hacen los primeros 512.", 0
t_e_ruta    db "Esa dirección no es válida. Escriba una ruta como C:\JUEGOS.", 0
t_e_larga   db "La ruta es demasiado larga para Hispa-DOS.", 0
t_e_noabre  db "No se sabe con qué programa se abre este archivo.", 10
            db "¿Quiere verlo en el Bloc de notas?", 0
t_e_unidad  db "No se puede leer la unidad ", 0
t_e_falta   db "No se encuentra el programa que lo abre:", 10, 0
t_no_copia  db "No se ha podido copiar ", 0
t_no_mover  db "No se ha podido mover ", 0
t_no_borrar db "No se ha podido eliminar ", 0
t_no_crear  db "No se ha podido crear la carpeta.", 10, 0
t_no_nombre db "No se ha podido cambiar el nombre.", 10, 0
t_no_atr    db "No se han podido cambiar los atributos.", 10, 0
t_comillas1 db "«", 0
t_comillas2 db "»", 0
t_dos_puntos db ":", 10, 0
t_sobrescr1 db "Ya existe «", 0
t_sobrescr2 db "» en la carpeta de destino.", 10, "¿Quiere poner encima el que se copia?", 0
t_borrar1   db "¿Quiere eliminar «", 0
t_borrar2   db "»?", 0
t_borrar_n1 db "¿Quiere eliminar estos ", 0
t_borrar_n2 db " elementos?", 0
t_borrar_c  db 10, "Se borra también todo lo que hay dentro de las carpetas.", 0
t_t_borrar  db "Eliminar", 0
t_t_sobre   db "Ya existe", 0
t_t_nombre  db "Cambiar nombre", 0
t_p_nombre  db "Nombre nuevo:", 0
t_t_nueva   db "Nueva carpeta", 0
t_p_nueva   db "Nombre de la carpeta nueva:", 0
t_nueva_def db "NUEVA", 0
t_t_buscar  db "Buscar", 0
t_p_buscar  db "Nombre (vale * y ?), aquí y dentro:", 0
t_t_props   db "Propiedades", 0
t_p_nom     db "Nombre:", 0
t_p_tipo    db "Tipo:", 0
t_p_donde   db "Carpeta:", 0
t_p_tam     db "Tamaño:", 0
t_p_cont    db "Contiene:", 0
t_p_fecha   db "Fecha:", 0
t_p_libre   db "Libre:", 0
t_p_total   db "Total:", 0
t_p_atr     db "Atributos", 0
t_a_ro      db "Solo lectura", 0
t_a_oc      db "Oculto", 0
t_a_si      db "Sistema", 0
t_a_ar      db "Archivo", 0
t_varios    db " elementos", 0
t_carpetas_ db " carpetas", 0
t_ayuda_tit db "Cómo se usa el Administrador de archivos", 0
t_ayuda     db "A la izquierda, las unidades y sus carpetas: un clic en una la abre,", 10
            db "y el cuadradito (o las flechas izquierda y derecha) la despliega.", 10
            db "A la derecha, lo que hay dentro. Doble clic o Intro: abrir (un", 10
            db "programa se ejecuta, un texto va al Bloc de notas, un BMP a Pintura,", 10
            db "un WAV al Reproductor, un cartucho a su emulador).", 10
            db 10
            db "Elegir: clic; Mayús+clic, un trozo; Ctrl+Espacio, uno más; Ctrl+E,", 10
            db "todo. Ctrl+C copia, Ctrl+X corta y Ctrl+V pega. Arrastrar lo elegido", 10
            db "a una carpeta lo mueve (a otra unidad, lo copia; con Ctrl, copia).", 10
            db "Supr elimina, F2 cambia el nombre, F3 busca, F5 actualiza y", 10
            db "Retroceso sube a la carpeta de arriba. Tab pasa del árbol a la", 10
            db "lista y a la dirección, donde se puede escribir una ruta.", 10
            db "El botón derecho (o Mayús+F10) da el menú de lo que hay debajo.", 0
t_acerca    db "Administrador de archivos", 10, "de Hispa Ventanas.", 10, 10
            db "Copiar, mover, ordenar y buscar los archivos de los discos.", 0
n_bloc      db ":BLOC", 0
n_pintura   db "PINTURA.HVP", 0
n_sonidos   db "SONIDOS.HVP", 0
n_hispanes  db "JUEGOS\HISPANES.HVP", 0
n_hispasms  db "JUEGOS\HISPASMS.HVP", 0
t_param_r   db " /R", 0

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        mov [param], edi
        mov edi, mi_ruta
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov eax, [hv_api + HV_dos_buf * 4]
        mov [dbuf], eax
        ; las carpetas de los otros programas: la mía (PROGS) y la de arriba
        ; (la de VENTANAS.COM)
        mov esi, mi_ruta
        mov edi, d_progs
        call copiar_n
        mov esi, d_progs
        call ruta_subir
        mov edi, d_hisp
        call copiar_n
        mov esi, d_hisp
        call ruta_subir
        mov esi, mi_ruta            ; ARCHIVOS.INI
        mov edi, ini_ruta
        mov eax, 'INI'
        call hvl_ruta
        mov esi, hijos
        call hvl_clases
        mov esi, barra_a_pl
        call hvl_clases
        mov esi, barra_l_pl
        call hvl_clases
        mov esi, prog_hijos
        call hvl_clases
        call iconos_hacer
        ; lo de siempre y, encima, lo de ARCHIVOS.INI
        mov dword [vista], 0
        mov dword [orden], 0
        mov dword [ta], 0
        HV pantalla
        cmp eax, 800
        jae .p
        mov dword [arc_plant+P_X], 10
        mov dword [arc_plant+P_Y], 34
.p:
        call ini_leer
        call marcas_menu
        mov esi, arc_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        mov ebp, eax
        cmp dword [ini_max], 0
        je .nm
        mov eax, 3
        HV ven_mostrar
.nm:
        call unidades_buscar
        ; la carpeta: la de ARCHIVOS.INI o la raíz de la unidad de Hispa Ventanas
        cmp byte [ini_carpeta], 0
        je .raiz
        mov esi, ini_carpeta
        call ir_a_quieto
        jnc .dentro
.raiz:
        mov al, [mi_ruta]
        mov [tmp], al
        mov word [tmp+1], ':\'
        mov byte [tmp+3], 0
        mov esi, tmp
        call ir_a_quieto
        jnc .dentro
        mov dword [tmp], 'C:\'      ; (y si no, C:)
        mov esi, tmp
        call ir_a_quieto
.dentro:
        cmp byte [ini_marcado], 0   ; lo que estaba marcado (al volver de un programa)
        je .b
        mov esi, ini_marcado
        call lista_marcar_nombre
.b:
        mov ebp, [v_lista]
        HV ven_enfocar
        HV app_bucle
.x:
        ret

; (interna) los iconos, de letras a colores (CLAVE: transparente)
iconos_hacer:
        pushad
        mov esi, ico32
        mov ecx, TI_N * 1024 + (TI_N + 3) * 256
.i:
        mov al, [esi]
        cmp al, '.'
        jne .i1
        mov al, CLAVE
        jmp .i3
.i1:
        cmp al, 'a'
        jb .i2
        sub al, 'a' - '9' - 1
.i2:
        sub al, '0'
.i3:
        mov [esi], al
        inc esi
        dec ecx
        jnz .i
        popad
        ret

; (interna) «»» delante de la vista y del orden elegidos, en el menú Ver
marcas_menu:
        pushad
        xor ecx, ecx
.m:
        mov edi, [mv_marcas+ecx*4]
        mov al, ' '
        cmp ecx, 2
        jae .o
        cmp ecx, [vista]
        jne .p
        mov al, 0xAF
        jmp .p
.o:
        cmp ecx, 6
        je .inv
        lea edx, [ecx-2]
        cmp edx, [orden]
        jne .p
        mov al, 0xAF
        jmp .p
.inv:
        cmp dword [inverso], 0
        je .p
        mov al, 0xAF
.p:
        mov [edi], al
        inc ecx
        cmp ecx, 7
        jb .m
        popad
        ret

; =============================================================================
;  La ventana
; =============================================================================
arc_proc:
        cmp eax, WM_PAINT
        je arc_pintar
        cmp eax, WM_COMMAND
        je arc_orden
        cmp eax, WM_KEYDOWN
        je arc_tecla
        cmp eax, WM_CONTEXTMENU
        je arc_ctx
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_MOUSEMOVE
        je sep_mueve
        cmp eax, WM_LBUTTONDOWN
        je sep_pulsa
        cmp eax, WM_LBUTTONUP
        je sep_suelta
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_EXITSIZEMOVE
        je .soltada
        cmp eax, WM_GETMINMAXINFO
        jne .nmin
        mov eax, 380 + 260 * 65536
        ret
.nmin:
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov [dlg_dueno], ebp
        mov dword [ebp+V_MENU], arc_menu
        mov dword [ebp+V_ICONO], hvp_icono
        mov esi, hijos
        HV ven_lista
        mov ecx, ID_DIR
        HV ven_hija_id
        mov [v_dir], eax
        mov ecx, ID_SUBIR
        HV ven_hija_id
        mov [v_subir], eax
        mov ecx, ID_ARBOL
        HV ven_hija_id
        mov [v_arbol], eax
        mov ecx, ID_LISTA
        HV ven_hija_id
        mov [v_lista], eax
        push ebp
        mov ebp, [v_arbol]
        mov esi, barra_a_pl
        HV ven_lista
        mov ecx, ID_BARRA_A
        HV ven_hija_id
        mov [v_barra_a], eax
        mov ebp, [v_lista]
        mov esi, barra_l_pl
        HV ven_lista
        mov ecx, ID_BARRA_L
        HV ven_hija_id
        mov [v_barra_l], eax
        mov ebp, eax
        mov eax, BDM_LINEA
        mov ecx, FILA_D
        HV msg_enviar
        mov ebp, [v_barra_a]
        mov eax, BDM_LINEA
        HV msg_enviar
        pop ebp
        call disponer
        jmp hecho
.tam:
        call disponer
        HV ven_invalidar
        mov eax, [ebp+V_ESTADO]     ; maximizada o no: se apunta
        and eax, S_MAXIM
        cmp eax, [max_ant]
        je hecho
        mov [max_ant], eax
        call ini_guardar
        jmp hecho
.soltada:
        call ini_guardar
        jmp hecho
.foco:
        mov ebp, [v_lista]          ; el foco, al panel que lo tenía
        cmp dword [panel_foco], 1
        jne .f1
        mov ebp, [v_arbol]
.f1:
        HV ven_enfocar
        jmp hecho
.cerrar:
        cmp dword [ocupado], 0
        jne hecho
        call ini_guardar
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        mov dword [ven], 0
        HV msg_salir
        jmp hecho

hecho:
        xor eax, eax
        ret

; ---- dónde va cada cosa ----
disponer:
        pushad
        mov ebp, [ven]
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        mov eax, [ta]               ; el ancho del árbol
        test eax, eax
        jnz .t
        mov eax, ecx
        imul eax, 3
        xor edx, edx
        mov ebx, 10
        div ebx
        cmp eax, 200
        jbe .t
        mov eax, 200
.t:
        mov ebx, [cli_an]
        sub ebx, 150 + SEP
        cmp eax, ebx
        jle .t1
        mov eax, ebx
.t1:
        cmp eax, 90
        jge .t2
        mov eax, 90
.t2:
        mov [ta_real], eax
        ; la dirección
        mov ebp, [v_dir]
        mov eax, 84
        mov ebx, 4
        mov ecx, [cli_an]
        sub ecx, 84 + 74
        mov edx, 20
        HV ven_mover
        mov ebp, [v_subir]
        mov eax, [cli_an]
        sub eax, 68
        mov ecx, 62
        HV ven_mover
        ; los paneles
        mov edx, [cli_al]
        sub edx, DIR_H + EST_H
        jns .p
        xor edx, edx
.p:
        mov [panel_al], edx
        mov ebp, [v_arbol]
        xor eax, eax
        mov ebx, DIR_H
        mov ecx, [ta_real]
        HV ven_mover
        mov ebp, [v_lista]
        mov eax, [ta_real]
        add eax, SEP
        mov ecx, [cli_an]
        sub ecx, eax
        jns .l
        xor ecx, ecx
.l:
        HV ven_mover
        popad
        call arbol_medidas
        call lista_medidas
        ret

; ---- la ventana: la barra de dirección, el separador y la de estado ----
arc_pintar:
        HV dc_pintar
        COLOR C_ARENA
        xor eax, eax
        xor ebx, ebx
        mov ecx, [cli_an]
        mov edx, DIR_H
        HV dc_rect
        COLOR C_ARENA_OSC
        mov ebx, DIR_H - 1
        mov edx, 1
        HV dc_rect
        COLOR C_TINTA
        mov eax, 6
        mov ebx, 6
        mov esi, t_direccion
        HV dc_texto
        ; el separador
        COLOR C_ARENA
        mov eax, [ta_real]
        mov ebx, DIR_H
        mov ecx, SEP
        mov edx, [panel_al]
        HV dc_rect
        COLOR C_ARENA_OSC
        add eax, 2
        mov ecx, 1
        HV dc_rect
        ; la barra de estado
        COLOR C_ARENA
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, EST_H
        mov ecx, [cli_an]
        mov edx, EST_H
        HV dc_rect
        COLOR C_ARENA_OSC
        mov edx, 1
        HV dc_rect
        call estado_pintar
        HV dc_cerrar
        jmp hecho

; (interna, dentro de un contexto abierto) los textos de la barra de estado
estado_pintar:
        pushad
        COLOR C_ARENA
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, EST_H - 1
        mov ecx, [cli_an]
        mov edx, EST_H - 1
        HV dc_rect
        COLOR C_TINTA
        mov eax, 6
        add ebx, 2
        mov esi, st_izq
        call largo
        mov ecx, [cli_an]
        sub ecx, 12
        shr ecx, 3
        cmp eax, ecx
        jbe .n
        mov eax, ecx
.n:
        mov ecx, eax
        mov eax, 6
        HV dc_texto_n
        mov esi, st_der             ; el espacio libre, a la derecha
        call largo
        test eax, eax
        jz .x
        shl eax, 3
        neg eax
        add eax, [cli_an]
        sub eax, 8
        mov ecx, eax                ; (si no cabe con lo de la izquierda, no sale)
        mov esi, st_izq
        call largo
        lea eax, [eax*8+24]
        cmp ecx, eax
        jl .x
        mov eax, ecx
        mov esi, st_der
        HV dc_texto
.x:
        popad
        ret

; la barra de estado, de nuevo
estado_repintar:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, EST_H
        mov ecx, [cli_an]
        mov edx, EST_H
        HV ven_invalidar_rect
.x:
        popad
        ret

; ---- el separador se arrastra: el ancho del árbol ----
sep_en:                             ; EDX=x,y -> CF=0 si está en el separador
        movsx eax, dx
        sub eax, [ta_real]
        cmp eax, -2
        jl .no
        cmp eax, SEP + 2
        jg .no
        mov eax, edx
        sar eax, 16
        cmp eax, DIR_H
        jl .no
        cmp eax, [cli_al]
        jge .no
        clc
        ret
.no:
        stc
        ret
sep_pulsa:
        call sep_en
        jc hecho
        mov dword [sep_arr], 1
        HV ven_capturar
        jmp hecho
sep_mueve:
        cmp dword [sep_arr], 0
        jne .arr
        call sep_en
        mov eax, CUR_FLECHA
        jc .c
        mov eax, CUR_HORIZ
.c:
        HV cur_forma
        jmp hecho
.arr:
        movsx eax, dx
        sub eax, SEP / 2
        cmp eax, [ta_real]
        je hecho
        mov [ta], eax
        call disponer
        HV ven_invalidar
        jmp hecho
sep_suelta:
        cmp dword [sep_arr], 0
        je hecho
        mov dword [sep_arr], 0
        HV ven_soltar
        mov eax, [ta_real]
        mov [ta], eax
        call ini_guardar
        jmp hecho

; ---- las órdenes ----
arc_orden:
        cmp dword [ocupado], 0
        jne hecho
        mov eax, ecx
        shr eax, 16
        test eax, eax
        jz .menu
        cmp cx, ID_SUBIR            ; el botón «Subir»
        jne hecho
        mov ecx, O_SUBIR
.menu:
        movzx ecx, cx
        mov esi, tab_orden
.b:
        mov eax, [esi]
        test eax, eax
        jz hecho
        cmp eax, ecx
        je .h
        add esi, 8
        jmp .b
.h:
        call [esi+4]
        jmp hecho

; las teclas que suben de los hijos (y las de la dirección)
arc_tecla:
        cmp dword [ocupado], 0
        jne hecho
        cmp dword [panel_foco], 0
        jne .panel
        cmp ecx, TC_INTRO           ; en la dirección: Intro va, Esc la deja
        jne .d1
        call o_ir
        jmp hecho
.d1:
        cmp ecx, TC_ESC
        jne .panel
        call dir_poner
        mov ebp, [v_lista]
        HV ven_enfocar
        jmp hecho
.panel:
        call tecla_orden
        jmp hecho

; (interna) ECX=tecla: si es una orden, se hace -> CF=0
tecla_orden:
        mov esi, tab_tecla
.b:
        mov eax, [esi]
        test eax, eax
        jz .no
        cmp eax, ecx
        je .h
        add esi, 8
        jmp .b
.h:
        mov ecx, [esi+4]
        mov esi, tab_orden
.c:
        mov eax, [esi]
        test eax, eax
        jz .no
        cmp eax, ecx
        je .hc
        add esi, 8
        jmp .c
.hc:
        call [esi+4]
        clc
        ret
.no:
        stc
        ret

; ---- el menú del botón derecho: ECX=la ventana EDX=el punto (-1: teclado) ----
arc_ctx:
        cmp dword [ocupado], 0
        jne hecho
        cmp ecx, [v_arbol]
        je .arbol
        cmp ecx, [v_lista]
        je .lista
        HVJ ven_defecto
.lista:
        mov dword [ctx_en], 2
        call n_elegidos
        test eax, eax
        jz .fondo
        mov esi, ctx_ent
        xor ecx, ecx
        cmp dword [pp_n], 0         ; sin nada copiado, «Pegar» en gris
        jne .l1
        or ecx, 1 << 5
.l1:
        cmp eax, 1                  ; con varios, «Cambiar nombre» y «Abrir» no
        je .l2
        or ecx, (1 << 9) | 1 | 2
.l2:
        shl edx, 5                  ; una carpeta no se edita
        test byte [ents+edx+E_ATR], 0x10
        jz .menu
        or ecx, 2
        jmp .menu
.fondo:
        mov esi, ctx_fondo
        xor ecx, ecx
        cmp dword [pp_n], 0
        jne .f1
        or ecx, 1 << 8
.f1:
        cmp dword [modo_busca], 0
        je .menu
        or ecx, (1 << 8) | (1 << 9)
        jmp .menu
.arbol:
        mov dword [ctx_en], 1
        mov esi, ctx_nodo
        xor ecx, ecx
        cmp dword [pp_n], 0
        jne .a1
        or ecx, 1 << 5
.a1:
        mov eax, [sel_nodo]
        call nodo_ptr
        test byte [eax+N_BAND], NB_UNIDAD
        jz .menu
        or ecx, (1 << 3) | (1 << 4) | (1 << 7) | (1 << 8)
.menu:
        mov ebp, [ven]
        mov eax, -1
        HV ctx_menu
        jmp hecho

; =============================================================================
;  Ir a una carpeta
; =============================================================================
; ESI=ruta: la lee y la enseña (si no se puede, lo dice y se queda donde
; estaba) -> CF=1 si no se ha podido
ir_a:
        pushad
        mov byte [avisar], 1
        jmp ir_a_comun
; lo mismo sin avisar de los errores (al empezar)
ir_a_quieto:
        pushad
        mov byte [avisar], 0
ir_a_comun:
        mov edi, tmp_ruta
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov esi, tmp_ruta
        call ruta_limpia
        jc .mal
        call largo
        cmp eax, 66
        ja .larga
.otra:
        call crit_olvidar
        mov esi, tmp_ruta
        call leer_carpeta
        jnc .bien
        cmp byte [avisar], 0
        je .no
        call err_texto              ; con una unidad que no está lista, se puede
        mov edi, t_e_unidad         ; volver a probar
        call aviso_ruta
        cmp eax, ID_REINT
        je .otra
        jmp .no
.bien:
        mov esi, tmp_ruta
        mov edi, ruta
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov dword [modo_busca], 0
        call ordenar
        mov dword [foco_i], 0
        mov dword [ancla], 0
        mov dword [desp_l], 0
        call dir_poner
        call libre_mirar
        call arbol_seguir
        call lista_medidas
        call lista_inv
        call estado_poner
        popad
        clc
        ret
.mal:
        mov esi, t_e_ruta
        jmp .dice
.larga:
        mov esi, t_e_larga
.dice:
        cmp byte [avisar], 0
        je .no
        call aviso
.no:
        call leer_otra_vez          ; (lo que se veía, como estaba)
        popad
        stc
        ret

; (interna) si una lectura que falló ha estropeado la lista, se vuelve a leer
; la carpeta de antes
leer_otra_vez:
        cmp dword [lista_rota], 0
        je .x
        cmp byte [ruta], 0
        je .x
        pushad
        mov esi, ruta
        call leer_carpeta
        call ordenar
        call lista_medidas
        call lista_inv
        call estado_poner
        popad
.x:
        ret

; la dirección: la ruta de la ventana (o lo que se busca)
dir_poner:
        pushad
        mov edi, dir_txt
        cmp dword [modo_busca], 0
        je .r
        mov esi, t_resultados
        call hvl_cad
        mov esi, busca_pat
        call hvl_cad
        mov esi, t_en
        call hvl_cad
.r:
        mov esi, ruta
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [v_dir]
        mov eax, WM_SETTEXT
        mov edx, dir_txt
        HV msg_enviar
        popad
        ret

; el espacio libre de la unidad de la ventana
libre_mirar:
        pushad
        mov byte [st_der], 0
        mov al, [ruta]
        sub al, 'A'
        call d_libre
        jc .x
        movzx ebx, byte [ruta]      ; (un CD: lo dice el árbol)
        mov [es_cd+ebx-'A'], cl
        mov edi, st_der
        mov esi, t_libre
        call hvl_cad
        call pon_kb
        mov byte [edi], 0
.x:
        popad
        ret

; EAX=KB -> «1.234 KB» (o «12,3 MB») en EDI, que avanza
pon_kb:
        cmp eax, 10240
        jb .kb
        push eax
        push edx
        push ecx
        mov ecx, 1024
        xor edx, edx
        div ecx                     ; MB enteros
        push edx
        call pon_miles
        mov al, ','
        stosb
        pop eax
        imul eax, 10
        xor edx, edx
        div ecx
        add al, '0'
        stosb
        mov eax, ' MB'
        stosd
        dec edi
        pop ecx
        pop edx
        pop eax
        ret
.kb:
        call pon_miles
        push esi
        mov esi, t_kb
        call hvl_cad
        pop esi
        ret

; EAX en decimal con puntos de miles («1.234.567») en EDI, que avanza
pon_miles:
        push eax
        push ebx
        push ecx
        push edx
        push esi
        sub esp, 16
        mov esi, esp
        xor ecx, ecx
        mov ebx, 10
.d:
        xor edx, edx
        div ebx
        add dl, '0'
        mov [esi+ecx], dl           ; (las cifras, de atrás adelante)
        inc ecx
        test eax, eax
        jnz .d
        mov ebx, 3
.c:
        dec ecx
        mov al, [esi+ecx]
        stosb
        test ecx, ecx
        jz .f
        mov eax, ecx
        xor edx, edx
        div ebx
        test edx, edx
        jnz .c
        mov al, '.'
        stosb
        jmp .c
.f:
        add esp, 16
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; =============================================================================
;  Avisos y preguntas
; =============================================================================
; ESI=texto: un aviso con «Aceptar»
aviso:
        pushad
        mov ebp, [dlg_dueno]
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; ESI=motivo EDI=texto de delante («No se puede leer la unidad »): con la ruta
; de tmp_ruta y los botones Reintentar y Cancelar -> EAX
aviso_ruta:
        push esi
        push edi
        mov ebx, esi
        mov esi, edi
        mov edi, msg_txt
        call hvl_cad
        mov al, [tmp_ruta]
        stosb
        mov ax, ':' + 10 * 256
        stosw
        mov esi, ebx
        call hvl_cad
        mov byte [edi], 0
        mov esi, msg_txt
        mov edi, bot_reint
        call preguntar
        pop edi
        pop esi
        ret

; ESI=texto EDI=botones (dd id, texto...; dd 0) -> EAX=el id del que se pulsa
; (Esc: ID_CANCELAR). Un diálogo hecho aquí, del tamaño del texto
preguntar:
        pushad
        ; las líneas y la más larga
        xor ebx, ebx
        xor edx, edx
        push esi
.l0:
        inc ebx
        xor ecx, ecx
.l1:
        lodsb
        test al, al
        jz .l2
        cmp al, 10
        je .l2
        inc ecx
        jmp .l1
.l2:
        cmp ecx, edx
        jbe .l3
        mov edx, ecx
.l3:
        test al, al
        jnz .l0
        pop esi
        ; los botones
        xor ecx, ecx
.b:
        cmp dword [edi+ecx*8], 0
        je .b1
        inc ecx
        jmp .b
.b1:
        mov [pg_nb], ecx
        imul eax, ecx, 108
        add eax, 20                 ; ancho para los botones
        shl edx, 3
        add edx, 40                 ; para el texto
        cmp edx, eax
        jge .w
        mov edx, eax
.w:
        mov [pg_an], edx
        imul ebx, 16
        mov [pg_tal], ebx
        ; la plantilla del diálogo y las de los controles
        mov eax, [hv_api + HV_c_dialogo * 4]
        mov [pg_dlg+P_CLASE], eax
        mov eax, t_titulo           ; el título: el que se pide o el de siempre
        xchg eax, [pg_tit]
        test eax, eax
        jnz .ti
        mov eax, t_titulo
.ti:
        mov [pg_dlg+P_TEXTO], eax
        mov dword [pg_tit], 0
        mov dword [pg_dlg+P_ESTILO], E_DIALOGO
        mov [pg_dlg+P_AN], edx
        lea eax, [ebx+16+16+24+16 + TIT_H + 2 * BORDE]
        mov [pg_dlg+P_AL], eax
        mov dword [pg_dlg+P_ID], 0
        mov ebx, pg_ctl
        mov eax, [hv_api + HV_c_estatico * 4]
        mov [ebx+P_CLASE], eax
        mov [ebx+P_TEXTO], esi
        mov dword [ebx+P_ESTILO], E_HIJA
        mov dword [ebx+P_X], 20
        mov dword [ebx+P_Y], 16
        mov eax, edx
        sub eax, 40
        mov [ebx+P_AN], eax
        mov eax, [pg_tal]
        add eax, 4                  ; (que no se corten las letras de abajo)
        mov [ebx+P_AL], eax
        mov dword [ebx+P_ID], 99
        add ebx, P_TAM
        ; los botones, centrados
        imul eax, [pg_nb], 108
        sub eax, 8
        neg eax
        add eax, [pg_an]
        sar eax, 1
        sub eax, BORDE
        mov ecx, [pg_nb]
        mov esi, edi
.bt:
        mov edx, [hv_api + HV_c_boton * 4]
        mov [ebx+P_CLASE], edx
        mov edx, [esi+4]
        mov [ebx+P_TEXTO], edx
        mov dword [ebx+P_ESTILO], E_HIJA | E_TABULA
        cmp esi, edi
        jne .bt1
        or dword [ebx+P_ESTILO], BS_DEFECTO
.bt1:
        mov [ebx+P_X], eax
        mov edx, [pg_tal]
        add edx, 32
        mov [ebx+P_Y], edx
        mov dword [ebx+P_AN], 100
        mov dword [ebx+P_AL], 24
        mov edx, [esi]
        mov [ebx+P_ID], edx
        add eax, 108
        add esi, 8
        add ebx, P_TAM
        dec ecx
        jnz .bt
        mov dword [ebx], 0
        mov ebp, [dlg_dueno]
        mov esi, pg_dlg
        mov edi, pg_ctl
        xor ebx, ebx
        xor edx, edx
        HV dlg_modal
        mov [esp+28], eax
        popad
        ret

            align 4
bot_reint   dd ID_REINT, t_reint, ID_CANCELAR, t_cancelar, 0
bot_omitir  dd ID_REINT, t_reint, ID_OMITIR, t_omitir, ID_CANCELAR, t_cancelar, 0
bot_sobre   dd ID_SI, t_si, ID_NO, t_no, ID_TODOS, t_si_todos, ID_CANCELAR, t_cancelar, 0
bot_borrar  dd ID_SI, t_si, ID_NO, t_no, 0

; =============================================================================
;  ARCHIVOS.INI
; =============================================================================
k_ventana   db "Ventana=", 0
k_estado    db "Maximizada=", 0
k_vista     db "Vista=", 0
k_orden     db "Orden=", 0
k_inverso   db "Inverso=", 0
k_arbol     db "Arbol=", 0
k_carpeta   db "Carpeta=", 0
k_marcado   db "Marcado=", 0
t_ini_cab   db "; Hispa Ventanas - Administrador de archivos (ARCHIVOS.HVP).", 13, 10
            db "[Archivos]", 13, 10, 0

ini_leer:
        pushad
        mov esi, ini_ruta
        mov edi, ini_buf
        mov ecx, 1024
        call hvl_leer
        test eax, eax
        jz .x
        mov esi, ini_buf
        mov edi, k_ventana
        call hvl_clave
        jc .v
        mov edi, arc_plant + P_X
        mov ecx, 4
.r:
        call hvl_lee_num
        stosd
        cmp byte [esi], ','
        jne .r1
        inc esi
.r1:
        loop .r
        ; que quepa en la pantalla
        HV pantalla
        cmp dword [arc_plant+P_AN], 380
        jl .rmal
        cmp dword [arc_plant+P_AL], 260
        jl .rmal
        cmp [arc_plant+P_AN], eax
        jg .rmal
        cmp [arc_plant+P_AL], ebx
        jg .rmal
        mov ecx, eax
        sub ecx, 40
        cmp [arc_plant+P_X], ecx
        jg .rmal
        cmp dword [arc_plant+P_X], -100
        jl .rmal
        mov ecx, ebx
        sub ecx, 40
        cmp [arc_plant+P_Y], ecx
        jg .rmal
        cmp dword [arc_plant+P_Y], BARRA_H
        jge .v
.rmal:
        mov dword [arc_plant+P_X], 60
        mov dword [arc_plant+P_Y], 40
        mov dword [arc_plant+P_AN], 620
        mov dword [arc_plant+P_AL], 420
.v:
        mov esi, ini_buf
        mov edi, k_estado
        call hvl_clave
        jc .v1
        call hvl_lee_num
        mov [ini_max], eax
.v1:
        mov esi, ini_buf
        mov edi, k_vista
        call hvl_clave
        jc .o
        call hvl_lee_num
        and eax, 1
        mov [vista], eax
.o:
        mov esi, ini_buf
        mov edi, k_orden
        call hvl_clave
        jc .i
        call hvl_lee_num
        cmp eax, 3
        ja .i
        mov [orden], eax
.i:
        mov esi, ini_buf
        mov edi, k_inverso
        call hvl_clave
        jc .a
        call hvl_lee_num
        and eax, 1
        mov [inverso], eax
.a:
        mov esi, ini_buf
        mov edi, k_arbol
        call hvl_clave
        jc .c
        call hvl_lee_num
        mov [ta], eax
.c:
        mov esi, ini_buf
        mov edi, k_carpeta
        call hvl_clave
        jc .m
        mov edi, ini_carpeta
        call ini_valor
.m:
        mov esi, [param]            ; lo marcado, solo al volver de un programa
        mov al, 'R'
        call hvl_opcion
        jc .x
        mov esi, ini_buf
        mov edi, k_marcado
        call hvl_clave
        jc .x
        mov edi, ini_marcado
        call ini_valor
.x:
        popad
        ret
; (interna) ESI=valor -> EDI (hasta el fin de la línea)
ini_valor:
        mov ecx, RUTA_TAM - 1
.c:
        lodsb
        cmp al, ' '
        jb .f
        stosb
        loop .c
.f:
        mov byte [edi], 0
        ret

; lo escribe si ha cambiado
ini_guardar:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        cmp byte [ruta], 0          ; (todavía no se ve ninguna carpeta)
        je .x
        mov edi, ini_nuevo
        mov esi, t_ini_cab
        call hvl_cad
        mov esi, k_ventana
        call hvl_cad
        lea esi, [ebp+V_X]          ; (si está maximizada, el sitio de antes)
        test dword [ebp+V_ESTADO], S_MAXIM
        jz .r
        lea esi, [ebp+V_RX]
.r:
        mov ecx, 4
.r1:
        lodsd
        call num_s
        mov al, ','
        stosb
        loop .r1
        dec edi
        call nl
        mov esi, k_estado
        call hvl_cad
        mov eax, [ebp+V_ESTADO]
        shr eax, 2
        and eax, 1
        call num_nl
        mov esi, k_vista
        call hvl_cad
        mov eax, [vista]
        call num_nl
        mov esi, k_orden
        call hvl_cad
        mov eax, [orden]
        call num_nl
        mov esi, k_inverso
        call hvl_cad
        mov eax, [inverso]
        call num_nl
        mov esi, k_arbol
        call hvl_cad
        mov eax, [ta]
        call num_nl
        mov esi, k_carpeta
        call hvl_cad
        mov esi, ruta
        cmp dword [modo_busca], 0
        je .c
        mov esi, busca_raiz
.c:
        call hvl_cad
        call nl
        mov esi, k_marcado
        call hvl_cad
        mov eax, [foco_i]
        cmp eax, [n_ents]
        jae .m
        cmp dword [modo_busca], 0
        jne .m
        shl eax, 5
        lea esi, [ents+eax+E_NOM]
        call hvl_cad
.m:
        call nl
        mov ecx, edi
        sub ecx, ini_nuevo
        cmp ecx, [ini_largo]        ; ¿como lo que ya hay?
        jne .w
        push ecx
        mov esi, ini_nuevo
        mov edi, ini_hecho
        repe cmpsb
        pop ecx
        je .x
.w:
        mov [ini_largo], ecx
        push ecx
        mov esi, ini_nuevo
        mov edi, ini_hecho
        rep movsb
        pop ecx
        mov esi, ini_ruta
        mov edx, ini_nuevo
        call hvl_escribir
        call crit_olvidar           ; (en un disquete protegido, no se puede: nada)
.x:
        popad
        ret
num_s:
        test eax, eax
        jns .p
        push eax
        mov al, '-'
        stosb
        pop eax
        neg eax
.p:
        jmp hvl_num
num_nl:
        call hvl_num
nl:
        mov ax, 0x0A0D
        stosw
        ret

; =============================================================================
;  Las órdenes del menú
; =============================================================================
o_salir:
        mov eax, WM_CLOSE
        mov ebp, [ven]
        HV msg_poner
        ret

o_ayuda:
        mov esi, t_ayuda
        mov edi, t_ayuda_tit
        jmp o_mensaje
o_acerca:
        mov esi, t_acerca
        mov edi, t_titulo
o_mensaje:
        mov ebp, [ven]
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret

o_iconos:
        xor eax, eax
        jmp o_vista
o_detalles:
        mov eax, 1
o_vista:
        mov [vista], eax
        mov dword [desp_l], 0
        call marcas_menu
        call lista_medidas
        call lista_ver_foco
        call lista_inv
        jmp ini_guardar

o_orden:
        sub ecx, O_ORD_NOM
        cmp ecx, [orden]
        jne .o
        xor dword [inverso], 1      ; (la misma: al revés)
        jmp .r
.o:
        mov [orden], ecx
        mov dword [inverso], 0
.r:
        call ordenar_conservar
        call marcas_menu
        jmp ini_guardar
o_inverso:
        xor dword [inverso], 1
        call ordenar_conservar
        call marcas_menu
        jmp ini_guardar

o_actualizar:
        cmp dword [modo_busca], 0
        jne .b
        call refrescar
        call arbol_recargar_actual
        ret
.b:
        jmp buscar_otra_vez

o_subir:
        mov esi, ruta
        cmp dword [modo_busca], 0
        jne .ir                     ; (de la búsqueda: a la carpeta)
        mov edi, tmp
        mov ecx, RUTA_TAM - 1
        call copiar_n
        mov esi, tmp
        call ruta_subir
        jc .x
        mov eax, ruta               ; lo que se deja, marcado
        mov esi, eax
        call ruta_nombre
        mov esi, eax
        mov edi, ini_marcado
        mov ecx, 12
        call copiar_n
        mov esi, tmp
        call ir_a
        jc .x
        mov esi, ini_marcado
        call lista_marcar_nombre
.x:
        ret
.ir:
        jmp ir_a

o_ir:                               ; lo escrito en la dirección
        mov ebp, [v_dir]
        mov eax, WM_GETTEXT
        mov ecx, RUTA_TAM
        mov edx, tmp
        HV msg_enviar
        mov esi, tmp
        call ir_a
        jc .x
        mov ebp, [v_lista]
        HV ven_enfocar
.x:
        ret

o_esc:
        cmp dword [arr_activo], 0
        je .x
        call arr_cancelar
.x:
        ret

o_desplegar:
        mov eax, [sel_nodo]
        jmp arbol_alternar

o_todo:
        cmp dword [panel_foco], 1
        je .x
        mov eax, 1
        call sel_todos
.x:
        ret
o_invertir:
        mov eax, 2
        jmp sel_todos

%include "dos.inc"
%include "lista.inc"
%include "arbol.inc"
%include "ops.inc"
%include "iconos.inc"
%include "hvplib.inc"

        section .bss
            alignb 4
param       resd 1
dbuf        resd 1                  ; el búfer de paso de Hispa Ventanas
rl          resb RL_TAM             ; los registros de rm_interrup
ven         resd 1
v_dir       resd 1
v_subir     resd 1
v_arbol     resd 1
v_lista     resd 1
v_barra_a   resd 1
v_barra_l   resd 1
v_prog      resd 1
cli_an      resd 1
cli_al      resd 1
ta          resd 1                  ; el ancho del árbol que se pide (0: el de siempre)
ta_real     resd 1                  ; y el que tiene
panel_al    resd 1
panel_foco  resd 1                  ; 1 el árbol, 2 la lista, 0 otro (la dirección)
sep_arr     resd 1
max_ant     resd 1
ini_max     resd 1
ini_largo   resd 1
vista       resd 1                  ; 0 iconos, 1 detalles
orden       resd 1                  ; 0 nombre, 1 tamaño, 2 tipo, 3 fecha
inverso     resd 1
ctx_en      resd 1
ocupado     resd 1                  ; una operación en marcha (copiar...)
lista_rota  resd 1
avisar      resb 4
pg_nb       resd 1
pg_tit      resd 1
pg_an       resd 1
pg_tal      resd 1
pg_dlg      resb P_TAM
pg_ctl      resb P_TAM * 8
tmp         resb 128
tmp_ruta    resb 128
tmp2        resb 128
mi_ruta     resb RUTA_TAM
d_progs     resb RUTA_TAM
d_hisp      resb RUTA_TAM
ini_ruta    resb RUTA_TAM
ini_carpeta resb RUTA_TAM
ini_marcado resb RUTA_TAM
ruta        resb 128                ; la carpeta que se ve
dir_txt     resb 128
st_izq      resb 128                ; la barra de estado
st_der      resb 64
msg_txt     resb 512
ini_buf     resb 1024
ini_nuevo   resb 1024
ini_hecho   resb 1024
HVP_FIN
