/* http.c - el Navegador de Hispa-486: direcciones, galletas (cookies) y
 * HTTP/1.1 por WSOCK32.DLL, cifrado con HSPTLS.DLL si la dirección es https.
 * Una petición a la vez, sin bloquear la ventana (http_avanza se llama desde
 * el bucle de mensajes); la conexión se guarda abierta para la petición
 * siguiente al mismo servidor (el apretón de manos de TLS es lo caro).
 * Escrito desde las RFC 9110/9112 (HTTP), 6265 (cookies) y 3986 (URI).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "nav.h"

struct in_addr { u32 s_addr; };
struct sockaddr_in { short sin_family; u16 sin_port; struct in_addr sin_addr; char sin_zero[8]; };
struct hostent { char *h_name; char **h_aliases; short h_addrtype, h_length; char **h_addr_list; };
typedef struct { u32 fd_count; u32 fd_array[64]; } fd_set;
struct timeval { int tv_sec, tv_usec; };
int WINAPI WSAStartup(u32, void *);
int WINAPI WSACleanup(void);
int WINAPI WSAGetLastError(void);
u32 WINAPI socket(int, int, int);
int WINAPI connect(u32, const struct sockaddr_in *, int);
int WINAPI send(u32, const char *, int, int);
int WINAPI recv(u32, char *, int, int);
int WINAPI closesocket(u32);
int WINAPI ioctlsocket(u32, u32, u32 *);
int WINAPI select(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
struct hostent *WINAPI gethostbyname(const char *);
u16 WINAPI htons(u16);
TLS *WINAPI TlsAbrir(u32, const char *);
int WINAPI TlsConectar(TLS *, int);
int WINAPI TlsLeer(TLS *, void *, int, int);
int WINAPI TlsEscribir(TLS *, const void *, int);
void WINAPI TlsCerrar(TLS *);
int WINAPI TlsError(TLS *);
const char *WINAPI TlsTextoError(int);
int WINAPI TlsInfo(TLS *, char *, int);
u32 WINAPI GetTickCount(void);
typedef struct { u16 anio, mes, sem, dia, hora, min, seg, ms; } ST;
void WINAPI GetLocalTime(ST *);

char http_idioma[32] = "es";
#define CUERPO_MAX (4u << 20)

/* =============================================================================
 *  Direcciones
 * ============================================================================= */
