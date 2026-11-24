/* potencia.c - (TERMO) la potencia y los frenos del procesador (potencia.h).
 *
 * Para el MSI GT80 Titan (i7-4720HQ/4980HQ, Haswell): se congela y no tiene
 * fuerza para cargar Windows; ¿lo frena la fuente (el cargador) o un límite
 * de potencia? Todo es solo leer, salvo poner a 0 las marcas de registro de
 * los motivos de freno (volátiles: se borran solas al apagar), para saber
 * qué ha pasado en cada rato de 2 s. Ni límites ni frecuencias se tocan.
 *
 *  1. Los vatios (RAPL, como intel_rapl de Linux y turbostat): las unidades
 *     de MSR_RAPL_POWER_UNIT (606h; en los Atom Silvermont, en µJ y mW, como
 *     rapl_check_unit_atom), la energía del paquete (611h), los núcleos
 *     (PP0, 639h), la gráfica integrada (PP1, 641h), la memoria (DRAM,
 *     619h; en los de servidor, a 15,3 µJ fijos) y, de Skylake en adelante,
 *     el equipo entero (PSys, 64Dh). Contadores de 32 bits: la resta sin
 *     signo da bien una vuelta. Y el tiempo frenado por RAPL (PKG_PERF_STATUS
 *     613h). En AMD Zen (familia 17h en adelante): C0010299h (unidades),
 *     C001029Bh (paquete) y C001029Ah (este núcleo); sus límites (PPT, TDC,
 *     EDC) los guarda el SMU y no se leen por MSR: se dice.
 *  2. Los límites, al abrir: PKG_POWER_LIMIT (610h: PL1 y PL2, con sus
 *     tiempos, y si está bloqueado), PKG_POWER_INFO (614h: el TDP),
 *     MSR_PLATFORM_INFO (CEh: la relación base y la mínima), las de turbo
 *     (1ADh), el TDP configurable (648h a 64Ch, si CEh dice que lo hay) y,
 *     de Skylake en adelante, el límite del equipo (PSys, 65Ch).
 *  3. Por qué se frena: IA32_THERM_STATUS (19Ch, el núcleo que corre),
 *     IA32_PACKAGE_THERM_STATUS (1B1h) y los PERF_LIMIT_REASONS: Haswell y
 *     Broadwell, 690h (núcleo), 6B0h (gráfica) y 6B1h (anillo); Skylake en
 *     adelante, 64Fh. Cada bit de estado (lo de ahora) y su marca de
 *     registro (lo que ha pasado desde la última vez que se puso a 0): se
 *     cuenta en qué parte de los ratos de 2 s de cada fase ha pasado.
 *  4. Los estados de reposo (C-states, como turbostat por modelo): el
 *     paquete en PC2 (60Dh), PC3 (3F8h), PC6 (3F9h), PC7 (3FAh) y PC8 a
 *     PC10 (630h-632h); el núcleo en CC3, CC6 y CC7 (3FCh-3FEh). Van al
 *     ritmo del TSC: lo que suman frente al TSC es el tanto por ciento.
 * Si un MSR da la excepción 13, «no lo tiene»; si da siempre 0 (QEMU sin
 * KVM lo hace con los que no conoce), «no cuenta». Nunca se para por eso.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "potencia.h"

typedef uint32_t u32;
typedef uint64_t u64;

#define NO_T (-100000)
static struct po_lector L;
static int p_intel, p_amd, p_fam, p_modelo, empezado;

static void dice(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void suceso(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void dice(const char *fmt, ...)
{
	char t[400];
	va_list v;

	if (!L.serie)
		return;
	va_start(v, fmt);
	vsnprintf(t, sizeof t, fmt, v);
	va_end(v);
	L.serie(t);
}

static void suceso(const char *fmt, ...)
{
	char t[600];
	va_list v;

	if (!L.suceso)
		return;
	va_start(v, fmt);
	vsnprintf(t, sizeof t, fmt, v);
	va_end(v);
	L.suceso(t);
}

static int lee(u32 m, u32 *lo, u32 *hi)
{
	u32 a = 0, b = 0;

	if (!L.lee || !L.lee(m, &a, &b))
		return 0;
	*lo = a;
	if (hi)
		*hi = b;
	return 1;
}

/* «41,2» de miliunidades (mW) */
static char *w1(char *d, u64 mw)
{
	u64 x = (mw + 50) / 100;

	snprintf(d, 16, "%u,%u", (u32)(x / 10), (u32)(x % 10));
	return d;
}

/* s y espacios hasta «ancho» letras (UTF-8: las tildes son dos octetos) */
static const char *pad(char *d, int cabe, const char *s, int ancho)
{
	int n = snprintf(d, (size_t)cabe, "%s", s), letras = 0;
	const char *q;

	for (q = s; *q; q++)
		if ((*q & 0xC0) != 0x80)
			letras++;
	while (letras < ancho && n < cabe - 1)
		d[n++] = ' ', letras++;
	d[n < cabe ? n : cabe - 1] = 0;
	return d;
}

/* ---------------------------------------------------------------- los modelos */
static int en(int m, const int *l)
{
	for (; *l; l++)
		if (*l == m)
			return 1;
	return 0;
}
static const int m_atom_u[] = { 0x37, 0x4A, 0x5A, 0x5D, 0x4C, 0 };	/* unidades en µJ/mW (Linux: byt, tng, ann, cht) */
static const int m_slm[] = { 0x37, 0x4A, 0x4D, 0x5A, 0x5D, 0x4C, 0 };	/* Silvermont/Airmont: PC6 en 3FAh */
static const int m_nhm[] = { 0x1A, 0x1E, 0x1F, 0x2E, 0x25, 0x2C, 0x2F, 0 };
static const int m_srv[] = { 0x2D, 0x3E, 0x3F, 0x4F, 0x56, 0x55, 0x57, 0x85, 0x6A, 0x6C, 0x8F, 0xCF, 0xAD, 0xAE, 0 };
static const int m_dram_fija[] = { 0x3F, 0x4F, 0x56, 0x55, 0x57, 0x85, 0x6A, 0x6C, 0x8F, 0 };	/* DRAM a 15,3 µJ */
static const int m_hsw[] = { 0x3C, 0x45, 0x46, 0x3D, 0x47, 0x3F, 0x4F, 0x56, 0 };	/* 690h */
static const int m_hsw_cli[] = { 0x3C, 0x45, 0x46, 0x3D, 0x47, 0 };			/* 6B0h, 6B1h */
static const int m_c8[] = { 0x45, 0x3D, 0x47, 0x4E, 0x5E, 0x8E, 0x9E, 0xA5, 0xA6, 0x66, 0x7D, 0x7E, 0x8C, 0x8D, 0x97, 0x9A, 0xA7, 0xAA, 0xAC,
			    0xB7, 0xBA, 0xBF, 0xBD, 0xC5, 0xC6, 0 };
