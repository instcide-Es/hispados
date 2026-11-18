#!/bin/bash
# Batería de pruebas de Hispa-DOS en un PC con muchos núcleos, en RAM y en
# paralelo (pensado para el PC de Albinyana por SSH, con WSL abierto).
#
#   tools/pruebas_pc.sh [-j N] [destino_ssh] [puerto] [llave]
#
# Copia el proyecto a /dev/shm/$NOMBRE del PC, instala lo necesario la
# primera vez, compila y lanza las pruebas de `make test` de N en N (cada una
# con su carpeta temporal en RAM). Al final, resumen OK/FAIL por prueba y el
# registro completo en /dev/shm/$NOMBRE/build/pc/ (copiado aquí a build/pc/).
# Parallel test runner for a many-core PC (in RAM).
set -u
# SRC: carpeta del proyecto a probar (por omisión, la de este guion); NOMBRE:
# carpeta en la RAM del PC (para que varias copias puedan probarse a la vez).
# SOLO: expresión (grep -E) para lanzar solo algunas pruebas de `make test`,
# p. ej. SOLO='audicion|sound' (por omisión, todas).
# TMAX_PRUEBA: segundos que puede durar cada prueba como mucho (por omisión
# una hora: la más larga tarda media); si se pasa, se corta (con sus máquinas)
# y cuenta como fallo, para que una prueba colgada no pare la batería horas.
# Límite acordado con Carlos: como mucho el 80 % del PC (16 de 20 hilos).
cd "${SRC:-$(dirname "$0")/..}"
NOMBRE=${NOMBRE:-hispados}
J=10
if [ "${1:-}" = "-j" ]; then J=$2; shift 2; fi
DEST=${1:-carlos@100.126.100.79}
PORT=${2:-2222}
KEY=${3:-/root/.ssh/id_ed25519_pc_albinyana}
SSH="ssh -p $PORT -i $KEY -o BatchMode=yes -o ConnectTimeout=15 $DEST"
if ! $SSH true 2>/dev/null; then
  echo "No llego al PC ($DEST:$PORT). ¿Está Ubuntu abierto en el PC?"
  exit 2
fi
# 1) herramientas (solo la primera vez)
$SSH 'command -v make >/dev/null && command -v nasm >/dev/null && command -v mcopy >/dev/null && command -v qemu-system-i386 >/dev/null && command -v socat >/dev/null && command -v genisoimage >/dev/null && command -v fsck.fat >/dev/null && command -v zip >/dev/null && echo "int f(void){return 0;}" | gcc -m32 -ffreestanding -c -x c - -o /dev/null 2>/dev/null' \
  || $SSH 'sudo DEBIAN_FRONTEND=noninteractive apt-get update -qq && sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq make nasm mtools qemu-system-x86 qemu-utils socat genisoimage dosfstools python3 zip unzip xxd gcc-multilib >/dev/null' \
  || { echo "No se han podido instalar las herramientas en el PC"; exit 3; }
# 2) copiar el proyecto a la RAM del PC (sin build/, pero con los programas
#    de terceros descargados para las pruebas de compatibilidad)
tar --exclude=./build --exclude=./.git -cf - . | $SSH "rm -rf /dev/shm/$NOMBRE && mkdir -p /dev/shm/$NOMBRE && tar -xf - -C /dev/shm/$NOMBRE"
# 3) compilar y lanzar las pruebas en paralelo
# QEMU_MAS: opciones que se añaden a TODAS las máquinas de las pruebas (p. ej.
# '-cpu 486' o '-m 16'), con un envoltorio de qemu-system-i386 por delante
# en el PATH: así la misma batería se pasa en otras máquinas.
# Primero se compila; si falla, se enseña el error y NO se lanzan las pruebas
# (antes seguía adelante y pasaba la batería sin imagen: miles de fallos falsos).
$SSH "cd /dev/shm/$NOMBRE && export TMPDIR=/dev/shm/$NOMBRE/tmp TZ=UTC QEMU_MAS='${QEMU_MAS:-}' && mkdir -p \$TMPDIR build/pc qbin && \
  printf '#!/bin/sh\\nexec /usr/bin/qemu-system-i386 \"\$@\" %s\\n' '${QEMU_MAS:-}' > qbin/qemu-system-i386 && chmod +x qbin/qemu-system-i386 && \
  export PATH=/dev/shm/$NOMBRE/qbin:\$PATH && \
  { { make -s > build/pc/make.log 2>&1 && make -s para-pruebas >> build/pc/make.log 2>&1; } \
    || { tail -25 build/pc/make.log; exit 1; }; } && tail -3 build/pc/make.log" \
  || { echo "pruebas_pc: la COMPILACIÓN ha fallado en el PC (el error, arriba): no se lanzan las pruebas."; exit 4; }
