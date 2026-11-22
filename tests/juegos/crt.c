/* CRT.EXE - prueba de MSVCRT.DLL con lo que usa un programa de Visual C++ 6
 * (JUEGOS): el arranque (__getmainargs, _initterm, _acmdln y WinMain con su
 * lpCmdLine, como lo saca el arranque de VC6), stdio en texto y en binario,
 * sprintf con coma flotante, _findfirst, la hora, qsort y bsearch,
 * setjmp/longjmp, atexit, _beginthreadex, _ftol y las _CI... del
 * compilador, new y delete de C++, _splitpath, los de Unicode. Cada línea,
 * «nombre: valor».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "j.h"
char **__p__acmdln(void);
void _initterm(void (**)(void), void (**)(void));
int __getmainargs(int *, char ***, char ***, int, void *);
typedef struct { u32 cb; char *r, *esc, *tit; u32 x, y, xs, ys, xc, yc, rell, banderas; u16 mostrar, r2; void *r3; HANDLE e, s, err; } STARTUPINFOA;
void WINAPI GetStartupInfoA(STARTUPINFOA *);
void _splitpath(const char *, char *, char *, char *, char *);
void _makepath(char *, const char *, const char *, const char *, const char *);
int strftime(char *, u32, const char *, const void *);
void *localtime(const i32 *);
void *bsearch(const void *, const void *, u32, u32, int (*)(const void *, const void *));
char *_ecvt(double, int, int *, int *);
int _snwprintf(u16 *, u32, const u16 *, ...);
char *_strdate(char *);
char *_strtime(char *);
void *op_new(u32) __asm__("\"??2@YAPAXI@Z\"");
void op_delete(void *) __asm__("\"??3@YAXPAX@Z\"");
i64 a_entero(double);   /* (_ftol) */
double potencia(double, double);   /* (_CIpow) */
double raiz(double);   /* (_CIsqrt) */
__asm__(".text\n"
	"a_entero:\n  fldl 4(%esp)\n  call _ftol\n  ret\n"
	"potencia:\n  fldl 4(%esp)\n  fldl 12(%esp)\n  call _CIpow\n  ret\n"
	"raiz:\n  fldl 4(%esp)\n  call _CIsqrt\n  ret\n");

