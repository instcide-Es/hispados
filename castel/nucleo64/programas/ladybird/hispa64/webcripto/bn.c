/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/webcripto/bn.c - Hispa-64, etapa 10 i: los números grandes (el
 * BIGNUM de OpenSSL, por dentro nuestro) y lo que hace falta de ellos:
 * sumar, restar, multiplicar, dividir (el algoritmo D de Knuth), la potencia
 * modular, el máximo común divisor, el inverso, Miller-Rabin.
 * No son de tiempo constante: lo secreto de verdad (firmar, descifrar,
 * derivar) lo hace BearSSL, que sí lo es; esto es para las comprobaciones de
 * las claves (las mismas que OpenSSL), los puntos comprimidos de EC, sacar
 * los factores de una clave RSA que viene sin ellos (JWK con solo n, e y d)
 * y las claves de más de 4096 bits, que BearSSL no hace.
 * Y las funciones BN_ de OpenSSL que pide el Rust (sys.rs).
 */
#include <stdlib.h>
#include "interno.h"

BIGNUM *h64bn_nuevo(void)
{
	return calloc(1, sizeof(BIGNUM));
}
void h64bn_libera(BIGNUM *a)
{
	if (!a) return;
	if (a->w) { h64_borra(a->w, a->cap * 4); free(a->w); }
	free(a);
}
int h64bn_reserva(BIGNUM *a, size_t cap)
{
	if (cap <= a->cap) return 1;
	uint32_t *w = calloc(cap, 4);
	if (!w) return 0;
	if (a->w) { memcpy(w, a->w, a->n * 4); h64_borra(a->w, a->cap * 4); free(a->w); }
	a->w = w;
	a->cap = cap;
	return 1;
}
static void normaliza(BIGNUM *a)
{
	while (a->n && a->w[a->n - 1] == 0) a->n--;
}
int h64bn_pon(BIGNUM *r, uint32_t v)
{
	if (!h64bn_reserva(r, 1)) return 0;
	r->w[0] = v;
	r->n = v ? 1 : 0;
	return 1;
}
int h64bn_de_octetos(BIGNUM *a, const uint8_t *b, size_t n)
{
	size_t pal = (n + 3) / 4;
	if (!h64bn_reserva(a, pal ? pal : 1)) return 0;
	memset(a->w, 0, a->cap * 4);
	for (size_t i = 0; i < n; i++) {
		size_t j = n - 1 - i;   /* el octeto i desde el final */
		a->w[i / 4] |= (uint32_t)b[j] << (8 * (i % 4));
	}
	a->n = pal;
	normaliza(a);
	return 1;
}
size_t h64bn_bits(const BIGNUM *a)
{
	if (!a->n) return 0;
	uint32_t t = a->w[a->n - 1];
	size_t b = 0;
	while (t) { b++; t >>= 1; }
	return (a->n - 1) * 32 + b;
}
size_t h64bn_octetos(const BIGNUM *a) { return (h64bn_bits(a) + 7) / 8; }
int h64bn_a_octetos(const BIGNUM *a, uint8_t *b, size_t n)
{
	if (h64bn_octetos(a) > n) return 0;
	for (size_t i = 0; i < n; i++) {
		size_t k = i / 4;
		b[n - 1 - i] = k < a->n ? (uint8_t)(a->w[k] >> (8 * (i % 4))) : 0;
	}
	return 1;
}
int h64bn_cmp(const BIGNUM *a, const BIGNUM *b)
{
	if (a->n != b->n) return a->n < b->n ? -1 : 1;
	for (size_t i = a->n; i-- > 0;)
		if (a->w[i] != b->w[i]) return a->w[i] < b->w[i] ? -1 : 1;
	return 0;
}
int h64bn_copia(BIGNUM *r, const BIGNUM *a)
{
	if (r == a) return 1;
	if (!h64bn_reserva(r, a->n ? a->n : 1)) return 0;
	memcpy(r->w, a->w, a->n * 4);
	r->n = a->n;
	return 1;
}
int h64bn_es_cero(const BIGNUM *a) { return a->n == 0; }
int h64bn_es_uno(const BIGNUM *a) { return a->n == 1 && a->w[0] == 1; }
int h64bn_es_impar(const BIGNUM *a) { return a->n && (a->w[0] & 1); }
int h64bn_bit(const BIGNUM *a, size_t i) { return i / 32 < a->n ? (a->w[i / 32] >> (i % 32)) & 1 : 0; }

