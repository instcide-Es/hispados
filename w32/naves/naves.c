/* naves.c - «Naves», el juego de ejemplo de Hispa32 (fase 5 de Hispa-486,
 * docs/486/HISPA-486.md): un matamarcianos que usa todo lo de DirectX:
 *  - DirectDraw: pantalla completa en 320×240 a 8 bits con su paleta, un
 *    búfer trasero (Flip), los dibujos en superficies aparte con clave de
 *    color (BltFast), el fondo con Blt de color, la portada con GetDC;
 *  - DirectSound: la música (hecha aquí, en un búfer que suena en bucle) y
 *    los efectos (disparo y explosión: búferes duplicados para que suenen
 *    varios a la vez, con panorama según dónde pasa);
 *  - DirectInput: el teclado (flechas, espacio, Esc) y, si lo hay, el mando
 *    de HSPJOY (por WINMM: joyGetPosEx);
 *  - WINMM: timeGetTime para ir a 50 fotogramas por segundo.
 * Uso: NAVES [/PRUEBA] (con /PRUEBA las oleadas no se mueven: así las
 * capturas de tests/run_naves_tests.sh salen siempre igual).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --gui (ver el Makefile). */
#include "../../tests/progs/win32min.h"
#include "../../tests/progs/win32gdi.h"
#include "../dx/dxapi.h"
void *memset(void *, int, u32);
void *memcpy(void *, const void *, u32);
char *strstr(const char *, const char *);
typedef struct { u32 tam, flags, x, y, z, r, u, v, bot, nbot, pov, r1, r2; } JOYINFOEX;
u32 WINAPI joyGetPosEx(u32, JOYINFOEX *);
HANDLE WINAPI SetCursor(HANDLE);
#define WS_POPUP 0x80000000

#define AN 320
#define AL 240
/* los colores de la paleta */
enum { C_NEGRO, C_BLANCO, C_GRIS, C_AZUL, C_CIAN, C_ROJO, C_NARANJA, C_AMARILLO, C_VERDE, C_VERDEO, C_MORADO, C_CREMA, C_TERRA, C_GRISO };
static const u8 pal_rgb[][3] = {
  { 0, 0, 0 }, { 255, 255, 255 }, { 150, 150, 160 }, { 40, 80, 220 }, { 80, 220, 255 }, { 230, 30, 30 },
  { 255, 140, 0 }, { 255, 230, 40 }, { 40, 220, 60 }, { 20, 120, 40 }, { 170, 60, 200 }, { 240, 228, 200 },
  { 196, 90, 50 }, { 70, 70, 80 } };

static HANDLE ven;
static void *dd, *prim, *atras, *pal, *s_nave, *s_mal, *s_bala, *s_bum, *s_letras;
static void *ds, *b_musica, *b_tiro[4], *b_bum[4];
static void *di, *tec;
static int prueba, salir;

/* ---------- los dibujos (a mano, en texto: un carácter, un color) ---------- */
static const char *dib_nave[] = {
  "       aa       ", "       aa       ", "      abba      ", "      abba      ", "     abbbba     ",
  " r   abccba   r ", " r  abbccbba  r ", "aaa abbbbbba aaa", "abaabbbbbbbbaaba", "abbbbbbbbbbbbbba",
  "aabbbbaaaabbbbaa", " aaaa  oo  aaaa ", "       yy       ", "                ", "                ", "                " };
static const char *dib_mal[] = {
  "   v        v   ", "    v      v    ", "   mmmmmmmmmm   ", "  mmwmmmmmmwmm  ", " mmmkwmmmmwkmmm ",
  "mmmmmmmmmmmmmmmm", "m mmmmmmmmmmmm m", "m m          m m", "   mm      mm   ", "                ",
  "                ", "                ", "                ", "                ", "                ", "                " };
