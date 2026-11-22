/*
 * base.c - El suelo del núcleo Wi-Fi (HSPWIFI.BIN): corre en modo protegido
 * de 32 bits, sin sistema debajo. Aquí están la memoria, los textos, el
 * registro, el tiempo (por el contador TSC), los trabajos aplazados, los
 * paquetes (sk_buff), el PCI por sus puertos y el microcódigo.
 *
 * Con -DANFITRION se compila para la prueba del anfitrión (un programa de
 * Linux de 32 bits sin biblioteca: tests/wifi/chip_sim.c).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "hispa_linux.h"
#include "base.h"

u32 hl_base;

/* ------------------------------------------------------- textos y memoria */
void *memcpy(void *d, const void *s, size_t n)
{
	u8 *a = d;
	const u8 *b = s;
	while (n--)
		*a++ = *b++;
	return d;
}

void *memmove(void *d, const void *s, size_t n)
{
	u8 *a = d;
	const u8 *b = s;
	if (a <= b || a >= b + n) {
		while (n--)
			*a++ = *b++;
	} else {
		a += n; b += n;
		while (n--)
			*--a = *--b;
	}
	return d;
}

void *memset(void *d, int c, size_t n)
{
	u8 *a = d;
	while (n--)
		*a++ = c;
	return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
	const u8 *x = a, *y = b;
	for (; n; n--, x++, y++)
		if (*x != *y)
			return *x - *y;
	return 0;
}

size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (u8)*a - (u8)*b; }
int strncmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++) {
		if (*a != *b)
			return (u8)*a - (u8)*b;
		if (!*a)
			break;
	}
	return 0;
}
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }
char *strncpy(char *d, const char *s, size_t n)
{
	size_t i;
	for (i = 0; i < n && s[i]; i++)
		d[i] = s[i];
	for (; i < n; i++)
		d[i] = 0;
	return d;
}
size_t strlcpy(char *d, const char *s, size_t n)
{
	size_t l = strlen(s);
	if (n) {
		size_t c = l >= n ? n - 1 : l;
		memcpy(d, s, c);
		d[c] = 0;
	}
	return l;
}
char *strchr(const char *s, int c) { for (; *s; s++) if (*s == c) return (char *)s; return NULL; }
char *strrchr(const char *s, int c) { const char *r = NULL; for (; *s; s++) if (*s == c) r = s; return (char *)r; }
unsigned long simple_strtoul(const char *s, char **e, unsigned int base)
{
	unsigned long v = 0;
	if (!base) {
		base = 10;
		if (s[0] == '0' && (s[1] | 32) == 'x') { base = 16; s += 2; }
	}
	for (;; s++) {
		int d = *s >= '0' && *s <= '9' ? *s - '0' : (*s | 32) >= 'a' && (*s | 32) <= 'f' ? (*s | 32) - 'a' + 10 : 99;
		if (d >= (int)base)
			break;
		v = v * base + d;
	}
	if (e)
		*e = (char *)s;
	return v;
}

/* divisiones de 64 bits (no hay biblioteca del compilador) */
u64 hl_div64(u64 n, u32 d, u32 *resto)
{
	u32 alto = n >> 32, bajo = n, ca, cb, r;

	ca = alto / d;
	r = alto % d;
	__asm__("divl %4" : "=a"(cb), "=d"(r) : "a"(bajo), "d"(r), "rm"(d));
	if (resto)
		*resto = r;
	return ((u64)ca << 32) | cb;
}

