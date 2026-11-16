#!/bin/bash
# Prueba de Publicaciones (PUBLICA.HVP), el programa de panfletos y carteles,
# del grupo «Productividad».
#
# Abre Hispa Ventanas con Publicaciones (clave «Reabrir=» de CENTRO.INI) y,
# por fases (cada una arranca de nuevo):
#   1 la ventana, la barra, el pie, la página y las vistas (entera, 50 %,
#     100 %), los márgenes, la rejilla y el desplazamiento;
#   2 rectángulos, elipses y líneas: ponerlos, elegirlos, moverlos, cambiarles
#     el tamaño, el color y el grosor, y borrarlos;
#   3 cajas de texto: escribir (el texto se parte solo), el cursor, los tipos
#     de letra, el tamaño, negrita, alineación y color;
#   4 guardar y abrir (y un archivo que no vale);
#   5 deshacer y rehacer, duplicar, el orden, cortar, copiar y pegar, y el
#     ajuste a la rejilla;
#   6 varias páginas;
#   7 las plantillas;
#   8 imprimir (LPT1 a un archivo) y exportar la página como BMP;
#   9 una imagen BMP.
# Lo que queda en cada .PUB guardado se comprueba leyendo el archivo.
#
# uso: run_publica_tests.sh [IMAGEN]      (FASES="2 5" corre solo esas)
# Necesita qemu-system-i386, mtools, socat e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
HVP=$B/PUBLICA.HVP
NASM=${NASM:-nasm} python3 tools/mkhvp.py programas/publica.asm $HVP > /dev/null || { echo "  FAIL no se ensambla"; exit 1; }
FASES=${FASES:-1 2 3 4 5 6 7 8 9}
fase() { case " $FASES " in *" $1 "*) return 0;; esac; return 1; }
reabre() { printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\PUBLICA.HVP %s\r\n' "$1" > $T/CENTRO.INI
           mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI; }

# ---- la ventana: en (8, 30); su zona empieza en (10, 75) ----
# Al 100 %, el punto (x, y) de la página está en la pantalla en (18+x, 141+y).
PX=18; PY=141
aparca() { ir 796 400; }                                    # (el ratón, fuera de la ventana: no tapa el pie)
pc() { clic $((PX + $1)) $((PY + $2)); }                    # un clic en la página
pdc() { ir $((PX + $1)) $((PY + $2)); mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.12
        mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.5; }   # y un doble clic
arr() { ir $((PX + $1)) $((PY + $2)); mon "mouse_button 1" 0.3  # arrastrar de un punto a otro
        ir $((PX + ($1 + $3) / 2)) $((PY + ($2 + $4) / 2)); ir $((PX + $3)) $((PY + $4)); mon "mouse_button 0" 0.5; }
boton1() { clic $((10 + $1 + 12)) 89; }                     # un botón de la fila de arriba (su x)
boton2() { clic $((10 + $1 + 11)) 117; }                    # y de la de abajo
B_VISTA=522; B_ELEGIR=258; B_TEXTO=284; B_RECT=310; B_ELIPSE=336; B_LINEA=362
al100() { boton1 $B_VISTA; sleep 0.8; }
# un menú (la x de su nombre) y una orden suya (su y)
menu() { clic $1 65; sleep 0.9; clic $(( $1 + 30 )) $2; sleep 0.9; }
M_ARCH=50; M_EDIC=123; M_VER=178; M_INS=239; M_FORM=315; M_HERR=407
# elegir de una lista: tantas veces abajo desde la primera
lista() { sleep 1; tecla home; local i; for i in $(seq 1 $1); do tecla down; done; [ "${2:-}" ] && shot $2; tecla ret; sleep 0.8; }
guarda() { tecla ctrl-g; sleep 1.2; escribe "$1"; tecla ret; sleep 2.5; }
PIE="16 566 40 582 0 6"                                     # dónde se busca el texto del pie

# lee un .PUB y comprueba una condición en Python: o[i] son sus objetos
cat > $T/pub.py <<'PY'
import sys, struct
b = open(sys.argv[1], 'rb').read()
assert b[:8] == b'PUBLICA\x1a', 'no es un .PUB'
an, al, margen, pags, n, tam, nd = struct.unpack_from('<HHHHHHI', b, 8)
assert len(b) == 24 + n * tam + nd, 'el tamaño del archivo no cuadra'
class O: pass
o = []
for i in range(n):
    r = O()
    (r.tipo, r.pag, r.estilo, r.alin, r.x, r.y, r.an, r.al, r.tinta, r.fondo, r.borde, r.grosor,
     r.letra, r.cuerpo, r.ian, r.ial, r.papel, r.band, dat, largo) = struct.unpack_from('<BBBBiiiiBBBBBBHHBBII', b, 24 + i * tam)
    r.dat = b[24 + n * tam + dat:24 + n * tam + dat + largo]
    r.txt = r.dat.decode('cp437')
    r.caja = (r.x, r.y, r.an, r.al)
    o.append(r)
TEXTO, RECT, ELIPSE, LINEA, IMAGEN = 1, 2, 3, 4, 5
NADA = 255
if len(sys.argv) > 3:
    for r in o: print('   ', r.tipo, r.pag, r.caja, r.estilo, r.alin, r.tinta, r.fondo, r.borde, r.grosor, r.letra, r.cuerpo, repr(r.txt[:40]))
sys.exit(0 if eval(sys.argv[2]) else 1)
PY
# mira_pub NOMBRE.PUB "lo que se comprueba" 'condición'
mira_pub() {
  rm -f $T/m.pub; mcopy -n -i $T/f.img ::$1 $T/m.pub 2>/dev/null
  if python3 $T/pub.py $T/m.pub "$3" 2>/dev/null; then ok "$1: $2"
  else ko "$1: $2"; python3 $T/pub.py $T/m.pub "True" ver 2>&1 | head -30; fi
}

# lo que llega a la impresora (LPT1 a un archivo): se deshace la imagen de bits y se mira
cat > $T/prn.py <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
def paginas(d):
    """Lo que ha llegado a la impresora -> de cada página, (orden de imagen, filas de puntos)."""
    pags, i = [], 0
    while i < len(d):
        assert d[i:i + 5] == b'\x1b@\x1bA\x08', 'cada página empieza poniendo la impresora a cero y el salto de 8/72'
        i += 5
        modo, filas = None, []
        while d[i] != 12:
            cols = b''
            if d[i] == 27:
                assert d[i + 1] in b'KL'
                modo = chr(d[i + 1])
                n = d[i + 2] + 256 * d[i + 3]
                cols = d[i + 4:i + 4 + n]
                i += 4 + n
            assert d[i:i + 2] == b'\r\n'
            i += 2
            for k in range(8):
                filas.append([(c >> (7 - k)) & 1 for c in cols])
        i += 1
        pags.append((modo, filas))
    return pags
