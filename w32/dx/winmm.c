/* winmm.c - WINMM.DLL de Hispa-486 (fase 5, docs/486/HISPA-486.md).
 *  - la hora: timeGetTime (con RDTSC medido contra los tics de la BIOS; sin
 *    RDTSC, los tics: 55 ms), timeBeginPeriod/End, timeGetDevCaps,
 *    timeGetSystemTime;
 *  - temporizadores con llamada de vuelta (timeSetEvent/timeKillEvent): no
 *    hay hilos, así que se lanzan desde el «tic» que da Hispa Ventanas cada
 *    vez que el programa pide un mensaje (HispaDXTic), y desde timeGetTime;
 *  - LA MEZCLA del sonido de todo el programa: voces (HxVoz) que se suman por
 *    programa a 16 bits y se mandan a HSPSND por trozos enteros de lo que la
 *    tarjeta toca de una vez (HispaDXSonido); la usan waveOut, PlaySound,
 *    mciSendString y DSOUND.DLL (HispaMez*);
 *  - waveOut* (cabeceras en cola, WOM_DONE por mensaje, función o evento),
 *    PlaySound/sndPlaySound (WAV de archivo, de memoria o de recurso; SND_ASYNC,
 *    SND_LOOP, SND_SYNC), mciSendString lo mínimo (open/play/stop/close de WAV);
 *  - joyGetNumDevs, joyGetDevCaps, joyGetPos y joyGetPosEx sobre HSPJOY.
 * Sala limpia: hecho a partir de la documentación pública del API.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "dx.h"

void *memcpy(void *d, const void *s, u32 n) { hx_copia(d, s, n); return d; }
void *memset(void *d, int c, u32 n) { u8 *p = d; while (n--) *p++ = c; return d; }

/* ======================= la hora ======================= */
static volatile u32 *tics_bios;     /* 0040:006C */
#ifdef HISPA_PENTIUM
/* [WIN32] en Hispa-Pentium no hay tics de la BIOS que corran solos: los de
 * GetTickCount (castel/nucleo32/win32/), pasados a 18,2 por segundo */
#define TICS_BIOS() (GetTickCount() / 10 * 182 / 1000)
#else
#define TICS_BIOS() (*tics_bios)
#endif
static int hay_tsc = -1;
static u64 tsc0;
static u32 ms0, ms_ult, tsc_khz, tic_ult, dias;

static inline u64 rdtsc(void) { u32 lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return ((u64)hi << 32) | lo; }
static int cpu_tsc(void) {
  u32 a, b, d;
  __asm__ volatile("pushfl; popl %0; movl %0, %1; xorl $0x200000, %0; pushl %0; popfl; pushfl; popl %0; pushl %1; popfl"
                   : "=&r"(a), "=&r"(b));
  if (!((a ^ b) & 0x200000)) return 0;       /* sin CPUID: un 486 */
  __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(d), "=d"(d) : "a"(0));
  if (a < 1) return 0;
  __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(d), "=d"(d) : "a"(1));
  return (d >> 4) & 1;
}
static u32 tics_ms(void) {          /* los tics de la BIOS, en ms (con los días) */
  u32 t = TICS_BIOS();
  if (t < tic_ult) dias++;
  tic_ult = t;
  t += dias * 0x1800B0;
  return t * 54 + t / 1000 * 925 + t % 1000 * 925 / 1000;
}
static void reloj_ini(void) {
  if (hay_tsc >= 0) return;
  tics_bios = (volatile u32 *)((u8 *)HispaDXEntrada(3, 0, 0) + 0x46C);
  tic_ult = TICS_BIOS();
  hay_tsc = cpu_tsc();
  if (hay_tsc) {                    /* se mide el TSC durante dos tics */
    u32 t = TICS_BIOS(), n = 0; u64 a, b;
    while (TICS_BIOS() == t && ++n < 200000000) ;
    a = rdtsc(); t = TICS_BIOS();
    while (TICS_BIOS() == t) ;
    t = TICS_BIOS();
    while (TICS_BIOS() == t) ;
    b = rdtsc();
    tsc_khz = (u32)(b - a) / 110;  /* dos tics = 109,85 ms */
    if (!tsc_khz) hay_tsc = 0;
    tsc0 = b; ms0 = tics_ms();
  }
}
#ifdef HISPA_PENTIUM
/* (JUEGOS) en Hispa-Pentium, el contador fino de KERNEL32 (los tics del
 * núcleo y el TSC entre tic y tic, a 1 MHz): no se va aunque el TSC no
 * corra como el reloj */
int WINAPI QueryPerformanceCounter(u64 *c);
static u32 reloj(void) {
  u64 c = 0;
  QueryPerformanceCounter(&c);
  return (u32)hx_div64(c, 1000);
}
#else
static u32 reloj(void) {
  u32 ms, t;
  reloj_ini();
  t = tics_ms();
  if (!hay_tsc) ms = t;
  else {
    ms = ms0 + (u32)hx_div64(rdtsc() - tsc0, tsc_khz);
    if ((i32)(ms - t) > 200 || (i32)(t - ms) > 200) { tsc0 = rdtsc(); ms0 = ms = t; }  /* se ha ido: otra vez */
  }
  if ((i32)(ms - ms_ult) < 0) ms = ms_ult;    /* nunca hacia atrás */
  ms_ult = ms;
  return ms;
}
#endif

/* ======================= la mezcla ======================= */
static int snd_tarjeta = -2;        /* -2: sin mirar; -1: no hay HSPSND */
static u32 mez_hz, mez_tramas, mez_fmt;   /* fmt: bit 0 16 bits, bit 1 estéreo */
static int mez_trozos = 1;         /* trozos de HSPSND que ocupa cada envío */
static HxVoz *voces;
static volatile int en_bomba;
static i32 *acum;
static u8 *salida;
static void tic_init(void);
static void hechos_avisar(void);
static void eventos_tocar(void);
#ifdef HISPA_PENTIUM
static void cd_rellena_si(void);    /* (JUEGOS) el CD de sonido, más abajo */
#endif

