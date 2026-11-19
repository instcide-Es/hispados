#!/bin/bash
# Deja sin espera el menú de arranque del CONFIG.SYS de una imagen de uso
# (disquete o memoria USB): MENUDEFAULT=bloque,0 arranca esa opción sin
# preguntar. Para las pruebas que arrancan esas imágenes y teclean a tiempo
# fijo por el puerto serie.
#   uso: sinmenu.sh imagen [bloque]      (bloque: NORMAL si no se dice)
#        sinmenu.sh imagen ARRANCAR      solo el primer menú (arrancar sin
#                                        instalar / instalar), sin esperar:
#                                        el de las opciones sale como siempre
# La imagen USB lleva la partición en el sector 2048; el disquete, no.
# Makes the boot menu of a distribution image pick an option without waiting.
set -u
IMG=$1
BLOQUE=${2:-NORMAL}
if [ "$(stat -c %s "$IMG")" -gt 2949120 ]; then IMG="$IMG@@1048576"; fi
T=$(mktemp -d)
export MTOOLS_SKIP_CHECK=1
mtype -i "$IMG" ::CONFIG.SYS > $T/c.sys || { rm -rf $T; exit 1; }
# el primer menú (arrancar sin instalar / instalar), a la 1 sin esperar; el
# de las opciones, a la que se pida
if [ "$BLOQUE" = ARRANCAR ]; then
  sed -i "s/^MENUDEFAULT=ARRANCAR,.*/MENUDEFAULT=ARRANCAR,0\r/" $T/c.sys
else
  sed -i -e "s/^MENUDEFAULT=ARRANCAR,.*/MENUDEFAULT=ARRANCAR,0\r/" \
         -e "/^MENUDEFAULT=ARRANCAR,/!s/^MENUDEFAULT=.*/MENUDEFAULT=$BLOQUE,0\r/" $T/c.sys
fi
grep -q "^MENUDEFAULT=$BLOQUE,0" $T/c.sys || { echo "sinmenu.sh: $1 no tiene MENUDEFAULT en CONFIG.SYS"; rm -rf $T; exit 1; }
mcopy -o -i "$IMG" $T/c.sys ::CONFIG.SYS
rm -rf $T
