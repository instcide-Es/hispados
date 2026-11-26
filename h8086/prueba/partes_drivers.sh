# Las partes de la prueba de los controladores de DOS (el primer hito de
# docs/DRIVERS-MICROSOFT.md): DEVICE= con controladores de carácter y de
# bloque (también .EXE), el cambio de disco (MEDIA CHECK y BUILD BPB), IOCTL
# (INT 21h/44h), la lista de listas (52h), la consola por un ANSI.SYS y los
# «packet drivers» de red. Las carga prueba.sh, que tiene las funciones.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
# Los controladores de otros (libres, GPL) NO van en el repositorio: las
# partes que los usan los buscan en $DRV_LIBRES (por omisión,
# ~/.cache/hispa-drivers/descargas, con lo que deja prueba/trae_drivers.sh)
# y, si no están, lo dicen y se saltan:
#   pktd11/NE2000.COM, PKTSTAT.COM, TERMIN.COM  los «packet drivers» de Crynwr
#   nansi/BIN/NANSI.SYS                          NANSI, un ANSI.SYS de FreeDOS
#   ctmouse/BIN/CTMOUSE.EXE                      CuteMouse, de FreeDOS
DRV_LIBRES=${DRV_LIBRES:-$HOME/.cache/hispa-drivers/descargas}
hay_libres() {   # los archivos ($@, desde $DRV_LIBRES): ¿están todos?
  local f
  for f in "$@"; do
    [ -f "$DRV_LIBRES/$f" ] || { echo "  (se salta: falta $DRV_LIBRES/$f; prueba/trae_drivers.sh lo trae)" | tee -a $T/salida.txt; return 1; }
  done
}

# ---- los nuestros: DISQUITO (bloque, de quitar y poner), SALUDOX (.EXE) ----
p_drivers() {
echo "== Hispa-8086: controladores de DOS (bloque de quitar y poner, .EXE, IOCTL, 52h) =="
cp $IMG/hispa8086-1440.img $T/a.img
printf 'DEVICE=\\DISQUITO.SYS\r\nDEVICE=\\SALUDO.SYS\r\nDEVICE=\\SALUDOX.SYS\r\nDEVICE=\\DISCORAM.SYS 64\r\n' > $T/CONFIG.SYS
mcopy -i $T/a.img $T/CONFIG.SYS build/DISQUITO.SYS build/SALUDO.SYS build/SALUDOX.SYS build/IOCTL.COM ::
arranca $T/a.img 486
pantalla k1
esta k1 "DISQUITO: dos discos de 20 K en la unidad D:" "DEVICE= de bloque, con la unidad que le toca (pet. 22): "
esta k1 "DISCORAM: disco en memoria de 64 K en la unidad E:" "y otro detrás, en la siguiente: "
escribe "limpiar"; escribe "dir d:"; escribe "mostrar d:uno.txt"; escribe "mostrar exedrv"; escribe "ioctl d"
pantalla k2
esta k2 "UNO.TXT               11" "la unidad del controlador, con su disco: "
esta k2 "disco uno" ""
esta k2 "EXEDRV: controlador .EXE, reubicado bien" "un controlador .EXE, con su reubicación aplicada: "
esta k2 "IOCTL: memoria M; cadena: NUL [1] EXEDRV SALUDO [1] CON AUX PRN COM1 LPT1" "52h: la primera cabecera de memoria y la cadena, también los de bloque: "
esta k2 "IOCTL: 4404 DISQUITO: disco 1, preguntas" "4404h, lectura de control del controlador: "
esta k2 "medidas 000" "un disco que no cambia no pide medidas: "
esta k2 "IOCTL: 4408 0000; 4409 4840; 440D 1; consola 80D3" "4408h (se puede quitar), 4409h (atributos), 440Dh (genérica) y 4400h de la consola: "
esta k2 "IOCTL: SALUDO 80C0; 4406 00FF; 4402 0001" "4400h, 4406h y 4402h (sin IOCTL: error 1) de un dispositivo de carácter: "
escribe "limpiar"; escribe "ioctl d 2"; escribe "dir d:"; escribe "mostrar d:dos.txt"; escribe "ioctl d"
pantalla k3
esta k3 "IOCTL: disco cambiado" "4405h, escritura de control (cambia el disco): "
esta k3 "DOS.TXT               11" "MEDIA CHECK dice «otro» y BUILD BPB da el nuevo: DIR lo ve: "
noesta k3 "UNO.TXT" "y nada del de antes: "
esta k3 "disco dos" ""
esta k3 "disco 2, preguntas" ""
esta k3 "medidas 001" "una sola vez BUILD BPB: "
escribe "limpiar"; escribe "eco escrito en el dos > d:nuevo.txt"; escribe "ioctl d 1"; escribe "dir d:"; escribe "ioctl d 2"; escribe "mostrar d:nuevo.txt"
pantalla k4
esta k4 "UNO.TXT" "y al volver a poner el primero, el primero: "
noesta k4 "NUEVO.TXT" "(sin lo escrito en el otro) "
esta k4 "escrito en el dos" "y lo escrito en el segundo sigue en él: "
apaga
}

