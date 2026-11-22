/* orto.c - la oficina de 32 bits: el corrector ortográfico (oficina/comun/orto.h).
 * Es programas/comun/ortografia.inc (Hispa-386) pasado a C paso por paso:
 * el mismo recorrido del autómata, las mismas letras que valen en cada
 * sitio y la misma nota de las propuestas. Lo de dentro trabaja en la
 * página 437 de Hispa (la del diccionario); lo de fuera, en UTF-32.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "orto.h"
#include "texto.h"

#define CAB 32
#define TOPE 400000
#define MAX_DIC (16u * 1024 * 1024)
#define MAX_PROPIAS 32768

static u8 *dic;
static u32 n_arcos, largo_max;
static int hay;
static u8 t_min[256], t_may[256], t_base[256];
static int tablas_hechas;

/* la palabra que se mira */
static u8 alt[ORTO_LARGO + 1][3];	/* por cada letra, las que valen en su sitio (0: ninguna) */
static u32 w_n;
static int modo;			/* 0 tal cual, 1 Primera mayúscula, 2 TODA */
static i32 tope;

/* las propuestas */
static u8 ent[ORTO_LARGO + 3];		/* la tecleada, en minúsculas, desde ent[1] */
static u32 ent_n;
static u8 sal[ORTO_LARGO + 3];
static u8 s_pal[ORTO_SUG][ORTO_LARGO + 1];
static u8 s_pts[ORTO_SUG];
static u32 s_n;
static u32 pasos;

/* las del usuario y las omitidas: textos de la 437, una por línea (CR LF) */
static u8 *propias, *omitidas;
static u32 n_propias, n_omitidas;

static void haz_tablas(void)
{
	static const u8 a_min[][2] = { { 165, 164 }, { 144, 130 }, { 154, 129 }, { 128, 135 }, { 142, 132 }, { 143, 134 }, { 146, 145 },
		{ 153, 148 }, { 0xB6, 160 }, { 0xCF, 161 }, { 0xD3, 162 }, { 0xD7, 163 } };
	static const u8 a_may[][2] = { { 164, 165 }, { 130, 144 }, { 129, 154 }, { 160, 'A' }, { 161, 'I' }, { 162, 'O' }, { 163, 'U' },
		{ 135, 128 }, { 132, 142 }, { 134, 143 }, { 145, 146 }, { 148, 153 } };
	static const u8 a_base[][2] = { { 160, 'a' }, { 130, 'e' }, { 161, 'i' }, { 162, 'o' }, { 163, 'u' }, { 129, 'u' } };
	u32 i;

	for (i = 0; i < 256; i++) {
		t_min[i] = (u8)(i >= 'A' && i <= 'Z' ? i + 32 : i);
		t_may[i] = (u8)(i >= 'a' && i <= 'z' ? i - 32 : i);
		t_base[i] = (u8)i;
	}
	for (i = 0; i < sizeof a_min / 2; i++)
		t_min[a_min[i][0]] = a_min[i][1];
	for (i = 0; i < sizeof a_may / 2; i++)
		t_may[a_may[i][0]] = a_may[i][1];
	for (i = 0; i < sizeof a_base / 2; i++)
		t_base[a_base[i][0]] = a_base[i][1];
	tablas_hechas = 1;
}

static u32 arco_n(u32 i)
{
	const u8 *p = dic + CAB + 4 * i;

	return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}

/* ---- el diccionario ---- */
int orto_carga(u8 *d, u32 n)
{
	u32 na, i, suma = 0, ultimo = 0;

	if (!tablas_hechas)
		haz_tablas();
	orto_suelta();
	if (!d)
		return 0;
	if (n < CAB + 4 || n > MAX_DIC || memcmp(d, "DICCIO1\x1a", 8))
		goto mal;
	na = d[8] | (u32)d[9] << 8 | (u32)d[10] << 16 | (u32)d[11] << 24;
	if (!na || na > (MAX_DIC - CAB) / 4 || na * 4 + CAB != n)
		goto mal;
	if (!d[20] || d[20] > ORTO_LARGO)
		goto mal;
	for (i = 0; i < na; i++) {
		const u8 *p = d + CAB + 4 * i;
		u32 a = p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;

		suma += a;
		if (!(a & 0xFF) || (a >> 10) >= na)
			goto mal;
		ultimo = a;
	}
	if (!(ultimo & 0x200))
		goto mal;
	if (suma != (d[16] | (u32)d[17] << 8 | (u32)d[18] << 16 | (u32)d[19] << 24))
		goto mal;
	dic = d;
	n_arcos = na;
	largo_max = d[20];
	hay = 1;
	return 1;
mal:
	so_suelta(d);
	return 0;
}

