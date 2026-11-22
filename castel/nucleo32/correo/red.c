/* red.c - Correo: la conexión con su búfer, el HTTP pequeño y los SRV del
 * DNS (red.h). Lo que llega es hostil: renglones con tope, literales con
 * tope, respuestas de DNS mirando cada octeto antes de leerlo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "red.h"

int (*red_espera)(void *u);
void *red_espera_u;

const char *con_texto_error(int e)
{
	switch (e) {
	case SO_E_ESPERA: return "El servidor no contesta (se ha esperado demasiado).";
	case SO_E_RED: return "No hay red.";
	case SO_E_NOMBRE: return "No se encuentra el servidor.";
	case SO_E_CONEXION: return "El servidor no contesta.";
	case SO_E_CERTIFICADO: return "El certificado del servidor no vale.";
	case SO_E_TLS: return "La conexi\xc3\xb3n segura no se ha podido hacer.";
	case SO_E_STARTTLS: return "El servidor no ofrece STARTTLS: la conexi\xc3\xb3n ir\xc3\xad" "a sin cifrar.";
	case SO_E_CORTADA: return "Se ha cortado la conexi\xc3\xb3n.";
	}
	return "La red ha fallado.";
}

static void pon_error(Con *k, int e, const char *t)
{
	k->error = e;
	snprintf(k->texto, sizeof k->texto, "%s", t ? t : con_texto_error(e));
}

int con_abre(Con *k, const char *servidor, u32 puerto, int modo, u32 espera_ms)
{
	int c;

	memset(k, 0, sizeof *k);
	k->espera_ms = 60000;
	k->modo = modo;
	k->puerto = puerto;
	snprintf(k->servidor, sizeof k->servidor, "%s", servidor);
	c = so_conecta(servidor, puerto, modo, espera_ms, k->texto, sizeof k->texto);
	if (c <= 0) {
		k->error = c ? c : SO_E_OTRO;
		if (!k->texto[0])
			snprintf(k->texto, sizeof k->texto, "%s", con_texto_error(k->error));
		return k->error;
	}
	k->c = c;
	k->cabe = 16384;
	k->b = so_pide(k->cabe);
	if (!k->b) {
		so_cierra_con(c);
		k->c = 0;
		pon_error(k, SO_E_OTRO, "No hay memoria.");
		return SO_E_OTRO;
	}
	if (modo != SO_PLANO)
		so_cifrado(c, k->cifrado, sizeof k->cifrado);
	return 0;
}

void con_cierra(Con *k)
{
	if (k->c)
		so_cierra_con(k->c);
	k->c = 0;
	so_suelta(k->b);
	k->b = NULL;
	k->ini = k->fin = k->cabe = 0;
}

/* trae más (hasta que llegue algo, o se acabe el tiempo): 1 si ha llegado algo */
static int trae(Con *k)
{
	u32 hasta = so_tics() + (u32)((u64)k->espera_ms * so_hz() / 1000);
	int r;

	if (!k->c) {
		pon_error(k, SO_E_CORTADA, NULL);
		return -1;
	}
	if (k->ini && k->ini == k->fin)
		k->ini = k->fin = 0;
	if (k->fin + 4096 > k->cabe) {
		if (k->ini) {
			memmove(k->b, k->b + k->ini, k->fin - k->ini);
			k->fin -= k->ini;
			k->ini = 0;
		}
		if (k->fin + 4096 > k->cabe) {
			u32 n = k->cabe * 2;
			u8 *q;

			if (n > CON_MAX_RENGLON + 65536) {
				pon_error(k, SO_E_OTRO, "El servidor manda un rengl\xc3\xb3n demasiado largo.");
				return -1;
			}
			q = so_repide(k->b, n);
			if (!q) {
				pon_error(k, SO_E_OTRO, "No hay memoria.");
				return -1;
			}
			k->b = q;
			k->cabe = n;
		}
	}
	for (;;) {
		r = so_recibe(k->c, k->b + k->fin, k->cabe - k->fin, 200);
		if (r > 0) {
			k->fin += (u32)r;
			k->recibidos += (u32)r;
			return 1;
		}
		if (r == 0) {
			pon_error(k, SO_E_CORTADA, "El servidor ha cerrado la conexi\xc3\xb3n.");
			return -1;
		}
		if (r != SO_E_ESPERA) {
			pon_error(k, r, NULL);
			return -1;
		}
		if (red_espera && red_espera(red_espera_u)) {
			pon_error(k, SO_E_OTRO, "Cancelado.");
			return -1;
		}
		if ((int)(hasta - so_tics()) <= 0) {
			pon_error(k, SO_E_ESPERA, NULL);
			return -1;
		}
	}
}

