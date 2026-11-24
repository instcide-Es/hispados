/* nvidia_prueba.c - (TERMO) (NVIDIA) en el anfitrión: nvidia.c con fichas de
 * mentira (QEMU no tiene NVIDIA). Los registros, como los daría un MSI GT80
 * (dos GM204 en SLI) y un ASUS GL553VD (GP107, Optimus); dice «bien: ...» o
 * «mal: ...» por cada comprobación y, al final, el INFORME de cada caso.
 * Se hace con: gcc -DNV_ANFITRION -I.. nvidia_prueba.c ../nvidia.c
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include "nvidia.h"

static int mal;
static char t[12000];

static void mira(int c, const char *que)
{
	printf("%s: %s\n", c ? "bien" : "mal", que);
	mal |= !c;
}

static int hay(const char *o)
{
	return strstr(t, o) != 0;
}

/* una GM204 encendida, en 01:00.0 detrás de 00:01.0, con el sensor dado */
static void gm204(struct nv_ficha *f, uint32_t bus, uint32_t puerto, uint32_t crudo, uint32_t r20400)
{
	memset(f, 0, sizeof *f);
	f->bdf = bus << 16;
	f->id = 0x13D810DE;
	f->subsis = 0x115E1462;
	f->clase = 0x03000000 | 0xA1;
	f->orden0 = 0x00100006;
	f->bar0 = bus == 1 ? 0xF6000000u : 0xF4000000u;
	f->pmcsr = 0x00000008;		/* D0, No_Soft_Reset */
	f->enl_cap = 0x00000083;	/* 8 GT/s x8 */
	f->enl = 0x10830000u | 0x40;	/* estado: 8 GT/s x8, DLLLA */
	f->puerto = 0x80000000u | puerto;
	f->boot = 0x124000A1;		/* GM204 A1 */
	f->fusible = 1;
	f->crudo = crudo;
	f->rtemp = r20400;
	f->estado = NV_E_BIEN;
}

static void puerto(struct nv_puerto *p, uint32_t bdf, uint32_t did, uint32_t enl, uint32_t cap, uint32_t ranura, uint32_t sec, uint32_t detras)
{
	memset(p, 0, sizeof *p);
	p->bdf = bdf;
	p->id = did << 16 | 0x8086;
	p->cap = cap | 0x00420010;	/* raíz (4), v2 */
	p->enl_cap = 0x00100083;
	p->enl = enl;
	p->ranura = ranura;
	p->buses = sec << 8 | sec << 16;
	p->detras_id = detras;
	p->detras_clase = detras == 0xFFFFFFFFu ? 0 : 0x03000000;
	p->deven = 0x000000BD;		/* D0, D1F0, D1F1 (no D1F2), D2, ... */
	p->orden = 0x00100007;
}

