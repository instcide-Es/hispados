/* derp.c - los relevos DERP de Tailscale: cuando dos equipos no se ven
 * directamente, los paquetes de WireGuard van por un servidor DERP (HTTPS,
 * con «Upgrade: DERP»), dirigidos a la clave de nodo del otro. El servidor
 * se presenta (FrameServerKey), uno dice quién es con una caja de NaCl
 * (FrameClientInfo), y luego van FrameSendPacket y FrameRecvPacket.
 * Escrito a partir de la descripción del protocolo en derp/derp.go de
 * Tailscale (BSD-3), como referencia.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ts.h"
#include "cripto.h"
#include "monocypher.h"

#define CABE_DERP (66 * 1024)

struct derp {
	int usada, region, presentada;
	u32 sock, ultimo;
	u8 servidor[32];
	u8 *ent;
	u32 n;
	char host[96];
};
static struct derp derps[DERP_MAX];
int derp_casa;

static int cabeceras(struct derp *d)
{
	u32 limite = h_tics() + 20 * h_hz, i;

	for (;;) {
		u32 r;
		for (i = 3; i < d->n; i++)
			if (d->ent[i - 3] == '\r' && d->ent[i - 2] == '\n' && d->ent[i - 1] == '\r' && d->ent[i] == '\n') {
				int cod = 0;
				char *c = (char *)d->ent;
				while (*c != ' ' && c < (char *)d->ent + i)
					c++;
				cod = (c[1] - '0') * 100 + (c[2] - '0') * 10 + (c[3] - '0');
				memmove(d->ent, d->ent + i + 1, d->n - i - 1);
				d->n -= i + 1;
				return cod;
			}
		if (d->n >= 8192 || (int)(limite - h_tics()) <= 0)
			return -1;
		r = h_recibir(d->sock, d->ent + d->n, 8192 - d->n, 50);
		if (r == TIEMPO)
			continue;
		if (ES_ERROR(r) || r == 0)
			return -1;
		d->n += r;
	}
}

static int manda_frame(struct derp *d, u32 tipo, const u8 *a, u32 na, const u8 *b, u32 nb)
{
	static u8 f[5 + 32 + 2048];
	u32 n = na + nb;

	if (5 + n > sizeof f)
		return -1;
	f[0] = tipo;
	f[1] = n >> 24, f[2] = n >> 16, f[3] = n >> 8, f[4] = n;
	memcpy(f + 5, a, na);
	memcpy(f + 5 + na, b, nb);
	if (ES_ERROR(h_enviar(d->sock, f, 5 + n))) {
		d->usada = 0;
		h_cerrar_red(d->sock);
		return -1;
	}
	return 0;
}

static void cierra(struct derp *d)
{
	if (d->usada)
		h_cerrar_red(d->sock);
	d->usada = 0;
	d->presentada = 0;
}

/* un frame entero en d->ent: su tipo y largo; 0 si aún no */
static int frame_entero(struct derp *d, u32 *tipo, u32 *largo)
{
	if (d->n < 5)
		return 0;
	*tipo = d->ent[0];
	*largo = (u32)d->ent[1] << 24 | d->ent[2] << 16 | d->ent[3] << 8 | d->ent[4];
	if (*largo > CABE_DERP - 5)
		return -1;
	return d->n >= 5 + *largo;
}

static void gasta(struct derp *d, u32 largo)
{
	memmove(d->ent, d->ent + 5 + largo, d->n - 5 - largo);
	d->n -= 5 + largo;
}

int derp_region_conectada(int region)
{
	int i;

	for (i = 0; i < DERP_MAX; i++)
		if (derps[i].usada && derps[i].presentada && derps[i].region == region)
			return 1;
	return 0;
}

