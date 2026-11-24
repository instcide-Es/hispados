# Hispa-8086: el sistema de la familia para 8086, 8088, V20, 80186 y 286.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#   make            los cuatro disquetes (build/hispa8086-NNN.img):
#                     360 K   5¼ doble densidad: el de los PC y XT (el de referencia)
#                     720 K   3½ doble densidad: XT tardíos, portátiles, PS/2 modelo 30
#                     1,2 M   5¼ alta densidad: los AT (286)
#                     1,44 M  3½ alta densidad: 286 tardíos en adelante
#                   y el de Oficina, con las aplicaciones (PLUMA y CUENTAS),
#                   en las mismas cuatro medidas (build/oficina8086-NNN.img)
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

NUCLEO = nucleo.asm disco.inc archivos.inc proceso.inc config.inc dispositivos.inc alta.inc
$(B)/NUCLEO86.SYS: $(NUCLEO) | $(B)
	for f in $(NUCLEO); do $(CP437) $$f $(B)/gen/$$f || exit 1; done
	cd $(B)/gen && $(NASM) -f bin -l nucleo.lst -o ../NUCLEO86.SYS nucleo.asm

$(B)/COMMAND.COM: command.asm | $(B)
	$(CP437) $< $(B)/gen/command.asm && $(NASM) -f bin -o $@ $(B)/gen/command.asm

$(B)/PASAR.COM: ordenes/pasar.asm | $(B)
	$(CP437) $< $(B)/gen/pasar.asm && $(NASM) -f bin -o $@ $(B)/gen/pasar.asm

$(B)/mbr.bin: mbr.asm | $(B)
	$(CP437) $< $(B)/gen/mbr.asm && $(NASM) -f bin -o $@ $(B)/gen/mbr.asm

# las órdenes de disco comparten ordenes/util.inc y llevan dentro el sector de
# arranque y el de particiones (se ensamblan desde la carpeta de los ya
# convertidos, como el núcleo)
$(B)/gen/util.inc: ordenes/util.inc | $(B)
	$(CP437) $< $@

$(B)/FORMATEA.COM $(B)/SISTEMA.COM $(B)/PARTIR.COM: $(B)/gen/util.inc $(B)/arranque.bin $(B)/mbr.bin

$(B)/FORMATEA.COM: ordenes/formatea.asm | $(B)
	$(CP437) $< $(B)/gen/formatea.asm && cd $(B)/gen && $(NASM) -f bin -o ../FORMATEA.COM formatea.asm

$(B)/SISTEMA.COM: ordenes/sistema.asm | $(B)
	$(CP437) $< $(B)/gen/sistema.asm && cd $(B)/gen && $(NASM) -f bin -o ../SISTEMA.COM sistema.asm

$(B)/PARTIR.COM: ordenes/partir.asm | $(B)
	$(CP437) $< $(B)/gen/partir.asm && cd $(B)/gen && $(NASM) -f bin -o ../PARTIR.COM partir.asm

# las órdenes que solo usan de util.inc lo de la pantalla
SUELTAS = ETIQUETA ATRIBUTO ARBOL XCOPIAR REVISAR
define SUELTA
$(B)/$(1).COM: ordenes/$(shell echo $(1) | tr A-Z a-z).asm $(B)/gen/util.inc | $(B)
	$(CP437) $$< $(B)/gen/$(shell echo $(1) | tr A-Z a-z).asm && cd $(B)/gen && $(NASM) -f bin -o ../$(1).COM $(shell echo $(1) | tr A-Z a-z).asm
endef
$(foreach o,$(SUELTAS),$(eval $(call SUELTA,$(o))))

$(B)/EDITAR.COM: ordenes/editar.asm | $(B)
	$(CP437) $< $(B)/gen/editar.asm && $(NASM) -f bin -o $@ $(B)/gen/editar.asm

$(B)/TECLADO.COM: ordenes/teclado.asm | $(B)
	$(CP437) $< $(B)/gen/teclado.asm && $(NASM) -f bin -o $@ $(B)/gen/teclado.asm

$(B)/DISCORAM.SYS: ordenes/discoram.asm | $(B)
	$(CP437) $< $(B)/gen/discoram.asm && $(NASM) -f bin -o $@ $(B)/gen/discoram.asm

$(B)/MAS.COM: ordenes/mas.asm | $(B)
	$(CP437) $< $(B)/gen/mas.asm && $(NASM) -f bin -o $@ $(B)/gen/mas.asm

$(B)/ORDENAR.COM: ordenes/ordenar.asm | $(B)
	$(CP437) $< $(B)/gen/ordenar.asm && $(NASM) -f bin -o $@ $(B)/gen/ordenar.asm

# ---- PLUMA, el procesador de textos (va en el disquete de Oficina) ----
PLUMA_FUENTES = pluma/pluma.asm pluma/texto.inc pluma/pantalla.inc pluma/archivo.inc \
                pluma/imprime.inc pluma/dialogo.inc pluma/letras.inc pluma/variables.inc
$(B)/PLUMA.COM: $(PLUMA_FUENTES) | $(B)
	mkdir -p $(B)/gen/pluma
	for f in $(PLUMA_FUENTES); do $(CP437) $$f $(B)/gen/$$f || exit 1; done
	cd $(B)/gen/pluma && $(NASM) -f bin -l pluma.lst -o ../../PLUMA.COM pluma.asm
