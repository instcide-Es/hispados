/* vt.c - (TERMO) el terminal de SSH.BIN en una ventana (vt.h): lo que lee,
 * lo que contesta y cómo se pinta. Sin llamadas al sistema (lo prueba el
 * anfitrión: pruebas/vt_prueba.c).
 *
 * Lo que se entiende (el «xterm-256color» de terminfo, y lo que piden Ink —la
 * interfaz de Claude Code—, htop, vim, less, mc y tmux):
 *  - C0: BEL BS HT LF VT FF CR; ESC 7 8 D E H M c = > ( ) y los DCS, OSC, APC,
 *    PM y SOS (hasta ST o BEL);
 *  - CSI: @ A B C D E F G H I J K L M P S T X Z ` a b c d e f g h l m n p q r
 *    s t u, con «?», «>», «=», «$», «!» y espacio; los parámetros con «;» y
 *    con «:» (38:2::r:g:b, 4:3);
 *  - SGR: negrita, tenue, cursiva, subrayado (y sus formas), parpadeo,
 *    inverso, oculto, tachado; 16, 256 y 24 bits (fondo y letra);
 *  - modos: 1 (flechas de aplicación), 7 (envolver), 12, 25 (cursor), 47,
 *    1047, 1048, 1049 (la otra pantalla), 1000/1002/1003/1006 (el ratón: se
 *    apuntan, no se mandan), 1004 (el foco), 2004 (pegado entre corchetes),
 *    2026 (actualización sincronizada: no se pinta a medias); IRM (4);
 *  - preguntas: DA1, DA2, XTVERSION, DSR 5/6 (y ?6), DECRQM, OSC 10/11/12/4
 *    con «?», el tamaño (CSI 14/16/18 t); el título (OSC 0/2);
 *  - letras anchas (CJK, emoji: dos columnas) y las de ancho 0 (los acentos
 *    sueltos, el selector de variante) sin descolocar nada.
 * La letra es la de 8x16 del escritorio (letra.cas: para lo latino) y, lo
 * que esa no tiene, se dibuja aquí: las cajas (U+2500, con las esquinas
 * redondas ╭╮╰╯, gruesas y dobles), los bloques (U+2580, cuartos, sombras),
 * el Braille (U+2800, para las gráficas de htop y btop) y los símbolos de las
 * interfaces de hoy (● ○ ◆ ■ ▲ ▶ ❯ › ✓ ✗ ✻ ✳ ✢ ✶ ✽ ⏺ ⎿ ⏵ ⚠ … ← ↑ → ↓ ↵).
 * Lo que tampoco, sale como una cajita (el «tofu»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include <stddef.h>
#include "vt.h"
#include "letra.h"	/* letra8x16[256 * 16] (juegos/datos386.py) */

void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);

static struct vt_celda pantalla1[VT_MAX_COLS * VT_MAX_FILAS], pantalla2[VT_MAX_COLS * VT_MAX_FILAS];

/* ---------------------------------------------------------------- textos sin printf */
static int pon_num(char *o, int v)
{
	char t[12];
	int n = 0, i = 0;

	if (v < 0)
		o[i++] = '-', v = -v;
	do
		t[n++] = (char)('0' + v % 10);
	while ((v /= 10) && n < 11);
	while (n)
		o[i++] = t[--n];
	return i;
}

static void responde(struct vt *t, const char *s, int n)
{
	if (n < 0)
		for (n = 0; s[n]; n++)
			;
	if (t->nresp + n <= (int)sizeof t->resp) {
		memcpy(t->resp + t->nresp, s, (size_t)n);
		t->nresp += n;
	}
}

/* «ESC [ a ; b X» y parecidos: pre, los números (hasta 3; -1 los acaba) y fin */
static void responde_num(struct vt *t, const char *pre, int a, int b, int c, const char *fin)
{
	char o[64];
	int n = 0, i;

	for (i = 0; pre[i]; i++)
		o[n++] = pre[i];
	n += pon_num(o + n, a);
	if (b >= 0) {
		o[n++] = ';';
		n += pon_num(o + n, b);
	}
	if (c >= 0) {
		o[n++] = ';';
		n += pon_num(o + n, c);
	}
	for (i = 0; fin[i]; i++)
		o[n++] = fin[i];
	responde(t, o, n);
}

/* ---------------------------------------------------------------- el ancho */
int vt_ancho(uint32_t c)
{
	if (c == 0x200B || c == 0x200C || c == 0x200D || c == 0x2060 || (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0x0300 && c <= 0x036F) ||
	    (c >= 0x1AB0 && c <= 0x1AFF) || (c >= 0x20D0 && c <= 0x20FF) || (c >= 0xE0100 && c <= 0xE01EF))
		return 0;
	if ((c >= 0x1100 && c <= 0x115F) || c == 0x2329 || c == 0x232A || (c >= 0x231A && c <= 0x231B) || (c >= 0x23E9 && c <= 0x23EC) ||
	    c == 0x23F0 || c == 0x23F3 || (c >= 0x25FD && c <= 0x25FE) || (c >= 0x2614 && c <= 0x2615) || (c >= 0x2648 && c <= 0x2653) ||
	    c == 0x267F || c == 0x2693 || c == 0x26A1 || (c >= 0x26AA && c <= 0x26AB) || (c >= 0x26BD && c <= 0x26BE) ||
	    (c >= 0x26C4 && c <= 0x26C5) || c == 0x26CE || c == 0x26D4 || c == 0x26EA || (c >= 0x26F2 && c <= 0x26F3) || c == 0x26F5 ||
	    c == 0x26FA || c == 0x26FD || c == 0x2705 || (c >= 0x270A && c <= 0x270B) || c == 0x2728 || c == 0x274C || c == 0x274E ||
	    (c >= 0x2753 && c <= 0x2755) || c == 0x2757 || (c >= 0x2795 && c <= 0x2797) || c == 0x27B0 || c == 0x27BF ||
	    (c >= 0x2B1B && c <= 0x2B1C) || c == 0x2B50 || c == 0x2B55 || (c >= 0x2E80 && c <= 0x303E) || (c >= 0x3041 && c <= 0x33FF) ||
	    (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0xA000 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) ||
	    (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE4F) || (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6) ||
	    (c >= 0x1F300 && c <= 0x1F64F) || (c >= 0x1F680 && c <= 0x1F6FF) || (c >= 0x1F900 && c <= 0x1F9FF) ||
	    (c >= 0x1FA70 && c <= 0x1FAFF) || (c >= 0x20000 && c <= 0x3FFFD))
		return 2;
	return 1;
}

/* ---------------------------------------------------------------- la pantalla */
#define CEL(t, x, y) ((t)->p[(y) * (t)->cols + (x)])

static void limpia(struct vt *t, int y, int x0, int x1)
{
	int x;

	if (y < 0 || y >= t->filas)
		return;
	if (x0 < 0)
		x0 = 0;
	if (x1 > t->cols)
		x1 = t->cols;
	for (x = x0; x < x1; x++) {
		struct vt_celda *c = &CEL(t, x, y);

		c->c = ' ';
		c->fg = t->fg;
		c->bg = t->bg;	/* (borrar deja el fondo de ahora: el «bce» de xterm) */
		c->at = 0;
	}
	/* una letra ancha partida por lo borrado: la otra mitad, también */
	if (x0 > 0 && (CEL(t, x0 - 1, y).at & VT_ANCHA) && x0 < t->cols) {
		CEL(t, x0 - 1, y).c = ' ';
		CEL(t, x0 - 1, y).at = 0;
	}
	if (x1 < t->cols && (CEL(t, x1, y).at & VT_SEGUNDA)) {
		CEL(t, x1, y).c = ' ';
		CEL(t, x1, y).at = 0;
	}
	t->sucia[y] = 1;
}

static void sube(struct vt *t, int ini, int fin, int n)	/* de ini a fin (incluida), n filas arriba */
{
	int f;

	if (ini < 0 || fin >= t->filas || ini > fin)
		return;
	if (n > fin - ini + 1)
		n = fin - ini + 1;
	for (f = ini; f <= fin - n; f++) {
		memcpy(&CEL(t, 0, f), &CEL(t, 0, f + n), sizeof(struct vt_celda) * (size_t)t->cols);
		t->sucia[f] = 1;
	}
	for (; f <= fin; f++)
		limpia(t, f, 0, t->cols);
}

static void baja(struct vt *t, int ini, int fin, int n)
{
	int f;

	if (ini < 0 || fin >= t->filas || ini > fin)
		return;
	if (n > fin - ini + 1)
		n = fin - ini + 1;
	for (f = fin; f >= ini + n; f--) {
		memcpy(&CEL(t, 0, f), &CEL(t, 0, f - n), sizeof(struct vt_celda) * (size_t)t->cols);
		t->sucia[f] = 1;
	}
	for (; f >= ini; f--)
		limpia(t, f, 0, t->cols);
}

