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
$(B)/boot32.bin: boot/boot32.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/mbr.bin: boot/mbr.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/KERNEL.SYS: kernel/kernel.asm lib/version.inc | $(B)
	$(ASM) $(KFLAGS) -o $@ $(B)/gen/$<
$(B)/COMMAND.COM: shell/command.asm lib/version.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/FDISK.COM: utils/fdisk.asm lib/util.inc lib/version.inc $(B)/mbr.bin | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/FORMAT.COM: utils/format.asm lib/util.inc $(B)/boot.bin $(B)/boot16.bin $(B)/boot32.bin | $(B)
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
$(B)/HSPSND.COM: drivers/hspsnd.asm lib/vtd.inc lib/hspsb.inc lib/hspopl.inc lib/hspopl_st.inc lib/hspmez.inc lib/hspmezi.inc lib/cirrus.inc lib/macsim.inc $(B)/gen/lib/alcsim.bin | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPDPMI.COM: drivers/hspdpmi.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/DPMI16.COM: tests/progs/dpmi16.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/DPMI32.COM: tests/progs/dpmi32.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [HISPA-486] HW32: programas Win32 de consola sobre HSPDPMI (docs/486/HISPA-486.md)
$(B)/HW32.COM: utils/hw32.asm utils/hw32k.inc utils/hw32hilo.inc lib/version.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/PEHOLA.EXE: tests/progs/pehola.asm | $(B)
	$(NASM) -f bin -o $@ $<
$(B)/PEFALTA.EXE: tests/progs/pehola.asm | $(B)
	$(NASM) -f bin -DFALTA -o $@ $<
# un Win32 hecho en C (gcc -m32 y tools/elf2pe.py); si no hay gcc de 32 bits,
# la prueba lo salta
CC32 = gcc -m32 -std=gnu11 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -fno-tree-loop-distribute-patterns
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
# [HISPA-486] fase 3d: los controles comunes (barra de estado, pestañas,
# progreso, listas de imágenes); InitCommonControls con su número, el 17
$(B)/COMCTL32.DLL: w32/comctl32.c tools/elf2pe.py ventanas/win32.inc | $(B)
	$(CC32) -c $< -o $(B)/comctl32.o
	python3 tools/elf2pe.py --dll --ordinal InitCommonControls=17 -t ventanas/win32.inc -o $@ $(B)/comctl32.o
# [HISPA-486] fase 3d: el CRT de Visual C++ 2015 (api-ms-win-crt-*.dll,
# VCRUNTIME140.DLL, UCRTBASE.DLL: HW32 los lleva todos a UCRT.DLL): MSVCRT y
# lo del CRT «universal»
$(B)/UCRT.DLL: w32/msvcrt.c w32/ucrt.c tools/elf2pe.py | $(B)
	$(CC32) -c w32/msvcrt.c -o $(B)/ucrt_msvcrt.o
	$(CC32) -c w32/ucrt.c -o $(B)/ucrt.o
	python3 tools/elf2pe.py --dll -o $@ $(B)/ucrt_msvcrt.o $(B)/ucrt.o
# [DX] fase 5 de Hispa-486: DirectX hasta el 7 (sin Direct3D) y WINMM, en C
# (w32/dx/). Se exporta lo global menos memcpy/memset; DDRAW, DSOUND y DINPUT
# usan la mezcla y la hora de WINMM.DLL
DXEXP = $$(nm -g --defined-only $(1) | awk '$$2=="T"{print $$3}' | grep -v '^mem' | paste -sd,)
$(B)/WINMM.DLL: w32/dx/winmm.c w32/dx/dx.h tools/elf2pe.py ventanas/win32.inc | $(B)
	$(CC32) -c $< -o $(B)/winmm.o
	python3 tools/elf2pe.py --dll --exp $(call DXEXP,$(B)/winmm.o)  -t ventanas/win32.inc -o $@ $(B)/winmm.o
$(B)/DDRAW.DLL: w32/dx/ddraw.c w32/dx/dx.h tools/elf2pe.py ventanas/win32.inc $(B)/WINMM.DLL | $(B)
	$(CC32) -c $< -o $(B)/ddraw.o
	python3 tools/elf2pe.py --dll --exp $(call DXEXP,$(B)/ddraw.o) -l $(B)/WINMM.DLL -t ventanas/win32.inc -o $@ $(B)/ddraw.o
$(B)/DSOUND.DLL: w32/dx/dsound.c w32/dx/dx.h tools/elf2pe.py ventanas/win32.inc $(B)/WINMM.DLL | $(B)
	$(CC32) -c $< -o $(B)/dsound.o
	python3 tools/elf2pe.py --dll --exp $(call DXEXP,$(B)/dsound.o) --ordinal DirectSoundCreate=1 --ordinal DirectSoundEnumerateA=2 -l $(B)/WINMM.DLL -t ventanas/win32.inc -o $@ $(B)/dsound.o
$(B)/DINPUT.DLL: w32/dx/dinput.c w32/dx/dx.h tools/elf2pe.py ventanas/win32.inc $(B)/WINMM.DLL | $(B)
	$(CC32) -c $< -o $(B)/dinput.o
	python3 tools/elf2pe.py --dll --exp $(call DXEXP,$(B)/dinput.o) -l $(B)/WINMM.DLL -t ventanas/win32.inc -o $@ $(B)/dinput.o
$(B)/OLE32.DLL: w32/dx/ole32.c w32/dx/dx.h tools/elf2pe.py | $(B)
	$(CC32) -c $< -o $(B)/ole32.o
	python3 tools/elf2pe.py --dll -o $@ $(B)/ole32.o
DXDLL = $(B)/WINMM.DLL $(B)/DDRAW.DLL $(B)/DSOUND.DLL $(B)/DINPUT.DLL $(B)/OLE32.DLL
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
# [HISPA-486] la red de los programas Hispa32 (docs/RED.md): WSOCK32.DLL (la
# pila TCP/IP en C y Winsock 1.1) y su programa de prueba
$(B)/WSOCK32.DLL: w32/wsock32.c tools/elf2pe.py $(B)/MSVCRT.DLL | $(B)
	$(CC32) -march=i486 -Wall -Wextra -c $< -o $(B)/wsock32.o
	python3 tools/elf2pe.py --dll -o $@ -l $(B)/MSVCRT.DLL $(B)/wsock32.o
$(B)/crt0.o: w32/crt0.c | $(B)
	$(CC32) -march=i486 -c $< -o $@