int h64bn_suma(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	size_t n = (a->n > b->n ? a->n : b->n) + 1;
	uint32_t *t = calloc(n, 4);
	if (!t) return 0;
	uint64_t c = 0;
	for (size_t i = 0; i < n; i++) {
		c += (uint64_t)(i < a->n ? a->w[i] : 0) + (i < b->n ? b->w[i] : 0);
		t[i] = (uint32_t)c;
		c >>= 32;
	}
	int ok = h64bn_reserva(r, n);
	if (ok) { memcpy(r->w, t, n * 4); r->n = n; normaliza(r); }
	free(t);
	return ok;
}
int h64bn_resta(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	if (h64bn_cmp(a, b) < 0) return 0;
	size_t n = a->n;
	if (!h64bn_reserva(r, n ? n : 1)) return 0;
	int64_t c = 0;
	for (size_t i = 0; i < n; i++) {
		c += (int64_t)a->w[i] - (int64_t)(i < b->n ? b->w[i] : 0);
		r->w[i] = (uint32_t)c;
		c = c < 0 ? -1 : 0;
		/* (con la resta en 64 bits con signo, el acarreo es -1 o 0) */
	}
	r->n = n;
	normaliza(r);
	return 1;
}
int h64bn_resta1(BIGNUM *r, const BIGNUM *a)
{
	BIGNUM uno = { (uint32_t[]){ 1 }, 1, 1, 0 };
	return h64bn_resta(r, a, &uno);
}
int h64bn_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	size_t n = a->n + b->n;
	if (!a->n || !b->n) return h64bn_pon(r, 0);
	uint32_t *t = calloc(n, 4);
	if (!t) return 0;
	for (size_t i = 0; i < a->n; i++) {
		uint64_t c = 0;
		for (size_t j = 0; j < b->n; j++) {
			c += (uint64_t)a->w[i] * b->w[j] + t[i + j];
			t[i + j] = (uint32_t)c;
			c >>= 32;
		}
		t[i + b->n] = (uint32_t)c;
	}
	int ok = h64bn_reserva(r, n);
	if (ok) { memcpy(r->w, t, n * 4); r->n = n; normaliza(r); }
	h64_borra(t, n * 4);
	free(t);
	return ok;
}
int h64bn_shl(BIGNUM *r, const BIGNUM *a, size_t bits)
{
	size_t pal = bits / 32, b = bits % 32, n = a->n + pal + 1;
	uint32_t *t = calloc(n, 4);
	if (!t) return 0;
	for (size_t i = 0; i < a->n; i++) {
		t[i + pal] |= a->w[i] << b;
		if (b) t[i + pal + 1] |= a->w[i] >> (32 - b);
	}
	int ok = h64bn_reserva(r, n);
	if (ok) { memcpy(r->w, t, n * 4); r->n = n; normaliza(r); }
	free(t);
	return ok;
}
int h64bn_shr(BIGNUM *r, const BIGNUM *a, size_t bits)
{
	size_t pal = bits / 32, b = bits % 32;
	if (pal >= a->n) return h64bn_pon(r, 0);
	size_t n = a->n - pal;
	uint32_t *t = calloc(n, 4);
	if (!t) return 0;
	for (size_t i = 0; i < n; i++) {
		t[i] = a->w[i + pal] >> b;
		if (b && i + pal + 1 < a->n) t[i] |= a->w[i + pal + 1] << (32 - b);
	}
	int ok = h64bn_reserva(r, n);
	if (ok) { memcpy(r->w, t, n * 4); r->n = n; normaliza(r); }
	free(t);
	return ok;
}

