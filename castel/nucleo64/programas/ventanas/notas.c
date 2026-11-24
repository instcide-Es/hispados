/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ventanas/notas.c - Hispa-64, etapa 8: NOTAS, el bloc
 * de notas (docs/64/HISPA-64.md, sección 14): el programa sencillo de la
 * oficina de Hispa-Pentium hecho con su capa «so» (oficina/comun/so.h, con
 * so_hispa64.c) y sus letras TrueType (la DejaVu Sans Mono, con suavizado).
 * Texto en UTF-8 (hasta 256 KB): escribir (con los acentos y la ñ del
 * teclado español), Intro, Retroceso, Supr, las flechas, Inicio y Fin, el
 * ratón para poner el cursor, la rueda; Ctrl+G guarda, Esc o cerrar la
 * ventana salen.
 *   NOTAS [ARCHIVO]     (si no se dice: C:\NOTAS.TXT, en el primer disco)
 */
#include <string.h>
#include "so.h"
#include "texto.h"
#include "lienzo.h"
#include "letras.h"

#define MAX_TEXTO (256 * 1024)
#define TAM (14 * 4)			/* 14 puntos (en cuartos) */
#define ALTO_LINEA 20
#define MARGEN 8
#define ABAJO 26

static char texto_[MAX_TEXTO + 1];
static u32 n_texto, cur, arriba_;	/* lo escrito, el cursor y la primera línea a la vista (octetos) */
static char archivo[600], aviso[200];
static int v, sucio = 1, cambiado, cara, ancho_letra;
static lienzo L;

static void avisa(const char *t)
{
	copia(aviso, t, sizeof aviso);
	dice("NOTAS: %s", t);
}

static u32 ini_linea(u32 p)
{
	while (p > 0 && texto_[p - 1] != '\n')
		p--;
	return p;
}

static u32 fin_linea(u32 p)
{
	while (p < n_texto && texto_[p] != '\n')
		p++;
	return p;
}

/* letras (de UTF-8) de a a b */
static int letras_entre(u32 a, u32 b)
{
	int n = 0;

	for (; a < b; a++)
		if ((texto_[a] & 0xC0) != 0x80)
			n++;
	return n;
}

/* el octeto de la letra número col de la línea que empieza en a */
static u32 en_columna(u32 a, int col)
{
	u32 f = fin_linea(a);

	while (a < f && col > 0) {
		a++;
		while (a < f && (texto_[a] & 0xC0) == 0x80)
			a++;
		col--;
	}
	return a;
}

static int lineas_visibles(void)
{
	return (L.al - ABAJO - 2 * MARGEN) / ALTO_LINEA;
}

/* que el cursor se vea */
static void al_cursor(void)
{
	u32 p;
	int n = 0;

	if (cur < arriba_)
		arriba_ = ini_linea(cur);
	for (p = arriba_; p < cur; p++)
		if (texto_[p] == '\n')
			n++;
	while (n >= lineas_visibles()) {
		arriba_ = fin_linea(arriba_) + 1;
		n--;
	}
}

static void titulo(void)
{
	char t[700];

	fmt(t, sizeof t, "%sNotas \xE2\x80\x94 %s", cambiado ? "* " : "", sin_carpeta(archivo));
	so_titulo(v, t);
}

