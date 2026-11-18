/* nucleo.h - El núcleo ACPI de Hispa-486 (HSPACPI.BIN): lo común.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef NUCLEO_ACPI_H
#define NUCLEO_ACPI_H
#include "llamadas.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

/* ---- la máquina (maquina.c en el equipo; tests/acpi/maquina_sim.c en las pruebas) ---- */
u32 mq_es_lee(u32 puerto, u32 octetos);
void mq_es_escribe(u32 puerto, u32 octetos, u32 valor);
void *mq_mapa(u32 fisica, u32 tam);	/* memoria física -> puntero del núcleo */
void mq_desmapa(void *p, u32 tam);
u64 mq_ahora_us(void);			/* microsegundos desde que arrancó el núcleo */
void mq_espera_us(u32 us);		/* espera sin soltar el procesador */
u32 mq_rsdp(void);			/* dirección física de la RSDP (0: no hay ACPI) */
void mq_inicia(const struct ll_inicio *i);

/* ---- base.c: memoria, registro ---- */
void mem_inicia(void *ini, u32 tam);
void *mem_pide(u32 tam);
void mem_suelta(void *p);
extern u32 mem_usada, mem_max;
void reg(const char *f, ...) __attribute__((format(printf, 1, 2)));
void reg_v(const char *f, __builtin_va_list a);
u32 reg_saca(char *b, u32 tam);
extern u32 reg_perdidos;

/* ---- nucleo.c: la tarea ---- */
void tarea_duerme(u32 ms);		/* suelta el procesador hasta el próximo LL_PASO que toque */
extern int tarea_prisa;			/* LL_AHORA: acorta el descanso entre lecturas */
extern struct ll_inicio ini;
extern struct ll_apm apm;
extern struct ll_estado estado;
void apm_publica(void);
extern unsigned char pila_tarea_fin[];

/* ---- ec.c: el controlador embebido ---- */
int ec_prepara(void);			/* lo busca y pone su manejador (antes de los _INI) */
void ec_reg(int conecta);		/* _REG (3, conecta) */
void ec_eventos(void);			/* atiende los avisos pendientes (_Qxx) */
void *ec_disp(void);

/* ---- bateria.c ---- */
void bateria_tarea(void);		/* lo que hace la tarea: no vuelve */
void bateria_para(void);

/* ---- tarea.asm ---- */
void tarea_cambia(u32 *guarda_esp, u32 nuevo_esp);
#endif
