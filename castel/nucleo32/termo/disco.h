/* disco.h - (TERMO) los discos de dentro del equipo: quiénes son, su salud
 * (SMART de los SATA e IDE, el registro de salud de los NVMe, el desgaste
 * del eMMC) y una prueba de lectura (SOLO LEER: lo leído no sale del
 * núcleo; tdisco.cas, la llamada 238). Lo de entender los datos y escribir
 * el informe no llama al sistema: lo prueba también el anfitrión
 * (pruebas/disco_prueba.c, con datos simulados; -DDI_ANFITRION).
 * TERMO.BIN /DISCO, una tarea hija lanzada al abrir el Termómetro (antes
 * de la carga): deja TERMO\DISCOS.TXT (la sección del informe),
 * TERMO\DISCOS.VER (una línea para el veredicto), TERMO\DISCOS.EST (cómo
 * va) y TERMO\DISCOS.LOG (el detalle: todos los atributos).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef DISCO_H
#define DISCO_H

#define DI_MAX 8
#define DI_ATRIB 30

#define DI_IDE 1
#define DI_SATA 2
#define DI_EMMC 7
#define DI_NVME 8

#define DI_BIEN 0
#define DI_ATENCION 1
#define DI_MAL 2
#define DI_NOSE 3

struct di_atrib {
	int id, valor, peor, umbral;	/* (umbral: -1, no se sabe) */
	unsigned long long crudo;	/* los 6 octetos */
};

/* la prueba de lectura: las latencias, en microsegundos (por cada lectura de 64 K) */
struct di_lectura {
	int hecha;		/* 0: no se ha hecho; 1, sí */
	int lecturas, errores, reintentos, lentas;	/* lentas: más de 500 ms */
	int mb_saltos, mb_seguido;	/* lo leído (MB) en cada parte */
	unsigned us_saltos, us_seguido;	/* lo que han tardado en total */
	unsigned media_us, p99_us, max_us;
	int cortada;		/* se acabó el tiempo (60 s) antes de leerlo todo */
	int con_carga;		/* la prueba de esfuerzo estaba en marcha a la vez */
	unsigned err_mb[4];	/* dónde (MB desde el principio) los primeros errores */
};

struct di_disco {
	int clase;		/* DI_IDE, DI_SATA, DI_EMMC, DI_NVME */
	unsigned sectores, bloque, vida;	/* (sectores de 512; vida: la del eMMC, Sdhci.vida) */
	char modelo[48], serie[8], firmware[12];	/* (serie: las 4 últimas letras) */
	int ssd;		/* 1: sin partes que giran (IDENTIFY 217 = 1, o NVMe/eMMC) */
	int rpm;		/* (los que giran, si lo dicen) */
	/* SATA/IDE */
	int smart;		/* 1 leído; 0 el disco no tiene; -1 lo tiene apagado; -2 lo rechaza */
	int estado;		/* SMART RETURN STATUS: 0 bien, 1 umbral pasado, 2 no se sabe, -1 sin pedir */
	int nat;
	struct di_atrib at[DI_ATRIB];
	int errores_log;	/* los errores que ha apuntado el disco en su vida (registro 01h); -1 */
	/* NVMe */
	int nvme;		/* 1: con el registro de salud */
	int critico, temp, repuesto, repuesto_umbral, usado;
	unsigned long long horas, encendidos, apagados_mal, errores_medio, errores_reg, leidos_gb, escritos_gb;
	struct di_lectura lee;
	/* lo que se dice */
	int salud;		/* DI_BIEN... */
	char porque[700];	/* en palabras, separado por «; » */
};

/* entender lo que da el disco (buffers tal cual) */
void di_empieza(struct di_disco *d, int clase, unsigned sectores, unsigned bloque, unsigned vida, const char *modelo);
void di_identify_ata(struct di_disco *d, const unsigned char *id);
void di_smart_ata(struct di_disco *d, const unsigned char *datos, const unsigned char *umbrales);
void di_errores_ata(struct di_disco *d, const unsigned char *reg);
void di_identify_nvme(struct di_disco *d, const unsigned char *id);
void di_salud_nvme(struct di_disco *d, const unsigned char *reg);
/* la prueba de lectura: lo medido (las latencias de cada lectura, en us) */
void di_lectura_cuenta(struct di_lectura *l, const unsigned *us, int n);
const struct di_atrib *di_atributo(const struct di_disco *d, int id);
/* BIEN / ATENCIÓN / MAL, y el porqué en palabras */
void di_juzga(struct di_disco *d);
const char *di_salud_texto(int s);
/* la sección «Discos» del informe (UTF-8, con \r\n) */
int di_informe(const struct di_disco *d, int n, int escritos, char *t, int cabe);
/* el detalle (todos los atributos) para DISCOS.LOG */
int di_detalle(const struct di_disco *d, char *t, int cabe);
/* la línea del veredicto: lo peor y por qué (vacía si todo bien); con
 * parones, «puede ser la causa de los parones» */
int di_veredicto(const struct di_disco *d, int n, int parones, char *t, int cabe);

#ifndef DI_ANFITRION
/* (TERMO.BIN) la tarea hija y lo de la tarea principal */
int disco_principal(const char *carpeta);
void disco_lanza(const char *carpeta);
int disco_mira(const char *carpeta, char *aviso, int cabe);	/* 1 si ha cambiado (aviso, para el .LOG y la pantalla) */
int disco_informe(const char *carpeta, char *t, int cabe);	/* DISCOS.TXT, para INFORME.TXT */
void disco_veredicto(const char *carpeta, char *resumen, int cabe, int parones);	/* añade al resumen lo de DISCOS.VER (una vez) */
void disco_pausa(const char *carpeta, int si);	/* la prueba de esfuerzo empieza (1) o acaba (0): la lectura espera */
int disco_acabado(void);
#endif
#endif
