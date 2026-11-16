#!/bin/bash
# Prueba del «punto de inteligencia» del recorte a 8 letras en COMMAND.COM.
#
# Los nombres de DOS tienen 8 letras: al abrir el ejecutable el núcleo recorta
# a 8, así que ACTUALIZAR (10) arranca ACTUALIZ.COM, pero también lo haría un
# error como ACTUALIZQ. COMMAND.COM avisa cuando una orden de MÁS de 8 letras
# solo cuadra por el recorte y NO es un nombre largo legítimo conocido; las
# legítimas (ACTUALIZAR, VELOCIDAD...) siguen funcionando SIN aviso.
#
# Comprueba, arrancando Hispa-DOS en QEMU:
#   (a) VELOCIDAD (legítima, en el disco VELOCIDA.COM): corre SIN aviso.
#   (b) VELOCIDAX (error de 9 letras): AVISA, sugiere «VELOCIDAD» y AUN ASÍ
#       la ejecuta (preservar el comportamiento es lo seguro).
#   (c) VER y DIR (<=8 letras): van exactamente igual, SIN aviso.
#   (d) ACTUALIZQ (error de la orden de red): AVISA y sugiere «ACTUALIZAR»
#       (el aviso lo imprime COMMAND.COM antes de ejecutar, haya red o no).
# 8.3 truncation warning test for COMMAND.COM.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img

CMDS=(
  "VER"
  "ECHO [m1]"
  "VELOCIDAD"
  "ECHO [m2]"
  "VELOCIDAX"
  "ECHO [m3]"
  "DIR"
  "ECHO [m4]"
  "ACTUALIZQ"
  "ECHO [m5]"
)
{ sleep 6; for c in "${CMDS[@]}"; do printf '%s\r' "$c"; case "$c" in VELOCIDA*|ACTUALIZ*) sleep 3;; *) sleep 1.1;; esac; done; sleep 2; } | \
  timeout 150 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt

fail=0
ok()  { echo "  OK   $1"; }
bad() { echo "  FAIL $1"; fail=1; }
# trozo del registro entre dos marcas «ECHO [mN]» (igual que run_interprete)
sec() { awk -v a="ECHO [$1]" -v b="ECHO [$2]" 'index($0,b){f=0} f{print} index($0,a){f=1}' $T/log.txt; }
insec()  { if sec "$1" "$2" | grep -qF -- "$3"; then ok "$4"; else bad "$4"; fi; }
ninsec() { if sec "$1" "$2" | grep -qF -- "$3"; then bad "$4"; else ok "$4"; fi; }
# desde el arranque hasta la primera marca (para VER, antes de [m1])
top() { awk 'index($0,"ECHO [m1]"){exit} {print}' $T/log.txt; }

echo "== Hispa-DOS: aviso del recorte a 8 letras =="

AVISO="Aviso: las órdenes solo usan 8 letras"

# (c) VER (<=8, interna): sin aviso
if top | grep -qF -- "$AVISO"; then bad "VER no debía avisar"; else ok "VER (<=8): sin aviso"; fi

# (a) VELOCIDAD legítima: corre y NO avisa
insec  m1 m2 "Velocidad medida" "VELOCIDAD (legítima): se ejecuta"
ninsec m1 m2 "$AVISO"           "VELOCIDAD (legítima): sin aviso"

# (b) VELOCIDAX (typo de 9 letras): avisa, sugiere y aun así ejecuta
insec  m2 m3 "$AVISO"                     "VELOCIDAX: avisa del recorte"
insec  m2 m3 "se ha tomado como «VELOCIDA»" "VELOCIDAX: dice el nombre recortado"
insec  m2 m3 "¿Quería decir «VELOCIDAD»?"   "VELOCIDAX: sugiere VELOCIDAD"
insec  m2 m3 "Velocidad medida"            "VELOCIDAX: aun así ejecuta el programa"

# (c) DIR (<=8): sin aviso
ninsec m3 m4 "$AVISO" "DIR (<=8): sin aviso"

# (d) ACTUALIZQ (typo de la orden de red): avisa y sugiere ACTUALIZAR
insec  m4 m5 "$AVISO"                       "ACTUALIZQ: avisa del recorte"
insec  m4 m5 "¿Quería decir «ACTUALIZAR»?"   "ACTUALIZQ: sugiere ACTUALIZAR"

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat $T/log.txt; fi
rm -rf $T
exit $fail
