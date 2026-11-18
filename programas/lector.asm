; =============================================================================
;  LECTOR.HVP - Lector, el lector de documentos PDF de Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Abre un PDF y lo enseña página a página: el texto, en su sitio y con el
;  tipo de letra de los nuestros que más se parece al suyo (Lector no usa las
;  fuentes que el PDF lleva dentro); las rayas, los recuadros y las figuras
;  rellenas; y las imágenes, las fotos y los escaneos (JPEG) incluidos. Se pasa
;  de página, se amplía y se reduce (página entera, ancho de página, tamaño
;  real y de 25 a 400 %), se busca un texto por todo el documento y el texto
;  de una página o de todo el documento se guarda en un .TXT (para abrirlo
;  con Pluma). Se imprime la página, o el documento entero, como se ve: en
;  el modo gráfico de las impresoras de agujas (ESC/P, por LPT1), así que
;  salen también las imágenes y los códigos de barras de una etiqueta (Ctrl+P,
;  el botón de la impresora); o, si se prefiere, solo el texto (Archivo >
;  Imprimir solo el texto...).
;
;  Lo que no sabe, lo dice: un PDF cifrado no se abre; una página comprimida
;  de una forma que no conoce, un texto con letra de codificación propia o
;  una página dañada llevan un aviso, arriba, en granate. Un archivo roto o
;  cortado no lo cuelga: si le falta el índice, se rehace.
;
;  Cómo está hecho. El archivo se lee entero a un bloque de memoria extendida
;  y detrás se deja un «almacén» para lo que haya que descomprimir. Leer el
;  PDF es cosa del núcleo, que no sabe nada de Hispa Ventanas y se prueba
;  aparte (tests/tools/lector_pdf.py):
;     programas/comun/desinfla.inc    el descompresor (Deflate y zlib)
;     programas/comun/jpeg.inc        el descodificador de JPEG
;     programas/lector/pdf.inc        objetos, índice, flujos y páginas
;     programas/lector/letras.inc     fuentes: codificación, anchos, parecido
;     programas/lector/nombres.inc    tablas (las hace haznombres.py)
;     programas/lector/contenido.inc  el contenido de una página -> la lista
;                                     de lo que hay que pintar
;     programas/lector/texto.inc      el texto en orden de lectura; buscar
;  El núcleo corre con su propia pila (nuc): la de una tarea de Hispa Ventanas
;  es de 8 K y un PDF tiene listas dentro de listas.
;  Aquí está lo demás: la ventana, las órdenes y pintar la lista en un mapa
;  de puntos del tamaño de la zona de lectura (vbuf), que se copia a la
;  ventana de una vez; solo se rehace cuando cambia lo que se ve. Para
;  imprimir como se ve, lo mismo se pinta en un mapa propio, por franjas del
;  ancho del papel (imprime_g, más abajo).
;
;  El texto se pinta con los tipos de programas/comun/tipos.inc, en el tamaño
;  dibujado más cercano; cada trozo empieza donde dice el PDF y sus letras se
;  reparten para ocupar el ancho que el PDF dice que tiene (así las columnas
;  y los márgenes quedan rectos aunque nuestras letras midan otra cosa). Por
;  debajo de 8 puntos de pantalla las letras se encogen (se ve la mancha del
;  texto, no se lee); por debajo de 3, una raya gris.
;
;  Las imágenes sin comprimir o con Flate, LZW, RunLength, ASCII85 o
;  ASCIIHex (grises, RGB, CMYK, con paleta, de 1 a 16 bits, y las máscaras)
;  y las JPEG (/DCTDecode: grises, color, CMYK, también progresivas) se
;  pintan, reducidas a la paleta (programas/lector/imagen.inc), con su
;  transparencia a todo o nada (/SMask y /Mask); una página escaneada es una
;  imagen más. Los recortes rectangulares (W, W*) se respetan; el texto de la
;  página se copia al portapapeles (Ctrl+C) y la hoja se arrastra con el ratón.
;
;  Lo que no hace (todavía): las imágenes JPEG 2000, fax, JBIG2 y los JPEG
;  aritméticos o de 12 bits (van en un recuadro gris, con un aviso que dice
;  cuál es), los recortes que no son un rectángulo y los degradados; la
;  transparencia a medias; abrir documentos cifrados; formularios, enlaces y
;  marcadores; ordenar por columnas el texto que se guarda. Límites:
;  48 MB de archivo, 4.000 páginas y 32.768 objetos (si se pasa, lo dice y
;  enseña lo que cabe); de una página, 6.000 trozos y 128.000 letras. Necesita
;  2,5 MB de memoria, más el mapa de puntos (lo que mida la pantalla), el
;  archivo y de 1,25 a 7,25 MB de almacén, según el documento.
;
;  La prueba: tests/run_lector_tests.sh (el descompresor y el núcleo, fuera;
;  y el programa entero en QEMU).
;
;  Parámetros del acceso directo: el nombre de un PDF, que se abre.
; =============================================================================
%include "hvp.inc"

TB_AL       equ 30                  ; la barra de botones
AVISO_AL    equ 20                  ; la franja del aviso de la página
MARG        equ 8                   ; lo que se deja alrededor de la hoja
COPIA_MAX   equ 30000               ; letras que se copian al portapapeles
ID_PUENTE   equ 900
NUC_PILA    equ 48 * 1024           ; la pila del núcleo
ARCH_MAX    equ 48 * 1024 * 1024    ; un PDF mayor no se intenta
MAPA_N      equ 136
PV_MAX      equ 3000                ; vértices de una figura, al pintarla
HL_MAX      equ 16                  ; trozos de lo encontrado

ID_BARRA_V  equ 901
ID_BARRA_H  equ 902

; ---- órdenes ----
O_ABRIR     equ 101
O_TXT_PAG   equ 102
O_TXT_DOC   equ 103
O_INFO      equ 104
O_IMP_PAG   equ 105
O_IMP_DOC   equ 106
O_SALIR     equ 107
O_CERRAR    equ 108
O_IMPT_PAG  equ 109
O_IMPT_DOC  equ 110
O_BUSCAR    equ 111
O_SIGUIENTE equ 112
O_COPIAR    equ 113
O_ANT       equ 121
O_SIG       equ 122
O_PRIMERA   equ 123
O_ULTIMA    equ 124
O_IRA       equ 125
O_AMPLIAR   equ 126
O_REDUCIR   equ 127
O_REAL      equ 128
O_ANCHO     equ 129
O_ENTERA    equ 130
O_ACERCA    equ 151

HVP_CABECERA "Lector", entrada, 4
        db "................................"
        db "................................"
        db "......000000000000000..........."
        db "......0999999999999900.........."
        db "......099999999999990f0........."
        db "......099999999999990ff0........"
        db "......099222222222990fff0......."
        db "......099222222222990ffff0......"
        db "......09999999999999000000......"
        db "......09999999999999999990......"
        db "......09999999999999999990......"
        db "......09977777777777777990......"
        db "......09999999999999999990......"
        db "......09999999999999999990......"
        db "......09977777777777777990......"
        db "......09999999999999999990......"
        db "......09999999999999999990......"
        db "......09977777777777777990......"
        db "......09999999999999999990......"
        db "......09999999999999999990......"
        db "......09977777777777777990......"
        db "...ddddddddddddd9999999990......"
        db "...ddddddddddddd9999999990......"
        db "...dd888d88dd88d7999999990......"
        db "...dd888d8d8d88d9999999990......"
        db "...dd8ddd88dd8dd9999999990......"
        db "...ddddddddddddd9999999990......"
        db "...ddddddddddddd9999999990......"
        db "......09999999999999999990......"
        db "......00000000000000000000......"
        db "................................"
        db "................................"
HVP_CODIGO

c_lector    dd lec_proc
            db SIN_FONDO, 0, 0, 0   ; lo pinta todo ella
lec_plant:
        PLANTILLA c_lector, titulo, E_NORMAL, 40, 34, 720, 540, 0
hijos:
        PLANTILLA HV_c_edit, 0, E_HIJA | E_OCULTA | ES_MULTI, 0, 0, 40, 40, ID_PUENTE
        PLANTILLA HV_c_barra, 0, E_HIJA, 0, 0, BD_AN, 50, ID_BARRA_V
        PLANTILLA HV_c_barra, 0, E_HIJA | BDS_HORIZ, 0, 0, 50, BD_AN, ID_BARRA_H
        dd 0

lec_menu:
        db "&Archivo", 0
        dw O_ABRIR
        db "&Abrir...                        Ctrl+A", 0
        dw O_CERRAR
        db "&Cerrar", 0
        dw 0
        db 0
        dw O_TXT_PAG
        db "&Guardar el texto de la página...", 0
        dw O_TXT_DOC
        db "Guardar el texto del &documento...", 0
        dw 0
        db 0
        dw O_INFO
        db "I&nformación del documento...", 0
        dw 0
        db 0
        dw O_IMP_PAG
        db "&Imprimir esta página (como se ve)  Ctrl+P", 0
        dw O_IMP_DOC
        db "Imprimir &todo el documento (como se ve)", 0
        dw O_IMPT_PAG
        db "Imprimir solo el te&xto de la página", 0
        dw O_IMPT_DOC
        db "Imprimir solo el texto del doc&umento", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw O_COPIAR
        db "&Copiar el texto de la página  Ctrl+C", 0
        dw 0
        db 0
        dw O_BUSCAR
        db "&Buscar...                     Ctrl+B", 0
        dw O_SIGUIENTE
        db "Buscar si&guiente                  F3", 0
        dw 0xFFFF
        db "&Ver", 0
        dw O_ANT
        db "Página &anterior           RePág", 0
        dw O_SIG
        db "Página &siguiente          AvPág", 0
        dw O_PRIMERA
        db "Ir a la &primera     Ctrl+Inicio", 0
        dw O_ULTIMA
        db "Ir a la ú&ltima         Ctrl+Fin", 0
        dw O_IRA
        db "&Ir a la página...        Ctrl+I", 0
        dw 0
        db 0
        dw O_AMPLIAR
        db "A&mpliar                       +", 0
        dw O_REDUCIR
        db "&Reducir                       -", 0
        dw O_REAL
        db "&Tamaño real (100 %)", 0
        dw O_ANCHO
        db "Anc&ho de página", 0
        dw O_ENTERA
        db "Página &entera", 0
        dw 0xFFFF
        db "&?", 0
        dw O_ACERCA
        db "&Acerca de Lector...", 0
        dw 0xFFFF
        db 0

; ---- el menú del botón secundario, sobre la página (comun/contexto.inc) ----
ctx_pagina:
        dw O_COPIAR
        db "&Copiar el texto de la página  Ctrl+C", 0
        dw O_BUSCAR
        db "&Buscar...                     Ctrl+B", 0
        dw O_SIGUIENTE
        db "Buscar si&guiente                  F3", 0
        dw 0
        db 0
        dw O_ANT
        db "Página &anterior               RePág", 0
        dw O_SIG
        db "Página &siguiente              AvPág", 0
        dw O_IRA
        db "&Ir a la página...            Ctrl+I", 0
        dw 0
        db 0
        dw O_AMPLIAR
        db "A&mpliar                           +", 0
        dw O_REDUCIR
        db "&Reducir                           -", 0
        dw O_REAL
        db "&Tamaño real (100 %)", 0
        dw O_ANCHO
        db "Anc&ho de página", 0
        dw O_ENTERA
        db "Página &entera", 0
        dw 0
        db 0
        dw O_TXT_PAG
        db "Guardar el te&xto de la página...", 0
        dw O_IMP_PAG
        db "Im&primir esta página (como se ve)  Ctrl+P", 0
        dw O_INFO
        db "I&nformación del documento...", 0
        dw 0
        db 0
        dw O_ABRIR
        db "Abrir un d&ocumento...      Ctrl+A", 0
        dw 0xFFFF
ctx_gris:                           ; las que pueden ir en gris, y cuándo
        dw O_COPIAR
        dd cg_sin_doc
        dw O_BUSCAR
        dd cg_sin_doc
        dw O_SIGUIENTE
        dd cg_sin_buscar
        dw O_ANT
        dd cg_primera
        dw O_SIG
        dd cg_ultima
        dw O_IRA
        dd cg_una
        dw O_AMPLIAR
        dd cg_tope
        dw O_REDUCIR
        dd cg_minimo
        dw O_REAL
        dd cg_sin_doc
        dw O_ANCHO
        dd cg_sin_doc
        dw O_ENTERA
        dd cg_sin_doc
        dw O_TXT_PAG
        dd cg_sin_doc
        dw O_IMP_PAG
        dd cg_sin_doc
        dw O_INFO
        dd cg_sin_doc
        dw 0

; ---- la barra de botones (programas/comun/barra.inc) ----
botones     dd 6, 26, ic_abrir, O_ABRIR, nb_abrir, BT_ICONO
            dd 34, 26, ic_imprimir, O_IMP_PAG, nb_imprimir, BT_ICONO
            dd 70, 26, ic_pag_ant, O_ANT, nb_ant, BT_ICONO
            dd 98, 26, ic_pag_sig, O_SIG, nb_sig, BT_ICONO
            dd 134, 26, ic_menos, O_REDUCIR, nb_reducir, BT_ICONO
            dd 162, 26, ic_mas, O_AMPLIAR, nb_ampliar, BT_ICONO
            dd 198, 26, ic_buscar, O_BUSCAR, nb_buscar, BT_ICONO
            dd 0, 0, 0, 0, 0, 0
nb_abrir    db "Abrir un documento PDF (Ctrl+A)", 0
nb_imprimir db "Imprimir esta página tal como se ve (Ctrl+P)", 0
nb_ant      db "Página anterior (RePág)", 0
nb_sig      db "Página siguiente (AvPág)", 0
nb_reducir  db "Reducir: ver la página más pequeña (-)", 0
nb_ampliar  db "Ampliar: ver la página más grande (+)", 0
nb_buscar   db "Buscar un texto en el documento (Ctrl+B)", 0
; los iconos propios, con la forma de los de barra.inc
ic_pag_ant:
        db "................"
        db "................"
        db "................"
        db "..........2....."
        db "........222....."
        db ".......2222....."
        db "......2222222..."
        db "....222222222..."
        db "....222222222..."
        db "......2222222..."
        db ".......2222....."
        db "........222....."
        db "..........2....."
        db "................"
        db "................"
        db "................"
ic_pag_sig:
        db "................"
        db "................"
        db "................"
        db ".....2.........."
        db ".....222........"
        db ".....2222......."
        db "...2222222......"
        db "...222222222...."
        db "...222222222...."
        db "...2222222......"
        db ".....2222......."
        db ".....222........"
        db ".....2.........."
        db "................"
        db "................"
        db "................"

; las ampliaciones fijas, en tantos por ciento (100 % = 96 puntos por pulgada)
zooms       dd 25, 33, 50, 67, 75, 100, 125, 150, 200, 300, 400
ZOOMS_N     equ 11
; las órdenes de Ctrl+letra (de la A a la Z; 0: ninguna), como en Pluma
ctrl_ord    db 0, O_ABRIR, O_BUSCAR, O_COPIAR, 0, 0, 0, 0, 0, O_IRA, 0
            db 0, 0, 0, 0, 0, O_IMP_PAG, 0, 0, 0, 0
            db 0, 0, 0, 0, 0, 0

; ---- textos ----
t_lector    db "Lector", 0
t_guion     db " - ", 0
t_abrir     db "Abrir un PDF", 0
t_guardar   db "Guardar el texto como", 0
t_buscar    db "Buscar", 0
t_q_buscar  db "Texto que se busca:", 0
t_ira       db "Ir a la página", 0
t_q_ira     db "Número de página, de 1 a ", 0
t_info      db "Información del documento", 0
t_acerca_tit db "Acerca de Lector", 0
t_acerca    db "Lector", 10
            db "El lector de documentos PDF de Hispa Ventanas:", 10
            db "pasa las páginas, amplía, busca, imprime y guarda el texto.", 10
            db "El texto se ve con los tipos de letra de Hispa Ventanas.", 10, 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores.", 10
            db "GPL-2.0 o posterior.", 0
