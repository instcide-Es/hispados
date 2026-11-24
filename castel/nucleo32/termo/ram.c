/* ram.c - (TERMO, RAM) «Memoria RAM»: la prueba de la memoria del equipo,
 * para descartarla (o pillarla) cuando un portátil se congela (el MSI GT80
 * que no carga Windows). TERMO.BIN /RAM es una tarea hija (como /MIDE): va
 * diciendo en TERMO\RAM.EST lo que hace (antes de cada trozo: si el equipo
 * se cuelga, ahí queda dónde), deja el resultado en TERMO\RAM.TXT y se para
 * con TERMO\RAM.PAR (Esc).
 *
 * Qué memoria: toda la que el núcleo puede dar (termoram.cas, la llamada
 * 226). La de abajo (la que usa el núcleo, hasta 1 GB), la que el núcleo le
 * da a esta tarea (todo lo libre menos un poco para que el sistema siga);
 * la de arriba (de 1 GB al final, también por encima de los 4 GB con PSE-36),
 * entera, por una ventana que se va moviendo. Lo que no se prueba se dice,
 * y por qué.
 *
 * Las pruebas (ram_alg.c): las líneas de dirección, la dirección propia,
 * unos y ceros andantes, 0x55/0xAA, moving inversions con azar sembrado; y
 * con todos los núcleos a la vez (cada uno su trozo, en el núcleo: la
 * dirección propia, el azar con su contrario y, en la completa, los
 * patrones) para meter calor y tráfico.
 *   Rápida (la de siempre, tecla M): menos de 2 minutos, ANTES de la prueba
 *   de esfuerzo (no durante: la carga de coma flotante y la de memoria se
 *   quitarían el procesador, y el calor de la RAM falsearía el «reposo» con
 *   el que se mide cuánto sube). Primero todo con todos los núcleos (lo que
 *   más memoria cubre por segundo), luego un núcleo con el resto de
 *   pruebas, repartido por toda la memoria (si no da tiempo a todo, dice
 *   cuánto ha cubierto cada una).
 *   Completa (el menú del arranque «... prueba COMPLETA de la memoria RAM»,
 *   o TERMO.BIN /RAM /COMPLETA): dos pasadas de todo, sin límite de tiempo.
 * Los errores: la dirección física, lo esperado, lo leído y los bits;
 * agrupados (ram_alg.c: el mismo bit, un chip, una zona, dispersos, solo
 * con todos los núcleos), con la ranura (aproximada) si el SMBIOS la dice;
 * y los errores corregidos de memoria que haya contado el procesador
 * (Machine Check: solo con memoria ECC).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
#include <stdarg.h>
#include "ram.h"
#include "ram_alg.h"

#define SC_MSR 220
#define SC_RAM 226
#define MB (1u << 20)
#define EM "EN MARCHA · "	/* (lo que empieza RAM.EST mientras prueba) */
#define EMN ((int)sizeof EM - 1)
#define TROZO (16u * MB)	/* lo que prueba un núcleo de una vez */
#define RAPIDA_S 110		/* la rápida: lo que puede durar (más el informe: menos de 2 min) */
typedef unsigned long long u64;

/* ------------------------------------------------------------ lo que da el núcleo */
struct info_ram {
	u32 version, limite, libres, totales, banderas, fis, ventana, vpags, nreg, ap, motivo, r[5];
};
struct region_ram { u32 bl, bh, ll, lh, clase, z; };
struct trabajo_ram {	/* (Trabajo_ram de termoram.cas) */
	u32 orden, patron, semilla, inicio, largo, errores, procesadores, motivo, guardados, sin_acabar, reservado[6], err[48];
};

/* los trozos que se prueban: los de esta tarea (de la llamada 17) y los de la ventana */
#define MAXZ 160
struct zona {
	u32 v, bytes;		/* (en la ventana, v = VENTANA) */
	u64 fis;		/* (en la ventana) dónde empieza en la física */
	u32 pag0, npag;		/* (en la ventana) las páginas de 4 MB */
	int ventana;
};
static struct zona Z[MAXZ];
static int nz;

static char r_est[120], r_par[120], r_txt[120];
static u32 hz = 100, t0, ult_est, ult_par;
static int cancelada, simula, completa_;
static u32 info_b[1024];
static struct info_ram *I = (struct info_ram *)info_b;
static int con_226, nap, ap_motivo;
static struct ra_fallos F;
static struct ra_smbios SM;
static unsigned char *smb;
static struct trabajo_ram TR;

/* lo hecho de cada prueba: los octetos probados, los que había que probar y el tiempo */
static u64 hecho_b[RA_NPRUEBAS], tics_p[RA_NPRUEBAS];
static u64 total_propia, total_ventana, total_libre_bajo, total_libre_alto, alto_inalcanzable, alto_rechazado, vista;
static int pasadas_hechas, pasadas;

static u32 tics(void) { return sc(SC_TICS, 0, 0, 0); }
static u32 segundos(void) { return (tics() - t0) / hz; }

static int existe_r(const char *r)
{
	u32 m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);

	if (ES_ERROR(m))
		return 0;
	sc(SC_CERRAR, m, 0, 0);
	return 1;
}

static int escribe_r(const char *r, const char *t, u32 n)
{
	u32 m = sc(SC_ABRIR, (u32)r, PARA_CREAR, 0), x;

	if (ES_ERROR(m))
		return 0;
	x = sc(SC_ESCRIBIR_ARCHIVO, m, (u32)t, n);
	sc(SC_CERRAR, m, 0, 0);
	return x == n;
}

static int lee_r(const char *r, char *d, int cabe)
{
	u32 m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0), n;

	d[0] = 0;
	if (ES_ERROR(m))
		return -1;
	n = sc(SC_LEER, m, (u32)d, (u32)cabe - 1);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n))
		return -1;
	d[n] = 0;
	return (int)n;
}

static const char *busca_r(const char *t, const char *o)
{
	size_t n = strlen(o);

	for (; *t; t++)
		if (!strncmp(t, o, n))
			return t;
	return 0;
}

/* lo que está haciendo, en RAM.EST (y por el puerto serie); «forzar»: aunque
 * no haya pasado un segundo desde la última vez */
