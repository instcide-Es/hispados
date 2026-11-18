/* ddraw.c - DDRAW.DLL de Hispa-486 (fase 5, docs/486/HISPA-486.md):
 * DirectDraw 1 a 7 sin aceleradora, sobre la pantalla de Hispa Ventanas.
 *  - IDirectDraw, IDirectDraw2, IDirectDraw4 e IDirectDraw7 (un objeto, cuatro
 *    caras); IDirectDrawSurface 1, 2, 3, 4 y 7 (una tabla, en el orden de la
 *    7, que vale para todas); IDirectDrawPalette; IDirectDrawClipper.
 *  - Pantalla completa (DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE): el programa toma
 *    la pantalla de Hispa Ventanas (HispaDXPantalla) y la devuelve al acabar,
 *    con RestoreDisplayMode o si su ventana deja de ser la activa (entonces
 *    sus superficies se «pierden»: DDERR_SURFACELOST hasta Restore). El modo
 *    que pide (SetDisplayMode: 320×200, 320×240, 640×400, 640×480, 800×600...
 *    en 8, 16, 24 o 32 bits) no cambia el modo de la tarjeta: sus superficies
 *    son de ese tamaño y ese formato en memoria, y al presentar (Flip, Blt o
 *    Unlock en la primaria) se pasan a la pantalla de Hispa Ventanas
 *    agrandadas por un número entero de veces y centradas (o reducidas si no
 *    caben), convirtiendo el formato si hace falta. Los de 8 bits en una
 *    pantalla de 8 bits ponen su paleta en el DAC tal cual; los de 16 o 32
 *    bits en una de 8, con una paleta 3-3-2.
 *  - En una ventana (DDSCL_NORMAL): la primaria es la pantalla; lo que se
 *    copia a ella va a la ventana del recortador (o la del programa) con
 *    StretchDIBits del GDI de Hispa Ventanas.
 *  - Blt (estirar, rellenar con color, claves de color de origen y de
 *    destino), BltFast, Lock/Unlock, Flip (con 1 o más búferes traseros),
 *    paletas de 8 bits, GetDC/ReleaseDC (un DIB del GDI de la fase 3d),
 *    WaitForVerticalBlank, EnumDisplayModes, GetCaps, GetDisplayMode...
 *  Direct3D no está (QueryInterface dice que no).
 * Sala limpia: hecho a partir de la documentación pública del API.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "dx.h"

void *memcpy(void *d, const void *s, u32 n) { hx_copia(d, s, n); return d; }
static u32 hx_tr(const char *n, u32 r) { hx_t(n, r); return r; }
void *memset(void *d, int c, u32 n) { u8 *p = d; while (n--) *p++ = c; return d; }

#define DDH(n) ((HRESULT)(0x88760000 | (n)))
#define DDERR_ALREADYINITIALIZED DDH(5)
#define DDERR_INVALIDMODE DDH(120)
#define DDERR_INVALIDPIXELFORMAT DDH(145)
#define DDERR_INVALIDRECT DDH(150)
#define DDERR_NOEXCLUSIVEMODE DDH(225)
#define DDERR_NOTFOUND DDH(255)
#define DDERR_SURFACEBUSY DDH(430)
#define DDERR_SURFACELOST DDH(450)
#define DDERR_NOTFLIPPABLE DDH(582)
#define DDERR_NOTLOCKED DDH(584)
#define DDERR_UNSUPPORTEDMODE DDH(590)
#define DDERR_NOPALETTEATTACHED DDH(575)
#define DDERR_CANTCREATEDC DDH(577)
#define DDERR_NOCLIPLIST DDH(205)
#define DDERR_NOCOLORKEY DDH(215)
#define DDERR_NODC DDH(586)
#define DDERR_NOTPALETTIZED DDH(589)
#define DDERR_NOOVERLAYHW DDH(260)

#define DDSD_CAPS 1
#define DDSD_HEIGHT 2
#define DDSD_WIDTH 4
#define DDSD_PITCH 8
#define DDSD_BACKBUFFERCOUNT 0x20
#define DDSD_LPSURFACE 0x800
#define DDSD_PIXELFORMAT 0x1000
#define DDSD_CKDESTBLT 0x4000
#define DDSD_CKSRCBLT 0x10000
#define DDSD_REFRESHRATE 0x40000
#define DDSCAPS_BACKBUFFER 4
#define DDSCAPS_COMPLEX 8
#define DDSCAPS_FLIP 0x10
#define DDSCAPS_FRONTBUFFER 0x20
#define DDSCAPS_OFFSCREENPLAIN 0x40
#define DDSCAPS_PRIMARYSURFACE 0x200
#define DDSCAPS_SYSTEMMEMORY 0x800
#define DDSCAPS_VIDEOMEMORY 0x4000
#define DDSCAPS_VISIBLE 0x8000
#define DDSCAPS_ZBUFFER 0x20000
#define DDSCAPS_LOCALVIDMEM 0x10000000
#define DDPF_PALETTEINDEXED8 0x20
#define DDPF_RGB 0x40
#define DDSCL_FULLSCREEN 1
#define DDSCL_NORMAL 8
#define DDSCL_EXCLUSIVE 0x10

typedef struct { u32 tam, flags, fourcc, bits, r, g, b, a; } DDPIXELFORMAT;
typedef struct { u32 lo, hi; } DDCOLORKEY;
typedef struct {                    /* DDSURFACEDESC (108) y DDSURFACEDESC2 (124) */
  u32 tam, flags, alto, ancho; i32 paso; u32 traseros, refresco, alfa, res; void *mem;
  DDCOLORKEY ck_dov, ck_dblt, ck_sov, ck_sblt;
  DDPIXELFORMAT pf;
  u32 caps[4];                      /* (en la de 108, solo caps[0]) */
  u32 etapa;
} DDSD;
typedef struct { u32 tam, ddfx, rop, ddrop, giro, zop, zlo, zhi, zbase, zbits, zdst, zsbits, zsrc,
  abits, aedge, res, adbits, adst, asbits, asrc, relleno; DDCOLORKEY ck_dst, ck_src; } DDBLTFX;
typedef struct { u32 biSize; i32 biWidth, biHeight; u16 biPlanes, biBitCount; u32 biCompression, biSizeImage; i32 x, y; u32 biClrUsed, biClrImportant; u32 tabla[256]; } BMI;

struct DD; struct SUP; struct PAL; struct CLIP;
typedef struct CARA { void **vt; void *obj; int ver; } CARA;

typedef struct PAL {
  void **vt;
  int refs;
  struct DD *dd;
  u32 flags;
  u8 e[256][4];                     /* PALETTEENTRY: r, g, b, banderas */
} PAL;
typedef struct CLIP {
  void **vt;
  int refs;
  HWND hwnd;
  int hay_lista;
  RECT lista;
} CLIP;
typedef struct SUP {
  CARA cara[5];                     /* 1, 2, 3, 4 y 7 */
  int refs;
  struct DD *dd;
  u32 ancho, alto, paso, bits, r, g, b;
  u8 *mem; int mem_suya;
  u32 caps;
  int primaria, trasera;
  struct SUP *atras;                /* la cadena de Flip: primaria -> trasera -> ... */
  PAL *pal;
  CLIP *clip;
  int hay_cks, hay_ckd;
  DDCOLORKEY cks, ckd;
  int bloqueada;
  HANDLE dc, dib, viejo; u8 *dibmem;
  u32 epoca, unico;
  struct SUP *sig;
} SUP;
typedef struct DD {
  CARA cara[4];                     /* 1, 2, 4 y 7 */
  int refs;
  HWND hwnd;
  u32 nivel;
  u32 mw, mh, mbits;                /* el modo que ha pedido (0: el de la pantalla) */
  int tomada;                       /* la pantalla es suya */
  HXPANT p;
  u32 epoca;
  SUP *sups, *prim;
  /* cómo se presenta */
  u32 dw, dh, ox, oy;
  int borrar, dac332;
  u32 tab8[256];
  u32 *tab16; u32 t16_fmt;
  u32 borde;
  u32 *xmap, xmap_n;
  u32 *fila;
} DD;

#define SUPC(c) ((SUP *)((CARA *)(c))->obj)
#define DDC(c) ((DD *)((CARA *)(c))->obj)
static void **vt_sup, **vt_dd1, **vt_dd2, **vt_pal, **vt_clip;
static const int versiones_sup[5] = { 1, 2, 3, 4, 7 };
static CARA *cara_sup(SUP *s, int ver) { int k = ver >= 7 ? 4 : ver >= 4 ? 3 : ver - 1; if (k < 0) k = 0; return &s->cara[k]; }

