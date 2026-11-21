/* plataforma.c - ESCUDO.BIN en Hispa-Pentium: lo que el motor pide (pl_*),
 * con las llamadas al sistema; la memoria (un montón sencillo sobre la
 * llamada 17, la que mueve el final del programa); lo poco de la biblioteca
 * de C que hace falta (sin biblioteca del sistema: -ffreestanding); y los
 * textos, de UTF-8 (como están escritos aquí) a la página 437 de Hispa (la
 * de la letra del escritorio: tools/hispa437.py, con Á, Í, Ó, Ú).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdarg.h>
#include "pentium.h"

/* ------------------------------------------------------------ biblioteca */
void *memcpy(void *d, const void *o, size_t n)
{
	u8 *a = d;
	const u8 *b = o;
	if ((((u32)a | (u32)b) & 3) == 0)
		for (; n >= 4; n -= 4, a += 4, b += 4)
			*(u32 *)a = *(const u32 *)b;
	while (n--)
		*a++ = *b++;
	return d;
}
void *memmove(void *d, const void *o, size_t n)
{
	u8 *a = d;
	const u8 *b = o;
	if (a <= b || a >= b + n)
		return memcpy(d, o, n);
	while (n--)
		a[n] = b[n];
	return d;
}
void *memset(void *d, int c, size_t n)
{
	u8 *a = d;
	if (((u32)a & 3) == 0) {
		u32 v = (u8)c * 0x01010101u;
		for (; n >= 4; n -= 4, a += 4)
			*(u32 *)a = v;
	}
	while (n--)
		*a++ = (u8)c;
	return d;
}
int memcmp(const void *x, const void *y, size_t n)
{
	const u8 *a = x, *b = y;
	for (; n; n--, a++, b++)
		if (*a != *b)
			return *a - *b;
	return 0;
}
size_t strlen(const char *s)
{
	size_t n = 0;
	while (s[n])
		n++;
	return n;
}
int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return (u8)*a - (u8)*b;
}
int strncmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++) {
		if (*a != *b)
			return (u8)*a - (u8)*b;
		if (!*a)
			return 0;
	}
	return 0;
}
char *strchr(const char *s, int c)
{
	for (; *s; s++)
		if (*s == (char)c)
			return (char *)s;
	return c ? 0 : (char *)s;
}
char *strrchr(const char *s, int c)
{
	const char *r = 0;
	for (; *s; s++)
		if (*s == (char)c)
			r = s;
	return (char *)(c ? r : s);
}
size_t strcspn(const char *s, const char *r)
{
	size_t n = 0;
	while (s[n] && !strchr(r, s[n]))
		n++;
	return n;
}
char *strstr(const char *a, const char *b)
{
	size_t n = strlen(b);
	for (; *a; a++)
		if (!strncmp(a, b, n))
			return (char *)a;
	return n ? 0 : (char *)a;
}
int strncasecmp_es(const char *a, const char *b, u32 n)
{
	for (; n; n--, a++, b++) {
		int x = (u8)*a, y = (u8)*b;
		if (x >= 'A' && x <= 'Z')
			x += 32;
		if (y >= 'A' && y <= 'Z')
			y += 32;
		if (x != y)
			return x - y;
		if (!x)
			return 0;
	}
	return 0;
}