def pt(f, x, y): return f[y][x] if x < len(f[y]) else 0
def dens(f, x0, y0, x1, y1): return sum(pt(f, x, y) for y in range(y0, y1) for x in range(x0, x1)) / ((x1 - x0) * (y1 - y0))
p = paginas(d)
v = []
v.append(len(p) == 4 and [m for m, f in p] == ['L', 'K', 'L', 'L'])
t, b = p[0][1], p[1][1]
# la tarjeta (321 x 208): 401 columnas a 120 ppp y 156 filas a 72 ppp, en 20 tiras
v.append(len(t) == 160 and max(len(f) for f in t) <= 401)
v.append(0.5 < dens(t, 3, 5, 33, 150) < 0.95)              # la franja petróleo (hasta la columna 36), tramada
v.append(dens(t, 140, 140, 400, 156) == 0)                  # el papel, en blanco
v.append(dens(t, 60, 16, 300, 34) > 0.05)                  # el nombre
v.append(len(b) == 160 and max(len(f) for f in b) <= 200 and 0.5 < dens(b, 2, 5, 16, 150) < 0.95 and dens(b, 70, 140, 200, 156) == 0)
# el A5 apaisado (794 x 559) va girado: lo ancho del papel es el alto de la página. El rectángulo
# negro de (40,40) a (140,100) cae en las columnas 573-648 y las filas 30-105
g = p[2][1]
v.append(len(g) == 600 and dens(g, 576, 32, 646, 103) == 1 and dens(g, 0, 0, 560, 600) == 0 and dens(g, 0, 110, 699, 600) == 0)
g = p[3][1]                                                 # y el de la otra página, de (200,100) a (300,200)
v.append(dens(g, 451, 152, 571, 223) == 1 and dens(g, 0, 0, 440, 600) == 0)
if not all(v): print('    comprobaciones:', v)
if len(sys.argv) > 2:                                       # (para verlas: cada página, a un PGM)
    for i, (m, f) in enumerate(p):
        w = max(len(r) for r in f)
        open('%s%d.pgm' % (sys.argv[2], i), 'w').write('P2 %d %d 1\n' % (w, len(f)) + '\n'.join(' '.join(str(1 - pt(f, x, y)) for x in range(w)) for y in range(len(f))))
sys.exit(0 if all(v) else 1)
PY

if fase 1; then
echo "== Publicaciones: la ventana, la página y las vistas =="
prepara $HVP
{ arranca
  shot v0
  ir 332 89; shot v1                                        # el ratón sobre un botón: su nombre, abajo
  al100; aparca; shot v2
  clic 778 549; clic 778 549; aparca; shot v3               # la flecha de abajo de la barra, dos veces
  menu $M_VER 107; aparca; shot v4                          # Ver > Al 50 %
  menu $M_VER 127; aparca; shot v4b; menu $M_VER 107        # Ver > Al 75 %; y al 50 % otra vez
  menu $M_VER 195; aparca; shot v5                          # Ver > Ver la rejilla
  menu $M_VER 175; aparca; shot v6                          # Ver > Ver los márgenes: se quitan
  menu $M_VER 87; aparca; shot v7                           # Ver > Página entera
  clic 475 65; sleep 0.9; clic 505 87; sleep 1.2; shot v8; tecla ret   # ? > Acerca de
  sleep 0.8; tecla ctrl-p; sleep 2; shot v9; tecla ret; sleep 0.8      # imprimir, sin impresora
  menu $M_EDIC 271; shot v10; tecla ret                     # Edición > Quitar esta página: es la única
  sal; } | run 150 -vga std
chk $T/v0.ppm titulo 8 30 activa "Publicaciones - (sin nombre)"
chk $T/v0.ppm busca $PIE "Página 1 de 1    A4 vertical, 210 x 297 mm    Vista: 36 %"
chk $T/v0.ppm px 400 340 9 "la página, blanca" px 100 300 8 "crema alrededor" px 262 340 14 "la guía del margen" \
              px 12 103 6 "la barra, en arena" px 270 80 2 "la herramienta Elegir, puesta"
chk $T/v1.ppm busca $PIE "Rectángulo: arrastre sobre la página"
chk $T/v2.ppm busca $PIE "Página 1 de 1    A4 vertical, 210 x 297 mm    Vista: 100 %"
chk $T/v2.ppm px 30 150 9 "al 100 %: la página desde la esquina" px 56 300 14 "el margen izquierdo, a 10 mm" \
              px 300 $((PY+38)) 14 "el margen de arriba" px 300 $((PY+6)) 9 "blanco encima del margen"
chk $T/v3.ppm px 300 $((PY+6)) 14 "desplazada 32 puntos: el margen de arriba, más arriba" px 300 $((PY+38)) 9 "y blanco donde estaba"
chk $T/v4.ppm busca $PIE "Página 1 de 1    A4 vertical, 210 x 297 mm    Vista: 50 %"
chk $T/v4.ppm px 221 169 9 "sin rejilla, blanco"
chk $T/v4b.ppm busca $PIE "Página 1 de 1    A4 vertical, 210 x 297 mm    Vista: 75 %"
chk $T/v5.ppm px 221 169 7 "la rejilla: un punto a 15 mm de la esquina" px 224 172 9 "y blanco entre los puntos"
chk $T/v5.ppm px 212 305 14 "el margen"
chk $T/v6.ppm px 212 305 9 "sin márgenes"
chk $T/v7.ppm busca $PIE "Página 1 de 1    A4 vertical, 210 x 297 mm    Vista: 36 %"
chk $T/v8.ppm busca 150 200 650 400 0 8 "Copyright (C) 2026 Carlos Sanz y contribuidores."
chk $T/v8.ppm busca 150 200 650 400 0 8 "GPL-2.0 o posterior."
chk $T/v9.ppm busca 100 200 700 400 0 8 "No se ha podido imprimir."
chk $T/v9.ppm busca 100 200 700 400 0 8 "está encendida, en línea y con papel."
chk $T/v10.ppm busca 100 200 700 400 0 8 "Es la única página: no se puede quitar."
muestra v0 publica_vacia; muestra v2 publica_100; muestra v5 publica_rejilla; muestra v8 publica_acerca
fi

