/* redacta.c - Correo: escribir un mensaje (nuevo, responder, responder a
 * todos, reenviar o seguir un borrador), encima de todo en la misma ventana.
 * Para, CC y CCO con el autocompletado de la libreta (lo que empieza por lo
 * escrito, en el nombre o en la dirección, sin tildes; primero los más
 * usados); el asunto; el texto (con su cursor, seleccionar, copiar y pegar);
 * los adjuntos (Adjuntar...: un cuadro con las carpetas); la firma de la
 * cuenta; y, si se quiere, HTML sencillo: *negrita*, _cursiva_, listas con
 * «- » y enlaces [texto](https://...), que se mandan también como texto.
 * Enviar lo pasa a motor_envia (y queda en Enviados); Guardar borrador, al
 * servidor (Borradores).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "ventana.h"

#define MAX_ADJ 16
#define F_PARA 0
#define F_CC 1
#define F_CCO 2
#define F_ASUNTO 3
#define F_TEXTO 4

static struct {
	Campo f[4];
	u32 *t;			/* el texto (Unicode) */
	int n, cabe, cur, sel, desde;	/* desde: la primera línea que se ve */
	int foco;
	int html;
	char adj_ruta[MAX_ADJ][260], adj_nombre[MAX_ADJ][200];
	Adjunto adj[MAX_ADJ];
	u8 *adj_datos[MAX_ADJ];
	u32 adj_n[MAX_ADJ];
	int nadj;
	char en_resp[300], refs[1200];
	u32 uid_borrador;
	u32 uid_original;
	int como;
	int cambiado;
	/* el autocompletado */
	int sug[8], nsug, sug_cur;
	/* lo cortado (las líneas) */
	int *lin, nlin, clin, cols;
	int ex, ey, ean, eal;	/* dónde está el editor */
} R;

/* ---- el texto ---- */
static int hay_sitio(int mas)
{
	if (R.n + mas + 1 > R.cabe) {
		int c = R.cabe ? R.cabe * 2 : 4096;
		u32 *q;

		while (c < R.n + mas + 1)
			c *= 2;
		if (c > (4 << 20))
			return 0;
		q = so_repide(R.t, (size_t)c * sizeof(u32));
		if (!q)
			return 0;
		R.t = q;
		R.cabe = c;
	}
	return 1;
}

static void inserta(const u32 *l, int n)
{
	if (n <= 0 || !hay_sitio(n))
		return;
	memmove(R.t + R.cur + n, R.t + R.cur, (size_t)(R.n - R.cur) * sizeof(u32));
	memcpy(R.t + R.cur, l, (size_t)n * sizeof(u32));
	R.n += n;
	R.cur += n;
	R.cambiado = 1;
}

static void inserta_utf8(const char *s)
{
	u32 i = 0, n = (u32)strlen(s), b[256];
	int k = 0;

	while (i < n) {
		u32 c = utf8_letra((const u8 *)s, n, &i);

		if (c == '\r')
			continue;
		b[k++] = c;
		if (k == 256) {
			inserta(b, k);
			k = 0;
		}
	}
	inserta(b, k);
}

static char *texto_utf8(void)
{
	char *d = so_pide((size_t)R.n * 4 + 1);
	u32 k = 0;
	int i;

	if (!d)
		return NULL;
	for (i = 0; i < R.n; i++)
		k += (u32)utf8_pon(R.t[i], d + k);
	d[k] = 0;
	return d;
}

static void borra_sel(void)
{
	if (R.sel >= 0 && R.sel != R.cur) {
		int a = R.sel < R.cur ? R.sel : R.cur, b = R.sel < R.cur ? R.cur : R.sel;

		memmove(R.t + a, R.t + b, (size_t)(R.n - b) * sizeof(u32));
		R.n -= b - a;
		R.cur = a;
		R.cambiado = 1;
	}
	R.sel = -1;
}

/* las líneas (cortadas a «cols» por los espacios) */
static void corta(int cols)
{
	int i = 0, ini = 0, ult = -1;

	R.cols = cols;
	R.nlin = 0;
	for (;;) {
		if (R.nlin + 2 > R.clin) {
			int c = R.clin ? R.clin * 2 : 256;
			int *q = so_repide(R.lin, (size_t)c * sizeof(int));

			if (!q)
				return;
			R.lin = q;
			R.clin = c;
		}
		R.lin[R.nlin++] = ini;
		ult = -1;
		for (i = ini; i < R.n && R.t[i] != '\n' && i - ini < cols; i++)
			if (R.t[i] == ' ')
				ult = i;
		if (i >= R.n)
			break;
		if (R.t[i] == '\n')
			ini = i + 1;
		else
			ini = ult > ini ? ult + 1 : i;
	}
	R.lin[R.nlin] = R.n + 1;
}

static int linea_de(int pos)
{
	int a = 0, z = R.nlin - 1;

	while (a < z) {
		int m = (a + z + 1) / 2;

		if (R.lin[m] <= pos)
			a = m;
		else
			z = m - 1;
	}
	return a;
}

/* ---- el HTML sencillo ---- */
static void pon_esc(char **d, u32 *k, u32 *cabe, const char *s, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		const char *e = s[i] == '<' ? "&lt;" : s[i] == '>' ? "&gt;" : s[i] == '&' ? "&amp;" : s[i] == '"' ? "&quot;" : NULL;
		u32 l = e ? (u32)strlen(e) : 1;

		if (*k + l + 64 > *cabe) {
			char *q = so_repide(*d, *cabe * 2);

			if (!q)
				return;
			*d = q;
			*cabe *= 2;
		}
		if (e)
			memcpy(*d + *k, e, l);
		else
			(*d)[*k] = s[i];
		*k += l;
	}
}

