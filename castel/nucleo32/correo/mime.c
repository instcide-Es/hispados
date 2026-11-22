/* mime.c - Correo: los mensajes por dentro. base64 y quoted-printable, las
 * cabeceras de RFC 2047, los parámetros de RFC 2231, las direcciones y las
 * fechas de RFC 5322, el analizador de MIME (el árbol de partes, sin
 * recursión: una pila de MIME_MAX_NIVEL fronteras, en una sola pasada), lo
 * que se enseña y lo que es adjunto, y la suplantación sencilla.
 * Todo lo que llega es hostil: nada se lee fuera de [d, d+n), todo con tope,
 * en tiempo lineal.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "mime.h"
#include "html.h"
#include "interno.h"

#define TOPE_CABECERA (8u << 20)	/* lo más que da una cabecera descodificada */
#define TOPE_PARAM (64u << 10)		/* un parámetro */
#define MAX_TROZOS 256			/* trozos de RFC 2231 (*0 *1 ...) */
#define MAX_CAB_TOTAL 65536		/* cabeceras de todo el mensaje */

/* =================== base64 y quoted-printable =================== */
static int val64(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 26;
	if (c >= '0' && c <= '9')
		return c - '0' + 52;
	if (c == '+' || c == '-')
		return 62;
	if (c == '/' || c == '_')
		return 63;
	return -1;
}

u8 *b64_descodifica(const u8 *d, u32 n, u32 *largo)
{
	u8 *o = so_pide(n / 4 * 3 + 4);
	u32 i, k = 0, acc = 0;
	int bits = 0;

	if (!o) {
		if (largo)
			*largo = 0;
		return NULL;
	}
	for (i = 0; i < n; i++) {
		int v;

		if (d[i] == '=') {	/* el final de un cuarteto: lo que sobre, fuera */
			acc = 0;
			bits = 0;
			continue;
		}
		v = val64(d[i]);
		if (v < 0)
			continue;
		acc = (acc << 6) | (u32)v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			o[k++] = (u8)(acc >> bits);
			acc &= (1u << bits) - 1;
		}
	}
	o[k] = 0;
	if (largo)
		*largo = k;
	return o;
}

