; =============================================================================
;  PUBLICA.HVP - Publicaciones, el programa de panfletos, carteles, tarjetas
;  y folletos de Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Autoedición sencilla: una página (A4 o A5, de pie o apaisada, o una
;  tarjeta) y, sobre ella, objetos que se colocan libremente: cajas de texto
;  (el texto se parte solo por las palabras; cada caja lleva su tipo de
;  letra, tamaño, negrita, cursiva, subrayado, color, alineación, fondo y
;  borde), rectángulos, elipses, líneas e imágenes BMP. Un documento puede
;  tener varias páginas.
;
;  Se maneja con el ratón: un clic elige un objeto (salen sus tiradores),
;  arrastrando se mueve y por los tiradores cambia de tamaño; con una
;  herramienta puesta (Texto, Rectángulo, Elipse, Línea) se arrastra sobre la
;  página para poner uno nuevo; un doble clic en una caja de texto (o Intro, o
;  ponerse a escribir) deja escribir dentro. Las flechas mueven lo elegido,
;  Supr lo borra, Tab elige el siguiente. Se puede duplicar, traer al frente
;  o enviar al fondo, cortar, copiar y pegar (el texto de una caja va además
;  al portapapeles de Hispa Ventanas, y al escribir se pega el que haya),
;  deshacer y rehacer el último cambio, y ajustar a una rejilla de 5 mm.
;
;  Archivo: Nuevo (con plantillas: cartel, panfleto, tarjeta de visita,
;  anuncio con tiras), Abrir, Guardar y Guardar como (.PUB), Exportar la
;  página como BMP e Imprimir en una impresora matricial (ESC/P, imagen de
;  bits de 8 agujas) por el puerto paralelo (INT 17h), página a página.
;
;  Cómo está hecho. El documento y su dibujo son de programas/comun/lamina.inc
;  (el motor) y el manejo (la vista, el ratón, el teclado, el formato,
;  deshacer, el portapapeles, los archivos, la impresora), de
;  programas/comun/lamina_ed.inc (el editor de láminas): los dos los comparte
;  con Presentaciones. Aquí queda lo que es solo de Publicaciones: el menú,
;  la fila de botones de arriba, los papeles, «Nuevo» y sus plantillas
;  (programas/publica/plantillas.inc) y cómo se reparte la ventana. La
;  página se dibuja al 100 % en lam_buf solo cuando cambia; para verla a otro
;  tamaño se reduce a esc_buf. Lo que va por encima (márgenes, rejilla,
;  tiradores, el contorno de lo que se arrastra, el cursor de escribir) se
;  pinta en la ventana cada vez, así que arrastrar no redibuja la página.
;  Las medidas son puntos de lámina (96 por pulgada: un punto de pantalla al
;  100 %); abajo se dicen en milímetros.
;
;  Parámetros del acceso directo: el nombre de un documento, que se abre.
; =============================================================================
%include "hvp.inc"

; órdenes propias
O_PAPEL     equ 200

HVP_CABECERA "Publicaciones", entrada, 4
        db "................................"
        db "....0000000000000000000........."
        db "....09999999999999999900........"
        db "....099999999999999999090......."
        db "....0944444444444444499000......"
        db "....09444444444444444999990....."
        db "....09449994994999444999990....."
        db "....09444444444444444999990....."
        db "....09444444444444444999990....."
        db "....09999999999999999999990....."
        db "....09999999999999999999990....."
        db "....09999aaaaa9999222222990....."
        db "....0999aaaaaaa999999999990....."
        db "....099aaaaaaaaa99222222990....."
        db "....099aaaaaaaaa99999999990....."
        db "....099aaaaaaaaa99222222990....."
        db "....099aaaaaaaaa99999999990....."
        db "....0999aaaaaaa999222299990....."
        db "....09999aaaaa9999999999990....."
        db "....09999999999999999999990....."
        db "....09977777777777777779990....."
        db "....09999999999999999999990....."
        db "....09977777777777777779990....."
        db "....09999999999999999999990....."
        db "....09977777777777779999990....."
        db "....09999999999999999999990....."
        db "....09090909090909090909090....."
        db "....09c09c09c09c09c09c09c90....."
        db "....09c09c09c09c09c09c09c90....."
        db "....09909909909909909909990....."
        db "....00000000000000000000000....."
        db "................................"
