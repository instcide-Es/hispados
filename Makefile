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
$(B)/KERNEL.SYS: kernel/kernel.asm | $(B)
	$(ASM) $(KFLAGS) -o $@ $(B)/gen/$<
$(B)/COMMAND.COM: shell/command.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/FDISK.COM: utils/fdisk.asm lib/util.inc $(B)/mbr.bin | $(B)
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
$(B)/SBTEST.COM: tests/progs/sbtest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [SB-EOI] KBSB: el teclado mientras suena la SB (tests/run_sbeoi_tests.sh)
$(B)/KBSB.COM: tests/progs/kbsb.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SINIRQ.COM: tests/progs/sinirq.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# [SB-PM] SBPM: lo mismo desde modo protegido por DPMI (tests/run_sbpm_tests.sh)
$(B)/SBPM.COM: tests/progs/sbpm.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/XMSTEST.COM: tests/progs/xmstest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/MCBWALK.COM: tests/progs/mcbwalk.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/LOLTEST.COM: tests/progs/loltest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPSND.COM: drivers/hspsnd.asm lib/hspsb.inc lib/hspopl.inc lib/hspopl_st.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPDPMI.COM: drivers/hspdpmi.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/DPMI16.COM: tests/progs/dpmi16.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/DPMI32.COM: tests/progs/dpmi32.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/PLAYWAV.COM: utils/playwav.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SONIDO.COM: utils/sonido.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
# AUDICION: prueba de sonido con preguntas de sí o no (docs/AUDICION.md)
$(B)/AUDICION.COM: utils/audicion.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/INSTALAR.COM: utils/instalar.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/EDIT.COM: utils/edit.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/GESTOR.COM: utils/gestor.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SYSINFO.COM: utils/sysinfo.asm | $(B)
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
$(B)/XCOPIAR.COM: utils/xcopiar.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/REVISAR.COM: utils/revisar.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/EXPANDIR.COM: utils/expandir.asm lib/util.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPUSB.SYS: drivers/hspusb.asm lib/hidjoyx.inc lib/hidjoyp.inc lib/usb_hc.inc lib/usb_uhci.inc lib/usb_ohci.inc lib/usb_ehci.inc lib/usb_hcini.inc | $(B)
	for f in lib/usb_*.inc; do python3 tools/cp437.py $$f $(B)/gen/$$f || exit 1; done
	$(ASM) -l $(B)/gen/HSPUSB.lst -o $@ $(B)/gen/$<
	@# lo residente no puede usar nada de la inicialización, que se descarta
	@# (las permitidas: solo al arrancar, o con /V o /INFO; ver hspusb.asm)
	python3 tools/initcheck.py $(B)/gen/HSPUSB.lst init,init_end,info_line_go,role_hid,role_joy 'msg_*' || { rm -f $@; exit 1; }
# VELOCIDAD (en el disco VELOCIDA.COM: 8 letras) y LOADFIX: para los juegos
# antiguos que fallan en equipos rápidos o cargados en los primeros 64 KB
$(B)/VELOCIDAD.COM: utils/velocidad.asm lib/util.inc lib/ejecutar.inc | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/LOADFIX.COM: utils/loadfix.asm lib/util.inc lib/ejecutar.inc | $(B)
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
JOYPROGS = $(B)/HIDTEST.COM $(B)/JOYTEST.COM $(B)/HSPJOYT.COM
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
HVPDEP = ventanas/hvp.inc ventanas/hvplib.inc ventanas/defs.inc ventanas/api.inc \
         tools/mkhvp.py tools/icopack.py tools/cp437.py
