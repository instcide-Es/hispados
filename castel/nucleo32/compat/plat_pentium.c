/* plat_pentium.c - (COMPAT) plat.h en Hispa-Pentium, con las llamadas del
 * núcleo (int 0x80; docs/PENTIUM.md, sección 6): 27 redirigir la salida,
 * 10 y 11 lanzar y esperar, 68 y 200 el usuario, 157 el modo
 * administrador, 210 el azar, 80 colocar la ventana; la red como Correo y
 * el Termómetro: RED.BIN (90, 97) y TLS.BIN (99), con la cadena de
 * certificados comprobada por TLS.BIN.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "plat.h"
#include "texto.h"

#define ES_ERROR(r) ((u32)(r) >= 0xFFFFFFF0u)
#define ES_ERROR_RED(r) ((u32)(r) >= 0xFFFFFFE0u)
#define SIGUE 0xFFFFFF00u
#define SIN_SALIDA 0xFFFFFFFFu
#define SC_RED 90
#define SC_ESTADO_RED 97
#define SC_TLS 99
#define O_RESOLVER 2
#define O_CONECTAR 4
#define O_ENVIAR 5
#define O_RECIBIR 6
#define O_CERRAR 7
#define RED_CONECTADA 4
#define MAX_DATOS 8192

static u32 sc(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;

	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}

static void a_437(const char *s, char *d, u32 cabe) { utf8_a_437(s, d, cabe); }

/* la unidad del sistema (la de \HISPA), como ACTUALIZ */
static char sistema(void)
{
	static char u;
	u8 i_[128];
	u32 i;

	if (u)
		return u;
	u = 'C';
	for (i = 0; i < 26 && sc(43, i, (u32)i_, 0) == 1; i++)
		if (i_[12] == 1 && i_[0]) {
			u = (char)i_[0];
			break;
		}
	return u;
}

int plat_comprueba(const char *args, const char *salida, int *codigo, void (*espera)(void *), void *u, char *error, u32 cabe)
{
	char s4[300], a4[600], p[40];
	u32 m, t, c = SIGUE;

	a_437(salida, s4, sizeof s4);
	a_437(args, a4, sizeof a4);
	m = sc(7, (u32)s4, 2, 0);
	if (ES_ERROR(m)) {
		fmt(error, cabe, "No se puede escribir %s.", salida);
		return -1;
	}
	sc(27, m, 0, 0);
	fmt(p, sizeof p, "%c:\\HISPA\\WIN32.BIN", sistema());
	t = sc(10, (u32)p, (u32)a4, 0);
	if (ES_ERROR(t)) {
		/* (en la memoria USB de arranque, en la raíz) */
		fmt(p, sizeof p, "%c:\\WIN32.BIN", sistema());
		t = sc(10, (u32)p, (u32)a4, 0);
	}
	if (ES_ERROR(t)) {
		sc(27, SIN_SALIDA, 0, 0);
		sc(9, m, 0, 0);
		copia(error, "No está WIN32.BIN (el paquete «win32»: Programas y actualizaciones).", cabe);
		return -1;
	}
	while (c == SIGUE) {
		if (espera)
			espera(u);
		sc(3, 5, 0, 0);
		c = sc(11, t, 1, 0);
	}
	sc(27, SIN_SALIDA, 0, 0);
	sc(9, m, 0, 0);
	*codigo = (int)c;
	return 0;
}

/* el nombre de la carpeta del usuario, como lo hace WIN32.BIN
 * (diagnostico.c, prepara_carpeta_usuario): las letras y cifras de su
 * nombre, en mayúsculas, hasta 8 */
static void carpeta_win32(char *d, u32 cabe)
{
	char b[64], n[12];
	u32 k, j = 0;

	b[0] = 0;
	sc(68, 1, (u32)b, sizeof b);
	if (!b[0])
		copia(b, "Usuario", sizeof b);
	for (k = 0; b[k] && j < 8; k++) {
		char ch = b[k];

		if (ch >= 'a' && ch <= 'z')
			ch = (char)(ch - 32);
		if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
			n[j++] = ch;
	}
	n[j] = 0;
	if (!j)
		copia(n, "USUARIO", sizeof n);
	fmt(d, cabe, "%c:\\USUARIOS\\%s", sistema(), n);
}

int plat_carpeta_usuario(char *d, u32 cabe)
{
	u32 r = sc(200, 0, 0, 0);
	char t[200];

	if (!ES_ERROR(r) && r > 0 && r < 1000) {
		r = sc(200, 4, (u32)t, sizeof t);
		if (!ES_ERROR(r) && r > 2 && r < sizeof t) {
			t[r] = 0;
			while (r > 3 && t[r - 1] == '\\')
				t[--r] = 0;
			de437_a_utf8((const u8 *)t, r, d, cabe);
			return 0;
		}
	}
	carpeta_win32(d, cabe);
	{
		char c[40];

		fmt(c, sizeof c, "%c:\\USUARIOS", sistema());
		if (so_existe(c) != 2)
			plat_haz_carpeta(c);
	}
	if (so_existe(d) != 2)
		plat_haz_carpeta(d);
	return so_existe(d) == 2 ? 0 : -1;
}

