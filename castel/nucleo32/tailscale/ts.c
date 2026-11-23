/* ts.c - TAILSCALE.BIN, el cliente de Tailscale de Hispa-Pentium:
 *
 *   TAILSCALE                  cómo está: el nombre, la IP 100.x y los equipos de la red
 *   TAILSCALE /CONECTAR [url]  se conecta (con otra url, a ese servidor: Headscale)
 *   TAILSCALE /DESCONECTAR     se desconecta (y queda así en los arranques)
 *   TAILSCALE /OLVIDAR         olvida las claves: la próxima vez es un equipo nuevo
 *   TAILSCALE /IP              la IP 100.x de este equipo
 *
 * La primera vez el servidor de control da una dirección de alta: se abre
 * en otro equipo (el móvil, otro ordenador) con la cuenta de Tailscale y se
 * aprueba; desde entonces el equipo está en la red con su nombre y su IP
 * 100.x, y los demás se ven por su nombre (MagicDNS: «ssh carlos@mac3»).
 * Los paquetes van con WireGuard (wg/wg.c), por los relevos DERP (derp.c);
 * lo directo por UDP con perforación de NAT, todavía no. Lo que entra se
 * filtra con las reglas de la red (las ACL de Tailscale).
 *
 * Lo suyo va en \HISPA\TS: TS.INI (encendido=, servidor=, nombre=, derp=),
 * CLAVES.TS (las claves de la máquina, del nodo y del disco: secretas) y
 * ESTADO.TXT (cómo está, para la orden y para Configuración).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ts.h"
#include "cripto.h"
#include "monocypher.h"

static char carpeta[48], nombre_equipo[64], mi_nombre[128], dominio[128], usuario_red[96];
static u32 mi_ip;
static char url_alta[300];
static const char *estado = "apagado";
static int casa_elegida;

struct ts_region ts_regiones[TS_MAX_REGIONES];
int ts_n_regiones;

#define MAX_PARES 64
struct ts_par {
	int usado;
	long long id;
	u8 clave[32];
	char nombre[96], host[64], os[24];
	u32 ip;
	int derp, en_linea;
	struct wg_par *wg;
};
static struct ts_par pares[MAX_PARES];

/* ---- las reglas de lo que entra (PacketFilter) ---- */
#define MAX_REGLAS 64
struct regla { u32 src, src_m, dst, dst_m; u16 p0, p1; u8 protos; };	/* protos: 1 TCP, 2 UDP, 4 ICMP */
static struct regla reglas[MAX_REGLAS];
static int n_reglas, hay_filtro;
#define MAX_SALIDAS 64
struct salida { u32 ip, tic; u16 rpuerto, lpuerto; u8 proto; };
static struct salida salidas[MAX_SALIDAS];
static int sig_salida;

static void ruta_de(char *d, const char *n) { formatear(d, 96, "%s\\%s", carpeta, n); }

struct ts_region *ts_region(int id)
{
	int i;

	for (i = 0; i < ts_n_regiones; i++)
		if (ts_regiones[i].id == id)
			return &ts_regiones[i];
	return 0;
}

static u32 ip_le(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }

/* ---- lo que se manda con WireGuard: por el DERP del otro ---- */
int wg_manda(struct wg_par *p, const u8 *d, u32 n)
{
	struct ts_par *t = p->dato;
	int region = t && t->derp ? t->derp : derp_casa;
	struct ts_region *r;

	if (!derp_region_conectada(region)) {
		r = ts_region(region);
		if (!r || derp_conecta(region, r->host, r->ip, r->puerto) < 0) {
			if (region == derp_casa)
				return -1;
			region = derp_casa;
		}
	}
	return derp_manda(region, p->pub, d, n);
}

/* ---- las claves ---- */
static int claves(void)
{
	char r[100];
	u8 k[96];

	ruta_de(r, "CLAVES.TS");
	if (h_lee_archivo(r, k, 96) != 96) {
		h_azar(k, 96);
		if (h_escribe_archivo(r, k, 96) < 0) {
			h_apunta("no se pueden guardar las claves en %s", r);
			crypto_wipe(k, 96);
			return -1;
		}
		h_apunta("claves nuevas (máquina, nodo y disco)");
	}
	memcpy(ts_maquina, k, 32);
	memcpy(ts_nodo, k + 32, 32);
	memcpy(ts_disco, k + 64, 32);
	crypto_wipe(k, 96);
	crypto_x25519_public_key(ts_maquina_pub, ts_maquina);
	crypto_x25519_public_key(ts_nodo_pub, ts_nodo);
	crypto_x25519_public_key(ts_disco_pub, ts_disco);
	return 0;
}

static int lee_ini(char *t, int cabe)
{
	char r[100];
	int n;

	ruta_de(r, "TS.INI");
	n = h_lee_archivo(r, t, cabe - 1);
	if (n < 0)
		n = 0;
	t[n] = 0;
	return n;
}

static void pon_ini(int encendido, const char *servidor, int derp)
{
	char t[600], r[100];
	int k;

	k = formatear(t, sizeof t, "; Tailscale en Hispa-Pentium (TAILSCALE)\r\nencendido=%u\r\nservidor=%s\r\n", encendido, servidor);
	if (nombre_equipo[0])
		k += formatear(t + k, sizeof t - k, "nombre=%s\r\n", nombre_equipo);
	if (derp)
		k += formatear(t + k, sizeof t - k, "derp=%u\r\n", derp);
	ruta_de(r, "TS.INI");
	h_escribe_archivo(r, t, k);
}

static int encendido(void)
{
	char t[600], v[16];

	lee_ini(t, sizeof t);
	return h_ini(t, "encendido", v, sizeof v) && v[0] == '1';
}

static int derp_ini(void)
{
	char t[600], v[16];
	int r = 0, i;

	lee_ini(t, sizeof t);
	if (h_ini(t, "derp", v, sizeof v))
		for (i = 0; v[i] >= '0' && v[i] <= '9'; i++)
			r = r * 10 + v[i] - '0';
	return r;
}

static void lee_ajustes(void)
{
	char t[600];

	lee_ini(t, sizeof t);
	if (!h_ini(t, "servidor", ts_servidor, sizeof ts_servidor) || !ts_servidor[0])
		h_copia(ts_servidor, "https://controlplane.tailscale.com", sizeof ts_servidor);
	if (!h_ini(t, "nombre", nombre_equipo, sizeof nombre_equipo) || !nombre_equipo[0]) {
		memset(nombre_equipo, 0, sizeof nombre_equipo);
		sc(SC_AJUSTES, 3, (u32)nombre_equipo, 0);
		nombre_equipo[63] = 0;
		if (!nombre_equipo[0])
			h_copia(nombre_equipo, "hispa-pentium", sizeof nombre_equipo);
	}
	/* (en minúsculas y sin espacios: así lo quieren los nombres de la red) */
	{
		int i;
		for (i = 0; nombre_equipo[i]; i++) {
			char c = nombre_equipo[i];
			if (c >= 'A' && c <= 'Z')
				c += 32;
			if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
				c = '-';
			nombre_equipo[i] = c;
		}
	}
}

