/* hispa64.ld - ACPICA64.BIN: ACPI en el núcleo de Hispa-64, un binario plano
 * enlazado para 0xFFFFFFFFC0000000 (el último GB, donde el núcleo lo pone):
 * primero el código (de solo leer, en sus páginas), luego los datos y lo que
 * empieza a cero, en páginas aparte (sin ejecutar). La cabecera (entrada.asm)
 * dice dónde acaba cada cosa.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_inicio)
SECTIONS
{
	. = 0xFFFFFFFFC0000000;
	.text : { *(.entrada) *(.text .text.*) }
	.rodata : { *(.rodata .rodata.*) }
	. = ALIGN(4096);
	_ini_datos = .;
	.data : { *(.data .data.*) }
	_fin_imagen = .;
	. = ALIGN(16);
	.bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) }
	. = ALIGN(4096);
	_fin_bss = .;
	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.rela*) }
}
