/* formula.c - Cuentas 2: leer las fórmulas a sus piezas (en notación
 * polaca inversa), escribirlas de vuelta en cualquiera de las formas
 * (español, inglés, .XLSX, .ODS), copiarlas corriendo las referencias y
 * arreglarlas al meter o quitar filas y columnas (formula.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "formula.h"
#include "numeros.h"

#define HONDO 160		/* paréntesis y funciones metidos unos en otros, como mucho */
#define MAX_PIEZAS 8192

typedef struct {
	libro *l;
	const hoja *h;
	const cu_local *loc;
	const u8 *s;
	u32 n, i;
	cu_tok *t;
	u32 nt, cabe_t;
	bufer txt;
	int hondo;
	int mal;
	char *err;
	u32 cabe_err;
	u32 donde;
	int volatil;
	int nombres;
} lector;

static void falla(lector *L, const char *m)
{
	if (L->mal)
		return;
	L->mal = 1;
	L->donde = L->i;
	if (L->err)
		copia(L->err, m, L->cabe_err);
}

static cu_tok *pieza(lector *L, int t)
{
	cu_tok *k;

	if (L->nt >= L->cabe_t) {
		u32 c = L->cabe_t ? L->cabe_t * 2 : 16;
		cu_tok *n;

		if (c > MAX_PIEZAS) {
			falla(L, "La fórmula es demasiado larga");
			return NULL;
		}
		n = so_repide(L->t, c * sizeof(cu_tok));
		if (!n) {
			falla(L, "No hay memoria");
			return NULL;
		}
		L->t = n;
		L->cabe_t = c;
	}
	k = &L->t[L->nt++];
	memset(k, 0, sizeof *k);
	k->t = (u8)t;
	return k;
}

static u32 mete_txt(lector *L, const char *s, u32 n)
{
	u32 o = L->txt.n;

	buf_pon(&L->txt, s, n);
	buf_c(&L->txt, 0);
	if (L->txt.mal)
		falla(L, "No hay memoria");
	return o;
}

static void blancos(lector *L)
{
	while (L->i < L->n && (L->s[L->i] == ' ' || L->s[L->i] == '\t' || L->s[L->i] == '\n' || L->s[L->i] == '\r'))
		L->i++;
}

static int mira(lector *L)
{
	blancos(L);
	return L->i < L->n ? L->s[L->i] : 0;
}

/* ¿es letra de un nombre? (las de UTF-8 de dos o más octetos valen todas) */
static int de_nombre(u8 c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '\\' || c >= 0x80;
}

/* ---- las referencias ---- */
/* una columna de letras (A..IV) en s; da cuántas letras (0 si no) */
static int lee_col(const u8 *s, u32 n, u32 *c)
{
	u32 k = 0, v = 0;

	while (k < n && k < 3 && ((s[k] >= 'A' && s[k] <= 'Z') || (s[k] >= 'a' && s[k] <= 'z'))) {
		v = v * 26 + (u32)((s[k] | 0x20) - 'a' + 1);
		k++;
	}
	if (!k || v == 0 || v > CU_COLS)
		return 0;
	*c = v - 1;
	return (int)k;
}

static int lee_fila(const u8 *s, u32 n, u32 *f)
{
	u32 k = 0, v = 0;

	while (k < n && s[k] >= '0' && s[k] <= '9') {
		v = v * 10 + (u32)(s[k] - '0');
		if (v > CU_FILAS)
			return 0;
		k++;
	}
	if (!k || v == 0)
		return 0;
	*f = v - 1;
	return (int)k;
}

/* una celda ($A$1) al principio de s; da lo que ocupa y *a sus «$» (RA_C0 | RA_F0) */
static int lee_celda(const u8 *s, u32 n, u32 *f, u32 *c, int *a)
{
	u32 i = 0;
	int k;

	*a = 0;
	if (i < n && s[i] == '$') {
		*a |= RA_C0;
		i++;
	}
	k = lee_col(s + i, n - i, c);
	if (!k)
		return 0;
	i += (u32)k;
	if (i < n && s[i] == '$') {
		*a |= RA_F0;
		i++;
	}
	k = lee_fila(s + i, n - i, f);
	if (!k)
		return 0;
	i += (u32)k;
	return (int)i;
}

/* columna sola ($A) o fila sola ($3), para A:C y 3:7 */
static int lee_col_sola(const u8 *s, u32 n, u32 *c, int *fija)
{
	u32 i = 0;
	int k;

	*fija = 0;
	if (i < n && s[i] == '$') {
		*fija = 1;
		i++;
	}
	k = lee_col(s + i, n - i, c);
	return k ? (int)i + k : 0;
}

static int lee_fila_sola(const u8 *s, u32 n, u32 *f, int *fija)
{
	u32 i = 0;
	int k;

	*fija = 0;
	if (i < n && s[i] == '$') {
		*fija = 1;
		i++;
	}
	k = lee_fila(s + i, n - i, f);
	return k ? (int)i + k : 0;
}

int cu_lee_ref(const char *s, u32 *f, u32 *c, const char **fin)
{
	int a, k = lee_celda((const u8 *)s, largo(s), f, c, &a);

	if (!k)
		return 0;
	if (fin)
		*fin = s + k;
	return 1;
}