static void estado(int forzar, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void estado(int forzar, const char *fmt, ...)
{
	char t[400];
	va_list v;

	if (!forzar && tics() - ult_est < hz)
		return;
	ult_est = tics();
	va_start(v, fmt);
	vsnprintf(t, sizeof t, fmt, v);
	va_end(v);
	serie("termo: ram: %s\n", t);
	escribe_r(r_est, t, (u32)strlen(t));
}

static int parar_cb(void *x)
{
	(void)x;
	if (cancelada)
		return 1;
	if (tics() - ult_par < hz)
		return 0;
	ult_par = tics();
	if (existe_r(r_par))
		cancelada = 1;
	return cancelada;
}

/* ------------------------------------------------------------ la física */
static u32 cache_v = 1, cache_f, cache_fh;

static u64 fisica_de(const struct zona *z, u32 v, int *sabida)
{
	u32 b[2];

	*sabida = 1;
	if (z && z->ventana)
		return z->fis + (v - z->v);
	if ((v & ~0xFFFu) == cache_v)
		return ((u64)cache_fh << 32) | (cache_f | (v & 0xFFF));
	if (con_226 && sc(SC_RAM, 3, v, (u32)b) == 1) {
		cache_v = v & ~0xFFFu;
		cache_f = b[0] & ~0xFFFu;
		cache_fh = b[1];
		return ((u64)b[1] << 32) | b[0];
	}
	*sabida = 0;
	return v;
}

/* cada fallo de un núcleo (ram_alg.c) */
static struct zona *zona_actual;
static int todos_ahora;

static void fallo_cb(void *ctx, volatile ra_u32 *d, ra_u32 e, ra_u32 l, int prueba)
{
	int sabida;
	u64 f = fisica_de(zona_actual, (u32)d, &sabida);

	(void)ctx;
	if (F.total < 20)
		serie("termo: ram: FALLO en %llXh (%s): esperado %08X, leído %08X\n", f, ra_nombre_prueba(prueba), e, l);
	ra_apunta(&F, f, sabida, e, l, prueba, todos_ahora);
}

/* ------------------------------------------------------------ Machine Check */
#define MCA_MAX 32
static int mca_n = -1, mca_memoria, mca_corregidos;
static u32 mca0[MCA_MAX][2];

static int lee_msr(u32 m, u32 *lo, u32 *hi)
{
	u32 b[2] = { 0, 0 };

	if (sc(SC_MSR, m, (u32)b, 0) != 1)
		return 0;
	*lo = b[0];
	*hi = b[1];
	return 1;
}

static void cpuid_r(u32 h, u32 r[4])
{
	__asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(h), "c"(0));
}

/* ¿es de la memoria el error de este banco? (Intel: la clase del
 * controlador de memoria, 0000 0000 1MMM CCCC; AMD de antes de Zen: el
 * banco 4, el puente norte, donde va el ECC de la DRAM) */
static int mca_es_memoria(int banco, u32 lo, int amd, int fam)
{
	u32 cod = lo & 0xFFFF;

	if ((cod & 0xEF80) == 0x0080)
		return 1;
	return amd && fam >= 0x10 && fam < 0x17 && banco == 4;
}

static void mira_mca(int al_final)
{
	u32 r[4], lo, hi, marca[3];
	int i, amd, fam;

	cpuid_r(0, r);
	marca[0] = r[1], marca[1] = r[3], marca[2] = r[2];
	amd = !memcmp(marca, "AuthenticAMD", 12);
	cpuid_r(1, r);
	fam = (int)((r[0] >> 8) & 15) + (((r[0] >> 8) & 15) == 15 ? (int)((r[0] >> 20) & 0xFF) : 0);
	if (mca_n < 0) {
		mca_n = 0;
		if (!(r[3] & (1u << 14)) || !(r[3] & (1u << 7)) || !lee_msr(0x179, &lo, &hi))
			return;
		mca_n = (int)(lo & 0xFF) > MCA_MAX ? MCA_MAX : (int)(lo & 0xFF);
	}
	for (i = 0; i < mca_n; i++) {
		if (!lee_msr(0x401 + 4 * (u32)i, &lo, &hi))
			continue;
		if (!al_final) {
			mca0[i][0] = lo, mca0[i][1] = hi;
			continue;
		}
		if (!(hi & 0x80000000u) || (lo == mca0[i][0] && hi == mca0[i][1]) || !mca_es_memoria(i, lo, amd, fam))
			continue;
		/* nuevo: cuántos (el contador de corregidos, bits 38-52, si lo lleva) */
		{
			u32 c1 = (hi >> 6) & 0x7FFF, c0 = (mca0[i][1] & 0x80000000u) ? (mca0[i][1] >> 6) & 0x7FFF : 0;
			int n = c1 > c0 ? (int)(c1 - c0) : 1;

			mca_memoria += n;
			if (!(hi & (1u << 29)))
				mca_corregidos += n;
			serie("termo: ram: Machine Check, banco %d: error de memoria %s (estado %08X%08X)\n", i, hi & (1u << 29) ? "NO corregido" : "corregido", hi, lo);
		}
	}
}

/* ------------------------------------------------------------ preparar */
static u32 propia_pedida;

/* la memoria de esta tarea: todo lo libre menos lo que necesita el sistema
 * para seguir (64 MB, o la octava parte si es más) */
static void pide_propia(void)
{
	u32 libres = con_226 ? I->libres : sc(SC_DATO, 0, 0, 0), quiere, reserva, r;
	u64 b = (u64)libres * 4096;

	if (ES_ERROR(libres))
		return;
	reserva = (u32)(b / 8 > 64ull * MB ? b / 8 : 64ull * MB);
	if (b <= reserva + 8ull * MB)
		return;
	quiere = (u32)(b - reserva) & ~(4 * MB - 1);
	while (propia_pedida < quiere && nz < MAXZ) {
		r = sc(SC_MEMORIA, 4 * MB, 0, 0);
		if (ES_ERROR(r) || !r)
			break;
		/* (no puede llegar a la ventana: si llegara, se para) */
		if (con_226 && r + 4 * MB > I->ventana - 16 * MB)
			break;
		r = (r + 63) & ~63u;
		if (nz && !Z[nz - 1].ventana && Z[nz - 1].v + Z[nz - 1].bytes + 64 >= r && Z[nz - 1].bytes < 512 * MB)
			Z[nz - 1].bytes = r + 4 * MB - 64 - Z[nz - 1].v;
		else {
			Z[nz].v = r;
			Z[nz].bytes = 4 * MB - 64;
			Z[nz].ventana = 0;
			nz++;
		}
		propia_pedida += 4 * MB;
	}
	for (r = 0; (int)r < nz; r++) {
		Z[r].bytes &= ~63u;
		total_propia += Z[r].bytes;
	}
}

