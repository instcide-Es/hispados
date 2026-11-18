/* =============================================================================
 *  HTTPS.EXE de Hispa-486: descarga por HTTP o por HTTPS desde la línea de
 *  órdenes (programa Hispa32 de consola; lo usa DESCARGAR para las
 *  direcciones https://). Con WSOCK32.DLL y HSPTLS.DLL (docs/RED.md).
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *     HTTPS [/V] [/N] dirección [archivo]
 *
 *  Sin archivo, lo guarda con el último trozo de la ruta en 8.3 (INDEX.HTM si
 *  no hay). /N: no guarda nada (dice los octetos y su suma). /V: enseña las
 *  cabeceras, el cifrado y lo que tarda el apretón de manos de TLS. Sigue
 *  hasta 5 redirecciones. Sale con 0 si todo va bien.
 *  (Los textos, en la página 437 de la consola.)
 * ============================================================================= */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
#define WINAPI __attribute__((stdcall))
#define NULL ((void *)0)
struct in_addr { u32 s_addr; };
struct sockaddr_in { short sin_family; u16 sin_port; struct in_addr sin_addr; char sin_zero[8]; };
struct hostent { char *h_name; char **h_aliases; short h_addrtype, h_length; char **h_addr_list; };
int WINAPI WSAStartup(u32, void *);
int WINAPI WSACleanup(void);
int WINAPI WSAGetLastError(void);
u32 WINAPI socket(int, int, int);
int WINAPI connect(u32, const struct sockaddr_in *, int);
int WINAPI send(u32, const char *, int, int);
int WINAPI recv(u32, char *, int, int);
int WINAPI closesocket(u32);
struct hostent *WINAPI gethostbyname(const char *);
char *WINAPI inet_ntoa(struct in_addr);
u16 WINAPI htons(u16);
typedef struct TLS TLS;
TLS *WINAPI TlsAbrir(u32, const char *);
int WINAPI TlsConectar(TLS *, int);
int WINAPI TlsLeer(TLS *, void *, int, int);
int WINAPI TlsEscribir(TLS *, const void *, int);
void WINAPI TlsCerrar(TLS *);
int WINAPI TlsError(TLS *);
const char *WINAPI TlsTextoError(int);
int WINAPI TlsInfo(TLS *, char *, int);
u32 WINAPI GetTickCount(void);
typedef struct FILE FILE;
int printf(const char *, ...);
int sprintf(char *, const char *, ...);
FILE *fopen(const char *, const char *);
u32 fwrite(const void *, u32, u32, FILE *);
int fclose(FILE *);
int remove(const char *);
int atoi(const char *);
u32 strlen(const char *);
char *strcpy(char *, const char *);
char *strchr(const char *, int);
char *strrchr(const char *, int);
int _strnicmp(const char *, const char *, u32);
void *memcpy(void *, const void *, u32);
void *memmove(void *, const void *, u32);
int memcmp(const void *, const void *, u32);

static char dir[1024], servidor[256], ruta[1024], buf[4096], cab[4096], pet[1400];
static int verboso, sin_guardar;