static void pon(char **d, u32 *k, u32 *cabe, const char *s)
{
	u32 n = (u32)strlen(s);

	if (*k + n + 64 > *cabe) {
		u32 c = *cabe * 2 + n;
		char *q = so_repide(*d, c);

		if (!q)
			return;
		*d = q;
		*cabe = c;
	}
	memcpy(*d + *k, s, n);
	*k += n;
}

/* un renglón con *negrita*, _cursiva_ y [texto](url) */
static void renglon_html(char **d, u32 *k, u32 *cabe, const char *s, u32 n)
{
	u32 i = 0;
	int neg = 0, cur = 0;

	while (i < n) {
		if (s[i] == '*' && (neg || (i + 1 < n && s[i + 1] != ' ' && memchr(s + i + 1, '*', n - i - 1)))) {
			pon(d, k, cabe, neg ? "</b>" : "<b>");
			neg = !neg;
			i++;
			continue;
		}
		if (s[i] == '_' && (cur || (i + 1 < n && s[i + 1] != ' ' && memchr(s + i + 1, '_', n - i - 1))) &&
		    (cur || i == 0 || s[i - 1] == ' ')) {
			pon(d, k, cabe, cur ? "</i>" : "<i>");
			cur = !cur;
			i++;
			continue;
		}
		if (s[i] == '[') {
			const char *c = memchr(s + i, ']', n - i);

			if (c && (u32)(c - s) + 1 < n && c[1] == '(') {
				const char *f = memchr(c, ')', n - (u32)(c - s));

				if (f && (!strncmp(c + 2, "https://", 8) || !strncmp(c + 2, "http://", 7) || !strncmp(c + 2, "mailto:", 7))) {
					pon(d, k, cabe, "<a href=\"");
					pon_esc(d, k, cabe, c + 2, (u32)(f - c - 2));
					pon(d, k, cabe, "\">");
					pon_esc(d, k, cabe, s + i + 1, (u32)(c - s - (int)i - 1));
					pon(d, k, cabe, "</a>");
					i = (u32)(f - s) + 1;
					continue;
				}
			}
		}
		if ((!strncmp(s + i, "https://", 8) || !strncmp(s + i, "http://", 7)) && n - i > 8) {
			u32 j = i;

			while (j < n && s[j] != ' ')
				j++;
			pon(d, k, cabe, "<a href=\"");
			pon_esc(d, k, cabe, s + i, j - i);
			pon(d, k, cabe, "\">");
			pon_esc(d, k, cabe, s + i, j - i);
			pon(d, k, cabe, "</a>");
			i = j;
			continue;
		}
		pon_esc(d, k, cabe, s + i, 1);
		i++;
	}
	if (cur)
		pon(d, k, cabe, "</i>");
	if (neg)
		pon(d, k, cabe, "</b>");
}

static char *a_html(const char *t)
{
	u32 cabe = (u32)strlen(t) * 2 + 512, k = 0;
	char *d = so_pide(cabe);
	const char *p = t;
	int lista = 0, cita = 0;

	if (!d)
		return NULL;
	pon(&d, &k, &cabe, "<!DOCTYPE html>\r\n<html><head><meta charset=\"utf-8\"></head><body style=\"font-family: sans-serif\">\r\n");
	while (*p) {
		const char *e = strchr(p, '\n');
		u32 n = e ? (u32)(e - p) : (u32)strlen(p);
		int es_lista = n >= 2 && (p[0] == '-' || p[0] == '*') && p[1] == ' ', es_cita = n >= 1 && p[0] == '>';

		if (es_lista && !lista)
			pon(&d, &k, &cabe, "<ul>\r\n");
		if (!es_lista && lista)
			pon(&d, &k, &cabe, "</ul>\r\n");
		if (es_cita && !cita)
			pon(&d, &k, &cabe, "<blockquote style=\"border-left: 2px solid #999; padding-left: 8px\">\r\n");
		if (!es_cita && cita)
			pon(&d, &k, &cabe, "</blockquote>\r\n");
		lista = es_lista;
		cita = es_cita;
		if (es_lista) {
			pon(&d, &k, &cabe, "<li>");
			renglon_html(&d, &k, &cabe, p + 2, n - 2);
			pon(&d, &k, &cabe, "</li>\r\n");
		} else {
			u32 sal = es_cita ? (n > 1 && p[1] == ' ' ? 2 : 1) : 0;

			renglon_html(&d, &k, &cabe, p + sal, n - sal);
			pon(&d, &k, &cabe, "<br>\r\n");
		}
		p += n;
		if (*p == '\n')
			p++;
	}
	if (lista)
		pon(&d, &k, &cabe, "</ul>\r\n");
	if (cita)
		pon(&d, &k, &cabe, "</blockquote>\r\n");
	pon(&d, &k, &cabe, "</body></html>\r\n");
	d[k] = 0;
	return d;
}

/* ---- preparar ---- */
static void limpia(void)
{
	int i;

	for (i = 0; i < R.nadj; i++)
		so_suelta(R.adj_datos[i]);
	so_suelta(R.t);
	so_suelta(R.lin);
	memset(&R, 0, sizeof R);
	R.sel = -1;
}

static int hay_re(const char *s, const char *pre)
{
	u32 n = (u32)strlen(pre), i;

	for (i = 0; i < n; i++)
		if ((s[i] | 32) != (pre[i] | 32))
			return 0;
	return 1;
}