/* la ventana: las páginas de 4 MB libres de arriba, en trozos de hasta 768 MB */
static void prepara_ventana(void)
{
	struct region_ram *g = (struct region_ram *)(info_b + 16);
	u32 i, pse = I->banderas & 1, pse36 = I->banderas & 2;
	u64 lim = (u64)((I->limite + 4 * MB - 1) & ~(4 * MB - 1)), tope = 1ull << I->fis;

	for (i = 0; i < I->nreg && i < 128; i++) {
		u64 b = ((u64)g[i].bh << 32) | g[i].bl, l = ((u64)g[i].lh << 32) | g[i].ll, f = b + l, ini, fin;

		if (g[i].clase != 1)
			continue;
		vista += l;
		if (b < I->limite)
			total_libre_bajo += (f < I->limite ? f : I->limite) - b;
		if (f <= I->limite)
			continue;
		total_libre_alto += f - (b > I->limite ? b : I->limite);
		ini = (b > lim ? b : lim);
		ini = (ini + 4 * MB - 1) & ~(u64)(4 * MB - 1);
		fin = f & ~(u64)(4 * MB - 1);
		if (!pse) {
			alto_inalcanzable += f - (b > I->limite ? b : I->limite);
			continue;
		}
		if (!pse36 && fin > (1ull << 32)) {
			if (ini < (1ull << 32))
				alto_inalcanzable += fin - (1ull << 32);
			else
				alto_inalcanzable += fin - ini;
			fin = fin < (1ull << 32) ? fin : (1ull << 32);
		}
		if (fin > tope) {
			alto_inalcanzable += fin - (ini > tope ? ini : tope);
			fin = tope;
		}
		while (ini < fin && nz < MAXZ) {
			u64 n = fin - ini;

			if (n > (u64)I->vpags * 4 * MB)
				n = (u64)I->vpags * 4 * MB;
			Z[nz].v = I->ventana;
			Z[nz].fis = ini;
			Z[nz].pag0 = (u32)(ini >> 22);
			Z[nz].npag = (u32)(n >> 22);
			Z[nz].bytes = (u32)n;
			Z[nz].ventana = 1;
			nz++;
			ini += n;
		}
	}
}

/* pone la ventana de la zona; si el núcleo no da todas las páginas, la zona
 * se queda con las que da (y lo que no, se cuenta como rechazado) */
static int mapea(struct zona *z)
{
	u32 n;

	if (!z->ventana)
		return 1;
	n = sc(SC_RAM, 1, z->pag0, z->npag);
	if (ES_ERROR(n))
		n = 0;
	if (n < z->npag) {
		serie("termo: ram: el núcleo da %u de %u páginas de 4 MB desde %llXh\n", n, z->npag, z->fis);
		alto_rechazado += (u64)(z->npag - n) * 4 * MB;
		total_ventana -= (u64)(z->npag - n) * 4 * MB;
		z->npag = n;
		z->bytes = n * 4 * MB;
	}
	return n > 0;
}

/* ------------------------------------------------------------ con todos los núcleos */
static void trabajo(struct zona *z, u32 orden, u32 patron, u32 semilla)
{
	u32 k;

	memset(&TR, 0, sizeof TR);
	TR.orden = orden;
	TR.patron = patron;
	TR.semilla = semilla;
	TR.inicio = z->v;
	TR.largo = z->bytes;
	k = sc(SC_RAM, 5, (u32)&TR, 0);
	if (ES_ERROR(k) || !k) {
		if (!ES_ERROR(k))
			ap_motivo = (int)TR.motivo;
		nap = 0;
		return;
	}
	nap = (int)k;
	todos_ahora = 1;
	zona_actual = z;
	for (k = 0; k < TR.guardados && k < 16; k++) {
		int sabida;
		u64 f = fisica_de(z, TR.err[k * 3], &sabida);

		ra_apunta(&F, f, sabida, TR.err[k * 3 + 1], TR.err[k * 3 + 2], RA_TODOS, 1);
	}
	if (TR.errores > TR.guardados) {	/* (los demás, sin detalle) */
		F.total += TR.errores - TR.guardados;
		F.por_prueba[RA_TODOS] += TR.errores - TR.guardados;
		F.en_todos += TR.errores - TR.guardados;
	}
	if (TR.sin_acabar)
		ap_motivo = 8;
	todos_ahora = 0;
}

static const char *motivo_ap(int m)
{
	static const char *const t[] = { "", "el procesador no tiene APIC", "el APIC está apagado", "no se puede ver el APIC", "no hay sitio en la memoria baja",
					 "el trampolín no cabe", "ningún otro procesador ha contestado (tiene uno solo)", "la prueba de esfuerzo los estaba usando",
					 "alguno no ha acabado a tiempo (¿colgado?)" };

	return m >= 0 && m <= 8 ? t[m] : "?";
}

static void dice_zona(const char *que, int iz, int nzonas, const struct zona *z)
{
	char a[24], b[24];
	u64 f0 = z->ventana ? z->fis : 0;

	if (z->ventana)
		estado(1, "EN MARCHA · %s · %s a %s de la memoria física (trozo %d de %d) · %u s", que, ra_tam(a, f0), ra_tam(b, f0 + z->bytes), iz + 1, nzonas, segundos());
	else
		estado(1, "EN MARCHA · %s · la memoria baja, %s (trozo %d de %d) · %u s", que, ra_tam(a, z->bytes), iz + 1, nzonas, segundos());
}

