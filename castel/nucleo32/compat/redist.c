/* redist.c - (COMPAT) los componentes redistribuibles de Microsoft, al
 * estilo de winetricks: la lista (\HISPA\COMPAT\REDIST.LST, la del paquete:
 * solo los que valen de verdad con WIN32, docs/PENTIUM.md), bajarlos de su
 * URL oficial (download.microsoft.com) a la carpeta del usuario y
 * comprobar su SHA-256 (el medido al hacer la lista: Microsoft no lo
 * publica), enseñar la licencia que traen dentro, y sacar solo sus DLL a
 * \WIN32 (con el modo administrador). No se redistribuyen: los baja quien
 * los usa, de Microsoft, y acepta su licencia.
 * Por dentro son ejecutables que se extraen solos con un CAB (LZX) dentro
 * y, a veces, otro CAB dentro de ese («vc_red.cab»), donde las DLL llevan
 * nombres de Windows Installer: se reconocen por el nombre de su directorio
 * de exportaciones (pe.c).
 * REDIST.LST, por bloques:
 *   [vc2010]
 *   nombre=..., url=https://download.microsoft.com/..., tam=, sha256=,
 *   interior=vc_red.cab (o nada), dlls=MSVCR100.DLL,MSVCP100.DLL,
 *   licencia=3082\eula.rtf, licencia_url=https://..., para=...
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "plat.h"
#include "http.h"
#include "cab.h"
#include "pe.h"
#include "sha256.h"
#include "texto.h"

#define MAX_REDIST 16
#define MAX_DLL_SACADA (16u << 20)

static void valor(const char *l, const char *clave, char *d, u32 cabe)
{
	u32 n = largo(clave);

	if (!strncmp(l, clave, n) && l[n] == '=') {
		u32 j = 0;

		l += n + 1;
		while (*l && *l != '\r' && *l != '\n' && j + 1 < cabe)
			d[j++] = *l++;
		d[j] = 0;
	}
}

int red_lista(c_redist **rr, int *n)
{
	char c[RUTA_MAX], r[RUTA_MAX], *t, t2[24];
	const char *p;
	c_redist *v;

	*rr = NULL;
	*n = 0;
	plat_carpeta_compat(c, sizeof c);
	une_ruta(r, sizeof r, c, "REDIST.LST");
	t = lee_texto(r, NULL, 1u << 20);
	if (!t)
		return -1;
	v = so_pide(sizeof *v * MAX_REDIST);
	if (!v) {
		so_suelta(t);
		return -1;
	}
	memset(v, 0, sizeof *v * MAX_REDIST);
	for (p = t; *p;) {
		if (*p == '[' && *n < MAX_REDIST) {
			u32 j = 0;

			p++;
			(*n)++;
			while (*p && *p != ']' && j + 1 < sizeof v[0].id)
				v[*n - 1].id[j++] = *p++;
		} else if (*n && *p != '#') {
			c_redist *x = &v[*n - 1];

			valor(p, "nombre", x->nombre, sizeof x->nombre);
			valor(p, "url", x->url, sizeof x->url);
			t2[0] = 0;
			valor(p, "tam", t2, sizeof t2);
			if (t2[0])
				x->tam = (u32)numero(t2, NULL);
			valor(p, "sha256", x->sha, sizeof x->sha);
			valor(p, "interior", x->interior, sizeof x->interior);
			valor(p, "dlls", x->dlls, sizeof x->dlls);
			valor(p, "licencia", x->licencia, sizeof x->licencia);
			valor(p, "licencia_url", x->licencia_url, sizeof x->licencia_url);
			valor(p, "para", x->para, sizeof x->para);
		}
		while (*p && *p != '\n')
			p++;
		while (*p == '\n' || *p == '\r')
			p++;
	}
	so_suelta(t);
	/* (los que no traen lo que hace falta, fuera: sin URL por HTTPS (o la de
	 * las pruebas en Linux, http://127.0.0.1), sin suma o sin DLL no se bajan) */
	{
		int k, m = 0;

		for (k = 0; k < *n; k++)
			if (v[k].id[0] && (empieza(v[k].url, "https://") || empieza(v[k].url, "http://127.0.0.1:")) && largo(v[k].sha) == 64 && v[k].dlls[0] && v[k].tam)
				v[m++] = v[k];
		*n = m;
	}
	*rr = v;
	return 0;
}