static const int m_skl[] = { 0x4E, 0x5E, 0x8E, 0x9E, 0xA5, 0xA6, 0x66, 0x7D, 0x7E, 0x8C, 0x8D, 0x97, 0x9A, 0xA7, 0xAA, 0xAC, 0xB7, 0xBA, 0xBF,
			     0xBD, 0xC5, 0xC6, 0 };

static int es_skl(void)
{
	return p_intel && p_fam == 6 && (en(p_modelo, m_skl) || (p_modelo > 0x4E && !en(p_modelo, m_srv) && !en(p_modelo, m_slm) && !en(p_modelo, m_hsw)
		&& p_modelo != 0x55 && p_modelo != 0x5C && p_modelo != 0x5F && p_modelo != 0x7A && p_modelo != 0x86 && p_modelo != 0x96 && p_modelo != 0x9C));
}

/* ---------------------------------------------------------------- RAPL */
enum { D_PAQ, D_NUC, D_GPU, D_MEM, D_EQUIPO, ND };
static const char *const dom_nombre[ND] = { "Procesador entero (paquete)", "Núcleos (PP0)", "Gráfica integrada (PP1)", "Memoria RAM (DRAM)",
					    "Equipo entero (PSys)" };
static const char *const dom_corto[ND] = { "paq", "nuc", "gpu", "mem", "equipo" };
static struct dominio {
	u32 msr;
	int hay;		/* -1 no lo tiene; 1 se lee */
	int movido;		/* ha contado algo */
	u32 ultimo;
	u64 uj[3], us[3];	/* por fase: la energía (µJ) y el tiempo (µs) */
	u32 mw_ahora, mw_max[3];
} D[ND];
static int rapl;		/* 0 no; 1 Intel; 2 AMD */
static u32 u606, e_esu, e_pu, e_tu;
static int atom_u;
static int perf613 = -1;	/* PKG_PERF_STATUS: -1 no; 1 sí */
static u32 perf613_ult;
static u64 perf613_us[3];
static int pct613_ahora = -1;
/* la carga, rato a rato: lo del paquete (mW), para la media del final */
#define NRING 30
static u32 ring[NRING];
static int nring, pring;
static u64 us_fase[3], tsc_fase[3];
static int n_fase[3];

static u64 a_uj(int d, u32 delta)
{
	if (d == D_MEM && en(p_modelo, m_dram_fija) && p_intel)
		return (u64)delta * 1000000u >> 16;
	if (atom_u)
		return (u64)delta << e_esu;
	return (u64)delta * 1000000u >> e_esu;
}

static u64 a_mw(u32 x)		/* una potencia en unidades de 606h, en mW */
{
	if (atom_u)
		return (u64)x << e_pu;
	return (u64)x * 1000u >> e_pu;
}

static u64 a_us(u64 x)		/* un tiempo en unidades de 606h, en µs */
{
	return x * 1000000u >> e_tu;
}

/* ---------------------------------------------------------------- los límites */
static char lim_txt[1800];
static int nlim;
static u32 pl1_mw, pl2_mw, tdp_mw, pl1_en, pl_bloq, psys_pl1_mw;
static int base_ratio, min_ratio, bus_mhz;
#define LI(...) (nlim += snprintf(lim_txt + nlim, sizeof lim_txt - (size_t)nlim, __VA_ARGS__), nlim = nlim < (int)sizeof lim_txt ? nlim : (int)sizeof lim_txt - 1)

static char *tiempo_ms(char *d, u64 ms)
{
	if (ms >= 10000)
		snprintf(d, 24, "%u s", (u32)(ms / 1000));
	else if (ms >= 1000)
		snprintf(d, 24, "%u,%u s", (u32)(ms / 1000), (u32)(ms % 1000 / 100));
	else
		snprintf(d, 24, "%u ms", (u32)ms);
	return d;
}

/* la ventana de tiempo de un límite: 2^Y * (1 + Z/4) unidades */
static u64 ventana_ms(u32 v)
{
	u32 y = v & 0x1F, z = v >> 5 & 3;

	return ((u64)1 << y) * (4 + z) * 1000u / 4 >> e_tu;
}

static char *mhz(char *d, int ratio)
{
	if (bus_mhz && ratio)
		snprintf(d, 32, " (%d MHz)", ratio * bus_mhz);
	else
		d[0] = 0;
	return d;
}

