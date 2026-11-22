/* archivo.c - Pintura 2: abrir y guardar (formatos.h), exportar a PDF
 * (oficina/comun/pdfsal.h: la imagen en PNG, sin pérdida, en una página
 * de su tamaño a 96 puntos por pulgada) y «Usar como fondo de escritorio»
 * (la imagen llenando la pantalla, lo que sobra cortado por igual: la
 * llamada 172, como el Mirador).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pintura.h"
#include "formatos.h"
#include "operaciones.h"
#include "pdfsal.h"

#define MAX_ARCHIVO (64u << 20)

void pin_ruta_con_ext(char *ruta, u32 cabe, int formato)
{
	char e[8];
	const char *x = fi_extension(formato);

	extension(ruta, e);
	if (!e[0] || fi_por_extension(ruta) != formato) {
		anade(ruta, ".", cabe);
		anade(ruta, x, cabe);
	}
	if (!so_nombres_largos()) {
		/* (8+3: el nombre, a 8 letras, sin espacios ni puntos de más) */
		char *n = (char *)sin_carpeta(ruta), b[16];
		u32 i, k = 0;
		char *pt = n + largo(n), *q;

		for (q = n; *q; q++)
			if (*q == '.')
				pt = q;

		for (i = 0; n + i < pt && k < 8; i++)
			if (n[i] != ' ' && n[i] != '.')
				b[k++] = n[i];
		if (!k)
			b[k++] = 'I';
		b[k] = 0;
		anade(b, ".", sizeof b);
		anade(b, x, sizeof b);
		for (i = 0; b[i]; i++)
			b[i] = (char)mayuscula((u8)b[i]);
		copia(n, b, cabe - (u32)(n - ruta));
	}
}

int pin_lee_imagen(const char *ruta, lamina *d, char *err, u32 cabe)
{
	const char *e = NULL;
	u32 n;
	int an, al;
	u8 *datos = so_lee_archivo(ruta, &n, MAX_ARCHIVO, &e);
	u32 *px;

	memset(d, 0, sizeof *d);
	if (!datos) {
		fmt(err, cabe, "No se puede leer %s: %s.", sin_carpeta(ruta), e ? e : "no se puede");
		return 0;
	}
	px = fi_lee(datos, n, fi_por_extension(ruta), &an, &al, err, cabe);
	so_suelta(datos);
	if (!px)
		return 0;
	d->p = px;
	d->an = an;
	d->al = al;
	return 1;
}

int pin_abre(const char *ruta)
{
	char err[300];
	lamina n;
	u32 k;
	u8 *d;
	const char *e;

	if (!pin_lee_imagen(ruta, &n, err, sizeof err)) {
		dlg_aviso("No se puede abrir", err);
		dice("pintura: no se abre %s: %s", ruta, err);
		return -1;
	}
	herr_cancela();
	sel_nada();
	lam_suelta(&P.img);
	P.img = n;
	his_lamina(&P.img, 1);
	copia(P.ruta, ruta, sizeof P.ruta);
	/* el formato: el de dentro (o el de la extensión: TGA) */
	d = so_lee_archivo(ruta, &k, 64, &e);
	P.formato = d ? fi_por_contenido(d, k) : FI_NADA;
	so_suelta(d);
	if (!P.formato)
		P.formato = fi_por_extension(ruta);
	P.cambiado = 0;
	P.dx = P.dy = 0;
	P.zoom = 1000;
	{
		/* (la carpeta, para el siguiente Abrir) */
		const char *s = sin_carpeta(ruta);

		if (s > ruta) {
			u32 l = (u32)(s - ruta);

			copia(P.carpeta, ruta, l < sizeof P.carpeta ? l : sizeof P.carpeta);
			if (l > 1 && (P.carpeta[l - 1] == '\\' || P.carpeta[l - 1] == '/'))
				P.carpeta[l - 1] = 0;
		}
	}
	pin_cambio_imagen();
	if (VI.san > VI.an || VI.sal > VI.al) {
		P.zoom = vista_zoom_ajustado();
		pin_cambio_imagen();
	}
	pin_aviso("Abierta %s (%s, %d x %d%s)", sin_carpeta(ruta), fi_nombre(P.formato), P.img.an, P.img.al,
		  fi_hay_transparencia(P.img.p, P.img.an, P.img.al) ? ", con transparencia" : "");
	dice("pintura: abierto %s (%s, %d x %d)", sin_carpeta(ruta), fi_nombre(P.formato), P.img.an, P.img.al);
	return 0;
}

static int escribe_archivo(const char *ruta, const bufer *b, char *err, u32 cabe)
{
	const char *e = NULL;
	void *f = so_crea(ruta, &e);

	if (!f) {
		fmt(err, cabe, "No se puede crear %s: %s.", ruta, e ? e : "no se puede");
		return -1;
	}
	if (so_escribe(f, b->d, b->n) || so_cierra(f)) {
		fmt(err, cabe, "No se ha podido escribir %s (¿el disco está lleno?).", ruta);
		return -1;
	}
	return 0;
}

int pin_guarda_lamina(const lamina *l, const char *ruta, int formato, int calidad, char *err, u32 cabe)
{
	bufer b = { 0 };
	int r;

	if (fi_escribe(&b, l->p, l->an, l->al, formato, calidad, err, cabe)) {
		buf_suelta(&b);
		return -1;
	}
	if (b.mal) {
		fmt(err, cabe, "No hay memoria para guardarla.");
		buf_suelta(&b);
		return -1;
	}
	r = escribe_archivo(ruta, &b, err, cabe);
	if (!r)
		dice("pintura: guardado %s (%s, %d x %d, %u octetos)", sin_carpeta(ruta), fi_nombre(formato), l->an, l->al, b.n);
	buf_suelta(&b);
	return r;
}

