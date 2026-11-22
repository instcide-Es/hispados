/* orden.c - Hispa-PDF: la orden PDF (la Consola) y lo que le pide la
 * ventana (HISPAPDF.BIN). Todo en español:
 *
 *   PDF INFO a.pdf                          lo que lleva (páginas, medidas, cifrado)
 *   PDF UNE a.pdf b.pdf ... /SALIDA=c.pdf   unir
 *   PDF SEPARA a.pdf [1-3,4-9] /SALIDA=p.pdf   una por página (o por trozo;
 *                                            /CADA=n: de n en n) -> P1.PDF, P2.PDF...
 *   PDF EXTRAE a.pdf 3,1,5-7 /SALIDA=b.pdf  esas páginas, en ese orden (ORDENA, igual;
 *                                            /INVIERTE: al revés)
 *   PDF QUITA a.pdf 2,4-5 /SALIDA=b.pdf     sin esas páginas
 *   PDF GIRA a.pdf 90 [páginas] /SALIDA=b.pdf   girar (90, 180, 270, -90)
 *   PDF COMPRIME a.pdf /SALIDA=b.pdf [/CALIDAD=ALTA|MEDIA|BAJA]
 *   PDF TEXTO a.pdf [/SALIDA=a.txt] [/PAGINAS=1-3] [/UTF8]
 *   PDF IMAGENES a.pdf /SALIDA=pag.png [/PUNTOS=150] [/PAGINAS=..]   (o .jpg)
 *   PDF CREA a.jpg b.png ... /SALIDA=c.pdf [/PAPEL=A4|A3|A5|CARTA|IMAGEN] [/MARGEN=10]
 *   PDF CIFRA a.pdf /CLAVE=x [/DUENO=y] [/CIFRADO=AES128|AES256|RC4] /SALIDA=b.pdf
 *   PDF DESCIFRA a.pdf /CLAVE=x /SALIDA=b.pdf
 *   (y para la ventana: MINIATURAS, PAGINA y HAZ)
 *
 * /CLAVE= vale también para abrir los que llevan contraseña.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "hpdf.h"

#define MAX_TROZOS 64
#define MAX_RUTA 260

typedef struct {
	char *t[MAX_TROZOS];		/* lo que no son opciones */
	int n;
	char salida[MAX_RUTA], clave[130], dueno[130], paginas[200], calidad[16], cifrado[16], papel[16];
	int puntos, cada, utf8, invierte, margen, lado, desde, hasta, ancho, alto, marcadores;
	int hay_margen;
} pargs;

static char copia_args[1200];

static void opcion(pargs *a, char *t)
{
	struct { const char *n; char *d; int cabe; } textos[] = {
		{"SALIDA", a->salida, sizeof a->salida}, {"CLAVE", a->clave, sizeof a->clave},
		{"DUENO", a->dueno, sizeof a->dueno}, {"PROPIETARIO", a->dueno, sizeof a->dueno},
		{"PAGINAS", a->paginas, sizeof a->paginas}, {"CALIDAD", a->calidad, sizeof a->calidad},
		{"CIFRADO", a->cifrado, sizeof a->cifrado}, {"PAPEL", a->papel, sizeof a->papel},
		{"PAGINA", a->papel, sizeof a->papel}};
	struct { const char *n; int *d; } numeros[] = {
		{"PUNTOS", &a->puntos}, {"PPP", &a->puntos}, {"CADA", &a->cada}, {"MARGEN", &a->margen},
		{"LADO", &a->lado}, {"DESDE", &a->desde}, {"HASTA", &a->hasta}, {"ANCHO", &a->ancho}, {"ALTO", &a->alto}};
	char *igual = strchr(t, '=');
	u32 i;

	if (igual) {
		*igual = 0;
		for (i = 0; i < sizeof textos / sizeof textos[0]; i++)
			if (igual_sin_caja(t + 1, textos[i].n)) {
				u32 n = (u32)strlen(igual + 1);

				if (n >= (u32)textos[i].cabe)
					n = (u32)textos[i].cabe - 1;
				memcpy(textos[i].d, igual + 1, n);
				textos[i].d[n] = 0;
				return;
			}
		for (i = 0; i < sizeof numeros / sizeof numeros[0]; i++)
			if (igual_sin_caja(t + 1, numeros[i].n)) {
				const char *p = igual + 1;
				int v = 0;

				while (*p >= '0' && *p <= '9' && v < 100000000)
					v = v * 10 + (*p++ - '0');
				*numeros[i].d = v;
				if (numeros[i].d == &a->margen)
					a->hay_margen = 1;
				return;
			}
		*igual = '=';
	} else {
		if (igual_sin_caja(t + 1, "UTF8")) { a->utf8 = 1; return; }
		if (igual_sin_caja(t + 1, "INVIERTE")) { a->invierte = 1; return; }
		if (igual_sin_caja(t + 1, "MARCADORES")) { a->marcadores = 1; return; }
	}
	/* (no es una opción: una ruta de Linux que empieza por /) */
	if (a->n < MAX_TROZOS)
		a->t[a->n++] = t;
}

static int es_opcion(const char *t)
{
	static const char *conocidas[] = {"SALIDA=", "CLAVE=", "DUENO=", "PROPIETARIO=", "PAGINAS=", "CALIDAD=", "CIFRADO=",
					  "PAPEL=", "PAGINA=", "PUNTOS=", "PPP=", "CADA=", "MARGEN=", "LADO=", "DESDE=",
					  "HASTA=", "ANCHO=", "ALTO=", "UTF8", "INVIERTE", "MARCADORES"};
	u32 i;

	if (t[0] != '/')
		return 0;
	for (i = 0; i < sizeof conocidas / sizeof conocidas[0]; i++)
		if (empieza_sin_caja(t + 1, conocidas[i])) {
			u32 n = (u32)strlen(conocidas[i]);

			if (conocidas[i][n - 1] == '=' || t[1 + n] == 0)
				return 1;
		}
	return 0;
}