static void con_todos(int completa, int pasada)
{
	int i;
	u32 t;
	u32 sem = 0x2545F491u * (u32)(pasada + 1) ^ (tics() << 7);

	for (i = 0; i < nz && !cancelada; i++) {
		struct zona *z = &Z[i];

		if (!mapea(z))
			continue;
		dice_zona("con TODOS los núcleos a la vez", i, nz, z);
		t = tics();
		trabajo(z, 7, 0, 0);
		if (!nap)
			return;
		trabajo(z, 8, 0, 0);
		trabajo(z, 4, 0, sem + (u32)i);
		trabajo(z, 5, 0, sem + (u32)i);
		trabajo(z, 6, 0, sem + (u32)i);
		if (completa) {
			trabajo(z, 1, 0x55555555u, 0);
			trabajo(z, 2, 0x55555555u, 0);
			trabajo(z, 3, 0x55555555u, 0);
			trabajo(z, 1, 0x0F0F0F0Fu, 0);
			trabajo(z, 2, 0x0F0F0F0Fu, 0);
			trabajo(z, 3, 0x0F0F0F0Fu, 0);
		}
		if (!nap)
			return;
		hecho_b[RA_TODOS] += z->bytes;
		tics_p[RA_TODOS] += tics() - t;
		parar_cb(0);
	}
}

/* ------------------------------------------------------------ con un núcleo */
/* el orden de los trozos: el de los bits al revés (lo que dé tiempo, repartido por toda la zona) */
static u32 al_reves(u32 i, int bits)
{
	u32 r = 0;
	int k;

	for (k = 0; k < bits; k++)
		if (i & (1u << k))
			r |= 1u << (bits - 1 - k);
	return r;
}

static struct ra_ctx C;

static void simula_en(volatile u32 *p, u32 n, int prueba)
{
	/* (/SIMULAFALLO) el bit 9 de una palabra del primer trozo, como si estuviera pegado a 1 */
	volatile u32 *v = (volatile u32 *)(Z[0].v + MB);

	if (simula && !Z[0].ventana && v >= p && v < p + n)
		fallo_cb(0, v, 0x55555555u & ~(1u << 9), 0x55555555u | (1u << 9), prueba);
}

/* las pruebas de un núcleo sobre un trozo; da los tics que ha tardado */
static void un_trozo(volatile u32 *p, u32 n, int completa, int sin_direccion, int pasada, int extra)
{
	u32 t;
	int r;

#define PRUEBA(k, llamada) do { t = tics(); llamada; simula_en(p, n, k); hecho_b[k] += (u64)n * 4; tics_p[k] += tics() - t; } while (0)
	if (!sin_direccion || extra)
		PRUEBA(RA_DIRECCION, ra_direccion(&C, p, n));
	if (cancelada)
		return;
	if (extra) {	/* (la rápida, con tiempo de sobra: moving inversions bajando, con un núcleo) */
		PRUEBA(RA_INVERSIONES, ra_inversiones(&C, p, n, 0x9E3779B9u * (u32)(pasada + 1) + (u32)(unsigned long)p));
		return;
	}
	if (completa) {
		for (r = 0; r < 32 && !cancelada; r += 8) {
			PRUEBA(RA_ANDANTES, (ra_andantes(&C, p, n, r, 1), ra_andantes(&C, p, n, r, 0)));
			hecho_b[RA_ANDANTES] -= (u64)n * 4;
		}
		hecho_b[RA_ANDANTES] += (u64)n * 4;
	} else
		PRUEBA(RA_ANDANTES, (ra_andantes(&C, p, n, pasada, 1), ra_andantes(&C, p, n, pasada, 0)));
	if (cancelada)
		return;
	PRUEBA(RA_PATRON, ra_patron(&C, p, n, 0x55555555u));
	if (completa && !cancelada) {
		static const u32 pat[] = { 0x33333333u, 0x0F0F0F0Fu, 0x00FF00FFu, 0x0000FFFFu, 0x00000000u };
		unsigned k;

		for (k = 0; k < sizeof pat / sizeof pat[0] && !cancelada; k++) {
			PRUEBA(RA_PATRON, ra_patron(&C, p, n, pat[k]));
			hecho_b[RA_PATRON] -= (u64)n * 4;
		}
	}
	if (cancelada)
		return;
	if (completa || !nap)
		PRUEBA(RA_INVERSIONES, ra_inversiones(&C, p, n, 0x9E3779B9u * (u32)(pasada + 1) + (u32)(unsigned long)p));
#undef PRUEBA
}

static void con_uno(int completa, int pasada, u32 hasta_s, int extra)
{
	int i;
	u64 quedan = 0;

	for (i = 0; i < nz; i++)
		quedan += Z[i].bytes;
	for (i = 0; i < nz && !cancelada; i++) {
		struct zona *z = &Z[i];
		u32 ntr = (z->bytes + TROZO - 1) / TROZO, k, j, t;
		int bits = 0;
		u32 dura_s = 0;

		if (!z->bytes || !mapea(z))
			continue;
		dice_zona(extra ? "con un núcleo, lo que da tiempo de más" : "con un núcleo", i, nz, z);
		zona_actual = z;
		/* el tiempo de esta zona, en proporción (la rápida) */
		if (hasta_s && segundos() < hasta_s && quedan)
			dura_s = (u32)((u64)(hasta_s - segundos()) * z->bytes / quedan);
		t = tics();
		if (!extra) {
			u32 l = tics();

			ra_lineas(&C, (volatile u32 *)z->v, z->bytes / 4);
			hecho_b[RA_LINEAS] += z->bytes;
			tics_p[RA_LINEAS] += tics() - l;
		}
		while ((1u << bits) < ntr)
			bits++;
		for (j = 0, k = 0; k < (1u << bits) && !cancelada; k++) {
			u32 tr = al_reves(k, bits), n;

			if (tr >= ntr)
				continue;
			n = z->bytes - tr * TROZO < TROZO ? z->bytes - tr * TROZO : TROZO;
			un_trozo((volatile u32 *)(z->v + tr * TROZO), n / 4, completa, nap > 0, pasada, extra);
			j++;
			parar_cb(0);
			if (hasta_s && (segundos() >= hasta_s || (dura_s && (tics() - t) / hz >= dura_s && j < ntr)))
				break;
			if ((j & 7) == 0)
				dice_zona(extra ? "con un núcleo, lo que da tiempo de más" : "con un núcleo", i, nz, z);
		}
		quedan -= z->bytes;
	}
}

