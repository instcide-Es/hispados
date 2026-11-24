/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/libsaludo.c - Hispa-64, etapa 7: una biblioteca
 * compartida de prueba (libsaludo.so): la carga ld-musl con open y mmap del
 * archivo (sus segmentos, con MAP_FIXED), como cualquier otra. */
#include <stdio.h>
static int veces = 40;              /* (en .data: se copia al escribir) */
static char búfer[64];              /* (en .bss) */
const char *saludo(int n)
{
	veces += n;
	snprintf(búfer, sizeof búfer, "libsaludo.so: hola (%d)", veces);
	return búfer;
}
