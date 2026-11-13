; =============================================================================
;  SOLITARI.HVP - Solitario, un juego de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un solitario de columnas (el «Klondike», de dominio público) con dos
;  barajas, a elegir en el menú «Juego» (la elegida se recuerda en
;  SOLITARI.REC):
;   - la BARAJA FRANCESA de 52 cartas (la de siempre): corazones y
;     diamantes en rojo, picas y tréboles en negro; A, 2 a 10, J, Q y K. Se
;     reparten siete columnas de 1 a 7 cartas (28) y las otras 24 son el mazo;
;   - la BARAJA ESPAÑOLA de 40 cartas: oros y copas (claros, con el número en
;     terracota), espadas y bastos (oscuros, en tinta); as, 2 a 7, sota
;     (10), caballo (11) y rey (12). Seis columnas de 1 a 6 cartas (21); el
;     mazo, 19.
;  Las columnas tienen tapadas todas menos la de encima. Un clic en el mazo
;  (arriba a la izquierda) saca una carta al descarte (a su derecha); cuando
;  se acaba, otro clic le da la vuelta al descarte, las veces que haga falta.
;  En las columnas se baja de una en una, del rey al as, alternando los
;  colores (rojo y negro; con la española, claros y oscuros); se puede
;  llevar una carta destapada con todas las que tiene encima. A una columna
;  vacía solo va un rey. La carta tapada que queda encima de una columna se
;  destapa sola. Arriba a la derecha, los cuatro montones de salida, uno por
;  palo: se sube del as al rey. Se gana con todas las cartas arriba.
;
;  Ratón: arrastrar las cartas; doble clic (o botón derecho) en una carta la
;  sube a su montón si puede. Teclado: F2 partida nueva, Retroceso o Ctrl+Z
;  deshacer (todos los pasos), Espacio sacar carta del mazo, S subir todas
;  las que se puedan. El tiempo empieza con el primer movimiento. Los
;  récords (partidas jugadas y ganadas, mejor tiempo y menos movimientos),
;  uno por baraja, y la baraja elegida se guardan en SOLITARI.REC (al lado
;  de SOLITARI.HVP; texto).
;
;  Todo está dibujado aquí, con diseño propio: las cartas (blancas, con las
;  esquinas redondeadas y un filete gris; el índice arriba a la izquierda y,
;  girado, abajo a la derecha; las pintas en su sitio de siempre, las de la
;  mitad de abajo del revés; el as con una pinta grande; las figuras
;  enmarcadas: las francesas, de dos cabezas, y las españolas, de cuerpo
;  entero con su palo debajo), el dorso añil con margen blanco, celosía y un
;  rombo ocre, el tapete azul petróleo, el icono y los textos. Es un módulo
;  .HVP (ventanas/hvp.inc). En la pantalla de 256 colores usa el rojo y el
;  negro puros del cubo de la paleta (en la de 16, terracota y tinta).
;
;  Parámetros del acceso directo (para las pruebas):
;    /S=n   semilla de los números aleatorios (siempre el mismo reparto)
;    /P     partida de prueba, casi acabada. Francesa: faltan por subir el
;           rey de corazones (en el mazo), la dama y el rey de diamantes (en
;           la segunda columna, el rey tapado) y el rey de picas (en la
;           primera). Española: el rey de oros (en el mazo), el caballo y el
;           rey de copas (en la segunda columna, el rey tapado) y el rey de
;           espadas (en la primera)
; =============================================================================
%include "hvp.inc"

CAR_AN      equ 72                  ; una carta
CAR_AL      equ 100
GAP         equ 10                  ; entre cartas
PASO        equ CAR_AN + GAP
N_COL       equ 7                   ; las columnas (con la española, 6)
FILA_Y      equ 28                  ; mazo, descarte y montones de salida
COL_Y       equ FILA_Y + CAR_AL + 12 ; las columnas
D_TAP       equ 6                   ; lo que asoma una carta tapada
D_VIS       equ 26                  ; y una destapada (el índice entero)
CLI_AN      equ N_COL * PASO + GAP
CLI_AL      equ 520
COL_MAX     equ CLI_AL - COL_Y - 6  ; hasta dónde llega una columna (si no
                                    ; cabe, sus cartas se juntan más)
PILA_T      equ 56                  ; una pila: cuántas y hasta 52 cartas
P_MAZO      equ 0
P_DESC      equ 1
P_SAL       equ 2                   ; 2-5: un montón por palo
P_COL       equ 6                   ; 6-12: las columnas
N_PILAS     equ P_COL + N_COL
TAPADA      equ 0x40                ; la carta es palo*cartas_por_palo+orden + esto
DES_MAX     equ 512                 ; pasos que se pueden deshacer
E_LISTO     equ 0                   ; repartida, sin tocar (el reloj, parado)
E_JUEGA     equ 1
E_GANADA    equ 2
WM_FINAL    equ WM_USER + 100       ; (propio) decir que se ha ganado
B_FRA       equ 0                   ; las barajas
B_ESP       equ 1
; los dibujos ya hechos (un octeto por punto)
PIP_T       equ 13 * 13             ; una pinta
PEQ_T       equ 9 * 9               ; la pinta pequeña del índice
P26_T       equ 26 * 26             ; la pinta al doble (montón de salida vacío)
AS_T        equ 39 * 39             ; y al triple (el as)
IDX_AN      equ 14                  ; el número o la letra del índice
IDX_AL      equ 9
IDX_T       equ IDX_AN * IDX_AL
FIG_AN      equ 38                  ; una figura, dentro de su recuadro
FIG_AL      equ 82
FIG_T       equ FIG_AN * FIG_AL
CARTA_T     equ CAR_AN * CAR_AL
; colores del cubo de la paleta de 256 (16 + 36 rojo + 6 verde + azul, de 0 a
; 5; ventanas/video.inc). En la pantalla de 16 colores se ven como el de su
; cifra baja: el rojo, terracota; el negro, tinta; la piel, piedra; el gris,
; arena
ROJO        equ 16 + 36*5           ; FF0000
NEGRO       equ 16                  ; 000000
PIEL        equ 16 + 36*5 + 6*4 + 3 ; FFCC99
GRIS        equ 16 + 36*2 + 6*2 + 2 ; 666666 (el filete de las cartas)

HVP_CABECERA "Solitario", entrada
        db "................................"
        db "................................"
        db "....000000000000000............."
        db "...0ccccccccccccccc0............"
        db "...0ccccccccccccccc0............"
        db "...0ccacccacccacccc0............"
        db "...0ccc2ccc2ccc2ccc0............"
        db "...0ccccccc.0000000000000000...."
        db "...0ccccccc099999999999999990..."
        db "...0ccaccca094999999999999990..."
        db "...0ccc2ccc044999999999999990..."
        db "...0ccccccc094999999999999990..."
        db "...0ccccccc094999999999999990..."
        db "...0ccaccca099999999999999990..."
        db "...0ccc2ccc099999999999999990..."
        db "...0ccccccc099944499994449990..."
        db "...0ccccccc099444449944444990..."
        db "...0ccaccca094444444444444490..."
        db "...0ccc2ccc094444444444444490..."
        db "...0ccccccc094444444444444490..."
        db "...0ccccccc099444444444444990..."
        db "...0ccaccca099944444444449990..."
        db "...0ccc2ccc099994444444499990..."
        db "...0ccccccc099999444444999990..."
        db "....0000000099999944449999990..."
        db "...........099999994499999990..."
        db "...........099999999999999990..."
        db "...........099999999999999990..."
        db "...........099999999999999990..."
        db "...........099999999999999990..."
        db "............0000000000000000...."
        db "................................"
HVP_CODIGO

c_sol       dd sol_proc
            db C_PETROLEO, 0, 0, 0
sol_plant:
        PLANTILLA c_sol, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 106, 30, \
                  CLI_AN + 2*BORDE, CLI_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

sol_menu:
        db "&Juego", 0
        dw 101
        db "&Nueva partida    F2", 0
        dw 102
        db "&Deshacer    Retroceso", 0
        dw 103
        db "S&ubir las que se puedan    S", 0
        dw 0
        db 0
        dw 104
m_fra   db "» Baraja &francesa (52 cartas)", 0      ; (el primer carácter es
        dw 105                                      ; la marca de la elegida)
m_esp   db "  Baraja &española (40 cartas)", 0
        dw 0
        db 0
        dw 107
        db "&Récords...", 0
        dw 0
        db 0
        dw 108
        db "&Salir", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw 110
        db "&Cómo se juega...", 0
        dw 0xFFFF
        db 0

; ---- los dibujos: «.» transparente, 0-9 a-f los 16 colores, x/y/z los del
; palo, r rojo, n negro y p piel (los del cubo) ----
; las pintas de la francesa (13x13): corazones, diamantes, picas y tréboles
d_pipf:
        db "............."
        db "..xxx...xxx.."
        db ".xxxxx.xxxxx."
        db "xxxxxxxxxxxxx"
        db "xxxxxxxxxxxxx"
        db "xxxxxxxxxxxxx"
        db ".xxxxxxxxxxx."
        db "..xxxxxxxxx.."
        db "...xxxxxxx..."
        db "....xxxxx...."
        db ".....xxx....."
        db "......x......"
        db "............."

        db "......x......"
        db ".....xxx....."
        db ".....xxx....."
        db "....xxxxx...."
        db "...xxxxxxx..."
        db "..xxxxxxxxx.."
        db ".xxxxxxxxxxx."
        db "..xxxxxxxxx.."
        db "...xxxxxxx..."
        db "....xxxxx...."
        db ".....xxx....."
        db ".....xxx....."
        db "......x......"

        db "......x......"
        db ".....xxx....."
        db "....xxxxx...."
        db "...xxxxxxx..."
        db "..xxxxxxxxx.."
        db ".xxxxxxxxxxx."
        db "xxxxxxxxxxxxx"
        db "xxxxxxxxxxxxx"
        db ".xxxx.x.xxxx."
        db "..xx..x..xx.."
        db "......x......"
        db ".....xxx....."
        db "...xxxxxxx..."

        db ".....xxx....."
        db "....xxxxx...."
        db "...xxxxxxx..."
        db "...xxxxxxx..."
        db "....xxxxx...."
        db ".xxx.xxx.xxx."
        db "xxxxxxxxxxxxx"
        db "xxxxxxxxxxxxx"
        db "xxxxx.x.xxxxx"
        db ".xxx..x..xxx."
        db "......x......"
        db ".....xxx....."
        db "...xxxxxxx..."
