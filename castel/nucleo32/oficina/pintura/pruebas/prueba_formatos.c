/* prueba_formatos.c - Pintura 2: el arnés de las pruebas de los formatos (pruebas/formatos.sh).
 *   prueba_formatos ida CARPETA              ida y vuelta con las imágenes de prueba
 *                                            (y deja w_*.png, .bmp... para ImageMagick)
 *   prueba_formatos compara ARCH RGBA AN AL [PSNR]
 *                                            lo que leemos de ARCH frente a lo que dice
 *                                            ImageMagick (RGBA: sus puntos, de 8 bits)
 *   prueba_formatos fuzz N SEMILLA ARCH...   N mutaciones de cada muestra
 *   prueba_formatos enormes                  cabeceras con medidas enormes
 * Cada cosa, una línea «bien: ...» o «mal: ...».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "formatos.h"

const char ofi_programa[] = "PRUEBAFI";

static void bien(const char *f, ...)
{
	va_list a;

	printf("  bien: ");
	va_start(a, f);
	vprintf(f, a);
	va_end(a);
	printf("\n");
	fflush(stdout);
}

static void mal(const char *f, ...)
{
	va_list a;

	printf("  mal: ");
	va_start(a, f);
	vprintf(f, a);
	va_end(a);
	printf("\n");
	fflush(stdout);
}

static u8 *lee_todo(const char *r, u32 *n)
{
	FILE *f = fopen(r, "rb");
	long t;
	u8 *d;

	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	t = ftell(f);
	fseek(f, 0, SEEK_SET);
	d = malloc(t > 0 ? (size_t)t : 1);
	if (d && fread(d, 1, (size_t)t, f) != (size_t)t) {
		free(d);
		d = NULL;
	}
	fclose(f);
	*n = (u32)t;
	return d;
}

static void escribe_todo(const char *r, const void *d, u32 n)
{
	FILE *f = fopen(r, "wb");

	if (f) {
		fwrite(d, 1, n, f);
		fclose(f);
	}
}

/* ---- las imágenes de prueba ---- */
static u32 azar_s = 12345;
static u32 azar(void)
{
	azar_s = azar_s * 1103515245u + 12345u;
	return azar_s >> 8;
}

typedef struct {
	const char *nombre;
	int an, al;
	u32 *px;
} muestra;

static u32 *nueva(int an, int al)
{
	return so_pide((size_t)an * al * 4);
}

/* 0: degradado con alfa; 1: pocos colores; 2: pocos con transparente; 3: 1x1;
 * 4: 333x77 (ancho impar, muchos colores, opaco); 5: mucho detalle; 6: 256 colores
 * justos; 7: 255 colores y el transparente; 8: 1x1 transparente */
