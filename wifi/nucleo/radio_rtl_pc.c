/*
 * radio_rtl_pc.c - La Realtek RTL8723BS de radio_rtl.c en una tableta de
 * verdad (WIFI.BIN de Hispa-Pentium): dónde está su controladora SDHCI (la
 * tercera de Bay Trail, la del SDIO) y cómo se enciende el chip.
 *
 * Dónde, por este orden:
 *   1. en el PCI: 8086:0F15 (Bay Trail) o 8086:2295 (Cherry Trail), clase
 *      08 05, si el firmware no la esconde (el Medion S2217): su BAR0;
 *   2. lo que aprendió ACPI.BIN en un arranque con ACPI (\SDIO.TXT de la
 *      unidad del sistema o de la X:, si es de este mismo equipo: el puente
 *      00:00.0 y su subsistema): la controladora 80860F14 _UID 2 / INT33BB,
 *      con su dirección (de su _CRS) y lo que hizo su _PS0 y el de su hijo
 *      (OBDA8723...) con las patillas GPIO para encender el chip;
 * Sin ninguna de las dos no se busca a ciegas: en un Bay Trail, leer los
 * registros de un aparato apagado (la controladora del SDIO en D3) cuelga el
 * equipo entero.
 *
 * Encender: lo que hicieron los _PS0 (el de la controladora y el de su hijo,
 * el chip) lo apunta ACPI.BIN en \SDIO.TXT, escritura a escritura, con sus
 * esperas: la memoria (el PMCSR de la controladora en su «BAR1»: D0) y las
 * patillas GPIO (el WL_EN del chip). Se repite tal cual ANTES de tocar la
 * controladora (apagada, no se puede ni leer).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "hispa_linux.h"
#include "base.h"
#include "nucleo.h"
#include "rtl.h"

int hl_lee_texto(const char *ruta, char *d, int cabe);	/* servicio.c: -1 si no está */
const struct sdio_plat *rtl_plat_prueba;

static volatile u8 *regs;
static d32 base, base1;
static const char *origen = "?";
#define MAX_GPIO 32
static d32 gpio_dir[MAX_GPIO], gpio_val[MAX_GPIO], gpio_ms[MAX_GPIO];
static o8 gpio_bits[MAX_GPIO];
static int n_gpio;
static char hijo[24];

static d32 lee(d32 r, int a)
{
	if (a == 1)
		return regs[r];
	if (a == 2)
		return *(volatile u16 *)(regs + r);
	return *(volatile u32 *)(regs + r);
}
static void pon(d32 r, int a, d32 v)
{
	if (a == 1)
		regs[r] = v;
	else if (a == 2)
		*(volatile u16 *)(regs + r) = v;
	else
		*(volatile u32 *)(regs + r) = v;
}
static void espera(d32 us) { hl_udelay(us); }
static d32 ms(void) { return hl_jiffies(); }
static const struct sdio_plat plat = { lee, pon, espera, ms };

/* ------------------------------------------------------- los archivos */
static d32 hex(const char **p)
{
	d32 n = 0;
	const char *s = *p;

	while (*s == ' ')
		s++;
	for (;; s++) {
		char c = *s;
		if (c >= '0' && c <= '9')
			n = n * 16 + c - '0';
		else if (c >= 'A' && c <= 'F')
			n = n * 16 + c - 'A' + 10;
		else if (c >= 'a' && c <= 'f')
			n = n * 16 + c - 'a' + 10;
		else
			break;
	}
	*p = s;
	return n;
}

static d32 dec(const char **p)
{
	d32 n = 0;
	const char *s = *p;

	while (*s == ' ')
		s++;
	while (*s >= '0' && *s <= '9')
		n = n * 10 + *s++ - '0';
	*p = s;
	return n;
}

static int empieza(const char *s, const char *p)
{
	while (*p)
		if (*s++ != *p++)
			return 0;
	return 1;
}

static const char *busca(const char *t, const char *p)
{
	for (; *t; t++)
		if (empieza(t, p))
			return t;
	return NULL;
}

/* lee «nombre» de la unidad del sistema o de la X:; 1 si es de este equipo */
static int lee_de_este_equipo(const char *nombre, char *t, int cabe, const char **donde)
{
	static const char *const sitios[] = { "\\", "X:\\", NULL };
	char r[24];
	const char *p;
	d32 a, b;
	int k, n;

	for (k = 0; sitios[k]; k++) {
		snprintf(r, sizeof r, "%s%s", sitios[k], nombre);
		n = hl_lee_texto(r, t, cabe);
		if (n < 0)
			continue;
		*donde = sitios[k];
		p = busca(t, "equipo=");
		if (!p) {
			hl_log(1, "rtl: %s no dice «equipo=»: no se usa\n", r);
			return 0;
		}
		p += 7;
		a = hex(&p);
		b = hex(&p);
		if (a != hl_pci_lee(0, 0, 0) || b != hl_pci_lee(0, 0, 0x2c)) {
			hl_log(1, "rtl: %s es de otro equipo (dice %08X %08X; este es %08X %08X): no se usa\n", r, a, b,
			       hl_pci_lee(0, 0, 0), hl_pci_lee(0, 0, 0x2c));
			return 0;
		}
		return 1;
	}
	return 0;
}