static void avance(struct vt *t)
{
	if (t->cy == t->abajo)
		sube(t, t->arriba, t->abajo, 1);
	else if (t->cy < t->filas - 1)
		t->cy++;
}

static void retroceso_linea(struct vt *t)	/* RI */
{
	if (t->cy == t->arriba)
		baja(t, t->arriba, t->abajo, 1);
	else if (t->cy > 0)
		t->cy--;
}

static void tabs_de_siempre(struct vt *t)
{
	int i;

	for (i = 0; i < VT_MAX_COLS; i++)
		t->tabs[i] = i % 8 == 0 && i;
}

static void reinicia(struct vt *t)
{
	int f;

	t->fg = t->bg = VT_DEF;
	t->at = 0;
	for (f = 0; f < t->filas; f++)
		limpia(t, f, 0, t->cols);
	t->cx = t->cy = t->envolver = 0;
	t->arriba = 0;
	t->abajo = t->filas - 1;
	t->cursor_visible = 1;
	t->forma_cursor = 0;
	t->teclas_app = t->teclado_app = t->origen = t->insertar = 0;
	t->autoenvolver = 1;
	t->pegado = t->foco = t->sincro = t->raton = 0;
	t->g0_lineas = 0;
	t->s_cx = t->s_cy = 0;
	t->s_fg = t->s_bg = VT_DEF;
	t->s_at = 0;
	tabs_de_siempre(t);
}

int vt_inicia(struct vt *t, int cols, int filas)
{
	memset(t, 0, sizeof *t);
	t->p = pantalla1;
	t->alt = pantalla2;
	t->tinta = 0xD8DEE6;
	t->fondo = 0x15181D;
	if (cols < 20)
		cols = 20;
	if (filas < 5)
		filas = 5;
	t->cols = cols > VT_MAX_COLS ? VT_MAX_COLS : cols;
	t->filas = filas > VT_MAX_FILAS ? VT_MAX_FILAS : filas;
	reinicia(t);
	memcpy(t->alt, t->p, sizeof(struct vt_celda) * (size_t)t->cols * (size_t)t->filas);
	return 0;
}

void vt_libre(struct vt *t)
{
	(void)t;
}

/* otro tamaño: lo de dentro se queda (por arriba a la izquierda; si encoge
 * de alto, se pierde lo de arriba, como en xterm, para que el cursor siga) */
void vt_tamano(struct vt *t, int cols, int filas)
{
	static struct vt_celda copia[VT_MAX_COLS * VT_MAX_FILAS];
	int k, x, y, quita = 0;

	if (cols < 20)
		cols = 20;
	if (filas < 5)
		filas = 5;
	if (cols > VT_MAX_COLS)
		cols = VT_MAX_COLS;
	if (filas > VT_MAX_FILAS)
		filas = VT_MAX_FILAS;
	if (cols == t->cols && filas == t->filas)
		return;
	if (t->cy >= filas)
		quita = t->cy - filas + 1;
	for (k = 0; k < 2; k++) {
		struct vt_celda *p = k ? t->alt : t->p;

		memcpy(copia, p, sizeof(struct vt_celda) * (size_t)t->cols * (size_t)t->filas);
		for (y = 0; y < filas; y++)
			for (x = 0; x < cols; x++) {
				struct vt_celda *d = &p[y * cols + x];

				if (y + quita < t->filas && x < t->cols)
					*d = copia[(y + quita) * t->cols + x];
				else
					d->c = ' ', d->fg = d->bg = VT_DEF, d->at = 0;
			}
	}
	t->cols = cols;
	t->filas = filas;
	t->cy -= quita;
	if (t->cx >= cols)
		t->cx = cols - 1;
	t->arriba = 0;
	t->abajo = filas - 1;
	t->envolver = 0;
	memset(t->sucia, 1, sizeof t->sucia);
}

static void pon_letra(struct vt *t, uint32_t c)
{
	int w = vt_ancho(c);
	struct vt_celda *e;

	if (w == 0)
		return;		/* (los acentos sueltos y los selectores: nada) */
	if (t->g0_lineas && c >= 0x60 && c <= 0x7E) {
		static const uint16_t dec[31] = {
			0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0, 0x00B1, 0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C, 0x23BA,
			0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534, 0x252C, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7,
		};
		c = dec[c - 0x60];
	}
	if (t->envolver) {
		if (t->autoenvolver) {
			t->cx = 0;
			avance(t);
		}
		t->envolver = 0;
	}
	if (w == 2 && t->cx == t->cols - 1) {
		/* (no cabe: a la línea siguiente) */
		limpia(t, t->cy, t->cx, t->cols);
		if (!t->autoenvolver)
			return;
		t->cx = 0;
		avance(t);
	}
	if (t->insertar) {
		int n = t->cols - t->cx - w;

		if (n > 0)
			memmove(&CEL(t, t->cx + w, t->cy), &CEL(t, t->cx, t->cy), sizeof(struct vt_celda) * (size_t)n);
	}
	/* encima de media letra ancha: la otra media, fuera */
	if (CEL(t, t->cx, t->cy).at & VT_SEGUNDA && t->cx > 0)
		CEL(t, t->cx - 1, t->cy).c = ' ', CEL(t, t->cx - 1, t->cy).at = 0;
	if (CEL(t, t->cx, t->cy).at & VT_ANCHA && t->cx + 1 < t->cols && w == 1)
		CEL(t, t->cx + 1, t->cy).c = ' ', CEL(t, t->cx + 1, t->cy).at = 0;
	e = &CEL(t, t->cx, t->cy);
	e->c = c;
	e->fg = t->fg;
	e->bg = t->bg;
	e->at = t->at | (w == 2 ? VT_ANCHA : 0);
	if (w == 2) {
		e[1].c = 0;
		e[1].fg = t->fg;
		e[1].bg = t->bg;
		e[1].at = t->at | VT_SEGUNDA;
	}
	t->sucia[t->cy] = 1;
	t->ultima = c;
	if (t->cx + w >= t->cols) {
		t->cx = t->cols - 1;
		t->envolver = 1;
	} else
		t->cx += w;
}

static void alterna(struct vt *t, int si, int guarda_cursor, int borra)
{
	struct vt_celda *x;

	if (si == t->alterna)
		return;
	if (si && guarda_cursor) {
		t->s_cx = t->cx, t->s_cy = t->cy, t->s_fg = t->fg, t->s_bg = t->bg, t->s_at = t->at;
		t->s_envolver = t->envolver;
	}
	x = t->p;
	t->p = t->alt;
	t->alt = x;
	t->alterna = si;
	if (si && borra) {
		int f;

		for (f = 0; f < t->filas; f++)
			limpia(t, f, 0, t->cols);
	}
	if (!si && guarda_cursor) {
		t->cx = t->s_cx, t->cy = t->s_cy, t->fg = t->s_fg, t->bg = t->s_bg, t->at = t->s_at;
		t->envolver = t->s_envolver;
	}
	memset(t->sucia, 1, sizeof t->sucia);
}

/* ---------------------------------------------------------------- SGR */
static uint32_t color_ext(struct vt *t, int *i)
{
	int k = *i, n = t->npar;
	uint32_t r = VT_DEF;

	/* con «:» (38:5:n, 38:2::r:g:b o 38:2:r:g:b) */
	if (k + 1 < n && t->sub[k + 1]) {
		int g[8], ng = 0, j = k + 1;

		while (j < n && t->sub[j] && ng < 8)
			g[ng++] = t->par[j++];
		*i = j - 1;
		if (ng >= 2 && g[0] == 5)
			return VT_IDX | (uint32_t)(g[1] & 255);
		if (g[0] == 2 && ng >= 4) {
			int a = ng >= 5 ? ng - 3 : 1;

			return VT_RGB | (uint32_t)(g[a] & 255) << 16 | (uint32_t)(g[a + 1] & 255) << 8 | (uint32_t)(g[a + 2] & 255);
		}
		return r;
	}
	/* con «;» */
	if (k + 2 < n && t->par[k + 1] == 5) {
		*i = k + 2;
		return VT_IDX | (uint32_t)(t->par[k + 2] & 255);
	}
	if (k + 4 < n && t->par[k + 1] == 2) {
		*i = k + 4;
		return VT_RGB | (uint32_t)(t->par[k + 2] & 255) << 16 | (uint32_t)(t->par[k + 3] & 255) << 8 | (uint32_t)(t->par[k + 4] & 255);
	}
	*i = n;
	return r;
}