$(B)/REDPRUE.EXE: tests/progs/redprue.c $(B)/crt0.o $(B)/WSOCK32.DLL $(B)/MSVCRT.DLL tools/elf2pe.py | $(B)
	$(CC32) -march=i486 -Wall -c $< -o $(B)/redprue.o
	python3 tools/elf2pe.py -o $@ -l $(B)/WSOCK32.DLL -l $(B)/MSVCRT.DLL $(B)/crt0.o $(B)/redprue.o
# HSPTLS.DLL: TLS 1.2 de cliente (w32/tls/hsptls.c sobre BearSSL, que va en
# terceros/bearssl con su licencia MIT) y HTTPS.EXE, la descarga de consola
BEARCF = -m32 -std=gnu11 -march=i486 -Os -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables \
  -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections \
  -Iw32/inc -Iterceros/bearssl/inc -Iterceros/bearssl/src \
  -DBR_USE_URANDOM=0 -DBR_USE_UNIX_TIME=0 -DBR_USE_WIN32_TIME=0 -DBR_USE_WIN32_RAND=0 -DBR_USE_GETENTROPY=0 \
  -DBR_SSE2=0 -DBR_AES_X86NI=0 -DBR_RDRAND=0 -DBR_LE_UNALIGNED=1 -DBR_64=0 -DBR_INT128=0 -DBR_UMUL128=0 -DBR_POWER8=0
