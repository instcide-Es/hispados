/* potencia_prueba.c - (TERMO) en el anfitrión: potencia.c con MSR de
 * mentira (QEMU no tiene RAPL ni los motivos del freno). Equipos simulados:
 * el MSI GT80 (i7-4720HQ, Haswell 06_3Ch) frenado por potencia a 30 W con
 * el cargador puesto, con PROCHOT de la placa y el procesador fresco, pegado
 * a su PL1 de 47 W, con el PL1 por debajo del TDP; un Atom Z3735F (06_37h,
 * unidades en µJ y mW), un AMD Zen, una máquina virtual que da 0 a todo y
 * otra que da la excepción 13. Dice «bien: ...» o «mal: ...» por cada cosa;
 * con -v, también los textos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include "potencia.h"

#define NM 64
static struct { uint32_t m, lo, hi, log_mask, estado, marca; int hay; } R[NM];
static int nr, todo_cero, escrituras, verbose, mal;
static char sucesos[8000];

static void pon(uint32_t m, uint32_t lo, uint32_t hi, uint32_t log_mask)
{
	int i;

	for (i = 0; i < nr; i++)
		if (R[i].m == m)
			break;
	if (i == nr)
		nr++;
	R[i].m = m, R[i].lo = lo, R[i].hi = hi, R[i].log_mask = log_mask, R[i].hay = 1, R[i].estado = R[i].marca = 0;
}

static int busca(uint32_t m)
{
	int i;

	for (i = 0; i < nr; i++)
		if (R[i].m == m && R[i].hay)
			return i;
	return -1;
}

static int lee(uint32_t m, uint32_t *lo, uint32_t *hi)
{
	int i = busca(m);

	if (todo_cero) {
		*lo = *hi = 0;
		return 1;
	}
	if (i < 0)
		return 0;	/* la excepción 13 */
	*lo = R[i].lo | R[i].estado | R[i].marca;
	*hi = R[i].hi;
	return 1;
}

static int limpia(uint32_t m)
{
	int i = busca(m);

	escrituras++;
	if (todo_cero)
		return 1;
	if (i < 0)
		return 0;
	R[i].marca = 0;		/* (R/WC0: el 0 borra las marcas; el estado es de solo lectura) */
	return 1;
}

static void suceso(const char *t)
{
	size_t n = strlen(sucesos);

	snprintf(sucesos + n, sizeof sucesos - n, "%s\n", t);
}

static void serie(const char *t)
{
	if (verbose)
		printf("   serie: %s", t);
}

static void mira(int ok, const char *que, const char *texto)
{
	printf("%s: %s\n", ok ? "bien" : "mal", que);
	if (!ok || verbose)
		printf("----\n%s\n----\n", texto);
	mal |= !ok;
}

/* un rato de 2 s: la energía sube (paq, nuc, gpu en W), los C-states, el TSC */
static uint64_t tsc;
#define HZ 2600000000ull
static void suma(uint32_t m, double julios, double unidad_j)
{
	int i = busca(m);

	if (i >= 0)
		R[i].lo += (uint32_t)(julios / unidad_j + 0.5);
}

static void suma64(int i, uint64_t d)
{
	uint64_t v = ((uint64_t)R[i].hi << 32 | R[i].lo) + d;

	R[i].lo = (uint32_t)v, R[i].hi = (uint32_t)(v >> 32);
}

static void rato(int fase, double w_paq, double w_nuc, double w_gpu, double unidad_j, int ca, int t)
{
	tsc += 2 * HZ;
	suma(0x611, w_paq * 2, unidad_j);
	suma(0x639, w_nuc * 2, unidad_j);
	suma(0x641, w_gpu * 2, unidad_j);
	po_mide(fase, tsc, HZ, ca, t, 1000);
}

static struct po_lector LEC = { lee, limpia, suceso, serie };
static char inf[8000], ver[1000], dat[400];

static void acaba(void)
{
	po_informe(inf, sizeof inf);
	po_veredicto(ver, sizeof ver);
	po_datos(dat, sizeof dat);
}

