/* prueba_mime.c - Correo: la herramienta de pruebas del núcleo (mime.c,
 * juegos.c, construye.c, html.c, vcard.c) en Linux.
 *   prueba_mime ORDEN ARCHIVO [MÁS]
 * Órdenes: mime, nombres, principal, html, texto, 2047, 2231, dirs, fecha,
 * vcard, construye, juego (ARCHIVO JUEGO), plano, 437, b64, qp.
 * Sale con 0 si va, 1 si el archivo no se lee, 2 si una comprobación falla.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../mime.h"
#include "../html.h"
#include "../vcard.h"

/* ---- SHA-256 (para los adjuntos) ---- */
static const uint32_t K256[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
	0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
	0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
	0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
	0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
	0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
	0xc67178f2};
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_bloque(uint32_t h[8], const uint8_t *p)
{
	uint32_t w[64], a, b, c, d, e, f, g, k, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (i = 16; i < 64; i++)
		w[i] = (ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10)) + w[i - 7] +
		       (ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 16];
	a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
	for (i = 0; i < 64; i++) {
		t1 = k + (ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
		t2 = (ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		k = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
	}
	h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += k;
}

static void sha256(const uint8_t *d, uint32_t n, char *hex)
{
	uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	uint8_t b[128];
	uint32_t i, r;
	uint64_t bits = (uint64_t)n * 8;

	for (i = 0; i + 64 <= n; i += 64)
		sha_bloque(h, d + i);
	r = n - i;
	memset(b, 0, sizeof b);
	if (r)
		memcpy(b, d + i, r);
	b[r] = 0x80;
	r = r < 56 ? 64 : 128;
	for (i = 0; i < 8; i++)
		b[r - 1 - i] = (uint8_t)(bits >> (8 * i));
	sha_bloque(h, b);
	if (r == 128)
		sha_bloque(h, b + 64);
	for (i = 0; i < 8; i++)
		sprintf(hex + 8 * i, "%08x", h[i]);
}

static int fallos;

static void comprueba(int bien, const char *que)
{
	printf("%s %s\n", bien ? "bien:" : "MAL:", que);
	if (!bien)
		fallos++;
}

static void ver(const char *nombre, char *v)
{
	printf("%s: %s\n", nombre, v ? v : "(no está)");
	free(v);
}

static void orden_mime(const u8 *d, u32 n)
{
	Mensaje *m = mime_analiza(d, n);
	int i, k, nh, nt;
	char *s, motivo[200];
	u32 l;

	if (!m) {
		printf("sin memoria\n");
		return;
	}
	printf("partes: %d cortado=%d malformado=%d\n", m->n_partes, m->cortado, m->malformado);
	for (i = 0; i < m->n_partes; i++) {
		const Parte *p = &m->partes[i];

		printf("[%d] %*s%s juego=\"%s\" cod=%d nombre=\"%s\" id=\"%s\" disp=\"%s\" tam=%u adjunto=%d%s\n", i, p->nivel * 2, "",
		       p->tipo, p->juego, p->codificacion, p->nombre, p->id, p->disposicion, p->tam, p->es_adjunto,
		       p->flowed ? " flowed" : "");
	}
	ver("From", mime_cabecera_texto(m, 0, "from"));
	ver("To", mime_cabecera_texto(m, 0, "to"));
	ver("Subject", mime_cabecera_texto(m, 0, "subject"));
	s = mime_cabecera(m, 0, "date", 0);
	printf("Date: %s\n", s ? s : "(no está)");
	if (s) {
		char b[64];
		i64 f = mime_fecha(s, (u32)strlen(s));

		mime_fecha_rfc(f, 60, b, sizeof b);
		printf("fecha: %lld (%s)\n", (long long)f, f ? b : "no se entiende");
	}
	free(s);
	nh = mime_principal(m, 1);
	nt = mime_principal(m, 0);
	printf("principal: html=%d texto=%d\n", nh, nt);
	if (nt >= 0) {
		s = mime_texto(m, nt, &l);
		printf("--- texto (%u) ---\n%s\n--- fin ---\n", l, s ? s : "");
		free(s);
	}
	if (nh >= 0 && nh != nt) {
		s = mime_texto(m, nh, &l);
		printf("--- html (%u) ---\n%s\n--- fin ---\n", l, s ? s : "");
		free(s);
	}
	printf("adjuntos: %d\n", mime_n_adjuntos(m));
	for (k = 0; (i = mime_adjunto(m, k)) >= 0; k++) {
		u8 *c = mime_cuerpo(m, i, &l);
		char h[65];

		sha256(c ? c : (const u8 *)"", c ? l : 0, h);
		printf("adjunto %d: parte=%d nombre=\"%s\" tipo=%s largo=%u sha256=%.16s\n", k, i, m->partes[i].nombre, m->partes[i].tipo,
		       l, h);
		free(c);
	}
	k = mime_suplantacion(m, motivo, sizeof motivo);
	printf("suplantacion: %d %s\n", k, motivo);
	mime_libera(m);
}

/* para comparar con el email de Python: los nombres de los adjuntos */
static void orden_nombres(const u8 *d, u32 n)
{
	Mensaje *m = mime_analiza(d, n);
	int k, i;

	if (!m)
		return;
	for (k = 0; (i = mime_adjunto(m, k)) >= 0; k++)
		if (m->partes[i].nombre[0])
			printf("%s\n", m->partes[i].nombre);
	mime_libera(m);
}

/* y el texto de la principal (text/plain) */
static void orden_principal(const u8 *d, u32 n)
{
	Mensaje *m = mime_analiza(d, n);
	int i;
	u32 l;
	char *s;

	if (!m)
		return;
	i = mime_principal(m, 0);
	if (i >= 0 && (s = mime_texto(m, i, &l)) != NULL) {
		fwrite(s, 1, l, stdout);
		free(s);
	}
	mime_libera(m);
}

static void vuelca_doc(Doc *d)
{
	u32 k, j;
	char *t;
	u32 l;

	if (!d) {
		printf("sin memoria\n");
		return;
	}
	printf("parrafos: %u tramos: %u enlaces: %u imagenes: %u scripts=%d remotas=%d enganosos=%d cortado=%d\n", d->n_parrafos,
	       d->n_tramos, d->n_enlaces, d->n_imagenes, d->habia_scripts, d->remotas, d->enganosos, d->cortado);
	for (k = 0; k < d->n_parrafos && k < 200; k++) {
		const Parrafo *p = &d->parrafos[k];

		printf("P%u n%u h%u a%u:", p->clase, p->nivel, p->hueco, p->alinea);
		for (j = p->ini; j < p->fin; j++) {
			const Tramo *r = &d->tramos[j];

			printf(" [%x", r->estilo);
			if (r->estilo & (E_ENLACE | E_IMAGEN))
				printf("#%u", r->enlace);
			printf("]");
			fwrite(d->texto + r->ini, 1, r->n, stdout);
		}
		printf("\n");
	}
	for (k = 0; k < d->n_enlaces && k < 200; k++) {
		const Enlace *e = &d->enlaces[k];

		printf("enlace %u: href=<%s> visible=<%s> enganoso=%d %s\n", k, e->href, e->visible ? e->visible : "", e->enganoso,
		       e->motivo);
	}
	for (k = 0; k < d->n_imagenes && k < 200; k++) {
		const Imagen *im = &d->imagenes[k];

		printf("imagen %u: src=<%s> alt=<%s> %dx%d remota=%d rastreo=%d\n", k, im->src, im->alt, im->ancho, im->alto, im->remota,
		       im->rastreo);
	}
	t = doc_a_texto(d, &l);
	printf("--- texto ---\n%s--- fin ---\n", t ? t : "");
	free(t);
	doc_libera(d);
}

static void orden_dirs(const u8 *d, u32 n)
{
	Direccion v[64];
	int k, i;

	k = mime_direcciones((const char *)d, n, v, 64);
	printf("direcciones: %d\n", k);
	for (i = 0; i < k; i++) {
		char *c = mime_dir_cabecera(&v[i]);

		printf("nombre=<%s> dir=<%s> valida=%d cabecera=<%s>\n", v[i].nombre, v[i].dir, mime_dir_valida(v[i].dir), c ? c : "");
		free(c);
	}
}

static void orden_fecha(const u8 *d, u32 n)
{
	u32 p = 0;

	while (p < n) {
		const u8 *f = memchr(d + p, '\n', n - p);
		u32 e = f ? (u32)(f - d) : n;
		i64 t = mime_fecha((const char *)d + p, e - p);
		char a[64], b[64], c[64];

		mime_fecha_rfc(t, 60, a, sizeof a);
		mime_fecha_larga(t, 60, b, sizeof b);
		mime_fecha_corta(t, 60, 1795341900, c, sizeof c);
		printf("%.*s => %lld | %s | %s | %s\n", (int)(e - p), (const char *)d + p, (long long)t, t ? a : "-", t ? b : "-",
		       t ? c : "-");
		p = e + 1;
	}
}

static void pon_contacto(const Contacto *c)
{
	printf("nombre=<%s> correos=<%s|%s|%s> tel=<%s> org=<%s> nota=<%s> usado=%u\n", c->nombre, c->correo[0], c->correo[1],
	       c->correo[2], c->telefono, c->organizacion, c->nota, c->usado);
}

static void orden_vcard(const u8 *d, u32 n)
{
	Contacto *v = calloc(64, sizeof(Contacto)), *w = calloc(64, sizeof(Contacto));
	int k, j, i;
	u32 l;
	char *s;

	k = vcard_lee(d, n, v, 64);
	printf("contactos: %d\n", k);
	for (i = 0; i < k; i++)
		pon_contacto(&v[i]);
	s = vcard_escribe(v, k, &l);
	printf("--- escrito ---\n%s--- fin ---\n", s ? s : "");
	if (s) {
		u32 a = 0;
		int largo_bien = 1;

		/* renglones de 75 octetos como mucho */
		while (a < l) {
			const char *f = memchr(s + a, '\n', l - a);
			u32 e = f ? (u32)(f - s) : l;

			if (e - a > 76)
				largo_bien = 0;
			a = e + 1;
		}
		comprueba(largo_bien, "renglones de 75 como mucho");
		j = vcard_lee((const u8 *)s, l, w, 64);
		comprueba(j == k, "los mismos contactos al volverlos a leer");
		for (i = 0; i < k && i < j; i++)
			if (memcmp(&v[i], &w[i], sizeof v[i])) {
				printf("distinto:\n");
				pon_contacto(&w[i]);
				comprueba(0, "ida y vuelta igual");
			}
	}
	free(s);
	free(v);
	free(w);
}

static void orden_construye(const char *ruta)
{
	static const u8 datos[] = "Hola, soy un adjunto en memoria.\n\x00\x01\x02\xff";
	Adjunto adj[3];
	Redactado r;
	u8 *m;
	u32 l;
	char err[200];
	Mensaje *x;

	memset(adj, 0, sizeof adj);
	memset(&r, 0, sizeof r);
	adj[0].datos = datos;
	adj[0].n = sizeof datos - 1;
	adj[0].nombre = "Presupuesto año 2026 — versión definitiva con un nombre muy largo para partirlo.pdf";
	adj[1].ruta = ruta;
	adj[1].nombre = "foto ñandú.jpg";
	adj[2].datos = (const u8 *)"GIF89a";
	adj[2].n = 6;
	adj[2].nombre = "logo.gif";
	adj[2].id = "logo@correo";
	strcpy(r.de.nombre, "Carlos Pérez, el de Albinyana");
	strcpy(r.de.dir, "carlos@ejemplo.es");
	r.para = "Ana Núñez <ana@ejemplo.es>, luis@ejemplo.com";
	r.cc = "\"Sole, la del bingo\" <sole@ejemplo.org>";
	r.asunto = "Reunión del jueves: ¿café o té? — con tildes ÁÉÍÓÚ y € para ver que el asunto largo se parte bien";
	r.texto = "Hola, Ana:\n\nTe envío el presupuesto (€ 1.234,56).\nUn renglón muy largo: "
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa fin.\n"
		  "From aquí empieza un renglón peligroso.\n.\n-- \nCarlos\n";
	r.html = "<p>Hola, <b>Ana</b>:</p><p>Te envío el presupuesto (&euro; 1.234,56).</p><img src=\"cid:logo@correo\">";
	r.adjuntos = adj;
	r.n_adjuntos = 3;
	r.en_respuesta_a = "<abc@ejemplo.es>";
	r.referencias = "<uno@ejemplo.es> <dos@ejemplo.es> <abc@ejemplo.es>";
	r.fecha = 1795341900;
	r.zona = 60;
	r.programa = "Correo 1.0 (Hispa-Pentium)\r\nBcc: inyectado@malo.ejemplo";
	m = mime_construye(&r, &l, err, sizeof err);
	if (!m) {
		printf("no se ha hecho: %s\n", err);
		fallos++;
		return;
	}
	fwrite(m, 1, l, stdout);
	printf("--- fin del mensaje ---\n");
	{
		u32 a = 0;
		int largos = 0, sin_cr = 0;

		while (a < l) {
			const u8 *f = memchr(m + a, '\n', l - a);
			u32 e = f ? (u32)(f - m) : l;

			if (e - a > 998)
				largos++;
			if (f && (e == 0 || m[e - 1] != '\r'))
				sin_cr++;
			a = e + 1;
		}
		comprueba(!largos, "renglones de menos de 998");
		comprueba(!sin_cr, "todos los renglones con CRLF");
		comprueba(!memmem(m, l, "\r\nBcc:", 6), "sin cabeceras inyectadas");
	}
	x = mime_analiza(m, l);
	if (!x) {
		comprueba(0, "se vuelve a analizar");
		free(m);
		return;
	}
	{
		char *s = mime_cabecera_texto(x, 0, "subject");
		int t = mime_principal(x, 0), h = mime_principal(x, 1), k, i;
		char *tx;
		u32 n;

		comprueba(s && !strcmp(s, r.asunto), "el asunto con tildes sale igual");
		free(s);
		s = mime_cabecera_texto(x, 0, "from");
		{
			Direccion d[2];
			char *c = mime_cabecera(x, 0, "from", 0);
			int nd = c ? mime_direcciones(c, (u32)strlen(c), d, 2) : 0;

			comprueba(nd == 1 && !strcmp(d[0].nombre, r.de.nombre) && !strcmp(d[0].dir, r.de.dir), "el remitente sale igual");
			free(c);
			c = mime_cabecera(x, 0, "cc", 0);
			nd = c ? mime_direcciones(c, (u32)strlen(c), d, 2) : 0;
			comprueba(nd == 1 && !strcmp(d[0].nombre, "Sole, la del bingo"), "el nombre con coma sale igual");
			free(c);
		}
		free(s);
		comprueba(t >= 0 && !strcmp(x->partes[t].tipo, "text/plain"), "hay texto");
		comprueba(h >= 0 && !strcmp(x->partes[h].tipo, "text/html"), "hay html");
		tx = mime_texto(x, t, &n);
		comprueba(tx && n == strlen(r.texto) && !memcmp(tx, r.texto, n), "el texto sale igual");
		free(tx);
		comprueba(mime_n_adjuntos(x) == 2, "dos adjuntos (la imagen del HTML no cuenta)");
		for (k = 0; (i = mime_adjunto(x, k)) >= 0; k++) {
			const char *esperado = k == 0 ? adj[0].nombre : adj[1].nombre;

			printf("adjunto %d: \"%s\"\n", k, x->partes[i].nombre);
			comprueba(!strcmp(x->partes[i].nombre, esperado), "el nombre del adjunto sale igual");
			if (k == 0) {
				u8 *c = mime_cuerpo(x, i, &n);

				comprueba(c && n == adj[0].n && !memcmp(c, datos, n), "el adjunto sale igual");
				free(c);
			}
		}
		comprueba(mime_por_id(x, "cid:logo%40correo") >= 0, "la imagen por su cid");
		{
			char *c = mime_cabecera(x, 0, "user-agent", 0);

			comprueba(c && !strchr(c, '\r') && !strchr(c, '\n'), "User-Agent en un renglón");
			free(c);
		}
	}
	mime_libera(x);
	free(m);
	/* un adjunto que no se puede leer */
	adj[1].ruta = "/no/existe/esto.pdf";
	m = mime_construye(&r, &l, err, sizeof err);
	comprueba(!m && strstr(err, "foto"), "el error dice qué adjunto no se lee");
	free(m);
}

int main(int argc, char **argv)
{
	const char *o;
	u8 *d;
	u32 n;

	if (argc < 3) {
		fprintf(stderr, "uso: prueba_mime ORDEN ARCHIVO\n");
		return 1;
	}
	o = argv[1];
	if (!strcmp(o, "construye")) {
		orden_construye(argv[2]);
		return fallos ? 2 : 0;
	}
	d = so_lee_archivo(argv[2], &n, MIME_MAX_MENSAJE);
	if (!d) {
		fprintf(stderr, "no se lee %s\n", argv[2]);
		return 1;
	}
	if (!strcmp(o, "mime"))
		orden_mime(d, n);
	else if (!strcmp(o, "nombres"))
		orden_nombres(d, n);
	else if (!strcmp(o, "principal"))
		orden_principal(d, n);
	else if (!strcmp(o, "html"))
		vuelca_doc(html_a_doc((const char *)d, n));
	else if (!strcmp(o, "texto"))
		vuelca_doc(texto_a_doc((const char *)d, n));
	else if (!strcmp(o, "2047"))
		ver("valor", rfc2047_descodifica((const char *)d, n));
	else if (!strcmp(o, "2231")) {
		ver("valor", mime_valor((const char *)d, n));
		ver("filename", mime_parametro((const char *)d, n, "filename"));
		ver("name", mime_parametro((const char *)d, n, "name"));
	} else if (!strcmp(o, "dirs"))
		orden_dirs(d, n);
	else if (!strcmp(o, "fecha"))
		orden_fecha(d, n);
	else if (!strcmp(o, "vcard"))
		orden_vcard(d, n);
	else if (!strcmp(o, "juego")) {
		u32 l;
		char *u = jc_a_utf8(argc > 3 ? argv[3] : NULL, d, n, &l);

		printf("conocido=%d valido=%d\n", argc > 3 ? jc_conocido(argv[3]) : 0, u ? utf8_valido((u8 *)u, l) : 0);
		if (u)
			fwrite(u, 1, l, stdout);
		printf("\n");
		free(u);
	} else if (!strcmp(o, "plano"))
		ver("plano", utf8_plano((const char *)d, n, NULL));
	else if (!strcmp(o, "437")) {
		char b[4096];
		u32 k, l = utf8_a_437((const char *)d, n, b, sizeof b);

		for (k = 0; k < l; k++)
			printf("%02x", (u8)b[k]);
		printf("\n");
		for (k = 0; k < l; k++)
			if (letra_de_437((u8)b[k]) != '?' || b[k] == '?') {
				char u[4];
				int j = utf8_pon(letra_de_437((u8)b[k]), u);

				fwrite(u, 1, (size_t)j, stdout);
			}
		printf("\n");
	} else if (!strcmp(o, "b64")) {
		u32 l, l2;
		char *c = b64_codifica(d, n, &l, 1);
		u8 *x = c ? b64_descodifica((u8 *)c, l, &l2) : NULL;

		printf("%s", c ? c : "");
		comprueba(x && l2 == n && !memcmp(x, d, n), "base64 ida y vuelta");
		free(c);
		free(x);
	} else if (!strcmp(o, "qp")) {
		u32 l, l2;
		char *c = qp_codifica(d, n, &l);
		u8 *x = c ? qp_descodifica((u8 *)c, l, &l2, 0) : NULL;

		printf("%s\n--- descodificado ---\n", c ? c : "");
		if (x)
			fwrite(x, 1, l2, stdout);
		printf("\n");
		free(c);
		free(x);
	} else {
		fprintf(stderr, "orden desconocida: %s\n", o);
		free(d);
		return 1;
	}
	free(d);
	return fallos ? 2 : 0;
}
