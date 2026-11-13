; =============================================================================
;  Hispa Ventanas - datos del aspecto: paleta, cursor, mosaico y textos
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;  Todo diseñado aquí.
; =============================================================================

; ---- los 16 colores de Hispa Ventanas (rojo, verde, azul) ----
; (los nombres C_TINTA ... C_PIEDRA están en defs.inc)
paleta16:
        db 0x1A, 0x14, 0x10         ;  0 tinta
        db 0x3B, 0x2A, 0x20         ;  1 marrón
        db 0x1F, 0x4E, 0x5A         ;  2 azul petróleo
        db 0x2C, 0x66, 0x75         ;  3 azul petróleo claro
        db 0xC2, 0x54, 0x2D         ;  4 terracota
        db 0xE0, 0x7B, 0x4F         ;  5 terracota claro
        db 0xC9, 0xB7, 0x9C         ;  6 arena
        db 0x8F, 0x7F, 0x68         ;  7 arena oscura
        db 0xFB, 0xF3, 0xE0         ;  8 crema
        db 0xFF, 0xFF, 0xFF         ;  9 blanco
        db 0xE8, 0xB3, 0x3A         ; 10 ocre
        db 0x6B, 0x8E, 0x3D         ; 11 verde oliva
        db 0x2F, 0x4B, 0x8F         ; 12 añil
        db 0x9E, 0x2B, 0x2B         ; 13 granate
        db 0x7F, 0xC6, 0xBF         ; 14 turquesa
        db 0xD8, 0xD2, 0xC4         ; 15 piedra

; ---- el cursor: X tinta, o relleno (ocre), . transparente ----
cursor_dib:
        db "X..........."
        db "XX.........."
        db "XoX........."
        db "XooX........"
        db "XoooX......."
        db "XooooX......"
        db "XoooooX....."
        db "XooooooX...."
        db "XoooooooX..."
        db "XooooooooX.."
        db "XoooooooooX."
        db "XooooooXXXXX"
        db "XoooXooX...."
        db "XooX.XooX..."
        db "XoX..XooX..."
        db "XX....XooX.."
        db "X.....XooX.."
        db ".......XX..."

; ---- el mosaico del fondo (16x16): un azulejo de rombos ----
mosaico_dib:
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

; ---- textos ----
tit_barra       db "Hispa Ventanas", 0
tit_salir       db "Salir", 0
tit_bienvenida  db "Bienvenida", 0
tit_paleta      db "Paleta y dibujo", 0
tit_sistema     db "Sistema", 0

; líneas: x, y (dentro de la ventana), color, negrita, texto
lin_bienvenida:
        db 14, 12, C_TERRACOTA, 1, "¡Bienvenido a Hispa Ventanas!", 0
        db 14, 38, C_TINTA, 0, "Un entorno gráfico propio sobre Hispa-DOS,", 0
        db 14, 56, C_TINTA, 0, "hecho desde cero y en español.", 0
        db 14, 84, C_ANIL, 0, "Arrastre una ventana por su barra de título.", 0
        db 14, 104, C_ANIL, 0, "Tab: otra ventana · Flechas: mover · Esc: salir", 0
        db 0xFF
lin_sistema:
        db 14, 96, C_GRANATE, 0, "áéíóú ü ñ Ñ ¿? ¡! ç Ç « » º ª", 0
        db 0xFF

s_pantalla      db "Pantalla: ", 0
s_por           db " x ", 0
s_c16           db ", 16 colores", 0
s_c256          db ", 256 colores", 0
s_c32k          db ", 32.768 colores", 0
s_c64k          db ", 65.536 colores", 0
s_c16m          db ", 16,7 millones de colores", 0
s_vga           db "Vídeo: VGA (modo de reserva, sin VESA)", 0
s_vesa          db "Vídeo: VESA ", 0
s_lineal        db " con marco lineal", 0
s_raton_si      db "Ratón: driver INT 33h (HSPRATON)", 0
s_raton_no      db "Ratón: no hay (cargue HSPRATON)", 0
s_dpmi          db "DPMI ", 0
s_dpmi2         db " (32 bits); memoria libre: ", 0
s_kb            db " KB", 0