u8 *qp_descodifica(const u8 *d, u32 n, u32 *largo, int en_cabecera)
{
	u8 *o = so_pide(n + 1);
	u32 i = 0, k = 0, bueno = 0;

	if (!o) {
		if (largo)
			*largo = 0;
		return NULL;
	}
	while (i < n) {
		u8 c = d[i];

		if (c == '=') {
			int a = i + 1 < n ? valor_hex(d[i + 1]) : -1;
			int b = i + 2 < n ? valor_hex(d[i + 2]) : -1;
			u32 j;

			if (a >= 0 && b >= 0) {
				o[k++] = (u8)(a * 16 + b);
				bueno = k;
				i += 3;
				continue;
			}
			/* ¿un salto blando? («=», espacios y el final del renglón) */
			for (j = i + 1; j < n && es_blanco(d[j]); j++)
				;
			if (j >= n) {
				i = n;
				break;
			}
			if (d[j] == '\n') {
				i = j + 1;
				continue;
			}
			if (d[j] == '\r' && (j + 1 >= n || d[j + 1] == '\n')) {
				i = j + 2;
				continue;
			}
			o[k++] = '=';	/* un «=» suelto: tal cual */
			bueno = k;
			i++;
			continue;
		}
		if (c == '\n' && !en_cabecera) {
			/* los blancos del final del renglón no cuentan */
			int cr = k > bueno && o[k - 1] == '\r';

			k = bueno;
			if (cr)
				o[k++] = '\r';
			o[k++] = '\n';
			bueno = k;
			i++;
			continue;
		}
		if (c == '_' && en_cabecera)
			c = ' ';
		o[k++] = c;
		if (!es_blanco(c) && c != '\r')
			bueno = k;
		i++;
	}
	o[k] = 0;
	if (largo)
		*largo = k;
	return o;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *b64_codifica(const u8 *d, u32 n, u32 *largo, int con_renglones)
{
	Texto t;
	u32 i, col = 0;

	tx_ini(&t, 0);
	if (n > 0x30000000u) {
		if (largo)
			*largo = 0;
		return NULL;
	}
	for (i = 0; i < n; i += 3) {
		u32 v = (u32)d[i] << 16;
		char q[4];

		if (i + 1 < n)
			v |= (u32)d[i + 1] << 8;
		if (i + 2 < n)
			v |= d[i + 2];
		q[0] = B64[(v >> 18) & 63];
		q[1] = B64[(v >> 12) & 63];
		q[2] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
		q[3] = i + 2 < n ? B64[v & 63] : '=';
		tx_pon(&t, q, 4);
		col += 4;
		if (con_renglones && col >= 76 && i + 3 < n) {
			tx_pon(&t, "\r\n", 2);
			col = 0;
		}
	}
	if (con_renglones && n)
		tx_pon(&t, "\r\n", 2);
	return tx_fin(&t, largo);
}

char *qp_codifica(const u8 *d, u32 n, u32 *largo)
{
	Texto t;
	u32 i, col = 0;

	tx_ini(&t, 0);
	for (i = 0; i < n; i++) {
		u8 c = d[i];
		char q[4];
		u32 l;
		int fin_renglon;

		if (c == '\r' && i + 1 < n && d[i + 1] == '\n')
			continue;
		if (c == '\n') {
			tx_pon(&t, "\r\n", 2);
			col = 0;
			continue;
		}
		fin_renglon = i + 1 >= n || d[i + 1] == '\n' || (d[i + 1] == '\r' && i + 2 < n && d[i + 2] == '\n');
		if ((c >= 33 && c <= 126 && c != '=') || (es_blanco(c) && !fin_renglon)) {
			/* «From » y el punto solo al principio del renglón: mejor codificados */
			if (col == 0 && ((c == 'F' && i + 4 < n && !memcmp(d + i, "From ", 5)) || (c == '.')))
				goto codificado;
			q[0] = (char)c;
			l = 1;
		} else {
codificado:
			q[0] = '=';
			q[1] = "0123456789ABCDEF"[c >> 4];
			q[2] = "0123456789ABCDEF"[c & 15];
			l = 3;
		}
		if (col + l > 75) {
			tx_pon(&t, "=\r\n", 3);
			col = 0;
		}
		tx_pon(&t, q, l);
		col += l;
	}
	return tx_fin(&t, largo);
}

/* =================== RFC 2047 =================== */
/* lo que no es una palabra codificada: sin los saltos de los renglones
 * partidos, y los 8 bits como UTF-8 si lo son (si no, 1252) */
static void pon_plano(Texto *o, const char *t, u32 n)
{
	u32 i, ocho = 0;

	for (i = 0; i < n; i++)
		if ((u8)t[i] >= 0x80) {
			ocho = 1;
			break;
		}
	if (!ocho) {
		for (i = 0; i < n; i++) {
			u8 c = (u8)t[i];

			if (c == '\r' || c == '\n')
				continue;
			tx_c(o, c < 0x20 || c == 0x7f ? ' ' : c);
		}
		return;
	} else {
		u32 l;
		char *u = jc_a_utf8(NULL, (const u8 *)t, n, &l);

		if (!u) {
			o->malo = 1;
			return;
		}
		for (i = 0; i < l; i++) {
			u8 c = (u8)u[i];

			if (c == '\r' || c == '\n')
				continue;
			tx_c(o, c < 0x20 || c == 0x7f ? ' ' : c);
		}
		so_suelta(u);
	}
}

static void vacia_pendiente(Texto *o, Texto *crudo, const char *juego)
{
	u32 l, i;
	char *u;

	if (!crudo->n)
		return;
	u = jc_a_utf8(juego, (const u8 *)crudo->d, crudo->n, &l);
	crudo->n = 0;
	if (!u) {
		o->malo = 1;
		return;
	}
	for (i = 0; i < l; i++) {
		u8 c = (u8)u[i];

		/* nada de saltos de renglón ni controles metidos en una cabecera */
		tx_c(o, c < 0x20 || c == 0x7f ? ' ' : c);
	}
	so_suelta(u);
}

char *rfc2047_descodifica(const char *t, u32 n)
{
	Texto o, crudo;
	char juego[48];
	u32 i = 0, a = 0;
	u32 cache_ini = 0xFFFFFFFFu, cache_pos = 0;
	int antes_palabra = 0;

	tx_ini(&o, TOPE_CABECERA);
	tx_ini(&crudo, TOPE_CABECERA);
	juego[0] = 0;
	while (i + 1 < n) {
		u32 j, s, cierre, k;
		int cod;

		if (t[i] != '=' || t[i + 1] != '?') {
			i++;
			continue;
		}
		/* =?juego?C?texto?= */
		for (j = i + 2; j < n && j - (i + 2) < 40 && t[j] != '?' && !es_espacio((u8)t[j]); j++)
			;
		if (j >= n || t[j] != '?' || j == i + 2 || j + 2 >= n || t[j + 2] != '?') {
			i++;
			continue;
		}
		cod = minuscula((u8)t[j + 1]);
		if (cod != 'b' && cod != 'q') {
			i++;
			continue;
		}
		s = j + 3;
		/* el «?=» siguiente (con lo ya buscado, para no volver a mirar) */
		if (cache_ini != 0xFFFFFFFFu && s >= cache_ini && cache_pos >= s)
			cierre = cache_pos;
		else {
			u32 hasta = (cache_ini != 0xFFFFFFFFu && s < cache_ini) ? cache_ini + 1 : n;

			if (hasta > n)
				hasta = n;
			for (k = s; k + 1 < hasta; k++)
				if (t[k] == '?' && t[k + 1] == '=')
					break;
			if (k + 1 < hasta)
				cierre = k;
			else if (hasta < n)
				cierre = cache_pos;
			else
				cierre = n;
			cache_ini = s;
			cache_pos = cierre;
		}
		if (cierre >= n || cierre - s > 4096) {
			i++;
			continue;
		}
		/* lo de antes: si entre dos palabras solo hay blancos, no cuenta */
		{
			int solo_blancos = 1;

			for (k = a; k < i; k++)
				if (!es_espacio((u8)t[k])) {
					solo_blancos = 0;
					break;
				}
			if (!(antes_palabra && solo_blancos)) {
				vacia_pendiente(&o, &crudo, juego);
				pon_plano(&o, t + a, i - a);
			}
		}
		{
			char jn[48];
			u32 l;
			u8 *b;

			cr_copia_en(jn, sizeof jn, t + i + 2, j - (i + 2));
			if (crudo.n && !cr_igual(jn, juego))
				vacia_pendiente(&o, &crudo, juego);
			memcpy(juego, jn, sizeof juego);
			if (cod == 'b')
				b = b64_descodifica((const u8 *)t + s, cierre - s, &l);
			else
				b = qp_descodifica((const u8 *)t + s, cierre - s, &l, 1);
			if (!b) {
				o.malo = 1;
				break;
			}
			tx_pon(&crudo, b, l);
			so_suelta(b);
		}
		i = cierre + 2;
		a = i;
		antes_palabra = 1;
	}
	vacia_pendiente(&o, &crudo, juego);
	if (a < n)
		pon_plano(&o, t + a, n - a);
	tx_suelta(&crudo);
	return tx_fin(&o, NULL);
}

static int es_especial(int c)
{
	return c == '(' || c == ')' || c == '<' || c == '>' || c == '@' || c == ',' || c == ';' ||
	       c == ':' || c == '\\' || c == '"' || c == '.' || c == '[' || c == ']';
}

char *rfc2047_codifica(const char *utf8, u32 col, int es_frase)
{
	Texto o;
	u32 n = (u32)strlen(utf8), i, ocho = 0, especiales = 0, seguros = 0;
	int b64;
	u32 sitio;

	tx_ini(&o, TOPE_CABECERA);
	for (i = 0; i < n; i++) {
		u8 c = (u8)utf8[i];

		if (c >= 0x80 || c < 0x20 || c == 0x7f)
			ocho++;
		else if (es_alnum(c) || c == ' ')
			seguros++;
		if (c < 0x80 && es_especial(c))
			especiales++;
		if (c == '=' && i + 1 < n && utf8[i + 1] == '?')
			ocho++;
	}
	if (!ocho) {
		if (es_frase && especiales) {
			tx_c(&o, '"');
			for (i = 0; i < n; i++) {
				if (utf8[i] == '"' || utf8[i] == '\\')
					tx_c(&o, '\\');
				tx_c(&o, utf8[i]);
			}
			tx_c(&o, '"');
			return tx_fin(&o, NULL);
		}
		/* partido en los blancos, a 76 si se puede */
		{
			u32 ini = 0;

			while (ini < n) {
				u32 cabe = col < 70 ? 76 - col : 6, corte = n;

				if (n - ini > cabe) {
					u32 k;

					corte = n;
					for (k = ini + cabe; k > ini; k--)
						if (utf8[k] == ' ')
							break;
					if (k > ini)
						corte = k;
					else {
						for (k = ini + cabe; k < n && utf8[k] != ' '; k++)
							;
						corte = k;
					}
				}
				tx_pon(&o, utf8 + ini, corte - ini);
				if (corte >= n)
					break;
				tx_pon(&o, "\r\n", 2);	/* el blanco va en el renglón nuevo */
				ini = corte;
				col = 0;
			}
		}
		return tx_fin(&o, NULL);
	}
	/* con palabras codificadas: Q si casi todo es ASCII, si no B */
	b64 = seguros * 2 < n;
	sitio = col < 50 ? 75 - col : 25;
	i = 0;
	while (i < n) {
		u32 usado = 12, cuantos = 0;

		tx_s(&o, b64 ? "=?UTF-8?B?" : "=?UTF-8?Q?");
		if (b64) {
			u32 ini = i, lim = (sitio - 12) / 4 * 3;

			while (i < n) {
				u32 k = i + 1;

				while (k < n && ((u8)utf8[k] & 0xC0) == 0x80)
					k++;
				if (k - ini > lim && i > ini)
					break;
				i = k;
			}
			{
				u32 l;
				char *b = b64_codifica((const u8 *)utf8 + ini, i - ini, &l, 0);

				if (b) {
					tx_pon(&o, b, l);
					so_suelta(b);
				} else
					o.malo = 1;
			}
		} else {
			while (i < n) {
				u32 k = i + 1, l = 0, x;
				char q[16];

				while (k < n && ((u8)utf8[k] & 0xC0) == 0x80 && k - i < 4)
					k++;
				for (x = i; x < k; x++) {
					u8 c = (u8)utf8[x];

					if (c == ' ')
						q[l++] = '_';
					else if (es_alnum(c) || c == '!' || c == '*' || c == '+' || c == '-' || c == '/' ||
						 (!es_frase && c > 32 && c < 127 && c != '?' && c != '=' && c != '_' && !es_especial(c)))
						q[l++] = (char)c;
					else {
						q[l++] = '=';
						q[l++] = "0123456789ABCDEF"[c >> 4];
						q[l++] = "0123456789ABCDEF"[c & 15];
					}
				}
				if (usado + l > sitio && cuantos)
					break;
				tx_pon(&o, q, l);
				usado += l;
				cuantos++;
				i = k;
			}
		}
		tx_s(&o, "?=");
		if (i < n)
			tx_s(&o, "\r\n ");
		sitio = 74;
	}
	return tx_fin(&o, NULL);
}

/* =================== los valores con parámetros =================== */
/* salta un comentario (…) con su anidado (sin recursión); da lo de después */
static u32 salta_comentario(const char *c, u32 n, u32 i)
{
	u32 hondo = 0;

	for (; i < n; i++) {
		if (c[i] == '\\') {
			i++;
			continue;
		}
		if (c[i] == '(')
			hondo++;
		else if (c[i] == ')' && hondo && !--hondo)
			return i + 1;
	}
	return n;
}

/* lo de detrás del valor de delante (el primer «;» fuera de comillas) */
static u32 tras_valor(const char *c, u32 n)
{
	u32 i = 0;
	int com = 0;

	for (; i < n; i++) {
		if (com) {
			if (c[i] == '\\')
				i++;
			else if (c[i] == '"')
				com = 0;
			continue;
		}
		if (c[i] == '"')
			com = 1;
		else if (c[i] == ';')
			return i;
	}
	return n;
}

char *mime_valor(const char *c, u32 n)
{
	Texto t;
	u32 i, f = tras_valor(c, n);

	tx_ini(&t, 256);
	for (i = 0; i < f;) {
		u8 x = (u8)c[i];

		if (x == '(') {
			i = salta_comentario(c, f, i);
			continue;
		}
		if (es_espacio(x) || x == '"') {
			i++;
			continue;
		}
		if (x < 0x20)
			break;
		tx_c(&t, minuscula(x));
		i++;
	}
	if (t.malo == 2)
		t.malo = 0;
	return tx_fin(&t, NULL);
}

typedef struct {
	u32 ni, nn;	/* el nombre */
	u32 vi, vn;	/* el valor (crudo; con las comillas quitadas) */
	int comillas;
} Param;

/* el parámetro siguiente desde *i; 0 si no hay más */
static int sig_param(const char *c, u32 n, u32 *pi, Param *p)
{
	u32 i = *pi;

	for (;;) {
		while (i < n && (es_espacio((u8)c[i]) || c[i] == ';'))
			i++;
		if (i < n && c[i] == '(') {
			i = salta_comentario(c, n, i);
			continue;
		}
		break;
	}
	if (i >= n) {
		*pi = n;
		return 0;
	}
	p->ni = i;
	while (i < n && c[i] != '=' && c[i] != ';' && !es_espacio((u8)c[i]))
		i++;
	p->nn = i - p->ni;
	while (i < n && es_espacio((u8)c[i]))
		i++;
	p->vi = i;
	p->vn = 0;
	p->comillas = 0;
	if (i >= n || c[i] != '=') {
		/* sin valor: lo de después de los blancos puede ser otro */
		if (i < n && c[i] != ';' && p->nn == 0)
			i++;
		*pi = i;
		return 1;
	}
	i++;
	while (i < n && es_espacio((u8)c[i]))
		i++;
	if (i < n && c[i] == '"') {
		i++;
		p->vi = i;
		while (i < n && c[i] != '"') {
			if (c[i] == '\\')
				i++;
			i++;
		}
		if (i > n)
			i = n;
		p->vn = i - p->vi;
		p->comillas = 1;
		if (i < n)
			i++;
	} else {
		u32 f;

		p->vi = i;
		while (i < n && c[i] != ';')
			i++;
		f = i;
		while (f > p->vi && es_espacio((u8)c[f - 1]))
			f--;
		p->vn = f - p->vi;
	}
	*pi = i;
	return 1;
}

/* el valor de un parámetro sin las «\» y sin los saltos de renglón */
static void pon_valor(Texto *t, const char *c, const Param *p)
{
	u32 i;

	for (i = 0; i < p->vn; i++) {
		char x = c[p->vi + i];

		if (x == '\r' || x == '\n')
			continue;
		/* «\» solo quita su valor delante de «"» y «\» (Outlook pone las
		 * rutas de Windows tal cual) */
		if (p->comillas && x == '\\' && i + 1 < p->vn && (c[p->vi + i + 1] == '"' || c[p->vi + i + 1] == '\\'))
			x = c[p->vi + ++i];
		tx_c(t, x);
	}
}

static void pon_porcientos(Texto *t, const char *s, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		int a, b;

		if (s[i] == '%' && i + 2 < n && (a = valor_hex((u8)s[i + 1])) >= 0 && (b = valor_hex((u8)s[i + 2])) >= 0) {
			tx_c(t, a * 16 + b);
			i += 2;
		} else
			tx_c(t, s[i]);
	}
}

