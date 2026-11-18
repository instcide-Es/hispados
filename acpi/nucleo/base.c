/* base.c - Núcleo ACPI de Hispa-486: la memoria que pide ACPICA, el registro
 * (lo que luego es BATERIA.LOG) y las divisiones de 64 bits.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "nucleo.h"

/* ---- memoria: una lista de trozos seguidos; se busca desde donde se dejó ---- */
struct trozo {
	u32 tam;	/* con la cabecera; bit 0: ocupado */
	u32 marca;
};
#define MARCA 0x4D454D48
static struct trozo *m_ini, *m_fin, *m_ronda;
u32 mem_usada, mem_max;

void mem_inicia(void *p, u32 tam)
{
	m_ini = (struct trozo *)(((u32)p + 7) & ~7u);
	tam = (tam - ((u32)m_ini - (u32)p)) & ~7u;
	m_fin = (struct trozo *)((u32)m_ini + tam);
	m_ini->tam = tam;
	m_ini->marca = MARCA;
	m_ronda = m_ini;
	mem_usada = mem_max = 0;
}

static struct trozo *sig(struct trozo *t)
{
	return (struct trozo *)((u32)t + (t->tam & ~1u));
}

void *mem_pide(u32 tam)
{
	struct trozo *t, *s, *lim, *ronda0 = m_ronda;
	int pasada;

	if (!m_ini || tam > 0x40000000)
		return 0;
	tam = ((tam + 7) & ~7u) + sizeof(struct trozo);
	for (pasada = 0; pasada < 2; pasada++) {
		t = pasada ? m_ini : ronda0;
		lim = pasada ? ronda0 : m_fin;
		for (; t < lim; t = sig(t)) {
			if (t->tam & 1)
				continue;
			/* junta los libres que siguen */
			while ((s = sig(t)) < m_fin && !(s->tam & 1))
				t->tam += s->tam;
			if (t->tam < tam)
				continue;
			if (t->tam >= tam + 32) {
				s = (struct trozo *)((u32)t + tam);
				s->tam = t->tam - tam;
				s->marca = MARCA;
				t->tam = tam;
			}
			t->tam |= 1;
			m_ronda = sig(t);
			if (m_ronda >= m_fin)
				m_ronda = m_ini;
			mem_usada += t->tam & ~1u;
			if (mem_usada > mem_max)
				mem_max = mem_usada;
			return t + 1;
		}
	}
	return 0;
}

void mem_suelta(void *p)
{
	struct trozo *t = (struct trozo *)p - 1;

	if (!p)
		return;
	if (t < m_ini || t >= m_fin || t->marca != MARCA || !(t->tam & 1)) {
		reg("MEMORIA: se suelta %p, que no es un trozo ocupado\n", p);
		return;
	}
	t->tam &= ~1u;
	mem_usada -= t->tam;
}

/* ---- el registro: texto seguido; cuando se llena, lo nuevo se pierde (lo
 * que más importa es el principio: las tablas y la primera lectura) ---- */
#define REG_TAM 49152
static char reg_buf[REG_TAM];
static u32 reg_n;
u32 reg_perdidos;
#ifdef ANFITRION
void sim_vuelca(void);
#endif

void reg_v(const char *f, va_list a)
{
	char l[256];
	int n = vsnprintf(l, sizeof l, f, a), i;

	if (n > (int)sizeof l - 1)
		n = sizeof l - 1;
	for (i = 0; i < n; i++) {
		if (reg_n + 2 > REG_TAM) {
			reg_perdidos += n - i;
			return;
		}
		if (l[i] == '\n')
			reg_buf[reg_n++] = '\r';
		reg_buf[reg_n++] = l[i];
	}
#ifdef ANFITRION
	sim_vuelca();		/* en las pruebas, a la salida según se escribe */
#endif
}

void reg(const char *f, ...)
{
	va_list a;

	va_start(a, f);
	reg_v(f, a);
	va_end(a);
}

u32 reg_saca(char *b, u32 tam)
{
	u32 n = reg_n < tam ? reg_n : tam;

	memcpy(b, reg_buf, n);
	memmove(reg_buf, reg_buf + n, reg_n - n);
	reg_n -= n;
	return n;
}

/* ---- divisiones de 64 bits (no hay biblioteca del compilador) ---- */
static u64 divmod64(u64 n, u64 d, u64 *resto)
{
	u64 c = 0, r = 0;
	int i;

	if (!(d >> 32) && d) {
		u32 alto = n >> 32, bajo = n, ca, cb, r32, d32 = d;

		ca = alto / d32;
		r32 = alto % d32;
		__asm__("divl %4" : "=a"(cb), "=d"(r32) : "a"(bajo), "d"(r32), "rm"(d32));
		if (resto)
			*resto = r32;
		return ((u64)ca << 32) | cb;
	}
	if (!d) {
		if (resto)
			*resto = 0;
		return ~0ULL;
	}
	for (i = 63; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d) {
			r -= d;
			c |= 1ULL << i;
		}
	}
	if (resto)
		*resto = r;
	return c;
}
u64 __udivdi3(u64 n, u64 d) { return divmod64(n, d, 0); }
u64 __umoddi3(u64 n, u64 d) { u64 r; divmod64(n, d, &r); return r; }