int pin_guarda(const char *ruta, int formato)
{
	char err[300], r[600];

	herr_fija();
	copia(r, ruta, sizeof r);
	pin_ruta_con_ext(r, sizeof r, formato);
	if (pin_guarda_lamina(&P.img, r, formato, P.calidad, err, sizeof err)) {
		dlg_aviso("No se puede guardar", err);
		return -1;
	}
	copia(P.ruta, r, sizeof P.ruta);
	P.formato = formato;
	P.cambiado = 0;
	pin_titulo();
	if (formato == FI_JPG && fi_hay_transparencia(P.img.p, P.img.an, P.img.al))
		pin_aviso("Guardada %s (JPG no tiene transparencia: lo transparente va en blanco)", sin_carpeta(r));
	else if ((formato == FI_PCX) && fi_hay_transparencia(P.img.p, P.img.an, P.img.al))
		pin_aviso("Guardada %s (PCX no tiene transparencia: lo transparente va en blanco)", sin_carpeta(r));
	else
		pin_aviso("Guardada %s (%s, %d x %d)", sin_carpeta(r), fi_nombre(formato), P.img.an, P.img.al);
	return 0;
}

int pin_pdf(const char *ruta)
{
	char err[300], r[600];
	bufer png = { 0 }, b = { 0 };
	pdfsal *s;
	int img, ok;
	double an = P.img.an * 0.75, al = P.img.al * 0.75;

	herr_fija();
	copia(r, ruta, sizeof r);
	{
		char e[8];

		extension(r, e);
		if (!iguales(e, "PDF"))
			anade(r, ".pdf", sizeof r);
	}
	if (fi_escribe(&png, P.img.p, P.img.an, P.img.al, FI_PNG, 0, err, sizeof err) || png.mal) {
		buf_suelta(&png);
		dlg_aviso("No se puede exportar", "No hay memoria para pasar la imagen a PDF.");
		return -1;
	}
	s = pdf_nuevo();
	if (!s) {
		buf_suelta(&png);
		return -1;
	}
	pdf_datos(s, P.ruta[0] ? sin_carpeta(P.ruta) : "Imagen de Pintura", NULL, "Hecho con Pintura 2 de Hispa-Pentium");
	pdf_productor(s, "Pintura " PINTURA_VERSION " (Hispa-Pentium)");
	/* (las de más de 200 pulgadas, que no caben en un PDF: más pequeñas) */
	while (an > 14400 || al > 14400) {
		an /= 2;
		al /= 2;
	}
	pdf_pagina(s, an, al);
	img = pdf_imagen(s, png.d, png.n);
	if (img >= 0)
		pdf_pon_imagen(s, img, 0, 0, an, al);
	ok = img >= 0 && pdf_acaba(s, &b) == 0 && !b.mal;
	pdf_suelta(s);
	buf_suelta(&png);
	if (!ok) {
		buf_suelta(&b);
		dlg_aviso("No se puede exportar", "No se ha podido hacer el PDF (¿hay memoria?).");
		return -1;
	}
	if (escribe_archivo(r, &b, err, sizeof err)) {
		buf_suelta(&b);
		dlg_aviso("No se puede exportar", err);
		return -1;
	}
	dice("pintura: pdf %s (%u octetos)", sin_carpeta(r), b.n);
	pin_aviso("Exportada a %s", sin_carpeta(r));
	buf_suelta(&b);
	return 0;
}

int pin_fondo(void)
{
	int W, H, cx, cy, cw, ch, r;
	lamina trozo, fin;
	u32 i;

	so_pantalla_entera(&W, &H);
	if (W < 1 || H < 1 || W > 4096 || H > 4096)
		return -1;
	/* lo que se toma de la imagen: su centro, con la forma de la pantalla */
	if ((i64)P.img.an * H > (i64)P.img.al * W) {
		ch = P.img.al;
		cw = (int)((i64)P.img.al * W / H);
	} else {
		cw = P.img.an;
		ch = (int)((i64)P.img.an * H / W);
	}
	if (cw < 1)
		cw = 1;
	if (ch < 1)
		ch = 1;
	cx = (P.img.an - cw) / 2;
	cy = (P.img.al - ch) / 2;
	if (!lam_trozo(&trozo, &P.img, cx, cy, cw, ch)) {
		dlg_aviso("Pintura", "No hay memoria para hacer el fondo.");
		return -1;
	}
	r = op_escala(&trozo, &fin, W, H);
	lam_suelta(&trozo);
	if (!r) {
		dlg_aviso("Pintura", "No hay memoria para hacer el fondo.");
		return -1;
	}
	/* (lo transparente, sobre negro, como el Mirador) */
	for (i = 0; i < (u32)W * (u32)H; i++)
		fin.p[i] = lam_sobre_opaco(0x000000, fin.p[i]);
	r = so_fondo_escritorio(fin.p, W, H);
	lam_suelta(&fin);
	dice("pintura: de fondo de escritorio, %d x %d: %s", W, H, r ? "no" : "bien");
	if (r)
		dlg_aviso("Pintura", "No se ha podido poner de fondo (¿hay escritorio? ¿se puede escribir en el disco del sistema?).");
	else
		pin_aviso("Puesta de fondo de escritorio (%d x %d)", W, H);
	return r;
}
