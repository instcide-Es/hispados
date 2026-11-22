/* vcard.c - Correo: las tarjetas de visita (vCard 2.1, 3.0 y 4.0) de la
 * libreta: leer (los renglones partidos, QUOTED-PRINTABLE y CHARSET de la
 * 2.1, los escapes de la 3.0 y la 4.0, varios contactos en un archivo) y
 * escribir (vCard 3.0, UTF-8, CRLF, renglones de 75 octetos como mucho sin
 * partir una letra). Lo que llega es hostil: renglones con tope, tiempo
 * lineal.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "vcard.h"
#include "mime.h"
#include "interno.h"

#define TOPE_RENGLON (256u << 10)	/* un renglón lógico (las fotos, fuera) */

typedef struct {
	Contacto *c;
	int correo_pref;	/* el preferido ya está en el 0 */
	int tel_pref;
	int hay_fn;
	char n[128];		/* el de N (si no hay FN) */
	int algo;
} Leyendo;

/* el valor sin los escapes de la 3.0/4.0 (\\ \, \; \n); «estructurado»:
 * los «;» de verdad separan (se ponen como «sep») */
static void quita_escapes(const char *s, u32 n, char *d, int cabe, const char *sep_ptcoma)
{
	Texto t;
	u32 i;

	tx_ini(&t, 64u << 10);
	for (i = 0; i < n; i++) {
		char c = s[i];

		if (c == '\\' && i + 1 < n) {
			char x = s[++i];

			if (x == 'n' || x == 'N')
				tx_c(&t, '\n');
			else
				tx_c(&t, x);
			continue;
		}
		if (c == ';' && sep_ptcoma) {
			tx_s(&t, sep_ptcoma);
			continue;
		}
		if ((u8)c < 0x20 && c != '\n')
			c = ' ';
		tx_c(&t, c);
	}
	/* sin separadores vacíos al final ni al principio */
	if (sep_ptcoma && t.d) {
		u32 l = (u32)strlen(sep_ptcoma), a = 0;

		while (t.n >= l && !memcmp(t.d + t.n - l, sep_ptcoma, l))
			t.n -= l;
		while (a + l <= t.n && !memcmp(t.d + a, sep_ptcoma, l))
			a += l;
		cr_copia_en(d, cabe, t.d + a, t.n - a);
	} else
		cr_copia_en(d, cabe, t.d ? t.d : "", t.n);
	{
		/* sin blancos alrededor */
		u32 l = (u32)strlen(d), a = 0;

		while (l && es_espacio((u8)d[l - 1]))
			d[--l] = 0;
		while (a < l && es_espacio((u8)d[a]))
			a++;
		if (a)
			memmove(d, d + a, l - a + 1);
	}
	tx_suelta(&t);
}