static const char *dib_bala[] = { " y ", "yyy", "yoy", " o ", " o " };
static u8 color_de(char c) {
  switch (c) {
  case 'a': return C_GRIS; case 'b': return C_BLANCO; case 'c': return C_CIAN; case 'r': return C_ROJO;
  case 'o': return C_NARANJA; case 'y': return C_AMARILLO; case 'm': return C_VERDE; case 'v': return C_VERDEO;
  case 'w': return C_BLANCO; case 'k': return C_NEGRO + 0; default: return 0;
  }
}
static void *superficie(int w, int h) {
  DDSURFACEDESC d;
  void *s = NULL;
  DDCOLORKEY k = { 0, 0 };
  memset(&d, 0, sizeof d);
  d.tam = sizeof d; d.flags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; d.ancho = w; d.alto = h;
  d.caps = DDSCAPS_OFFSCREENPLAIN;
  if (DD_CreateSurface(dd, &d, &s)) return NULL;
  S_SetColorKey(s, DDCKEY_SRCBLT, &k);
  return s;
}
static void dibuja(void *s, const char **dib, int w, int h) {
  DDSURFACEDESC d;
  int x, y;
  memset(&d, 0, sizeof d); d.tam = sizeof d;
  if (S_Lock(s, NULL, &d, DDLOCK_WAIT)) return;
  for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
    u8 c = color_de(dib[y][x]);
    if (dib[y][x] == 'k') c = C_GRISO;
    ((u8 *)d.mem)[y * d.paso + x] = c;
  }
  S_Unlock(s, NULL);
}
/* la explosión: 4 tamaños de 16×16 seguidos */
static void dibuja_bum(void) {
  DDSURFACEDESC d;
  int f, x, y;
  memset(&d, 0, sizeof d); d.tam = sizeof d;
  if (S_Lock(s_bum, NULL, &d, DDLOCK_WAIT)) return;
  for (f = 0; f < 4; f++) for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
    int dx = x - 8, dy = y - 8, r2 = dx * dx + dy * dy, r = 2 + f * 2;
    u8 c = 0;
    if (r2 <= r * r) c = r2 < (r - 2) * (r - 2) ? C_AMARILLO : ((x + y + f) & 1) ? C_NARANJA : C_ROJO;
    if (f == 3 && ((x * 7 + y * 3) % 5)) c = c ? C_GRIS : 0;
    ((u8 *)d.mem)[y * d.paso + f * 16 + x] = c;
  }
  S_Unlock(s_bum, NULL);
}
/* las cifras y letras de 3×5 (para la puntuación, sin GDI en cada fotograma) */
static const char *letra(char c) {
  static const char *cif[] = { "111101101101111", "010110010010111", "111001111100111", "111001111001111", "101101111001001",
    "111100111001111", "111100111101111", "111001010010010", "111101111101111", "111101111001111" };
  if (c >= '0' && c <= '9') return cif[c - '0'];
  switch (c) {
  case 'P': return "111101111100100"; case 'U': return "101101101101111"; case 'N': return "110101101101101";
  case 'T': return "111010010010010"; case 'O': return "111101101101111"; case 'S': return "111100111001111";
  case 'V': return "101101101101010"; case 'I': return "111010010010111"; case 'D': return "110101101101110";
  case 'A': return "111101111101101"; case 'E': return "111100111100111"; case 'L': return "100100100100111";
  case 'F': return "111100111100100"; case 'M': return "101111111101101"; case 'G': return "111100101101111";
  case 'R': return "110101110101101"; case 'C': return "111100100100111"; case ':': return "000010000010000";
  }
  return "000000000000000";
}
static void texto(DDSURFACEDESC *d, int x, int y, const char *t, u8 col) {
  int k, i, j;
  for (k = 0; t[k]; k++, x += 4) {
    const char *l = letra(t[k]);
    for (j = 0; j < 5; j++) for (i = 0; i < 3; i++)
      if (l[j * 3 + i] == '1' && x + i >= 0 && x + i < AN && y + j >= 0 && y + j < AL) ((u8 *)d->mem)[(y + j) * d->paso + x + i] = col;
  }
}

