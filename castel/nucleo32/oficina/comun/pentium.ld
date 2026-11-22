/* pentium.ld - los programas de la oficina de 32 bits (PLUMA.BIN...), un programa
 * del anillo 3 (binario plano con la cabecera HPU3) enlazado para
 * 0x40000000, donde el núcleo pone los programas.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_inicio)
SECTIONS
{
	. = 0x40000000;
	.text : { *(.entrada) *(.text .text.*) }
	.rodata : { *(.rodata .rodata.*) }
	.data : { *(.data .data.*) }
	_fin_imagen = .;
	. = ALIGN(16);
	_ini_bss = .;
	.bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) }
	_fin_bss = .;
	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