int cu_lee_rango(const char *s, rango *r, const char **fin)
{
	u32 f0, c0, f1, c1, n = largo(s);
	int a, k = lee_celda((const u8 *)s, n, &f0, &c0, &a), fija;

	if (k) {
		if ((u32)k < n && s[k] == ':') {
			int k2 = lee_celda((const u8 *)s + k + 1, n - (u32)k - 1, &f1, &c1, &a);

			if (k2) {
				*r = cu_rango(f0, c0, f1, c1);
				if (fin)
					*fin = s + k + 1 + k2;
				return 1;
			}
		}
		*r = cu_rango(f0, c0, f0, c0);
		if (fin)
			*fin = s + k;
		return 1;
	}
	k = lee_col_sola((const u8 *)s, n, &c0, &fija);
	if (k && (u32)k < n && s[k] == ':') {
		int k2 = lee_col_sola((const u8 *)s + k + 1, n - (u32)k - 1, &c1, &fija);

		if (k2 && !((u32)(k + 1 + k2) < n && s[k + 1 + k2] >= '0' && s[k + 1 + k2] <= '9')) {
			*r = cu_rango(0, c0, CU_FILAS - 1, c1);
			if (fin)
				*fin = s + k + 1 + k2;
			return 1;
		}
	}
	k = lee_fila_sola((const u8 *)s, n, &f0, &fija);
	if (k && (u32)k < n && s[k] == ':') {
		int k2 = lee_fila_sola((const u8 *)s + k + 1, n - (u32)k - 1, &f1, &fija);

		if (k2) {
			*r = cu_rango(f0, 0, f1, CU_COLS - 1);
			if (fin)
				*fin = s + k + 1 + k2;
			return 1;
		}
	}
	return 0;
}

/* ---- leer ---- */
static void expresion(lector *L);

/* una hoja por su nombre (n octetos): su id, o -1 */
static int id_hoja(lector *L, const char *s, u32 n)
{
	hoja *h = lib_hoja_nombre(L->l, s, n);

	return h ? h->id : -1;
}

/* lo de detrás de «Hoja!» o una referencia sin hoja: la celda, el rango (A1:B2, A:C, 3:7) */
static int referencia(lector *L, int hid, int con_hoja)
{
	const u8 *s = L->s + L->i;
	u32 n = L->n - L->i, f0, c0, f1, c1;
	int a0, a1, k, k2, fija, fija2;
	cu_tok *t;

	k = lee_celda(s, n, &f0, &c0, &a0);
	if (k && !((u32)k < n && de_nombre(s[k]) && s[k] != '.') && !((u32)k < n && s[k] == '(')) {
		if ((u32)k < n && s[k] == ':') {
			k2 = lee_celda(s + k + 1, n - (u32)k - 1, &f1, &c1, &a1);
			if (k2 && !((u32)(k + 1 + k2) < n && de_nombre(s[k + 1 + k2]) && s[k + 1 + k2] != '.')) {
				rango r = cu_rango(f0, c0, f1, c1);

				t = pieza(L, T_RANGO);
				if (!t)
					return 1;
				/* (los «$» siguen a su esquina, aunque se den la vuelta) */
				t->a = (u8)((f0 <= f1 ? (a0 & RA_F0) | (a1 & RA_F0) << 2 : (a1 & RA_F0) | (a0 & RA_F0) << 2) |
					    (c0 <= c1 ? (a0 & RA_C0) | (a1 & RA_C0) << 2 : (a1 & RA_C0) | (a0 & RA_C0) << 2));
				t->u.r.f0 = (i32)r.f0;
				t->u.r.f1 = (i32)r.f1;
				t->u.r.c0 = r.c0;
				t->u.r.c1 = r.c1;
				t->h = (u16)hid;
				if (con_hoja)
					t->a |= RA_HOJA;
				L->i += (u32)(k + 1 + k2);
				return 1;
			}
		}
		t = pieza(L, T_REF);
		if (!t)
			return 1;
		t->a = (u8)a0;
		t->u.r.f0 = t->u.r.f1 = (i32)f0;
		t->u.r.c0 = t->u.r.c1 = (u16)c0;
		t->h = (u16)hid;
		if (con_hoja)
			t->a |= RA_HOJA;
		L->i += (u32)k;
		return 1;
	}
	/* A:C */
	k = lee_col_sola(s, n, &c0, &fija);
	if (k && (u32)k < n && s[k] == ':') {
		k2 = lee_col_sola(s + k + 1, n - (u32)k - 1, &c1, &fija2);
		if (k2 && !((u32)(k + 1 + k2) < n && de_nombre(s[k + 1 + k2]))) {
			t = pieza(L, T_RANGO);
			if (!t)
				return 1;
			t->a = RA_COLS | RA_F0 | RA_F1 | (c0 <= c1 ? (fija ? RA_C0 : 0) | (fija2 ? RA_C1 : 0) : (fija2 ? RA_C0 : 0) | (fija ? RA_C1 : 0));
			t->u.r.f0 = 0;
			t->u.r.f1 = (i32)CU_FILAS - 1;
			t->u.r.c0 = (u16)(c0 < c1 ? c0 : c1);
			t->u.r.c1 = (u16)(c0 < c1 ? c1 : c0);
			t->h = (u16)hid;
			if (con_hoja)
				t->a |= RA_HOJA;
			L->i += (u32)(k + 1 + k2);
			return 1;
		}
	}
	/* 3:7 */
	k = lee_fila_sola(s, n, &f0, &fija);
	if (k && (u32)k < n && s[k] == ':') {
		k2 = lee_fila_sola(s + k + 1, n - (u32)k - 1, &f1, &fija2);
		if (k2 && !((u32)(k + 1 + k2) < n && (de_nombre(s[k + 1 + k2]) || s[k + 1 + k2] == L->loc->dec))) {
			t = pieza(L, T_RANGO);
			if (!t)
				return 1;
			t->a = RA_FILAS | RA_C0 | RA_C1 | (f0 <= f1 ? (fija ? RA_F0 : 0) | (fija2 ? RA_F1 : 0) : (fija2 ? RA_F0 : 0) | (fija ? RA_F1 : 0));
			t->u.r.f0 = (i32)(f0 < f1 ? f0 : f1);
			t->u.r.f1 = (i32)(f0 < f1 ? f1 : f0);
			t->u.r.c0 = 0;
			t->u.r.c1 = CU_COLS - 1;
			t->h = (u16)hid;
			if (con_hoja)
				t->a |= RA_HOJA;
			L->i += (u32)(k + 1 + k2);
			return 1;
		}
	}
	return 0;
}

