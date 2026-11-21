/* wgmain.c - WG.BIN, el túnel WireGuard de Hispa-Pentium, como el de
 * wg-quick, con el archivo de siempre:
 *
 *   WG                     cómo está (como «wg show»)
 *   WG /ENCENDER [archivo] lo pone en marcha (\HISPA\WG\WG0.CNF si no se dice)
 *   WG /APAGAR             lo quita
 *   WG /LLAVE              una clave privada nueva y su pública
 *   WG /PUBLICA clave      la pública de una privada
 *
 * El archivo (wg-quick: [Interface] PrivateKey, Address, ListenPort;
 * [Peer] PublicKey, PresharedKey, Endpoint, AllowedIPs, PersistentKeepalive):
 * solo IPv4; la ruta por defecto (0.0.0.0/0) todavía no. Lo que va a las
 * redes de AllowedIPs sale por el túnel (la interfaz del túnel de RED.BIN).
 * El servicio (WG.BIN /SERVICIO) deja cómo está en \HISPA\WG\ESTADO.TXT.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "wg.h"
#include "cripto.h"
#include "monocypher.h"

static char carpeta[48];
static u32 sock;
static u32 mi_ip, mi_masc, puerto_mio;
static u32 rutas[16];
static int n_rutas;

static void ruta_de(char *d, const char *n) { formatear(d, 96, "%s\\%s", carpeta, n); }

int wg_manda(struct wg_par *p, const u8 *d, u32 n)
{
	if (!p->ep_ip)
		return -1;
	return ES_ERROR(h_udp_enviar(sock, p->ep_ip, p->ep_puerto, d, n)) ? -1 : 0;
}

static u32 mascara(int bits)
{
	u32 m = 0;
	int i;

	for (i = 0; i < bits && i < 32; i++)
		m |= 1u << ((i / 8) * 8 + 7 - i % 8);
	return m;
}

/* «a.b.c.d/n»: 1 si vale */
static int cidr(const char *t, u32 *ip, u32 *masc)
{
	char d[20];
	int i, bits = 32;

	for (i = 0; t[i] && t[i] != '/' && i < 19; i++)
		d[i] = t[i];
	d[i] = 0;
	if (t[i] == '/') {
		bits = 0;
		for (i++; t[i] >= '0' && t[i] <= '9'; i++)
			bits = bits * 10 + t[i] - '0';
		if (bits > 32)
			return 0;
	}
	if (!h_texto_a_ip(d, ip))
		return 0;
	*masc = mascara(bits);
	return 1;
}

static void quita_espacios(char *t)
{
	int i = 0, k = 0;

	while (t[i]) {
		if (t[i] != ' ' && t[i] != '\t')
			t[k++] = t[i];
		i++;
	}
	t[k] = 0;
}