char *mime_parametro(const char *c, u32 n, const char *nombre)
{
	struct {
		u32 vi, vn;
		u8 hay, cod, comillas;
	} trozo[MAX_TROZOS];
	u32 i = tras_valor(c, n), nl = (u32)strlen(nombre), k;
	Param p, llano;
	int hay_llano = 0, hay_trozos = 0;
	Texto t;
	char juego[48];
	char *r;

	memset(trozo, 0, sizeof trozo);
	juego[0] = 0;
	while (sig_param(c, n, &i, &p)) {
		const char *nm = c + p.ni;

		if (p.nn < nl || !cr_empieza(nm, p.nn, nombre))
			continue;
		if (p.nn == nl) {
			if (!hay_llano) {
				llano = p;
				hay_llano = 1;
			}
			continue;
		}
		if (nm[nl] != '*')
			continue;
		/* nombre* (todo de una vez) o nombre*N o nombre*N* */
		{
			u32 j = nl + 1, idx = 0;
			int cod = 0, hay;

			if (j == p.nn) {
				idx = 0;
				cod = 1;
			} else {
				idx = cr_numero(nm, p.nn, &j, &hay);
				if (!hay)
					continue;
				if (j < p.nn && nm[j] == '*') {
					cod = 1;
					j++;
				}
				if (j != p.nn)
					continue;
			}
			if (idx >= MAX_TROZOS || trozo[idx].hay)
				continue;
			trozo[idx].hay = 1;
			trozo[idx].cod = (u8)cod;
			trozo[idx].comillas = (u8)p.comillas;
			trozo[idx].vi = p.vi;
			trozo[idx].vn = p.vn;
			hay_trozos = 1;
		}
	}
	tx_ini(&t, TOPE_PARAM);
	if (hay_trozos && trozo[0].hay) {
		for (k = 0; k < MAX_TROZOS && trozo[k].hay; k++) {
			Param q;

			q.vi = trozo[k].vi;
			q.vn = trozo[k].vn;
			q.comillas = trozo[k].comillas;
			if (!trozo[k].cod) {
				pon_valor(&t, c, &q);
				continue;
			}
			{
				const char *s = c + q.vi;
				u32 sn = q.vn;

				if (k == 0) {
					/* juego'lenguaje'texto */
					const char *a = memchr(s, '\'', sn);
					const char *b = a ? memchr(a + 1, '\'', sn - (u32)(a + 1 - s)) : NULL;

					if (a && b) {
						cr_copia_en(juego, sizeof juego, s, (u32)(a - s));
						sn -= (u32)(b + 1 - s);
						s = b + 1;
					}
				}
				pon_porcientos(&t, s, sn);
			}
		}
		if (t.malo == 1)
			return tx_fin(&t, NULL);
		{
			u32 l;
			char *u = jc_a_utf8(juego[0] ? juego : NULL, (const u8 *)(t.d ? t.d : ""), t.n, &l);

			tx_suelta(&t);
			if (u && !juego[0] && cr_busca((const u8 *)u, l, "=?", 2) >= 0) {
				char *v = rfc2047_descodifica(u, l);

				so_suelta(u);
				return v;
			}
			return u;
		}
	}
	if (!hay_llano) {
		tx_suelta(&t);
		return NULL;
	}
	pon_valor(&t, c, &llano);
	if (t.malo == 1)
		return tx_fin(&t, NULL);
	r = rfc2047_descodifica(t.d ? t.d : "", t.n);
	tx_suelta(&t);
	return r;
}

/* =================== las direcciones =================== */
static void recorta(Texto *t)
{
	u32 a = 0;

	while (t->n && es_espacio((u8)t->d[t->n - 1]))
		t->n--;
	while (a < t->n && es_espacio((u8)t->d[a]))
		a++;
	if (a) {
		memmove(t->d, t->d + a, t->n - a);
		t->n -= a;
	}
}

static void termina_dir(Direccion *v, Texto *frase, Texto *coment, Texto *spec, Texto *ang, int angulo)
{
	char *nom;
	const char *dir;
	u32 nd;
	Texto *tn;

	v->nombre[0] = v->dir[0] = 0;
	recorta(frase);
	recorta(coment);
	if (angulo) {
		const char *a = ang->d ? ang->d : "";
		u32 na = ang->n;

		/* sin la ruta de antes («@a,@b:buzon@dominio») ni «mailto:» */
		if (na && a[0] == '@') {
			const char *dp = memchr(a, ':', na);

			if (dp) {
				na -= (u32)(dp + 1 - a);
				a = dp + 1;
			}
		}
		if (cr_empieza(a, na, "mailto:")) {
			a += 7;
			na -= 7;
		}
		dir = a;
		nd = na;
		tn = frase->n ? frase : coment;
	} else {
		dir = spec->d ? spec->d : "";
		nd = spec->n;
		tn = coment;
		if (!memchr(dir, '@', nd)) {
			/* sin dirección: solo un nombre */
			tn = frase;
			nd = 0;
		}
	}
	cr_copia_en(v->dir, sizeof v->dir, dir, nd);
	if (tn->n) {
		nom = rfc2047_descodifica(tn->d, tn->n);
		if (nom) {
			u32 l = (u32)strlen(nom), a = 0;

			while (l && es_espacio((u8)nom[l - 1]))
				l--;
			while (a < l && es_espacio((u8)nom[a]))
				a++;
			/* las comillas que quedan (de «'Ana'» o de lo mal hecho) */
			if (l - a >= 2 && ((nom[a] == '\'' && nom[l - 1] == '\'') || (nom[a] == '"' && nom[l - 1] == '"'))) {
				a++;
				l--;
			}
			cr_copia_en(v->nombre, sizeof v->nombre, nom + a, l - a);
			so_suelta(nom);
		}
	}
	frase->n = coment->n = spec->n = ang->n = 0;
}

int mime_direcciones(const char *v, u32 n, Direccion *out, int max)
{
	Texto frase, coment, spec, ang;
	u32 i = 0;
	int k = 0, angulo = 0, hay = 0;

	if (max <= 0)
		return 0;
	tx_ini(&frase, 4096);
	tx_ini(&coment, 4096);
	tx_ini(&spec, 4096);
	tx_ini(&ang, 4096);
	while (i <= n && k < max) {
		u8 c = i < n ? (u8)v[i] : ',';

		if (c == ',' || c == ';') {
			if (hay) {
				termina_dir(&out[k], &frase, &coment, &spec, &ang, angulo);
				if (out[k].dir[0] || out[k].nombre[0])
					k++;
			}
			frase.n = coment.n = spec.n = ang.n = 0;
			angulo = hay = 0;
			i++;
			continue;
		}
		if (c == '"' && memchr(v + i + 1, '"', n - i - 1)) {
			u32 j = i + 1;

			hay = 1;
			tx_c(&spec, '"');
			for (; j < n && v[j] != '"'; j++) {
				if (v[j] == '\\' && j + 1 < n)
					j++;
				if (v[j] == '\r' || v[j] == '\n')
					continue;
				tx_c(&frase, v[j]);
				tx_c(&spec, v[j]);
			}
			tx_c(&spec, '"');
			i = j + 1;
			continue;
		}
		if (c == '(') {
			u32 f = salta_comentario(v, n, i);

			{
				/* el último comentario (el nombre de «a@b (Ana)») */
				u32 a = i + 1, b = f;

				if (b > a && b <= n && v[b - 1] == ')')
					b--;
				coment.n = 0;
				tx_pon(&coment, v + a, b > a ? b - a : 0);
			}
			if (frase.n && frase.d[frase.n - 1] != ' ')
				tx_c(&frase, ' ');
			i = f;
			continue;
		}
		if (c == '<') {
			u32 j = i + 1;

			hay = 1;
			ang.n = 0;
			/* (sin cerrar: hasta la coma, salvo en la ruta «<@a,@b:x@y>») */
			for (; j < n && v[j] != '>' && v[j] != '<' && (v[j] != ',' || (ang.n && ang.d[0] == '@' && !memchr(ang.d, ':', ang.n))); j++)
				if (!es_espacio((u8)v[j]) && v[j] != '"')
					tx_c(&ang, v[j]);
			angulo = 1;
			i = j < n && v[j] == '>' ? j + 1 : j;
			continue;
		}
		if (c == ':' && !angulo) {
			/* un grupo («amigos: a@b, c@d;»): el nombre no cuenta */
			frase.n = coment.n = spec.n = 0;
			hay = 0;
			i++;
			continue;
		}
		if (es_espacio(c)) {
			if (frase.n && frase.d[frase.n - 1] != ' ')
				tx_c(&frase, ' ');
			i++;
			continue;
		}
		hay = 1;
		tx_c(&frase, c);
		if (c >= 0x20)
			tx_c(&spec, c);
		i++;
	}
	tx_suelta(&frase);
	tx_suelta(&coment);
	tx_suelta(&spec);
	tx_suelta(&ang);
	return k;
}