/* (ODS) [.A1], [.A1:.B2], [$Hoja2.A1:.B3], ['Mi hoja'.A1], [.A:.C], [.3:.7] */
static void ref_ods(lector *L)
{
	u32 fin = L->i, k;
	char nom[200];
	u32 nn = 0;
	int hid = L->h ? L->h->id : 0, con = 0;
	lector M;
	bufer b = { 0 };

	/* hasta el «]» (fuera de comillas) */
	{
		int com = 0;

		while (fin < L->n && (com || L->s[fin] != ']')) {
			if (L->s[fin] == '\'')
				com = !com;
			fin++;
		}
	}
	if (fin >= L->n) {
		falla(L, "Falta «]»");
		return;
	}
	/* se reescribe a la forma de siempre (Hoja!A1:B2) y se lee con ella */
	k = L->i;
	while (k < fin) {
		u8 c = L->s[k];

		if (c == '$' && k + 1 < fin && (L->s[k + 1] == '\'' || (L->s[k + 1] != '.' && !(k > L->i && L->s[k - 1] == '.')))) {
			/* ($ delante de la hoja: se quita) */
			int es_hoja = 0;
			u32 j = k + 1;

			if (L->s[j] == '\'')
				es_hoja = 1;
			else {
				while (j < fin && L->s[j] != '.' && L->s[j] != ':')
					j++;
				if (j < fin && L->s[j] == '.')
					es_hoja = 1;
			}
			if (es_hoja) {
				k++;
				continue;
			}
		}
		if (c == '\'') {
			/* un nombre de hoja entre comillas */
			k++;
			nn = 0;
			while (k < fin) {
				if (L->s[k] == '\'' && k + 1 < fin && L->s[k + 1] == '\'') {
					if (nn + 1 < sizeof nom)
						nom[nn++] = '\'';
					k += 2;
					continue;
				}
				if (L->s[k] == '\'')
					break;
				if (nn + 1 < sizeof nom)
					nom[nn++] = (char)L->s[k];
				k++;
			}
			k++;
			nom[nn] = 0;
			if (!con) {
				hid = id_hoja(L, nom, nn);
				con = 1;
			}
			continue;
		}
		if (c == '.') {
			k++;
			continue;
		}
		if (c == ':' ) {
			buf_c(&b, ':');
			k++;
			continue;
		}
		/* un nombre de hoja sin comillas: lo de antes del «.» */
		{
			u32 j = k;

			while (j < fin && L->s[j] != '.' && L->s[j] != ':')
				j++;
			if (j < fin && L->s[j] == '.') {
				if (!con) {
					hid = id_hoja(L, (const char *)L->s + k, j - k);
					con = 1;
				}
				k = j;
				continue;
			}
			buf_pon(&b, L->s + k, j - k);
			k = j;
		}
	}
	L->i = fin + 1;
	if (con && hid < 0) {
		cu_tok *t = pieza(L, T_ERR);

		if (t)
			t->u.x.i = ER_REF;
		buf_suelta(&b);
		return;
	}
	M = *L;
	M.s = b.d ? b.d : (const u8 *)"";
	M.n = b.n;
	M.i = 0;
	if (!referencia(&M, hid, con) || M.i != M.n) {
		/* ([.#REF!] y lo que no se entiende) */
		cu_tok *t;

		L->t = M.t;
		L->nt = M.nt;
		L->cabe_t = M.cabe_t;
		t = pieza(L, T_ERR);
		if (t)
			t->u.x.i = ER_REF;
		buf_suelta(&b);
		return;
	}
	L->t = M.t;
	L->nt = M.nt;
	L->cabe_t = M.cabe_t;
	L->mal |= M.mal;
	buf_suelta(&b);
}

static int sin_caja_n(const u8 *a, u32 n, const char *b)
{
	u32 k;

	for (k = 0; k < n; k++) {
		u8 x = a[k], y = (u8)b[k];

		if (!y)
			return 0;
		if (x >= 'a' && x <= 'z')
			x -= 32;
		if (y >= 'a' && y <= 'z')
			y -= 32;
		if (x != y)
			return 0;
	}
	return b[n] == 0;
}

/* un número escrito en la fórmula (con el decimal de loc) */
static int numero_formula(lector *L)
{
	const u8 *s = L->s + L->i;
	u32 n = L->n - L->i, k = 0;
	char d[80];
	u32 m = 0;
	int hay = 0;
	cu_tok *t;

	while (k < n && s[k] >= '0' && s[k] <= '9' && m < 70) {
		d[m++] = (char)s[k++];
		hay = 1;
	}
	if (k < n && s[k] == (u8)L->loc->dec && m < 70) {
		d[m++] = '.';
		k++;
		while (k < n && s[k] >= '0' && s[k] <= '9' && m < 70) {
			d[m++] = (char)s[k++];
			hay = 1;
		}
	}
	if (!hay)
		return 0;
	if (k < n && (s[k] == 'e' || s[k] == 'E')) {
		u32 j = k + 1;

		if (j < n && (s[j] == '+' || s[j] == '-'))
			j++;
		if (j < n && s[j] >= '0' && s[j] <= '9') {
			d[m++] = 'e';
			k++;
			if (s[k] == '+' || s[k] == '-')
				d[m++] = (char)s[k++];
			while (k < n && s[k] >= '0' && s[k] <= '9' && m < 78)
				d[m++] = (char)s[k++];
		}
	}
	d[m] = 0;
	t = pieza(L, T_NUM);
	if (!t)
		return 1;
	t->u.n = num_de_texto_c(d);
	L->i += k;
	return 1;
}