static muestra haz(int k)
{
	muestra m;
	int x, y;

	memset(&m, 0, sizeof m);
	switch (k) {
	case 0:
		m.nombre = "degradado"; m.an = 256; m.al = 200;
		m.px = nueva(m.an, m.al);
		for (y = 0; y < m.al; y++)
			for (x = 0; x < m.an; x++)
				m.px[y * m.an + x] = (u32)(x * 255 / 255) << 24 | (u32)x << 16 | (u32)(y * 255 / 199) << 8 | (u32)((x + y) & 255);
		break;
	case 1:
	case 2: {
		static const u32 c[7] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000, 0x808080, 0xC0FFEE };

		m.nombre = k == 1 ? "pocos" : "pocos_alfa"; m.an = 120; m.al = 90;
		m.px = nueva(m.an, m.al);
		for (y = 0; y < m.al; y++)
			for (x = 0; x < m.an; x++) {
				u32 v = 0xFF000000u | c[(x / 13 + y / 7) % 7];

				if (k == 2 && (x - 60) * (x - 60) + (y - 45) * (y - 45) < 400)
					v = 0x00000000u;
				m.px[y * m.an + x] = v;
			}
		break;
	}
	case 3:
		m.nombre = "uno"; m.an = 1; m.al = 1;
		m.px = nueva(1, 1);
		m.px[0] = 0xFF123456u;
		break;
	case 4:
		m.nombre = "impar"; m.an = 333; m.al = 77;
		m.px = nueva(m.an, m.al);
		for (y = 0; y < m.al; y++)
			for (x = 0; x < m.an; x++) {
				double fx = x / 332.0, fy = y / 76.0;

				m.px[y * m.an + x] = 0xFF000000u | (u32)(255 * fx) << 16 | (u32)(255 * fy) << 8 |
						     (u32)(127.5 + 127.5 * sin(fx * 6.0 + fy * 3.0));
			}
		break;
	case 5:
		m.nombre = "detalle"; m.an = 200; m.al = 150;
		m.px = nueva(m.an, m.al);
		azar_s = 7;
		for (y = 0; y < m.al; y++)
			for (x = 0; x < m.an; x++)
				m.px[y * m.an + x] = 0xFF000000u | (azar() & 0xFFFFFF);
		break;
	case 6:
		m.nombre = "justos256"; m.an = 64; m.al = 48;
		m.px = nueva(m.an, m.al);
		for (y = 0; y < m.al; y++)
			for (x = 0; x < m.an; x++) {
				u32 i = (u32)(y * m.an + x) % 256;

				m.px[y * m.an + x] = 0xFF000000u | (i * 37 & 255) << 16 | (i * 91 & 255) << 8 | i;
			}
		break;
	case 7:
		m.nombre = "trans255"; m.an = 64; m.al = 48;
		m.px = nueva(m.an, m.al);
		for (y = 0; y < m.al; y++)
			for (x = 0; x < m.an; x++) {
				u32 i = (u32)(y * m.an + x) % 256;

				m.px[y * m.an + x] = i == 255 ? 0x40FFFFFFu : 0xFF000000u | (i * 37 & 255) << 16 | (i * 91 & 255) << 8 | i;
			}
		break;
	case 8:
		m.nombre = "uno_transp"; m.an = 1; m.al = 1;
		m.px = nueva(1, 1);
		m.px[0] = 0x00ABCDEFu;
		break;
	case 9:
		m.nombre = "maxima"; m.an = FI_MAX_LADO; m.al = FI_MAX_LADO;
		m.px = nueva(m.an, m.al);
		for (y = 0; y < m.al; y++)
			for (x = 0; x < m.an; x++)
				m.px[y * m.an + x] = (x ^ y) & 64 ? 0xFF2040C0u : 0xFF000000u | (u32)(x >> 4) << 16 | (u32)(y >> 4) << 8 | 0x80;
		break;
	}
	return m;
}
#define N_MUESTRAS 10

static int opaca(const muestra *m)
{
	return !fi_hay_transparencia(m->px, m->an, m->al);
}

/* los colores distintos (contando uno para los de alfa < 128); hasta 300 */
static int colores_gif(const muestra *m)
{
	static u32 v[300];
	int n = 0, i, j, t = 0;

	for (i = 0; i < m->an * m->al; i++) {
		u32 c = m->px[i];

		if (c >> 24 < 128) {
			t = 1;
			continue;
		}
		c &= 0xFFFFFF;
		for (j = 0; j < n && v[j] != c; j++)
			;
		if (j == n) {
			if (n == 299)
				return 300;
			v[n++] = c;
		}
	}
	return n + t;
}

static u32 blanco(u32 c)
{
	u32 a = c >> 24, r, g, b;

	r = ((c >> 16 & 255) * a + 255 * (255 - a) + 127) / 255;
	g = ((c >> 8 & 255) * a + 255 * (255 - a) + 127) / 255;
	b = ((c & 255) * a + 255 * (255 - a) + 127) / 255;
	return 0xFF000000u | r << 16 | g << 8 | b;
}

/* el PSNR (en RGB) de b frente a a (sobre blanco) */
static double psnr(const u32 *a, const u32 *b, u32 n)
{
	double s = 0;
	u32 i;
	int k;

	for (i = 0; i < n; i++) {
		u32 x = blanco(a[i]), y = blanco(b[i]);

		for (k = 0; k < 24; k += 8) {
			double d = (double)(int)((x >> k & 255) - (y >> k & 255));

			s += d * d;
		}
	}
	s /= 3.0 * n;
	return s <= 1e-12 ? 99.0 : 10.0 * log10(255.0 * 255.0 / s);
}

/* ¿iguales? (con alfa 0, el color no cuenta). Da los distintos */
static u32 distintos(const u32 *a, const u32 *b, u32 n)
{
	u32 i, k = 0;

	for (i = 0; i < n; i++)
		if (a[i] != b[i] && (a[i] >> 24 || b[i] >> 24))
			k++;
	return k;
}