/* ---- formatear: %s %u %d %x %c %% (con ancho y ceros), y %S (de 1000 en 1000, con puntos) ---- */
static int pon(char *d, int k, int cabe, char c)
{
	if (k < cabe - 1)
		d[k] = c;
	return k + 1;
}
int vformatear(char *d, int cabe, const char *p, va_list va)
{
	int k = 0;
	for (; *p; p++) {
		char t[16];
		int ancho = 0, ceros = 0, izq = 0, i, n;
		u32 v;
		const char *s;
		if (*p != '%') {
			k = pon(d, k, cabe, *p);
			continue;
		}
		p++;
		if (*p == '-')
			izq = 1, p++;
		if (*p == '0')
			ceros = 1, p++;
		while (*p >= '0' && *p <= '9')
			ancho = ancho * 10 + (*p++ - '0');
		switch (*p) {
		case 's':
			s = va_arg(va, const char *);
			if (!s)
				s = "";
			for (n = 0; s[n]; n++) ;
			for (i = n; !izq && i < ancho; i++)
				k = pon(d, k, cabe, ' ');
			for (i = 0; i < n; i++)
				k = pon(d, k, cabe, s[i]);
			for (i = n; izq && i < ancho; i++)
				k = pon(d, k, cabe, ' ');
			break;
		case 'c':
			k = pon(d, k, cabe, (char)va_arg(va, int));
			break;
		case 'S':	/* 1.234.567 */
			v = va_arg(va, u32);
			n = 0;
			{
				int g = 0;
				do {
					if (g == 3)
						t[n++] = '.', g = 0;
					t[n++] = (char)('0' + v % 10);
					v /= 10;
					g++;
				} while (v);
			}
			for (i = n; i < ancho; i++)
				k = pon(d, k, cabe, ' ');
			while (n)
				k = pon(d, k, cabe, t[--n]);
			break;
		case 'd':
		case 'u':
		case 'x':
			v = va_arg(va, u32);
			n = 0;
			if (*p == 'd' && (i32)v < 0) {
				k = pon(d, k, cabe, '-');
				v = (u32)(-(i32)v);
			}
			do {
				u32 b = *p == 'x' ? 16 : 10;
				t[n++] = "0123456789abcdef"[v % b];
				v /= b;
			} while (v);
			for (i = n; i < ancho; i++)
				k = pon(d, k, cabe, ceros ? '0' : ' ');
			while (n)
				k = pon(d, k, cabe, t[--n]);
			break;
		default:
			k = pon(d, k, cabe, *p);
			if (!*p)
				p--;
		}
	}
	if (cabe > 0)
		d[k < cabe ? k : cabe - 1] = 0;
	return k < cabe ? k : cabe - 1;
}
int formatear(char *d, int cabe, const char *p, ...)
{
	va_list va;
	int r;
	va_start(va, p);
	r = vformatear(d, cabe, p, va);
	va_end(va);
	return r;
}

/* ------------------------------------------------- de UTF-8 a la 437 */
static const struct { u16 u; u8 c; } letras437[] = {
	{ 0xE1, 0xA0 }, { 0xE9, 0x82 }, { 0xED, 0xA1 }, { 0xF3, 0xA2 }, { 0xFA, 0xA3 }, { 0xF1, 0xA4 }, { 0xD1, 0xA5 },
	{ 0xAA, 0xA6 }, { 0xBA, 0xA7 }, { 0xBF, 0xA8 }, { 0xA1, 0xAD }, { 0xAB, 0xAE }, { 0xBB, 0xAF }, { 0xFC, 0x81 },
	{ 0xDC, 0x9A }, { 0xC9, 0x90 }, { 0xE7, 0x87 }, { 0xC7, 0x80 }, { 0xE0, 0x85 }, { 0xE8, 0x8A }, { 0xF2, 0x95 },
	{ 0xEF, 0x8B }, { 0xB7, 0xFA }, { 0xB0, 0xF8 },
	/* las de más de Hispa (tools/hispa437.py) */
	{ 0xC1, 0xB6 }, { 0xCD, 0xCF }, { 0xD3, 0xD3 }, { 0xDA, 0xD7 }, { 0xC0, 0xB5 }, { 0xC8, 0xBD }, { 0xD2, 0xD2 },
	{ 0x20AC, 0xEE }, { 0x2014, '-' }, { 0x2013, '-' }, { 0x2026, '.' }, { 0x201C, '"' }, { 0x201D, '"' },
	{ 0x2019, '\'' }, { 0x2192, 0x1A }, { 0xD7, 'x' },
};
u32 a437(char *d, u32 cabe, const char *s)
{
	u32 k = 0;
	const u8 *p = (const u8 *)s;
	while (*p && k + 1 < cabe) {
		u32 u = *p, n = 0, i;
		if (u >= 0xC2 && u <= 0xDF && (p[1] & 0xC0) == 0x80)
			u = (u & 0x1F) << 6 | (p[1] & 0x3F), n = 2;
		else if (u >= 0xE0 && u <= 0xEF && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80)
			u = (u & 0x0F) << 12 | (u32)(p[1] & 0x3F) << 6 | (p[2] & 0x3F), n = 3;
		if (!n) {	/* ASCII, o ya en la 437 */
			d[k++] = (char)*p++;
			continue;
		}
		p += n;
		for (i = 0; i < sizeof letras437 / sizeof letras437[0]; i++)
			if (letras437[i].u == u)
				break;
		d[k++] = i < sizeof letras437 / sizeof letras437[0] ? (char)letras437[i].c : '?';
	}
	d[k] = 0;
	return k;
}