int con_renglon(Con *k, char **r)
{
	u32 mirado = k->ini;

	for (;;) {
		u8 *p = memchr(k->b + mirado, '\n', k->fin - mirado);

		if (p) {
			u32 n = (u32)(p - (k->b + k->ini)), ini = k->ini;

			k->ini += n + 1;
			if (n && k->b[ini + n - 1] == '\r')
				n--;
			k->b[ini + n] = 0;
			*r = (char *)k->b + ini;
			return (int)n;
		}
		if (k->fin - k->ini > CON_MAX_RENGLON) {
			pon_error(k, SO_E_OTRO, "El servidor manda un rengl\xc3\xb3n demasiado largo.");
			return -1;
		}
		mirado = k->fin - k->ini;
		if (trae(k) < 0)
			return -1;
		mirado += k->ini;
	}
}

int con_lee(Con *k, u8 *d, u32 n)
{
	while (n) {
		u32 h = k->fin - k->ini;

		if (!h) {
			int r;

			k->ini = k->fin = 0;
			/* lo grande, derecho al destino */
			if (n >= 8192 && k->c) {
				u32 hasta = so_tics() + (u32)((u64)k->espera_ms * so_hz() / 1000);

				for (;;) {
					r = so_recibe(k->c, d, n > 8192 ? 8192 : n, 200);
					if (r > 0)
						break;
					if (r == 0) {
						pon_error(k, SO_E_CORTADA, "El servidor ha cerrado la conexi\xc3\xb3n.");
						return -1;
					}
					if (r != SO_E_ESPERA) {
						pon_error(k, r, NULL);
						return -1;
					}
					if (red_espera && red_espera(red_espera_u)) {
						pon_error(k, SO_E_OTRO, "Cancelado.");
						return -1;
					}
					if ((int)(hasta - so_tics()) <= 0) {
						pon_error(k, SO_E_ESPERA, NULL);
						return -1;
					}
				}
				k->recibidos += (u32)r;
				d += r;
				n -= (u32)r;
				if (red_espera && red_espera(red_espera_u)) {
					pon_error(k, SO_E_OTRO, "Cancelado.");
					return -1;
				}
				continue;
			}
			if (trae(k) < 0)
				return -1;
			continue;
		}
		if (h > n)
			h = n;
		memcpy(d, k->b + k->ini, h);
		k->ini += h;
		d += h;
		n -= h;
	}
	return 0;
}

int con_envia(Con *k, const void *d, u32 n)
{
	int r;

	if (!k->c) {
		pon_error(k, SO_E_CORTADA, NULL);
		return -1;
	}
	r = so_envia(k->c, d, n);
	if (r) {
		pon_error(k, r, NULL);
		return -1;
	}
	k->enviados += n;
	return 0;
}

int con_envia_texto(Con *k, const char *f, ...)
{
	char t[2048];
	va_list a;
	int n;

	va_start(a, f);
	n = vsnprintf(t, sizeof t, f, a);
	va_end(a);
	if (n < 0 || n >= (int)sizeof t) {
		pon_error(k, SO_E_OTRO, "Orden demasiado larga.");
		return -1;
	}
	return con_envia(k, t, (u32)n);
}