/* el i7-4720HQ del GT80: 606h = A0E03h (1/8 W, 61 µJ, 976 µs) */
static void haswell(uint32_t pl1_w8, uint32_t tdp_w8)
{
	nr = 0;
	todo_cero = 0;
	sucesos[0] = 0;
	escrituras = 0;
	tsc = 1000;
	pon(0x606, 0x000A0E03, 0, 0);
	pon(0x610, 0x00DD8000 | pl1_w8, 0x004281D6, 0);		/* PL1, 28 s, activo; PL2 58,75 W */
	pon(0x614, tdp_w8, 0, 0);
	pon(0xCE, 0x00001A00, 0x00000800, 0);			/* base 26, mínima 8 */
	pon(0x1AD, 0x22222324, 0, 0);
	pon(0x611, 0xFFFFF000, 0, 0);				/* (cerca de la vuelta: 32 bits) */
	pon(0x639, 0x10000, 0, 0);
	pon(0x641, 0x100, 0, 0);
	pon(0x613, 0, 0, 0);
	pon(0x19C, 0x88300000, 0, 0xAAAA);			/* (lectura válida, 48 °C por debajo) */
	pon(0x1B1, 0x88300000, 0, 0x0AAA);
	pon(0x690, 0, 0, 0xFFFF0000);
	pon(0x6B0, 0, 0, 0xFFFF0000);
	pon(0x6B1, 0, 0, 0xFFFF0000);
	pon(0x60D, 0, 0, 0);
	pon(0x3F8, 0, 0, 0);
	pon(0x3F9, 0, 0, 0);
	pon(0x3FA, 0, 0, 0);
	pon(0x3FC, 0, 0, 0);
	pon(0x3FD, 0, 0, 0);
	pon(0x3FE, 0, 0, 0);
}

static void marca(uint32_t m, uint32_t estado, uint32_t log)
{
	int i = busca(m);

	if (i >= 0) {
		R[i].estado = estado;
		R[i].marca |= log;
	}
}

