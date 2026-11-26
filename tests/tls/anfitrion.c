/* tests/tls/anfitrion.c - HSPTLS.DLL (w32/tls/hsptls.c, el código tal cual,
 * incluido aquí con TLS_ANFITRION, con BearSSL) en el anfitrión, con gcc y un
 * KERNEL32 y un WSOCK32 de mentira. «C:\» es la carpeta RAIZ (la variable de
 * entorno) y el programa está en C:\PROG\. Lo corre tests/run_tls_tests.sh:
 *  - la semilla: se guarda junto al programa o, si ahí no se puede, en C:\;
 *    sin TSC ni semilla, «poco azar» (también en TlsInfo);
 *  - las autoridades de más: solo los *.CER de AUTORID\, no los de la
 *    carpeta del programa;
 *  - TlsEscribir no se queda colgado si el servidor no lee;
 *  - la memoria de certificados: no da por buena una cadena caducada.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#define TLS_ANFITRION 1
#include <string.h>
#include "../../w32/tls/hsptls.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <glob.h>
#include <unistd.h>

static int fallos, tsc_hay, solo_leer, desplaza;
static u32 reloj = 1000;
u32 anf_pit(void) { return reloj & 0xFFFF; }
int anf_tsc(void) { return tsc_hay; }
static void mira(int si, const char *que) { printf("  %s %s\n", si ? "OK  " : "FAIL", que); if (!si) fallos = 1; }

/* «C:\PROG\X» -> RAIZ/PROG/X */
static void ruta_de(const char *w, char *d) {
  const char *r = getenv("RAIZ");
  int n;
  snprintf(d, 600, "%s/", r ? r : ".");
  n = strlen(d);
  if (w[1] == ':') w += 3;
  for (; *w && n < 598; w++) d[n++] = *w == '\\' ? '/' : *w;
  d[n] = 0;
}
int WINAPI send(u32 s, const char *b, int n, int f) { (void)s; (void)b; (void)n; (void)f; return -1; }   /* el servidor no lee */
int WINAPI recv(u32 s, char *b, int n, int f) { (void)s; (void)b; (void)n; (void)f; return -1; }
int WINAPI ioctlsocket(u32 s, u32 c, u32 *a) { (void)s; (void)c; (void)a; return 0; }
int WINAPI WSAGetLastError(void) { return 10035; }
void WINAPI Sleep(u32 ms) { (void)ms; reloj += 50; }
u32 WINAPI GetTickCount(void) { return reloj += 1; }
void WINAPI GetLocalTime(ST *st) {
  time_t t = time(NULL) + (time_t)desplaza * 86400;
  struct tm *g = gmtime(&t);
  st->anio = g->tm_year + 1900; st->mes = g->tm_mon + 1; st->dia = g->tm_mday; st->sem = g->tm_wday;
  st->hora = g->tm_hour; st->min = g->tm_min; st->seg = g->tm_sec; st->ms = 0;
}
u32 WINAPI GetModuleFileNameA(HANDLE h, char *b, u32 n) { (void)h; (void)n; strcpy(b, "C:\\PROG\\HTTPS.EXE"); return 17; }
HANDLE WINAPI CreateFileA(const char *n, u32 acc, u32 c, void *sec, u32 disp, u32 at, HANDLE pl) {
  char r[600];
  FILE *f;
  (void)c; (void)sec; (void)at; (void)pl;
  ruta_de(n, r);
  if ((acc & 0x40000000u) && solo_leer && strstr(r, "/PROG/")) return (HANDLE)-1;   /* (un disquete protegido) */
  f = fopen(r, disp == 2 ? "wb" : "rb");
  return f ? (HANDLE)f : (HANDLE)-1;
}
int WINAPI ReadFile(HANDLE h, void *b, u32 n, u32 *l, void *o) { (void)o; *l = fread(b, 1, n, (FILE *)h); return 1; }
int WINAPI WriteFile(HANDLE h, const void *b, u32 n, u32 *l, void *o) { (void)o; *l = fwrite(b, 1, n, (FILE *)h); return 1; }
int WINAPI CloseHandle(HANDLE h) { fclose((FILE *)h); return 1; }
static glob_t gl;
static size_t gl_i;
static void da_nombre(FINDA *f) { const char *p = strrchr(gl.gl_pathv[gl_i], '/'); strcpy(f->nombre, p ? p + 1 : gl.gl_pathv[gl_i]); }
HANDLE WINAPI FindFirstFileA(const char *n, FINDA *f) {
  char r[600];
  ruta_de(n, r);
  if (glob(r, 0, NULL, &gl) || !gl.gl_pathc) return (HANDLE)-1;
  gl_i = 0; da_nombre(f);
  return (HANDLE)&gl;
}
int WINAPI FindNextFileA(HANDLE h, FINDA *f) { (void)h; if (++gl_i >= gl.gl_pathc) return 0; da_nombre(f); return 1; }
int WINAPI FindClose(HANDLE h) { (void)h; globfree(&gl); return 1; }

