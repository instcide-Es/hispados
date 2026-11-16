#!/bin/bash
# Prueba de los cimientos de Hispa-8086: arranca el disquete en QEMU, teclea
# órdenes y mira lo que queda escrito en la pantalla de texto.
#
# QEMU no tiene un 8086: lo que aquí se comprueba es que el sistema arranca
# y se comporta, en un 486 y en un Pentium. Que no hay ni una instrucción
# posterior al 8086 lo garantiza el ensamblador («cpu 8086» en los fuentes).
# Necesita qemu-system-i386, socat y mtools.
set -u
cd "$(dirname "$0")/.."
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.25}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.12; done; }
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in ' ') tecla spc;; '.') tecla dot;; '*') tecla shift-8;; '<') tecla shift-comma;; '>') tecla shift-dot;; '=') tecla equal;; '\') tecla backslash;; *) tecla "$c";; esac
  done
  tecla ret; sleep 0.8
}
# la pantalla de texto (80x25, un octeto de letra y otro de color), a texto
pantalla() {
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.4
  python3 - $T/scr.bin <<'PY' > $T/$1.txt
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print(bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())
PY
}
esta() { if grep -qF -- "$2" $T/$1.txt; then ok "$3«$2»"; else ko "$3falta «$2»"; fi; }
noesta() { if grep -qF -- "$2" $T/$1.txt; then ko "$3sobra «$2»"; else ok "$3no sale «$2»"; fi; }
arranca() {   # imagen, cpu, memoria en K
  rm -f $T/mon
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu $2 -m 4 \
    -drive file=$1,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
  QPID=$!
  sleep 5
}
apaga() { mon quit 0.5; wait $QPID 2>/dev/null; }

echo "== Hispa-8086: arranque y órdenes (disquete de 360 K, en un 486) =="
cp build/hispa8086-360.img $T/a.img
arranca $T/a.img 486
pantalla p1
esta p1 "Hispa-8086 0.5 - de la familia Hispa-DOS" ""
esta p1 "Copyright (C) 2026 Carlos Sanz y contribuidores" ""
esta p1 "Procesador: 80486 o superior" ""
esta p1 "K libres para los programas)" ""
esta p1 "puede con HISPA-386" "el consejo: "
esta p1 'A:\>' ""
esta p1 "Arranque terminado (AUTOEXEC.BAT)." "AUTOEXEC.BAT al arrancar: "
noesta p1 "ECO OFF" "«@» delante: la línea no se enseña: "
noesta p1 "REM El arranque" "con ECO OFF no se enseñan las órdenes del lote: "
escribe "limpiar"; escribe "ver"; escribe "dir"
pantalla p2
esta p2 "Hispa-8086 versión 0.5" "VER: "
esta p2 "NUCLEO86.SYS" "DIR: "
esta p2 "HOLA.COM             206" "DIR: "
esta p2 "8 archivos, " "DIR: "
esta p2 "COMMAND.COM" "DIR: "
esta p2 "octetos libres" "DIR: "
noesta p2 "HISPA-8086 " "DIR: (la etiqueta) "
escribe "cls"; escribe "mostrar leeme.txt"
pantalla p3
esta p3 "El sistema de la familia Hispa para los PC, XT y AT: 8086, 8088, V20," "MOSTRAR: "
esta p3 "en español." "MOSTRAR: "
escribe "limpiar"; escribe "hola uno dos"; escribe "type nada.txt"; escribe "xyz"; escribe "equipo"
pantalla p4
esta p4 "Hola desde un programa .COM. DOS 3.30" "HOLA.COM: "
esta p4 "Parametros: uno dos" "HOLA.COM: "
esta p4 "Vectores: bien" "HOLA.COM: "
esta p4 "No existe ese archivo." ""
esta p4 "Orden desconocida. Escriba AYUDA." ""
esta p4 "Procesador: 80486 o superior" "EQUIPO: "