%if ($ - d_pipf) != 4 * PIP_T
%error "las pintas francesas no miden 13x13"
%endif

; las de la española (13x13): oros, copas, espadas y bastos
d_pipe:
        db "....11111...."
        db "..11aaaaa11.."
        db ".1aaaaaaaaa1."
        db ".1aaa111aaa1."
        db "1aaa1aaa1aaa1"
        db "1aa1aa9aa1aa1"
        db "1aa1a9a9a1aa1"
        db "1aa1aa9aa1aa1"
        db "1aaa1aaa1aaa1"
        db ".1aaa111aaa1."
        db ".1aaaaaaaaa1."
        db "..11aaaaa11.."
        db "....11111...."

        db ".ddddddddddd."
        db ".d444444444d."
        db ".d459444444d."
        db ".d459444444d."
        db ".d454444444d."
        db "..d4444444d.."
        db "...d44444d..."
        db "....ddddd...."
        db ".....d4d....."
        db ".....d4d....."
        db "....dd4dd...."
        db "..dd44444dd.."
        db "..ddddddddd.."

        db "......2......"
        db ".....222....."
        db "....22e22...."
        db "....2e9e2...."
        db "....2e9e2...."
        db "....2e9e2...."
        db "....2e9e2...."
        db "....2eee2...."
        db ".ccccccccccc."
        db ".caacccccaac."
        db ".ccccccccccc."
        db ".....1a1....."
        db ".....111....."

        db "....11111...."
        db "...1bbbbb1..."
        db "..1bbbbbbb1.."
        db "..1bb1bbbb1.."
        db "..1bbbbbbb1.."
        db "..1bbbb1bb1.."
        db "...1bbbbb1..."
        db "...1bbbbb1..."
        db "....1bbb1...."
        db "....1bbb1...."
        db "....1b1b1...."
        db ".....1b1....."
        db ".....111....."
%if ($ - d_pipe) != 4 * PIP_T
%error "las pintas españolas no miden 13x13"
%endif

; las pintas pequeñas del índice (9x9), las cuatro francesas y las cuatro
; españolas
d_peqf:
        db ".xx...xx."
        db "xxxx.xxxx"
        db "xxxxxxxxx"
        db "xxxxxxxxx"
        db ".xxxxxxx."
        db "..xxxxx.."
        db "...xxx..."
        db "....x...."
        db "........."

        db "....x...."
        db "...xxx..."
        db "..xxxxx.."
        db ".xxxxxxx."
        db "xxxxxxxxx"
        db ".xxxxxxx."
        db "..xxxxx.."
        db "...xxx..."
        db "....x...."

        db "....x...."
        db "...xxx..."
        db "..xxxxx.."
        db ".xxxxxxx."
        db "xxxxxxxxx"
        db "xxxxxxxxx"
        db ".xx.x.xx."
        db "....x...."
        db "..xxxxx.."

        db "...xxx..."
        db "..xxxxx.."
        db "..xxxxx.."
        db "...xxx..."
        db "xxx.x.xxx"
        db "xxxxxxxxx"
        db "xxx.x.xxx"
        db "....x...."
        db "..xxxxx.."
d_peqe:
        db "..11111.."
        db ".1aaaaa1."
        db "1aa111aa1"
        db "1a1aaa1a1"
        db "1a1a9a1a1"
        db "1a1aaa1a1"
        db "1aa111aa1"
        db ".1aaaaa1."
        db "..11111.."

        db "ddddddddd"
        db ".d44444d."
        db ".d45444d."
        db "..d444d.."
        db "...ddd..."
        db "....d...."
        db "....d...."
        db "..ddddd.."
        db "........."

        db "....2...."
        db "...2e2..."
        db "...2e2..."
        db "...2e2..."
        db "...2e2..."
        db ".ccccccc."
        db "....1...."
        db "....a...."
        db "........."

        db "...111..."
        db "..1bbb1.."
        db "..1b1b1.."
        db "..1bbb1.."
        db "...1bb1.."
        db "...1bb1.."
        db "....1b1.."
        db "....1b1.."
        db "....111.."
%if ($ - d_peqf) != 8 * PEQ_T
%error "las pintas pequeñas no miden 9x9"
%endif

; las figuras francesas (19x20, al doble): la mitad de arriba de la jota, la
; dama y el rey; la de abajo es la misma, girada (cartas de dos cabezas)
d_figf:
        db "..................."
        db ".......xxxxx......."
        db "......xxxxxxx.yy..."
        db ".....xxxxxxxxxyy..."
        db ".....yyyyyyyyy.y..."
        db ".....11ppppp11....."
        db ".....1ppppppp1....."
        db ".....1p0ppp0p1....."
        db ".....1ppppppp1....."
        db "......ppppppp......"
        db "......ppprppp......"
        db ".......ppppp......."
        db ".....zzz999zzz....."
        db "...zzzzz999zzzzz..."
        db "..zzxxxzz9zzxxxzz.."
        db ".zxxxxxxzyzxxxxxxz."
        db ".zxxyxxxxyxxxxyxxz."
        db ".zxxxxxxxyxxxxxxxz."
        db ".zxxyxxxzyzxxxyxxz."
        db ".zxxxxxxzyzxxxxxxz."

        db ".......y.y.y......."
        db "......yyyyyyy......"
        db "......yryyyry......"
        db ".....1yyyyyyy1....."
        db "....111ppppp111...."
        db "....11ppppppp11...."
        db "....11p0ppp0p11...."
        db "....11ppppppp11...."
        db "....11pppppp111...."
        db "....11pprrrpp11...."
        db "...111.ppppp.111..."
        db "...11..zzpzz..11..."
        db "..11zzzzz9zzzzz11.."
        db "..1zzzzz9y9zzzzz1.."
        db "..zzxxxxz9zxxxxzz.."
        db ".zzxxxxxxzxxxxxxzz."
        db ".zxxyyxxxxxxxyyxxz."
        db ".zxxyyxxxyxxxyyxxz."
        db ".zxxxxxxyyyxxxxxxz."
        db ".zxxxxxxxyxxxxxxxz."

        db "....y....y....y...."
        db "....yy..yyy..yy...."
        db "....yyyyyyyyyyy...."
        db "....yryyyzyyyry...."
        db "....yyyyyyyyyyy...."
        db ".....ppppppppp....."
        db ".....p0ppppp0p....."
        db ".....ppppppppp....."
        db ".....ppppppppp....."
        db ".....111ppp111....."
        db ".....111rrr111....."
        db "......1111111......"
        db "...zzzz11111zzzz..."
        db ".zzzzzzz111zzzzzzz."
        db ".zxxxxxzz1zzxxxxxz."
        db ".zxxyxxxzyzxxxyxxz."
        db ".zxxxxxxzyzxxxxxxz."
        db ".zxxyxxxzyzxxxyxxz."
        db ".zxxxxxxzyzxxxxxxz."
        db ".zxxyxxxzyzxxxyxxz."
%if ($ - d_figf) != 3 * 19 * 20
%error "las figuras francesas no miden 19x20"
%endif

; las figuras españolas (19x26, al doble), de cuerpo entero: sota, caballo y
; rey (debajo va la pinta de su palo, al doble)
d_fige:
        db "..................."
        db ".......yyyyy......."
        db "......yyyyyyy......"
        db "......1ppppp1......"
        db "......1p0p0p1......"
        db "......1ppppp1......"
        db ".......pp0pp......."
        db "........ppp........"
        db ".....xxx999xxx....."
        db "....xxxxx9xxxxx...."
        db "...xxxxxxyxxxxxx..."
        db "...xx.xxxyxxx.xx..."
        db "...pp.xxxyxxx.pp..."
        db "......yyyyyyy......"
        db "......xxxxxxx......"
        db ".....xxxxxxxxx....."
        db ".....xxxxxxxxx....."
        db ".....xxxxxxxxx....."
        db "......zz...zz......"
        db "......zz...zz......"
        db "......zz...zz......"
        db "......zz...zz......"
        db "......zz...zz......"
        db "......11...11......"
        db ".....111...111....."
        db "..................."

        db "..........yyy......"
        db ".........yyyyy....."
        db "..........ppp......"
        db "..........0pp......"
        db "..........ppp......"
        db ".........xxxxx....."
        db "........xxxxxxx...."
        db "..1.1...pxxyxxp...."
        db "..1111...xxyxx....."
        db ".111111..xxxxx....."
        db "1110111.yyyyyyy...."
        db "1111111.yyyyyyy11.."
        db ".111.11111111111111"
        db "....111111zz111111."
        db "....111111zz1111.1."
        db "....111111zz1111..1"
        db ".....111111111111.."
        db ".....11.......11..."
        db ".....11.......11..."
        db ".....11.......11..."
        db ".....1.1......1.1.."
        db ".....1.1......1.1.."
        db "....00.00....00.00."
        db "..................."
        db "..................."
        db "..................."

        db "....a..a..a..a....."
        db "....aaaaaaaaaa....."
        db "....a4aa44aa4a....."
        db "....aaaaaaaaaa....."
        db ".....pppppppp......"
        db ".....p0pppp0p......"
        db ".....pppppppp......"
        db ".....11pppp11......"
        db ".....11111111......"
        db "......111111......."
        db "...xxxxx11xxxxx...."
        db "..xxxxxxxxxxxxxx..."
        db "..xxyxxxxxxxxyxx..."
        db "..xxyxxxyyxxxyxx..."
        db "..xxyxxyyyyxxyxx..."
        db "..xxyxxxyyxxxyxx..."
        db "..pp.xxxxxxxx.pp..."
        db ".....xxxxxxxx......"
        db "....xxxxxxxxxx....."
        db "....xxxxxxxxxx....."
        db "....xxxxxxxxxx....."
        db "....xxxxxxxxxx....."
        db "....yyyyyyyyyy....."
        db ".....11....11......"
        db "....111....111....."
        db "..................."