static void sgr(struct vt *t)
{
	int i;

	if (t->npar == 0)
		t->npar = 1, t->par[0] = 0, t->sub[0] = 0;
	for (i = 0; i < t->npar; i++) {
		int p = t->par[i];

		if (t->sub[i])
			continue;	/* (lo de detrás de «:» que no es un color: 4:3...) */
		switch (p) {
		case 0: t->fg = t->bg = VT_DEF; t->at = 0; break;
		case 1: t->at |= VT_NEGRITA; break;
		case 2: t->at |= VT_TENUE; break;
		case 3: t->at |= VT_CURSIVA; break;
		case 4:
			if (i + 1 < t->npar && t->sub[i + 1]) {
				if (t->par[i + 1] == 0)
					t->at &= (uint16_t)~VT_SUBRAYA;
				else
					t->at |= VT_SUBRAYA;
				i++;
			} else
				t->at |= VT_SUBRAYA;
			break;
		case 5: case 6: t->at |= VT_PARPADEO; break;
		case 7: t->at |= VT_INVERSO; break;
		case 8: t->at |= VT_OCULTO; break;
		case 9: t->at |= VT_TACHADO; break;
		case 21: t->at |= VT_SUBRAYA; break;
		case 22: t->at &= (uint16_t)~(VT_NEGRITA | VT_TENUE); break;
		case 23: t->at &= (uint16_t)~VT_CURSIVA; break;
		case 24: t->at &= (uint16_t)~VT_SUBRAYA; break;
		case 25: t->at &= (uint16_t)~VT_PARPADEO; break;
		case 27: t->at &= (uint16_t)~VT_INVERSO; break;
		case 28: t->at &= (uint16_t)~VT_OCULTO; break;
		case 29: t->at &= (uint16_t)~VT_TACHADO; break;
		case 38: t->fg = color_ext(t, &i); break;
		case 39: t->fg = VT_DEF; break;
		case 48: t->bg = color_ext(t, &i); break;
		case 49: t->bg = VT_DEF; break;
		case 58: (void)color_ext(t, &i); break;	/* (el color del subrayado: no se usa) */
		default:
			if (p >= 30 && p <= 37)
				t->fg = VT_IDX | (uint32_t)(p - 30);
			else if (p >= 40 && p <= 47)
				t->bg = VT_IDX | (uint32_t)(p - 40);
			else if (p >= 90 && p <= 97)
				t->fg = VT_IDX | (uint32_t)(p - 90 + 8);
			else if (p >= 100 && p <= 107)
				t->bg = VT_IDX | (uint32_t)(p - 100 + 8);
		}
	}
}

/* ---------------------------------------------------------------- los modos */
static int modo_privado(struct vt *t, int m, int si)	/* -1: no se sabe; si < 0, solo pregunta */
{
	int *v = 0;

	switch (m) {
	case 1: v = &t->teclas_app; break;
	case 6: v = &t->origen; break;
	case 7: v = &t->autoenvolver; break;
	case 12: return 2;		/* (el parpadeo del cursor: no parpadea) */
	case 25: v = &t->cursor_visible; break;
	case 1000: case 1002: case 1003:
		if (si >= 0)
			t->raton = si ? m : 0;
		return t->raton == m ? 1 : 2;
	case 1006: case 1015: return si >= 0 ? 1 : 2;
	case 1004: v = &t->foco; break;
	case 2004: v = &t->pegado; break;
	case 2026:
		if (si >= 0)
			t->sincro = si;
		return t->sincro ? 1 : 2;
	case 47: case 1047:
		if (si >= 0)
			alterna(t, si, 0, m == 1047 && si);
		return t->alterna ? 1 : 2;
	case 1048:
		if (si > 0)
			t->s_cx = t->cx, t->s_cy = t->cy;
		else if (si == 0)
			t->cx = t->s_cx, t->cy = t->s_cy;
		return 1;
	case 1049:
		if (si >= 0)
			alterna(t, si, 1, 1);
		return t->alterna ? 1 : 2;
	default:
		return 0;
	}
	if (si >= 0) {
		*v = si;
		if (m == 6) {
			t->cx = 0;
			t->cy = si ? t->arriba : 0;
		}
		if (m == 25 || m == 7)
			t->sucia[t->cy] = 1;
	}
	return *v ? 1 : 2;
}

/* ---------------------------------------------------------------- CSI */
static void csi(struct vt *t, uint8_t f)
{
	int p0 = t->npar > 0 ? t->par[0] : 0, p1 = t->npar > 1 ? t->par[1] : 0, n = p0 ? p0 : 1, i;

	if (t->privado == '?') {
		if (f == 'h' || f == 'l') {
			for (i = 0; i < t->npar; i++)
				modo_privado(t, t->par[i], f == 'h');
		} else if (f == 'p' && t->inter == '$')
			responde_num(t, "\x1b[?", p0, modo_privado(t, p0, -1), -1, "$y");
		else if (f == 'n' && p0 == 6)
			responde_num(t, "\x1b[?", t->cy + 1, t->cx + 1, -1, "R");
		else if (f == 'J' || f == 'K') {
			t->privado = 0;
			csi(t, f);
		} else if (f == 'u')
		{ /* (el protocolo de teclado de kitty: no lo hay; no se contesta) */ }
		return;
	}
	if (t->privado == '>') {
		if (f == 'c')
			responde(t, "\x1b[>41;380;0c", -1);
		else if (f == 'q') {
			/* XTVERSION */
			responde(t, "\x1bP>|Hispa-Pentium SSH (xterm)\x1b\\", -1);
		}
		return;
	}
	if (t->privado == '=' || t->privado == '<')
		return;
	if (t->inter == ' ') {
		if (f == 'q') {
			t->forma_cursor = p0;
			t->sucia[t->cy] = 1;
		}
		return;
	}
	if (t->inter == '!') {
		if (f == 'p') {	/* DECSTR */
			t->cursor_visible = 1, t->insertar = 0, t->origen = 0, t->autoenvolver = 1;
			t->arriba = 0, t->abajo = t->filas - 1, t->fg = t->bg = VT_DEF, t->at = 0;
		}
		return;
	}
	if (t->inter == '$' || t->inter == '"' || t->inter == '\'')
		return;
	if (f != 'b')
		t->envolver = 0;
	switch (f) {
	case 'A': t->cy -= n; if (t->cy < t->arriba && t->cy + n >= t->arriba) t->cy = t->arriba; break;
	case 'B': case 'e': t->cy += n; if (t->cy > t->abajo && t->cy - n <= t->abajo) t->cy = t->abajo; break;
	case 'C': case 'a': t->cx += n; break;
	case 'D': t->cx -= n; break;
	case 'E': t->cy += n, t->cx = 0; if (t->cy > t->abajo && t->cy - n <= t->abajo) t->cy = t->abajo; break;
	case 'F': t->cy -= n, t->cx = 0; if (t->cy < t->arriba && t->cy + n >= t->arriba) t->cy = t->arriba; break;
	case 'G': case '`': t->cx = n - 1; break;
	case 'd': t->cy = (t->origen ? t->arriba : 0) + n - 1; break;
	case 'H': case 'f': t->cy = (t->origen ? t->arriba : 0) + (p0 ? p0 : 1) - 1, t->cx = (p1 ? p1 : 1) - 1; break;
	case 'I': for (i = 0; i < n; i++) { do t->cx++; while (t->cx < t->cols - 1 && !t->tabs[t->cx]); } break;
	case 'Z': for (i = 0; i < n; i++) { do t->cx--; while (t->cx > 0 && !t->tabs[t->cx]); } break;
	case 'J':
		if (p0 == 0) {
			limpia(t, t->cy, t->cx, t->cols);
			for (i = t->cy + 1; i < t->filas; i++)
				limpia(t, i, 0, t->cols);
		} else if (p0 == 1) {
			for (i = 0; i < t->cy; i++)
				limpia(t, i, 0, t->cols);
			limpia(t, t->cy, 0, t->cx + 1);
		} else if (p0 == 2 || p0 == 3)
			for (i = 0; i < t->filas; i++)
				limpia(t, i, 0, t->cols);
		break;
	case 'K':
		if (p0 == 0)
			limpia(t, t->cy, t->cx, t->cols);
		else if (p0 == 1)
			limpia(t, t->cy, 0, t->cx + 1);
		else
			limpia(t, t->cy, 0, t->cols);
		break;
	case 'L': if (t->cy >= t->arriba && t->cy <= t->abajo) { baja(t, t->cy, t->abajo, n); t->cx = 0; } break;
	case 'M': if (t->cy >= t->arriba && t->cy <= t->abajo) { sube(t, t->cy, t->abajo, n); t->cx = 0; } break;
	case 'S': sube(t, t->arriba, t->abajo, n); break;
	case 'T': if (t->npar <= 1) baja(t, t->arriba, t->abajo, n); break;
	case 'P':
		if (n > t->cols - t->cx)
			n = t->cols - t->cx;
		memmove(&CEL(t, t->cx, t->cy), &CEL(t, t->cx + n, t->cy), sizeof(struct vt_celda) * (size_t)(t->cols - t->cx - n));
		limpia(t, t->cy, t->cols - n, t->cols);
		break;
	case '@':
		if (n > t->cols - t->cx)
			n = t->cols - t->cx;
		memmove(&CEL(t, t->cx + n, t->cy), &CEL(t, t->cx, t->cy), sizeof(struct vt_celda) * (size_t)(t->cols - t->cx - n));
		limpia(t, t->cy, t->cx, t->cx + n);
		break;
	case 'X': limpia(t, t->cy, t->cx, t->cx + n); break;
	case 'b':
		for (i = 0; i < n && i < 1000 && t->ultima; i++)
			pon_letra(t, t->ultima);
		break;
	case 'm': sgr(t); break;
	case 'r':
		t->arriba = (p0 ? p0 : 1) - 1;
		t->abajo = (p1 ? p1 : t->filas) - 1;
		if (t->arriba < 0 || t->abajo >= t->filas || t->arriba >= t->abajo)
			t->arriba = 0, t->abajo = t->filas - 1;
		t->cx = 0;
		t->cy = t->origen ? t->arriba : 0;
		break;
	case 's': t->s_cx = t->cx, t->s_cy = t->cy; break;
	case 'u': t->cx = t->s_cx, t->cy = t->s_cy; break;
	case 'g':
		if (p0 == 0 && t->cx < VT_MAX_COLS)
			t->tabs[t->cx] = 0;
		else if (p0 == 3)
			memset(t->tabs, 0, sizeof t->tabs);
		break;
	case 'h': case 'l':
		for (i = 0; i < t->npar; i++)
			if (t->par[i] == 4)
				t->insertar = f == 'h';
		break;
	case 'n':
		if (p0 == 6)
			responde_num(t, "\x1b[", (t->origen ? t->cy - t->arriba : t->cy) + 1, t->cx + 1, -1, "R");
		else if (p0 == 5)
			responde(t, "\x1b[0n", -1);
		break;
	case 'c':
		if (p0 == 0)
			responde(t, "\x1b[?62;22c", -1);
		break;
	case 't':
		if (p0 == 18)
			responde_num(t, "\x1b[8;", t->filas, t->cols, -1, "t");
		else if (p0 == 14)
			responde_num(t, "\x1b[4;", t->filas * VT_AL, t->cols * VT_AN, -1, "t");
		else if (p0 == 16)
			responde_num(t, "\x1b[6;", VT_AL, VT_AN, -1, "t");
		break;
	}
	if (t->cx < 0) t->cx = 0;
	if (t->cx >= t->cols) t->cx = t->cols - 1;
	if (t->cy < 0) t->cy = 0;
	if (t->cy >= t->filas) t->cy = t->filas - 1;
}