static u32 *lee_mem(const bufer *b, int pista, int *an, int *al, char *e)
{
	u8 *d = malloc(b->n);
	u32 *r;

	memcpy(d, b->d, b->n);
	r = fi_lee(d, b->n, pista, an, al, e, 200);
	free(d);
	return r;
}

static int ida(const char *carpeta)
{
	static const int fs[] = { FI_PNG, FI_BMP, FI_PCX, FI_GIF, FI_JPG };
	int k, j;
	char e[200], ruta[600];

	for (k = 0; k < N_MUESTRAS; k++) {
		u32 antes = so_memoria_usada();
		muestra m = haz(k);
		u32 np = (u32)m.an * m.al;

		for (j = 0; j < 5; j++) {
			int f = fs[j], a, b;
			bufer bf = { 0 };
			u32 *r, i, *esp;

			if (fi_escribe(&bf, m.px, m.an, m.al, f, 90, e, sizeof e) < 0) {
				mal("%s a %s: no se escribe (%s)", m.nombre, fi_nombre(f), e);
				buf_suelta(&bf);
				continue;
			}
			fmt(ruta, sizeof ruta, "%s/w_%s.%s", carpeta, m.nombre, fi_extension(f));
			if (k != 9)
				escribe_todo(ruta, bf.d, bf.n);
			r = lee_mem(&bf, f, &a, &b, e);
			if (!r) {
				mal("%s en %s: no se lee lo escrito (%s)", m.nombre, fi_nombre(f), e);
				buf_suelta(&bf);
				continue;
			}
			if (a != m.an || b != m.al) {
				mal("%s en %s: mide %dx%d y no %dx%d", m.nombre, fi_nombre(f), a, b, m.an, m.al);
			} else if (f == FI_PNG || f == FI_BMP) {
				u32 d = distintos(m.px, r, np);

				if (d)
					mal("%s en %s: %u puntos distintos", m.nombre, fi_nombre(f), d);
				else
					bien("%s en %s (%u octetos%s): idéntico punto a punto", m.nombre, fi_nombre(f), bf.n,
					     f == FI_BMP ? (opaca(&m) ? ", 24 bits" : ", 32 bits con alfa") : opaca(&m) ? ", RGB" : ", RGBA");
			} else if (f == FI_PCX) {
				esp = malloc(np * 4);
				for (i = 0; i < np; i++)
					esp[i] = blanco(m.px[i]);
				if (distintos(esp, r, np))
					mal("%s en PCX: %u puntos distintos", m.nombre, distintos(esp, r, np));
				else
					bien("%s en PCX (%u octetos, %s): idéntico%s", m.nombre, bf.n, bf.d[65] == 1 ? "8 bits y paleta" : "24 bits",
					     opaca(&m) ? "" : " (sobre blanco)");
				free(esp);
			} else if (f == FI_GIF) {
				int nc = colores_gif(&m);

				esp = malloc(np * 4);
				for (i = 0; i < np; i++)
					esp[i] = m.px[i] >> 24 < 128 ? 0 : (m.px[i] | 0xFF000000u);
				if (nc <= 256) {
					if (distintos(esp, r, np))
						mal("%s en GIF (%d colores): %u puntos distintos", m.nombre, nc, distintos(esp, r, np));
					else
						bien("%s en GIF (%d colores, %u octetos): idéntico, sin pérdida", m.nombre, nc, bf.n);
				} else {
					double p = psnr(esp, r, np);
					u32 t = 0;

					for (i = 0; i < np; i++)
						if ((esp[i] >> 24 == 0) != (r[i] >> 24 == 0))
							t++;
					if (t)
						mal("%s en GIF reducido: %u puntos con la transparencia cambiada", m.nombre, t);
					else if (p < (k == 5 ? 14.0 : 28.0))
						mal("%s en GIF reducido a 255 colores: PSNR %.1f dB", m.nombre, p);
					else
						bien("%s en GIF reducido a 255 colores (corte de la mediana y Floyd-Steinberg): PSNR %.1f dB", m.nombre, p);
				}
				free(esp);
			} else {
				double p = psnr(m.px, r, np);

				if (k != 0 && k != 3 && k != 4 && k != 8 && k != 9)
					bien("%s en JPG: se lee (PSNR %.1f dB: no es una foto, tiene bordes o ruido)", m.nombre, p);
				else if (p >= 30.0)
					bien("%s en JPG a calidad 90 (%u octetos): PSNR %.1f dB", m.nombre, bf.n, p);
				else
					mal("%s en JPG a calidad 90: PSNR %.1f dB (< 30)", m.nombre, p);
				if (k == 0 || k == 4 || k == 5) {
					bufer b30 = { 0 };

					if (fi_escribe(&b30, m.px, m.an, m.al, FI_JPG, 30, e, sizeof e) == 0 && b30.n < bf.n)
						bien("%s en JPG: a calidad 30 pesa menos (%u) que a 90 (%u)", m.nombre, b30.n, bf.n);
					else
						mal("%s en JPG: a calidad 30 pesa %u y a 90 %u", m.nombre, b30.n, bf.n);
					buf_suelta(&b30);
				}
			}
			so_suelta(r);
			buf_suelta(&bf);
		}
		so_suelta(m.px);
		if (so_memoria_usada() != antes)
			mal("%s: se pierde memoria (%u octetos)", m.nombre, so_memoria_usada() - antes);
	}
	/* lo que no se escribe */
	{
		muestra m = haz(3);
		bufer bf = { 0 };

		if (fi_escribe(&bf, m.px, 1, 1, FI_ICO, 90, e, sizeof e) < 0 && fi_escribe(&bf, m.px, 5000, 1, FI_PNG, 90, e, sizeof e) < 0 &&
		    fi_escribe(&bf, m.px, 0, 1, FI_BMP, 90, e, sizeof e) < 0)
			bien("no se escribe ICO ni lo que pasa de %d (ni lo vacío): «%s»", FI_MAX_LADO, e);
		else
			mal("se escribe lo que no se debe");
		buf_suelta(&bf);
		so_suelta(m.px);
	}
	/* las extensiones y los nombres */
	if (fi_por_extension("A.PNG") == FI_PNG && fi_por_extension("c:\\x\\foto.jpeg") == FI_JPG && fi_por_extension("x.Jpe") == FI_JPG &&
	    fi_por_extension("a.cur") == FI_ICO && fi_por_extension("a.tga") == FI_TGA && fi_por_extension("a.pcx") == FI_PCX &&
	    fi_por_extension("a.txt") == FI_NADA && fi_por_extension("pcx") == FI_NADA && iguales(fi_nombre(FI_GIF), "GIF") &&
	    iguales(fi_extension(FI_BMP), "bmp") && fi_se_escribe(FI_PCX) && !fi_se_escribe(FI_TGA) && !fi_se_escribe(FI_ICO))
		bien("las extensiones, los nombres y lo que se escribe");
	else
		mal("las extensiones o los nombres");
	{
		const u8 basura[] = "esto no es una imagen, es un texto cualquiera...";
		int a, b;
		u32 *r = fi_lee(basura, sizeof basura, FI_NADA, &a, &b, e, sizeof e);

		if (!r && strstr(e, "no es una imagen que Pintura sepa leer"))
			bien("un texto: «%s»", e);
		else
			mal("un texto: %s", r ? "¡se lee!" : e);
		so_suelta(r);
	}
	return 0;
}