/* ------------------------------------------------------- la memoria
 * Un montón con sus trozos libres en una lista ordenada por dirección
 * (se juntan al soltar); lo que no cabe, más memoria del núcleo (17). */
typedef struct trozo { u32 tam; struct trozo *sig; } trozo;	/* tam: con la cabecera; el bit 0, libre */
static trozo *libres;
static u8 *tope_monton, *base_monton;

static void *nucleo_mas(u32 n)
{
	u32 r = sc(SC_MEMORIA, n, 0, 0);
	if (ES_ERROR(r))
		return 0;
	if (!base_monton)
		base_monton = (u8 *)r;
	tope_monton = (u8 *)r + n;
	return (void *)r;
}

void *pl_reserva(u32 n)
{
	trozo **pp, *t;
	u32 tam = (n + sizeof(trozo) + 15) & ~15u;
	if (n > 0x7FFFFFF0u)
		return 0;
	for (pp = &libres; (t = *pp); pp = &t->sig)
		if (t->tam >= tam) {
			if (t->tam - tam >= 64) {	/* lo que sobra, libre */
				trozo *r = (trozo *)((u8 *)t + tam);
				r->tam = t->tam - tam;
				r->sig = t->sig;
				*pp = r;
				t->tam = tam;
			} else
				*pp = t->sig;
			t->sig = 0;
			return t + 1;
		}
	/* ¿el último libre llega al tope? se alarga ese */
	{
		u32 pide = tam < 65536 ? 65536 : (tam + 4095) & ~4095u;
		u8 *m = nucleo_mas(pide);
		if (!m)
			return 0;
		t = (trozo *)m;
		t->tam = pide;
		t->sig = 0;
		pl_libera(t + 1);
		return pl_reserva(n);
	}
}

void pl_libera(void *p)
{
	trozo *t, **pp, *a;
	if (!p)
		return;
	t = (trozo *)p - 1;
	for (pp = &libres; *pp && *pp < t; pp = &(*pp)->sig) ;
	t->sig = *pp;
	*pp = t;
	/* con el de detrás */
	if (t->sig && (u8 *)t + t->tam == (u8 *)t->sig) {
		t->tam += t->sig->tam;
		t->sig = t->sig->sig;
	}
	/* con el de delante */
	for (a = libres; a && a->sig != t; a = a->sig) ;
	if (a && (u8 *)a + a->tam == (u8 *)t) {
		a->tam += t->tam;
		a->sig = t->sig;
	}
}

void *pl_rereserva(void *p, u32 n)
{
	trozo *t;
	void *q;
	if (!p)
		return pl_reserva(n);
	t = (trozo *)p - 1;
	if (t->tam - sizeof(trozo) >= n)
		return p;
	/* ¿lo de detrás está libre y basta? */
	{
		trozo **pp;
		u32 tam = (n + sizeof(trozo) + 15) & ~15u;
		for (pp = &libres; *pp && (u8 *)*pp < (u8 *)t + t->tam; pp = &(*pp)->sig) ;
		if (*pp && (u8 *)*pp == (u8 *)t + t->tam && t->tam + (*pp)->tam >= tam) {
			trozo *s = *pp;
			u32 junto = t->tam + s->tam;
			*pp = s->sig;
			t->tam = junto;
			if (junto - tam >= 64) {
				trozo *r = (trozo *)((u8 *)t + tam);
				t->tam = tam;
				r->tam = junto - tam;
				pl_libera(r + 1);
			}
			return p;
		}
		/* ¿es lo último del montón? se pide más al núcleo detrás */
		if ((u8 *)t + t->tam == tope_monton) {
			u32 falta = (tam - t->tam + 4095) & ~4095u;
			if (falta < 65536)
				falta = 65536;
			if (nucleo_mas(falta)) {
				t->tam += falta;
				return p;
			}
		}
	}
	q = pl_reserva(n);
	if (!q)
		return 0;
	memcpy(q, p, t->tam - sizeof(trozo));
	pl_libera(p);
	return q;
}

