/* anfitrion.c - Prueba del núcleo ACPI (acpi/) en el equipo de compilación,
 * con las tablas ACPI volcadas de un portátil de verdad y un controlador
 * embebido simulado (maquina_sim.c). Es el mismo código de 32 bits que lleva
 * HSPACPI.BIN (ACPICA y acpi/nucleo/), con otra «máquina» debajo.
 *
 *   acpi_sim [-v] [-n lecturas] [-o opciones] [-osi n] [-cada s]
 *            -t TABLA.BIN ... [-pci BB DD F ARCHIVO.BIN] ... [-e ESCENARIO]
 *
 * Las tablas: la FACP, la DSDT, la FACS y las SSDT (por su firma). Se les
 * hace una RSDP y una XSDT. El escenario (texto):
 *   ec DIR VALOR       un registro del controlador embebido (en hexadecimal)
 *   ec16 DIR VALOR     dos seguidos (primero el octeto bajo)
 *   es PUERTO VALOR    lo que se lee de un puerto
 *   evento NN          el controlador embebido tiene el aviso NN pendiente
 *   mudo 1             el controlador embebido deja de contestar (0: vuelve)
 *   @N                 lo que sigue se pone justo antes de la lectura N
 * -m DIR VALOR pone ese octeto en la memoria física (las variables de la BIOS:
 * la GNVS). Con -i2chid, al acabar, los dispositivos HID sobre I2C (acpi/pentium/i2chid.c,
 * lo que ACPI.BIN le manda al núcleo de Hispa-Pentium): una línea «I2CHID n: ...».
 * (HPRADIO) Con -hpradio, al acabar, lo de ACPI.BIN con el Wi-Fi de los HP
 * (acpi/pentium/hpradio.c: la WMI de HP, consulta 1Bh); -hpradiover, solo leer.
 * Escribe el registro (lo que en el equipo es BATERIA.LOG) y, por cada
 * lectura, una línea «APM n: ...» con lo que contestaría INT 15h AX=530Ah.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "nucleo.h"
#include "sim.h"
#include "../../acpi/pentium/pentium.h"

int tarea_quieta(void);
void hp_radio(int solo_ver);	/* (HPRADIO) acpi/pentium/hpradio.c */

int sim_charla;

