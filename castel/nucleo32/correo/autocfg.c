/* autocfg.c - Correo: los servidores de una dirección, sin que el usuario
 * tenga que saberlos. Por orden:
 *   1. los proveedores de siempre (Gmail, Outlook, Yahoo, iCloud, GMX...),
 *      que van aquí dentro, con lo que hay que saber de cada uno (la
 *      contraseña de aplicación);
 *   2. el autoconfig del propio dominio (autoconfig.dominio y su
 *      .well-known) y la base de datos de Mozilla (ISPDB, la de
 *      Thunderbird: ajustes.ispdb), por HTTPS;
 *   3. los registros SRV del DNS (RFC 6186: _imaps._tcp, _submissions._tcp...);
 *   4. probar imap., mail. y el dominio mismo (993) y smtp. y mail. (465, 587).
 * OAuth2 (lo que piden ya Google y Microsoft para entrar sin contraseña de
 * aplicación) queda para más adelante.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "correo.h"

static const char AVISO_GMAIL[] =
	"Gmail no deja entrar con la contrase\xc3\xb1" "a de siempre en un programa de correo: hace falta una "
	"\xc2\xab" "contrase\xc3\xb1" "a de aplicaci\xc3\xb3n\xc2\xbb. En otro equipo, entre en myaccount.google.com, "
	"Seguridad, active la verificaci\xc3\xb3n en dos pasos y, en \xc2\xab" "Contrase\xc3\xb1" "as de aplicaciones\xc2\xbb, "
	"haga una para \xc2\xab" "Correo\xc2\xbb. Son 16 letras: esc\xc3\xad" "balas aqu\xc3\xad (sin los espacios).";
static const char AVISO_OUTLOOK[] =
	"Outlook y Hotmail (Microsoft) piden cada vez m\xc3\xa1s entrar con OAuth2, que este programa a\xc3\xban no "
	"tiene. Si su cuenta tiene la verificaci\xc3\xb3n en dos pasos, pruebe con una \xc2\xab" "contrase\xc3\xb1" "a de "
	"aplicaci\xc3\xb3n\xc2\xbb (account.microsoft.com, Seguridad, Opciones de seguridad avanzadas).";
static const char AVISO_YAHOO[] =
	"Yahoo pide una \xc2\xab" "contrase\xc3\xb1" "a de aplicaci\xc3\xb3n\xc2\xbb: en login.yahoo.com, Seguridad de la cuenta, "
	"\xc2\xab" "Generar contrase\xc3\xb1" "a de aplicaci\xc3\xb3n\xc2\xbb.";
static const char AVISO_ICLOUD[] =
	"iCloud pide una \xc2\xab" "contrase\xc3\xb1" "a de aplicaci\xc3\xb3n\xc2\xbb: en appleid.apple.com, Inicio de sesi\xc3\xb3n "
	"y seguridad, Contrase\xc3\xb1" "as de apps.";

static const struct {
	const char *dominios;	/* separados por «|» */
	const char *imap;
	u32 puerto_imap;
	const char *smtp;
	u32 puerto_smtp;
	int modo_smtp;
	const char *aviso;
	int usuario_sin_dominio;
} conocidos[] = {
	{ "gmail.com|googlemail.com", "imap.gmail.com", 993, "smtp.gmail.com", 465, SO_TLS, AVISO_GMAIL, 0 },
	{ "outlook.com|outlook.es|hotmail.com|hotmail.es|live.com|live.es|msn.com", "outlook.office365.com", 993,
	  "smtp-mail.outlook.com", 587, SO_STARTTLS_SMTP, AVISO_OUTLOOK, 0 },
	{ "yahoo.com|yahoo.es|ymail.com", "imap.mail.yahoo.com", 993, "smtp.mail.yahoo.com", 465, SO_TLS, AVISO_YAHOO, 0 },
	{ "icloud.com|me.com|mac.com", "imap.mail.me.com", 993, "smtp.mail.me.com", 587, SO_STARTTLS_SMTP, AVISO_ICLOUD, 1 },
	{ "gmx.es|gmx.com|gmx.net", "imap.gmx.com", 993, "mail.gmx.com", 465, SO_TLS, NULL, 0 },
	{ "aol.com", "imap.aol.com", 993, "smtp.aol.com", 465, SO_TLS, NULL, 0 },
	{ "zoho.com|zohomail.eu", "imap.zoho.eu", 993, "smtp.zoho.eu", 465, SO_TLS, NULL, 0 },
	{ "telefonica.net|movistar.es", "imap.telefonica.net", 993, "smtp.telefonica.net", 465, SO_TLS, NULL, 0 },
};

