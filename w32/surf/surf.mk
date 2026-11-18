# [NETSURF] El Navegador completo de Hispa-486 (docs/486/NAVEGADOR2.md):
# NAVEGA2.EXE = NetSurf 3.11 (terceros/netsurf, GPL-2.0, y sus bibliotecas,
# MIT) con el frontal nuestro (w32/surf). Se compila con gcc -m32 sin la
# biblioteca del sistema (w32/surf/inc: lo de MSVCRT.DLL y w32/surf/apoyo.c)
# y se enlaza con tools/elf2pe.py; la red, con el http.c del Navegador propio
# (WSOCK32.DLL y HSPTLS.DLL). Lo incluye el Makefile.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
SURF_T = terceros/netsurf
SURF_LIBS = libcss libparserutils libwapcaplet libnsutils libnsfb libnspsl libsvgtiny libnsgif libnsbmp libhubbub libdom
SURF_SRC := $(shell cat w32/surf/fuentes.txt)
SURF_OBJ = $(patsubst %.c,$(B)/surf/%.o,$(SURF_SRC)) $(B)/surf/http.o $(B)/surf/red_http.o
SURF_CF = -m32 -march=i486 -Os -std=gnu99 -ffreestanding -fbuiltin -nostdinc -isystem $(shell gcc -m32 -print-file-name=include) \
  -Iw32/surf/inc $(foreach l,$(SURF_LIBS),-I$(SURF_T)/$(l)/include) -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables \
  -fno-tree-loop-distribute-patterns -fno-common -ffunction-sections -fdata-sections -DNDEBUG -D_ALIGNED="__attribute__((aligned))" -w
SURF_NSCF = -Iw32/surf -Iw32/surf/ns -I$(SURF_T)/netsurf -I$(SURF_T)/netsurf/include -I$(SURF_T)/netsurf/content/handlers \
  -Iterceros/stb -Dnshispa -Dsmall -DWITH_BMP -DWITH_GIF -DWITH_JPEG -DWITH_PNG -DWITH_NSPSL -DLIBICONV_PLUG -DSTMTEXPR=1 \
  -DNETSURF_HOMEPAGE=\"about:blank\" -DNETSURF_LOG_LEVEL=$(SURF_REGISTRO) -DNETSURF_BUILTIN_LOG_FILTER=\"\" \
  -DNETSURF_BUILTIN_VERBOSE_FILTER=\"\" '-DNETSURF_UA_FORMAT_STRING="HispaSurf/0.1 (%s; NetSurf %d.%d)"'
# (SURF_REGISTRO=INFO para ver en NS.LOG lo que hace NetSurf: ver NAVEGADOR2.md)
SURF_REGISTRO ?= WARNING
SURF_UA = '-DHTTP_AGENTE="HispaSurf/0.1 (Hispa-486; NetSurf 3.11)"'
surf_cf = $(if $(filter $(SURF_T)/netsurf/% w32/surf/%,$1),$(SURF_NSCF),$(if $(filter $(SURF_T)/libdom/%,$1),-I$(SURF_T)/libdom/src \
  -I$(SURF_T)/libdom/bindings,$(if $(filter $(SURF_T)/libnsgif/%,$1),-I$(SURF_T)/libnsgif/src -DNSGIF_NAME=libnsgif,-I$(SURF_T)/$(word 3,$(subst /, ,$1))/src)))
$(B)/surf/%.o: %.c
	@mkdir -p $(dir $@)
	@nice -n 19 gcc $(SURF_CF) $(call surf_cf,$<) -c $< -o $@
$(B)/surf/http.o: w32/navega/http.c w32/navega/nav.h | $(B)
	@mkdir -p $(B)/surf
	$(CC32) -march=i486 $(SURF_UA) -c $< -o $@
$(B)/surf/red_http.o: w32/surf/red_http.c w32/surf/rh.h w32/navega/nav.h | $(B)
	@mkdir -p $(B)/surf
	$(CC32) -march=i486 $(SURF_UA) -c $< -o $@
$(B)/NAVEGA2.EXE: $(SURF_OBJ) $(B)/HSPTLS.DLL $(B)/WSOCK32.DLL $(B)/MSVCRT.DLL ventanas/win32.inc tools/elf2pe.py
	ld -m elf_i386 -r --gc-sections -e inicio_prog -o $(B)/surf/todo.o $(SURF_OBJ)
	python3 tools/elf2pe.py --gui --pila 0x40000 -e inicio_prog -o $@ -l $(B)/HSPTLS.DLL -l $(B)/WSOCK32.DLL -l $(B)/MSVCRT.DLL -t ventanas/win32.inc $(B)/surf/todo.o
# lo que va junto al programa: los recursos (las hojas de estilo de NetSurf, sus
# textos y la página de inicio) y las letras (DejaVu, recortadas: terceros/dejavu)
SURF_RECURSOS = $(SURF_T)/netsurf/resources/default.css $(SURF_T)/netsurf/resources/quirks.css \
  $(SURF_T)/netsurf/resources/adblock.css $(SURF_T)/netsurf/resources/internal.css w32/surf/recursos/MESSAGES w32/surf/recursos/inicio.htm
SURF_FUENTES = $(wildcard terceros/dejavu/*.TTF)
# El disquete «Internet 2»: el Navegador completo (va a \HISPADOS\INTERNET,
# junto a lo del disquete «Internet», que trae las DLL)
$(B)/internet2.img: $(B)/NAVEGA2.EXE $(SURF_RECURSOS) $(SURF_FUENTES) w32/surf/NAVEGA2.INI w32/surf/LEEME.TXT
	rm -f $@
	dd if=/dev/zero of=$@ bs=1474560 count=1 2>/dev/null
	MTOOLS_SKIP_CHECK=1 mformat -i $@ -f 1440 -v INTERNET2 ::
	MTOOLS_SKIP_CHECK=1 mcopy -i $@ $(B)/NAVEGA2.EXE w32/surf/NAVEGA2.INI ::
	MTOOLS_SKIP_CHECK=1 mmd -i $@ ::RECURSOS ::FUENTES
	for f in $(SURF_RECURSOS); do MTOOLS_SKIP_CHECK=1 mcopy -i $@ $$f ::RECURSOS/ || exit 1; done
	for f in $(SURF_FUENTES); do MTOOLS_SKIP_CHECK=1 mcopy -i $@ $$f ::FUENTES/ || exit 1; done
	iconv -f UTF-8 -t CP437 w32/surf/LEEME.TXT | sed 's/$$/\r/' > $(B)/internet2_leeme.txt
	MTOOLS_SKIP_CHECK=1 mcopy -i $@ $(B)/internet2_leeme.txt ::LEEME.TXT
internet2: $(B)/internet2.img
surf: $(B)/NAVEGA2.EXE