static void mez_ini(void) {
  if (snd_tarjeta != -2) return;
  snd_tarjeta = HispaDXSonido(0, 0, 0, 0);
  if (snd_tarjeta <= 0) { snd_tarjeta = -1; mez_hz = 22050; return; }
  switch (snd_tarjeta) {
  case 1: mez_hz = 22050; mez_tramas = 4096; mez_fmt = 0; break;   /* SB de 8 bits */
  case 2: mez_hz = 44100; mez_tramas = 2048; mez_fmt = 3; mez_trozos = 2; break;   /* SB16 (trozos de 1.024) */
  default: mez_hz = 48000; mez_tramas = 2048; mez_fmt = 3; break;  /* AC'97, HD Audio */
  }
  acum = hx_pedir(mez_tramas * 8);
  salida = hx_pedir(mez_tramas * 4);
  tic_init();
}
u32 WINAPI HispaMezHz(void) { mez_ini(); return mez_hz; }
u32 WINAPI HispaMezPaso(u32 hz) { mez_ini(); return (u32)hx_div64((u64)hz << 16, mez_hz); }
/* centésimas de dB (-10000..0) -> 0..4096 */
int WINAPI HispaMezGanancia(int mb) {
  static const u16 t[16] = { 4096, 3922, 3756, 3597, 3444, 3298, 3158, 3025, 2896, 2774, 2656, 2543, 2435, 2332, 2233, 2139 };
  u32 e, g;
  if (mb >= 0) return 4096;
  if (mb <= -10000) return 0;
  e = (u32)(-mb) * 16 / 602;        /* pasos de 1/16 de octava (2^(-1/16)) */
  g = t[e & 15] >> (e >> 4);
  return g;
}
void WINAPI HispaMezPon(HxVoz *v) {
  HxVoz *p;
  mez_ini();
  for (p = voces; p; p = p->sig) if (p == v) { v->activa = 1; return; }
  v->sig = voces; voces = v; v->activa = 1;
}
void WINAPI HispaMezQuita(HxVoz *v) {
  HxVoz **p;
  for (p = &voces; *p; p = &(*p)->sig) if (*p == v) { *p = v->sig; break; }
  v->activa = 0; v->sig = NULL;
}
static inline int muestra(const HxVoz *v, u32 i, int lado) {
  const u8 *d = v->datos;
  if (v->bits == 16) {
    const i16 *s = (const i16 *)d;
    return v->canales == 2 ? s[i * 2 + lado] : s[i];
  }
  return ((int)(v->canales == 2 ? d[i * 2 + lado] : d[i]) - 128) << 8;
}
static void mezclar_voz(HxVoz *v, i32 *a, u32 n) {
  while (n && v->activa) {
    u32 i, j;
    int l0, r0, l1, r1, f;
    if (v->pos >= v->tramas) {
      if (v->bucle && v->tramas) v->pos %= v->tramas;
      else {
        if (v->fin) v->fin(v);
        if (!v->activa || v->pos >= v->tramas) { v->activa = 0; return; }
        continue;
      }
    }
    i = v->pos; j = i + 1;
    if (j >= v->tramas) j = v->bucle ? 0 : i;
    f = v->frac >> 4;               /* 12 bits */
    l0 = muestra(v, i, 0); l1 = muestra(v, j, 0);
    if (v->canales == 2) { r0 = muestra(v, i, 1); r1 = muestra(v, j, 1); } else { r0 = l0; r1 = l1; }
    l0 += ((l1 - l0) * f) >> 12;
    r0 += ((r1 - r0) * f) >> 12;
    a[0] += (l0 * v->vol_i) >> 12;
    a[1] += (r0 * v->vol_d) >> 12;
    a += 2; n--;
    v->frac += v->paso;
    v->pos += v->frac >> 16;
    v->frac &= 0xFFFF;
  }
}
static void mezclar_trozo(void) {
  u32 k, n = mez_tramas;
  HxVoz *v, *s;
  hx_cero(acum, n * 8);
  for (v = voces; v; v = s) { s = v->sig; if (v->activa) mezclar_voz(v, acum, n); }
  if (mez_fmt == 3) {
    i16 *o = (i16 *)salida;
    for (k = 0; k < n * 2; k++) { i32 x = acum[k]; o[k] = x > 32767 ? 32767 : x < -32768 ? -32768 : x; }
  } else {
    for (k = 0; k < n; k++) { i32 x = (acum[k * 2] + acum[k * 2 + 1]) >> 9; salida[k] = (x > 127 ? 127 : x < -128 ? -128 : x) + 128; }
  }
}
static int alguna_activa(void) { HxVoz *v; for (v = voces; v; v = v->sig) if (v->activa) return 1; return 0; }
/* la bomba: tantos trozos como quepan (la cola de HSPSND, llena: unos 170 ms;
 * así aguanta lo que tarde el programa entre dos llamadas) */
void WINAPI HispaMezBomba(void) {
  int sitio, n = 0;
  mez_ini();
  if (__sync_lock_test_and_set(&en_bomba, 1)) return;   /* (JUEGOS) uno a la vez, de cualquier hilo */
  eventos_tocar();
  if (snd_tarjeta > 0 && alguna_activa()) {
    static u32 ult_mira;            /* (preguntar a HSPSND cuesta un paso a modo
                                       real: como mucho, cada 8 ms) */
    u32 ahora = reloj();
    if (ult_mira && ahora - ult_mira < 8) { en_bomba = 0; hechos_avisar(); return; }
    ult_mira = ahora;
    while (n < 4 && (sitio = HispaDXSonido(2, 0, 0, 0)) >= mez_trozos) {
      mezclar_trozo();
      HispaDXSonido(1, (u32)salida, mez_tramas * (mez_fmt == 3 ? 4 : 1), mez_hz | (mez_fmt << 24));
      n++;
      if (!alguna_activa()) break;
    }
  } else if (snd_tarjeta <= 0) {
    /* sin tarjeta: el tiempo pasa igual (los juegos esperan a que acabe) */
    static u32 ult;
    u32 ahora = reloj(), d = ahora - ult;
    HxVoz *v, *s;
    if (ult && d > 0 && d < 1000) {
      u32 tr = d * mez_hz / 1000;
      for (v = voces; v; v = s) {
        s = v->sig;
        if (!v->activa) continue;
        { u64 x = v->frac + (u64)tr * v->paso;
          v->pos += (u32)(x >> 16); v->frac = (u32)x & 0xFFFF;
          if (v->pos >= v->tramas) {
            if (v->bucle && v->tramas) v->pos %= v->tramas;
            else { v->pos = v->tramas; v->frac = 0; if (v->fin) v->fin(v); if (v->pos >= v->tramas) v->activa = 0; }
          } }
      }
    }
    ult = ahora;
  }
  en_bomba = 0;
#ifdef HISPA_PENTIUM
  cd_rellena_si();                  /* (JUEGOS) el CD, si no tiene su hilo */
#endif
  hechos_avisar();
}
static int WINAPI tic(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; HispaMezBomba(); return 0; }
static int tic_puesto;
#ifdef HISPA_PENTIUM
/* (JUEGOS) con hilos de verdad, la mezcla sigue aunque el programa no pida
 * mensajes (cargando un nivel): un hilo la llama cada 10 ms; y el CD, si
 * suena sin el suyo */
int WINAPI HispaHilosDeVerdad(void);
typedef u32 (WINAPI *HILO_M)(void *);
HANDLE WINAPI CreateThread(void *, u32, HILO_M, void *, u32, u32 *);
static u32 WINAPI hilo_mezcla(void *p) { (void)p; for (;;) { Sleep(10); HispaMezBomba(); } return 0; }
#endif
static void tic_init(void) {
  if (!tic_puesto) {
    tic_puesto = 1;
    HispaDXTic((void *)tic);
#ifdef HISPA_PENTIUM
    if (HispaHilosDeVerdad()) CreateThread(NULL, 0x10000, hilo_mezcla, NULL, 0, NULL);
#endif
  }
}

/* ======================= los temporizadores ======================= */
typedef void WINAPI (*TIMECB)(u32, u32, u32, u32, u32);
#define NEV 16
static struct { u32 uso, id, retraso, sig, flags; TIMECB f; u32 dato; } ev[NEV];
static u32 ev_num = 1, periodo = 1;
static void eventos_tocar(void) {
  int k;
  u32 ahora;
  static int dentro;
  if (dentro) return;
  dentro = 1;
  ahora = reloj();
  for (k = 0; k < NEV; k++) {
    if (!ev[k].uso || (i32)(ahora - ev[k].sig) < 0) continue;
    if (ev[k].flags & 1) {          /* TIME_PERIODIC */
      ev[k].sig += ev[k].retraso ? ev[k].retraso : 1;
      if ((i32)(ahora - ev[k].sig) > 200) ev[k].sig = ahora + ev[k].retraso;
    } else ev[k].uso = 0;
    if (ev[k].flags & 0x10) SetEvent((HANDLE)ev[k].f);       /* TIME_CALLBACK_EVENT_SET */
    else if (ev[k].f) ev[k].f(ev[k].id, 0, ev[k].dato, 0, 0);
  }
  dentro = 0;
}
u32 WINAPI timeSetEvent(u32 retraso, u32 res, TIMECB f, u32 dato, u32 flags) {
  int k;
  (void)res;
  mez_ini();
  for (k = 0; k < NEV; k++) if (!ev[k].uso) {
    ev[k].uso = 1; ev[k].id = ev_num++; ev[k].retraso = retraso; ev[k].flags = flags;
    ev[k].f = f; ev[k].dato = dato; ev[k].sig = reloj() + retraso;
    tic_init();
    return ev[k].id;
  }
  return 0;
}
u32 WINAPI timeKillEvent(u32 id) {
  int k;
  for (k = 0; k < NEV; k++) if (ev[k].uso && ev[k].id == id) { ev[k].uso = 0; return 0; }
  return 97;                        /* MMSYSERR_INVALPARAM... TIMERR_NOCANDO */
}
u32 WINAPI timeGetTime(void) {
  u32 t = reloj();
  if (tic_puesto && !en_bomba) HispaMezBomba();   /* (quien espera mirando la hora) */
  return t;
}
u32 WINAPI timeBeginPeriod(u32 p) { if (!p) return 97; periodo = p; return 0; }
u32 WINAPI timeEndPeriod(u32 p) { (void)p; periodo = 1; return 0; }
u32 WINAPI timeGetDevCaps(u32 *tc, u32 n) { if (!tc || n < 8) return 97; tc[0] = 1; tc[1] = 1000000; return 0; }
u32 WINAPI timeGetSystemTime(u32 *mt, u32 n) { if (n >= 8) { mt[0] = 1; mt[1] = reloj(); } return 0; }

