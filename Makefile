# HispaDOS - compilación / build
NASM  ?= nasm
QEMU  ?= qemu-system-i386
B      = build
IMG    = $(B)/hispados.img
HDD    = $(B)/hdd.img
NFLAGS = -f bin -i lib/
ifdef TRACE
KFLAGS = -DTRACE
endif

all: $(IMG)

$(B):
	mkdir -p $(B)

$(B)/boot.bin: boot/boot.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/boot16.bin: boot/boot16.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/mbr.bin: boot/mbr.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/KERNEL.SYS: kernel/kernel.asm | $(B)
	$(NASM) $(NFLAGS) $(KFLAGS) -o $@ $<
$(B)/COMMAND.COM: shell/command.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/FDISK.COM: utils/fdisk.asm lib/util.inc $(B)/mbr.bin | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/FORMAT.COM: utils/format.asm lib/util.inc $(B)/boot.bin $(B)/boot16.bin | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/HELLO.COM: tests/progs/hello.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/HELLOEXE.EXE: tests/progs/helloexe.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/UPPER.COM: tests/progs/upper.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/HSPRAM.SYS: drivers/hspram.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/HSPTEST.SYS: drivers/hsptest.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/HSPXMS.SYS: drivers/hspxms.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/XMSTEST.COM: tests/progs/xmstest.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/MCBWALK.COM: tests/progs/mcbwalk.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<
$(B)/LOLTEST.COM: tests/progs/loltest.asm | $(B)
	$(NASM) $(NFLAGS) -o $@ $<

FILES = $(B)/KERNEL.SYS $(B)/COMMAND.COM $(B)/FDISK.COM $(B)/FORMAT.COM \
        $(B)/HELLO.COM $(B)/HELLOEXE.EXE $(B)/UPPER.COM \
        $(B)/HSPRAM.SYS $(B)/HSPTEST.SYS $(B)/HSPXMS.SYS \
        $(B)/XMSTEST.COM $(B)/MCBWALK.COM $(B)/LOLTEST.COM

$(IMG): $(B)/boot.bin $(FILES) $(wildcard tests/disk/*.* tests/disk/*/*.*)
	rm -f $@
	mformat -C -f 1440 -v HISPADOS -B $(B)/boot.bin -i $@ ::
	mcopy -i $@ $(B)/KERNEL.SYS ::KERNEL.SYS
	mcopy -i $@ $(B)/COMMAND.COM ::COMMAND.COM
	mcopy -i $@ $(B)/FDISK.COM ::FDISK.COM
	mcopy -i $@ $(B)/FORMAT.COM ::FORMAT.COM
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
	mcopy -i $@ $(B)/XMSTEST.COM ::BIN/XMSTEST.COM
	mcopy -i $@ $(B)/MCBWALK.COM ::BIN/MCBWALK.COM
	mcopy -i $@ $(B)/LOLTEST.COM ::BIN/LOLTEST.COM

# disco duro vacío de 64 MB para probar FDISK y FORMAT
$(HDD): | $(B)
	dd if=/dev/zero of=$@ bs=1M count=64 status=none

run: $(IMG)
	$(QEMU) -rtc base=localtime -drive file=$(IMG),format=raw,if=floppy -boot a

run-hdd: $(IMG) $(HDD)
	$(QEMU) -rtc base=localtime -drive file=$(IMG),format=raw,if=floppy \
	        -drive file=$(HDD),format=raw,if=ide -boot a

test: $(IMG)
	tests/run_tests.sh $(IMG)
	tests/run_hdd_tests.sh $(IMG)
	tests/run_write_tests.sh $(IMG)
	tests/run_batch_tests.sh $(IMG)
	tests/run_config_tests.sh $(IMG)
	tests/run_xms_tests.sh $(IMG)
	tests/run_compat_tests.sh

clean:
	rm -rf $(B)

.PHONY: all run run-hdd test clean