/* la DLL k (desde 0) de la lista «A.DLL,B.DLL»: 0 si no hay tantas */
static int dll_n(const char *l, int k, char *d, u32 cabe)
{
	u32 j = 0;

	while (k > 0 && *l) {
		if (*l++ == ',')
			k--;
	}
	if (!*l)
		return 0;
	while (*l == ' ')
		l++;
	while (*l && *l != ',' && j + 1 < cabe)
		d[j++] = *l++;
	d[j] = 0;
	a_mayusculas(d);
	return j > 0;
}

int red_puesto(const c_redist *r)
{
	char c[RUTA_MAX], d[40], x[RUTA_MAX];
	int k, hay = 0, todas = 0;

	plat_carpeta_win32(c, sizeof c);
	for (k = 0; dll_n(r->dlls, k, d, sizeof d); k++) {
		todas++;
		une_ruta(x, sizeof x, c, d);
		if (so_existe(x) == 1)
			hay++;
	}
	return !hay ? 0 : hay == todas ? 2 : 1;
}

/* ---- bajar ---- */
typedef struct {
	void *f;
	sha256 s;
	u32 n, max;
	red_avance av;
	void *u;
	const char *que;
} bajada;

static int a_archivo(void *u, const u8 *d, u32 n, u32 total)
{
	bajada *b = u;

	if (b->n + n > b->max)
		return -1;
	if (so_escribe(b->f, d, n))
		return -1;
	sha_anade(&b->s, d, n);
	b->n += n;
	if (b->av && (!(b->n & 0x3FFFF) || (b->n & 0x3FFFF) < n))
		b->av(b->u, b->que, b->n, total ? total : b->max);
	return 0;
}

static void carpeta_bajadas(char *d, u32 cabe)
{
	char c[RUTA_MAX];

	if (hist_carpeta(c, sizeof c))
		c[0] = 0;
	une_ruta(d, cabe, c, "BAJADAS");
	if (so_existe(d) != 2)
		plat_haz_carpeta(d);
}

int red_baja(const c_redist *r, char *ruta, u32 cabe, red_avance av, void *u, char *error, u32 cabe_e)
{
	char c[RUTA_MAX], url[220], ub[220], nom[16], e[200];
	bajada b;
	u8 h[32];
	char hx[65];
	int est, salto;
	const char *err;

	carpeta_bajadas(c, sizeof c);
	copia(nom, r->id, 9);
	a_mayusculas(nom);
	anade(nom, ".EXE", sizeof nom);
	une_ruta(ruta, cabe, c, nom);
	/* (si ya está bajado y cuadra, no se baja otra vez) */
	{
		void *f = plat_abre(ruta, &b.n);

		if (f) {
			u32 p = 0, t = b.n;
			u8 *x = so_pide(65536);
			int bien = t == r->tam && x;

			sha_empieza(&b.s);
			while (bien && p < t) {
				u32 k = t - p < 65536 ? t - p : 65536;

				bien = plat_lee_en(f, p, x, k) == (i32)k;
				sha_anade(&b.s, x, k);
				p += k;
			}
			so_suelta(x);
			plat_cierra(f);
			if (bien) {
				sha_acaba(&b.s, h);
				sha_hex(h, hx);
				if (iguales_sin_caja(hx, r->sha))
					return 0;
			}
			so_borra(ruta);
		}
	}
	if (!plat_hay_red(20)) {
		copia(error, "No hay red: enchufe el cable o conecte el Wi-Fi (Inicio > Configuración > Red).", cabe_e);
		return -1;
	}
	copia(url, r->url, sizeof url);
	for (salto = 0; salto < 3; salto++) {
		memset(&b, 0, sizeof b);
		b.f = so_crea(ruta, &err);
		if (!b.f) {
			fmt(error, cabe_e, "No se puede guardar en %s.", ruta);
			return -1;
		}
		sha_empieza(&b.s);
		b.max = r->tam;
		b.av = av;
		b.u = u;
		b.que = "Bajando";
		if (http_pide(url, "GET", NULL, NULL, 0, a_archivo, &b, &est, ub, sizeof ub, e, sizeof e)) {
			so_cierra(b.f);
			so_borra(ruta);
			fmt(error, cabe_e, "No se ha podido bajar: %s.", e);
			return -1;
		}
		so_cierra(b.f);
		if ((est == 301 || est == 302 || est == 303 || est == 307 || est == 308) && ub[0]) {
			http_url w;

			so_borra(ruta);
			/* (solo a HTTPS de Microsoft) */
			if (http_parte(ub, &w) || !w.tls || (!iguales_sin_caja(w.host, "download.microsoft.com") && !busca(r->url, "10.0.2.2"))) {
				fmt(error, cabe_e, "Microsoft manda a otro sitio (%s): no se sigue.", ub);
				return -1;
			}
			copia(url, ub, sizeof url);
			continue;
		}
		break;
	}
	if (est != 200) {
		so_borra(ruta);
		fmt(error, cabe_e, est == 404 ? "Microsoft ya no lo tiene en esa dirección (404)." : "El servidor contesta %d.", est);
		return -1;
	}
	sha_acaba(&b.s, h);
	sha_hex(h, hx);
	if (b.n != r->tam || !iguales_sin_caja(hx, r->sha)) {
		so_borra(ruta);
		fmt(error, cabe_e, "Lo bajado no es lo que tiene que ser (mide %u, su SHA-256 no cuadra): se ha borrado.", b.n);
		return -1;
	}
	return 0;
}