t_pagina    db "Página ", 0
t_de        db " de ", 0
t_pct       db " %", 0
t_sep       db "    ", 0
t_sin_doc   db "No hay ningún documento abierto.", 0
t_pista     db "Archivo > Abrir (Ctrl+A) para leer un documento PDF", 0
t_imagen    db "(imagen)", 0
t_ext_txt   db ".TXT", 0
t_primero   db "Primero abra un documento: Archivo > Abrir (Ctrl+A).", 0
t_no_abrir  db "No se ha podido abrir el archivo.", 10
            db "Compruebe que el nombre está bien escrito y que el disco se lee.", 0
t_no_leer   db "No se ha podido leer el archivo entero: el disco da error.", 0
t_e_nopdf   db "Este archivo no es un documento PDF.", 10
            db "Lector solo abre archivos PDF; los de texto se abren con Pluma.", 0
t_e_roto    db "El documento está dañado y no se puede leer:", 10
            db "le falta el índice y no se ha podido rehacer.", 10
            db "Pida a quien se lo dio que se lo envíe otra vez.", 0
t_e_cifrado db "El documento está cifrado (protegido con contraseña", 10
            db "o con permisos) y Lector no sabe abrir documentos cifrados.", 10
            db "Pida una copia sin proteger.", 0
t_e_memoria db "No hay memoria bastante para abrir este documento.", 10
            db "Cierre otros programas y pruebe otra vez.", 0
t_e_sinpag  db "El documento no tiene ninguna página que se pueda leer.", 0
t_e_grande  db "El documento es demasiado grande para Lector (más de 48 MB).", 0
t_muchos    db "El documento es muy grande para Lector:", 10
            db "puede que falten páginas o partes de alguna", 10
            db "(caben 4.000 páginas y 32.768 objetos).", 0
t_no_esta   db "No se encuentra ese texto en el documento.", 0
t_sin_texto db "Esta página no tiene texto que se pueda guardar, copiar o imprimir.", 0
t_copia_larga db "La página tiene mucho texto: se han copiado", 10, "solo las primeras 30.000 letras.", 0
t_guardado  db "El texto se ha guardado.", 10
            db "Se puede abrir con Pluma.", 0
t_no_guardar db "No se ha podido guardar el texto.", 10
            db "Compruebe que hay sitio en el disco y que no está protegido.", 0
t_mismo     db "Ese es el nombre del propio documento PDF:", 10
            db "elija otro nombre para el texto.", 0
t_impreso   db "El texto se ha enviado a la impresora.", 0
t_impreso_g db "La página se ha enviado a la impresora.", 0
t_impreso_d db "El documento se ha enviado a la impresora.", 0
t_e_imprimir db "No se ha podido imprimir.", 10
            db "Compruebe que la impresora (la del puerto paralelo, LPT1)", 10
            db "está encendida, en línea y con papel.", 0
t_cab_pag   db "--- Página ", 0
t_cab_fin   db " ---", 13, 10, 0
; los avisos de la página, del más grave al menos
t_av_filtro db "Aviso: página con una compresión que Lector no conoce", 0
t_av_filtro2 db ": no se muestra.", 0
t_av_memoria db "Aviso: falta memoria para mostrar esta página entera.", 0
t_av_roto   db "Aviso: la página está dañada; puede faltar una parte.", 0
t_av_ilegible db "Aviso: hay texto que no se puede leer (letra con codificación propia): va en gris.", 0
t_av_llena  db "Aviso: la página es muy compleja; se muestra solo una parte.", 0
t_av_descon db "Aviso: hay letras que Hispa-DOS no tiene; se ven como «?».", 0
t_av_imagen db "Hay imágenes que Lector no sabe mostrar", 0
t_av_imagen2 db ": van como recuadros grises.", 0
; la información
t_i_archivo db "Archivo: ", 0
t_i_titulo  db "Título: ", 0
t_i_autor   db "Autor: ", 0
t_i_asunto  db "Asunto: ", 0
t_i_hecho   db "Hecho con: ", 0
t_i_paginas db "Páginas: ", 0
t_i_version db "Versión de PDF: ", 0
t_i_tam     db "Tamaño de esta página: ", 0
t_i_mm      db " mm", 0
t_i_por     db " x ", 0
t_i_kb      db " KB)", 0
t_i_reparado db "El índice del archivo estaba dañado y se ha rehecho.", 0
t_i_nada    db "(no lo dice)", 0
t_i_muchos  db "Es muy grande: puede que falten páginas.", 0

; =============================================================================
entrada:
        cld
        call tl_cargar              ; los tipos de letra (ESI=ruta del módulo)
        mov [param], edi
        mov esi, hijos
        call hvl_clases
        mov dword [pag_cargada], -1
        mov dword [tx_pag], -1
        mov dword [bus_pag], -1
        mov dword [zmodo], 1
        mov dword [zoom], 0x15555
        call pon_titulo
        HV pantalla                 ; el mapa de puntos de la zona de lectura: nunca
        mov [vb_an], eax            ; hará falta más que la pantalla
        mov [vb_al], ebx
        mov ecx, eax
        imul ecx, ebx
        HV mem_bloque
        jc .x
        mov [vbuf_p], eax
        mov [vbuf_h], edx
        HV pantalla                 ; EAX=ancho EBX=alto: que quepa
        sub ebx, BARRA_H + 12
        cmp ebx, 540
        jae .a
        mov [lec_plant+P_AL], ebx
        mov dword [lec_plant+P_Y], BARRA_H + 4
.a:
        cmp eax, 800
        jae .b
        sub eax, 8
        mov [lec_plant+P_AN], eax
        mov dword [lec_plant+P_X], 4
.b:
        mov esi, lec_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        mov esi, [param]
        call nombre_param
        jc .bucle
        call abre_archivo
.bucle:
        HV app_bucle
        call cierra_doc
        mov edx, [vbuf_h]
        HV mem_bloque_soltar
.x:
        ret

; ESI=parámetros: el primer nombre, a nom_tmp; CF=1 si no hay
nombre_param:
        lodsb
        cmp al, ' '
        je nombre_param
        jb .no
        cmp al, '/'
        je .no
        dec esi
        mov edi, nom_tmp
        mov ecx, 79
.c:
        lodsb
        cmp al, ' '
        jbe .f
        stosb
        dec ecx
        jnz .c
.f:
        mov byte [edi], 0
        clc
        ret
.no:
        stc
        ret

; el título: «Lector - NOMBRE.PDF»
pon_titulo:
        pushad
        mov edi, titulo
        mov esi, t_lector
        call hvl_cad
        cmp byte [nombre], 0
        je .n
        mov esi, t_guion
        call hvl_cad
        mov esi, nombre
        call hvl_cad
.n:
        mov byte [edi], 0
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov esi, titulo
        HV ven_texto
.x:
        popad
        ret

; llama a una función del núcleo (EDX) con la pila del núcleo; los demás
; registros van y vuelven tal cual
nuc:
        mov [nuc_esp], esp
        mov esp, nuc_pila + NUC_PILA
        call edx
        mov esp, [nuc_esp]
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
lec_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_KEYDOWN
        je tecla
        cmp eax, WM_CHAR
        je letra
        cmp eax, WM_LBUTTONDOWN
        je raton
        cmp eax, WM_MOUSEMOVE
        je raton_mueve
        cmp eax, WM_LBUTTONUP
        je raton_suelta
        cmp eax, WM_RBUTTONDOWN     ; el botón secundario: al soltarlo, el
        je raton_der                ; menú de la página
        cmp eax, WM_CONTEXTMENU
        je menu_ctx
        cmp eax, WM_MOUSEWHEEL
        je .rueda
        cmp eax, WM_VSCROLL
        je .barra_v
        cmp eax, WM_HSCROLL
        je .barra_h
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_HV_ABRIR
        je .hvabrir
        cmp eax, WM_GETMINMAXINFO
        jne .nmin
        mov eax, 360 + 240 * 65536
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
        mov dword [ebp+V_MENU], lec_menu
        mov dword [ebp+V_ICONO], hvp_icono
        mov esi, hijos
        HV ven_lista
        mov ecx, ID_PUENTE
        HV ven_hija_id
        mov [puente], eax
        mov ecx, ID_BARRA_V
        HV ven_hija_id
        mov [barra_v], eax
        mov ecx, ID_BARRA_H
        HV ven_hija_id
        mov [barra_h], eax
        push ebp
        mov ebp, [barra_v]
        mov eax, BDM_LINEA
        mov ecx, 32
        HV msg_enviar
        mov ebp, [barra_h]
        mov eax, BDM_LINEA
        mov ecx, 32
        HV msg_enviar
        pop ebp
.tam:
        mov ebp, [ven]
        HV ven_cli                  ; ECX=ancho EDX=alto de la zona
        mov [cli_an], ecx
        mov [cli_al], edx
        call dispone
        call inval
        jmp hecho
.rueda:
        ; la rueda: tres renglones por paso; al llegar al final, a la otra página
        cmp byte [hay_doc], 0
        je hecho
        mov eax, ecx
        imul eax, 48
        test ecx, ecx
        js .r_arriba
        mov ebx, [sy]
        cmp ebx, [sy_max]
        jl .r_mueve
        mov ecx, O_SIG
        jmp orden
.r_arriba:
        cmp dword [sy], 0
        jg .r_mueve
        call pag_anterior_fin
        jmp hecho
.r_mueve:
        add eax, [sy]
        call desplaza_v
        jmp hecho
.barra_v:
        mov eax, ecx
        call desplaza_v
        jmp hecho
.barra_h:
        mov eax, ecx
        call desplaza_h
        jmp hecho
.cerrar:
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        mov dword [ven], 0
        call tl_soltar              ; la memoria de los tipos de letra, devuelta
        HV msg_salir
        jmp hecho
.hvabrir:
        mov esi, edx
        call nombre_param
        jc hecho
        call abre_archivo
hecho:
        xor eax, eax
        ret

; la ventana, a repintar
inval:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        HV ven_invalidar
.x:
        popad
        ret

; hay que rehacer el dibujo de la página
ensucia:
        mov byte [sucio], 1
        jmp inval

; pone el desplazamiento vertical (u horizontal) en EAX, dentro de sus límites
desplaza_v:
        pushad
        cmp eax, [sy_max]
        jle .a
        mov eax, [sy_max]
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        cmp eax, [sy]
        je .x
        mov [sy], eax
        call pon_barras
        call ensucia
.x:
        popad
        ret
desplaza_h:
        pushad
        cmp eax, [sx_max]
        jle .a
        mov eax, [sx_max]
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        cmp eax, [sx]
        je .x
        mov [sx], eax
        call pon_barras
        call ensucia
.x:
        popad
        ret

; -----------------------------------------------------------------------------
;  La disposición: cuánto mide la zona de lectura, a qué tamaño se ve la
;  página y cuánto se puede desplazar. Se llama cuando cambia la ventana, la
;  página o la ampliación
; -----------------------------------------------------------------------------
dispone:
        pushad
        mov eax, TB_AL
        cmp dword [av_txt], 0
        je .v1
        add eax, AVISO_AL
.v1:
        mov [vy], eax
        mov ebx, [cli_al]
        sub ebx, eax
        sub ebx, BR_PIE + BD_AN
        jns .v2
        xor ebx, ebx
.v2:
        cmp ebx, [vb_al]
        jbe .v3
        mov ebx, [vb_al]
.v3:
        mov [vh], ebx
        mov eax, [cli_an]
        sub eax, BD_AN
        jns .v4
        xor eax, eax
.v4:
        cmp eax, [vb_an]
        jbe .v5
        mov eax, [vb_an]
.v5:
        mov [vw], eax
        xor eax, eax
        mov [pw], eax
        mov [ph], eax
        mov [sx_max], eax
        mov [sy_max], eax
        cmp byte [hay_doc], 0
        je .limites
        ; ---- la ampliación ----
        mov eax, [zmodo]
        test eax, eax
        jz .fija
        mov eax, [vw]               ; a lo ancho: lo que quepa entre los márgenes
        call .cabe
        mov ebx, [pg_an]
        call .entre
        mov [zoom], eax
        cmp dword [zmodo], 2
        jne .z_ok
        mov eax, [vh]               ; la página entera: lo menor de ancho y alto
        call .cabe
        mov ebx, [pg_al]
        call .entre
        cmp eax, [zoom]
        jae .z_ok
        mov [zoom], eax
        jmp .z_ok
.fija:
        mov eax, [zpct]             ; el tanto por ciento, a puntos de pantalla
        shl eax, 16                 ; por punto: 100 % = 4/3
        xor edx, edx
        mov ebx, 75
        div ebx
        mov [zoom], eax
.z_ok:
        mov eax, [zoom]
        cmp eax, 0x0800             ; (entre un 2 % y un 600 %)
        jae .z1
        mov eax, 0x0800
.z1:
        cmp eax, 0x80000
        jbe .z2
        mov eax, 0x80000
.z2:
        mov [zoom], eax
        mov eax, [pg_an]
        call a_px
        mov [pw], eax
        mov eax, [pg_al]
        call a_px
        mov [ph], eax
        mov eax, [pw]
        add eax, 2 * MARG
        sub eax, [vw]
        jns .m1
        xor eax, eax
.m1:
        mov [sx_max], eax
        mov eax, [ph]
        add eax, 2 * MARG
        sub eax, [vh]
        jns .m2
        xor eax, eax
.m2:
        mov [sy_max], eax
.limites:
        mov eax, [sx]
        cmp eax, [sx_max]
        jle .l1
        mov eax, [sx_max]
.l1:
        test eax, eax
        jns .l2
        xor eax, eax
.l2:
        mov [sx], eax
        mov eax, [sy]
        cmp eax, [sy_max]
        jle .l3
        mov eax, [sy_max]
.l3:
        test eax, eax
        jns .l4
        xor eax, eax
.l4:
        mov [sy], eax
        ; las barras de desplazamiento, en su sitio
        mov ebp, [barra_v]
        test ebp, ebp
        jz .x
        mov eax, [cli_an]
        sub eax, BD_AN
        mov ebx, [vy]
        mov ecx, BD_AN
        mov edx, [vh]
        HV ven_mover
        mov ebp, [barra_h]
        xor eax, eax
        mov ebx, [vy]
        add ebx, [vh]
        mov ecx, [cli_an]
        sub ecx, BD_AN
        mov edx, BD_AN
        HV ven_mover
        call pon_barras
.x:
        mov byte [sucio], 1
        popad
        ret
; (interna) EAX=puntos de pantalla de la zona -> lo que queda entre los márgenes
.cabe:
        sub eax, 2 * MARG + 2
        cmp eax, 16
        jge .c1
        mov eax, 16
.c1:
        ret
; (interna) EAX=puntos de pantalla, EBX=puntos de la página (16.16) -> EAX=la
; ampliación (16.16) con la que caben justos
.entre:
        cmp ebx, 0x10000            ; (una página de menos de un punto: no)
        jge .e1
        mov ebx, 0x10000
.e1:
        mov edx, eax                ; EDX:EAX = puntos x 2^32
        xor eax, eax
        div ebx                     ; (no desborda: EBX > EDX siempre)
        ret

; las barras de desplazamiento, al día
pon_barras:
        pushad
        mov ebp, [barra_v]
        test ebp, ebp
        jz .x
        mov eax, BDM_PONER
        mov ecx, [sy_max]
        mov edx, [vh]
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [sy]
        HV msg_enviar
        mov ebp, [barra_h]
        mov eax, BDM_PONER
        mov ecx, [sx_max]
        mov edx, [vw]
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [sx]
        HV msg_enviar
.x:
        popad
        ret

; EAX=una medida de la página (16.16, en puntos) -> EAX=en puntos de pantalla
; con la ampliación de ahora (redondeada, y nunca más de 16.000: así las
; cuentas de después no desbordan)
a_px:
        push edx
        imul dword [zoom]
        add eax, 0x80000000
        adc edx, 0
        mov eax, edx
        cmp eax, 16000
        jle .a
        mov eax, 16000
.a:
        cmp eax, -16000
        jge .b
        mov eax, -16000
.b:
        pop edx
        ret
