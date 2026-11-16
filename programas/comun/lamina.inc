; =============================================================================
;  lamina.inc - una lámina con objetos: el motor que comparten Publicaciones
;  (panfletos, carteles, tarjetas) y Presentaciones (diapositivas)
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un documento son una o varias láminas (páginas, diapositivas) del mismo
;  tamaño y, sobre ellas, objetos colocados libremente: cajas de texto,
;  rectángulos, elipses, líneas e imágenes. Aquí está el modelo (la lista de
;  objetos y sus datos), el dibujo de una lámina en un mapa de puntos, saber
;  qué objeto hay en un punto, la composición del texto de una caja (para
;  poner el cursor) y guardar y abrir el archivo. No sabe nada de ventanas,
;  menús ni ratones: eso es de cada programa.
;
;  LAS MEDIDAS van en «puntos de lámina»: 96 por pulgada (un milímetro son
;  3,78), que es lo que mide un punto de la pantalla al 100 %. Así las letras
;  de tipos.inc (12 puntos de imprenta = casilla de 16) salen punto por punto
;  a tamaño real, y un A4 mide 794 x 1123. La lámina se dibuja siempre al
;  100 % en [lam_buf] (un octeto por punto, colores de la paleta de Hispa
;  Ventanas) y lam_reduce la lleva al tamaño que se quiera enseñar.
;
;  UN OBJETO es una ficha de LO_TAM (40) octetos; el orden de la tabla es el
;  de dibujo: el primero queda al fondo y el último, al frente.
;      LO_TIPO   db  LT_TEXTO, LT_RECT, LT_ELIPSE, LT_LINEA o LT_IMAGEN
;      LO_PAG    db  en qué lámina está (la primera es la 0)
;      LO_ESTILO db  de la letra: LE_NEG, LE_CUR, LE_SUB (se suman)
;      LO_ALIN   db  del texto: 0 izquierda, 1 centro, 2 derecha
;      LO_X, LO_Y    dd  la esquina de arriba a la izquierda (con signo)
;      LO_AN, LO_AL  dd  ancho y alto. En una línea, lo que hay del primer
;                        extremo (LO_X, LO_Y) al segundo: pueden ser negativos
;      LO_TINTA  db  color de la letra
;      LO_FONDO  db  color del relleno (LAM_NADA: sin relleno)
;      LO_BORDE  db  color del borde, o de la línea (LAM_NADA: sin borde)
;      LO_GROSOR db  grosor del borde o de la línea, en puntos de lámina
;      LO_LETRA  db  tipo de letra (los de tipos.inc; 0 es «Hispa»)
;      LO_CUERPO db  tamaño de la letra (los de tipos.inc; 0 son 12 puntos)
;      LO_IMG_AN, LO_IMG_AL  dw  una imagen: los puntos que tiene de verdad
;      LO_PAPEL  db  qué hace en la lámina (0: nada especial; lo demás lo
;                    decide cada programa: título, cuerpo, notas...)
;      LO_BAND   db  LB_VINETAS: el texto es una lista, cada párrafo con su
;                    viñeta (los tabuladores de delante de un párrafo, hasta
;                    dos, son su nivel: más sangría y letra más pequeña);
;                    LB_OCULTO: no se dibuja ni se elige (lo guarda el
;                    programa para lo suyo: las notas de una diapositiva);
;                    LB_HUECO: una caja en la que no se escribe (guarda el
;                    sitio de otra cosa: una imagen)
;      LO_DAT    dd  dónde empiezan sus datos en lam_dat
;      LO_LARGO  dd  y cuántos octetos son
;  Los datos son el texto de una caja (página 437; el 10 acaba el párrafo) o
;  los puntos de una imagen (un octeto por punto, ya en la paleta de Hispa
;  Ventanas, por filas). Están todos seguidos en lam_dat; lam_dat_cambia
;  mete o quita octetos y corre los de los demás objetos.
;
;  EL DOCUMENTO ENTERO son dos trozos de memoria: lam_doc (LAM_DOC_TAM
;  octetos: las medidas, las cuentas, [lam_extra] y la tabla de objetos) y los primeros
;  [lam_dat_n] octetos de lam_dat. Copiar los dos es sacar una instantánea
;  (para deshacer), y volver a ponerlos, recuperarla.
;
;  EL ARCHIVO (los .PUB de Publicaciones y los .PRE de Presentaciones):
;      0   8 octetos  la firma, que pone cada programa («PUBLICA», 26; «PRESENT», 26)
;      8   dw ancho y dw alto de la lámina, en puntos de lámina
;      12  dw margen (la guía que se enseña)     14  dw cuántas láminas
;      16  dw cuántos objetos                    18  dw octetos de cada ficha
;      20  dd octetos de datos
;      24  las fichas, una detrás de otra (si en el archivo son más largas
;          que LO_TAM, lo que sobra de cada una se salta: así un programa de
;          hoy abre un archivo de mañana), y detrás, los datos
;      y al final, solo si no es 0, los 4 octetos de [lam_extra] (lo que
;      quiera el programa: en Presentaciones, el tema)
;  Al abrir se repasa todo: lo que no se entiende se quita o se arregla, y un
;  archivo que no cuadra no se abre.
;
;  Se incluye en el código del programa, detrás de tipos.inc (escribe con sus
;  letras: tl_letra, tl_ancho, tl_alto_de), y en su «section .bss» se pone la
;  macro LAMINA_BSS. Usa la API de Hispa Ventanas solo para los archivos y la
;  paleta. Las rutinas conservan los registros que no devuelven nada.
;
;    lam_nueva      EAX=ancho EBX=alto ECX=margen: un documento vacío, de una
;                   lámina
;    lam_ptr        EAX=objeto -> ESI=su ficha
;    lam_obj_nuevo  AL=tipo CL=lámina -> EAX=objeto (el de más al frente)
;                   ESI=su ficha, a cero salvo el tipo, la lámina, sin relleno
;                   ni borde y grosor 1; CF=1 si no caben más
;    lam_obj_quita  EAX=objeto: lo quita, con sus datos
;    lam_obj_dup    EAX=objeto -> EAX=una copia, al frente; CF=1 si no cabe
;    lam_obj_orden  EAX=objeto EDX=1 al frente / 0 al fondo -> EAX=dónde queda
;    lam_dat_cambia EAX=objeto EBX=sitio ECX=octetos que se quitan ESI=los que
;                   se ponen EDX=cuántos: cambia sus datos (escribir y borrar
;                   en una caja, ponerle el texto o la imagen); CF=1 si no
;                   caben (y no cambia nada). ESI no puede estar en lam_dat,
;                   salvo que el objeto sea el último que se creó
;    lam_pag_nueva  ECX=lámina: abre una vacía detrás de esa; CF=1 si no caben
;    lam_pag_quita  ECX=lámina: la quita con lo que tiene (si es la única, la
;                   deja vacía)
;    lam_pag_mueve  ECX=lámina EDX=a qué sitio va: cambia el orden
;    lam_pag_dup    ECX=lámina: una copia suya detrás; CF=1 si no cabe (puede
;                   quedar a medias: quien llama vuelve a su instantánea)
;    lam_pinta      ECX=lámina: la dibuja en lam_buf ([lam_an] x [lam_al]).
;                   Si [lam_fondo_fn] no es 0, antes de los objetos llama a
;                   esa rutina del programa (ECX=lámina), que pinta el fondo
;                   con lam_caja (EAX=x EBX=y ECX=ancho EDX=alto, del color
;                   [lam_color]) y lam_tramo (EAX=x ECX=hasta dónde EBX=fila)
;    lam_destino    EDI=búfer ECX=ancho EDX=alto: desde ahora lam_pinta_obj,
;                   lam_caja y lam_tramo dibujan ahí (EDI=0: en lam_buf otra
;                   vez; lam_pinta lo pone siempre así)
;    lam_pinta_obj  ESI=ficha (puede no estar en la tabla: una copia con otro
;                   sitio y otro tamaño): dibuja ese objeto en el destino
;    lam_reduce     EDI=búfer ECX=ancho EDX=alto EBX=1 suavizando / 0 no:
;                   lleva lam_buf a ese tamaño (suavizando, los puntos que
;                   mezclan colores salen del cubo de 6x6x6 de la paleta)
;    lam_en_punto   EAX=x EBX=y ECX=lámina EDX=holgura -> EAX=el objeto de
;                   más al frente que hay ahí (-1: ninguno). De un rectángulo
;                   o una elipse sin relleno solo cuenta el borde
;    lam_compone    ESI=ficha de una caja de texto: parte su texto en líneas
;                   ([lam_l_n] líneas; lam_l_ini, dónde empieza cada una, y
;                   lam_l_y, a qué altura). Lo necesitan las dos siguientes
;    lam_sitio_xy   ESI=ficha EAX=sitio en el texto -> EAX=x EBX=y (desde la
;                   esquina de la caja) ECX=alto de la línea: dónde va el cursor
;    lam_xy_sitio   ESI=ficha EAX=x EBX=y (desde la esquina) -> EAX=el sitio
;                   del texto más cercano
;    [lam_guia_fn]  si no es 0, la rutina del programa que da el texto de
;                   ejemplo de una caja vacía (EAX=su LO_PAPEL -> ESI=texto
;                   acabado en 0, o 0); se pinta del color [lam_guia_color].
;                   Para imprimir o presentar se pone a 0. No se pinta en
;                   la caja cuya ficha esté en [lam_guia_salta] (la que se
;                   está escribiendo)
;    [lam_parr_max] 0: todo; n: de las listas solo se pintan los n-1 primeros
;                   puntos (la aparición por pasos de una presentación)
;    lam_bloque     EBX=bloque (tres dd: puntero, lo que cabe, mango) ECX=octetos
;                   AL=1 para conservar lo que tiene: que quepan esos; CF=1 si
;                   no hay memoria. lam_bloque_suelta (EBX) lo devuelve. Así se
;                   piden los búferes grandes solo cuando hacen falta y de su
;                   medida: [lam_buf] (la lámina) y [lam_dat] (los datos) son
;                   punteros; si no hay memoria para la lámina, lam_pinta deja
;                   [lam_sin_mem] a 1 y no dibuja
;    lam_suelta     al acabar el programa: devuelve la memoria del motor
;    lam_guarda     ESI=nombre EDI=firma (8 octetos) -> CF=1 si no se puede
;    lam_carga      ESI=nombre EDI=firma -> CF=1 si no se puede: EAX=0 no se
;                   abre, 1 no es un archivo de estos. OJO: si falla a medias
;                   el documento que había se pierde; quien llama saca antes
;                   una instantánea
;    lam_imagen_bmp ESI=nombre CL=lámina -> EAX=objeto nuevo (una imagen a su
;                   tamaño, en la esquina) ESI=su ficha; CF=1 si no se puede:
;                   EAX=0 no se abre, 1 no es un BMP de los que se admiten
;                   (1, 4, 8 o 24 bits, sin comprimir), 2 no cabe
; =============================================================================
LAM_MAX_OBJ equ 200                 ; objetos de un documento
LAM_MAX_PAG equ 32                  ; láminas
LAM_MAX_LADO equ 1124               ; lo más que mide un lado (un A4: 1123)
LAM_BUF_TAM equ 896 * 1024          ; puntos de lam_buf (un A4: 794 x 1123)
LAM_DAT_TAM equ 384 * 1024          ; octetos de textos e imágenes
LAM_MAX_LIN equ 400                 ; líneas de una caja de texto
LAM_SEP     equ 4                   ; del borde de una caja a su texto
LAM_NADA    equ 0xFF                ; «sin color»: sin relleno, sin borde

