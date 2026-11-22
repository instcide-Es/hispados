/* construye.c - Correo: hacer un mensaje para enviarlo (y para los
 * borradores): las cabeceras (RFC 5322, con RFC 2047), el texto en
 * quoted-printable si hace falta, el HTML en multipart/alternative, las
 * imágenes del HTML en multipart/related, los adjuntos en base64 con los
 * nombres en RFC 2231 y en RFC 2047; los Message-ID y los tipos por la
 * extensión.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "mime.h"
#include "interno.h"

#define MAX_DESTINOS 256

/* ---- los tipos por la extensión ---- */
static const struct {
	const char *ext, *tipo;
} TIPOS[] = {
	{"7z", "application/x-7z-compressed"}, {"aac", "audio/aac"}, {"avi", "video/x-msvideo"},
	{"bmp", "image/bmp"}, {"bz2", "application/x-bzip2"}, {"c", "text/plain"}, {"css", "text/css"},
	{"csv", "text/csv"}, {"doc", "application/msword"},
	{"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
	{"eml", "message/rfc822"}, {"epub", "application/epub+zip"}, {"flac", "audio/flac"}, {"gif", "image/gif"},
	{"gz", "application/gzip"}, {"h", "text/plain"}, {"heic", "image/heic"}, {"htm", "text/html"},
	{"html", "text/html"}, {"ico", "image/x-icon"}, {"ics", "text/calendar"}, {"jpeg", "image/jpeg"},
	{"jpg", "image/jpeg"}, {"js", "text/javascript"}, {"json", "application/json"}, {"log", "text/plain"},
	{"m4a", "audio/mp4"}, {"md", "text/markdown"}, {"mid", "audio/midi"}, {"mkv", "video/x-matroska"},
	{"mov", "video/quicktime"}, {"mp3", "audio/mpeg"}, {"mp4", "video/mp4"}, {"mpeg", "video/mpeg"},
	{"mpg", "video/mpeg"}, {"odg", "application/vnd.oasis.opendocument.graphics"},
	{"odp", "application/vnd.oasis.opendocument.presentation"},
	{"ods", "application/vnd.oasis.opendocument.spreadsheet"},
	{"odt", "application/vnd.oasis.opendocument.text"}, {"oga", "audio/ogg"}, {"ogg", "audio/ogg"},
	{"ogv", "video/ogg"}, {"opus", "audio/opus"}, {"pdf", "application/pdf"}, {"png", "image/png"},
	{"ppt", "application/vnd.ms-powerpoint"},
	{"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
	{"rar", "application/vnd.rar"}, {"rtf", "application/rtf"}, {"svg", "image/svg+xml"},
	{"tar", "application/x-tar"}, {"tif", "image/tiff"}, {"tiff", "image/tiff"}, {"txt", "text/plain"},
	{"vcf", "text/vcard"}, {"wav", "audio/wav"}, {"webm", "video/webm"}, {"webp", "image/webp"},
	{"xls", "application/vnd.ms-excel"},
	{"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
	{"xml", "application/xml"}, {"zip", "application/zip"},
};

const char *mime_tipo_de(const char *nombre)
{
	const char *p = nombre ? strrchr(nombre, '.') : NULL;
	u32 k;

	if (!p || !p[1])
		return "application/octet-stream";
	for (k = 0; k < sizeof TIPOS / sizeof TIPOS[0]; k++)
		if (cr_igual(p + 1, TIPOS[k].ext))
			return TIPOS[k].tipo;
	return "application/octet-stream";
}

/* ---- los Message-ID ---- */
void mime_nuevo_id(const char *dominio, char *d, int cabe)
{
	static const char cifras[] = "0123456789abcdefghijklmnopqrstuv";
	u8 az[15];
	char a[32], dom[128];
	int k, j = 0;
	u32 i;

	so_azar(az, sizeof az);
	for (k = 0; k < 24; k++) {
		/* 5 bits cada letra */
		u32 bit = (u32)k * 5, v = ((u32)az[bit / 8] << 8 | (bit / 8 + 1 < sizeof az ? az[bit / 8 + 1] : 0)) >> (11 - bit % 8);

		a[k] = cifras[v & 31];
	}
	a[24] = 0;
	if (dominio)
		for (i = 0; dominio[i] && j < (int)sizeof dom - 1; i++) {
			char c = dominio[i];

			if (es_alnum((u8)c) || c == '.' || c == '-')
				dom[j++] = (char)minuscula((u8)c);
		}
	dom[j] = 0;
	if (!j || dom[0] == '.' || !strchr(dom, '.'))
		strcpy(dom, "correo.invalid");
	snprintf(d, (size_t)cabe, "<%s@%s>", a, dom);
}

/* ---- hacer ---- */
typedef struct {
	Texto t;
	char *error;
	int cabe_error;
} Hacer;

/* un texto de una cabecera sin saltos de renglón (lo del usuario) */
static char *limpia(const char *s)
{
	u32 n = s ? (u32)strlen(s) : 0, i;
	char *r = cr_copia(s ? s : "", n);

	if (r)
		for (i = 0; i < n; i++)
			if ((u8)r[i] < 0x20 || r[i] == 0x7f)
				r[i] = ' ';
	return r;
}

static void cabecera_texto(Texto *t, const char *nombre, const char *valor)
{
	char *l = limpia(valor), *c;

	if (!l) {
		t->malo = 1;
		return;
	}
	c = rfc2047_codifica(l, (u32)strlen(nombre) + 2, 0);
	so_suelta(l);
	if (!c) {
		t->malo = 1;
		return;
	}
	tx_s(t, nombre);
	tx_s(t, ": ");
	tx_s(t, c);
	tx_s(t, "\r\n");
	so_suelta(c);
}

/* una cabecera de direcciones («Ana <a@b.es>, c@d.es»): 0 si no hay ninguna */
static int cabecera_dirs(Texto *t, const char *nombre, const char *valor)
{
	Direccion *v;
	int n, k, puestas = 0;
	u32 col;

	if (!valor || !valor[0])
		return 0;
	v = so_pide(sizeof(Direccion) * MAX_DESTINOS);
	if (!v) {
		t->malo = 1;
		return 0;
	}
	n = mime_direcciones(valor, (u32)strlen(valor), v, MAX_DESTINOS);
	for (k = 0; k < n; k++) {
		char *c;
		u32 l;

		if (!v[k].dir[0])
			continue;
		{
			char *s;

			for (s = v[k].dir; *s; s++)
				if ((u8)*s <= 32 || *s == 0x7f || *s == '<' || *s == '>' || *s == ',')
					*s = '_';
			for (s = v[k].nombre; *s; s++)
				if ((u8)*s < 0x20 || *s == 0x7f)
					*s = ' ';
		}
		c = mime_dir_cabecera(&v[k]);
		if (!c) {
			t->malo = 1;
			break;
		}
		l = (u32)strlen(c);
		if (!puestas) {
			tx_s(t, nombre);
			tx_s(t, ": ");
			col = (u32)strlen(nombre) + 2;
		} else {
			tx_c(t, ',');
			col++;
			if (col + 1 + l > 76) {
				tx_s(t, "\r\n ");
				col = 1;
			} else {
				tx_c(t, ' ');
				col++;
			}
		}
		tx_s(t, c);
		{
			const char *ult = strrchr(c, '\n');

			col = ult ? (u32)strlen(ult + 1) : col + l;
		}
		so_suelta(c);
		puestas++;
	}
	if (puestas)
		tx_s(t, "\r\n");
	so_suelta(v);
	return puestas;
}

static void frontera_nueva(char *d, int nivel)
{
	u8 az[12];
	int k;

	so_azar(az, sizeof az);
	strcpy(d, "----=_Correo_");
	for (k = 0; k < 12; k++) {
		d[13 + 2 * k] = "0123456789ABCDEF"[az[k] >> 4];
		d[14 + 2 * k] = "0123456789ABCDEF"[az[k] & 15];
	}
	d[37] = '_';
	d[38] = (char)('0' + nivel);
	d[39] = 0;
}

/* ¿hace falta quoted-printable? (8 bits, controles o renglones de más de 900) */
static int pide_qp(const char *s, u32 n)
{
	u32 i, col = 0;

	for (i = 0; i < n; i++) {
		u8 c = (u8)s[i];

		if (c == '\n') {
			col = 0;
			continue;
		}
		if (c >= 0x80 || (c < 0x20 && c != '\t' && c != '\r') || c == 0x7f)
			return 1;
		if (++col > 900)
			return 1;
		if (col == 1 && c == '-' && i + 1 < n && s[i + 1] == '-')
			return 1;	/* que no parezca una frontera */
	}
	return 0;
}

static void parte_texto(Texto *t, const char *tipo, const char *s)
{
	u32 n = s ? (u32)strlen(s) : 0, i;

	tx_s(t, "Content-Type: ");
	tx_s(t, tipo);
	tx_s(t, "; charset=UTF-8\r\n");
	if (pide_qp(s, n)) {
		u32 l;
		char *q = qp_codifica((const u8 *)s, n, &l);

		if (!q) {
			t->malo = 1;
			return;
		}
		tx_s(t, "Content-Transfer-Encoding: quoted-printable\r\n\r\n");
		tx_pon(t, q, l);
		so_suelta(q);
	} else {
		tx_s(t, "Content-Transfer-Encoding: 7bit\r\n\r\n");
		for (i = 0; i < n; i++) {
			if (s[i] == '\r')
				continue;
			if (s[i] == '\n')
				tx_s(t, "\r\n");
			else
				tx_c(t, s[i]);
		}
	}
	if (t->n < 2 || t->d[t->n - 1] != '\n')
		tx_s(t, "\r\n");
}

/* filename*=UTF-8''... (partido en trozos de RFC 2231 si es largo) */
static void nombre_2231(Texto *t, const char *nombre)
{
	Texto p;
	u32 i, n = (u32)strlen(nombre), ini, k;

	tx_ini(&p, 0);
	for (i = 0; i < n; i++) {
		u8 c = (u8)nombre[i];

		if (es_alnum(c) || c == '.' || c == '-' || c == '_' || c == '~' || c == '!' || c == '+')
			tx_c(&p, c);
		else
			tx_fmt(&p, "%%%c%c", "0123456789ABCDEF"[c >> 4], "0123456789ABCDEF"[c & 15]);
	}
	if (p.malo) {
		t->malo = 1;
		tx_suelta(&p);
		return;
	}
	if (p.n + 30 <= 76) {
		tx_s(t, ";\r\n filename*=UTF-8''");
		tx_pon(t, p.d, p.n);
	} else {
		for (ini = 0, k = 0; ini < p.n; k++) {
			u32 l = p.n - ini > 60 ? 60 : p.n - ini;

			/* sin partir un %XX */
			if (ini + l < p.n) {
				if (p.d[ini + l - 1] == '%')
					l--;
				else if (l >= 2 && p.d[ini + l - 2] == '%')
					l -= 2;
			}
			tx_fmt(t, ";\r\n filename*%u*=%s", k, k == 0 ? "UTF-8''" : "");
			tx_pon(t, p.d + ini, l);
			ini += l;
		}
	}
	tx_suelta(&p);
}

static int es_ascii_limpio(const char *s)
{
	for (; *s; s++)
		if ((u8)*s >= 0x7f || (u8)*s < 0x20 || *s == '"' || *s == '\\')
			return 0;
	return 1;
}

static int parte_adjunto(Hacer *h, const Adjunto *a, int en_linea)
{
	Texto *t = &h->t;
	const u8 *d = a->datos;
	u32 n = a->n, l;
	u8 *leido = NULL;
	char *b, *nom, *tipo;
	const char *nombre = a->nombre && a->nombre[0] ? a->nombre : "adjunto";

	if (a->ruta) {
		leido = so_lee_archivo(a->ruta, &n, MIME_MAX_MENSAJE / 2);
		if (!leido) {
			snprintf(h->error, (size_t)h->cabe_error, "No se puede leer el adjunto «%s».", nombre);
			return 0;
		}
		d = leido;
	}
	if (!d)
		n = 0;
	nom = limpia(nombre);
	tipo = limpia(a->tipo && a->tipo[0] ? a->tipo : mime_tipo_de(nombre));
	if (!nom || !tipo) {
		so_suelta(nom);
		so_suelta(tipo);
		so_suelta(leido);
		t->malo = 1;
		return 0;
	}
	{
		char *s;

		for (s = nom; *s; s++)
			if (*s == '/' || *s == '\\')
				*s = '_';
		for (s = tipo; *s; s++)
			if (*s == ' ' || *s == ';' || *s == '"')
				*s = '_';
	}
	tx_s(t, "Content-Type: ");
	tx_s(t, tipo);
	if (es_ascii_limpio(nom)) {
		tx_s(t, ";\r\n name=\"");
		tx_s(t, nom);
		tx_s(t, "\"\r\n");
	} else {
		char *c = rfc2047_codifica(nom, 7, 0);

		if (!c)
			t->malo = 1;
		else {
			/* (dentro de comillas: sin partirlo en renglones) */
			char *s, *w;

			for (s = w = c; *s; s++)
				if (*s != '\r' && *s != '\n')
					*w++ = *s;
			*w = 0;
			tx_s(t, ";\r\n name=\"");
			tx_s(t, c);
			tx_s(t, "\"\r\n");
			so_suelta(c);
		}
	}
	tx_s(t, "Content-Disposition: ");
	tx_s(t, en_linea ? "inline" : "attachment");
	if (es_ascii_limpio(nom) && strlen(nom) < 60) {
		tx_s(t, ";\r\n filename=\"");
		tx_s(t, nom);
		tx_s(t, "\"");
	} else
		nombre_2231(t, nom);
	tx_s(t, "\r\n");
	if (a->id && a->id[0]) {
		char *id = limpia(a->id), *s;

		if (id) {
			for (s = id; *s; s++)
				if (*s == ' ' || *s == '<' || *s == '>')
					*s = '_';
			tx_s(t, "Content-ID: <");
			tx_s(t, id);
			tx_s(t, ">\r\n");
			so_suelta(id);
		}
	}
	tx_s(t, "Content-Transfer-Encoding: base64\r\n\r\n");
	b = b64_codifica(d, n, &l, 1);
	so_suelta(leido);
	so_suelta(nom);
	so_suelta(tipo);
	if (!b) {
		t->malo = 1;
		return 0;
	}
	tx_pon(t, b, l);
	so_suelta(b);
	return 1;
}

static void abre_multi(Texto *t, const char *tipo, const char *fr, int con_cabecera)
{
	tx_s(t, "Content-Type: ");
	tx_s(t, tipo);
	tx_s(t, ";\r\n boundary=\"");
	tx_s(t, fr);
	tx_s(t, "\"\r\n\r\n");
	if (con_cabecera)
		tx_s(t, "Este es un mensaje MIME de varias partes.\r\n");
}

static void frontera(Texto *t, const char *fr, int cierre)
{
	if (t->n && t->d[t->n - 1] != '\n')
		tx_s(t, "\r\n");
	tx_s(t, "\r\n--");
	tx_s(t, fr);
	tx_s(t, cierre ? "--\r\n" : "\r\n");
}

u8 *mime_construye(const Redactado *r, u32 *largo, char *error, int cabe)
{
	Hacer h;
	Texto *t = &h.t;
	char b[160], id[200], fm[48], fa[48], fr_[48];
	i64 fecha = r->fecha;
	int zona = r->zona, k, n_lin = 0, n_adj = 0;

	if (largo)
		*largo = 0;
	if (cabe > 0)
		error[0] = 0;
	h.error = error;
	h.cabe_error = cabe;
	tx_ini(t, MIME_MAX_MENSAJE);
	if (!fecha) {
		fecha = so_ahora();
		zona = so_zona(fecha);
	}
	mime_fecha_rfc(fecha, zona, b, sizeof b);
	tx_s(t, "Date: ");
	tx_s(t, b);
	tx_s(t, "\r\n");
	{
		Direccion de = r->de;
		char *c, *s;

		de.nombre[sizeof de.nombre - 1] = 0;
		de.dir[sizeof de.dir - 1] = 0;
		for (s = de.dir; *s; s++)
			if ((u8)*s <= 32 || *s == '<' || *s == '>' || *s == ',')
				*s = '_';
		for (s = de.nombre; *s; s++)
			if ((u8)*s < 0x20)
				*s = ' ';
		c = mime_dir_cabecera(&de);
		if (!c)
			goto sin_memoria;
		tx_s(t, "From: ");
		tx_s(t, c);
		tx_s(t, "\r\n");
		so_suelta(c);
	}
	cabecera_dirs(t, "To", r->para);
	cabecera_dirs(t, "Cc", r->cc);
	if (r->con_cco)
		cabecera_dirs(t, "Bcc", r->cco);
	cabecera_dirs(t, "Reply-To", r->responder_a);
	cabecera_texto(t, "Subject", r->asunto ? r->asunto : "");
	if (r->id_mensaje && r->id_mensaje[0]) {
		char *l = limpia(r->id_mensaje);

		if (!l)
			goto sin_memoria;
		cr_copia_en(id, sizeof id, l, (u32)strlen(l));
		so_suelta(l);
	} else {
		const char *a = strrchr(r->de.dir, '@');

		mime_nuevo_id(a ? a + 1 : NULL, id, sizeof id);
	}
	tx_s(t, "Message-ID: ");
	tx_s(t, id);
	tx_s(t, "\r\n");
	if (r->en_respuesta_a && r->en_respuesta_a[0]) {
		char *l = limpia(r->en_respuesta_a);

		if (!l)
			goto sin_memoria;
		tx_s(t, "In-Reply-To: ");
		tx_s(t, l);
		tx_s(t, "\r\n");
		so_suelta(l);
	}
	if (r->referencias && r->referencias[0]) {
		/* partidas por los blancos */
		char *l = limpia(r->referencias), *s;
		u32 col = 12;

		if (!l)
			goto sin_memoria;
		tx_s(t, "References:");
		for (s = l; *s;) {
			u32 w;

			while (*s == ' ')
				s++;
			if (!*s)
				break;
			for (w = 0; s[w] && s[w] != ' '; w++)
				;
			if (col + 1 + w > 76 && col > 12) {
				tx_s(t, "\r\n");
				col = 0;
			}
			tx_c(t, ' ');
			tx_pon(t, s, w);
			col += 1 + w;
			s += w;
		}
		tx_s(t, "\r\n");
		so_suelta(l);
	}
	tx_s(t, "MIME-Version: 1.0\r\n");
	if (r->programa && r->programa[0])
		cabecera_texto(t, "User-Agent", r->programa);
	for (k = 0; k < r->n_adjuntos; k++) {
		if (r->html && r->adjuntos[k].id && r->adjuntos[k].id[0])
			n_lin++;
		else
			n_adj++;
	}
	frontera_nueva(fm, 1);
	frontera_nueva(fa, 2);
	frontera_nueva(fr_, 3);
	/* mixed(alternative(texto, related(html, imágenes)), adjuntos) */
	if (n_adj) {
		abre_multi(t, "multipart/mixed", fm, 1);
		frontera(t, fm, 0);
	}
	if (r->html) {
		abre_multi(t, "multipart/alternative", fa, !n_adj);
		frontera(t, fa, 0);
		parte_texto(t, "text/plain", r->texto ? r->texto : "");
		frontera(t, fa, 0);
		if (n_lin) {
			abre_multi(t, "multipart/related", fr_, 0);
			frontera(t, fr_, 0);
		}
		parte_texto(t, "text/html", r->html);
		if (n_lin) {
			for (k = 0; k < r->n_adjuntos; k++)
				if (r->adjuntos[k].id && r->adjuntos[k].id[0]) {
					frontera(t, fr_, 0);
					if (!parte_adjunto(&h, &r->adjuntos[k], 1))
						goto mal;
				}
			frontera(t, fr_, 1);
		}
		frontera(t, fa, 1);
	} else
		parte_texto(t, "text/plain", r->texto ? r->texto : "");
	if (n_adj) {
		for (k = 0; k < r->n_adjuntos; k++)
			if (!(r->html && r->adjuntos[k].id && r->adjuntos[k].id[0])) {
				frontera(t, fm, 0);
				if (!parte_adjunto(&h, &r->adjuntos[k], 0))
					goto mal;
			}
		frontera(t, fm, 1);
	}
	if (t->malo == 2) {
		snprintf(error, (size_t)cabe, "El mensaje es demasiado grande.");
		tx_suelta(t);
		return NULL;
	}
	if (t->malo)
		goto sin_memoria;
	return (u8 *)tx_fin(t, largo);
sin_memoria:
	snprintf(error, (size_t)cabe, "No queda memoria para hacer el mensaje.");
mal:
	if (!error[0])
		snprintf(error, (size_t)cabe, "No queda memoria para hacer el mensaje.");
	tx_suelta(t);
	return NULL;
}
