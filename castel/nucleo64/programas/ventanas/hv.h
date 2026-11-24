/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ventanas/hv.h - Hispa-64, etapa 8: el protocolo de
 * las ventanas (docs/64/HISPA-64.md, sección 14), entre el servidor
 * (VENTANAS: el compositor, el escritorio y la barra) y los programas
 * (oficina/comun/so_hispa64.c), todo por memoria compartida (/dev/shm del
 * núcleo, con shm_open de musl) y futex entre procesos:
 *
 *  - /dev/shm/hispa-ventanas (hv_control): lo hace el servidor. Tiene lo
 *    que mide la pantalla, un contador «aviso» (un futex: los programas lo
 *    suben y despiertan al servidor cuando piden algo), el portapapeles y
 *    HV_MAX huecos de ventana.
 *  - Cada ventana: un hueco (hv_ventana) que el programa toma (estado de
 *    LIBRE a TOMADA, con un cmpxchg), y sus puntos en otra memoria
 *    compartida suya, /dev/shm/hv-PID-N (an x al puntos 0x00RRGGBB, de
 *    izquierda a derecha y de arriba abajo), que el servidor pone también
 *    en su memoria. El programa escribe sus puntos y dice qué rectángulo ha
 *    cambiado (sucio); el servidor lo compone en el búfer de atrás de
 *    /dev/fb0 y lo copia a la pantalla.
 *  - Los sucesos (teclas, ratón, cerrar, tamaño, foco, menú) van del
 *    servidor al programa por una cola en el hueco (ev_pon, ev_toma), con
 *    su futex («ev_aviso»).
 *  - Las peticiones del programa (título, maximizar, centrar, cerrar, un
 *    menú emergente, la forma del puntero, un búfer nuevo) son bits de
 *    «pide»: el servidor los toma con un intercambio atómico.
 * Si un programa acaba sin cerrar su ventana, el servidor lo ve (kill con
 * la señal 0 da ESRCH) y la quita.
 */
#ifndef HV_H
#define HV_H
#include <stdint.h>

#define HV_NOMBRE "/hispa-ventanas"
#define HV_MAGIA 0x31564848u		/* «HHV1» */
#define HV_MAX 32
#define HV_COLA 128
#define HV_PAPEL 65536

enum { HV_LIBRE, HV_TOMADA, HV_LISTA, HV_VIVA, HV_CERRADA };

/* lo que pide el programa (bits de «pide») */
#define HV_P_TITULO 1
#define HV_P_MAXIMIZA 2
#define HV_P_CENTRA 4
#define HV_P_BUFER 8			/* un búfer nuevo (buf, an, al) */
#define HV_P_MENU 16
#define HV_P_PUNTERO 32
#define HV_P_CIERRA 64
#define HV_P_SUCIO 128

/* un suceso: como so_suceso (oficina/comun/so.h) */
typedef struct {
	int32_t clase, x, y;
	uint32_t tecla;
	int32_t mods, botones, veces, pasos, opcion;
	uint32_t etiqueta;
	int32_t foco;
} hv_suceso;

typedef struct {
	volatile uint32_t estado;
	volatile int32_t pid;
	volatile uint32_t pide;
	volatile uint32_t gen;		/* sube con cada búfer nuevo */
	int32_t an, al;			/* el búfer (lo de dentro de la ventana) */
	char buf[48];			/* su nombre en /dev/shm */
	char titulo[160];		/* UTF-8 */
	/* lo sucio: [sx0, sx1) x [sy0, sy1), con un cerrojo (0 libre) */
	volatile uint32_t s_cerrojo;
	int32_t sx0, sy0, sx1, sy1;
	/* el menú emergente pedido */
	int32_t menu_x, menu_y;
	uint32_t menu_grises, menu_etiqueta;
	char menu[1100];
	/* la forma del puntero: la de toda la ventana y la de hasta 16 zonas */
	int32_t forma, n_zonas;
	int32_t zona[16][5];		/* x, y, an, al, forma */
	/* los sucesos, del servidor al programa */
	volatile uint32_t ev_pon, ev_toma;
	volatile uint32_t ev_aviso;	/* (futex) */
	hv_suceso ev[HV_COLA];
} hv_ventana;

typedef struct {
	uint32_t magia, version;
	volatile int32_t servidor;	/* su pid */
	int32_t pant_an, pant_al;	/* la pantalla entera */
	int32_t libre_an, libre_al;	/* sin la barra */
	volatile uint32_t aviso;	/* (futex) los programas lo suben al pedir algo */
	volatile uint32_t p_cerrojo;	/* el portapapeles */
	uint32_t p_n;
	volatile uint32_t control;	/* 1 si Control está pulsado ahora (so_control) */
	char papel[HV_PAPEL];
	hv_ventana v[HV_MAX];
} hv_control;

/* ---- lo atómico y los futex, entre procesos ---- */
static inline int hv_toma(volatile uint32_t *c)
{
	while (__sync_lock_test_and_set(c, 1))
		while (*c)
			__builtin_ia32_pause();
	return 1;
}

static inline void hv_suelta(volatile uint32_t *c)
{
	__sync_lock_release(c);
}

#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
/* (sin FUTEX_PRIVATE_FLAG: entre procesos) */
static inline void hv_despierta(volatile uint32_t *f)
{
	syscall(SYS_futex, f, 1 /* FUTEX_WAKE */, 0x7fffffff, 0, 0, 0);
}

static inline void hv_espera(volatile uint32_t *f, uint32_t visto, int ms)
{
	struct timespec t;

	t.tv_sec = ms / 1000;
	t.tv_nsec = (long)(ms % 1000) * 1000000;
	syscall(SYS_futex, f, 0 /* FUTEX_WAIT */, visto, ms >= 0 ? &t : 0, 0, 0);
}
#endif