/* la división (Knuth, TAOCP vol. 2, 4.3.1, algoritmo D): a = q·m + r */
int h64bn_divmod(BIGNUM *q, BIGNUM *r, const BIGNUM *a, const BIGNUM *m)
{
	if (!m->n) return 0;
	if (h64bn_cmp(a, m) < 0) {
		if (r && !h64bn_copia(r, a)) return 0;
		if (q && !h64bn_pon(q, 0)) return 0;
		return 1;
	}
	size_t n = m->n, mm = a->n - n;
	if (n == 1) {
		uint64_t rem = 0;
		uint32_t *t = calloc(a->n, 4);
		if (!t) return 0;
		for (size_t i = a->n; i-- > 0;) {
			uint64_t cur = (rem << 32) | a->w[i];
			t[i] = (uint32_t)(cur / m->w[0]);
			rem = cur % m->w[0];
		}
		int ok = 1;
		if (q) { ok = h64bn_reserva(q, a->n); if (ok) { memcpy(q->w, t, a->n * 4); q->n = a->n; normaliza(q); } }
		free(t);
		if (ok && r) ok = h64bn_pon(r, (uint32_t)rem);
		return ok;
	}
	/* D1: normalizar (que la palabra alta del divisor tenga el bit alto) */
	unsigned s = 0;
	uint32_t top = m->w[n - 1];
	while (!(top & 0x80000000u)) { top <<= 1; s++; }
	uint32_t *u = calloc(a->n + 1, 4), *v = calloc(n, 4), *qq = calloc(mm + 1, 4);
	if (!u || !v || !qq) { free(u); free(v); free(qq); return 0; }
	for (size_t i = n; i-- > 0;)
		v[i] = (m->w[i] << s) | (s && i ? m->w[i - 1] >> (32 - s) : 0);
	u[a->n] = s ? a->w[a->n - 1] >> (32 - s) : 0;
	for (size_t i = a->n; i-- > 0;)
		u[i] = (a->w[i] << s) | (s && i ? a->w[i - 1] >> (32 - s) : 0);
	/* D2..D7 */
	for (size_t j = mm + 1; j-- > 0;) {
		uint64_t num = ((uint64_t)u[j + n] << 32) | u[j + n - 1];
		uint64_t qh = num / v[n - 1], rh = num % v[n - 1];
		while (qh >= ((uint64_t)1 << 32) || qh * v[n - 2] > ((rh << 32) | u[j + n - 2])) {
			qh--;
			rh += v[n - 1];
			if (rh >= ((uint64_t)1 << 32)) break;
		}
		/* D4: u[j..j+n] -= qh·v */
		int64_t borrow = 0;
		uint64_t carry = 0;
		for (size_t i = 0; i < n; i++) {
			uint64_t p = qh * v[i] + carry;
			carry = p >> 32;
			int64_t t = (int64_t)u[i + j] - borrow - (int64_t)(uint32_t)p;
			u[i + j] = (uint32_t)t;
			borrow = t < 0 ? 1 : 0;
		}
		int64_t t = (int64_t)u[j + n] - borrow - (int64_t)carry;
		u[j + n] = (uint32_t)t;
		if (t < 0) {
			/* D6: se ha pasado; se suma una vez el divisor */
			qh--;
			uint64_t c = 0;
			for (size_t i = 0; i < n; i++) {
				c += (uint64_t)u[i + j] + v[i];
				u[i + j] = (uint32_t)c;
				c >>= 32;
			}
			u[j + n] += (uint32_t)c;
		}
		qq[j] = (uint32_t)qh;
	}
	int ok = 1;
	if (q) { ok = h64bn_reserva(q, mm + 1); if (ok) { memcpy(q->w, qq, (mm + 1) * 4); q->n = mm + 1; normaliza(q); } }
	if (ok && r) {
		/* D8: el resto, desnormalizado */
		ok = h64bn_reserva(r, n);
		if (ok) {
			for (size_t i = 0; i < n; i++)
				r->w[i] = (u[i] >> s) | (s ? u[i + 1] << (32 - s) : 0);
			r->n = n;
			normaliza(r);
		}
	}
	h64_borra(u, (a->n + 1) * 4);
	free(u); free(v); free(qq);
	return ok;
}
int h64bn_mod(BIGNUM *r, const BIGNUM *a, const BIGNUM *m) { return h64bn_divmod(NULL, r, a, m); }
int h64bn_modmul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m)
{
	BIGNUM *t = h64bn_nuevo();
	int ok = t && h64bn_mul(t, a, b) && h64bn_mod(r, t, m);
	h64bn_libera(t);
	return ok;
}
int h64bn_modexp(BIGNUM *r, const BIGNUM *b, const BIGNUM *e, const BIGNUM *m)
{
	if (!m->n) return 0;
	BIGNUM *x = h64bn_nuevo(), *base = h64bn_nuevo();
	int ok = x && base && h64bn_pon(x, 1) && h64bn_mod(x, x, m) && h64bn_mod(base, b, m);
	for (size_t i = h64bn_bits(e); ok && i-- > 0;) {
		ok = h64bn_modmul(x, x, x, m);
		if (ok && h64bn_bit(e, i)) ok = h64bn_modmul(x, x, base, m);
	}
	if (ok) ok = h64bn_copia(r, x);
	h64bn_libera(x);
	h64bn_libera(base);
	return ok;
}
int h64bn_mcd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	BIGNUM *x = h64bn_nuevo(), *y = h64bn_nuevo(), *t = h64bn_nuevo();
	int ok = x && y && t && h64bn_copia(x, a) && h64bn_copia(y, b);
	while (ok && !h64bn_es_cero(y)) {
		ok = h64bn_mod(t, x, y);
		BIGNUM *z = x; x = y; y = t; t = z;
	}
	if (ok) ok = h64bn_copia(r, x);
	h64bn_libera(x); h64bn_libera(y); h64bn_libera(t);
	return ok;
}
/* el inverso de a módulo m (Euclides extendido, con los coeficientes módulo m) */
int h64bn_modinv(BIGNUM *r, const BIGNUM *a, const BIGNUM *m)
{
	/* r0 = m, r1 = a mod m; t0 = 0, t1 = 1; t_{i+1} = t_{i-1} - q·t_i (mod m) */
	BIGNUM *r0 = h64bn_nuevo(), *r1 = h64bn_nuevo(), *t0 = h64bn_nuevo(), *t1 = h64bn_nuevo();
	BIGNUM *q = h64bn_nuevo(), *rr = h64bn_nuevo(), *tt = h64bn_nuevo(), *qt = h64bn_nuevo();
	int ok = r0 && r1 && t0 && t1 && q && rr && tt && qt && h64bn_copia(r0, m) && h64bn_mod(r1, a, m)
		&& h64bn_pon(t0, 0) && h64bn_pon(t1, 1);
	int hay = 0;
	while (ok && !h64bn_es_cero(r1)) {
		ok = h64bn_divmod(q, rr, r0, r1) && h64bn_modmul(qt, q, t1, m);
		if (!ok) break;
		/* tt = t0 - qt mod m */
		if (h64bn_cmp(t0, qt) >= 0) ok = h64bn_resta(tt, t0, qt);
		else ok = h64bn_resta(tt, qt, t0) && (h64bn_es_cero(tt) || h64bn_resta(tt, m, tt));
		BIGNUM *z;
		z = r0; r0 = r1; r1 = rr; rr = z;
		z = t0; t0 = t1; t1 = tt; tt = z;
	}
	if (ok && h64bn_es_uno(r0)) { ok = h64bn_copia(r, t0); hay = ok; }
	h64bn_libera(r0); h64bn_libera(r1); h64bn_libera(t0); h64bn_libera(t1);
	h64bn_libera(q); h64bn_libera(rr); h64bn_libera(tt); h64bn_libera(qt);
	return hay;
}
int h64bn_azar_menor(BIGNUM *r, const BIGNUM *m)
{
	size_t n = h64bn_octetos(m);
	if (n == 0) return 0;
	uint8_t b[1100];
	if (n > sizeof b) return 0;
	for (int intento = 0; intento < 1000; intento++) {
		if (!h64_azar(b, n)) return 0;
		/* lo que pase de los bits de m, fuera */
		size_t bits = h64bn_bits(m) % 8;
		if (bits) b[0] &= (uint8_t)((1u << bits) - 1);
		if (!h64bn_de_octetos(r, b, n)) return 0;
		if (!h64bn_es_cero(r) && h64bn_cmp(r, m) < 0) { h64_borra(b, n); return 1; }
	}
	return 0;
}

