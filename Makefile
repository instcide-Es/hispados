# Hispa-DOS - compilación / build
NASM  ?= nasm
QEMU  ?= qemu-system-i386
B      = build
IMG    = $(B)/hispados.img
HDD    = $(B)/hdd.img
NFLAGS = -f bin -i $(B)/gen/lib/ -i lib/
# los fuentes están en UTF-8: se pasan a CP437 (la página de DOS) al ensamblar
ASM = python3 tools/cp437.py lib/util.inc $(B)/gen/lib/util.inc && python3 tools/cp437.py $< $(B)/gen/$< && $(NASM) $(NFLAGS)
ifdef TRACE
KFLAGS = -DTRACE
endif
# Para probar el núcleo con el código fuera de su segmento de datos en todas
# las pruebas (como con DOS=HIGH, haya o no gestor XMS), en otra carpeta:
#   make B=build_fh KFLAGS=-DFORCEHIGH && tests/run_xxx_tests.sh build_fh/hispados.img

all: $(IMG)

$(B):
	mkdir -p $(B)

$(B)/boot.bin: boot/boot.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/boot16.bin: boot/boot16.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/mbr.bin: boot/mbr.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/KERNEL.SYS: kernel/kernel.asm lib/version.inc | $(B)
	$(ASM) $(KFLAGS) -o $@ $(B)/gen/$<
$(B)/COMMAND.COM: shell/command.asm lib/version.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/FDISK.COM: utils/fdisk.asm lib/util.inc lib/version.inc $(B)/mbr.bin | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/FORMAT.COM: utils/format.asm lib/util.inc $(B)/boot.bin $(B)/boot16.bin | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HELLO.COM: tests/progs/hello.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HELLOEXE.EXE: tests/progs/helloexe.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/UPPER.COM: tests/progs/upper.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPRAM.SYS: drivers/hspram.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPTEST.SYS: drivers/hsptest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPXMS.SYS: drivers/hspxms.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPEMM.SYS: drivers/hspemm.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/EMSTEST.COM: tests/progs/emstest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/VCPITEST.COM: tests/progs/vcpitest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/UMBTEST.COM: tests/progs/umbtest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/EMMMISC.COM: tests/progs/emmmisc.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# SBTEST: lo que hace un juego con la Sound Blaster (tests/run_sbemu_tests.sh)
# MEZTEST: el mezclador de HSPSND y los registros de la SB (tests/run_mezclador_tests.sh)
$(B)/MEZTEST.COM: tests/progs/meztest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SBTEST.COM: tests/progs/sbtest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# VOLTIME: cuánto tarda un cambio de volumen en HSPSND (tests/run_volumen_tests.sh)
$(B)/VOLTIME.COM: tests/progs/voltime.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [SB-EOI] KBSB: el teclado mientras suena la SB (tests/run_sbeoi_tests.sh)
$(B)/KBSB.COM: tests/progs/kbsb.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SINIRQ.COM: tests/progs/sinirq.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [SB-PM] SBPM: lo mismo desde modo protegido por DPMI (tests/run_sbpm_tests.sh)
$(B)/SBPM.COM: tests/progs/sbpm.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [SB-VGA] SBVGA: modo X mientras suena la SB, por DPMI (tests/run_sbvga_tests.sh)
$(B)/SBVGA.COM: tests/progs/sbvga.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/XMSTEST.COM: tests/progs/xmstest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/MCBWALK.COM: tests/progs/mcbwalk.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/LOLTEST.COM: tests/progs/loltest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# el Realtek ALC269 del Toshiba NB250, simulado (AUDICION /SIM:ALC269, HSPSND /SIMALC)
$(B)/gen/lib/alcsim.bin: lib/alcsim.asm | $(B)
	mkdir -p $(B)/gen/lib
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPSND.COM: drivers/hspsnd.asm lib/hspsb.inc lib/hspopl.inc lib/hspopl_st.inc lib/hspmez.inc lib/hspmezi.inc lib/cirrus.inc lib/macsim.inc $(B)/gen/lib/alcsim.bin | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPDPMI.COM: drivers/hspdpmi.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/DPMI16.COM: tests/progs/dpmi16.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/DPMI32.COM: tests/progs/dpmi32.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [HISPA-486] HW32: programas Win32 de consola sobre HSPDPMI (docs/486/HISPA-486.md)
$(B)/HW32.COM: utils/hw32.asm utils/hw32k.inc lib/version.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/PEHOLA.EXE: tests/progs/pehola.asm | $(B)
	$(NASM) -f bin -o $@ $<
$(B)/PEFALTA.EXE: tests/progs/pehola.asm | $(B)
	$(NASM) -f bin -DFALTA -o $@ $<
# un Win32 hecho en C (gcc -m32 y tools/elf2pe.py); si no hay gcc de 32 bits,
# la prueba lo salta
CC32 = gcc -m32 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -fno-tree-loop-distribute-patterns
$(B)/HOLAC.EXE: tests/progs/holac.c tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/holac.o
	python3 tools/elf2pe.py -o $@ $(B)/holac.o
$(B)/K32PRUE.EXE: tests/progs/k32prue.c tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/k32prue.o
	python3 tools/elf2pe.py -o $@ $(B)/k32prue.o
# MSVCRT.DLL de Hispa-486 (la biblioteca de C de Win32, en C)
$(B)/MSVCRT.DLL: w32/msvcrt.c tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/msvcrt.o
	python3 tools/elf2pe.py --dll -o $@ $(B)/msvcrt.o
$(B)/DLLPRUE.DLL: tests/progs/dllprue.c tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/dllprue.o
	python3 tools/elf2pe.py --dll -o $@ $(B)/dllprue.o
$(B)/USADLL.EXE: tests/progs/usadll.c tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/usadll.o
	python3 tools/elf2pe.py -o $@ -i DLLPRUE.DLL:Doble,contador $(B)/usadll.o
$(B)/LANZA.EXE: tests/progs/lanza.c tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/lanza.o
	python3 tools/elf2pe.py -o $@ -i msvcrt.dll:printf,system,_spawnvp,exit $(B)/lanza.o
$(B)/K32CON.EXE: tests/progs/k32con.c tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/k32con.o
	python3 tools/elf2pe.py -o $@ $(B)/k32con.o
# [HISPA-486] fase 3a: programas Win32 de ventanas, que corren dentro de Hispa
# Ventanas (ventanas/win32.inc; tests/run_win32gui_tests.sh)
$(B)/WINGUI.EXE: tests/progs/wingui.c tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/wingui.o
	python3 tools/elf2pe.py --gui -o $@ -i USER32.dll:MessageBoxA $(B)/wingui.o
$(B)/WINFALTA.EXE: tests/progs/wingui.c tools/elf2pe.py | $(B)
	$(CC32) -DFALTA -c $< -o $(B)/winfalta.o
	python3 tools/elf2pe.py --gui -o $@ -i USER32.dll:MessageBoxA,SetWindowsHookA $(B)/winfalta.o
