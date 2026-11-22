/* orto_arnes.c - Pluma 2, pruebas: comun/orto.c con las órdenes del arnés de
 * la prueba de Hispa-386 (tests/tools/ortografia_prueba.py), para comparar
 * los dos línea a línea. Por la entrada, una orden por línea (en la 437):
 *   D ruta     cargar DICCIO.DAT -> H1 o H0
 *   P palabra  1 si está bien, 0 si no
 *   S palabra  las propuestas, separadas por comas (en la 437, como las da
 *              la de 386: las mayúsculas sin tilde)
 *   M n        cuántas propuestas se piden
 *   A palabra  apuntarla en las del usuario -> 0 (apuntada) o 1 (no)
 *   F texto    las faltas del texto: las palabras mal escritas, con comas
 *   V veces palabra   (para medir) consultarla tantas veces
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "orto.h"
#include "texto.h"



static u32 de(const char *s, u32 n, u32 *w)
{
	u32 k;

	for (k = 0; k < n && k < 300; k++)
		w[k] = de437((u8)s[k]);
	return k;
}

static u8 a437h(u32 u)
{
	u8 c = a437(u);

	if (c == 0xB6) c = 'A';
	if (c == 0xCF) c = 'I';
	if (c == 0xD3) c = 'O';
	if (c == 0xD7) c = 'U';
	return c;
}

static void pon437(u32 u)
{
	putchar(a437h(u));
}

int ofi_principal(const char *args)
{
	static char l[100000];
	(void)args;
	u32 w[300];
	int max = 8;

	while (fgets(l, sizeof l, stdin)) {
		u32 n = (u32)strlen(l);

		while (n && (l[n - 1] == '\n' || l[n - 1] == '\r'))
			l[--n] = 0;
		if (n < 1) {
			putchar('?');
			putchar('\n');
			continue;
		}
		const char *a = n >= 2 ? l + 2 : l + n;
		u32 m = n >= 2 ? n - 2 : 0;

		switch (l[0]) {
		case 'D':
			printf("H%d", orto_carga_archivo(a));
			break;
		case 'P':
			printf("%d", orto_bien(w, de(a, m, w)));
			break;
		case 'A':
			printf("%d", orto_propia(w, de(a, m, w)) ? 1 : 0);
			break;
		case 'M':
			max = atoi(a);
			break;
		case 'S': {
			/* (como la de 386: pasadas a su 437, sin repetir ni la tecleada) */
			u32 s[ORTO_SUG][ORTO_LARGO + 1];
			char b[ORTO_SUG][ORTO_LARGO + 1];
			u32 k0 = de(a, m, w);
			int k = orto_sugiere(w, k0, s, 8), i, j, q, puestas = 0;

			for (i = 0; i < k && puestas < max; i++) {
				for (j = 0; s[i][j]; j++)
					b[puestas][j] = (char)a437h(s[i][j]);
				b[puestas][j] = 0;
				if ((u32)j == m && !memcmp(b[puestas], a, m))
					continue;
				for (q = 0; q < puestas; q++)
					if (!strcmp(b[q], b[puestas]))
						break;
				if (q < puestas)
					continue;
				if (puestas)
					putchar(',');
				fputs(b[puestas], stdout);
				puestas++;
			}
			break;
		}
		case 'F': {
			u32 k = de(a, m, w), i;
			u8 mk[300];
			int primera = 1;

			orto_faltas(w, k, mk, 0xFFFFFFFF);
			for (i = 0; i < k; i++)
				if (mk[i]) {
					if (mk[i] == 1 && !primera)
						putchar(',');
					primera = 0;
					pon437(w[i]);
				}
			break;
		}
		case 'V': {
			char *e;
			long v = strtol(a, &e, 10), i;
			u32 k;
			int r = 0;
			clock_t t0 = clock();

			k = de(e + 1, (u32)strlen(e + 1), w);
			for (i = 0; i < v; i++)
				r += orto_bien(w, k);
			printf("%d %.3f us", r, (double)(clock() - t0) * 1e6 / CLOCKS_PER_SEC / (double)v);
			break;
		}
		default:
			putchar('?');
		}
		putchar('\n');
	}
	return 0;
}