/* ---- cómo está, en ESTADO.TXT ---- */
static void escribe_estado(void)
{
	static char t[12288];
	char r[100], ip[16];
	int k = 0, i;

	h_ip_a_texto(mi_ip, ip);
	k += formatear(t + k, sizeof t - k, "estado=%s\r\nservidor=%s\r\nnombre=%s\r\nip=%s\r\n", estado, ts_servidor,
		       mi_nombre[0] ? mi_nombre : nombre_equipo, mi_ip ? ip : "");
	if (url_alta[0])
		k += formatear(t + k, sizeof t - k, "alta=%s\r\n", url_alta);
	if (usuario_red[0])
		k += formatear(t + k, sizeof t - k, "usuario=%s\r\n", usuario_red);
	if (ts_error[0])
		k += formatear(t + k, sizeof t - k, "error=%s\r\n", ts_error);
	if (derp_casa) {
		struct ts_region *rg = ts_region(derp_casa);
		k += formatear(t + k, sizeof t - k, "derp=%u %s%s\r\n", derp_casa, rg ? rg->nombre : "", derp_region_conectada(derp_casa) ? "" : " (sin conectar)");
	}
	for (i = 0; i < MAX_PARES && k < (int)sizeof t - 300; i++) {
		struct ts_par *p = &pares[i];
		const char *via = "";
		if (!p->usado)
			continue;
		h_ip_a_texto(p->ip, ip);
		if (p->wg && p->wg->act.vale)
			via = " (túnel hecho)";
		k += formatear(t + k, sizeof t - k, "equipo=%s|%s|%s|%s|%u%s\r\n", p->host[0] ? p->host : p->nombre, ip,
			       p->en_linea ? "en línea" : "desconectado", p->os, p->derp, via);
	}
	ruta_de(r, "ESTADO.TXT");
	h_escribe_archivo(r, t, k);
}

/* ---- el JSON de lo que se manda ---- */
static int hostinfo(char *d, int cabe)
{
	char nom[80];

	j_cadena(nom, sizeof nom, nombre_equipo);
	return formatear(d, cabe,
			 "{\"IPNVersion\":\"1.80.0-hispa\",\"OS\":\"linux\",\"OSVersion\":\"Hispa-Pentium 0.8\",\"Distro\":\"hispa-pentium\","
			 "\"Hostname\":%s,\"GoArch\":\"386\",\"NetInfo\":{\"PreferredDERP\":%u}}", nom, derp_casa);
}

static void hex32(const char *pref, const u8 k[32], char *d)
{
	int n = strlen(pref);

	memcpy(d, pref, n);
	h_hex(k, 32, d + n);
}

static int registra(void)
{
	static char j[2048], r[8192];
	char nk[80], hi[600];
	int n, cod, intentos = 0;
	struct jv o, v;

	hex32("nodekey:", ts_nodo_pub, nk);
	hostinfo(hi, sizeof hi);
	for (;;) {
		char fu[340];
		j_cadena(fu, sizeof fu, url_alta);
		n = formatear(j, sizeof j,
			      "{\"Version\":%u,\"NodeKey\":\"%s\",\"OldNodeKey\":\"nodekey:0000000000000000000000000000000000000000000000000000000000000000\","
			      "\"Hostinfo\":%s,\"Followup\":%s}", TS_CAPVER, nk, hi, fu);
		ts_espera_cabeceras = url_alta[0] ? 900 : 60;	/* (con Followup, el servidor espera a que se apruebe) */
		cod = ts_pide("/machine/register", j, n, 0);
		ts_espera_cabeceras = 120;
		if (cod < 0)
			return -1;
		ts_cuerpo_entero(r, sizeof r, 600 * h_hz);
		/* el código puede no entenderse (299: «:status» en otra forma): si el cuerpo es la
		   respuesta del alta (con «MachineAuthorized» o «AuthURL»), vale igual */
		if (!j_valor(r, strlen(r), &o) || (cod != 200 && !j_campo(o, "MachineAuthorized", &v) && !j_campo(o, "AuthURL", &v))) {
			if (cod != 200)
				formatear(ts_error, sizeof ts_error, "el registro no va (%d): %s", cod, r);
			else
				formatear(ts_error, sizeof ts_error, "el registro: respuesta que no se entiende: %s", r);
			return -1;
		}
		if (j_campo(o, "Error", &v) && v.n > 2) {
			j_texto(v, ts_error, sizeof ts_error);
			return -1;
		}
		if (j_ruta(o, "Login.DisplayName", &v) || j_ruta(o, "User.DisplayName", &v) || j_ruta(o, "Login.LoginName", &v))
			j_texto(v, usuario_red, sizeof usuario_red);
		if (j_campo(o, "MachineAuthorized", &v) && j_cierto(v)) {
			url_alta[0] = 0;
			h_apunta("registrado en la red%s%s", usuario_red[0] ? " de " : "", usuario_red);
			estado = "conectando";
			return 0;
		}
		if (j_campo(o, "AuthURL", &v) && v.n > 2) {
			char nueva[300];
			j_texto(v, nueva, sizeof nueva);
			if (strcmp(nueva, url_alta)) {
				h_copia(url_alta, nueva, sizeof url_alta);
				h_apunta("dar de alta este equipo: abra %s", url_alta);
				estado = "alta";
				escribe_estado();
			}
			continue;	/* (y otra vez, con Followup: espera a que se apruebe) */
		}
		if (++intentos > 3) {
			formatear(ts_error, sizeof ts_error, "el servidor de control no da de alta el equipo");
			return -1;
		}
	}
}

/* El MapRequest. Con «Stream» (y Version >= 68) el servidor lo toma como de
 * solo leer: no guarda ni el DiscoKey, ni el Hostinfo (NetInfo.PreferredDERP,
 * de donde sale el HomeDERP que ven los demás), ni los Endpoints (tailcfg.go,
 * MapRequest.Stream). Eso se guarda con la «actualización ligera»: Stream
 * false y OmitPeers true (lo que hace el cliente oficial en SendUpdate); la
 * respuesta solo dice el código. Así que primero esa y luego el flujo. */
static int peticion_mapa(char *j, int cabe, int flujo)
{
	char nk[80], dk[80], hi[600];

	hex32("nodekey:", ts_nodo_pub, nk);
	hex32("discokey:", ts_disco_pub, dk);
	hostinfo(hi, sizeof hi);
	/* (sin Endpoints: de momento, solo por DERP; sin «Compress»: el mapa en claro) */
	return formatear(j, cabe, "{\"Version\":%u,\"KeepAlive\":true,\"NodeKey\":\"%s\",\"DiscoKey\":\"%s\",%s\"Hostinfo\":%s}",
			 TS_CAPVER, nk, dk, flujo ? "\"Stream\":true," : "\"OmitPeers\":true,", hi);
}

static int actualiza_nodo(void)
{
	static char j[2048], r[2048];
	int n, cod;

	n = peticion_mapa(j, sizeof j, 0);
	cod = ts_pide("/machine/map", j, n, 0);
	if (cod < 0)
		return -1;
	ts_cuerpo_entero(r, sizeof r, 30 * h_hz);
	if (cod != 200 && cod != 299) {
		formatear(ts_error, sizeof ts_error, "el servidor de control no guarda los datos del equipo (%d): %s", cod, r);
		return -1;
	}
	return 0;
}