int con_hay(Con *k, u32 espera_ms)
{
	int r;

	if (k->fin > k->ini)
		return 1;
	if (!k->c)
		return -1;
	if (k->ini == k->fin)
		k->ini = k->fin = 0;
	if (k->fin + 4096 > k->cabe)
		return 1;
	r = so_recibe(k->c, k->b + k->fin, k->cabe - k->fin, espera_ms);
	if (r > 0) {
		k->fin += (u32)r;
		k->recibidos += (u32)r;
		return 1;
	}
	if (r == SO_E_ESPERA)
		return 0;
	pon_error(k, r ? r : SO_E_CORTADA, r ? NULL : "El servidor ha cerrado la conexi\xc3\xb3n.");
	return -1;
}

/* =============================================================================
 *  HTTP
 * ============================================================================= */
static int sin_caja(const char *a, const char *b, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		char x = a[i], y = b[i];

		if (x >= 'A' && x <= 'Z')
			x += 32;
		if (y >= 'A' && y <= 'Z')
			y += 32;
		if (x != y)
			return 0;
		if (!x)
			return 1;
	}
	return 1;
}

static u32 numero(const char *t, int base)
{
	u32 v = 0;

	for (; *t; t++) {
		int c = *t >= '0' && *t <= '9' ? *t - '0' : base == 16 && *t >= 'a' && *t <= 'f' ? *t - 'a' + 10 :
			base == 16 && *t >= 'A' && *t <= 'F' ? *t - 'A' + 10 : -1;

		if (c < 0)
			break;
		if (v > 0x0FFFFFFF)
			return 0xFFFFFFFF;
		v = v * (u32)base + (u32)c;
	}
	return v;
}

/* parte «https://servidor:puerto/camino» */
static int parte_url(const char *url, int *seguro, char *serv, int cabe, u32 *puerto, char *camino, int cc)
{
	const char *p, *b, *dp;
	u32 n;

	if (!strncmp(url, "https://", 8)) {
		*seguro = 1;
		p = url + 8;
		*puerto = 443;
	} else if (!strncmp(url, "http://", 7)) {
		*seguro = 0;
		p = url + 7;
		*puerto = 80;
	} else
		return -1;
	b = strchr(p, '/');
	if (!b)
		b = p + strlen(p);
	dp = memchr(p, '@', (size_t)(b - p));
	if (dp)
		return -1;	/* (usuario@servidor: no) */
	dp = memchr(p, ':', (size_t)(b - p));
	n = (u32)((dp ? dp : b) - p);
	if (n == 0 || n >= (u32)cabe)
		return -1;
	memcpy(serv, p, n);
	serv[n] = 0;
	if (dp) {
		*puerto = numero(dp + 1, 10);
		if (!*puerto || *puerto > 65535)
			return -1;
	}
	snprintf(camino, (size_t)cc, "%s", *b ? b : "/");
	return 0;
}

