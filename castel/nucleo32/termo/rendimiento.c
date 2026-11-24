/* rendimiento.c - (TERMO) «Medir rendimiento»: unas medidas tipo «benchmark»
 * mientras el Termómetro vigila la temperatura (TERMO.BIN /MIDE, una tarea
 * hija, como la carga; lo va diciendo en TERMO\MIDE.EST y lo deja en
 * TERMO\RENDIMIE.TXT). Todo con cargas propias y deterministas, en menos de
 * 2 minutos; se para con Esc (el Termómetro deja TERMO\MIDE.PAR):
 *  1. el procesador, un núcleo: enteros (SHA-256 sobre 1 MB; la criba de
 *     Eratóstenes hasta 2 millones) y coma flotante x87 (matrices de 64 x 64);
 *  2. todos los procesadores despertados (la carga de termo.cas: sus vueltas
 *     de coma flotante, y las de este a la vez);
 *  3. la memoria: leer, escribir y copiar 16 MB (MB/s) y la latencia (saltos
 *     al azar por 16 MB, en ns);
 *  4. el disco: SOLO la memoria USB (un archivo propio en TERMO, que se
 *     borra al acabar), leer seguido y a saltos de 4 K. El disco del equipo
 *     no se toca nunca (ni para leer: su sistema de archivos, el de Windows,
 *     no lo lee Hispa-Pentium);
 *  5. pintar: la CPU rellenando una pantalla en la memoria (Mpíxeles/s): sin
 *     aceleración, mide la CPU pintando, no la tarjeta gráfica;
 *  6. el calor: la coma flotante, otra vez al final: si baja, «puntuación
 *     bajada por calor: X % menos que al principio».
 * La puntuación relativa: la del HP Mini 5103 (Atom N455) = 100, si ya hay
 * una medida suya en RENDIMIE.TXT (si no, se dice); y la tabla con todos los
 * equipos medidos que haya en ese archivo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
#include <stdarg.h>
#include "rendimiento.h"

#define SC_CARGA 224
static char carpeta[96], ruta_est[120], ruta_par[120];
static u32 hz = 100;

static u32 tics(void) { return sc(SC_TICS, 0, 0, 0); }

/* (la biblioteca de C de aquí no escribe «%f»: con décimas) */
static const char *d1(double v)
{
	static char t[8][24];
	static int k;
	char *d = t[k++ & 7];
	long x = (long)(v * 10 + (v >= 0 ? 0.5 : -0.5));

	if (x < 0)
		snprintf(d, 24, "-%ld,%ld", -x / 10, -x % 10);
	else
		snprintf(d, 24, "%ld,%ld", x / 10, x % 10);
	return d;
}

static long l10(double v) { return (long)(v * 10 + 0.5); }

static char *busca_en(const char *t, const char *o)
{
	size_t n = strlen(o);

	for (; *t; t++)
		if (!strncmp(t, o, n))
			return (char *)t;
	return 0;
}