/* ======================= los WAV ======================= */
typedef struct { u16 fmt, canales; u32 hz, octps; u16 bloque, bits; u16 extra; } WAVEFORMATEX;
static int wav_leer(const u8 *d, u32 n, WAVEFORMATEX *f, const u8 **datos, u32 *tam) {
  u32 p = 12;
  int hay_fmt = 0;
  if (n < 12 || d[0] != 'R' || d[1] != 'I' || d[2] != 'F' || d[3] != 'F' || d[8] != 'W' || d[9] != 'A') return 0;
  while (p + 8 <= n) {
    u32 t = *(const u32 *)(d + p + 4);
    if (d[p] == 'f' && d[p + 1] == 'm' && d[p + 2] == 't') { hx_copia(f, d + p + 8, 16); hay_fmt = 1; }
    else if (d[p] == 'd' && d[p + 1] == 'a' && d[p + 2] == 't' && d[p + 3] == 'a') {
      *datos = d + p + 8; *tam = t;
      if (p + 8 + t > n) *tam = n - p - 8;
      return hay_fmt && f->fmt == 1 && (f->bits == 8 || f->bits == 16) && (f->canales == 1 || f->canales == 2);
    }
    p += 8 + ((t + 1) & ~1u);
  }
  return 0;
}
static u8 *archivo_leer(const char *nom, u32 *n) {
  HANDLE h = CreateFileA(nom, 0x80000000, 1, NULL, 3, 0x80, NULL);
  u8 *b; u32 t, l;
  if (h == (HANDLE)-1) return NULL;
  t = GetFileSize(h, NULL);
  if (t == 0xFFFFFFFF || !(b = hx_pedir(t))) { CloseHandle(h); return NULL; }
  if (!ReadFile(h, b, t, &l, NULL)) l = 0;
  CloseHandle(h);
  *n = l;
  return b;
}
static void voz_wav(HxVoz *v, const WAVEFORMATEX *f, const u8 *datos, u32 tam) {
  v->datos = datos; v->bits = f->bits; v->canales = f->canales;
  v->tramas = tam / (f->bits / 8 * f->canales);
  v->pos = v->frac = 0; v->paso = HispaMezPaso(f->hz);
  v->vol_i = v->vol_d = 4096;
}

/* ======================= PlaySound ======================= */
static HxVoz ps_voz;
static u8 *ps_buf;                  /* lo leído del archivo (se suelta luego) */
#define SND_ASYNC 1
#define SND_NODEFAULT 2
#define SND_MEMORY 4
#define SND_LOOP 8
#define SND_NOSTOP 0x10
#define SND_PURGE 0x40
#define SND_FILENAME 0x20000
#define SND_RESOURCE 0x40004
#define SND_ALIAS 0x10000
static void ps_parar(void) {
  if (ps_voz.activa || ps_voz.sig) HispaMezQuita(&ps_voz);
  ps_voz.activa = 0;
  hx_soltar(ps_buf); ps_buf = NULL;
}
int WINAPI PlaySoundA(const char *s, HANDLE mod, u32 fl) {
  const u8 *d = NULL, *datos; u32 n = 0, tam;
  WAVEFORMATEX f;
  mez_ini();
  if ((fl & SND_NOSTOP) && ps_voz.activa) return 0;
  ps_parar();
  if (!s || (fl & SND_PURGE)) return 1;
  if ((fl & SND_RESOURCE) == SND_RESOURCE) {
    HANDLE r = FindResourceA(mod, s, "WAVE"), g;
    if (!r || !(g = LoadResource(mod, r))) return 0;
    d = LockResource(g); n = SizeofResource(mod, r);
  } else if (fl & SND_MEMORY) { d = (const u8 *)s; n = 0x7FFFFFFF; n = 8 + *(const u32 *)(d + 4); }
  else if (fl & SND_ALIAS) {
    return 0;                       /* (los sonidos del sistema: no hay registro) */
  } else {
    if (!(ps_buf = archivo_leer(s, &n))) return 0;
    d = ps_buf;
  }
  if (!d || !wav_leer(d, n, &f, &datos, &tam)) { ps_parar(); return 0; }
  voz_wav(&ps_voz, &f, datos, tam);
  ps_voz.bucle = (fl & SND_LOOP) != 0;
  ps_voz.fin = NULL;
  HispaMezPon(&ps_voz);
  HispaMezBomba();
  if (!(fl & SND_ASYNC) && !ps_voz.bucle) {
    while (ps_voz.activa) { HispaMezBomba(); Sleep(20); }
    ps_parar();
  }
  return 1;
}
int WINAPI PlaySound(const char *s, HANDLE m, u32 f) { return PlaySoundA(s, m, f); }
int WINAPI sndPlaySoundA(const char *s, u32 f) {
  return PlaySoundA(s, NULL, (f & (SND_ASYNC | SND_NODEFAULT | SND_LOOP | SND_NOSTOP)) | ((f & SND_MEMORY) ? SND_MEMORY : SND_FILENAME));
}

