/* maquina.c - ACPI.BIN, el servicio de ACPI de Hispa-Pentium: el equipo
 * (puertos, memoria física, configuración PCI y reloj) para el mismo núcleo
 * de C que HSPACPI.BIN (acpi/nucleo/). Aquí el núcleo corre en el anillo 3,
 * como un programa más, y lo que no puede hacer un programa se lo pide al
 * núcleo de Hispa-Pentium (castel/nucleo32/energia.cas), que solo se lo
 * deja hacer a este servicio:
 *   - los puertos, de uno en uno (llamada 150);
 *   - la memoria física: el núcleo la pone a la vista del programa, sin
 *     caché, en su zona de 0xB0000000 (llamada 151). Lo puesto no se quita:
 *     aquí se apunta qué trozos hay ya y se vuelven a usar;
 *   - la configuración PCI (llamada 152), de una vez: dirección y dato con
 *     las interrupciones tapadas, para no cruzarse con el núcleo, que
 *     también usa CF8h/CFCh. Lo que ACPICA (osl.c) escribe en CF8h se guarda
 *     aquí y se manda con el acceso a CFCh-CFFh.
 * El reloj sale del contador de tiempo del procesador (RDTSC, que se puede
 * leer desde el anillo 3), medido al empezar contra los tics del núcleo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "pentium.h"

static u32 pci_dir;		/* lo último escrito en CF8h */

u32 mq_es_lee(u32 p, u32 n)
{
	if (p == 0xCF8 && n == 4)
		return pci_dir;
	if (p >= 0xCFC && p <= 0xCFF)
		return sc(SC_PCI, (pci_dir & 0x7FFFFFFC) | (p & 3), 0, n);
	return sc(SC_PUERTO, p, 0, n);
}

void mq_es_escribe(u32 p, u32 n, u32 v)
{
	if (p == 0xCF8 && n == 4) {
		pci_dir = v;
		return;
	}
	if (p >= 0xCFC && p <= 0xCFF) {
		if (pci_dir & 0x80000000u)
			sc(SC_PCI, (pci_dir & 0x7FFFFFFC) | (p & 3), v, n | ES_ESCRIBE);
		return;
	}
	sc(SC_PUERTO, p, v, n | ES_ESCRIBE);
}

/* ---- la memoria física ---- */
#define NTROZOS 96
static struct { u32 fis, fin, vir; } trozos[NTROZOS];
static int ntrozos, ronda;
u32 mq_puestos;			/* octetos que el núcleo ha puesto a la vista (para el registro) */

void *mq_mapa(u32 fisica, u32 tam)
{
	u64 fin = (u64)fisica + (tam ? tam : 1);
	u32 pag, finp, v;
	int i;

	for (i = 0; i < ntrozos; i++)
		if (trozos[i].fis <= fisica && (u64)trozos[i].fin >= fin && trozos[i].fin)
			return (void *)(trozos[i].vir + (fisica - trozos[i].fis));
	pag = fisica & ~0xFFFu;
	fin = (fin + 0xFFF) & ~(u64)0xFFF;
	if (fin > 0x100000000ull)
		return 0;
	finp = (u32)fin;	/* (0: hasta el final de los 4 GB) */
	v = sc(SC_MEMORIA, pag, (u32)(fin - pag), 0);
	if (!v || ES_ERROR(v))
		return 0;
	mq_puestos += (u32)(fin - pag);
	if (ntrozos < NTROZOS)
		i = ntrozos++;
	else
		i = ronda++ % NTROZOS;
	trozos[i].fis = pag;
	trozos[i].fin = finp ? finp : 0xFFFFFFFFu;
	trozos[i].vir = v;
	return (void *)(v + (fisica - pag));
}
void mq_desmapa(void *p, u32 tam) { }

/* ---- el reloj ---- */
static u32 tsc_ms;
static u64 tsc0;
static u32 falso_us;

static u64 tsc(void)
{
	u32 a, d;

	__asm__ __volatile__("rdtsc" : "=a"(a), "=d"(d));
	return ((u64)d << 32) | a;
}

/* (TERMO) el reloj del servicio va con los tics del núcleo (100 por segundo,
 * los que cuentan bien: el Termómetro apunta cada 2 s con ellos) y, dentro
 * de cada tic, con el contador del procesador; y nunca hacia atrás. Antes
 * era solo el contador del procesador con las vueltas por ms medidas al
 * arrancar: si aquella medida salía grande (el servicio, quitado del
 * procesador mientras medía, con el arranque ocupado), su reloj iba más
 * despacio que el de verdad, y con él la lectura de la batería (cada 10 s
 * de su reloj) y la de las zonas (cada 2 s): en el Monovate 14.1, la
 * batería, la misma de principio a fin (23/11/2026) */
static u32 tic_visto;
static u64 tsc_visto, us_ultimo;

u64 mq_ahora_us(void)
{
	u32 t = sc(SC_TICS, 0, 0, 0);
	u64 v, dentro;

	if (tsc_ms) {
		u64 x = tsc();

		if (t != tic_visto || !tsc_visto) {
			tic_visto = t;
			tsc_visto = x;
		}
		dentro = (x - tsc_visto) * 1000 / tsc_ms;
	} else
		dentro = (falso_us += 2) % 10000;	/* (sin contador: un poco cada vez que se pregunta, para que avance) */
	if (dentro > 9999)
		dentro = 9999;
	v = (u64)t * 10000 + dentro;
	if (v < us_ultimo)
		v = us_ultimo;
	us_ultimo = v;
	return v;
}