int mime_dir_valida(const char *dir)
{
	const char *arroba = strrchr(dir, '@'), *p;
	u32 nl, nd, etiqueta = 0, tld = 0;
	int letras_tld = 1;

	if (!arroba)
		return 0;
	nl = (u32)(arroba - dir);
	nd = (u32)strlen(arroba + 1);
	if (!nl || nl > 64 || nd < 3 || nd > 253)
		return 0;
	for (p = dir; p < arroba; p++) {
		u8 c = (u8)*p;

		if (c <= 32 || c == 127 || c == '<' || c == '>' || c == '(' || c == ')' || c == ',' || c == ';' ||
		    c == ':' || c == '\\' || c == '[' || c == ']' || c == '@')
			return 0;
	}
	if (dir[0] == '.' || arroba[-1] == '.')
		return 0;
	for (p = arroba + 1; *p; p++) {
		u8 c = (u8)*p;

		if (c == '.') {
			if (!etiqueta)
				return 0;
			etiqueta = 0;
			tld = 0;
			letras_tld = 1;
			continue;
		}
		if (!(es_alnum(c) || c == '-' || c >= 0x80))
			return 0;
		if (!es_letra(c) && c < 0x80)
			letras_tld = 0;
		etiqueta++;
		tld++;
	}
	if (!etiqueta || !strchr(arroba + 1, '.'))
		return 0;
	return tld >= 2 && (letras_tld || cr_empieza(arroba + 1 + nd - tld, tld, "xn--"));
}

char *mime_dir_cabecera(const Direccion *d)
{
	Texto t;
	char *nom;

	tx_ini(&t, 0);
	if (!d->nombre[0]) {
		tx_s(&t, d->dir);
		return tx_fin(&t, NULL);
	}
	nom = rfc2047_codifica(d->nombre, 6, 1);
	if (!nom)
		return NULL;
	tx_s(&t, nom);
	so_suelta(nom);
	if (!d->dir[0])
		return tx_fin(&t, NULL);
	tx_s(&t, " <");
	tx_s(&t, d->dir);
	tx_c(&t, '>');
	return tx_fin(&t, NULL);
}

/* =================== las fechas =================== */
static i32 dias_de_fecha(i32 y, u32 m, u32 d)
{
	/* (días desde 1970-01-01; H. Hinnant, sin divisiones de 64 bits) */
	i32 era, yoe, doy, doe;

	y -= m <= 2;
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = y - era * 400;
	doy = (i32)((153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1);
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}

static void fecha_de_dias(i32 z, i32 *py, u32 *pm, u32 *pd)
{
	i32 era, doe, yoe, y, doy, mp;

	z += 719468;
	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = yoe + era * 400;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	*pd = (u32)(doy - (153 * mp + 2) / 5 + 1);
	*pm = (u32)(mp < 10 ? mp + 3 : mp - 9);
	*py = y + (*pm <= 2);
}

static const char *const MESES[12] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
static const char *const MESES_ES[12] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};

static int zona_nombre(const char *w, u32 n, int *z)
{
	static const struct {
		const char *n;
		int z;
	} zs[] = {{"ut", 0}, {"utc", 0}, {"gmt", 0}, {"z", 0}, {"est", -300}, {"edt", -240}, {"cst", -360}, {"cdt", -300},
		  {"mst", -420}, {"mdt", -360}, {"pst", -480}, {"pdt", -420}, {"cet", 60}, {"cest", 120}, {"met", 60},
		  {"mest", 120}, {"bst", 60}, {"eet", 120}, {"eest", 180}, {"wet", 0}, {"west", 60}, {"jst", 540}};
	u32 k;

	for (k = 0; k < sizeof zs / sizeof zs[0]; k++)
		if (cr_igual_n(w, n, zs[k].n)) {
			*z = zs[k].z;
			return 1;
		}
	if (n == 1 && es_letra((u8)w[0]) && minuscula((u8)w[0]) != 'j') {
		*z = 0;	/* las militares: como -0000 (RFC 5322) */
		return 1;
	}
	return 0;
}

i64 mime_fecha(const char *t, u32 n)
{
	i32 dia = -1, mes = -1, anio = -1, h = -1, mi = 0, s = 0, zona = 0;
	int hay_zona = 0;
	u32 i = 0;

	if (n > 300)
		n = 300;
	while (i < n && es_espacio((u8)t[i]))
		i++;
	/* ISO 8601: 2026-11-22T10:05:00Z */
	if (i + 10 <= n && es_cifra((u8)t[i]) && es_cifra((u8)t[i + 3]) && t[i + 4] == '-' && t[i + 7] == '-') {
		u32 j = i;
		int ok;

		anio = (i32)cr_numero(t, n, &j, &ok);
		j++;
		mes = (i32)cr_numero(t, n, &j, &ok) - 1;
		if (j < n && t[j] == '-')
			j++;
		dia = (i32)cr_numero(t, n, &j, &ok);
		i = j;
		if (i < n && (t[i] == 'T' || t[i] == 't' || t[i] == ' '))
			i++;
	}
	while (i < n) {
		u8 c = (u8)t[i];

		if (es_cifra(c)) {
			u32 a = i, v;
			int ok;

			v = cr_numero(t, n, &i, &ok);
			if (i < n && t[i] == ':' && h < 0) {
				h = (i32)(v > 99 ? 99 : v);
				i++;
				mi = (i32)cr_numero(t, n, &i, &ok);
				if (mi > 99)
					mi = 99;
				if (i < n && t[i] == ':') {
					i++;
					s = (i32)cr_numero(t, n, &i, &ok);
					if (s > 99)
						s = 99;
				}
				if (i < n && t[i] == '.')	/* las décimas */
					for (i++; i < n && es_cifra((u8)t[i]); i++)
						;
				continue;
			}
			if (i - a >= 3) {
				if (anio < 0)
					anio = (i32)(v > 9999 ? 9999 : v) + (i - a == 3 ? 1900 : 0);
			} else if (dia < 0)
				dia = (i32)v;
			else if (anio < 0)
				anio = (i32)v + (v < 50 ? 2000 : 1900);
			continue;
		}
		if ((c == '+' || c == '-') && h >= 0 && i + 4 < n + 0 && es_cifra((u8)t[i + 1]) && es_cifra((u8)t[i + 2]) &&
		    es_cifra((u8)t[i + 3]) && es_cifra((u8)t[i + 4]) && !hay_zona) {
			i32 z = ((t[i + 1] - '0') * 10 + (t[i + 2] - '0')) * 60 + (t[i + 3] - '0') * 10 + (t[i + 4] - '0');

			zona = c == '-' ? -z : z;
			hay_zona = 1;
			i += 5;
			continue;
		}
		if (es_letra(c)) {
			u32 a = i, k;
			int z;

			while (i < n && es_letra((u8)t[i]))
				i++;
			if (i - a >= 3 && mes < 0) {
				for (k = 0; k < 12; k++)
					if (cr_empieza(t + a, i - a, MESES[k]) || cr_empieza(t + a, i - a, MESES_ES[k])) {
						mes = (i32)k;
						break;
					}
				if (mes >= 0)
					continue;
			}
			if (!hay_zona && h >= 0 && zona_nombre(t + a, i - a, &z)) {
				zona = z;
				hay_zona = 1;
			}
			continue;
		}
		i++;
	}
	if (dia < 1 || dia > 31 || mes < 0 || mes > 11 || anio < 1900 || anio > 9999)
		return 0;
	if (h < 0)
		h = 0;
	if (h > 23 || mi > 59)
		return 0;
	if (s > 59)
		s = 59;
	if (zona > 18 * 60 || zona < -18 * 60)
		zona = 0;
	return (i64)dias_de_fecha(anio, (u32)mes + 1, (u32)dia) * 86400 + h * 3600 + mi * 60 + s - (i64)zona * 60;
}

i64 mime_fecha_imap(const char *t, u32 n)
{
	return mime_fecha(t, n);
}

/* la hora local, por piezas (sin divisiones de 64 bits: de 1970 a 2106) */
typedef struct {
	i32 anio;
	u32 mes, dia, h, mi, s, semana;	/* semana: 0 domingo */
	u32 dias;
} Hora;

static void hora_de(i64 utc, int zona, Hora *r)
{
	i64 v = utc + (i64)zona * 60;
	u32 x, d;

	if (v < 0)
		v = 0;
	if (v > (i64)0xFFFFFFFFu)
		v = 0xFFFFFFFFu;
	x = (u32)v;
	d = x / 86400;
	x %= 86400;
	r->dias = d;
	r->h = x / 3600;
	r->mi = x / 60 % 60;
	r->s = x % 60;
	r->semana = (d + 4) % 7;
	fecha_de_dias((i32)d, &r->anio, &r->mes, &r->dia);
}

