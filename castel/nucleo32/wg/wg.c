/* wg.c - el protocolo de WireGuard (wg.h). Escrito a partir del documento
 * de WireGuard («WireGuard: Next Generation Kernel Network Tunnel», J. A.
 * Donenfeld, secciones 5 y 6) y de la descripción de wireguard.com/protocol;
 * sin código de la implementación de Linux (GPL-2.0) ni de wireguard-go.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "wg.h"
#include "cripto.h"
#include "monocypher.h"

#define CONSTRUCCION "Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s"
#define IDENTIFICADOR "WireGuard v1 zx2c4 Jason@zx2c4.com"
#define REKEY_AFTER_MESSAGES (1ULL << 60)
#define REJECT_AFTER_MESSAGES (~0ULL - 8192)
#define REKEY_AFTER_TIME 120
#define REJECT_AFTER_TIME 180
#define REKEY_ATTEMPT_TIME 90
#define REKEY_TIMEOUT 5
#define KEEPALIVE_TIMEOUT 10

struct wg_par wg_pares[WG_MAX_PARES];
u8 wg_priv[32], wg_pub[32];
static u8 ck0[32], h0[32];		/* lo del principio de todos los apretones */
static u8 mac1_mia[32], galleta_mia[32];	/* las de nuestra clave pública (como respondedor) */
static u8 secreto_galletas[32];
static u32 secreto_tic;

static u32 seg(void) { return h_tics() / h_hz; }
static int pasado(u32 desde, u32 s) { return h_tics() - desde >= s * h_hz; }

static void le32(u8 *p, u32 v) { p[0] = v, p[1] = v >> 8, p[2] = v >> 16, p[3] = v >> 24; }
static u32 lee_le32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }
static void le64(u8 *p, u64 v) { le32(p, (u32)v); le32(p + 4, (u32)(v >> 32)); }
static u64 lee_le64(const u8 *p) { return lee_le32(p) | (u64)lee_le32(p + 4) << 32; }

static void hash2(u8 r[32], const u8 *a, u32 na, const u8 *b, u32 nb)
{
	blake2s_ctx c;

	blake2s_inicia(&c, 32, 0, 0);
	blake2s_mas(&c, a, na);
	blake2s_mas(&c, b, nb);
	blake2s_fin(&c, r);
}

static void mezcla_h(u8 h[32], const u8 *d, u32 n) { hash2(h, h, 32, d, n); }

/* ChaCha20-Poly1305 de la RFC 8439 con el nonce de WireGuard (4 ceros y el contador, de menor a mayor) */
static void aead_cierra(u8 *sale, const u8 k[32], u64 cont, const u8 *d, u32 n, const u8 *ad, u32 nad)
{
	crypto_aead_ctx c;
	u8 nonce[12];

	memset(nonce, 0, 4);
	le64(nonce + 4, cont);
	crypto_aead_init_ietf(&c, k, nonce);
	crypto_aead_write(&c, sale, sale + n, ad, nad, d, n);
	crypto_wipe(&c, sizeof c);
}

static int aead_abre(u8 *sale, const u8 k[32], u64 cont, const u8 *d, u32 n, const u8 *ad, u32 nad)
{
	crypto_aead_ctx c;
	u8 nonce[12];
	int r;

	if (n < 16)
		return -1;
	memset(nonce, 0, 4);
	le64(nonce + 4, cont);
	crypto_aead_init_ietf(&c, k, nonce);
	r = crypto_aead_read(&c, sale, d + n - 16, ad, nad, d, n - 16);
	crypto_wipe(&c, sizeof c);
	return r;
}

static void kdf1(u8 ck[32], const u8 *d, u32 n) { hkdf_blake2s(ck, d, n, ck, 0, 0); }
static void kdf2(u8 ck[32], u8 k[32], const u8 *d, u32 n) { hkdf_blake2s(ck, d, n, ck, k, 0); }

