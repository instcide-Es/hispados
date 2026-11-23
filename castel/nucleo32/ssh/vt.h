/* vt.h - (TERMO) el terminal de SSH.BIN en una ventana (vt.c): lo que pide
 * una interfaz de hoy como la de Claude Code (Ink), htop, vim o less:
 * xterm-256color con colores de 256 y de 24 bits, UTF-8 con cajas, bloques,
 * Braille y los símbolos de siempre (✻ ⏺ ⎿ ❯ ✓ …), letras anchas, la
 * pantalla alternativa, borrar líneas, el cursor oculto/visible y su forma,
 * el pegado entre corchetes, la actualización sincronizada (2026), las
 * preguntas del programa (DA, DSR, DECRQM, OSC 10/11, XTVERSION, tamaño) y
 * cualquier tamaño (lo que quepa en la ventana; window-change al cambiar).
 * C portable: lo usa SSH.BIN y la prueba del anfitrión (pruebas/vt_prueba.c),
 * que pinta lo mismo en un .PPM.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef VT_H
#define VT_H
#include <stdint.h>

#define VT_MAX_COLS 200
#define VT_MAX_FILAS 70
#define VT_AN 8			/* la letra: 8 x 16 */
#define VT_AL 16

/* los colores de una celda: VT_DEF (el de siempre), un índice (0-255) con
 * VT_IDX, o un RGB con VT_RGB */
#define VT_DEF 0
#define VT_IDX 0x01000000u
#define VT_RGB 0x02000000u

#define VT_NEGRITA 1
#define VT_TENUE 2
#define VT_CURSIVA 4
#define VT_SUBRAYA 8
#define VT_PARPADEO 16
#define VT_INVERSO 32
#define VT_OCULTO 64
#define VT_TACHADO 128
#define VT_ANCHA 256		/* la primera mitad de una letra ancha */
#define VT_SEGUNDA 512		/* la segunda mitad (no se pinta sola) */

struct vt_celda { uint32_t c, fg, bg; uint16_t at; };

struct vt {
	int cols, filas;
	struct vt_celda *p, *alt;	/* la pantalla y la otra (cols * filas) */
	uint8_t sucia[VT_MAX_FILAS];
	int cx, cy, envolver, arriba, abajo;
	uint32_t fg, bg;
	uint16_t at;
	int s_cx, s_cy, s_envolver; uint32_t s_fg, s_bg; uint16_t s_at; int s_g0;	/* DECSC */
	int cursor_visible, forma_cursor;	/* 0-6 de DECSCUSR */
	int teclas_app, teclado_app, autoenvolver, origen, insertar, alterna;
	int pegado, foco, sincro, raton;	/* 2004, 1004, 2026, 1000/1002/1003 */
	uint32_t sincro_desde;
	int g0_lineas;			/* ESC ( 0 */
	uint8_t tabs[VT_MAX_COLS];
	/* lo que se está leyendo */
	int estado, npar, par[24], sub[24], privado, inter;
	uint32_t utf_v; int utf_falta;
	char osc[512]; int nosc;
	uint32_t ultima;		/* la última letra (REP) */
	/* lo que hay que contestar (lo manda el programa) */
	char resp[256]; int nresp;
	char titulo[128]; int titulo_nuevo;
	uint32_t campana;
	uint32_t tinta, fondo;		/* los de siempre (RGB) */
};

int vt_inicia(struct vt *t, int cols, int filas);	/* 0, o -1 sin memoria */
void vt_tamano(struct vt *t, int cols, int filas);	/* lo de dentro, recortado */
void vt_entra(struct vt *t, const uint8_t *d, uint32_t n);
void vt_libre(struct vt *t);
/* lo que pinta: el lienzo (an x al puntos, paso «paso»), desde (x0, y0),
 * solo las filas sucias (todas si todo); da cuántas filas ha pintado y, en
 * *f0 y *f1, la primera y la última (para subir solo eso) */
int vt_pinta(struct vt *t, uint32_t *lz, int an, int al, int paso, int x0, int y0, int todo, int *f0, int *f1);
uint32_t vt_rgb(const struct vt *t, uint32_t c, int es_fondo);
/* una tecla a lo que manda un xterm: «c» es la letra (Unicode) o una de
 * VT_T_*; «mods» VT_M_*; da los octetos en o (caben 16) */
#define VT_M_MAYUS 1
#define VT_M_ALT 2
#define VT_M_CTRL 4
enum { VT_T_ARRIBA = 0x110000, VT_T_ABAJO, VT_T_DERECHA, VT_T_IZQUIERDA, VT_T_INICIO, VT_T_FIN, VT_T_INSERT, VT_T_SUPR,
       VT_T_REPAG, VT_T_AVPAG, VT_T_F1, VT_T_TAB_ATRAS = VT_T_F1 + 12, VT_T_INTRO, VT_T_RETROCESO, VT_T_ESC, VT_T_TAB };
int vt_tecla(const struct vt *t, uint32_t c, int mods, char *o);
/* lo pegado (UTF-8), con los corchetes si el programa los ha pedido: a o
 * (caben n + 12); los saltos de línea, como Intro (\r) */
int vt_pega(const struct vt *t, const char *d, int n, char *o);
int vt_ancho(uint32_t c);		/* 0, 1 o 2 columnas */
#endif
