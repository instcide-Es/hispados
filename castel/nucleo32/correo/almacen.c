/* almacen.c - Correo: lo guardado, para leer sin red (correo.h, «Lo
 * guardado»). INDICE.IDX: una cabecera de 32 octetos («HPCI», la versión,
 * UIDVALIDITY, cuántas, UIDNEXT) y las fichas por orden de UID; se escribe
 * solo lo cambiado (lo de detrás de la primera ficha tocada) o, si se ha
 * quitado algo, entero por INDICE.NUE (si se va la luz, queda uno entero).
 * CUERPOS.DAT: los mensajes, uno detrás de otro; lo que se borra se cuenta
 * como «muerto» y buzon_compacta lo quita. CARPETAS.LST: las carpetas de la
 * cuenta (su nombre, sus marcas, sus contadores y su número Fnnn).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "correo.h"

#define MAGIA 0x49435048u	/* «HPCI» */
#define VERSION 1
#define NADA 0xFFFFFFFFu

typedef struct {
	u32 magia, version, uidvalidity, n, uidnext, ficha, cuerpos, muertos;
} Cabeza;

void alm_ruta(char *d, u32 cabe, int cuenta, int carpeta, const char *nombre)
{
	char b = so_barra();

	if (carpeta > 0)
		snprintf(d, cabe, "%s%cC%d%cF%03d%s%s", dir_datos, b, cuenta, b, carpeta, nombre ? (b == '\\' ? "\\" : "/") : "", nombre ? nombre : "");
	else
		snprintf(d, cabe, "%s%cC%d%s%s", dir_datos, b, cuenta, nombre ? (b == '\\' ? "\\" : "/") : "", nombre ? nombre : "");
}

/* ---- las carpetas ---- */
int alm_carpetas_lee(int cuenta, Carpeta *v, int max)
{
	char r[260];
	u8 *d;
	u32 n, i = 0;
	int k = 0;

	alm_ruta(r, sizeof r, cuenta, 0, "CARPETAS.LST");
	d = so_lee_archivo(r, &n, 1u << 20);
	if (!d)
		return 0;
	while (i < n && k < max) {
		u32 j = i, campo = 0, ini;
		Carpeta *c = &v[k];
		char t[260];

		memset(c, 0, sizeof *c);
		while (j < n && d[j] != '\n')
			j++;
		/* num|marcas|total|no_leidos|uidnext|uidvalidity|delim|crudo|nombre */
		ini = i;
		for (; i <= j; i++) {
			if (i == j || d[i] == '|' || d[i] == '\r') {
				u32 l = i - ini, x = 0, q;

				if (campo < 7) {
					for (q = ini; q < i && d[q] >= '0' && d[q] <= '9'; q++)
						x = x * 10 + (u32)(d[q] - '0');
				}
				copia_utf8(t, sizeof t, (const char *)d + ini, l);
				switch (campo) {
				case 0: c->num = (int)x; break;
				case 1: c->marcas = x; break;
				case 2: c->total = x; break;
				case 3: c->no_leidos = x; break;
				case 4: c->uidnext = x; break;
				case 5: c->uidvalidity = x; break;
				case 6: c->delim = (char)x; break;
				case 7: snprintf(c->crudo, sizeof c->crudo, "%s", t); break;
				case 8: snprintf(c->nombre, sizeof c->nombre, "%s", t); break;
				}
				campo++;
				ini = i + 1;
				if (i == j || d[i] == '\r')
					break;
			}
		}
		if (campo >= 9 && c->crudo[0] && c->num > 0) {
			const char *p;

			for (p = c->nombre; *p; p++)
				if (c->delim && *p == c->delim)
					c->nivel++;
			k++;
		}
		i = j + 1;
	}
	so_suelta(d);
	return k;
}

int alm_carpetas_guarda(int cuenta, const Carpeta *v, int n)
{
	char r[260], t[600];
	void *f;
	int i, mal = 0;

	alm_ruta(r, sizeof r, cuenta, 0, NULL);
	so_haz_carpeta(r);
	alm_ruta(r, sizeof r, cuenta, 0, "CARPETAS.LST");
	f = so_abre(r, SO_CREAR);
	if (!f)
		return -1;
	for (i = 0; i < n; i++) {
		snprintf(t, sizeof t, "%d|%u|%u|%u|%u|%u|%u|%s|%s\r\n", v[i].num, (unsigned)v[i].marcas, (unsigned)v[i].total,
			 (unsigned)v[i].no_leidos, (unsigned)v[i].uidnext, (unsigned)v[i].uidvalidity, (unsigned)(u8)v[i].delim,
			 v[i].crudo, v[i].nombre);
		mal |= so_escribe(f, t, (u32)strlen(t));
	}
	return so_cierra(f) || mal ? -1 : 0;
}

