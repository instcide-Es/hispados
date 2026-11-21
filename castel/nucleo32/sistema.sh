#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/sistema.sh: lo que llevan la memoria USB (usb.sh) y el CD
# (cd.sh), hecho una vez para los dos. Se lee con «. sistema.sh» con $AQUI
# (la carpeta nucleo32), $W (una carpeta de trabajo con f/ e i/ ya hechas)
# y $MEDIO («usb» o «cd»). Deja:
#   $W/f     el núcleo (SEMILLA.BIN y sus copias ESCRIT*.BIN que piden el modo
#            gráfico), los programas, ACPI.BIN, TLS.BIN y LEEME.TXT
#   $W/i     lo que solo vale para instalar: los textos del sistema instalado,
#            BOOT.IMG, CORE.IMG, CARPETAS.TXT y los registros de los paquetes
#            (PAQUETES/*.PQI)
#   $W/of    la oficina (HVPRUN.BIN, los .HVP y los .DAT)
#   $W/paq   los paquetes del sistema (.HPQ) y su CATALOGO.TXT
#   $MODULOS los «module» de GRUB de las entradas de siempre (la unidad M:)
#   $INSTALA los de la entrada de instalar
python3 -I $AQUI/../ccastel.py $AQUI/semilla.cas -o $W/semilla.asm > /dev/null
nasm -f bin $W/semilla.asm -o $W/f/SEMILLA.BIN
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT8.BIN 800 600 32 > /dev/null
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT10.BIN 1024 768 32 > /dev/null
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT6.BIN 1024 600 32 > /dev/null
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT13.BIN 1366 768 32 > /dev/null
MODULOS=
for p in uordenes:ORDENES ubloc:BLOC uconsola:CONSOLA uequipo:EQUIPO upaquete:PAQUETES uactual:ACTUALIZ uhola:HOLA ueco:ECO uprueba:PRUEBA ubucle:BUCLE ured:RED uipconf:IPCONFIG uping:PING udescarg:DESCARGA uhttps:HTTPS uinstala:INSTALAR uasiste:ASISTE; do
  f=${p%%:*}; N=${p##*:}
  python3 -I $AQUI/../ccastel.py $AQUI/$f.cas -o $W/$f.asm > /dev/null
  nasm -f bin $W/$f.asm -o $W/f/$N.BIN
  # (el programa que instala y el asistente no van con las entradas de siempre)
  case $N in INSTALAR|ASISTE) continue ;; esac
  MODULOS="$MODULOS  module /$N.BIN $N.BIN
"
done
# el servicio de ACPI (acpi/pentium/: apagar de verdad y la batería), un
# programa más: va en la raíz, como módulo y, al instalar, a \HISPA
bash $AQUI/../../acpi/pentium/hacer.sh $W/f/ACPI.BIN
MODULOS="$MODULOS  module /ACPI.BIN ACPI.BIN
"
# (TLS) el servicio TLS (nucleo32/tls/: BearSSL, HTTPS), igual: en la raíz,
# como módulo y, al instalar, a \HISPA
bash $AQUI/tls/hacer.sh $W/f/TLS.BIN
MODULOS="$MODULOS  module /TLS.BIN TLS.BIN
"
# lo que se instala en el disco: el núcleo (con el nombre que tendrá allí), los
# programas, los textos del sistema instalado y el arranque del disco
INSTALA="  module /ESCRIT8.BIN NUCLEO.BIN
  module /INSTALAR.BIN INSTALAR.BIN
  module /ASISTE.BIN ASISTE.BIN
$MODULOS  module /INSTALA/LEEME.TXT LEEME.TXT
  module /INSTALA/MANUAL.TXT MANUAL.TXT
  module /INSTALA/BOOT.IMG BOOT.IMG
  module /INSTALA/CORE.IMG CORE.IMG