; lo mismo, y ya en la zona de lectura (con el sitio de la hoja)
a_px_x:
        call a_px
        add eax, [ox]
        ret
a_px_y:
        call a_px
        add eax, [oy]
        ret

; =============================================================================
;  Las órdenes: ECX=número
; =============================================================================
orden:
        call orden_haz
        jmp hecho
orden_haz:
        mov dword [br_sobre], 0
        cmp ecx, O_ABRIR
        je abrir
        cmp ecx, O_ACERCA
        je .acerca
        cmp ecx, O_SALIR
        je .salir
        cmp ecx, O_CERRAR
        je cerrar
        cmp ecx, 100                ; (un aviso de las barras: nada)
        jb .x
        cmp ecx, 200
        jae .x
        cmp byte [hay_doc], 0       ; las demás necesitan un documento
        jne .con_doc
        mov esi, t_primero
        jmp aviso
.con_doc:
        cmp ecx, O_TXT_PAG
        je guarda_pag
        cmp ecx, O_TXT_DOC
        je guarda_doc
        cmp ecx, O_INFO
        je informacion
        cmp ecx, O_IMP_PAG
        je imprime_gpag
        cmp ecx, O_IMP_DOC
        je imprime_gdoc
        cmp ecx, O_IMPT_PAG
        je imprime_pag
        cmp ecx, O_IMPT_DOC
        je imprime_doc
        cmp ecx, O_COPIAR
        je copiar
        cmp ecx, O_BUSCAR
        je buscar
        cmp ecx, O_SIGUIENTE
        je buscar_sig
        cmp ecx, O_IRA
        je ir_a
        cmp ecx, O_AMPLIAR
        je ampliar
        cmp ecx, O_REDUCIR
        je reducir
        mov eax, [pag]
        cmp ecx, O_ANT
        jne .o1
        dec eax
        jmp pag_ir
.o1:
        cmp ecx, O_SIG
        jne .o2
        inc eax
        jmp pag_ir
.o2:
        xor eax, eax
        cmp ecx, O_PRIMERA
        je pag_ir
        mov eax, [pdf_npag]
        dec eax
        cmp ecx, O_ULTIMA
        je pag_ir
        mov eax, 100
        xor ebx, ebx
        cmp ecx, O_REAL
        je pon_zoom
        inc ebx
        cmp ecx, O_ANCHO
        je pon_zoom
        inc ebx
        cmp ecx, O_ENTERA
        je pon_zoom
.x:
        ret
.acerca:
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret
.salir:
        mov ebp, [ven]
        HV ven_destruir
        ret

; mensaje de aviso: ESI=texto
aviso:
        pushad
        mov ebp, [ven]
        mov edi, t_lector
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; ---- la ampliación: EBX=modo (0 fija, 1 ancho, 2 página entera), EAX=tanto por ciento ----
pon_zoom:
        mov [zmodo], ebx
        mov [zpct], eax
        call dispone
        jmp inval
; (interna) -> EAX=el tanto por ciento de ahora
zoom_pct:
        mov eax, [zoom]
        imul eax, 75
        add eax, 0x8000
        shr eax, 16
        ret
ampliar:
        call zoom_pct
        xor ecx, ecx
.b:
        cmp [zooms+ecx*4], eax
        ja .es
        inc ecx
        cmp ecx, ZOOMS_N
        jb .b
        ret                         ; (ya está en el mayor)
.es:
        mov eax, [zooms+ecx*4]
        xor ebx, ebx
        jmp pon_zoom
reducir:
        call zoom_pct
        mov ecx, ZOOMS_N
.b:
        dec ecx
        js .x
        cmp [zooms+ecx*4], eax
        jae .b
        mov eax, [zooms+ecx*4]
        xor ebx, ebx
        jmp pon_zoom
.x:
        ret

; ---- las páginas ----
; va a la página EAX (desde 0), arriba del todo
pag_ir:
        test eax, eax
        js .x
        cmp eax, [pdf_npag]
        jae .x
        cmp eax, [pag]
        je .x
        mov [pag], eax
        mov dword [sy], 0
        mov dword [hl_n], 0
        call carga_pagina
.x:
        ret
; a la página anterior, abajo del todo (subiendo con la rueda o con RePág)
pag_anterior_fin:
        mov eax, [pag]
        test eax, eax
        jz .x
        dec eax
        call pag_ir
        mov eax, [sy_max]
        call desplaza_v
.x:
        ret

; compone la página [pag] y prepara su aviso y su disposición
carga_pagina:
        pushad
        mov eax, [pag]
        mov edx, pdf_pagina
        call nuc
        mov eax, [pag]
        mov [pag_cargada], eax
        mov dword [tx_pag], -1
        call pon_aviso
        call dispone
        call inval
        popad
        ret

; el aviso de la página que hay compuesta: el más grave de los que tenga
pon_aviso:
        pushad
        mov dword [av_txt], 0
        mov eax, [pg_avisos]
        mov esi, t_av_filtro
        test eax, AV_FILTRO
        jnz .filtro
        mov esi, t_av_memoria
        test eax, AV_MEMORIA
        jnz .pon
        mov esi, t_av_roto
        test eax, AV_ROTO
        jnz .pon
        mov esi, t_av_ilegible
        test eax, AV_ILEGIBLE
        jnz .pon
        mov esi, t_av_llena
        test eax, AV_LLENA
        jnz .pon
        mov esi, t_av_descon
        test eax, AV_DESCON
        jnz .pon
        test eax, AV_IMAGEN
        jz .x
        mov esi, t_av_imagen        ; con el nombre de lo que no se sabe, si lo hay
        mov edi, aviso_buf
        call hvl_cad
        cmp byte [im_que], 0
        je .i1
        mov al, ' '
        stosb
        mov al, '('
        stosb
        mov esi, im_que
        call hvl_cad
        mov al, ')'
        stosb
.i1:
        mov esi, t_av_imagen2
        call hvl_cad
        mov byte [edi], 0
        mov esi, aviso_buf
.pon:
        mov [av_txt], esi
.x:
        popad
        ret
.filtro:
        mov edi, aviso_buf          ; con el nombre de la compresión, si se sabe
        call hvl_cad
        cmp byte [pg_filtro], 0
        je .f1
        mov al, ' '
        stosb
        mov al, '('
        stosb
        mov esi, pg_filtro
        call hvl_cad
        mov al, ')'
        stosb
.f1:
        mov esi, t_av_filtro2
        call hvl_cad
        mov byte [edi], 0
        mov esi, aviso_buf
        jmp .pon

; el texto de la página compuesta, en tx_buf (si no está ya)
texto_pagina:
        push eax
        push edx
        mov eax, [pag_cargada]
        cmp eax, [tx_pag]
        je .x
        mov [tx_pag], eax
        mov edx, tx_extrae
        call nuc
.x:
        pop edx
        pop eax
        ret

; =============================================================================
;  Teclado y ratón
; =============================================================================
tecla:
        cmp ecx, TC_CTRL
        jae .ctrl
        cmp ecx, TC_F1 + 2          ; F3
        jne .f5
        mov ecx, O_SIGUIENTE
        jmp orden
.f5:
        cmp ecx, TC_F1 + 4          ; F5
        jne .mov
        mov ecx, O_IRA
        jmp orden
.mov:
        cmp byte [hay_doc], 0
        je hecho
        cmp ecx, TC_ESC
        jne .m0
        mov dword [hl_n], 0         ; lo encontrado deja de estar marcado
        call ensucia
        jmp hecho
.m0:
        mov eax, [sy]
        cmp ecx, TC_ABAJO
        jne .m1
        add eax, 32
        jmp .v
.m1:
        cmp ecx, TC_ARRIBA
        jne .m2
        sub eax, 32
        jmp .v
.m2:
        cmp ecx, TC_INICIO
        jne .m3
        xor eax, eax
        jmp .v
.m3:
        cmp ecx, TC_FIN
        jne .m4
        mov eax, [sy_max]
        jmp .v
.m4:
        cmp ecx, TC_AVPAG
        jne .m5
        cmp eax, [sy_max]           ; al final de la página, a la siguiente
        jl .m4a
        mov ecx, O_SIG
        jmp orden
.m4a:
        add eax, [vh]
        sub eax, 32
        jmp .v
.m5:
        cmp ecx, TC_REPAG
        jne .m6
        test eax, eax               ; al principio, al final de la anterior
        jg .m5a
        call pag_anterior_fin
        jmp hecho
.m5a:
        sub eax, [vh]
        add eax, 32
        jmp .v
.m6:
        cmp ecx, TC_CTRL_INICIO
        jne .m7
        mov ecx, O_PRIMERA
        jmp orden
.m7:
        cmp ecx, TC_CTRL_FIN
        jne .m8
        mov ecx, O_ULTIMA
        jmp orden
.m8:
        mov eax, [sx]
        cmp ecx, TC_DER
        jne .m9
        add eax, 32
        jmp .h
.m9:
        cmp ecx, TC_IZQ
        jne hecho
        sub eax, 32
.h:
        call desplaza_h
        jmp hecho
.v:
        call desplaza_v
        jmp hecho
.ctrl:
        sub ecx, TC_CTRL
        cmp ecx, 26
        ja hecho
        movzx ecx, byte [ctrl_ord+ecx]
        test ecx, ecx
        jz hecho
        jmp orden
letra:
        cmp ecx, '+'
        jne .l1
        mov ecx, O_AMPLIAR
        jmp .o
.l1:
        cmp ecx, '-'
        jne hecho
        mov ecx, O_REDUCIR
.o:
        cmp byte [hay_doc], 0
        je hecho
        jmp orden

raton:
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jge .hoja
        movsx eax, dx
        mov esi, botones
        call br_boton_en
        jc hecho
        mov ecx, [esi+12]
        jmp orden
.hoja:
        ; en la zona de lectura: se coge la hoja y se arrastra
        cmp byte [hay_doc], 0
        je hecho
        mov eax, [vy]
        cmp ebx, eax
        jl hecho
        add eax, [vh]
        cmp ebx, eax
        jge hecho
        movsx eax, dx
        cmp eax, [vw]
        jge hecho
        mov [arr_x], eax
        mov [arr_y], ebx
        mov byte [arrastra], 1
        mov ebp, [ven]
        HV ven_capturar
        jmp hecho
raton_suelta:
        cmp byte [arrastra], 0
        je hecho
        mov byte [arrastra], 0
        HV ven_soltar
        jmp hecho

; ---- el botón secundario ----
; Se aprieta: apunta si ha sido en la zona de lectura (en la barra de botones
; no hay menú)
raton_der:
        call ctx_punto              ; EAX=x EBX=y
        cmp ebx, TB_AL
        setge byte [ctx_donde]
        jmp hecho
; El menú de la página: EDX=el punto de la pantalla, o -1 si se pide con el
; teclado (Mayús+F10 o la tecla de menú)
menu_ctx:
        cmp edx, -1
        je .abre
        cmp byte [ctx_donde], 0
        je hecho
.abre:
        mov esi, ctx_pagina
        mov edi, ctx_gris
        mov eax, -1
        call ctx_abre
        jmp hecho

; (ctx_gris) lo que ahora no se puede hacer: CF=1
cg_sin_buscar:                      ; no se ha buscado nada todavía
        cmp byte [busca_txt], 0
        je cg_no
cg_sin_doc:                         ; no hay ningún documento abierto
        cmp byte [hay_doc], 0
        je cg_no
cg_si:
        clc
        ret
cg_no:
        stc
        ret
cg_primera:                         ; ya es la primera página
        cmp dword [pag], 0
        je cg_no
        jmp cg_sin_doc
cg_ultima:                          ; ya es la última
        mov eax, [pag]
        inc eax
        cmp eax, [pdf_npag]
        jae cg_no
        jmp cg_sin_doc
cg_tope:                            ; ya está en el aumento mayor
        cmp byte [hay_doc], 0
        je cg_no
        call zoom_pct
        cmp eax, [zooms+(ZOOMS_N-1)*4]
        jae cg_no
        jmp cg_si
cg_minimo:                          ; ya está en el menor
        cmp byte [hay_doc], 0
        je cg_no
        call zoom_pct
        cmp eax, [zooms]
        jbe cg_no
        jmp cg_si
cg_una:                             ; el documento tiene una sola página
        cmp dword [pdf_npag], 1
        jbe cg_no
        jmp cg_sin_doc
raton_mueve:
        cmp byte [arrastra], 0
        je .barra
        movsx eax, dx               ; la hoja sigue al ratón
        mov ebx, edx
        sar ebx, 16
        mov ecx, [arr_x]
        sub ecx, eax
        mov [arr_x], eax
        mov edx, [arr_y]
        sub edx, ebx
        mov [arr_y], ebx
        mov eax, [sx]
        add eax, ecx
        call desplaza_h
        mov eax, [sy]
        add eax, edx
        call desplaza_v
        jmp hecho
.barra:
        ; sobre un botón de la barra: su nombre, abajo
        xor edi, edi
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jae .nombre
        test ebx, ebx
        js .nombre
        movsx eax, dx
        mov esi, botones
        call br_boton_en
        jc .nombre
        mov edi, [esi+16]
.nombre:
        call br_sobre_pon
        je hecho
        call inval
        jmp hecho

; =============================================================================
;  Abrir y cerrar
; =============================================================================
abrir:
        mov byte [nom_tmp], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_abrir
        HV dlg_archivo
        test eax, eax
        jz .x
        cmp byte [nom_tmp], 0
        je .x
        call abre_archivo
.x:
        ret

cerrar:
        call cierra_doc
        call pon_titulo
        call dispone
        jmp inval

; suelta el documento que haya
cierra_doc:
        pushad
        cmp byte [hay_bloque], 0
        je .x
        mov edx, [bloque]
        HV mem_bloque_soltar
        mov byte [hay_bloque], 0
.x:
        xor eax, eax
        mov [hay_doc], al
        mov [nombre], al
        mov [pdf_npag], eax
        mov [it_n], eax
        mov [hl_n], eax
        mov [av_txt], eax
        mov [pag], eax
        mov [sx], eax
        mov [sy], eax
        mov dword [pag_cargada], -1
        mov dword [tx_pag], -1
        mov dword [bus_pag], -1
        mov byte [sucio], 1
        popad
        ret

; abre el PDF nom_tmp
abre_archivo:
        pushad
        mov esi, nom_tmp
        xor al, al
        HV dos_abrir
        jnc .abierto
        mov esi, t_no_abrir
        call aviso
        jmp .x
.abierto:
        mov ebx, eax                ; EBX=el manejador
        mov al, 2                   ; cuánto ocupa: el puntero, al final...
        xor edx, edx
        HV dos_puntero
        jc .mal_leer
        mov [arch_tam], eax
        xor al, al                  ; ...y otra vez al principio
        xor edx, edx
        HV dos_puntero
        jc .mal_leer
        mov esi, t_e_grande
        cmp dword [arch_tam], ARCH_MAX
        ja .mal
        call cierra_doc             ; (el de antes deja su memoria)
        call pon_titulo
        ; un bloque para el archivo y, detrás, el almacén más grande que haya
        ; el almacén, según el documento: seis veces el archivo más un mega
        ; (de 1 a 8 MB); si no hay tanta memoria, la mitad, hasta medio mega
        mov edi, [arch_tam]
        cmp edi, 1024 * 1024
        jbe .a1
        mov edi, 1024 * 1024
.a1:
        imul edi, 6
        add edi, 1024 * 1024 + 256 * 1024
.pide:
        mov ecx, edi
        mov esi, t_e_memoria
        cmp ecx, 512 * 1024
        jb .mal
        mov [alm_tam], ecx
        add ecx, [arch_tam]
        add ecx, 64
        HV mem_bloque
        jnc .hay_mem
        shr edi, 1
        jmp .pide
.hay_mem:
        mov [bloque], edx
        mov byte [hay_bloque], 1
        mov [pdf_dat], eax
        mov ecx, [arch_tam]
        mov [pdf_tam], ecx
        add eax, ecx
        add eax, 3
        and eax, ~3
        mov [ar_base], eax
        add eax, [alm_tam]
        mov [ar_fin], eax
        ; el archivo, a trozos (de una vez, DOS no pasa de 64 K)
        mov edi, [pdf_dat]