void alm_numera(Carpeta *v, int n)
{
	int i, j, mayor = 0;

	for (i = 0; i < n; i++)
		if (v[i].num > mayor)
			mayor = v[i].num;
	for (i = 0; i < n; i++)
		if (v[i].num <= 0) {
			v[i].num = ++mayor;
			for (j = 0; j < i; j++)
				if (v[j].num == v[i].num)
					v[i].num = ++mayor;
		}
}

/* ---- el índice ---- */
static void ruta(const Buzon *b, char *d, u32 cabe, const char *nombre)
{
	alm_ruta(d, cabe, b->cuenta, b->carpeta, nombre);
}

int buzon_abre(Buzon *b, int cuenta, int carpeta)
{
	char r[260];
	void *f;
	Cabeza c;
	u32 n;

	memset(b, 0, sizeof *b);
	b->cuenta = cuenta;
	b->carpeta = carpeta;
	b->sucio_desde = NADA;
	alm_ruta(r, sizeof r, cuenta, 0, NULL);
	so_haz_carpeta(r);
	ruta(b, r, sizeof r, NULL);
	so_haz_carpeta(r);
	ruta(b, r, sizeof r, "INDICE.IDX");
	f = so_abre(r, SO_LEER);
	if (!f)
		return 0;
	if (so_lee(f, &c, sizeof c) != (int)sizeof c || c.magia != MAGIA || c.version != VERSION || c.ficha != sizeof(Ficha) ||
	    c.n > 2000000u) {
		so_cierra(f);
		so_apunta("almac\xc3\xa9n: %s no vale: se empieza de nuevo", r);
		return 0;
	}
	n = so_largo(f);
	if (n < sizeof c + c.n * sizeof(Ficha))
		c.n = (n - (u32)sizeof c) / (u32)sizeof(Ficha);
	b->cabe = c.n + 64;
	b->f = so_pide((size_t)b->cabe * sizeof(Ficha));
	if (!b->f) {
		so_cierra(f);
		b->cabe = 0;
		return -1;
	}
	{
		u32 hecho = 0;

		while (hecho < c.n * sizeof(Ficha)) {
			int k = so_lee(f, (u8 *)b->f + hecho, c.n * (u32)sizeof(Ficha) - hecho);

			if (k <= 0)
				break;
			hecho += (u32)k;
		}
		b->n = hecho / (u32)sizeof(Ficha);
	}
	so_cierra(f);
	b->uidvalidity = c.uidvalidity;
	b->uidnext = c.uidnext;
	b->cuerpos = c.cuerpos;
	b->muertos = c.muertos;
	/* lo que miente (no ordenado, cuerpos fuera de CUERPOS.DAT): se arregla */
	{
		u32 i, k = 0;

		for (i = 0; i < b->n; i++) {
			Ficha *x = &b->f[i];

			if (k && x->uid <= b->f[k - 1].uid)
				continue;
			if (x->largo && (x->pos > b->cuerpos || x->largo > b->cuerpos - x->pos)) {
				x->largo = 0;
				x->banderas &= ~B_BAJADO;
			}
			x->de[sizeof x->de - 1] = 0;
			x->dir[sizeof x->dir - 1] = 0;
			x->asunto[sizeof x->asunto - 1] = 0;
			b->f[k++] = *x;
		}
		if (k != b->n)
			b->reescribir = 1;
		b->n = k;
	}
	return 1;
}

static int escribe_cabeza(Buzon *b, void *f)
{
	Cabeza c;

	c.magia = MAGIA;
	c.version = VERSION;
	c.uidvalidity = b->uidvalidity;
	c.n = b->n;
	c.uidnext = b->uidnext;
	c.ficha = sizeof(Ficha);
	c.cuerpos = b->cuerpos;
	c.muertos = b->muertos;
	return so_escribe(f, &c, sizeof c);
}

