/* termo_acpi.h - (TERMO) lo que el servicio de ACPI (acpi/pentium/servicio.c)
 * le da al Termómetro (TERMO.BIN) por el núcleo (la llamada 223, termo.cas):
 * las zonas térmicas de ACPI (\_TZ), los ventiladores (PNP0C0B) y lo que dice
 * el SMBIOS del equipo. Lo usan los dos: si cambia, se hacen los dos.
 * Las temperaturas, en décimas de grado (°C); TA_NO, no se sabe.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef TERMO_ACPI_H
#define TERMO_ACPI_H
#include <stdint.h>

#define TA_VERSION 1
#define TA_NO ((int32_t)0x80000000)
#define TA_ZONAS 10
#define TA_VENT 4
#define TA_TAM 1024		/* lo que copia el núcleo */

struct ta_zona {
	char nombre[24];	/* la ruta: \_TZ_.THRM */
	int32_t tmp, crt, psv, hot, ac0;
};

struct ta_vent {
	char nombre[24];
	int32_t velocidad;	/* r.p.m. (de _FST), o TA_NO */
	int32_t control;	/* % (de _FST), o TA_NO */
	int32_t encendido;	/* 1 sí, 0 no, -1 no se sabe (de _STA/_PSC) */
};

struct termo_acpi {
	uint32_t version;	/* TA_VERSION */
	uint32_t lecturas;	/* sube con cada lectura de las zonas */
	uint32_t nzonas, nvent;
	uint32_t smbios;	/* 1: lo de abajo vale */
	struct ta_zona zona[TA_ZONAS];
	struct ta_vent vent[TA_VENT];
	char fabricante[40], producto[40], version_eq[24];
	char placa_fab[40], placa[40], bios[40];
};

#endif
