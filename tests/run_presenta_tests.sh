#!/bin/bash
# Prueba de Presentaciones (PRESENTA.HVP), el programa de diapositivas, del
# grupo «Productividad».
#
# Abre Hispa Ventanas con Presentaciones (clave «Reabrir=» de CENTRO.INI) y,
# por fases (cada una arranca de nuevo):
#   1 la ventana y la tira de miniaturas: añadir diapositivas, ir de una a
#     otra, cambiarlas de sitio (Ctrl y las flechas, arrastrando), duplicar,
#     quitar y deshacer;
#   2 los diseños, el texto de ejemplo de las cajas, las listas con viñetas y
#     niveles (Tab) y el hueco de una imagen;
#   3 las plantillas de presentación y los temas;
#   4 la presentación a pantalla completa: avanzar, retroceder, ir a un
#     número, los puntos de uno en uno, las transiciones y el final;
#   5 las notas del orador, guardar y abrir, exportar a BMP e imprimir (una
#     por página, folletos de 2, 3 y 6, y las notas);
#   6 cambiar el diseño de una diapositiva, las listas con números, escribir
#     las notas en su recuadro y el clasificador.
# Lo que queda en cada .PRE guardado se comprueba leyendo el archivo.
#
# uso: run_presenta_tests.sh [IMAGEN]      (FASES="2 5" corre solo esas)
# Necesita qemu-system-i386, mtools, socat e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
HVP=$B/PRESENTA.HVP
NASM=${NASM:-nasm} python3 tools/mkhvp.py programas/presenta.asm $HVP > /dev/null || { echo "  FAIL no se ensambla"; exit 1; }
FASES=${FASES:-1 2 3 4 5 6}
fase() { case " $FASES " in *" $1 "*) return 0;; esac; return 1; }
reabre() { printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\PRESENTA.HVP %s\r\n' "$1" > $T/CENTRO.INI
           mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI; }
aparca() { ir 796 400; }                                    # (el ratón, fuera de la ventana: no tapa el pie)

# ---- la ventana: en (8, 30); su zona empieza en (10, 75) ----
# La diapositiva (800 x 600) entera se ve al 58 % (148/256) desde (231, 142);
# al 100 %, el punto (x, y) está en la pantalla en (158+x, 141+y).
EX=231; EY=142
e() { echo $(( EX + $1 * 148 / 256 )) $(( EY + $2 * 148 / 256 )); }     # un punto de la diapositiva, vista entera
ec() { clic $(e $1 $2); }
PX=158; PY=141
pc() { clic $((PX + $1)) $((PY + $2)); }
# (las dos filas de botones van centradas en la ventana: 38 puntos más allá)
boton1() { clic $((48 + $1 + 12)) 89; }                     # un botón de la fila de arriba (su x)
boton2() { clic $((48 + $1 + 11)) 117; }
B_VISTA=522; B_NUEVA=472; B_PRES=572; B_VIN=648
mini() { clic 88 $((183 + 100 * ($1 - 1))); }               # un clic en la miniatura de la diapositiva N
menu() { clic $1 65; sleep 0.9; clic $(( $1 + 30 )) $2; sleep 0.9; }
M_ARCH=50; M_EDIC=123; M_VER=178; M_INS=239; M_FORM=315; M_DISP=390; M_PRES=500
lista() { sleep 1; tecla home; local i; for i in $(seq 1 $1); do tecla down; done; [ "${2:-}" ] && shot $2; tecla ret; sleep 0.8; }
nueva_dia() { boton1 $B_NUEVA; lista $1 ${2:-}; sleep 0.5; }        # una diapositiva nueva con el diseño N
nueva() { tecla ctrl-u; sleep 1.2; local i; for i in $(seq 1 $1); do tecla down; done
          if [ $2 -gt 0 ]; then tecla tab; for i in $(seq 1 $2); do tecla down; done; fi
          [ "${3:-}" ] && shot $3; tecla ret; sleep 3; }    # Nuevo: la plantilla N con el tema M
guarda() { tecla ctrl-g; sleep 1.2; escribe "$1"; tecla ret; sleep 2.5; }
PIE="16 566 40 582 0 6"
# espera N TEXTO CAPTURA: hasta N segundos a que salga ese mensaje (lo que tarda el disquete o la
# impresora no es fijo); la captura queda con ese nombre
espera() { local i; for i in $(seq 1 $1); do sleep 1; mon "screendump $T/$3.ppm" 0.5
             $CHK $T/$3.ppm busca 100 200 700 400 0 8 "$2" > /dev/null 2>&1 && return 0; done; return 1; }

