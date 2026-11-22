/* imap.c - Correo: el cliente IMAP4rev1 (RFC 3501), con IDLE (RFC 2177),
 * UIDPLUS (RFC 4315), MOVE (RFC 6851), SPECIAL-USE (RFC 6154) y LITERAL+
 * (RFC 7888) si el servidor los tiene. Todo por UID.
 *
 * Cada respuesta se lee entera (sus literales {n} dentro, justo detrás de
 * su «{n}», sin el CRLF) en un búfer con tope, y luego se mira con un
 * analizador sin recursión (tok_*). Lo que diga el servidor es hostil: una
 * respuesta rota se salta, un literal de más del tope corta la conexión,
 * nunca se lee fuera de lo llegado.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "correo.h"

#define MAX_RESPUESTA (64u << 20)

/* ---- lo pequeño ---- */
static int minus(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int igual_n(const u8 *a, u32 n, const char *b)
{
	u32 i;

	for (i = 0; i < n; i++)
		if (!b[i] || minus(a[i]) != minus((u8)b[i]))
			return 0;
	return b[n] == 0;
}

static int empieza(const char *a, const char *b)
{
	for (; *b; a++, b++)
		if (minus((u8)*a) != minus((u8)*b))
			return 0;
	return 1;
}

static u32 num_de(const u8 *p, u32 n)
{
	u32 v = 0, i;

	for (i = 0; i < n && p[i] >= '0' && p[i] <= '9'; i++) {
		if (v > 429496728u)
			return 0xFFFFFFFFu;
		v = v * 10 + (u32)(p[i] - '0');
	}
	return v;
}

/* =============================================================================
 *  El analizador de lo que dice el servidor
 * ============================================================================= */
enum { TK_FIN, TK_ATOMO, TK_CADENA, TK_NIL, TK_ABRE, TK_CIERRA };

typedef struct {
	const u8 *p, *fin;
} Tok;

/* lo siguiente: su clase y, si es un átomo o una cadena, dónde está. Las
 * cadenas entre comillas vienen con sus «\» (tok_copia las quita) */
static int tok(Tok *t, const u8 **v, u32 *n, int *entre_comillas)
{
	const u8 *p = t->p;

	*v = NULL;
	*n = 0;
	if (entre_comillas)
		*entre_comillas = 0;
	while (p < t->fin && (*p == ' ' || *p == '\t'))
		p++;
	if (p >= t->fin) {
		t->p = p;
		return TK_FIN;
	}
	if (*p == '(') {
		t->p = p + 1;
		return TK_ABRE;
	}
	if (*p == ')') {
		t->p = p + 1;
		return TK_CIERRA;
	}
	if (*p == '"') {
		const u8 *q = ++p;

		while (q < t->fin && *q != '"') {
			if (*q == '\\' && q + 1 < t->fin)
				q++;
			q++;
		}
		*v = p;
		*n = (u32)(q - p);
		t->p = q < t->fin ? q + 1 : q;
		if (entre_comillas)
			*entre_comillas = 1;
		return TK_CADENA;
	}
	if (*p == '{') {
		/* un literal: {n} o {n+}, y los n octetos justo detrás */
		const u8 *q = p + 1;
		u32 k = 0;

		while (q < t->fin && *q >= '0' && *q <= '9') {
			if (k > 0x0FFFFFFF)
				break;
			k = k * 10 + (u32)(*q - '0');
			q++;
		}
		if (q < t->fin && *q == '+')
			q++;
		if (q < t->fin && *q == '}' && (u32)(t->fin - q - 1) >= k) {
			*v = q + 1;
			*n = k;
			t->p = q + 1 + k;
			return TK_CADENA;
		}
		/* (mal hecho: como un átomo) */
	}
	{
		const u8 *q = p;
		int corchete = 0;

		while (q < t->fin) {
			if (*q == '[')
				corchete++;
			else if (*q == ']' && corchete)
				corchete--;
			else if (!corchete && (*q == ' ' || *q == '(' || *q == ')' || *q == '"'))
				break;
			q++;
		}
		if (q == p)
			q++;
		*v = p;
		*n = (u32)(q - p);
		t->p = q;
		if (*n == 3 && igual_n(p, 3, "NIL"))
			return TK_NIL;
		return TK_ATOMO;
	}
}

/* salta un valor entero (una lista con lo que lleve dentro, sin recursión) */
static int tok_salta(Tok *t)
{
	const u8 *v;
	u32 n;
	int k = tok(t, &v, &n, NULL), hondo = 0;

	if (k != TK_ABRE)
		return k == TK_FIN ? -1 : 0;
	hondo = 1;
	while (hondo > 0) {
		k = tok(t, &v, &n, NULL);
		if (k == TK_FIN)
			return -1;
		if (k == TK_ABRE)
			hondo++;
		else if (k == TK_CIERRA)
			hondo--;
	}
	return 0;
}

/* copia una cadena (sin las «\» de las comillas) */
static void tok_copia(const u8 *v, u32 n, int comillas, char *d, u32 cabe)
{
	u32 i, k = 0;

	if (!cabe)
		return;
	for (i = 0; i < n && k + 1 < cabe; i++) {
		if (comillas && v[i] == '\\' && i + 1 < n)
			i++;
		d[k++] = (char)v[i];
	}
	d[k] = 0;
}

/* =============================================================================
 *  El UTF-7 modificado (los nombres de las carpetas)
 * ============================================================================= */
static const char b64m[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";

static int b64m_val(char c)
{
	const char *p = c ? strchr(b64m, c) : NULL;

	return p ? (int)(p - b64m) : -1;
}

void utf7m_a_utf8(const char *t, char *d, u32 cabe)
{
	u32 k = 0;

	if (!cabe)
		return;
	while (*t && k + 5 < cabe) {
		if (*t != '&') {
			d[k++] = *t++;
			continue;
		}
		t++;
		if (*t == '-') {
			d[k++] = '&';
			t++;
			continue;
		}
		{
			u32 bits = 0, nb = 0, alto = 0;

			while (*t && *t != '-') {
				int v = b64m_val(*t++);

				if (v < 0)
					break;
				bits = bits << 6 | (u32)v;
				nb += 6;
				if (nb >= 16) {
					u32 u = (bits >> (nb - 16)) & 0xFFFF;

					nb -= 16;
					if (u >= 0xD800 && u < 0xDC00)
						alto = u;
					else {
						if (u >= 0xDC00 && u < 0xE000 && alto)
							u = 0x10000 + ((alto - 0xD800) << 10) + (u - 0xDC00);
						alto = 0;
						if (k + 5 < cabe)
							k += (u32)utf8_pon(u, d + k);
					}
				}
			}
			if (*t == '-')
				t++;
		}
	}
	d[k] = 0;
}

void utf8_a_utf7m(const char *t, char *d, u32 cabe)
{
	u32 k = 0, i = 0, n = (u32)strlen(t);

	if (!cabe)
		return;
	while (i < n && k + 1 < cabe) {
		u8 c = (u8)t[i];

		if (c >= 0x20 && c < 0x7F) {
			if (c == '&') {
				if (k + 3 >= cabe)
					break;
				d[k++] = '&';
				d[k++] = '-';
			} else
				d[k++] = (char)c;
			i++;
			continue;
		}
		if (k + 2 >= cabe)
			break;
		d[k++] = '&';
		{
			u32 bits = 0, nb = 0;

			while (i < n && ((u8)t[i] < 0x20 || (u8)t[i] >= 0x7F)) {
				u32 u = utf8_letra((const u8 *)t, n, &i), w[2], j, m = 1;

				if (u >= 0x10000) {
					u -= 0x10000;
					w[0] = 0xD800 + (u >> 10);
					w[1] = 0xDC00 + (u & 0x3FF);
					m = 2;
				} else
					w[0] = u;
				for (j = 0; j < m; j++) {
					bits = bits << 16 | w[j];
					nb += 16;
					while (nb >= 6 && k + 2 < cabe) {
						d[k++] = b64m[(bits >> (nb - 6)) & 63];
						nb -= 6;
					}
				}
			}
			if (nb && k + 2 < cabe)
				d[k++] = b64m[(bits << (6 - nb)) & 63];
		}
		if (k + 1 < cabe)
			d[k++] = '-';
	}
	d[k] = 0;
}

/* =============================================================================
 *  Leer una respuesta entera
 * ============================================================================= */
static void error_de(Imap *m, const char *f, const char *extra)
{
	snprintf(m->error, sizeof m->error, f, extra ? extra : "");
}

static int r_pon(Imap *m, const void *d, u32 n)
{
	if (m->rn + n + 1 > m->rcabe) {
		u32 c = m->rcabe ? m->rcabe : 8192;
		u8 *q;

		while (c < m->rn + n + 1)
			c *= 2;
		if (c > MAX_RESPUESTA + 65536) {
			error_de(m, "El servidor manda una respuesta demasiado grande.%s", NULL);
			return -1;
		}
		q = so_repide(m->r, c);
		if (!q) {
			error_de(m, "No hay memoria para la respuesta del servidor.%s", NULL);
			return -1;
		}
		m->r = q;
		m->rcabe = c;
	}
	memcpy(m->r + m->rn, d, n);
	m->rn += n;
	m->r[m->rn] = 0;
	return 0;
}

/* lee una respuesta (un renglón, y los literales que lleve con los renglones
 * que siguen): queda en m->r (m->rn octetos) */
static int lee_respuesta(Imap *m)
{
	char *l;
	int n;

	m->rn = 0;
	for (;;) {
		u32 k, lit = 0;

		n = con_renglon(&m->con, &l);
		if (n < 0) {
			snprintf(m->error, sizeof m->error, "%s", m->con.texto);
			return -1;
		}
		/* ¿acaba en {n} o {n+}? */
		k = (u32)n;
		if (k >= 3 && l[k - 1] == '}') {
			u32 j = k - 2, peso = 1;

			if (l[j] == '+')
				j--;
			while (j > 0 && l[j] >= '0' && l[j] <= '9') {
				if (peso > 100000000u) {
					lit = 0xFFFFFFFF;
					break;
				}
				lit += (u32)(l[j] - '0') * peso;
				peso *= 10;
				j--;
			}
			if (l[j] != '{' || j + 2 >= k - (l[k - 2] == '+'))
				lit = 0, j = 0;
			if (lit && (lit == 0xFFFFFFFF || lit > m->max_literal)) {
				snprintf(m->error, sizeof m->error, "El servidor manda algo demasiado grande (%u octetos).", (unsigned)lit);
				return -1;
			}
			if (l[j] == '{' && j + 2 < k) {
				if (r_pon(m, l, k))
					return -1;
				if (lit) {
					u32 a = m->rn;

					if (r_pon(m, "", 0))
						return -1;
					/* sitio para el literal */
					if (m->rn + lit + 1 > m->rcabe) {
						u32 c = m->rcabe;
						u8 *q;

						while (c < m->rn + lit + 1)
							c *= 2;
						q = so_repide(m->r, c);
						if (!q) {
							error_de(m, "No hay memoria.%s", NULL);
							return -1;
						}
						m->r = q;
						m->rcabe = c;
					}
					if (con_lee(&m->con, m->r + a, lit) < 0) {
						snprintf(m->error, sizeof m->error, "%s", m->con.texto);
						return -1;
					}
					m->rn = a + lit;
					m->r[m->rn] = 0;
				}
				continue;	/* sigue en el renglón de detrás */
			}
		}
		if (r_pon(m, l, k))
			return -1;
		return 0;
	}
}

/* =============================================================================
 *  Las respuestas sin etiqueta («*»)
 * ============================================================================= */
static void lee_caps(Imap *m, Tok *t)
{
	const u8 *v;
	u32 n;
	int k;

	m->caps = 0;
	while ((k = tok(t, &v, &n, NULL)) == TK_ATOMO) {
		if (n && v[n - 1] == ']')
			n--;
		if (igual_n(v, n, "IMAP4rev1")) m->caps |= IC_IMAP4REV1;
		else if (igual_n(v, n, "IDLE")) m->caps |= IC_IDLE;
		else if (igual_n(v, n, "UIDPLUS")) m->caps |= IC_UIDPLUS;
		else if (igual_n(v, n, "MOVE")) m->caps |= IC_MOVE;
		else if (igual_n(v, n, "SPECIAL-USE")) m->caps |= IC_SPECIAL_USE;
		else if (igual_n(v, n, "LITERAL+")) m->caps |= IC_LITERALPLUS;
		else if (igual_n(v, n, "STARTTLS")) m->caps |= IC_STARTTLS;
		else if (igual_n(v, n, "LOGINDISABLED")) m->caps |= IC_LOGINDISABLED;
		else if (igual_n(v, n, "AUTH=PLAIN")) m->caps |= IC_AUTH_PLAIN;
		else if (igual_n(v, n, "UNSELECT")) m->caps |= IC_UNSELECT;
	}
}

/* los [CÓDIGO ...] de las respuestas de estado */
static void lee_codigo(Imap *m, const u8 *p, u32 n)
{
	Tok t;
	const u8 *v;
	u32 k;

	if (n < 2 || p[0] != '[')
		return;
	t.p = p + 1;
	t.fin = p + n;
	if (tok(&t, &v, &k, NULL) != TK_ATOMO)
		return;
	if (k && v[k - 1] == ']')
		k--;
	if (igual_n(v, k, "CAPABILITY"))
		lee_caps(m, &t);
	else if (igual_n(v, k, "UIDVALIDITY") && tok(&t, &v, &k, NULL) == TK_ATOMO)
		m->uidvalidity = num_de(v, k);
	else if (igual_n(v, k, "UIDNEXT") && tok(&t, &v, &k, NULL) == TK_ATOMO)
		m->uidnext = num_de(v, k);
	else if (igual_n(v, k, "READ-ONLY"))
		m->solo_leer = 1;
	else if (igual_n(v, k, "READ-WRITE"))
		m->solo_leer = 0;
	else if (igual_n(v, k, "APPENDUID") && tok(&t, &v, &k, NULL) == TK_ATOMO && tok(&t, &v, &k, NULL) == TK_ATOMO)
		m->copyuid = num_de(v, k);
}

typedef struct {
	int (*cada)(Imap *m, void *u, u32 num, const u8 *clase, u32 cn, Tok *t);
	void *u;
} Oyente;

/* lo que se mira de todas las respuestas sin etiqueta */
static void sin_etiqueta(Imap *m, Oyente *o)
{
	Tok t;
	const u8 *v, *c;
	u32 n, cn, num = 0;
	int k;

	t.p = m->r + 2;
	t.fin = m->r + m->rn;
	k = tok(&t, &v, &n, NULL);
	if (k != TK_ATOMO)
		return;
	if (v[0] >= '0' && v[0] <= '9') {
		num = num_de(v, n);
		if (tok(&t, &c, &cn, NULL) != TK_ATOMO)
			return;
	} else
		c = v, cn = n;
	if (igual_n(c, cn, "OK") || igual_n(c, cn, "NO") || igual_n(c, cn, "BAD") || igual_n(c, cn, "PREAUTH")) {
		while (t.p < t.fin && *t.p == ' ')
			t.p++;
		lee_codigo(m, t.p, (u32)(t.fin - t.p));
		return;
	}
	if (igual_n(c, cn, "BYE")) {
		while (t.p < t.fin && *t.p == ' ')
			t.p++;
		snprintf(m->error, sizeof m->error, "El servidor cierra: %.*s", (int)(t.fin - t.p > 200 ? 200 : t.fin - t.p), t.p);
		return;
	}
	if (igual_n(c, cn, "CAPABILITY")) {
		lee_caps(m, &t);
		return;
	}
	if (igual_n(c, cn, "EXISTS")) {
		m->exists = num;
		m->nuevo_exists = num;
	} else if (igual_n(c, cn, "EXPUNGE")) {
		m->hubo_expunge++;
		if (m->exists)
			m->exists--;
	} else if (igual_n(c, cn, "FETCH"))
		m->hubo_banderas++;
	if (o && o->cada)
		o->cada(m, o->u, num, c, cn, &t);
}

/* una orden y sus respuestas hasta la etiquetada: 0 si OK; 1 si NO o BAD
 * (m->error lo dice); <0 si la conexión ha fallado */
static int orden(Imap *m, Oyente *o, const char *f, ...)
{
	char t[3000], et[16];
	va_list a;
	int n;

	if (!m->con.c) {
		error_de(m, "No hay conexi\xc3\xb3n con el servidor.%s", NULL);
		return -1;
	}
	snprintf(et, sizeof et, "c%u", (unsigned)++m->etiqueta);
	n = snprintf(t, sizeof t, "%s ", et);
	va_start(a, f);
	n += vsnprintf(t + n, sizeof t - (u32)n, f, a);
	va_end(a);
	if (n >= (int)sizeof t - 3) {
		error_de(m, "Orden demasiado larga.%s", NULL);
		return 1;
	}
	t[n++] = '\r';
	t[n++] = '\n';
	if (con_envia(&m->con, t, (u32)n) < 0) {
		snprintf(m->error, sizeof m->error, "%s", m->con.texto);
		return -1;
	}
	for (;;) {
		if (lee_respuesta(m) < 0)
			return -1;
		if (m->rn >= 2 && m->r[0] == '*' && m->r[1] == ' ') {
			sin_etiqueta(m, o);
			continue;
		}
		if (m->r[0] == '+')
			continue;	/* (una continuación que no se ha pedido: se salta) */
		if (!strncmp((char *)m->r, et, strlen(et)) && m->r[strlen(et)] == ' ') {
			const char *p = (char *)m->r + strlen(et) + 1;

			if (empieza(p, "OK")) {
				p += 2;
				while (*p == ' ')
					p++;
				lee_codigo(m, (const u8 *)p, (u32)strlen(p));
				return 0;
			}
			snprintf(m->error, sizeof m->error, "El servidor dice: %.250s", p);
			{
				const char *q = p;

				while (*q && *q != ' ')
					q++;
				while (*q == ' ')
					q++;
				lee_codigo(m, (const u8 *)q, (u32)strlen(q));
			}
			return 1;
		}
		/* (una respuesta con otra etiqueta, o rota: se salta) */
	}
}

/* manda una orden con literales: «partes» se alternan texto y literal; los
 * literales se mandan como {n+} si hay LITERAL+, y si no se espera al «+» */
static int orden_literal(Imap *m, Oyente *o, const char *antes, const u8 *lit, u32 ln, const char *despues)
{
	char t[600], et[16];
	int n;

	if (!m->con.c)
		return -1;
	snprintf(et, sizeof et, "c%u", (unsigned)++m->etiqueta);
	n = snprintf(t, sizeof t, "%s %s{%u%s}\r\n", et, antes, (unsigned)ln, (m->caps & IC_LITERALPLUS) ? "+" : "");
	if (n >= (int)sizeof t || con_envia(&m->con, t, (u32)n) < 0)
		goto mal;
	if (!(m->caps & IC_LITERALPLUS)) {
		/* esperar al «+» */
		for (;;) {
			if (lee_respuesta(m) < 0)
				return -1;
			if (m->r[0] == '+')
				break;
			if (m->rn >= 2 && m->r[0] == '*') {
				sin_etiqueta(m, o);
				continue;
			}
			if (!strncmp((char *)m->r, et, strlen(et))) {
				snprintf(m->error, sizeof m->error, "El servidor dice: %.250s", (char *)m->r + strlen(et) + 1);
				return 1;
			}
		}
	}
	if (con_envia(&m->con, lit, ln) < 0)
		goto mal;
	n = snprintf(t, sizeof t, "%s\r\n", despues);
	if (con_envia(&m->con, t, (u32)n) < 0)
		goto mal;
	for (;;) {
		if (lee_respuesta(m) < 0)
			return -1;
		if (m->rn >= 2 && m->r[0] == '*' && m->r[1] == ' ') {
			sin_etiqueta(m, o);
			continue;
		}
		if (m->r[0] == '+')
			continue;
		if (!strncmp((char *)m->r, et, strlen(et)) && m->r[strlen(et)] == ' ') {
			const char *p = (char *)m->r + strlen(et) + 1;

			if (empieza(p, "OK")) {
				p += 2;
				while (*p == ' ')
					p++;
				lee_codigo(m, (const u8 *)p, (u32)strlen(p));
				return 0;
			}
			snprintf(m->error, sizeof m->error, "El servidor dice: %.250s", p);
			return 1;
		}
	}
mal:
	snprintf(m->error, sizeof m->error, "%s", m->con.texto);
	return -1;
}

/* una cadena para una orden: entre comillas (con \" y \\), o NULL si lleva
 * algo que no puede ir así (CR, LF, 8 bits): entonces hay que usar un literal */
static int entrecomilla(const char *s, char *d, u32 cabe)
{
	u32 k = 0;

	if (cabe < 3)
		return -1;
	d[k++] = '"';
	for (; *s; s++) {
		u8 c = (u8)*s;

		if (c == '\r' || c == '\n' || c >= 0x80 || c == 0)
			return -1;
		if (k + 4 >= cabe)
			return -1;
		if (c == '"' || c == '\\')
			d[k++] = '\\';
		d[k++] = (char)c;
	}
	d[k++] = '"';
	d[k] = 0;
	return 0;
}

/* =============================================================================
 *  Entrar
 * ============================================================================= */
int imap_conecta(Imap *m, const Cuenta *c, const char *clave)
{
	char u[300], k[300];
	int r, modo = c->modo_entrada;

	memset(m, 0, sizeof *m);
	m->max_literal = (ajustes.max_mensaje_kb ? ajustes.max_mensaje_kb : 25 * 1024) * 1024u + 65536;
	if (modo == SO_PLANO && (!c->sin_cifrar || !servidor_local(c->entrada))) {
		snprintf(m->error, sizeof m->error, "No se manda la contrase\xc3\xb1" "a sin cifrar a %s: use TLS (993) o STARTTLS (143).", c->entrada);
		return -1;
	}
	if (con_abre(&m->con, c->entrada, c->puerto_entrada, modo, 20000)) {
		snprintf(m->error, sizeof m->error, "%s", m->con.texto);
		so_apunta("imap: %s:%u: %s", c->entrada, (unsigned)c->puerto_entrada, m->con.texto);
		return -1;
	}
	m->con.espera_ms = 60000;
	so_apunta("imap: conectado con %s:%u (%s)", c->entrada, (unsigned)c->puerto_entrada,
		  m->con.cifrado[0] ? m->con.cifrado : "SIN CIFRAR");
	if (modo != SO_STARTTLS_IMAP) {
		/* el saludo (con STARTTLS ya lo ha leído TLS.BIN) */
		if (lee_respuesta(m) < 0)
			goto mal;
		if (empieza((char *)m->r, "* BYE")) {
			snprintf(m->error, sizeof m->error, "El servidor no deja entrar: %.200s", (char *)m->r + 5);
			goto mal;
		}
		if (!empieza((char *)m->r, "* OK") && !empieza((char *)m->r, "* PREAUTH")) {
			snprintf(m->error, sizeof m->error, "El servidor no saluda como IMAP: %.100s", (char *)m->r);
			goto mal;
		}
		sin_etiqueta(m, NULL);
	}
	if (!(m->caps & IC_IMAP4REV1) || modo == SO_STARTTLS_IMAP) {
		r = orden(m, NULL, "CAPABILITY");
		if (r < 0)
			goto mal;
	}
	/* entrar: AUTHENTICATE PLAIN si lo tiene; si no, LOGIN */
	if ((m->caps & IC_AUTH_PLAIN) || (m->caps & IC_LOGINDISABLED)) {
		u8 p[400];
		u32 un = (u32)strlen(c->usuario), kn = (u32)strlen(clave), bn;
		char *b;

		if (un + kn + 2 > sizeof p) {
			error_de(m, "El usuario o la contrase\xc3\xb1" "a son demasiado largos.%s", NULL);
			goto mal;
		}
		p[0] = 0;
		memcpy(p + 1, c->usuario, un);
		p[1 + un] = 0;
		memcpy(p + 2 + un, clave, kn);
		b = b64_codifica(p, un + kn + 2, &bn, 0);
		memset(p, 0, sizeof p);
		if (!b)
			goto mal;
		snprintf(k, sizeof k, "%s", b);
		memset(b, 0, bn);
		so_suelta(b);
		{
			/* AUTHENTICATE PLAIN, y la respuesta tras el «+» */
			char et[16], t[400];
			int n;

			snprintf(et, sizeof et, "c%u", (unsigned)++m->etiqueta);
			n = snprintf(t, sizeof t, "%s AUTHENTICATE PLAIN\r\n", et);
			if (con_envia(&m->con, t, (u32)n) < 0)
				goto mal_con;
			for (;;) {
				if (lee_respuesta(m) < 0)
					goto mal;
				if (m->r[0] == '+')
					break;
				if (m->r[0] == '*') {
					sin_etiqueta(m, NULL);
					continue;
				}
				snprintf(m->error, sizeof m->error, "El servidor no acepta la entrada: %.200s", (char *)m->r);
				goto mal;
			}
			n = snprintf(t, sizeof t, "%s\r\n", k);
			memset(k, 0, sizeof k);
			r = con_envia(&m->con, t, (u32)n);
			memset(t, 0, sizeof t);
			if (r < 0)
				goto mal_con;
			for (;;) {
				if (lee_respuesta(m) < 0)
					goto mal;
				if (m->r[0] == '*') {
					sin_etiqueta(m, NULL);
					continue;
				}
				if (!strncmp((char *)m->r, et, strlen(et))) {
					const char *q = (char *)m->r + strlen(et) + 1;

					if (empieza(q, "OK")) {
						q += 2;
						while (*q == ' ')
							q++;
						lee_codigo(m, (const u8 *)q, (u32)strlen(q));
						r = 0;
					} else {
						snprintf(m->error, sizeof m->error, "No se ha podido entrar (\xc2\xbf" "est\xc3\xa1 bien la contrase\xc3\xb1" "a?). El servidor dice: %.200s", q);
						r = 1;
					}
					break;
				}
			}
		}
	} else {
		if (entrecomilla(c->usuario, u, sizeof u) || entrecomilla(clave, k, sizeof k)) {
			/* (con letras raras: en literales) */
			char a[400];

			if (entrecomilla(c->usuario, u, sizeof u))
				snprintf(u, sizeof u, "\"\"");
			snprintf(a, sizeof a, "LOGIN %s ", u);
			r = orden_literal(m, NULL, a, (const u8 *)clave, (u32)strlen(clave), "");
		} else {
			r = orden(m, NULL, "LOGIN %s %s", u, k);
			memset(k, 0, sizeof k);
		}
		if (r == 1) {
			char t[300];

			snprintf(t, sizeof t, "%s", m->error);
			snprintf(m->error, sizeof m->error, "No se ha podido entrar (\xc2\xbf" "est\xc3\xa1 bien la contrase\xc3\xb1" "a?). %.200s", t);
		}
	}
	if (r)
		goto mal;
	m->dentro = 1;
	if (!(m->caps & IC_IMAP4REV1) || !(m->caps & (IC_IDLE | IC_UIDPLUS | IC_MOVE | IC_SPECIAL_USE)))
		orden(m, NULL, "CAPABILITY");
	so_apunta("imap: dentro como %s (IDLE %s, UIDPLUS %s, MOVE %s, SPECIAL-USE %s)", c->usuario,
		  m->caps & IC_IDLE ? "s\xc3\xad" : "no", m->caps & IC_UIDPLUS ? "s\xc3\xad" : "no",
		  m->caps & IC_MOVE ? "s\xc3\xad" : "no", m->caps & IC_SPECIAL_USE ? "s\xc3\xad" : "no");
	return 0;
mal_con:
	snprintf(m->error, sizeof m->error, "%s", m->con.texto);
mal:
	so_apunta("imap: %s", m->error);
	con_cierra(&m->con);
	return -1;
}

void imap_cierra(Imap *m)
{
	if (m->con.c) {
		if (m->en_idle)
			imap_idle_acaba(m);
		m->con.espera_ms = 3000;
		orden(m, NULL, "LOGOUT");
	}
	con_cierra(&m->con);
	so_suelta(m->r);
	m->r = NULL;
	m->rn = m->rcabe = 0;
	m->dentro = 0;
}

/* =============================================================================
 *  Las carpetas
 * ============================================================================= */
typedef struct {
	Carpeta *v;
	int n, max;
} Lista;

static void marcas_por_nombre(Carpeta *c)
{
	/* sin SPECIAL-USE: por el nombre, como hacen todos */
	static const struct { const char *n; u32 m; } nombres[] = {
		{ "sent", CM_ENVIADOS }, { "sent items", CM_ENVIADOS }, { "sent messages", CM_ENVIADOS }, { "enviados", CM_ENVIADOS },
		{ "elementos enviados", CM_ENVIADOS }, { "[gmail]/sent mail", CM_ENVIADOS }, { "inbox.sent", CM_ENVIADOS },
		{ "drafts", CM_BORRADORES }, { "borradores", CM_BORRADORES }, { "inbox.drafts", CM_BORRADORES }, { "[gmail]/drafts", CM_BORRADORES },
		{ "trash", CM_PAPELERA }, { "deleted items", CM_PAPELERA }, { "deleted messages", CM_PAPELERA }, { "papelera", CM_PAPELERA },
		{ "elementos eliminados", CM_PAPELERA }, { "inbox.trash", CM_PAPELERA }, { "[gmail]/trash", CM_PAPELERA },
		{ "junk", CM_BASURA }, { "spam", CM_BASURA }, { "correo no deseado", CM_BASURA }, { "junk e-mail", CM_BASURA }, { "inbox.junk", CM_BASURA },
		{ "archive", CM_ARCHIVO }, { "archivo", CM_ARCHIVO }, { "archivados", CM_ARCHIVO },
	};
	u32 i;
	char t[200];

	for (i = 0; c->nombre[i] && i + 1 < sizeof t; i++)
		t[i] = (char)minus((u8)c->nombre[i]);
	t[i] = 0;
	for (i = 0; i < sizeof nombres / sizeof nombres[0]; i++)
		if (!strcmp(t, nombres[i].n)) {
			c->marcas |= nombres[i].m;
			return;
		}
}

static int de_list(Imap *m, void *u, u32 num, const u8 *c, u32 cn, Tok *t)
{
	Lista *l = u;
	Carpeta *f;
	const u8 *v;
	u32 n;
	int k, com;

	(void)m;
	(void)num;
	if (!igual_n(c, cn, "LIST") && !igual_n(c, cn, "LSUB"))
		return 0;
	if (l->n >= l->max)
		return 0;
	f = &l->v[l->n];
	memset(f, 0, sizeof *f);
	if (tok(t, &v, &n, NULL) != TK_ABRE)
		return 0;
	while ((k = tok(t, &v, &n, NULL)) == TK_ATOMO) {
		if (igual_n(v, n, "\\Noselect") || igual_n(v, n, "\\NonExistent")) f->marcas |= CM_NOSELECT;
		else if (igual_n(v, n, "\\HasChildren")) f->marcas |= CM_HIJOS;
		else if (igual_n(v, n, "\\Sent")) f->marcas |= CM_ENVIADOS;
		else if (igual_n(v, n, "\\Drafts")) f->marcas |= CM_BORRADORES;
		else if (igual_n(v, n, "\\Trash")) f->marcas |= CM_PAPELERA;
		else if (igual_n(v, n, "\\Junk")) f->marcas |= CM_BASURA;
		else if (igual_n(v, n, "\\Archive")) f->marcas |= CM_ARCHIVO;
		else if (igual_n(v, n, "\\All")) f->marcas |= CM_TODOS;
		else if (igual_n(v, n, "\\Flagged")) f->marcas |= CM_DESTACADOS;
	}
	if (k != TK_CIERRA)
		return 0;
	k = tok(t, &v, &n, &com);
	f->delim = k == TK_CADENA && n ? (char)v[n == 2 && v[0] == '\\' ? 1 : 0] : 0;
	k = tok(t, &v, &n, &com);
	if (k != TK_CADENA && k != TK_ATOMO)
		return 0;
	tok_copia(v, n, com, f->crudo, sizeof f->crudo);
	if (!f->crudo[0])
		return 0;
	if (!strcmp(f->crudo, "INBOX") || (strlen(f->crudo) == 5 && empieza(f->crudo, "inbox"))) {
		snprintf(f->crudo, sizeof f->crudo, "INBOX");
		snprintf(f->nombre, sizeof f->nombre, "Bandeja de entrada");
		f->marcas |= CM_ENTRADA;
	} else {
		char *p;

		utf7m_a_utf8(f->crudo, f->nombre, sizeof f->nombre);
		for (p = f->nombre; *p; p++)
			if (f->delim && *p == f->delim)
				f->nivel++;
		if (!(f->marcas & (CM_ENVIADOS | CM_BORRADORES | CM_PAPELERA | CM_BASURA | CM_ARCHIVO)))
			marcas_por_nombre(f);
	}
	/* (repetidas: no) */
	for (k = 0; k < l->n; k++)
		if (!strcmp(l->v[k].crudo, f->crudo)) {
			l->v[k].marcas |= f->marcas;
			return 0;
		}
	l->n++;
	return 0;
}

static int de_status(Imap *m, void *u, u32 num, const u8 *c, u32 cn, Tok *t)
{
	Carpeta *f = u;
	const u8 *v;
	u32 n;
	int k, com;
	char nom[256];

	(void)m;
	(void)num;
	if (!igual_n(c, cn, "STATUS"))
		return 0;
	k = tok(t, &v, &n, &com);
	if (k != TK_CADENA && k != TK_ATOMO)
		return 0;
	tok_copia(v, n, com, nom, sizeof nom);
	if (strcmp(nom, f->crudo) && !(f->marcas & CM_ENTRADA))
		return 0;
	if (tok(t, &v, &n, NULL) != TK_ABRE)
		return 0;
	for (;;) {
		const u8 *a, *b;
		u32 an, bn;

		if (tok(t, &a, &an, NULL) != TK_ATOMO || tok(t, &b, &bn, NULL) != TK_ATOMO)
			break;
		if (igual_n(a, an, "MESSAGES")) f->total = num_de(b, bn);
		else if (igual_n(a, an, "UNSEEN")) f->no_leidos = num_de(b, bn);
		else if (igual_n(a, an, "UIDNEXT")) f->uidnext = num_de(b, bn);
		else if (igual_n(a, an, "UIDVALIDITY")) f->uidvalidity = num_de(b, bn);
	}
	return 0;
}

int imap_status(Imap *m, Carpeta *c)
{
	char q[300];
	Oyente o = { de_status, c };

	if (c->marcas & CM_NOSELECT)
		return 0;
	if (entrecomilla(c->crudo, q, sizeof q))
		return 1;
	return orden(m, &o, "STATUS %s (MESSAGES UNSEEN UIDNEXT UIDVALIDITY)", q);
}

/* el orden de la lista: la entrada, luego por nombre (y las de dentro detrás de la suya) */
static int antes_que(const Carpeta *a, const Carpeta *b)
{
	if ((a->marcas & CM_ENTRADA) != (b->marcas & CM_ENTRADA))
		return (a->marcas & CM_ENTRADA) != 0;
	return strcmp(a->crudo, b->crudo) < 0;
}

int imap_carpetas(Imap *m, Carpeta *v, int max, int con_status)
{
	Lista l = { v, 0, max };
	Oyente o = { de_list, &l };
	int r, i, j;

	r = orden(m, &o, (m->caps & IC_SPECIAL_USE) ? "LIST \"\" \"*\" RETURN (SPECIAL-USE)" : "LIST \"\" \"*\"");
	if (r == 1 && (m->caps & IC_SPECIAL_USE))
		r = orden(m, &o, "LIST \"\" \"*\"");
	if (r)
		return -1;
	/* que esté la entrada aunque no la diga */
	for (i = 0; i < l.n && !(v[i].marcas & CM_ENTRADA); i++)
		;
	if (i == l.n && l.n < max) {
		memset(&v[l.n], 0, sizeof v[0]);
		snprintf(v[l.n].crudo, sizeof v[0].crudo, "INBOX");
		snprintf(v[l.n].nombre, sizeof v[0].nombre, "Bandeja de entrada");
		v[l.n].marcas = CM_ENTRADA;
		l.n++;
	}
	/* ordenar (de inserción: son pocas) */
	for (i = 1; i < l.n; i++) {
		Carpeta x = v[i];

		for (j = i; j > 0 && antes_que(&x, &v[j - 1]); j--)
			v[j] = v[j - 1];
		v[j] = x;
	}
	if (con_status)
		for (i = 0; i < l.n; i++)
			if (imap_status(m, &v[i]) < 0)
				return -1;
	return l.n;
}

int imap_selecciona(Imap *m, const char *crudo, int solo_leer)
{
	char q[300];
	int r;

	if (entrecomilla(crudo, q, sizeof q))
		return 1;
	m->uidvalidity = m->uidnext = m->exists = 0;
	m->solo_leer = solo_leer;
	r = orden(m, NULL, "%s %s", solo_leer ? "EXAMINE" : "SELECT", q);
	if (r == 0) {
		snprintf(m->sel, sizeof m->sel, "%s", crudo);
		m->nuevo_exists = m->exists;
		m->hubo_expunge = 0;
	} else
		m->sel[0] = 0;
	return r;
}

/* =============================================================================
 *  Buscar
 * ============================================================================= */
typedef struct {
	u32 *v;
	u32 n, cabe;
	int sin_memoria;
} Uids;

static int de_search(Imap *m, void *u, u32 num, const u8 *c, u32 cn, Tok *t)
{
	Uids *l = u;
	const u8 *v;
	u32 n;

	(void)m;
	(void)num;
	if (!igual_n(c, cn, "SEARCH"))
		return 0;
	while (tok(t, &v, &n, NULL) == TK_ATOMO) {
		u32 x = num_de(v, n);

		if (!x || x == 0xFFFFFFFF)
			continue;
		if (l->n == l->cabe) {
			u32 nc = l->cabe ? l->cabe * 2 : 1024;
			u32 *q;

			if (nc > 4000000u) {
				l->sin_memoria = 1;
				return 0;
			}
			q = so_repide(l->v, nc * sizeof(u32));
			if (!q) {
				l->sin_memoria = 1;
				return 0;
			}
			l->v = q;
			l->cabe = nc;
		}
		l->v[l->n++] = x;
	}
	return 0;
}

static int cmp_u32(const void *a, const void *b)
{
	u32 x = *(const u32 *)a, y = *(const u32 *)b;

	return x < y ? -1 : x > y;
}

void qsort(void *, size_t, size_t, int (*)(const void *, const void *));

int imap_busca_uids(Imap *m, const char *criterio, const char *texto, u32 **uids, u32 *n)
{
	Uids l = { NULL, 0, 0, 0 };
	Oyente o = { de_search, &l };
	int r;

	*uids = NULL;
	*n = 0;
	if (texto && texto[0]) {
		char a[200], q[600];
		int ascii = 1;
		const char *p;

		for (p = texto; *p; p++)
			if ((u8)*p >= 0x80 || *p == '"' || *p == '\\')
				ascii = 0;
		if (ascii && !entrecomilla(texto, q, sizeof q))
			r = orden(m, &o, "UID SEARCH %s %s", criterio, q);
		else {
			snprintf(a, sizeof a, "UID SEARCH CHARSET UTF-8 %s ", criterio);
			r = orden_literal(m, &o, a, (const u8 *)texto, (u32)strlen(texto), "");
		}
	} else
		r = orden(m, &o, "UID SEARCH %s", criterio);
	if (r || l.sin_memoria) {
		so_suelta(l.v);
		if (l.sin_memoria)
			error_de(m, "Demasiados resultados.%s", NULL);
		return r ? r : -1;
	}
	if (l.n > 1)
		qsort(l.v, l.n, sizeof(u32), cmp_u32);
	*uids = l.v;
	*n = l.n;
	return 0;
}

/* «1:5,7,9:12» de unos UID ordenados, desde *i; a lo sumo «cabe» octetos */
static void conjunto(const u32 *u, u32 n, u32 *i, char *d, u32 cabe)
{
	u32 k = 0;

	d[0] = 0;
	while (*i < n) {
		u32 a = u[*i], b = a, j = *i + 1;
		char t[24];
		int l;

		while (j < n && u[j] == b + 1)
			b = u[j++];
		l = a == b ? snprintf(t, sizeof t, "%s%u", k ? "," : "", (unsigned)a) :
			     snprintf(t, sizeof t, "%s%u:%u", k ? "," : "", (unsigned)a, (unsigned)b);
		if (k + (u32)l + 1 > cabe)
			break;
		memcpy(d + k, t, (size_t)l + 1);
		k += (u32)l;
		*i = j;
	}
}

/* =============================================================================
 *  Las fichas y las banderas
 * ============================================================================= */
static u32 banderas_de(Tok *t)
{
	const u8 *v;
	u32 n, b = 0;
	int k;

	if (tok(t, &v, &n, NULL) != TK_ABRE)
		return 0;
	while ((k = tok(t, &v, &n, NULL)) == TK_ATOMO) {
		if (igual_n(v, n, "\\Seen")) b |= B_LEIDO;
		else if (igual_n(v, n, "\\Flagged")) b |= B_DESTACADO;
		else if (igual_n(v, n, "\\Answered")) b |= B_RESPONDIDO;
		else if (igual_n(v, n, "\\Deleted")) b |= B_BORRADO;
		else if (igual_n(v, n, "\\Draft")) b |= B_BORRADOR;
		else if (igual_n(v, n, "$Forwarded")) b |= B_REENVIADO;
	}
	while (k != TK_CIERRA && k != TK_FIN)
		k = tok(t, &v, &n, NULL);
	return b;
}

typedef struct {
	int de_enviados;
	void (*cada)(void *u, Ficha *f);
	void (*cada_b)(void *u, u32 uid, u32 banderas);
	void *u;
	/* (imap_mensaje) */
	u32 uid;
	u8 *cuerpo;
	u32 n;
	int sin_memoria;
} Fetch;

static int de_fetch(Imap *m, void *u, u32 num, const u8 *c, u32 cn, Tok *t)
{
	Fetch *x = u;
	Ficha f;
	const u8 *v, *cab = NULL, *cuerpo = NULL;
	u32 n, cabn = 0, cuerpon = 0, uid = 0, ban = 0, tam = 0, fecha = 0;
	int k, hay_ban = 0;

	(void)m;
	(void)num;
	if (!igual_n(c, cn, "FETCH"))
		return 0;
	if (tok(t, &v, &n, NULL) != TK_ABRE)
		return 0;
	for (;;) {
		k = tok(t, &v, &n, NULL);
		if (k != TK_ATOMO)
			break;
		if (igual_n(v, n, "UID")) {
			if (tok(t, &v, &n, NULL) != TK_ATOMO)
				return 0;
			uid = num_de(v, n);
		} else if (igual_n(v, n, "FLAGS")) {
			ban = banderas_de(t);
			hay_ban = 1;
		} else if (igual_n(v, n, "RFC822.SIZE")) {
			if (tok(t, &v, &n, NULL) != TK_ATOMO)
				return 0;
			tam = num_de(v, n);
		} else if (igual_n(v, n, "INTERNALDATE")) {
			int com;

			k = tok(t, &v, &n, &com);
			if (k == TK_CADENA) {
				i64 s = mime_fecha_imap((const char *)v, n);

				fecha = s > 0 && s < 0xFFFFFFFFll ? (u32)s : 0;
			}
		} else if (n >= 5 && (igual_n(v, 5, "BODY[") || (n >= 4 && igual_n(v, 4, "RFC8")))) {
			const u8 *w;
			u32 wn;

			k = tok(t, &w, &wn, NULL);
			if (k == TK_CADENA) {
				/* lo de las cabeceras, o el mensaje entero */
				if (n >= 12 && igual_n(v, 12, "BODY[HEADER.")) {
					cab = w;
					cabn = wn;
				} else if (igual_n(v, n, "BODY[]") || igual_n(v, n, "RFC822")) {
					cuerpo = w;
					cuerpon = wn;
				} else if (igual_n(v, n, "BODY[HEADER]") || igual_n(v, n, "RFC822.HEADER")) {
					cab = w;
					cabn = wn;
				}
			} else if (k == TK_ABRE) {
				t->p--;
				tok_salta(t);
			}
		} else {
			/* lo demás (ENVELOPE, BODYSTRUCTURE...): se salta */
			if (tok_salta(t) < 0)
				break;
		}
	}
	if (!uid)
		return 0;
	if (x->cada_b && hay_ban)
		x->cada_b(x->u, uid, ban);
	if (x->cada && cab) {
		memset(&f, 0, sizeof f);
		ficha_de_cabeceras(&f, cab, cabn, x->de_enviados);
		f.uid = uid;
		f.banderas |= ban;
		f.tam = tam;
		if (!f.fecha)
			f.fecha = fecha;
		x->cada(x->u, &f);
	}
	if (x->uid == uid && cuerpo && !x->cuerpo) {
		x->cuerpo = so_pide(cuerpon + 1);
		if (!x->cuerpo)
			x->sin_memoria = 1;
		else {
			memcpy(x->cuerpo, cuerpo, cuerpon);
			x->cuerpo[cuerpon] = 0;
			x->n = cuerpon;
		}
	}
	return 0;
}

#define CABECERAS "BODY.PEEK[HEADER.FIELDS (FROM TO CC SUBJECT DATE MESSAGE-ID IN-REPLY-TO REFERENCES CONTENT-TYPE REPLY-TO SENDER)]"

int imap_fichas(Imap *m, const u32 *uids, u32 n, int de_enviados, void (*cada)(void *u, Ficha *f), void *u)
{
	Fetch x;
	Oyente o = { de_fetch, &x };
	u32 i = 0;
	char con[1500];

	memset(&x, 0, sizeof x);
	x.de_enviados = de_enviados;
	x.cada = cada;
	x.u = u;
	while (i < n) {
		int r;

		conjunto(uids, n, &i, con, sizeof con);
		if (!con[0])
			break;
		r = orden(m, &o, "UID FETCH %s (UID FLAGS RFC822.SIZE INTERNALDATE " CABECERAS ")", con);
		if (r)
			return r;
	}
	return 0;
}

int imap_banderas(Imap *m, u32 desde, u32 hasta, void (*cada)(void *u, u32 uid, u32 banderas), void *u)
{
	Fetch x;
	Oyente o = { de_fetch, &x };

	memset(&x, 0, sizeof x);
	x.cada_b = cada;
	x.u = u;
	if (hasta && hasta < desde)
		return 0;
	if (hasta)
		return orden(m, &o, "UID FETCH %u:%u (UID FLAGS)", (unsigned)desde, (unsigned)hasta);
	return orden(m, &o, "UID FETCH %u:* (UID FLAGS)", (unsigned)desde);
}

int imap_mensaje(Imap *m, u32 uid, u8 **d, u32 *n, u32 max, int marcar_leido)
{
	Fetch x;
	Oyente o = { de_fetch, &x };
	u32 viejo = m->max_literal;
	int r;

	memset(&x, 0, sizeof x);
	x.uid = uid;
	*d = NULL;
	*n = 0;
	if (max)
		m->max_literal = max;
	r = orden(m, &o, "UID FETCH %u (UID BODY.PEEK[])", (unsigned)uid);
	m->max_literal = viejo;
	if (r || !x.cuerpo) {
		so_suelta(x.cuerpo);
		if (!r)
			error_de(m, "El servidor no da ese mensaje (\xc2\xbfse ha borrado?).%s", NULL);
		return r ? r : 1;
	}
	if (marcar_leido && !m->solo_leer)
		orden(m, NULL, "UID STORE %u +FLAGS.SILENT (\\Seen)", (unsigned)uid);
	*d = x.cuerpo;
	*n = x.n;
	return 0;
}

static void texto_banderas(u32 b, char *d, u32 cabe)
{
	snprintf(d, cabe, "%s%s%s%s%s%s", b & B_LEIDO ? "\\Seen " : "", b & B_DESTACADO ? "\\Flagged " : "",
		 b & B_RESPONDIDO ? "\\Answered " : "", b & B_BORRADO ? "\\Deleted " : "", b & B_BORRADOR ? "\\Draft " : "",
		 b & B_REENVIADO ? "$Forwarded " : "");
	if (d[0])
		d[strlen(d) - 1] = 0;
}

int imap_marca(Imap *m, const u32 *uids, u32 n, u32 banderas, int poner)
{
	char con[1500], b[120];
	u32 i = 0;

	texto_banderas(banderas, b, sizeof b);
	while (i < n) {
		int r;

		conjunto(uids, n, &i, con, sizeof con);
		if (!con[0])
			break;
		r = orden(m, NULL, "UID STORE %s %cFLAGS.SILENT (%s)", con, poner ? '+' : '-', b);
		if (r)
			return r;
	}
	return 0;
}

int imap_borra(Imap *m, const u32 *uids, u32 n)
{
	char con[1500];
	u32 i = 0;
	int r;

	r = imap_marca(m, uids, n, B_BORRADO, 1);
	if (r)
		return r;
	if (!(m->caps & IC_UIDPLUS))
		return orden(m, NULL, "EXPUNGE");
	while (i < n) {
		conjunto(uids, n, &i, con, sizeof con);
		if (!con[0])
			break;
		r = orden(m, NULL, "UID EXPUNGE %s", con);
		if (r)
			return r;
	}
	return 0;
}

int imap_mueve(Imap *m, const u32 *uids, u32 n, const char *destino)
{
	char con[1400], q[300];
	u32 i = 0;
	int r;

	if (entrecomilla(destino, q, sizeof q))
		return 1;
	while (i < n) {
		u32 ini = i;

		conjunto(uids, n, &i, con, sizeof con);
		if (!con[0])
			break;
		if (m->caps & IC_MOVE)
			r = orden(m, NULL, "UID MOVE %s %s", con, q);
		else {
			r = orden(m, NULL, "UID COPY %s %s", con, q);
			if (!r)
				r = imap_borra(m, uids + ini, i - ini);
		}
		if (r)
			return r;
	}
	return 0;
}

int imap_anade(Imap *m, const char *carpeta, const u8 *d, u32 n, u32 banderas, u32 *uid)
{
	char q[300], a[500], b[120];
	int r;

	if (entrecomilla(carpeta, q, sizeof q))
		return 1;
	texto_banderas(banderas, b, sizeof b);
	snprintf(a, sizeof a, "APPEND %s (%s) ", q, b);
	m->copyuid = 0;
	r = orden_literal(m, NULL, a, d, n, "");
	if (uid)
		*uid = r ? 0 : m->copyuid;
	return r;
}

int imap_noop(Imap *m)
{
	return orden(m, NULL, "NOOP");
}

/* =============================================================================
 *  IDLE
 * ============================================================================= */
int imap_idle_empieza(Imap *m)
{
	char t[32], et[16];
	int n;

	if (!(m->caps & IC_IDLE) || !m->con.c)
		return -1;
	if (m->en_idle)
		return 0;
	snprintf(et, sizeof et, "c%u", (unsigned)++m->etiqueta);
	n = snprintf(t, sizeof t, "%s IDLE\r\n", et);
	if (con_envia(&m->con, t, (u32)n) < 0)
		return -1;
	for (;;) {
		if (lee_respuesta(m) < 0)
			return -1;
		if (m->r[0] == '+')
			break;
		if (m->r[0] == '*') {
			sin_etiqueta(m, NULL);
			continue;
		}
		snprintf(m->error, sizeof m->error, "El servidor no deja esperar (IDLE): %.200s", (char *)m->r);
		return 1;
	}
	m->en_idle = (int)m->etiqueta;
	return 0;
}

int imap_idle_mira(Imap *m, u32 espera_ms)
{
	int r, algo = 0;
	u32 antes = m->con.espera_ms;

	if (!m->en_idle)
		return 0;
	for (;;) {
		r = con_hay(&m->con, algo ? 0 : espera_ms);
		if (r < 0) {
			snprintf(m->error, sizeof m->error, "%s", m->con.texto);
			m->en_idle = 0;
			return -1;
		}
		if (r == 0)
			return algo;
		m->con.espera_ms = 10000;
		if (lee_respuesta(m) < 0) {
			m->con.espera_ms = antes;
			m->en_idle = 0;
			return -1;
		}
		m->con.espera_ms = antes;
		if (m->r[0] == '*') {
			u32 e = m->nuevo_exists, x = m->hubo_expunge, b = m->hubo_banderas;

			sin_etiqueta(m, NULL);
			if (m->nuevo_exists != e || m->hubo_expunge != (int)x || m->hubo_banderas != (int)b)
				algo = 1;
			if (empieza((char *)m->r, "* BYE")) {
				m->en_idle = 0;
				return -1;
			}
		}
	}
}

int imap_idle_acaba(Imap *m)
{
	char et[16];

	if (!m->en_idle)
		return 0;
	snprintf(et, sizeof et, "c%u", (unsigned)m->en_idle);
	m->en_idle = 0;
	if (con_envia(&m->con, "DONE\r\n", 6) < 0)
		return -1;
	for (;;) {
		if (lee_respuesta(m) < 0)
			return -1;
		if (m->r[0] == '*') {
			sin_etiqueta(m, NULL);
			continue;
		}
		if (!strncmp((char *)m->r, et, strlen(et)))
			return 0;
	}
}

/* =============================================================================
 *  Para las pruebas: lo que diga un servidor (malo), sin red
 * ============================================================================= */
static void cuenta_ficha(void *u, Ficha *f)
{
	(*(u32 *)u)++;
	(void)f;
}

static void cuenta_ban(void *u, u32 uid, u32 b)
{
	(*(u32 *)u) += (uid ^ b) & 1;
}

int imap_prueba_respuestas(const u8 *d, u32 n)
{
	/* cada respuesta, separada por renglones; los literales, como llegarían:
	 * se hace como si fuese la conexión (el búfer de Con), y se mira con
	 * todos los oyentes */
	Imap m;
	Carpeta v[16], st;
	Lista l = { v, 0, 16 };
	Uids us = { NULL, 0, 0, 0 };
	Fetch x;
	u32 cuantas = 0, cb = 0;
	Oyente ol = { de_list, &l }, os = { de_search, &us }, of = { de_fetch, &x }, ost = { de_status, &st };
	int vueltas = 0;

	memset(&m, 0, sizeof m);
	memset(&x, 0, sizeof x);
	memset(&st, 0, sizeof st);
	snprintf(st.crudo, sizeof st.crudo, "INBOX");
	x.cada = cuenta_ficha;
	x.cada_b = cuenta_ban;
	x.u = &cuantas;
	x.uid = 1;
	m.max_literal = 1u << 22;
	m.con.b = so_pide(n + 1);
	if (!m.con.b)
		return -1;
	memcpy(m.con.b, d, n);
	m.con.fin = n;
	m.con.cabe = n + 1;
	m.con.c = 0;
	while (vueltas++ < 100000) {
		if (lee_respuesta(&m) < 0)
			break;
		if (m.rn >= 2 && m.r[0] == '*') {
			sin_etiqueta(&m, &ol);
			sin_etiqueta(&m, &os);
			sin_etiqueta(&m, &of);
			sin_etiqueta(&m, &ost);
		} else
			lee_codigo(&m, m.r, m.rn);
		(void)cb;
	}
	{
		char t[300];

		utf8_a_utf7m("Asociaci\xc3\xb3n/Cuotas & m\xc3\xa1s", t, sizeof t);
		utf7m_a_utf8((const char *)m.r ? (const char *)m.r : "", t, sizeof t);
	}
	so_suelta(x.cuerpo);
	so_suelta(us.v);
	so_suelta(m.r);
	so_suelta(m.con.b);
	return (int)(l.n + us.n + cuantas);
}
