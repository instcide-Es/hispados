/* hispa64.h - ACPICA64.BIN, ACPI en el núcleo de Hispa-64 (docs/64/HISPA-64.md,
 * etapa 11 b): las llamadas del núcleo, lo que se pasan, y lo que el núcleo
 * le da (castel/nucleo64/acpica64.cas tiene lo mismo: si cambia uno, el otro).
 *
 * ACPICA64.BIN es el mismo núcleo de C que HSPACPI.BIN de Hispa-DOS y
 * ACPI.BIN de Hispa-Pentium (acpi/nucleo/ y ACPICA), compilado para 64 bits y
 * enlazado en 0xFFFFFFFFC0000000: el núcleo lo copia ahí desde la unidad en
 * memoria y lo llama en el anillo 0, siempre con su cerrojo (un hilo dentro
 * cada vez: ACPICA va con ACPI_SINGLE_THREADED), en la pila del hilo que
 * llama. Lo que no puede hacer él (la memoria física que no está en el mapa
 * directo, la configuración PCI, dormir, la hora, escribir) se lo pide al
 * núcleo con las funciones de Castel de struct h64_tabla.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA64_ACPI_H
#define HISPA64_ACPI_H
#include "nucleo.h"

#define H64_BASE 0xFFFFFFFFC0000000ul
#define H64_VERSION 1

/* las llamadas (la entrada: f, a, b -> lo que devuelve) */
#define H64_INICIA 0	/* a: struct h64_inicio */
#define H64_ARRANCA 1	/* -> 0 con baterías, 1 sin ellas, -1 ACPI no va */
#define H64_LEE 2	/* una lectura de la batería y el cable -> 1 si el equipo había avisado */
#define H64_EVENTOS 3	/* los avisos del controlador embebido (_Qxx) -> 1 si hay cambios (batería, cable, botón) */
#define H64_ESTADO 4	/* a: struct h64_estado, b: su tamaño -> octetos */
#define H64_PRT 5	/* a: ruta del dispositivo, b: struct h64_rutas -> entradas (-1: no hay _PRT) */
#define H64_APAGA 6	/* _PTS(5), \_S5 y SLP_EN: no vuelve si se apaga -> el código de ACPICA */
#define H64_BRILLO 7	/* a: el nivel (0-100) o -1 para solo leerlo -> el nivel (_BQC), -1 si no hay */
#define H64_REGISTRO 8	/* a: búfer, b: su tamaño -> octetos (se van del registro) */
#define H64_PRUEBA 9	/* (pruebas) a: 1 evalúa un método de nombre b (texto) y devuelve el entero; 2, lo ejecuta (sin argumentos) */
/* [etapa 11 a] suspender (S3) */
#define H64_DORMIR 10	/* a: el estado (3): _PTS(3), _SST (AcpiEnterSleepStatePrep) -> SLP_TYPa | SLP_TYPb << 8 (-1: no hay \_S3) */
#define H64_DESPIERTA 11	/* a: el estado (3): _WAK, _SST (AcpiLeaveSleepStatePrep y AcpiLeaveSleepState) -> 0 */

/* opciones (h64_inicio.opciones) */
#define H64_OP_DETALLE 1	/* el registro de cada octeto del controlador embebido */
#define H64_OP_SININI 2		/* no ejecutar los _INI */

/* las funciones del núcleo (direcciones de funciones de Castel: los
 * argumentos por la pila, el primero arriba; las llama entrada.asm) */
struct h64_tabla {
	u64 escribe;	/* (texto, octetos): al puerto serie (el registro de ACPI) */
	u64 mapa;	/* (física, octetos) -> dónde se ve (el mapa directo o mapea_disp sin caché), 0 si no */
	u64 pci_lee;	/* (bus, aparato, función, registro) -> los 32 bits */
	u64 pci_pon;	/* (bus, aparato, función, registro, valor) */
	u64 duerme_ms;	/* (ms): suelta el procesador */
	u64 ahora_ns;	/* () -> ns desde el arranque */
};

struct h64_inicio {
	u32 version;
	u32 opciones;
	u64 rsdp;		/* la física de la RSDP */
	u64 monton, monton_largo;	/* la memoria para ACPICA (del núcleo, ya puesta) */
	struct h64_tabla t;
};

/* lo que se sabe, para el núcleo (todo de 32 bits, sin huecos) */
struct h64_estado {
	u32 version;
	u32 fase;		/* FASE_... de llamadas.h */
	u32 nbat, pct;		/* las puestas; 0-100 o FFh */
	u32 ca;			/* 0 sin cable, 1 con cable, FFh no se sabe */
	u32 estado;		/* 0 alta, 1 baja, 2 crítica, 3 cargando, FFh */
	u32 minutos;		/* lo que queda (FFFFh no se sabe) */
	u32 lecturas;
	u32 unidad;		/* 0 mWh, 1 mAh */
	u32 diseno, llena, queda, ritmo, voltaje;	/* de la primera batería (_BIF/_BIX y _BST) */
	u32 bst_estado;		/* _BST: bit 0 descargando, 1 cargando, 2 crítica */
	u32 ciclos;		/* _BIX (FFFFFFFFh no se sabe) */
	u32 s5, slp_a, slp_b;	/* \_S5 */
	u32 botones;		/* los Notify (80h) a un botón de encendido PNP0C0C */
	u32 brillo_hay, brillo, brillo_n;	/* _BCM: hay; el nivel (_BQC); cuántos niveles (_BCL) */
	u32 ec_hay, ec_eventos;
	u32 errores, memoria_kb, memoria_max_kb;
	u32 avisos;		/* los Notify del equipo */
	char bat_ruta[32], modelo[20], ca_ruta[32], ec_ruta[32], brillo_ruta[48];
	/* [etapa 11 a] la tapa (PNP0C0D), las teclas del brillo (los Notify 86h y
	 * 87h al dispositivo de _BCM) y \_S3 */
	u32 tapa_hay, tapa;	/* tapa: 1 abierta, 0 cerrada (_LID) */
	u32 tapa_avisos;	/* los Notify (80h) a la tapa */
	u32 brillo_sube, brillo_baja;	/* los Notify 86h (y 85h) y 87h */
	u32 s3, s3_a, s3_b;	/* \_S3: hay; SLP_TYPa y SLP_TYPb */
	char tapa_ruta[32];
};

/* las rutas de un _PRT, resueltas */
#define H64_MAX_RUTAS 128
struct h64_ruta {
	u32 ranura, patilla, gsi;
	u32 marcas;		/* como en la MADT: bits 0-1 la polaridad (1 alto, 3 bajo), 2-3 el disparo (1 flanco, 3 nivel) */
	u32 enlace;		/* 1: por un enlace (su _CRS), 0: la GSI escrita */
};
struct h64_rutas {
	u32 n, sin_resolver;	/* las resueltas; las de un enlace sin IRQ puesta */
	struct h64_ruta r[H64_MAX_RUTAS];
};

/* entrada.asm: llamar a una función de Castel con 0 a 5 argumentos */
u64 castel0(u64 f);
u64 castel1(u64 f, u64 a);
u64 castel2(u64 f, u64 a, u64 b);
u64 castel4(u64 f, u64 a, u64 b, u64 c, u64 d);
u64 castel5(u64 f, u64 a, u64 b, u64 c, u64 d, u64 e);

extern struct h64_inicio h64;
#endif
