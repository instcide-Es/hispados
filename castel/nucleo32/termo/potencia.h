/* potencia.h - (TERMO) la potencia y los frenos del procesador (potencia.c):
 * los vatios (RAPL), los límites que le ha puesto la BIOS (PL1, PL2, TDP,
 * las relaciones de turbo), por qué se frena (los MSR de estado térmico y
 * los PERF_LIMIT_REASONS) y los estados de reposo (C-states). Sin llamadas
 * al sistema: quien llama da cómo leer y limpiar un MSR, así que lo usan
 * TERMO.BIN (termo.c) y, en el anfitrión, la prueba con MSR de mentira
 * (pruebas/potencia_prueba.c). Todos los textos, en UTF-8.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef POTENCIA_H
#define POTENCIA_H
#include <stdint.h>

struct po_lector {
	/* leer un MSR: 1 bien; 0 si no lo tiene (la excepción 13, recogida) */
	int (*lee)(uint32_t msr, uint32_t *lo, uint32_t *hi);
	/* poner a 0 un MSR de motivos de freno (sus marcas de registro, que
	 * son volátiles): 1 bien; 0 si no se deja. Puede ser 0 (no se limpia) */
	int (*limpia)(uint32_t msr);
	/* un suceso para el .LOG (y el puerto serie) y una línea solo para el
	 * puerto serie; los dos pueden ser 0 */
	void (*suceso)(const char *texto);
	void (*serie)(const char *texto);
};

#define PO_REPOSO 0
#define PO_CARGA 1
#define PO_ENFRIA 2

/* al abrir: qué procesador es (familia y modelo de CPUID) y qué tiene; lee
 * las unidades, los límites, las relaciones y los C-states, lo cuenta y deja
 * a 0 las marcas de registro de los motivos de freno (después de que termo.c
 * haya visto las «de antes») */
void po_empieza(const struct po_lector *l, int intel, int amd, int fam, int modelo);
/* cada 2 s: la fase (PO_*), el TSC y su frecuencia (Hz), el cargador (1
 * enchufado, 0 no, -1 no se sabe) y la temperatura del procesador más alta y
 * su TjMax (décimas de grado; -100000 si no se sabe) */
void po_mide(int fase, uint64_t tsc, uint64_t tsc_hz, int cargador, int t_cpu, int tjmax);
/* lo de ahora, para la línea DATOS del .LOG (« | W paq=41,2 ...»): octetos */
int po_datos(char *d, int cabe);
/* las secciones del INFORME.TXT: POTENCIA, POR QUÉ SE FRENA y ESTADOS DE
 * REPOSO (con «\r\n»): octetos */
int po_informe(char *d, int cabe);
/* la frase para el RESULTADO («Potencia: ...»; 0 si no hay nada que decir) */
int po_veredicto(char *d, int cabe);
/* ¿se ha frenado en la carga por potencia, por la placa o por corriente? */
int po_frenado_carga(void);
/* ¿tiene RAPL (los vatios)? */
int po_hay_rapl(void);
#endif