/* \SDIO.TXT (lo escribe ACPI.BIN: acpi/pentium/sdhci.c) */
static int de_sdio_txt(void)
{
	static char t[2048];
	const char *p, *d = "";

	if (!lee_de_este_equipo("SDIO.TXT", t, sizeof t, &d))
		return 0;
	n_gpio = 0;
	for (p = t; p && *p; p = strchr(p, '\n') ? strchr(p, '\n') + 1 : NULL) {
		const char *q = p;
		if (empieza(q, "sdio=")) {
			q += 5;
			base = hex(&q);
		} else if (empieza(q, "sdio1=")) {
			q += 6;
			base1 = hex(&q);
		} else if (empieza(q, "hijo=")) {
			int i;
			q += 5;
			for (i = 0; i < (int)sizeof hijo - 1 && q[i] > ' '; i++)
				hijo[i] = q[i];
			hijo[i] = 0;
		} else if ((empieza(q, "mem=") || empieza(q, "gpio=")) && n_gpio < MAX_GPIO) {
			q += *q == 'm' ? 4 : 5;
			gpio_dir[n_gpio] = hex(&q);
			gpio_val[n_gpio] = hex(&q);
			gpio_bits[n_gpio] = dec(&q);
			if (gpio_bits[n_gpio] != 8 && gpio_bits[n_gpio] != 16)
				gpio_bits[n_gpio] = 32;
			gpio_ms[n_gpio] = 0;
			n_gpio++;
		} else if (empieza(q, "espera=") && n_gpio > 0) {
			q += 7;
			gpio_ms[n_gpio - 1] += dec(&q);
		}
	}
	if (!base)
		return 0;
	hl_log(2, "rtl: %sSDIO.TXT (lo aprendido de ACPI en este equipo): la controladora SDIO en %08X%s%s; %d escrituras para encenderla (y el chip)\n",
	       d, base, hijo[0] ? ", el chip: " : "", hijo, n_gpio);
	origen = "ACPI (\\SDIO.TXT)";
	return 1;
}

int rtl_busca(void)
{
	int bus, df, i;
	d32 id, b0, p, v;

	if (rtl_plat_prueba) {
		origen = "el modelo de las pruebas";
		return 0;
	}
	base = base1 = 0;
	for (bus = 0; bus < 256 && !base; bus++)
		for (df = 0; df < 256; df++) {
			id = hl_pci_lee(bus, df, 0);
			if (id == 0xffffffff || (id & 0xffff) == 0xffff) {
				if (!(df & 7))
					df += 7;
				continue;
			}
			if (id == 0x0F158086 || id == 0x22958086) {
				b0 = hl_pci_lee(bus, df, 0x10);
				hl_log(2, "rtl: la controladora SDIO en el PCI, %02x:%02x.%d: %04X:%04X, BAR0 %08X\n", bus, df >> 3, df & 7,
				       id & 0xffff, id >> 16, b0);
				/* D0, memoria y DMA (como sdhci.cas) */
				p = hl_pci_lee(bus, df, 0x34) & 0xFC;
				for (i = 0; p && i < 48; i++) {
					v = hl_pci_lee(bus, df, p);
					if ((v & 0xff) == 1) {
						v = hl_pci_lee(bus, df, p + 4);
						if (v & 3) {
							struct pci_dev d;
							memset(&d, 0, sizeof d);
							d.bus = &d.bus0;
							d.bus0.number = bus;
							d.devfn = df;
							pci_write_config_dword(&d, p + 4, v & ~3u);
							hl_log(2, "rtl:   estaba en D%u: a D0\n", v & 3);
							hl_udelay(10000);
						}
						break;
					}
					p = (v >> 8) & 0xFC;
				}
				{
					struct pci_dev d;
					memset(&d, 0, sizeof d);
					d.bus = &d.bus0;
					d.bus0.number = bus;
					d.devfn = df;
					pci_write_config_dword(&d, 4, hl_pci_lee(bus, df, 4) | 6);
				}
				if ((b0 & 1) || !(b0 & ~0xFFFu)) {
					hl_log(0, "rtl: su BAR0 no vale (%08X)\n", b0);
					return -2;
				}
				base = b0 & ~0xFFFu;
				origen = "el PCI";
				break;
			}
			if (!(df & 7) && !(hl_pci_lee(bus, df, 0x0c) & 0x800000))
				df += 7;
		}
	if (base)
		return 0;
	if (de_sdio_txt())
		return 0;
	id = hl_pci_lee(0, 0, 0);
	if (id != 0x0F008086 && id != 0x22808086)
		return 1;		/* no es una tableta Bay Trail o Cherry Trail */
	hl_log(2, "rtl: Bay Trail/Cherry Trail (%08X) sin la controladora SDIO en el PCI ni \\SDIO.TXT\n", id);
	hl_log(1, "rtl: no se sabe dónde está la controladora del Wi-Fi (SDIO). Lo aprende un arranque con ACPI\n");
	hl_log(1, "rtl:   (la entrada «con ACPI» del menú: ACPI.BIN deja \\SDIO.TXT en la memoria USB) y se usa en los siguientes.\n");
	return 1;
}

