/* =============================================================================
 *  WSOCK32.DLL de Hispa-486: la red de los programas Hispa32 (docs/RED.md).
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *  Dos cosas en un archivo:
 *   1. Una pila TCP/IP propia, en C: Ethernet II, ARP, IPv4 (sin fragmentos),
 *      ICMP (contesta al eco), UDP, cliente de DHCP, cliente de DNS (registros
 *      A) y TCP de cliente con varias conexiones a la vez (reenvíos, ventana,
 *      cierre ordenado). Escrita desde las RFC (791, 792, 768, 793, 826,
 *      1122, 2131, 1035), como la de lib/red.inc. Habla con el «packet
 *      driver» (HSPRED) por tres funciones de HW32 (HispaRedAbrir,
 *      HispaRedEnviar, HispaRedCerrar: utils/hw32k.inc) y lee las tramas que
 *      llegan de un anillo en memoria convencional.
 *   2. Encima, el subconjunto de Winsock 1.1 que usan los programas: los
 *      nombres, las estructuras y los números de error son los documentados
 *      del API (las funciones, stdcall). Lo que no hay: servidores (listen y
 *      accept), datos urgentes, las WSAAsync* y la importación por número.
 *
 *  No hay hilos ni interrupciones aquí: la pila avanza cada vez que el
 *  programa llama a una función (sondear). Las llamadas que esperan ceden el
 *  turno con Sleep(0) (dentro de Hispa Ventanas, a las demás tareas).
 *  La configuración IP es la que guarda HSPRED (la de CONFRED); si no hay,
 *  se consigue aquí (RED.INI: DHCP o a mano) y se le deja a HSPRED.
 *  Se compila con gcc -m32 sin biblioteca y tools/elf2pe.py --dll.
 * ============================================================================= */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef void *HANDLE;
#define WINAPI __attribute__((stdcall))
#define NULL ((void *)0)

/* KERNEL32 (las tres primeras, nuestras: utils/hw32k.inc) */
typedef struct {
  u8 mac[6]; u16 vector;
  u8 *anillo;                 /* +0 cabeza, +2 cola, +4 perdidas (palabras) */
  u8 *cfg;                    /* la configuración compartida de HSPRED o NULL */
  u32 n_ran, t_ran;           /* ranuras y tamaño de cada una */
  u8 *ranuras;                /* cada ranura: longitud (2) y la trama */
  u32 lineal;                 /* la dirección lineal del anillo */
} HRED;
int WINAPI HispaRedAbrir(HRED *);
int WINAPI HispaRedEnviar(const void *, u32);
int WINAPI HispaRedCerrar(void);
void WINAPI Sleep(u32);
typedef struct { u16 anio, mes, sem, dia, hora, min, seg, ms; } ST;
void WINAPI GetLocalTime(ST *);
u32 WINAPI GetEnvironmentVariableA(const char *, char *, u32);
HANDLE WINAPI CreateFileA(const char *, u32, u32, void *, u32, u32, HANDLE);
int WINAPI ReadFile(HANDLE, void *, u32, u32 *, void *);
int WINAPI CloseHandle(HANDLE);
HANDLE WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(HANDLE, u32, u32);
int WINAPI HeapFree(HANDLE, u32, void *);
u32 WINAPI GetModuleFileNameA(HANDLE, char *, u32);
u32 WINAPI GetTickCount(void);
/* MSVCRT */
void *memcpy(void *, const void *, u32);
void *memset(void *, int, u32);
int memcmp(const void *, const void *, u32);
u32 strlen(const char *);

/* ---- Winsock 1.1: estructuras y números ---- */
typedef u32 SOCKET;
struct in_addr { u32 s_addr; };
struct sockaddr_in { short sin_family; u16 sin_port; struct in_addr sin_addr; char sin_zero[8]; };
struct hostent { char *h_name; char **h_aliases; short h_addrtype, h_length; char **h_addr_list; };
typedef struct { u32 fd_count; SOCKET fd_array[64]; } fd_set;
struct timeval { int tv_sec, tv_usec; };
typedef struct {
  u16 wVersion, wHighVersion; char szDescription[257], szSystemStatus[129];
  u16 iMaxSockets, iMaxUdpDg; char *lpVendorInfo;
} WSADATA;
#define INVALID_SOCKET ((SOCKET)~0)
#define SOCKET_ERROR (-1)
#define FIONBIO 0x8004667Eu
#define FIONREAD 0x4004667Fu
#define WSAEINTR 10004
#define WSAEFAULT 10014
#define WSAEINVAL 10022
#define WSAEMFILE 10024
#define WSAEWOULDBLOCK 10035
#define WSAEALREADY 10037
#define WSAENOTSOCK 10038
#define WSAEMSGSIZE 10040
#define WSAEPROTONOSUPPORT 10043
#define WSAEOPNOTSUPP 10045
#define WSAEAFNOSUPPORT 10047
#define WSAENETDOWN 10050
#define WSAENETUNREACH 10051
#define WSAECONNABORTED 10053
#define WSAECONNRESET 10054
#define WSAENOBUFS 10055
#define WSAEISCONN 10056
#define WSAENOTCONN 10057
#define WSAESHUTDOWN 10058
#define WSAETIMEDOUT 10060
#define WSAECONNREFUSED 10061
#define WSANOTINITIALISED 10093
#define WSAHOST_NOT_FOUND 11001
#define WSATRY_AGAIN 11002
#define WSANO_DATA 11004

/* ---- la pila ---- */
#define N_SOCK 8
#define RXB 16368             /* lo que guarda cada conexión por leer */
#define TXB 8176              /*   y por enviar (o sin acuse) */
#define MSS 1460
#define VENT_MAX (8 * MSS)    /* (el anillo de HW32 tiene 12 ranuras) */
#define RTO0 1000             /* ms: primer reenvío; luego se dobla */
#define N_ARP 8
enum { E_CERRADO, E_SYN, E_ESTAB };
enum { F_FIN = 1, F_SYN = 2, F_RST = 4, F_PSH = 8, F_ACK = 16 };

typedef struct {
  u8 uso;                     /* 0 libre, 1 TCP, 2 UDP */
  u8 estado, huerfano, nobloq, fin_rx, fin_pedido, fin_enviado, fin_acusado;
  u8 ack_pend, estuvo, sin_leer;
  int error;
  u32 ip_rem;
  u16 p_rem, p_loc, mss;
  u32 snd_una, snd_nxt, snd_max, rcv_nxt, snd_wnd, vent_dicha;
  u32 t_rtx, rto, t_muerte;
  int reintentos;
  u8 *rx, *tx;
  u32 rx_ini, rx_n, tx_ini, tx_n;
} SOCK;

static SOCK socks[N_SOCK];
static HRED red;
static int abierta, iniciada, ult_error, dentro;
static u32 mi_ip, mi_masc, mi_gw, mi_dns;
static volatile u32 *bios_tics;
static u32 tic_ant, tic_dias, p_sig;
static u8 trama[1514];
static u8 pend[1514];          /* una trama que espera a que ARP conteste */
static u32 pend_ip, pend_n, pend_t;
static struct { u32 ip; u8 mac[6]; u8 vale; u32 t; } arp[N_ARP];
static u16 ip_id;
static const u8 difusion[6] = { 255, 255, 255, 255, 255, 255 };
/* DHCP y DNS: lo que esperan */
static u32 dh_xid, dh_ip, dh_masc, dh_gw, dh_dns, dh_srv, dh_dura;
static int dh_tipo;
static u16 dns_id, dns_puerto;
static u32 dns_ip;
static int dns_estado;         /* 0 nada, 1 esperando, 2 hay, 3 no existe */