# [HISPA-486] fase 3b: el «Hola, mundo» de ventana (ventanas/win32u.inc;
# tests/run_win32hola_tests.sh)
W32HOLA_U = RegisterClassA,CreateWindowExA,ShowWindow,UpdateWindow,DestroyWindow,DefWindowProcA,GetMessageA,TranslateMessage,DispatchMessageA,PostQuitMessage,PostMessageA,SendMessageA,GetClientRect,GetWindowRect,InvalidateRect,SetWindowTextA,GetWindowTextA,SetTimer,KillTimer,MessageBoxA,LoadCursorA,LoadIconA,GetSystemMetrics,GetDC,ReleaseDC,BeginPaint,EndPaint,FillRect,DrawTextA,wsprintfA
W32HOLA_G = TextOutA,SetTextColor,SetBkColor,SetBkMode,GetStockObject,CreateSolidBrush,CreatePen,SelectObject,DeleteObject,Rectangle,MoveToEx,LineTo,GetTextExtentPoint32A
$(B)/WINHOLA.EXE: tests/progs/winhola.c tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/winhola.o
	python3 tools/elf2pe.py --gui -o $@ -i USER32.dll:$(W32HOLA_U) -i GDI32.dll:$(W32HOLA_G) $(B)/winhola.o
# [HISPA-486] fase 3c: KERNEL32 completo (HW32 de módulo), DLL y MSVCRT dentro
# de Hispa Ventanas (tests/run_win32crt_tests.sh). -l y -t: las importaciones
# salen de lo que exporta la DLL y de la tabla de USER32 y GDI32 de Hispa Ventanas
W32L = -l $(B)/MSVCRT.DLL -t ventanas/win32.inc
W32DEP = tests/progs/win32min.h tools/elf2pe.py $(B)/MSVCRT.DLL ventanas/win32.inc
$(B)/WINCRT.EXE: tests/progs/wincrt.c $(W32DEP) $(B)/DLLPRUE.DLL | $(B)
	$(CC32) -c $< -o $(B)/wincrt.o
	python3 tools/elf2pe.py --gui -o $@ -l $(B)/DLLPRUE.DLL $(W32L) $(B)/wincrt.o
$(B)/WINSINDL.EXE: tests/progs/wincrt.c $(W32DEP) $(B)/DLLPRUE.DLL | $(B)
	$(CC32) -DSINDLL -c $< -o $(B)/winsindl.o
	python3 tools/elf2pe.py --gui -o $@ -i NOESTA.DLL:NoEsta -l $(B)/DLLPRUE.DLL $(W32L) $(B)/winsindl.o
# los controles de serie (tests/run_win32ctl_tests.sh)
$(B)/WINCTL.EXE: tests/progs/winctl.c $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/winctl.o
	python3 tools/elf2pe.py --gui -o $@ $(W32L) $(B)/winctl.o
# los menús, los recursos (--res: tools/rcmini.py) y los diálogos de plantilla
# (tests/run_win32menu_tests.sh)
$(B)/WINMENU.EXE: tests/progs/winmenu.c tests/progs/winmenu.rc tools/rcmini.py $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/winmenu.o
	python3 tools/elf2pe.py --gui -o $@ --res tests/progs/winmenu.rc $(W32L) $(B)/winmenu.o
# la calculadora: la prueba final de la fase 3c (tests/run_win32calc_tests.sh)
$(B)/WINCALC.EXE: tests/progs/wincalc.c tests/progs/wincalc.rc tools/rcmini.py $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/wincalc.o
	python3 tools/elf2pe.py --gui -o $@ --res tests/progs/wincalc.rc $(W32L) $(B)/wincalc.o
$(B)/PLAYWAV.COM: utils/playwav.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SONIDO.COM: utils/sonido.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# AUDICION: prueba de sonido con preguntas de sí o no (docs/AUDICION.md)
$(B)/AUDICION.COM: utils/audicion.asm lib/cirrus.inc lib/macsim.inc $(B)/gen/lib/alcsim.bin | $(B)
	$(ASM) -o $@ $(B)/gen/$<
 $(B)/INSTALAR.COM: utils/instalar.asm lib/copiaarbol.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/EDIT.COM: utils/edit.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/GESTOR.COM: utils/gestor.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SYSINFO.COM: utils/sysinfo.asm lib/version.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# EQUIPO: informe del hardware en texto; DIAGNOSTICO (sistema/DIAGNOST.BAT) lo
# guarda con lo demás en INFORME.TXT (tests/run_equipo_tests.sh)
$(B)/EQUIPO.COM: utils/equipo.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# APAGAR y REINICIAR (en el disco REINICIA.COM): el mismo fuente
$(B)/APAGAR.COM: utils/apagar.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/REINICIAR.COM: utils/apagar.asm lib/util.inc | $(B)
	$(ASM) -DREINICIAR -o $@ $(B)/gen/$<
$(B)/HSPCD.SYS: drivers/hspcd.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/KEYB.COM: utils/keyb.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SYS.COM: utils/sys.asm $(B)/boot.bin $(B)/boot16.bin | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPRATON.COM: drivers/hspraton.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# Red (docs/RED.md): HSPRED (packet driver residente) y las órdenes CONFRED
# (IPCONFIG), PING y DESCARGAR (en el disco DESCARGA.COM), con la pila TCP/IP
# propia de lib/red.inc
$(B)/HSPRED.COM: drivers/hspred.asm | $(B)
	$(ASM) -l $(B)/gen/HSPRED.lst -o $@ $(B)/gen/$<
	@# lo residente no puede usar nada de la instalación (se descarta)
	python3 tools/initcheck.py $(B)/gen/HSPRED.lst init || { rm -f $@; exit 1; }
RED_DEP = lib/red.inc lib/util.inc
$(B)/CONFRED.COM: utils/confred.asm $(RED_DEP) | $(B)
	python3 tools/cp437.py lib/red.inc $(B)/gen/lib/red.inc && $(ASM) -o $@ $(B)/gen/$<
$(B)/PING.COM: utils/ping.asm $(RED_DEP) | $(B)
	python3 tools/cp437.py lib/red.inc $(B)/gen/lib/red.inc && $(ASM) -o $@ $(B)/gen/$<
$(B)/DESCARGAR.COM: utils/descargar.asm $(RED_DEP) | $(B)
	python3 tools/cp437.py lib/red.inc $(B)/gen/lib/red.inc && $(ASM) -o $@ $(B)/gen/$<
# CONSOLA (docs/CONSOLA.md): consola remota por red, abierta y con código de
# acceso. Usa el modo servidor (tcp_escuchar) de la pila de lib/red.inc.
$(B)/CONSOLA.COM: utils/consola.asm $(RED_DEP) | $(B)
	python3 tools/cp437.py lib/red.inc $(B)/gen/lib/red.inc && $(ASM) -o $@ $(B)/gen/$<
# ACTUALIZAR (docs/RED.md): busca por HTTP si hay una versión más nueva del
# sistema (lib/version.inc) y avisa. En el disco es ACTUALIZ.COM (8.3).
 $(B)/ACTUALIZAR.COM: utils/actualizar.asm $(RED_DEP) lib/version.inc lib/copiaarbol.inc lib/equipodet.inc | $(B)
	python3 tools/cp437.py lib/red.inc $(B)/gen/lib/red.inc && $(ASM) -o $@ $(B)/gen/$<
