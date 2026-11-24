/*
 * wpa.c - WPA2-Personal para Hispa-486: SHA-1, HMAC, PBKDF2 (la clave PMK a
 * partir de la contraseña), PRF, AES-128, el envoltorio de claves (RFC 3394),
 * CCMP (cifrado de las tramas) y el suplicante: el acuerdo de 4 pasos y la
 * renovación de la clave de grupo (IEEE 802.11i).
 *
 * Escrito aquí desde las normas (FIPS 180, FIPS 197, RFC 2104, 2898, 3394,
 * 3610 e IEEE 802.11-2007, cláusula 8). Las pruebas (tests/wifi/) lo
 * comprueban con los vectores de las normas.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "nucleo.h"

/* ---------------------------------------------------------------- SHA-1 */
struct sha1c { d32 h[5]; c64 n; o8 buf[64]; int nb; };

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_bloque(d32 *h, const o8 *p)
{
	d32 w[80], a, b, c, d, e, t;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = ((d32)p[4 * i] << 24) | (p[4 * i + 1] << 16) | (p[4 * i + 2] << 8) | p[4 * i + 3];
	for (; i < 80; i++)
		w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4];
	for (i = 0; i < 80; i++) {
		if (i < 20)
			t = ((b & c) | (~b & d)) + 0x5A827999;
		else if (i < 40)
			t = (b ^ c ^ d) + 0x6ED9EBA1;
		else if (i < 60)
			t = ((b & c) | (b & d) | (c & d)) + 0x8F1BBCDC;
		else
			t = (b ^ c ^ d) + 0xCA62C1D6;
		t += ROL(a, 5) + e + w[i];
		e = d; d = c; c = ROL(b, 30); b = a; a = t;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

static void sha1_ini(struct sha1c *c)
{
	c->h[0] = 0x67452301; c->h[1] = 0xEFCDAB89; c->h[2] = 0x98BADCFE;
	c->h[3] = 0x10325476; c->h[4] = 0xC3D2E1F0;
	c->n = 0; c->nb = 0;
}

static void sha1_mas(struct sha1c *c, const o8 *d, size_t n)
{
	c->n += n;
	while (n) {
		size_t k = 64 - c->nb;
		if (k > n)
			k = n;
		memcpy(c->buf + c->nb, d, k);
		c->nb += k; d += k; n -= k;
		if (c->nb == 64) {
			sha1_bloque(c->h, c->buf);
			c->nb = 0;
		}
	}
}

static void sha1_fin(struct sha1c *c, o8 *sal)
{
	c64 bits = c->n * 8;
	o8 x = 0x80, z = 0, l[8];
	int i;

	sha1_mas(c, &x, 1);
	while (c->nb != 56)
		sha1_mas(c, &z, 1);
	for (i = 0; i < 8; i++)
		l[i] = bits >> (56 - 8 * i);
	sha1_mas(c, l, 8);
	for (i = 0; i < 20; i++)
		sal[i] = c->h[i / 4] >> (24 - 8 * (i & 3));
}

void sha1(const o8 *d, size_t n, o8 *sal)
{
	struct sha1c c;
	sha1_ini(&c);
	sha1_mas(&c, d, n);
	sha1_fin(&c, sal);
}

void hmac_sha1_v(const o8 *k, size_t kn, int nv, const o8 **v, const size_t *vn, o8 *sal)
{
	struct sha1c c;
	o8 kk[64], t[20];
	int i;

	memset(kk, 0, 64);
	if (kn > 64)
		sha1(k, kn, kk);
	else
		memcpy(kk, k, kn);
	for (i = 0; i < 64; i++)
		kk[i] ^= 0x36;
	sha1_ini(&c);
	sha1_mas(&c, kk, 64);
	for (i = 0; i < nv; i++)
		sha1_mas(&c, v[i], vn[i]);
	sha1_fin(&c, t);
	for (i = 0; i < 64; i++)
		kk[i] ^= 0x36 ^ 0x5c;
	sha1_ini(&c);
	sha1_mas(&c, kk, 64);
	sha1_mas(&c, t, 20);
	sha1_fin(&c, sal);
}

void hmac_sha1(const o8 *k, size_t kn, const o8 *d, size_t n, o8 *sal)
{
	hmac_sha1_v(k, kn, 1, &d, &n, sal);
}

/* --------------------------------------------------------------- PBKDF2 */
/* La PMK son dos bloques de 20 octetos (se usan 32). Cada vuelta, un HMAC
 * por bloque: 8.192 HMAC en total, unos 32.000 SHA-1. En un Atom, menos de
 * medio segundo; por eso se guarda la PMK en RED.INI y no la contraseña. */
void pbkdf2_empieza(struct pbkdf2 *p, const char *clave, const o8 *ssid, int ssid_n)
{
	int b;

	memset(p, 0, sizeof(*p));
	p->clave_n = strlen(clave) > 63 ? 63 : strlen(clave);
	memcpy(p->clave, clave, p->clave_n);
	p->ssid_n = ssid_n > 32 ? 32 : ssid_n;
	memcpy(p->ssid, ssid, p->ssid_n);
	for (b = 0; b < 2; b++) {
		o8 s[36];
		memcpy(s, p->ssid, p->ssid_n);
		s[p->ssid_n] = 0; s[p->ssid_n + 1] = 0; s[p->ssid_n + 2] = 0; s[p->ssid_n + 3] = b + 1;
		hmac_sha1(p->clave, p->clave_n, s, p->ssid_n + 4, p->u[b]);
		memcpy(p->t[b], p->u[b], 20);
	}
	p->vuelta = 1;
}

int pbkdf2_sigue(struct pbkdf2 *p, int vueltas, o8 *pmk)
{
	int b, i;

	while (p->vuelta < 4096 && vueltas-- > 0) {
		for (b = 0; b < 2; b++) {
			hmac_sha1(p->clave, p->clave_n, p->u[b], 20, p->u[b]);
			for (i = 0; i < 20; i++)
				p->t[b][i] ^= p->u[b][i];
		}
		p->vuelta++;
	}
	if (p->vuelta < 4096)
		return 0;
	memcpy(pmk, p->t[0], 20);
	memcpy(pmk + 20, p->t[1], 12);
	return 1;
}

/* la forma general (para los vectores de la RFC 6070) */
void pbkdf2_sha1(const char *clave, const o8 *sal_, int sal_n, int vueltas, o8 *sal, int nsal)
{
	o8 u[20], t[20], s[80];
	int b, i, j, kn = strlen(clave);

	for (b = 1; nsal > 0; b++) {
		memcpy(s, sal_, sal_n);
		s[sal_n] = 0; s[sal_n + 1] = 0; s[sal_n + 2] = 0; s[sal_n + 3] = b;
		hmac_sha1((const o8 *)clave, kn, s, sal_n + 4, u);
		memcpy(t, u, 20);
		for (i = 1; i < vueltas; i++) {
			hmac_sha1((const o8 *)clave, kn, u, 20, u);
			for (j = 0; j < 20; j++)
				t[j] ^= u[j];
		}
		memcpy(sal, t, nsal < 20 ? nsal : 20);
		sal += 20; nsal -= 20;
	}
}

/* PRF de 802.11i: HMAC-SHA1(K, etiqueta || 0 || datos || i) */
void prf_sha1(const o8 *k, size_t kn, const char *etiqueta, const o8 *d, size_t n, o8 *sal, size_t nsal)
{
	o8 i = 0, t[20];
	const o8 *v[3];
	size_t vn[3];

	v[0] = (const o8 *)etiqueta; vn[0] = strlen(etiqueta) + 1;
	v[1] = d; vn[1] = n;
	v[2] = &i; vn[2] = 1;
	while (nsal) {
		size_t c = nsal < 20 ? nsal : 20;
		hmac_sha1_v(k, kn, 3, v, vn, t);
		memcpy(sal, t, c);
		sal += c; nsal -= c; i++;
	}
}

/* -------------------------------------------------------------- AES-128 */
static o8 caja[256], caja_inv[256];
static d32 te[256];		/* columna de MixColumns(SubBytes) */
static int aes_hecha;

static o8 por2(o8 x) { return (x << 1) ^ ((x & 0x80) ? 0x1b : 0); }

static o8 gmul(o8 a, o8 b)
{
	o8 r = 0;
	while (b) {
		if (b & 1)
			r ^= a;
		a = por2(a);
		b >>= 1;
	}
	return r;
}

static void aes_tablas(void)
{
	o8 p = 1, q = 1;
	int i;

	/* la caja S: el inverso en GF(2^8) y la transformación afín */
	do {
		o8 x;
		p = p ^ por2(p);		/* p por 3 */
		q ^= q << 1; q ^= q << 2; q ^= q << 4;
		if (q & 0x80)
			q ^= 0x09;		/* q entre 3 */
		x = q ^ ((q << 1) | (q >> 7)) ^ ((q << 2) | (q >> 6)) ^ ((q << 3) | (q >> 5)) ^ ((q << 4) | (q >> 4));
		caja[p] = x ^ 0x63;
	} while (p != 1);
	caja[0] = 0x63;
	for (i = 0; i < 256; i++) {
		o8 s = caja[i];
		caja_inv[s] = i;
		te[i] = ((d32)por2(s) << 24) | ((d32)s << 16) | ((d32)s << 8) | (d32)(por2(s) ^ s);
	}
	aes_hecha = 1;
}

void aes_clave(struct aes *a, const o8 *k)
{
	d32 t, rc = 1;
	int i;

	if (!aes_hecha)
		aes_tablas();
	for (i = 0; i < 4; i++)
		a->rk[i] = ((d32)k[4 * i] << 24) | (k[4 * i + 1] << 16) | (k[4 * i + 2] << 8) | k[4 * i + 3];
	for (i = 4; i < 44; i++) {
		t = a->rk[i - 1];
		if ((i & 3) == 0) {
			t = (t << 8) | (t >> 24);
			t = ((d32)caja[t >> 24] << 24) | (caja[(t >> 16) & 255] << 16) | (caja[(t >> 8) & 255] << 8) | caja[t & 255];
			t ^= rc << 24;
			rc = por2(rc);
		}
		a->rk[i] = a->rk[i - 4] ^ t;
	}
}

#define ROR8(x) (((x) >> 8) | ((x) << 24))

void aes_cifra(const struct aes *a, const o8 *e, o8 *s)
{
	d32 x[4], y[4];
	const d32 *rk = a->rk;
	int i, r;

	for (i = 0; i < 4; i++)
		x[i] = (((d32)e[4 * i] << 24) | (e[4 * i + 1] << 16) | (e[4 * i + 2] << 8) | e[4 * i + 3]) ^ rk[i];
	for (r = 1; r < 10; r++) {
		rk += 4;
		for (i = 0; i < 4; i++) {
			d32 t0 = te[x[i] >> 24];
			d32 t1 = te[(x[(i + 1) & 3] >> 16) & 255];
			d32 t2 = te[(x[(i + 2) & 3] >> 8) & 255];
			d32 t3 = te[x[(i + 3) & 3] & 255];
			y[i] = t0 ^ ROR8(t1) ^ ROR8(ROR8(t2)) ^ ROR8(ROR8(ROR8(t3))) ^ rk[i];
		}
		x[0] = y[0]; x[1] = y[1]; x[2] = y[2]; x[3] = y[3];
	}
	rk += 4;
	for (i = 0; i < 4; i++) {
		d32 t = ((d32)caja[x[i] >> 24] << 24) | (caja[(x[(i + 1) & 3] >> 16) & 255] << 16) |
			(caja[(x[(i + 2) & 3] >> 8) & 255] << 8) | caja[x[(i + 3) & 3] & 255];
		t ^= rk[i];
		s[4 * i] = t >> 24; s[4 * i + 1] = t >> 16; s[4 * i + 2] = t >> 8; s[4 * i + 3] = t;
	}
}

/* descifrar: solo hace falta para desenvolver la clave de grupo (poco) */
void aes_descifra(const struct aes *a, const o8 *e, o8 *s)
{
	o8 st[16], t[16];
	int i, r, c;

	for (i = 0; i < 16; i++)
		st[i] = e[i] ^ (a->rk[40 + i / 4] >> (24 - 8 * (i & 3)));
	for (r = 9; r >= 0; r--) {
		/* InvShiftRows + InvSubBytes */
		for (c = 0; c < 4; c++)
			for (i = 0; i < 4; i++)
				t[4 * ((c + i) & 3) + i] = caja_inv[st[4 * c + i]];
		for (i = 0; i < 16; i++)
			st[i] = t[i] ^ (a->rk[4 * r + i / 4] >> (24 - 8 * (i & 3)));
		if (r == 0)
			break;
		for (c = 0; c < 4; c++) {
			o8 *p = st + 4 * c, a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
			p[0] = gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9);
			p[1] = gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13);
			p[2] = gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11);
			p[3] = gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14);
		}
	}
	memcpy(s, st, 16);
}

