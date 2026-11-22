/* dinput.c - DINPUT.DLL de Hispa-486 (fase 5, docs/486/HISPA-486.md):
 * DirectInput 3 a 7. IDirectInputA, IDirectInput2A e IDirectInput7A (una
 * tabla), DirectInputCreateA/W/Ex; los dispositivos (IDirectInputDeviceA, 2A
 * y 7A, una tabla):
 *  - el teclado (GUID_SysKeyboard): las teclas que están pulsadas las apunta
 *    Hispa Ventanas por la BIOS (tec_mapa, HispaDXEntrada); estado inmediato
 *    (GetDeviceState) y por búfer (GetDeviceData, con DIPROP_BUFFERSIZE);
 *  - el ratón (GUID_SysMouse): relativo (lo que se ha movido desde la última
 *    vez; con DISCL_EXCLUSIVE el cursor vuelve al centro para que no tope con
 *    el borde) o absoluto, y sus botones;
 *  - los mandos de HSPJOY (EnumDevices los da si están): ejes, botones y
 *    cruceta (POV), con DIPROP_RANGE y DIPROP_DEADZONE.
 * Con el formato que diga el programa (SetDataFormat: c_dfDIKeyboard,
 * c_dfDIMouse, c_dfDIMouse2, c_dfDIJoystick, c_dfDIJoystick2 u otro hecho a
 * mano: se mira cada objeto). Sin hilos: los cambios se ven al preguntar.
 * Sala limpia: hecho a partir de la documentación pública del API.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "dx.h"

void *memcpy(void *d, const void *s, u32 n) { hx_copia(d, s, n); return d; }
void *memset(void *d, int c, u32 n) { u8 *p = d; while (n--) *p++ = c; return d; }

#define DIERR_NOTACQUIRED ((HRESULT)0x8007000C)
#define DIERR_NOTINITIALIZED ((HRESULT)0x80070015)
#define DIERR_DEVICENOTREG ((HRESULT)0x80040154)
#define DIERR_OBJECTNOTFOUND ((HRESULT)0x80070002)
#define DIERR_ACQUIRED ((HRESULT)0x800700AA)
#define DIERR_UNSUPPORTED E_NOTIMPL
#define DI_BUFFEROVERFLOW 1
#define DI_NOEFFECT 1
#define DIDFT_AXIS 3
#define DIDFT_BUTTON 0x0C
#define DIDFT_POV 0x10
#define DIDFT_OPTIONAL 0x80000000

enum { TECLADO = 1, RATON, MANDO };
/* lo que hay en el formato: un objeto -> qué es */
enum { O_TECLA = 1, O_EJE, O_BOTON, O_POV };
typedef struct { u32 ofs, tipo, n; } OBJ;
typedef struct { u32 ofs, dato, hora, seq; } EVENTO;
typedef struct DEV {
  void **vt;
  int refs;
  int clase, mando;
  int adquirido, exclusivo;
  HWND hwnd;
  u32 tam_datos;
  OBJ *obj; u32 nobj;
  /* el estado de antes (para el búfer y lo relativo) */
  u8 teclas[256];
  u8 rapidas[128];                  /* las pulsadas y soltadas entre dos lecturas */
  i32 ejes[8]; u8 botones[32]; u32 pov;
  int rx, ry, rabs;                 /* ratón: dónde estaba */
  i32 acum[3];                      /* lo movido desde el último GetDeviceState */
  int relativo;
  i32 rmin, rmax, zona;
  EVENTO *buf; u32 nbuf, cab, cola, perdidos, seq;
} DEV;
typedef struct { void **vt; int refs; u32 ver; } DI;

static const u8 extendidas[] = { 0x1C, 0x1D, 0x35, 0x38, 0x47, 0x48, 0x49, 0x4B, 0x4D, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x5B, 0x5C, 0x5D };
typedef struct { u8 tipo, presente, ejes, botones; u16 a, b; u8 eje[6]; u8 cruceta, r; u16 bot; u8 interno[14]; } MANDOE;