static u8 *lee(const char *nombre, u32 *n) {
  char r[600];
  FILE *f;
  u8 *b;
  snprintf(r, sizeof r, "%s/%s", getenv("RAIZ"), nombre);
  if (!(f = fopen(r, "rb"))) { *n = 0; return NULL; }
  b = malloc(65536);
  *n = fread(b, 1, 65536, f);
  fclose(f);
  return b;
}
static int existe(const char *nombre) { u32 n; u8 *b = lee(nombre, &n); free(b); return b && n == 32; }
static void borra(const char *nombre) { char r[600]; snprintf(r, sizeof r, "%s/%s", getenv("RAIZ"), nombre); unlink(r); }
/* la cadena (hoja, raíz) a la comprobación, como la daría BearSSL */
static unsigned cadena(TLS *t, const u8 *hoja, u32 nh) {
  const br_x509_class **x = &t->xv;
  (*x)->start_chain(x, t->servidor);
  (*x)->start_cert(x, nh); (*x)->append(x, hoja, nh); (*x)->end_cert(x);
  return (*x)->end_chain(x);
}

int main(int argc, char **argv) {
  const char *que = argc > 1 ? argv[1] : "";
  if (!strcmp(que, "semilla")) {
    borra("PROG/HSPTLS.SEM"); borra("HSPTLS.SEM");
    tsc_hay = 0; pozo_listo = 0; pozo_inicia();
    mira(existe("PROG/HSPTLS.SEM") && !existe("HSPTLS.SEM"), "semilla: se guarda junto al programa");
    mira(poco_azar == 1, "semilla: la primera vez, sin TSC: «poco azar»");
    pozo_listo = 0; pozo_inicia();
    mira(poco_azar == 0, "semilla: la segunda vez ya hay semilla");
    borra("PROG/HSPTLS.SEM");
    solo_leer = 1; pozo_listo = 0; pozo_inicia();
    mira(!existe("PROG/HSPTLS.SEM") && existe("HSPTLS.SEM"), "semilla: si junto al programa no se puede escribir, en C:\\");
    pozo_listo = 0; pozo_inicia();
    mira(poco_azar == 0, "semilla: y la de C:\\ se lee la vez siguiente");
    solo_leer = 0; borra("HSPTLS.SEM");
    tsc_hay = 1; pozo_listo = 0; pozo_inicia();
    mira(poco_azar == 0, "semilla: con TSC no es «poco azar»");
    {
      TLS *t;
      char b[200];
      borra("PROG/HSPTLS.SEM"); tsc_hay = 0; pozo_listo = 0; pozo_inicia();
      t = TlsAbrir(5, "x.es");
      t->hecho = 1;
      TlsInfo(t, b, sizeof b);
      mira(strstr(b, "TLS 1.2") && strstr(b, "poco azar"), "TlsInfo: lo dice");
      TlsCerrar(t);
    }
  } else if (!strcmp(que, "anclas")) {
    /* RAIZ.CER en C:\PROG\ y, si argv[2] es «autorid», también en AUTORID\ */
    pozo_listo = 0; pozo_inicia(); n_anclas = 0; anclas_inicia();
    if (argc > 2 && !strcmp(argv[2], "autorid")) mira(n_anclas == TAs_NUM + 1, "autoridades: la de AUTORID\\ vale");
    else mira(n_anclas == TAs_NUM, "autoridades: un *.CER suelto junto al programa no vale");
  } else if (!strcmp(que, "escribe")) {
    TLS *t;
    int r;
    pozo_listo = 0; pozo_inicia();
    t = TlsAbrir(5, "x.es");
    alarm(20);
    r = TlsEscribir(t, "hola", 4);
    mira(r == -1 && t->error == -10060, "TlsEscribir: si el servidor no lee, a los 60 s se da por cortada (no se cuelga)");
    TlsCerrar(t);
  } else if (!strcmp(que, "memoria")) {
    /* la hoja de x.es (vale 3 días), firmada por la raíz de AUTORID\ */
    u32 nh;
    u8 *hoja = lee("HOJA.DER", &nh);
    TLS *t;
    unsigned r;
    borra("PROG/HSPTLS.CCH");
    pozo_listo = 0; pozo_inicia(); n_anclas = 0; mem_leida = 0; memset(memoria, 0, sizeof memoria);
    desplaza = 0;
    t = TlsAbrir(5, "x.es");
    r = cadena(t, hoja, nh);
    mira(r == 0 && !t->de_memoria, "memoria: la primera vez se comprueba entera y vale");
    { unsigned u = 0; (*t->xv->get_pkey)(&t->xv, &u); mira(u == memoria[mem_sig].usos || u, "memoria: se apuntan los usos de la clave"); }
    TlsCerrar(t);
    t = TlsAbrir(5, "x.es");
    r = cadena(t, hoja, nh);
    { unsigned u = 0, u0 = memoria[0].usos | memoria[1].usos; (*t->xv->get_pkey)(&t->xv, &u);
      mira(r == 0 && t->de_memoria && u == u0 && u != (BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN), "memoria: la segunda, de memoria, con los usos de verdad (no KEYX|SIGN)"); }
    TlsCerrar(t);
    desplaza = 5;                                /* 5 días después: la hoja ya ha caducado */
    t = TlsAbrir(5, "x.es");
    r = cadena(t, hoja, nh);
    mira(r == BR_ERR_X509_EXPIRED && !t->de_memoria, "memoria: caducada, no vale aunque esté recordada (antes valía 7 días)");
    TlsCerrar(t);
    free(hoja);
  }
  return fallos;
}
