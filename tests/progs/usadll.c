/* USADLL.EXE - usa DLLPRUE.DLL: una función y un dato importados; acaba con 5.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
typedef unsigned int u32;
#define WINAPI __attribute__((stdcall))
void *WINAPI GetStdHandle(int);
int WINAPI WriteFile(void*, const void*, u32, u32*, void*);
void WINAPI ExitProcess(u32);
int WINAPI Doble(int);
extern int *__imp_contador;
static void *sal;
static void pon(const char *s) { u32 n = 0, e; while (s[n]) n++; WriteFile(sal, s, n, &e, 0); }
static void num(u32 v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); pon(b + i); }
void inicio(void) {
  sal = GetStdHandle(-11);
  pon("Doble(20) = "); num(Doble(20)); pon("\r\ncontador = "); num(*__imp_contador); pon("\r\n");
  ExitProcess(5);
}
