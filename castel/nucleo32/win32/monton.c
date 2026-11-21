/* monton.c - WIN32.BIN: la memoria de los programas de Windows. Las zonas
 * (lo de VirtualAlloc, las imágenes, la pila), con las páginas que pone y
 * quita el núcleo (la llamada 180, win32.cas), y el montón (HeapAlloc,
 * malloc de MSVCRT, GlobalAlloc...): bloques con cabecera y pie, listas de
 * sueltos por tamaño y los vecinos sueltos que se juntan.
 *
 * Las direcciones (del programa: base 0x40000000):
 *   0x00000000  WIN32.BIN
 *   0x00400000  el .EXE (donde diga: casi siempre aquí)
 *   0x10000000  las DLL (reubicadas donde quepan)
 *   0x20000000  el montón (256 MB como mucho)
 *   0x30000000  VirtualAlloc y la pila del programa
 *   0x7FBF0000  la TEB y la PEB; 0x7FC00000, la pila del núcleo (sin usar)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"

#define PAG 4096u
#define ZONAS 512
#define VA_INI 0x30000000u
#define VA_FIN 0x7F000000u
static struct { u32 base, tam; } zona[ZONAS];
static int nzonas;

static int choca(u32 d, u32 n)
{
	int i;
	for (i = 0; i < nzonas; i++)
		if (d < zona[i].base + zona[i].tam && zona[i].base < d + n)
			return 1;
	return 0;
}
static int apunta(u32 d, u32 n)
{
	if (nzonas >= ZONAS)
		return 0;
	zona[nzonas].base = d;
	zona[nzonas].tam = n;
	nzonas++;
	return 1;
}

/* reserva (y, con «poner», pone las páginas a cero) n octetos en dir, o
 * donde quepan si dir = 0. Da la dirección, o 0. */
u32 v_reservar(u32 dir, u32 n, int poner)
{
	u32 d;
	n = (n + PAG - 1) & ~(PAG - 1);
	if (!n)
		return 0;
	if (dir) {
		dir &= ~(PAG - 1);
		if (dir < 0x10000 || dir + n < dir || dir + n > 0x7FC00000u || choca(dir, n))
			return 0;
		d = dir;
	} else {
		for (d = VA_INI; d + n <= VA_FIN && d + n > d; ) {
			int i, hay = 0;
			for (i = 0; i < nzonas; i++)
				if (d < zona[i].base + zona[i].tam && zona[i].base < d + n) {
					d = (zona[i].base + zona[i].tam + 0xFFFF) & ~0xFFFFu;
					hay = 1;
					break;
				}
			if (!hay)
				break;
		}
		if (d + n > VA_FIN || d + n < d)
			return 0;
	}
	if (poner && ES_ERROR(sys_memoria(d, n, 0))) {
		sys_memoria(d, n, 1);
		return 0;
	}
	if (!apunta(d, n)) {
		if (poner)
			sys_memoria(d, n, 1);
		return 0;
	}
	return d;
}

/* suelta la zona que empieza en dir (n = 0: entera) */
int v_soltar(u32 dir, u32 n)
{
	int i;
	for (i = 0; i < nzonas; i++)
		if (zona[i].base == dir) {
			if (!n || n >= zona[i].tam) {
				sys_memoria(dir, zona[i].tam, 1);
				zona[i] = zona[--nzonas];
			} else
				sys_memoria(dir, (n + PAG - 1) & ~(PAG - 1), 1);
			return 1;
		}
	return 0;
}

/* ¿es de alguna zona? da su base (0 si no) */
int v_hay(u32 dir, u32 n)
{
	int i;
	for (i = 0; i < nzonas; i++)
		if (dir >= zona[i].base && dir + n <= zona[i].base + zona[i].tam)
			return (int)zona[i].base;
	return 0;
}
u32 v_tam_zona(u32 base)
{
	int i;
	for (i = 0; i < nzonas; i++)
		if (zona[i].base == base)
			return zona[i].tam;
	return 0;
}

/* ---- el montón ----
 * Cada bloque empieza en una dirección múltiplo de 8 con su cabecera: el
 * tamaño (múltiplo de 8, con ella), USADO y ANTES_USADO (el de delante está
 * usado), y lo que se pidió (HeapSize); lo que se da va detrás, a 8. Los sueltos llevan además su tamaño al final (para
 * juntarlos con el de detrás) y van en listas por tamaño. Detrás del último
 * está la «cima»: una cabecera sin bloque (con ANTES_USADO) y, de ahí para
 * arriba, lo que aún no se ha dado. Dos sueltos nunca van seguidos. */