static void estado(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void estado(const char *fmt, ...)
{
	char t[300];
	va_list v;
	u32 m;

	va_start(v, fmt);
	vsnprintf(t, sizeof t, fmt, v);
	va_end(v);
	serie("termo: rendimiento: %s\n", t);
	m = sc(SC_ABRIR, (u32)ruta_est, PARA_CREAR, 0);
	if (!ES_ERROR(m)) {
		sc(SC_ESCRIBIR_ARCHIVO, m, (u32)t, (u32)strlen(t));
		sc(SC_CERRAR, m, 0, 0);
	}
}

static int parar(void)
{
	u32 m = sc(SC_ABRIR, (u32)ruta_par, PARA_LEER, 0);

	if (ES_ERROR(m))
		return 0;
	sc(SC_CERRAR, m, 0, 0);
	return 1;
}

/* ---- SHA-256 (lo justo) ---- */
static const u32 K256[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_bloque(u32 h[8], const u8 *p)
{
	u32 w[64], a, b, c, d, e, f, g, k, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = (u32)p[4 * i] << 24 | (u32)p[4 * i + 1] << 16 | (u32)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (; i < 64; i++)
		w[i] = (ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10)) + w[i - 7] + (ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 16];
	a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
	for (i = 0; i < 64; i++) {
		t1 = k + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
		t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		k = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
	}
	h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += k;
}

/* ---- las medidas: cada una, «s» segundos (o hasta Esc) ---- */
static u8 *buf1, *buf2;
#define MB16 (16u << 20)

static double mide_sha(int s, u32 *salida)
{
	u32 h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 }, t0 = tics(), n = 0, i;

	do {
		for (i = 0; i < (1u << 20); i += 64)
			sha_bloque(h, buf1 + i);
		n++;
	} while (tics() - t0 < (u32)s * hz && !parar());
	*salida = h[0];
	return (double)n * hz / (double)(tics() - t0 + 1);	/* MB/s */
}

static double mide_criba(int s)
{
	u32 t0 = tics(), n = 0, i, j, primos = 0;
	const u32 N = 2000000;

	do {
		memset(buf2, 1, N);
		primos = 0;
		for (i = 2; i < N; i++)
			if (buf2[i]) {
				primos++;
				for (j = i + i; j < N; j += i)
					buf2[j] = 0;
			}
		n++;
	} while (tics() - t0 < (u32)s * hz && !parar());
	(void)primos;		/* (148933) */
	return (double)n * hz / (double)(tics() - t0 + 1);	/* cribas por segundo */
}

static double ma[64 * 64], mb[64 * 64], mc[64 * 64];

static double mide_matrices(int s)
{
	u32 t0 = tics(), n = 0;
	int i, j, k;

	for (i = 0; i < 64 * 64; i++)
		ma[i] = (double)(i % 17) * 0.25 + 1.0, mb[i] = (double)(i % 13) * 0.5 - 2.0;
	do {
		for (i = 0; i < 64; i++)
			for (j = 0; j < 64; j++) {
				double acu = 0;

				for (k = 0; k < 64; k++)
					acu += ma[i * 64 + k] * mb[k * 64 + j];
				mc[i * 64 + j] = acu;
			}
		n++;
	} while (tics() - t0 < (u32)s * hz && !(n % 16 == 0 && parar()));
	return (double)n * (2.0 * 64 * 64 * 64) * hz / (double)(tics() - t0 + 1) / 1e6;	/* MFLOPS */
}

static double mide_memoria(int que, int s)
{
	u32 t0 = tics(), n = 0;
	volatile u32 suma = 0;

	do {
		if (que == 0) {
			const u32 *p = (const u32 *)buf1;
			u32 i, acu = 0;

			for (i = 0; i < MB16 / 4; i += 4)
				acu += p[i] + p[i + 1] + p[i + 2] + p[i + 3];
			suma += acu;
		} else if (que == 1)
			memset(buf1, (int)n, MB16);
		else
			memcpy(buf2, buf1, MB16);
		n++;
	} while (tics() - t0 < (u32)s * hz && !parar());
	return (double)n * 16.0 * hz / (double)(tics() - t0 + 1);	/* MB/s */
}

/* la latencia: una cadena de saltos al azar por 16 MB (de 64 en 64 octetos) */
static volatile u32 sumidero;
static double mide_latencia(void)
{
	u32 *p = (u32 *)buf1, n = MB16 / 64, i, x = 0, semilla = 12345, t0, pasos = 0;
	u64 c0, c1;

	for (i = 0; i < n; i++)
		p[i * 16] = i;
	for (i = n - 1; i > 0; i--) {	/* (Sattolo: una sola vuelta que pasa por todos; generador fijo) */
		u32 j, t;

		semilla = semilla * 1103515245u + 12345u;
		j = (semilla >> 8) % i;
		t = p[i * 16], p[i * 16] = p[j * 16], p[j * 16] = t;
	}
	t0 = tics();
	c0 = rdtsc();
	while (tics() - t0 < hz * 2) {
		for (i = 0; i < 100000; i++)
			x = p[x * 16];
		pasos += 100000;
	}
	c1 = rdtsc();
	(void)c1, (void)c0;
	if (!pasos)
		return 0;
	sumidero = x;		/* (que el compilador no se salte la cadena: el resultado se usa) */
	return (double)(tics() - t0) * 1e9 / hz / (double)pasos;	/* ns */
}