int url_parte(const char *u, Url *d) {
  const char *p = u, *f;
  u32 n;
  d->ruta = NULL;
  if (!_strnicmp(p, "https://", 8)) { d->seguro = 1; p += 8; }
  else if (!_strnicmp(p, "http://", 7)) { d->seguro = 0; p += 7; }
  else return 0;
  for (f = p; *f && *f != '/' && *f != ':' && *f != '?' && *f != '#'; f++) ;
  n = f - p;
  if (!n || n > 127) return 0;
  memcpy(d->servidor, p, n);
  d->servidor[n] = 0;
  for (n = 0; d->servidor[n]; n++) d->servidor[n] = (char)min_c(d->servidor[n]);
  d->puerto = d->seguro ? 443 : 80;
  if (*f == ':') {
    d->puerto = atoi(f + 1);
    while (*f && *f != '/' && *f != '?' && *f != '#') f++;
    if (d->puerto < 1 || d->puerto > 65535) return 0;
  }
  for (p = f; *p && *p != '#'; p++) ;
  n = p - f;
  d->ruta = malloc(n + 2);
  if (!d->ruta) return 0;
  if (*f != '/') { d->ruta[0] = '/'; memcpy(d->ruta + 1, f, n); d->ruta[n + 1] = 0; }
  else { memcpy(d->ruta, f, n); d->ruta[n] = 0; }
  return 1;
}
/* quita los «.» y «..» de la ruta (hasta la consulta) */
static void aplana(char *ruta) {
  char *e = ruta, *s = ruta, *fin = ruta;
  while (*fin && *fin != '?' && *fin != '#') fin++;
  while (e < fin) {
    if (e[0] == '/' && e[1] == '.' && (e + 2 == fin || e[2] == '/')) { e += 2; if (e == fin) *s++ = '/'; continue; }
    if (e[0] == '/' && e[1] == '.' && e[2] == '.' && (e + 3 == fin || e[3] == '/')) {
      e += 3;
      while (s > ruta && *--s != '/') ;
      if (e == fin) *s++ = '/';
      continue;
    }
    *s++ = *e++;
  }
  while (*e) *s++ = *e++;
  *s = 0;
}
char *url_absoluta(const char *base, const char *rel) {
  u32 nb = strlen(base), nr, k;
  char *d, *p;
  const char *rz, *q;
  while (*rel == ' ' || *rel == '\t' || *rel == '\n' || *rel == '\r') rel++;
  nr = strlen(rel);
  while (nr && (u8)rel[nr - 1] <= ' ') nr--;
  d = malloc(nb + nr * 3 + 8);
  if (!d) return NULL;
  /* ¿trae esquema? */
  for (k = 0; k < nr && (((rel[k] | 32) >= 'a' && (rel[k] | 32) <= 'z') || (k && (rel[k] == '+' || rel[k] == '-' || rel[k] == '.' || (rel[k] >= '0' && rel[k] <= '9')))); k++) ;
  if (k && k < nr && rel[k] == ':') {
    if (_strnicmp(rel, "http:", 5) && _strnicmp(rel, "https:", 6) && _strnicmp(rel, "about:", 6)) { free(d); return NULL; }
    d[0] = 0;
    p = d;
  } else {
    /* la raíz de la base: «esquema://servidor» */
    rz = strstr(base, "://");
    if (!rz) { free(d); return NULL; }
    for (rz += 3; *rz && *rz != '/' && *rz != '?' && *rz != '#'; rz++) ;
    if (nr >= 2 && rel[0] == '/' && rel[1] == '/') {
      k = strstr(base, "://") + 1 - base;
      memcpy(d, base, k); p = d + k;
    } else if (nr && rel[0] == '/') {
      memcpy(d, base, rz - base); p = d + (rz - base);
    } else {
      for (q = rz; *q && *q != '?' && *q != '#'; q++) ;
      if (!nr || rel[0] == '#') { for (q = rz; *q && *q != '#'; q++) ; }
      else if (rel[0] != '?') { while (q > rz && q[-1] != '/') q--; }
      memcpy(d, base, q - base); p = d + (q - base);
      if (q == rz && nr && rel[0] != '?' && rel[0] != '#') *p++ = '/';
    }
  }
  for (k = 0; k < nr; k++) {
    u8 c = (u8)rel[k];
    if (c == ' ') { *p++ = '%'; *p++ = '2'; *p++ = '0'; }
    else if (c < ' ') continue;
    else if (c >= 0x80) {               /* (las letras de fuera del ASCII, en UTF-8 y con %) */
      char u8b[4];
      int n = a_utf8(rel + k, 1, u8b), j;
      for (j = 0; j < n; j++) { sprintf(p, "%%%02X", (u8)u8b[j]); p += 3; }
    } else *p++ = (char)c;
  }
  *p = 0;
  if (!_strnicmp(d, "http", 4)) {
    rz = strstr(d, "://");
    if (rz) {
      for (rz += 3; *rz && *rz != '/' && *rz != '?' && *rz != '#'; rz++) ;
      if (*rz == '/') aplana((char *)rz);
    }
  }
  return d;
}
void url_codifica(const char *t, int n, char **d, u32 *nd, u32 *cap) {
  static const char hex[] = "0123456789ABCDEF";
  char u8b[4];
  int k, crudo = n == -2;
  if (crudo) n = strlen(t);
  if (*nd + (u32)n * 9 + 16 > *cap) {
    u32 c = *cap + n * 9 + 256;
    char *q = realloc(*d, c);
    if (!q) return;
    *d = q; *cap = c;
  }
  for (k = 0; k < n; k++) {
    u8 c = (u8)t[k];
    int m, j;
    if (crudo || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '*') {
      (*d)[(*nd)++] = (char)c;
      continue;
    }
    if (c == ' ') { (*d)[(*nd)++] = '+'; continue; }
    if (c == '\n' && (k == 0 || t[k - 1] != '\r')) { memcpy(*d + *nd, "%0D%0A", 6); *nd += 6; continue; }
    m = a_utf8(t + k, 1, u8b);
    for (j = 0; j < m; j++) {
      (*d)[(*nd)++] = '%';
      (*d)[(*nd)++] = hex[(u8)u8b[j] >> 4];
      (*d)[(*nd)++] = hex[u8b[j] & 15];
    }
  }
  (*d)[*nd] = 0;
}