int main(int argc, char **argv)
{
	int k, i;
	const double U = 1.0 / 16384;	/* 61 µJ */

	verbose = argc > 1 && !strcmp(argv[1], "-v");

	/* 1. el GT80 con el cargador: frenado por PL1 a 30 W, lejos del PL1 (47 W) */
	haswell(376, 376);
	marca(0x690, 0, 1u << 26);	/* (de antes: PL1 anotado al abrir) */
	po_empieza(&LEC, 1, 0, 6, 0x3C);
	k = busca(0x690);
	mira(R[k].marca == 0 && escrituras >= 1, "al abrir: la marca de PL1 de antes, anotada y puesta a 0 (escribiendo 0 en 690h)", sucesos);
	mira(strstr(sucesos, "PL1 47,0 W (activo), PL2 58,7 W; TDP 47,0 W") != 0 || strstr(sucesos, "PL1 47,0 W (activo), PL2 58,8 W; TDP 47,0 W") != 0,
	     "los límites de la BIOS en el .LOG: PL1 47 W, PL2 58,75 W, TDP 47 W", sucesos);
	for (i = 0; i < 10; i++) {
		int c = busca(0x3F9), c6 = busca(0x3FD);

		suma64(c, HZ * 2 / 10);		/* PC6 un 10 % en reposo */
		suma64(c6, HZ * 2 * 6 / 10);	/* CC6 un 60 % */
		rato(PO_REPOSO, 8, 3, 0.5, U, 1, 450);
	}
	for (i = 0; i < 60; i++) {
		marca(0x690, i >= 5 ? 1u << 10 : 0, i >= 5 ? 1u << 26 : 0);
		if (i >= 5)
			R[busca(0x613)].lo += (uint32_t)(2 * 1024 * 0.9);	/* 90 % del rato frenado por RAPL */
		rato(PO_CARGA, i < 5 ? 55 : 30, i < 5 ? 45 : 22, 0.4, U, 1, 700);
	}
	marca(0x690, 0, 0);
	for (i = 0; i < 20; i++)
		rato(PO_ENFRIA, 9, 3, 0.4, U, 1, 550);
	acaba();
	mira(strstr(inf, "Procesador entero (paquete)") && strstr(inf, "8,0 W") && strstr(inf, "55,0 W"),
	     "INFORME: el paquete a 8 W en reposo y 55 W de máximo (y la vuelta de 32 bits del contador, bien)", inf);
	mira(strstr(inf, "último minuto de la carga: 30,0 W de media, bastante por debajo de su PL1") != 0, "INFORME: el último minuto de la carga, 30 W, por debajo del PL1", inf);
	mira(strstr(inf, "  - Por límite de potencia PL1 (el de carga larga): reposo 0 % carga 91 % enfriando 0 %") != 0, "POR QUÉ SE FRENA: el PL1, el 91 % de la carga", inf);
	mira(strstr(inf, "Anotado ya al abrir el Termómetro") && strstr(inf, "PL1"), "POR QUÉ SE FRENA: lo anotado de antes (PL1)", inf);
	mira(strstr(inf, "PKG_PERF_STATUS 613h): 82 % en la carga") != 0 || strstr(inf, "PKG_PERF_STATUS 613h): 81 % en la carga") != 0,
	     "el tiempo frenado por RAPL (613h)", inf);
	mira(strstr(inf, "PC6  (el procesador entero)") && strstr(inf, "10 %") && strstr(inf, "CC6  (este núcleo)") && strstr(inf, "60 %"),
	     "ESTADOS DE REPOSO: PC6 10 % y CC6 60 % en reposo", inf);
	mira(strstr(ver, "Potencia: el procesador se frena por potencia a 30,0 W con el cargador puesto, bastante por debajo de su PL1 (47,0 W)") &&
	     strstr(ver, "lo limita otra cosa: la fuente o el ajuste del fabricante"), "RESULTADO: «se frena por potencia a 30 W con el cargador puesto: la fuente o el ajuste del fabricante»", ver);
	mira(po_frenado_carga(), "po_frenado_carga: sí", "");
	mira(strncmp(dat, " | potencia=paq ", 16) == 0 && !strchr(strchr(dat, '=') + 1, '='), "la línea DATOS: « | potencia=paq ...» con un solo «=»", dat);

	/* 2. PROCHOT de la placa a ratos (solo la marca: el estado no se ve) con el procesador a 60 °C de 100 */
	haswell(376, 376);
	po_empieza(&LEC, 1, 0, 6, 0x3C);
	for (i = 0; i < 5; i++)
		rato(PO_REPOSO, 8, 3, 0.5, U, 1, 450);
	for (i = 0; i < 40; i++) {
		if (i % 4 == 1) {
			marca(0x19C, 0, 1u << 3);
			marca(0x690, 0, 1u << 16);
		}
		rato(PO_CARGA, 25, 18, 0.4, U, 1, 600);
	}
	acaba();
	mira(strstr(inf, "PROCHOT desde fuera") && strstr(inf, "carga 25 %"), "PROCHOT visto solo por su marca de registro (pulsos cortos): el 25 % de la carga", inf);
	mira(strstr(ver, "PROCHOT desde fuera") && strstr(ver, "lejos de su límite") && strstr(ver, "apunta a la alimentación") && strstr(ver, "con el cargador puesto"),
	     "RESULTADO: PROCHOT con el procesador fresco: «apunta a la alimentación», con el cargador", ver);
	mira(strstr(sucesos, "se frena por: PROCHOT desde fuera") != 0, "el .LOG: el suceso de PROCHOT, una vez", sucesos);

	/* 3. pegado a su PL1 (47 W): lo normal */
	haswell(376, 376);
	po_empieza(&LEC, 1, 0, 6, 0x3C);
	for (i = 0; i < 5; i++)
		rato(PO_REPOSO, 8, 3, 0.5, U, 0, 450);
	for (i = 0; i < 40; i++) {
		marca(0x690, 1u << 10, 1u << 26);
		rato(PO_CARGA, 47, 38, 0.4, U, 0, 850);
	}
	acaba();
	mira(strstr(ver, "se frena por potencia a 47,0 W sin el cargador (con la batería): es su PL1 (47,0 W), el límite que le pone el fabricante") != 0,
	     "RESULTADO: pegado al PL1 de 47 W: «el límite que le pone el fabricante», no un fallo", ver);

	/* 4. la BIOS con el PL1 en 25 W y el TDP de 47 W, con el cargador */
	haswell(200, 376);
	po_empieza(&LEC, 1, 0, 6, 0x3C);
	for (i = 0; i < 5; i++)
		rato(PO_REPOSO, 8, 3, 0.5, U, 1, 450);
	for (i = 0; i < 40; i++) {
		marca(0x690, 1u << 10, 1u << 26);
		rato(PO_CARGA, 25, 18, 0.4, U, 1, 650);
	}
	acaba();
	mira(strstr(inf, "OJO: el PL1 que ha puesto la BIOS (25,0 W) está por debajo del TDP del procesador (47,0 W)") != 0, "INFORME: el PL1 por debajo del TDP, con OJO", inf);
	mira(strstr(ver, "con el cargador puesto: la BIOS le ha puesto el PL1 en 25,0 W, por debajo de su TDP (47,0 W)") != 0, "RESULTADO: el PL1 bajado por el fabricante", ver);

	/* 5. sin frenos: «sin frenarse» */
	haswell(376, 376);
	po_empieza(&LEC, 1, 0, 6, 0x3C);
	for (i = 0; i < 5; i++)
		rato(PO_REPOSO, 8, 3, 0.5, U, 1, 450);
	for (i = 0; i < 40; i++)
		rato(PO_CARGA, 40, 32, 0.4, U, 1, 750);
	acaba();
	mira(strstr(ver, "llegado a 40,0 W de media") && strstr(ver, "sin frenarse") && strstr(inf, "No se ha frenado por nada"), "sin frenos: «40 W sin frenarse»", ver);
	mira(!po_frenado_carga(), "po_frenado_carga: no", "");

	/* 6. una máquina virtual que da 0 a todo (QEMU sin KVM) */
	nr = 0;
	todo_cero = 1;
	sucesos[0] = 0;
	tsc = 1000;
	po_empieza(&LEC, 1, 0, 6, 0x3C);
	for (i = 0; i < 10; i++)
		rato(PO_CARGA, 40, 30, 0, U, -1, -100000);
	acaba();
	mira(strstr(inf, "no dice sus vatios (RAPL, MSR 606h: no lo tiene)") && strstr(inf, "dan siempre 0") && strstr(inf, "no se han movido") && !ver[0] && !dat[0],
	     "QEMU sin KVM (todo da 0): «no lo tiene», «dan siempre 0», sin veredicto ni datos", inf);

	/* 7. una que da la excepción 13 a todo (KVM): «no lo tiene» */
	nr = 0;
	todo_cero = 0;
	sucesos[0] = 0;
	tsc = 1000;
	po_empieza(&LEC, 1, 0, 6, 0x3C);
	for (i = 0; i < 10; i++)
		rato(PO_CARGA, 40, 30, 0, U, -1, -100000);
	acaba();
	mira(strstr(inf, "no lo tiene") && strstr(inf, "no lo tiene: ni 19Ch") && strstr(inf, "no da sus contadores de reposo") && !ver[0] && strstr(sucesos, "RAPL: no lo tiene"),
	     "KVM (la excepción 13 a todo): «no lo tiene», sin romper nada", inf);

	/* 8. el Atom Z3735F (Bay Trail): 606h = 505h (32 mW, 32 µJ) */
	nr = 0;
	sucesos[0] = 0;
	tsc = 1000;
	pon(0x606, 0x00000505, 0, 0);
	pon(0x611, 0, 0, 0);
	pon(0x639, 0, 0, 0);
	pon(0x19C, 0x88300000, 0, 0xAAAA);
	po_empieza(&LEC, 1, 0, 6, 0x37);
	for (i = 0; i < 10; i++)
		rato(PO_CARGA, 2.2, 1.5, 0, 32e-6, 1, 700);
	acaba();
	mira(strstr(inf, "2,2 W") != 0, "el Atom Z3735F (unidades de Silvermont, 32 µJ): 2,2 W", inf);

	/* 9. un AMD Zen (17h): C0010299h = A1003h (15,3 µJ) */
	nr = 0;
	sucesos[0] = 0;
	tsc = 1000;
	pon(0xC0010299, 0x000A1003, 0, 0);
	pon(0xC001029B, 0, 0, 0);
	pon(0xC001029A, 0, 0, 0);
	po_empieza(&LEC, 0, 1, 0x17, 0x18);
	for (i = 0; i < 10; i++) {
		tsc += 2 * HZ;
		R[busca(0xC001029B)].lo += (uint32_t)(15.0 * 2 * 65536 + 0.5);
		R[busca(0xC001029A)].lo += (uint32_t)(6.0 * 2 * 65536 + 0.5);
		po_mide(PO_CARGA, tsc, HZ, 1, 700, 950);
	}
	acaba();
	mira(strstr(inf, "15,0 W") && strstr(inf, "Este núcleo") && strstr(inf, "los guarda el SMU") && strstr(inf, "Los AMD no dicen"), "AMD Zen: 15 W el paquete, el núcleo, y que los límites y los frenos no se leen", inf);
	return mal;
}