static void pinta(void)
{
	u32 p = arriba_;
	int y = MARGEN, fila = 0;

	lz_sin_recorte(&L);
	lz_rect(&L, 0, 0, L.an, L.al - ABAJO, 0xFFFFFF);
	while (fila < lineas_visibles() && p <= n_texto) {
		u32 f = fin_linea(p);
		char linea[2048];
		u32 n = f - p < sizeof linea - 1 ? f - p : sizeof linea - 1;

		memcpy(linea, texto_ + p, n);
		linea[n] = 0;
		lz_texto(&L, cara, TAM, 0, MARGEN, y + 15, linea, 0x101418);
		if (cur >= p && cur <= f) {
			int x = MARGEN + letras_entre(p, cur) * ancho_letra;

			lz_rect(&L, x, y + 1, 2, ALTO_LINEA - 2, 0x000000);
		}
		y += ALTO_LINEA;
		fila++;
		if (f >= n_texto)
			break;
		p = f + 1;
	}
	/* abajo: el archivo, la línea y lo que se puede hacer */
	{
		char t[400];
		u32 q;
		int lin = 1;

		for (q = 0; q < cur; q++)
			if (texto_[q] == '\n')
				lin++;
		lz_rect(&L, 0, L.al - ABAJO, L.an, ABAJO, 0xE4E7EB);
		lz_raya_h(&L, 0, L.al - ABAJO, L.an, 0xA0A6AE);
		fmt(t, sizeof t, "%s  \xC2\xB7  línea %d  \xC2\xB7  Ctrl+G guarda, Esc sale  \xC2\xB7  %s", sin_carpeta(archivo), lin, aviso);
		lz_ui(&L, 8, L.al - 8, t, 0x20242A);
	}
	so_mapa(v, 0, 0, L.an, L.al, L.p, L.paso);
	sucio = 0;
}

static void mete(const char *s, u32 n)
{
	if (n_texto + n > MAX_TEXTO) {
		avisa("no cabe más");
		return;
	}
	memmove(texto_ + cur + n, texto_ + cur, n_texto - cur);
	memcpy(texto_ + cur, s, n);
	n_texto += n;
	cur += n;
	texto_[n_texto] = 0;
	if (!cambiado) {
		cambiado = 1;
		titulo();
	}
}

static void quita_(u32 a, u32 b)
{
	memmove(texto_ + a, texto_ + b, n_texto - b);
	n_texto -= b - a;
	texto_[n_texto] = 0;
	if (!cambiado) {
		cambiado = 1;
		titulo();
	}
}

static u32 letra_atras(u32 p)
{
	if (p == 0)
		return 0;
	p--;
	while (p > 0 && (texto_[p] & 0xC0) == 0x80)
		p--;
	return p;
}

static u32 letra_adelante(u32 p)
{
	if (p >= n_texto)
		return n_texto;
	p++;
	while (p < n_texto && (texto_[p] & 0xC0) == 0x80)
		p++;
	return p;
}

static void guarda(void)
{
	const char *e = "";
	void *f = so_crea(archivo, &e);

	if (!f || so_escribe(f, texto_, n_texto) || so_cierra(f)) {
		char t[200];

		fmt(t, sizeof t, "no se ha podido guardar: %s", f ? "no se puede escribir" : e);
		avisa(t);
		return;
	}
	cambiado = 0;
	{
		char t[200];

		fmt(t, sizeof t, "guardado (%u octetos)", n_texto);
		avisa(t);
	}
	titulo();
}

static void tecla(so_suceso *s)
{
	u32 t = s->tecla;

	aviso[0] = 0;
	if (t == 7 && (s->mods & SO_CON_CONTROL)) {	/* Ctrl+G */
		guarda();
	} else if (t == SO_ESC) {
		v = -v;
		return;
	} else if (t == SO_INTRO) {
		mete("\n", 1);
	} else if (t == SO_TAB) {
		mete("    ", 4);
	} else if (t == SO_RETROCESO) {
		if (cur > 0) {
			u32 a = letra_atras(cur);

			quita_(a, cur);
			cur = a;
		}
	} else if (t == SO_T_SUPR) {
		if (cur < n_texto)
			quita_(cur, letra_adelante(cur));
	} else if (t == SO_T_IZQUIERDA) {
		cur = letra_atras(cur);
	} else if (t == SO_T_DERECHA) {
		cur = letra_adelante(cur);
	} else if (t == SO_T_INICIO) {
		cur = ini_linea(cur);
	} else if (t == SO_T_FIN) {
		cur = fin_linea(cur);
	} else if (t == SO_T_ARRIBA || t == SO_T_ABAJO) {
		u32 a = ini_linea(cur);
		int col = letras_entre(a, cur);

		if (t == SO_T_ARRIBA && a > 0)
			cur = en_columna(ini_linea(a - 1), col);
		else if (t == SO_T_ABAJO && fin_linea(cur) < n_texto)
			cur = en_columna(fin_linea(cur) + 1, col);
	} else if (t >= 32 && t < SO_T_BASE && !(s->mods & SO_CON_CONTROL)) {
		char u[5];
		u32 n = 0;

		if (t < 0x80) {
			u[n++] = (char)t;
		} else if (t < 0x800) {
			u[n++] = (char)(0xC0 | (t >> 6));
			u[n++] = (char)(0x80 | (t & 63));
		} else if (t < 0x10000) {
			u[n++] = (char)(0xE0 | (t >> 12));
			u[n++] = (char)(0x80 | ((t >> 6) & 63));
			u[n++] = (char)(0x80 | (t & 63));
		} else {
			u[n++] = (char)(0xF0 | (t >> 18));
			u[n++] = (char)(0x80 | ((t >> 12) & 63));
			u[n++] = (char)(0x80 | ((t >> 6) & 63));
			u[n++] = (char)(0x80 | (t & 63));
		}
		mete(u, n);
	} else {
		return;
	}
	al_cursor();
	sucio = 1;
}

