/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/navegador/ventana.h - Hispa-64, etapa 10 h (docs/64/HISPA-64.md,
 * sección 25): lo que NAVEGADOR (C++) pide de las ventanas de Hispa-64
 * (VENTANAS, por la capa «so» de la oficina: so_hispa64.c) y de la letra de
 * la oficina (lienzo.c y letras.c, la DejaVu Sans con suavizado), con tipos
 * de C sencillos: así las cabeceras de la oficina (u32, so_suceso...) no se
 * cruzan con las de Ladybird (AK). Lo hace ventana.c. */
#ifndef NV_VENTANA_H
#define NV_VENTANA_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* los sucesos, como so_suceso (oficina/comun/so.h) */
enum { NV_NADA, NV_TECLA, NV_BAJA, NV_SUBE, NV_MUEVE, NV_CERRAR, NV_TAMANO, NV_FOCO, NV_SALE, NV_MENU, NV_RUEDA };
#define NV_T_BASE 0x110000u
enum {
	NV_T_ARRIBA = NV_T_BASE + 1, NV_T_ABAJO, NV_T_IZQUIERDA, NV_T_DERECHA,
	NV_T_INICIO, NV_T_FIN, NV_T_RE_PAG, NV_T_AV_PAG, NV_T_SUPR, NV_T_INSERTAR,
	NV_T_F1, NV_T_F2, NV_T_F3, NV_T_F4, NV_T_F5, NV_T_F6, NV_T_F7, NV_T_F8,
	NV_T_F9, NV_T_F10, NV_T_F11, NV_T_F12, NV_T_MAYUS_TAB, NV_T_CTRL_TAB,
	NV_T_ALT
};
#define NV_CON_MAYUS 1
#define NV_CON_CONTROL 2
#define NV_BOTON_IZQ 1
#define NV_BOTON_DER 2
#define NV_BOTON_CEN 4
#define NV_P_FLECHA 0
#define NV_P_TEXTO 7

typedef struct {
	int clase, x, y;
	uint32_t tecla;
	int mods, botones, veces, pasos, opcion;
	uint32_t etiqueta;
	int foco;
} nv_suceso;

/* la ventana */
int nv_hay_escritorio(void);
void nv_pantalla(int *an, int *al);			/* lo libre (sin la barra) */
int nv_abre(const char *titulo, int an, int al);	/* 0 si no se puede */
void nv_titulo(int v, const char *t);
void nv_maximiza(int v);
void nv_cierra(int v);
void nv_mapa(int v, int x, int y, int an, int al, const uint32_t *px, int paso);
int nv_suceso_de(int v, nv_suceso *s);			/* 1 si había uno (sin esperar) */
int nv_espera(int v, int ms);				/* hasta que haya uno (1) o pase el plazo (0) */
void nv_puntero(int v, int forma);
int nv_control(void);					/* Control pulsado ahora */
void nv_al_portapapeles(const char *t, uint32_t n);
char *nv_del_portapapeles(uint32_t *n);			/* (con free) o NULL */

/* la letra de la oficina, sobre unos puntos 0x00RRGGBB (de «paso» por fila) */
void nv_letras(void);
int nv_texto(uint32_t *px, int an, int al, int paso, int x, int y, const char *t, uint32_t c, int negrita, int tam4);
int nv_ancho(const char *t, int negrita, int tam4);
#ifdef __cplusplus
}
#endif
#endif
