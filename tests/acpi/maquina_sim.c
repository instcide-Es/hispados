/* maquina_sim.c - Pruebas del núcleo ACPI en el equipo de compilación: un
 * portátil de mentira. Sustituye a acpi/nucleo/maquina.c:
 *  - la memoria física: las tablas ACPI volcadas de un equipo de verdad
 *    (con una RSDP y una XSDT hechas aquí) y, para lo demás que los métodos
 *    toquen (las variables de la BIOS, registros de la placa), páginas a cero;
 *  - los puertos: un controlador embebido en 62h/66h con sus 256 registros
 *    (los pone el escenario) y sus avisos; el resto se apunta y lee FFh (o
 *    lo que diga el escenario);
 *  - el reloj: de mentira, avanza con lo que se espera y se duerme.
 * Programa de 32 bits sin biblioteca (como el núcleo): usa int 80h.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "nucleo.h"
#include "sim.h"

/* ---- memoria ---- */
#define NREG 256
static struct { u32 fis, tam; u8 *p; } rg[NREG];
static int nrg;
static u8 monton[3 << 20];
static u32 monton_n;
u32 sim_paginas;

void sim_region(u32 fis, u32 tam, void *p)
{
	if (nrg == NREG)
		sim_muere("demasiadas regiones de memoria");
	rg[nrg].fis = fis;
	rg[nrg].tam = tam;
	rg[nrg].p = p;
	nrg++;
}

void *sim_trozo(u32 tam)
{
	void *p = monton + monton_n;

	tam = (tam + 15) & ~15u;
	if (monton_n + tam > sizeof monton)
		sim_muere("memoria simulada agotada");
	monton_n += tam;
	return p;
}

void *mq_mapa(u32 fis, u32 tam)
{
	int i;
	u32 ini, fin;

	for (i = 0; i < nrg; i++)
		if (fis >= rg[i].fis && fis - rg[i].fis + tam <= rg[i].tam)
			return rg[i].p + (fis - rg[i].fis);
	/* memoria que nadie ha puesto: páginas a cero (se apunta la primera vez) */
	ini = fis & ~0xFFFu;
	fin = (fis + tam + 0xFFF) & ~0xFFFu;
	if (sim_charla)
		reg("  [sim] memoria física %08Xh..%08Xh: páginas a cero\n", ini, fin - 1);
	sim_paginas += (fin - ini) >> 12;
	sim_region(ini, fin - ini, sim_trozo(fin - ini));
	return rg[nrg - 1].p + (fis - ini);
}
void mq_desmapa(void *p, u32 tam) { }
void mq_traza_mem(u32 fisica, u32 v, u32 bits) { }

/* ---- reloj ---- */
static u64 ahora;
u64 mq_ahora_us(void) { return ahora += 1; }
void mq_espera_us(u32 us) { ahora += us; }
void sim_avanza_us(u32 us) { ahora += us; }
void mq_inicia(const struct ll_inicio *i) { }
u32 mq_rsdp(void) { return sim_rsdp; }
u32 sim_rsdp;

/* ---- puertos ---- */
u8 sim_ec[256];
u8 sim_ec_tocado[256];		/* bit 0 leído, bit 1 escrito */
static u8 ec_cola[16];
static int ec_ncola;
static int ec_fase, ec_cmd, ec_dir;	/* 0 libre, 1 espera dirección, 2 espera dato */
static int ec_sale = -1;
int sim_ec_mudo;			/* el controlador no contesta (para probar los plazos) */
u32 sim_ec_ordenes;

#define NES 64
static struct { u32 puerto; u32 valor; } es_fijo[NES];
static int nes;
static u32 es_visto[64];
static int nvisto;
u32 sim_cf8;

void sim_evento(int q)
{
	if (ec_ncola < 16)
		ec_cola[ec_ncola++] = q;
}
void sim_es_fija(u32 puerto, u32 valor)
{
	if (nes < NES) {
		es_fijo[nes].puerto = puerto;
		es_fijo[nes++].valor = valor;
	}
}

static void es_apunta(u32 p, int escribe, u32 v)
{
	int i;

	for (i = 0; i < nvisto; i++)
		if (es_visto[i] == (p | escribe << 31))
			return;
	if (nvisto < 64)
		es_visto[nvisto++] = p | escribe << 31;
	if (sim_charla)
		reg(escribe ? "  [sim] puerto %04Xh <- %Xh (primera vez)\n" : "  [sim] puerto %04Xh leído (primera vez)\n", p, v);
}

u32 mq_es_lee(u32 p, u32 n)
{
	int i;

	if (p == 0x66) {
		if (sim_ec_mudo)
			return 0x02;	/* IBF para siempre */
		return (ec_sale >= 0 ? 1 : 0) | (ec_ncola ? 0x20 : 0);
	}
	if (p == 0x62) {
		u32 v = ec_sale >= 0 ? (u32)ec_sale : 0;

		ec_sale = -1;
		return v;
	}
	if (p >= 0xCFC && p < 0xD00)
		return sim_pci_lee(sim_cf8, p & 3, n);
	if (p == 0xCF8)
		return sim_cf8;
	for (i = 0; i < nes; i++)
		if (es_fijo[i].puerto == p)
			return es_fijo[i].valor;
	es_apunta(p, 0, 0);
	return n == 1 ? 0xFF : n == 2 ? 0xFFFF : 0xFFFFFFFF;
}

void mq_es_escribe(u32 p, u32 n, u32 v)
{
	if (p == 0x66) {
		if (sim_ec_mudo)
			return;
		sim_ec_ordenes++;
		ec_cmd = v & 0xFF;
		ec_sale = -1;
		if (ec_cmd == 0x80 || ec_cmd == 0x81)
			ec_fase = 1;
		else if (ec_cmd == 0x84) {
			ec_sale = 0;
			if (ec_ncola) {
				ec_sale = ec_cola[0];
				ec_ncola--;
				memmove(ec_cola, ec_cola + 1, ec_ncola);
			}
			ec_fase = 0;
		}
		return;
	}
	if (p == 0x62) {
		if (sim_ec_mudo)
			return;
		if (ec_fase == 1) {
			ec_dir = v & 0xFF;
			if (ec_cmd == 0x80) {
				ec_sale = sim_ec[ec_dir];
				sim_ec_tocado[ec_dir] |= 1;
				ec_fase = 0;
			} else
				ec_fase = 2;
		} else if (ec_fase == 2) {
			sim_ec[ec_dir] = v;
			sim_ec_tocado[ec_dir] |= 2;
			ec_fase = 0;
		}
		return;
	}
	if (p == 0xCF8 && n == 4) {
		sim_cf8 = v;
		return;
	}
	if (p >= 0xCFC && p < 0xD00)
		return;
	if (p == 0x80)
		return;
	es_apunta(p, 1, v);
}