/* el disco: un archivo de 8 MB en TERMO (la memoria USB), leído seguido y a
 * saltos de 4 K; se borra al acabar. Da los MB/s seguidos y las lecturas/s */
static int mide_disco(double *seguido, double *saltos)
{
	char r[120];
	u32 m, t0, i, n, semilla = 777;

	*seguido = *saltos = 0;
	snprintf(r, sizeof r, "%s\\RENDIM.DAT", carpeta);
	m = sc(SC_ABRIR, (u32)r, PARA_CREAR, 0);
	if (ES_ERROR(m))
		return 0;
	for (i = 0; i < 8; i++)
		if (sc(SC_ESCRIBIR_ARCHIVO, m, (u32)buf1 + i * (1u << 20), 1u << 20) != (1u << 20))
			break;
	sc(SC_CERRAR, m, 0, 0);
	if (i < 8) {
		sc(SC_BORRAR, (u32)r, 0, 0);
		return 0;
	}
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m)) {
		sc(SC_BORRAR, (u32)r, 0, 0);
		return 0;
	}
	t0 = tics();
	for (i = 0; i < 8 && !parar(); i++)
		sc(SC_LEER, m, (u32)buf2, 1u << 20);
	*seguido = (double)i * hz / (double)(tics() - t0 + 1);
	t0 = tics();
	for (n = 0; tics() - t0 < hz * 3 && !parar(); n++) {
		semilla = semilla * 1103515245u + 12345u;
		sc(SC_MOVER, m, ((semilla >> 8) % 2048) * 4096, 0);
		sc(SC_LEER, m, (u32)buf2, 4096);
	}
	*saltos = (double)n * hz / (double)(tics() - t0 + 1);
	sc(SC_CERRAR, m, 0, 0);
	sc(SC_BORRAR, (u32)r, 0, 0);
	return 1;
}

/* pintar: la CPU rellenando una pantalla de 1024 x 768 (rectángulos y una
 * degradación, como un escritorio), en la memoria */
static double mide_pintar(int s)
{
	u32 *p = (u32 *)buf1, t0 = tics(), n = 0, x, y;

	do {
		for (y = 0; y < 768; y++) {
			u32 c = 0x203040 + (y << 8) + n;

			for (x = 0; x < 1024; x++)
				p[y * 1024 + x] = c + x;
		}
		n++;
	} while (tics() - t0 < (u32)s * hz && !parar());
	return (double)n * 1024.0 * 768.0 * hz / (double)(tics() - t0 + 1) / 1e6;	/* Mpíxeles/s */
}

/* todos los procesadores: los demás cargan (termo.cas) y este mide sus
 * matrices; las vueltas de los demás (cada una, 300.000 operaciones de coma
 * flotante: 100.000 x fsqrt, fadd, fsqrt) */
struct ap_ficha { u32 apic, estado, vueltas, msr, lecturas, latido, tsc, z; };
struct ap_tabla { u32 n, motivo, base, r; struct ap_ficha e[16]; };

/* la vuelta de los demás procesadores (termo.cas: 100.000 veces fsqrt, fadd,
 * fsqrt), aquí también: así se cuenta igual en todos */
