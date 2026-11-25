/* maquina.c - ACPICA64.BIN (Hispa-64): el equipo para el núcleo de C de
 * acpi/nucleo/. Corre en el anillo 0, así que los puertos van directos; lo
 * demás se le pide al núcleo de Hispa-64 (struct h64_tabla):
 *   - la memoria física: el mapa directo (la RAM y lo de ACPI) o, si no
 *     está en él (los registros de un aparato), mapea_disp sin caché. Lo
 *     puesto no se quita: aquí se apunta y se vuelve a usar;
 *   - la configuración PCI: lo que osl.c escribe en CF8h se guarda aquí y se
 *     manda con el acceso a CFCh-CFFh por Pci64 (que también la usa: así no
 *     se cruzan). Lo de 8 y 16 bits se escribe leyendo y escribiendo los 32;
 *   - la hora (ahora_ns) y dormir (duerme_ms, que suelta el procesador).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hispa64.h"

struct h64_inicio h64;
static u32 pci_dir;		/* lo último escrito en CF8h */

static inline u32 entra(u32 p, u32 n)
{
	u32 v = 0;

	if (n == 1) {
		u8 b;
		__asm__ __volatile__("inb %w1, %0" : "=a"(b) : "Nd"((u16)p));
		v = b;
	} else if (n == 2) {
		u16 w;
		__asm__ __volatile__("inw %w1, %0" : "=a"(w) : "Nd"((u16)p));
		v = w;
	} else
		__asm__ __volatile__("inl %w1, %0" : "=a"(v) : "Nd"((u16)p));
	return v;
}

static inline void sale(u32 p, u32 n, u32 v)
{
	if (n == 1)
		__asm__ __volatile__("outb %b0, %w1" : : "a"((u8)v), "Nd"((u16)p));
	else if (n == 2)
		__asm__ __volatile__("outw %w0, %w1" : : "a"((u16)v), "Nd"((u16)p));
	else
		__asm__ __volatile__("outl %0, %w1" : : "a"(v), "Nd"((u16)p));
}

static u32 pci32(u32 d)
{
	return (u32)castel4(h64.t.pci_lee, (d >> 16) & 0xFF, (d >> 11) & 31, (d >> 8) & 7, d & 0xFC);
}

u32 mq_es_lee(u32 p, u32 n)
{
	if (p == 0xCF8 && n == 4)
		return pci_dir;
	if (p >= 0xCFC && p <= 0xCFF) {
		u32 v = pci32(pci_dir), s = (p & 3) * 8;

		if (!(pci_dir & 0x80000000u))
			return 0xFFFFFFFFu;
		return n == 4 ? v : n == 2 ? (v >> s) & 0xFFFF : (v >> s) & 0xFF;
	}
	return entra(p, n);
}

void mq_es_escribe(u32 p, u32 n, u32 v)
{
	if (p == 0xCF8 && n == 4) {
		pci_dir = v;
		return;
	}
	if (p >= 0xCFC && p <= 0xCFF) {
		u32 d = pci_dir, s = (p & 3) * 8, m;

		if (!(d & 0x80000000u))
			return;
		if (n != 4) {
			m = (n == 2 ? 0xFFFFu : 0xFFu) << s;
			v = (pci32(d) & ~m) | ((v << s) & m);
		}
		castel5(h64.t.pci_pon, (d >> 16) & 0xFF, (d >> 11) & 31, (d >> 8) & 7, d & 0xFC, v);
		return;
	}
	sale(p, n, v);
}

/* ---- la memoria física ---- */
#define NTROZOS 128
static struct { u64 fis, fin, vir; } trozos[NTROZOS];
static int ntrozos, ronda;

void *mq_mapa(ufis fisica, u32 tam)
{
	u64 fin = fisica + (tam ? tam : 1), pag, v;
	int i;

	for (i = 0; i < ntrozos; i++)
		if (trozos[i].fis <= fisica && trozos[i].fin >= fin)
			return (void *)(trozos[i].vir + (fisica - trozos[i].fis));
	pag = fisica & ~0xFFFull;
	fin = (fin + 0xFFF) & ~0xFFFull;
	v = castel2(h64.t.mapa, pag, fin - pag);
	if (!v)
		return 0;
	if (ntrozos < NTROZOS)
		i = ntrozos++;
	else
		i = ronda++ % NTROZOS;
	trozos[i].fis = pag;
	trozos[i].fin = fin;
	trozos[i].vir = v;
	return (void *)(v + (fisica - pag));
}
void mq_desmapa(void *p, u32 tam) { }

/* ---- el reloj ---- */
u64 mq_ahora_us(void)
{
	return castel0(h64.t.ahora_ns) / 1000;
}

void mq_espera_us(u32 us)
{
	u64 fin;

	/* lo largo, durmiendo (el equipo sigue); lo corto, mirando el reloj */
	if (us >= 20000) {
		castel1(h64.t.duerme_ms, us / 1000);
		return;
	}
	fin = castel0(h64.t.ahora_ns) + (u64)us * 1000;
	while (castel0(h64.t.ahora_ns) < fin)
		__asm__ __volatile__("pause");
}

ufis mq_rsdp(void)
{
	return h64.rsdp;
}

void mq_inicia(const struct ll_inicio *i) { }

/* ---- lo que pide nucleo.h de la tarea (aquí no hay tarea: el hilo «acpi»
 * del núcleo llama a cada cosa, y dormir es dormir de verdad) ---- */
int tarea_prisa;
struct ll_inicio ini;
struct ll_apm apm;
struct ll_salud salud;
struct ll_estado estado;

void tarea_duerme(u32 ms)
{
	if (ms < 3) {
		mq_espera_us(ms * 1000);
		return;
	}
	castel1(h64.t.duerme_ms, ms);
}

int tarea_descansa(u32 ms)
{
	tarea_duerme(ms);
	return 0;
}

int tarea_quieta(void) { return 1; }

void apm_publica(void)
{
	estado.apm = apm;
}