.lee:
        mov ecx, [pdf_dat]
        add ecx, [pdf_tam]
        sub ecx, edi
        jz .leido
        cmp ecx, 16384
        jbe .l1
        mov ecx, 16384
.l1:
        HV dos_leer
        jc .mal_leer
        test eax, eax
        jz .mal_leer                ; (se acaba antes de lo que dijo)
        add edi, eax
        jmp .lee
.leido:
        HV dos_cerrar
        mov edx, pdf_abrir
        call nuc
        test eax, eax
        jnz .no_pdf
        mov byte [hay_doc], 1
        mov esi, nom_tmp
        mov edi, nombre
        mov ecx, 90
.n:
        lodsb
        stosb
        test al, al
        jz .n1
        dec ecx
        jnz .n
        mov byte [edi], 0
.n1:
        mov dword [pag], 0
        mov dword [sx], 0
        mov dword [sy], 0
        mov dword [zmodo], 1        ; se empieza viendo el ancho de la página
        mov dword [hl_n], 0
        mov dword [bus_pag], -1
        call pon_titulo
        call carga_pagina
        cmp dword [pdf_muchos], 0   ; más de lo que cabe: se dice, y se sigue
        je .x
        mov esi, t_muchos
        call aviso
        jmp .x
.no_pdf:
        mov esi, t_e_nopdf
        cmp eax, PE_NOPDF
        je .cierra
        mov esi, t_e_cifrado
        cmp eax, PE_CIFRADO
        je .cierra
        mov esi, t_e_memoria
        cmp eax, PE_MEMORIA
        je .cierra
        mov esi, t_e_sinpag
        cmp eax, PE_SINPAG
        je .cierra
        mov esi, t_e_roto
.cierra:
        call cierra_doc
        call dispone
        call inval
        call aviso
        jmp .x
.mal_leer:
        mov esi, t_no_leer
.mal:
        push esi
        HV dos_cerrar
        pop esi
        call cierra_doc
        call dispone
        call inval
        call aviso
.x:
        popad
        ret

; =============================================================================
;  Ir a, buscar
; =============================================================================
ir_a:
        mov edi, linea              ; «Número de página, de 1 a 7:»
        mov esi, t_q_ira
        call hvl_cad
        mov eax, [pdf_npag]
        call hvl_num
        mov al, ':'
        stosb
        mov byte [edi], 0
        mov byte [num_txt], 0
        mov ebp, [ven]
        mov esi, linea
        mov edi, t_ira
        mov ebx, num_txt
        mov ecx, 8
        HV dlg_pedir
        test eax, eax
        jz .x
        mov esi, num_txt
        call hvl_lee_num
        test eax, eax
        jz .x
        cmp eax, [pdf_npag]
        jbe .l
        mov eax, [pdf_npag]
.l:
        dec eax
        call pag_ir
.x:
        ret

buscar:
        mov ebp, [ven]
        mov esi, t_q_buscar
        mov edi, t_buscar
        mov ebx, busca_txt
        mov ecx, 40
        HV dlg_pedir
        test eax, eax
        jz buscar_sig.x
        cmp byte [busca_txt], 0
        je buscar_sig.x
        mov dword [bus_pag], -1     ; desde el principio de esta página
buscar_sig:
        cmp byte [busca_txt], 0
        je buscar
        pushad
        mov ebx, [pag]              ; EBX=la página que se mira
        xor eax, eax                ; EAX=desde dónde
        cmp ebx, [bus_pag]
        jne .e
        mov eax, [bus_pos]
.e:
        mov edi, [pdf_npag]         ; (la primera se mira dos veces: al dar
        inc edi                     ; la vuelta, desde su principio)
.pag:
        cmp ebx, [pag_cargada]
        je .p1
        push eax
        mov eax, ebx
        mov edx, pdf_pagina
        call nuc
        mov [pag_cargada], ebx
        mov dword [tx_pag], -1
        pop eax
.p1:
        call texto_pagina
        mov esi, busca_txt
        mov edx, tx_busca
        call nuc
        cmp eax, -1
        jne .hallado
        inc ebx
        cmp ebx, [pdf_npag]
        jb .p2
        xor ebx, ebx
.p2:
        xor eax, eax
        dec edi
        jnz .pag
        ; no está: se vuelve a la página que se veía
        mov dword [hl_n], 0
        mov dword [bus_pag], -1
        call carga_pagina
        mov esi, t_no_esta
        call aviso
        jmp .fin
.hallado:
        mov [bus_pag], ebx
        lea edx, [eax+1]
        mov [bus_pos], edx
        cmp ebx, [pag]
        je .h1
        mov [pag], ebx
        mov dword [sy], 0
.h1:
        call marca                  ; EAX=dónde ECX=cuántas letras
        call pon_aviso
        call dispone
        ; que se vea: la línea del primer trozo, a un tercio de la zona
        cmp dword [hl_n], 0
        je .h3
        mov eax, [hl_it]
        shl eax, 5
        mov eax, [it_tab+eax+IT_Y]
        call a_px
        add eax, MARG
        mov ebx, eax
        sub ebx, [sy]
        cmp ebx, 40
        jl .h2
        mov edx, [vh]
        sub edx, 24
        cmp ebx, edx
        jl .h3
.h2:
        mov ebx, [vh]
        xor edx, edx
        push eax
        mov eax, ebx
        mov ebx, 3
        div ebx
        mov ebx, eax
        pop eax
        sub eax, ebx
        cmp eax, [sy_max]
        jle .h2a
        mov eax, [sy_max]
.h2a:
        test eax, eax
        jns .h2b
        xor eax, eax
.h2b:
        mov [sy], eax
        call pon_barras
.h3:
        call ensucia
.fin:
        popad
.x:
        ret

; (interna) lo encontrado (en tx_buf, desde EAX, ECX letras), a la lista de
; trozos marcados: de cada elemento de la página, de qué letra a qué letra
marca:
        pushad
        mov dword [hl_n], 0
        mov edi, eax                ; EDI=la letra de tx_buf
        lea ebp, [eax+ecx]          ; EBP=el final
        mov ebx, -1                 ; EBX=el trozo que se va haciendo
.b:
        cmp edi, ebp
        jae .x
        movzx edx, word [tx_mapa+edi*2]
        cmp edx, 0xFFFF             ; (un espacio añadido: de ningún elemento)
        je .sig
        ; ¿qué letra del elemento es? Las de un elemento van seguidas
        mov esi, edi
.atras:
        test esi, esi
        jz .a1
        cmp [tx_mapa+esi*2-2], dx
        jne .a1
        dec esi
        jmp .atras
.a1:
        mov eax, edi
        sub eax, esi                ; EAX=su número en el elemento
        test ebx, ebx
        js .nuevo
        cmp [hl_it+ebx*4], edx
        jne .nuevo
        cmp [hl_b+ebx*4], eax
        jne .nuevo
        inc dword [hl_b+ebx*4]
        jmp .sig
.nuevo:
        mov ebx, [hl_n]
        cmp ebx, HL_MAX
        jae .x
        inc dword [hl_n]
        mov [hl_it+ebx*4], edx
        mov [hl_a+ebx*4], eax
        inc eax
        mov [hl_b+ebx*4], eax
.sig:
        inc edi
        jmp .b
.x:
        popad
        ret

; Edición > Copiar: el texto de la página, al portapapeles de Hispa Ventanas
; (por un cuadro de texto escondido, como Pluma); las líneas, acabadas en 10
copiar:
        pushad
        call texto_pagina
        mov esi, t_sin_texto
        cmp dword [tx_n], 0
        je .aviso
        mov esi, tx_buf
        mov ecx, [tx_n]
        mov edi, copia_buf
.c:
        lodsb
        cmp al, 13
        je .c1
        cmp edi, copia_buf + COPIA_MAX
        jae .c2
        stosb
.c1:
        dec ecx
        jnz .c
.c2:
        mov byte [edi], 0
        push ecx
        mov ebp, [puente]
        mov eax, WM_SETTEXT
        mov edx, copia_buf
        HV msg_enviar
        mov eax, EM_SEL_TODO
        HV msg_enviar
        mov eax, WM_COPY
        HV msg_enviar
        pop ecx
        mov esi, t_copia_larga
        test ecx, ecx
        jz .x
.aviso:
        call aviso
.x:
        popad
        ret

; =============================================================================
;  Guardar el texto
; =============================================================================
; (interna) pregunta el nombre del .TXT (propone el del PDF) -> CF=1 si no se
; sigue; el nombre queda en nom_tmp
pide_txt:
        mov esi, nombre             ; el nombre del PDF, sin carpeta ni extensión
        mov edi, nom_tmp
        mov ebx, edi
.c:
        lodsb
        stosb
        cmp al, '\'
        je .barra
        cmp al, ':'
        jne .c1
.barra:
        mov edi, nom_tmp            ; (lo de antes era la carpeta)
        mov ebx, edi
.c1:
        cmp al, '.'
        jne .c2
        lea ebx, [edi-1]
.c2:
        test al, al
        jnz .c
        cmp ebx, nom_tmp
        jne .c3
        lea ebx, [edi-1]            ; (sin extensión)
.c3:
        mov edi, ebx
        mov esi, t_ext_txt
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_guardar
        HV dlg_archivo
        test eax, eax
        jz .no
        cmp byte [nom_tmp], 0
        je .no
        mov esi, nom_tmp            ; sin extensión, .TXT
        xor edx, edx
.e:
        lodsb
        test al, al
        jz .e2
        cmp al, '.'
        jne .e1
        inc edx
.e1:
        cmp al, '\'
        jne .e
        xor edx, edx
        jmp .e
.e2:
        test edx, edx
        jnz .e3
        lea edi, [esi-1]
        mov esi, t_ext_txt
        call hvl_cad
        mov byte [edi], 0
.e3:
        mov esi, nom_tmp            ; que no sea el propio PDF
        mov edi, nombre
.i:
        mov al, [esi]
        mov ah, [edi]
        call .may
        xchg al, ah
        call .may
        cmp al, ah
        jne .si
        inc esi
        inc edi
        test al, al
        jnz .i
        mov esi, t_mismo
        call aviso
.no:
        stc
        ret
.si:
        clc
        ret
.may:
        cmp al, 'a'
        jb .m
        cmp al, 'z'
        ja .m
        sub al, 32
.m:
        ret

guarda_pag:
        call texto_pagina
        cmp dword [tx_n], 0
        jne .hay
        mov esi, t_sin_texto
        jmp aviso
.hay:
        call pide_txt
        jc .x
        mov esi, nom_tmp
        HV dos_crear
        jc .mal
        mov ebx, eax
        call texto_pagina           ; (el diálogo no cambia la página, pero por si acaso)
        mov esi, tx_buf
        mov ecx, [tx_n]
        HV dos_escribir
        pushf
        HV dos_cerrar
        popf
        jc .mal
        mov esi, t_guardado
        jmp aviso
.mal:
        mov esi, t_no_guardar
        jmp aviso
.x:
        ret

guarda_doc:
        call pide_txt
        jc .x
        mov esi, nom_tmp
        HV dos_crear
        jc .mal
        mov ebx, eax                ; EBX=el manejador
        xor edi, edi                ; EDI=la página
.pag:
        mov eax, edi
        mov edx, pdf_pagina
        call nuc
        mov [pag_cargada], edi
        mov dword [tx_pag], -1
        call texto_pagina
        push edi                    ; «--- Página 3 ---»
        lea eax, [edi+1]
        mov edi, linea
        mov esi, t_cab_pag
        call hvl_cad
        call hvl_num
        mov esi, t_cab_fin
        call hvl_cad
        mov ecx, edi
        sub ecx, linea
        pop edi
        mov esi, linea
        HV dos_escribir
        jc .mal_c
        mov esi, tx_buf
        mov ecx, [tx_n]
        HV dos_escribir
        jc .mal_c
        mov word [linea], 0x0A0D
        mov esi, linea
        mov ecx, 2
        HV dos_escribir
        jc .mal_c
        inc edi
        cmp edi, [pdf_npag]
        jb .pag
        HV dos_cerrar
        call carga_pagina           ; otra vez la página que se veía
        mov esi, t_guardado
        jmp aviso
.mal_c:
        HV dos_cerrar
        call carga_pagina
.mal:
        mov esi, t_no_guardar
        jmp aviso
.x:
        ret

; =============================================================================
;  Imprimir el texto (por el puerto paralelo, como Pluma en texto llano)
; =============================================================================
imprime_pag:
        mov byte [imp_todo], 0
        jmp imprime
imprime_doc:
        mov byte [imp_todo], 1
imprime:
        pushad
        mov eax, 0                  ; ¿hay puerto paralelo? (INT 11h)
        mov bl, 0x11
        call bios
        test ah, 0xC0
        jz .mal
        mov eax, 0x0200             ; ¿y la impresora está encendida?
        mov bl, 0x17
        call bios
        test ah, 0x28
        jnz .mal
        test ah, 0x10
        jz .mal
        xor edi, edi                ; EDI=la página
        cmp byte [imp_todo], 0
        jne .pag
        mov edi, [pag]
.pag:
        cmp edi, [pag_cargada]
        je .p1
        mov eax, edi
        mov edx, pdf_pagina
        call nuc
        mov [pag_cargada], edi
        mov dword [tx_pag], -1
.p1:
        call texto_pagina
        ; línea a línea, con un margen de cuatro espacios; las largas se parten
        mov esi, tx_buf
        mov ecx, [tx_n]
        xor ebx, ebx                ; EBX=la columna
.car:
        test ecx, ecx
        jz .pag_fin
        lodsb
        dec ecx
        cmp al, 13
        je .cr
        cmp al, 10
        je .car
        cmp ebx, 76
        jb .c1
        call .salto
        jc .mal_p
.c1:
        test ebx, ebx
        jnz .c2
        push eax
        mov al, ' '
        call imp_car
        call imp_car
        call imp_car
        call imp_car
        pop eax
        jc .mal_p
.c2:
        call imp_car
        jc .mal_p
        inc ebx
        jmp .car
.cr:
        call .salto
        jc .mal_p
        jmp .car
.pag_fin:
        mov al, 12                  ; la hoja, fuera
        call imp_car
        jc .mal_p
        cmp byte [imp_todo], 0
        je .fin
        inc edi
        cmp edi, [pdf_npag]
        jb .pag
.fin:
        call imp_lista
        jc .mal_p
        call .vuelve
        mov esi, t_impreso
        call aviso
        popad
        ret
.mal_p:
        call .vuelve
.mal:
        mov esi, t_e_imprimir
        call aviso
        popad
        ret
; (interna) fin de línea en la impresora
.salto:
        mov al, 13
        call imp_car
        jc .s1
        mov al, 10
        call imp_car
.s1:
        mov ebx, 0
        ret
; (interna) si se ha compuesto otra página, otra vez la que se veía
.vuelve:
        mov eax, [pag]
        cmp eax, [pag_cargada]
        je .v1
        call carga_pagina
.v1:
        ret

; AL: a la impresora. CF=1 si no se puede
imp_car:
        push eax
        push ebx
        call imp_lista
        jc .x
        movzx eax, al
        mov bl, 0x17
        call bios
        test ah, 0x28               ; error o sin papel
        jz .x                       ; (CF=0)
        stc
.x:
        pop ebx
        pop eax
        ret
; espera a que la impresora admita otro carácter, hasta veinte segundos (una
; de verdad se para a ratos, con la memoria llena); CF=1 si no contesta
imp_lista:
        push eax
        push ebx
        push edx
        HV tics
        mov edx, eax
.l:
        mov eax, 0x0200
        mov bl, 0x17
        call bios
        test ah, 0x28
        jnz .no
        test ah, 0x80               ; no está ocupada
        jnz .si
        HV tics
        sub eax, edx
        cmp eax, 20 * 18
        jb .l
.no:
        stc
        jmp .x
.si:
        clc
.x:
        pop edx
        pop ebx
        pop eax
        ret