/* une las direcciones de un valor, sin la de la cuenta ni las repetidas */
static void anade_dirs(char *d, u32 cabe, const char *valor, const char *quitar)
{
	Direccion v[32];
	int n, i;
	u32 k = (u32)strlen(d);

	if (!valor)
		return;
	n = mime_direcciones(valor, (u32)strlen(valor), v, 32);
	for (i = 0; i < n; i++) {
		char *c;

		if (!v[i].dir[0] || (quitar && !strcmp(v[i].dir, quitar)) || strstr(d, v[i].dir))
			continue;
		c = mime_dir_cabecera(&v[i]);
		if (!c)
			continue;
		{
			/* (en el campo, sin RFC 2047: el nombre tal cual) */
			char t[400];

			if (v[i].nombre[0])
				snprintf(t, sizeof t, "%s <%s>", v[i].nombre, v[i].dir);
			else
				snprintf(t, sizeof t, "%s", v[i].dir);
			if (k + strlen(t) + 3 < cabe)
				k += (u32)snprintf(d + k, cabe - k, "%s%s", k ? ", " : "", t);
		}
		so_suelta(c);
	}
}

static void prepara(int como, const char *para)
{
	Mensaje *m = como == RD_NUEVO ? NULL : vista_mensaje();
	char t[2000];

	limpia();
	R.como = como;
	R.foco = F_PARA;
	if (para) {
		char p[400];

		snprintf(p, sizeof p, "%s", para);
		{
			char *q = strchr(p, '?');

			if (q)
				*q = 0;	/* (mailto:a@b?subject=...: lo de detrás no) */
		}
		campo_pon(&R.f[F_PARA], p);
		R.foco = F_ASUNTO;
	}
	if (m && como != RD_BORRADOR) {
		char *asunto = mime_cabecera_texto(m, 0, "Subject"), *mid = mime_cabecera(m, 0, "Message-ID", 0);
		char *refs = mime_cabecera(m, 0, "References", 0);
		const char *pre = como == RD_REENVIAR ? "Fwd: " : "Re: ";

		snprintf(t, sizeof t, "%s%s", asunto && (hay_re(asunto, "re:") || (como == RD_REENVIAR && hay_re(asunto, "fwd:"))) ? "" : pre,
			 asunto ? asunto : "");
		campo_pon(&R.f[F_ASUNTO], t);
		if (como != RD_REENVIAR) {
			char *rt = mime_cabecera(m, 0, "Reply-To", 0), *de = mime_cabecera(m, 0, "From", 0);

			t[0] = 0;
			anade_dirs(t, sizeof t, rt ? rt : de, NULL);
			campo_pon(&R.f[F_PARA], t);
			if (como == RD_TODOS) {
				char *to = mime_cabecera(m, 0, "To", 0), *cc = mime_cabecera(m, 0, "Cc", 0);
				char c[2000] = "";

				anade_dirs(c, sizeof c, to, ses->c->correo);
				anade_dirs(c, sizeof c, cc, ses->c->correo);
				campo_pon(&R.f[F_CC], c);
				so_suelta(to);
				so_suelta(cc);
			}
			so_suelta(rt);
			so_suelta(de);
			if (mid) {
				snprintf(R.en_resp, sizeof R.en_resp, "%s", mid);
				snprintf(R.refs, sizeof R.refs, "%s%s%s", refs ? refs : "", refs ? " " : "", mid);
			}
			R.foco = F_TEXTO;
		}
		so_suelta(asunto);
		so_suelta(mid);
		so_suelta(refs);
	}
	/* el texto: la firma y, debajo, lo citado */
	if (como != RD_BORRADOR) {
		inserta_utf8("\n\n");
		if (ses->c->firma[0]) {
			inserta_utf8("-- \n");
			inserta_utf8(ses->c->firma);
			inserta_utf8("\n");
		}
	}
	if (m) {
		u32 n;
		char *pl = vista_texto_plano(&n);
		char *de = mime_cabecera_texto(m, 0, "From"), *fe = mime_cabecera(m, 0, "Date", 0);
		char fl[80] = "";

		if (fe) {
			i64 s = mime_fecha(fe, (u32)strlen(fe));

			if (s)
				mime_fecha_larga(s, so_zona(s), fl, sizeof fl);
		}
		if (como == RD_BORRADOR) {
			char *x;

			x = mime_cabecera_texto(m, 0, "To");
			campo_pon(&R.f[F_PARA], x ? x : "");
			so_suelta(x);
			x = mime_cabecera_texto(m, 0, "Cc");
			campo_pon(&R.f[F_CC], x ? x : "");
			so_suelta(x);
			x = mime_cabecera_texto(m, 0, "Bcc");
			campo_pon(&R.f[F_CCO], x ? x : "");
			so_suelta(x);
			x = mime_cabecera_texto(m, 0, "Subject");
			campo_pon(&R.f[F_ASUNTO], x ? x : "");
			so_suelta(x);
			if (pl)
				inserta_utf8(pl);
		} else if (como == RD_REENVIAR) {
			char *as = mime_cabecera_texto(m, 0, "Subject"), *pa = mime_cabecera_texto(m, 0, "To");

			snprintf(t, sizeof t, "\n---------- Mensaje reenviado ----------\nDe: %s\nFecha: %s\nAsunto: %s\nPara: %s\n\n", de ? de : "", fl,
				 as ? as : "", pa ? pa : "");
			inserta_utf8(t);
			if (pl)
				inserta_utf8(pl);
			so_suelta(as);
			so_suelta(pa);
		} else if (pl) {
			char *p = pl;

			snprintf(t, sizeof t, "El %s, %s escribi\xc3\xb3:\n", fl[0] ? fl : "d\xc3\xad" "a", de ? de : "alguien");
			inserta_utf8(t);
			while (*p) {
				char *e = strchr(p, '\n');
				u32 l = e ? (u32)(e - p) : (u32)strlen(p);
				char r[1100];

				snprintf(r, sizeof r, ">%s%.*s\n", p[0] == '>' ? "" : " ", (int)(l > 1000 ? 1000 : l), p);
				inserta_utf8(r);
				p += l;
				if (*p)
					p++;
			}
		}
		so_suelta(pl);
		so_suelta(de);
		so_suelta(fe);
		/* reenviar y borradores: con sus adjuntos */
		if (como == RD_REENVIAR || como == RD_BORRADOR) {
			int k;

			for (k = 0; k < mime_n_adjuntos(m) && R.nadj < MAX_ADJ; k++) {
				int a = mime_adjunto(m, k);
				u32 n2;
				u8 *d = mime_cuerpo(m, a, &n2);

				if (!d)
					continue;
				snprintf(R.adj_nombre[R.nadj], 200, "%s", m->partes[a].nombre[0] ? m->partes[a].nombre : "adjunto");
				R.adj_datos[R.nadj] = d;
				R.adj_n[R.nadj] = n2;
				R.adj_ruta[R.nadj][0] = 0;
				R.nadj++;
			}
		}
	}
	R.cur = 0;
	if (como == RD_BORRADOR && ses->sel >= 0) {
		u32 n;

		(void)vista_datos(&n);
		R.uid_borrador = 0;
	}
	R.cambiado = 0;
}

