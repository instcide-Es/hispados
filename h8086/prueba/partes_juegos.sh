# Las partes de la prueba de los juegos (las carga prueba.sh): DERRIBO, en
# el disquete de juegos puesto en la unidad B:.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

# la pantalla de la CGA (16 K, con las líneas pares y las impares por
# separado) y el modo de vídeo de la BIOS; $1 = el nombre de la captura
cga() {
  mon "pmemsave 0xb8000 16384 \"$T/$1.cga\"" 0.3
  mon "pmemsave 0x449 1 \"$T/$1.modo\"" 0.3
}
# lo que dice de una captura de la CGA: «modo N», los puntos de cada color,
# y dónde está la pelota (un 4×4 de color 3 entre los ladrillos y la pala)
mira_cga() {
  python3 - $T/$1.cga $T/$1.modo <<'PY' > $T/$1.txt
import sys
d = open(sys.argv[1], 'rb').read()
print("modo", open(sys.argv[2], 'rb').read()[0])
def px(x, y):
    b = d[(y & 1) * 0x2000 + (y >> 1) * 80 + (x >> 2)]
    return (b >> ((3 - (x & 3)) * 2)) & 3
n = [0] * 4
for y in range(200):
    for x in range(320):
        n[px(x, y)] += 1
print("colores", *[min(c, 1) for c in n[1:]])
print("puntos", *n)
p = [(x, y) for y in range(80, 184) for x in range(4, 316) if px(x, y) == 3]
print("pelota", "%d,%d" % (min(p)[0], min(q[1] for q in p)) if len(p) == 12 else "no")
# los cuatro ladrillos de la portada (x 28 a 51, filas 3, 5, 7 y 9), en
# forma: que cada uno se distinga de los otros aun sin color
f = ["".join("x" if px(x, 56 + 16 * k + y) else "." for y in range(8) for x in range(28, 52)) for k in range(4)]
print("formas", len(set(f)), "distintas")
PY
}

