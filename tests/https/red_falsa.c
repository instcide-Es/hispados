/* tests/https/red_falsa.c - una red de mentira para HTTPS.EXE (w32/https.c,
 * tal cual) en el anfitrión: WSOCK32 y HSPTLS que no salen del programa. Cada
 * conexión responde con el archivo RESP<n> de la carpeta RED (la variable de
 * entorno), la n-ésima; lo que se pide queda en PET<n>. TlsAbrir finge el
 * cifrado (los mismos octetos). Lo corre tests/run_https_tests.sh.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef unsigned int u32;
typedef unsigned short u16;
struct in_addr { u32 s_addr; };
struct sockaddr_in { short sin_family; u16 sin_port; struct in_addr sin_addr; char sin_zero[8]; };
struct hostent { char *h_name; char **h_aliases; short h_addrtype, h_length; char **h_addr_list; };

static int con, pos;
static char *resp;
static long n_resp;
static const char *carpeta(void) { const char *r = getenv("RED"); return r ? r : "."; }

int WSAStartup(u32 v, void *d) { (void)v; (void)d; return 0; }
int WSACleanup(void) { return 0; }
int WSAGetLastError(void) { return 0; }
struct hostent *gethostbyname(const char *n) {
  static char ip[4] = { 10, 0, 2, 2 }, *lista[2] = { ip, NULL };
  static struct hostent h = { NULL, NULL, 2, 4, lista };
  FILE *f;
  char r[600];
  snprintf(r, sizeof r, "%s/SERVIDORES", carpeta());
  if ((f = fopen(r, "a"))) { fprintf(f, "%s\n", n); fclose(f); }
  return &h;
}
char *inet_ntoa(struct in_addr a) { (void)a; return "10.0.2.2"; }
u16 htons(u16 p) { return (u16)(p << 8 | p >> 8); }
u32 socket(int a, int b, int c) {
  char r[600];
  FILE *f;
  (void)a; (void)b; (void)c;
  con++;
  pos = 0;
  free(resp); resp = NULL; n_resp = 0;
  snprintf(r, sizeof r, "%s/RESP%d", carpeta(), con);
  if ((f = fopen(r, "rb"))) {
    fseek(f, 0, SEEK_END); n_resp = ftell(f); fseek(f, 0, SEEK_SET);
    resp = malloc(n_resp + 1);
    if (resp && fread(resp, 1, n_resp, f) != (size_t)n_resp) n_resp = 0;
    fclose(f);
  }
  return 5;
}
int connect(u32 s, const struct sockaddr_in *a, int n) { (void)s; (void)a; (void)n; return 0; }
int send(u32 s, const char *b, int n, int fl) {
  char r[600];
  FILE *f;
  (void)s; (void)fl;
  snprintf(r, sizeof r, "%s/PET%d", carpeta(), con);
  if ((f = fopen(r, "ab"))) { fwrite(b, 1, n, f); fclose(f); }
  return n;
}
int recv(u32 s, char *b, int n, int fl) {
  int k = n_resp - pos;
  (void)s; (void)fl;
  if (k > n) k = n;
  if (k > 100) k = 100;              /* (a trozos, como la red) */
  if (k <= 0) return 0;
  memcpy(b, resp + pos, k);
  pos += k;
  return k;
}
int closesocket(u32 s) { (void)s; return 0; }
/* HSPTLS de mentira: lo mismo, «cifrado» */
typedef struct TLS { int x; } TLS;
static TLS uno;
TLS *TlsAbrir(u32 s, const char *n) { (void)s; (void)n; return &uno; }
int TlsConectar(TLS *t, int ms) { (void)t; (void)ms; return 1; }
int TlsLeer(TLS *t, void *b, int n, int ms) { (void)t; (void)ms; return recv(0, b, n, 0); }
int TlsEscribir(TLS *t, const void *b, int n) { (void)t; return send(0, b, n, 0); }
void TlsCerrar(TLS *t) { (void)t; }
int TlsError(TLS *t) { (void)t; return 0; }
const char *TlsTextoError(int e) { (void)e; return "-"; }
int TlsInfo(TLS *t, char *b, int n) { (void)t; (void)n; strcpy(b, "TLS de mentira"); return 0; }
u32 TlsCiclos(TLS *t) { (void)t; return 0; }
u32 GetTickCount(void) { return 0; }
int _strnicmp(const char *a, const char *b, u32 n) { return strncasecmp(a, b, n); }