LT_TEXTO    equ 1
LT_RECT     equ 2
LT_ELIPSE   equ 3
LT_LINEA    equ 4
LT_IMAGEN   equ 5

LB_VINETAS  equ 1                   ; (LO_BAND) cada párrafo, con su viñeta
LB_OCULTO   equ 2                   ; ni se dibuja ni se elige con el ratón
LB_HUECO    equ 4                   ; una caja en la que no se escribe (guarda un sitio)

LE_NEG      equ 1
LE_CUR      equ 2
LE_SUB      equ 4

LO_TIPO     equ 0
LO_PAG      equ 1
LO_ESTILO   equ 2
LO_ALIN     equ 3
LO_X        equ 4
LO_Y        equ 8
LO_AN       equ 12
LO_AL       equ 16
LO_TINTA    equ 20
LO_FONDO    equ 21
LO_BORDE    equ 22
LO_GROSOR   equ 23
LO_LETRA    equ 24
LO_CUERPO   equ 25
LO_IMG_AN   equ 26
LO_IMG_AL   equ 28
LO_PAPEL    equ 30
LO_BAND     equ 31
LO_DAT      equ 32
LO_LARGO    equ 36
LO_TAM      equ 40

LAM_DOC_TAM equ 28 + LAM_MAX_OBJ * LO_TAM

%macro LAMINA_BSS 0
            alignb 4
lam_doc:                            ; ---- el documento (con lam_dat) ----
lam_an      resd 1                  ; ancho y alto de la lámina
lam_al      resd 1
lam_margen  resd 1
lam_pags    resd 1                  ; cuántas láminas
lam_n       resd 1                  ; cuántos objetos
lam_dat_n   resd 1                  ; octetos de datos
lam_extra   resd 1                  ; lo que quiera el programa (su tema...)
lam_obj     resb LAM_MAX_OBJ * LO_TAM
            ; ---- lo de trabajo ----
lam_cx0     resd 1                  ; fuera de este rectángulo no se dibuja
lam_cy0     resd 1
lam_cx1     resd 1
lam_cy1     resd 1
lam_p_pag   resd 1
lam_l_n     resd 1                  ; el texto compuesto: cuántas líneas,
lam_l_alto  resd 1                  ; su alto
lam_l_ancho resd 1                  ; y el ancho que tienen para el texto
lam_c_w     resd 1
lam_c_esp   resd 1
lam_c_ini   resd 1
lam_m_s     resd 1                  ; una línea medida: dónde empieza,
lam_m_e     resd 1                  ; dónde acaba (sin su salto de línea),
lam_m_f     resd 1                  ; dónde acaba lo que se ve
lam_m_x0    resd 1                  ; y a qué distancia de la caja empieza
lam_g_x     resd 1
lam_g_y     resd 1
lam_g_fy    resd 1
lam_g_av    resd 1
lam_g_gordo resd 1
lam_r_dw    resd 1
lam_r_dh    resd 1
lam_r_suave resd 1
lam_r_x0    resd 1
lam_r_x1    resd 1
lam_r_y0    resd 1
lam_r_y1    resd 1
lam_r_r     resd 1
lam_r_g     resd 1
lam_r_b     resd 1
lam_r_n     resd 1
lam_e_g     resd 1
lam_e_x0    resd 1
lam_e_x1    resd 1
lam_v_x     resd 1
lam_v_y     resd 1
lam_v_dx    resd 1
lam_v_dy    resd 1
lam_v_sx    resd 1
lam_v_sy    resd 1
lam_v_err   resd 1
lam_v_g     resd 1
lam_h_x     resd 1
lam_h_y     resd 1
lam_h_tol   resd 1
lam_h_pag   resd 1
lam_f_arch  resd 1
lam_f_firma resd 1
lam_f_ficha resd 1
lam_i_an    resd 1
lam_i_al    resd 1
lam_i_bpp   resd 1
lam_i_fila  resd 1
lam_i_ncol  resd 1
lam_i_obj   resd 1
lam_i_pasox resd 1
lam_i_pasoy resd 1
lam_i_rgb   resd 1
lam_i_ind   resd 1
lam_dst     resd 1                  ; dónde se dibuja (lam_buf, si no se dice otra cosa)
lam_dst_an  resd 1
lam_dst_al  resd 1
lam_fondo_fn resd 1                 ; la rutina del programa que pinta el fondo (0: blanco)
lam_guia_salta resd 1               ; la ficha de la caja en la que se escribe (sin ejemplo)
lam_guia_fn resd 1                  ; la que da el texto de ejemplo de una caja vacía (0: no hay)
lam_parr_max resd 1                 ; de las listas, solo los n-1 primeros puntos (0: todos)
lam_t_txt   resd 1                  ; el texto que se compone
lam_t_n     resd 1
lam_c_y     resd 1
lam_d_antes resd 1
lam_c_izq   resd 1
lam_c_cabe  resd 1
lam_p_parr  resd 1
lam_t_vin   resb 1
lam_t_base  resb 1
lam_c_par   resb 1
lam_c_niv   resb 1
lam_guia_color resb 1               ; el color del texto de ejemplo
lam_color   resb 1
lam_t_tipo  resb 1                  ; la letra con la que se compone y se dibuja
lam_t_tam   resb 1
lam_t_estilo resb 1
lam_i_arriba resb 1
lam_i_pag   resb 1
            alignb 4
lam_tmp     resb LO_TAM
lam_l_ini   resd LAM_MAX_LIN + 2    ; de cada línea: dónde empieza en el texto,
lam_l_y     resd LAM_MAX_LIN + 2    ; a qué altura (desde la primera),
lam_l_izq   resd LAM_MAX_LIN + 2    ; su sangría,
lam_l_tam   resb LAM_MAX_LIN + 2    ; el tamaño de su letra
lam_l_niv   resb LAM_MAX_LIN + 2    ; y su nivel (+40h si empieza párrafo, +80h con viñeta)
            alignb 4
lam_r_x     resd LAM_MAX_LADO + 2
lam_i_mapa  resb 256
lam_fila    resb 4096 + 1024        ; para leer y escribir archivos
; los dos búferes grandes no están aquí: se piden (lam_bloque) cuando hacen
; falta y de la medida que hace falta. De cada uno: el puntero, lo que cabe y
; su mango
lam_dat     resd 3                  ; los datos (crece de 16 en 16 K, hasta LAM_DAT_TAM)
lam_buf     resd 3                  ; la lámina dibujada (ancho x alto del documento)
lam_sin_mem resb 1                  ; 1: no ha habido memoria para dibujar la lámina
            alignb 4
%endmacro

; =============================================================================
;  El modelo
; =============================================================================
lam_nueva:
        mov [lam_an], eax
        mov [lam_al], ebx
        mov [lam_margen], ecx
        mov dword [lam_pags], 1
        mov dword [lam_n], 0
        mov dword [lam_dat_n], 0
        mov dword [lam_extra], 0
        ret

lam_ptr:
        imul esi, eax, LO_TAM
        add esi, lam_obj
        ret

lam_obj_nuevo:
        push ecx
        push edx
        push edi
        mov edx, eax
        mov eax, [lam_n]
        cmp eax, LAM_MAX_OBJ
        jae .no
        call lam_ptr
        push eax
        push ecx
        mov edi, esi
        xor eax, eax
        mov ecx, LO_TAM / 4
        rep stosd
        pop ecx
        pop eax
        mov [esi+LO_TIPO], dl
        mov [esi+LO_PAG], cl
        mov byte [esi+LO_FONDO], LAM_NADA
        mov byte [esi+LO_BORDE], LAM_NADA
        mov byte [esi+LO_GROSOR], 1
        mov edx, [lam_dat_n]        ; sus datos (ninguno todavía), al final
        mov [esi+LO_DAT], edx
        inc dword [lam_n]
        clc
        jmp .x
.no:
        stc
.x:
        pop edi
        pop edx
        pop ecx
        ret

lam_dat_cambia:
        pushad
        mov ebp, esi                ; EBP=lo que se pone
        call lam_ptr
        mov edi, [esi+LO_LARGO]
        cmp ebx, edi                ; el sitio y lo que se quita, dentro
        jbe .a
        mov ebx, edi
.a:
        sub edi, ebx
        cmp ecx, edi
        jbe .b
        mov ecx, edi
.b:
        mov edi, edx
        sub edi, ecx                ; EDI=lo que crece (o encoge)
        mov eax, [lam_dat_n]
        add eax, edi
        cmp eax, LAM_DAT_TAM
        jg .no
        cmp eax, [lam_dat+4]        ; ¿cabe en lo que hay pedido? Si no, se pide más
        jle .cabe
        push eax
        push ebx
        mov ebx, [lam_dat]          ; (lo que se pone puede estar en los propios
        mov eax, ebp                ; datos: se mueve con ellos)
        sub eax, ebx
        cmp eax, [lam_dat+4]
        pop ebx
        pop eax
        pushf
        call lam_dat_pide
        jc .no_f
        popf
        jae .cabe
        sub ebp, [lam_d_antes]
        add ebp, [lam_dat]
        jmp .cabe
.no_f:
        popf
        jmp .no
.cabe:
        push esi
        push ecx
        push edx
        ; lo que hay detrás se corre
        mov eax, [esi+LO_DAT]
        add eax, ebx
        add eax, ecx                ; EAX=donde empieza lo que se corre
        mov ecx, [lam_dat_n]
        sub ecx, eax                ; ECX=cuánto es
        jbe .corrido
        test edi, edi
        jz .corrido
        js .abajo
        mov esi, [lam_dat]              ; hacia arriba: de atrás adelante
        add esi, eax
        lea esi, [esi+ecx-1]
        lea edx, [esi+edi]
        xchg edi, edx
        std
        rep movsb
        cld
        mov edi, edx
        jmp .corrido
.abajo:
        mov esi, [lam_dat]
        add esi, eax
        lea edx, [esi+edi]
        xchg edi, edx
        rep movsb
        mov edi, edx
.corrido:
        pop edx
        pop ecx
        pop esi
        ; los datos de los demás objetos que estaban detrás, también
        mov eax, [esi+LO_DAT]
        add eax, [esi+LO_LARGO]     ; EAX=donde acababan los de este
        push ecx
        push edx
        mov ecx, [lam_n]
        mov edx, lam_obj
.o:
        test ecx, ecx
        jz .o2
        cmp edx, esi
        je .o1
        cmp [edx+LO_DAT], eax
        jb .o1
        add [edx+LO_DAT], edi
.o1:
        add edx, LO_TAM
        dec ecx
        jmp .o
.o2:
        pop edx
        pop ecx
        add [esi+LO_LARGO], edi
        add [lam_dat_n], edi
        mov ecx, edx                ; y lo nuevo, a su sitio
        mov edi, [esi+LO_DAT]
        add edi, [lam_dat]
        add edi, ebx
        mov esi, ebp
        rep movsb
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; (interna) que en lam_dat quepan EAX octetos (y algo más); CF=1 si no hay
; memoria. Deja en [lam_d_antes] dónde estaban los datos
lam_dat_pide:
        push eax
        push ebx
        push ecx
        mov ecx, [lam_dat]
        mov [lam_d_antes], ecx
        lea ecx, [eax+16384]
        cmp ecx, LAM_DAT_TAM
        jbe .a
        mov ecx, LAM_DAT_TAM
        cmp ecx, eax
        jae .a
        mov ecx, eax