static void argumentos(lector *L, int fid, const char *nombre, u32 nn)
{
	u32 na = 0;
	cu_tok *t;

	L->i++;			/* ( */
	if (mira(L) == ')') {
		L->i++;
	} else
		for (;;) {
			int c = mira(L);

			if (c == L->loc->sep || c == ')' || (c == ';' && !L->loc->ods && L->loc->sep == ';')) {
				pieza(L, T_FALTA);
			} else
				expresion(L);
			if (L->mal)
				return;
			na++;
			c = mira(L);
			if (c == L->loc->sep || (c == ';' && L->loc->sep == ';')) {
				L->i++;
				if (na >= 255) {
					falla(L, "Demasiados argumentos");
					return;
				}
				continue;
			}
			if (c == ')') {
				L->i++;
				break;
			}
			falla(L, L->loc->sep == ';' ? "Falta «)» o «;»" : "Falta «)» o «,»");
			return;
		}
	if (fid >= 0) {
		const cu_funcion *f = &CU_FUNCIONES[fid];

		if (na < f->min || (f->max != 255 && na > f->max)) {
			char m[160];

			fmt(m, sizeof m, "%s: %s", L->loc->ingles ? f->en : f->es, na < f->min ? "faltan argumentos" : "sobran argumentos");
			falla(L, m);
			return;
		}
		if (f->opc & FN_VOLATIL)
			L->volatil = 1;
	}
	t = pieza(L, T_FUNC);
	if (!t)
		return;
	t->h = (u16)(fid >= 0 ? fid : 0xFFFF);
	t->u.x.n = na;
	if (fid < 0) {
		/* (una que no se conoce: #¿NOMBRE? al calcular; su nombre, para escribirla) */
		u32 o = mete_txt(L, nombre, nn);

		t = &L->t[L->nt - 1];
		t->u.x.i = o;
		t->u.x.n = na;
	}
}

static void primario(lector *L)
{
	int c = mira(L);

	if (L->mal)
		return;
	if (++L->hondo > HONDO) {
		falla(L, "La fórmula tiene demasiados paréntesis o funciones unos dentro de otros");
		return;
	}
	if (c == '(') {
		L->i++;
		expresion(L);
		if (L->mal)
			return;
		if (mira(L) != ')') {
			falla(L, "Falta «)»");
			return;
		}
		L->i++;
		pieza(L, T_PAREN);
	} else if (c == '"') {
		bufer b = { 0 };
		cu_tok *t;

		L->i++;
		for (;;) {
			if (L->i >= L->n) {
				buf_suelta(&b);
				falla(L, "Falta cerrar las comillas");
				return;
			}
			if (L->s[L->i] == '"') {
				if (L->i + 1 < L->n && L->s[L->i + 1] == '"') {
					buf_c(&b, '"');
					L->i += 2;
					continue;
				}
				L->i++;
				break;
			}
			buf_c(&b, L->s[L->i++]);
		}
		t = pieza(L, T_TXT);
		if (t) {
			u32 o = mete_txt(L, b.d ? (char *)b.d : "", b.n);

			t = &L->t[L->nt - 1];
			t->u.x.i = o;
			t->u.x.n = b.n;
		}
		buf_suelta(&b);
	} else if (c == '#') {
		u32 lg;
		int e = cu_error_de((const char *)L->s + L->i, &lg);
		cu_tok *t;

		if (!e || L->i + lg > L->n) {
			falla(L, "Ese error no existe");
			return;
		}
		t = pieza(L, T_ERR);
		if (t)
			t->u.x.i = (u32)e;
		L->i += lg;
	} else if (c == '[' && L->loc->ods) {
		L->i++;
		ref_ods(L);
	} else if (c == '\'') {
		/* 'Mi hoja'!A1 */
		char nom[200];
		u32 nn = 0;
		int hid;

		L->i++;
		for (;;) {
			if (L->i >= L->n) {
				falla(L, "Falta cerrar el «'» del nombre de la hoja");
				return;
			}
			if (L->s[L->i] == '\'') {
				if (L->i + 1 < L->n && L->s[L->i + 1] == '\'') {
					if (nn + 1 < sizeof nom)
						nom[nn++] = '\'';
					L->i += 2;
					continue;
				}
				L->i++;
				break;
			}
			if (nn + 1 < sizeof nom)
				nom[nn++] = (char)L->s[L->i];
			L->i++;
		}
		nom[nn] = 0;
		if (L->i >= L->n || L->s[L->i] != '!') {
			falla(L, "Falta «!» detrás del nombre de la hoja");
			return;
		}
		L->i++;
		hid = id_hoja(L, nom, nn);
		if (hid < 0) {
			/* (una hoja que no está: #¡REF!, pero se lee lo de detrás) */
			u32 i0 = L->i, nt0 = L->nt;

			if (!referencia(L, 0, 1)) {
				falla(L, "Falta la referencia detrás de la hoja");
				return;
			}
			(void)i0;
			L->nt = nt0;
			{
				cu_tok *t = pieza(L, T_ERR);

				if (t)
					t->u.x.i = ER_REF;
			}
			return;
		}
		if (!referencia(L, hid, 1))
			falla(L, "Falta la referencia detrás de la hoja");
	} else if ((c >= '0' && c <= '9') || c == L->loc->dec) {
		/* (3:7, filas enteras, antes que un número) */
		if (c >= '0' && c <= '9' && referencia(L, L->h ? L->h->id : 0, 0))
			;
		else if (!numero_formula(L))
			falla(L, "Número mal escrito");
	} else if (de_nombre((u8)c) || c == '$') {
		/* una celda, un rango, una función, VERDADERO, un nombre o Hoja!A1 */
		u32 i0 = L->i, j = L->i;
		const u8 *s;
		u32 nn;

		if (c != '$' && referencia(L, L->h ? L->h->id : 0, 0)) {
			/* (salvo si es el nombre de una hoja: «A1!B2») */
			if (mira(L) != '!')
				return;
			L->nt--;
			L->i = i0;
		} else if (c == '$') {
			if (referencia(L, L->h ? L->h->id : 0, 0))
				return;
			falla(L, "Referencia mal escrita");
			return;
		}
		while (j < L->n && de_nombre(L->s[j]))
			j++;
		s = L->s + i0;
		nn = j - i0;
		L->i = j;
		if (j < L->n && L->s[j] == '!') {
			int hid = id_hoja(L, (const char *)s, nn);

			L->i++;
			if (hid < 0) {
				u32 nt0 = L->nt;

				if (!referencia(L, 0, 1)) {
					falla(L, "Falta la referencia detrás de la hoja");
					return;
				}
				L->nt = nt0;
				{
					cu_tok *t = pieza(L, T_ERR);

					if (t)
						t->u.x.i = ER_REF;
				}
				return;
			}
			if (!referencia(L, hid, 1))
				falla(L, "Falta la referencia detrás de la hoja");
			return;
		}
		if (mira(L) == '(') {
			int fid = fn_busca((const char *)s, nn);

			argumentos(L, fid, (const char *)s, nn);
			return;
		}
		if (sin_caja_n(s, nn, "VERDADERO") || sin_caja_n(s, nn, "TRUE") || sin_caja_n(s, nn, "FALSO") || sin_caja_n(s, nn, "FALSE")) {
			cu_tok *t = pieza(L, T_LOG);

			if (t)
				t->u.n = (s[0] == 'V' || s[0] == 'v' || s[0] == 'T' || s[0] == 't') ? 1 : 0;
			return;
		}
		{
			/* un nombre definido (si no lo está aún, #¿NOMBRE? al calcular) */
			cu_tok *t = pieza(L, T_NOMBRE);

			if (t) {
				u32 o = mete_txt(L, (const char *)s, nn);

				t = &L->t[L->nt - 1];
				t->u.x.i = o;
				t->u.x.n = nn;
			}
			L->nombres = 1;
		}
	} else if (!c)
		falla(L, "La fórmula está incompleta");
	else
		falla(L, "No se entiende la fórmula aquí");
	L->hondo--;
}

