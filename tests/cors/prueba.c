/* tests/cors/prueba.c - las reglas del origen de los Navegadores de Hispa-486
 * (w32/navega/cors.h, tal cual) en el anfitrión, con gcc: el origen, SameSite,
 * las cabeceras que puede mandar una página y si puede leer la respuesta.
 * La corre tests/run_navegador_cors_tests.sh.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../w32/navega/cors.h"

static int fallos;
static void mira(int si, const char *que) { printf("  %s %s\n", si ? "OK  " : "FAIL", que); if (!si) fallos = 1; }
static int origen_es(const char *u, const char *o) {
  char d[200];
  if (!o) return cors_origen(u, d, sizeof d) == 0;
  return cors_origen(u, d, sizeof d) && !strcmp(d, o);
}
static int tiene(const char *t, const char *s) { return t && strstr(t, s) != NULL; }

int main(void) {
  char *d = NULL;
  const char *por;
  char *v;
  /* el origen */
  mira(origen_es("https://Banco.ES/cuenta?x=1", "https://banco.es"), "origen: https://banco.es");
  mira(origen_es("http://banco.es:80/", "http://banco.es"), "origen: sin el puerto de siempre");
  mira(origen_es("https://banco.es:8443/a", "https://banco.es:8443"), "origen: con otro puerto");
  mira(origen_es("http://banco.es@malo.es/", "http://banco.es@malo.es"), "origen: con @ no es banco.es");
  mira(origen_es("about:blank", NULL), "origen: about: es opaco");
  mira(origen_es("file:///C:/a.htm", NULL), "origen: file: es opaco");
  mira(origen_es("javascript:alert(1)", NULL), "origen: javascript: es opaco");
  mira(origen_es("http://ba nco.es/", NULL), "origen: un espacio no vale");
  mira(origen_es("http://banco.es:99999/", NULL), "origen: puerto imposible");
  mira(cors_mismo("https://banco.es/a", "https://BANCO.es/b?c"), "mismo origen: el mismo servidor");
  mira(!cors_mismo("https://banco.es/", "http://banco.es/"), "mismo origen: http y https no");
  mira(!cors_mismo("https://banco.es/", "https://www.banco.es/"), "mismo origen: otro servidor no");
  mira(!cors_mismo("https://banco.es/", "https://banco.es:444/"), "mismo origen: otro puerto no");
  mira(!cors_mismo("about:blank", "about:blank"), "mismo origen: dos opacos no");
  /* SameSite (el sitio: los dos últimos trozos) */
  mira(!cors_cruzada("https://www.clicgest.es/", "https://admin.clicgest.es/x"), "sitio: www y admin de clicgest.es, el mismo");
  mira(cors_cruzada("https://malo.es/", "https://banco.es/"), "sitio: malo.es y banco.es, otro");
  mira(cors_cruzada("https://banco.es.malo.es/", "https://banco.es/"), "sitio: banco.es.malo.es no es banco.es");
  mira(!cors_cruzada(NULL, "https://banco.es/"), "sitio: desde la barra, no es de otro");
  /* lo que pide la página */
  por = cors_pide("GET", "Accept: */*\r\nX-Algo: 1\r\n", 0, "https://banco.es", &d);
  mira(!por && tiene(d, "X-Algo: 1\r\n") && !tiene(d, "Origin"), "pide: al mismo origen, sus cabeceras y sin Origin");
  free(d);
  por = cors_pide("GET", "Cookie: sesion=robada\r\nHost: banco.es\r\nOrigin: https://banco.es\r\nSec-Fetch-Site: same-origin\r\nUser-Agent: x\r\nAccept: */*\r\n", 0, "https://malo.es", &d);
  mira(!por && !tiene(d, "Cookie") && !tiene(d, "Host") && !tiene(d, "Origin") && !tiene(d, "Sec-") && !tiene(d, "User-Agent") && tiene(d, "Accept: */*"),
       "pide: sin las prohibidas (Cookie, Host, Origin, Sec-, User-Agent)");
  free(d);
  por = cors_pide("GET", "X-A: uno\rCookie: robada\r\n", 0, "https://malo.es", &d);
  mira(!por && !tiene(d, "X-A") && !tiene(d, "robada"), "pide: un \\r suelto en el valor, fuera");
  free(d);
  por = cors_pide("GET", "Mal nombre: 1\r\n: 2\r\nsin dos puntos\r\n", 0, "https://malo.es", &d);
  mira(!por && d && !d[0], "pide: las mal hechas, fuera");
  free(d);
  por = cors_pide("GET / HTTP/1.1\r\nHost: banco.es\r\n\r\nGET", "", 0, "https://malo.es", &d);
  mira(por != NULL && !d, "pide: un método con espacios y saltos no vale");
  por = cors_pide("TRACE", "", 0, "https://malo.es", &d);
  mira(por != NULL, "pide: TRACE no");
  por = cors_pide("GET", "Accept: */*\r\n", 1, "https://malo.es", &d);
  mira(!por && tiene(d, "Origin: https://malo.es\r\n"), "pide: a otro origen, con Origin");
  free(d);
  por = cors_pide("POST", "Content-Type: text/plain;charset=UTF-8\r\nAccept: */*\r\n", 1, "https://malo.es", &d);
  mira(!por, "pide: a otro origen, POST de texto (simple)");
  free(d);
  por = cors_pide("POST", "Content-Type: application/x-www-form-urlencoded;charset=UTF-8\r\n", 1, "https://malo.es", &d);
  mira(!por, "pide: a otro origen, POST de formulario (simple)");
  free(d);
  por = cors_pide("POST", "Content-Type: application/json\r\n", 1, "https://malo.es", &d);
  mira(por != NULL && !d, "pide: a otro origen, JSON no (pediría OPTIONS)");
  por = cors_pide("GET", "X-Requested-With: XMLHttpRequest\r\n", 1, "https://malo.es", &d);
  mira(por != NULL, "pide: a otro origen, una cabecera propia no");
  por = cors_pide("PUT", "", 1, "https://malo.es", &d);
  mira(por != NULL, "pide: a otro origen, PUT no");
  por = cors_pide("DELETE", "", 0, "https://banco.es", &d);
  mira(!por, "pide: al mismo origen, DELETE sí");
  free(d);
  por = cors_pide("GET", "Cookie: x=1\r\n", 1, NULL, &d);
  mira(!por && tiene(d, "Origin: null\r\n") && !tiene(d, "Cookie"), "pide: desde una página opaca, Origin: null");
  free(d);
  /* lo que puede leer la página */
  {
    const char *c1 = "\nHTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    const char *c2 = "\nHTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: https://malo.es\r\n\r\n";
    const char *c3 = "\nHTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: https://malo.es\r\nAccess-Control-Allow-Credentials: true\r\n\r\n";
    const char *c4 = "\nHTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    const char *c5 = "\nHTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: https://malo.es.otro.es\r\n\r\n";
    const char *c6 = "\nHTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: https://malo.es\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    mira(cors_deja(c1, "https://malo.es", 0), "deja: «*» sin galletas");
    mira(!cors_deja(c1, "https://malo.es", 1), "deja: «*» con galletas, no");
    mira(cors_deja(c2, "https://malo.es", 0), "deja: el origen exacto");
    mira(!cors_deja(c2, "https://malo.es", 1), "deja: con galletas sin Allow-Credentials, no");
    mira(cors_deja(c3, "https://malo.es", 1), "deja: con galletas y Allow-Credentials: true");
    mira(!cors_deja(c4, "https://malo.es", 0), "deja: sin Access-Control-Allow-Origin, no");
    mira(!cors_deja(c5, "https://malo.es", 0), "deja: otro origen que empieza igual, no");
    mira(!cors_deja(c6, "https://malo.es", 0), "deja: dos Access-Control-Allow-Origin, no");
    mira(!cors_deja(c2, NULL, 0), "deja: página opaca y un origen, no");
  }
  /* las cabeceras que ve la página */
  {
    const char *c = "\nHTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nSet-Cookie: sesion=S1; HttpOnly\r\nX-Secreto: 7\r\n"
                    "X-Visible: 8\r\nAccess-Control-Expose-Headers: x-visible\r\n\r\n";
    v = cors_visibles(c, 0, 0);
    mira(v && !tiene(v, "Set-Cookie") && tiene(v, "X-Secreto: 7") && tiene(v, "HTTP/1.1 200 OK\r\n") && v[0] == '\n',
         "visibles: al mismo origen todas menos Set-Cookie");
    free(v);
    v = cors_visibles(c, 1, 0);
    mira(v && !tiene(v, "Set-Cookie") && !tiene(v, "X-Secreto") && tiene(v, "X-Visible: 8") && tiene(v, "Content-Type: text/plain"),
         "visibles: a otro origen, las simples y las de Expose-Headers");
    free(v);
  }
  printf(fallos ? "  HAY FALLOS\n" : "  todo bien\n");
  return fallos;
}
