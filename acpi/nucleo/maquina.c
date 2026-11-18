/* maquina.c - Núcleo ACPI de Hispa-486: el equipo de verdad (puertos,
 * memoria física, reloj). En las pruebas del anfitrión lo sustituye
 * tests/acpi/maquina_sim.c.
 *
 * El núcleo corre con segmentos de base = el bloque de memoria extendida
 * donde está cargado (enlazado para la dirección 0), así que la memoria
 * física F se ve en el puntero F - base (los segmentos llegan a 4 GB y la
 * cuenta da la vuelta).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "nucleo.h"

u32 mq_base;		/* la pone entrada.asm en la primera llamada */
static u32 tsc_ms;
static u64 tsc0, falso_us;

u32 mq_es_lee(u32 p, u32 n)
{
	u32 v;

	if (n == 1) {
		u8 b;
		__asm__ __volatile__("inb %w1, %0" : "=a"(b) : "d"(p));
		v = b;
	} else if (n == 2) {
		u16 w;
		__asm__ __volatile__("inw %w1, %0" : "=a"(w) : "d"(p));
		v = w;
	} else
		__asm__ __volatile__("inl %w1, %0" : "=a"(v) : "d"(p));
	return v;
}

void mq_es_escribe(u32 p, u32 n, u32 v)
{
	if (n == 1)
		__asm__ __volatile__("outb %b0, %w1" : : "a"(v), "d"(p));
	else if (n == 2)
		__asm__ __volatile__("outw %w0, %w1" : : "a"(v), "d"(p));
	else
		__asm__ __volatile__("outl %0, %w1" : : "a"(v), "d"(p));
}

void *mq_mapa(u32 fisica, u32 tam)
{
	return (void *)(fisica - mq_base);
}
void mq_desmapa(void *p, u32 tam) { }

static u64 tsc(void)
{
	u32 a, d;

	__asm__ __volatile__("rdtsc" : "=a"(a), "=d"(d));
	return ((u64)d << 32) | a;
}

/* Con contador de tiempo en el procesador (Pentium y siguientes: todos los
 * equipos con batería ACPI), el reloj sale de él. Sin él, se cuenta lo que
 * se espera: cada escritura en el puerto 80h tarda más o menos 1 µs. */
u64 mq_ahora_us(void)
{
	if (tsc_ms)
		return (tsc() - tsc0) * 1000 / tsc_ms;
	return falso_us += 2;
}

void mq_espera_us(u32 us)
{
	if (tsc_ms) {
		u64 fin = tsc() + (u64)us * tsc_ms / 1000 + 1;

		while (tsc() < fin)
			__asm__ __volatile__("pause");
		return;
	}
	falso_us += us;
	while (us--)
		mq_es_escribe(0x80, 1, 0);
}

/* la RSDP: «RSD PTR » de 16 en 16 en el primer KB de la EBDA y entre
 * E0000h y FFFFFh, con la suma de sus 20 primeros octetos a cero */
static u32 busca_rsdp(u32 desde, u32 hasta)
{
	u32 d;

	for (d = desde; d < hasta; d += 16) {
		const u8 *p = mq_mapa(d, 20);
		u8 s = 0;
		int i;

		if (*(const u32 *)p != 0x20445352 || *(const u32 *)(p + 4) != 0x20525450)
			continue;
		for (i = 0; i < 20; i++)
			s += p[i];
		if (!s)
			return d;
	}
	return 0;
}

u32 mq_rsdp(void)
{
	u32 ebda = *(const u16 *)mq_mapa(0x40E, 2) << 4, d = 0;

	if (ebda >= 0x80000 && ebda < 0xA0000)
		d = busca_rsdp(ebda, ebda + 1024);
	if (!d)
		d = busca_rsdp(0xE0000, 0x100000);
	return d;
}

void mq_inicia(const struct ll_inicio *i)
{
	tsc_ms = i->tsc_ms;
	if (tsc_ms)
		tsc0 = tsc();
}
