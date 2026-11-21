/* control.c - la conversación con el servidor de control de Tailscale (o
 * Headscale): la clave del servidor (GET /key), el protocolo «ts2021» (una
 * petición HTTP con Upgrade: tailscale-control-protocol que lleva el primer
 * mensaje de Noise_IK_25519_ChaChaPoly_BLAKE2s en X-Tailscale-Handshake; la
 * respuesta 101 y, después, registros cifrados de hasta 4096 octetos) y,
 * dentro, HTTP/2 (RFC 9113) con lo justo: un pedido cada vez, las cabeceras
 * en HPACK sin tablas al mandar, y de lo que llega solo el «:status 200».
 * Escrito a partir del código de Tailscale (BSD-3) como referencia
 * (control/controlbase, control/controlhttp, control/ts2021), no copiado.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ts.h"
#include "cripto.h"
#include "monocypher.h"

char ts_servidor[200] = "https://controlplane.tailscale.com";
u8 ts_maquina[32], ts_maquina_pub[32], ts_nodo[32], ts_nodo_pub[32], ts_disco[32], ts_disco_pub[32];
u8 ts_control_pub[32];
char ts_error[200];
u32 ts_espera_cabeceras = 120;
int ts_depura = 0;

static char host[128];
static u32 puerto, seguro, sock = 0xFFFFFFFF;
static int conectado;

/* ---- la dirección del servidor ---- */
static int parte_url(void)
{
	const char *p = ts_servidor;
	int k = 0;

	seguro = 1;
	if (h_empieza(p, "https://"))
		p += 8;
	else if (h_empieza(p, "http://"))
		p += 7, seguro = 0;
	while (*p && *p != ':' && *p != '/' && k < (int)sizeof host - 1)
		host[k++] = *p++;
	host[k] = 0;
	puerto = seguro ? 443 : 80;
	if (*p == ':') {
		puerto = 0;
		for (p++; *p >= '0' && *p <= '9'; p++)
			puerto = puerto * 10 + *p - '0';
	}
	return host[0] ? 0 : -1;
}

static u32 abre(void)
{
	u32 ip, r, s;

	r = h_resolver(host, &ip);
	if (ES_ERROR(r)) {
		formatear(ts_error, sizeof ts_error, "%s: %s", host, h_texto_error_red(r));
		return 0xFFFFFFFF;
	}
	if (seguro)
		s = h_conectar_seguro(host, ip, puerto, 15 * h_hz);
	else
		s = h_conectar(ip, puerto, 15 * h_hz);
	if (ES_ERROR(s)) {
		formatear(ts_error, sizeof ts_error, "%s:%u: %s", host, puerto, h_texto_error_red(s));
		return 0xFFFFFFFF;
	}
	return s;
}

/* lo que llega hasta el final de las cabeceras HTTP/1.1; lo que sobra queda en «resto» */
static u8 resto[8192];
static u32 n_resto;

static int cabeceras_http(u32 s, char *d, u32 cabe, u32 espera_s)
{
	u32 n = 0, limite = h_tics() + espera_s * h_hz, i;

	n_resto = 0;
	for (;;) {
		u32 r;
		for (i = 3; i < n; i++)
			if (d[i - 3] == '\r' && d[i - 2] == '\n' && d[i - 1] == '\r' && d[i] == '\n') {
				memcpy(resto, d + i + 1, n - i - 1);
				n_resto = n - i - 1;
				d[i + 1] = 0;
				return i + 1;
			}
		if (n + 1 >= cabe || (int)(limite - h_tics()) <= 0)
			return -1;
		r = h_recibir(s, d + n, cabe - 1 - n, 50);
		if (r == TIEMPO)
			continue;
		if (ES_ERROR(r) || r == 0)
			return -1;
		n += r;
	}
}

static int codigo_http(const char *c)
{
	while (*c && *c != ' ')
		c++;
	return (c[1] - '0') * 100 + (c[2] - '0') * 10 + (c[3] - '0');
}