if fase 2; then
echo "== Publicaciones: rectángulos, elipses y líneas =="
prepara $HVP
{ arranca
  al100
  boton1 $B_RECT; arr 60 60 200 160; aparca; shot f1        # un rectángulo de 140 x 100
  boton2 444; lista 5 f1l                                   # su relleno: terracota
  boton2 554; lista 2                                       # el grosor del borde: 3
  boton1 $B_ELIPSE; arr 250 60 400 160                      # una elipse (sale con ese relleno)
  boton2 524; lista 13                                      # su borde: añil
  boton1 $B_LINEA; arr 60 200 300 240                       # una línea
  boton1 $B_LINEA; pc 500 300                               # y otra de un clic (tamaño corriente)
  aparca; shot f2
  pc 100 100; aparca; shot f3                               # un clic elige el rectángulo
  ir $((PX+130)) $((PY+160)); shot f3c                      # sobre un tirador, el cursor cambia de forma
  arr 100 100 130 140                                       # arrastrando se mueve: a (90, 100)
  arr 230 200 260 230; aparca; shot f4                      # por el tirador de la esquina: 170 x 130
  tecla right right right down; aparca; shot f5             # con las flechas: a (93, 101)
  pc 575 300; tecla delete; aparca; shot f6                 # la línea del clic: elegida y borrada
  pc 180 220; arr 300 240 300 300                           # la otra línea: su extremo, más abajo
  tecla esc; aparca; shot f7
  guarda formas
  sal; } | run 200 -vga std
chk $T/f1.ppm busca $PIE "Página 1 de 1    Rectángulo: x 16, y 16 mm; 37 x 26 mm"
chk $T/f1.ppm px $((PX+60)) $((PY+100)) 0 "el borde del rectángulo" px $((PX+100)) $((PY+100)) 9 "sin relleno"
chk $T/f1l.ppm busca 200 100 600 500 9 12 "Terracota"                       # la lista de colores, con ese elegido
chk $T/f2.ppm px $((PX+100)) $((PY+100)) 4 "el relleno, terracota" px $((PX+61)) $((PY+100)) 0 "el borde, de 3" \
              px $((PX+62)) $((PY+100)) 0 "(3)" px $((PX+63)) $((PY+100)) 4 "y dentro, relleno"
chk $T/f2.ppm px $((PX+325)) $((PY+110)) 4 "la elipse, rellena" px $((PX+252)) $((PY+62)) 9 "blanco en su esquina" \
              px $((PX+251)) $((PY+110)) 12 "su borde, añil" px $((PX+180)) $((PY+220)) 12 "la línea" \
              px $((PX+575)) $((PY+300)) 12 "la línea de un clic"
chk $T/f3.ppm busca $PIE "Página 1 de 1    Rectángulo: x 16, y 16 mm; 37 x 26 mm"
chk $T/f3.ppm px $((PX+57)) $((PY+57)) 0 "un tirador en la esquina" px $((PX+127)) $((PY+157)) 0 "y otro en medio de abajo"
chk $T/f3c.ppm px $((PX+131)) $((PY+156)) 10 "sobre el tirador de abajo, el cursor de estirar (arriba y abajo)" px $((PX+131)) $((PY+164)) 10 "(sus dos puntas)"
chk $T/f4.ppm busca $PIE "Página 1 de 1    Rectángulo: x 24, y 26 mm; 45 x 34 mm"
chk $T/f4.ppm px $((PX+70)) $((PY+70)) 9 "donde estaba, blanco" px $((PX+250)) $((PY+220)) 4 "y ahora llega más allá"
chk $T/f5.ppm busca $PIE "Página 1 de 1    Rectángulo: x 25, y 27 mm; 45 x 34 mm"
chk $T/f6.ppm px $((PX+575)) $((PY+300)) 9 "la línea, borrada"
chk $T/f7.ppm px $((PX+280)) $((PY+292)) 12 "la línea, con el extremo más abajo" px $((PX+280)) $((PY+237)) 9 "ya no pasa por donde antes"
muestra f2 publica_formas; muestra f4 publica_tiradores; muestra f1l publica_colores
mira_pub FORMAS.PUB "las tres formas, con su sitio, su tamaño y sus colores" \
  "(an, al, pags, n) == (794, 1123, 1, 3) and [r.tipo for r in o] == [RECT, ELIPSE, LINEA] \
   and o[0].caja == (93, 101, 170, 130) and (o[0].fondo, o[0].borde, o[0].grosor) == (4, 0, 3) \
   and o[1].caja == (250, 60, 150, 100) and (o[1].fondo, o[1].borde, o[1].grosor) == (4, 12, 3) \
   and o[2].caja == (60, 200, 240, 100) and (o[2].borde, o[2].grosor) == (12, 3)"
fi

if fase 3; then
echo "== Publicaciones: cajas de texto =="
prepara $HVP
{ arranca
  al100
  boton1 $B_TEXTO; arr 60 60 360 200                        # una caja de 300 x 140: se escribe ya
  escribe "Hola mundo, esto es una caja de texto que se parte sola por las palabras"; tecla ret
  escribe "segunda"; aparca; shot t1
  tecla home; escribe "X"; tecla up; escribe "Y"; tecla end; aparca; shot t2      # el cursor: Inicio, arriba, Fin
  tecla home right right backspace down left delete esc; aparca; shot t3           # Retroceso y Supr; Esc acaba
  boton1 $B_TEXTO; arr 60 240 500 330                       # otra, con formato
  boton2 6; lista 10 t4l                                    # tipo: Llano negra
  boton2 106; lista 3                                       # tamaño: 24 puntos
  tecla ctrl-n ctrl-t                                       # negrita y centrado
  boton2 364; lista 13                                      # letra granate
  boton2 444; lista 11                                      # fondo ocre
  boton2 524; lista 1                                       # borde negro
  escribe "Titulo"; aparca; shot t4; tecla esc
  pdc 104 72; escribe "gran "; tecla end; aparca; shot t5              # doble clic entre dos palabras: se escribe ahí
  pc 600 380                                                # un clic fuera: se acaba de escribir
  pc 100 100; escribe "s"; tecla esc                        # elegida, ponerse a escribir sigue al final
  tecla ctrl-d; aparca; shot t6                             # a la derecha
  tecla ctrl-k ctrl-s; aparca; shot t7                      # cursiva y subrayado
  boton1 $B_TEXTO; arr 400 60 500 90; escribe "esto no cabe en una caja tan chica"; tecla esc; aparca; shot t9   # más texto del que cabe: se avisa
  tecla delete
  boton1 $B_TEXTO; arr 400 240 700 330; boton2 648          # otra caja, con viñetas: una lista
  escribe "uno"; tecla ret tab; escribe "dos"; tecla ret shift-tab; escribe "tres"; tecla esc; aparca; shot t10
  boton1 $B_TEXTO; pc 600 380; tecla esc                    # una caja que se queda vacía se quita sola
  aparca; shot t8
  guarda texto
  sal; } | run 240 -vga std