echo "== Hispa-8086: archivos por lotes y redirección =="
escribe "limpiar"; escribe "lote uno dos"; sleep 6
pantalla pl
esta pl "Lote: empiezo LOTE [uno] [dos] [] 100%" "%0, %1, %2 y %%: "
esta pl "Parametros: desde el lote" "lanza un programa: "
esta pl "Nivel: 7 o mas" "IF ERRORLEVEL: "
esta pl "Nivel: menos de 8" "IF NOT ERRORLEVEL: "
esta pl "Existe leeme" "IF EXIST: "
esta pl "No existe nohay" "IF NOT EXIST: "
esta pl "Lote: acabo" "GOTO y hasta el final: "
noesta pl "NO DEBE SALIR" "lo que no se cumple no se ejecuta: "
noesta pl "Primera linea" "con «>», a la pantalla no sale: "
escribe "limpiar"; escribe "mostrar sale.txt"; escribe "pausa < leeme.txt"; escribe "eco tras la pausa"
escribe "eco tecleado > t.txt"; escribe "if a==a mostrar t.txt"
pantalla pr
esta pr "Primera linea" "«>»: lo escrito está en el archivo: "
esta pr "Hispa-8086 versión 0.5" "«>>»: y lo añadido detrás: "
esta pr "pausa < leeme.txt" "(la orden, como se tecleó) "
esta pr "Pulse una tecla para seguir..." "«<»: la tecla sale del archivo: "
noesta pr "Orden desconocida" "y la orden siguiente llega entera: "
esta pr "tras la pausa" ""
esta pr "if a==a mostrar t.txt" "(la orden, como se tecleó) "
if [ "$(grep -c '^tecleado' $T/pr.txt)" = 1 ]; then ok "«>» desde el teclado e IF a==a: el texto solo sale al mostrar el archivo"; else ko "«>» desde el teclado"; fi

echo "== Hispa-8086: memoria, programas .EXE y lanzar otro programa =="
escribe "limpiar"; escribe "exe"; sleep 4; escribe "equipo"
pantalla pe
esta pe "EXE: datos bien" "un .EXE con sus direcciones reubicadas: "
esta pe "EXE: llamada lejana bien" ""
esta pe "EXE: memoria bien" "pedir, soltar y encoger memoria: "
esta pe 'EXE: soy A:\EXE.EXE' "su ruta, detrás del entorno: "
esta pe "Parametros: hijo" "lanza HOLA.COM con parámetros: "
esta pe "EXE: el hijo acaba con 7" "y recoge su código de salida: "
noesta pe "EXE: MAL" ""
libres=$(grep -o "([0-9]* K libres" $T/pe.txt | tr -dc 0-9)
antes=$(grep -o "([0-9]* K libres" $T/p1.txt | tr -dc 0-9)
if [ -n "$libres" ] && [ "$libres" = "$antes" ]; then ok "al acabar, la memoria vuelve a estar libre entera ($libres K)"; else ko "memoria libre: $antes K antes, $libres K después"; fi

echo "== Hispa-8086: archivos y directorios =="
TL=$(stat -c %s build/LEEME.TXT)      # lo que ocupa el archivo con el que se juega
fila() { printf '%-14s%10d' "$1" "$2"; }
escribe "limpiar"; escribe "arch"; sleep 6
pantalla p6
esta p6 "ARCHIVOS: BIEN" "la interfaz de INT 21h, paso a paso: "
# y con las órdenes: crear un directorio, copiar dentro, entrar, renombrar,
# copiar otra vez, borrar con comodín, salir
escribe "limpiar"; escribe "creardir cartas"; escribe "copiar leeme.txt cartas"; sleep 2
escribe "cd cartas"; escribe "renombrar leeme.txt juan.txt"; escribe "copy juan.txt ana.txt"; sleep 2
escribe "copiar juan.txt pepe.doc"; sleep 2; escribe "dir"
pantalla p7
esta p7 'A:\CARTAS>' "CD: "
esta p7 "1 archivo copiado." "COPIAR: "
esta p7 "$(fila JUAN.TXT $TL)" "RENOMBRAR y DIR: "
esta p7 "$(fila ANA.TXT $TL)" "COPIAR y DIR: "
esta p7 "5 archivos, $((3 * TL)) octetos" "DIR del directorio: "
escribe "limpiar"; escribe "borrar *.txt"; sleep 2; escribe "dir"
pantalla p8
noesta p8 "JUAN.TXT" "BORRAR *.TXT: "
esta p8 "$(fila PEPE.DOC $TL)" "BORRAR *.TXT deja lo demás: "
escribe "limpiar"; escribe "cd .."; escribe "borrardir cartas"
escribe "mostrar cartas\\pepe.doc"; escribe "cd nohay"; escribe "cd"
pantalla p9
esta p9 "No se puede: acceso denegado." "BORRARDIR con algo dentro: "
esta p9 "en español." "MOSTRAR con ruta: "
esta p9 "No existe esa ruta." "CD a donde no hay: "
apaga
# lo que ha quedado en el disquete, mirado desde fuera
if python3 - $T/a.img <<'PY'
import subprocess, sys
img = sys.argv[1]
def cat(n):
    return subprocess.run(['mtype', '-i', img, '::' + n], capture_output=True).stdout
