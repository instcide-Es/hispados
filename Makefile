# HispaDOS - compilación / build
NASM  ?= nasm
QEMU  ?= qemu-system-i386
B      = build
IMG    = $(B)/hispados.img
HDD    = $(B)/hdd.img
NFLAGS = -f bin -i lib/

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
	$(NASM) $(NFLAGS) -o $@ $<
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

FILES = $(B)/KERNEL.SYS $(B)/COMMAND.COM $(B)/FDISK.COM $(B)/FORMAT.COM \
        $(B)/HELLO.COM $(B)/HELLOEXE.EXE

$(IMG): $(B)/boot.bin $(FILES) tests/disk/README.TXT tests/disk/DOCS/INFO.TXT
	rm -f $@
	mformat -C -f 1440 -v HISPADOS -B $(B)/boot.bin -i $@ ::
	mcopy -i $@ $(B)/KERNEL.SYS ::KERNEL.SYS
	mcopy -i $@ $(B)/COMMAND.COM ::COMMAND.COM
	mcopy -i $@ $(B)/FDISK.COM ::FDISK.COM
	mcopy -i $@ $(B)/FORMAT.COM ::FORMAT.COM
	mcopy -i $@ tests/disk/README.TXT ::README.TXT
	mmd   -i $@ ::DOCS
	mcopy -i $@ tests/disk/DOCS/INFO.TXT ::DOCS/INFO.TXT
	mmd   -i $@ ::BIN
	mcopy -i $@ $(B)/HELLO.COM ::BIN/HELLO.COM
	mcopy -i $@ $(B)/HELLOEXE.EXE ::HELLOEXE.EXE

# disco duro vacío de 64 MB para probar FDISK y FORMAT
$(HDD): | $(B)
	dd if=/dev/zero of=$@ bs=1M count=64 status=none

run: $(IMG)
	$(QEMU) -drive file=$(IMG),format=raw,if=floppy -boot a

run-hdd: $(IMG) $(HDD)
	$(QEMU) -drive file=$(IMG),format=raw,if=floppy \
	        -drive file=$(HDD),format=raw,if=ide -boot a

test: $(IMG)
	tests/run_tests.sh $(IMG)
	tests/run_hdd_tests.sh $(IMG)

clean:
	rm -rf $(B)

.PHONY: all run run-hdd test clean