/* envoltorio de claves de AES (RFC 3394); n = octetos de la clave en claro
 * (múltiplo de 8); el envuelto ocupa n + 8 */
void aes_envuelve(const o8 *kek, const o8 *e, int n, o8 *s)
{
	struct aes a;
	o8 b[16];
	int i, j, k = n / 8;

	aes_clave(&a, kek);
	memset(s, 0xA6, 8);
	memcpy(s + 8, e, n);
	for (j = 0; j < 6; j++)
		for (i = 1; i <= k; i++) {
			memcpy(b, s, 8);
			memcpy(b + 8, s + 8 * i, 8);
			aes_cifra(&a, b, b);
			b[7] ^= k * j + i;
			memcpy(s, b, 8);
			memcpy(s + 8 * i, b + 8, 8);
		}
}

int aes_desenvuelve(const o8 *kek, const o8 *e, int n, o8 *s)
{
	struct aes a;
	o8 b[16], iv[8];
	int i, j, k = n / 8;

	aes_clave(&a, kek);
	memcpy(iv, e, 8);
	memmove(s, e + 8, n);
	for (j = 5; j >= 0; j--)
		for (i = k; i >= 1; i--) {
			memcpy(b, iv, 8);
			b[7] ^= k * j + i;
			memcpy(b + 8, s + 8 * (i - 1), 8);
			aes_descifra(&a, b, b);
			memcpy(iv, b, 8);
			memcpy(s + 8 * (i - 1), b + 8, 8);
		}
	for (i = 0; i < 8; i++)
		if (iv[i] != 0xA6)
			return -1;
	return 0;
}