static u64 divmod64(u64 n, u64 d, u64 *resto)
{
	u64 c = 0, r = 0;
	int i;

	if (!(d >> 32)) {
		u32 r32;
		c = hl_div64(n, (u32)d, &r32);
		if (resto)
			*resto = r32;
		return c;
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
u64 __udivdi3(u64 n, u64 d) { return divmod64(n, d, NULL); }
u64 __umoddi3(u64 n, u64 d) { u64 r; divmod64(n, d, &r); return r; }
s64 __divdi3(s64 n, s64 d)
{
	int neg = (n < 0) != (d < 0);
	u64 c = divmod64(n < 0 ? -n : n, d < 0 ? -d : d, NULL);
	return neg ? -(s64)c : (s64)c;
}
s64 __moddi3(s64 n, s64 d)
{
	u64 r;
	divmod64(n < 0 ? -n : n, d < 0 ? -d : d, &r);
	return n < 0 ? -(s64)r : (s64)r;
}
int __popcountsi2(u32 x) { int n = 0; for (; x; x &= x - 1) n++; return n; }
int __ctzsi2(u32 x) { int n = 0; if (!x) return 32; while (!(x & 1)) { x >>= 1; n++; } return n; }
int __clzsi2(u32 x) { int n = 0; if (!x) return 32; while (!(x & 0x80000000u)) { x <<= 1; n++; } return n; }
int __ffssi2(u32 x) { return x ? __ctzsi2(x) + 1 : 0; }
u32 __bswapsi2(u32 x) { return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24); }

u32 hl_isqrt(u32 x)
{
	u32 r = 0, b = 1u << 30;
	while (b > x)
		b >>= 2;
	while (b) {
		if (x >= r + b) {
			x -= r + b;
			r = (r >> 1) + b;
		} else {
			r >>= 1;
		}
		b >>= 2;
	}
	return r;
}

u32 crc32_le(u32 crc, const unsigned char *p, size_t n)
{
	int i;
	while (n--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	return crc;
}

static u32 semilla = 0x48535046;
static inline u64 tsc(void)
{
	u32 a, d;
	__asm__ __volatile__("rdtsc" : "=a"(a), "=d"(d));
	return ((u64)d << 32) | a;
}
u32 prandom_u32(void)
{
	semilla ^= (u32)tsc();
	semilla ^= semilla << 13; semilla ^= semilla >> 17; semilla ^= semilla << 5;
	return semilla;
}
#if defined(HL_PENTIUM) && !defined(ANFITRION)
/* (USUARIOS) en Hispa-Pentium, del pozo del azar del núcleo (pozo.cas, la
 * llamada 210: castel/nucleo32/wifi/servicio.c): las claves de WPA2 (la
 * SNonce) y la de WIFI.LLV; con un núcleo de antes, lo de siempre */
int hl_pentium_azar(u32 *v);
u32 hl_azar(void)
{
	u32 v;

	if (hl_pentium_azar(&v))
		return v;
	return prandom_u32();
}
#else
u32 hl_azar(void) { return prandom_u32(); }
#endif

/* ---------------------------------------------------------------- montón */
struct trozo { u32 tam; u32 libre; struct trozo *sig, *ant; };	/* 16 octetos */
/* «libre» lleva además una marca: si un trozo se sale de lo suyo y pisa la
 * cabecera del siguiente, se nota y se dice (en vez de fallar más tarde) */
#define T_LIBRE 0x4c494201u
#define T_USADO 0x55534100u
static int monton_roto;
static int trozo_bien(struct trozo *t, const char *donde)
{
	if (t->libre == T_LIBRE || t->libre == T_USADO)
		return 1;
	if (!monton_roto++)
		hl_log(0, "MEMORIA: el montón está dañado en %p (%s): algo ha escrito fuera de su sitio\n", t, donde);
	return 0;
}
static struct trozo *monton;
static u32 monton_tam, monton_usado, monton_pico;

void hl_monton(void *ini, u32 tam)
{
	monton = (struct trozo *)(((u32)ini + 15) & ~15u);
	tam = (tam - ((u32)monton - (u32)ini)) & ~15u;
	monton->tam = tam - sizeof(struct trozo);
	monton->libre = T_LIBRE;
	monton->sig = monton->ant = NULL;
	monton_tam = tam;
	monton_usado = monton_pico = 0;
}

void *hl_malloc(size_t n)
{
	struct trozo *t;

	n = (n + 15) & ~15u;
	if (!n)
		n = 16;
	for (t = monton; t; t = t->sig) {
		if (!trozo_bien(t, "al pedir"))
			return NULL;
		if (t->libre != T_LIBRE || t->tam < n)
			continue;
		if (t->tam >= n + sizeof(struct trozo) + 32) {
			struct trozo *r = (struct trozo *)((u8 *)(t + 1) + n);
			r->tam = t->tam - n - sizeof(struct trozo);
			r->libre = T_LIBRE;
			r->sig = t->sig; r->ant = t;
			if (t->sig)
				t->sig->ant = r;
			t->sig = r;
			t->tam = n;
		}
		t->libre = T_USADO;
		monton_usado += t->tam;
		if (monton_usado > monton_pico)
			monton_pico = monton_usado;
		return t + 1;
	}
	hl_log(0, "MEMORIA: no quedan %u octetos (usados %u de %u)\n", (u32)n, monton_usado, monton_tam);
	return NULL;
}

void *hl_zalloc(size_t n)
{
	void *p = hl_malloc(n);
	if (p)
		memset(p, 0, n);
	return p;
}

void hl_free(const void *p)
{
	struct trozo *t;

	if (!p)
		return;
	t = (struct trozo *)p - 1;
	if (!trozo_bien(t, "al soltar"))
		return;
	if (t->libre == T_LIBRE) {
		hl_log(0, "MEMORIA: se suelta dos veces %p\n", p);
		return;
	}
	if (t->sig && !trozo_bien(t->sig, "detrás de lo que se suelta"))
		return;
	t->libre = T_LIBRE;
	monton_usado -= t->tam;
	if (t->sig && t->sig->libre == T_LIBRE) {
		t->tam += sizeof(struct trozo) + t->sig->tam;
		t->sig = t->sig->sig;
		if (t->sig)
			t->sig->ant = t;
	}
	if (t->ant && t->ant->libre == T_LIBRE) {
		t->ant->tam += sizeof(struct trozo) + t->tam;
		t->ant->sig = t->sig;
		if (t->sig)
			t->sig->ant = t->ant;
	}
}

void hl_monton_info(u32 *tam, u32 *usado, u32 *pico) { *tam = monton_tam; *usado = monton_usado; *pico = monton_pico; }

/* repasar el montón entero (las cabeceras, una a una): 0 si está bien */
int hl_monton_repasa(void)
{
	struct trozo *t;

	for (t = monton; t; t = t->sig) {
		if (!trozo_bien(t, "al repasar"))
			return -1;
		if (t->sig && (u8 *)t->sig != (u8 *)(t + 1) + t->tam) {
			if (!monton_roto++)
				hl_log(0, "MEMORIA: el montón está dañado en %p (los trozos no casan)\n", t);
			return -1;
		}
	}
	return monton_roto ? -1 : 0;
}

void *kmemdup(const void *s, size_t n, gfp_t g)
{
	void *p = hl_malloc(n);
	if (p)
		memcpy(p, s, n);
	return p;
}

/* Memoria para DMA: a cero y con la dirección FÍSICA alineada a 8 KB (lo que
 * piden los anillos de la tarjeta). El puntero queda alineado igual porque
 * la base del núcleo está alineada a 64 KB (lo comprueba el arranque). */
#define DMA_ALIN 8192u
void *dma_alloc_coherent(struct device *d, size_t n, dma_addr_t *pa, gfp_t g)
{
	u8 *bruto = hl_malloc(n + DMA_ALIN + 8), *p;

	if (!bruto)
		return NULL;
	p = (u8 *)((hl_fis(bruto) + 8 + DMA_ALIN - 1) & ~(DMA_ALIN - 1));
	p = hl_virt((u32)p);
	((void **)p)[-1] = bruto;
	memset(p, 0, n);
	*pa = hl_fis(p);
	return p;
}

void dma_free_coherent(struct device *d, size_t n, void *v, dma_addr_t pa)
{
	if (v)
		hl_free(((void **)v)[-1]);
}

/* -------------------------------------------------------------- registro */
#define REG_TAM (96 * 1024)
static char reg[REG_TAM];
static u32 reg_w, reg_r;	/* contadores totales (la posición, módulo el tamaño) */
int hl_nivel = 2;
int hl_pantalla;
static int col;

static void pinta(char c)
{
#if !defined(ANFITRION) && !defined(HL_PENTIUM)	/* (PENTIUM) sin pantalla de texto a la vista */
	/* la última línea de la pantalla de texto, subiendo las demás: se ve
	 * aunque el equipo se quede colgado dentro del núcleo. Las columnas,
	 * las filas y dónde empieza la página que se ve, de los datos de la
	 * BIOS (40:4A, 40:84 y 40:4E): no siempre es 80x25 desde B800:0 */
	const u8 *bda = hl_virt(0x400);
	int cols = bda[0x4a] | bda[0x4b] << 8, filas = bda[0x84] + 1;
	int des = bda[0x4e] | bda[0x4f] << 8, i;
	u16 *v;

	if (cols < 40 || cols > 132)
		cols = 80;
	if (filas < 25 || filas > 60)
		filas = 25;
	if (des + cols * filas * 2 > 0x8000)
		des = 0;
	v = hl_virt(0xB8000 + des);
	if (c == '\n' || col >= cols) {
		for (i = 0; i < cols * (filas - 1); i++)
			v[i] = v[i + cols];
		for (i = cols * (filas - 1); i < cols * filas; i++)
			v[i] = 0x0720;
		col = 0;
		if (c == '\n')
			return;
	}
	v[cols * (filas - 1) + col++] = 0x0700 | (u8)c;
#endif
}

#ifdef HL_PENTIUM
void hl_letra_pentium(char c);	/* (PENTIUM) servicio.c: al puerto serie y a WIFI.LOG */
#endif
void hl_reg_c(char c)
{
#ifdef HL_PENTIUM
	hl_letra_pentium(c);
#endif
	reg[reg_w % REG_TAM] = c;
	reg_w++;
	if (reg_w - reg_r > REG_TAM)
		reg_r = reg_w - REG_TAM;
	if (hl_pantalla)
		pinta(c);
}

int hl_reg_lee(char *b, int max)
{
	int n = 0;
	while (n < max && reg_r != reg_w) {
		b[n++] = reg[reg_r % REG_TAM];
		reg_r++;
	}
	return n;
}

int hl_reg_hay(void) { return reg_r != reg_w; }

/* de UTF-8 (los fuentes) a la página 437 (la de DOS) */
static int a_cp437(u32 u)
{
	static const u16 t[] = {
		0xe1, 0xa0, 0xe9, 0x82, 0xed, 0xa1, 0xf3, 0xa2, 0xfa, 0xa3, 0xf1, 0xa4, 0xd1, 0xa5,
		0xfc, 0x81, 0xdc, 0x9a, 0xbf, 0xa8, 0xa1, 0xad, 0xab, 0xae, 0xbb, 0xaf, 0xc9, 0x90,
		0xba, 0xa7, 0xaa, 0xa6, 0xe7, 0x87, 0xc7, 0x80, 0xc1, 'A', 0xcd, 'I', 0xd3, 'O', 0xda, 'U',
		0xe0, 0x85, 0xe8, 0x8a, 0xf2, 0x95, 0xb7, 0xfa, 0
	};
	int i;
	if (u < 128)
		return u;
	for (i = 0; t[i]; i += 2)
		if (t[i] == u)
			return t[i + 1];
	return '?';
}

static int principio = 1;

/* acabar la línea que esté a medias (los avisos de Linux sin fin de línea) */
void hl_linea(void)
{
	if (!principio) {
		hl_reg_c('\r');
		hl_reg_c('\n');
		principio = 1;
	}
}

static void reg_texto(const char *s)
{
	while (*s) {
		u32 u = (u8)*s++;
		if (u >= 0xc0 && u < 0xe0 && (*s & 0xc0) == 0x80)
			u = ((u & 0x1f) << 6) | (*s++ & 0x3f);
		else if (u >= 0xe0 && (s[0] & 0xc0) == 0x80 && (s[1] & 0xc0) == 0x80) {
			u = 0x10000;
			s += 2;
		}
		if (principio) {
			/* la hora de cada línea, en milésimas */
			char h[16];
			int i;
			snprintf(h, sizeof(h), "%7lu ", hl_jiffies());
			for (i = 0; h[i]; i++)
				hl_reg_c(h[i]);
			principio = 0;
		}
		if (u == '\n') {
			hl_reg_c('\r');
			principio = 1;
		}
		hl_reg_c(a_cp437(u));
	}
}

static void pon_num(char **b, char *fin, u64 v, int base, int ancho, int cero, int izq, int mayus, int neg)
{
	char t[24];
	int n = 0, i;

	do {
		u32 r;
		v = hl_div64(v, base, &r);
		t[n++] = r < 10 ? '0' + r : (mayus ? 'A' : 'a') + r - 10;
	} while (v);
	if (neg)
		ancho--;
	if (!izq && !cero)
		for (i = n; i < ancho; i++)
			if (*b < fin) *(*b)++ = ' ';
	if (neg)
		if (*b < fin) *(*b)++ = '-';
	if (!izq && cero)
		for (i = n; i < ancho; i++)
			if (*b < fin) *(*b)++ = '0';
	for (i = n - 1; i >= 0; i--)
		if (*b < fin) *(*b)++ = t[i];
	if (izq)
		for (i = n; i < ancho; i++)
			if (*b < fin) *(*b)++ = ' ';
}

int vsnprintf(char *buf, size_t tam, const char *f, va_list ap)
{
	char *b = buf, *fin = buf + (tam ? tam - 1 : 0);

	for (; *f; f++) {
		int ancho = 0, cero = 0, izq = 0, largo = 0, prec = -1;
		if (*f != '%') {
			if (b < fin) *b++ = *f;
			continue;
		}
		f++;
		for (;; f++) {
			if (*f == '-') izq = 1;
			else if (*f == '0') cero = 1;
			else if (*f == '#' || *f == ' ' || *f == '+') ;
			else break;
		}
		if (*f == '*') { ancho = va_arg(ap, int); f++; }
		while (*f >= '0' && *f <= '9')
			ancho = ancho * 10 + *f++ - '0';
		if (*f == '.') {
			f++;
			prec = 0;
			if (*f == '*') { prec = va_arg(ap, int); f++; }
			while (*f >= '0' && *f <= '9')
				prec = prec * 10 + *f++ - '0';
		}
		while (*f == 'l' || *f == 'h' || *f == 'z' || *f == 'L') {
			if (*f == 'l')
				largo++;
			f++;
		}
		switch (*f) {
		case 'd':
		case 'i': {
			s64 v = largo >= 2 ? va_arg(ap, s64) : va_arg(ap, int);
			pon_num(&b, fin, v < 0 ? -v : v, 10, ancho, cero, izq, 0, v < 0);
			break;
		}
		case 'u':
		case 'x':
		case 'X': {
			u64 v = largo >= 2 ? va_arg(ap, u64) : va_arg(ap, unsigned int);
			pon_num(&b, fin, v, *f == 'u' ? 10 : 16, ancho, cero, izq, *f == 'X', 0);
			break;
		}
		case 'p': {
			void *p = va_arg(ap, void *);
			if (f[1] == 'M') {
				const u8 *m = p;
				int i;
				f++;
				for (i = 0; i < 6; i++) {
					pon_num(&b, fin, m[i], 16, 2, 1, 0, 0, 0);
					if (i < 5 && b < fin) *b++ = ':';
				}
			} else {
				pon_num(&b, fin, (u32)p, 16, 8, 1, 0, 0, 0);
			}
			break;
		}
		case 's': {
			const char *s = va_arg(ap, const char *);
			int n, i;
			if (!s)
				s = "(nulo)";
			n = strlen(s);
			if (prec >= 0 && n > prec)
				n = prec;
			if (!izq)
				for (i = n; i < ancho; i++)
					if (b < fin) *b++ = ' ';
			for (i = 0; i < n; i++)
				if (b < fin) *b++ = s[i];
			if (izq)
				for (i = n; i < ancho; i++)
					if (b < fin) *b++ = ' ';
			break;
		}
		case 'c':
			if (b < fin) *b++ = va_arg(ap, int);
			break;
		case '%':
			if (b < fin) *b++ = '%';
			break;
		default:
			if (b < fin) *b++ = '%';
			if (*f && b < fin) *b++ = *f;
			if (!*f)
				f--;
		}
	}
	if (tam)
		*b = 0;
	return b - buf;
}

int snprintf(char *b, size_t n, const char *f, ...)
{
	va_list ap;
	int r;
	va_start(ap, f);
	r = vsnprintf(b, n, f, ap);
	va_end(ap);
	return r;
}
int scnprintf(char *b, size_t n, const char *f, ...)
{
	va_list ap;
	int r;
	va_start(ap, f);
	r = vsnprintf(b, n, f, ap);
	va_end(ap);
	return r;
}
int sprintf(char *b, const char *f, ...)
{
	va_list ap;
	int r;
	va_start(ap, f);
	r = vsnprintf(b, 256, f, ap);
	va_end(ap);
	return r;
}

void hl_vlog(int nivel, const char *f, va_list ap)
{
	static char linea[400];

	if (nivel > hl_nivel)
		return;
	vsnprintf(linea, sizeof(linea), f, ap);
	if (nivel == 0)
		reg_texto("ERROR: ");
	reg_texto(linea);
}

void hl_log(int nivel, const char *f, ...)
{
	va_list ap;
	va_start(ap, f);
	hl_vlog(nivel, f, ap);
	va_end(ap);
}

int printk(const char *f, ...)
{
	va_list ap;
	int n = 2;

	if (f[0] == '\001' && f[1]) {		/* KERN_...: el nivel */
		n = f[1] <= '3' ? 0 : f[1] == '4' ? 1 : f[1] == '7' ? 3 : 2;
		f += 2;
	}
	va_start(ap, f);
	hl_vlog(n, f, ap);
	va_end(ap);
	return 0;
}

void hl_aviso(const char *archivo, int linea, const char *que)
{
	static int cuantos;
	const char *a = strrchr(archivo, '/');

	if (++cuantos > 60)
		return;		/* que un aviso repetido no llene el registro */
	hl_linea();
	hl_log(1, "AVISO del controlador: %s (%s, línea %d)\n", que, a ? a + 1 : archivo, linea);
}

/* ---------------------------------------------------------------- tiempo */
static u64 tsc0;
static u32 tsc_por_ms = 1000000;
#if defined(HL_PENTIUM) && !defined(ANFITRION)
/* (PENTIUM) en el anillo 3: el TSC se mide con los tics del núcleo (el reloj
 * reparte el procesador: una espera con el 8254 se alargaría si nos quitan
 * el procesador en medio; los tics no) */
static inline u8 inb(u16 p) { u8 v; __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outb(u8 v, u16 p) { __asm__ __volatile__("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline u32 inl(u16 p) { u32 v; __asm__ __volatile__("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outl(u32 v, u16 p) { __asm__ __volatile__("outl %0, %1" : : "a"(v), "Nd"(p)); }
u32 hl_pentium_tics(void);
u32 hl_pentium_hz(void);

unsigned long hl_jiffies(void)
{
	return (unsigned long)hl_div64(tsc() - tsc0, tsc_por_ms, NULL);
}

void hl_udelay(unsigned long us)
{
	u64 fin = tsc() + hl_div64((u64)us * tsc_por_ms, 1000, NULL) + 1;
	while (tsc() < fin)
		__asm__ __volatile__("pause");
}

int hl_tiempo_inicia(void)
{
	u32 a, b, c, d, t, hz = hl_pentium_hz(), n;
	u64 t0, t1;

	__asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
	if (!(d & 0x10))
		return -1;
	if (hz < 10)
		hz = 100;
	n = hz / 5;		/* 200 ms */
	t = hl_pentium_tics();
	while (hl_pentium_tics() == t)
		;
	t0 = tsc();
	t = hl_pentium_tics();
	while (hl_pentium_tics() - t < n)
		;
	t1 = tsc();
	a = (u32)hl_div64(t1 - t0, n * 1000 / hz, NULL);
	hl_log(3, "Tiempo: %u ciclos en %u tics\n", (u32)(t1 - t0), n);
	if (a < 1000)
		return -2;
	tsc_por_ms = a;
	tsc0 = tsc();
	return 0;
}
#elif defined(ANFITRION)
unsigned long hl_reloj_falso;
unsigned long hl_jiffies(void) { return hl_reloj_falso; }
void hl_udelay(unsigned long us) { static u32 r; r += us; hl_reloj_falso += r / 1000; r %= 1000; }
int hl_tiempo_inicia(void) { return 0; }
#else
static inline u8 inb(u16 p) { u8 v; __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outb(u8 v, u16 p) { __asm__ __volatile__("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline u32 inl(u16 p) { u32 v; __asm__ __volatile__("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outl(u32 v, u16 p) { __asm__ __volatile__("outl %0, %1" : : "a"(v), "Nd"(p)); }

unsigned long hl_jiffies(void)
{
	return (unsigned long)hl_div64(tsc() - tsc0, tsc_por_ms, NULL);
}

void hl_udelay(unsigned long us)
{
	u64 fin = tsc() + hl_div64((u64)us * tsc_por_ms, 1000, NULL) + 1;
	while (tsc() < fin)
		__asm__ __volatile__("pause");
}

/* medir el TSC con el canal 2 del temporizador 8254 (el del altavoz), que
 * cuenta aunque las interrupciones estén cerradas */
int hl_tiempo_inicia(void)
{
	u32 a, b, c, d, i, mejor = 0;
	u8 p61;

	/* ¿hay CPUID y TSC? */
	__asm__ __volatile__("pushfl; popl %%eax; movl %%eax, %%ecx; xorl $0x200000, %%eax; pushl %%eax; popfl;"
		"pushfl; popl %%eax; xorl %%ecx, %%eax; pushl %%ecx; popfl" : "=a"(a) : : "ecx");
	if (!(a & 0x200000))
		return -1;
	__asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
	if (!(d & 0x10))
		return -1;
	p61 = inb(0x61);
	for (i = 0; i < 3; i++) {
		u64 t0, t1;
		u32 vueltas = 0;
		outb((p61 & 0xfc) | 1, 0x61);		/* puerta abierta, altavoz callado */
		outb(0xb0, 0x43);			/* canal 2, modo 0, cuenta de 16 bits */
		outb(11932 & 0xff, 0x42);		/* 10 ms */
		outb(11932 >> 8, 0x42);
		t0 = tsc();
		while (!(inb(0x61) & 0x20) && ++vueltas < 50000000)
			;
		t1 = tsc();
		hl_log(3, "Tiempo: medida %u: %u vueltas, %u ciclos (puerto 61h: %02x -> %02x)\n", i, vueltas, (u32)(t1 - t0), p61, inb(0x61));
		if (vueltas < 10 || vueltas >= 50000000)
			continue;
		a = (u32)hl_div64(t1 - t0, 10, NULL);
		if (!mejor || a < mejor)
			mejor = a;
	}
	outb(p61, 0x61);
	if (!mejor)
		return -2;
	tsc_por_ms = mejor;
	tsc0 = tsc();
	return 0;
}
#endif
u32 hl_tsc_khz(void) { return tsc_por_ms; }
void hl_ceder(void) { hl_trabajos(); }

/* ------------------------------------------- trabajos aplazados y tasklets */
static struct delayed_work *dworks;
static struct tasklet_struct *tasklets;

void hl_init_dwork(struct delayed_work *d, work_func_t f)
{
	struct delayed_work *x;

	d->work.func = f;
	d->armado = 0;
	for (x = dworks; x; x = x->sig)
		if (x == d)
			return;
	d->sig = dworks;
	dworks = d;
}

int hl_queue_dwork(struct delayed_work *d, unsigned long retraso)
{
	if (d->armado)
		return 0;
	d->cuando = hl_jiffies() + retraso;
	d->armado = 1;
	return 1;
}

int hl_cancel_dwork(struct delayed_work *d)
{
	int r = d->armado;
	d->armado = 0;
	return r;
}

void hl_olvida_dwork(struct delayed_work *d)
{
	struct delayed_work **x;
	for (x = &dworks; *x; x = &(*x)->sig)
		if (*x == d) {
			*x = d->sig;
			return;
		}
}

void tasklet_setup(struct tasklet_struct *t, void (*f)(struct tasklet_struct *))
{
	t->func = f;
	t->pend = 0;
	if (!t->puesto) {
		t->puesto = 1;
		t->sig = tasklets;
		tasklets = t;
	}
}

void tasklet_schedule(struct tasklet_struct *t) { t->pend = 1; }
void tasklet_kill(struct tasklet_struct *t) { t->pend = 0; }

void hl_trabajos(void)
{
	static int dentro;
	struct tasklet_struct *t;
	struct delayed_work *d;
	int vueltas = 0, hubo;

	if (dentro)
		return;
	dentro = 1;
	do {
		hubo = 0;
		for (t = tasklets; t; t = t->sig)
			if (t->pend) {
				t->pend = 0;
				t->func(t);
				hubo = 1;
			}
	} while (hubo && ++vueltas < 8);
	/* los trabajos aplazados que ya tocan (uno puede armar o soltar otros:
	 * se vuelve a mirar la lista desde el principio cada vez) */
	for (vueltas = 0; vueltas < 16; vueltas++) {
		for (d = dworks; d; d = d->sig)
			if (d->armado && time_after_eq(hl_jiffies(), d->cuando))
				break;
		if (!d)
			break;
		d->armado = 0;
		d->work.func(&d->work);
	}
	dentro = 0;
}

/* -------------------------------------------------------------- paquetes */
static u32 skb_vivos;

struct sk_buff *dev_alloc_skb(unsigned int tam)
{
	struct sk_buff *s = hl_malloc(sizeof(*s) + tam + 64);

	if (!s)
		return NULL;
	memset(s, 0, sizeof(*s));
	s->head = (u8 *)(s + 1);
	s->data = s->tail = s->head + 32;	/* un poco de sitio delante, como Linux */
	s->end = s->head + tam + 64;
	skb_vivos++;
	return s;
}

void kfree_skb(struct sk_buff *s)
{
	if (!s)
		return;
	skb_vivos--;
	hl_free(s);
}

u32 hl_skb_vivos(void) { return skb_vivos; }

/* ------------------------------------------------------------------- PCI */
/* El registro de dirección de la configuración PCI (puerto CF8h). El núcleo
 * se llama también desde el reloj (INT 08h): puede haber interrumpido a otro
 * programa entre que puso la dirección y leyó o escribió el dato (puerto
 * CFCh). Se guarda al entrar en cada llamada y se repone al salir; si no, el
 * dato del otro iría a parar a un registro de la tarjeta Wi-Fi (o al revés). */
#ifndef ANFITRION
u32 hl_pci_dir(void) { return inl(0xcf8); }
void hl_pci_dir_pon(u32 v) { outl(v, 0xcf8); }
#else
u32 hl_pci_dir(void) { return 0; }
void hl_pci_dir_pon(u32 v) { }
#endif
#ifndef HL_SIMULA_ES
static u32 pci_dir(struct pci_dev *d, int w)
{
	return 0x80000000u | (d->bus0.number << 16) | (d->devfn << 8) | (w & 0xfc);
}
#ifdef HL_PENTIUM
/* (PENTIUM) RED.BIN y el núcleo también usan los dos puertos: sin que el
 * reloj se cuele entre poner la dirección y leer o escribir el dato */
static u32 pci_lee_c(u32 dir)
{
	u32 v, f;
	__asm__ __volatile__("pushfl; popl %0; cli" : "=r"(f));
	outl(dir, 0xcf8);
	v = inl(0xcfc);
	__asm__ __volatile__("pushl %0; popfl" : : "r"(f) : "cc");
	return v;
}
static void pci_pon_c(u32 dir, u32 v)
{
	u32 f;
	__asm__ __volatile__("pushfl; popl %0; cli" : "=r"(f));
	outl(dir, 0xcf8);
	outl(v, 0xcfc);
	__asm__ __volatile__("pushl %0; popfl" : : "r"(f) : "cc");
}
u32 hl_pci_lee(int bus, int devfn, int w) { return pci_lee_c(0x80000000u | (bus << 16) | (devfn << 8) | (w & 0xfc)); }
int pci_read_config_dword(struct pci_dev *d, int w, u32 *v) { *v = pci_lee_c(pci_dir(d, w)); return 0; }
int pci_write_config_dword(struct pci_dev *d, int w, u32 v) { pci_pon_c(pci_dir(d, w), v); return 0; }
#else
u32 hl_pci_lee(int bus, int devfn, int w)
{
	outl(0x80000000u | (bus << 16) | (devfn << 8) | (w & 0xfc), 0xcf8);
	return inl(0xcfc);
}
int pci_read_config_dword(struct pci_dev *d, int w, u32 *v) { outl(pci_dir(d, w), 0xcf8); *v = inl(0xcfc); return 0; }
int pci_write_config_dword(struct pci_dev *d, int w, u32 v) { outl(pci_dir(d, w), 0xcf8); outl(v, 0xcfc); return 0; }
#endif
#else
u32 hl_sim_pci_lee(int w);
void hl_sim_pci_escribe(int w, u32 v);
u32 hl_pci_lee(int bus, int devfn, int w) { return (bus == 2 && devfn == 0) ? hl_sim_pci_lee(w & 0xfc) : 0xffffffff; }
int pci_read_config_dword(struct pci_dev *d, int w, u32 *v) { *v = hl_sim_pci_lee(w & 0xfc); return 0; }
int pci_write_config_dword(struct pci_dev *d, int w, u32 v) { hl_sim_pci_escribe(w & 0xfc, v); return 0; }
#endif
int pci_read_config_word(struct pci_dev *d, int w, u16 *v) { u32 x; pci_read_config_dword(d, w, &x); *v = x >> ((w & 2) * 8); return 0; }
int pci_read_config_byte(struct pci_dev *d, int w, u8 *v) { u32 x; pci_read_config_dword(d, w, &x); *v = x >> ((w & 3) * 8); return 0; }
int pci_write_config_word(struct pci_dev *d, int w, u16 v)
{
	u32 x;
	pci_read_config_dword(d, w, &x);
	x = (x & ~(0xffffu << ((w & 2) * 8))) | ((u32)v << ((w & 2) * 8));
	return pci_write_config_dword(d, w, x);
}
int pci_write_config_byte(struct pci_dev *d, int w, u8 v)
{
	u32 x;
	pci_read_config_dword(d, w, &x);
	x = (x & ~(0xffu << ((w & 3) * 8))) | ((u32)v << ((w & 3) * 8));
	return pci_write_config_dword(d, w, x);
}

int pci_find_capability(struct pci_dev *d, int cap)
{
	u16 st;
	u8 p, id;
	int n = 0;

	pci_read_config_word(d, PCI_STATUS, &st);
	if (!(st & 0x10))
		return 0;
	pci_read_config_byte(d, 0x34, &p);
	while (p >= 0x40 && n++ < 48) {
		p &= 0xfc;
		pci_read_config_byte(d, p, &id);
		if (id == cap)
			return p;
		pci_read_config_byte(d, p + 1, &p);
	}
	return 0;
}

/* Encender: estado D0, memoria y «bus master». La línea de interrupción se
 * deja APAGADA (bit 10 de la orden): el núcleo mira la tarjeta por sondeo,
 * y así no molesta a quien comparta su IRQ. */
int pci_enable_device(struct pci_dev *d)
{
	u16 cmd, pm;
	u32 bar0, bar1, il;
	int p = pci_find_capability(d, PCI_CAP_ID_PM);

	if (p) {
		pci_read_config_word(d, p + PCI_PM_CTRL, &pm);
		if (pm & 3) {
			hl_log(2, "PCI: la tarjeta estaba dormida (D%d); se despierta\n", pm & 3);
			pci_read_config_dword(d, 0x10, &bar0);
			pci_read_config_dword(d, 0x14, &bar1);
			pci_read_config_dword(d, 0x3c, &il);
			pci_write_config_word(d, p + PCI_PM_CTRL, pm & ~3);
			hl_udelay(20000);
			pci_write_config_dword(d, 0x10, bar0);
			pci_write_config_dword(d, 0x14, bar1);
			pci_write_config_byte(d, 0x3c, il);
		}
	}
	pci_read_config_word(d, PCI_COMMAND, &cmd);
	cmd |= PCI_COMMAND_MEMORY | 0x400;
	pci_write_config_word(d, PCI_COMMAND, cmd);
	return 0;
}

void pci_set_master(struct pci_dev *d)
{
	u16 cmd;
	pci_read_config_word(d, PCI_COMMAND, &cmd);
	pci_write_config_word(d, PCI_COMMAND, cmd | PCI_COMMAND_MASTER | 0x400);
}

void pci_apaga(struct pci_dev *d)
{
	u16 cmd;
	pci_read_config_word(d, PCI_COMMAND, &cmd);
	pci_write_config_word(d, PCI_COMMAND, (cmd & ~PCI_COMMAND_MASTER) | 0x400);
}

#ifdef HL_PENTIUM
/* (PENTIUM) los registros, a la vista con la llamada 95 (servicio.c) */
void *hl_mapea(u32 fis, u32 tam);
void __iomem *pci_iomap(struct pci_dev *d, int bar, unsigned long max)
{
	return hl_mapea(d->bar0, d->bar0_tam ? d->bar0_tam : 0x10000);
}

void __iomem *ioremap(u32 fis, unsigned long tam)
{
	return hl_mapea(fis, tam);
}
#else
void __iomem *pci_iomap(struct pci_dev *d, int bar, unsigned long max)
{
	return hl_virt(d->bar0);
}

void __iomem *ioremap(u32 fis, unsigned long tam)
{
	return hl_virt(fis);
}
#endif

/* ----------------------------------------------------------- microcódigo */
static struct { const char *nombre; const u8 *datos; u32 tam; } fws[2];
static struct firmware fw_obj[4];

void hl_fw_pon(int i, const char *nombre, const u8 *datos, u32 tam)
{
	fws[i].nombre = nombre; fws[i].datos = datos; fws[i].tam = tam;
}

int request_firmware(const struct firmware **fw, const char *nombre, struct device *d)
{
	static int n;
	int i;

	for (i = 0; i < 2; i++)
		if (fws[i].nombre && fws[i].tam && strcmp(fws[i].nombre, nombre) == 0) {
			struct firmware *f = &fw_obj[n++ & 3];
			f->data = fws[i].datos;
			f->size = fws[i].tam;
			*fw = f;
			hl_log(2, "Microcódigo %s: %u octetos\n", nombre, fws[i].tam);
			return 0;
		}
	hl_log(0, "falta el archivo del microcódigo %s\n", nombre);
	return -ENOENT;
}

void release_firmware(const struct firmware *fw) { }