static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static void p16(u8 *p, u32 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static void p32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static u32 ip_de(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }

/* los milisegundos, de los tics de la BIOS (18,2 por segundo; a medianoche
 * vuelven a cero) */
static u32 ahora(void) {
  u32 t = bios_tics ? *bios_tics : 0;
  if (t < tic_ant) tic_dias += 0x1800B0;
  tic_ant = t;
  return (tic_dias + t) * 55;
}
/* ---- el azar de la red (el número inicial de TCP, el de las preguntas de
 * DNS y su puerto, el de DHCP): ChaCha20 (RFC 8439) como generador, con
 * «borrado de la clave» (cada bloque da la clave del siguiente y 8 palabras
 * para fuera: lo que sale no deja saber ni lo de antes ni lo de después).
 * La semilla, como la de HSPTLS (w32/tls/hsptls.c): TSC si lo hay, PIT,
 * los tics, la hora, la MAC y HSPTLS.SEM (solo se lee: la escribe HSPTLS);
 * y luego el momento en que llega cada trama. Antes era un xorshift de 32
 * bits que daba su estado entero: con un número inicial de TCP se sabía el
 * de la siguiente pregunta de DNS y su puerto ---- */
#define ROTL(v, n) ((v) << (n) | (v) >> (32 - (n)))
#define QR(a, b, c, d) (a += b, d ^= a, d = ROTL(d, 16), c += d, b ^= c, b = ROTL(b, 12), \
                        a += b, d ^= a, d = ROTL(d, 8), c += d, b ^= c, b = ROTL(b, 7))
/* un bloque de ChaCha20: clave k (8 palabras), contador y nonce (3) */
static void chacha_bloque(const u32 *k, u32 cont, const u32 *nonce, u32 *sal) {
  u32 x[16], i;
  x[0] = 0x61707865; x[1] = 0x3320646e; x[2] = 0x79622d32; x[3] = 0x6b206574;
  for (i = 0; i < 8; i++) x[4 + i] = k[i];
  x[12] = cont; x[13] = nonce[0]; x[14] = nonce[1]; x[15] = nonce[2];
  for (i = 0; i < 16; i++) sal[i] = x[i];
  for (i = 0; i < 10; i++) {
    QR(sal[0], sal[4], sal[8], sal[12]); QR(sal[1], sal[5], sal[9], sal[13]);
    QR(sal[2], sal[6], sal[10], sal[14]); QR(sal[3], sal[7], sal[11], sal[15]);
    QR(sal[0], sal[5], sal[10], sal[15]); QR(sal[1], sal[6], sal[11], sal[12]);
    QR(sal[2], sal[7], sal[8], sal[13]); QR(sal[3], sal[4], sal[9], sal[14]);
  }
  for (i = 0; i < 16; i++) sal[i] += x[i];
}
static u32 az_clave[8], az_sal[8], az_ruido[4], isn_clave[8];
static int az_quedan, az_listo, hay_tsc;
#ifdef WSOCK_ANFITRION
/* (la prueba en el anfitrión, tests/red32/azar.c: sin puertos ni CPUID) */
static u32 pit(void) { return 0; }
static int cpuid_tsc(void) { return 0; }
#else
static u32 pit(void) {
  u8 a, b;
  __asm__ volatile("outb %%al, $0x43" : : "a"(0));
  __asm__ volatile("inb $0x40, %%al" : "=a"(a));
  __asm__ volatile("inb $0x40, %%al" : "=a"(b));
  return a | b << 8;
}
static int cpuid_tsc(void) {
  u32 a, b, c, d, f1, f2;
  __asm__ volatile("pushfl; popl %0; movl %0, %1; xorl $0x200000, %0; pushl %0; popfl; pushfl; popl %0; pushl %1; popfl"
                   : "=&r"(f1), "=&r"(f2));
  if (!((f1 ^ f2) & 0x200000)) return 0;       /* un 486 sin CPUID */
  __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
  if (a < 1) return 0;
  __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
  return (d >> 4) & 1;
}
#endif
/* un poco de ruido (el reloj fino de ahora) al pozo: se mezcla en la clave
 * la próxima vez que se rehace */
static void az_ruido_pon(u32 v) {
  u32 lo = 0, hi = 0;
#ifndef WSOCK_ANFITRION
  if (hay_tsc) __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
#endif
  az_ruido[0] += v; az_ruido[1] ^= pit() | (bios_tics ? *bios_tics : 0) << 16;
  az_ruido[2] += lo; az_ruido[3] ^= hi + GetTickCount();
  az_ruido[0] = ROTL(az_ruido[0], 7) ^ az_ruido[2];
}
/* lo siguiente: un bloque cada 8 palabras; el bloque da la clave nueva */
static void az_inicia(const u8 *mac);
static u32 azar(void) {
  if (!az_listo) az_inicia(NULL);
  if (!az_quedan) {
    static const u32 n0[3] = { 0x48535052, 0x45442d41, 0x5a415221 };   /* «HSPRED-AZAR!» */
    u32 b[16], k;
    for (k = 0; k < 4; k++) az_clave[k] ^= az_ruido[k];
    az_ruido[0] = az_ruido[1] = az_ruido[2] = az_ruido[3] = 0;
    chacha_bloque(az_clave, 0, n0, b);
    for (k = 0; k < 8; k++) { az_clave[k] = b[k]; az_sal[k] = b[8 + k]; }
    memset(b, 0, sizeof b);
    az_quedan = 8;
  }
  return az_sal[--az_quedan];
}
/* la semilla: una vez, al abrir la red */
static void az_inicia(const u8 *mac) {
  ST st;
  char ruta[170];
  u8 sem[32];
  u32 n, k;
  HANDLE h;
  hay_tsc = cpuid_tsc();
  GetLocalTime(&st);
  memcpy(az_clave, &st, sizeof st);            /* 16 octetos: las 4 primeras palabras */
  az_clave[4] ^= GetTickCount();
  if (mac) { az_clave[5] ^= mac[0] | mac[1] << 8 | mac[2] << 16 | (u32)mac[3] << 24; az_clave[6] ^= mac[4] | mac[5] << 8; }
  az_ruido_pon(0x9E3779B9u);
  /* la semilla de HSPTLS (junto al programa): no se escribe aquí */
  n = GetModuleFileNameA(NULL, ruta, sizeof ruta - 12);
  if (n >= sizeof ruta - 12) n = 0;
  for (k = n; k > 0 && ruta[k - 1] != '\\' && ruta[k - 1] != ':'; k--) ;
  memcpy(ruta + k, "HSPTLS.SEM", 11);
  h = CreateFileA(ruta, 0x80000000u, 1, NULL, 3, 0, NULL);
  if (h != (HANDLE)-1) {
    n = 0;
    if (ReadFile(h, sem, sizeof sem, &n, NULL) && n)
      for (k = 0; k < n && k < 32; k++) ((u8 *)az_clave)[k] ^= sem[k];
    CloseHandle(h);
    memset(sem, 0, sizeof sem);
  }
  az_ruido_pon(0x7F4A7C15u);
  az_quedan = 0;
  az_listo = 1;
  /* la clave del número inicial de TCP (RFC 6528), aparte */
  for (k = 0; k < 8; k++) isn_clave[k] = azar();
  az_quedan = 0;                               /* (lo que quedaba del bloque, fuera) */
}
/* el número inicial de una conexión TCP (RFC 6528): un reloj de 4 µs más
 * una función con clave de las dos direcciones y los dos puertos; así no
 * dice nada de las demás conexiones ni del generador */
static u32 isn_de(u32 ip_loc, u16 p_loc, u32 ip_rem, u16 p_rem, u32 ms) {
  u32 nonce[3], b[16];
  nonce[0] = ip_loc; nonce[1] = ip_rem; nonce[2] = (u32)p_loc << 16 | p_rem;
  chacha_bloque(isn_clave, 0x6528, nonce, b);
  return ms * 250 + b[0];
}
static u32 suma(const u8 *p, int n, u32 s) {
  for (; n > 1; n -= 2, p += 2) s += (u32)p[0] << 8 | p[1];
  if (n) s += (u32)p[0] << 8;
  return s;
}
static u16 cierra_suma(u32 s) {
  while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
  return (u16)~s;
}