/* un renglón lógico «grupo.NOMBRE;PARAM=X;...:valor» */
static void propiedad(Leyendo *L, const char *r, u32 n)
{
	Contacto *c = L->c;
	u32 i = 0, ni, nn, dp;
	int qp = 0, b64 = 0, pref = 0;
	char juego[48];
	const char *v;
	u32 vn;
	char *u = NULL;
	u8 *crudo = NULL;

	juego[0] = 0;
	/* los dos puntos (fuera de las comillas de los parámetros) */
	{
		int com = 0;

		for (dp = 0; dp < n; dp++) {
			if (r[dp] == '"')
				com = !com;
			else if (r[dp] == ':' && !com)
				break;
		}
	}
	if (dp >= n)
		return;
	/* el nombre (sin el grupo) */
	for (i = 0; i < dp && r[i] != ';'; i++)
		;
	ni = 0;
	nn = i;
	{
		const char *pt = memchr(r, '.', nn);

		if (pt) {
			ni = (u32)(pt - r) + 1;
			nn -= ni;
		}
	}
	/* los parámetros */
	while (i < dp) {
		u32 a = ++i, b, e;

		while (i < dp && r[i] != ';')
			i++;
		b = i;
		for (e = a; e < b && r[e] != '='; e++)
			;
		if (e < b) {
			const char *pv = r + e + 1;
			u32 pn = b - e - 1;

			if (pn >= 2 && pv[0] == '"' && pv[pn - 1] == '"') {
				pv++;
				pn -= 2;
			}
			if (cr_igual_n(r + a, e - a, "encoding")) {
				if (cr_igual_n(pv, pn, "quoted-printable"))
					qp = 1;
				else if (cr_igual_n(pv, pn, "b") || cr_igual_n(pv, pn, "base64"))
					b64 = 1;
			} else if (cr_igual_n(r + a, e - a, "charset"))
				cr_copia_en(juego, sizeof juego, pv, pn);
			else if (cr_igual_n(r + a, e - a, "type")) {
				/* TYPE=INTERNET,PREF o TYPE=pref */
				u32 k;

				for (k = 0; k + 4 <= pn; k++)
					if (cr_empieza(pv + k, pn - k, "pref") && (k == 0 || pv[k - 1] == ',') &&
					    (k + 4 == pn || pv[k + 4] == ','))
						pref = 1;
			} else if (cr_igual_n(r + a, e - a, "pref"))
				pref = 1;
		} else {
			/* los de la 2.1 sin nombre: QUOTED-PRINTABLE, PREF, BASE64 */
			if (cr_igual_n(r + a, b - a, "quoted-printable"))
				qp = 1;
			else if (cr_igual_n(r + a, b - a, "pref"))
				pref = 1;
			else if (cr_igual_n(r + a, b - a, "base64"))
				b64 = 1;
		}
	}
	if (b64)
		return;	/* fotos y llaves: no se guardan */
	v = r + dp + 1;
	vn = n - dp - 1;
	if (qp) {
		u32 l;

		crudo = qp_descodifica((const u8 *)v, vn, &l, 0);
		if (!crudo)
			return;
		v = (const char *)crudo;
		vn = l;
	}
	if (juego[0] || qp || !utf8_valido((const u8 *)v, vn)) {
		u32 l;

		u = jc_a_utf8(juego[0] ? juego : NULL, (const u8 *)v, vn, &l);
		if (!u) {
			so_suelta(crudo);
			return;
		}
		v = u;
		vn = l;
	}
	{
		const char *nm = r + ni;

		L->algo = 1;
		if (cr_igual_n(nm, nn, "fn")) {
			quita_escapes(v, vn, c->nombre, sizeof c->nombre, NULL);
			L->hay_fn = c->nombre[0] != 0;
		} else if (cr_igual_n(nm, nn, "n")) {
			/* Apellidos;Nombre;Otros;Tratamiento;Sufijo -> «Nombre Otros Apellidos» */
			char p[5][96];
			u32 k = 0, a = 0, x;
			int j;

			memset(p, 0, sizeof p);
			for (x = 0; x <= vn && k < 5; x++)
				if (x == vn || (v[x] == ';' && (x == 0 || v[x - 1] != '\\'))) {
					quita_escapes(v + a, x - a, p[k], sizeof p[k], NULL);
					k++;
					a = x + 1;
				}
			L->n[0] = 0;
			for (j = 0; j < 3; j++) {
				const char *s = p[j == 0 ? 1 : j == 1 ? 2 : 0];

				if (!s[0])
					continue;
				if (L->n[0] && strlen(L->n) + 1 < sizeof L->n)
					strcat(L->n, " ");
				cr_copia_en(L->n + strlen(L->n), (int)(sizeof L->n - strlen(L->n)), s, (u32)strlen(s));
			}
		} else if (cr_igual_n(nm, nn, "email")) {
			char e[128];
			int k;

			quita_escapes(v, vn, e, sizeof e, NULL);
			if (cr_empieza(e, (u32)strlen(e), "mailto:"))
				memmove(e, e + 7, strlen(e + 7) + 1);
			if (e[0]) {
				for (k = 0; k < VC_MAX_CORREOS && c->correo[k][0]; k++)
					;
				if (pref && !L->correo_pref) {
					/* el preferido, delante */
					int j = k < VC_MAX_CORREOS ? k : VC_MAX_CORREOS - 1;

					for (; j > 0; j--)
						memcpy(c->correo[j], c->correo[j - 1], sizeof c->correo[0]);
					cr_copia_en(c->correo[0], sizeof c->correo[0], e, (u32)strlen(e));
					L->correo_pref = 1;
				} else if (k < VC_MAX_CORREOS)
					cr_copia_en(c->correo[k], sizeof c->correo[0], e, (u32)strlen(e));
			}
		} else if (cr_igual_n(nm, nn, "tel")) {
			if (!c->telefono[0] || (pref && !L->tel_pref)) {
				char t[64];

				quita_escapes(v, vn, t, sizeof t, NULL);
				if (cr_empieza(t, (u32)strlen(t), "tel:"))
					memmove(t, t + 4, strlen(t + 4) + 1);
				cr_copia_en(c->telefono, sizeof c->telefono, t, (u32)strlen(t));
				if (pref)
					L->tel_pref = 1;
			}
		} else if (cr_igual_n(nm, nn, "org")) {
			quita_escapes(v, vn, c->organizacion, sizeof c->organizacion, ", ");
		} else if (cr_igual_n(nm, nn, "note")) {
			quita_escapes(v, vn, c->nota, sizeof c->nota, NULL);
		} else if (cr_igual_n(nm, nn, "x-hispa-usado")) {
			u32 k = 0;
			int hay;

			c->usado = cr_numero(v, vn, &k, &hay);
		}
	}
	so_suelta(u);
	so_suelta(crudo);
}