/* los trozos de la línea (con «comillas» para los que llevan espacios) */
static void parte(const char *linea, pargs *a)
{
	char *p = copia_args;
	u32 n = (u32)strlen(linea);

	memset(a, 0, sizeof *a);
	a->marcadores = 1;
	if (n >= sizeof copia_args)
		n = sizeof copia_args - 1;
	memcpy(copia_args, linea, n);
	copia_args[n] = 0;
	while (*p) {
		char *ini;

		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			p++;
		if (!*p)
			break;
		if (*p == '"') {
			ini = ++p;
			while (*p && *p != '"')
				p++;
		} else {
			ini = p;
			while (*p && *p != ' ' && *p != '\t')
				p++;
		}
		if (*p)
			*p++ = 0;
		if (es_opcion(ini))
			opcion(a, ini);
		else if (a->n < MAX_TROZOS)
			a->t[a->n++] = ini;
	}
}

static const char *ayuda_texto =
	"Hispa-PDF 1.0: trabajar con PDF.\n"
	"  PDF INFO a.pdf                         lo que lleva\n"
	"  PDF UNE a.pdf b.pdf ... /SALIDA=c.pdf  unir varios\n"
	"  PDF SEPARA a.pdf [1-3,4-9] /SALIDA=p.pdf   uno por página (o por trozo; /CADA=n)\n"
	"  PDF EXTRAE a.pdf 3,1,5-7 /SALIDA=b.pdf  esas páginas, en ese orden (ORDENA; /INVIERTE)\n"
	"  PDF QUITA a.pdf 2,4-5 /SALIDA=b.pdf    sin esas páginas\n"
	"  PDF GIRA a.pdf 90 [páginas] /SALIDA=b.pdf  girar (90, 180, 270)\n"
	"  PDF COMPRIME a.pdf /SALIDA=b.pdf [/CALIDAD=ALTA|MEDIA|BAJA]\n"
	"  PDF TEXTO a.pdf [/SALIDA=a.txt] [/PAGINAS=1-3] [/UTF8]\n"
	"  PDF IMAGENES a.pdf /SALIDA=pag.png [/PUNTOS=150] [/PAGINAS=..]  (o .jpg)\n"
	"  PDF CREA a.jpg b.png ... /SALIDA=c.pdf [/PAPEL=A4|A3|A5|CARTA|IMAGEN] [/MARGEN=10]\n"
	"  PDF CIFRA a.pdf /CLAVE=x [/DUENO=y] [/CIFRADO=AES128|AES256|RC4] /SALIDA=b.pdf\n"
	"  PDF DESCIFRA a.pdf /CLAVE=x /SALIDA=b.pdf\n"
	"Las páginas: 1-3,5,8- (z o fin: la última). /CLAVE= abre los que llevan contraseña.\n";

/* ---- abrir ---- */
static int falta_clave;

static pdoc *abre(pargs *a, const char *ruta)
{
	const char *error = NULL;
	int nc = 0;
	pdoc *d = pdf_abre_archivo(ruta, a->clave[0] ? a->clave : NULL, &nc, &error);

	if (!d) {
		if (nc == 1) {
			falta_clave = 1;
			if (a->clave[0])
				dice("%s: la contraseña no es la suya\n", ruta);
			else
				dice("%s: lleva contraseña: póngala con /CLAVE=...\n", ruta);
		} else
			dice("%s: %s\n", ruta, error ? error : "no se puede abrir");
		return NULL;
	}
	if (d->n_paginas == 0) {
		dice("%s: no tiene páginas (o no se encuentran)\n", ruta);
		pdf_cierra(d);
		return NULL;
	}
	return d;
}

/* el nombre del archivo k de «total» (P.PDF -> P1.PDF..., cortando el nombre
 * para que quepa en 8+3 si el sistema es de 8+3) */
static void numerado(const char *salida, u32 k, u32 total, const char *ext_si_no, char *d, int cabe)
{
	char base[MAX_RUTA], ext[16], cif[12];
	const char *barra = salida, *punto = NULL, *p;
	int nb, digitos = 1, nd;
	u32 t = total;

	for (p = salida; *p; p++)
		if (*p == '\\' || *p == '/' || *p == ':')
			barra = p + 1;
	for (p = barra; *p; p++)
		if (*p == '.')
			punto = p;
	if (!punto)
		punto = p;
	nb = (int)(punto - salida);
	if (nb >= (int)sizeof base)
		nb = sizeof base - 1;
	memcpy(base, salida, (size_t)nb);
	base[nb] = 0;
	if (*punto)
		fmt(ext, sizeof ext, "%s", punto);
	else
		fmt(ext, sizeof ext, "%s", ext_si_no);
	while (t >= 10) {
		digitos++;
		t /= 10;
	}
	{
		/* (con ceros delante, para que salgan en orden) */
		char f[8];

		fmt(f, sizeof f, "%%0%du", digitos);
		nd = fmt(cif, sizeof cif, f, k);
	}
	if (so_pagina() == 437) {
		int nombre = (int)(punto - barra);

		if (nombre + nd > 8) {
			int quita = nombre + nd - 8;

			if (quita > nombre)
				quita = nombre;
			base[nb - quita] = 0;
		}
	}
	fmt(d, cabe, "%s%s%s", base, cif, ext);
}

static int escribe(pnuevo *s, const char *ruta, popciones *op)
{
	const char *error = NULL;

	if (escribe_pdf(s, ruta, op, &error)) {
		dice("%s: %s\n", ruta, error ? error : "no se ha podido escribir");
		return 1;
	}
	return 0;
}

static int pide_salida(pargs *a)
{
	if (!a->salida[0]) {
		dice("Falta dónde dejarlo: /SALIDA=archivo\n");
		return 1;
	}
	return 0;
}

static void opciones_salida(pargs *a, popciones *op)
{
	memset(op, 0, sizeof *op);
	/* lo que se escribe, con lo que traiga (sin volver a comprimir) */
	(void)a;
}

/* ---- las órdenes ---- */
static const char *metodo_texto(pdoc *d)
{
	if (!d->cifrado)
		return "no";
	if (d->c.r >= 5)
		return "sí, AES de 256 bits";
	if (d->c.metodo_flujo == 2)
		return "sí, AES de 128 bits";
	return d->c.largo == 5 ? "sí, RC4 de 40 bits" : "sí, RC4 de 128 bits";
}