/* GET /key?v=...: {"publicKey":"mkey:..."} */
int ts_clave_control(void)
{
	static char t[8192];
	char pedido[400];
	u32 s, n = 0;
	int k, cod;
	struct jv j, v;
	char mk[80];

	if (parte_url() < 0) {
		formatear(ts_error, sizeof ts_error, "la dirección del servidor de control no vale: %s", ts_servidor);
		return -1;
	}
	s = abre();
	if (s == 0xFFFFFFFF)
		return -1;
	k = formatear(pedido, sizeof pedido, "GET /key?v=%u HTTP/1.1\r\nHost: %s\r\nUser-Agent: HispaPentium-Tailscale/0.1\r\nConnection: close\r\n\r\n", TS_CAPVER, host);
	h_enviar(s, pedido, k);
	k = cabeceras_http(s, t, sizeof t, 20);
	if (k < 0) {
		h_cerrar_red(s);
		formatear(ts_error, sizeof ts_error, "el servidor de control no contesta a /key");
		return -1;
	}
	cod = codigo_http(t);
	memcpy(t, resto, n_resto);
	n = n_resto;
	for (;;) {
		u32 r;
		if (n >= sizeof t - 1)
			break;
		t[n] = 0;
		/* (un objeto JSON entero: ya está) */
		{
			struct jv x;
			char *p = t;
			while (*p && *p != '{')
				p++;
			if (*p && j_valor(p, strlen(p), &x))
				break;
		}
		r = h_recibir(s, t + n, sizeof t - 1 - n, 3 * h_hz);
		if (ES_ERROR(r) || r == 0)
			break;
		n += r;
	}
	t[n] = 0;
	h_cerrar_red(s);
	if (cod != 200) {
		formatear(ts_error, sizeof ts_error, "/key: el servidor de control contesta %u", cod);
		return -1;
	}
	/* (puede venir en trozos: se busca la llave en el texto, venga como venga) */
	{
		char *p = t;
		while (*p && *p != '{')
			p++;
		if (!j_valor(p, strlen(p), &j) || !j_campo(j, "publicKey", &v) || j_texto(v, mk, sizeof mk) != 69 ||
		    memcmp(mk, "mkey:", 5) || !h_de_hex(mk + 5, ts_control_pub, 32)) {
			formatear(ts_error, sizeof ts_error, "/key: no viene la clave del servidor de control");
			return -1;
		}
	}
	return 0;
}

/* ================== Noise (controlbase) ================== */
static u8 n_h[32], n_ck[32], k_env[32], k_rec[32];
static u64 c_env, c_rec;

static void mix_hash(const void *d, u32 n)
{
	blake2s_ctx c;

	blake2s_inicia(&c, 32, 0, 0);
	blake2s_mas(&c, n_h, 32);
	blake2s_mas(&c, d, n);
	blake2s_fin(&c, n_h);
}

static void mix_dh(const u8 priv[32], const u8 pub[32], u8 k[32])
{
	u8 dh[32];

	crypto_x25519(dh, priv, pub);
	hkdf_blake2s(n_ck, dh, 32, n_ck, k, 0);
	crypto_wipe(dh, 32);
}

static void nonce_be(u8 n[12], u64 c)
{
	int i;

	memset(n, 0, 4);
	for (i = 0; i < 8; i++)
		n[4 + i] = c >> (56 - 8 * i);
}

static void sella(u8 *sale, const u8 k[32], u64 cont, const u8 *d, u32 n, const u8 *ad, u32 nad)
{
	crypto_aead_ctx c;
	u8 nn[12];

	nonce_be(nn, cont);
	crypto_aead_init_ietf(&c, k, nn);
	crypto_aead_write(&c, sale, sale + n, ad, nad, d, n);
	crypto_wipe(&c, sizeof c);
}

static int abre_sello(u8 *sale, const u8 k[32], u64 cont, const u8 *d, u32 n, const u8 *ad, u32 nad)
{
	crypto_aead_ctx c;
	u8 nn[12];
	int r;

	if (n < 16)
		return -1;
	nonce_be(nn, cont);
	crypto_aead_init_ietf(&c, k, nn);
	r = crypto_aead_read(&c, sale, d + n - 16, ad, nad, d, n - 16);
	crypto_wipe(&c, sizeof c);
	return r;
}

/* lo que llega por la conexión, sin descifrar */
static u8 crudo[16384];
static u32 n_crudo;

static int llena_crudo(u32 espera)
{
	u32 r;

	if (n_crudo >= sizeof crudo)
		return -1;
	r = h_recibir(sock, crudo + n_crudo, sizeof crudo - n_crudo, espera);
	if (r == TIEMPO)
		return 0;
	if (ES_ERROR(r) || r == 0) {
		formatear(ts_error, sizeof ts_error, "la conexión con el servidor de control se ha cortado");
		conectado = 0;
		return -1;
	}
	n_crudo += r;
	return 1;
}

