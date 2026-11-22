/* ui.h - Correo: lo poco que la ventana pide al sistema para pintar y para
 * los sucesos (como so.h, pero de la ventana). En Hispa-Pentium, las
 * llamadas 32 a 40 y 80 (so_pentium.c); en Linux, nada (las pruebas de la
 * ventana van en QEMU). Los textos, en UTF-8: el sistema los pasa a su letra
 * (la 437 de Hispa, de 8x16).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_UI_H
#define CORREO_UI_H
#include "so.h"

#define UI_LETRA_AN 8		/* la letra del sistema: 8 x 16 */
#define UI_LETRA_AL 16

/* los sucesos */
#define UE_TECLA 1		/* a: la tecla (la letra, ya en Unicode; o UT_...); b: CON_... */
#define UE_BAJA 2		/* a, b: dónde; c: botones (UB_...), modificadores y (c >> 16) las veces (dos clics: 2) */
#define UE_SUBE 3
#define UE_MUEVE 4
#define UE_CERRAR 5
#define UE_TAMANO 6		/* a, b: lo que mide ahora */
#define UE_FOCO 7
#define UE_SALE 9
#define UE_MENU 10		/* a: la opción (desde 0; UI_NINGUNA), b: la etiqueta */
#define UE_UNIDADES 12
#define UE_RUEDA 13		/* a, b: dónde; c: los pasos (con signo en el octeto bajo; arriba, positivos) */
#define UE_SOLTADO 14		/* (0.8.7) se ha soltado algo de Mi equipo en la ventana: a, b, dónde; la ruta, con ui_soltado() */
#define UI_NINGUNA 0xFFFFFFFFu
#define UB_IZQ 1
#define UB_DER 2
#define UC_MAYUS 0x100
#define UC_CONTROL 0x200
typedef struct { u32 clase, a, b, c; } Suceso;

/* las teclas que no son letras (las de Hispa-Pentium: 0x100 + su código) */
#define UT_ARRIBA 0x148
#define UT_ABAJO 0x150
#define UT_IZQUIERDA 0x14B
#define UT_DERECHA 0x14D
#define UT_INICIO 0x147
#define UT_FIN 0x14F
#define UT_SUPR 0x153
#define UT_RE_PAG 0x149
#define UT_AV_PAG 0x151
#define UT_F1 0x13B
#define UT_F2 0x13C
#define UT_F3 0x13D
#define UT_F5 0x13F
#define UT_MAYUS_TAB 0x10F
#define UT_TAB 9
#define UT_INTRO 13
#define UT_ESC 27
#define UT_BORRAR 8

int ui_abre(const char *titulo, int an, int al);	/* 0 si va */
int ui_abre_aviso(int an, int al);	/* una ventanita sin marco, encima, abajo a la derecha (el aviso del correo nuevo) */
void ui_cierra(void);
void ui_titulo(const char *t);
void ui_pantalla(int *an, int *al);	/* lo que mide (sin la barra de tareas) */
void ui_maximiza(void);
/* el suceso siguiente, esperando como mucho espera_ms (0: sin esperar): 1
 * si hay */
int ui_suceso(Suceso *s, u32 espera_ms);
/* (0.8.7) tras UE_SOLTADO, la ruta de lo soltado (UTF-8; la llamada 173 de
 * Hispa-Pentium, la misma del Bloc de notas y del Reproductor): 1 si hay */
int ui_soltado(char *ruta, int cabe);
void ui_rect(int x, int y, int an, int al, u32 c);
void ui_degradado(int x, int y, int an, int al, u32 a, u32 b);
void ui_redondo(int x, int y, int an, int al, int r, u32 a, u32 b);
void ui_marco(int x, int y, int an, int al, u32 c);
void ui_linea(int x1, int y1, int x2, int y2, u32 c);
void ui_mapa(int x, int y, int an, int al, const u32 *p);
/* n octetos de UTF-8, en la letra del sistema (fina o negrita); a lo sumo
 * «max» letras */
void ui_texto(int x, int y, const char *t, u32 n, int max, u32 c, int negrita);
/* un menú emergente: las opciones (UTF-8) separadas por «|»; las de los bits
 * de «grises», en gris. Lo elegido llega como UE_MENU con esa etiqueta */
void ui_menu(int x, int y, const char *opciones, u32 grises, u32 etiqueta);

#endif
