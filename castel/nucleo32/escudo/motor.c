/* motor.c - ESCUDO.BIN: el motor de análisis. Lee las firmas de ClamAV (su
 * formato: docs.clamav.net, «Signatures»; un motor propio, sin código de
 * libclamav) y mira con ellas un archivo:
 *
 *   - las sumas del archivo entero (.hdb MD5, .hsb MD5/SHA-1/SHA-256, con su
 *     tamaño; y las de «no es un virus», .fp/.sfp, que mandan sobre todas) y
 *     las de cada sección de un ejecutable PE (.mdb/.msb). Solo se calcula
 *     una suma si hay alguna firma de ese tamaño (como hace ClamAV).
 *   - los patrones del cuerpo (.ndb): octetos en hexadecimal con sus comodines
 *     (??, a?, ?a, ~HH, *, {n}, {n-m}, {-n}, {n-}, [n-m], (aa|bb), !(aa|bb),
 *     (aaaa|bbbb), las alternativas de largo variable, (B), (L), (W)), con su
 *     clase de archivo (cualquiera, PE, ELF, Mach-O, PDF, Java) y su
 *     desplazamiento (*, n, n,m, EOF-n, EP+n, EP-n, Sx+n, SEx, SL+n).
 *   - las lógicas (.ldb), con las subfirmas como las del cuerpo y sus
 *     modificadores (::i ::w ::a ::f), la expresión (& | () y las cuentas
 *     =n >n <n con «,m») y lo de la cabecera que se entiende (Target, Engine,
 *     FileSize, EntryPoint, NumberOfSections, Container:CL_TYPE_ZIP). Las que
 *     llevan PCRE, comparación de octetos, macros o iconos se dejan.
 *   - dentro de los ZIP (guardados y deflate; hasta tres niveles).
 * Los patrones se buscan con un «ancla»: los tres (o dos) octetos más raros
 * de cada uno, en una tabla dispersa con un mapa de bits delante; lo demás
 * se comprueba alrededor del ancla. Lo que se pierde frente a ClamAV, en
 * LEEME.txt y docs/PENTIUM.md («Escudo»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "escudo.h"

u32 motor_max_archivo = 16u << 20;
int motor_zip = 1;
int motor_guarda_lineas;		/* (para hacer FIRMAS.ESC) guardar las líneas del cuerpo */

#define INF 0xFFFFFFFFu
#define MAX_PROF 3

/* ------------------------------------------------------------------ memoria */
static void *crece(void *p, u32 *cap, u32 n, u32 tam)
{
	u32 c = *cap;
	void *q;
	if (n <= c)
		return p;
	if (n > 0x7FFFFFFFu / tam)		/* (que la cuenta no dé la vuelta) */
		return 0;
	if (!c)
		c = 64;
	while (c < n)
		c = c > 0x3FFFFFFFu ? n : c * 2;
	if (c > 0x7FFFFFFFu / tam)
		c = n;
	q = pl_rereserva(p, c * tam);
	if (!q)
		return 0;
	*cap = c;
	return q;
}
#define CRECE(arr, cap, n) ((tmp_ = crece((arr), &(cap), (n), sizeof *(arr))) ? ((arr) = tmp_, 1) : 0)
static void *tmp_;

static u8 fold[256];

/* ------------------------------------------------------------------ nombres */
static char *arena;
static u32 arena_n, arena_cap;
static u32 *nombres;			/* id -> sitio en «arena» */
static u32 n_nombres, cap_nombres;
static u32 *disp_nombres;		/* id + 1, por la suma del nombre */
static u32 cap_disp;

static u32 suma_texto(const char *s, u32 n)
{
	u32 h = 2166136261u, i;
	for (i = 0; i < n; i++)
		h = (h ^ (u8)s[i]) * 16777619u;
	return h;
}

static int redispersa(void)
{
	u32 c = cap_disp ? cap_disp * 2 : 4096, i;
	u32 *d = pl_reserva(c * 4);
	if (!d)
		return 0;
	memset(d, 0, c * 4);
	for (i = 0; i < n_nombres; i++) {
		const char *s = arena + nombres[i];
		u32 h = suma_texto(s, (u32)strlen(s)) & (c - 1);
		while (d[h])
			h = (h + 1) & (c - 1);
		d[h] = i + 1;
	}
	pl_libera(disp_nombres);
	disp_nombres = d;
	cap_disp = c;
	return 1;
}

/* lo de leer una firma sola (de un disparador, o para hacer los disparadores) */
#define ID_TEMPORAL 0xFF0000u		/* + n: el nombre n de los de un momento */
#define MAX_TEMPORALES 512
#define HALLADA_TEMPORAL 0xFFFFFFFEu
static int modo_temporal;
static char tnombres[MAX_TEMPORALES][MAX_NOMBRE_VIRUS], nombre_hallado[MAX_NOMBRE_VIRUS];
static u32 n_tnombres;
#define nombre_temporal (tnombres[0])

/* el número de un nombre (lo apunta si es nuevo); INF si no hay memoria */
static u32 nombre_id(const char *s, u32 n)
{
	u32 h, id;
	if (n >= MAX_NOMBRE_VIRUS)
		n = MAX_NOMBRE_VIRUS - 1;
	if (modo_temporal) {	/* (una firma leída un momento: su nombre, aparte) */
		if (n_tnombres >= MAX_TEMPORALES)
			return INF;
		memcpy(tnombres[n_tnombres], s, n);
		tnombres[n_tnombres][n] = 0;
		return ID_TEMPORAL + n_tnombres++;
	}
	if ((n_nombres + 1) * 2 > cap_disp && !redispersa())
		return INF;
	h = suma_texto(s, n) & (cap_disp - 1);
	while ((id = disp_nombres[h])) {
		const char *t = arena + nombres[id - 1];
		if (!strncmp(t, s, n) && !t[n])
			return id - 1;
		h = (h + 1) & (cap_disp - 1);
	}
	if (!CRECE(arena, arena_cap, arena_n + n + 1) || !CRECE(nombres, cap_nombres, n_nombres + 1))
		return INF;
	memcpy(arena + arena_n, s, n);
	arena[arena_n + n] = 0;
	nombres[n_nombres] = arena_n;
	arena_n += n + 1;
	disp_nombres[h] = n_nombres + 1;
	return n_nombres++;
}

/* --------------------------------------------------------------- las sumas */
#define S_MD5 1
#define S_SHA1 2
#define S_SHA256 3
#define S_SEC 3				/* + S_SEC: de una sección PE */
#define S_LIMPIA 0x10			/* .fp/.sfp: no es un virus */
#define S_MAIN 0x20			/* (solo al hacer FIRMAS.ESC) viene de main.cvd: lo primero que se deja si no cabe */
static u32 marca_origen;		/* S_MAIN mientras se lee algo de main */
typedef struct suma {
	u32 tam;			/* INF: cualquier tamaño */
	u32 nt;				/* (nombre << 8) | tipo */
	u8 h[8];			/* los 8 primeros octetos de la suma */
} suma;
static suma *sumas;
static u32 n_sumas, cap_sumas;
static u32 n_comodin_ram;		/* las de cualquier tamaño, al final (tras ordenar) */
static u32 tipos_comodin;		/* (bits 1 << tipo) */

/* los FIRMAS.ESC (versión 2): el índice propio, en el disco. Puede haber
 * varios: el de la base (BASE.ESC) y el del día (DIA.ESC), cada uno de su
 * paquete. De cada uno, en memoria, solo lo pequeño: el mapa de los tamaños,
 * la página de cada 256 tramos, la tabla de los bloques del cuerpo y sus
 * disparadores (que van todos juntos, en «disps»). El formato, en
 * motor_escribe_esc. */
#define TRAMOS_POR_PAGINA 256
#define MAX_ESC 4
#define BLOQUE 65536u		/* (lo que mide como mucho un bloque del cuerpo, descomprimido) */
#define BLOQUE_OBJETIVO 8192u	/* (lo que mide uno, de ordinario: cuanto menos, menos hay que descomprimir) */
#define BLOQUES_CACHE 4
typedef struct tramo { u32 tam, tipo, primero, cuantos; } tramo;
typedef struct esc {
	int h;
	u32 n_sumas, off_sumas, n_tramos, off_tramos, paginas, nbits, n_nombres;
	u32 idx, blob;
	u32 *pag;			/* el tamaño del primer tramo de cada página */
	u8 *bits;			/* 1 << 21 bits: ¿hay alguna suma con este tamaño? */
	tramo *comodin;			/* los tramos de cualquier tamaño (en memoria) */
	u32 n_comodin;
	u32 n_bloques;
	u32 *bloques;			/* dónde empieza cada bloque del cuerpo (n_bloques + 1) */
	tramo cache[TRAMOS_POR_PAGINA];
	u32 cache_pag;
} esc;
static esc escs[MAX_ESC];
static int n_escs;
static esc *E;				/* el que se está mirando */

/* los disparadores: 4 octetos fijos (y hasta 2 más) de una firma del cuerpo
 * que está en el disco; si aparecen, se lee la firma y se mira entera */
typedef struct disp { u32 clave, ref, sl; } disp;	/* sl: los 3 de detrás, cuántos valen (<< 24) y el .ESC (<< 28) */
#define D_SIGUE(d) ((d)->sl & 0xFFFFFF)
#define D_LARGO(d) (((d)->sl >> 24) & 3)
#define D_ESC(d) ((d)->sl >> 28)
#define D_SOLO_PE 0x04000000u		/* (en sl) la firma es solo de ejecutables PE */
static disp *disps;
static u32 n_disps, cap_disps;
static u8 *mapa4;			/* 1 << 23 bits */
static u32 *cubo4;			/* (1 << 16) + 1 */
static u32 h4(u32 k) { return (k ^ (k >> 7) ^ (k >> 17) ^ (k >> 27)) & 0x7FFFFF; }


static int compara_suma(const suma *a, const suma *b)
{
	if (a->tam != b->tam)
		return a->tam < b->tam ? -1 : 1;
	if ((a->nt & 0x1F) != (b->nt & 0x1F))
		return (a->nt & 0x1F) < (b->nt & 0x1F) ? -1 : 1;
	return memcmp(a->h, b->h, 8);
}

/* ordenar sin biblioteca (montículo: sin pila que crezca) */
static void ordena_sumas(suma *v, u32 n)
{
	u32 i, raiz, hijo, fin;
	suma t;
	if (n < 2)
		return;
	for (i = n / 2; i-- > 0;) {
		for (raiz = i; (hijo = 2 * raiz + 1) < n; raiz = hijo) {
			if (hijo + 1 < n && compara_suma(&v[hijo], &v[hijo + 1]) < 0)
				hijo++;
			if (compara_suma(&v[raiz], &v[hijo]) >= 0)
				break;
			t = v[raiz], v[raiz] = v[hijo], v[hijo] = t;
		}
	}
	for (fin = n - 1; fin > 0; fin--) {
		t = v[0], v[0] = v[fin], v[fin] = t;
		for (raiz = 0; (hijo = 2 * raiz + 1) < fin; raiz = hijo) {
			if (hijo + 1 < fin && compara_suma(&v[hijo], &v[hijo + 1]) < 0)
				hijo++;
			if (compara_suma(&v[raiz], &v[hijo]) >= 0)
				break;
			t = v[raiz], v[raiz] = v[hijo], v[hijo] = t;
		}
	}
}


/* ¿cabe [off, off + n * tam) en un archivo de «largo» octetos? (sin dar la vuelta) */
static int cabe_en(u32 off, u32 n, u32 tam, u32 largo)
{
	u64 fin = (u64)off + (u64)n * tam;
	return off <= largo && fin <= largo;
}