/* los primos menores que 752 (lo que mira OpenSSL de un módulo RSA: que no
 * tenga factores pequeños) */
static int primo_peq(unsigned v)
{
	if (v < 2) return 0;
	for (unsigned d = 2; d * d <= v; d++) if (v % d == 0) return 0;
	return 1;
}
static uint32_t mod_peq(const BIGNUM *a, uint32_t d)
{
	uint64_t r = 0;
	for (size_t i = a->n; i-- > 0;) r = ((r << 32) | a->w[i]) % d;
	return (uint32_t)r;
}
int h64bn_factor_pequeno(const BIGNUM *a)
{
	for (unsigned p = 2; p < 752; p++)
		if (primo_peq(p) && mod_peq(a, p) == 0) return 1;
	return 0;
}
int h64bn_primo(const BIGNUM *a, int rondas)
{
	if (a->n == 1 && a->w[0] < 752) return primo_peq(a->w[0]);
	if (!h64bn_es_impar(a) || h64bn_factor_pequeno(a)) return 0;
	BIGNUM *a1 = h64bn_nuevo(), *d = h64bn_nuevo(), *b = h64bn_nuevo(), *x = h64bn_nuevo(), *a3 = h64bn_nuevo();
	int ok = a1 && d && b && x && a3 && h64bn_resta1(a1, a), res = ok;
	size_t s = 0;
	while (ok && !h64bn_bit(a1, s)) s++;
	ok = ok && h64bn_shr(d, a1, s) && h64bn_resta1(a3, a1);
	for (int i = 0; ok && res && i < rondas; i++) {
		/* la base, en [2, a-2] */
		ok = h64bn_azar_menor(b, a3);
		if (!ok) break;
		if (h64bn_es_uno(b)) { ok = h64bn_pon(b, 2); }
		ok = ok && h64bn_modexp(x, b, d, a);
		if (!ok || h64bn_es_uno(x) || !h64bn_cmp(x, a1)) continue;
		int compuesto = 1;
		for (size_t j = 1; ok && j < s; j++) {
			ok = h64bn_modmul(x, x, x, a);
			if (!h64bn_cmp(x, a1)) { compuesto = 0; break; }
			if (h64bn_es_uno(x)) break;
		}
		if (compuesto) res = 0;
	}
	h64bn_libera(a1); h64bn_libera(d); h64bn_libera(b); h64bn_libera(x); h64bn_libera(a3);
	return ok && res;
}