static int ini_hecho;
static void ini1(void) { ini_hecho += 1; }
static void ini2(void) { ini_hecho += 10; }
static void (*tabla_ini[])(void) = { ini1, 0, ini2 };
static void al_salir1(void) { printf("atexit: primero puesto, el ultimo\n"); }
static void al_salir2(void) { printf("atexit: segundo puesto, el primero\n"); }
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
static int cmp_txt(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static volatile int en_hilo;
static u32 WINAPI hilo_ex(void *p) { en_hilo = (int)p; return 9; }
static void hilo_c(void *p) { en_hilo += (int)p; }

/* WinMain y su lpCmdLine, como los saca el arranque de VC6 (WinMainCRTStartup) */
static int WINAPI WinMain_(HINSTANCE i, HINSTANCE p, char *linea, int ver)
{
	(void)p;
	printf("winmain: [%s] %s %d\n", linea, i == GetModuleHandleA(0) ? "hinstance" : "MAL", ver);
	return 0;
}
static void arranque_winmain(void)
{
	char *p = *__p__acmdln();
	STARTUPINFOA si;
	if (*p == '"') {
		p++;
		while (*p && *p != '"')
			p++;
		if (*p == '"')
			p++;
	} else
		while ((u8)*p > ' ')
			p++;
	while (*p && (u8)*p <= ' ')
		p++;
	si.banderas = 0;
	GetStartupInfoA(&si);
	WinMain_(GetModuleHandleA(0), 0, p, si.banderas & 1 ? si.mostrar : 10);
}

int main(int argc, char **argv)
{
	char b[300];
	FILE *f;
	int k;
	jmp_buf jb;
	printf("argc: %d\n", argc);
	for (k = 1; k < argc; k++)
		printf("argv%d: [%s]\n", k, argv[k]);
	_initterm(tabla_ini, tabla_ini + 3);
	printf("initterm: %d\n", ini_hecho);
	arranque_winmain();
	atexit(al_salir1);
	atexit(al_salir2);
	/* stdio: texto y binario */
	f = fopen("PRUEBA.TXT", "wt");
	fprintf(f, "uno\ndos\n");
	fclose(f);
	f = fopen("PRUEBA.TXT", "rb");
	k = fread(b, 1, sizeof b, f);
	fclose(f);
	printf("texto_en_disco: %d %02x%02x\n", k, (u8)b[3], (u8)b[4]);
	f = fopen("PRUEBA.TXT", "rt");
	k = fread(b, 1, sizeof b, f);
	fclose(f);
	printf("texto_leido: %d %02x\n", k, (u8)b[3]);
	f = fopen("PRUEBA.BIN", "wb");
	fwrite("a\nb\x1A" "c", 1, 5, f);
	fclose(f);
	f = fopen("PRUEBA.BIN", "rb");
	fseek(f, 0, 2);
	printf("binario: %d", (int)ftell(f));
	fseek(f, 2, 0);
	printf(" %c\n", fgetc(f));
	fclose(f);
	remove("PRUEBA.BIN");
	/* sprintf con coma flotante */
	sprintf(b, "%f|%.3f|%5.1f|%e|%g|%g|%.0f|%-8.2f|%+.2f", 3.14159265, -2.5, 7.25, 12345.678, 0.0001, 1e20, 2.5, 1.005, 3.0);
	printf("sprintf: %s\n", b);
	{
		double x = 0;
		int n = sscanf("12 3.5", "%d %lf", &k, &x);
		printf("sscanf: %d %d %g\n", n, k, x);
	}
	/* _findfirst */
	{
		struct _finddata_t d;
		i32 h = _findfirst("*.EXE", &d);
		int n = 0, crt = 0;
		if (h != -1) {
			do {
				n++;
				if (!strcmp(d.name, "CRT.EXE"))
					crt = 1;
			} while (!_findnext(h, &d));
			_findclose(h);
		}
		printf("findfirst: %s %s\n", n >= 3 ? "varios" : "pocos", crt ? "CRT.EXE" : "sin CRT.EXE");
		printf("findfirst_nada: %d\n", _findfirst("NOHAY*.ZZZ", &d));
	}
	/* la hora */
	{
		i32 t = time(0);
		strftime(b, sizeof b, "%Y-%m-%d", localtime(&t));
		printf("time: %s %s\n", t > 1767225600 ? "2026 o despues" : "MAL", strlen(b) == 10 ? "fecha" : b);
		_strdate(b);
		printf("strdate: %u\n", strlen(b));
	}
	/* qsort y bsearch */
	{
		int v[8] = { 5, 3, 9, 1, 7, 2, 8, 4 }, x = 7, *r;
		char *t[4] = { "pera", "manzana", "uva", "kiwi" };
		qsort(v, 8, 4, cmp_int);
		qsort(t, 4, sizeof t[0], cmp_txt);
		r = bsearch(&x, v, 8, 4, cmp_int);
		printf("qsort: %d%d%d%d%d%d%d%d %s %s %d\n", v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], t[0], t[3], r ? (int)(r - v) : -1);
	}
	/* setjmp y longjmp */
	k = _setjmp3(jb, 0);
	if (!k)
		longjmp(jb, 5);
	printf("longjmp: %d\n", k);
	/* los hilos de la biblioteca */
	{
		u32 id = 0, c = 0;
		HANDLE h = (HANDLE)_beginthreadex(0, 0, hilo_ex, (void *)40, 0, &id);
		WaitForSingleObject(h, 5000);
		GetExitCodeThread(h, &c);
		printf("beginthreadex: %d %u %s\n", en_hilo, c, id ? "con id" : "sin id");
		h = (HANDLE)_beginthread(hilo_c, 0, (void *)2);
		WaitForSingleObject(h, 5000);
		Sleep(10);
		printf("beginthread: %d\n", en_hilo);
	}
	/* lo del compilador: _ftol, _CIpow, _CIsqrt */
	{
		i64 a = a_entero(-3.7), c = a_entero(1e10);
		double p = potencia(2.0, 10.0), q = raiz(2.0);
		printf("ftol: %d %08x%08x\n", (int)a, (u32)((u64)c >> 32), (u32)c);
		printf("cipow: %d %d\n", (int)p, (int)(q * 100000));
	}
	/* C++: new y delete */
	{
		int *p = op_new(40);
		p[9] = 3;
		printf("new: %s %d\n", p ? "si" : "no", p[9]);
		op_delete(p);
	}
	/* rutas */
	{
		char d[4], c[64], n[32], e[8], r[100];
		_splitpath("C:\\JUEGOS\\VALVE\\hl.exe", d, c, n, e);
		_makepath(r, "D", "\\OTRA", n, "dll");
		printf("splitpath: %s|%s|%s|%s %s\n", d, c, n, e, r);
	}
	{
		int dec, sig;
		char *s = _ecvt(-123.456, 5, &dec, &sig);
		printf("ecvt: %s %d %d\n", s, dec, sig);
	}
	{
		u16 w[40];
		static const u16 fw[] = { '%', 's', '=', '%', 'd', 0 }, nom[] = { 'v', 'i', 'd', 'a', 0 };
		int n = _snwprintf(w, 40, fw, nom, 100);
		printf("unicode: %d %c%c%c%c%c%c%c%c\n", n, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
	}
	remove("PRUEBA.TXT");
	printf("FIN CRT\n");
	return 0;
}