/* ---- el CAB de dentro ---- */
typedef struct {
	const char *quiere;	/* el nombre (sin carpeta) que se busca, o NULL */
	const char *dlls;	/* las DLL que se buscan */
	const char *destino;	/* la carpeta (o el archivo, con «quiere») */
	void *f;
	int hechas;
	char error[160];
	red_avance av;
	void *u;
	/* (las DLL, en la memoria hasta saber cuál es) */
	struct { const cab_archivo *a; u8 *d; u32 n; } en[4];
	char sacadas[512];
} saca;

static const char *base_cab(const char *n)
{
	const char *b = n, *p;

	for (p = n; *p; p++)
		if (*p == '\\' || *p == '/')
			b = p + 1;
	return b;
}

/* el nombre de dentro del CAB sin los «.\» del principio */
static const char *sin_puntos(const char *n)
{
	while (n[0] == '.' && (n[1] == '\\' || n[1] == '/'))
		n += 2;
	return n;
}

static int quiere_uno(void *u, const cab_archivo *a)
{
	saca *s = u;

	/* («3082\eula.rtf»: con su carpeta; «vc_red.cab»: por el nombre) */
	if (strchr(s->quiere, '\\'))
		return iguales_sin_caja(sin_puntos(a->nombre), s->quiere);
	return iguales_sin_caja(base_cab(a->nombre), s->quiere);
}

static int escribe_uno(void *u, const cab_archivo *a, const u8 *d, u32 n, int ultimo)
{
	saca *s = u;
	const char *e;

	(void)a;
	if (!s->f) {
		s->f = so_crea(s->destino, &e);
		if (!s->f)
			return -1;
	}
	if (n && so_escribe(s->f, d, n))
		return -1;
	if (ultimo) {
		so_cierra(s->f);
		s->f = NULL;
		s->hechas++;
	}
	return 0;
}

/* ¿puede ser una de las DLL? (por el nombre que le da el CAB: «msvcr100.dll»,
 * «F_CENTRAL_msvcr100_x86», «msvcr90.dll.30729...»); cuál es de verdad lo
 * dice luego su directorio de exportaciones */
static int parece(const char *nombre, const char *dlls)
{
	char m[160], d[40];
	int k;

	copia(m, base_cab(nombre), sizeof m);
	a_mayusculas(m);
	for (k = 0; dll_n(dlls, k, d, sizeof d); k++) {
		char raiz[40];
		u32 n = largo(d);

		copia(raiz, d, sizeof raiz);
		if (n > 4)
			raiz[n - 4] = 0;
		if (busca(m, raiz))
			return 1;
	}
	return 0;
}

static int quiere_dll(void *u, const cab_archivo *a)
{
	saca *s = u;

	return a->tam > 0x40 && a->tam <= MAX_DLL_SACADA && parece(a->nombre, s->dlls);
}