static void mac16(u8 r[16], const u8 clave[32], const u8 *d, u32 n) { blake2s(r, 16, clave, 32, d, n); }

int wg_de_base64(const char *t, u8 d[32])
{
	u8 b[40];
	int n = 0;

	while (t[n] && t[n] != ' ' && t[n] != '\r' && t[n] != '\n' && t[n] != '\t')
		n++;
	if (n != 44)
		return 0;
	if (h_de_base64(t, n, b, sizeof b) != 32)
		return 0;
	memcpy(d, b, 32);
	return 1;
}

void wg_a_base64(const u8 d[32], char *t)
{
	h_base64(d, 32, t, 45);
}

void wg_inicia(const u8 priv[32])
{
	memcpy(wg_priv, priv, 32);
	crypto_x25519_public_key(wg_pub, wg_priv);
	blake2s(ck0, 32, 0, 0, CONSTRUCCION, strlen(CONSTRUCCION));
	hash2(h0, ck0, 32, (const u8 *)IDENTIFICADOR, strlen(IDENTIFICADOR));
	hash2(mac1_mia, (const u8 *)"mac1----", 8, wg_pub, 32);
	hash2(galleta_mia, (const u8 *)"cookie--", 8, wg_pub, 32);
	h_azar(secreto_galletas, 32);
	secreto_tic = h_tics();
	memset(wg_pares, 0, sizeof wg_pares);
}

struct wg_par *wg_par_de_clave(const u8 pub[32])
{
	int i;

	for (i = 0; i < WG_MAX_PARES; i++)
		if (wg_pares[i].usado && !memcmp(wg_pares[i].pub, pub, 32))
			return &wg_pares[i];
	return 0;
}

struct wg_par *wg_nuevo_par(const u8 pub[32])
{
	struct wg_par *p = wg_par_de_clave(pub);
	int i;

	if (p)
		return p;
	for (i = 0; i < WG_MAX_PARES; i++)
		if (!wg_pares[i].usado) {
			p = &wg_pares[i];
			memset(p, 0, sizeof *p);
			p->usado = 1;
			memcpy(p->pub, pub, 32);
			crypto_x25519(p->pre_ss, wg_priv, pub);
			hash2(p->mac1_clave, (const u8 *)"mac1----", 8, pub, 32);
			hash2(p->galleta_clave, (const u8 *)"cookie--", 8, pub, 32);
			return p;
		}
	return 0;
}

void wg_quita_par(struct wg_par *p)
{
	crypto_wipe(p, sizeof *p);
}

struct wg_par *wg_par_de_ip(u32 ip)
{
	int i, k;

	for (i = 0; i < WG_MAX_PARES; i++) {
		struct wg_par *p = &wg_pares[i];
		if (!p->usado)
			continue;
		for (k = 0; k < p->n_redes; k++)
			if ((ip & p->masc[k]) == p->red[k])
				return p;
	}
	return 0;
}

static struct wg_par *par_de_indice(u32 idx, struct wg_claves **cl)
{
	int i;

	for (i = 0; i < WG_MAX_PARES; i++) {
		struct wg_par *p = &wg_pares[i];
		if (!p->usado)
			continue;
		if (p->act.vale && p->act.mio == idx)
			return *cl = &p->act, p;
		if (p->sig.vale && p->sig.mio == idx)
			return *cl = &p->sig, p;
		if (p->ant.vale && p->ant.mio == idx)
			return *cl = &p->ant, p;
	}
	return 0;
}

static u32 indice_nuevo(void)
{
	u32 v;
	struct wg_claves *c;

	do
		h_azar(&v, 4);
	while (!v || par_de_indice(v, &c));
	return v;
}

/* TAI64N: los segundos desde 1970 (más 2^62 + 10) y los nanosegundos, de
 * mayor a menor; siempre más que la vez anterior (el otro tira lo que no suba) */
