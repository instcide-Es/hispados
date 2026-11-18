/* =============================================================================
 *  HSPJS.DLL de Hispa-486: el motor de JavaScript de los programas Hispa32.
 *  Es QuickJS (Fabrice Bellard y Charlie Gordon, licencia MIT; va tal cual en
 *  terceros/quickjs, con su LEEME) más lo que QuickJS pide a la biblioteca de
 *  C y MSVCRT.DLL no tiene, o no como hace falta:
 *    - la memoria (un repartidor propio, rápido para los trozos pequeños que
 *      pide QuickJS a miles y que sabe cuánto mide cada trozo);
 *    - las divisiones de 64 bits (lo que en gcc pone libgcc);
 *    - la salida (fprintf, fputc...: a donde diga el programa, HjSalida);
 *    - la hora (gettimeofday, localtime_r) y la zona (HjZona);
 *    - las cuentas de Math, en mates.c (con el x87).
 *  Se exporta la interfaz de QuickJS (quickjs.h: JS_...) y unas pocas Hj...
 *  docs/486/NAVEGADOR.md, «JavaScript».
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

typedef void *HANDLE;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long s64;
#define WINAPI __attribute__((stdcall))

typedef struct { u32 lo, hi; } FT;
HANDLE WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(HANDLE, u32, u32);
int WINAPI HeapFree(HANDLE, u32, void *);
void WINAPI GetSystemTimeAsFileTime(FT *);
void WINAPI ExitProcess(u32);
HANDLE WINAPI GetStdHandle(int);
int WINAPI WriteFile(HANDLE, const void *, u32, u32 *, void *);

/* =============================================================================
 *  Las divisiones de 64 bits (gcc las pide a libgcc: __udivdi3...)
 * ============================================================================= */
