/* redprue.c - prueba de WSOCK32.DLL de Hispa-486 (docs/RED.md): la pila
 * TCP/IP de los programas Hispa32, desde un programa de consola.
 *   REDPRUE servidor puerto ruta   -> pide la ruta por HTTP/1.0 y dice la
 *       respuesta: la primera línea, los octetos del cuerpo y su suma
 *   REDPRUE servidor puerto ruta P -> lo mismo con el socket sin bloqueo y
 *       select; y antes, una conexión a un puerto cerrado (rechazada)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
#define WINAPI __attribute__((stdcall))
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
char *WINAPI inet_ntoa(struct in_addr);
u16 WINAPI htons(u16);
int WINAPI HispaRedConfig(u8 *, u8 *);
int printf(const char *, ...);
int sprintf(char *, const char *, ...);
int atoi(const char *);
u32 strlen(const char *);
void *memcpy(void *, const void *, u32);
char *strstr(const char *, const char *);
int memcmp(const void *, const void *, u32);

static char buf[4096], cab[2048];

int main(int argc, char **argv) {
  static char wsa[400];
  u8 c[16], mac[6];
  struct hostent *he;
  struct sockaddr_in a = { 2 };
  struct in_addr ia;
  u32 s, suma = 0, total = 0, ncab = 0, uno = 1;
  int n, cuerpo = 0, sinbloq = argc > 4;
  if (argc < 4) { printf("REDPRUE servidor puerto ruta [P]\n"); return 1; }
  WSAStartup(0x0101, wsa);
  if (!HispaRedConfig(c, mac)) { printf("No hay red\n"); return 2; }
  memcpy(&ia, c, 4);
  printf("IP %s", inet_ntoa(ia));
  memcpy(&ia, c + 8, 4);
  printf(" puerta %s MAC %02X:%02X:%02X:%02X:%02X:%02X\n", inet_ntoa(ia), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  he = gethostbyname(argv[1]);
  if (!he) { printf("No se encuentra %s (error %d)\n", argv[1], WSAGetLastError()); return 3; }
  memcpy(&a.sin_addr, he->h_addr_list[0], 4);
  printf("%s es %s\n", argv[1], inet_ntoa(a.sin_addr));
  if (sinbloq) {                 /* un puerto cerrado: rechazada */
    a.sin_port = htons(9);
    s = socket(2, 1, 0);
    n = connect(s, &a, sizeof a);
    printf("Puerto cerrado: %d, error %d\n", n, WSAGetLastError());
    closesocket(s);
  }
  a.sin_port = htons((u16)atoi(argv[2]));
  s = socket(2, 1, 0);
  if (s == ~0u) { printf("socket: error %d\n", WSAGetLastError()); return 4; }
  if (sinbloq) {
    fd_set e, x;
    struct timeval t = { 10, 0 };
    ioctlsocket(s, 0x8004667Eu, &uno);
    n = connect(s, &a, sizeof a);
    printf("connect sin bloqueo: %d, error %d\n", n, WSAGetLastError());
    e.fd_count = 1; e.fd_array[0] = s; x = e;
    n = select(0, 0, &e, &x, &t);
    printf("select: %d (escribir %u, fallo %u)\n", n, e.fd_count, x.fd_count);
    if (!e.fd_count) return 5;
  } else if (connect(s, &a, sizeof a)) { printf("connect: error %d\n", WSAGetLastError()); return 5; }
  n = sprintf(buf, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: REDPRUE\r\n\r\n", argv[3], argv[1]);
  if (send(s, buf, n, 0) != n) { printf("send: error %d\n", WSAGetLastError()); return 6; }
  for (;;) {
    int k;
    if (sinbloq) {
      fd_set l;
      struct timeval t = { 20, 0 };
      l.fd_count = 1; l.fd_array[0] = s;
      if (select(0, &l, 0, 0, &t) != 1) { printf("select: nada en 20 s\n"); return 7; }
    }
    n = recv(s, buf, sizeof buf, 0);
    if (n < 0) { printf("recv: error %d\n", WSAGetLastError()); return 7; }
    if (n == 0) break;
    for (k = 0; k < n; k++) {
      if (!cuerpo) {
        if (ncab < sizeof cab - 1) cab[ncab++] = buf[k];
        cab[ncab] = 0;
        if (ncab >= 4 && !memcmp(cab + ncab - 4, "\r\n\r\n", 4)) cuerpo = 1;
      } else { suma += (u8)buf[k]; total++; }
    }
  }
  closesocket(s);
  *strstr(cab, "\r\n") = 0;
  printf("Respuesta: %s\n", cab);
  printf("Cuerpo: %u octetos, suma %u\n", total, suma);
  WSACleanup();
  return 0;
}
int memcmp(const void *, const void *, u32);