BEARSRC = $(wildcard terceros/bearssl/src/*.c)
BEAROBJ = $(patsubst terceros/bearssl/src/%.c,$(B)/bearssl/%.o,$(BEARSRC))
TLSEXP = TlsAbrir,TlsConectar,TlsLeer,TlsEscribir,TlsCerrar,TlsError,TlsTextoError,TlsInfo,TlsAzar,TlsCiclos
empty :=
space := $(empty) $(empty)
comma := ,
$(B)/bearssl/%.o: terceros/bearssl/src/%.c | $(B)
	@mkdir -p $(B)/bearssl
	gcc $(BEARCF) -c $< -o $@
$(B)/HSPTLS.DLL: w32/tls/hsptls.c w32/tls/anclas.h $(BEAROBJ) $(B)/WSOCK32.DLL $(B)/MSVCRT.DLL tools/elf2pe.py
	gcc $(BEARCF) -Wall -Iw32/tls -c w32/tls/hsptls.c -o $(B)/hsptls.o
	ld -m elf_i386 -r --gc-sections $(addprefix -u ,$(subst $(comma),$(space),$(TLSEXP))) -o $(B)/hsptls_todo.o $(B)/hsptls.o $(BEAROBJ)
	python3 tools/elf2pe.py --dll --exp $(TLSEXP) -o $@ -l $(B)/WSOCK32.DLL -l $(B)/MSVCRT.DLL $(B)/hsptls_todo.o
$(B)/HTTPS.EXE: w32/https.c $(B)/crt0.o $(B)/HSPTLS.DLL $(B)/WSOCK32.DLL $(B)/MSVCRT.DLL tools/elf2pe.py | $(B)
	$(CC32) -march=i486 -Wall -c $< -o $(B)/https.o
	python3 tools/elf2pe.py -o $@ -l $(B)/HSPTLS.DLL -l $(B)/WSOCK32.DLL -l $(B)/MSVCRT.DLL $(B)/crt0.o $(B)/https.o
# El Navegador de Hispa-486 (docs/486/NAVEGADOR.md): programa Hispa32 de ventanas
# [JS] con JavaScript: dom.c (el DOM), script.c (la unión con HSPJS.DLL) y
# dom.js (el preludio, que va dentro del programa como texto)
NAVSRC = w32/navega/navega.c w32/navega/pagina.c w32/navega/html.c w32/navega/http.c w32/navega/imagen.c w32/navega/dom.c w32/navega/script.c
NAVOBJ = $(patsubst w32/navega/%.c,$(B)/nav_%.o,$(NAVSRC)) $(B)/nav_formatos.o
$(B)/nav_%.o: w32/navega/%.c w32/navega/nav.h w32/navega/dom.h | $(B)
	$(CC32) -march=i486 -Wall -Wextra -Wno-parentheses -Wno-sign-compare -c $< -o $@
# las imágenes: el núcleo de Mirador tal cual (programas/mirador, programas/comun)
$(B)/nav_formatos.o: w32/navega/imagen.asm $(wildcard programas/mirador/*.inc) programas/comun/desinfla_t.inc programas/comun/jpeg.inc | $(B)
	$(NASM) -f elf32 -i programas/mirador/ -i programas/comun/ -o $@ $<
$(B)/nav_domjs.h: w32/navega/dom.js | $(B)
	python3 -c "import sys; d = open(sys.argv[1], 'rb').read(); print('static const char dom_js[] = {' + ','.join(str(b) for b in d) + ',0};')" $< > $@
$(B)/nav_script.o: w32/navega/script.c w32/navega/nav.h w32/navega/dom.h $(B)/nav_domjs.h terceros/quickjs/quickjs.h | $(B)
	gcc $(QJSCF) -Iw32/navega -I$(B) -Wall -Wextra -Wno-parentheses -Wno-sign-compare -Wno-unused-parameter -c $< -o $@
$(B)/NAVEGA.EXE: $(NAVOBJ) $(B)/HSPJS.DLL $(B)/HSPTLS.DLL $(B)/WSOCK32.DLL $(B)/MSVCRT.DLL ventanas/win32.inc tools/elf2pe.py
	python3 tools/elf2pe.py --gui --pila 0x30000 -e inicio_prog -o $@ -l $(B)/HSPJS.DLL -l $(B)/HSPTLS.DLL -l $(B)/WSOCK32.DLL -l $(B)/MSVCRT.DLL -t ventanas/win32.inc $(NAVOBJ)
# El segundo disquete de Hispa-486: «Internet» (el Navegador, HTTPS y sus DLL;
# con HW32.COM y MSVCRT.DLL al lado para que se puedan abrir desde él)
INTERNET = $(B)/NAVEGA.EXE $(B)/HSPJS.DLL $(B)/HTTPS.EXE $(B)/HSPTLS.DLL $(B)/WSOCK32.DLL $(B)/MSVCRT.DLL $(B)/HW32.COM
$(B)/internet.img: $(INTERNET) w32/navega/NAVEGA.INI w32/navega/LEEME.TXT
	rm -f $@
	dd if=/dev/zero of=$@ bs=1474560 count=1 2>/dev/null
	MTOOLS_SKIP_CHECK=1 mformat -i $@ -f 1440 -v INTERNET ::
	for f in $(INTERNET) w32/navega/NAVEGA.INI; do MTOOLS_SKIP_CHECK=1 mcopy -i $@ $$f :: || exit 1; done
	iconv -f UTF-8 -t CP437 w32/navega/LEEME.TXT | sed 's/$$/\r/' > $(B)/internet_leeme.txt
	MTOOLS_SKIP_CHECK=1 mcopy -i $@ $(B)/internet_leeme.txt ::LEEME.TXT
internet: $(B)/internet.img
# [NETSURF] el Navegador completo (NetSurf): NAVEGA2.EXE y el disquete «Internet 2»
include w32/surf/surf.mk
# [JS] HSPJS.DLL: JavaScript para los programas Hispa32 (QuickJS, que va en
# terceros/quickjs con su licencia MIT, más w32/js: memoria, hora, Math) y
# JS.EXE, la consola para probarlo. docs/486/NAVEGADOR.md, «JavaScript»
QJSCF = -m32 -march=i486 -O2 -ffreestanding -fbuiltin -nostdinc -isystem $(shell gcc -print-file-name=include) -Iw32/js/inc \
  -Iterceros/quickjs -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-tree-loop-distribute-patterns \
  -fno-common -fwrapv -funsigned-char -ffunction-sections -fdata-sections \
  -DCONFIG_VERSION=\"$(shell cat terceros/quickjs/VERSION)\" -DCONFIG_SIN_HILOS -D_GNU_SOURCE -DNDEBUG
QJSSRC = quickjs libregexp libunicode cutils dtoa
QJSOBJ = $(patsubst %,$(B)/qjs/%.o,$(QJSSRC)) $(B)/qjs/hspjs.o $(B)/qjs/mates.o
$(B)/qjs/%.o: terceros/quickjs/%.c terceros/quickjs/quickjs.h | $(B)
	@mkdir -p $(B)/qjs
	nice -n 19 gcc $(QJSCF) -c $< -o $@
$(B)/qjs/%.o: w32/js/%.c | $(B)
	@mkdir -p $(B)/qjs
	gcc $(QJSCF) -Wall -c $< -o $@
$(B)/HSPJS.DLL: $(QJSOBJ) $(B)/MSVCRT.DLL tools/elf2pe.py
	ld -m elf_i386 -r -o $(B)/qjs/todo0.o $(QJSOBJ)
	nm -g --defined-only $(B)/qjs/todo0.o | awk '$$3 ~ /^(_*JS_|Hj|js_free|js_malloc|js_realloc|js_mallocz|js_strdup)/ {print $$3}' | sort -u > $(B)/qjs/exporta.txt
	ld -m elf_i386 -r --gc-sections -e DllMain $$(sed 's/^/-u /' $(B)/qjs/exporta.txt) -o $(B)/qjs/todo.o $(QJSOBJ)
	python3 tools/elf2pe.py --dll --exp $$(paste -sd, $(B)/qjs/exporta.txt) -o $@ -l $(B)/MSVCRT.DLL $(B)/qjs/todo.o
$(B)/JS.EXE: w32/js/js.c $(B)/crt0.o $(B)/HSPJS.DLL $(B)/MSVCRT.DLL tools/elf2pe.py | $(B)
	gcc $(QJSCF) -Wall -c $< -o $(B)/qjs/js.o
	python3 tools/elf2pe.py -o $@ -l $(B)/HSPJS.DLL -l $(B)/MSVCRT.DLL $(B)/crt0.o $(B)/qjs/js.o
js: $(B)/HSPJS.DLL $(B)/JS.EXE
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
# [DX] fase 5: la prueba de DirectX y WINMM (tests/run_dx_tests.sh)
DXL = -l $(B)/DDRAW.DLL -l $(B)/DSOUND.DLL -l $(B)/DINPUT.DLL -l $(B)/WINMM.DLL
$(B)/WINDX.EXE: tests/progs/windx.c w32/dx/dxapi.h $(DXDLL) $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/windx.o
	python3 tools/elf2pe.py --gui -o $@ $(DXL) $(W32L) $(B)/windx.o
# [DX] «Naves», el juego de ejemplo de Hispa32 (w32/naves/, tests/run_naves_tests.sh)
$(B)/NAVES.EXE: w32/naves/naves.c w32/dx/dxapi.h $(DXDLL) $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/naves.o
	python3 tools/elf2pe.py --gui -o $@ $(DXL) $(W32L) $(B)/naves.o
# los controles de serie (tests/run_win32ctl_tests.sh)
$(B)/WINCTL.EXE: tests/progs/winctl.c $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/winctl.o
	python3 tools/elf2pe.py --gui -o $@ $(W32L) $(B)/winctl.o
# los menús, los recursos (--res: tools/rcmini.py) y los diálogos de plantilla
# (tests/run_win32menu_tests.sh)
$(B)/WINMENU.EXE: tests/progs/winmenu.c tests/progs/winmenu.rc tools/rcmini.py $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/winmenu.o
	python3 tools/elf2pe.py --gui -o $@ --res tests/progs/winmenu.rc $(W32L) $(B)/winmenu.o
# [RUEDA] la rueda del ratón en los programas de Windows (tests/run_rueda_tests.sh)
$(B)/WINRUEDA.EXE: tests/progs/winrueda.c $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/winrueda.o
	python3 tools/elf2pe.py --gui -o $@ $(W32L) $(B)/winrueda.o
# la calculadora: la prueba final de la fase 3c (tests/run_win32calc_tests.sh)
$(B)/WINCALC.EXE: tests/progs/wincalc.c tests/progs/wincalc.rc tools/rcmini.py $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/wincalc.o
	python3 tools/elf2pe.py --gui -o $@ --res tests/progs/wincalc.rc $(W32L) $(B)/wincalc.o
# [HISPA-486] fase 3d: el GDI (tests/run_win32gdi_tests.sh)
$(B)/WINGDI.EXE: tests/progs/wingdi.c tests/progs/win32gdi.h $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/wingdi.o
	python3 tools/elf2pe.py --gui -o $@ $(W32L) $(B)/wingdi.o
# USER32 y COMDLG32 de la fase 3d (tests/run_win32var_tests.sh): los dibujos de
# sus recursos los hace tests/progs/mkwinvar.py (están en el repositorio)
$(B)/WINVAR.EXE: tests/progs/winvar.c tests/progs/winvar.rc tests/progs/win32gdi.h tests/progs/winvar.ico tests/progs/winvar.cur tests/progs/winvar.bmp tools/rcmini.py $(W32DEP) | $(B)
	$(CC32) -c $< -o $(B)/winvar.o
	python3 tools/elf2pe.py --gui -o $@ --res tests/progs/winvar.rc $(W32L) $(B)/winvar.o
$(B)/PLAYWAV.COM: utils/playwav.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SONIDO.COM: utils/sonido.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# AUDICION: prueba de sonido con preguntas de sí o no (docs/AUDICION.md)
$(B)/AUDICION.COM: utils/audicion.asm lib/vtd.inc lib/cirrus.inc lib/macsim.inc $(B)/gen/lib/alcsim.bin | $(B)
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
$(B)/SYS.COM: utils/sys.asm $(B)/boot.bin $(B)/boot16.bin $(B)/boot32.bin | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPRATON.COM: drivers/hspraton.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# Red (docs/RED.md): HSPRED (packet driver residente) y las órdenes CONFRED
# (IPCONFIG), PING y DESCARGAR (en el disco DESCARGA.COM), con la pila TCP/IP
# propia de lib/red.inc
$(B)/HSPRED.COM: drivers/hspred.asm lib/redpref.inc lib/wifi_ids.inc | $(B)
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
# Wi-Fi de Hispa-486 (docs/WIFI.md): HSPWIFI.COM (residente, packet driver),
# su núcleo de 32 bits HSPWIFI.BIN (wifi/: la radio Broadcom con los fuentes
# de brcmsmac y bcma de Linux, 802.11 y WPA2 propios) y la orden WIFI. El
# núcleo es un binario plano enlazado para la dirección 0 (wifi/nucleo/
# nucleo.ld); gcc es solo la herramienta, como con MSVCRT.DLL.
$(B)/HSPWIFI.COM: drivers/hspwifi.asm lib/redpref.inc lib/wifi_ids.inc | $(B)
	$(ASM) -l $(B)/gen/HSPWIFI.lst -o $@ $(B)/gen/$<
	python3 tools/initcheck.py $(B)/gen/HSPWIFI.lst init || { rm -f $@; exit 1; }
$(B)/WIFI.COM: utils/wifi.asm lib/redpref.inc lib/wifi_ids.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
WIFI_INC = -Iwifi/compat -Iwifi/compat/include -Iwifi/linux/include -Iwifi/linux/brcminc \
           -Iwifi/linux/brcmsmac -Iwifi/linux/brcmsmac/phy -Iwifi/linux/bcma -Iwifi/linux -Iwifi/nucleo
WIFI_BASEF = -m32 -std=gnu11 -march=i586 -Os -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables \
          -fno-strict-aliasing -fno-common -mno-80387 -nostdinc -isystem $(shell gcc -m32 -print-file-name=include)
WIFI_CF = $(WIFI_BASEF) $(WIFI_INC) -D__HISPA__ -DCONFIG_BCMA_HOST_PCI -DCONFIG_BCMA_HOST_PCI_POSSIBLE -DCONFIG_BCMA_DRIVER_PCI
# La Atheros AR9002 (ath9k_hw de Linux, wifi/linux/ath/): sus propias rutas
# (sin las de brcmsmac) y objetos con prefijo, porque ath/hw.c y ath9k/hw.c
# se llaman igual.
ATH_INC = -Iwifi/compat -Iwifi/compat/include -Iwifi/linux/include -Iwifi/linux/ath -Iwifi/linux/ath/ath9k \
          -Iwifi/linux -Iwifi/nucleo
ATH_CF = $(WIFI_BASEF) $(ATH_INC) -D__HISPA__ -DCONFIG_ATH_DEBUG -DCONFIG_ATH9K_RFKILL
ATH_SA = $(wildcard wifi/linux/ath/*.c)
ATH_S9 = $(wildcard wifi/linux/ath/ath9k/*.c)
ATH_DEP = $(wildcard wifi/compat/*.h wifi/nucleo/*.h wifi/linux/ath/*.h wifi/linux/ath/ath9k/*.h)
# ath_obj(dir): los objetos de la Atheros en esa carpeta de build
ath_obj = $(patsubst wifi/linux/ath/%.c,$(1)/ath_%.o,$(ATH_SA)) \
          $(patsubst wifi/linux/ath/ath9k/%.c,$(1)/ath9k_%.o,$(ATH_S9)) $(1)/radio_ath9k.o
WIFI_LINUX = $(wildcard wifi/linux/brcmsmac/*.c wifi/linux/brcmsmac/phy/*.c wifi/linux/brcmutil/*.c \
             wifi/linux/bcma/*.c) wifi/linux/cordic.c
WIFI_PROPIO = wifi/compat/base.c $(filter-out wifi/nucleo/radio_ath9k.c,$(wildcard wifi/nucleo/*.c))
WIFI_DEP = $(wildcard tests/wifi/*.h wifi/compat/*.h wifi/nucleo/*.h wifi/linux/brcmsmac/*.h wifi/linux/brcmsmac/phy/*.h \
           wifi/linux/bcma/*.h wifi/linux/include/linux/bcma/*.h)
WIFI_OL = $(patsubst %.c,$(B)/wifi/%.o,$(notdir $(WIFI_LINUX)))
WIFI_OP = $(patsubst %.c,$(B)/wifi/%.o,$(notdir $(WIFI_PROPIO)))
vpath %.c wifi/linux/brcmsmac wifi/linux/brcmsmac/phy wifi/linux/brcmutil wifi/linux/bcma wifi/linux wifi/compat wifi/nucleo
$(B)/wifi:
	mkdir -p $(B)/wifi
# los fuentes de Linux, sin avisos (no son nuestros) salvo los que serían un
# fallo de la capa de compatibilidad; los propios, con todos
$(WIFI_OL): $(B)/wifi/%.o: %.c $(WIFI_DEP) | $(B)/wifi
	nice gcc $(WIFI_CF) -w -Werror=implicit-function-declaration -Werror=int-conversion \
	    -Werror=incompatible-pointer-types -Werror=return-type -c $< -o $@
$(WIFI_OP): $(B)/wifi/%.o: %.c $(WIFI_DEP) | $(B)/wifi
	nice gcc $(WIFI_CF) -Wall -Wno-unused -Wno-packed-not-aligned -c $< -o $@
$(B)/wifi/entrada.o: wifi/nucleo/entrada.asm | $(B)/wifi
	$(NASM) -f elf32 -o $@ $<
$(B)/wifi/ath_%.o: wifi/linux/ath/%.c $(ATH_DEP) | $(B)/wifi
	nice gcc $(ATH_CF) -w -Werror=implicit-function-declaration -Werror=int-conversion \
	    -Werror=incompatible-pointer-types -Werror=return-type -c $< -o $@
$(B)/wifi/ath9k_%.o: wifi/linux/ath/ath9k/%.c $(ATH_DEP) | $(B)/wifi
	nice gcc $(ATH_CF) -w -Werror=implicit-function-declaration -Werror=int-conversion \
	    -Werror=incompatible-pointer-types -Werror=return-type -c $< -o $@
$(B)/wifi/radio_ath9k.o: wifi/nucleo/radio_ath9k.c $(ATH_DEP) | $(B)/wifi
	nice gcc $(ATH_CF) -Wall -Wno-unused -Wno-packed-not-aligned -c $< -o $@
WIFI_OA = $(call ath_obj,$(B)/wifi)
$(B)/HSPWIFI.BIN: $(B)/wifi/entrada.o $(WIFI_OL) $(WIFI_OP) $(WIFI_OA) wifi/nucleo/nucleo.ld
	ld -m elf_i386 --no-warn-rwx-segments -z noexecstack -T wifi/nucleo/nucleo.ld -o $(B)/wifi/nucleo.elf \
	    $(B)/wifi/entrada.o $(WIFI_OL) $(WIFI_OP) $(WIFI_OA)
	objcopy -O binary $(B)/wifi/nucleo.elf $@
# el microcódigo de Broadcom (wifi/firmware/, con su licencia), con nombres 8.3
$(B)/BCM43XX.FW: wifi/firmware/bcm43xx-0.fw | $(B)
	cp $< $@
$(B)/BCM43XXH.FW: wifi/firmware/bcm43xx_hdr-0.fw | $(B)
	cp $< $@
# (la licencia del microcódigo va siempre con él: lo pide la propia licencia)
$(B)/BCM43XX.TXT: wifi/firmware/LICENCE.broadcom_bcm43xx | $(B)
	cp $< $@
WIFI = $(B)/HSPWIFI.COM $(B)/WIFI.COM $(B)/HSPWIFI.BIN $(B)/BCM43XX.FW $(B)/BCM43XXH.FW $(B)/BCM43XX.TXT
# la prueba del controlador de la Broadcom contra un modelo de la tarjeta
# (tests/wifi/chip_sim.c): los mismos fuentes, con -DHL_SIMULA_ES (los
# registros pasan por el modelo) y -DANFITRION, en un programa de Linux de
# 32 bits sin biblioteca
WIFI_SIMF = $(WIFI_CF) -DHL_SIMULA_ES -DANFITRION -O1 -g -Itests/wifi
WIFI_SL = $(patsubst %.c,$(B)/wifisim/%.o,$(notdir $(WIFI_LINUX)))
WIFI_SP = $(patsubst %.c,$(B)/wifisim/%.o,base.c radio_brcm.c w80211.c wpa.c sim.c chip_sim.c tarjeta_sim.c)
vpath %.c tests/wifi
$(B)/wifisim:
	mkdir -p $(B)/wifisim
$(WIFI_SL): $(B)/wifisim/%.o: %.c $(WIFI_DEP) | $(B)/wifisim
	nice gcc $(WIFI_SIMF) -w -c $< -o $@
$(WIFI_SP): $(B)/wifisim/%.o: %.c $(WIFI_DEP) wifi/firmware/bcm43xx-0.fw | $(B)/wifisim
	nice gcc $(WIFI_SIMF) -Wall -Wno-unused -Wno-packed-not-aligned -c $< -o $@
$(B)/chip_sim: $(WIFI_SL) $(WIFI_SP)
	ld -m elf_i386 -static -z noexecstack -o $@ $(WIFI_SL) $(WIFI_SP)
# HSPWSIM.BIN: el núcleo de verdad con el modelo de la tarjeta dentro, para
# probarlo en QEMU (tests/run_wifi_tests.sh lo copia como HSPWIFI.BIN)
WIFI_CHF = $(WIFI_CF) -DHL_SIMULA_ES -DCHIP_SIMULADO -Itests/wifi
WIFI_CL = $(patsubst %.c,$(B)/wifichip/%.o,$(notdir $(WIFI_LINUX)))
WIFI_CP = $(patsubst %.c,$(B)/wifichip/%.o,$(notdir $(WIFI_PROPIO)) tarjeta_sim.c)
$(B)/wifichip:
	mkdir -p $(B)/wifichip
$(WIFI_CL): $(B)/wifichip/%.o: %.c $(WIFI_DEP) | $(B)/wifichip
	nice gcc $(WIFI_CHF) -w -c $< -o $@
$(WIFI_CP): $(B)/wifichip/%.o: %.c $(WIFI_DEP) | $(B)/wifichip
	nice gcc $(WIFI_CHF) -Wall -Wno-unused -Wno-packed-not-aligned -c $< -o $@
# (la Atheros va también, para que enlace; el modelo de la tarjeta es de la Broadcom)
ATH_CHF = $(ATH_CF) -DHL_SIMULA_ES -DCHIP_SIMULADO -Itests/wifi
$(B)/wifichip/ath_%.o: wifi/linux/ath/%.c $(ATH_DEP) | $(B)/wifichip
	nice gcc $(ATH_CHF) -w -c $< -o $@
$(B)/wifichip/ath9k_%.o: wifi/linux/ath/ath9k/%.c $(ATH_DEP) | $(B)/wifichip
	nice gcc $(ATH_CHF) -w -c $< -o $@
$(B)/wifichip/radio_ath9k.o: wifi/nucleo/radio_ath9k.c $(ATH_DEP) | $(B)/wifichip
	nice gcc $(ATH_CHF) -Wall -Wno-unused -Wno-packed-not-aligned -c $< -o $@
WIFI_CA = $(call ath_obj,$(B)/wifichip)
$(B)/HSPWSIM.BIN: $(B)/wifi/entrada.o $(WIFI_CL) $(WIFI_CP) $(WIFI_CA) wifi/nucleo/nucleo.ld
	ld -m elf_i386 --no-warn-rwx-segments -z noexecstack -T wifi/nucleo/nucleo.ld -o $(B)/wifichip/nucleo.elf \
	    $(B)/wifi/entrada.o $(WIFI_CL) $(WIFI_CP) $(WIFI_CA)
	objcopy -O binary $(B)/wifichip/nucleo.elf $@
# la prueba del anfitrión (tests/run_wifi_tests.sh)
$(B)/prueba_wifi: tests/wifi/prueba_wifi.c wifi/nucleo/wpa.c wifi/nucleo/w80211.c wifi/nucleo/sim.c wifi/nucleo/nucleo.h | $(B)
	gcc -std=gnu11 -O2 -Wall -fno-builtin -Iwifi/nucleo -o $@ tests/wifi/prueba_wifi.c wifi/nucleo/wpa.c wifi/nucleo/w80211.c wifi/nucleo/sim.c
# ACPI de Hispa-486 (docs/ACPI.md): la batería de los portátiles que no tienen
# APM. HSPACPI.COM (residente: contesta INT 15h AX=530Ah como una BIOS con
# APM) y su núcleo de 32 bits HSPACPI.BIN: ACPICA de Intel (acpi/acpica/, sin
# modificar salvo el entorno) sobre acpi/nucleo/. Como HSPWIFI.BIN, un binario
# plano enlazado para la dirección 0 que el residente carga en memoria
# extendida. De ACPICA no se compila rsdump.c (solo sirve con su depurador).
$(B)/HSPACPI.COM: drivers/hspacpi.asm | $(B)
	$(ASM) -l $(B)/gen/HSPACPI.lst -o $@ $(B)/gen/$<
	python3 tools/initcheck.py $(B)/gen/HSPACPI.lst init || { rm -f $@; exit 1; }
ACPI_BASEF = -m32 -std=gnu11 -march=i586 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables \
             -D__HISPA__ -Iacpi/acpica/include -Iacpi/nucleo
# (los textos del registro, en la página 437, que es como los enseña DOS)
ACPI_CF = $(ACPI_BASEF) -Os -ffunction-sections -fdata-sections -fexec-charset=CP437
ACPI_SA = $(filter-out acpi/acpica/resources/rsdump.c,$(wildcard acpi/acpica/*/*.c))
ACPI_SP = $(addprefix acpi/nucleo/,base.c osl.c ec.c nucleo.c bateria.c)
ACPI_DEP = $(wildcard acpi/nucleo/*.h acpi/acpica/include/*.h acpi/acpica/include/platform/*.h tests/acpi/*.h)
acpi_obj = $(patsubst %.c,$(1)/%.o,$(notdir $(2)))
# $(1) fuente, $(2) carpeta de objetos, $(3) opciones
define ACPI_REGLA
$(2)/$(notdir $(basename $(1))).o: $(1) $$(ACPI_DEP) | $(2)
	nice gcc $(3) -c $$< -o $$@
endef
$(B)/acpi $(B)/acpisim:
	mkdir -p $@
$(foreach f,$(ACPI_SA),$(eval $(call ACPI_REGLA,$(f),$(B)/acpi,$$(ACPI_CF) -w)))
$(foreach f,$(ACPI_SP) acpi/nucleo/maquina.c,$(eval $(call ACPI_REGLA,$(f),$(B)/acpi,$$(ACPI_CF) -Wall -Wno-unused)))
$(B)/acpi/entrada.o: acpi/nucleo/entrada.asm | $(B)/acpi
	$(NASM) -f elf32 -o $@ $<
$(B)/acpi/tarea.o: acpi/nucleo/tarea.asm | $(B)/acpi
	$(NASM) -f elf32 -o $@ $<
ACPI_O = $(call acpi_obj,$(B)/acpi,$(ACPI_SP) acpi/nucleo/maquina.c) $(B)/acpi/tarea.o $(call acpi_obj,$(B)/acpi,$(ACPI_SA))
$(B)/HSPACPI.BIN: $(B)/acpi/entrada.o $(ACPI_O) acpi/nucleo/nucleo.ld
	ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T acpi/nucleo/nucleo.ld -o $(B)/acpi/nucleo.elf \
	    $(B)/acpi/entrada.o $(ACPI_O)
	objcopy -O binary $(B)/acpi/nucleo.elf $@
ACPI = $(B)/HSPACPI.COM $(B)/HSPACPI.BIN
# El mismo núcleo, para el equipo de compilación, con un portátil de mentira
# debajo (tests/acpi/: las tablas ACPI volcadas del Toshiba NB250 y del HP
# Mini 5103 y un controlador embebido simulado): tests/run_acpi_tests.sh
ACPI_SIMF = $(ACPI_BASEF) -DANFITRION -O1 -g -Itests/acpi
ACPI_SS = tests/acpi/maquina_sim.c tests/acpi/anfitrion.c
$(foreach f,$(ACPI_SA),$(eval $(call ACPI_REGLA,$(f),$(B)/acpisim,$$(ACPI_SIMF) -w)))
$(foreach f,$(ACPI_SP) $(ACPI_SS),$(eval $(call ACPI_REGLA,$(f),$(B)/acpisim,$$(ACPI_SIMF) -Wall -Wno-unused)))
$(B)/acpisim/tarea.o: acpi/nucleo/tarea.asm | $(B)/acpisim
	$(NASM) -f elf32 -o $@ $<
ACPI_SO = $(call acpi_obj,$(B)/acpisim,$(ACPI_SP) $(ACPI_SS) $(ACPI_SA)) $(B)/acpisim/tarea.o
$(B)/acpi_sim: $(ACPI_SO)
	ld -m elf_i386 -static -z noexecstack -o $@ $(ACPI_SO)
# (pruebas) escribe en la CMOS: la batería de mentira de QEMU (tests/acpi/bateria-qemu.asl)
$(B)/CMOSW.COM: tests/progs/cmosw.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [VTD] (pruebas) enciende la protección de DMA de QEMU, como un firmware UEFI (tests/run_vtd_tests.sh)
$(B)/VTDON.COM: tests/progs/vtdon.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/FSTEST.COM: tests/progs/fstest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SHARETST.COM: tests/progs/sharetst.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/TECLAS.COM: tests/progs/teclas.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/MOUSETST.COM: tests/progs/mousetst.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [RUEDA] el touchpad Synaptics en modo absoluto, sin touchpad (tests/run_rueda_tests.sh)
$(B)/SYNTEST.COM: tests/progs/syntest.asm | $(B)
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
$(B)/REVISAR.COM: utils/revisar.asm utils/revisar32.inc lib/util.inc | $(B)
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
$(B)/VENTANAS.COM: $(VENSRC) lib/wifi_ids.inc ventanas/fuente.txt tools/mkfuente.py tools/hispa437.py tools/cp437.py tools/icopack.py | $(B)
	for f in $(VENSRC); do python3 tools/cp437.py --ventanas $$f $(B)/gen/$$f || exit 1; done
	python3 tools/mkfuente.py ventanas/fuente.txt $(B)/gen/ventanas/fuente.inc $(B)/gen/ventanas/fuente.ppm
	python3 tools/icopack.py $(B)/gen/ventanas/iconos.inc $(B)/gen/ventanas/iconos2.inc $(B)/gen/ventanas/apagado.inc
	# (lo que usa la parte de modo real tiene que caber en 64 KB: si no, error)
	# (-w-zeroing: las variables de la parte de modo real van en el archivo, a cero)
	$(NASM) -f bin -w+error=number-overflow -w-zeroing -i $(B)/gen/ventanas/ -i lib/ -o $@ $(B)/gen/ventanas/ventanas.asm

# Programas de Hispa Ventanas (.HVP, docs/VENTANAS.md «Programas aparte»): los
# juegos, en juegos/ (HVPS: van a \HISPADOS\JUEGOS), y los accesorios, en
# programas/ (HVPA: a \HISPADOS\PROGS). tools/mkhvp.py los ensambla y
# saca las reubicaciones.
HVPDEP = ventanas/hvp.inc ventanas/hvplib.inc ventanas/defs.inc ventanas/api.inc ventanas/hvpmez.inc \
         tools/mkhvp.py tools/icopack.py tools/cp437.py tools/hispa437.py
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
	# la campanita del volumen; el sonido de prueba (HISPA.WAV, 103 K) va en el
	# disquete «Instalación 2» (tools/mkusb.py): sitio libre en este
	mcopy -i $@ $(B)/CAMPANA.WAV ::HISPADOS/CAMPANA.WAV
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
HBEFI = $(B)/BOOTX64.EFI $(B)/BOOTIA32.EFI $(B)/HBLOG.COM
# HBLOG.COM: enseña y guarda (\HBLOG.TXT) el registro de la BIOS de HispaBIOS
$(B)/HBLOG.COM: hispabios/hblog.asm | $(B)
	$(NASM) -f bin -o $@ hispabios/hblog.asm
$(B)/FUENTE16.BIN: $(B)/VENTANAS.COM tools/mkfuentebios.py tools/hispa437.py
	python3 tools/mkfuentebios.py $(B)/gen/ventanas/fuente.inc $@
$(B)/BIOS64.BIN: $(HBSRC) | $(B)
	$(NASM) -f bin $(HBFLAGS) -I hispabios/ -o $@ hispabios/bios64.asm
$(B)/BIOS32.BIN: $(HBSRC) | $(B)
	$(NASM) -f bin -DARQ32 $(HBFLAGS) -I hispabios/ -o $@ hispabios/bios64.asm
# La BIOS de modo real va en 90000h; si el firmware del equipo usa esa zona,
# en 70000h o 60000h: una copia ensamblada para cada sitio (_7, _6). En 80000h
# no: ahí se copia el sector de arranque de Hispa-DOS.
$(B)/BIOS64_%.BIN: $(HBSRC) | $(B)
	$(NASM) -f bin -DBASE=$*0000h $(HBFLAGS) -I hispabios/ -o $@ hispabios/bios64.asm
$(B)/BIOS32_%.BIN: $(HBSRC) | $(B)
	$(NASM) -f bin -DARQ32 -DBASE=$*0000h $(HBFLAGS) -I hispabios/ -o $@ hispabios/bios64.asm
HBBIOS64 = $(B)/BIOS64.BIN $(B)/BIOS64_7.BIN $(B)/BIOS64_6.BIN
HBBIOS32 = $(B)/BIOS32.BIN $(B)/BIOS32_7.BIN $(B)/BIOS32_6.BIN
$(B)/BOOTX64.EFI: hispabios/efi64.asm hispabios/efi64_raton.inc hispabios/efi64_tecl.inc hispabios/pe.inc $(HBBIOS64) $(B)/FUENTE16.BIN
	$(NASM) -f bin -I hispabios/ -I $(B)/ -o $@ hispabios/efi64.asm
HBEFI32 = hispabios/efi32.asm hispabios/efi32_raton.inc hispabios/efi32_tecl.inc hispabios/efi32_reloj.inc hispabios/efi32_pant.inc hispabios/efi32_datos.inc
$(B)/BOOTIA32.EFI: $(HBEFI32) hispabios/pe.inc $(HBBIOS32) $(B)/FUENTE16.BIN
	$(NASM) -f bin -I hispabios/ -I $(B)/ -o $@ hispabios/efi32.asm
hispabios: $(HBEFI)

usb dist: $(IMG) $(B)/boot16.bin $(B)/mbr.bin $(FILES) $(HBEFI) $(HVPO) $(PROGSDAT) $(B)/HW32.COM $(B)/MSVCRT.DLL $(WIFI) $(ACPI) $(wildcard usb/* floppy/*)
	HVPA="$(notdir $(HVPA) $(HVPO))" HVPO="$(notdir $(HVPO))" PROGSDAT="$(PROGSDAT)" python3 tools/mkusb.py

# [HISPA-486] la imagen USB de Hispa-486 (build/hispa486-usb.img) y su disquete
# «Hispa-486» (build/hispa486-disquete.img, el último de la instalación desde
# disquetes): lo de Hispa-386 más el escritorio de Hispa-486 (la clave de
# VENTANAS.INI y el AUTOEXEC.BAT que lo arranca), el Centro de programas con
# el grupo «Hispa32», la edición (EDICION.INI), los programas de 32 bits de
# ejemplo en \HISPA32, el Navegador en \HISPADOS\INTERNET (lo del disquete
# «Internet») y la red puesta sola al arrancar (HSPRED y HSPWIFI /AUTO). Todo por tools/mkusb.py (docs/486/HISPA-486.md, «La
# imagen»). Hacen falta gcc -m32 (los programas en C) y mtools.
H32EXE = $(B)/WINCALC.EXE $(B)/WINHOLA.EXE $(B)/WINCTL.EXE $(B)/WINMENU.EXE $(B)/WINGUI.EXE $(B)/NAVES.EXE
usb486: $(IMG) $(B)/boot16.bin $(B)/mbr.bin $(FILES) $(HBEFI) $(HVPO) $(PROGSDAT) $(B)/HW32.COM $(B)/MSVCRT.DLL $(WIFI) $(ACPI) $(B)/COMCTL32.DLL $(B)/UCRT.DLL $(DXDLL) $(H32EXE) $(INTERNET) $(B)/internet.img w32/navega/NAVEGA.INI w32/navega/LEEME.TXT $(wildcard usb/* floppy/* sistema/486/*)
	HVPA="$(notdir $(HVPA) $(HVPO))" HVPO="$(notdir $(HVPO))" PROGSDAT="$(PROGSDAT)" python3 tools/mkusb.py 486

# [CD] las imágenes de CD (docs/CD.md): lo mismo que la imagen USB de cada
# gama, en ISO 9660 nivel 1 + Joliet, con arranque El Torito en emulación de
# disquete de 1,44 MB (ARRANQUE.IMG: el disquete de arranque del CD, que busca
# el CD con HSPCD y lo usa todo de allí). Hace falta genisoimage o mkisofs.
cd: usb tools/mkcd.py cd/LEEMECD.TXT
	python3 tools/mkcd.py
cd486: usb486 tools/mkcd.py cd/LEEMECD.TXT
	python3 tools/mkcd.py 486

# [FONDO] la imagen de fondo del escritorio de Hispa-486 (sistema/486/FONDO.BMP,
# que va en el repositorio ya hecha): 800x600 con los colores fijos de Hispa
# Ventanas y difuminado, a partir de sistema/486/HISPADOS.JPG (la imagen es de
# Carlos Sanz). Solo hace falta repetirlo si cambia la foto (docs/VENTANAS.md,
# «La imagen de fondo»; para leer el JPEG: Pillow, ImageMagick, ffmpeg o djpeg)
fondo486:
	python3 tools/mkfondo.py sistema/486/HISPADOS.JPG sistema/486/FONDO.BMP

# [HISPA-486] los paquetes de ACTUALIZAR (docs/RED.md, «Formato del paquete
# .PAQ»), hechos de la imagen USB de cada edición (sin INSTALL.BAT ni \EFI, que
# en un disco duro no hacen falta): build/hispados.paq (Hispa-386) y
# build/hispa486.paq (Hispa-486, con \HISPA32 y EDICION.INI). En
# versiones.txt, el de Hispa-486 va en una sección con «gama=486».
paq: usb
	rm -rf $(B)/paq386 && mkdir -p $(B)/paq386
	MTOOLS_SKIP_CHECK=1 mcopy -s -n -i $(B)/hispados-usb.img@@1M ::/ $(B)/paq386/
	rm -rf $(B)/paq386/INSTALL.BAT $(B)/paq386/EFI
	python3 -I tools/paquete.py $(B)/paq386 $(B)/hispados.paq
paq486: usb486
	rm -rf $(B)/paq486 && mkdir -p $(B)/paq486
	MTOOLS_SKIP_CHECK=1 mcopy -s -n -i $(B)/hispa486-usb.img@@1M ::/ $(B)/paq486/
	rm -rf $(B)/paq486/INSTALL.BAT $(B)/paq486/EFI
	python3 -I tools/paquete.py $(B)/paq486 $(B)/hispa486.paq

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

# Lo que necesitan las pruebas (también lo usa tools/pruebas_pc.sh).
TEST_DEPS = $(IMG) usb usb486 cd cd486 nucprogs emmprogs joyprogs nesprogs smsprogs $(B)/LANZTEST.COM $(B)/ACCJUEGO.COM $(B)/prueba_wifi $(B)/chip_sim $(B)/HSPWSIM.BIN $(ACPI) $(B)/acpi_sim $(B)/CMOSW.COM $(B)/VTDON.COM

para-pruebas: $(TEST_DEPS)

test: $(TEST_DEPS)
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
	tests/run_win32gdi_tests.sh $(IMG)
	tests/run_win32var_tests.sh $(IMG)
	tests/run_win32fasmw_tests.sh $(IMG)
	tests/run_dx_tests.sh $(IMG)
	tests/run_naves_tests.sh $(IMG)
	tests/run_dxterceros_tests.sh $(IMG)
	tests/run_emm_tests.sh $(IMG)
	tests/run_emmhigh_tests.sh $(IMG)
	tests/run_modojuegos_tests.sh $(IMG)
	tests/run_memjuegos_tests.sh $(IMG)
	EMM=1 tests/run_dpmi_tests.sh $(IMG)
	EMM=1 DOSHIGH=1 tests/run_dpmi_tests.sh $(IMG)
	EMM=1 tests/run_sound_tests.sh $(IMG)
	tests/run_sound_tests.sh $(IMG)
	tests/run_audicion_tests.sh $(IMG)
	tests/run_vtd_tests.sh $(IMG)
	tests/run_aprendido_tests.sh $(IMG)
	tests/run_sbemu_tests.sh $(IMG)
	tests/run_sbpm_tests.sh $(IMG)
	tests/run_sbeoi_tests.sh $(IMG)
	tests/run_sbvga_tests.sh $(IMG)
	tests/run_usb_tests.sh
	tests/run_instalar_usb_tests.sh
	tests/run_imagen486_tests.sh
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
	tests/run_cd_imagen_tests.sh
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
	tests/run_fat32_tests.sh $(IMG)
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
	tests/run_fondo_tests.sh $(IMG)
	tests/run_resolucion_tests.sh $(IMG)
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
	tests/run_red32_tests.sh $(IMG)
	tests/run_navegador_tests.sh $(IMG)
	tests/run_navegador_js_tests.sh $(IMG)
	tests/run_navegador2_tests.sh $(IMG)      # [NETSURF] el Navegador completo
	tests/run_js_tests.sh $(IMG)
	tests/run_red_usb_tests.sh $(IMG)
	tests/run_wifi_tests.sh $(IMG)
	tests/run_acpi_tests.sh $(IMG)
	tests/run_wifi_ventana_tests.sh $(IMG)
	tests/run_consola_tests.sh $(IMG)
	tests/run_actualizar_tests.sh $(IMG)
	tests/run_actualizar_hspred_tests.sh $(IMG)
	EMM=1 tests/run_ventanas_tests.sh $(IMG)
	EMM=1 tests/run_ventanas3_tests.sh $(IMG)

clean:
	rm -rf $(B)

PHONY_CD = cd cd486
.PHONY: all usb usb486 $(PHONY_CD) fondo486 paq paq486 run run-hdd test para-pruebas clean nucprogs emmprogs joyprogs nesprogs smsprogs