static u32 guid_eje(const GUID *g) {     /* GUID_XAxis... -> número de eje (0..7), o -1 */
  if (!g) return 0xFFFFFFFF;
  switch (g->d1) {
  case 0xA36D02E0: return 0; case 0xA36D02E1: return 1; case 0xA36D02E2: return 2;
  case 0xA36D02F4: return 3; case 0xA36D02F5: return 4; case 0xA36D02E3: return 5;
  case 0xA36D02E4: return 6;
  }
  return 0xFFFFFFFF;
}

/* ---------------- leer el dispositivo ---------------- */
static void evento(DEV *d, u32 ofs, u32 dato) {
  u32 sig;
  if (!d->nbuf) return;
  sig = (d->cab + 1) % (d->nbuf + 1);
  if (sig == d->cola) { d->perdidos = 1; return; }
  d->buf[d->cab].ofs = ofs; d->buf[d->cab].dato = dato;
  d->buf[d->cab].hora = timeGetTime(); d->buf[d->cab].seq = ++d->seq;
  d->cab = sig;
}
static u32 ofs_de(DEV *d, u32 tipo, u32 n) {
  u32 k;
  for (k = 0; k < d->nobj; k++) if (d->obj[k].tipo == tipo && d->obj[k].n == n) return d->obj[k].ofs;
  return 0xFFFFFFFF;
}
static void leer(DEV *d) {
  u32 k, o;
  if (d->clase == TECLADO) {
    const u8 *m = (const u8 *)HispaDXEntrada(0, 0, 0);
    u8 t[256], pu[128];
    hx_cero(t, 256);
    if (!HispaDXEntrada(4, (u32)pu, 0)) hx_cero(pu, 128);
    for (k = 1; k < 128; k++) {
      t[k] = m[k] ? 0x80 : 0;
      if (pu[k] && !t[k] && !d->teclas[k]) {   /* pulsada y soltada sin verla: las dos cosas */
        u32 o2 = ofs_de(d, O_TECLA, k);
        if (o2 != 0xFFFFFFFF) { evento(d, o2, 0x80); evento(d, o2, 0); }
        if (k == 0x1C || k == 0x1D || k == 0x35 || k == 0x38 || (k >= 0x47 && k <= 0x53) || k == 0x5B || k == 0x5C || k == 0x5D)
          if ((o2 = ofs_de(d, O_TECLA, k | 0x80)) != 0xFFFFFFFF) { evento(d, o2, 0x80); evento(d, o2, 0); }
        d->rapidas[k] = 1;
      }
    }
    for (k = 0; k < sizeof extendidas; k++) t[extendidas[k] | 0x80] = t[extendidas[k]];
    for (k = 0; k < 256; k++) if (t[k] != d->teclas[k]) {
      d->teclas[k] = t[k];
      if ((o = ofs_de(d, O_TECLA, k)) != 0xFFFFFFFF) evento(d, o, t[k]);
    }
  } else if (d->clase == RATON) {
    int r[3], dx, dy, m[2];
    HispaDXEntrada(1, (u32)r, 0);
    dx = r[0] - d->rx; dy = r[1] - d->ry;
    d->rx = r[0]; d->ry = r[1];
    if (d->relativo && HispaDXEntrada(5, (u32)m, 0)) {
      /* (JUEGOS) Hispa-Pentium da lo que se ha movido de verdad: sin topar
       * con el borde ni perder los puntos al pasar al modo del juego */
      dx = m[0]; dy = m[1];
    } else if (d->exclusivo && d->adquirido) {   /* que no tope con el borde */
      static HXPANT p;
      int cx, cy;
      if (!p.ancho) { HispaDXPantalla(7, (u32)&p, 0, 0); if (!p.ancho) { p.ancho = 640; p.alto = 480; } }
      cx = p.ancho / 2; cy = p.alto / 2;
      if ((r[0] < 64 || r[1] < 64 || r[0] > (int)p.ancho - 64 || r[1] > (int)p.alto - 64) && SetCursorPos(cx, cy)) { d->rx = cx; d->ry = cy; }
    }
    d->acum[0] += dx; d->acum[1] += dy;
    if (dx && (o = ofs_de(d, O_EJE, 0)) != 0xFFFFFFFF) evento(d, o, dx);
    if (dy && (o = ofs_de(d, O_EJE, 1)) != 0xFFFFFFFF) evento(d, o, dy);
    for (k = 0; k < 3; k++) {
      u8 b = (r[2] >> k) & 1 ? 0x80 : 0;
      if (b != d->botones[k]) { d->botones[k] = b; if ((o = ofs_de(d, O_BOTON, k)) != 0xFFFFFFFF) evento(d, o, b); }
    }
  } else if (d->clase == MANDO) {
    MANDOE m;
    i32 e[8];
    u32 pov = 0xFFFFFFFF;
    if (!HispaDXEntrada(2, d->mando, (u32)&m)) hx_cero(&m, sizeof m), m.eje[0] = m.eje[1] = 128;
    for (k = 0; k < 6; k++) {
      i32 v = m.eje[k];
      if (k < 2 && m.cruceta && m.eje[0] == 128 && m.eje[1] == 128)   /* sin palanca: la cruceta */
        v = k == 0 ? (m.cruceta & 4 ? 0 : m.cruceta & 8 ? 255 : 128) : (m.cruceta & 1 ? 0 : m.cruceta & 2 ? 255 : 128);
      if (k < 2 && d->zona && (u32)((v > 128 ? v - 128 : 128 - v) * 10000) < d->zona * 128) v = 128;
      e[k] = d->rmin + (i32)((i64)v * (d->rmax - d->rmin) / 255);
    }
    e[6] = e[7] = d->rmin;
    switch (m.cruceta & 15) {
    case 1: pov = 0; break; case 9: pov = 4500; break; case 8: pov = 9000; break; case 10: pov = 13500; break;
    case 2: pov = 18000; break; case 6: pov = 22500; break; case 4: pov = 27000; break; case 5: pov = 31500; break;
    }
    for (k = 0; k < 8; k++) if (e[k] != d->ejes[k]) { d->ejes[k] = e[k]; if ((o = ofs_de(d, O_EJE, k)) != 0xFFFFFFFF) evento(d, o, e[k]); }
    if (pov != d->pov) { d->pov = pov; if ((o = ofs_de(d, O_POV, 0)) != 0xFFFFFFFF) evento(d, o, pov); }
    for (k = 0; k < 16; k++) {
      u8 b = (m.bot >> k) & 1 ? 0x80 : 0;
      if (b != d->botones[k]) { d->botones[k] = b; if ((o = ofs_de(d, O_BOTON, k)) != 0xFFFFFFFF) evento(d, o, b); }
    }
  }
}