.a:
        mov ebx, lam_dat
        mov al, 1
        call lam_bloque
        pop ecx
        pop ebx
        pop eax
        ret

; EBX=un bloque (tres dd: puntero, lo que cabe y mango) ECX=octetos AL=1 para
; conservar lo que tiene: que quepan al menos esos (si hay que pedir otro más
; grande, el de antes se suelta); CF=1 si no hay memoria (se queda como estaba)
lam_bloque:
        pushad
        cmp ecx, [ebx+4]
        jbe .ok
        add ecx, 16383
        and ecx, ~16383
        mov ebp, ecx
        HV mem_bloque               ; EAX=puntero EDX=mango
        jc .no
        mov esi, [ebx]
        test esi, esi
        jz .pon
        cmp byte [esp+28], 0
        je .suelta
        mov edi, eax
        mov ecx, [ebx+4]
        rep movsb
.suelta:
        push edx
        mov edx, [ebx+8]
        HV mem_bloque_soltar
        pop edx
.pon:
        mov [ebx], eax
        mov [ebx+4], ebp
        mov [ebx+8], edx
.ok:
        popad
        clc
        ret
.no:
        popad
        stc
        ret
; EBX=un bloque: lo suelta
lam_bloque_suelta:
        pushad
        cmp dword [ebx], 0
        je .x
        mov edx, [ebx+8]
        HV mem_bloque_soltar
        xor eax, eax
        mov [ebx], eax
        mov [ebx+4], eax
        mov [ebx+8], eax
.x:
        popad
        ret
; suelta la memoria del motor (al acabar el programa)
lam_suelta:
        push ebx
        mov ebx, lam_dat
        call lam_bloque_suelta
        mov ebx, lam_buf
        call lam_bloque_suelta
        pop ebx
        ret

lam_obj_quita:
        pushad
        cmp eax, [lam_n]
        jae .x
        xor ebx, ebx                ; sus datos, fuera
        mov ecx, -1
        xor edx, edx
        call lam_dat_cambia
        call lam_ptr                ; y las fichas de detrás, una atrás
        mov edi, esi
        add esi, LO_TAM
        mov ecx, [lam_n]
        sub ecx, eax
        dec ecx
        imul ecx, LO_TAM
        rep movsb
        dec dword [lam_n]
.x:
        popad
        ret

lam_obj_dup:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        call lam_ptr
        mov ebx, esi                ; EBX=la ficha de origen
        mov al, [ebx+LO_TIPO]
        mov cl, [ebx+LO_PAG]
        call lam_obj_nuevo
        jc .x
        push eax
        mov edx, [esi+LO_DAT]
        mov edi, esi
        mov esi, ebx
        mov ecx, LO_TAM
        rep movsb
        lea esi, [edi-LO_TAM]
        mov [esi+LO_DAT], edx
        mov dword [esi+LO_LARGO], 0
        pop eax
        mov esi, [ebx+LO_DAT]       ; sus datos: al final de lam_dat (no se
        add esi, [lam_dat]            ; corre nada, así que pueden salir de ahí)
        mov edx, [ebx+LO_LARGO]
        xor ebx, ebx
        xor ecx, ecx
        call lam_dat_cambia
        jnc .x
        dec dword [lam_n]           ; no caben: la copia, fuera
        stc
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

lam_obj_orden:
        push ebx
        push ecx
        push esi
        push edi
        cmp eax, [lam_n]
        jae .x
        call lam_ptr
        mov ebx, esi
        mov edi, lam_tmp            ; su ficha, aparte
        mov ecx, LO_TAM
        rep movsb
        test edx, edx
        jz .fondo
        mov edi, ebx                ; al frente: las de detrás, una atrás
        lea esi, [ebx+LO_TAM]
        mov ecx, [lam_n]
        sub ecx, eax
        dec ecx
        imul ecx, LO_TAM
        rep movsb
        mov eax, [lam_n]
        dec eax
        jmp .pon
.fondo:
        lea esi, [ebx-1]            ; al fondo: las de delante, una adelante
        lea edi, [ebx+LO_TAM-1]
        imul ecx, eax, LO_TAM
        std
        rep movsb
        cld
        xor eax, eax
.pon:
        call lam_ptr
        mov edi, esi
        mov esi, lam_tmp
        mov ecx, LO_TAM
        rep movsb
.x:
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret

lam_pag_nueva:
        push eax
        push esi
        cmp dword [lam_pags], LAM_MAX_PAG
        jae .no
        xor eax, eax
.b:
        cmp eax, [lam_n]
        jae .f
        call lam_ptr
        cmp [esi+LO_PAG], cl
        jbe .s
        inc byte [esi+LO_PAG]
.s:
        inc eax
        jmp .b
.f:
        inc dword [lam_pags]
        clc
        jmp .x
.no:
        stc
.x:
        pop esi
        pop eax
        ret

lam_pag_quita:
        push eax
        push esi
        mov eax, [lam_n]
.b:
        dec eax                     ; de arriba abajo: al quitar uno no se
        js .f                       ; mueven los que faltan por mirar
        call lam_ptr
        cmp [esi+LO_PAG], cl
        jb .b
        je .q
        dec byte [esi+LO_PAG]
        jmp .b
.q:
        call lam_obj_quita
        jmp .b
.f:
        cmp dword [lam_pags], 1
        jbe .x
        dec dword [lam_pags]
.x:
        pop esi
        pop eax
        ret

lam_pag_mueve:
        push eax
        push ebx
        push esi
        cmp ecx, edx
        je .x
        xor eax, eax
.b:
        cmp eax, [lam_n]
        jae .x
        call lam_ptr
        movzx ebx, byte [esi+LO_PAG]
        cmp ebx, ecx
        jne .otra
        mov [esi+LO_PAG], dl        ; las suyas, a su sitio nuevo
        jmp .s
.otra:
        cmp ecx, edx
        ja .atras
        cmp ebx, ecx                ; va hacia delante: las de en medio, una atrás
        jb .s
        cmp ebx, edx
        ja .s
        dec byte [esi+LO_PAG]
        jmp .s
.atras:
        cmp ebx, edx                ; va hacia atrás: las de en medio, una adelante
        jb .s
        cmp ebx, ecx
        ja .s
        inc byte [esi+LO_PAG]
.s:
        inc eax
        jmp .b
.x:
        pop esi
        pop ebx
        pop eax
        ret

lam_pag_dup:
        push eax
        push ebx
        push edx
        push esi
        call lam_pag_nueva
        jc .x
        mov ebx, [lam_n]            ; (las copias se añaden detrás: no se miran)
        xor edx, edx
.b:
        cmp edx, ebx
        jae .ok
        mov eax, edx
        call lam_ptr
        cmp [esi+LO_PAG], cl
        jne .s
        call lam_obj_dup
        jc .x
        call lam_ptr
        inc byte [esi+LO_PAG]
.s:
        inc edx
        jmp .b
.ok:
        clc
.x:
        pop esi
        pop edx
        pop ebx
        pop eax
        ret

; =============================================================================
;  El dibujo
; =============================================================================
lam_pinta:
        pushad
        mov [lam_p_pag], ecx
        mov byte [lam_sin_mem], 1   ; el búfer, de la medida de la lámina
        mov ecx, [lam_an]
        imul ecx, [lam_al]
        mov ebx, lam_buf
        xor al, al
        call lam_bloque
        jc .x
        mov byte [lam_sin_mem], 0
        xor edi, edi
        call lam_destino
        mov ecx, [lam_an]           ; el papel, en blanco
        imul ecx, [lam_al]
        mov edi, [lam_buf]
        mov al, C_BLANCO
        rep stosb
        cmp dword [lam_fondo_fn], 0 ; o como lo quiera el programa
        je .obj
        call lam_recorte
        mov ecx, [lam_p_pag]
        call [lam_fondo_fn]
.obj:
        xor eax, eax
.b:
        cmp eax, [lam_n]
        jae .x
        call lam_ptr
        movzx edx, byte [esi+LO_PAG]
        cmp edx, [lam_p_pag]
        jne .s
        test byte [esi+LO_BAND], LB_OCULTO
        jnz .s
        call lam_pinta_obj
.s:
        inc eax
        jmp .b
.x:
        popad
        ret

lam_destino:
        push eax
        test edi, edi
        jnz .otro
        mov eax, [lam_buf]
        mov [lam_dst], eax
        mov eax, [lam_an]
        mov [lam_dst_an], eax
        mov eax, [lam_al]
        mov [lam_dst_al], eax
        pop eax
        ret
.otro:
        mov [lam_dst], edi
        mov [lam_dst_an], ecx
        mov [lam_dst_al], edx
        pop eax
        ret

; (interna) el recorte, a todo el búfer de destino
lam_recorte:
        push eax
        mov dword [lam_cx0], 0
        mov dword [lam_cy0], 0
        mov eax, [lam_dst_an]
        mov [lam_cx1], eax
        mov eax, [lam_dst_al]
        mov [lam_cy1], eax
        pop eax
        ret

lam_pinta_obj:
        pushad
        call lam_recorte
        movzx eax, byte [esi+LO_TIPO]
        cmp eax, LT_TEXTO
        je .texto
        cmp eax, LT_RECT
        je .rect
        cmp eax, LT_ELIPSE
        je .elipse
        cmp eax, LT_LINEA
        je .linea
        cmp eax, LT_IMAGEN
        jne .x
        call lam_p_imagen
        call lam_p_marco
        jmp .x
.texto:
        call lam_p_caja
        call lam_p_texto
        jmp .x
.rect:
        call lam_p_caja
        jmp .x
.elipse:
        call lam_p_elipse
        jmp .x
.linea:
        call lam_p_linea
.x:
        popad
        ret

; (interna) un tramo de una fila: de la x EAX a la ECX (sin ella), en la fila
; EBX, del color [lam_color]; lo que cae fuera del recorte no se pinta
lam_tramo:
        pushad
        cmp ebx, [lam_cy0]
        jl .x
        cmp ebx, [lam_cy1]
        jge .x
        cmp eax, [lam_cx0]
        jge .a
        mov eax, [lam_cx0]
.a:
        cmp ecx, [lam_cx1]
        jle .b
        mov ecx, [lam_cx1]
.b:
        sub ecx, eax
        jle .x
        imul ebx, [lam_dst_an]
        add ebx, [lam_dst]
        lea edi, [ebx+eax]
        mov al, [lam_color]
        rep stosb
.x:
        popad
        ret

; (interna) un rectángulo lleno: EAX=x EBX=y ECX=ancho EDX=alto
lam_caja:
        pushad
        add ecx, eax
.f:
        test edx, edx
        jle .x
        call lam_tramo
        inc ebx
        dec edx
        jmp .f
.x:
        popad
        ret