%if ($ - d_fige) != 3 * 19 * 26
%error "las figuras españolas no miden 19x26"
%endif
; los tres colores (x, y, z) de las figuras de cada palo
fig_colf    db ROJO, C_OCRE, C_ANIL,        ROJO, C_OCRE, C_OLIVA
            db NEGRO, C_OCRE, ROJO,         NEGRO, C_OCRE, C_ANIL
fig_cole    db C_OCRE, C_MARRON, C_TERRACOTA,     C_TERRACOTA, C_GRANATE, C_OCRE
            db C_PETROLEO, C_TURQUESA, C_ANIL,    C_OLIVA, C_ARENA_OSC, C_MARRON

; dónde van las pintas del 2 al 10 (cuántas; x, y de cada una en la carta,
; +128 en x si va del revés). Columnas en 16, 30 y 44; filas en 8 (arriba) y
; 80 (abajo)
INV         equ 128
t_pintas    dd pin2, pin3, pin4, pin5, pin6, pin7, pin8, pin9, pin10
pin2        db 2,  30, 8,  30+INV, 80
pin3        db 3,  30, 8,  30, 44,  30+INV, 80
pin4        db 4,  16, 8,  44, 8,  16+INV, 80,  44+INV, 80
pin5        db 5,  16, 8,  44, 8,  30, 44,  16+INV, 80,  44+INV, 80
pin6        db 6,  16, 8,  44, 8,  16, 44,  44, 44,  16+INV, 80,  44+INV, 80
pin7        db 7,  16, 8,  44, 8,  30, 26,  16, 44,  44, 44,  16+INV, 80,  44+INV, 80
pin8        db 8,  16, 8,  44, 8,  30, 26,  16, 44,  44, 44,  30+INV, 62
            db     16+INV, 80,  44+INV, 80
pin9        db 9,  16, 8,  44, 8,  16, 32,  44, 32,  30, 44
            db     16+INV, 56,  44+INV, 56,  16+INV, 80,  44+INV, 80
pin10       db 10, 16, 8,  44, 8,  30, 20,  16, 32,  44, 32
            db     16+INV, 56,  44+INV, 56,  30+INV, 68,  16+INV, 80,  44+INV, 80
; lo que se escribe en el índice (tres octetos cada uno)
t_numf      db "A", 0, 0, "2", 0, 0, "3", 0, 0, "4", 0, 0, "5", 0, 0, "6", 0, 0
            db "7", 0, 0, "8", 0, 0, "9", 0, 0, "10", 0, "J", 0, 0, "Q", 0, 0, "K", 0, 0
t_nume      db "1", 0, 0, "2", 0, 0, "3", 0, 0, "4", 0, 0, "5", 0, 0, "6", 0, 0
            db "7", 0, 0, "10", 0, "11", 0, "12", 0
; las esquinas redondeadas de las cartas: en las cuatro primeras filas, cuántos
; puntos no son de la carta y cuántos del filete
esq_fuera   db 4, 2, 1, 1
esq_filete  db 200, 2, 1, 1
; los rombos del montón del mazo vacío
; la partida de prueba: pila, cuántas y sus cartas
d_pruebaf   db P_SAL, 12,     0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
            db P_SAL + 1, 11, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
            db P_SAL + 2, 12, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37
            db P_SAL + 3, 13, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
            db P_COL, 1,      38
            db P_COL + 1, 2,  25 | TAPADA, 24
            db P_MAZO, 1,     12
            db 0xFF
d_pruebae   db P_SAL, 9,      0, 1, 2, 3, 4, 5, 6, 7, 8
            db P_SAL + 1, 8,  10, 11, 12, 13, 14, 15, 16, 17
            db P_SAL + 2, 9,  20, 21, 22, 23, 24, 25, 26, 27, 28
            db P_SAL + 3, 10, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39
            db P_COL, 1,      29
            db P_COL + 1, 2,  19 | TAPADA, 18
            db P_MAZO, 1,     9
            db 0xFF

; ---- lo de cada baraja (francesa, española) ----
b_ord       dd 13, 10               ; cartas por palo
b_cartas    dd 52, 40
b_cols      dd 7, 6                 ; columnas
b_col_x0    dd GAP, GAP + PASO / 2  ; dónde empieza la primera (centradas)
b_mazo      dd 52 - 28, 40 - 21     ; lo que queda para el mazo
b_pip       dd d_pipf, d_pipe
b_peq       dd d_peqf, d_peqe
b_num       dd t_numf, t_nume
b_figcol    dd fig_colf, fig_cole
b_prueba    dd d_pruebaf, d_pruebae
b_gana      dd t_ganaf, t_ganae
b_rec_tit   dd t_rec_titf, t_rec_tite
b_color     db ROJO, NEGRO,  C_TERRACOTA, C_TINTA   ; del índice: rojos/claros, negros/oscuros

t_titulo    db "Solitario", 0
t_movs      db "Movimientos: ", 0
t_tiempo    db "Tiempo: ", 0
t_mazo      db "Mazo: ", 0
t_gana_tit  db "Solitario resuelto", 0
t_ganaf     db "¡Las cincuenta y dos arriba!", 10, "Tiempo: ", 0
t_ganae     db "¡Las cuarenta arriba!", 10, "Tiempo: ", 0
t_gana2     db "    Movimientos: ", 0
t_gana3     db 10, "Es el mejor tiempo.", 0
t_gana4     db 10, "Nunca se había hecho con menos movimientos.", 0
t_rec_titf  db "Récords (francesa)", 0
t_rec_tite  db "Récords (española)", 0
t_rec_jug   db "Partidas jugadas:  ", 0
t_rec_gan   db "Partidas ganadas:  ", 0
t_rec_tie   db "Mejor tiempo:  ", 0
t_rec_mov   db "Menos movimientos:  ", 0
t_rec_no    db "sin apuntar", 0
t_ayuda_tit db "Cómo se juega", 0
t_ayuda     db "Hay que subir los cuatro palos, del as al rey, a los", 10
            db "montones de arriba. En las columnas se baja de una en", 10
            db "una, del rey al as, alternando los colores. A una", 10
            db "columna vacía solo va un rey. El mazo se puede pasar", 10
            db "las veces que se quiera.", 10
            db 10
            db "Juego > Baraja: la francesa, de 52 cartas (rojas,", 10
            db "corazones y diamantes; negras, picas y tréboles; siete", 10
            db "columnas), o la española, de 40 (claros, oros y copas;", 10
            db "oscuros, espadas y bastos; rey, caballo, sota, 7... as;", 10
            db "seis columnas). Al cambiar, empieza otra partida.", 10
            db 10
            db "Clic en el mazo: saca carta. Las cartas se arrastran;", 10
            db "doble clic o botón derecho: la carta sube a su montón.", 10
            db "F2: nueva.  Retroceso: deshacer.  Espacio: sacar carta.", 0
k_cab       db "Solitario - récords (Hispa Ventanas)", 13, 10, 0
k_baraja    db "Baraja=", 0
k_fra       db "francesa", 0
k_esp       db "española", 0
; las claves de cada baraja (las de la española, las de siempre; ninguna
; está dentro de otra, que hvl_clave busca en todo el texto)
k_claves    dd k_jugf, k_ganf, k_tief, k_movf,  k_jug, k_gan, k_tie, k_mov
k_jug       db "Jugadas=", 0
k_gan       db "Ganadas=", 0
k_tie       db "Tiempo=", 0
k_mov       db "Movimientos=", 0
k_jugf      db "Francesa jugadas=", 0
k_ganf      db "Francesa ganadas=", 0
k_tief      db "Francesa tiempo=", 0
k_movf      db "Francesa movimientos=", 0

; ---- la entrada: EBX=base ESI=ruta del módulo EDI=parámetros ----
entrada:
        push edi
        mov edi, ruta_rec           ; SOLITARI.REC, al lado del módulo
        mov eax, 'REC'
        call hvl_ruta
        pop edi
        mov esi, edi
        mov al, 'S'
        call hvl_opcion
        jc .reloj
        call hvl_lee_num
        jmp .sem
.reloj:
        HV tics
.sem:
        HV azar_semilla
        mov esi, edi
        mov al, 'P'
        call hvl_opcion
        setnc byte [a_prueba]
        call rec_leer               ; (también la baraja)
        call preparar
        call poner_baraja
        mov esi, sol_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