chk $T/t1.ppm busca $((PX+62)) $((PY+62)) $((PX+66)) $((PY+66)) 0 9 "Hola mundo, esto es una caja de"
chk $T/t1.ppm busca $((PX+62)) $((PY+78)) $((PX+66)) $((PY+82)) 0 9 "texto que se parte sola por las"
chk $T/t1.ppm busca $((PX+62)) $((PY+94)) $((PX+66)) $((PY+98)) 0 9 "palabras"
chk $T/t1.ppm busca $((PX+62)) $((PY+110)) $((PX+66)) $((PY+114)) 0 9 "segunda"
chk $T/t1.ppm busca $PIE "Página 1 de 1    Caja de texto: escriba; Esc o un clic fuera para acabar"
chk $T/t1.ppm px $((PX+120)) $((PY+118)) 0 "el cursor, detrás de lo escrito" px $((PX+121)) $((PY+118)) 0 "(de dos puntos)" \
              px $((PX+123)) $((PY+118)) 9 "y blanco detrás"
chk $T/t2.ppm busca $((PX+62)) $((PY+110)) $((PX+66)) $((PY+114)) 0 9 "Xsegunda"
chk $T/t2.ppm busca $((PX+62)) $((PY+94)) $((PX+66)) $((PY+98)) 0 9 "pYalabras"
chk $T/t3.ppm busca $((PX+62)) $((PY+94)) $((PX+66)) $((PY+98)) 0 9 "palabras"
chk $T/t3.ppm busca $((PX+62)) $((PY+110)) $((PX+66)) $((PY+114)) 0 9 "segunda"
chk $T/t3.ppm busca $PIE "Página 1 de 1    Caja de texto: x 16, y 16 mm; 79 x 37 mm"
chk $T/t4l.ppm busca 200 100 600 500 9 12 "Llano negra"
chk $T/t4.ppm px $((PX+60)) $((PY+280)) 0 "el borde de la caja" px $((PX+66)) $((PY+246)) 10 "su fondo, ocre" \
              px $((PX+100)) $((PY+265)) 10 "a la izquierda del título centrado, fondo"
chk $T/t4.ppm cambia $((PX+200)) $((PY+244)) 160 40 $T/t3.ppm 300        # ahí está el título
chk $T/t4.ppm nobusca $((PX+62)) $((PY+240)) $((PX+400)) $((PY+280)) 13 10 "Titulo"   # (y no es la letra normal)
chk $T/t4.ppm busca 20 108 60 124 0 8 "Llano negra"                                 # la barra dice el tipo...
chk $T/t4.ppm busca 120 108 140 124 0 8 "24"                                        # ...y el tamaño
chk $T/t4.ppm px 170 110 2 "el botón N, puesto" px 280 110 2 "y el de centrar"
chk $T/t5.ppm busca $((PX+62)) $((PY+62)) $((PX+66)) $((PY+66)) 0 9 "Hola gran mundo, esto es una caja de"
chk $T/t6.ppm busca $((PX+290)) $((PY+110)) $((PX+294)) $((PY+114)) 0 9 "segundas"       # arrimado a la derecha
chk $T/t6.ppm busca $((PX+106)) $((PY+78)) $((PX+110)) $((PY+82)) 0 9 "texto que se parte sola por las"
chk $T/t7.ppm nobusca $((PX+280)) $((PY+108)) $((PX+300)) $((PY+116)) 0 9 "segundas"     # en cursiva ya no es igual
chk $T/t7.ppm px $((PX+320)) $((PY+126)) 0 "el subrayado"
chk $T/t9.ppm busca $PIE "Página 1 de 1    Caja de texto: x 106, y 16 mm; 26 x 8 mm; el texto no cabe"
chk $T/t9.ppm px $((PX+489)) $((PY+79)) 13 "y la marca granate en la esquina de la caja"
chk $T/t10.ppm px $((PX+411)) $((PY+252)) 0 "la lista: la viñeta del primer punto" px $((PX+427)) $((PY+266)) 0 "la raya del segundo, con su sangría" \
               px $((PX+411)) $((PY+280)) 0 "y la del tercero, otra vez al margen" px $((PX+411)) $((PY+266)) 10 "(el segundo no está al margen: ahí, el fondo de la caja)"
chk $T/t8.ppm px $((PX+610)) $((PY+390)) 9 "la caja vacía no deja nada"
muestra t1 publica_texto; muestra t4 publica_titulo; muestra t7 publica_cursiva; muestra t4l publica_tipos
mira_pub TEXTO.PUB "las dos cajas, con su texto y su formato, y la lista con sus niveles" \
  "n == 3 and o[2].txt == 'uno\n\tdos\ntres' and o[2].band == 1 and o[0].band == 0 and o[0].tipo == TEXTO and o[1].tipo == TEXTO and o[0].caja == (60, 60, 300, 140) \
   and o[0].txt == 'Hola gran mundo, esto es una caja de texto que se parte sola por las palabras\nsegundas' \
   and (o[0].letra, o[0].cuerpo, o[0].estilo, o[0].alin, o[0].tinta, o[0].fondo, o[0].borde) == (0, 0, 6, 2, 0, NADA, NADA) \
   and o[1].caja == (60, 240, 440, 90) and o[1].txt == 'Titulo' \
   and (o[1].letra, o[1].cuerpo, o[1].estilo, o[1].alin, o[1].tinta, o[1].fondo, o[1].borde) == (10, 3, 1, 1, 13, 10, 0)"
fi

if fase 4; then
echo "== Publicaciones: guardar y abrir =="
prepara $HVP
printf 'esto no es una publicacion' > $T/NOVALE.PUB
# FUTURO.PUB: de un programa posterior (fichas más largas y un objeto de un tipo que no se conoce);
# ROTO.PUB: dice que tiene cinco objetos y se acaba en el segundo
python3 -c "
import struct, sys
def ficha(tipo, x, y, an, al, fondo=255, borde=0, largo=0):
    return struct.pack('<BBBBiiiiBBBBBBHHHII', tipo, 0, 0, 0, x, y, an, al, 0, fondo, borde, 1, 0, 0, 0, 0, 0, 0, largo).ljust(48, b'\\xEE')