; ---- textos de los controles, los diálogos y los programas ----
t_aceptar       db "Aceptar", 0
t_cancelar      db "Cancelar", 0
t_si            db "Sí", 0
t_no            db "No", 0
t_nombre        db "Nombre del archivo:", 0
t_carpeta       db "Carpeta: ", 0
t_todos         db "*.*", 0
t_centro        db "Centro de programas", 0
t_centro_pista  db "Doble clic o Intro para abrir un grupo.", 0
t_grupo_vacio   db "Este grupo está vacío.", 0
t_grupo_vacio2  db "Archivo > Nuevo acceso directo...", 0
t_borrar_acc    db "¿Borrar el acceso directo «", 0
t_borrar_fin    db "»?", 0
t_borrar_gr     db "¿Borrar el grupo «", 0
t_borrar_gr2    db "»", 10, "con todos sus accesos directos?", 0
t_ad_prop       db "Propiedades del acceso directo", 0
t_ad_nuevo      db "Nuevo acceso directo", 0
t_ad_nombre     db "Nombre:", 0
t_ad_prog       db "Programa:", 0
t_ad_examinar   db "Examinar...", 0
t_ad_dir        db "Carpeta:", 0
t_ad_param      db "Parámetros:", 0
t_ad_icono      db "Icono:", 0
t_ad_volver     db "Volver a Hispa Ventanas al terminar", 0
t_ad_pausa      db "Esperar una tecla al terminar", 0
t_ad_elegir     db "Elegir el programa", 0
t_gr_preg       db "Nombre del grupo:", 0
t_gr_renombrar  db "Cambiar el nombre del grupo", 0
t_gr_nuevo      db "Nuevo grupo", 0
t_no_caben      db "No caben más.", 0
t_sin_grupo     db "Elija antes un grupo.", 0
t_mover_tit     db "Mover a otro grupo", 0
t_mover_preg    db "Grupo al que se lleva:", 0
t_mover_no      db "Abra un grupo y marque el acceso directo", 10
                db "que quiere mover.", 0
t_no_interno    db "Hispa Ventanas no tiene ese programa.", 0
t_no_programa   db "El acceso directo no dice qué programa abre.", 10
                db "Póngalo en Archivo > Propiedades.", 0
t_sin_lanzador  db "Para abrir programas de Hispa-DOS hay que entrar", 10
                db "en Hispa Ventanas con la orden VENTANAS.", 0
t_no_esta       db "No se encuentra el programa del acceso directo.", 10
                db "Mire la ruta en Archivo > Propiedades.", 0
t_lanzar_bloc   db "Para ejecutar un programa de Hispa-DOS, Hispa Ventanas", 10
                db "se cierra y vuelve al terminar el programa.", 10
                db "El Bloc de notas tiene cambios sin guardar y se perderán.", 10
                db "¿Continuar?", 0
t_bp_tit        db "Buscar programas", 0
t_bp_preg       db "Carpeta donde están los juegos:", 0
t_bp_defecto    db "C:\JUEGOS", 0
t_bp_todo       db "\*.*", 0
t_bp_exe        db "\*.EXE", 0
t_bp_com        db "\*.COM", 0
t_bp_bat        db "\*.BAT", 0
t_bp_cual       db "Programa principal de ", 0
t_bp_hecho      db "Accesos directos añadidos: ", 0
t_volumen       db "Volumen", 0
t_vol_pct       db "Volumen: ", 0
t_vol_mudo      db "Silencio", 0
t_vol_mez       db "Mezclador...", 0     ; [MEZCLADOR]
t_vol_no        db "No hay driver de sonido.", 0
t_vol_no2       db "Cargue antes HSPSND", 0
t_vol_no3       db "(está en \HISPADOS).", 0
t_bloc          db "Bloc de notas", 0
t_calc          db "Calculadora", 0
t_reloj         db "Reloj", 0
t_acerca_tit    db "Acerca de Hispa Ventanas", 0
t_acerca        db "Hispa Ventanas 0.3", 10
                db "El entorno gráfico de Hispa-DOS.", 10
                db "Hecho desde cero, en ensamblador y en español.", 10
                db "Software libre (GPL).", 0
; [APAGAR] (t_salir_preg, la pregunta de salir, ya no hace falta: ver apagado.inc)
t_guion         db " - ", 0
t_sin_nombre    db "(sin nombre)", 0
t_bloc_acerca   db "Bloc de notas de Hispa Ventanas.", 10
                db "Para textos sencillos de hasta 60.000 letras.", 0
t_bloc_cambios  db "El texto ha cambiado.", 10
                db "¿Quiere guardar los cambios?", 0
t_abrir         db "Abrir", 0
t_guardar_como  db "Guardar como", 0
t_ext_txt       db ".TXT", 0
t_no_guardar    db "No se ha podido guardar el archivo.", 0
t_no_abrir      db "No se ha podido abrir el archivo.", 0
t_buscar        db "Buscar", 0
t_buscar_preg   db "Texto que se busca:", 0
t_no_encontrado db "No se ha encontrado el texto.", 0
t_reloj_op      db "Opciones del reloj", 0
t_ro_seg        db "Aguja de los segundos", 0
t_ro_agujas     db "Esfera con agujas", 0
t_ro_cifras     db "Solo las cifras", 0
