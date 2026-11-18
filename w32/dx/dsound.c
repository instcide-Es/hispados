/* dsound.c - DSOUND.DLL de Hispa-486 (fase 5, docs/486/HISPA-486.md):
 * DirectSound 3 a 7 (y lo de IDirectSound8 que es igual) sobre la mezcla de
 * WINMM.DLL (HispaMez*), que va a HSPSND. IDirectSound (CreateSoundBuffer,
 * GetCaps, DuplicateSoundBuffer, SetCooperativeLevel...) e
 * IDirectSoundBuffer: búfer primario (SetFormat; con DSSCL_WRITEPRIMARY se
 * puede escribir en él y suena) y secundarios, cada uno una voz de la mezcla
 * por programa: volumen, panorama, frecuencia, bucles, GetCurrentPosition,
 * SetCurrentPosition, Lock/Unlock con la vuelta del anillo, GetStatus.
 * IDirectSoundNotify se acepta (sin hilos, los avisos no se esperan).
 * Sala limpia: hecho a partir de la documentación pública del API.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "dx.h"

void *memcpy(void *d, const void *s, u32 n) { hx_copia(d, s, n); return d; }
void *memset(void *d, int c, u32 n) { u8 *p = d; while (n--) *p++ = c; return d; }

#define DSERR_ALLOCATED ((HRESULT)0x8878000A)
#define DSERR_CONTROLUNAVAIL ((HRESULT)0x8878001E)
#define DSERR_INVALIDCALL ((HRESULT)0x88780032)
#define DSERR_NODRIVER ((HRESULT)0x88780078)
#define DSERR_BUFFERLOST ((HRESULT)0x88780096)
#define DSBCAPS_PRIMARYBUFFER 1
#define DSBCAPS_STATIC 2
#define DSBCAPS_CTRLFREQUENCY 0x20
#define DSBCAPS_CTRLPAN 0x40
#define DSBCAPS_CTRLVOLUME 0x80
#define DSBCAPS_CTRLPOSITIONNOTIFY 0x100
#define DSSCL_WRITEPRIMARY 4

typedef struct { u16 fmt, canales; u32 hz, octps; u16 bloque, bits; u16 extra; } WAVEFORMATEX;
typedef struct { u32 dwSize, dwFlags, dwBufferBytes, dwReserved; WAVEFORMATEX *lpwfxFormat; GUID g3d; } DSBUFFERDESC;
typedef struct { u32 tam, flags, bytes, ritmo, cpu; } DSBCAPS;

typedef struct { int refs; u8 *d; u32 n; } DATOS;     /* compartidos por los duplicados */
typedef struct DSB {
  void **vt;
  void **vt_notif;                  /* IDirectSoundNotify (QueryInterface) */
  int refs;
  struct DS *ds;
  DATOS *datos;
  WAVEFORMATEX f;
  u32 flags, bpt;                   /* octetos por trama */
  int primario, sonando, bucle;
  int vol, pan;
  u32 hz;
  HxVoz voz;
  struct DSB *sig;
} DSB;
typedef struct DS {
  void **vt;
  int refs;
  u32 nivel;
  DSB *primario;
  DSB *bufs;
} DS;

static void ganancias(DSB *b) {
  int g = HispaMezGanancia(b->vol);
  int gi = g, gd = g;
  if (b->pan > 0) gi = (g * HispaMezGanancia(-b->pan)) >> 12;
  else if (b->pan < 0) gd = (g * HispaMezGanancia(b->pan)) >> 12;
  b->voz.vol_i = gi; b->voz.vol_d = gd;
}
static void voz_prep(DSB *b) {
  b->voz.datos = b->datos->d;
  b->voz.bits = b->f.bits; b->voz.canales = b->f.canales;
  b->bpt = b->f.bits / 8 * b->f.canales;
  if (!b->bpt) b->bpt = 1;
  b->voz.tramas = b->datos->n / b->bpt;
  b->voz.paso = HispaMezPaso(b->hz ? b->hz : b->f.hz);
  b->voz.dueno = b;
  ganancias(b);
}