static int en_lista(const char *lista, const char *dom)
{
	u32 n = (u32)strlen(dom);

	while (*lista) {
		const char *b = strchr(lista, '|');
		u32 k = b ? (u32)(b - lista) : (u32)strlen(lista);

		if (k == n && !memcmp(lista, dom, n))
			return 1;
		lista += k;
		if (*lista == '|')
			lista++;
	}
	return 0;
}

static void base(Cuenta *c, const char *correo)
{
	memset(c, 0, sizeof *c);
	snprintf(c->correo, sizeof c->correo, "%s", correo);
	snprintf(c->usuario, sizeof c->usuario, "%s", correo);
	snprintf(c->nombre, sizeof c->nombre, "%s", correo);
	c->tipo = CT_IMAP;
	c->modo_entrada = SO_TLS;
	c->puerto_entrada = 993;
	c->modo_salida = SO_TLS;
	c->puerto_salida = 465;
	c->dejar_en_servidor = 1;
	c->avisar = 1;
}

/* ---- el XML de autoconfig (clientConfig 1.1): sin un analizador entero,
 * mirando las etiquetas que hacen falta, dentro de cada servidor ---- */
static const char *busca(const char *a, const char *fin, const char *t)
{
	u32 n = (u32)strlen(t);

	for (; a + n <= fin; a++)
		if (*a == *t && !memcmp(a, t, n))
			return a;
	return NULL;
}