int orto_carga_archivo(const char *ruta)
{
	u32 n;
	u8 *d = so_lee_archivo(ruta, &n, MAX_DIC, NULL);

	return d ? orto_carga(d, n) : 0;
}

void orto_suelta(void)
{
	so_suelta(dic);
	dic = NULL;
	hay = 0;
	n_arcos = 0;
}

int orto_hay(void)
{
	return hay;
}

/* ---- preparar la palabra (de UTF-32 a la 437, y lo que vale en cada sitio) ---- */
/* la letra k es una mayúscula: vale también por su minúscula y, si es A, I, O
 * o U, por la acentuada; si es Á, Í, Ó o Ú, por la letra sin acento */
static void abre(u32 k)
{
	u8 c = alt[k][0], b = 0;

	alt[k][1] = t_min[c];
	switch (c) {
	case 0xB6: b = 'A'; break;
	case 0xCF: b = 'I'; break;
	case 0xD3: b = 'O'; break;
	case 0xD7: b = 'U'; break;
	case 'A': b = 160; break;
	case 'I': b = 161; break;
	case 'O': b = 162; break;
	case 'U': b = 163; break;
	}
	alt[k][2] = b;
}

/* 1 si lleva cifras (no se juzga) */
static int prepara(const u8 *w, u32 n)
{
	u32 k, mayus = 0, minus = 0;
	int cifras = 0;

	w_n = n;
	for (k = 0; k < n; k++) {
		u8 c = w[k];

		alt[k][0] = c;
		alt[k][1] = alt[k][2] = 0;
		if (c >= '0' && c <= '9')
			cifras = 1;
		if (c != t_min[c])
			mayus++;
		if (c != t_may[c])
			minus++;
	}
	modo = 0;
	if (mayus == n) {
		modo = 2;
		for (k = 0; k < n; k++)
			abre(k);
	} else if (mayus == 1 && minus + 1 == n && w[0] != t_min[w[0]]) {
		modo = 1;
		abre(0);
	}
	return cifras;
}

/* de UTF-32 a la 437; 0 si no cabe o no es una palabra que se mire */
static int a_437(const u32 *p, u32 n, u8 *w)
{
	u32 k;

	if (n < 1 || n > ORTO_LARGO)
		return 0;
	for (k = 0; k < n; k++)
		w[k] = a437(p[k]);
	return 1;
}

/* el arco de la letra l en el nodo (que empieza en el arco «nodo»); 0 si no está */
static u32 arco(u32 nodo, u8 l)
{
	u32 i;

	for (i = nodo; i < n_arcos; i++) {
		u32 a = arco_n(i);
		u8 v = (u8)a;

		if (v == l)
			return a;
		if (v > l || (a & 0x200))
			return 0;
	}
	return 0;
}

/* ¿desde el nodo se llega justo al final de la palabra por letras que valen? */
static int casa(i32 nodo, u32 k)
{
	int j;

	if (nodo < 0 || --tope < 0)
		return 0;
	for (j = 0; j < 3; j++) {
		u8 l = alt[k][j];
		u32 a;

		if (!l)
			continue;
		a = arco((u32)nodo, l);
		if (!a)
			continue;
		if (k + 1 == w_n) {
			if (a & 0x100)
				return 1;
			continue;
		}
		if (casa((a >> 10) ? (i32)(a >> 10) : -1, k + 1))
			return 1;
	}
	return 0;
}

/* ¿está la palabra preparada en la lista? */
static int en_lista(const u8 *l, u32 n)
{
	u32 i = 0;

	while (i < n) {
		u32 k = 0;
		int vale = 1;

		for (; i < n && l[i] != 13 && l[i] != 10; i++, k++) {
			u8 c = l[i];

			if (k >= w_n || (c != alt[k][0] && c != alt[k][1] && c != alt[k][2]))
				vale = 0;
		}
		if (vale && k == w_n)
			return 1;
		while (i < n && (l[i] == 13 || l[i] == 10))
			i++;
	}
	return 0;
}