static int es_de_la_lista(const char *nombre, const char *dlls)
{
	char d[40];
	int k;

	for (k = 0; dll_n(dlls, k, d, sizeof d); k++)
		if (iguales(d, nombre))
			return 1;
	return 0;
}

static int escribe_dll(void *u, const cab_archivo *a, const u8 *d, u32 n, int ultimo)
{
	saca *s = u;
	int k, l = -1;

	for (k = 0; k < 4; k++)
		if (s->en[k].a == a)
			l = k;
	if (l < 0) {
		for (k = 0; k < 4 && l < 0; k++)
			if (!s->en[k].a)
				l = k;
		if (l < 0)
			return -1;
		s->en[l].a = a;
		s->en[l].d = so_pide(a->tam ? a->tam : 1);
		s->en[l].n = 0;
		if (!s->en[l].d) {
			s->en[l].a = NULL;
			return -1;
		}
	}
	if (s->en[l].n + n > a->tam)
		return -1;
	memcpy(s->en[l].d + s->en[l].n, d, n);
	s->en[l].n += n;
	if (ultimo) {
		pe_info pi;

		if (!pe_mira(s->en[l].d, s->en[l].n, &pi) && pi.dll && es_de_la_lista(pi.nombre, s->dlls) &&
		    !busca(s->sacadas, pi.nombre)) {
			char r[RUTA_MAX];
			const char *e;
			void *f;

			une_ruta(r, sizeof r, s->destino, pi.nombre);
			f = so_crea(r, &e);
			if (!f || so_escribe(f, s->en[l].d, s->en[l].n) || so_cierra(f)) {
				fmt(s->error, sizeof s->error, "No se puede escribir %s.", r);
				so_suelta(s->en[l].d);
				s->en[l].a = NULL;
				return -1;
			}
			anade(s->sacadas, pi.nombre, sizeof s->sacadas);
			anade(s->sacadas, " ", sizeof s->sacadas);
			s->hechas++;
		}
		so_suelta(s->en[l].d);
		s->en[l].a = NULL;
	}
	return 0;
}

static void avance_cab(void *u, u32 hecho, u32 total)
{
	saca *s = u;

	if (s->av)
		s->av(s->u, "Sacando", hecho, total);
}

typedef struct { void *f; u32 tam; } origen;
static i32 lee_origen(void *u, u32 pos, void *d, u32 n)
{
	origen *o = u;

	return plat_lee_en(o->f, pos, d, n);
}

/* abre el primer CAB del archivo (un .CAB, o un .EXE que lo lleva dentro) */
static int abre_cab(const char *ruta, origen *o, cab *c, char *error, u32 cabe)
{
	i32 p;
	const char *e;

	o->f = plat_abre(ruta, &o->tam);
	if (!o->f) {
		fmt(error, cabe, "No se puede abrir %s.", ruta);
		return -1;
	}
	p = cab_busca(lee_origen, o, o->tam, 0);
	if (p < 0 || cab_abre(c, lee_origen, o, (u32)p, o->tam, &e)) {
		plat_cierra(o->f);
		copia(error, "No se encuentra el CAB de dentro (¿es el paquete de Microsoft que tiene que ser?).", cabe);
		return -1;
	}
	return 0;
}