static int pide_mapa(void)
{
	static char j[2048];
	int n, cod;

	if (actualiza_nodo() < 0)
		return -1;
	n = peticion_mapa(j, sizeof j, 1);
	cod = ts_pide("/machine/map", j, n, 1);
	if (cod != 200 && cod != 299) {	/* (299: «:status» en otra forma; el mapa dirá si vale) */
		if (cod > 0) {
			static char r[2048];
			ts_cuerpo_entero(r, sizeof r, 10 * h_hz);
			formatear(ts_error, sizeof ts_error, "el mapa de la red: %s", r);
		}
		return -1;
	}
	return 0;
}

/* ---- el mapa de la red ---- */
static int ip_de_cidr(struct jv v, u32 *ip, u32 *masc)
{
	char t[64], *b;
	int bits = 32, i;

	j_texto(v, t, sizeof t);
	for (b = t; *b && *b != '/'; b++) ;
	if (*b == '/') {
		*b++ = 0;
		bits = 0;
		while (*b >= '0' && *b <= '9')
			bits = bits * 10 + *b++ - '0';
	}
	if (!h_texto_a_ip(t, ip))
		return 0;
	*masc = 0;
	for (i = 0; i < bits && i < 32; i++)
		*masc |= 1u << ((i / 8) * 8 + 7 - i % 8);
	*ip &= *masc;
	return 1;
}

static int clave_de(struct jv v, const char *pref, u8 k[32])
{
	char t[100];
	int n = strlen(pref);

	if (j_texto(v, t, sizeof t) != n + 64 || memcmp(t, pref, n))
		return 0;
	return h_de_hex(t + n, k, 32);
}

static struct ts_par *par_de_id(long long id)
{
	int i;

	for (i = 0; i < MAX_PARES; i++)
		if (pares[i].usado && pares[i].id == id)
			return &pares[i];
	return 0;
}

static void quita_par(struct ts_par *p)
{
	if (p->wg)
		wg_quita_par(p->wg);
	memset(p, 0, sizeof *p);
}

static void derp_de(struct jv n, struct ts_par *p)
{
	struct jv v;

	if (j_campo(n, "HomeDERP", &v) && j_numero(v) > 0)
		p->derp = j_numero(v);
	else if (j_campo(n, "DERP", &v)) {
		char t[40], *c;
		j_texto(v, t, sizeof t);
		for (c = t; *c && *c != ':'; c++) ;
		if (*c == ':' && !memcmp(t, "127.3.3.40", 10))
			p->derp = j_numero((struct jv){ c + 1, strlen(c + 1) });
	}
}

static void pon_par(struct jv n)
{
	struct jv v, a;
	long long id;
	struct ts_par *p;
	u8 k[32];
	int i;

	if (!j_campo(n, "ID", &v))
		return;
	id = j_numero(v);
	if (!j_campo(n, "Key", &v) || !clave_de(v, "nodekey:", k))
		return;
	p = par_de_id(id);
	if (p && memcmp(p->clave, k, 32)) {
		quita_par(p);
		p = 0;
	}
	if (!p) {
		for (i = 0; i < MAX_PARES && !p; i++)
			if (!pares[i].usado)
				p = &pares[i];
		if (!p)
			return;
		memset(p, 0, sizeof *p);
		p->usado = 1;
		p->id = id;
		memcpy(p->clave, k, 32);
		p->en_linea = 1;
	}
	if (j_campo(n, "Name", &v))
		j_texto(v, p->nombre, sizeof p->nombre);
	if (j_ruta(n, "Hostinfo.Hostname", &v))
		j_texto(v, p->host, sizeof p->host);
	if (j_ruta(n, "Hostinfo.OS", &v))
		j_texto(v, p->os, sizeof p->os);
	if (j_campo(n, "Online", &v))
		p->en_linea = j_cierto(v);
	derp_de(n, p);
	if (!p->wg)
		p->wg = wg_nuevo_par(k);
	if (!p->wg)
		return;
	p->wg->dato = p;
	h_copia(p->wg->nombre, p->host[0] ? p->host : p->nombre, sizeof p->wg->nombre);
	if (j_campo(n, "AllowedIPs", &a) || j_campo(n, "Addresses", &a)) {
		p->wg->n_redes = 0;
		for (i = 0; j_elemento(a, i, &v); i++) {
			u32 ip, m;
			if (ip_de_cidr(v, &ip, &m) && p->wg->n_redes < WG_MAX_REDES) {
				p->wg->red[p->wg->n_redes] = ip;
				p->wg->masc[p->wg->n_redes++] = m;
			}
		}
	}
	if (j_campo(n, "Addresses", &a))
		for (i = 0; j_elemento(a, i, &v); i++) {
			u32 ip, m;
			if (ip_de_cidr(v, &ip, &m) && m == 0xFFFFFFFF) {
				p->ip = ip;
				break;
			}
		}
}

static int en_cidr_texto(const char *t, u32 *ip, u32 *m)
{
	struct jv v;
	char c[70];

	formatear(c, sizeof c, "\"%s\"", t);
	v.p = c, v.n = strlen(c);
	return ip_de_cidr(v, ip, m);
}

static void pon_filtro(struct jv f, int desde_cero)
{
	struct jv r, s, d, x, pr;
	int i, a, b, c;

	if (desde_cero)
		n_reglas = 0;
	hay_filtro = 1;
	for (i = 0; j_elemento(f, i, &r); i++) {
		u8 protos = 7;
		if (j_campo(r, "IPProto", &pr) && j_cuantos(pr) > 0) {
			protos = 0;
			for (c = 0; j_elemento(pr, c, &x); c++) {
				long long p = j_numero(x);
				if (p == 6)
					protos |= 1;
				else if (p == 17)
					protos |= 2;
				else if (p == 1)
					protos |= 4;
			}
		}
		if (!j_campo(r, "SrcIPs", &s) || !j_campo(r, "DstPorts", &d))
			continue;
		for (a = 0; j_elemento(s, a, &x); a++) {
			u32 sip = 0, sm = 0;
			char t[64];
			j_texto(x, t, sizeof t);
			if (!strcmp(t, "*"))
				sip = 0, sm = 0;
			else if (!en_cidr_texto(t, &sip, &sm))
				continue;
			for (b = 0; j_elemento(d, b, &x); b++) {
				struct jv ipv, po, v;
				u32 dip = 0, dm = 0;
				char u[64];
				if (n_reglas >= MAX_REGLAS)
					break;
				if (j_campo(x, "IP", &ipv)) {
					j_texto(ipv, u, sizeof u);
					if (strcmp(u, "*") && !en_cidr_texto(u, &dip, &dm))
						continue;
				}
				reglas[n_reglas].src = sip, reglas[n_reglas].src_m = sm;
				reglas[n_reglas].dst = dip, reglas[n_reglas].dst_m = dm;
				reglas[n_reglas].p0 = 0, reglas[n_reglas].p1 = 65535;
				if (j_campo(x, "Ports", &po)) {
					if (j_campo(po, "First", &v))
						reglas[n_reglas].p0 = j_numero(v);
					if (j_campo(po, "Last", &v))
						reglas[n_reglas].p1 = j_numero(v);
				}
				reglas[n_reglas].protos = protos;
				n_reglas++;
			}
		}
	}
}