static int lee_conf(const char *archivo)
{
	static char t[16384];
	int n = h_lee_archivo(archivo, t, sizeof t - 1), i = 0, seccion = 0, priv = 0;
	struct wg_par *p = 0;
	u8 clave[32];

	if (n <= 0) {
		h_apunta("no se puede leer %s", archivo);
		return -1;
	}
	t[n] = 0;
	while (i < n) {
		char l[600], *v;
		int k = 0;
		while (i < n && t[i] != '\n') {
			if (t[i] != '\r' && k < (int)sizeof l - 1)
				l[k++] = t[i];
			i++;
		}
		i++;
		l[k] = 0;
		for (k = 0; l[k] && l[k] != '#'; k++) ;
		l[k] = 0;
		quita_espacios(l);
		if (!l[0])
			continue;
		if (h_es_ascii_igual(l, "[Interface]")) {
			seccion = 1;
			continue;
		}
		if (h_es_ascii_igual(l, "[Peer]")) {
			seccion = 2;
			p = 0;
			continue;
		}
		for (v = l; *v && *v != '='; v++) ;
		if (!*v)
			continue;
		*v++ = 0;
		if (seccion == 1) {
			if (h_es_ascii_igual(l, "PrivateKey")) {
				if (!wg_de_base64(v, clave)) {
					h_apunta("PrivateKey no vale");
					return -1;
				}
				wg_inicia(clave);
				crypto_wipe(clave, 32);
				priv = 1;
			} else if (h_es_ascii_igual(l, "Address")) {
				char *c = v;
				while (*c) {
					char *e = c;
					while (*e && *e != ',')
						e++;
					if (*e)
						*e++ = 0;
					if (!mi_ip && cidr(c, &mi_ip, &mi_masc))
						;
					c = e;
				}
			} else if (h_es_ascii_igual(l, "ListenPort")) {
				puerto_mio = 0;
				for (; *v >= '0' && *v <= '9'; v++)
					puerto_mio = puerto_mio * 10 + *v - '0';
			}
		} else if (seccion == 2) {
			if (h_es_ascii_igual(l, "PublicKey")) {
				if (!priv) {
					h_apunta("[Peer] antes que la PrivateKey de [Interface]");
					return -1;
				}
				if (!wg_de_base64(v, clave)) {
					h_apunta("PublicKey no vale: %s", v);
					return -1;
				}
				p = wg_nuevo_par(clave);
				if (!p)
					return -1;
				formatear(p->nombre, sizeof p->nombre, "par %u", (u32)(p - wg_pares) + 1);
			} else if (!p) {
				h_apunta("%s antes que la PublicKey del [Peer]", l);
			} else if (h_es_ascii_igual(l, "PresharedKey")) {
				if (!wg_de_base64(v, p->psk))
					h_apunta("PresharedKey no vale");
			} else if (h_es_ascii_igual(l, "Endpoint")) {
				char *c = v, *dp = 0;
				u32 ip;
				for (; *c; c++)
					if (*c == ':')
						dp = c;
				if (dp) {
					*dp = 0;
					p->ep_puerto = 0;
					for (c = dp + 1; *c >= '0' && *c <= '9'; c++)
						p->ep_puerto = p->ep_puerto * 10 + *c - '0';
					if (ES_ERROR(h_resolver(v, &ip)))
						h_apunta("Endpoint %s: no se encuentra", v);
					else
						p->ep_ip = ip;
					formatear(p->nombre, sizeof p->nombre, "%s", v);
				}
			} else if (h_es_ascii_igual(l, "AllowedIPs")) {
				char *c = v;
				while (*c) {
					char *e = c;
					u32 ip, m;
					while (*e && *e != ',')
						e++;
					if (*e)
						*e++ = 0;
					if (cidr(c, &ip, &m)) {
						if (m == 0)
							h_apunta("AllowedIPs 0.0.0.0/0 (todo por el túnel): todavía no; solo las demás");
						else if (p->n_redes < WG_MAX_REDES) {
							p->red[p->n_redes] = ip & m;
							p->masc[p->n_redes++] = m;
						}
					}
					c = e;
				}
			} else if (h_es_ascii_igual(l, "PersistentKeepalive")) {
				p->mantener = 0;
				for (; *v >= '0' && *v <= '9'; v++)
					p->mantener = p->mantener * 10 + *v - '0';
			}
		}
	}
	if (!priv || !mi_ip) {
		h_apunta("faltan PrivateKey o Address en [Interface]");
		return -1;
	}
	return 0;
}

static void pon_rutas(void)
{
	int i, k;
	u32 r;

	n_rutas = 0;
	rutas[0] = mi_ip & mi_masc, rutas[1] = mi_masc;
	n_rutas = 1;
	for (i = 0; i < WG_MAX_PARES; i++) {
		struct wg_par *p = &wg_pares[i];
		if (!p->usado)
			continue;
		for (k = 0; k < p->n_redes && n_rutas < 8; k++) {
			int j, ya = 0;
			for (j = 0; j < n_rutas; j++)
				if (rutas[2 * j] == p->red[k] && rutas[2 * j + 1] == p->masc[k])
					ya = 1;
			if (!ya) {
				rutas[2 * n_rutas] = p->red[k];
				rutas[2 * n_rutas + 1] = p->masc[k];
				n_rutas++;
			}
		}
	}
	r = h_red(O_TUNEL, mi_ip, 0, 0, rutas, n_rutas * 8, rutas, 0);
	if (ES_ERROR(r))
		h_apunta("RED.BIN no deja poner la interfaz del túnel (¿hay otro túnel en marcha?)");
}

static int encendido(void)
{
	char r[100], t[300], v[16];
	int n;

	ruta_de(r, "WG.INI");
	n = h_lee_archivo(r, t, sizeof t - 1);
	if (n <= 0)
		return 0;
	t[n] = 0;
	return h_ini(t, "encendido", v, sizeof v) && v[0] == '1';
}