/* ---------------- IDirectSoundBuffer ---------------- */
static void **vt_dsb, **vt_notif;
static HRESULT WINAPI B_QueryInterface(DSB *b, const GUID *g, void **p) {
  if (!p) return E_POINTER;
  if (hx_guid(g, 0x279AFA85) || hx_guid(g, 0) /* IUnknown */ || hx_guid(g, 0x6825A449) /* IDirectSoundBuffer8 */) { b->refs++; *p = b; return S_OK; }
  if (hx_guid(g, 0xB0210783)) { b->refs++; *p = &b->vt_notif; return S_OK; }   /* IDirectSoundNotify */
  *p = NULL;
  return E_NOINTERFACE;
}
static u32 WINAPI B_AddRef(DSB *b) { return ++b->refs; }
static u32 WINAPI B_Release(DSB *b) {
  DSB **p;
  if (--b->refs > 0) return b->refs;
  HispaMezQuita(&b->voz);
  for (p = &b->ds->bufs; *p; p = &(*p)->sig) if (*p == b) { *p = b->sig; break; }
  if (b->ds->primario == b) b->ds->primario = NULL;
  if (--b->datos->refs <= 0) { hx_soltar(b->datos->d); hx_soltar(b->datos); }
  hx_soltar(b);
  return 0;
}
static HRESULT WINAPI B_GetCaps(DSB *b, DSBCAPS *c) {
  if (!c || c->tam < sizeof(DSBCAPS)) return E_INVALIDARG;
  c->flags = b->flags | 0x8;        /* DSBCAPS_LOCSOFTWARE */
  c->bytes = b->datos->n; c->ritmo = 0; c->cpu = 0;
  return S_OK;
}
static HRESULT WINAPI B_GetCurrentPosition(DSB *b, u32 *jug, u32 *esc) {
  u32 p;
  HispaMezBomba();
  p = b->voz.pos * b->bpt;
  if (p >= b->datos->n) p = 0;
  if (jug) *jug = p;
  if (esc) { p += b->bpt * (b->sonando ? 64 : 0); *esc = b->datos->n ? p % b->datos->n : 0; }
  return S_OK;
}
static HRESULT WINAPI B_GetFormat(DSB *b, WAVEFORMATEX *f, u32 n, u32 *esc) {
  if (esc) *esc = 18;
  if (f) hx_copia(f, &b->f, n < 18 ? n : 18);
  return S_OK;
}
static HRESULT WINAPI B_GetVolume(DSB *b, int *v) { if (!v) return E_INVALIDARG; *v = b->vol; return S_OK; }
static HRESULT WINAPI B_GetPan(DSB *b, int *v) { if (!v) return E_INVALIDARG; *v = b->pan; return S_OK; }
static HRESULT WINAPI B_GetFrequency(DSB *b, u32 *v) { if (!v) return E_INVALIDARG; *v = b->hz ? b->hz : b->f.hz; return S_OK; }
static HRESULT WINAPI B_GetStatus(DSB *b, u32 *s) {
  if (!s) return E_INVALIDARG;
  HispaMezBomba();
  if (b->sonando && !b->voz.activa) { b->sonando = 0; b->voz.pos = 0; }
  *s = b->sonando ? (1 | (b->bucle ? 4 : 0)) : 0;
  if (b->primario) *s = 1 | 4;
  return S_OK;
}
static HRESULT WINAPI B_Initialize(DSB *b, void *ds, const DSBUFFERDESC *d) { (void)b; (void)ds; (void)d; return 0x88780082; /* DSERR_ALREADYINITIALIZED */ }
static HRESULT WINAPI B_Lock(DSB *b, u32 off, u32 n, void **p1, u32 *n1, void **p2, u32 *n2, u32 fl) {
  u32 t = b->datos->n;
  if (!p1 || !n1 || !t) return E_INVALIDARG;
  if (fl & 1) B_GetCurrentPosition(b, NULL, &off);   /* DSBLOCK_FROMWRITECURSOR */
  if (fl & 2) { off = 0; n = t; }                    /* DSBLOCK_ENTIREBUFFER */
  if (off >= t || n > t) return E_INVALIDARG;
  *p1 = b->datos->d + off;
  if (off + n <= t) { *n1 = n; if (p2) *p2 = NULL; if (n2) *n2 = 0; }
  else { *n1 = t - off; if (p2) *p2 = b->datos->d; if (n2) *n2 = n - (t - off); }
  return S_OK;
}
static HRESULT WINAPI B_Play(DSB *b, u32 r1, u32 prio, u32 fl) {
  (void)r1; (void)prio;
  if (b->primario) { if (b->ds->nivel == DSSCL_WRITEPRIMARY) { b->voz.bucle = 1; HispaMezPon(&b->voz); } return S_OK; }
  b->bucle = fl & 1;
  b->voz.bucle = b->bucle;
  if (b->voz.pos >= b->voz.tramas) { b->voz.pos = 0; b->voz.frac = 0; }
  b->sonando = 1;
  HispaMezPon(&b->voz);
  HispaMezBomba();
  return S_OK;
}
static HRESULT WINAPI B_SetCurrentPosition(DSB *b, u32 p) {
  if (p >= b->datos->n) return E_INVALIDARG;
  b->voz.pos = p / b->bpt; b->voz.frac = 0;
  return S_OK;
}
static HRESULT WINAPI B_SetFormat(DSB *b, const WAVEFORMATEX *f) {
  if (!b->primario) return DSERR_INVALIDCALL;
  if (!f || f->fmt != 1 || (f->bits != 8 && f->bits != 16) || f->canales < 1 || f->canales > 2) return E_INVALIDARG;
  hx_copia(&b->f, f, 16);
  b->f.bloque = f->bits / 8 * f->canales; b->f.octps = b->f.hz * b->f.bloque;
  voz_prep(b);
  return S_OK;
}
static HRESULT WINAPI B_SetVolume(DSB *b, int v) {
  if (v > 0 || v < -10000) return E_INVALIDARG;
  b->vol = v; ganancias(b); return S_OK;
}
static HRESULT WINAPI B_SetPan(DSB *b, int v) {
  if (v > 10000 || v < -10000) return E_INVALIDARG;
  b->pan = v; ganancias(b); return S_OK;
}
static HRESULT WINAPI B_SetFrequency(DSB *b, u32 hz) {
  if (b->primario) return DSERR_CONTROLUNAVAIL;
  if (hz && (hz < 100 || hz > 200000)) return E_INVALIDARG;
  b->hz = hz;
  b->voz.paso = HispaMezPaso(hz ? hz : b->f.hz);
  return S_OK;
}
static HRESULT WINAPI B_Stop(DSB *b) {
  if (b->primario) { if (b->voz.activa) HispaMezQuita(&b->voz); return S_OK; }
  b->sonando = 0;
  HispaMezQuita(&b->voz);
  return S_OK;
}
static HRESULT WINAPI B_Unlock(DSB *b, void *p1, u32 n1, void *p2, u32 n2) { (void)b; (void)p1; (void)n1; (void)p2; (void)n2; return S_OK; }
static HRESULT WINAPI B_Restore(DSB *b) { (void)b; return S_OK; }
/* IDirectSoundNotify: this = &b->vt_notif */
#define NB(p) ((DSB *)((u8 *)(p) - 4))
static HRESULT WINAPI N_QueryInterface(void *p, const GUID *g, void **q) { return B_QueryInterface(NB(p), g, q); }
static u32 WINAPI N_AddRef(void *p) { return B_AddRef(NB(p)); }
static u32 WINAPI N_Release(void *p) { return B_Release(NB(p)); }
static HRESULT WINAPI N_SetNotificationPositions(void *p, u32 n, const void *pos) { (void)p; (void)n; (void)pos; return S_OK; }
static void *tabla_dsb[] = { B_QueryInterface, B_AddRef, B_Release, B_GetCaps, B_GetCurrentPosition, B_GetFormat,
  B_GetVolume, B_GetPan, B_GetFrequency, B_GetStatus, B_Initialize, B_Lock, B_Play, B_SetCurrentPosition, B_SetFormat,
  B_SetVolume, B_SetPan, B_SetFrequency, B_Stop, B_Unlock, B_Restore };
