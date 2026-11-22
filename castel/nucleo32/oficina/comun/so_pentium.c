/* so_pentium.c - la oficina de 32 bits en Hispa-Pentium: lo de so.h con
 * las llamadas del núcleo (int 0x80; docs/PENTIUM.md, sección 6) y lo poco
 * de la biblioteca de C que piden miniz y stb (sin biblioteca del sistema:
 * -ffreestanding). La memoria, la biblioteca de C y la coma flotante (el
 * x87) son las de Hispa-PDF (castel/nucleo32/pdf/so_pentium.c); la ventana,
 * como el Navegador (castel/nucleo32/navega/frontal.c): llamadas 32-36, 39,
 * 40, 80 y 81, y el mapa de teclas de la 181 para saber si hay Control.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stddef.h>
#include <string.h>
#include "so.h"
#include "texto.h"

#define ES_ERROR(r) ((u32)(r) >= 0xFFFFFFF0u)

static u32 llamada(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;

	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}

/* ---- lo de la biblioteca de C ---- */
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
	return c ? NULL : (char *)s;
}

char *strstr(const char *a, const char *b)
{
	size_t n = strlen(b);

	for (; *a; a++)
		if (!strncmp(a, b, n))
			return (char *)a;
	return n ? NULL : (char *)a;
}

char *strcpy(char *d, const char *o)
{
	char *r = d;

	while ((*d++ = *o++))
		;
	return r;
}

/* (CUENTAS32) las dos que pide Cuentas 2 */
void *memchr(const void *s, int c, size_t n)
{
	const u8 *p = s;

	for (; n; n--, p++)
		if (*p == (u8)c)
			return (void *)p;
	return NULL;
}

char *strrchr(const char *s, int c)
{
	const char *u = NULL;

	for (;; s++) {
		if (*s == (char)c)
			u = s;
		if (!*s)
			return (char *)u;
	}
}

int abs(int x)
{
	return x < 0 ? -x : x;
}

/* ---- las divisiones de 64 bits (las de libgcc; como win32/libc.c) ---- */
u64 __udivmoddi4(u64 n, u64 d, u64 *r)
{
	u64 q = 0;
	int k;

	if (!d)
		return 0;
	if (!(d >> 32) && !(n >> 32)) {
		if (r)
			*r = (u32)n % (u32)d;
		return (u32)n / (u32)d;
	}
	for (k = 63; k >= 0; k--)
		if ((n >> k) >= d) {
			n -= d << k;
			q |= 1ull << k;
		}
	if (r)
		*r = n;
	return q;
}
u64 __udivdi3(u64 n, u64 d) { return __udivmoddi4(n, d, 0); }
u64 __umoddi3(u64 n, u64 d) { u64 r; __udivmoddi4(n, d, &r); return r; }
i64 __divdi3(i64 n, i64 d)
{
	int neg = (n < 0) ^ (d < 0);
	u64 q = __udivmoddi4(n < 0 ? -(u64)n : (u64)n, d < 0 ? -(u64)d : (u64)d, 0);

	return neg ? -(i64)q : (i64)q;
}
i64 __moddi3(i64 n, i64 d)
{
	u64 r;

	__udivmoddi4(n < 0 ? -(u64)n : (u64)n, d < 0 ? -(u64)d : (u64)d, &r);
	return n < 0 ? -(i64)r : (i64)r;
}

/* ---- la memoria ---- */
#define CLASES 13		/* de 16 a 64 KB */
#define GRANDE (64u * 1024)
typedef struct trozo {
	u32 tam;		/* lo que cabe (sin la cabecera) */
	u32 marca;		/* «HPDF» en uso; «LIBR» libre */
	struct trozo *sig;	/* (libre) el siguiente de su lista */
	u32 relleno;
} trozo;
#define EN_USO 0x46445048u
#define LIBRE 0x5242494Cu

