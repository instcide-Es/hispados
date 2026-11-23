/* vt_prueba.c - (TERMO) en el anfitrión: el terminal de SSH.BIN (vt.c) con lo
 * que manda un programa de verdad (grabado con graba.py: htop, vim, less,
 * colores.sh...), sin red ni QEMU.
 *   vt_prueba COLS FILAS GRABACION SALIDA.ppm
 * Pinta la pantalla en SALIDA.ppm (como la vería la ventana) y escribe por la
 * salida las filas de texto (en UTF-8), el cursor, los modos y lo que el
 * terminal ha contestado («resp: ...», con los ESC como \e), para que la
 * prueba los mire.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vt.h"

static struct vt T;
static uint32_t lz[VT_MAX_COLS * VT_AN * VT_MAX_FILAS * VT_AL];

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

int main(int argc, char **argv)
{
	FILE *f;
	static uint8_t d[1 << 22];
	size_t n;
	int cols, filas, an, al, y, x, f0, f1, i;

	if (argc < 5)
		return 2;
	cols = atoi(argv[1]);
	filas = atoi(argv[2]);
	vt_inicia(&T, cols, filas);
	f = fopen(argv[3], "rb");
	if (!f)
		return 2;
	n = fread(d, 1, sizeof d, f);
	fclose(f);
	/* (a trozos de 777, como llegarían por la red: las órdenes partidas también) */
	for (i = 0; (size_t)i < n; i += 777)
		vt_entra(&T, d + i, (uint32_t)((size_t)i + 777 <= n ? 777 : n - (size_t)i));
	an = T.cols * VT_AN;
	al = T.filas * VT_AL;
	vt_pinta(&T, lz, an, al, an, 0, 0, 1, &f0, &f1);
	f = fopen(argv[4], "wb");
	fprintf(f, "P6\n%d %d\n255\n", an, al);
	for (i = 0; i < an * al; i++) {
		unsigned char p[3] = { (unsigned char)(lz[i] >> 16), (unsigned char)(lz[i] >> 8), (unsigned char)lz[i] };

		fwrite(p, 1, 3, f);
	}
	fclose(f);
	for (y = 0; y < T.filas; y++) {
		char l[VT_MAX_COLS * 4 + 1];
		int k = 0;

		for (x = 0; x < T.cols; x++) {
			struct vt_celda *c = &T.p[y * T.cols + x];

			if (c->at & VT_SEGUNDA)
				continue;
			k += utf8(c->c ? c->c : ' ', l + k);
		}
		while (k > 0 && l[k - 1] == ' ')
			k--;
		l[k] = 0;
		printf("fila %02d: %s\n", y, l);
	}
	printf("cursor: %d,%d %s; alterna %d; pegado %d; sincro %d; teclas_app %d\n", T.cx, T.cy, T.cursor_visible ? "visible" : "oculto", T.alterna, T.pegado,
	       T.sincro, T.teclas_app);
	printf("resp: ");
	for (i = 0; i < T.nresp; i++)
		if (T.resp[i] == 0x1b)
			printf("\\e");
		else
			putchar(T.resp[i]);
	printf("\n");
	/* el color de algunas celdas, por si la prueba lo pide: «color x y» */
	for (i = 5; i + 1 < argc; i += 2) {
		int cx = atoi(argv[i]), cy = atoi(argv[i + 1]);

		if (cx < T.cols && cy < T.filas) {
			struct vt_celda *c = &T.p[cy * T.cols + cx];

			printf("celda %d,%d: U+%04X letra %06X fondo %06X at %X\n", cx, cy, c->c, vt_rgb(&T, c->fg, 0), vt_rgb(&T, c->bg, 1), c->at);
		}
	}
	return 0;
}
