/* f_csv.c - Cuentas 2: el texto separado (.CSV). Una fila por línea; las
 * celdas, con «;» (lo de siempre en español), «,» o el tabulador; entre
 * comillas la que lleve el separador, comillas o saltos de línea (y las
 * suyas, dobles). Al leer, el separador y la codificación se adivinan si no
 * se dicen: el que más aparece fuera de comillas en las primeras líneas, y
 * UTF-8 si lo es (con su marca delante o sin ella); si no, Windows-1252.
 *
 * (Sin decir la codificación y sin ser UTF-8: si hay más octetos de 0x80 a
 * 0xA5 que de 0xC0 para arriba, es la 437 de Hispa —la del Bloc—; si no, la 1252.)
 * Con «;» o el tabulador, cada celda se lee como si se tecleara en español
 * (1.234,5; 22/11/2026; =SUMA(A1:A3)). Con «,» el archivo es de los de fuera
 * (los de Excel en inglés): los números con punto decimal y las fórmulas en
 * inglés. Al escribir, lo que se ve en cada celda (o, si se pide, las
 * fórmulas); con «,», los números tal cual, con punto, para que se lean
 * igual en cualquier parte. Fin de línea CR LF.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "numeros.h"

static const u16 T1252[32] = { 0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D, 0x017D, 0x8F,
			       0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178 };

static int es_utf8(const u8 *d, u32 n)
{
	u32 i = 0;

	while (i < n) {
		u8 c = d[i];
		u32 k;

		if (c < 0x80) {
			i++;
			continue;
		}
		k = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : 9;
		if (k == 9 || (c & 0xFE) == 0xC0)
			return 0;
		if (i + k >= n + (i + k == n))
			return 0;
		{
			u32 j;

			for (j = 1; j <= k; j++)
				if (i + j >= n || (d[i + j] & 0xC0) != 0x80)
					return 0;
		}
		i += k + 1;
	}
	return 1;
}

/* todo a UTF-8 (en b) */
static void a_utf8(const u8 *d, u32 n, int cod, bufer *b)
{
	u32 i;

	if (cod == CSV_UTF8) {
		if (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF) {
			d += 3;
			n -= 3;
		}
		buf_pon(b, d, n);
		return;
	}
	for (i = 0; i < n; i++) {
		u8 c = d[i];

		if (cod == CSV_437)
			buf_u32(b, de437(c));
		else if (c >= 0x80 && c < 0xA0)
			buf_u32(b, T1252[c - 0x80]);
		else
			buf_u32(b, c);
	}
}

/* el separador que más aparece fuera de comillas en las primeras líneas */
static char adivina_sep(const u8 *d, u32 n)
{
	u32 i, cuenta[3] = { 0, 0, 0 }, lineas = 0;
	int com = 0;
	static const char seps[3] = { ';', ',', '\t' };

	for (i = 0; i < n && lineas < 20; i++) {
		u8 c = d[i];

		if (c == '"')
			com = !com;
		else if (!com) {
			if (c == ';')
				cuenta[0]++;
			else if (c == ',')
				cuenta[1]++;
			else if (c == '\t')
				cuenta[2]++;
			else if (c == '\n')
				lineas++;
		}
	}
	/* (a igualdad, el punto y coma) */
	if (cuenta[2] > cuenta[0] && cuenta[2] >= cuenta[1])
		return seps[2];
	if (cuenta[1] > cuenta[0])
		return seps[1];
	return seps[0];
}