static trozo *libres[CLASES];
static trozo *grandes;		/* los grandes libres */
static u8 *tope;		/* el final de la memoria del programa */
static u32 pedido, lo_mas;

static void *mas(u32 n)
{
	u32 r = llamada(17, n, 0, 0);

	if (ES_ERROR(r) || r == 0)
		return NULL;
	tope = (u8 *)r + n;
	return (void *)r;
}

void *so_pide(size_t n)
{
	trozo *t;
	u32 tam;
	int c;

	if (n > 0x30000000u)
		return NULL;
	if (n <= GRANDE / 2) {
		for (c = 0, tam = 16; tam < n; c++, tam *= 2)
			;
		if (libres[c]) {
			t = libres[c];
			libres[c] = t->sig;
		} else {
			t = mas(tam + sizeof(trozo));
			if (!t)
				return NULL;
			t->tam = tam;
		}
	} else {
		trozo **p, **mejor = NULL;

		tam = ((u32)n + GRANDE - 1) & ~(GRANDE - 1);
		for (p = &grandes; *p; p = &(*p)->sig)
			if ((*p)->tam >= tam && (!mejor || (*p)->tam < (*mejor)->tam))
				mejor = p;
		if (mejor && (*mejor)->tam <= tam + tam / 2) {
			t = *mejor;
			*mejor = t->sig;
		} else {
			t = mas(tam + sizeof(trozo));
			if (!t)
				return NULL;
			t->tam = tam;
		}
	}
	t->marca = EN_USO;
	pedido += t->tam;
	if (pedido > lo_mas)
		lo_mas = pedido;
	return t + 1;
}

void so_suelta(void *q)
{
	trozo *t;
	int c;
	u32 tam;

	if (!q)
		return;
	t = (trozo *)q - 1;
	if (t->marca != EN_USO)
		return;		/* (no es nuestro, o ya se soltó) */
	t->marca = LIBRE;
	pedido -= t->tam;
	if (t->tam > GRANDE / 2) {
		/* el de arriba del todo, al núcleo */
		if ((u8 *)(t + 1) + t->tam == tope) {
			u32 n = t->tam + sizeof(trozo);

			if (!ES_ERROR(llamada(17, (u32)-(i32)n, 0, 0))) {
				tope -= n;
				return;
			}
		}
		t->sig = grandes;
		grandes = t;
		return;
	}
	for (c = 0, tam = 16; tam < t->tam; c++, tam *= 2)
		;
	t->sig = libres[c];
	libres[c] = t;
}

void *so_repide(void *q, size_t n)
{
	trozo *t;
	void *r;

	if (!q)
		return so_pide(n);
	t = (trozo *)q - 1;
	if (n <= t->tam)
		return q;
	r = so_pide(n);
	if (!r)
		return NULL;
	memcpy(r, q, t->tam);
	so_suelta(q);
	return r;
}

void *malloc(size_t n) { return so_pide(n); }
void free(void *p) { so_suelta(p); }
void *realloc(void *p, size_t n) { return so_repide(p, n); }
void *calloc(size_t a, size_t b)
{
	void *p;

	if (b && a > 0x30000000u / b)
		return NULL;
	p = so_pide(a * b);
	if (p)
		memset(p, 0, a * b);
	return p;
}


u32 so_memoria_usada(void)
{
	return pedido;
}

/* ---- los archivos: las rutas, de UTF-8 a la página 437 ---- */
static void ruta437(const char *r, char *d)
{
	utf8_a_437(r, d, 300);
}

static const char *texto_error(u32 m)
{
	switch (m) {
	case 0xFFFFFFFFu: return "no está";
	case 0xFFFFFFFEu: return "no hay disco";
	case 0xFFFFFFFDu: return "hay demasiados archivos abiertos";
	case 0xFFFFFFF8u: return "el disco está lleno";
	case 0xFFFFFFF6u: return "está en uso";
	case 0xFFFFFFF5u: return "es de solo lectura";
	case 0xFFFFFFF3u: return "el nombre no vale (8+3)";
	case 0xFFFFFFF2u: return "es una carpeta";
	case 0xFFFFFFF0u: return "acceso denegado: es del sistema";
	}
	return "no se puede";
}

