#!/bin/bash
# [MEM-JUEGOS] La memoria que queda para los juegos con la opción «Juegos»
# del menú de arranque del disquete de uso (floppy/CONFIG.SYS y
# AUTOEXEC.BAT, tal cual: HSPEMM, LH HSPSND /SB, LH HSPDPMI /SB, HSPJOY,
# HSPRATON y TECLADO), en un equipo con Intel HD Audio (QEMU con intel-hda,
# sin Sound Blaster: la emula HSPSND), y que un juego lanzado desde Hispa
# Ventanas tiene la MISMA memoria convencional, EMS y XMS que desde la línea
# de órdenes. El «juego» es tests/progs/memjuego.asm (MEMJUEGO.COM): apunta
# en \MEMJ.TXT y en la pantalla el bloque que le ha dado DOS, la memoria
# convencional libre, la EMS, la XMS y las páginas VCPI.
#   qemu    el equipo de QEMU: memoria superior en B000-B7FF (vídeo en
#           color), C000-DFFF y lo que SeaBIOS deja libre en E000-EFFF.
#           MEM (convencional, superior), MEM /C (HSPDPMI arriba, sus tablas
#           también), la Sound Blaster emulada en modo real (SBTEST) y por
#           DPMI (SBPM: HSPDPMI con sus tablas en la memoria superior), DPMI16,
#           y Hispa Ventanas: un acceso que vuelve y otro que no
#   hpmini  como el HP Mini 5103: la ROM de vídeo en C000-CFFF, la BIOS en
#           E000-FFFF (X=C000-CFFF X=E000-EFFF) y sin lector de CD (sin
#           HSPCD): solo queda B000-B7FF, y HSPDPMI cabe en ella con sus
#           tablas de páginas
#   sinumb  sin ninguna memoria superior: el lanzador de Ventanas se queda
#           abajo (como antes) y todo sigue funcionando
# Sin segundo argumento se hacen todas. MIN_QEMU y MIN_HP: los mínimos de
# memoria convencional libre (en octetos) según MEM.
# Memory left for games in the "Games" boot option; same memory from Ventanas.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
MODE=${2:-todo}
B=$(dirname "$IMG")
DISQ=$B/hispados-disquete.img
MIN_QEMU=${MIN_QEMU:-560000}
MIN_HP=${MIN_HP:-550000}   # (14/11: 558000 antes de Ctrl+Alt+Fin en HSPDPMI; pendiente adelgazarlo)
for f in $DISQ $B/MEMJUEGO.COM $B/SBTEST.COM $B/SBPM.COM $B/DPMI16.COM; do
  [ -f $f ] || { echo "== Hispa-DOS memoria para juegos =="; echo "  FAIL falta $f (make usb emmprogs nucprogs)"; exit 1; }
done
fail=0
T=""
QP=""
P='A:\>'
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; cfail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
checkre() { if grep -q -- "$1" $T/log.txt; then ok "$2"; else ko "$2"; fi; }
veces() { grep -aoF -- "$1" $T/log 2>/dev/null | wc -l; }
espera() {  # texto [veces] [décimas]
  local n=${2:-1} m=${3:-900} i=0
  while [ "$(veces "$1")" -lt "$n" ] && [ $i -lt $m ]; do sleep 0.1; i=$((i + 1)); done
  [ $i -lt $m ] || echo "  (no llega: $1)"
}
orden() {  # orden [décimas]
  local n=$(veces "$P")
  printf '%s\r' "$1" >&3
  espera "$P" $((n + 1)) ${2:-900}
}
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.4}"; }
quiere() { [ "$MODE" = todo ] || [ "$MODE" = "$1" ]; }
# tono HZ SEGUNDOS: hay un tramo de esa frecuencia (5 %) y esa duración (0,2 s)
tono() {
  python3 - $T/audio.txt "$1" "$2" <<'EOF' || cfail=1
import re, sys
txt = open(sys.argv[1]).read()
want, dur = float(sys.argv[2]), float(sys.argv[3])
tr = [(float(a), float(b), float(h)) for a, b, h in re.findall(r'tramo ([\d.]+)-([\d.]+) s: (\d+) Hz', txt)]
m = [t for t in tr if abs(t[2] - want) / want < 0.05 and abs((t[1] - t[0]) - dur) < 0.2]
if m:
    print("  OK   suena %d Hz durante %.2f s" % (want, m[0][1] - m[0][0]))
else:
    print("  FAIL no suena %d Hz unos %.1f s (tramos: %s)" % (want, dur, tr)); sys.exit(1)
EOF
}