t = b'del futuro'
f = ficha(2, 40, 40, 200, 100, fondo=10) + ficha(9, 0, 0, 50, 50) + ficha(1, 60, 60, 180, 40, borde=255, largo=len(t))
open(sys.argv[1] + '/FUTURO.PUB', 'wb').write(b'PUBLICA\\x1a' + struct.pack('<HHHHHHI', 559, 794, 38, 1, 3, 48, len(t)) + f + t)
open(sys.argv[1] + '/ROTO.PUB', 'wb').write(b'PUBLICA\\x1a' + struct.pack('<HHHHHHI', 794, 1123, 38, 1, 5, 40, 0) + f[:60])
" $T
mcopy -o -i $T/f.img $T/NOVALE.PUB $T/FUTURO.PUB $T/ROTO.PUB ::
{ arranca
  al100
  boton1 $B_RECT; arr 40 40 300 120; boton2 444; lista 3    # un rectángulo petróleo
  boton1 $B_TEXTO; arr 60 60 280 100; boton2 364; lista 9; escribe "Guardado"; tecla esc   # con un texto blanco
  tecla ret; escribe " ya"; tecla esc                       # Intro: a escribir otra vez, al final
  boton1 $B_ELIPSE; arr 320 40 420 120
  tecla esc; aparca; shot g1
  guarda carta; shot g2
  menu $M_ARCH 155; sleep 0.5; escribe "copia"; tecla ret; sleep 2.5; shot g3      # Guardar como
  pc 370 80; tecla delete                                   # un cambio (borrar la elipse)...
  tecla ctrl-a; sleep 1.2; shot g4; tecla tab tab ret; sleep 0.8; shot g5     # ...y Abrir pregunta: Cancelar
  tecla ctrl-a; sleep 1.2; tecla tab ret; sleep 1.2          # otra vez: No; y el diálogo de abrir
  escribe "novale.pub"; tecla ret; sleep 2; shot g6; tecla ret; sleep 0.8; aparca; shot g7
  tecla ctrl-a; sleep 1.2; tecla tab ret; sleep 1.2; escribe "roto.pub"; tecla ret; sleep 2; shot g8; tecla ret; sleep 0.8; aparca; shot g9
  sal; } | run 200 -vga std
chk $T/g2.ppm titulo 8 30 activa "Publicaciones - carta.PUB"
chk $T/g3.ppm titulo 8 30 activa "Publicaciones - copia.PUB"
chk $T/g4.ppm busca 150 200 650 400 0 8 "La publicación tiene cambios sin guardar."
chk $T/g5.ppm px $((PX+370)) $((PY+41)) 9 "cancelado: sigue como estaba (sin la elipse)" px $((PX+100)) $((PY+50)) 2 "y con lo demás"
chk $T/g6.ppm busca 100 200 700 400 0 8 "Ese archivo no es una publicación de Publicaciones (.PUB)."
chk $T/g7.ppm titulo 8 30 activa "Publicaciones - copia.PUB"
chk $T/g7.ppm px $((PX+100)) $((PY+50)) 2 "tras el archivo que no vale, el documento sigue ahí"
chk $T/g8.ppm busca 100 200 700 400 0 8 "Ese archivo no es una publicación de Publicaciones (.PUB)."
chk $T/g9.ppm igual $PX $PY 700 400 $T/g7.ppm $PX $PY 0      # un archivo que se acaba a medias: el documento, intacto
mira_pub CARTA.PUB "el rectángulo, el texto y la elipse" \
  "[r.tipo for r in o] == [RECT, TEXTO, ELIPSE] and o[0].fondo == 2 and o[1].txt == 'Guardado ya' and o[1].tinta == 9 and o[2].caja == (320, 40, 100, 80)"
mcopy -n -i $T/f.img ::CARTA.PUB $T/carta.pub; mcopy -n -i $T/f.img ::COPIA.PUB $T/copia.pub
if cmp -s $T/carta.pub $T/copia.pub; then ok "COPIA.PUB (Guardar como) es igual que CARTA.PUB"; else ko "COPIA.PUB no es igual que CARTA.PUB"; fi
# se guarda en un temporal que luego toma el nombre: no queda ninguno por el disco
if mdir -i $T/f.img :: | grep -qi '\.TMP\| TMP '; then ko "ha quedado un temporal (.TMP) en el disco"; else ok "guardado seguro: no queda ningún temporal en el disco"; fi
# SOLOLEE.PUB: una copia que no se puede cambiar; guardar encima tiene que fallar sin estropearla
cp $T/carta.pub $T/SOLOLEE.PUB; mcopy -o -i $T/f.img $T/SOLOLEE.PUB ::; mattrib -i $T/f.img +r ::SOLOLEE.PUB
# se arranca otra vez abriendo el documento: se ve igual que antes de guardar
reabre 'A:\CARTA.PUB'
{ arranca
  shot a0
  al100; aparca; shot a1
  tecla ctrl-a; sleep 1.2; escribe "futuro.pub"; tecla ret; sleep 2; aparca; shot a4; tecla ctrl-g; sleep 2.5   # el de mañana
  tecla ctrl-a; sleep 1.2; escribe "sololee.pub"; tecla ret; sleep 2          # el que no se puede cambiar
  boton1 $B_LINEA; pc 300 300; tecla ctrl-g; sleep 2.5; shot a5; tecla ret; sleep 0.8   # un cambio y guardar: no se puede
  clic 779 41; sleep 1.2; shot a2; tecla tab ret; sleep 1.2; shot a3   # ...y al cerrar pregunta: No
  sal; } | run 120 -vga std
chk $T/a0.ppm titulo 8 30 activa "Publicaciones - A:\\CARTA.PUB"
chk $T/a0.ppm busca $PIE "Página 1 de 1    A4 vertical, 210 x 297 mm    Vista: 36 %"
chk $T/a1.ppm igual $PX $PY 700 400 $T/g1.ppm $PX $PY 0
chk $T/a4.ppm titulo 8 30 activa "Publicaciones - futuro.pub"
chk $T/a4.ppm busca $PIE "Página 1 de 1    A5 vertical, 148 x 210 mm    Vista: 100 %"
chk $T/a4.ppm px $((112+45)) $((PY+45)) 10 "el archivo de mañana: su rectángulo" busca $((112+62)) $((PY+62)) $((112+66)) $((PY+66)) 0 10 "del futuro"
mira_pub FUTURO.PUB "guardado otra vez, con las fichas de hoy y sin el objeto que no se entendía" \
  "(an, al, n, tam) == (559, 794, 2, 40) and [r.tipo for r in o] == [RECT, TEXTO] and o[1].txt == 'del futuro'"