"
# la oficina (nucleo32/hvprun/hacer.sh): su ejecutor, HVPRUN.BIN,
# va con los programas, a \HISPA; lo de \OFICINA, a C:\OFICINA: lo dice
# CARPETAS.TXT («ARCHIVO.EXT=CARPETA»), que INSTALAR lee y no instala
bash $AQUI/hvprun/hacer.sh $W/of > /dev/null
INSTALA="$INSTALA  module /HVPRUN.BIN HVPRUN.BIN
"
: > $W/i/CARPETAS.TXT
for f in $W/of/*.HVP $W/of/*.DAT; do
  n=$(basename $f)
  INSTALA="$INSTALA  module /OFICINA/$n $n
"
  printf '%s=OFICINA\r\n' $n >> $W/i/CARPETAS.TXT
done
iconv -f UTF-8 -t CP437 > $W/i/LEEME.TXT <<'L'
Hispa-Pentium 0.8, instalado en el disco duro.

El sistema está en la carpeta \HISPA de esta unidad (C:): el núcleo
(NUCLEO.BIN), los programas (ORDENES.BIN, BLOC.BIN, CONSOLA.BIN, EQUIPO.BIN...),
sus ajustes (HISPA.INI) y el menú de arranque (GRUB.CFG).
En MANUAL.TXT, en esa carpeta, hay un resumen de lo que se puede hacer.
L
iconv -f UTF-8 -t CP437 > $W/i/MANUAL.TXT <<'L'
HISPA-PENTIUM 0.8 - RESUMEN

Hispa-Pentium es un sistema de 32 bits escrito en Castel, de la familia
Hispa-DOS. Este es el sistema instalado en el disco duro.

EL ESCRITORIO
  Inicio (abajo a la izquierda) abre el menú: el Bloc de notas, la Consola,
  Mi equipo, los documentos (.TXT) de la raíz del disco, Configuración y
  Apagar el equipo. Los iconos del tapiz se abren con dos clics.
  Las ventanas se mueven por su título, cambian de tamaño por el borde y
  tienen sus botones de minimizar, maximizar y cerrar. El botón secundario
  del ratón abre un menú en casi todas partes.
  El Bloc de notas guarda con F2. Cortar, copiar y pegar: Ctrl+X, Ctrl+C y
  Ctrl+V (el portapapeles es de todo el sistema).
  En los portátiles, la bandeja enseña la batería: con el puntero quieto
  encima, lo que queda, si está enchufado y la salud de la batería.

LA RED (por cable)
  Con el cable enchufado, el sistema pide la IP solo (DHCP) y el icono de la
  bandeja (abajo a la derecha) se pone verde. En la Consola:
    IPCONFIG               cómo está la red (IPCONFIG /TODO: con detalle)
    PING nombre            si contesta ese equipo (PING 8.8.8.8)
    DESCARGAR http[s]://...  baja un archivo (https: cifrado, con el
                           certificado del servidor comprobado)
    HTTPS dirección        pide una página y la enseña (HTTPS /ESTADO: la
                           fecha que usa y las autoridades de confianza)
  Las tarjetas que lleva: Intel PRO/1000, Realtek RTL8139 y Marvell Yukon
  88E8059. El Wi-Fi, todavía no.

LA CONSOLA (el intérprete de órdenes)
  AYUDA dice las órdenes: DIR, TYPE, COPY, DEL, REN, MD, CD, RD, PATH, VER,
  MEM, FECHA, CLS... Un programa se lanza por su nombre. C:, X: cambian de
  unidad (X: es una memoria USB). Hay archivos de órdenes (.BAT).

PROGRAMAS Y ACTUALIZACIONES (PAQUETES)
  Inicio > Actualizar abre la ventana «Programas y actualizaciones»: lo
  instalado, lo que hay, «Buscar novedades» y los botones para instalar,
  actualizar y quitar, con su barra. Al arrancar, si hay red, el sistema
  mira solo si hay actualizaciones y, si las hay, lo dice en una ventanita
  encima de la bandeja (que se va sola).
  En la Consola: PAQUETES LISTA dice lo instalado; PAQUETES CATALOGO, lo
  que se puede instalar; PAQUETES INSTALA nombre (o X.HPQ) lo instala con
  lo que necesite; PAQUETES ACTUALIZA lo pone al día; PAQUETES QUITA nombre
  lo quita. Los paquetes se buscan en la carpeta \PAQUETES de una memoria
  USB, si la hay, y si no, en Internet (http://actualiza.hispados.es/
  pentium/): lo de Internet solo se usa si lleva la firma de Hispa-Pentium.
  Si se va la luz a mitad, la próxima vez se deshace lo que quedó a medias.
  Si se pone al día el núcleo, hay que reiniciar; si el nuevo no llegara a
  arrancar, el equipo vuelve solo al de antes (NUCLEO.ANT).

LAS UNIDADES
  C:, D:...  los discos de dentro del equipo (con formato FAT)
  D:, E:...  los lectores de CD y DVD (la primera letra libre tras los
             discos): el disco se mete y se saca en marcha; en Mi equipo,
             su menú tiene «Expulsar» (en la Consola: EXPULSAR D:)
  X:, Y:     las memorias USB
  Imágenes   una .ISO (o .CUE/.BIN, o .IMG de disquete) se monta como una
             unidad más: dos clics en ella en Mi equipo, o en la Consola
             MONTAR C:\DISCOS\ALGO.ISO (MONTAR sin nada dice lo montado;
             DESMONTAR E: la quita)
  M:         los archivos que trae el cargador (solo en el arranque de
             emergencia)

LA CARPETA \HISPA
  NUCLEO.BIN   el núcleo
  *.BIN        los programas
  ACPI.BIN     el servicio de ACPI: apagar de verdad y la batería de los
               portátiles (sin él, «Apagar» solo dice que ya se puede)
  TLS.BIN      el servicio TLS: las conexiones cifradas (https). Con el
               reloj del PC mal, pide la hora a actualiza.hispados.es;
               TLS.CCH y TLS.SEM son suyos. Más autoridades: los .CER de
               la carpeta \HISPA\RAICES
  HISPA.INI    los ajustes: usuario, equipo, teclado (es o us) y modo
  GRUB.CFG     el menú de arranque. La línea «set modo=» es la medida de la
               pantalla (por ejemplo 1024x768x32, o auto)
  PANTALLA.ENV la medida elegida en Configuración (Cambiar...): la lee
               GRUB.CFG y va por delante de «set modo=»; no tocar a mano
  PAQUETES     lo instalado (un .PQI por paquete): no tocar a mano
  ASISTE.BIN   el asistente del primer arranque: si se borra HISPA.INI,
               vuelve a salir al arrancar
  MBR.ANT      el primer sector que tenía el disco antes de instalar
  NUCLEO.ANT   el núcleo de antes de la última actualización (si la hubo)
  GRUBENV      lo que apunta el arranque al cambiar el núcleo: no tocar

EL ARRANQUE
  El menú que sale al encender tiene, además del escritorio, el intérprete
  de órdenes en modo texto y un arranque de emergencia que no toca los
  discos ni el USB.

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
L
sed -i 's/$/\r/' $W/i/LEEME.TXT $W/i/MANUAL.TXT
# el arranque del disco: GRUB con todo lo que necesita dentro (en el disco
# instalado no hay carpeta de módulos), que busca \HISPA\GRUB.CFG
printf 'search.file /HISPA/GRUB.CFG root\nset prefix=($root)/HISPA\n' > $W/antes.cfg
grub-mkimage -O i386-pc -o $W/i/CORE.IMG -c $W/antes.cfg -p /HISPA biosdisk part_msdos part_gpt fat multiboot normal configfile loadenv \
  search_fs_file vbe vga video video_fb video_bochs video_cirrus chain echo test read boot halt reboot videoinfo ls cat
cp /usr/lib/grub/i386-pc/boot.img $W/i/BOOT.IMG
# (como hace grub-bios-setup en un disco duro: no fiarse de la unidad que
# dice la BIOS si no es un disco; el resto de GRUB, desde el sector 1)
printf '\x90\x90' | dd of=$W/i/BOOT.IMG bs=1 seek=102 conv=notrunc status=none
[ $(stat -c %s $W/i/CORE.IMG) -le 1000000 ] || { echo "usb.sh: CORE.IMG no cabe en el hueco de arranque"; exit 1; }
# los paquetes del sistema (tools/hpq.py: base, bloc, consola, equipo, red,
# tls, ejemplos, paquetes, hvprun y oficina), hechos de lo mismo que se instala:
# van en \PAQUETES de la memoria, con su CATALOGO.TXT (PAQUETES los encuentra
# ahí); y sus registros (.PQI), con lo que se instala: INSTALAR los deja en
# C:\HISPA\PAQUETES (lo dice CARPETAS.TXT), y así PAQUETES sabe luego qué hay
python3 -I $AQUI/../../tools/hpq.py sistema $W/paq $W/i $W/f $W/of > /dev/null
mkdir $W/i/PAQUETES
for f in $W/paq/PQI/*.PQI; do
  n=$(basename $f)
  cp $f $W/i/PAQUETES/
  INSTALA="$INSTALA  module /INSTALA/PAQUETES/$n $n
"
done
cat $W/paq/PQI/CARPETAS.TXT >> $W/i/CARPETAS.TXT
INSTALA="$INSTALA  module /INSTALA/CARPETAS.TXT CARPETAS.TXT
"
if [ "$MEDIO" = cd ]; then
  printf 'Hispa-Pentium 0.8: un n\xa3cleo de 32 bits escrito en Castel.\r\nEste archivo est\xa0 en el CD de arranque y lo ha tra\xa1do el cargador a la\r\nmemoria (la unidad M:, de solo lectura): el n\xa3cleo no lee el CD.\r\n' > $W/f/LEEME.TXT
  # (para INSTALAR: se instala desde el CD; él no lo instala)
  printf 'CD\r\n' > $W/i/MEDIO.TXT
  INSTALA="$INSTALA  module /INSTALA/MEDIO.TXT MEDIO.TXT
"
else
  printf 'Hispa-Pentium 0.8: un n\xa3cleo de 32 bits escrito en Castel.\r\nEste archivo est\xa0 en la memoria USB de arranque (la unidad X:) y, por si\r\nel USB no se deja, ha venido tambi\x82n con el cargador (la unidad M:).\r\n' > $W/f/LEEME.TXT
fi
MODULOS="$MODULOS  module /LEEME.TXT LEEME.TXT
"
