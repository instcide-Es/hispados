/* smtp.c - Correo: enviar (RFC 5321, el envío de RFC 6409 y RFC 8314): por el
 * 465 con TLS desde el principio, o por el 587 con STARTTLS (lo plano de
 * antes lo hace TLS.BIN: so.h, SO_STARTTLS_SMTP); AUTH PLAIN o LOGIN, y
 * nunca la contraseña sin cifrar (salvo a un servidor de casa con «permitir
 * sin cifrar»). El mensaje, con el relleno de puntos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "correo.h"

#define SM_PLAIN 1
#define SM_LOGIN 2
#define SM_SIZE 4
#define SM_8BIT 8

/* una respuesta (de varios renglones: «250-...» y «250 ...»): su código, y
 * lo que dice (el último renglón) en «texto»; con las extensiones del EHLO */
static int respuesta(Con *k, char *texto, int cabe, u32 *ext)
{
	char *r;
	int n, codigo = -1;

	for (;;) {
		n = con_renglon(k, &r);
		if (n < 0) {
			snprintf(texto, (size_t)cabe, "%s", k->texto);
			return -1;
		}
		if (n < 3 || r[0] < '1' || r[0] > '5') {
			snprintf(texto, (size_t)cabe, "El servidor no contesta como SMTP: %.80s", r);
			return -1;
		}
		codigo = (r[0] - '0') * 100 + (r[1] - '0') * 10 + (r[2] - '0');
		if (ext && n > 4) {
			const char *e = r + 4;

			if (!strncmp(e, "AUTH", 4) && (e[4] == ' ' || e[4] == '=')) {
				if (strstr(e, "PLAIN"))
					*ext |= SM_PLAIN;
				if (strstr(e, "LOGIN"))
					*ext |= SM_LOGIN;
			} else if (!strncmp(e, "SIZE", 4))
				*ext |= SM_SIZE;
			else if (!strncmp(e, "8BITMIME", 8))
				*ext |= SM_8BIT;
		}
		if (n == 3 || r[3] != '-') {
			snprintf(texto, (size_t)cabe, "%s", n > 4 ? r + 4 : r);
			return codigo;
		}
	}
}

static int envia_base64(Con *k, const u8 *d, u32 n)
{
	u32 bn;
	char *b = b64_codifica(d, n, &bn, 0);
	int r;

	if (!b)
		return -1;
	r = con_envia(k, b, bn);
	if (!r)
		r = con_envia(k, "\r\n", 2);
	memset(b, 0, bn);
	so_suelta(b);
	return r;
}