static long llamada(long n, long a, long b, long c)
{
	long r;
	__asm__ __volatile__("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}
static void escribe(const char *s, int n) { llamada(4, 1, (long)s, n); }
static void sal(int c) { llamada(1, c, 0, 0); for (;;) ; }
static void di(const char *f, ...)
{
	char b[512];
	va_list a;
	int n;

	va_start(a, f);
	n = vsnprintf(b, sizeof b, f, a);
	va_end(a);
	escribe(b, n > (int)sizeof b - 1 ? (int)sizeof b - 1 : n);
}
static void vuelca_registro(void)
{
	static char b[4096];
	int n;

	while ((n = reg_saca(b, sizeof b)) > 0)
		escribe(b, n);
}
void sim_vuelca(void) { vuelca_registro(); }
void sim_muere(const char *que)
{
	vuelca_registro();
	di("acpi_sim: %s\n", que);
	sal(2);
	for (;;)
		;
}

static u32 lee_archivo(const char *nombre, u8 **p)
{
	long f = llamada(5, (long)nombre, 0, 0), n, t = 0;
	static u8 tmp[512 << 10];

	if (f < 0) {
		di("acpi_sim: no se puede abrir %s\n", nombre);
		sal(2);
	}
	while ((n = llamada(3, f, (long)(tmp + t), sizeof tmp - t)) > 0)
		t += n;
	llamada(6, f, 0, 0);
	*p = sim_trozo(t + 16);
	memcpy(*p, tmp, t);
	return t;
}

static void suma(u8 *t, u32 n, u32 donde)
{
	u8 s = 0;
	u32 i;

	t[donde] = 0;
	for (i = 0; i < n; i++)
		s += t[i];
	t[donde] = -s;
}
static void pon32(u8 *p, u32 v) { memcpy(p, &v, 4); }

/* ---- PCI ---- */
#define NPCI 40
static struct { u32 bdf; u8 *cfg; } pci[NPCI];
static int npci;
u32 sim_pci_lee(u32 cf8, u32 desp, u32 n)
{
	int i;
	u32 v = 0xFFFFFFFF, r = (cf8 & 0xFC) + desp;

	if (cf8 & 0x80000000)
		for (i = 0; i < npci; i++)
			if (pci[i].bdf == ((cf8 >> 8) & 0xFFFF)) {
				v = 0;
				memcpy(&v, pci[i].cfg + r, r + n > 256 ? 256 - r : n);
				return v;
			}
	return n == 1 ? 0xFF : n == 2 ? 0xFFFF : v;
}

/* ---- el escenario ---- */
static char *esc;
static u32 esc_n;
static void escenario(u32 etapa)
{
	char *p = esc, *fin = esc + esc_n;
	u32 ahora = 1;

	while (p && p < fin) {
		char *l = p, *q;
		u32 a, b;

		while (p < fin && *p != '\n')
			p++;
		if (p < fin)
			p++;
		while (*l == ' ' || *l == '\t')
			l++;
		if (*l == '@') {
			ahora = strtoul(l + 1, &q, 10);
			continue;
		}
		if (ahora != etapa || *l == '#' || *l == '\n' || *l == '\r')
			continue;
		q = l;
		while (*q > ' ')
			q++;
		a = strtoul(q, &q, 16);
		b = strtoul(q, &q, 16);
		if (!strncmp(l, "ec16 ", 5)) {
			sim_ec[a & 255] = b;
			sim_ec[(a + 1) & 255] = b >> 8;
		} else if (!strncmp(l, "ec ", 3))
			sim_ec[a & 255] = b;
		else if (!strncmp(l, "es ", 3))
			sim_es_fija(a, b);
		else if (!strncmp(l, "evento ", 7))
			sim_evento(a);
		else if (!strncmp(l, "mudo ", 5))
			sim_ec_mudo = a;
		else
			sim_muere("escenario: línea que no se entiende");
	}
}

static u8 mem_acpica[6 << 20];
static u8 baja[0x1000], bios[0x20000];
static struct ll_inicio in;
static struct ll_apm ap;
static struct ll_salud sa;
static struct ll_estado es;
static u32 pet[4];
#define FIS_TABLAS 0x7FE00000u

void entrada_c(long *pila)
{
	int argc = pila[0], i, lecturas = 3, ntab = 0, con_i2chid = 0, npoke = 0, con_hp = -1;
	u32 poke_d[32], poke_v[32];
	char **argv = (char **)(pila + 1);
	u8 *xsdt, *rsdp, *fadt = 0;
	u32 fis = FIS_TABLAS, dsdt = 0, facs = 0, hechas = 0, vueltas = 0, nx = 0;
	u32 dir_x[32];

	in.osi = LL_OSI_XP;
	in.cada_s = 10;
	sim_region(0, sizeof baja, baja);
	sim_region(0xE0000, sizeof bios, bios);
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v"))
			sim_charla = 1, in.opciones |= LL_OP_DETALLE;
		else if (!strcmp(argv[i], "-m") && i + 2 < argc && npoke < 32) {
			poke_d[npoke] = strtoul(argv[i + 1], 0, 16);
			poke_v[npoke++] = strtoul(argv[i + 2], 0, 16);
			i += 2;
		} else if (!strcmp(argv[i], "-i2chid"))
			con_i2chid = 1;
		else if (!strcmp(argv[i], "-hpradio"))
			con_hp = 0;		/* (HPRADIO) */
		else if (!strcmp(argv[i], "-hpradiover"))
			con_hp = 1;
		else if (!strcmp(argv[i], "-n") && i + 1 < argc)
			lecturas = strtoul(argv[++i], 0, 10);
		else if (!strcmp(argv[i], "-o") && i + 1 < argc)
			in.opciones |= strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "-osi") && i + 1 < argc)
			in.osi = strtoul(argv[++i], 0, 10);
		else if (!strcmp(argv[i], "-cada") && i + 1 < argc)
			in.cada_s = strtoul(argv[++i], 0, 10);
		else if (!strcmp(argv[i], "-e") && i + 1 < argc)
			esc_n = lee_archivo(argv[++i], (u8 **)&esc);
		else if (!strcmp(argv[i], "-pci") && i + 4 < argc && npci < NPCI) {
			u32 b = strtoul(argv[i + 1], 0, 16), d = strtoul(argv[i + 2], 0, 16),
			    f = strtoul(argv[i + 3], 0, 16);
			u8 *c;
			u32 n = lee_archivo(argv[i + 4], &c);

			pci[npci].cfg = sim_trozo(256);
			memcpy(pci[npci].cfg, c, n > 256 ? 256 : n);
			pci[npci++].bdf = b << 8 | d << 3 | f;
			i += 4;
		} else if (!strcmp(argv[i], "-t") && i + 1 < argc) {
			u8 *t;
			u32 n = lee_archivo(argv[++i], &t);

			if (n < 36 && memcmp(t, "FACS", 4))
				sim_muere("tabla demasiado corta");
			sim_region(fis, n, t);
			if (!memcmp(t, "DSDT", 4))
				dsdt = fis;
			else if (!memcmp(t, "FACS", 4))
				facs = fis;
			else if (memcmp(t, "RSD ", 4) && memcmp(t, "XSDT", 4) && memcmp(t, "RSDT", 4)) {
				if (!memcmp(t, "FACP", 4))
					fadt = t;
				if (nx < 32)
					dir_x[nx++] = fis;
			}
			fis += (n + 0xFFF) & ~0xFFFu;
			ntab++;
		} else {
			di("acpi_sim: opción desconocida: %s\n", argv[i]);
			sal(2);
		}
	}
	if (ntab) {
		if (!fadt || !dsdt)
			sim_muere("faltan la FACP o la DSDT");
		pon32(fadt + 40, dsdt);
		pon32(fadt + 36, facs);
		if (*(u32 *)(fadt + 4) >= 148) {
			memset(fadt + 132, 0, 16);
			pon32(fadt + 132, facs);
			pon32(fadt + 140, dsdt);
		}
		suma(fadt, *(u32 *)(fadt + 4), 9);
		xsdt = sim_trozo(36 + 8 * nx);
		memcpy(xsdt, "XSDT", 4);
		pon32(xsdt + 4, 36 + 8 * nx);
		xsdt[8] = 1;
		memcpy(xsdt + 10, "HISPA SIMULADO", 14);
		for (i = 0; i < (int)nx; i++)
			pon32(xsdt + 36 + 8 * i, dir_x[i]);
		suma(xsdt, 36 + 8 * nx, 9);
		sim_region(fis, 36 + 8 * nx, xsdt);
		rsdp = bios + 0x10000;
		memcpy(rsdp, "RSD PTR ", 8);
		memcpy(rsdp + 9, "HISPA ", 6);
		rsdp[15] = 2;
		pon32(rsdp + 20, 36);
		pon32(rsdp + 24, fis);
		suma(rsdp, 20, 8);
		suma(rsdp, 36, 32);
		sim_rsdp = 0xF0000;
	}
	mem_inicia(mem_acpica, sizeof mem_acpica);
	sim_region((u32)&in, sizeof in, &in);
	sim_region((u32)&ap, sizeof ap, &ap);
	sim_region((u32)&sa, sizeof sa, &sa);
	sim_region((u32)&es, sizeof es, &es);
	sim_region((u32)pet, sizeof pet, pet);
	in.dir_apm = (u32)&ap;
	in.dir_salud = (u32)&sa;
	for (i = 0; i < npoke; i++)
		*(u8 *)mq_mapa(poke_d[i], 1) = poke_v[i];
	escenario(1);
	if (nucleo_llamada(LL_INICIA, (u32)&in))
		sim_muere("LL_INICIA falla");
	for (;;) {
		int r = nucleo_llamada(LL_PASO, 0);

		vuelca_registro();
		if (r < 0) {
			di("FIN: la tarea ha muerto (%d)\n", r);
			sal(1);
		}
		if (ap.fase >= FASE_SIN_BATERIA) {
			di("APM: fase=%u error=%u\n", ap.fase, ap.error);
			break;
		}
		if (ap.lecturas != hechas) {
			hechas = ap.lecturas;
			di("APM %u: fase=%u ca=%02X estado=%02X banderas=%02X pct=%u min=%u nbat=%u\n", hechas,
			   ap.fase, ap.ca, ap.estado, ap.banderas, ap.pct,
			   ap.minutos == 0xFFFF ? 0 : ap.minutos & 0x7FFF, ap.nbat);
			di("SALUD %u: pct=%u nbat=%u", hechas, sa.pct, sa.nbat);
			for (i = 0; i < LL_NBAT; i++)
				if (sa.bat[i].valida)
					di(" [%d: %u de %u %s, ciclos=%d]", i + 1, sa.bat[i].llena, sa.bat[i].diseno,
					   sa.bat[i].unidad ? "mAh" : "mWh", sa.bat[i].ciclos == 0xFFFF ? -1 : sa.bat[i].ciclos);
			di("\n");
			if ((int)hechas >= lecturas)
				break;
			escenario(hechas + 1);
		}
		if (++vueltas > 200000)
			sim_muere("la tarea no acaba");
		sim_avanza_us(r * 54925);
	}
	if (con_i2chid) {
		static struct pe_i2chid f[I2CHID_MAX];
		int n, k;

		for (k = 0; k < 2000 && !tarea_quieta(); k++)
			if (nucleo_llamada(LL_PASO, 0) < 0)
				break;
		n = i2chid_busca(f, I2CHID_MAX);
		vuelca_registro();
		for (k = 0; k < n; k++)
			di("I2CHID %d: %s hid=%s cid=%s sta=%X esclavo=%02X velocidad=%u diez=%u fuente=%s adr=%08X pci=%06X "
			   "registro=%X gpio=%d/%X ss=%u/%u/%u fm=%u/%u/%u origen=%u\n", k + 1, f[k].ruta, f[k].hid, f[k].cid, f[k].sta,
			   f[k].esclavo, f[k].velocidad, f[k].diez, f[k].fuente, f[k].adr, f[k].pci, f[k].registro,
			   (int)f[k].gpio_pin, f[k].gpio_modo, f[k].ss[0], f[k].ss[1], f[k].ss[2], f[k].fm[0], f[k].fm[1], f[k].fm[2], f[k].origen);
	}
	if (con_hp >= 0) {
		/* (HPRADIO) lo de ACPI.BIN con el Wi-Fi de los HP, contra su AML */
		int k;

		for (k = 0; k < 2000 && !tarea_quieta(); k++)
			if (nucleo_llamada(LL_PASO, 0) < 0)
				break;
		hp_radio(con_hp);
		vuelca_registro();
	}
	pet[0] = (u32)&es;
	pet[1] = sizeof es;
	nucleo_llamada(LL_ESTADO, (u32)pet);
	for (i = 0; i < LL_NBAT; i++)
		if (es.bat[i].hay)
			di("BAT %d: %s sta=%X valida=%u unidad=%u diseno=%u llena=%u queda=%u ritmo=%u voltaje=%u estado=%u "
			   "modelo=[%s] tipo=[%s] marca=[%s]\n", i + 1, es.bat[i].ruta, es.bat[i].sta, es.bat[i].valida,
			   es.bat[i].unidad, es.bat[i].diseno, es.bat[i].llena, es.bat[i].queda, es.bat[i].ritmo,
			   es.bat[i].voltaje, es.bat[i].estado, es.bat[i].modelo, es.bat[i].tipo, es.bat[i].marca);
	di("EC: hay=%u %02X/%02X lecturas=%u escrituras=%u fallos=%u eventos=%u; CA: hay=%u %u [%s]\n", es.ec_hay,
	   es.ec_datos, es.ec_orden, es.ec_lecturas, es.ec_escrituras, es.ec_fallos, es.ec_eventos, es.ca_hay, es.ca,
	   es.ca_ruta);
	di("MEMORIA: %u KB ahora, %u KB como mucho; ACPICA: %u avisos o errores; páginas a cero: %u\n",
	   es.memoria >> 10, es.memoria_max >> 10, es.errores_acpica, sim_paginas);
	if (sim_charla) {
		di("EC tocado (L leído, E escrito):");
		for (i = 0; i < 256; i++)
			if (sim_ec_tocado[i])
				di(" %02X%s%s", i, sim_ec_tocado[i] & 1 ? "L" : "", sim_ec_tocado[i] & 2 ? "E" : "");
		di("\n");
	}
	sal(0);
}

__asm__(".globl _start\n_start:\n\tmovl %esp, %eax\n\tandl $-16, %esp\n\tsubl $12, %esp\n\tpushl %eax\n\tcall entrada_c\n");