libro *csv_lee(const u8 *d0, u32 n0, const csv_opc *o, char *err, u32 cabe)
{
	bufer t = { 0 }, cel = { 0 };
	int cod = o ? o->cod : CSV_AUTO;
	char sep;
	libro *l;
	hoja *h;
	u32 i, f = 0, c = 0;
	int com = 0, hubo_com = 0;
	const u8 *d;
	u32 n;
	cu_local antes;

	if (cod == CSV_AUTO) {
		cod = es_utf8(d0, n0) ? CSV_UTF8 : CSV_1252;
		if (cod == CSV_1252) {
			/* ¿la 437 de Hispa (á í ó ú ñ Ñ é ü, de 0x80 a 0xA5) o la 1252 (de 0xC0 para arriba)? */
			u32 i, bajos = 0, altos = 0;

			for (i = 0; i < n0; i++) {
				if (d0[i] >= 0x80 && d0[i] <= 0xA5)
					bajos++;
				else if (d0[i] >= 0xC0)
					altos++;
			}
			if (bajos > altos)
				cod = CSV_437;
		}
	}
	a_utf8(d0, n0, cod, &t);
	if (t.mal) {
		buf_suelta(&t);
		copia(err, "No hay memoria", cabe);
		return NULL;
	}
	d = t.d ? t.d : (const u8 *)"";
	n = t.n;
	sep = o && o->sep ? o->sep : adivina_sep(d, n);
	l = lib_nuevo(1);
	if (!l) {
		buf_suelta(&t);
		copia(err, "No hay memoria", cabe);
		return NULL;
	}
	h = l->h[0];
	antes = l->loc;
	if (sep == ',')
		l->loc = CU_ARCHIVO;
	for (i = 0; i <= n; i++) {
		u8 k = i < n ? d[i] : 0;
		int fin_celda = 0, fin_linea = 0;

		if (com) {
			if (i >= n) {
				fin_celda = fin_linea = 1;
			} else if (k == '"') {
				if (i + 1 < n && d[i + 1] == '"') {
					buf_c(&cel, '"');
					i++;
				} else
					com = 0;
				continue;
			} else {
				buf_c(&cel, k);
				continue;
			}
		} else if (i >= n) {
			fin_celda = fin_linea = 1;
		} else if (k == '"' && cel.n == 0 && !hubo_com) {
			com = hubo_com = 1;
			continue;
		} else if (k == (u8)sep) {
			fin_celda = 1;
		} else if (k == '\n') {
			fin_celda = fin_linea = 1;
		} else if (k == '\r') {
			continue;
		} else {
			buf_c(&cel, k);
			continue;
		}
		if (fin_celda) {
			buf_c(&cel, 0);
			if (cel.n > 1 && f < CU_FILAS && c < CU_COLS) {
				char e[200];
				const char *s = (const char *)cel.d;

				/* (lo que venía entre comillas y parece número, sigue siendo lo que es) */
				if (lib_pon_entrada(l, h, f, c, s, e, sizeof e))
					lib_pon_texto(l, h, f, c, s);
			}
			cel.n = 0;
			hubo_com = 0;
			c++;
			if (fin_linea) {
				if (i < n || c > 1)
					f++;
				c = 0;
			}
		}
		if (cel.mal || f >= CU_FILAS)
			break;
	}
	l->loc = antes;
	buf_suelta(&cel);
	buf_suelta(&t);
	return l;
}

/* una letra en la codificación de salida */
static void pon_letra(bufer *b, u32 c, int cod)
{
	u32 k;

	if (cod == CSV_UTF8 || cod == CSV_AUTO) {
		buf_u32(b, c);
		return;
	}
	if (cod == CSV_437) {
		buf_c(b, a437(c));
		return;
	}
	if (c < 0x80 || (c >= 0xA0 && c < 0x100)) {
		buf_c(b, (u8)c);
		return;
	}
	for (k = 0; k < 32; k++)
		if (T1252[k] == c) {
			buf_c(b, (u8)(0x80 + k));
			return;
		}
	buf_c(b, '?');
}

static void pon_texto(bufer *b, const char *s, int cod)
{
	u32 i = 0, n = largo(s);

	while (i < n) {
		u32 c;

		i += (u32)utf8_lee((const u8 *)s + i, n - i, &c);
		pon_letra(b, c, cod);
	}
}

int csv_escribe(libro *l, const csv_opc *o, bufer *b, char *err, u32 cabe)
{
	hoja *h = lib_hoja(l, o ? o->hoja : l->activa);
	char sep = o && o->sep ? o->sep : ';';
	int cod = o && o->cod != CSV_AUTO ? o->cod : CSV_UTF8;
	u32 uf, uc, f, c;
	cu_local antes = l->loc;

	if (!h) {
		copia(err, "No hay hoja", cabe);
		return 1;
	}
	if (cod == CSV_UTF8 && o && o->bom)
		buf_txt(b, "\xEF\xBB\xBF");
	if (sep == ',')
		l->loc = CU_ARCHIVO;
	hj_usado(h, &uf, &uc);
	if (uf != CU_SIN_FILA)
		for (f = 0; f <= uf; f++) {
			for (c = 0; c <= uc; c++) {
				const celda *x = hj_celda(h, f, c);
				char t[2000];

				if (c)
					buf_c(b, (u8)sep);
				t[0] = 0;
				if (x && (x->v.t != V_VACIO || x->f)) {
					if (o && o->formulas && x->f)
						lib_texto_edicion(l, h, f, c, t, sizeof t);
					else if (sep == ',' && x->v.t == V_NUM && !fmt_es_fecha(lib_fmt(l, lib_estilo(l, x->estilo)->numfmt)))
						num_a_texto_c(x->v.n, t, sizeof t);
					else
						lib_texto_visible(l, x, t, sizeof t, NULL);
				}
				if (strchr(t, sep) || strchr(t, '"') || strchr(t, '\n') || strchr(t, '\r') || (t[0] == ' ' || (t[0] && t[largo(t) - 1] == ' '))) {
					const char *p;

					buf_c(b, '"');
					for (p = t; *p; p++)
						if (*p == '"')
							buf_txt(b, "\"\"");
						else {
							char u[2] = { *p, 0 };

							if ((u8)*p < 0x80)
								buf_txt(b, u);
							else {
								/* (una letra entera) */
								u32 cc;
								int k = utf8_lee((const u8 *)p, largo(p), &cc);

								pon_letra(b, cc, cod);
								p += k - 1;
							}
						}
					buf_c(b, '"');
				} else
					pon_texto(b, t, cod);
			}
			buf_txt(b, "\r\n");
		}
	l->loc = antes;
	if (b->mal) {
		copia(err, "No hay memoria para guardar", cabe);
		return 1;
	}
	return 0;
}