int buzon_guarda(Buzon *b)
{
	char r[260], nueva[260];
	void *f;
	int mal = 0;

	if (b->sucio_desde == NADA && !b->reescribir)
		return 0;
	ruta(b, r, sizeof r, "INDICE.IDX");
	if (!b->reescribir) {
		f = so_abre(r, SO_CAMBIAR);
		if (f && so_largo(f) >= sizeof(Cabeza) + b->sucio_desde * sizeof(Ficha)) {
			mal |= escribe_cabeza(b, f);
			if (b->sucio_desde < b->n) {
				mal |= so_posicion(f, (u32)sizeof(Cabeza) + b->sucio_desde * (u32)sizeof(Ficha));
				mal |= so_escribe(f, b->f + b->sucio_desde, (b->n - b->sucio_desde) * (u32)sizeof(Ficha));
			}
			if (!so_cierra(f) && !mal) {
				b->sucio_desde = NADA;
				return 0;
			}
		} else if (f)
			so_cierra(f);
	}
	ruta(b, nueva, sizeof nueva, "INDICE.NUE");
	f = so_abre(nueva, SO_CREAR);
	if (!f)
		return -1;
	mal = escribe_cabeza(b, f);
	if (b->n)
		mal |= so_escribe(f, b->f, b->n * (u32)sizeof(Ficha));
	if (so_cierra(f) || mal) {
		so_borra(nueva);
		return -1;
	}
	so_borra(r);
	if (so_renombra(nueva, r))
		return -1;
	b->reescribir = 0;
	b->sucio_desde = NADA;
	return 0;
}

void buzon_cierra(Buzon *b)
{
	buzon_guarda(b);
	so_suelta(b->f);
	b->f = NULL;
	b->n = b->cabe = 0;
}

void buzon_vacia(Buzon *b, u32 uidvalidity)
{
	char r[260];

	b->n = 0;
	b->uidvalidity = uidvalidity;
	b->uidnext = 0;
	b->cuerpos = 0;
	b->muertos = 0;
	b->reescribir = 1;
	ruta(b, r, sizeof r, "CUERPOS.DAT");
	so_borra(r);
}

u32 buzon_sitio(Buzon *b, u32 uid)
{
	u32 a = 0, z = b->n;

	while (a < z) {
		u32 m = a + (z - a) / 2;

		if (b->f[m].uid < uid)
			a = m + 1;
		else
			z = m;
	}
	return a;
}

Ficha *buzon_busca(Buzon *b, u32 uid)
{
	u32 i = buzon_sitio(b, uid);

	return i < b->n && b->f[i].uid == uid ? &b->f[i] : NULL;
}

static void sucio(Buzon *b, u32 i)
{
	if (b->sucio_desde == NADA || i < b->sucio_desde)
		b->sucio_desde = i;
}

Ficha *buzon_pon(Buzon *b, const Ficha *f)
{
	u32 i = buzon_sitio(b, f->uid);

	if (i < b->n && b->f[i].uid == f->uid) {
		/* ya está: lo de la ficha nueva, sin perder lo bajado */
		u32 pos = b->f[i].pos, largo = b->f[i].largo, bajado = b->f[i].banderas & B_BAJADO;

		b->f[i] = *f;
		if (largo && !f->largo) {
			b->f[i].pos = pos;
			b->f[i].largo = largo;
			b->f[i].banderas |= bajado;
		}
		sucio(b, i);
		return &b->f[i];
	}
	if (b->n == b->cabe) {
		u32 c = b->cabe ? b->cabe + b->cabe / 2 + 64 : 256;
		Ficha *q = so_repide(b->f, (size_t)c * sizeof(Ficha));

		if (!q)
			return NULL;
		b->f = q;
		b->cabe = c;
	}
	if (i < b->n) {
		memmove(&b->f[i + 1], &b->f[i], (b->n - i) * sizeof(Ficha));
		b->reescribir = 1;
	}
	b->f[i] = *f;
	b->n++;
	sucio(b, i);
	return &b->f[i];
}

void buzon_quita(Buzon *b, u32 uid)
{
	u32 i = buzon_sitio(b, uid);

	if (i >= b->n || b->f[i].uid != uid)
		return;
	b->muertos += b->f[i].largo;
	memmove(&b->f[i], &b->f[i + 1], (b->n - i - 1) * sizeof(Ficha));
	b->n--;
	b->reescribir = 1;
}

void buzon_cambia(Buzon *b, Ficha *f)
{
	if (f >= b->f && f < b->f + b->n)
		sucio(b, (u32)(f - b->f));
}

u32 buzon_max_uid(const Buzon *b)
{
	return b->n ? b->f[b->n - 1].uid : 0;
}

