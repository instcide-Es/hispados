# =============================================================================
#  navega.mk - NAVEGA.BIN, el Navegador de Hispa-Pentium (docs/PENTIUM.md,
#  «El Navegador»): NetSurf 3.11 (terceros/netsurf, GPL-2.0, y sus
#  bibliotecas, MIT) con el frontal de Hispa-Pentium (frontal.c), QuickJS
#  (terceros/quickjs, MIT) para el JavaScript y la biblioteca de C de
#  Hispa-486 (w32/msvcrt.c) sobre las llamadas del núcleo (sistema.c), todo
#  junto en un programa del anillo 3 (cabecera HPU3, enlazado para
#  0x40000000), como TLS.BIN.
#  Lo mismo que el Navegador completo de Hispa-486 (w32/surf, surf.mk): los
#  mismos fuentes, sin tocar, menos surf.c (la ventana de Hispa Ventanas),
#  que aquí es frontal.c. Lo llama hacer.sh (make -f, sin -j).
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# =============================================================================
O ?= build/navega-pentium
AQUI = castel/nucleo32/navega
SURF_T = terceros/netsurf
SURF_LIBS = libcss libparserutils libwapcaplet libnsutils libnsfb libnspsl libsvgtiny libnsgif libnsbmp libhubbub libdom
# los fuentes del Navegador completo, menos su ventana (surf.c)
SURF_SRC := $(filter-out w32/surf/surf.c,$(shell cat w32/surf/fuentes.txt)) $(AQUI)/frontal.c $(AQUI)/sistema.c $(AQUI)/descarga.c
SURF_OBJ = $(patsubst %.c,$(O)/surf/%.o,$(SURF_SRC)) $(O)/surf/http.o $(O)/surf/red_http.o
GCCINC := $(shell gcc -m32 -print-file-name=include)
SURF_CF = -m32 -march=i586 -Os -std=gnu99 -ffreestanding -fbuiltin -nostdinc -isystem $(GCCINC) -DHISPA_PENTIUM \
  -Iw32/surf/inc $(foreach l,$(SURF_LIBS),-I$(SURF_T)/$(l)/include) -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables \
  -fno-tree-loop-distribute-patterns -fno-common -ffunction-sections -fdata-sections -DNDEBUG -D_ALIGNED="__attribute__((aligned))" -w
SURF_NSCF = -Iw32/surf -Iw32/surf/ns -I$(SURF_T)/netsurf -I$(SURF_T)/netsurf/include -I$(SURF_T)/netsurf/content/handlers \
  -Iterceros/stb -Dnshispa -Dsmall -DWITH_BMP -DWITH_GIF -DWITH_JPEG -DWITH_PNG -DWITH_NSPSL -DLIBICONV_PLUG -DSTMTEXPR=1 \
  -DNETSURF_HOMEPAGE=\"about:blank\" -DNETSURF_LOG_LEVEL=$(SURF_REGISTRO) -DNETSURF_BUILTIN_LOG_FILTER=\"\" \
  -DNETSURF_BUILTIN_VERBOSE_FILTER=\"\" '-DNETSURF_UA_FORMAT_STRING="HispaSurf/0.1 (%s; NetSurf %d.%d)"'
SURF_REGISTRO ?= WARNING
# el que se presenta a los servidores: sin «HispaNavegador», «(Hispa-486» ni
# «Hispa32» (ClicGest daría su web ligera: NAVEGADOR2.md, «La red»)
SURF_UA = '-DHTTP_AGENTE="HispaSurf/0.2 (NetSurf 3.11; Hispa-Pentium)"'
SURF_BRCF = -Iterceros/brotli/c/include -DBROTLI_EXTERNAL_DICTIONARY_DATA
surf_cf = $(if $(filter terceros/brotli/%,$1),$(SURF_BRCF),$(if $(filter w32/surf/guion.c $(AQUI)/frontal.c,$1),$(SURF_NSCF) -Iterceros/quickjs -I$(O)/surf,$(if $(filter $(SURF_T)/netsurf/% w32/surf/% $(AQUI)/%,$1),$(SURF_NSCF),$(if $(filter $(SURF_T)/libdom/%,$1),-I$(SURF_T)/libdom/src \
  -I$(SURF_T)/libdom/bindings,$(if $(filter $(SURF_T)/libnsgif/%,$1),-I$(SURF_T)/libnsgif/src -DNSGIF_NAME=libnsgif,-I$(SURF_T)/$(word 3,$(subst /, ,$1))/src))))) \
  $(if $(filter w32/surf/woff.c,$1),$(SURF_BRCF))
$(O)/surf/%.o: %.c
	@mkdir -p $(dir $@)
	@nice -n 19 gcc $(SURF_CF) $(call surf_cf,$<) -c $< -o $@
$(O)/surf/surf_js.h: w32/navega/dom.js w32/surf/surf.js
	@mkdir -p $(O)/surf
	python3 -c "import sys; f = lambda n, a: 'static const char %s[] = {' % n + ','.join(str(b) for b in open(a, 'rb').read()) + ',0};'; print(f('dom_js', sys.argv[1])); print(f('surf_js', sys.argv[2]))" $^ > $@