/* el signo y el % */
static void factor(lector *L)
{
	int c = mira(L);

	if (L->mal)
		return;
	if (c == '-' || c == '+') {
		L->i++;
		if (++L->hondo > HONDO) {
			falla(L, "Demasiados signos seguidos");
			return;
		}
		factor(L);
		L->hondo--;
		pieza(L, c == '-' ? T_NEG : T_POS);
		return;
	}
	primario(L);
	while (!L->mal && mira(L) == '%') {
		L->i++;
		pieza(L, T_PORC);
	}
}

static void potencia(lector *L)
{
	factor(L);
	while (!L->mal && mira(L) == '^') {
		L->i++;
		factor(L);
		pieza(L, T_POT);
	}
}

static void termino(lector *L)
{
	potencia(L);
	for (;;) {
		int c = mira(L);

		if (L->mal || (c != '*' && c != '/'))
			return;
		L->i++;
		potencia(L);
		pieza(L, c == '*' ? T_POR : T_ENTRE);
	}
}

static void suma(lector *L)
{
	termino(L);
	for (;;) {
		int c = mira(L);

		if (L->mal || (c != '+' && c != '-'))
			return;
		L->i++;
		termino(L);
		pieza(L, c == '+' ? T_SUMA : T_RESTA);
	}
}

static void union_(lector *L)
{
	suma(L);
	while (!L->mal && mira(L) == '&') {
		L->i++;
		suma(L);
		pieza(L, T_UNE);
	}
}

static void expresion(lector *L)
{
	if (++L->hondo > HONDO) {
		falla(L, "La fórmula tiene demasiados paréntesis o funciones unos dentro de otros");
		return;
	}
	union_(L);
	for (;;) {
		int c = mira(L), op = -1;

		if (L->mal)
			break;
		if (c == '=') {
			op = T_IGUAL;
			L->i++;
		} else if (c == '<') {
			L->i++;
			if (L->i < L->n && L->s[L->i] == '>') {
				op = T_DISTINTO;
				L->i++;
			} else if (L->i < L->n && L->s[L->i] == '=') {
				op = T_MENOR_IG;
				L->i++;
			} else
				op = T_MENOR;
		} else if (c == '>') {
			L->i++;
			if (L->i < L->n && L->s[L->i] == '=') {
				op = T_MAYOR_IG;
				L->i++;
			} else
				op = T_MAYOR;
		} else
			break;
		union_(L);
		pieza(L, op);
	}
	L->hondo--;
}

formula *fm_lee(libro *l, const hoja *h, const char *s, u32 n, const cu_local *loc, char *err, u32 cabe, u32 *donde)
{
	lector L;
	formula *fm;

	memset(&L, 0, sizeof L);
	L.l = l;
	L.h = h;
	L.loc = loc;
	L.s = (const u8 *)s;
	L.n = n;
	L.err = err;
	L.cabe_err = cabe;
	if (err && cabe)
		err[0] = 0;
	/* (el «=» de delante, si viene; y el «of:=» de los .ODS) */
	if (L.n >= 3 && memcmp(L.s, "of:", 3) == 0)
		L.i = 3;
	else if (L.n >= 4 && memcmp(L.s, "oooc:", 5 <= L.n ? 5 : 4) == 0)
		L.i = 5;
	blancos(&L);
	if (L.i < L.n && L.s[L.i] == '=')
		L.i++;
	expresion(&L);
	if (!L.mal && mira(&L) != 0)
		falla(&L, mira(&L) == ')' ? "Sobra un «)»" : "Sobra algo al final de la fórmula");
	if (!L.mal && L.nt == 0)
		falla(&L, "La fórmula está vacía");
	if (L.mal) {
		if (donde)
			*donde = L.donde;
		so_suelta(L.t);
		buf_suelta(&L.txt);
		return NULL;
	}
	fm = so_pide(sizeof(formula) + (L.nt > 1 ? L.nt - 1 : 0) * sizeof(cu_tok));
	if (!fm) {
		if (err)
			copia(err, "No hay memoria", cabe);
		so_suelta(L.t);
		buf_suelta(&L.txt);
		return NULL;
	}
	memset(fm, 0, sizeof *fm);
	fm->n = L.nt;
	memcpy(fm->t, L.t, L.nt * sizeof(cu_tok));
	so_suelta(L.t);
	fm->volatil = (u8)L.volatil;
	fm->con_nombres = (u8)L.nombres;
	fm->n_txt = L.txt.n;
	fm->txt = (char *)L.txt.d;	/* (se queda con el búfer) */
	return fm;
}

