/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/mbucle.c - Hispa-64, etapa 7: un programa que
 * da vueltas sin llamar al sistema (para Ctrl+C desde ORDENES: SIGINT le
 * llega al volver de una interrupción, en el tic). */
#include <stdio.h>
int main(void)
{
	volatile unsigned long k = 0;
	printf("MBUCLE: doy vueltas sin llamar al sistema (Ctrl+C me para)\n");
	fflush(stdout);
	for (;;) k++;
}
