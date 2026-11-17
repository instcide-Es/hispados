/* K32CON.EXE - prueba de la consola de KERNEL32 de HW32 (Hispa-486, fase 2):
 * colores, cursor, rellenar y escribir en la pantalla, y lo que dice de ella.
 * La prueba (tests/run_hw32_tests.sh) mira la memoria de la pantalla.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef void *HANDLE;
#define WINAPI __attribute__((stdcall))
typedef struct { u16 x, y; } COORD;
typedef struct { u16 sx, sy, cx, cy, attr, l, t, r, b, mx, my; } CSBI;
HANDLE WINAPI GetStdHandle(int);
int WINAPI WriteConsoleA(HANDLE, const void *, u32, u32 *, void *);
int WINAPI SetConsoleTextAttribute(HANDLE, u16);
int WINAPI SetConsoleCursorPosition(HANDLE, COORD);
int WINAPI GetConsoleScreenBufferInfo(HANDLE, CSBI *);
int WINAPI FillConsoleOutputCharacterA(HANDLE, char, u32, COORD, u32 *);
int WINAPI FillConsoleOutputAttribute(HANDLE, u16, u32, COORD, u32 *);
int WINAPI WriteConsoleOutputCharacterA(HANDLE, const char *, u32, COORD, u32 *);
u32 WINAPI GetFileType(HANDLE);
void WINAPI ExitProcess(u32);
void *memset(void *d, int c, u32 n) { char *p = d; while (n--) *p++ = c; return d; }
static HANDLE sal;
static void pon(const char *s) { u32 n = 0, e; while (s[n]) n++; WriteConsoleA(sal, s, n, &e, 0); }
static void num(u32 v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); pon(b + i); }
void inicio(void) {
  sal = GetStdHandle(-11);
  u32 e;
  COORD c = { 10, 12 };
  SetConsoleCursorPosition(sal, c);
  SetConsoleTextAttribute(sal, 0x1E);
  pon("AMARILLO");
  SetConsoleTextAttribute(sal, 7);
  CSBI i;
  GetConsoleScreenBufferInfo(sal, &i);
  COORD r = { 70, 3 };
  FillConsoleOutputCharacterA(sal, '#', 5, r, &e);
  FillConsoleOutputAttribute(sal, 0x4F, 5, r, &e);
  COORD w = { 40, 20 };
  WriteConsoleOutputCharacterA(sal, "HISPA-486", 9, w, &e);
  COORD f = { 0, 14 };
  SetConsoleCursorPosition(sal, f);
  pon("CON: "); num(i.sx); pon("x"); num(i.sy); pon(" cursor "); num(i.cx); pon(","); num(i.cy);
  pon(" tipo "); num(GetFileType(sal)); pon(" escritos "); num(e); pon("\r\n");
  ExitProcess(0);
}