/* ======================= waveOut ======================= */
typedef struct WAVEHDR { u8 *lpData; u32 dwBufferLength, dwBytesRecorded, dwUser, dwFlags, dwLoops; struct WAVEHDR *lpNext; u32 reserved; } WAVEHDR;
#define WHDR_DONE 1
#define WHDR_PREPARED 2
#define WHDR_BEGINLOOP 4
#define WHDR_ENDLOOP 8
#define WHDR_INQUEUE 0x10
#define WOM_OPEN 0x3BB
#define WOM_CLOSE 0x3BC
#define WOM_DONE 0x3BD
#define NWO 4
typedef struct {
  u32 uso;
  WAVEFORMATEX f;
  u32 cb, inst, modo;               /* modo: 0 nada, 1 ventana, 3 función, 5 evento */
  WAVEHDR *cola, *hechos;           /* lo que suena (en orden) y lo hecho, por avisar */
  HxVoz voz;
  u32 vueltas;                      /* las que le quedan a la cabecera de ahora */
  u32 octetos;                      /* los que ya han sonado (de las anteriores) */
  int parado;
  u32 vol;
} WO;
static WO wo[NWO];
typedef void WINAPI (*WAVECB)(HANDLE, u32, u32, u32, u32);
static void wo_aviso(WO *w, u32 msg, u32 p1) {
  HANDLE h = (HANDLE)(w - wo + 1);
  if (w->modo == 1) PostMessageA((HWND)w->cb, msg, (u32)h, p1);
  else if (w->modo == 3 && w->cb) ((WAVECB)w->cb)(h, msg, w->inst, p1, 0);
  else if (w->modo == 5 && w->cb) SetEvent((HANDLE)w->cb);
}
static void wo_pon_voz(WO *w) {
  WAVEHDR *h = w->cola;
  u32 bpt = w->f.bits / 8 * w->f.canales;
  if (!h) { w->voz.activa = 0; w->voz.tramas = 0; return; }
  w->voz.datos = h->lpData;
  w->voz.tramas = h->dwBufferLength / bpt;
  w->voz.pos = 0;
  w->voz.activa = !w->parado;
}
/* (la mezcla) se ha acabado la cabecera de delante */
static void wo_fin(HxVoz *v) {
  WO *w = v->dueno;
  WAVEHDR *h = w->cola, **p;
  if (!h) { v->activa = 0; return; }
  if ((h->dwFlags & WHDR_ENDLOOP) && w->vueltas > 1) { w->vueltas--; v->pos = 0; return; }
  w->octetos += h->dwBufferLength;
  w->cola = h->lpNext;
  h->lpNext = NULL;
  for (p = &w->hechos; *p; p = &(*p)->lpNext) ;
  *p = h;
  if (w->cola && (w->cola->dwFlags & WHDR_BEGINLOOP)) w->vueltas = w->cola->dwLoops;
  wo_pon_voz(w);
  if (!w->cola) v->activa = 0;
}
static void hechos_avisar(void) {
  int k;
  for (k = 0; k < NWO; k++) {
    WO *w = &wo[k];
    while (w->uso && w->hechos) {
      WAVEHDR *h = w->hechos;
      w->hechos = h->lpNext;
      h->lpNext = NULL;
      h->dwFlags = (h->dwFlags & ~WHDR_INQUEUE) | WHDR_DONE;
      wo_aviso(w, WOM_DONE, (u32)h);
    }
  }
}
static WO *wo_de(HANDLE h) { u32 k = (u32)h - 1; return k < NWO && wo[k].uso ? &wo[k] : NULL; }
u32 WINAPI waveOutGetNumDevs(void) { mez_ini(); return snd_tarjeta > 0 ? 1 : 0; }
typedef struct { u16 wMid, wPid; u32 vDriverVersion; char szPname[32]; u32 dwFormats; u16 wChannels, wReserved1; u32 dwSupport; } WAVEOUTCAPSA;
u32 WINAPI waveOutGetDevCapsA(u32 id, WAVEOUTCAPSA *c, u32 n) {
  static const char nom[] = "Hispa-486 (HSPSND)";
  (void)id;
  mez_ini();
  if (snd_tarjeta <= 0) return 2;   /* MMSYSERR_BADDEVICEID */
  if (!c || n < sizeof(WAVEOUTCAPSA)) return 11;
  hx_cero(c, sizeof *c);
  c->wMid = 0xFF; c->wPid = 1; c->vDriverVersion = 0x100;
  hx_copia(c->szPname, nom, sizeof nom);
  c->dwFormats = 0xFFF; c->wChannels = 2; c->dwSupport = 0x0C;   /* volumen de los dos lados */
  return 0;
}
u32 WINAPI waveOutOpen(HANDLE *ph, u32 id, const WAVEFORMATEX *f, u32 cb, u32 inst, u32 fl) {
  int k;
  (void)id;
  mez_ini();
  if (snd_tarjeta <= 0) return 2;
  if (!f || f->fmt != 1 || (f->bits != 8 && f->bits != 16) || f->canales < 1 || f->canales > 2 || !f->hz) return 32;  /* WAVERR_BADFORMAT */
  if (fl & 1) return 0;             /* WAVE_FORMAT_QUERY */
  for (k = 0; k < NWO; k++) if (!wo[k].uso) break;
  if (k == NWO) return 4;           /* MMSYSERR_ALLOCATED */
  hx_cero(&wo[k], sizeof wo[k]);
  wo[k].uso = 1;
  hx_copia(&wo[k].f, f, 16);
  wo[k].cb = cb; wo[k].inst = inst; wo[k].modo = (fl >> 16) & 7;
  wo[k].vol = 0xFFFFFFFF;
  voz_wav(&wo[k].voz, &wo[k].f, NULL, 0);
  wo[k].voz.dueno = &wo[k];
  wo[k].voz.fin = wo_fin;
  if (ph) *ph = (HANDLE)(k + 1);
  wo_aviso(&wo[k], WOM_OPEN, 0);
  return 0;
}
u32 WINAPI waveOutPrepareHeader(HANDLE h, WAVEHDR *w, u32 n) { (void)n; if (!wo_de(h)) return 5; w->dwFlags = (w->dwFlags & ~WHDR_DONE) | WHDR_PREPARED; return 0; }
u32 WINAPI waveOutUnprepareHeader(HANDLE h, WAVEHDR *w, u32 n) {
  (void)n; if (!wo_de(h)) return 5;
  if (w->dwFlags & WHDR_INQUEUE) return 33;   /* WAVERR_STILLPLAYING */
  w->dwFlags &= ~WHDR_PREPARED; return 0;
}
u32 WINAPI waveOutWrite(HANDLE h, WAVEHDR *w, u32 n) {
  WO *o = wo_de(h);
  WAVEHDR **p;
  (void)n;
  if (!o) return 5;
  if (!(w->dwFlags & WHDR_PREPARED)) return 34;   /* WAVERR_UNPREPARED */
  w->dwFlags = (w->dwFlags & ~WHDR_DONE) | WHDR_INQUEUE;
  w->lpNext = NULL;
  for (p = &o->cola; *p; p = &(*p)->lpNext) ;
  *p = w;
  if (o->cola == w) {
    if (w->dwFlags & WHDR_BEGINLOOP) o->vueltas = w->dwLoops;
    wo_pon_voz(o);
    if (!o->parado) HispaMezPon(&o->voz);
  }
  HispaMezBomba();
  return 0;
}
static void wo_todo_hecho(WO *o) {
  WAVEHDR **p;
  for (p = &o->hechos; *p; p = &(*p)->lpNext) ;
  *p = o->cola;
  o->cola = NULL;
  o->voz.activa = 0;
  HispaMezQuita(&o->voz);
}
u32 WINAPI waveOutReset(HANDLE h) {
  WO *o = wo_de(h);
  if (!o) return 5;
  wo_todo_hecho(o);
  o->octetos = 0; o->parado = 0;
  hechos_avisar();
  return 0;
}
u32 WINAPI waveOutBreakLoop(HANDLE h) { WO *o = wo_de(h); if (!o) return 5; o->vueltas = 0; return 0; }
u32 WINAPI waveOutPause(HANDLE h) { WO *o = wo_de(h); if (!o) return 5; o->parado = 1; o->voz.activa = 0; return 0; }
u32 WINAPI waveOutRestart(HANDLE h) {
  WO *o = wo_de(h); if (!o) return 5;
  o->parado = 0;
  if (o->cola) { o->voz.activa = 1; HispaMezPon(&o->voz); }
  return 0;
}
u32 WINAPI waveOutClose(HANDLE h) {
  WO *o = wo_de(h);
  if (!o) return 5;
  if (o->cola) return 33;
  HispaMezQuita(&o->voz);
  hechos_avisar();
  wo_aviso(o, WOM_CLOSE, 0);
  o->uso = 0;
  return 0;
}
typedef struct { u32 wType; u32 u[2]; } MMTIME;
u32 WINAPI waveOutGetPosition(HANDLE h, MMTIME *t, u32 n) {
  WO *o = wo_de(h);
  u32 b, bpt;
  (void)n;
  if (!o || !t) return 5;
  bpt = o->f.bits / 8 * o->f.canales;
  b = o->octetos + (o->cola ? o->voz.pos * bpt : 0);
  if (t->wType == 1) t->u[0] = (u32)hx_div64((u64)b * 1000, o->f.hz * bpt);   /* TIME_MS */
  else if (t->wType == 2) t->u[0] = b / bpt;                              /* TIME_SAMPLES */
  else { t->wType = 4; t->u[0] = b; }                                      /* TIME_BYTES */
  return 0;
}
u32 WINAPI waveOutSetVolume(HANDLE h, u32 v) {
  int k;
  for (k = 0; k < NWO; k++) if (wo[k].uso && (!h || wo_de(h) == &wo[k])) {
    wo[k].vol = v;
    wo[k].voz.vol_i = (v & 0xFFFF) >> 4;
    wo[k].voz.vol_d = (v >> 16) >> 4;
  }
  return 0;
}
u32 WINAPI waveOutGetVolume(HANDLE h, u32 *v) { WO *o = wo_de(h); if (v) *v = o ? o->vol : 0xFFFFFFFF; return 0; }
u32 WINAPI waveOutGetID(HANDLE h, u32 *id) { if (!wo_de(h)) return 5; if (id) *id = 0; return 0; }
u32 WINAPI waveOutMessage(HANDLE h, u32 m, u32 a, u32 b) { (void)h; (void)m; (void)a; (void)b; return 8; }
u32 WINAPI waveOutGetErrorTextA(u32 e, char *s, u32 n) {
  const char *t = e == 0 ? "Correcto." : e == 2 ? "No hay tarjeta de sonido (HSPSND)." : e == 32 ? "Formato de sonido no admitido." : "Error de sonido.";
  u32 k = 0;
  if (!s || !n) return 11;
  while (t[k] && k + 1 < n) { s[k] = t[k]; k++; }
  s[k] = 0;
  return 0;
}
u32 WINAPI waveInGetNumDevs(void) { return 0; }
u32 WINAPI midiOutGetNumDevs(void) { return 0; }
u32 WINAPI auxGetNumDevs(void) { return 0; }
u32 WINAPI mixerGetNumDevs(void) { return 0; }