u64 __udivmoddi4(u64 n, u64 d, u64 *resto) {
  u64 q = 0;
  int k;
  if (d == 0) { if (resto) *resto = 0; return 0; }   /* (no pasa: QuickJS lo mira antes) */
  if (!(d >> 32)) {
    /* el divisor cabe en 32 bits: dos «divl» */
    u32 dv = (u32)d, ah = (u32)(n >> 32), al = (u32)n, qh, ql, r;
    qh = ah / dv;
    r = ah % dv;
    __asm__("divl %4" : "=a"(ql), "=d"(r) : "0"(al), "1"(r), "rm"(dv));
    if (resto) *resto = r;
    return (u64)qh << 32 | ql;
  }
  if (n < d) { if (resto) *resto = n; return 0; }
  /* restar y desplazar, con el divisor alineado con el dividendo */
  k = __builtin_clzll(d) - __builtin_clzll(n);
  d <<= k;
  for (; k >= 0; k--, d >>= 1) {
    q <<= 1;
    if (n >= d) { n -= d; q |= 1; }
  }
  if (resto) *resto = n;
  return q;
}
u64 __udivdi3(u64 n, u64 d) { return __udivmoddi4(n, d, NULL); }
u64 __umoddi3(u64 n, u64 d) { u64 r; __udivmoddi4(n, d, &r); return r; }
s64 __divdi3(s64 n, s64 d) {
  int neg = (n < 0) ^ (d < 0);
  u64 q = __udivmoddi4(n < 0 ? -(u64)n : (u64)n, d < 0 ? -(u64)d : (u64)d, NULL);
  return neg ? -(s64)q : (s64)q;
}
s64 __moddi3(s64 n, s64 d) {
  u64 r;
  __udivmoddi4(n < 0 ? -(u64)n : (u64)n, d < 0 ? -(u64)d : (u64)d, &r);
  return n < 0 ? -(s64)r : (s64)r;
}
s64 __divmoddi4(s64 n, s64 d, s64 *resto) {
  s64 q = __divdi3(n, d);
  if (resto) *resto = n - q * d;
  return q;
}
int __popcountsi2(u32 x) {
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  return (int)((((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24);
}
int __popcountdi2(u64 x) { return __popcountsi2((u32)x) + __popcountsi2((u32)(x >> 32)); }

/* =============================================================================
 *  La memoria. QuickJS pide muchísimos trozos pequeños (objetos, textos,
 *  formas): los de hasta 1 KB salen de bloques de 64 KB, por clases de 8 en
 *  8 octetos, cada clase con su lista de sueltos; delante de cada trozo, 4
 *  octetos con su clase. Los grandes van al montón de KERNEL32 con 8 octetos
 *  delante: el tamaño y un 0.
 * ============================================================================= */
#define PEQ_MAX 1024
#define BLOQUE (64 * 1024)
static void *libres[PEQ_MAX / 8 + 1];
static char *bump, *bump_fin;
static u32 en_uso, pedido_sistema, pico;
static HANDLE monton;

static void *del_sistema(u32 n) {
  void *p;
  if (!monton) monton = GetProcessHeap();
  p = HeapAlloc(monton, 0, n);
  if (p) pedido_sistema += n;
  return p;
}
void *malloc(size_t n) {
  u32 total, k;
  u32 *p;
  if (n == 0) n = 1;
  total = (n + 4 + 7) & ~7u;
  if (total <= PEQ_MAX) {
    k = total >> 3;
    if (libres[k]) {
      p = libres[k];
      libres[k] = *(void **)p;
    } else {
      if (bump_fin - bump < (int)total) {
        bump = del_sistema(BLOQUE);
        if (!bump) { bump_fin = NULL; return NULL; }
        bump_fin = bump + BLOQUE;
      }
      p = (u32 *)bump;
      bump += total;
      *p++ = k;
    }
    en_uso += total;
  } else {
    if (n > 0x7FFFFFF0u) return NULL;
    p = del_sistema(n + 8);
    if (!p) return NULL;
    p[0] = n; p[1] = 0;
    p += 2;
    en_uso += n + 8;
  }
  if (en_uso > pico) pico = en_uso;
  return p;
}
size_t malloc_usable_size(void *v) {
  u32 *p = v;
  if (!p) return 0;
  return p[-1] ? (p[-1] << 3) - 4 : p[-2];
}
void free(void *v) {
  u32 *p = v, k;
  if (!p) return;
  k = p[-1];
  if (k) {
    en_uso -= k << 3;
    *(void **)p = libres[k];
    libres[k] = p;
  } else {
    en_uso -= p[-2] + 8;
    pedido_sistema -= p[-2] + 8;
    HeapFree(monton, 0, p - 2);
  }
}
void *calloc(size_t a, size_t b) {
  u32 n = a * b;
  void *p;
  if (b && n / b != a) return NULL;
  p = malloc(n);
  if (p) memset(p, 0, n);
  return p;
}
void *realloc(void *v, size_t n) {
  size_t u;
  void *q;
  if (!v) return malloc(n);
  if (!n) { free(v); return NULL; }
  u = malloc_usable_size(v);
  if (n <= u && (((u32 *)v)[-1] == 0 ? n > u / 2 : 1)) return v;
  q = malloc(n);
  if (!q) return NULL;
  memcpy(q, v, u < n ? u : n);
  free(v);
  return q;
}
char *strdup(const char *t) {
  u32 n = strlen(t) + 1;
  char *p = malloc(n);
  if (p) memcpy(p, t, n);
  return p;
}
/* lo que hay en uso, lo más que ha habido y lo pedido al sistema (octetos) */
void HjMemoria(u32 *uso, u32 *maximo, u32 *sistema) {
  if (uso) *uso = en_uso;
  if (maximo) *maximo = pico;
  if (sistema) *sistema = pedido_sistema;
}

/* =============================================================================
 *  La salida: lo que QuickJS escribe (volcados, avisos) va a HjSalida; si no
 *  se ha dicho nada, a la consola.
 * ============================================================================= */
struct HJ_FILE { int fd; };
static struct HJ_FILE f_out = { 1 }, f_err = { 2 };
FILE *hj_stdout = &f_out, *hj_stderr = &f_err;
static void (*salida)(int fd, const char *t, int n);
void HjSalida(void (*f)(int, const char *, int)) { salida = f; }
static void escribe(FILE *f, const char *t, int n) {
  u32 h;
  if (n <= 0) return;
  if (salida) { salida(f->fd, t, n); return; }
  WriteFile(GetStdHandle(f->fd == 2 ? -12 : -11), t, n, &h, NULL);
}
int vfprintf(FILE *f, const char *fmt, va_list ap) {
  char b[1024];
  int n = vsnprintf(b, sizeof b, fmt, ap);
  escribe(f, b, n < (int)sizeof b ? n : (int)sizeof b - 1);
  return n;
}
int fprintf(FILE *f, const char *fmt, ...) {
  va_list ap;
  int n;
  va_start(ap, fmt);
  n = vfprintf(f, fmt, ap);
  va_end(ap);
  return n;
}
int fputc(int c, FILE *f) { char x = (char)c; escribe(f, &x, 1); return c & 255; }
int fputs(const char *t, FILE *f) { escribe(f, t, strlen(t)); return 0; }
size_t fwrite(const void *p, size_t a, size_t b, FILE *f) { escribe(f, p, a * b); return b; }
int fflush(FILE *f) { (void)f; return 0; }

/* lo que no debería pasar: se dice y se acaba el programa */
static void (*al_caer)(const char *);
void HjAlCaer(void (*f)(const char *)) { al_caer = f; }
void abort(void) {
  if (al_caer) al_caer("QuickJS: abort()");
  fputs("QuickJS: abort()\n", stderr);
  ExitProcess(3);
  for (;;) ;
}
void hj_assert(const char *que, const char *archivo, int linea) {
  char b[300];
  snprintf(b, sizeof b, "QuickJS: falla la comprobacion %.120s (%.60s:%d)", que, archivo, linea);
  if (al_caer) al_caer(b);
  fprintf(stderr, "%s\n", b);
  ExitProcess(3);
  for (;;) ;
}

/* =============================================================================
 *  La hora. El reloj de un equipo con DOS va en la hora de aquí, sin zona:
 *  por omisión se toma como si fuera la universal (zona 0), y así Date
 *  enseña la hora del reloj. HjZona(minutos al este) la cambia.
 * ============================================================================= */
static int zona_min;
void HjZona(int minutos) { zona_min = minutos; }
int gettimeofday(struct timeval *tv, void *tz) {
  FT ft;
  u64 t;
  (void)tz;
  GetSystemTimeAsFileTime(&ft);
  t = ((u64)ft.hi << 32 | ft.lo) / 10;          /* microsegundos desde 1601 */
  t -= 11644473600000000ULL;                    /* desde 1970 */
  t -= (s64)zona_min * 60000000LL;
  tv->tv_sec = (time_t)(t / 1000000);
  tv->tv_usec = (long)(t % 1000000);
  return 0;
}
time_t time(time_t *p) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (p) *p = tv.tv_sec;
  return tv.tv_sec;
}
struct tm *localtime_r(const time_t *t, struct tm *tm) {
  s64 s = *t + (s64)zona_min * 60, dias, seg;
  int a, m, d;
  static const int ac[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  dias = s / 86400; seg = s % 86400;
  if (seg < 0) { seg += 86400; dias--; }
  tm->tm_hour = (int)(seg / 3600); tm->tm_min = (int)(seg / 60 % 60); tm->tm_sec = (int)(seg % 60);
  tm->tm_wday = (int)((dias % 7 + 11) % 7);     /* el 1/1/1970 fue jueves */
  /* los días a año, mes y día (calendario gregoriano proléptico) */
  {
    s64 z = dias + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    u32 doe = (u32)(z - era * 146097), yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    u32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    a = (int)(yoe + era * 400 + (m <= 2));
  }
  tm->tm_year = a - 1900; tm->tm_mon = m - 1; tm->tm_mday = d;
  tm->tm_yday = ac[m - 1] + d - 1 + (m > 2 && ((a % 4 == 0 && a % 100) || a % 400 == 0));
  tm->tm_isdst = 0;
  tm->tm_gmtoff = zona_min * 60;
  tm->tm_zone = "";
  return tm;
}

/* =============================================================================
 *  La entrada de la DLL: el x87 a doble precisión (53 bits), como en Windows,
 *  para que las sumas y productos redondeen como manda JavaScript.
 * ============================================================================= */
void hj_fpu_doble(void) {
  unsigned short cw;
  __asm__ volatile("fnstcw %0" : "=m"(cw));
  cw = (unsigned short)((cw & ~0x0300) | 0x0200);
  __asm__ volatile("fldcw %0" : : "m"(cw));
}
int WINAPI DllMain(HANDLE h, u32 razon, void *x) {
  (void)h; (void)x;
  if (razon == 1) hj_fpu_doble();
  return 1;
}