/* ---- Ethernet y ARP ---- */
static int eth_enviar(const u8 *mac, u16 tipo, u8 *t, int n) {
  memcpy(t, mac, 6);
  memcpy(t + 6, red.mac, 6);
  p16(t + 12, tipo);
  return HispaRedEnviar(t, n + 14);
}
static void arp_enviar(int op, const u8 *mac, u32 ip) {
  u8 b[42];
  p16(b + 14, 1); p16(b + 16, 0x0800); b[18] = 6; b[19] = 4; p16(b + 20, op);
  memcpy(b + 22, red.mac, 6); memcpy(b + 28, &mi_ip, 4);
  if (op == 1) memset(b + 32, 0, 6); else memcpy(b + 32, mac, 6);
  memcpy(b + 38, &ip, 4);
  eth_enviar(op == 1 ? difusion : mac, 0x0806, b, 28);
}
static void arp_apunta(u32 ip, const u8 *mac, int nueva) {
  int k, v = 0;
  for (k = 0; k < N_ARP; k++) {
    if (arp[k].vale && arp[k].ip == ip) { v = k; goto pon; }
    if (!arp[k].vale || (arp[v].vale && arp[k].t < arp[v].t)) v = k;
  }
  if (!nueva) return;
pon:
  arp[v].ip = ip; arp[v].vale = 1; arp[v].t = ahora();
  memcpy(arp[v].mac, mac, 6);
  if (pend_n && pend_ip == ip) {      /* la trama que esperaba */
    memcpy(pend, mac, 6);
    HispaRedEnviar(pend, pend_n);
    pend_n = 0;
  }
}
static void arp_in(const u8 *p, int n) {
  u32 de, para;
  if (n < 28 || be16(p) != 1 || be16(p + 2) != 0x0800) return;
  de = ip_de(p + 14); para = ip_de(p + 24);
  if (!mi_ip) return;
  arp_apunta(de, p + 8, para == mi_ip);
  if (be16(p + 6) == 1 && para == mi_ip) arp_enviar(2, p + 8, de);
}
static const u8 *arp_busca(u32 ip) {
  int k;
  for (k = 0; k < N_ARP; k++) if (arp[k].vale && arp[k].ip == ip) return arp[k].mac;
  return NULL;
}

/* ---- IP: la carga ya está en trama+34 ---- */
static void ip_enviar(u32 de, u32 a, int proto, int n) {
  u8 *h = trama + 14;
  const u8 *mac;
  u32 salto;
  h[0] = 0x45; h[1] = 0; p16(h + 2, n + 20); p16(h + 4, ++ip_id); p16(h + 6, 0x4000);
  h[8] = 64; h[9] = (u8)proto; h[10] = h[11] = 0;
  memcpy(h + 12, &de, 4); memcpy(h + 16, &a, 4);
  p16(h + 10, cierra_suma(suma(h, 20, 0)));
  if (a == 0xFFFFFFFFu) { eth_enviar(difusion, 0x0800, trama, n + 20); return; }
  salto = ((a ^ mi_ip) & mi_masc) == 0 ? a : mi_gw;
  if (!salto) return;
  mac = arp_busca(salto);
  if (mac) { eth_enviar(mac, 0x0800, trama, n + 20); return; }
  /* no se sabe su MAC: se pregunta y la trama espera (una sola) */
  memcpy(trama + 6, red.mac, 6); p16(trama + 12, 0x0800);
  memcpy(pend, trama, n + 34);
  pend_n = n + 34; pend_ip = salto; pend_t = ahora();
  arp_enviar(1, NULL, salto);
}
static u32 seudo(u32 de, u32 a, int proto, int n) {
  u32 s = suma((const u8 *)&de, 4, 0);
  s = suma((const u8 *)&a, 4, s);
  return s + proto + n;
}

/* ---- UDP ---- */
static void udp_enviar(u32 de, u32 a, u16 pde, u16 pa, const u8 *datos, int n) {
  u8 *u = trama + 34;
  u16 c;
  p16(u, pde); p16(u + 2, pa); p16(u + 4, n + 8); u[6] = u[7] = 0;
  if (datos != u + 8) memcpy(u + 8, datos, n);
  c = cierra_suma(suma(u, n + 8, seudo(de, a, 17, n + 8)));
  p16(u + 6, c ? c : 0xFFFF);
  ip_enviar(de, a, 17, n + 8);
}
static void dhcp_in(const u8 *b, int n) {
  const u8 *o;
  int tipo = 0;
  u32 masc = 0, gw = 0, dns = 0, srv = 0, dura = 0;
  if (n < 240 || b[0] != 2 || be32(b + 4) != dh_xid || be32(b + 236) != 0x63825363) return;
  if (memcmp(b + 28, red.mac, 6)) return;
  for (o = b + 240; o < b + n && *o != 255; ) {
    int c = o[0], l;
    if (c == 0) { o++; continue; }
    if (o + 2 > b + n) break;
    l = o[1];
    if (o + 2 + l > b + n) break;
    if (c == 53 && l >= 1) tipo = o[2];
    if (c == 1 && l >= 4) masc = ip_de(o + 2);
    if (c == 3 && l >= 4) gw = ip_de(o + 2);
    if (c == 6 && l >= 4) dns = ip_de(o + 2);
    if (c == 54 && l >= 4) srv = ip_de(o + 2);
    if (c == 51 && l >= 4) dura = be32(o + 2);
    o += 2 + l;
  }
  if (tipo == 2 && dh_tipo == 0) {             /* OFFER */
    dh_ip = ip_de(b + 16); dh_srv = srv; dh_tipo = 2;
  } else if (tipo == 5 && dh_tipo == 3) {      /* ACK */
    dh_ip = ip_de(b + 16); dh_masc = masc; dh_gw = gw; dh_dns = dns; dh_dura = dura;
    if (srv) dh_srv = srv;
    dh_tipo = 5;
  } else if (tipo == 6 && dh_tipo == 3) dh_tipo = 6;   /* NAK */
}
static void dns_in(const u8 *b, int n) {
  const u8 *p = b + 12, *fin = b + n;
  int k, nq, na;
  if (n < 12 || be16(b) != dns_id || !(b[2] & 0x80) || dns_estado != 1) return;
  if ((b[3] & 15) == 3) { dns_estado = 3; return; }
  if (b[3] & 15) return;
  nq = be16(b + 4); na = be16(b + 6);
  for (k = 0; k < nq; k++) {
    while (p < fin && *p && (*p & 0xC0) != 0xC0) p += *p + 1;
    p += (p < fin && *p) ? 2 : 1;
    p += 4;
  }
  for (k = 0; k < na && p < fin; k++) {
    while (p < fin && *p && (*p & 0xC0) != 0xC0) p += *p + 1;
    p += (p < fin && *p) ? 2 : 1;
    if (p + 10 > fin) return;
    if (be16(p) == 1 && be16(p + 2) == 1 && be16(p + 8) == 4 && p + 14 <= fin) {
      dns_ip = ip_de(p + 10);
      dns_estado = 2;
      return;
    }
    p += 10 + be16(p + 8);
  }
  dns_estado = 3;                              /* contestó, sin dirección */
}
static void udp_in(u32 de, u32 a, const u8 *u, int n) {
  u16 pde, pa;
  int l, k;
  if (n < 8) return;
  l = be16(u + 4);
  if (l < 8 || l > n) return;
  if (be16(u + 6) && cierra_suma(suma(u, l, seudo(de, a, 17, l)))) return;
  pde = be16(u); pa = be16(u + 2);
  l -= 8;
  if (pa == 68 && pde == 67) { dhcp_in(u + 8, l); return; }
  /* (la respuesta de DNS: del servidor al que se preguntó, desde el 53 y al
   * puerto de la pregunta; la de otro, no vale) */
  if (dns_puerto && pa == dns_puerto) { if (pde == 53 && de == mi_dns) dns_in(u + 8, l); return; }
  for (k = 0; k < N_SOCK; k++) {
    SOCK *s = &socks[k];
    if (s->uso != 2 || s->p_loc != pa) continue;
    if (s->rx_n + l + 8 > RXB) return;         /* no cabe: se pierde */
    {
      u8 *r = s->rx + s->rx_n;
      r[0] = (u8)l; r[1] = (u8)(l >> 8);
      memcpy(r + 2, &de, 4); r[6] = (u8)pde; r[7] = (u8)(pde >> 8);
      memcpy(r + 8, u + 8, l);
      s->rx_n += l + 8;
    }
    return;
  }
}