; la interrupción BL de la BIOS con EAX y DX=0 (LPT1) -> EAX
bios:
        push ecx
        push esi
        push edi
        push eax
        mov edi, rl
        mov ecx, RL_TAM / 4
        xor eax, eax
        rep stosd
        pop eax
        mov [rl+RL_EAX], eax
        mov [rl+RL_INT], bl
        mov esi, rl
        HV rm_interrup
        mov eax, [rl+RL_EAX]
        pop edi
        pop esi
        pop ecx
        ret

; =============================================================================
;  Imprimir como se ve: la página, punto por punto, en el modo gráfico de las
;  impresoras de agujas (ESC/P), como «Imprimir como se ve» de Pluma y la
;  impresión de Mirador: 72 puntos por pulgada a lo ancho («ESC * 5», 576
;  puntos en la hoja) y tiras de 8 agujas con el avance a 8/72 de pulgada,
;  así que los puntos son cuadrados. Un punto del PDF es uno del papel (la
;  página sale a su tamaño de verdad: una etiqueta mide lo que tiene que
;  medir), salvo que no quepa en 576 x 760: entonces se reduce lo justo.
;  Va centrada a lo ancho y sin la sombra, el fondo ni lo marcado al buscar.
;  Se pinta con lo mismo que la pantalla (dibuja_lista), por franjas de
;  IG_FR filas, en un mapa propio (ig_buf). Cada punto pasa a negro o blanco
;  según lo claro que sea su color: lo oscuro (el texto, las rayas, las
;  barras de un código de barras), negro macizo; el blanco y el crema, en
;  blanco; lo de en medio (grises, colores claros, fotos), con un tramado
;  ordenado de 4x4 (Bayer), que no toca lo macizo: los bordes quedan limpios.
; =============================================================================
IG_AN       equ 576                 ; puntos de ancho de la hoja (8 pulgadas)
IG_AL       equ 760                 ; y de alto que se usan
IG_FR       equ 128                 ; filas que se pintan de una vez

ig_ini      db 27, '@', 27, 'A', 8  ; la impresora, como de fábrica; avance de 8/72
ig_graf     db 27, '*', 5, IG_AN & 0xFF, IG_AN >> 8
ig_fin      db 27, '2'              ; al acabar, el avance de siempre (1/6)
; el tramado: un punto es negro si su claridad (0-255) es menor que la de su
; casilla (de 100 a 243, en el orden de Bayer); por debajo de 100, siempre
; negro; de 243 arriba (el crema, el blanco), nunca
ig_umbral   db 100, 176, 119, 195
            db 214, 138, 233, 157
            db 129, 205, 110, 186
            db 243, 167, 224, 148

imprime_gpag:
        mov byte [imp_todo], 0
        jmp imprime_g
imprime_gdoc:
        mov byte [imp_todo], 1
imprime_g:
        pushad
        mov eax, 0                  ; ¿hay puerto paralelo? (INT 11h)
        mov bl, 0x11
        call bios
        test ah, 0xC0
        jz .mal
        mov eax, 0x0200             ; ¿y la impresora está encendida?
        mov bl, 0x17
        call bios
        test ah, 0x28
        jnz .mal
        test ah, 0x10
        jz .mal
        ; la claridad de cada color de la paleta (rojo, verde, azul, 0)
        mov esi, [hv_api + HV_pal_rgb * 4]
        xor ecx, ecx
.lum:
        movzx eax, byte [esi+ecx*4]
        imul eax, 77
        movzx edx, byte [esi+ecx*4+1]
        imul edx, 150
        add eax, edx
        movzx edx, byte [esi+ecx*4+2]
        imul edx, 29
        add eax, edx
        shr eax, 8
        mov [ig_lum+ecx], al
        inc ecx
        cmp ecx, 256
        jb .lum
        ; lo que se cambia para pintar en el papel, guardado
        mov esi, ig_vars
        mov edi, ig_guarda
        mov ecx, IG_NVARS
.g:
        mov eax, [esi]
        mov eax, [eax]
        stosd
        add esi, 4
        loop .g
        mov dword [hl_n], 0         ; (lo marcado al buscar no se imprime)
        mov esi, ig_ini
        mov ecx, 5
        call ig_manda
        jc .mal_p
        xor edi, edi                ; EDI=la página
        cmp byte [imp_todo], 0
        jne .pag
        mov edi, [pag]
.pag:
        cmp edi, [pag_cargada]
        je .p1
        mov eax, edi
        mov edx, pdf_pagina
        call nuc
        mov [pag_cargada], edi
        mov dword [tx_pag], -1
.p1:
        call ig_pagina
        jc .mal_p
        mov al, 12                  ; la hoja, fuera
        call imp_car
        jc .mal_p
        cmp byte [imp_todo], 0
        je .fin
        inc edi
        cmp edi, [pdf_npag]
        jb .pag
.fin:
        mov esi, ig_fin
        mov ecx, 2
        call ig_manda
        jc .mal_p
        call imp_lista
        jc .mal_p
        call .vuelve
        mov esi, t_impreso_g
        cmp byte [imp_todo], 0
        je .dice
        mov esi, t_impreso_d
.dice:
        call aviso
        popad
        ret
.mal_p:
        call .vuelve
.mal:
        mov esi, t_e_imprimir
        call aviso
        popad
        ret
; (interna) todo como estaba; y, si se ha compuesto otra, la página que se veía
.vuelve:
        mov esi, ig_vars
        mov edx, ig_guarda
        mov ecx, IG_NVARS
.v0:
        mov eax, [esi]
        mov ebx, [edx]
        mov [eax], ebx
        add esi, 4
        add edx, 4
        loop .v0
        mov eax, [pag]
        cmp eax, [pag_cargada]
        je .v1
        call carga_pagina
.v1:
        ret

; lo que la impresión cambia y luego deja como estaba
ig_vars     dd zoom, vw, vh, pw, ph, ox, oy, vbuf_p, hl_n
IG_NVARS    equ ($ - ig_vars) / 4

; (interna) la página compuesta, al papel: sus tiras, sin la hoja de detrás
; (eso lo pone quien llama). CF=1 si la impresora falla
ig_pagina:
        pushad
        ; la ampliación: 1 (un punto del PDF, uno del papel), o lo que quepa
        mov dword [zoom], 0x10000
        mov eax, IG_AN
        mov ebx, [pg_an]
        call dispone.entre
        cmp eax, [zoom]
        jae .z1
        mov [zoom], eax
.z1:
        mov eax, IG_AL
        mov ebx, [pg_al]
        call dispone.entre
        cmp eax, [zoom]
        jae .z2
        mov [zoom], eax
.z2:
        cmp dword [zoom], 0x0800    ; (como en la pantalla: no menos de un 2 %)
        jae .z3
        mov dword [zoom], 0x0800
.z3:
        mov eax, [pg_an]
        call a_px
        cmp eax, IG_AN
        jbe .w
        mov eax, IG_AN
.w:
        mov [pw], eax
        mov eax, [pg_al]
        call a_px
        cmp eax, IG_AL
        jbe .h
        mov eax, IG_AL
.h:
        mov [ph], eax
        mov eax, IG_AN              ; centrada a lo ancho
        sub eax, [pw]
        sar eax, 1
        mov [ox], eax
        mov dword [vw], IG_AN
        mov dword [vbuf_p], ig_buf
        xor ebp, ebp                ; EBP=la fila del papel donde empieza la franja
.franja:
        cmp ebp, [ph]
        jae .bien
        mov eax, [ph]
        sub eax, ebp
        cmp eax, IG_FR
        jbe .f1
        mov eax, IG_FR
.f1:
        mov [vh], eax
        mov eax, ebp
        neg eax
        mov [oy], eax
        mov edi, ig_buf             ; en blanco, y lo que haya en la página
        mov ecx, IG_AN * IG_FR / 4
        mov eax, C_BLANCO * 0x01010101
        rep stosd
        call vc_todo
        call dibuja_lista
        xor ebx, ebx                ; EBX=la fila de la franja
.tira:
        cmp ebx, [vh]
        jae .sig
        call ig_tira
        jc .mal
        add ebx, 8
        jmp .tira
.sig:
        add ebp, IG_FR
        jmp .franja
.bien:
        popad
        clc
        ret
.mal:
        popad
        stc
        ret

; (interna) la tira de 8 filas que empieza en la fila EBX de la franja (EBP:
; la fila del papel donde empieza la franja), a la impresora; las filas que
; pasan de [vh], en blanco. CF=1 si falla
ig_tira:
        pushad
        mov edi, ig_cols
        mov ecx, IG_AN / 4
        xor eax, eax
        rep stosd
        mov byte [ig_mask], 0x80    ; la aguja de arriba, el bit 7
        mov byte [ig_hay], 0
        mov edi, ebx                ; EDI=la fila
.fila:
        cmp edi, [vh]
        jae .manda
        lea eax, [ebp+edi]          ; EDX=los umbrales de su fila del papel
        and eax, 3
        lea edx, [ig_umbral+eax*4]
        mov esi, edi
        imul esi, IG_AN
        add esi, ig_buf
        xor ecx, ecx                ; ECX=la columna
.pt:
        movzx ebx, byte [esi+ecx]
        mov bl, [ig_lum+ebx]
        mov eax, ecx
        and eax, 3
        cmp bl, [edx+eax]
        jae .blanco
        mov al, [ig_mask]
        or [ig_cols+ecx], al
        mov byte [ig_hay], 1
.blanco:
        inc ecx
        cmp ecx, IG_AN
        jb .pt
        inc edi
        shr byte [ig_mask], 1
        jnz .fila
.manda:
        cmp byte [ig_hay], 0        ; (una tira en blanco: solo el avance)
        je .avanza
        mov esi, ig_graf
        mov ecx, 5
        call ig_manda
        jc .mal
        mov esi, ig_cols
        mov ecx, IG_AN
        call ig_manda
        jc .mal
.avanza:
        mov al, 13
        call imp_car
        jc .mal
        mov al, 10
        call imp_car
        jc .mal
        popad
        clc
        ret
.mal:
        popad
        stc
        ret

; (interna) manda ECX octetos de ESI; CF=1 si falla
ig_manda:
        lodsb
        call imp_car
        jc .x
        dec ecx
        jnz ig_manda
.x:
        ret

; =============================================================================
;  Información del documento
; =============================================================================
informacion:
        pushad
        mov edi, info_buf
        mov esi, t_i_archivo
        call hvl_cad
        mov esi, nombre
        call hvl_cad
        mov al, ' '
        stosb
        mov al, '('
        stosb
        mov eax, [pdf_tam]
        add eax, 1023
        shr eax, 10
        call hvl_num
        mov esi, t_i_kb
        call hvl_cad
        mov al, 10
        stosb
        mov esi, t_i_titulo
        mov ebx, k_title
        call .dato
        mov esi, t_i_autor
        mov ebx, k_author
        call .dato
        mov esi, t_i_asunto
        mov ebx, k_subject
        call .dato_si
        mov esi, t_i_hecho
        mov ebx, k_creator
        call .dato_si
        jnc .p
        mov esi, t_i_hecho
        mov ebx, k_producer
        call .dato_si
.p:
        mov esi, t_i_paginas
        call hvl_cad
        mov eax, [pdf_npag]
        call hvl_num
        mov al, 10
        stosb
        mov esi, t_i_version
        call hvl_cad
        mov eax, [pdf_ver]
        shr eax, 4
        call hvl_num
        mov al, '.'
        stosb
        mov eax, [pdf_ver]
        and eax, 15
        call hvl_num
        mov al, 10
        stosb
        mov esi, t_i_tam            ; en milímetros: puntos x 25,4 / 72
        call hvl_cad
        mov eax, [pg_an]
        call .mm
        mov esi, t_i_por
        call hvl_cad
        mov eax, [pg_al]
        call .mm
        mov esi, t_i_mm
        call hvl_cad
        cmp dword [pdf_reparado], 0
        je .i_muchos
        mov al, 10
        stosb
        stosb
        mov esi, t_i_reparado
        call hvl_cad
.i_muchos:
        cmp dword [pdf_muchos], 0
        je .fin
        mov al, 10
        stosb
        stosb
        mov esi, t_i_muchos
        call hvl_cad
.fin:
        mov byte [edi], 0
        mov ebp, [ven]
        mov esi, info_buf
        mov edi, t_info
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret
; (interna) ESI=rótulo EBX=clave: una línea con ese dato; si no está, «(no lo dice)»
.dato:
        call .dato_si
        jc .d1
        ret
.d1:
        call hvl_cad
        mov esi, t_i_nada
        call hvl_cad
        mov al, 10
        stosb
        ret
; (interna) lo mismo, pero si no está no pone nada y vuelve con CF=1 (y ESI)
.dato_si:
        push esi
        push edi
        mov edi, ebx
        mov ebx, dato_buf
        mov ecx, 60
        mov edx, pdf_dato
        call nuc
        pop edi
        pop esi
        test eax, eax
        jz .ds_no
        call hvl_cad
        mov esi, dato_buf
        call hvl_cad
        mov al, 10
        stosb
        clc
        ret
.ds_no:
        stc
        ret
; (interna) EAX=puntos (16.16): los escribe en milímetros
.mm:
        push edx
        push ebx
        mov ebx, 127 * 65536 / 360  ; 25,4 / 72, en 16.16
        imul ebx
        shrd eax, edx, 16
        add eax, 0x8000
        shr eax, 16
        pop ebx
        pop edx
        jmp hvl_num

; =============================================================================
;  Pintar la ventana
; =============================================================================
pintar:
        HV dc_pintar
        ; la barra de botones
        mov ecx, [cli_an]
        mov edx, TB_AL
        call br_fondo
        mov edi, botones
        mov ebx, 4
        call br_fila
        ; la franja del aviso
        cmp dword [av_txt], 0
        je .zona
        COLOR C_CREMA
        xor eax, eax
        mov ebx, TB_AL
        mov ecx, [cli_an]
        mov edx, AVISO_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        mov ebx, TB_AL + AVISO_AL - 1
        mov edx, 1
        HV dc_rect
        COLOR C_GRANATE
        mov eax, 8
        mov ebx, TB_AL + 2
        mov esi, [av_txt]
        HV dc_texto
.zona:
        ; la zona de lectura: el mapa de puntos, rehecho si hace falta
        COLOR C_CREMA
        xor eax, eax
        mov ebx, [vy]
        mov ecx, [cli_an]
        mov edx, [cli_al]
        sub edx, ebx
        sub edx, BR_PIE
        jle .pie
        HV dc_rect
        cmp dword [vw], 0
        je .esquina
        cmp dword [vh], 0
        je .esquina
        cmp byte [sucio], 0
        je .copia
        call dibuja
        mov byte [sucio], 0
.copia:
        xor eax, eax
        mov ebx, [vy]
        mov ecx, [vw]
        mov edx, [vh]
        mov esi, [vbuf_p]
        HV dc_copiar
.esquina:
        COLOR C_ARENA               ; el rincón entre las dos barras
        mov eax, [cli_an]
        sub eax, BD_AN
        mov ebx, [vy]
        add ebx, [vh]
        mov ecx, BD_AN
        mov edx, BD_AN
        HV dc_rect
.pie:
        call pinta_pie
        HV dc_cerrar
        jmp hecho

; lo que pregunta la barra común de cada botón: ninguno se queda «puesto»
barra_estado:
        xor ebp, ebp
        ret

; el pie: «Página 2 de 7    100 %    NOMBRE.PDF»
pinta_pie:
        pushad
        mov ecx, [cli_an]
        mov edx, [cli_al]
        call br_pie                 ; (CF=1: ha puesto el nombre de un botón)
        jc .x
        mov esi, t_sin_doc
        cmp byte [hay_doc], 0
        je .pon
        mov edi, linea
        mov esi, t_pagina
        call hvl_cad
        mov eax, [pag]
        inc eax
        call hvl_num
        mov esi, t_de
        call hvl_cad
        mov eax, [pdf_npag]
        call hvl_num
        mov esi, t_sep
        call hvl_cad
        call zoom_pct
        call hvl_num
        mov esi, t_pct
        call hvl_cad
        mov esi, t_sep
        call hvl_cad
        mov esi, nombre
        call hvl_cad
        mov byte [edi], 0
        mov esi, linea