# lee un .PRE y comprueba una condición en Python: o[i] son sus objetos
cat > $T/pre.py <<'PY'
import sys, struct
b = open(sys.argv[1], 'rb').read()
assert b[:8] == b'PRESENT\x1a', 'no es un .PRE'
an, al, margen, pags, n, tam, nd = struct.unpack_from('<HHHHHHI', b, 8)
resto = len(b) - (24 + n * tam + nd)
assert resto in (0, 4), 'el tamaño del archivo no cuadra'
tema, trans, pasos = (b[-4], b[-3], b[-2]) if resto else (0, 0, 0)
class O: pass
o = []
for i in range(n):
    r = O()
    (r.tipo, r.pag, r.estilo, r.alin, r.x, r.y, r.an, r.al, r.tinta, r.fondo, r.borde, r.grosor,
     r.letra, r.cuerpo, r.ian, r.ial, r.papel, r.band, dat, largo) = struct.unpack_from('<BBBBiiiiBBBBBBHHBBII', b, 24 + i * tam)
    r.dat = b[24 + n * tam + dat:24 + n * tam + dat + largo]
    r.txt = r.dat.decode('cp437')
    r.caja = (r.x, r.y, r.an, r.al)
    r.grupo, r.cadena, r.eslabon = b[24 + i * tam + 40:24 + i * tam + 43] if tam >= 48 else (0, 0, 0)
    o.append(r)
TEXTO, RECT, ELIPSE, LINEA, IMAGEN = 1, 2, 3, 4, 5
TITULO, CUERPO, CUERPO2, SUBTIT, HUECO, NOTAS = 1, 2, 3, 4, 5, 9
NADA = 255
def de(pag, papel):
    """El texto de la caja con ese papel en esa diapositiva (None si no la hay)."""
    for r in o:
        if r.pag == pag and r.papel == papel: return r.txt
titulos = [de(p, TITULO) for p in range(pags)]
if len(sys.argv) > 3:
    print('   ', (an, al, pags, n), 'tema', tema, trans, pasos)
    for r in o: print('   ', r.tipo, r.pag, r.caja, r.papel, r.band, r.tinta, r.letra, r.cuerpo, repr(r.txt[:40]))
sys.exit(0 if eval(sys.argv[2]) else 1)
PY
mira_pre() {
  rm -f $T/m.pre; mcopy -n -i $T/f.img ::$1 $T/m.pre 2>/dev/null
  if python3 $T/pre.py $T/m.pre "$3" 2>/dev/null; then ok "$1: $2"
  else ko "$1: $2"; python3 $T/pre.py $T/m.pre "True" ver 2>&1 | head -40; fi
}

if fase 1; then
echo "== Presentaciones: la ventana y la tira de miniaturas =="
prepara $HVP
{ arranca
  shot v0
  ir 630 89; shot v1                                        # el ratón sobre el botón de presentar
  ec 400 230; escribe "Hola"; aparca; shot v2; tecla esc    # un clic en la caja del título: se escribe
  nueva_dia 1; aparca; shot v3                              # otra diapositiva (título y texto)
  ec 200 60; escribe "Dos"; tecla esc
  nueva_dia 3; ec 200 60; escribe "Tres"; tecla esc; aparca; shot v4      # y otra (solo título)
  mini 1; aparca; shot v5                                   # un clic en la primera miniatura
  tecla ctrl-right; aparca; shot v6                         # Ctrl+derecha: baja un puesto (Dos, Hola, Tres)
  ir 88 383; mon "mouse_button 1" 0.3; ir 88 300; ir 88 190; shot v7m; mon "mouse_button 0" 0.6   # la tercera, arrastrada arriba del todo
  aparca; shot v7
  menu $M_EDIC 291; aparca; shot v8                         # Edición > Duplicar la diapositiva
  menu $M_EDIC 311; shot v9; tecla ret; sleep 0.8; aparca; shot v10       # Edición > Quitar esta diapositiva
  tecla ctrl-z; aparca; shot v11                            # deshacer: vuelve
  tecla pgdn pgdn ctrl-left; aparca; shot v12               # a la cuarta (Hola); Ctrl+izquierda la sube
  guarda tira
  clic 554 65; sleep 0.9; clic 584 87; sleep 1.2; shot v13; tecla ret     # ? > Acerca de
  sleep 0.8; clic 763 43; sleep 1; aparca; shot v14         # el botón de maximizar
  sal; } | run 240 -vga std
chk $T/v0.ppm titulo 8 30 activa "Presentaciones - (sin nombre)"
# la diapositiva (en lo que deja la tira) y las dos filas de botones, en el
# centro: en la ventana corriente y maximizada
chk $T/v0.ppm centrado 150 774 400 8 1 "la diapositiva" centrado 10 790 117 6 1 "los botones" \
              alineado 10 790 89 117 6 "las dos filas de botones empiezan en el mismo sitio"
chk $T/v14.ppm titulo 0 28 activa "Presentaciones - tira.PRE"
chk $T/v14.ppm centrado 142 782 400 8 1 "maximizada: la diapositiva" centrado 2 798 115 6 1 "maximizada: los botones" \
               alineado 2 798 87 115 6 "maximizada: las dos filas de botones empiezan en el mismo sitio"
chk $T/v0.ppm busca $PIE "Diapositiva 1 de 1    Tema: Claro    Vista: 58 %"
chk $T/v0.ppm px 400 200 9 "la diapositiva, blanca" px 400 480 2 "la franja del tema" px 250 480 10 "y su tramo ocre" \
              px 700 540 9 "el recuadro de las notas" px 30 150 2 "la miniatura de la que se ve, con su marco"