int main(void)
{
	struct nv_todo T;
	struct nv_ficha f;
	char d[900];
	int i;

	/* 1. la decodificación (como termo.cas y nouveau) */
	gm204(&f, 1, 0x0800, 0x80000000u, 0x0000002D);
	nv_decodifica(&f);
	mira(f.estado == NV_E_BIEN && nv_temperatura(&f) == 450, "Maxwell GM204: 20400h = 2Dh, fusible 1 y sensor encendido -> 45,0 °C");
	gm204(&f, 1, 0x0800, 0x00000000u, 0);
	nv_decodifica(&f);
	mira(f.estado == NV_E_APAGADO && nv_temperatura(&f) == NV_NO, "Maxwell con el sensor apagado (20008h bit 31 a 0): «sensor apagado», sin temperatura");
	gm204(&f, 1, 0x0800, 0x80008000u, 0x30);
	nv_decodifica(&f);
	mira(f.estado == NV_E_FORZADO, "Maxwell con el sensor forzado (bit 15): «sensor forzado»");
	gm204(&f, 1, 0x0800, 0x80000000u, 0x30);
	f.fusible = 0;
	nv_decodifica(&f);
	mira(f.estado == NV_E_FUSIBLE, "Maxwell sin el fusible 1A8h: «sin sensor calibrado»");
	gm204(&f, 1, 0x0800, 0x80000000u, 0xBADF1100u);
	nv_decodifica(&f);
	mira(f.estado == NV_E_BADF && nv_temperatura(&f) == NV_NO, "Maxwell con 20400h = BADF1100h (el priv ring): no es una temperatura");
	gm204(&f, 1, 0x0800, 0x80000000u, 0x00000092);
	nv_decodifica(&f);
	mira(f.estado == NV_E_IMPOSIBLE, "Maxwell con 146 °C: «lectura imposible»");
	gm204(&f, 1, 0x0800, 0, 0);
	f.boot = 0x137000A1;		/* GP107 */
	f.id = 0x1C8D10DE;
	f.rtemp = f.crudo = 0x20000000u | (41u << 8) | 0x18;
	nv_decodifica(&f);
	mira(f.estado == NV_E_BIEN && nv_temperatura(&f) == 410, "Pascal GP107 (20460h, bit 29 válida, (v y 1FFF8h) >> 8): 41,0 °C");
	f.rtemp = (41u << 8);
	f.estado = NV_E_BIEN;
	nv_decodifica(&f);
	mira(f.estado == NV_E_APAGADO, "Pascal sin el bit 29: sin temperatura");
	f.boot = 0x0E7000A1;		/* GK107 (Kepler) */
	f.estado = NV_E_BIEN;
	nv_decodifica(&f);
	mira(f.estado == NV_E_OTRA, "una Kepler: «no es una Maxwell ni una Pascal»");
	mira(!strcmp(nv_modelo(0x13D810DE), "GeForce GTX 970M") && !strcmp(nv_chip(0x124000A1), "GM204 (Maxwell 2)") && !strcmp(nv_chip(0x137000A1), "GP107 (Pascal)"),
	     "los nombres: 10DE:13D8 GTX 970M, GM204, GP107");
	nv_enlace(d, sizeof d, 0x00100083, 0x10830000u);
	mira(!strcmp(d, "enlace PCIe 3.0 (8 GT/s) x8 (puede PCIe 3.0 (8 GT/s) x8)"), "el enlace: «PCIe 3.0 (8 GT/s) x8»");
	nv_enlace(d, sizeof d, 0x00100103, 0x10010000u);
	mira(!strcmp(d, "enlace CAÍDO (x0) (puede PCIe 3.0 (8 GT/s) x16)"), "el enlace caído: «enlace CAÍDO (x0)»");

	/* 2. el GT80 con las dos: la 1 bien (de 45 a 58 °C en la carga), la 2 que
	 * pasa de 90 °C sin carga */
	memset(&T, 0, sizeof T);
	T.n = 2;
	gm204(&T.g[0].f, 1, 0x0800, 0x80000000u, 45);
	gm204(&T.g[1].f, 2, 0x0900, 0x80000000u, 80);
	T.g[1].f.bdf = 0x020000;
	T.g[1].f.hecho = NV_H_SENSOR;
	T.np = 2;
	puerto(&T.p[0], 0x0800, 0x0C01, 0x10830000u | 0x20000000u, 0, 0, 1, 0x13D810DE);
	puerto(&T.p[1], 0x0900, 0x0C05, 0x10830000u | 0x20000000u, 0, 0, 2, 0x13D810DE);
	for (i = 0; i < 2; i++)
		T.g[i].primera = NV_NO;
	for (i = 0; i < 30; i++) {
		nv_apunta(&T.g[0], 0, 450);
		nv_apunta(&T.g[1], 0, 820 + i);
	}
	for (i = 0; i < 150; i++) {
		nv_apunta(&T.g[0], 1, 460 + i * 120 / 149);
		nv_apunta(&T.g[1], 1, 850 + i * 30 / 149);
	}
	for (i = 0; i < 60; i++) {
		nv_apunta(&T.g[0], 2, 500);
		nv_apunta(&T.g[1], 2, 880);
	}
	nv_apunta(&T.g[0], 3, 999);	/* (el resultado: no cuenta) */
	T.g[0].t = 500;
	T.g[1].t = 880;
	nv_decodifica(&T.g[0].f);
	nv_decodifica(&T.g[1].f);
	nv_informe(&T, t, sizeof t);
	printf("----- INFORME (GT80, las dos) -----\n%s-----\n", t);
	mira(hay("2 gráficas NVIDIA en el PCI") && hay("GPU 1: NVIDIA GeForce GTX 970M, chip GM204 (Maxwell 2), en el PCI 01:00.0, 10DE:13D8 clase 0300, subsistema 1462:115E")
	     && hay("GPU 2: NVIDIA GeForce GTX 970M, chip GM204 (Maxwell 2), en el PCI 02:00.0"), "el GT80: «GPU 1» y «GPU 2», con su ID, chip y subsistema");
	mira(hay("BAR0 F6000000h, D0 (encendida), memoria activada") && hay("enlace PCIe 3.0 (8 GT/s) x8") && hay("cuelga del puerto 00:01.0") && hay("Temperatura: 45 °C (BAR0+20400h"),
	     "BAR0, energía, memoria, enlace y puerto");
	mira(hay("durante la carga: mínima 46,0 °C, media 52,0 °C, máxima 58,0 °C") && hay("Ha subido 13 °C mientras solo se cargaba el procesador") == 0,
	     "la GPU 1: mínima, media y máxima de la carga (sin la línea del resultado)");
	mira(hay("PELIGRO: ha pasado de 90 °C") == 0 && hay("Caliente sin trabajar: 83,5 °C antes de la carga"), "la GPU 2: «caliente sin trabajar» (83,5 °C de media en reposo)");
	mira(hay("el sensor encendido como nouveau (20008h y 2000Ch)") && hay("volátil, se pierde al apagar"), "lo escrito, dicho: el sensor, como nouveau, volátil");
	mira(hay("00:01.0 activo, 00:01.1 activo, 00:01.2 DESACTIVADO"), "DEVEN: los puertos de gráficas del procesador");
	nv_veredicto(&T, t, sizeof t);
	mira(t[0] == 0, "sin 90 °C ni fallos: nada en el resultado");
	nv_apunta(&T.g[1], 1, 935);
	nv_veredicto(&T, t, sizeof t);
	printf("veredicto: %s\n", t);
	mira(hay("GRÁFICA EN PELIGRO: la GPU 2 (GeForce GTX 970M) ha llegado a 93,5 °C"), "más de 90 °C: «GRÁFICA EN PELIGRO» en el resultado");

	/* 3. el GT80 con una sola: la 2.ª tarjeta puesta y el enlace caído */
	memset(&T, 0, sizeof T);
	T.n = 1;
	gm204(&T.g[0].f, 1, 0x0800, 0x00000000u, 0);
	T.g[0].f.estado = NV_E_APAGADO;
	T.g[0].primera = NV_NO;
	T.g[0].t = NV_NO;
	T.np = 2;
	puerto(&T.p[0], 0x0800, 0x0C01, 0x10830000u | 0x20000000u, 0, 0, 1, 0x13D810DE);
	puerto(&T.p[1], 0x0900, 0x0C05, 0x00000000u, 0x01000000u, 0x00400000u, 2, 0xFFFFFFFFu);
	for (i = 0; i < 10; i++)
		nv_apunta(&T.g[0], 0, NV_NO);
	nv_informe(&T, t, sizeof t);
	printf("----- INFORME (GT80, una sola) -----\n%s-----\n", t);
	mira(hay("1 gráfica NVIDIA en el PCI") && hay("Sin temperatura: sensor apagado") && hay("Sin ninguna temperatura en toda la prueba"), "una sola: «sensor apagado», sin temperatura");
	mira(hay("Puerto PCI Express 00:01.1 (raíz, 8086:0C05): enlace CAÍDO (x0)") && hay("ranura con tarjeta puesta") && hay("detrás (bus 02): nada contesta")
	     && hay("OJO: hay una tarjeta puesta y el enlace no sube"), "el puerto 00:01.1: enlace caído, tarjeta puesta, nada detrás");
	nv_veredicto(&T, t, sizeof t);
	printf("veredicto: %s\n", t);
	mira(hay("En el puerto 00:01.1 hay una tarjeta puesta que NO RESPONDE"), "el resultado lo dice: «NO RESPONDE»");

	/* 4. el GL553VD con Optimus: la NVIDIA no contesta (D3cold) y el ACPI la da por apagada */
	memset(&T, 0, sizeof T);
	T.np = 1;
	puerto(&T.p[0], 0x0800, 0x1901, 0x00010000u, 0, 0, 1, 0xFFFFFFFFu);
	T.hay_acpi = 1;
	T.npeg = 1;
	T.peg[0].ap_fun = 1 << 3;
	T.peg[0].puerto = TA_PEG_PR0 | TA_PEG_PR3;
	T.peg[0].hijo = TA_PEG_HAY | TA_PEG_DSM | TA_PEG_PS0 | TA_PEG_PS3;
	T.peg[0].estado = 0x20 | 3;
	nv_informe(&T, t, sizeof t);
	printf("----- INFORME (GL553VD, Optimus) -----\n%s-----\n", t);
	mira(hay("No hay gráficas NVIDIA en el PCI") && hay("la gráfica NVIDIA está apagada por Optimus (ACPI)") && hay("_PR0 _PR3") && hay("_PS0 _PS3 _DSM"),
	     "Optimus: «no hay gráficas NVIDIA» y «apagada por Optimus», con los objetos del ACPI");
	nv_veredicto(&T, t, sizeof t);
	mira(t[0] == 0, "Optimus apagada: no es una avería, nada en el resultado");

	/* 5. sin nada (QEMU) */
	memset(&T, 0, sizeof T);
	nv_informe(&T, t, sizeof t);
	mira(hay("No hay gráficas NVIDIA en el PCI") && !hay("Puertos PCI Express"), "sin NVIDIA ni puertos: «No hay gráficas NVIDIA», sin más");
	nv_veredicto(&T, t, sizeof t);
	mira(t[0] == 0, "sin NVIDIA: nada en el resultado");
	return mal;
}
