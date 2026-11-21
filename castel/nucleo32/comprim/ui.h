/* ui.h - la ventana de Comprimidos y del Mirador (ui.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef UI_H
#define UI_H
#include <stdint.h>
#include "pentium.h"

/* los colores de controles.cas */
#define C_FONDO 0xE9EDF2
#define C_PAPEL 0xFFFFFF
#define C_TINTA 0x111827
#define C_SUAVE 0x404B5A
#define C_AZUL 0x1D4ED8
#define C_AZUL_CLARO 0xDBEAFE
#define C_NARANJA 0xB45309
#define C_NARANJA_CLARO 0xFEF3C7
#define C_ROJO 0x991B1B
#define C_VERDE 0x166534
#define C_BORDE 0x6B7280
#define C_GRIS 0xD1D5DB
#define C_BARRA 0xF3F4F6
#define ANCHO_LETRA 8
#define ALTO_LETRA 16
#define ALTO_BOTON 28

extern uint32_t ui_v;
extern int ui_an, ui_al;
int ui_ventana(const char *titulo, int an, int al);
void ui_titulo(const char *t);
int ui_suceso(struct suceso *s, int espera);
void ui_rect(int x, int y, int an, int al, uint32_t c);
void ui_degradado(int x, int y, int an, int al, uint32_t a, uint32_t b);
void ui_redondo(int x, int y, int an, int al, int r, uint32_t a, uint32_t b);
void ui_marco(int x, int y, int an, int al, uint32_t c);
void ui_linea(int x1, int y1, int x2, int y2, uint32_t c);
void ui_mapa(int x, int y, int an, int al, const uint32_t *p);
void ui_texto(int x, int y, const char *t, int max, uint32_t c);
void ui_negrita(int x, int y, const char *t, int max, uint32_t c);
void ui_olvida_zonas(void);
void ui_zona(int id, int x, int y, int an, int al);
int ui_en_zona(int x, int y);
int ui_ancho_boton(const char *t);
int ui_boton(int id, int x, int y, int an, const char *t, int principal, int apagado);
void ui_menu(int x, int y, const char *opciones, uint32_t grises, uint32_t etiqueta);
void ui_al_repintar(void (*f)(void));
int ui_pregunta(const char *titulo, const char *texto, const char *b1, const char *b2, const char *b3);
void ui_aviso(const char *titulo, const char *texto);
int ui_pide_texto(const char *titulo, const char *pregunta, char *t, int cabe, int oculto);
void ui_barra(int x, int y, int an, uint32_t hecho, uint32_t total);
uint32_t ui_tics(void);
uint32_t ui_hz(void);
void ui_serie(const char *t);
#endif
