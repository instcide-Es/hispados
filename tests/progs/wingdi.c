/* wingdi.c - prueba de la fase 3d de Hispa-486 (docs/486/HISPA-486.md): el
 * GDI de los programas de Windows 95/98 dentro de Hispa Ventanas. Una ventana
 * con una cuadrícula de 6 × 4 casillas; cada una pinta algo y la prueba
 * (tests/run_win32gdi_tests.sh) mira sus puntos:
 *  fila 0: Rectangle (pluma ancha), Ellipse, RoundRect, Pie, Chord y Arc
 *          (pluma de rayas);
 *  fila 1: Polygon (ALTERNATE y WINDING), los seis rayados, Polyline y
 *          PolyBezier, las ROP2 (XOR dos veces y NOT), DrawEdge y FrameRect;
 *  fila 2: letras de 8, 16 (negrita) y 32 (cursiva, subrayada), y DrawTextA
 *          partiendo las palabras;
 *  fila 3: un mapa en memoria (BitBlt y StretchBlt), un dibujo con máscara
 *          (SRCAND y SRCPAINT), StretchDIBits de un DIB de 24 bits,
 *          CreateDIBSection de 8 bits y el recorte con una región elíptica.
 * Abajo, cuántas comprobaciones de valores han salido bien (GetTextMetricsA,
 * GetTextExtentPoint32A, GetDeviceCaps, CombineRgn, GetObjectA, GetPixel,
 * GetNearestColor, SaveDC, GetDIBits y las paletas).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --gui (ver el Makefile). */
#include "win32min.h"
#include "win32gdi.h"
void *memset(void *, int, u32);

#define CX(c) (8 + (c) * 124)
#define CY(r) (8 + (r) * 112)

static int bien, hechas;
static char fallos[64];

static void comprueba(int n, int ok)
{
  hechas++;
  if (ok) { bien++; return; }
  if (lstrlenA(fallos) < 50) { char t[8]; wsprintfA(t, " %d", n); lstrcatA(fallos, t); }
}

static HANDLE pincel(u32 c) { return CreateSolidBrush(c); }

static void caja(HANDLE dc, int x, int y, int w, int h, u32 c)
{
  RECT r = { x, y, x + w, y + h };
  HANDLE b = pincel(c);
  FillRect(dc, &r, b);
  DeleteObject(b);
}

static void fila0(HANDLE dc)
{
  int X, Y = CY(0);
  HANDLE p, b, vp, vb;
  X = CX(0);
  p = CreatePen(PS_SOLID, 4, RGB(0, 0, 255)); b = pincel(RGB(255, 0, 0));
  vp = SelectObject(dc, p); vb = SelectObject(dc, b);
  Rectangle(dc, X, Y, X + 100, Y + 80);
  SelectObject(dc, vp); SelectObject(dc, vb); DeleteObject(p); DeleteObject(b);
  X = CX(1);
  b = pincel(RGB(0, 255, 0)); vb = SelectObject(dc, b);
  Ellipse(dc, X, Y, X + 100, Y + 80);
  SelectObject(dc, vb); DeleteObject(b);
  X = CX(2);
  b = pincel(RGB(255, 255, 0)); vb = SelectObject(dc, b);
  RoundRect(dc, X, Y, X + 100, Y + 80, 40, 40);
  SelectObject(dc, vb); DeleteObject(b);
  X = CX(3);
  b = pincel(RGB(255, 0, 255)); vb = SelectObject(dc, b);
  Pie(dc, X, Y, X + 100, Y + 80, X + 100, Y + 40, X + 50, Y);
  SelectObject(dc, vb); DeleteObject(b);
  X = CX(4);
  b = pincel(RGB(0, 255, 255)); vb = SelectObject(dc, b);
  Chord(dc, X, Y, X + 100, Y + 80, X + 100, Y + 40, X, Y + 40);
  SelectObject(dc, vb); DeleteObject(b);
  X = CX(5);
  p = CreatePen(PS_DASH, 1, RGB(255, 0, 0)); vp = SelectObject(dc, p);
  SetBkMode(dc, TRANSPARENT);
  Arc(dc, X, Y, X + 100, Y + 80, X + 100, Y + 40, X, Y + 40);
  SetBkMode(dc, OPAQUE);
  SelectObject(dc, vp); DeleteObject(p);
}