/* ---------------- IDirectInputDevice ---------------- */
static HRESULT WINAPI V_QueryInterface(DEV *d, const GUID *g, void **p) {
  if (!p) return E_POINTER;
  if (hx_guid(g, 0) || (g && (g->d1 & 0xFFFFFFF0) == 0x5944E680) || hx_guid(g, 0x57D7C6BC) || hx_guid(g, 0x57D7C6BD)) { d->refs++; *p = d; return S_OK; }
  *p = NULL;
  return E_NOINTERFACE;
}
static u32 WINAPI V_AddRef(DEV *d) { return ++d->refs; }
static u32 WINAPI V_Release(DEV *d) {
  if (--d->refs > 0) return d->refs;
  hx_soltar(d->obj); hx_soltar(d->buf); hx_soltar(d);
  return 0;
}
typedef struct { u32 tam, flags, tipo, ejes, botones, povs, ff1, ff2, ffv, ffh, ffd; } DIDEVCAPS;
static u32 tipo_dev(DEV *d) { return d->clase == TECLADO ? 3 | (4 << 8) : d->clase == RATON ? 2 | (1 << 8) : 4 | (2 << 8); }
static HRESULT WINAPI V_GetCapabilities(DEV *d, DIDEVCAPS *c) {
  MANDOE m;
  if (!c || c->tam < 24) return E_INVALIDARG;
  hx_cero((u8 *)c + 4, c->tam - 4 > 40 ? 40 : c->tam - 4);
  c->flags = 1;                     /* DIDC_ATTACHED */
  c->tipo = tipo_dev(d);
  if (d->clase == TECLADO) c->botones = 128;
  else if (d->clase == RATON) { c->ejes = 3; c->botones = 3; }
  else { HispaDXEntrada(2, d->mando, (u32)&m); c->ejes = m.ejes < 2 ? 2 : m.ejes; c->botones = m.botones ? m.botones : 4; c->povs = 1; }
  return S_OK;
}
typedef struct { u32 tam; GUID g; u32 ofs, tipo, flags; char nombre[260]; } DIOBJINST;
typedef int WINAPI (*ENUMOBJ)(DIOBJINST *, void *);
static HRESULT WINAPI V_EnumObjects(DEV *d, ENUMOBJ cb, void *ctx, u32 fl) {
  DIOBJINST o;
  u32 k, n;
  static const char *ejes[] = { "Eje X", "Eje Y", "Rueda", "Eje Rx", "Eje Ry", "Eje Rz" };
  (void)fl;
  if (!cb) return E_INVALIDARG;
  n = d->clase == RATON ? 3 : d->clase == MANDO ? 2 : 0;
  for (k = 0; k < n; k++) {
    hx_cero(&o, sizeof o);
    o.tam = 296; o.g.d1 = 0xA36D02E0 + k; o.g.d2 = 0xC9F3; o.g.d3 = 0x11CF;
    o.ofs = k * 4; o.tipo = (d->clase == RATON ? 1 : 2) | (k << 8);
    { const char *s = ejes[k]; int i = 0; while (s[i]) { o.nombre[i] = s[i]; i++; } }
    if (!cb(&o, ctx)) return S_OK;
  }
  n = d->clase == TECLADO ? 0 : d->clase == RATON ? 3 : 8;
  for (k = 0; k < n; k++) {
    hx_cero(&o, sizeof o);
    o.tam = 296; o.g.d1 = 0xA36D02F0; o.g.d2 = 0xC9F3; o.g.d3 = 0x11CF;
    o.ofs = (d->clase == RATON ? 12 : 48) + k; o.tipo = 4 | (k << 8);
    o.nombre[0] = 'B'; o.nombre[1] = 'o'; o.nombre[2] = 't'; o.nombre[3] = 0xF3; o.nombre[4] = 'n'; o.nombre[5] = ' '; o.nombre[6] = '1' + k;
    if (!cb(&o, ctx)) return S_OK;
  }
  return S_OK;
}
typedef struct { u32 tam, cab, obj, como; } DIPROPHEADER;
static HRESULT WINAPI V_GetProperty(DEV *d, const GUID *g, DIPROPHEADER *h) {
  u32 p = (u32)g, *v = (u32 *)(h + 1);
  if (!h) return E_INVALIDARG;
  if (p == 1) { v[0] = d->nbuf; return S_OK; }
  if (p == 2) { v[0] = d->relativo ? 1 : 0; return S_OK; }
  if (p == 4) { v[0] = d->rmin; v[1] = d->rmax; return S_OK; }
  if (p == 5) { v[0] = d->zona; return S_OK; }
  if (p == 6) { v[0] = 10000; return S_OK; }
  if (p == 3) { v[0] = 1; return S_OK; }
  return DIERR_UNSUPPORTED;
}
static HRESULT WINAPI V_SetProperty(DEV *d, const GUID *g, const DIPROPHEADER *h) {
  u32 p = (u32)g;
  const u32 *v = (const u32 *)(h + 1);
  if (!h) return E_INVALIDARG;
  if (p == 1) {                     /* DIPROP_BUFFERSIZE */
    if (d->adquirido) return DIERR_ACQUIRED;
    hx_soltar(d->buf); d->buf = NULL; d->nbuf = 0; d->cab = d->cola = 0;
    if (v[0]) { u32 n = v[0] > 4096 ? 4096 : v[0]; if (!(d->buf = hx_pedir((n + 1) * sizeof(EVENTO)))) return E_OUTOFMEMORY; d->nbuf = n; }
    return S_OK;
  }
  if (p == 2) { d->relativo = v[0] == 1; return S_OK; }
  if (p == 4) { d->rmin = (i32)v[0]; d->rmax = (i32)v[1]; return S_OK; }
  if (p == 5) { d->zona = v[0]; return S_OK; }
  if (p == 6 || p == 3) return S_OK;
  return DIERR_UNSUPPORTED;
}
static HRESULT WINAPI V_Acquire(DEV *d) {
  if (!d->obj) return E_INVALIDARG;   /* (sin formato) */
  if (d->adquirido) return S_FALSE;
  d->adquirido = 1;
  leer(d);                          /* lo de ahora no es un cambio */
  d->cab = d->cola = 0;
  d->acum[0] = d->acum[1] = d->acum[2] = 0;
  return S_OK;
}
static HRESULT WINAPI V_Unacquire(DEV *d) { if (!d->adquirido) return S_FALSE; d->adquirido = 0; return S_OK; }
static HRESULT WINAPI V_GetDeviceState(DEV *d, u32 n, u8 *s) {
  u32 k;
  if (!d->adquirido) return DIERR_NOTACQUIRED;
  if (!s || n != d->tam_datos) return E_INVALIDARG;
  leer(d);
  hx_cero(s, n);
  for (k = 0; k < d->nobj; k++) {
    OBJ *o = &d->obj[k];
    if (o->tipo == O_TECLA) s[o->ofs] = d->teclas[o->n & 255] | (d->rapidas[o->n & 0x7F] ? 0x80 : 0);
    else if (o->tipo == O_BOTON) s[o->ofs] = d->botones[o->n & 31];
    else if (o->ofs + 4 <= n) {
      i32 v;
      if (o->tipo == O_POV) v = (i32)d->pov;
      else if (d->clase == RATON) v = o->n < 3 ? (d->relativo ? d->acum[o->n] : (o->n == 0 ? d->rx : o->n == 1 ? d->ry : 0)) : 0;
      else v = d->ejes[o->n & 7];
      *(i32 *)(s + o->ofs) = v;
    }
  }
  if (d->clase == RATON) d->acum[0] = d->acum[1] = d->acum[2] = 0;
  if (d->clase == TECLADO) hx_cero(d->rapidas, 128);
  return S_OK;
}
static HRESULT WINAPI V_GetDeviceData(DEV *d, u32 tam, u8 *dat, u32 *n, u32 fl) {
  u32 k = 0, c, max;
  HRESULT r = S_OK;
  if (!d->adquirido) return DIERR_NOTACQUIRED;
  if (!n) return E_INVALIDARG;
  if (!d->nbuf) return (HRESULT)0x80040207;   /* DIERR_NOTBUFFERED */
  leer(d);
  if (d->perdidos) { r = DI_BUFFEROVERFLOW; d->perdidos = 0; }
  max = *n;
  c = d->cola;
  while (c != d->cab && k < max) {
    if (dat) { hx_cero(dat + k * tam, tam); hx_copia(dat + k * tam, &d->buf[c], tam < 16 ? tam : 16); }
    c = (c + 1) % (d->nbuf + 1);
    k++;
  }
  if (!(fl & 1)) d->cola = c;       /* DIGDD_PEEK: se dejan */
  *n = k;
  return r;
}
typedef struct { const GUID *g; u32 ofs, tipo, flags; } DIOBJFMT;
typedef struct { u32 tam, tam_obj, flags, tam_datos, nobj; DIOBJFMT *obj; } DIDATAFORMAT;
static HRESULT WINAPI V_SetDataFormat(DEV *d, const DIDATAFORMAT *f) {
  u32 k, n = 0, sigbot = 0, sigeje = 0, sigpov = 0;
  OBJ *o;
  if (!f || f->tam_obj < 16) return E_INVALIDARG;
  if (d->adquirido) return DIERR_ACQUIRED;
  o = hx_pedir((f->nobj + 1) * sizeof(OBJ));
  if (!o) return E_OUTOFMEMORY;
  for (k = 0; k < f->nobj; k++) {
    const DIOBJFMT *x = (const DIOBJFMT *)((const u8 *)f->obj + k * f->tam_obj);
    u32 inst = (x->tipo >> 8) & 0xFFFF, e = guid_eje(x->g);
    int cualquiera = inst == 0xFFFF;
    if (x->ofs >= f->tam_datos) continue;
    if (d->clase == TECLADO) {
      if (x->tipo & DIDFT_BUTTON || hx_guid(x->g, 0x55728220)) { o[n].ofs = x->ofs; o[n].tipo = O_TECLA; o[n].n = cualquiera ? x->ofs : inst; n++; }
      continue;
    }
    if (hx_guid(x->g, 0xA36D02F2) || (!x->g && (x->tipo & DIDFT_POV))) {
      if (sigpov >= 1 && cualquiera) continue;
      o[n].ofs = x->ofs; o[n].tipo = O_POV; o[n].n = cualquiera ? sigpov++ : inst; n++;
    } else if (e != 0xFFFFFFFF || (!x->g && (x->tipo & DIDFT_AXIS))) {
      if (e == 0xFFFFFFFF) e = cualquiera ? sigeje++ : inst;
      if (d->clase == RATON && e > 2) continue;
      o[n].ofs = x->ofs; o[n].tipo = O_EJE; o[n].n = e; n++;
    } else if (hx_guid(x->g, 0xA36D02F0) || (!x->g && (x->tipo & DIDFT_BUTTON))) {
      u32 b = cualquiera ? sigbot++ : inst;
      if (b >= (d->clase == RATON ? 8u : 32u)) continue;
      o[n].ofs = x->ofs; o[n].tipo = O_BOTON; o[n].n = b; n++;
    }
  }
  hx_soltar(d->obj);
  d->obj = o; d->nobj = n; d->tam_datos = f->tam_datos;
  d->relativo = d->clase == RATON && !(f->flags & 1);   /* DIDF_ABSAXIS = 1 */
  return S_OK;
}
static HRESULT WINAPI V_SetEventNotification(DEV *d, HANDLE e) { (void)d; (void)e; return S_OK; }
static HRESULT WINAPI V_SetCooperativeLevel(DEV *d, HWND h, u32 f) {
  d->hwnd = h;
  d->exclusivo = (f & 1) != 0;      /* DISCL_EXCLUSIVE */
  return S_OK;
}
static HRESULT WINAPI V_GetObjectInfo(DEV *d, DIOBJINST *o, u32 obj, u32 como) {
  (void)d; (void)como;
  if (!o) return E_INVALIDARG;
  hx_cero((u8 *)o + 4, o->tam > 4 ? o->tam - 4 : 0);
  o->ofs = obj; o->tipo = 4;
  return S_OK;
}
typedef struct { u32 tam; GUID inst, prod; u32 tipo; char nombre[260], producto[260]; GUID ff; u16 uso_pag, uso; } DIDEVINST;
static void instancia(int clase, int mando, DIDEVINST *x) {
  static const char *nom[] = { "", "Teclado", "Rat\xF3n", "Mando de Hispa-DOS" };
  u32 t = x->tam > sizeof(DIDEVINST) ? sizeof(DIDEVINST) : x->tam;
  const char *s = nom[clase];
  int i = 0;
  hx_cero((u8 *)x + 4, t - 4);
  if (clase == MANDO) { x->inst.d1 = 0x4A4F5930 + mando; x->inst.d2 = 0x4853; x->prod.d1 = 0x4A4F5930; x->prod.d2 = 0x4853; x->tipo = 4 | (2 << 8); }
  else { x->inst.d1 = clase == RATON ? 0x6F1D2B60 : 0x6F1D2B61; x->inst.d2 = 0xD5A0; x->inst.d3 = 0x11CF;
    x->inst.d4[0] = 0xBF; x->inst.d4[1] = 0xC7; x->inst.d4[2] = 0x44; x->inst.d4[3] = 0x45; x->inst.d4[4] = 0x53; x->inst.d4[5] = 0x54;
    x->prod = x->inst; x->tipo = clase == RATON ? 2 | (1 << 8) : 3 | (4 << 8); }
  while (s[i]) { x->nombre[i] = x->producto[i] = s[i]; i++; }
}
static HRESULT WINAPI V_GetDeviceInfo(DEV *d, DIDEVINST *x) {
  if (!x || x->tam < 560) return E_INVALIDARG;
  instancia(d->clase, d->mando, x);
  return S_OK;
}
static HRESULT WINAPI V_RunControlPanel(DEV *d, HWND h, u32 f) { (void)d; (void)h; (void)f; return S_OK; }
static HRESULT WINAPI V_Initialize(DEV *d, HANDLE i, u32 v, const GUID *g) { (void)d; (void)i; (void)v; (void)g; return S_OK; }
static HRESULT WINAPI V_CreateEffect(DEV *d, const GUID *g, void *e, void **p, void *u) { (void)d; (void)g; (void)e; (void)u; if (p) *p = NULL; return DIERR_UNSUPPORTED; }
static HRESULT WINAPI V_EnumEffects(DEV *d, void *cb, void *ctx, u32 t) { (void)d; (void)cb; (void)ctx; (void)t; return S_OK; }
static HRESULT WINAPI V_GetEffectInfo(DEV *d, void *e, const GUID *g) { (void)d; (void)e; (void)g; return DIERR_UNSUPPORTED; }
static HRESULT WINAPI V_GetForceFeedbackState(DEV *d, u32 *s) { (void)d; (void)s; return DIERR_UNSUPPORTED; }
static HRESULT WINAPI V_SendForceFeedbackCommand(DEV *d, u32 f) { (void)d; (void)f; return DIERR_UNSUPPORTED; }
static HRESULT WINAPI V_EnumCreatedEffectObjects(DEV *d, void *cb, void *ctx, u32 f) { (void)d; (void)cb; (void)ctx; (void)f; return S_OK; }
static HRESULT WINAPI V_Escape(DEV *d, void *e) { (void)d; (void)e; return DIERR_UNSUPPORTED; }
static HRESULT WINAPI V_Poll(DEV *d) { if (!d->adquirido) return DIERR_NOTACQUIRED; leer(d); return S_OK; }
static HRESULT WINAPI V_SendDeviceData(DEV *d, u32 t, void *x, u32 *n, u32 f) { (void)d; (void)t; (void)x; (void)n; (void)f; return DIERR_UNSUPPORTED; }
static HRESULT WINAPI V_EnumEffectsInFile(DEV *d, const char *a, void *cb, void *ctx, u32 f) { (void)d; (void)a; (void)cb; (void)ctx; (void)f; return DIERR_UNSUPPORTED; }
static HRESULT WINAPI V_WriteEffectToFile(DEV *d, const char *a, u32 n, void *e, u32 f) { (void)d; (void)a; (void)n; (void)e; (void)f; return DIERR_UNSUPPORTED; }
static void *tabla_dev[] = { V_QueryInterface, V_AddRef, V_Release, V_GetCapabilities, V_EnumObjects, V_GetProperty,
  V_SetProperty, V_Acquire, V_Unacquire, V_GetDeviceState, V_GetDeviceData, V_SetDataFormat, V_SetEventNotification,
  V_SetCooperativeLevel, V_GetObjectInfo, V_GetDeviceInfo, V_RunControlPanel, V_Initialize,
  V_CreateEffect, V_EnumEffects, V_GetEffectInfo, V_GetForceFeedbackState, V_SendForceFeedbackCommand,
  V_EnumCreatedEffectObjects, V_Escape, V_Poll, V_SendDeviceData, V_EnumEffectsInFile, V_WriteEffectToFile };

