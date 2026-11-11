# HispaDOS - compilación / build
NASM  ?= nasm
QEMU  ?= qemu-system-i386
B      = build
IMG    = $(B)/hispados.img

all: $(IMG)

$(B):
	mkdir -p $(B)

$(B)/boot.bin: boot/boot.asm | $(B)
	$(NASM) -f bin -o $@ $<
$(B)/KERNEL.SYS: kernel/kernel.asm | $(B)
	$(NASM) -f bin -o $@ $<
$(B)/COMMAND.COM: shell/command.asm | $(B)
	$(NASM) -f bin -o $@ $<
$(B)/HELLO.COM: tests/progs/hello.asm | $(B)
	$(NASM) -f bin -o $@ $<
$(B)/HELLOEXE.EXE: tests/progs/helloexe.asm | $(B)
	$(NASM) -f bin -o $@ $<

FILES = $(B)/KERNEL.SYS $(B)/COMMAND.COM $(B)/HELLO.COM $(B)/HELLOEXE.EXE

$(IMG): $(B)/boot.bin $(FILES) tests/disk/README.TXT tests/disk/DOCS/INFO.TXT
	rm -f $@
	mformat -C -f 1440 -v HISPADOS -B $(B)/boot.bin -i $@ ::
	mcopy -i $@ $(B)/KERNEL.SYS ::KERNEL.SYS
	mcopy -i $@ $(B)/COMMAND.COM ::COMMAND.COM
	mcopy -i $@ tests/disk/README.TXT ::README.TXT
	mmd   -i $@ ::DOCS
	mcopy -i $@ tests/disk/DOCS/INFO.TXT ::DOCS/INFO.TXT
	mmd   -i $@ ::BIN
	mcopy -i $@ $(B)/HELLO.COM ::BIN/HELLO.COM
	mcopy -i $@ $(B)/HELLOEXE.EXE ::HELLOEXE.EXE

run: $(IMG)
	$(QEMU) -drive file=$(IMG),format=raw,if=floppy -boot a

test: $(IMG)
	tests/run_tests.sh $(IMG)

clean:
	rm -rf $(B)

.PHONY: all run test clean