.pon:
        COLOR C_TINTA
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, BR_PIE - 3
        HV dc_texto
.x:
        popad
        ret

; =============================================================================
;  El dibujo de la página, en vbuf ([vw] x [vh] puntos, uno por octeto)
; =============================================================================
dibuja:
        pushad
        mov edi, [vbuf_p]           ; el fondo
        mov ecx, [vw]
        imul ecx, [vh]
        mov al, C_CREMA
        rep stosb
        call vc_todo
        cmp byte [hay_doc], 0
        jne .doc
        ; sin documento: una pista, en el centro
        mov byte [vb_color], C_ARENA_OSC
        mov eax, [vw]
        sub eax, 51 * 8
        sar eax, 1
        mov ebx, [vh]
        sar ebx, 1
        sub ebx, 8
        mov esi, t_pista
        call vb_txt8
        jmp .x
.doc:
        ; dónde cae la hoja: centrada si cabe; si no, según el desplazamiento
        mov eax, [vw]
        sub eax, [pw]
        sar eax, 1
        cmp dword [sx_max], 0
        je .o1
        mov eax, MARG
        sub eax, [sx]
.o1:
        mov [ox], eax
        mov eax, MARG
        sub eax, [sy]
        mov [oy], eax
        ; la sombra, la hoja y su filete
        mov byte [vb_color], C_ARENA_OSC
        mov eax, [ox]
        mov ebx, [oy]
        add eax, 3
        add ebx, 3
        mov ecx, [pw]
        mov edx, [ph]
        call vb_rect
        mov eax, [ox]
        mov ebx, [oy]
        dec eax
        dec ebx
        add ecx, 2
        add edx, 2
        call vb_rect
        mov byte [vb_color], C_BLANCO
        mov eax, [ox]
        mov ebx, [oy]
        mov ecx, [pw]
        mov edx, [ph]
        call vb_rect
        call dibuja_lista
.x:
        popad
        ret

; lo que hay en la página, en su orden, con la hoja en [ox], [oy] (lo usa
; también la impresión como se ve)
dibuja_lista:
        pushad
        mov esi, it_tab
        xor ebx, ebx
.it:
        cmp ebx, [it_n]
        jae .x
        call vc_pon                 ; su recorte
        mov al, [esi+IT_COLOR]
        mov [vb_color], al
        mov al, [esi+IT_TIPO]
        cmp al, ITT_TEXTO
        jne .i1
        call vb_texto
        jmp .sig
.i1:
        cmp al, ITT_RECT
        jne .i2
        call vb_rect_it
        jmp .sig
.i2:
        cmp al, ITT_LINEA
        jne .i3
        call vb_linea_it
        jmp .sig
.i3:
        cmp al, ITT_POLI
        jne .i4
        call vb_poli
        jmp .sig
.i4:
        cmp al, ITT_IMAGEN
        jne .i5
        call vb_imagen
        jmp .sig
.i5:
        cmp al, ITT_ILEGIBLE
        jne .sig
        call vb_ilegible
.sig:
        add esi, IT_TAM
        inc ebx
        jmp .it
.x:
        popad
        ret

; el recorte: toda la zona de lectura
vc_todo:
        push eax
        xor eax, eax
        mov [vc_x0], eax
        mov [vc_y0], eax
        mov eax, [vw]
        mov [vc_x1], eax
        mov eax, [vh]
        mov [vc_y1], eax
        pop eax
        ret
; el recorte del elemento ESI: su rectángulo de la página (si lo tiene), a
; puntos de la zona
vc_pon:
        push eax
        push ebx
        call vc_todo
        movzx ebx, word [esi+IT_CLIP]
        test ebx, ebx
        jz .x
        shl ebx, 4
        add ebx, cl_tab
        mov eax, [ebx]
        call a_px_x
        cmp eax, [vc_x0]
        jle .a
        mov [vc_x0], eax
.a:
        mov eax, [ebx+4]
        call a_px_y
        cmp eax, [vc_y0]
        jle .b
        mov [vc_y0], eax
.b:
        mov eax, [ebx+8]
        call a_px_x
        cmp eax, [vc_x1]
        jge .c
        mov [vc_x1], eax
.c:
        mov eax, [ebx+12]
        call a_px_y
        cmp eax, [vc_y1]
        jge .x
        mov [vc_y1], eax
.x:
        pop ebx
        pop eax
        ret

; un rectángulo del color [vb_color]: EAX=x EBX=y ECX=ancho EDX=alto, en
; puntos de la zona; lo que se sale no se pinta
vb_rect:
        pushad
        test ecx, ecx
        jle .x
        test edx, edx
        jle .x
        add ecx, eax                ; ECX, EDX=donde acaba; todo, dentro del recorte
        add edx, ebx
        cmp eax, [vc_x0]
        jge .a
        mov eax, [vc_x0]
.a:
        cmp ecx, [vc_x1]
        jle .b
        mov ecx, [vc_x1]
.b:
        sub ecx, eax
        jle .x
        cmp ebx, [vc_y0]
        jge .c
        mov ebx, [vc_y0]
.c:
        cmp edx, [vc_y1]
        jle .d
        mov edx, [vc_y1]
.d:
        sub edx, ebx
        jle .x
        imul ebx, [vw]
        mov edi, [vbuf_p]
        add edi, ebx
        add edi, eax
        mov al, [vb_color]
        mov ebx, ecx
.f:
        mov ecx, ebx
        push edi
        rep stosb
        pop edi
        add edi, [vw]
        dec edx
        jnz .f
.x:
        popad
        ret

; ESI=un ITT_RECT: relleno; de un punto de pantalla al menos (una raya fina
; hecha con un rectángulo tiene que verse)
vb_rect_it:
        pushad
        mov eax, [esi+IT_X]
        call a_px_x
        mov edi, eax
        mov eax, [esi+IT_X]
        add eax, [esi+IT_AN]
        call a_px_x
        mov ecx, eax
        sub ecx, edi
        jg .w
        mov ecx, 1
.w:
        mov eax, [esi+IT_Y]
        call a_px_y
        mov ebx, eax
        mov eax, [esi+IT_Y]
        add eax, [esi+IT_AL]
        call a_px_y
        mov edx, eax
        sub edx, ebx
        jg .h
        mov edx, 1
.h:
        mov eax, edi
        call vb_rect
        popad
        ret

; ESI=un ITT_LINEA: de (IT_X, IT_Y) a (IT_AN, IT_AL), con el grosor IT_TXT
vb_linea_it:
        pushad
        mov eax, [esi+IT_TXT]
        call a_px
        cmp eax, 1
        jge .g
        mov eax, 1
.g:
        cmp eax, 64
        jle .g1
        mov eax, 64
.g1:
        mov [vb_gros], eax
        mov eax, [esi+IT_X]
        call a_px_x
        mov edi, eax
        mov eax, [esi+IT_Y]
        call a_px_y
        mov ebx, eax
        mov eax, [esi+IT_AN]
        call a_px_x
        mov ecx, eax
        mov eax, [esi+IT_AL]
        call a_px_y
        mov edx, eax
        mov eax, edi
        call vb_linea
        popad
        ret

; una raya de (EAX, EBX) a (ECX, EDX), del grosor [vb_gros] y el color [vb_color]
vb_linea:
        pushad
        mov ebp, [vb_gros]
        mov esi, ebp
        shr esi, 1                  ; ESI=medio grosor
        ; del todo fuera: nada
        mov edi, [vw]
        add edi, ebp
        cmp eax, edi
        jl .f1
        cmp ecx, edi
        jge .x
.f1:
        mov edi, ebp
        neg edi
        cmp eax, edi
        jg .f2
        cmp ecx, edi
        jle .x
.f2:
        mov edi, [vh]
        add edi, ebp
        cmp ebx, edi
        jl .f3
        cmp edx, edi
        jge .x
.f3:
        mov edi, ebp
        neg edi
        cmp ebx, edi
        jg .f4
        cmp edx, edi
        jle .x
.f4:
        cmp ebx, edx
        je .horizontal
        cmp eax, ecx
        je .vertical
        ; inclinada (Bresenham): un cuadrado del grosor en cada punto
        mov [ln_x], eax
        mov [ln_y], ebx
        mov [ln_x1], ecx
        mov [ln_y1], edx
        mov edi, 1                  ; EDI=paso de x
        sub ecx, eax
        jns .dx
        neg ecx
        neg edi
.dx:
        mov [ln_dx], ecx
        mov [ln_sx], edi
        mov edi, 1
        sub edx, ebx
        jns .dy
        neg edx
        neg edi
.dy:
        mov [ln_dy], edx
        mov [ln_sy], edi
        mov edi, ecx                ; EDI=el error: dx - dy
        sub edi, edx
.p:
        mov eax, [ln_x]
        mov ebx, [ln_y]
        sub eax, esi
        sub ebx, esi
        mov ecx, ebp
        mov edx, ebp
        call vb_rect
        mov eax, [ln_x]
        cmp eax, [ln_x1]
        jne .p1
        mov eax, [ln_y]
        cmp eax, [ln_y1]
        je .x
.p1:
        mov eax, edi
        shl eax, 1
        mov edx, [ln_dy]
        neg edx
        cmp eax, edx
        jle .p2
        sub edi, [ln_dy]
        mov edx, [ln_sx]
        add [ln_x], edx
.p2:
        cmp eax, [ln_dx]
        jge .p
        add edi, [ln_dx]
        mov edx, [ln_sy]
        add [ln_y], edx
        jmp .p
.horizontal:
        cmp eax, ecx
        jle .h1
        xchg eax, ecx
.h1:
        sub ecx, eax
        inc ecx
        sub ebx, esi
        mov edx, ebp
        call vb_rect
        jmp .x
.vertical:
        cmp ebx, edx
        jle .v1
        xchg ebx, edx
.v1:
        sub edx, ebx
        inc edx
        sub eax, esi
        mov ecx, ebp
        call vb_rect
.x:
        popad
        ret

; ESI=un ITT_IMAGEN: un recuadro gris que lo dice
vb_imagen:
        pushad
        mov eax, [esi+IT_X]
        call a_px_x
        mov edi, eax
        mov eax, [esi+IT_X]
        add eax, [esi+IT_AN]
        call a_px_x
        mov ecx, eax
        sub ecx, edi
        mov eax, [esi+IT_Y]
        call a_px_y
        mov ebx, eax
        mov eax, [esi+IT_Y]
        add eax, [esi+IT_AL]
        call a_px_y
        mov edx, eax
        sub edx, ebx
        mov eax, edi
        cmp dword [esi+IT_TXT], 0
        jne .pinta
        cmp ecx, 2
        jl .x
        cmp edx, 2
        jl .x
        mov byte [vb_color], C_ARENA_OSC
        call vb_rect
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        mov byte [vb_color], C_PIEDRA
        call vb_rect
        cmp ecx, 70                 ; el letrero, si cabe: en el centro
        jl .x
        cmp edx, 18
        jl .x
        sub ecx, 64
        sar ecx, 1
        add eax, ecx
        sub edx, 16
        sar edx, 1
        add ebx, edx
        mov byte [vb_color], C_MARRON
        mov esi, t_imagen
        call vb_txt8
.x:
        popad
        ret
.pinta:
        ; la miniatura, estirada o encogida a su sitio (EAX, EBX, de ECX x EDX):
        ; de cada punto de la pantalla, el suyo de la miniatura
        test ecx, ecx
        jg .p0
        mov ecx, 1
.p0:
        test edx, edx
        jg .p1
        mov edx, 1
.p1:
        mov [im_px], eax
        mov [im_py], ebx
        mov [im_pw], ecx
        mov [im_ph], edx
        movzx eax, byte [esi+IT_BAND]
        mov [im_pb], eax
        mov esi, [esi+IT_TXT]       ; ESI=la miniatura: ancho, alto y puntos
        mov ebp, [im_py]            ; EBP=la fila de la pantalla
        cmp ebp, [vc_y0]
        jge .fy
        mov ebp, [vc_y0]
.fy:
        mov eax, [im_py]
        add eax, [im_ph]
        cmp ebp, eax
        jge .x
        cmp ebp, [vc_y1]
        jge .x
        mov edi, [im_px]            ; EDI=la columna
        cmp edi, [vc_x0]
        jge .f0
        mov edi, [vc_x0]
.f0:
        mov eax, [im_px]
        add eax, [im_pw]
        cmp edi, eax
        jge .f_sig
        test byte [im_pb], II_GIRADA
        jnz .fx
        ; Lo corriente (sin girar), sin una sola división por punto: las filas
        ; de la miniatura que caen en esta de la pantalla se calculan una vez,
        ; y las columnas se van llevando con su resto, de punto en punto
        mov eax, ebp
        sub eax, [im_py]
        mov ebx, eax
        mul dword [esi+4]
        div dword [im_ph]
        mov [im_r0], eax
        lea eax, [ebx+1]
        mul dword [esi+4]
        div dword [im_ph]
        sub eax, [im_r0]
        jg .rr1
        mov eax, 1
.rr1:
        mov [im_rn], eax
        test byte [im_pb], II_VUELTA
        jz .rr2
        mov eax, [esi+4]
        sub eax, [im_r0]
        sub eax, [im_rn]
        jns .rr1b
        xor eax, eax
.rr1b:
        mov [im_r0], eax
.rr2:
        mov eax, [im_r0]
        mul dword [esi]
        lea eax, [esi+eax+8]
        mov [im_fila_p], eax        ; la primera de esas filas
        mov eax, ebp
        imul eax, [vw]
        add eax, [vbuf_p]
        mov [im_vfila], eax         ; y la de la pantalla
        mov eax, edi
        sub eax, [im_px]
        mul dword [esi]
        div dword [im_pw]
        mov [im_cc], eax            ; la columna de la miniatura por la que se va
        mov [im_ca], edx            ; y lo que sobra
.rx:
        mov eax, [im_px]
        add eax, [im_pw]
        cmp edi, eax
        jge .f_sig
        cmp edi, [vc_x1]
        jge .f_sig
        mov ebx, [im_cc]
        mov eax, [im_ca]
        add eax, [esi]
        mov ecx, [im_pw]
.rd:
        cmp eax, ecx
        jb .rd1
        sub eax, ecx
        inc dword [im_cc]
        jmp .rd
.rd1:
        mov [im_ca], eax
        mov eax, [im_cc]
        sub eax, ebx                ; EBX=la primera columna EAX=cuántas (una, al menos)
        jnz .rc1
        inc eax
.rc1:
        test byte [im_pb], II_ESPEJO
        jz .rc2
        mov ecx, [esi]
        sub ecx, ebx
        sub ecx, eax
        jns .rc1b
        xor ecx, ecx
.rc1b:
        mov ebx, ecx
.rc2:
        cmp eax, 1
        jne .rcaja
        cmp dword [im_rn], 1
        jne .rcaja
        mov eax, [im_fila_p]        ; un punto solo (al ampliar o a su tamaño): tal cual
        mov al, [eax+ebx]
        jmp .rpon
.rcaja:
        mov [im_c0], ebx
        mov [im_cn], eax
        call vb_im_caja             ; varios (al encoger): su media
.rpon:
        cmp al, IM_NADA
        je .r4
        mov ebx, [im_vfila]
        mov [ebx+edi], al
.r4:
        inc edi
        jmp .rx
        ; girada: punto a punto, con sus cuentas
.fx:
        mov eax, [im_px]
        add eax, [im_pw]
        cmp edi, eax
        jge .f_sig
        cmp edi, [vc_x1]
        jge .f_sig
        mov eax, edi                ; a lo ancho y a lo alto del sitio: de 0 a casi 1
        sub eax, [im_px]
        mov ebx, ebp
        sub ebx, [im_py]
        mov ecx, [im_pw]
        mov edx, [im_ph]
        test byte [im_pb], II_GIRADA
        jz .o1
        xchg eax, ebx               ; girada: las columnas van a lo alto
        xchg ecx, edx