/* ---------------- IDirectInput ---------------- */
static int hay_mando(int n) { MANDOE m; return HispaDXEntrada(2, n, (u32)&m); }
static HRESULT WINAPI I_QueryInterface(DI *i, const GUID *g, void **p) {
  if (!p) return E_POINTER;
  if (hx_guid(g, 0) || hx_guid(g, 0x89521360) || hx_guid(g, 0x89521361) || hx_guid(g, 0x5944E662) || hx_guid(g, 0x5944E663)
      || hx_guid(g, 0x9A4CB684) || hx_guid(g, 0x9A4CB685)) { i->refs++; *p = i; return S_OK; }
  *p = NULL;
  return E_NOINTERFACE;
}
static u32 WINAPI I_AddRef(DI *i) { return ++i->refs; }
static u32 WINAPI I_Release(DI *i) { if (--i->refs > 0) return i->refs; hx_soltar(i); return 0; }
static HRESULT WINAPI I_CreateDevice(DI *i, const GUID *g, DEV **p, void *u) {
  DEV *d;
  int clase, mando = 0;
  (void)i;
  if (!p || !g) return E_INVALIDARG;
  *p = NULL;
  if (u) return CLASS_E_NOAGGREGATION;
  if (g->d1 == 0x6F1D2B61) clase = TECLADO;
  else if (g->d1 == 0x6F1D2B60) clase = RATON;
  else if ((g->d1 & 0xFFFFFFF8) == 0x4A4F5930) { clase = MANDO; mando = g->d1 & 7; if (!hay_mando(mando)) return DIERR_DEVICENOTREG; }
  else return DIERR_DEVICENOTREG;
  if (!(d = hx_pedir(sizeof(DEV)))) return E_OUTOFMEMORY;
  d->vt = tabla_dev; d->refs = 1; d->clase = clase; d->mando = mando;
  d->rmin = 0; d->rmax = 65535;
  d->pov = 0xFFFFFFFF;
  *p = d;
  return S_OK;
}
typedef int WINAPI (*ENUMDEV)(DIDEVINST *, void *);
static HRESULT WINAPI I_EnumDevices(DI *i, u32 tipo, ENUMDEV cb, void *ctx, u32 fl) {
  DIDEVINST x;
  int k;
  (void)fl;
  if (!cb) return E_INVALIDARG;
  tipo &= 0xFF;
  x.tam = i->ver >= 0x500 ? 580 : 560;
  if (!tipo || tipo == 2 || tipo == 0x12) { instancia(RATON, 0, &x); if (!cb(&x, ctx)) return S_OK; }
  if (!tipo || tipo == 3 || tipo == 0x13) { instancia(TECLADO, 0, &x); if (!cb(&x, ctx)) return S_OK; }
  if (!tipo || tipo == 4 || tipo == 0x14 || tipo == 0x15)
    for (k = 0; k < 6; k++) if (hay_mando(k)) { instancia(MANDO, k, &x); if (!cb(&x, ctx)) return S_OK; }
  return S_OK;
}
static HRESULT WINAPI I_GetDeviceStatus(DI *i, const GUID *g) {
  (void)i;
  if (!g) return E_INVALIDARG;
  if (g->d1 == 0x6F1D2B60 || g->d1 == 0x6F1D2B61) return S_OK;
  if ((g->d1 & 0xFFFFFFF8) == 0x4A4F5930) return hay_mando(g->d1 & 7) ? S_OK : S_FALSE;
  return DIERR_DEVICENOTREG;
}
static HRESULT WINAPI I_RunControlPanel(DI *i, HWND h, u32 f) { (void)i; (void)h; (void)f; return S_OK; }
static HRESULT WINAPI I_Initialize(DI *i, HANDLE h, u32 v) { (void)h; i->ver = v; return S_OK; }
static HRESULT WINAPI I_FindDevice(DI *i, const GUID *c, const char *n, GUID *g) { (void)i; (void)c; (void)n; (void)g; return DIERR_DEVICENOTREG; }
static HRESULT WINAPI I_CreateDeviceEx(DI *i, const GUID *g, const GUID *iid, void **p, void *u) { (void)iid; return I_CreateDevice(i, g, (DEV **)p, u); }
static void *tabla_di[] = { I_QueryInterface, I_AddRef, I_Release, I_CreateDevice, I_EnumDevices, I_GetDeviceStatus,
  I_RunControlPanel, I_Initialize, I_FindDevice, I_CreateDeviceEx };

HRESULT WINAPI DirectInputCreateA(HANDLE inst, u32 ver, DI **p, void *u) {
  DI *i;
  (void)inst;
  if (!p) return E_INVALIDARG;
  *p = NULL;
  if (u) return CLASS_E_NOAGGREGATION;
  if (!(i = hx_pedir(sizeof(DI)))) return E_OUTOFMEMORY;
  i->vt = tabla_di; i->refs = 1; i->ver = ver;
  *p = i;
  return S_OK;
}
HRESULT WINAPI DirectInputCreateW(HANDLE inst, u32 ver, DI **p, void *u) { return DirectInputCreateA(inst, ver, p, u); }
HRESULT WINAPI DirectInputCreateEx(HANDLE inst, u32 ver, const GUID *iid, void **p, void *u) { (void)iid; return DirectInputCreateA(inst, ver, (DI **)p, u); }
HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
HRESULT WINAPI DllGetClassObject(const GUID *a, const GUID *b, void **p) { (void)a; (void)b; if (p) *p = NULL; return 0x80040111; }
