#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/win32/juegos.sh CARPETA
# (JUEGOS) Hace los programas de prueba de «lo que pide un juego de Windows»
# (tests/juegos/*.c, docs/pentium/WIN32.md, «Lo que pide un juego»): con
# gcc -m32 -march=i586 y tools/elf2pe.py (PE de Win32 como los de MinGW,
# con MSVCRT.DLL y el arranque de w32/crt0.c), y los deja en CARPETA. Las
# funciones que no son de KERNEL32 se importan de su DLL (-i).
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -m "${1:?uso: juegos.sh CARPETA}")
cd "$RAIZ"
O=build/juegos
mkdir -p $O "$SAL"
make -s build/MSVCRT.DLL build/crt0.o build/WINMM.DLL build/DSOUND.DLL build/DINPUT.DLL build/DDRAW.DLL > /dev/null
CC="gcc -m32 -march=i586 -std=gnu11 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin
    -fno-tree-loop-distribute-patterns -fno-strict-aliasing -Wall -Wno-unused -Wno-unused-function -Werror=implicit-function-declaration -Itests/juegos"
REG=ADVAPI32.dll:RegOpenKeyExA,RegOpenKeyA,RegCreateKeyExA,RegCreateKeyA,RegCloseKey,RegQueryValueExA,RegSetValueExA,RegQueryValueA,RegSetValueA,RegEnumKeyExA,RegEnumKeyA,RegEnumValueA,RegDeleteValueA,RegDeleteKeyA,RegQueryInfoKeyA,RegFlushKey
RED=WSOCK32.dll:WSAStartup,WSACleanup,WSAGetLastError,socket,bind,sendto,recvfrom,select,ioctlsocket,closesocket,setsockopt,getsockname,gethostbyname,gethostname,htons,ntohs,htonl,inet_addr,inet_ntoa
# programa:extras de elf2pe (separados por «;»)
hacer() {
  local n=$1; shift
  # shellcheck disable=SC2086
  $CC -c tests/juegos/$n.c -o $O/$n.o
  python3 tools/elf2pe.py "$@" -l build/MSVCRT.DLL -l build/WINMM.DLL -t ventanas/win32.inc -o "$SAL/$(echo $n | tr a-z A-Z).EXE" build/crt0.o $O/$n.o
}
hacer falta -i WINMM.DLL:mixerNoHay
hacer sindll -i NOHAY.DLL:FuncionDeNoHay
hacer hilos
hacer seh
hacer reg -i $REG
hacer crt
hacer juegov -l build/DDRAW.DLL -l build/DINPUT.DLL
hacer redudp -i $RED
hacer sonj -l build/DSOUND.DLL
# la DLL de JUEGOV, en su carpeta SUB
$CC -c tests/juegos/jdll.c -o $O/jdll.o
mkdir -p "$SAL/SUB"
python3 tools/elf2pe.py --dll -o "$SAL/SUB/JDLL.DLL" $O/jdll.o
echo "win32/juegos.sh: $(ls "$SAL"/*.EXE | wc -l) programas en $SAL"