/* RTF a texto (lo justo para leer una licencia) */
static void rtf_a_texto(const char *s, u32 n, bufer *b)
{
	u32 k = 0;
	int prof = 0, salta = -1;

	while (k < n) {
		char c = s[k];

		if (c == '{') {
			prof++;
			if (k + 2 < n && s[k + 1] == '\\' && s[k + 2] == '*' && salta < 0)
				salta = prof;
			k++;
		} else if (c == '}') {
			if (salta == prof)
				salta = -1;
			prof--;
			k++;
		} else if (c == '\\') {
			char pal[32];
			u32 j = 0;
			long v = 0;
			int hay_v = 0, neg = 0;

			k++;
			if (k < n && (s[k] == '\\' || s[k] == '{' || s[k] == '}')) {
				if (salta < 0)
					buf_c(b, (u8)s[k]);
				k++;
				continue;
			}
			if (k < n && s[k] == '\'') {
				/* \'xx: de la 1252 */
				u32 x = 0, q;

				for (q = 1; q <= 2 && k + q < n; q++) {
					char h = s[k + q];

					x = x * 16 + (u32)(h >= 'a' ? h - 'a' + 10 : h >= 'A' ? h - 'A' + 10 : h - '0');
				}
				k += 3;
				if (salta < 0)
					buf_u32(b, x == 0x80 ? 0x20AC : x == 0x93 || x == 0x94 ? '"' : x == 0x92 || x == 0x91 ? '\'' : x == 0x96 || x == 0x97 ? '-' : x);
				continue;
			}
			while (k < n && ((s[k] >= 'a' && s[k] <= 'z') || (s[k] >= 'A' && s[k] <= 'Z')) && j + 1 < sizeof pal)
				pal[j++] = s[k++];
			pal[j] = 0;
			if (k < n && s[k] == '-')
				neg = 1, k++;
			while (k < n && s[k] >= '0' && s[k] <= '9')
				v = v * 10 + (s[k++] - '0'), hay_v = 1;
			if (k < n && s[k] == ' ')
				k++;
			if (salta >= 0)
				continue;
			if (iguales(pal, "par") || iguales(pal, "line") || iguales(pal, "sect"))
				buf_c(b, '\n');
			else if (iguales(pal, "tab"))
				buf_c(b, ' ');
			else if (iguales(pal, "u") && hay_v) {
				buf_u32(b, (u32)(neg ? 65536 - v : v));
				/* (y la letra de repuesto que sigue) */
				if (k < n && s[k] != '\\' && s[k] != '{' && s[k] != '}')
					k++;
			} else if (iguales(pal, "fonttbl") || iguales(pal, "colortbl") || iguales(pal, "stylesheet") || iguales(pal, "info") ||
				   iguales(pal, "pict") || iguales(pal, "header") || iguales(pal, "footer"))
				salta = prof;
		} else {
			if (salta < 0 && c != '\r' && c != '\n')
				buf_c(b, (u8)c);
			k++;
		}
	}
}

char *red_licencia(const c_redist *r, const char *ruta, char *error, u32 cabe)
{
	origen o;
	cab c;
	saca s;
	char tmp[RUTA_MAX], car[RUTA_MAX], *t;
	u32 n;
	const char *e;
	bufer b;

	memset(&b, 0, sizeof b);
	if (!r->licencia[0]) {
		fmt(error, cabe, "Este paquete no trae la licencia en un archivo aparte: léala en %s", r->licencia_url);
		return NULL;
	}
	carpeta_bajadas(car, sizeof car);
	une_ruta(tmp, sizeof tmp, car, "LICENCIA.TMP");
	if (empieza(r->licencia, "recurso:")) {
		/* (un recurso del propio ejecutable: el «LICENSE» de los de IExpress) */
		u32 nb, de, tm;
		const char *er;
		u8 *x = so_lee_archivo(ruta, &nb, 64u << 20, &er);

		if (!x || pe_recurso(x, nb, 10, r->licencia + 8, &de, &tm)) {
			so_suelta(x);
			fmt(error, cabe, "No se encuentra la licencia (%s) en el paquete.", r->licencia);
			return NULL;
		}
		escribe_texto(tmp, (const char *)x + de, tm);
		so_suelta(x);
		goto leida;
	}
	if (abre_cab(ruta, &o, &c, error, cabe))
		return NULL;
	memset(&s, 0, sizeof s);
	s.quiere = r->licencia;
	s.destino = tmp;
	if (cab_saca(&c, quiere_uno, escribe_uno, NULL, &s, &e) || !s.hechas) {
		cab_cierra(&c);
		plat_cierra(o.f);
		fmt(error, cabe, "No se encuentra la licencia (%s) dentro: %s", r->licencia, e ? e : "no está");
		return NULL;
	}
	cab_cierra(&c);
	plat_cierra(o.f);
leida:
	t = lee_texto(tmp, &n, 1u << 20);
	so_borra(tmp);
	if (!t) {
		copia(error, "No se puede leer la licencia.", cabe);
		return NULL;
	}
	if (n >= 2 && (u8)t[0] == 0xFF && (u8)t[1] == 0xFE) {
		/* UTF-16 (LE) */
		u32 k;

		for (k = 2; k + 1 < n; k += 2) {
			u32 ch = (u8)t[k] | (u32)(u8)t[k + 1] << 8;

			if (ch != '\r')
				buf_u32(&b, ch);
		}
	} else if (n >= 5 && !memcmp(t, "{\\rtf", 5))
		rtf_a_texto(t, n, &b);
	else {
		u32 k = (n >= 3 && (u8)t[0] == 0xEF && (u8)t[1] == 0xBB && (u8)t[2] == 0xBF) ? 3 : 0;

		/* (UTF-8 si lo parece; si no, de la 1252) */
		u32 j;
		int utf8 = 1;

		for (j = k; j < n && utf8; j++)
			if ((u8)t[j] >= 0x80) {
				u32 ch;
				int l = utf8_lee((const u8 *)t + j, n - j, &ch);

				if (ch == 0xFFFD)
					utf8 = 0;
				j += (u32)l - 1;
			}
		for (; k < n; k++)
			if (t[k] == '\r' || !t[k])
				continue;
			else if (utf8 || (u8)t[k] < 0x80)
				buf_c(&b, (u8)t[k]);
			else
				buf_u32(&b, (u8)t[k] == 0x80 ? 0x20AC : (u8)t[k] == 0x92 ? '\'' : (u8)t[k] == 0x93 || (u8)t[k] == 0x94 ? '"' : (u8)t[k]);
	}
	so_suelta(t);
	buf_c(&b, 0);
	if (b.mal) {
		buf_suelta(&b);
		copia(error, "No hay memoria.", cabe);
		return NULL;
	}
	return (char *)b.d;
}