chk $T/v0.ppm busca 215 514 225 524 7 9 "(haga clic aquí para escribir las notas del orador)"
# la miniatura no enseña los textos de ejemplo de las cajas vacías: donde caen el título y el
# subtítulo está tan en blanco como arriba, donde no hay nada
chk $T/v0.ppm igual 40 165 96 16 $T/v0.ppm 40 143 0
chk $T/v0.ppm igual 40 183 96 12 $T/v0.ppm 40 143 0
chk $T/v1.ppm busca $PIE "Presentar a pantalla completa desde el principio (F5)"
chk $T/v2.ppm busca $PIE "Diapositiva 1 de 1    Caja de texto: escriba; Esc o un clic fuera para acabar"
chk $T/v3.ppm busca $PIE "Diapositiva 2 de 2    Tema: Claro    Vista: 58 %"
chk $T/v3.ppm px 30 250 2 "la segunda miniatura, con el marco" px 30 150 15 "y la primera, ya no"
chk $T/v4.ppm busca $PIE "Diapositiva 3 de 3"
chk $T/v5.ppm busca $PIE "Diapositiva 1 de 3"
chk $T/v6.ppm busca $PIE "Diapositiva 2 de 3"
chk $T/v7m.ppm px 88 134 10 "arrastrando una miniatura: la raya de dónde va"
chk $T/v7.ppm busca $PIE "Diapositiva 1 de 3"
chk $T/v8.ppm busca $PIE "Diapositiva 2 de 4"
chk $T/v9.ppm busca 100 200 700 400 0 8 "Se va a quitar esta diapositiva con todo lo que tiene."
chk $T/v10.ppm busca $PIE "Diapositiva 2 de 3"
chk $T/v11.ppm busca $PIE "Diapositiva 2 de 4"
chk $T/v12.ppm busca $PIE "Diapositiva 3 de 4"
chk $T/v13.ppm busca 150 200 650 400 0 8 "Copyright (C) 2026 Carlos Sanz y contribuidores."
muestra v0 presenta_vacia; muestra v4 presenta_tira; muestra v7m presenta_arrastre; muestra v12 presenta_orden
mira_pre TIRA.PRE "cuatro diapositivas de 800 x 600 en su orden, cada una con su título" \
  "(an, al, pags) == (800, 600, 4) and titulos == ['Tres', 'Tres', 'Hola', 'Dos'] and de(2, SUBTIT) == '' and de(3, CUERPO) == ''"
fi

if fase 2; then
echo "== Presentaciones: los diseños, las listas y el hueco de una imagen =="
prepara $HVP
mkdir -p $T/bmp; python3 tests/tools/pintura_bmp.py crea $T/bmp > /dev/null
mcopy -o -i $T/f.img $T/bmp/T8.BMP ::
{ arranca
  nueva_dia 1 d0                                            # título y texto
  boton1 $B_VISTA; sleep 0.8; aparca; shot d1               # al 100 %
  pc 300 300; escribe "uno"; tecla ret tab; escribe "dos"; tecla ret tab; escribe "tres"
  tecla ret shift-tab shift-tab; escribe "cuatro"; aparca; shot d2; tecla esc    # una lista con tres niveles
  boton2 $B_VIN; aparca; shot d3; boton2 $B_VIN             # sin viñetas, y con ellas otra vez
  boton1 $B_VISTA; sleep 0.8
  nueva_dia 2; aparca; shot d4                              # dos columnas
  ec 600 400; escribe "derecha"; tecla esc
  nueva_dia 5; aparca; shot d5                              # título e imagen
  ir $(e 600 400); mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.12; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.5
  sleep 1; escribe "t8.bmp"; tecla ret; sleep 2.5; tecla esc; aparca; shot d6    # doble clic en el hueco: la imagen
  nueva_dia 0; nueva_dia 4; aparca; shot d7                 # una portada y una en blanco
  guarda disenos
  sal; } | run 240 -vga std
chk $T/d0.ppm busca 200 100 600 500 9 12 "Título y texto"
chk $T/d0.ppm busca 200 100 600 500 0 9 "Título e imagen"
chk $T/d1.ppm busca $PIE "Diapositiva 2 de 2    Tema: Claro    Vista: 100 %"
chk $T/d2.ppm px $((PX+59)) $((PY+153)) 0 "la viñeta del primer punto" px $((PX+91)) $((PY+181)) 0 "la raya del segundo nivel" \
              px $((PX+59)) $((PY+181)) 9 "(con su sangría)" px $((PX+59)) $((PY+225)) 0 "la viñeta del cuarto, otra vez al principio" px 698 108 2 "y el botón de las viñetas, puesto"
chk $T/d3.ppm cambia $((PX+40)) $((PY+132)) 100 110 $T/d2.ppm 50      # sin viñetas, todo va al margen
chk $T/d3.ppm px 698 108 8 "y el botón, sin poner"
chk $T/d4.ppm busca $PIE "Diapositiva 3 de 3"
chk $T/d5.ppm busca $PIE "Diapositiva 4 de 4"
chk $T/d6.ppm px $(e 420 300) 4 "la imagen, en el hueco: la banda terracota" px $(e 420 268) 11 "y arriba, verde oliva" \
              px $(e 408 200) 9 "y el marco del hueco ya no está"
