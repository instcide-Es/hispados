/* anfitrion.c - [JS] el JavaScript del Navegador de Hispa-486, en el
 * anfitrión (Linux, gcc -m32 con su biblioteca): html.c, pagina.c, http.c,
 * dom.c y script.c tal cual, con QuickJS compilado para Linux; lo de
 * Windows (Winsock, la ventana) se hace aquí con lo de Linux (http.c se
 * compila con sus funciones de Winsock renombradas: -Dsocket=w_socket...).
 * Para probar el DOM y los guiones de una página sin arrancar Hispa-DOS
 * (tests/run_navegador_js_tests.sh). Solo HTTP (sin TLS).
 *
 *    anfitrion http://servidor:puerto/pagina [órdenes.txt]
 *
 * Las órdenes (una por línea, # para comentar): «clic SELECTOR»,
 * «escribe SELECTOR | texto», «tecla SELECTOR | Abajo, Arriba, Intro, Esc o
 * Tab», «elige SELECTOR | índice», «foco SELECTOR», «espera ms», «vuelca» (la
 * página como texto), «registro» (NAVEGA.LOG), «responde si|no» (a los
 * confirm que vengan) y «js código» (se ejecuta en la página).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "dom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ---- lo de MSVCRT que Linux no tiene ---- */
int _stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int _strnicmp(const char *a, const char *b, u32 n) { return strncasecmp(a, b, n); }

/* ---- KERNEL32 y USER32 ---- */
u32 WINAPI GetTickCount(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (u32)(t.tv_sec * 1000 + t.tv_nsec / 1000000); }
typedef struct { u16 anio, mes, sem, dia, hora, min, seg, ms; } ST;
void WINAPI GetLocalTime(ST *s) { time_t t = time(NULL); struct tm *m = localtime(&t); s->anio = m->tm_year + 1900; s->mes = m->tm_mon + 1; s->dia = m->tm_mday; s->hora = m->tm_hour; s->min = m->tm_min; s->seg = m->tm_sec; s->ms = 0; s->sem = m->tm_wday; }
u32 WINAPI SetTimer(HANDLE h, u32 id, u32 ms, void *f) { (void)h; (void)ms; (void)f; return id; }
int WINAPI KillTimer(HANDLE h, u32 id) { (void)h; (void)id; return 1; }
typedef struct { int left, top, right, bottom; } RECTJ;
int WINAPI GetClientRect(HANDLE h, RECTJ *r) { (void)h; r->left = r->top = 0; r->right = 976; r->bottom = 700; return 1; }

/* ---- Winsock (w_...), con los sockets de Linux ---- */
struct w_hostent { char *h_name; char **h_aliases; short h_addrtype, h_length; char **h_addr_list; };
typedef struct { u32 fd_count; u32 fd_array[64]; } w_fd_set;
static int ult_error;
int WINAPI WSAStartup(u32 v, void *d) { (void)v; (void)d; return 0; }
int WINAPI WSACleanup(void) { return 0; }
int WINAPI WSAGetLastError(void) { return ult_error; }
u32 WINAPI w_socket(int a, int b, int c) { (void)a; (void)b; (void)c; return (u32)socket(AF_INET, SOCK_STREAM, 0); }
int WINAPI w_connect(u32 s, const void *a, int n) {
  struct sockaddr_in d;
  memset(&d, 0, sizeof d);
  d.sin_family = AF_INET;
  memcpy(&d.sin_port, (const char *)a + 2, 2);
  memcpy(&d.sin_addr, (const char *)a + 4, 4);
  (void)n;
  return connect((int)s, (struct sockaddr *)&d, sizeof d) < 0 && errno != EINPROGRESS ? -1 : 0;
}
int WINAPI w_send(u32 s, const char *b, int n, int f) { int r = (int)send((int)s, b, n, f | MSG_NOSIGNAL); if (r < 0) ult_error = errno == EAGAIN ? 10035 : 10054; return r; }
int WINAPI w_recv(u32 s, char *b, int n, int f) { int r = (int)recv((int)s, b, n, f); if (r < 0) ult_error = errno == EAGAIN ? 10035 : 10054; return r; }
int WINAPI w_closesocket(u32 s) { return close((int)s); }
int WINAPI w_ioctlsocket(u32 s, u32 c, u32 *v) { (void)c; (void)v; return fcntl((int)s, F_SETFL, O_NONBLOCK); }
int WINAPI w_select(int n, w_fd_set *l, w_fd_set *e, w_fd_set *x, const void *t) {
  fd_set ee, xx;
  struct timeval cero = { 0, 0 };
  int s = (int)(e ? e->fd_array[0] : x->fd_array[0]), err = 0;
  socklen_t le = sizeof err;
  (void)n; (void)l; (void)t;
  FD_ZERO(&ee); FD_ZERO(&xx); FD_SET(s, &ee); FD_SET(s, &xx);
  select(s + 1, NULL, &ee, &xx, &cero);
  if (FD_ISSET(s, &ee)) getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &le);
  if (e) e->fd_count = FD_ISSET(s, &ee) && !err;
  if (x) x->fd_count = err != 0;
  return 0;
}
struct w_hostent *WINAPI w_gethostbyname(const char *n) {
  static struct w_hostent h;
  static char *l[2];
  static struct in_addr a;
  struct hostent *r = gethostbyname(n);
  if (!r) { ult_error = 11001; return NULL; }
  memcpy(&a, r->h_addr_list[0], 4);
  l[0] = (char *)&a; l[1] = NULL;
  h.h_addr_list = l;
  return &h;
}
u16 WINAPI w_htons(u16 x) { return htons(x); }
typedef struct TLS TLS;
TLS *WINAPI TlsAbrir(u32 s, const char *n) { (void)s; (void)n; return NULL; }
int WINAPI TlsConectar(TLS *t, int x) { (void)t; (void)x; return -1; }
int WINAPI TlsLeer(TLS *t, void *b, int n, int x) { (void)t; (void)b; (void)n; (void)x; return -1; }
int WINAPI TlsEscribir(TLS *t, const void *b, int n) { (void)t; (void)b; (void)n; return -1; }
void WINAPI TlsCerrar(TLS *t) { (void)t; }
int WINAPI TlsError(TLS *t) { (void)t; return 0; }
const char *WINAPI TlsTextoError(int e) { (void)e; return "(sin TLS en el anfitrión)"; }
int WINAPI TlsInfo(TLS *t, char *b, int n) { (void)t; (void)n; b[0] = 0; return 0; }