/* ======================= la pantalla ======================= */
static u32 masc_tam(u32 m) { u32 n = 0; while (m) { n += m & 1; m >>= 1; } return n; }
static u32 masc_pos(u32 m) { u32 n = 0; if (!m) return 0; while (!(m & 1)) { m >>= 1; n++; } return n; }
static void fmt_de(u32 bits, u32 *r, u32 *g, u32 *b) {
  if (bits == 16) { *r = 0xF800; *g = 0x07E0; *b = 0x001F; }
  else if (bits == 15) { *r = 0x7C00; *g = 0x03E0; *b = 0x001F; }
  else if (bits >= 24) { *r = 0xFF0000; *g = 0xFF00; *b = 0xFF; }
  else *r = *g = *b = 0;
}
/* RGB de 8 bits por componente -> un punto de la pantalla */
static u32 a_pantalla(DD *d, u32 r, u32 g, u32 b) {
  const u8 *m = d->p.masc;
  if (d->p.octetos == 1) {
    if (d->dac332) return (r & 0xE0) | ((g >> 3) & 0x1C) | (b >> 6);
    return 0;
  }
  return ((r >> (8 - m[0])) << m[1]) | ((g >> (8 - m[2])) << m[3]) | ((b >> (8 - m[4])) << m[5]);
}
static void dac_332(void) {
  static u8 e[256][4];
  int k;
  for (k = 0; k < 256; k++) {
    e[k][0] = (k & 0xE0) * 255 / 0xE0;
    e[k][1] = ((k >> 2) & 7) * 255 / 7;
    e[k][2] = (k & 3) * 255 / 3;
    e[k][3] = 0;
  }
  HispaDXPantalla(2, 0, 256, (u32)e);
}
static u32 modo_w(DD *d) { return d->mw ? d->mw : d->p.ancho; }
static u32 modo_h(DD *d) { return d->mh ? d->mh : d->p.alto; }
static u32 modo_bits(DD *d) { return d->mbits ? d->mbits : (d->p.bits ? d->p.bits : 8); }
static void tabla8(DD *d);
static void geometria(DD *d) {
  u32 w = modo_w(d), h = modo_h(d), sw = d->p.ancho, sh = d->p.alto, k;
  int ver[6];
  u32 s = sw / w < sh / h ? sw / w : sh / h;
  if (s >= 1) { d->dw = w * s; d->dh = h * s; }
  else if (w * sh > h * sw) { d->dw = sw; d->dh = h * sw / w; }
  else { d->dh = sh; d->dw = w * sh / h; }
  d->ox = (sw - d->dw) / 2; d->oy = (sh - d->dh) / 2;
  if (d->xmap_n < d->dw) { hx_soltar(d->xmap); d->xmap = hx_pedir(d->dw * 4); d->xmap_n = d->dw; }
  for (k = 0; k < d->dw; k++) d->xmap[k] = k * w / d->dw;
  hx_soltar(d->fila); d->fila = hx_pedir(w * 4 + 16);
  d->borrar = 1;
  ver[0] = w; ver[1] = h; ver[2] = d->ox; ver[3] = d->oy; ver[4] = d->dw; ver[5] = d->dh;
  HispaDXPantalla(6, (u32)ver, 0, 0);
}
static int tomar(DD *d) {
  HispaMezBomba();
  if (d->tomada) return 1;
  if (!HispaDXPantalla(0, (u32)d->hwnd, (u32)&d->p, 0)) return 0;
  d->tomada = 1;
  d->dac332 = d->p.octetos == 1 && modo_bits(d) > 8;
  if (d->dac332) dac_332();
  d->t16_fmt = 0;
  geometria(d);
  tabla8(d);
  return 1;
}
static void soltar(DD *d) {
  if (!d->tomada) return;
  HispaMezBomba();
  HispaDXPantalla(1, 0, 0, 0);
  HispaMezBomba();
  d->tomada = 0;
}
static void tabla8(DD *d) {
  PAL *p = d->prim ? d->prim->pal : NULL;
  u32 k, osc = 0, mejor = 0xFFFFFFFF;
  for (k = 0; k < 256; k++) {
    u32 r = p ? p->e[k][0] : k, g = p ? p->e[k][1] : k, b = p ? p->e[k][2] : k, l = r * 3 + g * 6 + b;
    if (d->p.octetos == 1 && !d->dac332) d->tab8[k] = k;
    else d->tab8[k] = a_pantalla(d, r, g, b);
    if (l < mejor) { mejor = l; osc = k; }
  }
  d->borde = (d->p.octetos == 1 && !d->dac332) ? osc : 0;
  if (p && d->tomada && d->p.octetos == 1 && !d->dac332) HispaDXPantalla(2, 0, 256, (u32)p->e);
  d->borrar = 1;
}
static void tabla16(DD *d, SUP *s) {
  u32 fmt = s->r | (s->bits == 15 ? 0x10000 : 0), k;
  u32 rs = masc_pos(s->r), gs = masc_pos(s->g), bs = masc_pos(s->b);
  u32 rt = masc_tam(s->r), gt = masc_tam(s->g), bt = masc_tam(s->b);
  if (d->tab16 && d->t16_fmt == fmt) return;
  if (!d->tab16) d->tab16 = hx_pedir(65536 * 4);
  if (!d->tab16) return;
  for (k = 0; k < 65536; k++) {
    u32 r = ((k & s->r) >> rs) << (8 - rt), g = ((k & s->g) >> gs) << (8 - gt), b = ((k & s->b) >> bs) << (8 - bt);
    r |= r >> rt; g |= g >> gt; b |= b >> bt;
    d->tab16[k] = a_pantalla(d, r & 255, g & 255, b & 255);
  }
  d->t16_fmt = fmt;
}
static void poner_punto(u8 *p, u32 oct, u32 v) {
  if (oct == 1) *p = v;
  else if (oct == 2) *(u16 *)p = v;
  else if (oct == 3) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; }
  else *(u32 *)p = v;
}
/* una fila de la superficie -> d->fila (puntos de la pantalla) */
static void fila_conv(DD *d, SUP *s, u32 y) {
  u32 w = s->ancho, k, *o = d->fila;
  const u8 *f = s->mem + y * s->paso;
  if (s->bits == 8) for (k = 0; k < w; k++) o[k] = d->tab8[f[k]];
  else if (s->bits <= 16) { const u16 *q = (const u16 *)f; if (d->tab16) for (k = 0; k < w; k++) o[k] = d->tab16[q[k]]; }
  else {
    u32 st = s->bits == 24 ? 3 : 4, rs = masc_pos(s->r), gs = masc_pos(s->g), bs = masc_pos(s->b);
    for (k = 0; k < w; k++, f += st) {
      u32 v = st == 3 ? (u32)f[0] | ((u32)f[1] << 8) | ((u32)f[2] << 16) : *(const u32 *)f;
      o[k] = a_pantalla(d, (v >> rs) & 255, (v >> gs) & 255, (v >> bs) & 255);
    }
  }
}
static void borrar_todo(DD *d) {
  u32 y, x, oct = d->p.octetos;
  for (y = 0; y < d->p.alto; y++) {
    u8 *p = d->p.lfb + y * d->p.paso;
    if (oct == 1) for (x = 0; x < d->p.ancho; x++) p[x] = d->borde;
    else for (x = 0; x < d->p.ancho * oct; x++) p[x] = 0;
  }
  d->borrar = 0;
}
/* la superficie s (la de delante) a la pantalla, de la fila y0 a la y1 */
static void presentar(DD *d, SUP *s, u32 y0, u32 y1) {
  u32 oct, dy, ult = 0xFFFFFFFF, x;
  HispaMezBomba();                  /* (que el sonido no se quede sin datos) */
  if (!d->tomada || !s || !s->mem) return;
  if (!HispaDXPantalla(4, 0, 0, 0)) { d->tomada = 0; d->epoca++; return; }
  if (s->ancho != modo_w(d) || s->alto != modo_h(d)) return;
  if (s->bits > 8 && s->bits <= 16) tabla16(d, s);
  if (d->borrar) borrar_todo(d);
  oct = d->p.octetos;
  if (y1 > s->alto) y1 = s->alto;
  for (dy = y0 * d->dh / s->alto; dy < d->dh; dy++) {
    u32 sy = dy * s->alto / d->dh;
    u8 *p;
    if (sy < y0) continue;
    if (sy >= y1) break;
    p = d->p.lfb + (d->oy + dy) * d->p.paso + d->ox * oct;
    if (sy != ult) { fila_conv(d, s, sy); ult = sy; }
    if (oct == 1) { u8 *q = p; for (x = 0; x < d->dw; x++) q[x] = d->fila[d->xmap[x]]; }
    else if (oct == 2) { u16 *q = (u16 *)p; for (x = 0; x < d->dw; x++) q[x] = d->fila[d->xmap[x]]; }
    else if (oct == 4) { u32 *q = (u32 *)p; for (x = 0; x < d->dw; x++) q[x] = d->fila[d->xmap[x]]; }
    else for (x = 0; x < d->dw; x++) poner_punto(p + x * 3, 3, d->fila[d->xmap[x]]);
  }
}
/* en una ventana: un rectángulo de la superficie a la ventana */
static void a_ventana(DD *d, SUP *prim, SUP *s, const RECT *dr, const RECT *sr) {
  HWND h = prim->clip && prim->clip->hwnd ? prim->clip->hwnd : d->hwnd;
  POINT o = { 0, 0 };
  HANDLE dc;
  BMI bmi;
  u32 k;
  if (!h || !s->mem) return;
  ClientToScreen(h, &o);
  hx_cero(&bmi, sizeof bmi);
  bmi.biSize = 40; bmi.biWidth = s->paso * 8 / (s->bits == 15 ? 16 : s->bits); bmi.biHeight = -(i32)s->alto;
  bmi.biPlanes = 1; bmi.biBitCount = s->bits == 15 ? 16 : s->bits;
  if (s->bits == 16 || s->bits == 15) { bmi.biCompression = 3; bmi.tabla[0] = s->r; bmi.tabla[1] = s->g; bmi.tabla[2] = s->b; }
  if (s->bits == 8) {
    PAL *p = s->pal ? s->pal : prim->pal;
    for (k = 0; k < 256; k++) bmi.tabla[k] = p ? ((u32)p->e[k][0] << 16) | ((u32)p->e[k][1] << 8) | p->e[k][2] : (k << 16) | (k << 8) | k;
  }
  dc = GetDC(h);
  if (!dc) return;
  StretchDIBits(dc, dr->left - o.x, dr->top - o.y, dr->right - dr->left, dr->bottom - dr->top,
                sr->left, sr->top, sr->right - sr->left, sr->bottom - sr->top, s->mem, &bmi, 0, 0x00CC0020);
  ReleaseDC(h, dc);
}