/* ---- las direcciones de los campos ---- */
static int direcciones(int campo, char *dests, u32 cabe, u32 *k, char *malo, u32 mc)
{
	char t[CAMPO_MAX * 4];
	Direccion v[64];
	int n, i;

	campo_saca(&R.f[campo], t, sizeof t);
	n = mime_direcciones(t, (u32)strlen(t), v, 64);
	for (i = 0; i < n; i++) {
		if (!mime_dir_valida(v[i].dir)) {
			snprintf(malo, mc, "%s", v[i].dir[0] ? v[i].dir : v[i].nombre);
			return -1;
		}
		if (*k + strlen(v[i].dir) + 2 < cabe)
			*k += (u32)snprintf(dests + *k, cabe - *k, "%s%s", *k ? "," : "", v[i].dir);
	}
	/* (algo escrito que no es ninguna dirección) */
	if (!n) {
		for (i = 0; t[i] == ' ' || t[i] == ','; i++)
			;
		if (t[i]) {
			snprintf(malo, mc, "%s", t);
			return -1;
		}
	}
	return n;
}

static void apunta_libreta(int campo)
{
	char t[CAMPO_MAX * 4];
	Direccion v[64];
	int n, i;

	campo_saca(&R.f[campo], t, sizeof t);
	n = mime_direcciones(t, (u32)strlen(t), v, 64);
	for (i = 0; i < n; i++)
		if (mime_dir_valida(v[i].dir))
			libreta_usa(v[i].nombre, v[i].dir);
}

static void haz_redactado(Redactado *r, char *para, char *cc, char *cco, char *as, char **texto, char **html)
{
	int i;

	memset(r, 0, sizeof *r);
	snprintf(r->de.nombre, sizeof r->de.nombre, "%s", ses->c->visible);
	snprintf(r->de.dir, sizeof r->de.dir, "%s", ses->c->correo);
	campo_saca(&R.f[F_PARA], para, CAMPO_MAX * 4);
	campo_saca(&R.f[F_CC], cc, CAMPO_MAX * 4);
	campo_saca(&R.f[F_CCO], cco, CAMPO_MAX * 4);
	campo_saca(&R.f[F_ASUNTO], as, CAMPO_MAX * 4);
	r->para = para;
	r->cc = cc[0] ? cc : NULL;
	r->cco = cco[0] ? cco : NULL;
	r->asunto = as;
	*texto = texto_utf8();
	r->texto = *texto ? *texto : "";
	*html = R.html && *texto ? a_html(*texto) : NULL;
	r->html = *html;
	for (i = 0; i < R.nadj; i++) {
		memset(&R.adj[i], 0, sizeof R.adj[i]);
		if (R.adj_datos[i]) {
			R.adj[i].datos = R.adj_datos[i];
			R.adj[i].n = R.adj_n[i];
		} else
			R.adj[i].ruta = R.adj_ruta[i];
		R.adj[i].nombre = R.adj_nombre[i];
	}
	r->adjuntos = R.adj;
	r->n_adjuntos = R.nadj;
	r->en_respuesta_a = R.en_resp[0] ? R.en_resp : NULL;
	r->referencias = R.refs[0] ? R.refs : NULL;
	r->programa = CORREO_PROGRAMA;
	r->fecha = so_ahora();
	r->zona = so_zona(r->fecha);
}

