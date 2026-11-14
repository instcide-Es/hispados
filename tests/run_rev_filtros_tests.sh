#!/bin/bash
# Pruebas de la revisión de los filtros y utilidades: BUSCAR (FIND), ORDENAR
# (SORT), ARBOL (TREE), ELEGIR (CHOICE), SONAR (PLAYWAV) y TECLADO (KEYB).
# Cada comprobación es un fallo real que se encontró y se arregló:
#   BUSCAR   FIND /V /C "" cuenta las líneas; opciones detrás del texto;
#            Ctrl+Z en medio de una línea termina el archivo
#   ORDENAR  Ctrl+Z final sin línea vacía de más; más de 8000 líneas; avisos
#            por la salida de errores (no dentro del archivo ordenado) y
#            ERRORLEVEL 1 si no cabe todo; 9000 líneas a un archivo en menos
#            de 6 s (antes, casi un minuto: dos escrituras por línea)
#   ARBOL    carpeta que no existe
#   ELEGIR   Ctrl+C = ERRORLEVEL 0
#   SONAR    /TONE 65536 (división por cero), WAV sin «fmt », de 24 bits,
#            con 0 canales, a 0 Hz y con tamaños de bloque negativos
#   TECLADO  tecla muerta y después Mayús+vocal (É); KEYB SP dos veces
# Review tests for the filters and small utilities.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
# lo que escribió una orden: desde su línea hasta el siguiente símbolo del sistema
sec() { awk -v c=">$1" 'f && /^A:\\>/ {exit} index($0, c) {f=1} f' $T/log.txt; }
insec() { if sec "$1" | grep -qF -- "$2"; then ok "$3"; else ko "$3"; fi; }
notinsec() { if sec "$1" | grep -qF -- "$2"; then ko "$3"; else ok "$3"; fi; }
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }

# ----------------------------------------------------------------------------
#  1. Filtros (consola por el puerto serie)
# ----------------------------------------------------------------------------
cp "$IMG" $T/f.img
# sitio para los archivos grandes de ORDENAR (el disquete va casi lleno): fuera
# los módulos de Ventanas, que esta prueba no usa
for d in ::HISPADOS/JUEGOS ::HISPADOS/PROGS; do
  MTOOLS_SKIP_CHECK=1 mdeltree -i $T/f.img $d > /dev/null 2>&1