/* ------------------------------------------------------- los archivos */
int pl_abrir(const char *ruta)
{
	u32 m = sc(SC_ABRIR, (u32)ruta, PARA_LEER, 0);
	return ES_ERROR(m) ? -1 : (int)m;
}
int pl_crear(const char *ruta)
{
	u32 m = sc(SC_ABRIR, (u32)ruta, PARA_CREAR, 0);
	return ES_ERROR(m) ? -1 : (int)m;
}
int pl_leer(int h, void *d, u32 n)
{
	u32 k = 0;
	while (k < n) {
		u32 r = sc(SC_LEER, (u32)h, (u32)d + k, n - k);
		if (ES_ERROR(r))
			return k ? (int)k : -1;
		if (!r)
			break;
		k += r;
	}
	return (int)k;
}
int pl_escribir(int h, const void *d, u32 n)
{
	u32 r = sc(SC_ESCRIBIR_ARCHIVO, (u32)h, (u32)d, n);
	return ES_ERROR(r) ? -1 : (int)r;
}
int pl_mover(int h, u32 p)
{
	u32 r = sc(SC_MOVER, (u32)h, p, 0);
	return ES_ERROR(r) ? -1 : 0;
}
u32 pl_largo(int h) { return sc(SC_LARGO, (u32)h, 0, 0); }
void pl_cerrar(int h) { sc(SC_CERRAR, (u32)h, 0, 0); }
u32 hz(void)
{
	static u32 v;
	if (!v)
		v = sc(SC_DATO, DATO_HZ, 0, 0);
	return v ? v : 100;
}
u32 tics(void) { return sc(SC_TICS, 0, 0, 0); }
void dormir(u32 t) { sc(SC_DORMIR, t, 0, 0); }
u32 pl_ms(void) { return tics() * (1000 / hz()); }