HVP_CODIGO

c_publica   dd ed_proc
            db SIN_FONDO, 0, 0, 0   ; lo pinta todo ella: sin parpadeo
pub_plant:
        PLANTILLA c_publica, titulo, E_NORMAL, 8, 30, 784, 562, 0

menu_prog:
        db "&Archivo", 0
        dw O_NUEVO
        db "&Nuevo...               Ctrl+U", 0
        dw O_ABRIR
        db "&Abrir...               Ctrl+A", 0
        dw 0
        db 0
        dw O_GUARDAR
        db "&Guardar                Ctrl+G", 0
        dw O_COMO
        db "Guardar c&omo...", 0
        dw O_EXPORTAR
        db "&Exportar la página como BMP...", 0
        dw 0
        db 0
        dw O_IMPRIMIR
        db "&Imprimir               Ctrl+P", 0
        dw O_BORRADOR
        db "Imprimir en &borrador (rápido)", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw O_DESHACER
        db "&Deshacer              Ctrl+Z", 0
        dw O_REHACER
        db "&Rehacer               Ctrl+Y", 0
        dw 0
        db 0
        dw O_CORTAR
        db "Cor&tar                Ctrl+X", 0
        dw O_COPIAR
        db "&Copiar                Ctrl+C", 0
        dw O_PEGAR
        db "&Pegar                 Ctrl+V", 0
        dw O_BORRAR
        db "&Borrar el objeto        Supr", 0
        dw O_DUPLICAR
        db "D&uplicar              Ctrl+R", 0
        dw 0
        db 0
        dw O_ESCRIBIR
        db "&Escribir en la caja    Intro", 0
        dw 0
        db 0
        dw O_PAG_QUITA
        db "&Quitar esta página", 0
        dw 0xFFFF
        db "&Ver", 0
        dw O_VISTA
        db "&Página entera", 0
        dw O_VISTA + 1
        db "Al &50 %", 0
        dw O_VISTA + 2
        db "Al &75 %", 0
        dw O_VISTA + 3
        db "Al &100 % (tamaño real)", 0
        dw 0
        db 0
        dw O_MARGENES
        db "Ver los &márgenes", 0
        dw O_REJILLA
        db "Ver la &rejilla", 0
        dw 0
        db 0
        dw O_PAG_ANT
        db "Página &anterior        RePág", 0
        dw O_PAG_SIG
        db "Página &siguiente       AvPág", 0
        dw 0xFFFF
        db "&Insertar", 0
        dw O_HERR + 1
        db "Caja de &texto", 0
        dw O_HERR + 2
        db "&Rectángulo", 0
        dw O_HERR + 3
        db "&Elipse", 0
        dw O_HERR + 4
        db "&Línea", 0
        dw O_IMAGEN
        db "&Imagen (BMP)...", 0
        dw 0
        db 0
        dw O_PAG_NUEVA
        db "&Página nueva", 0
        dw 0xFFFF
        db "&Formato", 0
        dw O_TIPO
        db "&Tipo de letra...", 0
        dw O_TAM
        db "Ta&maño de letra...", 0
        dw O_NEG
        db "&Negrita               Ctrl+N", 0
        dw O_CUR
        db "&Cursiva               Ctrl+K", 0
        dw O_SUB
        db "&Subrayado             Ctrl+S", 0
        dw O_GRANDE
        db "Letra &grande          Ctrl+W", 0
        dw 0
        db 0
        dw O_IZQ
        db "A la &izquierda        Ctrl+Q", 0
        dw O_CEN
        db "C&entrado              Ctrl+T", 0
        dw O_DER
        db "A la &derecha          Ctrl+D", 0
        dw 0
        db 0
        dw O_C_LETRA
        db "Color de la &letra...", 0
        dw O_C_FONDO
        db "Color del &relleno...", 0
        dw O_C_BORDE
        db "Color del &borde o de la línea...", 0
        dw O_GROSOR
        db "Gr&osor del borde o de la línea...", 0
        dw 0
        db 0
        dw O_VINETAS
        db "&Viñetas (una lista en la caja)", 0
        dw 0
        db 0
        dw O_FRENTE
        db "Traer al &frente       Ctrl+F", 0
        dw O_ALFONDO
        db "Enviar al fond&o       Ctrl+O", 0
        dw 0
        db 0
        dw O_PAPEL
        db "Tamaño de la &página...", 0
        dw 0xFFFF
        db "&Herramientas", 0
        dw O_AJUSTAR
        db "&Ajustar a la rejilla (5 mm): poner o quitar", 0
        dw 0xFFFF
        db "&?", 0
        dw O_ACERCA
        db "&Acerca de Publicaciones...", 0
        dw 0xFFFF
        db 0