int smtp_envia(const Cuenta *c, const char *clave, const char *de, const char *destinos, const u8 *d, u32 n,
	       char *error, int cabe)
{
	Con k;
	char t[400];
	u32 ext = 0, i;
	int r, modo = c->modo_salida, hay = 0;
	const char *p;

	error[0] = 0;
	if (modo == SO_PLANO && (!c->sin_cifrar || !servidor_local(c->salida))) {
		snprintf(error, (size_t)cabe, "No se manda la contrase\xc3\xb1" "a sin cifrar a %s: use TLS (465) o STARTTLS (587).", c->salida);
		return -1;
	}
	if (con_abre(&k, c->salida, c->puerto_salida, modo, 20000)) {
		snprintf(error, (size_t)cabe, "%s", k.texto);
		so_apunta("smtp: %s:%u: %s", c->salida, (unsigned)c->puerto_salida, k.texto);
		return -1;
	}
	k.espera_ms = 60000;
	so_apunta("smtp: conectado con %s:%u (%s)", c->salida, (unsigned)c->puerto_salida, k.cifrado[0] ? k.cifrado : "SIN CIFRAR");
	if (modo != SO_STARTTLS_SMTP) {
		r = respuesta(&k, t, sizeof t, NULL);
		if (r != 220) {
			snprintf(error, (size_t)cabe, "El servidor de salida no deja enviar: %s", t);
			goto mal;
		}
	}
	if (con_envia_texto(&k, "EHLO hispa-pentium\r\n") < 0)
		goto mal_con;
	r = respuesta(&k, t, sizeof t, &ext);
	if (r != 250) {
		snprintf(error, (size_t)cabe, "El servidor de salida no acepta EHLO: %s", t);
		goto mal;
	}
	/* entrar */
	if (clave && clave[0]) {
		u32 un = (u32)strlen(c->usuario), kn = (u32)strlen(clave);
		u8 b[400];

		if (un + kn + 2 > sizeof b) {
			snprintf(error, (size_t)cabe, "El usuario o la contrase\xc3\xb1" "a son demasiado largos.");
			goto mal;
		}
		if (ext & SM_PLAIN) {
			b[0] = 0;
			memcpy(b + 1, c->usuario, un);
			b[1 + un] = 0;
			memcpy(b + 2 + un, clave, kn);
			if (con_envia_texto(&k, "AUTH PLAIN ") < 0 || envia_base64(&k, b, un + kn + 2) < 0) {
				memset(b, 0, sizeof b);
				goto mal_con;
			}
			memset(b, 0, sizeof b);
			r = respuesta(&k, t, sizeof t, NULL);
		} else if (ext & SM_LOGIN) {
			if (con_envia_texto(&k, "AUTH LOGIN\r\n") < 0)
				goto mal_con;
			r = respuesta(&k, t, sizeof t, NULL);
			if (r == 334) {
				if (envia_base64(&k, (const u8 *)c->usuario, un) < 0)
					goto mal_con;
				r = respuesta(&k, t, sizeof t, NULL);
			}
			if (r == 334) {
				if (envia_base64(&k, (const u8 *)clave, kn) < 0)
					goto mal_con;
				r = respuesta(&k, t, sizeof t, NULL);
			}
		} else {
			snprintf(error, (size_t)cabe, "El servidor de salida no dice c\xc3\xb3mo entrar (AUTH PLAIN o LOGIN).");
			goto mal;
		}
		if (r != 235) {
			snprintf(error, (size_t)cabe, "No se ha podido entrar en el servidor de salida (\xc2\xbf" "est\xc3\xa1 bien la contrase\xc3\xb1" "a?): %s", t);
			goto mal;
		}
	}
	if (ext & SM_SIZE)
		r = con_envia_texto(&k, "MAIL FROM:<%s> SIZE=%u\r\n", de, (unsigned)n);
	else
		r = con_envia_texto(&k, "MAIL FROM:<%s>\r\n", de);
	if (r < 0)
		goto mal_con;
	r = respuesta(&k, t, sizeof t, NULL);
	if (r != 250) {
		snprintf(error, (size_t)cabe, "El servidor no acepta el remitente %s: %s", de, t);
		goto mal;
	}
	/* los destinatarios */
	for (p = destinos; *p;) {
		char a[260];

		while (*p == ',' || *p == ' ')
			p++;
		for (i = 0; *p && *p != ',' && *p != ' ' && i + 1 < sizeof a; p++)
			a[i++] = *p;
		a[i] = 0;
		while (*p && *p != ',')
			p++;
		if (!a[0])
			continue;
		if (con_envia_texto(&k, "RCPT TO:<%s>\r\n", a) < 0)
			goto mal_con;
		r = respuesta(&k, t, sizeof t, NULL);
		if (r != 250 && r != 251) {
			snprintf(error, (size_t)cabe, "El servidor no acepta el destinatario %s: %s", a, t);
			goto mal;
		}
		hay++;
	}
	if (!hay) {
		snprintf(error, (size_t)cabe, "No hay ning\xc3\xban destinatario.");
		goto mal;
	}
	if (con_envia_texto(&k, "DATA\r\n") < 0)
		goto mal_con;
	r = respuesta(&k, t, sizeof t, NULL);
	if (r != 354) {
		snprintf(error, (size_t)cabe, "El servidor no deja mandar el mensaje: %s", t);
		goto mal;
	}
	/* el mensaje, con los puntos del principio de renglón doblados, a trozos */
	{
		u8 *b = so_pide(16384 + 4);
		u32 j = 0, bi = 0;
		int ini = 1;

		if (!b)
			goto mal;
		for (j = 0; j < n; j++) {
			if (ini && d[j] == '.')
				b[bi++] = '.';
			b[bi++] = d[j];
			ini = d[j] == '\n';
			if (bi >= 16384) {
				if (con_envia(&k, b, bi) < 0) {
					so_suelta(b);
					goto mal_con;
				}
				bi = 0;
			}
		}
		if (n && d[n - 1] != '\n') {
			b[bi++] = '\r';
			b[bi++] = '\n';
		}
		r = con_envia(&k, b, bi);
		so_suelta(b);
		if (r < 0 || con_envia_texto(&k, ".\r\n") < 0)
			goto mal_con;
	}
	k.espera_ms = 120000;
	r = respuesta(&k, t, sizeof t, NULL);
	if (r != 250) {
		snprintf(error, (size_t)cabe, "El servidor no ha aceptado el mensaje: %s", t);
		goto mal;
	}
	so_apunta("smtp: enviado (%u octetos, %d destinatarios): %s", (unsigned)n, hay, t);
	k.espera_ms = 5000;
	con_envia_texto(&k, "QUIT\r\n");
	respuesta(&k, t, sizeof t, NULL);
	con_cierra(&k);
	return 0;
mal_con:
	if (!error[0])
		snprintf(error, (size_t)cabe, "%s", k.texto);
mal:
	so_apunta("smtp: %s", error);
	con_cierra(&k);
	return -1;
}