static void *tabla_notif[] = { N_QueryInterface, N_AddRef, N_Release, N_SetNotificationPositions };

static DSB *dsb_nuevo(DS *ds, DATOS *dat, const WAVEFORMATEX *f, u32 flags) {
  DSB *b = hx_pedir(sizeof(DSB));
  if (!b) return NULL;
  vt_dsb = tabla_dsb; vt_notif = tabla_notif;
  b->vt = vt_dsb; b->vt_notif = vt_notif;
  b->refs = 1; b->ds = ds; b->datos = dat; dat->refs++;
  hx_copia(&b->f, f, 16);
  b->flags = flags;
  voz_prep(b);
  b->sig = ds->bufs; ds->bufs = b;
  return b;
}

/* ---------------- IDirectSound ---------------- */
static HRESULT WINAPI S_QueryInterface(DS *s, const GUID *g, void **p) {
  if (!p) return E_POINTER;
  if (hx_guid(g, 0x279AFA83) || hx_guid(g, 0xC50A7E93) || hx_guid(g, 0)) { s->refs++; *p = s; return S_OK; }
  *p = NULL;
  return E_NOINTERFACE;
}
static u32 WINAPI S_AddRef(DS *s) { return ++s->refs; }
static u32 WINAPI S_Release(DS *s) {
  if (--s->refs > 0) return s->refs;
  while (s->bufs) { DSB *b = s->bufs; b->refs = 1; B_Release(b); }
  hx_soltar(s);
  return 0;
}
static HRESULT WINAPI S_CreateSoundBuffer(DS *s, const DSBUFFERDESC *d, DSB **pb, void *unk) {
  DATOS *dat;
  DSB *b;
  WAVEFORMATEX f;
  if (!d || !pb || (d->dwSize != 20 && d->dwSize != 36)) return E_INVALIDARG;
  if (unk) return CLASS_E_NOAGGREGATION;
  *pb = NULL;
  if (d->dwFlags & DSBCAPS_PRIMARYBUFFER) {
    if (s->primario) { s->primario->refs++; *pb = s->primario; return S_OK; }
    f.fmt = 1; f.canales = 2; f.hz = 22050; f.bits = 16; f.bloque = 4; f.octps = 88200; f.extra = 0;
    dat = hx_pedir(sizeof(DATOS));
    if (!dat || !(dat->d = hx_pedir(44100))) { hx_soltar(dat); return E_OUTOFMEMORY; }
    dat->n = 44100;                 /* medio segundo */
    b = dsb_nuevo(s, dat, &f, d->dwFlags | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN);
    if (!b) return E_OUTOFMEMORY;
    dat->refs--;
    b->primario = 1;
    s->primario = b;
    *pb = b;
    return S_OK;
  }
  if (!d->lpwfxFormat) return E_INVALIDARG;
  hx_copia(&f, d->lpwfxFormat, 16);
  if (f.fmt != 1 || (f.bits != 8 && f.bits != 16) || f.canales < 1 || f.canales > 2 || !f.hz) return E_INVALIDARG;
  if (d->dwBufferBytes < 4 || d->dwBufferBytes > 0x0FFFFFFF) return E_INVALIDARG;
  dat = hx_pedir(sizeof(DATOS));
  if (!dat || !(dat->d = hx_pedir(d->dwBufferBytes))) { hx_soltar(dat); return E_OUTOFMEMORY; }
  dat->n = d->dwBufferBytes;
  if (f.bits == 8) { u32 k; for (k = 0; k < dat->n; k++) dat->d[k] = 0x80; }
  b = dsb_nuevo(s, dat, &f, d->dwFlags);
  if (!b) return E_OUTOFMEMORY;
  dat->refs--;
  *pb = b;
  return S_OK;
}
typedef struct { u32 tam, flags, minhz, maxhz, prim, mezcla[3], libres[3], hw3d[3], libres3d[3], mem, memlibre, memgrande, ritmo, cpu, r1, r2; } DSCAPS;   /* 96 */
static HRESULT WINAPI S_GetCaps(DS *s, DSCAPS *c) {
  (void)s;
  if (!c || c->tam < sizeof(DSCAPS)) return E_INVALIDARG;
  hx_cero((u8 *)c + 4, sizeof(DSCAPS) - 4);
  c->flags = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x100 | 0x200 | 0x400 | 0x800;   /* mono, estéreo, 8 y 16 bits, frecuencia continua, secundarios (no DSCAPS_EMULDRIVER) */
  c->minhz = 100; c->maxhz = 200000;
  c->prim = 1;
  return S_OK;
}
static HRESULT WINAPI S_DuplicateSoundBuffer(DS *s, DSB *o, DSB **pb) {
  DSB *b;
  if (!o || !pb || o->primario) return E_INVALIDARG;
  b = dsb_nuevo(s, o->datos, &o->f, o->flags);
  if (!b) return E_OUTOFMEMORY;
  b->vol = o->vol; b->pan = o->pan; b->hz = o->hz;
  voz_prep(b);
  *pb = b;
  return S_OK;
}
static HRESULT WINAPI S_SetCooperativeLevel(DS *s, HWND h, u32 n) { (void)h; if (n < 1 || n > 4) return E_INVALIDARG; s->nivel = n; return S_OK; }
static HRESULT WINAPI S_Compact(DS *s) { (void)s; return S_OK; }
static HRESULT WINAPI S_GetSpeakerConfig(DS *s, u32 *c) { (void)s; if (!c) return E_INVALIDARG; *c = 4; /* DSSPEAKER_STEREO */ return S_OK; }
static HRESULT WINAPI S_SetSpeakerConfig(DS *s, u32 c) { (void)s; (void)c; return S_OK; }
static HRESULT WINAPI S_Initialize(DS *s, const GUID *g) { (void)s; (void)g; return 0x88780082; }
static HRESULT WINAPI S_VerifyCertification(DS *s, u32 *c) { (void)s; if (c) *c = 1; /* DS_UNCERTIFIED */ return S_OK; }
static void *tabla_ds[] = { S_QueryInterface, S_AddRef, S_Release, S_CreateSoundBuffer, S_GetCaps, S_DuplicateSoundBuffer,
  S_SetCooperativeLevel, S_Compact, S_GetSpeakerConfig, S_SetSpeakerConfig, S_Initialize, S_VerifyCertification };