chk $T/a2.ppm busca 150 200 650 400 0 8 "La publicación tiene cambios sin guardar."
chk $T/a3.ppm nobusca 8 30 400 50 8 4 "Publicaciones"       # cerrada
chk $T/a5.ppm busca 100 200 700 400 0 8 "No se ha podido guardar la publicación."
mattrib -i $T/f.img -r ::SOLOLEE.PUB; rm -f $T/sololee2.pub; mcopy -n -i $T/f.img ::SOLOLEE.PUB $T/sololee2.pub 2>/dev/null
if cmp -s $T/carta.pub $T/sololee2.pub && ! mdir -i $T/f.img :: | grep -qi 'SOLOLEE *TMP'; then ok "si no se puede guardar, el archivo de antes sigue entero y no queda el temporal"
else ko "el archivo de antes ha cambiado, o ha quedado el temporal"; mdir -i $T/f.img :: | grep -i sololee; fi
mcopy -n -i $T/f.img ::CARTA.PUB $T/carta2.pub
if cmp -s $T/carta.pub $T/carta2.pub; then ok "CARTA.PUB no ha cambiado (se contestó que no)"; else ko "CARTA.PUB ha cambiado"; fi
muestra g1 publica_guardar; muestra a1 publica_abierta
fi

if fase 5; then
echo "== Publicaciones: deshacer, duplicar, el orden, el portapapeles y la rejilla =="
prepara $HVP
{ arranca
  al100
  boton1 $B_RECT; arr 60 60 200 160; boton2 444; lista 5    # A: un rectángulo terracota
  tecla ctrl-z; aparca; shot d1; tecla ctrl-y; shot d2      # deshacer el color; rehacerlo
  tecla ctrl-r; boton2 444; lista 11; aparca; shot d3       # B: su copia (10 puntos más allá), en ocre
  tecla ctrl-o; shot d4; tecla ctrl-f; shot d5              # al fondo; al frente
  tecla ctrl-c ctrl-v ctrl-v; aparca; shot d6               # copiar y pegar dos veces
  tecla ctrl-x; shot d7; tecla ctrl-v; shot d8              # cortar la última; pegar
  tecla right right right; shot d9; tecla ctrl-z; shot d10  # tres flechas son un solo cambio
  tecla tab; shot d11; tecla shift-tab; shot d12            # Tab elige el siguiente (da la vuelta); Mayús+Tab vuelve
  menu $M_HERR 87                                           # Herramientas > Ajustar a la rejilla
  boton1 $B_RECT; arr 65 205 203 297; aparca; shot d13; tecla right; shot d14
  menu $M_HERR 87
  boton1 $B_TEXTO; arr 300 60 500 120; escribe "abc"; tecla esc ctrl-c   # el texto de una caja, al portapapeles
  boton1 $B_TEXTO; arr 300 140 500 200; tecla ctrl-v; escribe "d"; tecla esc; aparca; shot d15
  guarda orden
  sal; } | run 240 -vga std
chk $T/d1.ppm px $((PX+100)) $((PY+100)) 9 "deshecho: sin relleno"
chk $T/d2.ppm px $((PX+100)) $((PY+100)) 4 "rehecho: terracota"
chk $T/d3.ppm px $((PX+100)) $((PY+100)) 10 "la copia, encima, en ocre" px $((PX+65)) $((PY+65)) 4 "y el original, debajo"
chk $T/d3.ppm busca $PIE "Página 1 de 1    Rectángulo: x 19, y 19 mm; 37 x 26 mm"
chk $T/d4.ppm px $((PX+100)) $((PY+100)) 4 "al fondo: se ve el original" px $((PX+205)) $((PY+165)) 10 "y la copia asoma"
chk $T/d5.ppm px $((PX+100)) $((PY+100)) 10 "al frente otra vez"
chk $T/d6.ppm busca $PIE "Página 1 de 1    Rectángulo: x 24, y 24 mm; 37 x 26 mm"
chk $T/d6.ppm px $((PX+225)) $((PY+185)) 10 "la segunda copia pegada llega a (230, 190)"
chk $T/d7.ppm px $((PX+225)) $((PY+185)) 9 "cortada"
chk $T/d7.ppm busca $PIE "Página 1 de 1    A4 vertical, 210 x 297 mm    Vista: 100 %"
chk $T/d8.ppm busca $PIE "Página 1 de 1    Rectángulo: x 26, y 26 mm; 37 x 26 mm"
chk $T/d9.ppm busca $PIE "Página 1 de 1    Rectángulo: x 27, y 26 mm; 37 x 26 mm"
chk $T/d10.ppm busca $PIE "Página 1 de 1    Rectángulo: x 26, y 26 mm; 37 x 26 mm"
chk $T/d11.ppm busca $PIE "Página 1 de 1    Rectángulo: x 16, y 16 mm; 37 x 26 mm"
chk $T/d12.ppm busca $PIE "Página 1 de 1    Rectángulo: x 26, y 26 mm; 37 x 26 mm"
chk $T/d13.ppm busca $PIE "Página 1 de 1    Rectángulo: x 15, y 55 mm; 40 x 25 mm    Ajuste a la rejilla"
chk $T/d14.ppm busca $PIE "Página 1 de 1    Rectángulo: x 20, y 55 mm; 40 x 25 mm    Ajuste a la rejilla"
chk $T/d15.ppm busca $((PX+302)) $((PY+142)) $((PX+306)) $((PY+146)) 0 9 "abcd"
muestra d6 publica_copias; muestra d15 publica_orden
mira_pub ORDEN.PUB "el original, sus copias (cada una 10 puntos más allá), el ajustado y los textos" \
  "[r.tipo for r in o] == [RECT] * 5 + [TEXTO] * 2 and [r.caja[:2] for r in o[:4]] == [(60, 60), (70, 70), (80, 80), (100, 100)] \
   and [r.fondo for r in o[:5]] == [4, 10, 10, 10, 10] and all(r.caja[2:] == (140, 100) for r in o[:4]) \
   and o[4].caja == (76, 208, 151, 94) and o[5].txt == 'abc' and o[6].txt == 'abcd'"
fi