static void lee_limites(void)
{
	u32 lo, hi;
	char a[24], b[24], c[16], e[16], m1[32];

	nlim = 0;
	lim_txt[0] = 0;
	if (p_amd) {
		LI("  Límites: en AMD (PPT, TDC, EDC) los guarda el SMU del procesador: no se leen por MSR.\r\n");
		return;
	}
	if (!p_intel)
		return;
	bus_mhz = en(p_modelo, m_nhm) ? 133 : en(p_modelo, m_slm) ? 0 : 100;
	if (lee(0xCE, &lo, &hi) && (lo || hi)) {
		base_ratio = (int)(lo >> 8 & 0xFF);
		min_ratio = (int)(hi >> 8 & 0xFF);
		LI("  Relación base %d%s, mínima %d%s (MSR_PLATFORM_INFO, CEh)%s%s.\r\n", base_ratio, mhz(m1, base_ratio), min_ratio, mhz(m1 + 16, min_ratio),
		   lo & (1u << 28) ? "; turbo programable" : "", lo & (1u << 29) ? "; límite de TDP programable" : "");
		dice("termo: potencia: CEh relación base %d, mínima %d\n", base_ratio, min_ratio);
	} else
		LI("  MSR_PLATFORM_INFO (CEh): no lo tiene.\r\n");
	if (!en(p_modelo, m_slm) && lee(0x1AD, &lo, &hi) && (lo || hi)) {
		int i, n = 0;
		char t[200];

		t[0] = 0;
		for (i = 0; i < 8; i++) {
			u32 r = (i < 4 ? lo >> (8 * i) : hi >> (8 * (i - 4))) & 0xFF;

			if (r)
				n += snprintf(t + n, sizeof t - (size_t)n, "%s%d %s: %u", n ? ", " : "", i + 1, i ? "núcleos" : "núcleo", r);
		}
		LI("  Turbo (relación máxima con tantos núcleos activos, 1ADh%s): %s%s.\r\n", en(p_modelo, m_srv) ? ", en los de servidor por grupos" : "", t,
		   bus_mhz ? " (por 100 MHz)" : "");
		dice("termo: potencia: 1ADh %08X%08X\n", hi, lo);
	} else
		LI("  Relaciones de turbo (1ADh): no las dice.\r\n");
	if (!rapl) {
		LI("  Límites de potencia (610h, 614h): sin RAPL no se pueden leer.\r\n");
		return;
	}
	if (lee(0x614, &lo, &hi) && lo) {
		tdp_mw = (u32)a_mw(lo & 0x7FFF);
		LI("  TDP (la potencia de diseño, PKG_POWER_INFO 614h): %s W", w1(c, tdp_mw));
		if (lo >> 16 & 0x7FFF)
			LI("; mínima %s W", w1(c, a_mw(lo >> 16 & 0x7FFF)));
		if (hi & 0x7FFF)
			LI("; máxima %s W", w1(c, a_mw(hi & 0x7FFF)));
		LI(".\r\n");
	} else
		LI("  TDP (614h): no lo dice.\r\n");
	if (lee(0x610, &lo, &hi) && (lo || hi)) {
		pl1_mw = (u32)a_mw(lo & 0x7FFF);
		pl2_mw = (u32)a_mw(hi & 0x7FFF);
		pl1_en = lo >> 15 & 1;
		pl_bloq = hi >> 31;
		LI("  PL1 (carga larga): %s W en %s, %s%s; PL2 (picos): %s W en %s, %s%s; %s (PKG_POWER_LIMIT 610h).\r\n", w1(c, pl1_mw), tiempo_ms(a, ventana_ms(lo >> 17 & 0x7F)),
		   pl1_en ? "activo" : "APAGADO", lo & (1u << 16) ? ", con freno aunque baje del mínimo (clamp)" : "", w1(e, pl2_mw), tiempo_ms(b, ventana_ms(hi >> 17 & 0x7F)),
		   hi & (1u << 15) ? "activo" : "apagado", hi & (1u << 16) ? ", clamp" : "", pl_bloq ? "bloqueado por la BIOS hasta apagar" : "sin bloquear");
		if (tdp_mw && pl1_en && pl1_mw && pl1_mw < tdp_mw * 85 / 100)
			LI("  OJO: el PL1 que ha puesto la BIOS (%s W) está por debajo del TDP del procesador (%s W): el fabricante lo limita.\r\n", w1(c, pl1_mw), w1(e, tdp_mw));
		LI("  (La placa puede poner otro límite aparte, en el registro MMIO de MCHBAR 59A0h, que el Termómetro no lee: manda el más bajo de los dos.)\r\n");
		suceso("potencia: límites de la BIOS: PL1 %s W (%s), PL2 %s W; TDP %s W; %s", w1(c, pl1_mw), pl1_en ? "activo" : "apagado", w1(e, pl2_mw),
		       tdp_mw ? w1(a, tdp_mw) : "?", pl_bloq ? "bloqueado" : "sin bloquear");
	} else
		LI("  Límites de potencia (610h): no los dice.\r\n");
	if (es_skl() && lee(0x65C, &lo, &hi) && (lo || hi)) {
		psys_pl1_mw = (u32)a_mw(lo & 0x7FFF);
		LI("  Límite del equipo entero (PSys, 65Ch): PL1 %s W %s, PL2 %s W %s.\r\n", w1(c, psys_pl1_mw), lo & (1u << 15) ? "activo" : "apagado",
		   w1(e, a_mw(hi & 0x7FFF)), hi & (1u << 15) ? "activo" : "apagado");
	}
	if (lee(0xCE, &lo, &hi) && (hi >> 1 & 3)) {
		u32 l2, h2;

		LI("  TDP configurable (%u niveles más):", hi >> 1 & 3);
		if (lee(0x648, &l2, &h2))
			LI(" nominal relación %u;", l2 & 0xFF);
		if (lee(0x649, &l2, &h2) && l2)
			LI(" nivel 1 %s W, relación %u;", w1(c, a_mw(l2 & 0x7FFF)), l2 >> 16 & 0xFF);
		if (lee(0x64A, &l2, &h2) && l2)
			LI(" nivel 2 %s W, relación %u;", w1(c, a_mw(l2 & 0x7FFF)), l2 >> 16 & 0xFF);
		if (lee(0x64B, &l2, &h2))
			LI(" en uso el nivel %u%s;", l2 & 3, l2 >> 31 ? " (bloqueado)" : "");
		if (lee(0x64C, &l2, &h2))
			LI(" turbo desde la relación %u%s", l2 & 0xFF, l2 >> 31 ? " (bloqueado)" : "");
		LI(".\r\n");
	}
}

/* ---------------------------------------------------------------- los frenos */
enum { C_TEMP, C_CRIT, C_PROCHOT, C_CORR, C_VRTEMP, C_PL1, C_PL2, C_PLN, C_PP0, C_PP1, C_GRAF, C_AUTO, C_TURBO, C_TRANS, C_CRUZ, NC };
static const char *const cat_texto[NC] = {
	"temperatura (el procesador en su límite, TjMax)",
	"temperatura crítica",
	"PROCHOT desde fuera (la placa pide frenar: el cargador, la batería, el regulador o la gráfica)",
	"límite de corriente del regulador de tensión (VR, EDP)",
	"el regulador de tensión de la placa, caliente (VR Therm Alert)",
	"límite de potencia PL1 (el de carga larga)",
	"límite de potencia PL2 (el de picos cortos)",
	"aviso de límite de potencia (sin decir cuál)",
	"límite de potencia de los núcleos (PP0)",
	"límite de potencia de la gráfica integrada",
	"lo pide el controlador de la gráfica",
	"ahorro automático del procesador (según el uso)",
	"turbo con varios núcleos a la vez (con más núcleos, menos relación: normal)",
	"cambios de frecuencia muy seguidos (atenuación del turbo: normal)",
	"límite de otra parte del procesador (cross-domain)",
};
static const char *const cat_corto[NC] = { "temp", "critica", "PROCHOT", "corriente", "VRcaliente", "PL1", "PL2", "aviso_pot", "PP0", "PP1", "graf", "auto",
					   "turbo", "transic", "cruzado" };
/* de la placa (apunta a la alimentación), de potencia, de temperatura, normal */
static const char cat_clase[NC] = { 't', 't', 'p', 'p', 'p', 'w', 'w', 'w', 'w', 'w', 'n', 'n', 'n', 'n', 'n' };

