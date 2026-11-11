#!/bin/bash
# Crea build/compat.img (2,88 MB): Hispa-DOS + programas de FreeDOS en \FD
# Builds build/compat.img with Hispa-DOS plus the FreeDOS test programs in \FD.
set -eu
cd "$(dirname "$0")/../.."
DL=tests/thirdparty/dl
IMG=build/compat.img
rm -f $IMG
mformat -C -f 2880 -v COMPAT -B build/boot.bin -i $IMG ::
for f in KERNEL.SYS COMMAND.COM FDISK.COM FORMAT.COM; do mcopy -i $IMG build/$f ::$f; done
mcopy -i $IMG tests/disk/README.TXT ::README.TXT
mmd -i $IMG ::FD
for f in attrib/BIN/ATTRIB.COM choice/BIN/CHOICE.EXE deltree/BIN/DELTREE.COM \
         edit/BIN/EDIT.EXE edlin/BIN/EDLIN.EXE fc/BIN/FC.EXE find/BIN/FIND.COM \
         label/BIN/LABEL.EXE mem/BIN/MEM.EXE more/BIN/MORE.EXE sort/BIN/SORT.EXE \
         tree/BIN/TREE.COM xcopy/BIN/XCOPY.EXE debug/BIN/DEBUG.COM \
         nansi/BIN/NANSI.SYS ctmouse/BIN/CTMOUSE.EXE himemx/BIN/HIMEMX.EXE; do
  mcopy -i $IMG $DL/$f ::FD/$(basename $f)
done
mmd -i $IMG ::DATOS
printf 'zeta\r\nalfa\r\nmedio\r\nbeta\r\n' > build/lista.txt
mcopy -i $IMG build/lista.txt ::DATOS/LISTA.TXT
mcopy -i $IMG tests/disk/DOCS/INFO.TXT ::DATOS/INFO.TXT
mdir -i $IMG ::FD | tail -2
