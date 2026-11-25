/* tests/red32/azar.c - el azar y el DNS de WSOCK32.DLL (w32/wsock32.c, el
 * código tal cual, incluido aquí con WSOCK_ANFITRION) en el anfitrión, con
 * gcc: ChaCha20 contra el ejemplo del RFC 8439, que lo que sale del
 * generador no deja saber lo siguiente (como el xorshift de antes), el
 * número inicial de TCP del RFC 6528 y que una respuesta de DNS solo vale si
 * viene del servidor al que se preguntó, desde el 53. Lo corre
 * tests/run_red32_azar_tests.sh.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#define WSOCK_ANFITRION 1
#include "../../w32/wsock32.c"
int printf(const char *, ...);
void *malloc(unsigned long);
void free(void *);

/* lo que pide WSOCK32 a KERNEL32 (HW32), de mentira */
static u32 reloj = 1000;
int WINAPI HispaRedAbrir(HRED *r) { (void)r; return 1; }
int WINAPI HispaRedEnviar(const void *t, u32 n) { (void)t; (void)n; return 0; }
int WINAPI HispaRedCerrar(void) { return 0; }
void WINAPI Sleep(u32 ms) { (void)ms; }
void WINAPI GetLocalTime(ST *st) { memset(st, 0, sizeof *st); st->anio = 2026; st->mes = 11; st->dia = 25; st->ms = (u16)reloj; }
u32 WINAPI GetEnvironmentVariableA(const char *n, char *b, u32 t) { (void)n; (void)b; (void)t; return 0; }
HANDLE WINAPI CreateFileA(const char *n, u32 a, u32 b, void *c, u32 d, u32 e, HANDLE f) { (void)n; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; return (HANDLE)-1; }
int WINAPI ReadFile(HANDLE h, void *b, u32 n, u32 *l, void *o) { (void)h; (void)b; (void)n; (void)l; (void)o; return 0; }
int WINAPI CloseHandle(HANDLE h) { (void)h; return 1; }
HANDLE WINAPI GetProcessHeap(void) { return (HANDLE)1; }
void *WINAPI HeapAlloc(HANDLE h, u32 f, u32 n) { (void)h; (void)f; return malloc(n); }
int WINAPI HeapFree(HANDLE h, u32 f, void *p) { (void)h; (void)f; free(p); return 1; }
u32 WINAPI GetModuleFileNameA(HANDLE h, char *b, u32 n) { (void)h; (void)n; memcpy(b, "C:\\PRUEBA.EXE", 14); return 13; }
u32 WINAPI GetTickCount(void) { return reloj; }

static int fallos;
static void mira(int si, const char *que) { printf("  %s %s\n", si ? "OK  " : "FAIL", que); if (!si) fallos = 1; }
static u32 xorshift(u32 x) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }

/* una respuesta de DNS (A 1.2.3.4) para id, de de:pde a dns_puerto */
static int respuesta(u8 *u, u16 id, u16 pde) {
  static const u8 dns[] = { 0, 0, 0x81, 0x80, 0, 1, 0, 1, 0, 0, 0, 0,
    4, 'b', 'a', 'n', 'k', 2, 'e', 's', 0, 0, 1, 0, 1,
    0xC0, 12, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 1, 2, 3, 4 };
  int n = 8 + sizeof dns;
  p16(u, pde); p16(u + 2, dns_puerto); p16(u + 4, n); p16(u + 6, 0);   /* (sin suma) */
  memcpy(u + 8, dns, sizeof dns);
  p16(u + 8, id);
  return n;
}

