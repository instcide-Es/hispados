/* orto_pluma.c - Pluma 2: el corrector (oficina/comun/orto.h) en Pluma: el
 * diccionario (DICCIO.DAT, el de la oficina de Hispa-386: el del paquete
 * pluma, o el de la oficina), las palabras del usuario (\DOCUMENT\PROPIAS.TXT)
 * y las faltas de cada párrafo, para el trazo ondulado.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "ventana.h"
#include "orto.h"
#include "texto.h"

static char propias[80];

void pl_orto_inicia(void)
{
	char r[80];
	char u = so_unidad_sistema();
	static const char *const sitios[] = { "%c:\\HISPA\\PLUMA\\DICCIO.DAT", "%c:\\OFICINA\\DICCIO.DAT", "%c:\\DICCIO.DAT",
		"%c:\\HISPADOS\\PROGS\\DICCIO.DAT", NULL };
	int i;

	for (i = 0; sitios[i] && !orto_hay(); i++) {
		fmt(r, sizeof r, sitios[i], u);
		if (orto_carga_archivo(r))
			dice("pluma: diccionario %s", r);
	}
#if !defined(OFI_PENTIUM)
	/* (en el anfitrión, el del repositorio: PLUMA_DICCIO, o donde está siempre) */
	if (!orto_hay()) {
		extern char *getenv(const char *);
		const char *e = getenv("PLUMA_DICCIO");

		if (e && orto_carga_archivo(e))
			dice("pluma: diccionario %s", e);
	}
#endif
	if (!orto_hay()) {
		dice("pluma: sin diccionario: no se revisa la ortografía");
		return;
	}
	fmt(propias, sizeof propias, "%c:\\DOCUMENT\\PROPIAS.TXT", u);
	{
		u32 n;
		u8 *t = so_lee_archivo(propias, &n, 1u << 20, NULL);

		if (t) {
			orto_propias_poner((char *)t, n);
			so_suelta(t);
		}
	}
}

int pl_orto_hay(void)
{
	return orto_hay();
}

void pl_orto_faltas(documento *d, parrafo *p)
{
	u32 no_hasta = 0xFFFFFFFFu;

	if (!orto_hay())
		return;
	if (p->faltas && p->faltas_version == p->version)
		return;
	so_suelta(p->faltas);
	p->faltas = so_pide(p->n + 1);
	if (!p->faltas)
		return;
	/* (la palabra que se está escribiendo, con el cursor detrás, aún no) */
	if (P.d == d && p == &d->par[P.e.cur.p])
		no_hasta = P.e.cur.o;
	orto_faltas(p->t, p->n, p->faltas, no_hasta);
	p->faltas_version = p->version;
}

int pl_orto_bien(const u32 *pal, u32 n)
{
	return orto_bien(pal, n);
}

int pl_orto_sugiere(const u32 *pal, u32 n, char sug[][64], int max)
{
	u32 s[ORTO_SUG][ORTO_LARGO + 1];
	int k, m;

	if (max > ORTO_SUG)
		max = ORTO_SUG;
	m = orto_sugiere(pal, n, s, max);
	for (k = 0; k < m; k++) {
		char *u = utf8_de_u32(s[k], u32_largo(s[k]));

		copia(sug[k], u ? u : "", 64);
		so_suelta(u);
	}
	return m;
}

void pl_orto_anade(const u32 *pal, u32 n)
{
	u32 m;
	char *t;

	if (orto_propia(pal, n))
		return;
	t = orto_propias(&m);
	if (t) {
		const char *e = NULL;
		void *f = so_crea(propias, &e);

		if (f) {
			so_escribe(f, t, m);
			so_cierra(f);
		} else
			pl_aviso("No se ha podido guardar %s: %s", propias, e ? e : "?");
		so_suelta(t);
	}
}

void pl_orto_omite(const u32 *pal, u32 n)
{
	orto_omite(pal, n);
}
