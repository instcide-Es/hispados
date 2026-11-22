/* ordenes.h - las órdenes ZIP y UNZIP (ordenes.c) y lo que les da la plataforma.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef ORDENES_H
#define ORDENES_H
#include <stdint.h>
#include <stdarg.h>
int orden_zip(int argc, char **argv);
int orden_unzip(int argc, char **argv);
int pl_progreso_orden(uint64_t hecho, uint64_t total, const char *nombre);
void pl_escribe(const char *t);			/* un texto de la página 437, a la salida */
int pl_cortado(void);				/* ¿Ctrl+C? */
const char *pl_pide_clave(const char *archivo);	/* la contraseña, si se puede preguntar (NULL si no) */
void pl_temporal(const char *ruta, char *sale, int cabe);	/* un nombre de al lado para escribir aparte */
#endif
