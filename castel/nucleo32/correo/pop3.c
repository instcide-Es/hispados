/* pop3.c - Correo: el POP3 (RFC 1939, con UIDL y TOP de RFC 2449; STLS de
 * RFC 2595, que hace TLS.BIN), por el 995 con TLS o por el 110 con STLS. Lo
 * bajado va a la «Bandeja de entrada» de aquí (almacen.c); se deja en el
 * servidor si la cuenta lo dice.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "correo.h"

static int ok(Pop3 *p, char **r)
{
	int n = con_renglon(&p->con, r);

	if (n < 0) {
		snprintf(p->error, sizeof p->error, "%s", p->con.texto);
		return -1;
	}
	if (strncmp(*r, "+OK", 3)) {
		snprintf(p->error, sizeof p->error, "El servidor dice: %.250s", *r);
		return 1;
	}
	return 0;
}

int pop3_conecta(Pop3 *p, const Cuenta *c, const char *clave)
{
	char *r, t[300];
	int x;
	u32 i;

	memset(p, 0, sizeof *p);
	if (c->modo_entrada == SO_PLANO && (!c->sin_cifrar || !servidor_local(c->entrada))) {
		snprintf(p->error, sizeof p->error, "No se manda la contrase\xc3\xb1" "a sin cifrar a %s: use TLS (995) o STLS (110).", c->entrada);
		return -1;
	}
	if (con_abre(&p->con, c->entrada, c->puerto_entrada, c->modo_entrada, 20000)) {
		snprintf(p->error, sizeof p->error, "%s", p->con.texto);
		return -1;
	}
	p->con.espera_ms = 60000;
	so_apunta("pop3: conectado con %s:%u (%s)", c->entrada, (unsigned)c->puerto_entrada,
		  p->con.cifrado[0] ? p->con.cifrado : "SIN CIFRAR");
	if (c->modo_entrada != SO_STARTTLS_POP3 && ok(p, &r))
		goto mal;
	for (i = 0; c->usuario[i]; i++)
		if (c->usuario[i] == '\r' || c->usuario[i] == '\n')
			goto mal;
	for (i = 0; clave[i]; i++)
		if (clave[i] == '\r' || clave[i] == '\n')
			goto mal;
	if (con_envia_texto(&p->con, "USER %s\r\n", c->usuario) < 0 || ok(p, &r))
		goto mal;
	snprintf(t, sizeof t, "PASS %s\r\n", clave);
	x = con_envia(&p->con, t, (u32)strlen(t));
	memset(t, 0, sizeof t);
	if (x < 0)
		goto mal;
	x = ok(p, &r);
	if (x) {
		if (x > 0) {
			char e[300];

			snprintf(e, sizeof e, "%s", p->error);
			snprintf(p->error, sizeof p->error, "No se ha podido entrar (\xc2\xbf" "est\xc3\xa1 bien la contrase\xc3\xb1" "a?). %s", e);
		}
		goto mal;
	}
	if (con_envia_texto(&p->con, "STAT\r\n") < 0 || ok(p, &r))
		goto mal;
	{
		u32 v = 0;
		const char *q = r + 3;

		while (*q == ' ')
			q++;
		while (*q >= '0' && *q <= '9' && v < 100000000)
			v = v * 10 + (u32)(*q++ - '0');
		p->n = v;
	}
	return 0;
mal:
	if (!p->error[0])
		snprintf(p->error, sizeof p->error, "%s", p->con.texto[0] ? p->con.texto : "El servidor POP3 no contesta bien.");
	so_apunta("pop3: %s", p->error);
	con_cierra(&p->con);
	return -1;
}

int pop3_uidl(Pop3 *p, void (*cada)(void *u, u32 num, const char *uidl, u32 tam), void *u)
{
	char *r;
	u32 *tam = NULL, i;
	int n;

	/* los tamaños (LIST) y luego los UIDL */
	if (p->n) {
		tam = so_pide((p->n + 1) * sizeof(u32));
		if (!tam)
			return -1;
		memset(tam, 0, (p->n + 1) * sizeof(u32));
	}
	if (con_envia_texto(&p->con, "LIST\r\n") < 0 || ok(p, &r))
		goto mal;
	for (;;) {
		u32 a = 0, b = 0;
		const char *q;

		n = con_renglon(&p->con, &r);
		if (n < 0)
			goto mal;
		if (!strcmp(r, "."))
			break;
		for (q = r; *q >= '0' && *q <= '9' && a < 100000000; q++)
			a = a * 10 + (u32)(*q - '0');
		while (*q == ' ')
			q++;
		for (; *q >= '0' && *q <= '9' && b < 400000000; q++)
			b = b * 10 + (u32)(*q - '0');
		if (a >= 1 && a <= p->n)
			tam[a] = b;
	}
	if (con_envia_texto(&p->con, "UIDL\r\n") < 0)
		goto mal;
	if (ok(p, &r)) {
		/* sin UIDL: el número (no se puede saber qué es nuevo: se baja todo una vez) */
		for (i = 1; i <= p->n; i++) {
			char t[16];

			snprintf(t, sizeof t, "N%u", (unsigned)i);
			cada(u, i, t, tam ? tam[i] : 0);
		}
		so_suelta(tam);
		return 0;
	}
	for (;;) {
		u32 a = 0;
		const char *q;

		n = con_renglon(&p->con, &r);
		if (n < 0)
			goto mal;
		if (!strcmp(r, "."))
			break;
		for (q = r; *q >= '0' && *q <= '9' && a < 100000000; q++)
			a = a * 10 + (u32)(*q - '0');
		while (*q == ' ')
			q++;
		if (a >= 1 && a <= p->n && *q && strlen(q) <= 70)
			cada(u, a, q, tam ? tam[a] : 0);
	}
	so_suelta(tam);
	return 0;