/* ------------------------------------------------------------ el resultado */
static char T[16000];
static int nt;
#define TX(...) do { if (nt < (int)sizeof T) nt += snprintf(T + nt, sizeof T - (size_t)nt, __VA_ARGS__); if (nt > (int)sizeof T - 1) nt = (int)sizeof T - 1; } while (0)

static int pct(u64 a, u64 b) { return b ? (int)(a * 100 / b) : 0; }

static void mb_s(char *d, u64 b, u64 t)
{
	u64 v = t ? b * hz / t / MB : 0;

	snprintf(d, 24, "%llu MB/s", v);
}

static int mal_(void) { return F.total > 0 || mca_memoria > 0; }

static void escribe_resultado(int esc)
{
	char a[24], b[24], c[24], d[2400];
	u32 f = sc(SC_DATO, DATO_FECHA, 0, 0), fe = f >> 16, ho = f & 0xFFFF;
	u64 probada = total_propia + total_ventana, smb_mb = 0;
	int i, k;
	struct ra_diag dg;

	for (i = 0; i < SM.nmod; i++)
		smb_mb += SM.m[i].mb;
	ra_diagnostica(&F, &dg);
	nt = 0;
	TX("\xEF\xBB\xBFPRUEBA DE LA MEMORIA RAM (Termómetro de Hispa-Pentium)\r\n");
	TX("Fecha: %02u/%02u/%04u %02u:%02u. Modo: %s; %u s.%s\r\n", fe & 31, (fe >> 5) & 15, 1980 + (fe >> 9), ho >> 11, (ho >> 5) & 63,
	   completa_ ? "COMPLETA" : "rápida (menos de 2 minutos)", segundos(), esc ? " CANCELADA (Esc): lo de abajo es lo hecho hasta entonces." : "");
	if (simula)
		TX("OJO: con /SIMULAFALLO (para probar el Termómetro): el fallo del bit 9 es de mentira.\r\n");
	TX("MEMORIA RAM: %s\r\n", mal_() ? "MAL" : esc ? "sin fallos en lo probado (pero CANCELADA: no es una prueba entera)" : "BIEN");
	if (F.total) {
		ra_diag_texto(&F, &dg, d, sizeof d);
		TX("  Por qué: %s\r\n", d);
	} else
		TX("  Por qué: ni un solo fallo: cada palabra probada ha devuelto lo que se escribió%s.\r\n", nap ? ", también con todos los núcleos a la vez" : "");
	if (mca_memoria)
		TX("  El procesador ha anotado %d %s de la memoria durante la prueba (Machine Check)%s: la memoria FALLA aunque esta prueba lo corrija.\r\n", mca_memoria,
		   mca_memoria == 1 ? "error" : "errores", mca_corregidos ? ", corregidos por el ECC" : "");
	/* la ranura */
	if (F.total) {
		int m = -1, iguales = 1, puestos = 0, solo = -1;

		for (i = 0; i < SM.nmod; i++)
			if (SM.m[i].mb)
				puestos++, solo = i;
		for (i = 0; i < F.ndet; i++) {
			int x = F.det[i].fis_sabida ? ra_modulo_de(&SM, F.det[i].fis) : -1;

			if (i == 0)
				m = x;
			else if (x != m)
				iguales = 0;
		}
		if (puestos == 1)
			TX("  Módulo: solo hay uno, «%s» (si el fallo es de un módulo, es ese).\r\n", SM.m[solo].ranura[0] ? SM.m[solo].ranura : "?");
		else if (m >= 0 && iguales)
			TX("  Módulo: según la BIOS (SMBIOS tipo 20), las direcciones con fallo caen en «%s%s%s». ES APROXIMADO: con dos canales la memoria se reparte "
			   "entre los módulos a trozos y la BIOS no siempre dice bien qué va a cada uno. Para estar seguro: probar con un solo módulo cada vez.\r\n",
			   SM.m[m].ranura[0] ? SM.m[m].ranura : "?", SM.m[m].banco[0] ? " / " : "", SM.m[m].banco);
		else
			TX("  Módulo: no se puede saber cuál (la BIOS no dice qué direcciones van a cada uno, o caen en varios). Probar con un solo módulo cada vez.\r\n");
	}
	/* cuánta */
	TX("\r\nCUÁNTA MEMORIA\r\n");
	if (SM.hay && SM.nmod)
	{
		ra_texto_modulos(&SM, d, sizeof d);
		TX("  Puesta: %s.\r\n", d);
	}
	else
		TX("  Puesta: la BIOS no lo dice (SMBIOS).\r\n");
	TX("  La BIOS deja libre (su mapa de memoria, E820/UEFI): %s", ra_tam(a, vista));
	if (smb_mb && (smb_mb << 20) > vista + 16ull * MB)
		TX(" (el resto, %s, se lo queda el equipo: la gráfica integrada, el firmware, ACPI)", ra_tam(b, (smb_mb << 20) - vista));
	TX(".\r\n  Probada: %s, el %d %% de lo libre.\r\n", ra_tam(c, probada), pct(probada, vista));
	if (total_libre_bajo > total_propia)
		TX("  No se prueban %s de abajo (hasta %s): los usa el propio Hispa-Pentium (el núcleo, la pantalla, los servicios y el Termómetro) y se deja un poco libre "
		   "para que sigan funcionando mientras tanto.\r\n", ra_tam(a, total_libre_bajo - total_propia), ra_tam(b, I->limite ? I->limite : 0));
	if (alto_inalcanzable)
		TX("  No se prueban %s de arriba: %s.\r\n", ra_tam(a, alto_inalcanzable),
		   !(I->banderas & 1) ? "este procesador no tiene páginas de 4 MB (PSE), y el núcleo es de 32 bits sin PAE" :
		   !(I->banderas & 2) ? "están por encima de los 4 GB y este procesador no tiene PSE-36 (el núcleo es de 32 bits, sin PAE)" :
		   "están más allá de lo que alcanza el procesador con PSE-36 (40 bits)");
	if (alto_rechazado)
		TX("  No se prueban %s que el núcleo no deja (el cargador o sus módulos están ahí).\r\n", ra_tam(a, alto_rechazado));
	if (total_libre_alto && !alto_inalcanzable && (I->banderas & 2))
		TX("  Por encima de 1 GB: el núcleo de 32 bits no usa PAE, pero con páginas de 4 MB (PSE-36) llega a toda la memoria (también por encima de los 4 GB).\r\n");
	if (!con_226)
		TX("  (Este núcleo no tiene la llamada 226: solo se prueba la memoria de esta tarea, con un núcleo. Actualice Hispa-Pentium.)\r\n");
	/* qué pruebas */
	TX("\r\nPRUEBAS HECHAS%s\r\n", pasadas > 1 ? "" : "");
	if (pasadas > 1)
		TX("  %d de %d pasadas enteras.\r\n", pasadas_hechas, pasadas);
	for (k = 1; k < RA_NPRUEBAS; k++) {
		u64 por = pasadas_hechas > 1 ? hecho_b[k] / (u64)pasadas_hechas : hecho_b[k];

		if (k == RA_TODOS) {
			if (!hecho_b[k]) {
				TX("  - %s: no%s%s.\r\n", ra_nombre_prueba(k), ap_motivo ? ": " : "", motivo_ap(ap_motivo));
				continue;
			}
			TX("  - %s: %d procesadores más que este, cada uno su trozo; %d %% de lo probado; %s; %llu %s.\r\n", ra_nombre_prueba(k), nap ? nap : (int)I->ap,
			   pct(por, probada), (mb_s(a, hecho_b[k] * (completa_ ? 11 : 5), tics_p[k]), a), (unsigned long long)F.por_prueba[k], F.por_prueba[k] == 1 ? "fallo" : "fallos");
			continue;
		}
		if (!hecho_b[k]) {
			if (k == RA_INVERSIONES && nap)
				TX("  - %s: con todos los núcleos (arriba).\r\n", ra_nombre_prueba(k));
			else if (k == RA_DIRECCION && nap)
				TX("  - %s: con todos los núcleos (arriba).\r\n", ra_nombre_prueba(k));
			else
				TX("  - %s: no ha dado tiempo.\r\n", ra_nombre_prueba(k));
			continue;
		}
		TX("  - %s: %d %% de lo probado%s; %llu %s.\r\n", ra_nombre_prueba(k), pct(por > probada ? probada : por, probada),
		   pct(por, probada) < 100 ? " (repartido por toda la memoria: no ha dado tiempo a más)" : "", (unsigned long long)F.por_prueba[k],
		   F.por_prueba[k] == 1 ? "fallo" : "fallos");
	}
	if (mca_n > 0)
		TX("  - Machine Check (%d bancos, antes y después): %d %s de la memoria.\r\n", mca_n, mca_memoria, mca_memoria == 1 ? "error" : "errores");
	if (SM.ecc == 0)
		TX("    (Sin ECC, lo normal en un portátil: el procesador no ve los fallos de la RAM; los ve esta prueba.)\r\n");
	/* los fallos */
	if (F.total) {
		TX("\r\nLOS FALLOS (los primeros %d de %llu)\r\n", F.ndet < 24 ? F.ndet : 24, (unsigned long long)F.total);
		TX("  DIRECCIÓN FÍSICA   ESPERADO  LEÍDO     BITS QUE FALLAN   PRUEBA\r\n");
		for (i = 0; i < F.ndet && i < 24; i++) {
			const struct ra_detalle *e = &F.det[i];
			u32 x = e->esperado ^ e->leido;
			char bits[80];
			int q = 0, j, m = e->fis_sabida ? ra_modulo_de(&SM, e->fis) : -1;

			bits[0] = 0;
			for (j = 0; j < 32 && q < 60; j++)
				if (x & (1u << j))
					q += snprintf(bits + q, sizeof bits - (size_t)q, "%sD%d", q ? "," : "", j + (int)((e->fis >> 2) & 1) * 32);
			TX("  %010llXh%s  %08X  %08X  %-16s  %s%s%s%s\r\n", e->fis, e->fis_sabida ? " " : "?", e->esperado, e->leido, bits, ra_nombre_prueba(e->prueba),
			   m >= 0 ? " («" : "", m >= 0 ? SM.m[m].ranura : "", m >= 0 ? "», aprox.)" : "");
		}
		TX("  (D0-D63: los bits del bus de 64; «?» tras la dirección: es la virtual, la física no se sabe)\r\n");
	}
	TX("\r\nSi el equipo se cuelga probando la memoria, al volver a arrancar el Termómetro lo dice (lo que se estaba probando queda en RAM.EST).\r\n");
	escribe_r(r_txt, T, (u32)nt);
}