HVPS = $(B)/CULEBRA.HVP $(B)/MINAS.HVP \
       $(B)/BLOQUES.HVP $(B)/SOLITARI.HVP $(B)/TRAGABOL.HVP $(B)/ALMACEN.HVP \
       $(B)/HISPANES.HVP
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
# ensamblados aquí con tools/asm6502.py (NESROMS: van a \HISPADOS\JUEGOS)
$(B)/HISPANES.HVP: programas/hispanes.asm $(wildcard programas/hispanes/*.inc) $(HVPDEP) | $(B)
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
# [Configuración] los programas de Hispa Ventanas que no son juegos van en
# \HISPADOS, al lado de VENTANAS.COM (HVPSIS): Configuración (CONFIG.HVP)
HVPSIS = $(B)/CONFIG.HVP
$(B)/CONFIG.HVP: programas/config.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# los accesorios (con el 1 en HVP_CABECERA) van en \HISPADOS\PROGS
HVPA = $(B)/PINTURA.HVP $(B)/SONIDOS.HVP
$(B)/PINTURA.HVP: programas/pintura.asm $(HVPDEP) | $(B)
	NASM=$(NASM) python3 tools/mkhvp.py $< $@
# el Reproductor de sonido y los sonidos propios (tools/mksample.py): HISPA.WAV,
# su sonido de prueba, y CAMPANA.WAV, la campanita del volumen de Hispa
# Ventanas (SONS: van a \HISPADOS)
$(B)/SONIDOS.HVP: programas/sonidos.asm $(HVPDEP) | $(B)
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
        $(B)/MOVER.COM $(B)/ATRIBUTO.COM $(B)/ARBOL.COM $(B)/BUSCAR.COM $(B)/ORDENAR.COM $(B)/PAGINAR.COM $(B)/ELEGIR.COM $(B)/EXPANDIR.COM $(B)/ETIQUETA.COM $(B)/XCOPIAR.COM $(B)/REVISAR.COM $(B)/VELOCIDAD.COM $(B)/LOADFIX.COM \
        $(B)/VENTANAS.COM $(HVPS) $(HVPSIS) $(HVPA) $(NESROMS) $(SONS)

$(IMG): $(B)/boot.bin $(FILES) sistema/ALIAS.INI sistema/MANDO.INI sistema/DIAGNOST.BAT $(wildcard tests/disk/*.* tests/disk/*/*.*)
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
	mcopy -i $@ $(B)/XCOPIAR.COM ::HISPADOS/XCOPIAR.COM
	mcopy -i $@ $(B)/REVISAR.COM ::HISPADOS/REVISAR.COM
	mcopy -i $@ $(B)/VELOCIDAD.COM ::HISPADOS/VELOCIDA.COM
	mcopy -i $@ $(B)/LOADFIX.COM ::HISPADOS/LOADFIX.COM
	iconv -f UTF-8 -t CP437 sistema/ALIAS.INI | sed 's/$$/\r/' > $(B)/ALIAS.INI
	mcopy -i $@ $(B)/ALIAS.INI ::HISPADOS/ALIAS.INI
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
	mcopy -i $@ $(B)/VENTANAS.COM ::HISPADOS/VENTANAS.COM
	for f in $(HVPSIS); do mcopy -i $@ $$f ::HISPADOS/ || exit 1; done	# [Configuración]
	for f in $(SONS); do mcopy -i $@ $$f ::HISPADOS/ || exit 1; done	# el sonido de prueba y la campanita
	# los juegos de Hispa Ventanas, en \HISPADOS\JUEGOS
	mmd   -i $@ ::HISPADOS/JUEGOS
	for f in $(HVPS) $(NESROMS); do mcopy -i $@ $$f ::HISPADOS/JUEGOS/ || exit 1; done
	# y los accesorios (Pintura y el Reproductor de sonido), en \HISPADOS\PROGS
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
usb dist: $(IMG) $(B)/boot16.bin $(B)/mbr.bin $(FILES) $(wildcard usb/* floppy/*)
	python3 tools/mkusb.py

run: $(IMG)
	$(QEMU) -rtc base=localtime -drive file=$(IMG),format=raw,if=floppy -boot a

run-hdd: $(IMG) $(HDD)
	$(QEMU) -rtc base=localtime -drive file=$(IMG),format=raw,if=floppy \
	        -drive file=$(HDD),format=raw,if=ide -boot a

# programas de las pruebas del núcleo (tests/run_nucleo_tests.sh los copia
# a una copia de la imagen; no van en el disco de distribución)
# programas de las pruebas de HSPEMM (tests/run_emm_tests.sh)
EMMPROGS = $(B)/EMSTEST.COM $(B)/VCPITEST.COM $(B)/UMBTEST.COM $(B)/EMMMISC.COM $(B)/SBTEST.COM $(B)/SBPM.COM $(B)/KBSB.COM $(B)/SINIRQ.COM
emmprogs: $(EMMPROGS)
NUCPROGS = $(EMMPROGS) $(B)/DPMI16.COM $(B)/DPMI32.COM $(B)/NUCTEST.COM $(B)/NUCHIJO.COM $(B)/NUCEXE.EXE $(B)/NUCBRK.COM $(B)/NUCCON.SYS $(B)/NUCDRV.SYS $(B)/PILTEST.COM $(B)/HMATEST.COM $(B)/HMAUSO.SYS $(B)/MEMTEST.COM $(B)/EXEPILA.EXE $(B)/LEETEST.COM $(B)/VELTEST.COM $(B)/PSPTEST.COM $(B)/PSPSH.COM
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

# y el de las opciones de juego: velocidad, LOADFIX... (tests/run_accesos_tests.sh)
$(B)/ACCJUEGO.COM: tests/progs/accjuego.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<

test: $(IMG) usb nucprogs emmprogs joyprogs nesprogs $(B)/LANZTEST.COM $(B)/ACCJUEGO.COM
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
	tests/run_emm_tests.sh $(IMG)
	tests/run_emmhigh_tests.sh $(IMG)
	tests/run_modojuegos_tests.sh $(IMG)
	EMM=1 tests/run_dpmi_tests.sh $(IMG)
	EMM=1 DOSHIGH=1 tests/run_dpmi_tests.sh $(IMG)
	EMM=1 tests/run_sound_tests.sh $(IMG)
	tests/run_sound_tests.sh $(IMG)
	tests/run_audicion_tests.sh $(IMG)
	tests/run_aprendido_tests.sh $(IMG)
	tests/run_sbemu_tests.sh $(IMG)
	tests/run_sbpm_tests.sh $(IMG)
	tests/run_sbeoi_tests.sh $(IMG)
	tests/run_usb_tests.sh
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
	tests/run_tamano_tests.sh $(IMG)
	tests/run_accesos_tests.sh $(IMG)
	tests/run_juegos_tests.sh $(IMG)
	tests/run_juegos2_tests.sh $(IMG)
	tests/run_configuracion_tests.sh $(IMG)
	tests/run_pintura_tests.sh $(IMG)
	tests/run_sonidos_tests.sh $(IMG)
	tests/run_hispanes_tests.sh $(IMG)
	EMM=1 tests/run_ventanas_tests.sh $(IMG)
	EMM=1 tests/run_ventanas3_tests.sh $(IMG)

clean:
	rm -rf $(B)

.PHONY: all usb run run-hdd test clean nucprogs emmprogs joyprogs nesprogs