/* ======================= superficies ======================= */
static u32 oct_de(u32 bits) { return bits == 15 ? 2 : (bits + 7) / 8; }
static SUP *sup_nueva(DD *d, u32 w, u32 h, u32 bits, u32 r, u32 g, u32 b, u32 caps) {
  SUP *s = hx_pedir(sizeof(SUP));
  int k;
  if (!s) return NULL;
  for (k = 0; k < 5; k++) { s->cara[k].vt = vt_sup; s->cara[k].obj = s; s->cara[k].ver = versiones_sup[k]; }
  s->refs = 1; s->dd = d;
  s->ancho = w; s->alto = h; s->bits = bits;
  if (bits > 8 && !r) fmt_de(bits, &r, &g, &b);
  s->r = r; s->g = g; s->b = b;
  s->paso = (w * oct_de(bits) + 3) & ~3u;
  s->caps = caps;
  if (!(s->mem = hx_pedir(s->paso * h))) { hx_soltar(s); return NULL; }
  s->mem_suya = 1;
  s->epoca = d->epoca;
  s->sig = d->sups; d->sups = s;
  d->refs++;
  return s;
}
static void sup_quitar(SUP *s) {
  DD *d = s->dd;
  SUP **p;
  for (p = &d->sups; *p; p = &(*p)->sig) if (*p == s) { *p = s->sig; break; }
  if (d->prim == s) d->prim = NULL;
  if (s->mem_suya) hx_soltar(s->mem);
  hx_soltar(s);
}
static void pf_rellena(DDPIXELFORMAT *pf, u32 bits, u32 r, u32 g, u32 b) {
  hx_cero(pf, sizeof *pf);
  pf->tam = 32;
  if (bits == 8) { pf->flags = DDPF_PALETTEINDEXED8 | DDPF_RGB; pf->bits = 8; return; }
  pf->flags = DDPF_RGB; pf->bits = bits == 15 ? 16 : bits; pf->r = r; pf->g = g; pf->b = b;
}
static void desc_rellena(SUP *s, DDSD *x) {
  u32 t = x->tam == 124 ? 124 : 108;
  hx_cero((u8 *)x + 4, t - 4);
  x->tam = t;
  x->flags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT;
  x->ancho = s->ancho; x->alto = s->alto; x->paso = s->paso;
  pf_rellena(&x->pf, s->bits, s->r, s->g, s->b);
  x->caps[0] = s->caps;
  if (s->atras) { SUP *q = s->atras; while (q && q != s) { x->traseros++; q = q->atras; } x->flags |= DDSD_BACKBUFFERCOUNT; }
  if (s->hay_cks) { x->flags |= DDSD_CKSRCBLT; x->ck_sblt = s->cks; }
  if (s->hay_ckd) { x->flags |= DDSD_CKDESTBLT; x->ck_dblt = s->ckd; }
}
static int perdida(SUP *s) {
  DD *d = s->dd;
  if (!(d->nivel & DDSCL_EXCLUSIVE)) return 0;
  if (d->tomada && !HispaDXPantalla(4, 0, 0, 0)) { d->tomada = 0; d->epoca++; }
  return s->epoca != d->epoca;
}
/* la pantalla otra vez, si su ventana vuelve a ser la activa */
static void recobrar(DD *d) {
  if (d->tomada || !(d->nivel & DDSCL_EXCLUSIVE) || !d->prim) return;
  if (GetActiveWindow() != d->hwnd) return;
  if (tomar(d)) { d->epoca++; }
}