static void texto_info(pdoc *d, const char *clave, char *t, int cabe)
{
	pobj *info = dic_r(d, d->trailer, "Info"), *v = info ? dic_r(d, info, clave) : NULL;
	u32 i, k = 0;

	t[0] = 0;
	if (!v || v->t != O_CADENA)
		return;
	/* UTF-16 (con su marca) o PDFDocEncoding (casi Latin-1) */
	if (v->n >= 2 && v->u.s[0] == 0xFE && v->u.s[1] == 0xFF) {
		for (i = 2; i + 1 < v->n && k + 4 < (u32)cabe; i += 2) {
			u32 c = (u32)v->u.s[i] << 8 | v->u.s[i + 1];
			u8 *p = (u8 *)t + k;

			if (c >= 0xD800 && c < 0xE000)
				c = '?';
			a_utf8(c, &p);
			k = (u32)(p - (u8 *)t);
		}
	} else {
		for (i = 0; i < v->n && k + 3 < (u32)cabe; i++) {
			u8 *p = (u8 *)t + k;

			a_utf8(v->u.s[i] < 32 ? ' ' : v->u.s[i], &p);
			k = (u32)(p - (u8 *)t);
		}
	}
	t[k] = 0;
}

static int o_info(pargs *a)
{
	pdoc *d;
	u32 i;
	char t[300];

	if (a->n < 2) {
		dice("Uso: PDF INFO a.pdf\n");
		return 1;
	}
	d = abre(a, a->t[1]);
	if (!d)
		return falta_clave ? 2 : 1;
	dice("%s: PDF %u.%u, %u páginas%s\n", a->t[1], (u32)d->version / 10, (u32)d->version % 10, d->n_paginas,
	     d->reparado ? " (estaba estropeado: se ha rehecho su tabla)" : "");
	dice("  cifrado: %s\n", metodo_texto(d));
	texto_info(d, "Title", t, sizeof t);
	if (t[0])
		dice("  título: %s\n", t);
	texto_info(d, "Author", t, sizeof t);
	if (t[0])
		dice("  autor: %s\n", t);
	texto_info(d, "Producer", t, sizeof t);
	if (t[0])
		dice("  hecho con: %s\n", t);
	for (i = 0; i < d->n_paginas && i < 2000; i++) {
		double c[4];
		int g = pagina_giro(d, i);

		pagina_caja(d, i, c);
		if (i < 20 || i == d->n_paginas - 1)
			dice("  página %u: %u x %u mm%s%s\n", i + 1, (u32)((c[2] - c[0]) * 25.4 / 72 + 0.5),
			     (u32)((c[3] - c[1]) * 25.4 / 72 + 0.5), g ? ", girada " : "", g == 90 ? "90" : g == 180 ? "180" : g == 270 ? "270" : "");
		else if (i == 20)
			dice("  ...\n");
	}
	pdf_cierra(d);
	return 0;
}

static int o_une(pargs *a)
{
	pdoc *docs[MAX_TROZOS];
	ppag *p;
	u32 n = 0, k, total = 0;
	int i, r;
	pnuevo *s;
	popciones op;

	if (a->n < 2) {
		dice("Uso: PDF UNE a.pdf b.pdf ... /SALIDA=c.pdf\n");
		return 1;
	}
	if (pide_salida(a))
		return 1;
	for (i = 1; i < a->n; i++) {
		docs[i] = abre(a, a->t[i]);
		if (!docs[i]) {
			while (--i >= 1)
				pdf_cierra(docs[i]);
			return falta_clave ? 2 : 1;
		}
		total += docs[i]->n_paginas;
	}
	p = pide(total * sizeof(ppag));
	for (i = 1; i < a->n; i++)
		for (k = 0; k < docs[i]->n_paginas; k++) {
			p[n].d = docs[i];
			p[n].pagina = k;
			p[n].giro = 0;
			n++;
		}
	s = compone(p, n, a->marcadores);
	opciones_salida(a, &op);
	r = escribe(s, a->salida, &op);
	if (!r)
		dice("Hecho: %s, con %u páginas de %d archivos.\n", a->salida, n, a->n - 1);
	nuevo_suelta(s);
	so_suelta(p);
	for (i = 1; i < a->n; i++)
		pdf_cierra(docs[i]);
	return r;
}

/* las páginas que dice «t» (o todas) */
static u32 *lista(pdoc *d, const char *t, u32 *n)
{
	u32 *l = pide((d->n_paginas * 4 + 64) * sizeof(u32)), i;
	int k;

	if (!t || !*t) {
		for (i = 0; i < d->n_paginas; i++)
			l[i] = i;
		*n = d->n_paginas;
		return l;
	}
	k = rangos(t, d->n_paginas, l, d->n_paginas * 4 + 64);
	if (k <= 0) {
		dice("«%s»: no son páginas que tenga (tiene %u)\n", t, d->n_paginas);
		so_suelta(l);
		return NULL;
	}
	*n = (u32)k;
	return l;
}

static int haz_con(pargs *a, pdoc *d, u32 *l, u32 n, int giro_todas, const char *salida, popciones *op)
{
	ppag *p = pide((n ? n : 1) * sizeof(ppag));
	u32 i;
	pnuevo *s;
	int r;

	for (i = 0; i < n; i++) {
		p[i].d = d;
		p[i].pagina = l[i];
		p[i].giro = giro_todas;
	}
	s = compone(p, n, a->marcadores);
	r = escribe(s, salida, op);
	nuevo_suelta(s);
	so_suelta(p);
	return r;
}

static int o_extrae(pargs *a)
{
	pdoc *d;
	u32 *l, n;
	int r;
	popciones op;
	const char *rango;

	if (a->n < 2) {
		dice("Uso: PDF EXTRAE a.pdf 3,1,5-7 /SALIDA=b.pdf\n");
		return 1;
	}
	if (pide_salida(a))
		return 1;
	d = abre(a, a->t[1]);
	if (!d)
		return falta_clave ? 2 : 1;
	rango = a->n > 2 ? a->t[2] : a->paginas;
	if (!*rango && a->invierte)
		rango = "z-1";
	l = lista(d, rango, &n);
	if (!l) {
		pdf_cierra(d);
		return 1;
	}
	if (a->invierte && (a->n > 2 || a->paginas[0])) {
		u32 i;

		for (i = 0; i < n / 2; i++) {
			u32 t = l[i];
			l[i] = l[n - 1 - i];
			l[n - 1 - i] = t;
		}
	}
	opciones_salida(a, &op);
	r = haz_con(a, d, l, n, 0, a->salida, &op);
	if (!r)
		dice("Hecho: %s, con %u página%s.\n", a->salida, n, n == 1 ? "" : "s");
	so_suelta(l);
	pdf_cierra(d);
	return r;
}

