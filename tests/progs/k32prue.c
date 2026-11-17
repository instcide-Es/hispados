/* K32PRUE.EXE - prueba de KERNEL32 de HW32 (Hispa-486, fase 2): archivos,
 * búsquedas, directorios, hora, memoria, entorno, TLS, GetProcAddress y
 * textos. Cada línea dice «nombre: valor»; tests/run_hw32_tests.sh las
 * compara con lo que da Windows 98.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * gcc -m32 -ffreestanding ... y tools/elf2pe.py (ver el Makefile). */
typedef unsigned int u32;
typedef unsigned short u16;
typedef void *HANDLE;
#define WINAPI __attribute__((stdcall))
#define INVALID ((HANDLE)-1)
typedef struct { u32 lo, hi; } FT;
typedef struct { u16 anio, mes, sem, dia, hora, min, seg, ms; } ST;
typedef struct { u32 atrib; FT c, a, e; u32 tam_hi, tam_lo, r0, r1; char nombre[260]; char alt[14]; } FINDA;
typedef struct { u32 tam, may, men, build, plat; char csd[128]; } OSVER;
typedef struct { u32 len, carga, total, libre, tp, lp, tv, lv; } MEMST;

HANDLE WINAPI GetStdHandle(int);
int WINAPI WriteFile(HANDLE, const void *, u32, u32 *, void *);
int WINAPI ReadFile(HANDLE, void *, u32, u32 *, void *);
void WINAPI ExitProcess(u32);
HANDLE WINAPI CreateFileA(const char *, u32, u32, void *, u32, u32, HANDLE);
int WINAPI CloseHandle(HANDLE);
u32 WINAPI SetFilePointer(HANDLE, int, int *, u32);
u32 WINAPI GetFileSize(HANDLE, u32 *);
u32 WINAPI GetLastError(void);
u32 WINAPI GetFileAttributesA(const char *);
int WINAPI DeleteFileA(const char *);
int WINAPI CopyFileA(const char *, const char *, int);
int WINAPI MoveFileA(const char *, const char *);
int WINAPI CreateDirectoryA(const char *, void *);
int WINAPI RemoveDirectoryA(const char *);
int WINAPI SetCurrentDirectoryA(const char *);
u32 WINAPI GetCurrentDirectoryA(u32, char *);
u32 WINAPI GetFullPathNameA(const char *, u32, char *, char **);
HANDLE WINAPI FindFirstFileA(const char *, FINDA *);
int WINAPI FindNextFileA(HANDLE, FINDA *);
int WINAPI FindClose(HANDLE);
u32 WINAPI GetTempFileNameA(const char *, const char *, u32, char *);
u32 WINAPI GetDriveTypeA(const char *);
int WINAPI GetDiskFreeSpaceA(const char *, u32 *, u32 *, u32 *, u32 *);
int WINAPI SetEndOfFile(HANDLE);
int WINAPI SystemTimeToFileTime(const ST *, FT *);
int WINAPI FileTimeToSystemTime(const FT *, ST *);
int WINAPI FileTimeToDosDateTime(const FT *, u16 *, u16 *);
int WINAPI DosDateTimeToFileTime(u16, u16, FT *);
void WINAPI GetLocalTime(ST *);
int WINAPI CompareFileTime(const FT *, const FT *);
HANDLE WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(HANDLE, u32, u32);
int WINAPI HeapFree(HANDLE, u32, void *);
void *WINAPI HeapReAlloc(HANDLE, u32, void *, u32);
u32 WINAPI HeapSize(HANDLE, u32, void *);
void *WINAPI LocalAlloc(u32, u32);
void *WINAPI LocalFree(void *);
void *WINAPI VirtualAlloc(void *, u32, u32, u32);
int WINAPI VirtualFree(void *, u32, u32);
void WINAPI GlobalMemoryStatus(MEMST *);
u32 WINAPI GetEnvironmentVariableA(const char *, char *, u32);
u32 WINAPI TlsAlloc(void);
int WINAPI TlsSetValue(u32, void *);
void *WINAPI TlsGetValue(u32);
int WINAPI InterlockedIncrement(int *);
int WINAPI InterlockedExchange(int *, int);
int WINAPI InterlockedCompareExchange(int *, int, int);
HANDLE WINAPI GetModuleHandleA(const char *);
void *WINAPI GetProcAddress(HANDLE, const char *);
HANDLE WINAPI LoadLibraryA(const char *);
u32 WINAPI GetVersion(void);
int WINAPI GetVersionExA(OSVER *);
int WINAPI lstrcmpiA(const char *, const char *);
int WINAPI lstrcmpA(const char *, const char *);
char *WINAPI lstrcatA(char *, const char *);
char *WINAPI lstrcpyA(char *, const char *);
int WINAPI lstrlenA(const char *);
int WINAPI MultiByteToWideChar(u32, u32, const char *, int, u16 *, int);
int WINAPI WideCharToMultiByte(u32, u32, const u16 *, int, char *, int, const char *, int *);