void fm_suelta(formula *fm)
{
	if (!fm)
		return;
	so_suelta(fm->txt);
	so_suelta(fm->nodos);
	so_suelta(fm);
}

formula *fm_copia(const formula *fm)
{
	formula *c;
	u32 tam;

	if (!fm)
		return NULL;
	tam = sizeof(formula) + (fm->n > 1 ? fm->n - 1 : 0) * sizeof(cu_tok);
	c = so_pide(tam);
	if (!c)
		return NULL;
	memcpy(c, fm, tam);
	c->nodos = NULL;
	c->n_nodos = 0;
	c->txt = NULL;
	if (fm->n_txt && fm->txt) {
		c->txt = so_pide(fm->n_txt + 1);
		if (!c->txt) {
			so_suelta(c);
			return NULL;
		}
		memcpy(c->txt, fm->txt, fm->n_txt);
		c->txt[fm->n_txt] = 0;
	}
	return c;
}

/* ---- escribir ---- */
static const int PRIO[] = {
	[T_IGUAL] = 1, [T_DISTINTO] = 1, [T_MENOR] = 1, [T_MAYOR] = 1, [T_MENOR_IG] = 1, [T_MAYOR_IG] = 1,
	[T_UNE] = 2, [T_SUMA] = 3, [T_RESTA] = 3, [T_POR] = 4, [T_ENTRE] = 4, [T_POT] = 5,
};
static const char *const OPS[] = {
	[T_SUMA] = "+", [T_RESTA] = "-", [T_POR] = "*", [T_ENTRE] = "/", [T_POT] = "^", [T_UNE] = "&", [T_IGUAL] = "=",
	[T_DISTINTO] = "<>", [T_MENOR] = "<", [T_MAYOR] = ">", [T_MENOR_IG] = "<=", [T_MAYOR_IG] = ">=",
};

typedef struct {
	u32 o, n;		/* dónde está en el búfer de trabajo */
	int prio;		/* 9: no hace falta paréntesis */
} trozo;

static int hoja_con_comillas(const char *s)
{
	const char *p;
	u32 f, c;
	const char *fin;

	if (!s[0] || (s[0] >= '0' && s[0] <= '9'))
		return 1;
	for (p = s; *p; p++)
		if (!de_nombre((u8)*p) || *p == '.')
			return 1;
	/* (si parece una celda: «A1») */
	if (cu_lee_ref(s, &f, &c, &fin) && !*fin)
		return 1;
	if (iguales_sin_caja(s, "VERDADERO") || iguales_sin_caja(s, "FALSO") || iguales_sin_caja(s, "TRUE") || iguales_sin_caja(s, "FALSE"))
		return 1;
	return 0;
}

static void pon_hoja(bufer *b, const char *nombre, int ods, int dolar)
{
	if (ods && dolar)
		buf_c(b, '$');
	if (hoja_con_comillas(nombre)) {
		const char *p;

		buf_c(b, '\'');
		for (p = nombre; *p; p++) {
			if (*p == '\'')
				buf_c(b, '\'');
			buf_c(b, (u8)*p);
		}
		buf_c(b, '\'');
	} else
		buf_txt(b, nombre);
}

static void pon_celda(bufer *b, u32 f, u32 c, int fc, int ff)
{
	char t[16];

	if (fc)
		buf_c(b, '$');
	cu_col_texto(c, t);
	buf_txt(b, t);
	if (ff)
		buf_c(b, '$');
	buf_fmt(b, "%u", f + 1);
}

static void pon_ref(bufer *b, const libro *l, const hoja *h, const cu_tok *t, const cu_local *loc)
{
	const hoja *hr = l ? lib_hoja_id((libro *)l, t->h) : NULL;
	int con = (t->a & RA_HOJA) || !h || t->h != h->id;
	int ods = loc->ods;

	if (l && !hr) {
		buf_txt(b, ods ? "[.#REF!]" : cu_error_texto(ER_REF, loc->ingles));
		return;
	}
	if (ods)
		buf_c(b, '[');
	if (con && hr)
		pon_hoja(b, hr->nombre, ods, 1);
	if (con && hr && !ods)
		buf_c(b, '!');
	if (ods)
		buf_c(b, '.');
	if (t->t == T_REF)
		pon_celda(b, (u32)t->u.r.f0, t->u.r.c0, t->a & RA_C0, t->a & RA_F0);
	else if (t->a & RA_COLS) {
		char x[8];

		if (t->a & RA_C0)
			buf_c(b, '$');
		cu_col_texto(t->u.r.c0, x);
		buf_txt(b, x);
		buf_txt(b, ods ? ":." : ":");
		if (t->a & RA_C1)
			buf_c(b, '$');
		cu_col_texto(t->u.r.c1, x);
		buf_txt(b, x);
	} else if (t->a & RA_FILAS) {
		if (t->a & RA_F0)
			buf_c(b, '$');
		buf_fmt(b, "%u", (u32)t->u.r.f0 + 1);
		buf_txt(b, ods ? ":." : ":");
		if (t->a & RA_F1)
			buf_c(b, '$');
		buf_fmt(b, "%u", (u32)t->u.r.f1 + 1);
	} else {
		pon_celda(b, (u32)t->u.r.f0, t->u.r.c0, t->a & RA_C0, t->a & RA_F0);
		buf_txt(b, ods ? ":." : ":");
		pon_celda(b, (u32)t->u.r.f1, t->u.r.c1, t->a & RA_C1, t->a & RA_F1);
	}
	if (ods)
		buf_c(b, ']');
}