static int envia(void)
{
	static char para[CAMPO_MAX * 4], cc[CAMPO_MAX * 4], cco[CAMPO_MAX * 4], as[CAMPO_MAX * 4];
	char dests[4000], malo[300], t[600];
	u32 k = 0;
	int n = 0, x, c;
	Redactado r;
	char *texto, *html;

	for (c = F_PARA; c <= F_CCO; c++) {
		x = direcciones(c, dests, sizeof dests, &k, malo, sizeof malo);
		if (x < 0) {
			snprintf(t, sizeof t, "\xc2\xab%s\xc2\xbb no es una direcci\xc3\xb3n de correo v\xc3\xa1lida (ana@ejemplo.es).", malo);
			aviso("Enviar", t);
			R.foco = c;
			return 0;
		}
		n += x;
	}
	if (!n) {
		aviso("Enviar", "Falta a qui\xc3\xa9n se manda (Para).");
		R.foco = F_PARA;
		return 0;
	}
	if (!R.f[F_ASUNTO].n && pregunta("Enviar", "El mensaje no lleva asunto. \xc2\xbf" "Enviarlo as\xc3\xad?", "Enviarlo", "Ponerle asunto", NULL) != 0) {
		R.foco = F_ASUNTO;
		return 0;
	}
	if (!ses->clave[0] && !pide_texto("Contrase\xc3\xb1" "a", "La contrase\xc3\xb1" "a de la cuenta:", ses->clave, sizeof ses->clave, 1))
		return 0;
	haz_redactado(&r, para, cc, cco, as, &texto, &html);
	cancelado = 0;
	red_espera = espera_red;
	snprintf(ses->estado, sizeof ses->estado, "Enviando...");
	progreso("Enviando...", 0, 0);
	x = motor_envia(ses, &r, dests);
	so_suelta(texto);
	so_suelta(html);
	if (x < 0) {
		snprintf(t, sizeof t, "No se ha enviado: %s", ses->error);
		aviso("Enviar", t);
		return 0;
	}
	so_apunta("enviado: \xc2\xab%s\xc2\xbb a %s (%d adjuntos%s)", as, dests, R.nadj, R.html ? ", con HTML" : "");
	for (c = F_PARA; c <= F_CCO; c++)
		apunta_libreta(c);
	libreta_guarda();
	/* el original, respondido o reenviado */
	if ((R.como == RD_RESPONDER || R.como == RD_TODOS || R.como == RD_REENVIAR) && R.uid_original && ses->buz_abierto) {
		Ficha *f = buzon_busca(&ses->buz, R.uid_original);

		if (f)
			motor_marca(ses, &f, 1, R.como == RD_REENVIAR ? B_REENVIADO : B_RESPONDIDO, 1);
	}
	/* el borrador que se seguía, fuera */
	if (R.como == RD_BORRADOR && R.uid_borrador && ses->buz_abierto && (ses->car[ses->sel].marcas & CM_BORRADORES)) {
		Ficha *f = buzon_busca(&ses->buz, R.uid_borrador);

		if (f)
			motor_borra(ses, &f, 1);
	}
	di_estado(x ? "%s" : "Enviado.", ses->error);
	return 1;
}

static void guarda_borrador(void)
{
	static char para[CAMPO_MAX * 4], cc[CAMPO_MAX * 4], cco[CAMPO_MAX * 4], as[CAMPO_MAX * 4];
	Redactado r;
	char *texto, *html;
	int x;

	haz_redactado(&r, para, cc, cco, as, &texto, &html);
	cancelado = 0;
	red_espera = espera_red;
	x = motor_borrador(ses, &r, &R.uid_borrador);
	so_suelta(texto);
	so_suelta(html);
	if (x < 0)
		di_estado("No se ha guardado el borrador: %s", ses->error);
	else {
		di_estado(x ? "%s" : "Borrador guardado.", ses->error);
		R.cambiado = 0;
	}
}

static void adjunta(void)
{
	char r[260] = "";
	u32 n;
	int c;

	if (R.nadj >= MAX_ADJ) {
		aviso("Adjuntar", "No caben m\xc3\xa1s adjuntos.");
		return;
	}
	if (!elige_archivo("Adjuntar un archivo", r, sizeof r, 0, NULL))
		return;
	if (!so_existe(r, &c) || c) {
		aviso("Adjuntar", "Eso no es un archivo.");
		return;
	}
	{
		void *f = so_abre(r, SO_LEER);

		n = f ? so_largo(f) : 0;
		so_cierra(f);
	}
	if (n > ajustes.max_mensaje_kb * 1024u) {
		aviso("Adjuntar", "Es demasiado grande para mandarlo por correo.");
		return;
	}
	snprintf(R.adj_ruta[R.nadj], 260, "%s", r);
	{
		const char *b = strrchr(r, so_barra());

		snprintf(R.adj_nombre[R.nadj], 200, "%s", b ? b + 1 : r);
	}
	R.adj_datos[R.nadj] = NULL;
	R.adj_n[R.nadj] = n;
	R.nadj++;
	R.cambiado = 1;
	so_apunta("adjuntado: %s (%u octetos)", r, (unsigned)n);
}

/* ---- el autocompletado ---- */
static void sugiere(void)
{
	char t[CAMPO_MAX * 4], *p;

	R.nsug = 0;
	if (R.foco > F_CCO)
		return;
	campo_saca(&R.f[R.foco], t, sizeof t);
	p = strrchr(t, ',');
	p = p ? p + 1 : t;
	while (*p == ' ')
		p++;
	if (strlen(p) < 1 || strchr(p, '<'))
		return;
	R.nsug = libreta_completa(p, R.sug, 6);
	if (R.sug_cur >= R.nsug)
		R.sug_cur = 0;
}

static void elige_sug(int i)
{
	char t[CAMPO_MAX * 4], *p;
	Contacto *c;

	if (i < 0 || i >= R.nsug)
		return;
	c = &libreta[R.sug[i]];
	campo_saca(&R.f[R.foco], t, sizeof t);
	p = strrchr(t, ',');
	p = p ? p + 1 : t;
	*p = 0;
	{
		char n[CAMPO_MAX * 4];

		snprintf(n, sizeof n, "%s%s%s%s<%s>, ", t, p > t ? " " : "", c->nombre, c->nombre[0] ? " " : "", c->correo[0]);
		campo_pon(&R.f[R.foco], n);
	}
	R.nsug = 0;
	R.cambiado = 1;
}

/* ---- pintar ---- */
static int ey_campos[4];