/* ---- lo nuestro frente a lo de ImageMagick ---- */
/* inv: ImageMagick lee los PCX de 1 bit al revés (1 = negro, sin mirar la paleta) */
static int compara(const char *arch, const char *rgba, int an, int al, double minimo, int inv)
{
	u32 n, m, *r, *im, i;
	u8 *d = lee_todo(arch, &n), *q = lee_todo(rgba, &m);
	char e[200];
	int a, b;
	const char *nom = strrchr(arch, '/') ? strrchr(arch, '/') + 1 : arch;

	if (!d || !q || m != (u32)an * al * 4) {
		mal("%s: no está o ImageMagick no lo lee (%u octetos de RGBA para %dx%d)", nom, m, an, al);
		free(d);
		free(q);
		return 1;
	}
	r = fi_lee(d, n, fi_por_extension(arch), &a, &b, e, sizeof e);
	if (!r) {
		mal("%s: no lo leemos (%s)", nom, e);
	} else if (a != an || b != al) {
		mal("%s: medimos %dx%d e ImageMagick %dx%d", nom, a, b, an, al);
	} else {
		im = malloc((size_t)an * al * 4);
		for (i = 0; i < (u32)an * al; i++)
			im[i] = ((u32)q[4 * i + 3] << 24 | (u32)q[4 * i] << 16 | (u32)q[4 * i + 1] << 8 | q[4 * i + 2]) ^ (inv ? 0xFFFFFFu : 0);
		if (minimo > 0) {
			double p = psnr(im, r, (u32)an * al);

			if (p >= minimo)
				bien("%s (%dx%d): como ImageMagick, PSNR %.1f dB", nom, an, al, p);
			else
				mal("%s (%dx%d): frente a ImageMagick, PSNR %.1f dB (< %.0f)", nom, an, al, p, minimo);
		} else {
			u32 k = distintos(im, r, (u32)an * al);

			if (k) {
				for (i = 0; i < (u32)an * al && (im[i] == r[i] || !(im[i] >> 24 || r[i] >> 24)); i++)
					;
				mal("%s (%dx%d): %u puntos distintos de ImageMagick (el primero, %u,%u: %08X y él %08X)", nom, an, al, k,
				    i % an, i / an, r[i], im[i]);
			} else
				bien("%s (%dx%d): %s", nom, an, al, inv ? "como ImageMagick, pero en negativo: él no mira la paleta de los de 1 bit (1 = negro); nosotros sí, como GIMP" : "idéntico a ImageMagick");
		}
		free(im);
	}
	so_suelta(r);
	free(d);
	free(q);
	return 0;
}