int orto_bien(const u32 *p, u32 n)
{
	u8 w[ORTO_LARGO + 1];

	if (!hay || n < 2 || n > ORTO_LARGO)
		return 1;
	if (!a_437(p, n, w) || prepara(w, n))
		return 1;
	if (en_lista(propias, n_propias) || en_lista(omitidas, n_omitidas))
		return 1;
	if (n > largo_max)
		return 0;
	tope = TOPE;
	return casa(0, 0);
}

/* ---- las propuestas ---- */
static int es_par(u8 a, u8 b)
{
	static const u8 pares[][2] = { { 'b', 'v' }, { 'g', 'j' }, { 'c', 'z' }, { 'c', 's' }, { 's', 'z' }, { 's', 'x' }, { 'm', 'n' },
		{ 'q', 'k' }, { 'c', 'k' }, { 'c', 'q' }, { 'y', 'i' }, { 'n', 164 } };
	u32 i;

	for (i = 0; i < sizeof pares / 2; i++)
		if ((pares[i][0] == a && pares[i][1] == b) || (pares[i][0] == b && pares[i][1] == a))
			return 1;
	return 0;
}

/* el nodo al que lleva el arco (-1 ninguno) y, en el bit 9 de bp, si con él se acaba una palabra */
static u32 hijo(u32 a, u32 bp, i32 *nodo)
{
	bp &= ~0x200u;
	if (a & 0x100)
		bp |= 0x200;
	*nodo = (a >> 10) ? (i32)(a >> 10) : -1;
	return bp;
}

static u32 cuesta_poner(u32 c, u32 k, u32 bp)
{
	u8 l = sal[c];

	if (l == 'h')
		goto poco;
	if (c) {
		u8 an = sal[c - 1];

		if (l == an || (l == 'c' && an == 'x') || (l == 'c' && an == 's') || (l == 'u' && an == 'g') || (l == 'u' && an == 'q'))
			goto poco;
	}
	bp += 5;
	if (k == 1)
		bp++;
	return bp | 0x100;
poco:
	return (bp + 2) | 0x100;
}

static u32 cuesta_quitar(u32 k, u32 bp)
{
	u8 l = ent[k];

	if (l == 'h' || (k > 1 && l == ent[k - 1]))
		return (bp + 2) | 0x100;
	bp += 5;
	if (k == 1)
		bp++;
	return bp | 0x100;
}

/* la candidata sal (c letras, nota bp) es una palabra: entre las mejores */
static void apunta(u32 c, u32 bp)
{
	u8 nota = (u8)bp;
	u32 j, p;

	if (!nota || !c || c > ORTO_LARGO)
		return;
	sal[c] = 0;
	for (j = 0; j < s_n; j++)
		if (!memcmp(s_pal[j], sal, c + 1)) {
			if (nota >= s_pts[j])
				return;
			/* (estaba con peor nota: se quita de ahí) */
			for (; j + 1 < s_n; j++) {
				s_pts[j] = s_pts[j + 1];
				memcpy(s_pal[j], s_pal[j + 1], ORTO_LARGO + 1);
			}
			s_n--;
			break;
		}
	for (p = 0; p < s_n && s_pts[p] <= nota; p++)
		;
	if (p >= ORTO_SUG)
		return;
	for (j = s_n < ORTO_SUG ? s_n : ORTO_SUG - 1; j > p; j--) {
		s_pts[j] = s_pts[j - 1];
		memcpy(s_pal[j], s_pal[j - 1], ORTO_LARGO + 1);
	}
	s_pts[p] = nota;
	memcpy(s_pal[p], sal, c + 1);
	if (s_n < ORTO_SUG)
		s_n++;
}

/* el paso de la búsqueda (ort_s_rec): nodo (-1 ninguno), k la letra de la
 * tecleada por la que se va (la primera es la 1; ent_n, la de detrás de la
 * última), c las letras de la candidata, bp la nota (octeto bajo), el bit 8
 * (la corrección ya está gastada) y el 9 (la candidata es una palabra) */