/* ------------------------------------------------------------ la hija */
int ram_principal(const char *carpeta, int completa, int simula_fallo)
{
	char a[24], b[24];
	u32 n;
	int p;

	snprintf(r_est, sizeof r_est, "%s\\RAM.EST", carpeta);
	snprintf(r_par, sizeof r_par, "%s\\RAM.PAR", carpeta);
	snprintf(r_txt, sizeof r_txt, "%s\\RAM.TXT", carpeta);
	sc(SC_BORRAR, (u32)r_par, 0, 0);
	hz = sc(SC_DATO, DATO_HZ, 0, 0);
	if (ES_ERROR(hz) || !hz)
		hz = 100;
	t0 = tics();
	simula = simula_fallo;
	completa_ = completa;
	pasadas = completa ? 2 : 1;
	memset(&C, 0, sizeof C);
	C.fallo = fallo_cb;
	C.parar = parar_cb;
	ra_fallos_empieza(&F);
	estado(1, "EN MARCHA · preparando (el mapa de la memoria, el SMBIOS) · 0 s");
	con_226 = sc(SC_RAM, 0, (u32)info_b, 0) == 1;
	if (!con_226)
		memset(info_b, 0, sizeof info_b);
	smb = (unsigned char *)malloc(65536 + 32);
	if (smb && con_226) {
		n = sc(SC_RAM, 4, (u32)smb, 65536 + 32);
		if (!ES_ERROR(n) && n > 32)
			ra_lee_smbios(smb + 32, n - 32, &SM);
	}
	mira_mca(0);
	pide_propia();
	if (con_226)
		prepara_ventana();
	for (p = 0; p < nz; p++)
		if (Z[p].ventana)
			total_ventana += Z[p].bytes;
	serie("termo: ram: %d trozos: propia %llu MB, ventana %llu MB; libre %llu MB (abajo %llu, arriba %llu); límite %u MB; PSE %u PSE-36 %u; %u bits\n", nz,
	      total_propia >> 20, total_ventana >> 20, vista >> 20, total_libre_bajo >> 20, total_libre_alto >> 20, I->limite >> 20, I->banderas & 1, (I->banderas >> 1) & 1, I->fis);
	if (!nz) {
		estado(1, "No hay memoria que probar (el sistema no da más).");
		return 2;
	}
	estado(1, "EN MARCHA · se prueban %s de %s libres · 0 s", ra_tam(a, total_propia + total_ventana), ra_tam(b, vista));
	for (p = 0; p < pasadas && !cancelada; p++) {
		nap = 1;
		if (con_226)
			con_todos(completa, p);
		else
			nap = 0;
		if (!cancelada)
			con_uno(completa, p, completa ? 0 : RAPIDA_S, 0);
		/* (la rápida, si sobra tiempo: la dirección y moving inversions con un núcleo, hasta donde dé) */
		if (!cancelada && !completa && nap && segundos() + 10 < RAPIDA_S)
			con_uno(0, p, RAPIDA_S, 1);
		if (!cancelada)
			pasadas_hechas++;
	}
	if (con_226)
		sc(SC_RAM, 6, 0, 0);
	mira_mca(1);
	escribe_resultado(cancelada);
	if (cancelada) {
		estado(1, "Cancelada (Esc) a los %u s: %s fallos hasta entonces (en RAM.TXT).", segundos(), F.total ? "CON" : "sin");
		return 2;
	}
	if (mal_())
		estado(1, "Hecho: MAL · %llu fallos%s en %s probados (de %s libres) · %u s · todo en RAM.TXT", (unsigned long long)F.total, mca_memoria ? " y errores de Machine Check" : "",
		       ra_tam(a, total_propia + total_ventana), ra_tam(b, vista), segundos());
	else
		estado(1, "Hecho: BIEN · ni un fallo en %s probados (de %s libres)%s · %u s · todo en RAM.TXT", ra_tam(a, total_propia + total_ventana), ra_tam(b, vista),
		       nap ? ", también con todos los núcleos" : "", segundos());
	return mal_() ? 1 : 0;
}