int plat_ruta_log(char *d, u32 cabe)
{
	char c[200];

	carpeta_win32(c, sizeof c);
	fmt(d, cabe, "%s\\WIN32.LOG", c);
	return 0;
}

void plat_carpeta_compat(char *d, u32 cabe) { fmt(d, cabe, "%c:\\HISPA\\COMPAT", sistema()); }
void plat_carpeta_win32(char *d, u32 cabe) { fmt(d, cabe, "%c:\\WIN32", sistema()); }
void plat_carpeta_paquetes(char *d, u32 cabe) { fmt(d, cabe, "%c:\\HISPA\\PAQUETES", sistema()); }

int plat_haz_carpeta(const char *ruta)
{
	char r[300];
	u32 x;

	a_437(ruta, r, sizeof r);
	x = sc(23, (u32)r, 0, 0);
	return ES_ERROR(x) && so_existe(ruta) != 2 ? -1 : 0;
}

int plat_azar(u8 *d, u32 n)
{
	u32 r = sc(210, (u32)d, n, 0);

	return ES_ERROR(r) ? -1 : 0;
}

int plat_es_admin(void) { return sc(157, 2, 0, 0) == 1; }
int plat_pide_admin(void) { return sc(157, 0, 0, 0) == 1; }
void plat_deja_admin(void) { sc(157, 1, 0, 0); }
void plat_coloca(int v, int x, int y) { sc(80, (u32)v, (u32)x, (u32)y); }

int plat_lanza(const char *programa, const char *args)
{
	char p[300], a[400];
	u32 t;

	a_437(programa, p, sizeof p);
	a_437(args, a, sizeof a);
	t = sc(41, (u32)p, (u32)a, 0);
	return ES_ERROR(t) ? -1 : 0;
}

void plat_escribe(const char *t)
{
	char b[1100];
	u32 n = largo(t);

	while (n) {
		u32 k = n > 360 ? 360 : n, m;

		/* (sin partir una letra de UTF-8) */
		while (k < n && ((u8)t[k] & 0xC0) == 0x80)
			k--;
		{
			char x[400];

			memcpy(x, t, k);
			x[k] = 0;
			m = utf8_a_437(x, b, sizeof b);
		}
		sc(1, (u32)b, m, 0);
		t += k;
		n -= k;
	}
}

int plat_hay_escritorio(void)
{
	u32 r = sc(15, 13, 0, 0);

	return !ES_ERROR(r) && r != 0;
}

typedef struct { u32 m; } archivo;

void *plat_abre(const char *ruta, u32 *tam)
{
	char r[300];
	u32 m, l;
	archivo *a;

	a_437(ruta, r, sizeof r);
	m = sc(7, (u32)r, 0, 0);
	if (ES_ERROR(m))
		return NULL;
	l = sc(13, m, 0, 0);
	a = so_pide(sizeof *a);
	if (ES_ERROR(l) || !a) {
		so_suelta(a);
		sc(9, m, 0, 0);
		return NULL;
	}
	a->m = m;
	*tam = l;
	return a;
}

i32 plat_lee_en(void *f, u32 pos, void *d, u32 n)
{
	archivo *a = f;
	u32 r, h = 0;

	if (ES_ERROR(sc(19, a->m, pos, 0)))
		return -1;
	while (h < n) {
		r = sc(8, a->m, (u32)d + h, n - h);
		if (ES_ERROR(r))
			return -1;
		if (!r)
			break;
		h += r;
	}
	return (i32)h;
}

void plat_cierra(void *f)
{
	archivo *a = f;

	if (!a)
		return;
	sc(9, a->m, 0, 0);
	so_suelta(a);
}

/* ---- la red ---- */
struct plat_con { u32 c; int tls; };
static u32 cab[8];

static u32 pide(u32 sv, u32 orden, u32 a, u32 b, u32 c, const void *datos, u32 largo_, void *dest, u32 cabe)
{
	cab[0] = orden, cab[1] = a, cab[2] = b, cab[3] = c;
	cab[4] = cab[5] = cab[6] = 0;
	cab[7] = largo_;
	if (largo_ && dest && dest != datos)
		memcpy(dest, datos, largo_);
	return sc(sv, (u32)cab, (u32)(dest ? dest : (void *)datos), cabe);
}