int vcard_lee(const u8 *d, u32 n, Contacto *v, int max)
{
	Texto r;
	u32 p = 0;
	int k = 0, hondo = 0;
	Leyendo L;

	if (max <= 0 || !d)
		return 0;
	memset(&L, 0, sizeof L);
	tx_ini(&r, TOPE_RENGLON);
	while (p < n && k < max) {
		u32 e, ef;
		int qp_sigue;

		/* un renglón lógico: con los partidos (un blanco delante) y los
		 * «=» del final de QUOTED-PRINTABLE */
		r.n = 0;
		r.malo = 0;
		for (;;) {
			const u8 *f = memchr(d + p, '\n', n - p);

			e = f ? (u32)(f - d) : n;
			ef = e;
			if (ef > p && d[ef - 1] == '\r')
				ef--;
			tx_pon(&r, d + p, ef - p);
			if (r.malo == 1)
				break;
			p = e < n ? e + 1 : n;
			qp_sigue = r.n && r.d[r.n - 1] == '=' && cr_busca_sin((const u8 *)r.d, r.n > 200 ? 200 : r.n, "quoted-printable", 16) >= 0;
			if (p < n && es_blanco(d[p])) {
				p++;	/* la 3.0: se quita el blanco */
				continue;
			}
			if (qp_sigue && p < n) {
				r.n--;	/* el «=» del salto blando */
				continue;
			}
			break;
		}
		if (r.malo == 1)
			break;
		if (!r.n)
			continue;
		if (cr_igual_n(r.d, r.n, "begin:vcard")) {
			if (hondo++ == 0) {
				memset(&v[k], 0, sizeof v[k]);
				memset(&L, 0, sizeof L);
				L.c = &v[k];
			}
			continue;
		}
		if (cr_igual_n(r.d, r.n, "end:vcard")) {
			if (hondo > 0 && --hondo == 0) {
				if (!L.hay_fn && L.n[0])
					cr_copia_en(v[k].nombre, sizeof v[k].nombre, L.n, (u32)strlen(L.n));
				if (!v[k].nombre[0] && v[k].correo[0][0])
					cr_copia_en(v[k].nombre, sizeof v[k].nombre, v[k].correo[0], (u32)strlen(v[k].correo[0]));
				if (v[k].nombre[0] || v[k].correo[0][0] || v[k].telefono[0])
					k++;
				L.c = NULL;
			}
			continue;
		}
		if (hondo == 1 && L.c)
			propiedad(&L, r.d, r.n);
	}
	/* sin END:VCARD al final */
	if (hondo > 0 && L.c && k < max) {
		if (!L.hay_fn && L.n[0])
			cr_copia_en(v[k].nombre, sizeof v[k].nombre, L.n, (u32)strlen(L.n));
		if (!v[k].nombre[0] && v[k].correo[0][0])
			cr_copia_en(v[k].nombre, sizeof v[k].nombre, v[k].correo[0], (u32)strlen(v[k].correo[0]));
		if (v[k].nombre[0] || v[k].correo[0][0] || v[k].telefono[0])
			k++;
	}
	tx_suelta(&r);
	return k;
}