/* ---- el fuzz ---- */
static u32 alloc_tam(const void *p)
{
	return (u32)((const size_t *)((const u8 *)p - 16))[0];	/* (so_anfitrion.c: el tamaño, delante) */
}

static int fuzz(int vueltas, u32 semilla, char **archs, int n_archs)
{
	int k, v;

	for (k = 0; k < n_archs; k++) {
		u32 n, leidos = 0, fallos = 0, antes = so_memoria_usada();
		u8 *orig = lee_todo(archs[k], &n);
		int pista = fi_por_extension(archs[k]), malo = 0;
		const char *nom = strrchr(archs[k], '/') ? strrchr(archs[k], '/') + 1 : archs[k];

		if (!orig || !n) {
			mal("fuzz: no está %s", archs[k]);
			continue;
		}
		azar_s = semilla + (u32)k * 7919u;
		for (v = 0; v < vueltas && !malo; v++) {
			u32 m = n, i, cuantos, t = azar() % 8;
			u8 *d;
			int a = -1, b = -1;
			char e[200];
			u32 *r;

			/* la mutación */
			if (t == 6)
				m = n + 1 + azar() % 300;	/* alargar */
			else if (t == 5)
				m = azar() % n;			/* cortar */
			d = malloc(m ? m : 1);
			memcpy(d, orig, m < n ? m : n);
			for (i = n; i < m; i++)
				d[i] = (u8)azar();
			if (t == 6 && n > 8 && azar() % 2)	/* (o repetir un trozo) */
				for (i = n; i < m; i++)
					d[i] = orig[8 + (i - n) % (n - 8)];
			cuantos = 1 + azar() % 8;
			if (m)
				switch (t) {
				case 0:
				case 1:
				case 5:
				case 6:
					for (i = 0; i < cuantos; i++)
						d[azar() % m] = (u8)azar();
					break;
				case 2:		/* en la cabecera */
					for (i = 0; i < cuantos; i++)
						d[azar() % (m < 140 ? m : 140)] ^= (u8)(1u << (azar() % 8));
					break;
				case 3: {	/* un campo de la cabecera a 0, 0xFFFF, 0x7FFF, 0x8000, 1... */
					static const u32 vals[] = { 0, 0xFFFF, 0x7FFF, 0x8000, 1, 0xFFFFFFFFu, 0x7FFFFFFFu, 4097, 256 };
					u32 o = azar() % (m < 140 ? m : 140), val = vals[azar() % 9], w = 1 + azar() % 4;

					for (i = 0; i < w && o + i < m; i++)
						d[o + i] = (u8)(val >> (8 * i));
					break;
				}
				case 4:		/* un trozo de en medio, borrado */
					if (m > 20) {
						u32 o = azar() % (m - 10), l = 1 + azar() % (m - o - 1);

						memmove(d + o, d + o + l, m - o - l);
						m -= l;
					}
					break;
				case 7:		/* muchos octetos al azar */
					for (i = 0; i < m / 50 + 1; i++)
						d[azar() % m] = (u8)azar();
					break;
				}
			r = fi_lee(d, m, pista, &a, &b, e, sizeof e);
			if (r) {
				u64 s = 0;

				leidos++;
				if (a <= 0 || b <= 0 || a > FI_MAX_LADO || b > FI_MAX_LADO || (u64)a * b > FI_MAX_PUNTOS ||
				    alloc_tam(r) < (u64)a * b * 4) {
					mal("fuzz %s, vuelta %d: dice que lee %dx%d (y pidió %u)", nom, v, a, b, alloc_tam(r));
					malo = 1;
				} else
					for (i = 0; i < (u32)a * b; i++)
						s += r[i];
				(void)s;
				so_suelta(r);
			} else {
				fallos++;
				if (!e[0]) {
					mal("fuzz %s, vuelta %d: no lee y no dice por qué", nom, v);
					malo = 1;
				}
			}
			free(d);
		}
		if (so_memoria_usada() != antes) {
			mal("fuzz %s: se pierde memoria (%u octetos)", nom, so_memoria_usada() - antes);
			malo = 1;
		}
		if (!malo)
			bien("fuzz de %s: %d vueltas (%u leídas, %u rechazadas con su porqué), sin fugas", nom, vueltas, leidos, fallos);
		free(orig);
	}
	return 0;
}