; ---- el procedimiento de la ventana ----
sol_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONDOWN
        je .aprieta
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_LBUTTONDBLCLK
        je .doble
        cmp eax, WM_RBUTTONDOWN
        je .doble
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .letra
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_FINAL
        je .final
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], sol_menu
        mov dword [ebp+V_ICONO], hvp_icono
        call nueva
        jmp .hecho
.destruir:
        HV msg_salir
        jmp .hecho
.reloj:
        cmp dword [estado], E_JUEGA
        jne .hecho
        test dword [ebp+V_ESTADO], S_MINIM
        jnz .hecho                  ; minimizada, el tiempo no corre
        cmp dword [segundos], 5999
        jae .hecho
        inc dword [segundos]
        call inv_arriba
        jmp .hecho
.aprieta:
        cmp dword [estado], E_GANADA
        je .hecho
        cmp byte [arr_on], 0
        jne .hecho
        call donde                  ; EAX=pila ECX=carta
        test eax, eax
        js .hecho
        jnz .a1
        call pasar                  ; el mazo
        jmp .hecho
.a1:
        test ecx, ecx
        js .hecho
        imul esi, eax, PILA_T
        add esi, pilas
        test byte [esi+1+ecx], TAPADA
        jnz .hecho
        mov [arr_pila], eax         ; se lleva esa carta y las de encima
        mov [arr_desde], ecx
        movzx ebx, byte [esi]
        sub ebx, ecx
        mov [arr_n], ebx
        mov eax, [g_x]
        mov ebx, [g_y]
        mov [arr_x], eax
        mov [arr_y], ebx
        movsx ecx, dx
        sar edx, 16
        sub ecx, eax
        sub edx, ebx
        mov [arr_dx], ecx
        mov [arr_dy], edx
        mov byte [arr_on], 1
        HV ven_capturar
        jmp .hecho
.mueve:
        cmp byte [arr_on], 0
        je .hecho
        call inv_arr
        movsx eax, dx
        sar edx, 16
        sub eax, [arr_dx]
        sub edx, [arr_dy]
        mov [arr_x], eax
        mov [arr_y], edx
        call inv_arr
        jmp .hecho
.suelta:
        cmp byte [arr_on], 0
        je .hecho
        mov byte [arr_on], 0
        HV ven_soltar
        HV ven_invalidar
        call soltar
        jmp .hecho
.doble:
        cmp dword [estado], E_GANADA
        je .hecho
        cmp byte [arr_on], 0
        jne .hecho
        call donde
        cmp eax, P_DESC
        jl .hecho
        je .d1
        cmp eax, P_COL
        jb .hecho
.d1:
        imul esi, eax, PILA_T       ; solo la de encima
        movzx ebx, byte [pilas+esi]
        dec ebx
        cmp ebx, ecx
        jne .hecho
        call subir
        jmp .hecho
.letra:
        cmp ecx, ' '
        jne .l1
        call pasar
        jmp .hecho
.l1:
        and ecx, 0xDF
        cmp ecx, 'S'
        jne .hecho
        call subir_todas
        jmp .hecho
.tecla:
        cmp ecx, TC_F1 + 1          ; F2
        jne .t1
        call nueva
        jmp .hecho
.t1:
        cmp ecx, TC_RETRO
        je .t2
        cmp ecx, TC_CTRL + 26       ; Ctrl+Z
        jne .hecho
.t2:
        call deshacer
        jmp .hecho
.orden:
        cmp ecx, 101
        jne .o2
        call nueva
        jmp .hecho
.o2:
        cmp ecx, 102
        je .t2
        cmp ecx, 103
        jne .o4
        call subir_todas
        jmp .hecho
.o4:
        cmp ecx, 104                ; las barajas
        je .bar
        cmp ecx, 105
        jne .o7
.bar:
        sub ecx, 104
        cmp byte [arr_on], 0
        jne .hecho
        mov [baraja], ecx
        call poner_baraja
        call rec_guardar
        call nueva
        jmp .hecho
.o7:
        cmp ecx, 107
        je .records
        cmp ecx, 108
        jne .o10
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.o10:
        cmp ecx, 110
        jne .hecho
        mov esi, t_ayuda
        mov edi, t_ayuda_tit
        jmp .dlg
.records:
        call rec_de                 ; EBX=los de esta baraja
        mov edi, txt
        mov esi, t_rec_jug
        call hvl_cad
        mov eax, [ebx]
        call hvl_num
        mov al, 10
        stosb
        mov esi, t_rec_gan
        call hvl_cad
        mov eax, [ebx+4]
        call hvl_num
        mov al, 10
        stosb
        mov esi, t_rec_tie
        call hvl_cad
        mov eax, [ebx+8]
        test eax, eax
        jz .r1
        call pon_tiempo
        jmp .r2
.r1:
        mov esi, t_rec_no
        call hvl_cad
.r2:
        mov al, 10
        stosb
        mov esi, t_rec_mov
        call hvl_cad
        mov eax, [ebx+12]
        test eax, eax
        jz .r3
        call hvl_num
        jmp .r4
.r3:
        mov esi, t_rec_no
        call hvl_cad
.r4:
        mov byte [edi], 0
        mov esi, txt
        mov eax, [baraja]
        mov edi, [b_rec_tit+eax*4]
        jmp .dlg
.final:
        mov edi, txt                ; «¡Las cuarenta arriba! Tiempo...»
        mov eax, [baraja]
        mov esi, [b_gana+eax*4]
        call hvl_cad
        mov eax, [segundos]
        call pon_tiempo
        mov esi, t_gana2
        call hvl_cad
        mov eax, [movs]
        call hvl_num
        test byte [nuevo_rec], 1
        jz .f1
        mov esi, t_gana3
        call hvl_cad
.f1:
        test byte [nuevo_rec], 2
        jz .f2
        mov esi, t_gana4
        call hvl_cad
.f2:
        mov byte [edi], 0
        mov esi, txt
        mov edi, t_gana_tit
.dlg:
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
.hecho:
        xor eax, eax
        ret

; EAX=segundos: «M:SS» en EDI, que avanza
pon_tiempo:
        push eax
        push ecx
        push edx
        xor edx, edx
        mov ecx, 60
        div ecx
        call hvl_num
        mov al, ':'
        stosb
        mov eax, edx
        cmp eax, 10
        jae .d
        mov byte [edi], '0'
        inc edi
.d:
        call hvl_num
        pop edx
        pop ecx
        pop eax
        ret

; los rótulos de arriba hay que repintarlos
inv_arriba:
        pushad
        mov ebp, [ven]
        xor eax, eax
        xor ebx, ebx
        mov ecx, CLI_AN
        mov edx, FILA_Y - 4
        HV ven_invalidar_rect
        popad
        ret

; las cartas que se llevan con el ratón hay que repintarlas
inv_arr:
        pushad
        mov ebp, [ven]
        mov eax, [arr_x]
        mov ebx, [arr_y]
        mov ecx, CAR_AN
        mov edx, [arr_n]
        dec edx
        imul edx, D_VIS
        add edx, CAR_AL
        HV ven_invalidar_rect
        popad
        ret

; EAX=pila -> EAX=x EBX=y de su sitio. Arriba: el mazo, el descarte, un hueco
; y los cuatro montones de salida
pila_pos:
        cmp eax, P_COL
        jae .col
        cmp eax, P_SAL
        jb .a
        inc eax                     ; (después del hueco)
.a:
        imul eax, PASO
        add eax, GAP
        mov ebx, FILA_Y
        ret
.col:
        sub eax, P_COL
        imul eax, PASO
        add eax, [col_x0]
        mov ebx, COL_Y
        ret

; ESI=una columna -> EAX=lo que asoma cada carta destapada (D_VIS, o menos si
; la columna no cabe entera)
col_paso:
        push ebx
        push ecx
        push edx
        movzx ecx, byte [esi]
        xor ebx, ebx                ; EBX=las tapadas
.t:
        cmp ebx, ecx
        jae .c
        test byte [esi+1+ebx], TAPADA
        jz .c
        inc ebx
        jmp .t
.c:
        sub ecx, ebx                ; ECX=las destapadas
        mov eax, D_VIS
        cmp ecx, 2
        jb .x
        dec ecx
        imul ebx, D_TAP
        mov eax, COL_MAX - CAR_AL
        sub eax, ebx
        xor edx, edx
        div ecx
        cmp eax, D_VIS
        jbe .x
        mov eax, D_VIS
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; ESI=una columna ECX=carta -> EBX=lo que baja desde COL_Y
carta_y:
        push eax
        push edx
        call col_paso
        xor ebx, ebx
        xor edx, edx
.b:
        cmp edx, ecx
        jae .x
        test byte [esi+1+edx], TAPADA
        jz .v
        add ebx, D_TAP
        jmp .s
.v:
        add ebx, eax
.s:
        inc edx
        jmp .b
.x:
        pop edx
        pop eax
        ret

; EDX=x + y*65536 (de la zona) -> EAX=pila (-1: ninguna) ECX=carta (-1: no
; hay; en las de arriba, la de encima); g_x, g_y = dónde está esa carta
donde:
        push ebx
        push edx
        push esi
        push edi
        movsx eax, dx
        sar edx, 16                 ; EAX=x EDX=y
        cmp edx, FILA_Y
        jl .no
        cmp edx, FILA_Y + CAR_AL
        jge .cols
        sub eax, GAP                ; arriba
        js .no
        push edx
        xor edx, edx
        mov ecx, PASO
        div ecx
        mov ebx, edx
        pop edx
        cmp ebx, CAR_AN             ; (entre dos cartas)
        jae .no
        cmp eax, N_COL - 1
        ja .no
        cmp eax, 2                  ; (el hueco)
        je .no
        imul ebx, eax, PASO
        add ebx, GAP
        mov [g_x], ebx
        mov dword [g_y], FILA_Y
        cmp eax, 2
        jb .arr
        dec eax                     ; un montón de salida