/* ----------------------------------------------------------------- CCMP */
/* CCM con M = 8 y L = 2 (RFC 3610). «cab» es la longitud de la cabecera
 * 802.11 (24, 26 con QoS, 30/32 con cuatro direcciones). */
static void ccm(const struct aes *a, const o8 *nonce, const o8 *aad, int naad,
		o8 *datos, int n, o8 *mic, int descifrar)
{
	o8 x[16], ctr[16], s0[16], b[16];
	int i, k;

	/* B0 y los datos autenticados */
	x[0] = 0x59;
	memcpy(x + 1, nonce, 13);
	x[14] = n >> 8; x[15] = n;
	aes_cifra(a, x, x);
	x[0] ^= naad >> 8; x[1] ^= naad;
	k = 2;
	for (i = 0; i < naad; i++) {
		x[k++] ^= aad[i];
		if (k == 16) {
			aes_cifra(a, x, x);
			k = 0;
		}
	}
	if (k)
		aes_cifra(a, x, x);
	ctr[0] = 0x01;
	memcpy(ctr + 1, nonce, 13);
	ctr[14] = 0; ctr[15] = 0;
	aes_cifra(a, ctr, s0);
	for (i = 0; i < n; i += 16) {
		int c = n - i < 16 ? n - i : 16;
		ctr[14] = (i / 16 + 1) >> 8; ctr[15] = i / 16 + 1;
		aes_cifra(a, ctr, b);
		if (!descifrar)
			for (k = 0; k < c; k++)
				x[k] ^= datos[i + k];
		for (k = 0; k < c; k++)
			datos[i + k] ^= b[k];
		if (descifrar)
			for (k = 0; k < c; k++)
				x[k] ^= datos[i + k];
		aes_cifra(a, x, x);
	}
	for (i = 0; i < 8; i++)
		mic[i] = x[i] ^ s0[i];
}