static void pon_regiones(struct jv dm)
{
	struct jv rs, nm, r, nodos, n0, v;
	int i;

	if (!j_campo(dm, "Regions", &rs))
		return;
	ts_n_regiones = 0;
	for (i = 0; j_par(rs, i, &nm, &r) && ts_n_regiones < TS_MAX_REGIONES; i++) {
		struct ts_region *g = &ts_regiones[ts_n_regiones];
		int k;
		memset(g, 0, sizeof *g);
		if (j_campo(r, "RegionID", &v))
			g->id = j_numero(v);
		if (j_campo(r, "RegionCode", &v))
			j_texto(v, g->codigo, sizeof g->codigo);
		if (j_campo(r, "RegionName", &v))
			j_texto(v, g->nombre, sizeof g->nombre);
		if (!j_campo(r, "Nodes", &nodos))
			continue;
		for (k = 0; j_elemento(nodos, k, &n0); k++) {
			char ip[40];
			if (j_campo(n0, "STUNOnly", &v) && j_cierto(v))
				continue;
			if (j_campo(n0, "HostName", &v))
				j_texto(v, g->host, sizeof g->host);
			if (j_campo(n0, "IPv4", &v)) {
				j_texto(v, ip, sizeof ip);
				h_texto_a_ip(ip, &g->ip);
			}
			g->puerto = 443;
			if (j_campo(n0, "DERPPort", &v) && j_numero(v) > 0)
				g->puerto = j_numero(v);
			break;
		}
		if (g->id && g->host[0])
			ts_n_regiones++;
	}
	h_apunta("relevos DERP: %u regiones", ts_n_regiones);
}

/* la región de casa: la que antes conecta (o la que diga TS.INI) */
static void elige_casa(void)
{
	int i, mejor = 0;
	u32 mejor_t = 0xFFFFFFFF;

	if (casa_elegida && ts_region(derp_casa))
		return;
	i = derp_ini();
	if (i && ts_region(i)) {
		derp_casa = i;
		casa_elegida = 1;
		return;
	}
	for (i = 0; i < ts_n_regiones; i++) {
		struct ts_region *g = &ts_regiones[i];
		u32 t0, s;
		if (!g->ip)
			continue;
		t0 = h_tics();
		s = h_conectar(g->ip, g->puerto, h_hz);
		if (!ES_ERROR(s)) {
			u32 t = h_tics() - t0;
			h_cerrar_red(s);
			if (t < mejor_t)
				mejor_t = t, mejor = g->id;
		}
	}
	if (!mejor && ts_n_regiones)
		mejor = ts_regiones[0].id;
	derp_casa = mejor;
	casa_elegida = 1;
	{
		struct ts_region *g = ts_region(mejor);
		h_apunta("relevo de casa: derp-%u (%s)", mejor, g ? g->nombre : "?");
	}
}

static void pon_nombres(void)
{
	static char t[4096];
	char ip[16], c[96];
	int k = 0, i;

	for (i = 0; i < MAX_PARES; i++) {
		struct ts_par *p = &pares[i];
		int n;
		if (!p->usado || !p->ip)
			continue;
		h_ip_a_texto(p->ip, ip);
		h_copia(c, p->nombre, sizeof c);
		n = strlen(c);
		if (n && c[n - 1] == '.')
			c[n - 1] = 0;
		if (k + 120 < (int)sizeof t)
			k += formatear(t + k, sizeof t - k, "%s %s\n", c, ip);
		if (p->host[0] && k + 90 < (int)sizeof t)
			k += formatear(t + k, sizeof t - k, "%s %s\n", p->host, ip);
	}
	h_red(O_TUNEL_DNS, 0, 0, 0, t, k, t, 0);
}

static int tunel_puesto;
static u32 ip_puesta;

static void pon_tunel(void)
{
	u32 r[16];
	int n = 1, i, k;

	if (!mi_ip)
		return;
	r[0] = 0x00004064, r[1] = 0x0000C0FF;	/* 100.64.0.0/10 */
	for (i = 0; i < MAX_PARES && n < 8; i++) {
		struct ts_par *p = &pares[i];
		if (!p->usado || !p->wg)
			continue;
		for (k = 0; k < p->wg->n_redes && n < 8; k++)
			if ((p->wg->red[k] & r[1]) != r[0]) {
				r[2 * n] = p->wg->red[k];
				r[2 * n + 1] = p->wg->masc[k];
				n++;
			}
	}
	if (ES_ERROR(h_red(O_TUNEL, mi_ip, 0, 0, r, n * 8, r, 0)))
		h_apunta("RED.BIN no deja poner la interfaz del túnel (¿hay otro túnel, WG, en marcha?)");
	else {
		tunel_puesto = 1;
		ip_puesta = mi_ip;
	}
}

static void mapa(const char *t, u32 n)
{
	struct jv o, v, a, x;
	int i, cambios = 0;

	if (!j_valor(t, n, &o))
		return;
	if (j_campo(o, "Node", &v)) {
		if (j_campo(v, "Name", &x))
			j_texto(x, mi_nombre, sizeof mi_nombre);
		if (j_campo(v, "Addresses", &a))
			for (i = 0; j_elemento(a, i, &x); i++) {
				u32 ip, m;
				if (ip_de_cidr(x, &ip, &m) && m == 0xFFFFFFFF) {
					if (ip != mi_ip)
						cambios = 1;
					mi_ip = ip;
					break;
				}
			}
		{
			char *pto = mi_nombre;
			while (*pto && *pto != '.')
				pto++;
			if (*pto)
				h_copia(dominio, pto + 1, sizeof dominio);
		}
	}
	if (j_campo(o, "DERPMap", &v)) {
		pon_regiones(v);
		elige_casa();
		cambios = 1;
	}
	if (j_campo(o, "Peers", &a)) {
		for (i = 0; i < MAX_PARES; i++)
			if (pares[i].usado)
				quita_par(&pares[i]);
		for (i = 0; j_elemento(a, i, &x); i++)
			pon_par(x);
		cambios = 1;
	}
	if (j_campo(o, "PeersChanged", &a)) {
		for (i = 0; j_elemento(a, i, &x); i++)
			pon_par(x);
		cambios = 1;
	}
	if (j_campo(o, "PeersRemoved", &a)) {
		for (i = 0; j_elemento(a, i, &x); i++) {
			struct ts_par *p = par_de_id(j_numero(x));
			if (p)
				quita_par(p);
		}
		cambios = 1;
	}
	if (j_campo(o, "PeersChangedPatch", &a))
		for (i = 0; j_elemento(a, i, &x); i++) {
			struct jv id, w;
			struct ts_par *p;
			if (!j_campo(x, "NodeID", &id) || !(p = par_de_id(j_numero(id))))
				continue;
			if (j_campo(x, "Online", &w))
				p->en_linea = j_cierto(w);
			if (j_campo(x, "DERPRegion", &w) && j_numero(w) > 0)
				p->derp = j_numero(w);
		}
	if (j_campo(o, "OnlineChange", &a)) {
		struct jv nm;
		for (i = 0; j_par(a, i, &nm, &x); i++) {
			struct ts_par *p = par_de_id(j_numero(nm));
			if (p)
				p->en_linea = j_cierto(x);
		}
	}
	if (j_campo(o, "PacketFilter", &v) && !j_nulo(v)) {
		pon_filtro(v, 1);
		h_apunta("reglas de la red: %u", n_reglas);
	}
	/* (las de ahora: PacketFilters, por nombre; «*» a null las borra todas) */
	if (j_campo(o, "PacketFilters", &v)) {
		struct jv nm;
		int primero = 1;
		for (i = 0; j_par(v, i, &nm, &x); i++) {
			if (j_es(nm, "*") && j_nulo(x)) {
				n_reglas = 0;
				continue;
			}
			if (!j_nulo(x)) {
				pon_filtro(x, primero);
				primero = 0;
			}
		}
		h_apunta("reglas de la red: %u", n_reglas);
	}
	if (cambios) {
		if (!tunel_puesto || ip_puesta != mi_ip)
			pon_tunel();
		pon_nombres();
		estado = "conectado";
		{
			char ip[16];
			h_ip_a_texto(mi_ip, ip);
			h_apunta("en la red: %s, %s; %u equipos", mi_nombre, ip, (u32)({
				int c = 0, q;
				for (q = 0; q < MAX_PARES; q++)
					c += pares[q].usado;
				c;
			}));
		}
		escribe_estado();
	}
}