/* ---------------------------------------------------------------- OSC */
static void hex4(char *o, int v)
{
	static const char h[] = "0123456789abcdef";

	o[0] = h[(v >> 4) & 15], o[1] = h[v & 15], o[2] = h[(v >> 4) & 15], o[3] = h[v & 15];
}

static void responde_color(struct vt *t, const char *pre, uint32_t rgb)
{
	char o[64];
	int n = 0, i;

	for (i = 0; pre[i]; i++)
		o[n++] = pre[i];
	memcpy(o + n, "rgb:", 4), n += 4;
	hex4(o + n, (int)(rgb >> 16) & 255), n += 4, o[n++] = '/';
	hex4(o + n, (int)(rgb >> 8) & 255), n += 4, o[n++] = '/';
	hex4(o + n, (int)rgb & 255), n += 4;
	o[n++] = 0x1b, o[n++] = '\\';
	responde(t, o, n);
}

static void osc(struct vt *t)
{
	int num = 0, i = 0, k;
	char *s;

	t->osc[t->nosc < (int)sizeof t->osc ? t->nosc : (int)sizeof t->osc - 1] = 0;
	while (t->osc[i] >= '0' && t->osc[i] <= '9')
		num = num * 10 + t->osc[i++] - '0';
	if (t->osc[i] != ';')
		return;
	s = t->osc + i + 1;
	if (num == 0 || num == 2) {
		for (k = 0; s[k] && k < (int)sizeof t->titulo - 1; k++)
			t->titulo[k] = s[k];
		t->titulo[k] = 0;
		t->titulo_nuevo = 1;
	} else if ((num == 10 || num == 11 || num == 12) && s[0] == '?') {
		char pre[8] = "\x1b]1?;";

		pre[3] = (char)('0' + num - 10);
		responde_color(t, pre, num == 11 ? t->fondo : t->tinta);
	} else if (num == 4) {
		/* «4;n;?»: el color n de la paleta */
		int c = 0;

		k = 0;
		while (s[k] >= '0' && s[k] <= '9')
			c = c * 10 + s[k++] - '0';
		if (s[k] == ';' && s[k + 1] == '?' && c < 256) {
			char pre[16] = "\x1b]4;";
			int n = 4;

			n += pon_num(pre + n, c);
			pre[n++] = ';';
			pre[n] = 0;
			responde_color(t, pre, vt_rgb(t, VT_IDX | (uint32_t)c, 0));
		}
	}
	/* (8: los enlaces; 52: el portapapeles; 7, 133...: no se usan) */
}

/* ---------------------------------------------------------------- lo que llega */
enum { E_NORMAL, E_ESC, E_CSI, E_OSC, E_OSC_ESC, E_CADENA, E_CADENA_ESC, E_JUEGO, E_JUEGO1, E_OTRO };

void vt_entra(struct vt *t, const uint8_t *d, uint32_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++) {
		uint8_t c = d[i];

		/* el UTF-8 (solo fuera de las órdenes) */
		if (t->estado == E_NORMAL) {
			if (t->utf_falta) {
				if ((c & 0xC0) == 0x80) {
					t->utf_v = t->utf_v << 6 | (c & 63);
					if (--t->utf_falta == 0)
						pon_letra(t, t->utf_v > 0x10FFFF ? 0xFFFD : t->utf_v);
					continue;
				}
				t->utf_falta = 0;
				pon_letra(t, 0xFFFD);
			}
			if (c >= 0xC2 && c < 0xE0) {
				t->utf_v = c & 31, t->utf_falta = 1;
				continue;
			}
			if (c >= 0xE0 && c < 0xF0) {
				t->utf_v = c & 15, t->utf_falta = 2;
				continue;
			}
			if (c >= 0xF0 && c < 0xF5) {
				t->utf_v = c & 7, t->utf_falta = 3;
				continue;
			}
			if (c >= 0x80) {
				pon_letra(t, 0xFFFD);
				continue;
			}
		}
		/* lo de control vale en medio de una orden (menos en las cadenas) */
		if (c < 32 && t->estado != E_OSC && t->estado != E_CADENA && t->estado != E_OSC_ESC && t->estado != E_CADENA_ESC) {
			switch (c) {
			case 7: t->campana++; break;
			case 8: if (t->cx > 0) t->cx--; t->envolver = 0; break;
			case 9:
				do
					t->cx++;
				while (t->cx < t->cols - 1 && !t->tabs[t->cx]);
				if (t->cx >= t->cols)
					t->cx = t->cols - 1;
				break;
			case 10: case 11: case 12: avance(t); t->envolver = 0; break;
			case 13: t->cx = 0; t->envolver = 0; break;
			case 14: case 15: break;
			case 24: case 26: t->estado = E_NORMAL; break;
			case 27:
				t->estado = E_ESC;
				break;
			}
			continue;
		}
		switch (t->estado) {
		case E_NORMAL:
			if (c == 127)
				break;
			pon_letra(t, c);
			break;
		case E_ESC:
			t->estado = E_NORMAL;
			switch (c) {
			case '[':
				t->estado = E_CSI;
				t->npar = 0;
				t->privado = t->inter = 0;
				memset(t->par, 0, sizeof t->par);
				memset(t->sub, 0, sizeof t->sub);
				break;
			case ']': t->estado = E_OSC; t->nosc = 0; break;
			case 'P': case '_': case '^': case 'X': t->estado = E_CADENA; break;
			case '(': t->estado = E_JUEGO; break;
			case ')': case '*': case '+': case '-': case '.': case '/': t->estado = E_JUEGO1; break;
			case '#': case '%': case ' ': t->estado = E_OTRO; break;
			case '7':
				t->s_cx = t->cx, t->s_cy = t->cy, t->s_at = t->at, t->s_fg = t->fg, t->s_bg = t->bg;
				t->s_envolver = t->envolver, t->s_g0 = t->g0_lineas;
				break;
			case '8':
				t->cx = t->s_cx, t->cy = t->s_cy, t->at = t->s_at, t->fg = t->s_fg, t->bg = t->s_bg;
				t->envolver = t->s_envolver, t->g0_lineas = t->s_g0;
				break;
			case 'D': avance(t); break;
			case 'E': t->cx = 0; avance(t); break;
			case 'H': if (t->cx < VT_MAX_COLS) t->tabs[t->cx] = 1; break;
			case 'M': retroceso_linea(t); break;
			case 'c': reinicia(t); if (t->alterna) alterna(t, 0, 0, 0); break;
			case '=': t->teclado_app = 1; break;
			case '>': t->teclado_app = 0; break;
			case '\\': break;
			}
			break;
		case E_CSI:
			if (c >= '0' && c <= '9') {
				if (t->npar == 0)
					t->npar = 1;
				if (t->par[t->npar - 1] < 100000)
					t->par[t->npar - 1] = t->par[t->npar - 1] * 10 + c - '0';
			} else if (c == ';' || c == ':') {
				if (t->npar == 0)
					t->npar = 1;
				if (t->npar < 24) {
					t->sub[t->npar] = c == ':';
					t->par[t->npar++] = 0;
				}
			} else if (c == '?' || c == '>' || c == '=' || c == '<') {
				t->privado = c;
			} else if (c >= 0x20 && c <= 0x2F) {
				t->inter = c;
			} else if (c >= 0x40 && c <= 0x7E) {
				csi(t, c);
				t->estado = E_NORMAL;
			}
			break;
		case E_OSC:
			if (c == 7) {
				osc(t);
				t->estado = E_NORMAL;
			} else if (c == 27)
				t->estado = E_OSC_ESC;
			else if (t->nosc < (int)sizeof t->osc - 1)
				t->osc[t->nosc++] = (char)c;
			break;
		case E_OSC_ESC:
			if (c == '\\') {
				osc(t);
				t->estado = E_NORMAL;
			} else {
				t->estado = E_ESC;
				i--;	/* (ESC y otra cosa: lo que sea) */
			}
			break;
		case E_CADENA:
			if (c == 27)
				t->estado = E_CADENA_ESC;
			else if (c == 7)
				t->estado = E_NORMAL;
			break;
		case E_CADENA_ESC:
			t->estado = c == '\\' ? E_NORMAL : E_CADENA;
			break;
		case E_JUEGO:
			t->g0_lineas = c == '0';
			t->estado = E_NORMAL;
			break;
		case E_JUEGO1:
		case E_OTRO:
			t->estado = E_NORMAL;
			break;
		}
	}
}