static HRESULT WINAPI S_QueryInterface(CARA *c, const GUID *g, void **p) {
  SUP *s = SUPC(c);
  int v = 0;
  if (!p) return (HRESULT)hx_tr("Sup::QueryInterface", E_POINTER);
  if (hx_guid(g, 0x6C14DB81) || hx_guid(g, 0)) v = 1;
  else if (hx_guid(g, 0x57805885)) v = 2;
  else if (hx_guid(g, 0xDA044E00)) v = 3;
  else if (hx_guid(g, 0x0B2B8630)) v = 4;
  else if (hx_guid(g, 0x06675A80)) v = 7;
  if (!v) { *p = NULL; return (HRESULT)hx_tr("Sup::QueryInterface", E_NOINTERFACE); }
  s->refs++;
  *p = cara_sup(s, v);
  return (HRESULT)hx_tr("Sup::QueryInterface", S_OK);
}
static u32 WINAPI S_AddRef(CARA *c) { return ++SUPC(c)->refs; }
static u32 WINAPI S_Release(CARA *c) {
  SUP *s = SUPC(c), *q, *n;
  DD *d = s->dd;
  if (--s->refs > 0) return s->refs;
  if (s->primaria) {                /* con ella se van sus traseras */
    for (q = s->atras; q && q != s; q = n) { n = q->atras; sup_quitar(q); d->refs--; }
    if (d->nivel & DDSCL_EXCLUSIVE) soltar(d);
  }
  if (s->pal) s->pal->refs--;
  if (s->clip) s->clip->refs--;
  sup_quitar(s);
  if (--d->refs <= 0) hx_soltar(d);
  return 0;
}
static HRESULT WINAPI S_AddAttachedSurface(CARA *c, CARA *o) { (void)c; (void)o; return S_OK; }
static HRESULT WINAPI S_AddOverlayDirtyRect(CARA *c, RECT *r) { (void)c; (void)r; return E_NOTIMPL; }
static int recorta(RECT *dr, RECT *sr, u32 dw, u32 dh, u32 sw, u32 sh, int estira) {
  if (!estira) {
    int w = sr->right - sr->left, h = sr->bottom - sr->top;
    if (sr->left < 0) { dr->left -= sr->left; sr->left = 0; }
    if (sr->top < 0) { dr->top -= sr->top; sr->top = 0; }
    if (dr->left < 0) { sr->left -= dr->left; dr->left = 0; }
    if (dr->top < 0) { sr->top -= dr->top; dr->top = 0; }
    w = sr->right - sr->left; h = sr->bottom - sr->top;
    if (sr->right > (int)sw) sr->right = sw;
    if (sr->bottom > (int)sh) sr->bottom = sh;
    w = sr->right - sr->left; h = sr->bottom - sr->top;
    if (dr->left + w > (int)dw) w = dw - dr->left;
    if (dr->top + h > (int)dh) h = dh - dr->top;
    if (w <= 0 || h <= 0) return 0;
    dr->right = dr->left + w; dr->bottom = dr->top + h;
    sr->right = sr->left + w; sr->bottom = sr->top + h;
    return 1;
  }
  if (dr->left >= dr->right || dr->top >= dr->bottom) return 0;
  if (sr->left < 0 || sr->top < 0 || sr->right > (int)sw || sr->bottom > (int)sh || sr->left >= sr->right || sr->top >= sr->bottom) return 0;
  return dr->right > 0 && dr->bottom > 0 && dr->left < (int)dw && dr->top < (int)dh;
}
static inline u32 leer_punto(const u8 *p, u32 oct) {
  return oct == 1 ? *p : oct == 2 ? *(const u16 *)p : oct == 3 ? (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) : *(const u32 *)p;
}
/* la copia de verdad (mismos formatos). ks/kd: claves (lo..hi), o -1 */
static void copiar(SUP *d, const RECT *dr, SUP *s, const RECT *sr, int hay_ks, DDCOLORKEY ks, int hay_kd, DDCOLORKEY kd) {
  u32 oct = oct_de(d->bits), dw = dr->right - dr->left, dh = dr->bottom - dr->top;
  u32 sw = sr->right - sr->left, sh = sr->bottom - sr->top, x, y;
  u32 mask = oct == 4 ? 0xFFFFFFFF : (1u << (oct * 8)) - 1;
  int estira = dw != sw || dh != sh, solapa = d == s;
  u8 *tmp = NULL;
  if (solapa) {                     /* la misma superficie: por una copia */
    tmp = hx_pedir(sw * sh * oct);
    if (!tmp) return;
    for (y = 0; y < sh; y++) hx_copia(tmp + y * sw * oct, s->mem + (sr->top + y) * s->paso + sr->left * oct, sw * oct);
  }
  for (y = 0; y < dh; y++) {
    u32 sy = estira ? y * sh / dh : y;
    const u8 *fs = tmp ? tmp + sy * sw * oct : s->mem + (sr->top + sy) * s->paso + sr->left * oct;
    u8 *fd = d->mem + (dr->top + y) * d->paso + dr->left * oct;
    if (!estira && !hay_ks && !hay_kd) { hx_copia(fd, fs, dw * oct); continue; }
    if (oct == 1 && !estira && hay_ks && !hay_kd && ks.lo == ks.hi) {
      u8 k = ks.lo;
      for (x = 0; x < dw; x++) { u8 v = fs[x]; if (v != k) fd[x] = v; }
      continue;
    }
    for (x = 0; x < dw; x++) {
      u32 sx = estira ? x * sw / dw : x;
      u32 v = leer_punto(fs + sx * oct, oct) & mask;
      if (hay_ks && v >= (ks.lo & mask) && v <= (ks.hi & mask)) continue;
      if (hay_kd) { u32 w = leer_punto(fd + x * oct, oct) & mask; if (w < (kd.lo & mask) || w > (kd.hi & mask)) continue; }
      poner_punto(fd + x * oct, oct, v);
    }
  }
  hx_soltar(tmp);
}
static void rellenar(SUP *d, const RECT *r, u32 color) {
  u32 oct = oct_de(d->bits), x, y;
  for (y = r->top; y < (u32)r->bottom; y++) {
    u8 *f = d->mem + y * d->paso + r->left * oct;
    if (oct == 1) for (x = 0; x < (u32)(r->right - r->left); x++) f[x] = color;
    else for (x = 0; x < (u32)(r->right - r->left); x++) poner_punto(f + x * oct, oct, color);
  }
}
static int en_ventana(SUP *s) { return s->primaria && !(s->dd->nivel & DDSCL_EXCLUSIVE); }
static void tras_dibujar(SUP *s, u32 y0, u32 y1) {
  DD *d = s->dd;
  if (s->primaria && d->tomada) presentar(d, s, y0, y1);
}
#define DDBLT_COLORFILL 0x400
#define DDBLT_KEYDEST 0x2000
#define DDBLT_KEYDESTOVERRIDE 0x4000
#define DDBLT_KEYSRC 0x8000
#define DDBLT_KEYSRCOVERRIDE 0x10000
#define DDBLT_DEPTHFILL 0x2000000
static HRESULT WINAPI S_Blt(CARA *c, RECT *pdr, CARA *co, RECT *psr, u32 fl, DDBLTFX *fx) {
  SUP *d = SUPC(c), *s = co ? SUPC(co) : NULL;
  RECT dr, sr;
  DDCOLORKEY ks = { 0, 0 }, kd = { 0, 0 };
  int hks = 0, hkd = 0;
  if (perdida(d) || (s && perdida(s))) { recobrar(d->dd); return DDERR_SURFACELOST; }
  if (d->bloqueada) return DDERR_SURFACEBUSY;
  if (pdr) dr = *pdr; else { dr.left = dr.top = 0; dr.right = d->ancho; dr.bottom = d->alto; }
  if (fl & (DDBLT_COLORFILL | DDBLT_DEPTHFILL)) {
    if (!fx) return E_INVALIDARG;
    if (dr.left < 0) dr.left = 0;
    if (dr.top < 0) dr.top = 0;
    if (dr.right > (int)d->ancho) dr.right = d->ancho;
    if (dr.bottom > (int)d->alto) dr.bottom = d->alto;
    if (dr.left >= dr.right || dr.top >= dr.bottom) return S_OK;
    if (en_ventana(d)) return S_OK;
    rellenar(d, &dr, (fl & DDBLT_DEPTHFILL) ? fx->zdst : fx->relleno);
    tras_dibujar(d, dr.top, dr.bottom);
    return S_OK;
  }
  if (!s) return E_INVALIDARG;
  if (psr) sr = *psr; else { sr.left = sr.top = 0; sr.right = s->ancho; sr.bottom = s->alto; }
  if (en_ventana(d)) { a_ventana(d->dd, d, s, &dr, &sr); return S_OK; }
  if (oct_de(s->bits) != oct_de(d->bits)) return DDERR_INVALIDPIXELFORMAT;
  if (fl & DDBLT_KEYSRCOVERRIDE) { if (!fx) return E_INVALIDARG; ks = fx->ck_src; hks = 1; }
  else if ((fl & DDBLT_KEYSRC) && s->hay_cks) { ks = s->cks; hks = 1; }
  if (fl & DDBLT_KEYDESTOVERRIDE) { if (!fx) return E_INVALIDARG; kd = fx->ck_dst; hkd = 1; }
  else if ((fl & DDBLT_KEYDEST) && d->hay_ckd) { kd = d->ckd; hkd = 1; }
  {
    int estira = (dr.right - dr.left) != (sr.right - sr.left) || (dr.bottom - dr.top) != (sr.bottom - sr.top);
    if (estira) {                   /* lo que sale de la superficie se corta (en proporción) */
      RECT o = dr;
      if (sr.left < 0 || sr.top < 0 || sr.right > (int)s->ancho || sr.bottom > (int)s->alto) return DDERR_INVALIDRECT;
      if (dr.left < 0) dr.left = 0;
      if (dr.top < 0) dr.top = 0;
      if (dr.right > (int)d->ancho) dr.right = d->ancho;
      if (dr.bottom > (int)d->alto) dr.bottom = d->alto;
      if (dr.left >= dr.right || dr.top >= dr.bottom) return S_OK;
      if (o.right <= o.left || o.bottom <= o.top) return DDERR_INVALIDRECT;
      {
        int sw = sr.right - sr.left, sh = sr.bottom - sr.top, ow = o.right - o.left, oh = o.bottom - o.top;
        RECT n;
        n.left = sr.left + (dr.left - o.left) * sw / ow;
        n.right = sr.left + (dr.right - o.left) * sw / ow;
        n.top = sr.top + (dr.top - o.top) * sh / oh;
        n.bottom = sr.top + (dr.bottom - o.top) * sh / oh;
        if (n.right <= n.left) n.right = n.left + 1;
        if (n.bottom <= n.top) n.bottom = n.top + 1;
        sr = n;
      }
    } else if (!recorta(&dr, &sr, d->ancho, d->alto, s->ancho, s->alto, 0)) return S_OK;
  }
  copiar(d, &dr, s, &sr, hks, ks, hkd, kd);
  tras_dibujar(d, dr.top, dr.bottom);
  return S_OK;
}
static HRESULT WINAPI S_BltBatch(CARA *c, void *b, u32 n, u32 f) { (void)c; (void)b; (void)n; (void)f; return E_NOTIMPL; }
static HRESULT WINAPI S_BltFast(CARA *c, u32 x, u32 y, CARA *co, RECT *psr, u32 tr) {
  SUP *d = SUPC(c), *s = co ? SUPC(co) : NULL;
  RECT dr, sr;
  DDCOLORKEY ks = { 0, 0 }, kd = { 0, 0 };
  if (!s) return E_INVALIDARG;
  if (perdida(d) || perdida(s)) { recobrar(d->dd); return DDERR_SURFACELOST; }
  if (d->bloqueada) return DDERR_SURFACEBUSY;
  if (psr) sr = *psr; else { sr.left = sr.top = 0; sr.right = s->ancho; sr.bottom = s->alto; }
  dr.left = x; dr.top = y; dr.right = x + sr.right - sr.left; dr.bottom = y + sr.bottom - sr.top;
  if (en_ventana(d)) { a_ventana(d->dd, d, s, &dr, &sr); return S_OK; }
  if (oct_de(s->bits) != oct_de(d->bits)) return DDERR_INVALIDPIXELFORMAT;
  if (!recorta(&dr, &sr, d->ancho, d->alto, s->ancho, s->alto, 0)) return S_OK;
  if (tr & 1) ks = s->cks;
  if (tr & 2) kd = d->ckd;
  copiar(d, &dr, s, &sr, (tr & 1) && s->hay_cks, ks, (tr & 2) && d->hay_ckd, kd);
  tras_dibujar(d, dr.top, dr.bottom);
  return S_OK;
}
static HRESULT WINAPI S_DeleteAttachedSurface(CARA *c, u32 f, CARA *o) { (void)c; (void)f; (void)o; return S_OK; }
typedef HRESULT WINAPI (*ENUMSUP)(void *, DDSD *, void *);
static HRESULT WINAPI S_EnumAttachedSurfaces(CARA *c, void *ctx, ENUMSUP cb) {
  SUP *s = SUPC(c);
  DDSD x;
  if (!cb) return E_INVALIDARG;
  if (s->atras && s->atras != s) {
    x.tam = c->ver >= 4 ? 124 : 108;
    desc_rellena(s->atras, &x);
    s->atras->refs++;
    cb(cara_sup(s->atras, c->ver), &x, ctx);
  }
  return S_OK;
}
static HRESULT WINAPI S_EnumOverlayZOrders(CARA *c, u32 f, void *x, void *cb) { (void)c; (void)f; (void)x; (void)cb; return DDERR_NOOVERLAYHW; }
static HRESULT WINAPI S_Flip(CARA *c, CARA *co, u32 fl) {
  SUP *s = SUPC(c), *q, *t = co ? SUPC(co) : NULL;
  DD *d = s->dd;
  u8 *m;
  (void)fl;
  if (!s->primaria || !s->atras) return DDERR_NOTFLIPPABLE;
  if (perdida(s)) { recobrar(d); return DDERR_SURFACELOST; }
  if (s->bloqueada) return DDERR_SURFACEBUSY;
  if (t && t != s->atras) {          /* a una en concreto: se cambia con esa */
    m = s->mem; s->mem = t->mem; t->mem = m;
  } else {                           /* la cadena gira: cada una toma la de detrás */
    m = s->mem;
    for (q = s; q->atras && q->atras != s; q = q->atras) q->mem = q->atras->mem;
    q->mem = m;
  }
  s->unico++;
  if (en_ventana(s)) return S_OK;
  presentar(d, s, 0, s->alto);
  return S_OK;
}
static HRESULT WINAPI S_GetAttachedSurface(CARA *c, u32 *caps, CARA **p) {
  SUP *s = SUPC(c);
  if (!caps || !p) return (HRESULT)hx_tr("GetAttachedSurface", E_INVALIDARG);
  *p = NULL;
  if ((caps[0] & DDSCAPS_BACKBUFFER) && s->atras && s->atras != s) {
    s->atras->refs++;
    *p = cara_sup(s->atras, c->ver);
    return (HRESULT)hx_tr("GetAttachedSurface", S_OK);
  }
  if ((caps[0] & DDSCAPS_FLIP) && s->atras && s->atras != s) { s->atras->refs++; *p = cara_sup(s->atras, c->ver); return (HRESULT)hx_tr("GetAttachedSurface", S_OK); }
  return (HRESULT)hx_tr("GetAttachedSurface", DDERR_NOTFOUND);
}
static HRESULT WINAPI S_GetBltStatus(CARA *c, u32 f) { (void)c; (void)f; return S_OK; }
static HRESULT WINAPI S_GetCaps(CARA *c, u32 *caps) {
  if (!caps) return E_INVALIDARG;
  caps[0] = SUPC(c)->caps;
  if (c->ver >= 4) caps[1] = caps[2] = caps[3] = 0;
  return S_OK;
}
static HRESULT WINAPI S_GetClipper(CARA *c, CLIP **p) {
  SUP *s = SUPC(c);
  if (!p) return E_INVALIDARG;
  if (!s->clip) return DDH(578);     /* DDERR_NOCLIPPERATTACHED */
  s->clip->refs++; *p = s->clip; return S_OK;
}
static HRESULT WINAPI S_GetColorKey(CARA *c, u32 f, DDCOLORKEY *k) {
  SUP *s = SUPC(c);
  if (!k) return E_INVALIDARG;
  if (f & 8) { if (!s->hay_cks) return DDERR_NOCOLORKEY; *k = s->cks; return S_OK; }
  if (f & 2) { if (!s->hay_ckd) return DDERR_NOCOLORKEY; *k = s->ckd; return S_OK; }
  return E_INVALIDARG;
}
static HRESULT WINAPI S_GetDC(CARA *c, HANDLE *pdc) {
  SUP *s = SUPC(c);
  BMI bmi;
  u32 k, y, fila;
  void *bits;
  if (!pdc) return E_INVALIDARG;
  if (perdida(s)) return DDERR_SURFACELOST;
  if (s->dc || s->bloqueada) return DDERR_SURFACEBUSY;
  hx_cero(&bmi, sizeof bmi);
  bmi.biSize = 40; bmi.biWidth = s->ancho; bmi.biHeight = -(i32)s->alto; bmi.biPlanes = 1;
  bmi.biBitCount = s->bits == 15 ? 16 : s->bits;
  if (bmi.biBitCount == 16) { bmi.biCompression = 3; bmi.tabla[0] = s->r; bmi.tabla[1] = s->g; bmi.tabla[2] = s->b; }
  if (s->bits == 8) {
    PAL *p = s->pal ? s->pal : s->dd->prim ? s->dd->prim->pal : NULL;
    for (k = 0; k < 256; k++) bmi.tabla[k] = p ? ((u32)p->e[k][0] << 16) | ((u32)p->e[k][1] << 8) | p->e[k][2] : (k << 16) | (k << 8) | k;
    bmi.biClrUsed = 256;
  }
  s->dib = CreateDIBSection(NULL, &bmi, 0, &bits, NULL, 0);
  if (!s->dib || !bits) return DDERR_CANTCREATEDC;
  s->dibmem = bits;
  fila = (s->ancho * bmi.biBitCount + 31) / 32 * 4;
  for (y = 0; y < s->alto; y++) hx_copia(s->dibmem + y * fila, s->mem + y * s->paso, s->ancho * oct_de(s->bits));
  s->dc = CreateCompatibleDC(NULL);
  if (!s->dc) { DeleteObject(s->dib); s->dib = NULL; return DDERR_CANTCREATEDC; }
  s->viejo = SelectObject(s->dc, s->dib);
  *pdc = s->dc;
  HispaMezBomba();
  return S_OK;
}
static HRESULT WINAPI S_ReleaseDC(CARA *c, HANDLE dc) {
  SUP *s = SUPC(c);
  u32 y, fila;
  if (!s->dc || dc != s->dc) return DDERR_NODC;
  fila = (s->ancho * (s->bits == 15 ? 16 : s->bits) + 31) / 32 * 4;
  for (y = 0; y < s->alto; y++) hx_copia(s->mem + y * s->paso, s->dibmem + y * fila, s->ancho * oct_de(s->bits));
  SelectObject(s->dc, s->viejo);
  DeleteDC(s->dc);
  DeleteObject(s->dib);
  s->dc = s->dib = NULL; s->dibmem = NULL;
  HispaMezBomba();
  tras_dibujar(s, 0, s->alto);
  return S_OK;
}
static HRESULT WINAPI S_GetFlipStatus(CARA *c, u32 f) { (void)c; (void)f; return S_OK; }
static HRESULT WINAPI S_GetOverlayPosition(CARA *c, int *x, int *y) { (void)c; (void)x; (void)y; return DDERR_NOOVERLAYHW; }
static HRESULT WINAPI S_GetPalette(CARA *c, PAL **p) {
  SUP *s = SUPC(c);
  if (!p) return E_INVALIDARG;
  if (!s->pal) return DDERR_NOPALETTEATTACHED;
  s->pal->refs++; *p = s->pal; return S_OK;
}
static HRESULT WINAPI S_GetPixelFormat(CARA *c, DDPIXELFORMAT *pf) {
  SUP *s = SUPC(c);
  if (!pf) return E_INVALIDARG;
  pf_rellena(pf, s->bits, s->r, s->g, s->b);
  return S_OK;
}
static HRESULT WINAPI S_GetSurfaceDesc(CARA *c, DDSD *x) {
  if (!x || (x->tam != 108 && x->tam != 124)) return E_INVALIDARG;
  desc_rellena(SUPC(c), x);
  return S_OK;
}
static HRESULT WINAPI S_Initialize(CARA *c, void *d, DDSD *x) { (void)c; (void)d; (void)x; return DDERR_ALREADYINITIALIZED; }
static HRESULT WINAPI S_IsLost(CARA *c) { return perdida(SUPC(c)) ? DDERR_SURFACELOST : S_OK; }
static HRESULT WINAPI S_Lock(CARA *c, RECT *r, DDSD *x, u32 fl, HANDLE ev) {
  SUP *s = SUPC(c);
  u32 oct = oct_de(s->bits);
  (void)fl; (void)ev;
  if (!x || (x->tam != 108 && x->tam != 124)) return (HRESULT)hx_tr("Lock", E_INVALIDARG);
  if (perdida(s)) { recobrar(s->dd); return (HRESULT)hx_tr("Lock", DDERR_SURFACELOST); }
  if (s->bloqueada || s->dc) return (HRESULT)hx_tr("Lock", DDERR_SURFACEBUSY);
  if (r && (r->left < 0 || r->top < 0 || r->right > (int)s->ancho || r->bottom > (int)s->alto || r->left >= r->right || r->top >= r->bottom)) return (HRESULT)hx_tr("Lock", DDERR_INVALIDRECT);
  desc_rellena(s, x);
  x->flags |= DDSD_LPSURFACE;
  x->mem = s->mem + (r ? r->top * s->paso + r->left * oct : 0);
  s->bloqueada = 1;
  return (HRESULT)hx_tr("Lock", S_OK);
}
static HRESULT WINAPI S_Restore(CARA *c) {
  SUP *s = SUPC(c);
  DD *d = s->dd;
  if (!perdida(s)) return S_OK;
  if (!d->tomada) recobrar(d);
  if (!d->tomada) return DDERR_SURFACELOST;
  s->epoca = d->epoca;
  if (s->primaria) { SUP *q; for (q = s->atras; q && q != s; q = q->atras) q->epoca = d->epoca; }
  return S_OK;
}
static HRESULT WINAPI S_SetClipper(CARA *c, CLIP *k) {
  SUP *s = SUPC(c);
  if (s->clip) s->clip->refs--;
  s->clip = k;
  if (k) k->refs++;
  return S_OK;
}
static HRESULT WINAPI S_SetColorKey(CARA *c, u32 f, DDCOLORKEY *k) {
  SUP *s = SUPC(c);
  if (f & 8) { s->hay_cks = k != NULL; if (k) { s->cks = *k; if (!(f & 1)) s->cks.hi = s->cks.lo; } return S_OK; }
  if (f & 2) { s->hay_ckd = k != NULL; if (k) { s->ckd = *k; if (!(f & 1)) s->ckd.hi = s->ckd.lo; } return S_OK; }
  return E_INVALIDARG;
}
static HRESULT WINAPI S_SetOverlayPosition(CARA *c, int x, int y) { (void)c; (void)x; (void)y; return DDERR_NOOVERLAYHW; }
static HRESULT WINAPI S_SetPalette(CARA *c, PAL *p) {
  SUP *s = SUPC(c);
  if (s->bits != 8 && p) return DDERR_NOTPALETTIZED;
  if (s->pal) s->pal->refs--;
  s->pal = p;
  if (p) p->refs++;
  if (s->primaria) { tabla8(s->dd); tras_dibujar(s, 0, s->alto); }
  return S_OK;
}
static HRESULT WINAPI S_Unlock(CARA *c, void *r) {
  SUP *s = SUPC(c);
  (void)r;
  if (!s->bloqueada) return DDERR_NOTLOCKED;
  s->bloqueada = 0;
  s->unico++;
  if (en_ventana(s)) {
    RECT a = { 0, 0, (int)s->ancho, (int)s->alto };
    a_ventana(s->dd, s, s, &a, &a);
  } else tras_dibujar(s, 0, s->alto);
  return S_OK;
}
static HRESULT WINAPI S_UpdateOverlay(CARA *c, RECT *a, CARA *d, RECT *b, u32 f, void *fx) { (void)c; (void)a; (void)d; (void)b; (void)f; (void)fx; return DDERR_NOOVERLAYHW; }
static HRESULT WINAPI S_UpdateOverlayDisplay(CARA *c, u32 f) { (void)c; (void)f; return DDERR_NOOVERLAYHW; }
static HRESULT WINAPI S_UpdateOverlayZOrder(CARA *c, u32 f, CARA *r) { (void)c; (void)f; (void)r; return DDERR_NOOVERLAYHW; }
static HRESULT WINAPI S_GetDDInterface(CARA *c, void **p) {
  SUP *s = SUPC(c);
  if (!p) return E_INVALIDARG;
  s->dd->refs++;
  *p = &s->dd->cara[0];
  return S_OK;
}
static HRESULT WINAPI S_PageLock(CARA *c, u32 f) { (void)c; (void)f; return S_OK; }
static HRESULT WINAPI S_PageUnlock(CARA *c, u32 f) { (void)c; (void)f; return S_OK; }
static HRESULT WINAPI S_SetSurfaceDesc(CARA *c, DDSD *x, u32 f) {
  SUP *s = SUPC(c);
  (void)f;
  if (!x || !(x->flags & DDSD_LPSURFACE) || !x->mem) return E_INVALIDARG;
  if (s->mem_suya) hx_soltar(s->mem);
  s->mem = x->mem; s->mem_suya = 0;
  if (x->flags & DDSD_WIDTH) s->ancho = x->ancho;
  if (x->flags & DDSD_HEIGHT) s->alto = x->alto;
  if (x->flags & DDSD_PITCH) s->paso = x->paso;
  return S_OK;
}
static HRESULT WINAPI S_SetPrivateData(CARA *c, const GUID *g, void *d, u32 n, u32 f) { (void)c; (void)g; (void)d; (void)n; (void)f; return E_NOTIMPL; }
static HRESULT WINAPI S_GetPrivateData(CARA *c, const GUID *g, void *d, u32 *n) { (void)c; (void)g; (void)d; (void)n; return DDERR_NOTFOUND; }
static HRESULT WINAPI S_FreePrivateData(CARA *c, const GUID *g) { (void)c; (void)g; return DDERR_NOTFOUND; }
static HRESULT WINAPI S_GetUniquenessValue(CARA *c, u32 *v) { if (!v) return E_INVALIDARG; *v = SUPC(c)->unico; return S_OK; }
static HRESULT WINAPI S_ChangeUniquenessValue(CARA *c) { SUPC(c)->unico++; return S_OK; }
static HRESULT WINAPI S_SetPriority(CARA *c, u32 p) { (void)c; (void)p; return S_OK; }
static HRESULT WINAPI S_GetPriority(CARA *c, u32 *p) { (void)c; if (p) *p = 0; return S_OK; }
static HRESULT WINAPI S_SetLOD(CARA *c, u32 p) { (void)c; (void)p; return S_OK; }
static HRESULT WINAPI S_GetLOD(CARA *c, u32 *p) { (void)c; if (p) *p = 0; return S_OK; }
static void *tabla_sup[] = { S_QueryInterface, S_AddRef, S_Release, S_AddAttachedSurface, S_AddOverlayDirtyRect, S_Blt,
  S_BltBatch, S_BltFast, S_DeleteAttachedSurface, S_EnumAttachedSurfaces, S_EnumOverlayZOrders, S_Flip,
  S_GetAttachedSurface, S_GetBltStatus, S_GetCaps, S_GetClipper, S_GetColorKey, S_GetDC, S_GetFlipStatus,
  S_GetOverlayPosition, S_GetPalette, S_GetPixelFormat, S_GetSurfaceDesc, S_Initialize, S_IsLost, S_Lock,
  S_ReleaseDC, S_Restore, S_SetClipper, S_SetColorKey, S_SetOverlayPosition, S_SetPalette, S_Unlock,
  S_UpdateOverlay, S_UpdateOverlayDisplay, S_UpdateOverlayZOrder,
  S_GetDDInterface, S_PageLock, S_PageUnlock,                          /* 2 */
  S_SetSurfaceDesc,                                                    /* 3 */
  S_SetPrivateData, S_GetPrivateData, S_FreePrivateData, S_GetUniquenessValue, S_ChangeUniquenessValue,  /* 4 */
  S_SetPriority, S_GetPriority, S_SetLOD, S_GetLOD };                  /* 7 */