/* ======================= mciSendString (lo mínimo) ======================= */
#define NMCI 4
static struct { char alias[32]; u8 *buf; HxVoz voz; } mci[NMCI];
static int pal(const char **s, char *d, int n) {
  int k = 0;
  while (**s == ' ' || **s == '\t') (*s)++;
  if (**s == '"') { (*s)++; while (**s && **s != '"') { if (k + 1 < n) d[k++] = **s; (*s)++; } if (**s) (*s)++; }
  else while (**s && **s != ' ' && **s != '\t') { if (k + 1 < n) d[k++] = **s; (*s)++; }
  d[k] = 0;
  return k;
}
static int igual(const char *a, const char *b) {
  for (; *a && *b; a++, b++) { char x = *a, y = *b; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return 0; }
  return *a == *b;
}
static int mci_busca(const char *a) { int k; for (k = 0; k < NMCI; k++) if (mci[k].buf && igual(mci[k].alias, a)) return k; return -1; }
static int mci_abre(const char *arch, const char *alias) {
  int k; u32 n, tam; const u8 *datos; WAVEFORMATEX f;
  for (k = 0; k < NMCI; k++) if (!mci[k].buf) break;
  if (k == NMCI) return -1;
  if (!(mci[k].buf = archivo_leer(arch, &n))) return -2;
  if (!wav_leer(mci[k].buf, n, &f, &datos, &tam)) { hx_soltar(mci[k].buf); mci[k].buf = NULL; return -3; }
  voz_wav(&mci[k].voz, &f, datos, tam);
  { int i = 0; while (alias[i] && i < 31) { mci[k].alias[i] = alias[i]; i++; } mci[k].alias[i] = 0; }
  return k;
}
static void mci_cierra(int k) { HispaMezQuita(&mci[k].voz); hx_soltar(mci[k].buf); mci[k].buf = NULL; }
/* ======================= (JUEGOS) el CD de sonido (MCI «cdaudio») =======================
 * Solo en Hispa-Pentium: las pistas (READ TOC) y sus muestras en crudo (READ
 * CD, 2352 octetos por sector: 44,1 kHz, 16 bits, estéreo) por la llamada
 * 186 (HispaCDAudio, de WIN32.BIN), a un anillo de 4 s que suena como una voz
 * más de la mezcla; un hilo lo va llenando por delante (o la bomba, si no
 * hay hilos de verdad). Con mciSendString y mciSendCommand: open, set time
 * format (tmsf, msf, milliseconds), status (number of tracks, length,
 * position, mode, media present, type track, current track, ready), play
 * from/to (con notify: MM_MCINOTIFY a la ventana, como lo esperan los juegos
 * para repetir la pista), stop, pause, resume, seek, close. */