# ---- la consola por un ANSI.SYS: NANSI (GPL, de FreeDOS) ----
p_drivers_ansi() {
echo "== Hispa-8086: la consola por un ANSI.SYS (NANSI.SYS, de FreeDOS) =="
hay_libres nansi/BIN/NANSI.SYS || return 0
cp $IMG/hispa8086-1440.img $T/a.img
printf 'DEVICE=\\NANSI.SYS\r\n' > $T/CONFIG.SYS
printf '\033[2J\033[1;33;44mAMARILLO SOBRE AZUL\033[0m normal\r\n' > $T/ANSI.TXT
mcopy -i $T/a.img $T/CONFIG.SYS $T/ANSI.TXT $DRV_LIBRES/nansi/BIN/NANSI.SYS ::
arranca $T/a.img 486
pantalla n1
esta n1 "NANSI.sys 4.0d" "DEVICE=NANSI.SYS se carga: "
noesta n1 "Possible options" "y entiende su línea (acabada en retorno y salto, como en MS-DOS): "
escribe "mostrar ansi.txt"; sleep 1
rm -f $T/scr.bin; mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.4
pantalla n2
esta n2 "AMARILLO SOBRE AZUL normal" "las órdenes ANSI no salen en la pantalla: "
noesta n2 "[1;33;44m" ""
if python3 - $T/scr.bin <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
t = bytes(d[0::2]).decode('cp437'); i = t.index('AMARILLO')
sys.exit(0 if d[2 * i + 1] == 0x1E and d[2 * t.index('normal') + 1] == 0x07 else 1)
PY
then ok "los colores, puestos por NANSI (amarillo sobre azul, y luego los de siempre)"; else ko "los colores de NANSI no se ven"; fi
escribe "limpiar"; escribe "eco hola por ansi"; escribe "dir nansi.sys"
pantalla n3
esta n3 "hola por ansi" "la consola de los programas, por NANSI: "
esta n3 "NANSI.SYS" ""
apaga
}