RED = $(B)/HSPRED.COM $(B)/CONFRED.COM $(B)/PING.COM $(B)/DESCARGAR.COM $(B)/CONSOLA.COM $(B)/ACTUALIZAR.COM
$(B)/FSTEST.COM: tests/progs/fstest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SHARETST.COM: tests/progs/sharetst.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/TECLAS.COM: tests/progs/teclas.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/MOUSETST.COM: tests/progs/mousetst.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/NOC2.COM: tests/progs/noc2.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/MOVER.COM: utils/mover.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/ATRIBUTO.COM: utils/atributo.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/ARBOL.COM: utils/arbol.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/BUSCAR.COM: utils/buscar.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/ORDENAR.COM: utils/ordenar.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/PAGINAR.COM: utils/paginar.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/ELEGIR.COM: utils/elegir.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/ETIQUETA.COM: utils/etiqueta.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/TECLA.COM: utils/tecla.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/XCOPIAR.COM: utils/xcopiar.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/REVISAR.COM: utils/revisar.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/EXPANDIR.COM: utils/expandir.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPUSB.SYS: drivers/hspusb.asm lib/hidjoyx.inc lib/hidjoyp.inc lib/hidraton.inc lib/hidratx.inc lib/usb_hc.inc lib/usb_uhci.inc lib/usb_ohci.inc lib/usb_ehci.inc lib/usb_hcini.inc lib/usb_red.inc tools/redcheck.py | $(B)
	for f in lib/usb_*.inc; do python3 tools/cp437.py $$f $(B)/gen/$$f || exit 1; done
	$(ASM) -l $(B)/gen/HSPUSB.lst -o $@ $(B)/gen/$<
	@# lo residente no puede usar nada de la inicialización, que se descarta
	@# (las permitidas: solo al arrancar, o con /V o /INFO; ver hspusb.asm)
	python3 tools/initcheck.py $(B)/gen/HSPUSB.lst init,init_end,info_line_go,role_hid,role_joy,role_net 'msg_*' || { rm -f $@; exit 1; }
	@# [USB-RED] el bloque de la red (.red) se ejecuta en otro sitio: sus
	@# llamadas a .text, con MCALL, y sus datos, con el registro de la base
	python3 tools/redcheck.py $(B)/gen/HSPUSB.lst || { rm -f $@; exit 1; }
# VELOCIDAD (en el disco VELOCIDA.COM: 8 letras) y LOADFIX: para los juegos
# antiguos que fallan en equipos rápidos o cargados en los primeros 64 KB
$(B)/VELOCIDAD.COM: utils/velocidad.asm lib/util.inc lib/ejecutar.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/LOADFIX.COM: utils/loadfix.asm lib/util.inc lib/ejecutar.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# PROTECTOR (en el disco PROTECTO.COM: 8 letras): protector de pantalla en
# modo texto, residente («Lluvia verde»; tests/run_protector_tests.sh)
$(B)/PROTECTOR.COM: utils/protector.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# Mandos de juego (docs/MANDOS.md): HSPJOY.COM (residente) y MANDO.COM
$(B)/HSPJOY.COM: drivers/hspjoy.asm lib/hidjoyx.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/MANDO.COM: utils/mando.asm lib/util.inc lib/hidjoyx.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# programas de las pruebas de los mandos (tests/run_joy_tests.sh los copia a
# una copia de la imagen; no van en el disco de distribución): el analizador
# de descriptores HID con mandos reales, lo que ve un juego (INT 15h 84h y la
# interfaz de HSPJOY) y HSPJOY con los puertos simulados (el 201h y las
# entradas del puerto de impresora no se pueden mover en QEMU)
JOYPROGS = $(B)/HIDTEST.COM $(B)/JOYTEST.COM $(B)/HSPJOYT.COM $(B)/RATONTST.COM
$(B)/RATONTST.COM: tests/progs/ratontst.asm lib/hidraton.inc lib/hidratx.inc lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HIDTEST.COM: tests/progs/hidtest.asm tests/progs/hidmandos.inc lib/hidjoyx.inc lib/hidjoyp.inc lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/JOYTEST.COM: tests/progs/joytest.asm lib/util.inc lib/hidjoyx.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPJOYT.COM: drivers/hspjoy.asm tests/progs/joysim.inc lib/hidjoyx.inc | $(B)
	$(ASM) -DPRUEBA -o $@ $(B)/gen/$<
joyprogs: $(JOYPROGS)