/* ---- lo de OpenSSL (sys.rs) ---- */
typedef struct bignum_ctx { int nada; } BN_CTX;
H64_VISIBLE BIGNUM *BN_new(void) { return h64bn_nuevo(); }
H64_VISIBLE BIGNUM *BN_secure_new(void)
{
	BIGNUM *a = h64bn_nuevo();
	if (a) a->secreto = 1;
	return a;
}
H64_VISIBLE void BN_clear_free(BIGNUM *a) { h64bn_libera(a); }
H64_VISIBLE BIGNUM *BN_bin2bn(const uint8_t *b, int n, BIGNUM *r)
{
	if (n < 0) return NULL;
	BIGNUM *a = r ? r : h64bn_nuevo();
	if (!a) return NULL;
	if (!h64bn_de_octetos(a, b, (size_t)n)) { if (!r) h64bn_libera(a); return NULL; }
	return a;
}
H64_VISIBLE int BN_bn2binpad(const BIGNUM *a, uint8_t *b, int n)
{
	if (n < 0 || !h64bn_a_octetos(a, b, (size_t)n)) return -1;
	return n;
}
H64_VISIBLE int BN_num_bits(const BIGNUM *a) { return (int)h64bn_bits(a); }
H64_VISIBLE int BN_is_odd(const BIGNUM *a) { return h64bn_es_impar(a); }
H64_VISIBLE int BN_cmp(const BIGNUM *a, const BIGNUM *b) { return h64bn_cmp(a, b); }
H64_VISIBLE int BN_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b) { return h64bn_suma(r, a, b); }
static uint32_t uno_w[1] = { 1 };
static const BIGNUM uno_bn = { uno_w, 1, 1, 0 };
H64_VISIBLE const BIGNUM *BN_value_one(void) { return &uno_bn; }
H64_VISIBLE BN_CTX *BN_CTX_new(void) { return calloc(1, sizeof(BN_CTX)); }
H64_VISIBLE void BN_CTX_free(BN_CTX *c) { free(c); }