static u32 vueltas_aqui(u32 hasta)
{
	u32 n = 0;

	while (tics() < hasta) {
		__asm__ volatile("fld1\n\tmov $100000, %%ecx\n1:\n\tfsqrt\n\tfadd %%st(0), %%st(0)\n\tfsqrt\n\tdec %%ecx\n\tjnz 1b\n\tfstp %%st(0)" ::: "ecx", "st");
		n++;
	}
	return n;
}

/* vueltas por segundo: este solo (*uno) y todos a la vez (lo que da) */
static double mide_todos(int *cuantos, double *uno)
{
	static struct ap_tabla a0, a1;
	u32 t0, t1, i, n;
	double ap = 0;

	t0 = tics();
	n = vueltas_aqui(t0 + 4 * hz);
	*uno = (double)n * hz / (double)(tics() - t0 + 1);
	sc(SC_CARGA, 1, 1, 0);
	sc(SC_CARGA, 4, (u32)&a0, 0);
	t0 = tics();
	n = vueltas_aqui(t0 + 6 * hz);
	t1 = tics();
	sc(SC_CARGA, 4, (u32)&a1, 0);
	sc(SC_CARGA, 1, 0, 0);
	*cuantos = (int)a1.n;
	for (i = 0; i < a1.n && i < 16; i++)
		ap += (double)(a1.e[i].vueltas - a0.e[i].vueltas);
	return ((double)n + ap) * hz / (double)(t1 - t0 + 1);
}

/* ---- el archivo y la tabla ---- */
static int lee_todo(char *d, int cabe)
{
	char r[120];
	u32 m, n;

	snprintf(r, sizeof r, "%s\\RENDIMIE.TXT", carpeta);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return 0;
	n = sc(SC_LEER, m, (u32)d, (u32)cabe - 1);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n))
		n = 0;
	d[n] = 0;
	return (int)n;
}

/* «MEDIDA;equipo;fecha;sha;criba;mflops;todos;leer;escribir;copiar;lat;usb;usb4k;pintar;calor» */
/* (en el archivo, cada número en décimas) */
static double campo(const char *l, int k)
{
	int i;
	long v = 0;

	for (i = 0; i < k && *l; l++)
		if (*l == ';')
			i++;
	while (*l >= '0' && *l <= '9')
		v = v * 10 + (*l++ - '0');
	return (double)v / 10;
}