u8 *so_lee_archivo(const char *ruta, u32 *largo_, u32 max, const char **error)
{
	char r[300];
	u32 m, n, i, k;
	u8 *d;

	ruta437(ruta, r);
	m = llamada(7, (u32)r, 0, 0);
	if (ES_ERROR(m)) {
		if (error)
			*error = texto_error(m);
		return NULL;
	}
	n = llamada(13, m, 0, 0);
	if (ES_ERROR(n) || n > max) {
		llamada(9, m, 0, 0);
		if (error)
			*error = ES_ERROR(n) ? "no se puede leer" : "es demasiado grande";
		return NULL;
	}
	d = so_pide(n + 1);
	if (!d) {
		llamada(9, m, 0, 0);
		if (error)
			*error = "no hay memoria para leerlo";
		return NULL;
	}
	for (i = 0; i < n; i += k) {
		k = llamada(8, m, (u32)(d + i), n - i);
		if (ES_ERROR(k) || k == 0)
			break;
	}
	llamada(9, m, 0, 0);
	if (i < n) {
		so_suelta(d);
		if (error)
			*error = "no se puede leer";
		return NULL;
	}
	d[n] = 0;
	*largo_ = n;
	return d;
}

typedef struct { u32 m; } parchivo;

void *so_crea(const char *ruta, const char **error)
{
	char r[300];
	u32 m;
	parchivo *f;

	ruta437(ruta, r);
	m = llamada(7, (u32)r, 2, 0);
	if (ES_ERROR(m)) {
		if (error)
			*error = texto_error(m);
		return NULL;
	}
	f = so_pide(sizeof(parchivo));
	if (!f) {
		llamada(9, m, 0, 0);
		return NULL;
	}
	f->m = m;
	return f;
}

int so_escribe(void *f, const void *d, u32 n)
{
	parchivo *a = f;
	u32 hecho = 0;

	while (hecho < n) {
		u32 k = llamada(18, a->m, (u32)d + hecho, n - hecho);

		if (ES_ERROR(k) || k == 0)
			return -1;
		hecho += k;
	}
	return 0;
}

int so_cierra(void *f)
{
	parchivo *a = f;
	u32 r = llamada(9, a->m, 0, 0);

	so_suelta(a);
	return ES_ERROR(r) ? -1 : 0;
}

int so_borra(const char *ruta)
{
	char r[300];

	ruta437(ruta, r);
	return ES_ERROR(llamada(21, (u32)r, 0, 0)) ? -1 : 0;
}

int so_renombra(const char *de, const char *a)
{
	char r[300], s[300];

	ruta437(de, r);
	ruta437(a, s);
	return ES_ERROR(llamada(22, (u32)r, (u32)s, 0)) ? -1 : 0;
}

/* una entrada de carpeta (Entrada, llamadas.cas: 32 octetos) */
struct entrada {
	char nombre[13];
	u8 atributos;
	u16 reservado;
	u32 tamano;
	u16 fecha, hora;
	u32 cluster, libre;
};

int so_existe(const char *ruta)
{
	char r[300];
	u32 m, m2;
	struct entrada e;
	u8 nl[256];

	ruta437(ruta, r);
	m = llamada(7, (u32)r, 0, 0);
	if (ES_ERROR(m))
		return 0;
	/* (una carpeta abierta da sus entradas; un archivo, no) */
	memset(&e, 0, sizeof e);
	m2 = llamada(28, m, (u32)&e, (u32)nl);
	llamada(9, m, 0, 0);
	return m2 == 1 && e.nombre[0] ? 2 : 1;
}