# Hispa Ventanas (docs/VENTANAS.md): el entorno gráfico, en ventanas/.
# La fuente de mapa de bits sale del diseño en texto ventanas/fuente.txt.
VENSRC = $(wildcard ventanas/*.asm ventanas/*.inc)
$(B)/VENTANAS.COM: $(VENSRC) ventanas/fuente.txt tools/mkfuente.py tools/icopack.py | $(B)
	for f in $(VENSRC); do python3 tools/cp437.py $$f $(B)/gen/$$f || exit 1; done
	python3 tools/mkfuente.py ventanas/fuente.txt $(B)/gen/ventanas/fuente.inc $(B)/gen/ventanas/fuente.ppm
	python3 tools/icopack.py $(B)/gen/ventanas/iconos.inc $(B)/gen/ventanas/iconos2.inc $(B)/gen/ventanas/apagado.inc
	# (lo que usa la parte de modo real tiene que caber en 64 KB: si no, error)
	# (-w-zeroing: las variables de la parte de modo real van en el archivo, a cero)
	$(NASM) -f bin -w+error=number-overflow -w-zeroing -i $(B)/gen/ventanas/ -o $@ $(B)/gen/ventanas/ventanas.asm

# Programas de Hispa Ventanas (.HVP, docs/VENTANAS.md «Programas aparte»): los
# juegos, en juegos/ (HVPS: van a \HISPADOS\JUEGOS), y los accesorios, en
# programas/ (HVPA: a \HISPADOS\PROGS). tools/mkhvp.py los ensambla y
# saca las reubicaciones.
HVPDEP = ventanas/hvp.inc ventanas/hvplib.inc ventanas/defs.inc ventanas/api.inc ventanas/hvpmez.inc \
         tools/mkhvp.py tools/icopack.py tools/cp437.py
HVPS = $(B)/CULEBRA.HVP $(B)/MINAS.HVP \
       $(B)/BLOQUES.HVP $(B)/SOLITARI.HVP $(B)/TRAGABOL.HVP $(B)/ALMACEN.HVP \
       $(B)/HISPANES.HVP $(B)/HISPASMS.HVP
$(B)/CULEBRA.HVP: juegos/culebra.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
$(B)/MINAS.HVP: juegos/minas.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# la segunda tanda de juegos: Bloques, Solitario, Tragabolas y Almacén
$(B)/BLOQUES.HVP: juegos/bloques.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
$(B)/SOLITARI.HVP: juegos/solitario.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
$(B)/TRAGABOL.HVP: juegos/tragabolas.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
$(B)/ALMACEN.HVP: juegos/almacen.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# Hispa-Nes (docs/HISPANES.md): el emulador, con sus piezas en
# programas/hispanes/, y sus juegos propios, hechos para la consola y
# ensamblados aquí con tools/asm6502.py (NESROMS: van a \HISPADOS\JUEGOS\NES)
$(B)/HISPANES.HVP: programas/hispanes.asm $(wildcard programas/hispanes/*.inc) lib/emufull.inc $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
NESROMS = $(B)/HISPA1.NES
$(B)/HISPA1.NES: nes/hispa1/hispa1.s nes/hispa1/dibujos.s tools/asm6502.py | $(B)
	python3 tools/asm6502.py $< $@
# el núcleo de Hispa-Nes como programa de Linux y sus ROM de prueba
# (tests/run_hispanes_tests.sh)
NESNAT = tests/hispanes/nesnativo.asm $(wildcard programas/hispanes/*.inc)
$(B)/nesnativo: $(NESNAT) | $(B)
	$(NASM) -f bin -i programas/hispanes/ -o $@ $< && chmod +x $@
$(B)/nesnativo_t: $(NESNAT) | $(B)
	$(NASM) -f bin -DTRAZA -i programas/hispanes/ -o $@ $< && chmod +x $@
$(B)/CPUTEST.NES: tests/hispanes/cputest.s tools/asm6502.py | $(B)
	python3 tools/asm6502.py $< $@
# (un mando «pulsado» para HSPJOY: lo que ve Hispa-Nes sin tocar el teclado)
$(B)/NESJOY.COM: tests/progs/nesjoy.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
nesprogs: $(B)/nesnativo $(B)/nesnativo_t $(B)/CPUTEST.NES $(B)/NESJOY.COM
# Hispa-System (docs/HISPASYS.md): el emulador, con sus piezas en
# programas/hispasms/, y sus juegos propios, hechos para la consola y
# ensamblados aquí con tools/asmz80.py (SMSROMS: van a \HISPADOS\JUEGOS\SMS)
$(B)/HISPASMS.HVP: programas/hispasms.asm $(wildcard programas/hispasms/*.inc) lib/emufull.inc $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
SMSROMS = $(B)/BUZO.SMS
$(B)/BUZO.SMS: sms/buzo/buzo.s $(wildcard sms/buzo/*.s sms/comun/*.s) tools/asmz80.py | $(B)
	python3 tools/asmz80.py $< $@
# el núcleo de Hispa-System como programa de Linux y sus ROM de prueba
# (tests/run_hispasms_tests.sh)
SMSNAT = tests/hispasms/smsnativo.asm $(wildcard programas/hispasms/*.inc)
$(B)/smsnativo: $(SMSNAT) | $(B)
	$(NASM) -f bin -i programas/hispasms/ -o $@ $< && chmod +x $@
$(B)/smsnativo_t: $(SMSNAT) | $(B)
	$(NASM) -f bin -DTRAZA -i programas/hispasms/ -o $@ $< && chmod +x $@
$(B)/VDPTEST.SMS: tests/hispasms/vdptest.s sms/comun/letras.s tools/asmz80.py | $(B)
	python3 tools/asmz80.py $< $@
$(B)/TMSTEST.SMS: tests/hispasms/tmstest.s sms/comun/letras.s tools/asmz80.py | $(B)
	python3 tools/asmz80.py $< $@
smsprogs: $(B)/smsnativo $(B)/smsnativo_t $(B)/VDPTEST.SMS $(B)/TMSTEST.SMS
# [Configuración] los programas de Hispa Ventanas que no son juegos van en
# \HISPADOS, al lado de VENTANAS.COM (HVPSIS): Configuración (CONFIG.HVP)
HVPSIS = $(B)/CONFIG.HVP
$(B)/CONFIG.HVP: programas/config.asm programas/comun/asocia_ini.inc $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# los accesorios (con el 1 en HVP_CABECERA) van en \HISPADOS\PROGS. El ORDEN
# importa: los grupos que no existen se crean al encontrar su primer módulo, así
# que los de «Productividad» (el 4: Pluma, Cuentas y Facturas) van al final
# para que ese grupo nuevo salga detrás de «Sonido» y no le quite su sitio.
# (tools/mkusb.py recibe esta lista y copia en este mismo orden)
# datos comunes de los programas de Productividad, que van en \HISPADOS\PROGS al
# lado de los .HVP (TIPOS.DAT: los tipos de letra; ver docs/TIPOS-DE-LETRA.md).
# Solo en la instalación (imagen USB y .PAQ, por tools/mkusb.py), NO en el
# disquete de arranque: no cabe con sitio para guardar (Pluma, sin él, usa la letra
# «Hispa»). Irá en el disquete de Oficina cuando se divida la distribución.
# [0.9.22] Con la familia de oficina ya no cabe todo en el disquete de arranque:
#  HVPA: los accesorios, en el disquete de arranque (y en el USB y el .PAQ);
#  HVPO: la oficina (Productividad), en el disquete «Oficina» (build/
#        hispados-oficina.img) y en el USB y el .PAQ, con sus datos (PROGSDAT).
# En el USB van todos, HVPA y luego HVPO (el orden de los grupos, arriba).
PROGSDAT = programas/comun/TIPOS.DAT programas/comun/TIPOS12.DAT programas/comun/TIPOS24.DAT \
           programas/comun/TIPOS32.DAT programas/comun/DICCIO.DAT
HVPA = $(B)/PINTURA.HVP $(B)/TERMINAL.HVP $(B)/TECLADO.HVP $(B)/MIRADOR.HVP $(B)/TAREAS.HVP \
       $(B)/SONIDOS.HVP $(B)/MEZCLAD.HVP $(B)/ARCHIVOS.HVP
HVPO = $(B)/PLUMA.HVP $(B)/CUENTAS.HVP $(B)/FACTURAS.HVP $(B)/DATOS.HVP $(B)/LECTOR.HVP \
       $(B)/PUBLICA.HVP $(B)/PRESENTA.HVP
# [ARCHIVOS] el Administrador de archivos (el 3 de HVP_CABECERA: grupo «Principal»)
$(B)/ARCHIVOS.HVP: programas/archivos.asm $(wildcard programas/archivos/*.inc) programas/comun/asocia_ini.inc $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
$(B)/PINTURA.HVP: programas/pintura.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# Pluma, el procesador de textos (Productividad: el 4 de HVP_CABECERA)
$(B)/PLUMA.HVP: programas/pluma.asm $(wildcard programas/pluma/*.inc) $(wildcard programas/comun/*.inc) $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# Cuentas, la hoja de cálculo (Productividad: el 4 de HVP_CABECERA)
$(B)/CUENTAS.HVP: programas/cuentas.asm $(wildcard programas/cuentas/*.inc) $(wildcard programas/comun/*.inc) $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# Facturas, la economía de casa y de un pequeño negocio (Productividad: el 4)
$(B)/FACTURAS.HVP: programas/facturas.asm $(wildcard programas/comun/*.inc) $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# [0.9.22] el resto de la familia de oficina (docs/OFICINA.md): Datos, el Lector
# de PDF, Publicaciones y Presentaciones (Productividad, el 4) y Mirador, el
# visor de imágenes (Accesorios, el 1). Cada uno con sus .inc y los comunes.
$(B)/DATOS.HVP: programas/datos.asm $(wildcard programas/datos/*.inc) $(wildcard programas/comun/*.inc) $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
$(B)/LECTOR.HVP: programas/lector.asm $(wildcard programas/lector/*.inc) $(wildcard programas/comun/*.inc) $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
$(B)/PUBLICA.HVP: programas/publica.asm $(wildcard programas/publica/*) $(wildcard programas/comun/*.inc) $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
$(B)/PRESENTA.HVP: programas/presenta.asm $(wildcard programas/presenta/*) $(wildcard programas/comun/*.inc) $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
$(B)/MIRADOR.HVP: programas/mirador.asm $(wildcard programas/mirador/*.inc) $(wildcard programas/comun/*.inc) $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# [TAREAS] el Administrador de tareas (Principal, el 3; ventanas/tareas_api.inc).
# CUELGA.HVP (programas/tareas/cuelga.asm) es solo para tests/run_tareas_tests.sh
$(B)/TAREAS.HVP: programas/tareas.asm $(wildcard programas/tareas/*.inc) $(wildcard programas/comun/*.inc) $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# Terminal, el símbolo del sistema (accesorio: el 1 de HVP_CABECERA)
$(B)/TERMINAL.HVP: programas/terminal.asm $(wildcard programas/terminal/*.inc) lib/version.inc $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# Teclado, la herramienta de taller del teclado (accesorio: el 1 de HVP_CABECERA)
$(B)/TECLADO.HVP: programas/teclado.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# el Reproductor de sonido y los sonidos propios (tools/mksample.py): HISPA.WAV,
# su sonido de prueba, y CAMPANA.WAV, la campanita del volumen de Hispa
# Ventanas (SONS: van a \HISPADOS)
$(B)/SONIDOS.HVP: programas/sonidos.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# [MEZCLADOR] el Mezclador (docs/MEZCLADOR.md); con el Reproductor, en el
# grupo «Sonido» (el 2 de HVP_CABECERA), y en \HISPADOS\PROGS
$(B)/MEZCLAD.HVP: programas/mezclad.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
SONS = $(B)/HISPA.WAV $(B)/CAMPANA.WAV
$(B)/HISPA.WAV: tools/mksample.py | $(B)
	python3 tools/mksample.py $(B)
$(B)/CAMPANA.WAV: $(B)/HISPA.WAV

FILES = $(B)/KERNEL.SYS $(B)/COMMAND.COM $(B)/FDISK.COM $(B)/FORMAT.COM \
        $(B)/HELLO.COM $(B)/HELLOEXE.EXE $(B)/UPPER.COM \
        $(B)/HSPRAM.SYS $(B)/HSPTEST.SYS $(B)/HSPXMS.SYS $(B)/HSPEMM.SYS \
        $(B)/XMSTEST.COM $(B)/MCBWALK.COM $(B)/LOLTEST.COM \
        $(B)/HSPSND.COM $(B)/HSPDPMI.COM $(B)/PLAYWAV.COM $(B)/SONIDO.COM $(B)/AUDICION.COM $(B)/SYS.COM $(B)/HSPUSB.SYS $(B)/KEYB.COM $(B)/INSTALAR.COM $(B)/EDIT.COM $(B)/GESTOR.COM \
        $(B)/SYSINFO.COM $(B)/EQUIPO.COM $(B)/APAGAR.COM $(B)/REINICIAR.COM $(B)/HSPCD.SYS $(B)/HSPRATON.COM $(B)/HSPJOY.COM $(B)/MANDO.COM $(B)/MOUSETST.COM $(B)/TECLAS.COM $(B)/NOC2.COM $(B)/SHARETST.COM $(B)/FSTEST.COM \
        $(B)/MOVER.COM $(B)/ATRIBUTO.COM $(B)/ARBOL.COM $(B)/BUSCAR.COM $(B)/ORDENAR.COM $(B)/PAGINAR.COM $(B)/ELEGIR.COM $(B)/EXPANDIR.COM $(B)/ETIQUETA.COM $(B)/XCOPIAR.COM $(B)/REVISAR.COM $(B)/VELOCIDAD.COM $(B)/LOADFIX.COM $(B)/PROTECTOR.COM $(B)/TECLA.COM \
        $(B)/VENTANAS.COM $(HVPS) $(HVPSIS) $(HVPA) $(NESROMS) $(SMSROMS) $(SONS) $(RED)

$(IMG): $(B)/boot.bin $(FILES) sistema/ALIAS.INI sistema/AYUDA.HLP sistema/MANDO.INI sistema/RED.INI sistema/TECLAS.INI sistema/DIAGNOST.BAT $(wildcard tests/disk/*.* tests/disk/*/*.*)
	rm -f $@
	mformat -C -f 1440 -v HISPADOS -B $(B)/boot.bin -i $@ ::
	mcopy -i $@ $(B)/KERNEL.SYS ::KERNEL.SYS
	mcopy -i $@ $(B)/COMMAND.COM ::COMMAND.COM
	mcopy -i $@ tests/disk/README.TXT ::README.TXT
	mcopy -i $@ tests/disk/AUTOEXEC.BAT ::AUTOEXEC.BAT
	# el sistema, en \HISPADOS (como \DOS en MS-DOS): nombres en español y
	# los clásicos en ALIAS.INI
	mmd   -i $@ ::HISPADOS
	mcopy -i $@ $(B)/EDIT.COM ::HISPADOS/EDITAR.COM
	mcopy -i $@ $(B)/FDISK.COM ::HISPADOS/PARTIR.COM
	mcopy -i $@ $(B)/FORMAT.COM ::HISPADOS/FORMATEA.COM
	mcopy -i $@ $(B)/SYS.COM ::HISPADOS/SISTEMA.COM
	mcopy -i $@ $(B)/KEYB.COM ::HISPADOS/TECLADO.COM
	mcopy -i $@ $(B)/PLAYWAV.COM ::HISPADOS/SONAR.COM
	mcopy -i $@ $(B)/SONIDO.COM ::HISPADOS/SONIDO.COM
	mcopy -i $@ $(B)/AUDICION.COM ::HISPADOS/AUDICION.COM
	mcopy -i $@ $(B)/SYSINFO.COM ::HISPADOS/INFO.COM
	mcopy -i $@ $(B)/GESTOR.COM ::HISPADOS/GESTOR.COM
	# informe del equipo: EQUIPO y DIAGNOSTICO (DIAGNOST.BAT, 8 letras)
	mcopy -i $@ $(B)/EQUIPO.COM ::HISPADOS/EQUIPO.COM
	iconv -f UTF-8 -t CP437 sistema/DIAGNOST.BAT | sed 's/$$/\r/' > $(B)/DIAGNOST.BAT
	mcopy -i $@ $(B)/DIAGNOST.BAT ::HISPADOS/DIAGNOST.BAT
	mcopy -i $@ $(B)/INSTALAR.COM ::HISPADOS/INSTALAR.COM
	mcopy -i $@ $(B)/APAGAR.COM ::HISPADOS/APAGAR.COM
	mcopy -i $@ $(B)/REINICIAR.COM ::HISPADOS/REINICIA.COM
	mcopy -i $@ $(B)/MOVER.COM ::HISPADOS/MOVER.COM
	mcopy -i $@ $(B)/ATRIBUTO.COM ::HISPADOS/ATRIBUTO.COM
	mcopy -i $@ $(B)/ARBOL.COM ::HISPADOS/ARBOL.COM
	mcopy -i $@ $(B)/BUSCAR.COM ::HISPADOS/BUSCAR.COM
	mcopy -i $@ $(B)/ORDENAR.COM ::HISPADOS/ORDENAR.COM
	mcopy -i $@ $(B)/PAGINAR.COM ::HISPADOS/PAGINAR.COM
	mcopy -i $@ $(B)/ELEGIR.COM ::HISPADOS/ELEGIR.COM
	mcopy -i $@ $(B)/EXPANDIR.COM ::HISPADOS/EXPANDIR.COM
	mcopy -i $@ $(B)/ETIQUETA.COM ::HISPADOS/ETIQUETA.COM
	mcopy -i $@ $(B)/TECLA.COM ::HISPADOS/TECLA.COM
	mcopy -i $@ $(B)/XCOPIAR.COM ::HISPADOS/XCOPIAR.COM
	mcopy -i $@ $(B)/REVISAR.COM ::HISPADOS/REVISAR.COM
	mcopy -i $@ $(B)/VELOCIDAD.COM ::HISPADOS/VELOCIDA.COM
	mcopy -i $@ $(B)/LOADFIX.COM ::HISPADOS/LOADFIX.COM
	mcopy -i $@ $(B)/PROTECTOR.COM ::HISPADOS/PROTECTO.COM
	iconv -f UTF-8 -t CP437 sistema/ALIAS.INI | sed 's/$$/\r/' > $(B)/ALIAS.INI
	mcopy -i $@ $(B)/ALIAS.INI ::HISPADOS/ALIAS.INI
	# los temas de AYUDA (fuera del intérprete: no le quitan memoria a los programas)
	grep -v '^;' sistema/AYUDA.HLP | iconv -f UTF-8 -t CP437 | sed 's/$$/\r/' > $(B)/AYUDA.HLP
	mcopy -i $@ $(B)/AYUDA.HLP ::HISPADOS/AYUDA.HLP
	# remapeo de teclas para teclados raros (docs/TECLADO.md); ejemplo sin
	# remapeos activos, el instalador por red lo conserva al actualizar
	iconv -f UTF-8 -t CP437 sistema/TECLAS.INI | sed 's/$$/\r/' > $(B)/TECLAS.INI
	mcopy -i $@ $(B)/TECLAS.INI ::HISPADOS/TECLAS.INI
	mcopy -i $@ $(B)/HSPRAM.SYS ::HISPADOS/HSPRAM.SYS
	mcopy -i $@ $(B)/HSPTEST.SYS ::HISPADOS/HSPTEST.SYS
	mcopy -i $@ $(B)/HSPXMS.SYS ::HISPADOS/HSPXMS.SYS
	mcopy -i $@ $(B)/HSPEMM.SYS ::HISPADOS/HSPEMM.SYS
	mcopy -i $@ $(B)/HSPSND.COM ::HISPADOS/HSPSND.COM
	mcopy -i $@ $(B)/HSPDPMI.COM ::HISPADOS/HSPDPMI.COM
	mcopy -i $@ $(B)/HSPUSB.SYS ::HISPADOS/HSPUSB.SYS
	mcopy -i $@ $(B)/HSPCD.SYS ::HISPADOS/HSPCD.SYS
	mcopy -i $@ $(B)/HSPRATON.COM ::HISPADOS/HSPRATON.COM
	# mandos de juego: el residente, la utilidad y su configuración de ejemplo
	mcopy -i $@ $(B)/HSPJOY.COM ::HISPADOS/HSPJOY.COM
	mcopy -i $@ $(B)/MANDO.COM ::HISPADOS/MANDO.COM
	iconv -f UTF-8 -t CP437 sistema/MANDO.INI | sed 's/$$/\r/' > $(B)/MANDO.INI
	mcopy -i $@ $(B)/MANDO.INI ::HISPADOS/MANDO.INI
	# la red: el controlador, las órdenes y su configuración (docs/RED.md)
	mcopy -i $@ $(B)/HSPRED.COM ::HISPADOS/HSPRED.COM
	mcopy -i $@ $(B)/CONFRED.COM ::HISPADOS/CONFRED.COM
	mcopy -i $@ $(B)/PING.COM ::HISPADOS/PING.COM
	mcopy -i $@ $(B)/DESCARGAR.COM ::HISPADOS/DESCARGA.COM
	mcopy -i $@ $(B)/CONSOLA.COM ::HISPADOS/CONSOLA.COM
	mcopy -i $@ $(B)/ACTUALIZAR.COM ::HISPADOS/ACTUALIZ.COM
	iconv -f UTF-8 -t CP437 sistema/RED.INI | sed 's/$$/\r/' > $(B)/RED.INI
	mcopy -i $@ $(B)/RED.INI ::HISPADOS/RED.INI
	mcopy -i $@ $(B)/VENTANAS.COM ::HISPADOS/VENTANAS.COM
	for f in $(HVPSIS); do mcopy -i $@ $$f ::HISPADOS/ || exit 1; done	# [Configuración]
	for f in $(SONS); do mcopy -i $@ $$f ::HISPADOS/ || exit 1; done	# el sonido de prueba y la campanita
	# los juegos de Hispa Ventanas, en \HISPADOS\JUEGOS, y los cartuchos de
	# sus emuladores, cada uno en su carpeta: JUEGOS\NES y JUEGOS\SMS
	mmd   -i $@ ::HISPADOS/JUEGOS ::HISPADOS/JUEGOS/NES ::HISPADOS/JUEGOS/SMS
	for f in $(HVPS); do mcopy -i $@ $$f ::HISPADOS/JUEGOS/ || exit 1; done
	for f in $(NESROMS); do mcopy -i $@ $$f ::HISPADOS/JUEGOS/NES/ || exit 1; done
	for f in $(SMSROMS); do mcopy -i $@ $$f ::HISPADOS/JUEGOS/SMS/ || exit 1; done
	# y los accesorios (Pintura, el Reproductor de sonido y el Mezclador), en \HISPADOS\PROGS
	mmd   -i $@ ::HISPADOS/PROGS
	for f in $(HVPA); do mcopy -i $@ $$f ::HISPADOS/PROGS/ || exit 1; done
	# pruebas
	mmd   -i $@ ::BAT
	mcopy -i $@ tests/disk/BAT/PRUEBA.BAT ::BAT/PRUEBA.BAT
	mcopy -i $@ tests/disk/BAT/OTRA.BAT ::BAT/OTRA.BAT
	mmd   -i $@ ::DOCS
	mcopy -i $@ tests/disk/DOCS/INFO.TXT ::DOCS/INFO.TXT
	mmd   -i $@ ::BIN
	mcopy -i $@ $(B)/HELLO.COM ::BIN/HELLO.COM
	mcopy -i $@ $(B)/UPPER.COM ::BIN/UPPER.COM
	mcopy -i $@ $(B)/HELLOEXE.EXE ::HELLOEXE.EXE
	mcopy -i $@ $(B)/MOUSETST.COM ::BIN/MOUSETST.COM
	mcopy -i $@ $(B)/TECLAS.COM ::BIN/TECLAS.COM
	mcopy -i $@ $(B)/NOC2.COM ::BIN/NOC2.COM
	mcopy -i $@ $(B)/SHARETST.COM ::BIN/SHARETST.COM
	mcopy -i $@ $(B)/FSTEST.COM ::BIN/FSTEST.COM
	mcopy -i $@ $(B)/XMSTEST.COM ::BIN/XMSTEST.COM
	mcopy -i $@ $(B)/MCBWALK.COM ::BIN/MCBWALK.COM
	mcopy -i $@ $(B)/LOLTEST.COM ::BIN/LOLTEST.COM

