/* http.c - (COMPAT) el cliente HTTP (http.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "http.h"
#include "plat.h"
#include "texto.h"
#include "compat.h"

int http_parte(const char *url, http_url *u)
{
	const char *p = url;
	u32 i = 0;

	memset(u, 0, sizeof *u);
	if (empieza(p, "https://"))
		u->tls = 1, p += 8, u->puerto = 443;
	else if (empieza(p, "http://"))
		p += 7, u->puerto = 80;
	else
		return -1;
	while (*p && *p != '/' && *p != ':' && i + 1 < sizeof u->host)
		u->host[i++] = *p++;
	u->host[i] = 0;
	if (!i)
		return -1;
	if (*p == ':') {
		u->puerto = 0;
		for (p++; *p >= '0' && *p <= '9'; p++)
			u->puerto = u->puerto * 10 + (u32)(*p - '0');
		if (!u->puerto || u->puerto > 65535)
			return -1;
	}
	copia(u->camino, *p ? p : "/", sizeof u->camino);
	return 0;
}

typedef struct {
	plat_con *c;
	u8 b[8192];
	u32 i, n;
	int fin;
} lector;

static int llena(lector *l)
{
	i32 r;

	if (l->fin)
		return -1;
	r = plat_recibe(l->c, l->b, sizeof l->b, 60);
	if (r <= 0) {
		l->fin = 1;
		return -1;
	}
	l->i = 0;
	l->n = (u32)r;
	return 0;
}

static int linea(lector *l, char *d, u32 cabe)
{
	u32 j = 0;

	for (;;) {
		u8 c;

		if (l->i >= l->n && llena(l))
			return -1;
		c = l->b[l->i++];
		if (c == '\n')
			break;
		if (c != '\r' && j + 1 < cabe)
			d[j++] = (char)c;
	}
	d[j] = 0;
	return 0;
}

/* hasta n octetos del cuerpo (los que haya ya, o los que lleguen) */
static i32 trozo_de(lector *l, u8 *d, u32 n)
{
	u32 k;

	if (l->i >= l->n && llena(l))
		return 0;
	k = l->n - l->i < n ? l->n - l->i : n;
	memcpy(d, l->b + l->i, k);
	l->i += k;
	return (i32)k;
}

static u32 hex(const char *s)
{
	u32 v = 0;

	for (; *s; s++) {
		int c = *s;

		if (c >= '0' && c <= '9')
			v = v * 16 + (u32)(c - '0');
		else if (c >= 'a' && c <= 'f')
			v = v * 16 + (u32)(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			v = v * 16 + (u32)(c - 'A' + 10);
		else
			break;
	}
	return v;
}

int http_pide(const char *url, const char *metodo, const char *tipo, const u8 *cuerpo, u32 n,
	      http_trozo trozo, void *u, int *estado, char *ubicacion, u32 cabe_ub, char *error, u32 cabe)
{
	http_url w;
	char cab[900], l[600];
	lector *rd;
	u32 largo_ = 0xFFFFFFFFu, llevado = 0;
	int chunked = 0, r = -1;
	static u8 tmp[8192];

	*estado = 0;
	if (ubicacion && cabe_ub)
		ubicacion[0] = 0;
	if (http_parte(url, &w)) {
		copia(error, "una dirección que no vale", cabe);
		return -1;
	}
	rd = so_pide(sizeof *rd);
	if (!rd) {
		copia(error, "no hay memoria", cabe);
		return -1;
	}
	memset(rd, 0, sizeof *rd);
	rd->c = plat_conecta(w.host, w.puerto, w.tls, error, cabe);
	if (!rd->c) {
		so_suelta(rd);
		return -1;
	}
	if (cuerpo)
		fmt(cab, sizeof cab, "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Hispa-Pentium-Compat/%s\r\nContent-Type: %s\r\n"
		    "Content-Length: %u\r\nConnection: close\r\n\r\n", metodo, w.camino, w.host, COMPAT_VERSION, tipo, n);
	else
		fmt(cab, sizeof cab, "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Hispa-Pentium-Compat/%s\r\nAccept: */*\r\n"
		    "Connection: close\r\n\r\n", metodo, w.camino, w.host, COMPAT_VERSION);
	if (plat_envia(rd->c, cab, largo(cab)) || (cuerpo && n && plat_envia(rd->c, cuerpo, n))) {
		copia(error, "se ha cortado la conexión al mandar", cabe);
		goto fin;
	}
	if (linea(rd, l, sizeof l) || !empieza(l, "HTTP/1.")) {
		copia(error, "el servidor no contesta como un servidor web", cabe);
		goto fin;
	}
	*estado = (int)numero(l + 9, NULL);
	for (;;) {
		if (linea(rd, l, sizeof l)) {
			copia(error, "se ha cortado la respuesta", cabe);
			goto fin;
		}
		if (!l[0])
			break;
		{
			char m[40];
			u32 j = 0;

			while (l[j] && l[j] != ':' && j + 1 < sizeof m) {
				m[j] = (char)minuscula((u8)l[j]);
				j++;
			}
			m[j] = 0;
			if (l[j] == ':') {
				const char *v = l + j + 1;

				while (*v == ' ')
					v++;
				if (iguales(m, "content-length"))
					largo_ = (u32)numero(v, NULL);
				else if (iguales(m, "transfer-encoding") && busca(v, "chunked"))
					chunked = 1;
				else if (iguales(m, "location") && ubicacion)
					copia(ubicacion, v, cabe_ub);
			}
		}
	}
	if (!trozo) {
		r = 0;
		goto fin;
	}
	if (chunked) {
		for (;;) {
			u32 t;

			if (linea(rd, l, sizeof l)) {
				copia(error, "se ha cortado la respuesta", cabe);
				goto fin;
			}
			t = hex(l);
			if (!t)
				break;
			while (t) {
				i32 k = trozo_de(rd, tmp, t < sizeof tmp ? t : sizeof tmp);

				if (k <= 0) {
					copia(error, "se ha cortado la respuesta", cabe);
					goto fin;
				}
				llevado += (u32)k;
				t -= (u32)k;
				if (trozo(u, tmp, (u32)k, 0)) {
					copia(error, "cortado", cabe);
					goto fin;
				}
			}
			linea(rd, l, sizeof l);
		}
	} else {
		while (llevado < largo_) {
			u32 q = largo_ - llevado;
			i32 k = trozo_de(rd, tmp, q < sizeof tmp ? q : sizeof tmp);

			if (k <= 0) {
				if (largo_ == 0xFFFFFFFFu)
					break;
				copia(error, "se ha cortado la descarga", cabe);
				goto fin;
			}
			llevado += (u32)k;
			if (trozo(u, tmp, (u32)k, largo_ == 0xFFFFFFFFu ? 0 : largo_)) {
				copia(error, "cortado", cabe);
				goto fin;
			}
		}
	}
	r = 0;
fin:
	plat_desconecta(rd->c);
	so_suelta(rd);
	return r;
}