.arr:
        imul esi, eax, PILA_T
        movzx ecx, byte [pilas+esi]
        dec ecx
        jmp .x
.cols:
        sub eax, [col_x0]
        js .no
        push edx
        xor edx, edx
        mov ecx, PASO
        div ecx
        mov ebx, edx
        pop edx
        cmp ebx, CAR_AN
        jae .no
        cmp eax, [n_cols]
        jae .no
        imul ebx, eax, PASO
        add ebx, [col_x0]
        mov [g_x], ebx
        sub edx, COL_Y
        js .no
        add eax, P_COL
        imul esi, eax, PILA_T
        add esi, pilas
        mov dword [g_y], COL_Y
        movzx ecx, byte [esi]
        dec ecx
        js .x                       ; vacía
        call carta_y                ; la de encima, entera
        lea edi, [ebx+CAR_AL]
        cmp edx, edi
        jge .no
.b:
        cmp edx, ebx
        jge .esa
        dec ecx
        call carta_y
        jmp .b
.esa:
        add ebx, COL_Y
        mov [g_y], ebx
        jmp .x
.no:
        mov eax, -1
        mov ecx, -1
.x:
        pop edi
        pop esi
        pop edx
        pop ebx
        ret

; AL=carta -> EAX=palo EDX=orden (sin la marca de tapada)
palo_orden:
        push ecx
        movzx eax, al
        and eax, 0x3F
        xor edx, edx
        div dword [n_ord]
        pop ecx
        ret

; AL=carta -> EBX=su montón de salida; CF=0 si le toca subir
puede_sal:
        push eax
        push ecx
        push edx
        call palo_orden             ; EAX=palo EDX=orden
        lea ebx, [eax+P_SAL]
        imul ecx, ebx, PILA_T
        movzx ecx, byte [pilas+ecx]
        cmp ecx, edx
        jne .no
        clc
        jmp .x
.no:
        stc
.x:
        pop edx
        pop ecx
        pop eax
        ret

; AL=carta EBX=columna (su pila) -> CF=0 si se puede poner encima
puede_col:
        pushad
        call palo_orden             ; EAX=palo EDX=orden
        imul esi, ebx, PILA_T
        add esi, pilas
        movzx ecx, byte [esi]
        test ecx, ecx
        jnz .hay
        inc edx                     ; a un hueco, solo un rey
        cmp edx, [n_ord]
        jne .no
        jmp .si
.hay:
        mov bl, [esi+ecx]           ; la de encima
        test bl, TAPADA
        jnz .no
        mov edi, eax
        mov ebp, edx
        mov al, bl
        call palo_orden             ; EAX=su palo EDX=su orden
        inc ebp                     ; una menos
        cmp ebp, edx
        jne .no
        xor eax, edi                ; y de distinto color: rojo sobre negro o
        test al, 2                  ; al revés (claro sobre oscuro)
        jz .no
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; EAX=de qué pila EBX=a cuál ECX=cuántas (las de encima, en su orden)
mover:
        pushad
        imul esi, eax, PILA_T
        add esi, pilas
        imul edi, ebx, PILA_T
        add edi, pilas
        movzx eax, byte [esi]
        sub eax, ecx
        mov [esi], al
        lea esi, [esi+1+eax]
        movzx eax, byte [edi]
        add [edi], cl
        lea edi, [edi+1+eax]
        rep movsb
        popad
        ret

; EAX=de qué pila EBX=a cuál: todas, dándoles la vuelta (mazo y descarte)
volcar:
        pushad
        imul esi, eax, PILA_T
        add esi, pilas
        imul edi, ebx, PILA_T
        add edi, pilas
.b:
        movzx ecx, byte [esi]
        test ecx, ecx
        jz .x
        mov al, [esi+ecx]
        dec byte [esi]
        movzx edx, byte [edi]
        mov [edi+1+edx], al
        inc byte [edi]
        jmp .b
.x:
        popad
        ret

; un movimiento: EAX=de qué pila EBX=a cuál ECX=cuántas. Destapa la carta que
; queda encima, lo apunta para deshacer y mira si se ha ganado
hacer_mov:
        pushad
        call mover
        mov edx, ecx
        imul esi, eax, PILA_T
        add esi, pilas
        movzx ecx, byte [esi]
        test ecx, ecx
        jz .a
        test byte [esi+ecx], TAPADA
        jz .a
        and byte [esi+ecx], ~TAPADA & 0xFF
        or dl, 0x80                 ; (al deshacer se vuelve a tapar)
.a:
        shl edx, 8                  ; el apunte: 1, de, a, cuántas
        or dl, bl
        shl edx, 8
        or dl, al
        shl edx, 8
        or dl, 1
        call des_apuntar
        call tras_mov
        popad
        ret

; EDX=un paso, a la lista de deshacer (si está llena, se pierde el más viejo)
des_apuntar:
        push eax
        push ecx
        push esi
        push edi
        mov eax, [des_n]
        cmp eax, DES_MAX
        jb .p
        mov esi, des + 4
        mov edi, des
        mov ecx, DES_MAX - 1
        rep movsd
        dec eax
.p:
        mov [des+eax*4], edx
        inc eax
        mov [des_n], eax
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; EBX -> los récords de la baraja de ahora (jugadas, ganadas, tiempo, movs.)
rec_de:
        mov ebx, [baraja]
        shl ebx, 4
        add ebx, recs
        ret

; después de un movimiento: la cuenta, el reloj y el final
tras_mov:
        pushad
        mov ebp, [ven]
        call rec_de
        inc dword [movs]
        cmp dword [estado], E_LISTO
        jne .j
        mov dword [estado], E_JUEGA ; el primero: empieza el tiempo
        mov ecx, 1
        mov edx, 1000
        HV tmp_poner
        inc dword [ebx]
        call rec_guardar
.j:
        mov al, [pilas+P_SAL*PILA_T]
        add al, [pilas+(P_SAL+1)*PILA_T]
        add al, [pilas+(P_SAL+2)*PILA_T]
        add al, [pilas+(P_SAL+3)*PILA_T]
        movzx eax, al
        cmp eax, [n_cartas]
        jne .inv
        mov dword [estado], E_GANADA
        mov ecx, 1
        HV tmp_quitar
        inc dword [ebx+4]
        mov byte [nuevo_rec], 0
        mov eax, [segundos]
        test eax, eax
        jnz .t0
        inc eax
        mov [segundos], eax
.t0:
        cmp dword [ebx+8], 0
        je .t1
        cmp eax, [ebx+8]
        jae .m0
.t1:
        mov [ebx+8], eax
        or byte [nuevo_rec], 1
.m0:
        mov eax, [movs]
        cmp dword [ebx+12], 0
        je .m1
        cmp eax, [ebx+12]
        jae .g
.m1:
        mov [ebx+12], eax
        or byte [nuevo_rec], 2
.g:
        call rec_guardar
        mov eax, WM_FINAL           ; el aviso, cuando se haya pintado
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
.inv:
        HV ven_invalidar
        popad
        ret

; deshace el último paso
deshacer:
        pushad
        cmp dword [estado], E_GANADA
        je .x
        cmp byte [arr_on], 0
        jne .x
        mov eax, [des_n]
        test eax, eax
        jz .x
        dec eax
        mov [des_n], eax
        mov edx, [des+eax*4]
        cmp dl, 3
        jne .m
        mov eax, P_MAZO             ; la vuelta del descarte
        mov ebx, P_DESC
        call volcar
        jmp .f
.m:
        movzx eax, dh               ; de dónde salieron
        shr edx, 16
        movzx ebx, dl               ; dónde están
        movzx ecx, dh               ; cuántas (y si se destapó una)
        test cl, 0x80
        jz .m1
        and ecx, 0x7F
        imul esi, eax, PILA_T
        movzx edi, byte [pilas+esi]
        or byte [pilas+esi+edi], TAPADA
.m1:
        xchg eax, ebx
        call mover
.f:
        dec dword [movs]
        mov ebp, [ven]
        HV ven_invalidar
.x:
        popad
        ret

; clic en el mazo: una carta al descarte o, si no quedan, el descarte al mazo
pasar:
        pushad
        cmp dword [estado], E_GANADA
        je .x
        cmp byte [arr_on], 0
        jne .x
        cmp byte [pilas+P_MAZO*PILA_T], 0
        je .vuelta
        mov eax, P_MAZO
        mov ebx, P_DESC
        mov ecx, 1
        call hacer_mov
        jmp .x
.vuelta:
        cmp byte [pilas+P_DESC*PILA_T], 0
        je .x
        mov eax, P_DESC
        mov ebx, P_MAZO
        call volcar
        mov edx, 3
        call des_apuntar
        call tras_mov
.x:
        popad
        ret

; EAX=pila: su carta de encima sube a su montón, si le toca; CF=1 si no
subir:
        pushad
        imul esi, eax, PILA_T
        add esi, pilas
        movzx ecx, byte [esi]
        test ecx, ecx
        jz .no
        mov dl, [esi+ecx]
        push eax
        mov al, dl
        call puede_sal              ; EBX=el montón
        pop eax
        jc .no
        mov ecx, 1
        call hacer_mov
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; sube todas las que se puedan, del descarte y de las columnas
subir_todas:
        pushad
        cmp byte [arr_on], 0
        jne .x