/* ---- TCP ---- */
#define MENOR(a, b) ((int)((a) - (b)) < 0)
#define MAYOR(a, b) ((int)((a) - (b)) > 0)
/* un segmento: los datos (nd) ya están en trama+54+nopc */
static void tcp_arma(u32 a, u16 pl, u16 pr, u32 seq, u32 ack, int fl, u32 vent, int nopc, int nd) {
  u8 *t = trama + 34;
  int n = 20 + nopc + nd;
  p16(t, pl); p16(t + 2, pr); p32(t + 4, seq); p32(t + 8, ack);
  t[12] = (u8)((20 + nopc) << 2); t[13] = (u8)fl; p16(t + 14, vent); t[16] = t[17] = t[18] = t[19] = 0;
  p16(t + 16, cierra_suma(suma(t, n, seudo(mi_ip, a, 6, n))));
  ip_enviar(mi_ip, a, 6, n);
}
static u32 ventana(SOCK *s) {
  u32 v = RXB - s->rx_n;
  return v > VENT_MAX ? VENT_MAX : v;
}
static void tcp_ack(SOCK *s) {
  s->vent_dicha = ventana(s);
  tcp_arma(s->ip_rem, s->p_loc, s->p_rem, s->snd_nxt, s->rcv_nxt, F_ACK, s->vent_dicha, 0, 0);
  s->ack_pend = 0;
}
static void tcp_salida(SOCK *s, int sonda) {
  if (s->estado == E_SYN) {
    u8 *o = trama + 54;
    o[0] = 2; o[1] = 4; p16(o + 2, MSS);
    s->vent_dicha = ventana(s);
    tcp_arma(s->ip_rem, s->p_loc, s->p_rem, s->snd_una, 0, F_SYN, s->vent_dicha, 4, 0);
    s->snd_nxt = s->snd_max = s->snd_una + 1;
    if (!s->t_rtx) s->t_rtx = ahora() + s->rto;
    return;
  }
  if (s->estado != E_ESTAB) return;
  for (;;) {
    u32 ido = s->snd_nxt - s->snd_una, queda, vent, n, k, ini;
    int fl = F_ACK;
    if (ido > s->tx_n) break;                  /* el FIN ya salió */
    queda = s->tx_n - ido;
    vent = s->snd_wnd > ido ? s->snd_wnd - ido : 0;
    if (sonda && !vent && !ido) vent = 1;      /* ventana cerrada: un octeto */
    n = queda < vent ? queda : vent;
    if (n > s->mss) n = s->mss;
    if (!n && !(queda == 0 && s->fin_pedido && !s->fin_enviado)) break;
    ini = s->tx_ini + ido;
    if (ini >= TXB) ini -= TXB;
    k = TXB - ini;
    if (k > n) k = n;
    memcpy(trama + 54, s->tx + ini, k);
    memcpy(trama + 54 + k, s->tx, n - k);
    if (n == queda) {
      if (n) fl |= F_PSH;
      if (s->fin_pedido) { fl |= F_FIN; s->fin_enviado = 1; }
    }
    s->vent_dicha = ventana(s);
    tcp_arma(s->ip_rem, s->p_loc, s->p_rem, s->snd_nxt, s->rcv_nxt, fl, s->vent_dicha, 0, n);
    s->snd_nxt += n + ((fl & F_FIN) ? 1 : 0);
    if (MAYOR(s->snd_nxt, s->snd_max)) s->snd_max = s->snd_nxt;
    if (!s->t_rtx) s->t_rtx = ahora() + s->rto;
    s->ack_pend = 0;
    if (fl & F_FIN) break;
  }
}
static void tcp_suelta(SOCK *s) {
  HANDLE m = GetProcessHeap();
  if (s->rx) HeapFree(m, 0, s->rx);
  if (s->tx) HeapFree(m, 0, s->tx);
  memset(s, 0, sizeof *s);
}
static void tcp_fuera(SOCK *s, int error) {    /* la conexión se acabó */
  s->estado = E_CERRADO;
  s->t_rtx = 0;
  if (error && !s->error) s->error = error;
  if (s->huerfano) tcp_suelta(s);
}
static void tcp_rst(u32 a, u16 pl, u16 pr, u32 seq, u32 ack, int con_ack) {
  tcp_arma(a, pl, pr, seq, ack, con_ack ? F_RST | F_ACK : F_RST, 0, 0, 0);
}
static void tcp_in(u32 de, u32 a, const u8 *t, int n) {
  u16 pde, pa;
  u32 seq, ack, vent;
  int cab, fl, nd, k;
  const u8 *d;
  SOCK *s = NULL;
  if (n < 20 || a != mi_ip) return;
  if (cierra_suma(suma(t, n, seudo(de, a, 6, n)))) return;
  pde = be16(t); pa = be16(t + 2); seq = be32(t + 4); ack = be32(t + 8);
  cab = (t[12] >> 4) * 4; fl = t[13]; vent = be16(t + 14);
  if (cab < 20 || cab > n) return;
  d = t + cab; nd = n - cab;
  for (k = 0; k < N_SOCK; k++)
    if (socks[k].uso == 1 && socks[k].estado != E_CERRADO && socks[k].p_loc == pa
        && socks[k].p_rem == pde && socks[k].ip_rem == de) { s = &socks[k]; break; }
  if (!s) {                                    /* de nadie: se rechaza */
    if (fl & F_RST) return;
    if (fl & F_ACK) tcp_rst(de, pa, pde, ack, 0, 0);
    else tcp_rst(de, pa, pde, 0, seq + nd + ((fl & F_SYN) ? 1 : 0) + ((fl & F_FIN) ? 1 : 0), 1);
    return;
  }
  if (s->estado == E_SYN) {
    if ((fl & F_ACK) && ack != s->snd_nxt) { if (!(fl & F_RST)) tcp_rst(de, pa, pde, ack, 0, 0); return; }
    if (fl & F_RST) { if (fl & F_ACK) tcp_fuera(s, WSAECONNREFUSED); return; }
    if ((fl & (F_SYN | F_ACK)) != (F_SYN | F_ACK)) return;
    s->mss = 536;
    for (k = 20; k + 1 < cab; ) {              /* las opciones: el MSS */
      if (t[k] == 0) break;
      if (t[k] == 1) { k++; continue; }
      if (t[k + 1] < 2) break;
      if (t[k] == 2 && t[k + 1] == 4 && k + 3 < cab) s->mss = be16(t + k + 2);
      k += t[k + 1];
    }
    if (s->mss > MSS) s->mss = MSS;
    if (s->mss < 64) s->mss = 64;
    s->rcv_nxt = seq + 1; s->snd_una = ack; s->snd_wnd = vent;
    s->estado = E_ESTAB; s->estuvo = 1; s->t_rtx = 0; s->rto = RTO0; s->reintentos = 0;
    tcp_ack(s);
    return;
  }
  if (fl & F_RST) {
    if (!MENOR(seq, s->rcv_nxt) && MENOR(seq, s->rcv_nxt + RXB)) tcp_fuera(s, WSAECONNRESET);
    return;
  }
  if (fl & F_SYN) { tcp_ack(s); return; }      /* un SYN+ACK repetido */
  if (fl & F_ACK) {
    if (MAYOR(ack, s->snd_una) && !MAYOR(ack, s->snd_max)) {
      u32 q = ack - s->snd_una;
      if (q > s->tx_n) { q = s->tx_n; if (s->fin_enviado) s->fin_acusado = 1; }
      s->tx_ini += q; if (s->tx_ini >= TXB) s->tx_ini -= TXB;
      s->tx_n -= q;
      s->snd_una = ack;
      if (MENOR(s->snd_nxt, ack)) s->snd_nxt = ack;
      s->rto = RTO0; s->reintentos = 0;
      s->t_rtx = s->snd_nxt != s->snd_una ? ahora() + s->rto : 0;
    }
    if (!MENOR(ack, s->snd_una)) s->snd_wnd = vent;
  }
  if (nd || (fl & F_FIN)) {
    u32 salto = s->rcv_nxt - seq;              /* lo que ya se tenía */
    s->ack_pend = 1;
    if (!MAYOR(seq, s->rcv_nxt) && salto <= (u32)nd && !s->fin_rx) {
      u32 q = nd - salto, sitio = RXB - s->rx_n, fin, kk;
      int entero = 1;
      d += salto;
      if (s->sin_leer) sitio = q;              /* (cerrado para leer: se tira) */
      if (q > sitio) { q = sitio; entero = 0; }
      if (!s->sin_leer) {
        fin = s->rx_ini + s->rx_n; if (fin >= RXB) fin -= RXB;
        kk = RXB - fin; if (kk > q) kk = q;
        memcpy(s->rx + fin, d, kk);
        memcpy(s->rx, d + kk, q - kk);
        s->rx_n += q;
      }
      s->rcv_nxt += q;
      if ((fl & F_FIN) && entero) { s->fin_rx = 1; s->rcv_nxt++; }
    }
  }
  tcp_salida(s, 0);
  if (s->ack_pend) tcp_ack(s);
  if (s->fin_rx && s->fin_acusado) tcp_fuera(s, 0);
}
static void icmp_in(u32 de, u32 a, const u8 *p, int n) {
  if (n < 8 || n > 1472 || p[0] != 8 || a != mi_ip) return;
  if (cierra_suma(suma(p, n, 0))) return;
  memcpy(trama + 34, p, n);
  trama[34] = 0; trama[36] = trama[37] = 0;
  p16(trama + 36, cierra_suma(suma(trama + 34, n, 0)));
  ip_enviar(mi_ip, de, 1, n);
}
static void ip_in(const u8 *p, int n) {
  int cab, l;
  u32 de, a;
  if (n < 20 || (p[0] >> 4) != 4) return;
  cab = (p[0] & 15) * 4; l = be16(p + 2);
  if (cab < 20 || l < cab || l > n) return;
  if (be16(p + 6) & 0x3FFF) return;            /* fragmentos, no */
  if (cierra_suma(suma(p, cab, 0))) return;
  de = ip_de(p + 12); a = ip_de(p + 16);
  if (mi_ip && a != mi_ip && a != 0xFFFFFFFFu && a != (mi_ip | ~mi_masc)) return;
  if (p[9] == 6) tcp_in(de, a, p + cab, l - cab);
  else if (p[9] == 17) udp_in(de, a, p + cab, l - cab);
  else if (p[9] == 1) icmp_in(de, a, p + cab, l - cab);
}