struct bit { signed char estado, log, cat; };
static const struct bit b19c[] = { { 0, 1, C_TEMP }, { 2, 3, C_PROCHOT }, { 4, 5, C_CRIT }, { 10, 11, C_PLN }, { 12, 13, C_CORR }, { 14, 15, C_CRUZ }, { -1, 0, 0 } };
static const struct bit b1b1[] = { { 0, 1, C_TEMP }, { 2, 3, C_PROCHOT }, { 4, 5, C_CRIT }, { 10, 11, C_PLN }, { -1, 0, 0 } };
static const struct bit b690[] = { { 0, 16, C_PROCHOT }, { 1, 17, C_TEMP }, { 4, 20, C_GRAF }, { 5, 21, C_AUTO }, { 6, 22, C_VRTEMP }, { 8, 24, C_CORR },
				   { 9, 25, C_PP0 }, { 10, 26, C_PL1 }, { 11, 27, C_PL2 }, { 12, 28, C_TURBO }, { 13, 29, C_TRANS }, { -1, 0, 0 } };
static const struct bit b6b0[] = { { 0, 16, C_PROCHOT }, { 1, 17, C_TEMP }, { 4, 20, C_GRAF }, { 5, 21, C_AUTO }, { 6, 22, C_VRTEMP }, { 8, 24, C_CORR },
				   { 9, 25, C_PP1 }, { 10, 26, C_PL1 }, { 11, 27, C_PL2 }, { -1, 0, 0 } };
static const struct bit b6b1[] = { { 0, 16, C_PROCHOT }, { 1, 17, C_TEMP }, { 6, 22, C_VRTEMP }, { 8, 24, C_CORR }, { 10, 26, C_PL1 }, { 11, 27, C_PL2 }, { -1, 0, 0 } };
static const struct bit b64f[] = { { 0, 16, C_PROCHOT }, { 1, 17, C_TEMP }, { 4, 20, C_AUTO }, { 5, 21, C_TEMP }, { 6, 22, C_VRTEMP }, { 7, 23, C_CORR },
				   { 8, 24, C_CORR }, { 10, 26, C_PL1 }, { 11, 27, C_PL2 }, { 12, 28, C_TURBO }, { 13, 29, C_TRANS }, { -1, 0, 0 } };

/* las partes: el núcleo y el paquete (19Ch, 1B1h, 690h, 64Fh), la gráfica (6B0h), el anillo (6B1h) */
#define NG 3
static const char *const grupo_nombre[NG] = { "el procesador (núcleos y paquete)", "la gráfica integrada", "el anillo (la caché compartida)" };
static struct fuente {
	u32 msr;
	const struct bit *b;
	int grupo;
	int hay;		/* -1 no lo tiene, 0 sin mirar, 1 se lee */
	int log_vale;		/* las marcas se dejan poner a 0 (si no, solo cuenta el estado) */
	u32 antes;		/* lo anotado al abrir (de antes) */
	u32 primero;		/* lo leído al abrir */
} F[] = {
	{ 0x19C, b19c, 0, 0, 0, 0, 0 }, { 0x1B1, b1b1, 0, 0, 0, 0, 0 }, { 0x690, b690, 0, 0, 0, 0, 0 }, { 0x64F, b64f, 0, 0, 0, 0, 0 },
	{ 0x6B0, b6b0, 1, 0, 0, 0, 0 }, { 0x6B1, b6b1, 2, 0, 0, 0, 0 },
};
#define NF (int)(sizeof F / sizeof F[0])
static u32 cuenta[NG][NC][3];		/* los ratos de 2 s, por fase, en que ha pasado */
static int ahora[NG][NC];		/* el último rato */
static int antes_cat[NC];		/* anotado ya al abrir */
static int visto_cat[NG][NC];		/* (dicho ya en el .LOG) */
static int frenos_hay;			/* alguna fuente se lee */
static int frenos_cero;			/* se leen, pero todo 0 (una máquina virtual) */
static int t_max_carga = NO_T, tj = NO_T, ca_carga = -1, ca_visto[2];

static u32 mascara_log(const struct bit *b)
{
	u32 m = 0;

	for (; b->estado >= 0; b++)
		m |= 1u << b->log;
	return m;
}

/* las marcas cuyo estado sigue puesto (el procesador las vuelve a poner enseguida) */
static u32 estados(const struct bit *b, u32 v)
{
	u32 m = 0;

	for (; b->estado >= 0; b++)
		if (v & (1u << b->estado))
			m |= 1u << b->log;
	return m;
}

static void prepara_frenos(void)
{
	int i;
	u32 lo, hi;

	if (!p_intel)
		return;
	for (i = 0; i < NF; i++) {
		struct fuente *f = &F[i];
		const struct bit *b;
		u32 ml = mascara_log(f->b);

		f->hay = -1;
		if (f->msr == 0x690 && !en(p_modelo, m_hsw))
			continue;
		if ((f->msr == 0x6B0 || f->msr == 0x6B1) && !en(p_modelo, m_hsw_cli))
			continue;
		if (f->msr == 0x64F && !es_skl())
			continue;
		if (!lee(f->msr, &lo, &hi)) {
			dice("termo: potencia: %03Xh (motivos del freno): no lo tiene\n", f->msr);
			continue;
		}
		f->hay = 1;
		frenos_hay = 1;
		f->antes = lo & ml;
		f->primero = lo;
		for (b = f->b; b->estado >= 0; b++)
			if (lo & (1u << b->log))
				antes_cat[(int)b->cat] = 1;
		/* poner a 0 las marcas y ver si se dejan */
		f->log_vale = L.limpia != 0;
		if (f->antes)
			f->log_vale = L.limpia && L.limpia(f->msr) && lee(f->msr, &lo, &hi) && !(lo & ml & f->antes & ~estados(f->b, lo));
		dice("termo: potencia: %03Xh motivos del freno: %08X (anotado de antes %08X; las marcas %s)\n", f->msr, lo, f->antes,
		     f->log_vale ? "se ponen a 0" : f->antes ? "no se dejan poner a 0: solo cuenta lo de ahora" : "no se tocan");
	}
	/* todos a 0 (también 19Ch, que en un procesador de verdad lleva el bit
	 * de «lectura válida»): una máquina virtual que da 0 a lo que no conoce */
	{
		int nada = 1;

		for (i = 0; i < NF; i++)
			if (F[i].hay > 0 && (F[i].antes || F[i].primero))
				nada = 0;
		if (nada && frenos_hay) {
			dice("termo: potencia: los motivos del freno dan todos 0: no los da de verdad (máquina virtual)\n");
			for (i = 0; i < NF; i++)
				F[i].hay = -1;
			frenos_hay = 0;
			frenos_cero = 1;
		}
	}
	{
		int c, n = 0;
		char t[500];

		t[0] = 0;
		for (c = 0; c < NC; c++)
			if (antes_cat[c] && cat_clase[c] != 'n')
				n += snprintf(t + n, sizeof t - (size_t)n, "%s%s", n ? "; " : "", cat_texto[c]);
		if (n)
			suceso("potencia: anotado ya al abrir el Termómetro (de antes: desde que se encendió el equipo) que se ha frenado por: %s", t);
	}
}