/* =============================================================================
 *  La página, como en navega.c (sin ventana)
 * ============================================================================= */
#define COLS 120
static Pagina *pag;
static int responde = 1, se_va;
static Nodo *enfocado;
static void sale(const char *t) {
  char b[4000];
  int n = a_utf8(t, strlen(t) > 1300 ? 1300 : strlen(t), b);
  b[n] = 0;
  printf("%s\n", b);
}
static void cb_remaqueta(void) { pag_maqueta(pag, COLS); }
static void cb_ir(const char *u, int post, char *c, u32 n) { char b[1200]; sprintf(b, "IR: %.1000s", u); sale(b); free(c); (void)post; (void)n; se_va = 1; }
static void cb_envia(Nodo *f, Nodo *bt) {
  char *url, *cuerpo, b[2400];
  u32 nc;
  int post, k = -1, j;
  for (j = 0; bt && j < pag->n_ctl; j++) if (pag->ctl[j].nodo == bt) k = j;
  if (!pag_formulario(pag, f, k, &url, &cuerpo, &nc, &post)) { sale("ENVIA: no se puede"); return; }
  sprintf(b, "ENVIA: %s %.1000s %.1000s", post ? "POST" : "GET", url, cuerpo ? cuerpo : "");
  sale(b);
  free(url); free(cuerpo);
  se_va = 1;
}
static void cb_pon_url(const char *u) { char b[1200]; sprintf(b, "URL: %.1000s", u); sale(b); }
static void cb_historia(int k) { printf("HISTORIA: %d\n", k); se_va = 1; }
static void cb_foco(Nodo *n) { if (n != enfocado) { enfocado = n; js_foco(n); } }
static int cb_cuadro(const char *t, int tipo) { char b[1400]; sprintf(b, "%s: %.1300s", tipo ? "CONFIRM" : "ALERT", t); sale(b); return tipo ? responde : 1; }
static int cb_pregunta(const char *t, const char *d, char *r, int max) { char b[1400]; sprintf(b, "PROMPT: %.1300s", t); sale(b); snprintf(r, max, "%s", d); return 1; }
static void cb_titulo(const char *t) { char b[400]; sprintf(b, "TITULO: %.300s", t); sale(b); }
static void cb_estado(const char *t) { char b[400]; sprintf(b, "ESTADO: %.300s", t); sale(b); }
static void cb_vista(Nodo *n) { (void)n; }
static const JsNav nav = { cb_remaqueta, cb_ir, cb_envia, cb_pon_url, cb_historia, cb_foco, cb_cuadro, cb_pregunta, cb_titulo, cb_estado, cb_vista };