void mq_espera_us(u32 us)
{
	/* lo largo, durmiendo (el equipo sigue); lo corto, mirando el reloj */
	if (us >= 20000) {
		sc(SC_DORMIR, us / 10000, 0, 0);
		us %= 10000;
	}
	if (tsc_ms) {
		u64 fin = tsc() + (u64)us * tsc_ms / 1000 + 1;

		while (tsc() < fin)
			__asm__ __volatile__("pause");
		return;
	}
	while (us--)
		sc(SC_PUERTO, 0x80, 0, 1 | ES_ESCRIBE);
}

/* la RSDP: «RSD PTR » de 16 en 16 en el primer KB de la EBDA y entre
 * E0000h y FFFFFh, con la suma de sus 20 primeros octetos a cero (como en
 * acpi/nucleo/maquina.c) */
static u32 busca_rsdp(u32 desde, u32 hasta)
{
	const u8 *b = mq_mapa(desde, hasta - desde);
	u32 d;

	if (!b)
		return 0;
	for (d = desde; d < hasta; d += 16) {
		const u8 *p = b + (d - desde);
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

/* (UEFI) la que dice el núcleo («rsdp=» en la línea: la copia de GRUB EFI,
 * porque sin BIOS no está en su zona ni hay EBDA) */
static u32 rsdp_dada;

void mq_pon_rsdp(u32 d)
{
	rsdp_dada = d;
}

/* (UEFI) el punto de entrada del SMBIOS («_SM3_», «_SM_» o «_DMI_»): el
 * que dice el núcleo («smbios=», sacado de la tabla del sistema de EFI) o,
 * si no lo dice (la BIOS de siempre), el que haya entre F0000h y FFFFFh, de
 * 16 en 16. Da su dirección física (0: no hay) */
static u32 smbios_dado;

static int es(const u8 *p, const char *t)
{
	for (; *t; p++, t++)
		if (*p != (u8)*t)
			return 0;
	return 1;
}

void mq_pon_smbios(u32 d)
{
	smbios_dado = d;
}

u32 mq_smbios(void)
{
	const u8 *b;
	u32 i;

	if (smbios_dado)
		return smbios_dado;
	b = mq_mapa(0xF0000, 0x10000);
	if (!b)
		return 0;
	for (i = 0; i + 32 <= 0x10000; i += 16)
		if (es(b + i, "_SM3_") || es(b + i, "_SM_") || es(b + i, "_DMI_"))
			return 0xF0000 + i;
	return 0;
}

u32 mq_rsdp(void)
{
	static u32 hallada = 1;
	const u16 *bda;
	u32 ebda, d = 0;

	if (hallada != 1)
		return hallada;
	if (rsdp_dada) {	/* (UEFI) */
		hallada = rsdp_dada;
		return hallada;
	}
	bda = mq_mapa(0x40E, 2);
	ebda = bda ? (u32)*bda << 4 : 0;
	if (ebda >= 0x40000 && ebda < 0xA0000)
		d = busca_rsdp(ebda, ebda + 1024);
	if (!d)
		d = busca_rsdp(0xE0000, 0x100000);
	hallada = d;
	return d;
}

void mq_inicia(const struct ll_inicio *i)
{
	tsc_ms = i->tsc_ms;
	if (tsc_ms)
		tsc0 = tsc();
}

/* las vueltas del contador de tiempo por milisegundo, contra los tics del
 * núcleo (100 por segundo). 0 si el procesador no lo tiene. (TERMO) Cinco
 * medidas de 5 tics; solo valen las que han visto los tics de uno en uno (el
 * servicio no se ha quedado sin procesador en medio: entonces la medida sale
 * de más) y, de esas, la del medio. Si ninguna vale, de las otras la más
 * pequeña, con los tics que han pasado de verdad */
u32 mq_mide_tsc(void)
{
	u32 a, b, c, d, t, t1, i, n = 0, j;
	u64 x0, x1, m[5], peor = 0;

	__asm__ __volatile__("pushfl; popl %0; movl %0, %1; xorl $0x200000, %0; pushl %0; popfl; pushfl; popl %0"
			     : "=&r"(a), "=&r"(b));
	if (((a ^ b) & 0x200000) == 0)
		return 0;	/* sin CPUID */
	__asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
	if (!(d & 0x10))
		return 0;	/* sin RDTSC */
	for (i = 0; i < 5; i++) {
		int bien = 1;

		t = sc(SC_TICS, 0, 0, 0);
		while ((t1 = sc(SC_TICS, 0, 0, 0)) == t)
			;
		x0 = tsc();
		if (t1 != t + 1)
			bien = 0;
		t = t1;
		for (;;) {
			u32 ahora = sc(SC_TICS, 0, 0, 0);

			if (ahora != t1) {
				if (ahora != t1 + 1)
					bien = 0;
				t1 = ahora;
			}
			if (t1 >= t + 5)
				break;
		}
		x1 = tsc();
		if (sc(SC_TICS, 0, 0, 0) != t1)
			bien = 0;
		if (bien && t1 == t + 5)
			m[n++] = (x1 - x0) / 50;
		else if (t1 > t) {
			u64 v = (x1 - x0) / ((u64)(t1 - t) * 10);

			if (!peor || v < peor)
				peor = v;
		}
	}
	if (!n)
		return (u32)peor;
	for (i = 1; i < n; i++)		/* (de menor a mayor) */
		for (j = i; j > 0 && m[j] < m[j - 1]; j--) {
			u64 x = m[j];

			m[j] = m[j - 1];
			m[j - 1] = x;
		}
	return (u32)m[n / 2];
}