/* =============================================================================
 *  Galletas (cookies): solo en memoria, hasta que se cierra el Navegador
 * ============================================================================= */
#define N_GAL 48
typedef struct { char *dom, *ruta, *nombre, *valor; u8 seguro, exacto; u32 orden; } Galleta;
static Galleta gal[N_GAL];
static u32 gal_orden;
static char *copia(const char *t, int n) {
  char *p = malloc(n + 1);
  if (p) { memcpy(p, t, n); p[n] = 0; }
  return p;
}
static int memchr_punto(const char *t, u32 n) {
  u32 k;
  for (k = 0; k < n; k++) if (t[k] == '.') return 1;
  return 0;
}
static void gal_quita(Galleta *g) {
  free(g->dom); free(g->ruta); free(g->nombre); free(g->valor);
  memset(g, 0, sizeof *g);
}
/* «Set-Cookie: nombre=valor; Path=/; Secure...» de una respuesta de u */
static void gal_pon(const Url *u, const char *l) {
  const char *f, *ig, *p;
  char dom[128], ruta[256], atrib[24];
  int borra = 0, seguro = 0, exacto = 1, k, n, libre = -1, viejo = 0;
  ST st;
  for (f = l; *f && *f != '\r' && *f != '\n' && *f != ';'; f++) ;
  for (ig = l; ig < f && *ig != '='; ig++) ;
  if (ig == f || ig == l) return;
  strcpy(dom, u->servidor);
  /* la ruta por omisión: la carpeta de lo pedido */
  for (n = 0; u->ruta[n] && u->ruta[n] != '?' && n < 250; n++) ruta[n] = u->ruta[n];
  while (n > 1 && ruta[n - 1] != '/') n--;
  if (n > 1) n--;
  ruta[n ? n : 1] = 0;
  if (!n) ruta[0] = '/';
  for (p = f; *p == ';'; ) {
    const char *v, *fa;
    p++;
    while (*p == ' ') p++;
    for (fa = p; *fa && *fa != '\r' && *fa != '\n' && *fa != ';'; fa++) ;
    for (v = p, k = 0; v < fa && *v != '=' && k < 23; v++) atrib[k++] = (char)min_c(*v);
    atrib[k] = 0;
    if (v < fa && *v == '=') v++;
    if (!strcmp(atrib, "secure")) seguro = 1;
    else if (!strcmp(atrib, "path") && *v == '/' && fa - v < 250) { memcpy(ruta, v, fa - v); ruta[fa - v] = 0; }
    else if (!strcmp(atrib, "domain") && fa > v) {
      u32 ld, ls = strlen(u->servidor);
      if (*v == '.') v++;
      ld = fa - v;
      /* solo si es el servidor o uno de sus dominios de arriba (con algún punto) */
      if (ld < 127 && ld <= ls && !_strnicmp(u->servidor + ls - ld, v, ld) && (ld == ls || u->servidor[ls - ld - 1] == '.')
          && memchr_punto(v, ld)) {
        memcpy(dom, v, ld); dom[ld] = 0;
        for (k = 0; dom[k]; k++) dom[k] = (char)min_c(dom[k]);
        exacto = ld == ls;
      } else return;
    } else if (!strcmp(atrib, "max-age")) { if (*v == '-' || *v == '0') borra = 1; }
    else if (!strcmp(atrib, "expires")) {
      /* caducada si el año es anterior al de hoy */
      const char *y;
      GetLocalTime(&st);
      for (y = v; y + 3 < fa; y++)
        if (y[0] >= '1' && y[0] <= '9' && y[1] >= '0' && y[1] <= '9' && y[2] >= '0' && y[2] <= '9' && y[3] >= '0' && y[3] <= '9'
            && (y == v || y[-1] < '0' || y[-1] > '9')) { if (atoi(y) < st.anio) borra = 1; break; }
    }
    p = fa;
  }
  for (k = 0; k < N_GAL; k++) {
    Galleta *g = &gal[k];
    if (!g->nombre) { if (libre < 0) libre = k; continue; }
    if ((int)strlen(g->nombre) == ig - l && !memcmp(g->nombre, l, ig - l) && !strcmp(g->dom, dom) && !strcmp(g->ruta, ruta)) {
      gal_quita(g);
      if (libre < 0) libre = k;
    } else if (gal[k].orden < gal[viejo].orden) viejo = k;
  }
  if (borra) return;
  if (libre < 0) { gal_quita(&gal[viejo]); libre = viejo; }
  gal[libre].nombre = copia(l, ig - l);
  gal[libre].valor = copia(ig + 1, f - ig - 1);
  gal[libre].dom = copia(dom, strlen(dom));
  gal[libre].ruta = copia(ruta, strlen(ruta));
  gal[libre].seguro = (u8)seguro; gal[libre].exacto = (u8)exacto;
  gal[libre].orden = ++gal_orden;
  if (!gal[libre].nombre || !gal[libre].valor || !gal[libre].dom || !gal[libre].ruta) gal_quita(&gal[libre]);
}
/* la cabecera «Cookie: a=b; c=d\r\n» para u, en d (malloc) o NULL */
static char *gal_para(const Url *u) {
  char *d = NULL;
  u32 n = 0, k, ls = strlen(u->servidor);
  for (k = 0; k < N_GAL; k++) {
    Galleta *g = &gal[k];
    u32 ld, lr, ln, lv;
    char *q;
    if (!g->nombre) continue;
    if (g->seguro && !u->seguro) continue;
    ld = strlen(g->dom); lr = strlen(g->ruta);
    if (g->exacto ? strcmp(g->dom, u->servidor) != 0
                  : !(ld <= ls && !strcmp(u->servidor + ls - ld, g->dom) && (ld == ls || u->servidor[ls - ld - 1] == '.'))) continue;
    if (strncmp(u->ruta, g->ruta, lr) || !(g->ruta[lr - 1] == '/' || u->ruta[lr] == 0 || u->ruta[lr] == '/' || u->ruta[lr] == '?')) continue;
    ln = strlen(g->nombre); lv = strlen(g->valor);
    q = realloc(d, n + ln + lv + 16);
    if (!q) break;
    d = q;
    if (!n) { memcpy(d, "Cookie: ", 8); n = 8; } else { d[n++] = ';'; d[n++] = ' '; }
    memcpy(d + n, g->nombre, ln); n += ln;
    d[n++] = '=';
    memcpy(d + n, g->valor, lv); n += lv;
  }
  if (d) { d[n++] = '\r'; d[n++] = '\n'; d[n] = 0; }
  return d;
}