#ifdef HISPA_PENTIUM
u32 WINAPI HispaCDAudio(u32 op, u32 letra, void *d, u32 lba, u32 n);
int WINAPI HispaHilosDeVerdad(void);
typedef u32 (WINAPI *HILO_W)(void *);
HANDLE WINAPI CreateThread(void *, u32, HILO_W, void *, u32, u32 *);
#define CD_ID 0x4344                /* el número del dispositivo «cdaudio» */
#define CD_TRAMAS (44100 * 4)       /* el anillo: 4 s */
#define SECT 588                    /* tramas por sector */
#define F_MS 0
#define F_MSF 2
#define F_TMSF 10
static struct {
  int abierto, n, primera, hay_disco;
  u32 lba[101];                     /* donde empieza cada pista (y, detrás de la última, el final) */
  u8 sonido[101];
  u32 formato;
  char alias[32];
  volatile int sonando, pausado, con_hilo;
  u32 desde, hasta, cursor;         /* lba: lo que suena, y lo que ya se ha leído */
  u8 *anillo;
  volatile u32 escritas;            /* tramas metidas en el anillo */
  u32 vueltas, ult_pos;
  HxVoz voz;
  HWND aviso;
  int avisar;
} cd;
static u32 msf(u32 lba) { lba += 150; return (lba / 4500) | ((lba / 75 % 60) << 8) | ((lba % 75) << 16); }
static u32 de_msf(u32 v) { u32 m = v & 0xFF, s = (v >> 8) & 0xFF, f = (v >> 16) & 0xFF, l = m * 4500 + s * 75 + f; return l >= 150 ? l - 150 : 0; }
static int cd_lee_toc(void) {
  static u8 t[804];
  u32 n = HispaCDAudio(0, 0, t, 0, 0), k, j = 0;
  cd.hay_disco = 0;
  cd.n = 0;
  if ((i32)n < 12) return 0;
  for (k = 4; k + 8 <= n && j < 100; k += 8) {
    u32 p = t[k + 2], l = ((u32)t[k + 4] << 24) | ((u32)t[k + 5] << 16) | ((u32)t[k + 6] << 8) | t[k + 7];
    if (p == 0xAA) { cd.lba[j] = l; break; }
    if (!j) cd.primera = p;
    cd.lba[j] = l;
    cd.sonido[j] = !(t[k + 1] & 4);
    j++;
  }
  cd.n = j;
  cd.hay_disco = j > 0;
  return cd.hay_disco;
}
static u32 jugadas(void) {
  u32 p = cd.voz.pos;
  if (p < cd.ult_pos) cd.vueltas++;
  cd.ult_pos = p;
  return cd.vueltas * CD_TRAMAS + p;
}
static void cd_aviso(u32 como) {
  if (cd.avisar && cd.aviso) PostMessageA(cd.aviso, 0x3B9, como, CD_ID);   /* MM_MCINOTIFY */
  cd.avisar = 0;
}
static void cd_para(u32 como) {
  if (cd.sonando) { HispaMezQuita(&cd.voz); cd.sonando = 0; cd_aviso(como); }
  cd.pausado = 0;
}
/* lee lo que falte del anillo (lo llama el hilo, o la bomba) */
static void cd_rellena(void) {
  static u8 tmp[25 * 2352];
  u32 j, libres, n;
  if (!cd.sonando || cd.pausado) return;
  j = jugadas();
  if (cd.cursor >= cd.hasta) {
    if (j >= cd.escritas) cd_para(1);   /* MCI_NOTIFY_SUCCESSFUL */
    return;
  }
  libres = CD_TRAMAS - (cd.escritas - j);
  if (libres < 25 * SECT) return;
  n = cd.hasta - cd.cursor;
  if (n > 25) n = 25;
  if (HispaCDAudio(1, 0, tmp, cd.cursor, n) != n * 2352) {
    /* (no se lee: silencio, y se sigue) */
    hx_cero(tmp, n * 2352);
  }
  {
    u32 o = (cd.escritas % CD_TRAMAS) * 4, t = n * 2352, a = CD_TRAMAS * 4 - o;
    if (t <= a) hx_copia(cd.anillo + o, tmp, t);
    else { hx_copia(cd.anillo + o, tmp, a); hx_copia(cd.anillo, tmp + a, t - a); }
  }
  cd.escritas += n * SECT;
  cd.cursor += n;
}
static void cd_rellena_si(void) { static int dentro; if (cd.sonando && !cd.con_hilo && !dentro) { dentro = 1; cd_rellena(); dentro = 0; } }
static u32 WINAPI hilo_cd(void *p) {
  (void)p;
  for (;;) {
    if (!cd.abierto) { cd.con_hilo = 0; return 0; }
    if (cd.sonando && !cd.pausado) {
      u32 antes = cd.cursor;
      cd_rellena();
      if (cd.cursor == antes) Sleep(20);
    } else Sleep(50);
  }
}
static u32 cd_abre(void) {
  mez_ini();
  if (!cd.abierto) {
    cd.anillo = hx_pedir(CD_TRAMAS * 4);
    if (!cd.anillo) return 262;
    cd.abierto = 1;
    cd.formato = F_MSF;
  }
  cd_lee_toc();
  if (!cd.con_hilo && HispaHilosDeVerdad()) {
    cd.con_hilo = 1;
    if (!CreateThread(NULL, 0x10000, hilo_cd, NULL, 0, NULL)) cd.con_hilo = 0;
  }
  return 0;
}
static void cd_cierra(void) {
  cd_para(4);                       /* MCI_NOTIFY_ABORTED */
  cd.abierto = 0;
  while (cd.con_hilo) Sleep(10);
  hx_soltar(cd.anillo);
  cd.anillo = NULL;
}
/* una posición del formato de ahora, a lba */
static u32 cd_a_lba(u32 v, int fin) {
  if (cd.formato == F_TMSF) {
    u32 t = v & 0xFF, k = t - cd.primera;
    if (t < (u32)cd.primera || k >= (u32)cd.n) return 0xFFFFFFFF;
    { u32 m = (v >> 8) & 0xFF, s = (v >> 16) & 0xFF, f = v >> 24, l = cd.lba[k] + m * 4500 + s * 75 + f;
      if (fin && !(v & 0xFFFFFF00)) l = cd.lba[k + 1];   /* «to pista»: hasta que acaba */
      return l; }
  }
  if (cd.formato == F_MSF) return de_msf(v);
  return (u32)hx_div64((u64)v * 75, 1000);
}
static u32 cd_de_lba(u32 l, int duracion) {
  if (cd.formato == F_MS) return (u32)hx_div64((u64)l * 1000, 75);
  if (duracion || cd.formato == F_MSF) { if (duracion) l += 0; return duracion ? ((l / 4500) | ((l / 75 % 60) << 8) | ((l % 75) << 16)) : msf(l); }
  { int k; for (k = cd.n - 1; k > 0 && cd.lba[k] > l; k--) ;
    l -= cd.lba[k];
    return (u32)(cd.primera + k) | ((l / 4500) << 8) | ((l / 75 % 60) << 16) | ((l % 75) << 24); }
}
static u32 cd_posicion(void) {
  if (!cd.sonando) return cd.cursor ? cd.cursor : cd.n ? cd.lba[0] : 0;
  return cd.desde + jugadas() / SECT;
}
static u32 cd_juega(u32 desde, u32 hasta, int hay_desde, int hay_hasta, HWND aviso, int avisar, int espera) {
  u32 d, h;
  if (!cd.hay_disco && !cd_lee_toc()) return 276;     /* MCIERR_DEVICE_NOT_READY */
  d = hay_desde ? cd_a_lba(desde, 0) : cd_posicion();
  h = hay_hasta ? cd_a_lba(hasta, 1) : cd.lba[cd.n];
  if (d == 0xFFFFFFFF || h == 0xFFFFFFFF || d >= h || h > cd.lba[cd.n]) return 282;   /* MCIERR_OUTOFRANGE */
  { int k; for (k = cd.n - 1; k > 0 && cd.lba[k] > d; k--) ;
    if (!cd.sonido[k]) return 262; }  /* (una pista de datos: MCIERR_HARDWARE, como Windows) */
  cd_para(2);                       /* (la de antes: MCI_NOTIFY_SUPERSEDED) */
  cd.desde = cd.cursor = d;
  cd.hasta = h;
  cd.escritas = 0;
  cd.vueltas = cd.ult_pos = 0;
  hx_cero(cd.anillo, CD_TRAMAS * 4);
  cd.voz.datos = cd.anillo;
  cd.voz.tramas = CD_TRAMAS;
  cd.voz.bits = 16;
  cd.voz.canales = 2;
  cd.voz.pos = cd.voz.frac = 0;
  cd.voz.paso = HispaMezPaso(44100);
  cd.voz.bucle = 1;
  cd.voz.vol_i = cd.voz.vol_d = 4096;
  cd.voz.fin = NULL;
  cd.sonando = 1;
  cd.pausado = 0;
  cd_rellena();                     /* (lo primero, ya) */
  cd_rellena();
  HispaMezPon(&cd.voz);
  cd.aviso = aviso;
  cd.avisar = avisar;
  if (espera) while (cd.sonando) { HispaMezBomba(); if (!cd.con_hilo) cd_rellena(); Sleep(20); }
  return 0;
}
static u32 cd_estado(u32 que, u32 pista, int con_pista, u32 *r) {
  if (!cd.hay_disco && que != 4 && que != 5 && que != 6 && que != 7) cd_lee_toc();
  switch (que) {
  case 1:                           /* MCI_STATUS_LENGTH */
    if (!cd.hay_disco) return 276;
    if (con_pista) { u32 k = pista - cd.primera; if (k >= (u32)cd.n) return 282; *r = cd_de_lba(cd.lba[k + 1] - cd.lba[k], 1); }
    else *r = cd_de_lba(cd.lba[cd.n] - cd.lba[0], 1);
    if (cd.formato == F_MS) *r = (u32)hx_div64((u64)(con_pista ? cd.lba[pista - cd.primera + 1] - cd.lba[pista - cd.primera] : cd.lba[cd.n] - cd.lba[0]) * 1000, 75);
    return 0;
  case 2:                           /* MCI_STATUS_POSITION */
    if (!cd.hay_disco) return 276;
    if (con_pista) { u32 k = pista - cd.primera; if (k >= (u32)cd.n) return 282; *r = cd_de_lba(cd.lba[k], 0); }
    else *r = cd_de_lba(cd_posicion(), 0);
    return 0;
  case 3: if (!cd.hay_disco) return 276; *r = cd.n; return 0;   /* MCI_STATUS_NUMBER_OF_TRACKS */
  case 4: *r = !cd.hay_disco ? 524 : cd.pausado ? 529 : cd.sonando ? 526 : 525; return 0;   /* MODE */
  case 5: *r = cd.hay_disco || cd_lee_toc(); return 0;           /* MEDIA_PRESENT */
  case 6: *r = cd.formato; return 0;
  case 7: *r = 1; return 0;                                      /* READY */
  case 8: { u32 l = cd_posicion(); int k; if (!cd.hay_disco) return 276; for (k = cd.n - 1; k > 0 && cd.lba[k] > l; k--) ; *r = cd.primera + k; return 0; }
  case 0x4001: {                    /* MCI_CDA_STATUS_TYPE_TRACK */
    u32 k = pista - cd.primera;
    if (!cd.hay_disco) return 276;
    if (k >= (u32)cd.n) return 282;
    *r = cd.sonido[k] ? 1088 : 1089;
    return 0;
  }
  }
  return 274;                       /* MCIERR_UNSUPPORTED_FUNCTION */
}
/* mciSendCommand de «cdaudio» */
typedef struct { u32 cb, id; const char *tipo, *elem, *alias; } MCI_OPEN_P;
typedef struct { u32 cb, desde, hasta; } MCI_PLAY_P;
typedef struct { u32 cb, ret, item, pista; } MCI_STATUS_P;
typedef struct { u32 cb, formato, audio; } MCI_SET_P;
static u32 cd_orden(u32 m, u32 f, u32 p) {
  switch (m) {
  case 0x804: cd_cierra(); return 0;                             /* MCI_CLOSE */
  case 0x806: { MCI_PLAY_P *x = (MCI_PLAY_P *)p;                  /* MCI_PLAY */
    return cd_juega(x ? x->desde : 0, x ? x->hasta : 0, x && (f & 4), x && (f & 8), x ? (HWND)x->cb : NULL, (f & 1) != 0, (f & 2) != 0); }
  case 0x807: { MCI_PLAY_P *x = (MCI_PLAY_P *)p;                  /* MCI_SEEK */
    cd_para(4);
    if (f & 0x100) cd.cursor = cd.n ? cd.lba[0] : 0;
    else if (f & 0x200) cd.cursor = cd.lba[cd.n];
    else if ((f & 8) && x) cd.cursor = cd_a_lba(x->hasta, 0);
    return 0; }
  case 0x808: cd.cursor = cd_posicion(); cd_para(4); return 0;    /* MCI_STOP */
  case 0x809: if (cd.sonando && !cd.pausado) { cd.pausado = 1; cd.voz.activa = 0; } return 0;   /* MCI_PAUSE */
  case 0x855: if (cd.sonando && cd.pausado) { cd.pausado = 0; HispaMezPon(&cd.voz); } return 0; /* MCI_RESUME */
  case 0x80D: { MCI_SET_P *x = (MCI_SET_P *)p;                    /* MCI_SET */
    if ((f & 0x400) && x) { if (x->formato != F_MS && x->formato != F_MSF && x->formato != F_TMSF) return 274; cd.formato = x->formato; }
    return 0; }
  case 0x814: { MCI_STATUS_P *x = (MCI_STATUS_P *)p;              /* MCI_STATUS */
    u32 r = 0, e;
    if (!x || !(f & 0x100)) return 274;
    if (f & 0x200) { if (!cd.hay_disco) return 276; x->ret = cd_de_lba(cd.lba[0], 0); return 0; }   /* MCI_STATUS_START */
    e = cd_estado(x->item, x->pista, (f & 0x10) != 0, &r);
    if (!e) x->ret = r;
    return e; }
  case 0x80B: { MCI_STATUS_P *x = (MCI_STATUS_P *)p;              /* MCI_GETDEVCAPS */
    if (x) x->ret = x->item == 2 ? 516 : 1;                         /* (el tipo: MCI_DEVTYPE_CD_AUDIO) */
    return 0; }
  }
  return 274;
}
static int cd_es(const char *a) { return igual(a, "cdaudio") || (cd.alias[0] && igual(a, cd.alias)); }
/* un número de una orden: «2», «2:30:10», «1:2:3:4» */
static u32 cd_num(const char *s) {
  u32 c[4] = { 0 }, n = 0;
  while (*s && n < 4) { if (*s >= '0' && *s <= '9') c[n] = c[n] * 10 + (*s - '0'); else if (*s == ':') n++; s++; }
  if (cd.formato == F_TMSF) return c[0] | c[1] << 8 | c[2] << 16 | c[3] << 24;
  if (cd.formato == F_MSF) return c[0] | c[1] << 8 | c[2] << 16;
  return c[0];
}
static void cd_texto(u32 v, u32 que, char *d, u32 n) {
  char b[24]; u32 k = 0, i;
  if (que == 4) { const char *t = v == 526 ? "playing" : v == 529 ? "paused" : v == 525 ? "stopped" : "not ready"; while (t[k] && k + 1 < n) { d[k] = t[k]; k++; } d[k] = 0; return; }
  if (que == 5 || que == 7) { const char *t = v ? "true" : "false"; while (t[k] && k + 1 < n) { d[k] = t[k]; k++; } d[k] = 0; return; }
  if (que == 0x4001) { const char *t = v == 1088 ? "audio" : "other"; while (t[k] && k + 1 < n) { d[k] = t[k]; k++; } d[k] = 0; return; }
  if ((que == 1 || que == 2) && cd.formato != F_MS) {
    u32 p[4] = { v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, v >> 24 }, np = (que == 2 && cd.formato == F_TMSF) ? 4 : 3;
    for (i = 0; i < np; i++) { b[k++] = '0' + p[i] / 10 % 10; b[k++] = '0' + p[i] % 10; if (i + 1 < np) b[k++] = ':'; }
  } else { char t[12]; int j = 0; do { t[j++] = '0' + v % 10; v /= 10; } while (v); while (j) b[k++] = t[--j]; }
  b[k] = 0;
  for (i = 0; b[i] && i + 1 < n; i++) d[i] = b[i];
  d[i] = 0;
}
static u32 cd_cadena(const char *o, const char *s, char *ret, u32 nret, HWND cb) {
  char p[64];
  if (igual(o, "open")) {
    cd.alias[0] = 0;
    while (pal(&s, p, sizeof p)) if (igual(p, "alias")) pal(&s, cd.alias, sizeof cd.alias);
    return cd_abre();
  }
  if (!cd.abierto) { u32 e = cd_abre(); if (e) return e; }   /* (abrirlo al usarlo, como Windows) */
  if (igual(o, "close")) { cd_cierra(); return 0; }
  if (igual(o, "set")) {
    while (pal(&s, p, sizeof p)) {
      if (igual(p, "format")) { pal(&s, p, sizeof p);
        if (igual(p, "tmsf")) cd.formato = F_TMSF; else if (igual(p, "msf")) cd.formato = F_MSF;
        else if (igual(p, "milliseconds") || igual(p, "ms")) cd.formato = F_MS; else return 274; }
      else if (igual(p, "door")) { pal(&s, p, sizeof p); }
    }
    return 0;
  }
  if (igual(o, "play")) {
    u32 a = 0, b = 0; int ha = 0, hb = 0, av = 0, es = 0;
    while (pal(&s, p, sizeof p)) {
      if (igual(p, "from")) { pal(&s, p, sizeof p); a = cd_num(p); ha = 1; }
      else if (igual(p, "to")) { pal(&s, p, sizeof p); b = cd_num(p); hb = 1; }
      else if (igual(p, "notify")) av = 1;
      else if (igual(p, "wait")) es = 1;
    }
    return cd_juega(a, b, ha, hb, cb, av, es);
  }
  if (igual(o, "stop")) return cd_orden(0x808, 0, 0);
  if (igual(o, "pause")) return cd_orden(0x809, 0, 0);
  if (igual(o, "resume")) return cd_orden(0x855, 0, 0);
  if (igual(o, "seek")) {
    MCI_PLAY_P x = { 0, 0, 0 };
    pal(&s, p, sizeof p);
    pal(&s, p, sizeof p);
    if (igual(p, "start")) return cd_orden(0x807, 0x100, 0);
    if (igual(p, "end")) return cd_orden(0x807, 0x200, 0);
    x.hasta = cd_num(p);
    return cd_orden(0x807, 8, (u32)&x);
  }
  if (igual(o, "status")) {
    char q[64]; u32 que = 0, pista = 0, r = 0, e; int con = 0;
    q[0] = 0;
    while (pal(&s, p, sizeof p)) {
      if (igual(p, "track")) { pal(&s, p, sizeof p); pista = cd_num(p) & 0xFF; if (cd.formato != F_TMSF) { const char *t = p; pista = 0; while (*t >= '0' && *t <= '9') pista = pista * 10 + (*t++ - '0'); } con = 1; }
      else if (igual(p, "number")) que = 3;
      else if (igual(p, "length")) que = 1;
      else if (igual(p, "position")) que = 2;
      else if (igual(p, "mode")) que = 4;
      else if (igual(p, "media")) que = 5;
      else if (igual(p, "ready")) que = 7;
      else if (igual(p, "current")) que = 8;
      else if (igual(p, "type")) que = 0x4001;
      else if (igual(p, "format")) que = 6;
    }
    if (!que) return 274;
    e = cd_estado(que, pista, con, &r);
    if (!e && ret) {
      if (que == 6) { const char *t = cd.formato == F_TMSF ? "tmsf" : cd.formato == F_MSF ? "msf" : "milliseconds"; u32 k = 0; while (t[k] && k + 1 < nret) { ret[k] = t[k]; k++; } ret[k] = 0; }
      else cd_texto(r, que, ret, nret);
    }
    return e;
  }
  return 261;
}
#endif