static void rec(i32 nodo, u32 k, u32 c, u32 bp)
{
	static const u8 grupos[][4] = { { 'l', 'l', 'y', 0 }, { 'y', 0, 'l', 'l' }, { 'k', 0, 'q', 'u' }, { 'q', 'u', 'k', 0 },
		{ 'x', 0, 'c', 'c' }, { 'c', 'c', 'x', 0 } };
	u32 b, g;
	i32 h;

	if (--tope < 0)
		return;
	pasos++;
	if (k == ent_n && (bp & 0x200))
		apunta(c, bp);
	if (c > ORTO_LARGO + 1)
		return;
	if (nodo >= 0) {
		u32 i;

		for (i = (u32)nodo; i < n_arcos; i++) {
			u32 a = arco_n(i);

			sal[c] = (u8)a;
			b = hijo(a, bp, &h);
			if (k < ent_n) {
				u8 l = (u8)a, t = ent[k];

				if (l == t)
					b += 0;
				else if (t_base[l] == t_base[t])
					b++;
				else if (c == 0 && t_base[t_min[l]] == t_base[t])
					b++;	/* (la mayúscula de un nombre propio) */
				else {
					if (b & 0x100)
						goto otro;
					if (es_par(l, t))
						b += 2;
					else {
						b += 6;
						if (k == 1)
							b++;
					}
					b |= 0x100;
				}
				rec(h, k + 1, c + 1, b);
			}
			/* falta una letra: la del diccionario */
			if (bp & 0x100)
				goto otro;
			b = hijo(a, bp, &h);
			b = cuesta_poner(c, k, b);
			rec(h, k, c + 1, b);
otro:
			if (a & 0x200)
				break;
		}
	}
	if ((bp & 0x100) || k >= ent_n)
		return;
	/* sobra una letra: la tecleada */
	rec(nodo, k + 1, c, cuesta_quitar(k, bp));
	if (nodo < 0)
		return;
	/* dos vecinas trocadas */
	if (k + 2 <= ent_n && ent[k] != ent[k + 1]) {
		u8 x = ent[k];

		ent[k] = ent[k + 1];
		ent[k + 1] = x;
		rec(nodo, k, c, (bp + 4) | 0x100);
		ent[k + 1] = ent[k];
		ent[k] = x;
	}
	/* los grupos que se confunden («ll» por «y»...), cambiados en la tecleada */
	for (g = 0; g < sizeof grupos / 4; g++) {
		const u8 *q = grupos[g];

		if (q[0] != ent[k])
			continue;
		b = (bp + 2) | 0x100;
		if (q[1]) {
			u8 x;

			if (k + 2 > ent_n || q[1] != ent[k + 1])
				continue;
			x = ent[k + 1];
			ent[k + 1] = q[2];
			rec(nodo, k + 1, c, b);
			ent[k + 1] = x;
		} else {
			u8 x = ent[k - 1], y = ent[k];

			ent[k - 1] = q[2];
			ent[k] = q[3];
			rec(nodo, k - 1, c, b);
			ent[k - 1] = x;
			ent[k] = y;
		}
	}
}

u32 orto_pasos(void)
{
	return pasos;
}

int orto_sugiere(const u32 *p, u32 n, u32 sug[][ORTO_LARGO + 1], int max)
{
	u8 w[ORTO_LARGO + 1];
	u32 k, j, puestas = 0;

	s_n = 0;
	pasos = 0;
	if (!hay || n < 2 || n > ORTO_LARGO || n > largo_max + 1 || max <= 0)
		return 0;
	if (max > ORTO_SUG)
		max = ORTO_SUG;
	if (!a_437(p, n, w) || prepara(w, n))
		return 0;
	for (k = 0; k < n; k++)
		ent[k + 1] = t_min[w[k]];
	ent[0] = 0;
	ent_n = n + 1;
	tope = TOPE;
	rec(0, 1, 0, 0);
	/* con las mayúsculas de la tecleada */
	for (j = 0; j < s_n && puestas < (u32)max; j++) {
		u32 *d = sug[puestas], q, m;

		for (m = 0; s_pal[j][m]; m++) {
			u32 u = de437(s_pal[j][m]);

			if (modo == 2 || (modo == 1 && m == 0))
				u = mayuscula(u);
			d[m] = u;
		}
		d[m] = 0;
		/* que no sea la tecleada ni una ya puesta */
		if (m == n && !memcmp(d, p, n * 4))
			continue;
		for (q = 0; q < puestas; q++) {
			u32 r = 0;

			while (sug[q][r] == d[r] && d[r])
				r++;
			if (sug[q][r] == d[r])
				break;
		}
		if (q < puestas)
			continue;
		puestas++;
	}
	return (int)puestas;
}

