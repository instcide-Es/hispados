/* nucleo.c - Núcleo ACPI de Hispa-486 (HSPACPI.BIN): las llamadas del
 * residente y la tarea.
 *
 * ACPICA corre en una «tarea» con su propia pila. El residente la deja
 * correr con LL_PASO (en la instalación y luego desde el tic del reloj); la
 * tarea corre hasta que un método del equipo pide dormir (Sleep) o hasta que
 * acaba la lectura, y entonces vuelve al residente diciendo cuántos tics
 * faltan para que le toque otra vez. Así una lectura de la batería (que en
 * el Toshiba NB250 duerme 20 ms cinco veces) no para el equipo: son unos
 * milisegundos por tic, con las interrupciones cerradas solo ese rato.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "nucleo.h"

#define PILA_TAREA 131072
static unsigned char pila_tarea[PILA_TAREA] __attribute__((aligned(16)));
static u32 esp_tarea, esp_llamada;
static int en_tarea, tarea_muerta, tarea_hecha;
static u64 despierta_us;
int tarea_prisa;
static int descansando;

struct ll_inicio ini;
struct ll_apm apm;
struct ll_estado estado;
extern u32 osl_errores;
extern char _fin_bss[];

static void tarea_entra(void)
{
	bateria_tarea();
	for (;;)
		tarea_duerme(60000);
}

static void tarea_crea(void)
{
	u32 *p = (u32 *)(pila_tarea + PILA_TAREA);

	*--p = 0;			/* (alineación) */
	*--p = 0;			/* adonde «volvería» tarea_entra */
	*--p = (u32)tarea_entra;
	*--p = 0;			/* ebp, ebx, esi, edi */
	*--p = 0;
	*--p = 0;
	*--p = 0;
	esp_tarea = (u32)p;
	tarea_hecha = 1;
	tarea_muerta = 0;
	despierta_us = 0;
}

/* Desde la tarea: dormir. Los ratos muy cortos se esperan sin soltar. */
void tarea_duerme(u32 ms)
{
	if (!en_tarea || ms < 3) {
		mq_espera_us(ms * 1000);
		return;
	}
	despierta_us = mq_ahora_us() + (u64)ms * 1000;
	tarea_cambia(&esp_tarea, esp_llamada);
}

/* el descanso entre lecturas: LL_AHORA lo corta (-> 1) */
int tarea_descansa(u32 ms)
{
	u64 fin = mq_ahora_us() + (u64)ms * 1000;
	int prisa;

	descansando = 1;
	while (!tarea_prisa && mq_ahora_us() < fin) {
		u64 q = (fin - mq_ahora_us()) / 1000 + 1;

		tarea_duerme(q > 1000 ? 1000 : q < 3 ? 3 : q);
	}
	prisa = tarea_prisa;
	tarea_prisa = 0;
	descansando = 0;
	return prisa;
}

static int paso(void)
{
	u64 ahora;

	if (!tarea_hecha || tarea_muerta)
		return -1;
	ahora = mq_ahora_us();
	if (ahora >= despierta_us || (tarea_prisa && descansando)) {
		en_tarea = 1;
		tarea_cambia(&esp_llamada, esp_tarea);
		en_tarea = 0;
		ahora = mq_ahora_us();
	}
	if (despierta_us <= ahora)
		return 1;
	/* un tic del reloj son 54,9 ms */
	return (u32)((despierta_us - ahora) / 54925) + 1;
}

/* lo cocinado, al residente (está en memoria convencional) */
void apm_publica(void)
{
	estado.apm = apm;
	if (ini.dir_apm)
		memcpy(mq_mapa(ini.dir_apm, sizeof apm), &apm, sizeof apm);
}

int nucleo_llamada(u32 f, u32 dir_pet)
{
	u32 *pet = dir_pet ? mq_mapa(dir_pet, 16) : 0;

	switch (f) {
	case LL_INICIA:
		if (!pet)
			return -2;
		memcpy(&ini, pet, sizeof ini);
		mq_inicia(&ini);
#ifndef ANFITRION
		mem_inicia(_fin_bss, ini.tam - (u32)_fin_bss);
#endif
		memset(&estado, 0, sizeof estado);
		memset(&apm, 0, sizeof apm);
		estado.version = LL_VERSION;
		apm.ca = apm.estado = apm.pct = 0xFF;
		apm.minutos = 0xFFFF;
		apm.banderas = 0x80;
		apm_publica();
		tarea_crea();
		return 0;
	case LL_PASO:
		return paso();
	case LL_REGISTRO:
		if (!pet)
			return -2;
		return reg_saca(mq_mapa(pet[0], pet[1]), pet[1]);
	case LL_ESTADO: {
		u32 n = pet && pet[1] < sizeof estado ? pet[1] : sizeof estado;

		if (!pet)
			return -2;
		estado.apm = apm;
		estado.memoria = mem_usada;
		estado.memoria_max = mem_max;
		estado.errores_acpica = osl_errores;
		memcpy(mq_mapa(pet[0], n), &estado, n);
		return n;
	}
	case LL_PARA:
		/* solo si la tarea no está a medias de un método */
		if (tarea_hecha && !tarea_muerta && descansando)
			bateria_para();
		tarea_muerta = 1;
		return 0;
	case LL_AHORA:
		tarea_prisa = 1;
		return 0;
	}
	return -1;
}

/* una excepción del procesador dentro del núcleo (entrada.asm): se apunta
 * y la tarea se da por muerta; el residente sigue contestando lo último */
void nucleo_excepcion(u32 vector, u32 *pila)
{
	tarea_muerta = 1;
	en_tarea = 0;
	apm.fase = FASE_ERROR;
	apm.error = 1000 + vector;
	apm.banderas = 0x80;
	apm.pct = 0xFF;
	apm_publica();
	reg("NÚCLEO: excepción %u del procesador; en la pila: %08X %08X %08X %08X\n", vector, pila[0], pila[1],
	    pila[2], pila[3]);
}