static int o_quita(pargs *a)
{
	pdoc *d;
	u32 *l, n, i, k = 0, *quedan;
	u8 *fuera;
	int r;
	popciones op;

	if (a->n < 3) {
		dice("Uso: PDF QUITA a.pdf 2,4-5 /SALIDA=b.pdf\n");
		return 1;
	}
	if (pide_salida(a))
		return 1;
	d = abre(a, a->t[1]);
	if (!d)
		return falta_clave ? 2 : 1;
	l = lista(d, a->t[2], &n);
	if (!l) {
		pdf_cierra(d);
		return 1;
	}
	fuera = pide(d->n_paginas);
	memset(fuera, 0, d->n_paginas);
	for (i = 0; i < n; i++)
		fuera[l[i]] = 1;
	quedan = pide(d->n_paginas * sizeof(u32));
	for (i = 0; i < d->n_paginas; i++)
		if (!fuera[i])
			quedan[k++] = i;
	if (k == 0) {
		dice("No puede quedarse sin ninguna página.\n");
		r = 1;
	} else {
		opciones_salida(a, &op);
		r = haz_con(a, d, quedan, k, 0, a->salida, &op);
		if (!r)
			dice("Hecho: %s, con %u páginas (se han quitado %u).\n", a->salida, k, d->n_paginas - k);
	}
	so_suelta(fuera);
	so_suelta(quedan);
	so_suelta(l);
	pdf_cierra(d);
	return r;
}

static int o_gira(pargs *a)
{
	pdoc *d;
	u32 *l, n, i;
	ppag *p;
	int giro = 0, menos = 0, r;
	const char *g;
	u8 *girar;
	pnuevo *s;
	popciones op;

	if (a->n < 3) {
		dice("Uso: PDF GIRA a.pdf 90 [páginas] /SALIDA=b.pdf\n");
		return 1;
	}
	if (pide_salida(a))
		return 1;
	g = a->t[2];
	if (*g == '-') {
		menos = 1;
		g++;
	} else if (*g == '+')
		g++;
	while (*g >= '0' && *g <= '9' && giro < 10000)
		giro = giro * 10 + (*g++ - '0');
	if (*g || giro % 90) {
		dice("El giro va de 90 en 90 grados (90, 180, 270 o -90).\n");
		return 1;
	}
	if (menos)
		giro = 360 - giro % 360;
	d = abre(a, a->t[1]);
	if (!d)
		return falta_clave ? 2 : 1;
	l = lista(d, a->n > 3 ? a->t[3] : a->paginas, &n);
	if (!l) {
		pdf_cierra(d);
		return 1;
	}
	girar = pide(d->n_paginas);
	memset(girar, 0, d->n_paginas);
	for (i = 0; i < n; i++)
		girar[l[i]] = 1;
	p = pide(d->n_paginas * sizeof(ppag));
	for (i = 0; i < d->n_paginas; i++) {
		p[i].d = d;
		p[i].pagina = i;
		p[i].giro = girar[i] ? giro : 0;
	}
	s = compone(p, d->n_paginas, a->marcadores);
	opciones_salida(a, &op);
	r = escribe(s, a->salida, &op);
	if (!r)
		dice("Hecho: %s (%u páginas giradas %d grados).\n", a->salida, n, giro % 360);
	nuevo_suelta(s);
	so_suelta(p);
	so_suelta(girar);
	so_suelta(l);
	pdf_cierra(d);
	return r;
}

static int o_separa(pargs *a)
{
	pdoc *d;
	u32 grupos = 0, i, g;
	u32 ini[4096], fin_[4096];
	int r = 0;
	popciones op;
	char nombre[MAX_RUTA];

	if (a->n < 2) {
		dice("Uso: PDF SEPARA a.pdf [1-3,4-9] /SALIDA=parte.pdf\n");
		return 1;
	}
	if (pide_salida(a))
		return 1;
	d = abre(a, a->t[1]);
	if (!d)
		return falta_clave ? 2 : 1;
	if (a->n > 2) {
		/* cada trozo de la lista, un archivo */
		char *p = a->t[2];

		while (*p && grupos < 4096) {
			char uno[64];
			int k = 0;
			u32 l[2];
			int m;
			u32 *todo;

			while (*p && *p != ',' && k < 63)
				uno[k++] = *p++;
			uno[k] = 0;
			if (*p == ',')
				p++;
			todo = pide((d->n_paginas + 2) * sizeof(u32));
			m = rangos(uno, d->n_paginas, todo, d->n_paginas + 1);
			if (m <= 0) {
				dice("«%s»: no son páginas que tenga (tiene %u)\n", uno, d->n_paginas);
				so_suelta(todo);
				pdf_cierra(d);
				return 1;
			}
			l[0] = todo[0];
			l[1] = todo[m - 1];
			so_suelta(todo);
			ini[grupos] = l[0];
			fin_[grupos] = l[1];
			grupos++;
		}
	} else {
		u32 cada = a->cada > 0 ? (u32)a->cada : 1;

		for (i = 0; i < d->n_paginas && grupos < 4096; i += cada) {
			ini[grupos] = i;
			fin_[grupos] = i + cada - 1 < d->n_paginas ? i + cada - 1 : d->n_paginas - 1;
			grupos++;
		}
	}
	opciones_salida(a, &op);
	for (g = 0; g < grupos && !r; g++) {
		u32 l[MAX_PAGINAS > 0 ? 1 : 1], n = 0, k, *v;
		int paso = ini[g] <= fin_[g] ? 1 : -1;

		(void)l;
		v = pide((d->n_paginas + 1) * sizeof(u32));
		for (k = ini[g];; k += (u32)paso) {
			v[n++] = k;
			if (k == fin_[g])
				break;
		}
		numerado(a->salida, g + 1, grupos, ".PDF", nombre, sizeof nombre);
		r = haz_con(a, d, v, n, 0, nombre, &op);
		so_suelta(v);
		if (!r)
			dice("  %s: %u página%s\n", nombre, n, n == 1 ? "" : "s");
	}
	if (!r)
		dice("Hecho: %u archivos.\n", grupos);
	pdf_cierra(d);
	return r;
}