; (interna) ESI=ficha -> EBP=el grosor de su borde: sin pasar de la mitad de
; su lado menor (y al menos 1, si lo tiene)
lam_grosor:
        push eax
        movzx ebp, byte [esi+LO_GROSOR]
        mov eax, [esi+LO_AN]
        cmp eax, [esi+LO_AL]
        jle .a
        mov eax, [esi+LO_AL]
.a:
        sar eax, 1
        cmp eax, 1
        jge .b
        mov eax, 1
.b:
        cmp ebp, eax
        jbe .x
        mov ebp, eax
.x:
        pop eax
        ret

; (interna) el relleno y el borde de una caja
lam_p_caja:
        pushad
        mov al, [esi+LO_FONDO]
        cmp al, LAM_NADA
        je .m
        mov [lam_color], al
        mov eax, [esi+LO_X]
        mov ebx, [esi+LO_Y]
        mov ecx, [esi+LO_AN]
        mov edx, [esi+LO_AL]
        call lam_caja
.m:
        call lam_p_marco
        popad
        ret
lam_p_marco:
        pushad
        mov al, [esi+LO_BORDE]
        cmp al, LAM_NADA
        je .x
        mov [lam_color], al
        call lam_grosor
        test ebp, ebp
        jz .x
        mov eax, [esi+LO_X]         ; arriba
        mov ebx, [esi+LO_Y]
        mov ecx, [esi+LO_AN]
        mov edx, ebp
        call lam_caja
        add ebx, [esi+LO_AL]        ; abajo
        sub ebx, ebp
        call lam_caja
        mov ebx, [esi+LO_Y]         ; a la izquierda
        mov ecx, ebp
        mov edx, [esi+LO_AL]
        call lam_caja
        add eax, [esi+LO_AN]        ; a la derecha
        sub eax, ebp
        call lam_caja
.x:
        popad
        ret

; ---- la elipse ----
; (interna) EAX -> EAX=su raíz cuadrada (la parte entera)
lam_raiz:
        push ebx
        push ecx
        push edx
        mov ecx, eax
        xor eax, eax
        mov ebx, 1 << 30
.a:
        cmp ebx, ecx
        jbe .b
        shr ebx, 2
        jnz .a
.b:
        test ebx, ebx
        jz .x
        lea edx, [eax+ebx]
        shr eax, 1
        cmp ecx, edx
        jb .c
        sub ecx, edx
        add eax, ebx
.c:
        shr ebx, 2
        jmp .b
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) la fila EBX (de 0 a alto-1) de una elipse de ECX de ancho y EDX
; de alto -> EAX=dónde empieza y ECX=dónde acaba (sin él), desde su borde
; izquierdo; si no hay nada en esa fila, EAX=ECX
lam_elip_fila:
        push ebx
        push edx
        push esi
        push edi
        mov esi, ecx                ; ESI=ancho
        mov edi, edx                ; EDI=alto
        test esi, esi
        jle .nada
        test edi, edi
        jle .nada
        cmp ebx, edi
        jae .nada
        lea eax, [ebx*2+1]
        sub eax, edi                ; la distancia al centro, por dos
        imul eax, eax
        mov ecx, edi
        imul ecx, ecx
        sub ecx, eax                ; alto² - d²
        jle .nada
        mov eax, ecx
        call lam_raiz
        mul esi                     ; ancho x raíz / alto, redondeando
        mov ecx, edi
        shr ecx, 1
        add eax, ecx
        adc edx, 0
        div edi
        mov ecx, eax                ; ECX=lo que mide la fila
        mov eax, esi
        sub eax, ecx
        sar eax, 1
        add ecx, eax
        jmp .x
.nada:
        xor eax, eax
        xor ecx, ecx
.x:
        pop edi
        pop esi
        pop edx
        pop ebx
        ret

lam_p_elipse:
        pushad
        xor ebp, ebp                ; el grosor del borde (0: sin borde)
        cmp byte [esi+LO_BORDE], LAM_NADA
        je .g
        call lam_grosor
.g:
        mov [lam_e_g], ebp
        xor edi, edi                ; la fila
.f:
        cmp edi, [esi+LO_AL]
        jge .x
        mov ebx, edi
        mov ecx, [esi+LO_AN]
        mov edx, [esi+LO_AL]
        call lam_elip_fila
        cmp eax, ecx
        jae .s
        mov [lam_e_x0], eax
        mov [lam_e_x1], ecx
        mov ebp, [lam_e_g]
        test ebp, ebp
        jz .sin_borde
        ; con borde: la elipse de dentro es la misma, EBP más pequeña por
        ; cada lado; lo que queda entre las dos es el borde
        mov ebx, edi
        sub ebx, ebp
        js .todo_borde
        mov ecx, [esi+LO_AN]
        sub ecx, ebp
        sub ecx, ebp
        mov edx, [esi+LO_AL]
        sub edx, ebp
        sub edx, ebp
        call lam_elip_fila
        cmp eax, ecx
        jae .todo_borde
        add eax, ebp
        add ecx, ebp
        push eax
        push ecx
        mov dl, [esi+LO_BORDE]
        mov [lam_color], dl
        mov ebx, [esi+LO_Y]
        add ebx, edi
        mov ecx, eax                ; el borde, a la izquierda
        add ecx, [esi+LO_X]
        mov eax, [lam_e_x0]
        add eax, [esi+LO_X]
        call lam_tramo
        pop eax                     ; y a la derecha
        push eax
        add eax, [esi+LO_X]
        mov ecx, [lam_e_x1]
        add ecx, [esi+LO_X]
        call lam_tramo
        pop ecx
        pop eax
        mov dl, [esi+LO_FONDO]      ; y el relleno, en medio
        cmp dl, LAM_NADA
        je .s
        mov [lam_color], dl
        add eax, [esi+LO_X]
        add ecx, [esi+LO_X]
        call lam_tramo
        jmp .s
.todo_borde:
        mov dl, [esi+LO_BORDE]
        jmp .fila
.sin_borde:
        mov dl, [esi+LO_FONDO]
        cmp dl, LAM_NADA
        je .s
.fila:
        mov [lam_color], dl
        mov eax, [lam_e_x0]
        add eax, [esi+LO_X]
        mov ecx, [lam_e_x1]
        add ecx, [esi+LO_X]
        mov ebx, [esi+LO_Y]
        add ebx, edi
        call lam_tramo
.s:
        inc edi
        jmp .f
.x:
        popad
        ret

; ---- la línea: de (x, y) a (x+ancho, y+alto), con una pluma cuadrada ----
lam_p_linea:
        pushad
        mov al, [esi+LO_BORDE]
        cmp al, LAM_NADA
        je .x
        mov [lam_color], al
        movzx eax, byte [esi+LO_GROSOR]
        test eax, eax
        jnz .g
        inc eax
.g:
        mov [lam_v_g], eax
        mov eax, [esi+LO_X]
        mov [lam_v_x], eax
        mov eax, [esi+LO_Y]
        mov [lam_v_y], eax
        mov eax, [esi+LO_AN]
        mov ebx, 1
        test eax, eax
        jns .dx
        neg eax
        neg ebx
.dx:
        mov [lam_v_dx], eax
        mov [lam_v_sx], ebx
        mov ecx, [esi+LO_AL]
        mov ebx, 1
        test ecx, ecx
        jns .dy
        neg ecx
        neg ebx
.dy:
        mov [lam_v_dy], ecx
        mov [lam_v_sy], ebx
        sub eax, ecx
        mov [lam_v_err], eax
        mov edi, [esi+LO_X]         ; EDI, EBP=el segundo extremo
        add edi, [esi+LO_AN]
        mov ebp, [esi+LO_Y]
        add ebp, [esi+LO_AL]
.p:
        mov eax, [lam_v_g]          ; la pluma, centrada en el punto
        mov edx, eax
        shr eax, 1
        mov ebx, [lam_v_y]
        sub ebx, eax
        neg eax
        add eax, [lam_v_x]
        mov ecx, [lam_v_g]
        call lam_caja
        cmp [lam_v_x], edi
        jne .sigue
        cmp [lam_v_y], ebp
        je .x
.sigue:
        mov eax, [lam_v_err]
        shl eax, 1
        mov ebx, [lam_v_dy]
        neg ebx
        cmp eax, ebx
        jle .n1
        mov ebx, [lam_v_dy]
        sub [lam_v_err], ebx
        mov ebx, [lam_v_sx]
        add [lam_v_x], ebx
.n1:
        cmp eax, [lam_v_dx]
        jge .p
        mov ebx, [lam_v_dx]
        add [lam_v_err], ebx
        mov ebx, [lam_v_sy]
        add [lam_v_y], ebx
        jmp .p
.x:
        popad
        ret

; ---- la imagen: sus puntos, estirados o encogidos hasta llenar la caja ----
lam_p_imagen:
        pushad
        movzx eax, word [esi+LO_IMG_AN]
        movzx ebx, word [esi+LO_IMG_AL]
        test eax, eax
        jz .x
        test ebx, ebx
        jz .x
        cmp dword [esi+LO_AN], 0
        jle .x
        cmp dword [esi+LO_AL], 0
        jle .x
        shl eax, 16                 ; lo que avanza en la imagen por cada
        xor edx, edx                ; punto de la lámina, en 65536 avos
        div dword [esi+LO_AN]
        mov [lam_i_pasox], eax
        mov eax, ebx
        shl eax, 16
        xor edx, edx
        div dword [esi+LO_AL]
        mov [lam_i_pasoy], eax
        xor ebp, ebp                ; la fila de la caja
        xor edx, edx                ; y por dónde va en la imagen
.f:
        cmp ebp, [esi+LO_AL]
        jge .x
        mov ebx, [esi+LO_Y]
        add ebx, ebp
        cmp ebx, [lam_cy0]
        jl .sf
        cmp ebx, [lam_cy1]
        jge .x
        imul ebx, [lam_dst_an]
        add ebx, [lam_dst]          ; EBX=la fila del búfer
        push edx
        push ebp
        shr edx, 16
        movzx eax, word [esi+LO_IMG_AN]
        imul edx, eax
        add edx, [esi+LO_DAT]
        add edx, [lam_dat]            ; EDX=la fila de la imagen
        xor ecx, ecx                ; la columna
        xor ebp, ebp
        mov edi, [esi+LO_X]
.c:
        cmp ecx, [esi+LO_AN]
        jge .cf
        cmp edi, [lam_cx0]
        jl .cs
        cmp edi, [lam_cx1]
        jge .cf
        mov eax, ebp
        shr eax, 16
        mov al, [edx+eax]
        mov [ebx+edi], al
.cs:
        add ebp, [lam_i_pasox]
        inc edi
        inc ecx
        jmp .c
.cf:
        pop ebp
        pop edx
.sf:
        add edx, [lam_i_pasoy]
        inc ebp
        jmp .f
.x:
        popad
        ret

; =============================================================================
;  El texto de una caja
; =============================================================================
; (interna) ECX=carácter -> ECX (lo que no es letra, un espacio) EDX=lo que
; avanza, con la letra de la línea que se compone. El tabulador (9) no ocupa:
; al principio de un párrafo dice su nivel en una lista
lam_ancho_car:
        push eax
        xor edx, edx
        cmp ecx, 9
        je .x
        cmp ecx, 32
        jae .a
        mov ecx, 32
.a:
        movzx eax, byte [lam_t_tipo]
        movzx edx, byte [lam_t_tam]
        call tl_ancho
        mov edx, eax