; ---- diálogo: una publicación nueva (la plantilla y el papel) ----
dlg_nuevo:
        PLANTILLA HV_c_dialogo, t_d_nuevo, E_DIALOGO, 0, 0, 440, 236, 0
        PLANTILLA HV_c_estatico, t_n_plant, E_HIJA, 16, 10, 200, 18, 100
        PLANTILLA c_lista2, t_nada, E_HIJA | E_TABULA, 16, 30, 200, 116, 60
        PLANTILLA HV_c_estatico, t_n_papel, E_HIJA, 224, 10, 200, 18, 101
        PLANTILLA c_lista2, t_nada, E_HIJA | E_TABULA, 224, 30, 200, 116, 61
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 124, 160, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 224, 160, 90, 26, ID_CANCELAR
        dd 0

; ---- la fila de botones de arriba: x, ancho, dibujo, orden, nombre, clase
;      (la de abajo, la del formato, es la común: lamina_ed.inc) ----
            align 4
botones1    dd 6, 24, ic_nuevo, O_NUEVO, n_nuevo, BT_ICONO
            dd 32, 24, ic_abrir, O_ABRIR, n_abrir, BT_ICONO
            dd 58, 24, ic_guardar, O_GUARDAR, n_guardar, BT_ICONO
            dd 84, 24, ic_imprimir, O_IMPRIMIR, n_imprimir, BT_ICONO
            dd 116, 24, ic_cortar, O_CORTAR, n_cortar, BT_ICONO
            dd 142, 24, ic_copiar, O_COPIAR, n_copiar, BT_ICONO
            dd 168, 24, ic_pegar, O_PEGAR, n_pegar, BT_ICONO
            dd 200, 24, ic_deshacer, O_DESHACER, n_deshacer, BT_ICONO
            dd 226, 24, ic_rehacer, O_REHACER, n_rehacer, BT_ICONO
            dd 258, 24, ic_elegir, O_HERR, n_elegir, BT_ICONO
            dd 284, 24, ic_caja, O_HERR + 1, n_caja, BT_ICONO
            dd 310, 24, ic_rect, O_HERR + 2, n_rect, BT_ICONO
            dd 336, 24, ic_elipse, O_HERR + 3, n_elipse, BT_ICONO
            dd 362, 24, ic_linea, O_HERR + 4, n_linea, BT_ICONO
            dd 388, 24, ic_imagen, O_IMAGEN, n_imagen, BT_ICONO
            dd 420, 24, ic_pag_ant, O_PAG_ANT, n_pag_ant, BT_ICONO
            dd 446, 24, ic_pag_sig, O_PAG_SIG, n_pag_sig, BT_ICONO
            dd 472, 24, ic_mas, O_PAG_NUEVA, n_pag_nueva, BT_ICONO
            dd 504, 60, t_nada, O_CAMBIA_V, n_vista, BT_TEXTO
            dd 0, 0, 0, 0, 0, 0
