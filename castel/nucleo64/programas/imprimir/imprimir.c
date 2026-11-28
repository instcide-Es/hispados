/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/imprimir/imprimir.c - Hispa-64, etapa 11 a
 * (docs/64/HISPA-64.md, sección 27): IMPRIMIR, imprimir sin CUPS en una
 * impresora de red «sin controlador» (IPP Everywhere, la de casi todas las
 * de hoy), por IPP/1.1 sobre HTTP (RFC 8010 y 8011, PWG 5100.14):
 *   - Get-Printer-Attributes: lo que admite (document-format-supported: PDF,
 *     PWG raster...), las resoluciones y los colores de PWG raster, su nombre
 *     y su estado;
 *   - Print-Job con el PDF tal cual (application/pdf) si lo admite; si no,
 *     con las páginas pintadas por Hispa-PDF (castel/nucleo32/pdf: el mismo
 *     pintor de PDF IMAGENES) a PWG raster (image/pwg-raster, PWG 5102.4:
 *     sRGB o gris de 8 bits, comprimido como pide la norma), a la resolución
 *     que diga la impresora (la más baja de 300 o más, o la más alta);
 *   - la impresora de cada uno, en $HOME/.config/impresora.txt (una línea:
 *     ipp://IP[:631]/ipp/print);
 *   - [si el núcleo deja mandar a 224.0.0.251] buscarlas con mDNS/DNS-SD
 *     (_ipp._tcp.local), con la pregunta «de unicast antiguo» de la RFC 6762
 *     (6.7): desde un puerto que no es el 5353, la respuesta llega por
 *     unicast a ese puerto.
 * Uso:
 *   IMPRIMIR [-i URI] [-t TÍTULO] [-n COPIAS] [-f pdf|pwg] ARCHIVO.PDF
 *   IMPRIMIR -m [-i URI]          lo que dice la impresora
 *   IMPRIMIR -g URI               la guarda como la de siempre
 *   IMPRIMIR -b [DIRECCIÓN]       la busca en la red (mDNS)
 *   IMPRIMIR                      dice cuál es la de siempre
 * Los códigos: 0 bien; 1 mal usado; 2 sin impresora; 3 no se llega a ella;
 * 4 la impresora dice que no; 5 no admite ni PDF ni PWG raster; 6 el
 * archivo no se puede leer o pintar. */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "../../../nucleo32/pdf/hpdf.h"

void hpdf_fuentes(void);

/* los errores más corrientes, en español (los de la biblioteca de C, en inglés) */
static const char *error_es(int e)
{
	switch (e) {
	case ENOENT: return "no está";
	case EACCES: return "no se deja";
	case ECONNREFUSED: return "no contesta en ese puerto (¿está encendida? ¿es ese el puerto?)";
	case ETIMEDOUT: return "no contesta (se ha acabado el plazo)";
	case EAGAIN: return "no contesta (se ha acabado el plazo)";
	case EHOSTUNREACH: return "no se llega a ella (¿está en la misma red?)";
	case ENETUNREACH: return "no hay red";
	case ECONNRESET: return "ha cortado la conexión";
	case EPIPE: return "ha cortado la conexión";
	default: return strerror(e);
	}
}

/* ---- un búfer que crece ---- */
typedef struct {
	unsigned char *d;
	size_t n, cap;
} buf;

static void b_pon(buf *b, const void *d, size_t n)
{
	if (b->n + n > b->cap) {
		size_t c = b->cap ? b->cap * 2 : 4096;

		while (c < b->n + n)
			c *= 2;
		b->d = realloc(b->d, c);
		if (!b->d) {
			fprintf(stderr, "IMPRIMIR: no hay memoria\n");
			exit(6);
		}
		b->cap = c;
	}
	memcpy(b->d + b->n, d, n);
	b->n += n;
}

static void b_1(buf *b, unsigned v)
{
	unsigned char c = (unsigned char)v;

	b_pon(b, &c, 1);
}

static void b_2(buf *b, unsigned v)
{
	b_1(b, v >> 8);
	b_1(b, v);
}

static void b_4(buf *b, uint32_t v)
{
	b_2(b, v >> 16);
	b_2(b, v & 0xFFFF);
}

/* ---- la URI: ipp://NOMBRE[:PUERTO]/RUTA (o http://) ---- */
typedef struct {
	char nombre[256], ruta[512], puerto[8];
	char uri[900];		/* la de printer-uri (ipp://...) */
} destino;

static int lee_uri(const char *u, destino *d)
{
	const char *p, *b, *dp;
	int ipp = 0;
	size_t n;

	memset(d, 0, sizeof *d);
	if (!strncmp(u, "ipp://", 6)) {
		p = u + 6;
		ipp = 1;
	} else if (!strncmp(u, "http://", 7))
		p = u + 7;
	else
		return -1;
	b = strchr(p, '/');
	if (!b)
		b = p + strlen(p);
	/* (IPv6 entre corchetes) */
	if (*p == '[') {
		const char *c = strchr(p, ']');

		if (!c || c > b)
			return -1;
		n = (size_t)(c - p - 1);
		if (n >= sizeof d->nombre)
			return -1;
		memcpy(d->nombre, p + 1, n);
		dp = c[1] == ':' ? c + 2 : NULL;
	} else {
		dp = memchr(p, ':', (size_t)(b - p));
		n = (size_t)((dp ? dp : b) - p);
		if (n == 0 || n >= sizeof d->nombre)
			return -1;
		memcpy(d->nombre, p, n);
		if (dp)
			dp++;
	}
	if (dp && dp < b) {
		n = (size_t)(b - dp);
		if (n >= sizeof d->puerto)
			return -1;
		memcpy(d->puerto, dp, n);
	} else
		snprintf(d->puerto, sizeof d->puerto, "%s", ipp ? "631" : "80");
	snprintf(d->ruta, sizeof d->ruta, "%s", *b ? b : "/ipp/print");
	snprintf(d->uri, sizeof d->uri, "ipp://%s%s%s:%s%s", strchr(d->nombre, ':') ? "[" : "", d->nombre,
		 strchr(d->nombre, ':') ? "]" : "", d->puerto, d->ruta);
	return 0;
}

/* ---- la impresora de siempre ---- */
static const char *archivo_impresora(void)
{
	static char r[600];
	const char *h = getenv("HOME");

	if (!h || !h[0])
		h = "/";
	snprintf(r, sizeof r, "%s%s.config/impresora.txt", h, h[strlen(h) - 1] == '/' ? "" : "/");
	return r;
}

static int lee_impresora(char *u, size_t cabe)
{
	FILE *f = fopen(archivo_impresora(), "r");
	size_t n;

	if (!f)
		return -1;
	if (!fgets(u, (int)cabe, f)) {
		fclose(f);
		return -1;
	}
	fclose(f);
	n = strlen(u);
	while (n && (u[n - 1] == '\n' || u[n - 1] == '\r' || u[n - 1] == ' '))
		u[--n] = 0;
	return n ? 0 : -1;
}

static int guarda_impresora(const char *u)
{
	char c[600], t[640];
	const char *a = archivo_impresora();
	FILE *f;
	char *b;

	snprintf(c, sizeof c, "%s", a);
	b = strrchr(c, '/');
	if (b) {
		*b = 0;
		mkdir(c, 0755);
	}
	snprintf(t, sizeof t, "%s.nuevo", a);
	f = fopen(t, "w");
	if (!f)
		return -1;
	fprintf(f, "%s\n", u);
	if (fclose(f))
		return -1;
	return rename(t, a);
}

/* ---- HTTP: POST de application/ipp y la respuesta entera ---- */
static int conecta(const destino *d)
{
	struct addrinfo h, *r, *x;
	int s = -1, e;

	memset(&h, 0, sizeof h);
	h.ai_socktype = SOCK_STREAM;
	h.ai_family = AF_UNSPEC;
	e = getaddrinfo(d->nombre, d->puerto, &h, &r);
	if (e) {
		fprintf(stderr, "IMPRIMIR: no se encuentra «%s»: %s\n", d->nombre, gai_strerror(e));
		return -1;
	}
	for (x = r; x; x = x->ai_next) {
		s = socket(x->ai_family, x->ai_socktype | SOCK_CLOEXEC, x->ai_protocol);
		if (s < 0)
			continue;
		{
			struct timeval tv = { 30, 0 };

			setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
			setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
		}
		if (connect(s, x->ai_addr, x->ai_addrlen) == 0)
			break;
		close(s);
		s = -1;
	}
	freeaddrinfo(r);
	if (s < 0)
		fprintf(stderr, "IMPRIMIR: no se puede llegar a %s:%s: %s\n", d->nombre, d->puerto, error_es(errno));
	return s;
}

static int manda_todo(int s, const unsigned char *d, size_t n)
{
	while (n) {
		ssize_t k = send(s, d, n, MSG_NOSIGNAL);

		if (k < 0 && errno == EINTR)
			continue;
		if (k <= 0)
			return -1;
		d += k;
		n -= (size_t)k;
	}
	return 0;
}

/* manda la petición (IPP: cabeza y, detrás, el documento) y deja en «r» el
 * cuerpo de la respuesta; el código de HTTP, o -1 */
static int post(const destino *d, const buf *ipp, const unsigned char *doc, size_t ndoc, buf *r)
{
	char cab[1200];
	int s, http = -1, n;
	buf todo = { 0 };
	char *fin, *p;
	size_t cuerpo, largo = (size_t)-1;
	int trozos = 0;

	s = conecta(d);
	if (s < 0)
		return -1;
	n = snprintf(cab, sizeof cab,
		     "POST %s HTTP/1.1\r\nHost: %s:%s\r\nContent-Type: application/ipp\r\nContent-Length: %zu\r\n"
		     "User-Agent: IMPRIMIR (Hispa-64)\r\nConnection: close\r\n\r\n",
		     d->ruta, d->nombre, d->puerto, ipp->n + ndoc);
	if (manda_todo(s, (unsigned char *)cab, (size_t)n) || manda_todo(s, ipp->d, ipp->n) ||
	    (ndoc && manda_todo(s, doc, ndoc))) {
		fprintf(stderr, "IMPRIMIR: no se ha podido mandar a %s: %s\n", d->nombre, error_es(errno));
		close(s);
		return -1;
	}
	for (;;) {
		unsigned char t[16384];
		ssize_t k = recv(s, t, sizeof t, 0);

		if (k < 0 && errno == EINTR)
			continue;
		if (k <= 0)
			break;
		b_pon(&todo, t, (size_t)k);
		if (todo.n > (64u << 20))
			break;
	}
	close(s);
	b_1(&todo, 0);
	todo.n--;
	/* (un «100 Continue» delante, fuera) */
	p = (char *)todo.d;
	while (p && !strncmp(p, "HTTP/1.1 100", 12)) {
		char *f2 = strstr(p, "\r\n\r\n");

		p = f2 ? f2 + 4 : NULL;
	}
	if (!p || strncmp(p, "HTTP/1.", 7) || !(fin = strstr(p, "\r\n\r\n"))) {
		fprintf(stderr, "IMPRIMIR: la respuesta de %s no es de HTTP\n", d->nombre);
		free(todo.d);
		return -1;
	}
	http = atoi(p + 9);
	*fin = 0;
	{
		char *l;

		for (l = strstr(p, "\r\n"); l; l = strstr(l + 2, "\r\n")) {
			if (!strncasecmp(l + 2, "Content-Length:", 15))
				largo = (size_t)strtoul(l + 17, NULL, 10);
			if (!strncasecmp(l + 2, "Transfer-Encoding:", 18) && strstr(l + 2, "chunked"))
				trozos = 1;
		}
	}
	p = fin + 4;
	cuerpo = todo.n - (size_t)(p - (char *)todo.d);
	r->n = 0;
	if (trozos) {
		char *q = p, *lim = p + cuerpo;

		while (q < lim) {
			size_t t = (size_t)strtoul(q, NULL, 16);
			char *e = strstr(q, "\r\n");

			if (!e || t == 0)
				break;
			e += 2;
			if (e + t > lim)
				t = (size_t)(lim - e);
			b_pon(r, e, t);
			q = e + t + 2;
		}
	} else {
		if (largo != (size_t)-1 && largo < cuerpo)
			cuerpo = largo;
		b_pon(r, p, cuerpo);
	}
	free(todo.d);
	return http;
}

/* ---- IPP ---- */
enum {
	G_OPERACION = 0x01, G_TRABAJO = 0x02, G_FIN = 0x03, G_IMPRESORA = 0x04,
	I_ENTERO = 0x21, I_BOOL = 0x22, I_ENUM = 0x23, I_RESOLUCION = 0x32, I_RANGO = 0x33,
	I_TEXTO = 0x41, I_NOMBRE = 0x42, I_PALABRA = 0x44, I_URI = 0x45, I_JUEGO = 0x47,
	I_IDIOMA = 0x48, I_MIME = 0x49
};

static void a_txt(buf *b, int tag, const char *nombre, const char *v)
{
	b_1(b, (unsigned)tag);
	b_2(b, (unsigned)strlen(nombre));
	b_pon(b, nombre, strlen(nombre));
	b_2(b, (unsigned)strlen(v));
	b_pon(b, v, strlen(v));
}

static void a_ent(buf *b, int tag, const char *nombre, uint32_t v)
{
	b_1(b, (unsigned)tag);
	b_2(b, (unsigned)strlen(nombre));
	b_pon(b, nombre, strlen(nombre));
	b_2(b, 4);
	b_4(b, v);
}

static uint32_t petición_n = 1;

static void cabeza(buf *b, unsigned op, const destino *d)
{
	const char *u = getenv("USER");

	b->n = 0;
	b_1(b, 1);		/* IPP/1.1 */
	b_1(b, 1);
	b_2(b, op);
	b_4(b, petición_n++);
	b_1(b, G_OPERACION);
	a_txt(b, I_JUEGO, "attributes-charset", "utf-8");
	a_txt(b, I_IDIOMA, "attributes-natural-language", "es");
	a_txt(b, I_URI, "printer-uri", d->uri);
	a_txt(b, I_NOMBRE, "requesting-user-name", u && u[0] ? u : "hispa64");
}

/* lo de la respuesta: un atributo con sus valores */
typedef struct {
	char nombre[80];
	int tag, n;
	char v[48][128];	/* (los textos; los números, escritos) */
} atributo;

typedef struct {
	int estado;		/* status-code */
	int n;
	atributo a[96];
} respuesta;

static int lee_respuesta(const buf *r, respuesta *x)
{
	size_t i = 8;
	const unsigned char *d = r->d;
	atributo *a = NULL;

	memset(x, 0, sizeof *x);
	if (r->n < 9)
		return -1;
	x->estado = d[2] << 8 | d[3];
	while (i < r->n) {
		int tag = d[i++], nl, vl;

		if (tag == G_FIN)
			return 0;
		if (tag < 0x10)
			continue;	/* un grupo nuevo */
		if (i + 2 > r->n)
			return -1;
		nl = d[i] << 8 | d[i + 1];
		i += 2;
		if (i + (size_t)nl + 2 > r->n)
			return -1;
		if (nl) {
			if (x->n >= (int)(sizeof x->a / sizeof x->a[0])) {
				a = NULL;
			} else {
				a = &x->a[x->n++];
				memset(a, 0, sizeof *a);
				snprintf(a->nombre, sizeof a->nombre, "%.*s", nl < 79 ? nl : 79, (const char *)d + i);
				a->tag = tag;
			}
		}
		i += (size_t)nl;
		vl = d[i] << 8 | d[i + 1];
		i += 2;
		if (i + (size_t)vl > r->n)
			return -1;
		if (a && a->n < 48) {
			char *v = a->v[a->n++];

			if ((tag == I_ENTERO || tag == I_ENUM) && vl == 4)
				snprintf(v, 128, "%d", (int)((uint32_t)d[i] << 24 | (uint32_t)d[i + 1] << 16 | (uint32_t)d[i + 2] << 8 | d[i + 3]));
			else if (tag == I_BOOL && vl == 1)
				snprintf(v, 128, "%s", d[i] ? "sí" : "no");
			else if (tag == I_RESOLUCION && vl == 9) {
				int xr = (int)((uint32_t)d[i] << 24 | (uint32_t)d[i + 1] << 16 | (uint32_t)d[i + 2] << 8 | d[i + 3]);
				int yr = (int)((uint32_t)d[i + 4] << 24 | (uint32_t)d[i + 5] << 16 | (uint32_t)d[i + 6] << 8 | d[i + 7]);

				snprintf(v, 128, "%dx%d%s", xr, yr, d[i + 8] == 3 ? "dpi" : "dpcm");
			} else if (tag >= 0x40)
				snprintf(v, 128, "%.*s", vl < 127 ? vl : 127, (const char *)d + i);
			else
				snprintf(v, 128, "(%d octetos)", vl);
		}
		i += (size_t)vl;
	}
	return 0;
}

static const atributo *busca(const respuesta *x, const char *n)
{
	int i;

	for (i = 0; i < x->n; i++)
		if (!strcmp(x->a[i].nombre, n))
			return &x->a[i];
	return NULL;
}

static int tiene(const respuesta *x, const char *n, const char *v)
{
	const atributo *a = busca(x, n);
	int i;

	for (i = 0; a && i < a->n; i++)
		if (!strcasecmp(a->v[i], v))
			return 1;
	return 0;
}

static const char *estado_ipp(int e)
{
	switch (e) {
	case 0x0000: return "bien";
	case 0x0001: return "bien (sin algunos atributos)";
	case 0x0002: return "bien (con atributos en conflicto)";
	case 0x0400: return "la petición está mal";
	case 0x0401: return "no se deja";
	case 0x0402: return "hay que identificarse";
	case 0x0403: return "no se puede";
	case 0x0404: return "no encontrada";
	case 0x040A: return "el formato del documento no lo admite";
	case 0x040B: return "atributos que no admite";
	case 0x0500: return "error de la impresora";
	case 0x0501: return "la operación no la admite";
	case 0x0507: return "no acepta trabajos ahora";
	case 0x0508: return "está ocupada";
	default: return "error";
	}
}

static int mira(const destino *d, respuesta *x)
{
	buf b = { 0 }, r = { 0 };
	int h;
	static const char *pide[] = { "printer-name", "printer-make-and-model", "printer-info", "printer-state",
				      "printer-state-reasons", "document-format-supported", "pwg-raster-document-resolution-supported",
				      "pwg-raster-document-type-supported", "media-default", "printer-is-accepting-jobs" };
	unsigned i;

	cabeza(&b, 0x000B, d);
	for (i = 0; i < sizeof pide / sizeof pide[0]; i++)
		a_txt(&b, I_PALABRA, i ? "" : "requested-attributes", pide[i]);
	b_1(&b, G_FIN);
	h = post(d, &b, NULL, 0, &r);
	free(b.d);
	if (h < 0) {
		free(r.d);
		return 3;
	}
	if (h != 200) {
		fprintf(stderr, "IMPRIMIR: %s contesta con HTTP %d (¿es una impresora IPP? la ruta suele ser /ipp/print)\n", d->nombre, h);
		free(r.d);
		return 3;
	}
	if (lee_respuesta(&r, x)) {
		fprintf(stderr, "IMPRIMIR: la respuesta IPP de %s está mal hecha\n", d->nombre);
		free(r.d);
		return 4;
	}
	free(r.d);
	if (x->estado >= 0x0400) {
		fprintf(stderr, "IMPRIMIR: la impresora dice: %s (0x%04X)\n", estado_ipp(x->estado), x->estado);
		return 4;
	}
	return 0;
}

static void dice_impresora(const respuesta *x)
{
	const atributo *a;
	int i;

	a = busca(x, "printer-make-and-model");
	if (!a)
		a = busca(x, "printer-name");
	printf("IMPRIMIR: la impresora: %s", a && a->n ? a->v[0] : "(sin nombre)");
	a = busca(x, "printer-state");
	if (a && a->n)
		printf(" (%s)", !strcmp(a->v[0], "3") ? "lista" : !strcmp(a->v[0], "4") ? "imprimiendo" : !strcmp(a->v[0], "5") ? "parada" : a->v[0]);
	printf("\n");
	a = busca(x, "document-format-supported");
	printf("  formatos:");
	for (i = 0; a && i < a->n; i++)
		printf(" %s", a->v[i]);
	printf("\n");
	a = busca(x, "pwg-raster-document-resolution-supported");
	if (a) {
		printf("  PWG raster, resoluciones:");
		for (i = 0; i < a->n; i++)
			printf(" %s", a->v[i]);
		printf("\n");
	}
	a = busca(x, "pwg-raster-document-type-supported");
	if (a) {
		printf("  PWG raster, colores:");
		for (i = 0; i < a->n; i++)
			printf(" %s", a->v[i]);
		printf("\n");
	}
}

/* ---- PWG raster (PWG 5102.4) desde las páginas pintadas por Hispa-PDF ---- */
static void h4(unsigned char *h, int o, uint32_t v)
{
	h[o] = (unsigned char)(v >> 24);
	h[o + 1] = (unsigned char)(v >> 16);
	h[o + 2] = (unsigned char)(v >> 8);
	h[o + 3] = (unsigned char)v;
}

/* una fila (de «n» puntos de «bp» octetos) comprimida: un octeto 0..127,
 * el punto que sigue, repetido (octeto + 1) veces; 129..255, que siguen
 * (257 - octeto) puntos tal cual */
static void pwg_fila(buf *b, const unsigned char *f, int n, int bp)
{
	int i = 0;

	while (i < n) {
		int r = 1;

		while (i + r < n && r < 128 && !memcmp(f + (size_t)(i + r) * bp, f + (size_t)i * bp, (size_t)bp))
			r++;
		if (r > 1) {
			b_1(b, (unsigned)(r - 1));
			b_pon(b, f + (size_t)i * bp, (size_t)bp);
			i += r;
		} else {
			int k = 1;

			while (i + k < n && k < 128 &&
			       (i + k + 1 >= n || memcmp(f + (size_t)(i + k) * bp, f + (size_t)(i + k + 1) * bp, (size_t)bp)))
				k++;
			if (k == 1) {
				b_1(b, 0);
			} else {
				b_1(b, (unsigned)(257 - k));
			}
			b_pon(b, f + (size_t)i * bp, (size_t)k * bp);
			i += k;
		}
	}
}

/* la página (an x al puntos a «ppp»; el papel, A4) */
static void pwg_pagina(buf *b, const pimagen *im, int ppp, int gris, int copias, uint32_t total)
{
	unsigned char h[1796];
	int an = (int)(210.0 / 25.4 * ppp + 0.5), al = (int)(297.0 / 25.4 * ppp + 0.5);
	int bp = gris ? 1 : 3, y, x;
	unsigned char *fila = malloc((size_t)an * bp), *antes = malloc((size_t)an * bp);
	int hay_antes = 0, rep = 0;

	memset(h, 0, sizeof h);
	memcpy(h, "PwgRaster", 9);				/* (la marca de PWG 5102.4) */
	h4(h, 276, (uint32_t)ppp);				/* HWResolution */
	h4(h, 280, (uint32_t)ppp);
	h4(h, 340, (uint32_t)copias);				/* NumCopies */
	h4(h, 352, 595);					/* PageSize, en puntos */
	h4(h, 356, 842);
	h4(h, 372, (uint32_t)an);				/* Width */
	h4(h, 376, (uint32_t)al);				/* Height */
	h4(h, 384, 8);						/* BitsPerColor */
	h4(h, 388, (uint32_t)bp * 8);				/* BitsPerPixel */
	h4(h, 392, (uint32_t)(an * bp));			/* BytesPerLine */
	h4(h, 396, 0);						/* ColorOrder: chunky */
	h4(h, 400, gris ? 18 : 19);				/* ColorSpace: sGray o sRGB */
	h4(h, 420, (uint32_t)bp);				/* NumColors */
	h4(h, 452, total);					/* TotalPageCount */
	h4(h, 456, 1);						/* CrossFeedTransform */
	h4(h, 460, 1);						/* FeedTransform */
	snprintf((char *)h + 1732, 64, "%s", "iso_a4_210x297mm");	/* PageSizeName */
	b_pon(b, h, sizeof h);
	/* las filas: lo pintado, en medio del papel (lo que sobre, blanco) */
	for (y = 0; y < al; y++) {
		int sy = y - (al - im->al) / 2;

		memset(fila, 0xFF, (size_t)an * bp);
		if (sy >= 0 && sy < im->al) {
			int dx = (an - im->an) / 2;

			for (x = 0; x < im->an; x++) {
				int tx = x + dx;
				uint32_t c = im->px[(size_t)sy * im->an + x];

				if (tx < 0 || tx >= an)
					continue;
				if (gris)
					fila[tx] = (unsigned char)((((c >> 16) & 255) * 299 + ((c >> 8) & 255) * 587 + (c & 255) * 114) / 1000);
				else {
					fila[tx * 3] = (unsigned char)(c >> 16);
					fila[tx * 3 + 1] = (unsigned char)(c >> 8);
					fila[tx * 3 + 2] = (unsigned char)c;
				}
			}
		}
		/* (las filas iguales seguidas, juntas: hasta 256) */
		if (hay_antes && rep < 255 && !memcmp(fila, antes, (size_t)an * bp)) {
			rep++;
			continue;
		}
		if (hay_antes) {
			b_1(b, (unsigned)rep);
			pwg_fila(b, antes, an, bp);
		}
		memcpy(antes, fila, (size_t)an * bp);
		hay_antes = 1;
		rep = 0;
	}
	if (hay_antes) {
		b_1(b, (unsigned)rep);
		pwg_fila(b, antes, an, bp);
	}
	free(fila);
	free(antes);
}

static int a_pwg(const char *ruta, int ppp, int gris, int copias, buf *b, int *paginas)
{
	const char *err = NULL;
	int nc = 0;
	pdoc *d = pdf_abre_archivo(ruta, NULL, &nc, &err);
	u32 i;

	if (!d) {
		fprintf(stderr, "IMPRIMIR: %s: %s\n", ruta, nc ? "lleva contraseña" : err ? err : "no se puede abrir");
		return 6;
	}
	if (!d->n_paginas) {
		fprintf(stderr, "IMPRIMIR: %s: no tiene páginas\n", ruta);
		pdf_cierra(d);
		return 6;
	}
	b_pon(b, "RaS2", 4);
	for (i = 0; i < d->n_paginas; i++) {
		/* (que quepa en A4: a lo más su lado largo, 297 mm) */
		pimagen *im = pinta_pagina(d, i, ppp, 0);

		if (im && (im->an > (int)(210.0 / 25.4 * ppp + 1) || im->al > (int)(297.0 / 25.4 * ppp + 1))) {
			int lado = (int)(297.0 / 25.4 * ppp);

			imagen_suelta(im);
			im = pinta_pagina(d, i, ppp, lado);
		}
		if (!im) {
			fprintf(stderr, "IMPRIMIR: %s: la página %u no se ha podido pintar\n", ruta, (unsigned)i + 1);
			pdf_cierra(d);
			return 6;
		}
		pwg_pagina(b, im, ppp, gris, copias, d->n_paginas);
		imagen_suelta(im);
	}
	*paginas = (int)d->n_paginas;
	pdf_cierra(d);
	return 0;
}

/* ---- imprimir ---- */
static unsigned char *lee(const char *ruta, size_t *n)
{
	FILE *f = fopen(ruta, "rb");
	unsigned char *d;
	long l;

	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	l = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (l < 0 || l > (512L << 20)) {
		fclose(f);
		return NULL;
	}
	d = malloc((size_t)l + 1);
	if (d && l && fread(d, 1, (size_t)l, f) != (size_t)l) {
		free(d);
		d = NULL;
	}
	fclose(f);
	*n = (size_t)l;
	return d;
}

static int imprime(const destino *d, const char *ruta, const char *título, int copias, const char *forzado)
{
	respuesta *x = calloc(1, sizeof *x);
	buf b = { 0 }, r = { 0 }, doc = { 0 };
	unsigned char *pdf;
	size_t n;
	int e, h, pdf_sí, pwg_sí, ppp = 0, gris = 0, paginas = 0;
	const char *fmt;
	const atributo *a;

	pdf = lee(ruta, &n);
	if (!pdf || n < 5 || memcmp(pdf, "%PDF-", 5)) {
		fprintf(stderr, "IMPRIMIR: %s: %s\n", ruta, pdf ? "no es un PDF" : error_es(errno));
		free(pdf);
		free(x);
		return 6;
	}
	e = mira(d, x);
	if (e) {
		free(pdf);
		free(x);
		return e;
	}
	pdf_sí = tiene(x, "document-format-supported", "application/pdf");
	pwg_sí = tiene(x, "document-format-supported", "image/pwg-raster");
	if (forzado && !strcmp(forzado, "pwg"))
		pdf_sí = 0;
	if (forzado && !strcmp(forzado, "pdf"))
		pwg_sí = 0;
	if (!pdf_sí && !pwg_sí) {
		fprintf(stderr, "IMPRIMIR: la impresora no admite PDF ni PWG raster (¿no es «sin controlador», IPP Everywhere?)\n");
		free(pdf);
		free(x);
		return 5;
	}
	if (pdf_sí) {
		fmt = "application/pdf";
		b_pon(&doc, pdf, n);
	} else {
		int i, mejor = 0, alta = 0;

		fmt = "image/pwg-raster";
		/* la resolución: la más baja de 300 o más (si no, la más alta) */
		a = busca(x, "pwg-raster-document-resolution-supported");
		for (i = 0; a && i < a->n; i++) {
			int v = atoi(a->v[i]);

			if (strstr(a->v[i], "dpcm"))
				v = (int)(v * 2.54 + 0.5);
			if (v >= 300 && (!mejor || v < mejor))
				mejor = v;
			if (v > alta)
				alta = v;
		}
		ppp = mejor ? mejor : alta ? alta : 300;
		if (ppp > 600)
			ppp = 600;
		/* (en color si lo admite o si no dice; en gris si solo admite gris) */
		gris = tiene(x, "pwg-raster-document-type-supported", "sgray_8") && !tiene(x, "pwg-raster-document-type-supported", "srgb_8");
		hpdf_fuentes();
		e = a_pwg(ruta, ppp, gris, copias, &doc, &paginas);
		if (e) {
			free(pdf);
			free(doc.d);
			free(x);
			return e;
		}
	}
	free(pdf);
	cabeza(&b, 0x0002, d);
	a_txt(&b, I_NOMBRE, "job-name", título);
	a_txt(&b, I_MIME, "document-format", fmt);
	if (copias > 1 && pdf_sí) {
		b_1(&b, G_TRABAJO);
		a_ent(&b, I_ENTERO, "copies", (uint32_t)copias);
	}
	b_1(&b, G_FIN);
	h = post(d, &b, doc.d, doc.n, &r);
	free(b.d);
	if (h != 200) {
		if (h > 0)
			fprintf(stderr, "IMPRIMIR: %s contesta con HTTP %d\n", d->nombre, h);
		free(r.d);
		free(doc.d);
		free(x);
		return 3;
	}
	if (lee_respuesta(&r, x) || x->estado >= 0x0400) {
		fprintf(stderr, "IMPRIMIR: la impresora no lo acepta: %s (0x%04X)\n", estado_ipp(x->estado), x->estado);
		free(r.d);
		free(doc.d);
		free(x);
		return 4;
	}
	a = busca(x, "job-id");
	if (pdf_sí)
		printf("IMPRIMIR: mandado a %s: trabajo %s, en PDF (%zu octetos)\n", d->uri, a && a->n ? a->v[0] : "?", doc.n);
	else
		printf("IMPRIMIR: mandado a %s: trabajo %s, en PWG raster (%d página%s a %d ppp, %s; %zu octetos)\n", d->uri,
		       a && a->n ? a->v[0] : "?", paginas, paginas == 1 ? "" : "s", ppp, gris ? "en gris" : "en color", doc.n);
	free(r.d);
	free(doc.d);
	free(x);
	return 0;
}

/* ---- buscar con mDNS (DNS-SD, _ipp._tcp.local) ---- */
static int nombre_dns(const unsigned char *m, int n, int i, char *d, int cabe, int *sig)
{
	int k = 0, saltos = 0, fin = -1;

	while (i < n) {
		int l = m[i];

		if (l == 0) {
			i++;
			break;
		}
		if ((l & 0xC0) == 0xC0) {
			if (i + 1 >= n || ++saltos > 16)
				return -1;
			if (fin < 0)
				fin = i + 2;
			i = (l & 0x3F) << 8 | m[i + 1];
			continue;
		}
		i++;
		if (i + l > n)
			return -1;
		if (k && k < cabe - 1)
			d[k++] = '.';
		while (l > 0) {
			if (k < cabe - 1)
				d[k++] = (char)m[i];
			i++;
			l--;
		}
	}
	d[k] = 0;
	*sig = fin >= 0 ? fin : i;
	return 0;
}

static int busca_red(const char *dir)
{
	int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0), n, vistas = 0;
	struct sockaddr_in a;
	unsigned char q[64], m[4096];
	static const unsigned char nombre[] = "\4_ipp\4_tcp\5local";
	struct pollfd p;
	time_t hasta = time(NULL) + 3;
	unsigned char ttl = 255;

	if (s < 0)
		return 3;
	setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);
	memset(q, 0, 12);
	q[5] = 1;			/* una pregunta */
	memcpy(q + 12, nombre, sizeof nombre);
	n = 12 + (int)sizeof nombre;
	q[n++] = 0;
	q[n++] = 12;			/* PTR */
	q[n++] = 0x80;			/* QU: que conteste por unicast */
	q[n++] = 1;
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons(5353);
	a.sin_addr.s_addr = inet_addr(dir);
	if (sendto(s, q, (size_t)n, 0, (struct sockaddr *)&a, sizeof a) < 0) {
		fprintf(stderr, "IMPRIMIR: no se puede preguntar a %s (mDNS): %s\n", dir, error_es(errno));
		close(s);
		return 3;
	}
	p.fd = s;
	p.events = POLLIN;
	while (time(NULL) < hasta) {
		int k, i, qd, an, j;
		char ins[256] = "", objetivo[256] = "", ruta[256] = "/ipp/print", ip[64] = "";
		int puerto = 631;

		if (poll(&p, 1, 500) <= 0)
			continue;
		k = (int)recv(s, m, sizeof m, 0);
		if (k < 12)
			continue;
		qd = m[4] << 8 | m[5];
		an = (m[6] << 8 | m[7]) + (m[8] << 8 | m[9]) + (m[10] << 8 | m[11]);
		i = 12;
		for (j = 0; j < qd && i < k; j++) {
			char t[256];

			if (nombre_dns(m, k, i, t, sizeof t, &i))
				break;
			i += 4;
		}
		for (j = 0; j < an && i + 10 <= k; j++) {
			char t[256];
			int tipo, l, dato;

			if (nombre_dns(m, k, i, t, sizeof t, &i) || i + 10 > k)
				break;
			tipo = m[i] << 8 | m[i + 1];
			l = m[i + 8] << 8 | m[i + 9];
			dato = i + 10;
			if (dato + l > k)
				break;
			if (tipo == 12) {
				int z;

				nombre_dns(m, k, dato, ins, sizeof ins, &z);
			} else if (tipo == 33 && l > 6) {
				int z;

				puerto = m[dato + 4] << 8 | m[dato + 5];
				nombre_dns(m, k, dato + 6, objetivo, sizeof objetivo, &z);
			} else if (tipo == 16) {
				int z = dato;

				while (z < dato + l) {
					int tl = m[z++];

					if (tl >= 3 && !memcmp(m + z, "rp=", 3) && z + tl <= dato + l)
						snprintf(ruta, sizeof ruta, "/%.*s", tl - 3, (const char *)m + z + 3);
					z += tl;
				}
			} else if (tipo == 1 && l == 4)
				snprintf(ip, sizeof ip, "%u.%u.%u.%u", m[dato], m[dato + 1], m[dato + 2], m[dato + 3]);
			i = dato + l;
		}
		if (ins[0]) {
			printf("IMPRIMIR: en la red: «%s»: ipp://%s:%d%s\n", ins, ip[0] ? ip : objetivo, puerto, ruta);
			vistas++;
		}
	}
	close(s);
	if (!vistas) {
		printf("IMPRIMIR: no se ha encontrado ninguna impresora en la red (mDNS)\n");
		return 2;
	}
	return 0;
}