int main(void) {
  /* 1. ChaCha20: RFC 8439, 2.3.2 */
  {
    u32 k[8], nonce[3] = { 0x09000000, 0x4a000000, 0 }, b[16], i;
    static const u32 bien[16] = { 0xe4e7f110, 0x15593bd1, 0x1fdd0f50, 0xc47120a3, 0xc7f4d1c7, 0x0368c033, 0x9aaa2204, 0x4e6cd4c3,
                                  0x466482d2, 0x09aa9f07, 0x05d7c214, 0xa2028bd9, 0xd19c12b5, 0xb94e16de, 0xe883d0cb, 0x4e3c50a2 };
    for (i = 0; i < 8; i++) k[i] = (4 * i) | (4 * i + 1) << 8 | (4 * i + 2) << 16 | (u32)(4 * i + 3) << 24;
    chacha_bloque(k, 1, nonce, b);
    mira(!memcmp(b, bien, sizeof b), "ChaCha20: el bloque del RFC 8439 (2.3.2)");
  }
  /* 2. el generador */
  {
    u32 a[2000], k, iguales = 0, xs = 0, b0;
    az_inicia((const u8 *)"\x52\x54\x00\x12\x34\x56");
    for (k = 0; k < 2000; k++) a[k] = azar();
    for (k = 1; k < 2000; k++) { if (a[k] == a[k - 1]) iguales++; if (a[k] == xorshift(a[k - 1])) xs++; }
    mira(!iguales && !xs, "azar: lo siguiente no sale de lo anterior (ni como el xorshift de antes)");
    for (k = 0, b0 = 0; k < 2000; k++) b0 += __builtin_popcount(a[k]);
    mira(b0 > 2000 * 16 - 1500 && b0 < 2000 * 16 + 1500, "azar: la mitad de los bits a 1 (más o menos)");
    reloj = 1001;
    memset(az_clave, 0, sizeof az_clave);
    az_listo = 0;
    az_inicia((const u8 *)"\x52\x54\x00\x12\x34\x56");
    mira(azar() != a[0], "azar: otra semilla (otra hora), otra serie");
  }
  /* 3. el número inicial de TCP (RFC 6528) */
  {
    u32 i1 = isn_de(0x0F02000A, 50000, 0x0202000A, 443, 1000), i2 = isn_de(0x0F02000A, 50000, 0x0202000A, 443, 1001);
    u32 i3 = isn_de(0x0F02000A, 50001, 0x0202000A, 443, 1000), k, sale = 0;
    mira(i2 - i1 == 250, "ISN: la misma conexión, un milisegundo después: 250 más (4 µs)");
    mira(i3 != i1 && i3 - i1 != 1 && i3 - i1 != 250, "ISN: con otro puerto, otro sin relación");
    for (k = 0; k < 64; k++) if (azar() == i1) sale = 1;
    mira(!sale, "ISN: no es lo que da el generador");
    for (k = 0; k < 8 && isn_clave[k] == 0; k++) ;
    mira(k < 8, "ISN: con su clave");
  }
  /* 4. el DNS: solo del servidor al que se preguntó, desde el 53 */
  {
    u8 u[200];
    int n;
    mi_dns = 0x0302000A; mi_ip = 0x0F02000A;     /* 10.0.2.3 y 10.0.2.15 */
    dns_id = 0x1234; dns_puerto = 50123; dns_estado = 1; dns_ip = 0;
    n = respuesta(u, 0x1234, 53);
    udp_in(0x0101A8C0, mi_ip, u, n);             /* de 192.168.1.1: otro */
    mira(dns_estado == 1 && !dns_ip, "DNS: la respuesta de otra dirección no vale");
    n = respuesta(u, 0x1234, 5353);
    udp_in(mi_dns, mi_ip, u, n);
    mira(dns_estado == 1, "DNS: desde otro puerto que el 53 no vale");
    n = respuesta(u, 0x1235, 53);
    udp_in(mi_dns, mi_ip, u, n);
    mira(dns_estado == 1, "DNS: con otro número de pregunta no vale");
    n = respuesta(u, 0x1234, 53);
    udp_in(mi_dns, mi_ip, u, n);
    mira(dns_estado == 2 && dns_ip == 0x04030201, "DNS: la del servidor, desde el 53: 1.2.3.4");
  }
  printf(fallos ? "  HAY FALLOS\n" : "  todo bien\n");
  return fallos;
}