/* =============================================================================
 *  La conexión (una, que se guarda) y la petición
 * ============================================================================= */
static struct { int usada, puerto, seguro; char servidor[128]; u32 s; TLS *t; u32 t_ult; } con;
static char cifrado[80];
static int ws_lista;
void http_inicia(void) {
  static char wsa[400];
  if (!ws_lista) { WSAStartup(0x0101, wsa); ws_lista = 1; }
}
static void con_cierra(void) {
  if (!con.usada) return;
  if (con.t) TlsCerrar(con.t);
  closesocket(con.s);
  con.usada = 0; con.t = NULL;
  cifrado[0] = 0;
}
void http_fin(void) {
  con_cierra();
  if (ws_lista) { WSACleanup(); ws_lista = 0; }
}
const char *http_cifrado(void) { return con.usada && con.t ? cifrado : ""; }
static int falla(Pet *p, const char *t) {
  int k;
  for (k = 0; t[k] && k < 198; k++) p->error[k] = t[k];
  p->error[k] = 0;
  p->fase = H_ERROR;
  con_cierra();
  return -1;
}
const char *http_cabecera(const Pet *p, const char *nombre) {
  u32 n = strlen(nombre);
  const char *l = p->cab;
  if (!l) return NULL;
  while ((l = strchr(l, '\n'))) {
    l++;
    if (!_strnicmp(l, nombre, n) && l[n] == ':') {
      l += n + 1;
      while (*l == ' ' || *l == '\t') l++;
      return l;
    }
  }
  return NULL;
}
void http_empieza(Pet *p) {
  char *u = p->url, *c = p->cuerpo;
  int post = p->post;
  u32 nc = p->n_cuerpo;
  memset(p, 0, sizeof *p);
  p->url = u; p->cuerpo = c; p->post = post; p->n_cuerpo = nc;
  p->fase = H_NADA;
  p->largo = -1;
}
void http_suelta(Pet *p) {
  if (p->fase != H_NADA && p->fase != H_HECHO && p->fase != H_ERROR) con_cierra();   /* a medias */
  free(p->url); free(p->cuerpo); free(p->pet); free(p->cab); free(p->datos); free(p->u.ruta);
  memset(p, 0, sizeof *p);
}
static int pet_arma(Pet *p) {
  char *g = gal_para(&p->u), host[150];
  u32 n = strlen(p->u.ruta) + (g ? strlen(g) : 0) + p->n_cuerpo + 600 + (p->js_cab ? 2010 : 0);
  free(p->pet);
  p->pet = malloc(n);
  if (!p->pet) { free(g); return 0; }
  if (p->u.puerto != (p->u.seguro ? 443 : 80)) sprintf(host, "%s:%d", p->u.servidor, p->u.puerto);
  else strcpy(host, p->u.servidor);
  n = sprintf(p->pet, "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Navegador/0.1 (Hispa-486; Hispa32)\r\n"
              "Accept: text/html,text/plain;q=0.9,*/*;q=0.5\r\nAccept-Language: %s\r\nConnection: keep-alive\r\n%s",
              p->js_metodo ? p->js_metodo : p->post ? "POST" : "GET", p->u.ruta, host, http_idioma, g ? g : "");
  free(g);
  if (p->js_cab) n += sprintf(p->pet + n, "%.2000s", p->js_cab);     /* [JS] las de fetch */
  if (p->post && (!p->js_cab || !strstr(p->js_cab, "Content-Type:")))
    n += sprintf(p->pet + n, "Content-Type: application/x-www-form-urlencoded\r\n");
  if (p->post) n += sprintf(p->pet + n, "Content-Length: %u\r\n", p->n_cuerpo);
  memcpy(p->pet + n, "\r\n", 2); n += 2;
  if (p->post && p->n_cuerpo) { memcpy(p->pet + n, p->cuerpo, p->n_cuerpo); n += p->n_cuerpo; }
  p->n_pet = n;
  p->enviado = 0;
  return 1;
}
static int datos_pon(Pet *p, const u8 *b, u32 n) {
  if (p->n_datos + n + 1 > p->cap_datos) {
    u32 c = p->cap_datos ? p->cap_datos * 2 : 16384;
    u8 *q;
    while (c < p->n_datos + n + 1) c *= 2;
    q = realloc(p->datos, c);
    if (!q) return 0;
    p->datos = q; p->cap_datos = c;
  }
  memcpy(p->datos + p->n_datos, b, n);
  p->n_datos += n;
  p->datos[p->n_datos] = 0;
  return p->n_datos <= CUERPO_MAX;
}
/* octetos del cuerpo; 1 = ya está entero, -1 = no cabe */
static int cuerpo_pon(Pet *p, const u8 *b, u32 n) {
  if (!p->troceado) {
    if (p->largo >= 0 && p->n_datos + n > (u32)p->largo) n = p->largo - p->n_datos;
    if (!datos_pon(p, b, n)) return -1;
    return p->largo >= 0 && p->n_datos >= (u32)p->largo;
  }
  /* a trozos: «tamaño en hexadecimal CRLF datos CRLF ... 0 CRLF CRLF» */
  while (n) {
    if (p->trozo_f == 0) {                     /* la línea del tamaño */
      int c = *b++;
      n--;
      if (c == '\n') { p->trozo_f = p->trozo_q ? 1 : 3; continue; }
      if (c == ';') p->trozo_f = 4;
      else if (c != '\r' && c != ' ') {
        int h = c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1;
        if (h < 0 || p->trozo_q > CUERPO_MAX) return -1;
        p->trozo_q = p->trozo_q * 16 + h;
      }
    } else if (p->trozo_f == 4) {              /* (extensiones del trozo: hasta el fin de línea) */
      if (*b++ == '\n') p->trozo_f = p->trozo_q ? 1 : 3;
      n--;
    } else if (p->trozo_f == 1) {
      u32 q = n < p->trozo_q ? n : p->trozo_q;
      if (!datos_pon(p, b, q)) return -1;
      b += q; n -= q; p->trozo_q -= q;
      if (!p->trozo_q) p->trozo_f = 2;
    } else if (p->trozo_f == 2) {              /* el CRLF de detrás de los datos */
      if (*b++ == '\n') p->trozo_f = 0;
      n--;
    } else {                                   /* tras el trozo 0: líneas hasta una vacía */
      int c = *b++;
      n--;
      if (c == '\n') { if (p->trozo_q == 0) return 1; p->trozo_q = 0; }
      else if (c != '\r') p->trozo_q = 1;
    }
  }
  return 0;
}
/* las cabeceras ya están enteras en p->cab */
static int cabeceras(Pet *p) {
  const char *t, *l;
  int v11;
  if (memcmp(p->cab + 1, "HTTP/", 5) || !(t = strchr(p->cab + 1, ' '))) return -1;
  v11 = p->cab[6] == '1' && p->cab[8] >= '1';
  p->estado = atoi(t + 1);
  for (l = p->cab; (l = strchr(l, '\n')); ) {
    l++;
    if (!_strnicmp(l, "Set-Cookie:", 11)) { l += 11; while (*l == ' ') l++; gal_pon(&p->u, l); }
  }
  p->cerrar = !v11;
  if ((t = http_cabecera(p, "Connection"))) { if (!_strnicmp(t, "close", 5)) p->cerrar = 1; else if (!_strnicmp(t, "keep-alive", 10)) p->cerrar = 0; }
  if ((t = http_cabecera(p, "Transfer-Encoding")) && !_strnicmp(t, "chunked", 7)) p->troceado = 1;
  else if ((t = http_cabecera(p, "Content-Length"))) p->largo = atoi(t);
  else { p->hasta_cierre = 1; p->cerrar = 1; }
  p->tipo[0] = 0;
  if ((t = http_cabecera(p, "Content-Type"))) {
    int k;
    for (k = 0; t[k] && t[k] != '\r' && t[k] != '\n' && k < 78; k++) p->tipo[k] = (char)min_c(t[k]);
    p->tipo[k] = 0;
  }
  return 0;
}
static int hecha(Pet *p) {
  p->fase = H_HECHO;
  if (p->cerrar) con_cierra(); else con.t_ult = GetTickCount();
  return 1;
}