/* el texto en claro que ha llegado (los registros ya abiertos) */
static u8 claro[65536];
static u32 n_claro;

/* ¿hay en «claro» algo entero que mirar (un frame de HTTP/2, o lo temprano)? */
static int temprano;
static int claro_entero(void)
{
	if (n_claro < 9)
		return 0;
	if (temprano && !memcmp(claro, "\xff\xff\xffTS", 5))
		return n_claro >= 9 + ((u32)claro[5] << 24 | claro[6] << 16 | claro[7] << 8 | claro[8]);
	return n_claro >= 9 + (claro[0] << 16 | claro[1] << 8 | claro[2]);
}

static int registros(u32 espera)
{
	int r;

	for (;;) {
		while (n_crudo >= 3) {
			u32 largo = crudo[1] << 8 | crudo[2];
			if (crudo[0] != 4) {
				formatear(ts_error, sizeof ts_error, "el servidor de control manda algo que no es un registro (%u)", crudo[0]);
				conectado = 0;
				return -1;
			}
			if (largo > 4093 || largo < 16) {
				conectado = 0;
				return -1;
			}
			if (n_crudo < 3 + largo)
				break;
			if (n_claro + largo > sizeof claro)
				return 1;	/* (que se lea antes lo que hay) */
			if (abre_sello(claro + n_claro, k_rec, c_rec, crudo + 3, largo, 0, 0)) {
				formatear(ts_error, sizeof ts_error, "un registro del servidor de control no pasa la comprobación");
				conectado = 0;
				return -1;
			}
			c_rec++;
			n_claro += largo - 16;
			memmove(crudo, crudo + 3 + largo, n_crudo - 3 - largo);
			n_crudo -= 3 + largo;
			espera = 0;
		}
		if (claro_entero())
			return 1;
		r = llena_crudo(espera);
		if (r <= 0)
			return r;
		espera = 0;
	}
}

static int manda_claro(const u8 *d, u32 n)
{
	static u8 m[4096];

	while (n) {
		u32 k = n > 4077 ? 4077 : n;
		m[0] = 4;
		m[1] = (k + 16) >> 8;
		m[2] = (k + 16);
		sella(m + 3, k_env, c_env, d, k, 0, 0);
		c_env++;
		if (ES_ERROR(h_enviar(sock, m, 3 + k + 16))) {
			conectado = 0;
			return -1;
		}
		d += k, n -= k;
	}
	return 0;
}

/* ================== HTTP/2 (lo justo) ================== */
#define H2_DATA 0
#define H2_HEADERS 1
#define H2_RST 3
#define H2_SETTINGS 4
#define H2_PING 6
#define H2_GOAWAY 7
#define H2_WINDOW 8
#define H2_CONT 9
static u32 flujo, flujo_sig = 1;
static int estado_flujo;		/* 0 nada; 1 esperando las cabeceras; 2 llega el cuerpo; 3 acabado */
static int codigo_flujo;
static u8 *cuerpo;			/* lo que llega del cuerpo, hasta que se lea */
static u32 n_cuerpo, cabe_cuerpo;
static u32 sin_avisar;			/* lo leído sin decir WINDOW_UPDATE */
/* (temprano: aún puede venir lo «temprano» del servidor, antes de HTTP/2; arriba) */

static int frame(u32 tipo, u32 banderas, u32 id, const u8 *d, u32 n)
{
	static u8 f[16384 + 9];

	f[0] = n >> 16, f[1] = n >> 8, f[2] = n;
	f[3] = tipo, f[4] = banderas;
	f[5] = id >> 24 & 0x7F, f[6] = id >> 16, f[7] = id >> 8, f[8] = id;
	memcpy(f + 9, d, n);
	return manda_claro(f, 9 + n);
}

static void ventana(u32 id, u32 n)
{
	u8 v[4] = { n >> 24 & 0x7F, n >> 16, n >> 8, n };

	frame(H2_WINDOW, 0, id, v, 4);
}

