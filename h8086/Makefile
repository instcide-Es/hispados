# Hispa-8086: el sistema de la familia para 8086, 8088, V20, 80186 y 286.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#   make            los cuatro disquetes (build/hispa8086-NNN.img):
#                     360 K   5¼ doble densidad: el de los PC y XT (el de referencia)
#                     720 K   3½ doble densidad: XT tardíos, portátiles, PS/2 modelo 30
#                     1,2 M   5¼ alta densidad: los AT (286)
#                     1,44 M  3½ alta densidad: 286 tardíos en adelante
#   make prueba     arranca los cuatro en QEMU y comprueba lo que sale
#
# Una unidad no lee más densidad que la suya (la de 360 K no lee 1,2 M, ni la
# de 720 K lee 1,44 M), y la BIOS de un XT no sabe de alta densidad. Por eso
# el sistema base tiene que caber SIEMPRE en el disquete de 360 K.
#
# Va aparte del Makefile de Hispa-DOS: son dos núcleos distintos (ver
# docs/8086/HISPA-8086.md). Los fuentes están en UTF-8 y se pasan a la página
# 437 antes de ensamblar, como en el resto del proyecto (y el núcleo se ensambla
# desde la carpeta de los ya convertidos, para que sus %include sean esos).
NASM ?= nasm
B = build
CP437 = python3 ../tools/cp437.py

MEDIDAS = 360 720 1200 1440
DISCOS = $(foreach m,$(MEDIDAS),$(B)/hispa8086-$(m).img)

all: $(DISCOS)

$(B):
	mkdir -p $(B)/gen

$(B)/arranque.bin: arranque.asm | $(B)
	$(CP437) $< $(B)/gen/arranque.asm && $(NASM) -f bin -o $@ $(B)/gen/arranque.asm

NUCLEO = nucleo.asm disco.inc archivos.inc proceso.inc
$(B)/NUCLEO86.SYS: $(NUCLEO) | $(B)
	for f in $(NUCLEO); do $(CP437) $$f $(B)/gen/$$f || exit 1; done
	cd $(B)/gen && $(NASM) -f bin -l nucleo.lst -o ../NUCLEO86.SYS nucleo.asm

$(B)/COMMAND.COM: command.asm | $(B)
	$(CP437) $< $(B)/gen/command.asm && $(NASM) -f bin -o $@ $(B)/gen/command.asm

$(B)/PASAR.COM: ordenes/pasar.asm | $(B)
	$(CP437) $< $(B)/gen/pasar.asm && $(NASM) -f bin -o $@ $(B)/gen/pasar.asm

$(B)/HOLA.COM: prueba/hola.asm | $(B)
	$(CP437) $< $(B)/gen/hola.asm && $(NASM) -f bin -o $@ $(B)/gen/hola.asm

$(B)/ARCH.COM: prueba/arch.asm | $(B)
	$(CP437) $< $(B)/gen/arch.asm && $(NASM) -f bin -o $@ $(B)/gen/arch.asm

$(B)/EXE.EXE: prueba/exe.asm | $(B)
	$(CP437) $< $(B)/gen/exe.asm && $(NASM) -f bin -o $@ $(B)/gen/exe.asm

$(B)/LEEME.TXT: LEEME.TXT | $(B)
	$(CP437) $< $(B)/gen/leeme.tmp && sed 's/$$/\r/' $(B)/gen/leeme.tmp > $@

ARCHIVOS = $(B)/NUCLEO86.SYS $(B)/COMMAND.COM $(B)/PASAR.COM $(B)/HOLA.COM $(B)/ARCH.COM $(B)/EXE.EXE $(B)/LEEME.TXT AUTOEXEC.BAT prueba/LOTE.BAT

# el disquete: se le da formato (mformat rellena la BPB), se le pone nuestro
# sector de arranque conservando esa BPB, y se copian los archivos
define DISQUETE
	rm -f $@
	mformat -C -f $(1) -v HISPA-8086 -i $@ ::
	python3 pon_arranque.py $(B)/arranque.bin $@
	mcopy -i $@ $(ARCHIVOS) ::
endef

$(B)/hispa8086-%.img: $(B)/arranque.bin $(ARCHIVOS) pon_arranque.py
	$(call DISQUETE,$*)

prueba: $(DISCOS)
	./prueba/prueba.sh

clean:
	rm -rf $(B)

.PHONY: all prueba clean