# disco duro vacío de 64 MB para probar FDISK y FORMAT
$(HDD): | $(B)
	dd if=/dev/zero of=$@ bs=1M count=64 status=none

# imagen para memorias USB (arrancar o instalar)
# ---- HispaBIOS: Hispa-DOS en equipos que solo tienen UEFI (docs/HISPABIOS.md) ----
# BOOTX64.EFI y BOOTIA32.EFI van en \EFI\BOOT de la imagen USB (tools/mkusb.py).
# Con «make HBFLAGS=-DDEPURA» la BIOS cuenta lo que hace por el puerto serie.
HBSRC = hispabios/bios64.asm hispabios/bios_int.inc
HBEFI = $(B)/BOOTX64.EFI $(B)/BOOTIA32.EFI
$(B)/FUENTE16.BIN: $(B)/VENTANAS.COM tools/mkfuentebios.py
	python3 tools/mkfuentebios.py $(B)/gen/ventanas/fuente.inc $@
$(B)/BIOS64.BIN: $(HBSRC) | $(B)
	$(NASM) -f bin $(HBFLAGS) -I hispabios/ -o $@ hispabios/bios64.asm
$(B)/BIOS32.BIN: $(HBSRC) | $(B)
	$(NASM) -f bin -DARQ32 $(HBFLAGS) -I hispabios/ -o $@ hispabios/bios64.asm