/* ---- medidas enormes ---- */
static u32 be32(u8 *p, u32 v)
{
	p[0] = (u8)(v >> 24);
	p[1] = (u8)(v >> 16);
	p[2] = (u8)(v >> 8);
	p[3] = (u8)v;
	return v;
}

static void l16(u8 *p, u32 v)
{
	p[0] = (u8)v;
	p[1] = (u8)(v >> 8);
}

static void l32(u8 *p, u32 v)
{
	l16(p, v);
	l16(p + 2, v >> 16);
}

static void prueba_enorme(const char *que, const u8 *d0, u32 n, int pista)
{
	u8 *d = malloc(n);
	int a, b;
	char e[200];
	u32 *r, antes = so_memoria_usada();

	memcpy(d, d0, n);
	e[0] = 0;
	r = fi_lee(d, n, pista, &a, &b, e, sizeof e);
	if (!r && strstr(e, "mide") && so_memoria_usada() == antes)
		bien("%s: se rechaza («%s»)", que, e);
	else
		mal("%s: %s", que, r ? "¡se lee!" : e);
	so_suelta(r);
	free(d);
}

static void enormes(void)
{
	u8 d[2048];
	static const u32 lados[][2] = { { 100000, 100000 }, { 4097, 10 }, { 10, 4097 }, { 65535, 65535 }, { 0x7FFFFFFF, 2 } };
	u32 k;
	char q[100];

	for (k = 0; k < 5; k++) {
		u32 w = lados[k][0], h = lados[k][1], w16 = w > 65535 ? 65535 : w, h16 = h > 65535 ? 65535 : h;

		/* PNG */
		memset(d, 0, sizeof d);
		memcpy(d, "\x89PNG\r\n\x1a\n\0\0\0\x0dIHDR", 16);
		be32(d + 16, w);
		be32(d + 20, h);
		d[24] = 8;
		d[25] = 6;
		memcpy(d + 33, "\0\0\0\x10IDAT", 8);
		fmt(q, sizeof q, "PNG de %u x %u", w, h);
		prueba_enorme(q, d, 200, FI_PNG);
		/* BMP */
		memset(d, 0, sizeof d);
		d[0] = 'B';
		d[1] = 'M';
		l32(d + 2, 1000);
		l32(d + 10, 54);
		l32(d + 14, 40);
		l32(d + 18, w);
		l32(d + 22, k == 3 ? (u32)-(i32)h : h);
		l16(d + 26, 1);
		l16(d + 28, 24);
		fmt(q, sizeof q, "BMP de %u x %u", w, h);
		prueba_enorme(q, d, 1000, FI_BMP);
		/* GIF, PCX, TGA y JPG: de 16 bits */
		if (w16 > 4096 || h16 > 4096) {
			memset(d, 0, sizeof d);
			memcpy(d, "GIF89a", 6);
			l16(d + 6, w16);
			l16(d + 8, h16);
			d[10] = 0x80;
			d[19] = 0x2C;
			l16(d + 24, w16);
			l16(d + 26, h16);
			d[29] = 2;
			fmt(q, sizeof q, "GIF de %u x %u", w16, h16);
			prueba_enorme(q, d, 100, FI_GIF);
			memset(d, 0, sizeof d);
			d[0] = 0x0A;
			d[1] = 5;
			d[2] = 1;
			d[3] = 8;
			l16(d + 8, w16 - 1);
			l16(d + 10, h16 - 1);
			d[65] = 3;
			l16(d + 66, w16 & ~1u);
			fmt(q, sizeof q, "PCX de %u x %u", w16, h16);
			prueba_enorme(q, d, 1000, FI_PCX);
			memset(d, 0, sizeof d);
			d[2] = 2;
			l16(d + 12, w16);
			l16(d + 14, h16);
			d[16] = 32;
			fmt(q, sizeof q, "TGA de %u x %u", w16, h16);
			prueba_enorme(q, d, 1000, FI_TGA);
			memset(d, 0, sizeof d);
			memcpy(d, "\xFF\xD8\xFF\xC0\x00\x11\x08", 7);
			d[7] = (u8)(h16 >> 8);
			d[8] = (u8)h16;
			d[9] = (u8)(w16 >> 8);
			d[10] = (u8)w16;
			d[11] = 3;
			memcpy(d + 12, "\x01\x22\x00\x02\x11\x01\x03\x11\x01\xFF\xD9", 11);
			fmt(q, sizeof q, "JPG de %u x %u", w16, h16);
			prueba_enorme(q, d, 100, FI_JPG);
		}
		/* ICO con un BMP dentro (de doble alto) y con un PNG */
		memset(d, 0, sizeof d);
		l16(d + 2, 1);
		l16(d + 4, 1);
		l16(d + 6 + 12, 1000);
		l32(d + 6 + 8, 1000);
		l32(d + 6 + 12, 22);
		l32(d + 22, 40);
		l32(d + 26, w);
		l32(d + 30, h > 0x7FFFFFFF / 2 ? 0x7FFFFFFE : 2 * h);
		l16(d + 34, 1);
		l16(d + 36, 32);
		fmt(q, sizeof q, "ICO con un BMP de %u x %u", w, h);
		prueba_enorme(q, d, 1100, FI_ICO);
		memset(d + 22, 0, 200);
		memcpy(d + 22, "\x89PNG\r\n\x1a\n\0\0\0\x0dIHDR", 16);
		be32(d + 22 + 16, w);
		be32(d + 22 + 20, h);
		d[22 + 24] = 8;
		d[22 + 25] = 6;
		fmt(q, sizeof q, "ICO con un PNG de %u x %u", w, h);
		prueba_enorme(q, d, 1100, FI_ICO);
	}
}

int ofi_principal(const char *args)
{
	static char buf[8192];
	char *v[300];
	int n = 0;
	char *s;

	copia(buf, args, sizeof buf);
	for (s = strtok(buf, " "); s && n < 300; s = strtok(NULL, " "))
		v[n++] = s;
	if (n >= 2 && iguales(v[0], "ida"))
		return ida(v[1]);
	if (n >= 5 && iguales(v[0], "compara"))
		return compara(v[1], v[2], atoi(v[3]), atoi(v[4]), n >= 6 ? atof(v[5]) : 0, n >= 6 && iguales(v[5], "invertido"));
	if (n >= 4 && iguales(v[0], "fuzz"))
		return fuzz(atoi(v[1]), (u32)strtoul(v[2], NULL, 10), v + 3, n - 3);
	if (n >= 1 && iguales(v[0], "enormes")) {
		enormes();
		return 0;
	}
	fprintf(stderr, "uso: prueba_formatos ida CARPETA | compara ARCH RGBA AN AL [PSNR] | fuzz N SEMILLA ARCH... | enormes\n");
	return 2;
}