static void estrella(HANDLE dc, int X, int Y, int modo)
{
  POINT e[5] = { { X + 50, Y }, { X + 80, Y + 80 }, { X + 4, Y + 30 }, { X + 96, Y + 30 }, { X + 20, Y + 80 } };
  HANDLE b = pincel(RGB(0, 0, 255)), vb = SelectObject(dc, b);
  SetPolyFillMode(dc, modo);
  Polygon(dc, e, 5);
  SelectObject(dc, vb); DeleteObject(b);
}

static void fila1(HANDLE dc)
{
  int X, Y = CY(1), i;
  HANDLE p, b, vp, vb;
  estrella(dc, CX(0), Y, ALTERNATE);
  estrella(dc, CX(1), Y, WINDING);
  X = CX(2);                                   /* los seis rayados */
  SetBkColor(dc, RGB(255, 255, 255));
  vp = SelectObject(dc, GetStockObject(NULL_PEN));
  for (i = 0; i < 6; i++) {
    b = CreateHatchBrush(i, RGB(0, 0, 255)); vb = SelectObject(dc, b);
    Rectangle(dc, X + (i % 3) * 34, Y + (i / 3) * 40, X + (i % 3) * 34 + 33, Y + (i / 3) * 40 + 33);
    SelectObject(dc, vb); DeleteObject(b);
  }
  SelectObject(dc, vp);
  X = CX(3);                                   /* Polyline y PolyBezier */
  {
    POINT z[5] = { { X, Y + 70 }, { X + 25, Y + 40 }, { X + 50, Y + 70 }, { X + 75, Y + 40 }, { X + 100, Y + 70 } };
    POINT c[4] = { { X, Y + 30 }, { X + 30, Y - 20 }, { X + 70, Y + 70 }, { X + 100, Y + 10 } };
    p = CreatePen(PS_SOLID, 2, RGB(0, 0, 0)); vp = SelectObject(dc, p);
    Polyline(dc, z, 5);
    SelectObject(dc, vp); DeleteObject(p);
    p = CreatePen(PS_SOLID, 1, RGB(255, 0, 0)); vp = SelectObject(dc, p);
    PolyBezier(dc, c, 4);
    SelectObject(dc, vp); DeleteObject(p);
  }
  X = CX(4);                                   /* ROP2 */
  caja(dc, X, Y, 100, 80, RGB(102, 102, 102));
  p = CreatePen(PS_SOLID, 1, RGB(255, 255, 255)); vp = SelectObject(dc, p);
  SetROP2(dc, R2_XORPEN);
  MoveToEx(dc, X, Y + 20, NULL); LineTo(dc, X + 100, Y + 20);
  MoveToEx(dc, X, Y + 20, NULL); LineTo(dc, X + 100, Y + 20);    /* (dos veces: nada) */
  MoveToEx(dc, X, Y + 30, NULL); LineTo(dc, X + 100, Y + 30);    /* (una: blanco xor gris) */
  SetROP2(dc, R2_COPYPEN);
  SelectObject(dc, vp); DeleteObject(p);
  { RECT r = { X + 50, Y + 40, X + 100, Y + 80 }; InvertRect(dc, &r); }
  X = CX(5);                                   /* DrawEdge, FrameRect */
  caja(dc, X, Y, 100, 80, RGB(192, 192, 192));
  { RECT r = { X + 5, Y + 5, X + 45, Y + 35 }; DrawEdge(dc, &r, EDGE_RAISED, BF_RECT); }
  { RECT r = { X + 55, Y + 5, X + 95, Y + 35 }; DrawEdge(dc, &r, EDGE_SUNKEN, BF_RECT); }
  { RECT r = { X + 5, Y + 45, X + 95, Y + 75 }; b = pincel(RGB(255, 0, 0)); FrameRect(dc, &r, b); DeleteObject(b); }
}