static void valor(const char *a, const char *fin, const char *etiqueta, char *d, u32 cabe)
{
	char ab[40], ci[40];
	const char *p, *q;
	u32 k = 0;

	d[0] = 0;
	snprintf(ab, sizeof ab, "<%s>", etiqueta);
	snprintf(ci, sizeof ci, "</%s>", etiqueta);
	p = busca(a, fin, ab);
	if (!p)
		return;
	p += strlen(ab);
	q = busca(p, fin, ci);
	if (!q)
		return;
	while (p < q && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
		p++;
	while (q > p && (q[-1] == ' ' || q[-1] == '\n' || q[-1] == '\r' || q[-1] == '\t'))
		q--;
	for (; p < q && k + 1 < cabe; p++) {
		if (*p == '&') {
			if (!strncmp(p, "&amp;", 5)) {
				d[k++] = '&';
				p += 4;
				continue;
			}
		}
		d[k++] = *p;
	}
	d[k] = 0;
}

/* %EMAILADDRESS%, %EMAILLOCALPART%, %EMAILDOMAIN% */
static void sustituye(const char *t, const char *correo, char *d, u32 cabe)
{
	const char *arroba = strchr(correo, '@');
	u32 k = 0;

	while (*t && k + 1 < cabe) {
		if (!strncmp(t, "%EMAILADDRESS%", 14)) {
			k += (u32)snprintf(d + k, cabe - k, "%s", correo);
			t += 14;
		} else if (!strncmp(t, "%EMAILLOCALPART%", 16)) {
			k += (u32)snprintf(d + k, cabe - k, "%.*s", arroba ? (int)(arroba - correo) : (int)strlen(correo), correo);
			t += 16;
		} else if (!strncmp(t, "%EMAILDOMAIN%", 13)) {
			k += (u32)snprintf(d + k, cabe - k, "%s", arroba ? arroba + 1 : "");
			t += 13;
		} else
			d[k++] = *t++;
		if (k >= cabe)
			k = cabe - 1;
	}
	d[k] = 0;
}

static u32 numero(const char *t)
{
	u32 v = 0;

	while (*t >= '0' && *t <= '9' && v < 100000)
		v = v * 10 + (u32)(*t++ - '0');
	return v;
}

int autocfg_xml(const char *xml, u32 n, const char *correo, Cuenta *c)
{
	const char *fin = xml + n, *p = xml;
	int hay_imap = 0, hay_pop = 0, hay_smtp = 0;
	Cuenta pop;

	base(c, correo);
	pop = *c;
	if (!busca(xml, fin, "<clientConfig"))
		return 0;
	while ((p = busca(p, fin, "<incomingServer")) != NULL) {
		const char *e = busca(p, fin, "</incomingServer>"), *cab = busca(p, fin, ">");
		char tipo[16] = "", h[128], pu[16], so[32], us[128];
		int imap;

		if (!e || !cab)
			break;
		{
			const char *t = busca(p, cab, "type=\""), *q = t ? memchr(t + 6, '"', (size_t)(cab - t - 6)) : NULL;

			if (t && q && q - t - 6 < (int)sizeof tipo)
				snprintf(tipo, sizeof tipo, "%.*s", (int)(q - t - 6), t + 6);
		}
		imap = !strcmp(tipo, "imap");
		valor(cab, e, "hostname", h, sizeof h);
		valor(cab, e, "port", pu, sizeof pu);
		valor(cab, e, "socketType", so, sizeof so);
		valor(cab, e, "username", us, sizeof us);
		if (h[0] && numero(pu) && (imap || !strcmp(tipo, "pop3"))) {
			Cuenta *x = imap ? c : &pop;
			int *hay = imap ? &hay_imap : &hay_pop;
			int modo = !strcmp(so, "SSL") ? SO_TLS : !strcmp(so, "STARTTLS") ? (imap ? SO_STARTTLS_IMAP : SO_STARTTLS_POP3) : SO_PLANO;

			/* el primero con cifrado; uno sin cifrar, solo si no hay otro */
			if (!*hay || (x->modo_entrada == SO_PLANO && modo != SO_PLANO)) {
				sustituye(h, correo, x->entrada, sizeof x->entrada);
				x->puerto_entrada = numero(pu);
				x->modo_entrada = modo;
				if (us[0])
					sustituye(us, correo, x->usuario, sizeof x->usuario);
				x->tipo = imap ? CT_IMAP : CT_POP3;
				*hay = 1;
			}
		}
		p = e;
	}
	p = xml;
	while ((p = busca(p, fin, "<outgoingServer")) != NULL) {
		const char *e = busca(p, fin, "</outgoingServer>"), *cab = busca(p, fin, ">");
		char h[128], pu[16], so[32];
		int modo;

		if (!e || !cab)
			break;
		valor(cab, e, "hostname", h, sizeof h);
		valor(cab, e, "port", pu, sizeof pu);
		valor(cab, e, "socketType", so, sizeof so);
		modo = !strcmp(so, "SSL") ? SO_TLS : !strcmp(so, "STARTTLS") ? SO_STARTTLS_SMTP : SO_PLANO;
		if (h[0] && numero(pu) && (!hay_smtp || (c->modo_salida == SO_PLANO && modo != SO_PLANO))) {
			sustituye(h, correo, c->salida, sizeof c->salida);
			c->puerto_salida = numero(pu);
			c->modo_salida = modo;
			hay_smtp = 1;
		}
		p = e;
	}
	if (!hay_imap && hay_pop) {
		char s[128];
		u32 ps = c->puerto_salida;
		int ms = c->modo_salida;

		snprintf(s, sizeof s, "%s", c->salida);
		*c = pop;
		snprintf(c->salida, sizeof c->salida, "%s", s);
		c->puerto_salida = ps;
		c->modo_salida = ms;
	}
	return (hay_imap || hay_pop) && hay_smtp;
}

/* ---- probar un servidor (¿contesta con TLS?) ---- */
static int contesta(const char *s, u32 puerto, int modo)
{
	char e[200];
	int c = so_conecta(s, puerto, modo, 6000, e, sizeof e);

	if (c > 0) {
		so_cierra_con(c);
		return 1;
	}
	return 0;
}

int autocfg_busca(const char *correo, Autocfg *a, void (*paso)(void *u, const char *que), void *u)
{
	const char *arroba = strchr(correo, '@');
	char dom[128], url[400], t[300];
	u8 *d;
	u32 n, i;
	int r;

	memset(a, 0, sizeof *a);
	base(&a->c, correo);
	if (!arroba || !arroba[1] || strlen(arroba + 1) >= sizeof dom)
		return 0;
	for (i = 0; arroba[1 + i]; i++)
		dom[i] = (char)(arroba[1 + i] >= 'A' && arroba[1 + i] <= 'Z' ? arroba[1 + i] + 32 : arroba[1 + i]);
	dom[i] = 0;
	/* 1. los de siempre */
	for (i = 0; i < sizeof conocidos / sizeof conocidos[0]; i++)
		if (en_lista(conocidos[i].dominios, dom)) {
			snprintf(a->c.entrada, sizeof a->c.entrada, "%s", conocidos[i].imap);
			a->c.puerto_entrada = conocidos[i].puerto_imap;
			snprintf(a->c.salida, sizeof a->c.salida, "%s", conocidos[i].smtp);
			a->c.puerto_salida = conocidos[i].puerto_smtp;
			a->c.modo_salida = conocidos[i].modo_smtp;
			if (conocidos[i].usuario_sin_dominio)
				snprintf(a->c.usuario, sizeof a->c.usuario, "%.*s", (int)(arroba - correo), correo);
			if (conocidos[i].aviso)
				snprintf(a->aviso, sizeof a->aviso, "%s", conocidos[i].aviso);
			a->encontrada = 1;
			snprintf(a->de_donde, sizeof a->de_donde, "los proveedores conocidos");
			return 1;
		}
	/* 2. autoconfig del dominio y la ISPDB */
	{
		const char *urls[3];
		char u1[300], u2[300], u3[300];

		snprintf(u1, sizeof u1, "https://autoconfig.%s/mail/config-v1.1.xml?emailaddress=%s", dom, correo);
		snprintf(u2, sizeof u2, "https://%s/.well-known/autoconfig/mail/config-v1.1.xml", dom);
		snprintf(u3, sizeof u3, "%s%s", ajustes.ispdb[0] ? ajustes.ispdb : "https://autoconfig.thunderbird.net/v1.1/", dom);
		urls[0] = u1;
		urls[1] = u2;
		urls[2] = u3;
		for (i = 0; i < 3; i++) {
			snprintf(t, sizeof t, "Preguntando a %s...", i == 2 ? "la base de datos de Mozilla" : urls[i] + 8);
			if (paso)
				paso(u, t);
			r = http_get(urls[i], &d, &n, 256 * 1024, 1, url, sizeof url);
			if (r == 200 && d && autocfg_xml((const char *)d, n, correo, &a->c)) {
				so_suelta(d);
				a->encontrada = 2;
				snprintf(a->de_donde, sizeof a->de_donde, "%s", i == 2 ? "la base de datos de Mozilla (ISPDB)" : "la configuraci\xc3\xb3n del dominio (autoconfig)");
				so_apunta("autoconfig: %s: %s:%u y %s:%u, de %s", correo, a->c.entrada, (unsigned)a->c.puerto_entrada,
					  a->c.salida, (unsigned)a->c.puerto_salida, a->de_donde);
				return 1;
			}
			so_suelta(d);
		}
	}
	/* 3. SRV (RFC 6186 y RFC 8314) */
	{
		char s[128];
		u32 p;

		if (paso)
			paso(u, "Preguntando al DNS (SRV)...");
		snprintf(t, sizeof t, "_imaps._tcp.%s", dom);
		if (dns_srv(t, s, sizeof s, &p)) {
			snprintf(a->c.entrada, sizeof a->c.entrada, "%s", s);
			a->c.puerto_entrada = p;
			a->c.modo_entrada = SO_TLS;
			snprintf(t, sizeof t, "_submissions._tcp.%s", dom);
			if (dns_srv(t, s, sizeof s, &p)) {
				snprintf(a->c.salida, sizeof a->c.salida, "%s", s);
				a->c.puerto_salida = p;
				a->c.modo_salida = SO_TLS;
			} else {
				snprintf(t, sizeof t, "_submission._tcp.%s", dom);
				if (dns_srv(t, s, sizeof s, &p)) {
					snprintf(a->c.salida, sizeof a->c.salida, "%s", s);
					a->c.puerto_salida = p;
					a->c.modo_salida = SO_STARTTLS_SMTP;
				} else {
					snprintf(a->c.salida, sizeof a->c.salida, "%s", a->c.entrada);
					a->c.puerto_salida = 465;
				}
			}
			a->encontrada = 3;
			snprintf(a->de_donde, sizeof a->de_donde, "el DNS (registros SRV)");
			return 1;
		}
	}
	/* 4. probando */
	{
		static const char *const pre_e[] = { "imap.", "mail.", "" }, *const pre_s[] = { "smtp.", "mail.", "" };
		int ok_e = 0, ok_s = 0;

		for (i = 0; i < 3 && !ok_e; i++) {
			snprintf(t, sizeof t, "Probando %s%s...", pre_e[i], dom);
			if (paso)
				paso(u, t);
			snprintf(url, sizeof url, "%s%s", pre_e[i], dom);
			if (contesta(url, 993, SO_TLS)) {
				snprintf(a->c.entrada, sizeof a->c.entrada, "%s", url);
				a->c.puerto_entrada = 993;
				a->c.modo_entrada = SO_TLS;
				ok_e = 1;
			} else if (contesta(url, 143, SO_STARTTLS_IMAP)) {
				snprintf(a->c.entrada, sizeof a->c.entrada, "%s", url);
				a->c.puerto_entrada = 143;
				a->c.modo_entrada = SO_STARTTLS_IMAP;
				ok_e = 1;
			}
		}
		for (i = 0; i < 3 && ok_e && !ok_s; i++) {
			snprintf(t, sizeof t, "Probando %s%s...", pre_s[i], dom);
			if (paso)
				paso(u, t);
			snprintf(url, sizeof url, "%s%s", pre_s[i], dom);
			if (contesta(url, 465, SO_TLS)) {
				snprintf(a->c.salida, sizeof a->c.salida, "%s", url);
				a->c.puerto_salida = 465;
				a->c.modo_salida = SO_TLS;
				ok_s = 1;
			} else if (contesta(url, 587, SO_STARTTLS_SMTP)) {
				snprintf(a->c.salida, sizeof a->c.salida, "%s", url);
				a->c.puerto_salida = 587;
				a->c.modo_salida = SO_STARTTLS_SMTP;
				ok_s = 1;
			}
		}
		if (ok_e && ok_s) {
			a->encontrada = 4;
			snprintf(a->de_donde, sizeof a->de_donde, "probando los nombres de siempre");
			return 1;
		}
	}
	snprintf(a->c.entrada, sizeof a->c.entrada, "imap.%s", dom);
	snprintf(a->c.salida, sizeof a->c.salida, "smtp.%s", dom);
	return 0;
}