/* ---------- el sonido (todo hecho aquí) ---------- */
static u32 semilla = 12345;
static int azar(int n) { semilla = semilla * 1103515245 + 12345; return (semilla >> 16) % n; }
static void *bufer(int bytes, int hz, short **datos) {
  WAVEFORMATEX f = { 1, 1, 0, 0, 2, 16, 0 };
  DSBUFFERDESC d = { 20, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_CTRLFREQUENCY | DSBCAPS_STATIC, 0, 0, &f };
  void *b = NULL, *p2; u32 n1, n2;
  f.hz = hz; f.octps = hz * 2; d.bytes = bytes;
  if (!ds || DS_CreateSoundBuffer(ds, &d, &b)) return NULL;
  if (B_Lock(b, 0, bytes, datos, &n1, &p2, &n2, 0)) { Release(b); return NULL; }
  return b;
}
static void sonidos(void) {
  short *p; int k, n;
  void *copia;
  if (DirectSoundCreate(NULL, &ds, NULL)) { ds = NULL; return; }
  DS_SetCooperativeLevel(ds, ven, DSSCL_PRIORITY);
  /* el disparo: un pitido que baja (cuadrada), 0,12 s */
  n = 22050 * 12 / 100;
  if ((b_tiro[0] = bufer(n * 2, 22050, &p))) {
    u32 fase = 0;
    for (k = 0; k < n; k++) { fase += 0x10000 * (1600 - k * 1200 / n) / 22050; p[k] = ((fase >> 15) & 1 ? 1 : -1) * (6000 - k * 5000 / n); }
    B_Unlock(b_tiro[0], p, n * 2, NULL, 0);
  }
  /* la explosión: ruido que se apaga, 0,5 s */
  n = 22050 / 2;
  if ((b_bum[0] = bufer(n * 2, 22050, &p))) {
    int v = 0;
    for (k = 0; k < n; k++) { if (!(k % 3)) v = azar(20000) - 10000; p[k] = v * (n - k) / n; }
    B_Unlock(b_bum[0], p, n * 2, NULL, 0);
  }
  for (k = 1; k < 4; k++) {         /* copias, para que suenen a la vez */
    copia = NULL;
    if (b_tiro[0] && C2(ds, 5, b_tiro[0], &copia) == 0) b_tiro[k] = copia;
    copia = NULL;
    if (b_bum[0] && C2(ds, 5, b_bum[0], &copia) == 0) b_bum[k] = copia;
  }
  /* la música: un bajo y una melodía de 4 compases (3,2 s), en bucle */
  {
    static const u8 mel[32] = { 64, 0, 67, 0, 71, 69, 67, 0, 64, 0, 67, 0, 72, 71, 69, 0, 62, 0, 65, 0, 69, 67, 65, 0, 62, 64, 65, 67, 69, 0, 71, 0 };
    static const u8 baj[8] = { 40, 40, 43, 43, 38, 38, 43, 45 };
    static const u16 hz_nota[12] = { 26163, 27718, 29366, 31113, 32963, 34923, 36999, 39200, 41530, 44000, 46616, 49388 };
    int t = 22050 * 32 / 10, paso = t / 32;
    if ((b_musica = bufer(t * 2, 22050, &p))) {
      u32 f1 = 0, f2 = 0;
      for (k = 0; k < t; k++) {
        int nm = mel[k / paso], nb = baj[k / (paso * 4)], v = 0, en = k % paso;
        u32 h1 = nm ? (u32)hz_nota[nm % 12] * (1u << (nm / 12)) / 3200 : 0;   /* centésimas de Hz -> Hz × 2^... */
        u32 h2 = (u32)hz_nota[nb % 12] * (1u << (nb / 12)) / 3200;
        f1 += h1 * 0x10000 / 22050 / 100 * 100 / 100;
        f2 += h2 * 0x10000 / 22050;
        if (nm) v += ((f1 >> 15) & 1 ? 2200 : -2200) * (paso - en) / paso;
        v += (f2 >> 14) & 2 ? 1800 : -1800;
        p[k] = v;
      }
      B_Unlock(b_musica, p, t * 2, NULL, 0);
      B_SetVolume(b_musica, -600);
      B_Play(b_musica, DSBPLAY_LOOPING);
    }
  }
}
static void suena(void **b, int x) {
  int k;
  for (k = 0; k < 4; k++) {
    u32 st = 0;
    if (!b[k]) continue;
    B_GetStatus(b[k], &st);
    if (!(st & DSBSTATUS_PLAYING)) { B_SetPan(b[k], (x - AN / 2) * 3000 / (AN / 2)); B_SetCurrentPosition(b[k], 0); B_Play(b[k], 0); return; }
  }
}