$(O)/surf/w32/surf/guion.o: $(O)/surf/surf_js.h w32/surf/guion.h terceros/quickjs/quickjs.h
$(O)/surf/w32/surf/selector.o $(O)/surf/w32/surf/rehaz.o: w32/surf/guion.h
$(O)/surf/$(AQUI)/frontal.o $(O)/surf/$(AQUI)/descarga.o: $(AQUI)/frontal.h
$(O)/surf/$(AQUI)/sistema.o: $(AQUI)/pentium.h
$(O)/surf/$(AQUI)/frontal.o: $(AQUI)/pentium.h
CC32 = gcc -m32 -march=i586 -std=gnu11 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin \
  -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -DHISPA_PENTIUM
$(O)/surf/http.o: w32/navega/http.c w32/navega/nav.h $(AQUI)/navega.mk
	@mkdir -p $(O)/surf
	$(CC32) $(SURF_UA) -c $< -o $@
$(O)/surf/red_http.o: w32/surf/red_http.c w32/surf/rh.h w32/navega/nav.h $(AQUI)/navega.mk
	@mkdir -p $(O)/surf
	$(CC32) $(SURF_UA) -c $< -o $@

# ---- la biblioteca de C (la MSVCRT.DLL de Hispa-486, tal cual): su malloc,
# con otro nombre (el de todos es el de w32/surf/memoria.c; su HeapAlloc
# también, en sistema.c); DllMain, msvcrt_arranca ----
$(O)/crt.o: w32/msvcrt.c $(AQUI)/navega.mk
	@mkdir -p $(O)
	$(CC32) -Dmalloc=crt_malloc -Dfree=crt_free -Dcalloc=crt_calloc -Drealloc=crt_realloc -DDllMain=msvcrt_arranca -c $< -o $@

# ---- QuickJS (HSPJS.DLL de Hispa-486): como en la DLL, solo se ve desde
# fuera lo que exporta (JS_..., Hj..., js_malloc...); lo suyo (su malloc,
# su fprintf, sus divisiones de 64 bits) queda dentro ----
QJSCF = -m32 -march=i586 -O2 -ffreestanding -fbuiltin -nostdinc -isystem $(GCCINC) -Iw32/js/inc \
  -Iterceros/quickjs -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-tree-loop-distribute-patterns \
  -fno-common -fwrapv -funsigned-char -ffunction-sections -fdata-sections \
  -DCONFIG_VERSION=\"$(shell cat terceros/quickjs/VERSION)\" -DCONFIG_SIN_HILOS -D_GNU_SOURCE -DNDEBUG
QJSSRC = quickjs libregexp libunicode cutils dtoa
QJSOBJ = $(patsubst %,$(O)/qjs/%.o,$(QJSSRC)) $(O)/qjs/hspjs.o $(O)/qjs/mates.o
$(O)/qjs/%.o: terceros/quickjs/%.c terceros/quickjs/quickjs.h
	@mkdir -p $(O)/qjs
	nice -n 19 gcc $(QJSCF) -c $< -o $@
$(O)/qjs/hspjs.o: w32/js/hspjs.c
	@mkdir -p $(O)/qjs
	gcc $(QJSCF) -DDllMain=hspjs_arranca -c $< -o $@
$(O)/qjs/mates.o: w32/js/mates.c
	@mkdir -p $(O)/qjs
	gcc $(QJSCF) -c $< -o $@
$(O)/qjs.o: $(QJSOBJ)
	ld -m elf_i386 -r -o $(O)/qjs/todo0.o $(QJSOBJ)
	nm -g --defined-only $(O)/qjs/todo0.o | awk '$$3 ~ /^(_*JS_|Hj|hspjs_arranca|js_free|js_malloc|js_realloc|js_mallocz|js_strdup)/ || $$3 == "__udivmoddi4" {print $$3}' | sort -u > $(O)/qjs/exporta.txt
	objcopy --keep-global-symbols=$(O)/qjs/exporta.txt $(O)/qjs/todo0.o $@

# ---- la entrada (la cabecera HPU3) y el programa entero ----
$(O)/entrada.o: $(AQUI)/entrada.asm
	@mkdir -p $(O)
	nasm -f elf32 $< -o $@
$(O)/NAVEGA.BIN: $(O)/entrada.o $(SURF_OBJ) $(O)/crt.o $(O)/qjs.o $(AQUI)/pentium.ld
	ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $(AQUI)/pentium.ld -Map $(O)/navega.map -o $(O)/navega.elf \
	  $(O)/entrada.o $(SURF_OBJ) $(O)/crt.o $(O)/qjs.o
	objcopy -O binary $(O)/navega.elf $@
# X87.BIN: la prueba de que cada programa tiene su x87 (prueba_x87.c, castel/pruebas.sh)
$(O)/X87.BIN: $(O)/entrada.o $(AQUI)/prueba_x87.c $(AQUI)/pentium.h $(AQUI)/pentium.ld
	$(CC32) -Wall -c $(AQUI)/prueba_x87.c -o $(O)/prueba_x87.o
	ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $(AQUI)/pentium.ld -o $(O)/x87.elf $(O)/entrada.o $(O)/prueba_x87.o
	objcopy -O binary $(O)/x87.elf $@
navega: $(O)/NAVEGA.BIN
.PHONY: navega