.x:
        pop eax
        ret

lam_compone:
        push eax
        mov eax, [esi+LO_DAT]
        add eax, [lam_dat]
        mov [lam_t_txt], eax
        mov eax, [esi+LO_LARGO]
        mov [lam_t_n], eax
        mov al, [esi+LO_BAND]
        and al, LB_VINETAS
        mov [lam_t_vin], al
        pop eax
; (interna) lo mismo, con el texto que haya en [lam_t_txt] ([lam_t_n] letras)
; y con viñetas o sin ellas según [lam_t_vin]: así se compone también el
; texto de ejemplo de una caja vacía, que no está en lam_dat
lam_compone_x:
        pushad
        mov al, [esi+LO_LETRA]
        mov [lam_t_tipo], al
        mov al, [esi+LO_CUERPO]
        cmp al, TL_TAMANOS
        jb .t
        xor al, al
.t:
        mov [lam_t_base], al
        mov [lam_t_tam], al
        mov al, [esi+LO_ESTILO]
        mov [lam_t_estilo], al
        movzx edx, byte [lam_t_base]
        call tl_alto_de
        mov [lam_l_alto], eax
        mov eax, [esi+LO_AN]
        sub eax, 2 * LAM_SEP
        cmp eax, 8
        jge .w
        mov eax, 8
.w:
        mov [lam_l_ancho], eax
        mov ebp, [lam_t_n]          ; EBP=cuánto texto
        mov ebx, [lam_t_txt]        ; EBX=el texto
        xor edi, edi                ; EDI=la línea
        xor esi, esi                ; ESI=por dónde va
        mov dword [lam_c_y], 0
        mov byte [lam_c_par], 1     ; la línea que viene empieza un párrafo
.lin:
        cmp esi, ebp
        jae .fin
        cmp edi, LAM_MAX_LIN
        jae .lleno
        call .abre
.c:
        cmp esi, ebp
        jae .lin
        movzx ecx, byte [ebx+esi]
        cmp ecx, 10
        jne .c1
        inc esi
        mov byte [lam_c_par], 1
        jmp .lin
.c1:
        call lam_ancho_car
        add edx, [lam_c_w]
        cmp edx, [lam_c_cabe]
        ja .parte
        mov [lam_c_w], edx
        cmp ecx, ' '
        jne .c2
        mov [lam_c_esp], esi
.c2:
        inc esi
        jmp .c
.parte:
        cmp byte [ebx+esi], ' '     ; un espacio que no cabe se queda colgando
        ja .p0                      ; al final de su línea
        inc esi
        jmp .lin
.p0:
        mov eax, [lam_c_esp]        ; se parte detrás del último espacio
        cmp eax, -1
        je .p1
        lea esi, [eax+1]
        jmp .lin
.p1:
        cmp esi, [lam_c_ini]        ; (una palabra más larga que la caja: por
        jne .lin                    ; donde caiga; y al menos una letra)
        inc esi
        jmp .lin
.fin:
        ; detrás de un salto de línea (o sin texto) hay una línea vacía, donde
        ; puede estar el cursor
        test ebp, ebp
        jz .extra
        cmp byte [ebx+ebp-1], 10
        jne .lleno
.extra:
        cmp edi, LAM_MAX_LIN
        jae .lleno
        call .abre
.lleno:
        mov [lam_l_n], edi
        mov [lam_l_ini+edi*4], esi
        mov eax, [lam_c_y]
        mov [lam_l_y+edi*4], eax
        popad
        ret
; (interna) abre la línea EDI en el sitio ESI. Si empieza un párrafo y la
; caja lleva viñetas, los tabuladores que tenga delante (hasta dos) dicen su
; nivel: cada nivel, más sangría y la letra un tamaño más pequeña
.abre:
        cmp byte [lam_c_par], 0
        je .ab2
        mov byte [lam_c_niv], 0x40  ; (0x40: empieza un párrafo)
        mov dword [lam_c_izq], 0
        mov al, [lam_t_base]
        mov [lam_t_tam], al
        cmp byte [lam_t_vin], 0
        je .ab2
        xor ecx, ecx
.ab0:
        lea eax, [esi+ecx]
        cmp eax, ebp
        jae .ab1
        cmp byte [ebx+eax], 9
        jne .ab1
        inc ecx
        cmp ecx, 2
        jb .ab0
.ab1:
        mov al, cl
        or al, 0xC0                 ; (0x80: lleva viñeta)
        mov [lam_c_niv], al
        lea eax, [ecx+1]
        imul eax, [lam_l_alto]
        mov [lam_c_izq], eax
        movzx eax, byte [lam_t_base]
        movzx eax, byte [tl_orden+eax]
        sub eax, ecx
        jns .ab1b
        xor eax, eax
.ab1b:
        mov al, [tl_orden+eax]
        mov [lam_t_tam], al
.ab2:
        mov [lam_l_ini+edi*4], esi
        mov eax, [lam_c_y]
        mov [lam_l_y+edi*4], eax
        mov al, [lam_t_tam]
        mov [lam_l_tam+edi], al
        mov al, [lam_c_niv]
        mov [lam_l_niv+edi], al
        and byte [lam_c_niv], 0x03  ; (las líneas siguientes del párrafo, sin marcas)
        mov eax, [lam_c_izq]
        mov [lam_l_izq+edi*4], eax
        push edx
        movzx edx, byte [lam_t_tam]
        call tl_alto_de
        pop edx
        add [lam_c_y], eax
        mov eax, [lam_l_ancho]
        sub eax, [lam_c_izq]
        cmp eax, 8
        jge .ab3
        mov eax, 8
.ab3:
        mov [lam_c_cabe], eax
        inc edi
        mov byte [lam_c_par], 0
        mov dword [lam_c_w], 0
        mov dword [lam_c_esp], -1
        mov [lam_c_ini], esi
        ret

; (interna) mide la línea EDI de la caja ESI (ya compuesta): [lam_m_s],
; [lam_m_e], [lam_m_f] y [lam_m_x0]; y deja puesta la letra de esa línea
lam_mide:
        pushad
        mov al, [lam_l_tam+edi]
        mov [lam_t_tam], al
        mov eax, [lam_l_ini+edi*4]
        mov [lam_m_s], eax
        mov edx, [lam_l_ini+edi*4+4]
        mov ebx, [lam_t_txt]
        cmp edx, eax
        jbe .t1
        cmp byte [ebx+edx-1], 10
        jne .t1
        dec edx
.t1:
        mov [lam_m_e], edx
.t2:
        cmp edx, eax                ; los espacios del final no cuentan para
        jbe .t3                     ; centrar ni para arrimar a la derecha
        cmp byte [ebx+edx-1], ' '
        ja .t3
        dec edx
        jmp .t2
.t3:
        mov [lam_m_f], edx
        xor ebp, ebp
        push edi
        mov edi, eax
.w:
        cmp edi, edx
        jae .w1
        movzx ecx, byte [ebx+edi]
        push edx
        call lam_ancho_car
        add ebp, edx
        pop edx
        inc edi
        jmp .w
.w1:
        pop edi
        mov eax, [lam_l_ancho]      ; lo que sobra
        sub eax, [lam_l_izq+edi*4]
        sub eax, ebp
        jns .a
        xor eax, eax
.a:
        movzx ecx, byte [esi+LO_ALIN]
        cmp ecx, 2
        je .p
        cmp ecx, 1
        jne .izq
        shr eax, 1
        jmp .p
.izq:
        xor eax, eax
.p:
        add eax, LAM_SEP
        add eax, [lam_l_izq+edi*4]
        mov [lam_m_x0], eax
        popad
        ret

lam_sitio_xy:
        push edx
        push edi
        push ebp
        mov ebp, eax
        xor edi, edi                ; su línea: la última que empieza antes
.b:
        lea edx, [edi+1]
        cmp edx, [lam_l_n]
        jae .f
        cmp ebp, [lam_l_ini+edx*4]
        jb .f
        inc edi
        jmp .b
.f:
        call lam_mide
        mov eax, [lam_m_x0]
        mov ebx, [lam_m_s]
.c:
        cmp ebx, ebp
        jae .h
        cmp ebx, [lam_t_n]
        jae .h
        mov edx, [lam_t_txt]
        movzx ecx, byte [edx+ebx]
        call lam_ancho_car
        add eax, edx
        inc ebx
        jmp .c
.h:
        push eax
        movzx edx, byte [lam_l_tam+edi]
        call tl_alto_de
        mov ecx, eax
        pop eax
        mov ebx, [lam_l_y+edi*4]
        add ebx, LAM_SEP
        pop ebp
        pop edi
        pop edx
        ret

lam_xy_sitio:
        push ebx
        push ecx
        push edx
        push edi
        push ebp
        mov ebp, eax                ; EBP=la x
        sub ebx, LAM_SEP
        jns .y
        xor ebx, ebx
.y:
        xor edi, edi                ; la línea de esa altura
.y0:
        lea edx, [edi+1]
        cmp edx, [lam_l_n]
        jae .y1
        cmp ebx, [lam_l_y+edx*4]
        jl .y1
        inc edi
        jmp .y0
.y1:
        call lam_mide
        mov eax, [lam_m_s]
        mov ebx, [lam_m_x0]
.c:
        cmp eax, [lam_m_e]
        jae .fin
        mov edx, [lam_t_txt]
        movzx ecx, byte [edx+eax]
        call lam_ancho_car
        mov ecx, edx
        shr ecx, 1
        add ecx, ebx
        cmp ebp, ecx
        jl .x
        add ebx, edx
        inc eax
        jmp .c
.fin:
        ; al final de una línea partida: delante del espacio por el que se
        ; partió (detrás ya es la línea siguiente)
        cmp eax, [lam_m_s]
        je .x
        cmp eax, [lam_l_ini+edi*4+4]
        jne .x
        inc edi
        cmp edi, [lam_l_n]
        jae .x
        mov edx, [lam_t_txt]
        cmp byte [edx+eax-1], ' '
        jne .x
        dec eax
.x:
        pop ebp
        pop edi
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) el texto de la caja ESI, dentro de ella. Una caja vacía con un
; papel (LO_PAPEL) enseña, si el programa lo ha pedido ([lam_guia_fn]), un
; texto de ejemplo que dice para qué es
lam_p_texto:
        pushad
        cmp dword [esi+LO_LARGO], 0
        jne .normal
        cmp dword [lam_guia_fn], 0
        je .x
        cmp esi, [lam_guia_salta]   ; (en la que se está escribiendo, no)
        je .x
        movzx eax, byte [esi+LO_PAPEL]
        test eax, eax
        jz .x
        push esi
        call [lam_guia_fn]          ; EAX=papel -> ESI=su texto (0: ninguno)
        mov eax, esi
        pop esi
        test eax, eax
        jz .x
        mov [lam_t_txt], eax
        xor ecx, ecx
.g:
        cmp byte [eax+ecx], 0
        je .g1
        inc ecx
        jmp .g
.g1:
        mov [lam_t_n], ecx
        mov byte [lam_t_vin], 0
        call lam_compone_x
        mov al, [lam_guia_color]
        jmp .pinta
.normal:
        call lam_compone
        mov al, [esi+LO_TINTA]
.pinta:
        mov [lam_color], al
        mov eax, [esi+LO_X]         ; lo que se sale de la caja no se dibuja
        cmp eax, [lam_cx0]
        jle .r1
        mov [lam_cx0], eax
