/* principal.c - Pintura 2: el principio. Sin nada, o con una imagen, la
 * ventana. Con órdenes (para las pruebas y para quien las quiera en la
 * Consola):
 *   PINTURA --convierte ENTRADA SALIDA [CALIDAD]   (el formato de SALIDA, por su extensión)
 *   PINTURA --info ENTRADA                         formato, medidas, transparencia, colores
 *   PINTURA --compara A B                          punto a punto: iguales, o cuánto difieren (PSNR)
 *   PINTURA --mide [AN AL [PASOS]]                 lo que tarda el pincel en un lienzo de an x al
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pintura.h"
#include "formatos.h"
#include "letras.h"

const char ofi_programa[] = "PINTURA.BIN";

const char *pin_arg(const char *s, char *d, u32 cabe)
{
	u32 k = 0;

	while (*s == ' ')
		s++;
	if (*s == '"') {
		s++;
		while (*s && *s != '"') {
			if (k + 1 < cabe)
				d[k++] = *s;
			s++;
		}
		if (*s == '"')
			s++;
	} else
		while (*s && *s != ' ') {
			if (k + 1 < cabe)
				d[k++] = *s;
			s++;
		}
	d[k] = 0;
	return s;
}

static int convierte(const char *de, const char *a, int calidad)
{
	char err[300];
	lamina l;
	int f = fi_por_extension(a);

	if (!f || !fi_se_escribe(f)) {
		dice("pintura: no sé guardar en «%s» (PNG, JPG, BMP, GIF o PCX)", sin_carpeta(a));
		return 1;
	}
	if (!pin_lee_imagen(de, &l, err, sizeof err)) {
		dice("pintura: %s", err);
		return 1;
	}
	if (pin_guarda_lamina(&l, a, f, calidad > 0 ? calidad : 90, err, sizeof err)) {
		dice("pintura: %s", err);
		lam_suelta(&l);
		return 1;
	}
	lam_suelta(&l);
	return 0;
}

static int info(const char *de)
{
	char err[300];
	lamina l;
	u32 n, i, k, distintos = 0, *vistos;
	u8 *d;
	const char *e;
	int f;

	if (!pin_lee_imagen(de, &l, err, sizeof err)) {
		dice("pintura: %s", err);
		return 1;
	}
	d = so_lee_archivo(de, &n, 64, &e);
	f = d ? fi_por_contenido(d, n) : 0;
	so_suelta(d);
	if (!f)
		f = fi_por_extension(de);
	/* los colores distintos (hasta 4096 se cuentan; más: «más de») */
	n = (u32)l.an * (u32)l.al;
	vistos = so_pide(4097 * 4);
	for (i = 0; vistos && i < n && distintos <= 4096; i++) {
		for (k = 0; k < distintos; k++)
			if (vistos[k] == l.p[i])
				break;
		if (k == distintos)
			vistos[distintos++] = l.p[i];
	}
	so_suelta(vistos);
	if (distintos > 4096)
		dice("pintura: %s: %s, %d x %d, %s, más de 4096 colores", sin_carpeta(de), fi_nombre(f), l.an, l.al,
		     fi_hay_transparencia(l.p, l.an, l.al) ? "con transparencia" : "opaca");
	else
		dice("pintura: %s: %s, %d x %d, %s, %u colores", sin_carpeta(de), fi_nombre(f), l.an, l.al,
		     fi_hay_transparencia(l.p, l.an, l.al) ? "con transparencia" : "opaca", distintos);
	lam_suelta(&l);
	return 0;
}

/* el logaritmo decimal (sin la libm: Hispa-Pentium no la tiene) */
static double log10_(double x)
{
	int k = 0, i;
	double y, y2, t, s = 0;

	if (x <= 0)
		return -99;
	while (x > 2) {
		x /= 2;
		k++;
	}
	while (x < 1) {
		x *= 2;
		k--;
	}
	y = (x - 1) / (x + 1);
	y2 = y * y;
	t = y;
	for (i = 1; i < 40; i += 2) {
		s += t / i;
		t *= y2;
	}
	return (2 * s + k * 0.6931471805599453) / 2.302585092994046;
}

static int compara(const char *a, const char *b)
{
	char err[300];
	lamina x, y;
	u32 i, n, distintos = 0, maxd = 0;
	double se = 0;

	if (!pin_lee_imagen(a, &x, err, sizeof err) || !pin_lee_imagen(b, &y, err, sizeof err)) {
		dice("pintura: %s", err);
		return 2;
	}
	if (x.an != y.an || x.al != y.al) {
		dice("pintura: distintas: %d x %d y %d x %d", x.an, x.al, y.an, y.al);
		lam_suelta(&x);
		lam_suelta(&y);
		return 1;
	}
	n = (u32)x.an * (u32)x.al;
	for (i = 0; i < n; i++) {
		int k;
		/* (lo transparente del todo es igual, sea del color que sea) */
		u32 p = ALFA(x.p[i]) ? x.p[i] : 0, q = ALFA(y.p[i]) ? y.p[i] : 0;

		if (p == q)
			continue;
		distintos++;
		for (k = 0; k < 32; k += 8) {
			int d = (int)((p >> k) & 255) - (int)((q >> k) & 255);

			if (d < 0)
				d = -d;
			if ((u32)d > maxd)
				maxd = (u32)d;
			se += (double)d * d;
		}
	}
	if (!distintos)
		dice("pintura: iguales (%d x %d)", x.an, x.al);
	else {
		double mse = se / (n * 4.0), psnr = mse > 0 ? 10 * log10_(255.0 * 255.0 / mse) : 99;

		dice("pintura: distintas en %u puntos de %u; la mayor diferencia, %u; PSNR %d dB", distintos, n, maxd, (int)psnr);
	}
	lam_suelta(&x);
	lam_suelta(&y);
	return distintos ? 1 : 0;
}

int ofi_principal(const char *args)
{
	char a1[600], a2[600], a3[600], a4[60];
	const char *s = args;

	letras_inicia();
	paleta_de_fabrica();
	s = pin_arg(s, a1, sizeof a1);
	s = pin_arg(s, a2, sizeof a2);
	s = pin_arg(s, a3, sizeof a3);
	s = pin_arg(s, a4, sizeof a4);
	if (iguales_sin_caja(a1, "--convierte") || iguales_sin_caja(a1, "/CONVIERTE"))
		return convierte(a2, a3, a4[0] ? (int)numero(a4, NULL) : 90);
	if (iguales_sin_caja(a1, "--info") || iguales_sin_caja(a1, "/INFO"))
		return info(a2);
	if (iguales_sin_caja(a1, "--compara") || iguales_sin_caja(a1, "/COMPARA"))
		return compara(a2, a3);
	if (iguales_sin_caja(a1, "--mide") || iguales_sin_caja(a1, "/MIDE"))
		return pintura_mide(a2[0] ? (int)numero(a2, NULL) : 1600, a3[0] ? (int)numero(a3, NULL) : 1200, a4[0] ? (int)numero(a4, NULL) : 200);
	return pintura_ventana(a1[0] ? a1 : NULL);
}