/* ---------------------------------------------------------------- los colores */
uint32_t vt_rgb(const struct vt *t, uint32_t c, int es_fondo)
{
	static const uint32_t b16[16] = { 0x000000, 0xCD3131, 0x0DBC79, 0xE5E510, 0x2472C8, 0xBC3FBC, 0x11A8CD, 0xE5E5E5,
					  0x666666, 0xF14C4C, 0x23D18B, 0xF5F543, 0x3B8EEA, 0xD670D6, 0x29B8DB, 0xFFFFFF };
	static const uint8_t nivel[6] = { 0, 95, 135, 175, 215, 255 };

	if (c & VT_RGB)
		return c & 0xFFFFFF;
	if (c & VT_IDX) {
		int i = (int)(c & 255);

		if (i < 16)
			return b16[i];
		if (i < 232) {
			i -= 16;
			return (uint32_t)nivel[i / 36] << 16 | (uint32_t)nivel[i / 6 % 6] << 8 | nivel[i % 6];
		}
		i = 8 + (i - 232) * 10;
		return (uint32_t)i << 16 | (uint32_t)i << 8 | (uint32_t)i;
	}
	return es_fondo ? t->fondo : t->tinta;
}

static uint32_t mezcla(uint32_t a, uint32_t b, int k)	/* de a hacia b, k/256 */
{
	uint32_t r = (((a >> 16) & 255) * (uint32_t)(256 - k) + ((b >> 16) & 255) * (uint32_t)k) >> 8;
	uint32_t g = (((a >> 8) & 255) * (uint32_t)(256 - k) + ((b >> 8) & 255) * (uint32_t)k) >> 8;
	uint32_t z = ((a & 255) * (uint32_t)(256 - k) + (b & 255) * (uint32_t)k) >> 8;

	return r << 16 | g << 8 | z;
}

/* ---------------------------------------------------------------- la letra */
static const uint16_t alta437[128] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* (las cajas: se dibujan) */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
	0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0, 0x00A0,
};

/* la de la página 437 (o -1) */
static int de437(uint32_t u)
{
	int i;

	if (u >= 0x20 && u < 0x7F)
		return (int)u;
	if (u == 0xA0)
		return ' ';
	for (i = 0; i < 128; i++)
		if (alta437[i] == u)
			return 128 + i;
	switch (u) {
	/* (las mayúsculas con tilde que la 437 no tiene: sin tilde) */
	case 0xC1: case 0xC0: case 0xC2: case 0xC3: return 'A';
	case 0xC8: case 0xCA: case 0xCB: return 'E';
	case 0xCD: case 0xCC: case 0xCE: case 0xCF: return 'I';
	case 0xD3: case 0xD2: case 0xD4: case 0xD5: return 'O';
	case 0xDA: case 0xD9: case 0xDB: return 'U';
	case 0xE3: return 'a';
	case 0xF5: return 'o';
	case 0x2018: case 0x2019: case 0x201A: case 0x2032: return '\'';
	case 0x201C: case 0x201D: case 0x201E: case 0x2033: return '"';
	case 0x2010: case 0x2011: case 0x2012: case 0x2212: return '-';
	case 0x2039: return '<';
	case 0x203A: return '>';
	case 0x00D7: return 'x';
	case 0x2044: case 0x2215: return '/';
	case 0x00A9: return 'c';
	case 0x00AE: return 'R';
	}
	return -1;
}

/* el dibujo de una celda, a «m» (an x 16, de 0 o 1) */
#define MAXA 16
static uint8_t m[VT_AL][MAXA];

static void punto(int x, int y, int an)
{
	if (x >= 0 && x < an && y >= 0 && y < VT_AL)
		m[y][x] = 1;
}

static void rect(int x0, int y0, int x1, int y1, int an)	/* [x0, x1) x [y0, y1) */
{
	int x, y;

	for (y = y0; y < y1; y++)
		for (x = x0; x < x1; x++)
			punto(x, y, an);
}

static void linea(int x0, int y0, int x1, int y1, int g, int an)
{
	int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx - dy, k;

	for (;;) {
		for (k = 0; k < g; k++)
			punto(x0 + k, y0, an);
		if (x0 == x1 && y0 == y1)
			break;
		if (2 * e > -dy)
			e -= dy, x0 += sx;
		if (2 * e < dx)
			e += dx, y0 += sy;
	}
}

/* en medias décimas: el centro (cx, cy) y el radio r, todo x2 */
static void disco(int cx2, int cy2, int r2, int hueco2, int an)
{
	int x, y;

	for (y = 0; y < VT_AL; y++)
		for (x = 0; x < an; x++) {
			int dx = 2 * x + 1 - cx2, dy = 2 * y + 1 - cy2, d = dx * dx + dy * dy;

			if (d <= r2 * r2 && d >= hueco2 * hueco2)
				m[y][x] = 1;
		}
}

/* un triángulo lleno (en puntos x2) */
static void triangulo(int ax, int ay, int bx, int by, int cx, int cy, int an)
{
	int x, y;

	for (y = 0; y < VT_AL; y++)
		for (x = 0; x < an; x++) {
			int px = 2 * x + 1, py = 2 * y + 1;
			long d1 = (long)(px - bx) * (ay - by) - (long)(ax - bx) * (py - by);
			long d2 = (long)(px - cx) * (by - cy) - (long)(bx - cx) * (py - cy);
			long d3 = (long)(px - ax) * (cy - ay) - (long)(cx - ax) * (py - ay);

			if (!((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0)))
				m[y][x] = 1;
		}
}

/* las cajas: los cuatro brazos (arriba, derecha, abajo, izquierda; 2 bits
 * cada uno: 0 nada, 1 fino, 2 grueso, 3 doble), sacados de los nombres de
 * Unicode (U+2500 a U+257F) */