# ---- la red: un «packet driver» de Crynwr (GPL) para la NE2000 de QEMU ----
p_drivers_red() {
echo "== Hispa-8086: red con un packet driver (NE2000.COM de Crynwr, en la NE2000 de QEMU) =="
hay_libres pktd11/NE2000.COM pktd11/PKTSTAT.COM pktd11/TERMIN.COM || return 0
cp $IMG/hispa8086-1440.img $T/a.img
mcopy -i $T/a.img $DRV_LIBRES/pktd11/NE2000.COM $DRV_LIBRES/pktd11/PKTSTAT.COM $DRV_LIBRES/pktd11/TERMIN.COM build/PAQUETE.COM ::
QMAS="-netdev user,id=n0 -device ne2k_isa,iobase=0x300,irq=5,netdev=n0" arranca $T/a.img 486
escribe "limpiar"; escribe "equipo"
pantalla r0
antes=$(grep -o "([0-9]* K libres" $T/r0.txt | tr -dc 0-9)
escribe "limpiar"; escribe "ne2000 0x60 5 0x300"; sleep 3
pantalla r1
esta r1 "Packet driver software interrupt is 0x60 (96)" "NE2000.COM se instala (residente, INT 21h/31h): "
esta r1 "My Ethernet address is 52:54:00:12:34:56" "y lee la tarjeta: "
escribe "limpiar"; escribe "paquete"; sleep 4; escribe "pktstat 0x60"; sleep 2
pantalla r2
esta r2 'Paquetes: INT 60h, clase 1, version 11, "NE2000"' "PAQUETE lo encuentra (función 1): "
esta r2 "Paquetes: tarjeta 52:54:00:12:34:56" "función 6, la dirección: "
esta r2 "respuesta ARP de 10.0.2.2: 52:55:0A:00:02:02" "manda una pregunta ARP y recibe la respuesta de QEMU: "
if grep -qE "^60[[:space:]]+[1-9][0-9]*[[:space:]]+[1-9]" $T/r2.txt; then ok "PKTSTAT (de Crynwr) cuenta paquetes recibidos y enviados"; else ko "PKTSTAT no cuenta los paquetes"; fi
escribe "limpiar"; escribe "termin 0x60"; sleep 2; escribe "paquete"; sleep 1; escribe "equipo"
pantalla r3
esta r3 "termin: terminate completed" "TERMIN lo quita de la memoria: "
esta r3 "Paquetes: no hay controlador" "y ya no está: "
despues=$(grep -o "([0-9]* K libres" $T/r3.txt | tr -dc 0-9)
if [ -n "$antes" ] && [ "$antes" = "$despues" ]; then ok "y la memoria vuelve a ser la de antes ($antes K)"; else ko "memoria: $antes K antes, $despues K después"; fi
apaga
}

# ---- un residente de los de FreeDOS: CuteMouse (GPL), el controlador del ratón ----
p_drivers_raton() {
echo "== Hispa-8086: un residente .EXE (CTMOUSE, el ratón de FreeDOS) =="
hay_libres ctmouse/BIN/CTMOUSE.EXE || return 0
cp $IMG/hispa8086-1440.img $T/a.img
mcopy -i $T/a.img $DRV_LIBRES/ctmouse/BIN/CTMOUSE.EXE ::
arranca $T/a.img 486
escribe "limpiar"; escribe "ctmouse"; sleep 3
pantalla m1
esta m1 "CuteMouse v2.1" "CTMOUSE arranca: "
esta m1 "Installed at PS/2 port" "y se queda residente, con el ratón PS/2: "
apaga
}

# ---- un disco en memoria extendida de FreeDOS: SRDISK (GPL), comprimido con
#      UPX, de tamaño cambiante (el disco «cambia»: MEDIA CHECK y BUILD BPB) ----
p_drivers_srdisk() {
echo "== Hispa-8086: SRDXMS.SYS y SRDISK.EXE (un disco en memoria XMS de FreeDOS, comprimido con UPX) =="
hay_libres srdisk/BIN/SRDXMS.SYS srdisk/BIN/SRDISK.EXE || return 0
cp $IMG/hispa8086-1440.img $T/a.img
printf 'DEVICE=\\SRDXMS.SYS\r\n' > $T/CONFIG.SYS
mcopy -i $T/a.img $T/CONFIG.SYS $DRV_LIBRES/srdisk/BIN/SRDXMS.SYS $DRV_LIBRES/srdisk/BIN/SRDISK.EXE ::
QMEM=8 arranca $T/a.img 486
pantalla s1
esta s1 "Installed RAMDrive D:" "DEVICE=SRDXMS.SYS (con DS en su cabecera, como lo espera UPX): "
escribe "limpiar"; escribe "srdisk 200"; sleep 3
pantalla s2
esta s2 "XMS: Allocated 200 K" "SRDISK.EXE le da 200 K de la memoria XMS del núcleo: "
esta s2 "Disk formatted" ""
escribe "limpiar"; escribe "copiar leeme.txt d:"; sleep 2; escribe "dir d:"
pantalla s3
esta s3 "LEEME.TXT            751" "se copia en él: "
esta s3 "201728 octetos libres" ""
escribe "limpiar"; escribe "srdisk 300"; sleep 3; escribe "limpiar"; escribe "dir d:"
pantalla s4
esta s4 "LEEME.TXT            751" "SRDISK le cambia el tamaño (otro disco: MEDIA CHECK y BUILD BPB) y el archivo sigue: "
esta s4 "303616 octetos libres" "con las medidas nuevas: "
apaga
}