/* ------------------------------------------------------------ en el Termómetro */
static char carpeta_m[96], aviso_m[400], est_ult[400];
static int activa_m, resultado_m = -1;
static u32 t_lanzada;

static void rutas(const char *carpeta)
{
	snprintf(carpeta_m, sizeof carpeta_m, "%s", carpeta);
	snprintf(r_est, sizeof r_est, "%s\\RAM.EST", carpeta);
	snprintf(r_par, sizeof r_par, "%s\\RAM.PAR", carpeta);
	snprintf(r_txt, sizeof r_txt, "%s\\RAM.TXT", carpeta);
}

int ram_al_abrir(const char *carpeta, char *d, int cabe)
{
	char t[400];

	rutas(carpeta);
	hz = sc(SC_DATO, DATO_HZ, 0, 0);
	if (ES_ERROR(hz) || !hz)
		hz = 100;
	if (lee_r(r_est, t, sizeof t) > 0 && !strncmp(t, "EN MARCHA", 9)) {
		char x[440];

		snprintf(d, (size_t)cabe, "La última vez, la prueba de la memoria RAM NO llegó a acabar: se quedó en «%s». Si el equipo se colgó o se apagó solo probando la memoria, "
			 "eso apunta a la RAM o a su controlador (o a la alimentación): repita la prueba y, si vuelve a pasar, pruebe con un solo módulo cada vez.", t + EMN);
		snprintf(x, sizeof x, "Interrumpida (la vez anterior): %s", t + EMN);
		escribe_r(r_est, x, (u32)strlen(x));
		snprintf(aviso_m, sizeof aviso_m, "Memoria RAM: la vez anterior NO llegó a acabar (se quedó probando la memoria): ver el .LOG");
		resultado_m = 1;
		return 1;
	}
	return 0;
}

int ram_linea(const char *palabra)
{
	static char l[600];
	static int leida;

	if (!leida) {
		u32 n = sc(SC_RAM, 7, (u32)l, sizeof l - 1);

		leida = 1;
		l[ES_ERROR(n) ? 0 : n < sizeof l ? n : sizeof l - 1] = 0;
	}
	return busca_r(l, palabra) != 0;
}

int ram_lanza(const char *carpeta, int completa, int simula_fallo)
{
	u32 x;
	const char *arg = completa ? (simula_fallo ? "/RAM /COMPLETA /SIMULAFALLO" : "/RAM /COMPLETA") : (simula_fallo ? "/RAM /SIMULAFALLO" : "/RAM");

	if (activa_m)
		return 0;
	rutas(carpeta);
	escribe_r(r_est, "Empezando...", 12);
	sc(SC_BORRAR, (u32)r_par, 0, 0);
	x = sc(SC_LANZAR_SUELTO, (u32)"\\TERMO.BIN", (u32)arg, 0);
	if (ES_ERROR(x))
		x = sc(SC_LANZAR_SUELTO, (u32)"\\HISPA\\TERMO.BIN", (u32)arg, 0);
	if (ES_ERROR(x)) {
		snprintf(aviso_m, sizeof aviso_m, "Memoria RAM: no se puede poner en marcha TERMO.BIN %s.", arg);
		return 0;
	}
	activa_m = 1;
	t_lanzada = tics();
	est_ult[0] = 0;
	snprintf(aviso_m, sizeof aviso_m, "Memoria RAM: empezando (%s)...", completa ? "la prueba COMPLETA" : "la rápida, menos de 2 min");
	return 1;
}

void ram_para(void)
{
	if (activa_m)
		escribe_r(r_par, "parar", 5);
}

int ram_activa(void) { return activa_m; }
const char *ram_aviso(void) { return aviso_m; }
int ram_resultado(void) { return resultado_m; }