int rendimiento_principal(const char *carpeta_termo, const char *equipo, const char *procesador)
{
	static char todo[16384], linea[600], t[4096];
	char fecha[32];
	double sha, criba, mfl, mfl2, todos, uno, lee, esc, cop, lat, usb = 0, usb4k = 0, pint, calor, ref_sha = 0, ref_mfl = 0;
	u32 f, sal;
	int cuantos = 0, n, k;

	snprintf(carpeta, sizeof carpeta, "%s", carpeta_termo);
	snprintf(ruta_est, sizeof ruta_est, "%s\\MIDE.EST", carpeta);
	snprintf(ruta_par, sizeof ruta_par, "%s\\MIDE.PAR", carpeta);
	sc(SC_BORRAR, (u32)ruta_par, 0, 0);
	hz = sc(SC_DATO, DATO_HZ, 0, 0);
	if (ES_ERROR(hz) || !hz)
		hz = 100;
	buf1 = (u8 *)malloc(MB16);
	buf2 = (u8 *)malloc(MB16);
	if (!buf1 || !buf2) {
		estado("No hay memoria para medir (hacen falta 32 MB).");
		return 1;
	}
	for (k = 0; k < (int)MB16; k++)
		buf1[k] = (u8)(k * 7 + (k >> 9));
	estado("1/7 El procesador, un núcleo: enteros (SHA-256)...");
	sha = mide_sha(5, &sal);
	if (parar())
		goto cancelado;
	estado("1/7 El procesador, un núcleo: enteros (criba)...");
	criba = mide_criba(4);
	estado("2/7 El procesador, un núcleo: coma flotante (matrices)...");
	mfl = mide_matrices(5);
	if (parar())
		goto cancelado;
	estado("3/7 Todos los procesadores a la vez...");
	todos = mide_todos(&cuantos, &uno);
	if (parar())
		goto cancelado;
	estado("4/7 La memoria: leer, escribir, copiar y la latencia...");
	lee = mide_memoria(0, 3);
	esc = mide_memoria(1, 3);
	cop = mide_memoria(2, 3);
	lat = mide_latencia();
	if (parar())
		goto cancelado;
	estado("5/7 El disco: la memoria USB (un archivo propio en TERMO; el disco del equipo, no)...");
	mide_disco(&usb, &usb4k);
	if (parar())
		goto cancelado;
	estado("6/7 Pintar (la CPU rellenando una pantalla, sin aceleración)...");
	pint = mide_pintar(4);
	estado("7/7 Otra vez la coma flotante (¿baja con el calor?)...");
	mfl2 = mide_matrices(5);
	calor = mfl > 0 ? (mfl - mfl2) * 100.0 / mfl : 0;
	/* el archivo: una línea por medida (para la tabla) y el texto */
	f = sc(SC_DATO, DATO_FECHA, 0, 0);
	snprintf(fecha, sizeof fecha, "%02u/%02u/%04u %02u:%02u", (f >> 16) & 31, (f >> 21) & 15, 1980 + (f >> 25), (f & 0xFFFF) >> 11, (f >> 5) & 63);
	/* «MEDIDA;equipo;fecha;» y los números en décimas (para la tabla) */
	snprintf(linea, sizeof linea, "MEDIDA;%s;%s;%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld\r\n", equipo, fecha, l10(sha), l10(criba), l10(mfl), l10(todos),
		 l10(lee), l10(esc), l10(cop), l10(lat), l10(usb), l10(usb4k), l10(pint), l10(calor < 0 ? 0 : calor));
	n = lee_todo(todo, sizeof todo);
	/* la referencia: la primera medida de un HP Mini (el 5103, Atom N455) */
	{
		char *p = todo;

		while (p && *p) {
			if (!strncmp(p, "MEDIDA;", 7) && busca_en(p, "HP Mini")) {
				ref_sha = campo(p, 3);
				ref_mfl = campo(p, 5);
				break;
			}
			p = strchr(p, '\n');
			if (p)
				p++;
		}
		if (!ref_sha && busca_en(equipo, "HP Mini"))
			ref_sha = sha, ref_mfl = mfl;
	}
	k = snprintf(t, sizeof t, "\r\nRENDIMIENTO - %s - %s\r\nProcesador: %s\r\n", equipo, fecha, procesador);
	k += snprintf(t + k, sizeof t - (size_t)k, "  Un núcleo, enteros: SHA-256 %s MB/s; criba hasta 2 millones %s veces/s\r\n", d1(sha), d1(criba));
	k += snprintf(t + k, sizeof t - (size_t)k, "  Un núcleo, coma flotante (x87, matrices 64x64): %s MFLOPS\r\n", d1(mfl));
	k += snprintf(t + k, sizeof t - (size_t)k, "  Todos los procesadores (%d): %s vueltas/s, %s veces lo de uno solo (%s vueltas/s; la vuelta: 300.000 operaciones x87)\r\n",
		      cuantos + 1, d1(todos), d1(uno > 0 ? todos / uno : 0), d1(uno));
	k += snprintf(t + k, sizeof t - (size_t)k, "  Memoria: leer %s MB/s, escribir %s MB/s, copiar %s MB/s; latencia %s ns\r\n", d1(lee), d1(esc), d1(cop), d1(lat));
	if (usb > 0)
		k += snprintf(t + k, sizeof t - (size_t)k, "  Memoria USB (archivo de prueba propio): leer seguido %s MB/s, a saltos de 4 K %s lecturas/s\r\n", d1(usb), d1(usb4k));
	else
		k += snprintf(t + k, sizeof t - (size_t)k, "  Memoria USB: no se ha podido medir\r\n");
	k += snprintf(t + k, sizeof t - (size_t)k, "  Disco del equipo: lo mide el Termómetro aparte, solo leyendo (TERMO\\DISCOS.TXT y en INFORME.TXT, «DISCOS»)\r\n");
	k += snprintf(t + k, sizeof t - (size_t)k, "  Pintar: %s Mpíxeles/s (sin aceleración: mide la CPU pintando, no la tarjeta gráfica)\r\n", d1(pint));
	if (calor >= 5)
		k += snprintf(t + k, sizeof t - (size_t)k, "  PUNTUACIÓN BAJADA POR CALOR: la coma flotante, al final, un %s %% menos que al principio\r\n", d1(calor));
	else
		k += snprintf(t + k, sizeof t - (size_t)k, "  Con el calor: la coma flotante, al final, igual que al principio (o mejor)\r\n");
	if (ref_sha > 0 && ref_mfl > 0)
		k += snprintf(t + k, sizeof t - (size_t)k, "  Puntuación (HP Mini 5103, Atom N455 = 100): enteros %ld, coma flotante %ld\r\n", (long)(sha * 100 / ref_sha + 0.5), (long)(mfl * 100 / ref_mfl + 0.5));
	else
		k += snprintf(t + k, sizeof t - (size_t)k, "  Puntuación relativa: sin referencia todavía (la referencia es el HP Mini 5103: mídalo con esta memoria)\r\n");
	/* la tabla: todas las medidas del archivo, y esta */
	k += snprintf(t + k, sizeof t - (size_t)k, "\r\n  Equipos medidos (SHA-256 MB/s · MFLOPS 1 núcleo · vueltas/s todos · memoria leer MB/s):\r\n");
	{
		char *p = todo;
		int filas = 0;

		while (p && *p && k < (int)sizeof t - 400) {
			if (!strncmp(p, "MEDIDA;", 7)) {
				char eq[48];
				int i = 0;
				const char *q = p + 7;

				while (*q && *q != ';' && i < (int)sizeof eq - 1)
					eq[i++] = *q++;
				eq[i] = 0;
				k += snprintf(t + k, sizeof t - (size_t)k, "    %-36s %8s %9s %9s %8s\r\n", eq, d1(campo(p, 3)), d1(campo(p, 5)), d1(campo(p, 6)), d1(campo(p, 7)));
				filas++;
			}
			p = strchr(p, '\n');
			if (p)
				p++;
		}
		k += snprintf(t + k, sizeof t - (size_t)k, "    %-36s %8s %9s %9s %8s  (esta)\r\n", equipo, d1(sha), d1(mfl), d1(todos), d1(lee));
	}
	{
		char r[120];
		u32 m;

		snprintf(r, sizeof r, "%s\\RENDIMIE.TXT", carpeta);
		m = sc(SC_ABRIR, (u32)r, n ? PARA_ESCRIBIR : PARA_CREAR, 0);
		if (!ES_ERROR(m)) {
			if (n)
				sc(SC_MOVER, m, 0, 2);
			sc(SC_ESCRIBIR_ARCHIVO, m, (u32)linea, (u32)strlen(linea));
			sc(SC_ESCRIBIR_ARCHIVO, m, (u32)t, (u32)k);
			sc(SC_CERRAR, m, 0, 0);
		}
	}
	estado("Hecho: SHA-256 %s MB/s, %s MFLOPS (1 núcleo), todos %s veces uno, memoria %s MB/s%s. Todo en RENDIMIE.TXT.", d1(sha), d1(mfl), d1(uno > 0 ? todos / uno : 0), d1(lee),
	       calor >= 5 ? "; BAJADA POR CALOR" : "");
	return 0;
cancelado:
	sc(SC_CARGA, 1, 0, 0);
	estado("Medida cancelada (Esc).");
	return 2;
}