/* ---- escribir ---- */
typedef struct {
	Texto t;
	u32 col;
} Escribe;

/* octetos de un renglón, partido a 75 sin partir una letra */
static void pon_partido(Escribe *w, const char *s, u32 n)
{
	u32 i = 0;

	while (i < n) {
		u32 l = 1;

		while (i + l < n && ((u8)s[i + l] & 0xC0) == 0x80 && l < 4)
			l++;
		if (w->col + l > 75) {
			tx_s(&w->t, "\r\n ");
			w->col = 1;
		}
		tx_pon(&w->t, s + i, l);
		w->col += l;
		i += l;
	}
}

/* lo de un campo fijo (que puede no acabar en 0) */
static u32 largo_fijo(const char *s, u32 cabe)
{
	const char *z = memchr(s, 0, cabe);

	return z ? (u32)(z - s) : cabe;
}

/* el valor con los escapes de la 3.0 (\\ \, \; \n) */
static void escapa(Texto *x, const char *s, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		char c = s[i];

		if (c == '\\' || c == ',' || c == ';') {
			tx_c(x, '\\');
			tx_c(x, c);
		} else if (c == '\n')
			tx_s(x, "\\n");
		else if (c == '\r' || ((u8)c < 0x20 && c != '\t') || c == 0x7f)
			continue;
		else
			tx_c(x, c);
	}
}

/* NOMBRE:valor (ya escapado), partido */
static void renglon(Escribe *w, const char *nombre, Texto *x)
{
	tx_s(&w->t, nombre);
	w->col = (u32)strlen(nombre);
	tx_c(&w->t, ':');
	w->col++;
	if (x->malo == 1)
		w->t.malo = 1;
	else if (x->d)
		pon_partido(w, x->d, x->n);
	tx_s(&w->t, "\r\n");
	w->col = 0;
	x->n = 0;
}

static void campo(Escribe *w, Texto *x, const char *nombre, const char *s, u32 cabe)
{
	u32 n = largo_fijo(s, cabe);

	if (!n)
		return;
	escapa(x, s, n);
	renglon(w, nombre, x);
}

char *vcard_escribe(const Contacto *v, int n, u32 *largo)
{
	Escribe w;
	Texto x;
	int k, j;

	tx_ini(&w.t, 0);
	tx_ini(&x, 0);
	w.col = 0;
	for (k = 0; k < n; k++) {
		const Contacto *c = &v[k];
		u32 nn = largo_fijo(c->nombre, sizeof c->nombre), sp;

		while (nn && es_espacio((u8)c->nombre[nn - 1]))
			nn--;
		tx_s(&w.t, "BEGIN:VCARD\r\nVERSION:3.0\r\n");
		escapa(&x, c->nombre, nn);
		renglon(&w, "FN", &x);
		/* N: Apellidos;Nombre (lo primero es el nombre; lo demás, los apellidos) */
		for (sp = 0; sp < nn && c->nombre[sp] != ' '; sp++)
			;
		if (sp < nn) {
			u32 a = sp;

			while (a < nn && c->nombre[a] == ' ')
				a++;
			escapa(&x, c->nombre + a, nn - a);
			tx_c(&x, ';');
			escapa(&x, c->nombre, sp);
		} else {
			tx_c(&x, ';');
			escapa(&x, c->nombre, nn);
		}
		tx_s(&x, ";;;");
		renglon(&w, "N", &x);
		for (j = 0; j < VC_MAX_CORREOS; j++)
			campo(&w, &x, j == 0 ? "EMAIL;TYPE=INTERNET,PREF" : "EMAIL;TYPE=INTERNET", c->correo[j], sizeof c->correo[j]);
		campo(&w, &x, "TEL", c->telefono, sizeof c->telefono);
		campo(&w, &x, "ORG", c->organizacion, sizeof c->organizacion);
		campo(&w, &x, "NOTE", c->nota, sizeof c->nota);
		if (c->usado) {
			tx_fmt(&x, "%u", (unsigned)c->usado);
			renglon(&w, "X-HISPA-USADO", &x);
		}
		tx_s(&w.t, "END:VCARD\r\n");
	}
	tx_suelta(&x);
	return tx_fin(&w.t, largo);
}