if fase 6; then
echo "== Publicaciones: varias páginas =="
prepara $HVP
{ arranca
  al100
  menu $M_INS 107; arr 60 60 200 160                        # página 1: un rectángulo (Insertar > Rectángulo)
  boton1 472; aparca; shot p1                               # una página nueva: la 2
  boton1 $B_ELIPSE; arr 60 60 200 160                       # con una elipse
  boton1 472; boton1 $B_LINEA; arr 60 60 200 60             # y la 3, con una línea
  tecla esc pgup; aparca; shot p2                           # RePág: a la 2
  boton1 420; aparca; shot p3; boton1 446                   # el botón de la anterior: la 1; y a la 2 otra vez
  menu $M_EDIC 271; shot p4; tecla ret; sleep 0.8; aparca; shot p5     # Edición > Quitar esta página
  tecla ctrl-z; shot p6; tecla ctrl-y; shot p7              # deshacer la devuelve; rehacer la quita
  guarda paginas
  sal; } | run 200 -vga std
chk $T/p1.ppm busca $PIE "Página 2 de 2    A4 vertical, 210 x 297 mm    Vista: 100 %"
chk $T/p1.ppm px $((PX+60)) $((PY+100)) 9 "la página nueva, vacía"
chk $T/p2.ppm busca $PIE "Página 2 de 3    A4 vertical, 210 x 297 mm    Vista: 100 %"
chk $T/p2.ppm px $((PX+60)) $((PY+110)) 0 "en la 2, la elipse" px $((PX+61)) $((PY+61)) 9 "(y no el rectángulo)"
chk $T/p3.ppm busca $PIE "Página 1 de 3    A4 vertical, 210 x 297 mm    Vista: 100 %"
chk $T/p3.ppm px $((PX+60)) $((PY+61)) 0 "en la 1, el rectángulo"
chk $T/p4.ppm busca 100 200 700 400 0 8 "Se va a quitar esta página con todo lo que tiene."
chk $T/p5.ppm busca $PIE "Página 2 de 2    A4 vertical, 210 x 297 mm    Vista: 100 %"
chk $T/p5.ppm px $((PX+130)) $((PY+60)) 0 "quitada la 2: ahora la 2 es la de la línea" px $((PX+60)) $((PY+110)) 9 "(sin la elipse)"
chk $T/p6.ppm busca $PIE "Página 2 de 3    A4 vertical, 210 x 297 mm    Vista: 100 %"
chk $T/p6.ppm px $((PX+60)) $((PY+110)) 0 "deshecho: vuelve la página de la elipse"
chk $T/p7.ppm busca $PIE "Página 2 de 2    A4 vertical, 210 x 297 mm    Vista: 100 %"
mira_pub PAGINAS.PUB "dos páginas: el rectángulo en la primera y la línea en la segunda" \
  "pags == 2 and [(r.tipo, r.pag) for r in o] == [(RECT, 0), (LINEA, 1)] and o[1].caja == (60, 60, 140, 0)"
fi

if fase 7; then
echo "== Publicaciones: las plantillas =="
prepara $HVP
nueva() { tecla ctrl-u; sleep 1.2; local i; for i in $(seq 1 $1); do tecla down; done
          if [ $2 -gt 0 ]; then tecla tab; for i in $(seq 1 $2); do tecla down; done; fi
          [ "${3:-}" ] && shot $3; tecla ret; sleep 2; }
{ arranca
  nueva 1 0 n0; aparca; shot n1; al100; shot n1b; boton1 $B_VISTA; guarda cartel   # cartel, en A4 de pie
  nueva 2 2; aparca; shot n2; guarda panfleto               # panfleto, en A5 de pie
  nueva 3 0; aparca; shot n3; guarda tarjeta                # tarjeta de visita (en su papel)
  nueva 4 1; aparca; shot n4; guarda anuncio                # anuncio con tiras, en A4 apaisado
  nueva 0 3; aparca; shot n5                                # en blanco, en A5 apaisado
  menu $M_FORM 447; lista 0; aparca; shot n6                # Formato > Tamaño de la página: A4 de pie
  tecla ctrl-z; shot n7                                     # y se deshace
  sal; } | run 240 -vga std
chk $T/n0.ppm busca 100 150 700 450 9 12 "Cartel"
chk $T/n0.ppm busca 100 150 700 450 0 9 "Anuncio con tiras" busca 100 150 700 450 0 9 "A5 horizontal"
chk $T/n1.ppm px 400 160 2 "el cartel: la franja de arriba" px 400 520 4 "y la de abajo" px 400 280 10 "y la elipse"
chk $T/n1b.ppm busca $PIE "Página 1 de 1    A4 vertical, 210 x 297 mm    Vista: 100 %"
chk $T/n2.ppm busca $PIE "Página 1 de 1    A5 vertical, 148 x 210 mm    Vista: 51 %"
chk $T/n3.ppm busca $PIE "Página 1 de 1    Tarjeta (85 x 55 mm)    Vista: 100 %"
chk $T/n4.ppm busca $PIE "Página 1 de 1    A4 horizontal, 297 x 210 mm    Vista: 51 %"
chk $T/n5.ppm busca $PIE "Página 1 de 1    A5 horizontal, 210 x 148 mm    Vista: 72 %"
chk $T/n6.ppm busca $PIE "Página 1 de 1    A4 vertical, 210 x 297 mm    Vista: 36 %"
chk $T/n7.ppm busca $PIE "Página 1 de 1    A5 horizontal, 210 x 148 mm    Vista: 72 %"
muestra n0 publica_nuevo; muestra n1 publica_cartel; muestra n1b publica_cartel100; muestra n2 publica_panfleto
muestra n3 publica_tarjeta; muestra n4 publica_anuncio
mira_pub CARTEL.PUB "el cartel: ocho objetos en un A4" \
  "(an, al, margen, n) == (794, 1123, 38, 8) and o[1].txt == 'GRAN FIESTA' and (o[1].letra, o[1].cuerpo, o[1].alin) == (10, 5, 1) \
   and o[0].caja == (0, 0, 794, 258) and o[3].tipo == ELIPSE and 'Plaza Mayor' in o[7].txt"
mira_pub PANFLETO.PUB "el panfleto: título, línea, frase y tres bloques en un A5, con la letra pequeña" \
  "(an, al, n) == (559, 794, 16) and o[1].cuerpo == 3 and o[2].tipo == LINEA and [r.txt for r in o if r.txt.endswith('bloque')] == ['Primer bloque', 'Segundo bloque', 'Tercer bloque']"
mira_pub TARJETA.PUB "la tarjeta de visita, de 85 x 55 mm" \
  "(an, al, margen, n) == (321, 208, 19, 6) and o[2].txt == 'Nombre Apellidos' and 'correo@ejemplo.es' in o[5].txt"