.o1:
        ; las columnas de la miniatura que caen en este punto: de
        ; x * ancho / sitio a (x + 1) * ancho / sitio (una, al menos)
        push edx
        push ebx
        mov ebx, eax
        mul dword [esi]
        div ecx
        mov [im_c0], eax
        lea eax, [ebx+1]
        mul dword [esi]
        div ecx
        sub eax, [im_c0]
        jg .c1
        mov eax, 1
.c1:
        mov [im_cn], eax
        test byte [im_pb], II_ESPEJO
        jz .c2
        mov eax, [esi]
        sub eax, [im_c0]
        sub eax, [im_cn]
        jns .c1b
        xor eax, eax
.c1b:
        mov [im_c0], eax
.c2:
        pop ebx
        pop ecx
        mov eax, ebx                ; y las filas, lo mismo
        mul dword [esi+4]
        div ecx
        mov [im_r0], eax
        lea eax, [ebx+1]
        mul dword [esi+4]
        div ecx
        sub eax, [im_r0]
        jg .r1
        mov eax, 1
.r1:
        mov [im_rn], eax
        test byte [im_pb], II_VUELTA
        jz .r2
        mov eax, [esi+4]
        sub eax, [im_r0]
        sub eax, [im_rn]
        jns .r1b
        xor eax, eax
.r1b:
        mov [im_r0], eax
.r2:
        mov eax, [im_cn]
        or eax, [im_rn]
        cmp eax, 1
        jne .caja
        mov eax, [im_r0]
        mul dword [esi]
        add eax, [im_c0]
        mov al, [esi+8+eax]
        jmp .o3
.caja:
        call vb_im_caja
.o3:
        cmp al, IM_NADA
        je .o4
        mov ebx, ebp
        imul ebx, [vw]
        add ebx, edi
        add ebx, [vbuf_p]
        mov [ebx], al
.o4:
        inc edi
        jmp .fx
.f_sig:
        inc ebp
        jmp .fy

; (interna) ESI=una miniatura; [im_c0], [im_cn], [im_r0], [im_rn]=un cuadro
; suyo; EDI, EBP=el punto de la pantalla -> AL=el color de la media del cuadro
; (IM_NADA si casi todo él deja ver lo de debajo). La miniatura va ya en la
; paleta y tramada: al encogerla, quedarse con un punto de cada tantos haría
; aguas con el tramado y se comería rayas de las letras; se deshace la paleta,
; se promedia (hasta 4 x 4 puntos) y se vuelve a tramar, ya en la pantalla
vb_im_caja:
        push ebx
        push ecx
        push edx
        cmp byte [im_tab_hecha], 0
        jne .hay
        call vb_im_tabla
.hay:
        xor eax, eax
        mov [im_s], eax
        mov [im_s+4], eax
        mov [im_s+8], eax
        mov [im_sn], eax
        mov [im_st], eax
        mov dword [im_s1], -1
        mov eax, [im_cn]            ; los pasos: no más de cuatro por lado
        add eax, 3
        shr eax, 2
        mov [im_sc], eax
        mov eax, [im_rn]
        add eax, 3
        shr eax, 2
        mov [im_sr], eax
        mul dword [esi]
        mov [im_spaso], eax         ; de una fila de las que se miran a la siguiente
        mov eax, [im_r0]
        mul dword [esi]
        add eax, [im_c0]
        lea ebx, [esi+eax+8]        ; EBX=el primer punto de la fila
        xor ecx, ecx                ; ECX=la fila, dentro del cuadro
.f:
        xor edx, edx                ; EDX=la columna, dentro del cuadro
.c:
        movzx eax, byte [ebx+edx]
        cmp al, IM_NADA
        jne .c1
        inc dword [im_st]
        jmp .c3
.c1:
        cmp dword [im_s1], -1       ; ¿son todos iguales? (lo liso se queda como está)
        jne .c2
        mov [im_s1], eax
.c2:
        cmp eax, [im_s1]
        je .c2b
        mov dword [im_s1], -2
.c2b:
        mov eax, [im_tab_rgb+eax*4]
        push ebx
        movzx ebx, al
        add [im_s], ebx
        movzx ebx, ah
        add [im_s+4], ebx
        shr eax, 16
        add [im_s+8], eax
        pop ebx
        inc dword [im_sn]
.c3:
        add edx, [im_sc]
        cmp edx, [im_cn]
        jb .c
        add ebx, [im_spaso]
        add ecx, [im_sr]
        cmp ecx, [im_rn]
        jb .f
        mov al, IM_NADA
        mov ecx, [im_sn]
        cmp ecx, [im_st]            ; se ve si son más los que se pintan
        jbe .x
        mov eax, [im_s1]
        test eax, eax
        jns .x
        mov edx, [im_rec+ecx*4]     ; entre cuántos son (16 a lo más), sin dividir
        mov ecx, [im_s+8]
        imul ecx, edx
        shr ecx, 16
        mov ebx, [im_s+4]
        imul ebx, edx
        shr ebx, 16
        mov eax, [im_s]
        imul eax, edx
        shr eax, 16
        push esi
        push edi
        mov esi, edi
        mov edi, ebp
        call im_color
        pop edi
        pop esi
        cmp al, IM_NADA
        jne .x
        dec al
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; 65.536 entre n, hacia arriba (de 1 a 16)
im_rec      dd 0, 65536, 32768, 21846, 16384, 13108, 10923, 9363, 8192, 7282, 6554, 5958, 5462, 5042, 4682, 4370, 4096

; (interna) de cada color de la paleta de las imágenes, su rojo, verde y azul:
; el cubo de 6x6x6 (16-231, de 51 en 51) y los grises (232-255, de 11 en 11)
vb_im_tabla:
        pushad
        mov edi, im_tab_rgb
        xor eax, eax
        mov ecx, 16
        rep stosd
        xor ecx, ecx
.cubo:
        mov eax, ecx                ; azul
        xor edx, edx
        mov ebx, 6
        div ebx
        imul esi, edx, 51
        shl esi, 16
        xor edx, edx                ; verde
        div ebx
        imul edx, edx, 51
        shl edx, 8
        or esi, edx
        imul eax, eax, 51           ; rojo
        or eax, esi
        stosd
        inc ecx
        cmp ecx, 216
        jb .cubo
        xor ecx, ecx
.gris:
        imul eax, ecx, 11
        imul eax, eax, 0x010101
        stosd
        inc ecx
        cmp ecx, 24
        jb .gris
        mov byte [im_tab_hecha], 1
        popad
        ret

; ESI=un ITT_ILEGIBLE: una banda gris donde iría el texto
vb_ilegible:
        pushad
        test byte [esi+IT_BAND], IB_GIRO
        jnz .x
        mov eax, [esi+IT_AL]
        call a_px
        mov edx, eax
        lea edx, [edx+edx*2]
        sar edx, 2                  ; tres cuartos del tamaño
        cmp edx, 2
        jge .h
        mov edx, 2
.h:
        mov eax, [esi+IT_Y]
        call a_px_y
        mov ebx, eax
        sub ebx, edx
        mov eax, [esi+IT_AN]
        call a_px
        mov ecx, eax
        cmp ecx, 2
        jge .w
        mov ecx, 2
.w:
        mov eax, [esi+IT_X]
        call a_px_x
        mov byte [vb_color], C_PIEDRA
        call vb_rect
        mov byte [vb_color], C_ARENA_OSC    ; rayado, para que no parezca un recuadro
        push edx
        mov edx, 1
        call vb_rect
        pop edx
        add ebx, edx
        dec ebx
        mov edx, 1
        call vb_rect
.x:
        popad
        ret

; un texto con la letra de Hispa Ventanas (8x16): EAX=x EBX=y ESI=texto
vb_txt8:
        pushad
        mov edi, eax
.c:
        movzx eax, byte [esi]
        inc esi
        test eax, eax
        jz .x
        shl eax, 4
        add eax, [hv_api + HV_fuente8x16 * 4]
        mov ebp, eax                ; EBP=las 16 filas de la letra
        xor edx, edx
.f:
        mov al, [ebp+edx]
        xor ecx, ecx
.b:
        test al, 0x80
        jz .nb
        pushad
        lea eax, [edi+ecx]
        add ebx, edx
        mov ecx, 1
        mov edx, 1
        call vb_rect
        popad
.nb:
        shl al, 1
        inc ecx
        cmp ecx, 8
        jb .b
        inc edx
        cmp edx, 16
        jb .f
        add edi, 8
        jmp .c
.x:
        popad
        ret

; -----------------------------------------------------------------------------
;  Una figura rellena (ESI=un ITT_POLI): línea a línea de la pantalla, se
;  buscan los cortes con sus lados y se rellena de corte en corte (regla
;  par-impar). Los vértices, antes, a puntos de pantalla con 16 bits de decimales
; -----------------------------------------------------------------------------
vb_poli:
        pushad
        movzx ecx, word [esi+IT_N]  ; ECX=trozos
        mov esi, [esi+IT_TXT]
        mov edi, pv_tab
        mov dword [pv_n], 0
        mov dword [pv_y0], 0x7FFFFFFF
        mov dword [pv_y1], 0x80000000
        mov [pv_trozos], ecx
.tr:
        test ecx, ecx
        jz .hechos
        lodsd                       ; cuántos vértices
        mov edx, eax
        add eax, [pv_n]
        cmp eax, PV_MAX - 2
        ja .x                       ; (demasiados: no se pinta)
        mov [pv_n], eax
        mov [edi], edx              ; en pv_tab: cuántos, y sus x, y
        add edi, 4
.v:
        lodsd
        mov ebx, [ox]
        call .a16
        stosd
        lodsd
        mov ebx, [oy]
        call .a16
        stosd
        cmp eax, [pv_y0]
        jge .v1
        mov [pv_y0], eax
.v1:
        cmp eax, [pv_y1]
        jle .v2
        mov [pv_y1], eax
.v2:
        dec edx
        jnz .v
        dec ecx
        jmp .tr
.hechos:
        mov ebp, [pv_y0]            ; EBP=la línea de pantalla
        sar ebp, 16
        test ebp, ebp
        jns .y0
        xor ebp, ebp
.y0:
        mov eax, [pv_y1]
        sar eax, 16
        cmp eax, [vh]
        jl .y1
        mov eax, [vh]
        dec eax
.y1:
        mov [pv_fin], eax
.linea:
        cmp ebp, [pv_fin]
        jg .x
        mov eax, ebp                ; el centro de la línea
        shl eax, 16
        add eax, 0x8000
        mov [pv_yc], eax
        mov dword [pv_nx], 0
        mov esi, pv_tab
        mov ecx, [pv_trozos]
.t:
        test ecx, ecx
        jz .cortes
        push ecx
        mov ecx, [esi]              ; ECX=vértices del trozo
        add esi, 4
        mov edi, esi                ; EDI=el primero (el último lado vuelve a él)
        mov edx, ecx
.lado:
        mov eax, esi                ; de este vértice (ESI) al siguiente (EBX)
        lea ebx, [esi+8]
        cmp edx, 1
        jne .l1
        mov ebx, edi
.l1:
        push edx
        mov eax, [esi+4]            ; ya, yb
        mov edx, [ebx+4]
        cmp eax, [pv_yc]
        setle cl
        cmp edx, [pv_yc]
        setle ch
        cmp cl, ch
        je .l_no                    ; los dos al mismo lado: no la corta
        ; x = xa + (yc - ya) (xb - xa) / (yb - ya)
        sub edx, eax                ; yb - ya (no es 0)
        push edx
        mov eax, [pv_yc]
        sub eax, [esi+4]
        mov edx, [ebx]
        sub edx, [esi]
        imul edx
        pop ebx
        idiv ebx                    ; (no desborda: |yc - ya| <= |yb - ya|)
        add eax, [esi]
        mov ebx, [pv_nx]
        cmp ebx, 62
        jae .l_no
        ; a su sitio, en orden
.ins:
        test ebx, ebx
        jz .ins1
        cmp [pv_x+ebx*4-4], eax
        jle .ins1
        mov edx, [pv_x+ebx*4-4]
        mov [pv_x+ebx*4], edx
        dec ebx
        jmp .ins
.ins1:
        mov [pv_x+ebx*4], eax
        inc dword [pv_nx]
.l_no:
        pop edx
        add esi, 8
        dec edx
        jnz .lado
        pop ecx
        dec ecx
        jmp .t
.cortes:
        xor esi, esi
.c:
        lea eax, [esi+1]
        cmp eax, [pv_nx]
        jae .c_fin
        mov eax, [pv_x+esi*4]
        add eax, 0x7FFF
        sar eax, 16
        mov ecx, [pv_x+esi*4+4]
        add ecx, 0x7FFF
        sar ecx, 16
        sub ecx, eax
        jg .c1
        mov ecx, 1                  ; (más fina que un punto: que se vea)
.c1:
        mov ebx, ebp
        mov edx, 1
        call vb_rect
        add esi, 2
        jmp .c
.c_fin:
        inc ebp
        jmp .linea
.x:
        popad
        ret
; (interna) EAX=medida de la página, EBX=el sitio de la hoja -> EAX=en puntos
; de pantalla con 16 bits de decimales (sin pasar de +-16.000)
.a16:
        push edx
        imul dword [zoom]
        cmp edx, 16000
        jl .a1
        mov edx, 16000
        xor eax, eax
.a1:
        cmp edx, -16000
        jg .a2
        mov edx, -16000
        xor eax, eax
.a2:
        shrd eax, edx, 16
        shl ebx, 16
        add eax, ebx
        pop edx
        ret

; -----------------------------------------------------------------------------
;  Un trozo de texto (ESI=un ITT_TEXTO, EBX=su número en la lista)
; -----------------------------------------------------------------------------
vb_texto:
        pushad
        mov [t_it], esi
        ; ¿lleva parte de lo encontrado? De qué letra a qué letra
        mov dword [t_ha], -1
        mov dword [t_hb], -1
        xor ecx, ecx
.hl:
        cmp ecx, [hl_n]
        jae .hl_fin
        cmp [hl_it+ecx*4], ebx
        je .hl_es
        inc ecx
        jmp .hl
.hl_es:
        mov eax, [hl_a+ecx*4]
        mov [t_ha], eax
        mov eax, [hl_b+ecx*4]
        mov [t_hb], eax
.hl_fin:
        mov eax, [esi+IT_X]
        call a_px_x
        mov [t_x0], eax
        mov eax, [esi+IT_Y]
        call a_px_y
        mov [t_y0], eax
        movzx eax, byte [esi+IT_BAND]
        mov [t_band], eax
        shr eax, 4
        and eax, 3
        mov [t_giro], eax
        movzx eax, byte [esi+IT_LETRA]
        mov [t_tipo], eax
        mov al, [esi+IT_COLOR]
        mov [t_color], al
        mov eax, [esi+IT_AN]
        call a_px
        mov [t_wt], eax
        mov eax, [esi+IT_AL]
        call a_px
        mov [t_c], eax              ; el alto de casilla que le toca
        ; si no está girado y cae fuera de la zona, nada
        cmp dword [t_giro], 0
        jne .dentro
        mov ebx, [t_y0]
        sub ebx, eax
        cmp ebx, [vh]
        jge .x
        mov ebx, [t_y0]
        add ebx, eax
        js .x
        mov ebx, [t_x0]
        cmp ebx, [vw]
        jge .x
.dentro:
        ; ---- qué tamaño de los dibujados (casillas de 12, 16, 24, 32, 48 y 64) ----
        ; El mayor que no pase de vez y media la casilla que le toca: una letra
        ; algo grande y estrechada se lee mejor que una pequeña y espaciada.
        mov eax, [t_c]
        cmp eax, 3
        jl .raya
        mov dword [t_num], 1
        mov dword [t_den], 1
        cmp eax, 76
        jg .grande
        lea ecx, [eax+eax*2]        ; 3 x casilla (se compara con 2 x tamaño)
        mov edx, 5
        cmp ecx, 64 * 2
        jge .e_ok
        mov edx, 4
        cmp ecx, 48 * 2
        jge .e_ok
        mov edx, 3
        cmp ecx, 32 * 2
        jge .e_ok
        mov edx, 2
        cmp ecx, 24 * 2
        jge .e_ok
        mov edx, 0
        cmp ecx, 16 * 2
        jge .e_ok
        mov edx, 1
        cmp ecx, 12 * 2
        jl .peq