static u32 hz(void)
{
	u32 h = sc(15, 9, 0, 0);

	return ES_ERROR(h) || !h ? 100 : h;
}

static const char *texto_red(u32 e)
{
	switch (e) {
	case 0xFFFFFFEFu: return "el servicio de red (RED.BIN) no está en marcha";
	case 0xFFFFFFEEu: return "no hay una tarjeta de red que se sepa llevar";
	case 0xFFFFFFEDu: return "no hay cable ni Wi-Fi conectado";
	case 0xFFFFFFECu: return "todavía no hay dirección IP";
	case 0xFFFFFFEBu: return "el servidor no contesta a tiempo";
	case 0xFFFFFFEAu: return "el servidor rechaza la conexión";
	case 0xFFFFFFE9u: return "no se encuentra el servidor (¿hay salida a Internet?)";
	case 0xFFFFFFE8u: return "se ha cortado la conexión";
	case 0xFFFFFFE6u: return "no se llega al servidor";
	case 0xFFFFFFE5u: return "el certificado del servidor no vale";
	case 0xFFFFFFE4u: return "la conexión cifrada ha fallado";
	case 0xFFFFFFE3u: return "falta el servicio TLS (TLS.BIN)";
	case 0xFFFFFFE2u: return "el reloj del equipo no está en hora (hace falta para los certificados)";
	}
	return "la red ha fallado";
}

plat_con *plat_conecta(const char *host, u32 puerto, int tls, char *error, u32 cabe)
{
	char nom[160], t[240];
	u32 n = largo(host), r;
	plat_con *c;

	if (n >= sizeof nom) {
		copia(error, "un nombre de servidor demasiado largo", cabe);
		return NULL;
	}
	memcpy(nom, host, n + 1);
	if (!tls) {
		r = pide(SC_RED, O_RESOLVER, 0, 0, 0, nom, n, t, sizeof t);
		if (r != 0) {
			copia(error, texto_red(r), cabe);
			return NULL;
		}
		r = pide(SC_RED, O_CONECTAR, cab[1], puerto, 20 * hz(), NULL, 0, NULL, 0);
	} else {
		r = pide(SC_TLS, O_CONECTAR, 0, puerto, 30 * hz(), nom, n, t, sizeof t - 1);
		if (ES_ERROR_RED(r) && cab[7] > 0 && cab[7] < sizeof t) {
			t[cab[7]] = 0;
			fmt(error, cabe, "%s (%s)", texto_red(r), t);
			return NULL;
		}
	}
	if (ES_ERROR_RED(r)) {
		copia(error, texto_red(r), cabe);
		return NULL;
	}
	c = so_pide(sizeof *c);
	if (!c) {
		pide(tls ? SC_TLS : SC_RED, O_CERRAR, r, 0, 0, NULL, 0, NULL, 0);
		copia(error, "no hay memoria", cabe);
		return NULL;
	}
	c->c = r;
	c->tls = tls;
	return c;
}

int plat_envia(plat_con *c, const void *d_, u32 n)
{
	const u8 *d = d_;

	while (n) {
		u32 k = n > MAX_DATOS ? MAX_DATOS : n;
		u32 r = pide(c->tls ? SC_TLS : SC_RED, O_ENVIAR, c->c, 0, 0, d, k, NULL, 0);

		if (ES_ERROR_RED(r) || r == 0)
			return -1;
		d += r;
		n -= r;
	}
	return 0;
}

i32 plat_recibe(plat_con *c, void *d, u32 cabe, u32 segundos)
{
	u32 r = pide(c->tls ? SC_TLS : SC_RED, O_RECIBIR, c->c, segundos * hz(), 0, NULL, 0, d, cabe > MAX_DATOS ? MAX_DATOS : cabe);

	if (r == 0 || r == 0xFFFFFFE8u)
		return 0;
	if (ES_ERROR_RED(r))
		return -1;
	return (i32)r;
}

void plat_desconecta(plat_con *c)
{
	if (!c)
		return;
	pide(c->tls ? SC_TLS : SC_RED, O_CERRAR, c->c, 0, 0, NULL, 0, NULL, 0);
	so_suelta(c);
}

int plat_hay_red(u32 segundos)
{
	u32 ip, i;

	/* (sin escritorio, RED.BIN se pone en marcha cuando un programa pide
	 * la 90: con una cabecera nula, el núcleo lo pone y luego dice NO_VALE) */
	if (sc(SC_ESTADO_RED, (u32)&ip, 0, 0) == 0)
		sc(SC_RED, 0, 0, 0);
	for (i = 0; i <= segundos * 2; i++) {
		if (sc(SC_ESTADO_RED, (u32)&ip, 0, 0) == RED_CONECTADA)
			return 1;
		sc(3, hz() / 2, 0, 0);
	}
	return 0;
}