static void fila2(HANDLE dc)
{
  int Y = CY(2);
  HANDLE f, vf;
  RECT r;
  SetTextColor(dc, RGB(0, 0, 0));
  SetBkMode(dc, TRANSPARENT);
  f = CreateFontA(8, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial"); vf = SelectObject(dc, f);
  TextOutA(dc, CX(0), Y, "Letra de 8 puntos", 17);
  SelectObject(dc, vf); DeleteObject(f);
  f = CreateFontA(-13, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Arial"); vf = SelectObject(dc, f);
  TextOutA(dc, CX(0), Y + 16, "Negrita", 7);
  SelectObject(dc, vf); DeleteObject(f);
  f = CreateFontA(32, 0, 0, 0, 400, 1, 1, 0, 0, 0, 0, 0, 0, "Times"); vf = SelectObject(dc, f);
  SetTextColor(dc, RGB(0, 0, 255));
  TextOutA(dc, CX(0), Y + 40, "Grande", 6);
  SelectObject(dc, vf); DeleteObject(f);
  SetTextColor(dc, RGB(0, 0, 0));
  r.left = CX(3); r.top = Y; r.right = CX(3) + 200; r.bottom = Y + 100;
  DrawTextA(dc, "Una frase larga que DrawTextA parte en varias l\xedneas.", -1, &r, DT_WORDBREAK | DT_CENTER);
  SetBkMode(dc, OPAQUE);
}

static u32 rgb24[16] = {
  RGB(255, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 255, 0),
  RGB(255, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 255, 0),
  RGB(0, 0, 255), RGB(0, 0, 255), RGB(255, 255, 0), RGB(255, 255, 0),
  RGB(0, 0, 255), RGB(0, 0, 255), RGB(255, 255, 0), RGB(255, 255, 0) };

static void fila3(HANDLE dc)
{
  int X, Y = CY(3), x, y;
  HANDLE m, bm, vbm, b, vb, mm, mbm, vmbm;
  /* un mapa de 64 × 64 en memoria: azul con una elipse amarilla */
  m = CreateCompatibleDC(dc);
  bm = CreateCompatibleBitmap(dc, 64, 64);
  vbm = SelectObject(m, bm);
  caja(m, 0, 0, 64, 64, RGB(0, 0, 255));
  b = pincel(RGB(255, 255, 0)); vb = SelectObject(m, b);
  SelectObject(m, GetStockObject(NULL_PEN));
  Ellipse(m, 8, 8, 56, 56);
  SelectObject(m, vb); DeleteObject(b);
  BitBlt(dc, CX(0), Y, 64, 64, m, 0, 0, SRCCOPY);
  StretchBlt(dc, CX(1), Y, 100, 100, m, 0, 0, 64, 64, SRCCOPY);
  {
    BITMAP o; BITMAPINFO256 bi; static unsigned char px[64 * 64 * 3];
    memset(&bi, 0, sizeof bi);
    comprueba(6, GetObjectA(bm, sizeof o, &o) == 24 && o.bmWidth == 64 && o.bmHeight == 64);
    SelectObject(m, vbm);
    bi.h.biSize = 40; bi.h.biWidth = 64; bi.h.biHeight = 64; bi.h.biPlanes = 1; bi.h.biBitCount = 24;
    bi.h.biCompression = BI_RGB;
    comprueba(10, GetDIBits(dc, bm, 0, 64, px, &bi, DIB_RGB_COLORS) == 64 &&
              px[(32 * 64 + 32) * 3] == 0 && px[(32 * 64 + 32) * 3 + 1] == 255 && px[(32 * 64 + 32) * 3 + 2] == 255 &&
              px[(2 * 64 + 2) * 3] == 255 && px[(2 * 64 + 2) * 3 + 2] == 0);
    SelectObject(m, bm);
  }
  /* con máscara: un círculo rojo sobre verde (SRCAND y SRCPAINT) */
  X = CX(2);
  caja(dc, X, Y, 64, 64, RGB(0, 255, 0));
  mm = CreateCompatibleDC(dc);
  mbm = CreateBitmap(64, 64, 1, 1, NULL);
  vmbm = SelectObject(mm, mbm);
  PatBlt(mm, 0, 0, 64, 64, WHITENESS);
  SelectObject(mm, GetStockObject(BLACK_BRUSH));
  Ellipse(mm, 8, 8, 56, 56);
  caja(m, 0, 0, 64, 64, RGB(0, 0, 0));
  b = pincel(RGB(255, 0, 0)); vb = SelectObject(m, b);
  SelectObject(m, GetStockObject(NULL_PEN));
  Ellipse(m, 8, 8, 56, 56);
  SelectObject(m, vb); DeleteObject(b);
  SetTextColor(dc, RGB(0, 0, 0)); SetBkColor(dc, RGB(255, 255, 255));
  BitBlt(dc, X, Y, 64, 64, mm, 0, 0, SRCAND);
  BitBlt(dc, X, Y, 64, 64, m, 0, 0, SRCPAINT);
  SelectObject(mm, vmbm); DeleteObject(mbm); DeleteDC(mm);
  SelectObject(m, vbm); DeleteObject(bm); DeleteDC(m);
  /* un DIB de 24 bits de 4 × 4 (cuatro colores), estirado a 96 × 96 */
  {
    static unsigned char dib[4 * 12];
    BITMAPINFO256 bi;
    memset(&bi, 0, sizeof bi);
    for (y = 0; y < 4; y++)
      for (x = 0; x < 4; x++) {
        u32 c = rgb24[y * 4 + x];
        unsigned char *p = dib + (3 - y) * 12 + x * 3;     /* de abajo arriba */
        p[0] = c >> 16; p[1] = c >> 8; p[2] = c;
      }
    bi.h.biSize = 40; bi.h.biWidth = 4; bi.h.biHeight = 4; bi.h.biPlanes = 1; bi.h.biBitCount = 24;
    bi.h.biCompression = BI_RGB; bi.h.biClrUsed = 0;
    StretchDIBits(dc, CX(3), Y, 96, 96, 0, 0, 4, 4, dib, &bi, DIB_RGB_COLORS, SRCCOPY);
  }
  /* CreateDIBSection de 8 bits: el programa escribe sus puntos */
  {
    BITMAPINFO256 bi; unsigned char *bits; HANDLE ds, vds, sm;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = 40; bi.h.biWidth = 64; bi.h.biHeight = -64; bi.h.biPlanes = 1; bi.h.biBitCount = 8;
    bi.h.biCompression = BI_RGB; bi.h.biClrUsed = 64; bi.h.biClrImportant = 0;
    for (x = 0; x < 64; x++) { bi.c[x].r = x * 4; bi.c[x].g = 0; bi.c[x].b = 255 - x * 4; bi.c[x].x = 0; }
    ds = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    for (y = 0; y < 64; y++) for (x = 0; x < 64; x++) bits[y * 64 + x] = x;
    sm = CreateCompatibleDC(dc);
    vds = SelectObject(sm, ds);
    b = pincel(RGB(0, 0, 255));
    { RECT r = { 24, 24, 40, 40 }; FillRect(sm, &r, b); }   /* (pintado con GDI: vuelve a sus puntos) */
    DeleteObject(b);
    comprueba(11, bits[32 * 64 + 32] == 0 && bits[0] == 0 && bits[63] == 63 && bits[10 * 64 + 50] == 50);
    BitBlt(dc, CX(4), Y, 64, 64, sm, 0, 0, SRCCOPY);
    SelectObject(sm, vds); DeleteDC(sm); DeleteObject(ds);
  }
  /* el recorte: una región elíptica, con un agujero */
  {
    HANDLE r = CreateEllipticRgn(CX(5), Y, CX(5) + 100, Y + 80);
    SelectClipRgn(dc, r);
    ExcludeClipRect(dc, CX(5) + 40, Y + 30, CX(5) + 60, Y + 50);
    caja(dc, CX(5), Y, 100, 80, RGB(255, 0, 0));
    SelectClipRgn(dc, NULL);
    DeleteObject(r);
  }
}

static void valores(HANDLE dc)
{
  TEXTMETRICA tm; SIZE s; RECT rb; HANDLE f, vf, a, b, c, pal, vpal, br;
  int t; u32 col;
  f = CreateFontA(32, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Times"); vf = SelectObject(dc, f);
  comprueba(1, GetTextMetricsA(dc, &tm) && tm.tmHeight == 32 && tm.tmAveCharWidth == 16);
  SelectObject(dc, vf); DeleteObject(f);
  comprueba(2, GetTextExtentPoint32A(dc, "Hispa", 5, &s) && s.cx == 40 && s.cy == 16);
  comprueba(3, GetDeviceCaps(dc, BITSPIXEL) == 8 && GetDeviceCaps(dc, LOGPIXELSY) == 96);
  a = CreateRectRgn(0, 0, 10, 10); b = CreateRectRgn(5, 5, 15, 15); c = CreateRectRgn(0, 0, 0, 0);
  t = CombineRgn(c, a, b, RGN_OR);
  GetRgnBox(c, &rb);
  comprueba(4, t == COMPLEXREGION && rb.left == 0 && rb.top == 0 && rb.right == 15 && rb.bottom == 15 &&
               !PtInRegion(c, 12, 2) && PtInRegion(c, 12, 12) && PtInRegion(c, 2, 2));
  t = CombineRgn(c, a, b, RGN_DIFF);
  comprueba(5, t == COMPLEXREGION && !PtInRegion(c, 7, 7) && PtInRegion(c, 2, 2) && PtInRegion(c, 7, 2));
  DeleteObject(a); DeleteObject(b); DeleteObject(c);
  SetPixel(dc, 750, 4, RGB(255, 0, 0));
  comprueba(7, GetPixel(dc, 750, 4) == RGB(255, 0, 0));
  comprueba(8, GetNearestColor(dc, RGB(250, 10, 10)) == RGB(255, 0, 0));
  SetTextColor(dc, RGB(1, 2, 3));
  t = SaveDC(dc);
  SetTextColor(dc, RGB(9, 9, 9));
  RestoreDC(dc, t);
  col = SetTextColor(dc, RGB(0, 0, 0));
  comprueba(9, t == 1 && col == RGB(1, 2, 3));
  {
    LOGPALETTE256 lp; lp.palVersion = 0x300; lp.palNumEntries = 2; lp.pe[0] = RGB(0, 255, 0); lp.pe[1] = RGB(255, 0, 255);
    pal = CreatePalette(&lp);
    vpal = SelectPalette(dc, pal, 0);
    RealizePalette(dc);
    br = pincel(PALETTEINDEX(1));
    { RECT r = { 744, 0, 748, 4 }; FillRect(dc, &r, br); }
    DeleteObject(br);
    comprueba(12, GetPixel(dc, 745, 1) == RGB(255, 0, 255));
    SelectPalette(dc, vpal, 0);
    DeleteObject(pal);
  }
}

static int WINAPI proc(HANDLE w, u32 m, u32 wp, u32 lp)
{
  if (m == WM_PAINT) {
    PAINTSTRUCT ps; char t[80];
    HANDLE dc = BeginPaint(w, &ps);
    bien = hechas = 0; fallos[0] = 0;
    fila0(dc); fila1(dc); fila2(dc); fila3(dc); valores(dc);
    wsprintfA(t, "Comprobaciones: %d de %d bien.%s%s", bien, hechas, fallos[0] ? " Mal:" : "", fallos);
    SetTextColor(dc, RGB(0, 0, 0));
    TextOutA(dc, 8, CY(4) + 4, t, lstrlenA(t));
    EndPaint(w, &ps);
    return 0;
  }
  if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcA(w, m, wp, lp);
}

void inicio(void)
{
  WNDCLASSA c;
  MSG msg;
  HANDLE w;
  memset(&c, 0, sizeof c);
  c.lpfnWndProc = proc;
  c.hInstance = GetModuleHandleA(NULL);
  c.hbrBackground = GetStockObject(WHITE_BRUSH);
  c.lpszClassName = "PruebaGDI";
  RegisterClassA(&c);
  w = CreateWindowExA(0, "PruebaGDI", "GDI de Hispa-486", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                      10, 30, 780, 540, NULL, NULL, c.hInstance, NULL);
  while (GetMessageA(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageA(&msg); }
  ExitProcess(0);
}