static void mira_frenos_po(int fase, int contar)
{
	int i, g, c;
	u32 lo, hi;
	int hoy[NG][NC];

	memset(hoy, 0, sizeof hoy);
	for (i = 0; i < NF; i++) {
		struct fuente *f = &F[i];
		const struct bit *b;
		u32 ml = mascara_log(f->b);

		if (f->hay <= 0)
			continue;
		if (!lee(f->msr, &lo, &hi)) {
			f->hay = -1;
			continue;
		}
		for (b = f->b; b->estado >= 0; b++)
			if ((lo & (1u << b->estado)) || (f->log_vale && (lo & (1u << b->log))))
				hoy[f->grupo][(int)b->cat] = 1;
		if (f->log_vale && (lo & ml)) {
			u32 l2, h2;

			if (!L.limpia(f->msr) || !lee(f->msr, &l2, &h2) || (l2 & ml & lo & ~estados(f->b, lo)))
				f->log_vale = 0;	/* (no se deja: desde ahora, solo lo de ahora) */
		}
	}
	for (g = 0; g < NG; g++)
		for (c = 0; c < NC; c++) {
			ahora[g][c] = hoy[g][c];
			if (!hoy[g][c] || !contar)
				continue;
			cuenta[g][c][fase]++;
			if (!visto_cat[g][c] && cat_clase[c] != 'n') {
				visto_cat[g][c] = 1;
				suceso("potencia: %s se frena por: %s (%s)", grupo_nombre[g], cat_texto[c], fase == PO_CARGA ? "en la carga" : fase == PO_ENFRIA ? "enfriando" : "en reposo");
			}
		}
}

/* ---------------------------------------------------------------- los C-states */
struct cestado { u32 msr; const char *nombre; int paquete; int hay; u64 ult; u64 suma[3]; int pct; };
static struct cestado CE[12];
static int nce, ce_cuenta, ce_algo;

static void pon_ce(u32 msr, const char *nombre, int paquete)
{
	if (nce < (int)(sizeof CE / sizeof CE[0])) {
		CE[nce].msr = msr, CE[nce].nombre = nombre, CE[nce].paquete = paquete;
		nce++;
	}
}

static void prepara_cestados(void)
{
	int i;
	u32 lo = 0, hi = 0;

	if (!p_intel || p_fam != 6)
		return;
	if (en(p_modelo, m_slm)) {
		pon_ce(0x3FA, "PC6", 1);
		pon_ce(0x3FD, "CC6", 0);
	} else if (en(p_modelo, m_nhm)) {
		pon_ce(0x3F8, "PC3", 1);
		pon_ce(0x3F9, "PC6", 1);
		pon_ce(0x3FC, "CC3", 0);
		pon_ce(0x3FD, "CC6", 0);
	} else if (p_modelo >= 0x2A) {
		pon_ce(0x60D, "PC2", 1);
		pon_ce(0x3F8, "PC3", 1);
		pon_ce(0x3F9, "PC6", 1);
		pon_ce(0x3FA, "PC7", 1);
		if (en(p_modelo, m_c8) || es_skl()) {
			pon_ce(0x630, "PC8", 1);
			pon_ce(0x631, "PC9", 1);
			pon_ce(0x632, "PC10", 1);
		}
		pon_ce(0x3FC, "CC3", 0);
		pon_ce(0x3FD, "CC6", 0);
		pon_ce(0x3FE, "CC7", 0);
	}
	for (i = 0; i < nce; i++) {
		CE[i].pct = -1;
		CE[i].hay = lee(CE[i].msr, &lo, &hi) ? 1 : -1;
		CE[i].ult = (u64)hi << 32 | lo;
	}
}

static void mira_cestados(int fase, u64 dtsc)
{
	int i;
	u32 lo, hi;

	for (i = 0; i < nce; i++) {
		u64 v, d;

		if (CE[i].hay <= 0 || !lee(CE[i].msr, &lo, &hi))
			continue;
		v = (u64)hi << 32 | lo;
		d = v - CE[i].ult;
		CE[i].ult = v;
		if (d > dtsc + dtsc / 8)	/* (ha dado la vuelta o se ha puesto a 0: este rato no) */
			continue;
		if (d) {
			ce_algo = 1;
			if (d > dtsc)
				d = dtsc;
		}
		CE[i].suma[fase] += d;
		CE[i].pct = dtsc ? (int)(d * 100 / dtsc) : -1;
	}
	ce_cuenta++;
}

/* ---------------------------------------------------------------- empezar */
static void prepara_rapl(void)
{
	u32 lo, hi;
	int d;
	static const u32 m_amd[ND] = { 0xC001029B, 0xC001029A, 0, 0, 0 };
	static const u32 m_int[ND] = { 0x611, 0x639, 0x641, 0x619, 0x64D };

	rapl = 0;
	if (p_amd && p_fam >= 0x17) {
		if (lee(0xC0010299, &lo, &hi) && lo) {
			rapl = 2;
			u606 = lo;
		}
	} else if (p_intel && p_fam == 6) {
		if (lee(0x606, &lo, &hi) && lo) {
			rapl = 1;
			u606 = lo;
		}
	}
	if (!rapl) {
		dice("termo: potencia: RAPL (%s): no lo tiene\n", p_amd ? "C0010299h" : "606h");
		suceso("potencia: este procesador no dice sus vatios (RAPL: no lo tiene)");
		return;
	}
	atom_u = p_intel && en(p_modelo, m_atom_u);
	e_pu = u606 & 0xF;
	e_esu = u606 >> 8 & 0x1F;
	e_tu = u606 >> 16 & 0xF;
	for (d = 0; d < ND; d++) {
		u32 m = rapl == 2 ? m_amd[d] : m_int[d];

		D[d].msr = m;
		D[d].hay = -1;
		if (!m || (d == D_EQUIPO && !es_skl()) || (d == D_GPU && p_intel && en(p_modelo, m_srv)))
			continue;
		if (lee(m, &lo, &hi)) {
			D[d].hay = 1;
			D[d].ultimo = lo;
		}
	}
	if (rapl == 1 && lee(0x613, &lo, &hi)) {
		perf613 = 1;
		perf613_ult = lo;
	}
	dice("termo: potencia: RAPL %s, unidades %08X (%s: potencia %u, energía %u, tiempo 1/%u s); paq %s, nuc %s, gpu %s, mem %s, equipo %s; 613h %s\n",
	     rapl == 2 ? "AMD" : "Intel", u606, atom_u ? "Atom, en mW y µJ por 2^n" : "1/2^n W y J", 1u << e_pu, 1u << e_esu, 1u << e_tu,
	     D[0].hay > 0 ? "sí" : "no", D[1].hay > 0 ? "sí" : "no", D[2].hay > 0 ? "sí" : "no", D[3].hay > 0 ? "sí" : "no", D[4].hay > 0 ? "sí" : "no",
	     perf613 > 0 ? "sí" : "no");
}