/* la pila avanza: las tramas que han llegado y los relojes */
static void sondear(void) {
  u16 *an = (u16 *)red.anillo;
  u32 t;
  int k;
  if (!abierta || dentro) return;
  dentro = 1;
  while (an[0] != an[1]) {
    const u8 *r = red.ranuras + an[0] * red.t_ran;
    int n = r[0] | r[1] << 8;
    az_ruido_pon((u32)n << 16 | an[0]);          /* (cuándo llega cada trama: algo de azar) */
    if (n >= 14 && n <= 1514) {
      u16 tipo = be16(r + 14);
      if (tipo == 0x0800) ip_in(r + 16, n - 14);
      else if (tipo == 0x0806) arp_in(r + 16, n - 14);
    }
    an[0] = (u16)(an[0] + 1u >= red.n_ran ? 0 : an[0] + 1);
  }
  t = ahora();
  if (pend_n && (int)(t - pend_t) > 3000) pend_n = 0;
  for (k = 0; k < N_SOCK; k++) {
    SOCK *s = &socks[k];
    if (s->uso != 1) continue;
    if (s->estado != E_CERRADO && s->t_rtx && (int)(t - s->t_rtx) >= 0) {
      if (++s->reintentos > (s->estado == E_SYN ? 4 : 8)) { tcp_fuera(s, WSAETIMEDOUT); continue; }
      s->rto *= 2; if (s->rto > 8000) s->rto = 8000;
      s->t_rtx = t + s->rto;
      if (s->estado == E_ESTAB) {              /* otra vez desde lo último acusado */
        s->snd_nxt = s->snd_una;
        if (!s->fin_acusado) s->fin_enviado = 0;
      }
      tcp_salida(s, 1);
    }
    if (s->huerfano && (int)(t - s->t_muerte) >= 0) {
      if (s->estado != E_CERRADO) tcp_rst(s->ip_rem, s->p_loc, s->p_rem, s->snd_nxt, 0, 0);
      tcp_suelta(s);
    }
  }
  dentro = 0;
}
static void cede(void) { Sleep(0); sondear(); }