static void vuelca(void) {
  int y, k;
  printf("---- página (%d filas) ----\n", pag->n_filas);
  for (y = 0; y < pag->n_filas; y++) {
    char f[400], u[1200];
    Fila *r = &pag->filas[y];
    int n = r->n < 380 ? r->n : 380;
    memcpy(f, r->c, n);
    /* los controles, con lo que tienen */
    for (k = 0; k < pag->n_ctl; k++) {
      Ctl *c = &pag->ctl[k];
      char v[300];
      int j, l;
      if (!c->visible || c->y != y || c->tipo == C_OCULTO) continue;
      if (c->tipo == C_CASILLA || c->tipo == C_RADIO) sprintf(v, "[%c]", c->marcado ? 'X' : ' ');
      else if (c->tipo == C_LISTA) { v[0] = '['; v[1] = 0; if (c->n_opc && c->elegida < c->n_opc) texto_de(c->opc[c->elegida], v + 1, 200); strcat(v, "]"); }
      else if (c->tipo == C_ENVIAR || c->tipo == C_BOTON || c->tipo == C_REINICIO) {
        char t[200] = "";
        if (c->nodo->etq == E_BUTTON) texto_de(c->nodo, t, 190); else snprintf(t, 190, "%s", c->valor);
        pag_rotulo_vacio(c, t, 190);
        sprintf(v, "<%s>", t);
      } else sprintf(v, "[%.*s]", c->n_valor < 200 ? c->n_valor : 200, c->valor);
      l = strlen(v);
      for (j = 0; j < l && j < c->w && c->x + j < 380; j++) { if (c->x + j >= n) { memset(f + n, ' ', c->x + j - n + 1); n = c->x + j + 1; } f[c->x + j] = v[j]; }
    }
    while (n && f[n - 1] == ' ') n--;
    u[a_utf8(f, n, u)] = 0;
    printf("%s\n", u);
  }
  printf("---- fin ----\n");
}
static void registro(void) {
  char l[2000];
  FILE *f = fopen("NAVEGA.LOG", "r");
  printf("---- registro ----\n");
  if (f) { while (fgets(l, sizeof l, f)) fputs(l, stdout); fclose(f); }
  printf("---- fin ----\n");
}
static void espera_red(void) {
  int k;
  for (k = 0; k < 20000 && js_ocupado(); k++) { js_avanza(); usleep(200); }
}
static void espera(int ms) {
  u32 fin = GetTickCount() + ms;
  do { espera_red(); js_tic(); usleep(5000); } while ((int)(GetTickCount() - fin) < 0);
  espera_red();
}
static Nodo *busca(const char *sel) {
  Nodo *r[1];
  char b[400];
  b[utf8_a_1252((const u8 *)sel, strlen(sel), b)] = 0;
  if (dom_busca(pag->doc.raiz, b, r, 1) != 1) { printf("NO ESTA: %s\n", sel); return NULL; }
  return r[0];
}
static Ctl *ctl_de(Nodo *n) { int k; for (k = 0; k < pag->n_ctl; k++) if (pag->ctl[k].nodo == n) return &pag->ctl[k]; return NULL; }
/* el envío de un formulario, como envia() de navega.c */
static void envia(Nodo *f, Nodo *boton) {
  if (js_evento(f, "submit", boton)) { printf("(submit impedido)\n"); return; }
  if (!se_va) cb_envia(f, boton);
}
static void clic(Nodo *n) {
  Ctl *c = ctl_de(n);
  const char *h;
  cb_foco(c ? n : enfocado);
  if (js_evento(n, "mousedown", NULL) || se_va) return;
  if (c && (c->tipo == C_CASILLA || c->tipo == C_RADIO)) {
    int antes = c->marcado;
    c->marcado = c->tipo == C_RADIO ? 1 : !antes;
    if (js_evento(n, "click", NULL)) { if ((c = ctl_de(n))) c->marcado = antes; printf("(clic impedido)\n"); return; }
    if ((c = ctl_de(n)) && c->marcado != antes) { js_evento(n, "input", NULL); js_evento(n, "change", NULL); }
    return;
  }
  if (js_evento(n, "click", NULL)) { printf("(clic impedido)\n"); return; }
  if (se_va) return;
  c = ctl_de(n);
  if (c && c->tipo == C_ENVIAR && c->forma) { envia(c->forma, n); return; }
  if (!c && n->etq == E_A && (h = atr(n, "href"))) {
    char *u;
    if (!_strnicmp(h, "javascript:", 11)) { js_enlace(h); return; }
    if (h[0] == '#') return;
    u = url_absoluta(pag->url, h);
    cb_ir(u ? u : h, 0, NULL, 0);
    free(u);
  }
  if (n->etq == E_SUMMARY && n->padre && n->padre->etq == E_DETAILS) { n->padre->abierto = !n->padre->abierto; pag_maqueta(pag, COLS); }
}
static int vk_de(const char *t) {
  if (!strcmp(t, "Abajo")) return 40;
  if (!strcmp(t, "Arriba")) return 38;
  if (!strcmp(t, "Intro")) return 13;
  if (!strcmp(t, "Esc")) return 27;
  if (!strcmp(t, "Tab")) return 9;
  if (!strcmp(t, "Retroceso")) return 8;
  return 0;
}
int main(int argc, char **argv) {
  Pet p;
  int r;
  FILE *o = NULL;
  char l[4000];
  if (argc < 2) { fprintf(stderr, "uso: anfitrion dirección [órdenes]\n"); return 2; }
  http_inicia();
  memset(&p, 0, sizeof p);
  p.url = malloc(strlen(argv[1]) + 1);
  strcpy(p.url, argv[1]);
  http_empieza(&p);
  while (!(r = http_avanza(&p))) usleep(500);
  if (r < 0) { printf("ERROR: %s\n", p.error); return 1; }
  pag = pag_nueva(p.url, p.datos, p.n_datos, 0, 0);
  http_suelta(&p);
  pag_maqueta(pag, COLS);
  js_inicia((HANDLE)1, &nav, "./");
  js_pagina(pag);
  espera(50);
  if (argc > 2 && !(o = fopen(argv[2], "r"))) { perror(argv[2]); return 2; }
  while (o && fgets(l, sizeof l, o)) {
    char *orden = l, *arg, *resto;
    Nodo *n;
    l[strcspn(l, "\r\n")] = 0;
    if (!l[0] || l[0] == '#') continue;
    printf(">> %s\n", l);
    if ((arg = strchr(l, ' '))) *arg++ = 0;
    resto = arg ? strstr(arg, " | ") : NULL;      /* «escribe SELECTOR | texto» */
    if (resto) { *resto = 0; resto += 3; }
    se_va = 0;
    if (!strcmp(orden, "vuelca")) vuelca();
    else if (!strcmp(orden, "registro")) registro();
    else if (!strcmp(orden, "espera")) espera(arg ? atoi(arg) : 100);
    else if (!strcmp(orden, "responde")) responde = arg && !strcmp(arg, "si");
    else if (!strcmp(orden, "js")) {
      char b[4200];
      if (resto) { resto[-3] = ' '; }
      snprintf(b, sizeof b, "javascript:%s", arg ? arg : "");
      js_enlace(b);
    } else if (!arg || !(n = busca(arg))) continue;
    else if (!strcmp(orden, "clic")) clic(n);
    else if (!strcmp(orden, "foco")) cb_foco(n);
    else if (!strcmp(orden, "escribe")) {
      char t[2000];
      int k, m;
      cb_foco(n);
      m = utf8_a_1252((const u8 *)(resto ? resto : ""), resto ? strlen(resto) : 0, t);
      for (k = 0; k < m; k++) {
        Ctl *c;
        if (js_tecla(n, 0, (u8)t[k])) continue;
        if ((c = ctl_de(n))) {
          char v[4000];
          int q = c->n_valor < 3990 ? c->n_valor : 3990;
          memcpy(v, c->valor, q); v[q] = t[k];
          ctl_pon(c, v, q + 1);
        }
        js_evento(n, "input", NULL);
      }
    } else if (!strcmp(orden, "tecla")) {
      int vk = vk_de(resto ? resto : "");
      Ctl *c;
      cb_foco(n);
      if (js_tecla(n, vk, 0)) { printf("(tecla impedida)\n"); }
      else if (vk == 13 && (c = ctl_de(n)) && c->forma && (c->tipo == C_TEXTO || c->tipo == C_CLAVE)) envia(c->forma, NULL);
    } else if (!strcmp(orden, "elige")) {
      Ctl *c = ctl_de(n);
      if (c && c->tipo == C_LISTA && resto) { c->elegida = atoi(resto); js_evento(n, "input", NULL); js_evento(n, "change", NULL); }
    } else printf("¿%s?\n", orden);
    espera(30);
    fflush(stdout);
  }
  js_fin();
  return 0;
}
/* (HSPJS.DLL la da; aquí, nada) */
void HjMemoria(u32 *uso, u32 *maximo, u32 *sistema) { *uso = *maximo = *sistema = 0; }