.r1:
        add eax, [esi+LO_AN]
        cmp eax, [lam_cx1]
        jge .r2
        mov [lam_cx1], eax
.r2:
        mov eax, [esi+LO_Y]
        cmp eax, [lam_cy0]
        jle .r3
        mov [lam_cy0], eax
.r3:
        add eax, [esi+LO_AL]
        cmp eax, [lam_cy1]
        jge .r4
        mov [lam_cy1], eax
.r4:
        mov dword [lam_p_parr], 0
        xor edi, edi
.l:
        cmp edi, [lam_l_n]
        jae .x
        mov ebx, [lam_l_y+edi*4]
        add ebx, LAM_SEP
        cmp ebx, [esi+LO_AL]
        jge .x
        add ebx, [esi+LO_Y]
        test byte [lam_l_niv+edi], 0x40
        jz .l1
        inc dword [lam_p_parr]      ; otro párrafo: ¿se enseña ya? (los puntos
        cmp byte [lam_t_vin], 0     ; de una lista pueden salir de uno en uno)
        je .l1
        mov eax, [lam_parr_max]
        test eax, eax
        jz .l1
        cmp [lam_p_parr], eax
        jae .x
.l1:
        call lam_mide
        test byte [lam_l_niv+edi], 0x80
        jz .l2
        call .vineta
.l2:
        mov eax, [lam_m_x0]
        add eax, [esi+LO_X]
        mov ebp, [lam_m_s]
.c:
        cmp ebp, [lam_m_f]
        jae .sl
        mov edx, [lam_t_txt]
        movzx ecx, byte [edx+ebp]
        call lam_ancho_car
        cmp ecx, 9
        je .cs
        call lam_letra
.cs:
        add eax, edx
        inc ebp
        jmp .c
.sl:
        inc edi
        jmp .l
.x:
        popad
        ret
; (interna) la viñeta de la línea EDI (EBX=su y): un cuadrado del color de la
; letra en el primer nivel y una raya en los demás, en el hueco de la sangría
.vineta:
        pushad
        movzx edx, byte [lam_t_tam]
        call tl_alto_de             ; EAX=el alto de la línea
        mov ecx, eax
        shr ecx, 2                  ; ECX=el lado
        cmp ecx, 3
        jae .v1
        mov ecx, 3
.v1:
        mov edx, ecx                ; EDX=su alto
        test byte [lam_l_niv+edi], 3
        jz .v2
        shr edx, 1
.v2:
        push eax                    ; a media altura de las letras
        shr eax, 1
        add ebx, eax
        pop eax
        shr eax, 4
        add ebx, eax
        mov eax, edx
        shr eax, 1
        sub ebx, eax
        mov eax, [esi+LO_X]         ; y en medio del último hueco de sangría
        add eax, LAM_SEP
        add eax, [lam_l_izq+edi*4]
        push edx
        mov edx, [lam_l_alto]
        shr edx, 1
        sub eax, edx
        mov edx, ecx
        shr edx, 1
        sub eax, edx
        pop edx
        call lam_caja
        popad
        ret

; (interna) la letra ECX en (EAX, EBX: arriba de su línea), que avanza EDX,
; con el tipo, el tamaño y el estilo de la caja que se compone y el color
; [lam_color]. La negrita engorda el trazo, la cursiva inclina las filas y
; el subrayado va bajo la línea base (a tres cuartos de la casilla)
lam_letra:
        pushad
        mov [lam_g_x], eax
        mov [lam_g_y], ebx
        mov [lam_g_av], edx
        movzx eax, byte [lam_t_tipo]
        movzx edx, byte [lam_t_tam]
        call tl_letra               ; ESI=las filas de su dibujo
        mov eax, [tl_g_filas]       ; el grueso de un trazo: uno de cada 16
        shr eax, 4
        jnz .g1
        inc eax
.g1:
        mov [lam_g_gordo], eax
        xor ecx, ecx                ; ECX=la fila del dibujo
.fila:
        mov eax, ecx
        imul eax, [tl_g_esc]
        add eax, [lam_g_y]
        mov [lam_g_fy], eax
        xor edi, edi
        test byte [lam_t_estilo], LE_CUR
        jz .f3
        mov edi, [tl_g_filas]
        dec edi
        sub edi, ecx
        sar edi, 2
        sub edi, [lam_g_gordo]
        imul edi, [tl_g_esc]
.f3:
        add edi, [lam_g_x]          ; EDI=la x del primer punto de la fila
        mov edx, [tl_g_bpr]
.oct:
        mov al, [esi]
        inc esi
        mov ah, 8
.bit:
        test al, 0x80
        jz .nb
        push eax
        mov eax, [tl_g_esc]
        test byte [lam_t_estilo], LE_NEG
        jz .p1
        mov ebp, [lam_g_gordo]
        inc ebp
        imul eax, ebp
.p1:
        call .bloque
        pop eax
.nb:
        shl al, 1
        add edi, [tl_g_esc]
        dec ah
        jnz .bit
        dec edx
        jnz .oct
        test byte [lam_t_estilo], LE_SUB
        jz .sig
        mov eax, [tl_g_filas]
        lea eax, [eax+eax*2]
        shr eax, 2
        add eax, [lam_g_gordo]
        inc eax
        cmp ecx, eax
        jne .sig
        mov edi, [lam_g_x]
        mov eax, [lam_g_av]
        call .bloque
.sig:
        inc ecx
        cmp ecx, [tl_g_filas]
        jb .fila
        popad
        ret
; (interna) un bloque desde la x EDI, EAX de ancho, en la fila [lam_g_fy] y
; con [tl_g_esc] de alto
.bloque:
        pushad
        mov ebp, [tl_g_esc]
        mov ebx, [lam_g_fy]
        lea ecx, [edi+eax]
        mov eax, edi
.q:
        call lam_tramo
        inc ebx
        dec ebp
        jnz .q
        popad
        ret

; =============================================================================
;  La lámina, a otro tamaño
; =============================================================================
lam_reduce:
        pushad
        cmp byte [lam_sin_mem], 0   ; (sin lámina dibujada no hay nada que reducir)
        jne .x
        mov [lam_r_dw], ecx
        mov [lam_r_dh], edx
        mov [lam_r_suave], ebx
        test ecx, ecx
        jle .x
        test edx, edx
        jle .x
        xor esi, esi                ; a qué x de la lámina va cada columna
.tx:
        mov eax, esi
        mul dword [lam_an]
        div dword [lam_r_dw]
        mov [lam_r_x+esi*4], eax
        inc esi
        cmp esi, LAM_MAX_LADO
        ja .tx1
        cmp esi, [lam_r_dw]
        jbe .tx
.tx1:
        xor ebp, ebp                ; la fila
.fy:
        cmp ebp, [lam_r_dh]
        jae .x
        mov eax, ebp
        mul dword [lam_al]
        div dword [lam_r_dh]
        mov [lam_r_y0], eax
        lea eax, [ebp+1]
        mul dword [lam_al]
        div dword [lam_r_dh]
        cmp eax, [lam_r_y0]
        ja .y1
        mov eax, [lam_r_y0]
        inc eax
.y1:
        cmp eax, [lam_al]
        jbe .y2
        mov eax, [lam_al]
.y2:
        mov [lam_r_y1], eax
        xor ebx, ebx                ; la columna
.fx:
        cmp ebx, [lam_r_dw]
        jae .sy
        mov eax, [lam_r_x+ebx*4]
        mov [lam_r_x0], eax
        mov ecx, [lam_r_x+ebx*4+4]
        cmp ecx, eax
        ja .x1
        lea ecx, [eax+1]
.x1:
        cmp ecx, [lam_an]
        jbe .x2
        mov ecx, [lam_an]
.x2:
        mov [lam_r_x1], ecx
        call .bloque
        stosb
        inc ebx
        jmp .fx
.sy:
        inc ebp
        jmp .fy
.x:
        popad
        ret
; (interna) -> AL=el color del bloque de puntos de la lámina que va a un
; punto del búfer: si son todos iguales, ese; si no, el del cubo que más se
; parece a su mezcla
.bloque:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov esi, [lam_r_y0]
        imul esi, [lam_an]
        add esi, [lam_buf]            ; ESI=la primera fila del bloque
        mov edx, [lam_r_x0]
        mov al, [esi+edx]
        cmp dword [lam_r_suave], 0
        je .hecho
        mov ebp, [lam_r_y1]
        sub ebp, [lam_r_y0]
        mov edi, esi
.u1:
        mov edx, [lam_r_x0]
.u2:
        cmp al, [edi+edx]
        jne .mezcla
        inc edx
        cmp edx, [lam_r_x1]
        jb .u2
        add edi, [lam_an]
        dec ebp
        jnz .u1
        jmp .hecho
.mezcla:
        xor eax, eax
        mov [lam_r_r], eax
        mov [lam_r_g], eax
        mov [lam_r_b], eax
        mov [lam_r_n], eax
        mov ebx, [hv_api + HV_pal_rgb * 4]
        mov ebp, [lam_r_y1]
        sub ebp, [lam_r_y0]
.m1:
        mov edx, [lam_r_x0]
.m2:
        movzx eax, byte [esi+edx]
        mov eax, [ebx+eax*4]        ; rojo, verde, azul
        movzx ecx, al
        add [lam_r_r], ecx
        movzx ecx, ah
        add [lam_r_g], ecx
        shr eax, 16
        and eax, 0xFF
        add [lam_r_b], eax
        inc dword [lam_r_n]
        inc edx
        cmp edx, [lam_r_x1]
        jb .m2
        add esi, [lam_an]
        dec ebp
        jnz .m1
        mov ecx, 51                 ; cada uno, al escalón del cubo más cercano
        mov eax, [lam_r_r]
        call .escalon
        imul ebx, eax, 36
        mov eax, [lam_r_g]
        call .escalon
        imul eax, eax, 6
        add ebx, eax
        mov eax, [lam_r_b]
        call .escalon
        lea eax, [ebx+eax+16]
.hecho:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
.escalon:
        xor edx, edx
        div dword [lam_r_n]
        add eax, 25
        xor edx, edx
        div ecx
        ret

; =============================================================================
;  Qué objeto hay en un punto
; =============================================================================
lam_en_punto:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov [lam_h_x], eax
        mov [lam_h_y], ebx
        mov [lam_h_pag], ecx
        mov [lam_h_tol], edx
        mov eax, [lam_n]
