#!/bin/bash
# Prueba de TECLA.COM (diagnóstico del teclado): muestra el código en crudo
# (código de rastreo) de cada tecla, incluidas las modificadoras que INT 16h
# no ve. El monitor de QEMU pulsa teclas de verdad (sendkey -> INT 09h ->
# INT 15h 4Fh) y se lee la salida por el puerto serie en la página 437.
# TECLA diagnostic tool test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" "$T/log.txt"; then ok "$1"; else ko "$1"; fi; }
mon() { printf '%s\n' "$1" | socat - "UNIX-CONNECT:$T/mon" >/dev/null 2>&1; }
k() { local x; for x in "$@"; do mon "sendkey $x"; sleep "${TK:-0.15}"; done; }
# escribe una palabra letra a letra (minúsculas y espacio)
t() {
  local s=$1 i ch
  for ((i = 0; i < ${#s}; i++)); do
    ch=${s:i:1}
    [ "$ch" = ' ' ] && ch=spc
    k "$ch"
  done
}
arranca() {
  T=$(mktemp -d)
  cp "$IMG" "$T/f.img"
  qemu-system-i386 -display none -serial "file:$T/ser.txt" -monitor "unix:$T/mon,server,nowait" \
      -drive "file=$T/f.img,format=raw,if=floppy" -boot a -no-reboot &
  QP=$!
  sleep 6
}
acaba() {
  kill $QP 2>/dev/null; wait $QP 2>/dev/null
  tr -d '\r' < "$T/ser.txt" | iconv -f CP437 -t UTF-8 > "$T/log.txt"
}
fin() {
  if [ $fail -ne "$1" ] || [ -n "${VERBOSE:-}" ]; then sed -n '/TECLA/,$p' "$T/log.txt"; fi
  rm -rf "$T"
}

# ---- 1) teclas conocidas y salida con F10 ----
f0=$fail
arranca
t "tecla"; k ret; sleep 1.5
k a                                 # letra: 1E + ASCII 'a'
k left                              # flecha: E0 4B
TK=0.2 k ctrl                       # modificadora Ctrl: 1D con el bit 04
k right                            # flecha: E0 4D
k f10                              # salir sin usar Esc ni Ctrl
sleep 1
t "echo holatrastecla"; k ret    # el teclado sigue vivo tras desenganchar
sleep 1
acaba
echo "== Hispa-DOS TECLA: teclas conocidas y salida con F10 =="
check "Codigo:    1E  (make)"       # la tecla 'a'
check "ASCII: 61 'a'"
check "Codigo:    9E  (break)"      # al soltar la 'a'
check "Codigo: E0 4B  (make)"       # flecha izquierda: prefijo E0 + 4B
check "Codigo: E0 4D  (make)"       # flecha derecha: E0 + 4D
check "Codigo:    1D  (make)"       # Ctrl izquierdo
check "Estado: 04 [Ctrl]"           # y la BIOS marca el bit de Ctrl
check "Codigo:    44  (make)"       # F10
check "Fin del diagnóstico. Teclado restaurado."
check "holatrastecla"             # el intérprete responde: salida limpia
fin $f0

# ---- 2) salida escribiendo la palabra FIN (sin Esc ni Ctrl) ----
f0=$fail
arranca
t "tecla"; k ret; sleep 1.5
k a                                 # una tecla cualquiera
t "fin"                             # la palabra FIN termina el programa
sleep 1
t "echo adiostecla"; k ret
sleep 1
acaba
echo "== Hispa-DOS TECLA: salida escribiendo FIN =="
check "Codigo:    1E  (make)"       # la 'a'
check "Codigo:    21  (make)"       # F
check "Codigo:    17  (make)"       # I
check "Codigo:    31  (make)"       # N
check "Fin del diagnóstico. Teclado restaurado."
check "adiostecla"                 # el intérprete responde tras FIN
fin $f0

exit $fail