$(B)/BOOTX64.EFI: hispabios/efi64.asm hispabios/pe.inc $(B)/BIOS64.BIN $(B)/FUENTE16.BIN
	$(NASM) -f bin -I hispabios/ -I $(B)/ -o $@ hispabios/efi64.asm
$(B)/BOOTIA32.EFI: hispabios/efi32.asm hispabios/pe.inc $(B)/BIOS32.BIN $(B)/FUENTE16.BIN
	$(NASM) -f bin -I hispabios/ -I $(B)/ -o $@ hispabios/efi32.asm
hispabios: $(HBEFI)

usb dist: $(IMG) $(B)/boot16.bin $(B)/mbr.bin $(FILES) $(HBEFI) $(HVPO) $(PROGSDAT) $(B)/HW32.COM $(B)/MSVCRT.DLL $(wildcard usb/* floppy/*)
	HVPA="$(notdir $(HVPA) $(HVPO))" HVPO="$(notdir $(HVPO))" PROGSDAT="$(PROGSDAT)" python3 tools/mkusb.py

run: $(IMG)
	$(QEMU) -rtc base=localtime -drive file=$(IMG),format=raw,if=floppy -boot a

run-hdd: $(IMG) $(HDD)
	$(QEMU) -rtc base=localtime -drive file=$(IMG),format=raw,if=floppy \
	        -drive file=$(HDD),format=raw,if=ide -boot a

# programas de las pruebas del núcleo (tests/run_nucleo_tests.sh los copia
# a una copia de la imagen; no van en el disco de distribución)
# programas de las pruebas de HSPEMM (tests/run_emm_tests.sh)
EMMPROGS = $(B)/EMSTEST.COM $(B)/VCPITEST.COM $(B)/UMBTEST.COM $(B)/EMMMISC.COM $(B)/SBTEST.COM $(B)/MEZTEST.COM $(B)/SBPM.COM $(B)/KBSB.COM $(B)/SINIRQ.COM $(B)/SBVGA.COM $(B)/MEMJUEGO.COM $(B)/VOLTIME.COM
emmprogs: $(EMMPROGS)
NUCPROGS = $(EMMPROGS) $(B)/DPMI16.COM $(B)/DPMI32.COM $(B)/HW32.COM $(B)/PEHOLA.EXE $(B)/PEFALTA.EXE $(B)/NUCTEST.COM $(B)/NUCHIJO.COM $(B)/NUCEXE.EXE $(B)/NUCBRK.COM $(B)/NUCCON.SYS $(B)/NUCDRV.SYS $(B)/PILTEST.COM $(B)/HMATEST.COM $(B)/HMAUSO.SYS $(B)/MEMTEST.COM $(B)/EXEPILA.EXE $(B)/LEETEST.COM $(B)/VELTEST.COM $(B)/PSPTEST.COM $(B)/PSPSH.COM
$(B)/MEMTEST.COM: tests/progs/memtest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/EXEPILA.EXE: tests/progs/exepila.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/LEETEST.COM: tests/progs/leetest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# (VELTEST, PSPTEST y PSPSH: tests/run_velocidad_tests.sh)
$(B)/PSPSH.COM: tests/progs/pspsh.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/VELTEST.COM: tests/progs/veltest.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/PSPTEST.COM: tests/progs/psptest.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HMATEST.COM: tests/progs/hmatest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HMAUSO.SYS: tests/progs/hmauso.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/PILTEST.COM: tests/progs/piltest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/NUCTEST.COM: tests/progs/nuctest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/NUCHIJO.COM: tests/progs/nuchijo.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/NUCEXE.EXE: tests/progs/nucexe.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/NUCBRK.COM: tests/progs/nucbrk.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/NUCCON.SYS: tests/progs/nuccon.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/NUCDRV.SYS: tests/progs/nucdrv.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
nucprogs: $(NUCPROGS)
# el «juego» de prueba de los accesos directos (tests/run_ventanas4_tests.sh)
$(B)/LANZTEST.COM: tests/progs/lanztest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [SONIDO-JUEGOS] (SBJUEGO: tests/run_sbjuego_tests.sh)
$(B)/SBJUEGO.COM: tests/progs/sbjuego.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<

# el que mide la memoria que ve un juego (tests/run_memjuegos_tests.sh)
$(B)/MEMJUEGO.COM: tests/progs/memjuego.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<

# y el de las opciones de juego: velocidad, LOADFIX... (tests/run_accesos_tests.sh)
$(B)/ACCJUEGO.COM: tests/progs/accjuego.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<

test: $(IMG) usb nucprogs emmprogs joyprogs nesprogs smsprogs $(B)/LANZTEST.COM $(B)/ACCJUEGO.COM
	tests/run_tests.sh $(IMG)
	tests/run_boot_tests.sh $(IMG)
	tests/run_hdd_tests.sh $(IMG)
	tests/run_write_tests.sh $(IMG)
	tests/run_batch_tests.sh $(IMG)
	tests/run_config_tests.sh $(IMG)
	tests/run_xms_tests.sh $(IMG)
	tests/run_hma_tests.sh $(IMG)
	tests/run_dpmi_tests.sh $(IMG)
	DOSHIGH=1 tests/run_dpmi_tests.sh $(IMG)
	tests/run_hw32_tests.sh $(IMG)
	tests/run_win32gui_tests.sh $(IMG)
	tests/run_win32hola_tests.sh $(IMG)
	tests/run_win32crt_tests.sh $(IMG)
	tests/run_win32ctl_tests.sh $(IMG)
	tests/run_win32menu_tests.sh $(IMG)
	tests/run_win32calc_tests.sh $(IMG)
	tests/run_emm_tests.sh $(IMG)
	tests/run_emmhigh_tests.sh $(IMG)
	tests/run_modojuegos_tests.sh $(IMG)
	tests/run_memjuegos_tests.sh $(IMG)
	EMM=1 tests/run_dpmi_tests.sh $(IMG)
	EMM=1 DOSHIGH=1 tests/run_dpmi_tests.sh $(IMG)
	EMM=1 tests/run_sound_tests.sh $(IMG)
	tests/run_sound_tests.sh $(IMG)
	tests/run_audicion_tests.sh $(IMG)
	tests/run_aprendido_tests.sh $(IMG)
	tests/run_sbemu_tests.sh $(IMG)
	tests/run_sbpm_tests.sh $(IMG)
	tests/run_sbeoi_tests.sh $(IMG)
	tests/run_sbvga_tests.sh $(IMG)
	tests/run_usb_tests.sh
	tests/run_instalar_usb_tests.sh
	tests/run_actualizar_disco_tests.sh
	tests/run_actualizar_instala_tests.sh
	tests/run_partir_modelo_tests.sh $(IMG)
	tests/run_hispabios_tests.sh
	tests/run_sys_tests.sh $(IMG)
	tests/run_keyb_tests.sh $(IMG)
	tests/run_usbnative_tests.sh $(IMG)
	tests/run_usbhid_tests.sh $(IMG)
	tests/run_usbhub_tests.sh $(IMG)
	tests/run_usbhc_tests.sh $(IMG) uhci
	tests/run_usbhc_tests.sh $(IMG) ohci
	tests/run_usbhc_tests.sh $(IMG) ehci
	tests/run_usbhc_tests.sh $(IMG) ehcicomp
	tests/run_usbhc_tests.sh $(IMG) xhci
	tests/run_usbhc_tests.sh $(IMG) mezcla
	tests/run_usbhc_tests.sh $(IMG) aparcar
	tests/run_usbauto_tests.sh $(IMG) raton
	tests/run_usbauto_tests.sh $(IMG) sin8042
	tests/run_usbauto_tests.sh $(IMG) ehci
	tests/run_usbauto_tests.sh $(IMG) descr
	tests/run_usbauto_tests.sh $(IMG) arranque
	tests/run_usbauto_tests.sh $(IMG) con8042
	tests/run_usbauto_tests.sh $(IMG) sinaux
	tests/run_usbauto_tests.sh $(IMG) intel
	tests/run_usbauto_tests.sh $(IMG) proxy
	tests/run_joy_tests.sh $(IMG)
	tests/run_instalar_tests.sh $(IMG)
	tests/run_instalar_sonido_tests.sh $(IMG)
	tests/run_cd_tests.sh $(IMG)
	tests/run_cd_tests.sh $(IMG) ahci
	tests/run_edit_tests.sh $(IMG)
	tests/run_deltree_tests.sh $(IMG)
	tests/run_share_tests.sh $(IMG)
	tests/run_mem_tests.sh $(IMG)
	tests/run_memprog_tests.sh $(IMG)
	tests/run_ordenes_tests.sh $(IMG)
	tests/run_recorte_tests.sh $(IMG)
	tests/run_expandir_tests.sh $(IMG)
	tests/run_mediachange_tests.sh $(IMG)
	CDBUS=ahci tests/run_mediachange_tests.sh $(IMG)
	tests/run_usbchange_tests.sh $(IMG)
	tests/run_floppychange_tests.sh $(IMG)
	tests/run_etiqueta_tests.sh $(IMG)
	tests/run_dir_tests.sh $(IMG)
	tests/run_xcopiar_tests.sh $(IMG)
	tests/run_revisar_tests.sh $(IMG)
	tests/run_ayuda_tests.sh $(IMG)
	tests/run_equipo_tests.sh $(IMG)
	tests/run_apagar_tests.sh $(IMG)
	tests/run_velocidad_tests.sh $(IMG)
	tests/run_interprete_tests.sh $(IMG)
	tests/run_criterr_tests.sh $(IMG)
	tests/run_tab_tests.sh $(IMG)
	tests/run_editlinea_tests.sh $(IMG)
	tests/run_mackeys_tests.sh $(IMG)
	tests/run_tecla_tests.sh $(IMG)
	tests/run_nucleo_tests.sh $(IMG)
	tests/run_pila_tests.sh $(IMG)
	tests/run_reloj_tests.sh $(IMG)
	tests/run_logicas_tests.sh $(IMG)
	tests/run_fs_tests.sh $(IMG)
	tests/run_rev_revisar_tests.sh $(IMG)
	tests/run_rev_utilidades_tests.sh $(IMG)
	tests/run_rev_editar_tests.sh $(IMG)
	tests/run_rev_filtros_tests.sh $(IMG)
	tests/run_drivers_tests.sh $(IMG)
	tests/run_sbload_tests.sh $(IMG)
	tests/run_sbjuego_tests.sh $(IMG)
	tests/run_bootfat12_tests.sh
	tests/run_mouse_tests.sh $(IMG)
	tests/run_compat_tests.sh
	tests/run_ventanas_tests.sh $(IMG)
	DOSHIGH=1 tests/run_ventanas_tests.sh $(IMG)
	tests/run_ventanas3_tests.sh $(IMG)
	tests/run_ventanas4_tests.sh $(IMG)
	tests/run_terminal_tests.sh $(IMG)
	tests/run_teclado_tests.sh $(IMG)
	tests/run_teclas_seguidas_tests.sh $(IMG)
	tests/run_pluma_tests.sh $(IMG)
	tests/run_cuentas_tests.sh $(IMG)
	tests/run_facturas_tests.sh $(IMG)
	tests/run_ventanas_bateria_tests.sh $(IMG)
	tests/run_ventanas_red_tests.sh $(IMG)
	tests/run_tamano_tests.sh $(IMG)
	tests/run_contexto_tests.sh $(IMG)
	tests/run_barra_tests.sh $(IMG)
	tests/run_accesos_tests.sh $(IMG)
	tests/run_juegos_tests.sh $(IMG)
	tests/run_juegos2_tests.sh $(IMG)
	tests/run_configuracion_tests.sh $(IMG)
	tests/run_configuracion_red_tests.sh $(IMG)
	tests/run_sorpresa_tests.sh $(IMG)
	tests/run_protector_tests.sh $(IMG)
	tests/run_pintura_tests.sh $(IMG)
	tests/run_sonidos_tests.sh $(IMG)
	tests/run_mezclador_tests.sh $(IMG)
	tests/run_volumen_tests.sh $(IMG)
	tests/run_archivos_tests.sh $(IMG)
	tests/run_hispanes_tests.sh $(IMG)
	tests/run_hispasms_tests.sh $(IMG)
	tests/run_red_tests.sh $(IMG)
	tests/run_yukon_tests.sh
	tests/run_red_usb_tests.sh $(IMG)
	tests/run_consola_tests.sh $(IMG)
	tests/run_actualizar_tests.sh $(IMG)
	tests/run_actualizar_hspred_tests.sh $(IMG)
	EMM=1 tests/run_ventanas_tests.sh $(IMG)
	EMM=1 tests/run_ventanas3_tests.sh $(IMG)

clean:
	rm -rf $(B)

.PHONY: all usb run run-hdd test clean nucprogs emmprogs joyprogs nesprogs smsprogs
