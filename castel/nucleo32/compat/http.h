/* http.h - (COMPAT) un cliente HTTP/1.1 pequeño sobre plat.h (TLS.BIN en
 * Hispa-Pentium): GET a un archivo (a trozos, con su avance y su suma) y
 * POST de un formulario multipart. Content-Length, «chunked» o hasta que se
 * cierre; una redirección como mucho, y solo a https://.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef COMPAT_HTTP_H
#define COMPAT_HTTP_H
#include "so.h"

typedef struct { char host[128], camino[220]; u32 puerto; int tls; } http_url;
int http_parte(const char *url, http_url *u);

/* lo que llega del cuerpo, a trozos: 0 para seguir; -1 para cortar */
typedef int (*http_trozo)(void *u, const u8 *d, u32 n, u32 total);

/* una petición: «cuerpo» (o NULL) con su tipo. *estado, el código HTTP.
 * 0 si se ha hecho (mire *estado); -1 sin conexión o cortada (*error) */
int http_pide(const char *url, const char *metodo, const char *tipo, const u8 *cuerpo, u32 n,
	      http_trozo trozo, void *u, int *estado, char *ubicacion, u32 cabe_ub, char *error, u32 cabe);
#endif