static void pinta(void)
{
	static const char *const rot[4] = { "Para:", "CC:", "CCO:", "Asunto:" };
	int x = 0, y = 0, bx = 8, i, fy, cols, primera;

	zonas_olvida();
	ui_rect(0, 0, van, val, C_FONDO);
	ui_degradado(0, 0, van, 38, 0xF9FAFB, 0xE5E7EB);
	bx += boton(1, bx, 6, 0, "Enviar", 1, 0, 0) + 6;
	bx += boton(2, bx, 6, 0, "Guardar borrador", 0, 0, 0) + 6;
	bx += boton(3, bx, 6, 0, "Adjuntar...", 0, 0, 0) + 6;
	bx += boton(4, bx, 6, 0, "Libreta", 0, 0, 0) + 6;
	bx += boton(5, bx, 6, 0, R.html ? "Formato: HTML" : "Formato: texto", 0, 0, 0) + 6;
	if (R.html && bx + 260 < van - 90) {
		bx += boton(6, bx, 6, 0, "*N*", 0, 0, 0) + 4;
		bx += boton(7, bx, 6, 0, "_C_", 0, 0, 0) + 4;
		bx += boton(8, bx, 6, 0, "- Lista", 0, 0, 0) + 4;
		bx += boton(9, bx, 6, 0, "Enlace", 0, 0, 0) + 4;
	}
	boton(10, van - 8 - ancho_texto("Cerrar") - 20, 6, 0, "Cerrar", 0, 0, 0);
	fy = 46;
	{
		char t[300];

		snprintf(t, sizeof t, "%s <%s>", ses->c->visible[0] ? ses->c->visible : "", ses->c->correo);
		negrita(10, fy + 4, "De:", 8, C_SUAVE);
		texto(90, fy + 4, t, (van - 100) / LA, C_TINTA);
		fy += 26;
	}
	for (i = 0; i < 4; i++) {
		negrita(10, fy + 4, rot[i], 8, C_SUAVE);
		campo_pinta(&R.f[i], 90, fy, van - 100, R.foco == i);
		zona(20 + i, 90, fy, van - 100, 24);
		ey_campos[i] = fy;
		fy += 28;
	}
	/* los adjuntos */
	if (R.nadj) {
		int ax = 90;

		negrita(10, fy + 4, "Adjuntos:", 9, C_SUAVE);
		for (i = 0; i < R.nadj; i++) {
			char t[260];
			int w;

			snprintf(t, sizeof t, "%s (%u KB) X", R.adj_nombre[i], (unsigned)((R.adj_n[i] + 1023) / 1024));
			w = ancho_texto(t) + 28;
			if (ax + w > van - 10)
				break;
			ui_redondo(ax, fy, w, 22, 4, C_BORDE, 0xE5E7EB);
			icono(IC_CLIP, ax + 4, fy + 5, C_TINTA, 0xE5E7EB);
			texto(ax + 20, fy + 3, t, (w - 24) / LA, C_TINTA);
			zona(40 + i, ax, fy, w, 22);
			ax += w + 6;
		}
		fy += 28;
	}
	/* el texto */
	R.ex = 8;
	R.ey = fy;
	R.ean = van - 16;
	R.eal = val - fy - 8;
	marco_hundido(R.ex, R.ey, R.ean, R.eal, C_PAPEL);
	if (R.foco == F_TEXTO)
		ui_marco(R.ex + 1, R.ey + 1, R.ean - 2, R.eal - 2, C_AZUL);
	cols = (R.ean - 20) / LA;
	corta(cols);
	{
		int filas = (R.eal - 8) / (LT + 2), cl = linea_de(R.cur), l;
		int sa = R.sel >= 0 && R.sel != R.cur ? (R.sel < R.cur ? R.sel : R.cur) : -1, sb = sa >= 0 ? (R.sel < R.cur ? R.cur : R.sel) : -1;

		if (cl < R.desde)
			R.desde = cl;
		if (cl >= R.desde + filas)
			R.desde = cl - filas + 1;
		primera = R.desde;
		for (l = primera; l < R.nlin && l < primera + filas; l++) {
			int a = R.lin[l], b = l + 1 < R.nlin ? R.lin[l + 1] : R.n, j, k = 0, ly = R.ey + 4 + (l - primera) * (LT + 2);
			char d[1600];

			if (b > R.n)
				b = R.n;
			if (sa >= 0 && sb > a && sa < b) {
				int x1 = sa > a ? sa : a, x2 = sb < b ? sb : b;

				ui_rect(R.ex + 8 + (x1 - a) * LA, ly, (x2 - x1) * LA, LT, C_AZUL_CLARO);
			}
			for (j = a; j < b && k < 1590; j++)
				if (R.t[j] != '\n')
					k += utf8_pon(R.t[j], d + k);
			d[k] = 0;
			ui_texto(R.ex + 8, ly, d, (u32)k, cols + 1, C_TINTA, 0);
			if (R.foco == F_TEXTO && R.cur >= a && (R.cur < b || (R.cur == b && (l + 1 >= R.nlin || R.lin[l + 1] > R.cur))))
				ui_rect(R.ex + 8 + (R.cur - a) * LA, ly, 2, LT, C_TINTA);
		}
		zona(30, R.ex, R.ey, R.ean, R.eal);
	}
	/* las sugerencias, encima de todo */
	if (R.nsug && R.foco <= F_CCO) {
		int sy = ey_campos[R.foco] + 26, sw = van - 120;

		ui_rect(92, sy, sw, R.nsug * FILA + 4, C_BORDE);
		ui_rect(93, sy + 1, sw - 2, R.nsug * FILA + 2, C_PAPEL);
		for (i = 0; i < R.nsug; i++) {
			char t[300];
			Contacto *c = &libreta[R.sug[i]];

			if (i == R.sug_cur)
				ui_rect(94, sy + 2 + i * FILA, sw - 4, FILA, C_ELEGIDO);
			snprintf(t, sizeof t, "%s%s<%s>", c->nombre, c->nombre[0] ? " " : "", c->correo[0]);
			ui_texto(100, sy + 4 + i * FILA, t, (u32)strlen(t), (sw - 16) / LA, i == R.sug_cur ? C_PAPEL : C_TINTA, i == R.sug_cur);
			zona(50 + i, 94, sy + 2 + i * FILA, sw - 4, FILA);
		}
	}
	(void)x;
	(void)y;
}