/* ---- la configuración: la de HSPRED, o RED.INI (DHCP o a mano) ---- */
static int ip_texto(const char *t, u32 *ip) {
  u8 b[4];
  int k;
  for (k = 0; k < 4; k++) {
    u32 v = 0, n = 0;
    while (*t >= '0' && *t <= '9') { v = v * 10 + (*t++ - '0'); if (++n > 3) return 0; }
    if (!n || v > 255) return 0;
    b[k] = (u8)v;
    if (k < 3) { if (*t != '.') return 0; t++; }
  }
  while (*t == ' ' || *t == '\t' || *t == '\r') t++;
  if (*t && *t != '\n' && *t != ';' && *t != '#') return 0;
  memcpy(ip, b, 4);
  return 1;
}
static int clave(const char *l, const char *c) {
  int n = 0;
  for (; *c; c++, n++) { int x = l[n]; if (x >= 'a' && x <= 'z') x -= 32; if (x != *c) return 0; }
  while (l[n] == ' ' || l[n] == '\t') n++;
  return l[n] == '=' ? n + 1 : 0;
}
static void ini_leer(int *dhcp, u32 *ip, u32 *masc, u32 *gw, u32 *dns) {
  static char b[2048];
  char ruta[40];
  HANDLE h;
  u32 n = 0;
  char *l;
  int k;
  if (!GetEnvironmentVariableA("COMSPEC", ruta, 8) || ruta[1] != ':') ruta[0] = 'C';
  memcpy(ruta + 1, ":\\HISPADOS\\RED.INI", 19);
  h = CreateFileA(ruta, 0x80000000u, 1, NULL, 3, 0, NULL);
  if (h == (HANDLE)-1) return;
  ReadFile(h, b, sizeof b - 1, &n, NULL);
  CloseHandle(h);
  b[n] = 0;
  for (l = b; *l; ) {
    while (*l == ' ' || *l == '\t') l++;
    if ((k = clave(l, "DHCP"))) {
      while (l[k] == ' ') k++;
      *dhcp = !(l[k] == 'N' || l[k] == 'n' || l[k] == '0');
    } else if ((k = clave(l, "IP"))) { while (l[k] == ' ') k++; ip_texto(l + k, ip); }
    else if ((k = clave(l, "MASCARA")) || (k = clave(l, "MASK"))) { while (l[k] == ' ') k++; ip_texto(l + k, masc); }
    else if ((k = clave(l, "PUERTA")) || (k = clave(l, "GATEWAY"))) { while (l[k] == ' ') k++; ip_texto(l + k, gw); }
    else if ((k = clave(l, "DNS"))) { while (l[k] == ' ') k++; ip_texto(l + k, dns); }
    while (*l && *l != '\n') l++;
    if (*l) l++;
  }
}
static void dhcp_enviar(int tipo) {
  u8 *b = trama + 42, *o;
  memset(b, 0, 300);
  b[0] = 1; b[1] = 1; b[2] = 6; p32(b + 4, dh_xid); p16(b + 10, 0x8000);
  memcpy(b + 28, red.mac, 6);
  p32(b + 236, 0x63825363);
  o = b + 240;
  *o++ = 53; *o++ = 1; *o++ = (u8)tipo;
  if (tipo == 3) {
    *o++ = 50; *o++ = 4; memcpy(o, &dh_ip, 4); o += 4;
    *o++ = 54; *o++ = 4; memcpy(o, &dh_srv, 4); o += 4;
  }
  *o++ = 55; *o++ = 3; *o++ = 1; *o++ = 3; *o++ = 6;
  *o++ = 255;
  udp_enviar(0, 0xFFFFFFFFu, 68, 67, b, 300);
}
static int dhcp_pedir(void) {
  int vez;
  u32 t;
  for (vez = 0; vez < 4; vez++) {
    dh_xid = azar(); dh_tipo = 0;
    dhcp_enviar(1);
    for (t = ahora(); dh_tipo != 2 && ahora() - t < 2000u + 1000u * vez; ) cede();
    if (dh_tipo != 2) continue;
    dh_tipo = 3;
    dhcp_enviar(3);
    for (t = ahora(); dh_tipo == 3 && ahora() - t < 3000; ) cede();
    if (dh_tipo == 5) return 1;
  }
  return 0;
}
static int configurar(void) {
  int dhcp = 1;
  u32 ip = 0, masc = 0, gw = 0, dns = 0;
  u8 c[30];
  ST st;
  if (red.cfg && (red.cfg[0] & 1)) {
    mi_ip = ip_de(red.cfg + 2); mi_masc = ip_de(red.cfg + 6);
    mi_gw = ip_de(red.cfg + 10); mi_dns = ip_de(red.cfg + 14);
    return 1;
  }
  ini_leer(&dhcp, &ip, &masc, &gw, &dns);
  memset(c, 0, sizeof c);
  if (dhcp) {
    if (!dhcp_pedir()) return 0;
    mi_ip = dh_ip; mi_masc = dh_masc ? dh_masc : 0x00FFFFFF; mi_gw = dh_gw; mi_dns = dh_dns;
    c[0] = 3; memcpy(c + 18, &dh_srv, 4); memcpy(c + 22, &dh_dura, 4);
    c[22] = (u8)dh_dura; c[23] = (u8)(dh_dura >> 8); c[24] = (u8)(dh_dura >> 16); c[25] = (u8)(dh_dura >> 24);
  } else {
    if (!ip) return 0;
    mi_ip = ip; mi_masc = masc ? masc : 0x00FFFFFF; mi_gw = gw; mi_dns = 0;
    c[0] = 1;
  }
  if (dns) mi_dns = dns;
  memcpy(c + 2, &mi_ip, 4); memcpy(c + 6, &mi_masc, 4); memcpy(c + 10, &mi_gw, 4); memcpy(c + 14, &mi_dns, 4);
  GetLocalTime(&st);
  c[26] = (u8)st.dia; c[27] = (u8)st.mes; c[28] = (u8)st.anio; c[29] = (u8)(st.anio >> 8);
  if (red.cfg) memcpy(red.cfg, c, 30);         /* a HSPRED, para los demás */
  return 1;
}
/* abre la red la primera vez que hace falta: 0 si no se puede */
static int red_lista(void) {
  if (abierta) return mi_ip != 0;
  if (HispaRedAbrir(&red)) return 0;
  bios_tics = (volatile u32 *)(red.anillo - red.lineal + 0x46C);
  az_inicia(red.mac);
  p_sig = 49152 + azar() % 12000;
  abierta = 1;
  if (!configurar()) { abierta = 0; HispaRedCerrar(); return 0; }
  return 1;
}

/* ---- DNS ---- */
static struct { char nombre[64]; u32 ip; u32 t; } dns_mem[8];
static int dns_busca(const char *nombre, u32 *ip) {
  u8 q[300], *p = q + 12;
  const char *c = nombre;
  int vez, k, n = strlen(nombre), v = 0;
  u32 t;
  if (n < 1 || n > 250) return WSAHOST_NOT_FOUND;
  if (n < 64)
    for (k = 0; k < 8; k++) {
      if (dns_mem[k].ip && !memcmp(dns_mem[k].nombre, nombre, n + 1) && ahora() - dns_mem[k].t < 600000u) {
        *ip = dns_mem[k].ip;
        return 0;
      }
      if (dns_mem[k].t < dns_mem[v].t) v = k;
    }
  if (!mi_dns) return WSATRY_AGAIN;
  memset(q, 0, 12);
  q[2] = 1; q[5] = 1;                          /* con recursión, una pregunta */
  while (*c) {
    const char *f = c;
    while (*f && *f != '.') f++;
    if (f - c > 63 || f == c) return WSAHOST_NOT_FOUND;
    *p++ = (u8)(f - c);
    memcpy(p, c, f - c); p += f - c;
    c = *f ? f + 1 : f;
  }
  *p++ = 0; p16(p, 1); p16(p + 2, 1); p += 4;
  for (vez = 0; vez < 3; vez++) {
    /* (de dos llamadas distintas al generador) */
    dns_id = (u16)azar(); p16(q, dns_id);
    { u32 r = azar(); dns_puerto = (u16)(50000 + r % 10000); }
    dns_estado = 1;
    udp_enviar(mi_ip, mi_dns, dns_puerto, 53, q, p - q);
    for (t = ahora(); dns_estado == 1 && ahora() - t < 2500; ) cede();
    dns_puerto = 0;
    if (dns_estado == 2) {
      *ip = dns_ip;
      if (n < 64) { memcpy(dns_mem[v].nombre, nombre, n + 1); dns_mem[v].ip = dns_ip; dns_mem[v].t = ahora(); }
      return 0;
    }
    if (dns_estado == 3) return WSAHOST_NOT_FOUND;
  }
  return WSATRY_AGAIN;
}

/* =============================================================================
 *  Winsock 1.1
 * ============================================================================= */
static SOCK *sock(SOCKET h) {
  if (!iniciada) { ult_error = WSANOTINITIALISED; return NULL; }
  if (h < 1 || h > N_SOCK || !socks[h - 1].uso || socks[h - 1].huerfano) { ult_error = WSAENOTSOCK; return NULL; }
  sondear();
  return &socks[h - 1];
}
static int falla(int e) { ult_error = e; return SOCKET_ERROR; }