void fm_escribe(const libro *l, const formula *fm, const hoja *h, const cu_local *loc, bufer *b)
{
	trozo *pila;
	bufer w = { 0 };
	u32 np = 0, i;

	if (!fm || !fm->n)
		return;
	pila = so_pide(sizeof(trozo) * (fm->n + 1));
	if (!pila) {
		b->mal = 1;
		return;
	}
	for (i = 0; i < fm->n; i++) {
		const cu_tok *t = &fm->t[i];
		u32 o = w.n;

		switch (t->t) {
		case T_NUM: {
			char d[48];

			num_a_texto_c(t->u.n, d, sizeof d);
			if (loc->dec != '.') {
				char *p = strchr(d, '.');

				if (p)
					*p = loc->dec;
			}
			buf_txt(&w, d);
			pila[np].prio = t->u.n < 0 ? 0 : 9;
			break;
		}
		case T_TXT: {
			u32 k;
			const char *s = fm->txt ? fm->txt + t->u.x.i : "";

			buf_c(&w, '"');
			for (k = 0; k < t->u.x.n; k++) {
				if (s[k] == '"')
					buf_c(&w, '"');
				buf_c(&w, (u8)s[k]);
			}
			buf_c(&w, '"');
			pila[np].prio = 9;
			break;
		}
		case T_LOG:
			buf_txt(&w, t->u.n != 0 ? (loc->ingles ? "TRUE" : "VERDADERO") : (loc->ingles ? "FALSE" : "FALSO"));
			pila[np].prio = 9;
			break;
		case T_ERR:
			buf_txt(&w, loc->ods && t->u.x.i == ER_REF ? "[.#REF!]" : cu_error_texto((int)t->u.x.i, loc->ingles));
			pila[np].prio = 9;
			break;
		case T_REF:
		case T_RANGO:
			pon_ref(&w, l, h, t, loc);
			pila[np].prio = 9;
			break;
		case T_NOMBRE:
			buf_txt(&w, fm->txt ? fm->txt + t->u.x.i : "");
			pila[np].prio = 9;
			break;
		case T_FALTA:
			pila[np].prio = 9;
			break;
		case T_FUNC: {
			u32 na = t->u.x.n, k;
			const char *nom = t->h < CU_N_FUNCIONES ? (loc->ingles ? CU_FUNCIONES[t->h].en : CU_FUNCIONES[t->h].es) : (fm->txt ? fm->txt + t->u.x.i : "?");

			if (na > np)
				na = np;
			buf_txt(&w, nom);
			buf_c(&w, '(');
			for (k = np - na; k < np; k++) {
				if (k > np - na)
					buf_c(&w, (u8)loc->sep);
				buf_pon(&w, w.d + pila[k].o, pila[k].n);
			}
			buf_c(&w, ')');
			np -= na;
			pila[np].prio = 9;
			break;
		}
		case T_NEG:
		case T_POS:
			if (np < 1)
				break;
			np--;
			buf_c(&w, t->t == T_NEG ? '-' : '+');
			buf_pon(&w, w.d + pila[np].o, pila[np].n);
			pila[np].prio = 6;
			break;
		case T_PORC:
			if (np < 1)
				break;
			np--;
			if (pila[np].prio < 7) {
				buf_c(&w, '(');
				buf_pon(&w, w.d + pila[np].o, pila[np].n);
				buf_c(&w, ')');
			} else
				buf_pon(&w, w.d + pila[np].o, pila[np].n);
			buf_c(&w, '%');
			pila[np].prio = 7;
			break;
		case T_PAREN:
			if (np < 1)
				break;
			np--;
			buf_c(&w, '(');
			buf_pon(&w, w.d + pila[np].o, pila[np].n);
			buf_c(&w, ')');
			pila[np].prio = 9;
			break;
		default:
			if (t->t >= T_SUMA && t->t <= T_MAYOR_IG && np >= 2) {
				int p = PRIO[t->t];
				trozo a = pila[np - 2], z = pila[np - 1];

				np -= 2;
				/* (lo de la izquierda puede tener la misma prioridad; lo de la derecha, no) */
				if (a.prio < p) {
					buf_c(&w, '(');
					buf_pon(&w, w.d + a.o, a.n);
					buf_c(&w, ')');
				} else
					buf_pon(&w, w.d + a.o, a.n);
				buf_txt(&w, OPS[t->t]);
				if (z.prio <= p && z.prio != 9) {
					buf_c(&w, '(');
					buf_pon(&w, w.d + z.o, z.n);
					buf_c(&w, ')');
				} else
					buf_pon(&w, w.d + z.o, z.n);
				pila[np].prio = p;
			} else
				continue;
			break;
		}
		if (w.mal)
			break;
		pila[np].o = o;
		pila[np].n = w.n - o;
		np++;
	}
	if (np && !w.mal)
		buf_pon(b, w.d + pila[np - 1].o, pila[np - 1].n);
	if (w.mal)
		b->mal = 1;
	buf_suelta(&w);
	so_suelta(pila);
}

u32 fm_texto(const libro *l, const formula *fm, const hoja *h, const cu_local *loc, char *d, u32 cabe)
{
	bufer b = { 0 };
	u32 n;

	if (!cabe)
		return 0;
	fm_escribe(l, fm, h, loc, &b);
	n = b.n < cabe - 1 ? b.n : cabe - 1;
	if (n && b.d)
		memcpy(d, b.d, n);
	/* (sin cortar una letra por la mitad) */
	while (n > 0 && n < b.n && ((u8)d[n] & 0xC0) == 0x80)
		n--;
	d[n] = 0;
	buf_suelta(&b);
	return n;
}

/* ---- correr y arreglar ---- */
static void a_ref_mala(cu_tok *t)
{
	t->t = T_ERR;
	t->a = 0;
	t->u.x.i = ER_REF;
	t->u.x.n = 0;
}

