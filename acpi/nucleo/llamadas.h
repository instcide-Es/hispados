/* llamadas.h - Las funciones del núcleo ACPI (HSPACPI.BIN) que llama el
 * residente (los mismos números están en drivers/hspacpi.asm) y lo que se
 * pasan.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef LLAMADAS_ACPI_H
#define LLAMADAS_ACPI_H
#define LL_VERSION 1
#define LL_INICIA 0		/* petición: struct ll_inicio. Prepara la tarea (no toca ACPI todavía) */
#define LL_PASO 1		/* deja correr la tarea hasta que se duerma -> tics hasta la próxima (>= 1) */
#define LL_REGISTRO 2		/* [0] búfer, [1] tamaño -> octetos copiados (se van del registro) */
#define LL_ESTADO 3		/* [0] búfer (struct ll_estado), [1] su tamaño -> octetos */
#define LL_PARA 4		/* avisa al controlador embebido (_REG 3,0) y se para */
#define LL_AHORA 5		/* pide una lectura ya (la hará el próximo LL_PASO) */

/* opciones de ll_inicio */
#define LL_OP_DETALLE 1		/* /V: registro detallado (cada octeto del controlador embebido) */
#define LL_OP_SININI 2		/* no ejecutar los _INI de los dispositivos */
#define LL_OP_SINEVENTOS 4	/* no atender los avisos (_Qxx) del controlador embebido */
/* El residente, sin opciones, pide el modo «mínimo»: SININI | SINEVENTOS |
 * OS98 y LL_OSI_NADA (ver docs/ACPI.md); con /COMPLETO, nada de eso y
 * LL_OSI_XP: lo que haría un sistema con ACPI. */
#define LL_OP_MINIMO (LL_OP_SININI | LL_OP_SINEVENTOS | LL_OP_OS98)
#define LL_OP_SINREFRESCO 8	/* /SINREFRESCO: no pedir al equipo que relea (HP: PWUP) */
#define LL_OP_OS98 32		/* decir que somos «Microsoft Windows» (el de 1998) en \_OS */
#define LL_OP_PSR 16		/* /PSR: preguntar el cable al adaptador (_PSR) y no deducirlo de la batería */
/* a quién decimos parecernos cuando el equipo pregunta por _OSI */
#define LL_OSI_NADA 0
#define LL_OSI_XP 1		/* hasta «Windows 2001 SP2» (lo normal: ver docs/ACPI.md) */
#define LL_OSI_W7 2		/* hasta «Windows 2009» */
#define LL_OSI_TODO 3

struct ll_inicio {
	unsigned int base;	/* dirección física del bloque (y base de los segmentos) */
	unsigned int tam;	/* su tamaño */
	unsigned int opciones;
	unsigned int osi;
	unsigned int tsc_ms;	/* vueltas del contador de tiempo del procesador por milisegundo (0: no hay) */
	unsigned int dir_apm;	/* dirección física de la struct ll_apm del residente */
	unsigned int cada_s;	/* segundos entre lecturas */
};

/* lo que el residente contesta por INT 15h AX=530Ah (APM 1.2), ya cocinado */
#define FASE_EMPEZANDO 0
#define FASE_LISTO 1		/* hay lecturas */
#define FASE_SIN_BATERIA 2	/* ACPI va, pero el equipo no declara baterías (sobremesa) */
#define FASE_ERROR 3		/* no hay ACPI o ha fallado (error: el código) */
struct ll_apm {
	unsigned char fase;
	unsigned char ca;	/* BH: 0 sin cable, 1 con cable, FFh no se sabe */
	unsigned char estado;	/* BL: 0 alta, 1 baja, 2 crítica, 3 cargando, FFh no se sabe */
	unsigned char banderas;	/* CH: bit 0 alta, 1 baja, 2 crítica, 3 cargando, 7 no hay batería */
	unsigned char pct;	/* CL: 0..100, FFh no se sabe */
	unsigned char nbat;	/* baterías puestas */
	unsigned short minutos;	/* DX: bit 15 = en minutos; FFFFh no se sabe */
	unsigned int lecturas;	/* cuántas lecturas completas van */
	unsigned int error;
} __attribute__((packed));

#define LL_NBAT 4
struct ll_bat {
	unsigned int hay;	/* 1: el equipo la declara */
	unsigned int sta;	/* _STA (bit 4: puesta) */
	unsigned int valida;	/* 1: _BIF/_BIX y _BST leídos bien */
	unsigned int unidad;	/* 0 mW y mWh, 1 mA y mAh */
	unsigned int diseno, llena, voltaje_diseno;
	unsigned int estado;	/* _BST: bit 0 descargando, 1 cargando, 2 crítica */
	unsigned int ritmo, queda, voltaje;
	char ruta[32];		/* \_SB_.PCI0.LPC0.BAT1 */
	char modelo[20], tipo[12], marca[20];
};
struct ll_estado {
	unsigned int version;
	struct ll_apm apm;
	unsigned int ec_hay, ec_datos, ec_orden;	/* el controlador embebido y sus puertos */
	unsigned int ec_lecturas, ec_escrituras, ec_fallos, ec_eventos;
	unsigned int ca_hay, ca;			/* el adaptador (_PSR) */
	unsigned int memoria, memoria_max;		/* octetos pedidos ahora y el máximo */
	unsigned int errores_acpica;
	char ec_ruta[32], ca_ruta[32];
	struct ll_bat bat[LL_NBAT];
};
#endif
