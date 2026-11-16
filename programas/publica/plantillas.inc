; =============================================================================
;  plantillas.inc - las plantillas de Publicaciones (Archivo > Nuevo): cartel,
;  panfleto, tarjeta de visita y anuncio con tiras para arrancar
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Una plantilla es una tabla de objetos que plantilla_pon (publica.asm) pone
;  en la primera página de un documento recién hecho (la macro PL y
;  plantilla_pon son del editor de láminas, programas/comun/lamina_ed.inc,
;  que explica qué lleva cada objeto: las medidas van en milésimas de la
;  página, así que valen para cualquier papel). Para añadir una plantilla: su
;  tabla aquí, y su nombre y su tabla en t_plantillas y plantillas
;  (publica.asm).
; =============================================================================
pl_blanco:
        db 0
pl_cartel:
        PL LT_RECT, 0, 0, 1000, 230, 0, 0, 0, C_PETROLEO, N, 1, 0, 0, 0, 0
        PL LT_TEXTO, 40, 45, 920, 110, 0, 1, C_BLANCO, N, N, 1, 10, 5, 4, pt_c_tit
        PL LT_TEXTO, 40, 150, 920, 70, 0, 1, C_OCRE, N, N, 1, 18, 4, 3, pt_c_sub
        PL LT_ELIPSE, 300, 290, 400, 220, 0, 0, 0, C_OCRE, C_TERRACOTA, 4, 0, 0, 0, 0
        PL LT_TEXTO, 300, 355, 400, 90, 0, 1, C_GRANATE, N, N, 1, 18, 5, 4, pt_c_ven
        PL LT_TEXTO, 80, 560, 840, 220, 0, 1, C_TINTA, N, N, 1, 9, 3, 2, pt_c_txt
        PL LT_RECT, 0, 840, 1000, 160, 0, 0, 0, C_TERRACOTA, N, 1, 0, 0, 0, 0
        PL LT_TEXTO, 40, 862, 920, 120, 0, 1, C_BLANCO, N, N, 1, 10, 3, 2, pt_c_pie
        db 0
pt_c_tit    db "GRAN FIESTA", 0
pt_c_sub    db "de la primavera", 0
pt_c_ven    db "¡Ven!", 0
pt_c_txt    db "Música, baile y merienda para todos.", 10
            db "Trae a tu familia y a tus amigos.", 0
pt_c_pie    db "Sábado 12 de junio, a las 18 h", 10, "Plaza Mayor", 0

pl_panfleto:
        PL LT_RECT, 0, 0, 1000, 150, 0, 0, 0, C_PETROLEO, N, 1, 0, 0, 0, 0
        PL LT_TEXTO, 60, 35, 880, 100, 0, 1, C_BLANCO, N, N, 1, 2, 4, 3, pt_p_tit
        PL LT_LINEA, 0, 152, 1000, 0, 0, 0, 0, N, C_OCRE, 4, 0, 0, 0, 0
        PL LT_TEXTO, 60, 175, 880, 80, LE_CUR, 1, C_TINTA, N, N, 1, 1, 2, 0, pt_p_frase
        ; tres bloques iguales: el fondo, una raya de color, el título y el texto
%macro PL_BLOQUE 3
        PL LT_RECT, 60, %1, 880, 205, 0, 0, 0, C_CREMA, C_ARENA_OSC, 1, 0, 0, 0, 0
        PL LT_RECT, 60, %1, 14, 205, 0, 0, 0, %2, N, 1, 0, 0, 0, 0
        PL LT_TEXTO, 95, %1 + 10, 825, 45, 0, 0, C_GRANATE, N, N, 1, 10, 2, 0, %3
        PL LT_TEXTO, 95, %1 + 60, 825, 135, 0, 0, C_TINTA, N, N, 1, 9, 0, 1, pt_p_txt
%endmacro
        PL_BLOQUE 270, C_TERRACOTA, pt_p_b1
        PL_BLOQUE 500, C_OCRE, pt_p_b2
        PL_BLOQUE 730, C_OLIVA, pt_p_b3
        db 0
pt_p_tit    db "Título del panfleto", 0
pt_p_frase  db "Una frase que diga de qué va, en una línea o dos.", 0
pt_p_b1     db "Primer bloque", 0
pt_p_b2     db "Segundo bloque", 0
pt_p_b3     db "Tercer bloque", 0
pt_p_txt    db "Escriba aquí el texto de este bloque. Haga doble clic en la caja "
            db "para cambiarlo.", 0

pl_tarjeta:
        PL LT_RECT, 0, 0, 90, 1000, 0, 0, 0, C_PETROLEO, N, 1, 0, 0, 0, 0
        PL LT_RECT, 90, 0, 16, 1000, 0, 0, 0, C_OCRE, N, 1, 0, 0, 0, 0
        PL LT_TEXTO, 140, 80, 830, 170, 0, 0, C_PETROLEO, N, N, 1, 10, 2, 2, pt_t_nom
        PL LT_TEXTO, 140, 250, 830, 130, LE_CUR, 0, C_TERRACOTA, N, N, 1, 1, 0, 0, pt_t_ofi
        PL LT_LINEA, 150, 440, 800, 0, 0, 0, 0, N, C_ARENA_OSC, 1, 0, 0, 0, 0
        PL LT_TEXTO, 140, 490, 830, 440, 0, 0, C_TINTA, N, N, 1, 9, 1, 1, pt_t_dat
        db 0
pt_t_nom    db "Nombre Apellidos", 0
pt_t_ofi    db "Profesión u oficio", 0
pt_t_dat    db "Calle Mayor, 1 - 43000 Ciudad", 10
            db "Tel. 600 000 000", 10
            db "correo@ejemplo.es", 0

pl_anuncio:
        PL LT_TEXTO, 60, 40, 880, 120, 0, 1, C_TINTA, C_OCRE, C_TINTA, 3, 10, 5, 4, pt_a_tit
        PL LT_TEXTO, 60, 210, 880, 300, 0, 1, C_TINTA, N, N, 1, 9, 3, 2, pt_a_txt
        PL LT_TEXTO, 60, 590, 880, 60, LE_CUR, 1, C_MARRON, N, N, 1, 1, 0, 1, pt_a_arr
        PL LT_LINEA, 0, 690, 1000, 0, 0, 0, 0, N, C_ARENA_OSC, 1, 0, 0, 0, 0
%assign i 0
%rep 8
        PL LT_TEXTO, i * 125, 700, 125, 300, 0, 1, C_TINTA, N, C_ARENA_OSC, 1, 9, 0, 1, pt_a_tel
%assign i i + 1
%endrep
        db 0
pt_a_tit    db "SE ALQUILA", 0
pt_a_txt    db "Piso de tres habitaciones, céntrico y con mucha luz.", 10
            db "Razón: en este mismo portal.", 0
pt_a_arr    db "Arranque un papel con el teléfono:", 0
pt_a_tel    db "Tel.", 10, "600", 10, "000", 10, "000", 0