chk $T/d7.ppm busca $PIE "Diapositiva 6 de 6"
muestra d1 presenta_diseno; muestra d2 presenta_lista; muestra d5 presenta_hueco; muestra d6 presenta_imagen
mira_pre DISENOS.PRE "las cajas de cada diseño con su papel, la lista con sus niveles y la imagen en su hueco" \
  "pags == 6 and [sorted(r.papel for r in o if r.pag == p) for p in range(6)] == [[TITULO, SUBTIT], [TITULO, CUERPO], [TITULO, CUERPO, CUERPO2], [0, TITULO, CUERPO], [TITULO, SUBTIT], []] \
   and de(1, CUERPO) == 'uno\n\tdos\n\t\ttres\ncuatro' and [r.band for r in o if r.pag == 1] == [0, 1] and de(2, CUERPO2) == 'derecha' \
   and [(r.tipo, r.caja, r.ian) for r in o if r.pag == 3 and r.papel == 0] == [(IMAGEN, (408, 264, 352, 156), 83)]"
fi

if fase 3; then
echo "== Presentaciones: las plantillas de presentación y los temas =="
prepara $HVP
{ arranca
  nueva 1 1 n0; aparca; shot t1                             # «Una asociación», con el tema oscuro
  menu $M_FORM 87; lista 2 t2l; aparca; shot t2             # Formato > Tema: petróleo
  menu $M_FORM 87; lista 5; aparca; shot t3                 # atardecer (un degradado tramado)
  tecla ctrl-z; aparca; shot t4                             # deshacer: petróleo otra vez
  menu $M_FORM 87; lista 3; aparca; shot t5                 # terracota
  tecla pgdn pgdn; menu $M_FORM 87; lista 4; aparca; shot t6     # arena, viendo la tercera
  guarda temas
  nueva 2 0; aparca; shot t7                                # «Informe», claro
  nueva 3 3; aparca; shot t8                                # «Clase», terracota
  sal; } | run 240 -vga std
chk $T/n0.ppm busca 100 150 700 450 9 7 "Una asociación" busca 100 150 700 450 9 12 "Oscuro" busca 100 150 700 450 0 9 "Atardecer"
chk $T/t1.ppm busca $PIE "Diapositiva 1 de 5    Tema: Oscuro    Vista: 58 %"
chk $T/t1.ppm px $(e 400 100) 0 "el fondo oscuro" px $(e 10 300) 4 "con su banda terracota" px 140 300 0 "y también en las miniaturas"
chk $T/t2l.ppm busca 200 100 600 500 9 12 "Petróleo"
chk $T/t2.ppm busca $PIE "Diapositiva 1 de 5    Tema: Petróleo    Vista: 58 %"
chk $T/t2.ppm px $(e 400 100) 2 "el fondo petróleo" px $(e 700 580) 10 "el bloque ocre" px 140 300 2 "y las miniaturas, también"
chk $T/t3.ppm busca $PIE "Diapositiva 1 de 5    Tema: Atardecer    Vista: 58 %"
chk $T/t3.ppm px $(e 400 6) 12 "el degradado: arriba, añil" px $(e 400 575) 2 "y abajo, petróleo"
chk $T/t4.ppm busca $PIE "Diapositiva 1 de 5    Tema: Petróleo    Vista: 58 %"
chk $T/t4.ppm px $(e 400 100) 2 "deshecho: el fondo petróleo otra vez"
chk $T/t5.ppm px $(e 400 100) 8 "terracota: el fondo crema" px $(e 10 300) 4 "y la banda"
chk $T/t6.ppm busca $PIE "Diapositiva 3 de 5    Tema: Arena    Vista: 58 %"
chk $T/t6.ppm px $(e 400 100) 6 "arena"
chk $T/t7.ppm busca $PIE "Diapositiva 1 de 4    Tema: Claro    Vista: 58 %"
chk $T/t8.ppm busca $PIE "Diapositiva 1 de 4    Tema: Terracota    Vista: 58 %"
muestra n0 presenta_nuevo; muestra t1 presenta_oscuro; muestra t2 presenta_petroleo; muestra t3 presenta_atardecer
muestra t5 presenta_terracota; muestra t6 presenta_arena; muestra t7 presenta_informe; muestra t8 presenta_clase
mira_pre TEMAS.PRE "la presentación de la asociación, con el tema arena puesto en todas sus cajas" \
  "(pags, tema) == (5, 4) and titulos == ['Asociación de vecinos', 'Quiénes somos', 'Lo que hemos hecho', 'Las cuentas', 'Gracias'] \
   and all((r.tinta, r.letra) == (13, 2) for r in o if r.papel == TITULO) and all((r.tinta, r.letra) == (0, 3) for r in o if r.papel in (CUERPO, CUERPO2, SUBTIT)) \
   and de(1, CUERPO).startswith('Una asociación sin ánimo de lucro\n') and '\n\tPintura\n' in de(2, CUERPO) and de(4, SUBTIT) == '¿Alguna pregunta?'"