void mime_fecha_rfc(i64 utc, int zona, char *d, int cabe)
{
	static const char *const sem[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static const char *const mes[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	Hora h;
	int z = zona < 0 ? -zona : zona;

	if (z > 18 * 60) {
		zona = 0;
		z = 0;
	}
	hora_de(utc, zona, &h);
	snprintf(d, (size_t)cabe, "%s, %d %s %d %02d:%02d:%02d %c%02d%02d", sem[h.semana], (int)h.dia, mes[h.mes - 1], (int)h.anio,
		 (int)h.h, (int)h.mi, (int)h.s, zona < 0 ? '-' : '+', z / 60, z % 60);
}

void mime_fecha_corta(i64 utc, int zona, i64 ahora, char *d, int cabe)
{
	Hora h, a;

	hora_de(utc, zona, &h);
	hora_de(ahora, zona, &a);
	if (h.dias == a.dias)
		snprintf(d, (size_t)cabe, "%02d:%02d", (int)h.h, (int)h.mi);
	else if (h.anio == a.anio)
		snprintf(d, (size_t)cabe, "%02d/%02d %02d:%02d", (int)h.dia, (int)h.mes, (int)h.h, (int)h.mi);
	else
		snprintf(d, (size_t)cabe, "%02d/%02d/%d", (int)h.dia, (int)h.mes, (int)h.anio);
}

void mime_fecha_larga(i64 utc, int zona, char *d, int cabe)
{
	static const char *const sem[7] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
	Hora h;

	hora_de(utc, zona, &h);
	snprintf(d, (size_t)cabe, "%s, %02d/%02d/%d %02d:%02d", sem[h.semana], (int)h.dia, (int)h.mes, (int)h.anio, (int)h.h,
		 (int)h.mi);
}

/* =================== el analizador =================== */
typedef struct {
	int parte;	/* el contenedor (multipart, o message/rfc822: «envoltorio») */
	int hijo;	/* la parte de ahora (-1: el preámbulo, o nada) */
	int ultimo;	/* el último hijo enlazado */
	u32 nf;		/* lo largo de la frontera (0: envoltorio) */
	int larga;	/* la frontera no cabía: vale con que empiece igual */
} Nivel;

typedef struct {
	Mensaje *m;
	int cap_partes, cap_cab;
	Nivel pila[MIME_MAX_NIVEL + 1];
	int np;
	int sin_memoria;
} Analisis;

/* el valor crudo de la primera cabecera «nombre» de la parte (sin copiar) */
static const char *cab_cruda(const Mensaje *m, int parte, const char *nombre, int cual, u32 *n)
{
	const Parte *p;
	int k;

	if (parte < 0 || parte >= m->n_partes)
		return NULL;
	p = &m->partes[parte];
	for (k = 0; k < p->n_cab; k++) {
		const Cabecera *c = &m->cab[p->cab0 + k];

		if (cr_igual(c->nombre, nombre) && cual-- == 0) {
			*n = c->n;
			return (const char *)m->d + c->ini;
		}
	}
	return NULL;
}

/* ¿es [s, e) (sin el \r) una frontera de la pila? el nivel, o -1 */
static int es_frontera(const Analisis *A, u32 s, u32 e, int *cierre)
{
	const u8 *d = A->m->d;
	int k;

	if (e - s < 3 || d[s] != '-' || d[s + 1] != '-')
		return -1;
	for (k = A->np - 1; k >= 0; k--) {
		const Nivel *L = &A->pila[k];
		const char *fr = A->m->partes[L->parte].frontera;
		u32 r;

		if (!L->nf || e - s - 2 < L->nf || memcmp(d + s + 2, fr, L->nf))
			continue;
		r = s + 2 + L->nf;
		*cierre = r + 1 < e && d[r] == '-' && d[r + 1] == '-';
		if (L->larga)
			return k;
		if (*cierre)
			r += 2;
		while (r < e && es_blanco(d[r]))
			r++;
		if (r == e)
			return k;
	}
	return -1;
}

static int crece(void **v, int *cap, int quiere, size_t tam)
{
	int c;
	void *q;

	if (quiere <= *cap)
		return 1;
	c = *cap ? *cap * 2 : 16;
	while (c < quiere)
		c *= 2;
	q = so_repide(*v, (size_t)c * tam);
	if (!q)
		return 0;
	*v = q;
	*cap = c;
	return 1;
}

/* las cabeceras desde «p» (de la parte «parte»): da dónde empieza el cuerpo */
static u32 lee_cabeceras(Analisis *A, int parte, u32 p)
{
	Mensaje *m = A->m;
	const u8 *d = m->d;
	u32 n = m->n;
	Parte *P = &m->partes[parte];
	Cabecera *ultima = NULL;

	P->cab0 = m->n_cab;
	P->n_cab = 0;
	if (p == 0 && n >= 5 && !memcmp(d, "From ", 5)) {	/* el renglón de mbox */
		const u8 *f = memchr(d, '\n', n);

		p = f ? (u32)(f - d) + 1 : n;
	}
	while (p < n) {
		const u8 *f = memchr(d + p, '\n', n - p);
		u32 e = f ? (u32)(f - d) : n, ef = e, k;
		int cierre;

		if (ef > p && d[ef - 1] == '\r')
			ef--;
		if (ef == p)
			return e < n ? e + 1 : n;	/* el renglón en blanco */
		if (es_blanco(d[p])) {
			if (ultima)
				ultima->n = ef - ultima->ini;
			p = e + 1;
			continue;
		}
		if (es_frontera(A, p, ef, &cierre) >= 0)
			return p;
		for (k = p; k < ef && d[k] != ':' && d[k] > 32 && d[k] < 127; k++)
			;
		{
			u32 nf = k;

			while (k < ef && es_blanco(d[k]))
				k++;
			if (k >= ef || d[k] != ':' || nf == p) {
				m->malformado = 1;
				return p;	/* no es una cabecera: empieza el cuerpo */
			}
			ultima = NULL;
			if (P->n_cab < MIME_MAX_CABECERAS && m->n_cab < MAX_CAB_TOTAL &&
			    crece((void **)&m->cab, &A->cap_cab, m->n_cab + 1, sizeof(Cabecera))) {
				Cabecera *c = &m->cab[m->n_cab++];
				u32 v = k + 1;

				P = &m->partes[parte];
				P->n_cab++;
				cr_copia_en(c->nombre, sizeof c->nombre, (const char *)d + p, nf - p);
				while (v < ef && es_blanco(d[v]))
					v++;
				c->ini = v;
				c->n = ef - v;
				ultima = c;
			} else if (P->n_cab >= MIME_MAX_CABECERAS)
				m->cortado = 1;
		}
		p = e + 1;
	}
	return n;
}

static void copia_minus(char *d, int cabe, const char *s)
{
	int k = 0;

	for (; s && *s && k < cabe - 1; s++)
		d[k++] = (char)minuscula((u8)*s);
	d[k] = 0;
}

/* lo que dicen las cabeceras de la parte */
static void interpreta(Analisis *A, int i)
{
	Mensaje *m = A->m;
	Parte *P = &m->partes[i];
	const char *c;
	u32 n;
	char *v, *x;
	int digest = P->padre >= 0 && !strcmp(m->partes[P->padre].tipo, "multipart/digest");

	strcpy(P->tipo, digest ? "message/rfc822" : "text/plain");
	c = cab_cruda(m, i, "content-type", 0, &n);
	if (c) {
		v = mime_valor(c, n);
		if (v && strchr(v, '/') && v[0] != '/') {
			cr_copia_en(P->tipo, sizeof P->tipo, v, (u32)strlen(v));
		} else if (v && !strcmp(v, "text"))
			strcpy(P->tipo, "text/plain");
		else if (v && v[0])
			m->malformado = 1;
		so_suelta(v);
		x = mime_parametro(c, n, "charset");
		copia_minus(P->juego, sizeof P->juego, x);
		so_suelta(x);
		if (!strncmp(P->tipo, "multipart/", 10)) {
			x = mime_parametro(c, n, "boundary");
			if (x && x[0])
				cr_copia_en(P->frontera, sizeof P->frontera, x, (u32)strlen(x));
			so_suelta(x);
		}
		x = mime_parametro(c, n, "format");
		P->flowed = x && cr_igual(x, "flowed");
		so_suelta(x);
		x = mime_parametro(c, n, "delsp");
		P->delsp = x && cr_igual(x, "yes");
		so_suelta(x);
		x = mime_parametro(c, n, "name");
		if (x)
			cr_copia_en(P->nombre, sizeof P->nombre, x, (u32)strlen(x));
		so_suelta(x);
	}
	c = cab_cruda(m, i, "content-disposition", 0, &n);
	if (c) {
		v = mime_valor(c, n);
		cr_copia_en(P->disposicion, sizeof P->disposicion, v ? v : "", v ? (u32)strlen(v) : 0);
		so_suelta(v);
		x = mime_parametro(c, n, "filename");
		if (x && x[0])
			cr_copia_en(P->nombre, sizeof P->nombre, x, (u32)strlen(x));
		so_suelta(x);
	}
	/* el nombre: sin carpetas delante ni controles */
	{
		char *s = P->nombre, *u = s, *w;

		for (w = s; *w; w++)
			if (*w == '/' || *w == '\\')
				u = w + 1;
		if (u != s)
			memmove(s, u, strlen(u) + 1);
		for (w = s; *w; w++)
			if ((u8)*w < 0x20 || *w == 0x7f)
				*w = '_';
	}
	c = cab_cruda(m, i, "content-transfer-encoding", 0, &n);
	P->codificacion = COD_NADA;
	if (c) {
		v = mime_valor(c, n);
		if (v) {
			if (!strcmp(v, "base64"))
				P->codificacion = COD_BASE64;
			else if (!strcmp(v, "quoted-printable"))
				P->codificacion = COD_QP;
			else if (!strcmp(v, "x-uuencode") || !strcmp(v, "uuencode") || !strcmp(v, "x-uue") || !strcmp(v, "uue"))
				P->codificacion = COD_UUENCODE;
		}
		so_suelta(v);
	}
	if (!strncmp(P->tipo, "multipart/", 10) || !strcmp(P->tipo, "message/rfc822"))
		P->codificacion = !strcmp(P->tipo, "message/rfc822") ? P->codificacion : COD_NADA;
	c = cab_cruda(m, i, "content-id", 0, &n);
	if (c) {
		u32 a = 0, k = 0;

		while (a < n && (es_espacio((u8)c[a]) || c[a] == '<'))
			a++;
		while (n > a && (es_espacio((u8)c[n - 1]) || c[n - 1] == '>'))
			n--;
		for (; a < n && k + 1 < sizeof P->id; a++)
			if ((u8)c[a] > 32)
				P->id[k++] = c[a];
		P->id[k] = 0;
	}
}

/* una parte nueva, con sus cabeceras desde «p»: su número, o -1 */
static int parte_nueva(Analisis *A, int padre, u32 p)
{
	Mensaje *m = A->m;
	Parte *P;
	int i;

	if (m->n_partes >= MIME_MAX_PARTES) {
		m->cortado = 1;
		return -1;
	}
	if (!crece((void **)&m->partes, &A->cap_partes, m->n_partes + 1, sizeof(Parte))) {
		A->sin_memoria = 1;
		return -1;
	}
	i = m->n_partes++;
	P = &m->partes[i];
	memset(P, 0, sizeof *P);
	P->padre = padre;
	P->primera = P->siguiente = -1;
	P->nivel = padre >= 0 ? m->partes[padre].nivel + 1 : 0;
	P->cab_ini = p;
	P->ini = lee_cabeceras(A, i, p);
	P = &m->partes[i];
	P->cab_fin = P->ini;
	P->fin = m->n;
	interpreta(A, i);
	return i;
}

static void enlaza(Mensaje *m, int padre, int *ultimo, int hijo)
{
	if (padre < 0)
		return;
	if (*ultimo < 0)
		m->partes[padre].primera = hijo;
	else
		m->partes[*ultimo].siguiente = hijo;
	*ultimo = hijo;
}

/* si la parte es un contenedor, a la pila (y lo de dentro de un
 * message/rfc822, enseguida) */
static void abre(Analisis *A, int i)
{
	Mensaje *m = A->m;

	while (i >= 0) {
		Parte *P = &m->partes[i];
		Nivel *L;

		if (!strncmp(P->tipo, "multipart/", 10)) {
			if (!P->frontera[0]) {
				m->malformado = 1;
				return;
			}
			if (A->np >= MIME_MAX_NIVEL) {
				m->cortado = 1;
				return;
			}
			L = &A->pila[A->np++];
			L->parte = i;
			L->hijo = -1;
			L->ultimo = -1;
			L->nf = (u32)strlen(P->frontera);
			L->larga = L->nf >= sizeof P->frontera - 1;
			return;
		}
		if (!strcmp(P->tipo, "message/rfc822") && P->codificacion == COD_NADA) {
			int c;

			if (A->np >= MIME_MAX_NIVEL) {
				m->cortado = 1;
				return;
			}
			L = &A->pila[A->np++];
			L->parte = i;
			L->hijo = -1;
			L->ultimo = -1;
			L->nf = 0;
			L->larga = 0;
			c = parte_nueva(A, i, P->ini);
			if (c < 0)
				return;
			L = &A->pila[A->np - 1];
			enlaza(m, i, &L->ultimo, c);
			L->hijo = c;
			i = c;
			continue;
		}
		return;
	}
}

/* el renglón de [p, ...) : su final (sin \n) */
static u32 fin_renglon(const u8 *d, u32 n, u32 p)
{
	const u8 *f = memchr(d + p, '\n', n - p);

	return f ? (u32)(f - d) : n;
}

/* los uuencode sueltos de un texto: «begin 644 nombre» ... «end» */
static void busca_uuencode(Analisis *A, int i)
{
	Mensaje *m = A->m;
	const u8 *d = m->d;
	u32 p = m->partes[i].ini, fin = m->partes[i].fin;
	int ultimo = -1;

	while (p < fin) {
		u32 e = fin_renglon(d, fin, p), ef = e, k;

		if (ef > p && d[ef - 1] == '\r')
			ef--;
		if (ef - p > 10 && !memcmp(d + p, "begin ", 6)) {
			for (k = p + 6; k < ef && d[k] >= '0' && d[k] <= '7'; k++)
				;
			if (k - (p + 6) >= 3 && k - (p + 6) <= 4 && k < ef && d[k] == ' ' && k + 1 < ef) {
				u32 q = e + 1 > fin ? fin : e + 1, fn = fin;
				int c;
				Parte *P;

				/* el «end» */
				while (q < fin) {
					u32 e2 = fin_renglon(d, fin, q), f2 = e2;

					while (f2 > q && (d[f2 - 1] == '\r' || es_blanco(d[f2 - 1])))
						f2--;
					if (f2 - q == 3 && !memcmp(d + q, "end", 3)) {
						fn = e2 < fin ? e2 + 1 : fin;
						break;
					}
					q = e2 + 1;
				}
				if (m->n_partes >= MIME_MAX_PARTES) {
					m->cortado = 1;
					return;
				}
				if (!crece((void **)&m->partes, &A->cap_partes, m->n_partes + 1, sizeof(Parte))) {
					A->sin_memoria = 1;
					return;
				}
				c = m->n_partes++;
				P = &m->partes[c];
				memset(P, 0, sizeof *P);
				P->padre = i;
				P->primera = P->siguiente = -1;
				P->nivel = m->partes[i].nivel + 1;
				P->cab_ini = P->cab_fin = p;
				P->ini = p;
				P->fin = fn;
				P->cab0 = m->n_cab;
				P->codificacion = COD_UUENCODE;
				cr_copia_en(P->nombre, sizeof P->nombre, (const char *)d + k + 1, ef - (k + 1));
				{
					char *s;

					for (s = P->nombre; *s; s++)
						if (*s == '/' || *s == '\\' || (u8)*s < 0x20)
							*s = '_';
				}
				cr_copia_en(P->tipo, sizeof P->tipo, mime_tipo_de(P->nombre), (u32)strlen(mime_tipo_de(P->nombre)));
				strcpy(P->disposicion, "attachment");
				P->es_adjunto = 1;
				enlaza(m, i, &ultimo, c);
				p = fn;
				continue;
			}
		}
		p = e + 1;
	}
}

static int tipo_es(const Parte *P, const char *t)
{
	return !strcmp(P->tipo, t);
}

static int es_multi(const Parte *P)
{
	return !strncmp(P->tipo, "multipart/", 10);
}

/* la raíz de un multipart/related (la del parámetro start, o la primera) */
static int raiz_related(const Mensaje *m, int i)
{
	u32 n;
	const char *c = cab_cruda(m, i, "content-type", 0, &n);
	int h = m->partes[i].primera;

	if (c) {
		char *st = mime_parametro(c, n, "start");

		if (st) {
			char *s = st;
			u32 l;
			int k;

			while (*s == '<' || es_espacio((u8)*s))
				s++;
			l = (u32)strlen(s);
			while (l && (s[l - 1] == '>' || es_espacio((u8)s[l - 1])))
				l--;
			s[l] = 0;
			for (k = h; k >= 0; k = m->partes[k].siguiente)
				if (cr_igual(m->partes[k].id, s)) {
					so_suelta(st);
					return k;
				}
			so_suelta(st);
		}
	}
	return h;
}

/* qué es una opción de un alternative: 2 html, 1 texto, 0 otra cosa */
static int clase_alt(const Mensaje *m, int i)
{
	int vueltas = 0;

	while (i >= 0 && vueltas++ < MIME_MAX_NIVEL + 2) {
		const Parte *P = &m->partes[i];

		if (tipo_es(P, "text/html"))
			return 2;
		if (tipo_es(P, "text/plain") || tipo_es(P, "text/enriched"))
			return 1;
		if (tipo_es(P, "multipart/related"))
			i = raiz_related(m, i);
		else if (es_multi(P))
			i = P->primera;
		else
			return 0;
	}
	return 0;
}

static int es_texto_cuerpo(const Parte *P)
{
	return (tipo_es(P, "text/plain") || tipo_es(P, "text/html")) && strcmp(P->disposicion, "attachment");
}

int mime_principal(const Mensaje *m, int prefiere_html)
{
	int i = 0, vueltas = 0;

	if (!m || m->n_partes <= 0)
		return -1;
	while (i >= 0 && vueltas++ < MIME_MAX_PARTES) {
		const Parte *P = &m->partes[i];

		if (es_texto_cuerpo(P))
			return i;
		if (tipo_es(P, "multipart/alternative")) {
			int k, elegida = -1, otra = -1, quiere = prefiere_html ? 2 : 1;

			for (k = P->primera; k >= 0; k = m->partes[k].siguiente) {
				int c = clase_alt(m, k);

				if (c == quiere)
					elegida = k;	/* la última de las que valen */
				else if (c)
					otra = k;
			}
			if (elegida < 0)
				elegida = otra >= 0 ? otra : P->primera;
			i = elegida;
			continue;
		}
		if (tipo_es(P, "multipart/related")) {
			i = raiz_related(m, i);
			continue;
		}
		if (tipo_es(P, "multipart/encrypted"))
			return -1;
		if (es_multi(P)) {
			int k;

			/* el primero que no sea un adjunto */
			for (k = P->primera; k >= 0; k = m->partes[k].siguiente) {
				const Parte *H = &m->partes[k];

				if (strcmp(H->disposicion, "attachment") && (es_multi(H) || es_texto_cuerpo(H)))
					break;
			}
			i = k;
			continue;
		}
		if (tipo_es(P, "message/rfc822") && i == 0 && P->primera >= 0) {
			i = P->primera;
			continue;
		}
		return -1;
	}
	return -1;
}

/* ¿está dentro de un message/rfc822 (que no sea el de arriba)? */
static int dentro_de_mensaje(const Mensaje *m, int i)
{
	int p = m->partes[i].padre, v = 0;

	while (p > 0 && v++ < MIME_MAX_NIVEL + 2) {
		if (tipo_es(&m->partes[p], "message/rfc822"))
			return 1;
		p = m->partes[p].padre;
	}
	return 0;
}

static void decide_adjuntos(Analisis *A)
{
	Mensaje *m = A->m;
	int ph = mime_principal(m, 1), pt = mime_principal(m, 0), i;
	char *html = NULL;
	u32 nhtml = 0;
	int html_hecho = 0;

	for (i = 0; i < m->n_partes; i++) {
		Parte *P = &m->partes[i];
		int padre = P->padre;

		P->tam = P->codificacion == COD_BASE64 ? (P->fin - P->ini) / 4 * 3 : P->fin - P->ini;
		if (P->codificacion == COD_UUENCODE) {
			P->tam = (P->fin - P->ini) / 4 * 3;
			if (!dentro_de_mensaje(m, i))
				P->es_adjunto = 1;
			continue;
		}
		P->es_adjunto = 0;
		if (i == ph || i == pt || es_multi(P) || dentro_de_mensaje(m, i))
			continue;
		if (tipo_es(P, "message/rfc822")) {
			if (i == 0)
				continue;
			P->es_adjunto = 1;
			if (!P->nombre[0] && P->primera >= 0) {
				char *s = mime_cabecera_texto(m, P->primera, "subject");

				if (s) {
					u32 l = (u32)strlen(s);
					char *w;

					if (l > 200)
						l = 200;
					while (l && ((u8)s[l] & 0xC0) == 0x80)
						l--;
					s[l] = 0;
					for (w = s; *w; w++)
						if (*w == '/' || *w == '\\' || *w == ':')
							*w = '_';
					snprintf(P->nombre, sizeof P->nombre, "%s.eml", s[0] ? s : "mensaje");
					so_suelta(s);
				} else
					strcpy(P->nombre, "mensaje.eml");
			}
			continue;
		}
		if (padre >= 0 && tipo_es(&m->partes[padre], "multipart/signed") &&
		    (strstr(P->tipo, "pgp-signature") || strstr(P->tipo, "pkcs7-signature")))
			continue;
		if (padre >= 0 && tipo_es(&m->partes[padre], "multipart/alternative") && (tipo_es(P, "text/plain") || tipo_es(P, "text/html")))
			continue;
		if (P->ini >= P->fin && !P->nombre[0])
			continue;
		if (padre >= 0 && tipo_es(&m->partes[padre], "multipart/related") && P->id[0] &&
		    strcmp(P->disposicion, "attachment") && i != raiz_related(m, padre)) {
			/* ¿la usa el HTML? */
			if (!html_hecho) {
				html_hecho = 1;
				if (ph >= 0 && tipo_es(&m->partes[ph], "text/html"))
					html = mime_texto(m, ph, &nhtml);
			}
			if (!html)
				continue;
			{
				char b[140];
				u32 l;

				snprintf(b, sizeof b, "cid:%s", P->id);
				l = (u32)strlen(b);
				if (cr_busca_sin((const u8 *)html, nhtml, b, l) >= 0)
					continue;
			}
		}
		P->es_adjunto = 1;
	}
	so_suelta(html);
}

Mensaje *mime_analiza(const u8 *d, u32 n)
{
	Analisis A;
	Mensaje *m;
	u32 pos;
	int i, nt;

	memset(&A, 0, sizeof A);
	m = so_pide(sizeof *m);
	if (!m)
		return NULL;
	memset(m, 0, sizeof *m);
	if (!d)
		n = 0;
	m->d = d ? d : (const u8 *)"";
	if (n > MIME_MAX_MENSAJE) {
		n = MIME_MAX_MENSAJE;
		m->cortado = 1;
	}
	m->n = n;
	A.m = m;
	if (parte_nueva(&A, -1, 0) < 0) {
		mime_libera(m);
		return NULL;
	}
	abre(&A, 0);
	pos = m->partes[0].ini;
	/* (el envoltorio de arriba ya ha puesto su hijo: sus cabeceras) */
	if (A.np > 0 && A.pila[A.np - 1].hijo >= 0)
		pos = m->partes[A.pila[A.np - 1].hijo].ini;
	while (A.np > 0 && pos < n && !A.sin_memoria) {
		u32 e = fin_renglon(d, n, pos), ef = e;
		int cierre = 0, k;

		if (ef > pos && d[ef - 1] == '\r')
			ef--;
		k = (d[pos] == '-') ? es_frontera(&A, pos, ef, &cierre) : -1;
		if (k >= 0) {
			u32 corte = pos;
			int j;
			Nivel *L;

			/* el salto de renglón de delante es de la frontera */
			if (corte > 0 && d[corte - 1] == '\n') {
				corte--;
				if (corte > 0 && d[corte - 1] == '\r')
					corte--;
			}
			for (j = A.np - 1; j > k; j--) {
				Nivel *J = &A.pila[j];

				if (J->hijo >= 0)
					m->partes[J->hijo].fin = corte < m->partes[J->hijo].ini ? m->partes[J->hijo].ini : corte;
				m->partes[J->parte].fin = corte < m->partes[J->parte].ini ? m->partes[J->parte].ini : corte;
				if (J->nf)
					m->malformado = 1;	/* un multipart sin cerrar */
			}
			A.np = k + 1;
			L = &A.pila[k];
			if (L->hijo >= 0) {
				Parte *H = &m->partes[L->hijo];

				H->fin = corte < H->ini ? H->ini : corte;
				L->hijo = -1;
			}
			if (cierre) {
				A.np = k;
				pos = e + 1;
				continue;
			}
			{
				int c = parte_nueva(&A, L->parte, e < n ? e + 1 : n);

				L = &A.pila[k];
				if (c < 0) {
					pos = e + 1;
					continue;
				}
				enlaza(m, L->parte, &L->ultimo, c);
				L->hijo = c;
				pos = m->partes[c].ini;
				abre(&A, c);
				if (A.np > 0 && A.pila[A.np - 1].hijo >= 0 && A.pila[A.np - 1].nf == 0)
					pos = m->partes[A.pila[A.np - 1].hijo].ini;
				continue;
			}
		}
		pos = e + 1;
	}
	if (A.np > 0) {
		int j;

		for (j = 0; j < A.np; j++)
			if (A.pila[j].nf)
				m->malformado = 1;	/* sin cerrar */
	}
	if (A.sin_memoria) {
		mime_libera(m);
		return NULL;
	}
	/* los uuencode de los textos */
	nt = m->n_partes;
	for (i = 0; i < nt; i++) {
		Parte *P = &m->partes[i];

		if (tipo_es(P, "text/plain") && P->codificacion == COD_NADA && P->primera < 0 && P->fin - P->ini > 20 &&
		    cr_busca(d + P->ini, P->fin - P->ini, "begin ", 6) >= 0)
			busca_uuencode(&A, i);
	}
	if (A.sin_memoria) {
		mime_libera(m);
		return NULL;
	}
	decide_adjuntos(&A);
	return m;
}

void mime_libera(Mensaje *m)
{
	if (!m)
		return;
	so_suelta(m->partes);
	so_suelta(m->cab);
	so_suelta(m);
}

char *mime_cabecera(const Mensaje *m, int parte, const char *nombre, int cual)
{
	u32 n, i;
	const char *c = m ? cab_cruda(m, parte, nombre, cual, &n) : NULL;
	Texto t;

	if (!c)
		return NULL;
	tx_ini(&t, 0);
	for (i = 0; i < n; i++)
		if (c[i] != '\r' && c[i] != '\n')
			tx_c(&t, c[i]);
	return tx_fin(&t, NULL);
}

char *mime_cabecera_texto(const Mensaje *m, int parte, const char *nombre)
{
	u32 n, a = 0, l;
	const char *c = m ? cab_cruda(m, parte, nombre, 0, &n) : NULL;
	char *r;

	if (!c)
		return NULL;
	r = rfc2047_descodifica(c, n);
	if (!r)
		return NULL;
	l = (u32)strlen(r);
	while (l && es_espacio((u8)r[l - 1]))
		l--;
	while (a < l && es_espacio((u8)r[a]))
		a++;
	memmove(r, r + a, l - a);
	r[l - a] = 0;
	return r;
}

/* uudecode («begin» ... «end»; sin «begin», desde el principio) */
static u8 *uu_descodifica(const u8 *d, u32 n, u32 *largo)
{
	u8 *o = so_pide(n / 4 * 3 + 4);
	u32 p = 0, k = 0;
	i32 b = cr_busca(d, n, "begin ", 6);

	if (!o) {
		*largo = 0;
		return NULL;
	}
	if (b >= 0 && (b == 0 || d[b - 1] == '\n')) {
		p = fin_renglon(d, n, (u32)b) + 1;
	}
	while (p < n) {
		u32 e = fin_renglon(d, n, p), ef = e, l, j;

		if (ef > p && d[ef - 1] == '\r')
			ef--;
		if (ef - p >= 3 && !memcmp(d + p, "end", 3))
			break;
		if (ef > p) {
			l = (u32)(d[p] - 32) & 63;
			for (j = p + 1; l > 0 && j < ef; j += 4) {
				u32 c[4], q;

				for (q = 0; q < 4; q++)
					c[q] = j + q < ef ? ((u32)(d[j + q] - 32) & 63) : 0;
				if (l > 0) {
					o[k++] = (u8)((c[0] << 2) | (c[1] >> 4));
					l--;
				}
				if (l > 0) {
					o[k++] = (u8)((c[1] << 4) | (c[2] >> 2));
					l--;
				}
				if (l > 0) {
					o[k++] = (u8)((c[2] << 6) | c[3]);
					l--;
				}
			}
		}
		p = e + 1;
	}
	o[k] = 0;
	*largo = k;
	return o;
}

u8 *mime_cuerpo(const Mensaje *m, int parte, u32 *largo)
{
	const Parte *P;
	u32 l = 0;
	u8 *r;

	if (largo)
		*largo = 0;
	if (!m || parte < 0 || parte >= m->n_partes)
		return NULL;
	P = &m->partes[parte];
	if (P->fin <= P->ini)
		r = (u8 *)cr_copia("", 0);
	else if (P->codificacion == COD_BASE64)
		r = b64_descodifica(m->d + P->ini, P->fin - P->ini, &l);
	else if (P->codificacion == COD_QP)
		r = qp_descodifica(m->d + P->ini, P->fin - P->ini, &l, 0);
	else if (P->codificacion == COD_UUENCODE)
		r = uu_descodifica(m->d + P->ini, P->fin - P->ini, &l);
	else {
		l = P->fin - P->ini;
		r = (u8 *)cr_copia((const char *)m->d + P->ini, l);
	}
	if (largo)
		*largo = r ? l : 0;
	return r;
}

/* format=flowed (RFC 3676) a renglones de siempre; «t» ya con \n */
static char *desflowa(const char *t, u32 n, int delsp, u32 *largo)
{
	Texto o;
	u32 p = 0;
	int en_parrafo = 0, cita_antes = 0;

	tx_ini(&o, 0);
	while (p < n) {
		const char *f = memchr(t + p, '\n', n - p);
		u32 e = f ? (u32)(f - t) : n, a = p, c;
		int q = 0, firma, blando;

		while (a < e && t[a] == '>') {
			q++;
			a++;
		}
		if (a < e && t[a] == ' ')
			a++;	/* el espacio de relleno */
		c = e - a;
		firma = c == 3 && !memcmp(t + a, "-- ", 3);
		blando = !firma && c > 0 && t[e - 1] == ' ';
		if (en_parrafo && q != cita_antes) {
			tx_c(&o, '\n');
			en_parrafo = 0;
		}
		if (!en_parrafo) {
			int k;

			for (k = 0; k < q; k++)
				tx_c(&o, '>');
			if (q)
				tx_c(&o, ' ');
		}
		tx_pon(&o, t + a, blando && delsp ? c - 1 : c);
		if (blando) {
			en_parrafo = 1;
			cita_antes = q;
		} else {
			if (f)
				tx_c(&o, '\n');
			en_parrafo = 0;
		}
		p = e + 1;
	}
	return tx_fin(&o, largo);
}

char *mime_texto(const Mensaje *m, int parte, u32 *largo)
{
	const Parte *P;
	u8 *b;
	u32 l, i, k;
	char *u;

	if (largo)
		*largo = 0;
	if (!m || parte < 0 || parte >= m->n_partes)
		return NULL;
	P = &m->partes[parte];
	if (P->codificacion == COD_NADA && P->primera >= 0 && !es_multi(P)) {
		/* sin los uuencode de dentro */
		Texto t;
		u32 p = P->ini;
		int h;

		tx_ini(&t, 0);
		for (h = P->primera; h >= 0; h = m->partes[h].siguiente) {
			const Parte *H = &m->partes[h];

			if (H->ini > p && H->ini <= P->fin)
				tx_pon(&t, m->d + p, H->ini - p);
			if (H->fin > p)
				p = H->fin;
		}
		if (p < P->fin)
			tx_pon(&t, m->d + p, P->fin - p);
		b = (u8 *)tx_fin(&t, &l);
	} else
		b = mime_cuerpo(m, parte, &l);
	if (!b)
		return NULL;
	u = jc_a_utf8(P->juego[0] ? P->juego : NULL, b, l, &l);
	so_suelta(b);
	if (!u)
		return NULL;
	/* CRLF -> LF; los \0 fuera */
	for (i = k = 0; i < l; i++) {
		if (u[i] == '\r' && (i + 1 >= l || u[i + 1] == '\n'))
			continue;
		if (u[i] == '\r')
			u[k++] = '\n';
		else if (u[i] == 0)
			u[k++] = ' ';
		else
			u[k++] = u[i];
	}
	u[k] = 0;
	l = k;
	if (P->flowed && tipo_es(P, "text/plain")) {
		char *f = desflowa(u, l, P->delsp, &l);

		so_suelta(u);
		u = f;
		if (!u)
			return NULL;
	}
	if (largo)
		*largo = l;
	return u;
}

int mime_por_id(const Mensaje *m, const char *cid)
{
	char b[160];
	u32 i = 0, k = 0, n;
	int p;

	if (!m || !cid)
		return -1;
	n = (u32)strlen(cid);
	if (cr_empieza(cid, n, "cid:"))
		i = 4;
	while (i < n && (cid[i] == '<' || es_espacio((u8)cid[i])))
		i++;
	for (; i < n && k + 1 < sizeof b; i++) {
		int a, c;

		if (cid[i] == '>')
			break;
		if (cid[i] == '%' && i + 2 < n && (a = valor_hex((u8)cid[i + 1])) >= 0 && (c = valor_hex((u8)cid[i + 2])) >= 0) {
			b[k++] = (char)(a * 16 + c);
			i += 2;
		} else
			b[k++] = cid[i];
	}
	b[k] = 0;
	if (!k)
		return -1;
	for (p = 0; p < m->n_partes; p++)
		if (m->partes[p].id[0] && cr_igual(m->partes[p].id, b))
			return p;
	return -1;
}

int mime_adjunto(const Mensaje *m, int k)
{
	int i;

	if (!m || k < 0)
		return -1;
	for (i = 0; i < m->n_partes; i++)
		if (m->partes[i].es_adjunto && k-- == 0)
			return i;
	return -1;
}

int mime_n_adjuntos(const Mensaje *m)
{
	int i, k = 0;

	if (!m)
		return 0;
	for (i = 0; i < m->n_partes; i++)
		k += m->partes[i].es_adjunto != 0;
	return k;
}

/* =================== la suplantación =================== */
static const char *dominio_de(const char *dir)
{
	const char *a = strrchr(dir, '@');

	return a ? a + 1 : dir;
}

/* la primera dirección (buzon@dominio.tld) que haya en un texto */
static int dir_en_texto(const char *t, char *d, int cabe)
{
	u32 n = (u32)strlen(t), i;

	for (i = 0; i < n; i++) {
		u32 a, b;
		char x[256];

		if (t[i] != '@')
			continue;
		for (a = i; a > 0 && (es_alnum((u8)t[a - 1]) || strchr("._%+-'", t[a - 1])); a--)
			;
		for (b = i + 1; b < n && (es_alnum((u8)t[b]) || t[b] == '.' || t[b] == '-'); b++)
			;
		while (b > i + 1 && t[b - 1] == '.')
			b--;
		if (a == i || b == i + 1 || b - a >= sizeof x)
			continue;
		memcpy(x, t + a, b - a);
		x[b - a] = 0;
		if (mime_dir_valida(x)) {
			cr_copia_en(d, cabe, x, b - a);
			return 1;
		}
	}
	return 0;
}

static int suplantacion(const Mensaje *m, char *motivo, int cabe)
{
	Direccion de[4], rt[2];
	u32 n;
	const char *c;
	int nd, nivel = 0;
	char base1[128], base2[128], x[256];

	if (cabe > 0)
		motivo[0] = 0;
	if (!m || m->n_partes <= 0)
		return 0;
	if (cab_cruda(m, 0, "from", 1, &n)) {
		snprintf(motivo, (size_t)cabe, "El mensaje lleva varios remitentes (From): puede ser falso.");
		return 2;
	}
	c = cab_cruda(m, 0, "from", 0, &n);
	if (!c) {
		snprintf(motivo, (size_t)cabe, "El mensaje no dice quién lo envía.");
		return 1;
	}
	nd = mime_direcciones(c, n, de, 4);
	if (nd <= 0 || !de[0].dir[0]) {
		snprintf(motivo, (size_t)cabe, "No se entiende la dirección del remitente.");
		return 1;
	}
	if (nd > 1) {
		snprintf(motivo, (size_t)cabe, "El remitente (From) lleva varias direcciones: puede ser falso.");
		nivel = 1;
	}
	/* el nombre lleva una dirección que no es la de verdad */
	if (dir_en_texto(de[0].nombre, x, sizeof x) && !cr_igual(x, de[0].dir)) {
		html_dominio_base(dominio_de(x), base1, sizeof base1);
		html_dominio_base(dominio_de(de[0].dir), base2, sizeof base2);
		if (!cr_igual(base1, base2)) {
			snprintf(motivo, (size_t)cabe, "El nombre dice «%s» pero el correo viene de «%s».", x, de[0].dir);
			return 2;
		}
	}
	/* las respuestas, a otro dominio */
	c = cab_cruda(m, 0, "reply-to", 0, &n);
	if (c && mime_direcciones(c, n, rt, 2) > 0 && rt[0].dir[0]) {
		html_dominio_base(dominio_de(rt[0].dir), base1, sizeof base1);
		html_dominio_base(dominio_de(de[0].dir), base2, sizeof base2);
		if (!cr_igual(base1, base2) && !nivel) {
			snprintf(motivo, (size_t)cabe, "Las respuestas irían a «%s», que no es del dominio del remitente («%s»).",
				 rt[0].dir, dominio_de(de[0].dir));
			nivel = 1;
		}
	}
	/* Sender: si el nombre lleva un dominio y lo envía otro */
	c = cab_cruda(m, 0, "sender", 0, &n);
	if (c && !nivel && mime_direcciones(c, n, rt, 1) > 0 && rt[0].dir[0] && dir_en_texto(de[0].nombre, x, sizeof x)) {
		html_dominio_base(dominio_de(rt[0].dir), base1, sizeof base1);
		html_dominio_base(dominio_de(x), base2, sizeof base2);
		if (!cr_igual(base1, base2)) {
			snprintf(motivo, (size_t)cabe, "El nombre dice «%s» pero lo envía «%s».", x, rt[0].dir);
			nivel = 1;
		}
	}
	return nivel;
}

int mime_suplantacion(const Mensaje *m, char *motivo, int cabe)
{
	int r = suplantacion(m, motivo, cabe);

	if (cabe > 0) {
		/* (snprintf puede haber partido la última letra) */
		u32 l = (u32)strlen(motivo), a = l;

		while (a > 0 && ((u8)motivo[a - 1] & 0xC0) == 0x80)
			a--;
		if (a > 0 && (u8)motivo[a - 1] >= 0xC0) {
			u8 c = (u8)motivo[a - 1];
			u32 quiere = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;

			if (l - (a - 1) < quiere)
				motivo[a - 1] = 0;
		}
	}
	return r;
}
