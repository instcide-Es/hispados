#!/bin/bash
# Prueba del tema «Sorpresa» de Configuración (CONFIG.HVP).
#   «Sorpresa» (Pantalla › Tema): en cada arranque el Centro de programas carga
#   CONFIG.HVP con «/A» y, si está puesto, sortea uno de los 8 temas y lo aplica
#   al escritorio (en vez del tema guardado). Esta prueba arranca Hispa Ventanas
#   varias veces con «Sorpresa=1» y un fondo guardado que NO es de ningún tema
#   (piedra); comprueba que el escritorio sale SIEMPRE con uno de los 7 colores
#   de fondo de los temas (nunca piedra, nunca se cuelga) y que con el tiempo
#   salen temas distintos (de verdad se sortea).
set -u
cd "$(dirname "$0")/.."
B=build
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }

mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }

# arranca Hispa Ventanas una vez; deja la captura del escritorio en $T/$1.ppm.
# $2 = segundos de reloj para sembrar el azar distinto en cada arranque.
arranca() {
  cp "$IMG" $T/f.img
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  printf '%s\r\n' '[Pantalla]' 'Dibujo=damero' 'ColorFondo=15' 'ColorDibujo=0' \
    'Sorpresa=1' > $T/VENTANAS.INI     # 15=piedra (no es fondo de ningún tema)
  mcopy -o -i $T/f.img $T/VENTANAS.INI ::HISPADOS/VENTANAS.INI
  rm -f $T/mon
  { sleep 6
    send 'A:\HISPADOS\HSPRATON' 2
    send 'VENTANAS' 7
    shot "$1"
    mon quit
  } | timeout 90 qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -parallel none -m 16 -rtc "base=2026-11-15T10:${2}:00" \
      -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a > $T/raw 2>&1
}

# cuál de los 7 fondos de tema domina la captura (o «ninguno»); también avisa si
# hay mucho «piedra» (el fondo guardado, que NO se debería ver con Sorpresa).
# fondos de tema: petróleo 31,78,90 · tinta 26,20,16 · añil 47,75,143 ·
#   oliva 107,142,61 · granate 158,43,43 · arena_osc 143,127,104 · terracota 194,84,45
# piedra (no-tema): 216,210,196
dominante() {
  python3 -c '
import re, sys
d = open(sys.argv[1], "rb").read()
m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", d); d = d[m.end():]
temas = {"petroleo":(31,78,90),"tinta":(26,20,16),"anil":(47,75,143),
         "oliva":(107,142,61),"granate":(158,43,43),"arena_osc":(143,127,104),
         "terracota":(194,84,45)}
piedra = (216,210,196)
cnt = {k:0 for k in temas}; piedras = 0
def cerca(px, c): return all(abs(px[k]-c[k]) < 18 for k in range(3))
for i in range(0, len(d), 3):
    px = (d[i], d[i+1], d[i+2])
    for k,c in temas.items():
        if cerca(px, c): cnt[k]+=1; break
    else:
        if cerca(px, piedra): piedras += 1
best = max(cnt, key=cnt.get)
print(best, cnt[best], piedras)' "$1"
}

echo "== Configuración: el tema «Sorpresa» (sorteo en cada arranque) =="
temas_vistos=""
for n in 03 27 51; do          # tres arranques con el reloj (segundos) distinto
  arranca sorp$n $n
  read -r tema pts piedras < <(dominante $T/sorp$n.ppm)
  if [ "${pts:-0}" -gt 80000 ]; then
    ok "arranque (reloj :$n): el escritorio toma el tema «$tema» ($pts puntos de fondo)"
  else
    ko "arranque (reloj :$n): no se ve un fondo de tema (mejor «$tema», $pts puntos)"
  fi
  # el escritorio tiene algo de cromo color piedra (barra, botones): unos pocos
  # miles de puntos fijos. Si NO se aplicara Sorpresa, el fondo entero sería
  # piedra (cientos de miles). Con un buen margen: menos de 30000 = solo cromo.
  if [ "${piedras:-0}" -lt 30000 ]; then
    ok "arranque (reloj :$n): el fondo guardado «piedra» no cubre el escritorio ($piedras puntos, solo cromo): Sorpresa manda"
  else
    ko "arranque (reloj :$n): «piedra» cubre el escritorio ($piedras puntos): no se aplicó Sorpresa"
  fi
  case " $temas_vistos " in *" $tema "*) : ;; *) temas_vistos="$temas_vistos $tema" ;; esac
done
# con tres arranques y 8 temas, lo normal es ver más de uno (si siempre saliera
# el mismo, el «azar» no estaría sembrado). No es un fallo duro: solo aviso.
nvistos=$(echo $temas_vistos | wc -w)
if [ "$nvistos" -ge 2 ]; then
  ok "en tres arranques salen temas distintos ($temas_vistos): el azar se siembra"
else
  echo "  AVISO en tres arranques salió siempre «$temas_vistos» (puede ser casualidad: 1 de 8^2)"
fi

[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/ 2>/dev/null
rm -rf $T
exit $fail
