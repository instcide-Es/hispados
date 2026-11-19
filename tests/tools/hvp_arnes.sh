# Arnés común de las pruebas de Productividad (Facturas, Pluma, Cuentas).
# Se incluye con «. tests/tools/hvp_arnes.sh» tras definir IMG. Arranca Hispa
# Ventanas en QEMU con un módulo .HVP abierto directamente (clave «Reabrir=»
# de CENTRO.INI), sin depender de en qué grupo ni en qué orden esté su icono.
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" "${TEC_ESPERA:-0.12}"; done; }
shot() { sleep 0.5; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
# el botón secundario (el derecho): su menú sale al soltarlo
derecho() { ir $1 $2; mon "mouse_button 2" 0.25; mon "mouse_button 0" 0.6; }
aparca() { ir 16 590; }
# teclea un texto con el teclado español (KEYB SP) ya cargado
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in
      ' ') tecla spc;;
      '.') tecla dot;;
      ',') tecla comma;;
      '-') tecla slash;;
      '+') tecla bracket_right;;
      '*') tecla shift-bracket_right;;
      '/') tecla shift-7;;
      '=') tecla shift-0;;
      '(') tecla shift-8;;
      ')') tecla shift-9;;
      ':') tecla shift-dot;;
      ';') tecla shift-comma;;
      '%') tecla shift-5;;
      [A-Z]) tecla "shift-$(echo $c | tr A-Z a-z)";;
      *) tecla "$c";;
    esac
  done
}
# prepara $T/f.img con el módulo $1 (archivo .HVP) y lo deja para reabrir
prepara() {
  local hvp=$1 nom
  nom=$(basename $hvp)
  cp "$IMG" $T/f.img
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mdeltree -i $T/f.img ::BIN 2>/dev/null          # sitio para los datos
  mmd -i $T/f.img ::HISPADOS/PROGS 2>/dev/null
  mcopy -o -i $T/f.img $hvp ::HISPADOS/PROGS/$nom
  # los tipos de letra (medio mega): se hace sitio quitando lo que la prueba no usa
  mdel -i $T/f.img ::HISPADOS/HISPA.WAV 2>/dev/null; mdeltree -i $T/f.img ::HISPADOS/JUEGOS 2>/dev/null
  # (0.9.25: el disquete va más lleno; estos tampoco los usan los módulos)
  mdel -i $T/f.img ::HISPADOS/AUDICION.COM ::HISPADOS/EQUIPO.COM 2>/dev/null
  # [GLIFOS] (los tipos crecieron con Á, Ó, €...: 90 K más; tampoco usan el
  # Mirador ni HSPUSB, salvo la prueba del propio Mirador)
  mdel -i $T/f.img ::HISPADOS/HSPUSB.SYS 2>/dev/null
  [ "$nom" = MIRADOR.HVP ] || mdel -i $T/f.img ::HISPADOS/PROGS/MIRADOR.HVP 2>/dev/null
  # [GLIFOS] solo si el módulo los usa (incluye tipos.inc): Cuentas, Datos...
  # no, y así les queda sitio para sus archivos
  local src=programas/$(echo ${nom%.*} | tr A-Z a-z).asm
  if [ ! -f $src ] || grep -q 'tipos\.inc' $src; then
    mcopy -o -i $T/f.img programas/comun/TIPOS*.DAT ::HISPADOS/PROGS/ || echo "  FAIL no caben los tipos de letra en el disquete de la prueba"
  fi
  printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\%s\r\n' "$nom" > $T/CENTRO.INI
  mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
}
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel ${PARALELO:-none} -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
}
arranca() { sleep 6; send 'KEYB SP' 2; send 'A:\HISPADOS\HSPRATON' 2; send 'VENTANAS' 6; aparca; }
sal() { mon quit; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/$2.png; }