static void tai64n(u8 t[12])
{
	static u8 ultimo[12];
	u32 f = sc(SC_DATO, DATO_FECHA, 0, 0), fe = f >> 16, ho = f & 0xFFFF;
	u32 an = 1980 + (fe >> 9), me = fe >> 5 & 15, di = fe & 31;
	u32 hh = ho >> 11, mi = ho >> 5 & 63, ss = (ho & 31) * 2;
	static const u16 antes[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	u64 dias, s;
	u32 n;
	int i;

	if (me < 1 || me > 12)
		me = 1;
	if (di < 1)
		di = 1;
	dias = (u64)(an - 1970) * 365 + (an - 1969) / 4 + antes[me - 1] + di - 1;
	if (me > 2 && an % 4 == 0)
		dias++;
	s = dias * 86400 + hh * 3600 + mi * 60 + ss + 0x400000000000000aULL;
	n = (h_tics() % (2 * h_hz)) * (500000000u / h_hz);	/* (la FAT cuenta de dos en dos segundos) */
	if (n >= 1000000000u)
		n -= 1000000000u, s++;
	t[0] = s >> 56, t[1] = s >> 48, t[2] = s >> 40, t[3] = s >> 32, t[4] = s >> 24, t[5] = s >> 16, t[6] = s >> 8, t[7] = s;
	t[8] = n >> 24, t[9] = n >> 16, t[10] = n >> 8, t[11] = n;
	if (memcmp(t, ultimo, 12) <= 0) {
		memcpy(t, ultimo, 12);
		for (i = 11; i >= 0 && ++t[i] == 0; i--) ;
	}
	memcpy(ultimo, t, 12);
}

static void pon_macs(struct wg_par *p, u8 *m, u32 n)
{
	mac16(m + n - 32, p->mac1_clave, m, n - 32);
	memcpy(p->ult_mac1, m + n - 32, 16);
	if (p->galleta_tic && !pasado(p->galleta_tic, 120))
		blake2s(m + n - 16, 16, p->galleta, 16, m, n - 16);
	else
		memset(m + n - 16, 0, 16);
}

/* ---- el iniciador: el primer mensaje ---- */
static void empieza_apreton(struct wg_par *p)
{
	u8 m[148], k[32], dh[32], ep[32], ts[12];

	if (p->hs_curso && !pasado(p->hs_empezado, REKEY_TIMEOUT))
		return;
	if (!p->hs_curso || !p->hs_primero)
		p->hs_primero = h_tics();
	p->hs_curso = 1;
	p->hs_empezado = h_tics();
	p->hs_indice = indice_nuevo();
	memcpy(p->hs_ck, ck0, 32);
	hash2(p->hs_h, h0, 32, p->pub, 32);
	h_azar(p->hs_e, 32);
	crypto_x25519_public_key(ep, p->hs_e);
	memset(m, 0, sizeof m);
	m[0] = 1;
	le32(m + 4, p->hs_indice);
	memcpy(m + 8, ep, 32);
	kdf1(p->hs_ck, ep, 32);
	mezcla_h(p->hs_h, ep, 32);
	crypto_x25519(dh, p->hs_e, p->pub);
	kdf2(p->hs_ck, k, dh, 32);
	aead_cierra(m + 40, k, 0, wg_pub, 32, p->hs_h, 32);
	mezcla_h(p->hs_h, m + 40, 48);
	kdf2(p->hs_ck, k, p->pre_ss, 32);
	tai64n(ts);
	aead_cierra(m + 88, k, 0, ts, 12, p->hs_h, 32);
	mezcla_h(p->hs_h, m + 88, 28);
	pon_macs(p, m, 148);
	crypto_wipe(k, 32);
	crypto_wipe(dh, 32);
	p->ult_hs = h_tics();
	h_apunta("%s: apretón de manos (iniciación)", p->nombre);
	wg_manda(p, m, 148);
	p->ult_env = h_tics();
}

static void claves_de(struct wg_claves *c, const u8 ck[32], int soy_inicio, u32 mio, u32 suyo)
{
	u8 a[32], b[32];

	hkdf_blake2s(ck, (const u8 *)"", 0, a, b, 0);
	memset(c, 0, sizeof *c);
	c->vale = 1;
	c->soy_inicio = soy_inicio;
	c->mio = mio;
	c->suyo = suyo;
	memcpy(soy_inicio ? c->enviar : c->recibir, a, 32);
	memcpy(soy_inicio ? c->recibir : c->enviar, b, 32);
	c->nacida = h_tics();
	crypto_wipe(a, 32);
	crypto_wipe(b, 32);
}

static void envia_datos(struct wg_par *p, const u8 *d, u32 n);

static void manda_pendiente(struct wg_par *p)
{
	if (p->n_pend) {
		u32 n = p->n_pend;
		p->n_pend = 0;
		envia_datos(p, p->pend, n);
	} else
		envia_datos(p, 0, 0);	/* (un mensaje vacío: así el respondedor da por buenas las claves) */
}

/* ---- el respondedor: llega una iniciación ---- */
static int mira_macs(const u8 *m, u32 n, const u8 *de_quien)
{
	u8 mac[16];

	mac16(mac, mac1_mia, m, n - 32);
	(void)de_quien;
	return crypto_verify16(mac, m + n - 32) == 0;
}

static void llega_iniciacion(const u8 *m, u32 ip, u32 puerto)
{
	u8 ck[32], h[32], k[32], dh[32], est[32], ts[12], er[32], ep[32], r[92];
	struct wg_par *p;
	u32 suyo = lee_le32(m + 4), mio;

	if (!mira_macs(m, 148, 0))
		return;
	memcpy(ck, ck0, 32);
	hash2(h, h0, 32, wg_pub, 32);
	kdf1(ck, m + 8, 32);
	mezcla_h(h, m + 8, 32);
	crypto_x25519(dh, wg_priv, m + 8);
	kdf2(ck, k, dh, 32);
	if (aead_abre(est, k, 0, m + 40, 48, h, 32))
		return;
	mezcla_h(h, m + 40, 48);
	p = wg_par_de_clave(est);
	if (!p)
		return;		/* (una clave que no es de nadie de los nuestros) */
	kdf2(ck, k, p->pre_ss, 32);
	if (aead_abre(ts, k, 0, m + 88, 28, h, 32))
		return;
	mezcla_h(h, m + 88, 28);
	if (memcmp(ts, p->tai_max, 12) <= 0) {
		h_apunta("%s: iniciación repetida (la marca de tiempo no sube): fuera", p->nombre);
		return;
	}
	memcpy(p->tai_max, ts, 12);
	/* la respuesta */
	h_azar(er, 32);
	crypto_x25519_public_key(ep, er);
	mio = indice_nuevo();
	memset(r, 0, sizeof r);
	r[0] = 2;
	le32(r + 4, mio);
	le32(r + 8, suyo);
	memcpy(r + 12, ep, 32);
	kdf1(ck, ep, 32);
	mezcla_h(h, ep, 32);
	crypto_x25519(dh, er, m + 8);
	kdf1(ck, dh, 32);
	crypto_x25519(dh, er, p->pub);
	kdf1(ck, dh, 32);
	{
		u8 tau[32];
		hkdf_blake2s(ck, p->psk, 32, ck, tau, k);
		mezcla_h(h, tau, 32);
		crypto_wipe(tau, 32);
	}
	aead_cierra(r + 44, k, 0, (const u8 *)"", 0, h, 32);
	mezcla_h(h, r + 44, 16);
	pon_macs(p, r, 92);
	if (p->sig.vale)
		crypto_wipe(&p->sig, sizeof p->sig);
	claves_de(&p->sig, ck, 0, mio, suyo);
	if (ip) {
		p->ep_ip = ip;
		p->ep_puerto = puerto;
	}
	h_apunta("%s: apretón de manos (respuesta)", p->nombre);
	wg_manda(p, r, 92);
	p->ult_env = h_tics();
	p->ult_rec = h_tics();
	crypto_wipe(ck, 32);
	crypto_wipe(k, 32);
	crypto_wipe(dh, 32);
	crypto_wipe(er, 32);
}

/* ---- el iniciador: llega la respuesta ---- */
static void llega_respuesta(const u8 *m, u32 ip, u32 puerto)
{
	u32 suyo = lee_le32(m + 4), mio = lee_le32(m + 8);
	struct wg_par *p = 0;
	u8 ck[32], h[32], k[32], dh[32], nada[1];
	int i;

	for (i = 0; i < WG_MAX_PARES; i++)
		if (wg_pares[i].usado && wg_pares[i].hs_curso && wg_pares[i].hs_indice == mio)
			p = &wg_pares[i];
	if (!p)
		return;
	{
		u8 mac[16];
		mac16(mac, mac1_mia, m, 92 - 32);
		if (crypto_verify16(mac, m + 60))
			return;
	}
	memcpy(ck, p->hs_ck, 32);
	memcpy(h, p->hs_h, 32);
	kdf1(ck, m + 12, 32);
	mezcla_h(h, m + 12, 32);
	crypto_x25519(dh, p->hs_e, m + 12);
	kdf1(ck, dh, 32);
	crypto_x25519(dh, wg_priv, m + 12);
	kdf1(ck, dh, 32);
	{
		u8 tau[32];
		hkdf_blake2s(ck, p->psk, 32, ck, tau, k);
		mezcla_h(h, tau, 32);
		crypto_wipe(tau, 32);
	}
	if (aead_abre(nada, k, 0, m + 44, 16, h, 32)) {
		h_apunta("%s: la respuesta no vale (¿otra clave?)", p->nombre);
		return;
	}
	if (p->ant.vale)
		crypto_wipe(&p->ant, sizeof p->ant);
	p->ant = p->act;
	claves_de(&p->act, ck, 1, mio, suyo);
	p->hs_curso = 0;
	p->hs_primero = 0;
	p->hs_reintentos = 0;
	crypto_wipe(p->hs_e, 32);
	if (ip) {
		p->ep_ip = ip;
		p->ep_puerto = puerto;
	}
	p->ult_rec = h_tics();
	p->hs_fecha = sc(SC_DATO, DATO_FECHA, 0, 0);
	h_apunta("%s: túnel hecho (claves nuevas)", p->nombre);
	manda_pendiente(p);
	crypto_wipe(ck, 32);
	crypto_wipe(k, 32);
	crypto_wipe(dh, 32);
}

/* ---- una galleta (el otro está cargado y pide prueba de que somos quien decimos) ---- */
static void llega_galleta(const u8 *m)
{
	u32 mio = lee_le32(m + 4);
	int i;

	for (i = 0; i < WG_MAX_PARES; i++) {
		struct wg_par *p = &wg_pares[i];
		if (p->usado && p->hs_curso && p->hs_indice == mio) {
			u8 g[16];
			if (crypto_aead_unlock(g, m + 8 + 24 + 16, p->galleta_clave, m + 8, p->ult_mac1, 16, m + 32, 16) == 0) {
				memcpy(p->galleta, g, 16);
				p->galleta_tic = h_tics();
				h_apunta("%s: llega una galleta (el otro está cargado)", p->nombre);
			}
			return;
		}
	}
}

/* ---- los datos ---- */
static void envia_datos(struct wg_par *p, const u8 *d, u32 n)
{
	static u8 m[16 + WG_MTU + 32];
	u8 bloque[WG_MTU + 16];
	u32 np = (n + 15) & ~15u;
	struct wg_claves *c = &p->act;

	if (!c->vale || c->env >= REJECT_AFTER_MESSAGES || pasado(c->nacida, REJECT_AFTER_TIME)) {
		if (n && n <= WG_MTU) {
			memcpy(p->pend, d, n);
			p->n_pend = n;
		}
		empieza_apreton(p);
		return;
	}
	if (np > WG_MTU + 15)
		return;
	memcpy(bloque, d, n);
	memset(bloque + n, 0, np - n);
	m[0] = 4, m[1] = m[2] = m[3] = 0;
	le32(m + 4, c->suyo);
	le64(m + 8, c->env);
	aead_cierra(m + 16, c->enviar, c->env, bloque, np, (const u8 *)"", 0);
	c->env++;
	wg_manda(p, m, 16 + np + 16);
	p->ult_env = h_tics();
	if (n)
		p->ult_datos_env = h_tics();
	p->tx += 16 + np + 16;
	/* hace tiempo de las claves (y somos los que empezamos): unas nuevas */
	if (c->soy_inicio && (c->env > REKEY_AFTER_MESSAGES || pasado(c->nacida, REKEY_AFTER_TIME)))
		empieza_apreton(p);
}

static int ventana_vale(struct wg_claves *c, u64 n)
{
	if (n >= REJECT_AFTER_MESSAGES)
		return 0;
	if (n > c->rec_max || (c->rec_max == 0 && c->ventana == 0)) {
		u64 d = n - c->rec_max;
		if (c->rec_max == 0 && c->ventana == 0)
			c->ventana = 1;
		else
			c->ventana = d >= 64 ? 1 : (c->ventana << d) | 1;
		c->rec_max = n;
		return 1;
	}
	if (c->rec_max - n >= 64)
		return 0;
	if (c->ventana >> (c->rec_max - n) & 1)
		return 0;
	c->ventana |= 1ULL << (c->rec_max - n);
	return 1;
}

static int llegan_datos(const u8 *m, u32 n, u32 ip, u32 puerto, u8 *sale, struct wg_par **quien)
{
	struct wg_claves *c;
	struct wg_par *p = par_de_indice(lee_le32(m + 4), &c);
	u64 cont = lee_le64(m + 8);
	u32 largo;

	if (!p || n < 32 || n - 32 > WG_MTU + 15)
		return 0;
	if (pasado(c->nacida, REJECT_AFTER_TIME))
		return 0;
	{
		/* (la ventana se mira después de comprobar el mensaje, sobre una copia) */
		struct wg_claves copia = *c;
		if (!ventana_vale(&copia, cont))
			return 0;
		if (aead_abre(sale, c->recibir, cont, m + 16, n - 16, (const u8 *)"", 0))
			return 0;
		c->rec_max = copia.rec_max;
		c->ventana = copia.ventana;
	}
	if (c == &p->sig) {
		/* el primer dato con las claves del respondedor: pasan a ser las de ahora */
		if (p->ant.vale)
			crypto_wipe(&p->ant, sizeof p->ant);
		p->ant = p->act;
		p->act = p->sig;
		memset(&p->sig, 0, sizeof p->sig);
		p->hs_fecha = sc(SC_DATO, DATO_FECHA, 0, 0);
		h_apunta("%s: túnel hecho (claves nuevas, del otro)", p->nombre);
		if (p->n_pend)
			manda_pendiente(p);
	}
	if (ip) {
		p->ep_ip = ip;
		p->ep_puerto = puerto;
	}
	p->ult_rec = h_tics();
	p->rx += n;
	*quien = p;
	if (n == 32)
		return 0;	/* (un «sigo aquí») */
	p->ult_datos_rec = h_tics();
	/* el largo de verdad: el de la cabecera IP */
	if (sale[0] >> 4 != 4)
		return 0;
	largo = sale[2] << 8 | sale[3];
	if (largo > n - 32 || largo < 20)
		return 0;
	/* que venga de lo que tiene permitido */
	{
		u32 de = sale[12] | sale[13] << 8 | sale[14] << 16 | (u32)sale[15] << 24;
		int k, bien = 0;
		for (k = 0; k < p->n_redes; k++)
			if ((de & p->masc[k]) == p->red[k])
				bien = 1;
		if (!bien)
			return 0;
	}
	return largo;
}

int wg_es_mensaje(const u8 *d, u32 n)
{
	if (n < 4 || d[1] || d[2] || d[3])
		return 0;
	return (d[0] == 1 && n == 148) || (d[0] == 2 && n == 92) || (d[0] == 3 && n == 64) || (d[0] == 4 && n >= 32);
}

int wg_entra(const u8 *d, u32 n, u32 ip, u32 puerto, u8 *sale, struct wg_par **quien)
{
	*quien = 0;
	if (!wg_es_mensaje(d, n))
		return 0;
	switch (d[0]) {
	case 1:
		llega_iniciacion(d, ip, puerto);
		return 0;
	case 2:
		llega_respuesta(d, ip, puerto);
		return 0;
	case 3:
		llega_galleta(d);
		return 0;
	}
	return llegan_datos(d, n, ip, puerto, sale, quien);
}

int wg_sale(const u8 *pq, u32 n)
{
	u32 a;
	struct wg_par *p;

	if (n < 20 || n > WG_MTU)
		return -1;
	a = pq[16] | pq[17] << 8 | pq[18] << 16 | (u32)pq[19] << 24;
	p = wg_par_de_ip(a);
	if (!p)
		return -1;
	envia_datos(p, pq, n);
	return 0;
}

void wg_tic(void)
{
	int i;

	if (pasado(secreto_tic, 120)) {
		h_azar(secreto_galletas, 32);
		secreto_tic = h_tics();
	}
	for (i = 0; i < WG_MAX_PARES; i++) {
		struct wg_par *p = &wg_pares[i];
		if (!p->usado)
			continue;
		/* el apretón que no contesta: otra vez, hasta 90 s */
		if (p->hs_curso && pasado(p->hs_empezado, REKEY_TIMEOUT)) {
			if (pasado(p->hs_primero, REKEY_ATTEMPT_TIME)) {
				h_apunta("%s: no contesta al apretón de manos", p->nombre);
				p->hs_curso = 0;
				p->hs_primero = 0;
				p->n_pend = 0;
			} else
				empieza_apreton(p);
		}
		/* han llegado datos y no hemos mandado nada en 10 s: un «sigo aquí» */
		if (p->act.vale && p->ult_datos_rec && p->ult_rec - p->ult_env < 0x80000000u && pasado(p->ult_env, KEEPALIVE_TIMEOUT) &&
		    !pasado(p->ult_datos_rec, KEEPALIVE_TIMEOUT + 2)) {
			envia_datos(p, 0, 0);
		}
		/* el «sigo aquí» de siempre (PersistentKeepalive) */
		if (p->mantener && pasado(p->ult_env, p->mantener)) {
			if (p->act.vale && !pasado(p->act.nacida, REJECT_AFTER_TIME))
				envia_datos(p, 0, 0);
			else if (!p->hs_curso)
				empieza_apreton(p);
		}
		/* las claves caducadas, fuera */
		if (p->act.vale && pasado(p->act.nacida, REJECT_AFTER_TIME * 3))
			crypto_wipe(&p->act, sizeof p->act);
		if (p->ant.vale && pasado(p->ant.nacida, REJECT_AFTER_TIME))
			crypto_wipe(&p->ant, sizeof p->ant);
		/* las del iniciador que llegan a 165 s sin hablar: unas nuevas al mandar algo */
		if (p->act.vale && p->act.soy_inicio && pasado(p->act.nacida, REJECT_AFTER_TIME - KEEPALIVE_TIMEOUT - REKEY_TIMEOUT) &&
		    !pasado(p->ult_datos_rec, KEEPALIVE_TIMEOUT) && !p->hs_curso)
			empieza_apreton(p);
	}
	(void)seg;
}