n_nuevo     db "Nuevo: otra publicación, en blanco o de una plantilla (Ctrl+U)", 0
n_abrir     db "Abrir una publicación guardada (Ctrl+A)", 0
n_guardar   db "Guardar la publicación (Ctrl+G)", 0
n_caja      db "Caja de texto: arrastre sobre la página y escriba", 0
n_rect      db "Rectángulo: arrastre sobre la página", 0
n_elipse    db "Elipse: arrastre sobre la página", 0
n_linea     db "Línea: arrastre sobre la página", 0
n_pag_ant   db "Página anterior (RePág)", 0
n_pag_sig   db "Página siguiente (AvPág)", 0
n_pag_nueva db "Añadir una página detrás de esta", 0

; ---- los papeles: ancho, alto y margen (en puntos de lámina) y su nombre ----
            align 4
papeles     dd 794, 1123, 38, t_p_a4v
            dd 1123, 794, 38, t_p_a4h
            dd 559, 794, 38, t_p_a5v
            dd 794, 559, 38, t_p_a5h
            dd 321, 208, 19, t_p_tarj
N_PAPELES   equ 5
P_TARJETA   equ 4
t_p_a4v     db "A4 vertical", 0
t_p_a4h     db "A4 horizontal", 0
t_p_a5v     db "A5 vertical", 0
t_p_a5h     db "A5 horizontal", 0
t_p_tarj    db "Tarjeta (85 x 55 mm)", 0
t_p_medida  db "A medida", 0
; las órdenes de Ctrl+letra (de la A a la Z; 0: ninguna). Ctrl+H y Ctrl+M no
; valen: para el teclado son Retroceso e Intro
ctrl_ord    db 0, O_ABRIR, 0, O_COPIAR, O_DER, 0, O_FRENTE, O_GUARDAR, 0, 0, 0
            db O_CUR, 0, 0, O_NEG, O_ALFONDO, O_IMPRIMIR, O_IZQ, O_DUPLICAR, O_SUB, O_CEN
            db O_NUEVO, O_PEGAR, O_GRANDE, O_CORTAR, O_REHACER, O_DESHACER
; las órdenes propias (las demás son las del editor de láminas)
            align 4
ordenes_prog dd O_PAPEL, papel_elige, 0

; ---- textos (los que el editor de láminas pide a cada programa, y los propios) ----
t_programa  db "Publicaciones", 0
t_ext       db ".PUB", 0
t_pagina    db "Página ", 0
t_arrastre  db ": arrastre sobre la página (o haga un clic en ella)", 0
t_exportar  db "Exportar la página como BMP", 0
t_cambios   db "La publicación tiene cambios sin guardar.", 10, "¿Quiere guardarlos?", 0
t_no_valido db "Ese archivo no es una publicación de Publicaciones (.PUB).", 0
t_no_guardar db "No se ha podido guardar la publicación.", 10
            db "Compruebe que el disco no está lleno ni protegido.", 0
t_exportado db "La página se ha guardado como imagen BMP.", 0
t_lleno     db "No caben más objetos en la publicación (200).", 0
t_sin_sitio db "No cabe: la publicación ha llegado al límite de texto e imágenes.", 0
t_max_pag   db "La publicación ya tiene todas las páginas que caben (32).", 0
t_unica     db "Es la única página: no se puede quitar.", 0
t_q_quitar  db "Se va a quitar esta página con todo lo que tiene.", 0
t_bmp_grande db "La imagen es demasiado grande para la publicación.", 0
t_impreso   db "La publicación se ha enviado a la impresora.", 0
t_d_papel   db "Tamaño de la página", 0
t_d_nuevo   db "Publicación nueva", 0
t_n_plant   db "Plantilla:", 0
t_n_papel   db "Papel:", 0
t_pl0       db "En blanco", 0
t_pl1       db "Cartel", 0
t_pl2       db "Panfleto", 0
t_pl3       db "Tarjeta de visita", 0
t_pl4       db "Anuncio con tiras", 0
            align 4