typedef struct { u32 m; } pcarpeta;

void *so_abre_carpeta(const char *ruta)
{
	char r[300];
	u32 m;
	pcarpeta *c;

	ruta437(ruta, r);
	m = llamada(7, (u32)r, 0, 0);
	if (ES_ERROR(m))
		return NULL;
	c = so_pide(sizeof *c);
	if (!c) {
		llamada(9, m, 0, 0);
		return NULL;
	}
	c->m = m;
	return c;
}

int so_lee_carpeta(void *cc, so_entrada *e)
{
	pcarpeta *c = cc;
	struct entrada en;
	u8 nl[260];
	int i;

	for (;;) {
		memset(&en, 0, sizeof en);
		nl[0] = 0;
		if (llamada(28, c->m, (u32)&en, (u32)nl) != 1)
			return 0;
		if (en.atributos & 0x08)
			continue;	/* (el nombre del volumen) */
		if (en.nombre[0] == '.' && (en.nombre[1] == 0 || (en.nombre[1] == '.' && en.nombre[2] == 0)))
			continue;
		break;
	}
	nl[255] = 0;
	for (i = 0; i < 12 && en.nombre[i]; i++)
		e->corto[i] = en.nombre[i];
	e->corto[i] = 0;
	if (nl[0])
		de437_a_utf8(nl, largo((char *)nl), e->nombre, sizeof e->nombre);
	else
		de437_a_utf8((u8 *)e->corto, largo(e->corto), e->nombre, sizeof e->nombre);
	e->tamano = en.tamano;
	e->carpeta = (en.atributos & 0x10) != 0;
	return 1;
}

void so_cierra_carpeta(void *cc)
{
	pcarpeta *c = cc;

	if (!c)
		return;
	llamada(9, c->m, 0, 0);
	so_suelta(c);
}

int so_nombres_largos(void)
{
	return 0;
}

char so_unidad_sistema(void)
{
	u32 l = llamada(42, 0, 0, 0);

	return ES_ERROR(l) || l < 'A' || l > 'Z' ? 'C' : (char)l;
}

/* ---- lo demás ---- */
/* lo que se dice, al puerto serie (lo miran las pruebas), en la página 437 */
void so_dice(const char *t)
{
	char b[700];
	u32 n = utf8_a_437(t, b, sizeof b);

	llamada(61, (u32)b, n, 0);
}

int so_argumentos(char *d, int cabe)
{
	char b[400];
	u32 n = llamada(14, (u32)b, sizeof b - 1, 0);

	if (ES_ERROR(n))
		n = 0;
	if (n >= sizeof b)
		n = sizeof b - 1;
	b[n] = 0;
	return (int)de437_a_utf8((u8 *)b, n, d, (u32)cabe);
}

u32 so_tics(void)
{
	return llamada(4, 0, 0, 0);
}

u32 so_hz(void)
{
	u32 h = llamada(15, 9, 0, 0);

	return ES_ERROR(h) || !h ? 100 : h;
}

void so_duerme(u32 t)
{
	llamada(3, t, 0, 0);
}

void so_ahora(int *ano, int *mes, int *dia, int *hora, int *min, int *seg)
{
	u32 f = llamada(15, 10, 0, 0), d = f >> 16, h = f & 0xFFFF;

	*ano = 1980 + (int)(d >> 9);
	*mes = (int)(d >> 5 & 15);
	*dia = (int)(d & 31);
	*hora = (int)(h >> 11);
	*min = (int)(h >> 5 & 63);
	*seg = (int)(h & 31) * 2;
}

/* ---- la ventana ---- */
struct suceso { u32 clase, a, b, c; };
struct orden {
	u32 op;
	i32 x, y, ancho, alto;
	u32 tinta, tinta2;
	const void *datos;
	i32 n;
};
struct pide_menu { i32 x, y; const char *opciones; u32 inactivas, etiqueta; };
static u32 *temporal;
static u32 n_temporal;
static int ultimos_mods;