static void escribe_estado(void)
{
	static char t[8192];
	char b[48], ip[16], r[100];
	int k = 0, i;

	wg_a_base64(wg_pub, b);
	h_ip_a_texto(mi_ip, ip);
	k += formatear(t + k, sizeof t - k, "interfaz: %s\r\n  clave pública: %s\r\n  puerto: %u\r\n", ip, b, puerto_mio);
	for (i = 0; i < WG_MAX_PARES; i++) {
		struct wg_par *p = &wg_pares[i];
		int j;
		if (!p->usado)
			continue;
		wg_a_base64(p->pub, b);
		k += formatear(t + k, sizeof t - k, "\r\npar: %s\r\n", b);
		if (p->ep_ip) {
			h_ip_a_texto(p->ep_ip, ip);
			k += formatear(t + k, sizeof t - k, "  dirección: %s:%u\r\n", ip, p->ep_puerto);
		}
		k += formatear(t + k, sizeof t - k, "  redes:");
		for (j = 0; j < p->n_redes; j++) {
			u32 m = p->masc[j], bits = 0;
			while (m) {
				bits += m & 1;
				m >>= 1;
			}
			h_ip_a_texto(p->red[j], ip);
			k += formatear(t + k, sizeof t - k, " %s/%u", ip, bits);
		}
		k += formatear(t + k, sizeof t - k, "\r\n");
		if (p->hs_fecha) {
			u32 f = p->hs_fecha;
			k += formatear(t + k, sizeof t - k, "  último apretón de manos: %02u:%02u:%02u (hace %u s)\r\n", (f & 0xFFFF) >> 11,
				       (f >> 5) & 63, (f & 31) * 2, p->act.vale ? (h_tics() - p->act.nacida) / h_hz : 0);
		} else
			k += formatear(t + k, sizeof t - k, "  aún sin apretón de manos\r\n");
		k += formatear(t + k, sizeof t - k, "  recibido: %u K, enviado: %u K\r\n", (u32)(p->rx / 1024), (u32)(p->tx / 1024));
		if (p->mantener)
			k += formatear(t + k, sizeof t - k, "  «sigo aquí» cada %u s\r\n", p->mantener);
	}
	ruta_de(r, "ESTADO.TXT");
	h_escribe_archivo(r, t, k);
}

static int servicio(const char *archivo)
{
	static u8 d[2048], sale[2048];
	u32 ult_tic = h_tics(), ult_estado = h_tics(), ult_ini = h_tics(), r, ip, pu;
	int i;

	sc(SC_ATENDER_CORTE, 1, 0, 0);
	if (lee_conf(archivo) < 0)
		return 1;
	sock = h_udp_abrir(puerto_mio);
	if (ES_ERROR(sock)) {
		h_apunta("no se puede abrir el puerto UDP %u: %s", puerto_mio, h_texto_error_red(sock));
		return 1;
	}
	pon_rutas();
	{
		char b[48], t[16];
		wg_a_base64(wg_pub, b);
		h_ip_a_texto(mi_ip, t);
		h_apunta("Hispa-Pentium: WireGuard en marcha (%s, clave pública %s)", t, b);
	}
	escribe_estado();
	for (;;) {
		struct wg_par *quien;
		int n;
		r = h_udp_recibir(sock, d, sizeof d, 1, &ip, &pu);
		if (!ES_ERROR(r) && r > 0) {
			n = wg_entra(d, r, ip, pu, sale, &quien);
			if (n > 0)
				h_red(O_TUNEL_ENTRA, 0, 0, 0, sale, n, sale, 0);
		}
		for (i = 0; i < 8; i++) {
			r = h_red(O_TUNEL_LEER, 0, 0, 0, 0, 0, d, sizeof d);
			if (ES_ERROR(r) || r == 0)
				break;
			wg_sale(d, r);
		}
		if (h_tics() - ult_tic >= h_hz / 10 + 1) {
			ult_tic = h_tics();
			wg_tic();
		}
		if (h_tics() - ult_estado >= 5 * h_hz) {
			ult_estado = h_tics();
			escribe_estado();
		}
		if (h_tics() - ult_ini >= 2 * h_hz) {
			ult_ini = h_tics();
			if (!encendido()) {
				h_apunta("apagado (WG /APAGAR)");
				h_red(O_TUNEL_QUITAR, 0, 0, 0, 0, 0, 0, 0);
				h_cerrar_red(sock);
				for (i = 0; i < WG_MAX_PARES; i++)
					wg_quita_par(&wg_pares[i]);
				crypto_wipe(wg_priv, 32);
				return 0;
			}
		}
	}
}