static u32 le32(const u8 *p) { return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static __attribute__((unused)) void pon32(u8 *p, u32 v) { p[0] = (u8)v, p[1] = (u8)(v >> 8), p[2] = (u8)(v >> 16), p[3] = (u8)(v >> 24); }

/* el bit del mapa de los tamaños (de «bits» bits: 21 en uno grande, 16 en uno pequeño) */
static u32 bit_tam2(u32 tam, u32 bits) { return (tam * 2654435761u) >> (32 - bits); }

/* leer de un FIRMAS.ESC (el de E) con una caché de páginas de 4 K: las
 * sumas y los tramos se leen a saltos, y los tamaños de siempre (512, 4096...)
 * se repiten en cada archivo */
#define PAG_ESC 4096u
#define PAGS_CACHE 32
typedef struct pag_c { int esc; u32 pag, usada; u8 d[PAG_ESC]; } pag_c;
static pag_c *pcache;
static u32 pcache_reloj;
static int lee_esc(u32 off, void *d, u32 n)
{
	u8 *o = d;
	int e = (int)(E - escs);
	while (n) {
		u32 pg = off / PAG_ESC, dentro = off % PAG_ESC, k = PAG_ESC - dentro, i, viejo = 0;
		pag_c *c = 0;
		if (k > n)
			k = n;
		if (!pcache) {
			pcache = pl_reserva(PAGS_CACHE * sizeof(pag_c));
			if (!pcache)
				return pl_mover(E->h, off) >= 0 && pl_leer(E->h, d, n) == (int)n;
			for (i = 0; i < PAGS_CACHE; i++)
				pcache[i].esc = -1, pcache[i].usada = 0;
		}
		for (i = 0; i < PAGS_CACHE; i++) {
			if (pcache[i].esc == e && pcache[i].pag == pg) {
				c = &pcache[i];
				break;
			}
			if (pcache[i].usada < pcache[viejo].usada)
				viejo = i;
		}
		if (!c) {
			int r;
			c = &pcache[viejo];
			c->esc = -1;
			if (pl_mover(E->h, pg * PAG_ESC) < 0)
				return 0;
			r = pl_leer(E->h, c->d, PAG_ESC);
			if (r < (int)(dentro + k))
				return 0;
			c->esc = e, c->pag = pg;
		}
		c->usada = ++pcache_reloj;
		memcpy(o, c->d + dentro, k);
		o += k, off += k, n -= k;
	}
	return 1;
}

/* la página «pg» de los tramos de E, a su caché */
static int esc_pagina(u32 pg)
{
	u32 n, i;
	u8 b[TRAMOS_POR_PAGINA * 16];
	if (pg == E->cache_pag)
		return 1;
	n = E->n_tramos - E->n_comodin - pg * TRAMOS_POR_PAGINA;
	if (n > TRAMOS_POR_PAGINA)
		n = TRAMOS_POR_PAGINA;
	if (!lee_esc(E->off_tramos + pg * TRAMOS_POR_PAGINA * 12, b, n * 12))
		return 0;
	for (i = 0; i < n; i++) {
		E->cache[i].tam = le32(b + 12 * i);
		E->cache[i].tipo = le32(b + 12 * i + 4) & 0xFF;
		E->cache[i].primero = le32(b + 12 * i + 4) >> 8;
		E->cache[i].cuantos = le32(b + 12 * i + 8);
	}
	for (; i < TRAMOS_POR_PAGINA; i++)
		E->cache[i].tam = INF, E->cache[i].cuantos = 0;
	E->cache_pag = pg;
	return 1;
}

/* los tramos de E de ese tamaño (y los de cualquiera), a «f» */
typedef int (*con_tramo)(void *yo, const tramo *t);
static int recorre_esc(u32 tam, con_tramo f, void *yo)
{
	u32 a, b, m, i, pg, np = E->paginas;
	for (i = 0; i < E->n_comodin; i++)
		if (f(yo, &E->comodin[i]))
			return 1;
	if (E->h < 0 || !np || !(E->bits[bit_tam2(tam, E->nbits) >> 3] >> (bit_tam2(tam, E->nbits) & 7) & 1))
		return 0;
	a = 0, b = np;
	while (a < b) {			/* la primera página que empieza en >= tam */
		m = (a + b) / 2;
		if (E->pag[m] < tam)
			a = m + 1;
		else
			b = m;
	}
	pg = a ? a - 1 : 0;
	for (; pg < np && E->pag[pg] <= tam; pg++) {
		if (!esc_pagina(pg))
			return 0;
		for (i = 0; i < TRAMOS_POR_PAGINA; i++) {
			if (E->cache[i].tam > tam)
				return 0;
			if (E->cache[i].tam == tam && f(yo, &E->cache[i]))
				return 1;
		}
	}
	return 0;
}

static int junta_tipos_t(void *yo, const tramo *t)
{
	*(u32 *)yo |= 1u << (t->tipo & 0x1F);
	return 0;
}

/* qué tipos de suma (bits 1 << tipo, con S_LIMPIA) hay para ese tamaño */
static u32 tipos_de(u32 tam)
{
	u32 t = 0, a = 0, b = n_sumas - n_comodin_ram, m, i;
	int k;
	while (a < b) {
		m = (a + b) / 2;
		if (sumas[m].tam < tam)
			a = m + 1;
		else
			b = m;
	}
	for (i = a; i < n_sumas - n_comodin_ram && sumas[i].tam == tam; i++)
		t |= 1u << (sumas[i].nt & 0x1F);
	for (k = 0; k < n_escs; k++) {
		E = &escs[k];
		recorre_esc(tam, junta_tipos_t, &t);
	}
	return t;
}

/* busca en un tramo (en el disco: por la mitad, 8 octetos cada vez) */
typedef struct busca_s { u32 tipo; const u8 *h; u32 nombre; } busca_s;
static int busca_en_tramo(void *yo, const tramo *t)
{
	busca_s *b = yo;
	u32 a = 0, z = t->cuantos;
	if ((t->tipo & 0x1F) != b->tipo || !cabe_en(t->primero, t->cuantos, 1, E->n_sumas))
		return 0;
	if (z <= 512) {		/* (un tramo pequeño, de una vez: una lectura en vez de nueve) */
		static u8 e[512 * 8];
		u32 i;
		if (!lee_esc(E->off_sumas + t->primero * 8, e, z * 8))
			return 0;
		for (i = 0; i < z; i++)
			if (!memcmp(e + 8 * i, b->h, 5)) {
				b->nombre = e[8 * i + 5] | (u32)e[8 * i + 6] << 8 | (u32)e[8 * i + 7] << 16;
				return 1;
			}
		return 0;
	}
	while (a < z) {
		u32 m = (a + z) / 2;
		u8 e[8];
		int c;
		if (!lee_esc(E->off_sumas + (t->primero + m) * 8, e, 8))
			return 0;
		c = memcmp(e, b->h, 5);
		if (!c) {
			b->nombre = e[5] | (u32)e[6] << 8 | (u32)e[7] << 16;
			return 1;
		}
		if (c < 0)
			a = m + 1;
		else
			z = m;
	}
	return 0;
}

/* busca la suma (de ese tamaño o de cualquiera): su nombre, o INF; de_esc:
 * 0 si es de memoria, k + 1 si es del FIRMAS.ESC k */
static u32 busca_suma(u32 tam, u32 tipo, const u8 *h, int *de_esc)
{
	u32 a = 0, b = n_sumas - n_comodin_ram, m, i;
	int k;
	busca_s s;
	*de_esc = 0;
	while (a < b) {
		m = (a + b) / 2;
		if (sumas[m].tam < tam)
			a = m + 1;
		else
			b = m;
	}
	for (i = a; i < n_sumas - n_comodin_ram && sumas[i].tam == tam; i++)
		if ((sumas[i].nt & 0x1F) == tipo && !memcmp(sumas[i].h, h, 8))
			return sumas[i].nt >> 8;
	for (i = n_sumas - n_comodin_ram; i < n_sumas; i++)
		if ((sumas[i].nt & 0x1F) == tipo && !memcmp(sumas[i].h, h, 8))
			return sumas[i].nt >> 8;
	s.tipo = tipo, s.h = h, s.nombre = INF;
	for (k = 0; k < n_escs; k++) {
		E = &escs[k];
		if (recorre_esc(tam, busca_en_tramo, &s)) {
			*de_esc = k + 1;
			return s.nombre;
		}
	}
	return INF;
}

/* -------------------------------------------------------- los patrones */
#define T_BYTE 0			/* (b & m) == v */
#define T_BYTEI 1			/* fold(b) == v */
#define T_NO 2				/* (b & m) != v */
#define T_CONJ 3			/* b en el conjunto x (v = 1: que no esté) */
#define T_ALT 4				/* alternativas de x.largo octetos (empieza aquí) */
#define T_ALTC 5			/* (la continuación de una T_ALT) */
#define T_CLASE 6			/* v: 'B' borde de palabra, 'L' fin de línea, 'W' no alfanumérico */
typedef struct tok { u8 k, v, m, x; } tok;
typedef struct seg { u32 ini; u16 largo; u16 nada; u32 gmin, gmax; } seg;	/* gmin/gmax: el hueco DETRÁS */
#define O_CUALQUIERA 0
#define O_ABS 1
#define O_EOF 2
#define O_EP 3
#define O_SX 4
#define O_SEX 5
#define O_SL 6
typedef struct patron {
	u32 firma;
	u32 seg0;
	u16 sub;			/* la subfirma (ldb) */
	u8 nseg, ancla_seg;
	u16 ancla_pos;
	u8 ancla_largo, palabra;	/* palabra: ::f */
	u8 o_tipo, o_sec;
	i32 o_n;
	u32 o_m;
	u8 ancho;			/* ::w (para ::f) */
	u8 nada[3];
} patron;
#define F_NDB 1
#define F_LDB 2
typedef struct firma {
	u32 nombre;
	u8 clase, objetivo, contenedor, nada;
	u16 nsub, nada2;
	u32 expr, cuentas;		/* dónde empieza su expresión y sus cuentas */
	u32 tmin, tmax, epmin, epmax;
	u16 smin, smax;
} firma;
static tok *toks;
static u32 n_toks, cap_toks;
static seg *segs;
static u32 n_segs, cap_segs;
static patron *pats;
static u32 n_pats, cap_pats;
static firma *firmas;
static u32 n_firmas, cap_firmas;
static u8 *conjuntos;			/* 32 octetos cada uno */
static u32 n_conj, cap_conj;
typedef struct alt { u32 datos; u16 largo, n; } alt;
static alt *alts;
static u32 n_alts, cap_alts;
static u8 *altdatos;
static u32 n_altdatos, cap_altdatos;
static u32 *exprs;			/* la expresión, al revés (polaca) */
static u32 n_exprs, cap_exprs;
static u32 n_cuentas;			/* las subfirmas de todas las ldb */
static u8 *cuentas;			/* (por archivo) */
static u32 n_cuentas_cabe;
static u32 *tocadas;
static u32 n_tocadas, cap_tocadas;
static u32 *ignorar;			/* los nombres (.ign2) que no se dicen */
static u32 n_ignorar, cap_ignorar;
/* las líneas del cuerpo (para FIRMAS.ESC) */
static char *lineas;
static u32 n_lineas, cap_lineas;

/* el índice de las anclas */
#define BITS3 20
typedef struct ancla { u32 clave; u32 pat; } ancla;
static u8 *mapa3, *mapa2;		/* 1 << 20 y 1 << 16 bits */
static u32 *cubo3, *cubo2;		/* dónde empieza cada cubo (1 << 16 + 1 y 1 << 16 + 1) */
static ancla *anclas3, *anclas2;
static u32 n_a3, n_a2;
static u32 *fijos;			/* los patrones con desplazamiento fijo */
static u32 n_fijos;

/* (sin multiplicar: en un Pentium, «imul» son 10 ciclos y esto va por cada octeto) */
static u32 h3(u32 k) { return (k ^ (k >> 4) ^ (k >> 12)) & ((1u << BITS3) - 1); }

/* ---- leer un patrón en hexadecimal ---- */
typedef struct hexp {
	int ntok;
	int nseg;
	u32 seg_ini[64];
	u32 gmin[64], gmax[64];
	int error;
} hexp;

static int pon_tok(u8 k, u8 v, u8 m, u8 x)
{
	if (!CRECE(toks, cap_toks, n_toks + 1))
		return 0;
	toks[n_toks].k = k, toks[n_toks].v = v, toks[n_toks].m = m, toks[n_toks].x = x;
	n_toks++;
	return 1;
}

static int num_hasta(const char **p, u32 *v)
{
	u32 n = 0;
	int hay = 0;
	while (**p >= '0' && **p <= '9')
		n = n * 10 + (u32)(*(*p)++ - '0'), hay = 1;
	*v = n;
	return hay;
}

/* el par de cifras (con ?): da v y m */
static int par(const char *p, u8 *v, u8 *m)
{
	int a = p[0], b = p[1];
	if (!a || !b)
		return 0;
	if (a == '?' && b == '?')
		*v = 0, *m = 0;
	else if (a == '?' && es_hex(b))
		*v = (u8)val_hex(b), *m = 0x0F;
	else if (es_hex(a) && b == '?')
		*v = (u8)(val_hex(a) << 4), *m = 0xF0;
	else if (es_hex(a) && es_hex(b))
		*v = (u8)(val_hex(a) << 4 | val_hex(b)), *m = 0xFF;
	else
		return 0;
	return 1;
}

#define HEX_MAL -1
#define HEX_VARIANTES -2		/* lleva una alternativa de largo variable: hay que desdoblarlo */

/* lee el patrón «s» (hasta «fin»): tokens y segmentos nuevos (en toks/segs) */
static int lee_hex(const char *s, const char *fin, int insens, int ancho, hexp *h)
{
	const char *p = s;
	u32 desde = n_toks;
	h->nseg = 0;
	h->seg_ini[0] = n_toks;
	h->gmin[0] = h->gmax[0] = 0;
	while (p < fin) {
		u8 v, m;
		if (*p == '*' || *p == '{' || *p == '[') {
			u32 a = 0, b = INF;
			if (*p == '*')
				p++;
			else {
				char cierra = *p == '{' ? '}' : ']';
				p++;
				if (*p == '-') {
					p++;
					if (!num_hasta(&p, &b))
						return HEX_MAL;
				} else {
					if (!num_hasta(&p, &a))
						return HEX_MAL;
					if (*p == '-') {
						p++;
						if (!num_hasta(&p, &b))
							b = INF;
					} else
						b = a;
				}
				if (*p != cierra)
					return HEX_MAL;
				p++;
				if (cierra == '}' && a == b && a < 128) {	/* {n} pequeño: n comodines */
					while (a--) {
						if (!pon_tok(T_BYTE, 0, 0, 0))
							return HEX_MAL;
						if (ancho && !pon_tok(T_BYTE, 0, 0, 0))
							return HEX_MAL;
					}
					continue;
				}
			}
			if (ancho) {
				a *= 2;
				if (b != INF)
					b *= 2;
			}
			if (n_toks == h->seg_ini[h->nseg])	/* hueco al principio: no vale */
				return HEX_MAL;
			if (h->nseg >= 62)
				return HEX_MAL;
			h->gmin[h->nseg] = a, h->gmax[h->nseg] = b;
			h->nseg++;
			h->seg_ini[h->nseg] = n_toks;
			h->gmin[h->nseg] = h->gmax[h->nseg] = 0;
			continue;
		}
		if (*p == '~') {
			if (!par(p + 1, &v, &m) || m == 0)
				return HEX_MAL;
			if (!pon_tok(T_NO, v, m, 0) || (ancho && !pon_tok(T_BYTE, 0, 0xFF, 0)))
				return HEX_MAL;
			p += 3;
			continue;
		}
		if (*p == '(' || (*p == '!' && p[1] == '(')) {
			int neg = *p == '!';
			const char *q, *c;
			int largo = -1, n = 0, fijo = 1, todos_par = 1;
			if (neg)
				p++;
			if (p + 2 < fin && p[2] == ')' && (p[1] == 'B' || p[1] == 'L' || p[1] == 'W') && !neg) {
				if (!pon_tok(T_CLASE, (u8)p[1], 0, 0))
					return HEX_MAL;
				p += 3;
				continue;
			}
			c = p + 1;
			for (q = c;; q++) {
				if (q >= fin)
					return HEX_MAL;
				if (*q == '|' || *q == ')') {
					int l = (int)(q - c);
					if (l <= 0 || (l & 1))
						fijo = 0;
					else {
						const char *r;
						for (r = c; r < q; r++)
							if (!es_hex(*r))
								todos_par = 0;
					}
					if (largo < 0)
						largo = l;
					else if (l != largo)
						fijo = 0;
					n++;
					c = q + 1;
					if (*q == ')')
						break;
				} else if (*q == '(' || *q == '*' || *q == '{' || *q == '[')
					fijo = 0;
			}
			if (!fijo || !todos_par) {
				if (neg)
					return HEX_MAL;
				return HEX_VARIANTES;
			}
			if (largo == 2) {	/* de un octeto: un conjunto */
				u8 *cj;
				if (!CRECE(conjuntos, cap_conj, (n_conj + 1) * 32))
					return HEX_MAL;
				cj = conjuntos + n_conj * 32;
				memset(cj, 0, 32);
				for (c = p + 1; c < q; c += 3) {
					u8 b = (u8)(val_hex(c[0]) << 4 | val_hex(c[1]));
					if (insens && b >= 'A' && b <= 'Z')
						cj[(b | 32) >> 3] |= (u8)(1u << ((b | 32) & 7));
					cj[b >> 3] |= (u8)(1u << (b & 7));
					if (insens && b >= 'a' && b <= 'z')
						cj[(b & ~32) >> 3] |= (u8)(1u << ((b & ~32) & 7));
				}
				if (n_conj > 0xFFFF)
					return HEX_MAL;
				if (!pon_tok(T_CONJ, (u8)neg, (u8)(n_conj & 0xFF), (u8)(n_conj >> 8)))
					return HEX_MAL;
				if (ancho && !pon_tok(T_BYTE, 0, 0xFF, 0))
					return HEX_MAL;
				n_conj++;
			} else {
				u32 L = (u32)largo / 2, k, j;
				if (neg || ancho || n_alts > 0xFFFF || L > 255)
					return neg ? HEX_MAL : HEX_VARIANTES;
				if (!CRECE(alts, cap_alts, n_alts + 1) || !CRECE(altdatos, cap_altdatos, n_altdatos + L * (u32)n))
					return HEX_MAL;
				alts[n_alts].datos = n_altdatos, alts[n_alts].largo = (u16)L, alts[n_alts].n = (u16)n;
				for (c = p + 1, k = 0; k < (u32)n; k++, c++)
					for (j = 0; j < L; j++, c += 2)
						altdatos[n_altdatos++] = (u8)(val_hex(c[0]) << 4 | val_hex(c[1]));
				if (!pon_tok(T_ALT, 0, (u8)(n_alts & 0xFF), (u8)(n_alts >> 8)))
					return HEX_MAL;
				for (j = 1; j < L; j++)
					if (!pon_tok(T_ALTC, 0, 0, 0))
						return HEX_MAL;
				n_alts++;
			}
			p = q + 1;
			continue;
		}
		if (!par(p, &v, &m))
			return HEX_MAL;
		p += 2;
		if (insens && m == 0xFF && ((v >= 'A' && v <= 'Z') || (v >= 'a' && v <= 'z'))) {
			if (!pon_tok(T_BYTEI, fold[v], 0xFF, 0))
				return HEX_MAL;
		} else if (!pon_tok(T_BYTE, v, m, 0))
			return HEX_MAL;
		if (ancho && !pon_tok(T_BYTE, 0, 0xFF, 0))
			return HEX_MAL;
	}
	if (n_toks == h->seg_ini[h->nseg])	/* hueco al final */
		return HEX_MAL;
	h->nseg++;
	h->ntok = (int)(n_toks - desde);
	return 0;
}

/* lo que vale un octeto como ancla (los muy comunes, poco) */
static int rareza(u8 b)
{
	if (b == 0 || b == 0xFF)
		return 1;
	if (b == 0x20 || b == 0x90 || b == 0xCC || b == 0x0A || b == 0x0D)
		return 2;
	if ((b >= 'a' && b <= 'z') || (b >= '0' && b <= '9'))
		return 3;
	return 4;
}

static int es_fijo(const tok *t) { return (t->k == T_BYTE && t->m == 0xFF) || t->k == T_BYTEI; }

/* ---- leer un desplazamiento: *, n, n,m, EOF-n, EP+n, EP-n, Sx+n, SEx, SL+n ---- */
static int lee_desplazamiento(const char *s, const char *fin, patron *p)
{
	u32 v;
	const char *q = s;
	p->o_tipo = O_CUALQUIERA, p->o_n = 0, p->o_m = 0, p->o_sec = 0;
	if (fin - s == 1 && *s == '*')
		return 0;
	if (!strncmp(q, "EOF-", 4)) {
		q += 4;
		if (!num_hasta(&q, &v))
			return -1;
		p->o_tipo = O_EOF, p->o_n = -(i32)v;
	} else if (!strncmp(q, "EP+", 3) || !strncmp(q, "EP-", 3)) {
		int menos = q[2] == '-';
		q += 3;
		if (!num_hasta(&q, &v))
			return -1;
		p->o_tipo = O_EP, p->o_n = menos ? -(i32)v : (i32)v;
	} else if (!strncmp(q, "SL+", 3)) {
		q += 3;
		if (!num_hasta(&q, &v))
			return -1;
		p->o_tipo = O_SL, p->o_n = (i32)v;
	} else if (!strncmp(q, "SE", 2)) {
		q += 2;
		if (!num_hasta(&q, &v) || v > 255)
			return -1;
		p->o_tipo = O_SEX, p->o_sec = (u8)v;
	} else if (*q == 'S') {
		u32 x;
		q++;
		if (!num_hasta(&q, &x) || x > 255 || *q != '+')
			return -1;
		q++;
		if (!num_hasta(&q, &v))
			return -1;
		p->o_tipo = O_SX, p->o_sec = (u8)x, p->o_n = (i32)v;
	} else if (*q >= '0' && *q <= '9') {
		num_hasta(&q, &v);
		p->o_tipo = O_ABS, p->o_n = (i32)v;
	} else
		return -1;
	if (q < fin && *q == ',') {
		q++;
		if (!num_hasta(&q, &v))
			return -1;
		p->o_m = v;
	}
	return q == fin ? 0 : -1;
}

/* hace los patrones de una (sub)firma: «hex» con sus variantes desdobladas */
static int patrones_de(u32 fi, u16 sub, const char *hex, u32 nhex, const patron *plantilla, int insens, int ancho, int nivel);

static int un_patron(u32 fi, u16 sub, const char *hex, u32 nhex, const patron *plantilla, int insens, int ancho)
{
	hexp h;
	u32 t0 = n_toks, s0 = n_segs, k, mejor = 0;
	int r, i;
	patron p = *plantilla;

	r = lee_hex(hex, hex + nhex, insens, ancho, &h);
	if (r) {
		n_toks = t0;
		return r;
	}
	if (!CRECE(segs, cap_segs, n_segs + (u32)h.nseg) || !CRECE(pats, cap_pats, n_pats + 1)) {
		n_toks = t0;
		return HEX_MAL;
	}
	for (i = 0; i < h.nseg; i++) {
		segs[n_segs + i].ini = h.seg_ini[i];
		segs[n_segs + i].largo = (u16)((i + 1 < h.nseg ? h.seg_ini[i + 1] : n_toks) - h.seg_ini[i]);
		segs[n_segs + i].gmin = h.gmin[i];
		segs[n_segs + i].gmax = h.gmax[i];
		if (segs[n_segs + i].largo == 0) {
			n_toks = t0;
			return HEX_MAL;
		}
	}
	p.firma = fi, p.sub = sub, p.seg0 = n_segs, p.nseg = (u8)h.nseg, p.ancho = (u8)ancho;
	/* el ancla: la ventana de 3 (o 2) octetos fijos más rara */
	p.ancla_largo = 0;
	for (i = 0; i < h.nseg; i++) {
		const seg *sg = &segs[n_segs + i];
		for (k = 0; k + 1 < sg->largo; k++) {
			const tok *t = &toks[sg->ini + k];
			u32 l, puntos;
			if (!es_fijo(t) || !es_fijo(t + 1))
				continue;
			l = (k + 2 < sg->largo && es_fijo(t + 2)) ? 3 : 2;
			puntos = (u32)rareza(t[0].v) + (u32)rareza(t[1].v) + (l == 3 ? (u32)rareza(t[2].v) + 100 : 0);
			if (puntos > mejor) {
				mejor = puntos;
				p.ancla_seg = (u8)i, p.ancla_pos = (u16)k, p.ancla_largo = (u8)l;
			}
		}
	}
	if (!p.ancla_largo && p.o_tipo == O_CUALQUIERA) {
		n_toks = t0;
		n_segs = s0;
		return HEX_MAL;	/* (ClamAV pide dos octetos fijos seguidos) */
	}
	n_segs += (u32)h.nseg;
	pats[n_pats++] = p;
	return 0;
}

static u32 variantes;			/* (las de una subfirma: con tope, que 84 K no pidan 1 GB) */
#define MAX_VARIANTES 64
static int patrones_de(u32 fi, u16 sub, const char *hex, u32 nhex, const patron *plantilla, int insens, int ancho, int nivel)
{
	int r;
	const char *a, *c, *q;
	char *nuevo;
	if (nivel == 0)
		variantes = 0;
	if (++variantes > MAX_VARIANTES || nhex > 65536)
		return HEX_MAL;
	r = un_patron(fi, sub, hex, nhex, plantilla, insens, ancho);
	if (r != HEX_VARIANTES)
		return r;
	if (nivel >= 4)
		return HEX_MAL;
	/* la primera alternativa de largo variable: una copia por cada una */
	for (a = hex; a < hex + nhex; a++) {
		if (*a == '(' && !(a + 2 < hex + nhex && a[2] == ')' && (a[1] == 'B' || a[1] == 'L' || a[1] == 'W'))) {
			int prof = 0, malo = 0;
			for (q = a + 1; q < hex + nhex && *q != ')'; q++)
				if (*q == '(')
					malo = 1;
			(void)prof;
			if (malo || q >= hex + nhex)
				return HEX_MAL;
			nuevo = pl_reserva(nhex + 1);
			if (!nuevo)
				return HEX_MAL;
			c = a + 1;
			for (;;) {
				const char *e = c;
				u32 n0 = (u32)(a - hex), nl, nr = (u32)(hex + nhex - (q + 1));
				while (*e != '|' && *e != ')')
					e++;
				nl = (u32)(e - c);
				memcpy(nuevo, hex, n0);
				memcpy(nuevo + n0, c, nl);
				memcpy(nuevo + n0 + nl, q + 1, nr);
				r = patrones_de(fi, sub, nuevo, n0 + nl + nr, plantilla, insens, ancho, nivel + 1);
				if (r) {
					pl_libera(nuevo);
					return r;
				}
				if (*e == ')')
					break;
				c = e + 1;
			}
			pl_libera(nuevo);
			return 0;
		}
	}
	return HEX_MAL;
}

/* -------------------------------------------------- leer las líneas */
static info_firmas *inf_actual;

static int nuevo_firma(void)
{
	if (!CRECE(firmas, cap_firmas, n_firmas + 1))
		return 0;
	memset(&firmas[n_firmas], 0, sizeof firmas[0]);
	firmas[n_firmas].tmax = INF, firmas[n_firmas].epmax = INF, firmas[n_firmas].smax = 0xFFFF;
	return 1;
}

static int objetivo_vale(u32 t) { return t == 0 || t == 1 || t == 6 || t == 9 || t == 10 || t == 12; }

/* parte «s» por «sep»: hasta «max» trozos (con su largo) */
static int parte(const char *s, u32 n, char sep, const char **tr, u32 *lr, int max)
{
	int k = 0;
	const char *p = s, *fin = s + n;
	while (k < max) {
		const char *q = p;
		while (q < fin && *q != sep)
			q++;
		tr[k] = p, lr[k] = (u32)(q - p);
		k++;
		if (q >= fin)
			break;
		p = q + 1;
	}
	return k;
}

static u32 num_n(const char *s, u32 n, int *bien)
{
	u32 v = 0, i;
	*bien = n > 0;
	for (i = 0; i < n; i++) {
		if (s[i] < '0' || s[i] > '9') {
			*bien = 0;
			return 0;
		}
		v = v * 10 + (u32)(s[i] - '0');
	}
	return v;
}

/* ¿vale el nivel de funciones de [min[:max]]? */
static int nivel_vale(const char **tr, const u32 *lr, int k, int desde)
{
	int bien;
	u32 v;
	if (k > desde && lr[desde]) {
		v = num_n(tr[desde], lr[desde], &bien);
		if (bien && v > ESCUDO_FLEVEL)
			return 0;
	}
	if (k > desde + 1 && lr[desde + 1]) {
		v = num_n(tr[desde + 1], lr[desde + 1], &bien);
		if (bien && v < ESCUDO_FLEVEL)
			return 0;
	}
	return 1;
}

static int guarda_linea(char clase, const char *s, u32 n)
{
	if (!motor_guarda_lineas || modo_temporal)
		return 1;
	if (!CRECE(lineas, cap_lineas, n_lineas + n + 3))
		return 0;
	lineas[n_lineas++] = marca_origen ? (char)(clase & ~32) : clase;	/* (en mayúscula, las de main) */
	lineas[n_lineas++] = '|';
	memcpy(lineas + n_lineas, s, n);
	n_lineas += n;
	lineas[n_lineas++] = '\n';
	return 1;
}

/* una suma: «hash:tamaño:nombre» (al revés en las de sección) */
static int linea_suma(const char *s, u32 n, int seccion, int limpia)
{
	const char *tr[6];
	u32 lr[6], tam, nh, i, id;
	int k = parte(s, n, ':', tr, lr, 6), ih = seccion ? 1 : 0, it = seccion ? 0 : 1, bien;
	u32 tipo;
	suma x;
	if (k < 3)
		return -1;
	if (!nivel_vale(tr, lr, k, 3))
		return 1;
	nh = lr[ih];
	tipo = nh == 32 ? S_MD5 : nh == 40 ? S_SHA1 : nh == 64 ? S_SHA256 : 0;
	if (!tipo)
		return -1;
	for (i = 0; i < nh; i++)
		if (!es_hex(tr[ih][i]))
			return -1;
	if (lr[it] == 1 && tr[it][0] == '*')
		tam = INF;
	else {
		tam = num_n(tr[it], lr[it], &bien);
		if (!bien)
			return -1;
	}
	if (seccion)
		tipo += S_SEC;
	if (limpia)
		tipo |= S_LIMPIA;
	tipo |= marca_origen;
	id = nombre_id(tr[2], lr[2]);
	if (id == INF || id > 0xFFFFFF || !CRECE(sumas, cap_sumas, n_sumas + 1))
		return -2;
	x.tam = tam;
	x.nt = id << 8 | tipo;
	for (i = 0; i < 8; i++)
		x.h[i] = (u8)(val_hex(tr[ih][2 * i]) << 4 | val_hex(tr[ih][2 * i + 1]));
	sumas[n_sumas++] = x;
	if (limpia)
		inf_actual->limpias++;
	else
		inf_actual->sumas++;
	return 0;
}

/* .ndb: nombre:objetivo:desplazamiento:hex[:min[:max]] */
static int linea_ndb(const char *s, u32 n)
{
	const char *tr[7];
	u32 lr[7], obj, f0 = n_firmas, p0 = n_pats, t0 = n_toks, s0 = n_segs;
	int k = parte(s, n, ':', tr, lr, 7), bien, r;
	patron pl;
	if (k < 4)
		return -1;
	if (!nivel_vale(tr, lr, k, 4))
		return 1;
	obj = num_n(tr[1], lr[1], &bien);
	if (!bien || !objetivo_vale(obj))
		return -1;
	memset(&pl, 0, sizeof pl);
	if (lee_desplazamiento(tr[2], tr[2] + lr[2], &pl))
		return -1;
	if (!nuevo_firma())
		return -2;
	firmas[f0].nombre = nombre_id(tr[0], lr[0]);
	if (firmas[f0].nombre == INF)
		return -2;
	firmas[f0].clase = F_NDB, firmas[f0].objetivo = (u8)obj, firmas[f0].nsub = 1;
	n_firmas++;
	r = patrones_de(f0, 0, tr[3], lr[3], &pl, 0, 0, 0);
	if (r) {
		n_firmas = f0, n_pats = p0, n_toks = t0, n_segs = s0;
		return -1;
	}
	inf_actual->cuerpo++;
	return guarda_linea('n', s, n) ? 0 : -2;
}

/* la expresión de una ldb, a polaca: SUB i | AND | OR | CMP (op, n, m) */
#define X_SUB 0x01000000u
#define X_AND 0x02000000u
#define X_OR 0x03000000u
#define X_IGUAL 0x04000000u
#define X_MAYOR 0x05000000u
#define X_MENOR 0x06000000u
typedef struct lex { const char *p, *fin; int error; u32 nsub, prof; } lex;	/* prof: los paréntesis abiertos */
static int pon_x(u32 x) { if (!CRECE(exprs, cap_exprs, n_exprs + 1)) return 0; exprs[n_exprs++] = x; return 1; }
static void x_o(lex *l);
static void x_cuenta(lex *l)
{
	u32 v, m = 0;
	char op;
	if (l->p >= l->fin || (*l->p != '=' && *l->p != '>' && *l->p != '<'))
		return;
	op = *l->p++;
	if (!num_hasta(&l->p, &v)) {
		l->error = 1;
		return;
	}
	if (l->p < l->fin && *l->p == ',') {
		l->p++;
		if (!num_hasta(&l->p, &m)) {
			l->error = 1;
			return;
		}
	}
	if (v > 0xFFF || m > 0xFFF) {
		l->error = 1;
		return;
	}
	pon_x((op == '=' ? X_IGUAL : op == '>' ? X_MAYOR : X_MENOR) | v << 12 | m);
}
static void x_primario(lex *l)
{
	u32 v;
	if (l->p < l->fin && *l->p == '(') {
		if (++l->prof > 32) {	/* (paréntesis sin fin: la pila no se acaba) */
			l->error = 1;
			return;
		}
		l->p++;
		x_o(l);
		l->prof--;
		if (l->p >= l->fin || *l->p != ')') {
			l->error = 1;
			return;
		}
		l->p++;
	} else if (num_hasta(&l->p, &v)) {
		if (v >= l->nsub || v > 63) {
			l->error = 1;
			return;
		}
		pon_x(X_SUB | v);
	} else {
		l->error = 1;
		return;
	}
	x_cuenta(l);
}
static void x_y(lex *l)
{
	x_primario(l);
	while (!l->error && l->p < l->fin && *l->p == '&') {
		l->p++;
		x_primario(l);
		pon_x(X_AND);
	}
}
static void x_o(lex *l)
{
	x_y(l);
	while (!l->error && l->p < l->fin && *l->p == '|') {
		l->p++;
		x_y(l);
		pon_x(X_OR);
	}
}

static int rango(const char *s, u32 n, u32 *a, u32 *b)
{
	const char *q = s, *fin = s + n;
	if (!num_hasta(&q, a))
		return 0;
	*b = *a;
	if (q < fin && *q == '-') {
		q++;
		if (!num_hasta(&q, b))
			return 0;
	}
	return q == fin;
}

/* .ldb: nombre;cabecera;expresión;subfirma0;subfirma1... */
static int linea_ldb(const char *s, u32 n)
{
	const char *tr[66], *cb[16];
	u32 lr[66], lc[16], f0 = n_firmas, p0 = n_pats, t0 = n_toks, s0 = n_segs, x0 = n_exprs;
	int k = parte(s, n, ';', tr, lr, 66), i, nc;
	firma *f;
	lex l;
	if (k < 4)
		return -1;
	if (k == 66)
		return -1;
	if (!nuevo_firma())
		return -2;
	f = &firmas[f0];
	f->clase = F_LDB, f->objetivo = 0;
	nc = parte(tr[1], lr[1], ',', cb, lc, 16);
	for (i = 0; i < nc; i++) {
		const char *c = cb[i];
		u32 a, b;
		if (!strncmp(c, "Target:", 7)) {
			if (!rango(c + 7, lc[i] - 7, &a, &b) || a != b || !objetivo_vale(a))
				return -1;
			f->objetivo = (u8)a;
		} else if (!strncmp(c, "Engine:", 7)) {
			if (!rango(c + 7, lc[i] - 7, &a, &b))
				return -1;
			if (a > ESCUDO_FLEVEL || b < ESCUDO_FLEVEL)
				return 1;
		} else if (!strncmp(c, "FileSize:", 9)) {
			if (!rango(c + 9, lc[i] - 9, &a, &b))
				return -1;
			f->tmin = a, f->tmax = b;
		} else if (!strncmp(c, "EntryPoint:", 11)) {
			if (!rango(c + 11, lc[i] - 11, &a, &b))
				return -1;
			f->epmin = a, f->epmax = b;
		} else if (!strncmp(c, "NumberOfSections:", 17)) {
			if (!rango(c + 17, lc[i] - 17, &a, &b))
				return -1;
			f->smin = (u16)(a > 0xFFFF ? 0xFFFF : a), f->smax = (u16)(b > 0xFFFF ? 0xFFFF : b);
		} else if (!strncmp(c, "Container:", 10)) {
			if (lc[i] == 21 && !strncmp(c + 10, "CL_TYPE_ZIP", 11))
				f->contenedor = 1;
			else
				return -1;
		} else
			return -1;	/* (IconGroup, Intermediates, HandlerType...: no se llevan) */
	}
	f->nombre = nombre_id(tr[0], lr[0]);
	if (f->nombre == INF)
		return -2;
	f->nsub = (u16)(k - 3);
	f->expr = n_exprs;
	f->cuentas = n_cuentas;
	l.p = tr[2], l.fin = tr[2] + lr[2], l.error = 0, l.nsub = (u32)(k - 3), l.prof = 0;
	x_o(&l);
	if (l.error || l.p != l.fin || !pon_x(0)) {
		n_exprs = x0;
		return -1;
	}
	n_firmas++;
	for (i = 3; i < k; i++) {
		const char *sub = tr[i], *mod, *dos;
		u32 ns = lr[i], j;
		int insens = 0, ancho = 0, ascii = 0, palabra = 0, r;
		patron pl;
		memset(&pl, 0, sizeof pl);
		/* los modificadores: «hex::iwaf» */
		mod = 0;
		for (j = 0; j + 1 < ns; j++)
			if (sub[j] == ':' && sub[j + 1] == ':') {
				mod = sub + j + 2;
				break;
			}
		if (mod) {
			const char *m;
			for (m = mod; m < sub + ns; m++) {
				if (*m == 'i')
					insens = 1;
				else if (*m == 'w')
					ancho = 1;
				else if (*m == 'a')
					ascii = 1;
				else if (*m == 'f')
					palabra = 1;
				else
					goto mal;
			}
			ns = (u32)(mod - 2 - sub);
		}
		/* PCRE (con «/»), comparar octetos («#»), macros («$»), iconos...: no */
		for (j = 0; j < ns; j++)
			if (sub[j] == '/' || sub[j] == '#' || sub[j] == '$' || sub[j] == '=')
				goto mal;
		/* «desplazamiento:hex» */
		dos = 0;
		for (j = 0; j < ns; j++)
			if (sub[j] == ':') {
				dos = sub + j;
				break;
			}
		if (dos) {
			if (lee_desplazamiento(sub, dos, &pl))
				goto mal;
			ns -= (u32)(dos + 1 - sub);
			sub = dos + 1;
		}
		pl.palabra = (u8)palabra;
		if (!ancho || ascii) {
			r = patrones_de(f0, (u16)(i - 3), sub, ns, &pl, insens, 0, 0);
			if (r)
				goto mal;
		}
		if (ancho) {
			r = patrones_de(f0, (u16)(i - 3), sub, ns, &pl, insens, 1, 0);
			if (r)
				goto mal;
		}
	}
	n_cuentas += (u32)(k - 3);
	inf_actual->logicas++;
	return guarda_linea('l', s, n) ? 0 : -2;
mal:
	n_firmas = f0, n_pats = p0, n_toks = t0, n_segs = s0, n_exprs = x0;
	return -1;
}

/* .ign2: el nombre de una firma que no se dice; .ign: «base:línea:nombre» */
static int linea_ign(const char *s, u32 n, int dos)
{
	const char *tr[4];
	u32 lr[4], id;
	int k;
	if (!dos) {
		k = parte(s, n, ':', tr, lr, 4);
		if (k < 3)
			return -1;
		s = tr[2], n = lr[2];
	}
	id = nombre_id(s, n);
	if (id == INF || !CRECE(ignorar, cap_ignorar, n_ignorar + 1))
		return -2;
	ignorar[n_ignorar++] = id;
	return guarda_linea('i', s, n) ? 0 : -2;
}

/* ---- lo que se lee de un archivo de firmas: por la extensión ---- */
#define B_HDB 1
#define B_HSB 2
#define B_MDB 3
#define B_MSB 4
#define B_FP 5
#define B_SFP 6
#define B_NDB 7
#define B_LDB 8
#define B_IGN 9
#define B_IGN2 10
#define B_PUA 11
#define B_NADA 12
static int clase_de(const char *nombre)
{
	const char *e = strrchr(nombre, '.');
	char x[8];
	int i;
	if (!e)
		return B_NADA;
	for (i = 0; i < 7 && e[i + 1]; i++)
		x[i] = (char)(e[i + 1] | 32);
	x[i] = 0;
	if (!strcmp(x, "hdb"))
		return B_HDB;
	if (!strcmp(x, "hsb"))
		return B_HSB;
	if (!strcmp(x, "mdb"))
		return B_MDB;
	if (!strcmp(x, "msb"))
		return B_MSB;
	if (!strcmp(x, "fp"))
		return B_FP;
	if (!strcmp(x, "sfp"))
		return B_SFP;
	if (!strcmp(x, "ndb"))
		return B_NDB;
	if (!strcmp(x, "ldb"))
		return B_LDB;
	if (!strcmp(x, "ign"))
		return B_IGN;
	if (!strcmp(x, "ign2"))
		return B_IGN2;
	if (!strcmp(x, "hdu") || !strcmp(x, "hsu") || !strcmp(x, "mdu") || !strcmp(x, "msu") || !strcmp(x, "ndu") ||
	    !strcmp(x, "ldu"))
		return B_PUA;
	return B_NADA;
}

static int una_linea(int clase, const char *s, u32 n)
{
	int r;
	while (n && (s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t'))
		n--;
	if (!n || s[0] == '#')
		return 0;
	switch (clase) {
	case B_HDB: r = linea_suma(s, n, 0, 0); break;
	case B_HSB: r = linea_suma(s, n, 0, 0); break;
	case B_MDB: r = linea_suma(s, n, 1, 0); break;
	case B_MSB: r = linea_suma(s, n, 1, 0); break;
	case B_FP: r = linea_suma(s, n, 0, 1); break;
	case B_SFP: r = linea_suma(s, n, 0, 1); break;
	case B_NDB: r = linea_ndb(s, n); break;
	case B_LDB: r = linea_ldb(s, n); break;
	case B_IGN: r = linea_ign(s, n, 0); break;
	case B_IGN2: r = linea_ign(s, n, 1); break;
	case B_PUA: inf_actual->pua++; return 0;
	default: return 0;
	}
	if (r == -1)
		inf_actual->no_soportadas++;
	return r == -2 ? -2 : 0;
}

/* el lector por trozos (para los .cvd, que se descomprimen al vuelo) */
#define MAX_LINEA (256u * 1024)
typedef struct lector {
	char *b;
	u32 n;
	int clase;
	int larga;			/* la línea pasa de MAX_LINEA: se deja */
	int sin_memoria;
	char nombre[101];
} lector;
static lector lec;

static int al_lector(lector *l, const u8 *d, u32 k)
{
	u32 i;
	for (i = 0; i < k; i++) {
		char c = (char)d[i];
		if (c == '\n') {
			l->b[l->n] = 0;	/* (que nadie lea más allá de la línea) */
			if (!l->larga && una_linea(l->clase, l->b, l->n) == -2) {
				l->sin_memoria = 1;
				return 1;
			}
			if (l->larga)
				inf_actual->no_soportadas++;
			l->n = 0, l->larga = 0;
			continue;
		}
		if (l->n < MAX_LINEA)
			l->b[l->n++] = c;
		else
			l->larga = 1;
	}
	return 0;
}

static int lector_acaba(lector *l)
{
	l->b[l->n] = 0;
	if (l->n && !l->larga && una_linea(l->clase, l->b, l->n) == -2)
		l->sin_memoria = 1;
	l->n = 0, l->larga = 0;
	return l->sin_memoria ? -2 : 0;
}

int motor_carga_texto(const char *nombre, const char *t, u32 n, info_firmas *inf)
{
	inf_actual = inf;
	lec.clase = clase_de(nombre);
	if (!lec.b && !(lec.b = pl_reserva(MAX_LINEA + 1)))
		return -2;
	lec.n = 0, lec.larga = 0, lec.sin_memoria = 0;
	al_lector(&lec, (const u8 *)t, n);
	return lector_acaba(&lec);
}

/* lo de dentro de un .cvd, por trozos; d = 0: acaba un archivo */
static int del_cvd(void *yo, const char *nombre, const u8 *d, u32 n)
{
	(void)yo;
	if (strcmp(lec.nombre, nombre)) {
		formatear(lec.nombre, sizeof lec.nombre, "%s", nombre);
		lec.clase = clase_de(nombre);
		lec.n = 0, lec.larga = 0;
	}
	if (!d) {
		lec.nombre[0] = 0;
		return lector_acaba(&lec) == -2;
	}
	return al_lector(&lec, d, n);
}

static int carga_cvd(const char *ruta, info_firmas *inf)
{
	int h = pl_abrir(ruta), r;
	u8 c[512], md5[16], b[8192];
	cabecera_cvd cab;
	md5_ctx m;
	char e[80];
	u32 fecha;
	if (h < 0) {
		formatear(inf->error, sizeof inf->error, "no se puede abrir %s", ruta);
		return -1;
	}
	if (pl_leer(h, c, 512) != 512 || cvd_cabecera(c, &cab)) {
		pl_cerrar(h);
		formatear(inf->error, sizeof inf->error, "%s no es un .cvd de ClamAV", ruta);
		return -1;
	}
	/* primero, la firma digital (la suma de todo lo de detrás) */
	md5_inicia(&m);
	while ((r = pl_leer(h, b, sizeof b)) > 0)
		md5_suma(&m, b, (u32)r);
	md5_acaba(&m, md5);
	if (!cvd_firma_buena(&cab, md5)) {
		pl_cerrar(h);
		formatear(inf->error, sizeof inf->error, "%s: la firma digital de ClamAV no es buena (no se usa)", ruta);
		pl_apunta("escudo: %s: la firma digital NO es buena", ruta);
		return -1;
	}
	pl_apunta("escudo: %s: firma digital de ClamAV buena (versión %u, %u firmas, de %s)", ruta, cab.version,
		  cab.firmas, cab.autor);
	fecha = fecha_de_segundos(cab.segundos);
	if (fecha > inf->fecha)
		inf->fecha = fecha;
	{
		const char *base = strrchr(ruta, '\\');
		const char *b2 = strrchr(ruta, '/');
		if (b2 > base)
			base = b2;
		base = base ? base + 1 : ruta;
		marca_origen = (base[0] | 32) == 'm' ? S_MAIN : 0;
		if ((base[0] | 32) == 'd')
			inf->version_daily = cab.version;
		else if ((base[0] | 32) == 'm')
			inf->version_main = cab.version;
		else if ((base[0] | 32) == 'b')
			inf->version_bytecode = cab.version;
	}
	if (cab.flevel > ESCUDO_FLEVEL)
		pl_apunta("escudo: %s pide el nivel %u (el motor dice %u): lo que no entienda, se deja", ruta, cab.flevel,
			  ESCUDO_FLEVEL);
	inf_actual = inf;
	if (!lec.b && !(lec.b = pl_reserva(MAX_LINEA + 1))) {
		pl_cerrar(h);
		return -2;
	}
	lec.nombre[0] = 0, lec.sin_memoria = 0;
	r = cvd_recorre(h, 512, del_cvd, 0, e, sizeof e);
	marca_origen = 0;
	pl_cerrar(h);
	if (r) {
		formatear(inf->error, sizeof inf->error, "%s: %s", ruta, e);
		return -1;
	}
	if (lec.sin_memoria) {
		formatear(inf->error, sizeof inf->error, "%s: no hay memoria bastante para todas sus firmas", ruta);
		return -2;
	}
	return 0;
}

typedef struct marca { u32 toks, segs, pats, firmas, conj, alts, altdatos, exprs, cuentas; } marca;
static void pon_marca(marca *m)
{
	m->toks = n_toks, m->segs = n_segs, m->pats = n_pats, m->firmas = n_firmas, m->conj = n_conj;
	m->alts = n_alts, m->altdatos = n_altdatos, m->exprs = n_exprs, m->cuentas = n_cuentas;
}
static void vuelve_a(const marca *m)
{
	n_toks = m->toks, n_segs = m->segs, n_pats = m->pats, n_firmas = m->firmas, n_conj = m->conj;
	n_alts = m->alts, n_altdatos = m->altdatos, n_exprs = m->exprs, n_cuentas = m->cuentas;
}

/* ---- el cuerpo de los FIRMAS.ESC: bloques de 64 K, comprimidos (deflate) ---- */
typedef struct bloque_c { int esc; u32 bloque; u32 n; u8 *d; } bloque_c;
static bloque_c bcache[BLOQUES_CACHE];
extern u32 motor_bloques;	/* (para medir) */
static int bcache_turno;
typedef struct de_mem { const u8 *p; u32 n, i; u8 *sale; u32 hecho, cabe; } de_mem;
static int mem_lee(void *yo, u8 *d, u32 cabe)
{
	de_mem *m = yo;
	u32 k = m->n - m->i < cabe ? m->n - m->i : cabe;
	memcpy(d, m->p + m->i, k);
	m->i += k;
	return (int)k;
}
static int mem_saca(void *yo, const u8 *d, u32 n)
{
	de_mem *m = yo;
	if (m->hecho + n > m->cabe)
		return 1;
	memcpy(m->sale + m->hecho, d, n);
	m->hecho += n;
	return 0;
}

/* el bloque «bl» del FIRMAS.ESC «e», descomprimido (en una de las dos cachés) */
static bloque_c *bloque_de(int e, u32 bl)
{
	esc *x = &escs[e];
	bloque_c *c;
	u32 n;
	u8 *z;
	static inflar zi;
	de_mem m;
	int i;
	for (i = 0; i < BLOQUES_CACHE; i++)
		if (bcache[i].d && bcache[i].esc == e && bcache[i].bloque == bl)
			return &bcache[i];
	if (bl >= x->n_bloques)
		return 0;
	c = &bcache[bcache_turno];
	bcache_turno = (bcache_turno + 1) % BLOQUES_CACHE;
	if (!c->d && !(c->d = pl_reserva(BLOQUE + 1)))
		return 0;
	if (x->bloques[bl + 1] < x->bloques[bl] || x->bloques[bl + 1] - x->bloques[bl] > 2 * BLOQUE)
		return 0;
	n = x->bloques[bl + 1] - x->bloques[bl];
	z = pl_reserva(n + 1);
	if (!z)
		return 0;
	if (pl_mover(x->h, x->bloques[bl]) < 0 || pl_leer(x->h, z, n) != (int)n) {
		pl_libera(z);
		return 0;
	}
	memset(&zi, 0, sizeof zi);
	m.p = z, m.n = n, m.i = 0, m.sale = c->d, m.hecho = 0, m.cabe = BLOQUE;
	zi.lee = mem_lee, zi.saca = mem_saca, zi.yo = &m;
	i = inflar_todo(&zi);
	pl_libera(z);
	if (i != INF_BIEN) {
		c->esc = -1;
		return 0;
	}
	c->esc = e, c->bloque = bl, c->n = m.hecho;
	motor_bloques++;
	c->d[c->n] = 0;
	return c;
}

/* la línea de «ref» (bloque << 16 | sitio): su clase ('n', 'l' o 'i') y su texto */
static int linea_de(int e, u32 ref, const char **t, u32 *n)
{
	bloque_c *c = bloque_de(e, ref >> 16);
	u32 p = ref & 0xFFFF, q;
	if (!c || p + 2 >= c->n || c->d[p + 1] != '|')
		return 0;
	for (q = p; q < c->n && c->d[q] != '\n'; q++) ;
	*t = (const char *)c->d + p + 2;
	*n = q - p - 2;
	return c->d[p];
}

static int clase_linea(int c) { return c == 'n' ? B_NDB : c == 'l' ? B_LDB : c == 'i' ? B_IGN2 : B_NADA; }

/* un FIRMAS.ESC: no se fía de nada de su cabecera (puede venir de una memoria
 * USB o estar roto): todo se mide contra lo que mide el archivo; si algo no
 * cuadra, o falta memoria, se deja entero (no se queda contado a medias) */
static int carga_esc(const char *ruta, info_firmas *inf)
{
	u8 c[128];
	u32 i, n_disp, off_disp, n_siempre, off_siempre, off_bits, off_bloques, largo, disps_antes = n_disps;
	u32 sum0 = inf->sumas, cu0 = inf->cuerpo, lo0 = inf->logicas, li0 = inf->limpias, fe0 = inf->fecha;
	u32 vd0 = inf->version_daily, vm0 = inf->version_main, vb0 = inf->version_bytecode, tc0 = tipos_comodin;
	int h = pl_abrir(ruta), e, r = -1;
	marca mk;
	if (h < 0)
		return -1;
	largo = pl_largo(h);
	if (pl_leer(h, c, 128) != 128 || memcmp(c, "HPESCUD2", 8)) {
		pl_cerrar(h);
		formatear(inf->error, sizeof inf->error, "%s no es un FIRMAS.ESC (de la versión 2)", ruta);
		return -1;
	}
	if (n_escs >= MAX_ESC) {
		pl_cerrar(h);
		formatear(inf->error, sizeof inf->error, "demasiados .ESC (%u como mucho)", MAX_ESC);
		return -1;
	}
	pon_marca(&mk);
	e = n_escs++;
	E = &escs[e];
	memset(E, 0, sizeof *E);
	E->h = h;
	E->cache_pag = INF;
	E->n_sumas = le32(c + 24), E->off_sumas = le32(c + 28);
	E->n_tramos = le32(c + 32), E->off_tramos = le32(c + 36);
	off_bits = le32(c + 40);
	E->n_nombres = le32(c + 44), E->idx = le32(c + 48), E->blob = le32(c + 52);
	E->n_bloques = le32(c + 56), off_bloques = le32(c + 60);
	n_disp = le32(c + 64), off_disp = le32(c + 68);
	n_siempre = le32(c + 72), off_siempre = le32(c + 76);
	E->n_comodin = le32(c + 80);
	E->nbits = le32(c + 96) ? le32(c + 96) : 21;
	/* lo que dice la cabecera, contra lo que mide el archivo */
	if (E->nbits < 8 || E->nbits > 24 || E->n_comodin > E->n_tramos || E->n_bloques >= (1u << 14) ||
	    !cabe_en(E->off_sumas, E->n_sumas, 8, largo) || !cabe_en(E->off_tramos, E->n_tramos, 12, largo) ||
	    !cabe_en(off_bits, 1u << (E->nbits - 3), 1, largo) || !cabe_en(E->idx, E->n_nombres, 4, largo) ||
	    E->blob > largo || !cabe_en(off_bloques, E->n_bloques + 1, 4, largo) || !cabe_en(off_disp, n_disp, 12, largo) ||
	    !cabe_en(off_siempre, n_siempre, 4, largo))
		goto roto;
	E->bits = pl_reserva(1u << (E->nbits - 3));
	E->bloques = pl_reserva((E->n_bloques + 1) * 4);
	E->comodin = pl_reserva(E->n_comodin * sizeof(tramo) + 1);
	E->paginas = (E->n_tramos - E->n_comodin + TRAMOS_POR_PAGINA - 1) / TRAMOS_POR_PAGINA;
	E->pag = pl_reserva(E->paginas * 4 + 4);
	r = -2;
	if (!E->bits || !E->bloques || !E->comodin || !E->pag)
		goto fuera;
	r = -1;
	if (pl_mover(h, off_bits) < 0 || pl_leer(h, E->bits, 1u << (E->nbits - 3)) != (int)(1u << (E->nbits - 3)))
		goto roto;
	if (pl_mover(h, off_bloques) < 0 || pl_leer(h, E->bloques, (E->n_bloques + 1) * 4) != (int)((E->n_bloques + 1) * 4))
		goto roto;
	for (i = 0; i <= E->n_bloques; i++) {
		E->bloques[i] = le32((u8 *)&E->bloques[i]);
		/* (cada bloque, detrás del anterior, dentro del archivo y no mayor que 2 × BLOQUE) */
		if (E->bloques[i] > largo || (i && (E->bloques[i] < E->bloques[i - 1] || E->bloques[i] - E->bloques[i - 1] > 2 * BLOQUE)))
			goto roto;
	}
	for (i = 0; i < E->paginas; i++) {
		u8 b[4];
		if (pl_mover(h, E->off_tramos + i * TRAMOS_POR_PAGINA * 12) < 0 || pl_leer(h, b, 4) != 4)
			goto roto;
		E->pag[i] = le32(b);
	}
	for (i = 0; i < E->n_comodin; i++) {	/* (los últimos tramos) */
		u8 b[12];
		if (pl_mover(h, E->off_tramos + (E->n_tramos - E->n_comodin + i) * 12) < 0 || pl_leer(h, b, 12) != 12)
			goto roto;
		E->comodin[i].tam = INF;
		E->comodin[i].tipo = le32(b + 4) & 0xFF;
		E->comodin[i].primero = le32(b + 4) >> 8;
		E->comodin[i].cuantos = le32(b + 8);
		if (!cabe_en(E->comodin[i].primero, E->comodin[i].cuantos, 1, E->n_sumas))
			goto roto;
		tipos_comodin |= 1u << (E->comodin[i].tipo & 0x1F);
	}
	/* los disparadores, a memoria (todos los de todos los .ESC juntos) */
	r = -2;
	if (n_disp > 0x7FFFFFFFu - n_disps || !CRECE(disps, cap_disps, n_disps + n_disp))
		goto fuera;
	r = -1;
	if (pl_mover(h, off_disp) < 0)
		goto roto;
	for (i = 0; i < n_disp;) {
		static u8 b[12 * 1024];
		u32 k = n_disp - i < 1024 ? n_disp - i : 1024, j;
		if (pl_leer(h, b, k * 12) != (int)(k * 12))
			goto roto;
		for (j = 0; j < k; j++, i++) {
			disp *d = &disps[n_disps++];
			d->clave = le32(b + 12 * j);
			d->ref = le32(b + 12 * j + 4);
			d->sl = (le32(b + 12 * j + 8) & 0x07FFFFFF) | (u32)e << 28;
		}
	}
	/* las que van siempre en memoria (sin 4 octetos fijos, y las .ign2) */
	inf_actual = inf;
	for (i = 0; i < n_siempre; i++) {
		u8 b[4];
		const char *t;
		u32 n;
		int cl;
		if (pl_mover(h, off_siempre + i * 4) < 0 || pl_leer(h, b, 4) != 4)
			goto roto;
		cl = linea_de(e, le32(b), &t, &n);
		if (cl && una_linea(clase_linea(cl), t, n) == -2) {
			r = -2;
			goto fuera;
		}
	}
	/* bien: lo de la cabecera, a la cuenta */
	inf->cuerpo = cu0, inf->logicas = lo0;
	if (le32(c + 8) > inf->fecha)
		inf->fecha = le32(c + 8);
	if (le32(c + 12))
		inf->version_daily = le32(c + 12);
	if (le32(c + 16))
		inf->version_main = le32(c + 16);
	if (le32(c + 20))
		inf->version_bytecode = le32(c + 20);
	inf->sumas += E->n_sumas;
	inf->cuerpo += le32(c + 84);
	inf->logicas += le32(c + 88);
	inf->limpias += le32(c + 92);
	return 0;
roto:
	formatear(inf->error, sizeof inf->error, "%s está roto o no es de este Escudo: no se usa", ruta);
	pl_apunta("escudo: %s está roto: no se usa", ruta);
fuera:
	if (r == -2)
		formatear(inf->error, sizeof inf->error, "no hay memoria para %s: no se usa", ruta);
	/* fuera entero: ni sus disparadores, ni lo que haya cargado, ni sus cuentas */
	vuelve_a(&mk);
	n_disps = disps_antes;
	tipos_comodin = tc0;
	inf->sumas = sum0, inf->cuerpo = cu0, inf->logicas = lo0, inf->limpias = li0, inf->fecha = fe0;
	inf->version_daily = vd0, inf->version_main = vm0, inf->version_bytecode = vb0;
	pl_cerrar(h);
	pl_libera(E->pag), pl_libera(E->bits), pl_libera(E->comodin), pl_libera(E->bloques);
	memset(E, 0, sizeof *E);
	E->h = -1;
	n_escs--;
	for (i = 0; i < BLOQUES_CACHE; i++)
		if (bcache[i].esc == e)
			bcache[i].esc = -1;
	if (pcache)
		for (i = 0; i < PAGS_CACHE; i++)
			if (pcache[i].esc == e)
				pcache[i].esc = -1;
	E = 0;
	return r;
}

int motor_carga(const char *ruta, info_firmas *inf) { return motor_carga_como(ruta, ruta, inf); }

/* «nombre» (el largo, en Hispa-Pentium: PRUEBA.IGN2 es PRUEBA~1.IGN) dice qué es */
int motor_carga_como(const char *ruta, const char *nombre, info_firmas *inf)
{
	const char *e = strrchr(nombre, '.');
	char x[5] = { 0 };
	int i, h, r;
	u32 n;
	if (e)
		for (i = 0; i < 4 && e[i + 1]; i++)
			x[i] = (char)(e[i + 1] | 32);
	if (!strcmp(x, "cvd") || !strcmp(x, "cld"))
		return carga_cvd(ruta, inf);
	if (!strcmp(x, "esc"))
		return carga_esc(ruta, inf);
	if (clase_de(nombre) == B_NADA)
		return 1;
	h = pl_abrir(ruta);
	if (h < 0) {
		formatear(inf->error, sizeof inf->error, "no se puede abrir %s", ruta);
		return -1;
	}
	inf_actual = inf;
	if (!lec.b && !(lec.b = pl_reserva(MAX_LINEA + 1))) {
		pl_cerrar(h);
		return -2;
	}
	lec.clase = clase_de(nombre), lec.n = 0, lec.larga = 0, lec.sin_memoria = 0;
	n = 0;
	for (;;) {
		u8 b[4096];
		r = pl_leer(h, b, sizeof b);
		if (r <= 0)
			break;
		n += (u32)r;
		if (al_lector(&lec, b, (u32)r))
			break;
	}
	pl_cerrar(h);
	return lector_acaba(&lec);
}

/* ------------------------------------------------ los disparadores
 * De cada firma del cuerpo que va en el disco se guardan unos cuantos
 * «disparadores»: 4 octetos fijos de sus patrones (y hasta 2 más para
 * descartar antes). Bastan para la firma: si no aparece ninguno, la firma no
 * puede casar. En una .ldb, los de las subfirmas que la expresión pide sí o
 * sí: de «0&1&2», los de una sola (la de menos patrones); de «0|1», los de
 * las dos; con «=0» o «<n» la firma puede casar sin nada: va siempre en
 * memoria. */
/* lee una línea sola (sin apuntar su nombre ni contarla): 1 si ha quedado una
 * firma (la última de «firmas»), con sus patrones desde m->pats */
static __attribute__((unused)) int linea_temporal(int clase, const char *t, u32 n, marca *m)
{
	static info_firmas falsa;
	info_firmas *antes = inf_actual;
	int r;
	pon_marca(m);
	inf_actual = &falsa;
	modo_temporal = 1;
	n_tnombres = 0;
	r = una_linea(clase, t, n);
	modo_temporal = 0;
	inf_actual = antes;
	if (r < 0 || n_firmas == m->firmas || n_pats == m->pats) {
		vuelve_a(m);
		return 0;
	}
	return 1;
}

/* los 4 octetos fijos más raros de un patrón (y hasta 3 detrás: un
 * disparador de 7 octetos casi nunca salta por casualidad): 0 si no tiene */
/* (al hacer FIRMAS.ESC) cuántas firmas tienen cada ventana de 4 a 7 octetos:
 * el disparador de cada una, de las que menos se repiten («msvbvm» sale en
 * 100.000 firmas de Visual Basic y en cualquier programa hecho con él) */
static u16 *frec;
static u8 *limpio4, *limpio7;		/* (y cuántas veces sale cada una en archivos limpios: motor_corpus) */
#define LIMPIO_BITS 26
static u32 h_limpio(u32 clave, u32 sl) { return ((clave * 2654435761u) ^ (sl * 2246822519u) ^ (sl >> 11)) >> (32 - LIMPIO_BITS); }
/* (lo que cuenta: el ruido de la tabla —unas pocas por casilla— no) */
static u32 cuenta_limpio(const u8 *t, u32 h) { return t[h] > 3 ? t[h] - 3 : 0; }
#define FREC_BITS 24
static u32 h_ventana(u32 clave, u32 sl) { return ((clave * 2654435761u) ^ (sl * 40503u) ^ (sl >> 13)) >> (32 - FREC_BITS); }

static int ancla4(const patron *p, u32 *clave, u32 *sl, u32 *coste)
{
	u32 mejor = 0, s, k;
	for (s = 0; s < p->nseg; s++) {
		const seg *sg = &segs[p->seg0 + s];
		const tok *t = &toks[sg->ini];
		for (k = 0; k + 4 <= sg->largo; k++) {
			u32 pt = 0, j, l = 0, distintos = 0;
			for (j = 0; j < 4; j++)
				if (!es_fijo(&t[k + j]))
					break;
			if (j < 4)
				continue;
			while (l < 3 && k + 4 + l < sg->largo && es_fijo(&t[k + 4 + l]))
				l++;
			for (j = 0; j < 4 + l; j++) {
				u32 q;
				u8 v = fold[t[k + j].v];
				pt += (u32)rareza(t[k + j].v) * 4;
				if (v == 0 || v == 0xFF)
					continue;
				for (q = 0; q < j && fold[t[k + q].v] != v; q++) ;
				if (q == j)
					distintos++;	/* (los valores distintos, sin contar ceros ni FF) */
			}
			/* (ceros, FF o lo mismo una y otra vez: eso está en cualquier archivo) */
			if (distintos < 3)
				continue;
			pt += distintos * 3 + l * 8;
			{
				u32 c = fold[t[k].v] | (u32)fold[t[k + 1].v] << 8 | (u32)fold[t[k + 2].v] << 16 |
					(u32)fold[t[k + 3].v] << 24, x = l << 24, f = 0;
				for (j = 0; j < l; j++)
					x |= (u32)fold[t[k + 4 + j].v] << (8 * j);
				if (frec) {
					f = frec[h_ventana(c, x)];
					if (limpio4)	/* lo que sale en los archivos limpios, mucho peor */
						f += cuenta_limpio(limpio7, h_limpio(c, x)) * 4000 + cuenta_limpio(limpio4, h_limpio(c, 0)) * 60;
					if (f > 65535)
						f = 65535;
					pt += (65535 - f) * 256;
				}
				if (pt > mejor) {
					mejor = pt;
					*clave = c, *sl = x;
					if (coste)
						*coste = f;
				}
			}
		}
	}
	return mejor > 0;
}

static u32 cuenta_bits(u64 x)
{
	u32 n = 0;
	for (; x; x &= x - 1)
		n++;
	return n;
}

/* las subfirmas que la expresión de «f» pide sí o sí: 0 si puede casar sin ninguna */
static u32 coste_de(u64 s, const u32 *coste)
{
	u32 c = 0, i;
	for (i = 0; i < 64; i++)
		if (s >> i & 1)
			c = c + coste[i] < c ? INF : c + coste[i];
	return c;
}

static int requeridas(const firma *f, u64 *sale, const u32 *coste)
{
	u64 s[32];
	u8 ok[32];
	int sp = 0;
	const u32 *x;
	if (f->clase == F_NDB) {
		*sale = 1;
		return 1;
	}
	for (x = exprs + f->expr; *x; x++) {
		u32 op = *x & 0xFF000000u;
		if (op == X_SUB) {
			if (sp >= 32)
				return 0;
			s[sp] = (u64)1 << (*x & 0x3F), ok[sp] = 1;
			sp++;
		} else if (op == X_AND || op == X_OR) {
			if (sp < 2)
				return 0;
			sp--;
			if (op == X_OR) {
				ok[sp - 1] = ok[sp - 1] && ok[sp];
				s[sp - 1] |= s[sp];
			} else if (ok[sp - 1] && ok[sp]) {
				u32 a = coste_de(s[sp - 1], coste), b = coste_de(s[sp], coste);
				if (b < a || (b == a && cuenta_bits(s[sp]) < cuenta_bits(s[sp - 1])))
					s[sp - 1] = s[sp];
			} else if (ok[sp]) {
				s[sp - 1] = s[sp], ok[sp - 1] = 1;
			}
		} else {
			u32 v = (*x >> 12) & 0xFFF;
			if (sp < 1)
				return 0;
			if (op == X_MENOR || (op == X_IGUAL && v == 0))
				ok[sp - 1] = 0;
		}
	}
	if (sp != 1 || !ok[0])
		return 0;
	*sale = s[0];
	return 1;
}

/* los disparadores de la firma que acaba de quedar (desde «m»): cuántos, o
 * -1 si tiene que ir siempre en memoria */
static __attribute__((unused)) int disparadores_de(const marca *m, disp *d, int max)
{
	const firma *f = &firmas[n_firmas - 1];
	u64 req;
	u32 p, coste[64], c;
	int n = 0, i;
	for (i = 0; i < 64; i++)
		coste[i] = 0;
	for (p = m->pats; p < n_pats; p++) {	/* lo que cuesta cada subfirma: su peor patrón */
		disp x;
		if (pats[p].sub >= 64)
			return -1;
		if (!ancla4(&pats[p], &x.clave, &x.sl, &c))
			c = INF;
		if (c > coste[pats[p].sub])
			coste[pats[p].sub] = c;
	}
	if (!requeridas(f, &req, coste))
		return -1;
	for (p = m->pats; p < n_pats; p++) {
		disp x;
		if (!(req >> pats[p].sub & 1))
			continue;
		if (!ancla4(&pats[p], &x.clave, &x.sl, 0))
			return -1;
		for (i = 0; i < n; i++)
			if (d[i].clave == x.clave && d[i].sl == x.sl)
				break;
		if (i < n)
			continue;
		if (n >= max)
			return -1;
		d[n++] = x;
	}
	return n ? n : -1;
}

/* el índice de los disparadores (en motor_prepara) */
static int compara_disp(const disp *a, const disp *b)
{
	u32 x = h4(a->clave), y = h4(b->clave);
	return x < y ? -1 : x > y;
}
static void ordena_disps(void)
{
	u32 i, raiz, hijo, fin, n = n_disps;
	disp t, *v = disps;
	if (n < 2)
		return;
	for (i = n / 2; i-- > 0;)
		for (raiz = i; (hijo = 2 * raiz + 1) < n; raiz = hijo) {
			if (hijo + 1 < n && compara_disp(&v[hijo], &v[hijo + 1]) < 0)
				hijo++;
			if (compara_disp(&v[raiz], &v[hijo]) >= 0)
				break;
			t = v[raiz], v[raiz] = v[hijo], v[hijo] = t;
		}
	for (fin = n - 1; fin > 0; fin--) {
		t = v[0], v[0] = v[fin], v[fin] = t;
		for (raiz = 0; (hijo = 2 * raiz + 1) < fin; raiz = hijo) {
			if (hijo + 1 < fin && compara_disp(&v[hijo], &v[hijo + 1]) < 0)
				hijo++;
			if (compara_disp(&v[raiz], &v[hijo]) >= 0)
				break;
			t = v[raiz], v[raiz] = v[hijo], v[hijo] = t;
		}
	}
}
static int prepara_disps(void)
{
	u32 i, j;
	if (!n_disps)
		return 1;
	ordena_disps();
	if (!mapa4)
		mapa4 = pl_reserva(1u << 20);
	if (!cubo4)
		cubo4 = pl_reserva(((1u << 16) + 1) * 4);
	if (!mapa4 || !cubo4)
		return 0;
	memset(mapa4, 0, 1u << 20);
	for (i = 0; i < n_disps; i++) {
		u32 h = h4(disps[i].clave);
		mapa4[h >> 3] |= (u8)(1u << (h & 7));
	}
	for (i = 0, j = 0; i <= (1u << 16); i++) {
		while (j < n_disps && (h4(disps[j].clave) >> 7) < i)
			j++;
		cubo4[i] = j;
	}
	return 1;
}

/* --------------------------------------------- preparar los índices */
static int compara_ancla(const ancla *a, const ancla *b, int bits3)
{
	u32 x = bits3 ? h3(a->clave) : a->clave, y = bits3 ? h3(b->clave) : b->clave;
	return x < y ? -1 : x > y;
}

static void ordena_anclas(ancla *v, u32 n, int bits3)
{
	u32 i, raiz, hijo, fin;
	ancla t;
	if (n < 2)
		return;
	for (i = n / 2; i-- > 0;)
		for (raiz = i; (hijo = 2 * raiz + 1) < n; raiz = hijo) {
			if (hijo + 1 < n && compara_ancla(&v[hijo], &v[hijo + 1], bits3) < 0)
				hijo++;
			if (compara_ancla(&v[raiz], &v[hijo], bits3) >= 0)
				break;
			t = v[raiz], v[raiz] = v[hijo], v[hijo] = t;
		}
	for (fin = n - 1; fin > 0; fin--) {
		t = v[0], v[0] = v[fin], v[fin] = t;
		for (raiz = 0; (hijo = 2 * raiz + 1) < fin; raiz = hijo) {
			if (hijo + 1 < fin && compara_ancla(&v[hijo], &v[hijo + 1], bits3) < 0)
				hijo++;
			if (compara_ancla(&v[raiz], &v[hijo], bits3) >= 0)
				break;
			t = v[raiz], v[raiz] = v[hijo], v[hijo] = t;
		}
	}
}

static u32 clave_de(const patron *p)
{
	const seg *s = &segs[p->seg0 + p->ancla_seg];
	const tok *t = &toks[s->ini + p->ancla_pos];
	u32 k = fold[t[0].v] | (u32)fold[t[1].v] << 8;
	if (p->ancla_largo == 3)
		k |= (u32)fold[t[2].v] << 16;
	return k;
}

void motor_prepara(info_firmas *inf)
{
	u32 i, j, n3 = 0, n2 = 0;
	/* las sumas: por tamaño (las de cualquiera, al final) */
	ordena_sumas(sumas, n_sumas);
	n_comodin_ram = 0;
	for (i = n_sumas; i > 0 && sumas[i - 1].tam == INF; i--)
		n_comodin_ram++, tipos_comodin |= 1u << (sumas[i - 1].nt & 0x1F);
	/* las anclas */
	pl_libera(anclas3), pl_libera(anclas2), pl_libera(fijos);
	anclas3 = anclas2 = 0;
	fijos = 0;
	n_fijos = 0;
	for (i = 0; i < n_pats; i++) {
		if (pats[i].o_tipo != O_CUALQUIERA)
			n_fijos++;
		else if (pats[i].ancla_largo == 3)
			n3++;
		else
			n2++;
	}
	anclas3 = pl_reserva(n3 * sizeof(ancla) + 1);
	anclas2 = pl_reserva(n2 * sizeof(ancla) + 1);
	fijos = pl_reserva(n_fijos * 4 + 4);
	if (!mapa3)
		mapa3 = pl_reserva(1u << (BITS3 - 3));
	if (!mapa2)
		mapa2 = pl_reserva(1u << 13);
	if (!cubo3)
		cubo3 = pl_reserva(((1u << 16) + 1) * 4);
	if (!cubo2)
		cubo2 = pl_reserva(((1u << 16) + 1) * 4);
	if (!anclas3 || !anclas2 || !fijos || !mapa3 || !mapa2 || !cubo3 || !cubo2) {
		formatear(inf->error, sizeof inf->error, "no hay memoria para los índices de los patrones");
		n_pats = 0;
		n_a3 = n_a2 = n_fijos = 0;
		return;
	}
	memset(mapa3, 0, 1u << (BITS3 - 3));
	memset(mapa2, 0, 1u << 13);
	n_a3 = n_a2 = n_fijos = 0;
	for (i = 0; i < n_pats; i++) {
		if (pats[i].o_tipo != O_CUALQUIERA) {
			fijos[n_fijos++] = i;
			continue;
		}
		if (pats[i].ancla_largo == 3) {
			anclas3[n_a3].clave = clave_de(&pats[i]);
			anclas3[n_a3].pat = i;
			mapa3[h3(anclas3[n_a3].clave) >> 3] |= (u8)(1u << (h3(anclas3[n_a3].clave) & 7));
			n_a3++;
		} else {
			anclas2[n_a2].clave = clave_de(&pats[i]);
			anclas2[n_a2].pat = i;
			mapa2[anclas2[n_a2].clave >> 3] |= (u8)(1u << (anclas2[n_a2].clave & 7));
			n_a2++;
		}
	}
	ordena_anclas(anclas3, n_a3, 1);
	ordena_anclas(anclas2, n_a2, 0);
	/* los cubos: por los 16 bits altos de h3, y por la clave de 2 */
	for (i = 0, j = 0; i <= (1u << 16); i++) {
		while (j < n_a3 && (h3(anclas3[j].clave) >> 4) < i)
			j++;
		cubo3[i] = j;
	}
	for (i = 0, j = 0; i <= (1u << 16); i++) {
		while (j < n_a2 && anclas2[j].clave < i)
			j++;
		cubo2[i] = j;
	}
	pl_libera(cuentas);
	n_cuentas_cabe = n_cuentas + 8192;	/* (y sitio para las que se leen de los disparadores) */
	cuentas = pl_reserva(n_cuentas_cabe);
	if (!cuentas)
		n_cuentas = n_cuentas_cabe = 0;
	else
		memset(cuentas, 0, n_cuentas_cabe);
	if (!prepara_disps())
		formatear(inf->error, sizeof inf->error, "no hay memoria para los disparadores");
	inf->memoria_k = (n_sumas * sizeof(suma) + arena_n + n_nombres * 4 + cap_disp * 4 + n_toks * sizeof(tok) +
			  n_segs * sizeof(seg) + n_pats * sizeof(patron) + n_firmas * sizeof(firma) + n_conj * 32 +
			  n_altdatos + n_exprs * 4 + (n_a3 + n_a2) * sizeof(ancla) + (1u << (BITS3 - 3)) + (1u << 13) +
			  2 * ((1u << 16) + 1) * 4 + MAX_LINEA + BLOQUES_CACHE * BLOQUE +
			  cap_disps * sizeof(disp) + (n_disps ? (1u << 20) + ((1u << 16) + 1) * 4 : 0)) / 1024;
	{
		int k;
		for (k = 0; k < n_escs; k++)
			inf->memoria_k += (escs[k].paginas * 4 + escs[k].n_bloques * 4 + (1u << (escs[k].nbits - 3))) / 1024;
	}
	pl_apunta("escudo: firmas: %u sumas, %u limpias, %u del cuerpo, %u lógicas (%u patrones en memoria: %u con ancla de 3, %u de 2, %u fijos; %u disparadores de las del disco); %u no soportadas, %u PUA sin cargar; %u K en memoria",
		  inf->sumas, inf->limpias, inf->cuerpo, inf->logicas, n_pats, n_a3, n_a2, n_fijos, n_disps, inf->no_soportadas,
		  inf->pua, inf->memoria_k);
}

void motor_inicia(void)
{
	int i;
	for (i = 0; i < 256; i++)
		fold[i] = (u8)(i >= 'A' && i <= 'Z' ? i + 32 : i);
}

void motor_suelta(void)
{
	pl_libera(sumas), pl_libera(arena), pl_libera(nombres), pl_libera(disp_nombres);
	pl_libera(toks), pl_libera(segs), pl_libera(pats), pl_libera(firmas), pl_libera(conjuntos);
	pl_libera(alts), pl_libera(altdatos), pl_libera(exprs), pl_libera(cuentas), pl_libera(tocadas);
	pl_libera(ignorar), pl_libera(lineas), pl_libera(anclas3), pl_libera(anclas2), pl_libera(fijos);
	sumas = 0, arena = 0, nombres = 0, disp_nombres = 0, toks = 0, segs = 0, pats = 0, firmas = 0;
	conjuntos = 0, alts = 0, altdatos = 0, exprs = 0, cuentas = 0, tocadas = 0, ignorar = 0, lineas = 0;
	anclas3 = anclas2 = 0, fijos = 0;
	n_sumas = cap_sumas = arena_n = arena_cap = n_nombres = cap_nombres = cap_disp = 0;
	n_toks = cap_toks = n_segs = cap_segs = n_pats = cap_pats = n_firmas = cap_firmas = 0;
	n_conj = cap_conj = n_alts = cap_alts = n_altdatos = cap_altdatos = n_exprs = cap_exprs = 0;
	n_cuentas = n_tocadas = cap_tocadas = n_ignorar = cap_ignorar = n_lineas = cap_lineas = 0;
	n_a3 = n_a2 = n_fijos = 0;
	n_comodin_ram = 0, tipos_comodin = 0;
	while (n_escs > 0) {
		E = &escs[--n_escs];
		if (E->h >= 0)
			pl_cerrar(E->h);
		pl_libera(E->pag), pl_libera(E->bits), pl_libera(E->comodin), pl_libera(E->bloques);
		memset(E, 0, sizeof *E);
	}
	E = 0;
	pl_libera(disps);
	disps = 0, n_disps = cap_disps = 0;
	for (bcache_turno = 0; bcache_turno < BLOQUES_CACHE; bcache_turno++)
		bcache[bcache_turno].esc = -1;
	bcache_turno = 0;
	if (pcache) {
		int k;
		for (k = 0; k < PAGS_CACHE; k++)
			pcache[k].esc = -1;
	}
}

/* ------------------------------------------------------- buscar */
/* lo que se sabe del archivo que se mira */
#define A_PE 1
#define A_ELF 6
#define A_MACHO 9
#define A_PDF 10
#define A_JAVA 12
typedef struct pe_sec { u32 va, vtam, raw, rtam; } pe_sec;
typedef struct mirado {
	const u8 *b;
	u32 n;				/* lo que hay en «b» */
	u32 total;			/* lo que mide el archivo entero */
	u32 clases;			/* bits 1 << A_... */
	int contenedor;			/* 1: dentro de un ZIP */
	u32 ep;				/* el punto de entrada (desplazamiento en el archivo); INF si no */
	u32 nsec;
	pe_sec sec[96];
	int pasos;			/* lo que queda para los huecos largos */
} mirado;

static int casa_seg(const tok *t, u32 largo, const u8 *b, u32 n, u32 p)
{
	u32 j;
	if (p + largo > n)
		return 0;
	for (j = 0; j < largo; j++, t++) {
		u8 c = b[p + j];
		switch (t->k) {
		case T_BYTE:
			if ((c & t->m) != t->v)
				return 0;
			break;
		case T_BYTEI:
			if (fold[c] != t->v)
				return 0;
			break;
		case T_NO:
			if ((c & t->m) == t->v)
				return 0;
			break;
		case T_CONJ: {
			const u8 *cj = conjuntos + 32 * (t->m | (u32)t->x << 8);
			int esta = cj[c >> 3] >> (c & 7) & 1;
			if (esta == t->v)
				return 0;
			break;
		}
		case T_ALT: {
			const alt *a = &alts[t->m | (u32)t->x << 8];
			u32 i;
			for (i = 0; i < a->n; i++)
				if (p + j + a->largo <= n && !memcmp(b + p + j, altdatos + a->datos + i * a->largo, a->largo))
					break;
			if (i == a->n)
				return 0;
			break;
		}
		case T_ALTC:
			break;
		case T_CLASE:
			if (t->v == 'W' && ((c >= '0' && c <= '9') || (fold[c] >= 'a' && fold[c] <= 'z')))
				return 0;
			if (t->v == 'L' && c != '\n' && c != '\r')
				return 0;
			if (t->v == 'B' && ((c >= '0' && c <= '9') || (fold[c] >= 'a' && fold[c] <= 'z') || c == '_'))
				return 0;
			break;
		}
	}
	return 1;
}

static int adelante(const patron *pt, u32 k, u32 fin, mirado *m)
{
	const seg *s, *sig;
	u32 lo, hi, p;
	if (k + 1 >= pt->nseg) {
		if (pt->palabra && fin < m->n) {
			u8 c = m->b[fin];
			if ((c >= '0' && c <= '9') || (fold[c] >= 'a' && fold[c] <= 'z'))
				return 0;
		}
		return 1;
	}
	s = &segs[pt->seg0 + k];
	sig = s + 1;
	lo = fin + s->gmin;
	if (lo + sig->largo > m->n || lo < fin)
		return 0;
	hi = s->gmax == INF || fin + s->gmax < fin ? m->n - sig->largo : fin + s->gmax;
	if (hi > m->n - sig->largo)
		hi = m->n - sig->largo;
	for (p = lo; p <= hi; p++) {
		if (--m->pasos < 0)
			return 0;
		if (casa_seg(toks + sig->ini, sig->largo, m->b, m->n, p) && adelante(pt, k + 1, p + sig->largo, m))
			return 1;
	}
	return 0;
}

static u32 ini_casado;
static int atras(const patron *pt, u32 k, u32 ini, mirado *m)
{
	const seg *ant;
	u32 lo, hi, p;
	if (k == 0) {
		if (pt->palabra && ini > 0) {
			u8 c = m->b[ini - 1];
			if ((c >= '0' && c <= '9') || (fold[c] >= 'a' && fold[c] <= 'z'))
				return 0;
		}
		ini_casado = ini;
		return 1;
	}
	ant = &segs[pt->seg0 + k - 1];
	/* ant empieza en p: p + largo + hueco = ini, hueco en [gmin, gmax] */
	if (ini < ant->largo + ant->gmin)
		return 0;
	hi = ini - ant->largo - ant->gmin;
	lo = (ant->gmax == INF || ant->gmax > ini - ant->largo) ? 0 : ini - ant->largo - ant->gmax;
	for (p = hi + 1; p-- > lo;) {
		if (--m->pasos < 0)
			return 0;
		if (casa_seg(toks + ant->ini, ant->largo, m->b, m->n, p) && atras(pt, k - 1, p, m))
			return 1;
	}
	return 0;
}

/* ¿se puede mirar esa firma en este archivo? */
static int firma_vale(const firma *f, const mirado *m)
{
	if (f->objetivo && !(m->clases & (1u << f->objetivo)))
		return 0;
	if (f->clase == F_LDB) {
		if (f->contenedor && !m->contenedor)
			return 0;
		if (m->total < f->tmin || m->total > f->tmax)
			return 0;
		if (f->epmin || f->epmax != INF) {
			if (m->ep == INF || m->ep < f->epmin || m->ep > f->epmax)
				return 0;
		}
		if (f->smin || f->smax != 0xFFFF) {
			if (!(m->clases & (1u << A_PE)) || m->nsec < f->smin || m->nsec > f->smax)
				return 0;
		}
	}
	return 1;
}

static u32 hallada;			/* la firma encontrada (INF: ninguna) */

/* ¿está ese nombre en .ign2? (sin apuntarlo si no está) */
static int ignorada_texto(const char *s)
{
	u32 n = (u32)strlen(s), h, id, i;
	if (!n_ignorar || !cap_disp)
		return 0;
	h = suma_texto(s, n) & (cap_disp - 1);
	while ((id = disp_nombres[h])) {
		const char *t = arena + nombres[id - 1];
		if (!strcmp(t, s)) {
			for (i = 0; i < n_ignorar; i++)
				if (ignorar[i] == id - 1)
					return 1;
			return 0;
		}
		h = (h + 1) & (cap_disp - 1);
	}
	return 0;
}

static int ignorada(u32 nombre)
{
	u32 i;
	if (nombre >= ID_TEMPORAL && nombre < ID_TEMPORAL + MAX_TEMPORALES)
		return ignorada_texto(tnombres[nombre - ID_TEMPORAL]);
	for (i = 0; i < n_ignorar; i++)
		if (ignorar[i] == nombre)
			return 1;
	return 0;
}

/* un patrón ha casado: la ndb se dice; la ldb cuenta */
static int casado(const patron *pt)
{
	const firma *f = &firmas[pt->firma];
	if (f->clase == F_NDB) {
		if (ignorada(f->nombre))
			return 0;
		hallada = pt->firma;
		return 1;
	}
	{
		u32 c = f->cuentas + pt->sub;
		if (cuentas[c] == 0) {
			u32 i;
			int ya = 0;
			for (i = 0; i < n_tocadas; i++)
				if (tocadas[i] == pt->firma) {
					ya = 1;
					break;
				}
			if (!ya && CRECE(tocadas, cap_tocadas, n_tocadas + 1))
				tocadas[n_tocadas++] = pt->firma;
		}
		if (cuentas[c] < 255)
			cuentas[c]++;
	}
	return 0;
}

/* un patrón de desplazamiento fijo: dónde puede empezar */
static int ventana_fija(const patron *pt, const mirado *m, u32 *lo, u32 *hi)
{
	int64_t base;	/* (con 64 bits: un PE con lo que sea no da la vuelta) */
	switch (pt->o_tipo) {
	case O_ABS: base = pt->o_n; break;
	case O_EOF: base = (int64_t)m->total + pt->o_n; break;
	case O_EP:
		if (m->ep == INF)
			return 0;
		base = (int64_t)m->ep + pt->o_n;
		break;
	case O_SX:
		if (pt->o_sec >= m->nsec)
			return 0;
		base = (int64_t)m->sec[pt->o_sec].raw + pt->o_n;
		break;
	case O_SL:
		if (!m->nsec)
			return 0;
		base = (int64_t)m->sec[m->nsec - 1].raw + pt->o_n;
		break;
	case O_SEX:
		if (pt->o_sec >= m->nsec)
			return 0;
		*lo = m->sec[pt->o_sec].raw;
		*hi = *lo + m->sec[pt->o_sec].rtam;
		if (*hi < *lo)
			*hi = INF;
		return *lo < m->n;
	default: return 0;
	}
	if (base < 0 || base >= m->n)
		return 0;
	*lo = (u32)base;
	*hi = *lo + pt->o_m;
	if (*hi < *lo)
		*hi = INF;
	return *lo < m->n;
}

static int prueba_fijo(const patron *pt, mirado *m)
{
	u32 lo, hi, p;
	const seg *s0 = &segs[pt->seg0];
	if (!ventana_fija(pt, m, &lo, &hi))
		return 0;
	if (hi > m->n - 1)
		hi = m->n - 1;
	for (p = lo; p <= hi; p++) {
		if (casa_seg(toks + s0->ini, s0->largo, m->b, m->n, p) && atras(pt, 0, p, m) &&
		    adelante(pt, 0, p + s0->largo, m)) {
			if (casado(pt))
				return 1;
			if (firmas[pt->firma].clase == F_LDB)
				break;	/* (una vez basta para lo fijo) */
		}
		if (m->pasos < 0)
			break;
	}
	return 0;
}

static int prueba_ancla(const patron *pt, u32 pos, mirado *m)
{
	const seg *s = &segs[pt->seg0 + pt->ancla_seg];
	u32 ini;
	if (pos < pt->ancla_pos)
		return 0;
	ini = pos - pt->ancla_pos;
	if (!casa_seg(toks + s->ini, s->largo, m->b, m->n, ini))
		return 0;
	if (!atras(pt, pt->ancla_seg, ini, m) || !adelante(pt, pt->ancla_seg, ini + s->largo, m))
		return 0;
	return casado(pt);
}

/* evalúa la expresión de una ldb */
static int evalua(const firma *f)
{
	u32 pc[32], pd[32], i;
	u8 pv[32];
	int sp = 0;
	const u32 *x;
	for (x = exprs + f->expr; *x; x++) {
		u32 op = *x & 0xFF000000u;
		if (op == X_SUB) {
			u32 c = cuentas[f->cuentas + (*x & 0xFF)];
			if (sp >= 32)
				return 0;
			pc[sp] = c, pd[sp] = c > 0, pv[sp] = c > 0;
			sp++;
		} else if (op == X_AND || op == X_OR) {
			if (sp < 2)
				return 0;
			sp--;
			pc[sp - 1] += pc[sp];
			pd[sp - 1] += pd[sp];
			pv[sp - 1] = op == X_AND ? (pv[sp - 1] && pv[sp]) : (pv[sp - 1] || pv[sp]);
		} else {
			u32 v = (*x >> 12) & 0xFFF, mm = *x & 0xFFF;
			int r;
			if (sp < 1)
				return 0;
			r = op == X_IGUAL ? pc[sp - 1] == v : op == X_MAYOR ? pc[sp - 1] > v : pc[sp - 1] < v;
			if (mm && pd[sp - 1] < mm)
				r = 0;
			pv[sp - 1] = (u8)r;
			pc[sp - 1] = (u32)r;
			pd[sp - 1] = (u32)r;
		}
	}
	(void)i;
	return sp == 1 && pv[0];
}

/* ---- los disparadores: primero se apuntan las firmas cuyo disparador sale
 * en lo que se mira; luego se leen todas del disco y se buscan todas juntas,
 * en una sola pasada (con una tabla por su primer octeto de ancla) ---- */
#define MAX_CANDIDATAS 384
static void nombre_de(u32 id, int de_esc, char *d);
static u32 memo_ref[1024], memo_gen[1024], gen_actual;
static u32 cand[MAX_CANDIDATAS], n_cand;
u32 motor_disparos, motor_leidas, motor_ms_sumas, motor_ms_disparos, motor_ms_candidatas, motor_bloques;	/* (para medir) los disparadores que han saltado y las firmas leídas del disco */
int motor_depura;
static u32 *cab_t, *sig_t;			/* la tabla de las candidatas: 256 cabezas y la lista */
static u32 cap_sig_t;

static void apunta_candidata(const disp *d)
{
	u32 esc_d = D_ESC(d), r = d->ref ^ esc_d << 30, k = (r * 2654435761u) >> 22;
	motor_disparos++;
#ifdef ESCUDO_ANFITRION
	if (motor_depura > 1)
		pl_apunta("escudo: salta %08x/%06x l%u", d->clave, D_SIGUE(d), D_LARGO(d));
#endif
	if (memo_gen[k] == gen_actual && memo_ref[k] == r)
		return;
	memo_gen[k] = gen_actual, memo_ref[k] = r;
	if (n_cand < MAX_CANDIDATAS)
		cand[n_cand++] = r;
}

static u32 t_cand;
/* lee las candidatas y las busca: 1 si alguna casa (su nombre, a nombre_hallado) */
static int busca_candidatas(mirado *m)
{
	marca mk;
	u32 i, p, f, t0 = n_tocadas, cabe;
	int r = 0;
	static info_firmas falsa;
	info_firmas *antes = inf_actual;
	if (!n_cand)
		return 0;
	t_cand = pl_ms();
	/* (por orden: las del mismo bloque, seguidas, con el bloque ya descomprimido) */
	for (i = 1; i < n_cand; i++) {
		u32 x = cand[i], q = i;
		while (q > 0 && cand[q - 1] > x) {
			cand[q] = cand[q - 1];
			q--;
		}
		cand[q] = x;
	}
	pon_marca(&mk);
	inf_actual = &falsa;
	modo_temporal = 1;
	n_tnombres = 0;
	for (i = 0; i < n_cand; i++) {
		const char *t;
		u32 n;
		int cl = linea_de((int)(cand[i] >> 30), cand[i] & 0x3FFFFFFF, &t, &n);
		motor_leidas++;
#ifdef ESCUDO_ANFITRION
		if (motor_depura)
			pl_apunta("escudo: lee %.*s", (int)(n < 60 ? n : 60), t);
#endif
		if (cl && una_linea(clase_linea(cl), t, n) == -2)
			break;
	}
	modo_temporal = 0;
	inf_actual = antes;
	if (n_cuentas > n_cuentas_cabe)		/* (no caben sus cuentas: no se miran las lógicas de más) */
		goto fuera;
	/* la tabla: por el primer octeto del ancla (de las que se pueden mirar aquí) */
	if (!cab_t && !(cab_t = pl_reserva(257 * 4)))
		goto fuera;
	cabe = n_pats - mk.pats;
	if (!CRECE(sig_t, cap_sig_t, cabe + 1))
		goto fuera;
	for (i = 0; i < 256; i++)
		cab_t[i] = INF;
	m->pasos = 1 << 22;
	for (p = n_pats; p-- > mk.pats;) {
		const patron *pt = &pats[p];
		u32 a;
		if (!firma_vale(&firmas[pt->firma], m))
			continue;
		if (pt->o_tipo != O_CUALQUIERA) {
			if (prueba_fijo(pt, m) && hallada != INF) {
				r = 1;
				goto acaba;
			}
			continue;
		}
		a = fold[toks[segs[pt->seg0 + pt->ancla_seg].ini + pt->ancla_pos].v];
		sig_t[p - mk.pats] = cab_t[a];
		cab_t[a] = p;
	}
	/* una pasada */
	{
		const u8 *b = m->b;
		u32 n = m->n;
		hallada = INF;
		for (i = 0; i + 1 < n; i++) {
			u32 q = cab_t[fold[b[i]]];
			while (q != INF) {
				const patron *pt = &pats[q];
				const tok *a = &toks[segs[pt->seg0 + pt->ancla_seg].ini + pt->ancla_pos];
				if (fold[b[i + 1]] == fold[a[1].v] && (pt->ancla_largo < 3 || (i + 2 < n && fold[b[i + 2]] == fold[a[2].v])) &&
				    prueba_ancla(pt, i, m) && hallada != INF) {
					r = 1;
					goto acaba;
				}
				q = sig_t[q - mk.pats];
			}
			if (m->pasos < 0)
				break;
		}
	}
	/* las lógicas que han visto algo */
	for (i = t0; i < n_tocadas; i++) {
		f = tocadas[i];
		if (f >= mk.firmas && evalua(&firmas[f]) && !ignorada(firmas[f].nombre)) {
			hallada = f;
			r = 1;
			break;
		}
	}
acaba:
	if (r) {
		nombre_de(firmas[hallada].nombre, 0, nombre_hallado);
		hallada = HALLADA_TEMPORAL;
	}
	for (f = mk.firmas; f < n_firmas; f++)
		if (firmas[f].clase == F_LDB && firmas[f].cuentas + firmas[f].nsub <= n_cuentas_cabe)
			memset(cuentas + firmas[f].cuentas, 0, firmas[f].nsub);
fuera:
	n_tocadas = t0;
	vuelve_a(&mk);
	n_cand = 0;
	motor_ms_candidatas += pl_ms() - t_cand;
	return r;
}

/* los disparadores, por todo lo que se mira; luego, las candidatas */
static u32 t_disp;
static int busca_disparos(mirado *m)
{
	const u8 *b = m->b;
	u32 n = m->n, i, j, k, es_pe = (m->clases >> A_PE) & 1;
	if (!n_disps || n < 4)
		return 0;
	gen_actual++;
	n_cand = 0;
	t_disp = pl_ms();
	k = (u32)fold[b[0]] << 8 | (u32)fold[b[1]] << 16 | (u32)fold[b[2]] << 24;
	for (i = 0; i + 3 < n; i++) {
		u32 h;
		k = (k >> 8) | (u32)fold[b[i + 3]] << 24;
		h = h4(k);
		if (!(mapa4[h >> 3] >> (h & 7) & 1))
			continue;
		for (j = cubo4[h >> 7]; j < cubo4[(h >> 7) + 1]; j++) {
			const disp *d = &disps[j];
			if (d->clave != k || ((d->sl & D_SOLO_PE) && !es_pe))
				continue;
			if (D_LARGO(d)) {
				u32 l = D_LARGO(d), q, sg = D_SIGUE(d);
				if (i + 4 + l > n)
					continue;
				for (q = 0; q < l; q++)
					if (fold[b[i + 4 + q]] != ((sg >> (8 * q)) & 0xFF))
						break;
				if (q < l)
					continue;
			}
			apunta_candidata(d);
		}
		/* (de vez en cuando: si ya hay muchas, se miran y se sigue) */
		if (n_cand >= MAX_CANDIDATAS - 8 && busca_candidatas(m))
			return 1;
		if ((i & 0xFFFFF) == 0xFFFFF && pl_cortar())
			break;
	}
	motor_ms_disparos += pl_ms() - t_disp;
	return busca_candidatas(m);
}

/* las firmas del cuerpo en lo que hay en m->b: 1 si hay algo (en «hallada») */
static int busca_cuerpo(mirado *m)
{
	u32 i, j;
	const u8 *b = m->b;
	u32 n = m->n;
	n_tocadas = 0;
	m->pasos = 1 << 22;
	for (i = 0; i < n_fijos; i++) {
		const patron *pt = &pats[fijos[i]];
		if (!firma_vale(&firmas[pt->firma], m))
			continue;
		if (prueba_fijo(pt, m))
			goto fin;
		m->pasos = m->pasos < (1 << 20) ? 1 << 20 : m->pasos;
	}
	if (n >= 3 && n_a3) {
		for (i = 0; i + 2 < n; i++) {
			u32 k = fold[b[i]] | (u32)fold[b[i + 1]] << 8 | (u32)fold[b[i + 2]] << 16, h = h3(k);
			if (!(mapa3[h >> 3] >> (h & 7) & 1))
				continue;
			for (j = cubo3[h >> 4]; j < cubo3[(h >> 4) + 1]; j++) {
				const patron *pt;
				if (anclas3[j].clave != k)
					continue;
				pt = &pats[anclas3[j].pat];
				if (!firma_vale(&firmas[pt->firma], m))
					continue;
				if (prueba_ancla(pt, i, m))
					goto fin;
			}
			if ((i & 0xFFFFF) == 0xFFFFF && pl_cortar())
				break;
		}
	}
	if (n >= 2 && n_a2) {
		for (i = 0; i + 1 < n; i++) {
			u32 k = fold[b[i]] | (u32)fold[b[i + 1]] << 8;
			if (!(mapa2[k >> 3] >> (k & 7) & 1))
				continue;
			for (j = cubo2[k]; j < cubo2[k + 1]; j++) {
				const patron *pt = &pats[anclas2[j].pat];
				if (!firma_vale(&firmas[pt->firma], m))
					continue;
				if (prueba_ancla(pt, i, m))
					goto fin;
			}
		}
	}
	/* las firmas del disco, por sus disparadores */
	if (busca_disparos(m))
		goto fin;
	/* las lógicas que han visto algo */
	for (i = 0; i < n_tocadas; i++) {
		const firma *f = &firmas[tocadas[i]];
		if (evalua(f) && !ignorada(f->nombre)) {
			hallada = tocadas[i];
			goto fin;
		}
	}
	for (i = 0; i < n_tocadas; i++)
		memset(cuentas + firmas[tocadas[i]].cuentas, 0, firmas[tocadas[i]].nsub);
	n_tocadas = 0;
	return 0;
fin:
	for (i = 0; i < n_tocadas; i++)
		memset(cuentas + firmas[tocadas[i]].cuentas, 0, firmas[tocadas[i]].nsub);
	n_tocadas = 0;
	return 1;
}

/* el nombre de una firma (de_esc: 0, de memoria; k + 1, del FIRMAS.ESC k) */
static void nombre_de(u32 id, int de_esc, char *d)
{
	if (id >= ID_TEMPORAL && id < ID_TEMPORAL + MAX_TEMPORALES) {
		formatear(d, MAX_NOMBRE_VIRUS, "%s", tnombres[id - ID_TEMPORAL]);
		return;
	}
	if (!de_esc) {
		formatear(d, MAX_NOMBRE_VIRUS, "%s", id < n_nombres ? arena + nombres[id] : "?");
		return;
	}
	{
		u8 b[4];
		u32 off;
		int n;
		d[0] = 0;
		E = &escs[de_esc - 1];
		if (id >= E->n_nombres)
			return;
		if (pl_mover(E->h, E->idx + id * 4) < 0 || pl_leer(E->h, b, 4) != 4)
			return;
		off = le32(b);
		if (pl_mover(E->h, E->blob + off) < 0)
			return;
		n = pl_leer(E->h, d, MAX_NOMBRE_VIRUS - 1);
		if (n < 0)
			n = 0;
		d[n] = 0;
	}
}

/* ------------------------------------------------- las clases de archivo */
static void mira_clase(mirado *m)
{
	const u8 *b = m->b;
	u32 n = m->n, i;
	m->clases = 1;
	m->ep = INF;
	m->nsec = 0;
	if (n >= 0x40 && b[0] == 'M' && b[1] == 'Z') {
		u32 pe = le32(b + 0x3C);
		if (pe + 24 <= n && pe < n && !memcmp(b + pe, "PE\0\0", 4)) {
			u32 ns = le16(b + pe + 6), so = le16(b + pe + 20), ep, st = pe + 24 + so;
			m->clases |= 1u << A_PE;
			ep = pe + 24 + 16 + 4 <= n ? le32(b + pe + 24 + 16) : 0;
			if (ns > 96)
				ns = 96;
			for (i = 0; i < ns && st + 40 * i + 40 <= n; i++) {
				const u8 *s = b + st + 40 * i;
				m->sec[i].vtam = le32(s + 8);
				m->sec[i].va = le32(s + 12);
				m->sec[i].rtam = le32(s + 16);
				m->sec[i].raw = le32(s + 20);
				m->nsec = i + 1;
			}
			for (i = 0; i < m->nsec; i++) {
				u32 tam = m->sec[i].vtam > m->sec[i].rtam ? m->sec[i].vtam : m->sec[i].rtam;
				if (ep >= m->sec[i].va && ep < m->sec[i].va + tam) {
					m->ep = ep - m->sec[i].va + m->sec[i].raw;
					break;
				}
			}
			if (m->ep == INF && ep < n)
				m->ep = ep;	/* (en la cabecera) */
		}
	}
	if (n >= 4 && b[0] == 0x7F && b[1] == 'E' && b[2] == 'L' && b[3] == 'F')
		m->clases |= 1u << A_ELF;
	if (n >= 4) {
		u32 w = le32(b);
		if (w == 0xFEEDFACEu || w == 0xFEEDFACFu || w == 0xCEFAEDFEu || w == 0xCFFAEDFEu)
			m->clases |= 1u << A_MACHO;
		if (b[0] == 0xCA && b[1] == 0xFE && b[2] == 0xBA && b[3] == 0xBE)
			m->clases |= 1u << A_JAVA | 1u << A_MACHO;
	}
	for (i = 0; i + 5 <= n && i < 1024; i++)
		if (b[i] == '%' && !memcmp(b + i, "%PDF-", 5)) {
			m->clases |= 1u << A_PDF;
			break;
		}
}

/* ------------------------------------------------- lo que se mira */
static const char eicar[] = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*";

typedef struct fuente {
	const u8 *mem;			/* todo en memoria, o... */
	int h;				/* ...en el archivo */
	u32 n;
} fuente;

static int lee_en(const fuente *f, u32 off, void *d, u32 n)
{
	if (off > f->n || n > f->n - off)
		return 0;
	if (f->mem) {
		memcpy(d, f->mem + off, n);
		return 1;
	}
	return pl_mover(f->h, off) >= 0 && pl_leer(f->h, d, n) == (int)n;
}

/* las sumas de un trozo (de la fuente): las que pidan los bits de «tipos» */
typedef struct sumas3 { u8 md5[16], sha1[20], sha256[32]; } sumas3;
static int suma_trozo(const fuente *f, u32 off, u32 n, u32 tipos, int sec, sumas3 *s)
{
	md5_ctx a;
	sha1_ctx b;
	sha256_ctx c;
	int q5 = tipos & (1u << (S_MD5 + sec) | 1u << ((S_MD5 + sec) | S_LIMPIA)) ? 1 : 0;
	int q1 = tipos & (1u << (S_SHA1 + sec) | 1u << ((S_SHA1 + sec) | S_LIMPIA)) ? 1 : 0;
	int q2 = tipos & (1u << (S_SHA256 + sec) | 1u << ((S_SHA256 + sec) | S_LIMPIA)) ? 1 : 0;
	if (q5)
		md5_inicia(&a);
	if (q1)
		sha1_inicia(&b);
	if (q2)
		sha256_inicia(&c);
	if (f->mem) {
		if (off > f->n || n > f->n - off)
			return 0;
		if (q5)
			md5_suma(&a, f->mem + off, n);
		if (q1)
			sha1_suma(&b, f->mem + off, n);
		if (q2)
			sha256_suma(&c, f->mem + off, n);
	} else {
		static u8 t[16384];
		if (pl_mover(f->h, off) < 0)
			return 0;
		while (n) {
			u32 k = n < sizeof t ? n : sizeof t;
			if (pl_leer(f->h, t, k) != (int)k)
				return 0;
			if (q5)
				md5_suma(&a, t, k);
			if (q1)
				sha1_suma(&b, t, k);
			if (q2)
				sha256_suma(&c, t, k);
			n -= k;
		}
	}
	if (q5)
		md5_acaba(&a, s->md5);
	if (q1)
		sha1_acaba(&b, s->sha1);
	if (q2)
		sha256_acaba(&c, s->sha256);
	return 1;
}

/* 1: limpia (una .fp); 2: hallada (r->nombre); 0: nada */
static int mira_sumas(const fuente *f, u32 off, u32 n, int sec, resultado *r)
{
	u32 tipos = tipos_de(n) | tipos_comodin, id, t;
	sumas3 s;
	int k, de_esc;
	u32 base = sec ? S_SEC : 0;
	u32 todos = 0;
	for (k = 1; k <= 3; k++)
		todos |= 1u << (k + base) | 1u << ((k + base) | S_LIMPIA);
	if (!(tipos & todos))
		return 0;
	if (!suma_trozo(f, off, n, tipos, (int)base, &s))
		return 0;
	/* primero las limpias */
	for (k = 0; k < 2; k++)
		for (t = 1; t <= 3; t++) {
			u32 tipo = (t + base) | (k == 0 ? S_LIMPIA : 0);
			const u8 *h = t == 1 ? s.md5 : t == 2 ? s.sha1 : s.sha256;
			if (!(tipos & (1u << tipo)))
				continue;
			id = busca_suma(n, tipo, h, &de_esc);
			if (id == INF)
				continue;
			if (k == 0)
				return 1;
			nombre_de(id, de_esc, r->nombre);
			if (ignorada_texto(r->nombre))
				continue;
			return 2;
		}
	return 0;
}

static int revisa(const fuente *f, const char *nombre, int prof, int contenedor, resultado *r);

/* ---- los ZIP: el directorio central, del final ---- */
typedef struct de_zip { const fuente *f; u32 pos, queda; u8 *sale; u32 n, cabe; } de_zip;
static int zip_lee(void *yo, u8 *d, u32 cabe)
{
	de_zip *z = yo;
	u32 k = cabe < z->queda ? cabe : z->queda;
	if (!k || !lee_en(z->f, z->pos, d, k))
		return 0;
	z->pos += k, z->queda -= k;
	return (int)k;
}
static int zip_saca(void *yo, const u8 *d, u32 n)
{
	de_zip *z = yo;
	if (z->n + n > z->cabe)
		return 1;
	memcpy(z->sale + z->n, d, n);
	z->n += n;
	return pl_cortar();
}

/* contra las «bombas»: lo que se descomprime en total por cada archivo que se
 * revisa (con lo de los ZIP de dentro) y los miembros que se miran */
#define MAX_ZIP_TOTAL (64u << 20)
#define MAX_ZIP_MIEMBROS 1024
static u32 zip_gastado, zip_miembros;

static int revisa_zip(const fuente *f, const char *nombre, int prof, resultado *r)
{
	u8 e[22], c[46], l[30];
	u32 n = f->n, k, cd, ncd, i, pos, j, n_tramos_zip = 0;
	int hecho = 0;
	static u32 ini_z[MAX_PROF + 1][256], fin_z[MAX_PROF + 1][256];	/* lo ya mirado de este ZIP (los solapes, fuera) */
	if (n < 22)
		return 0;
	/* el final del directorio central: «PK\5\6», en los últimos 64 K */
	for (k = n - 22;; k--) {
		if (lee_en(f, k, e, 4) && e[0] == 'P' && e[1] == 'K' && e[2] == 5 && e[3] == 6) {
			if (!lee_en(f, k, e, 22))
				return 0;
			break;
		}
		if (k == 0 || n - k > 65557)
			return 0;
	}
	ncd = le16(e + 10);
	cd = le32(e + 16);
	pos = cd;
	for (i = 0; i < ncd && i < 65535; i++) {
		u32 metodo, flag, csize, usize, nlen, xlen, clen, loff, dat;
		char miembro[100];
		if (!lee_en(f, pos, c, 46) || le32(c) != 0x02014b50)
			break;
		flag = le16(c + 8), metodo = le16(c + 10);
		csize = le32(c + 20), usize = le32(c + 24);
		nlen = le16(c + 28), xlen = le16(c + 30), clen = le16(c + 32);
		loff = le32(c + 42);
		{
			u32 q = nlen < sizeof miembro - 1 ? nlen : sizeof miembro - 1;
			if (!lee_en(f, pos + 46, miembro, q))
				break;
			miembro[q] = 0;
		}
		pos += 46 + nlen + xlen + clen;
		for (j = 0; miembro[j]; j++)	/* (sin letras de control: el nombre va al registro) */
			if ((u8)miembro[j] < 0x20 || miembro[j] == 0x7F)
				miembro[j] = '?';
		if (nlen && miembro[nlen < sizeof miembro - 1 ? nlen - 1 : sizeof miembro - 2] == '/')
			continue;	/* una carpeta */
		if (++zip_miembros > MAX_ZIP_MIEMBROS) {
			r->sin_mirar++;
			break;
		}
		if (flag & 1) {
			r->cifrados++;
			continue;
		}
		if (!lee_en(f, loff, l, 30) || le32(l) != 0x04034b50) {
			r->sin_mirar++;
			continue;
		}
		dat = loff + 30 + le16(l + 26) + le16(l + 28);
		if (usize > motor_max_archivo || (metodo != 0 && metodo != 8) || usize > MAX_ZIP_TOTAL - zip_gastado ||
		    dat < loff || dat > n || csize > n - dat) {
			r->sin_mirar++;
			continue;
		}
		/* (dos miembros que usan los mismos octetos: una bomba; el segundo, no) */
		for (j = 0; j < n_tramos_zip; j++)
			if (loff < fin_z[prof][j] && dat + csize > ini_z[prof][j])
				break;
		if (j < n_tramos_zip) {
			r->sin_mirar++;
			continue;
		}
		if (n_tramos_zip < 256)
			ini_z[prof][n_tramos_zip] = loff, fin_z[prof][n_tramos_zip++] = dat + csize;
		zip_gastado += usize;
		{
			u8 *b = pl_reserva(usize + 1);
			u32 largo = 0;
			if (!b) {
				r->sin_mirar++;
				continue;
			}
			if (metodo == 0) {
				if (csize != usize || !lee_en(f, dat, b, usize)) {
					pl_libera(b);
					r->sin_mirar++;
					continue;
				}
				largo = usize;
			} else {
				static inflar z;
				de_zip dz;
				int e2;
				memset(&z, 0, sizeof z);
				dz.f = f, dz.pos = dat, dz.queda = csize, dz.sale = b, dz.n = 0, dz.cabe = usize;
				z.lee = zip_lee, z.saca = zip_saca, z.yo = &dz, z.tope = usize;
				e2 = inflar_todo(&z);
				if (e2 != INF_BIEN && !(e2 == INF_PARADO && pl_cortar())) {
					pl_libera(b);
					r->sin_mirar++;
					continue;
				}
				largo = dz.n;
			}
			{
				fuente g;
				char dentro[MAX_DENTRO];
				int x;
				g.mem = b, g.h = -1, g.n = largo;
				hecho++;
				x = revisa(&g, miembro, prof + 1, 1, r);
				pl_libera(b);
				if (x) {
					formatear(dentro, sizeof dentro, "%s%s%s", miembro, r->dentro[0] ? " > " : "", r->dentro);
					memcpy(r->dentro, dentro, sizeof dentro);
					return 1;
				}
				if (pl_cortar()) {
					r->cortado = 1;
					return 0;
				}
			}
		}
	}
	(void)nombre;
	return 0;
}

static u8 *buf_prof[MAX_PROF + 2];
static u32 cabe_prof[MAX_PROF + 2];

static int revisa(const fuente *f, const char *nombre, int prof, int contenedor, resultado *r)
{
	mirado m;
	u32 i, t0;
	int x;
	memset(&m, 0, sizeof m);
	m.total = f->n;
	m.contenedor = contenedor;
	r->octetos += f->n;
	/* lo que se mira con los patrones: todo (hasta motor_max_archivo) en memoria */
	if (f->mem) {
		m.b = f->mem;
		m.n = f->n;
	} else {
		u32 n = f->n < motor_max_archivo ? f->n : motor_max_archivo;
		if (f->n > motor_max_archivo)
			r->sin_mirar++;
		if (n + 1 > cabe_prof[prof]) {
			pl_libera(buf_prof[prof]);
			buf_prof[prof] = pl_reserva(n + 1);
			cabe_prof[prof] = buf_prof[prof] ? n + 1 : 0;
		}
		if (!buf_prof[prof] || !lee_en(f, 0, buf_prof[prof], n)) {
			r->sin_mirar++;
			return 0;
		}
		m.b = buf_prof[prof];
		m.n = n;
	}
	/* EICAR, siempre (aunque no haya firmas): el archivo de prueba de los antivirus */
	if (m.n >= 68 && m.n <= 128 && !memcmp(m.b, eicar, 68)) {
		for (i = 68; i < m.n && (m.b[i] == ' ' || m.b[i] == '\t' || m.b[i] == '\r' || m.b[i] == '\n' || m.b[i] == 0x1A); i++) ;
		if (i == m.n) {
			formatear(r->nombre, sizeof r->nombre, "Eicar-Test-Signature");
			r->dentro[0] = 0;
			return 1;
		}
	}
	/* las sumas del archivo entero */
	t0 = pl_ms();
	x = mira_sumas(f, 0, f->n, 0, r);
	motor_ms_sumas += pl_ms() - t0;
	if (x == 1)
		return 0;
	if (x == 2) {
		r->dentro[0] = 0;
		return 1;
	}
	mira_clase(&m);
	/* las de las secciones de un PE */
	t0 = pl_ms();
	if (m.clases & (1u << A_PE))
		for (i = 0; i < m.nsec; i++)
			if (m.sec[i].rtam && m.sec[i].raw < f->n && m.sec[i].rtam <= f->n - m.sec[i].raw &&
			    mira_sumas(f, m.sec[i].raw, m.sec[i].rtam, 1, r) == 2) {
				r->dentro[0] = 0;
				return 1;
			}
	motor_ms_sumas += pl_ms() - t0;
	/* el cuerpo */
	hallada = INF;
	if ((n_pats || n_disps) && busca_cuerpo(&m)) {
		if (hallada == HALLADA_TEMPORAL)
			formatear(r->nombre, sizeof r->nombre, "%s", nombre_hallado);
		else
			nombre_de(firmas[hallada].nombre, 0, r->nombre);
		r->dentro[0] = 0;
		return 1;
	}
	if (pl_cortar()) {
		r->cortado = 1;
		return 0;
	}
	/* dentro de un ZIP */
	if (motor_zip && prof < MAX_PROF && m.n >= 4 && m.b[0] == 'P' && m.b[1] == 'K' && m.b[2] == 3 && m.b[3] == 4) {
		r->dentro[0] = 0;
		if (revisa_zip(f, nombre, prof, r))
			return 1;
	}
	return 0;
}

int motor_revisa_memoria(const u8 *d, u32 n, const char *nombre, resultado *r)
{
	fuente f;
	memset(r, 0, sizeof *r);
	zip_gastado = 0, zip_miembros = 0;
	f.mem = d, f.h = -1, f.n = n;
	r->hallado = revisa(&f, nombre, 0, 0, r);
	return r->hallado;
}

int motor_revisa_archivo(const char *ruta, const char *nombre, resultado *r)
{
	fuente f;
	int h = pl_abrir(ruta);
	memset(r, 0, sizeof *r);
	zip_gastado = 0, zip_miembros = 0;
	if (h < 0)
		return -1;
	f.mem = 0, f.h = h, f.n = pl_largo(h);
	r->hallado = revisa(&f, nombre ? nombre : ruta, 0, 0, r);
	pl_cerrar(h);
	return r->hallado;
}

/* ----------------------------------------------------- FIRMAS.ESC (versión 2)
 * Todo en little endian. La cabecera (128 octetos):
 *    0 «HPESCUD2»       8 fecha (AAAAMMDD)
 *   12 versión de daily, 16 de main, 20 de bytecode
 *   24 sumas            28 dónde (8 octetos cada una: 5 de la suma y 3 del nombre)
 *   32 tramos           36 dónde (12 octetos: tamaño, primera << 8 | tipo, cuántas;
 *                          por tamaño y tipo; los de cualquier tamaño, al final)
 *   40 dónde el mapa de los tamaños (1 << bits bits; los bits, en 96: 21, o 16 si hay pocas)
 *   44 nombres          48 dónde su índice (4 por nombre)   52 dónde los nombres (con 0)
 *   56 bloques          60 dónde su tabla (bloques + 1 sitios: dónde empieza cada uno)
 *   64 disparadores     68 dónde (12 octetos: los 4 fijos, ref = bloque << 16 | sitio,
 *                          y los 3 de detrás | cuántos de esos 3 valen << 24)
 *   72 «siempre»        76 dónde (4 octetos: ref)
 *   80 tramos de cualquier tamaño   84 firmas del cuerpo   88 lógicas   92 limpias
 *   96 los bits del mapa de los tamaños
 * Las sumas de cada tramo van por su suma (se busca por la mitad en el disco).
 * El cuerpo: líneas «n|...» (.ndb), «l|...» (.ldb) e «i|...» (.ign2), en
 * bloques de hasta 64 K, cada uno comprimido (deflate) por su cuenta; una
 * línea no pasa de un bloque a otro. Con «familias», el nombre de una suma
 * pierde su número («Win.Trojan.Agent-123456» queda «Win.Trojan.Agent»).
 * Solo lo hace escudo-anfitrion (con la zlib del sistema, para comprimir). */
static __attribute__((unused)) u32 familia(const char *s, u32 n)
{
	u32 i, vueltas;
	for (vueltas = 0; vueltas < 2; vueltas++) {
		i = n;
		while (i > 0 && s[i - 1] >= '0' && s[i - 1] <= '9')
			i--;
		if (i == n || i == 0 || s[i - 1] != '-')
			break;
		n = i - 1;
	}
	return n;
}

#ifdef ESCUDO_ANFITRION
#include <zlib.h>
/* una firma que irá siempre en memoria: ¿tiene un ancla que diga algo? (la
 * de 3 o 2 octetos de cada patrón: no solo ceros o FF) */
static int buena_en_memoria(const char *t, u32 n, int cl)
{
	marca mk;
	u32 p;
	int buena = 1;
	if (!linea_temporal(clase_linea(cl), t, n, &mk))
		return 0;
	for (p = mk.pats; p < n_pats && buena; p++) {
		const patron *pt = &pats[p];
		const tok *a;
		u32 j, malos = 0;
		if (pt->o_tipo != O_CUALQUIERA)
			continue;	/* (las de sitio fijo solo se miran allí) */
		a = &toks[segs[pt->seg0 + pt->ancla_seg].ini + pt->ancla_pos];
		for (j = 0; j < pt->ancla_largo; j++)
			if (a[j].v == 0 || a[j].v == 0xFF)
				malos++;
		if (malos == pt->ancla_largo)
			buena = 0;
	}
	vuelve_a(&mk);
	return buena;
}
static int escribe(int h, const void *d, u32 n) { return pl_escribir(h, d, n) == (int)n ? 0 : -1; }

/* lo que sale en un archivo limpio (los programas de Windows de un
 * compilador, por ejemplo): los disparadores lo evitan */
void motor_corpus(const u8 *b, u32 n)
{
	u32 i, k;
	if (!limpio4) {
		limpio4 = pl_reserva(1u << LIMPIO_BITS);
		limpio7 = pl_reserva(1u << LIMPIO_BITS);
		if (!limpio4 || !limpio7) {
			pl_libera(limpio4), pl_libera(limpio7);
			limpio4 = limpio7 = 0;
			return;
		}
		memset(limpio4, 0, 1u << LIMPIO_BITS);
		memset(limpio7, 0, 1u << LIMPIO_BITS);
	}
	if (n < 7)
		return;
	if (!fold['A'])
		motor_inicia();
	for (i = 0; i + 7 <= n; i++) {
		u32 x;
		k = fold[b[i]] | (u32)fold[b[i + 1]] << 8 | (u32)fold[b[i + 2]] << 16 | (u32)fold[b[i + 3]] << 24;
		x = 3u << 24 | fold[b[i + 4]] | (u32)fold[b[i + 5]] << 8 | (u32)fold[b[i + 6]] << 16;
		if (limpio4[h_limpio(k, 0)] < 255)
			limpio4[h_limpio(k, 0)]++;
		if (limpio7[h_limpio(k, x)] < 255)
			limpio7[h_limpio(k, x)]++;
	}
}

int motor_escribe_esc(const char *ruta, int familias, u32 fecha)
{
	int h = pl_crear(ruta);
	u8 c[128], *bits;
	u32 i, j, *nuevo_id, *orden_nombres, n_nuevos = 0, blob = 0, n_tramos = 0, n_comodin = 0, pos, nbits;
	u32 *tramos = 0, cap_tramos = 0;
	u32 off_sumas, off_tramos, off_bits, off_idx, off_blob, off_bloques, off_disp, off_siempre, fin_cuerpo;
	u32 n_bl = 0, *bl_off = 0, cap_bl = 0, n_siempre = 0, *siempre = 0, cap_siempre = 0;
	u32 n_d = 0, cap_d = 0, n_cu = 0, n_lo = 0, n_li = 0, sin_disp = 0, largas = 0, debiles = 0;
	disp *ds = 0;
	static u8 bloque[BLOQUE], z[BLOQUE + 4096];
	u32 en_bloque = 0;
	if (h < 0)
		return -1;
	ordena_sumas(sumas, n_sumas);
	for (i = 0; i < n_sumas; i++)
		if (sumas[i].nt & S_LIMPIA)
			n_li++;
	/* los nombres que usan las sumas (por familia si se pide) */
	nuevo_id = pl_reserva(n_nombres * 4 + 4);
	orden_nombres = pl_reserva(n_nombres * 4 + 4);
	nbits = n_sumas > 20000 ? 21 : 16;
	bits = pl_reserva(1u << 18);
	if (!nuevo_id || !orden_nombres || !bits)
		return -2;
	memset(nuevo_id, 0xFF, n_nombres * 4 + 4);
	memset(bits, 0, 1u << 18);
	{
		u32 cd = 1, *dd;
		while (cd < n_nombres * 2 + 16)
			cd *= 2;
		dd = pl_reserva(cd * 4);
		if (!dd)
			return -2;
		memset(dd, 0, cd * 4);
		for (i = 0; i < n_sumas; i++) {
			u32 id = sumas[i].nt >> 8, l, hh, k;
			const char *s;
			if (nuevo_id[id] != INF)
				continue;
			s = arena + nombres[id];
			l = familias ? familia(s, (u32)strlen(s)) : (u32)strlen(s);
			hh = suma_texto(s, l) & (cd - 1);
			while ((k = dd[hh])) {
				const char *t = arena + nombres[orden_nombres[k - 1]];
				u32 lt = familias ? familia(t, (u32)strlen(t)) : (u32)strlen(t);
				if (lt == l && !strncmp(t, s, l))
					break;
				hh = (hh + 1) & (cd - 1);
			}
			if (k)
				nuevo_id[id] = k - 1;
			else {
				orden_nombres[n_nuevos] = id;
				dd[hh] = n_nuevos + 1;
				nuevo_id[id] = n_nuevos++;
				blob += l + 1;
			}
		}
		pl_libera(dd);
		if (n_nuevos > 0xFFFFFF)
			return -2;
	}
	/* los tramos (por tamaño y tipo) */
	for (i = 0; i < n_sumas; i = j) {
		for (j = i; j < n_sumas && sumas[j].tam == sumas[i].tam && (sumas[j].nt & 0x1F) == (sumas[i].nt & 0x1F); j++) ;
		if (!CRECE(tramos, cap_tramos, (n_tramos + 1) * 3))
			return -2;
		tramos[3 * n_tramos] = sumas[i].tam;
		tramos[3 * n_tramos + 1] = i << 8 | (sumas[i].nt & 0x1F);
		tramos[3 * n_tramos + 2] = j - i;
		n_tramos++;
		if (sumas[i].tam == INF)
			n_comodin++;
		else
			bits[bit_tam2(sumas[i].tam, nbits) >> 3] |= (u8)(1u << (bit_tam2(sumas[i].tam, nbits) & 7));
	}
	off_sumas = 128;
	off_tramos = off_sumas + n_sumas * 8;
	off_bits = off_tramos + n_tramos * 12;
	off_idx = off_bits + (1u << (nbits - 3));
	off_blob = off_idx + n_nuevos * 4;
	off_bloques = off_blob + blob;	/* (de momento: la tabla va detrás de los bloques) */
	memset(c, 0, sizeof c);
	escribe(h, c, 128);
	/* las sumas: dentro de cada tramo, por sus 5 primeros octetos (ya lo están: ordena_sumas) */
	for (i = 0; i < n_sumas; i++) {
		u8 e[8];
		u32 id = nuevo_id[sumas[i].nt >> 8];
		memcpy(e, sumas[i].h, 5);
		e[5] = (u8)id, e[6] = (u8)(id >> 8), e[7] = (u8)(id >> 16);
		if (escribe(h, e, 8))
			return -1;
	}
	for (i = 0; i < n_tramos * 3; i++) {
		u8 e[4];
		pon32(e, tramos[i]);
		escribe(h, e, 4);
	}
	escribe(h, bits, 1u << (nbits - 3));
	for (i = 0, pos = 0; i < n_nuevos; i++) {
		u8 e[4];
		const char *s = arena + nombres[orden_nombres[i]];
		pon32(e, pos);
		escribe(h, e, 4);
		pos += (familias ? familia(s, (u32)strlen(s)) : (u32)strlen(s)) + 1;
	}
	for (i = 0; i < n_nuevos; i++) {
		const char *s = arena + nombres[orden_nombres[i]];
		u32 l = familias ? familia(s, (u32)strlen(s)) : (u32)strlen(s);
		escribe(h, s, l);
		escribe(h, "", 1);
	}
	/* antes, cuántas firmas tienen cada ventana (para elegir los disparadores) */
	frec = pl_reserva((1u << FREC_BITS) * 2);
	if (frec) {
		char *p = lineas, *fin = lineas + n_lineas;
		memset(frec, 0, (1u << FREC_BITS) * 2);
		while (p < fin) {
			char *q = p;
			marca mk;
			while (q < fin && *q != '\n')
				q++;
			if (q - p > 2 && p[1] == '|' && (p[0] | 32) != 'i' &&
			    linea_temporal(clase_linea(p[0] | 32), p + 2, (u32)(q - p - 2), &mk)) {
				u32 pp, sg, k, j, l;
				for (pp = mk.pats; pp < n_pats; pp++)
					for (sg = 0; sg < pats[pp].nseg; sg++) {
						const seg *g = &segs[pats[pp].seg0 + sg];
						const tok *t = &toks[g->ini];
						for (k = 0; k + 4 <= g->largo; k++) {
							u32 c, x;
							for (j = 0; j < 4 && es_fijo(&t[k + j]); j++) ;
							if (j < 4)
								continue;
							c = fold[t[k].v] | (u32)fold[t[k + 1].v] << 8 | (u32)fold[t[k + 2].v] << 16 |
							    (u32)fold[t[k + 3].v] << 24;
							for (l = 0, x = 0; l < 3 && k + 4 + l < g->largo && es_fijo(&t[k + 4 + l]); l++)
								x |= (u32)fold[t[k + 4 + l].v] << (8 * l);
							/* (las cuatro largas: 4, 5, 6 y 7) */
							for (j = 0; j <= l; j++) {
								u32 y = (u32)j << 24 | (x & ((1u << (8 * j)) - 1));
								u16 *f = &frec[h_ventana(c, y)];
								if (*f < 65535)
									(*f)++;
							}
						}
					}
				vuelve_a(&mk);
			}
			p = q + 1;
		}
	}
	/* el cuerpo: cada línea, con sus disparadores (o a «siempre») */
	{
		char *p = lineas, *fin = lineas + n_lineas;
		u32 sitio = off_bloques;
		while (p <= fin) {
			char *q = p;
			u32 l;
			int ultimo = p == fin;
			while (q < fin && *q != '\n')
				q++;
			l = (u32)(q - p);
			if (!ultimo && l > 2 && p[1] == '|' && l + 1 > BLOQUE - 16)
				largas++;
			if (ultimo || (l > 2 && p[1] == '|' && l + 1 <= BLOQUE - 16)) {
				if (ultimo || (en_bloque && en_bloque + l + 1 > BLOQUE_OBJETIVO)) {
					/* el bloque, comprimido */
					if (en_bloque) {
						z_stream zs;
						memset(&zs, 0, sizeof zs);
						deflateInit2(&zs, 9, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
						zs.next_in = bloque, zs.avail_in = en_bloque, zs.next_out = z, zs.avail_out = sizeof z;
						deflate(&zs, Z_FINISH);
						if (!CRECE(bl_off, cap_bl, n_bl + 2))
							return -2;
						bl_off[n_bl++] = sitio;
						escribe(h, z, (u32)zs.total_out);
						sitio += (u32)zs.total_out;
						deflateEnd(&zs);
						en_bloque = 0;
					}
					if (ultimo)
						break;
				}
				{
					u32 ref = n_bl << 16 | en_bloque;
					int cl = (u8)p[0] | 32;
					marca mk;
					disp d[64];
					int nd = -1;
					memcpy(bloque + en_bloque, p, l);
					bloque[en_bloque] = (u8)cl;
					bloque[en_bloque + l] = '\n';
					en_bloque += l + 1;
					if (cl == 'n')
						n_cu++;
					else if (cl == 'l')
						n_lo++;
					int solo_pe = 0;
					if (cl != 'i' && linea_temporal(clase_linea(cl), p + 2, l - 2, &mk)) {
						nd = disparadores_de(&mk, d, 64);
						solo_pe = firmas[n_firmas - 1].objetivo == 1;
						vuelve_a(&mk);
					}
					if (nd <= 0 && cl != 'i' && !buena_en_memoria(p + 2, l - 2, cl)) {
						debiles++;	/* (sin nada que la distinga: fuera) */
						en_bloque -= l + 1;
						if (cl == 'n')
							n_cu--;
						else
							n_lo--;
					} else if (nd <= 0) {
						if (!CRECE(siempre, cap_siempre, n_siempre + 1))
							return -2;
						siempre[n_siempre++] = ref;
						if (cl != 'i')
							sin_disp++;
					} else {
						if (!CRECE(ds, cap_d, n_d + (u32)nd))
							return -2;
						for (i = 0; i < (u32)nd; i++) {
							ds[n_d] = d[i];
							if (solo_pe)
								ds[n_d].sl |= D_SOLO_PE;
							ds[n_d++].ref = ref;
						}
					}
				}
			}
			p = q + 1;
		}
		if (!CRECE(bl_off, cap_bl, n_bl + 1))
			return -2;
		bl_off[n_bl] = sitio;
		off_bloques = sitio;
	}
	for (i = 0; i <= n_bl; i++) {
		u8 e[4];
		pon32(e, bl_off[i]);
		escribe(h, e, 4);
	}
	off_disp = off_bloques + (n_bl + 1) * 4;
	for (i = 0; i < n_d; i++) {
		u8 e[12];
		pon32(e, ds[i].clave);
		pon32(e + 4, ds[i].ref);
		pon32(e + 8, ds[i].sl);
		escribe(h, e, 12);
	}
	off_siempre = off_disp + n_d * 12;
	for (i = 0; i < n_siempre; i++) {
		u8 e[4];
		pon32(e, siempre[i]);
		escribe(h, e, 4);
	}
	fin_cuerpo = off_siempre + n_siempre * 4;
	/* la cabecera, ahora que se sabe todo */
	memcpy(c, "HPESCUD2", 8);
	pon32(c + 8, fecha);
	pon32(c + 12, inf_actual ? inf_actual->version_daily : 0);
	pon32(c + 16, inf_actual ? inf_actual->version_main : 0);
	pon32(c + 20, inf_actual ? inf_actual->version_bytecode : 0);
	pon32(c + 24, n_sumas), pon32(c + 28, off_sumas);
	pon32(c + 32, n_tramos), pon32(c + 36, off_tramos);
	pon32(c + 40, off_bits);
	pon32(c + 44, n_nuevos), pon32(c + 48, off_idx), pon32(c + 52, off_blob);
	pon32(c + 56, n_bl), pon32(c + 60, off_bloques);
	pon32(c + 64, n_d), pon32(c + 68, off_disp);
	pon32(c + 72, n_siempre), pon32(c + 76, off_siempre);
	pon32(c + 80, n_comodin), pon32(c + 84, n_cu), pon32(c + 88, n_lo), pon32(c + 92, n_li);
	pon32(c + 96, nbits);
	pl_mover(h, 0);
	escribe(h, c, 128);
	pl_cerrar(h);
	pl_apunta("escudo: FIRMAS.ESC: %u sumas en %u tramos (%u nombres); %u firmas del cuerpo y %u lógicas en %u bloques; %u disparadores; %u siempre en memoria (%u sin 4 octetos fijos que digan algo); %u fuera por débiles (solo ceros, FF o repeticiones); %u líneas demasiado largas; %u octetos",
		  n_sumas, n_tramos, n_nuevos, n_cu, n_lo, n_bl, n_d, n_siempre, sin_disp, debiles, largas, fin_cuerpo);
	pl_libera(nuevo_id), pl_libera(orden_nombres), pl_libera(bits), pl_libera(tramos);
	pl_libera(bl_off), pl_libera(siempre), pl_libera(ds), pl_libera(frec);
	frec = 0;
	return 0;
}
#endif
