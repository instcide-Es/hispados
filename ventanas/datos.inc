; =============================================================================
;  Hispa Ventanas - datos del aspecto: paleta, cursor, mosaico y textos
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;  Todo diseñado aquí.
; =============================================================================

; ---- los 16 colores de Hispa Ventanas (rojo, verde, azul) ----
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