/* ---- las teclas del texto ---- */
static void tecla_texto(u32 k, u32 mod)
{
	int l, col;

	(void)mod;
	if (k == UT_INTRO) {
		u32 c = '\n';

		borra_sel();
		inserta(&c, 1);
	} else if (k == UT_BORRAR) {
		if (R.sel >= 0 && R.sel != R.cur)
			borra_sel();
		else if (R.cur > 0) {
			memmove(R.t + R.cur - 1, R.t + R.cur, (size_t)(R.n - R.cur) * sizeof(u32));
			R.n--;
			R.cur--;
			R.cambiado = 1;
		}
	} else if (k == UT_SUPR) {
		if (R.sel >= 0 && R.sel != R.cur)
			borra_sel();
		else if (R.cur < R.n) {
			memmove(R.t + R.cur, R.t + R.cur + 1, (size_t)(R.n - R.cur - 1) * sizeof(u32));
			R.n--;
			R.cambiado = 1;
		}
	} else if (k == UT_IZQUIERDA) {
		if (R.cur > 0)
			R.cur--;
		R.sel = -1;
	} else if (k == UT_DERECHA) {
		if (R.cur < R.n)
			R.cur++;
		R.sel = -1;
	} else if (k == UT_ARRIBA || k == UT_ABAJO || k == UT_RE_PAG || k == UT_AV_PAG) {
		int paso = k == UT_ARRIBA ? -1 : k == UT_ABAJO ? 1 : k == UT_RE_PAG ? -15 : 15, nl;

		l = linea_de(R.cur);
		col = R.cur - R.lin[l];
		nl = l + paso;
		if (nl < 0)
			nl = 0;
		if (nl >= R.nlin)
			nl = R.nlin - 1;
		{
			int fin = nl + 1 < R.nlin ? R.lin[nl + 1] - 1 : R.n;

			R.cur = R.lin[nl] + col;
			if (R.cur > fin)
				R.cur = fin;
			if (R.cur > R.n)
				R.cur = R.n;
		}
		R.sel = -1;
	} else if (k == UT_INICIO) {
		R.cur = R.lin[linea_de(R.cur)];
		R.sel = -1;
	} else if (k == UT_FIN) {
		l = linea_de(R.cur);
		R.cur = l + 1 < R.nlin ? R.lin[l + 1] - 1 : R.n;
		if (R.cur < R.lin[l])
			R.cur = R.lin[l];
		R.sel = -1;
	} else if (k == 1) {
		R.sel = 0;
		R.cur = R.n;
	} else if (k == 3 || k == 24) {
		if (R.sel >= 0 && R.sel != R.cur) {
			int a = R.sel < R.cur ? R.sel : R.cur, b = R.sel < R.cur ? R.cur : R.sel, i;
			char *d = so_pide((size_t)(b - a) * 4 + 1);
			u32 m = 0;

			if (d) {
				for (i = a; i < b; i++)
					m += (u32)utf8_pon(R.t[i], d + m);
				so_al_portapapeles(d, m);
				so_suelta(d);
			}
			if (k == 24)
				borra_sel();
		}
	} else if (k == 22) {
		u32 n;
		char *p = so_del_portapapeles(&n);

		if (p) {
			borra_sel();
			inserta_utf8(p);
			so_suelta(p);
		}
	} else if (k >= 32 && k != 127 && (k < 0x100 || k >= 0x2000)) {
		borra_sel();
		inserta(&k, 1);
	}
}

/* rodea lo seleccionado (o pone en el cursor) a y b */
static void rodea(const char *a, const char *b)
{
	int i0 = R.sel >= 0 && R.sel != R.cur ? (R.sel < R.cur ? R.sel : R.cur) : R.cur;
	int i1 = R.sel >= 0 && R.sel != R.cur ? (R.sel < R.cur ? R.cur : R.sel) : R.cur;

	R.sel = -1;
	R.cur = i1;
	inserta_utf8(b);
	R.cur = i0;
	inserta_utf8(a);
	R.cur = i1 + (int)utf8_cuenta(a, (u32)strlen(a));
	R.foco = F_TEXTO;
}