$SSH "cd /dev/shm/$NOMBRE && export TMPDIR=/dev/shm/$NOMBRE/tmp TZ=UTC QEMU_MAS='${QEMU_MAS:-}' PATH=/dev/shm/$NOMBRE/qbin:\$PATH && \
  sed -n '/^test:/,/^\$/p' Makefile | grep 'tests/' | sed 's/^\t//; s/\$(IMG)/build\/hispados.img/g' | grep -E -- '${SOLO:-.}' > build/pc/lista.txt && \
  echo \"\$(wc -l < build/pc/lista.txt) pruebas, $J a la vez\" && \
  date +%s > build/pc/inicio && \
  cat -n build/pc/lista.txt | xargs -P $J -L 1 bash -c 'n=\$0; l=build/pc/\$(printf %02d \$n); timeout -k 20 ${TMAX_PRUEBA:-3600} bash -c \"\$*\" > \$l.log 2>&1; r=\$?; [ \$r -ge 124 ] && echo \"  FAIL la prueba no ha acabado en ${TMAX_PRUEBA:-3600} s: cortada\" >> \$l.log; echo \"\$r \$*\" > \$l.rc' ; \
  echo \"tiempo: \$(( \$(date +%s) - \$(cat build/pc/inicio) )) s\""
# 4) traer los registros y resumir
rm -rf build/pc && mkdir -p build/pc
$SSH "cd /dev/shm/$NOMBRE/build/pc && tar -cf - ." | tar -xf - -C build/pc
# 5) las que necesitan KVM: el WSL del PC (Windows 10) no lo tiene (sin
# virtualización anidada); si aquí sí hay /dev/kvm, se repiten aquí, con nice,
# y su resultado sustituye al del PC (18/11/2026).
if [ -c /dev/kvm ] && ! $SSH 'timeout 8 qemu-system-i386 -enable-kvm -display none -nodefaults -m 16 -S -monitor none </dev/null 2>&1 | grep -q "No such device\|failed to initialize kvm"; [ $? -ne 0 ]'; then
  for t in $(grep -l -E '/dev/kvm|enable-kvm|accel=kvm' tests/run_*.sh 2>/dev/null); do
    n=$(grep -n -F "$t" build/pc/lista.txt | head -1 | cut -d: -f1)
    [ -n "$n" ] || continue
    l=build/pc/$(printf %02d $n)
    # solo las que en el PC no han podido probar nada (las que usan KVM si
    # lo hay, como run_red, ya pasaron sin él)
    [ "$(grep -c '  OK ' $l.log 2>/dev/null)" = 0 ] || continue
    echo "  (sin KVM en el PC: $t se pasa aquí)"
    make -s para-pruebas > /dev/null 2>&1
    l2=$(sed -n "${n}p" build/pc/lista.txt)
    timeout -k 20 ${TMAX_PRUEBA:-3600} nice -n 19 bash -c "$l2" > $l.log 2>&1; echo "$? $l2 (aquí, con KVM)" > $l.rc
  done
fi
ok=0; fail=0
for r in build/pc/*.rc; do
  n=${r%.rc}
  o=$(grep -c '  OK ' $n.log); f=$(grep -c 'FAIL' $n.log)
  ok=$((ok + o)); fail=$((fail + f))
  if [ "$f" -gt 0 ] || [ "$(cut -d' ' -f1 $r)" != 0 ]; then
    echo "  FALLA: $(cut -d' ' -f2- $r) ($o OK, $f FAIL)"
  fi
done
echo "== Total: $ok OK, $fail FAIL (registros en build/pc/) =="
[ $fail -eq 0 ]