fi

if fase 4; then
echo "== Presentaciones: la presentación a pantalla completa =="
prepara $HVP
# presentando (800 x 600: punto por punto), la primera viñeta de «título y texto» está en (59, 153)
V1="59 153"; V2="59 185"; V3="59 217"
{ arranca
  nueva 1 0                                                 # «Una asociación», tema claro
  tecla f5; sleep 2.5; shot s1                              # F5: la primera, a toda la pantalla
  tecla spc; sleep 1; shot s2                               # espacio: la segunda
  tecla right; sleep 1; shot s3; tecla left; sleep 1; shot s4    # flechas: la tercera, y vuelta
  escribe "4"; tecla ret; sleep 1; tecla esc; sleep 2; aparca; shot s5    # un número e Intro: a esa; Esc sale
  menu $M_PRES 155; aparca; shot s6                         # Presentación > Puntos de uno en uno
  mini 2; tecla shift-f5; sleep 2.5; shot s7                # Mayús+F5: desde esta (la segunda), sin puntos
  tecla spc; sleep 1; shot s8; tecla spc spc; sleep 1; shot s9   # de uno en uno
  tecla spc; sleep 1; shot s10; tecla backspace; sleep 1; shot s11    # la tercera (sin puntos); Retroceso: la segunda, entera
  tecla end; sleep 1; tecla spc; sleep 1; shot s12          # Fin: la última; y después, el aviso del final
  tecla spc; sleep 2; aparca; shot s13                      # otra tecla: se vuelve
  menu $M_PRES 155                                          # (los puntos, todos a la vez otra vez)
  menu $M_PRES 135; lista 1                                 # Presentación > Transición: cortina
  tecla f5; sleep 2.5; mon "sendkey spc" 0.2; mon "screendump $T/c1.ppm" 0.15; mon "screendump $T/c2.ppm" 0.15; mon "screendump $T/c3.ppm" 0.15
  sleep 2; shot s14; tecla esc; sleep 2
  menu $M_PRES 135; lista 3; tecla f5; sleep 2.5; tecla spc; sleep 3; shot s15; tecla esc; sleep 2      # disolver
  menu $M_PRES 135; lista 2; tecla f5; sleep 2.5; tecla spc; sleep 2.5; shot s16; tecla esc; sleep 2; aparca; shot s17   # barrido
  guarda pase
  sal; } | run 280 -vga std
chk $T/s1.ppm px 400 10 9 "a toda la pantalla: sin ventana" px 20 40 9 "(ni barras)" px 400 590 2 "la franja del tema, abajo" \
              px 60 590 10 "con su tramo ocre" px $V1 9 "(la portada no tiene lista)"
chk $T/s2.ppm px $V1 0 "la segunda: su lista" px $V3 0 "con sus tres puntos"
chk $T/s3.ppm px 55 148 0 "la tercera: la lista de la primera columna" px 423 148 0 "y la de la segunda"
chk $T/s4.ppm px $V1 0 "atrás: la segunda otra vez" px $V3 0 "(con sus tres puntos)"
chk $T/s5.ppm busca $PIE "Diapositiva 4 de 5    Tema: Claro    Vista: 58 %"
chk $T/s5.ppm titulo 8 30 activa "Presentaciones - (sin nombre)"
chk $T/s6.ppm busca $PIE "Diapositiva 4 de 5    Tema: Claro, puntos de uno en uno    Vista: 58 %"
chk $T/s7.ppm px 400 10 9 "desde la segunda" px $V1 9 "al principio, sin puntos"
chk $T/s8.ppm px $V1 0 "un paso: el primer punto" px $V2 9 "y no el segundo"
chk $T/s9.ppm px $V2 0 "dos pasos más: el segundo" px $V3 0 "y el tercero"
chk $T/s10.ppm px 55 148 9 "y otro: la tercera diapositiva, sin puntos todavía"
chk $T/s11.ppm px $V3 0 "Retroceso: la segunda, con todos sus puntos"
chk $T/s12.ppm busca 150 280 250 320 15 16 "Fin de la presentación. Pulse una tecla para volver."
chk $T/s13.ppm busca $PIE "Diapositiva 5 de 5"
# la cortina, a medias: arriba ya está la diapositiva nueva (su primer punto) y abajo sigue la de
# antes (el subtítulo de la portada, que en la nueva no está)
a_medias=0
for c in c1 c2 c3; do
  [ -f $T/$c.ppm ] && $CHK $T/$c.ppm px $V1 0 "" cambia 300 300 200 32 $T/s14.ppm 50 > /dev/null 2>&1 && a_medias=1 && cp $T/$c.ppm $T/cm.ppm