t_plantillas dd t_pl0, t_pl1, t_pl2, t_pl3, t_pl4
plantillas  dd pl_blanco, pl_cartel, pl_panfleto, pl_tarjeta, pl_anuncio
N_PLANT     equ 5
PL_TARJETA  equ 3
t_acerca_tit db "Acerca de Publicaciones", 0
t_acerca    db "Publicaciones", 10
            db "Panfletos, carteles, tarjetas y folletos:", 10
            db "cajas de texto, formas, líneas e imágenes", 10
            db "que se colocan libremente sobre la página.", 10, 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores.", 10
            db "GPL-2.0 o posterior.", 0
firma       db "PUBLICA", 26        ; el principio de un .PUB (8 octetos)


%include "tipos.inc"
%include "lamina.inc"
%include "lamina_ed.inc"
%include "plantillas.inc"          ; las plantillas de «Nuevo» (programas/publica/)

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        call ed_inicia              ; el editor de láminas (ESI, EDI: lo que llega)
        mov esi, dlg_nuevo
        call hvl_clases
        xor eax, eax                ; un A4 de pie, en blanco
        call doc_nuevo
        HV pantalla                 ; EAX=ancho EBX=alto: que quepa
        sub ebx, BARRA_H + 8
        cmp ebx, 562
        jae .a
        mov [pub_plant+P_AL], ebx
.a:
        sub eax, 16
        cmp eax, 784
        jae .b
        mov [pub_plant+P_AN], eax
.b:
        mov esi, pub_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        mov esi, [param]
        call nombre_param
        jc .bucle
        call cargar
.bucle:
        HV app_bucle
.x:
        ret

; EAX=papel (0 a N_PAPELES-1): un documento vacío de ese papel, sin nombre
doc_nuevo:
        pushad
        shl eax, 4
        mov ecx, [papeles+eax+8]
        mov ebx, [papeles+eax+4]
        mov eax, [papeles+eax]
        call lam_nueva
        mov byte [nombre], 0
        call doc_estrena
        popad
        ret
; WM_PAINT: la barra, el pie y la vista de la página
pintar:
        HV dc_pintar
        call pinta_botones
        call pinta_estado
        call pinta_vista
        HV dc_cerrar
        jmp hecho

; dónde va la vista: toda la zona, menos la barra de arriba, el pie y las
; barras de desplazamiento
medidas:
        mov dword [vx0], 0
        mov dword [vy0], TB_AL
        mov eax, [cli_an]
        sub eax, BD_AN
        jns .a
        xor eax, eax
.a:
        mov [vw], eax
        mov eax, [cli_al]
        sub eax, TB_AL + BR_PIE + BD_AN
        jns .b
        xor eax, eax
.b:
        mov [vh], eax
        ret

; los botones de arriba no tienen ninguno que esté «puesto» fuera de los comunes
barra_propia:
        ret

; el pie, sin nada elegido: el papel y sus medidas
pie_papel:
        call papel_de               ; el papel, sus medidas y la vista
        mov esi, t_p_medida
        cmp eax, N_PAPELES
        jae .p1
        call papel_texto
.p1:
        cmp eax, P_TARJETA << 4     ; (la tarjeta ya dice lo que mide)
        je .p2
        call hvl_cad
        mov esi, t_coma
        call hvl_cad
        mov eax, [lam_an]
        call num_mm
        mov esi, t_por
        call hvl_cad
        mov eax, [lam_al]
        call num_mm
        mov esi, t_mm
.p2:
        jmp hvl_cad