static const uint8_t cajas[128] = {
	0x44, 0x88, 0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x14, 0x18, 0x24, 0x28,
	0x50, 0x90, 0x60, 0xA0, 0x05, 0x09, 0x06, 0x0A, 0x41, 0x81, 0x42, 0x82, 0x15, 0x19, 0x16, 0x25,
	0x26, 0x1A, 0x29, 0x2A, 0x51, 0x91, 0x52, 0x61, 0x62, 0x92, 0xA1, 0xA2, 0x54, 0x94, 0x58, 0x98,
	0x64, 0xA4, 0x68, 0xA8, 0x45, 0x85, 0x49, 0x89, 0x46, 0x86, 0x4A, 0x8A, 0x55, 0x95, 0x59, 0x99,
	0x56, 0x65, 0x66, 0x96, 0x5A, 0xA5, 0x69, 0x9A, 0xA9, 0xA6, 0x6A, 0xAA, 0xCC, 0xCC, 0x33, 0x33,
	0xCC, 0x33, 0x1C, 0x34, 0x3C, 0xD0, 0x70, 0xF0, 0x0D, 0x07, 0x0F, 0xC1, 0x43, 0xC3, 0x1D, 0x37,
	0x3F, 0xD1, 0x73, 0xF3, 0xDC, 0x74, 0xFC, 0xCD, 0x47, 0xCF, 0xDD, 0x77, 0xFF, 0x14, 0x50, 0x41,
	0x05, 0x00, 0x00, 0x00, 0x40, 0x01, 0x04, 0x10, 0x80, 0x02, 0x08, 0x20, 0x48, 0x21, 0x84, 0x12,
};

static void caja(uint32_t c)
{
	int v = cajas[c - 0x2500], up = v & 3, de = (v >> 2) & 3, ab = (v >> 4) & 3, iz = (v >> 6) & 3;
	const int cx = 3, cy = 7;

	if (c >= 0x2571 && c <= 0x2573) {	/* las diagonales */
		if (c != 0x2572)
			linea(7, 0, 0, 15, 1, 8);
		if (c != 0x2571)
			linea(0, 0, 7, 15, 1, 8);
		return;
	}
	if (c >= 0x256D && c <= 0x2570) {	/* las esquinas redondas */
		int dx = de ? 1 : -1, dy = ab ? 1 : -1;

		if (de)
			rect(cx + 2, cy, 8, cy + 1, 8);
		else
			rect(0, cy, cx - 1, cy + 1, 8);
		if (ab)
			rect(cx, cy + 2, cx + 1, 16, 8);
		else
			rect(cx, 0, cx + 1, cy - 1, 8);
		punto(cx + dx, cy, 8);
		punto(cx, cy + dy, 8);
		punto(cx + dx, cy + dy, 8);	/* (el codo, suave) */
		m[cy][cx + dx] = 1;
		m[cy + dy][cx] = 1;
		return;
	}
	/* los brazos: fino, 1 punto; grueso, 2; doble, dos de 1 separados */
#define BRAZO_V(y0, y1, w) do { if ((w) == 1) rect(cx, y0, cx + 1, y1, 8); else if ((w) == 2) rect(cx, y0, cx + 2, y1, 8); \
			      else if ((w) == 3) { rect(cx - 1, y0, cx, y1, 8); rect(cx + 2, y0, cx + 3, y1, 8); } } while (0)
#define BRAZO_H(x0, x1, w) do { if ((w) == 1) rect(x0, cy, x1, cy + 1, 8); else if ((w) == 2) rect(x0, cy, x1, cy + 2, 8); \
			      else if ((w) == 3) { rect(x0, cy - 1, x1, cy, 8); rect(x0, cy + 2, x1, cy + 3, 8); } } while (0)
	BRAZO_V(0, cy + (up == 3 ? 3 : up ? 1 : 0), up);
	BRAZO_V(cy - (ab == 3 ? 1 : 0), 16, ab);
	BRAZO_H(0, cx + (iz == 3 ? 3 : iz ? 1 : 0), iz);
	BRAZO_H(cx - (de == 3 ? 1 : 0), 8, de);
	/* (las de rayas: con huecos) */
	if ((c >= 0x2504 && c <= 0x250B) || (c >= 0x254C && c <= 0x254F)) {
		int y, x;

		if (iz || de)
			for (x = 1; x < 8; x += 3)
				for (y = 0; y < 16; y++)
					m[y][x] = 0;
		if (up || ab)
			for (y = 2; y < 16; y += 4)
				for (x = 0; x < 8; x++)
					m[y][x] = m[y + 1 < 16 ? y + 1 : y][x] = 0;
	}
}

static void bloque(uint32_t c)
{
	int y, x;

	if (c == 0x2580) rect(0, 0, 8, 8, 8);
	else if (c >= 0x2581 && c <= 0x2588) rect(0, 16 - (int)(c - 0x2580) * 2, 8, 16, 8);
	else if (c >= 0x2589 && c <= 0x258F) rect(0, 0, 8 - (int)(c - 0x2588), 16, 8);
	else if (c == 0x2590) rect(4, 0, 8, 16, 8);
	else if (c >= 0x2591 && c <= 0x2593) {
		for (y = 0; y < 16; y++)
			for (x = 0; x < 8; x++)
				m[y][x] = c == 0x2591 ? ((x + 2 * y) % 4 == 0) : c == 0x2592 ? ((x + y) & 1) : ((x + 2 * y) % 4 != 0);
	} else if (c == 0x2594) rect(0, 0, 8, 2, 8);
	else if (c == 0x2595) rect(7, 0, 8, 16, 8);
	else {
		/* los cuartos: arriba-izq, arriba-der, abajo-izq, abajo-der */
		static const uint8_t q[10] = { 4, 8, 1, 1 | 4 | 8, 1 | 8, 1 | 2 | 4, 1 | 2 | 8, 2, 2 | 4, 2 | 4 | 8 };
		int b = q[c - 0x2596];

		if (b & 1) rect(0, 0, 4, 8, 8);
		if (b & 2) rect(4, 0, 8, 8, 8);
		if (b & 4) rect(0, 8, 4, 16, 8);
		if (b & 8) rect(4, 8, 8, 16, 8);
	}
}

static void braille(uint32_t c)
{
	static const uint8_t px[8] = { 1, 1, 1, 5, 5, 5, 1, 5 }, py[8] = { 1, 5, 9, 1, 5, 9, 13, 13 };
	int b = (int)(c - 0x2800), k;

	for (k = 0; k < 8; k++)
		if (b & (1 << k))
			rect(px[k], py[k], px[k] + 2, py[k] + 2, 8);
}

/* una estrella de «n» puntas (el spinner de Claude Code: ✢ ✳ ✶ ✻ ✽) */
static void estrella(int puntas, int gruesa)
{
	static const int8_t d8[8][2] = { { 0, -6 }, { 4, -4 }, { 6, 0 }, { 4, 4 }, { 0, 6 }, { -4, 4 }, { -6, 0 }, { -4, -4 } };
	int k;

	for (k = 0; k < 8; k++) {
		if (puntas == 4 && (k & 1))
			continue;
		if (puntas == 6 && (k == 2 || k == 6))
			continue;
		linea(3, 8, 3 + d8[k][0] / 2 + (d8[k][0] > 0), 8 + d8[k][1], gruesa ? 2 : 1, 8);
	}
}