/* ---- las listas ---- */
static int apunta_en(u8 **l, u32 *n, const u32 *p, u32 m)
{
	u8 w[ORTO_LARGO + 1];
	u32 k;
	u8 *nl;

	if (!tablas_hechas)
		haz_tablas();
	if (m < 1 || m > ORTO_LARGO)
		return -1;
	for (k = 0; k < m; k++)
		if (p[k] <= 32 || p[k] >= 0x110000)
			return -1;
	a_437(p, m, w);
	prepara(w, m);
	if (en_lista(*l, *n))
		return 0;
	if (*n + m + 2 > MAX_PROPIAS)
		return -1;
	nl = so_repide(*l, *n + m + 2);
	if (!nl)
		return -1;
	memcpy(nl + *n, w, m);
	nl[*n + m] = 13;
	nl[*n + m + 1] = 10;
	*l = nl;
	*n += m + 2;
	return 0;
}

int orto_propia(const u32 *p, u32 n)
{
	return apunta_en(&propias, &n_propias, p, n);
}

int orto_omite(const u32 *p, u32 n)
{
	return apunta_en(&omitidas, &n_omitidas, p, n);
}

void orto_olvida_omitidas(void)
{
	so_suelta(omitidas);
	omitidas = NULL;
	n_omitidas = 0;
}

char *orto_propias(u32 *n)
{
	char *d = so_pide(n_propias * 2 + 1);

	*n = 0;
	if (!d)
		return NULL;
	*n = n_propias ? de437_a_utf8(propias, n_propias, d, n_propias * 2 + 1) : 0;
	d[*n] = 0;
	return d;
}

void orto_propias_poner(const char *t, u32 n)
{
	u32 i = 0;

	so_suelta(propias);
	propias = NULL;
	n_propias = 0;
	while (i < n) {
		u32 w[ORTO_LARGO + 1], k = 0, c;
		int largo_ = 0;

		while (i < n && (u8)t[i] <= 32)
			i++;
		while (i < n && (u8)t[i] > 32) {
			i += (u32)utf8_lee((const u8 *)t + i, n - i, &c);
			if (k < ORTO_LARGO)
				w[k++] = c;
			else
				largo_ = 1;
		}
		if (k && !largo_)
			orto_propia(w, k);
	}
}

/* ---- las palabras del texto ---- */
int orto_de_palabra(u32 c)
{
	return c < 0x110000 && es_letra(c);
}

int orto_palabra_en(const u32 *t, u32 n, u32 o, u32 *ini, u32 *fin)
{
	u32 a = o, b = o;

	if (o > n)
		return 0;
	if ((o >= n || !orto_de_palabra(t[o])) && o > 0 && orto_de_palabra(t[o - 1]))
		a = b = o - 1;
	if (a >= n || !orto_de_palabra(t[a]))
		return 0;
	while (a > 0 && orto_de_palabra(t[a - 1]))
		a--;
	while (b < n && orto_de_palabra(t[b]))
		b++;
	*ini = a;
	*fin = b;
	return 1;
}

u32 orto_faltas(const u32 *t, u32 n, u8 *marcas, u32 no_hasta)
{
	u32 i = 0, cuantas = 0;

	memset(marcas, 0, n);
	if (!hay)
		return 0;
	while (i < n) {
		u32 j;

		if (!orto_de_palabra(t[i])) {
			i++;
			continue;
		}
		for (j = i; j < n && orto_de_palabra(t[j]); j++)
			;
		if (j != no_hasta && !orto_bien(t + i, j - i)) {
			marcas[i] = 1;
			if (j - i > 1)
				memset(marcas + i + 1, 2, j - i - 1);
			cuantas++;
		}
		i = j;
	}
	return cuantas;
}