mira_pub ANUNCIO.PUB "el anuncio: ocho tiras iguales para arrancar, en un A4 apaisado" \
  "(an, al, n) == (1123, 794, 12) and o[0].txt == 'SE ALQUILA' and len([r for r in o if r.txt.startswith('Tel.')]) == 8 \
   and [r.x for r in o[4:]] == [i * 125 * 1123 // 1000 for i in range(8)]"
fi

if fase 8; then
echo "== Publicaciones: imprimir y exportar la página como BMP =="
prepara $HVP
rm -f $T/prn.bin
{ arranca
  tecla ctrl-u; sleep 1.2; tecla down down down ret; sleep 2          # la tarjeta de visita
  tecla ctrl-p; sleep 6; shot i1; tecla ret; sleep 0.8
  menu $M_ARCH 223; sleep 4; shot i2; tecla ret; sleep 0.8            # Archivo > Imprimir en borrador
  menu $M_ARCH 175; sleep 0.5; escribe "tarj"; tecla ret; sleep 3; shot i3; tecla ret; sleep 0.8   # Exportar como BMP
  tecla ctrl-u; sleep 1.2; tecla tab down down down ret; sleep 2      # en blanco, en A5 apaisado: se imprime girado
  al100; boton1 $B_RECT; arr 40 40 140 100; boton2 444; lista 1       # un rectángulo negro...
  boton1 472; boton1 $B_RECT; arr 200 100 300 200                     # ...y en otra página, otro
  tecla ctrl-p; sleep 9; shot i4; tecla ret; sleep 0.8
  sal; } | PARALELO=file:$T/prn.bin run 240 -vga std
chk $T/i1.ppm busca 100 200 700 400 0 8 "La publicación se ha enviado a la impresora."
chk $T/i2.ppm busca 100 200 700 400 0 8 "La publicación se ha enviado a la impresora."
chk $T/i3.ppm busca 100 200 700 400 0 8 "La página se ha guardado como imagen BMP."
chk $T/i4.ppm busca 100 200 700 400 0 8 "La publicación se ha enviado a la impresora."
if python3 $T/prn.py $T/prn.bin
then ok "impresora: cuatro páginas en imagen de bits (ESC L y ESC K), tramadas, y las apaisadas, giradas"
else ko "impresora: no ha salido lo esperado"; ls -l $T/prn.bin; xxd $T/prn.bin | head -8; fi
mcopy -n -i $T/f.img ::TARJ.BMP $T/tarj.bmp 2>/dev/null
python3 tests/tools/pintura_bmp.py mira $T/tarj.bmp tam 321 208 px 10 100 2 "la franja petróleo" px 31 100 10 "la raya ocre" \
  px 200 200 9 "el papel" px 100 91 7 "la línea" || fail=1
[ -f $T/tarj.bmp ] && convert $T/tarj.bmp $B/publica_exportada.png
fi

if fase 9; then
echo "== Publicaciones: imágenes BMP =="
prepara $HVP
mkdir -p $T/bmp; python3 tests/tools/pintura_bmp.py crea $T/bmp > /dev/null
mcopy -o -i $T/f.img $T/bmp/T4.BMP $T/bmp/T8.BMP $T/bmp/RLE.BMP $T/bmp/ENORME.BMP ::
{ arranca
  al100
  menu $M_INS 175; sleep 0.5; escribe "t8.bmp"; tecla ret; sleep 2; aparca; shot b1      # Insertar > Imagen (256 colores)
  boton1 388; sleep 1; escribe "rle.bmp"; tecla ret; sleep 2; shot b2; tecla ret; sleep 0.8   # una comprimida: no vale
  boton1 388; sleep 1; escribe "enorme.bmp"; tecla ret; sleep 2; shot b3; tecla ret; sleep 0.8   # ni una de 2000 de ancho
  boton1 388; sleep 1; escribe "t4.bmp"; tecla ret; sleep 2                              # otra, de 16 colores
  tecla down down down down down down down down down down                               # diez puntos más abajo
  arr 104 78 170 108; tecla esc; aparca; shot b4                                         # y al doble, por el tirador
  guarda imagen
  sal; } | run 200 -vga std
# T8.BMP (83 x 37): arriba verde oliva; debajo, bandas de 8 (terracota, añil, rojo, gris... blanco)
chk $T/b1.ppm px $((PX+38+10)) $((PY+38+2)) 11 "la imagen de 256 colores: arriba, verde oliva" \
              px $((PX+38+4)) $((PY+38+20)) 4 "la banda terracota" px $((PX+38+12)) $((PY+38+20)) 12 "la añil" \
              px $((PX+38+52)) $((PY+38+20)) 9 "la blanca" rgb $((PX+38+20)) $((PY+38+20)) FF0000 "y la roja, del cubo de colores"
chk $T/b1.ppm busca $PIE "Página 1 de 1    Imagen: x 10, y 10 mm; 22 x 10 mm"
chk $T/b2.ppm busca 100 200 700 400 0 8 "Ese archivo no es una imagen BMP de las que se admiten"
chk $T/b3.ppm busca 100 200 700 400 0 8 "La imagen es demasiado grande para la publicación."
# T4.BMP (66 x 30, bandas de 4 con los 16 colores), en (38, 48) y al doble: bandas de 8
chk $T/b4.ppm px $((PX+38+4)) $((PY+48+30)) 0 "la de 16 colores, al doble: la banda de tinta" px $((PX+38+12)) $((PY+48+30)) 1 "la marrón" \
              px $((PX+38+36)) $((PY+48+30)) 4 "la terracota" px $((PX+38+124)) $((PY+48+30)) 15 "y la última, piedra" \
              px $((PX+38+124)) $((PY+48+4)) 0 "arriba van al revés" px $((PX+38+60)) $((PY+38+3)) 11 "y detrás asoma la otra"
muestra b1 publica_imagen; muestra b4 publica_imagenes
mira_pub IMAGEN.PUB "las dos imágenes, con sus puntos ya en la paleta" \
  "[r.tipo for r in o] == [IMAGEN, IMAGEN] and (o[0].ian, o[0].ial, len(o[0].dat)) == (83, 37, 83 * 37) and o[0].caja == (38, 38, 83, 37) \
   and (o[1].ian, o[1].ial) == (66, 30) and o[1].caja == (38, 48, 132, 60) and o[0].dat[0] == 11 and o[0].dat[5 * 83] == 4 \
   and all(o[1].dat[y * 66 + x] == (15 - x // 4 % 16 if y < 6 else x // 4 % 16) for y in range(30) for x in range(66))"
fi

rm -rf $T
exit $fail