/* ======================= paletas y recortadores ======================= */
static HRESULT WINAPI P_QueryInterface(PAL *p, const GUID *g, void **q) {
  if (!q) return E_POINTER;
  if (hx_guid(g, 0x6C14DB84) || hx_guid(g, 0)) { p->refs++; *q = p; return S_OK; }
  *q = NULL; return E_NOINTERFACE;
}
static u32 WINAPI P_AddRef(PAL *p) { return ++p->refs; }
static u32 WINAPI P_Release(PAL *p) { if (--p->refs > 0) return p->refs; hx_soltar(p); return 0; }
static HRESULT WINAPI P_GetCaps(PAL *p, u32 *c) { if (!c) return E_INVALIDARG; *c = p->flags | 4; return S_OK; }
static HRESULT WINAPI P_GetEntries(PAL *p, u32 f, u32 a, u32 n, u8 *e) {
  (void)f;
  if (!e || a + n > 256) return E_INVALIDARG;
  hx_copia(e, p->e[a], n * 4);
  return S_OK;
}
static HRESULT WINAPI P_Initialize(PAL *p, void *d, u32 f, void *e) { (void)p; (void)d; (void)f; (void)e; return DDERR_ALREADYINITIALIZED; }
static HRESULT WINAPI P_SetEntries(PAL *p, u32 f, u32 a, u32 n, const u8 *e) {
  DD *d = p->dd;
  (void)f;
  if (!e || a + n > 256) return E_INVALIDARG;
  hx_copia(p->e[a], e, n * 4);
  if (d && d->prim && d->prim->pal == p) {
    if (d->tomada && d->p.octetos == 1 && !d->dac332) HispaDXPantalla(2, a, n, (u32)p->e[a]);
    else { tabla8(d); tras_dibujar(d->prim, 0, d->prim->alto); }
  }
  return S_OK;
}
static void *tabla_pal[] = { P_QueryInterface, P_AddRef, P_Release, P_GetCaps, P_GetEntries, P_Initialize, P_SetEntries };
static HRESULT WINAPI C_QueryInterface(CLIP *c, const GUID *g, void **q) {
  if (!q) return E_POINTER;
  if (hx_guid(g, 0x6C14DB85) || hx_guid(g, 0)) { c->refs++; *q = c; return S_OK; }
  *q = NULL; return E_NOINTERFACE;
}
static u32 WINAPI C_AddRef(CLIP *c) { return ++c->refs; }
static u32 WINAPI C_Release(CLIP *c) { if (--c->refs > 0) return c->refs; hx_soltar(c); return 0; }
typedef struct { u32 tam, tipo, n, tam_r; RECT caja; RECT r[1]; } RGNDATA;
static HRESULT WINAPI C_GetClipList(CLIP *c, RECT *r, RGNDATA *d, u32 *n) {
  RECT a;
  (void)r;
  if (!n) return E_INVALIDARG;
  if (c->hay_lista) a = c->lista;
  else if (c->hwnd) { POINT o = { 0, 0 }; GetClientRect(c->hwnd, &a); ClientToScreen(c->hwnd, &o); a.left += o.x; a.right += o.x; a.top += o.y; a.bottom += o.y; }
  else return DDERR_NOCLIPLIST;
  if (!d) { *n = sizeof(RGNDATA); return S_OK; }
  if (*n < sizeof(RGNDATA)) { *n = sizeof(RGNDATA); return DDH(570); }   /* DDERR_REGIONTOOSMALL */
  d->tam = 32; d->tipo = 1; d->n = 1; d->tam_r = 16; d->caja = a; d->r[0] = a;
  return S_OK;
}
static HRESULT WINAPI C_GetHWnd(CLIP *c, HWND *h) { if (!h) return E_INVALIDARG; *h = c->hwnd; return S_OK; }
static HRESULT WINAPI C_Initialize(CLIP *c, void *d, u32 f) { (void)c; (void)d; (void)f; return DDERR_ALREADYINITIALIZED; }
static HRESULT WINAPI C_IsClipListChanged(CLIP *c, int *b) { (void)c; if (b) *b = 0; return S_OK; }
static HRESULT WINAPI C_SetClipList(CLIP *c, RGNDATA *d, u32 f) {
  (void)f;
  if (!d) { c->hay_lista = 0; return S_OK; }
  c->hay_lista = 1; c->lista = d->caja;
  return S_OK;
}
static HRESULT WINAPI C_SetHWnd(CLIP *c, u32 f, HWND h) { (void)f; c->hwnd = h; return S_OK; }
static void *tabla_clip[] = { C_QueryInterface, C_AddRef, C_Release, C_GetClipList, C_GetHWnd, C_Initialize,
  C_IsClipListChanged, C_SetClipList, C_SetHWnd };