int so_ventana(const char *titulo, int an, int al)
{
	char t[200];
	u32 v;

	utf8_a_437(titulo, t, sizeof t);
	v = llamada(32, (u32)t, (u32)an, (u32)al);
	return ES_ERROR(v) ? 0 : (int)v;
}

void so_titulo(int v, const char *titulo)
{
	char t[200];

	utf8_a_437(titulo, t, sizeof t);
	llamada(36, (u32)v, (u32)t, 0);
}

void so_maximiza(int v)
{
	llamada(81, (u32)v, 0, 0);
}

void so_centra(int v)
{
	llamada(80, (u32)v, 0xFFFFFFFFu, 0);
}

void so_cierra_ventana(int v)
{
	llamada(33, (u32)v, 0, 0);
}

void so_pantalla(int *an, int *al)
{
	u32 a = llamada(15, 11, 0, 0), b = llamada(15, 14, 0, 0);

	if (ES_ERROR(b) || !b)
		b = llamada(15, 12, 0, 0);
	*an = ES_ERROR(a) || !a ? 800 : (int)a;
	*al = ES_ERROR(b) || !b ? 600 : (int)b;
}

void so_mapa(int v, int x, int y, int an, int al, const u32 *px, int paso)
{
	struct orden o;

	if (an <= 0 || al <= 0)
		return;
	memset(&o, 0, sizeof o);
	o.op = 7;
	o.x = x;
	o.y = y;
	o.ancho = an;
	o.alto = al;
	if (paso == an) {
		o.datos = px;
	} else {
		u32 n = (u32)an * (u32)al;
		int k;

		if (n > n_temporal) {
			so_suelta(temporal);
			temporal = so_pide(n * 4);
			n_temporal = temporal ? n : 0;
			if (!temporal)
				return;
		}
		for (k = 0; k < al; k++)
			memcpy(temporal + k * an, px + k * paso, (size_t)an * 4);
		o.datos = temporal;
	}
	llamada(34, (u32)v, (u32)&o, 0);
}

static u32 tecla_unicode(u32 k, int *mods)
{
	*mods = 0;
	if (k < 0x100) {
		if (k >= 1 && k <= 26 && k != 8 && k != 9 && k != 10 && k != 13)
			*mods = SO_CON_CONTROL;
		if (k == 13)
			return SO_INTRO;
		return de437((u8)k);
	}
	if (k >= 0x180 && k <= 0x199)
		return SO_T_ALT + (k - 0x180);
	switch (k) {
	case 0x148: return SO_T_ARRIBA;
	case 0x150: return SO_T_ABAJO;
	case 0x14B: return SO_T_IZQUIERDA;
	case 0x14D: return SO_T_DERECHA;
	case 0x147: return SO_T_INICIO;
	case 0x14F: return SO_T_FIN;
	case 0x149: return SO_T_RE_PAG;
	case 0x151: return SO_T_AV_PAG;
	case 0x153: return SO_T_SUPR;
	case 0x152: return SO_T_INSERTAR;
	case 0x10F: return SO_T_MAYUS_TAB;
	case 0x110: return SO_T_CTRL_TAB;
	case 0x157: return SO_T_F11;	/* (CUENTAS32) F11 y F12, si el núcleo los da */
	case 0x158: return SO_T_F12;
	}
	if (k >= 0x13B && k <= 0x144)
		return SO_T_F1 + (k - 0x13B);
	return 0;
}