static u64 tsc_ult;

void po_empieza(const struct po_lector *l, int intel, int amd, int fam, int modelo)
{
	memset(D, 0, sizeof D);
	memset(cuenta, 0, sizeof cuenta);
	memset(antes_cat, 0, sizeof antes_cat);
	memset(visto_cat, 0, sizeof visto_cat);
	memset(us_fase, 0, sizeof us_fase);
	memset(tsc_fase, 0, sizeof tsc_fase);
	memset(n_fase, 0, sizeof n_fase);
	memset(perf613_us, 0, sizeof perf613_us);
	memset(CE, 0, sizeof CE);
	nce = ce_cuenta = ce_algo = nring = pring = frenos_hay = frenos_cero = 0;
	pl1_mw = pl2_mw = tdp_mw = pl1_en = pl_bloq = psys_pl1_mw = 0;
	base_ratio = min_ratio = bus_mhz = 0;
	perf613 = -1;
	pct613_ahora = -1;
	t_max_carga = tj = NO_T;
	ca_carga = -1;
	ca_visto[0] = ca_visto[1] = 0;
	tsc_ult = 0;
	L = *l;
	p_intel = intel, p_amd = amd, p_fam = fam, p_modelo = modelo;
	empezado = 1;
	{
		int i;

		for (i = 0; i < NF; i++)
			F[i].hay = 0, F[i].log_vale = 0, F[i].antes = 0, F[i].primero = 0;
	}
	if (!intel && !amd) {
		dice("termo: potencia: ni Intel ni AMD: sin potencia ni frenos\n");
		return;
	}
	prepara_rapl();
	lee_limites();
	prepara_frenos();
	prepara_cestados();
}

/* ---------------------------------------------------------------- cada 2 s */
void po_mide(int fase, uint64_t tsc, uint64_t tsc_hz, int cargador, int t_cpu, int tjmax)
{
	u64 dtsc, dus;
	int d;
	u32 lo, hi;

	if (!empezado)
		return;
	if (fase < 0 || fase > 2)
		fase = PO_REPOSO;
	if (tjmax != NO_T)
		tj = tjmax;
	if (fase == PO_CARGA) {
		if (t_cpu != NO_T && (t_max_carga == NO_T || t_cpu > t_max_carga))
			t_max_carga = t_cpu;
		if (cargador == 0 || cargador == 1)
			ca_visto[cargador] = 1;
	}
	if (!tsc_ult || tsc <= tsc_ult || !tsc_hz) {
		mira_frenos_po(fase, 0);	/* (la primera vez: solo poner a 0 las marcas) */
		tsc_ult = tsc;
		for (d = 0; d < ND; d++)
			if (D[d].hay > 0 && lee(D[d].msr, &lo, &hi))
				D[d].ultimo = lo;
		if (perf613 > 0 && lee(0x613, &lo, &hi))
			perf613_ult = lo;
		mira_cestados(fase, 0);
		return;
	}
	dtsc = tsc - tsc_ult;
	tsc_ult = tsc;
	dus = dtsc * 1000000u / tsc_hz;
	if (!dus)
		return;
	us_fase[fase] += dus;
	tsc_fase[fase] += dtsc;
	n_fase[fase]++;
	mira_frenos_po(fase, 1);
	for (d = 0; d < ND; d++) {
		u32 delta;
		u64 uj;

		if (D[d].hay <= 0)
			continue;
		if (!lee(D[d].msr, &lo, &hi)) {
			D[d].hay = -1;
			continue;
		}
		delta = lo - D[d].ultimo;	/* (32 bits: una vuelta se resta bien) */
		D[d].ultimo = lo;
		if (delta)
			D[d].movido = 1;
		uj = a_uj(d, delta);
		D[d].uj[fase] += uj;
		D[d].us[fase] += dus;
		D[d].mw_ahora = (u32)(uj * 1000u / dus);
		if (D[d].mw_ahora > D[d].mw_max[fase])
			D[d].mw_max[fase] = D[d].mw_ahora;
	}
	if (fase == PO_CARGA && D[D_PAQ].hay > 0) {
		ring[pring] = D[D_PAQ].mw_ahora;
		pring = (pring + 1) % NRING;
		if (nring < NRING)
			nring++;
	}
	if (perf613 > 0 && lee(0x613, &lo, &hi)) {
		u64 t = a_us(lo - perf613_ult);

		perf613_ult = lo;
		if (t > dus)
			t = dus;
		perf613_us[fase] += t;
		pct613_ahora = (int)(t * 100 / dus);
	}
	mira_cestados(fase, dtsc);
}

/* ---------------------------------------------------------------- lo que se dice */
static int rapl_cuenta(void)
{
	return rapl && D[D_PAQ].hay > 0 && D[D_PAQ].movido;
}

int po_hay_rapl(void)
{
	return rapl_cuenta();
}

/* « | potencia=paq 41,2 W nuc 30,1 W; frena PL1 PROCHOT; PC6 40 % CC6 55 %»:
 * un solo «nombre=» (pruebas/lee_log.c lo salta), sin más «=» detrás */
int po_datos(char *d, int cabe)
{
	int n = 0, k, g, c, partes = 0;
	char w[16];
#define PD(...) (n += snprintf(d + n, (size_t)(cabe > n ? cabe - n : 0), __VA_ARGS__), n = n < cabe ? n : cabe - 1)

	if (cabe <= 1 || !empezado)
		return 0;
	d[0] = 0;
	if (rapl_cuenta()) {
		PD(" | potencia=");
		partes++;
		for (k = 0; k < ND; k++)
			if (D[k].hay > 0 && D[k].movido)
				PD("%s%s %s W", k ? " " : "", dom_corto[k], w1(w, D[k].mw_ahora));
		if (pct613_ahora > 0)
			PD(" (frenado por RAPL %d %%)", pct613_ahora);
	}
	if (frenos_hay) {
		int primero = 1;

		for (g = 0; g < NG; g++)
			for (c = 0; c < NC; c++)
				if (ahora[g][c]) {
					PD("%s%s%s", primero ? (partes++ ? "; frena " : " | potencia=frena ") : " ", g == 1 ? "gpu:" : g == 2 ? "anillo:" : "", cat_corto[c]);
					primero = 0;
				}
	}
	if (ce_algo) {
		int primero = 1;

		for (k = 0; k < nce; k++)
			if (CE[k].hay > 0 && CE[k].pct > 0) {
				PD("%s%s %d %%", primero ? (partes++ ? "; " : " | potencia=") : " ", CE[k].nombre, CE[k].pct);
				primero = 0;
			}
	}
#undef PD
	return n;
}