int WINAPI WSAStartup(u32 version, WSADATA *d) {
  (void)version;
  if (d) {
    memset(d, 0, sizeof *d);
    d->wVersion = 0x0101; d->wHighVersion = 0x0101;
    memcpy(d->szDescription, "Hispa-486: Winsock 1.1", 23);
    memcpy(d->szSystemStatus, "En marcha", 10);
    d->iMaxSockets = N_SOCK; d->iMaxUdpDg = 1472;
  }
  iniciada++;
  return 0;
}
int WINAPI WSACleanup(void) {
  int k;
  if (!iniciada) return falla(WSANOTINITIALISED);
  if (--iniciada) return 0;
  sondear();
  for (k = 0; k < N_SOCK; k++) {
    SOCK *s = &socks[k];
    if (s->uso == 1 && s->estado != E_CERRADO) tcp_rst(s->ip_rem, s->p_loc, s->p_rem, s->snd_nxt, 0, 0);
    if (s->uso) tcp_suelta(s);
  }
  if (abierta) { HispaRedCerrar(); abierta = 0; mi_ip = 0; pend_n = 0; }
  return 0;
}
int WINAPI WSAGetLastError(void) { return ult_error; }
void WINAPI WSASetLastError(int e) { ult_error = e; }
u16 WINAPI htons(u16 v) { return (u16)(v << 8 | v >> 8); }
u16 WINAPI ntohs(u16 v) { return (u16)(v << 8 | v >> 8); }
u32 WINAPI htonl(u32 v) { return v << 24 | (v & 0xFF00) << 8 | (v >> 8 & 0xFF00) | v >> 24; }
u32 WINAPI ntohl(u32 v) { return v << 24 | (v & 0xFF00) << 8 | (v >> 8 & 0xFF00) | v >> 24; }
u32 WINAPI inet_addr(const char *t) {
  u32 ip;
  return t && ip_texto(t, &ip) ? ip : 0xFFFFFFFFu;
}
char *WINAPI inet_ntoa(struct in_addr a) {
  static char b[16];
  const u8 *p = (const u8 *)&a;
  char *o = b;
  int k;
  for (k = 0; k < 4; k++) {
    if (p[k] >= 100) *o++ = (char)('0' + p[k] / 100);
    if (p[k] >= 10) *o++ = (char)('0' + p[k] / 10 % 10);
    *o++ = (char)('0' + p[k] % 10);
    *o++ = k < 3 ? '.' : 0;
  }
  return b;
}
SOCKET WINAPI socket(int familia, int tipo, int proto) {
  int k;
  SOCK *s;
  HANDLE m = GetProcessHeap();
  (void)proto;
  if (!iniciada) { ult_error = WSANOTINITIALISED; return INVALID_SOCKET; }
  if (familia != 2) { ult_error = WSAEAFNOSUPPORT; return INVALID_SOCKET; }
  if (tipo != 1 && tipo != 2) { ult_error = WSAEPROTONOSUPPORT; return INVALID_SOCKET; }
  if (!red_lista()) { ult_error = WSAENETDOWN; return INVALID_SOCKET; }
  sondear();
  for (k = 0; k < N_SOCK && socks[k].uso; k++) ;
  if (k == N_SOCK) { ult_error = WSAEMFILE; return INVALID_SOCKET; }
  s = &socks[k];
  memset(s, 0, sizeof *s);
  s->rx = HeapAlloc(m, 0, RXB);
  if (tipo == 1) s->tx = HeapAlloc(m, 0, TXB);
  if (!s->rx || (tipo == 1 && !s->tx)) { tcp_suelta(s); ult_error = WSAENOBUFS; return INVALID_SOCKET; }
  s->uso = (u8)tipo;
  s->mss = MSS;
  return k + 1;
}
static u16 puerto_nuevo(void) {
  for (;;) {
    int k;
    if (++p_sig > 65000) p_sig = 49152;
    for (k = 0; k < N_SOCK; k++) if (socks[k].uso && socks[k].p_loc == p_sig) break;
    if (k == N_SOCK) return (u16)p_sig;
  }
}
int WINAPI bind(SOCKET h, const struct sockaddr_in *a, int n) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (!a || n < 16) return falla(WSAEFAULT);
  s->p_loc = a->sin_port ? ntohs(a->sin_port) : puerto_nuevo();
  return 0;
}
int WINAPI connect(SOCKET h, const struct sockaddr_in *a, int n) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (!a || n < 16) return falla(WSAEFAULT);
  if (a->sin_family != 2) return falla(WSAEAFNOSUPPORT);
  if (s->uso == 2) {                           /* UDP: solo el destino */
    s->ip_rem = a->sin_addr.s_addr; s->p_rem = ntohs(a->sin_port);
    if (!s->p_loc) s->p_loc = puerto_nuevo();
    return 0;
  }
  if (s->estado == E_SYN) return falla(WSAEALREADY);
  if (s->estado == E_ESTAB) return falla(WSAEISCONN);
  if (s->estuvo || s->error) return falla(WSAEINVAL);
  s->ip_rem = a->sin_addr.s_addr; s->p_rem = ntohs(a->sin_port);
  if (!s->p_loc) s->p_loc = puerto_nuevo();
  s->snd_una = isn_de(mi_ip, s->p_loc, s->ip_rem, s->p_rem, ahora());
  s->rto = RTO0 * 2; s->reintentos = 0; s->t_rtx = 0;
  s->estado = E_SYN;
  tcp_salida(s, 0);
  if (s->nobloq) return falla(WSAEWOULDBLOCK);
  while (s->estado == E_SYN) cede();
  if (s->estado != E_ESTAB) return falla(s->error ? s->error : WSAECONNREFUSED);
  return 0;
}
int WINAPI send(SOCKET h, const char *b, int n, int f) {
  SOCK *s = sock(h);
  int hecho = 0;
  (void)f;
  if (!s) return SOCKET_ERROR;
  if (s->uso == 2) {
    if (!s->p_rem) return falla(WSAENOTCONN);
    if (n < 0 || n > 1472) return falla(WSAEMSGSIZE);
    udp_enviar(mi_ip, s->ip_rem, s->p_loc, s->p_rem, (const u8 *)b, n);
    return n;
  }
  if (n < 0) return falla(WSAEINVAL);
  for (;;) {
    u32 sitio, fin, k, q;
    if (s->error) return falla(s->error);
    if (s->estado != E_ESTAB) return falla(s->estuvo ? WSAECONNRESET : WSAENOTCONN);
    if (s->fin_pedido) return falla(WSAESHUTDOWN);
    sitio = TXB - s->tx_n;
    q = (u32)(n - hecho) < sitio ? (u32)(n - hecho) : sitio;
    if (q) {
      fin = s->tx_ini + s->tx_n; if (fin >= TXB) fin -= TXB;
      k = TXB - fin; if (k > q) k = q;
      memcpy(s->tx + fin, b + hecho, k);
      memcpy(s->tx, b + hecho + k, q - k);
      s->tx_n += q;
      hecho += q;
      tcp_salida(s, 0);
    }
    if (hecho == n) return n;
    if (s->nobloq) { if (hecho) return hecho; return falla(WSAEWOULDBLOCK); }
    cede();
  }
}
int WINAPI recv(SOCKET h, char *b, int n, int f) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (n < 0) return falla(WSAEINVAL);
  if (s->uso == 2) {
    for (;;) {
      if (s->rx_n) {
        u32 l = s->rx[0] | s->rx[1] << 8, q = l < (u32)n ? l : (u32)n;
        memcpy(b, s->rx + 8, q);
        if (!(f & 2)) { s->rx_n -= l + 8; memcpy(s->rx, s->rx + l + 8, s->rx_n); }
        if (l > (u32)n) return falla(WSAEMSGSIZE);
        return q;
      }
      if (s->nobloq) return falla(WSAEWOULDBLOCK);
      cede();
    }
  }
  for (;;) {
    if (s->rx_n) {
      u32 q = s->rx_n < (u32)n ? s->rx_n : (u32)n, k = RXB - s->rx_ini;
      if (k > q) k = q;
      memcpy(b, s->rx + s->rx_ini, k);
      memcpy(b + k, s->rx, q - k);
      if (!(f & 2)) {                          /* (MSG_PEEK: se queda) */
        s->rx_ini += q; if (s->rx_ini >= RXB) s->rx_ini -= RXB;
        s->rx_n -= q;
        /* la ventana se había quedado corta y ahora hay sitio: decirlo */
        if (s->estado == E_ESTAB && !s->fin_rx && ventana(s) >= s->vent_dicha + 2 * MSS) tcp_ack(s);
      }
      return q;
    }
    if (s->fin_rx) return 0;
    if (s->error) return falla(s->error);
    if (s->estado != E_ESTAB) return s->estuvo ? 0 : falla(WSAENOTCONN);
    if (s->nobloq) return falla(WSAEWOULDBLOCK);
    cede();
  }
}
int WINAPI sendto(SOCKET h, const char *b, int n, int f, const struct sockaddr_in *a, int na) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (s->uso != 2 || !a) return send(h, b, n, f);
  if (na < 16) return falla(WSAEFAULT);
  if (n < 0 || n > 1472) return falla(WSAEMSGSIZE);
  if (!s->p_loc) s->p_loc = puerto_nuevo();
  udp_enviar(mi_ip, a->sin_addr.s_addr, s->p_loc, ntohs(a->sin_port), (const u8 *)b, n);
  return n;
}
int WINAPI recvfrom(SOCKET h, char *b, int n, int f, struct sockaddr_in *a, int *na) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (s->uso == 2 && a && na && *na >= 16) {
    while (!s->rx_n) {
      if (s->nobloq) return falla(WSAEWOULDBLOCK);
      cede();
    }
    memset(a, 0, 16);
    a->sin_family = 2;
    memcpy(&a->sin_addr, s->rx + 2, 4);
    a->sin_port = htons((u16)(s->rx[6] | s->rx[7] << 8));
    *na = 16;
  }
  return recv(h, b, n, f);
}
int WINAPI shutdown(SOCKET h, int como) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (s->uso != 1) return 0;
  if (como == 0 || como == 2) { s->sin_leer = 1; s->rx_n = 0; }
  if ((como == 1 || como == 2) && s->estado == E_ESTAB && !s->fin_pedido) {
    s->fin_pedido = 1;
    tcp_salida(s, 0);
  }
  return 0;
}
int WINAPI closesocket(SOCKET h) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (s->uso == 2 || s->estado == E_CERRADO) { tcp_suelta(s); return 0; }
  if (s->estado == E_SYN) {
    tcp_rst(s->ip_rem, s->p_loc, s->p_rem, s->snd_nxt, 0, 0);
    tcp_suelta(s);
    return 0;
  }
  /* cierre ordenado: lo que queda por enviar y el FIN salen solos; la ficha
   * se suelta cuando el otro lado acaba, o a los 5 segundos */
  s->huerfano = 1; s->sin_leer = 1; s->rx_n = 0;
  s->t_muerte = ahora() + 5000;
  if (!s->fin_pedido) { s->fin_pedido = 1; tcp_salida(s, 0); }
  if (s->fin_rx && s->fin_acusado) tcp_suelta(s);
  return 0;
}
int WINAPI ioctlsocket(SOCKET h, u32 orden, u32 *arg) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (!arg) return falla(WSAEFAULT);
  if (orden == FIONBIO) { s->nobloq = *arg != 0; return 0; }
  if (orden == FIONREAD) {
    *arg = s->uso == 2 ? (s->rx_n ? (u32)(s->rx[0] | s->rx[1] << 8) : 0) : s->rx_n;
    return 0;
  }
  return falla(WSAEINVAL);
}
int WINAPI setsockopt(SOCKET h, int nivel, int cual, const char *v, int n) {
  (void)nivel; (void)cual; (void)v; (void)n;
  return sock(h) ? 0 : SOCKET_ERROR;
}
int WINAPI getsockopt(SOCKET h, int nivel, int cual, char *v, int *n) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (!v || !n || *n < 4) return falla(WSAEFAULT);
  if (nivel == 0xFFFF && cual == 0x1007) {     /* SO_ERROR: lo da y lo borra */
    memcpy(v, &s->error, 4); *n = 4;
    return 0;
  }
  memset(v, 0, 4); *n = 4;
  return 0;
}
static int nombre_de(SOCK *s, struct sockaddr_in *a, int *n, int remoto) {
  if (!a || !n || *n < 16) return falla(WSAEFAULT);
  memset(a, 0, 16);
  a->sin_family = 2;
  a->sin_addr.s_addr = remoto ? s->ip_rem : mi_ip;
  a->sin_port = htons(remoto ? s->p_rem : s->p_loc);
  *n = 16;
  return 0;
}
int WINAPI getsockname(SOCKET h, struct sockaddr_in *a, int *n) {
  SOCK *s = sock(h);
  return s ? nombre_de(s, a, n, 0) : SOCKET_ERROR;
}
int WINAPI getpeername(SOCKET h, struct sockaddr_in *a, int *n) {
  SOCK *s = sock(h);
  if (!s) return SOCKET_ERROR;
  if (s->uso == 1 && s->estado != E_ESTAB) return falla(WSAENOTCONN);
  return nombre_de(s, a, n, 1);
}
int WINAPI listen(SOCKET h, int n) { (void)n; return sock(h) ? falla(WSAEOPNOTSUPP) : SOCKET_ERROR; }
SOCKET WINAPI accept(SOCKET h, struct sockaddr_in *a, int *n) {
  (void)a; (void)n;
  if (sock(h)) ult_error = WSAEOPNOTSUPP;
  return INVALID_SOCKET;
}
/* ¿se puede leer, escribir, o falló la conexión? */
static int listo(SOCKET h, int que) {
  SOCK *s;
  if (h < 1 || h > N_SOCK || !socks[h - 1].uso || socks[h - 1].huerfano) return 0;
  s = &socks[h - 1];
  if (s->uso == 2) return que == 0 ? s->rx_n != 0 : que == 1;
  if (que == 0) return s->rx_n || s->fin_rx || s->error || (s->estado == E_CERRADO && s->estuvo);
  if (que == 1) return s->estado == E_ESTAB && s->tx_n < TXB;
  return s->estado == E_CERRADO && !s->estuvo && s->error;
}
int WINAPI select(int nada, fd_set *l, fd_set *e, fd_set *x, const struct timeval *plazo) {
  fd_set *c[3];
  u32 t, ms = 0;
  (void)nada;
  if (!iniciada) return falla(WSANOTINITIALISED);
  c[0] = l; c[1] = e; c[2] = x;
  if (plazo) ms = (u32)plazo->tv_sec * 1000 + (u32)plazo->tv_usec / 1000;
  sondear();
  for (t = ahora();;) {
    int total = 0, q;
    u32 k;
    for (q = 0; q < 3; q++)
      for (k = 0; c[q] && k < c[q]->fd_count && k < 64; k++) total += listo(c[q]->fd_array[k], q);
    if (total || (plazo && ahora() - t >= ms)) {
      for (q = 0; q < 3; q++) {
        u32 n = 0;
        for (k = 0; c[q] && k < c[q]->fd_count && k < 64; k++)
          if (listo(c[q]->fd_array[k], q)) c[q]->fd_array[n++] = c[q]->fd_array[k];
        if (c[q]) c[q]->fd_count = n;
      }
      return total;
    }
    cede();
  }
}
int WINAPI __WSAFDIsSet(SOCKET h, fd_set *c) {
  u32 k;
  for (k = 0; c && k < c->fd_count && k < 64; k++) if (c->fd_array[k] == h) return 1;
  return 0;
}
struct hostent *WINAPI gethostbyname(const char *nombre) {
  static struct hostent he;
  static char nom[256];
  static char *lista[2], *sin[1];
  static u32 ip;
  int n, e;
  if (!iniciada) { ult_error = WSANOTINITIALISED; return NULL; }
  if (!nombre || (n = strlen(nombre)) > 255) { ult_error = WSAHOST_NOT_FOUND; return NULL; }
  if (!ip_texto(nombre, &ip)) {
    if (!red_lista()) { ult_error = WSAENETDOWN; return NULL; }
    sondear();
    if ((e = dns_busca(nombre, &ip))) { ult_error = e; return NULL; }
  }
  memcpy(nom, nombre, n + 1);
  lista[0] = (char *)&ip; lista[1] = NULL; sin[0] = NULL;
  he.h_name = nom; he.h_aliases = sin; he.h_addrtype = 2; he.h_length = 4; he.h_addr_list = lista;
  return &he;
}
int WINAPI gethostname(char *b, int n) {
  if (!b || n < 10) return falla(WSAEFAULT);
  memcpy(b, "hispa-486", 10);
  return 0;
}
/* Nuestra (no es de Winsock): la configuración IP en uso, para enseñarla.
 * c: 16 octetos (IP, máscara, puerta y DNS, en el orden de la red) y mac: 6.
 * Abre la red si hace falta; devuelve 0 si no hay red. */
int WINAPI HispaRedConfig(u8 *c, u8 *mac) {
  if (!red_lista()) return 0;
  if (c) { memcpy(c, &mi_ip, 4); memcpy(c + 4, &mi_masc, 4); memcpy(c + 8, &mi_gw, 4); memcpy(c + 12, &mi_dns, 4); }
  if (mac) memcpy(mac, red.mac, 6);
  return 1;
}
/* al descargarse la DLL (el programa acaba): soltar la red */
int WINAPI DllMain(HANDLE h, u32 motivo, void *r) {
  (void)h; (void)r;
  if (motivo == 0 && abierta) { iniciada = 1; WSACleanup(); }
  return 1;
}
