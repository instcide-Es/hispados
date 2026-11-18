/* =============================================================================
 *  mates.c - las funciones de <math.h> que pide QuickJS (Math.sin, Math.pow,
 *  Math.round...), para HSPJS.DLL de Hispa-486. Con las instrucciones del
 *  x87 (fsin, fyl2x, f2xm1, fpatan...), que hace las cuentas por dentro con
 *  64 bits de mantisa: así basta una línea para cada una y sale bien
 *  redondeado a doble casi siempre. Los casos raros (infinitos, NaN, ceros con
 *  signo) se miran antes, como dice la norma de C (anexo F).
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <math.h>
#include <stdint.h>

typedef long double LD;

/* las cuentas de dentro, con los 64 bits del x87 (la DLL pone el x87 a 53
 * bits para las sumas de JavaScript: aquí se sube un momento y se vuelve) */
static inline unsigned short pc64(void) {
  unsigned short cw, n;
  __asm__ volatile("fnstcw %0" : "=m"(cw));
  n = cw | 0x0300;
  __asm__ volatile("fldcw %0" : : "m"(n));
  return cw;
}
static inline void pc_vuelve(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

static inline uint64_t bits(double x) { union { double d; uint64_t u; } v; v.d = x; return v.u; }
static inline double de_bits(uint64_t u) { union { double d; uint64_t u; } v; v.u = u; return v.d; }

double fabs(double x) { return __builtin_fabs(x); }
double copysign(double x, double y) { return __builtin_copysign(x, y); }

/* ---- redondeos (con los bits: exactos) ---- */
double trunc(double x) {
  uint64_t u = bits(x);
  int e = (int)(u >> 52 & 0x7FF) - 1023;
  if (e >= 52) return x;                         /* ya es entero (o inf, NaN) */
  if (e < 0) return de_bits(u & 0x8000000000000000ULL);
  return de_bits(u & ~(0x000FFFFFFFFFFFFFULL >> e));
}
double floor(double x) {
  double t = trunc(x);
  return (x < t) ? t - 1.0 : t;
}
double ceil(double x) {
  double t = trunc(x);
  if (x > t) return t + 1.0;
  return t;
}
double round(double x) {                         /* la mitad, lejos del cero */
  double t = trunc(x), d = x - t;
  if (d >= 0.5) return t + 1.0;
  if (d <= -0.5) return t - 1.0;
  return t;
}
double rint(double x) {
  LD r;
  if (!isfinite(x)) return x;
  __asm__("frndint" : "=t"(r) : "0"((LD)x));
  return copysign((double)r, x);
}
double nearbyint(double x) { return rint(x); }
long lrint(double x) {
  long r;
  __asm__("fistpl %0" : "=m"(r) : "t"((LD)x) : "st");
  return r;
}
long long llrint(double x) {
  long long r;
  __asm__("fistpll %0" : "=m"(r) : "t"((LD)x) : "st");
  return r;
}
double fmin(double a, double b) {
  if (isnan(a)) return b;
  if (isnan(b)) return a;
  if (a == b) return signbit(a) ? a : b;
  return a < b ? a : b;
}
double fmax(double a, double b) {
  if (isnan(a)) return b;
  if (isnan(b)) return a;
  if (a == b) return signbit(a) ? b : a;
  return a > b ? a : b;
}
double fmod(double x, double y) {
  LD r = x;
  unsigned short sw;
  if (isnan(x) || isnan(y) || isinf(x) || y == 0) return NAN;
  if (isinf(y)) return x;
  do {
    __asm__("fprem; fnstsw %%ax" : "=t"(r), "=a"(sw) : "0"(r), "u"((LD)y));
  } while (sw & 0x400);
  return copysign((double)r, x);
}
double scalbn(double x, int n) {
  LD r;
  __asm__("fscale" : "=t"(r) : "0"((LD)x), "u"((LD)n));
  return (double)r;
}
double ldexp(double x, int n) { return scalbn(x, n); }
double frexp(double x, int *e) {
  uint64_t u = bits(x);
  int ex = (int)(u >> 52 & 0x7FF);
  if (!ex) {                                     /* cero o subnormal */
    if (x == 0) { *e = 0; return x; }
    x *= 18446744073709551616.0;                 /* 2^64 */
    u = bits(x); ex = (int)(u >> 52 & 0x7FF) - 64;
  }
  if (ex == 0x7FF) { *e = 0; return x; }
  *e = ex - 1022;
  return de_bits((u & 0x800FFFFFFFFFFFFFULL) | 0x3FE0000000000000ULL);
}
double modf(double x, double *ent) {
  double t = trunc(x);
  *ent = t;
  if (isinf(x)) return copysign(0.0, x);
  return copysign(x - t, x);
}

/* ---- raíces ---- */
double sqrt(double x) {
  double r;
  __asm__("fsqrt" : "=t"(r) : "0"(x));
  return r;
}
double hypot(double x, double y) {
  unsigned short cw;
  LD r;
  if (isinf(x) || isinf(y)) return INFINITY;
  if (isnan(x) || isnan(y)) return NAN;
  cw = pc64();
  r = (LD)x * x + (LD)y * y;                    /* (el x87 no se desborda: 15 bits de exponente) */
  __asm__("fsqrt" : "=t"(r) : "0"(r));
  pc_vuelve(cw);
  return (double)r;
}

/* ---- exponenciales y logaritmos ---- */
/* 2^x con x finito, en 64 bits */
static LD dos_a(LD x) {
  LD i, f, r;
  __asm__("frndint" : "=t"(i) : "0"(x));
  f = x - i;
  __asm__("f2xm1" : "=t"(r) : "0"(f));
  r += 1.0L;
  __asm__("fscale" : "=t"(r) : "0"(r), "u"(i));
  return r;
}
#define LOG2E 1.44269504088896340735992468100189214L
#define LN2 0.693147180559945309417232121458176568L
#define LG2 0.301029995663981195213738894724493027L
double exp(double x) {
  unsigned short cw;
  double r;
  if (isnan(x)) return x;
  if (isinf(x)) return x > 0 ? x : 0.0;
  if (x > 710) return INFINITY;
  if (x < -746) return 0.0;
  cw = pc64();
  r = (double)dos_a(x * LOG2E);
  pc_vuelve(cw);
  return r;
}
double exp2(double x) {
  unsigned short cw;
  double r;
  if (isnan(x)) return x;
  if (isinf(x)) return x > 0 ? x : 0.0;
  if (x > 1025) return INFINITY;
  if (x < -1080) return 0.0;
  cw = pc64();
  r = (double)dos_a(x);
  pc_vuelve(cw);
  return r;
}
double expm1(double x) {
  unsigned short cw;
  LD y, r;
  if (isnan(x) || x == 0) return x;
  if (isinf(x)) return x > 0 ? x : -1.0;
  if (x > 710) return INFINITY;
  if (x < -50) return -1.0;
  cw = pc64();
  y = x * LOG2E;
  if (y > -1 && y < 1) __asm__("f2xm1" : "=t"(r) : "0"(y));   /* exacto cerca del cero */
  else r = dos_a(y) - 1.0L;
  pc_vuelve(cw);
  return (double)r;
}
static double lg(double x, LD k) {               /* k * log2(x) */
  LD r;
  __asm__("fyl2x" : "=t"(r) : "0"((LD)x), "u"(k) : "st(1)");
  return (double)r;
}
double log(double x) { return lg(x, LN2); }
double log2(double x) { return lg(x, 1.0L); }
double log10(double x) { return lg(x, LG2); }
double log1p(double x) {
  LD r;
  if (isnan(x) || x == 0) return x;
  if (x > -0.29 && x < 0.29) {
    __asm__("fyl2xp1" : "=t"(r) : "0"((LD)x), "u"(LN2) : "st(1)");
    return (double)r;
  }
  {
    unsigned short cw = pc64();
    LD u = 1.0L + x;
    __asm__("fyl2x" : "=t"(r) : "0"(u), "u"(LN2) : "st(1)");
    pc_vuelve(cw);
  }
  return (double)r;
}
/* ¿y es un entero impar? (|y| < 2^53) */
static int impar(double y) {
  double m;
  if (fabs(y) >= 9007199254740992.0) return 0;
  m = fmod(y, 2.0);
  return m == 1.0 || m == -1.0;
}
static int entero(double y) { return isfinite(y) && trunc(y) == y; }
double pow(double x, double y) {
  unsigned short cw;
  LD l, r;
  int neg = 0;
  if (y == 0) return 1.0;
  if (x == 1.0) return 1.0;
  if (isnan(x) || isnan(y)) return NAN;
  if (isinf(y)) {
    double ax = fabs(x);
    if (ax == 1.0) return 1.0;
    return (ax > 1.0) == (y > 0) ? INFINITY : 0.0;
  }
  if (isinf(x)) {
    if (x > 0) return y > 0 ? INFINITY : 0.0;
    if (impar(y)) return y > 0 ? -INFINITY : -0.0;
    return y > 0 ? INFINITY : 0.0;
  }
  if (x == 0) {
    if (impar(y)) return y < 0 ? copysign(INFINITY, x) : x;
    return y < 0 ? INFINITY : 0.0;
  }
  if (x < 0) {
    if (!entero(y)) return NAN;
    neg = impar(y);
    x = -x;
  }
  if (y == 1.0) return neg ? -x : x;
  cw = pc64();
  if (entero(y) && fabs(y) <= 64) {
    /* potencia entera pequeña: multiplicando (exacta cuando cabe) */
    unsigned n = (unsigned)fabs(y);
    LD b = x;
    r = 1.0L;
    while (n) { if (n & 1) r *= b; b *= b; n >>= 1; }
    if (y < 0) r = 1.0L / r;
  } else {
    __asm__("fyl2x" : "=t"(l) : "0"((LD)x), "u"((LD)y) : "st(1)");    /* y * log2(x) */
    if (l > 16500) r = INFINITY;
    else if (l < -16500) r = 0;
    else r = dos_a(l);
  }
  pc_vuelve(cw);
  return neg ? -(double)r : (double)r;
}
double cbrt(double x) {
  unsigned short cw;
  LD a, r;
  if (isnan(x) || isinf(x) || x == 0) return x;
  cw = pc64();
  a = fabs(x);
  __asm__("fyl2x" : "=t"(r) : "0"(a), "u"(1.0L / 3) : "st(1)");
  r = dos_a(r);
  r = r - (r * r * r - a) / (3 * r * r);          /* un paso de Newton */
  pc_vuelve(cw);
  return x < 0 ? -(double)r : (double)r;
}

/* ---- trigonometría ---- */
/* el ángulo a [-2^62, 2^62] (fsin y compañía no pasan de 2^63) */
static LD reduce(LD x) {
  static const LD dospi = 6.28318530717958647692528676655900577L;
  unsigned short sw;
  if (!(x > -4.6e18L && x < 4.6e18L)) {
    do {
      __asm__("fprem1; fnstsw %%ax" : "=t"(x), "=a"(sw) : "0"(x), "u"(dospi));
    } while (sw & 0x400);
  }
  return x;
}
double sin(double x) {
  LD r;
  if (!isfinite(x)) return NAN;
  if (x == 0) return x;
  __asm__("fsin" : "=t"(r) : "0"(reduce(x)));
  return (double)r;
}
double cos(double x) {
  LD r;
  if (!isfinite(x)) return NAN;
  __asm__("fcos" : "=t"(r) : "0"(reduce(x)));
  return (double)r;
}
double tan(double x) {
  LD r, uno;
  if (!isfinite(x)) return NAN;
  if (x == 0) return x;
  __asm__("fptan" : "=t"(uno), "=u"(r) : "0"(reduce(x)));
  (void)uno;
  return (double)r;
}
double atan2(double y, double x) {
  LD r;
  if (isnan(x) || isnan(y)) return NAN;
  __asm__("fpatan" : "=t"(r) : "0"((LD)x), "u"((LD)y) : "st(1)");
  return (double)r;
}
double atan(double x) {
  if (isnan(x) || x == 0) return x;
  return atan2(x, 1.0);
}
double asin(double x) {
  unsigned short cw;
  LD s, r;
  if (isnan(x) || x == 0) return x;
  if (x > 1 || x < -1) return NAN;
  cw = pc64();
  s = (1.0L - x) * (1.0L + x);
  __asm__("fsqrt" : "=t"(s) : "0"(s));
  __asm__("fpatan" : "=t"(r) : "0"(s), "u"((LD)x) : "st(1)");
  pc_vuelve(cw);
  return (double)r;
}
double acos(double x) {
  unsigned short cw;
  LD s, r;
  if (isnan(x)) return x;
  if (x > 1 || x < -1) return NAN;
  cw = pc64();
  s = (1.0L - x) * (1.0L + x);
  __asm__("fsqrt" : "=t"(s) : "0"(s));
  __asm__("fpatan" : "=t"(r) : "0"((LD)x), "u"(s) : "st(1)");
  pc_vuelve(cw);
  return (double)r;
}

/* ---- hiperbólicas ---- */
double sinh(double x) {
  double a = fabs(x), t, r;
  if (isnan(x) || x == 0 || isinf(x)) return x;
  if (a < 22) {
    t = expm1(a);
    r = 0.5 * (t + t / (t + 1.0));
  } else if (a < 709) r = 0.5 * exp(a);
  else { t = exp(0.5 * a); r = (0.5 * t) * t; }
  return x < 0 ? -r : r;
}
double cosh(double x) {
  double a = fabs(x), t;
  if (isnan(x)) return x;
  if (isinf(x)) return INFINITY;
  if (a < 22) { t = exp(a); return 0.5 * t + 0.5 / t; }
  if (a < 709) return 0.5 * exp(a);
  t = exp(0.5 * a);
  return (0.5 * t) * t;
}
double tanh(double x) {
  double a = fabs(x), t, r;
  if (isnan(x) || x == 0) return x;
  if (a > 22) r = 1.0;
  else { t = expm1(2 * a); r = t / (t + 2.0); }
  return x < 0 ? -r : r;
}
double asinh(double x) {
  double a = fabs(x), r;
  if (isnan(x) || x == 0 || isinf(x)) return x;
  if (a > 268435456.0) r = log(a) + (double)LN2;
  else r = log1p(a + a * a / (1.0 + sqrt(1.0 + a * a)));
  return x < 0 ? -r : r;
}
double acosh(double x) {
  if (isnan(x)) return x;
  if (x < 1) return NAN;
  if (isinf(x)) return x;
  if (x > 268435456.0) return log(x) + (double)LN2;
  return log1p((x - 1) + sqrt((x - 1) * (x + 1)));
}
double atanh(double x) {
  double a = fabs(x), r;
  if (isnan(x) || x == 0) return x;
  if (a > 1) return NAN;
  if (a == 1) return copysign(INFINITY, x);
  r = 0.5 * log1p(2 * a / (1 - a));
  return x < 0 ? -r : r;
}
