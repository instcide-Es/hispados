# Escribir con el teclado de QEMU (sendkey) en las pruebas del Navegador
# completo (tests/run_navegador2_tests.sh): minúsculas, cifras y / : . - ? = _
# espacio. Se incluye con «.» después de tests/tools/win32_ventanas.sh (mon).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
escribe() {
  local s="$1" i c k
  for ((i = 0; i < ${#s}; i++)); do
    c=${s:i:1}
    case "$c" in
      [a-z0-9]) k=$c;; /) k=slash;; :) k=shift-semicolon;; .) k=dot;; -) k=minus;; '?') k=shift-slash;;
      =) k=equal;; _) k=shift-minus;; ' ') k=spc;; *) continue;;
    esac
    mon "sendkey $k" 0.12
  done
}