done
if [ $a_medias = 1 ]; then ok "la cortina, a medias: arriba la diapositiva nueva y abajo la de antes"
else ko "no se ha visto la cortina a medias"; fi
chk $T/s14.ppm px $V1 0 "y entera al acabar" px $V3 0 "(con todos sus puntos)"
chk $T/s15.ppm px $V1 0 "disolver: al acabar, la diapositiva entera" px $V3 0 "(entera)" px 400 300 9 "(y limpia)"
chk $T/s16.ppm px $V1 0 "barrido: al acabar, la diapositiva entera" px $V3 0 "(entera)"
chk $T/s17.ppm busca $PIE "Diapositiva 2 de 5    Tema: Claro, transición: barrido    Vista: 58 %"
muestra s1 presenta_pase; muestra s2 presenta_pase2; muestra s8 presenta_pasos; muestra s12 presenta_fin; muestra cm presenta_cortina
mira_pre PASE.PRE "la transición y lo de los puntos, guardados con la presentación" "(tema, trans, pasos, pags) == (0, 2, 0, 5)"
fi

if fase 5; then
echo "== Presentaciones: las notas, guardar y abrir, exportar e imprimir =="
prepara $HVP
# una diapositiva exportada son 481 KB: en el disquete de la prueba se hace sitio quitando lo que no hace falta
( cd $T; for f in PROGS/PINTURA.HVP PROGS/ARCHIVOS.HVP PROGS/MEZCLAD.HVP PROGS/SONIDOS.HVP PROGS/TERMINAL.HVP PROGS/CUENTAS.HVP \
    AUDICION.COM EQUIPO.COM HSPSND.COM HSPUSB.SYS HSPEMM.SYS ACTUALIZ.COM PARTIR.COM HSPRED.COM DESCARGA.COM CONSOLA.COM \
    REVISAR.COM VELOCIDA.COM HSPJOY.COM CONFRED.COM PING.COM INSTALAR.COM PROTECTO.COM HSPCD.SYS XCOPIAR.COM; do
    mdel -i f.img ::HISPADOS/$f 2>/dev/null; done )
rm -f $T/prn.bin
acepta() { clic 351 392; sleep 0.8; }                       # el botón Aceptar del cuadro de las notas
{ arranca
  nueva 2 0                                                 # «Informe», tema claro
  clic 400 540; sleep 1.2; escribe "Saludar y presentar el informe"; tecla esc; aparca; shot q1   # un clic en el recuadro: se escriben ahí
  tecla pgdn; menu $M_INS 223; sleep 0.8; shot q0; escribe "Dar las cifras"; acepta   # la segunda: Insertar > Notas del orador (su cuadro)
  tecla pgup; guarda informe; aparca; shot q2
  menu $M_ARCH 175; sleep 0.5; escribe "dia"; tecla ret; espera 90 "La diapositiva se ha guardado" q3; tecla ret; sleep 0.8   # Exportar como BMP
  IMPRESA="La presentación se ha enviado a la impresora."
  tecla ctrl-p; espera 90 "$IMPRESA" i1; tecla ret; sleep 0.8         # una por página
  menu $M_ARCH 243; espera 90 "$IMPRESA" i2; tecla ret; sleep 0.8     # folleto de 2
  menu $M_ARCH 263; espera 90 "$IMPRESA" i3; tecla ret; sleep 0.8     # de 3, con renglones
  menu $M_ARCH 283; espera 90 "$IMPRESA" i4; tecla ret; sleep 0.8     # de 6
  menu $M_ARCH 303; espera 90 "$IMPRESA" i5; tecla ret; sleep 0.8     # las notas
  sal; } | PARALELO=file:$T/prn.bin run 600 -vga std
chk $T/q0.ppm titulo 150 169 activa "Notas del orador de esta diapositiva"
chk $T/q1.ppm busca 215 514 225 524 0 9 "Saludar y presentar el informe"
chk $T/q2.ppm titulo 8 30 activa "Presentaciones - informe.PRE"
chk $T/q3.ppm busca 100 200 700 400 0 8 "La diapositiva se ha guardado como imagen BMP."
for i in 1 2 3 4 5; do chk $T/i$i.ppm busca 100 200 700 400 0 8 "La presentación se ha enviado a la impresora."; done
mira_pre INFORME.PRE "las notas, en una caja escondida de su diapositiva" \
  "pags == 4 and [(r.pag, r.txt, r.band) for r in o if r.papel == NOTAS] == [(0, 'Saludar y presentar el informe', 2), (1, 'Dar las cifras', 2)]"
mcopy -n -i $T/f.img ::DIA.BMP $T/dia.bmp 2>/dev/null
python3 tests/tools/pintura_bmp.py mira $T/dia.bmp tam 800 600 px 10 10 9 "el fondo del tema claro" px 60 590 10 "el tramo ocre" \
  px 400 590 2 "la franja" px 400 400 9 "sin los textos de ejemplo" || fail=1
[ -f $T/dia.bmp ] && convert $T/dia.bmp $B/presenta_exportada.png
cat > $T/prn.py <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
pags, i = [], 0
while i < len(d):
    assert d[i:i + 5] == b'\x1b@\x1bA\x08', 'cada hoja empieza poniendo la impresora a cero'
    i += 5
    filas = []
    while d[i] != 12:
        cols = b''
        if d[i] == 27:
            assert d[i + 1] == ord('L')
            n = d[i + 2] + 256 * d[i + 3]
            cols = d[i + 4:i + 4 + n]
            i += 4 + n
        assert d[i:i + 2] == b'\r\n'
        i += 2
        for k in range(8):
            filas.append([(c >> (7 - k)) & 1 for c in cols])
    i += 1
    pags.append(filas)