int http_avanza(Pet *p) {
  static u8 b[4096];
  int n, vueltas, r;
  switch (p->fase) {
  case H_NADA:
    http_inicia();
    if (!url_parte(p->url, &p->u)) return falla(p, "La direcci\363n no vale.");
    if (con.usada && con.puerto == p->u.puerto && con.seguro == p->u.seguro && !strcmp(con.servidor, p->u.servidor)
        && GetTickCount() - con.t_ult < 50000u) {
      p->reusada = 1;
      if (!pet_arma(p)) return falla(p, "No hay memoria.");
      p->fase = H_ENVIA;
      return 0;
    }
    con_cierra();
    p->fase = H_DNS;
    return 0;
  case H_DNS: {
    struct hostent *he = gethostbyname(p->u.servidor);
    struct sockaddr_in a;
    u32 uno = 1;
    if (!he) {
      if (WSAGetLastError() == 10050)
        return falla(p, "No hay red. Hay que cargar HSPRED (el controlador de la tarjeta de red) antes de entrar en Hispa Ventanas.");
      sprintf((char *)b, "No se encuentra el servidor %.100s.", p->u.servidor);
      return falla(p, (char *)b);
    }
    memset(&a, 0, sizeof a);
    a.sin_family = 2;
    memcpy(&a.sin_addr, he->h_addr_list[0], 4);
    a.sin_port = htons((u16)p->u.puerto);
    con.s = socket(2, 1, 0);
    if (con.s == ~0u) return falla(p, "No se puede abrir la conexi\363n.");
    con.usada = 1; con.t = NULL; con.puerto = p->u.puerto; con.seguro = p->u.seguro;
    strcpy(con.servidor, p->u.servidor);
    ioctlsocket(con.s, 0x8004667Eu, &uno);
    connect(con.s, &a, sizeof a);
    p->t0 = GetTickCount();
    p->fase = H_CONECTA;
    return 0;
  }
  case H_CONECTA: {
    fd_set e, x;
    struct timeval cero = { 0, 0 };
    e.fd_count = 1; e.fd_array[0] = con.s; x = e;
    select(0, NULL, &e, &x, &cero);
    if (x.fd_count) return falla(p, "El servidor no contesta (conexi\363n rechazada).");
    if (!e.fd_count) {
      if (GetTickCount() - p->t0 > 20000u) return falla(p, "El servidor no contesta.");
      return 0;
    }
    if (p->u.seguro) {
      con.t = TlsAbrir(con.s, p->u.servidor);
      if (!con.t) return falla(p, "No hay memoria para la conexi\363n segura.");
      p->t0 = GetTickCount();
      p->fase = H_TLS;
      return 0;
    }
    if (!pet_arma(p)) return falla(p, "No hay memoria.");
    p->fase = H_ENVIA;
    return 0;
  }
  case H_TLS:
    r = TlsConectar(con.t, 0);
    if (r == -1) {
      sprintf((char *)b, "No se ha podido hacer la conexi\363n segura con %.60s. %s", p->u.servidor, TlsTextoError(TlsError(con.t)));
      return falla(p, (char *)b);
    }
    if (r != 1) {
      if (GetTickCount() - p->t0 > 90000u) return falla(p, "El servidor no contesta (conexi\363n segura).");
      return 0;
    }
    TlsInfo(con.t, cifrado, sizeof cifrado);
    if (!pet_arma(p)) return falla(p, "No hay memoria.");
    p->fase = H_ENVIA;
    return 0;
  case H_ENVIA:
    if (con.t) {
      if (TlsEscribir(con.t, p->pet, p->n_pet) != (int)p->n_pet) goto cortada;
      p->enviado = p->n_pet;
    } else {
      n = send(con.s, p->pet + p->enviado, p->n_pet - p->enviado, 0);
      if (n < 0 && WSAGetLastError() != 10035) goto cortada;
      if (n > 0) p->enviado += n;
    }
    if ((u32)p->enviado < p->n_pet) return 0;
    p->t0 = GetTickCount();
    p->fase = H_CABECERAS;
    return 0;
  case H_CABECERAS: case H_CUERPO:
    for (vueltas = 0; vueltas < 8; vueltas++) {
      const u8 *d = b;
      n = con.t ? TlsLeer(con.t, b, sizeof b, 0) : recv(con.s, (char *)b, sizeof b, 0);
      if (con.t ? n == -2 : (n < 0 && WSAGetLastError() == 10035)) {
        if (GetTickCount() - p->t0 > 60000u) return falla(p, "El servidor ha dejado de contestar.");
        return 0;
      }
      if (n <= 0) {
        if (p->fase == H_CUERPO && p->hasta_cierre) return hecha(p);
        goto cortada;
      }
      p->t0 = GetTickCount();
      if (p->fase == H_CABECERAS) {
        while (n) {
          if (p->n_cab + 2 > p->cap_cab) {
            char *q = realloc(p->cab, p->cap_cab + 2048);
            if (!q) return falla(p, "No hay memoria.");
            p->cab = q; p->cap_cab += 2048;
            if (!p->n_cab) p->cab[p->n_cab++] = '\n';
          }
          p->cab[p->n_cab++] = (char)*d++;
          p->cab[p->n_cab] = 0;
          n--;
          if (p->n_cab >= 5 && !memcmp(p->cab + p->n_cab - 4, "\r\n\r\n", 4)) break;
          if (p->n_cab >= 3 && !memcmp(p->cab + p->n_cab - 2, "\n\n", 2)) break;
          if (p->n_cab > 32000) return falla(p, "La respuesta del servidor no se entiende.");
        }
        if (!(p->n_cab >= 5 && !memcmp(p->cab + p->n_cab - 4, "\r\n\r\n", 4)) && !(p->n_cab >= 3 && !memcmp(p->cab + p->n_cab - 2, "\n\n", 2)))
          continue;
        if (cabeceras(p)) return falla(p, "La respuesta del servidor no se entiende.");
        if (p->estado >= 100 && p->estado < 200) {      /* de trámite: vienen otras */
          p->n_cab = 1; p->largo = -1; p->troceado = 0; p->hasta_cierre = 0;
          if (n) { memmove(b, d, n); d = b; }
          /* (lo que queda en b se mira en la vuelta siguiente: es raro) */
          continue;
        }
        p->fase = H_CUERPO;
        if (p->estado == 204 || p->estado == 304 || (p->largo == 0 && !p->troceado)) { p->hasta_cierre = 0; return hecha(p); }
        if (!n) continue;
      }
      r = cuerpo_pon(p, d, n);
      if (r < 0) return falla(p, "La p\341gina es demasiado grande o llega mal.");
      if (r > 0) return hecha(p);
    }
    return 0;
  case H_HECHO:
    return 1;
  default:
    return -1;
  }
cortada:
  /* la conexión guardada la había cerrado el servidor: otra vez, con una nueva */
  if (p->reusada && !p->reintento && p->n_cab <= 1) {
    con_cierra();
    p->reusada = 0; p->reintento = 1;
    p->fase = H_DNS;
    return 0;
  }
  return falla(p, "Se ha cortado la conexi\363n con el servidor.");
}

/* [JS] document.cookie: las galletas de url («a=b; c=d», malloc) y poner una */
char *http_galletas(const char *url) {
  Url u;
  char *g, *r;
  if (!url_parte(url, &u)) return NULL;
  g = gal_para(&u);
  free(u.ruta);
  if (!g) return NULL;
  r = g;
  memmove(r, g + 8, strlen(g + 8) + 1);         /* sin «Cookie: » */
  if (strlen(r) >= 2) r[strlen(r) - 2] = 0;      /* ni el \r\n */
  return r;
}
void http_pon_galleta(const char *url, const char *linea) {
  Url u;
  if (!url_parte(url, &u)) return;
  gal_pon(&u, linea);
  free(u.ruta);
}