.b:
        dec eax
        js .x                       ; (queda en -1)
        call lam_ptr
        movzx edx, byte [esi+LO_PAG]
        cmp edx, [lam_h_pag]
        jne .b
        test byte [esi+LO_BAND], LB_OCULTO
        jnz .b
        call lam_dentro
        jc .b
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) CF=0 si el punto ([lam_h_x], [lam_h_y]) toca el objeto ESI
lam_dentro:
        pushad
        movzx eax, byte [esi+LO_TIPO]
        cmp eax, LT_LINEA
        je .linea
        mov ebp, [lam_h_tol]
        mov eax, [lam_h_x]          ; EAX, EBX=el punto, desde su esquina
        sub eax, [esi+LO_X]
        mov ebx, [lam_h_y]
        sub ebx, [esi+LO_Y]
        mov ecx, eax                ; dentro de su caja (con la holgura)
        add ecx, ebp
        js .no
        mov ecx, ebx
        add ecx, ebp
        js .no
        mov ecx, [esi+LO_AN]
        add ecx, ebp
        cmp eax, ecx
        jge .no
        mov ecx, [esi+LO_AL]
        add ecx, ebp
        cmp ebx, ecx
        jge .no
        movzx ecx, byte [esi+LO_TIPO]
        cmp ecx, LT_ELIPSE
        je .elipse
        cmp ecx, LT_RECT
        jne .si                     ; una caja de texto o una imagen: toda
        cmp byte [esi+LO_FONDO], LAM_NADA
        jne .si
        ; un rectángulo sin relleno: solo el borde. No cuenta lo que queda
        ; dentro, a más de su grosor y la holgura
        movzx edx, byte [esi+LO_GROSOR]
        add ebp, edx                ; EBP=lo que cuenta desde el borde
        cmp eax, ebp
        jl .si
        cmp ebx, ebp
        jl .si
        mov ecx, [esi+LO_AN]
        sub ecx, ebp
        cmp eax, ecx
        jge .si
        mov ecx, [esi+LO_AL]
        sub ecx, ebp
        cmp ebx, ecx
        jge .si
        jmp .no
.elipse:
        test ebx, ebx               ; (la holgura, aquí, solo hacia dentro)
        js .no
        mov edi, eax                ; EDI=la x
        mov ecx, [esi+LO_AN]
        mov edx, [esi+LO_AL]
        call lam_elip_fila
        cmp edi, eax
        jl .no
        cmp edi, ecx
        jge .no
        cmp byte [esi+LO_FONDO], LAM_NADA
        jne .si
        movzx edx, byte [esi+LO_GROSOR]
        add ebp, edx
        sub ebx, ebp                ; ¿dentro de la elipse de dentro?
        js .si
        mov ecx, [esi+LO_AN]
        sub ecx, ebp
        sub ecx, ebp
        mov edx, [esi+LO_AL]
        sub edx, ebp
        sub edx, ebp
        call lam_elip_fila
        sub edi, ebp
        cmp edi, eax
        jl .si
        cmp edi, ecx
        jge .si
        jmp .no
.linea:
        ; la distancia del punto al segmento, sin pasar de la holgura más
        ; medio grosor
        movzx ebp, byte [esi+LO_GROSOR]
        shr ebp, 1
        add ebp, [lam_h_tol]
        inc ebp
        imul ebp, ebp               ; EBP=el radio, al cuadrado
        mov eax, [lam_h_x]
        sub eax, [esi+LO_X]         ; EAX, EBX=del primer extremo al punto
        mov ebx, [lam_h_y]
        sub ebx, [esi+LO_Y]
        mov ecx, [esi+LO_AN]
        mov edx, [esi+LO_AL]
        mov edi, ecx
        imul edi, edi
        push edx
        imul edx, edx
        add edi, edx                ; EDI=el largo, al cuadrado
        pop edx
        push eax
        push ebx
        imul eax, ecx
        imul ebx, edx
        add eax, ebx                ; EAX=lo que avanza a lo largo
        mov ecx, eax
        pop ebx
        pop eax
        test ecx, ecx
        jle .extremo                ; antes del primer extremo: hasta él
        cmp ecx, edi
        jl .medio
        sub eax, [esi+LO_AN]        ; pasado el segundo: hasta él
        sub ebx, [esi+LO_AL]
.extremo:
        imul eax, eax
        imul ebx, ebx
        add eax, ebx
        cmp eax, ebp
        ja .no
        jmp .si
.medio:
        imul eax, [esi+LO_AL]       ; lo que se aparta de la recta
        imul ebx, [esi+LO_AN]
        sub eax, ebx
        imul eax                    ; (al cuadrado: 64 bits)
        test edx, edx
        jnz .no
        mov ecx, eax
        mov eax, ebp
        mul edi
        test edx, edx
        jnz .si
        cmp ecx, eax
        ja .no
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; =============================================================================
;  El archivo
; =============================================================================
; (interna) escribe ECX octetos de ESI en el archivo; CF=1 si no caben
lam_f_escribe:
        push eax
        push ebx
        push ecx
        push edx
        push esi
        mov edx, ecx
.b:
        test edx, edx
        jz .ok
        mov ecx, edx                ; (a trozos: DOS no pasa de 64 K)
        cmp ecx, 16384
        jbe .t
        mov ecx, 16384
.t:
        mov ebx, [lam_f_arch]
        HV dos_escribir
        jc .x
        cmp eax, ecx
        jb .mal
        add esi, ecx
        sub edx, ecx
        jmp .b
.mal:
        stc
        jmp .x
.ok:
        clc
.x:
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret
; (interna) lee ECX octetos en EDI; CF=1 si no están
lam_f_lee:
        push eax
        push ebx
        push ecx
        push edx
        push edi
        mov edx, ecx
.b:
        test edx, edx
        jz .ok
        mov ecx, edx
        cmp ecx, 16384
        jbe .t
        mov ecx, 16384
.t:
        mov ebx, [lam_f_arch]
        HV dos_leer
        jc .x
        cmp eax, ecx
        jb .mal
        add edi, ecx
        sub edx, ecx
        jmp .b
.mal:
        stc
        jmp .x
.ok:
        clc
.x:
        pop edi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

lam_guarda:
        pushad
        mov ebp, edi
        HV dos_crear
        jc .no
        mov [lam_f_arch], eax
        mov esi, ebp                ; la cabecera
        mov edi, lam_fila
        mov ecx, 8
        rep movsb
        mov eax, [lam_an]
        stosw
        mov eax, [lam_al]
        stosw
        mov eax, [lam_margen]
        stosw
        mov eax, [lam_pags]
        stosw
        mov eax, [lam_n]
        stosw
        mov eax, LO_TAM
        stosw
        mov eax, [lam_dat_n]
        stosd
        mov esi, lam_fila
        mov ecx, 24
        call lam_f_escribe
        jc .mal
        mov esi, lam_obj            ; las fichas
        imul ecx, dword [lam_n], LO_TAM
        call lam_f_escribe
        jc .mal
        mov esi, [lam_dat]            ; y los datos
        mov ecx, [lam_dat_n]
        call lam_f_escribe
        jc .mal
        cmp dword [lam_extra], 0    ; y, si lo hay, lo del programa
        je .cierra
        mov esi, lam_extra
        mov ecx, 4
        call lam_f_escribe
        jc .mal
.cierra:
        mov ebx, [lam_f_arch]
        HV dos_cerrar
        popad
        clc
        ret
.mal:
        mov ebx, [lam_f_arch]
        HV dos_cerrar
.no:
        popad
        stc
        ret

lam_carga:
        pushad
        mov [lam_f_firma], edi
        xor al, al
        HV dos_abrir
        jc .noabre
        mov [lam_f_arch], eax
        mov edi, lam_fila
        mov ecx, 24
        call lam_f_lee
        jc .malo
        mov esi, lam_fila
        mov edi, [lam_f_firma]
        mov ecx, 8
        repe cmpsb
        jne .malo
        ; las medidas y las cuentas, dentro de lo que cabe
        movzx eax, word [lam_fila+8]
        movzx ebx, word [lam_fila+10]
        cmp eax, 16
        jb .malo
        cmp ebx, 16
        jb .malo
        cmp eax, LAM_MAX_LADO
        ja .malo
        cmp ebx, LAM_MAX_LADO
        ja .malo
        mov ecx, eax
        imul ecx, ebx
        cmp ecx, LAM_BUF_TAM
        ja .malo
        movzx ecx, word [lam_fila+14]
        test ecx, ecx
        jz .malo
        cmp ecx, LAM_MAX_PAG
        ja .malo
        movzx edx, word [lam_fila+16]
        cmp edx, LAM_MAX_OBJ
        ja .malo
        movzx esi, word [lam_fila+18]
        cmp esi, LO_TAM
        jb .malo
        cmp esi, 1024
        ja .malo
        mov edi, [lam_fila+20]
        cmp edi, LAM_DAT_TAM
        ja .malo
        ; desde aquí se cambia el documento que había
        mov [lam_an], eax
        mov [lam_al], ebx
        mov [lam_pags], ecx
        mov [lam_n], edx
        mov [lam_f_ficha], esi
        mov [lam_dat_n], edi
        movzx eax, word [lam_fila+12]
        mov [lam_margen], eax
        xor ebp, ebp                ; las fichas
.f:
        cmp ebp, [lam_n]
        jae .datos
        mov edi, lam_fila
        mov ecx, [lam_f_ficha]
        call lam_f_lee
        jc .roto
        mov eax, ebp
        call lam_ptr
        mov edi, esi
        mov esi, lam_fila
        mov ecx, LO_TAM
        rep movsb
        inc ebp
        jmp .f
.datos:
        mov eax, [lam_dat_n]
        call lam_dat_pide
        jc .roto
        mov edi, [lam_dat]
        mov ecx, [lam_dat_n]
        call lam_f_lee
        jc .roto
        mov edi, lam_extra          ; lo del programa, si está
        mov ecx, 4
        call lam_f_lee
        jnc .leido
        mov dword [lam_extra], 0
.leido:
        mov ebx, [lam_f_arch]
        HV dos_cerrar
        call lam_repasa
        popad
        clc
        ret
.roto:
        mov eax, 16                 ; a medias: que al menos quede un documento
        mov ebx, 16                 ; entero (quien llama pone el que tenía)
        xor ecx, ecx
        call lam_nueva
.malo:
        mov ebx, [lam_f_arch]
        HV dos_cerrar
        popad
        mov eax, 1
        stc
        ret
.noabre:
        popad
        xor eax, eax
        stc
        ret

; (interna) repasa el documento recién leído: quita los objetos que no se
; entienden o cuyos datos no cuadran y deja lo demás dentro de sus límites
lam_repasa:
        pushad
        mov eax, [lam_margen]
        mov ecx, [lam_an]
        shr ecx, 2
        cmp eax, ecx
        jbe .m
        mov [lam_margen], ecx
.m:
        mov eax, [lam_n]
.b:
        dec eax
        js .x
        call lam_ptr
        movzx ecx, byte [esi+LO_TIPO]
        dec ecx
        cmp ecx, LT_IMAGEN - 1
        ja .quita
        mov ecx, [esi+LO_DAT]       ; sus datos, dentro de los que hay
        cmp ecx, [lam_dat_n]
        ja .quita
        mov edx, [esi+LO_LARGO]
        cmp edx, [lam_dat_n]
        ja .quita
        add ecx, edx
        cmp ecx, [lam_dat_n]
        ja .quita
        cmp byte [esi+LO_TIPO], LT_IMAGEN
        jne .n1
        movzx ecx, word [esi+LO_IMG_AN] ; una imagen: todos sus puntos
        movzx ebx, word [esi+LO_IMG_AL]
        imul ecx, ebx
        test ecx, ecx
        jz .quita
        cmp ecx, edx
        ja .quita
.n1:
        movzx ecx, byte [esi+LO_PAG]
        cmp ecx, [lam_pags]
        jb .n2
        mov ecx, [lam_pags]
        dec ecx
        mov [esi+LO_PAG], cl
.n2:
        cmp byte [esi+LO_ALIN], 2
        jbe .n3
        mov byte [esi+LO_ALIN], 0