; ---- Formato > Tamaño de la página: otro papel; los objetos se quedan
;      donde están ----
papel_elige:
        mov dword [le_titulo], t_d_papel
        mov dword [le_n], N_PAPELES
        mov dword [le_texto], papel_texto
        call papel_de
        mov [le_sel], eax
        call lista_elige
        jne .x
        call fin_edicion
        call instantanea
        mov eax, [le_sel]
        shl eax, 4
        mov ecx, [papeles+eax]
        mov [lam_an], ecx
        mov ecx, [papeles+eax+4]
        mov [lam_al], ecx
        mov ecx, [papeles+eax+8]
        mov [lam_margen], ecx
        mov dword [sx], 0
        mov dword [sy], 0
        mov byte [esc_sucio], 1
        call calc_vista
        call cambio
.x:
        ret
papel_texto:
        shl eax, 4
        mov esi, [papeles+eax+12]
        ret
; -> EAX=el papel del documento (N_PAPELES: ninguno de la lista)
papel_de:
        push ebx
        xor eax, eax
.b:
        mov ebx, eax
        shl ebx, 4
        push ecx
        mov ecx, [papeles+ebx]
        cmp ecx, [lam_an]
        jne .s
        mov ecx, [papeles+ebx+4]
        cmp ecx, [lam_al]
.s:
        pop ecx
        je .x
        inc eax
        cmp eax, N_PAPELES
        jb .b
.x:
        pop ebx
        ret

; Nuevo: se elige la plantilla y el papel
nuevo:
        call preguntar
        jc .x
        mov ebp, [ven]
        mov esi, dlg_nuevo
        mov edi, dlg_nuevo + P_TAM
        mov ebx, nuevo_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        mov eax, [dl_papel]
        cmp dword [dl_plant], PL_TARJETA    ; la tarjeta de visita, en su papel
        jne .p
        mov eax, P_TARJETA
.p:
        call doc_nuevo
        mov eax, [dl_plant]
        mov esi, [plantillas+eax*4]
        xor ecx, ecx
        call plantilla_pon
        call sucio
.x:
        ret
nuevo_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp cx, 60                  ; una de las listas: doble clic o Intro
        je .lista
        cmp cx, 61
        jne .boton
.lista:
        shr ecx, 16
        cmp ecx, LN_DOBLE
        jne .no
        mov eax, WM_COMMAND
        mov ecx, ID_ACEPTAR
        xor edx, edx
        HV msg_enviar
        jmp .no
.boton:
        cmp ecx, ID_ACEPTAR
        jne .no
        pushad
        mov ecx, 60
        mov edi, dl_plant
        call .lee
        mov ecx, 61
        mov edi, dl_papel
        call .lee
        popad
        jmp .no
.ini:
        pushad
        mov dword [dl_plant], 0
        mov dword [dl_papel], 0
        mov ecx, 60
        mov esi, t_plantillas
        mov edi, N_PLANT
        call .llena
        mov ecx, 61
        mov esi, papeles + 12
        mov edi, N_PAPELES
        mov ebx, 16
        call .llena2
        popad
.no:
        xor eax, eax
        ret
; (internas) la lista ECX del diálogo EBP: su elegido, a [EDI]; y llenarla
; con EDI textos cuyos punteros están en ESI (cada EBX octetos)
.lee:
        push ebp
        HV ven_hija_id
        mov ebp, eax
        mov eax, LM_SELEC
        HV msg_enviar
        pop ebp
        test eax, eax
        js .l1
        mov [edi], eax
.l1:
        ret
.llena:
        mov ebx, 4
.llena2:
        push ebp
        HV ven_hija_id
        mov ebp, eax
.ll1:
        mov edx, [esi]
        mov eax, LM_ANADIR
        HV msg_enviar
        add esi, ebx
        dec edi
        jnz .ll1
        mov eax, LM_PONER_SELEC
        xor ecx, ecx
        HV msg_enviar
        pop ebp
        ret

%include "barra.inc"
%include "hvplib.inc"

        section .bss
            TIPOS_BSS
            LAMINA_BSS
            LAMINA_ED_BSS
            BARRA_BSS
HVP_FIN