/* 1 si lo dibuja aquí (en m) */
static int simbolo(uint32_t c, int an)
{
	switch (c) {
	case 0x25CF: case 0x23FA: case 0x2B24: case 0x26AB: disco(8, 17, 7, 0, an); return 1;	/* ● ⏺ */
	case 0x2022: case 0x2219: disco(8, 17, 4, 0, an); return 1;	/* • */
	case 0x25CB: case 0x25EF: case 0x26AA: disco(8, 17, 7, 5, an); return 1;	/* ○ ◯ */
	case 0x25E6: disco(8, 17, 4, 2, an); return 1;
	case 0x25C9: case 0x25CE: disco(8, 17, 7, 5, an); disco(8, 17, 3, 0, an); return 1;	/* ◉ ◎ */
	case 0x25C6: case 0x2666: triangulo(8, 5, 15, 17, 1, 17, an); triangulo(8, 29, 15, 17, 1, 17, an); return 1;	/* ◆ */
	case 0x25C7: linea(4, 2, 7, 8, 1, an); linea(7, 8, 4, 14, 1, an); linea(4, 14, 0, 8, 1, an); linea(0, 8, 3, 2, 1, an); return 1;
	case 0x25A0: case 0x2B1B: rect(1, 4, 7, 12, an); return 1;	/* ■ */
	case 0x25A1: case 0x2B1C: rect(1, 4, 7, 5, an); rect(1, 11, 7, 12, an); rect(1, 4, 2, 12, an); rect(6, 4, 7, 12, an); return 1;
	case 0x25AA: case 0x25FE: case 0x25FC: rect(2, 6, 6, 10, an); return 1;
	case 0x25AB: case 0x25FD: case 0x25FB: rect(2, 6, 6, 7, an); rect(2, 9, 6, 10, an); rect(2, 6, 3, 10, an); rect(5, 6, 6, 10, an); return 1;
	case 0x25B2: triangulo(8, 7, 15, 23, 1, 23, an); return 1;
	case 0x25BC: triangulo(1, 9, 15, 9, 8, 25, an); return 1;
	case 0x25B6: case 0x25BA: triangulo(1, 7, 15, 17, 1, 27, an); return 1;
	case 0x25B8: case 0x23F5: case 0x2023: triangulo(3, 11, 12, 17, 3, 23, an); return 1;	/* ▸ ⏵ ‣ */
	case 0x25C0: case 0x25C4: triangulo(15, 7, 1, 17, 15, 27, an); return 1;
	case 0x25C2: case 0x23F4: triangulo(13, 11, 4, 17, 13, 23, an); return 1;
	case 0x25B3: linea(3, 4, 0, 11, 1, an); linea(4, 4, 7, 11, 1, an); rect(0, 11, 8, 12, an); return 1;
	case 0x25BD: linea(0, 4, 3, 11, 1, an); linea(7, 4, 4, 11, 1, an); rect(0, 4, 8, 5, an); return 1;
	case 0x23F8: rect(1, 4, 3, 12, an); rect(5, 4, 7, 12, an); return 1;	/* ⏸ */
	case 0x23F9: rect(1, 4, 7, 12, an); return 1;
	case 0x276F: linea(1, 3, 5, 8, 2, an); linea(5, 8, 1, 13, 2, an); return 1;	/* ❯ */
	case 0x276E: linea(6, 3, 2, 8, 2, an); linea(2, 8, 6, 13, 2, an); return 1;
	case 0x2190: linea(0, 8, 7, 8, 1, an); linea(0, 8, 3, 5, 1, an); linea(0, 8, 3, 11, 1, an); return 1;	/* ← */
	case 0x2192: case 0x279C: case 0x2794: linea(0, 8, 7, 8, 1, an); linea(7, 8, 4, 5, 1, an); linea(7, 8, 4, 11, 1, an); return 1;
	case 0x2191: linea(3, 3, 3, 13, 1, an); linea(3, 3, 0, 6, 1, an); linea(3, 3, 6, 6, 1, an); return 1;
	case 0x2193: linea(3, 3, 3, 13, 1, an); linea(3, 13, 0, 10, 1, an); linea(3, 13, 6, 10, 1, an); return 1;
	case 0x2194: linea(0, 8, 7, 8, 1, an); linea(0, 8, 2, 6, 1, an); linea(0, 8, 2, 10, 1, an); linea(7, 8, 5, 6, 1, an); linea(7, 8, 5, 10, 1, an); return 1;
	case 0x2195: linea(3, 2, 3, 14, 1, an); linea(3, 2, 1, 4, 1, an); linea(3, 2, 5, 4, 1, an); linea(3, 14, 1, 12, 1, an); linea(3, 14, 5, 12, 1, an); return 1;
	case 0x21B5: case 0x23CE: linea(6, 4, 6, 10, 1, an); linea(6, 10, 0, 10, 1, an); linea(0, 10, 3, 7, 1, an); linea(0, 10, 3, 13, 1, an); return 1;	/* ↵ */
	case 0x21B3: case 0x2937: linea(1, 3, 1, 10, 1, an); linea(1, 10, 7, 10, 1, an); linea(7, 10, 4, 7, 1, an); linea(7, 10, 4, 13, 1, an); return 1;	/* ↳ */
	case 0x21E7: linea(3, 3, 0, 7, 1, an); linea(3, 3, 6, 7, 1, an); rect(2, 7, 3, 13, an); rect(4, 7, 5, 13, an); rect(2, 12, 5, 13, an); return 1;
	case 0x2713: case 0x2714: case 0x2705: linea(0, 9, 2, 12, c == 0x2713 ? 1 : 2, an); linea(2, 12, 7, 3, c == 0x2713 ? 1 : 2, an); return 1;	/* ✓ ✔ */
	case 0x2717: case 0x2718: case 0x2715: case 0x2716: case 0x274C:
		linea(1, 4, 6, 12, c == 0x2717 || c == 0x2715 ? 1 : 2, an); linea(6, 4, 1, 12, c == 0x2717 || c == 0x2715 ? 1 : 2, an); return 1;	/* ✗ ✘ */
	case 0x2722: case 0x2723: case 0x2724: case 0x2725: case 0x2726: case 0x2727: estrella(4, c != 0x2722); return 1;	/* ✢ ✦ */
	case 0x2733: case 0x2734: case 0x2735: case 0x2731: case 0x2732: estrella(8, c != 0x2733); return 1;	/* ✳ */
	case 0x2736: case 0x2737: case 0x2738: case 0x2739: estrella(6, 1); return 1;	/* ✶ */
	case 0x273A: case 0x273B: case 0x273C: case 0x273D: case 0x273E: case 0x2749: case 0x274A: case 0x274B:
		estrella(8, 0); disco(8, 17, 2, 0, an); return 1;	/* ✻ ✽ */
	case 0x2605: case 0x2B50: triangulo(8, 5, 15, 21, 1, 21, an); triangulo(8, 27, 15, 13, 1, 13, an); return 1;	/* ★ */
	case 0x2606: linea(3, 3, 0, 12, 1, an); linea(4, 3, 7, 12, 1, an); linea(0, 7, 7, 7, 1, an); return 1;
	case 0x26A0: linea(3, 3, 0, 12, 1, an); linea(4, 3, 7, 12, 1, an); rect(0, 12, 8, 13, an); rect(3, 6, 5, 9, an); rect(3, 10, 5, 11, an); return 1;	/* ⚠ */
	case 0x2026: rect(0, 12, 1, 13, an); rect(3, 12, 4, 13, an); rect(6, 12, 7, 13, an); return 1;	/* … */
	case 0x22EF: rect(0, 8, 1, 9, an); rect(3, 8, 4, 9, an); rect(6, 8, 7, 9, an); return 1;
	case 0x23BF: rect(1, 0, 2, 10, an); rect(1, 9, 8, 10, an); return 1;	/* ⎿ */
	case 0x23BE: rect(1, 6, 2, 16, an); rect(1, 6, 8, 7, an); return 1;
	case 0x23AF: case 0x2014: case 0x2015: rect(0, 8, 8, 9, an); return 1;	/* ⎯ — */
	case 0x2013: rect(1, 8, 7, 9, an); return 1;	/* – */
	case 0x23BA: rect(0, 1, 8, 2, an); return 1;
	case 0x23BB: rect(0, 5, 8, 6, an); return 1;
	case 0x23BC: rect(0, 10, 8, 11, an); return 1;
	case 0x23BD: rect(0, 14, 8, 15, an); return 1;
	case 0x29C9: rect(0, 4, 5, 5, an); rect(0, 4, 1, 10, an); rect(0, 9, 3, 10, an); rect(3, 7, 8, 8, an); rect(3, 7, 4, 13, an); rect(7, 7, 8, 13, an); rect(3, 12, 8, 13, an); rect(4, 4, 5, 7, an); return 1;	/* ⧉ */
	case 0x2260: rect(0, 6, 8, 7, an); rect(0, 10, 8, 11, an); linea(5, 3, 2, 13, 1, an); return 1;
	case 0xFFFD: rect(1, 3, 7, 13, an); m[6][3] = m[6][4] = m[9][3] = 0; return 1;
	}
	return 0;
}

/* lo de una celda (c, de an 8 o 16) a «m»; 0 si es un espacio */
static int dibujo(uint32_t c, int an)
{
	int k, y, x;

	memset(m, 0, sizeof m);
	if (c == ' ' || c == 0)
		return 0;
	if (c >= 0x2500 && c <= 0x257F) {
		caja(c);
		return 1;
	}
	if (c >= 0x2580 && c <= 0x259F) {
		bloque(c);
		return 1;
	}
	if (c >= 0x2800 && c <= 0x28FF) {
		braille(c);
		return 1;
	}
	if (simbolo(c, an))
		return 1;
	k = de437(c);
	if (k >= 0 && k != ' ') {
		const unsigned char *g = letra8x16 + k * 16;
		int x0 = an == 16 ? 4 : 0;

		for (y = 0; y < 16; y++)
			for (x = 0; x < 8; x++)
				if (g[y] & (0x80 >> x))
					m[y][x0 + x] = 1;
		return 1;
	}
	if (k == ' ')
		return 0;
	/* el «tofu»: una cajita con el borde (las letras anchas, más ancha) */
	rect(1, 2, an - 1, 3, an);
	rect(1, 13, an - 1, 14, an);
	rect(1, 2, 2, 14, an);
	rect(an - 2, 2, an - 1, 14, an);
	return 1;
}

