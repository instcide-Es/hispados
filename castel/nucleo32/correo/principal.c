/* principal.c - Correo: la entrada. Sin argumentos (o con un .EML o un
 * mailto:), la ventana (ventana.c); con «/ORDEN ...», las órdenes (orden.c);
 * con «/BANDEJA», mirar sin ventana si hay correo nuevo y avisar (lo lanza
 * el escritorio al arrancar si el usuario lo quiere: CORREO.INI, avisar=si).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "correo.h"

int correo_orden(char *linea);
int correo_ventana(const char *args);
int correo_bandeja(void);

int correo_principal(void)
{
	static char args[1200];

	so_argumentos(args, sizeof args);
	if (so_carpeta_datos(dir_datos, sizeof dir_datos)) {
		so_apunta("no se puede usar la carpeta del correo (%s)", dir_datos);
		so_dice("Correo: no se puede usar la carpeta del correo.\n");
		return 3;
	}
	ajustes_lee();
	cuentas_lee();
	libreta_lee();
	so_apunta("Correo %s: %d cuentas, la carpeta %s", CORREO_VERSION, n_cuentas, dir_datos);
	if (!strncmp(args, "/ORDEN", 6) || !strncmp(args, "/orden", 6))
		return correo_orden(args + 6);
	if (!strncmp(args, "/BANDEJA", 8) || !strncmp(args, "/bandeja", 8))
		return correo_bandeja();
	return correo_ventana(args);
}