static void ayuda(void)
{
	h_escribe("WG: un túnel WireGuard (como wg-quick).\n"
		  "  WG                     cómo está\n"
		  "  WG /ENCENDER [archivo] lo pone en marcha (\\HISPA\\WG\\WG0.CNF si no se dice)\n"
		  "  WG /APAGAR             lo quita\n"
		  "  WG /LLAVE              una clave privada nueva y su pública\n"
		  "  WG /PUBLICA clave      la pública de una privada\n");
}

int principal(void)
{
	char a[300], r[100], archivo[160];

	h_argumentos(a, sizeof a);
	h_inicia("wg");
	h_ruta(carpeta, sizeof carpeta, "WG");
	h_carpeta(carpeta);
	if (h_empieza(a, "/SERVICIO ")) {
		h_copia(archivo, a + 10, sizeof archivo);
		return servicio(archivo);
	}
	if (h_empieza(a, "/LLAVE")) {
		u8 k[32], p[32];
		char b[48];
		h_azar(k, 32);
		k[0] &= 248, k[31] = (k[31] & 127) | 64;
		crypto_x25519_public_key(p, k);
		wg_a_base64(k, b);
		h_escribef("PrivateKey = %s\n", b);
		wg_a_base64(p, b);
		h_escribef("(su PublicKey = %s)\n", b);
		h_escribe("La privada va en [Interface] del archivo; la pública, en el [Peer] del otro equipo.\n");
		crypto_wipe(k, 32);
		return 0;
	}
	if (h_empieza(a, "/PUBLICA ")) {
		u8 k[32], p[32];
		char b[48];
		if (!wg_de_base64(a + 9, k)) {
			h_escribe("Esa clave no vale (son 44 letras en base64).\n");
			return 1;
		}
		crypto_x25519_public_key(p, k);
		crypto_wipe(k, 32);
		wg_a_base64(p, b);
		h_escribef("%s\n", b);
		return 0;
	}
	if (h_empieza(a, "/ENCENDER")) {
		char t[300], arg[200];
		u32 tarea;
		const char *f = a + 9;
		while (*f == ' ')
			f++;
		if (*f)
			h_copia(archivo, f, sizeof archivo);
		else
			ruta_de(archivo, "WG0.CNF");
		if (!h_existe(archivo)) {
			h_escribef("No está %s (el archivo de wg-quick: [Interface] y [Peer]).\n", archivo);
			return 1;
		}
		formatear(t, sizeof t, "encendido=1\r\narchivo=%s\r\n", archivo);
		ruta_de(r, "WG.INI");
		h_escribe_archivo(r, t, strlen(t));
		formatear(arg, sizeof arg, "/SERVICIO %s", archivo);
		formatear(r, sizeof r, "%c:\\HISPA\\WG.BIN", h_sistema);
		tarea = sc(SC_LANZAR_SUELTO, (u32)r, (u32)arg, 0);
		if (ES_ERROR(tarea)) {
			formatear(r, sizeof r, "%c:\\WG.BIN", h_sistema);
			tarea = sc(SC_LANZAR_SUELTO, (u32)r, (u32)arg, 0);
		}
		if (ES_ERROR(tarea)) {
			h_escribe("No se puede poner en marcha WG.BIN.\n");
			return 1;
		}
		h_escribef("WireGuard en marcha con %s (WG: cómo está).\n", archivo);
		return 0;
	}
	if (h_empieza(a, "/APAGAR")) {
		ruta_de(r, "WG.INI");
		h_escribe_archivo(r, "encendido=0\r\n", 13);
		h_escribe("WireGuard apagado (en unos segundos).\n");
		return 0;
	}
	if (a[0]) {
		ayuda();
		return 0;
	}
	{
		static char t[8192];
		int n;
		ruta_de(r, "ESTADO.TXT");
		n = h_lee_archivo(r, t, sizeof t - 1);
		if (!encendido() || n <= 0) {
			h_escribe("WireGuard: apagado (WG /ENCENDER para ponerlo en marcha; WG /? la ayuda).\n");
			return 0;
		}
		t[n] = 0;
		for (n = 0; t[n]; n++)
			if (t[n] == '\r')
				t[n] = ' ';
		h_escribe(t);
	}
	return 0;
}