/* la página 1252 (los textos de HSPTLS) a la 437 de la consola */
static void di1252(const char *t) {
  static const char de[] = "\341\351\355\363\372\361", a[] = "\240\202\241\242\243\244";
  for (; *t; t++) {
    const char *p = strchr(de, *t);
    printf("%c", p && *t ? a[p - de] : *t);
  }
  printf("\n");
}
/* parte «http[s]://servidor[:puerto]/ruta»; devuelve 0 si no vale */
static int parte(const char *d, int *seguro, int *puerto) {
  const char *p = d, *f;
  u32 n;
  *seguro = 0;
  if (!_strnicmp(p, "https://", 8)) { *seguro = 1; p += 8; }
  else if (!_strnicmp(p, "http://", 7)) p += 7;
  else if (strchr(p, ':') && strchr(p, ':')[1] == '/') return 0;
  for (f = p; *f && *f != '/' && *f != ':' && *f != '?'; f++) ;
  n = f - p;
  if (!n || n > 255) return 0;
  memcpy(servidor, p, n); servidor[n] = 0;
  *puerto = *seguro ? 443 : 80;
  if (*f == ':') { *puerto = atoi(f + 1); while (*f && *f != '/' && *f != '?') f++; }
  if (*puerto < 1 || *puerto > 65535) return 0;
  if (*f == '/') strcpy(ruta, f);
  else { ruta[0] = '/'; strcpy(ruta + 1, f); }
  n = strlen(ruta);
  if (strchr(ruta, '#')) *strchr(ruta, '#') = 0;
  return 1;
}
/* el nombre 8.3 de lo que se baja, de la ruta */
static void nombre83(char *n) {
  char *q = strchr(ruta, '?'), *p, *pt;
  char t[1024];
  int k = 0, e;
  strcpy(t, ruta);
  if (q) t[q - ruta] = 0;
  p = strrchr(t, '/');
  p = p ? p + 1 : t;
  if (!*p) { strcpy(n, "INDEX.HTM"); return; }
  pt = strrchr(p, '.');
  for (; *p && p != pt && k < 8; p++) {
    int c = (u8)*p;
    if (c >= 'a' && c <= 'z') c -= 32;
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') n[k++] = (char)c;
  }
  if (!k) n[k++] = 'X';
  if (pt && pt[1]) {
    n[k++] = '.';
    for (e = 0, p = pt + 1; *p && e < 3; p++) {
      int c = (u8)*p;
      if (c >= 'a' && c <= 'z') c -= 32;
      if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) { n[k++] = (char)c; e++; }
    }
  }
  n[k] = 0;
}
static const char *cabecera(const char *nombre) {
  u32 n = strlen(nombre);
  const char *l = cab;
  while ((l = strchr(l, '\n'))) {
    l++;
    if (!_strnicmp(l, nombre, n) && l[n] == ':') {
      l += n + 1;
      while (*l == ' ') l++;
      return l;
    }
  }
  return NULL;
}