u32 WINAPI mciSendStringA(const char *orden, char *ret, u32 nret, HWND cb) {
  char o[16], a[128], p[128], alias[32];
  const char *s = orden;
  int k, espera = 0;
  (void)cb;
  mez_ini();
  if (ret && nret) ret[0] = 0;
  pal(&s, o, sizeof o);
  pal(&s, a, sizeof a);
#ifdef HISPA_PENTIUM
  if (cd_es(a)) return cd_cadena(o, s, ret, nret, cb);    /* (JUEGOS) el CD de sonido */
#endif
  if (igual(o, "open")) {
    alias[0] = 0;
    while (pal(&s, p, sizeof p)) {
      if (igual(p, "alias")) pal(&s, alias, sizeof alias);
      else if (igual(p, "type")) { pal(&s, p, sizeof p); if (!igual(p, "waveaudio")) return 263; }
    }
    if (!alias[0]) { int i = 0; while (a[i] && i < 31) { alias[i] = a[i]; i++; } alias[i] = 0; }
    if (igual(a, "cdaudio") || igual(a, "sequencer")) return 263;   /* MCIERR_INVALID_DEVICE_NAME */
    k = mci_abre(a, alias);
    return k >= 0 ? 0 : k == -2 ? 275 : 263;                         /* MCIERR_FILE_NOT_FOUND */
  }
  k = mci_busca(a);
  if (igual(o, "close")) {
    if (igual(a, "all")) { for (k = 0; k < NMCI; k++) if (mci[k].buf) mci_cierra(k); return 0; }
    if (k < 0) return 263;
    mci_cierra(k); return 0;
  }
  if (igual(o, "play")) {
    if (k < 0) { k = mci_abre(a, a); if (k < 0) return k == -2 ? 275 : 263; }
    while (pal(&s, p, sizeof p)) if (igual(p, "wait")) espera = 1; else if (igual(p, "repeat")) mci[k].voz.bucle = 1;
    mci[k].voz.pos = mci[k].voz.frac = 0;
    HispaMezPon(&mci[k].voz);
    HispaMezBomba();
    if (espera) while (mci[k].voz.activa) { HispaMezBomba(); Sleep(20); }
    return 0;
  }
  if (k < 0) return 263;
  if (igual(o, "stop") || igual(o, "pause")) { HispaMezQuita(&mci[k].voz); return 0; }
  if (igual(o, "resume")) { HispaMezPon(&mci[k].voz); return 0; }
  if (igual(o, "status")) {
    const char *r = mci[k].voz.activa ? "playing" : "stopped";
    u32 i = 0;
    if (ret) { while (r[i] && i + 1 < nret) { ret[i] = r[i]; i++; } ret[i] = 0; }
    return 0;
  }
  if (igual(o, "seek") || igual(o, "set")) return 0;
  return 261;                       /* MCIERR_UNRECOGNIZED_COMMAND */
}
u32 WINAPI mciSendCommandA(u32 id, u32 m, u32 f, u32 p) {
#ifdef HISPA_PENTIUM
  /* (JUEGOS) el CD de sonido: abrirlo por su tipo («cdaudio», o el número
   * MCI_DEVTYPE_CD_AUDIO) y lo demás por su número */
  if (m == 0x803 && p) {                                          /* MCI_OPEN */
    MCI_OPEN_P *x = (MCI_OPEN_P *)p;
    int es_cd = 0;
    if ((f & 0x1000) && ((u32)x->tipo & 0xFFFF) == 516) es_cd = 1;   /* MCI_OPEN_TYPE_ID */
    else if ((f & 0x2000) && !(f & 0x1000) && x->tipo && igual(x->tipo, "cdaudio")) es_cd = 1;
    if (es_cd) {
      u32 e = cd_abre();
      if (!e) x->id = CD_ID;
      if (!e && (f & 0x400) && x->alias) { int i = 0; while (x->alias[i] && i < 31) { cd.alias[i] = x->alias[i]; i++; } cd.alias[i] = 0; }
      return e;
    }
  }
  if (id == CD_ID) return cd.abierto || m == 0x804 ? cd_orden(m, f, p) : 257;
#endif
  (void)id; (void)m; (void)f; (void)p; return 263;
}
int WINAPI mciGetErrorStringA(u32 e, char *s, u32 n) {
  const char *t = e == 275 ? "No se encuentra el archivo." : e == 263 ? "Dispositivo no admitido (WAV y el CD de sonido)." :
                  e == 276 ? "No hay disco en el lector (o no se puede leer)." : e == 282 ? "Esa pista o ese momento no están en el disco." :
                  e == 257 ? "Ese dispositivo no está abierto." : "Orden MCI no admitida.";
  u32 k = 0;
  if (!s || !n) return 0;
  while (t[k] && k + 1 < n) { s[k] = t[k]; k++; }
  s[k] = 0;
  return 1;
}