int derp_conecta(int region, const char *host, u32 ip, u32 puerto)
{
	struct derp *d = 0;
	char pedido[400];
	int i, k, cod;
	u32 tipo, largo, limite;

	for (i = 0; i < DERP_MAX; i++)
		if (derps[i].usada && derps[i].region == region)
			return i;
	for (i = 0; i < DERP_MAX && !d; i++)
		if (!derps[i].usada)
			d = &derps[i];
	if (!d) {
		/* (la que menos se usa, fuera; nunca la de casa) */
		for (i = 0; i < DERP_MAX; i++)
			if (derps[i].region != derp_casa && (!d || derps[i].ultimo < d->ultimo))
				d = &derps[i];
		cierra(d);
	}
	if (!d->ent) {
		d->ent = h_memoria(CABE_DERP);
		if (!d->ent)
			return -1;
	}
	if (!ip && ES_ERROR(h_resolver(host, &ip))) {
		h_apunta("derp-%u: %s no se encuentra", region, host);
		return -1;
	}
	d->sock = h_conectar_seguro(host, ip, puerto ? puerto : 443, 15 * h_hz);
	if (ES_ERROR(d->sock)) {
		h_apunta("derp-%u: %s: %s", region, host, h_texto_error_red(d->sock));
		return -1;
	}
	d->usada = 1;
	d->region = region;
	d->n = 0;
	d->presentada = 0;
	h_copia(d->host, host, sizeof d->host);
	k = formatear(pedido, sizeof pedido, "GET /derp HTTP/1.1\r\nHost: %s\r\nUser-Agent: HispaPentium-Tailscale/0.1\r\nUpgrade: DERP\r\nConnection: Upgrade\r\n\r\n", host);
	h_enviar(d->sock, pedido, k);
	cod = cabeceras(d);
	if (cod != 101) {
		h_apunta("derp-%u: el servidor contesta %d en vez de 101", region, cod);
		cierra(d);
		return -1;
	}
	/* FrameServerKey */
	limite = h_tics() + 20 * h_hz;
	for (;;) {
		int r = frame_entero(d, &tipo, &largo);
		u32 x;
		if (r < 0) {
			cierra(d);
			return -1;
		}
		if (r)
			break;
		if ((int)(limite - h_tics()) <= 0) {
			cierra(d);
			return -1;
		}
		x = h_recibir(d->sock, d->ent + d->n, CABE_DERP - d->n, 50);
		if (x == TIEMPO)
			continue;
		if (ES_ERROR(x) || x == 0) {
			cierra(d);
			return -1;
		}
		d->n += x;
	}
	if (tipo != 1 || largo < 40 || memcmp(d->ent + 5, "DERP\xf0\x9f\x94\x91", 8)) {
		h_apunta("derp-%u: no se presenta como un servidor DERP", region);
		cierra(d);
		return -1;
	}
	memcpy(d->servidor, d->ent + 13, 32);
	gasta(d, largo);
	/* FrameClientInfo: nuestra clave, y la caja con {"version":2} */
	{
		u8 caja[16 + 128], nonce[24], k32[32];
		const char *info = "{\"version\":2,\"CanAckPings\":true}";
		u32 ni = strlen(info);
		h_azar(nonce, 24);
		box_clave(k32, d->servidor, ts_nodo);
		secretbox_cierra(caja, (const u8 *)info, ni, nonce, k32);
		crypto_wipe(k32, 32);
		{
			u8 todo[32 + 24];
			memcpy(todo, ts_nodo_pub, 32);
			memcpy(todo + 32, nonce, 24);
			if (manda_frame(d, 2, todo, 56, caja, 16 + ni) < 0)
				return -1;
		}
	}
	d->presentada = 1;
	d->ultimo = h_tics();
	/* que es la de casa */
	if (region == derp_casa) {
		u8 uno = 1;
		manda_frame(d, 7, &uno, 1, 0, 0);
	}
	h_apunta("derp-%u: conectado con %s", region, host);
	return d - derps;
}

void derp_cierra_todas(void)
{
	int i;

	for (i = 0; i < DERP_MAX; i++)
		cierra(&derps[i]);
}

int derp_manda(int region, const u8 dest[32], const u8 *p, u32 n)
{
	int i;

	for (i = 0; i < DERP_MAX; i++)
		if (derps[i].usada && derps[i].presentada && derps[i].region == region) {
			derps[i].ultimo = h_tics();
			return manda_frame(&derps[i], 4, dest, 32, p, n);
		}
	return -1;
}

int derp_recibe(u8 de[32], u8 *p, u32 cabe, u32 *n, int *region)
{
	static int sig;
	int k;

	for (k = 0; k < DERP_MAX; k++) {
		struct derp *d = &derps[(sig + k) % DERP_MAX];
		u32 tipo, largo, x;
		int r;
		if (!d->usada || !d->presentada)
			continue;
		r = frame_entero(d, &tipo, &largo);
		if (r == 0) {
			x = h_recibir(d->sock, d->ent + d->n, CABE_DERP - d->n, 0);
			if (x != TIEMPO && (ES_ERROR(x) || x == 0)) {
				h_apunta("derp-%u: se ha cortado", d->region);
				cierra(d);
				continue;
			}
			if (!ES_ERROR(x))
				d->n += x;
			r = frame_entero(d, &tipo, &largo);
		}
		if (r < 0) {
			cierra(d);
			continue;
		}
		if (!r)
			continue;
		sig = (sig + k + 1) % DERP_MAX;
		switch (tipo) {
		case 5:		/* FrameRecvPacket: la clave del que lo manda y el paquete */
			if (largo >= 32 && largo - 32 <= cabe) {
				memcpy(de, d->ent + 5, 32);
				memcpy(p, d->ent + 5 + 32, largo - 32);
				*n = largo - 32;
				*region = d->region;
				gasta(d, largo);
				return 1;
			}
			break;
		case 0x12:	/* FramePing: el pong */
			if (largo >= 8) {
				u8 pp[8];
				memcpy(pp, d->ent + 5, 8);
				gasta(d, largo);
				manda_frame(d, 0x13, pp, 8, 0, 0);
				return 0;
			}
			break;
		case 0x14:	/* FrameHealth */
			if (largo) {
				char t[120];
				u32 m = largo < 119 ? largo : 119;
				memcpy(t, d->ent + 5, m);
				t[m] = 0;
				h_apunta("derp-%u dice: %s", d->region, t);
			}
			break;
		case 0x15:	/* FrameRestarting */
			h_apunta("derp-%u se reinicia", d->region);
			break;
		}
		gasta(d, largo);
	}
	return 0;
}

void derp_tic(void)
{
}