int ram_mira(char *d, int cabe, int *acabada)
{
	char t[400];

	*acabada = 0;
	if (!activa_m)
		return 0;
	if (lee_r(r_est, t, sizeof t) <= 0 || !strcmp(t, est_ult)) {
		/* (la completa puede durar horas; la rápida, si en 10 min no dice nada, se ha perdido) */
		if (tics() - t_lanzada > 600 * hz && !busca_r(aviso_m, "COMPLETA") && !strncmp(est_ult, "Empezando", 9)) {
			activa_m = 0;
			*acabada = 1;
			snprintf(d, (size_t)cabe, "la prueba de la RAM no dice nada desde hace 10 minutos: se da por perdida");
			return 1;
		}
		return 0;
	}
	snprintf(est_ult, sizeof est_ult, "%s", t);
	snprintf(aviso_m, sizeof aviso_m, "Memoria RAM: %s", !strncmp(t, EM, EMN) ? t + EMN : t);
	snprintf(d, (size_t)cabe, "%s", t);
	if (!strncmp(t, "Hecho", 5) || !strncmp(t, "Cancelada", 9) || !strncmp(t, "No hay", 6)) {
		activa_m = 0;
		*acabada = 1;
		resultado_m = !strncmp(t, "Hecho: MAL", 10) ? 1 : !strncmp(t, "Hecho: BIEN", 11) ? 0 : resultado_m;
	}
	return 1;
}

/* la sección de INFORME.TXT: lo de RAM.TXT sin su cabecera, entre marcas */
#define MARCA_INI "\r\nMEMORIA RAM\r\n-----------\r\n"
#define MARCA_FIN "  (fin de lo de la memoria RAM)\r\n"

int ram_informe(char *d, int cabe)
{
	static char t[16000];
	int n = 0, k;
	const char *p;

	if (cabe <= 0)
		return 0;
	d[0] = 0;
	k = carpeta_m[0] ? lee_r(r_txt, t, sizeof t) : -1;
#define DX(...) do { if (n < cabe) n += snprintf(d + n, (size_t)(cabe - n), __VA_ARGS__); if (n > cabe - 1) n = cabe - 1; } while (0)
	DX("%s", MARCA_INI);
	if (k <= 0) {
		if (resultado_m == 1 && aviso_m[0])
			DX("  MAL: la vez anterior, la prueba de la memoria se quedó a medias (el equipo se colgó o se apagó probándola). Ver «LA VEZ ANTERIOR» y el .LOG.\r\n");
		else
			DX("  No se ha probado. Con la tecla M (o el botón «Memoria RAM») se prueba en menos de 2 minutos; la completa, en el menú del arranque.\r\n");
	} else {
		/* (sin la marca UTF-8 ni el título) */
		p = t;
		if (!strncmp(p, "\xEF\xBB\xBF", 3))
			p += 3;
		while (*p && *p != '\n')
			p++;
		if (*p)
			p++;
		for (; *p && n < cabe - (int)sizeof MARCA_FIN - 2; p++)
			d[n++] = *p;
		d[n] = 0;
		/* (la de si la vez anterior se colgó, también) */
		if (resultado_m == 1 && busca_r(aviso_m, "NO llegó"))
			DX("  OJO: además, una vez anterior la prueba se quedó a medias (el equipo se colgó o se apagó probando la memoria).\r\n");
	}
	DX("%s", MARCA_FIN);
#undef DX
	return n;
}

int ram_veredicto(char *d, int cabe)
{
	char t[300];

	d[0] = 0;
	if (lee_r(r_txt, t, sizeof t) <= 0) {
		if (resultado_m == 1) {
			snprintf(d, (size_t)cabe, "La memoria RAM: la vez anterior la prueba se quedó a medias (el equipo se colgó probándola): sospechosa; repetirla.");
			return 1;
		}
		return 0;
	}
	if (busca_r(t, "MEMORIA RAM: MAL"))
		snprintf(d, (size_t)cabe, "La memoria RAM ha FALLADO la prueba (ver MEMORIA RAM): eso basta para que el equipo se cuelgue o no arranque; arreglar eso primero.");
	else if (busca_r(t, "MEMORIA RAM: BIEN"))
		snprintf(d, (size_t)cabe, "La memoria RAM ha pasado la prueba sin un fallo (ver MEMORIA RAM): si el equipo se cuelga, no parece la RAM (aunque una prueba larga, la completa, lo asegura más).");
	else
		snprintf(d, (size_t)cabe, "La memoria RAM: la prueba se canceló; sin fallos en lo probado.");
	return 1;
}

void ram_pon_en_informe(const char *carpeta)
{
	static char inf[24000], sec[17000], nuevo[40000];
	char r[120];
	int n, s, k = 0;
	const char *a, *b;

	snprintf(r, sizeof r, "%s\\INFORME.TXT", carpeta);
	s = ram_informe(sec, sizeof sec);
	n = lee_r(r, inf, sizeof inf);
	if (n <= 0) {
		k = snprintf(nuevo, sizeof nuevo, "\xEF\xBB\xBFINFORME DEL TERMÓMETRO DE HISPA-PENTIUM\r\n=======================================\r\n%s\r\n"
			     "(La prueba de esfuerzo, «Calentar», no se ha hecho todavía: al hacerla, este informe se completa.)\r\n", sec);
	} else if ((a = busca_r(inf, MARCA_INI)) && (b = busca_r(a, MARCA_FIN))) {
		b += strlen(MARCA_FIN);
		memcpy(nuevo, inf, (size_t)(a - inf));
		k = (int)(a - inf);
		memcpy(nuevo + k, sec, (size_t)s);
		k += s;
		k += snprintf(nuevo + k, sizeof nuevo - (size_t)k, "%s", b);
	} else if ((a = busca_r(inf, "\r\nRESULTADO\r\n"))) {
		k = (int)(a - inf);
		memcpy(nuevo, inf, (size_t)k);
		memcpy(nuevo + k, sec, (size_t)s);
		k += s;
		k += snprintf(nuevo + k, sizeof nuevo - (size_t)k, "%s", a);
	} else {
		memcpy(nuevo, inf, (size_t)n);
		k = n;
		memcpy(nuevo + k, sec, (size_t)s);
		k += s;
	}
	escribe_r(r, nuevo, (u32)k);
}