static int o_comprime(pargs *a)
{
	pdoc *d;
	u32 *l, n;
	int r;
	popciones op;

	if (a->n < 2) {
		dice("Uso: PDF COMPRIME a.pdf /SALIDA=b.pdf [/CALIDAD=ALTA|MEDIA|BAJA]\n");
		return 1;
	}
	if (pide_salida(a))
		return 1;
	d = abre(a, a->t[1]);
	if (!d)
		return falta_clave ? 2 : 1;
	l = lista(d, NULL, &n);
	memset(&op, 0, sizeof op);
	op.comprime = 1;
	op.objetos_en_flujo = 1;
	if (!a->calidad[0] || igual_sin_caja(a->calidad, "MEDIA")) {
		op.calidad_img = 70;
		op.lado_img = 1600;
	} else if (igual_sin_caja(a->calidad, "BAJA")) {
		op.calidad_img = 50;
		op.lado_img = 1000;
	} else if (igual_sin_caja(a->calidad, "ALTA")) {
		op.calidad_img = 0;	/* las imágenes, como estaban */
	} else {
		dice("/CALIDAD= es ALTA (las imágenes no se tocan), MEDIA o BAJA.\n");
		so_suelta(l);
		pdf_cierra(d);
		return 1;
	}
	r = haz_con(a, d, l, n, 0, a->salida, &op);
	if (!r) {
		const char *e;
		u32 largo;
		u8 *x = so_lee_archivo(a->salida, &largo, MAX_ARCHIVO_PDF, &e);

		if (x) {
			dice("Hecho: %s: de %u KB a %u KB (%u%%).\n", a->salida, (d->largo + 1023) / 1024, (largo + 1023) / 1024,
			     d->largo ? (u32)((u64)largo * 100 / d->largo) : 100);
			so_suelta(x);
		}
	}
	so_suelta(l);
	pdf_cierra(d);
	return r;
}

static int o_cifra(pargs *a, int poner)
{
	pdoc *d;
	u32 *l, n;
	int r;
	popciones op;
	char clave_abrir[130];

	if (a->n < 2) {
		dice(poner ? "Uso: PDF CIFRA a.pdf /CLAVE=x [/DUENO=y] [/CIFRADO=AES128|AES256|RC4] /SALIDA=b.pdf\n"
			   : "Uso: PDF DESCIFRA a.pdf /CLAVE=x /SALIDA=b.pdf\n");
		return 1;
	}
	if (pide_salida(a))
		return 1;
	if (poner && !a->clave[0]) {
		dice("Falta la contraseña: /CLAVE=...\n");
		return 1;
	}
	strcpy(clave_abrir, a->clave);
	if (poner)
		a->clave[0] = 0;	/* (el de entrada, sin contraseña; si la tiene, /DUENO no vale para abrir) */
	d = abre(a, a->t[1]);
	if (!d && poner && falta_clave) {
		/* (lleva ya una: se prueba con la nueva) */
		strcpy(a->clave, clave_abrir);
		falta_clave = 0;
		d = abre(a, a->t[1]);
	}
	strcpy(a->clave, clave_abrir);
	if (!d)
		return falta_clave ? 2 : 1;
	l = lista(d, NULL, &n);
	memset(&op, 0, sizeof op);
	if (poner) {
		op.cifrado = 2;
		if (igual_sin_caja(a->cifrado, "RC4") || igual_sin_caja(a->cifrado, "RC4128"))
			op.cifrado = 1;
		else if (igual_sin_caja(a->cifrado, "AES256"))
			op.cifrado = 3;
		else if (a->cifrado[0] && !igual_sin_caja(a->cifrado, "AES128") && !igual_sin_caja(a->cifrado, "AES")) {
			dice("/CIFRADO= es AES128 (el de siempre), AES256 o RC4.\n");
			so_suelta(l);
			pdf_cierra(d);
			return 1;
		}
		op.clave_usuario = a->clave;
		op.clave_dueno = a->dueno[0] ? a->dueno : NULL;
		op.permisos = -4;
	} else if (!d->cifrado)
		dice("(%s no llevaba contraseña)\n", a->t[1]);
	r = haz_con(a, d, l, n, 0, a->salida, &op);
	if (!r)
		dice(poner ? "Hecho: %s, con contraseña (%s).\n" : "Hecho: %s, sin contraseña.\n", a->salida,
		     op.cifrado == 1 ? "RC4 de 128 bits" : op.cifrado == 3 ? "AES de 256 bits" : "AES de 128 bits");
	so_suelta(l);
	pdf_cierra(d);
	return r;
}

static int escribe_archivo(const char *ruta, const void *d, u32 n)
{
	const char *e = NULL;
	void *f = so_crea(ruta, &e);
	int r;

	if (!f) {
		dice("%s: %s\n", ruta, e ? e : "no se puede crear");
		return 1;
	}
	r = so_escribe(f, d, n);
	if (so_cierra(f) || r) {
		dice("%s: no se ha podido escribir (¿el disco, lleno?)\n", ruta);
		return 1;
	}
	return 0;
}

