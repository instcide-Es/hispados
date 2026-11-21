/* lanza.c - ZIP.BIN y UNZIP.BIN, las órdenes ZIP y UNZIP de Hispa-Pentium
 * (en la Consola y en el intérprete): lanzan COMPRIM.BIN con /ZIP o /UNZIP
 * delante de sus argumentos, lo esperan y acaban con lo que acabe él (lo
 * que escribe sale en la misma pantalla). Así el motor va una sola vez en
 * el disco. ORDEN, en la línea del compilador: "/ZIP" o "/UNZIP".
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include "pentium.h"

int lanza(void)
{
	static char args[700];
	static const char orden[] = ORDEN " ";
	uint32_t n, t, i;

	for (i = 0; orden[i]; i++)
		args[i] = orden[i];
	n = sc(SC_ARGUMENTOS, (uint32_t)(args + i), sizeof(args) - 1 - i, 0);
	if (ES_ERROR(n))
		n = 0;
	if (i + n > sizeof(args) - 1)
		n = sizeof(args) - 1 - i;
	args[i + n] = 0;
	/* (el núcleo lo busca también en \HISPA, en un sistema instalado) */
	t = sc(SC_LANZAR, (uint32_t)"\\COMPRIM.BIN", (uint32_t)args, 0);
	if (ES_ERROR(t)) {
		static const char m[] = "Falta COMPRIM.BIN (el paquete \xAE" "comprim\xAF: Programas y actualizaciones)\r\n";

		sc(SC_ESCRIBIR, (uint32_t)m, sizeof(m) - 1, 0);
		return 1;
	}
	return (int)sc(SC_ESPERAR, t, 0, 0);
}