.otra:
        cmp dword [estado], E_GANADA
        je .x
        xor edi, edi
        mov eax, P_DESC
        call subir
        jc .c
        inc edi
.c:
        mov eax, P_COL
.c1:
        cmp dword [estado], E_GANADA
        je .x
        call subir
        jc .c2
        inc edi
.c2:
        inc eax
        mov edx, [n_cols]
        add edx, P_COL
        cmp eax, edx
        jb .c1
        test edi, edi
        jnz .otra
.x:
        popad
        ret

; se sueltan las cartas que se llevaban: a la columna o al montón donde caen
soltar:
        pushad
        imul esi, [arr_pila], PILA_T
        add esi, pilas
        mov ecx, [arr_desde]
        mov cl, [esi+1+ecx]         ; CL=la de abajo de las que se llevan
        mov edx, [arr_y]
        add edx, CAR_AL / 2
        cmp edx, COL_Y - 6
        jge .col
        mov eax, [arr_x]            ; arriba: a su montón de salida
        add eax, CAR_AN / 2 - GAP / 2
        js .x
        xor edx, edx
        mov ebx, PASO
        div ebx
        cmp eax, 3
        jb .x
        cmp eax, N_COL - 1
        ja .x
        cmp dword [arr_n], 1
        jne .x
        mov al, cl
        call puede_sal
        jc .x
        jmp .mov
.col:
        mov eax, [arr_x]
        add eax, CAR_AN / 2 + GAP / 2
        sub eax, [col_x0]
        js .x
        xor edx, edx
        mov ebx, PASO
        div ebx
        cmp eax, [n_cols]
        jae .x
        lea ebx, [eax+P_COL]
        cmp ebx, [arr_pila]
        je .x
        mov al, cl
        call puede_col
        jc .x
.mov:
        mov eax, [arr_pila]
        mov ecx, [arr_n]
        call hacer_mov
.x:
        popad
        ret

; ---- partida nueva ----
nueva:
        pushad
        mov ebp, [ven]
        mov edi, pilas
        mov ecx, N_PILAS * PILA_T
        xor al, al
        rep stosb
        xor eax, eax
        mov [movs], eax
        mov [segundos], eax
        mov [des_n], eax
        mov [arr_on], al
        mov [estado], eax
        mov ecx, 1
        HV tmp_quitar
        mov ebx, [baraja]
        cmp byte [a_prueba], 0
        jne .prueba
        xor ecx, ecx                ; se baraja
.i:
        mov [mezcla+ecx], cl
        inc ecx
        cmp ecx, [n_cartas]
        jb .i
        dec ecx
.s:
        lea eax, [ecx+1]
        HV azar
        mov dl, [mezcla+ecx]
        xchg dl, [mezcla+eax]
        mov [mezcla+ecx], dl
        dec ecx
        jnz .s
        mov esi, mezcla             ; y se reparte: 1, 2... 7 (o 6) cartas
        xor ebx, ebx
.c:
        imul edi, ebx, PILA_T
        add edi, pilas + P_COL * PILA_T
        lea ecx, [ebx+1]
        mov [edi], cl
        inc edi
.c1:
        lodsb
        cmp ecx, 1
        je .c2
        or al, TAPADA
.c2:
        stosb
        dec ecx
        jnz .c1
        inc ebx
        cmp ebx, [n_cols]
        jb .c
        mov edi, pilas + P_MAZO * PILA_T
        mov eax, [baraja]
        mov ecx, [b_mazo+eax*4]
        mov [edi], cl
        inc edi
        rep movsb
        jmp .f
.prueba:
        mov esi, [b_prueba+ebx*4]
.p:
        lodsb
        cmp al, 0xFF
        je .f
        movzx edi, al
        imul edi, PILA_T
        add edi, pilas
        lodsb
        movzx ecx, al
        stosb
        rep movsb
        jmp .p
.f:
        HV ven_invalidar
        popad
        ret

; ---- los récords y la baraja ----
rec_guardar:
        pushad
        mov edi, txt
        mov esi, k_cab
        call hvl_cad
        mov esi, k_baraja
        call hvl_cad
        mov esi, k_fra
        cmp dword [baraja], B_ESP
        jne .b
        mov esi, k_esp
.b:
        call hvl_cad
        mov ax, 0x0A0D
        stosw
        mov ebx, recs + 16          ; los de la española (las claves de siempre)
        mov ebp, k_claves + 16
        call .baraja
        mov ebx, recs               ; y los de la francesa
        mov ebp, k_claves
        call .baraja
        mov ecx, edi
        mov edx, txt
        sub ecx, edx
        mov esi, ruta_rec
        call hvl_escribir
        popad
        ret
.baraja:
        xor ecx, ecx
.f:
        mov esi, [ebp+ecx*4]
        call hvl_cad
        mov eax, [ebx+ecx*4]
        call hvl_num
        mov ax, 0x0A0D
        stosw
        inc ecx
        cmp ecx, 4
        jb .f
        ret

rec_leer:
        pushad
        mov esi, ruta_rec
        mov edi, txt
        mov ecx, 400
        call hvl_leer
        test eax, eax
        jz .x
        xor ecx, ecx
.f:
        mov esi, txt
        mov edi, [k_claves+ecx*4]
        call hvl_clave
        jc .f1
        call hvl_lee_num
        mov [recs+ecx*4], eax
.f1:
        inc ecx
        cmp ecx, 8
        jb .f
        mov esi, txt
        mov edi, k_baraja
        call hvl_clave
        jc .x
        mov al, [esi]
        or al, 0x20
        cmp al, 'e'
        jne .x
        mov dword [baraja], B_ESP
.x:
        popad
        ret

; ---- la baraja elegida: sus medidas, la marca del menú y sus dibujos ----
poner_baraja:
        pushad
        mov eax, [baraja]
        mov ebx, [b_ord+eax*4]
        mov [n_ord], ebx
        mov ebx, [b_cartas+eax*4]
        mov [n_cartas], ebx
        mov ebx, [b_cols+eax*4]
        mov [n_cols], ebx
        mov ebx, [b_col_x0+eax*4]
        mov [col_x0], ebx
        mov byte [m_fra], '»'
        mov byte [m_esp], ' '
        test eax, eax
        jz .m
        mov byte [m_fra], ' '
        mov byte [m_esp], '»'
.m:
        ; las pintas: normales, del revés, al doble y al triple
        mov esi, [b_pip+eax*4]
        xor ebp, ebp
.p:
        mov dl, [b_color+eax*2]     ; (las francesas, de su color)
        cmp ebp, 2
        jb .p1
        mov dl, [b_color+eax*2+1]
.p1:
        imul edi, ebp, PIP_T
        add edi, pip13
        mov ecx, PIP_T
        call convertir              ; (ESI avanza)
        push esi
        mov esi, edi
        imul edi, ebp, PIP_T
        add edi, pip13r
        call girar
        imul edi, ebp, P26_T
        add edi, pip26
        mov ebx, 13
        mov ecx, 13
        mov edx, 2
        call ampliar
        imul edi, ebp, AS_T
        add edi, pip39
        mov edx, 3
        call ampliar
        pop esi
        inc ebp
        cmp ebp, 4
        jb .p
        ; las pequeñas, normales y del revés
        mov esi, [b_peq+eax*4]
        xor ebp, ebp
.q:
        mov dl, [b_color+eax*2]
        cmp ebp, 2
        jb .q1
        mov dl, [b_color+eax*2+1]
.q1:
        imul edi, ebp, PEQ_T
        add edi, pip9
        mov ecx, PEQ_T
        call convertir
        push esi
        mov esi, edi
        imul edi, ebp, PEQ_T
        add edi, pip9r
        call girar
        pop esi
        inc ebp
        cmp ebp, 4
        jb .q
        call hacer_indices
        call hacer_figuras
        popad
        ret

; los índices de las cartas, con la letra de Hispa Ventanas en negrita: para
; cada color (rojo/claro y negro/oscuro) y cada orden, derecho y del revés
hacer_indices:
        pushad
        mov eax, [baraja]
        mov ebx, [b_num+eax*4]      ; EBX=lo que se escribe
        xor ebp, ebp                ; EBP=color*13 + orden
.i:
        mov eax, ebp
        xor edx, edx
        mov ecx, 13
        div ecx                     ; EAX=color EDX=orden
        cmp edx, [n_ord]
        jae .sig
        mov ecx, [baraja]
        mov ah, [b_color+ecx*2+eax]
        imul edi, ebp, IDX_T
        add edi, idx
        push edi
        push eax
        mov ecx, IDX_T
        mov al, CLAVE
        rep stosb
        pop eax
        pop edi
        lea esi, [edx*2+edx]
        add esi, ebx                ; ESI=el texto (uno o dos caracteres)
        mov ecx, 3                  ; uno solo, centrado
        cmp byte [esi+1], 0
        je .c
        xor ecx, ecx
.c:
        movzx edx, byte [esi]
        test edx, edx
        jz .gira
        call idx_letra
        add ecx, 6                  ; (las dos cifras, juntas)
        inc esi
        jmp .c
.gira:
        mov esi, edi
        imul edi, ebp, IDX_T
        add edi, idx_r
        mov ecx, IDX_T
        call girar
.sig:
        inc ebp
        cmp ebp, 2 * 13
        jb .i
        popad
        ret

