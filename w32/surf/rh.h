/* rh.h - el puente con el HTTP del Navegador propio (w32/surf/red_http.c)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef RH_H
#define RH_H
/* una petición: url, POST (con su cuerpo y su Content-Type), cabeceras de
 * más («Nombre: valor\r\n»...), sub (no es la página: SameSite) y lo más
 * grande que se acepta (0: 4 MB) */
void *rh_nueva(const char *url, int post, const char *cuerpo, unsigned n_cuerpo, const char *tipo,
               const char *cab, int sub, unsigned max);
int rh_avanza(void *r);                 /* 0 sigue, 1 hecha, -1 error */
int rh_con_cabeceras(void *r);          /* ya han llegado las cabeceras */
int rh_estado(void *r);                 /* 200, 302... */
const char *rh_cabeceras(void *r);      /* «\nHTTP/1.1 200 OK\r\nNombre: valor\r\n...» */
const unsigned char *rh_datos(void *r, unsigned *n);   /* el cuerpo que ha llegado */
const char *rh_error(void *r);          /* en UTF-8 */
int rh_cifrada(void *r);
void rh_suelta(void *r);
void rh_inicia(const char *idioma);
void rh_fin(void);
void rh_galletas_carga(const char *ruta);
/* [JS] para fetch y XMLHttpRequest (w32/surf/guion.c) */
void rh_metodo(void *r, const char *metodo);   /* PUT, DELETE...: tras rh_nueva */
char *rh_galletas_de(const char *url);         /* document.cookie (malloc) o NULL */
void rh_pon_galleta(const char *url, const char *linea);
int rh_galletas_guarda(const char *ruta);
#endif