static int o_texto(pargs *a)
{
	pdoc *d;
	u32 *l, n, i, total = 0, cap = 65536;
	u8 *todo;
	int r = 0, pag437 = so_pagina() == 437 && !a->utf8;

	if (a->n < 2) {
		dice("Uso: PDF TEXTO a.pdf [/SALIDA=a.txt] [/PAGINAS=1-3] [/UTF8]\n");
		return 1;
	}
	d = abre(a, a->t[1]);
	if (!d)
		return falta_clave ? 2 : 1;
	l = lista(d, a->n > 2 ? a->t[2] : a->paginas, &n);
	if (!l) {
		pdf_cierra(d);
		return 1;
	}
	todo = pide(cap);
	for (i = 0; i < n; i++) {
		u32 m;
		u8 *t = pagina_texto(d, l[i], &m);

		if (total + m + 8 > cap) {
			while (total + m + 8 > cap)
				cap *= 2;
			todo = repide(todo, cap);
		}
		memcpy(todo + total, t, m);
		total += m;
		if (i + 1 < n)
			todo[total++] = '\f';	/* (el salto de página, como pdftotext) */
		so_suelta(t);
	}
	so_suelta(l);
	/* a la página del sistema y con sus finales de línea */
	if (pag437 || so_pagina() == 437) {
		u8 *c = pide(total * 4 + 8);
		u32 k = 0, j;
		u8 *conv = pag437 ? pide(total * 3 + 8) : todo;
		u32 m = pag437 ? utf8_a_437(todo, total, conv) : total;

		for (j = 0; j < m; j++) {
			if (conv[j] == '\n')
				c[k++] = '\r';
			c[k++] = conv[j];
		}
		if (pag437)
			so_suelta(conv);
		so_suelta(todo);
		todo = c;
		total = k;
	}
	if (a->salida[0]) {
		r = escribe_archivo(a->salida, todo, total);
		if (!r)
			dice("Hecho: %s (%u octetos de texto, %u páginas).\n", a->salida, total, n);
	} else {
		u32 j = 0;

		/* a la pantalla, a trozos (so_dice lleva UTF-8: lo de la 437 se pasa antes) */
		while (j < total) {
			char t[400];
			u32 k = 0;

			while (j < total && k < 300) {
				u8 c = todo[j++];

				if (c == '\f' || c == '\r')
					continue;
				if (pag437 && c >= 128) {
					u8 *p = (u8 *)t + k;

					a_utf8(de_437(c), &p);
					k = (u32)(p - (u8 *)t);
				} else
					t[k++] = (char)c;
			}
			t[k] = 0;
			so_dice(t);
		}
	}
	so_suelta(todo);
	pdf_cierra(d);
	return r;
}

static int acaba_en(const char *t, const char *ext)
{
	u32 a = (u32)strlen(t), b = (u32)strlen(ext);

	return a >= b && igual_sin_caja(t + a - b, ext);
}

static int o_imagenes(pargs *a)
{
	pdoc *d;
	u32 *l, n, i;
	int r = 0, jpg, ppp = a->puntos > 0 ? a->puntos : 150;
	char nombre[MAX_RUTA];

	if (a->n < 2) {
		dice("Uso: PDF IMAGENES a.pdf /SALIDA=pag.png [/PUNTOS=150] [/PAGINAS=1-3]\n");
		return 1;
	}
	if (pide_salida(a))
		return 1;
	if (ppp < 10 || ppp > 600) {
		dice("/PUNTOS= va de 10 a 600 (puntos por pulgada).\n");
		return 1;
	}
	jpg = acaba_en(a->salida, ".JPG") || acaba_en(a->salida, ".JPEG");
	d = abre(a, a->t[1]);
	if (!d)
		return falta_clave ? 2 : 1;
	l = lista(d, a->n > 2 ? a->t[2] : a->paginas, &n);
	if (!l) {
		pdf_cierra(d);
		return 1;
	}
	for (i = 0; i < n && !r; i++) {
		pimagen *im = pinta_pagina(d, l[i], ppp, 0);
		u32 m;
		u8 *f;

		if (!im) {
			dice("La página %u no se ha podido pintar.\n", l[i] + 1);
			r = 1;
			break;
		}
		f = imagen_a_archivo(im, jpg, a->calidad[0] && igual_sin_caja(a->calidad, "BAJA") ? 60 : 88, &m);
		if (n == 1)
			fmt(nombre, sizeof nombre, "%s", a->salida);
		else
			numerado(a->salida, l[i] + 1, d->n_paginas, jpg ? ".JPG" : ".PNG", nombre, sizeof nombre);
		if (!f)
			r = 1;
		else {
			r = escribe_archivo(nombre, f, m);
			if (!r)
				dice("  %s: %u x %u puntos\n", nombre, (u32)im->an, (u32)im->al);
			so_suelta(f);
		}
		imagen_suelta(im);
	}
	if (!r)
		dice("Hecho: %u imagen%s.\n", n, n == 1 ? "" : "es");
	so_suelta(l);
	pdf_cierra(d);
	return r;
}

static int o_crea(pargs *a)
{
	pnuevo *s;
	ppapel papel;
	u32 *hojas, n = 0, arbol;
	int i, r;
	pobj *cat;
	popciones op;

	if (a->n < 2) {
		dice("Uso: PDF CREA a.jpg b.png ... /SALIDA=c.pdf [/PAPEL=A4|A3|A5|CARTA|IMAGEN] [/MARGEN=10]\n");
		return 1;
	}
	if (pide_salida(a))
		return 1;
	memset(&papel, 0, sizeof papel);
	papel.an = 595.28;
	papel.al = 841.89;
	if (igual_sin_caja(a->papel, "A3")) {
		papel.an = 841.89;
		papel.al = 1190.55;
	} else if (igual_sin_caja(a->papel, "A5")) {
		papel.an = 419.53;
		papel.al = 595.28;
	} else if (igual_sin_caja(a->papel, "CARTA")) {
		papel.an = 612;
		papel.al = 792;
	} else if (igual_sin_caja(a->papel, "IMAGEN")) {
		papel.an = papel.al = 0;
	} else if (a->papel[0] && !igual_sin_caja(a->papel, "A4")) {
		dice("/PAPEL= es A4, A3, A5, CARTA o IMAGEN (la página, lo que mida la imagen).\n");
		return 1;
	}
	papel.margen = (a->hay_margen ? a->margen : papel.an > 0 ? 10 : 0) * 72.0 / 25.4;
	s = nuevo_doc();
	hojas = pide((u32)a->n * sizeof(u32));
	for (i = 1; i < a->n; i++) {
		const char *e = NULL;
		u32 largo, num;
		u8 *datos = so_lee_archivo(a->t[i], &largo, 64u * 1024 * 1024, &e);

		if (!datos) {
			dice("%s: %s\n", a->t[i], e ? e : "no se puede leer");
			so_suelta(hojas);
			nuevo_suelta(s);
			return 1;
		}
		num = pagina_de_imagen(s, datos, largo, &papel, &e);
		so_suelta(datos);
		if (!num) {
			dice("%s: %s\n", a->t[i], e);
			so_suelta(hojas);
			nuevo_suelta(s);
			return 1;
		}
		hojas[n++] = num;
	}
	arbol = arbol_paginas(s, hojas, n);
	so_suelta(hojas);
	cat = o_dicc(s->d);
	o_pon(s->d, cat, "Type", o_nombre(s->d, "Catalog"));
	o_pon(s->d, cat, "Pages", o_ref(s->d, arbol, 0));
	s->raiz = nuevo_obj(s, cat, NULL);
	{
		pobj *info = o_dicc(s->d);
		static const u8 prod[] = "Hispa-PDF 1.0 (Hispa-Pentium)";

		o_pon(s->d, info, "Producer", o_cadena(s->d, prod, sizeof prod - 1));
		s->info = nuevo_obj(s, info, NULL);
	}
	memset(&op, 0, sizeof op);
	r = escribe(s, a->salida, &op);
	if (!r)
		dice("Hecho: %s, con %u página%s.\n", a->salida, n, n == 1 ? "" : "s");
	nuevo_suelta(s);
	return r;
}