int red_saca_a(const c_redist *r, const char *ruta, const char *carpeta, red_avance av, void *u, char *error, u32 cabe)
{
	origen o, o2;
	cab c, c2;
	saca s;
	const char *e;
	char tmp[RUTA_MAX], car[RUTA_MAX];
	const char *fuente = ruta;
	int res;

	if (abre_cab(ruta, &o, &c, error, cabe))
		return -1;
	memset(&s, 0, sizeof s);
	s.av = av;
	s.u = u;
	if (r->interior[0]) {
		/* el CAB de dentro, a un archivo */
		carpeta_bajadas(car, sizeof car);
		une_ruta(tmp, sizeof tmp, car, "INTERIOR.CAB");
		s.quiere = r->interior;
		s.destino = tmp;
		if (cab_saca(&c, quiere_uno, escribe_uno, avance_cab, &s, &e) || !s.hechas) {
			cab_cierra(&c);
			plat_cierra(o.f);
			so_borra(tmp);
			fmt(error, cabe, "No se encuentra %s dentro: %s", r->interior, e ? e : "no está");
			return -1;
		}
		cab_cierra(&c);
		plat_cierra(o.f);
		fuente = tmp;
		if (abre_cab(fuente, &o2, &c2, error, cabe)) {
			so_borra(tmp);
			return -1;
		}
		o = o2;
		c = c2;
	}
	memset(&s, 0, sizeof s);
	s.av = av;
	s.u = u;
	s.dlls = r->dlls;
	s.destino = carpeta;
	/* (todos los CAB que haya en el archivo: un .MSI lleva varios dentro) */
	res = 0;
	{
		u32 desde = 0;
		int vueltas = 0;

		for (;;) {
			res = cab_saca(&c, quiere_dll, escribe_dll, avance_cab, &s, &e);
			{
				int k;

				for (k = 0; k < 4; k++)
					if (s.en[k].a) {
						so_suelta(s.en[k].d);
						s.en[k].a = NULL;
					}
			}
			desde = c.base + c.tam;
			cab_cierra(&c);
			if (res || ++vueltas >= 64)
				break;
			{
				i32 p = cab_busca(lee_origen, &o, o.tam, desde);
				const char *e2;

				if (p < 0 || cab_abre(&c, lee_origen, &o, (u32)p, o.tam, &e2))
					break;
			}
		}
	}
	plat_cierra(o.f);
	if (fuente != ruta)
		so_borra(fuente);
	if (res) {
		fmt(error, cabe, "%s", s.error[0] ? s.error : e ? e : "no se ha podido sacar");
		return -1;
	}
	if (!s.hechas) {
		copia(error, "Dentro no están las DLL que se esperaban.", cabe);
		return -1;
	}
	return s.hechas;
}