dib = bytes((i * 7 + 3) & 255 for i in range(5000))
leeme = cat('LEEME.TXT')
ok = cat('GRANDE.DAT') == dib * 4 and cat('PRUEBA.DAT') == dib[:500] and cat('SUB/TRES.TXT') == b'hola'
ok = ok and len(leeme) > 100 and cat('CARTAS/PEPE.DOC') == leeme
dentro = subprocess.run(['mdir', '-b', '-i', img, '::CARTAS'], capture_output=True).stdout.decode()
ok = ok and 'PEPE.DOC' in dentro and 'JUAN' not in dentro and 'ANA' not in dentro
sys.exit(0 if ok else 1)
PY
then ok "el disquete, visto desde fuera: cada archivo con su contenido exacto"; else ko "el disquete no ha quedado como se esperaba"; mdir -/ -i $T/a.img :: | head -30; fi
if command -v fsck.fat > /dev/null; then
  if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en el disquete"; else ko "fsck.fat encuentra errores"; cat $T/fsck.txt; fi
fi

echo "== Hispa-8086: las demás medidas de disquete (720 K, 1,2 M y 1,44 M) =="
for m in 720 1200 1440; do
  cp build/hispa8086-$m.img $T/b.img
  arranca $T/b.img pentium
  escribe "hola"; escribe "arch"; sleep 7; escribe "dir"
  pantalla p5
  esta p5 "Hola desde un programa .COM. DOS 3.30" "$m K: "
  esta p5 "ARCHIVOS: BIEN" "$m K: "
  esta p5 "GRANDE.DAT         20000" "$m K: "
  apaga
  if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "$m K: fsck.fat no encuentra ningún error"; else ko "$m K: fsck.fat encuentra errores"; cat $T/fsck.txt; fi
done
# el sistema base tiene que caber en el disquete de los XT
libre=$(mdir -i build/hispa8086-360.img :: | awk '/bytes free/ { gsub(/ /, "", $0); sub(/bytesfree/, "", $0); print }')
if [ "${libre:-0}" -gt 100000 ]; then ok "en el disquete de 360 K quedan $libre octetos libres"; else ko "el disquete de 360 K se queda sin sitio ($libre)"; fi

echo "== Hispa-8086: solo instrucciones del 8086 =="
for f in arranque nucleo command; do
  if grep -q "^\s*cpu 8086" $f.asm; then ok "$f.asm se ensambla con «cpu 8086»"; else ko "$f.asm no lleva «cpu 8086»"; fi
done
# lo único de 386 es la prueba que distingue el 386 del 486, que solo corre
# cuando ya se sabe que el procesador es de 32 bits
n=$(grep -c "^\s*cpu 386" nucleo.asm)
if [ "$n" = 1 ]; then ok "un solo tramo de 386 (detrás de la prueba del 286)"; else ko "tramos de 386: $n"; fi
tam=$(stat -c %s build/NUCLEO86.SYS)
if [ $tam -lt 16384 ]; then ok "el núcleo ocupa $tam octetos"; else ko "el núcleo ha crecido a $tam octetos"; fi
tam=$(stat -c %s build/COMMAND.COM)
if [ $tam -lt 16384 ]; then ok "el intérprete ocupa $tam octetos"; else ko "el intérprete ha crecido a $tam octetos"; fi

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then for p in p1 p2 p3 p4 pl pr pe p6 p7 p8 p9 p5; do echo "--- $p"; cat $T/$p.txt 2>/dev/null; done; fi
rm -rf $T
exit $fail