def pt(f, x, y): return f[y][x] if y < len(f) and x < len(f[y]) else 0
def dens(f, x0, y0, x1, y1): return sum(pt(f, x, y) for y in range(y0, y1) for x in range(x0, x1)) / ((x1 - x0) * (y1 - y0))
v = [len(pags) == 13]                                       # 4 + 2 + 2 + 1 + 4 hojas
# una por página: girada (lo ancho del papel es el alto de la diapositiva); la franja de abajo, a la izquierda
f = pags[0]
v.append(len(f) == 600 and dens(f, 3, 120, 32, 580) > 0.5 and dens(f, 60, 0, 150, 600) == 0)
# folleto de 2: dos diapositivas de 640 x 480 con su filete, una encima de otra; la franja de cada una
f = pags[4]
v.append(len(f) == 848 and dens(f, 100, 43, 860, 46) > 0.2 and dens(f, 250, 391, 850, 402) > 0.5 and dens(f, 250, 796, 850, 807) > 0.5 and dens(f, 0, 412, 960, 440) == 0)
v.append(dens(f, 52, 46, 74, 62) > 0.04 and dens(f, 52, 451, 74, 467) > 0.04)       # a la izquierda de cada una, su número
# de 3: a la izquierda las diapositivas y a la derecha los renglones
f = pags[6]
v.append(dens(f, 500, 81, 900, 84) > 0.2 and dens(f, 500, 90, 900, 100) == 0 and dens(f, 120, 232, 440, 238) > 0.4)
v.append(dens(f, 20, 61, 40, 77) > 0.04 and dens(f, 0, 100, 42, 300) == 0)              # su número, y debajo nada
v.append(dens(pags[7], 120, 232, 440, 238) > 0.4 and dens(pags[7], 0, 300, 960, 800) == 0)      # la cuarta, sola en otra hoja
# de 6: la cuarta va en la segunda fila, a la derecha
f = pags[8]
v.append(dens(f, 590, 487, 900, 493) > 0.4 and dens(f, 120, 487, 440, 493) > 0.4 and dens(f, 0, 560, 960, 840) == 0)
# las notas: debajo de la diapositiva, su texto; en la tercera no hay
v.append(dens(pags[9], 85, 424, 500, 446) > 0.03 and dens(pags[10], 85, 424, 300, 446) > 0.03 and dens(pags[11], 0, 415, 960, 840) == 0)
if not all(v): print('    comprobaciones:', v)
if len(sys.argv) > 2:
    for k in (0, 4, 6, 8, 9):
        f = pags[k]; w = max(len(r) for r in f)
        open('%s%d.pbm' % (sys.argv[2], k), 'w').write('P1\n%d %d\n' % (w, len(f)) + '\n'.join(' '.join(str(pt(f, x, y)) for x in range(w)) for y in range(len(f))) + '\n')
sys.exit(0 if all(v) else 1)
PY
if python3 $T/prn.py $T/prn.bin $T/hoja
then ok "impresora: una por página (girada), folletos de 2, 3 y 6 con el número de cada una, y las notas: trece hojas"
else ko "impresora: no ha salido lo esperado"; ls -l $T/prn.bin; fi
for k in 0 4 6 8 9; do [ -f $T/hoja$k.pbm ] && convert $T/hoja$k.pbm -resize 50%x83% $B/presenta_hoja$k.png; done
# se arranca otra vez abriendo la presentación: se ve igual que al guardarla
reabre 'A:\INFORME.PRE'
{ arranca
  aparca; shot r0
  sal; } | run 100 -vga std
chk $T/r0.ppm titulo 8 30 activa "Presentaciones - A:\\INFORME.PRE"
chk $T/r0.ppm igual 12 134 776 436 $T/q2.ppm 12 134 0
muestra q0 presenta_notas_dlg; muestra q1 presenta_notas; muestra r0 presenta_abierta
fi

if fase 6; then
echo "== Presentaciones: cambiar el diseño, numeración, las notas en su recuadro y el clasificador =="
prepara $HVP
{ arranca
  nueva 1 0                                                 # «Una asociación», tema claro: cinco diapositivas
  aparca; shot f0
  menu $M_FORM 107; lista 1 f0l; aparca; shot f1            # Formato > Diseño: la portada pasa a «título y texto»
  tecla pgdn; menu $M_FORM 107; lista 2; aparca; shot f2    # la segunda (título y texto), a dos columnas
  tecla pgdn; menu $M_FORM 107; lista 3; aparca; shot f3    # la tercera (dos columnas llenas), a solo título: se quedan
  tecla pgdn; ec 400 300; boton2 674; aparca; shot f4       # la cuarta: su lista, con números
  tecla esc
  clic 400 540; sleep 0.8; escribe "Nota uno"; tecla ret; escribe "linea dos"; aparca; shot f5   # las notas, en su recuadro
  tecla esc; aparca; shot f6
  clic 228 526; sleep 0.5; escribe "X"; tecla esc           # un clic en ellas lleva el cursor ahí
  menu $M_VER 291; aparca; shot k1                          # Ver > Clasificador de diapositivas
  ir 630 189; mon "mouse_button 1" 0.3; ir 400 189; ir 90 189; shot k2; mon "mouse_button 0" 0.6   # la quinta, al principio
  aparca; shot k3
  tecla right right; aparca; shot k4                        # las flechas: a la tercera
  ir 222 189; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.12; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.6
  aparca; shot k5                                           # un doble clic en la segunda: se vuelve a ella
  guarda clasif
  clic $M_PRES 65; sleep 0.9; shot z1; tecla esc; sleep 0.5 # el menú Presentación: las marcas
  menu $M_PRES 155; clic $M_PRES 65; sleep 0.9; shot z2; tecla esc   # con los puntos de uno en uno
  sal; } | run 300 -vga std
