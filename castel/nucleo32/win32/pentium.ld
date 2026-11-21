/* pentium.ld - WIN32.BIN: enlazado para la dirección 0 de los segmentos de
 * los programas de Windows (base 0x40000000: entrada.asm), con la cabecera
 * HPU3 delante.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_inicio)
SECTIONS
{
	. = 0;
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