/* ---- para la ventana ---- */

/* las miniaturas: «HPMP», cuántas páginas tiene, la primera y cuántas van;
 * y de cada una su ancho, su alto, su giro y sus puntos 0x00RRGGBB */
static int o_miniaturas(pargs *a)
{
	pdoc *d;
	u32 desde, hasta, i, cab[4];
	int lado = a->lado >= 16 && a->lado <= 512 ? a->lado : 96;
	void *f;
	const char *e = NULL;
	int r = 0;

	if (a->n < 2 || pide_salida(a))
		return 1;
	d = abre(a, a->t[1]);
	if (!d) {
		/* (aun así, el archivo: 0 páginas; y en el código, por qué) */
		return falta_clave ? 2 : 1;
	}
	desde = a->desde > 0 ? (u32)a->desde - 1 : 0;
	hasta = a->hasta > 0 ? (u32)a->hasta : d->n_paginas;
	if (hasta > d->n_paginas)
		hasta = d->n_paginas;
	if (desde > hasta)
		desde = hasta;
	f = so_crea(a->salida, &e);
	if (!f) {
		pdf_cierra(d);
		return 1;
	}
	cab[0] = 0x504D5048;	/* «HPMP» */
	cab[1] = d->n_paginas;
	cab[2] = desde + 1;
	cab[3] = hasta - desde;
	if (so_escribe(f, cab, 16))
		r = 1;
	for (i = desde; i < hasta && !r; i++) {
		pimagen *im = pinta_pagina(d, i, 0, lado);
		u32 c[3];

		if (!im) {
			c[0] = c[1] = 0;
			c[2] = 0;
			so_escribe(f, c, 12);
			continue;
		}
		c[0] = (u32)im->an;
		c[1] = (u32)im->al;
		c[2] = (u32)pagina_giro(d, i);
		if (so_escribe(f, c, 12) || so_escribe(f, im->px, (u32)im->an * im->al * 4))
			r = 1;
		imagen_suelta(im);
	}
	if (so_cierra(f))
		r = 1;
	pdf_cierra(d);
	return r;
}

/* una página, para verla grande: «HPMI», el ancho, el alto y sus puntos */
static int o_pagina(pargs *a)
{
	pdoc *d;
	u32 k = 0, cab[3];
	pimagen *im;
	void *f;
	const char *e = NULL;
	int r = 0, lado;

	if (a->n < 3 || pide_salida(a))
		return 1;
	{
		const char *p = a->t[2];

		while (*p >= '0' && *p <= '9' && k < 1000000)
			k = k * 10 + (u32)(*p++ - '0');
	}
	d = abre(a, a->t[1]);
	if (!d)
		return falta_clave ? 2 : 1;
	if (k < 1 || k > d->n_paginas) {
		pdf_cierra(d);
		return 1;
	}
	lado = a->lado > 0 && a->lado <= 4000 ? a->lado : 800;
	im = pinta_pagina(d, k - 1, 0, lado);
	if (!im) {
		pdf_cierra(d);
		return 1;
	}
	f = so_crea(a->salida, &e);
	if (!f)
		r = 1;
	else {
		cab[0] = 0x494D5048;
		cab[1] = (u32)im->an;
		cab[2] = (u32)im->al;
		if (so_escribe(f, cab, 12) || so_escribe(f, im->px, (u32)im->an * im->al * 4))
			r = 1;
		if (so_cierra(f))
			r = 1;
	}
	imagen_suelta(im);
	pdf_cierra(d);
	return r;
}

/* HAZ lista.txt: lo que ha preparado la ventana. Líneas:
 *   SALIDA=ruta              ARCHIVO=ruta (el 0, el 1...)   CLAVE=x (la de abrir)
 *   PAGINA=archivo página giro    COMPRIME=0|1|2|3   CIFRA=clave   CIFRADO=AES128|AES256|RC4
 *   DUENO=clave              SEPARA=1 (un archivo por página) */