/* ---------- el teclado y el mando ---------- */
static DIOBJECTDATAFORMAT obj_tec[256];
static DIDATAFORMAT df_tec = { 24, 16, 2, 256, 256, obj_tec };
static u8 teclas[256];
static int espacio_pulsado;
static void entrada_ini(void) {
  int k;
  DIPROPDWORD p = { 20, 16, 0, 0, 16 };
  for (k = 0; k < 256; k++) { obj_tec[k].g = &GUID_Key; obj_tec[k].ofs = k; obj_tec[k].tipo = 0x8000000C | (k << 8); }
  if (DirectInputCreateA(GetModuleHandleA(NULL), 0x0700, &di, NULL)) return;
  if (DI_CreateDevice(di, &GUID_SysKeyboard, &tec)) { tec = NULL; return; }
  DV_SetDataFormat(tec, &df_tec);
  DV_SetCooperativeLevel(tec, ven, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
  DV_SetProperty(tec, 1, &p);
  DV_Acquire(tec);
}
static void entrada(int *dx, int *dy, int *fuego) {
  DIDEVICEOBJECTDATA e[16];
  JOYINFOEX j;
  u32 n = 16, k;
  *dx = *dy = 0; *fuego = 0;
  if (tec) {
    if (DV_GetDeviceData(tec, sizeof e[0], e, &n, 0) >= 0)
      for (k = 0; k < n; k++) if (e[k].ofs == DIK_SPACE && (e[k].dato & 0x80)) espacio_pulsado = 1;
    DV_GetDeviceState(tec, 256, teclas);
    if (teclas[DIK_LEFT] & 0x80) *dx = -1;
    if (teclas[DIK_RIGHT] & 0x80) *dx = 1;
    if (teclas[DIK_UP] & 0x80) *dy = -1;
    if (teclas[DIK_DOWN] & 0x80) *dy = 1;
    if ((teclas[DIK_SPACE] | teclas[DIK_LCONTROL]) & 0x80) *fuego = 1;
    if (teclas[DIK_ESCAPE] & 0x80) salir = 1;
  }
  memset(&j, 0, sizeof j); j.tam = sizeof j; j.flags = 0xFF;
  if (joyGetPosEx(0, &j) == 0) {
    if (j.x < 16384) *dx = -1; else if (j.x > 49152) *dx = 1;
    if (j.y < 16384) *dy = -1; else if (j.y > 49152) *dy = 1;
    if (j.bot & 1) *fuego = 1, espacio_pulsado |= 0;
    if (j.bot & 2) espacio_pulsado = 1;
  }
}

/* ---------- el juego ---------- */
#define NMAL 24
#define NBALA 8
#define NBUM 8
static struct { int x, y, vivo; } mal[NMAL];
static struct { int x, y, vivo; } bala[NBALA], balam[NBALA];
static struct { int x, y, f; } bum[NBUM];
static struct { int x, y, v; } estrella[40];
static int nx, ny, vidas, puntos, oleada, estado, recarga, mueve, sentido = 1, invencible, cuenta;
enum { PORTADA, JUGANDO, FIN };
static void oleada_nueva(void) {
  int k;
  for (k = 0; k < NMAL; k++) { mal[k].x = 40 + (k % 8) * 30; mal[k].y = 24 + (k / 8) * 20; mal[k].vivo = 1; }
  oleada++;
}
static void partida(void) {
  int k;
  nx = AN / 2 - 8; ny = AL - 24; vidas = 3; puntos = 0; oleada = 0; recarga = 0; invencible = 50;
  for (k = 0; k < NBALA; k++) bala[k].vivo = balam[k].vivo = 0;
  for (k = 0; k < NBUM; k++) bum[k].f = -1;
  oleada_nueva();
  estado = JUGANDO;
}
static void explota(int x, int y) {
  int k;
  for (k = 0; k < NBUM; k++) if (bum[k].f < 0) { bum[k].x = x; bum[k].y = y; bum[k].f = 0; break; }
  suena(b_bum, x);
}
static void paso_juego(void) {
  int dx, dy, fuego, k, i, vivos = 0, borde = 0;
  entrada(&dx, &dy, &fuego);
  for (k = 0; k < 40; k++) { estrella[k].y += estrella[k].v; if (estrella[k].y >= AL) { estrella[k].y = 0; estrella[k].x = azar(AN); } }
  for (k = 0; k < NBUM; k++) if (bum[k].f >= 0 && ++bum[k].f >= 16) bum[k].f = -1;
  cuenta++;
  if (estado != JUGANDO) {
    if (espacio_pulsado) { espacio_pulsado = 0; partida(); }
    return;
  }
  espacio_pulsado = 0;
  nx += dx * 3; ny += dy * 2;
  if (nx < 0) nx = 0;
  if (nx > AN - 16) nx = AN - 16;
  if (ny < AL / 2) ny = AL / 2;
  if (ny > AL - 16) ny = AL - 16;
  if (recarga) recarga--;
  if (fuego && !recarga) {
    for (k = 0; k < NBALA; k++) if (!bala[k].vivo) { bala[k].x = nx + 6; bala[k].y = ny - 4; bala[k].vivo = 1; recarga = 8; suena(b_tiro, nx); break; }
  }
  for (k = 0; k < NBALA; k++) if (bala[k].vivo && (bala[k].y -= 5) < -5) bala[k].vivo = 0;
  for (k = 0; k < NBALA; k++) if (balam[k].vivo && (balam[k].y += 3) > AL) balam[k].vivo = 0;
  /* la oleada: de lado a lado, y baja al tocar el borde */
  if (!prueba && ++mueve >= 2) {
    mueve = 0;
    for (k = 0; k < NMAL; k++) if (mal[k].vivo && ((sentido > 0 && mal[k].x >= AN - 18) || (sentido < 0 && mal[k].x <= 2))) borde = 1;
    for (k = 0; k < NMAL; k++) if (mal[k].vivo) { if (borde) mal[k].y += 6; else mal[k].x += sentido; }
    if (borde) sentido = -sentido;
  }
  for (k = 0; k < NMAL; k++) {
    if (!mal[k].vivo) continue;
    vivos++;
    for (i = 0; i < NBALA; i++)
      if (bala[i].vivo && bala[i].x + 2 >= mal[k].x && bala[i].x <= mal[k].x + 15 && bala[i].y <= mal[k].y + 9 && bala[i].y + 4 >= mal[k].y) {
        bala[i].vivo = 0; mal[k].vivo = 0; puntos += 10 * oleada; explota(mal[k].x, mal[k].y); break;
      }
    if (mal[k].vivo && !prueba && azar(400) == 0)
      for (i = 0; i < NBALA; i++) if (!balam[i].vivo) { balam[i].x = mal[k].x + 6; balam[i].y = mal[k].y + 10; balam[i].vivo = 1; break; }
    if (mal[k].vivo && mal[k].y >= AL - 30) vidas = 0;
  }
  if (invencible) invencible--;
  for (i = 0; i < NBALA; i++)
    if (balam[i].vivo && !invencible && balam[i].x + 2 >= nx + 2 && balam[i].x <= nx + 13 && balam[i].y + 4 >= ny + 2 && balam[i].y <= ny + 12) {
      balam[i].vivo = 0; explota(nx, ny); vidas--; invencible = 60;
    }
  if (!vivos) oleada_nueva();
  if (vidas <= 0) { estado = FIN; cuenta = 0; }
}

/* ---------- pintar ---------- */
static void portada(void) {
  HANDLE dc;
  if (S_GetDC(atras, &dc)) return;
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(255, 230, 40));
  TextOutA(dc, 116, 70, "N A V E S", 9);
  SetTextColor(dc, RGB(240, 228, 200));
  TextOutA(dc, 72, 110, "Un juego de Hispa32", 19);
  SetTextColor(dc, RGB(80, 220, 255));
  TextOutA(dc, 52, 150, "Espacio: empezar   Esc: salir", 29);
  S_ReleaseDC(atras, dc);
}
static void pinta(void) {
  DDBLTFX fx;
  DDSURFACEDESC d;
  RECT r;
  int k;
  char t[32];
  memset(&fx, 0, sizeof fx); fx.tam = sizeof fx; fx.relleno = C_NEGRO;
  if (S_Blt(atras, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx) == DDERR_SURFACELOST) { S_Restore(prim); return; }
  memset(&d, 0, sizeof d); d.tam = sizeof d;
  if (S_Lock(atras, NULL, &d, DDLOCK_WAIT) == 0) {
    for (k = 0; k < 40; k++) ((u8 *)d.mem)[estrella[k].y * d.paso + estrella[k].x] = estrella[k].v > 1 ? C_BLANCO : C_GRIS;
    if (estado != PORTADA) {
      wsprintfA(t, "PUNTOS %d", puntos); texto(&d, 4, 4, t, C_CREMA);
      wsprintfA(t, "VIDAS %d", vidas > 0 ? vidas : 0); texto(&d, AN - 40, 4, t, C_CREMA);
      if (estado == FIN) texto(&d, AN / 2 - 34, AL / 2, "FIN DE PARTIDA", C_ROJO);
    }
    S_Unlock(atras, NULL);
  }
  if (estado == JUGANDO || estado == FIN) {
    for (k = 0; k < NMAL; k++) if (mal[k].vivo) S_BltFast(atras, mal[k].x, mal[k].y, s_mal, NULL, DDBLTFAST_SRCCOLORKEY);
    for (k = 0; k < NBALA; k++) if (bala[k].vivo) S_BltFast(atras, bala[k].x, bala[k].y, s_bala, NULL, DDBLTFAST_SRCCOLORKEY);
    for (k = 0; k < NBALA; k++) if (balam[k].vivo) {
      r.left = balam[k].x; r.top = balam[k].y; r.right = r.left + 2; r.bottom = r.top + 5;
      fx.relleno = C_MORADO; S_Blt(atras, &r, NULL, NULL, DDBLT_COLORFILL, &fx);
    }
    if (estado == JUGANDO && !(invencible & 4)) S_BltFast(atras, nx, ny, s_nave, NULL, DDBLTFAST_SRCCOLORKEY);
  }
  for (k = 0; k < NBUM; k++) if (bum[k].f >= 0) {
    RECT q = { (bum[k].f / 4) * 16, 0, (bum[k].f / 4) * 16 + 16, 16 };
    S_BltFast(atras, bum[k].x, bum[k].y, s_bum, &q, DDBLTFAST_SRCCOLORKEY);
  }
  if (estado == PORTADA) {
    S_BltFast(atras, AN / 2 - 8, 190, s_nave, NULL, DDBLTFAST_SRCCOLORKEY);
    portada();
  }
  if (S_Flip(prim, NULL, DDFLIP_WAIT) == DDERR_SURFACELOST) S_Restore(prim);
}