static int ccmp_aad(const o8 *t, int cab, o8 *aad, o8 *nonce, const o8 *pn)
{
	int qos = (t[0] & 0x8c) == 0x88, a4 = (t[1] & 3) == 3, n;

	aad[0] = t[0] & 0x8f;			/* sin los bits 4 a 6 del subtipo */
	aad[1] = (t[1] & 0xc7) | 0x40;		/* sin reintento, ahorro ni más datos; protegida */
	memcpy(aad + 2, t + 4, 18);		/* las tres direcciones */
	aad[20] = t[22] & 0x0f;			/* solo el número de fragmento */
	aad[21] = 0;
	n = 22;
	if (a4) {
		memcpy(aad + n, t + 24, 6);
		n += 6;
	}
	nonce[0] = 0;
	if (qos) {
		aad[n++] = t[cab - 2] & 0x0f;
		aad[n++] = 0;
		nonce[0] = t[cab - 2] & 0x0f;
	}
	memcpy(nonce + 1, t + 10, 6);		/* A2 */
	nonce[7] = pn[5]; nonce[8] = pn[4]; nonce[9] = pn[3];
	nonce[10] = pn[2]; nonce[11] = pn[1]; nonce[12] = pn[0];
	return n;
}

/* trama: cabecera (cab) + datos; hace falta sitio para 16 octetos más.
 * pn[0] es el octeto bajo. Devuelve la longitud nueva. */
