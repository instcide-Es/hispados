#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas.sh - las pruebas de Castel. Cuatro partes:
#   1. Los errores: cada ejemplos/errores/*.cas tiene que dar, al compilar, el
#      mensaje (con su línea y su columna) que lleva escrito en «; ERROR: ».
#   2. Los ejemplos: se compilan todos, se ejecutan dentro de Hispa-8086 (en
#      QEMU) con la salida mandada a un archivo, y cada una se compara con
#      ejemplos/esperado/NOMBRE.txt. Entre ellos, los de la Fase 4: los
#      números de 32 bits (largos), la biblioteca escrita en Castel (biblio,
#      ficheros) y los programas en varios archivos (modulos, con los módulos
#      de ejemplos/partes).
#   3. MAS: la orden MAS escrita en Castel (ejemplos/mas.cas) frente a la de
#      ensamblador (h8086/ordenes/mas.asm), con las mismas entradas y las
#      mismas teclas: archivo, «<», tubería, archivo que no está, pausas,
#      Esc. Tienen que escribir lo mismo, octeto a octeto, en la salida y en
#      la pantalla.
#   4. CONTAR y NUMERA (ejemplos/contar.cas y numera.cas): los programas de
#      verdad de la Fase 4, con archivos, «<», tubería, uno que no está y uno
#      de más de 65535 octetos; lo que escriben se compara con lo que sale de
#      hacer la misma cuenta en Python.
#   5. El ensamblador propio: ensambla.py (la referencia, en Python) y ENSAMBLA
#      (ensamblador/ensambla.cas, escrito en Castel, corriendo en Hispa-8086)
#      ensamblan todos los ejemplos, la biblioteca entera, las formas de
#      ensamblador/formas.asm y el propio ENSAMBLA, y cada archivo tiene que
#      salir igual, octeto a octeto, que el de NASM.
#   6. El compilador escrito en Castel (compilador/castel.cas -> CASTEL.EXE),
#      dentro de Hispa-8086: saca el mismo ensamblador que ccastel.py para
#      todos los ejemplos, la biblioteca entera y ENSAMBLA, y dice los mismos
#      errores. Y EL ARRANQUE EN TRES PASOS: (1) ccastel.py compila el
#      compilador; (2) ese CASTEL.EXE, dentro de Hispa-8086, se compila a sí
#      mismo y ENSAMBLA lo ensambla; (3) el resultado se compila otra vez.
#      Los pasos 2 y 3 tienen que dar el mismo ensamblador y el mismo .EXE
#      (que son, además, los del paso 1 con NASM).
#   8. Castel de 32 bits (CASTEL32.md): los ejemplos de 16 bits que no tocan
#      el DOS, compilados como núcleo de 32 bits, escriben lo mismo; los de
#      ejemplos32 (núcleos en QEMU y programas Hispa32 con HW32, si está el
#      disquete de Hispa-DOS), con la biblioteca de 32 bits (biblio32);
#      cuentas al azar con las reglas de 32 bits; y la semilla de
#      Hispa-Pentium (nucleo32): IDT, reloj, teclado, excepciones, la
#      paginación con su fallo de página atendido, el montón (sin fugas, y
#      viendo lo que lo estropea), las tareas que se alternan con el reloj,
#      dormir y esperar, y los programas del anillo 3 con su llamada al
#      sistema (y los que hacen lo que no deben, que se acaban). Y con un
#      disco duro IDE de imagen (nucleo32/disco.sh; FAT12, FAT16, FAT32 y con
#      tabla de particiones): el controlador ATA, la FAT, cargar programas
#      del disco, las llamadas al sistema nuevas (teclado, archivos, lanzar y
#      esperar, memoria que crece), la pila que crece sola y el intérprete
#      de órdenes del anillo 3. Y escribiendo en el disco (en copias de esas
#      imágenes, que luego miran fsck.fat y mtools): crear, escribir, alargar,
#      cortar y borrar archivos, las carpetas, COPY, DEL, REN, MD, RD y la
#      redirección; CD y la carpeta de trabajo, PATH, los archivos .BAT, la
#      historia de la línea y Ctrl+C; el teclado español y los nombres largos.
#   9. Hispa-Pentium 0.6: los discos SATA (AHCI: leer y escribir por DMA,
#      con FAT32, particiones MBR y GPT), dos discos a la vez con su letra
#      (C:, D:) y las rutas con letra, el permiso de escritura por disco
#      («escritura», «escritura=usb»); y el USB (xHCI, EHCI y UHCI) sin teclado ni
#      ratón PS/2: el teclado USB (el español), el ratón USB en el
#      escritorio, un concentrador, la memoria USB como unidad X: (leer,
#      escribir, fsck.fat), «sinusb», Mi equipo con sus unidades, y la
#      memoria USB con GRUB arrancada como tal (el núcleo la lee él). Y la
#      oficina de Hispa-386 dentro de Hispa-Pentium (HVPRUN.BIN, los programas
#      de Hispa Ventanas): el grupo «Oficina» del menú Inicio, Facturas (hacer
#      una factura, que queda en el disco, y volver a abrirla), Pluma (escribir
#      y guardar), el portapapeles hasta el Bloc de notas, Cuentas, y Mi equipo
#      abriendo un documento de Pluma y una imagen con Mirador.
#  10. Hispa-Pentium 0.7: instalarlo en el disco duro. El programa INSTALAR
#      (pantalla azul, en modo texto) con discos SATA e IDE en blanco, con
#      una partición de otro sistema y espacio sin usar (no se toca lo que
#      hay), llenos (borrar pide dos confirmaciones), con tabla GPT y de los
#      de antes (la partición en el sector 63); la FAT32 que escribe (la mira
#      fsck.fat) y lo que copia; que sin «instalar» el núcleo no deja; y, de
#      verdad: arrancar la memoria USB (xHCI y EHCI), instalar, arrancar
#      SOLO del disco (GRUB en el MBR), el asistente del primer arranque,
#      HISPA.INI y el saludo del menú Inicio; la memoria USB, intacta.
#      Y la oficina en el disco instalado (C:\OFICINA y C:\HISPA\HVPRUN.BIN):
#      desde el menú Inicio y desde Mi equipo.
#  11. El USB en caliente (usbcal.cas): con el monitor de QEMU (device_add y
#      device_del), en xHCI y EHCI (y un teclado por las UHCI compañeras):
#      una memoria que se enchufa sale como W: y se lee; escribir y «quitar
#      con seguridad» la deja sana (fsck.fat); quitarla a lo bruto con un
#      archivo abierto no cuelga nada (el programa ve errores y sigue);
#      teclados, ratones y un concentrador enchufados y quitados en marcha;
#      la memoria del sistema no se deja quitar; y en el escritorio, el
#      globo de los avisos, el icono de la bandeja con su menú y Mi equipo,
#      que se pone al día solo y tiene «Quitar con seguridad».
#  12. Hispa-Pentium 0.8: los paquetes. tools/hpq.py (paquetes .HPQ, su
#      catálogo y los registros); el programa PAQUETES en un disco
#      «instalado», con una memoria USB de paquetes: instalar con
#      dependencias (de un archivo y por su nombre), actualizar, una suma mala,
#      un conflicto, quitar lo que otro necesita (el disco, idéntico), quitar,
#      COMPRUEBA, un corte a mitad que se deshace al volver, el gancho de la red
#      (con un DESCARGA de mentira) y la ventana «Programas y actualizaciones»;
#      fsck.fat y mtools miran el disco después de cada paso.
#  13. Hispa-Pentium 0.8: la red. El servicio de red (RED.BIN) con la e1000
#      y la RTL8139 de QEMU (-netdev user): DHCP, IPCONFIG, PING, el DNS,
#      DESCARGAR contra un servidor HTTP de aquí (nucleo32/servidor_prueba.py:
#      con largo, en trozos, hasta que se cierra, redirección, 404) con lo
#      descargado igual octeto a octeto, Ctrl+C, sin tarjeta, «sinred», y el
#      icono de la red en el escritorio.
#  13 bis. (TLS) Hispa-Pentium 0.8: HTTPS. El servicio TLS (TLS.BIN,
#      nucleo32/tls/: BearSSL en el anillo 3) contra los servidores HTTPS de
#      nucleo32/tls/servidor_https.py (con su raíz de prueba en \RAICES):
#      DESCARGAR https:// (ECDSA y RSA, con largo y en trozos, igual octeto
#      a octeto), la orden HTTPS con la biblioteca Https (galletas,
#      keep-alive, POST con cabeceras, redirecciones entre servidores), los
#      certificados que no valen (otro nombre, raíz desconocida, caducado),
#      la memoria de cadenas (TLS.CCH) y la hora: el reloj del PC malo, la
#      de la cabecera Date (TLS.INI) y sin hora. Si esta máquina sale a
#      internet, también https://clicgest.es/ y https://hispados.es/ frente
#      a curl, y la hora de actualiza.hispados.es.
#  14. Hispa-Pentium 0.8: apagar de verdad y la batería. El servicio de ACPI
#      (ACPI.BIN, acpi/pentium/: HSPACPI con ACPICA en el anillo 3) con la
#      batería de mentira de las pruebas de Hispa-DOS (-acpitable): la pila
#      de la bandeja y su recuadro de ayuda, apagar (S5) y reiniciar en
#      orden con PIIX4 y con ICH9, «sinacpi» y las llamadas 150 a 153.
#  15. CASTEL32 (pruebas32.sh): el compilador escrito en Castel, de 32 bits.
#      Con corre32, en Linux: el mismo ensamblador que ccastel.py en todos los
#      programas (y los mismos errores), ENSAMB32 igual que NASM y el arranque
#      en tres pasos. Dentro de Hispa-Pentium (QEMU): el núcleo entero y sus
#      programas compilados y ensamblados ahí, idénticos, con las tablas lejos
#      del tope (--gasto), y CASTEL32 compilándose a sí mismo hasta el punto
#      fijo. Y con HW32 en Hispa-DOS (Hispa-386/486), CASTELW, también hasta
#      el punto fijo. (Desde el 20/11 el núcleo de Hispa-Pentium ya no lo
#      compila CASTEL de 16 bits en las partes 5 y 6: lo hacen CASTEL32 y
#      ENSAMB32 aquí; CASTEL.md y CASTEL32.md §11.)
#  16. Hispa-Pentium 0.8: el CD y las actualizaciones por Internet. El CD
#      (nucleo32/cd.sh: ISO con El Torito sin emulación y GRUB), en QEMU
#      como lo pone VirtualBox: «pc» con IDE (PIIX), USB OHCI y la e1000
#      82540EM, y «q35» con SATA (AHCI, el CD también); arrancar sin
#      instalar, instalar desde él y arrancar del disco. Y en el sistema
#      instalado, por la red (un servidor HTTP de aquí con un catálogo
#      firmado): el aviso de la bandeja al arrancar, la ventana, actualizar
#      el núcleo, reiniciar, que arranque el nuevo; y con un núcleo que se
#      cuelga, que GRUB vuelva solo al de antes.
#  17. Hispa-Pentium 0.8: la medida de la pantalla, desde Configuración
#      (Cambiar...). Con la VBE de Bochs (QEMU -vga std; y el CD con el modo
#      de GRUB, en un «pc» como VirtualBox, con -vga std y con -vga vmware,
#      la VMSVGA): la lista (lo que da la tarjeta y lo que dice la pantalla,
#      su EDID), «Probar...» en el momento, con una ventana maximizada que
#      se pone a la medida nueva, «Mantener», y la vuelta sola a los 15 s.
#      Sin ella (-vga cirrus, como un PC de verdad), en un sistema instalado
#      desde el CD: \HISPA\PANTALLA.ENV y GRUB.CFG, «Reiniciar ahora», el
#      arranque a prueba y «Mantener»; y si se reinicia sin darla por buena,
#      GRUB vuelve solo a la de antes. Y, con la VBE de Bochs, la medida
#      guardada que la BIOS no tiene (1200 x 800) la pone el núcleo al arrancar.
#  18. El Navegador (NAVEGA.BIN, nucleo32/navega/: NetSurf con QuickJS en el
#      anillo 3) contra el sitio de prueba de navega/servidor_pruebas.py
#      (HTTP y HTTPS con su raíz en \RAICES): CSS de fuera con variables,
#      PNG y JPEG, JavaScript (DOM, setTimeout, localStorage, fetch con
#      galletas), un formulario POST, la rueda, Fin, Atrás, el menú del botón
#      derecho y las pestañas, la dirección, el portapapeles, una descarga a
#      C:\DESCARGA (igual octeto a octeto) y las galletas al cerrar; y el x87
#      de cada programa (X87.BIN).
#  19. Hispa-Pentium 0.8.3: el Administrador de dispositivos (DISPOSIT.BIN y
#      la llamada 155, dispositivos.cas; lo de ACPI lo apunta ACPI.BIN con la
#      156): en un «pc» con e1000, EHCI (teclado, ratón, tableta y memoria),
#      SATA y la batería de mentira, en el escritorio (Mi equipo > Propiedades,
#      Configuración, las pestañas de las propiedades, copiar y guardar el
#      informe en X:); en un «q35» (RTL8139, xHCI con concentrador) el
#      informe en texto; y con «sinusb» y «sinacpi», lo desactivado.
#  20. SSH, WireGuard y Tailscale (SOLO=ssh): SSH.BIN y SSHD.BIN (en C, con
#      Monocypher) contra OpenSSH de aquí; WG.BIN contra el WireGuard del
#      núcleo Linux y TAILSCAL.BIN contra Headscale y un tailscaled (estas
#      dos, con root e ip netns; Tailscale, con HEADSCALE=binario).
#  21. (CD, SOLO=lectores) Hispa-Pentium: los lectores de CD y DVD y las imágenes de disco.
#      Una ISO de prueba con Joliet (genisoimage o xorriso): por IDE (como
#      VirtualBox: el lector en el secundario maestro, el disco en SATA), por
#      AHCI (el q35) y por USB (usb-storage con la ISO, también enchufado en
#      marcha); sus archivos, copiados al disco y comparados octeto a octeto;
#      sacar, cambiar y meter el disco con el monitor de QEMU (eject, change)
#      y EXPULSAR; MONTAR la misma ISO (y una .IMG de disquete y una
#      .CUE/.BIN en crudo) desde C:, desde el CD y desde una memoria USB que
#      se quita (la imagen se desmonta sola); y Mi equipo, con el CD y el CD
#      virtual, montado con dos clics.
#  22. [WIN32, SOLO=win32] Los programas de Windows de 32 bits y DirectX (WIN32.BIN,
#      nucleo32/win32/, docs/pentium/WIN32.md): los de consola de las pruebas
#      de HW32 (PEHOLA, HOLAC, K32PRUE con lo que da Windows 98, una DLL),
#      NASM, TCC (compila un «hola» que luego corre) y FASM, de otros
#      (tests/thirdparty/fetch_win32.sh; si no están, se saltan); los de
#      ventanas de \HISPA32 (HOLA32, AVISO32 con sus MessageBox, CALC32 con
#      el ratón y su menú, MENU32, CTL32) como ventanas de Hispa-Pentium; Naves
#      (DirectDraw a pantalla completa, DirectInput) jugando; el ejemplo DDRAW
#      de FASM (ensamblado ahí); FASMW abriendo y ensamblando HELLO.ASM, y
#      ese HELLO.EXE; Mi equipo abriendo un .EXE; y WSOCK32 (REDPRUE) contra
#      el servidor HTTP de aquí.
#  23. (MENÚ, SOLO=menu) El menú Inicio: su forma (fijados, más usados,
#      Todos los programas, la columna de la derecha y el pie siempre a la
#      vista, a 800x600, 640x480 y 1024x600), las carpetas por el «tipo» de
#      los accesos, un submenú más alto que la pantalla (flechas, rueda),
#      la flecha en diagonal, el teclado (logo, letra inicial, flechas,
#      Esc), Mis documentos; un menú emergente de 30 opciones; y los atajos
#      de todo el sistema: Alt+Tab, Alt+Espacio (Tamaño con las flechas),
#      Ctrl+Alt+Supr, Logo+R, Logo+D, Impr Pant (el BMP en \CAPTURAS),
#      Alt+F4 (en el tapiz: Apagar, con Cerrar sesión), F1 y Logo+E.
#  23 bis. (RUEDA, SOLO=rueda) La rueda del ratón en los programas: la
#      Consola (el historial de lo que ha subido, y vuelve abajo al
#      teclear), el Bloc de notas (sin mover el cursor), Mi equipo (iconos,
#      lista y detalles; Ctrl+rueda cambia la vista), el Administrador de
#      dispositivos y MUESTRA (las rejillas de controles.cas: sin mover lo
#      elegido; Ctrl+rueda, la letra).
# Necesita python3, nasm, qemu-system-i386, socat, mtools, mkfs.fat y fsck.fat
# (y, para corre32, gcc y ld con elf_i386; para la parte 16, grub-mkrescue,
# xorriso y python3-cryptography). El disquete de
# Hispa-8086 se monta solo (make en h8086) si no está hecho.
#   SOLO=errores ./pruebas.sh     solo la primera parte (no arranca QEMU)
#   SOLO=32 ./pruebas.sh          la primera, la octava y la novena (lo de 32 bits)
#   SOLO=06 ./pruebas.sh          la primera y la novena (lo nuevo de Hispa-Pentium 0.6)
#   SOLO=07 ./pruebas.sh          la primera y la décima (instalar en el disco: la 0.7)
#   SOLO=08 ./pruebas.sh          la primera y las de la 0.8 (de la undécima a la decimocuarta)
#   SOLO=caliente ./pruebas.sh    la primera y la undécima (el USB en caliente)
#   SOLO=paquetes ./pruebas.sh    la primera y la duodécima (los paquetes: PAQUETES y tools/hpq.py)
#   SOLO=red ./pruebas.sh         la primera y la decimotercera (la red)
#   SOLO=acpi ./pruebas.sh        la primera y la decimocuarta (apagar de verdad y la batería: ACPI.BIN)
#   SOLO=tls ./pruebas.sh         la primera y la 13 bis (HTTPS: el servicio TLS, TLS.BIN)
#   SOLO=cd ./pruebas.sh          la primera y la decimosexta (el CD y las actualizaciones por Internet)
#   SOLO=pantalla ./pruebas.sh    la primera y la decimoséptima (la medida de la pantalla)
#   SOLO=navegador ./pruebas.sh   la primera y la decimoctava (el Navegador: NAVEGA.BIN)
#   SOLO=dispositivos ./pruebas.sh  la primera y la 19 (el Administrador de dispositivos)
#   SOLO=ssh ./pruebas.sh         la primera y la 20 (SSH, WireGuard y Tailscale; con HEADSCALE=binario, también Tailscale)
#   SOLO=lectores ./pruebas.sh    la primera y la 21 (los CD y DVD, y las imágenes de disco)
#   SOLO=win32 ./pruebas.sh       la primera y la 22 (los programas de Windows y DirectX)
#   SOLO=rueda ./pruebas.sh       la primera y la 23 bis (la rueda del ratón en los programas)
#   SOLO=menu ./pruebas.sh        la primera y la 23 (el menú Inicio, los menús largos y los atajos)
#   SOLO=16 ./pruebas.sh          de la primera a la séptima (sin arrancar el núcleo de Hispa-Pentium)
#   SOLO=castel32 ./pruebas.sh    la primera y la decimoquinta (CASTEL32: corre32, Hispa-Pentium y HW32)
#   SIN_ARRANQUE=1 ./pruebas.sh   sin el arranque en tres pasos
#   AZAR=3 ./pruebas.sh           además, tres programas de cuentas al azar
#                                 de 16 bits (azar.py) y otros tres de 32
#                                 (azar32.py): lo que calculan frente a lo
#                                 que tiene que dar. Unos 10 s más cada uno.
#   VER=1 ./pruebas.sh            deja lo que sale en /tmp/castel-pruebas
#   CAPTURAS_A=carpeta ./pruebas.sh   deja ahí las capturas del escritorio (PPM)
set -u
AQUI=$(dirname "$(readlink -f "$0")")
H=$AQUI/../h8086
# los programas de usuario de Hispa-Pentium (usuario.cas y llamadas.cas son módulos)
USU="$AQUI/nucleo32/uinstala.cas $AQUI/nucleo32/uasiste.cas $AQUI/nucleo32/uhola.cas $AQUI/nucleo32/umalo.cas $AQUI/nucleo32/ucli.cas $AQUI/nucleo32/ueco.cas $AQUI/nucleo32/uordenes.cas $AQUI/nucleo32/uprueba.cas $AQUI/nucleo32/uescribe.cas $AQUI/nucleo32/ubucle.cas $AQUI/nucleo32/ubloc.cas $AQUI/nucleo32/uconsola.cas $AQUI/nucleo32/uequipo.cas $AQUI/nucleo32/udisposi.cas $AQUI/nucleo32/upaquete.cas $AQUI/nucleo32/uactual.cas"
# y los de la red (la 0.8): el servicio, RED.BIN, y sus programas (con sus
# módulos: rbase, rtarjeta, ryukon, rpila, rtcp y redes); van en su tanda
RED_USU="$AQUI/nucleo32/ured.cas $AQUI/nucleo32/uipconf.cas $AQUI/nucleo32/uping.cas $AQUI/nucleo32/udescarg.cas"
T=$(mktemp -d)
QPID=
acaba() { [ -n "$QPID" ] && kill $QPID 2>/dev/null; [ -n "${VER:-}" ] && { rm -rf /tmp/castel-pruebas; cp -r $T /tmp/castel-pruebas; }; rm -rf $T; }
trap acaba EXIT
fail=0
nok=0
ok() { echo "  OK   $1"; nok=$((nok + 1)); }
ko() { echo "  FAIL $1"; fail=$((fail + 1)); }
# (los ayudantes de las partes 8 y 9, aquí arriba: valen para las dos)
mira() {       # lo que se prueba, y las líneas que tienen que salir (expresiones de grep)
  local que=$1 l; shift
  for l in "$@"; do
    if ! grep -q -- "$l" $S; then ko "$que: falta «$l»"; sed 's/^/        /' $S | grep -v '^ *$' | head -60 | tail -n 20; return; fi
  done
  ok "$que"
}
# ¿está sano el volumen de esa imagen (según fsck.fat)? Con un segundo argumento, el volumen empieza en ese sector
sano() {
  local v=$1
  if [ -n "${2:-}" ]; then v=$P/vol.img; dd if=$1 of=$v bs=512 skip=$2 status=none; fi
  fsck.fat -n $v > $P/fsck.txt 2>&1
}
# un archivo de la imagen, a un archivo de aquí (falla si no está)
saca() { rm -f $3; mcopy -n -i $1 "::$2" $3 > /dev/null 2>&1; }
cap() { python3 -I $AQUI/nucleo32/mira_captura.py "$@" > $G/cap.txt 2>&1; }
miracap() {    # lo que se prueba, la captura y sus comprobaciones
  local que=$1 c=$2; shift 2
  if [ -f $G/$c.ppm ] && cap $G/$c.ppm "$@"; then ok "$que"; else ko "$que:"; sed 's/^/        /' $G/cap.txt | head -6; [ -f $G/$c.ppm ] || echo "        (no hay captura $c)"; fi
}

. $AQUI/pruebas32.sh               # la parte 15 (CASTEL32), como la función parte15

# ---------------------------------------------------------------- 1. errores
echo "== Castel: los errores se dicen en español, con línea y columna =="
for f in $AQUI/ejemplos/errores/*.cas $AQUI/ejemplos32/errores/*.cas; do
  n=$(basename $f .cas)
  esperado=$(sed -n 's/^; ERROR: //p' $f | head -1)
  if python3 -I $AQUI/ccastel.py $f -o $T/err.asm -I $AQUI/ejemplos/partes > $T/err.txt 2>&1; then
    ko "$n: compila, y tenía que dar un error"
    continue
  fi
  dicho=$(head -1 $T/err.txt | sed 's/^.*\.cas://')
  if [ "$dicho" = "$esperado" ]; then ok "$n: $dicho"; else ko "$n: dice «$dicho» y se esperaba «$esperado»"; fi
done
# SOLO=caliente, paquetes, red o acpi: la primera parte y solo esa de la 0.8;
# SOLO=08, la primera y todas las de la 0.8 (como SOLO=32, que las lleva también)
case "${SOLO:-}" in caliente|paquetes|red|acpi|tls|cd|pantalla|navegador|dispositivos|ssh|lectores|win32|rueda|menu) SOLO08=$SOLO; SOLO=08 ;; *) SOLO08= ;; esac
parte08() { case "${SOLO:-}" in ""|32) return 0 ;; 08) [ -z "$SOLO08" ] || [ "$SOLO08" = $1 ] ;; *) return 1 ;; esac; }

if [ "${SOLO:-}" = errores ]; then
  echo "== $nok bien, $fail mal =="
  exit $((fail > 0))
fi
if [ "${SOLO:-}" = castel32 ]; then
  parte15
  echo "== $nok bien, $fail mal =="
  exit $((fail > 0))
fi

# SOLO=32: solo la primera parte, la octava y la novena (los 32 bits);
# SOLO=06: solo la primera y la novena (lo nuevo de Hispa-Pentium 0.6)
if [ "${SOLO:-}" != 32 ] && [ "${SOLO:-}" != 06 ] && [ "${SOLO:-}" != 07 ] && [ "${SOLO:-}" != 08 ]; then
# -------------------------------------------------------- 2. compilar todo
echo "== Castel: los ejemplos compilan =="
EJEMPLOS=
for f in $AQUI/ejemplos/*.cas; do
  n=$(basename $f .cas)
  N=$(echo $n | tr a-z A-Z)
  [ $n = mas ] && N=MASC               # MAS.COM es la de ensamblador
  X=COM
  grep -q '^ *modelo grande' $f && X=EXE   # el «modelo grande» da un .EXE
  if $AQUI/compilar.sh $f $T/$N.$X -I $AQUI/ejemplos/partes > $T/comp.txt 2>&1; then
    ok "$n.cas -> $N.$X ($(stat -c %s $T/$N.$X) octetos)"
    # MAS, CONTAR y NUMERA llevan argumentos y sus pruebas aparte
    [ $n = mas ] || [ $n = contar ] || [ $n = numera ] || EJEMPLOS="$EJEMPLOS $n"
  else
    ko "$n.cas no compila: $(head -1 $T/comp.txt)"
  fi
done

if [ -f $T/modulos.asm ] && ! grep -q "F_Figuras__sin_usar" $T/modulos.asm && grep -q "F_Figuras__cuadrado" $T/modulos.asm; then
  ok "de un módulo solo va al programa lo que se usa"
else ko "de un módulo solo va al programa lo que se usa"; fi

# el disquete: el de Hispa-8086 con los programas, los textos y un AUTOEXEC
# que lo ejecuta todo con la salida en archivos .SAL
[ -f $H/build/hispa8086-1440.img ] || make -C $H > /dev/null 2>&1
[ -f $H/build/hispa8086-1440.img ] || { echo "No hay disquete de Hispa-8086 (make en h8086)"; exit 2; }
cp $H/build/hispa8086-1440.img $T/a.img
python3 - $T $EJEMPLOS <<'PY'
import sys
t = sys.argv[1]
# CORTO.TXT: cabe en una pantalla. Tabuladores, acentos (página 437), una
# línea sin retorno de carro y otra sin fin de línea al final
corto = ("Primera línea\r\n\tcon un tabulador\r\nuno\tdos\ttres\tcuatro\r\n"
         "1234567\ta\r\n12345678\tb\r\nsolo salto de línea\nñandú, pingüino, ¿qué?\r\n"
         "sin fin de línea").encode('cp437')
open(t + '/CORTO.TXT', 'wb').write(corto)
# LARGO.TXT: tres pausas con 24 líneas de pantalla. Líneas de justo 80 letras
# y de más, tabuladores que llegan al borde, más de un trozo de 512 octetos
# y, al final, el fin de archivo de los textos (1Ah) con algo detrás
ln = []
for i in range(1, 76):
    if i == 7:
        ln.append("x" * 80)
    elif i == 13:
        ln.append("".join(str(j % 10) for j in range(100)))
    elif i == 19:
        ln.append("a\tb\t\tc" + "\t" * 9 + "tras el borde")
    elif i == 31:
        ln.append("y" * 79)
    elif i == 40:
        ln.append("z" * 73 + "\ttab al borde")
    elif i == 45:
        ln.append("")
    else:
        ln.append(f"Línea {i:02d} de la prueba de MAS:\t{'=' * (i % 30)}")
largo = ("\r\n".join(ln) + "\r\nfin del texto\r\n").encode('cp437') + b"\x1aNO DEBE SALIR\r\n"
open(t + '/LARGO.TXT', 'wb').write(largo)
# GRANDE.TXT: más de 65535 octetos, para las cuentas de 32 bits de CONTAR
grande = "".join(f"renglón {i} del archivo grande, que pasa de 65535 octetos\r\n"
                 for i in range(1, 1501)).encode('cp437')
open(t + '/GRANDE.TXT', 'wb').write(grande)
# lo que tiene que decir CONTAR de cada uno (y el total): líneas, palabras y
# octetos, en columnas de 8
def cuenta(d):
    return (d.count(b'\n'), len(d.split()), len(d))
def renglon(c, nombre):
    return "%8d%8d%8d" % c + (" " + nombre if nombre else "") + "\n"
cl, cc, cg = cuenta(largo), cuenta(corto), cuenta(grande)
assert cg[2] > 65535
open(t + '/contar1.esp', 'w').write(
    renglon(cl, 'LARGO.TXT') + renglon(cc, 'CORTO.TXT') + "CONTAR: no puedo abrir NOHAY.TXT\n"
    + renglon(cg, 'GRANDE.TXT') + renglon(tuple(a + b + c for a, b, c in zip(cl, cc, cg)), 'total'))
open(t + '/contar2.esp', 'w').write(renglon(cc, 'CORTO.TXT'))
open(t + '/contar3.esp', 'w').write(renglon(cc, ''))
# (por la tubería llega lo que escribe TYPE, que acaba siempre con un fin de línea suyo)
open(t + '/contar4.esp', 'w').write(renglon(cuenta(grande + b'\r\n'), ''))
# y lo que tiene que escribir NUMERA: cada línea con su número delante, hasta
# el fin de archivo de los textos (1Ah)
def numera(d):
    d = d.split(b'\x1a')[0]
    sal = b''
    for i, ln in enumerate(d.splitlines(keepends=True)):
        sal += b"%5d: " % (i + 1) + ln
    return sal
assert b'\r' not in corto.replace(b'\r\n', b'') and b'\r' not in largo.replace(b'\r\n', b'')
open(t + '/numera1.esp', 'wb').write(numera(largo))
open(t + '/numera2.esp', 'wb').write(numera(corto))
# cuántas pausas tiene que haber (la misma cuenta que hace MAS)
col = lin = pausas = 0
for c in largo:
    if c == 0x1A:
        break
    nueva = False
    if c == 9:
        while True:
            col += 1
            if col >= 80:
                col = 0
                nueva = True
                break
            if col % 8 == 0:
                break
    elif c == 10:
        nueva = True
    elif c == 13:
        col = 0
    elif c >= 32:
        col += 1
        if col >= 80:
            col = 0
            nueva = True
    if nueva:
        lin += 1
        if lin >= 24:
            lin = 0
            pausas += 1
assert pausas == 3, pausas
lote = ["@ECO OFF"]
for n in sys.argv[2:]:
    N = n.upper()
    lote.append(f"{N} > {N}.SAL")
    if N == "NIVEL":
        lote += ["IF ERRORLEVEL 7 ECO siete > NIV7.SAL", "IF ERRORLEVEL 8 ECO ocho > NIV8.SAL"]
lote += [
    "CONTAR LARGO.TXT CORTO.TXT NOHAY.TXT GRANDE.TXT > CONTAR1.SAL",
    "IF ERRORLEVEL 1 ECO nivel 1 > CONTAR1N.SAL",
    "CONTAR CORTO.TXT > CONTAR2.SAL",
    "IF ERRORLEVEL 1 ECO nivel 1 > CONTAR2N.SAL",
    "CONTAR < CORTO.TXT > CONTAR3.SAL",
    "TYPE GRANDE.TXT | CONTAR > CONTAR4.SAL",
    "NUMERA LARGO.TXT > NUMERA1.SAL",
    "NUMERA < CORTO.TXT > NUMERA2.SAL",
    "NUMERA NOHAY.TXT > NUMERA3.SAL",
    "IF ERRORLEVEL 1 ECO nivel 1 > NUMERA3N.SAL",
]
for prog, l in (("MAS", "M"), ("MASC", "C")):
    lote += [
        "LIMPIAR",
        f"{prog} CORTO.TXT > {l}1.SAL",
        f"{prog} < CORTO.TXT > {l}2.SAL",
        f"TYPE CORTO.TXT | {prog} > {l}3.SAL",
        f"{prog} NOHAY.TXT > {l}4.SAL",
        f"IF ERRORLEVEL 1 ECO nivel 1 > {l}4N.SAL",
        f"{prog}    CORTO.TXT   > {l}5.SAL",
        f"{prog} LARGO.TXT > {l}6.SAL",           # pausas 1 a 3
        "LIMPIAR",
        f"{prog} LARGO.TXT",                      # pausas 4 a 6, en la pantalla
        "LIMPIAR",
        f"TYPE LARGO.TXT | {prog}",               # pausa 7 y pausa 8: Esc
        f"IF ERRORLEVEL 1 ECO nivel 1 > {l}7N.SAL",
        f"{prog} CORTO.TXT",
        f"{prog} < LARGO.TXT > {l}8.SAL",         # pausas 9 a 11
        f"{prog} NOHAY.TXT",
        f"{prog} LARGO.TXT > {l}9.SAL",           # pausas 12 a 14
        "LIMPIAR",
        f"{prog} LARGO.TXT",                      # pausa 15: Esc
        f"IF ERRORLEVEL 1 ECO nivel 1 > {l}10N.SAL",
    ]
lote += ["LIMPIAR", "ECO [castel-fin]"]
open(t + '/AUTOEXEC.BAT', 'wb').write(("\r\n".join(lote) + "\r\n").encode('cp437'))
PY
[ $? = 0 ] || { echo "No se han podido preparar los textos de prueba"; exit 2; }
mcopy -o -i $T/a.img $T/*.COM $T/*.EXE $T/CORTO.TXT $T/LARGO.TXT $T/GRANDE.TXT $T/AUTOEXEC.BAT ::
POR_PROG=15         # pausas de cada una de las dos MAS
ESC="8 15"          # las que se contestan con Esc

# ------------------------------------------------------------ QEMU
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.25}"; }
# la pantalla de texto (80x25, un octeto de letra y otro de color), a texto
pantalla() {
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.3
  python3 - $T/scr.bin <<'PY' > $T/$1.txt
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print(bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())
PY
}
echo "== Castel: los ejemplos, dentro de Hispa-8086 (QEMU) =="
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/a.img,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
sleep 4
np=0
hecho=0
for ((i = 0; i < 400; i++)); do
  pantalla p
  if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
  if grep -qF -- '-- Más --' $T/p.txt; then
    np=$((np + 1))
    cp $T/p.txt $T/pausa_$np.txt
    tecla=spc
    for e in $ESC; do
      if [ $np = $e ] || [ $np = $((e + POR_PROG)) ]; then tecla=esc; fi
    done
    mon "sendkey $tecla" 0.6
  else
    sleep 0.4
  fi
done
mon quit 0.5
wait $QPID 2>/dev/null
QPID=
if [ $hecho = 0 ]; then
  ko "el lote no ha llegado al final (pantalla en $T/p.txt):"
  sed 's/^/        /' $T/p.txt | grep -v '^ *$' | tail -n 8
fi
mkdir $T/sal
mcopy -n -i $T/a.img '::*.SAL' $T/sal/ > /dev/null 2>&1

for n in $EJEMPLOS; do
  N=$(echo $n | tr a-z A-Z)
  if [ ! -f $T/sal/$N.SAL ]; then ko "$n: no ha dejado salida"; continue; fi
  tr -d '\r' < $T/sal/$N.SAL | iconv -f CP437 -t UTF-8 > $T/sal/$n.txt
  if cmp -s $T/sal/$n.txt $AQUI/ejemplos/esperado/$n.txt; then
    ok "$n: la salida es la esperada"
  else
    ko "$n: la salida no es la esperada (< esperada, > la que sale):"
    diff $AQUI/ejemplos/esperado/$n.txt $T/sal/$n.txt | sed 's/^/        /' | head -12
  fi
done

if [ -s $T/sal/NIV7.SAL ] && [ ! -s $T/sal/NIV8.SAL ]; then ok "nivel: lo que devuelve principal() es el nivel de error (7)"
else ko "nivel: el programa no ha acabado con el nivel de error 7"; fi

# ---------------------------------------------------------------- 3. MAS
echo "== MAS en Castel frente a MAS en ensamblador =="
echo "  (MAS.COM en ensamblador: $(stat -c %s $H/build/MAS.COM) octetos; en Castel: $(stat -c %s $T/MASC.COM))"
compara() {    # número de la salida y lo que se prueba
  if [ ! -s $T/sal/M$1.SAL ]; then ko "$2: la de ensamblador no ha escrito nada"
  elif cmp -s $T/sal/M$1.SAL $T/sal/C$1.SAL; then ok "$2: la misma salida ($(stat -c %s $T/sal/C$1.SAL) octetos)"
  else ko "$2: la salida es distinta"; cmp $T/sal/M$1.SAL $T/sal/C$1.SAL 2>&1 | sed 's/^/        /'; fi
}
compara 1 "MAS archivo"
compara 2 "MAS < archivo"
compara 3 "orden | MAS"
compara 4 "MAS con un archivo que no está («No existe ese archivo.»)"
compara 4N "y acaba con el nivel de error 1"
compara 5 "MAS con espacios antes y después del nombre"
compara 6 "MAS archivo largo > salida (tres pausas)"
compara 8 "MAS < archivo largo > salida"
compara 9 "MAS archivo largo > salida, tras el aviso de que no está"
if [ -s $T/sal/M10N.SAL ] || [ -s $T/sal/C10N.SAL ]; then ko "tras Esc, el nivel de error es 0"; else ok "tras Esc, las dos acaban con el nivel de error 0"; fi
if [ -s $T/sal/M7N.SAL ] && [ -s $T/sal/C7N.SAL ]; then ok "tras Esc en una tubería, el mismo nivel de error (1 o más)"
elif [ -s $T/sal/M7N.SAL ] || [ -s $T/sal/C7N.SAL ]; then ko "tras Esc en una tubería, el nivel de error es distinto"
else ok "tras Esc en una tubería, el mismo nivel de error (0)"; fi
if tr -d '\r' < $T/sal/C6.SAL | iconv -f CP437 -t UTF-8 | grep -q "NO DEBE SALIR"; then ko "lo que hay tras el 1Ah no se escribe"
else ok "lo que hay tras el fin de archivo (1Ah) no se escribe"; fi
if tr -d '\r' < $T/sal/C6.SAL | iconv -f CP437 -t UTF-8 | grep -q -- "-- Más --"; then ko "el aviso no va a la salida"
else ok "el aviso «-- Más --» no acaba en el archivo de salida"; fi
if [ $np = $((2 * POR_PROG)) ]; then ok "las mismas pausas: $POR_PROG cada una"; else ko "pausas: $np, y tenían que ser $((2 * POR_PROG))"; fi
iguales=1
for ((i = 1; i <= POR_PROG; i++)); do
  if ! cmp -s $T/pausa_$i.txt $T/pausa_$((i + POR_PROG)).txt; then
    iguales=0
    ko "la pantalla en la pausa $i es distinta:"
    diff $T/pausa_$i.txt $T/pausa_$((i + POR_PROG)).txt 2>&1 | sed 's/^/        /' | head -8
  fi
done
[ $iguales = 1 ] && ok "la pantalla es la misma en las $POR_PROG pausas (también tras Esc)"
if [ -f $T/pausa_4.txt ] && grep -q "Línea 1[0-9] de la prueba" $T/pausa_4.txt && grep -qF -- "-- Más --" $T/pausa_4.txt; then
  ok "en la pantalla: el texto y, debajo, «-- Más --»"
else ko "en la pantalla no está el texto con el aviso"; fi

# ---------------------------------------------------------------- 4. CONTAR
echo "== CONTAR y NUMERA: programas con módulos, biblioteca y 32 bits =="
contar() {     # número de la salida, la esperada y lo que se prueba
  if [ ! -f $T/sal/CONTAR$1.SAL ]; then ko "$3: no ha dejado salida"; return; fi
  tr -d '\r' < $T/sal/CONTAR$1.SAL | iconv -f CP437 -t UTF-8 > $T/sal/contar$1.txt
  if cmp -s $T/sal/contar$1.txt $T/contar$2.esp; then ok "$3"
  else ko "$3 (< esperado, > lo que sale):"; diff $T/contar$2.esp $T/sal/contar$1.txt | sed 's/^/        /' | head -8; fi
}
contar 1 1 "CONTAR de cuatro archivos (uno no está; otro pasa de 65535 octetos) y el total"
if [ -s $T/sal/CONTAR1N.SAL ]; then ok "y acaba con el nivel de error 1"; else ko "CONTAR no ha acabado con el nivel de error 1"; fi
contar 2 2 "CONTAR de un archivo"
if [ -s $T/sal/CONTAR2N.SAL ]; then ko "CONTAR de un archivo que está acaba con nivel de error"; else ok "y acaba con el nivel de error 0"; fi
contar 3 3 "CONTAR < archivo"
contar 4 4 "orden | CONTAR (con el archivo de más de 65535 octetos)"
numera() {     # número de la salida y lo que se prueba
  if [ ! -f $T/sal/NUMERA$1.SAL ]; then ko "$2: no ha dejado salida"
  elif cmp -s $T/sal/NUMERA$1.SAL $T/numera$1.esp; then ok "$2 ($(stat -c %s $T/sal/NUMERA$1.SAL) octetos, los mismos)"
  else ko "$2: no sale lo esperado"; cmp $T/numera$1.esp $T/sal/NUMERA$1.SAL 2>&1 | sed 's/^/        /'; fi
}
numera 1 "NUMERA de un archivo de 76 líneas (para en el 1Ah)"
numera 2 "NUMERA < archivo"
if [ -s $T/sal/NUMERA3N.SAL ] && grep -q "no puedo abrir NOHAY.TXT" $T/sal/NUMERA3.SAL; then ok "NUMERA de uno que no está: lo dice y acaba con el nivel de error 1"
else ko "NUMERA de uno que no está: no lo dice o no acaba con el nivel de error 1"; fi

# ---------------------------------------------------- 5. el ensamblador propio
echo "== El ensamblador propio: los mismos octetos que NASM =="
mkdir $T/ens $T/ens/nasm
cp $T/*.asm $T/ens/
cp $AQUI/ensamblador/formas.asm $T/ens/formas.asm
# y los de 32 bits: las formas de 32, los ejemplos de ejemplos32, la semilla
# de Hispa-Pentium y unas cuentas al azar (CASTEL32.md)
cp $AQUI/ensamblador/formas32.asm $T/ens/formas32.asm
python3 -I $AQUI/azar32.py 9 150 $T/ens/az32p.cas $T/ens/az32p.esp plano
for f in $AQUI/ejemplos32/*.cas $AQUI/nucleo32/semilla.cas $USU $RED_USU $T/ens/az32p.cas; do
  python3 -I $AQUI/ccastel.py $f -o $T/ens/$(basename $f .cas).asm > /dev/null 2>&1 || ko "$(basename $f) no compila"
done
rm -f $T/ens/az32p.cas $T/ens/az32p.esp
# la biblioteca entera (también lo que nadie usa) y el propio ensamblador
printf 'm\xc3\xb3dulo Toda\n    importar Textos, Archivos, Memoria, Argumentos, Lejos\n    funci\xc3\xb3n principal()\n    fin\nfin Toda.\n' > $T/ens/toda.cas
python3 -I $AQUI/ccastel.py --todo $T/ens/toda.cas -o $T/ens/toda.asm > /dev/null 2>&1 || ko "la biblioteca entera no compila"
python3 -I $AQUI/ccastel.py $AQUI/ensamblador/ensambla.cas -o $T/ens/ensambla.asm > /dev/null 2>&1 || ko "ensambla.cas no compila"
iguales=0
distintos=
for f in $T/ens/*.asm; do
  n=$(basename $f .asm)
  nasm -f bin $f -o $T/ens/nasm/$n.bin 2> /dev/null
  python3 -I $AQUI/ensambla.py $f -o $T/ens/$n.py.bin 2> /dev/null
  if cmp -s $T/ens/nasm/$n.bin $T/ens/$n.py.bin; then iguales=$((iguales + 1)); else distintos="$distintos $n"; fi
done
if [ -z "$distintos" ]; then ok "ensambla.py: los $iguales archivos (ejemplos de 16 y de 32 bits, biblioteca entera, formas y ENSAMBLA), iguales que con NASM"
else ko "ensambla.py no da lo mismo que NASM en:$distintos"; fi
# ENSAMBLA, dentro de Hispa-8086: los ensambla todos (también a sí mismo),
# menos el núcleo de Hispa-Pentium y sus programas: esos, desde el 20/11, los
# ensambla ENSAMB32 dentro de Hispa-Pentium (la parte 15); el ENSAMBLA de 16
# bits ya no manda en el núcleo (CASTEL32.md §11)
NUC=" $(for f in $USU; do basename $f .cas; done | tr '\n' ' ')"
NRED=" $(for f in $RED_USU; do basename $f .cas; done | tr '\n' ' ')"
# en qué tanda va cada uno: 2, la semilla; 3, sus programas; 4, los de la red; 1, lo demás
tanda_de() { case "$1" in semilla) echo 2 ;; *) case "$NUC" in *" $1 "*) echo 3 ;; *) case "$NRED" in *" $1 "*) echo 4 ;; *) echo 1 ;; esac ;; esac ;; esac; }
mkdir $T/ens/sal
for tanda in 1; do
cp $H/build/hispa8086-1440.img $T/e.img
{ printf '@ECO OFF\r\n'
  for f in $T/ens/*.asm; do
    n=$(basename $f .asm)
    [ "$(tanda_de $n)" = $tanda ] || continue
    N=$(echo $n | tr a-z A-Z)
    mcopy -o -i $T/e.img $f ::$N.ASM || ko "ENSAMBLA: $N.ASM no cabe en el disquete $tanda"
    printf 'ENSAMBLA %s.ASM %s.BIN > %s.SAL\r\n' $N $N $N
  done
  printf 'ECO [castel-fin]\r\n'; } > $T/AUTOEXEC.BAT
mcopy -o -i $T/e.img $T/ens/nasm/ensambla.bin ::ENSAMBLA.COM
mcopy -o -i $T/e.img $T/AUTOEXEC.BAT ::
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/e.img,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
sleep 4
hecho=0
for ((i = 0; i < 300; i++)); do
  pantalla p
  if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
  sleep 1
done
mon quit 0.5
wait $QPID 2>/dev/null
QPID=
[ $hecho = 1 ] || ko "ENSAMBLA: el lote $tanda no ha llegado al final"
mcopy -n -i $T/e.img '::*.BIN' $T/ens/sal/ > /dev/null 2>&1
done
iguales=0
distintos=
for f in $T/ens/*.asm; do
  n=$(basename $f .asm)
  N=$(echo $n | tr a-z A-Z)
  [ "$(tanda_de $n)" = 1 ] || continue
  if cmp -s $T/ens/nasm/$n.bin $T/ens/sal/$N.BIN; then iguales=$((iguales + 1)); else distintos="$distintos $n"; fi
done
if [ -z "$distintos" ]; then ok "ENSAMBLA (en Castel, dentro de Hispa-8086): los $iguales archivos, iguales que con NASM (él mismo incluido)"
else ko "ENSAMBLA no da lo mismo que NASM en:$distintos"; fi

# ------------------------------------------------ 6. el compilador en Castel
echo "== El compilador escrito en Castel, dentro de Hispa-8086 =="
A=$T/aut
mkdir $A $A/ref $A/sal
# paso 1: ccastel.py compila el compilador, y NASM lo ensambla
if python3 -I $AQUI/ccastel.py $AQUI/compilador/castel.cas -o $A/c1.asm > $T/comp.txt 2>&1 && nasm -f bin $A/c1.asm -o $A/CASTEL.EXE 2>> $T/comp.txt; then
  ok "paso 1: ccastel.py compila el compilador (castel.cas -> CASTEL.EXE, $(stat -c %s $A/CASTEL.EXE) octetos)"
else ko "el compilador en Castel no compila: $(head -1 $T/comp.txt)"; fi
# dos disquetes: en A: la biblioteca en \BIB (la de 32 bits en \BIB32), los
# módulos de ejemplo en \PARTES, los de la semilla de Hispa-Pentium,
# los ejemplos, la biblioteca entera (--todo), ENSAMBLA, los errores y el
# temporal de CASTEL; en B:, lo que sale (el ensamblador de cada uno son, todos
# juntos, más de un disquete si va con los fuentes: el de ENSAMBLA solo pasa de
# 250 K)
python3 -I $AQUI/azar.py 7 45 $A/azar16.cas $A/azar16.esp
python3 -I $AQUI/azar32.py 7 28 $A/azar32.cas $A/azar32.esp
python3 -I $AQUI/azar32.py 8 40 $A/azar32p.cas $A/azar32p.esp plano
# (en tres tandas, cada una con sus disquetes: en la primera, los ejemplos, la
# biblioteca, ENSAMBLA, las cuentas al azar y los errores; en la segunda y la
# tercera, lo de Hispa-Pentium, que con el entorno gráfico ya no cabe con lo
# demás: los fuentes del núcleo en A: y, en B:, el ensamblador de la semilla,
# de más de 600 K (la segunda), o el de sus programas de usuario (la tercera))
# (el núcleo de Hispa-Pentium y sus programas, desde el 20/11, los compila
# CASTEL32 en la parte 15: las tablas del CASTEL de 16 bits, en 640 K, ya no
# dan para él; las tandas 2, 3 y 4 de antes eran eso)
for tanda in 1; do
cp $H/build/hispa8086-1440.img $T/c.img
mmd -i $T/c.img ::BIB ::PARTES ::BIB32
for f in $AQUI/biblioteca32/*.cas; do mcopy -i $T/c.img $f ::BIB32/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
if [ $tanda = 1 ]; then
  for f in $AQUI/biblioteca/*.cas; do mcopy -i $T/c.img $f ::BIB/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  for f in $AQUI/ejemplos/partes/*.cas; do mcopy -i $T/c.img $f ::PARTES/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  mcopy -i $T/c.img $AQUI/ensamblador/montador.cas ::MONTADOR.CAS
  LOTE="$(echo $AQUI/ejemplos/*.cas) $AQUI/ensamblador/ensambla.cas $T/ens/toda.cas $A/azar16.cas $A/azar32.cas $(echo $AQUI/ejemplos32/*.cas) $A/azar32p.cas"
else
  # (en la segunda, los módulos del núcleo; en la tercera, los programas y
  # los dos módulos que usan: todos juntos ya no caben con lo demás)
  for f in $AQUI/nucleo32/*.cas; do
    case "$(basename $f .cas)" in
      ured|uipconf|uping|udescarg|rbase|rtarjeta|ryukon|rpila|rtcp|redes) [ $tanda = 4 ] || continue ;;
      llamadas) [ $tanda = 3 ] || [ $tanda = 4 ] || continue ;;
      uhci|unidades|usb|usbalm|usbbase|usbcal|usbhid|usuario) [ $tanda = 2 ] || continue ;;
      u[a-z]*|barramenu|sha256|firma|clave) [ $tanda = 3 ] || continue ;;
      *) [ $tanda = 2 ] || continue ;;
    esac
    mcopy -i $T/c.img $f ::$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS
  done
  if [ $tanda = 2 ]; then LOTE="$AQUI/nucleo32/semilla.cas"; elif [ $tanda = 3 ]; then LOTE="$USU"; else LOTE="$RED_USU"; fi
fi
mcopy -i $T/c.img $A/CASTEL.EXE ::
# (lo de Hispa-Pentium, con B: de 2,88 MB y desde B:: ahí deja CASTEL su
# archivo temporal, que con el núcleo de la 0.6 —900 K de ensamblador— ya no
# cabe en A: con los fuentes, ni con la salida en un disquete de 1,44)
if [ $tanda = 1 ]; then mformat -C -f 1440 -i $T/cb.img ::; else mformat -C -f 2880 -i $T/cb.img ::; fi
{ printf '@ECO OFF\r\n'
  [ $tanda = 1 ] || printf 'B:\r\n'
  for f in $LOTE; do
    n=$(basename $f .cas); N=$(echo $n | tr a-z A-Z)
    todo=; [ $n = toda ] && todo=--todo
    mcopy -o -i $T/c.img $f ::$N.CAS
    python3 -I $AQUI/ccastel.py $todo $f -o $A/ref/$N.ASM -I $AQUI/ejemplos/partes > /dev/null 2>&1
    if [ $tanda = 1 ]; then printf 'CASTEL %s.CAS -o B:%s.ASM -I BIB -I PARTES -I BIB32 %s > B:%s.SAL\r\n' $N $N "$todo" $N
    else printf 'A:\\CASTEL A:\\%s.CAS -o B:%s.ASM -I A:\\BIB32 > B:%s.SAL\r\n' $N $N $N; fi
  done
  [ $tanda = 1 ] || printf 'A:\r\n'
  if [ $tanda = 1 ]; then
    i=0
    for f in $AQUI/ejemplos/errores/*.cas $AQUI/ejemplos32/errores/*.cas; do
      i=$((i + 1)); N=$(printf 'E%02d' $i)
      mcopy -i $T/c.img $f ::$N.CAS
      echo "$N $(basename $f .cas)" >> $A/errores.txt
      printf 'CASTEL %s.CAS -I BIB -I PARTES > B:%s.SAL\r\n' $N $N
    done
  fi
  printf 'ECO [castel-fin]\r\n'; } > $T/AUTOEXEC.BAT
mcopy -o -i $T/c.img $T/AUTOEXEC.BAT ::
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/c.img,format=raw,if=floppy,index=0 -drive file=$T/cb.img,format=raw,if=floppy,index=1 \
  -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
sleep 4
hecho=0
for ((i = 0; i < 300; i++)); do
  pantalla p
  if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
  sleep 1
done
mon quit 0.5
wait $QPID 2>/dev/null
QPID=
[ $hecho = 1 ] || ko "CASTEL: el lote $tanda no ha llegado al final"
mcopy -n -i $T/cb.img '::*.ASM' '::*.SAL' $A/sal/ > /dev/null 2>&1
done
iguales=0
distintos=
for f in $A/ref/*.ASM; do
  N=$(basename $f .ASM)
  if cmp -s $f $A/sal/$N.ASM; then iguales=$((iguales + 1)); else distintos="$distintos $N"; fi
done
if [ -z "$distintos" ]; then ok "CASTEL: el mismo ensamblador que ccastel.py en los $iguales programas (ejemplos de 16 y de 32 bits, biblioteca entera, ENSAMBLA y tres de cuentas al azar)"
else ko "CASTEL no saca el mismo ensamblador que ccastel.py en:$distintos"; fi
iguales=0
distintos=
while read N n; do
  esperado=$(sed -n 's/^; ERROR: //p' $AQUI/ejemplos/errores/$n.cas $AQUI/ejemplos32/errores/$n.cas 2> /dev/null | head -1)
  dicho=$(tr -d '\r' < $A/sal/$N.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1 | sed 's/^.*\.CAS://')
  if [ "$dicho" = "$esperado" ]; then iguales=$((iguales + 1)); else distintos="$distintos $n"; fi
done < $A/errores.txt
if [ -z "$distintos" ]; then ok "CASTEL: los $iguales errores, dichos igual que ccastel.py (con su línea y su columna)"
else ko "CASTEL no dice igual que ccastel.py los errores de:$distintos"; fi

# El arranque en tres pasos. Cada paso, en un Hispa-8086 con dos disquetes: en
# A: el sistema, el compilador que toca, ENSAMBLA y los fuentes (y ahí deja
# CASTEL su archivo temporal); en B:, lo que sale (el ensamblador del
# compilador son 700 K: no cabe todo en uno). Compilar y ensamblar van en dos
# arranques seguidos con el mismo B: (en Hispa-8086, después de que CASTEL
# compile, leer archivos grandes en ese mismo arranque va decenas de veces
# más despacio: el ensamblado tardaría cuatro minutos en vez de tres segundos).
arranca_ab() {     # las órdenes del AUTOEXEC (con A: en $T/d.img y B: en $T/b.img)
  printf '@ECO OFF\r\n%s\r\nECO [castel-fin]\r\n' "$1" > $T/AUTOEXEC.BAT
  mcopy -o -i $T/d.img $T/AUTOEXEC.BAT ::
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
    -drive file=$T/d.img,format=raw,if=floppy,index=0 -drive file=$T/b.img,format=raw,if=floppy,index=1 \
    -boot a -no-reboot > /dev/null 2>&1 &
  QPID=$!
  sleep 4
  hecho=0
  for ((i = 0; i < 600; i++)); do
    pantalla p
    if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
    sleep 1
  done
  mon quit 0.5
  wait $QPID 2>/dev/null
  QPID=
  [ $hecho = 1 ]
}
paso() {       # número del paso, el compilador (un .EXE) y cómo se llama en el disquete
  cp $H/build/hispa8086-1440.img $T/d.img
  mmd -i $T/d.img ::BIB
  mcopy -i $T/d.img $2 ::$3.EXE
  mcopy -i $T/d.img $T/ens/nasm/ensambla.bin ::ENSAMBLA.COM
  for f in $AQUI/compilador/*.cas; do mcopy -i $T/d.img $f ::$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  for f in $AQUI/biblioteca/*.cas; do mcopy -i $T/d.img $f ::BIB/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  rm -f $T/b.img; mformat -C -f 1440 -i $T/b.img ::
  arranca_ab "$3 CASTEL.CAS -o B:C$1.ASM -I BIB > B:P$1.SAL" || ko "el paso $1 del arranque (compilar) no ha llegado al final"
  arranca_ab "ENSAMBLA B:C$1.ASM B:C$1.EXE > B:E$1.SAL" || ko "el paso $1 del arranque (ensamblar) no ha llegado al final"
  mcopy -n -i $T/b.img "::C$1.*" '::*.SAL' $A/arr/ > /dev/null 2>&1
}
if [ -z "${SIN_ARRANQUE:-}" ]; then
  echo "== El arranque en tres pasos: el compilador se compila a sí mismo =="
  mkdir $A/arr
  paso 2 $A/CASTEL.EXE CASTEL
  if [ -s $A/arr/C2.EXE ]; then paso 3 $A/arr/C2.EXE C2; else ko "el paso 2 no ha dejado C2.EXE: $(tr -d '\r' < $A/arr/P2.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C2.ASM ] && cmp -s $A/c1.asm $A/arr/C2.ASM; then ok "paso 2: CASTEL se compila a sí mismo dentro de Hispa-8086: el mismo ensamblador que el paso 1 ($(wc -l < $A/arr/C2.ASM) líneas)"
  else ko "paso 2: el ensamblador no es el del paso 1: $(tr -d '\r' < $A/arr/P2.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C2.EXE ] && cmp -s $A/CASTEL.EXE $A/arr/C2.EXE; then ok "paso 2: ENSAMBLA lo ensambla: el mismo CASTEL.EXE que el paso 1 con NASM ($(stat -c %s $A/arr/C2.EXE) octetos)"
  else ko "paso 2: el .EXE no es el del paso 1: $(tr -d '\r' < $A/arr/E2.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C3.ASM ] && cmp -s $A/arr/C2.ASM $A/arr/C3.ASM; then ok "paso 3: el resultado se compila otra vez: el mismo ensamblador que el paso 2"
  else ko "paso 3: el ensamblador no es el del paso 2: $(tr -d '\r' < $A/arr/P3.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C3.EXE ] && cmp -s $A/arr/C2.EXE $A/arr/C3.EXE; then ok "paso 3: y el mismo .EXE: el compilador se sostiene solo"
  else ko "paso 3: el .EXE no es el del paso 2: $(tr -d '\r' < $A/arr/E3.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
fi

# ------------------------------------------------- 7. cuentas al azar (AZAR=n)
if [ -n "${AZAR:-}" ]; then
  echo "== Castel: cuentas al azar =="
  for ((s = 1; s <= AZAR; s++)); do
    python3 -I $AQUI/azar32.py $s 120 $T/azar32_$s.cas $T/azar32_$s.esp
    if ! $AQUI/compilar.sh $T/azar32_$s.cas $T/AZ32_$s.COM > $T/comp.txt 2>&1; then
      ko "azar de 32 bits $s: no compila: $(head -1 $T/comp.txt)"
      continue
    fi
    $AQUI/probar.sh $T/AZ32_$s.COM > $T/azar32_$s.sal 2>&1
    if cmp -s $T/azar32_$s.esp $T/azar32_$s.sal; then ok "azar de 32 bits $s: $(wc -l < $T/azar32_$s.esp) resultados, todos bien"
    else ko "azar de 32 bits $s: hay cuentas que no salen (< esperado, > lo que sale):"; diff $T/azar32_$s.esp $T/azar32_$s.sal | head -6 | sed 's/^/        /'; fi
  done
  for ((s = 1; s <= AZAR; s++)); do
    python3 -I $AQUI/azar.py $s 250 $T/azar$s.cas $T/azar$s.esp
    if ! $AQUI/compilar.sh $T/azar$s.cas $T/AZAR$s.COM > $T/comp.txt 2>&1; then
      ko "azar $s: no compila: $(head -1 $T/comp.txt)"
      continue
    fi
    $AQUI/probar.sh $T/AZAR$s.COM > $T/azar$s.sal 2>&1
    if cmp -s $T/azar$s.esp $T/azar$s.sal; then ok "azar $s: $(wc -l < $T/azar$s.esp) resultados, todos bien"
    else ko "azar $s: hay cuentas que no salen (< esperado, > lo que sale):"; diff $T/azar$s.esp $T/azar$s.sal | head -6 | sed 's/^/        /'; fi
  done
fi

fi   # SOLO=32

# SOLO=16: hasta aquí (lo de 16 bits, los dos compiladores y los dos ensambladores)
if [ "${SOLO:-}" = 16 ]; then
  echo "== $nok bien, $fail mal =="
  exit $((fail > 0))
fi

# ------------------------------------------------- 8. Castel de 32 bits
echo "== Castel de 32 bits: «modelo núcleo» (QEMU -kernel) y «modelo hispa32» (HW32) =="
P=$T/p32
mkdir $P
if [ "${SOLO:-}" != 06 ] && [ "${SOLO:-}" != 07 ] && [ "${SOLO:-}" != 08 ]; then
# los ejemplos de 16 bits que no tocan el DOS, compilados como núcleo de 32
# bits: tienen que escribir lo mismo
MISMOS="arreglos bucles cuenta estruct mezcla nivel pares pruebas refs saltos saludo textos modulos"
iguales=0
distintos=
for n in $MISMOS; do
  python3 - $AQUI/ejemplos/$n.cas $P/$n.cas <<'PY'
import sys, re
s = open(sys.argv[1], encoding='utf-8').read().split('\n')
for i, l in enumerate(s):
    if re.match(r'^módulo \w+', l):
        s.insert(i + 1, '    modelo núcleo')
        break
open(sys.argv[2], 'w', encoding='utf-8').write('\n'.join(s))
PY
  if ! $AQUI/compilar.sh $P/$n.cas $P/$n.bin -I $AQUI/ejemplos/partes > $T/comp.txt 2>&1; then
    distintos="$distintos $n(no compila)"; continue
  fi
  $AQUI/probar32.sh $P/$n.bin > $P/$n.sal 2>&1
  cod=$(tail -n 1 $P/$n.sal); sed -i '$d' $P/$n.sal
  esperado_c="[código 0]"; [ $n = nivel ] && esperado_c="[código 7]"
  if cmp -s $P/$n.sal $AQUI/ejemplos/esperado/$n.txt && [ "$cod" = "$esperado_c" ]; then iguales=$((iguales + 1))
  else distintos="$distintos $n"; fi
done
if [ -z "$distintos" ]; then ok "los $iguales ejemplos de 16 bits sin DOS, como núcleos de 32 bits: escriben lo mismo (y nivel acaba con 7)"
else ko "como núcleos de 32 bits, no escriben lo mismo:$distintos"; fi
# los de ejemplos32: los núcleos, en QEMU; los de hispa32, con HW32 (si está el
# disquete de Hispa-DOS: make y make nucprogs en la raíz)
for f in $AQUI/ejemplos32/*.cas; do
  n=$(basename $f .cas)
  if grep -q '^ *modelo hispa32' $f; then X=EXE; else X=BIN; fi
  if ! $AQUI/compilar.sh $f $P/$n.$X > $T/comp.txt 2>&1; then ko "$n.cas no compila: $(head -1 $T/comp.txt)"; continue; fi
  if [ $X = BIN ]; then
    $AQUI/probar32.sh $P/$n.$X > $P/$n.sal 2>&1
    cod=$(tail -n 1 $P/$n.sal); sed -i '$d' $P/$n.sal
  elif [ -f $AQUI/../build/hispados.img ] && [ -f $AQUI/../build/HW32.COM ]; then
    NIVEL=42 $AQUI/probar32.sh $P/$n.$X > $P/$n.log 2>&1
    sed '/^A:\\>/,$d' $P/$n.log | sed '/^$/d' > $P/$n.sal
    cod="[código ?]"
    grep -qx '\[nivel-42\]' $P/$n.log && ! grep -qx '\[nivel-43\]' $P/$n.log && cod="[código 42]"
  else
    echo "  (sin el disquete de Hispa-DOS no se prueba $n.EXE: make y make nucprogs en la raíz)"
    continue
  fi
  esp_cod="[código $(sed -n 's/^; código de salida: //p' $f)]"
  if cmp -s $P/$n.sal $AQUI/ejemplos32/esperado/$n.txt && [ "$cod" = "$esp_cod" ]; then
    ok "$n.$X ($(stat -c %s $P/$n.$X) octetos): la salida es la esperada y acaba con el código que toca ($cod)"
  else
    ko "$n.$X: la salida no es la esperada o acaba con otro código ($cod y no $esp_cod; < esperada, > la que sale):"
    diff $AQUI/ejemplos32/esperado/$n.txt $P/$n.sal | sed 's/^/        /' | head -12
  fi
done
# cuentas al azar, con las reglas de 32 bits
for s in 1 2; do
  python3 -I $AQUI/azar32.py $s 150 $P/az$s.cas $P/az$s.esp plano
  $AQUI/compilar.sh $P/az$s.cas $P/az$s.bin > /dev/null 2>&1
  $AQUI/probar32.sh $P/az$s.bin > $P/az$s.sal 2>&1
  sed -i '$d' $P/az$s.sal
  if cmp -s $P/az$s.esp $P/az$s.sal; then ok "azar de 32 bits en el modelo plano $s: $(wc -l < $P/az$s.esp) resultados, todos bien"
  else ko "azar plano $s: hay cuentas que no salen"; diff $P/az$s.esp $P/az$s.sal | head -6 | sed 's/^/        /'; fi
done
# la semilla de Hispa-Pentium: la IDT, el reloj, el teclado y las excepciones
$AQUI/nucleo32/probar.sh "" ayuda hora :espera hora Hola teclas apagar > $P/semilla.sal 2>&1
if grep -q "^órdenes: ayuda hora memoria mapa paginas monton tareas programas ejecutar N" $P/semilla.sal && grep -q "^no conozco «Hola»" $P/semilla.sal \
    && grep -q "^\[código 5\]" $P/semilla.sal; then ok "la semilla de Hispa-Pentium arranca, lee el teclado y entiende sus órdenes"
else ko "la semilla de Hispa-Pentium no hace lo que debe:"; sed 's/^/        /' $P/semilla.sal | head -12; fi
s1=$(grep "^llevo" $P/semilla.sal | head -1 | awk '{print $2 * 100 + $5}')
s2=$(grep "^llevo" $P/semilla.sal | tail -n 1 | awk '{print $2 * 100 + $5}')
if [ -n "$s1" ] && [ -n "$s2" ] && [ $((s2 - s1)) -ge 100 ]; then ok "el reloj (IRQ 0) corre: $s1 y luego $s2 centésimas"
else ko "el reloj no corre: «$s1» y «$s2»"; fi
if grep -q "^28 teclas pulsadas" $P/semilla.sal; then ok "el teclado (IRQ 1) cuenta las teclas, mayúsculas incluidas"
else ko "el teclado no cuenta bien las teclas: $(grep teclas $P/semilla.sal | tail -n 1)"; fi
$AQUI/nucleo32/probar.sh "" cero > $P/cero.sal 2>&1
if grep -q "excepción 0: división por cero" $P/cero.sal && grep -q "^\[código 9\]" $P/cero.sal; then ok "una división por cero llega a su excepción (la 0, por la IDT)"
else ko "la división por cero no llega a la excepción"; sed 's/^/        /' $P/cero.sal | head -6; fi
# la memoria, las tareas y el anillo 3 (con los programas uhola, umalo y ucli
# como módulos de multiboot)
$AQUI/nucleo32/probar.sh "" memoria paginas toca monton estropear turnos dormir tareas programas \
  "ejecutar 0" "ejecutar 1" "ejecutar 2" memoria apagar > $P/pentium.sal 2>&1
S=$P/pentium.sal
mira "la memoria: el mapa de multiboot da los marcos de 4 K (mapa de bits)" "^memoria: 32 MB, [0-9]* marcos libres; paginación activa; 3 programas de usuario" \
  "^marcos: [0-9]* libres de 8160 "
mira "la paginación está encendida (CR0.PG), con la identidad para el núcleo" "^paginación: activa (CR0 = 0x8" "^  identidad: de 0 a 0x01FDFFFF, 8160 páginas en 8 tablas"
mira "un fallo de página en la zona a la demanda se atiende (un marco nuevo) y la escritura llega" \
  "^escribo en 0xD0000123 (fallos de página atendidos: 1) y leo 0xCAFE0001" "^otra vez (fallos de página atendidos: 0) y leo 0xCAFE0002"
mira "el montón: 3000 vueltas de reservar y liberar, sin fugas (ni de bloques ni de marcos)" "^montón: 3000 vueltas de reservar y liberar (y [0-9]* al final), 0 fallos" "^montón sin fugas"
mira "el montón ve lo que lo estropea: escribir de más, soltar dos veces, soltar lo que no es suyo" \
  "se ha escrito más allá de los 10 octetos del bloque" "^falso" "^lo arreglo y suelto: cierto" "ya estaba libre (¿se suelta dos veces?)" \
  "no es de un bloque" "^suelto el otro: cierto" "^el montón ha visto 3 fallos; revisado, 0"
t=$(grep "^turnos:" $S)
a=$(echo "$t" | awk '{print $5}'); b=$(echo "$t" | awk '{print $9}' | tr -d ';'); n=$(echo "$t" | awk '{print $13}')
if [ -n "$t" ] && [ "${a:-0}" -gt 0 ] && [ "${b:-0}" -gt 0 ] && [ "${n:-0}" -ge 50 ]; then ok "dos tareas que no ceden nunca se alternan con el reloj (multitarea apropiativa): $n veces"
else ko "las tareas no se alternan: «$t»"; fi
if [ "$(grep '^la dormilona' $S | awk '{print $3}' | tr -d '\n')" = 123 ] && grep -q "^la dormilona 2 se despierta: ha dormido 4[0-9] tics (pidió 40)" $S \
    && grep -q "^las tres han acabado" $S; then ok "dormir y esperar: tres dormilonas despiertan cada una a su hora, y se las espera"
else ko "las dormilonas no despiertan en orden:"; grep dormilona $S | sed 's/^/        /'; fi
mira "las tareas, con sus turnos" "^   1  consola       corriendo       0" "^   2  ociosa  " "cambios de tarea en"
m1=$(grep "^uhola.bin ha acabado" $S | awk '{print $10, $13}')
mira "un programa del anillo 3, en su espacio de direcciones, escribe por la int 0x80, duerme y acaba con su código" \
  "^ejecuto uhola.bin (la tarea [0-9]*, en el anillo 3)" "^hola desde el anillo 3: soy la tarea" "^1 + 2 + ... + 1000 = 500500" \
  "^he dormido 10 tics: cierto" "^uhola.bin ha acabado con el código 3; marcos libres"
mira "la llamada al sistema no deja escribir desde la memoria del núcleo ni desde lo que no es del programa" \
  "^el núcleo no me deja enseñar su memoria: cierto" "^ni lo que no tengo: cierto"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ]; then ok "al acabar el programa, su espacio de direcciones vuelve entero: los mismos marcos libres ($m1)"
else ko "el programa de usuario se deja marcos: «$m1»"; fi
mira "un programa que lee la memoria del núcleo: fallo de página de protección, se acaba él y el núcleo sigue" \
  "fallo de página en 0x00100000 (lectura, anillo 3, protección)" "^\*\*\* se acaba la tarea [0-9]* (umalo.bin)" "^umalo.bin ha acabado con el código 255"
mira "un programa que hace «cli» en el anillo 3: protección general, y se acaba" \
  "excepción 13: protección general (código 0)" "^ucli.bin ha acabado con el código 255" "^\[código 5\]"
if grep -q "no tenía que salir" $S; then ko "un programa del anillo 3 ha seguido después de lo que no podía hacer"; else ok "lo que no podían hacer, no lo han hecho"; fi
$AQUI/nucleo32/probar.sh "" fallo > $P/fallo.sal 2>&1
if grep -q "fallo de página en 0xDEAD0000 (lectura, núcleo, no presente)" $P/fallo.sal && grep -q "^\[código 14\]" $P/fallo.sal; then ok "un fallo de página del núcleo fuera de la zona a la demanda: lo dice con la dirección y se para (código 14)"
else ko "el fallo de página del núcleo no se dice bien"; sed 's/^/        /' $P/fallo.sal | head -6; fi

# ---- el disco, la FAT, las llamadas nuevas y el intérprete de órdenes ----
# (un disco duro IDE de imagen, hecho con mkfs.fat y mtools: nucleo32/disco.sh)
echo "== Hispa-Pentium con disco: ATA, FAT, programas del disco y el intérprete de órdenes del anillo 3 =="
hechos=1
for c in 12 16 32 mbr; do
  $AQUI/nucleo32/disco.sh $P/d$c.img $c > $P/disco$c.txt 2>&1 || { hechos=0; ko "no se puede hacer el disco de prueba ($c): $(tail -n 1 $P/disco$c.txt)"; }
done
if [ $hechos = 1 ]; then
# lo que tiene que sumar TEXTOS\LARGO.TXT (el mismo que escribe disco.sh)
suma=$(python3 -c "print(sum(sum(('linea %03d de un archivo de varios clusters\r\n' % i).encode()) for i in range(1, 401)))")
# 1. desde la consola del núcleo, con FAT16
DISCO=$P/d16.img $AQUI/nucleo32/probar.sh "" disco dir "dir textos" "dir nada" "lanzar hola.bin" "lanzar eco.bin uno dos tres" \
  "lanzar noprog.bin" "lanzar nada.bin" disco apagar > $P/disco.sal 2>&1
S=$P/disco.sal
mira "el controlador ATA (PIO): IDENTIFY dice qué disco hay en el canal primario, y cuántos sectores" \
  "^disco 0: QEMU HARDDISK, 16 MB, FAT16\.$" "^disco 0: QEMU HARDDISK, 32768 sectores (16 MB), LBA" "^disco 1: QEMU DVD-ROM, lector de CD o DVD (IDE), sin disco$"
mira "la FAT16 se monta y se lee su raíz (los nombres largos se saltan; queda el corto)" \
  "^sistema de archivos: FAT16, desde el sector 0, [0-9]* clusters de 2048 octetos, [0-9]* libres" \
  "^ORDENES.BIN  *[0-9]*$" "^LEEME.TXT  *118$" "^NOMBRE~1.TXT  *32$" "^TEXTOS  *<carpeta>" "^20 entradas"
mira "una carpeta dentro de otra, y una que no está" "^LARGO.TXT  *17600$" "^PIRATA.TXT  *110$" "^5 entradas" "^no puedo abrir «nada»: no existe"
m1=$(grep "^hola.bin ha acabado" $S | awk '{print $10, $13}')
mira "un programa del anillo 3 cargado del disco (no de multiboot): corre y acaba con su código" \
  "^lanzo hola.bin del disco (la tarea [0-9]*, en el anillo 3)" "^1 + 2 + ... + 1000 = 500500" "^hola.bin ha acabado con el código 3; marcos libres"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ]; then ok "al acabar el programa del disco, la memoria vuelve entera: los mismos marcos libres ($m1)"
else ko "el programa del disco se deja marcos: «$m1»"; fi
mira "un programa del disco recibe sus argumentos" "^uno dos tres$" "^eco.bin ha acabado con el código 3"
mira "lo que no es un programa, o no está, no se lanza (y se dice por qué)" "^no puedo lanzar noprog.bin: no es un programa de Hispa-Pentium" \
  "^no puedo lanzar nada.bin: no existe" "; archivos abiertos: 0$" "^\[código 5\]"
# 2. el intérprete de órdenes del anillo 3 (ORDENES.BIN, del disco)
DISCO=$P/d16.img $AQUI/nucleo32/probar.sh "" ordenes ayuda ver mem dir "dir textos" "dir nada" "type leeme.txt" "type textos\\pirata.txt" \
  "type nada.txt" "TYPE textos" "eco hola y adios" "textos\\eco2 desde una carpeta" hola malo nada noprog prueba :espera "prueba fuera" \
  "prueba pozo" :espera salir tareas memoria disco apagar > $P/ordenes.sal 2>&1
S=$P/ordenes.sal
mira "el intérprete de órdenes es un programa del anillo 3 cargado del disco, y lee el teclado por la llamada al sistema" \
  "^lanzo ORDENES.BIN del disco (la tarea [0-9]*, en el anillo 3)" "^Hispa-Pentium: intérprete de órdenes (un programa del anillo 3" \
  "^C:.>ayuda$" "^Las órdenes de Hispa-Pentium:" "^  TYPE archivo "
mira "VER y MEM" "^Hispa-Pentium 0\.8: un núcleo de 32 bits escrito en Castel" "^Memoria: 32640 K en total, [0-9]* K libres ([0-9]* marcos de 4 K)" \
  "^Este intérprete ocupa [0-9]* K. Tareas en marcha: 4\.$"
mira "DIR: la raíz y una carpeta, con sus tamaños, sus fechas y lo que queda libre" "^ Directorio de C:.$" "^LEEME    TXT  *118  [0-9/]*  [0-9:]*$" \
  "^TEXTOS  *<DIR>  " "^ *17 archivos  *[0-9]* octetos$" "^ *3 carpetas  *[0-9]* octetos libres$" "^ Directorio de C:.TEXTOS$" \
  "^LARGO    TXT  *17600  " "^ *3 archivos  *18[0-9]* octetos$" "^No puedo abrir nada: no existe\.$"
mira "TYPE: un archivo de la raíz y uno de una carpeta (abrir, leer y cerrar por la llamada al sistema)" \
  "^Este archivo está en un disco FAT y lo enseña la orden TYPE\.$" "^Con diez cañones por banda,$" "^un velero bergantín\.$" \
  "^No puedo abrir nada.txt: no existe\.$" "^textos es una carpeta"
mira "un programa se lanza por su nombre, con sus argumentos, y se le espera (también de una carpeta)" "^hola y adios$" \
  "^(ECO.BIN ha acabado con el código 3)$" "^desde una carpeta$" "^(TEXTOS.ECO2.BIN ha acabado con el código 3)$" "^(HOLA.BIN ha acabado con el código 3)$"
mira "un programa que hace lo que no debe se acaba, y el intérprete sigue; lo desconocido se dice" \
  "^\*\*\* se acaba la tarea [0-9]* (MALO.BIN)" "^(MALO.BIN ha acabado con el código 255)$" \
  "^Orden o nombre de programa desconocido: nada (escriba AYUDA)\.$" "^No puedo lanzar NOPROG.BIN: no es un programa de Hispa-Pentium\.$"
mira "la pila de un programa crece sola: el fallo de página le pone páginas según baja (y el núcleo, si va a escribir en ella)" \
  "^pila: leo 17600 octetos del disco a un arreglo local sin estrenar ([4-6] páginas más)$" \
  "^pila: 200 llamadas, una dentro de otra, con 4000 octetos cada una: 200; la pila ha crecido 19[0-9] páginas$"
mira "una pila que se desboca (una función que se llama sin fin) acaba en su tope de 4 MB: se acaba el programa, no el núcleo" \
  "^pozo: una función que se llama a sí misma sin fin\.\.\.$" "fallo de página en 0xBFC00[0-9A-F]* (escritura, anillo 3, no presente)"
mira "la memoria del programa crece y se devuelve (sbrk): páginas nuevas a cero, y vuelven" \
  "^memoria: pido 100000 octetos: 25 páginas más, a cero: cierto, y la suma de lo escrito es 312487500$" \
  "^memoria: la devuelvo: cierto, y quedan 0 páginas de más$" "^memoria: no me dan 3 GB: cierto; ni devuelvo lo que no pedí: cierto$"
mira "leer un archivo de varios clusters a trozos de 777 octetos: lo mismo que hay en el disco" \
  "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
  "^archivo: al final ya no da más: cierto$" "^archivo: cerrado, ya no se deja leer: cierto$"
mira "los archivos abiertos tienen un tope, y las direcciones que no son del programa no se tocan" \
  "^abiertos: caben 16 a la vez; el siguiente: hay demasiados archivos abiertos$" "^abiertos: cerrados todos, se puede abrir otra vez: cierto$" \
  "^punteros: leer a la memoria del núcleo: cierto; a donde no tengo nada: cierto; un nombre que no es mío: cierto$" \
  "^punteros: un manejador que no existe: cierto; esperar a una tarea que no es mía: cierto$"
mira "un programa lanza otros dos a la vez y los espera" "^soy el hijo uno$" "^y yo soy el dos$" \
  "^hijos: los dos han acabado, con los códigos 4 y 5; ya no se les puede esperar: cierto$" "^hijos: los dos ruidosos han acabado, con los códigos 9 y 9$"
ra=$(grep -c "^ruido A [0-9]\{3\}: una línea entera, de principio a fin, sin letras de otro$" $S)
rb=$(grep -c "^ruido B [0-9]\{3\}: una línea entera, de principio a fin, sin letras de otro$" $S)
rm_=$(grep -a "ruido [AB]" $S | grep -vc "^ruido [AB] [0-9]\{3\}: una línea entera, de principio a fin, sin letras de otro$")
alt=$(grep "^ruido [AB] " $S | awk '{print $2}' | uniq | wc -l)
if [ "$ra" = 120 ] && [ "$rb" = 120 ] && [ "$rm_" = 0 ] && [ "$alt" -gt 2 ]; then ok "la consola con su cerrojo: dos programas que escriben a la vez (240 líneas, $alt turnos) no se mezclan ni una letra"
else ko "las líneas de dos programas que escriben a la vez se mezclan o se pierden: $ra de A, $rb de B, $rm_ rotas, $alt turnos"; fi
mira "la memoria devuelta ya no se puede tocar: fallo de página, y se acaba el programa" "^fuera: escribo en la memoria que me han dado: cierto$" \
  "fallo de página en 0x4000[0-9A-F]*000 (escritura, anillo 3, no presente)" "^(PRUEBA.BIN ha acabado con el código 255)$"
if grep -q "no tenía que salir" $S; then ko "un programa ha seguido después de lo que no podía hacer"; else ok "lo que no podían hacer (leer el núcleo, tocar lo devuelto, salir del pozo), no lo han hecho"; fi
m1=$(grep "^ORDENES.BIN ha acabado" $S | awk '{print $10, $13}')
mira "al salir, el núcleo ha recogido lo que dejó PRUEBA sin cerrar ni esperar (un archivo abierto y un programa)" \
  "^prueba: acabo con el código 7, un archivo abierto y un programa sin esperar$" "^(PRUEBA.BIN ha acabado con el código 7)$" \
  "soy el que nadie espera: he dormido y acabo solo$" \
  "^Hasta luego\.$" "^ORDENES.BIN ha acabado con el código 0; marcos libres" "; archivos abiertos: 0$" "^montón: 2 bloques ocupados con 16384 octetos"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ] && [ "$(grep -c '^ *[0-9]*  [A-Za-z.-]*  *[a-z]*  *[03]  ' $S)" = 3 ]; then
  ok "y no queda nada: los mismos marcos libres que antes de lanzar el intérprete ($m1) y solo las tres tareas del núcleo (con cd-cambios, la del lector de CD de QEMU)"
else ko "tras el intérprete queda memoria o tareas sin recoger: «$m1»"; grep -A6 "^> tareas" $S | sed 's/^/        /'; fi
# CLS: se mira la pantalla (lo que hay justo antes de la última orden)
VER=1 DISCO=$P/d16.img $AQUI/nucleo32/probar.sh "" ordenes dir cls apagar > $P/cls.sal 2>&1
if [ "$(sed -n '/^\[pantalla\]/,$p' $P/cls.sal | sed -n '2p')" = '|C:\>' ] && [ "$(sed -n '/^\[pantalla\]/,$p' $P/cls.sal | sed '1,2d' | grep -vc '^|$')" = 0 ]; then
  ok "CLS deja la pantalla en blanco, con la petición de órdenes arriba"
else ko "CLS no borra la pantalla:"; sed -n '/^\[pantalla\]/,$p' $P/cls.sal | head -5 | sed 's/^/        /'; fi
# 3. las otras FAT, y el disco con tabla de particiones
for c in 12 32 mbr; do
  case $c in 12) que="FAT12"; f=FAT12; s0=0 ;; 32) que="FAT32"; f=FAT32; s0=0 ;; mbr) que="un disco con tabla de particiones (la FAT16, en la primera)"; f=FAT16; s0=63 ;; esac
  DISCO=$P/d$c.img $AQUI/nucleo32/probar.sh "" disco "dir textos" "lanzar prueba.bin" :espera "lanzar textos\\eco2.bin hola" apagar > $P/fat$c.sal 2>&1
  S=$P/fat$c.sal
  mira "$que: se monta, se lee una carpeta, un archivo de varios clusters y se cargan programas" \
    "^sistema de archivos: $f, desde el sector $s0, " "^LARGO.TXT  *17600$" \
    "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
    "^hijos: los dos ruidosos han acabado, con los códigos 9 y 9$" "^prueba.bin ha acabado con el código 7" "^hola$" "^eco2.bin ha acabado con el código 1"
done
# 4. el intérprete, él solo al arrancar (si el cargador dice «ordenes»); y sin disco
DISCO=$P/d16.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" ver salir apagar > $P/arranque.sal 2>&1
S=$P/arranque.sal
mira "si el cargador se lo dice (-append ordenes), el núcleo arranca con el intérprete de órdenes (y sin módulos de multiboot: todo del disco)" \
  "^memoria: 32 MB, [0-9]* marcos libres; paginación activa; 0 programas de usuario" \
  "^C:.>ver$" "^Hispa-Pentium 0\.8" "^ORDENES.BIN ha acabado con el código 0" "^\[código 5\]"
MODULOS= $AQUI/nucleo32/probar.sh "" disco dir ordenes apagar > $P/sindisco.sal 2>&1
S=$P/sindisco.sal
mira "sin disco (y sin nada que traiga el cargador), el núcleo lo dice y sigue" "^disco 0: QEMU DVD-ROM, lector de CD o DVD (IDE), sin disco$" "^sistema de archivos: no hay" "^no puedo lanzar ORDENES.BIN: no hay disco" "^\[código 5\]"

# ---- escribir en el disco: ATA, la FAT (12, 16, 32 y con particiones) y las órdenes ----
# (cada prueba escribe en una copia del disco, con ESCRIBE=1; luego la miran fsck.fat y mtools)
echo "== Hispa-Pentium escribe en el disco: ATA, FAT12/16/32, COPY, DEL, REN, MD, RD y la redirección =="
export MTOOLS_SKIP_CHECK=1
for c in 16 12 32 mbr; do
  case $c in 12) que="FAT12" ;; 16) que="FAT16" ;; 32) que="FAT32" ;; mbr) que="FAT16 en una partición" ;; esac
  I=$P/w$c.img; cp $P/d$c.img $I
  M=$I; s0=; [ $c = mbr ] && { M=$I@@32256; s0=63; }
  lleno=(); [ $c = 12 ] && lleno=("escribe lleno" :espera :espera :espera)
  ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes escribe "echo primera línea > nuevo.txt" "echo y la segunda >> nuevo.txt" "type nuevo.txt" \
    "copy textos\\largo.txt copia.txt" "md obra" "cd obra" "copy \\textos\\*.txt" "copy \\prueba.bin ." "md sub" "ren pirata.txt poema.txt" \
    "del largo.txt" "rd sub" "cd \\" "rd obra" "eco hola mundo > eco.txt" "dir obra >> eco.txt" "ren leeme.txt hola.bin" "del nada.txt" \
    "copy nada.txt x.txt" "md textos" "${lleno[@]}" salir disco apagar > $P/escribe$c.sal 2>&1
  S=$P/escribe$c.sal
  mira "$que: un programa crea un archivo, lo escribe a trozos, lo lee de vuelta, escribe en medio, lo corta, lo alarga y le añade" \
    "^escribir: un archivo nuevo, 10000 octetos a trozos de 777; mide 10000; ocupa los clusters que tocan: cierto$" \
    "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
    "^en medio: en 5000 escribo HOLA y leo HOLA; sigue midiendo 10000$" "^cortar: en 3000: 0; mide 3000; quedan libres los clusters que sobran: cierto$" \
    "^hueco: escribo en 20000: mide 20001, y lo de en medio está a cero (0 octetos que no)$" "^añadir: tres octetos al final: mide 20004 y acaba en FIN$" \
    "^crear encima: mide 0; borrado: 0; ya no está: cierto$"
  mira "$que: lo que no se puede (abrir dos veces para escribir, borrar lo abierto, nombres que no valen...) se dice" \
    "^no se puede: abrirlo otra vez para escribir: cierto; ni para leer: cierto; ni borrarlo: cierto; ni cambiarle el nombre: cierto$" \
    "^no se puede: crear en una carpeta que no está: cierto; un nombre de más de 8+3: cierto; ni con letras raras: cierto; ni el de un dispositivo: cierto; escribir en una carpeta: cierto$" \
    "^no se puede: borrar lo que no está: cierto; borrar una carpeta como archivo: cierto; abrir para escribir lo que no está: cierto$"
  mira "$que: carpetas: hacerlas, entrar, 70 archivos (la carpeta crece), mover y cambiar nombres, vaciarlas y borrarlas; todo queda como estaba" \
    "^carpetas: hago PD: 0; otra vez no: cierto; entro: 0; estoy en .PD$" "^carpetas: 70 archivos en PD: 70 hechos; la carpeta tiene 72 entradas" \
    "^carpetas: F42.TXT dice «soy el archivo 42»; y «...PD...F07.TXT» se abre: cierto$" \
    "^mover: F00.TXT a DENTRO.CERO.TXT: 0; está allí: cierto y no aquí: cierto; la carpeta pasa a llamarse FUERA: 0 y lleva lo suyo: cierto$" \
    "^no se puede: llamarlo como otro que ya está: cierto; borrar una carpeta con algo: cierto; ni la carpeta donde estoy: cierto; ni un archivo como carpeta: cierto$" \
    "^carpetas: borrados 70 archivos; FUERA: 0; PD: 0; ya no se puede entrar: cierto$" "^escribe: todo queda como estaba (los mismos clusters libres): cierto$"
  mira "$que: las órdenes dicen lo que no puede ser (y el programa redirigido, su código, por la pantalla)" \
    "^No puedo borrar la carpeta obra: la carpeta no está vacía\.$" "^No puedo llamar hola.bin a leeme.txt: ya existe\.$" "^No puedo borrar nada.txt: no existe\.$" \
    "^No puedo abrir nada.txt: no existe\.$" "^No puedo hacer la carpeta textos: ya existe\.$" "^(ECO.BIN ha acabado con el código 2)$" \
    "^ *2 archivo(s) copiado(s)\.$" "^primera línea$" "^y la segunda$" "; archivos abiertos: 0$" "^\[código 5\]"
  if [ $c = 12 ]; then
    mira "$que: el disco se llena (se escribe hasta el último cluster), se dice, y al borrar vuelve todo" \
      "^lleno: escribo hasta que no cabe: cierto; quedan 0 clusters libres; el siguiente: no queda sitio en el disco$" \
      "^lleno: ni otro archivo con algo: no queda sitio en el disco; ni una carpeta: no queda sitio en el disco$" \
      "^lleno: lo borro: 0; vuelven a estar libres los de antes: cierto$"
  fi
  # lo que ha quedado en el disco, visto desde fuera
  if sano $I $s0; then ok "$que: después de todo eso, fsck.fat da el disco por sano ($(tail -n 1 $P/fsck.txt | sed 's/^[^:]*: //'))"
  else ko "$que: fsck.fat encuentra el disco mal:"; sed 's/^/        /' $P/fsck.txt | head -12; fi
  bien=1
  saca $M NUEVO.TXT $P/x.txt && printf 'primera l\xa1nea\r\ny la segunda\r\n' | cmp -s - $P/x.txt || bien=0
  saca $M COPIA.TXT $P/x.txt && saca $P/d16.img TEXTOS/LARGO.TXT $P/y.txt && cmp -s $P/x.txt $P/y.txt || bien=0
  saca $M OBRA/POEMA.TXT $P/x.txt && saca $P/d16.img TEXTOS/PIRATA.TXT $P/y.txt && cmp -s $P/x.txt $P/y.txt || bien=0
  saca $M OBRA/PRUEBA.BIN $P/x.txt && saca $P/d16.img PRUEBA.BIN $P/y.txt && cmp -s $P/x.txt $P/y.txt || bien=0
  saca $M ECO.TXT $P/x.txt && [ "$(head -1 $P/x.txt)" = $'hola mundo\r' ] && grep -a -q '^ Directorio de C:.OBRA' $P/x.txt && grep -a -q '^POEMA    TXT  *110  ' $P/x.txt \
    && ! grep -a -q -v $'\r$' $P/x.txt || bien=0
  saca $M OBRA/LARGO.TXT $P/x.txt && bien=0
  saca $M OBRA/PIRATA.TXT $P/x.txt && bien=0
  mdir -i $M ::OBRA 2> /dev/null | grep -q '^SUB ' && bien=0
  mdir -i $M :: 2> /dev/null | grep -q -i '^PD \|^P1 \|^LLENO ' && bien=0
  if [ $bien = 1 ]; then ok "$que: y mtools ve lo mismo: lo creado con «>» y «>>» (con sus finales de línea), lo copiado (igual que el original), lo renombrado y lo borrado"
  else ko "$que: lo que hay en el disco no es lo que tenía que ser:"; mdir -i $M :: ::OBRA 2>&1 | sed 's/^/        /' | head -40; fi
  # y otra vez encendido, el núcleo lee lo que escribió
  DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes "type nuevo.txt" "dir obra" "type eco.txt" salir apagar > $P/relee$c.sal 2>&1
  S=$P/relee$c.sal
  mira "$que: al volver a arrancar, lo escrito sigue ahí" "^primera línea$" "^y la segunda$" "^POEMA    TXT  *110  " "^PRUEBA   BIN  *[0-9]*  " \
    "^ *2 archivos  *[0-9]* octetos$" "^hola mundo$" "^ Directorio de C:.OBRA$"
done

# ---- el intérprete: carpetas de trabajo, PATH, archivos de órdenes, la historia, Ctrl+C ----
echo "== Hispa-Pentium: CD y la carpeta de trabajo, PATH, archivos .BAT, la historia de la línea y Ctrl+C =="
I=$P/int.img; cp $P/d16.img $I
ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes "cd textos" cd "dir" "type pirata.txt" "eco desde textos" "cd .." "cd textos\\.." cd "cd nada" "cd leeme.txt" \
  "md uno" "cd uno" "md dos" "cd dos" "echo hondo > aqui.txt" "type \\uno\\dos\\aqui.txt" "cd..\\.." "type uno\\dos\\aqui.txt" "rd uno\\dos" \
  "del uno\\dos\\aqui.txt" "rd uno\\dos" "rd uno" path "cd textos" "path ;" path "eco sin path" "eco2 con el de aquí" "path c:\\nada;c:\\" "eco con path" "cd \\" \
  "lote uno dos" :espera ":tecla spc" visto "eco primera" "eco segunda" ":tecla up up ret" ":tecla up left left left backspace x home delete z end y ret" \
  ":tecla e c o esc v e r ret" fecha salir apagar > $P/interprete.sal 2>&1
S=$P/interprete.sal
mira "CD: la carpeta de trabajo sale en la petición de órdenes, y las rutas se entienden desde ella («..» sube)" \
  "^C:.>cd textos$" "^C:.TEXTOS>cd$" "^C:.TEXTOS$" "^ Directorio de C:.TEXTOS$" "^un velero bergantín\.$" "^C:.TEXTOS>cd \.\.$" "^C:.>cd textos...$" \
  "^No puedo entrar en nada: no existe\.$" "^No puedo entrar en leeme.txt: no es una carpeta\.$"
mira "carpetas dentro de carpetas: se crean, se entra, se escribe en ellas, y no se borran con algo dentro" \
  "^C:.UNO.DOS>echo hondo > aqui.txt$" "^C:.UNO.DOS>cd.....$" "^C:.>type uno.dos.aqui.txt$" "^No puedo borrar la carpeta uno.dos: la carpeta no está vacía\.$" \
  "^C:.>rd uno$"
if [ "$(grep -c '^hondo$' $S)" = 2 ] && ! grep -q "^No puedo borrar la carpeta uno:" $S; then ok "y la carpeta de trabajo vale igual para crear que para leer (con la ruta entera se lee lo mismo)"
else ko "lo escrito en la carpeta de trabajo no se lee por su ruta entera"; fi
mira "la carpeta de trabajo la hereda el programa que se lanza, y el PATH dice dónde se buscan los programas" \
  "^PATH=C:.$" "^desde textos$" "^(C:.ECO.BIN ha acabado con el código 2)$" "^No hay PATH\.$" "^Orden o nombre de programa desconocido: eco (escriba AYUDA)\.$" \
  "^con el de aquí$" "^(ECO2.BIN ha acabado con el código 4)$" "^con path$"
mira "un archivo de órdenes (.BAT): sus líneas, ECHO, REM, PAUSE, sus argumentos (%1), programas y redirección dentro" \
  "^C:.>lote uno dos$" "^lote: empiezo con uno y dos (y el 100%)$" "^desde el lote$" "^(ECO.BIN ha acabado con el código 3)$" "^lote: una línea$" "^lote: y otra$" \
  "^Pulse una tecla para seguir \. \. \. $" "^lote: fin$"
if ! grep -q "^C:.>ECHO lote\|REM un archivo\|^C:.>PAUSE" $S && grep -q "^C:.>REM con ECHO activado se ve cada orden$" $S && grep -q "^C:.>ECO visto$" $S \
    && grep -q "^esta no se ve$" $S && ! grep -q "ECHO esta no se ve" $S; then ok "ECHO OFF y la @ callan las órdenes del archivo; sin ellos, se ven antes de hacerse"
else ko "ECHO OFF o la @ no callan lo que deben (o callan de más)"; grep -n "lote\|visto\|REM\|se ve" $S | sed 's/^/        /' | head; fi
# la historia: lo que queda en la pantalla, quitando lo que borra cada retroceso
python3 -I $AQUI/nucleo32/pantalla.py < $S > $P/historia.txt
if [ "$(grep -c '^C:.>eco primera$' $P/historia.txt)" = 2 ] && [ "$(grep -c '^primera$' $S)" = 2 ] && grep -q '^C:.>zco prixeray$' $P/historia.txt \
    && grep -q "^Orden o nombre de programa desconocido: zco " $S && grep -q '^C:.>ver$' $P/historia.txt && grep -q "^Hispa-Pentium 0\.8" $S; then
  ok "la historia de la línea: la flecha de arriba trae las órdenes de antes; se corrigen con las flechas, Retroceso, Supr, Inicio y Fin; Esc borra la línea"
else ko "la historia o la corrección de la línea no van:"; grep -n "primera\|zco\|ver" $P/historia.txt | sed 's/^/        /' | head; fi
mira "FECHA: la fecha y la hora del reloj del PC" "^Hoy es $(date +%d/%m/%Y) y son las [0-2][0-9]:[0-5][0-9]:[0-5][0-9]\.$"
if sano $I && ! mdir -i $I :: 2> /dev/null | grep -q '^UNO \|^LOTE *TXT'; then ok "y el disco queda sano, sin las carpetas ni los archivos de paso"
else ko "tras las pruebas del intérprete el disco no queda bien:"; sed 's/^/        /' $P/fsck.txt | head -8; fi
# Ctrl+C
cp $P/d16.img $I
ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes bucle :espera ":tecla ctrl-c" "bucle duerme" ":tecla ctrl-c" "bucle tecla" ":tecla a ctrl-c" \
  sinfin :espera ":tecla ctrl-c" "bucle escribe" :espera ":tecla ctrl-c" "dir bucle.dat" ":tecla e c o ctrl-c" "echo sigo aquí" "prueba ruido Z > ruido.txt" ":tecla ctrl-c" \
  "lote a b" :espera ":tecla ctrl-c" salir tareas memoria disco "lanzar bucle.bin" :espera ":tecla ctrl-c" tareas apagar > $P/ctrlc.sal 2>&1
S=$P/ctrlc.sal
mira "Ctrl+C acaba el programa que corre: uno que da vueltas sin llamar al sistema, uno que duerme y uno que espera teclas" \
  "^bucle: doy vueltas sin fin" "^bucle: duermo sin fin" "^bucle: espero teclas sin fin" "^tecla 97$" "^C:.>bucle duerme$" "^C:.>bucle tecla$" "^C:.>sinfin$"
if [ "$(grep -c '^\^C$' $S)" -ge 7 ] && ! grep -q "ha acabado con el código 25[45])" $S; then ok "cada vez sale ^C y vuelve la petición de órdenes (sin más avisos)"
else ko "Ctrl+C no deja las cosas como debe: $(grep -c '^\^C$' $S) veces ^C"; fi
mira "Ctrl+C en un archivo de órdenes: acaba el programa y el archivo (no sigue con la línea siguiente); y en PAUSE" "^sinfin: lanzo BUCLE$" "^C:.>lote a b$" \
  "^Pulse una tecla para seguir"
if grep -q "no tenía que salir\|^lote: fin" $S; then ko "tras Ctrl+C, el archivo de órdenes ha seguido"; else ok "y lo que venía detrás en el archivo no se hace"; fi
mira "Ctrl+C sin programa en marcha: deja la línea que se estaba escribiendo, y el intérprete sigue" "^C:.>eco^C$" "^sigo aquí$"
t=$(sed -n 's/^bucle.dat es un archivo de \([0-9]*\) octetos\.$/\1/p' $S)
if [ -n "$t" ] && [ "$t" -gt 0 ] && [ $((t % 1000)) = 0 ] && sano $I && saca $I BUCLE.DAT $P/x.txt && [ "$(wc -c < $P/x.txt)" = "$t" ]; then
  ok "un programa cortado mientras escribía: su archivo queda cerrado, con lo que llevaba ($t octetos), y el disco sano"
else ko "el archivo del programa cortado no queda bien: «$t»"; sed 's/^/        /' $P/fsck.txt | head -8; fi
m1=$(grep "^ORDENES.BIN ha acabado" $S | awk '{print $10, $13}')
mira "al cortar, el núcleo lo recoge todo: ni archivos abiertos ni tareas de más; y Ctrl+C también desde la consola del núcleo" \
  "^ORDENES.BIN ha acabado con el código 0; marcos libres" "; archivos abiertos: 0$" "^montón: 2 bloques ocupados con 16384 octetos" \
  "^bucle.bin ha acabado con el código 254; marcos libres" "^\[código 5\]"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ] && [ "$(grep -c '^ *[0-9]*  [A-Za-z.-]*  *[a-z]*  *[03]  ' $S)" = 6 ]; then
  ok "y la memoria vuelve entera ($m1 marcos libres antes y después)"
else ko "tras Ctrl+C queda memoria o tareas sin recoger: «$m1»"; grep -A6 "^> tareas" $S | sed 's/^/        /'; fi

# ---- el teclado español y los nombres largos ----
echo "== Hispa-Pentium: el teclado español (acentos con tecla muerta, ñ, ç, ¿ ¡) y los nombres largos (VFAT) =="
cp $P/d16.img $I
ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes "echo ¿Dónde está el pingüino? ¡Aquí, señor! Ñandú, cigüeña, garçon, Çédille; áéíóú àèò âêô É Ü ï > letras.txt" \
  "echo 1º 2ª [x] {y} @ # | \\ < ; : _ - + * = % & ( ) \" ' ^ \` ~ ! ? / . , \$ ÁÍÓÚ > signos.txt" "type letras.txt" teclado "teclado us" ":tecla e c h o spc bracket_left bracket_right semicolon apostrophe backslash slash ret" "teclado es" \
  ":tecla caps_lock" "echo bloqueo con ñ y ç" ":tecla caps_lock" "dir largos" "type largos\\cancion_del_pirata.txt" "type largos\\AÑO" \
  "copy largos\\cancion_del_pirata.txt cancion.txt" "cd largos" "ren cancion_del_pirata.txt pirata.txt" "del añonue~1.txt" "dir" salir teclas apagar > $P/teclado.sal 2>&1
S=$P/teclado.sal
bien=1
saca $I LETRAS.TXT $P/x.txt && printf '¿Dónde está el pingüino? ¡Aquí, señor! Ñandú, cigüeña, garçon, Çédille; áéíóú àèò âêô É Ü ï\r\n' | iconv -f UTF-8 -t CP437 | cmp -s - $P/x.txt || bien=0
saca $I SIGNOS.TXT $P/y.txt && printf '%s\r\n' "1º 2ª [x] {y} @ # | \\ < ; : _ - + * = % & ( ) \" ' ^ \` ~ ! ? / . , \$ AIOU" | iconv -f UTF-8 -t CP437 | cmp -s - $P/y.txt || bien=0
if [ $bien = 1 ]; then ok "el teclado español: ñ, ç, ¿ ¡, º ª, los acentos con su tecla muerta (´ \` ^ ¨), Alt Gr ([ ] { } @ # | \\ ~) y los signos, octeto a octeto en la página 437"
else ko "el teclado español no da las letras que debe:"; od -c $P/x.txt | head -8 | sed 's/^/        /'; od -c $P/y.txt | head -6 | sed 's/^/        /'; fi
mira "lo tecleado se ve en la pantalla; TECLADO cambia al de EE. UU. y vuelve; Bloq Mayús" "^¿Dónde está el pingüino? ¡Aquí, señor! Ñandú, cigüeña, garçon, Çédille; áéíóú àèò âêô É Ü ï$" \
  "^Teclado: español (ES)\.$" "^Teclado: de EE\. UU\. (US)\.$" "^\[\];'./$" "^BLOQUEO CON Ñ Y Ç$"
mira "los nombres largos (VFAT) se leen de las carpetas, con sus letras de fuera del ASCII, y se abre por ellos" \
  "^CANCIO~1 TXT  *57  [0-9/]*  [0-9:]*  cancion_del_pirata.txt$" "^AÑONUE~1 TXT  *18  [0-9/]*  [0-9:]*  Año nuevo.txt$" "^que es mi dios la libertad\.$" \
  "^No puedo abrir largos.AÑO: no existe\.$" "^ *1 archivo(s) copiado(s)\.$" "^PIRATA   TXT  *57  [0-9/]*  [0-9:]*$"
if sano $I && saca $I CANCION.TXT $P/x.txt && [ "$(wc -c < $P/x.txt)" = 57 ] && saca $I LARGOS/PIRATA.TXT $P/y.txt && cmp -s $P/x.txt $P/y.txt \
    && ! mdir -i $I ::LARGOS 2> /dev/null | grep -q -i 'cancion_del\|nuevo'; then
  ok "al cambiar el nombre o borrar un archivo con nombre largo, sus trozos se van con él (fsck.fat no ve nombres largos sueltos)"
else ko "los nombres largos no quedan bien al renombrar o borrar:"; sed 's/^/        /' $P/fsck.txt | head -8; mdir -i $I ::LARGOS 2>&1 | sed 's/^/        /'; fi
fi

# ---- sin disco (la unidad en memoria), el permiso de escritura y el escritorio ----
echo "== Hispa-Pentium: sin disco, los módulos del cargador hacen de unidad M:; en el disco no se escribe sin permiso =="
G=$P/graf
mkdir -p $G $G/mod
for p in uordenes:ORDENES uhola:HOLA ueco:ECO; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${p%%:*}.cas -o $G/mod/x.asm > /dev/null 2>&1 && nasm -f bin $G/mod/x.asm -o $G/mod/${p##*:}.BIN
done
printf 'un texto que ha venido con el cargador\r\n' > $G/mod/LEEME.TXT
MODULOS="$G/mod/ORDENES.BIN,$G/mod/HOLA.BIN,$G/mod/ECO.BIN,$G/mod/LEEME.TXT" ARRANQUE=ordenes $AQUI/nucleo32/probar.sh "" dir "type leeme.txt" "eco uno dos" hola \
  "echo x > a.txt" "del leeme.txt" "md c" "cd nada" nada salir "lanzar eco.bin desde la consola" apagar > $G/mem.sal 2>&1
S=$G/mem.sal
mira "sin disco, lo que trae el cargador (los módulos de multiboot) hace de unidad M: en memoria: «ordenes» lanza el ORDENES.BIN que venga" \
  "^sin disco: los 4 archivos que ha traído el cargador hacen de unidad M: (en memoria, de solo lectura)" "^Hispa-Pentium: intérprete de órdenes"
mira "la unidad en memoria: DIR enseña los módulos, TYPE los lee y los programas se lanzan por su nombre" \
  "^ORDENES  BIN  *[0-9]* " "^LEEME    TXT  *40 " "^ *4 archivos  *[0-9]* octetos$" "^un texto que ha venido con el cargador$" "^uno dos$" \
  "^1 + 2 + ... + 1000 = 500500" "^(HOLA.BIN ha acabado con el código 3)" "^Orden o nombre de programa desconocido: nada "
mira "la unidad en memoria es de solo lectura: ni crear, ni borrar, ni carpetas (y se dice)" "^No puedo escribir en a.txt: es de solo lectura\.$" \
  "^No puedo borrar leeme.txt: es de solo lectura\.$" "^No puedo hacer la carpeta c: es de solo lectura\.$"
mira "y desde la consola del núcleo también se lanzan por su nombre" "^desde la consola$" \
  "^ECO.BIN ha acabado con el código 3; marcos libres" "^\[código 5\]"
m1=$(grep "^ORDENES.BIN ha acabado" $S | awk '{print $10, $13}')
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ]; then ok "sin disco, al salir del intérprete quedan los mismos marcos libres ($m1)"
else ko "sin disco, el intérprete se deja marcos: «$m1»"; fi
# el disco, sin permiso para escribir (como en un PC de verdad: hay que decir «escritura»)
cp $P/d16.img $G/sl.img
SOLO_LECTURA=1 ESCRIBE=1 DISCO=$G/sl.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "echo hola > nuevo.txt" "del leeme.txt" "md cosas" \
  "ren leeme.txt otro.txt" "copy leeme.txt copia.txt" "type leeme.txt" hola salir apagar > $G/sl.sal 2>&1
S=$G/sl.sal
mira "sin «escritura» en la línea del cargador, el disco es de solo lectura: se lee y se lanzan sus programas, pero no se crea, borra ni cambia nada" \
  "^el disco es de solo lectura (para escribir en él: «escritura» en la línea del cargador)" "^No puedo escribir en nuevo.txt: es de solo lectura\.$" \
  "^No puedo borrar leeme.txt: es de solo lectura\.$" "^No puedo hacer la carpeta cosas: es de solo lectura\.$" \
  "^Este archivo está en un disco FAT" "^1 + 2 + ... + 1000 = 500500" "^\[código 5\]"
if cmp -s $P/d16.img $G/sl.img; then ok "y la imagen del disco queda idéntica, octeto a octeto: no se ha escrito ni un sector"
else ko "sin permiso de escritura, el disco ha cambiado"; fi
DISCO=$P/d16.img ARRANQUE="ordenes sindisco" MODULOS="$G/mod/ORDENES.BIN,$G/mod/ECO.BIN" $AQUI/nucleo32/probar.sh "" dir salir disco apagar > $G/sin.sal 2>&1
S=$G/sin.sal
if ! grep -q "^disco 0: QEMU" $S && grep -q "^disco 0: no hay" $S && grep -q "^ *2 archivos " $S && grep -q "^sin disco: los 2 archivos" $S; then
  ok "con «sindisco» el núcleo ni mira los discos del PC: la unidad es la de los módulos"
else ko "«sindisco» no deja los discos sin mirar:"; sed 's/^/        /' $S | head -12; fi

echo "== Hispa-Pentium: el escritorio (modo gráfico, ratón, ventanas, barra de tareas, menú Inicio, iconos, Bloc de notas, Consola y Mi equipo) =="
cp $P/d16.img $G/e.img
hora1=$(date +%H:%M); hora2=$(date -d "+1 minute" +%H:%M)
CAPTURAS=$G ESCRIBE=1 DISCO=$G/e.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":captura e1" \
  ":clic 40 585" ":captura e2" ":clic 80 176" ":pausa 2" ":escribe Querida señora: ¿qué tal está?" ":tecla ret" ":escribe Adiós." ":captura e3" \
  ":tecla f2" ":pausa 1" ":captura e4" ":arrastra 300 75 400 175" ":raton 600 450" ":captura e5" ":doble 400 175" ":pausa 1" ":captura e6" \
  ":doble 400 14" ":pausa 1" ":clic 630 176" ":captura e7" ":clic 150 585" ":captura e8" \
  ":clic 40 585" ":clic 80 216" ":pausa 3" "eco hola desde la ventana" "bucle" ":pausa 1" ":tecla ctrl-c" ":pausa 1" "ver" ":captura e9" \
  "salir" ":pausa 1" ":captura e10" ":clic 40 585" ":clic 300 238" ":raton 700 470" ":captura e11" ":clic 550 340" ":pausa 2" ":captura e12" ":clic 570 399" \
  ":clic 678 176" ":pausa 1" ":captura e13" ":clic 40 585" ":clic 360 545" ":captura e14" ":clic 275 327" > $G/escritorio.sal 2>&1
S=$G/escritorio.sal
mira "«escritorio» en la línea del cargador: el núcleo pone el modo gráfico (VBE de Bochs/QEMU, 800x600 con 32 bits) y arranca el escritorio" \
  "^escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha puesto el núcleo, por la VBE de Bochs/QEMU), tapiz 0"
miracap "el escritorio: el tapiz con el disquete y su nombre, la barra de tareas con «Inicio» y el reloj en la bandeja" e1 tam 800 600 \
  negrita 30 570 46 580 Inicio negrita 630 505 645 520 Hispa-Pentium rgb 400 2 154C9C rgb 300 598 1D5DBA 10 rgb 795 597 3393E0 12
if cap $G/e1.ppm negrita 740 570 752 580 $hora1 || cap $G/e1.ppm negrita 740 570 752 580 $hora2; then ok "el reloj de la bandeja da la hora del PC (la de la CMOS: $hora1)"
else ko "el reloj de la bandeja no da la hora ($hora1)"; fi
miracap "la flecha del ratón está pintada donde está el ratón (y solo ahí)" e2 rgb 40 585 10233F rgb 42 589 FFFFFF nrgb 400 300 10233F
miracap "el menú Inicio (MENÚ): los fijados a la izquierda, «Todos los programas» abajo, Mis documentos y Configuración a la derecha, y Cerrar sesión, Reiniciar y Apagar equipo en el pie" e2 \
  negrita 54 104 62 112 Hispa-Pentium negrita 46 155 54 163 "Bloc de notas" negrita 46 195 54 203 Consola negrita 58 492 66 500 "Todos los programas" \
  negrita 256 159 264 167 "Mis documentos" negrita 256 226 264 234 "Configuración" negrita 68 533 76 541 "Cerrar sesión" negrita 210 533 222 541 Reiniciar \
  negrita 320 533 328 541 "Apagar equipo" rgb 100 400 FFFFFF rgb 300 450 DCEAFB
mira "el menú lanza el Bloc de notas (un programa del anillo 3, del disco)" "escritorio: lanzo .BLOC.BIN (la tarea [0-9]*)"
miracap "el Bloc de notas en su ventana: el título con su nombre, su botón en la barra de tareas, y lo tecleado (con eñe, acentos y ¿?) dentro" e3 \
  negrita 112 64 124 72 "NOTAS.TXT - Bloc de notas" texto 130 572 140 580 "NOTAS.TXT - Blo" texto 94 146 102 156 "Querida señora: ¿qué tal está?" \
  texto 94 162 102 172 "Adiós." negrita 110 116 130 126 Nuevo negrita 190 116 220 126 Guardar texto 100 88 108 98 Archivo texto 168 88 178 98 "Edición" \
  notexto 0 180 330 560 Programas
mira "F2 guarda el texto en el disco (lo dice el propio programa)" "^bloc: guardado NOTAS.TXT, 38 octetos"
miracap "y lo dice abajo, en la ventana" e4 texto 94 428 104 448 "Guardado en NOTAS.TXT (38 octetos)."
miracap "arrastrar la ventana por su título la mueve (y donde estaba queda el tapiz)" e5 negrita 212 164 224 172 "NOTAS.TXT - Bloc de notas" \
  texto 194 246 202 256 "Querida señora: ¿qué tal está?" notexto 100 60 140 80 "NOTAS.TXT - Bloc de notas"
miracap "dos clics en el título la maximizan (de lado a lado, hasta la barra de tareas), y el programa vuelve a pintar lo suyo" e6 \
  negrita 24 2 36 10 "NOTAS.TXT - Bloc de notas" texto 6 84 14 94 "Querida señora: ¿qué tal está?" rgb 797 300 1A5AB6 negrita 30 570 46 580 Inicio
miracap "otros dos la devuelven a su tamaño, y el botón de minimizar la recoge en la barra de tareas" e7 notexto 150 150 300 180 "NOTAS.TXT - Bloc de notas" \
  notexto 0 0 100 20 "NOTAS.TXT - Bloc de notas" texto 130 572 140 580 "NOTAS.TXT - Blo"
miracap "su botón de la barra de tareas la recupera, con lo que tenía" e8 negrita 212 164 224 172 "NOTAS.TXT - Bloc de notas" texto 194 262 202 272 "Adiós."
mira "el menú lanza la Consola" "^escritorio: lanzo .CONSOLA.BIN (la tarea [0-9]*)"
miracap "la Consola: el intérprete de órdenes dentro de una ventana (un terminal): órdenes, un programa, Ctrl+C y la versión" e9 \
  negrita 140 90 152 98 Consola texto 120 116 132 520 "C:\\>eco hola desde la ventana" texto 120 116 132 520 "hola desde la ventana" \
  texto 120 116 132 520 "bucle: doy vueltas sin fin (Ctrl+C para acabar)" texto 120 116 132 520 "^C" \
  texto 120 116 132 520 "Hispa-Pentium 0.8: un núcleo de 32 bits escrito en Castel." texto 290 572 300 580 Consola
miracap "SALIR acaba el intérprete y la Consola se cierra (su ventana y su botón)" e10 notexto 130 80 160 100 Consola notexto 280 570 310 582 Consola \
  negrita 212 164 224 172 "NOTAS.TXT - Bloc de notas"
miracap "Configuración: lo que hay (pantalla, memoria), el teclado y el tapiz" e11 negrita 196 126 206 136 "Configuración" \
  negrita 240 160 256 170 "Hispa-Pentium 0.8" texto 190 224 200 234 "Pantalla:  800 x 600, 32 bits por punto" negrita 290 280 330 300 "Español" rgb 10 10 154C9C
miracap "el tapiz se cambia desde Configuración (a «Verde»), y se repinta todo el escritorio" e12 rgb 10 10 1F6B52 negrita 30 570 46 580 Inicio \
  negrita 196 126 206 136 "Configuración"
miracap "el botón de cerrar acaba el Bloc de notas (sin cambios pendientes): ni ventana ni botón en la barra" e13 \
  notexto 180 150 260 180 "NOTAS.TXT - Bloc de notas" notexto 100 570 200 582 "NOTAS.TXT - Blo" notexto 180 120 220 140 "Configuración"
miracap "«Apagar el equipo»: el cuadro con Apagar, Reiniciar, Cerrar sesión y Cancelar (MENÚ)" e14 negrita 170 190 180 200 "Apagar el equipo" \
  negrita 196 315 204 323 Apagar negrita 304 315 312 323 Reiniciar negrita 548 315 556 323 Cancelar
mira "«Apagar» apaga (el núcleo acaba con su código)" "^escritorio: apagar" "^\[código 5\]"
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda la sesión del escritorio, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado durante la sesión del escritorio:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
fsck.fat -n $G/e.img > $G/fsck.txt 2>&1
MTOOLS_SKIP_CHECK=1 mtype -i $G/e.img ::NOTAS.TXT > $G/notas.bin 2> /dev/null
printf 'Querida se\xa4ora: \xa8qu\x82 tal est\xa0?\r\nAdi\xa2s.' > $G/notas.esp
if cmp -s $G/notas.bin $G/notas.esp && ! grep -qi "differ\|error\|unused\|orphan" $G/fsck.txt; then
  ok "NOTAS.TXT está en el disco con lo tecleado, octeto a octeto (página 437, finales de línea del DOS), y fsck.fat da el disco por sano"
else ko "lo que guarda el Bloc de notas no está bien en el disco:"; od -c $G/notas.bin | head -4 | sed 's/^/        /'; head -5 $G/fsck.txt | sed 's/^/        /'; fi
# ---- la 0.5: el botón secundario y sus menús, seleccionar, el portapapeles, los iconos, cambiar el tamaño ----
echo "== Hispa-Pentium 0.5: los menús del botón secundario, seleccionar texto, cortar, copiar y pegar, los iconos, las ventanas que se estiran y Mi equipo =="
RES=2F74D0     # el azul de lo seleccionado
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":clicd 400 300" ":captura a1" ":clic 440 345" ":pausa 1" ":captura a2" \
  ":doble 60 110" ":pausa 2" ":escribe Hola mundo cruel" ":tecla ret" ":escribe segunda linea" ":doble 150 158" ":raton 300 300" ":captura a3" ":clicd 155 158" ":captura a4" ":clic 175 175" \
  ":captura a5" ":tecla end" ":tecla ctrl-v" ":tecla shift-left" ":tecla shift-left" ":tecla shift-left" ":captura a6" ":tecla ctrl-c" ":tecla right" ":tecla ctrl-v" \
  ":triple 150 174" ":captura a7" ":escribe X" ":clic 98 158" ":mayusclic 138 158" ":captura a8" ":tecla delete" ":arrastra 98 158 122 160" ":clic 200 100" ":captura a9" ":clic 220 152" \
  ":arrastra 595 455 695 505" ":pausa 1" ":captura a10" ":clic 40 585" ":clicd 80 216" ":captura a11" ":clic 100 228" ":pausa 3" ":clicd 400 400" ":captura a12" ":clic 420 440" \
  ":pausa 1" ":captura a13" > $G/menus.sal 2>&1
S=$G/menus.sal
miracap "los iconos del tapiz (Mi equipo, Bloc de notas, Consola), con su rótulo en letra normal" a1 texto 24 50 32 58 "Mi equipo" texto 8 126 16 134 "Bloc de notas" \
  texto 32 202 40 210 Consola
miracap "el botón secundario en el tapiz saca su menú: Configuración, Cambiar el tapiz, Organizar iconos y Acerca de Hispa-Pentium" a1 texto 410 304 418 314 "Configuración" \
  texto 410 328 418 338 "Cambiar el tapiz" texto 410 352 418 362 "Organizar iconos" texto 410 385 418 395 "Acerca de Hispa-Pentium" rgb 500 302 FFFFFF 12
miracap "«Cambiar el tapiz» pasa al siguiente (de «Olas» a «Azul»), y el menú se cierra" a2 rgb 300 4 16468E 8 notexto 400 300 430 400 "Organizar iconos" \
  texto 24 50 32 58 "Mi equipo"
mira "dos clics en el icono «Bloc de notas» lo abren" "escritorio: lanzo .BLOC.BIN (la tarea [0-9]*)"
miracap "en el Bloc de notas, dos clics seleccionan la palabra, que se ve en blanco sobre azul" a3 texto 134 146 142 154 mundo rgb 139 151 $RES rgb 176 164 $RES \
  nrgb 135 151 $RES nrgb 181 151 $RES texto 94 162 102 170 "segunda linea"
miracap "el botón secundario en el Bloc saca Cortar, Copiar, Pegar y Seleccionar todo" a4 texto 165 163 173 171 Cortar texto 165 187 173 195 Copiar texto 165 211 173 219 Pegar \
  texto 165 244 173 252 "Seleccionar todo"
miracap "«Cortar» quita lo seleccionado" a5 texto 94 146 102 154 "Hola  cruel" notexto 90 140 200 160 mundo
miracap "Ctrl+V lo pega donde está el cursor, y Mayús con la flecha selecciona hacia atrás, letra a letra" a6 texto 94 146 102 154 "Hola  cruelmu" rgb 203 151 $RES \
  rgb 225 164 $RES nrgb 199 151 $RES
miracap "Ctrl+C copia lo seleccionado y Ctrl+V lo pega; tres clics seleccionan la línea entera" a7 texto 94 146 102 154 "Hola  cruelmundondo" rgb 99 167 $RES rgb 200 180 $RES \
  nrgb 99 151 $RES
miracap "lo que se teclea sustituye a lo seleccionado; y un clic y Mayús con otro clic seleccionan de uno a otro" a8 texto 94 162 102 170 X notexto 90 160 200 176 segunda \
  rgb 99 151 $RES rgb 136 164 $RES nrgb 141 151 $RES
miracap "la barra de menús del Bloc (Archivo, Edición, Ayuda): «Edición» abre Cortar, Copiar, Pegar y Seleccionar todo, con sus teclas" a9 \
  texto 178 116 186 124 "Cortar            Ctrl+X" texto 178 140 186 148 "Copiar            Ctrl+C" texto 178 197 186 205 "Seleccionar todo  Ctrl+A" rgb 99 151 $RES
miracap "arrastrar la esquina de la ventana la cambia de tamaño, y el programa vuelve a pintar lo suyo (el pie, más abajo; el papel, más ancho)" a10 \
  texto 94 480 102 488 "Copiado (3 letras)." texto 118 146 126 154 uelmundondo rgb 650 300 FFFFFF rgb 693 300 1A5AB6 10 notexto 90 430 110 446 NOTAS.TXT
miracap "el botón secundario en un programa del menú Inicio: «Abrir» y «Quitar del menú Inicio» (MENÚ)" a11 texto 90 220 98 228 Abrir texto 90 244 98 252 "Quitar del menú Inicio" negrita 46 155 54 163 "Bloc de notas"
mira "y «Abrir» lo lanza" "^escritorio: lanzo .CONSOLA.BIN (la tarea [0-9]*)"
miracap "el botón secundario en la Consola: Copiar (en gris: no hay nada seleccionado) y Pegar" a12 texto 410 405 418 413 Copiar texto 410 429 418 437 Pegar
miracap "el portapapeles es del sistema: lo copiado en el Bloc de notas (con el menú Edición) se pega en la Consola" a13 texto 120 140 132 300 "C:\\> cr"
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda esa sesión, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# la Consola: seleccionar y copiar; el menú de una ventana (en el título y en la barra); Mi equipo; Acerca de
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":doble 60 186" ":pausa 4" ":doble 150 140" ":raton 400 300" ":captura b1" ":tecla ctrl-c" \
  ":clicd 300 75" ":captura b2" ":clic 320 188" ":pausa 1" ":captura b3" ":clicd 180 585" ":captura b4" ":clic 200 421" ":pausa 1" ":captura b5" ":arrastra 743 523 643 423" \
  ":pausa 1" ":captura b6" ":clic 40 585" ":clic 80 176" ":pausa 2" ":tecla ctrl-v" ":captura b7" ":clicd 300 75" ":clic 320 221" ":pausa 1" ":captura b8" ":doble 60 40" ":pausa 2" \
  ":captura b9" ":doble 330 160" ":pausa 1" ":raton 600 440" ":captura b10" ":doble 356 310" ":pausa 2" ":captura b11" ":clicd 150 540" ":clic 200 523" ":pausa 1" ":captura b12" > $G/menus2.sal 2>&1
S=$G/menus2.sal
mira "dos clics en el icono «Consola» la abren" "escritorio: lanzo .CONSOLA.BIN (la tarea [0-9]*)"
miracap "en la Consola, dos clics seleccionan una palabra de lo que hay en la pantalla" b1 rgb 97 133 $RES rgb 206 146 $RES nrgb 213 133 $RES texto 92 128 100 136 "Hispa-Pentium:"
miracap "el botón secundario en el título de una ventana: Restaurar, Mover, Tamaño (MENÚ), Minimizar, Maximizar y Cerrar" b2 texto 310 80 318 88 Restaurar texto 310 104 318 112 Mover \
  texto 310 128 318 136 "Tamaño" texto 310 152 318 160 Minimizar texto 310 176 318 184 Maximizar texto 310 209 318 217 Cerrar
miracap "«Maximizar», desde ese menú, la maximiza" b3 negrita 24 2 36 10 Consola rgb 797 300 1A5AB6 notexto 300 70 330 200 Minimizar
miracap "el botón secundario en su botón de la barra de tareas saca el mismo menú (hacia arriba)" b4 texto 190 409 198 417 Restaurar texto 190 542 198 550 Cerrar
miracap "y «Restaurar» la devuelve a su tamaño" b5 negrita 112 64 124 72 Consola rgb 780 300 154C9C 60 nrgb 780 300 0C1626
miracap "la Consola también cambia de tamaño por la esquina (su pantalla sigue siendo de 80 x 25: lo que no cabe no se ve)" b6 rgb 600 300 0C1626 nrgb 700 300 0C1626 \
  nrgb 400 480 0C1626 negrita 112 64 124 72 Consola
miracap "lo copiado en la Consola (Ctrl+C con algo seleccionado) se pega en el Bloc de notas" b7 texto 122 172 130 180 "Hispa-Pentium:"
miracap "«Cerrar», en el menú de la ventana, la cierra: ni ventana ni botón en la barra" b8 notexto 100 60 200 80 Consola notexto 120 570 300 582 Consola \
  negrita 140 90 152 98 "NOTAS.TXT - Bloc de notas"
miracap "dos clics en «Mi equipo»: el explorador, con la unidad C: (el disco FAT16) en su mosaico y lo que tiene libre" b9 negrita 30 10 40 20 "Mi equipo" \
  negrita 265 133 275 143 "HISPA (C:)" texto 313 151 323 161 "libres de"
miracap "dos clics en la unidad: sus archivos y sus carpetas" b10 negrita 30 10 40 20 "C:\\" texto 316 322 326 332 LEEME.TXT texto 428 166 438 176 VACIA
miracap "y dos clics en un texto lo abren con el Bloc de notas" b11 negrita 198 142 206 150 "\\LEEME.TXT - Bloc de notas" \
  texto 178 224 186 232 "Hispa-Pentium: un núcleo de 32 bits escrito en Castel."
miracap "«Acerca de Hispa-Pentium», desde el menú del tapiz: la versión y la licencia" b12 negrita 206 158 214 166 "Acerca de Hispa-Pentium" negrita 276 196 284 204 "Hispa-Pentium 0.8" \
  texto 196 294 204 302 "Copyright (C) 2026 Carlos Sanz y contribuidores."
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda esa sesión, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# a la medida de un netbook (1024 x 600); llevar un icono a otro sitio y organizarlos; mover una ventana desde su menú
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE="escritorio modo=1024x600" PANTALLA=1024x600 MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":captura c1" ":arrastra 60 190 500 300" \
  ":raton 700 100" ":captura c2" ":clicd 600 200" ":clic 620 265" ":captura c3" ":clicd 600 200" ":clic 620 215" ":pausa 1" ":captura c4" ":clicd 400 137" ":clic 420 173" \
  ":raton 460 233" ":clic 460 233" ":raton 700 400" ":captura c5" > $G/medida.sal 2>&1
S=$G/medida.sal
mira "con «modo=1024x600» en la línea del cargador, el escritorio arranca a esa medida" "^escritorio: en marcha, 1024 x 600 con 32 bits por punto"
miracap "a 1024 x 600 (la pantalla de un netbook): la barra de tareas abajo, de lado a lado, el reloj a la derecha y los iconos" c1 tam 1024 600 negrita 30 570 46 580 Inicio \
  rgb 1019 597 3393E0 12 rgb 600 598 1D5DBA 10 texto 24 50 32 58 "Mi equipo" negrita 854 505 869 520 Hispa-Pentium
miracap "un icono del tapiz se lleva a otro sitio arrastrándolo" c2 texto 472 312 480 320 Consola notexto 20 196 60 214 Consola texto 24 50 32 58 "Mi equipo"
miracap "«Organizar iconos» los devuelve a su sitio" c3 texto 32 202 40 210 Consola notexto 460 300 500 330 Consola
miracap "Configuración dice la medida de la pantalla" c4 texto 302 225 310 233 "Pantalla:  1024 x 600, 32 bits por punto"
miracap "«Mover», en el menú de una ventana: la flecha va a su título y la ventana la sigue hasta el clic siguiente" c5 negrita 348 185 356 193 "Configuración" \
  notexto 300 120 330 140 "Configuración"
# (explorador, 21/11) el explorador de archivos (EQUIPO.BIN: el árbol, las
# vistas, las miniaturas con MINIAT.BIN, copiar y pegar, cambiar el nombre,
# carpeta nueva, la Papelera, el menú del botón secundario, elegir varios,
# buscar) y el dock (soltar en el tapiz y en el dock, el punto de los
# abiertos, su menú, ocultarlo); el disco lo miran luego mtools y fsck.fat
echo "== Hispa-Pentium: el explorador de archivos y el dock =="
cp $P/d16.img $G/ex.img
export MTOOLS_SKIP_CHECK=1
mmd -i $G/ex.img ::FOTOS
python3 - $G <<'PY'
import struct, sys, zlib, base64
g = sys.argv[1]
def png(w, h, f):
    raw = b''.join(b'\0' + b''.join(bytes(f(x, y)) for x in range(w)) for y in range(h))
    ch = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    return b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b'')
open(g + '/FLOR.PNG', 'wb').write(png(160, 120, lambda x, y: (255, 122, 51) if (x - 80) ** 2 + (y - 60) ** 2 < 35 ** 2 else (47, 116, 208)))
w, h = 64, 96
filas = b''.join(bytes((255, 255, 255) if 10 <= x < 54 and 56 <= y < 86 else (0x4A, 0x29, 0x14)) * 1 for y in range(h) for x in range(w))
bmp = b'BM' + struct.pack('<IHHI', 54 + len(filas), 0, 0, 54) + struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(filas), 2835, 2835, 0, 0) + filas
open(g + '/CASA.BMP', 'wb').write(bmp)
open(g + '/PAISAJE.JPG', 'wb').write(base64.b64decode('/9j/4AAQSkZJRgABAQAAAAAAAAD/2wBDAAYEBQYFBAYGBQYHBwYIChAKCgkJChQODwwQFxQYGBcUFhYaHSUfGhsjHBYWICwgIyYnKSopGR8tMC0oMCUoKSj/2wBDAQcHBwoIChMKChMoGhYaKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCj/wAARCAAgADADASIAAhEBAxEB/8QAFgABAQEAAAAAAAAAAAAAAAAAAAYH/8QAFhAAAwAAAAAAAAAAAAAAAAAAABRh/8QAFwEBAQEBAAAAAAAAAAAAAAAAAAYFB//EABcRAQEBAQAAAAAAAAAAAAAAAAATFGH/2gAMAwEAAhEDEQA/ANQfofpKP0P04rgWkVW/Q/SUfofowEVW/Q/SUfofowEUo/Q/SUfofpa4ONWKrfofpKP0P0YOEVW/Q/SUfofowcIv/9k='))
PY
mcopy -i $G/ex.img $G/CASA.BMP $G/FLOR.PNG $G/PAISAJE.JPG ::FOTOS/
miniat=no
if bash $AQUI/nucleo32/miniat/hacer.sh $G/MINIAT.BIN > $G/miniat.txt 2>&1; then mcopy -i $G/ex.img $G/MINIAT.BIN ::; miniat=si; fi
SES=(":pausa 2" ":captura ex1" ":doble 60 40" ":pausa 3" ":captura ex2" ":doble 330 160" ":pausa 2" ":raton 600 440" ":captura ex3"
 ":tecla alt-v" ":tecla down down down down ret" ":pausa 1" ":captura ex4"
 ":tecla ret" ":pausa 1" ":tecla alt-v" ":tecla down ret" ":pausa 6" ":captura ex5"
 ":tecla alt-v" ":tecla down down ret" ":tecla backspace" ":pausa 1"
 ":clic 452 310" ":tecla ctrl-c" ":doble 548 155" ":pausa 1" ":tecla ctrl-v" ":pausa 2" ":raton 600 440" ":captura ex6"
 ":clic 260 160" ":tecla f2" ":pausa 1" ":captura ex7" ":escribe NUEVO.TXT" ":tecla ret" ":pausa 1" ":tecla f7" ":pausa 1" ":captura ex8"
 ":tecla ret" ":pausa 1" ":raton 600 440" ":captura ex9" ":clic 260 160" ":tecla delete" ":pausa 2" ":raton 600 440" ":captura ex10"
 ":clic 84 280" ":pausa 2" ":raton 600 440" ":captura ex11" ":clic 260 160" ":tecla alt-h" ":tecla down down ret" ":pausa 2" ":captura ex12"
 ":tecla f4" ":escribe C:\\" ":tecla ret" ":pausa 1" ":clicd 452 310" ":pausa 1" ":captura ex13" ":tecla esc"
 ":clic 260 260" ":ctrlclic 356 260" ":pausa 1" ":captura ex14" ":arrastra 600 455 380 290" ":pausa 1" ":captura ex15"
 ":clic 452 310" ":arrastra 452 310 120 540" ":pausa 2" ":clic 260 410" ":arrastra 260 410 400 535" ":pausa 2" ":captura ex16"
 ":tecla f3" ":escribe pir" ":tecla ret" ":pausa 3" ":captura ex17" ":doble 100 525" ":pausa 3" ":captura ex18" ":arrastra 292 530 400 530" ":pausa 1"
 ":clicd 320 530" ":pausa 1" ":captura ex19" ":clic 380 513" ":pausa 1" ":captura ex20")
CAPTURAS=$G ESCRIBE=1 DISCO=$G/ex.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" "${SES[@]}" > $G/explorador.sal 2>&1
S=$G/explorador.sal
miracap "el dock: centrado abajo, encima de la barra de tareas, con el borde del panel" ex1 rgb 400 504 0F3F8A 20 negrita 30 570 46 580 Inicio
miracap "«Mi equipo» es el explorador: el árbol (Mi equipo), la unidad C: en su mosaico, con lo libre, y la Papelera" ex2 negrita 30 10 40 20 "Mi equipo" \
  negrita 265 133 275 143 "HISPA (C:)" negrita 515 133 525 143 "CD (D:)" texto 515 151 525 161 "sin disco" negrita 265 195 275 205 Papelera texto 313 151 323 161 "libres de" texto 48 127 58 137 "Mi equipo"
miracap "dos clics en la unidad: sus carpetas y sus archivos, en iconos, y lo libre en la barra de estado" ex3 texto 236 166 246 176 FOTOS \
  texto 412 322 422 332 LEEME.TXT texto 687 475 697 485 "Libre:"
miracap "la vista de detalles: la rejilla de los controles, con su nombre, su tamaño, su tipo y su fecha (las carpetas, delante)" ex4 \
  negrita 221 127 231 137 Nombre negrita 629 127 639 137 Modificado texto 477 177 487 187 Carpeta texto 477 249 487 259 Programa
if [ $miniat = si ]; then
  miracap "las miniaturas (MINIAT.BIN, con stb_image): un BMP, un PNG y un JPG en pequeño, con su nombre" ex5 texto 344 204 354 214 FLOR.PNG \
    texto 444 204 454 214 PAISAJE.JPG rgb 381 170 FF7A33 24 rgb 270 150 FFFFFF 10 nrgb 493 170 FFFFFF 30
else ko "no se ha podido hacer MINIAT.BIN (¿gcc con -m32?): $(tail -n 1 $G/miniat.txt)"; fi
miracap "copiar (Ctrl+C) en una carpeta y pegar (Ctrl+V) en otra: el archivo, y «Hecho» en la barra de estado" ex6 texto 220 166 230 176 LEEME.TXT \
  texto 159 475 169 485 "copiados 1"
miracap "F2: el cuadro para cambiar el nombre, con el que tiene" ex7 negrita 187 170 197 180 Cambiar texto 194 225 204 235 LEEME.TXT texto 191 253 201 263 "Hasta 8 letras"
miracap "F7: la carpeta nueva, con su nombre (CARPETA)" ex8 texto 194 225 204 235 CARPETA
miracap "el nombre cambiado y la carpeta nueva, en la carpeta" ex9 texto 228 166 238 176 CARPETA texto 316 166 326 176 NUEVO.TXT
miracap "Supr manda lo elegido a la Papelera (y lo dice)" ex10 texto 220 166 230 176 NUEVO.TXT notexto 210 160 400 180 CARPETA texto 207 475 217 485 Papelera
miracap "la Papelera, en el árbol: lo que se ha tirado" ex11 negrita 30 10 40 20 Papelera texto 228 166 238 176 CARPETA
miracap "Herramientas > Restaurar lo elegido: lo devuelve a su sitio, y la Papelera queda vacía" ex12 texto 217 135 227 145 "La Papelera est"
miracap "el botón secundario en un archivo: Abrir, Abrir con..., Mandar a la Papelera, Propiedades..." ex13 texto 461 338 471 348 "Abrir con..." \
  texto 461 452 471 462 "Mandar a la Papelera" texto 461 509 471 519 Propiedades
miracap "Control y un clic eligen dos" ex14 texto 111 475 121 485 "2 elegidos"
miracap "y un recuadro con el ratón, los que toca" ex15 texto 111 475 121 485 "7 elegidos"
mira "arrastrar un archivo al tapiz deja en él un acceso" "^escritorio: un acceso nuevo en el tapiz: C:.LEEME.TXT"
miracap "el acceso en el tapiz (con su flechita), y un programa soltado en el dock queda fijado" ex16 texto 79 541 89 551 LEEME.TXT texto 615 529 625 539 "Fijado en el dock."
miracap "buscar (F3) «pir» en la carpeta y las de dentro: lo encontrado, con su carpeta" ex17 negrita 236 172 256 192 PIRATA.TXT texto 373 177 383 187 "C:\\TEXTOS" \
  texto 103 475 113 485 "Encontrados: 2."
miracap "dos clics en el acceso del tapiz lo abren con lo suyo (EQUIPO /ABRIR: el Bloc de notas)" ex18 negrita 149 89 159 99 "LEEME.TXT"
miracap "el botón secundario en el dock: Abrir, Abrir otra ventana, Fijar, Quitar, Cerrar y Ocultar el dock" ex19 texto 329 501 339 511 "Ocultar el dock"
miracap "«Ocultar el dock» lo quita (y lo de debajo se ve)" ex20 nrgb 400 504 0F3F8A 20
saca $G/ex.img VACIA/NUEVO.TXT $G/nuevo.txt; saca $G/ex.img LEEME.TXT $G/leeme.txt
if cmp -s $G/nuevo.txt $G/leeme.txt && mdir -i $G/ex.img ::VACIA/CARPETA > /dev/null 2>&1; then ok "en el disco: la copia, con su nombre nuevo, igual que el original, y la carpeta restaurada en su sitio"
else ko "en el disco no está lo que se ha hecho en el explorador:"; mdir -i $G/ex.img ::VACIA 2>&1 | head -6 | sed 's/^/        /'; fi
saca $G/ex.img PAPELERA/INDICE.TXT $G/indice.txt
if [ -f $G/indice.txt ] && [ ! -s $G/indice.txt ] && mattrib -i $G/ex.img ::PAPELERA 2>/dev/null | grep -q "H"; then ok "la Papelera (\\PAPELERA, oculta) queda con su índice vacío"
else ko "la Papelera no ha quedado bien: $(cat $G/indice.txt 2>/dev/null | head -2)"; fi
saca $G/ex.img ATAJOS.INI $G/atajos.ini; saca $G/ex.img DOCK.INI $G/dock.ini
if grep -q "^atajo=C:.LEEME.TXT|LEEME.TXT|" $G/atajos.ini 2>/dev/null && grep -q "^fijado=.MINIAT.BIN||MINIAT.BIN|" $G/dock.ini 2>/dev/null && grep -q "^visible=no" $G/dock.ini \
  && [ "$(grep "^fijado=" $G/dock.ini | sed -n 4p | cut -d'|' -f1)" = 'fijado=\BLOC.BIN' ]; then
  ok "ATAJOS.INI guarda el acceso del tapiz, y DOCK.INI el orden (el Bloc, arrastrado al cuarto sitio), el programa fijado y que está oculto"
else ko "ATAJOS.INI o DOCK.INI no guardan lo que tocaba:"; cat $G/atajos.ini $G/dock.ini 2>&1 | head -12 | sed 's/^/        /'; fi
if sano $G/ex.img; then ok "y el disco, sano (fsck.fat)"; else ko "el disco ha quedado mal:"; sed 's/^/        /' $P/fsck.txt | head -5; fi
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda esa sesión, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# la orden ESCRITORIO del intérprete, y abrir un documento desde el menú
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" escritorio ":pausa 2" ":clic 40 585" ":clic 300 171" ":pausa 1" ":clic 500 171" ":pausa 2" \
  ":captura o1" ":clic 40 585" ":clic 360 545" ":clic 533 327" ":captura o2" > $G/orden.sal 2>&1
S=$G/orden.sal
mira "la orden ESCRITORIO del intérprete de órdenes (por la llamada al sistema) pone en marcha el escritorio, y ese intérprete acaba" \
  "^C:.>escritorio$" "^escritorio: en marcha, 800 x 600" "^El escritorio está en marcha\.$" "^ORDENES.BIN ha acabado con el código 0"
miracap "un documento del menú Inicio se abre con el Bloc de notas (LEEME.TXT, leído del disco)" o1 negrita 112 64 124 72 "LEEME.TXT - Bloc de notas" \
  texto 94 146 102 156 "Hispa-Pentium: un núcleo de 32 bits escrito en Castel."
miracap "«Cancelar» cierra el cuadro de apagar y todo sigue" o2 notexto 200 180 240 210 "Apagar el equipo" negrita 112 64 124 72 "LEEME.TXT - Bloc de notas"
# otros modos: 1024x768, y una pantalla de 24 bits por punto (se dibuja en 32 y se vuelca)
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE="escritorio modo=1024x768 bits=24 tapiz=1" PANTALLA=1024x768 MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  ":clic 40 753" ":raton 600 300" ":captura m1" > $G/modo.sal 2>&1
S=$G/modo.sal
mira "con «modo=1024x768 bits=24 tapiz=1» en la línea del cargador, ese modo y ese tapiz" "^escritorio: en marcha, 1024 x 768 con 24 bits por punto .*, tapiz 1"
miracap "a 1024x768 y con 24 bits por punto se ve igual: la barra abajo, el menú Inicio y el tapiz «Azul»" m1 tam 1024 768 negrita 30 738 46 748 Inicio \
  negrita 46 323 54 331 "Bloc de notas" rgb 500 3 16468E 8
# sin tarjeta gráfica que valga, se dice y la consola sigue
VGA=none $AQUI/nucleo32/probar.sh "" escritorio hora apagar > $G/sinvga.sal 2>&1
S=$G/sinvga.sal
mira "sin modo gráfico (ni del cargador ni VBE de Bochs), «escritorio» lo dice y la consola del núcleo sigue" \
  "^pantalla: no hay modo gráfico" "^no se puede poner en marcha el escritorio" "^llevo [0-9]* segundos" "^\[código 5\]"
# el núcleo que le pide el modo gráfico al cargador (para GRUB, en los PC de verdad)
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $G/semilla.asm > /dev/null 2>&1 && nasm -f bin $G/semilla.asm -o $G/SEMILLA.BIN
python3 -I $AQUI/nucleo32/modo_grafico.py $G/SEMILLA.BIN $G/GRAFICO.BIN 1024 768 32 > $G/mg.txt 2>&1
if python3 -I $AQUI/nucleo32/modo_grafico.py --mira $G/SEMILLA.BIN $G/GRAFICO.BIN 1024 768 32 >> $G/mg.txt 2>&1; then
  ok "modo_grafico.py: la copia del núcleo pide al cargador 1024x768x32 en su cabecera multiboot (bandera de vídeo y suma al día); lo demás, igual"
else ko "modo_grafico.py no deja bien la cabecera multiboot: $(tail -n 1 $G/mg.txt)"; fi
# la memoria USB con GRUB (hace falta ser root, y grub-install para PC)
if [ "$(id -u)" = 0 ] && command -v grub-install > /dev/null && [ -d /usr/lib/grub/i386-pc ] && command -v parted > /dev/null; then
  if $AQUI/nucleo32/usb.sh $G/usb.img > $G/usb.txt 2>&1; then
    rm -f $G/monu
    qemu-system-i386 -nic none -m 64 -display none -monitor unix:$G/monu,server,nowait -serial file:$G/usb.sal -no-reboot -device qemu-xhci \
      -drive file=$G/usb.img,format=raw,if=none,id=u,snapshot=on -device usb-storage,drive=u,bootindex=1 > /dev/null 2>&1 &
    QPID=$!
    monu() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$G/monu > /dev/null 2>&1; sleep "${2:-0.3}"; }
    for ((i = 0; i < 40; i++)); do [ -S $G/monu ] && break; sleep 0.1; done
    sleep 6; monu "sendkey ret" 12
    monu "mouse_move -360 285" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 0.8; monu "mouse_move 300 -200" 0.6
    monu "screendump $G/u1.ppm" 0.6
    monu "mouse_move -130 -105" 0.2; monu "mouse_move -130 -104" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 2.5; monu "mouse_move 400 200" 0.6
    monu "screendump $G/u2.ppm" 0.6
    monu quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $G/usb.sal | iconv -f CP437 -t UTF-8 > $G/usb.txt2
    S=$G/usb.txt2
    # (SSH, TAILSCALE: 23 programas: SSH, SSHD, WG y TAILSCAL también van como módulos)
    mira "la memoria USB con GRUB (usb.sh): arranca el núcleo que pide el modo gráfico, con los programas también como módulos (M:) y la memoria como unidad X:" \
      "^memoria: 64 MB, .* 26 programas de usuario" "^unidades: M: (en memoria) X: (disco 0, USB, FAT16); la del sistema, X:" \
      "^escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)"
    miracap "y el escritorio sale en el modo que ha puesto GRUB: la barra, el menú Inicio y el documento que ha venido como módulo" u1 tam 800 600 \
      negrita 30 570 46 580 Inicio negrita 46 155 54 163 "Bloc de notas" negrita 256 159 264 167 "Mis documentos"
    if grep -q "escritorio: lanzo .BLOC.BIN" $S && cap $G/u2.ppm negrita 112 64 124 72 "NOTAS.TXT - Bloc de notas"; then
      ok "y desde su menú se lanza el Bloc de notas (de la memoria USB, la unidad del sistema)"
    else ko "en la memoria USB, el Bloc de notas no se lanza desde el menú:"; sed 's/^/        /' $G/cap.txt | head -3; fi
    # la entrada de 1024 x 600 (la tercera): el modo lo pide la cabecera de ESCRIT6.BIN
    rm -f $G/monu
    qemu-system-i386 -nic none -m 64 -display none -monitor unix:$G/monu,server,nowait -serial file:$G/usb6.sal -no-reboot -device qemu-xhci \
      -drive file=$G/usb.img,format=raw,if=none,id=u,snapshot=on -device usb-storage,drive=u,bootindex=1 > /dev/null 2>&1 &
    QPID=$!
    for ((i = 0; i < 40; i++)); do [ -S $G/monu ] && break; sleep 0.1; done
    sleep 6; monu "sendkey down"; monu "sendkey down"; monu "sendkey ret" 12
    monu "screendump $G/u3.ppm" 0.6
    monu quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $G/usb6.sal | iconv -f CP437 -t UTF-8 > $G/usb6.txt
    S=$G/usb6.txt
    mira "la memoria USB, con la entrada de 1024 x 600: GRUB pone ese modo (el de la cabecera de esa copia del núcleo) y el escritorio sale a esa medida" \
      "^escritorio: en marcha, 1024 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)"
    miracap "y se ve entero: la barra abajo y los iconos" u3 tam 1024 600 negrita 30 570 46 580 Inicio texto 24 50 32 58 "Mi equipo"
    MTOOLS_SKIP_CHECK=1 mtype -i $G/usb.img@@1M ::/boot/grub/grub.cfg > $G/grub.cfg 2> /dev/null
    if grep -q "a la medida de la pantalla" $G/grub.cfg && grep -q "^  set gfxpayload=auto$" $G/grub.cfg && grep -q "^  videoinfo$" $G/grub.cfg \
      && [ "$(grep -c "^  multiboot /ESCRIT" $G/grub.cfg)" = 6 ] && grep -q "multiboot /ESCRIT13.BIN" $G/grub.cfg \
      && [ "$(grep -c "escritura=usb unidad=X registro" $G/grub.cfg)" = 7 ] && grep -q "multiboot /ESCRIT8.BIN escritorio sindisco sinusb sinred sinacpi$" $G/grub.cfg; then
      ok "el menú de GRUB: el escritorio a 800 x 600, «a la medida de la pantalla» (gfxpayload=auto), a 1024 x 600, 1366 x 768 y 1024 x 768, uno sin tocar el USB ni los discos, y los modos de vídeo de la BIOS (videoinfo); las demás, con «escritura=usb unidad=X registro»"
    else ko "el menú de GRUB de la memoria USB no trae las entradas nuevas"; fi
  else ko "usb.sh no puede hacer la imagen: $(tail -n 1 $G/usb.txt)"; fi
else
  echo "  (sin root o sin grub-install para PC no se prueba la memoria USB con GRUB)"
fi
fi   # SOLO=06, SOLO=07

# ------------------------------------------------- 9. Hispa-Pentium 0.6
# Los discos SATA (AHCI), las unidades con letra (varias a la vez) y el USB
# (xHCI, EHCI y UHCI): teclado, ratón, concentradores y memorias. QEMU da la
# controladora AHCI (-device ahci), las de USB (qemu-xhci, usb-ehci, las UHCI) y sus
# aparatos; con -machine pc,i8042=off no hay teclado ni ratón PS/2, como en
# un Mac, y lo que se teclea por el monitor llega por el teclado USB.
G=$P/graf
N=$P/n06
mkdir -p $G $N
if [ "${SOLO:-}" != 07 ] && [ "${SOLO:-}" != 08 ]; then
echo "== Hispa-Pentium 0.6: los discos SATA (AHCI), las unidades con letra, el permiso de escritura por disco =="
hechos=1
for c in 16 32 mbr; do
  [ -f $P/d$c.img ] || $AQUI/nucleo32/disco.sh $P/d$c.img $c > $P/disco$c.txt 2>&1 || { hechos=0; ko "no se puede hacer el disco de prueba ($c)"; }
done
if [ $hechos = 1 ]; then
suma=$(python3 -c "print(sum(sum(('linea %03d de un archivo de varios clusters\r\n' % i).encode()) for i in range(1, 401)))")
# 1. un disco SATA: leer
SATA=$P/d16.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir textos" "lanzar prueba.bin" :espera "lanzar textos\\eco2.bin hola" apagar > $N/sata.sal 2>&1
S=$N/sata.sal
mira "AHCI: la controladora SATA se encuentra en el bus PCI, IDENTIFY dice el disco, y la FAT se monta encima (C:)" \
  "^disco 0: QEMU HARDDISK, 16 MB, FAT16 (SATA)\.$" "^disco 0: QEMU HARDDISK, 32768 sectores (16 MB), LBA, SATA" \
  "^sistema de archivos: FAT16, desde el sector 0, " "^unidades: C: (disco 0, SATA, FAT16); la del sistema, C:"
mira "AHCI: se leen sectores por DMA: una carpeta, un archivo de varios clusters (su suma, la de verdad) y programas" "^LARGO.TXT  *17600$" \
  "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
  "^hijos: los dos ruidosos han acabado, con los códigos 9 y 9$" "^prueba.bin ha acabado con el código 7" "^hola$" "^eco2.bin ha acabado con el código 1"
# 1b. lo que ahci.cas apunta para un PC de verdad (la orden «disco» y ARRANQUE.TXT), y
# los caminos de las averías que en QEMU no se dan: PI a 0 (se miran los puertos de CAP),
# unos registros que no empiezan en una página (la ABAR del ICH7 va a 1 KB) y la segunda
# vuelta, reiniciando la controladora y cada puerto (COMRESET), si a la primera no sale el disco
S=$N/sata.sal
mira "AHCI: la orden «disco» dice lo que se ha visto: la controladora, CAP/PI/VS y cada puerto (SSTS, TFD, firma)" \
  "^ahci: controladora 00:0[0-9A-F]\.0, 8086:2922, orden 0107, ABAR " "^ahci: CAP [0-9A-F]*, GHC 80000000, PI 0000003F, VS 00010000" \
  "^ahci: puerto 0: SSTS 00000113, CMD [0-9A-F]*, TFD 00000050, SIG 00000101$" "^ahci: puerto 0: disco QEMU HARDDISK, 16 MB$"
ARRANQUE="ahci=pi0 ahci=desalineada ahci=reinicio" SATA=$P/d16.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir" apagar > $N/sataav.sal 2>&1
S=$N/sataav.sal
mira "AHCI, averías: con PI a 0, con los registros a media página y reiniciando la controladora (GHC.HR y COMRESET), el disco sale igual" \
  "^ahci: prueba de la base desalineada ([0-9A-F]*100): bien$" "^ahci: PI vale 0: se miran los puertos que dice CAP (0000003F)$" \
  "^ahci: ningún disco: se reinicia la controladora$" "^ahci: puerto 0: SSTS 00000113, CMD [0-9A-F]*, TFD 0000007F, SIG FFFFFFFF$" \
  "^ahci: puerto 0: disco QEMU HARDDISK, 16 MB$" "^disco 0: QEMU HARDDISK, 16 MB, FAT16 (SATA)\.$" "^LEEME.TXT  *118$"
# 1c. el orden de la norma (AHCI 1.3, 10.1.2), lo que no le salía al ICH7 del
# Toshiba: FRE = 1 antes de mirar TFD y, si el disco sigue ocupado, el COMRESET
# con FRE = 1 y luego ST = 1 («ahci=ocupado»: como si TFD siguiera en 80h); y la
# segunda vuelta del ICH6/ICH7 («ahci=ich7»), sin GHC.HR: solo el COMRESET de cada puerto
ARRANQUE="ahci=ocupado" SATA=$P/d16.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir" apagar > $N/sataoc.sal 2>&1
S=$N/sataoc.sal
l1=$(grep -n "^ahci: puerto 0: tras FRE=1: SSTS 00000113, TFD [0-9A-F]*, SERR 00000000, CMD [0-9A-F]*, SIG 00000101$" $S | head -1 | cut -d: -f1)
l2=$(grep -n "^ahci: puerto 0: tras COMRESET: SSTS 00000113, TFD [0-9A-F]*, SERR 00000000" $S | head -1 | cut -d: -f1)
l3=$(grep -n "^ahci: puerto 0: tras ST=1: SSTS 00000113, TFD [0-9A-F]*, SERR 00000000, CMD [0-9A-F]*[13579BDF], SIG 00000101$" $S | head -1 | cut -d: -f1)
if [ -n "$l1" ] && [ -n "$l2" ] && [ -n "$l3" ] && [ "$l1" -lt "$l2" ] && [ "$l2" -lt "$l3" ] && grep -q "^ahci: puerto 0: disco QEMU HARDDISK, 16 MB$" $S \
  && grep -q "^LEEME.TXT  *118$" $S && ! grep -q "se reinicia la controladora" $S; then
  ok "AHCI, el orden de la norma: FRE=1 antes de mirar TFD; con el disco ocupado, COMRESET (con FRE=1) y luego ST=1; cada paso, apuntado (SSTS, TFD, SERR); el disco sale y se lee"
else ko "AHCI: el orden FRE=1, COMRESET, ST=1 no sale así ($l1, $l2, $l3):"; grep "^ahci: puerto 0" $S | sed 's/^/        /' | head -6; fi
ARRANQUE="ahci=reinicio ahci=ich7" SATA=$P/d16.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir" apagar > $N/sataich7.sal 2>&1
S=$N/sataich7.sal
if grep -q "^ahci: ningún disco: COMRESET de cada puerto (en el ICH6/ICH7, sin reiniciar la controladora)$" $S && ! grep -q "se reinicia la controladora\|^ahci: reiniciada" $S \
  && grep -q "^ahci: puerto 0: tras COMRESET: SSTS 00000113" $S && grep -q "^ahci: puerto 0: disco QEMU HARDDISK, 16 MB$" $S && grep -q "^LEEME.TXT  *118$" $S; then
  ok "AHCI, la segunda vuelta en un ICH6/ICH7: sin GHC.HR (que en el Toshiba deja los puertos sin enlace), solo el COMRESET de cada puerto; el disco sale"
else ko "AHCI: la segunda vuelta del ICH7 no va como debe:"; grep "^ahci:" $S | sed 's/^/        /' | head -8; fi
# 2. escribir en él (FAT32 y la FAT16 de una partición), y que quede sano
for c in 32 mbr; do
  case $c in 32) que="FAT32" ;; mbr) que="FAT16 en una partición" ;; esac
  I=$N/s$c.img; cp $P/d$c.img $I
  M=$I; s0=; [ $c = mbr ] && { M=$I@@32256; s0=63; }
  ESCRIBE=1 SATA=$I MODULOS= $AQUI/nucleo32/probar.sh "" ordenes escribe "echo Querida señora: ¿qué tal está? > maría.txt" "copy textos\\largo.txt copia.txt" \
    "md obra" "copy *.bat obra" "del obra\\lote.bat" salir disco apagar > $N/sataw$c.sal 2>&1
  S=$N/sataw$c.sal
  mira "AHCI, $que: se escribe por DMA: un programa crea, escribe, corta, alarga y borra archivos y carpetas, y todo queda como estaba" \
    "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
    "^carpetas: 70 archivos en PD: 70 hechos; la carpeta tiene 72 entradas" "^escribe: todo queda como estaba (los mismos clusters libres): cierto$" \
    "^ *3 archivo(s) copiado(s)\.$" "; archivos abiertos: 0$" "^\[código 5\]"
  if sano $I $s0; then ok "AHCI, $que: fsck.fat da el disco por sano ($(tail -n 1 $P/fsck.txt | sed 's/^[^:]*: //'))"
  else ko "AHCI, $que: fsck.fat encuentra el disco mal:"; sed 's/^/        /' $P/fsck.txt | head -12; fi
  saca $M TEXTOS/LARGO.TXT $N/largo.txt; saca $M COPIA.TXT $N/copia.txt; saca $M MARIA.TXT $N/maria.txt
  if cmp -s $N/largo.txt $N/copia.txt && [ "$(iconv -f CP437 -t UTF-8 $N/maria.txt | tr -d '\r')" = "Querida señora: ¿qué tal está?" ] \
    && MTOOLS_SKIP_CHECK=1 mdir -i $M ::OBRA 2> /dev/null | grep -q "^SINFIN  *BAT" && ! MTOOLS_SKIP_CHECK=1 mdir -i $M ::OBRA 2> /dev/null | grep -q "^LOTE "; then
    ok "AHCI, $que: mtools ve lo escrito: lo copiado, igual que el original; lo tecleado, con sus letras; lo borrado, borrado"
  else ko "AHCI, $que: lo escrito no está bien en el disco"; fi
done
# 3. sin permiso no se escribe ni un sector
cp $P/d16.img $N/sl.img
SOLO_LECTURA=1 ESCRIBE=1 SATA=$N/sl.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "echo hola > nuevo.txt" "del leeme.txt" "md cosas" \
  "type leeme.txt" hola salir apagar > $N/sl.sal 2>&1
S=$N/sl.sal
mira "AHCI: sin «escritura» en la línea del cargador, el disco SATA es de solo lectura" \
  "^el disco es de solo lectura (para escribir en él: «escritura» en la línea del cargador)" "^No puedo escribir en nuevo.txt: es de solo lectura\.$" \
  "^No puedo borrar leeme.txt: es de solo lectura\.$" "^Este archivo está en un disco FAT" "^1 + 2 + ... + 1000 = 500500"
if cmp -s $P/d16.img $N/sl.img; then ok "AHCI: y la imagen queda idéntica, octeto a octeto: no se ha escrito ni un sector"
else ko "AHCI: sin permiso de escritura, el disco ha cambiado"; fi
# 4. un disco con tabla de particiones GPT (la de los discos de ahora)
if command -v parted > /dev/null; then
  truncate -s 20M $N/gpt.img
  parted -s $N/gpt.img mklabel gpt mkpart datos 2048s 34815s > /dev/null 2>&1
  dd if=$P/d16.img of=$N/gpt.img bs=512 seek=2048 conv=notrunc status=none
  SATA=$N/gpt.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir textos" "lanzar hola.bin" apagar > $N/gpt.sal 2>&1
  S=$N/gpt.sal
  mira "un disco con tabla de particiones GPT: se encuentra su partición y la FAT se monta en ella" \
    "^disco 0: QEMU HARDDISK, 20 MB, FAT16 (SATA)\.$" "^sistema de archivos: FAT16, desde el sector 2048, " "^PIRATA.TXT  *110$" "^hola.bin ha acabado con el código 3"
else
  echo "  (sin parted no se prueba la tabla de particiones GPT)"
fi
# 5. dos discos a la vez: el IDE (C:) y el SATA (D:)
cp $P/d16.img $N/c.img; cp $P/d32.img $N/d.img
ESCRIBE=1 DISCO=$N/c.img SATA=$N/d.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "dir d:\\textos" "copy c:\\textos\\pirata.txt d:\\poema.txt" \
  "d:" "type poema.txt" "md nueva" "cd nueva" "copy c:\\leeme.txt" "dir" "c:" "type d:\\nueva\\leeme.txt" "z:" "ren leeme.txt d:\\otro.txt" \
  "d:\\textos\\eco2 desde la otra" salir disco apagar > $N/dos.sal 2>&1
S=$N/dos.sal
mira "dos discos a la vez, cada uno con su letra: C: (el IDE) y D: (el SATA), y la FAT con dos volúmenes montados" \
  "^disco 0: QEMU HARDDISK, 16 MB, FAT16\.$" "^disco 1: QEMU HARDDISK, 40 MB, FAT32 (SATA)\.$" \
  "^unidades: C: (disco 0, IDE, FAT16) D: (disco 1, SATA, FAT32); la del sistema, C:"
mira "las rutas con la letra de la unidad: DIR, COPY de una a otra, TYPE y lanzar un programa de la otra unidad" \
  "^ Directorio de D:.TEXTOS$" "^ *1 archivo(s) copiado(s)\.$" "^un velero bergantín\.$" "^Este archivo está en un disco FAT" "^desde la otra$"
mira "«D:» cambia de unidad (la petición lo dice), con su carpeta de trabajo; una unidad que no hay, se dice; y de una a otra no se cambia de nombre" \
  "^D:.>type poema.txt$" "^D:.NUEVA>dir$" "^ Directorio de D:.NUEVA$" "^C:.>type d:.nueva.leeme.txt$" "^No hay unidad Z:\.$" \
  "^No puedo llamar d:.otro.txt a leeme.txt: "
if sano $N/c.img && sano $N/d.img; then ok "los dos discos quedan sanos (fsck.fat)"; else ko "con dos discos, alguno queda mal:"; sed 's/^/        /' $P/fsck.txt | head -8; fi
saca $N/d.img POEMA.TXT $N/poema.txt; saca $N/c.img TEXTOS/PIRATA.TXT $N/pirata.txt; saca $N/d.img NUEVA/LEEME.TXT $N/leeme2.txt; saca $N/c.img LEEME.TXT $N/leeme1.txt
if cmp -s $N/poema.txt $N/pirata.txt && cmp -s $N/leeme1.txt $N/leeme2.txt; then ok "lo copiado de C: a D: es igual que el original, octeto a octeto"
else ko "lo copiado de un disco a otro no es igual"; fi

echo "== Hispa-Pentium 0.6: el USB (xHCI, EHCI y UHCI): teclado, ratón, concentradores y memorias; sin teclado ni ratón PS/2 =="
# 6. xHCI y EHCI, sin 8042: el teclado USB (el español, con sus acentos) y la memoria USB (X:), en la que se escribe
for hc in xhci ehci; do
  case $hc in xhci) dev="qemu-xhci,id=u"; que="xHCI" ;; ehci) dev="usb-ehci,id=u"; que="EHCI" ;; esac
  cp $P/d16.img $N/u$hc.img
  ESCRIBE=1 MODULOS= MAQUINA=pc,i8042=off ARRANQUE="ordenes escritura" QEMU_MAS="-device $dev -device usb-kbd,bus=u.0 -device usb-mouse,bus=u.0 -drive id=mu,file=$N/u$hc.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu" \
    $AQUI/nucleo32/probar.sh "" "dir textos" "echo Querida señora: ¿qué tal está? El pingüino > maría.txt" "type maría.txt" \
    bucle :espera ":tecla ctrl-c" ":tecla up ret" :espera ":tecla ctrl-c" "copy textos\\largo.txt copia.txt" prueba :espera escribe salir disco apagar > $N/usb$hc.sal 2>&1
  S=$N/usb$hc.sal
  mira "$que: la controladora se toma, se enumeran los aparatos (teclado, ratón y memoria) y la memoria es la unidad X:" \
    "^usb: controladora $que " "($que); teclado$" "($que); ratón$" "($que); almacenamiento$" "^usb: 1 teclados, 1 ratones, 1 memorias\.$" \
    "^disco 0: QEMU    QEMU HARDDISK, 16 MB, FAT16 (USB)\.$" "^unidades: X: (disco 0, USB, FAT16); la del sistema, X:"
  mira "$que, sin teclado PS/2: el teclado USB es el español (eñe, acentos y diéresis con tecla muerta, ¿?), con Ctrl+C y las flechas" \
    "^X:.>echo Querida señora: ¿qué tal está? El pingüino > maría.txt$" "^Querida señora: ¿qué tal está? El pingüino$" "^X:.>prueba$" \
    "^bucle: doy vueltas sin fin" "^\^C$"
  if [ "$(grep -c '^bucle: doy vueltas sin fin' $S)" = 2 ]; then ok "$que: las flechas del teclado USB traen la orden de antes (la historia)"
  else ko "$que: las flechas del teclado USB no traen la orden de antes"; fi
  mira "$que: la memoria USB se lee (un archivo de varios clusters, programas) y se escribe (almacenamiento: solo a granel, SCSI)" \
    "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
    "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
    "^carpetas: 70 archivos en PD: 70 hechos; la carpeta tiene 72 entradas" "^escribe: todo queda como estaba (los mismos clusters libres): cierto$" \
    "; archivos abiertos: 0$" "^\[código 5\]"
  if sano $N/u$hc.img; then ok "$que: la memoria USB queda sana (fsck.fat: $(tail -n 1 $P/fsck.txt | sed 's/^[^:]*: //'))"
  else ko "$que: fsck.fat encuentra mal la memoria USB:"; sed 's/^/        /' $P/fsck.txt | head -12; fi
  saca $N/u$hc.img TEXTOS/LARGO.TXT $N/largo.txt; saca $N/u$hc.img COPIA.TXT $N/copia.txt; saca $N/u$hc.img MARIA.TXT $N/maria.txt
  if cmp -s $N/largo.txt $N/copia.txt && [ "$(iconv -f CP437 -t UTF-8 $N/maria.txt | tr -d '\r')" = "Querida señora: ¿qué tal está? El pingüino" ]; then
    ok "$que: mtools ve en la memoria lo copiado (igual que el original) y lo tecleado (con sus letras)"
  else ko "$que: lo escrito en la memoria USB no está bien"; fi
done
# 7. detrás de un concentrador (aparatos de USB 1.1), con el xHCI
ESCRIBE=1 MODULOS= MAQUINA=pc,i8042=off QEMU_MAS="-device qemu-xhci,id=u -device usb-hub,bus=u.0,port=1 -device usb-kbd,bus=u.0,port=1.1 -device usb-mouse,bus=u.0,port=1.2 -drive id=mu,file=$P/d16.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,port=1.3,drive=mu" \
  $AQUI/nucleo32/probar.sh "" "lanzar prueba.bin" :espera "lanzar hola.bin" apagar > $N/hub.sal 2>&1
S=$N/hub.sal
mira "un concentrador USB: se enciende, se reinician sus puertos y se enumera lo que cuelga de él (teclado, ratón y memoria, a velocidad plena)" \
  ": concentrador de 8 puertos$" "velocidad plena, en el puerto 1 del concentrador 0; teclado$" "en el puerto 2 del concentrador 0; ratón$" \
  "en el puerto 3 del concentrador 0; almacenamiento$" "^usb: 1 teclados, 1 ratones, 1 memorias\.$"
mira "y detrás del concentrador van el teclado (se teclea por él) y la memoria (se leen sus archivos y sus programas)" \
  "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" "^hola.bin ha acabado con el código 3" "^\[código 5\]"
# 7 bis. un EHCI con sus compañeras UHCI (los PC de 2002 a 2010): el teclado y el ratón, de USB 1.1, se ceden a ellas; la memoria va por el EHCI
cp $P/d16.img $N/comp.img
ESCRIBE=1 MODULOS= MAQUINA=pc,i8042=off ARRANQUE="ordenes escritura" QEMU_MAS="-device ich9-usb-ehci1,id=u -device ich9-usb-uhci1,masterbus=u.0,firstport=0 -device ich9-usb-uhci2,masterbus=u.0,firstport=2 -device usb-kbd,bus=u.0,port=1,usb_version=1 -device usb-mouse,bus=u.0,port=3,usb_version=1 -drive id=mu,file=$N/comp.img,format=raw,if=none -device usb-storage,bus=u.0,port=2,drive=mu" \
  $AQUI/nucleo32/probar.sh "" "copy textos\\largo.txt copia.txt" "echo El pingüino de la señora > a.txt" "type a.txt" prueba :espera salir disco apagar > $N/comp.sal 2>&1
S=$N/comp.sal
mira "un EHCI con dos UHCI compañeras: lo que no es de alta velocidad (el teclado y el ratón) se cede a ellas y se enumera allí; la memoria, por el EHCI" \
  "^usb: controladora EHCI 8086:293A, 6 puertos (2 cedidos a las compañeras: aparatos de USB 1.x)$" "^usb: controladora UHCI 8086:2934, 2 puertos$" \
  "velocidad alta, en el puerto 2 (EHCI); almacenamiento$" "velocidad plena, en el puerto 1 (UHCI); teclado$" "velocidad plena, en el puerto 1 (UHCI); ratón$" \
  "^usb: 1 teclados, 1 ratones, 1 memorias\.$"
mira "UHCI: el teclado USB (se teclea por él, sin teclado PS/2), con sus letras" "^X:.>echo El pingüino de la señora > a.txt$" "^El pingüino de la señora$" \
  "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" "; archivos abiertos: 0$" "^\[código 5\]"
saca $N/comp.img TEXTOS/LARGO.TXT $N/largo.txt; saca $N/comp.img COPIA.TXT $N/copia.txt
if sano $N/comp.img && cmp -s $N/largo.txt $N/copia.txt; then ok "y la memoria queda sana, con lo copiado igual que el original"
else ko "con el EHCI y sus compañeras, la memoria no queda bien"; fi
# 7 ter. una UHCI sola (un PC de antes), con la memoria en ella (a velocidad plena): leer y escribir
cp $P/d16.img $N/uhci.img
ESCRIBE=1 MODULOS= ARRANQUE="ordenes escritura" QEMU_MAS="-device piix3-usb-uhci,id=u -drive id=mu,file=$N/uhci.img,format=raw,if=none -device usb-storage,bus=u.0,port=1,drive=mu" \
  $AQUI/nucleo32/probar.sh "" "copy textos\\largo.txt copia.txt" escribe :espera :espera :espera :espera :espera :espera :espera :espera :espera :espera :espera :espera :espera :espera salir disco apagar > $N/uhci.sal 2>&1
S=$N/uhci.sal
mira "UHCI: una memoria USB a velocidad plena (cada paquete, un TD; la alternancia, llevada aquí): se lee y se escribe" \
  "^usb: controladora UHCI 8086:7020, 2 puertos$" "velocidad plena, en el puerto 1 (UHCI); almacenamiento$" "^ *1 archivo(s) copiado(s)\.$" \
  "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
  "^escribe: todo queda como estaba (los mismos clusters libres): cierto$" "; archivos abiertos: 0$" "^\[código 5\]"
saca $N/uhci.img TEXTOS/LARGO.TXT $N/largo.txt; saca $N/uhci.img COPIA.TXT $N/copia.txt
if sano $N/uhci.img && cmp -s $N/largo.txt $N/copia.txt; then ok "UHCI: la memoria queda sana (fsck.fat), con lo copiado igual que el original"
else ko "UHCI: la memoria no queda bien:"; sed 's/^/        /' $P/fsck.txt | head -8; fi
# 8. el permiso solo para las memorias USB: en el disco de dentro no se escribe
cp $P/d16.img $N/pc.img; cp $P/d16.img $N/pu.img
SOLO_LECTURA=1 ESCRIBE=1 DISCO=$N/pc.img MODULOS= ARRANQUE="ordenes escritura=usb unidad=X" QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$N/pu.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh "" "echo en la memoria > nuevo.txt" "type nuevo.txt" "echo en el disco > c:\\nuevo.txt" "del c:\\leeme.txt" "copy c:\\leeme.txt" \
  "dir c:\\textos" salir disco apagar > $N/perm.sal 2>&1
S=$N/perm.sal
mira "«escritura=usb unidad=X»: la unidad del sistema es la memoria USB y en ella se escribe; el disco de dentro se ve (C:), pero es de solo lectura" \
  "^los discos de dentro son de solo lectura" "^unidades: C: (disco 0, IDE, FAT16) X: (disco 1, USB, FAT16); la del sistema, X:" \
  "^X:.>type nuevo.txt$" "^en la memoria$" "^No puedo escribir en c:.nuevo.txt: es de solo lectura\.$" "^No puedo borrar c:.leeme.txt: es de solo lectura\.$" \
  "^ *1 archivo(s) copiado(s)\.$" "^ Directorio de C:.TEXTOS$"
saca $N/pu.img NUEVO.TXT $N/nuevo.txt; saca $N/pu.img LEEME.TXT $N/leeme2.txt; saca $N/pc.img LEEME.TXT $N/leeme1.txt
if cmp -s $P/d16.img $N/pc.img && [ "$(tr -d '\r' < $N/nuevo.txt)" = "en la memoria" ] && cmp -s $N/leeme1.txt $N/leeme2.txt && sano $N/pu.img; then
  ok "y así es: la imagen del disco de dentro queda idéntica octeto a octeto, y lo escrito y lo copiado están en la memoria USB (sana)"
else ko "con «escritura=usb», o el disco de dentro ha cambiado o la memoria no tiene lo escrito"; fi
# 9. «sinusb»: el USB ni se toca
MODULOS= QEMU_MAS="-device qemu-xhci,id=u -drive id=mu,file=$P/d16.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" ARRANQUE=sinusb \
  $AQUI/nucleo32/probar.sh "" disco apagar > $N/sinusb.sal 2>&1
S=$N/sinusb.sal
if ! grep -q "^usb:" $S && grep -q "^disco 0: QEMU DVD-ROM, lector de CD o DVD (IDE), sin disco$" $S && grep -q "^\[código 5\]" $S; then ok "con «sinusb» el núcleo ni toca el USB (se teclea por el teclado PS/2, y la memoria no se ve)"
else ko "«sinusb» no deja el USB sin tocar:"; sed 's/^/        /' $S | head -8; fi
# 10. el escritorio, sin teclado ni ratón PS/2: el ratón y el teclado USB, guardar en la memoria, y Mi equipo con sus unidades
cp $P/d16.img $N/eu.img
CAPTURAS=$G ESCRIBE=1 MODULOS= MAQUINA=pc,i8042=off SOLO_LECTURA=1 SATA=$P/d32.img ARRANQUE="escritorio escritura=usb unidad=X" \
  QEMU_MAS="-device usb-ehci,id=u -device usb-kbd,bus=u.0 -device usb-mouse,bus=u.0 -drive id=mu,file=$N/eu.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh "" ":pausa 2" ":clic 40 585" ":captura m1" ":clic 80 176" ":pausa 2" ":escribe Guardado en la memoria USB: ¡señor!" ":tecla f2" ":pausa 1" ":captura m2" \
  ":clic 578 76" ":pausa 1" ":doble 62 30" ":pausa 2" ":captura m3" ":doble 580 160" ":pausa 1" ":captura m4" > $N/escusb.sal 2>&1
S=$N/escusb.sal
mira "el escritorio en un PC sin teclado ni ratón PS/2 (como un Mac): arranca con el ratón y el teclado USB" \
  "^usb: 1 teclados, 1 ratones, 1 memorias\.$" "^escritorio: en marcha, 800 x 600" "escritorio: lanzo .BLOC.BIN" "^bloc: guardado NOTAS.TXT, 35 octetos"
miracap "el ratón USB mueve la flecha y pulsa: el menú Inicio se abre con un clic" m1 negrita 46 155 54 163 "Bloc de notas" negrita 320 533 328 541 "Apagar equipo"
miracap "el teclado USB escribe en el Bloc de notas (con eñe y «¡»), y F2 guarda" m2 texto 94 146 102 156 "Guardado en la memoria USB: ¡señor!" \
  texto 94 428 104 448 "Guardado en NOTAS.TXT (35 octetos)."
saca $N/eu.img NOTAS.TXT $N/notas.txt
if [ "$(iconv -f CP437 -t UTF-8 $N/notas.txt 2> /dev/null)" = "Guardado en la memoria USB: ¡señor!" ] && sano $N/eu.img; then ok "y lo guardado está en la memoria USB (la unidad del sistema, X:), que queda sana"
else ko "lo guardado con el Bloc de notas no está en la memoria USB"; fi
miracap "Mi equipo enseña las unidades con su letra: C: (el disco SATA de dentro, de solo lectura) y X: (la memoria USB)" m3 negrita 20 5 60 25 "Mi equipo" \
  negrita 313 133 323 143 "(C:)" texto 313 151 323 161 "libres de" texto 264 169 274 179 "(de solo lectura)" negrita 563 133 573 143 "(X:)"
miracap "y dos clics en X: enseñan lo que hay en la memoria (con lo recién guardado)" m4 negrita 20 5 60 25 "X:\\" texto 316 322 326 332 "LEEME.TXT" texto 316 400 326 410 "NOTAS.TXT"
fi   # los discos de prueba

# 11. la memoria USB con GRUB, como la arranca un PC: el núcleo la lee él (X:)
if [ "$(id -u)" = 0 ] && command -v grub-install > /dev/null && [ -d /usr/lib/grub/i386-pc ] && command -v parted > /dev/null; then
  [ -f $G/usb.img ] || $AQUI/nucleo32/usb.sh $G/usb.img > $G/usb.txt 2>&1
  if [ -f $G/usb.img ]; then
    for hc in xhci ehci; do
      case $hc in xhci) dev="qemu-xhci,id=u"; que="xHCI" ;; ehci) dev="usb-ehci,id=u"; que="EHCI" ;; esac
      cp $G/usb.img $N/g$hc.img
      rm -f $G/monu
      qemu-system-i386 -nic none -m 64 -machine pc,i8042=off -display none -monitor unix:$G/monu,server,nowait -serial file:$N/g$hc.sal -no-reboot -device $dev \
        -drive file=$N/g$hc.img,format=raw,if=none,id=m -device usb-storage,bus=u.0,drive=m,bootindex=1 -device usb-kbd,bus=u.0 -device usb-mouse,bus=u.0 > /dev/null 2>&1 &
      QPID=$!
      monu() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$G/monu > /dev/null 2>&1; sleep "${2:-0.3}"; }
      for ((i = 0; i < 40; i++)); do [ -S $G/monu ] && break; sleep 0.1; done
      sleep 6; monu "sendkey ret" 14
      monu "mouse_move -360 285" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 0.8
      monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -103" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 2.5
      for k in h o l a spc u s b; do monu "sendkey $k" 0.1; done
      monu "sendkey f2" 1.5
      monu "screendump $G/g$hc.ppm" 0.6
      monu quit 0.5; wait $QPID 2> /dev/null; QPID=
      tr -d '\r' < $N/g$hc.sal | iconv -f CP437 -t UTF-8 > $N/g$hc.txt
      S=$N/g$hc.txt
      mira "la memoria USB con GRUB, arrancada de un $que sin teclado PS/2: el núcleo le quita el USB a la BIOS y lee él la memoria (X:, la del sistema)" \
        "^usb: controladora $que " "^usb: 1 teclados, 1 ratones, 1 memorias\.$" "^disco 0: QEMU    QEMU HARDDISK, 64 MB, FAT16 (USB)\.$" \
        "^unidades: M: (en memoria) X: (disco 0, USB, FAT16); la del sistema, X:" "^escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)" \
        "escritorio: lanzo .BLOC.BIN" "^bloc: guardado NOTAS.TXT, 8 octetos"
      dd if=$N/g$hc.img of=$N/g$hc.vol bs=1M skip=1 status=none
      saca $N/g$hc.vol NOTAS.TXT $N/notas.txt; saca $N/g$hc.vol ARRANQUE.TXT $N/arranque.txt
      if cap $G/g$hc.ppm texto 94 146 102 156 "hola usb" && [ "$(cat $N/notas.txt 2> /dev/null)" = "hola usb" ] && grep -q "^usb: 1 teclados, 1 ratones, 1 memorias" $N/arranque.txt && grep -q "la del sistema, X:" $N/arranque.txt \
        && fsck.fat -n $N/g$hc.vol > $P/fsck.txt 2>&1; then
        ok "$que: lo tecleado con el teclado USB y guardado con el Bloc de notas está en la memoria (sana), con el registro del arranque (ARRANQUE.TXT)"
      else ko "$que: en la memoria USB de arranque no queda lo guardado, o no queda sana:"; sed 's/^/        /' $P/fsck.txt | head -6; fi
    done
    # la sexta entrada, «sin tocar el USB ni los discos»: como la 0.5 (con el
    # teclado y el ratón PS/2, y los programas de la unidad en memoria)
    rm -f $G/monu
    qemu-system-i386 -nic none -m 64 -display none -monitor unix:$G/monu,server,nowait -serial file:$N/gsin.sal -no-reboot -device qemu-xhci \
      -drive file=$G/usb.img,format=raw,if=none,id=u,snapshot=on -device usb-storage,drive=u,bootindex=1 > /dev/null 2>&1 &
    QPID=$!
    for ((i = 0; i < 40; i++)); do [ -S $G/monu ] && break; sleep 0.1; done
    sleep 6; for i in 1 2 3 4 5; do monu "sendkey down"; done; monu "sendkey ret" 12
    monu "mouse_move -360 285" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 0.8
    monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -103" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 2.5
    monu quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $N/gsin.sal | iconv -f CP437 -t UTF-8 > $N/gsin.txt
    S=$N/gsin.txt
    if ! grep -q "^usb:" $S && ! grep -q "^disco " $S && grep -q "^sin disco: los 26 archivos que ha traído el cargador hacen de unidad M:" $S \
      && grep -q "escritorio: lanzo .BLOC.BIN" $S; then
      ok "la entrada «sin tocar el USB, los discos ni la red»: ni una línea de USB ni de discos, la unidad del sistema es M: (en memoria) y de ella se lanza el Bloc de notas"
    else ko "la entrada «sin tocar el USB ni los discos» no arranca como la 0.5:"; sed 's/^/        /' $S | head -10; fi
  else ko "usb.sh no puede hacer la imagen: $(tail -n 1 $G/usb.txt)"; fi
else
  echo "  (sin root o sin grub-install para PC no se prueba la memoria USB con GRUB)"
fi

# 12. la oficina de Hispa-386 (programas de Hispa Ventanas, .HVP) con HVPRUN.BIN
echo "== Hispa-Pentium: la oficina de Hispa-386 dentro (HVPRUN: Facturas, Pluma, Cuentas y Mirador) =="
if $AQUI/nucleo32/disco.sh $N/of.img 16 oficina > $N/discoof.txt 2>&1; then
  CAPTURAS=$G ESCRIBE=1 DISCO=$N/of.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":clic 40 585" ":raton 150 470" ":captura of1" \
    ":clic 80 258" ":pausa 4" ":captura of2" ":clic 70 83" ":pausa 2" ":clic 440 190" ":escribe Taller Peña" ":clic 456 434" ":escribe Revisión del año" \
    ":clic 360 466" ":escribe 121,50" ":pausa 1" ":captura of3" ":clic 356 509" ":pausa 2" ":captura of4" ":clic 734 34" ":pausa 2" ":captura of5" \
    ":clic 40 585" ":clic 80 258" ":pausa 4" ":captura of6" ":clic 734 34" ":pausa 2" \
    ":clic 40 585" ":clic 80 288" ":pausa 4" ":escribe Hola, señor Pérez." ":tecla ret" ":escribe Segunda línea." ":clic 186 92" ":pausa 2" ":escribe carta" \
    ":tecla ret" ":pausa 2" ":captura of7" ":tecla ctrl-e" ":tecla ctrl-c" ":clic 695 44" ":pausa 2" ":doble 64 110" ":pausa 3" ":tecla ctrl-v" ":pausa 1" ":captura of8" \
    ":clic 40 585" ":clic 80 318" ":pausa 4" ":escribe 12" ":tecla ret" ":escribe 30" ":tecla ret" ":escribe =A1+A2" ":tecla ret" ":pausa 1" ":captura of9" > $N/oficina.sal 2>&1
  S=$N/oficina.sal
  mira "el menú Inicio lanza los programas de la oficina con HVPRUN.BIN" "escritorio: lanzo .HVPRUN.BIN (la tarea [0-9]*)"
  miracap "el menú Inicio tiene la oficina a mano (MENÚ: entre los más usados; y en Todos los programas > Oficina): Facturas, Pluma, Cuentas y Mirador" of1 \
    texto 38 246 46 254 Facturas texto 38 276 46 284 Pluma texto 38 306 46 314 Cuentas texto 38 336 46 344 Mirador negrita 46 155 54 163 "Bloc de notas" negrita 320 533 328 541 "Apagar equipo"
  miracap "Facturas (FACTURAS.HVP, de Hispa-386) en una ventana de Hispa-Pentium: su barra de menús, sus botones y su lista" of2 negrita 72 22 84 30 Facturas \
    texto 62 126 74 138 "No hay facturas emitidas." texto 206 70 218 80 Emitidas
  miracap "sus cuadros de diálogo salen dentro, con la fecha de hoy (INT 21h), y se escribe en ellos (con eñe y acentos)" of3 negrita 202 76 214 86 "Datos de la factura" \
    texto 310 178 322 188 "Taller Peña"
  miracap "la factura nueva: base, IVA y total, como saldrá en el papel" of4 texto 150 178 162 190 "Taller Peña" texto 70 252 82 264 "Revisión del año" negrita 670 314 682 326 "147,02"
  miracap "el botón de cerrar de la ventana acaba Facturas (ni ventana ni botón en la barra de tareas)" of5 notexto 50 20 200 40 Facturas notexto 100 570 400 595 Facturas
  miracap "y al volver a abrirla, la factura está (FACTURAS.DAT, en el disco FAT)" of6 texto 274 115 286 127 "Taller Peña" texto 590 115 602 127 "147,02" texto 54 497 66 509 "1 factura"
  miracap "Pluma (el procesador de textos): lo tecleado, y «Guardar» con su cuadro de elegir archivo; el título lo dice" of7 negrita 110 30 122 42 "Pluma - carta.PLU" \
    texto 98 142 110 154 "Hola, señor Pérez." texto 98 158 110 170 "Segunda línea."
  miracap "el portapapeles: lo copiado en Pluma (Ctrl+E, Ctrl+C) se pega en el Bloc de notas" of8 texto 140 180 260 260 "Hola, señor Pérez."
  miracap "Cuentas (la hoja de cálculo): dos números y su suma con una fórmula" of9 negrita 92 20 104 32 "Cuentas - (sin nombre)" texto 170 196 184 210 42
  saca $N/of.img OFICINA/FACTURAS.DAT $N/facturas.dat; saca $N/of.img CARTA.PLU $N/carta.plu
  if [ -s $N/facturas.dat ] && LC_ALL=C grep -qa "Taller Pe.a" $N/facturas.dat && LC_ALL=C grep -qa "Hola, se.or P.rez" $N/carta.plu && sano $N/of.img; then
    ok "lo guardado está en el disco: OFICINA\\FACTURAS.DAT (con la factura) y CARTA.PLU (lo de Pluma), y el disco queda sano"
  else ko "lo guardado por Facturas o Pluma no está en el disco, o no queda sano:"; sed 's/^/        /' $P/fsck.txt | head -6; fi
  # Mi equipo: un documento de Pluma y una imagen (Mirador), con el teclado:
  # Intro entra en C:, una letra va al primero que empieza por ella, Intro abre
  python3 - $N/FOTO.BMP <<'PY'
import struct, sys
an, al = 160, 100
filas = []
for y in range(al):
    f = bytearray()
    for x in range(an):
        f += bytes([255 * x // an, 255 * y // al, 200 if (x // 20 + y // 20) % 2 else 60])
    filas.append(bytes(f))
datos = b''.join(reversed(filas))
cab = b'BM' + struct.pack('<IHHI', 54 + len(datos), 0, 0, 54) + struct.pack('<IiiHHIIiiII', 40, an, al, 1, 24, 0, len(datos), 2835, 2835, 0, 0)
open(sys.argv[1], 'wb').write(cab + datos)
PY
  MTOOLS_SKIP_CHECK=1 mcopy -i $N/of.img $N/FOTO.BMP ::
  CAPTURAS=$G DISCO=$N/of.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":doble 62 30" ":pausa 3" ":tecla ret" ":pausa 2" \
    ":tecla f" ":pausa 1" ":tecla ret" ":pausa 5" ":raton 400 450" ":captura of10" ":clic 180 582" ":pausa 1" ":tecla c" ":tecla ret" ":pausa 5" ":captura of11" > $N/oficina2.sal 2>&1
  miracap "Mi equipo abre una imagen con Mirador (MIRADOR.HVP): su nombre, lo que mide y la imagen" of10 negrita 30 20 42 32 "Mirador - FOTO.BMP" \
    texto 10 520 20 535 "FOTO.BMP" texto 108 519 120 531 "160 x 100"
  miracap "y un documento de Pluma (.PLU) con Pluma: lo guardado antes, con sus acentos" of11 negrita 110 30 122 42 "Pluma - \\CARTA.PLU" \
    texto 146 142 158 154 "señor Pérez." texto 98 158 110 170 "Segunda línea."
else ko "disco.sh no puede hacer el disco con la oficina: $(tail -n 1 $N/discoof.txt)"; fi

fi   # SOLO=07

# ------------------------------------------------- 10. Hispa-Pentium 0.7
# Instalar en el disco duro: el programa INSTALAR (uinstala.cas), el arranque
# del disco (GRUB en el MBR y en el hueco de delante de la partición) y el
# asistente del primer arranque (uasiste.cas). Lo que se instala (el núcleo,
# los programas, BOOT.IMG y CORE.IMG) sale de la memoria USB que hace usb.sh.
if [ "${SOLO:-}" != 08 ]; then
echo "== Hispa-Pentium 0.7: instalar en el disco duro (INSTALAR), arrancar de él y el asistente del primer arranque =="
if [ "$(id -u)" = 0 ] && command -v grub-install > /dev/null && [ -d /usr/lib/grub/i386-pc ] && command -v parted > /dev/null; then
Z=$P/n07
K=$Z/kit
mkdir -p $Z $K
[ -f $G/usb.img ] || $AQUI/nucleo32/usb.sh $G/usb.img > $G/usb.txt 2>&1
if [ -f $G/usb.img ]; then
INST=
for f in ESCRIT8.BIN:NUCLEO.BIN INSTALAR.BIN ASISTE.BIN ORDENES.BIN BLOC.BIN CONSOLA.BIN EQUIPO.BIN HOLA.BIN INSTALA/LEEME.TXT:LEEME.TXT INSTALA/MANUAL.TXT:MANUAL.TXT INSTALA/BOOT.IMG:BOOT.IMG INSTALA/CORE.IMG:CORE.IMG; do
  mcopy -n -i $G/usb.img@@1M ::${f%%:*} $K/${f##*:} > /dev/null 2>&1
  INST=$INST${INST:+,}$K/${f##*:}
done
mcopy -n -i $G/usb.img@@1M ::SEMILLA.BIN $K/SEMILLA.BIN > /dev/null 2>&1
# (y dos archivos para otra carpeta, como los de la oficina: CARPETAS.TXT
# dice adónde va cada uno, y él no se instala)
head -c 3000 /dev/urandom > $K/UNO.DAT; printf 'el dos\r\n' > $K/DOS.TXT; printf 'UNO.DAT=COSAS\r\ndos.txt=cosas\r\nBLOC.PQI=HISPA\\PAQUETES\r\n' > $K/CARPETAS.TXT
# (y el registro de un paquete, que va dentro de \HISPA: a \HISPA\PAQUETES)
mcopy -n -i $G/usb.img@@1M ::INSTALA/PAQUETES/BLOC.PQI $K/BLOC.PQI > /dev/null 2>&1
INST=$INST,$K/UNO.DAT,$K/DOS.TXT,$K/BLOC.PQI,$K/CARPETAS.TXT
# instala con el núcleo arrancado por QEMU (-kernel) y lo que se instala como
# módulos (la unidad M:, como con la entrada de la memoria USB): el disco
# (SATA; con IDE=1, IDE), el nombre de lo que sale y las teclas
instala() {
  local d=$1 n=$2; shift 2
  if [ -n "${IDE:-}" ]; then
    CAPTURAS=$Z ESCRIBE=1 DISCO=$d MODULOS=$INST ARRANQUE="instalar unidad=M" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 1" "$@" > $Z/$n.sal 2>&1
  else
    CAPTURAS=$Z ESCRIBE=1 SATA=$d MODULOS=$INST ARRANQUE="instalar unidad=M" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 1" "$@" > $Z/$n.sal 2>&1
  fi
  S=$Z/$n.sal
}
# la partición (desde ese sector) de un disco, a $Z/vol.img, y ¿está sana?
volumen() { dd if=$1 of=$Z/vol.img bs=512 skip=$2 conv=sparse status=none; fsck.fat -n $Z/vol.img > $Z/fsck.txt 2>&1; }
# ¿está en el volumen ($Z/vol.img) el sistema, igual que el que se instala, y el disco arrancable?
instalado() {
  local f bien=1
  for f in NUCLEO.BIN INSTALAR.BIN ASISTE.BIN ORDENES.BIN BLOC.BIN CONSOLA.BIN EQUIPO.BIN HOLA.BIN LEEME.TXT MANUAL.TXT BOOT.IMG CORE.IMG; do
    rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::HISPA/$f $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $K/$f || { bien=0; echo "distinto: $f" >> $Z/fsck.txt; }
  done
  rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::LEEME.TXT $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $K/LEEME.TXT || { bien=0; echo "distinto: LEEME.TXT de la raíz" >> $Z/fsck.txt; }
  if [ "${2:-}" = usb ]; then
    # (instalado desde la memoria USB: si lleva la oficina, HVPRUN.BIN en \HISPA y lo suyo en \OFICINA)
    if mcopy -n -i $G/usb.img@@1M ::OFICINA/PLUMA.HVP $Z/o.bin > /dev/null 2>&1; then
      rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::OFICINA/PLUMA.HVP $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $Z/o.bin || { bien=0; echo "distinto: OFICINA/PLUMA.HVP" >> $Z/fsck.txt; }
      mcopy -n -i $G/usb.img@@1M ::HVPRUN.BIN $Z/o.bin > /dev/null 2>&1
      rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::HISPA/HVPRUN.BIN $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $Z/o.bin || { bien=0; echo "distinto: HVPRUN.BIN" >> $Z/fsck.txt; }
      rm -f $Z/o.bin
    fi
  else
    for f in UNO.DAT DOS.TXT; do
      rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::COSAS/$f $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $K/$f || { bien=0; echo "distinto: COSAS/$f" >> $Z/fsck.txt; }
    done
    mdir -i $Z/vol.img ::HISPA 2> /dev/null | grep -q "^UNO \|^DOS \|^CARPETAS\|^BLOC  *PQI" && { bien=0; echo "en HISPA hay lo que no toca" >> $Z/fsck.txt; }
    rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::HISPA/PAQUETES/BLOC.PQI $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $K/BLOC.PQI || { bien=0; echo "distinto: HISPA/PAQUETES/BLOC.PQI" >> $Z/fsck.txt; }
  fi
  # el arranque: el primer sector de GRUB en el MBR (sus 440 octetos) y el resto desde el sector 1
  cmp -s -n 440 $1 $K/BOOT.IMG || { bien=0; echo "el MBR no lleva BOOT.IMG" >> $Z/fsck.txt; }
  cmp -s <(dd if=$1 bs=512 skip=1 status=none | head -c $(stat -c %s $K/CORE.IMG)) $K/CORE.IMG || { bien=0; echo "CORE.IMG no está desde el sector 1" >> $Z/fsck.txt; }
  [ $bien = 1 ]
}
tabla() { parted -s $1 unit s print 2> /dev/null | sed -n '/^Partition Table/p; /^ *[0-9]/p' | tr -s ' ' | sed 's/ *$//' > $Z/tabla.txt; }

# 1. un disco SATA en blanco: la bienvenida, la ayuda, elegir el disco, instalar
truncate -s 300M $Z/blanco.img
instala $Z/blanco.img blanco ":texto i1" ":tecla f1" ":texto i2" ":tecla ret" ":tecla ret" ":texto i3" ":tecla ret" ":texto i4" ":tecla ret" ":pausa 5" ":texto i5" ":tecla ret"
if grep -q "Instalación de Hispa-Pentium 0.8" $Z/i1.txt && grep -q "Bienvenido a la instalación de Hispa-Pentium" $Z/i1.txt && grep -q "INTRO=Continuar   F1=Ayuda   F3=Salir" $Z/i1.txt \
  && grep -q "La memoria USB de arranque no se toca" $Z/i1.txt && grep -q "^|   Ayuda$" $Z/i2.txt && grep -q "hay que confirmarlo dos veces" $Z/i2.txt; then
  ok "INSTALAR: la pantalla azul de bienvenida (el título, el pie con INTRO=Continuar, F1=Ayuda y F3=Salir) y la ayuda (F1)"
else ko "INSTALAR: la bienvenida o la ayuda no salen como deben:"; sed 's/^/        /' $Z/i1.txt | head -8; fi
if grep -q "Elija el disco donde instalar Hispa-Pentium" $Z/i3.txt && grep -q "Disco 1   SATA  QEMU HARDDISK  *300 MB" $Z/i3.txt && grep -q "(en blanco: sin particiones)" $Z/i3.txt \
  && grep -q "Todo listo para instalar" $Z/i4.txt && grep -q "Partición:  una nueva de 299 MB (el disco está en blanco)" $Z/i4.txt && grep -q "INTRO=Instalar   ESC=Volver   F3=Salir" $Z/i4.txt \
  && grep -q "Hispa-Pentium está instalado" $Z/i5.txt && grep -q "Quite ahora la memoria USB y pulse INTRO para reiniciar" $Z/i5.txt; then
  ok "INSTALAR: elegir el disco (con su tamaño y lo que tiene), la pantalla de «todo listo» y la del final"
else ko "INSTALAR: las pantallas de elegir el disco, «todo listo» o el final no salen como deben:"; sed 's/^/        /' $Z/i3.txt | head -10; fi
mira "INSTALAR en un disco SATA en blanco: la partición, el formato FAT32, la copia a \\HISPA y el arranque; y reinicia" \
  "^instalar: disco 0, SATA, 300 MB, se puede: 1$" "^instalar: partición 1, desde el sector 2048, 612352 sectores$" \
  "^instalar: FAT32 con clusters de 4096 octetos, [0-9]* clusters, [0-9]* sectores por FAT$" "^instalar: copiado NUCLEO.BIN ([0-9]* octetos)$" \
  "^instalar: copiado LEEME.TXT a la raíz" "^instalar: Dejando el disco arrancable\.\.\.$" "^instalar: terminado$" "^reiniciar$"
tabla $Z/blanco.img
if volumen $Z/blanco.img 2048 && instalado $Z/blanco.img && grep -q "^Partition Table: msdos" $Z/tabla.txt && [ "$(grep -c '^ [0-9]' $Z/tabla.txt)" = 1 ] \
  && grep -q "^ 1 2048s 614399s 612352s primary fat32 boot, lba" $Z/tabla.txt; then
  ok "el disco instalado: una partición FAT32 arrancable en el MBR, sana para fsck.fat, con el sistema en \\HISPA igual que el original y GRUB en el MBR y en el hueco de delante"
else ko "el disco instalado no queda bien:"; cat $Z/tabla.txt $Z/fsck.txt | sed 's/^/        /' | head -8; fi
if grep -q "^instalar: copiado UNO.DAT (3000 octetos) a .COSAS$" $S && grep -q "^instalar: copiado DOS.TXT (8 octetos) a .COSAS$" $S && grep -q "^instalar: 15 archivos que instalar$" $S \
  && [ "$(mdir -b -i $Z/vol.img ::COSAS 2> /dev/null | tr '\n' ' ')" = "::/COSAS/UNO.DAT ::/COSAS/DOS.TXT " ] \
  && grep -q "^instalar: copiado BLOC.PQI ([0-9]* octetos) a .HISPA.PAQUETES$" $S && [ "$(mdir -b -i $Z/vol.img ::HISPA/PAQUETES 2> /dev/null | tr '\n' ' ')" = "::/HISPA/PAQUETES/BLOC.PQI " ]; then
  ok "lo que dice CARPETAS.TXT («ARCHIVO.EXT=CARPETA») va a su carpeta de la raíz (C:\\COSAS: así irá la oficina a C:\\OFICINA) y no a \\HISPA, y «HISPA\\PAQUETES», dentro de \\HISPA (los registros de lo instalado); CARPETAS.TXT no se instala"
else ko "lo de CARPETAS.TXT no va a su carpeta:"; mdir -i $Z/vol.img :: 2>&1 | sed 's/^/        /' | head -8; grep "archivos que\|COSAS" $S | sed 's/^/        /' | head -4; fi
rm -f $Z/cfg.txt; mcopy -n -i $Z/vol.img ::HISPA/GRUB.CFG $Z/cfg.txt > /dev/null 2>&1
if grep -q "^set modo=800x600x32,800x600,auto$" $Z/cfg.txt && grep -q "multiboot /HISPA/NUCLEO.BIN escritorio escritura$" $Z/cfg.txt && grep -q "module /HISPA/ORDENES.BIN ORDENES.BIN" $Z/cfg.txt \
  && ! grep -q chainloader $Z/cfg.txt && ! grep -q "INSTALAR.BIN" $Z/cfg.txt; then
  ok "el menú de arranque del disco (\\HISPA\\GRUB.CFG): el escritorio, el modo texto y el de emergencia con los programas como módulos"
else ko "\\HISPA\\GRUB.CFG no es el que tiene que ser:"; sed 's/^/        /' $Z/cfg.txt | head -8; fi

# (y otra vez encima: el disco ya tiene su FAT32, que el núcleo ha montado
# como C:; al escribir en él por debajo deja de estarlo)
instala $Z/blanco.img otravez ":tecla ret" ":texto r1" ":tecla ret" ":tecla ret" ":tecla s" ":escribe BORRAR" ":tecla ret" ":tecla ret" ":pausa 5" ":texto r2" ":tecla ret"
if grep -q "^disco 0: QEMU HARDDISK, 300 MB, FAT32 (SATA)\.$" $S && grep -q "partición 1: FAT32  *299 MB" $Z/r1.txt && grep -q "^instalar: terminado$" $S \
  && grep -q "Hispa-Pentium está instalado" $Z/r2.txt && volumen $Z/blanco.img 2048 && instalado $Z/blanco.img; then
  ok "instalar otra vez encima (el disco ya llevaba Hispa-Pentium, montado como C:): borrándolo con sus dos confirmaciones, queda instalado y sano"
else ko "instalar encima de un Hispa-Pentium ya instalado no va:"; sed 's/^/        /' $Z/fsck.txt | head -6; tail -n 4 $S | sed 's/^/        /'; fi

# 2. un disco con la partición de otro sistema y espacio sin usar: salir con
# F3 y no confirmar el borrado no tocan nada; se instala en lo libre
truncate -s 300M $Z/otro.img
parted -s $Z/otro.img mklabel msdos mkpart primary ntfs 1MiB 101MiB set 1 boot on
dd if=/dev/urandom of=$Z/otro.img bs=1M seek=1 count=100 conv=notrunc status=none
cp $Z/otro.img $Z/otro0.img
instala $Z/otro.img otro1 ":tecla ret" ":texto o1" ":tecla ret" ":texto o2" ":tecla down" ":tecla ret" ":texto o3" ":tecla s" ":texto o4" ":escribe BORRA" ":tecla ret" ":texto o5" \
  ":tecla down" ":tecla ret" ":tecla n" ":tecla f3" ":texto o6" ":tecla n" ":tecla f3" ":tecla s"
if grep -q "partición 1: NTFS o exFAT (Windows)  *100 MB" $Z/o1.txt && grep -q "espacio sin usar:  *199 MB" $Z/o1.txt \
  && grep -q "Tiene 199 MB sin usar: se puede instalar ahí sin tocar lo que hay" $Z/o2.txt && grep -q "Instalar en el espacio sin usar (no se toca lo que hay)" $Z/o2.txt \
  && grep -q "Borrar el disco entero e instalar (se pierde TODO lo que tiene)" $Z/o2.txt; then
  ok "un disco con otro sistema: se enseñan su partición (NTFS) y el espacio sin usar, y se ofrece instalar ahí o borrar el disco"
else ko "un disco con otro sistema: no se enseña bien lo que tiene:"; sed 's/^/        /' $Z/o1.txt | sed -n 5,12p; fi
if grep -q "ATENCION: se va a BORRAR TODO lo que hay en este disco" $Z/o3.txt && grep -q "S=Sí, borrar el disco   ESC=Volver (no se toca nada)" $Z/o3.txt \
  && grep -q "Borrar el disco entero: segunda confirmación" $Z/o4.txt && grep -q "escriba la palabra BORRAR y pulse INTRO" $Z/o4.txt \
  && grep -q "Dónde instalar Hispa-Pentium en este disco" $Z/o5.txt && grep -q "^instalar: no se borra$" $S && grep -q "¿Salir de la instalación y reiniciar el equipo?  (S/N)" $Z/o6.txt \
  && grep -q "^instalar: se sale$" $S && ! grep -q "^instalar: partición" $S && cmp -s $Z/otro.img $Z/otro0.img; then
  ok "borrar el disco pide dos confirmaciones (una S y escribir BORRAR): con la palabra mal escrita, con N y saliendo con F3, el disco queda idéntico octeto a octeto"
else ko "las confirmaciones de borrar no van (o el disco ha cambiado sin confirmarlo):"; sed 's/^/        /' $Z/o5.txt | sed -n 4,10p; fi
instala $Z/otro.img otro2 ":tecla ret" ":tecla ret" ":tecla ret" ":texto o7" ":tecla ret" ":pausa 5" ":texto o8" ":tecla ret"
tabla $Z/otro.img
if grep -q "una nueva de 199 MB en el espacio sin usar; lo demás no se toca" $Z/o7.txt && grep -q "hay en el disco (su partición 1)" $Z/o7.txt && grep -q "^instalar: partición 2, desde el sector 206848, 407552 sectores$" $S \
  && grep -q "El sistema que ya había en el disco sigue en su sitio" $Z/o8.txt && cmp -s <(dd if=$Z/otro.img bs=1M skip=1 count=100 status=none) <(dd if=$Z/otro0.img bs=1M skip=1 count=100 status=none) \
  && grep -q "^ 1 2048s 206847s 204800s primary$" $Z/tabla.txt && grep -q "^ 2 206848s 614399s 407552s primary fat32 boot, lba" $Z/tabla.txt \
  && volumen $Z/otro.img 206848 && instalado $Z/otro.img; then
  ok "instalar en el espacio sin usar: la partición del otro sistema queda idéntica octeto a octeto, la nueva (la 2) es la arrancable, y está sana y con el sistema"
else ko "instalar en el espacio sin usar no va (o toca lo que había):"; cat $Z/tabla.txt $Z/fsck.txt | sed 's/^/        /' | head -8; fi
rm -f $Z/cfg.txt $Z/mbr.ant; mcopy -n -i $Z/vol.img ::HISPA/GRUB.CFG $Z/cfg.txt > /dev/null 2>&1; mcopy -n -i $Z/vol.img ::HISPA/MBR.ANT $Z/mbr.ant > /dev/null 2>&1
if grep -q "^set timeout=8$" $Z/cfg.txt && grep -q 'El sistema que ya estaba en el disco (particion 1)' $Z/cfg.txt && grep -q "set root=(hd0,msdos1)" $Z/cfg.txt && grep -q "chainloader +1" $Z/cfg.txt \
  && cmp -s -n 512 $Z/mbr.ant $Z/otro0.img; then
  ok "el menú de arranque lleva una entrada para el sistema que ya había, y el primer sector de antes queda guardado (\\HISPA\\MBR.ANT)"
else ko "el menú de arranque no lleva el otro sistema, o no se guarda el primer sector de antes:"; sed 's/^/        /' $Z/cfg.txt | tail -n 6; fi

# 3. un disco lleno (sin espacio sin usar): solo se puede borrar, con sus dos confirmaciones
truncate -s 200M $Z/lleno.img
parted -s $Z/lleno.img mklabel msdos mkpart primary ext2 1MiB 100%
dd if=/dev/urandom of=$Z/lleno.img bs=1M seek=1 count=20 conv=notrunc status=none
instala $Z/lleno.img lleno ":tecla ret" ":tecla ret" ":texto l1" ":tecla ret" ":tecla s" ":escribe borrar" ":tecla ret" ":texto l2" ":tecla ret" ":pausa 5" ":texto l3" ":tecla ret"
tabla $Z/lleno.img
if grep -q "No tiene bastante espacio sin usar" $Z/l1.txt && ! grep -q "Instalar en el espacio sin usar" $Z/l1.txt && grep -q "una nueva de 199 MB, BORRANDO todo lo que tiene el disco" $Z/l2.txt \
  && grep -q "^instalar: confirmar borrar (2)$" $S && grep -q "^instalar: FAT32 con clusters de 512 octetos, " $S && grep -q "Hispa-Pentium está instalado" $Z/l3.txt && [ "$(grep -c '^ [0-9]' $Z/tabla.txt)" = 1 ] && grep -q "^ 1 2048s 409599s 407552s primary fat32 boot, lba" $Z/tabla.txt \
  && volumen $Z/lleno.img 2048 && instalado $Z/lleno.img; then
  ok "un disco lleno: no se ofrece el espacio sin usar; confirmado dos veces (S y BORRAR), se borra entero y queda instalado y sano (199 MB: FAT32 con clusters de 512 octetos)"
else ko "borrar un disco lleno e instalar no va:"; cat $Z/tabla.txt $Z/fsck.txt | sed 's/^/        /' | head -8; sed 's/^/        /' $Z/l1.txt | sed -n 6,14p; fi

# 4. un disco de los de antes (la partición empieza en el sector 63: no hay
# hueco para el arranque) y uno con tabla GPT: solo borrándolos
truncate -s 200M $Z/viejo.img
printf 'label: dos\n63,80M,7\n' | sfdisk -q $Z/viejo.img > /dev/null 2>&1
cp $Z/viejo.img $Z/viejo0.img
instala $Z/viejo.img viejo ":tecla ret" ":tecla ret" ":texto v1" ":tecla f3" ":tecla s"
if grep -q "su primera partición empieza demasiado" $Z/v1.txt && ! grep -q "Instalar en el espacio sin usar" $Z/v1.txt && cmp -s $Z/viejo.img $Z/viejo0.img; then
  ok "un disco de los de antes (la partición en el sector 63): se dice que no hay sitio para el arranque sin borrar, y no se toca"
else ko "el disco con la partición en el sector 63 no se trata bien:"; sed 's/^/        /' $Z/v1.txt | sed -n 6,14p; fi
truncate -s 200M $Z/gpt.img
parted -s $Z/gpt.img mklabel gpt mkpart datos ntfs 1MiB 60MiB mkpart linux ext4 60MiB 120MiB
instala $Z/gpt.img gpt ":tecla ret" ":texto g1" ":tecla ret" ":texto g2" ":tecla ret" ":tecla s" ":escribe BORRAR" ":tecla ret" ":tecla ret" ":pausa 5" ":texto g3" ":tecla ret"
tabla $Z/gpt.img
if grep -q "partición 1: NTFS o exFAT (Windows)  *59 MB" $Z/g1.txt && grep -q "partición 2: Linux  *60 MB" $Z/g1.txt && grep -q "Usa una tabla de particiones GPT" $Z/g2.txt \
  && ! grep -q "Instalar en el espacio sin usar" $Z/g2.txt && grep -q "Hispa-Pentium está instalado" $Z/g3.txt && grep -q "^Partition Table: msdos" $Z/tabla.txt \
  && volumen $Z/gpt.img 2048 && instalado $Z/gpt.img; then
  ok "un disco con tabla GPT: se leen sus particiones; solo se instala borrándolo entero (queda con tabla MBR, sin rastro de la GPT), sano y con el sistema"
else ko "el disco con tabla GPT no se trata bien:"; cat $Z/tabla.txt $Z/fsck.txt | sed 's/^/        /' | head -8; sed 's/^/        /' $Z/g2.txt | sed -n 6,14p; fi

# 5. un disco IDE (el canal de siempre), de 2 GB: la FAT32 con clusters de 4 K
truncate -s 2G $Z/ide.img
IDE=1 instala $Z/ide.img ide ":tecla ret" ":texto d1" ":tecla ret" ":tecla ret" ":pausa 9" ":texto d2" ":tecla ret"
if grep -q "Disco 1   IDE   QEMU HARDDISK  *2048 MB" $Z/d1.txt && grep -q "^instalar: FAT32 con clusters de 4096 octetos, [0-9]* clusters, [0-9]* sectores por FAT$" $S \
  && grep -q "Hispa-Pentium está instalado" $Z/d2.txt && volumen $Z/ide.img 2048 && instalado $Z/ide.img; then
  ok "un disco IDE de 2 GB: instalado, con su FAT32 de clusters de 4 K sana y el sistema dentro"
else ko "instalar en un disco IDE de 2 GB no va:"; sed 's/^/        /' $Z/fsck.txt | head -6; tail -n 4 $S | sed 's/^/        /'; fi

# 6. sin «instalar» en la línea del cargador, el núcleo no deja escribir en
# los discos sector a sector: INSTALAR no encuentra dónde, aunque haya «escritura»
truncate -s 100M $Z/sin.img
dd if=/dev/urandom of=$Z/sin.img bs=1M count=1 conv=notrunc status=none
cp $Z/sin.img $Z/sin0.img
CAPTURAS=$Z ESCRIBE=1 SATA=$Z/sin.img MODULOS=$INST ARRANQUE="ordenes escritura unidad=M" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 1" instalar ":tecla ret" ":texto n1" ":tecla f3" > $Z/sin.sal 2>&1
S=$Z/sin.sal
if grep -q "^instalar: disco 0, SATA, 100 MB, se puede: 0$" $S && grep -q "No se ha encontrado ningún disco duro en este equipo" $Z/n1.txt && cmp -s $Z/sin.img $Z/sin0.img; then
  ok "sin «instalar» en la línea del cargador (aunque haya «escritura»), INSTALAR no puede tocar ningún disco: el núcleo no le deja"
else ko "sin «instalar» en la línea del cargador, INSTALAR ve discos donde escribir:"; sed 's/^/        /' $Z/n1.txt | sed -n 4,12p; fi

# 7. de verdad: la memoria USB arrancada como tal (GRUB), su entrada «Instalar
# Hispa-Pentium en el disco duro», un disco SATA en blanco de 2 GB; luego, SOLO
# el disco: GRUB desde su MBR, el escritorio y el asistente del primer arranque
monz() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$Z/mon > /dev/null 2>&1; sleep "${2:-0.25}"; }
teclas() { local k; for k in "$@"; do monz "sendkey $k" 0.12; done; }
textoz() {     # la pantalla de texto, a $Z/$1.txt
  monz "pmemsave 0xb8000 4000 \"$Z/scr.bin\"" 0.3
  python3 -I -c "
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print('|' + bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())" $Z/scr.bin > $Z/$1.txt 2> /dev/null
}
espera_a() {   # espera (hasta $2 segundos) a que salga esa línea por el puerto serie ($3)
  local i
  for ((i = 0; i < $2 * 2; i++)); do grep -qa -- "$1" $3 2> /dev/null && return 0; sleep 0.5; done
  return 1
}
espera_menu() {    # espera (hasta 15 s) a que el menú de GRUB esté en la pantalla, con ese texto; queda en $Z/$2.txt
  local i
  for ((i = 0; i < 40; i++)); do
    textoz $2
    # (ya está: un momento más, que acabe de pintarse, y otra vez)
    if grep -q -- "$1" $Z/$2.txt; then sleep 0.6; textoz $2; return 0; fi
    sleep 0.1
  done
  return 1
}
desde_disco() {    # arranca solo del disco $1 (SATA); lo que sale, a $2
  rm -f $Z/mon
  qemu-system-i386 -nic none -m 64 -display none -monitor unix:$Z/mon,server,nowait -serial file:$2 -no-reboot -rtc base=localtime \
    -device ahci,id=sata -drive id=dsata,file=$1,format=raw,if=none -device ide-hd,drive=dsata,bus=sata.0,bootindex=1 > /dev/null 2>&1 &
  QPID=$!
  for ((i = 0; i < 40; i++)); do [ -S $Z/mon ] && break; sleep 0.1; done
}
for hc in xhci ehci; do
  case $hc in xhci) dev="qemu-xhci,id=u"; que="xHCI" ;; ehci) dev="usb-ehci,id=u"; que="EHCI" ;; esac
  cp $G/usb.img $Z/u$hc.img
  rm -f $Z/d$hc.img; truncate -s 2G $Z/d$hc.img
  rm -f $Z/mon
  qemu-system-i386 -nic none -m 64 -display none -monitor unix:$Z/mon,server,nowait -serial file:$Z/i$hc.sal -no-reboot -rtc base=localtime -device $dev \
    -drive file=$Z/u$hc.img,format=raw,if=none,id=m -device usb-storage,bus=u.0,drive=m,bootindex=1 \
    -device ahci,id=sata -drive id=dsata,file=$Z/d$hc.img,format=raw,if=none -device ide-hd,drive=dsata,bus=sata.0,bootindex=2 > /dev/null 2>&1 &
  QPID=$!
  for ((i = 0; i < 40; i++)); do [ -S $Z/mon ] && break; sleep 0.1; done
  espera_menu "Instalar Hispa-Pentium" m$hc; teclas down down down down down down down down down; textoz m2$hc; teclas ret
  espera_a "^instalar: bienvenida" 40 $Z/i$hc.sal; sleep 0.5
  [ $hc = xhci ] && [ -n "${CAPTURAS_A:-}" ] && monz "screendump $G/i1-bienvenida.ppm" 0.5
  teclas ret; sleep 1; textoz u1$hc
  [ $hc = xhci ] && [ -n "${CAPTURAS_A:-}" ] && monz "screendump $G/i2-discos.ppm" 0.5
  teclas ret; sleep 1
  [ $hc = xhci ] && [ -n "${CAPTURAS_A:-}" ] && monz "screendump $G/i3-listo.ppm" 0.5
  teclas ret
  espera_a "^instalar: terminado" 60 $Z/i$hc.sal; sleep 0.5; textoz u2$hc
  [ $hc = xhci ] && [ -n "${CAPTURAS_A:-}" ] && monz "screendump $G/i4-final.ppm" 0.5
  teclas ret; sleep 1.5
  kill -0 $QPID 2> /dev/null && monz quit 0.5
  wait $QPID 2> /dev/null; QPID=
  tr -d '\r' < $Z/i$hc.sal | iconv -f CP437 -t UTF-8 > $Z/i$hc.txt
  S=$Z/i$hc.txt
  if grep -q "Instalar Hispa-Pentium 0.8 en el disco duro" $Z/m$hc.txt && grep -q "^| │\*Instalar Hispa-Pentium 0.8 en el disco duro" $Z/m2$hc.txt; then
    ok "$que: el menú de la memoria USB trae la entrada «Instalar Hispa-Pentium 0.8 en el disco duro» (la última)"
  else ko "$que: el menú de la memoria USB no trae la entrada de instalar:"; sed 's/^/        /' $Z/m2$hc.txt | head -14; fi
  mira "$que: arrancando de la memoria USB con «instalar», el núcleo lanza INSTALAR con lo que trae el cargador (la unidad M:) y se instala en el disco SATA" \
    "^usb: controladora $que " "^disco 0: QEMU HARDDISK, 2048 MB, sin FAT (SATA)\.$" "la del sistema, M:$" "^lanzo INSTALAR.BIN del disco" \
    "^instalar: disco 0, SATA, 2048 MB, se puede: 1$" "^instalar: disco 1, USB, 64 MB, se puede: 0$" "^instalar: partición 1, desde el sector 2048, 4192256 sectores$" \
    "^instalar: copiado CORE.IMG" "^instalar: terminado$" "^reiniciar$"
  if grep -q "Disco 1   SATA  QEMU HARDDISK  *2048 MB" $Z/u1$hc.txt && grep -q "Memoria USB  QEMU    QEMU HARDDISK  *64 MB  (no se toca)" $Z/u1$hc.txt \
    && [ "$(grep -c '^|    Disco ' $Z/u1$hc.txt)" = 1 ] && grep -q "Hispa-Pentium está instalado" $Z/u2$hc.txt && cmp -s $Z/u$hc.img $G/usb.img; then
    ok "$que: la memoria USB de arranque no se ofrece como disco (se enseña: «no se toca») y, después de instalar, está idéntica octeto a octeto"
  else ko "$que: la memoria USB de arranque se ofrece como disco, o ha cambiado al instalar:"; sed 's/^/        /' $Z/u1$hc.txt | sed -n 5,12p; fi
  if volumen $Z/d$hc.img 2048 && instalado $Z/d$hc.img usb; then ok "$que: el disco de 2 GB queda con su FAT32 sana (fsck.fat), el sistema en \\HISPA y el arranque puesto"
  else ko "$que: el disco instalado desde la memoria USB no queda bien:"; sed 's/^/        /' $Z/fsck.txt | head -6; fi
  # lo instalado, apuntado para PAQUETES: los registros de los paquetes del sistema, en \HISPA\PAQUETES
  rm -rf $Z/pqi $Z/pqu; mkdir $Z/pqi $Z/pqu
  mcopy -n -i $Z/vol.img '::HISPA/PAQUETES/*' $Z/pqi/ > /dev/null 2>&1; mcopy -n -i $G/usb.img@@1M '::INSTALA/PAQUETES/*' $Z/pqu/ > /dev/null 2>&1
  if [ "$(ls $Z/pqi | tr '\n' ' ')" = "BASE.PQI BLOC.PQI CONSOLA.PQI EJEMPLOS.PQI EQUIPO.PQI HVPRUN.PQI OFICINA.PQI PAQUETES.PQI RED.PQI SSH.PQI TAILSCAL.PQI TLS.PQI WIN32.PQI " ] && diff -r $Z/pqi $Z/pqu > /dev/null \
    && mcopy -n -i $Z/vol.img ::HISPA/PAQUETES.BIN $Z/f.bin > /dev/null 2>&1; then
    ok "$que: INSTALAR apunta lo que instala: los registros de los 13 paquetes del sistema, en C:\\HISPA\\PAQUETES (y PAQUETES.BIN en \\HISPA)"
  else ko "$que: lo instalado no queda apuntado en C:\\HISPA\\PAQUETES:"; ls $Z/pqi | sed 's/^/        /' | head -10; fi
  # SOLO el disco: el primer arranque
  desde_disco $Z/d$hc.img $Z/a$hc.sal
  espera_menu "Hispa-Pentium: el interprete" a0$hc
  espera_a "asistente: el primer arranque" 50 $Z/a$hc.sal; sleep 1.5
  monz "screendump $G/a1$hc.ppm" 0.6
  if [ $hc = xhci ]; then
    # el nombre, el del equipo, la hora (las 07:45), el teclado como está y la pantalla a 1024 x 600
    teclas shift-c a r l o s tab backspace backspace backspace backspace backspace shift-t o s h i b a tab tab backspace backspace backspace backspace backspace 0 7 shift-dot 4 5 tab tab right
    monz "screendump $G/a2$hc.ppm" 0.6
    teclas ret; sleep 2
    monz "screendump $G/a3$hc.ppm" 0.6
    teclas ret; sleep 1
    monz "mouse_move -360 285" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 1
    monz "screendump $G/a4$hc.ppm" 0.6
  else
    # con el ratón: clic en «de EE. UU.», se teclea el nombre (con el teclado de EE. UU. ya puesto) y clic en Aceptar
    monz "mouse_move 30 12" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 0.6
    monz "mouse_move -100 -136" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 0.6
    teclas shift-a n a shift-semicolon
    monz "screendump $G/a2$hc.ppm" 0.6
    monz "mouse_move 150 266" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 2
    monz "screendump $G/a3$hc.ppm" 0.6
  fi
  monz quit 0.5; wait $QPID 2> /dev/null; QPID=
  tr -d '\r' < $Z/a$hc.sal | iconv -f CP437 -t UTF-8 > $Z/a$hc.txt
  S=$Z/a$hc.txt
  if grep -q "GNU GRUB" $Z/a0$hc.txt && grep -q "^| │\*Hispa-Pentium  " $Z/a0$hc.txt && grep -q "Hispa-Pentium: el interprete de ordenes (modo texto)" $Z/a0$hc.txt; then
    ok "$que: SOLO con el disco (sin la memoria USB), la BIOS arranca GRUB desde su MBR y sale el menú de \\HISPA\\GRUB.CFG"
  else ko "$que: el disco instalado no arranca solo (no sale su menú):"; sed 's/^/        /' $Z/a0$hc.txt | head -8; fi
  mira "$que: el primer arranque desde el disco: el núcleo lo lee (C:, FAT32), el escritorio a 800 x 600 y el asistente (de \\HISPA)" \
    "^disco 0: QEMU HARDDISK, 2048 MB, FAT32 (SATA)\.$" "^escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)" \
    "^escritorio: lanzo .HISPA.ASISTE.BIN" "asistente: el primer arranque$"
  miracap "$que: el asistente del primer arranque, en su ventana: el nombre, el equipo, la fecha y la hora, el teclado y la pantalla" a1$hc \
    negrita 114 64 122 72 "Bienvenido a Hispa-Pentium" texto 112 164 120 172 "Su nombre:" texto 290 198 298 206 "HISPA" texto 112 266 120 274 "Hora (horas:minutos):" \
    texto 292 334 300 342 "800 x 600" negrita 454 427 462 435 "Aceptar"
  volumen $Z/d$hc.img 2048; sano=$?
  rm -f $Z/ini.txt $Z/cfg.txt; mcopy -n -i $Z/vol.img ::HISPA/HISPA.INI $Z/ini.txt > /dev/null 2>&1; mcopy -n -i $Z/vol.img ::HISPA/GRUB.CFG $Z/cfg.txt > /dev/null 2>&1
  if [ $hc = xhci ]; then
    miracap "el asistente, con el teclado: el nombre, el equipo, la hora y la pantalla (Tab de casilla en casilla; la flecha cambia la medida)" a2$hc \
      texto 290 164 298 172 "Carlos" texto 290 198 298 206 "Toshiba" texto 290 266 298 274 "07:45" texto 292 334 300 342 "1024 x 600"
    miracap "al aceptar: «Todo listo», con el saludo; y el reloj de la barra ya marca la hora puesta (el reloj del PC)" a3$hc \
      negrita 154 98 162 106 "Todo listo" negrita 112 176 120 184 "¡Hola, Carlos!" negrita 454 427 462 435 "Empezar" negrita 740 570 760 584 "07:45"
    miracap "el menú Inicio saluda con el nombre («Hola, Carlos») y dice la versión; LEEME.TXT, en los documentos" a4$hc \
      negrita 54 104 62 112 "Hola, Carlos" texto 54 122 62 130 "Hispa-Pentium, versión 0.8" negrita 256 159 264 167 "Mis documentos" texto 38 246 46 254 Naves
    if grep -q "asistente: guardado HISPA.INI (Carlos, Toshiba)$" $S && [ "$(tr -d '\r' < $Z/ini.txt | tr '\n' '|')" = "[hispa]|usuario=Carlos|equipo=Toshiba|teclado=es|modo=1024x600|" ] \
      && grep -q "^set modo=1024x600x32,1024x600,auto$" $Z/cfg.txt && [ "$(grep -c '^set modo=' $Z/cfg.txt)" = 1 ] && [ $sano = 0 ]; then
      ok "lo guardado: C:\\HISPA\\HISPA.INI (usuario, equipo, teclado y modo) y la medida de la pantalla en el menú de arranque (GRUB.CFG); el disco, sano"
    else ko "el asistente no deja bien HISPA.INI o GRUB.CFG:"; cat $Z/ini.txt | sed 's/^/        /'; grep "set modo" $Z/cfg.txt | sed 's/^/        /'; sed 's/^/        /' $Z/fsck.txt | head -4; grep "asistente:" $S | sed 's/^/        /' | head -4; echo "        (fsck: $sano)"; fi
    # el segundo arranque: ya sin asistente, a 1024 x 600 y con el saludo
    desde_disco $Z/d$hc.img $Z/b$hc.sal
    espera_a "escritorio: en marcha" 50 $Z/b$hc.sal; sleep 3
    monz "mouse_move -472 285" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 1
    monz "screendump $G/b1$hc.ppm" 0.6
    monz quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $Z/b$hc.sal | iconv -f CP437 -t UTF-8 > $Z/b$hc.txt
    S=$Z/b$hc.txt
    if grep -q "^escritorio: en marcha, 1024 x 600 con 32 bits por punto" $S && ! grep -q "ASISTE.BIN" $S && cap $G/b1$hc.ppm tam 1024 600 negrita 54 104 62 112 "Hola, Carlos"; then
      ok "el segundo arranque: sin asistente, el escritorio a 1024 x 600 (la medida elegida, puesta por GRUB) y el menú Inicio con el saludo"
    else ko "el segundo arranque no sale como se dejó:"; grep "escritorio" $S | sed 's/^/        /' | head -4; sed 's/^/        /' $G/cap.txt | head -4; fi
  else
    miracap "el asistente, con el ratón: clic en «de EE. UU.» cambia el teclado en el momento (la tecla de la eñe da «:»), y clic en Aceptar" a2$hc \
      texto 290 164 298 172 "Ana:"
    miracap "y queda «Todo listo» con su saludo" a3$hc negrita 154 98 162 106 "Todo listo" negrita 112 176 120 184 "¡Hola, Ana:!"
    if grep -q "asistente: guardado HISPA.INI (Ana:, HISPA)$" $S && grep -q "^teclado=us" $Z/ini.txt && grep -q "^modo=800x600" $Z/ini.txt && grep -q "^set modo=800x600x32,800x600,auto$" $Z/cfg.txt; then
      ok "lo guardado con el ratón: el teclado de EE. UU. y la pantalla como estaba"
    else ko "el asistente, con el ratón, no guarda lo elegido:"; cat $Z/ini.txt | sed 's/^/        /'; fi
    # la segunda entrada del menú del disco: el intérprete de órdenes, en modo texto, con el teclado guardado
    desde_disco $Z/d$hc.img $Z/b$hc.sal
    espera_menu "Hispa-Pentium: el interprete" b0$hc; teclas down ret
    espera_a "lanzo ORDENES.BIN" 40 $Z/b$hc.sal; sleep 1
    teclas v e r ret d i r spc backslash h i s p a backslash h i s p a dot i n i ret; sleep 1.5; textoz b1$hc
    # y PAQUETES, en el sistema instalado: lo instalado, y que sus archivos son los que se instalaron
    teclas c l s ret p a q u e t e s spc l i s t a ret p a q u e t e s spc c o m p r u e b a ret
    espera_a "^Los archivos de los\|no est.n como se instalaron" 60 $Z/b$hc.sal; sleep 0.5; textoz b2$hc
    monz quit 0.5; wait $QPID 2> /dev/null; QPID=
    if grep -q "Hispa-Pentium 0.8: un núcleo de 32 bits escrito en Castel" $Z/b1$hc.txt && grep -q "^|C:\\\\>dir \\\\hispa\\\\hispa.ini" $Z/b1$hc.txt && grep -q "^|.hispa.hispa.ini es un archivo de [0-9]* octetos" $Z/b1$hc.txt; then
      ok "la entrada de modo texto del disco: el intérprete de órdenes (de C:\\HISPA), en C:, con el teclado guardado en HISPA.INI (el de EE. UU.: su tecla «\\» da la barra)"
    else ko "la entrada de modo texto del disco no arranca el intérprete:"; sed 's/^/        /' $Z/b1$hc.txt | grep -v '^ *|$' | tail -n 8; fi
    if grep -q "^|  win32     1.0     Programas de Windows de 32 bits" $Z/b2$hc.txt && grep -q "^|  tailscal  0.8.4   Tailscale y WireGuard" $Z/b2$hc.txt && grep -q "^|13 paquetes\.$" $Z/b2$hc.txt \
      && grep -q "^|Los archivos de los 13 paquetes instalados están como se instalaron\.$" $Z/b2$hc.txt; then
      ok "PAQUETES en el sistema recién instalado: LISTA dice los 13 paquetes del sistema (..., ssh, tailscal, tls y win32: con 13, base ya no cabe en la pantalla) y COMPRUEBA, que sus archivos son los instalados (SHA-256)"
    else ko "PAQUETES en el sistema recién instalado no ve bien lo instalado:"; sed 's/^/        /' $Z/b2$hc.txt | grep -v '^ *|$' | tail -n 12; fi
    # la oficina, en el disco instalado: el grupo «Oficina» del menú Inicio
    # (Facturas, con C:\\HISPA\\HVPRUN.BIN) y Mi equipo abriendo Cuentas de C:\\OFICINA
    desde_disco $Z/d$hc.img $Z/c$hc.sal
    espera_a "escritorio: en marcha" 50 $Z/c$hc.sal; sleep 3
    monz "mouse_move -360 285" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 1
    monz "screendump $G/c1$hc.ppm" 0.6
    monz "mouse_move 20 -150" 0.2; monz "mouse_move 20 -147" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 5
    monz "screendump $G/c2$hc.ppm" 0.6
    # (los saltos grandes, en dos: con la rueda, el ratón PS/2 manda cuatro
    # octetos y en la cola de QEMU, de 16, caben 4 paquetes de 127 puntos por
    # suceso; lo que sobra llegaría con el clic siguiente)
    monz "mouse_move 327 -127" 0.2; monz "mouse_move 327 -127" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 2
    monz "mouse_move -336 -2" 0.2; monz "mouse_move -336 -2" 0.4; monz "mouse_button 1" 0.1; monz "mouse_button 0" 0.1; monz "mouse_button 1" 0.1; monz "mouse_button 0" 3
    teclas ret; sleep 2; teclas o ret; sleep 2
    monz "screendump $G/c3$hc.ppm" 0.6
    teclas c ret; sleep 5
    monz "screendump $G/c4$hc.ppm" 0.6
    monz quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $Z/c$hc.sal | iconv -f CP437 -t UTF-8 > $Z/c$hc.txt
    S=$Z/c$hc.txt
    volumen $Z/d$hc.img 2048; sano=$?
    if grep -q "escritorio: lanzo .HVPRUN.BIN (la tarea [0-9]*)" $S && ! grep -q "ASISTE.BIN" $S \
      && cap $G/c1$hc.ppm texto 38 276 46 284 Facturas texto 38 246 46 254 Naves negrita 54 104 62 112 "Hola, Ana:" \
      && cap $G/c2$hc.ppm negrita 72 22 84 30 Facturas texto 62 126 74 138 "No hay facturas emitidas."; then
      ok "la oficina en el disco instalado: el menú Inicio (con su saludo) tiene el grupo «Oficina», y Facturas se abre con C:\\HISPA\\HVPRUN.BIN (FACTURAS.HVP, de C:\\OFICINA)"
    else ko "la oficina no se abre desde el menú Inicio del disco instalado:"; sed 's/^/        /' $G/cap.txt | head -4; grep "lanzo" $S | sed 's/^/        /' | head -4; fi
    if cap $G/c3$hc.ppm negrita 29 9 39 19 "C:\\OFICINA" texto 213 166 223 176 "CUENTAS.HV" texto 311 166 321 176 "DICCIO.DAT" \
      && cap $G/c4$hc.ppm negrita 80 16 110 36 "Cuentas - (sin nombre)" && [ $sano = 0 ]; then
      ok "y Mi equipo entra en C:\\OFICINA y abre CUENTAS.HVP (Cuentas, con HVPRUN); el disco queda sano"
    else ko "Mi equipo no abre la oficina de C:\\OFICINA en el disco instalado:"; sed 's/^/        /' $G/cap.txt | head -4; sed 's/^/        /' $Z/fsck.txt | head -3; fi
  fi
done
else ko "usb.sh no puede hacer la imagen: $(tail -n 1 $G/usb.txt)"; fi
else
  echo "  (sin root o sin grub-install para PC no se prueba la instalación en el disco)"
fi
fi   # SOLO=08

# ------------------------------------------------- 11. El USB en caliente
# Lo que se enchufa y se desenchufa con el equipo en marcha (usbcal.cas),
# por el monitor de QEMU: «:monitor drive_add ...» y «:monitor device_add
# usb-storage...» enchufan una memoria; «:monitor device_del ...», la quitan.
if parte08 caliente; then
echo "== Hispa-Pentium 0.8: el USB en caliente (enchufar y quitar memorias, teclados, ratones y concentradores en marcha) =="
Q=$P/n08
mkdir -p $Q $G
[ -f $P/d16.img ] || $AQUI/nucleo32/disco.sh $P/d16.img 16 > $P/disco16.txt 2>&1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Q/semilla.asm > /dev/null 2>&1 && nasm -f bin $Q/semilla.asm -o $Q/SEMILLA.BIN
# una memoria con un texto, y otras vacías (FAT16, 16 MB)
memoria() { rm -f $1; dd if=/dev/zero of=$1 bs=1M count=16 status=none; mkfs.fat -F 16 -n ${2:-MEMORIA} $1 > /dev/null; }
memoria $Q/m1.img MEMORIA1
printf 'hola desde la memoria enchufada\r\n' > $Q/HOLA.TXT
MTOOLS_SKIP_CHECK=1 mcopy -i $Q/m1.img $Q/HOLA.TXT ::HOLA.TXT
# 1 y 2. en xHCI y en EHCI, con el intérprete de órdenes (del disco IDE) y sin
# teclado ni ratón PS/2: un teclado que llega (y se va, y vuelve); una
# memoria que se lee y que se quita a lo bruto con un archivo abierto (PRUEBA
# abierto); otra, que se escribe y se quita con seguridad; y en el xHCI, un
# concentrador con un teclado y un ratón detrás, que se quita entero
for hc in xhci ehci; do
  case $hc in xhci) dev="qemu-xhci,id=u"; que="xHCI" ;; ehci) dev="usb-ehci,id=u"; que="EHCI" ;; esac
  cp $P/d16.img $Q/c$hc.img; cp $Q/m1.img $Q/a$hc.img; memoria $Q/b$hc.img MEMORIA2
  CONC=(":pausa 0.1")
  [ $hc = xhci ] && CONC=(":monitor device_del k1" ":pausa 2" ":monitor device_add usb-hub,bus=u.0,port=3,id=h1" ":pausa 2" ":monitor device_add usb-kbd,bus=u.0,port=3.1,id=k2" \
    ":monitor device_add usb-mouse,bus=u.0,port=3.2,id=r2" ":pausa 3" "eco dos" ":monitor device_del h1" ":pausa 2" ":monitor device_add usb-kbd,bus=u.0,id=k1" ":pausa 2")
  ESCRIBE=1 DISCO=$Q/c$hc.img MODULOS= MAQUINA=pc,i8042=off ARRANQUE=ordenes QEMU_MAS="-device $dev" \
    $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 1" ":monitor device_add usb-kbd,bus=u.0,id=k1" ":pausa 2" "eco uno" \
    ":monitor drive_add 0 if=none,id=ma,file=$Q/a$hc.img,format=raw" ":monitor device_add usb-storage,bus=u.0,drive=ma,id=mema" ":pausa 3" \
    "type w:\\hola.txt" "prueba abierto w:\\hola.txt" ":pausa 1" ":monitor device_del mema" ":pausa 2" ":tecla ret" ":pausa 1" "dir w:" \
    "${CONC[@]}" ":monitor drive_add 0 if=none,id=mb,file=$Q/b$hc.img,format=raw" ":monitor device_add usb-storage,bus=u.0,drive=mb,id=memb" ":pausa 3" \
    "copy textos\\largo.txt w:\\largo.txt" "md w:\\nueva" "copy leeme.txt w:\\nueva\\leeme.txt" "eco tres" salir "quitar w" "disco" \
    ":monitor device_del memb" ":pausa 2" usb apagar > $Q/$hc.sal 2>&1
  S=$Q/$hc.sal
  mira "$que: un teclado USB enchufado con el intérprete en marcha teclea al momento" \
    "usb: se enchufa el aparato 0, 0627:0001, velocidad [a-z]*, en el puerto [0-9] ($que); teclado$" "eco uno$" "^uno$"
  mira "$que: una memoria enchufada en marcha sale como W: (y el aviso) y se lee" \
    "($que); almacenamiento$" "^disco 2: QEMU    QEMU HARDDISK, 16 MB (USB), recién enchufado, W: (FAT16)\.$" "usb: aviso: Memoria USB conectada: W:$" \
    "^hola desde la memoria enchufada$"
  mira "$que: quitada a lo bruto con un archivo abierto: el programa ve errores y sigue, y la unidad ya no está" \
    "^abierto: leídos 16 octetos; pulse una tecla$" "usb: aviso: Memoria USB desconectada sin «quitar con seguridad»: W:$" "^abierto: al leer otra vez: el disco falla$" \
    "^abierto: al escribir: el disco falla$" "^abierto: al abrirlo otra vez: no hay disco" "^abierto: sigo en marcha, y acabo$" "^No puedo abrir w:: no hay disco"
  if [ $hc = xhci ]; then
    mira "$que: un concentrador enchufado en marcha, con un teclado y un ratón detrás: el teclado teclea; y al quitarlo se va todo, y otro teclado vuelve a ir" \
      "usb: se desenchufa el aparato 0$" ": concentrador de 8 puertos$" "en el puerto 1 del concentrador [0-9]*; teclado$" "en el puerto 2 del concentrador [0-9]*; ratón$" \
      "usb: 1 teclados, 1 ratones\.$" "^dos$" "usb: 1 teclados, 0 ratones\.$" "^tres$"
  else
    mira "$que: el teclado sigue tecleando después de quitar y enchufar memorias" "^tres$"
  fi
  mira "$que: la otra memoria (W: otra vez) se escribe y se quita con seguridad: «Ya puede quitar la memoria W:»" \
    "^ *1 archivo(s) copiado(s)\.$" "^> quitar w$" "^disco 2: quitado con seguridad\.$" "usb: aviso: Ya puede quitar la memoria W:$"
  saca $Q/b$hc.img LARGO.TXT $Q/largo$hc.txt; saca $Q/b$hc.img NUEVA/LEEME.TXT $Q/leeme$hc.txt
  if sano $Q/b$hc.img && sano $Q/a$hc.img && [ "$(wc -c < $Q/largo$hc.txt 2> /dev/null)" = 17600 ] && [ -s $Q/leeme$hc.txt ] && [ "$(grep -c "sin «quitar con seguridad»" $S)" = 1 ]; then
    ok "$que: lo escrito está en la memoria quitada con seguridad, y las dos quedan sanas (fsck.fat)"
  else ko "$que: la memoria quitada con seguridad no queda bien:"; sed 's/^/        /' $P/fsck.txt | head -6; fi
done
# 3. un teclado de USB 1.1 enchufado en un EHCI con compañeras: va a la UHCI
cp $P/d16.img $Q/cu.img
ESCRIBE=1 DISCO=$Q/cu.img MODULOS= MAQUINA=pc,i8042=off ARRANQUE=ordenes QEMU_MAS="-device ich9-usb-ehci1,id=u -device ich9-usb-uhci1,masterbus=u.0,firstport=0 -device ich9-usb-uhci2,masterbus=u.0,firstport=2" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 1" ":monitor device_add usb-kbd,bus=u.0,port=1,usb_version=1,id=k1" ":pausa 3" "eco uno" \
  ":monitor device_del k1" ":pausa 2" ":monitor device_add usb-kbd,bus=u.0,port=3,usb_version=1,id=k2" ":pausa 3" "eco dos" salir apagar > $Q/uhci.sal 2>&1
S=$Q/uhci.sal
mira "UHCI: un teclado de USB 1.1 enchufado en marcha en un puerto del EHCI pasa a su compañera y teclea; quitado y enchufado en otro, también" \
  "usb: se enchufa el aparato 0, 0627:0001, velocidad plena, en el puerto 1 (UHCI); teclado$" "^uno$" "usb: se desenchufa el aparato 0$" "^dos$"
# 4. la memoria del sistema (X:, de la que se ha arrancado) no se deja quitar
cp $P/d16.img $Q/x.img
ESCRIBE=1 MODULOS= ARRANQUE="escritura=usb unidad=X" QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$Q/x.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN "quitar x" "quitar c" "disco" apagar > $Q/x.sal 2>&1
S=$Q/x.sal
mira "la memoria del sistema (X:) no se deja quitar con seguridad: lo dice, y sigue montada" \
  "usb: aviso: X: es la memoria del sistema: no se puede quitar$" "usb: aviso: No hay unidad C:$" "^unidades: X: (disco 0, USB, FAT16); la del sistema, X:$"
# 5. en el escritorio (EHCI, con el teclado USB y sin ratón): un ratón que
# llega abre Mi equipo; una memoria que llega sale en Mi equipo (que se pone
# al día solo), en el globo de los avisos y en la bandeja; «Quitar con
# seguridad» desde Mi equipo (botón secundario en la unidad) y desde la bandeja
cp $P/d16.img $Q/ce.img; cp $Q/m1.img $Q/ea.img; memoria $Q/eb.img MEMORIA3
CAPTURAS=$G ESCRIBE=1 DISCO=$Q/ce.img MODULOS= MAQUINA=pc,i8042=off ARRANQUE="escritorio escritura" QEMU_MAS="-device usb-ehci,id=u -device usb-kbd,bus=u.0" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":monitor device_add usb-mouse,bus=u.0,id=r1" ":pausa 2" ":doble 62 30" ":pausa 2" \
  ":monitor drive_add 0 if=none,id=ma,file=$Q/ea.img,format=raw" ":monitor device_add usb-storage,bus=u.0,drive=ma,id=mema" ":pausa 2.5" ":captura u1" \
  ":clicd 330 222" ":pausa 0.5" ":captura u2" ":clic 370 257" ":pausa 1.5" ":captura u3" ":monitor device_del mema" ":pausa 2" \
  ":monitor drive_add 0 if=none,id=mb,file=$Q/eb.img,format=raw" ":monitor device_add usb-storage,bus=u.0,drive=mb,id=memb" ":pausa 2.5" ":raton 600 450" ":pausa 6" \
  ":clic 720 583" ":pausa 0.5" ":captura u4" ":clic 680 549" ":pausa 1.5" ":captura u5" ":monitor device_del memb" ":pausa 2" > $Q/escritorio.sal 2>&1
S=$Q/escritorio.sal
mira "escritorio: un ratón USB enchufado en marcha mueve la flecha y abre Mi equipo con dos clics" \
  "usb: se enchufa el aparato 1, 0627:0001, velocidad alta, en el puerto 2 (EHCI); ratón$" "^escritorio: lanzo .EQUIPO.BIN"
miracap "escritorio: la memoria que llega sale en el globo de los avisos, en la bandeja (su icono) y en Mi equipo, que se pone al día solo" u1 \
  texto 570 532 590 548 "Memoria USB conectada: W:" negrita 570 510 590 526 "USB" negrita 216 190 460 212 "(W:)" rgb 721 585 2463C2 40
miracap "escritorio: Mi equipo, botón secundario en la memoria: «Abrir» y «Quitar con seguridad»" u2 texto 335 242 355 267 "Quitar con seguridad"
miracap "escritorio: quitada con seguridad desde Mi equipo: el globo «Ya puede quitar la memoria W:», y en Mi equipo ya no está" u3 \
  texto 540 532 600 548 "Ya puede quitar la memoria W:" texto 100 474 130 490 "Ya puede quitar la memoria W:." notexto 216 190 460 212 "(W:)"
miracap "escritorio: el icono de la bandeja saca «Quitar con seguridad W:» de la otra memoria" u4 texto 580 535 600 560 "Quitar con seguridad W:"
mira "escritorio: y desde la bandeja también se quita con seguridad; las dos memorias quedan sanas" \
  "^escritorio: globo: Ya puede quitar la memoria W:$" "^disco 2: quitado con seguridad\.$"
if [ "$(grep -c "^escritorio: globo: Ya puede quitar la memoria W:$" $S)" = 2 ] && ! grep -q "sin «quitar con seguridad»" $S && sano $Q/ea.img && sano $Q/eb.img; then
  ok "escritorio: las dos memorias se han quitado con seguridad (ninguna «a lo bruto») y quedan sanas (fsck.fat)"
else ko "escritorio: las memorias no se han quitado con seguridad, o no quedan sanas:"; grep "globo\|aviso" $S | sed 's/^/        /' | head -8; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el USB en caliente)"
fi   # el USB en caliente

# ------------------------------------------------- 12. Hispa-Pentium 0.8: los paquetes
# El programa PAQUETES (upaquete.cas) en un sistema «instalado» (un disco SATA
# con C:\HISPA y su base de lo instalado, C:\HISPA\PAQUETES) y una memoria USB
# (X:) con paquetes .HPQ y su CATALOGO.TXT, hechos con tools/hpq.py: instalar
# con dependencias (de un archivo y por su nombre), actualizar, lo que no
# vale (una suma mala, un conflicto, quitar lo que otro necesita), quitar,
# comprobar lo instalado, un corte a mitad (se deshace al volver) y el gancho
# de la red (con un DESCARGA de mentira). Después de cada paso, fsck.fat y
# mtools miran el disco desde fuera.
if parte08 paquetes; then
echo "== Hispa-Pentium 0.8: los paquetes (PAQUETES, .HPQ): instalar con dependencias, actualizar, quitar, deshacer un corte =="
Q=$P/n08p
mkdir -p $Q/f/paq
HPQ="python3 -I $AQUI/../tools/hpq.py"
export MTOOLS_SKIP_CHECK=1
hechos=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Q/semilla.asm > /dev/null 2>&1 && nasm -f bin $Q/semilla.asm -o $Q/SEMILLA.BIN || hechos=0
# (PAQUETES, con una clave de prueba: la pública, en un clave.cas al lado de
# una copia de upaquete.cas, que el compilador encuentra antes que el de
# nucleo32; la privada, solo aquí: firma los catálogos de prueba)
$HPQ clave $Q/prueba.key > /dev/null && $HPQ publica $Q/prueba.key $Q/clave.cas > /dev/null || hechos=0
cp $AQUI/nucleo32/upaquete.cas $Q/upaquete.cas
for p in uordenes:ORDENES upaquete:PAQUETES uactual:ACTUALIZ uhola:HOLA; do
  fu=$AQUI/nucleo32/${p%%:*}.cas; [ ${p%%:*} = upaquete ] && fu=$Q/upaquete.cas
  python3 -I $AQUI/ccastel.py $fu -o $Q/${p%%:*}.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $Q/${p%%:*}.asm -o $Q/f/${p##*:}.BIN || hechos=0
done
# un DESCARGA de mentira, para el gancho de la red: «DESCARGA http://prueba/ARCHIVO
# DESTINO» copia X:\PAQUETES\ARCHIVO a DESTINO (el de verdad lo trae por HTTP)
cat > $Q/udescar.cas <<'CAS'
módulo UDescar
    modelo usuario
    importar Llamadas, Textos32
    variable a: arreglo [400] de byte
    variable url: arreglo [200] de byte
    variable destino: arreglo [200] de byte
    variable origen: arreglo [200] de byte
    variable trozo: arreglo [4096] de byte
    función principal(): byte
        variable m, s, n: nat32
        argumentos(a, 400)
        si no palabra(a, 0, url, 200) o no palabra(a, 1, destino, 200) entonces devolver 2 fin
        si no empieza_por(url, "http://prueba/") entonces devolver 3 fin
        copiar(origen, "X:\\PAQUETES\\")
        concatenar(origen, dirección(url[14]))
        m := abrir(origen)
        si es_error(m) entonces devolver 4 fin
        s := crear(destino)
        si es_error(s) entonces devolver 5 fin
        n := leer(m, trozo, 4096)
        mientras no es_error(n) y n > 0 hacer
            escribir_archivo(s, trozo, n)
            n := leer(m, trozo, 4096)
        fin
        cerrar(s)
        cerrar(m)
        escribir("descarga (de prueba): ", url, " -> ", destino, "\n")
        devolver 0
    fin
fin UDescar.
CAS
python3 -I $AQUI/ccastel.py $Q/udescar.cas -o $Q/udescar.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $Q/udescar.asm -o $Q/f/DESCARGA.BIN || hechos=0
# los paquetes de prueba: letras (1.0 y 1.1), saludo (1.0, que necesita
# letras >= 1.0, base y el núcleo; 2.0, que necesita letras >= 1.1, cambia
# un archivo, trae otro y deja de traer el grande), choque (no puede estar con
# saludo) y malo (con un octeto cambiado: su suma no es la suya)
F=$Q/f
printf 'letras A\r\n' > $F/A.TXT; printf 'letras B (1.0)\r\n' > $F/B.TXT; printf 'letras B (1.1)\r\n' > $F/B11.TXT
printf 'saludo, la 2\r\n' > $F/S2.TXT; head -c 70000 /dev/urandom > $F/GRANDE.DAT
( cd $F
  $HPQ crea BASE.HPQ --nombre base --version 0.7 --descripcion 'El sistema base (de prueba)' ORDENES.BIN='\HISPA\ORDENES.BIN' PAQUETES.BIN='\HISPA\PAQUETES.BIN' DESCARGA.BIN='\HISPA\DESCARGA.BIN'
  $HPQ registro BASE.HPQ BASE.PQI
  $HPQ crea paq/LETRAS1.HPQ --nombre letras --version 1.0 --descripcion 'Unas letras' A.TXT='\DOCS\LETRAS\A.TXT' B.TXT='\DOCS\LETRAS\B.TXT'
  $HPQ crea paq/LETRAS11.HPQ --nombre letras --version 1.1 --descripcion 'Unas letras, mejores' A.TXT='\DOCS\LETRAS\A.TXT' B11.TXT='\DOCS\LETRAS\B.TXT'
  $HPQ crea paq/SALUDO1.HPQ --nombre saludo --version 1.0 --descripcion 'Un programa que saluda' --necesita 'letras >= 1.0, base >= 0.7, nucleo >= 0.7' HOLA.BIN='\HISPA\SALUDO.BIN' GRANDE.DAT='\HISPA\SALUDO.DAT'
  $HPQ crea paq/SALUDO2.HPQ --nombre saludo --version 2.0 --descripcion 'Un programa que saluda (la 2)' --necesita 'letras >= 1.1, nucleo >= 0.7' HOLA.BIN='\HISPA\SALUDO.BIN' S2.TXT='\HISPA\SALUDO.TXT'
  $HPQ crea paq/CHOQUE.HPQ --nombre choque --version 1.0 --descripcion 'No puede estar con saludo' --choca 'saludo' A.TXT='\CHOQUE.TXT'
  $HPQ catalogo paq --clave $Q/prueba.key
  $HPQ crea paq/MALO.HPQ --nombre malo --version 1.0 --descripcion 'Estropeado' A.TXT='\MALO1.TXT' GRANDE.DAT='\MALO2.DAT'
  python3 -I -c "
d = bytearray(open('paq/MALO.HPQ', 'rb').read()); d[-5000] ^= 1; open('paq/MALO.HPQ', 'wb').write(d)"
  $HPQ registro paq/SALUDO1.HPQ SALUDO1.PQI; $HPQ registro paq/SALUDO2.HPQ SALUDO2.PQI ) > $Q/hpq.txt 2>&1 || hechos=0
# el disco del sistema (FAT32, con C:\HISPA y lo instalado: base 0.7) y la memoria USB
mkfs.fat -F 32 -s 1 -n HISPA -C $Q/sis.img 65536 > /dev/null && mmd -i $Q/sis.img ::HISPA ::HISPA/PAQUETES \
  && mcopy -i $Q/sis.img $F/ORDENES.BIN $F/PAQUETES.BIN $F/DESCARGA.BIN ::HISPA/ && mcopy -i $Q/sis.img $F/BASE.PQI ::HISPA/PAQUETES/ || hechos=0
mkfs.fat -F 16 -n MEMORIA -C $Q/usb.img 16384 > /dev/null && mmd -i $Q/usb.img ::PAQUETES && mcopy -i $Q/usb.img $F/paq/* ::PAQUETES/ || hechos=0
# (y, para la parte 10, un «base» 0.9 y 0.9.1 que cambian el núcleo:
# \HISPA\NUCLEO.BIN; van en la memoria, fuera del catálogo)
head -c 5000 /dev/urandom > $F/NUC07.BIN; head -c 6000 /dev/urandom > $F/NUC09.BIN; head -c 7000 /dev/urandom > $F/NUC091.BIN
( cd $F
  for v in 09:0.9 091:0.9.1; do
    $HPQ crea BASE${v%%:*}.HPQ --nombre base --version ${v##*:} --descripcion 'El sistema base (de prueba)' ORDENES.BIN='\HISPA\ORDENES.BIN' PAQUETES.BIN='\HISPA\PAQUETES.BIN' DESCARGA.BIN='\HISPA\DESCARGA.BIN' NUC${v%%:*}.BIN='\HISPA\NUCLEO.BIN'
  done ) >> $Q/hpq.txt 2>&1 || hechos=0
mcopy -i $Q/usb.img $F/BASE09.HPQ $F/BASE091.HPQ ::PAQUETES/ || hechos=0
if [ $hechos = 1 ] && $HPQ mira $F/paq/SALUDO1.HPQ > $Q/mira.txt && ! $HPQ mira $F/paq/MALO.HPQ > $Q/mira2.txt && grep -q '^paquete=saludo 2.0 SALUDO2.HPQ 1[0-9]* [0-9a-f]\{64\} Un programa que saluda (la 2)' $F/paq/CATALOGO.TXT \
  && grep -q 'MALO2.DAT  *70000  MAL' $Q/mira2.txt && head -1 $F/BASE.PQI | grep -q '^HPQ 1'; then
  ok "tools/hpq.py: hace los paquetes (crea), su catálogo (CATALOGO.TXT, con el tamaño y el SHA-256 de cada .HPQ), los registros (.PQI) y mira las sumas (la de MALO.HPQ, mal)"
else ko "tools/hpq.py no hace bien los paquetes de prueba:"; tail -n 4 $Q/hpq.txt | sed 's/^/        /'; fi
# PAQUETES en el sistema del disco $1, con la memoria USB: lo que sale, a $Q/$2.sal (y en $S)
paq() {
  local d=$1 n=$2; shift 2
  ESCRIBE=1 SATA=$d MEMORIA=64 MODULOS= ARRANQUE=ordenes QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=${USB_IMG:-$Q/usb.img},format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
    $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 2" "$@" ":pausa 1" > $Q/$n.sal 2>&1
  S=$Q/$n.sal
}
# lo que hay en el disco: la lista entera de mdir y fsck.fat; sin restos de una instalación
lista_de() { mdir -/ -b -i $1 :: 2> /dev/null | sort; }
sin_restos() { ! lista_de $1 | grep -q 'HPQ[0-9]*\.\(NUE\|VIE\)\|DIARIO\.TXT\|HECHO\.TXT'; }
igual_en() { rm -f $Q/x.bin; mcopy -n -i $1 "::$2" $Q/x.bin > /dev/null 2>&1 && cmp -s $Q/x.bin $3; }

# 1. lo instalado, el catálogo de la memoria USB (lo encuentra solo: X:\PAQUETES)
# e instalar un archivo de paquete de la memoria, con lo que necesita (del catálogo)
cp $Q/sis.img $Q/s1.img
paq $Q/s1.img uno "paquetes lista" "paquetes catalogo" 'paquetes instala x:\paquetes\saludo1.hpq' ":pausa 3" "paquetes lista" saludo
mira "PAQUETES LISTA: lo apuntado en C:\\HISPA\\PAQUETES (base 0.7); CATALOGO: lo de la memoria USB (X:\\PAQUETES, que encuentra solo), cada uno con su versión más nueva" \
  "^  base      0.7     El sistema base (de prueba)$" "^1 paquetes\.$" "^Paquetes que hay en X:.PAQUETES.:$" "^  letras    1\.1  *Unas letras, mejores$" \
  "^  saludo    2\.0  *Un programa que saluda (la 2)$"
mira "PAQUETES INSTALA X:\\PAQUETES\\SALUDO1.HPQ: antes, lo que necesita (letras, la más nueva del catálogo; base y el núcleo ya están), comprobando las sumas de todo; y el programa nuevo se lanza por su nombre (PATH con C:\\HISPA)" \
  "^Se va a instalar: letras 1\.1, saludo 1\.0$" "^Las sumas (SHA-256) de todos los archivos están bien\.$" "^  instalado letras 1\.1$" "^  instalado saludo 1\.0$" "^Hecho\.$" \
  "^  saludo    1\.0     Un programa que saluda$" "^3 paquetes\.$" "^hola desde el anillo 3" "^(C:.HISPA.SALUDO.BIN ha acabado con el código 3)$"
if sano $Q/s1.img && sin_restos $Q/s1.img && igual_en $Q/s1.img HISPA/SALUDO.BIN $F/HOLA.BIN && igual_en $Q/s1.img HISPA/SALUDO.DAT $F/GRANDE.DAT \
  && igual_en $Q/s1.img DOCS/LETRAS/A.TXT $F/A.TXT && igual_en $Q/s1.img DOCS/LETRAS/B.TXT $F/B11.TXT && igual_en $Q/s1.img HISPA/PAQUETES/SALUDO.PQI $F/SALUDO1.PQI; then
  ok "lo instalado, desde fuera: fsck.fat da el disco por sano, los archivos son los del paquete octeto a octeto (también en carpetas nuevas: C:\\DOCS\\LETRAS), el registro es la cabecera del paquete y no quedan restos (ni HPQ*.NUE, ni .VIE, ni el diario)"
else ko "lo instalado no queda bien en el disco:"; lista_de $Q/s1.img | sed 's/^/        /' | head -14; tail -n 3 $P/fsck.txt | sed 's/^/        /'; fi

# 2. por su nombre, del catálogo (la versión más nueva, con lo que necesita)
cp $Q/sis.img $Q/s5.img
paq $Q/s5.img nombre "paquetes instala saludo" ":pausa 2" "paquetes instala saludo"
mira "PAQUETES INSTALA saludo: del catálogo, la versión más nueva (2.0), con lo que necesita delante (letras 1.1); y otra vez: ya está" \
  "^Se va a instalar: letras 1\.1, saludo 2\.0$" "^  instalado saludo 2\.0$" "^paquetes: saludo ya está instalado (la 2\.0); para una más nueva: PAQUETES ACTUALIZA$"
if sano $Q/s5.img && sin_restos $Q/s5.img && igual_en $Q/s5.img HISPA/SALUDO.TXT $F/S2.TXT && igual_en $Q/s5.img DOCS/LETRAS/B.TXT $F/B11.TXT; then
  ok "y en el disco: sano, con los dos paquetes octeto a octeto y sin restos"
else ko "instalar por su nombre no deja bien el disco:"; lista_de $Q/s5.img | sed 's/^/        /' | head -14; fi

# 3. actualizar: saludo 1.0 -> 2.0 (cambia un archivo, trae otro, deja de traer el grande)
cp $Q/s1.img $Q/s2.img
paq $Q/s2.img actualiza "paquetes catalogo" "paquetes actualiza" ":pausa 2" "paquetes actualiza" "paquetes comprueba"
mira "PAQUETES ACTUALIZA: el catálogo dice lo que es más nuevo; se pone saludo 2.0 en lugar de la 1.0, y luego todo está al día; COMPRUEBA: los archivos instalados siguen iguales" \
  "^  saludo    2\.0     (nuevo; tiene la 1\.0)    Un programa que saluda (la 2)$" "^  letras    1\.1     (instalado)              Unas letras, mejores$" \
  "^Se va a instalar: saludo 2\.0 (en lugar de la 1\.0)$" "^  instalado saludo 2\.0$" "^Todo está al día\.$" "^Los archivos de los 3 paquetes instalados están como se instalaron\.$"
if sano $Q/s2.img && sin_restos $Q/s2.img && igual_en $Q/s2.img HISPA/SALUDO.TXT $F/S2.TXT && ! igual_en $Q/s2.img HISPA/SALUDO.DAT $F/GRANDE.DAT \
  && igual_en $Q/s2.img HISPA/PAQUETES/SALUDO.PQI $F/SALUDO2.PQI && igual_en $Q/s2.img HISPA/SALUDO.BIN $F/HOLA.BIN; then
  ok "lo actualizado, desde fuera: el disco sano, lo de la 2.0 en su sitio, el archivo que la 2.0 ya no trae (SALUDO.DAT) quitado y el registro, el de la 2.0"
else ko "lo actualizado no queda bien en el disco:"; lista_de $Q/s2.img | sed 's/^/        /' | head -14; tail -n 3 $P/fsck.txt | sed 's/^/        /'; fi

# 4. lo que no vale no toca nada: una suma mala, un conflicto, quitar lo que
# otro necesita; y COMPRUEBA ve un archivo cambiado (la imagen, idéntica)
cp $Q/s1.img $Q/s4.img
printf 'cambiado a mano\r\n' > $Q/cambiado.txt; mcopy -o -i $Q/s4.img $Q/cambiado.txt ::DOCS/LETRAS/A.TXT
cp $Q/s4.img $Q/s40.img
paq $Q/s4.img malo 'paquetes instala x:\paquetes\malo.hpq' "paquetes instala choque" "paquetes quita letras" 'paquetes mira x:\paquetes\malo.hpq' "paquetes comprueba" "paquetes quita nadie" "paquetes instala nadie"
mira "una suma mala (MALO.HPQ, un octeto cambiado): se ve antes de copiar nada, y no se instala nada; MIRA dice lo mismo" \
  "^paquetes: X:.paquetes.malo\.hpq: la suma de .MALO2\.DAT no es la suya: el paquete está estropeado$" "^No se instala nada: el disco queda como estaba\.$" \
  "^  .MALO2\.DAT  *70000 octetos$"
mira "lo que choca (choque no puede estar con saludo), quitar lo que otro necesita (letras, que necesita saludo) y lo que no hay: se dice, y no se hace" \
  "^paquetes: choque no puede estar con saludo, que está instalado: quítelo antes (PAQUETES QUITA saludo)$" "^paquetes: letras no se quita: lo necesita saludo (quite antes saludo, o /FORZAR)$" \
  "^paquetes: nadie no está instalado$" "^paquetes: hace falta nadie y no está en el catálogo$"
if grep -q "^  letras: C:.DOCS.LETRAS.A\.TXT ha cambiado$" $S && grep -q "^1 archivos no están como se instalaron" $S && cmp -s $Q/s4.img $Q/s40.img; then
  ok "COMPRUEBA ve el archivo cambiado a mano (su SHA-256 no es el apuntado); y con todo lo que no valía, el disco queda idéntico octeto a octeto"
else ko "COMPRUEBA no ve el archivo cambiado, o lo que no valía ha tocado el disco:"; grep "cambiado\|como se instalaron" $S | sed 's/^/        /'; fi

# 5. quitar (saludo, y luego letras, que ya nadie necesita): el disco, como al principio
cp $Q/s2.img $Q/s3.img
paq $Q/s3.img quita "paquetes quita saludo letras" "paquetes lista" "dir \\docs"
mira "PAQUETES QUITA saludo letras: quita sus archivos, sus registros y las carpetas que se quedan vacías (C:\\DOCS\\LETRAS y C:\\DOCS)" \
  "^  quitado saludo 2\.0$" "^  quitado letras 1\.1$" "^Hecho\.$" "^1 paquetes\.$" "^No puedo abrir .docs: no existe\.$"
sano $Q/sis.img; antes=$(grep -o '[0-9]*/[0-9]* clusters' $P/fsck.txt)
if sano $Q/s3.img && [ "$(grep -o '[0-9]*/[0-9]* clusters' $P/fsck.txt)" = "$antes" ] && [ "$(lista_de $Q/s3.img)" = "$(lista_de $Q/sis.img)" ]; then
  ok "lo quitado, desde fuera: el disco sano, con los mismos archivos y carpetas que antes de instalar nada y los mismos clusters ocupados ($antes)"
else ko "después de quitarlo todo el disco no queda como al principio:"; diff <(lista_de $Q/s3.img) <(lista_de $Q/sis.img) | sed 's/^/        /' | head -8; fi

# 6. un corte a mitad (como si se fuera la luz): actualizar saludo y pararse
# tras el cuarto cambio (/CORTE=4: lo de antes apartado, lo nuevo a medio
# poner); QEMU se apaga ahí. Al volver, PAQUETES lo deshace
cp $Q/s1.img $Q/s6.img
paq $Q/s6.img corte "paquetes actualiza /corte=4" ":pausa 2"
cp $Q/s6.img $Q/s6corte.img
if grep -q "^paquetes: corte de prueba" $S && lista_de $Q/s6.img | grep -q "DIARIO\.TXT" && lista_de $Q/s6.img | grep -q "HPQ00000\.VIE" && ! lista_de $Q/s6.img | grep -q "SALUDO\.PQI"; then
  ok "un corte a mitad de actualizar (QEMU apagado tras el cuarto cambio): quedan el diario (C:\\HISPA\\PAQUETES\\DIARIO.TXT), lo de antes apartado (HPQ*.VIE) y lo nuevo a medias"
else ko "el corte a mitad no deja lo que se esperaba:"; lista_de $Q/s6.img | sed 's/^/        /' | head -14; fi
paq $Q/s6.img vuelta "paquetes lista"
mira "al volver, PAQUETES ve el diario y lo deshace: saludo sigue en la 1.0" \
  "^paquetes: saludo 2\.0 se quedó a medias (¿se fue la luz?): se ha deshecho y todo está como antes$" "^  saludo    1\.0     Un programa que saluda$"
if sano $Q/s6.img && [ "$(lista_de $Q/s6.img)" = "$(lista_de $Q/s1.img)" ] && igual_en $Q/s6.img HISPA/SALUDO.DAT $F/GRANDE.DAT && igual_en $Q/s6.img HISPA/SALUDO.BIN $F/HOLA.BIN \
  && igual_en $Q/s6.img HISPA/PAQUETES/SALUDO.PQI $F/SALUDO1.PQI && ! lista_de $Q/s6.img | grep -q "SALUDO\.TXT"; then
  ok "lo deshecho, desde fuera: fsck.fat da el disco por sano y está como antes del corte (los mismos archivos, iguales octeto a octeto; sin restos)"
else ko "después del corte el disco no vuelve a como estaba:"; diff <(lista_de $Q/s6.img) <(lista_de $Q/s1.img) | sed 's/^/        /' | head -8; tail -n 3 $P/fsck.txt | sed 's/^/        /'; fi

# 7. el gancho de la red: con /DESDE=http://..., PAQUETES lanza DESCARGA
# (aquí, uno de mentira) para el catálogo y cada paquete; sin DESCARGA, lo dice
cp $Q/sis.img $Q/s7.img
paq $Q/s7.img red "paquetes instala letras /desde=http://prueba/" ":pausa 2" "del \\hispa\\descarga.bin" "paquetes catalogo /desde=http://actualiza.hispados.es/pentium/"
mira "el gancho de la red: con /DESDE=http://..., el catálogo y los paquetes los trae DESCARGA (DESCARGA.BIN, la orden de la red) a C:\\HISPA\\PAQUETES\\BAJADAS, y se instalan igual" \
  "^Trayendo http://prueba/CATALOGO\.TXT\.\.\.$" "^descarga (de prueba): http://prueba/LETRAS11\.HPQ -> C:.HISPA.PAQUETES.BAJADAS.LETRAS11\.HPQ$" "^  instalado letras 1\.1$" \
  "^paquetes: para traer http://actualiza\.hispados\.es/pentium/CATALOGO\.TXT de Internet hace falta la red (el programa DESCARGA),$"
if sano $Q/s7.img && sin_restos $Q/s7.img && igual_en $Q/s7.img DOCS/LETRAS/B.TXT $F/B11.TXT && igual_en $Q/s7.img HISPA/PAQUETES/BAJADAS/LETRAS11.HPQ $F/paq/LETRAS11.HPQ; then
  ok "y en el disco: sano, con lo instalado y lo traído (en BAJADAS) igual que lo de la memoria octeto a octeto"
else ko "lo traído por el gancho de la red no queda bien en el disco:"; lista_de $Q/s7.img | sed 's/^/        /' | head -14; fi

# 8. la ventana «Programas y actualizaciones» (ACTUALIZ.BIN), desde el menú
# Inicio («Actualizar»): la lista, instalar (con lo que necesita) y quitar con
# sus botones; por debajo, PAQUETES
cp $Q/sis.img $Q/s8.img
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\nmodo=800x600\r\n' > $Q/HISPA.INI
mcopy -i $Q/s8.img $F/ACTUALIZ.BIN $Q/HISPA.INI ::HISPA/
CAPTURAS=$G ESCRIBE=1 SATA=$Q/s8.img MEMORIA=64 MODULOS= ARRANQUE=escritorio QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$Q/usb.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":clic 40 585" ":captura pq0" ":clic 300 338" ":pausa 4" ":captura pq1" ":clic 200 236" ":clic 199 471" ":pausa 5" ":captura pq2" \
  ":clic 200 216" ":clic 310 471" ":pausa 4" ":captura pq3" ":clic 600 471" ":pausa 4" ":captura pq4" ":clic 661 71" ":pausa 3" ":captura pq5" > $Q/ventana.sal 2>&1
miracap "el menú Inicio trae «Actualizar» (en la columna de la derecha): abre la ventana «Programas y actualizaciones»" pq0 negrita 256 330 264 338 Actualizar
miracap "la ventana: lo instalado (base 0.7) y lo del catálogo de la memoria USB, con su firma buena (choque, letras, saludo: «(no)» instalados), cada uno con su versión y lo que es; y el botón «Buscar novedades»" pq1 \
  negrita 150 90 154 94 "Programas y actualizaciones" texto 150 109 154 113 'Los paquetes, de X:\PAQUETES\' negrita 112 163 116 167 base \
  texto 208 183 212 187 "(no)" negrita 112 223 116 227 saludo texto 304 223 308 227 "2.0" negrita 132 458 136 462 Actualizarlo \
  negrita 536 456 540 462 "Buscar novedades" negrita 576 107 584 113 "(firma buena)"
miracap "con el botón «Instalarlo», saludo (y antes letras, que lo necesita): lo que dice PAQUETES sale abajo" pq2 \
  texto 112 397 116 401 "  instalado saludo 2.0" texto 112 415 116 419 "Hecho." negrita 112 183 116 187 letras texto 208 183 212 187 "1.1"
miracap "y «Quitarlo», con saludo elegido (sigue elegido aunque la lista cambie de orden): se quita" pq3 \
  texto 112 397 116 401 "  quitado saludo 2.0" negrita 112 203 116 207 choque
miracap "«Buscar novedades» trae el catálogo otra vez (y su firma, buena)" pq4 \
  texto 112 415 116 419 "La firma del catálogo es buena" negrita 112 203 116 207 choque
miracap "maximizada, la ventana se pone a su medida: la lista, los mensajes y los botones, al ancho y al alto de la pantalla, y lo que no cabe, cortado" pq5 \
  negrita 52 32 60 38 "Programas y actualizaciones" negrita 660 50 700 58 "(firma buena)" negrita 182 512 190 522 Quitarlo
if sano $Q/s8.img && sin_restos $Q/s8.img && [ "$(mdir -/ -b -i $Q/s8.img ::HISPA/PAQUETES 2> /dev/null | tr '\n' ' ')" = "::/HISPA/PAQUETES/BASE.PQI ::/HISPA/PAQUETES/LETRAS.PQI " ]; then
  ok "lo hecho desde la ventana, desde fuera: el disco sano, letras instalado, saludo quitado y sin restos (tampoco VENTANA.TXT)"
else ko "lo hecho desde la ventana no queda bien en el disco:"; lista_de $Q/s8.img | sed 's/^/        /' | head -12; fi

# 9. los accesos de los paquetes (accesos.cas; clicgest-pentium, 20/11): un
# .MNU en \HISPA\MENU con su icono .ICP (tools/hpicono.py, de un PNG) sale en
# el menú Inicio, debajo de Mi equipo, y en el tapiz; elegirlo lanza el programa
A9=$Q/acc
mkdir -p $A9
python3 - $A9/rojo.png <<'PY'
import struct, sys, zlib
filas = b''.join(b'\0' + b''.join(bytes((200, 40, 40, 255)) if 4 <= x < 28 and 4 <= y < 28 else bytes(4) for x in range(32)) for y in range(32))
def tr(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + tr(b'IHDR', struct.pack('>IIBBBBB', 32, 32, 8, 6, 0, 0, 0)) + tr(b'IDAT', zlib.compress(filas)) + tr(b'IEND', b''))
PY
if python3 -I $AQUI/../tools/hpicono.py $A9/rojo.png $A9/SALUDO.ICP && [ "$(stat -c %s $A9/SALUDO.ICP)" = 4108 ]; then
  ok "tools/hpicono.py: de un PNG de 32 x 32 (RGBA), el .ICP (HPIC, 32, 32 y los puntos)"
else ko "tools/hpicono.py no hace el .ICP"; fi
$AQUI/nucleo32/disco.sh $A9/d.img 16 > /dev/null 2>&1
printf 'nombre=Saludo\r\nprograma=\\HOLA.BIN\r\nicono=\\HISPA\\MENU\\SALUDO.ICP\r\nescritorio=si\r\n' > $A9/SALUDO.MNU
mmd -i $A9/d.img ::HISPA ::HISPA/MENU 2> /dev/null
mcopy -i $A9/d.img $A9/SALUDO.MNU $A9/SALUDO.ICP ::HISPA/MENU/
CAPTURAS=$G DISCO=$A9/d.img MODULOS= ARRANQUE=escritorio $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":captura ac0" ":clic 40 585" ":pausa 1" ":captura ac1" ":clic 80 258" ":pausa 2" > $A9/sal.txt 2>&1
miracap "un acceso de un paquete (\\HISPA\\MENU\\SALUDO.MNU): su icono (el .ICP, rojo) y su nombre en el tapiz, debajo de los de siempre" ac0 texto 30 270 60 295 Saludo rgb 64 259 C82828
miracap "y en el menú Inicio, el primero de los más usados (MENÚ: mientras no hay cuentas, los de los paquetes)" ac1 texto 38 246 46 254 Saludo negrita 46 155 54 163 "Bloc de notas"
S=$A9/sal.txt
mira "elegirlo en el menú lanza su programa (programa=\\HOLA.BIN)" "escritorio: lanzo .HOLA.BIN"

# 10. los controles de las ventanas (controles.cas; controles-erp, 21/11):
# MUESTRA.BIN, maximizada y con la letra a vez y media: la barra de menús
# (Alt+V), la de herramientas, las pestañas (Ctrl+Tab), una rejilla que se
# ordena (un clic en un rótulo) y se filtra (sin tildes), recuadros, un
# campo de fecha, una desplegable y la barra de estado
C10=$Q/ctl
mkdir -p $C10
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/umuestra.cas -o $C10/m.asm > /dev/null 2>&1 && nasm -f bin $C10/m.asm -o $C10/MUESTRA.BIN
$AQUI/nucleo32/disco.sh $C10/d.img 16 > /dev/null 2>&1
mcopy -i $C10/d.img $C10/MUESTRA.BIN ::
CAPTURAS=$G DISCO=$C10/d.img MEMORIA=64 MODULOS= ARRANQUE=escritorio $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":doble 40 190" ":pausa 2" ":escribe muestra" ":tecla ret" ":pausa 4" \
  ":raton 620 590" ":captura ct1" ":clic 60 217" ":pausa 1" ":raton 620 590" ":captura ct2" ":clic 30 178" ":escribe angel" ":raton 620 590" ":pausa 1" ":captura ct3" \
  ":tecla ctrl-tab" ":pausa 1" ":captura ct4" ":tecla alt-v" ":pausa 1" ":captura ct5" ":tecla esc" ":pausa 1" ":clic 60 112" ":pausa 1" ":raton 620 590" ":captura ct6" \
  ":clic 300 178" ":tecla backspace" ":tecla backspace" ":tecla backspace" ":tecla backspace" ":tecla backspace" ":pausa 1" \
  ":raton 620 590" ":captura cr0" ":arrastra 653 217 713 217" ":pausa 1" ":raton 620 590" ":captura cr1" ":doble 713 217" ":pausa 1" ":raton 620 590" ":captura cr2" \
  ":arrastra 72 217 120 217" ":pausa 1" ":raton 620 590" ":captura cr3" > $C10/sal.txt 2>&1
miracap "MUESTRA.BIN: maximizada (800 x 600), la barra de menús, la de herramientas, las pestañas, la rejilla con sus rótulos y la barra de estado, a vez y media" ct1 \
  tam 800 600 texto15 10 30 140 60 rchivo texto 20 60 120 90 Nuevo texto 90 60 300 90 Guardar negrita15 10 95 120 130 Lista texto15 90 95 300 130 Ficha \
  negrita15 60 195 200 235 Nombre negrita15 400 195 660 235 Importe texto15 0 525 300 560 "Ana (titular)"
miracap "un clic en el rótulo «Nº» ordena la rejilla por él (de menos a más: la 1 arriba)" ct2 texto15 60 230 420 270 "Persona de ejemplo 1"
miracap "filtrar sin mirar tildes ni mayúsculas: «angel» encuentra «Àngel álvarez»" ct3 negrita15 90 230 420 270 "ngel álvarez" texto15 200 525 800 560 "1 renglones"
miracap "Ctrl+Tab: la pestaña Ficha, con su recuadro «Datos generales», los campos, la fecha, la desplegable y la casilla" ct4 \
  negrita15 10 125 300 160 "Datos generales" texto15 20 180 300 215 "Ana Ejemplo" texto15 20 245 300 280 "21/11/2026" texto15 200 245 600 280 "Socio" texto15 40 280 400 320 "Al corriente de pago"
miracap "Alt+V abre el menú «Ver» de la barra (un menú del escritorio)" ct5 texto 100 40 330 160 "Letra muy grande"
miracap "y un clic en la pestaña «Lista» vuelve a ella (pestaña_en: la fila de pestañas que hay en ese punto)" ct6 negrita15 60 195 200 235 Nombre
# los anchos de las columnas (pentium-letras): cada una, lo que mide lo más
# largo (Nombre, la del texto largo, lo que sobra); se arrastra el borde de
# un rótulo (va con el ratón: Importe gana lo que pierde Estado) y dos clics
# en él la ajustan a lo más largo que se ve
miracap "la rejilla reparte el ancho por lo que mide cada columna (Fecha e Importe, lo justo; Nombre, lo que sobra)" cr0 \
  negrita15 560 200 580 235 Importe negrita15 70 200 90 235 Nombre negrita15 425 200 445 235 Fecha
miracap "arrastrar el borde entre Importe y Estado: Importe más ancha (60 puntos), Fecha donde estaba" cr1 negrita15 620 200 640 235 Importe negrita15 425 200 445 235 Fecha distinto 0 200 800 100 $G/cr0.ppm
miracap "dos clics en ese borde: Importe, a lo que mide lo más largo que se ve (lo que sobra, a Nombre)" cr2 negrita15 620 200 640 235 Importe negrita15 485 200 505 235 Fecha
miracap "y el borde entre Nº y Nombre: Nº más ancha" cr3 negrita15 117 200 137 235 Nombre

# 9. la firma (Ed25519, firma.cas): el catálogo de prueba va firmado con la
# clave de prueba (la que lleva este PAQUETES); PAQUETES FIRMA la comprueba;
# lo de Internet sin firma, o con el catálogo cambiado, no se usa
if $HPQ comprueba $F/paq $Q/clave.cas > $Q/firma.txt 2>&1 && ! $HPQ comprueba $F/paq $AQUI/nucleo32/clave.cas >> $Q/firma.txt 2>&1 \
  && grep -q '^ed25519 [0-9a-f]\{128\}' $F/paq/CATALOGO.FIR; then
  ok "tools/hpq.py: hace la clave (la privada, con permisos 600), firma el catálogo (CATALOGO.FIR, Ed25519) y comprueba la firma (buena con su clave pública; mala con la de Hispa-Pentium)"
else ko "tools/hpq.py no firma bien el catálogo:"; sed 's/^/        /' $Q/firma.txt; fi
# una memoria con el catálogo cambiado (una letra de una descripción) y la
# misma firma; y otra sin CATALOGO.FIR
mkdir -p $Q/u2 $Q/u3
cp $F/paq/* $Q/u2/; sed -i 's/Unas letras, mejores/Unas letras, peores/' $Q/u2/CATALOGO.TXT
cp $F/paq/* $Q/u3/; rm $Q/u3/CATALOGO.FIR
for u in 2 3; do mkfs.fat -F 16 -n MEMORIA -C $Q/usb$u.img 16384 > /dev/null && mmd -i $Q/usb$u.img ::PAQUETES && mcopy -i $Q/usb$u.img $Q/u$u/* ::PAQUETES/; done
cp $Q/sis.img $Q/s9.img
paq $Q/s9.img firma 'paquetes firma x:\paquetes\catalogo.txt' ":pausa 3" 'paquetes firma x:\paquetes\letras1.hpq' "paquetes catalogo /desde=http://prueba/" ":pausa 3" "paquetes catalogo /desde=http://prueba/nada/" ":pausa 2"
USB_IMG=$Q/usb2.img paq $Q/s9.img firma2 'paquetes firma x:\paquetes\catalogo.txt' ":pausa 3" "paquetes instala letras /desde=http://prueba/" ":pausa 3" "paquetes catalogo" ":pausa 3"
USB_IMG=$Q/usb3.img paq $Q/s9.img firma3 "paquetes instala letras /desde=http://prueba/" ":pausa 2" "paquetes catalogo" ":pausa 2"
cat $Q/firma2.sal $Q/firma3.sal >> $Q/firma.sal; S=$Q/firma.sal
mira "la firma (Ed25519, en Castel): PAQUETES FIRMA dice si es buena; el catálogo de Internet firmado se usa (y lo dice); cambiado una letra, o sin su CATALOGO.FIR, no se usa nada de él; de una carpeta, sin firma, sí; y si el servidor aún no tiene catálogo (404), lo dice en llano" \
  "^La firma de X:.paquetes.catalogo\.txt es buena (la clave [0-9a-f]\{16\}; [0-9]* tics)\.$" "^paquetes: no está su firma (X:.paquetes.letras1\.FIR), o no se entiende$" \
  "^La firma del catálogo es buena (la clave [0-9a-f]\{16\}; [0-9]* tics)\.$" "^  letras    1\.1  *Unas letras, mejores$" \
  "^La firma de X:.paquetes.catalogo\.txt NO es buena\.$" "^paquetes: la firma del catálogo NO es buena: no se usa nada de él$" \
  "^paquetes: el catálogo de Internet no trae su firma (CATALOGO.FIR): no se usa$" "^  letras    1\.1  *Unas letras, mejores$" \
  "^Todavía no hay actualizaciones publicadas en el servidor\.$"
if [ "$(grep -c '^paquetes: la firma del catálogo NO es buena' $S)" = 2 ] && ! grep -q '^  instalado' $S && sano $Q/s9.img && [ "$(mdir -/ -b -i $Q/s9.img ::HISPA/PAQUETES 2> /dev/null | grep -v BAJADAS | tr '\n' ' ')" = "::/HISPA/PAQUETES/BASE.PQI " ]; then
  ok "y con la firma mala, nada: ni de Internet ni de la carpeta (que trae CATALOGO.FIR); el disco sano, con lo de antes"
else ko "con la firma mala se ha usado algo:"; lista_de $Q/s9.img | sed 's/^/        /' | head -10; fi

# 10. el núcleo: un «base» que cambia \HISPA\NUCLEO.BIN deja el de antes en
# NUCLEO.ANT, GRUBENV con «nuevo=1» (1.024 octetos, como lo quiere GRUB) y
# la entrada del de antes en GRUB.CFG, con lo que hace que GRUB vuelva solo a
# él (una vez, aunque se actualice otra vez); y pide reiniciar
cp $Q/sis.img $Q/s10.img
printf 'set modo=800x600x32,800x600,auto\nset default=0\nmenuentry "Hispa-Pentium" {\n  multiboot /HISPA/NUCLEO.BIN escritorio escritura\n}\n' > $Q/GRUB.CFG
cp $F/NUC07.BIN $Q/NUCLEO.BIN
mcopy -i $Q/s10.img $Q/GRUB.CFG $Q/NUCLEO.BIN ::HISPA/
paq $Q/s10.img nucleo 'paquetes instala x:\paquetes\base09.hpq' ":pausa 3" "paquetes lista" 'paquetes instala x:\paquetes\base091.hpq' ":pausa 3"
mira "PAQUETES con un núcleo nuevo (\\HISPA\\NUCLEO.BIN): lo instala y pide reiniciar; si no arrancara, vuelve el de antes" \
  "^  instalado base 0\.9$" "^reiniciar|Reinicie para usar el núcleo nuevo; si no arranca, vuelve el de antes\.$" "^  base      0\.9 " "^  instalado base 0\.9\.1$"
saca $Q/s10.img HISPA/GRUB.CFG $Q/grub10.cfg; saca $Q/s10.img HISPA/GRUBENV $Q/grubenv10
if sano $Q/s10.img && sin_restos $Q/s10.img && igual_en $Q/s10.img HISPA/NUCLEO.BIN $F/NUC091.BIN && igual_en $Q/s10.img HISPA/NUCLEO.ANT $F/NUC09.BIN \
  && [ "$(stat -c %s $Q/grubenv10)" = 1024 ] && head -c 33 $Q/grubenv10 | grep -q '^nuevo=1$' && [ "$(grep -c -- '--id anterior' $Q/grub10.cfg)" = 1 ] \
  && grep -q '^  multiboot /HISPA/NUCLEO.ANT escritorio escritura$' $Q/grub10.cfg && grep -q '^  load_env -f /HISPA/GRUBENV nuevo$' $Q/grub10.cfg && head -1 $Q/grub10.cfg | grep -q '^set modo='; then
  ok "y en el disco: el núcleo nuevo en NUCLEO.BIN y el de justo antes en NUCLEO.ANT; GRUBENV (1.024 octetos) dice nuevo=1; GRUB.CFG, con la entrada del de antes una sola vez"
else ko "el núcleo nuevo no deja bien el arranque:"; lista_de $Q/s10.img | sed 's/^/        /' | head -12; tail -n 5 $Q/grub10.cfg | sed 's/^/        /'; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin los paquetes)"
fi

# ------------------------------------------------- 13. Hispa-Pentium 0.8: la red
# La red: el servicio de red (RED.BIN, en el anillo 3) con las tarjetas que
# da QEMU (-netdev user: DHCP, la puerta 10.0.2.2, que es esta máquina, y el
# DNS 10.0.2.3), IPCONFIG, PING y DESCARGAR contra un servidor HTTP de aquí
# (nucleo32/servidor_prueba.py), y el icono de la red en el escritorio.
if parte08 red; then
echo "== Hispa-Pentium 0.8: la red (el servicio de red, IPCONFIG, PING, DESCARGAR y el icono) =="
R=$P/n08r
mkdir -p $R/srv $G
python3 -I $AQUI/nucleo32/servidor_prueba.py $R/srv > $R/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 50); do [ -s $R/srv/puerto ] && break; sleep 0.1; done
U=http://10.0.2.2:$(cat $R/srv/puerto 2> /dev/null)
if $AQUI/nucleo32/disco.sh $R/d.img 16 red > $R/disco.txt 2>&1; then
# la e1000 (la de QEMU por omisión, la 82540EM)
cp $R/d.img $R/e.img
ESCRIBE=1 DISCO=$R/e.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 2" "ping -n 3 10.0.2.2" ":pausa 3" \
  "descargar $U/grande.bin" ":pausa 5" "descargar $U/trozos.bin" ":pausa 4" "descargar $U/cierra.bin" ":pausa 3" "descargar $U/redirige -" ":pausa 3" \
  "descargar $U/nohay" ":pausa 2" "descargar $U/ultima.txt c:\\textos\\otra.txt" ":pausa 2" "descargar https://10.0.2.2/" "descargar" \
  "descargar $U/lento.bin" ":pausa 3" ":tecla ctrl-c" ":pausa 2" "ipconfig /todo" ":pausa 2" "mem" "apagar" > $R/e1000.sal 2>&1
tr -d '\r' < $R/e1000.sal > $R/e1000.txt
S=$R/e1000.txt
mira "el servicio de red se pone en marcha solo (la primera vez que se pide algo), encuentra la e1000 y pide la IP por DHCP" \
  "^red: Hispa-Pentium 0.8: el servicio de red (RED.BIN)$" "^red: tarjeta 8086:100E en el bus PCI 00:03$" "^red: enlace: sí (1000 Mbit/s)$" \
  "^red: DHCP: IP 10.0.2.15, máscara 255.255.255.0, puerta 10.0.2.2, DNS 10.0.2.3$"
mira "IPCONFIG: la tarjeta, su MAC, el enlace, la IP, la máscara, la puerta de enlace, el DNS y el servidor de DHCP" \
  "^  Tarjeta . . . . . . . : Intel PRO/1000 (e1000) (8086:100E, IRQ [0-9]*)$" "^  MAC . . . . . . . . . : 52:54:00:12:34:56$" "^  Enlace  . . . . . . . : sí, a 1000 Mbit/s$" \
  "^  Estado  . . . . . . . : conectada$" "^  Dirección IP  . . . . : 10.0.2.15$" "^  Máscara de subred . . : 255.255.255.0$" "^  Puerta de enlace  . . : 10.0.2.2$" \
  "^  Servidor DNS  . . . . : 10.0.2.3$" "^  DHCP  . . . . . . . . : de 10.0.2.2, 86400 s"
mira "PING a la puerta de enlace: tres respuestas y ninguna perdida" "^Haciendo ping a 10.0.2.2 con 32 octetos de datos:$" \
  "^Respuesta desde 10.0.2.2: octetos=32 tiempo[<=][0-9]* ms TTL=[0-9]*$" "^    Paquetes: enviados = 3, recibidos = 3, perdidos = 0 (0% perdidos)$"
[ "$(grep -c "^Respuesta desde 10.0.2.2" $S)" = 3 ] || ko "PING: no son tres respuestas"
mira "DESCARGAR: con Content-Length, en trozos (chunked), hasta que se cierra, una redirección (302), un 404 y a otra carpeta" \
  "^Guardando GRANDE.BIN (409600 octetos)\.*$" "^Hecho: GRANDE.BIN, 409600 octetos en [0-9]*,[0-9] s" "^Hecho: TROZOS.BIN, 150001 octetos" "^Hecho: CIERRA.BIN, 70000 octetos" \
  "^Redirigido a /ultima.txt$" "^version=0.8$" "^DESCARGAR: no se ha bajado nada (el servidor ha dicho 404)\.$" "^Hecho: c:.textos.otra.txt, 41 octetos" \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: no hay servicio TLS (falta TLS.BIN): no se pueden hacer conexiones cifradas\.$" "^Uso: DESCARGAR http\[s\]://servidor\[:puerto\]/camino \[archivo\]$"
mira "Ctrl+C en medio de una descarga que no llega: el programa acaba, y la red sigue (IPCONFIG /TODO, con lo que ha apuntado el servicio)" \
  "^Guardando LENTO.BIN (1000000 octetos)^C$" "^Lo que ha apuntado el servicio de red:$" "^red: DHCP: IP 10.0.2.15" "^Este intérprete ocupa [0-9]* K. Tareas en marcha: 5\.$"
iguales=1
for f in GRANDE.BIN:grande.bin TROZOS.BIN:trozos.bin CIERRA.BIN:cierra.bin TEXTOS/OTRA.TXT:ultima.txt; do
  saca $R/e.img ${f%%:*} $R/x.bin && cmp -s $R/x.bin $R/srv/${f##*:} || { iguales=0; echo "        ${f%%:*} no es igual"; }
done
sano $R/e.img
if [ $iguales = 1 ] && grep -q "^$R/e.img: [0-9]* files" $P/fsck.txt; then ok "lo descargado, en el disco, es igual octeto a octeto que lo del servidor (los cuatro archivos), y el disco queda sano"
else ko "lo descargado no es igual que lo del servidor, o el disco no queda sano"; sed 's/^/        /' $P/fsck.txt | head -4; fi
# la RTL8139, y el DNS de QEMU (que pregunta al de esta máquina)
cp $R/d.img $R/r.img
ESCRIBE=1 DISCO=$R/r.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-netdev user,id=n0 -device rtl8139,netdev=n0" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 2" \
  "descargar $U/grande.bin" ":pausa 5" "ping -n 1 nombre.que.no.existe.invalid" ":pausa 4" "ping -n 1 actualiza.hispados.es" ":pausa 4" "apagar" > $R/rtl.sal 2>&1
tr -d '\r' < $R/rtl.sal > $R/rtl.txt
S=$R/rtl.txt
mira "la Realtek RTL8139: IPCONFIG y DESCARGAR" "^  Tarjeta . . . . . . . : Realtek RTL8139 (10EC:8139, IRQ [0-9]*)$" "^  Enlace  . . . . . . . : sí, a 100 Mbit/s$" \
  "^  Dirección IP  . . . . : 10.0.2.15$" "^Hecho: GRANDE.BIN, 409600 octetos"
if saca $R/r.img GRANDE.BIN $R/x.bin && cmp -s $R/x.bin $R/srv/grande.bin; then ok "con la RTL8139, lo descargado es igual octeto a octeto"; else ko "con la RTL8139, lo descargado no es igual"; fi
# (el DNS de QEMU le pregunta al de esta máquina: si esta no sale a la red, no se prueba)
if getent hosts actualiza.hispados.es > /dev/null 2>&1; then
  mira "el DNS (el de QEMU, que pregunta al de esta máquina): un nombre que no existe se dice; uno que sí, se resuelve" \
    "^PING: no se encuentra nombre.que.no.existe.invalid: ese nombre no existe\.$" "^Haciendo ping a actualiza.hispados.es \[$(getent hosts actualiza.hispados.es | awk '{print $1}' | head -1)\] con 32 octetos de datos:$"
else echo "  (esta máquina no resuelve nombres: no se prueba el DNS)"; fi
# sin tarjeta, y con «sinred»
DISCO=$R/d.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 2" "ping 10.0.2.2" ":pausa 2" "apagar" > $R/sin.sal 2>&1
tr -d '\r' < $R/sin.sal > $R/sin.txt
S=$R/sin.txt
mira "sin tarjeta de red: el servicio lo dice, y PING también" "^red: no hay ninguna tarjeta de red que se sepa llevar$" \
  "^  No hay ninguna tarjeta de red que Hispa-Pentium sepa llevar$" "^PING: no se encuentra\|^Error: no hay ninguna tarjeta de red que Hispa-Pentium sepa llevar\.$"
DISCO=$R/d.img ARRANQUE="ordenes sinred" MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 1" "apagar" > $R/sinred.sal 2>&1
tr -d '\r' < $R/sinred.sal > $R/sinred.txt
S=$R/sinred.txt
if grep -q "^  no hay servicio de red (falta RED.BIN o se arrancó con «sinred»)\.$" $S && ! grep -q "^red:" $S; then ok "con «sinred», ni se pone en marcha el servicio de red (y IPCONFIG lo dice)"
else ko "con «sinred» se pone en marcha la red:"; sed 's/^/        /' $S | tail -n 6; fi
# el escritorio: el servicio, en marcha al arrancar (hay tarjeta), y su icono
CAPTURAS=$G DISCO=$R/d.img ARRANQUE=escritorio MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 5" ":captura r1" ":clic 720 583" ":pausa 1" ":captura r2" > $R/esc.sal 2>&1
S=$R/esc.sal
mira "el escritorio pone en marcha el servicio de red (hay una tarjeta) y, con un clic en su icono, dice cómo está" \
  "^red: Hispa-Pentium 0.8: el servicio de red (RED.BIN)$" "^escritorio: aviso: La red: conectada, con la IP 10.0.2.15\.$"
miracap "el icono de la red en la bandeja, con la luz verde (conectada), a la izquierda del reloj" r1 rgb 729 587 2ECC40 rgb 725 582 8FDCFF
miracap "y el aviso de la red, al pulsarlo" r2 texto 230 255 260 280 "La red: conectada, con la IP 10.0.2.15."
else ko "no se puede hacer el disco de prueba de la red: $(tail -n 1 $R/disco.txt)"; fi
kill $SPID 2> /dev/null; wait $SPID 2> /dev/null
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin la red)"
fi   # la red

# ------------------------------------------- 13 bis. (TLS) Hispa-Pentium 0.8: HTTPS
# El servicio TLS (TLS.BIN: nucleo32/tls/, BearSSL en C en el anillo 3, como
# ACPI.BIN) con los servidores HTTPS de nucleo32/tls/servidor_https.py: su
# raíz de prueba va al disco (\RAICES\PRUEBA.CER) y sus certificados son
# para «10.0.2.2» (el bueno ECDSA, el bueno RSA, uno de otro nombre y uno de
# una raíz que no conoce nadie); y un HTTP para la cabecera Date.
if parte08 tls; then
echo "== Hispa-Pentium 0.8: HTTPS (el servicio TLS, TLS.BIN: DESCARGAR https://, la orden HTTPS y la biblioteca Https) =="
R=$P/n08t
mkdir -p $R/srv
if bash $AQUI/nucleo32/tls/hacer.sh $R/TLS.BIN > $R/hacer.txt 2>&1; then
  ok "nucleo32/tls/hacer.sh: TLS.BIN, BearSSL 0.6 (MIT) y tls.c como programa del anillo 3 ($(stat -c %s $R/TLS.BIN) octetos)"
else ko "nucleo32/tls/hacer.sh no hace TLS.BIN: $(tail -n 1 $R/hacer.txt)"; fi
python3 -I $AQUI/nucleo32/tls/servidor_https.py $R/srv > $R/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 100); do [ -s $R/srv/puertos ] && break; sleep 0.1; done
pu() { grep "^$1=" $R/srv/puertos | cut -d= -f2; }
B=https://10.0.2.2:$(pu bueno); RS=https://10.0.2.2:$(pu rsa); OT=https://10.0.2.2:$(pu otro); EX=https://10.0.2.2:$(pu extrano)
export MTOOLS_SKIP_CHECK=1
if [ -s $R/srv/puertos ] && $AQUI/nucleo32/disco.sh $R/d.img 16 red tls > $R/disco.txt 2>&1 && mmd -i $R/d.img ::RAICES && mcopy -i $R/d.img $R/srv/RAIZ.CER ::RAICES/PRUEBA.CER; then
cp $R/d.img $R/e.img
ESCRIBE=1 DISCO=$R/e.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  "descargar $B/grande.bin" ":pausa 6" "descargar $RS/trozos.bin" ":pausa 5" "https -i $B/galleta $B/mira" ":pausa 4" \
  "https -d hola=mundo -H X-Uno:dos $RS/eco" ":pausa 3" "https $B/redirige $RS/lejos" ":pausa 4" \
  "descargar $OT/ultima.txt" ":pausa 3" "descargar $EX/ultima.txt" ":pausa 3" "https /estado" ":pausa 2" > $R/e.sal 2>&1
tr -d '\r' < $R/e.sal > $R/e.txt
S=$R/e.txt
mira "el servicio TLS se pone en marcha solo (la primera conexión cifrada), con las autoridades de HSPTLS y la raíz de prueba de \\RAICES, y la hora del reloj del PC" \
  "tls: Hispa-Pentium 0.8: el servicio TLS (TLS.BIN, BearSSL 0.6, TLS 1.2)$" "tls: autoridad de más: .RAICES.PRUEBA.CER$" "tls: 21 autoridades raíz$" \
  "tls: la hora, del reloj del PC: [0-9][0-9]/[0-9][0-9]/20[0-9][0-9]$"
mira "DESCARGAR https://: el certificado ECDSA y el RSA comprobados (con la raíz de prueba), con largo y en trozos" \
  "tls: 10.0.2.2: TLS 1.2, ECDHE-ECDSA, .*; apretón de manos en [0-9]* ms (BearSSL: [0-9,]* M ciclos), la cadena comprobada$" \
  "^Conectando con 10.0.2.2 (10.0.2.2), puerto $(pu bueno)\.\.\. " "conectado (TLS 1.2, ECDHE-ECDSA, " "^Hecho: GRANDE.BIN, 409600 octetos en " \
  "tls: 10.0.2.2: TLS 1.2, ECDHE-RSA, .*la cadena comprobada$" "^Hecho: TROZOS.BIN, 150001 octetos en "
mira "la orden HTTPS (la biblioteca Https): las galletas que pone el servidor vuelven en el pedido siguiente, por la misma conexión (keep-alive)" \
  "^== $B/galleta: 200, 18 octetos (redirecciones: 0)$" "^Set-Cookie: sesion=abc123; Path=/; HttpOnly$" "^== $B/mira: 200, 35 octetos" "^galletas=sesion=abc123; idioma=es$"
if [ "$(sed -n "/^C:.>https -i /,/^C:.>https -d /p" $S | grep -c "apretón de manos")" = 1 ]; then ok "las dos direcciones del mismo servidor van por una sola conexión cifrada (un solo apretón de manos)"
else ko "HTTPS no vuelve a usar la conexión (keep-alive):"; sed -n "/^C:.>https -i /,/^C:.>https -d /p" $S | sed 's/^/        /' | head -12; fi
mira "HTTPS: un POST con su cuerpo y una cabecera propia; una redirección en el mismo servidor y otra (301) a otro servidor" \
  "^metodo=POST$" "^largo=10$" "^x-uno=dos$" "^tipo=application/x-www-form-urlencoded$" "^cuerpo=hola=mundo$" \
  "^== $B/ultima.txt: 200, 50 octetos (redirecciones: 1)$" "^Hispa-Pentium 0.8: la red, cifrada.$"
[ "$(grep -c "^== $B/ultima.txt: 200, 50 octetos (redirecciones: 1)$" $S)" = 2 ] || ko "HTTPS: la redirección a otro servidor no llega"
mira "los certificados que no valen no se aceptan, y se dice por qué en español: de otro nombre y de una raíz que no conoce nadie" \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: el certificado es de otro sitio, no de 10.0.2.2\.$" \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: el certificado de 10.0.2.2 no lo firma ninguna autoridad conocida\.$"
mira "HTTPS /ESTADO: la fecha que usa (y de dónde), las autoridades, las conexiones y la memoria" \
  "^  Servicio  . . . . . . : TLS.BIN (BearSSL 0.6), TLS 1.2$" "^  Fecha . . . . . . . . : .* (del reloj del PC)$" "^  Autoridades raíz  . . : 21$" \
  "^  Conexiones  . . . . . : 0 abiertas, [5-9] apretones de manos$" "^  Memoria . . . . . . . : 37 K de conexiones$"
iguales=1
for f in GRANDE.BIN:grande.bin TROZOS.BIN:trozos.bin; do
  saca $R/e.img ${f%%:*} $R/x.bin && cmp -s $R/x.bin $R/srv/${f##*:} || { iguales=0; echo "        ${f%%:*} no es igual"; }
done
sano $R/e.img
if [ $iguales = 1 ] && grep -q "^$R/e.img: [0-9]* files" $P/fsck.txt && saca $R/e.img TLS.CCH $R/x.bin && [ "$(stat -c %s $R/x.bin)" = 576 ]; then
  ok "lo descargado por HTTPS es igual octeto a octeto que lo del servidor, el disco queda sano y la memoria de cadenas (TLS.CCH) está guardada"
else ko "lo descargado por HTTPS no es igual, el disco no queda sano o no se ha guardado TLS.CCH"; sed 's/^/        /' $P/fsck.txt | head -4; fi
# otra vez (la cadena ya está en TLS.CCH), con el reloj del PC en el 2001 y la hora de la cabecera Date del servidor de aquí (TLS.INI)
printf 'hora=10.0.2.2:%s\r\n' "$(pu hora)" > $R/TLS.INI
mcopy -o -i $R/e.img $R/TLS.INI ::
cp $R/d.img $R/s.img
printf 'hora=10.0.2.2:1\r\n' > $R/TLS.INI
mcopy -o -i $R/s.img $R/TLS.INI ::
RELOJ=2001-01-01 DISCO=$R/e.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  "descargar $B/ultima.txt -" ":pausa 4" "https /estado" ":pausa 2" > $R/h.sal 2>&1 &
H1=$!
RELOJ=2001-01-01 DISCO=$R/s.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  "descargar $B/ultima.txt -" ":pausa 4" > $R/s.sal 2>&1 &
H2=$!
RELOJ=2050-06-01 DISCO=$R/d.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  "descargar $B/ultima.txt -" ":pausa 4" > $R/c.sal 2>&1 &
H3=$!
wait $H1 $H2 $H3
tr -d '\r' < $R/h.sal > $R/h.txt
S=$R/h.txt
mira "con el reloj del PC en el 2001, la hora se pide por HTTP (la cabecera Date: avisado como riesgo) y la cadena ya comprobada sale de TLS.CCH" \
  "tls: el reloj del PC dice 01/01/2001: no puede ser (este TLS.BIN es del .*RIESGO" "tls: la hora, de la cabecera Date de 10.0.2.2: .* (UTC)$" \
  "tls: 10.0.2.2: TLS 1.2, ECDHE-ECDSA, .*la cadena ya comprobada (de memoria)$" "^version=0.8$" "^  Fecha . . . . . . . . : .* (de la cabecera Date: el reloj del PC no está en hora)$"
tr -d '\r' < $R/s.sal > $R/s.txt
S=$R/s.txt
mira "con el reloj del PC mal y sin servidor de la hora: no hay conexión, y se dice por qué" "tls: la hora: no se conecta con 10.0.2.2:1 " \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: el reloj del equipo no está en hora y no se ha podido preguntar la hora a 10.0.2.2: sin la fecha no se pueden comprobar los certificados\.$"
tr -d '\r' < $R/c.sal > $R/c.txt
S=$R/c.txt
mira "con el reloj del PC en el 2050, el certificado ha caducado: no se acepta" "tls: la hora, del reloj del PC: 01/06/2050$" \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: el certificado de 10.0.2.2 ha caducado o aún no vale (con la fecha 01/06/2050, del reloj del PC); compruebe la fecha del equipo\.$"
# con internet: clicgest.es y hispados.es, frente a curl (y la hora de actualiza.hispados.es)
if curl -s -m 10 -o $R/clic.htm https://clicgest.es/ && curl -s -m 10 -o $R/hispa.htm https://hispados.es/ && [ -s $R/clic.htm ]; then
  cp $R/d.img $R/i.img
  ESCRIBE=1 RELOJ=2001-01-01 DISCO=$R/i.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
    "descargar https://clicgest.es/ clic.htm" ":pausa 8" "descargar https://hispados.es/ hispa.htm" ":pausa 8" > $R/i.sal 2>&1
  tr -d '\r' < $R/i.sal > $R/i.txt
  S=$R/i.txt
  mira "(con internet) la hora, de actualiza.hispados.es; https://clicgest.es/ y https://hispados.es/ con sus cadenas de Let's Encrypt (ISRG Root YE)" \
    "tls: la hora, de la cabecera Date de actualiza.hispados.es: " "tls: clicgest.es: TLS 1.2, ECDHE-ECDSA, .*la cadena comprobada$" \
    "tls: hispados.es: TLS 1.2, ECDHE-ECDSA, .*la cadena comprobada$"
  if saca $R/i.img CLIC.HTM $R/x.htm && curl -s -m 10 -o $R/clic.htm https://clicgest.es/ && cmp -s $R/x.htm $R/clic.htm \
     && saca $R/i.img HISPA.HTM $R/y.htm && curl -s -m 10 -o $R/hispa.htm https://hispados.es/ && cmp -s $R/y.htm $R/hispa.htm; then
    ok "(con internet) lo bajado de https://clicgest.es/ y https://hispados.es/ es igual octeto a octeto que lo que baja curl ($(stat -c %s $R/x.htm) y $(stat -c %s $R/y.htm) octetos)"
  else ko "(con internet) lo bajado de clicgest.es o hispados.es no es igual que con curl"; fi
else echo "  (esta máquina no llega a https://clicgest.es/: no se prueba contra internet)"; fi
else ko "no se pueden poner en marcha los servidores HTTPS de prueba o hacer el disco: $(tail -n 1 $R/srv.txt) $(tail -n 1 $R/disco.txt)"; fi
kill $SPID 2> /dev/null; wait $SPID 2> /dev/null
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin HTTPS)"
fi   # HTTPS

# ------------------------------------- 14. Hispa-Pentium 0.8: apagar de verdad y la batería
# El servicio de ACPI, ACPI.BIN (acpi/pentium/: el núcleo de C de HSPACPI
# de Hispa-DOS, con ACPICA, como programa del anillo 3), y energia.cas. En
# QEMU, que tiene ACPI (PIIX4 con «pc», ICH9 con «q35») pero no batería, la
# SSDT de las pruebas de Hispa-DOS (tests/acpi/bateria-qemu.aml, con
# -acpitable) declara una batería que lee su nivel de la CMOS: 60h el nivel
# (de 0 a 100), 61h bit 0 con cable y bit 1 puesta, 62h la salud en %. La
# CMOS se escribe por el monitor de QEMU («o /b 0x70 0x60»), con la máquina
# parada (-S) antes de arrancar y luego en marcha. Apagar de verdad: QEMU
# termina (ACPI S5) sin pasar por su puerto de pruebas (el código 5).
if parte08 acpi; then
echo "== Hispa-Pentium 0.8: apagar de verdad y la batería (el servicio de ACPI, ACPI.BIN) =="
Y=$P/acpi
mkdir -p $Y $G
if bash $AQUI/../acpi/pentium/hacer.sh $Y/ACPI.BIN > $Y/hacer.txt 2>&1; then
  ok "acpi/pentium/hacer.sh: ACPI.BIN, el núcleo de HSPACPI (ACPICA) como programa del anillo 3 ($(stat -c %s $Y/ACPI.BIN) octetos)"
else ko "acpi/pentium/hacer.sh no hace ACPI.BIN: $(tail -n 1 $Y/hacer.txt)"; fi
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN
for u in ubloc:BLOC uconsola:CONSOLA uordenes:ORDENES uhola:HOLA; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $Y/${u%%:*}.asm > /dev/null && nasm -f bin $Y/${u%%:*}.asm -o $Y/${u##*:}.BIN
done
SSDT="-acpitable file=$AQUI/../tests/acpi/bateria-qemu.aml"
# la CMOS de la batería: cmosq REGISTRO VALOR (órdenes del monitor para probar.sh)
cmosq() { echo ":monitor o /b 0x70 0x$1"; echo ":monitor o /b 0x71 0x$2"; }
mapfile -t PUESTA < <(cmosq 60 37; cmosq 61 02; cmosq 62 00)
# (con la máquina en marcha, parada un momento: que ni el núcleo, que lee
# su reloj de la CMOS, ni el servicio cambien el registro entre las dos)
mapfile -t ENCHUFA < <(echo ":monitor stop"; cmosq 61 03; echo ":monitor cont")
mapfile -t GASTADA < <(echo ":monitor stop"; cmosq 62 28; echo ":monitor cont")
# 1. el escritorio con la batería al 55 %, sin cable: la pila y su recuadro;
# luego enchufada (cargando) y gastada; y «Apagar» apaga (QEMU termina)
CAPTURAS=$G MODULOS=$Y/ACPI.BIN,$Y/BLOC.BIN ARRANQUE="escritorio acpicada=1" QEMU_MAS="-S $SSDT" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN \
  "${PUESTA[@]}" ":monitor cont" ":pausa 5" ":captura a1" ":raton 690 585" ":pausa 1.5" ":captura a2" \
  "${ENCHUFA[@]}" ":pausa 3" ":captura a3" ":clic 690 585" ":pausa 1" ":captura a4" ":raton 400 300" \
  "${GASTADA[@]}" ":pausa 3" ":raton 690 585" ":pausa 1.5" ":captura a5" ":raton 400 300" \
  ":clic 40 585" ":clic 80 176" ":pausa 2" ":escribe sin guardar" ":clic 40 585" ":clic 360 545" ":clic 275 327" ":pausa 3" ":captura a6" \
  ":clic 40 585" ":clic 360 545" ":clic 275 327" ":pausa 4" > $Y/apagar.sal 2>&1
S=$Y/apagar.sal
mira "el núcleo lanza el servicio de ACPI al arrancar, y este lee las tablas de QEMU y la batería de la SSDT de prueba (_BIF, _BIX, _BST)" \
  "^acpi: el servicio de ACPI (ACPI.BIN) en marcha, la tarea [0-9]*" "^ACPI: SSDT .*HISPA  BATPRUEB" "^Batería 1: ._SB_.HBAT" \
  "^Lectura 1: 55 % (2750 de 5000 mAh), sin cable, descargando, quedan 165 min"
mira "y le da al núcleo lo que hace falta para apagar (PM1a_CNT de PIIX4 y \\_S5) y reiniciar" \
  "^ACPI: para apagar: PM1a_CNT 604h, PM1b_CNT 0h, ._S5 0, 0; SMI_CMD B2h, ACPI_ENABLE F1h"
miracap "la bandeja enseña la pila con lo que queda (verde) y el porcentaje, al lado del reloj" a1 negrita 700 570 712 580 "55%" \
  rgb 680 580 3FAE49 rgb 695 580 EAF3FD
miracap "con el puntero quieto medio segundo encima, el recuadro de ayuda: lo que queda y los minutos, la salud y los ciclos de carga" a2 \
  texto 300 500 790 560 "Batería: 55 % · quedan 2 h 45 min" texto 300 500 790 560 "Salud: 68 % (guarda 5.000 de 7.350 mAh de cuando era nueva)" \
  texto 300 500 790 560 "312 ciclos de carga" rgb 790 520 FFF8E1
miracap "al enchufar el cargador (con el recuadro abierto), se pone al día solo: «cargando» y el rayo en la pila" a3 \
  texto 300 500 790 560 "Enchufado · cargando: 55 %" rgb 690 578 FFD43B
miracap "un clic en la pila quita el recuadro" a4 notexto 300 500 790 560 "Enchufado"
miracap "con la batería gastada (la salud al 40 %), el aviso en terracota" a5 texto 300 480 790 560 "Salud: 40 % (guarda 5.000 de 12.500 mAh de cuando era nueva)" \
  texto 300 480 790 560 "La batería está gastada: dura menos de la mitad que nueva"
miracap "«Apagar» con el Bloc de notas sin guardar: el Bloc avisa, no se apaga y el escritorio lo dice" a6 \
  texto 100 100 700 500 "Hay un programa sin cerrar: ¿algo sin guardar?"
mira "«Apagar» otra vez: se cierran los programas, se vacían los discos y el servicio apaga por ACPI (S5: _PTS, el modo ACPI, SLP_EN) y QEMU termina" \
  "^escritorio: apagar" "^energía: apagar (S5) por el servicio de ACPI" "^ACPI: preparar S5 (._S5, _PTS, _SST): AE_OK" \
  "^ACPI: modo ACPI (SMI_CMD B2h <- F1h): AE_OK" "^\[código 0\]"
if ! grep -q "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $S; then ok "ni una excepción del núcleo ni un error de ACPICA en toda la sesión"
else ko "hay excepciones o errores de ACPICA:"; grep "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $S | head -4 | sed 's/^/        /'; fi
# 2. reiniciar (con -no-reboot, QEMU termina al reiniciarse): con PIIX4 la
# FADT no trae registro de reinicio: el teclado
MODULOS=$Y/ACPI.BIN ARRANQUE="escritorio" QEMU_MAS="$SSDT" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" ":clic 40 585" ":clic 360 545" \
  ":clic 340 327" ":pausa 3" > $Y/reiniciar.sal 2>&1
S=$Y/reiniciar.sal
mira "«Reiniciar»: el servicio avisa al controlador embebido (_REG 3, 0) y el núcleo reinicia (PIIX4: por el teclado)" \
  "^escritorio: reiniciar" "^ACPI: el núcleo va a reiniciar el equipo" "^energía: reiniciar por el teclado (8042)" "^\[código 0\]"
# 3. con ICH9 (q35): apagar y reiniciar (aquí la FADT sí trae el registro de reinicio: CF9h)
for q in 275:apagar 340:reiniciar; do
  MAQUINA=q35 MODULOS=$Y/ACPI.BIN ARRANQUE="escritorio" QEMU_MAS="$SSDT" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" ":clic 40 585" \
    ":clic 360 545" ":clic ${q%%:*} 327" ":pausa 3" > $Y/q35${q##*:}.sal 2>&1
done
S=$Y/q35apagar.sal
mira "ICH9 (QEMU q35): «Apagar» apaga por ACPI (SMI_CMD B2h <- 02h) y QEMU termina" "^ACPI: modo ACPI (SMI_CMD B2h <- 2h): AE_OK" "^\[código 0\]"
S=$Y/q35reiniciar.sal
mira "ICH9: «Reiniciar» por el registro de reinicio de la FADT (E/S CF9h <- 0Fh)" \
  "^ACPI: para reiniciar: el registro de la FADT (E/S CF9h) <- Fh" "^energía: reiniciar por el registro de la FADT" "^\[código 0\]"
# 4. sin ACPI (-machine acpi=off; -no-acpi ya no está en QEMU 10): el servicio lo dice, no hay pila y «Apagar» es lo de antes (en QEMU, su puerto de pruebas)
CAPTURAS=$G MODULOS=$Y/ACPI.BIN ARRANQUE="escritorio" QEMU_MAS="-machine acpi=off" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" ":captura a7" \
  ":clic 40 585" ":clic 360 545" ":clic 275 327" ":pausa 2" > $Y/sinacpi.sal 2>&1
S=$Y/sinacpi.sal
mira "sin ACPI: el servicio lo dice y «Apagar» hace lo de antes (en QEMU, el código 5 de su puerto de pruebas)" \
  "^ACPI: el equipo no tiene ACPI" "escritorio: apagar" "^energía: el equipo no se ha apagado" "^\[código 5\]"
miracap "sin ACPI no hay pila en la bandeja (solo el reloj)" a7 notexto 670 570 740 590 "%" nrgb 695 580 EAF3FD
# 5. un sobremesa (ACPI sin batería): ni pila; apagar, por ACPI
CAPTURAS=$G MODULOS=$Y/ACPI.BIN ARRANQUE="escritorio" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" ":captura a8" \
  ":clic 40 585" ":clic 360 545" ":clic 275 327" ":pausa 3" > $Y/sobremesa.sal 2>&1
S=$Y/sobremesa.sal
mira "un sobremesa (ACPI sin batería): el servicio lo dice, y apaga por ACPI" "^El equipo no declara baterías" "^energía: apagar (S5) por el servicio de ACPI" "^\[código 0\]"
miracap "y en la bandeja no hay pila" a8 notexto 670 570 740 590 "%"
# 6. las llamadas del servicio no son para cualquiera: el mismo ACPI.BIN,
# lanzado a mano desde el intérprete, no puede (y lo dice)
MODULOS=$Y/ACPI.BIN,$Y/ORDENES.BIN ARRANQUE="ordenes sinacpi" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 2" "acpi" ":pausa 3" "salir" apagar > $Y/mano.sal 2>&1
S=$Y/mano.sal
mira "un programa cualquiera no puede usar las llamadas del servicio (150 a 153): ACPI.BIN lanzado a mano lo dice y acaba" \
  "^ACPI.BIN: es el servicio de ACPI; lo lanza el núcleo al arrancar"
if ! grep -q "^acpi: el servicio" $S; then ok "«sinacpi» en la línea del cargador: el núcleo no lanza el servicio"
else ko "con «sinacpi» el núcleo lanza el servicio"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin apagar de verdad ni la batería)"
fi   # apagar de verdad y la batería

# una máquina: maquina NOMBRE "opciones de QEMU" PASOS... (:pausa S, :tecla
# K..., :texto N, :captura N, :clic X Y, :monitor ORDEN, :espera S REGEX:
# hasta que salga REGEX por el puerto serie, como mucho S segundos; :centro X Y:
# la flecha está ahí, al arrancar con otra medida que 800 x 600); lo que
# sale por el puerto serie, en $V/NOMBRE.sal (lo usan las partes 16 y 17)
maquina() {
  local n=$1 o=$2 p x y s rx=400 ry=300 sx sy dx dy
  shift 2
  rm -f $V/mon
  # shellcheck disable=SC2086
  qemu-system-i386 $o -display none -monitor unix:$V/mon,server,nowait -serial file:$V/$n.sal -rtc base=localtime > /dev/null 2>&1 &
  QPID=$!
  for ((i = 0; i < 40; i++)); do [ -S $V/mon ] && break; sleep 0.1; done
  for p in "$@"; do
    case "$p" in
      ":pausa "*) sleep ${p:7} ;;
      ":centro "*) set -- ${p:8}; rx=$1; ry=$2 ;;
      ":tecla "*) for k in ${p:7}; do printf 'sendkey %s\n' $k | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.15; done ;;
      ":monitor "*) printf '%s\n' "${p:9}" | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.3 ;;
      ":captura "*) printf 'screendump %s\n' $G/${p:9}.ppm | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.6 ;;
      ":texto "*) printf 'pmemsave 0xb8000 4000 "%s"\n' $V/p.bin | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.4
        python3 -I -c "
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25): print('|' + bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())" $V/p.bin > $V/${p:7}.txt ;;
      ":espera "*) s=${p:8}; s=${s%% *}; x=${p#:espera $s }
        for ((i = 0; i < s * 2; i++)); do tr -d '\r' < $V/$n.sal | iconv -f CP437 -t UTF-8 2> /dev/null | grep -q -- "$x" && break; sleep 0.5; done ;;
      ":clic "*) set -- ${p:6}; dx=$(($1 - rx)); dy=$(($2 - ry))
        while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
          sx=$dx; [ $sx -gt 50 ] && sx=50; [ $sx -lt -50 ] && sx=-50
          sy=$dy; [ $sy -gt 50 ] && sy=50; [ $sy -lt -50 ] && sy=-50
          printf 'mouse_move %s %s\n' $sx $sy | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.05
          dx=$((dx - sx)); dy=$((dy - sy))
        done
        rx=$1; ry=$2; sleep 0.25
        printf 'mouse_button 1\n' | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.2
        printf 'mouse_button 0\n' | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.4 ;;
    esac
  done
  printf 'quit\n' | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1
  wait $QPID 2> /dev/null
  QPID=
  tr -d '\r' < $V/$n.sal | iconv -f CP437 -t UTF-8 > $V/$n.txt 2> /dev/null
  S=$V/$n.txt
}
# ------------------------------- 16. Hispa-Pentium 0.8: el CD y las actualizaciones por Internet
# El CD (cd.sh) en QEMU como lo pone VirtualBox, instalar desde él y, en el
# sistema instalado, actualizar por la red (el núcleo también) con un
# servidor HTTP de aquí (servidor_prueba.py con RAIZ) y un catálogo firmado
# con una clave de prueba (en el disco instalado se pone un PAQUETES.BIN
# hecho con ella, como en la parte 12).
if parte08 cd; then
echo "== Hispa-Pentium 0.8: el CD (VirtualBox) y las actualizaciones por Internet =="
V=$P/cd
mkdir -p $V/srv $V/raiz/pentium $G
export MTOOLS_SKIP_CHECK=1
if ! command -v grub-mkrescue > /dev/null || ! command -v xorriso > /dev/null; then
  echo "  (sin grub-mkrescue o xorriso no se prueba el CD)"
else
HPQ="python3 -I $AQUI/../tools/hpq.py"
# 1. el CD
if GUARDA_PAQ=$V/paq $AQUI/nucleo32/cd.sh $V/cd.iso > $V/cd.txt 2>&1 && xorriso -indev $V/cd.iso -report_el_torito plain > $V/torito.txt 2>&1 \
  && grep -q "^El Torito boot img : *1 *BIOS *y *none " $V/torito.txt && [ $(stat -c %s $V/cd.iso) -lt $((650 * 1048576)) ]; then
  ok "cd.sh hace el CD: una ISO de $(($(stat -c %s $V/cd.iso) / 1048576)) MB que arranca por la BIOS con El Torito sin emulación (vale en CD y en DVD), con GRUB"
else ko "cd.sh no hace bien el CD:"; tail -n 3 $V/cd.txt $V/torito.txt | sed 's/^/        /'; fi
VBOX_PC="-machine pc -m 256 -nic user,model=e1000 -device pci-ohci,id=o -device usb-tablet,bus=o.0"
VBOX_Q35="-machine q35 -m 256 -nic user,model=e1000"
# 2. arrancar sin instalar (la primera entrada), en un «pc» con IDE, USB OHCI (que no se toca) y la e1000
truncate -s 2G $V/ide.img
maquina vivo "$VBOX_PC -cdrom $V/cd.iso -drive file=$V/ide.img,format=raw,if=ide,index=0 -boot d" ":espera 40 ^escritorio: en marcha" ":espera 20 ^red: DHCP: IP" ":pausa 2" ":captura cd1"
# (TAILSCALE: sin «^» en la del DHCP: con más módulos, puede salir detrás del «> » de la consola)
mira "el CD en un «pc» como el de VirtualBox (IDE, USB OHCI, la e1000 82540EM): GRUB arranca solo la primera entrada, el escritorio sin instalar, con la red" \
  "escritorio: en marcha, 800 x 600" "red: tarjeta 8086:100E" "red: DHCP: IP 10\.0\.2\.15"
miracap "y el escritorio, desde la memoria (la unidad M:)" cd1 texto 20 50 110 70 "Mi equipo" negrita 30 570 80 590 Inicio
mira "(CD) y el núcleo lee el propio CD: el lector del IDE secundario maestro (el de VirtualBox) es D:, con su ISO 9660" \
  "^disco 1: QEMU DVD-ROM, lector de CD o DVD (IDE)\.$" "cd: D: CD de datos: «HISPA_PENTI[A-Z_0-9]*», ISO 9660"
if ! grep -q "excepción\|\*\*\*" $S; then ok "ni una excepción del núcleo (con el controlador OHCI y su tableta USB, que no se usan)"; else ko "hay excepciones:"; grep "excepción\|\*\*\*" $S | head -3 | sed 's/^/        /'; fi
# 3. instalar desde el CD: en el «pc» (IDE) y en el «q35» (SATA: AHCI, con el CD en AHCI también)
truncate -s 2G $V/sata.img
INSTALA_CD=(":pausa 3" ":tecla down ret" ":espera 40 ^instalar: bienvenida" ":texto c1" ":tecla ret" ":pausa 2" ":tecla ret" ":pausa 2" ":texto c3" ":tecla ret" ":espera 120 ^instalar: terminado" ":texto c4" ":tecla ret" ":pausa 2")
maquina inside "$VBOX_PC -cdrom $V/cd.iso -drive file=$V/ide.img,format=raw,if=ide,index=0 -boot d -no-reboot" "${INSTALA_CD[@]}"
mira "instalar desde el CD en el «pc» (el disco IDE): INSTALAR sabe que es el CD (MEDIO.TXT) y acaba" "^instalar: desde el CD$" "^instalar: terminado$"
if grep -q "para que arranque de él, sin el CD\." $V/c1.txt && grep -q "Disco: *IDE *QEMU HARDDISK, 2048 MB" $V/c3.txt && grep -q "Saque ahora el CD (o el DVD) y pulse INTRO" $V/c4.txt; then
  ok "y lo dice por el CD: «sin el CD», el disco IDE de 2 GB, «Saque ahora el CD (o el DVD)»"
else ko "INSTALAR, desde el CD, no dice lo suyo:"; cat $V/c3.txt $V/c4.txt | grep -v '^|$' | sed 's/^/        /' | head -20; fi
maquina insq35 "$VBOX_Q35 -cdrom $V/cd.iso -drive file=$V/sata.img,format=raw,if=none,id=d -device ide-hd,drive=d,bus=ide.0 -boot d -no-reboot" "${INSTALA_CD[@]}"
mira "y en el «q35» (SATA, AHCI; el CD, en AHCI): el disco SATA, instalado" "^instalar: desde el CD$" "^instalar: terminado$"
grep -q "Disco: *SATA *QEMU HARDDISK, 2048 MB" $V/c3.txt || ko "en el q35, el disco no sale como SATA: $(grep Disco: $V/c3.txt)"
# 4. arrancar del disco (con el CD aún puesto: «-boot c»): el asistente del primer arranque
for m in ide:pc sata:q35; do
  if [ ${m%%:*} = ide ]; then o="$VBOX_PC -drive file=$V/ide.img,format=raw,if=ide,index=0"; else o="$VBOX_Q35 -drive file=$V/sata.img,format=raw,if=none,id=d -device ide-hd,drive=d,bus=ide.0"; fi
  maquina arr${m%%:*} "$o -cdrom $V/cd.iso -boot c" ":espera 50 lanzo .HISPA.ASISTE.BIN"
  mira "el disco instalado desde el CD arranca solo (el «${m##*:}»): el escritorio y el asistente del primer arranque" "escritorio: en marcha" "lanzo .HISPA.ASISTE.BIN"
done
# 5. actualizar por Internet, en el instalado del q35: sus ajustes (sin
# asistente), de dónde (ORIGEN.TXT: el servidor de aquí) y PAQUETES con la
# clave de prueba
$HPQ clave $V/prueba.key > /dev/null && $HPQ publica $V/prueba.key $V/clave.cas > /dev/null
cp $AQUI/nucleo32/upaquete.cas $V/upaquete.cas
python3 -I $AQUI/ccastel.py $V/upaquete.cas -o $V/upaquete.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $V/upaquete.asm -o $V/PAQUETES.BIN
python3 -I $AQUI/nucleo32/servidor_prueba.py $V/srv $V/raiz > $V/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 50); do [ -s $V/srv/puerto ] && break; sleep 0.1; done
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\nmodo=800x600\r\n' > $V/HISPA.INI
printf 'http://10.0.2.2:%s/pentium/\r\n' "$(cat $V/srv/puerto)" > $V/ORIGEN.TXT
mcopy -o -i $V/sata.img@@1M $V/HISPA.INI $V/PAQUETES.BIN ::HISPA/ && mcopy -o -i $V/sata.img@@1M $V/ORIGEN.TXT ::HISPA/PAQUETES/
mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.BIN $V/NUCLEO08.BIN
# el servidor: los paquetes del CD, con un «base» más nuevo (sus mismos
# archivos; con NUCLEO, otro núcleo), el catálogo y su firma
sube_base() {   # VERSION [NÚCLEO]
  rm -f $V/raiz/pentium/*
  cp $V/paq/*.HPQ $V/raiz/pentium/
  python3 -I - $V/raiz/pentium "$@" <<'PY'
import os, sys
sys.path.insert(0, os.path.join(os.environ['AQUI'], '..', 'tools'))
import hpq
c, v = sys.argv[1], sys.argv[2]
campos, archivos, largo, datos = hpq.lee_paquete(os.path.join(c, 'BASE.HPQ'))
lista, p = [], largo
for d, t, s in archivos:
    b = datos[p:p + t]
    p += t
    if d == '\\HISPA\\NUCLEO.BIN' and len(sys.argv) > 3:
        b = open(sys.argv[3], 'rb').read()
    lista.append((d, b))
hpq.haz_paquete(os.path.join(c, 'BASE.HPQ'), 'base', v, campos['descripcion'], campos['necesita'], campos['choca'], lista)
PY
  $HPQ catalogo $V/raiz/pentium --clave $V/prueba.key > /dev/null
}
AQUI=$AQUI sube_base 0.9
DISCO="-drive file=$V/sata.img,format=raw,if=none,id=d -device ide-hd,drive=d,bus=ide.0 -boot c"
maquina act1 "$VBOX_Q35 $DISCO" ":espera 60 aviso en la bandeja: Hay una actualización" ":pausa 1" ":captura u1" ":clic 478 526" ":pausa 6" ":captura u2" \
  ":clic 443 467" ":pausa 12" ":captura u3" ":clic 240 467" ":espera 90 aviso en la bandeja: El núcleo de Hispa-Pentium El núcleo nuevo ha arrancado bien" ":pausa 1" ":captura u4"
mira "al arrancar, con red, ACTUALIZ /BANDEJA mira solo si hay actualizaciones (el catálogo firmado del servidor) y lo dice; tras actualizar el núcleo y reiniciar, que el nuevo ha arrancado bien" \
  "^actualiz: aviso en la bandeja: Hay una actualización\.$" "^reiniciar$" "^actualiz: aviso en la bandeja: El núcleo de Hispa-Pentium El núcleo nuevo ha arrancado bien\.$"
miracap "el aviso de la bandeja: una ventanita abajo a la derecha, «Hay una actualización», con «Verlas» y «Ahora no»" u1 negrita 400 460 480 480 "Hay una actualización." negrita 420 510 520 530 Verlas
miracap "«Verlas» abre «Programas y actualizaciones»: base, con la 0.9 nueva, y el catálogo de Internet con su firma buena" u2 \
  negrita 112 140 116 340 base negrita 300 140 310 340 "0.9 (nueva)" texto 140 105 160 113 "Los paquetes, de http://10.0.2.2:" negrita 300 107 700 113 "(firma buena)"
miracap "«Actualizar todo»: con el núcleo nuevo, «Reiniciar ahora» o «Más tarde»" u3 negrita 150 456 260 462 "Reiniciar ahora" negrita 400 456 520 462 "Más tarde" \
  texto 112 390 116 420 "Reinicie para usar el núcleo nuevo; si no arranca, vuelve el de antes."
miracap "tras reiniciar, el aviso: el núcleo nuevo ha arrancado bien" u4 texto 400 480 600 500 "El núcleo nuevo ha arrancado bien."
mcopy -n -i $V/sata.img@@1M ::HISPA/GRUBENV $V/grubenv1 > /dev/null 2>&1; mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.ANT $V/ant1 > /dev/null 2>&1
mcopy -n -i $V/sata.img@@1M ::HISPA/PAQUETES/BASE.PQI $V/base1.pqi > /dev/null 2>&1
if head -c 33 $V/grubenv1 | grep -q '^nuevo=0$' && cmp -s $V/ant1 $V/NUCLEO08.BIN && grep -q '^version=0\.9' $V/base1.pqi && grep -q '^pentium/BASE\.HPQ$' $V/srv/pedidos && sano $V/sata.img 2048; then
  ok "y en el disco: base 0.9 apuntado, el núcleo de antes en NUCLEO.ANT y GRUBENV a 0 (el nuevo vale); el paquete, traído del servidor"
else ko "la actualización por Internet no deja bien el disco:"; head -c 40 $V/grubenv1 | sed 's/^/        /'; fi
# 6. un núcleo que se cuelga (multiboot, «hlt» para siempre): al
# reiniciar a mano, GRUB vuelve solo al de antes, y el sistema lo dice
printf 'bits 32\norg 0x100000\ncab: dd 0x1BADB002, 0x00010000, -(0x1BADB002 + 0x00010000), cab, cab, fin, fin, ent\nent: cli\n.l: hlt\njmp .l\nfin:\n' > $V/colgado.asm
nasm -f bin $V/colgado.asm -o $V/COLGADO.BIN
AQUI=$AQUI sube_base 0.9.1 $V/COLGADO.BIN
mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.BIN $V/NUCLEO09.BIN
maquina act2 "$VBOX_Q35 $DISCO" ":espera 60 aviso en la bandeja: Hay una actualización" ":pausa 1" ":clic 478 526" ":pausa 6" ":clic 443 467" ":pausa 12" ":clic 240 467" \
  ":espera 30 ^reiniciar$" ":pausa 25" ":monitor system_reset" ":pausa 4" ":texto g5" ":espera 90 aviso en la bandeja: El núcleo de Hispa-Pentium El nuevo no" ":pausa 1" ":captura u6"
mira "con un núcleo nuevo que se cuelga, al reiniciar a mano GRUB arranca solo el de antes, y el sistema lo dice" \
  "^actualiz: aviso en la bandeja: El núcleo de Hispa-Pentium El nuevo no llegó a arrancar: se usa el de antes\.$"
if grep -q '\*Hispa-Pentium con el nucleo de antes (si el nuevo no arranca)' $V/g5.txt; then ok "GRUB, en ese arranque: la entrada del núcleo de antes, elegida sola"
else ko "GRUB no elige solo el núcleo de antes:"; grep -v '^|$' $V/g5.txt | sed 's/^/        /' | head -8; fi
mcopy -n -i $V/sata.img@@1M ::HISPA/GRUBENV $V/grubenv2 > /dev/null 2>&1; mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.ANT $V/ant2 > /dev/null 2>&1
mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.BIN $V/nuc2 > /dev/null 2>&1
if head -c 33 $V/grubenv2 | grep -q '^nuevo=4$' && cmp -s $V/ant2 $V/NUCLEO09.BIN && cmp -s $V/nuc2 $V/COLGADO.BIN && sano $V/sata.img 2048; then
  ok "y en el disco: GRUBENV con nuevo=4 (se sigue arrancando el de antes), NUCLEO.ANT el que valía y el disco sano"
else ko "tras el núcleo colgado, el disco no queda bien:"; head -c 40 $V/grubenv2 | sed 's/^/        /'; fi
kill $SPID 2> /dev/null; wait $SPID 2> /dev/null
fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el CD)"
fi   # el CD

# ------------------------------- 17. Hispa-Pentium 0.8: la medida de la pantalla
# Configuración > Cambiar...: con la VBE de Bochs, en el momento (y a prueba
# 15 s); sin ella (cirrus, como un PC de verdad), guardada para GRUB
# (\HISPA\PANTALLA.ENV) y a prueba al arrancar.
if parte08 pantalla; then
echo "== Hispa-Pentium 0.8: la medida de la pantalla (Configuración > Cambiar...: en el momento con la VBE de Bochs; al arrancar, con GRUB) =="
V=$P/pan
mkdir -p $V $G
export MTOOLS_SKIP_CHECK=1
[ -f $P/d16.img ] || $AQUI/nucleo32/disco.sh $P/d16.img 16 > $P/disco16.txt 2>&1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $V/semilla.asm > /dev/null 2>&1 && nasm -f bin $V/semilla.asm -o $V/SEMILLA.BIN
# 1. en QEMU con -kernel (el modo lo pone el núcleo): el Bloc de notas
# maximizado; Configuración > Cambiar...; 1024 x 768 a prueba y «Mantener»;
# luego 1280 x 800 a prueba, sin tocar nada: a los 15 s vuelve sola
CAPTURAS=$G MEMORIA=128 DISCO=$P/d16.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" ":doble 60 110" ":pausa 2" ":clic 553 76" ":pausa 1" \
  ":clic 40 585" ":clic 300 238" ":raton 700 470" ":captura m0" ":clic 572 236" ":pausa 1" ":captura m1" ":clic 565 229" ":clic 229 441" ":pausa 2" ":captura m2" ":clic 431 418" ":pausa 1" ":captura m3" \
  ":clic 495 347" ":clic 341 525" ":pausa 2" ":captura m4" ":pausa 17" ":captura m5" > $V/marcha.sal 2>&1
S=$V/marcha.sal
mira "con la VBE de Bochs (QEMU -vga std), «Probar...» pone la medida en el momento; «Mantener» la deja; sin tocar nada, a los 15 s vuelve sola la de antes" \
  "^pantalla: 1024x768 con 32 bits por punto (cambiado en marcha, por la VBE de Bochs)" "^escritorio: la pantalla, a 1024 x 768 (en marcha)" \
  "^escritorio: Se queda a 1024 x 768 (hasta que se apague)\." "^escritorio: la pantalla, a 1280 x 800 (en marcha)" \
  "^escritorio: no se ha dado por buena en 15 s" "^escritorio: Ha vuelto la de antes\."
miracap "Configuración: la medida de la pantalla y, a su lado, «Cambiar...»" m0 texto 190 224 200 234 "Pantalla:  800 x 600, 32 bits por punto" negrita 530 224 542 232 "Cambiar..."
miracap "la lista: los modos que da la tarjeta y caben en la pantalla (su EDID: 1280 x 800, recomendada), el de ahora elegido, «Probar...» y «Cerrar»" m1 \
  negrita 200 120 208 126 "La medida de la pantalla" texto 200 140 208 146 "Ahora: 800 x 600, 32 bits por punto." negrita 200 218 210 224 "800 x 600" \
  negrita 520 218 530 224 "1024 x 768" negrita 350 250 360 258 "1280 x 800 *" texto 160 283 168 291 "* La de la pantalla (recomendada): 1280 x 800." notexto 0 0 800 600 "1920 x 1080" \
  negrita 188 430 198 436 "Probar..." negrita 565 430 575 436 Cerrar rgb 170 228 5CC45E 40
miracap "a prueba: la pantalla ya a 1024 x 768, «¿Se ve bien?» con la cuenta atrás, y el Bloc de notas maximizado, a la medida nueva" m2 tam 1024 768 \
  negrita 334 305 342 313 "¿Se ve bien la pantalla?" texto 334 327 342 335 "Está a 1024 x 768." negrita 393 405 405 415 Mantener negrita 571 405 583 415 Volver \
  negrita 24 2 36 10 "NOTAS.TXT - Bloc de notas" rgb 1000 719 D7E5F8 10 texto 4 708 14 716 "NOTAS.TXT" negrita 30 742 46 752 Inicio
miracap "«Mantener»: la lista otra vez, con la de ahora (1024 x 768) y lo que ha pasado" m3 tam 1024 768 texto 312 223 320 231 "Ahora: 1024 x 768, 32 bits por punto." \
  texto 272 383 280 391 "Se queda a 1024 x 768 (hasta que se apague)."
miracap "1280 x 800 a prueba" m4 tam 1280 800
miracap "y a los 15 s, sin tocar nada, la de antes (1024 x 768): «Ha vuelto la de antes.»" m5 tam 1024 768 texto 272 383 280 391 "Ha vuelto la de antes."
if ! grep -q "excepción\|\*\*\*" $S; then ok "ni una excepción ni una queja del núcleo al cambiar la pantalla en marcha"; else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -3 | sed 's/^/        /'; fi
# 2. el CD, con el modo que ha puesto GRUB (por la VBE de la BIOS), en un
# «pc» como el de VirtualBox: con -vga std (como VBoxVGA) y con -vga vmware
# (la VMSVGA, 15AD:0405: la memoria, en su segunda zona PCI)
CD=$P/cd/cd.iso
[ -f $CD ] || { CD=$V/cd.iso; $AQUI/nucleo32/cd.sh $CD > $V/cd.txt 2>&1; }
VBOX_PC="-machine pc -m 256 -nic user,model=e1000 -device pci-ohci,id=o -device usb-tablet,bus=o.0"
truncate -s 2G $V/ide.img
for vga in std vmware; do
  # (EDID) con -vga std, la lista llega hasta lo que dice su EDID (1280 x 800, la quinta); con vmware, sin EDID, la sexta
  if [ $vga = std ]; then x=403; else x=565; fi
  maquina cd$vga "$VBOX_PC -vga $vga -cdrom $CD -drive file=$V/ide.img,format=raw,if=ide,index=0 -boot d" ":espera 40 ^escritorio: en marcha" ":pausa 3" \
    ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 572 236" ":pausa 1" ":captura cd$vga" ":clic $x 263" ":clic 229 441" ":pausa 2" ":captura cd${vga}2" ":clic 559 434" ":pausa 1"
  mira "el CD en un «pc» como VirtualBox con -vga $vga: el modo de GRUB (800 x 600) se cambia en marcha (1280 x 800) y se mantiene" \
    "escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)" "^pantalla: 1280x800 con 32 bits por punto (cambiado en marcha" \
    "^escritorio: Se queda a 1280 x 800 (hasta que se apague)\."
  miracap "y se ve: a 1280 x 800, con «¿Se ve bien la pantalla?» (-vga $vga)" cd${vga}2 tam 1280 800 negrita 462 321 470 329 "¿Se ve bien la pantalla?" negrita 30 770 46 780 Inicio
done
# 3. instalado desde el CD, con una tarjeta sin la VBE de Bochs (-vga
# cirrus: como un PC de verdad): «Guardar y reiniciar...»
truncate -s 2G $V/inst.img
maquina inst "$VBOX_PC -cdrom $CD -drive file=$V/inst.img,format=raw,if=ide,index=0 -boot d -no-reboot" ":pausa 3" ":tecla down ret" ":espera 40 ^instalar: bienvenida" \
  ":tecla ret" ":pausa 2" ":tecla ret" ":pausa 2" ":tecla ret" ":espera 120 ^instalar: terminado" ":tecla ret" ":pausa 2"
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\nmodo=800x600\r\n' > $V/HISPA.INI
mcopy -o -i $V/inst.img@@1M $V/HISPA.INI ::HISPA/
cp $V/inst.img $V/inst0.img
# (EDID) cirrus no da EDID: la nativa (1024 x 768), como si se hubiera
# sabido otro día arrancando «a la medida»; sin ella, nada mayor que la de ahora
python3 -I -c "import sys; t='# GRUB Environment Block\npantalla=\nantes=\nprueba=0\nnativa=1024x768\n'; open(sys.argv[1],'w').write(t+'#'*(1024-len(t)))" $V/envn
mcopy -o -i $V/inst.img@@1M $V/envn ::HISPA/PANTALLA.ENV
CIRRUS="$VBOX_PC -vga cirrus -drive file=$V/inst.img,format=raw,if=ide,index=0"
maquina pc1 "$CIRRUS -no-reboot" ":espera 40 ^escritorio: en marcha" ":pausa 3" ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 572 236" ":pausa 1" ":captura pc1" \
  ":clic 565 229" ":clic 274 441" ":pausa 1" ":captura pc2" ":clic 314 336" ":pausa 6"
mira "sin la VBE de Bochs (cirrus), instalado: la medida se guarda en \\HISPA\\PANTALLA.ENV (a prueba), GRUB.CFG la lee, y «Reiniciar ahora» reinicia" \
  "^escritorio: GRUB.CFG lee ahora PANTALLA.ENV" "^escritorio: .*PANTALLA.ENV: pantalla=1024x768 antes= prueba=1" "^escritorio: reiniciar"
miracap "la lista, sin cambio en marcha: «se pone al arrancar», «A la medida» y «Guardar y reiniciar...»" pc1 texto 160 167 168 175 "Esta tarjeta no deja cambiarla en marcha: se guarda y" \
  negrita 190 252 200 260 "A la medida" negrita 512 216 522 224 "1024 x 768 *" notexto 0 0 800 600 "1280 x 800" negrita 180 428 190 436 "Guardar y reiniciar..."
miracap "«Al arrancar otra vez, a 1024 x 768», con «Reiniciar ahora» y «Más tarde»" pc2 negrita 224 216 234 224 "Al arrancar otra vez, a 1024 x 768." \
  negrita 248 324 258 332 "Reiniciar ahora" negrita 448 324 458 332 "Más tarde"
mcopy -n -i $V/inst.img@@1M ::HISPA/GRUB.CFG $V/grub.cfg; mcopy -n -i $V/inst.img@@1M ::HISPA/PANTALLA.ENV $V/env1
if [ $(stat -c %s $V/env1) = 1024 ] && head -c 25 $V/env1 | grep -q '^# GRUB Environment Block' && grep -q '^  load_env -f /HISPA/PANTALLA.ENV pantalla antes prueba$' $V/grub.cfg \
  && grep -q 'set modo="${pantalla}x32,${pantalla},${modo}"' $V/grub.cfg && sano $V/inst.img 2048; then
  ok "PANTALLA.ENV es un bloque de GRUB de 1024 octetos (GRUB escribe en él: save_env), GRUB.CFG lo lee y el disco está sano"
else ko "PANTALLA.ENV o GRUB.CFG no quedan bien:"; head -c 80 $V/env1 | sed 's/^/        /'; fi
# al arrancar: GRUB pone 1024 x 768 (y la apunta «probándose»); «Mantener»
maquina pc2 "$CIRRUS -no-reboot" ":espera 40 está a prueba" ":pausa 2" ":captura pc3" ":centro 512 384" ":clic 431 418" ":pausa 2"
mcopy -n -i $V/inst.img@@1M ::HISPA/PANTALLA.ENV $V/env2
mira "al arrancar, GRUB la pone (1024 x 768) y el escritorio pregunta; «Mantener» la deja para siempre" \
  "escritorio: en marcha, 1024 x 768 con [0-9]* bits por punto (el modo lo ha dejado puesto el cargador)" "pantalla: está a prueba" \
  "^escritorio: .*PANTALLA.ENV: pantalla=1024x768 antes= prueba=0" "^escritorio: Se queda a 1024 x 768\."
miracap "«¿Se ve bien la pantalla?», a 1024 x 768, con la cuenta atrás «vuelve la de antes»" pc3 negrita 334 305 342 313 "¿Se ve bien la pantalla?" \
  texto 334 347 342 355 "Si no la da por buena, vuelve la de antes en 1"
if head -c 100 $V/env2 | grep -q '^prueba=0$' && head -c 100 $V/env2 | grep -q '^pantalla=1024x768$'; then ok "y PANTALLA.ENV: pantalla=1024x768, prueba=0"
else ko "PANTALLA.ENV no queda bien tras «Mantener»:"; head -c 70 $V/env2 | sed 's/^/        /'; fi
# otra, 1024 x 600: se reinicia y, en la prueba, se reinicia otra vez a lo
# bruto (como con la pantalla a oscuras): GRUB vuelve sola a 1024 x 768
maquina pc3 "$CIRRUS -no-reboot" ":espera 40 ^escritorio: en marcha" ":pausa 3" ":centro 512 384" ":clic 40 753" ":clic 300 406" ":clic 688 320" ":pausa 1" \
  ":clic 515 313" ":clic 386 525" ":pausa 1" ":clic 426 420" ":pausa 6"
mira "desde 1024 x 768, 1024 x 600 guardada a prueba (la de antes, apuntada)" "^escritorio: .*PANTALLA.ENV: pantalla=1024x600 antes=1024x768 prueba=1" "^escritorio: reiniciar"
maquina pc4 "$CIRRUS" ":espera 40 está a prueba" ":pausa 2" ":monitor system_reset" ":pausa 3" ":espera 40 en marcha, 1024 x 768" ":pausa 4"
mcopy -n -i $V/inst.img@@1M ::HISPA/PANTALLA.ENV $V/env3
mira "si se reinicia sin darla por buena, GRUB vuelve solo a la de antes (1024 x 768), sin preguntar" "escritorio: en marcha, 1024 x 600" "pantalla: está a prueba" \
  "escritorio: en marcha, 1024 x 768"
if [ $(grep -c "pantalla: está a prueba" $S) = 1 ] && head -c 100 $V/env3 | grep -q '^pantalla=1024x768$' && head -c 100 $V/env3 | grep -q '^antes=$' && head -c 100 $V/env3 | grep -q '^prueba=0$' && sano $V/inst.img 2048; then
  ok "y PANTALLA.ENV lo dice (GRUB lo ha escrito con save_env): pantalla=1024x768, antes vacío, prueba=0; el disco, sano"
else ko "tras reiniciar sin darla por buena, PANTALLA.ENV no queda bien:"; head -c 70 $V/env3 | sed 's/^/        /'; fi
# 4. instalado, con la VBE de Bochs (-vga std, como VirtualBox): «Mantener»
# la guarda para los próximos arranques; y una medida que la BIOS no da
# (1200 x 800; con -vga vmware, la VMSVGA: GRUB no tiene controlador
# para ella y va por la VBE de la BIOS, que no la da) la pone el núcleo al arrancar
cp $V/inst0.img $V/inst.img
STD="$VBOX_PC -vga std -drive file=$V/inst.img,format=raw,if=ide,index=0"
maquina vb1 "$STD" ":espera 40 ^escritorio: en marcha" ":pausa 3" ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 572 236" ":pausa 1" ":clic 565 229" ":clic 229 441" ":pausa 2" ":clic 431 418" ":pausa 2"
mira "instalado, con la VBE de Bochs: «Mantener» la guarda también para los próximos arranques" "^escritorio: .*PANTALLA.ENV: pantalla=1024x768 antes= prueba=0" \
  "^escritorio: Se queda a 1024 x 768, también al arrancar\."
maquina vb2 "$STD" ":espera 40 ^escritorio: en marcha" ":pausa 3"
mira "y al arrancar otra vez, GRUB ya la pone (1024 x 768)" "escritorio: en marcha, 1024 x 768 con 32 bits por punto (el modo lo ha dejado puesto el cargador)"
printf '# GRUB Environment Block\npantalla=1200x800\nantes=\nprueba=0\n' > $V/env4
head -c 1024 /dev/zero | tr '\0' '#' | dd of=$V/env4 bs=1 seek=$(stat -c %s $V/env4) count=$((1024 - $(stat -c %s $V/env4))) status=none 2> /dev/null
mcopy -o -i $V/inst.img@@1M $V/env4 ::HISPA/PANTALLA.ENV
maquina vb3 "${STD/-vga std/-vga vmware}" ":espera 40 la pantalla, a 1200" ":pausa 2" ":captura vb3"
mira "con la VMSVGA, 1200 x 800 (que su BIOS no da por VBE: GRUB pone otra) la pone el núcleo al arrancar, por la VBE de Bochs" \
  "escritorio: en marcha, 800 x 600" "^escritorio: la pantalla, a 1200 x 800 (en marcha)"
miracap "y el escritorio, a 1200 x 800" vb3 tam 1200 800 negrita 30 770 46 780 Inicio
# 5. (EDID) lo que dice la pantalla de sí misma: con QEMU -device VGA y un
# EDID de 1024 x 600 (un netbook), uno de 1366 x 768 y sin EDID; el
# Administrador de dispositivos dice el monitor
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/udisposi.cas -o $V/udisposi.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $V/udisposi.asm -o $V/DISPOSIT.BIN
cp $P/d16.img $V/de.img; mcopy -o -i $V/de.img $V/DISPOSIT.BIN ::DISPOSIT.BIN
for e in 1024:600 1366:768 no; do
  if [ $e = no ]; then o="edid=off"; else o="xres=${e%:*},yres=${e#*:}"; fi
  CAPTURAS=$G MEMORIA=128 DISCO=$V/de.img ARRANQUE=escritorio MODULOS= VGA=none QEMU_MAS="-device VGA,$o" $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" \
    ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 576 236" ":pausa 1" ":captura ed${e%:*}" ":clic 594 441" ":clic 333 399" ":pausa 3" ":captura dv${e%:*}" > $V/edid$e.sal 2>&1
done
S=$V/edid1024:600.sal
mira "con un EDID de 1024 x 600 (un netbook), el núcleo lo lee y lo apunta en el registro del arranque" "pantalla: EDID: «QEMU Monitor», hecha para 1024 x 600 (la tarjeta de QEMU)"
miracap "y la lista ofrece solo lo que cabe en la pantalla: 800 x 600 y 1024 x 600, la recomendada" ed1024 negrita 200 218 210 224 "800 x 600" negrita 355 218 365 224 "1024 x 600 *" \
  texto 160 249 168 257 "* La de la pantalla (recomendada): 1024 x 600." notexto 0 0 800 600 "1024 x 768"
miracap "el Administrador de dispositivos dice el monitor en Pantalla, colgado de la tarjeta (lo lleva edid.cas)" dv1024 texto 544 175 552 183 "edid.cas"
S=$V/edid1366:768.sal
mira "con un EDID de 1366 x 768" "pantalla: EDID: «QEMU Monitor», hecha para 1366 x 768"
miracap "nada mayor que 1366 x 768 (y 1366 no, que la VBE de QEMU no la da: la deja en 1360)" ed1366 negrita 0 0 800 600 "1280 x 720" texto 160 200 168 400 "* La de la pantalla (recomendada): 1366 x 768." \
  notexto 0 0 800 600 "1440 x 900" notexto 0 0 800 600 "1366 x 768 *"
S=$V/edidno.sal
mira "sin EDID, lo dice" "la pantalla no dice su medida"
miracap "y en una máquina virtual (su pantalla es una ventana) la lista sigue sin tope" edno negrita 0 0 800 600 "1920 x 1080"
# 6. (EDID) la memoria USB (usb.sh, como root), en un PC sin la VBE de
# Bochs ni EDID (-vga cirrus, como el HP Mini): se guarda en X:\PANTALLA.ENV
# y la usa la primera entrada de su menú; lo que la BIOS no da se dice
if [ "$(id -u)" = 0 ] && command -v grub-install > /dev/null && command -v parted > /dev/null; then
  $AQUI/nucleo32/usb.sh $V/usb.img > $V/usb.txt 2>&1
  USBQ="-machine pc -m 256 -vga cirrus -nic none -device qemu-xhci,id=u -drive file=$V/usb.img,format=raw,if=none,id=m -device usb-storage,bus=u.0,drive=m,bootindex=1"
  maquina us1 "$USBQ -no-reboot" ":espera 60 ^escritorio: en marcha" ":pausa 3" ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 576 236" ":pausa 1" ":captura us1" \
    ":clic 403 229" ":clic 274 441" ":pausa 1" ":clic 314 336" ":pausa 6"
  mira "desde la memoria USB (sin EDID): nada mayor que la de ahora, «A la medida», y se guarda en X:\\PANTALLA.ENV" \
    "la pantalla no dice su medida" "^escritorio: \\\\PANTALLA.ENV: pantalla=auto antes= prueba=1" "^escritorio: reiniciar"
  miracap "«No se sabe su medida: nada mayor que la de ahora.»" us1 texto 160 249 168 257 "No se sabe su medida: nada mayor que la de ahora." negrita 350 218 370 226 "A la medida"
  maquina us2 "$USBQ -no-reboot" ":espera 60 está a prueba" ":pausa 2" ":clic 431 418" ":pausa 2"
  mira "y al arrancar, GRUB la pone (a la medida) desde la primera entrada y pregunta" "PANTALLA.ENV pide «auto» (prueba=2)" "pantalla: está a prueba"
  python3 -I -c "import sys; t='# GRUB Environment Block\npantalla=1600x900\nantes=\nprueba=0\nnativa=\n'; open(sys.argv[1],'w').write(t+'#'*(1024-len(t)))" $V/envu
  mcopy -o -i $V/usb.img@@1M $V/envu ::PANTALLA.ENV
  maquina us3 "$USBQ -no-reboot" ":espera 60 no permite" ":pausa 3" ":captura us3"
  mcopy -n -i $V/usb.img@@1M ::ARRANQUE.TXT $V/arranque.txt; mcopy -n -i $V/usb.img@@1M ::PANTALLA.ENV $V/envu2
  mira "si la BIOS no da la pedida (1600 x 900 en cirrus), GRUB pone otra y el núcleo lo dice" "Este equipo no permite 1600 x 900: se queda en 800 x 600\."
  miracap "con un aviso" us3 texto 0 0 800 600 "Este equipo no permite 1600 x 900: se queda en 800 x 600."
  if tr -d '\r' < $V/arranque.txt | iconv -f CP437 -t UTF-8 | grep -q "Este equipo no permite 1600 x 900" && head -c 60 $V/envu2 | grep -q '^pantalla=800x600$'; then
    ok "y queda en X:\\ARRANQUE.TXT (para mandarlo), y PANTALLA.ENV pasa a la que hay"
  else ko "ARRANQUE.TXT o PANTALLA.ENV no lo dicen:"; head -c 60 $V/envu2 | sed 's/^/        /'; fi
else
  echo "  (sin ser root no se hace la memoria USB: sin la parte de la memoria USB)"
fi
if ! cat $V/pc*.txt $V/vb*.txt $V/cd*.txt | grep -q "excepción\|\*\*\*"; then ok "en todos esos arranques, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $V/pc*.txt $V/vb*.txt $V/cd*.txt | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin la medida de la pantalla)"
fi   # la pantalla
# ------------------------------- 19. Hispa-Pentium 0.8.3: el Administrador de dispositivos
# (dispositivos) DISPOSIT.BIN (udisposi.cas) y la llamada 155 del núcleo
# (dispositivos.cas): todo lo del PCI, lo USB, lo de ACPI (que apunta
# ACPI.BIN con la 156), los discos, lo de siempre y el procesador, con el
# controlador que lleva cada cosa. En un «pc» (PIIX) con la e1000, EHCI con
# teclado, ratón, tableta (sin controlador: no es de arranque) y una memoria,
# un disco SATA (AHCI) y la batería de mentira de la parte 14; y en un «q35»
# (ICH9) con la RTL8139, xHCI, el disco en el AHCI de la placa y la batería.
if parte08 dispositivos; then
echo "== Hispa-Pentium 0.8.3: el Administrador de dispositivos (DISPOSIT.BIN y la llamada 155) =="
Y=$P/disp
mkdir -p $Y $G
bash $AQUI/../acpi/pentium/hacer.sh $Y/ACPI.BIN > $Y/hacer.txt 2>&1 || ko "acpi/pentium/hacer.sh no hace ACPI.BIN: $(tail -n 1 $Y/hacer.txt)"
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN
for u in udisposi:DISPOSIT uequipo:EQUIPO uordenes:ORDENES ured:RED; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $Y/${u%%:*}.asm > /dev/null && nasm -f bin $Y/${u%%:*}.asm -o $Y/${u##*:}.BIN \
    || ko "${u%%:*}.cas no compila"
done
SSDT="-acpitable file=$AQUI/../tests/acpi/bateria-qemu.aml"
cmosq() { echo ":monitor o /b 0x70 0x$1"; echo ":monitor o /b 0x71 0x$2"; }
mapfile -t PUESTA < <(cmosq 60 37; cmosq 61 03; cmosq 62 00)
rm -f $Y/mem.img $Y/sata.img
mkfs.fat -C -F 16 -n MEMORIA $Y/mem.img 32768 > /dev/null
mkfs.fat -C -F 16 -n SATA $Y/sata.img 65536 > /dev/null
# 1. «pc», en el escritorio: Mi equipo > Propiedades; guardar el informe en la
# memoria (X:); las propiedades (General, Controlador, Recursos); copiar el
# informe; e Inicio > Configuración > «Administrador de dispositivos...»
USBS="-device usb-ehci,id=e -device usb-kbd,bus=e.0 -device usb-mouse,bus=e.0 -device usb-tablet,bus=e.0 -drive id=mu,file=$Y/mem.img,format=raw,if=none -device usb-storage,bus=e.0,drive=mu"
CAPTURAS=$G ESCRIBE=1 SATA=$Y/sata.img SOLO_LECTURA=1 MODULOS=$Y/ACPI.BIN,$Y/DISPOSIT.BIN,$Y/EQUIPO.BIN,$Y/RED.BIN ARRANQUE="escritorio escritura=usb unidad=M acpicada=1" \
  QEMU_MAS="-S $SSDT -nic user,model=e1000 $USBS" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "${PUESTA[@]}" ":monitor cont" ":pausa 7" \
  ":clicd 63 40" ":pausa 1" ":captura dv1" ":clic 100 70" ":pausa 4" ":captura dv2" ":clic 395 81" ":pausa 3" ":captura dv3" \
  ":tecla down" ":tecla ret" ":pausa 4" ":captura dv4" ":clic 228 140" ":raton 400 450" ":pausa 1" ":captura dv5" ":clic 323 140" ":raton 400 450" ":pausa 1" ":captura dv6" \
  ":tecla esc" ":pausa 1" ":clic 245 81" ":pausa 2" ":captura dv7" \
  ":tecla left" ":tecla down" ":tecla left" ":tecla down" ":tecla left" ":tecla down" ":tecla left" ":tecla down" ":tecla left" ":tecla down" ":tecla left" ":pausa 1" ":captura dv10" ":clic 742 31" ":pausa 1" \
  ":clic 40 585" ":pausa 1" ":clic 300 238" ":raton 700 470" ":pausa 1" ":captura dv8" ":clic 300 398" ":pausa 4" ":captura dv9" > $Y/pc.sal 2>&1
S=$Y/pc.sal
mira "ACPI.BIN apunta lo que hay en el espacio de nombres de ACPI (la llamada 156)" "^ACPI: [0-9]* dispositivos del espacio de nombres, apuntados para el Administrador de dispositivos"
mira "Mi equipo > Propiedades, Configuración > «Administrador de dispositivos...» y las propiedades lanzan DISPOSIT.BIN" \
  "^escritorio: lanzo .DISPOSIT.BIN (la tarea [0-9]*)$"
[ "$(grep -c '^escritorio: lanzo .DISPOSIT.BIN' $S)" = 2 ] && ok "(dos veces: desde Mi equipo y desde Configuración)" || ko "DISPOSIT.BIN no se lanza dos veces: $(grep -c '^escritorio: lanzo .DISPOSIT' $S)"
miracap "el botón secundario en Mi equipo: «Abrir» y «Propiedades»" dv1 texto 60 30 200 100 Abrir texto 60 30 200 100 Propiedades
miracap "la ventana: el árbol por categorías, con el controlador de cada cosa" dv2 \
  negrita 30 15 400 40 "Administrador de dispositivos" negrita 40 120 400 520 "[-] Pantalla (2)" negrita 40 120 500 520 "Intel PRO/1000 MT (82540EM)" \
  texto 400 120 760 520 RED.BIN texto 400 120 760 520 ahci.cas texto 30 60 700 100 "Guardar informe"
miracap "las categorías se cierran con la flecha izquierda; la tableta USB, «SIN CONTROLADOR» con su «!», fondo naranja y su franja" dv10 \
  negrita 40 120 400 520 "[+] Discos (2)" negrita 40 120 400 520 "[+] Red (1)" negrita 40 120 760 520 "! QEMU USB Tablet" texto 300 120 720 520 "SIN CONTROLADOR"
miracap "«Guardar informe»: dónde lo ha dejado (la memoria USB, X:)" dv3 texto 30 520 760 545 "Informe guardado en X:\\DISPOSIT.TXT"
mcopy -n -i $Y/mem.img ::DISPOSIT.TXT $Y/disposit.437 > /dev/null 2>&1
iconv -f CP437 -t UTF-8 < $Y/disposit.437 2> /dev/null | tr -d '\r' > $Y/disposit.txt
S=$Y/disposit.txt
mira "el informe (X:\\DISPOSIT.TXT): el equipo, el resumen y, lo primero, lo que no tiene controlador" \
  "^Administrador de dispositivos de Hispa-Pentium$" "^Resumen: [0-9]* dispositivos; [0-9]* funcionan, [1-9][0-9]* SIN CONTROLADOR" \
  "^Lo que falta (sin controlador o con error):$" "^  \[!\]  QEMU USB Tablet  \[0627:0001; Teclado y ratón\]$"
mira "el PCI: cada aparato con su nombre, su identificador, su controlador, sus zonas medidas y su IRQ" \
  "^  \[OK\] Intel 440FX: puente al procesador  \[8086:1237\]$" "^  \[OK\] QEMU/Bochs: tarjeta gráfica (VBE de Bochs)  \[1234:1111\]$" \
  "^       Memoria: FD000000-FDFFFFFF (16 MB)$" "^       Controlador: video.cas (en el núcleo)$" "^  \[OK\] Intel ICH4: USB 2.0 (EHCI)  \[8086:24CD\]$" \
  "^       Controlador: ehci.cas (en el núcleo)$" "^       Clase: 010601 (Controladora SATA (AHCI))$" "^       Controlador: ahci.cas (en el núcleo)$" \
  "^       Interrupción (IRQ): [0-9]*$"
mira "la red: la e1000 la lleva RED.BIN, con su MAC, el enlace y la IP" "^  \[OK\] Intel PRO/1000 MT (82540EM)  \[8086:100E\]$" \
  "^       Controlador: RED.BIN (un servicio del anillo 3)$" "^       MAC: 52:54:00:12:34:56$" "^       Enlace: sí, a 1000 Mbit/s$" "^       Detalle: conectada: IP 10.0.2.15$" \
  "^       Puertos: C0[0-9A-F]*-C0[0-9A-F]* (64 octetos)$"
mira "el USB: el teclado y el ratón (usbhid.cas), la memoria (usbalm.cas), con su nombre de verdad (sus descriptores de cadena) y de qué controladora cuelgan" \
  "^  \[OK\] QEMU USB Keyboard  \[0627:0001\]$" "^  \[OK\] QEMU USB Mouse  \[0627:0001\]$" "^       Controlador: usbhid.cas (en el núcleo)$" \
  "^  \[OK\] QEMU USB HARDDRIVE  \[46F4:0001\]$" "^       Controlador: usbalm.cas (en el núcleo)$" "^       Cuelga de: Intel ICH4: USB 2.0 (EHCI)$"
mira "los discos: el SATA y la memoria, con su tamaño y sus unidades" "^  \[OK\] QEMU HARDDISK (64 MB)$" "^       Detalle: 64 MB; unidades C:; de solo lectura$" \
  "^       Detalle: 32 MB; unidades X:; se puede escribir$" "^       Cuelga de: QEMU USB HARDDRIVE$"
mira "lo de siempre: el teclado y el ratón PS/2, el reloj, el 8253, los 8259 y COM1, con lo que dice ACPI de ellos" \
  "^  \[OK\] Teclado PS/2  \[PNP0303\]$" "^       En ACPI: .*KBD_$" "^  \[OK\] Ratón PS/2  \[PNP0F13\]$" "^  \[OK\] Reloj de la CMOS (fecha y hora)  \[PNP0B00\]$" \
  "^  \[OK\] Puerto serie COM1  \[PNP0501\]$" "^       Puertos: 03F8-03FF (8 octetos)$"
mira "ACPI: el sistema (ACPI.BIN), la batería con su carga y su salud, el adaptador, y lo que no tiene controlador (la disquetera, el puerto paralelo, el HPET)" \
  "^  \[OK\] Sistema ACPI (apagar, reiniciar y la batería)  \[ACPI\]$" "^  \[OK\] Batería  \[PNP0C0A\]$" "^       Carga: 55 %, cargando$" \
  "^       Salud: 68 % (guarda 5000 de 7350 mAh" "^  \[OK\] Adaptador de corriente  \[ACPI0003\]$" "^       Corriente: enchufado$" "^  \[!\]  Controladora de disquetes  \[PNP0700\]$" \
  "^  \[OK\] Bus PCI  \[PNP0A03\]$"
mira "el procesador (CPUID)" "^== Procesador ==$" "^  \[OK\] QEMU Virtual CPU" "^       Firma: [0-9A-F]* (CPUID 1)$"
miracap "las propiedades (Intro en la tarjeta gráfica): General, con su signo y su estado" dv4 negrita 100 60 700 100 "Propiedades: QEMU/Bochs" \
  negrita 100 120 400 160 General texto 100 120 400 160 Controlador texto 100 120 400 160 Recursos texto 100 150 700 400 "Funciona: lo lleva video.cas." \
  texto 100 150 700 400 "800 x 600, 32 bits"
miracap "la pestaña Controlador: el controlador, el identificador, el subsistema y la clase" dv5 negrita 100 120 400 160 Controlador \
  texto 100 150 700 400 "video.cas (en el núcleo)" texto 100 150 700 400 "PCI 1234:1111" texto 100 150 700 400 "030000 (Tarjeta gráfica (VGA))"
miracap "la pestaña Recursos: sus zonas de memoria, con lo que miden" dv6 negrita 100 120 400 160 Recursos texto 100 150 700 400 "FD000000-FDFFFFFF (16 MB)"
miracap "«Copiar informe»: al portapapeles" dv7 texto 30 520 760 545 "Informe copiado al portapapeles"
miracap "Configuración, con «Dispositivos...»" dv8 negrita 160 120 700 160 "Configuración" negrita 170 380 480 420 "Dispositivos..."
miracap "y desde ahí, la ventana otra vez" dv9 negrita 30 15 400 40 "Administrador de dispositivos"
if ! grep -q "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $Y/pc.sal; then ok "ni una excepción del núcleo ni un error de ACPICA (recorriendo el espacio de nombres y los _CRS)"
else ko "hay excepciones o errores:"; grep "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $Y/pc.sal | head -4 | sed 's/^/        /'; fi
# 2. «q35» (ICH9), sin escritorio: el informe, en texto, por la salida
QXHCI="-device qemu-xhci,id=x -device usb-kbd,bus=x.0 -device usb-hub,bus=x.0,id=h,port=2 -device usb-mouse,bus=x.0,port=2.1"
MAQUINA=q35 MODULOS=$Y/ACPI.BIN,$Y/DISPOSIT.BIN,$Y/ORDENES.BIN,$Y/RED.BIN ARRANQUE="ordenes conred acpicada=1" \
  QEMU_MAS="-S $SSDT -netdev user,id=n0 -device rtl8139,netdev=n0 -drive id=d0,file=$Y/sata.img,format=raw,if=none,snapshot=on -device ide-hd,drive=d0,bus=ide.0 $QXHCI" \
  $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "${PUESTA[@]}" ":monitor cont" ":pausa 8" "disposit /informe" ":pausa 6" > $Y/q35.sal 2>&1
tr -d '\r' < $Y/q35.sal > $Y/q35.txt
S=$Y/q35.txt
mira "q35: el informe en texto (sin escritorio), con la ICH9: el puente al procesador, el LPC, el AHCI de la placa con su disco y el SMBus sin controlador" \
  "^  \[OK\] Intel Q35: puente al procesador  \[8086:29C0\]$" "^  \[OK\] Intel ICH9: puente LPC  \[8086:2918\]$" "^  \[OK\] Intel ICH9: SATA (AHCI)  \[8086:2922\]$" \
  "^       Detalle: 1 discos SATA" "^  \[!\]  Intel ICH9: SMBus  \[8086:2930\]$" "^  \[OK\] QEMU HARDDISK (64 MB)$"
mira "q35: la RTL8139 (RED.BIN), la xHCI con el teclado y, detrás de un concentrador, el ratón" "^  \[OK\] Realtek RTL8139 (Fast Ethernet)  \[10EC:8139\]$" \
  "^       Controlador: RED.BIN (un servicio del anillo 3)$" "^  \[OK\] QEMU: controladora USB 3 (xHCI)  \[1B36:000D\]$" "^       Controlador: xhci.cas (en el núcleo)$" \
  "^  \[OK\] QEMU USB Hub  \[0409:55AA\]$" "^       Controlador: usb.cas (en el núcleo)$" "^       Cuelga de: QEMU USB Hub$" "^  \[OK\] QEMU USB Mouse  \[0627:0001\]$"
mira "q35: y la batería de ACPI" "^  \[OK\] Batería  \[PNP0C0A\]$" "^       Carga: 55 %, cargando$"
# 3. con «sinusb» y «sinacpi»: lo que no se usa, desactivado (y dicho así)
MODULOS=$Y/ACPI.BIN,$Y/DISPOSIT.BIN,$Y/ORDENES.BIN ARRANQUE="ordenes sinusb sinacpi" QEMU_MAS="-device usb-ehci,id=e" \
  $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 3" "disposit /informe" ":pausa 5" > $Y/sin.sal 2>&1
tr -d '\r' < $Y/sin.sal > $Y/sin.txt
S=$Y/sin.txt
mira "con «sinusb» y «sinacpi»: la controladora USB y ACPI, desactivadas (con el porqué)" "^  \[--\] Intel ICH4: USB 2.0 (EHCI)  \[8086:24CD\]$" \
  "^       Detalle: «sinusb» en la línea del cargador$" "^  \[--\] Sistema ACPI (apagar, reiniciar y la batería)  \[ACPI\]$" "^       Detalle: «sinacpi» en la línea del cargador$"
if ! cat $Y/q35.sal $Y/sin.sal | grep -q "excepción\|\*\*\*"; then ok "y ni una excepción del núcleo en el q35 ni sin USB"
else ko "el núcleo se ha quejado:"; cat $Y/q35.sal $Y/sin.sal | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el Administrador de dispositivos)"
fi   # el Administrador de dispositivos
# ------------------------------------------- 20. SSH, WireGuard y Tailscale
# (ssh; docs/PENTIUM.md §14 quinquies) Los programas en C de
# nucleo32/hispac/ (hacer.sh): SSHD contra el cliente OpenSSH de aquí (QEMU
# con hostfwd), SSH contra un sshd de OpenSSH propio de la prueba (puerto alto,
# ForceCommand: no da una shell); y, con root, ip netns y wg, WireGuard contra
# el del núcleo Linux en un espacio de nombres propio; con HEADSCALE= (el
# binario), Tailscale contra Headscale y un tailscaled de pares, en otro. Sin
# eso, esas dos se saltan (y se dice).
if parte08 ssh; then
echo "== Hispa-Pentium: SSH (SSH.BIN, SSHD.BIN), WireGuard (WG.BIN) y Tailscale (TAILSCAL.BIN) =="
R=$P/ssh
rm -rf $R; mkdir -p $R
export MTOOLS_SKIP_CHECK=1
if bash $AQUI/nucleo32/hispac/hacer.sh $R/bin > $R/hacer.txt 2>&1; then
  ok "nucleo32/hispac/hacer.sh: SSH.BIN, SSHD.BIN, WG.BIN y TAILSCAL.BIN con Monocypher ($(stat -c %s $R/bin/SSH.BIN), $(stat -c %s $R/bin/SSHD.BIN), $(stat -c %s $R/bin/WG.BIN) y $(stat -c %s $R/bin/TAILSCAL.BIN) octetos)"
else ko "nucleo32/hispac/hacer.sh no los hace: $(tail -n 1 $R/hacer.txt)"; fi
ssh-keygen -q -t ed25519 -N '' -f $R/clave -C prueba
PS=$((30000 + RANDOM % 10000)); PH=$((PS + 1))
bash $AQUI/nucleo32/hispac/prueba_red.sh sshd $R/sshd $PS
if $AQUI/nucleo32/disco.sh $R/d.img 16 red tls ssh > $R/disco.txt 2>&1 && mmd -i $R/d.img ::SSH && cp $R/clave.pub $R/AUT.TXT && mcopy -i $R/d.img $R/AUT.TXT ::SSH/AUTORIZA.TXT \
   && mcopy -i $R/d.img $R/sshd/ID_ED.SSH ::SSH/ID_ED.SSH && [ -s $R/sshd/sshd.pid ]; then
# 1. SSHD, con el ssh de aquí: una orden y una sesión con terminal
cp $R/d.img $R/a.img
( for _ in $(seq 120); do sleep 1; timeout 2 bash -c "</dev/tcp/127.0.0.1/$PH" 2> /dev/null && break; done
  sleep 20
  timeout 60 ssh -p $PH -o StrictHostKeyChecking=no -o UserKnownHostsFile=$R/conocidos -o UpdateHostKeys=no -o IdentitiesOnly=yes -o BatchMode=yes -i $R/clave prueba@127.0.0.1 \
    "echo hola desde hispa" < /dev/null > $R/orden.txt 2>&1; echo "código $?" >> $R/orden.txt
  printf 'echo dentro de la consola\rver\rsalir\r' > $R/teclas.txt
  setsid timeout 60 ssh -tt -p $PH -o StrictHostKeyChecking=no -o UserKnownHostsFile=$R/conocidos -o UpdateHostKeys=no -o IdentitiesOnly=yes -o BatchMode=yes -i $R/clave \
    prueba@127.0.0.1 < $R/teclas.txt > $R/tty.txt 2>&1
  echo "código $?" >> $R/tty.txt ) &
FUERA=$!
ESCRIBE=1 DISCO=$R/a.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-netdev user,id=n0,hostfwd=tcp:127.0.0.1:$PH-:22 -device e1000,netdev=n0" \
  $AQUI/nucleo32/probar.sh "" ":pausa 3" "sshd /encender" ":pausa 75" "sshd" ":pausa 2" > $R/a.sal 2>&1
wait $FUERA
tr -d '\r' < $R/a.sal > $R/a.txt
S=$R/a.txt
mira "SSHD: apagado hasta que se enciende; con /ENCENDER escucha en el puerto 22, con su llave del equipo (ssh-ed25519) hecha la primera vez" \
  "^Servidor SSH encendido (puerto 22)" "sshd: llave del equipo nueva (ssh-ed25519)$" "red: tcp: escuchando en el puerto 22$" \
  "sshd: Hispa-Pentium: el servidor SSH, en el puerto 22 (llave SHA256:"
mira "SSHD con el cliente OpenSSH: curve25519-sha256, chacha20-poly1305 y kex-strict; entra con la llave de AUTORIZA.TXT" \
  "sshd: la otra punta: SSH-2.0-OpenSSH" "sshd: kex-strict: sí (contra Terrapin)$" "sshd: cifrado: curve25519-sha256, ssh-ed25519, chacha20-poly1305@openssh.com$" \
  "sshd: entra prueba con su llave$" "sshd: orden: echo hola desde hispa$" "^El servidor SSH de Hispa-Pentium: encendido (puerto 22)"
S=$R/orden.txt
mira "ssh equipo orden: la orden se hace en Hispa-Pentium (ORDENES /C) y vuelve lo que escribe, con el código 0" "^hola desde hispa$" "^código 0$"
if grep -q "hola desde hispa.$(printf '\r')" $R/orden.txt; then ko "sin terminal, los finales de línea tienen que ir como en Unix (sin CR)"; else ok "sin terminal (una orden), los finales de línea van como en Unix"; fi
tr -d '\r' < $R/tty.txt > $R/tty2.txt
S=$R/tty2.txt
mira "una sesión con terminal: la Consola de Hispa-Pentium (en UTF-8), las órdenes que se teclean al otro lado y SALIR" \
  "^Hispa-Pentium: intérprete de órdenes" "^dentro de la consola$" "^Intérprete de órdenes del anillo 3 (ORDENES.BIN).$" "^Hasta luego.$" "^código 0$"
# 2. SSH, con un sshd de OpenSSH: la huella, la llave propia, el terminal y una orden
cp $R/d.img $R/b.img
CAPTURAS=$R ESCRIBE=1 DISCO=$R/b.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 3" \
  "ssh -p $PS root@10.0.2.2" ":pausa 10" ":escribe s" ":pausa 6" ":texto b1" "hola mundo" ":pausa 2" ":texto b2" ":pausa 4" \
  "ssh -p $PS root@10.0.2.2 una orden" ":pausa 10" ":texto b3" > $R/b.sal 2>&1
tr -d '\r' < $R/b.sal > $R/b.txt
S=$R/b.txt
mira "SSH con OpenSSH: la primera vez enseña la huella y pregunta; entra con la llave propia (ID_ED.SSH)" \
  "ssh: la otra punta: SSH-2.0-OpenSSH" "ssh: kex-strict: sí (contra Terrapin)$" "ssh: dentro, con la llave propia$" "ssh: sesión acabada (código 0)$" "ssh: sesión acabada (código 3)$"
S=$R/b1.txt
mira "el terminal de SSH (en la pantalla de texto): borrar, colores, mover el cursor y el UTF-8 (a la página 437)" \
  "^|HOLA DESDE OPENSSH TERM=xterm" "^|          caja azul  ─ñ" "^|Escriba algo:"
S=$R/b2.txt
mira "lo que se teclea va al otro lado (y vuelve: «leido»)" "^|leido: \[hola mundo\]"
S=$R/b3.txt
mira "SSH con una orden: sin terminal, lo que sale, aquí mismo, y el código de salida de allí" "^|orden: una orden$" "^|(SSH.BIN ha acabado con el código 3)"
S=$R/b.txt
if grep -q "^\[10.0.2.2\]:$PS ssh-ed25519 " <(mtype -i $R/b.img ::SSH/CONOCIDO.TXT 2> /dev/null); then ok "la llave del servidor queda en \\HISPA\\SSH\\CONOCIDO.TXT (la segunda vez no se pregunta)"
else ko "no está la llave del servidor en CONOCIDO.TXT"; fi
# 3. WireGuard contra el del núcleo Linux (root, ip netns y wg)
if [ "$(id -u)" = 0 ] && command -v wg > /dev/null && ip netns list > /dev/null 2>&1; then
  cp $R/d.img $R/w.img
  bash $AQUI/nucleo32/hispac/prueba_red.sh wg $R/wg $R/w.img
  tr -d '\r' < $R/wg/q.sal > $R/w.txt
  S=$R/w.txt
  mira "WireGuard (WG /ENCENDER, el archivo de wg-quick): el apretón de manos con el WireGuard de Linux y la interfaz del túnel en RED.BIN" \
    "red: túnel: la interfaz, con la IP 10.9.0.2 y 1 redes$" "wg: Hispa-Pentium: WireGuard en marcha (10.9.0.2, clave pública " \
    "wg: 10.0.2.2: túnel hecho (claves nuevas)$" "^Respuesta desde 10.9.0.1: octetos=32 " "^  último apretón de manos: "
  S=$R/wg/fuera.txt
  mira "por el túnel, desde Linux: ping a Hispa-Pentium y SSH a su SSHD" "^64 bytes from 10.9.0.2" "latest handshake" "^hola por wireguard$" "^código 0$"
else echo "  (WireGuard: hace falta root, ip netns y wg: se salta)"; fi
# 4. Tailscale contra Headscale (HEADSCALE=...) y un tailscaled de pares
if [ "$(id -u)" = 0 ] && [ -x "${HEADSCALE:-}" ] && command -v tailscaled > /dev/null && ip netns list > /dev/null 2>&1; then
  cp $R/d.img $R/t.img
  bash $AQUI/nucleo32/hispac/prueba_red.sh tailscale $R/ts $R/t.img $PS
  tr -d '\r' < $R/ts/q.sal > $R/t.txt
  S=$R/t.txt
  mira "Tailscale: ts2021 con Headscale, la dirección de alta (que aprueba la prueba) y el registro" \
    "tailscale: control: conectado con 10.0.2.2 (ts2021)$" "tailscale: dar de alta este equipo: abra https://10.0.2.2:8443/register/" \
    "^  https://10.0.2.2:8443/register/" "tailscale: registrado en la red de prueba$"
  mira "el mapa de la red: la IP 100.x, el relevo DERP de casa, las reglas, la interfaz del túnel y los nombres (MagicDNS)" \
    "tailscale: relevo de casa: derp-999 (Relevo de prueba)$" "tailscale: reglas de la red: [1-9]" "red: túnel: la interfaz, con la IP 100.64.0." \
    "tailscale: en la red: hispa-pentium.prueba.hispa., 100.64.0." "tailscale: derp-999: conectado con 10.0.2.2$" \
    "^Este equipo: 100.64.0.[0-9]*  hispa-pentium.prueba.hispa." "^  100.64.0.[0-9]* *pares-vps *en línea"
  mira "con el otro equipo, por DERP: el apretón de WireGuard, ping por el nombre y SSH a su sshd" \
    "tailscale: pares-vps: túnel hecho" "^Haciendo ping a pares-vps \[100.64.0." "^Respuesta desde 100.64.0." "ssh: conectando con pares-vps (100.64.0." "ssh: dentro, con la llave propia$"
  S=$R/ts/fuera.txt
  mira "y desde el otro equipo (tailscaled), SSH al SSHD de Hispa-Pentium por la red de Tailscale" "hispa-pentium" "^hola por tailscale$" "^código 0$"
else echo "  (Tailscale: hace falta root, ip netns, tailscaled y HEADSCALE=el binario de Headscale: se salta)"; fi
if ! cat $R/*.txt | grep -q "excepción\|\*\*\*"; then ok "en todos esos arranques, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $R/*.txt | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
else ko "no se ha podido hacer el disco de las pruebas de SSH: $(tail -n 1 $R/disco.txt)"; fi
[ -s $R/sshd/sshd.pid ] && kill "$(cat $R/sshd/sshd.pid)" 2> /dev/null
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin SSH, WireGuard ni Tailscale)"
fi   # ssh

# --------------------------------- 21. (CD) los lectores de CD y DVD y las imágenes
if parte08 lectores; then
echo "== Hispa-Pentium: los CD y DVD (IDE, AHCI y USB; ISO 9660 con Joliet), meter y sacar el disco, y las imágenes de disco =="
L=$P/lectores
mkdir -p $L $G
export MTOOLS_SKIP_CHECK=1
if command -v genisoimage > /dev/null; then HAZISO="genisoimage -quiet"; else HAZISO="xorriso -as mkisofs -quiet"; fi
[ -f $P/d16.img ] || $AQUI/nucleo32/disco.sh $P/d16.img 16 > $P/disco16.txt 2>&1
[ -f $P/d32.img ] || $AQUI/nucleo32/disco.sh $P/d32.img 32 > $P/disco32.txt 2>&1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $L/semilla.asm > /dev/null 2>&1 && nasm -f bin $L/semilla.asm -o $L/SEMILLA.BIN
# la ISO de prueba: nombres largos (Joliet, y Rock Ridge), una carpeta dentro de otra y un archivo de 300 K
rm -rf $L/iso $L/iso2; mkdir -p "$L/iso/Carpeta con nombre largo/sub" $L/iso2
printf 'Hola desde el CD de prueba\r\n' > $L/iso/LEEME.TXT
printf 'Texto con nombre largo, en Joliet.\r\n' > "$L/iso/Carpeta con nombre largo/documento-de-prueba-largo.txt"
printf 'dentro\r\n' > "$L/iso/Carpeta con nombre largo/sub/hondo.txt"
python3 -I -c "import random; random.seed(18); open('$L/iso/grande.bin', 'wb').write(bytes(random.randrange(256) for _ in range(300000)))"
$HAZISO -J -r -V PRUEBA_CD -o $L/joliet.iso $L/iso
$HAZISO -J -V OTRO -o $L/otro.iso $L/iso/LEEME.TXT
cp $L/joliet.iso $L/iso2/dentro.iso; $HAZISO -J -V CONTIENE -o $L/contiene.iso $L/iso2
LARGO="$L/iso/Carpeta con nombre largo/documento-de-prueba-largo.txt"
# ¿lo copiado al disco es igual que lo del CD? (la imagen y los archivos de ella)
igual_cd() { local img=$1 f; shift; for f in "$@"; do rm -f $L/sacado; mcopy -n -i $img ::$f $L/sacado 2> /dev/null || return 1
  case $f in DOC*) cmp -s $L/sacado "$LARGO" ;; *) cmp -s $L/sacado $L/iso/grande.bin ;; esac || return 1; done; }
if [ ! -f $L/joliet.iso ] || [ ! -f $L/SEMILLA.BIN ]; then ko "no se puede hacer la ISO de prueba (genisoimage o xorriso) o el núcleo"; else
# 1. como VirtualBox: un «pc» con el disco en SATA (AHCI) y el lector en el IDE secundario maestro
cp $P/d32.img $L/c.img
ESCRIBE=1 SATA=$L/c.img MAQUINA=pc ARRANQUE=ordenes MODULOS= QEMU_MAS="-drive id=cd,file=$L/joliet.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.1,unit=0" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "dir d:\\" "type d:\\leeme.txt" "dir d:\\carpet~1" "type d:\\carpet~1\\sub\\hondo.txt" "copy d:\\grande.bin c:\\grande.bin" \
  "copy d:\\carpet~1\\docume~1.txt c:\\doc.txt" ":monitor eject -f cd" ":pausa 3" "dir d:\\" ":monitor change cd $L/otro.iso" ":pausa 3" "dir d:\\" \
  ":monitor change cd $L/joliet.iso" ":pausa 3" "type d:\\leeme.txt" "expulsar d:" "montar" salir disco apagar > $L/vbox.sal 2>&1
S=$L/vbox.sal
mira "como VirtualBox (el disco en SATA, el lector en el IDE secundario maestro): ATAPI por PIO, el lector es D: y su ISO 9660 se monta" \
  "^disco 0: QEMU HARDDISK, 40 MB, FAT32 (SATA)\.$" "^disco 1: QEMU DVD-ROM, lector de CD o DVD (IDE)\.$" "cd: D: el lector QEMU DVD-ROM (IDE)$" \
  "cd: D: CD de datos: «PRUEBA_CD», ISO 9660 Joliet, 1 MB\.$" "^unidades: C: (disco 0, SATA, FAT32) D: (disco 1, CD, ISO 9660); la del sistema, C:"
mira "DIR y TYPE en el CD: los nombres largos de Joliet (y el corto que les toca, CARPET~1), carpetas dentro de carpetas" \
  "^ Directorio de D:.$" "^CARPET~1 *<DIR> .*Carpeta con nombre largo$" "^GRANDE   BIN      300000 " "^Hola desde el CD de prueba$" \
  "^DOCUME~1 TXT          36 .*documento-de-prueba-largo\.txt$" "^dentro$"
if igual_cd $L/c.img GRANDE.BIN DOC.TXT; then ok "lo copiado del CD al disco es igual que lo de la ISO, octeto a octeto (300 K y el del nombre largo)"
else ko "lo copiado del CD (IDE) no es igual que lo de la ISO"; fi
mira "sacar el disco, meter otro y volver al primero (el monitor de QEMU: eject y change): la unidad se va y vuelve sola, con aviso" \
  "cd: D: sin disco\.$" "cd: aviso: CD retirado: D:$" "^No puedo abrir d:.: no hay disco" "cd: D: CD de datos: «OTRO», ISO 9660 Joliet" "cd: aviso: CD insertado: D:$"
mira "EXPULSAR D: abre la bandeja; MONTAR dice lo que hay en el lector" "cd: D: expulsado\.$" "^Ya puede sacar el disco de D:\.$" "^  D:  lector de CD o DVD QEMU DVD-ROM: sin disco$"
# 2. por AHCI: el q35, con el lector en un puerto SATA (la firma EB140101)
cp $P/d16.img $L/q.img
ESCRIBE=1 DISCO=$L/q.img MAQUINA=q35 ARRANQUE=ordenes MODULOS= QEMU_MAS="-drive id=cd,file=$L/joliet.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.2" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "copy d:\\grande.bin c:\\grande.bin" "copy d:\\carpet~1\\docume~1.txt c:\\doc.txt" ":monitor change cd $L/otro.iso" ":pausa 3" "dir d:\\" salir apagar > $L/ahci.sal 2>&1
S=$L/ahci.sal
mira "por AHCI (el q35): el puerto con la firma EB140101 es un lector, ATAPI por DMA (PACKET con el bit A y el paquete en ACMD)" \
  "^disco 1: QEMU DVD-ROM, lector de CD o DVD (SATA)\.$" "cd: D: CD de datos: «PRUEBA_CD», ISO 9660 Joliet" "^ *1 archivo(s) copiado(s)\.$" "cd: D: CD de datos: «OTRO»"
if igual_cd $L/q.img GRANDE.BIN DOC.TXT; then ok "y lo copiado del CD por AHCI es igual que lo de la ISO"; else ko "lo copiado del CD (AHCI) no es igual que lo de la ISO"; fi
# 3. por USB: un lector de CD (usb-storage con la ISO: SCSI, clase 5) al arrancar, y otro enchufado en marcha
cp $P/d16.img $L/u.img
ESCRIBE=1 DISCO=$L/u.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-device qemu-xhci,id=u -drive id=ucd,file=$L/joliet.iso,media=cdrom,if=none -device usb-storage,bus=u.0,drive=ucd,id=ulector -drive id=ucd2,file=$L/otro.iso,media=cdrom,if=none" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "copy e:\\grande.bin c:\\grande.bin" "copy e:\\carpet~1\\docume~1.txt c:\\doc.txt" ":monitor eject -f ucd" ":pausa 3" \
  ":monitor device_add usb-storage,bus=u.0,drive=ucd2,id=ulector2" ":pausa 4" "type f:\\leeme.txt" ":monitor device_del ulector" ":pausa 3" "montar" salir apagar > $L/usb.sal 2>&1
S=$L/usb.sal
mira "por USB: un lector de CD (almacenamiento masivo, SCSI de la clase 5) es una unidad de CD más (E:; D: es el lector vacío de QEMU)" \
  "^disco 2: QEMU    QEMU CD-ROM, lector de CD o DVD (USB)\.$" "cd: E: CD de datos: «PRUEBA_CD», ISO 9660 Joliet" "cd: aviso: CD retirado: E:$"
mira "y enchufado en marcha, se lee al momento; desenchufado, se va (y su letra)" "cd: aviso: Lector de CD conectado: F:$" "^Hola desde el CD de prueba$" \
  "cd: aviso: Lector de CD desconectado: E:$" "^  F:  lector de CD o DVD QEMU    QEMU CD-ROM: «OTRO»$"
if igual_cd $L/u.img GRANDE.BIN DOC.TXT; then ok "y lo copiado del CD por USB es igual que lo de la ISO"; else ko "lo copiado del CD (USB) no es igual que lo de la ISO"; fi
# 4. las imágenes de disco: la misma ISO, un disquete (.IMG, FAT12) y la ISO en crudo (.CUE y .BIN, MODE1/2352), desde C:
cp $P/d16.img $L/i.img
mmd -i $L/i.img ::DISCOS; mcopy -i $L/i.img $L/joliet.iso ::DISCOS/PRUEBA.ISO
rm -f $L/flop.img; mformat -C -f 1440 -i $L/flop.img ::; printf 'en el disquete\r\n' > $L/disq.txt; mcopy -i $L/flop.img $L/disq.txt ::DISQUETE.TXT; mcopy -i $L/i.img $L/flop.img ::DISCOS/FLOP.IMG
python3 -I - $L <<'PY'
import sys
L = sys.argv[1]
d = open(L + '/joliet.iso', 'rb').read()
o = bytearray()
for i in range(len(d) // 2048):
    o += b'\x00' + b'\xff' * 10 + b'\x00' + bytes(3) + b'\x01' + d[i * 2048:(i + 1) * 2048] + bytes(288)
open(L + '/crudo.bin', 'wb').write(o)
open(L + '/crudo.cue', 'w').write('FILE "CRUDO.BIN" BINARY\r\n  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n')
PY
mcopy -i $L/i.img $L/crudo.bin ::DISCOS/CRUDO.BIN; mcopy -i $L/i.img $L/crudo.cue ::DISCOS/CRUDO.CUE
ESCRIBE=1 DISCO=$L/i.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-drive id=cd,file=$L/joliet.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.1,unit=0" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "montar c:\\discos\\prueba.iso" "montar c:\\discos\\flop.img" "montar c:\\discos\\crudo.cue" "montar" "type f:\\disquete.txt" \
  "copy e:\\grande.bin c:\\grande.bin" "copy g:\\carpet~1\\docume~1.txt c:\\doc.txt" "copy g:\\grande.bin c:\\crudo.bin" "type e:\\carpet~1\\sub\\hondo.txt" \
  "desmontar e:" "desmontar f:" "montar" "montar c:\\leeme.txt" "desmontar c:" salir apagar > $L/imagen.sal 2>&1
S=$L/imagen.sal
mira "MONTAR una imagen de disco: la .ISO (la misma del lector), un disquete .IMG (FAT12) y una .CUE con su .BIN en crudo, cada una con la primera letra libre" \
  "^c:.discos.prueba\.iso está montada como la unidad E:\.$" "^c:.discos.flop\.img está montada como la unidad F:\.$" "^c:.discos.crudo\.cue está montada como la unidad G:\.$" \
  "^  E:  imagen C:.discos.prueba\.iso  «PRUEBA_CD»$" "^  F:  imagen C:.discos.flop\.img$" "^en el disquete$" "^dentro$"
if igual_cd $L/i.img GRANDE.BIN DOC.TXT CRUDO.BIN; then ok "lo leído de las imágenes (la .ISO y la .BIN en crudo) es igual que lo de la ISO del lector"; else ko "lo leído de una imagen montada no es igual que lo de la ISO"; fi
mira "DESMONTAR la quita; lo que no es una imagen, o no es una imagen montada, se dice" "^La unidad E: está desmontada\.$" "^La unidad F: está desmontada\.$" \
  "^No puedo montar c:.leeme\.txt: no es una imagen de disco que se pueda leer" "^C: no es un CD ni una imagen montada\.$"
# 5. una imagen dentro del CD y otra en una memoria USB que se desenchufa: se desmontan solas
cp $P/d16.img $L/m.img; mcopy -i $L/m.img $L/joliet.iso ::PRUEBA.ISO
DISCO=$P/d16.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-drive id=cd,file=$L/contiene.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.1,unit=0 -device qemu-xhci,id=u -drive id=mu,file=$L/m.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu,id=memo" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "montar d:\\dentro.iso" "montar x:\\prueba.iso" "type e:\\leeme.txt" "type f:\\leeme.txt" ":monitor device_del memo" ":pausa 3" "montar" \
  ":monitor eject -f cd" ":pausa 3" "montar" salir apagar > $L/base.sal 2>&1
S=$L/base.sal
mira "una imagen dentro del CD y otra en la memoria USB: si se va la memoria, o se saca el CD, su imagen se desmonta sola (sin colgarse)" \
  "^d:.dentro\.iso está montada como la unidad E:\.$" "^x:.prueba\.iso está montada como la unidad F:\.$" "cd: aviso: Imagen desmontada: F: (su disco se ha ido)$" \
  "^  E:  imagen D:.dentro\.iso  «PRUEBA_CD»$" "cd: aviso: Imagen desmontada: E: (su disco se ha ido)$" "^  D:  lector de CD o DVD QEMU DVD-ROM: sin disco$"
if ! cat $L/*.sal | grep -q "excepción\|\*\*\*"; then ok "en todo eso, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $L/*.sal | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
# 6. el escritorio: el explorador (Mi equipo) con el CD (D:), y una .ISO
# montada al abrirla (el CD virtual, E:), que «Expulsar» desmonta
cp $P/d16.img $L/e.img; mmd -i $L/e.img ::DISCOS; mcopy -i $L/e.img $L/joliet.iso ::DISCOS/PRUEBA.ISO
CAPTURAS=$G ESCRIBE=1 DISCO=$L/e.img ARRANQUE=escritorio MODULOS= QEMU_MAS="-drive id=cd,file=$L/joliet.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.1,unit=0" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN ":pausa 3" ":doble 62 30" ":pausa 3" ":captura cd18a" ":tecla f4" ":escribe C:\\DISCOS" ":tecla ret" ":pausa 2" \
  ":tecla ret" ":pausa 3" ":raton 600 440" ":captura cd18b" ":tecla backspace" ":pausa 2" ":raton 600 440" ":captura cd18e" ":clicd 330 222" ":pausa 1" ":captura cd18c" ":clic 370 257" ":pausa 2" \
  ":raton 600 440" ":captura cd18d" > $L/escritorio.sal 2>&1
S=$L/escritorio.sal
miracap "Mi equipo (el explorador) enseña el CD con su nombre y su letra, lo que tiene y que es de solo lectura" cd18a negrita 265 133 275 143 "HISPA (C:)" \
  negrita 515 133 525 143 "PRUEBA_CD (D:)" texto 515 151 525 161 "670 K, ISO 9660" texto 515 167 525 177 "(de solo lectura)"
miracap "abrir una .ISO la monta (E:) y entra en ella; el aviso de la bandeja es de los CD y DVD" cd18b texto 100 98 130 112 "E:\\" texto 200 130 700 400 "grande.bin" \
  texto 10 474 700 490 "PRUEBA.ISO, montada como E:" negrita 570 505 640 520 "CD y DVD"
miracap "el CD virtual sale en Mi equipo («CD virtual», con lo que tiene)" cd18e negrita 265 195 275 205 "PRUEBA_CD (E:)" texto 265 213 275 223 "CD virtual: 670 K"
miracap "y su menú (el botón secundario): «Expulsar»" cd18c texto 335 242 355 267 "Expulsar"
miracap "y «Expulsar» la quita de Mi equipo" cd18d notexto 216 190 460 212 "(E:)" texto 10 474 700 490 "desmontada"
mira "y «Expulsar» la desmonta" "cd: E: la imagen C:.DISCOS.PRUEBA\.ISO (sectores de 2048)$" "cd: E: imagen desmontada\.$" "escritorio: globo: Imagen desmontada: E:"
fi   # la ISO de prueba
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin los CD y DVD)"
fi   # los lectores

# ------------------------------- 22. [WIN32] Los programas de Windows y DirectX
# WIN32.BIN (nucleo32/win32/): el cargador de PE y KERNEL32, USER32, GDI32...
# en C, con las DLL de Hispa-486 (MSVCRT, COMCTL32, DDRAW, DSOUND, DINPUT,
# WINMM); el disco, de nucleo32/win32/disco.sh (FAT32, con lo de las pruebas)
if parte08 win32; then
echo "== Hispa-Pentium: los programas de Windows de 32 bits y DirectX (WIN32.BIN: consola, ventanas, Naves, FASMW, red) =="
V=$P/w32
mkdir -p $V $G
export MTOOLS_SKIP_CHECK=1
if bash $AQUI/nucleo32/win32/disco.sh $V/d.img > $V/disco.txt 2>&1; then
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $V/semilla.asm > /dev/null 2>&1 && nasm -f bin $V/semilla.asm -o $V/SEMILLA.BIN
HAY_TP=0; mdir -i $V/d.img ::NASM > /dev/null 2>&1 && HAY_TP=1
# 1. los de consola, en el intérprete de órdenes (modo texto)
O=("cd \\PRUEBAS" "PEHOLA uno dos" ":pausa 2" "HOLAC tres" ":pausa 3" "K32PRUE" ":pausa 6" "USADLL" ":pausa 2")
[ $HAY_TP = 1 ] && O+=("cd \\NASM" "nasm -v" ":pausa 4" "nasm -f win32 hola.asm -o hola.obj" ":pausa 8" "dir hola.obj" ":pausa 1" \
  "cd \\TCC" "tcc hola.c -o hola.exe" ":pausa 12" "hola" ":pausa 3" "cd \\FASM\\DDRAW" "\\FASM\\FASM DDRAW.ASM" ":pausa 12" "dir ddraw.exe" ":pausa 1")
ESCRIBE=1 DISCO=$V/d.img ARRANQUE=ordenes MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN "${O[@]}" > $V/consola.sal 2>&1
tr -d '\r' < $V/consola.sal > $V/consola.txt
S=$V/consola.txt
mira "un .EXE se lanza por su nombre: el núcleo ve «MZ» y lo corre WIN32.BIN (PEHOLA: KERNEL32 a mano, su línea de órdenes y su código de salida)" \
  "^Hola desde Win32$" "^Linea: C:.PRUEBAS.PEHOLA.EXE uno dos$" "(PEHOLA.EXE ha acabado con el código 42)"
mira "HOLAC, en C (gcc y tools/elf2pe.py), con MSVCRT.DLL de Hispa-486" "^Hola desde C (gcc) en Hispa-486$" "^Primo 100: 541$" "^Suma: 704982704$" "(HOLAC.EXE ha acabado con el código 7)"
mira "K32PRUE: KERNEL32 de consola da lo que da Windows 98 (versión, archivos, búsquedas, carpetas, la hora, el montón, el entorno, TLS, los módulos)" \
  "^version: C0000A04$" "^versionex: 4.10 plataforma 1 build 1998$" "^escritos: 22$" "^leer3: uno$" "^crear_nuevo_existe: si error 80$" "^fin_busqueda: 18$" \
  "^ft_1970: 019DB1DED53E8000$" "^ida_y_vuelta: 2026-11-17 12:34:56.789 semana 2$" "^realloc: conservado 5000 0$" "^20000_vueltas: si$" "^tls: si 1234$" \
  "^getprocaddress: si 6$" "^modulo_propio: 00400000$" "^FIN K32PRUE$"
mira "una DLL propia (DLLPRUE.DLL, con su DllMain y sus datos exportados)" "^Doble(20) = 41$" "^contador = 42$"
if [ $HAY_TP = 1 ]; then
  mira "NASM 2.16 para Win32 (MinGW) dice su versión y ensambla un .ASM a un .OBJ de Win32" "^NASM version 2.16.03" "^hola.obj es un archivo de 196 octetos\.$"
  mira "TCC 0.9.27, el compilador de C de Windows, compila un «hola» (con su libtcc.dll) y el .EXE que hace corre" "^Hola desde TCC en Hispa-Pentium$" "(HOLA.EXE ha acabado con el código 7)"
  mira "FASM (el de consola) ensambla el ejemplo DDRAW de DirectDraw" "^flat assembler  version 1.73.32" "passes, .* seconds, 4608 bytes\.$" "^ddraw.exe es un archivo de 4608 octetos\.$"
else
  echo "  (sin NASM, TCC ni FASM: tests/thirdparty/fetch_win32.sh)"
fi
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda esa sesión, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# 2. los de ventanas, desde la Consola del escritorio: cada uno, su ventana
CAPTURAS=$G DISCO=$V/d.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" ":doble 60 186" ":pausa 4" \
  ":escribe \\HISPA32\\HOLA32" ":tecla ret" ":pausa 5" ":captura w1" ":tecla alt-f4" ":pausa 2" \
  ":escribe \\HISPA32\\AVISO32" ":tecla ret" ":pausa 4" ":captura w2" ":tecla ret" ":pausa 2" ":captura w3" ":tecla ret" ":pausa 2" \
  ":escribe \\HISPA32\\CALC32" ":tecla ret" ":pausa 5" ":clic 99 152" ":clic 156 152" ":clic 270 252" ":clic 156 186" ":clic 213 252" ":pausa 1" ":captura w4" \
  ":clic 175 88" ":pausa 1" ":captura w5" ":tecla esc" ":pausa 1" ":tecla alt-f4" ":pausa 2" \
  ":escribe \\HISPA32\\MENU32" ":tecla ret" ":pausa 5" ":captura w6" ":tecla alt-f4" ":pausa 2" \
  ":escribe \\HISPA32\\CTL32" ":tecla ret" ":pausa 5" ":captura w7" ":tecla alt-f4" ":pausa 2" > $V/ventanas.sal 2>&1
S=$V/ventanas.sal
miracap "HOLA32: un programa de Windows de ventanas es una ventana de Hispa-Pentium (su clase, su WM_PAINT, el texto y los dibujos de GDI, un temporizador)" w1 \
  negrita 100 60 140 90 "Hola desde Win32" texto 90 90 130 115 "¡Hola, mundo de Win32!" texto 90 150 130 175 "Temporizador: bien" texto 90 190 130 215 "Todo bien."
miracap "AVISO32: MessageBoxA es un cuadro de Hispa-Pentium, con sus botones en español" w2 \
  negrita 270 210 320 240 "Prueba de Win32" texto 280 250 320 275 "¿Le gusta Hispa-486?" texto 280 300 360 340 "Sí" texto 400 300 470 340 "No"
miracap "Intro: «Sí» (el botón de antes) y el cuadro siguiente con la respuesta" w3 texto 280 250 320 280 "Ha dicho que sí." texto 280 280 320 310 "Todo bien."
miracap "CALC32: los botones con el ratón (78 + 5 = 83) y la caja (EDIT)" w4 negrita 60 50 100 75 "Calculadora" texto 60 100 120 130 "83"
miracap "su menú (de los recursos), con el menú emergente del núcleo" w5 texto 60 80 200 100 "Archivo" texto 100 100 220 140 "Raíz cuadrada"
miracap "MENU32: los menús y los recursos (textos, iconos, menús de los recursos)" w6 negrita 40 40 100 70 "Menús y recursos" texto 40 60 120 90 "Archivo"
miracap "CTL32: los controles de serie (botones, casillas, opciones, una lista con sus cuatro colores, una lista desplegable, una caja de varias líneas)" w7 \
  negrita 40 40 100 70 "Controles de Win32" texto 40 70 120 100 "Nombre:" texto 260 70 340 100 "Saludar" texto 40 130 100 160 "Rojo" texto 40 160 100 190 "Azul" texto 40 170 100 200 "Amarillo"
if ! grep -q "excepción\|\*\*\*" $S; then ok "con todos esos, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# 3. Naves (DirectDraw a pantalla completa, DirectInput) desde Mi equipo: se
# juega; y el ejemplo DDRAW de FASM; y FASMW
# (en el explorador: la dirección, C:\\HISPA32, y la N lleva a NAVES.EXE)
CAPTURAS=$G DISCO=$V/d.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" ":doble 60 40" ":pausa 3" ":tecla f4" ":escribe C:\\HISPA32" ":tecla ret" ":pausa 2" \
  ":raton 600 440" ":captura n0" ":tecla n" ":tecla ret" ":pausa 9" ":captura n1" ":tecla spc" ":pausa 3" ":captura n2" \
  ":monitor sendkey left 1000" ":pausa 1" ":captura n3" ":monitor sendkey ctrl 200" ":pausa 0.3" ":captura n4" ":tecla esc" ":pausa 3" ":captura n5" > $V/naves.sal 2>&1
S=$V/naves.sal
miracap "Mi equipo ve los .EXE como programas y los abre (los corre WIN32.BIN)" n0 texto 100 98 220 112 "C:\\HISPA32" texto 210 130 790 470 "NAVES.EXE"
miracap "Naves: DirectDraw a pantalla completa (su modo, 320 x 240 a 8 bits, al doble y en el centro, sin la barra de tareas)" n1 \
  rgb 10 10 000000 rgb 790 590 000000 nrgb 40 585 37A53A 60
miracap "Espacio (DirectInput): empieza la partida, con las naves enemigas" n2 rgb 10 10 000000 distinto 80 60 640 200 $G/n1.ppm
miracap "la nave se mueve con las flechas (mantenidas) y dispara con Control" n3 distinto 80 380 640 100 $G/n2.ppm
miracap "Esc acaba el juego y vuelve el escritorio" n5 negrita 30 570 70 590 Inicio
if ! grep -q "excepción\|\*\*\*" $S; then ok "Naves: ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
if [ $HAY_TP = 1 ]; then
  CAPTURAS=$G DISCO=$V/d.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" ":doble 60 186" ":pausa 4" \
    ":escribe cd \\FASM\\DDRAW" ":tecla ret" ":escribe \\FASM\\FASM DDRAW.ASM" ":tecla ret" ":pausa 12" ":escribe DDRAW" ":tecla ret" ":pausa 10" ":captura d1" ":tecla esc" ":pausa 3" \
    ":escribe cd \\FASM" ":tecla ret" ":escribe FASMW" ":tecla ret" ":pausa 12" ":captura f1" ":tecla ctrl-o" ":pausa 3" ":captura f2" ":escribe hello.asm" ":tecla ret" ":pausa 4" \
    ":captura f3" ":tecla ctrl-f9" ":pausa 10" ":captura f4" ":tecla ret" ":pausa 2" ":tecla alt-f4" ":pausa 3" ":escribe hello" ":tecla ret" ":pausa 4" ":captura f5" > $V/fasm.sal 2>&1
  S=$V/fasm.sal
  miracap "el ejemplo DDRAW de FASM (ensamblado en Hispa-Pentium): DirectDraw a 640 x 480 con su paleta, en el centro" d1 rgb 10 10 000000 rgb 790 10 000000 nrgb 400 500 000000
  miracap "FASMW, el flat assembler de Windows: su menú (MENUEX), su editor propio, las pestañas y la barra de estado (COMCTL32)" f1 \
    negrita 100 60 200 90 "flat assembler 1.73.32" texto 100 90 140 110 "File" texto 140 90 200 110 "Edit" texto 280 90 400 110 "Options" texto 100 330 200 360 "Untitled"
  miracap "Ctrl+O (su acelerador): el cuadro de abrir de COMDLG32, con la carpeta y sus archivos" f2 negrita 150 90 260 125 "Abrir" texto 150 125 400 160 "C:\\FASM" texto 150 150 300 190 "HELLO.ASM"
  miracap "abre HELLO.ASM (con sus colores) y lo ensambla en otro hilo (Ctrl+F9): «4 passes»" f4 texto 90 320 220 370 "hello.asm" texto 150 100 600 260 "4 passes"
  miracap "y el HELLO.EXE que ha hecho FASMW corre: su MessageBox" f5 texto 250 240 400 290 "May I introduce myself?"
  if ! grep -q "excepción\|\*\*\*" $S; then ok "DDRAW y FASMW: ni una excepción ni una queja del núcleo"
  else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
fi
# 4. la red de los programas de Windows: WSOCK32 (REDPRUE) contra el servidor de aquí
mkdir -p $V/srv
python3 -I $AQUI/nucleo32/servidor_prueba.py $V/srv > $V/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 50); do [ -s $V/srv/puerto ] && break; sleep 0.1; done
PU=$(cat $V/srv/puerto 2> /dev/null)
DISCO=$V/d.img ARRANQUE=ordenes MODULOS= MEMORIA=64 QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN "cd \\PRUEBAS" ":pausa 3" \
  "REDPRUE 10.0.2.2 $PU /grande.bin" ":pausa 20" "REDPRUE 10.0.2.2 $PU /trozos.bin P" ":pausa 20" > $V/red.sal 2>&1
kill $SPID 2> /dev/null
tr -d '\r' < $V/red.sal > $V/red.txt
S=$V/red.txt
SUMA=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(len(d), sum(d))" $V/srv/grande.bin 2> /dev/null)
mira "WSOCK32 sobre el servicio de red: la IP (HispaRedConfig), el DNS, una descarga de 400 KB por TCP con su suma (la del archivo: $SUMA)" \
  "^IP 10.0.2.15 puerta 10.0.2.2 MAC 52:54:00:12:34:56$" "^Respuesta: HTTP/1.1 200 OK$" "^Cuerpo: ${SUMA% *} octetos, suma ${SUMA#* }$"
mira "y sin bloqueo: un puerto cerrado (WSAECONNREFUSED), connect y select, y otra descarga" "^Puerto cerrado: -1, error 10061$" "^select: 1 (escribir 1, fallo 0)$" "^Cuerpo: 150508 octetos"
else
  ko "nucleo32/win32/disco.sh no puede hacer el disco: $(tail -n 2 $V/disco.txt)"
fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin los programas de Windows)"
fi   # [WIN32] los programas de Windows
[ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/*.ppm "$CAPTURAS_A"/ 2> /dev/null; }

# ---------------------------------------------- 18. el Navegador (NAVEGA.BIN)
# NetSurf con QuickJS en un programa del anillo 3 (nucleo32/navega/: el
# Navegador completo de Hispa-486 con un frontal nuevo; docs/PENTIUM.md, «El
# Navegador»), contra el sitio de prueba de navega/servidor_pruebas.py (HTTP
# y HTTPS, con su raíz de prueba en \RAICES): una hoja de fuera con variables
# de CSS, un PNG y un JPEG, JavaScript (el DOM, setTimeout, localStorage,
# fetch con galletas y una cabecera), un formulario POST con eñe, la rueda del
# ratón, Fin, Atrás, el menú del botón derecho (abrir el enlace en otra
# pestaña), la dirección (F6) por HTTP, el portapapeles del sistema (Ctrl+L,
# Ctrl+C y Ctrl+V en un campo), una descarga a C:\DESCARGA con su nombre en
# 8+3 (igual octeto a octeto), las galletas que se guardan al cerrar; y el
# x87 de cada programa (X87.BIN, a la vez con su copia). Las posiciones, las de
# la ventana en una pantalla de 800 x 600 (la página empieza en (20, 108)).
if parte08 navegador; then
echo "== Hispa-Pentium: el Navegador (NetSurf con QuickJS, NAVEGA.BIN) =="
Q=$P/nav
rm -rf $Q; mkdir -p $Q $G
python3 -I $AQUI/nucleo32/navega/servidor_pruebas.py $Q/srv > $Q/srv.txt 2>&1 &
NPID=$!
for _ in $(seq 100); do [ -s $Q/srv/puertos ] && break; sleep 0.1; done
HP=$(sed -n 's/^http=//p' $Q/srv/puertos); HS=$(sed -n 's/^https=//p' $Q/srv/puertos)
export MTOOLS_SKIP_CHECK=1
if [ -n "$HS" ] && $AQUI/nucleo32/disco.sh $Q/d.img 32 red tls navegador > $Q/disco.txt 2>&1 && mmd -i $Q/d.img ::RAICES \
   && mcopy -i $Q/d.img $Q/srv/RAIZ.CER ::RAICES/PRUEBA.CER; then
  ok "nucleo32/navega/hacer.sh: NAVEGA.BIN, NetSurf 3.11 con QuickJS y la biblioteca de C de Hispa-486 en un programa del anillo 3 ($(mdir -i $Q/d.img ::NAVEGA/NAVEGA.BIN | sed -n 's/^NAVEGA *BIN *\([0-9]*\).*/\1/p') octetos)"
  printf 'Inicio=https://10.0.2.2:%s/\r\n' $HS > $Q/NAVEGA.INI
  mcopy -o -i $Q/d.img $Q/NAVEGA.INI ::NAVEGA/
  : > $Q/SURF.LOG; mcopy -i $Q/d.img $Q/SURF.LOG ::NAVEGA/
  cp $Q/d.img $Q/e.img
  CAPTURAS=$G ESCRIBE=1 DISCO=$Q/e.img ARRANQUE=escritorio MODULOS= MEMORIA=128 QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" \
    ":pausa 3" ":captura nv0" ":doble 62 260" ":pausa 15" ":captura nv1" \
    ":raton 400 400" ":monitor mouse_move 0 0 -3" ":pausa 2" ":captura nv2" ":tecla end" ":pausa 2" ":captura nv3" ":tecla home" ":pausa 2" \
    ":clic 130 267" ":escribe Peña" ":tecla ret" ":pausa 5" ":captura nv4" ":clic 41 89" ":pausa 5" ":captura nv5" \
    ":clicd 95 309" ":pausa 1" ":captura nv6" ":clic 150 326" ":pausa 5" ":captura nv7" \
    ":tecla f6" ":escribe 10.0.2.2:$HP/pagina2.html" ":tecla ret" ":pausa 4" ":captura nv8" \
    ":tecla ctrl-l" ":tecla ctrl-c" ":tecla esc" ":clic 482 57" ":pausa 6" ":clic 130 267" ":tecla ctrl-v" ":tecla ret" ":pausa 5" \
    ":tecla f6" ":escribe https://10.0.2.2:$HS/descarga" ":tecla ret" ":pausa 6" ":captura nv9" ":clic 765 26" ":pausa 4" > $Q/e.sal 2>&1
  tr -d '\r' < $Q/e.sal > $Q/e.txt
  S=$Q/e.txt
  mira "el acceso del menú (\\MENU\\NAVEGA.MNU) lo pone en el tapiz y lo lanza; arranca con la página de NAVEGA.INI" \
    "escritorio: lanzo .NAVEGA.NAVEGA.BIN (la tarea [0-9]*)" "^navega: Navegador [0-9.]*, en C:.NAVEGA$" "^navega: primera https://10.0.2.2:$HS/$"
  miracap "el icono «Navegador» en el tapiz" nv0 texto 20 276 40 292 Navegador
  mira "la página por HTTPS (TLS.BIN, con la raíz de prueba) y su hoja, sus imágenes y su fetch, por la misma conexión cifrada" \
    "tls: 10.0.2.2: TLS 1.2, ECDHE-ECDSA, .*la cadena comprobada$" "^navega: hecho https://10.0.2.2:$HS/ \[[0-9]* KB en uso"
  S=$Q/srv/pedidos.txt
  mira "lo que pide: el User-Agent propio (HispaSurf, sin «Hispa32» ni «HispaNavegador»: ClicGest da la web normal), las galletas y la cabecera de fetch" \
    "^GET / | HispaSurf/0.2 (NetSurf 3.11; Hispa-Pentium) |" "^GET /estilo.css | HispaSurf.* | sesion=abc; recuerda=1; desdejs=1 |" \
    "^GET /rojo.png |" "^GET /azul.jpg |" "^GET /api?visitas=1 | .* | sesion=abc; recuerda=1; desdejs=1 | fetch |"
  miracap "la página pintada: la hoja de fuera con variables de CSS (la cabecera verde), el JavaScript (la caja magenta), setTimeout (azul), fetch con las dos galletas (naranja), el PNG y el JPEG" nv1 \
    rgb 300 140 2e8b57 rgb 77 222 c71585 rgb 181 222 1e90ff rgb 285 222 ff8c00 rgb 382 208 d02020 rgb 472 208 2040d0 14
  miracap "la rueda del ratón mueve la página (60 puntos por paso)" nv2 nrgb 300 112 2e8b57 rgb 77 160 c71585 rgb 285 160 ff8c00
  miracap "Fin: al final de la página (el pie, morado)" nv3 rgb 300 500 8a2be2
  S=$Q/srv/pedidos.txt
  mira "el formulario: lo tecleado (con eñe) va por POST" "^POST /eco | .* | nombre=Pe%C3%B1a$"
  miracap "y la respuesta del servidor, pintada" nv4 rgb 300 150 daa520
  miracap "Atrás: la página de antes, con su JavaScript otra vez" nv5 rgb 300 140 2e8b57 rgb 77 222 c71585
  miracap "el botón derecho sobre un enlace: su menú (abrirlo en otra pestaña, copiar su dirección, descargarlo)" nv6 texto 100 310 140 340 Abrir
  S=$Q/e.txt
  mira "«Abrir el enlace en otra pestaña»: la segunda página, en una pestaña nueva" "^navega: hecho https://10.0.2.2:$HS/pagina2.html "
  mira "F6 y una dirección sin http:// (por HTTP, sin cifrar)" "^navega: ir a http://10.0.2.2:$HP/pagina2.html$" "^navega: hecho http://10.0.2.2:$HP/pagina2.html "
  miracap "la segunda página, por HTTP, en la segunda pestaña" nv8 rgb 300 150 008b8b
  S=$Q/srv/pedidos.txt
  mira "el portapapeles del sistema: Ctrl+L y Ctrl+C copian la dirección; Ctrl+V la pega en un campo de otra pestaña (+), y va con el formulario" \
    "^POST /eco | .* | nombre=http%3A%2F%2F10.0.2.2%3A$HP%2Fpagina2.html$"
  S=$Q/e.txt
  mira "una descarga (Content-Disposition: attachment): a C:\\DESCARGA, con su nombre en 8+3" \
    "^navega: descarga: Informe anual 2026.zip -> C:.DESCARGA.INFORMEA.ZIP$" "^navega: descarga: hecha C:.DESCARGA.INFORMEA.ZIP, 50000 octetos$"
  mira "y al cerrar la ventana, se acaba" "^navega: adiós$"
  if ! grep -q "excepción\|\*\*\*" $S; then ok "ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -3 | sed 's/^/        /'; fi
  sano $Q/e.img
  if saca $Q/e.img DESCARGA/INFORMEA.ZIP $Q/x.zip && cmp -s $Q/x.zip $Q/srv/descarga.bin && grep -q "^$Q/e.img: [0-9]* files" $P/fsck.txt; then
    ok "lo descargado es igual octeto a octeto que lo del servidor, y el disco queda sano"
  else ko "lo descargado no está, no es igual o el disco no queda sano"; sed 's/^/        /' $P/fsck.txt | head -4; fi
  if saca $Q/e.img NAVEGA/SURFGAL.TXT $Q/gal.txt && grep -q "recuerda" $Q/gal.txt && ! grep -q "sesion" $Q/gal.txt && saca $Q/e.img NAVEGA/NAVEGA2.ALM $Q/alm.txt; then
    ok "las galletas que dicen hasta cuándo valen se guardan al cerrar (SURFGAL.TXT; las de sesión, no), y lo de localStorage (NAVEGA2.ALM)"
  else ko "no se guardan las galletas o localStorage"; fi
  # position: fixed y sticky (navega 0.2: la barra lateral de ClicGest se
  # cortaba al desplazar): la rueda, y la cabecera fija, la barra lateral fija
  # (entera, hasta abajo) y el bloque sticky siguen en su sitio; y el enlace de
  # abajo de la barra lateral, ya desplazada, se puede pulsar
  printf 'Inicio=http://10.0.2.2:%s/fijo.html\r\n' $HP > $Q/NAVEGA.INI
  cp $Q/d.img $Q/f.img
  mcopy -o -i $Q/f.img $Q/NAVEGA.INI ::NAVEGA/
  CAPTURAS=$G DISCO=$Q/f.img ARRANQUE=escritorio MODULOS= MEMORIA=128 QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" \
    ":pausa 3" ":doble 62 260" ":pausa 12" ":captura nvf0" ":raton 400 400" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" \
    ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 2" ":captura nvf1" \
    ":clic 45 500" ":pausa 3" > $Q/f.sal 2>&1
  miracap "position: fixed y sticky, sin desplazar: la cabecera fija (roja), la barra lateral fija (azul, de height: 100vh: hasta abajo) y el sticky (verde) en su sitio" nvf0 \
    rgb 300 128 e03030 rgb 60 130 305080 rgb 60 524 305080 rgb 300 360 20a060
  miracap "y desplazada 300 puntos con la rueda: la cabecera y la barra lateral, donde estaban y enteras; el sticky, pegado bajo la cabecera" nvf1 \
    rgb 300 128 e03030 rgb 60 130 305080 rgb 60 524 305080 rgb 300 163 20a060 nrgb 300 360 20a060
  S=$Q/srv/pedidos.txt
  mira "y el enlace de abajo de la barra lateral fija, con la página desplazada, se pulsa (donde se ve)" "^GET /eco?lateral |"
  # el x87: dos programas con coma flotante a la vez
  DISCO=$Q/d.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "x87" ":pausa 6" > $Q/x.sal 2>&1
  tr -d '\r' < $Q/x.sal > $Q/x.txt
  S=$Q/x.txt
  mira "el x87 de cada programa: X87.BIN y sus dos copias, a la vez, cada una con su coma flotante (el núcleo la guarda y la repone)" \
    "^x87: bien (dos programas a la vez, cada uno con su x87)$"
else ko "no se puede poner en marcha el sitio de prueba o hacer el disco: $(tail -n 1 $Q/srv.txt) $(tail -n 3 $Q/disco.txt)"; fi
kill $NPID 2> /dev/null; wait $NPID 2> /dev/null
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el Navegador)"
fi   # el Navegador

# ------------------------------------- 23 bis. (RUEDA) la rueda en los programas
if parte08 rueda; then
echo "== Hispa-Pentium: la rueda del ratón en los programas (RUEDA) =="
W=$P/rueda
rm -rf $W; mkdir -p $W $G
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $W/semilla.asm > /dev/null && nasm -f bin $W/semilla.asm -o $W/SEMILLA.BIN || ko "semilla.cas no compila"
for u in udisposi:DISPOSIT umuestra:MUESTRA; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $W/x.asm > /dev/null && nasm -f bin $W/x.asm -o $W/${u##*:}.BIN || ko "${u%%:*}.cas no compila"
done
# (y Programas y actualizaciones, que solo se compila: su rueda se ve con un catálogo)
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/uactual.cas -o $W/x.asm > /dev/null && nasm -f bin $W/x.asm -o $W/ACTUALIZ.BIN \
  && ok "ACTUALIZ.BIN (Programas y actualizaciones, con la rueda en su lista) compila" || ko "uactual.cas no compila"
if $AQUI/nucleo32/disco.sh $W/d.img 32 > $W/disco.txt 2>&1; then
  python3 -c "
import sys
for i in range(1, 401): sys.stdout.write('linea %03d de un texto largo\r\n' % i)" > $W/LARGO.TXT
  mmd -i $W/d.img ::MUCHOS
  for i in $(seq -w 1 80); do printf 'archivo %s\r\n' $i > $W/A$i.TXT; done
  mcopy -i $W/d.img $W/A*.TXT ::MUCHOS/
  mcopy -i $W/d.img $W/LARGO.TXT $W/DISPOSIT.BIN $W/MUESTRA.BIN ::/
  # la Consola (dos clics en su icono): 400 líneas, y la rueda hacia arriba
  # (cada «mouse_move 0 0 N» de QEMU es un paso: tres líneas); luego el
  # Bloc, Mi equipo, el Administrador de dispositivos y MUESTRA, lanzados desde ella
  CAPTURAS=$G DISCO=$W/d.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":doble 62 185" ":pausa 3" "type largo.txt" ":pausa 3" ":raton 300 300" \
    ":monitor mouse_move 0 0 1" ":pausa 1" ":monitor mouse_move 0 0 1" ":pausa 1" ":captura ru1" \
    "bloc largo.txt" ":pausa 3" ":captura ru2" ":raton 300 300" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru3" \
    ":tecla right" ":pausa 1" ":captura ru4" ":clic 606 102" ":pausa 1" \
    "equipo c:\\muchos" ":pausa 4" ":raton 400 300" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru5" \
    ":monitor sendkey ctrl 1500" ":monitor mouse_move 0 0 -1" ":pausa 2" ":captura ru6" \
    ":tecla f8" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru7" ":clic 775 21" ":pausa 1" \
    "disposit" ":pausa 4" ":raton 400 300" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru8" ":clic 745 31" ":pausa 1" \
    "muestra normal" ":pausa 4" ":raton 400 350" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru9" \
    ":monitor sendkey ctrl 1500" ":monitor mouse_move 0 0 1" ":pausa 2" ":captura ru10" ":tecla down" ":pausa 1" ":tecla down" ":pausa 1" ":captura ru11" > $W/r.sal 2>&1
  tr -d '\r' < $W/r.sal > $W/r.txt
  S=$W/r.txt
  mira "la Consola, el Bloc, Mi equipo y el Administrador de dispositivos, lanzados" \
    "escritorio: lanzo .CONSOLA.BIN" 
  miracap "la Consola: la rueda hacia arriba enseña lo que había subido (dos pasos, seis líneas: la 371 arriba, en otro azul) y el título lo dice" ru1 \
    negrita 90 60 600 90 "Consola - historial: 6 l" texto 90 110 400 130 "linea 371 de un texto largo"
  miracap "el Bloc de notas con LARGO.TXT (y la Consola, al escribir, otra vez abajo)" ru2 texto 110 170 400 200 "linea 001 de un texto largo" notexto 80 60 600 90 "historial"
  miracap "el Bloc: dos pasos de rueda hacia abajo, seis líneas (la 007 arriba), y el cursor donde estaba (Lín 1)" ru3 \
    texto 110 170 400 200 "linea 007 de un texto largo" texto 110 455 400 480 "Lín 1, Col 1"
  miracap "y con una tecla (la flecha), otra vez al cursor" ru4 texto 110 170 400 200 "linea 001 de un texto largo" texto 110 455 400 480 "Lín 1, Col 2"
  miracap "Mi equipo: un paso de rueda, una fila de iconos (A06 arriba), sin elegir nada" ru5 texto 210 160 300 200 A06.TXT notexto 210 160 300 200 A01.TXT texto 10 478 300 500 "Nada elegido"
  miracap "Ctrl+rueda hacia abajo: la vista, más pequeña (de los iconos a la lista)" ru6 texto 210 130 320 160 A01.TXT texto 440 130 560 160 A16.TXT
  miracap "en los detalles (F8), la rueda baja seis renglones de la rejilla" ru7 negrita 220 150 320 180 A07.TXT notexto 210 130 320 470 A01.TXT
  miracap "el Administrador de dispositivos: la rueda baja la lista y la elegida («Pantalla», arriba) se queda elegida, aunque ya no se vea" ru8 \
    notexto 40 120 400 520 "[-] Pantalla (1)" negrita 40 120 400 520 "[-] Procesador (1)" texto 40 520 300 545 "16 dispositivos"
  miracap "MUESTRA: la rueda baja la rejilla seis renglones (el 20, arriba) sin mover la elegida (la 8, que ya no se ve)" ru9 \
    negrita 0 0 400 30 "Muestra de controles" texto 60 195 300 220 "Persona de ejemplo 20" notexto 0 190 800 520 "Persona de ejemplo 8"
  miracap "MUESTRA: Ctrl+rueda hacia arriba, la letra más grande (vez y media)" ru10 negrita15 0 150 800 300 "Nombre"
  miracap "y con la flecha abajo (la primera lleva el teclado a la rejilla), la elegida pasa a la siguiente (la 15) y se ve otra vez" ru11 negrita15 0 150 800 400 "Persona de ejemplo 15"
  [ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/ru*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
else ko "no se puede hacer el disco: $(tail -2 $W/disco.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin la rueda)"
fi   # la rueda

# ---------------------------------------------- 23. (MENÚ) el menú Inicio y los atajos
if parte08 menu; then
echo "== Hispa-Pentium: el menú Inicio, los menús largos y los atajos de teclado de todo el sistema (MENÚ) =="
W=$P/menu
rm -rf $W; mkdir -p $W/mnu $G
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $W/semilla.asm > /dev/null && nasm -f bin $W/semilla.asm -o $W/SEMILLA.BIN || ko "semilla.cas no compila"
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/ularga.cas -o $W/x.asm > /dev/null && nasm -f bin $W/x.asm -o $W/LARGA.BIN || ko "ularga.cas no compila"
if $AQUI/nucleo32/disco.sh $W/d.img 32 oficina > $W/disco.txt 2>&1; then
  # 24 juegos (más de los que caben a lo alto: el submenú se desplaza), un
  # acceso de «Herramientas» (a Accesorios) y uno sin tipo (el Navegador:
  # a Internet, por su programa); 8 documentos más en la raíz
  for i in $(seq -w 1 24); do printf 'nombre=Juego %s\r\nprograma=\\HOLA.BIN\r\ntipo=Juegos y emuladores\r\n' $i > $W/mnu/J$i.MNU; done
  printf 'nombre=Saludo\r\nprograma=\\HOLA.BIN\r\ntipo=Herramientas\r\n' > $W/mnu/SALUDO.MNU
  printf 'nombre=Navegador\r\nprograma=\\HISPA\\NAVEGA\\NAVEGA.BIN\r\n' > $W/mnu/NAVEGA.MNU
  for i in 1 2 3 4 5 6 7 8; do printf 'documento %s\r\n' $i > $W/DOC$i.TXT; done
  mmd -i $W/d.img ::MENU
  mcopy -i $W/d.img $W/mnu/*.MNU ::MENU/
  mcopy -i $W/d.img $W/DOC*.TXT $W/LARGA.BIN ::/
  # 1. el menú: su forma, Todos los programas, una carpeta larga (con la
  # flecha en diagonal: no se cierra), la rueda, el teclado y Mis documentos
  CAPTURAS=$G DISCO=$W/d.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":clic 40 585" ":pausa 1" ":captura mn1" ":raton 100 502" ":pausa 1" ":captura mn2" ":raton 300 454" ":pausa 1" ":captura mn3" \
    ":raton 560 300" ":monitor mouse_move 0 0 -4" ":pausa 0.5" ":monitor mouse_move 0 0 -4" ":pausa 1" ":captura mn4" ":tecla esc" ":tecla esc" ":tecla esc" ":pausa 1" ":captura mn5" \
    ":tecla meta_l" ":pausa 1" ":tecla t" ":tecla right" ":tecla a" ":tecla right" ":pausa 1" ":captura mn6" ":tecla s" ":tecla ret" ":pausa 2" \
    ":clic 40 585" ":clic 300 171" ":pausa 1" ":captura mn7" ":clic 500 171" ":pausa 2" ":captura mn8" > $W/m.sal 2>&1
  tr -d '\r' < $W/m.sal > $W/m.txt
  S=$W/m.txt
  miracap "el menú Inicio: los fijados (Bloc de notas y Consola, con su explicación), los más usados (los de los paquetes), «Todos los programas», la columna de la derecha y el pie con Cerrar sesión, Reiniciar y Apagar equipo" mn1 \
    negrita 46 155 54 163 "Bloc de notas" texto 46 175 54 183 "Escribir un texto" texto 38 246 46 254 "Juego 01" negrita 58 492 66 500 "Todos los programas" \
    negrita 256 159 264 167 "Mis documentos" negrita 256 192 264 200 "Mi equipo" negrita 256 226 264 234 "Configuración" negrita 256 256 264 264 "Administrador de" \
    negrita 256 296 264 304 "Programas instalados" negrita 256 330 264 338 "Actualizar" negrita 256 363 264 371 "Ayuda" negrita 256 393 264 401 "Ejecutar..." \
    negrita 68 533 76 541 "Cerrar sesión" negrita 210 533 222 541 Reiniciar negrita 320 533 328 541 "Apagar equipo"
  miracap "«Todos los programas» (con la flecha encima) abre sus carpetas: Accesorios, Internet (el Navegador, por su programa), Juegos (por su «tipo»), Oficina y Sistema" mn2 \
    texto 250 390 258 398 Accesorios texto 250 416 258 424 Internet texto 250 442 258 450 Juegos texto 250 468 258 476 Oficina texto 250 494 258 502 Sistema
  miracap "una carpeta con más de los que caben a lo alto: el submenú no crece más que la pantalla y lleva sus flechas (el 01 arriba, el 24 no se ve)" mn3 \
    texto 486 30 494 40 "Juego 01" notexto 460 0 700 570 "Juego 24" negrita 320 533 328 541 "Apagar equipo"
  miracap "la flecha, en diagonal hasta él, no lo cierra; y la rueda lo baja: el 24, a la vista" mn4 texto 460 0 700 570 "Juego 24" notexto 460 0 700 570 "Juego 01"
  miracap "Esc cierra un submenú cada vez, y luego el menú" mn5 notexto 0 90 440 570 "Todos los programas"
  miracap "con el teclado: el logo, la «t» (Todos los programas), la flecha a la derecha, la «a» (Accesorios) y otra vez a la derecha: su submenú, la primera elegida" mn6 \
    texto 486 390 494 398 "Bloc de notas" texto 486 468 494 476 "Saludo"
  mira "y la «s» e Intro lanzan «Saludo» (un acceso de un paquete)" "escritorio: lanzo .HOLA.BIN"
  miracap "«Mis documentos» abre su submenú con los .TXT de la raíz" mn7 texto 460 160 480 182 "LEEME.TXT" texto 460 0 700 570 "DOC8.TXT"
  mira "y uno de ellos se abre con el Bloc de notas" "escritorio: lanzo .BLOC.BIN"
  # 2. a 640 x 480 y a 1024 x 600: el pie, siempre a la vista
  for m in 640x480 1024x600; do
    al=${m#*x}
    CAPTURAS=$G DISCO=$W/d.img ARRANQUE="escritorio modo=$m" PANTALLA=$m MODULOS= $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN ":pausa 3" ":clic 40 $((al - 15))" ":pausa 1" ":captura mp$m" > $W/p$m.sal 2>&1
    miracap "a $m: el menú cabe encima de la barra, con «Todos los programas» y «Apagar equipo» a la vista" mp$m \
      negrita 58 $((al - 108)) 66 $((al - 100)) "Todos los programas" negrita 320 $((al - 67)) 328 $((al - 59)) "Apagar equipo" negrita 46 0 54 $((al - 120)) "Bloc de notas"
  done
  # 3. los atajos de teclado de todo el sistema
  cp $W/d.img $W/e.img
  CAPTURAS=$G ESCRIBE=1 DISCO=$W/e.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":doble 60 110" ":pausa 2" ":doble 60 190" ":pausa 2" ":tecla alt-tab" ":pausa 1" ":captura at1" \
    ":tecla alt-spc" ":pausa 1" ":captura at2" ":tecla down" ":tecla down" ":tecla ret" ":tecla right" ":tecla right" ":tecla right" ":tecla down" ":tecla ret" ":pausa 1" ":captura at3" \
    ":tecla ctrl-alt-delete" ":pausa 1" ":captura at4" ":tecla esc" ":tecla meta_l-r" ":pausa 1" ":escribe larga" ":captura at5" ":tecla ret" ":pausa 2" \
    ":clic 300 200" ":pausa 1" ":captura at6" ":raton 380 300" ":monitor mouse_move 0 0 -5" ":pausa 1" ":captura at7" ":tecla end" ":tecla ret" ":pausa 1" \
    ":tecla print" ":pausa 5" ":captura at8" ":tecla meta_l-d" ":pausa 1" ":captura at9" ":tecla meta_l-d" ":pausa 1" ":captura at10" \
    ":tecla alt-f4" ":pausa 2" ":clic 792 300" ":tecla alt-f4" ":pausa 1" ":captura at11" ":tecla right" ":tecla right" ":pausa 1" ":captura at12" ":tecla ret" ":pausa 3" ":captura at13" \
    ":tecla ret" ":pausa 1" ":clic 792 300" ":tecla f1" ":pausa 1" ":tecla ret" ":tecla meta_l-e" ":pausa 3" ":captura at14" > $W/a.sal 2>&1
  tr -d '\r' < $W/a.sal > $W/a.txt
  S=$W/a.txt
  mira "Alt+Tab: la franja con las dos ventanas, y al soltar Alt se queda la de detrás (el Bloc de notas)" "^escritorio: Alt+Tab, con 2 ventanas$" "^escritorio: Alt+Tab, a la 1$"
  miracap "Alt+Tab trae delante el Bloc de notas (estaba detrás de la Consola)" at1 negrita 100 60 130 80 "NOTAS.TXT - Bloc de notas"
  miracap "Alt+Espacio: el menú de la ventana (Restaurar, Mover, Tamaño, Minimizar, Maximizar y Cerrar)" at2 texto 90 80 140 110 Restaurar texto 90 120 140 160 "Tamaño" texto 90 210 140 240 Cerrar
  miracap "«Tamaño» con las flechas: la ventana, más ancha y 16 puntos más alta (la barra de estado del Bloc, más abajo)" at3 negrita 100 60 130 80 "NOTAS.TXT - Bloc de notas" texto 94 446 102 470 NOTAS.TXT
  miracap "Ctrl+Alt+Supr: el Administrador de tareas, con los programas abiertos" at4 negrita 160 60 300 200 "Los programas abiertos" texto 180 180 300 220 "NOTAS.TXT - Bloc de notas" texto 180 200 300 240 Consola
  miracap "Logo+R: «Ejecutar», con lo que se escribe" at5 negrita 140 200 260 300 "Abrir:" texto 200 250 300 300 larga
  mira "y lanza LARGA.BIN (sin .BIN: se pone solo)" "escritorio: lanzo .LARGA.BIN"
  miracap "un menú emergente de 30 opciones (más de las que caben): se ve lo que cabe, con sus flechas" at6 texto 250 0 450 60 "Opción número 01" notexto 250 0 450 600 "Opción número 30"
  miracap "y la rueda lo desplaza" at7 notexto 250 0 450 60 "Opción número 01" texto 250 0 450 80 "Opción número 0"
  mira "Fin e Intro eligen la última (la 30)" "^larga: elegida 30$"
  mira "Impr Pant: una captura de toda la pantalla en \\CAPTURAS (un BMP)" "^escritorio: captura de 800 puntos de ancho en .CAPTURAS.CAPT0001.BMP$"
  miracap "y lo dice en un globo encima de la bandeja" at8 negrita 450 500 800 570 "Captura de la pantalla"
  rm -f $W/capt.bmp; mcopy -n -i $W/e.img ::CAPTURAS/CAPT0001.BMP $W/capt.bmp > /dev/null 2>&1
  if [ "$(head -c 2 $W/capt.bmp 2> /dev/null)" = BM ] && [ "$(stat -c %s $W/capt.bmp 2> /dev/null)" = 1440054 ] && fsck.fat -n $W/e.img > /dev/null 2>&1; then
    ok "el BMP está en el disco: 800 x 600 con 24 bits (1.440.054 octetos), y el disco, sano (fsck.fat)"
  else ko "la captura no está bien en el disco ($(stat -c %s $W/capt.bmp 2> /dev/null) octetos)"; fi
  miracap "Logo+D enseña el escritorio (lo minimiza todo)" at9 notexto 80 60 700 100 "NOTAS.TXT - Bloc de notas" notexto 80 60 700 100 "Menú largo"
  miracap "y otra vez Logo+D lo devuelve" at10 negrita 80 60 700 120 "NOTAS.TXT - Bloc de notas"
  miracap "Alt+F4 con el tapiz elegido: «Apagar el equipo», con Apagar elegido (Apagar, Reiniciar, Cerrar sesión y Cancelar)" at11 \
    negrita 192 312 208 326 Apagar negrita 300 312 316 326 Reiniciar negrita 404 312 420 326 "Cerrar sesión" negrita 544 312 560 326 Cancelar rgb 166 306 14294A 30
  miracap "las flechas llevan lo elegido a «Cerrar sesión» (el borde oscuro, alrededor)" at12 negrita 404 312 420 326 "Cerrar sesión" rgb 406 306 14294A 30 nrgb 166 306 14294A 30
  mira "Intro cierra la sesión (se cierran los programas) y vuelve con «Entrar»" "^escritorio: sesión cerrada$" "^escritorio: sesión abierta$"
  miracap "el cuadro de la sesión cerrada" at13 negrita 200 200 600 300 "La sesión se ha cerrado." negrita 200 200 600 400 "Entrar"
  mira "F1 en el tapiz: la Ayuda (en este disco no hay MANUAL.TXT: lo dice)" "^escritorio: aviso: La Ayuda (MANUAL.TXT) no está en este sistema.$"
  mira "Logo+E: el explorador (Mi equipo)" "escritorio: lanzo .EQUIPO.BIN"
  if ! grep -q "excepción\|\*\*\*" $W/m.txt $W/a.txt; then ok "en esas sesiones, ni una excepción ni una queja del núcleo"
  else ko "el núcleo se ha quejado:"; grep -h "excepción\|\*\*\*" $W/m.txt $W/a.txt | head -4 | sed 's/^/        /'; fi
  [ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/mn*.ppm $G/mp*.ppm $G/at*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
else ko "no se puede hacer el disco: $(tail -2 $W/disco.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el menú Inicio)"
fi   # el menú Inicio

# ------------------------------------------------- 15. CASTEL32 (pruebas32.sh)
[ -z "${SOLO:-}" ] && parte15

echo "== $nok bien, $fail mal =="
exit $((fail > 0))