done
python3 - $T <<'EOF'
import sys, os
d = sys.argv[1]
w = lambda n, b: open(os.path.join(d, n), 'wb').write(b)
w('CZ.TXT', b'pera\r\nmanzana\r\n\x1a')                    # como COPY CON
w('SINFIN.TXT', b'beta\r\nalfa')                           # sin salto final
w('COL.TXT', b'a3\r\nb1\r\nc2\r\n')
w('MUCHAS.TXT', b''.join(b'%05d\r\n' % (9999 - i) for i in range(9000)))
w('LF30K.TXT', b''.join(b'%d\n' % (9 - i % 10) for i in range(30000)))
w('GRANDE.TXT', b''.join(b'%05d %s\r\n' % (1099 - i, b'y' * 60) for i in range(1100)))
w('CZMED.TXT', b'aguja uno\r\nxx\x1a' + b'relleno\r\n' * 1000 + b'aguja dos\r\n')
w('RESP.TXT', b'b\r\n')
EOF
for f in CZ SINFIN COL MUCHAS LF30K GRANDE CZMED RESP; do mcopy -i $T/f.img $T/$f.TXT ::$f.TXT; done
{
  sleep 5
  send "FIND /C /V \"\" SINFIN.TXT"
  send "FIND \"\" SINFIN.TXT"
  send "IF ERRORLEVEL 1 ECHO vacio-no-esta"
  send "FIND \"alfa\" /C SINFIN.TXT"
  send "TYPE SINFIN.TXT | FIND \"ALFA\" /I /N" 2
  send "FIND /C \"aguja\" CZMED.TXT" 3
  send "BUSCAR \"alfa\" < SINFIN.TXT" 2
  send "IF NOT ERRORLEVEL 1 ECHO el-cero"
  send "BUSCAR \"nada\" SINFIN.TXT"
  send "IF ERRORLEVEL 1 IF NOT ERRORLEVEL 2 ECHO el-uno"
  send "BUSCAR \"alfa\" NOEX.TXT"
  send "IF ERRORLEVEL 2 ECHO el-dos"
  send "ORDENAR CZ.TXT"
  send "SORT /+2 COL.TXT"
  send "TYPE SINFIN.TXT | SORT /R" 2
  send "ORDENAR < MUCHAS.TXT > M.OUT" 3
  send "IF NOT ERRORLEVEL 1 ECHO orden-completo" 6
  send "ORDENAR < LF30K.TXT > N.OUT" 2
  send "IF ERRORLEVEL 1 ECHO orden-incompleto" 6
  send "ORDENAR < GRANDE.TXT > G.OUT" 2
  send "IF ERRORLEVEL 1 ECHO orden-cortado" 6
  send "ARBOL NOEXISTE" 2
  send "ARBOL README.TXT"
  send "ARBOL BAT"
  send "ARBOL HISPADOS\\"
  send "ELEGIR /C:AB < RESP.TXT"
  send "IF ERRORLEVEL 2 ECHO eligio-b"
  send "ELEGIR /C:AB Ctrl"
  sleep 1.5; printf '\x03'
  send "IF NOT ERRORLEVEL 1 ECHO ctrlc-cero"
  send "ECHO fin"
  sleep 2
} | timeout 85 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
echo "== Hispa-DOS revisión de filtros: BUSCAR, ORDENAR, ARBOL, ELEGIR =="
insec 'FIND /C /V "" SINFIN.TXT' "---------- SINFIN.TXT: 2" 'FIND /C /V "" cuenta las líneas (2)'
check "vacio-no-esta"
insec 'FIND "alfa" /C SINFIN.TXT' "---------- SINFIN.TXT: 1" 'FIND "alfa" /C: la opción detrás del texto'
notinsec 'FIND "alfa" /C SINFIN.TXT' "No se encontr" 'FIND "alfa" /C: /C no es un archivo'
insec 'TYPE SINFIN.TXT | FIND "ALFA" /I /N' "[2]alfa" 'tubería con /I /N detrás del texto'
insec 'FIND /C "aguja" CZMED.TXT' "---------- CZMED.TXT: 1" "BUSCAR termina en el Ctrl+Z en medio de una línea"
check "el-cero"
check "el-uno"
check "el-dos"
if sec "ORDENAR CZ.TXT" | sed -n 2p | grep -qx "manzana"; then ok "ORDENAR: el Ctrl+Z final no es una línea vacía"; else ko "ORDENAR: línea vacía de más por el Ctrl+Z final"; fi
if sec "SORT /+2 COL.TXT" | tr '\n' ' ' | grep -q "b1 *c2 *a3"; then ok "SORT /+2"; else ko "SORT /+2"; fi
if sec "TYPE SINFIN.TXT | SORT /R" | tr '\n' ' ' | grep -q "beta *alfa"; then ok "tubería con SORT /R"; else ko "tubería con SORT /R"; fi
for f in M N G; do mcopy -n -i $T/f.img ::$f.OUT $T/$f.OUT 2>/dev/null; done
python3 - $T <<'EOF' || fail=1
import sys, os
d = sys.argv[1]
ok = True
def rd(n):
    try:
        return open(os.path.join(d, n), 'rb').read()
    except OSError:
        return b''
def res(c, t):
    global ok
    print(("  OK   " if c else "  FAIL ") + t)
    ok = ok and c
m = rd('M.OUT').split(b'\r\n')
res(m[-1] == b'' and m[:-1] == [b'%05d' % i for i in range(1000, 10000)],
    "ORDENAR: 9000 líneas, todas y en orden (salieron %d)" % (len(m) - 1))
n = rd('N.OUT').split(b'\r\n')
res(len(n) - 1 == 24000 and n[:-1] == sorted(n[:-1]) and b'El texto' not in rd('N.OUT'),
    "ORDENAR: de 30000 líneas ordena 24000 y el aviso no va al archivo (salieron %d)" % (len(n) - 1))
g = rd('G.OUT')
gl = g.split(b'\r\n')
res(b'El texto' not in g, "ORDENAR: el aviso de 64 KB no va dentro del archivo ordenado")
res(len(gl) > 900 and gl[:-1] == sorted(gl[:-1]) and all(set(x) <= set(b'0123456789 y') for x in gl),
    "ORDENAR: más de 64 KB, el principio ordenado y sin basura (%d líneas)" % (len(gl) - 1))
