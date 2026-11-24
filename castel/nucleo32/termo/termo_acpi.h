/* termo_acpi.h - (TERMO) lo que el servicio de ACPI (acpi/pentium/servicio.c)
 * le da al Termómetro (TERMO.BIN) por el núcleo (la llamada 223, termo.cas):
 * las zonas térmicas de ACPI (\_TZ), los ventiladores (PNP0C0B) y lo que dice
 * el SMBIOS del equipo. Lo usan los dos: si cambia, se hacen los dos.
 * Las temperaturas, en décimas de grado (°C); TA_NO, no se sabe.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef TERMO_ACPI_H
#define TERMO_ACPI_H
#include <stdint.h>

#define TA_VERSION 2
#define TA_NO ((int32_t)0x80000000)
#define TA_ZONAS 10
#define TA_VENT 4
#define TA_TAM 1024		/* lo que copia el núcleo */

struct ta_zona {
	char nombre[24];	/* la ruta: \_TZ_.THRM */
	int32_t tmp, crt, psv, hot, ac0;
	/* (TERMO) por qué no hay _TMP: lo que dijo ACPICA (ACPI_STATUS: 0, AE_OK)
	 * y lo que devolvió (décimas de kelvin; -1, nada) */
	uint32_t estado;
	int32_t crudo;
};

struct ta_vent {
	char nombre[24];
	int32_t velocidad;	/* r.p.m. (de _FST), o TA_NO */
	int32_t control;	/* % (de _FST), o TA_NO */
	int32_t encendido;	/* 1 sí, 0 no, -1 no se sabe (de _STA/_PSC) */
	uint16_t estado_fst, estado_psc;	/* (TERMO) lo que dijo ACPICA de _FST y _PSC */
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
	/* (TERMO) el controlador embebido (EC) mientras se leen las zonas: 1 si
	 * el equipo lo tiene; lecturas, fallos (sin respuesta a tiempo) y
	 * escrituras que el AML ha querido hacer y no se han hecho (al leer las
	 * zonas, el EC es de solo lectura); lo que tarda una lectura (ms) */
	uint32_t ec_hay, ec_lecturas, ec_fallos, ec_ignoradas, ms_lectura;
	/* (TERMO) los ASUS (ROG GL553V...): por WMI, \_SB.ATKD.WMNB con DSTS (solo
	 * leer; nunca DEVS): los ventiladores (r.p.m.) y la temperatura (décimas
	 * de °C); TA_NO si no lo tiene o no vale; asus_hay: 1 si está WMNB;
	 * asus_escribe: el AML ha querido escribir en el EC al leerlo (no fiable) */
	int32_t asus_vent_cpu, asus_vent_gpu, asus_temp;
	uint32_t asus_hay, asus_escribe;
	/* (TERMO) lo que ha contestado DSTS a cada ventilador, tal cual (o
	 * 0xFFFFFFFF si el método falla): 0 r.p.m. es «parado»; TA_NO en
	 * asus_vent_*, «no disponible» */
	uint32_t asus_crudo_cpu, asus_crudo_gpu;
	/* (TERMO) la batería, de _BST: lo que gasta o lo que carga ahora (mW,
	 * de todas las que se gastan o se cargan; con mA, por el voltaje), TA_NO
	 * si no lo dice; bat_estado: bit 0 descargando, 1 cargando */
	int32_t bat_mw;
	uint32_t bat_estado;
	/* (TERMO) lo guardado en \TERMO\EQUIPO de la unidad del sistema (para el
	 * futuro controlador del ratón táctil I2C-HID): las tablas ACPI, tal cual
	 * (DSDT.BIN, SSDT1.BIN...), y la lista PCI (PCI.TXT); equipo_hecho: 1 si
	 * ya se ha intentado, 2 si no se ha podido escribir */
	uint32_t equipo_tablas, equipo_pci, equipo_hecho;
	/* (TERMO) el voltaje de ahora de la primera batería (mV, el 4.º valor
	 * de _BST); 0 si no lo dice */
	uint32_t bat_mv;
};

_Static_assert(sizeof(struct termo_acpi) <= TA_TAM, "termo_acpi: no cabe en lo que copia el núcleo");
#endif