int http_get(const char *url, u8 **cuerpo, u32 *largo, u32 max, int solo_https, char *error, int cabe)
{
	char u[600], serv[128], camino[480], *r;
	u32 puerto, n = 0, total = 0xFFFFFFFF, aloja = 0;
	int seguro, vueltas, codigo = -1, trozos = 0, k;
	u8 *d = NULL;
	Con c;

	*cuerpo = NULL;
	*largo = 0;
	if (error && cabe)
		error[0] = 0;
	snprintf(u, sizeof u, "%s", url);
	for (vueltas = 0; vueltas < 4; vueltas++) {
		char nueva[600];

		nueva[0] = 0;
		if (parte_url(u, &seguro, serv, sizeof serv, &puerto, camino, sizeof camino) || (solo_https && !seguro)) {
			if (error)
				snprintf(error, (size_t)cabe, "La direcci\xc3\xb3n no vale: %s", u);
			return -1;
		}
		if (con_abre(&c, serv, puerto, seguro ? SO_TLS : SO_PLANO, 15000)) {
			if (error)
				snprintf(error, (size_t)cabe, "%s", c.texto);
			return c.error;
		}
		c.espera_ms = 20000;
		if (con_envia_texto(&c, "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Correo/1.0 (Hispa-Pentium)\r\n"
				    "Accept: */*\r\nConnection: close\r\n\r\n", camino, serv) < 0)
			goto mal;
		if (con_renglon(&c, &r) < 0)
			goto mal;
		if (strncmp(r, "HTTP/1.", 7) || strlen(r) < 12)
			goto mal;
		codigo = (int)numero(r + 9, 10);
		total = 0xFFFFFFFF;
		trozos = 0;
		for (;;) {
			k = con_renglon(&c, &r);
			if (k < 0)
				goto mal;
			if (k == 0)
				break;
			if (sin_caja(r, "content-length:", 15))
				total = numero(r + 15 + (r[15] == ' '), 10);
			else if (sin_caja(r, "transfer-encoding:", 18) && strstr(r, "hunked"))
				trozos = 1;
			else if (sin_caja(r, "location:", 9)) {
				const char *l = r + 9;

				while (*l == ' ')
					l++;
				if (*l == '/')
					snprintf(nueva, sizeof nueva, "%s://%s:%u%s", seguro ? "https" : "http", serv, (unsigned)puerto, l);
				else
					snprintf(nueva, sizeof nueva, "%s", l);
			}
		}
		if ((codigo == 301 || codigo == 302 || codigo == 303 || codigo == 307 || codigo == 308) && nueva[0]) {
			con_cierra(&c);
			snprintf(u, sizeof u, "%s", nueva);
			continue;
		}
		break;
	}
	if (vueltas == 4) {
		if (error)
			snprintf(error, (size_t)cabe, "Demasiadas redirecciones.");
		return -1;
	}
	aloja = total != 0xFFFFFFFF && total <= max ? total : 65536;
	if (total != 0xFFFFFFFF && total > max) {
		if (error)
			snprintf(error, (size_t)cabe, "Es demasiado grande (%u octetos).", (unsigned)total);
		con_cierra(&c);
		return -1;
	}
	d = so_pide(aloja + 1);
	if (!d)
		goto mal;
	for (;;) {
		u32 quiere;

		if (trozos) {
			if (con_renglon(&c, &r) < 0)
				goto mal;
			quiere = numero(r, 16);
			if (quiere == 0)
				break;
			if (quiere == 0xFFFFFFFF)
				goto mal;
		} else if (total != 0xFFFFFFFF) {
			if (n >= total)
				break;
			quiere = total - n;
		} else
			quiere = 4096;
		if (n + quiere > max) {
			if (error)
				snprintf(error, (size_t)cabe, "Es demasiado grande.");
			goto mal2;
		}
		if (n + quiere > aloja) {
			u32 m = aloja * 2 > n + quiere ? aloja * 2 : n + quiere;
			u8 *q;

			if (m > max)
				m = max;
			q = so_repide(d, m + 1);
			if (!q)
				goto mal;
			d = q;
			aloja = m;
		}
		if (trozos || total != 0xFFFFFFFF) {
			if (con_lee(&c, d + n, quiere) < 0)
				goto mal;
			n += quiere;
			if (trozos && con_renglon(&c, &r) < 0)
				goto mal;
		} else {
			/* hasta que se cierre */
			if (c.fin > c.ini) {
				u32 h = c.fin - c.ini;

				if (h > quiere)
					h = quiere;
				memcpy(d + n, c.b + c.ini, h);
				c.ini += h;
				n += h;
				continue;
			}
			k = so_recibe(c.c, d + n, quiere, 15000);
			if (k <= 0)
				break;
			n += (u32)k;
		}
	}
	con_cierra(&c);
	d[n] = 0;
	*cuerpo = d;
	*largo = n;
	return codigo;
mal:
	if (error && !error[0])
		snprintf(error, (size_t)cabe, "%s", c.texto[0] ? c.texto : "La respuesta del servidor no se entiende.");
mal2:
	con_cierra(&c);
	so_suelta(d);
	return -1;
}

/* =============================================================================
 *  El DNS: SRV
 * ============================================================================= */
