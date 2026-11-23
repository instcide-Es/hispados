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
# (HL) lo que pedía Half-Life 1: Winsock por número (los de wsock32.dll de
# Windows), y las DLL de dentro que no están en la tabla de Hispa Ventanas
WSO=WSOCK32.dll:accept@1,bind@2,closesocket@3,connect@4,getsockname@6,getsockopt@7,htonl@8,htons@9,inet_addr@10,inet_ntoa@11,ioctlsocket@12,ntohl@14,ntohs@15,recv@16,recvfrom@17,select@18,send@19,sendto@20,setsockopt@21,shutdown@22,socket@23,gethostbyname@52,getprotobynumber@54,gethostname@57,WSAAsyncSelect@101,WSAAsyncGetHostByName@103,WSAGetLastError@111,WSASetLastError@112,WSAStartup@115,WSACleanup@116,__WSAFDIsSet@151
WS2=WS2_32.dll:ws2_ioctlsocket@10,ws2_inet_addr@11,ws2_inet_ntoa@12,ws2_ntohl@14
USR=USER32.dll:GetLastActivePopup,ExcludeUpdateRgn,WaitForInputIdle,ExitWindowsEx,DdeInitializeA,DdeUninitialize,DdeCreateStringHandleA,DdeQueryStringA,DdeFreeStringHandle,DdeConnect,DdeGetLastError,DdeCreateDataHandle,DdeGetData,DdeFreeDataHandle,DdeClientTransaction,PackDDElParam,UnpackDDElParam,ReuseDDElParam,FreeDDElParam
GDI=GDI32.dll:ScaleWindowExtEx,ScaleViewportExtEx,Escape,GetBoundsRect
SPL=WINSPOOL.DRV:DocumentPropertiesA
SEG=ADVAPI32.dll:InitializeSecurityDescriptor,GetSecurityDescriptorLength,IsValidSecurityDescriptor,GetSecurityDescriptorOwner,GetSecurityDescriptorGroup,GetSecurityDescriptorDacl,GetSecurityDescriptorSacl,GetSecurityDescriptorControl,SetSecurityDescriptorDacl,IsValidSid,IsValidAcl,InitializeAcl,AddAccessAllowedAce,AllocateAndInitializeSid,FreeSid,GetLengthSid,EqualSid,GetKernelObjectSecurity,SetKernelObjectSecurity,GetFileSecurityA,SetFileSecurityA,OpenProcessToken,AdjustTokenPrivileges,LookupPrivilegeValueA
INET=WININET.dll:InternetOpenA,InternetOpenUrlA,InternetConnectA,HttpOpenRequestA,HttpSendRequestA,HttpQueryInfoA,InternetReadFile,InternetCloseHandle,InternetGetConnectedState,InternetCrackUrlA
HL="-i $WSO -i $USR -i $GDI -i $SPL -i $SEG -i $INET"
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
# (HL) las pruebas de lo que pedía Half-Life 1
# shellcheck disable=SC2086
hacer hlk32 $HL
# shellcheck disable=SC2086
hacer hlord $HL -i $WS2
# shellcheck disable=SC2086
hacer hlmm $HL
# shellcheck disable=SC2086
hacer hlnet $HL
# shellcheck disable=SC2086
hacer hlven $HL
# (HL) el «juego de mentira» de 1999: el .EXE y VGUI.DLL en la raíz, las del mod en sus subcarpetas
mkdir -p "$SAL/JUEGO99/MOD/CL_DLLS" "$SAL/JUEGO99/MOD/DLLS"
$CC -c tests/juegos/juego99/vgui.c -o $O/vgui.o
python3 tools/elf2pe.py --dll -o "$SAL/JUEGO99/VGUI.DLL" $O/vgui.o
for d in cliente:MOD/CL_DLLS/CLIENTE servidor:MOD/DLLS/SERVIDOR; do
  $CC -c tests/juegos/juego99/${d%%:*}.c -o $O/${d%%:*}.o
  # shellcheck disable=SC2086
  python3 tools/elf2pe.py --dll $HL -i VGUI.dll:VguiPanel -l build/WINMM.DLL -t ventanas/win32.inc -o "$SAL/JUEGO99/${d##*:}.DLL" $O/${d%%:*}.o
done
$CC -c tests/juegos/juego99/juego.c -o $O/juego.o
# shellcheck disable=SC2086
python3 tools/elf2pe.py $HL -i VGUI.dll:VguiPanel -l build/MSVCRT.DLL -l build/WINMM.DLL -t ventanas/win32.inc -o "$SAL/JUEGO99/JUEGO.EXE" build/crt0.o $O/juego.o
# la DLL de JUEGOV, en su carpeta SUB
$CC -c tests/juegos/jdll.c -o $O/jdll.o
mkdir -p "$SAL/SUB"
python3 tools/elf2pe.py --dll -o "$SAL/SUB/JDLL.DLL" $O/jdll.o
echo "win32/juegos.sh: $(ls "$SAL"/*.EXE | wc -l) programas en $SAL"