static CLIP *clip_nuevo(void) {
  CLIP *c = hx_pedir(sizeof(CLIP));
  if (c) { c->vt = vt_clip; c->refs = 1; }
  return c;
}

/* ======================= IDirectDraw ======================= */
static HRESULT WINAPI D_QueryInterface(CARA *c, const GUID *g, void **p) {
  DD *d = DDC(c);
  int k = -1;
  if (!p) return (HRESULT)hx_tr("DD::QueryInterface", E_POINTER);
  if (hx_guid(g, 0x6C14DB80) || hx_guid(g, 0)) k = 0;
  else if (hx_guid(g, 0xB3A6F3E0)) k = 1;
  else if (hx_guid(g, 0x9C59509A)) k = 2;
  else if (hx_guid(g, 0x15E65EC0)) k = 3;
  if (k < 0) { *p = NULL; return (HRESULT)hx_tr("DD::QueryInterface", E_NOINTERFACE); }   /* (Direct3D: aún no) */
  d->refs++;
  *p = &d->cara[k];
  return (HRESULT)hx_tr("DD::QueryInterface", S_OK);
}
static u32 WINAPI D_AddRef(CARA *c) { return ++DDC(c)->refs; }
static u32 WINAPI D_Release(CARA *c) {
  DD *d = DDC(c);
  if (--d->refs > 0) return d->refs;
  soltar(d);
  hx_soltar(d->tab16); hx_soltar(d->xmap); hx_soltar(d->fila);
  hx_soltar(d);
  return 0;
}
static HRESULT WINAPI D_Compact(CARA *c) { (void)c; return S_OK; }
static HRESULT WINAPI D_CreateClipper(CARA *c, u32 f, CLIP **p, void *u) {
  (void)c; (void)f;
  if (!p) return (HRESULT)hx_tr("CreateClipper", E_INVALIDARG);
  if (u) return (HRESULT)hx_tr("CreateClipper", CLASS_E_NOAGGREGATION);
  return (HRESULT)hx_tr("CreateClipper", (*p = clip_nuevo()) ? S_OK : E_OUTOFMEMORY);
}
static HRESULT WINAPI D_CreatePalette(CARA *c, u32 f, const u8 *e, PAL **p, void *u) {
  PAL *q;
  u32 n = (f & 4) ? 256 : (f & 2) ? 16 : (f & 1) ? 4 : (f & 0x800) ? 2 : 256;
  if (!p) return E_INVALIDARG;
  if (u) return CLASS_E_NOAGGREGATION;
  if (!(q = hx_pedir(sizeof(PAL)))) return E_OUTOFMEMORY;
  q->vt = vt_pal; q->refs = 1; q->dd = DDC(c); q->flags = f;
  if (e) hx_copia(q->e, e, n * 4);
  *p = q;
  return S_OK;
}
static HRESULT crear(DD *d, DDSD *x, CARA **pp, void *u, int ver) {
  u32 caps, w, h, bits, r = 0, g = 0, b = 0, n, k;
  SUP *s, *q, *ant;
  if (!x || !pp || (x->tam != 108 && x->tam != 124)) return (HRESULT)hx_tr("CreateSurface", E_INVALIDARG);
  if (u) return (HRESULT)hx_tr("CreateSurface", CLASS_E_NOAGGREGATION);
  *pp = NULL;
  caps = (x->flags & DDSD_CAPS) ? x->caps[0] : 0;
  if (caps & DDSCAPS_PRIMARYSURFACE) {
    if (d->prim) return (HRESULT)hx_tr("CreateSurface", DDH(561));    /* DDERR_PRIMARYSURFACEALREADYEXISTS */
    if (d->nivel & DDSCL_EXCLUSIVE) {
      if (!tomar(d)) return (HRESULT)hx_tr("CreateSurface", DDERR_NOEXCLUSIVEMODE);
      w = modo_w(d); h = modo_h(d); bits = modo_bits(d);
    } else {
      if (!d->p.ancho) HispaDXPantalla(7, (u32)&d->p, 0, 0);   /* (cómo es, sin tomarla) */
      w = d->p.ancho ? d->p.ancho : 640; h = d->p.alto ? d->p.alto : 480; bits = d->p.bits ? d->p.bits : 8;
      if (bits > 8) { const u8 *m = d->p.masc; r = ((1u << m[0]) - 1) << m[1]; g = ((1u << m[2]) - 1) << m[3]; b = ((1u << m[4]) - 1) << m[5]; }
      else bits = 32;                  /* en una ventana todo pasa por StretchDIBits, que
                                          convierte: con la pantalla de 8 bits se dice que
                                          es de 32 (los de la época pedían color verdadero) */
    }
    caps |= DDSCAPS_VISIBLE | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
    if (caps & DDSCAPS_FLIP) caps |= DDSCAPS_FRONTBUFFER;
    s = sup_nueva(d, w, h, bits, r, g, b, caps);
    if (!s) return (HRESULT)hx_tr("CreateSurface", E_OUTOFMEMORY);
    s->primaria = 1;
    d->prim = s;
    n = (x->flags & DDSD_BACKBUFFERCOUNT) ? x->traseros : 0;
    if ((caps & DDSCAPS_FLIP) && !n) n = 1;
    ant = s;
    for (k = 0; k < n && k < 3; k++) {
      q = sup_nueva(d, w, h, bits, r, g, b, (caps & ~(DDSCAPS_PRIMARYSURFACE | DDSCAPS_FRONTBUFFER | DDSCAPS_VISIBLE)) | (k == 0 ? DDSCAPS_BACKBUFFER : 0));
      if (!q) return (HRESULT)hx_tr("CreateSurface", E_OUTOFMEMORY);
      q->trasera = 1;
      ant->atras = q; ant = q;
    }
    if (n) ant->atras = s;
    if (d->tomada) d->borrar = 1;
    *pp = cara_sup(s, ver);
    return (HRESULT)hx_tr("CreateSurface", S_OK);
  }
  if (!(x->flags & DDSD_WIDTH) || !(x->flags & DDSD_HEIGHT) || !x->ancho || !x->alto || x->ancho > 8192 || x->alto > 8192) return (HRESULT)hx_tr("CreateSurface", E_INVALIDARG);
  w = x->ancho; h = x->alto;
  if (x->flags & DDSD_PIXELFORMAT) {
    if (x->pf.flags & DDPF_PALETTEINDEXED8) bits = 8;
    else if (x->pf.flags & DDPF_RGB) { bits = x->pf.bits; r = x->pf.r; g = x->pf.g; b = x->pf.b; if (bits == 16 && r == 0x7C00) bits = 15; }
    else if (caps & DDSCAPS_ZBUFFER) bits = 16;
    else return (HRESULT)hx_tr("CreateSurface", DDERR_INVALIDPIXELFORMAT);
    if (bits != 8 && bits != 15 && bits != 16 && bits != 24 && bits != 32) return (HRESULT)hx_tr("CreateSurface", DDERR_INVALIDPIXELFORMAT);
  } else if (caps & DDSCAPS_ZBUFFER) bits = 16;
  else {
    bits = d->prim ? d->prim->bits : modo_bits(d);
    if (d->prim) { r = d->prim->r; g = d->prim->g; b = d->prim->b; }
  }
  if (!(caps & (DDSCAPS_SYSTEMMEMORY | DDSCAPS_VIDEOMEMORY))) caps |= DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
  if (!(caps & ~(DDSCAPS_SYSTEMMEMORY | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM))) caps |= DDSCAPS_OFFSCREENPLAIN;
  s = sup_nueva(d, w, h, bits, r, g, b, caps);
  if (!s) return (HRESULT)hx_tr("CreateSurface", E_OUTOFMEMORY);
  if (x->flags & DDSD_CKSRCBLT) { s->hay_cks = 1; s->cks = x->ck_sblt; }
  if (x->flags & DDSD_CKDESTBLT) { s->hay_ckd = 1; s->ckd = x->ck_dblt; }
  if ((x->flags & DDSD_LPSURFACE) && x->mem) {
    hx_soltar(s->mem); s->mem = x->mem; s->mem_suya = 0;
    if (x->flags & DDSD_PITCH) s->paso = x->paso;
  }
  *pp = cara_sup(s, ver);
  return (HRESULT)hx_tr("CreateSurface", S_OK);
}
static HRESULT WINAPI D_CreateSurface(CARA *c, DDSD *x, CARA **pp, void *u) {
  return crear(DDC(c), x, pp, u, c->ver >= 7 ? 7 : c->ver >= 4 ? 4 : 1);
}
static HRESULT WINAPI D_DuplicateSurface(CARA *c, CARA *s, CARA **p) { (void)c; (void)s; (void)p; return E_NOTIMPL; }
typedef HRESULT WINAPI (*ENUMMODOS)(DDSD *, void *);
static HRESULT WINAPI D_EnumDisplayModes(CARA *c, u32 f, DDSD *filtro, void *ctx, ENUMMODOS cb) {
  static const u16 tam[][2] = { { 320, 200 }, { 320, 240 }, { 400, 300 }, { 512, 384 }, { 640, 400 }, { 640, 480 }, { 800, 600 }, { 1024, 768 } };
  static const u8 bits[] = { 8, 16, 24, 32 };
  DD *d = DDC(c);
  u32 i, j, sw;
  DDSD x;
  (void)f;
  if (!cb) return E_INVALIDARG;
  if (!d->p.ancho) HispaDXPantalla(7, (u32)&d->p, 0, 0);
  sw = d->p.ancho ? d->p.ancho : 800;
  for (j = 0; j < sizeof bits; j++)
    for (i = 0; i < sizeof tam / sizeof tam[0]; i++) {
      if (tam[i][0] > sw && tam[i][0] > 640) continue;
      if (filtro) {
        if ((filtro->flags & DDSD_WIDTH) && filtro->ancho != tam[i][0]) continue;
        if ((filtro->flags & DDSD_HEIGHT) && filtro->alto != tam[i][1]) continue;
        if ((filtro->flags & DDSD_PIXELFORMAT) && filtro->pf.bits != bits[j]) continue;
      }
      hx_cero(&x, sizeof x);
      x.tam = c->ver >= 4 ? 124 : 108;
      x.flags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_REFRESHRATE;
      x.ancho = tam[i][0]; x.alto = tam[i][1]; x.paso = tam[i][0] * bits[j] / 8; x.refresco = 60;
      { u32 r, g, b; fmt_de(bits[j], &r, &g, &b); pf_rellena(&x.pf, bits[j], r, g, b); }
      if (cb(&x, ctx) == 0) return S_OK;   /* DDENUMRET_CANCEL */
    }
  return S_OK;
}
static HRESULT WINAPI D_EnumSurfaces(CARA *c, u32 f, DDSD *x, void *ctx, void *cb) { (void)c; (void)f; (void)x; (void)ctx; (void)cb; return S_OK; }
static HRESULT WINAPI D_FlipToGDISurface(CARA *c) { (void)c; return S_OK; }
static HRESULT WINAPI D_GetCaps(CARA *c, u32 *hal, u32 *hel) {
  u32 *t[2] = { hal, hel }, k, n;
  (void)c;
  for (k = 0; k < 2; k++) {
    u32 *q = t[k];
    if (!q) continue;
    n = q[0] ? q[0] : 316;
    if (n > 380) n = 380;
    hx_cero(q + 1, n - 4);
    q[1] = 0x40 | 0x200 | 0x400000 | 0x4000000 | 0x80000000u;   /* BLT, BLTSTRETCH, COLORKEY, BLTCOLORFILL, NOHARDWARE... */
    q[1] = 0x40 | 0x200 | 0x400000 | 0x4000000;
    q[3] = 0x200 | 0x10;            /* DDCKEYCAPS_SRCBLT, DESTBLT */
    q[6] = 4;                       /* DDPCAPS_8BIT */
    q[15] = q[16] = 8 << 20;        /* 8 MB «de vídeo» */
  }
  return S_OK;
}
static HRESULT WINAPI D_GetDisplayMode(CARA *c, DDSD *x) {
  DD *d = DDC(c);
  u32 r, g, b;
  if (!x || (x->tam != 108 && x->tam != 124)) return E_INVALIDARG;
  if (!d->p.ancho) HispaDXPantalla(7, (u32)&d->p, 0, 0);
  hx_cero((u8 *)x + 4, x->tam - 4);
  x->flags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_REFRESHRATE;
  x->ancho = modo_w(d); x->alto = modo_h(d); x->refresco = 60;
  if (!x->ancho) { x->ancho = 640; x->alto = 480; }
  { u32 bits = modo_bits(d);
    if (!(d->nivel & DDSCL_EXCLUSIVE) && bits == 8) bits = 32;   /* (en una ventana: ver crear) */
    fmt_de(bits, &r, &g, &b);
    x->paso = x->ancho * oct_de(bits);
    pf_rellena(&x->pf, bits, r, g, b); }
  return S_OK;
}
static HRESULT WINAPI D_GetFourCCCodes(CARA *c, u32 *n, u32 *cod) { (void)c; (void)cod; if (n) *n = 0; return S_OK; }
static HRESULT WINAPI D_GetGDISurface(CARA *c, CARA **p) {
  DD *d = DDC(c);
  if (!p) return E_INVALIDARG;
  if (!d->prim) return DDERR_NOTFOUND;
  d->prim->refs++;
  *p = cara_sup(d->prim, c->ver >= 7 ? 7 : c->ver >= 4 ? 4 : 1);
  return S_OK;
}
static HRESULT WINAPI D_GetMonitorFrequency(CARA *c, u32 *f) { (void)c; if (!f) return E_INVALIDARG; *f = 60; return S_OK; }
static HRESULT WINAPI D_GetScanLine(CARA *c, u32 *l) { (void)c; if (!l) return E_INVALIDARG; *l = 0; return S_OK; }
static HRESULT WINAPI D_GetVerticalBlankStatus(CARA *c, int *b) { (void)c; if (!b) return E_INVALIDARG; *b = HispaDXPantalla(5, 0, 0, 0); return S_OK; }
static HRESULT WINAPI D_Initialize(CARA *c, GUID *g) { (void)c; (void)g; return DDERR_ALREADYINITIALIZED; }
static HRESULT WINAPI D_RestoreDisplayMode(CARA *c) {
  DD *d = DDC(c);
  soltar(d);
  d->mw = d->mh = d->mbits = 0;
  return S_OK;
}
static HRESULT WINAPI D_SetCooperativeLevel(CARA *c, HWND h, u32 f) {
  DD *d = DDC(c);
  if ((f & DDSCL_EXCLUSIVE) && !(f & DDSCL_FULLSCREEN)) return (HRESULT)hx_tr("SetCooperativeLevel", E_INVALIDARG);
  if (!(f & (DDSCL_NORMAL | DDSCL_EXCLUSIVE))) return (HRESULT)hx_tr("SetCooperativeLevel", E_INVALIDARG);
  if ((f & DDSCL_EXCLUSIVE) && !h) return (HRESULT)hx_tr("SetCooperativeLevel", E_INVALIDARG);
  if (h) d->hwnd = h;
  if (!(f & DDSCL_EXCLUSIVE) && (d->nivel & DDSCL_EXCLUSIVE)) soltar(d);
  d->nivel = f;
  return (HRESULT)hx_tr("SetCooperativeLevel", S_OK);
}
static HRESULT modo(DD *d, u32 w, u32 h, u32 bits) {
  if (!w || !h || w > 1600 || h > 1200) return (HRESULT)hx_tr("SetDisplayMode", DDERR_INVALIDMODE);
  if (bits != 8 && bits != 16 && bits != 24 && bits != 32 && bits != 15) return (HRESULT)hx_tr("SetDisplayMode", DDERR_UNSUPPORTEDMODE);
  if (!(d->nivel & DDSCL_EXCLUSIVE)) return (HRESULT)hx_tr("SetDisplayMode", DDERR_NOEXCLUSIVEMODE);
  d->mw = w; d->mh = h; d->mbits = bits;
  if (d->tomada) soltar(d);
  if (!tomar(d)) return (HRESULT)hx_tr("SetDisplayMode", DDERR_NOEXCLUSIVEMODE);
  borrar_todo(d);
  HispaMezBomba();
  return (HRESULT)hx_tr("SetDisplayMode", S_OK);
}
static HRESULT WINAPI D_SetDisplayMode1(CARA *c, u32 w, u32 h, u32 bits) { return modo(DDC(c), w, h, bits); }
static HRESULT WINAPI D_SetDisplayMode2(CARA *c, u32 w, u32 h, u32 bits, u32 hz, u32 f) { (void)hz; (void)f; return modo(DDC(c), w, h, bits); }
static HRESULT WINAPI D_WaitForVerticalBlank(CARA *c, u32 f, HANDLE e) { (void)c; (void)e; if (f & 4) { HispaDXPantalla(3, 0, 0, 0); return S_OK; } HispaDXPantalla(3, 0, 0, 0); return S_OK; }
static HRESULT WINAPI D_GetAvailableVidMem(CARA *c, u32 *caps, u32 *tot, u32 *libre) { (void)c; (void)caps; if (tot) *tot = 8 << 20; if (libre) *libre = 8 << 20; return S_OK; }
static HRESULT WINAPI D_GetSurfaceFromDC(CARA *c, HANDLE dc, CARA **p) {
  SUP *s;
  for (s = DDC(c)->sups; s; s = s->sig) if (s->dc == dc) { s->refs++; *p = cara_sup(s, c->ver); return S_OK; }
  return DDERR_NOTFOUND;
}
static HRESULT WINAPI D_RestoreAllSurfaces(CARA *c) {
  DD *d = DDC(c);
  SUP *s;
  recobrar(d);
  if (!d->tomada && (d->nivel & DDSCL_EXCLUSIVE)) return DDERR_SURFACELOST;
  for (s = d->sups; s; s = s->sig) s->epoca = d->epoca;
  return S_OK;
}
static HRESULT WINAPI D_TestCooperativeLevel(CARA *c) {
  DD *d = DDC(c);
  if (!(d->nivel & DDSCL_EXCLUSIVE)) return S_OK;
  if (d->tomada && HispaDXPantalla(4, 0, 0, 0)) return S_OK;
  return DDERR_NOEXCLUSIVEMODE;
}
static HRESULT WINAPI D_GetDeviceIdentifier(CARA *c, char *x, u32 f) {
  static const char a[] = "hispa486.drv", b[] = "Pantalla de Hispa-486 (VESA, sin aceleradora)";
  (void)c; (void)f;
  if (!x) return E_INVALIDARG;
  hx_cero(x, 1060);
  hx_copia(x, a, sizeof a);
  hx_copia(x + 512, b, sizeof b);
  return S_OK;
}
static HRESULT WINAPI D_StartModeTest(CARA *c, void *m, u32 n, u32 f) { (void)c; (void)m; (void)n; (void)f; return E_NOTIMPL; }
static HRESULT WINAPI D_EvaluateMode(CARA *c, u32 f, u32 *t) { (void)c; (void)f; (void)t; return E_NOTIMPL; }
#define TABLA_DD(SDM) { D_QueryInterface, D_AddRef, D_Release, D_Compact, D_CreateClipper, D_CreatePalette, \
  D_CreateSurface, D_DuplicateSurface, D_EnumDisplayModes, D_EnumSurfaces, D_FlipToGDISurface, D_GetCaps, \
  D_GetDisplayMode, D_GetFourCCCodes, D_GetGDISurface, D_GetMonitorFrequency, D_GetScanLine, \
  D_GetVerticalBlankStatus, D_Initialize, D_RestoreDisplayMode, D_SetCooperativeLevel, SDM, \
  D_WaitForVerticalBlank, D_GetAvailableVidMem, D_GetSurfaceFromDC, D_RestoreAllSurfaces, \
  D_TestCooperativeLevel, D_GetDeviceIdentifier, D_StartModeTest, D_EvaluateMode }