/* los frames que hay en «claro»: 1 si se ha hecho algo, 0 si falta, -1 */
static int frames(void)
{
	int algo = 0;

	if (temprano && n_claro >= 9) {
		temprano = 0;
		if (!memcmp(claro, "\xff\xff\xffTS", 5)) {
			u32 l = (u32)claro[5] << 24 | claro[6] << 16 | claro[7] << 8 | claro[8];
			if (9 + l > n_claro) {
				temprano = 1;
				return 0;
			}
			h_apunta("control: lo temprano del servidor (%u octetos)", l);
			memmove(claro, claro + 9 + l, n_claro - 9 - l);
			n_claro -= 9 + l;
		}
	}
	if (temprano)
		return 0;
	while (n_claro >= 9) {
		u32 l = claro[0] << 16 | claro[1] << 8 | claro[2], tipo = claro[3], band = claro[4];
		u32 id = (claro[5] & 0x7F) << 24 | claro[6] << 16 | claro[7] << 8 | claro[8];
		u8 *p = claro + 9;
		if (9 + l > sizeof claro) {
			conectado = 0;
			return -1;
		}
		if (n_claro < 9 + l)
			break;
		algo = 1;
		{
			static u32 vistos;
			if (ts_depura && vistos++ < 30)
				h_apunta("h2: frame %u, banderas %x, flujo %u, %u octetos (esperando el %u, estado %u)", tipo, band, id, l, flujo, estado_flujo);
		}
		switch (tipo) {
		case H2_SETTINGS:
			if (!(band & 1))
				frame(H2_SETTINGS, 1, 0, 0, 0);
			break;
		case H2_PING:
			if (!(band & 1))
				frame(H2_PING, 1, 0, p, 8);
			break;
		case H2_GOAWAY:
			h_apunta("control: GOAWAY del servidor");
			conectado = 0;
			break;
		case H2_RST:
			if (id == flujo) {
				estado_flujo = 3;
				if (!codigo_flujo)
					codigo_flujo = -1;
			}
			break;
		case H2_HEADERS:
		case H2_CONT:
			if (id == flujo && estado_flujo == 1) {
				u32 i = 0;
				if (band & 8)
					i = 1;		/* (relleno) */
				if (band & 0x20)
					i += 5;		/* (prioridad) */
				/* «:status» es casi siempre lo primero: 0x88 es el 200 de la tabla */
				if (!codigo_flujo)
					codigo_flujo = (i < l && p[i] == 0x88) ? 200 : 400;
				if (band & 4)
					estado_flujo = 2;
				if (band & 1)
					estado_flujo = 3;
			}
			break;
		case H2_DATA:
			if (id == flujo) {
				u32 i = 0, k = l;
				if (band & 8) {
					i = 1;
					k = l - 1 - p[0];
				}
				if (n_cuerpo + k > cabe_cuerpo) {
					formatear(ts_error, sizeof ts_error, "lo que manda el servidor de control no cabe");
					conectado = 0;
					return -1;
				}
				memcpy(cuerpo + n_cuerpo, p + i, k);
				n_cuerpo += k;
				if (band & 1)
					estado_flujo = 3;
			}
			sin_avisar += l;
			if (sin_avisar > 256 * 1024) {
				ventana(0, sin_avisar);
				if (id == flujo && estado_flujo != 3)
					ventana(flujo, sin_avisar);
				sin_avisar = 0;
			}
			break;
		}
		memmove(claro, claro + 9 + l, n_claro - 9 - l);
		n_claro -= 9 + l;
	}
	return algo;
}

static int bombea(u32 espera)
{
	int r = registros(espera);

	if (r < 0)
		return -1;
	if (r > 0)
		return frames() < 0 ? -1 : 1;
	return 0;
}

int ts_conectado(void) { return conectado; }

void ts_desconecta(void)
{
	if (sock != 0xFFFFFFFF)
		h_cerrar_red(sock);
	sock = 0xFFFFFFFF;
	conectado = 0;
}