int buzon_guarda_cuerpo(Buzon *b, Ficha *f, const u8 *d, u32 n)
{
	char r[260];
	void *a;
	u32 pos;

	ruta(b, r, sizeof r, "CUERPOS.DAT");
	a = so_abre(r, SO_CAMBIAR);
	if (!a)
		return -1;
	pos = so_largo(a);
	if (pos != b->cuerpos) {
		/* (lo de detrás no es de nadie: un corte a medias) */
		if (pos < b->cuerpos)
			b->cuerpos = pos;
		pos = b->cuerpos;
	}
	if (so_posicion(a, pos) || so_escribe(a, d, n)) {
		so_cierra(a);
		return -1;
	}
	if (so_cierra(a))
		return -1;
	if (f->largo)
		b->muertos += f->largo;
	f->pos = pos;
	f->largo = n;
	f->banderas |= B_BAJADO;
	f->banderas &= ~B_GRANDE;
	b->cuerpos = pos + n;
	buzon_cambia(b, f);
	/* la cabecera dice dónde acaba CUERPOS.DAT: se escribe ya */
	if (b->sucio_desde == NADA)
		b->sucio_desde = b->n;
	return 0;
}

u8 *buzon_cuerpo(Buzon *b, const Ficha *f, u32 *n)
{
	char r[260];
	void *a;
	u8 *d;
	u32 k = 0;

	*n = 0;
	if (!f->largo || f->pos > b->cuerpos || f->largo > b->cuerpos - f->pos)
		return NULL;
	ruta(b, r, sizeof r, "CUERPOS.DAT");
	a = so_abre(r, SO_LEER);
	if (!a)
		return NULL;
	d = so_pide(f->largo + 1);
	if (!d || so_posicion(a, f->pos)) {
		so_cierra(a);
		so_suelta(d);
		return NULL;
	}
	while (k < f->largo) {
		int x = so_lee(a, d + k, f->largo - k);

		if (x <= 0)
			break;
		k += (u32)x;
	}
	so_cierra(a);
	if (k != f->largo) {
		so_suelta(d);
		return NULL;
	}
	d[k] = 0;
	*n = k;
	return d;
}

int buzon_compacta(Buzon *b, u32 tope)
{
	char r[260], nueva[260];
	void *de, *a;
	u8 *t;
	u32 i, pos = 0, total = 0, desde = 0, *nuevo;

	if (!b->muertos && (!tope || b->cuerpos <= tope))
		return 0;
	/* con tope: los más viejos (los primeros UID) dejan de estar bajados */
	for (i = b->n; i-- > 0;)
		if (b->f[i].largo) {
			if (tope && total + b->f[i].largo > tope) {
				desde = i + 1;
				break;
			}
			total += b->f[i].largo;
		}
	ruta(b, r, sizeof r, "CUERPOS.DAT");
	ruta(b, nueva, sizeof nueva, "CUERPOS.NUE");
	de = so_abre(r, SO_LEER);
	a = so_abre(nueva, SO_CREAR);
	t = so_pide(65536);
	nuevo = so_pide((b->n + 1) * sizeof(u32));
	if (!de || !a || !t || !nuevo)
		goto mal;
	for (i = 0; i < b->n; i++) {
		Ficha *f = &b->f[i];
		u32 k = 0;

		nuevo[i] = NADA;
		if (!f->largo || i < desde)
			continue;
		if (so_posicion(de, f->pos))
			goto mal;
		while (k < f->largo) {
			u32 q = f->largo - k > 65536 ? 65536 : f->largo - k;

			if (so_lee(de, t, q) != (int)q || so_escribe(a, t, q))
				goto mal;
			k += q;
		}
		nuevo[i] = pos;
		pos += f->largo;
	}
	so_cierra(de);
	de = NULL;
	so_suelta(t);
	t = NULL;
	if (so_cierra(a)) {
		a = NULL;
		goto mal;
	}
	a = NULL;
	so_borra(r);
	if (so_renombra(nueva, r))
		goto mal;
	for (i = 0; i < b->n; i++) {
		Ficha *f = &b->f[i];

		if (!f->largo)
			continue;
		if (nuevo[i] == NADA) {
			f->largo = 0;
			f->banderas &= ~B_BAJADO;
		} else
			f->pos = nuevo[i];
	}
	so_suelta(nuevo);
	b->cuerpos = pos;
	b->muertos = 0;
	b->reescribir = 1;
	return buzon_guarda(b);
mal:
	so_cierra(de);
	so_cierra(a);
	so_suelta(t);
	so_suelta(nuevo);
	so_borra(nueva);
	return -1;
}
