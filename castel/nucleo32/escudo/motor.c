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
	if (!c)
		c = 64;
	while (c < n)
		c *= 2;
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

/* el número de un nombre (lo apunta si es nuevo); INF si no hay memoria */
static u32 nombre_id(const char *s, u32 n)
{
	u32 h, id;
	if (n >= MAX_NOMBRE_VIRUS)
		n = MAX_NOMBRE_VIRUS - 1;
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

/* los FIRMAS.ESC: el índice propio (en el disco; solo se lee lo que hace
 * falta). Puede haber varios: el de la base (BASE.ESC, de main.cvd) y el
 * del día (DIA.ESC, de daily.cvd), cada uno de su paquete. */
#define ESC_POR_PAGINA 256
#define MAX_ESC 4
typedef struct esc {
	int h;
	u32 n, off, idx, blob, paginas;
	u32 *pag;			/* el primer tamaño de cada página */
	u8 *bits;			/* 1 << 20 bits: ¿hay alguna con este tamaño? */
	suma *comodin;
	u32 n_comodin;
	suma cache[ESC_POR_PAGINA];
	u32 cache_pag;
} esc;
static esc escs[MAX_ESC];
static int n_escs;
static esc *E;				/* el que se está mirando */
#define esc_h (E->h)
#define esc_n (E->n)
#define esc_off (E->off)
#define esc_idx (E->idx)
#define esc_blob (E->blob)
#define esc_paginas (E->paginas)
#define esc_pag (E->pag)
#define esc_bits (E->bits)
#define esc_comodin (E->comodin)
#define esc_n_comodin (E->n_comodin)
#define esc_cache (E->cache)
#define esc_cache_pag (E->cache_pag)

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

static u32 bit_tam(u32 tam) { return (tam * 2654435761u) >> 12; }

static u32 le32(const u8 *p) { return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static void pon32(u8 *p, u32 v) { p[0] = (u8)v, p[1] = (u8)(v >> 8), p[2] = (u8)(v >> 16), p[3] = (u8)(v >> 24); }

static int esc_pagina(u32 pg)
{
	u32 n, i;
	u8 b[ESC_POR_PAGINA * 16];
	if (pg == esc_cache_pag)
		return 1;
	n = esc_n - pg * ESC_POR_PAGINA;
	if (n > ESC_POR_PAGINA)
		n = ESC_POR_PAGINA;
	if (pl_mover(esc_h, esc_off + pg * ESC_POR_PAGINA * 16) < 0 || pl_leer(esc_h, b, n * 16) != (int)(n * 16))
		return 0;
	for (i = 0; i < n; i++) {
		esc_cache[i].tam = le32(b + 16 * i);
		esc_cache[i].nt = le32(b + 16 * i + 4);
		memcpy(esc_cache[i].h, b + 16 * i + 8, 8);
	}
	for (; i < ESC_POR_PAGINA; i++)
		esc_cache[i].tam = INF, esc_cache[i].nt = 0;
	esc_cache_pag = pg;
	return 1;
}

/* las de ese tamaño en un FIRMAS.ESC (el de E) */
typedef int (*con_suma)(void *yo, const suma *s);
static int recorre_esc(u32 tam, con_suma f, void *yo)
{
	u32 a, b, m, i, pg;
	for (i = 0; i < esc_n_comodin; i++)
		if (f(yo, &esc_comodin[i]))
			return 1;
	if (esc_h < 0 || !esc_paginas || !(esc_bits[bit_tam(tam) >> 3] >> (bit_tam(tam) & 7) & 1))
		return 0;
	a = 0, b = esc_paginas;
	while (a < b) {			/* la primera página que empieza en >= tam */
		m = (a + b) / 2;
		if (esc_pag[m] < tam)
			a = m + 1;
		else
			b = m;
	}
	pg = a ? a - 1 : 0;
	for (; pg < esc_paginas && esc_pag[pg] <= tam; pg++) {
		if (!esc_pagina(pg))
			return 0;
		for (i = 0; i < ESC_POR_PAGINA; i++) {
			if (esc_cache[i].tam > tam)
				return 0;
			if (esc_cache[i].tam == tam && f(yo, &esc_cache[i]))
				return 1;
		}
	}
	return 0;
}

/* recorre las sumas de ese tamaño (en memoria y en los FIRMAS.ESC), dándolas
 * a «f» (con E en el .ESC de la que se da; nulo si es de memoria) */
static int recorre_tam(u32 tam, con_suma f, void *yo)
{
	u32 a = 0, b = n_sumas - n_comodin_ram, m, i;
	int k;
	E = 0;
	while (a < b) {
		m = (a + b) / 2;
		if (sumas[m].tam < tam)
			a = m + 1;
		else
			b = m;
	}
	for (i = a; i < n_sumas - n_comodin_ram && sumas[i].tam == tam; i++)
		if (f(yo, &sumas[i]))
			return 1;
	for (i = n_sumas - n_comodin_ram; i < n_sumas; i++)
		if (f(yo, &sumas[i]))
			return 1;
	for (k = 0; k < n_escs; k++) {
		E = &escs[k];
		if (recorre_esc(tam, f, yo))
			return 1;
	}
	E = 0;
	return 0;
}

static int junta_tipos(void *yo, const suma *s)
{
	*(u32 *)yo |= 1u << (s->nt & 0x1F);
	return 0;
}

/* qué tipos de suma (bits 1 << tipo, con S_LIMPIA) hay para ese tamaño */
static u32 tipos_de(u32 tam)
{
	u32 t = 0;
	recorre_tam(tam, junta_tipos, &t);
	return t;
}

typedef struct busca_s { u32 tipo; const u8 *h; u32 nombre; int de_esc; } busca_s;
static int compara_una(void *yo, const suma *s)
{
	busca_s *b = yo;
	if ((s->nt & 0x1F) == b->tipo && !memcmp(s->h, b->h, 8)) {
		b->nombre = s->nt >> 8;
		/* (las de memoria tienen su nombre en «arena»; las otras, en su FIRMAS.ESC) */
		b->de_esc = E ? (int)(E - escs) + 1 : 0;
		return 1;
	}
	return 0;
}

/* busca la suma (de ese tamaño o de cualquiera): su nombre, o INF */
static u32 busca_suma(u32 tam, u32 tipo, const u8 *h, int *de_esc)
{
	busca_s b;
	b.tipo = tipo, b.h = h, b.nombre = INF, b.de_esc = 0;
	recorre_tam(tam, compara_una, &b);
	*de_esc = b.de_esc;
	return b.nombre;
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

static int patrones_de(u32 fi, u16 sub, const char *hex, u32 nhex, const patron *plantilla, int insens, int ancho, int nivel)
{
	int r = un_patron(fi, sub, hex, nhex, plantilla, insens, ancho);
	const char *a, *c, *q;
	char *nuevo;
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
	if (!motor_guarda_lineas)
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
typedef struct lex { const char *p, *fin; int error; u32 nsub; } lex;
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
		l->p++;
		x_o(l);
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
	l.p = tr[2], l.fin = tr[2] + lr[2], l.error = 0, l.nsub = (u32)(k - 3);
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
	if (l->n && !l->larga && una_linea(l->clase, l->b, l->n) == -2)
		l->sin_memoria = 1;
	l->n = 0, l->larga = 0;
	return l->sin_memoria ? -2 : 0;
}

int motor_carga_texto(const char *nombre, const char *t, u32 n, info_firmas *inf)
{
	inf_actual = inf;
	lec.clase = clase_de(nombre);
	if (!lec.b && !(lec.b = pl_reserva(MAX_LINEA)))
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
	if (!lec.b && !(lec.b = pl_reserva(MAX_LINEA))) {
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

/* FIRMAS.ESC (el formato, en motor_escribe_esc) */
static int carga_esc(const char *ruta, info_firmas *inf)
{
	u8 c[64];
	u32 n_cuerpo, off_cuerpo, off_bits, i;
	int h = pl_abrir(ruta);
	if (h < 0)
		return -1;
	if (pl_leer(h, c, 64) != 64 || memcmp(c, "HPESCUD1", 8)) {
		pl_cerrar(h);
		formatear(inf->error, sizeof inf->error, "%s no es un FIRMAS.ESC", ruta);
		return -1;
	}
	if (n_escs >= MAX_ESC) {
		pl_cerrar(h);
		formatear(inf->error, sizeof inf->error, "demasiados .ESC (%u como mucho)", MAX_ESC);
		return -1;
	}
	E = &escs[n_escs++];
	memset(E, 0, sizeof *E);
	esc_h = h;
	if (le32(c + 8) > inf->fecha)
		inf->fecha = le32(c + 8);
	esc_n = le32(c + 12), esc_off = le32(c + 16);
	esc_idx = le32(c + 24), esc_blob = le32(c + 28);
	off_cuerpo = le32(c + 36), n_cuerpo = le32(c + 40), off_bits = le32(c + 44);
	if (le32(c + 48))
		inf->version_daily = le32(c + 48);
	if (le32(c + 52))
		inf->version_main = le32(c + 52);
	if (le32(c + 56))
		inf->version_bytecode = le32(c + 56);
	esc_n_comodin = le32(c + 60);
	esc_paginas = (esc_n + ESC_POR_PAGINA - 1) / ESC_POR_PAGINA;
	esc_cache_pag = INF;
	esc_pag = pl_reserva(esc_paginas * 4 + 4);
	esc_bits = pl_reserva(1u << 17);
	esc_comodin = pl_reserva(esc_n_comodin * sizeof(suma) + 1);
	if (!esc_pag || !esc_bits || !esc_comodin)
		return -2;
	if (pl_mover(h, off_bits) < 0 || pl_leer(h, esc_bits, 1u << 17) != (1 << 17))
		return -1;
	for (i = 0; i < esc_paginas; i++) {
		u8 b[4];
		if (pl_mover(h, esc_off + i * ESC_POR_PAGINA * 16) < 0 || pl_leer(h, b, 4) != 4)
			return -1;
		esc_pag[i] = le32(b);
	}
	for (i = 0; i < esc_n_comodin; i++) {
		u8 b[16];
		u32 j = esc_n - esc_n_comodin + i;
		if (pl_mover(h, esc_off + j * 16) < 0 || pl_leer(h, b, 16) != 16)
			return -1;
		esc_comodin[i].tam = INF;
		esc_comodin[i].nt = le32(b + 4);
		memcpy(esc_comodin[i].h, b + 8, 8);
		tipos_comodin |= 1u << (le32(b + 4) & 0x1F);
	}
	esc_paginas = (esc_n - esc_n_comodin + ESC_POR_PAGINA - 1) / ESC_POR_PAGINA;
	esc_n -= esc_n_comodin;
	inf->sumas += esc_n + esc_n_comodin;
	/* el cuerpo (texto: «n|línea» de .ndb, «l|» de .ldb, «i|» de .ign2) */
	if (n_cuerpo) {
		char *t = pl_reserva(n_cuerpo + 1), *p, *q;
		int r = 0;
		if (!t)
			return -2;
		if (pl_mover(h, off_cuerpo) < 0 || pl_leer(h, t, n_cuerpo) != (int)n_cuerpo) {
			pl_libera(t);
			return -1;
		}
		t[n_cuerpo] = 0;
		inf_actual = inf;
		for (p = t; *p && r != -2; p = q + 1) {
			q = strchr(p, '\n');
			if (!q)
				q = p + strlen(p);
			if (q - p > 2 && p[1] == '|') {
				int c0 = p[0] | 32;
				int cl = c0 == 'n' ? B_NDB : c0 == 'l' ? B_LDB : c0 == 'i' ? B_IGN2 : B_NADA;
				r = una_linea(cl, p + 2, (u32)(q - p - 2));
			}
			if (!*q)
				break;
		}
		pl_libera(t);
		if (r == -2)
			return -2;
	}
	return 0;
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
	if (!lec.b && !(lec.b = pl_reserva(MAX_LINEA))) {
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
	cuentas = pl_reserva(n_cuentas + 1);
	if (!cuentas)
		n_cuentas = 0;
	else
		memset(cuentas, 0, n_cuentas + 1);
	inf->memoria_k = (n_sumas * sizeof(suma) + arena_n + n_nombres * 4 + cap_disp * 4 + n_toks * sizeof(tok) +
			  n_segs * sizeof(seg) + n_pats * sizeof(patron) + n_firmas * sizeof(firma) + n_conj * 32 +
			  n_altdatos + n_exprs * 4 + (n_a3 + n_a2) * sizeof(ancla) + (1u << (BITS3 - 3)) + (1u << 13) +
			  2 * ((1u << 16) + 1) * 4 + n_escs * (1u << 17) + MAX_LINEA) / 1024;
	pl_apunta("escudo: firmas: %u sumas, %u limpias, %u del cuerpo, %u lógicas (%u patrones: %u con ancla de 3, %u de 2, %u fijos); %u no soportadas, %u PUA sin cargar; %u K en memoria",
		  inf->sumas, inf->limpias, inf->cuerpo, inf->logicas, n_pats, n_a3, n_a2, n_fijos, inf->no_soportadas,
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
		if (esc_h >= 0)
			pl_cerrar(esc_h);
		pl_libera(esc_pag), pl_libera(esc_bits), pl_libera(esc_comodin);
		memset(E, 0, sizeof *E);
	}
	E = 0;
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
	i32 base;
	switch (pt->o_tipo) {
	case O_ABS: base = pt->o_n; break;
	case O_EOF: base = (i32)m->total + pt->o_n; break;
	case O_EP:
		if (m->ep == INF)
			return 0;
		base = (i32)m->ep + pt->o_n;
		break;
	case O_SX:
		if (pt->o_sec >= m->nsec)
			return 0;
		base = (i32)m->sec[pt->o_sec].raw + pt->o_n;
		break;
	case O_SL:
		if (!m->nsec)
			return 0;
		base = (i32)m->sec[m->nsec - 1].raw + pt->o_n;
		break;
	case O_SEX:
		if (pt->o_sec >= m->nsec)
			return 0;
		*lo = m->sec[pt->o_sec].raw;
		*hi = *lo + m->sec[pt->o_sec].rtam;
		return *lo < m->n;
	default: return 0;
	}
	if (base < 0)
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
		if (pl_mover(esc_h, esc_idx + id * 4) < 0 || pl_leer(esc_h, b, 4) != 4)
			return;
		off = le32(b);
		if (pl_mover(esc_h, esc_blob + off) < 0)
			return;
		n = pl_leer(esc_h, d, MAX_NOMBRE_VIRUS - 1);
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

static int revisa_zip(const fuente *f, const char *nombre, int prof, resultado *r)
{
	u8 e[22], c[46], l[30];
	u32 n = f->n, k, cd, ncd, i, pos;
	int hecho = 0;
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
		if (nlen && miembro[nlen < sizeof miembro - 1 ? nlen - 1 : sizeof miembro - 2] == '/')
			continue;	/* una carpeta */
		if (flag & 1) {
			r->cifrados++;
			continue;
		}
		if (!lee_en(f, loff, l, 30) || le32(l) != 0x04034b50) {
			r->sin_mirar++;
			continue;
		}
		dat = loff + 30 + le16(l + 26) + le16(l + 28);
		if (usize > motor_max_archivo || (metodo != 0 && metodo != 8)) {
			r->sin_mirar++;
			continue;
		}
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
	u32 i;
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
	x = mira_sumas(f, 0, f->n, 0, r);
	if (x == 1)
		return 0;
	if (x == 2) {
		r->dentro[0] = 0;
		return 1;
	}
	mira_clase(&m);
	/* las de las secciones de un PE */
	if (m.clases & (1u << A_PE))
		for (i = 0; i < m.nsec; i++)
			if (m.sec[i].rtam && m.sec[i].raw < f->n && m.sec[i].rtam <= f->n - m.sec[i].raw &&
			    mira_sumas(f, m.sec[i].raw, m.sec[i].rtam, 1, r) == 2) {
				r->dentro[0] = 0;
				return 1;
			}
	/* el cuerpo */
	hallada = INF;
	if (n_pats && busca_cuerpo(&m)) {
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
	f.mem = d, f.h = -1, f.n = n;
	r->hallado = revisa(&f, nombre, 0, 0, r);
	return r->hallado;
}

int motor_revisa_archivo(const char *ruta, const char *nombre, resultado *r)
{
	fuente f;
	int h = pl_abrir(ruta);
	memset(r, 0, sizeof *r);
	if (h < 0)
		return -1;
	f.mem = 0, f.h = h, f.n = pl_largo(h);
	r->hallado = revisa(&f, nombre ? nombre : ruta, 0, 0, r);
	pl_cerrar(h);
	return r->hallado;
}

/* ----------------------------------------------------- FIRMAS.ESC
 * Todo en little endian. La cabecera (64 octetos):
 *    0 «HPESCUD1»       8 fecha (AAAAMMDD)     12 sumas (con las comodín)
 *   16 dónde las sumas 20 nombres             24 dónde su índice (4 por nombre)
 *   28 dónde los nombres (texto con 0)        32 lo que miden
 *   36 dónde el cuerpo 40 lo que mide         44 dónde el mapa de tamaños (128 K)
 *   48 versión de daily, 52 de main, 56 de bytecode
 *   60 cuántas sumas son de cualquier tamaño (las últimas)
 * Las sumas: 16 octetos (tamaño, nombre << 8 | tipo, 8 de la suma), por
 * tamaño, tipo y suma. El cuerpo: líneas «n|...» (.ndb), «l|...» (.ldb) e
 * «i|...» (.ign2), tal cual. Con «familias», el nombre de una suma pierde su
 * número («Win.Trojan.Agent-123456-0» se queda en «Win.Trojan.Agent»): son
 * muchas menos, y el aviso dice lo mismo. */
static u32 familia(const char *s, u32 n)
{
	u32 i = n, d1 = 0, d2 = 0;
	while (i > 0 && s[i - 1] >= '0' && s[i - 1] <= '9')
		i--, d1++;
	if (!d1 || i == 0 || s[i - 1] != '-')
		return n;
	i--;
	while (i > 0 && s[i - 1] >= '0' && s[i - 1] <= '9')
		i--, d2++;
	if (!d2 || i == 0 || s[i - 1] != '-')
		return n;
	return i - 1;
}

int motor_escribe_esc(const char *ruta, int familias, u32 fecha, u32 limite, int origen)
{
	int h = pl_crear(ruta);
	u8 c[64];
	u32 i, *nuevo_id, n_nuevos = 0, *orden_nombres, pos, blob = 0;
	u8 *bits;
	char *texto;
	u32 off_sumas = 64, off_bits, off_idx, off_blob, off_cuerpo;
	if (h < 0)
		return -1;
	/* solo lo de main (origen 1) o lo demás (2); y fuera las sumas de los
	 * nombres de .ign2 (los nombres de las sumas pueden quedar sin su número) */
	{
		u32 j, k, q;
		for (j = k = 0; j < n_sumas; j++) {
			int es_main = (sumas[j].nt & S_MAIN) != 0, fuera = 0;
			if ((origen == 1 && !es_main) || (origen == 2 && es_main))
				fuera = 1;
			for (q = 0; !fuera && q < n_ignorar; q++)
				if (ignorar[q] == sumas[j].nt >> 8)
					fuera = 1;
			if (!fuera)
				sumas[k++] = sumas[j];
		}
		n_sumas = k;
		if (origen) {
			char *d = lineas, *p = lineas, *fin = lineas + n_lineas;
			while (p < fin) {
				char *q2 = p;
				int es_main;
				while (q2 < fin && *q2 != '\n')
					q2++;
				es_main = p[0] >= 'A' && p[0] <= 'Z';
				/* (las .ign2 van con las dos) */
				if ((p[0] | 32) == 'i' || (origen == 1) == es_main) {
					u32 l = (u32)(q2 - p) + (q2 < fin ? 1 : 0);
					memmove(d, p, l);
					d[0] = (char)(d[0] | 32);
					d += l;
				}
				p = q2 + 1;
			}
			n_lineas = (u32)(d - lineas);
		}
	}
	/* si no cabe en «limite»: se dejan sumas, primero las de main (de archivo
	 * MD5, SHA-1, SHA-256 y luego las de sección), luego las del día */
	if (limite) {
		static const u32 orden[12] = { S_MAIN | 1, S_MAIN | 2, S_MAIN | 3, S_MAIN | 4, S_MAIN | 5, S_MAIN | 6, 1, 2, 3, 4, 5, 6 };
		u32 fijo = 64 + (1u << 17) + n_lineas, k, j, dejadas = 0;
		u64 total = fijo + (u64)n_sumas * (16 + 28);	/* (unos 28 octetos de nombre por suma, de más) */
		for (k = 0; k < 12 && total > limite; k++)
			for (j = 0; j < n_sumas && total > limite; j++)
				if ((sumas[j].nt & 0x3F) == orden[k] && sumas[j].tam != 0) {
					sumas[j].tam = 0, sumas[j].nt |= 0x40;
					total -= 16 + 28;
					dejadas++;
				}
		for (j = k = 0; j < n_sumas; j++)
			if (!(sumas[j].nt & 0x40))
				sumas[k++] = sumas[j];
		n_sumas = k;
		if (dejadas)
			pl_apunta("escudo: FIRMAS.ESC: no caben en %u K: se dejan %u sumas (primero las de main)", limite / 1024, dejadas);
	}
	ordena_sumas(sumas, n_sumas);
	n_comodin_ram = 0;
	{
		u32 i2;
		for (i2 = n_sumas; i2 > 0 && sumas[i2 - 1].tam == INF; i2--)
			n_comodin_ram++;
	}
	/* los nombres que usan las sumas (reducidos a su familia si se pide) */
	nuevo_id = pl_reserva(n_nombres * 4 + 4);
	orden_nombres = pl_reserva(n_nombres * 4 + 4);
	bits = pl_reserva(1u << 17);
	texto = pl_reserva(MAX_NOMBRE_VIRUS);
	if (!nuevo_id || !orden_nombres || !bits || !texto)
		return -2;
	memset(nuevo_id, 0xFF, n_nombres * 4);
	memset(bits, 0, 1u << 17);
	{
		/* (un segundo juego de nombres: el de las familias) */
		static u32 *disp;
		u32 cd = 1;
		while (cd < n_nombres * 2 + 16)
			cd *= 2;
		disp = pl_reserva(cd * 4);
		if (!disp)
			return -2;
		memset(disp, 0, cd * 4);
		for (i = 0; i < n_sumas; i++) {
			u32 id = sumas[i].nt >> 8, l, hh, k;
			const char *s;
			if (nuevo_id[id] != INF)
				continue;
			s = arena + nombres[id];
			l = familias ? familia(s, (u32)strlen(s)) : (u32)strlen(s);
			hh = suma_texto(s, l) & (cd - 1);
			while ((k = disp[hh])) {
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
				disp[hh] = n_nuevos + 1;
				nuevo_id[id] = n_nuevos++;
				blob += l + 1;
			}
		}
		pl_libera(disp);
	}
	off_bits = off_sumas + n_sumas * 16;
	off_idx = off_bits + (1u << 17);
	off_blob = off_idx + n_nuevos * 4;
	off_cuerpo = off_blob + blob;
	memset(c, 0, sizeof c);
	memcpy(c, "HPESCUD1", 8);
	pon32(c + 8, fecha);
	pon32(c + 12, n_sumas);
	pon32(c + 16, off_sumas);
	pon32(c + 20, n_nuevos);
	pon32(c + 24, off_idx);
	pon32(c + 28, off_blob);
	pon32(c + 32, blob);
	pon32(c + 36, off_cuerpo);
	pon32(c + 40, n_lineas);
	pon32(c + 44, off_bits);
	pon32(c + 48, inf_actual ? inf_actual->version_daily : 0);
	pon32(c + 52, inf_actual ? inf_actual->version_main : 0);
	pon32(c + 56, inf_actual ? inf_actual->version_bytecode : 0);
	pon32(c + 60, n_comodin_ram);
	pl_escribir(h, c, 64);
	for (i = 0; i < n_sumas; i++) {
		u8 e[16];
		pon32(e, sumas[i].tam);
		pon32(e + 4, nuevo_id[sumas[i].nt >> 8] << 8 | (sumas[i].nt & 0x1F));
		memcpy(e + 8, sumas[i].h, 8);
		if (sumas[i].tam != INF)
			bits[bit_tam(sumas[i].tam) >> 3] |= (u8)(1u << (bit_tam(sumas[i].tam) & 7));
		if (pl_escribir(h, e, 16) != 16) {
			pl_cerrar(h);
			return -1;
		}
	}
	pl_escribir(h, bits, 1u << 17);
	for (i = 0, pos = 0; i < n_nuevos; i++) {
		u8 e[4];
		const char *s = arena + nombres[orden_nombres[i]];
		pon32(e, pos);
		pl_escribir(h, e, 4);
		pos += (familias ? familia(s, (u32)strlen(s)) : (u32)strlen(s)) + 1;
	}
	for (i = 0; i < n_nuevos; i++) {
		const char *s = arena + nombres[orden_nombres[i]];
		u32 l = familias ? familia(s, (u32)strlen(s)) : (u32)strlen(s);
		pl_escribir(h, s, l);
		pl_escribir(h, "", 1);
	}
	if (n_lineas)
		pl_escribir(h, lineas, n_lineas);
	pl_cerrar(h);
	pl_libera(nuevo_id), pl_libera(orden_nombres), pl_libera(bits), pl_libera(texto);
	pl_apunta("escudo: FIRMAS.ESC: %u sumas (%u nombres), %u octetos de cuerpo, %u octetos en total", n_sumas,
		  n_nuevos, n_lineas, off_cuerpo + n_lineas);
	return 0;
}