int main(int argc, char **argv) {
  static char wsa[400];
  const char *archivo = NULL, *d = NULL;
  char nom[16];
  int k, vuelta, seguro, puerto;
  for (k = 1; k < argc; k++) {
    if (argv[k][0] == '/' && (argv[k][1] == 'V' || argv[k][1] == 'v') && !argv[k][2]) verboso = 1;
    else if (argv[k][0] == '/' && (argv[k][1] == 'N' || argv[k][1] == 'n') && !argv[k][2]) sin_guardar = 1;
    else if (!d) d = argv[k];
    else if (!archivo) archivo = argv[k];
  }
  if (!d || strlen(d) > 1000) {
    printf("Descarga un archivo por HTTP o HTTPS.\n\nHTTPS [/V] [/N] direcci\242n [archivo]\n\n"
           "  /V  ense\244a las cabeceras y el cifrado\n  /N  no guarda nada: dice los octetos y su suma\n");
    return 1;
  }
  strcpy(dir, d);
  WSAStartup(0x0101, wsa);
  for (vuelta = 0; vuelta < 6; vuelta++) {
    struct hostent *he;
    struct sockaddr_in a = { 2 };
    TLS *t = NULL;
    u32 s, t0, total = 0, suma = 0, ncab = 0;
    int n, cuerpo = 0, estado = 0, mal = 0;
    FILE *f = NULL;
    const char *loc;
    if (!parte(dir, &seguro, &puerto)) { printf("La direcci\242n no vale: %s\n", dir); return 1; }
    he = gethostbyname(servidor);
    if (!he) {
      if (WSAGetLastError() == 10050) printf("No hay red (cargue HSPRED).\n");
      else printf("No se encuentra el servidor %s.\n", servidor);
      return 2;
    }
    memcpy(&a.sin_addr, he->h_addr_list[0], 4);
    a.sin_port = htons((u16)puerto);
    printf("Servidor %s: %s:%d%s\n", servidor, inet_ntoa(a.sin_addr), puerto, seguro ? " (cifrado)" : "");
    s = socket(2, 1, 0);
    if (s == ~0u || connect(s, &a, sizeof a)) { printf("El servidor no contesta.\n"); return 3; }
    if (seguro) {
      t = TlsAbrir(s, servidor);
      t0 = GetTickCount();
      if (!t || TlsConectar(t, 60000) != 1) {
        printf("No se ha podido hacer la conexi\242n segura (%d).\n", t ? TlsError(t) : 0);
        if (t) di1252(TlsTextoError(TlsError(t)));
        return 4;
      }
      if (verboso) {
        TlsInfo(t, buf, 100);
        printf("Conexi\242n segura: %s (%u ms)\n", buf, GetTickCount() - t0);
      }
    }
    n = sprintf(pet, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Hispa-486 (HTTPS.EXE)\r\nAccept: */*\r\n\r\n", ruta, servidor);
    if ((t ? TlsEscribir(t, pet, n) : send(s, pet, n, 0)) != n) { printf("No se ha podido enviar la petici\242n.\n"); return 5; }
    cab[0] = '\n';
    ncab = 1;
    for (;;) {
      char *p = buf;
      n = t ? TlsLeer(t, buf, sizeof buf, 60000) : recv(s, buf, sizeof buf, 0);
      if (n < 0) { printf("Se ha cortado la conexi\242n.\n"); mal = 1; break; }
      if (n == 0) break;
      if (!cuerpo) {
        for (; n && !cuerpo; n--, p++) {
          if (ncab < sizeof cab - 1) cab[ncab++] = *p;
          cab[ncab] = 0;
          if (ncab >= 5 && !memcmp(cab + ncab - 4, "\r\n\r\n", 4)) cuerpo = 1;
          else if (ncab >= 3 && !memcmp(cab + ncab - 2, "\n\n", 2)) cuerpo = 1;
        }
        if (!cuerpo) continue;
        estado = (!memcmp(cab + 1, "HTTP/", 5) && strchr(cab + 1, ' ')) ? atoi(strchr(cab + 1, ' ') + 1) : 0;
        if (verboso) printf("%s", cab + 1);
        if (estado == 200 && !sin_guardar) {
          if (!archivo) { nombre83(nom); archivo = nom; }
          f = fopen(archivo, "wb");
          if (!f) { printf("No se puede crear %s.\n", archivo); return 6; }
        }
      }
      if (estado == 200) {
        for (k = 0; k < n; k++) suma += (u8)p[k];
        total += n;
        if (f && fwrite(p, 1, n, f) != (u32)n) { printf("Disco lleno.\n"); mal = 1; break; }
      }
    }
    if (t) TlsCerrar(t);
    closesocket(s);
    if (f) { fclose(f); if (mal) remove(archivo); }
    if (mal) return 7;
    loc = cabecera("Location");
    if (estado >= 301 && estado <= 308 && loc) {
      char nueva[1024];
      for (k = 0; loc[k] && loc[k] != '\r' && loc[k] != '\n' && k < 1000; k++) nueva[k] = loc[k];
      nueva[k] = 0;
      if (nueva[0] == '/') sprintf(dir, "%s://%s:%d%s", seguro ? "https" : "http", servidor, puerto, nueva);
      else strcpy(dir, nueva);
      printf("Redirigido a %s\n", dir);
      continue;
    }
    if (estado != 200) {
      *strchr(cab + 1, '\r' ) = 0;
      printf("El servidor responde: %s\n", cab + 1);
      return 8;
    }
    if (f) printf("Guardado en %s (%u octetos).\n", archivo, total);
    else printf("Recibidos %u octetos, suma %u.\n", total, suma);
    WSACleanup();
    return 0;
  }
  printf("Demasiadas redirecciones.\n");
  return 9;
}