HRESULT WINAPI DirectSoundCreate(const GUID *g, DS **pds, void *unk) {
  DS *s;
  (void)g;
  if (!pds) return E_INVALIDARG;
  *pds = NULL;
  if (unk) return CLASS_E_NOAGGREGATION;
  HispaMezHz();
  if (HispaDXSonido(0, 0, 0, 0) <= 0) return DSERR_NODRIVER;
  if (!(s = hx_pedir(sizeof(DS)))) return E_OUTOFMEMORY;
  s->vt = tabla_ds; s->refs = 1; s->nivel = 1;
  *pds = s;
  return S_OK;
}
HRESULT WINAPI DirectSoundCreate8(const GUID *g, DS **pds, void *unk) { return DirectSoundCreate(g, pds, unk); }
typedef int WINAPI (*DSENUMCB)(GUID *, const char *, const char *, void *);
HRESULT WINAPI DirectSoundEnumerateA(DSENUMCB cb, void *ctx) {
  if (!cb) return E_INVALIDARG;
  if (HispaDXSonido(0, 0, 0, 0) <= 0) return S_OK;
  cb(NULL, "Sonido principal (HSPSND)", "", ctx);
  return S_OK;
}
HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
HRESULT WINAPI DllGetClassObject(const GUID *a, const GUID *b, void **p) { (void)a; (void)b; if (p) *p = NULL; return 0x80040111; }