.n3:
        cmp byte [esi+LO_CUERPO], TL_TAMANOS
        jb .n4
        mov byte [esi+LO_CUERPO], 0
.n4:
        cmp byte [esi+LO_LETRA], TL_TIPOS
        jb .n5
        mov byte [esi+LO_LETRA], 0
.n5:
        and byte [esi+LO_ESTILO], LE_NEG | LE_CUR | LE_SUB
        lea edi, [esi+LO_X]         ; la posición y el tamaño, sin disparates
        mov ecx, 4
.n6:
        mov edx, [edi]
        cmp edx, 4 * LAM_MAX_LADO
        jle .n7
        mov edx, 4 * LAM_MAX_LADO
.n7:
        cmp edx, -4 * LAM_MAX_LADO
        jge .n8
        mov edx, -4 * LAM_MAX_LADO
.n8:
        mov [edi], edx
        add edi, 4
        dec ecx
        jnz .n6
        cmp byte [esi+LO_TIPO], LT_LINEA
        je .b
        cmp dword [esi+LO_AN], 1    ; lo que no es una línea mide algo
        jge .n9
        mov dword [esi+LO_AN], 1
.n9:
        cmp dword [esi+LO_AL], 1
        jge .b
        mov dword [esi+LO_AL], 1
        jmp .b
.quita:
        ; (sus datos pueden no cuadrar: se le dejan a cero antes de quitarlo)
        mov ecx, [esi+LO_DAT]
        cmp ecx, [lam_dat_n]
        jbe .q1
        mov ecx, [lam_dat_n]
        mov [esi+LO_DAT], ecx
.q1:
        mov edx, [lam_dat_n]
        sub edx, ecx
        cmp [esi+LO_LARGO], edx
        jbe .q2
        mov dword [esi+LO_LARGO], 0
.q2:
        call lam_obj_quita
        jmp .b
.x:
        popad
        ret

; =============================================================================
;  Una imagen de un archivo BMP: cada color, al más parecido de la paleta
; =============================================================================
lam_imagen_bmp:
        push ebx
        push ecx
        push edx
        push edi
        push ebp
        mov [lam_i_pag], cl
        xor al, al
        HV dos_abrir
        jc .noabre
        mov [lam_f_arch], eax
        mov edi, lam_fila
        mov ecx, 54
        call lam_f_lee
        jc .malo
        cmp word [lam_fila], 'BM'
        jne .malo
        cmp dword [lam_fila+14], 40
        jb .malo
        cmp dword [lam_fila+30], 0  ; sin comprimir
        jne .malo
        movzx eax, word [lam_fila+28]
        mov [lam_i_bpp], eax
        cmp eax, 1
        je .bien
        cmp eax, 4
        je .bien
        cmp eax, 8
        je .bien
        cmp eax, 24
        jne .malo
.bien:
        mov eax, [lam_fila+18]
        mov edx, [lam_fila+22]
        mov byte [lam_i_arriba], 0
        test edx, edx
        jns .h
        neg edx                     ; alto negativo: la primera fila, arriba
        mov byte [lam_i_arriba], 1
.h:
        test eax, eax
        jle .malo
        test edx, edx
        jle .malo
        cmp eax, LAM_MAX_LADO
        ja .grande
        cmp edx, LAM_MAX_LADO
        ja .grande
        mov [lam_i_an], eax
        mov [lam_i_al], edx
        imul eax, [lam_i_bpp]       ; octetos de una fila del archivo
        add eax, 31
        shr eax, 5
        shl eax, 2
        mov [lam_i_fila], eax
        mov dword [lam_i_rgb], -1
        cmp dword [lam_i_bpp], 8
        ja .sinpal
        mov ecx, [lam_i_bpp]        ; la paleta del archivo
        mov eax, 1
        shl eax, cl
        mov ecx, [lam_fila+46]
        test ecx, ecx
        jz .p1
        cmp ecx, eax
        jbe .p2
.p1:
        mov ecx, eax
.p2:
        mov [lam_i_ncol], ecx
        mov ebx, [lam_f_arch]
        mov edx, [lam_fila+14]
        add edx, 14
        xor al, al
        HV dos_puntero
        mov edi, lam_fila + 1024
        shl ecx, 2
        call lam_f_lee
        jc .malo
        xor ecx, ecx
.m:
        xor eax, eax
        cmp ecx, [lam_i_ncol]
        jae .m1
        movzx eax, byte [lam_fila+1024+ecx*4+2]
        mov ah, [lam_fila+1024+ecx*4+1]
        movzx edx, byte [lam_fila+1024+ecx*4]
        shl edx, 16
        or eax, edx
        call lam_rgb_ind
.m1:
        mov [lam_i_mapa+ecx], al
        inc ecx
        cmp ecx, 256
        jb .m
.sinpal:
        mov ebx, [lam_f_arch]
        mov edx, [lam_fila+10]
        xor al, al
        HV dos_puntero
        ; el objeto, con sitio para todos sus puntos (al final de lam_dat)
        mov al, LT_IMAGEN
        mov cl, [lam_i_pag]
        call lam_obj_nuevo
        jc .grande
        mov [lam_i_obj], eax
        mov ecx, [lam_i_an]
        mov edx, [lam_i_al]
        mov [esi+LO_AN], ecx
        mov [esi+LO_AL], edx
        mov [esi+LO_IMG_AN], cx
        mov [esi+LO_IMG_AL], dx
        imul ecx, edx
        mov eax, [lam_dat_n]
        add eax, ecx
        cmp eax, LAM_DAT_TAM
        ja .nocabe
        call lam_dat_pide
        jc .nocabe
        mov [esi+LO_LARGO], ecx
        mov [lam_dat_n], eax
        mov ebp, esi                ; EBP=la ficha
        xor edx, edx                ; la fila del archivo
.fila:
        cmp edx, [lam_i_al]
        jae .fin
        mov edi, lam_fila
        mov ecx, [lam_i_fila]
        call lam_f_lee
        jc .corto
        mov eax, edx
        cmp byte [lam_i_arriba], 0
        jne .y
        mov eax, [lam_i_al]
        dec eax
        sub eax, edx
.y:
        imul eax, [lam_i_an]
        add eax, [ebp+LO_DAT]
        mov edi, [lam_dat]
        add edi, eax
        mov esi, lam_fila
        mov ecx, [lam_i_an]
        push edx
        mov eax, [lam_i_bpp]
        cmp eax, 8
        je .b8
        cmp eax, 24
        je .b24
        cmp eax, 4
        je .b4
        xor edx, edx                ; 1 bit: el de más peso, a la izquierda
.b1:
        test edx, 7
        jnz .b1n
        mov ah, [esi]
        inc esi
.b1n:
        xor ebx, ebx
        shl ah, 1
        adc ebx, 0
        mov al, [lam_i_mapa+ebx]
        stosb
        inc edx
        dec ecx
        jnz .b1
        jmp .sigf
.b4:
        xor edx, edx                ; 4 bits: primero los cuatro altos
.b4l:
        test edx, 1
        jnz .b4n
        mov ah, [esi]
        inc esi
        movzx ebx, ah
        shr ebx, 4
        jmp .b4p
.b4n:
        movzx ebx, ah
        and ebx, 15
.b4p:
        mov al, [lam_i_mapa+ebx]
        stosb
        inc edx
        dec ecx
        jnz .b4l
        jmp .sigf
.b8:
        movzx ebx, byte [esi]
        inc esi
        mov al, [lam_i_mapa+ebx]
        stosb
        dec ecx
        jnz .b8
        jmp .sigf
.b24:
        movzx eax, byte [esi+2]     ; azul, verde, rojo
        mov ah, [esi+1]
        movzx edx, byte [esi]
        shl edx, 16
        or eax, edx
        add esi, 3
        call lam_rgb_ind
        stosb
        dec ecx
        jnz .b24
.sigf:
        pop edx
        inc edx
        jmp .fila
.corto:
        ; el archivo acaba antes de tiempo: lo que falta, en blanco
        mov eax, edx
.c1:
        cmp eax, [lam_i_al]
        jae .fin
        push eax
        cmp byte [lam_i_arriba], 0
        jne .c2
        neg eax
        add eax, [lam_i_al]
        dec eax
.c2:
        imul eax, [lam_i_an]
        add eax, [ebp+LO_DAT]
        mov edi, [lam_dat]
        add edi, eax
        mov ecx, [lam_i_an]
        mov al, C_BLANCO
        rep stosb
        pop eax
        inc eax
        jmp .c1
.fin:
        mov ebx, [lam_f_arch]
        HV dos_cerrar
        mov esi, ebp
        mov eax, [lam_i_obj]
        clc
        jmp .x
.nocabe:
        dec dword [lam_n]
.grande:
        mov ebx, [lam_f_arch]
        HV dos_cerrar
        mov eax, 2
        stc
        jmp .x
.malo:
        mov ebx, [lam_f_arch]
        HV dos_cerrar
        mov eax, 1
        stc
        jmp .x
.noabre:
        xor eax, eax
        stc
.x:
        pop ebp
        pop edi
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) EAX=color (rojo en el octeto bajo, verde, azul) -> EAX=el más
; parecido de la paleta: uno de los 16 propios si es igual; si no, del cubo
; de 6x6x6 (o de los grises, si es un gris y hay uno más cerca)
lam_rgb_ind:
        cmp eax, [lam_i_rgb]
        jne .b
        mov eax, [lam_i_ind]
        ret
.b:
        push ebx
        push ecx
        push edx
        push esi
        mov [lam_i_rgb], eax
        mov esi, [hv_api + HV_pal_rgb * 4]
        xor ecx, ecx
.p:
        cmp eax, [esi+ecx*4]
        je .hecho
        inc ecx
        cmp ecx, 16
        jb .p
        movzx ebx, al               ; rojo
        movzx ecx, ah               ; verde
        mov edx, eax
        shr edx, 16                 ; azul
        cmp ebx, ecx
        jne .cubo
        cmp ebx, edx
        jne .cubo
        lea eax, [ebx+5]            ; un gris: el de la escala (de 11 en 11)...
        xor edx, edx
        mov ecx, 11
        div ecx
        cmp eax, 23
        jbe .g1
        mov eax, 23
.g1:
        imul ecx, eax, 11
        sub ecx, ebx
        jns .g2
        neg ecx
.g2:
        push eax
        lea eax, [ebx+25]           ; ...o el del cubo (de 51 en 51)
        xor edx, edx
        mov esi, 51
        div esi
        imul edx, eax, 51
        sub edx, ebx
        jns .g3
        neg edx
.g3:
        cmp edx, ecx
        jae .rampa
        add esp, 4
        imul ecx, eax, 43
        add ecx, 16
        jmp .hecho
.rampa:
        pop ecx
        add ecx, 232
        jmp .hecho
.cubo:
        push edx
        mov esi, 51
        lea eax, [ebx+25]
        xor edx, edx
        div esi
        imul ebx, eax, 36
        lea eax, [ecx+25]
        xor edx, edx
        div esi
        imul eax, eax, 6
        add ebx, eax
        pop eax
        add eax, 25
        xor edx, edx
        div esi
        lea ecx, [ebx+eax+16]
.hecho:
        mov eax, ecx
        mov [lam_i_ind], eax
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