/* ---- el filtro de lo que entra ---- */
static void apunta_salida(const u8 *pq, u32 n)
{
	u32 hl = (pq[0] & 15) * 4, proto = pq[9], dst = ip_le(pq + 16), i;
	struct salida *s;

	if (n < hl + 4 && proto != 1)
		return;
	for (i = 0; i < MAX_SALIDAS; i++) {
		s = &salidas[i];
		if (s->ip == dst && s->proto == proto &&
		    (proto == 1 || (s->rpuerto == (pq[hl + 2] << 8 | pq[hl + 3]) && s->lpuerto == (pq[hl] << 8 | pq[hl + 1])))) {
			s->tic = h_tics();
			return;
		}
	}
	s = &salidas[sig_salida++ % MAX_SALIDAS];
	s->ip = dst, s->proto = proto, s->tic = h_tics();
	if (proto != 1) {
		s->lpuerto = pq[hl] << 8 | pq[hl + 1];
		s->rpuerto = pq[hl + 2] << 8 | pq[hl + 3];
	}
}

static int deja_entrar(const u8 *pq, u32 n)
{
	u32 hl = (pq[0] & 15) * 4, proto = pq[9], src = ip_le(pq + 12), dst = ip_le(pq + 16), i;
	u32 sp = 0, dp = 0;
	u8 bit = proto == 6 ? 1 : proto == 17 ? 2 : proto == 1 ? 4 : 0;

	if (!bit)
		return 0;
	if (proto != 1) {
		if (n < hl + 4)
			return 0;
		sp = pq[hl] << 8 | pq[hl + 1];
		dp = pq[hl + 2] << 8 | pq[hl + 3];
	}
	/* lo que contesta a lo que salió de aquí */
	for (i = 0; i < MAX_SALIDAS; i++) {
		struct salida *s = &salidas[i];
		if (s->ip == src && s->proto == proto && h_tics() - s->tic < 300 * h_hz && (proto == 1 || (s->rpuerto == sp && s->lpuerto == dp)))
			return 1;
	}
	if (proto == 1 && pq[hl] == 0 /* eco de vuelta */)
		return 0;
	for (i = 0; i < (u32)n_reglas; i++) {
		struct regla *r = &reglas[i];
		if (!(r->protos & bit))
			continue;
		if ((src & r->src_m) != r->src || (dst & r->dst_m) != r->dst)
			continue;
		if (proto == 1 || (dp >= r->p0 && dp <= r->p1))
			return 1;
	}
	return 0;
}