static u32 media_mw(int k, int f)
{
	return D[k].us[f] ? (u32)(D[k].uj[f] * 1000u / D[k].us[f]) : 0;
}

/* la media de los últimos 60 s de la carga (el paquete), en mW */
static u32 media_final(void)
{
	u64 s = 0;
	int i;

	if (!nring)
		return 0;
	for (i = 0; i < nring; i++)
		s += ring[i];
	return (u32)(s / (u32)nring);
}

static int pct(u32 x, int f)
{
	return n_fase[f] ? (int)(x * 100 / (u32)n_fase[f]) : 0;
}

static int pct_cat(int g, int c, int f)
{
	return pct(cuenta[g][c][f], f);
}

/* lo más visto de una clase en la carga (el procesador): el tanto por ciento */
static int pct_clase(char cl, int f, int *cat)
{
	int c, m = 0;

	for (c = 0; c < NC; c++)
		if (cat_clase[c] == cl) {
			int p = pct_cat(0, c, f);

			if (p > m) {
				m = p;
				if (cat)
					*cat = c;
			}
		}
	return m;
}

int po_informe(char *d, int cabe)
{
	int n = 0, k, f, g, c;
	char w[16], w2[16], w3[16], w4[16];
	static const char *const nf[3] = { "reposo", "carga", "enfriando" };
#define PI(...) (n += snprintf(d + n, (size_t)(cabe > n ? cabe - n : 0), __VA_ARGS__), n = n < cabe ? n : cabe - 1)

	if (cabe <= 1)
		return 0;
	d[0] = 0;
	if (!empezado)
		return 0;
	PI("\r\nPOTENCIA (lo que gasta el procesador, RAPL)\r\n");
	if (!rapl)
		PI("  Este procesador no dice sus vatios (RAPL, MSR %s: no lo tiene).\r\n", p_amd ? "C0010299h" : "606h");
	else if (!rapl_cuenta())
		PI("  El contador de energía del procesador no se ha movido (siempre igual): no da los vatios de verdad (pasa en las máquinas virtuales).\r\n");
	else {
		u32 mf = media_final();

		PI("  %-40s %-12s %-12s %s %s\r\n", "PARTE", "REPOSO", "CARGA media", pad(w, sizeof w, "CARGA máx.", 12), "ENFRIANDO");
		for (k = 0; k < ND; k++) {
			char nom[64];

			if (D[k].hay <= 0 || !D[k].movido)
				continue;
			PI("  %s %-12s %-12s %-12s %s\r\n", pad(nom, sizeof nom, rapl == 2 && k == D_NUC ? "Este núcleo" : dom_nombre[k], 40),
			   D[k].us[0] ? strcat(w1(w, media_mw(k, 0)), " W") : "--", D[k].us[1] ? strcat(w1(w2, media_mw(k, 1)), " W") : "--",
			   D[k].us[1] ? strcat(w1(w3, D[k].mw_max[1]), " W") : "--", D[k].us[2] ? strcat(w1(w4, media_mw(k, 2)), " W") : "--");
		}
		if (D[D_EQUIPO].hay > 0 && D[D_EQUIPO].movido)
			PI("  (PSys: lo que gasta el equipo entero, lo que entra del cargador o de la batería.)\r\n");
		for (k = 0; k < ND; k++)
			if (D[k].hay > 0 && !D[k].movido)
				PI("  (%s: el contador no se ha movido.)\r\n", dom_nombre[k]);
		if (mf)
			PI("  El paquete, en el último minuto de la carga: %s W de media%s.\r\n", w1(w, mf),
			   pl1_mw && pl1_en ? (mf + 2000 >= pl1_mw ? ", pegado a su PL1" : mf < pl1_mw * 80 / 100 ? ", bastante por debajo de su PL1" : "") : "");
		if (perf613 > 0)
			PI("  Tiempo frenado por los límites de potencia (PKG_PERF_STATUS 613h): %d %% en la carga, %d %% en reposo.\r\n",
			   us_fase[1] ? (int)(perf613_us[1] * 100 / us_fase[1]) : 0, us_fase[0] ? (int)(perf613_us[0] * 100 / us_fase[0]) : 0);
	}
	if (nlim)
		PI("%s", lim_txt);
	/* por qué se frena */
	PI("\r\nPOR QUÉ SE FRENA (lo que dice el procesador, en los ratos de 2 s de cada fase)\r\n");
	if (p_amd)
		PI("  Los AMD no dicen por MSR por qué se frenan.\r\n");
	else if (frenos_cero)
		PI("  Los registros de por qué se frena dan siempre 0 (también el de la temperatura): no los da de verdad (pasa en las máquinas virtuales).\r\n");
	else if (!frenos_hay)
		PI("  Este procesador no dice por qué se frena (no lo tiene: ni 19Ch ni los PERF_LIMIT_REASONS).\r\n");
	else {
		int algo = 0;

		PI("  (Mirados:");
		for (k = 0; k < NF; k++)
			if (F[k].hay > 0)
				PI(" %03Xh%s", F[k].msr, F[k].log_vale ? "" : "*");
		PI("; con «*», las marcas no se dejan borrar: solo cuenta lo visto en el momento.)\r\n");
		PI("  Ratos de 2 s: %d en reposo, %d en carga, %d enfriando.\r\n", n_fase[0], n_fase[1], n_fase[2]);
		for (g = 0; g < NG; g++)
			for (c = 0; c < NC; c++) {
				u32 t = cuenta[g][c][0] + cuenta[g][c][1] + cuenta[g][c][2];

				if (!t)
					continue;
				algo = 1;
				if (g)
					PI("  - En %s, por %s:", grupo_nombre[g], cat_texto[c]);
				else
					PI("  - Por %s:", cat_texto[c]);
				for (f = 0; f < 3; f++)
					if (n_fase[f])
						PI(" %s %d %%", nf[f], pct_cat(g, c, f));
				PI("%s\r\n", cat_clase[c] == 'n' ? " (normal, no es un fallo)" : "");
			}
		if (!algo)
			PI("  No se ha frenado por nada en toda la prueba.\r\n");
		{
			int hay = 0;

			for (c = 0; c < NC; c++)
				if (antes_cat[c] && cat_clase[c] != 'n')
					hay = 1;
			if (hay) {
				PI("  Anotado ya al abrir el Termómetro (de antes, con el sistema anterior o al arrancar; borrado al empezar):");
				for (c = 0, k = 0; c < NC; c++)
					if (antes_cat[c] && cat_clase[c] != 'n')
						PI("%s %s", k++ ? ";" : "", cat_texto[c]);
				PI(".\r\n");
			}
		}
	}
	/* los estados de reposo */
	if (nce) {
		int alguno = 0;

		PI("\r\nESTADOS DE REPOSO (C-states: lo que pasa el procesador dormido, en %% del tiempo)\r\n");
		for (k = 0; k < nce; k++)
			if (CE[k].hay > 0)
				alguno = 1;
		if (!alguno)
			PI("  Este procesador no da sus contadores de reposo.\r\n");
		else if (!ce_algo)
			PI("  Los contadores de reposo no se han movido (siempre igual): no los da de verdad (pasa en las máquinas virtuales) o no entra nunca en reposo profundo.\r\n");
		else {
			PI("  %-36s %-10s %-10s %s\r\n", "ESTADO", "REPOSO", "CARGA", "ENFRIANDO");
			for (k = 0; k < nce; k++) {
				char a[3][12], nom[48];

				if (CE[k].hay <= 0)
					continue;
				for (f = 0; f < 3; f++)
					if (tsc_fase[f])
						snprintf(a[f], sizeof a[f], "%d %%", (int)(CE[k].suma[f] * 100 / tsc_fase[f]));
					else
						strcpy(a[f], "--");
				PI("  %-4s %s %-10s %-10s %s\r\n", CE[k].nombre, pad(nom, sizeof nom, CE[k].paquete ? "(el procesador entero)" : "(este núcleo)", 31), a[0], a[1], a[2]);
			}
			PI("  (Hispa-Pentium deja descansar al procesador con «hlt» (C1): que el paquete no baje a PC6 o más aquí no es un fallo; sirve para comparar.)\r\n");
		}
	}
#undef PI
	return n;
}