.e_ok:
        mov [t_tam], edx
.prueba:
        call .ancho_nat             ; EBX=lo que ocuparía con nuestras letras
        jz .x
        mov eax, [t_wt]
        test eax, eax
        jle .sin_wt
        ; nuestras letras no miden lo que las del PDF. Si el trozo sale más
        ; ancho que su sitio, las letras se estrechan (hasta un 30 %); si ni
        ; así cabe, un tamaño menos; y si ya es el menor, encogido lo justo
        lea edx, [eax+eax*4]
        shl edx, 1                  ; sitio x 10
        lea ecx, [ebx*8]
        sub ecx, ebx                ; nuestro ancho x 7
        cmp ecx, edx
        jbe .mapa
        mov edx, [t_tam]
        cmp edx, 1
        je .encoge
        movzx edx, byte [tam_menor+edx]
        jmp .e_ok
.encoge:
        mov eax, [t_wt]             ; casilla de 12 x 1,25 x sitio / ancho
        imul eax, 15
        xor edx, edx
        div ebx
        cmp eax, 3
        jge .en1
        mov eax, 3
.en1:
        cmp eax, 11
        jle .en2
        mov eax, 11
.en2:
        mov [t_num], eax
        mov dword [t_den], 12
        jmp .mapa
.peq:
        ; por debajo de 8: el tipo de 12, encogido (se ve la mancha del texto)
        mov dword [t_tam], 1
        mov [t_num], eax
        mov dword [t_den], 12
        call .ancho_nat
        jz .x
        mov eax, [t_wt]
        test eax, eax
        jle .sin_wt
        imul eax, 15
        xor edx, edx
        div ebx
        cmp eax, [t_num]
        jae .mapa
        cmp eax, 3
        jge .pq1
        mov eax, 3
.pq1:
        mov [t_num], eax
        jmp .mapa
.grande:
        ; por encima de 76: el de 64, estirado
        mov dword [t_tam], 5
        mov [t_num], eax
        mov dword [t_den], 64
        call .ancho_nat
        jz .x
        cmp dword [t_wt], 0
        jg .mapa
.sin_wt:
        mov eax, ebx                ; el PDF no dice el ancho: el nuestro
        mul dword [t_num]
        div dword [t_den]
        test eax, eax
        jnz .sw1
        inc eax
.sw1:
        mov [t_wt], eax
.mapa:
        ; de puntos del tipo a puntos de pantalla: a lo alto (t_mapa), según lo
        ; encogido; a lo ancho (t_mapx), además, lo que haya que estrechar
        ; para que el trozo quepa en su sitio
        mov eax, [t_wn]
        mul dword [t_num]
        div dword [t_den]
        mov ebx, eax                ; EBX=el ancho con nuestras letras
        xor ecx, ecx
.mp:
        mov eax, ecx
        mul dword [t_num]
        div dword [t_den]
        mov [t_mapa+ecx*4], eax
        cmp ebx, [t_wt]
        jbe .mp1
        mov eax, ecx
        mul dword [t_wt]
        div dword [t_wn]
.mp1:
        mov [t_mapx+ecx*4], eax
        inc ecx
        cmp ecx, MAPA_N
        jb .mp
        mov edx, [t_tam]
        call tl_alto_de
        mov eax, [t_mapa+eax*4]
        mov [t_h], eax              ; el alto de la casilla en la pantalla
        lea eax, [eax+eax*2]
        sar eax, 2
        neg eax
        mov [t_vtop], eax           ; la línea base, a tres cuartos
        mov eax, [t_h]              ; la negrita fingida: un trazo más ancho. Con
        shr eax, 4                  ; la letra pequeña y estrechada no: se empasta
        jnz .g1
        inc eax
.g1:
        cmp ebx, [t_wt]
        jbe .g2
        cmp dword [t_h], 24
        jae .g2
        xor eax, eax
.g2:
        mov [t_gordo], eax
        ; ---- letra a letra ----
        mov dword [t_cum], 0
        mov dword [t_i], 0
.car:
        mov esi, [t_it]
        movzx ecx, word [esi+IT_N]
        mov eax, [t_i]
        cmp eax, ecx
        jae .x
        mov esi, [esi+IT_TXT]
        movzx ecx, byte [esi+eax]
        mov [t_car], ecx
        mov eax, [t_tipo]
        mov edx, [t_tam]
        call tl_letra               ; ESI=su dibujo
        ; dónde empieza y dónde acaba: repartidas en el ancho del trozo
        mov eax, [t_cum]
        mul dword [t_wt]
        div dword [t_wn]
        mov [t_u0], eax
        mov eax, [t_cum]
        add eax, [tl_g_an]
        mov [t_cum], eax
        mul dword [t_wt]
        div dword [t_wn]
        mov [t_u1], eax
        cmp dword [t_giro], 0
        jne .ve
        mov eax, [t_x0]             ; a la derecha de la zona: se acabó
        add eax, [t_u0]
        cmp eax, [vw]
        jge .x
        mov eax, [t_x0]             ; a la izquierda: esta no
        add eax, [t_u1]
        add eax, [t_h]
        js .sig
.ve:
        mov al, [t_color]
        mov [vb_color], al
        mov eax, [t_i]              ; ¿de lo encontrado? En petróleo, letra clara
        cmp eax, [t_ha]
        jb .pinta
        cmp eax, [t_hb]
        jae .pinta
        mov byte [vb_color], C_PETROLEO
        mov eax, [t_u0]
        mov ebx, [t_vtop]
        mov ecx, [t_u1]
        sub ecx, eax
        mov edx, [t_h]
        call vb_uv
        mov byte [vb_color], C_CREMA
.pinta:
        cmp dword [t_car], ' '
        je .sig
        xor ecx, ecx                ; ECX=la fila del dibujo
.fila:
        cmp ecx, [tl_g_filas]
        jae .sig
        mov eax, ecx
        imul eax, [tl_g_esc]
        cmp eax, MAPA_N - 6
        jae .sig
        mov ebx, [t_mapa+eax*4]     ; de dónde a dónde cae la fila
        add eax, [tl_g_esc]
        mov edx, [t_mapa+eax*4]
        sub edx, ebx
        jg .f1
        mov edx, 1
.f1:
        add ebx, [t_vtop]
        mov [t_v], ebx
        mov [t_alto], edx
        xor edi, edi                ; la cursiva: lo de arriba, a la derecha
        test byte [t_band], IB_CURSIVA
        jz .f2
        mov edi, ebx
        neg edi
        sar edi, 2
.f2:
        add edi, [t_u0]
        mov [t_ufila], edi
        xor ebp, ebp                ; EBP=la columna del dibujo
        mov edx, [tl_g_bpr]
.oct:
        mov al, [esi]
        inc esi
        mov ah, 8
.bit:
        test al, 0x80
        jz .nb
        push eax
        push ecx
        push edx
        mov eax, ebp
        imul eax, [tl_g_esc]
        cmp eax, MAPA_N - 6
        jae .nb1
        mov ebx, [t_mapx+eax*4]
        add eax, [tl_g_esc]
        mov ecx, [t_mapx+eax*4]
        sub ecx, ebx
        jg .b1
        mov ecx, 1
.b1:
        test byte [t_band], IB_NEGRA
        jz .b2
        add ecx, [t_gordo]
.b2:
        mov eax, ebx
        add eax, [t_ufila]
        mov ebx, [t_v]
        mov edx, [t_alto]
        call vb_uv
.nb1:
        pop edx
        pop ecx
        pop eax
.nb:
        shl al, 1
        inc ebp
        dec ah
        jnz .bit
        dec edx
        jnz .oct
        inc ecx
        jmp .fila
.sig:
        inc dword [t_i]
        jmp .car
.raya:
        ; tan pequeño que no se distinguen letras: una raya gris en su sitio
        mov byte [vb_color], C_ARENA_OSC
        xor eax, eax
        mov ebx, -1
        mov ecx, [t_wt]
        cmp ecx, 1
        jge .r1
        mov ecx, 1
.r1:
        mov edx, 1
        call vb_uv
.x:
        popad
        ret

; (interna de vb_texto) lo que ocupa el trozo con nuestras letras, en el
; tamaño [t_tam] y sin encoger -> EBX y [t_wn]; ZF=1 si nada
vb_texto.ancho_nat:
        mov esi, [t_it]
        movzx ecx, word [esi+IT_N]
        mov esi, [esi+IT_TXT]
        xor ebx, ebx
        test ecx, ecx
        jz .a_fin
.a_car:
        push ecx
        movzx ecx, byte [esi]
        inc esi
        mov eax, [t_tipo]
        mov edx, [t_tam]
        call tl_ancho
        add ebx, eax
        pop ecx
        dec ecx
        jnz .a_car
.a_fin:
        mov [t_wn], ebx
        test ebx, ebx
        ret
; de cada tamaño, el siguiente más pequeño (de menor a mayor: 1, 0, 2, 3, 4, 5)
tam_menor   db 1, 1, 0, 2, 3, 4

; un rectángulo del texto: EAX=u (a lo largo de la línea, desde donde empieza
; el trozo), EBX=v (hacia abajo desde la línea base), ECX=largo, EDX=alto;
; según hacia dónde vaya el texto ([t_giro])
vb_uv:
        pushad
        mov esi, [t_giro]
        cmp esi, 1
        jb .normal
        je .abajo
        cmp esi, 2
        je .reves
        ; hacia arriba: x = x0 + v, y = y0 - u - largo
        add ebx, [t_x0]
        neg eax
        sub eax, ecx
        add eax, [t_y0]
        xchg eax, ebx
        xchg ecx, edx
        jmp .pinta
.normal:
        add eax, [t_x0]
        add ebx, [t_y0]
        jmp .pinta
.abajo:
        ; hacia abajo: x = x0 - v - alto, y = y0 + u
        neg ebx
        sub ebx, edx
        add ebx, [t_x0]
        add eax, [t_y0]
        xchg eax, ebx
        xchg ecx, edx
        jmp .pinta
.reves:
        ; cabeza abajo: x = x0 - u - largo, y = y0 - v - alto
        neg eax
        sub eax, ecx
        add eax, [t_x0]
        neg ebx
        sub ebx, edx
        add ebx, [t_y0]
.pinta:
        call vb_rect
        popad
        ret

%include "desinfla.inc"
%include "jpeg.inc"
%include "pdf.inc"
%include "letras.inc"
%include "nombres.inc"
%include "contenido.inc"
%include "texto.inc"
%include "imagen.inc"
%include "tipos.inc"
%include "barra.inc"
%include "contexto.inc"
%include "hvplib.inc"

        section .bss
ven         resd 1
ctx_donde   resb 1                  ; 1: el botón secundario se apretó en la zona de lectura
            alignb 4
barra_v     resd 1
barra_h     resd 1
param       resd 1
cli_an      resd 1
cli_al      resd 1
vy          resd 1                  ; la zona de lectura: dónde empieza y lo que mide
vw          resd 1
vh          resd 1
pag         resd 1                  ; la página que se ve (desde 0)
pag_cargada resd 1                  ; la que hay compuesta en la lista (-1: ninguna)
tx_pag      resd 1                  ; la del texto que hay en tx_buf
zoom        resd 1                  ; puntos de pantalla por punto de la página (16.16)
zmodo       resd 1                  ; 0 fija, 1 ancho de página, 2 página entera
zpct        resd 1
pw          resd 1                  ; la hoja, en puntos de pantalla
ph          resd 1
sx          resd 1                  ; lo desplazado
sy          resd 1
sx_max      resd 1
sy_max      resd 1
ox          resd 1                  ; dónde cae la hoja en la zona
oy          resd 1
av_txt      resd 1                  ; el aviso de la página (0: ninguno)
bloque      resd 1                  ; el mango de la memoria del documento
arch_tam    resd 1
alm_tam     resd 1
nuc_esp     resd 1
bus_pag     resd 1                  ; dónde se encontró la última vez
bus_pos     resd 1
hl_n        resd 1                  ; lo encontrado: trozos marcados
hl_it       resd HL_MAX
hl_a        resd HL_MAX
hl_b        resd HL_MAX
vb_gros     resd 1
vbuf_p      resd 1                  ; el mapa de puntos de la zona: del tamaño de la pantalla
vbuf_h      resd 1
vb_an       resd 1
vb_al       resd 1
puente      resd 1                  ; el cuadro de texto escondido del portapapeles
arr_x       resd 1                  ; arrastrando la hoja: dónde estaba el ratón
arr_y       resd 1
arrastra    resb 1
            alignb 4
vc_x0       resd 1                  ; el recorte de lo que se pinta (en la zona)
vc_y0       resd 1
vc_x1       resd 1
vc_y1       resd 1
im_px       resd 1
im_py       resd 1
im_pw       resd 1
im_ph       resd 1
im_pb       resd 1
ln_x        resd 1
ln_y        resd 1
ln_x1       resd 1
ln_y1       resd 1
ln_dx       resd 1
ln_dy       resd 1
ln_sx       resd 1
ln_sy       resd 1
pv_n        resd 1
pv_trozos   resd 1
pv_y0       resd 1
pv_y1       resd 1
pv_yc       resd 1
pv_fin      resd 1
pv_nx       resd 1
pv_x        resd 64
t_it        resd 1
t_ha        resd 1
t_hb        resd 1
t_x0        resd 1
t_y0        resd 1
t_band      resd 1
t_giro      resd 1
t_tipo      resd 1
t_tam       resd 1
t_wt        resd 1
t_wn        resd 1
t_c         resd 1
t_num       resd 1
t_den       resd 1
t_h         resd 1
t_vtop      resd 1
t_gordo     resd 1
t_cum       resd 1
t_i         resd 1
t_car       resd 1
t_u0        resd 1
t_u1        resd 1
t_v         resd 1
t_alto      resd 1
t_ufila     resd 1
t_mapa      resd MAPA_N + 2
t_mapx      resd MAPA_N + 2
hay_doc     resb 1
hay_bloque  resb 1
sucio       resb 1
imp_todo    resb 1
vb_color    resb 1
t_color     resb 1
            alignb 4
rl          resb RL_TAM
titulo      resb 112
nombre      resb 96
nom_tmp     resb 96
busca_txt   resb 44
num_txt     resb 12
linea       resb 200
aviso_buf   resb 160
dato_buf    resb 64
info_buf    resb 900
            alignb 4
pv_tab      resd PV_MAX * 3 + 64
; Lo que se escribe por cada punto al pintar una miniatura va aquí, lejos del
; código: las primeras variables comparten página con el final del programa, y
; escribir en una página que tiene código es lentísimo en QEMU (y en un
; Pentium moderno): cada escritura le hace comprobar si se ha cambiado código.
; Con estas arriba, repintar una página escaneada tardaba 2,5 s en vez de 0,1
im_c0       resd 1                  ; el cuadro de la miniatura que cae en un punto
im_cn       resd 1
im_r0       resd 1
im_rn       resd 1
im_sc       resd 1
im_sr       resd 1
im_s        resd 3
im_sn       resd 1
im_st       resd 1
im_s1       resd 1
im_spaso    resd 1
im_fila_p   resd 1
im_vfila    resd 1
im_cc       resd 1
im_ca       resd 1
im_tab_rgb  resd 256                ; la paleta de las imágenes, en rojo, verde y azul
im_tab_hecha resb 1
            alignb 4
            DESINFLA_BSS
            JPEG_BSS
            PDF_BSS
            LETRAS_BSS
            CONTENIDO_BSS
            TEXTO_BSS
            IMAGEN_BSS
            TIPOS_BSS
            BARRA_BSS
            alignb 4
nuc_pila    resb NUC_PILA
copia_buf   resb COPIA_MAX + 4
; imprimir como se ve: una franja de la página, ya en el papel; las columnas
; de una tira (los ocho puntos de cada una, uno por bit); la claridad de cada
; color de la paleta; y lo que se guarda mientras
ig_buf      resb IG_AN * IG_FR
ig_cols     resb IG_AN
ig_lum      resb 256
ig_guarda   resd 16
ig_mask     resb 1
ig_hay      resb 1
HVP_FIN