/* ---- el servicio ---- */
static int servicio(void)
{
	static u8 d[2048], sale[2048];
	static u8 mbuf[1 << 20];
	u32 n_m = 0, ult_mapa = 0, ult_tic = h_tics(), ult_estado = h_tics(), ult_ini = h_tics(), espera_rec = 5;
	int conectado = 0, mapa_pedido_casa = 0, filtrados = 0;

	sc(SC_ATENDER_CORTE, 1, 0, 0);
	lee_ajustes();
	if (claves() < 0)
		return 1;
	wg_inicia(ts_nodo);
	estado = "conectando";
	escribe_estado();
	h_apunta("Hispa-Pentium: Tailscale, con %s, como «%s»", ts_servidor, nombre_equipo);
	for (;;) {
		u32 ahora = h_tics();
		/* la conexión con el servidor de control (y, si se corta, otra vez) */
		if (!conectado) {
			ts_error[0] = 0;
			if (ts_clave_control() == 0 && ts_conecta() == 0 && registra() == 0) {
				casa_elegida = 0;
				derp_casa = derp_ini();
				if (pide_mapa() == 0) {
					h_apunta("control: pedido el mapa de la red");
					conectado = 1;
					mapa_pedido_casa = derp_casa;
					n_m = 0;
					espera_rec = 5;
				}
			}
			if (!conectado) {
				h_apunta("sin conexión con el servidor de control: %s (otra vez en %u s)", ts_error, espera_rec);
				estado = "error";
				escribe_estado();
				ts_desconecta();
				{
					u32 hasta = h_tics() + espera_rec * h_hz;
					while ((int)(hasta - h_tics()) > 0) {
						h_dormir(h_hz / 2);
						if (!encendido())
							goto apagar;
					}
				}
				if (espera_rec < 60)
					espera_rec *= 2;
				continue;
			}
		}
		/* el mapa que llega (mensajes de 4 octetos de largo, de menor a mayor, y el JSON) */
		/* (cada medio segundo: por TLS, mirar cuesta un par de tics) */
		if (ahora - ult_mapa >= h_hz / 2) {
			int r;
			ult_mapa = ahora;
			r = ts_lee_cuerpo(mbuf + n_m, sizeof mbuf - n_m, 0);
			if (r < 0) {
				h_apunta("el mapa de la red se ha cortado: %s", ts_error[0] ? ts_error : "el servidor ha cerrado");
				conectado = 0;
				ts_desconecta();
				continue;
			}
			n_m += r;
			while (n_m >= 4) {
				u32 l = mbuf[0] | mbuf[1] << 8 | mbuf[2] << 16 | (u32)mbuf[3] << 24;
				if (l > sizeof mbuf - 4) {
					h_apunta("un mensaje del mapa no cabe (%u octetos)", l);
					conectado = 0;
					ts_desconecta();
					break;
				}
				if (n_m < 4 + l)
					break;
				mapa((const char *)mbuf + 4, l);
				memmove(mbuf, mbuf + 4 + l, n_m - 4 - l);
				n_m -= 4 + l;
			}
			if (!conectado)
				continue;
		}
		/* el relevo de casa: conectado, y el servidor de control lo sabe */
		if (derp_casa) {
			if (!derp_region_conectada(derp_casa)) {
				struct ts_region *g = ts_region(derp_casa);
				static u32 ult_intento;
				if (g && (!ult_intento || h_tics() - ult_intento > 10 * h_hz)) {
					ult_intento = h_tics();
					derp_conecta(derp_casa, g->host, g->ip, g->puerto);
				}
			}
			if (mapa_pedido_casa != derp_casa) {
				/* (el mapa otra vez, ya con PreferredDERP: así los demás nos encuentran) */
				mapa_pedido_casa = derp_casa;
				pon_ini(1, ts_servidor, derp_casa);
				if (pide_mapa() < 0) {
					conectado = 0;
					continue;
				}
				n_m = 0;
			}
		}
		/* lo que llega por DERP */
		{
			u8 de[32];
			u32 n;
			int region, k;
			for (k = 0; k < 8 && derp_recibe(de, d, sizeof d, &n, &region); k++) {
				struct wg_par *quien;
				int m;
				if (n >= 6 && !memcmp(d, "TS\xf0\x9f\x92\xac", 6))
					continue;	/* (un mensaje «disco»: lo directo, todavía no) */
				m = wg_entra(d, n, 0, 0, sale, &quien);
				if (m > 0) {
					if (deja_entrar(sale, m))
						h_red(O_TUNEL_ENTRA, 0, 0, 0, sale, m, sale, 0);
					else if (filtrados++ < 20)
						h_apunta("tirado por las reglas de la red: un paquete de %s", quien ? quien->nombre : "?");
				}
			}
		}
		/* lo que sale por el túnel */
		if (tunel_puesto) {
			int k;
			for (k = 0; k < 8; k++) {
				u32 r = h_red(O_TUNEL_LEER, 0, 0, 0, 0, 0, d, sizeof d);
				if (ES_ERROR(r) || r == 0)
					break;
				apunta_salida(d, r);
				wg_sale(d, r);
			}
		}
		if (ahora - ult_tic >= h_hz / 10 + 1) {
			ult_tic = ahora;
			wg_tic();
		}
		if (ahora - ult_estado >= 5 * h_hz) {
			ult_estado = ahora;
			escribe_estado();
		}
		if (ahora - ult_ini >= 2 * h_hz) {
			ult_ini = ahora;
			if (!encendido())
				goto apagar;
		}
		if (!tunel_puesto)
			h_dormir(1);
	}
apagar:
	h_apunta("desconectado (TAILSCALE /DESCONECTAR)");
	ts_desconecta();
	derp_cierra_todas();
	h_red(O_TUNEL_QUITAR, 0, 0, 0, 0, 0, 0, 0);
	estado = "apagado";
	mi_ip = 0;
	escribe_estado();
	crypto_wipe(ts_maquina, 32);
	crypto_wipe(ts_nodo, 32);
	crypto_wipe(ts_disco, 32);
	crypto_wipe(wg_priv, 32);
	return 0;
}

/* ---- la orden ---- */
static int lee_estado(char *t, int cabe)
{
	char r[100];
	int n;

	ruta_de(r, "ESTADO.TXT");
	n = h_lee_archivo(r, t, cabe - 1);
	if (n < 0)
		n = 0;
	t[n] = 0;
	return n;
}

/* un texto y espacios hasta «ancho» (los textos de la red van en UTF-8: a la página 437) */
static void columna(const char *t, int ancho)
{
	char d[80];
	int k = 0;
	u32 u = 0, falta = 0;

	for (; *t && k < 79; t++) {
		u8 c = *t;
		if (falta) {
			u = u << 6 | (c & 63);
			if (--falta == 0)
				d[k++] = h_437_de_unicode(u);
			continue;
		}
		if (c >= 0xC0 && c < 0xE0)
			u = c & 31, falta = 1;
		else if (c >= 0xE0 && c < 0xF0)
			u = c & 15, falta = 2;
		else if (c >= 0xF0)
			u = c & 7, falta = 3;
		else
			d[k++] = c;
	}
	while (k < ancho && k < 79)
		d[k++] = ' ';
	d[k++] = ' ';
	d[k] = 0;
	h_escribe(d);
}

static void enseña(void)
{
	static char t[12288];
	char v[320];
	int i = 0;

	if (!encendido()) {
		h_escribe("Tailscale: desconectado (TAILSCALE /CONECTAR para conectarse).\n");
		return;
	}
	lee_estado(t, sizeof t);
	h_ini(t, "estado", v, sizeof v);
	if (!strcmp(v, "alta")) {
		h_ini(t, "alta", v, sizeof v);
		h_escribef("Falta dar de alta este equipo en la red: abra esta dirección en otro equipo\n"
			   "(el móvil, otro ordenador) con su cuenta de Tailscale, y apruébelo:\n\n  %s\n\n", v);
		return;
	}
	if (!strcmp(v, "error")) {
		h_ini(t, "error", v, sizeof v);
		h_escribef("Tailscale: sin conexión con el servidor de control (%s); se reintenta solo.\n", v);
		return;
	}
	if (strcmp(v, "conectado")) {
		h_escribe("Tailscale: conectando...\n");
		return;
	}
	{
		char nom[160], ip[20], us[100], dp[80];
		h_ini(t, "nombre", nom, sizeof nom);
		h_ini(t, "ip", ip, sizeof ip);
		if (!h_ini(t, "usuario", us, sizeof us))
			us[0] = 0;
		if (!h_ini(t, "derp", dp, sizeof dp))
			dp[0] = 0;
		h_escribef("Este equipo: %s  %s", ip, nom);
		if (us[0])
			h_escribef("  (de %s)", us);
		h_escribe("\n");
		if (dp[0])
			h_escribef("Relevo DERP de casa: %s\n", dp);
	}
	h_escribe("\nLos equipos de la red:\n");
	while (t[i]) {
		if (!memcmp(t + i, "equipo=", 7)) {
			char l[300], *c[6];
			int k = 0, j = 0;
			i += 7;
			while (t[i] && t[i] != '\r' && t[i] != '\n' && k < 299)
				l[k++] = t[i++];
			l[k] = 0;
			c[0] = l;
			for (k = 0; l[k] && j < 5; k++)
				if (l[k] == '|')
					l[k] = 0, c[++j] = l + k + 1;
			if (j >= 4) {
				h_escribe("  ");
				columna(c[1], 16);
				columna(c[0], 24);
				columna(c[2], 15);
				h_escribef("%s\n", c[3]);
			}
			continue;
		}
		while (t[i] && t[i] != '\n')
			i++;
		if (t[i])
			i++;
	}
}

/* ---- la ventana (Configuración > Red y Tailscale; el icono de la bandeja) ---- */
#define V_ANCHO 560
#define V_ALTO 420
#define FONDO 0xF4F8FE
#define TINTA 0x1A2A44
#define SUAVE 0x5A6E8C
struct boton { int x, y, an, al; const char *t; int accion; };
static struct boton botones[4];
static int n_botones;

