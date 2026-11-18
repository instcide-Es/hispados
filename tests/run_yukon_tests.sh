#!/bin/bash
# Prueba de la tarjeta Marvell Yukon 2 «Optima» (88E8059, la del HP Mini 5103)
# de HSPRED. QEMU no la emula: HSPRED.COM corre en una máquina mínima hecha
# con KVM contra un modelo del chip (tests/yukon_sim.py: lo explica). Con los
# registros por memoria y por los puertos, con y sin cable, revisión 0 y 1 del
# chip, con IRQ y con /SONDEO: carga, MAC, anillos, enlace, envío, recepción,
# /INFO con el residente, /DEPURA y descarga. Sin /dev/kvm se salta.
#   -v: enseña lo que HSPRED escribe en la primera pasada.
# Marvell Yukon 2 test against a chip model (KVM real-mode harness).
set -u
cd "$(dirname "$0")/.."
[ -f build/HSPRED.COM ] || { echo "falta build/HSPRED.COM (make)"; exit 1; }
echo "== Hispa-DOS red: Marvell Yukon 88E8059 simulada (HSPRED contra un modelo del chip) =="
if [ ! -w /dev/kvm ]; then
  echo "  (sin /dev/kvm: no se puede simular; se salta)"
  exit 0
fi
python3 tests/yukon_sim.py build/HSPRED.COM "$@"
