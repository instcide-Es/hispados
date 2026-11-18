/* win32gdi.h - lo de GDI32 (y algo de USER32) de la fase 3d de Hispa-486
 * para los programas de prueba: va detrás de win32min.h. Los nombres y los
 * números son los documentados del API Win32.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef WIN32GDI_H
#define WIN32GDI_H
typedef struct { int cx, cy; } SIZE;
typedef struct { int lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
  unsigned char lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision,
  lfQuality, lfPitchAndFamily; char lfFaceName[32]; } LOGFONTA;
typedef struct { int tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading,
  tmAveCharWidth, tmMaxCharWidth, tmWeight, tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
  unsigned char tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar, tmItalic, tmUnderlined,
  tmStruckOut, tmPitchAndFamily, tmCharSet; } TEXTMETRICA;
typedef struct { int bmType, bmWidth, bmHeight, bmWidthBytes; u16 bmPlanes, bmBitsPixel; void *bmBits; } BITMAP;
typedef struct { u32 biSize; int biWidth, biHeight; u16 biPlanes, biBitCount; u32 biCompression,
  biSizeImage; int biXPelsPerMeter, biYPelsPerMeter; u32 biClrUsed, biClrImportant; } BITMAPINFOHEADER;
typedef struct { unsigned char b, g, r, x; } RGBQUAD;
typedef struct { BITMAPINFOHEADER h; RGBQUAD c[256]; } BITMAPINFO256;
typedef struct { u16 palVersion, palNumEntries; u32 pe[256]; } LOGPALETTE256;
typedef struct { u32 lbStyle, lbColor, lbHatch; } LOGBRUSH;
#define RGB(r, g, b) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16))
#define PALETTEINDEX(i) (0x01000000u | (u16)(i))
#define PS_SOLID 0
#define PS_DASH 1
#define PS_DOT 2
#define PS_NULL 5
#define HS_HORIZONTAL 0
#define HS_VERTICAL 1
#define HS_FDIAGONAL 2
#define HS_BDIAGONAL 3
#define HS_CROSS 4
#define HS_DIAGCROSS 5
#define NULL_BRUSH 5
#define NULL_PEN 8
#define WHITE_BRUSH 0
#define BLACK_BRUSH 4
#define BLACK_PEN 7
#define TRANSPARENT 1
#define OPAQUE 2
#define ALTERNATE 1
#define WINDING 2
#define R2_NOT 6
#define R2_XORPEN 7
#define R2_COPYPEN 13
#define SRCCOPY 0x00CC0020
#define SRCPAINT 0x00EE0086
#define SRCAND 0x008800C6
#define SRCINVERT 0x00660046
#define NOTSRCCOPY 0x00330008
#define PATCOPY 0x00F00021
#define DSTINVERT 0x00550009
#define BLACKNESS 0x00000042
#define WHITENESS 0x00FF0062
#define DIB_RGB_COLORS 0
#define BI_RGB 0
#define RGN_AND 1
#define RGN_OR 2
#define RGN_XOR 3
#define RGN_DIFF 4
#define NULLREGION 1
#define SIMPLEREGION 2
#define COMPLEXREGION 3
#define DT_LEFT 0
#define DT_CENTER 1
#define DT_RIGHT 2
#define DT_VCENTER 4
#define DT_WORDBREAK 0x10
#define DT_SINGLELINE 0x20
#define DT_CALCRECT 0x400
#define DT_NOPREFIX 0x800
#define TA_CENTER 6
#define TA_BOTTOM 8
#define TA_BASELINE 24
#define FW_BOLD 700
#define BITSPIXEL 12
#define LOGPIXELSY 90
#define RASTERCAPS 38
#define EDGE_RAISED 5
#define EDGE_SUNKEN 10
#define BF_RECT 15

int WINAPI TextOutA(HANDLE, int, int, const char *, int);
int WINAPI ExtTextOutA(HANDLE, int, int, u32, const RECT *, const char *, u32, const int *);
u32 WINAPI SetTextAlign(HANDLE, u32);
u32 WINAPI SetTextColor(HANDLE, u32);
u32 WINAPI SetBkColor(HANDLE, u32);
int WINAPI SetBkMode(HANDLE, int);
HANDLE WINAPI GetStockObject(int);
HANDLE WINAPI CreateSolidBrush(u32);
HANDLE WINAPI CreateHatchBrush(int, u32);
HANDLE WINAPI CreatePatternBrush(HANDLE);
HANDLE WINAPI CreateBrushIndirect(const LOGBRUSH *);
HANDLE WINAPI CreatePen(int, int, u32);
HANDLE WINAPI SelectObject(HANDLE, HANDLE);
int WINAPI DeleteObject(HANDLE);
int WINAPI GetObjectA(HANDLE, int, void *);
int WINAPI Rectangle(HANDLE, int, int, int, int);
int WINAPI Ellipse(HANDLE, int, int, int, int);
int WINAPI RoundRect(HANDLE, int, int, int, int, int, int);
int WINAPI Arc(HANDLE, int, int, int, int, int, int, int, int);
int WINAPI Pie(HANDLE, int, int, int, int, int, int, int, int);
int WINAPI Chord(HANDLE, int, int, int, int, int, int, int, int);
int WINAPI Polygon(HANDLE, const POINT *, int);
int WINAPI Polyline(HANDLE, const POINT *, int);
int WINAPI PolyBezier(HANDLE, const POINT *, u32);
int WINAPI MoveToEx(HANDLE, int, int, POINT *);
int WINAPI LineTo(HANDLE, int, int);
u32 WINAPI SetPixel(HANDLE, int, int, u32);
u32 WINAPI GetPixel(HANDLE, int, int);
int WINAPI SetPolyFillMode(HANDLE, int);
int WINAPI SetROP2(HANDLE, int);
HANDLE WINAPI CreateFontA(int, int, int, int, int, u32, u32, u32, u32, u32, u32, u32, u32, const char *);
HANDLE WINAPI CreateFontIndirectA(const LOGFONTA *);
int WINAPI GetTextMetricsA(HANDLE, TEXTMETRICA *);
int WINAPI GetTextExtentPoint32A(HANDLE, const char *, int, SIZE *);
HANDLE WINAPI CreateCompatibleDC(HANDLE);
int WINAPI DeleteDC(HANDLE);
HANDLE WINAPI CreateCompatibleBitmap(HANDLE, int, int);
HANDLE WINAPI CreateBitmap(int, int, u32, u32, const void *);
int WINAPI BitBlt(HANDLE, int, int, int, int, HANDLE, int, int, u32);
int WINAPI StretchBlt(HANDLE, int, int, int, int, HANDLE, int, int, int, int, u32);
int WINAPI PatBlt(HANDLE, int, int, int, int, u32);
int WINAPI StretchDIBits(HANDLE, int, int, int, int, int, int, int, int, const void *, const void *, u32, u32);
int WINAPI SetDIBitsToDevice(HANDLE, int, int, u32, u32, int, int, u32, u32, const void *, const void *, u32);
HANDLE WINAPI CreateDIBSection(HANDLE, const void *, u32, void **, HANDLE, u32);
int WINAPI GetDIBits(HANDLE, HANDLE, u32, u32, void *, void *, u32);
HANDLE WINAPI CreateRectRgn(int, int, int, int);
HANDLE WINAPI CreateEllipticRgn(int, int, int, int);
int WINAPI CombineRgn(HANDLE, HANDLE, HANDLE, int);
int WINAPI SelectClipRgn(HANDLE, HANDLE);
int WINAPI ExcludeClipRect(HANDLE, int, int, int, int);
int WINAPI IntersectClipRect(HANDLE, int, int, int, int);
int WINAPI GetRgnBox(HANDLE, RECT *);
int WINAPI PtInRegion(HANDLE, int, int);
int WINAPI FillRgn(HANDLE, HANDLE, HANDLE);
int WINAPI SaveDC(HANDLE);
int WINAPI RestoreDC(HANDLE, int);
int WINAPI SetViewportOrgEx(HANDLE, int, int, POINT *);
int WINAPI GetDeviceCaps(HANDLE, int);
HANDLE WINAPI CreatePalette(const void *);
HANDLE WINAPI SelectPalette(HANDLE, HANDLE, int);
u32 WINAPI RealizePalette(HANDLE);
u32 WINAPI GetNearestColor(HANDLE, u32);
int WINAPI FrameRect(HANDLE, const RECT *, HANDLE);
int WINAPI InvertRect(HANDLE, const RECT *);
int WINAPI DrawEdge(HANDLE, RECT *, u32, u32);
int WINAPI DrawFocusRect(HANDLE, const RECT *);
#endif