; (de hacer_indices) EDX=carácter ECX=columna AH=color EDI=el índice
idx_letra:
        pushad
        shl edx, 4
        add edx, [hv_api + HV_fuente8x16 * 4]
        add edx, 3                  ; las mayúsculas y las cifras: filas 3 a 11
        add edi, ecx
        mov ebx, IDX_AL
.f:
        movzx esi, byte [edx]
        mov ecx, esi
        shr ecx, 1
        or esi, ecx                 ; (la negrita: cada punto, también a la derecha)
        xor ecx, ecx
.p:
        mov eax, 0x80
        shr eax, cl
        test esi, eax
        jz .s
        mov al, [esp+29]            ; (AH de la entrada)
        mov [edi+ecx], al
.s:
        inc ecx
        cmp ecx, 8
        jb .p
        inc edx
        add edi, IDX_AN
        dec ebx
        jnz .f
        popad
        ret

; las figuras de cada palo, con sus colores, en su recuadro (38x82)
hacer_figuras:
        pushad
        xor ebp, ebp                ; EBP=palo*3 + figura
.f:
        imul edi, ebp, FIG_T
        add edi, figs
        push edi
        mov ecx, FIG_T
        mov al, CLAVE
        rep stosb
        pop edi
        mov eax, ebp
        xor edx, edx
        mov ecx, 3
        div ecx                     ; EAX=palo EDX=figura
        mov ecx, [baraja]
        mov esi, [b_figcol+ecx*4]
        add esi, eax
        lea esi, [esi+eax*2]
        push eax
        mov dx, [esi]               ; DL, DH, BL=los colores x, y, z
        mov bl, [esi+2]
        mov eax, ebp
        push edx
        xor edx, edx
        mov ecx, 3
        div ecx
        mov eax, edx                ; EAX=figura
        pop edx
        cmp dword [baraja], B_ESP
        je .esp
        imul esi, eax, 19 * 20      ; la francesa: la mitad de arriba...
        add esi, d_figf
        push edi
        mov edi, fig_tmp
        mov ecx, 19 * 20
        call convertir
        pop edi
        mov esi, fig_tmp
        mov ebx, 19
        mov ecx, 20
        mov edx, 2
        call ampliar
        mov esi, edi                ; ...y la de abajo, girada
        add edi, 42 * FIG_AN
        mov ecx, 40 * FIG_AN
        call girar
        pop eax
        jmp .sig
.esp:
        imul esi, eax, 19 * 26      ; la española: la figura...
        add esi, d_fige
        push edi
        mov edi, fig_tmp
        mov ecx, 19 * 26
        call convertir
        pop edi
        mov esi, fig_tmp
        mov ebx, 19
        mov ecx, 26
        mov edx, 2
        call ampliar
        pop eax                     ; ...y debajo, su palo al doble
        imul esi, eax, P26_T
        add esi, pip26
        add edi, 56 * FIG_AN + (FIG_AN - 26) / 2
        mov edx, 26
.e:
        mov ecx, 26
        rep movsb
        add edi, FIG_AN - 26
        dec edx
        jnz .e
.sig:
        inc ebp
        cmp ebp, 12
        jb .f
        popad
        ret

; ---- los dibujos de siempre (una vez, al empezar): la cara y el dorso ----
preparar:
        pushad
        mov edi, cara
        mov al, C_BLANCO
        call hacer_carta
        mov edi, dorso
        call hacer_carta
        mov edi, dorso + 4 * CAR_AN + 4   ; el dorso: dentro del margen blanco
        mov ebx, 4                  ; EBX=y
.f:
        mov ecx, 4                  ; ECX=x
.p:
        mov eax, ecx                ; EDX=|x-36| + |y-50|: el rombo del centro
        sub eax, CAR_AN / 2
        jns .p1
        neg eax
.p1:
        mov edx, ebx
        sub edx, CAR_AL / 2
        jns .p2
        neg edx
.p2:
        add edx, eax
        mov al, C_OCRE
        cmp edx, 4
        jbe .w
        mov al, C_ANIL
        cmp edx, 10
        jbe .w
        mov al, C_OCRE
        cmp edx, 13
        jbe .w
        mov eax, ecx                ; la celosía
        add eax, ebx
        test al, 7
        jz .cel
        mov eax, ecx
        sub eax, ebx
        test al, 7
        jz .cel
        mov al, C_ANIL
        jmp .w
.cel:
        mov al, C_TURQUESA
.w:
        mov [edi], al
        inc edi
        inc ecx
        cmp ecx, CAR_AN - 4
        jb .p
        add edi, 8
        inc ebx
        cmp ebx, CAR_AL - 4
        jb .f
        popad
        ret

; EDI=una carta (CAR_AN x CAR_AL) AL=su color: las esquinas redondeadas y el
; filete gris
hacer_carta:
        pushad
        mov [hc_color], al
        xor ebx, ebx                ; EBX=y
.f:
        mov eax, ebx                ; lo cerca que está de arriba o de abajo
        mov edx, CAR_AL - 1
        sub edx, ebx
        cmp edx, eax
        jae .f1
        mov eax, edx
.f1:
        xor esi, esi                ; ESI=puntos de fuera EBP=del filete
        mov ebp, 1
        cmp eax, 4
        jae .f2
        movzx esi, byte [esq_fuera+eax]
        movzx ebp, byte [esq_filete+eax]
.f2:
        xor ecx, ecx                ; ECX=x
.p:
        mov eax, ecx
        mov edx, CAR_AN - 1
        sub edx, ecx
        cmp edx, eax
        jae .p1
        mov eax, edx
.p1:
        mov dl, CLAVE
        cmp eax, esi
        jb .w
        mov dl, GRIS
        sub eax, esi
        cmp eax, ebp
        jb .w
        mov dl, [hc_color]
.w:
        mov [edi], dl
        inc edi
        inc ecx
        cmp ecx, CAR_AN
        jb .p
        inc ebx
        cmp ebx, CAR_AL
        jb .f
        popad
        ret

; ESI=dibujo en texto EDI=mapa ECX=puntos DL, DH, BL=los colores «x», «y» y
; «z» (ESI avanza)
convertir:
        push eax
        push ecx
        push edi
.b:
        lodsb
        cmp al, '.'
        jne .c1
        mov al, CLAVE
        jmp .p
.c1:
        cmp al, 'x'
        jne .c2
        mov al, dl
        jmp .p
.c2:
        cmp al, 'y'
        jne .c3
        mov al, dh
        jmp .p
.c3:
        cmp al, 'z'
        jne .c4
        mov al, bl
        jmp .p
.c4:
        cmp al, 'r'
        jne .c5
        mov al, ROJO
        jmp .p
.c5:
        cmp al, 'n'
        jne .c6
        mov al, NEGRO
        jmp .p
.c6:
        cmp al, 'p'
        jne .c7
        mov al, PIEL
        jmp .p
.c7:
        cmp al, 'a'
        jb .c8
        sub al, 'a' - 10
        jmp .p
.c8:
        sub al, '0'
.p:
        stosb
        dec ecx
        jnz .b
        pop edi
        pop ecx
        pop eax
        ret

; ESI=mapa de ECX puntos -> EDI=el mismo, girado media vuelta (al revés)
girar:
        pushad
        add esi, ecx
.b:
        dec esi
        mov al, [esi]
        stosb
        dec ecx
        jnz .b
        popad
        ret

; ESI=mapa de EBX x ECX -> EDI=el mismo, EDX veces más grande
ampliar:
        pushad
        mov [amp_n], edx
.f:
        mov edx, [amp_n]            ; cada fila, EDX veces
.r:
        push esi
        push ecx
        mov ecx, ebx
.p:
        lodsb
        push ecx
        mov ecx, [amp_n]
        rep stosb
        pop ecx
        dec ecx
        jnz .p
        pop ecx
        pop esi
        dec edx
        jnz .r
        add esi, ebx
        dec ecx
        jnz .f
        popad
        ret

; ---- el dibujo ----
pintar:
        HV ven_cli                  ; solo las cartas de la zona por repintar
        mov ecx, [ebp+V_UX0]
        sub ecx, eax
        mov [p_x0], ecx
        mov ecx, [ebp+V_UX1]
        sub ecx, eax
        mov [p_x1], ecx
        mov ecx, [ebp+V_UY0]
        sub ecx, ebx
        mov [p_y0], ecx
        mov ecx, [ebp+V_UY1]
        sub ecx, ebx
        mov [p_y1], ecx
        HV dc_pintar
        cmp dword [p_y0], FILA_Y - 4
        jge .pilas
        COLOR C_CREMA               ; los rótulos
        mov edi, rot
        mov esi, t_movs
        call hvl_cad
        mov eax, [movs]
        call hvl_num
        mov eax, GAP
        call .rotulo
        mov edi, rot
        mov esi, t_tiempo
        call hvl_cad
        mov eax, [segundos]
        call pon_tiempo
        mov eax, GAP + 3 * PASO
        call .rotulo
        mov edi, rot
        mov esi, t_mazo
        call hvl_cad
        movzx eax, byte [pilas+P_MAZO*PILA_T]
        call hvl_num
        mov eax, CLI_AN - GAP - 8 * 8
        call .rotulo
.pilas:
        xor edi, edi
.p:
        call dib_pila
        inc edi
        mov eax, [n_cols]
        add eax, P_COL
        cmp edi, eax
        jb .p
        cmp byte [arr_on], 0        ; y, encima de todo, las que se llevan
        je .x
        imul esi, [arr_pila], PILA_T
        add esi, pilas + 1
        add esi, [arr_desde]
        mov eax, [arr_x]
        mov ebx, [arr_y]
        mov edi, [arr_n]
