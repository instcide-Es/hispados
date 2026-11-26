/* Hispa-DOS - utils/firma/cripto16.ld: cripto16.c enlazado en 0x120 del
   segmento de ACTUALIZ.COM (justo detrás del salto del principio); todo, sus
   datos a cero incluidos, va dentro del binario plano.
   Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
OUTPUT_FORMAT("elf32-i386")
ENTRY(cr_firma)
SECTIONS
{
    . = 0x120;
    .todo : {
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
        *(.bss .bss.* COMMON)
        . = ALIGN(16);
    }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
