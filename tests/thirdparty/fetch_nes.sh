#!/bin/bash
# Descarga ROM de prueba de libre distribución para Hispa-Nes. No van en el
# repositorio (su licencia no es una licencia libre formal): se guardan en
# tests/thirdparty/dl/nes/ (ignorado) y tests/run_hispanes_tests.sh las usa
# si están; si no se pueden descargar, esas pruebas se saltan.
#  - nestest.nes y nestest.log, de Kevin Horton: todas las instrucciones del
#    procesador, con el registro de referencia instrucción a instrucción;
#  - algunas de las ROM de prueba de Shay Green («blargg»): instrucciones
#    oficiales (en un cartucho del tipo 1), sonido, retorno vertical y el
#    contador de líneas del cartucho del tipo 4.
# Downloads freely distributable NES test ROMs (not stored in the repository).
set -u
DIR=$(cd "$(dirname "$0")" && pwd)/dl/nes
mkdir -p "$DIR"
baja() {                                # baja URL NOMBRE
  [ -s "$DIR/$2" ] && return 0
  curl -sfL --max-time 60 -A "Mozilla/5.0" -o "$DIR/$2.tmp" "$1" && mv "$DIR/$2.tmp" "$DIR/$2" && echo "  $2"
  rm -f "$DIR/$2.tmp"
}
baja 'https://www.qmtpro.com/~nes/misc/nestest.nes' nestest.nes
baja 'https://www.qmtpro.com/~nes/misc/nestest.log' nestest.log
BASE=https://raw.githubusercontent.com/christopherpow/nes-test-roms/master
while read -r ruta nombre; do
  baja "$BASE/$ruta" "$nombre"
done <<'FIN'
instr_test-v5/official_only.nes official_only.nes
apu_test/rom_singles/1-len_ctr.nes apu_1-len_ctr.nes
apu_test/rom_singles/2-len_table.nes apu_2-len_table.nes
apu_test/rom_singles/3-irq_flag.nes apu_3-irq_flag.nes
apu_test/rom_singles/7-dmc_basics.nes apu_7-dmc_basics.nes
ppu_vbl_nmi/rom_singles/01-vbl_basics.nes vbl_01-vbl_basics.nes
ppu_vbl_nmi/rom_singles/03-vbl_clear_time.nes vbl_03-vbl_clear_time.nes
ppu_vbl_nmi/rom_singles/04-nmi_control.nes vbl_04-nmi_control.nes
mmc3_test_2/rom_singles/1-clocking.nes mmc3_1-clocking.nes
mmc3_test_2/rom_singles/2-details.nes mmc3_2-details.nes
mmc3_test_2/rom_singles/3-A12_clocking.nes mmc3_3-A12_clocking.nes
mmc3_test_2/rom_singles/5-MMC3.nes mmc3_5-MMC3.nes
oam_read/oam_read.nes oam_read.nes
FIN
exit 0