int po_frenado_carga(void)
{
	return pct_clase('p', PO_CARGA, 0) || pct_clase('w', PO_CARGA, 0) || pct_clase('t', PO_CARGA, 0);
}

int po_veredicto(char *d, int cabe)
{
	int n = 0, cat = 0, pp, pw, pt, cool, frases = 0;
	char w[16], w2[16], w3[16], g[24];
	u32 mf = media_final();
	int ca = ca_visto[1] && !ca_visto[0] ? 1 : ca_visto[0] && !ca_visto[1] ? 0 : -1;
#define PV(...) (n += snprintf(d + n, (size_t)(cabe > n ? cabe - n : 0), __VA_ARGS__), n = n < cabe ? n : cabe - 1)
#define FRASE() PV("%s", frases++ ? "Además, " : "Potencia: ")

	if (cabe <= 1)
		return 0;
	d[0] = 0;
	if (!empezado || !n_fase[PO_CARGA] || (!rapl_cuenta() && !frenos_hay))
		return 0;
	if (!mf && rapl_cuenta())
		mf = media_mw(D_PAQ, PO_CARGA);
	cool = t_max_carga != NO_T && tj != NO_T && t_max_carga < tj - 200;
	if (t_max_carga != NO_T)
		snprintf(g, sizeof g, "%d °C", t_max_carga / 10);
	else
		strcpy(g, "?");
	/* 1. la placa: PROCHOT, la corriente, el regulador caliente */
	pp = pct_clase('p', PO_CARGA, &cat);
	if (pp >= 5) {
		FRASE();
		PV("el procesador se frena por %s en el %d %% de la carga, %s (como mucho %s): ", cat_texto[cat], pp,
		   cool ? "con el procesador lejos de su límite" : "con el procesador caliente", g);
		if (cool)
			PV("lo pide la placa, no el calor; apunta a la alimentación (el cargador, la batería o el regulador de tensión)%s. ",
			   ca == 1 ? ", y era con el cargador puesto" : "");
		else
			PV("puede ser la placa protegiéndose por calor; si pasa también con el procesador fresco, hay que mirar la alimentación. ");
	}
	/* 2. los límites de potencia (y el tiempo frenado por RAPL, 613h) */
	cat = C_PL1;
	pw = pct_clase('w', PO_CARGA, &cat);
	if (perf613 > 0 && us_fase[1]) {
		int p613 = (int)(perf613_us[1] * 100 / us_fase[1]);

		if (p613 > pw)
			pw = p613;
	}
	if (pw >= 20 && rapl_cuenta()) {
		FRASE();
		PV("el procesador se frena por potencia a %s W%s", w1(w, mf), ca == 1 ? " con el cargador puesto" : ca == 0 ? " sin el cargador (con la batería)" : "");
		if (pl1_mw && tdp_mw && pl1_en && pl1_mw < tdp_mw * 85 / 100)
			PV(": la BIOS le ha puesto el PL1 en %s W, por debajo de su TDP (%s W); %s. ", w1(w2, pl1_mw), w1(w3, tdp_mw),
			   ca == 0 ? "con la batería es lo normal" : "la fuente o el ajuste del fabricante lo limitan (o un cargador que el equipo no reconoce como suyo)");
		else if (pl1_mw && pl1_en && mf < pl1_mw * 80 / 100)
			PV(", bastante por debajo de su PL1 (%s W): lo limita otra cosa%s. ", w1(w2, pl1_mw),
			   ca == 0 ? "; sin el cargador es lo normal" :
			   ": la fuente o el ajuste del fabricante (la placa, con el límite de MCHBAR, o el controlador embebido; en los MSI se baja el límite si el cargador no da bastante)");
		else if (pl1_mw && pl1_en)
			PV(": es su PL1 (%s W), el límite que le pone el fabricante; lo normal en una carga larga, no un fallo. ", w1(w2, pl1_mw));
		else
			PV(" (sus límites no se leen): la fuente o el ajuste del fabricante lo limitan. ");
	} else if (pw >= 20) {
		FRASE();
		PV("el procesador se frena por los límites de potencia en el %d %% de la carga%s (no dice sus vatios). ", pw, ca == 1 ? " con el cargador puesto" : "");
	}
	/* 3. la temperatura (el veredicto de siempre ya lo juzga: solo se dice) */
	pt = pct_clase('t', PO_CARGA, 0);
	if (pt >= 10) {
		FRASE();
		PV("se frena por temperatura en el %d %% de la carga: hay que mirar la refrigeración. ", pt);
	}
	if (!frases) {
		FRASE();
		if (rapl_cuenta())
			PV("en la carga ha llegado a %s W de media (lo más, %s W) sin frenarse por potencia ni por la placa. ", w1(w, mf), w1(w2, D[D_PAQ].mw_max[1]));
		else
			PV("en la carga no se ha frenado ni por potencia ni por la placa. ");
	}
	if (n >= 1 && d[n - 1] == ' ')
		d[--n] = 0;
#undef FRASE
#undef PV
	return n;
}
