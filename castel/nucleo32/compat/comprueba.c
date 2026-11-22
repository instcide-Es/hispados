/* comprueba.c - (COMPAT) comprobar un programa o una carpeta: corre «WIN32
 * COMPRUEBA "ruta" /MAQUINA» con la salida a un archivo (la de las máquinas,
 * con las rutas, los tamaños y las sumas, desde win32 1.3; un WIN32 de antes
 * dice «/MAQUINA: no se encuentra» y sigue con lo de las personas: se lee
 * igual), lo lee (informe.c) y, si se pide y WIN32 no lo ha dado, mide cada
 * archivo y saca su SHA-256 (para el informe que se manda). Y las versiones
 * de lo instalado (\HISPA\PAQUETES\*.PQI).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "plat.h"
#include "sha256.h"
#include "texto.h"

void (*comprueba_espera)(void *u);
void *comprueba_u;

static void version_de(const char *paquete, char *d, u32 cabe)
{
	char c[RUTA_MAX], r[RUTA_MAX], *t;
	const char *p;

	copia(d, "?", cabe);
	plat_carpeta_paquetes(c, sizeof c);
	fmt(r, sizeof r, "%s\\%s.PQI", c, paquete);
	t = lee_texto(r, NULL, 1u << 20);
	if (!t)
		return;
	p = busca(t, "\nversion=");
	if (p) {
		u32 j = 0;

		p += 9;
		while (*p && *p != '\r' && *p != '\n' && j + 1 < cabe && j < 20) {
			char ch = *p++;

			if ((ch >= '0' && ch <= '9') || ch == '.' || (ch >= 'a' && ch <= 'z'))
				d[j++] = ch;
		}
		d[j] = 0;
	}
	so_suelta(t);
}

void version_win32(char *d, u32 cabe) { version_de("WIN32", d, cabe); }
void version_hispa(char *d, u32 cabe) { version_de("BASE", d, cabe); }

/* el tamaño y la suma de un archivo, a trozos */
static int suma(const char *ruta, u32 *tam, char sha[65])
{
	void *f = plat_abre(ruta, tam);
	u8 *b, r[32];
	sha256 s;
	u32 p = 0;

	sha[0] = 0;
	if (!f)
		return -1;
	b = so_pide(65536);
	if (!b) {
		plat_cierra(f);
		return -1;
	}
	sha_empieza(&s);
	while (p < *tam) {
		u32 n = *tam - p < 65536 ? *tam - p : 65536;

		if (plat_lee_en(f, p, b, n) != (i32)n) {
			so_suelta(b);
			plat_cierra(f);
			return -1;
		}
		sha_anade(&s, b, n);
		p += n;
		if (comprueba_espera && !(p & 0xFFFFF))
			comprueba_espera(comprueba_u);
	}
	sha_acaba(&s, r);
	sha_hex(r, sha);
	so_suelta(b);
	plat_cierra(f);
	return 0;
}

/* busca «nombre» en la carpeta c (y en las de dentro, hasta tres niveles,
 * como COMPRUEBA): su ruta entera a d */
static int busca_en(const char *c, const char *nombre, int nivel, char *d, u32 cabe)
{
	void *h = so_abre_carpeta(c);
	so_entrada e;
	int hay = 0;

	if (!h)
		return 0;
	while (!hay && so_lee_carpeta(h, &e) == 1) {
		char r[RUTA_MAX];

		if (iguales(e.nombre, ".") || iguales(e.nombre, ".."))
			continue;
		une_ruta(r, sizeof r, c, e.nombre);
		if (e.carpeta) {
			if (nivel < 3)
				hay = busca_en(r, nombre, nivel + 1, d, cabe);
		} else if (iguales_sin_caja(e.nombre, nombre) || iguales_sin_caja(e.corto, nombre)) {
			copia(d, r, cabe);
			hay = 1;
		}
	}
	so_cierra_carpeta(h);
	return hay;
}

int comprueba(const char *ruta, c_informe *i, int con_sumas, char *error, u32 cabe)
{
	char c[RUTA_MAX], sal[RUTA_MAX], args[RUTA_MAX + 40], *t;
	u32 n;
	int cod = 0, k, es_carpeta = so_existe(ruta) == 2;

	inf_vacia(i);
	if (!so_existe(ruta)) {
		fmt(error, cabe, "No se encuentra %s.", ruta);
		return -1;
	}
	if (hist_carpeta(c, sizeof c)) {
		copia(error, "No se puede usar la carpeta del usuario (\\USUARIOS).", cabe);
		return -1;
	}
	une_ruta(sal, sizeof sal, c, "SALIDA.TXT");
	so_borra(sal);
	fmt(args, sizeof args, "COMPRUEBA \"%s\" /MAQUINA", ruta);
	if (plat_comprueba(args, sal, &cod, comprueba_espera, comprueba_u, error, cabe))
		return -1;
	t = lee_texto(sal, &n, 16u << 20);
	if (!t) {
		copia(error, "WIN32 no ha dejado nada (¿está el paquete win32?).", cabe);
		return -1;
	}
	{
		/* de la 437 a UTF-8 */
		char *u = so_pide(n * 3 + 1);

		if (!u) {
			so_suelta(t);
			copia(error, "No hay memoria.", cabe);
			return -1;
		}
		n = de437_a_utf8((const u8 *)t, n, u, n * 3 + 1);
		so_suelta(t);
		t = u;
	}
	if (inf_lee(i, t, n) < 0 && !i->leido_total) {
		const char *p = busca(t, "WIN32");

		so_suelta(t);
		inf_suelta(i);
		fmt(error, cabe, "WIN32 COMPRUEBA no ha dicho nada que se entienda%s.", p ? "" : " (¿está el paquete win32?)");
		return -1;
	}
	so_suelta(t);
	/* (el «/MAQUINA: no se encuentra» de un WIN32 de antes) */
	if (!i->maquina)
		for (k = 0; k < i->n_a; k++)
			if (i->a[k].clase == A_NO_ESTA && iguales_sin_caja(i->a[k].nombre, "MAQUINA")) {
				int j;

				for (j = 0; j < i->n_f; j++)
					if (i->f[j].archivo > k)
						i->f[j].archivo--;
				memmove(i->a + k, i->a + k + 1, sizeof i->a[0] * (u32)(i->n_a - k - 1));
				i->n_a--;
				k--;
			}
	if (!i->n_a) {
		copia(error, es_carpeta ? "En esa carpeta no hay programas de Windows (.EXE ni .DLL)." : "No es un programa de Windows de 32 bits.", cabe);
		inf_suelta(i);
		return -1;
	}
	saber_marca(i);
	if (con_sumas)
		for (k = 0; k < i->n_a; k++) {
			c_archivo *a = &i->a[k];

			if (a->clase == A_NO_ESTA)
				continue;
			if (!a->ruta[0]) {
				if (es_carpeta)
					busca_en(ruta, a->nombre, 0, a->ruta, sizeof a->ruta);
				else if (iguales_sin_caja(sin_carpeta(ruta), a->nombre))
					copia(a->ruta, ruta, sizeof a->ruta);
			}
			/* (con /MAQUINA, WIN32 ya da el tamaño y la suma) */
			if (a->ruta[0] && !a->sha[0])
				suma(a->ruta, &a->tam, a->sha);
		}
	(void)cod;
	return 0;
}