static void uso(void)
{
	fprintf(stderr,
		"Uso: IMPRIMIR [-i URI] [-t TÍTULO] [-n COPIAS] [-f pdf|pwg] ARCHIVO.PDF\n"
		"     IMPRIMIR -m [-i URI]     lo que dice la impresora\n"
		"     IMPRIMIR -g URI          la guarda como la de siempre (%s)\n"
		"     IMPRIMIR -b [DIRECCIÓN]  la busca en la red (mDNS)\n"
		"La URI: ipp://IP[:631]/ipp/print\n",
		archivo_impresora());
}

int main(int argc, char **argv)
{
	const char *uri = NULL, *título = NULL, *archivo = NULL, *forzado = NULL;
	char guardada[600];
	int copias = 1, ver = 0, i;
	destino d;

	setvbuf(stdout, NULL, _IOLBF, 0);
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-i") && i + 1 < argc)
			uri = argv[++i];
		else if (!strcmp(argv[i], "-t") && i + 1 < argc)
			título = argv[++i];
		else if (!strcmp(argv[i], "-n") && i + 1 < argc)
			copias = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-f") && i + 1 < argc)
			forzado = argv[++i];
		else if (!strcmp(argv[i], "-m"))
			ver = 1;
		else if (!strcmp(argv[i], "-b"))
			return busca_red(i + 1 < argc ? argv[i + 1] : "224.0.0.251");
		else if (!strcmp(argv[i], "-g") && i + 1 < argc) {
			if (lee_uri(argv[i + 1], &d)) {
				fprintf(stderr, "IMPRIMIR: «%s» no es una URI de impresora (ipp://IP[:631]/ipp/print)\n", argv[i + 1]);
				return 1;
			}
			if (guarda_impresora(d.uri)) {
				fprintf(stderr, "IMPRIMIR: no se puede guardar en %s: %s\n", archivo_impresora(), error_es(errno));
				return 6;
			}
			printf("IMPRIMIR: la impresora de siempre: %s (en %s)\n", d.uri, archivo_impresora());
			return 0;
		} else if (argv[i][0] == '-' && argv[i][1]) {
			uso();
			return 1;
		} else
			archivo = argv[i];
	}
	if (copias < 1 || copias > 99)
		copias = 1;
	if (!uri) {
		if (lee_impresora(guardada, sizeof guardada)) {
			if (!archivo && !ver) {
				printf("IMPRIMIR: no hay impresora de siempre (se pone con IMPRIMIR -g ipp://IP/ipp/print)\n");
				return 2;
			}
			fprintf(stderr, "IMPRIMIR: no hay impresora: diga cuál con -i ipp://IP/ipp/print (o guárdela con -g)\n");
			return 2;
		}
		uri = guardada;
	}
	if (lee_uri(uri, &d)) {
		fprintf(stderr, "IMPRIMIR: «%s» no es una URI de impresora (ipp://IP[:631]/ipp/print)\n", uri);
		return 1;
	}
	if (ver || !archivo) {
		respuesta *x = calloc(1, sizeof *x);
		int e;

		if (!ver)
			printf("IMPRIMIR: la impresora de siempre: %s\n", d.uri);
		e = mira(&d, x);
		if (!e)
			dice_impresora(x);
		free(x);
		return e;
	}
	if (!título) {
		título = strrchr(archivo, '/') ? strrchr(archivo, '/') + 1 : archivo;
	}
	return imprime(&d, archivo, título, copias, forzado);
}