formula *fm_copia_desplaza(const formula *fm, int df, int dc)
{
	formula *c = fm_copia(fm);
	u32 i;

	if (!c)
		return NULL;
	for (i = 0; i < c->n; i++) {
		cu_tok *t = &c->t[i];
		i64 f0, f1, c0, c1;

		if (t->t != T_REF && t->t != T_RANGO)
			continue;
		f0 = t->u.r.f0;
		f1 = t->u.r.f1;
		c0 = t->u.r.c0;
		c1 = t->u.r.c1;
		if (!(t->a & RA_F0) && !(t->a & RA_COLS))
			f0 += df;
		if (!(t->a & RA_C0) && !(t->a & RA_FILAS))
			c0 += dc;
		if (t->t == T_RANGO) {
			if (!(t->a & RA_F1) && !(t->a & RA_COLS))
				f1 += df;
			if (!(t->a & RA_C1) && !(t->a & RA_FILAS))
				c1 += dc;
		} else {
			f1 = f0;
			c1 = c0;
		}
		if (f0 < 0 || f1 < 0 || c0 < 0 || c1 < 0 || f0 >= CU_FILAS || f1 >= CU_FILAS || c0 >= CU_COLS || c1 >= CU_COLS) {
			a_ref_mala(t);
			continue;
		}
		t->u.r.f0 = (i32)f0;
		t->u.r.f1 = (i32)f1;
		t->u.r.c0 = (u16)c0;
		t->u.r.c1 = (u16)c1;
	}
	return c;
}

/* corre [a, b] por meter o quitar n desde «desde»; 0 si desaparece entero */
static int corre(i64 *a, i64 *b, u32 desde, int n, i64 tope)
{
	if (n > 0) {
		if (*a >= desde)
			*a += n;
		if (*b >= desde)
			*b += n;
		if (*a >= tope)
			return 0;
		if (*b >= tope)
			*b = tope - 1;
		return 1;
	} else {
		i64 k = -n, fin = (i64)desde + k;	/* se quitan [desde, fin) */

		if (*a >= desde && *b < fin)
			return 0;
		if (*a >= fin)
			*a -= k;
		else if (*a >= desde)
			*a = desde;
		if (*b >= fin)
			*b -= k;
		else if (*b >= desde)
			*b = (i64)desde - 1;
		return *a <= *b;
	}
}

int fm_ajusta(formula *fm, u16 aqui, u16 h, int eje, u32 desde, int n)
{
	u32 i;
	int cambia = 0;

	if (!fm || !n)
		return 0;
	for (i = 0; i < fm->n; i++) {
		cu_tok *t = &fm->t[i];
		i64 a, b;

		if ((t->t != T_REF && t->t != T_RANGO) || t->h != h)
			continue;
		(void)aqui;
		if (eje == 0) {
			if (t->a & RA_COLS)
				continue;
			a = t->u.r.f0;
			b = t->t == T_RANGO ? t->u.r.f1 : a;
			if (!corre(&a, &b, desde, n, CU_FILAS)) {
				a_ref_mala(t);
				cambia = 1;
				continue;
			}
			if (a != t->u.r.f0 || (t->t == T_RANGO && b != t->u.r.f1))
				cambia = 1;
			t->u.r.f0 = (i32)a;
			t->u.r.f1 = t->t == T_RANGO ? (i32)b : (i32)a;
		} else {
			if (t->a & RA_FILAS)
				continue;
			a = t->u.r.c0;
			b = t->t == T_RANGO ? t->u.r.c1 : a;
			if (!corre(&a, &b, desde, n, CU_COLS)) {
				a_ref_mala(t);
				cambia = 1;
				continue;
			}
			if (a != t->u.r.c0 || (t->t == T_RANGO && b != t->u.r.c1))
				cambia = 1;
			t->u.r.c0 = (u16)a;
			t->u.r.c1 = t->t == T_RANGO ? (u16)b : (u16)a;
		}
	}
	return cambia;
}

int fm_mueve(formula *fm, u16 aqui, u16 h, rango de, int df, int dc, u16 h_a)
{
	u32 i;
	int cambia = 0;

	(void)aqui;
	if (!fm)
		return 0;
	for (i = 0; i < fm->n; i++) {
		cu_tok *t = &fm->t[i];
		u32 f1, c1;

		if ((t->t != T_REF && t->t != T_RANGO) || t->h != h || (t->a & (RA_COLS | RA_FILAS)))
			continue;
		f1 = t->t == T_RANGO ? (u32)t->u.r.f1 : (u32)t->u.r.f0;
		c1 = t->t == T_RANGO ? t->u.r.c1 : t->u.r.c0;
		if (!rg_dentro(de, (u32)t->u.r.f0, t->u.r.c0) || !rg_dentro(de, f1, c1))
			continue;
		{
			i64 a = (i64)t->u.r.f0 + df, b = (i64)f1 + df, x = (i64)t->u.r.c0 + dc, y = (i64)c1 + dc;

			if (a < 0 || b >= CU_FILAS || x < 0 || y >= CU_COLS) {
				a_ref_mala(t);
				cambia = 1;
				continue;
			}
			t->u.r.f0 = (i32)a;
			t->u.r.c0 = (u16)x;
			if (t->t == T_RANGO) {
				t->u.r.f1 = (i32)b;
				t->u.r.c1 = (u16)y;
			} else {
				t->u.r.f1 = (i32)a;
				t->u.r.c1 = (u16)x;
			}
			if (h_a != h) {
				t->h = h_a;
				t->a |= RA_HOJA;
			}
			cambia = 1;
		}
	}
	return cambia;
}

int fm_sin_hoja(formula *fm, u16 aqui, u16 h)
{
	u32 i;
	int cambia = 0;

	(void)aqui;
	if (!fm)
		return 0;
	for (i = 0; i < fm->n; i++)
		if ((fm->t[i].t == T_REF || fm->t[i].t == T_RANGO) && fm->t[i].h == h) {
			a_ref_mala(&fm->t[i]);
			cambia = 1;
		}
	return cambia;
}