int ccmp_cifra(const struct aes *a, o8 *t, int n, int cab, const o8 *pn, int id)
{
	o8 aad[32], nonce[13];
	int naad, nd = n - cab;

	t[1] |= 0x40;
	naad = ccmp_aad(t, cab, aad, nonce, pn);
	memmove(t + cab + 8, t + cab, nd);
	t[cab] = pn[0]; t[cab + 1] = pn[1]; t[cab + 2] = 0; t[cab + 3] = 0x20 | (id << 6);
	t[cab + 4] = pn[2]; t[cab + 5] = pn[3]; t[cab + 6] = pn[4]; t[cab + 7] = pn[5];
	ccm(a, nonce, aad, naad, t + cab + 8, nd, t + cab + 8 + nd, 0);
	return n + 16;
}

/* Devuelve la longitud sin la cabecera CCMP ni el MIC, o -1 si el MIC no
 * cuadra. pn_sal: el número de paquete (para rechazar las repetidas). */
int ccmp_descifra(const struct aes *a, o8 *t, int n, int cab, o8 *pn)
{
	o8 aad[32], nonce[13], mic[8];
	int naad, nd = n - cab - 16, i, mal = 0;

	if (nd < 0 || !(t[cab + 3] & 0x20))
		return -1;
	pn[0] = t[cab]; pn[1] = t[cab + 1]; pn[2] = t[cab + 4];
	pn[3] = t[cab + 5]; pn[4] = t[cab + 6]; pn[5] = t[cab + 7];
	naad = ccmp_aad(t, cab, aad, nonce, pn);
	ccm(a, nonce, aad, naad, t + cab + 8, nd, mic, 1);
	for (i = 0; i < 8; i++)
		mal |= mic[i] ^ t[cab + 8 + nd + i];
	if (mal)
		return -1;
	memmove(t + cab, t + cab + 8, nd);
	return cab + nd;
}

/* ----------------------------------------------------------- suplicante */
#define KI_VER 0x0007
#define KI_PAR 0x0008
#define KI_INSTALA 0x0040
#define KI_ACUSE 0x0080
#define KI_MIC 0x0100
#define KI_SEGURA 0x0200
#define KI_ERROR 0x0400
#define KI_CIFRADA 0x1000

void wpa_ptk(const o8 *pmk, const o8 *aa, const o8 *spa, const o8 *an, const o8 *sn, o8 *ptk)
{
	o8 d[76];

	if (memcmp(aa, spa, 6) < 0) {
		memcpy(d, aa, 6); memcpy(d + 6, spa, 6);
	} else {
		memcpy(d, spa, 6); memcpy(d + 6, aa, 6);
	}
	if (memcmp(an, sn, 32) < 0) {
		memcpy(d + 12, an, 32); memcpy(d + 44, sn, 32);
	} else {
		memcpy(d + 12, sn, 32); memcpy(d + 44, an, 32);
	}
	prf_sha1(pmk, 32, "Pairwise key expansion", d, 76, ptk, 48);
}

void wpa_empieza(struct wpa *w, const o8 *pmk, const o8 *aa, const o8 *spa, const o8 *rsn, int rsn_n)
{
	memset(w, 0, sizeof(*w));
	memcpy(w->pmk, pmk, 32);
	memcpy(w->aa, aa, 6);
	memcpy(w->spa, spa, 6);
	if (rsn_n > (int)sizeof(w->rsn))
		rsn_n = sizeof(w->rsn);
	memcpy(w->rsn, rsn, rsn_n);
	w->rsn_n = rsn_n;
}