int ts_conecta(void)
{
	static char t[4096];
	u8 init[101], e[32], ep[32], k[32];
	char b64[160], pedido[600];
	char prologo[40];
	int n;

	ts_desconecta();
	if (parte_url() < 0)
		return -1;
	if (!cuerpo) {
		cabe_cuerpo = 1 << 20;
		cuerpo = h_memoria(cabe_cuerpo);
		if (!cuerpo) {
			formatear(ts_error, sizeof ts_error, "no hay memoria");
			return -1;
		}
	}
	/* el primer mensaje de Noise IK */
	blake2s(n_h, 32, 0, 0, "Noise_IK_25519_ChaChaPoly_BLAKE2s", 33);
	memcpy(n_ck, n_h, 32);
	n = formatear(prologo, sizeof prologo, "Tailscale Control Protocol v%u", TS_CAPVER);
	mix_hash(prologo, n);
	mix_hash(ts_control_pub, 32);
	init[0] = TS_CAPVER >> 8, init[1] = TS_CAPVER & 255;	/* la versión del protocolo: la de las capacidades */
	init[2] = 1;			/* msgTypeInitiation */
	init[3] = 0, init[4] = 96;
	h_azar(e, 32);
	crypto_x25519_public_key(ep, e);
	memcpy(init + 5, ep, 32);
	mix_hash(ep, 32);
	mix_dh(e, ts_control_pub, k);
	sella(init + 37, k, 0, ts_maquina_pub, 32, n_h, 32);
	mix_hash(init + 37, 48);
	mix_dh(ts_maquina, ts_control_pub, k);
	sella(init + 85, k, 0, (const u8 *)"", 0, n_h, 32);
	mix_hash(init + 85, 16);
	h_base64(init, 101, b64, sizeof b64);
	sock = abre();
	if (sock == 0xFFFFFFFF)
		return -1;
	n = formatear(pedido, sizeof pedido,
		      "POST /ts2021 HTTP/1.1\r\nHost: %s\r\nUser-Agent: HispaPentium-Tailscale/0.1\r\nUpgrade: tailscale-control-protocol\r\n"
		      "Connection: upgrade\r\nX-Tailscale-Handshake: %s\r\nContent-Length: 0\r\n\r\n", host, b64);
	if (ES_ERROR(h_enviar(sock, pedido, n)))
		goto mal;
	n = cabeceras_http(sock, t, sizeof t, 20);
	if (n < 0) {
		formatear(ts_error, sizeof ts_error, "el servidor de control no contesta a /ts2021");
		goto mal;
	}
	if (codigo_http(t) != 101) {
		formatear(ts_error, sizeof ts_error, "/ts2021: el servidor de control contesta %u", codigo_http(t));
		goto mal;
	}
	memcpy(crudo, resto, n_resto);
	n_crudo = n_resto;
	/* la respuesta de Noise: 3 + 48 */
	while (n_crudo < 3) {
		if (llena_crudo(100) < 0)
			goto mal;
	}
	if (crudo[0] == 3) {
		/* un error, en claro */
		u32 l = crudo[1] << 8 | crudo[2];
		while (n_crudo < 3 + l && n_crudo < sizeof crudo)
			if (llena_crudo(100) < 0)
				break;
		crudo[n_crudo < sizeof crudo ? n_crudo : sizeof crudo - 1] = 0;
		formatear(ts_error, sizeof ts_error, "el servidor de control dice: %s", (char *)crudo + 3);
		goto mal;
	}
	while (n_crudo < 51)
		if (llena_crudo(100) < 0)
			goto mal;
	if (crudo[0] != 2 || (crudo[1] << 8 | crudo[2]) != 48) {
		formatear(ts_error, sizeof ts_error, "/ts2021: respuesta de Noise mal hecha");
		goto mal;
	}
	{
		u8 ce[32], nada[1];
		memcpy(ce, crudo + 3, 32);
		mix_hash(ce, 32);
		mix_dh(e, ce, k);
		mix_dh(ts_maquina, ce, k);
		if (abre_sello(nada, k, 0, crudo + 35, 16, n_h, 32)) {
			formatear(ts_error, sizeof ts_error, "/ts2021: la respuesta de Noise no vale (¿otra clave de servidor?)");
			goto mal;
		}
		mix_hash(crudo + 35, 16);
		hkdf_blake2s(n_ck, (const u8 *)"", 0, k_env, k_rec, 0);
		c_env = c_rec = 0;
		memmove(crudo, crudo + 51, n_crudo - 51);
		n_crudo -= 51;
	}
	crypto_wipe(e, 32);
	crypto_wipe(k, 32);
	conectado = 1;
	n_claro = 0;
	temprano = 1;
	flujo_sig = 1;
	estado_flujo = 0;
	sin_avisar = 0;
	/* HTTP/2: el prefacio, los ajustes (sin «push»; ventana de 4 MB) y más ventana para la conexión */
	{
		static const u8 ajustes[12] = { 0, 2, 0, 0, 0, 0, 0, 4, 0, 0x40, 0, 0 };
		if (manda_claro((const u8 *)"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24) < 0 || frame(H2_SETTINGS, 0, 0, ajustes, 12) < 0)
			goto mal;
		ventana(0, (4 << 20) - 65535);
	}
	h_apunta("control: conectado con %s (ts2021)", host);
	return 0;
mal:
	ts_desconecta();
	return -1;
}

/* HPACK: «literal sin indexar, nombre nuevo» */
static u32 hpack(u8 *d, const char *nombre, const char *valor)
{
	u32 k = 0, nn = strlen(nombre), nv = strlen(valor);

	d[k++] = 0;
	d[k++] = nn;
	memcpy(d + k, nombre, nn);
	k += nn;
	if (nv < 127)
		d[k++] = nv;
	else {
		u32 v = nv - 127;
		d[k++] = 127;
		while (v >= 128) {
			d[k++] = (v & 127) | 128;
			v >>= 7;
		}
		d[k++] = v;
	}
	memcpy(d + k, valor, nv);
	return k + nv;
}

int ts_pide(const char *camino, const char *json, u32 n, int abierto)
{
	u8 cab[1024];
	char largo[16];
	u32 k = 0, hecho = 0;

	if (!conectado)
		return -1;
	flujo = flujo_sig;
	flujo_sig += 2;
	estado_flujo = 1;
	codigo_flujo = 0;
	n_cuerpo = 0;
	formatear(largo, sizeof largo, "%u", n);
	k += hpack(cab + k, ":method", "POST");
	k += hpack(cab + k, ":scheme", "https");
	k += hpack(cab + k, ":authority", host);
	k += hpack(cab + k, ":path", camino);
	k += hpack(cab + k, "content-type", "application/json");
	k += hpack(cab + k, "content-length", largo);
	k += hpack(cab + k, "user-agent", "HispaPentium-Tailscale/0.1");
	if (frame(H2_HEADERS, 4, flujo, cab, k) < 0)
		return -1;
	ventana(flujo, (4 << 20) - 65535);
	while (hecho < n) {
		u32 t = n - hecho > 16384 ? 16384 : n - hecho;
		if (frame(H2_DATA, hecho + t == n ? 1 : 0, flujo, (const u8 *)json + hecho, t) < 0)
			return -1;
		hecho += t;
	}
	if (!n && frame(H2_DATA, 1, flujo, 0, 0) < 0)
		return -1;
	/* hasta que lleguen las cabeceras de la respuesta */
	{
		u32 limite = h_tics() + ts_espera_cabeceras * h_hz;
		while (estado_flujo == 1) {
			if (bombea(50) < 0)
				return -1;
			if ((int)(limite - h_tics()) <= 0) {
				formatear(ts_error, sizeof ts_error, "el servidor de control no contesta a %s", camino);
				return -1;
			}
		}
	}
	(void)abierto;
	return codigo_flujo;
}

int ts_lee_cuerpo(u8 *d, u32 cabe, u32 espera)
{
	u32 k;
	static u32 veces;

	if (ts_depura && (veces++ % 20) == 0 && veces < 400)
		h_apunta("h2: leyendo (%u en crudo, %u en claro, %u del cuerpo, estado %u)", n_crudo, n_claro, n_cuerpo, estado_flujo);

	if (!n_cuerpo) {
		if (estado_flujo == 3)
			return -1;
		if (bombea(espera) < 0)
			return -1;
		if (!n_cuerpo)
			return estado_flujo == 3 ? -1 : 0;
	}
	k = n_cuerpo < cabe ? n_cuerpo : cabe;
	memcpy(d, cuerpo, k);
	memmove(cuerpo, cuerpo + k, n_cuerpo - k);
	n_cuerpo -= k;
	return k;
}

int ts_cuerpo_entero(char *d, u32 cabe, u32 espera)
{
	u32 limite = h_tics() + espera, n = 0;

	while (estado_flujo != 3) {
		if (bombea(50) < 0)
			break;
		if ((int)(limite - h_tics()) <= 0)
			break;
	}
	n = n_cuerpo < cabe - 1 ? n_cuerpo : cabe - 1;
	memcpy(d, cuerpo, n);
	d[n] = 0;
	n_cuerpo = 0;
	return n;
}
