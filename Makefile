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
$(B)/XMSTEST.COM: tests/progs/xmstest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/MCBWALK.COM: tests/progs/mcbwalk.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/LOLTEST.COM: tests/progs/loltest.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPSND.COM: drivers/hspsnd.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/PLAYWAV.COM: utils/playwav.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/INSTALAR.COM: utils/instalar.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/EDIT.COM: utils/edit.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/GESTOR.COM: utils/gestor.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SYSINFO.COM: utils/sysinfo.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPCD.SYS: drivers/hspcd.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/KEYB.COM: utils/keyb.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/SYS.COM: utils/sys.asm $(B)/boot.bin $(B)/boot16.bin | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPRATON.COM: drivers/hspraton.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/MOUSETST.COM: tests/progs/mousetst.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<
$(B)/HSPUSB.SYS: drivers/hspusb.asm | $(B)
	$(ASM) -o $@ $(B)/gen/$<

FILES = $(B)/KERNEL.SYS $(B)/COMMAND.COM $(B)/FDISK.COM $(B)/FORMAT.COM \
        $(B)/HELLO.COM $(B)/HELLOEXE.EXE $(B)/UPPER.COM \
        $(B)/HSPRAM.SYS $(B)/HSPTEST.SYS $(B)/HSPXMS.SYS \
        $(B)/XMSTEST.COM $(B)/MCBWALK.COM $(B)/LOLTEST.COM \
        $(B)/HSPSND.COM $(B)/PLAYWAV.COM $(B)/SYS.COM $(B)/HSPUSB.SYS $(B)/KEYB.COM $(B)/INSTALAR.COM $(B)/EDIT.COM $(B)/GESTOR.COM \
        $(B)/SYSINFO.COM $(B)/HSPCD.SYS $(B)/HSPRATON.COM $(B)/MOUSETST.COM

$(IMG): $(B)/boot.bin $(FILES) $(wildcard tests/disk/*.* tests/disk/*/*.*)
	rm -f $@
	mformat -C -f 1440 -v HISPADOS -B $(B)/boot.bin -i $@ ::
	mcopy -i $@ $(B)/KERNEL.SYS ::KERNEL.SYS
	mcopy -i $@ $(B)/COMMAND.COM ::COMMAND.COM
	mcopy -i $@ $(B)/FDISK.COM ::FDISK.COM
	mcopy -i $@ $(B)/FORMAT.COM ::FORMAT.COM
	mcopy -i $@ $(B)/PLAYWAV.COM ::PLAYWAV.COM
	mcopy -i $@ $(B)/SYS.COM ::SYS.COM
	mcopy -i $@ $(B)/KEYB.COM ::KEYB.COM
	mcopy -i $@ $(B)/INSTALAR.COM ::INSTALAR.COM
	mcopy -i $@ $(B)/EDIT.COM ::EDIT.COM
	mcopy -i $@ $(B)/EDIT.COM ::EDITAR.COM
	mcopy -i $@ $(B)/GESTOR.COM ::GESTOR.COM
	mcopy -i $@ $(B)/SYSINFO.COM ::SYSINFO.COM
	mcopy -i $@ $(B)/SYSINFO.COM ::INFO.COM
	mcopy -i $@ tests/disk/README.TXT ::README.TXT
	mcopy -i $@ tests/disk/AUTOEXEC.BAT ::AUTOEXEC.BAT
	mmd   -i $@ ::BAT
	mcopy -i $@ tests/disk/BAT/PRUEBA.BAT ::BAT/PRUEBA.BAT
	mcopy -i $@ tests/disk/BAT/OTRA.BAT ::BAT/OTRA.BAT
	mmd   -i $@ ::DOCS
	mcopy -i $@ tests/disk/DOCS/INFO.TXT ::DOCS/INFO.TXT
	mmd   -i $@ ::BIN
	mcopy -i $@ $(B)/HELLO.COM ::BIN/HELLO.COM
	mcopy -i $@ $(B)/UPPER.COM ::BIN/UPPER.COM
	mcopy -i $@ $(B)/HELLOEXE.EXE ::HELLOEXE.EXE
	mmd   -i $@ ::DRIVERS
	mcopy -i $@ $(B)/HSPRAM.SYS ::DRIVERS/HSPRAM.SYS
	mcopy -i $@ $(B)/HSPTEST.SYS ::DRIVERS/HSPTEST.SYS
	mcopy -i $@ $(B)/HSPXMS.SYS ::DRIVERS/HSPXMS.SYS
	mcopy -i $@ $(B)/HSPSND.COM ::DRIVERS/HSPSND.COM
	mcopy -i $@ $(B)/HSPUSB.SYS ::DRIVERS/HSPUSB.SYS
	mcopy -i $@ $(B)/HSPCD.SYS ::DRIVERS/HSPCD.SYS
	mcopy -i $@ $(B)/HSPRATON.COM ::DRIVERS/HSPRATON.COM
	mcopy -i $@ $(B)/MOUSETST.COM ::BIN/MOUSETST.COM
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

test: $(IMG) usb
	tests/run_tests.sh $(IMG)
	tests/run_hdd_tests.sh $(IMG)
	tests/run_write_tests.sh $(IMG)
	tests/run_batch_tests.sh $(IMG)
	tests/run_config_tests.sh $(IMG)
	tests/run_xms_tests.sh $(IMG)
	tests/run_sound_tests.sh $(IMG)
	tests/run_usb_tests.sh
	tests/run_sys_tests.sh $(IMG)
	tests/run_keyb_tests.sh $(IMG)
	tests/run_usbnative_tests.sh $(IMG)
	tests/run_usbhid_tests.sh $(IMG)
	tests/run_instalar_tests.sh $(IMG)
	tests/run_cd_tests.sh $(IMG)
	tests/run_cd_tests.sh $(IMG) ahci
	tests/run_edit_tests.sh $(IMG)
	tests/run_mouse_tests.sh $(IMG)
	tests/run_compat_tests.sh

clean:
	rm -rf $(B)

.PHONY: all usb run run-hdd test clean