/* monta un EAPOL-Key de respuesta y le pone el MIC */
static int wpa_responde(struct wpa *w, o8 *s, int info, const o8 *rep, const o8 *nonce,
			const o8 *datos, int nd)
{
	o8 mic[20];
	int n = 99 + nd;

	memset(s, 0, n);
	s[0] = 1;			/* 802.1X-2001 */
	s[1] = 3;			/* EAPOL-Key */
	s[2] = (n - 4) >> 8; s[3] = n - 4;
	s[4] = 2;			/* descriptor RSN */
	s[5] = info >> 8; s[6] = info;
	memcpy(s + 9, rep, 8);
	if (nonce)
		memcpy(s + 17, nonce, 32);
	s[97] = nd >> 8; s[98] = nd;
	if (nd)
		memcpy(s + 99, datos, nd);
	hmac_sha1(w->ptk, 16, s, n, mic);
	memcpy(s + 81, mic, 16);
	return n;
}

/* busca la clave de grupo (KDE 00-0F-AC 1) en los datos descifrados; rsc:
 * el contador de repetición de esa clave (los 6 octetos bajos del «Key RSC»
 * del mensaje). La MISMA clave que ya está puesta no se vuelve a poner: así su
 * contador no vuelve a cero y no se aceptan otra vez tramas ya recibidas
 * (la variante del ataque «KRACK» contra la clave de grupo) */
static int wpa_busca_gtk(struct wpa *w, const o8 *d, int n, const o8 *rsc)
{
	int i = 0;

	while (i + 2 <= n) {
		int tipo = d[i], l = d[i + 1];
		if (tipo == 0xdd && l == 0)
			break;			/* relleno */
		if (i + 2 + l > n)
			break;
		if (tipo == 0xdd && l >= 6 + 16 && d[i + 2] == 0x00 && d[i + 3] == 0x0f &&
		    d[i + 4] == 0xac && d[i + 5] == 1) {
			if (l - 6 != 16) {
				hl_log(0, "WPA: la clave de grupo no es de CCMP (%d octetos)\n", l - 6);
				return -1;
			}
			if (w->gtk_puesta && (d[i + 6] & 3) == w->gtk_id && memcmp(w->gtk, d + i + 8, 16) == 0) {
				hl_log(1, "WPA: la misma clave de grupo otra vez: no se vuelve a poner (ni su contador)\n");
				return 0;
			}
			w->gtk_id = d[i + 6] & 3;
			memcpy(w->gtk, d + i + 8, 16);
			aes_clave(&w->gk, w->gtk);
			memcpy(w->pn_rx_g, rsc, 6);
			w->gtk_puesta = 1;
			return 0;
		}
		i += 2 + l;
	}
	hl_log(0, "WPA: el punto de acceso no ha mandado la clave de grupo\n");
	return -1;
}