static void texto_437(u32 v, int x, int y, const char *t, u32 tinta, u32 fondo, int max)
{
	char d[120];
	int k = 0;
	u32 u = 0, falta = 0;

	for (; *t && k < max && k < 119; t++) {
		u8 c = *t;
		if (falta) {
			u = u << 6 | (c & 63);
			if (--falta == 0)
				d[k++] = h_437_de_unicode(u);
			continue;
		}
		if (c >= 0xC0 && c < 0xE0)
			u = c & 31, falta = 1;
		else if (c >= 0xE0 && c < 0xF0)
			u = c & 15, falta = 2;
		else if (c >= 0xF0)
			u = c & 7, falta = 3;
		else
			d[k++] = c;
	}
	if (k)
		h_texto_fondo(v, x, y, d, k, tinta, fondo);
}

static void boton(u32 v, int x, int y, int an, const char *t, int accion, u32 color)
{
	int n = strlen(t);

	h_rectangulo(v, x, y, an, 26, 0x2468C8);
	h_rectangulo(v, x + 1, y + 1, an - 2, 24, color);
	h_texto_fondo(v, x + (an - n * 8) / 2, y + 5, t, n, 0xFFFFFF, color);
	if (n_botones < 4)
		botones[n_botones++] = (struct boton){ x, y, an, 26, t, accion };
}

static int sshd_encendido(void)
{
	char r[100], t[300], v[16];
	int n;

	h_ruta(r, sizeof r, "SSH\\SSHD.INI");
	n = h_lee_archivo(r, t, sizeof t - 1);
	if (n <= 0)
		return 0;
	t[n] = 0;
	return h_ini(t, "encendido", v, sizeof v) && v[0] == '1';
}

static void lanza_y_espera(const char *prog, const char *args)
{
	char r[40];
	u32 t;

	formatear(r, sizeof r, "%c:\\HISPA\\%s", h_sistema, prog);
	t = sc(SC_LANZAR, (u32)r, (u32)args, 0);
	if (ES_ERROR(t)) {
		formatear(r, sizeof r, "%c:\\%s", h_sistema, prog);
		t = sc(SC_LANZAR, (u32)r, (u32)args, 0);
	}
	if (!ES_ERROR(t))
		sc(SC_ESPERAR, t, 0, 0);
}

static void pinta_ventana(u32 v)
{
	static char t[12288];
	char e[40], l[400];
	int y, i = 0, filas = 0, enc = encendido();

	n_botones = 0;
	lee_estado(t, sizeof t);
	if (!h_ini(t, "estado", e, sizeof e) || !enc)
		h_copia(e, "apagado", sizeof e);
	h_rectangulo(v, 0, 0, V_ANCHO, V_ALTO, FONDO);
	h_rectangulo(v, 0, 0, V_ANCHO, 40, 0x2F74D0);
	/* (el dibujo de Tailscale: nueve puntos) */
	for (i = 0; i < 9; i++)
		h_rectangulo(v, 14 + (i % 3) * 7, 10 + (i / 3) * 7, 5, 5, (i == 3 || i == 4 || i == 5 || i == 7) ? 0xFFFFFF : 0x9AB8E8);
	h_texto_fondo(v, 44, 12, "Tailscale: la red privada de sus equipos", 40, 0xFFFFFF, 0x2F74D0);
	y = 52;
	if (!strcmp(e, "conectado")) {
		char nom[160], ip[24];
		h_ini(t, "nombre", nom, sizeof nom);
		h_ini(t, "ip", ip, sizeof ip);
		h_rectangulo(v, 16, y + 3, 10, 10, 0x2ECC40);
		h_texto_fondo(v, 32, y, "Conectado", 9, TINTA, FONDO);
		formatear(l, sizeof l, "Este equipo: %s", ip);
		h_texto_fondo(v, 32, y + 20, l, strlen(l), TINTA, FONDO);
		texto_437(v, 32, y + 40, nom, SUAVE, FONDO, 60);
		boton(v, V_ANCHO - 150, y, 134, "Desconectar", 1, 0xC0504D);
	} else if (!strcmp(e, "alta")) {
		char u[300];
		h_ini(t, "alta", u, sizeof u);
		h_rectangulo(v, 16, y + 3, 10, 10, 0xFFB000);
		h_texto_fondo(v, 32, y, "Falta dar de alta este equipo en la red:", 40, TINTA, FONDO);
		h_texto_fondo(v, 32, y + 20, "abra en otro equipo (con su cuenta) y apruebe:", 47, SUAVE, FONDO);
		texto_437(v, 32, y + 40, u, 0x2468C8, FONDO, 64);
		if (strlen(u) > 64)
			texto_437(v, 32, y + 58, u + 64, 0x2468C8, FONDO, 64);
		boton(v, V_ANCHO - 150, y, 134, "Desconectar", 1, 0xC0504D);
	} else if (enc) {
		char er[200];
		h_rectangulo(v, 16, y + 3, 10, 10, 0xFFB000);
		h_texto_fondo(v, 32, y, "Conectando...", 13, TINTA, FONDO);
		if (h_ini(t, "error", er, sizeof er))
			texto_437(v, 32, y + 20, er, 0xC0504D, FONDO, 64);
		boton(v, V_ANCHO - 150, y, 134, "Desconectar", 1, 0xC0504D);
	} else {
		h_rectangulo(v, 16, y + 3, 10, 10, 0x9AA8B8);
		h_texto_fondo(v, 32, y, "Desconectado", 12, TINTA, FONDO);
		texto_437(v, 32, y + 20, ts_servidor, SUAVE, FONDO, 60);
		boton(v, V_ANCHO - 150, y, 134, "Conectar", 2, 0x3FA24C);
	}
	y = 136;
	h_rectangulo(v, 16, y, V_ANCHO - 32, 1, 0xA9C6EC);
	h_texto_fondo(v, 16, y + 8, "Los equipos de la red", 21, TINTA, FONDO);
	y += 30;
	for (i = 0; t[i] && filas < 10; ) {
		if (!memcmp(t + i, "equipo=", 7)) {
			char q[300], *c[6];
			int k = 0, j = 0;
			i += 7;
			while (t[i] && t[i] != '\r' && t[i] != '\n' && k < 299)
				q[k++] = t[i++];
			q[k] = 0;
			c[0] = q;
			for (k = 0; q[k] && j < 5; k++)
				if (q[k] == '|')
					q[k] = 0, c[++j] = q + k + 1;
			if (j >= 3) {
				int linea = !strcmp(c[2], "en línea") || !memcmp(c[2], "en l", 4);
				h_rectangulo(v, 20, y + 4, 8, 8, linea ? 0x2ECC40 : 0x9AA8B8);
				texto_437(v, 36, y, c[0], TINTA, FONDO, 22);
				h_texto_fondo(v, 220, y, c[1], strlen(c[1]), TINTA, FONDO);
				texto_437(v, 360, y, linea ? "en línea" : "desconectado", SUAVE, FONDO, 14);
				if (j >= 3)
					texto_437(v, 480, y, c[3], SUAVE, FONDO, 9);
				y += 20;
				filas++;
			}
			continue;
		}
		while (t[i] && t[i] != '\n')
			i++;
		if (t[i])
			i++;
	}
	if (!filas)
		h_texto_fondo(v, 36, y, "(ninguno todavía)", 17, SUAVE, FONDO);
	/* el servidor SSH */
	y = V_ALTO - 50;
	h_rectangulo(v, 16, y - 10, V_ANCHO - 32, 1, 0xA9C6EC);
	if (sshd_encendido()) {
		h_texto_fondo(v, 16, y + 5, "Servidor SSH: encendido (puerto 22)", 35, TINTA, FONDO);
		boton(v, V_ANCHO - 150, y, 134, "Apagar SSH", 3, 0xC0504D);
	} else {
		h_texto_fondo(v, 16, y + 5, "Servidor SSH: apagado", 21, TINTA, FONDO);
		boton(v, V_ANCHO - 150, y, 134, "Encender SSH", 4, 0x3FA24C);
	}
}