static int o_haz(pargs *a)
{
	const char *e = NULL;
	u32 largo, i, n_arch = 0, n_pag = 0;
	char *t, *p;
	static char rutas[MAX_TROZOS][MAX_RUTA];
	pdoc *docs[MAX_TROZOS];
	ppag *pags;
	int comprime = 0, separa = 0, r = 0;
	char cifra_con[130] = "", cifrado[16] = "", dueno[130] = "";
	popciones op;

	if (a->n < 2)
		return 1;
	t = (char *)so_lee_archivo(a->t[1], &largo, 4u * 1024 * 1024, &e);
	if (!t) {
		dice("%s: %s\n", a->t[1], e);
		return 1;
	}
	t = repide(t, largo + 1);
	t[largo] = 0;
	pags = pide((largo / 4 + 2) * sizeof(ppag));
	memset(docs, 0, sizeof docs);
	for (p = t; *p;) {
		char *l = p, *fin;

		while (*p && *p != '\n')
			p++;
		fin = p;
		if (*p)
			p++;
		while (fin > l && (fin[-1] == '\r' || fin[-1] == ' '))
			fin--;
		*fin = 0;
#define VALOR(k) (empieza_sin_caja(l, k "=") ? l + strlen(k) + 1 : NULL)
		if (VALOR("SALIDA"))
			fmt(a->salida, sizeof a->salida, "%s", VALOR("SALIDA"));
		else if (VALOR("CLAVE"))
			fmt(a->clave, sizeof a->clave, "%s", VALOR("CLAVE"));
		else if (VALOR("CIFRA"))
			fmt(cifra_con, sizeof cifra_con, "%s", VALOR("CIFRA"));
		else if (VALOR("CIFRADO"))
			fmt(cifrado, sizeof cifrado, "%s", VALOR("CIFRADO"));
		else if (VALOR("DUENO"))
			fmt(dueno, sizeof dueno, "%s", VALOR("DUENO"));
		else if (VALOR("COMPRIME"))
			comprime = *VALOR("COMPRIME") - '0';
		else if (VALOR("SEPARA"))
			separa = *VALOR("SEPARA") == '1';
		else if (VALOR("ARCHIVO") && n_arch < MAX_TROZOS)
			fmt(rutas[n_arch++], MAX_RUTA, "%s", VALOR("ARCHIVO"));
		else if (VALOR("PAGINA")) {
			const char *q = VALOR("PAGINA");
			u32 v[3] = {0, 0, 0}, k;
			int menos = 0;

			for (k = 0; k < 3; k++) {
				while (*q == ' ')
					q++;
				if (k == 2 && *q == '-') {
					menos = 1;
					q++;
				}
				while (*q >= '0' && *q <= '9' && v[k] < 1000000)
					v[k] = v[k] * 10 + (u32)(*q++ - '0');
			}
			if (v[0] < n_arch && v[1] >= 1) {
				pags[n_pag].d = (pdoc *)(uintptr_t)(v[0] + 1);
				pags[n_pag].pagina = v[1] - 1;
				pags[n_pag].giro = (int)(menos ? 360 - v[2] % 360 : v[2] % 360);
				n_pag++;
			}
		}
#undef VALOR
	}
	so_suelta(t);
	if (!n_pag || !a->salida[0]) {
		dice("La lista no dice qué hacer.\n");
		so_suelta(pags);
		return 1;
	}
	for (i = 0; i < n_arch; i++) {
		docs[i] = abre(a, rutas[i]);
		if (!docs[i]) {
			r = falta_clave ? 2 : 1;
			goto fuera;
		}
	}
	for (i = 0; i < n_pag; i++) {
		u32 k = (u32)(uintptr_t)pags[i].d - 1;

		pags[i].d = docs[k];
		if (pags[i].pagina >= docs[k]->n_paginas) {
			dice("La página %u de %s no está.\n", pags[i].pagina + 1, rutas[k]);
			r = 1;
			goto fuera;
		}
	}
	memset(&op, 0, sizeof op);
	if (comprime) {
		op.comprime = 1;
		op.objetos_en_flujo = 1;
		if (comprime == 2) {
			op.calidad_img = 70;
			op.lado_img = 1600;
		} else if (comprime == 3) {
			op.calidad_img = 50;
			op.lado_img = 1000;
		}
	}
	if (cifra_con[0]) {
		op.cifrado = igual_sin_caja(cifrado, "RC4") ? 1 : igual_sin_caja(cifrado, "AES256") ? 3 : 2;
		op.clave_usuario = cifra_con;
		op.clave_dueno = dueno[0] ? dueno : NULL;
		op.permisos = -4;
	}
	if (separa) {
		char nombre[MAX_RUTA];

		for (i = 0; i < n_pag && !r; i++) {
			pnuevo *s = compone(&pags[i], 1, 0);

			numerado(a->salida, i + 1, n_pag, ".PDF", nombre, sizeof nombre);
			r = escribe(s, nombre, &op);
			nuevo_suelta(s);
		}
		if (!r)
			dice("Hecho: %u archivos.\n", n_pag);
	} else {
		pnuevo *s = compone(pags, n_pag, 1);

		r = escribe(s, a->salida, &op);
		nuevo_suelta(s);
		if (!r)
			dice("Hecho: %s, con %u páginas.\n", a->salida, n_pag);
	}
fuera:
	for (i = 0; i < n_arch; i++)
		pdf_cierra(docs[i]);
	so_suelta(pags);
	return r;
}

/* la orden entera: da el código de salida (0 bien; 1 mal; 2 falta la contraseña) */
int hpdf_orden(const char *linea)
{
	so_salto salto;
	so_salto *antes = hpdf_salto;
	pargs *a = pide(sizeof(pargs));
	int r;

	falta_clave = 0;
	parte(linea, a);
	hpdf_salto = &salto;
	if (so_marca(salto)) {
		/* (un error sin vuelta: sin memoria o un límite pasado) */
		dice("No se puede seguir: %s.\n", hpdf_mensaje);
		hpdf_salto = antes;
		return 1;
	}
	if (a->n == 0 || igual_sin_caja(a->t[0], "AYUDA") || igual_sin_caja(a->t[0], "/?")) {
		so_dice(ayuda_texto);
		r = 0;
	} else if (igual_sin_caja(a->t[0], "INFO"))
		r = o_info(a);
	else if (igual_sin_caja(a->t[0], "UNE") || igual_sin_caja(a->t[0], "UNIR"))
		r = o_une(a);
	else if (igual_sin_caja(a->t[0], "SEPARA") || igual_sin_caja(a->t[0], "SEPARAR"))
		r = o_separa(a);
	else if (igual_sin_caja(a->t[0], "EXTRAE") || igual_sin_caja(a->t[0], "ORDENA"))
		r = o_extrae(a);
	else if (igual_sin_caja(a->t[0], "QUITA"))
		r = o_quita(a);
	else if (igual_sin_caja(a->t[0], "GIRA"))
		r = o_gira(a);
	else if (igual_sin_caja(a->t[0], "COMPRIME"))
		r = o_comprime(a);
	else if (igual_sin_caja(a->t[0], "TEXTO"))
		r = o_texto(a);
	else if (igual_sin_caja(a->t[0], "IMAGENES") || igual_sin_caja(a->t[0], "IMAGEN"))
		r = o_imagenes(a);
	else if (igual_sin_caja(a->t[0], "CREA"))
		r = o_crea(a);
	else if (igual_sin_caja(a->t[0], "CIFRA"))
		r = o_cifra(a, 1);
	else if (igual_sin_caja(a->t[0], "DESCIFRA"))
		r = o_cifra(a, 0);
	else if (igual_sin_caja(a->t[0], "MINIATURAS"))
		r = o_miniaturas(a);
	else if (igual_sin_caja(a->t[0], "PAGINA"))
		r = o_pagina(a);
	else if (igual_sin_caja(a->t[0], "HAZ"))
		r = o_haz(a);
	else {
		dice("No sé qué es «%s». Las órdenes: PDF AYUDA.\n", a->t[0]);
		r = 1;
	}
	hpdf_salto = antes;
	so_suelta(a);
	return r;
}