sys.exit(0 if ok else 1)
EOF
check "orden-completo"
insec "ORDENAR < LF30K.TXT > N.OUT" "24000 líneas: se ordena solo el principio" "ORDENAR avisa en pantalla de que hay demasiadas líneas"
check "orden-incompleto"
insec "ORDENAR < GRANDE.TXT > G.OUT" "64 KB: se ordena solo el principio" "ORDENAR avisa en pantalla de que pasa de 64 KB"
check "orden-cortado"
insec "ARBOL NOEXISTE" "Ruta no válida." "ARBOL de una carpeta que no existe"
insec "ARBOL README.TXT" "Ruta no válida." "ARBOL de un archivo"
insec "ARBOL BAT" "A:\\BAT" "ARBOL BAT"
insec "ARBOL HISPADOS\\" "Listado de carpetas del volumen" "ARBOL HISPADOS\\"
check "eligio-b"
check "ctrlc-cero"
check "fin"
if fsck.fat -n $T/f.img >/dev/null 2>&1; then ok "fsck.fat"; else ko "fsck.fat"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/>FIND \/C \/V/,$p' $T/log.txt | cut -c1-160; fi
fail1=$fail

# ----------------------------------------------------------------------------
#  2. SONAR con archivos WAV corruptos (tarjeta AC'97 y HSPSND)
# ----------------------------------------------------------------------------
cp "$IMG" $T/s.img
python3 - $T <<'EOF'
import sys, os, struct
d = sys.argv[1]
w = lambda n, b: open(os.path.join(d, n), 'wb').write(b)
fmt = lambda ch, hz, bits: struct.pack('<HHIIHH', 1, ch, hz, hz * ch * bits // 8, ch * bits // 8, bits)
data = bytes([128 + (60 if (i // 11) % 2 else -60) for i in range(4000)])
ck = lambda t, b, sz=None: t + struct.pack('<I', len(b) if sz is None else sz) + b
def riff(*chunks):
    body = b'WAVE' + b''.join(chunks)
    return b'RIFF' + struct.pack('<I', len(body)) + body
w('OK.WAV', riff(ck(b'fmt ', fmt(1, 22050, 8)), ck(b'data', data)))
w('NOFMT.WAV', riff(ck(b'data', data)))
w('B24.WAV', riff(ck(b'fmt ', fmt(1, 22050, 24)), ck(b'data', data)))
w('CH0.WAV', riff(ck(b'fmt ', fmt(0, 22050, 8)), ck(b'data', data)))
w('RATE0.WAV', riff(ck(b'fmt ', fmt(1, 0, 8)), ck(b'data', data)))
w('TRUNC.WAV', riff(ck(b'fmt ', fmt(2, 22050, 16)), ck(b'data', data[:1001], 0x7FFFFFFF)))
w('NEG.WAV', riff(ck(b'LIST', b'abcd', 0xFFFFFFF8), ck(b'fmt ', fmt(1, 22050, 8)), ck(b'data', data)))
w('NEGF.WAV', riff(ck(b'fmt ', fmt(1, 22050, 8), 0xFFFFFFF8), ck(b'data', data)))
w('CORTO.WAV', b'RIFF\x00\x00')
EOF
for f in OK NOFMT B24 CH0 RATE0 TRUNC NEG NEGF CORTO; do mcopy -i $T/s.img $T/$f.WAV ::$f.WAV; done
{
  sleep 5
  send "SONAR OK.WAV"
  send "IF ERRORLEVEL 2 ECHO sin-controlador"
  send "A:\\HISPADOS\\HSPSND" 2
  send "SONAR OK.WAV" 3
  send "SONAR NOFMT.WAV" 3
  send "SONAR B24.WAV" 3
  send "SONAR CH0.WAV" 3
  send "SONAR RATE0.WAV" 3
  send "IF ERRORLEVEL 6 ECHO no-admitido"
  send "SONAR TRUNC.WAV" 3
  send "SONAR CORTO.WAV" 3
  send "SONAR /TONE 65536 100" 2
  send "ECHO vivo-tono-65536" 3
  send "SONAR /TONE 32768 100" 2
  send "ECHO vivo-tono-32768" 3
  send "SONAR NEG.WAV" 2
  send "ECHO vivo-neg" 4
  send "SONAR NEGF.WAV" 2
  send "ECHO vivo-negf" 4
  sleep 2
} | timeout 65 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -audiodev wav,id=snd0,path=$T/out.wav -device AC97,audiodev=snd0 \
  -drive file=$T/s.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
fail=0
echo "== Hispa-DOS revisión de SONAR: archivos WAV corruptos =="
check "sin-controlador"
check "Reproduciendo OK.WAV (22050 Hz, 8 bits, mono) en Intel AC'97"
insec "SONAR NOFMT.WAV" "Solo se admiten archivos WAV PCM." "WAV sin bloque «fmt » (no se usa el formato del anterior)"
insec "SONAR B24.WAV" "Solo se admiten archivos WAV PCM." "WAV de 24 bits"
insec "SONAR CH0.WAV" "Solo se admiten archivos WAV PCM." "WAV con 0 canales"
insec "SONAR RATE0.WAV" "Solo se admiten archivos WAV PCM." "WAV a 0 Hz"
check "no-admitido"
insec "SONAR TRUNC.WAV" "Hecho." "WAV truncado (el tamaño de los datos miente)"
insec "SONAR CORTO.WAV" "No es un archivo WAV." "WAV de 6 bytes"
check "vivo-tono-65536"
check "vivo-tono-32768"
insec "SONAR NEG.WAV" "No es un archivo WAV." "bloque con tamaño negativo"
check "vivo-neg"
insec "SONAR NEGF.WAV" "No es un archivo WAV." "bloque «fmt » con tamaño negativo"
check "vivo-negf"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/>SONAR OK/,$p' $T/log.txt | cut -c1-160; fi
fail2=$fail

# ----------------------------------------------------------------------------
#  3. TECLADO con pulsaciones reales (monitor de QEMU, como run_keyb_tests.sh)
# ----------------------------------------------------------------------------
# Las pulsaciones de sendkey no son fiables con el servidor cargado (a veces
# se pierde o se repite alguna): se intenta hasta tres veces.
keyb_run() {
  cp "$IMG" $T/k.img
  qemu-system-i386 -display none -serial file:$T/ser.txt -monitor unix:$T/mon,server,nowait \
      -drive file=$T/k.img,format=raw,if=floppy -boot a -no-reboot &
  QP=$!
  sleep 4
  k() { for x in "$@"; do printf 'sendkey %s\n' "$x" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; sleep 0.08; done; }
  k shift-k shift-e shift-y shift-b spc shift-s shift-p ret
  sleep 1.5
  # cargarlo otra vez: no se instala dos veces
  k shift-k shift-e shift-y shift-b spc shift-s shift-p ret
  sleep 1.5
  # REM [É ü á Ü ñ]: ´ y después Mayús+E; ¨ u; ´ a; ¨ y Mayús+U
  k shift-r shift-e shift-m spc alt_r-bracket_left
  k apostrophe shift-e
  k shift-apostrophe u
  k apostrophe a
  k shift-apostrophe shift-u
  k semicolon
  k alt_r-bracket_right ret
  sleep 1.5
  # US y de vuelta a SP: sigue el mismo residente
  k shift-k shift-e shift-y shift-b spc shift-u shift-s ret
  sleep 1.5
  k shift-r shift-e shift-m spc bracket_left semicolon bracket_right ret
  sleep 1
  k shift-k shift-e shift-y shift-b spc shift-e shift-s ret
  sleep 1.5
  k shift-r shift-e shift-m spc shift-2 semicolon shift-2 ret
  sleep 1
  k shift-m shift-e shift-m spc shift-7 shift-c ret
  sleep 2
  kill $QP 2>/dev/null; wait $QP 2>/dev/null
  python3 - $T/ser.txt > $T/keyb.txt <<'EOF'
import sys
t = open(sys.argv[1], 'rb').read().decode('cp437').replace('\r', '')
ok = True
def res(c, s):
    global ok
    print(("  OK   " if c else "  FAIL ") + s)
    ok = ok and c
res(t.count("Teclado español instalado.") == 1, "KEYB SP dos veces: se instala una sola vez")
res("Teclado activo: español (SP)." in t, "la segunda vez dice el teclado activo")
res("REM [ÉüáÜñ]" in t, "´ y Mayús+E = É (y ü á Ü ñ)")
res("Teclado activo: estadounidense (US)." in t, "KEYB US")
res("REM [;]" in t, "con US las teclas son las de la BIOS")
res('REM "ñ"' in t, "de vuelta a SP")
# un solo residente: una sola línea de KEYB en MEM /C
n = sum(1 for l in t.split('\n') if l.strip().startswith(('TECLADO ', 'KEYB ')))
res(n == 1, "un solo TECLADO residente en MEM /C (%d)" % n)
if not ok:
    print(t[t.find('KEYB SP'):])
sys.exit(0 if ok else 1)
EOF
}
fail=1
for intento in 1 2 3; do
  if keyb_run; then fail=0; break; fi
done
echo "== Hispa-DOS revisión de TECLADO =="
cat $T/keyb.txt
fail3=$fail
rm -rf $T
[ $fail1 -eq 0 ] && [ $fail2 -eq 0 ] && [ $fail3 -eq 0 ]
exit $?