static int WINAPI proc(HANDLE w, u32 m, u32 wp, u32 lp) {
  if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
  if (m == 0x0020) { SetCursor(NULL); return 1; }   /* WM_SETCURSOR */
  return DefWindowProcA(w, m, wp, lp);
}
static int iniciar(void) {
  DDSURFACEDESC d;
  u32 caps[4] = { DDSCAPS_BACKBUFFER, 0, 0, 0 };
  u8 e[256][4];
  int k;
  if (DirectDrawCreate(NULL, &dd, NULL)) return 0;
  if (DD_SetCooperativeLevel(dd, ven, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE)) return 0;
  if (DD_SetDisplayMode(dd, AN, AL, 8)) return 0;
  memset(&d, 0, sizeof d);
  d.tam = sizeof d; d.flags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT; d.traseros = 1;
  d.caps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
  if (DD_CreateSurface(dd, &d, &prim) || S_GetAttachedSurface(prim, caps, &atras)) return 0;
  memset(e, 0, sizeof e);
  for (k = 0; k < (int)(sizeof pal_rgb / 3); k++) { e[k][0] = pal_rgb[k][0]; e[k][1] = pal_rgb[k][1]; e[k][2] = pal_rgb[k][2]; }
  DD_CreatePalette(dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, e, &pal);
  S_SetPalette(prim, pal);
  s_nave = superficie(16, 16); s_mal = superficie(16, 16); s_bala = superficie(3, 5); s_bum = superficie(64, 16);
  if (!s_nave || !s_mal || !s_bala || !s_bum) return 0;
  dibuja(s_nave, dib_nave, 16, 16); dibuja(s_mal, dib_mal, 16, 16); dibuja(s_bala, dib_bala, 3, 5);
  dibuja_bum();
  for (k = 0; k < 40; k++) { estrella[k].x = azar(AN); estrella[k].y = azar(AL); estrella[k].v = 1 + azar(3); }
  return 1;
}
void inicio(void) {
  WNDCLASSA c;
  MSG msg;
  u32 sig;
  prueba = strstr(GetCommandLineA(), "/PRUEBA") || strstr(GetCommandLineA(), "/prueba");
  memset(&c, 0, sizeof c);
  c.lpfnWndProc = proc;
  c.hInstance = GetModuleHandleA(NULL);
  c.hbrBackground = GetStockObject(BLACK_BRUSH);
  c.lpszClassName = "NavesHispa32";
  RegisterClassA(&c);
  ven = CreateWindowExA(0, "NavesHispa32", "Naves", WS_POPUP | WS_VISIBLE, 0, 0,
                        GetSystemMetrics(0), GetSystemMetrics(1), NULL, NULL, c.hInstance, NULL);
  if (!iniciar()) {
    if (dd) DD_RestoreDisplayMode(dd);
    MessageBoxA(ven, "Naves necesita DirectDraw en pantalla completa (Hispa Ventanas con VESA).", "Naves", 0);
    ExitProcess(1);
  }
  entrada_ini();
  sonidos();
  timeBeginPeriod(1);
  estado = PORTADA;
  sig = timeGetTime();
  while (!salir) {
    while (PeekMessageA(&msg, NULL, 0, 0, 1)) {
      if (msg.message == 0x12) salir = 1;   /* WM_QUIT */
      TranslateMessage(&msg); DispatchMessageA(&msg);
    }
    if ((int)(timeGetTime() - sig) < 0) { Sleep(5); continue; }
    sig += 20;                      /* 50 por segundo */
    if ((int)(timeGetTime() - sig) > 200) sig = timeGetTime();
    paso_juego();
    pinta();
  }
  timeEndPeriod(1);
  if (b_musica) B_Stop(b_musica);
  if (ds) Release(ds);
  if (tec) { DV_Unacquire(tec); Release(tec); }
  if (di) Release(di);
  DD_RestoreDisplayMode(dd);
  Release(dd);
  DestroyWindow(ven);
  ExitProcess(0);
}