int vt_pinta(struct vt *t, uint32_t *lz, int an, int al, int paso, int x0, int y0, int todo, int *f0, int *f1)
{
	static int cur_x = -1, cur_y = -1;
	int y, x, hechas = 0;

	*f0 = -1;
	*f1 = -1;
	if (cur_y >= 0 && cur_y < t->filas)
		t->sucia[cur_y] = 1;
	if (t->cursor_visible)
		t->sucia[t->cy] = 1;
	for (y = 0; y < t->filas; y++) {
		if (!todo && !t->sucia[y])
			continue;
		t->sucia[y] = 0;
		if (y0 + (y + 1) * VT_AL > al)
			continue;
		for (x = 0; x < t->cols; x++) {
			struct vt_celda *e = &CEL(t, x, y);
			int ancho = e->at & VT_ANCHA ? 16 : 8, px, py, cursor = t->cursor_visible && x == t->cx && y == t->cy;
			uint32_t fg, bg, *p;

			if (e->at & VT_SEGUNDA)
				continue;
			if (x0 + x * VT_AN + ancho > an)
				ancho = an - x0 - x * VT_AN;
			if (ancho <= 0)
				continue;
			fg = vt_rgb(t, e->fg, 0);
			bg = vt_rgb(t, e->bg, 1);
			if ((e->at & VT_NEGRITA) && (e->fg & VT_IDX) && (e->fg & 255) < 8)
				fg = vt_rgb(t, e->fg + 8, 0);	/* (como xterm: la negrita, más clara) */
			if (e->at & VT_INVERSO) {
				uint32_t k = fg;

				fg = bg, bg = k;
			}
			if (e->at & VT_TENUE)
				fg = mezcla(fg, bg, 110);
			if (e->at & VT_OCULTO)
				fg = bg;
			if (cursor && (t->forma_cursor <= 2)) {
				uint32_t k = fg;

				fg = bg, bg = k;
				if (fg == bg)
					bg = t->tinta, fg = t->fondo;
			}
			dibujo(e->c, ancho == 16 ? 16 : 8);
			if (e->at & VT_NEGRITA)
				for (py = 0; py < VT_AL; py++)
					for (px = ancho - 1; px > 0; px--)
						if (m[py][px - 1])
							m[py][px] = 1;
			if (e->at & VT_SUBRAYA)
				for (px = 0; px < ancho; px++)
					m[14][px] = 1;
			if (e->at & VT_TACHADO)
				for (px = 0; px < ancho; px++)
					m[8][px] = 1;
			if (cursor && (t->forma_cursor == 3 || t->forma_cursor == 4))
				for (px = 0; px < ancho; px++)
					m[14][px] = m[15][px] = 2;
			if (cursor && (t->forma_cursor == 5 || t->forma_cursor == 6))
				for (py = 0; py < VT_AL; py++)
					m[py][0] = m[py][1] = 2;
			p = lz + (size_t)(y0 + y * VT_AL) * (size_t)paso + (size_t)(x0 + x * VT_AN);
			for (py = 0; py < VT_AL; py++, p += paso)
				for (px = 0; px < ancho; px++) {
					/* (la cursiva: la mitad de arriba, un punto a la derecha) */
					int sx = (e->at & VT_CURSIVA) && py < 7 && px > 0 ? px - 1 : px;

					p[px] = m[py][sx] == 2 ? t->tinta : m[py][sx] ? fg : bg;
				}
		}
		/* lo que sobra a la derecha */
		{
			int xr = x0 + t->cols * VT_AN, py, px;

			for (py = 0; py < VT_AL && xr < an; py++)
				for (px = xr; px < an; px++)
					lz[(size_t)(y0 + y * VT_AL + py) * (size_t)paso + (size_t)px] = t->fondo;
		}
		if (*f0 < 0)
			*f0 = y;
		*f1 = y;
		hechas++;
	}
	cur_x = t->cx;
	cur_y = t->cursor_visible ? t->cy : -1;
	(void)cur_x;
	return hechas;
}

/* ---------------------------------------------------------------- las teclas */
static int pon(char *o, const char *s)
{
	int n = 0;

	while (s[n]) {
		o[n] = s[n];
		n++;
	}
	return n;
}

static int utf8(uint32_t c, char *o)
{
	if (c < 0x80)
		return o[0] = (char)c, 1;
	if (c < 0x800)
		return o[0] = (char)(0xC0 | c >> 6), o[1] = (char)(0x80 | (c & 63)), 2;
	if (c < 0x10000)
		return o[0] = (char)(0xE0 | c >> 12), o[1] = (char)(0x80 | ((c >> 6) & 63)), o[2] = (char)(0x80 | (c & 63)), 3;
	return o[0] = (char)(0xF0 | c >> 18), o[1] = (char)(0x80 | ((c >> 12) & 63)), o[2] = (char)(0x80 | ((c >> 6) & 63)), o[3] = (char)(0x80 | (c & 63)), 4;
}

int vt_tecla(const struct vt *t, uint32_t c, int mods, char *o)
{
	int n = 0, mod = 1 + ((mods & VT_M_MAYUS) ? 1 : 0) + ((mods & VT_M_ALT) ? 2 : 0) + ((mods & VT_M_CTRL) ? 4 : 0);
	static const char letra_cursor[] = "ABCD";

	if (c >= VT_T_ARRIBA && c <= VT_T_IZQUIERDA) {
		char l = letra_cursor[c == VT_T_ARRIBA ? 0 : c == VT_T_ABAJO ? 1 : c == VT_T_DERECHA ? 2 : 3];

		if (mod > 1) {
			n = pon(o, "\x1b[1;");
			n += pon_num(o + n, mod);
			o[n++] = l;
			return n;
		}
		o[0] = 0x1b, o[1] = t->teclas_app ? 'O' : '[', o[2] = l;
		return 3;
	}
	if (c == VT_T_INICIO || c == VT_T_FIN) {
		char l = c == VT_T_INICIO ? 'H' : 'F';

		if (mod > 1) {
			n = pon(o, "\x1b[1;");
			n += pon_num(o + n, mod);
			o[n++] = l;
			return n;
		}
		o[0] = 0x1b, o[1] = t->teclas_app ? 'O' : '[', o[2] = l;
		return 3;
	}
	if (c == VT_T_INSERT || c == VT_T_SUPR || c == VT_T_REPAG || c == VT_T_AVPAG || (c >= VT_T_F1 + 4 && c < VT_T_F1 + 12)) {
		static const int fn[8] = { 15, 17, 18, 19, 20, 21, 23, 24 };
		int k = c == VT_T_INSERT ? 2 : c == VT_T_SUPR ? 3 : c == VT_T_REPAG ? 5 : c == VT_T_AVPAG ? 6 : fn[c - VT_T_F1 - 4];

		n = pon(o, "\x1b[");
		n += pon_num(o + n, k);
		if (mod > 1) {
			o[n++] = ';';
			n += pon_num(o + n, mod);
		}
		o[n++] = '~';
		return n;
	}
	if (c >= VT_T_F1 && c < VT_T_F1 + 4) {
		if (mod > 1) {
			n = pon(o, "\x1b[1;");
			n += pon_num(o + n, mod);
			o[n++] = (char)('P' + (c - VT_T_F1));
			return n;
		}
		o[0] = 0x1b, o[1] = 'O', o[2] = (char)('P' + (c - VT_T_F1));
		return 3;
	}
	if (c == VT_T_TAB_ATRAS)
		return pon(o, "\x1b[Z");
	if (c == VT_T_TAB)
		return (mods & VT_M_MAYUS) ? pon(o, "\x1b[Z") : (o[0] = '\t', 1);
	if (c == VT_T_INTRO) {
		/* (Mayús o Alt con Intro: una línea nueva sin mandar, como en Claude Code) */
		if (mods & (VT_M_MAYUS | VT_M_ALT))
			return o[0] = 0x1b, o[1] = '\r', 2;
		return o[0] = '\r', 1;
	}
	if (c == VT_T_RETROCESO) {
		if (mods & VT_M_CTRL)
			return o[0] = 8, 1;
		if (mods & VT_M_ALT)
			return o[0] = 0x1b, o[1] = 127, 2;
		return o[0] = 127, 1;
	}
	if (c == VT_T_ESC)
		return o[0] = 0x1b, 1;
	if (c >= 0x110000)
		return 0;
	if (mods & VT_M_ALT)
		o[n++] = 0x1b;
	if ((mods & VT_M_CTRL) && ((c >= 'a' && c <= 'z') || (c >= '@' && c <= '_')))
		return o[n++] = (char)(c & 0x1F), n;
	if ((mods & VT_M_CTRL) && c == ' ')
		return o[n++] = 0, n;
	return n + utf8(c, o + n);
}

int vt_pega(const struct vt *t, const char *d, int n, char *o)
{
	int k = 0, i;

	if (t->pegado)
		k = pon(o, "\x1b[200~");
	for (i = 0; i < n; i++) {
		char c = d[i];

		if (c == '\r' && i + 1 < n && d[i + 1] == '\n')
			continue;
		if (c == '\n')
			c = '\r';
		/* (dentro de lo pegado no puede ir el final de lo pegado) */
		if (c == 0x1b && t->pegado)
			continue;
		o[k++] = c;
	}
	if (t->pegado)
		k += pon(o + k, "\x1b[201~");
	return k;
}