static int ventana_ts(void)
{
	u32 v = h_ventana("Red y Tailscale", V_ANCHO, V_ALTO), ult = h_tics();
	struct suceso e;

	if (ES_ERROR(v)) {
		h_escribe("La ventana de Tailscale necesita el escritorio.\n");
		return 1;
	}
	pinta_ventana(v);
	for (;;) {
		if (h_suceso(v, &e, 0)) {
			if (e.clase == EV_CERRAR || (e.clase == EV_TECLA && e.a == 27))
				break;
			if (e.clase == EV_RATON_BAJA) {
				int i;
				for (i = 0; i < n_botones; i++) {
					struct boton *b = &botones[i];
					if ((int)e.a >= b->x && (int)e.a < b->x + b->an && (int)e.b >= b->y && (int)e.b < b->y + b->al) {
						if (b->accion == 1)
							pon_ini(0, ts_servidor, derp_ini());
						else if (b->accion == 2) {
							char r[40];
							u32 t;
							pon_ini(1, ts_servidor, derp_ini());
							formatear(r, sizeof r, "%c:\\HISPA\\TAILSCAL.BIN", h_sistema);
							t = sc(SC_LANZAR_SUELTO, (u32)r, (u32)"/SERVICIO", 0);
							if (ES_ERROR(t)) {
								formatear(r, sizeof r, "%c:\\TAILSCAL.BIN", h_sistema);
								sc(SC_LANZAR_SUELTO, (u32)r, (u32)"/SERVICIO", 0);
							}
						} else if (b->accion == 3)
							lanza_y_espera("SSHD.BIN", "/APAGAR");
						else if (b->accion == 4)
							lanza_y_espera("SSHD.BIN", "/ENCENDER");
						pinta_ventana(v);
						break;
					}
				}
			}
			continue;
		}
		if (h_tics() - ult >= 2 * h_hz) {
			ult = h_tics();
			pinta_ventana(v);
		}
		h_dormir(h_hz / 10);
	}
	sc(SC_CERRAR_VENTANA, v, 0, 0);
	return 0;
}

static void ayuda(void)
{
	h_escribe("TAILSCALE: este equipo en su red de Tailscale.\n"
		  "  TAILSCALE                  cómo está y los equipos de la red\n"
		  "  TAILSCALE /CONECTAR [url]  se conecta (url: otro servidor de control, como Headscale)\n"
		  "  TAILSCALE /DESCONECTAR     se desconecta\n"
		  "  TAILSCALE /OLVIDAR         olvida las claves (la próxima vez, un equipo nuevo)\n"
		  "  TAILSCALE /IP              la IP 100.x de este equipo\n");
}

int principal(void)
{
	char a[300], r[100];

	h_argumentos(a, sizeof a);
	h_inicia("tailscale");
	h_ruta(carpeta, sizeof carpeta, "TS");
	h_carpeta(carpeta);
	if (h_empieza(a, "/SERVICIO") || (h_empieza(a, "/ARRANQUE") && encendido()))
		return servicio();
	if (h_empieza(a, "/ARRANQUE"))
		return 0;	/* (al arrancar el escritorio: solo si se dejó conectado) */
	lee_ajustes();
	if (h_empieza(a, "/VENTANA"))
		return ventana_ts();
	if (h_empieza(a, "/CONECTAR")) {
		const char *u = a + 9;
		u32 t;
		int i;
		while (*u == ' ')
			u++;
		if (*u)
			h_copia(ts_servidor, u, sizeof ts_servidor);
		/* (las direcciones, en minúsculas: el intérprete las puede dar en mayúsculas) */
		for (i = 0; ts_servidor[i]; i++)
			if (ts_servidor[i] >= 'A' && ts_servidor[i] <= 'Z')
				ts_servidor[i] += 32;
		pon_ini(1, ts_servidor, *u ? 0 : derp_ini());
		formatear(r, sizeof r, "%c:\\HISPA\\TAILSCAL.BIN", h_sistema);
		t = sc(SC_LANZAR_SUELTO, (u32)r, (u32)"/SERVICIO", 0);
		if (ES_ERROR(t)) {
			formatear(r, sizeof r, "%c:\\TAILSCAL.BIN", h_sistema);
			t = sc(SC_LANZAR_SUELTO, (u32)r, (u32)"/SERVICIO", 0);
		}
		if (ES_ERROR(t)) {
			h_escribe("No se puede poner en marcha TAILSCAL.BIN.\n");
			return 1;
		}
		h_escribef("Conectando con %s...\n", ts_servidor);
		/* hasta 40 s: la dirección de alta, o que ya está */
		{
			static char t2[12288];
			char v[320];
			int k;
			for (k = 0; k < 80; k++) {
				h_dormir(h_hz / 2);
				lee_estado(t2, sizeof t2);
				h_ini(t2, "estado", v, sizeof v);
				if (!strcmp(v, "alta") || !strcmp(v, "conectado"))
					break;
				if (sc(SC_HUBO_CORTE, 0, 0, 0) == 1)
					break;
			}
		}
		enseña();
		return 0;
	}
	if (h_empieza(a, "/DESCONECTAR")) {
		pon_ini(0, ts_servidor, derp_ini());
		h_escribe("Tailscale: desconectado (en unos segundos).\n");
		return 0;
	}
	if (h_empieza(a, "/OLVIDAR")) {
		if (encendido()) {
			h_escribe("Primero TAILSCALE /DESCONECTAR.\n");
			return 1;
		}
		ruta_de(r, "CLAVES.TS");
		sc(SC_BORRAR, (u32)r, 0, 0);
		h_escribe("Claves olvidadas: la próxima vez será un equipo nuevo en la red.\n");
		return 0;
	}
	if (h_empieza(a, "/IP")) {
		static char t[12288];
		char v[40];
		lee_estado(t, sizeof t);
		if (encendido() && h_ini(t, "ip", v, sizeof v) && v[0])
			h_escribef("%s\n", v);
		else {
			h_escribe("(sin IP de Tailscale)\n");
			return 1;
		}
		return 0;
	}
	if (a[0]) {
		ayuda();
		return 0;
	}
	enseña();
	return 0;
}
