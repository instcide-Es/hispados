typedef unsigned int u32;
#define WINAPI __attribute__((stdcall))
void *WINAPI GetStdHandle(int);
int WINAPI WriteFile(void*, const void*, u32, u32*, void*);
void WINAPI ExitProcess(u32);
char *WINAPI GetCommandLineA(void);
void *WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(void*, u32, u32);
static void *sal;
static void pon(const char *s) { u32 n = 0, e; while (s[n]) n++; WriteFile(sal, s, n, &e, 0); }
static void num(u32 v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); pon(b + i); }
static int primos[100];
void inicio(void) {
  sal = GetStdHandle(-11);
  pon("Hola desde C (gcc) en Hispa-486\r\n");
  pon("Linea: "); pon(GetCommandLineA()); pon("\r\n");
  int n = 0;
  for (int i = 2; n < 100; i++) { int p = 1; for (int j = 0; j < n && primos[j] * primos[j] <= i; j++) if (i % primos[j] == 0) { p = 0; break; } if (p) primos[n++] = i; }
  pon("Primo 100: "); num(primos[99]); pon("\r\n");
  u32 *m = HeapAlloc(GetProcessHeap(), 0, 400000);
  u32 s = 0; for (u32 i = 0; i < 100000; i++) m[i] = i; for (u32 i = 0; i < 100000; i++) s += m[i];
  pon("Suma: "); num(s); pon("\r\n");
  ExitProcess(7);
}