chk $T/f0l.ppm busca 200 100 600 500 9 12 "Título y texto"
chk $T/f1.ppm cambia $(e 100 180) 350 60 $T/f0.ppm 200          # el título ya no está en medio
chk $T/f1.ppm busca $PIE "Diapositiva 1 de 5    Tema: Claro    Vista: 58 %"
chk $T/f2.ppm busca $PIE "Diapositiva 2 de 5    Tema: Claro    Vista: 58 %"
chk $T/f4.ppm px 724 108 2 "la lista de la cuarta, con números: su botón, puesto"
chk $T/f5.ppm busca 215 514 225 524 0 9 "Nota uno"
chk $T/f5.ppm busca 215 530 225 540 0 9 "linea dos"
chk $T/f5.ppm busca $PIE "Diapositiva 4 de 5    Caja de texto: escriba; Esc o un clic fuera para acabar"
chk $T/f6.ppm busca 215 514 225 524 0 9 "Nota uno"
chk $T/f6.ppm busca $PIE "Diapositiva 4 de 5    Tema: Claro    Vista: 58 %"
chk $T/f5.ppm cambia 288 532 8 18 $T/f6.ppm 10                  # escribiendo, el cursor detrás de lo escrito
chk $T/k1.ppm px 400 420 15 "el clasificador: el fondo" px 436 189 2 "la cuarta, con su marco" px 28 189 15 "y la primera, sin él"
chk $T/k1.ppm busca $PIE "Diapositiva 4 de 5    Tema: Claro    Vista: 58 %"
chk $T/k2.ppm px 20 189 10 "arrastrando una miniatura: la raya de dónde va"
chk $T/k3.ppm busca $PIE "Diapositiva 1 de 5    Tema: Claro    Vista: 58 %"
chk $T/k4.ppm busca $PIE "Diapositiva 3 de 5    Tema: Claro    Vista: 58 %"
chk $T/k5.ppm busca $PIE "Diapositiva 2 de 5    Tema: Claro    Vista: 58 %"
chk $T/k5.ppm px 30 250 2 "fuera del clasificador: la tira, con la segunda marcada" px 700 540 9 "y el recuadro de las notas"
muestra f1 presenta_diseno2; muestra f2 presenta_columnas; muestra f4 presenta_numeros; muestra f5 presenta_notas_ed
muestra k1 presenta_clasificador; muestra k2 presenta_clasif_arrastre; muestra z2 presenta_menu_pres
chk $T/z2.ppm cambia 430 148 12 14 $T/z1.ppm 4                 # «puntos de uno en uno», ya con su marca
chk $T/z2.ppm igual 430 80 12 60 $T/z1.ppm 430 80 0            # (y las demás órdenes del menú, como estaban)
mira_pre CLASIF.PRE "el orden nuevo, los diseños cambiados (cada caja en su sitio, con su texto), la lista con números y las notas" \
  "titulos == ['Gracias', 'Asociación de vecinos', 'Quiénes somos', 'Lo que hemos hecho', 'Las cuentas'] \
   and sorted(r.papel for r in o if r.pag == 1) == [TITULO, CUERPO] and de(1, CUERPO) == 'Memoria del año' \
   and [r.caja for r in o if r.pag == 1 and r.papel == TITULO] == [(40, 21, 720, 102)] \
   and [(r.caja, r.band) for r in o if r.pag == 1 and r.papel == CUERPO] == [((40, 132, 720, 420), 1)] \
   and sorted(r.papel for r in o if r.pag == 2) == [TITULO, CUERPO, CUERPO2] and de(2, CUERPO2) == '' \
   and [r.caja for r in o if r.pag == 2 and r.papel == CUERPO] == [(40, 132, 352, 420)] and de(2, CUERPO).startswith('Una asociación') \
   and sorted(r.papel for r in o if r.pag == 3) == [TITULO, CUERPO, CUERPO2] and 'Pintura' in de(3, CUERPO) and 'Biblioteca' in de(3, CUERPO2) \
   and [r.band for r in o if r.pag == 4 and r.papel == CUERPO] == [8] and de(4, NOTAS) == 'NXota uno\nlinea dos'"
fi

rm -rf $T
exit $fail