int wpa_eapol(struct wpa *w, const o8 *e, int n, o8 *sal)
{
	o8 mic[20], copia[512], datos[256];
	int info, nd, i;

	if (n < 99 || e[1] != 3)
		return 0;
	if (e[4] != 2) {
		hl_log(1, "WPA: descriptor de clave %d (no es RSN/WPA2); se ignora\n", e[4]);
		return 0;
	}
	info = (e[5] << 8) | e[6];
	nd = (e[97] << 8) | e[98];
	if (99 + nd > n || nd > (int)sizeof(datos) + 8)
		return 0;
	if ((info & KI_VER) != 2) {
		hl_log(1, "WPA: versión de descriptor %d (se esperaba 2: HMAC-SHA1 y AES)\n", info & KI_VER);
		return 0;
	}
	if (!(info & KI_ACUSE))
		return 0;
	/* contador de repetición: tiene que crecer */
	if (w->hay_rep && memcmp(e + 9, w->repeticion, 8) <= 0 && (info & KI_MIC)) {
		hl_log(1, "WPA: mensaje repetido; se ignora\n");
		return 0;
	}

	if ((info & KI_PAR) && !(info & KI_MIC) && w->paso == 2) {
		/* un paso 1 sin firmar con las claves ya puestas: no (aún no se
		 * renueva la clave por pares: haría falta una PTK nueva aparte, sin
		 * dejar la de ahora hasta el paso 3). (0.8.10, el HP Mini: una
		 * descarga larga por el Wi-Fi se cortaba) Se dice, una vez: si el
		 * punto de acceso la renueva cada tanto, desasocia al no tenerla */
		static int dicho;

		if (!dicho++)
			hl_log(0, "WPA: el punto de acceso pide renovar la clave por pares (un paso 1 con las claves ya puestas): aún no se hace; si luego se corta la conexión, es esto\n");
		return 0;
	}
	if ((info & KI_PAR) && !(info & KI_MIC)) {
		/* mensaje 1: el ANonce. Sacamos el SNonce y la PTK. */
		o8 semilla[32 + 6 + 8];
		d32 r;
		memcpy(w->anonce, e + 17, 32);
		for (i = 0; i < 32; i += 4) {
			r = hl_azar();
			memcpy(semilla + i, &r, 4);
		}
		memcpy(semilla + 32, w->spa, 6);
		r = hl_jiffies(); memcpy(semilla + 38, &r, 4);
		r = hl_azar(); memcpy(semilla + 42, &r, 4);
		prf_sha1(w->pmk, 32, "Init Counter", semilla, sizeof(semilla), w->snonce, 32);
		wpa_ptk(w->pmk, w->aa, w->spa, w->anonce, w->snonce, w->ptk);
		w->paso = 1;
		hl_log(2, "WPA: paso 1 de 4 recibido; se manda el 2\n");
		return wpa_responde(w, sal, 2 | KI_PAR | KI_MIC, e + 9, w->snonce, w->rsn, w->rsn_n);
	}

	/* los demás llevan MIC: comprobarlo con la KCK */
	if (!w->paso || n > (int)sizeof(copia))
		return 0;
	memcpy(copia, e, n);
	memset(copia + 81, 0, 16);
	hmac_sha1(w->ptk, 16, copia, 99 + nd, mic);
	if (memcmp(mic, e + 81, 16) != 0) {
		/* no se corta: puede no venir del punto de acceso (cualquiera puede
		 * mandar un mensaje así). Se cuenta, y si el acuerdo no acaba, se dice */
		w->fallos_mic++;
		hl_log(1, "WPA: un mensaje del acuerdo de claves con el MIC mal: se ignora (%d)\n", w->fallos_mic);
		return 0;
	}
	memcpy(w->repeticion, e + 9, 8);
	w->hay_rep = 1;

	if (info & KI_PAR) {
		/* mensaje 3: la clave de grupo, cifrada con la KEK */
		if (memcmp(e + 17, w->anonce, 32) != 0) {
			hl_log(0, "WPA: el paso 3 trae otro ANonce\n");
			return -1;
		}
		if (!(info & KI_CIFRADA) || nd < 16 || (nd & 7)) {
			hl_log(0, "WPA: el paso 3 no trae los datos cifrados\n");
			return -1;
		}
		if (aes_desenvuelve(w->ptk + 16, e + 99, nd - 8, datos) < 0) {
			hl_log(0, "WPA: no se pueden descifrar los datos del paso 3\n");
			return -1;
		}
		if (w->paso == 2) {
			/* el paso 3 repetido (no llegó nuestro 4): se contesta otra
			 * vez, pero las claves y los contadores NO se vuelven a
			 * poner (si no, se repetirían números de paquete) */
			hl_log(1, "WPA: paso 3 repetido; se repite el 4\n");
			return wpa_responde(w, sal, 2 | KI_PAR | KI_MIC | KI_SEGURA, e + 9, NULL, NULL, 0);
		}
		if (wpa_busca_gtk(w, datos, nd - 8, e + 65) < 0)
			return -1;
		aes_clave(&w->tk, w->ptk + 32);
		memset(w->pn_tx, 0, 6);
		memset(w->pn_rx, 0, 6);
		/* el 4 se manda sin cifrar; las claves valen desde ahora */
		i = wpa_responde(w, sal, 2 | KI_PAR | KI_MIC | KI_SEGURA, e + 9, NULL, NULL, 0);
		w->ptk_puesta = 1;
		w->paso = 2;
		hl_log(2, "WPA: paso 3 de 4 correcto; se manda el 4. Claves puestas (grupo: clave %d)\n", w->gtk_id);
		return i;
	}

	/* renovación de la clave de grupo (1 de 2) */
	if (w->paso != 2 || !(info & KI_CIFRADA) || nd < 16 || (nd & 7))
		return 0;
	if (aes_desenvuelve(w->ptk + 16, e + 99, nd - 8, datos) < 0)
		return 0;
	if (wpa_busca_gtk(w, datos, nd - 8, e + 65) < 0)
		return 0;
	hl_log(2, "WPA: clave de grupo renovada (clave %d)\n", w->gtk_id);
	return wpa_responde(w, sal, 2 | KI_MIC | KI_SEGURA, e + 9, NULL, NULL, 0);
}