/* el bucle de la redacción: 1 si se ha enviado, 0 si se quiere cerrar */
static int bucle(void)
{
	Suceso s;

	for (;;) {
		pinta();
		if (!ui_suceso(&s, 200))
			continue;
		if (s.clase == UE_TAMANO) {
			van = (int)s.a;
			val = (int)s.b;
			continue;
		}
		if (s.clase == UE_CERRAR)
			return 0;
		if (s.clase == UE_RUEDA && R.foco == F_TEXTO) {
			R.desde -= 3 * (int)(signed char)(s.c & 0xFF);
			if (R.desde < 0)
				R.desde = 0;
			{
				/* (que el cursor siga en lo que se ve) */
				int filas = (R.eal - 8) / (LT + 2), cl = linea_de(R.cur);

				if (cl < R.desde && R.desde < R.nlin)
					R.cur = R.lin[R.desde];
				else if (cl >= R.desde + filas && R.desde + filas - 1 < R.nlin)
					R.cur = R.lin[R.desde + filas - 1];
			}
			continue;
		}
		if (s.clase == UE_BAJA) {
			int z = zona_en((int)s.a, (int)s.b);

			if (z >= 50 && z < 60) {
				elige_sug(z - 50);
				continue;
			}
			R.nsug = 0;
			if (z >= 20 && z < 24) {
				R.foco = z - 20;
				campo_clic(&R.f[R.foco], 90, (int)s.a);
			} else if (z == 30) {
				int l = R.desde + ((int)s.b - R.ey - 4) / (LT + 2), c = ((int)s.a - R.ex - 8 + LA / 2) / LA;

				R.foco = F_TEXTO;
				if (l >= R.nlin)
					l = R.nlin - 1;
				if (l < 0)
					l = 0;
				{
					int fin = l + 1 < R.nlin ? R.lin[l + 1] - 1 : R.n;

					R.cur = R.lin[l] + (c < 0 ? 0 : c);
					if (R.cur > fin)
						R.cur = fin < R.lin[l] ? R.lin[l] : fin;
					if (R.cur > R.n)
						R.cur = R.n;
				}
				R.sel = -1;
			} else if (z >= 40 && z < 40 + MAX_ADJ) {
				int k = z - 40;
				char t[300];

				snprintf(t, sizeof t, "\xc2\xbfQuitar el adjunto \xc2\xab%s\xc2\xbb?", R.adj_nombre[k]);
				if (pregunta("Adjuntos", t, "Quitarlo", "No", NULL) == 0) {
					so_suelta(R.adj_datos[k]);
					memmove(&R.adj_ruta[k], &R.adj_ruta[k + 1], (size_t)(R.nadj - k - 1) * sizeof R.adj_ruta[0]);
					memmove(&R.adj_nombre[k], &R.adj_nombre[k + 1], (size_t)(R.nadj - k - 1) * sizeof R.adj_nombre[0]);
					memmove(&R.adj_datos[k], &R.adj_datos[k + 1], (size_t)(R.nadj - k - 1) * sizeof R.adj_datos[0]);
					memmove(&R.adj_n[k], &R.adj_n[k + 1], (size_t)(R.nadj - k - 1) * sizeof R.adj_n[0]);
					R.nadj--;
				}
			} else if (z == 1) {
				if (envia())
					return 1;
			} else if (z == 2)
				guarda_borrador();
			else if (z == 3)
				adjunta();
			else if (z == 4) {
				char d[400];

				if (ventana_libreta(d, sizeof d)) {
					int f = R.foco <= F_CCO ? R.foco : F_PARA;
					char t[CAMPO_MAX * 4];

					campo_saca(&R.f[f], t, sizeof t);
					if (t[0])
						strncat(t, ", ", sizeof t - strlen(t) - 1);
					strncat(t, d, sizeof t - strlen(t) - 1);
					campo_pon(&R.f[f], t);
				}
			} else if (z == 5)
				R.html = !R.html;
			else if (z == 6)
				rodea("*", "*");
			else if (z == 7)
				rodea("_", "_");
			else if (z == 8) {
				int l = linea_de(R.cur);

				R.cur = R.lin[l];
				inserta_utf8("- ");
				R.foco = F_TEXTO;
			} else if (z == 9) {
				char u[300] = "https://";

				if (pide_texto("Enlace", "La direcci\xc3\xb3n (https://...):", u, sizeof u, 0)) {
					char a[340];

					snprintf(a, sizeof a, "](%s)", u);
					rodea("[", a);
				}
			} else if (z == 10)
				return 0;
			continue;
		}
		if (s.clase != UE_TECLA)
			continue;
		if (s.a == UT_ESC) {
			if (R.nsug) {
				R.nsug = 0;
				continue;
			}
			return 0;
		}
		if (s.a == 19) {	/* Ctrl+S */
			guarda_borrador();
			continue;
		}
		if (s.a == 5 || (s.a == UT_INTRO && (s.b & UC_CONTROL))) {	/* Ctrl+E, Ctrl+Intro: enviar */
			if (envia())
				return 1;
			continue;
		}
		if (s.a == 2 && R.html) {	/* Ctrl+B */
			rodea("*", "*");
			continue;
		}
		if (R.nsug && R.foco <= F_CCO) {
			if (s.a == UT_ABAJO) {
				R.sug_cur = (R.sug_cur + 1) % R.nsug;
				continue;
			}
			if (s.a == UT_ARRIBA) {
				R.sug_cur = (R.sug_cur + R.nsug - 1) % R.nsug;
				continue;
			}
			if (s.a == UT_INTRO || s.a == UT_TAB) {
				elige_sug(R.sug_cur);
				continue;
			}
		}
		if (s.a == UT_TAB) {
			R.foco = (R.foco + 1) % 5;
			R.nsug = 0;
			continue;
		}
		if (s.a == UT_MAYUS_TAB) {
			R.foco = (R.foco + 4) % 5;
			R.nsug = 0;
			continue;
		}
		if (R.foco == F_TEXTO)
			tecla_texto(s.a, s.b);
		else {
			if (s.a == UT_INTRO) {
				R.foco++;
				continue;
			}
			if (campo_tecla(&R.f[R.foco], s.a, s.b))
				R.cambiado = 1;
			sugiere();
		}
	}
	return 0;
}

void redacta(int como, const char *para)
{
	void (*fondo)(void) = repinta_fondo;
	extern u32 uid_abierto(void);

	if (!ses) {
		aviso("Escribir", "Primero hace falta una cuenta (Cuentas, Cuenta nueva).");
		return;
	}
	prepara(como, para);
	R.uid_original = como == RD_NUEVO ? 0 : uid_abierto();
	if (como == RD_BORRADOR)
		R.uid_borrador = R.uid_original;
	so_apunta("redactar: %s", como == RD_NUEVO ? "nuevo" : como == RD_RESPONDER ? "responder" : como == RD_TODOS ? "responder a todos" :
		  como == RD_REENVIAR ? "reenviar" : "borrador");
	for (;;) {
		int r;

		repinta_fondo = pinta;
		if (bucle())
			break;
		if (!R.cambiado)
			break;
		r = pregunta("Cerrar", "\xc2\xbfGuardar lo escrito como borrador?", "Guardar", "No guardar", "Seguir escribiendo");
		if (r == 0) {
			guarda_borrador();
			break;
		}
		if (r == 1)
			break;
	}
	repinta_fondo = fondo;
	limpia();
}