int red_saca(const c_redist *r, const char *ruta, red_avance av, void *u, char *error, u32 cabe)
{
	char w[RUTA_MAX], tmp[RUTA_MAX], car[RUTA_MAX], d[40];
	int n, k, puestas = 0;
	void *h;
	so_entrada e;

	/* primero a una carpeta del usuario; luego, a \WIN32, las que no choquen */
	carpeta_bajadas(car, sizeof car);
	une_ruta(tmp, sizeof tmp, car, "SACADAS");
	if (so_existe(tmp) != 2)
		plat_haz_carpeta(tmp);
	/* (lo que quedara de otra vez) */
	h = so_abre_carpeta(tmp);
	if (h) {
		while (so_lee_carpeta(h, &e) == 1)
			if (!e.carpeta) {
				char x[RUTA_MAX];

				une_ruta(x, sizeof x, tmp, e.nombre);
				so_borra(x);
			}
		so_cierra_carpeta(h);
	}
	n = red_saca_a(r, ruta, tmp, av, u, error, cabe);
	if (n < 0)
		return -1;
	plat_carpeta_win32(w, sizeof w);
	if (!plat_es_admin() && !plat_pide_admin()) {
		copia(error, "Para poner las DLL en \\WIN32 hace falta el modo administrador (se ha dicho que no).", cabe);
		return -1;
	}
	for (k = 0; dll_n(r->dlls, k, d, sizeof d); k++) {
		char de[RUTA_MAX], a[RUTA_MAX];
		u8 *x;
		u32 nx;
		const char *er;

		une_ruta(de, sizeof de, tmp, d);
		une_ruta(a, sizeof a, w, d);
		if (so_existe(de) != 1)
			continue;
		if (so_existe(a) == 1) {
			/* (una que ya hay: si es igual, nada; si es otra, no se pisa) */
			u32 n1, n2;
			u8 *p1 = so_lee_archivo(de, &n1, MAX_DLL_SACADA, &er), *p2 = so_lee_archivo(a, &n2, MAX_DLL_SACADA, &er);
			int igual = p1 && p2 && n1 == n2 && !memcmp(p1, p2, n1);

			so_suelta(p1);
			so_suelta(p2);
			so_borra(de);
			if (igual)
				puestas++;
			continue;
		}
		x = so_lee_archivo(de, &nx, MAX_DLL_SACADA, &er);
		if (!x || escribe_texto(a, (const char *)x, nx)) {
			so_suelta(x);
			fmt(error, cabe, "No se puede escribir %s (¿el núcleo es de antes del Asistente?).", a);
			plat_deja_admin();
			return -1;
		}
		so_suelta(x);
		so_borra(de);
		puestas++;
	}
	plat_deja_admin();
	return puestas;
}

/* (COMPAT /ORDEN cab) lo que hay en el CAB de un archivo; con «nombre», sacar ese a «destino» */
int red_cab(const char *ruta, const char *nombre, const char *destino, void (*di)(const char *f, ...), char *error, u32 cabe)
{
	origen o;
	cab c;
	int k;

	if (abre_cab(ruta, &o, &c, error, cabe))
		return -1;
	if (!nombre) {
		for (k = 0; k < c.n_carpetas; k++)
			di("carpeta %d: %s (ventana %u)\n", k, (c.carpetas[k].tipo & 15) == 3 ? "LZX" : (c.carpetas[k].tipo & 15) == 1 ? "MSZIP" :
			   (c.carpetas[k].tipo & 15) == 0 ? "sin comprimir" : "otra", (c.carpetas[k].tipo >> 8) & 31);
		for (k = 0; k < c.n_archivos; k++)
			di("%10u  %s\n", c.archivos[k].tam, c.archivos[k].nombre);
		cab_cierra(&c);
		plat_cierra(o.f);
		return 0;
	}
	{
		saca s;
		const char *e;
		int r;

		memset(&s, 0, sizeof s);
		s.quiere = nombre;
		s.destino = destino;
		r = cab_saca(&c, quiere_uno, escribe_uno, NULL, &s, &e);
		cab_cierra(&c);
		plat_cierra(o.f);
		if (r || !s.hechas) {
			fmt(error, cabe, "%s", e ? e : "no está");
			return -1;
		}
	}
	return 0;
}