static void *tabla_dd1[] = TABLA_DD(D_SetDisplayMode1);
static void *tabla_dd2[] = TABLA_DD(D_SetDisplayMode2);

static DD *dd_nuevo(void) {
  DD *d = hx_pedir(sizeof(DD));
  int k;
  static const int v[4] = { 1, 2, 4, 7 };
  vt_sup = tabla_sup; vt_dd1 = tabla_dd1; vt_dd2 = tabla_dd2; vt_pal = tabla_pal; vt_clip = tabla_clip;
  if (!d) return NULL;
  for (k = 0; k < 4; k++) { d->cara[k].vt = k ? vt_dd2 : vt_dd1; d->cara[k].obj = d; d->cara[k].ver = v[k]; }
  d->refs = 1; d->nivel = DDSCL_NORMAL;
  return d;
}
HRESULT WINAPI DirectDrawCreate(GUID *g, CARA **p, void *u) {
  DD *d;
  (void)g;
  if (!p) return (HRESULT)hx_tr("DirectDrawCreate", E_INVALIDARG);
  *p = NULL;
  if (u) return (HRESULT)hx_tr("DirectDrawCreate", CLASS_E_NOAGGREGATION);
  if (!(d = dd_nuevo())) return (HRESULT)hx_tr("DirectDrawCreate", E_OUTOFMEMORY);
  *p = &d->cara[0];
  return (HRESULT)hx_tr("DirectDrawCreate", S_OK);
}
HRESULT WINAPI DirectDrawCreateEx(GUID *g, void **p, const GUID *iid, void *u) {
  DD *d;
  (void)g;
  if (!p) return (HRESULT)hx_tr("DirectDrawCreateEx", E_INVALIDARG);
  *p = NULL;
  if (u) return (HRESULT)hx_tr("DirectDrawCreateEx", CLASS_E_NOAGGREGATION);
  if (iid && !hx_guid(iid, 0x15E65EC0)) return (HRESULT)hx_tr("DirectDrawCreateEx", E_INVALIDARG);   /* solo IDirectDraw7 */
  if (!(d = dd_nuevo())) return (HRESULT)hx_tr("DirectDrawCreateEx", E_OUTOFMEMORY);
  *p = &d->cara[3];
  return (HRESULT)hx_tr("DirectDrawCreateEx", S_OK);
}
HRESULT WINAPI DirectDrawCreateClipper(u32 f, CLIP **p, void *u) {
  (void)f;
  vt_clip = tabla_clip;
  if (!p) return E_INVALIDARG;
  if (u) return CLASS_E_NOAGGREGATION;
  return (*p = clip_nuevo()) ? S_OK : E_OUTOFMEMORY;
}
typedef int WINAPI (*DDENUMCB)(GUID *, char *, char *, void *);
typedef int WINAPI (*DDENUMCBEX)(GUID *, char *, char *, void *, HANDLE);
HRESULT WINAPI DirectDrawEnumerateA(DDENUMCB cb, void *ctx) {
  if (!cb) return E_INVALIDARG;
  cb(NULL, "Pantalla de Hispa-486", "display", ctx);
  return S_OK;
}
HRESULT WINAPI DirectDrawEnumerateExA(DDENUMCBEX cb, void *ctx, u32 f) {
  (void)f;
  if (!cb) return E_INVALIDARG;
  cb(NULL, "Pantalla de Hispa-486", "display", ctx, NULL);
  return S_OK;
}
HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
HRESULT WINAPI DllGetClassObject(const GUID *a, const GUID *b, void **p) { (void)a; (void)b; if (p) *p = NULL; return 0x80040111; }