/* repetir lo que hicieron los _PS0 (la memoria y las patillas), con sus esperas */
static void enciende_como_acpi(void)
{
	int i;

	for (i = 0; i < n_gpio; i++) {
		volatile u8 *r = ioremap(gpio_dir[i] & ~0xFFFu, 0x1000);
		d32 o = gpio_dir[i] & 0xFFF;
		if (!r)
			continue;
		if (gpio_bits[i] == 8)
			r[o] = gpio_val[i];
		else if (gpio_bits[i] == 16)
			*(volatile u16 *)(r + o) = gpio_val[i];
		else
			*(volatile u32 *)(r + o) = gpio_val[i];
		hl_log(2, "rtl: %08X := %0*X%s\n", gpio_dir[i], gpio_bits[i] / 4, gpio_val[i], gpio_ms[i] ? " y una espera" : "");
		hl_udelay(1000 * (gpio_ms[i] ? gpio_ms[i] : 1));
	}
	hl_udelay(50000);
}

/* (modo ACPI) la controladora, en D0: el PMCSR de su configuración PCI, que
 * se ve en su «BAR1» (es lo que hace el _PS0 de las DSDT de Intel: PSAT) */
static void d0_por_bar1(void)
{
	volatile u8 *c = ioremap(base1, 0x1000);
	d32 p, v;
	int i;

	if (!c)
		return;
	v = *(volatile u32 *)c;
	if ((v & 0xFFFF) != 0x8086) {
		hl_log(1, "rtl: en %08X (el «BAR1») no está la configuración PCI de la controladora (%08X): no se toca\n", base1, v);
		return;
	}
	p = c[0x34] & 0xFC;
	for (i = 0; p && p < 0x100 && i < 16; i++) {
		if (c[p] == 1) {
			v = *(volatile u16 *)(c + p + 4);
			hl_log(2, "rtl: la controladora %04X:%04X, en D%u%s\n", *(volatile u16 *)c, *(volatile u16 *)(c + 2), v & 3,
			       (v & 3) ? ": a D0" : "");
			if (v & 3) {
				*(volatile u16 *)(c + p + 4) = v & ~3u;
				hl_udelay(10000);
			}
			return;
		}
		p = c[p + 1] & 0xFC;
	}
	hl_log(1, "rtl: la configuración de la controladora (BAR1 %08X) no tiene la capacidad de energía\n", base1);
}

int rtl_monta(struct radio **r)
{
	int e;

	if (rtl_plat_prueba)
		return rtl_arranca(rtl_plat_prueba, r);
	if (base1)
		d0_por_bar1();
	if (n_gpio) {
		hl_log(2, "rtl: se enciende la controladora y el chip como lo hizo ACPI (%d escrituras)\n", n_gpio);
		enciende_como_acpi();
	}
	regs = ioremap(base, 0x1000);
	if (!regs) {
		hl_log(0, "rtl: no se ven los registros de %08X\n", base);
		return -1;
	}
	hl_log(2, "rtl: la controladora SDIO, en %08X (de %s). (Si el equipo se queda colgado justo aquí, estaba apagada: ver docs/WIFI.md)\n", base, origen);
	e = rtl_arranca(&plat, r);
	if (e == -2)
		hl_log(0, "rtl: el chip del Wi-Fi no contesta: estará apagado. Lo enciende ACPI (el _PS0 de la controladora y de su hijo): %s\n",
		       n_gpio ? "repetir lo aprendido no ha bastado" : "arranque una vez con la entrada «con ACPI» para que se aprenda (\\SDIO.TXT)");
	return e;
}