int so_lee_suceso(int v, so_suceso *s, int espera)
{
	struct suceso e;
	u32 r;

	memset(s, 0, sizeof *s);
	r = llamada(35, (u32)v, (u32)&e, espera ? 1 : 0);
	if (r != 1)
		return 0;
	switch (e.clase) {
	case 1:
		s->clase = SO_TECLA;
		s->tecla = tecla_unicode(e.a, &s->mods);
		if (e.b & 1)
			s->mods |= SO_CON_MAYUS;
		if (s->tecla >= SO_T_ARRIBA && s->tecla <= SO_T_SUPR && so_control())
			s->mods |= SO_CON_CONTROL;
		if (!s->tecla)
			s->clase = SO_NADA;
		break;
	case 2: case 3: case 4:
		s->clase = e.clase == 2 ? SO_BAJA : e.clase == 3 ? SO_SUBE : SO_MUEVE;
		s->x = (i32)e.a;
		s->y = (i32)e.b;
		s->botones = (int)(e.c & 7);
		s->mods = ((e.c & 0x100) ? SO_CON_MAYUS : 0) | ((e.c & 0x200) ? SO_CON_CONTROL : 0);
		s->veces = (int)(e.c >> 16);
		if (!s->veces)
			s->veces = 1;
		ultimos_mods = s->mods;
		break;
	case 5: s->clase = SO_CERRAR; break;
	case 6: s->clase = SO_TAMANO; s->x = (i32)e.a; s->y = (i32)e.b; break;
	case 7: s->clase = SO_FOCO; s->foco = (int)e.a; break;
	case 9: s->clase = SO_SALE; break;
	case 10:
		s->clase = SO_MENU;
		s->opcion = e.a == 0xFFFFFFFFu ? -1 : e.a == 0xFFFFFFFEu ? -2 : e.a == 0xFFFFFFFDu ? -3 : (int)e.a;
		s->etiqueta = e.b & ~0x40000000u;
		break;
	case 13:
		s->clase = SO_RUEDA;
		s->x = (i32)e.a;
		s->y = (i32)e.b;
		s->pasos = (int)(int8_t)(e.c & 0xFF);
		s->mods = ((e.c & 0x100) ? SO_CON_MAYUS : 0) | ((e.c & 0x200) ? SO_CON_CONTROL : 0);
		break;
	default:
		s->clase = SO_NADA;
	}
	return 1;
}

void so_menu(int v, int x, int y, const char *opciones, u32 grises, u32 etiqueta)
{
	static char t[1100];
	struct pide_menu p;

	utf8_a_437(opciones, t, sizeof t);
	p.x = x;
	p.y = y;
	p.opciones = t;
	p.inactivas = grises;
	p.etiqueta = etiqueta;
	llamada(39, (u32)v, (u32)&p, 0);
}

int so_control(void)
{
	u32 mapa[16];	/* (la 181 da dos mapas: las pulsadas ahora y desde la última vez) */

	memset(mapa, 0, sizeof mapa);
	if (ES_ERROR(llamada(181, (u32)mapa, 0, 0)))
		return 0;
	/* el Control de la izquierda (0x1D) y el de la derecha (0xE0 0x1D: 0x9D) */
	return ((mapa[0x1D >> 5] >> (0x1D & 31)) & 1) || ((mapa[0x9D >> 5] >> (0x9D & 31)) & 1);
}

void so_al_portapapeles(const char *t, u32 n)
{
	char *d = so_pide(n + 1);
	u32 k;

	if (!d)
		return;
	{
		char *z = duplica_n(t, n);

		if (!z) {
			so_suelta(d);
			return;
		}
		k = utf8_a_437(z, d, n + 1);
		so_suelta(z);
	}
	llamada(40, 0, (u32)d, k);
	so_suelta(d);
}

char *so_del_portapapeles(u32 *n)
{
	u32 m = llamada(40, 2, 0, 0);
	u8 *t;
	char *d;

	*n = 0;
	if (ES_ERROR(m) || !m)
		return NULL;
	t = so_pide(m + 1);
	if (!t)
		return NULL;
	m = llamada(40, 1, (u32)t, m);
	if (ES_ERROR(m)) {
		so_suelta(t);
		return NULL;
	}
	d = so_pide(m * 3 + 1);
	if (d)
		*n = de437_a_utf8(t, m, d, m * 3 + 1);
	so_suelta(t);
	return d;
}