void *memset(void *d, int c, u32 n) { char *p = d; while (n--) *p++ = c; return d; }
void *memcpy(void *d, const void *s, u32 n) { char *p = d; const char *q = s; while (n--) *p++ = *q++; return d; }

static HANDLE sal;
static void pon(const char *s) { u32 n = 0, e; while (s[n]) n++; WriteFile(sal, s, n, &e, 0); }
static void num(u32 v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); pon(b + i); }
static void sn(int v) { if (v < 0) { pon("-"); num(-v); } else num(v); }
static void hx(u32 v) { char b[9]; for (int i = 7; i >= 0; i--) { b[i] = "0123456789ABCDEF"[v & 15]; v >>= 4; } b[8] = 0; pon(b); }
static void lin(const char *n) { pon("\r\n"); pon(n); pon(": "); }
static void sino(int c) { pon(c ? "si" : "no"); }

#define GR 0x80000000
#define GW 0x40000000

void inicio(void) {
  sal = GetStdHandle(-11);
  u32 e;
  pon("K32PRUE");
  /* versión */
  lin("version"); hx(GetVersion());
  OSVER ov; ov.tam = sizeof ov; GetVersionExA(&ov);
  lin("versionex"); num(ov.may); pon("."); num(ov.men); pon(" plataforma "); num(ov.plat); pon(" build "); num(ov.build & 0xFFFF);
  /* archivos */
  HANDLE h = CreateFileA("PRUEBA.TXT", GW, 0, 0, 2, 0, 0);
  lin("crear"); sino(h != INVALID);
  WriteFile(h, "linea uno\r\nlinea dos\r\n", 22, &e, 0);
  lin("escritos"); num(e);
  CloseHandle(h);
  lin("atributos"); hx(GetFileAttributesA("PRUEBA.TXT"));
  h = CreateFileA("PRUEBA.TXT", GR, 1, 0, 3, 0, 0);
  lin("tamano"); num(GetFileSize(h, 0));
  char b[64];
  lin("mover"); num(SetFilePointer(h, 6, 0, 0));
  ReadFile(h, b, 3, &e, 0); b[e] = 0;
  lin("leer3"); pon(b);
  lin("desde_fin"); num(SetFilePointer(h, -3, 0, 2));
  ReadFile(h, b, 10, &e, 0);
  lin("leidos_fin"); num(e); pon(" "); hx(b[0]);
  ReadFile(h, b, 10, &e, 0);
  lin("leidos_eof"); num(e);
  CloseHandle(h);
  h = CreateFileA("NOHAY.TXT", GR, 0, 0, 3, 0, 0);
  lin("abrir_nohay"); sino(h == INVALID); pon(" error "); num(GetLastError());
  h = CreateFileA("PRUEBA.TXT", GW, 0, 0, 1, 0, 0);
  lin("crear_nuevo_existe"); sino(h == INVALID); pon(" error "); num(GetLastError());
  h = CreateFileA("PRUEBA.TXT", GW | GR, 0, 0, 4, 0, 0);
  SetFilePointer(h, 5, 0, 0); SetEndOfFile(h);
  lin("cortado"); num(GetFileSize(h, 0));
  CloseHandle(h);
  lin("copiar"); num(CopyFileA("PRUEBA.TXT", "COPIA.TXT", 0));
  lin("copiar_existe"); num(CopyFileA("PRUEBA.TXT", "COPIA.TXT", 1));
  lin("renombrar"); num(MoveFileA("COPIA.TXT", "MOVIDO.TXT"));
  lin("ya_no_esta"); hx(GetFileAttributesA("COPIA.TXT")); pon(" error "); num(GetLastError());
  /* buscar */
  FINDA f; int n = 0, vista1 = 0, vista2 = 0;
  h = FindFirstFileA("*.TXT", &f);
  if (h != INVALID) {
    do {
      n++;
      if (!lstrcmpiA(f.nombre, "PRUEBA.TXT")) vista1 = f.tam_lo;
      if (!lstrcmpiA(f.nombre, "MOVIDO.TXT")) vista2 = f.tam_lo + 100;
    } while (FindNextFileA(h, &f));
    lin("fin_busqueda"); num(GetLastError());
    FindClose(h);
  }
  lin("encontrados"); num(n); pon(" prueba "); num(vista1); pon(" movido "); num(vista2);
  ST st;
  FileTimeToSystemTime(&f.e, &st);
  lin("fecha_archivo_ok"); sino(st.anio >= 2026);
  h = FindFirstFileA("NOHAY*.XYZ", &f);
  lin("buscar_nada"); sino(h == INVALID); pon(" error "); num(GetLastError());
  /* directorios */
  lin("mkdir"); num(CreateDirectoryA("SUBDIR", 0));
  lin("cd"); num(SetCurrentDirectoryA("SUBDIR"));
  GetCurrentDirectoryA(64, b);
  lin("dir_actual"); pon(b);
  lin("necesita"); num(GetCurrentDirectoryA(2, b));
  SetCurrentDirectoryA("..");
  lin("rmdir"); num(RemoveDirectoryA("SUBDIR"));
  char *parte;
  lin("completo"); num(GetFullPathNameA("PRUEBA.TXT", 64, b, &parte)); pon(" "); pon(b); pon(" "); pon(parte);
  lin("tipo_a"); num(GetDriveTypeA("A:\\"));
  u32 spc, bps, lib, tot;
  lin("libre"); num(GetDiskFreeSpaceA("A:\\", &spc, &bps, &lib, &tot)); pon(" "); num(bps);
  u32 t = GetTempFileNameA("A:\\", "HW", 0, b);
  lin("temporal"); sino(t != 0); pon(" "); sino(GetFileAttributesA(b) != 0xFFFFFFFF);
  DeleteFileA(b);
  lin("borrar"); num(DeleteFileA("PRUEBA.TXT")); pon(" "); num(DeleteFileA("MOVIDO.TXT"));
  lin("borrar_otra_vez"); num(DeleteFileA("PRUEBA.TXT")); pon(" error "); num(GetLastError());
  /* hora */
  ST s1 = { 1970, 1, 0, 1, 0, 0, 0, 0 };
  FT ft;
  SystemTimeToFileTime(&s1, &ft);
  lin("ft_1970"); hx(ft.hi); hx(ft.lo);
  ST s2 = { 2026, 11, 0, 17, 12, 34, 56, 789 };
  SystemTimeToFileTime(&s2, &ft);
  ST s3; FileTimeToSystemTime(&ft, &s3);
  lin("ida_y_vuelta"); num(s3.anio); pon("-"); num(s3.mes); pon("-"); num(s3.dia); pon(" "); num(s3.hora); pon(":"); num(s3.min); pon(":"); num(s3.seg); pon("."); num(s3.ms); pon(" semana "); num(s3.sem);
  ST s4 = { 2000, 2, 0, 29, 23, 59, 58, 0 };
  SystemTimeToFileTime(&s4, &ft);
  FileTimeToSystemTime(&ft, &s3);
  lin("bisiesto"); num(s3.mes); pon("-"); num(s3.dia); pon(" semana "); num(s3.sem);
  u16 fd, fh; FileTimeToDosDateTime(&ft, &fd, &fh);
  lin("dos"); hx(fd); pon(" "); hx(fh);
  FT ft2; DosDateTimeToFileTime(fd, fh, &ft2);
  lin("dos_vuelta"); sn(CompareFileTime(&ft, &ft2));
  ft2.lo++;
  lin("comparar"); sn(CompareFileTime(&ft, &ft2)); pon(" "); sn(CompareFileTime(&ft2, &ft));
  ST ah; GetLocalTime(&ah);
  lin("ahora_ok"); sino(ah.anio >= 2026 && ah.mes >= 1 && ah.mes <= 12);
  /* memoria */
  HANDLE mh = GetProcessHeap();
  char *p1 = HeapAlloc(mh, 0, 100);
  HeapFree(mh, 0, p1);
  char *p2 = HeapAlloc(mh, 0, 90);
  lin("reusa"); sino(p1 == p2);
  lstrcpyA(p2, "conservado");
  p2 = HeapReAlloc(mh, 0, p2, 5000);
  lin("realloc"); pon(p2); pon(" "); num(HeapSize(mh, 0, p2)); pon(" "); num(p2[4999]);
  char *g = HeapAlloc(mh, 0, 3000000);
  g[0] = 1; g[2999999] = 2;
  lin("grande"); sino(g != 0); pon(" "); num(g[0] + g[2999999] + g[1500000]);
  HeapFree(mh, 0, g);
  int ok = 1;
  for (int i = 0; i < 20000; i++) { char *q = HeapAlloc(mh, 0, 1000); if (!q) { ok = 0; break; } q[999] = 1; HeapFree(mh, 0, q); }
  lin("20000_vueltas"); sino(ok);
  for (int i = 0; i < 20; i++) { char *q = HeapAlloc(mh, 0, 2000000); if (!q) { ok = 0; break; } HeapFree(mh, 0, q); }
  lin("20_grandes"); sino(ok);
  void *la = LocalAlloc(0x40, 50);
  lin("local"); sino(la != 0); pon(" "); sino(LocalFree(la) == 0);
  char *va = VirtualAlloc(0, 65536, 0x3000, 4);
  va[65535] = 7;
  lin("virtual"); sino(va != 0); pon(" "); num(VirtualAlloc(va + 4096, 4096, 0x1000, 4) == va + 4096); pon(" "); num(VirtualFree(va, 0, 0x8000));
  MEMST ms; GlobalMemoryStatus(&ms);
  lin("memoria"); num(ms.len); pon(" "); sino(ms.libre > 1000000); pon(" "); sino(ms.total >= ms.libre);
  /* entorno */
  lin("comspec"); sino(GetEnvironmentVariableA("COMSPEC", b, 64) > 0);
  lin("comspec_corto"); sino(GetEnvironmentVariableA("comspec", b, 2) > 2);
  lin("sin_variable"); num(GetEnvironmentVariableA("NOEXISTEESTA", b, 64)); pon(" error "); num(GetLastError());
  /* TLS e interlocked */
  u32 ti = TlsAlloc();
  TlsSetValue(ti, (void *)1234);
  lin("tls"); sino(ti != 0xFFFFFFFF); pon(" "); num((u32)TlsGetValue(ti));
  int v = 5;
  lin("interlocked"); sn(InterlockedIncrement(&v)); pon(" "); sn(InterlockedExchange(&v, 10)); pon(" "); sn(InterlockedCompareExchange(&v, 20, 10)); pon(" "); sn(v);
  /* módulos */
  HANDLE k = GetModuleHandleA("KERNEL32.DLL");
  lin("kernel32"); hx((u32)k);
  int (WINAPI *largo)(const char *) = GetProcAddress(k, "lstrlenA");
  lin("getprocaddress"); sino(largo != 0); pon(" "); num(largo ? largo("cuatro") : 0);
  lin("otra_vez_igual"); sino(GetProcAddress(k, "lstrlenA") == (void *)largo);
  lin("no_existe"); sino(GetProcAddress(k, "NoExisteNada") == 0); pon(" error "); num(GetLastError());
  lin("user32"); sino(LoadLibraryA("USER32.DLL") == 0); pon(" error "); num(GetLastError());
  lin("modulo_propio"); hx((u32)GetModuleHandleA(0));
  /* textos */
  lstrcpyA(b, "Hola"); lstrcatA(b, ", mundo");
  lin("textos"); pon(b); pon(" "); num(lstrlenA(b)); pon(" "); sn(lstrcmpiA("hola", "HOLA")); pon(" "); sn(lstrcmpA("a", "b"));
  u16 w[8];
  int nw = MultiByteToWideChar(0, 0, "abc", -1, w, 8);
  char b2[8]; int nb = WideCharToMultiByte(0, 0, w, -1, b2, 8, 0, 0);
  lin("anchos"); num(MultiByteToWideChar(0, 0, "abc", -1, 0, 0)); pon(" "); num(nw); pon(" "); num(w[1]); pon(" "); num(nb); pon(" "); pon(b2);
  pon("\r\nFIN K32PRUE\r\n");
  ExitProcess(3);
}