u32 dns_pregunta(const char *nombre, u16 id, u16 tipo, u8 *d, u32 cabe)
{
	u32 n = 12;
	const char *p = nombre;

	if (cabe < 12 + strlen(nombre) + 6 || strlen(nombre) > 250)
		return 0;
	memset(d, 0, 12);
	d[0] = (u8)(id >> 8);
	d[1] = (u8)id;
	d[2] = 0x01;		/* que se pregunte por nosotros (RD) */
	d[5] = 1;		/* una pregunta */
	while (*p) {
		const char *q = strchr(p, '.');
		u32 k = q ? (u32)(q - p) : (u32)strlen(p);

		if (k == 0 || k > 63)
			return 0;
		d[n++] = (u8)k;
		memcpy(d + n, p, k);
		n += k;
		p += k;
		if (*p == '.')
			p++;
	}
	d[n++] = 0;
	d[n++] = (u8)(tipo >> 8);
	d[n++] = (u8)tipo;
	d[n++] = 0;
	d[n++] = 1;		/* IN */
	return n;
}

/* un nombre del DNS (con sus punteros, que no se vuelvan bucles), a texto */
static int nombre_dns(const u8 *r, u32 n, u32 *i, char *d, int cabe)
{
	u32 p = *i, saltos = 0, k = 0;
	int saltado = 0;

	if (cabe < 2)
		return -1;
	for (;;) {
		u32 l;

		if (p >= n)
			return -1;
		l = r[p];
		if ((l & 0xC0) == 0xC0) {
			if (p + 1 >= n || ++saltos > 16)
				return -1;
			if (!saltado)
				*i = p + 2;
			saltado = 1;
			p = (l & 0x3F) << 8 | r[p + 1];
			continue;
		}
		if (l & 0xC0)
			return -1;
		p++;
		if (l == 0)
			break;
		if (p + l > n || k + l + 2 > (u32)cabe)
			return -1;
		if (k)
			d[k++] = '.';
		memcpy(d + k, r + p, l);
		k += l;
		p += l;
	}
	if (!saltado)
		*i = p;
	d[k] = 0;
	return 0;
}

int dns_lee_srv(const u8 *r, u32 n, u16 id, char *servidor, int cabe, u32 *puerto)
{
	u32 i = 12, q, a, k, mejor = 0x10000;
	char nom[256];

	if (n < 12 || r[0] != (u8)(id >> 8) || r[1] != (u8)id || !(r[2] & 0x80) || (r[3] & 0x0F))
		return 0;
	q = (u32)r[4] << 8 | r[5];
	a = (u32)r[6] << 8 | r[7];
	for (k = 0; k < q; k++) {
		if (nombre_dns(r, n, &i, nom, sizeof nom) || i + 4 > n)
			return 0;
		i += 4;
	}
	for (k = 0; k < a && k < 64; k++) {
		u32 tipo, l;

		if (nombre_dns(r, n, &i, nom, sizeof nom) || i + 10 > n)
			return 0;
		tipo = (u32)r[i] << 8 | r[i + 1];
		l = (u32)r[i + 8] << 8 | r[i + 9];
		i += 10;
		if (i + l > n)
			return 0;
		if (tipo == 33 && l >= 7) {
			u32 prio = (u32)r[i] << 8 | r[i + 1], pu = (u32)r[i + 4] << 8 | r[i + 5], j = i + 6;
			char s[256];

			if (!nombre_dns(r, n, &j, s, sizeof s) && s[0] && strcmp(s, ".") && prio < mejor && pu) {
				mejor = prio;
				snprintf(servidor, (size_t)cabe, "%s", s);
				*puerto = pu;
			}
		}
		i += l;
	}
	return mejor < 0x10000;
}

int dns_srv(const char *nombre, char *servidor, int cabe, u32 *puerto)
{
	u8 p[300], r[1500];
	u16 id;
	u32 n;
	int m;

	so_azar(&id, sizeof id);
	n = dns_pregunta(nombre, id, 33, p, sizeof p);
	if (!n)
		return 0;
	m = so_dns(p, n, r, sizeof r, 3000);
	if (m <= 0)
		return 0;
	return dns_lee_srv(r, (u32)m, id, servidor, cabe, puerto);
}