.a:
        mov cl, [esi]
        mov edx, D_VIS
        cmp edi, 1
        jne .a1
        mov edx, CAR_AL
.a1:
        call dib_carta
        inc esi
        add ebx, D_VIS
        dec edi
        jnz .a
.x:
        HV dc_cerrar
        xor eax, eax
        ret
; (interna) acaba rot y lo pinta en EAX, 6
.rotulo:
        mov byte [edi], 0
        mov ebx, 6
        mov esi, rot
        HV dc_texto
        ret

; dibuja la pila EDI (sin las cartas que se llevan con el ratón)
dib_pila:
        pushad
        mov eax, edi
        call pila_pos               ; EAX, EBX=su sitio
        imul esi, edi, PILA_T
        add esi, pilas
        movzx ecx, byte [esi]
        cmp byte [arr_on], 0
        je .n
        cmp edi, [arr_pila]
        jne .n
        sub ecx, [arr_n]
.n:
        mov [d_n], ecx
        test ecx, ecx
        jnz .hay
        call dib_hueco
        jmp .x
.hay:
        cmp edi, P_COL
        jae .col
        mov cl, [esi+ecx]           ; arriba solo se ve la de encima
        mov edx, CAR_AL
        test edi, edi
        jnz .c1
        or cl, TAPADA               ; (el mazo, boca abajo)
.c1:
        call dib_carta
        jmp .x
.col:
        push eax
        call col_paso
        mov [d_paso], eax
        pop eax
        xor edi, edi
.cb:
        mov cl, [esi+1+edi]
        mov edx, CAR_AL
        inc edi
        cmp edi, [d_n]
        jae .cd                     ; la de encima, entera
        mov edx, [d_paso]
        test cl, TAPADA
        jz .cd
        mov edx, D_TAP
.cd:
        call dib_carta
        add ebx, edx
        cmp edi, [d_n]
        jb .cb
.x:
        popad
        ret

; el sitio vacío de la pila EDI, en EAX, EBX
dib_hueco:
        pushad
        COLOR C_PETROLEO_CL
        mov ecx, CAR_AN
        mov edx, CAR_AL
        HV dc_marco
        test edi, edi
        jnz .sal
        add eax, CAR_AN / 2         ; el mazo: un rombo hueco (darle la vuelta)
        add ebx, CAR_AL / 2
        mov ecx, 11
        HV dc_rombo
        COLOR C_PETROLEO
        mov ecx, 7
        HV dc_rombo
        jmp .x
.sal:
        cmp edi, P_SAL
        jb .x
        cmp edi, P_COL
        jae .x
        sub edi, P_SAL              ; un montón de salida: su palo
        imul esi, edi, P26_T
        add esi, pip26
        add eax, (CAR_AN - 26) / 2
        add ebx, (CAR_AL - 26) / 2
        mov ecx, 26
        mov edx, 26
        HV dc_copiar_clave
.x:
        popad
        ret

; (de dib_carta) ESI=mapa ECX x EDX en c_x + EAX, c_y + EBX
copia:
        add eax, [c_x]
        add ebx, [c_y]
        HV dc_copiar_clave
        ret

; dibuja una carta: EAX=x EBX=y CL=carta EDX=el alto que se ve (CAR_AL: entera)
dib_carta:
        pushad
        cmp eax, [p_x1]             ; ¿cae en lo que hay que repintar?
        jge .x
        cmp ebx, [p_y1]
        jge .x
        lea esi, [eax+CAR_AN]
        cmp esi, [p_x0]
        jle .x
        lea esi, [ebx+edx+4]        ; (asoma también por las esquinas de la
        cmp esi, [p_y0]             ; de encima)
        jle .x
        mov [c_x], eax
        mov [c_y], ebx
        mov [c_h], edx
        movzx edi, cl
        add edx, 4                  ; el fondo: cara o dorso
        cmp edx, CAR_AL
        jbe .h
        mov edx, CAR_AL
.h:
        mov ecx, CAR_AN
        mov esi, cara
        test edi, TAPADA
        jz .f
        mov esi, dorso
.f:
        HV dc_copiar_clave
        test edi, TAPADA
        jnz .x
        mov eax, edi
        call palo_orden             ; EAX=palo EDX=orden
        mov [c_palo], eax
        mov [c_ord], edx
        shr eax, 1                  ; el índice: rojo o negro
        imul eax, 13
        add eax, edx
        imul eax, IDX_T
        mov [c_idx], eax
        lea esi, [eax+idx]
        mov eax, 2
        mov ebx, 4
        mov ecx, IDX_AN
        mov edx, IDX_AL
        call copia
        imul esi, [c_palo], PEQ_T   ; y la pinta pequeña
        add esi, pip9
        mov eax, 4
        mov ebx, 15
        mov ecx, 9
        mov edx, 9
        call copia
        cmp dword [c_h], CAR_AL
        jne .x
        mov esi, [c_idx]            ; el de abajo, del revés
        add esi, idx_r
        mov eax, CAR_AN - 2 - IDX_AN
        mov ebx, CAR_AL - 4 - IDX_AL
        mov ecx, IDX_AN
        mov edx, IDX_AL
        call copia
        imul esi, [c_palo], PEQ_T
        add esi, pip9r
        mov eax, CAR_AN - 4 - 9
        mov ebx, CAR_AL - 15 - 9
        mov ecx, 9
        mov edx, 9
        call copia
        mov edx, [c_ord]
        mov eax, [n_ord]
        sub eax, 3
        cmp edx, eax
        jae .figura
        test edx, edx
        jnz .pintas
        imul esi, [c_palo], AS_T    ; el as: el palo, grande
        add esi, pip39
        mov eax, (CAR_AN - 39) / 2
        mov ebx, (CAR_AL - 39) / 2
        mov ecx, 39
        mov edx, 39
        call copia
        jmp .x
.pintas:
        mov esi, [t_pintas+edx*4-4]
        movzx edi, byte [esi]
        inc esi
.pb:
        movzx eax, byte [esi]
        movzx ebx, byte [esi+1]
        add esi, 2
        push esi
        imul esi, [c_palo], PIP_T
        btr eax, 7                  ; (+128: del revés)
        jnc .pd
        add esi, pip13r - pip13
.pd:
        add esi, pip13
        mov ecx, 13
        mov edx, 13
        call copia
        pop esi
        dec edi
        jnz .pb
        jmp .x
.figura:
        sub edx, eax                ; EDX=0 jota/sota, 1 dama/caballo, 2 rey
        push edx
        mov eax, [c_palo]           ; el recuadro, del color del palo
        shr eax, 1
        mov ecx, [baraja]
        movzx eax, byte [b_color+ecx*2+eax]
        COLOR eax
        mov eax, [c_x]
        add eax, 16
        mov ebx, [c_y]
        add ebx, 8
        mov ecx, FIG_AN + 2
        mov edx, FIG_AL + 2
        HV dc_marco
        pop edx
        mov eax, [c_palo]
        lea eax, [eax+eax*2]
        add eax, edx
        imul esi, eax, FIG_T
        add esi, figs
        mov eax, 17
        mov ebx, 9
        mov ecx, FIG_AN
        mov edx, FIG_AL
        call copia
.x:
        popad
        ret

%include "hvplib.inc"

        section .bss
ven         resd 1                  ; la ventana
estado      resd 1
movs        resd 1
segundos    resd 1
des_n       resd 1                  ; pasos apuntados para deshacer
arr_pila    resd 1                  ; lo que se lleva con el ratón: de qué pila,
arr_desde   resd 1                  ; desde qué carta, cuántas,
arr_n       resd 1
arr_x       resd 1                  ; dónde están ahora
arr_y       resd 1
arr_dx      resd 1                  ; y por dónde se cogieron
arr_dy      resd 1
g_x         resd 1                  ; (donde) el sitio de la carta
g_y         resd 1
p_x0        resd 1                  ; lo que hay que repintar
p_y0        resd 1
p_x1        resd 1
p_y1        resd 1
d_n         resd 1
d_paso      resd 1
c_x         resd 1                  ; la carta que se está dibujando
c_y         resd 1
c_h         resd 1
c_palo      resd 1
c_ord       resd 1
c_idx       resd 1
amp_n       resd 1
baraja      resd 1                  ; B_FRA (0, la de por omisión) o B_ESP
n_ord       resd 1                  ; y sus medidas
n_cartas    resd 1
n_cols      resd 1
col_x0      resd 1
recs        resd 8                  ; los récords: jugadas, ganadas, tiempo y
                                    ; movimientos de la francesa y la española
arr_on      resb 1
a_prueba    resb 1
nuevo_rec   resb 1                  ; 1 el tiempo, 2 los movimientos
hc_color    resb 1
ruta_rec    resb 80
rot         resb 32                 ; el rótulo que se está pintando
txt         resb 404
mezcla      resb 52
pilas       resb N_PILAS * PILA_T
des         resd DES_MAX
pip13       resb 4 * PIP_T          ; las pintas
pip13r      resb 4 * PIP_T          ; del revés
pip26       resb 4 * P26_T          ; al doble
pip39       resb 4 * AS_T           ; al triple
pip9        resb 4 * PEQ_T          ; las pequeñas
pip9r       resb 4 * PEQ_T
idx         resb 2 * 13 * IDX_T     ; los índices (color, orden)
idx_r       resb 2 * 13 * IDX_T
fig_tmp     resb 19 * 26
figs        resb 12 * FIG_T         ; las figuras de cada palo
cara        resb CARTA_T            ; la carta en blanco
dorso       resb CARTA_T
HVP_FIN