/* ---- la coma flotante, con el x87 ---- */
double so_raiz(double x)
{
	if (x <= 0)
		return 0;
	__asm__("fsqrt" : "+t"(x));
	return x;
}

static double redondea(double x, u16 modo)
{
	u16 antes, cw;

	__asm__ volatile("fnstcw %0" : "=m"(antes));
	cw = (u16)((antes & ~0x0C00) | modo);
	__asm__ volatile("fldcw %0" : : "m"(cw));
	__asm__ volatile("frndint" : "+t"(x));
	__asm__ volatile("fldcw %0" : : "m"(antes));
	return x;
}

double so_suelo(double x) { return redondea(x, 0x0400); }
double so_techo(double x) { return redondea(x, 0x0800); }
double so_abs(double x) { return x < 0 ? -x : x; }

double so_resto(double x, double y)
{
	double r;

	if (y == 0)
		return 0;
	__asm__("1: fprem\n\tfnstsw %%ax\n\ttestb $4, %%ah\n\tjnz 1b" : "=t"(r) : "0"(x), "u"(y) : "ax", "cc");
	return r;
}

double so_ln(double x)
{
	double r;

	if (x <= 0)
		return -1e300;
	__asm__("fldln2\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x));
	return r;
}

static double exp2_(double x)
{
	double ent = redondea(x, 0), fr = x - ent, r;

	__asm__("f2xm1" : "=t"(r) : "0"(fr));
	r += 1;
	__asm__("fscale" : "=t"(r) : "0"(r), "u"(ent));
	return r;
}

double so_pot(double x, double y)
{
	double l;

	if (y == 0)
		return 1;
	if (x == 0)
		return 0;
	if (x < 0) {
		/* (solo con y entero) */
		double r = so_pot(-x, y);
		i32 e = (i32)y;

		return (e & 1) ? -r : r;
	}
	__asm__("fyl2x" : "=t"(l) : "0"(x), "u"(y) : "st(1)");
	if (l > 1000)
		return 1e300;
	if (l < -1000)
		return 0;
	return exp2_(l);
}

double so_cos(double x)
{
	__asm__("fcos" : "+t"(x));
	return x;
}

double so_sen(double x)
{
	__asm__("fsin" : "+t"(x));
	return x;
}

double so_atan2(double y, double x)
{
	double r;

	__asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
	return r;
}

double so_acos(double x)
{
	if (x >= 1)
		return 0;
	if (x <= -1)
		return 3.14159265358979323846;
	return so_atan2(so_raiz(1 - x * x), x);
}

/* (lo que pide gcc a veces para las de punto flotante de stb) */
double sqrt(double x) { return so_raiz(x); }
double floor(double x) { return so_suelo(x); }
double ceil(double x) { return so_techo(x); }
double fabs(double x) { return so_abs(x); }
double pow(double x, double y) { return so_pot(x, y); }
double fmod(double x, double y) { return so_resto(x, y); }
double cos(double x) { return so_cos(x); }
double acos(double x) { return so_acos(x); }


int so_lanza(const char *programa, const char *args)
{
	char p[300], a[400];

	utf8_a_437(programa, p, sizeof p);
	utf8_a_437(args ? args : "", a, sizeof a);
	return ES_ERROR(llamada(41, (u32)p, (u32)a, 0)) ? -1 : 0;
}

void so_programa(char *d, u32 cabe)
{
	char u = so_unidad_sistema();

	fmt(d, cabe, "%c:\\HISPA\\%s", u, ofi_programa);
}

/* ---- la entrada (la llama entrada.asm) ---- */
int ofi_pentium(void)
{
	static char args[1300];

	so_argumentos(args, sizeof args);
	return ofi_principal(args);
}