/* ======================= los mandos (HSPJOY) ======================= */
typedef struct { u8 tipo, presente, ejes, botones; u16 a, b; u8 eje[6]; u8 cruceta, r; u16 bot; u8 interno[14]; } MANDO;
static int mando(u32 id, MANDO *m) { return id < 2 && HispaDXEntrada(2, id ? 0xFE : 0xFF, (u32)m); }
u32 WINAPI joyGetNumDevs(void) { return 2; }
typedef struct { u16 wMid, wPid; char szPname[32]; u32 xmin, xmax, ymin, ymax, zmin, zmax, nbot, pmin, pmax, rmin, rmax, umin, umax, vmin, vmax, caps, maxejes, nejes, maxbot; char reg[32], oem[260]; } JOYCAPSA;
u32 WINAPI joyGetDevCapsA(u32 id, JOYCAPSA *c, u32 n) {
  static const char nom[] = "Mando de Hispa-DOS (HSPJOY)";
  MANDO m;
  if (!mando(id, &m)) return 167;   /* JOYERR_UNPLUGGED */
  if (!c || n < sizeof(JOYCAPSA) - 292) return 165;
  hx_cero(c, n < sizeof(JOYCAPSA) ? n : sizeof(JOYCAPSA));
  c->wMid = 0xFF; c->wPid = m.tipo;
  hx_copia(c->szPname, nom, sizeof nom);
  c->xmax = c->ymax = c->zmax = c->rmax = c->umax = c->vmax = 65535;
  c->nbot = m.botones ? m.botones : 4; c->maxbot = 32;
  c->pmin = 10; c->pmax = 1000;
  c->nejes = m.ejes < 2 ? 2 : m.ejes; c->maxejes = 6;
  c->caps = 0x10 | (m.ejes > 2 ? 3 : 0);   /* JOYCAPS_HASPOV (y Z, R) */
  return 0;
}
static void mando_ejes(const MANDO *m, u32 e[6]) {
  int k;
  for (k = 0; k < 6; k++) e[k] = m->eje[k] * 257;
  if (m->ejes < 2 || (m->eje[0] == 128 && m->eje[1] == 128 && m->cruceta)) {   /* cruceta como palanca */
    e[0] = m->cruceta & 4 ? 0 : m->cruceta & 8 ? 65535 : 32768;
    e[1] = m->cruceta & 1 ? 0 : m->cruceta & 2 ? 65535 : 32768;
  }
}
typedef struct { u32 x, y, z, bot; } JOYINFO;
u32 WINAPI joyGetPos(u32 id, JOYINFO *j) {
  MANDO m; u32 e[6];
  if (!mando(id, &m)) return 167;
  mando_ejes(&m, e);
  j->x = e[0]; j->y = e[1]; j->z = e[2]; j->bot = m.bot & 15;
  return 0;
}
typedef struct { u32 tam, flags, x, y, z, r, u, v, bot, nbot, pov, r1, r2; } JOYINFOEX;
u32 WINAPI joyGetPosEx(u32 id, JOYINFOEX *j) {
  MANDO m; u32 e[6], k;
  if (!mando(id, &m)) return 167;
  mando_ejes(&m, e);
  j->x = e[0]; j->y = e[1]; j->z = e[2]; j->r = e[3]; j->u = e[4]; j->v = e[5];
  j->bot = m.bot; j->nbot = 0;
  for (k = 0; k < 16; k++) if (m.bot & (1u << k)) { j->nbot = k + 1; break; }
  j->pov = 0xFFFF;                  /* JOY_POVCENTERED */
  switch (m.cruceta & 15) {
  case 1: j->pov = 0; break; case 9: j->pov = 4500; break; case 8: j->pov = 9000; break; case 10: j->pov = 13500; break;
  case 2: j->pov = 18000; break; case 6: j->pov = 22500; break; case 4: j->pov = 27000; break; case 5: j->pov = 31500; break;
  }
  return 0;
}
u32 WINAPI joySetCapture(HWND h, u32 id, u32 p, int c) { (void)h; (void)id; (void)p; (void)c; return 165; }
u32 WINAPI joyReleaseCapture(u32 id) { (void)id; return 0; }
u32 WINAPI joyGetThreshold(u32 id, u32 *t) { (void)id; if (t) *t = 0; return 0; }
u32 WINAPI joySetThreshold(u32 id, u32 t) { (void)id; (void)t; return 0; }
