# La parte de la prueba de Marcianada (juegos/marciana.asm), en el disquete
# de juegos (que va en B:). La carga prueba.sh: SOLO=marcianos la corre sola.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

# la pantalla gráfica, tal como la ve QEMU (screendump, 640x400): $1 = nombre
foto() { mon "screendump $T/$1.ppm" 0.6; }
# lo que dice de una foto: colores (los que no son negro) y puntos pintados en
# el rectángulo x0 y0 x1 y1 (en puntos de la CGA, 320x200)
mira_foto() {
  python3 - "$T/$1.ppm" "${2:-0}" "${3:-0}" "${4:-320}" "${5:-200}" <<'PY'
import sys
d = open(sys.argv[1], 'rb').read().split(b'\n', 3)
w, h = map(int, d[1].split()); px = d[3]
x0, y0, x1, y1 = map(int, sys.argv[2:6])
sx, sy = w // 320, h // 200
colores, n = set(), 0
for y in range(y0, y1):
    for x in range(x0, x1):
        i = (y * sy * w + x * sx) * 3
        c = px[i:i + 3]
        if c != b'\0\0\0':
            colores.add(c); n += 1
print(len(colores), n)
PY
}
# la pantalla de texto con las letras gráficas de la página 437 (también las
# de 01h a 1Fh, que son las caras, el corazón...)
pantalla437() {
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.4
  python3 - $T/scr.bin <<'PY' > $T/$1.txt
import sys
bajas = ' ☺☻♥♦♣♠•◘○◙♂♀♪♫☼►◄↕‼¶§▬↨↑↓→←∟↔▲▼'
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print(''.join(bajas[c] if c < 32 else bytes([c]).decode('cp437') for c in d[f * 160:(f + 1) * 160:2]).rstrip())
PY
}
modo_video() { mon "pmemsave 0x449 1 \"$T/modo.bin\"" 0.3; od -An -tu1 $T/modo.bin | tr -d ' '; }
reloj_bios() { mon "pmemsave 0x46c 4 \"$T/reloj.bin\"" 0.3; od -An -tu4 $T/reloj.bin | tr -d ' '; }

# Esc hasta volver al sistema: de la partida a «fin de la partida» (que no
# atiende teclas el primer segundo), a la portada y fuera. Si la partida ya
# se ha acabado sola, hacen falta menos (y con /T el juego también está en
# el modo 3: se mira que haya vuelto el indicador del sistema).
sal_del_juego() {
  local i
  for i in 1 2 3 4 5 6; do
    tecla esc; sleep 1.4
    [ "$(modo_video)" = 3 ] && pantalla437 sx && grep -qF 'B:\>' $T/sx.txt && return
  done
}