# caso nombre "opciones de HSPEMM"
caso() {
  T=$(mktemp -d)
  cfail=0
  echo "== Hispa-DOS memoria para juegos ($1) =="
  cp $DISQ $T/f.img
  if [ -n "$2" ]; then
    mtype -i $T/f.img ::CONFIG.SYS | sed "s/^DEVICE=A:\\\\HISPADOS\\\\HSPEMM.SYS/& $2/" | grep -v "${3:-NADA_QUE_QUITAR}" > $T/CONFIG.SYS
    grep -q "^DEVICE=A:.HISPADOS.HSPEMM.SYS $2" $T/CONFIG.SYS || ko "no está la línea de HSPEMM en CONFIG.SYS"
    mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  fi
  mcopy -o -i $T/f.img $B/MEMJUEGO.COM $B/SBTEST.COM $B/SBPM.COM $B/DPMI16.COM ::BIN/
  # (sin los juegos propios: no hacen falta y el grupo sale vacío)
  mdel -i $T/f.img '::HISPADOS/JUEGOS/*.HVP' 2>/dev/null
  # un grupo con dos accesos al «juego»: el primero vuelve a Hispa Ventanas,
  # el segundo no (acaba en Hispa-DOS)
  printf '%s\r\n' '[centro]' 'ventana = 300, 60, 380, 170' 'Activo=0' '' \
    '[Grupo]' 'Nombre=Juegos de prueba' 'Ventana=100,300,460,200' 'Estado=1' '' \
    '[Acceso]' 'Nombre=Medir' 'Icono=juego' 'Programa=A:\BIN\MEMJUEGO.COM' 'Parametros=VEN' 'Carpeta=A:\' '' \
    '[Acceso]' 'Nombre=Medir y salir' 'Icono=juego' 'Programa=A:\BIN\MEMJUEGO.COM' 'Parametros=VEN2' 'Carpeta=A:\' 'Volver=0' > $T/CENTRO.INI
  mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
  rm -f $T/in $T/log
  mkfifo $T/in
  QEMU_AUDIO_DRV=none qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -vga std -m 32 -audiodev wav,id=snd0,path=$T/out.wav -device intel-hda -device hda-duplex,audiodev=snd0 \
      -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot < $T/in > $T/log 2>&1 &
  QP=$!
  exec 3> $T/in
  espera 'Elija una opci'
  printf '2\r' >&3
  espera "$P"
}
# Hispa Ventanas: el primer acceso (vuelve) y el segundo (no vuelve)
ventanas() {
  local n=$(veces "$P")
  printf 'VENTANAS\r' >&3
  sleep 12
  mon "sendkey ret"
  espera "MEMJ VEN " 1 900
  sleep 12                                # (Hispa Ventanas se carga otra vez)
  mon "sendkey right"
  mon "sendkey ret"
  espera "MEMJ VEN2 " 1 900
  espera "$P" $((n + 1))
}
fin() {
  exec 3>&-
  sleep 0.5
  kill $QP 2>/dev/null
  wait $QP 2>/dev/null
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c | grep -av '^audio:\|^ALSA\|^alsa' > $T/log.txt
  mtype -i $T/f.img ::MEMJ.TXT 2>/dev/null | tr -d '\r' > $T/memj.txt
  [ -f $T/out.wav ] && python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
  "$@"
  if [ $cfail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; cat $T/memj.txt; fi
  [ $cfail -ne 0 ] && fail=1
  rm -rf $T
}
# bytes libres de memoria convencional según la MEM número N
libres() { awk '/memoria convencional en total/{getline; print}' $T/log.txt | sed -n "${1}p" | tr -dc '0-9'; }
umbtotal() { grep -a "bytes de memoria superior (UMB) en total" $T/log.txt | sed -n "${1}p" | tr -dc '0-9'; }
# campo de la línea de MEMJUEGO con esa etiqueta
mj() { grep -a "^MEMJ $1 " $T/memj.txt | head -1 | tr ' ' '\n' | grep "^$2=" | cut -d= -f2; }
igual() {  # etiqueta: lo mismo que desde la línea de órdenes (CMD)
  local c v k d=""
  [ -n "$(mj $1 PROG)" ] || { ko "MEMJUEGO $1 no ha escrito nada"; return; }
  for k in PROG LIBRE EMS XMS XMSMAX VCPI; do
    c=$(mj CMD $k); v=$(mj $1 $k)
    [ "$c" = "$v" ] || d="$d $k=$v (línea: $c)"
  done
  if [ -z "$d" ]; then ok "$2: la misma memoria que desde la línea de órdenes ($(mj $1 PROG) octetos, EMS $(mj $1 EMS) KB, XMS $(mj $1 XMS) KB, VCPI $(mj $1 VCPI) páginas)"
  else ko "$2: memoria distinta:$d"; fi
}
comunes() {  # mínimo de memoria convencional
  local n m
  check "HSPEMM 0.1: EMS 4.0 y VCPI"
  check "Sound Blaster 16 emulada en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)"
  check "CONFIG=JUEGOS"
  check "BLASTER=A220 I5 D1 H5 T6"
  check "Hispa-DOS está cargado en la memoria alta (HMA)"
  nocheck_err
  n=$(libres 1)
  if [ -n "$n" ] && [ "$n" -ge $1 ]; then ok "MEM: $n bytes de memoria convencional libres (mínimo $1)"; else ko "MEM: «$n» bytes de memoria convencional libres (se esperaban $1 o más)"; fi
  m=$(libres 2)
  if [ -n "$m" ] && [ "$m" = "$n" ]; then ok "MEM al final, tras Hispa Ventanas: igual ($m)"; else ko "MEM al final: «$m» (al principio: «$n»)"; fi
  echo "  (MEM: $n libres; memoria superior: $(umbtotal 1) en total; MEMJUEGO CMD: $(grep -a '^MEMJ CMD ' $T/memj.txt))"
  [ -n "$(mj CMD EMS)" ] && [ "$(mj CMD EMS)" -gt 0 ] && ok "hay EMS libre: $(mj CMD EMS) KB" || ko "EMS libre: «$(mj CMD EMS)»"
  [ -n "$(mj CMD XMS)" ] && [ "$(mj CMD XMS)" -gt 0 ] && ok "hay XMS libre: $(mj CMD XMS) KB" || ko "XMS libre: «$(mj CMD XMS)»"
  igual VEN "desde Hispa Ventanas (acceso que vuelve)"
  igual VEN2 "desde Hispa Ventanas otra vez (acceso que no vuelve)"
  igual CMD2 "en la línea de órdenes al salir de Hispa Ventanas"
  checkn "HSPDPMI: excepción"
  checkn "no se ha podido"
}
nocheck_err() {
  checkn "Error en CONFIG.SYS"
  checkn "Comando o nombre de archivo incorrecto"
  checkn "HSPEMM: "
}
# ¿el programa N está en la memoria superior según MEM /C?
arriba() {
  if awk '/Programas en la memoria superior/{u=1} /Programas en la memoria convencional/{u=0} u && $1 == "'$1'" {f=1} END{exit !f}' $T/log.txt; then
    ok "$1 en la memoria superior"; else ko "$1 en la memoria superior"; fi
}

# ---- qemu: el equipo de QEMU, todo ----
c_qemu() {
  comunes $MIN_QEMU
  check "memoria superior: "
  check " B000-B7FF"
  arriba HSPDPMI
  arriba HSPRATON
  arriba TECLADO
  # la Sound Blaster emulada en modo real
  check "SBTEST: tarjeta en 220h, IRQ 5, DMA 1 y 5"
  check "Reinicio del DSP: correcto"
  check "DSP versión 4.05"
  checkre "Autoinicio (1Ch): 9 IRQ en 1[3-6] tics, correcto" "SBTEST: autoinicio (1Ch): 9 IRQ a su hora"
  check "La cuenta del DMA avanza: correcto"
  check "AdLib (FM): detectado"
  # y por DPMI (como DOS/4GW), con HSPDPMI arriba
  check "SBPM: en modo protegido de 32 bits (DPMI)"
  check "SBPM: IOPL 0"
  check "SBPM terminado, fallos: 0"
  check "DPMI16 terminado, fallos: 00"
  check "Tono 440 Hz en Intel HD Audio"
  tono 1206 0.58    # SB16: 16 bits estéreo, 44100 Hz, 25 bloques
  tono 600 0.7      # FM: un operador a 600 Hz
  tono 440 0.5      # la API de HSPSND (SONAR)
}
if quiere qemu; then
  caso qemu ""
  orden 'MEM'
  orden 'MEM /C'
  orden 'SET'
  orden 'BIN\MEMJUEGO CMD'
  orden 'BIN\SBTEST' 1500
  orden 'BIN\SBPM' 1500
  orden 'BIN\DPMI16' 1500
  orden 'SONAR /TONE 440 500'
  ventanas
  orden 'BIN\MEMJUEGO CMD2'
  orden 'MEM'
  fin c_qemu
fi

# ---- hpmini: solo B000-B7FF de memoria superior ----
c_hp() {
  comunes $MIN_HP
  check "memoria superior: 32 KB ( B000-B7FF)"
  arriba HSPDPMI
  check "SBPM terminado, fallos: 0"
}
if quiere hpmini; then
  # (el HP Mini no tiene lector de CD: HSPCD no se queda; aquí se quita)
  caso hpmini "X=C000-CFFF X=E000-EFFF" 'HSPCD.SYS'
  orden 'MEM'
  orden 'MEM /C'
  orden 'SET'
  orden 'BIN\MEMJUEGO CMD'
  orden 'BIN\SBPM' 1500
  ventanas
  orden 'BIN\MEMJUEGO CMD2'
  orden 'MEM'
  fin c_hp
fi

# ---- sinumb: sin memoria superior, el lanzador se queda abajo ----
c_sin() {
  local c v
  check "HSPEMM 0.1: EMS 4.0 y VCPI"
  nocheck_err
  checkn "memoria superior: "
  n=$(libres 1); m=$(libres 2)
  if [ -n "$m" ] && [ "$m" = "$n" ]; then ok "MEM al final, tras Hispa Ventanas: igual ($m)"; else ko "MEM al final: «$m» (al principio: «$n»)"; fi
  c=$(mj CMD PROG); v=$(mj VEN PROG)
  if [ -n "$c" ] && [ -n "$v" ] && [ "$v" -le "$c" ] && [ $((c - v)) -lt 4096 ]; then
    ok "desde Hispa Ventanas: $v octetos (línea de órdenes: $c; el lanzador, abajo: $((c - v)))"
  else ko "desde Hispa Ventanas: «$v» octetos (línea de órdenes: «$c»)"; fi
  [ "$(mj VEN EMS)" = "$(mj CMD EMS)" ] && [ "$(mj VEN XMS)" = "$(mj CMD XMS)" ] && ok "EMS y XMS iguales desde Hispa Ventanas" || ko "EMS/XMS distintas desde Hispa Ventanas"
  [ -n "$(mj VEN2 PROG)" ] && ok "el segundo acceso (no vuelve) también" || ko "el segundo acceso no ha escrito nada"
  igual CMD2 "en la línea de órdenes al salir de Hispa Ventanas"
  checkn "HSPDPMI: excepción"
}
if quiere sinumb; then
  caso sinumb "X=B000-B7FF X=C000-CFFF X=E000-EFFF"
  orden 'MEM'
  orden 'BIN\MEMJUEGO CMD'
  ventanas
  orden 'BIN\MEMJUEGO CMD2'
  orden 'MEM'
  fin c_sin
fi
exit $fail