$(B)/PLUMA.TXT: pluma/PLUMA.TXT | $(B)
	$(CP437) $< $(B)/gen/pluma.tmp && sed 's/$$/\r/' $(B)/gen/pluma.tmp > $@
OFICINA += $(B)/PLUMA.COM $(B)/PLUMA.TXT
# ---- fin de PLUMA ----


$(B)/HOLA.COM: prueba/hola.asm | $(B)
	$(CP437) $< $(B)/gen/hola.asm && $(NASM) -f bin -o $@ $(B)/gen/hola.asm

$(B)/ARCH.COM: prueba/arch.asm | $(B)
	$(CP437) $< $(B)/gen/arch.asm && $(NASM) -f bin -o $@ $(B)/gen/arch.asm

$(B)/EXE.EXE: prueba/exe.asm | $(B)
	$(CP437) $< $(B)/gen/exe.asm && $(NASM) -f bin -o $@ $(B)/gen/exe.asm

$(B)/N8.COM: prueba/n8.asm | $(B)
	$(CP437) $< $(B)/gen/n8.asm && $(NASM) -f bin -o $@ $(B)/gen/n8.asm

$(B)/XMS.COM: prueba/xms.asm | $(B)
	$(CP437) $< $(B)/gen/xms.asm && $(NASM) -f bin -o $@ $(B)/gen/xms.asm

$(B)/CUENTA.COM: prueba/cuenta.asm | $(B)
	$(CP437) $< $(B)/gen/cuenta.asm && $(NASM) -f bin -o $@ $(B)/gen/cuenta.asm

$(B)/SALUDO.SYS: prueba/saludo.asm | $(B)
	$(CP437) $< $(B)/gen/saludo.asm && $(NASM) -f bin -o $@ $(B)/gen/saludo.asm

$(B)/LEEME.TXT: LEEME.TXT | $(B)
	$(CP437) $< $(B)/gen/leeme.tmp && sed 's/$$/\r/' $(B)/gen/leeme.tmp > $@

ARCHIVOS = $(B)/NUCLEO86.SYS $(B)/COMMAND.COM $(B)/PASAR.COM $(B)/FORMATEA.COM $(B)/SISTEMA.COM $(B)/PARTIR.COM $(B)/MAS.COM $(B)/ORDENAR.COM $(B)/REVISAR.COM $(B)/ETIQUETA.COM $(B)/ATRIBUTO.COM $(B)/ARBOL.COM $(B)/XCOPIAR.COM $(B)/DISCORAM.SYS $(B)/EDITAR.COM $(B)/TECLADO.COM $(B)/BASIC.COM $(B)/HOLA.COM $(B)/ARCH.COM $(B)/EXE.EXE $(B)/LEEME.TXT AUTOEXEC.BAT prueba/LOTE.BAT

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

# ---- las aplicaciones del disquete de Oficina: cada una tiene su bloque y se
# añade a OFICINA (OFICINA += ...); PLUMA, más arriba ----
# CUENTAS, la hoja de cálculo (programas/cuentas.asm y sus programas/cuentas/*.inc)
CUENTAS_INC = $(wildcard programas/cuentas/*.inc)
$(B)/CUENTAS.COM: programas/cuentas.asm $(CUENTAS_INC) | $(B)
	mkdir -p $(B)/gen/cuentas
	for f in $(CUENTAS_INC); do $(CP437) $$f $(B)/gen/cuentas/$$(basename $$f) || exit 1; done
	$(CP437) $< $(B)/gen/cuentas.asm && cd $(B)/gen && $(NASM) -f bin -l cuentas.lst -o ../CUENTAS.COM cuentas.asm
$(B)/GASTOS.CSV: programas/GASTOS.CSV | $(B)
	$(CP437) $< $(B)/gen/gastos.tmp && sed 's/$$/\r/' $(B)/gen/gastos.tmp > $@
OFICINA += $(B)/CUENTAS.COM $(B)/GASTOS.CSV
# (fin de CUENTAS)

# el disquete de Oficina (sin sistema), en las cuatro medidas como el del
# sistema: build/oficina8086-NNN.img. Su regla va después de todos los
# «OFICINA +=», que make lee los requisitos al leer la regla
OFICINAS = $(foreach m,$(MEDIDAS),$(B)/oficina8086-$(m).img)
$(B)/oficina8086-%.img: $(OFICINA)
	rm -f $@
	mformat -C -f $* -v OFICINA -i $@ ::
	mcopy -i $@ $(OFICINA) ::

all: $(OFICINAS)

# lo que solo hace falta para probar (la prueba lo copia a sus disquetes)
PRUEBAS = $(B)/N8.COM $(B)/CUENTA.COM $(B)/SALUDO.SYS $(B)/XMS.COM

# Hispa-BASIC (docs/8086/BASIC.md): varios fuentes; se ensambla desde la
# carpeta de los ya pasados a la página 437, como el núcleo
$(B)/BASIC.COM: basic/basic.asm $(wildcard basic/*.inc) | $(B)
	mkdir -p $(B)/gen/basic
	for f in basic/*.asm basic/*.inc; do $(CP437) $$f $(B)/gen/basic/$$(basename $$f) || exit 1; done
	cd $(B)/gen/basic && $(NASM) -f bin -l basic.lst -o ../../BASIC.COM basic.asm

prueba: $(DISCOS) $(PRUEBAS) $(OFICINAS)
	./prueba/prueba.sh
	./prueba/basic_compara.sh prueba/basic/b0*.bas

clean:
	rm -rf $(B)

.PHONY: all prueba clean