static void clic(so_suceso *s)
{
	int fila = (s->y - MARGEN) / ALTO_LINEA, col = (s->x - MARGEN + ancho_letra / 2) / (ancho_letra ? ancho_letra : 8);
	u32 p = arriba_;

	if (s->y >= L.al - ABAJO || fila < 0)
		return;
	while (fila > 0 && fin_linea(p) < n_texto) {
		p = fin_linea(p) + 1;
		fila--;
	}
	cur = en_columna(p, col < 0 ? 0 : col);
	sucio = 1;
}

int ofi_principal(const char *args)
{
	so_suceso s;
	u32 n = 0;
	u8 *d;
	const char *e;
	int an, al;

	letras_inicia();
	cara = letra_cara(FAM_MONO, 0, 0, NULL);
	ancho_letra = lz_ancho(cara, TAM, 0, "M");
	copia(archivo, args && args[0] ? args : "C:\\NOTAS.TXT", sizeof archivo);
	/* (sin comillas) */
	if (archivo[0] == '"') {
		memmove(archivo, archivo + 1, largo(archivo));
		if (largo(archivo) && archivo[largo(archivo) - 1] == '"')
			archivo[largo(archivo) - 1] = 0;
	}
	d = so_lee_archivo(archivo, &n, MAX_TEXTO, &e);
	if (d) {
		memcpy(texto_, d, n);
		n_texto = n;
		texto_[n] = 0;
		so_suelta(d);
		avisa("abierto");
	} else {
		avisa("nuevo");
	}
	so_pantalla(&an, &al);
	an = an * 6 / 10;
	al = al * 6 / 10;
	v = so_ventana("Notas", an, al);
	if (!v)
		return 1;
	if (!lz_nuevo(&L, an, al))
		return 1;
	titulo();
	dice("NOTAS: lista (%dx%d), %s, %u octetos", an, al, archivo, n_texto);
	while (v > 0) {
		if (sucio)
			pinta();
		if (!so_lee_suceso(v, &s, 1))
			continue;
		switch (s.clase) {
		case SO_TECLA: tecla(&s); break;
		case SO_BAJA: clic(&s); break;
		case SO_RUEDA:
			if (s.pasos > 0) {
				int k;

				for (k = 0; k < s.pasos * 3 && arriba_ > 0; k++)
					arriba_ = ini_linea(arriba_ - 1);
			} else if (s.pasos < 0) {
				int k;

				for (k = 0; k < -s.pasos * 3 && fin_linea(arriba_) < n_texto; k++)
					arriba_ = fin_linea(arriba_) + 1;
			}
			sucio = 1;
			break;
		case SO_TAMANO:
			if (s.x > 100 && s.y > 80) {
				lz_suelta(&L);
				if (!lz_nuevo(&L, s.x, s.y))
					return 1;
				al_cursor();
				sucio = 1;
			}
			break;
		case SO_FOCO: sucio = 1; break;
		case SO_CERRAR:
		case SO_SALE: v = -v; break;
		}
	}
	so_cierra_ventana(-v);
	dice("NOTAS: fuera");
	return 0;
}