p_marcianos() {
echo "== Hispa-8086: Marcianada, el juego de marcianitos (disquete de juegos en B:) =="
local m n c r1 r2 rec
make -s build/MARCIANA.COM build/MARPRUEB.COM build/juegos8086-360.img > /dev/null || ko "no se puede hacer el disquete de juegos"
for f in juegos/marciana.asm; do
  if grep -q "^\s*cpu 8086" $f && [ "$(grep -c '^\s*cpu ' $f)" = 1 ]; then ok "$f se ensambla con «cpu 8086» (y nada más)"; else ko "$f: falta «cpu 8086» o hay otro «cpu»"; fi
done
local tam=$(stat -c %s build/MARCIANA.COM)
if [ $tam -lt 8192 ]; then ok "MARCIANA.COM ocupa $tam octetos"; else ko "MARCIANA.COM ha crecido a $tam octetos"; fi
cp $IMG/hispa8086-360.img $T/a.img
cp build/juegos8086-360.img $T/j.img
mcopy -i $T/j.img build/MARPRUEB.COM ::
QMAS="-drive file=$T/j.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
# el motor, sin pantalla
escribe "b:"; escribe "marprueb"; sleep 3
pantalla437 mp
for t in "churro contra bicho" "hueco entre columnas" "churro contra muralla" "aceituna contra muralla" \
         "casco, dos tiros" "guindilla a la churrera" "la flota llega al suelo" "record en el disco"; do
  esta mp "$t: BIEN" "el motor: "
done
esta mp "MARCIANADA: fin de la prueba" "el motor: "
# el juego, en la CGA (modo 4)
escribe "limpiar"; escribe "marciana"; sleep 3
m=$(modo_video)
if [ "$m" = 4 ]; then ok "la portada, en el modo 4 (CGA, 320x200 a cuatro colores)"; else ko "la portada no está en el modo 4 ($m)"; fi
foto titulo
read c n < <(mira_foto titulo)
if [ "$c" = 3 ] && [ "$n" -gt 3000 ]; then ok "la portada pinta: $n puntos, de tres colores"; else ko "la portada: $c colores, $n puntos"; fi
tecla spc; sleep 1.5
foto j1
read c n < <(mira_foto j1 112 24 208 104)
if [ "$n" -gt 800 ]; then ok "la flota, pintada ($n puntos)"; else ko "la flota no se ve ($n puntos)"; fi
read c n < <(mira_foto j1 36 152 60 164)
if [ "$n" -gt 200 ]; then ok "la primera muralla, pintada ($n puntos)"; else ko "la primera muralla no se ve ($n puntos)"; fi
read c n < <(mira_foto j1 152 176 168 184)
if [ "$n" -gt 60 ]; then ok "la churrera, pintada ($n puntos)"; else ko "la churrera no se ve ($n puntos)"; fi
# el reloj de la BIOS sigue en hora (18,2 tics por segundo) aunque el 8254 vaya a 60
r1=$(reloj_bios); sleep 3; r2=$(reloj_bios)
if [ $((r2 - r1)) -ge 45 ] && [ $((r2 - r1)) -le 75 ]; then ok "con el juego en marcha, el reloj de la BIOS avanza lo suyo ($((r2 - r1)) tics en unos 3 s)"; else ko "el reloj de la BIOS: $((r2 - r1)) tics en 3 s"; fi
# a la izquierda y tirando: la churrera se mueve
mon "sendkey left" 0.05; mon "sendkey left" 0.05; mon "sendkey left" 0.05
for i in 1 2 3 4 5 6 7 8; do tecla spc; sleep 0.9; done
foto j2
read c n < <(mira_foto j2 152 176 168 184)
read c m < <(mira_foto j2 120 176 152 184)
if [ "$m" -gt 0 ]; then ok "la churrera, movida a la izquierda"; else ko "la churrera no se ha movido ($m)"; fi
tecla p; sleep 1; foto pa1; sleep 1.5; foto pa2
if cmp -s $T/pa1.ppm $T/pa2.ppm; then ok "con P, todo quieto (pausa)"; else ko "en pausa la pantalla cambia"; fi
tecla p; sleep 1.5; foto pa3
if cmp -s $T/pa2.ppm $T/pa3.ppm; then ko "otra P no sigue la partida"; else ok "otra P y sigue"; fi
sal_del_juego
pantalla437 fuera
m=$(modo_video)
if [ "$m" = 3 ]; then ok "al salir, la pantalla de texto como estaba"; else ko "al salir, modo $m"; fi
esta fuera 'B:\>' "y se vuelve al sistema: "
# en modo texto (como con una MDA o una Hercules)
escribe "limpiar"; escribe "marciana /t"; sleep 2
pantalla437 tt
esta tt "M A R C I A N A D A" "/T, la portada: "
esta tt "══" "/T: la Boina, con sus letras: "
rec=$(grep -o "RECORD [0-9]*" $T/tt.txt | head -1 | tr -dc 0-9)
tecla spc; sleep 1.5
pantalla437 tj
esta tj "PUNTOS" "/T, la partida: "
esta tj "══ ══ ══" "/T: las Boinas en fila: "
esta tj "ô) ô)" "/T: los Botijos: "
if grep -qF '\/ \/' $T/tj.txt || grep -qF '/\ /\' $T/tj.txt; then ok "/T: los Abanicos, abiertos o cerrados"; else ko "/T: no se ven los Abanicos"; fi
esta tj "▄██▄" "/T: la churrera: "
esta tj "██" "/T: las murallas: "
esta tj "▓" "/T: las almenas, a medio llenar: "
cp $T/tj.txt $T/captura_texto.txt
tecla s; sleep 0.6
pantalla437 ts
esta ts "SONIDO NO" "/T: con S, sin sonido: "
sal_del_juego
# monocromo con la CGA: modo 5 y un solo color
escribe "limpiar"; escribe "marciana /m"; sleep 2
m=$(modo_video)
if [ "$m" = 5 ]; then ok "/M: el modo 5 (sin color en la salida compuesta)"; else ko "/M: modo $m"; fi
tecla spc; sleep 1.5
foto mono
read c n < <(mira_foto mono 0 8 320 192)
if [ "$c" = 1 ] && [ "$n" -gt 1000 ]; then ok "/M: todo de un solo color ($n puntos)"; else ko "/M: $c colores, $n puntos"; fi
sal_del_juego
apaga
# el récord, en el disquete
local v=$(mtype -i $T/j.img ::MARCIANA.REC 2>/dev/null | od -An -tu2 | tr -d ' ')
if [ -n "$v" ] && [ "$v" -gt 0 ]; then ok "el récord, guardado en B:MARCIANA.REC ($v puntos)"; else ko "no hay récord en B:MARCIANA.REC"; fi
if [ -n "$rec" ] && [ "$rec" = "$(printf '%05d' "${v:-0}")" ]; then ok "y la portada lo enseña al volver a entrar ($rec)"; else ko "la portada enseña «$rec» y el archivo dice «$v»"; fi
v=$(mtype -i $T/j.img ::MARPRUEB.REC 2>/dev/null | od -An -tu2 | tr -d ' ')
if [ "$v" = 12345 ]; then ok "MARPRUEB.REC: el récord de la prueba del motor, en el disco"; else ko "MARPRUEB.REC dice «$v»"; fi
if fsck.fat -n $T/j.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra errores en el disquete de juegos"; else ko "fsck.fat encuentra errores en el disquete de juegos"; cat $T/fsck.txt; fi
[ -n "${CAPTURA:-}" ] && cp $T/captura_texto.txt "$CAPTURA"
}
