/* veredicto.h - (TERMO) lo que dice el Termómetro de una prueba de esfuerzo:
 * con qué sensor se mide (el del procesador o, si no hay, el que más
 * cambia; nunca una zona que no se mueve), cuánto sube, cómo se enfría y qué
 * le parece. Sin llamadas al sistema: lo usan TERMO.BIN (termo.c) y, en el
 * anfitrión, la prueba que lee un .LOG de verdad (pruebas/lee_log.c).
 * Las temperaturas, en décimas de grado; VE_NO, no se sabe.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef VEREDICTO_H
#define VEREDICTO_H

#define VE_NO ((int)0x80000000)
#define VE_MAXS 14
#define VE_NF_QUIETO 1		/* no se mueve con la carga */
#define VE_NF_IMPOSIBLE 2	/* fuera de lo posible, o por encima de su límite en reposo */

struct ve_sensor {
	char nombre[48];	/* (UTF-8) */
	int cpu;		/* 1: del procesador (o simulado) */
	int limite;		/* el suyo, o VE_NO */
	int no_fiable;		/* no cuenta (ni para la parada ni para el veredicto): VE_NF_* */
	int aproximado;		/* (los Atom) la cifra no es de fiar, la subida sí */
	int solo_carga;		/* solo da algo mientras se carga (los demás núcleos): no mide el enfriamiento */
};

/* la prueba entera: lo que se sabe de cada sensor */
struct ve_prueba {
	int ns;
	struct ve_sensor s[VE_MAXS];
	int ini[VE_MAXS], pico[VE_MAXS], lo[VE_MAXS], hi[VE_MAXS], parada[VE_MAXS], n[VE_MAXS];
	int ref;		/* el sensor con el que se mide (desde ve_para); -1 */
	int t_ini, pico_ref, t_parada;	/* los del de referencia */
	int baja30, baja60, vuelve_s, ultimo, ult_seg;
	int ocup_suma, ocup_n, ocup_max;	/* lo ocupado que está el procesador enfriando (%) */
	/* (TERMO) la frecuencia en la carga: la media medida (MHz, de
	 * ve_frecuencia), la nominal (la de la marca o la del TSC; 0: no se
	 * sabe), en cuántos procesadores se ha cargado y si se ha frenado
	 * (PROCHOT, temperatura, potencia) durante la carga: lo pone quien llama,
	 * después de ve_empieza */
	int mhz_suma, mhz_n, mhz_nominal, procesadores, frenado;
};

void ve_empieza(struct ve_prueba *p, int ns, const struct ve_sensor *s, const int *t);
void ve_carga(struct ve_prueba *p, const int *t);	/* una medida durante la carga */
int ve_para(struct ve_prueba *p, const int *t);		/* se para la carga: el de referencia */
/* una medida enfriando (seg desde que se paró): 1 si ya se puede acabar (ha
 * vuelto cerca de la de reposo y ha pasado el primer minuto) */
int ve_enfria(struct ve_prueba *p, int seg, const int *t);
/* lo ocupado que está el procesador (0-100) en una medida enfriando: si no
 * baja, el enfriamiento no se puede juzgar */
void ve_ocupacion(struct ve_prueba *p, int pct);
void ve_frecuencia(struct ve_prueba *p, int mhz);	/* una medida de la frecuencia durante la carga */
int ve_mhz_carga(const struct ve_prueba *p);		/* la media (0: no se sabe) */
int ve_mhz_nominal(const char *marca, int mhz_tsc);	/* la de la marca («@ 1.33GHz»), o la del TSC */
/* ¿ha ido lento sin frenarse (el estado P mínimo)? Escribe la frase */
int ve_lento(const struct ve_prueba *p, char *d, int cabe);
/* ¿es creíble la primera lectura de un sensor en reposo? (0-125 °C y por
 * debajo de su límite: el ThinkPad A275 daba 114,3 °C de 90, fijos) */
int ve_creible(int t, int limite);
int ve_fija(const struct ve_prueba *p, int k);		/* ¿no ha cambiado en toda la prueba? */
/* el resumen y el veredicto, en UTF-8 */
void ve_resumen(const struct ve_prueba *p, char *d, int cabe, int seg_carga, int seguridad, const char *motivo, int esc, int simula);
/* las décimas, «38,0 °C» (d: 24 octetos) */
char *ve_grados(char *d, int t);
/* la TjMax (°C) de un Intel que no la dice: familia, modelo, paso, la marca
 * de CPUID y, en los Core 2, el bit 30 del MSR EEh */
int ve_tjmax_intel(int fam, int modelo, int pasos, const char *marca, int movil_ee);
#endif