int existe(const char *ruta)
{
	int h = pl_abrir(ruta);
	if (h < 0)
		return 0;
	pl_cerrar(h);
	return 1;
}
int abre_carpeta(const char *ruta)
{
	return pl_abrir(ruta);
}
int entrada_siguiente(int h, struct entrada *e, char *largo)
{
	return sc(SC_ENTRADA_LARGA, (u32)h, (u32)e, (u32)largo) == 1;
}
int es_carpeta(const char *ruta)
{
	u8 x;
	int h = abre_carpeta(ruta), r;
	if (h < 0)
		return 0;
	/* una carpeta no se deja leer como un archivo (como en ORDENES) */
	r = ES_ERROR(sc(SC_LEER, (u32)h, (u32)&x, 0));
	pl_cerrar(h);
	return r;
}
int borrar(const char *ruta) { return ES_ERROR(sc(SC_BORRAR, (u32)ruta, 0, 0)) ? -1 : 0; }
int renombrar(const char *a, const char *b) { return ES_ERROR(sc(SC_RENOMBRAR, (u32)a, (u32)b, 0)) ? -1 : 0; }
int hacer_carpeta(const char *ruta)
{
	u32 r = sc(SC_HACER_CARPETA, (u32)ruta, 0, 0);
	return ES_ERROR(r) && r != E_YA_EXISTE ? -1 : 0;
}
int lee_todo(const char *ruta, char *d, u32 cabe)
{
	int h = pl_abrir(ruta), n;
	if (h < 0)
		return -1;
	n = pl_leer(h, d, cabe);
	pl_cerrar(h);
	return n;
}
int escribe_todo(const char *ruta, const void *d, u32 n)
{
	int h = pl_crear(ruta), r;
	if (h < 0)
		return -1;
	r = pl_escribir(h, d, n);
	pl_cerrar(h);
	return r == (int)n ? 0 : -1;
}
int anade_a(const char *ruta, const char *t)
{
	char b[1024];
	u32 n = a437(b, sizeof b, t), r = sc(SC_ABRIR, (u32)ruta, PARA_ANADIR, 0);
	if (ES_ERROR(r))
		return -1;
	sc(SC_ESCRIBIR_ARCHIVO, r, (u32)b, n);
	sc(SC_CERRAR, r, 0, 0);
	return 0;
}
int copia(const char *de, const char *a)
{
	static u8 b[32768];
	int h = pl_abrir(de), g, n, bien = 1;
	if (h < 0)
		return -1;
	g = pl_crear(a);
	if (g < 0) {
		pl_cerrar(h);
		return -1;
	}
	while ((n = pl_leer(h, b, sizeof b)) > 0)
		if (pl_escribir(g, b, (u32)n) != n) {
			bien = 0;
			break;
		}
	if (n < 0)
		bien = 0;
	pl_cerrar(h);
	pl_cerrar(g);
	if (!bien)
		borrar(a);
	return bien ? 0 : -1;
}
u32 fecha_hoy(void)
{
	u32 f = sc(SC_DATO, DATO_FECHA, 0, 0) >> 16;
	return ((f >> 9) + 1980) * 10000 + ((f >> 5) & 15) * 100 + (f & 31);
}
u32 hora_ahora(void)
{
	u32 h = sc(SC_DATO, DATO_FECHA, 0, 0) & 0xFFFF;
	return (h >> 11) * 100 + ((h >> 5) & 63);
}
int unidad_sistema(void)
{
	u32 u = sc(SC_UNIDAD, 0, 0, 0);
	return u >= 'A' && u <= 'Z' ? (int)u : u >= 'a' && u <= 'z' ? (int)(u - 32) : 'C';
}
int info_unidad(u32 n, struct info_unidad *u) { return sc(SC_INFO_UNIDAD, n, (u32)u, 0) == 1; }
u32 cambios_unidades(void) { return sc(SC_CAMBIOS_UNIDADES, 0, 0, 0); }
int escritorio_en_marcha(void) { return sc(SC_DATO, DATO_ESCRITORIO, 0, 0) == 1; }

/* ------------------------------------------------------- lo que se dice */
void escribe(const char *t)
{
	char b[1024];
	u32 n = a437(b, sizeof b, t);
	sc(SC_ESCRIBIR, (u32)b, n, 0);
}
void escribef(const char *f, ...)
{
	char b[1024];
	va_list va;
	va_start(va, f);
	vformatear(b, sizeof b, f, va);
	va_end(va);
	escribe(b);
}
/* al puerto serie (lo leen las pruebas), en la 437 como todo el sistema */
static void a_serie(const char *f, va_list va)
{
	char b[600], c[600];
	u32 n;
	vformatear(b, sizeof b, f, va);
	n = a437(c, sizeof c - 1, b);
	c[n++] = '\n';
	sc(SC_SERIE, (u32)c, n, 0);
}
void serie(const char *f, ...)
{
	va_list va;
	va_start(va, f);
	a_serie(f, va);
	va_end(va);
}
void pl_apunta(const char *f, ...)
{
	va_list va;
	va_start(va, f);
	a_serie(f, va);
	va_end(va);
}
int pl_cortar(void)
{
	static u32 visto;
	u32 t;
	if (!pl_atiende)
		return sc(SC_HUBO_CORTE, 0, 0, 0) == 1 ? (pl_atiende = -1, 1) : pl_atiende == -1;
	if (pl_atiende < 0)
		return 1;
	t = tics();
	if (t - visto < hz() / 10)
		return 0;
	visto = t;
	if (atiende_ventana())
		pl_atiende = -1;
	return pl_atiende < 0;
}
