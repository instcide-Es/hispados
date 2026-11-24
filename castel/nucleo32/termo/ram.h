/* ram.h - (TERMO, RAM) la prueba de la memoria RAM del Termómetro (ram.c):
 * la tarea hija que prueba (TERMO.BIN /RAM [/COMPLETA]) y lo que usa el
 * Termómetro para lanzarla, seguirla y ponerla en INFORME.TXT.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef RAM_H
#define RAM_H

/* TERMO.BIN /RAM: la prueba (va diciendo en TERMO\RAM.EST; el resultado, en
 * TERMO\RAM.TXT); se para con TERMO\RAM.PAR. 0 BIEN, 1 MAL, 2 cancelada */
int ram_principal(const char *carpeta_termo, int completa, int simula_fallo);

/* (en el Termómetro) al abrir: lo de la vez anterior (si se quedó a medias:
 * el equipo se colgó o se apagó probando la memoria) -> 1 y la frase en d */
int ram_al_abrir(const char *carpeta_termo, char *d, int cabe);
/* ¿lleva la línea del cargador esa palabra? («ramcompleta»: la completa al
 * abrirse; «ramrapida»: la rápida; «ramsimula»: con un fallo de mentira) */
int ram_linea(const char *palabra);
/* lanzarla (0 si no se puede: lo dice en ram_aviso) */
int ram_lanza(const char *carpeta_termo, int completa, int simula_fallo);
void ram_para(void);
int ram_activa(void);
/* cada 2 s: 1 si ha cambiado lo que dice (en d, para el .LOG); *acabada, 1 al acabar */
int ram_mira(char *d, int cabe, int *acabada);
const char *ram_aviso(void);	/* para la línea de abajo */
/* la sección «MEMORIA RAM» de INFORME.TXT (con sus marcas, para poder
 * cambiarla luego sola); y la frase para el RESULTADO (0 si no se ha probado) */
int ram_informe(char *d, int cabe);
int ram_veredicto(char *d, int cabe);
/* -1 sin probar, 0 BIEN, 1 MAL */
int ram_resultado(void);
/* pone (o cambia) la sección en el INFORME.TXT que haya (si no hay, lo
 * empieza): para cuando la prueba de la RAM acaba después del informe */
void ram_pon_en_informe(const char *carpeta_termo);
#endif
