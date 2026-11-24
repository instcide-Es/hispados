/* nvidia.h - (TERMO) (NVIDIA) las gráficas NVIDIA del Termómetro: lo que da
 * el núcleo (la llamada 225, termo.cas: la ficha de cada gráfica y la de
 * cada puerto PCI Express), cómo se lee y qué se dice de ellas en el .LOG,
 * en INFORME.TXT («GRÁFICAS») y en el resultado.
 * Lo de la decodificación y los textos no llama al sistema: se prueba en el
 * anfitrión con fichas de mentira (pruebas/nvidia_prueba.c, -DNV_ANFITRION).
 * Las temperaturas, en décimas de grado; NV_NO, no se sabe.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef NVIDIA_H
#define NVIDIA_H
#include <stdint.h>
#include "termo_acpi.h"

#define NV_NO ((int)0x80000000)
#define NV_MAX 4
#define NV_PUERTOS 16

/* la ficha de una gráfica (96 octetos; la 225 con EDX bit 0) */
struct nv_ficha {
	uint32_t bdf;		/* bus << 16 | aparato << 11 | función << 8 */
	uint32_t id;		/* fabricante | modelo << 16 */
	uint32_t estado;	/* NV_E_* */
	uint32_t boot;		/* PMC_BOOT_0 (BAR0+0) */
	uint32_t crudo;		/* el sensor: 20008h (Maxwell) o 20460h (Pascal) */
	uint32_t temp;		/* °C */
	uint32_t critico, frena;	/* 20408h y 20074h (Maxwell) */
	uint32_t subsis;	/* 2Ch: fabricante y modelo del subsistema */
	uint32_t orden0;	/* 04h la primera vez: la orden y el estado PCI */
	uint32_t bar0;		/* 10h */
	uint32_t pmcsr;		/* la capacidad de energía + 4 (0xFFFFFFFF: no la tiene) */
	uint32_t enl_cap, enl;	/* PCI Express + 0Ch y + 10h (control | estado << 16) */
	uint32_t puerto;	/* el puente del que cuelga (bdf | 80000000h; 0 ninguno) */
	uint32_t hecho;		/* NV_H_*: lo volátil que se ha hecho */
	uint32_t fusible;	/* BAR0+212A8h (el fusible 1A8h) */
	uint32_t r2000c;	/* BAR0+2000Ch */
	uint32_t rtemp;		/* BAR0+20400h (Maxwell) o 20460h (Pascal), tal cual */
	uint32_t clase;		/* 08h */
	uint32_t p_enl, p_ranura;	/* del puerto: PCI Express + 10h y + 18h */
	uint32_t libre[2];
};

/* la ficha de un puerto PCI Express (raíz o de bajada; 225 con 100h + k) */
struct nv_puerto {
	uint32_t bdf, id;
	uint32_t cap;		/* la palabra de la capacidad PCI Express (el tipo, bits 20-23) */
	uint32_t enl_cap, enl;	/* + 0Ch; + 10h (control | estado << 16) */
	uint32_t ranura_cap, ranura;	/* + 14h; + 18h (control | estado << 16) */
	uint32_t buses;		/* 18h */
	uint32_t detras_id, detras_clase;	/* lo del aparato 0 del bus de detrás (0xFFFFFFFF: nada) */
	uint32_t deven;		/* el DEVEN (54h) del 0:0.0 de Intel; 0xFFFFFFFF si no es Intel */
	uint32_t orden;		/* 04h */
	uint32_t ventana;	/* 20h: la ventana de memoria */
	uint32_t libre[11];
};

enum { NV_E_BIEN, NV_E_D3, NV_E_SIN_MEMORIA, NV_E_BAR0, NV_E_OTRA, NV_E_FUSIBLE, NV_E_APAGADO, NV_E_FORZADO, NV_E_IMPOSIBLE,
       NV_E_DORMIDA, NV_E_BADF, NV_E_N };
#define NV_H_MEMORIA 0x01	/* activada la memoria (bit 1 de la orden PCI) */
#define NV_H_D0 0x02		/* de D3hot a D0 (con No_Soft_Reset) */
#define NV_H_SENSOR 0x04	/* el sensor encendido como nouveau (20008h, 2000Ch) */
#define NV_H_D3_NO 0x10		/* en D3 sin No_Soft_Reset: no se despierta */
#define NV_H_FUERA 0x20		/* el BAR0 fuera de la ventana del puente: no se activa la memoria */

/* lo de una gráfica durante la prueba */
struct nv_gpu {
	struct nv_ficha f;
	int vista;		/* 1 si la ficha vale */
	int t;			/* la última (décimas) */
	int n[3], suma[3], min[3], max[3];	/* reposo (antes de la carga), carga, enfriando */
	int primera;		/* la primera lectura */
	int fallos;		/* lecturas sin dato después de haber dado alguna */
	int calla;		/* (ha dejado de contestar: PMC_BOOT_0 FFFFFFFFh o el enlace caído) */
	uint32_t estado_antes;
};

struct nv_todo {
	int n, np;
	struct nv_gpu g[NV_MAX];
	struct nv_puerto p[NV_PUERTOS];
	int hay_acpi;		/* lo de abajo vale (el servicio de ACPI lo ha dado) */
	uint32_t npeg;
	struct ta_peg peg[4];
};

const char *nv_modelo(uint32_t id);			/* «GTX 970M», o "" */
const char *nv_chip(uint32_t boot);			/* «GM204 (Maxwell)», o "" */
const char *nv_estado(uint32_t e);			/* el porqué, en palabras */
int nv_temperatura(const struct nv_ficha *f);
void nv_decodifica(struct nv_ficha *f);		/* estado y temperatura, de lo leído del BAR0 (como termo.cas) */		/* décimas, o NV_NO */
char *nv_enlace(char *d, int cabe, uint32_t cap, uint32_t enl);	/* «PCIe 3.0 x8 (puede 3.0 x16)» */
const char *nv_energia(uint32_t pmcsr);			/* «D0», «D3hot»... */
/* lo del ACPI de los puertos (de lo último del servicio de ACPI, la 223) */
void nv_acpi(struct nv_todo *t, const struct termo_acpi *a);
/* lo de cada gráfica y de cada puerto, para el .LOG y el INFORME */
void nv_describe(const struct nv_todo *t, int i, char *d, int cabe);
void nv_describe_puerto(const struct nv_todo *t, int k, char *d, int cabe);
/* una medida (fase: 0 reposo, 1 carga, 2 enfriando; 3, el resultado: como reposo no) */
void nv_apunta(struct nv_gpu *g, int fase, int t);
/* la sección «GRÁFICAS» de INFORME.TXT (con «\r\n») */
int nv_informe(const struct nv_todo *t, char *d, int cabe);
/* lo que va al resultado (vacío si nada que decir): más de 90 °C, no responde */
int nv_veredicto(const struct nv_todo *t, char *d, int cabe);

#ifndef NV_ANFITRION
/* (TERMO.BIN) por la 225: las gráficas y los puertos; con «escribe», lo
 * volátil de nouveau y de pci_enable_device (termo.cas) */
int nv_busca(struct nv_todo *t, int escribe);
int nv_lee(struct nv_todo *t, int i, int escribe);	/* la temperatura de ahora (décimas) o NV_NO */
#endif
#endif