mal:
	so_suelta(tam);
	if (!p->error[0])
		snprintf(p->error, sizeof p->error, "%s", p->con.texto);
	return -1;
}

int pop3_mensaje(Pop3 *p, u32 num, u8 **d, u32 *n, u32 max)
{
	char *r;
	u8 *b = NULL;
	u32 k = 0, cabe = 0;
	int l;

	*d = NULL;
	*n = 0;
	if (con_envia_texto(&p->con, "RETR %u\r\n", (unsigned)num) < 0 || ok(p, &r))
		return -1;
	for (;;) {
		l = con_renglon(&p->con, &r);
		if (l < 0) {
			so_suelta(b);
			snprintf(p->error, sizeof p->error, "%s", p->con.texto);
			return -1;
		}
		if (l == 1 && r[0] == '.')
			break;
		if (r[0] == '.') {
			r++;
			l--;
		}
		if (k + (u32)l + 3 > cabe) {
			u32 c = cabe ? cabe * 2 : 65536;
			u8 *q;

			while (c < k + (u32)l + 3)
				c *= 2;
			if (c > max + 65536) {
				so_suelta(b);
				snprintf(p->error, sizeof p->error, "El mensaje es demasiado grande.");
				/* (lo que queda se lee y se tira) */
				for (;;) {
					l = con_renglon(&p->con, &r);
					if (l < 0 || (l == 1 && r[0] == '.'))
						break;
				}
				return 1;
			}
			q = so_repide(b, c);
			if (!q) {
				so_suelta(b);
				return -1;
			}
			b = q;
			cabe = c;
		}
		memcpy(b + k, r, (u32)l);
		k += (u32)l;
		b[k++] = '\r';
		b[k++] = '\n';
	}
	if (!b) {
		b = so_pide(1);
		if (!b)
			return -1;
	}
	b[k] = 0;
	*d = b;
	*n = k;
	return 0;
}

int pop3_borra(Pop3 *p, u32 num)
{
	char *r;

	if (con_envia_texto(&p->con, "DELE %u\r\n", (unsigned)num) < 0)
		return -1;
	return ok(p, &r);
}

void pop3_cierra(Pop3 *p)
{
	char *r;

	if (p->con.c) {
		p->con.espera_ms = 5000;
		if (!con_envia_texto(&p->con, "QUIT\r\n"))
			ok(p, &r);
	}
	con_cierra(&p->con);
}