#define M_INI 0x20000000u
#define M_FIN 0x30000000u
#define USADO 1u
#define ANTES_USADO 2u
#define TAM(h) ((h) & ~7u)
#define CAB(b) (*(u32 *)(b))
typedef struct Suelto { u32 cab, pedido; struct Suelto *sig, *ant; } Suelto;
#define NCAJ 50
static Suelto *caja[NCAJ];
static u32 cima;                   /* la cabecera de la cima */
static u32 puesto = M_INI;         /* hasta dónde hay páginas */

static int cajita(u32 t)
{
	int k = 0;
	if (t < 256)
		return (int)(t >> 3) - 2;          /* 0..29: de 16 a 248, de 8 en 8 */
	t >>= 8;
	while (t > 1 && k < NCAJ - 31) {
		t >>= 1;
		k++;
	}
	return 30 + k;
}
static void meter(u32 b)
{
	Suelto *s = (Suelto *)b;
	int c = cajita(TAM(s->cab));
	*(u32 *)(b + TAM(s->cab) - 4) = TAM(s->cab);
	s->ant = 0;
	s->sig = caja[c];
	if (caja[c])
		caja[c]->ant = s;
	caja[c] = s;
}
static void sacar(u32 b)
{
	Suelto *s = (Suelto *)b;
	int c = cajita(TAM(s->cab));
	if (s->ant)
		s->ant->sig = s->sig;
	else
		caja[c] = s->sig;
	if (s->sig)
		s->sig->ant = s->ant;
}
static int crecer(u32 hasta)
{
	u32 n;
	if (hasta <= puesto)
		return 1;
	if (hasta > M_FIN)
		return 0;
	n = ((hasta - puesto) + 0xFFFF) & ~0xFFFFu;   /* de 64 K en 64 K */
	if (puesto + n > M_FIN)
		n = M_FIN - puesto;
	if (ES_ERROR(sys_memoria(puesto, n, 0)))
		return 0;
	puesto += n;
	return 1;
}

void *m_pedir(u32 n)
{
	u32 t, b;
	int c;
	Suelto *s;
	if (!cima) {
		apunta(M_INI, M_FIN - M_INI);
		cima = M_INI;
		if (!crecer(cima + 4))
			return 0;
		CAB(cima) = ANTES_USADO;
	}
	t = (n + 8 + 7) & ~7u;
	if (t < 24)
		t = 24;
	if (t < n)
		return 0;
	for (c = cajita(t); c < NCAJ; c++)
		for (s = caja[c]; s; s = s->sig)
			if (TAM(s->cab) >= t) {
				u32 tt = TAM(s->cab);
				b = (u32)s;
				sacar(b);
				if (tt - t >= 24) {
					CAB(b) = t | USADO | ANTES_USADO;
					CAB(b + t) = (tt - t) | ANTES_USADO;
					meter(b + t);
				} else {
					CAB(b) = tt | USADO | ANTES_USADO;
					CAB(b + tt) |= ANTES_USADO;
				}
				CAB(b + 4) = n;
				return (void *)(b + 8);
			}
	b = cima;
	if (!crecer(b + t + 4))
		return 0;
	CAB(b) = t | USADO | (CAB(b) & ANTES_USADO);
	cima = b + t;
	CAB(cima) = ANTES_USADO;
	CAB(b + 4) = n;
	return (void *)(b + 8);
}

void *m_cero(u32 n)
{
	void *p = m_pedir(n);
	if (p)
		memset(p, 0, n);
	return p;
}

void m_soltar(void *p)
{
	u32 b, t, h, sig;
	if (!p)
		return;
	b = (u32)p - 8;
	if (b < M_INI || b >= cima || (b & 7) || !(CAB(b) & USADO))
		return;
	h = CAB(b);
	t = TAM(h);
	sig = b + t;
	if (!(h & ANTES_USADO)) {
		u32 ta = *(u32 *)(b - 4);
		b -= ta;
		sacar(b);
		t += ta;
	}
	if (sig == cima) {
		cima = b;
		CAB(cima) = ANTES_USADO;
		return;
	}
	if (!(CAB(sig) & USADO)) {
		sacar(sig);
		t += TAM(CAB(sig));
	} else
		CAB(sig) &= ~ANTES_USADO;
	CAB(b) = t | ANTES_USADO;
	meter(b);
}

u32 m_tam(void *p)
{
	if (!p)
		return 0;
	return CAB((u32)p - 4);
}

void *m_cambiar(void *p, u32 n)
{
	void *q;
	u32 v;
	if (!p)
		return m_pedir(n);
	v = m_tam(p);
	if (n <= TAM(CAB((u32)p - 8)) - 8 && n + 256 > v / 2) {
		CAB((u32)p - 4) = n;
		return p;
	}
	q = m_pedir(n);
	if (!q)
		return 0;
	memcpy(q, p, v < n ? v : n);
	m_soltar(p);
	return q;
}