p_derribo() {
echo "== Hispa-8086: DERRIBO, el juego de pala y ladrillos (disquete de juegos en B:) =="
if grep -q "^\s*cpu 8086" juegos/derribo.asm; then ok "juegos/derribo.asm se ensambla con «cpu 8086»"; else ko "juegos/derribo.asm no lleva «cpu 8086»"; fi
tam=$(stat -c %s build/DERRIBO.COM)
if [ $tam -lt 8192 ]; then ok "DERRIBO.COM ocupa $tam octetos"; else ko "DERRIBO.COM ha crecido a $tam octetos"; fi
# los ladrillos que se pueden romper, contados en el dibujo de los muros
local n=$(python3 - juegos/derribo.asm <<'PY'
import re, sys
s = open(sys.argv[1], encoding='utf-8').read()
t = 0
for m in re.finditer(r'^\s+MURO (.*?)\s*;', s, re.M):
    for f in re.findall(r'"([^"]*)"', m.group(1)):
        t += sum(c in '123' for c in f + f[-2::-1])
print(t)
PY
)
cp $IMG/hispa8086-360.img $T/a.img
cp build/juegos8086-360.img $T/b.img
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "b:"; escribe "dir"
pantalla d0
esta d0 "DERRIBO.COM" "el disquete de juegos: "
esta d0 "JUEGOS.TXT" ""
# 1. el motor, sin pantalla: los diez muros, dos veces (a más velocidad de la
# que hay en el juego, y a la de empezar), con tres pelotas y la pala de
# pared a pared, mirando en cada paso que ninguna pelota se meta en un
# ladrillo, en una pared o pase la pala, ni salte más de un punto
escribe "limpiar"; escribe "derribo /p"
if espera d1 "fallos" 90; then ok "DERRIBO /P acaba"; else ko "DERRIBO /P no acaba"; fi
esta d1 "Motor: 20 de 20 muros derribados" "ningún muro se queda a medias (sin bucles eternos): "
esta d1 "Motor: $n ladrillos en los diez muros" "los muros guardados en bits se leen como están dibujados: "
esta d1 "Motor: 0 fallos" "ninguna pelota atraviesa un ladrillo, una pared ni la pala: "
# 2. en la CGA (en QEMU, la VGA, que es compatible)
escribe "limpiar"; escribe "derribo"; sleep 2
cga g1; mira_cga g1
esta g1 "modo 4" "gráficos de 320×200 a cuatro colores: "
esta g1 "colores 1 1 1" "la portada usa los tres colores: "
esta g1 "formas 4 distintas" "los cuatro ladrillos se distinguen por la forma: "
tecla spc; sleep 1.5
cga g2; mira_cga g2
esta g2 "pelota 158,180" "la pelota, pegada a la pala, en medio: "
tecla spc; sleep 0.6
cga g3; mira_cga g3
local p3=$(grep -o "pelota [0-9]*,[0-9]*" $T/g3.txt) p4 i
for ((i=0; i<10; i++)); do    # (con el equipo cargado, dos fotos seguidas
  sleep 0.4; cga g4; mira_cga g4  # pueden pillar la pelota quieta: hasta 10)
  p4=$(grep -o "pelota [0-9]*,[0-9]*" $T/g4.txt)
  [ -n "$p4" ] && [ "$p3" != "$p4" ] && break
done
if [ -n "$p3" ] && [ -n "$p4" ] && [ "$p3" != "$p4" ]; then ok "con Espacio, la pelota sale y se mueve ($p3 y $p4)"; else ko "la pelota no se mueve: «$p3» «$p4»"; fi
sleep 2; tecla esc; sleep 1; tecla spc; sleep 1; tecla esc; sleep 2
escribe "limpiar"; escribe "mostrar derribo.rec"; escribe "dir derribo.rec"
pantalla d2
esta d2 'B:\>' "Esc, Espacio y Esc: vuelve al sistema, en modo de texto: "
esta d2 "DERRIBO.REC            8" "guarda la mejor puntuación: "
if grep -qE "^[0-9]{5}0$" $T/d2.txt; then ok "DERRIBO.REC: «$(grep -E "^[0-9]{5}0$" $T/d2.txt)»"; else ko "DERRIBO.REC no tiene la puntuación"; fi
local rec=$(grep -E "^[0-9]{5}0$" $T/d2.txt)
# 3. monocromo: el modo 5 y un solo color (lo demás, por la forma)
escribe "limpiar"; escribe "derribo /m"; sleep 2
cga m1; mira_cga m1
esta m1 "modo 5" "DERRIBO /M, monocromo: el modo 5 (sin color en la salida compuesta): "
esta m1 "colores 0 0 1" "todo de un solo color: "
esta m1 "formas 4 distintas" "y los ladrillos, distintos por la forma: "
tecla spc; sleep 1; tecla spc; sleep 1
cga m2; mira_cga m2
esta m2 "colores 0 0 1" "también en el juego: "
tecla esc; sleep 1; tecla spc; sleep 1; tecla esc; sleep 2
# 4. en texto (como con MDA o Hercules)
escribe "limpiar"; escribe "derribo /t"; sleep 2
pantalla t1
esta t1 "D E R R I B O" "DERRIBO /T, en texto: "
esta t1 "▓▓3▓▓    3 golpes" "los ladrillos, por la forma y el número: "
esta t1 "(T)  Tres pelotas" "los premios, por la letra: "
esta t1 "Mejor puntuación: $rec" "lee la mejor puntuación guardada: "
tecla spc; sleep 1.5
pantalla t2
esta t2 "Puntos 000000  Mejor $rec Nivel 01" "la fila de arriba: "
esta t2 "▐█████ █████ █████" "el primer muro: "
esta t2 "Espacio: lanzar   P: pausa   Esc: fin" ""
esta t2 "▀▀▀▀▀▀▀▀" "la pala: "
tecla p; sleep 0.8
pantalla t3
esta t3 "Pausa. Pulse P para seguir" "P, pausa: "
tecla p; sleep 0.5; tecla esc; sleep 1
pantalla t4
esta t4 "Fin de la partida. Pulse Espacio" "Esc acaba la partida: "
tecla spc; sleep 1; tecla esc; sleep 2
escribe "limpiar"; escribe "ver"
pantalla t5
esta t5 "Hispa-8086 versión" "y el sistema sigue bien: "
apaga
if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en el disquete de juegos"; else ko "fsck.fat encuentra errores en el disquete de juegos"; cat $T/fsck.txt; fi
}
