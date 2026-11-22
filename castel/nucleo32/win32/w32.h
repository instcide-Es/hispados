/* w32.h - WIN32.BIN, los programas de Windows de 32 bits en Hispa-Pentium
 * (docs/pentium/WIN32.md): lo común de sus piezas (el cargador de PE,
 * KERNEL32, USER32, GDI32 y las demás, en C).
 *
 * WIN32.BIN corre con los segmentos de base 0x40000000 (cpu.cas, win32.cas):
 * un puntero del programa (y de estas piezas) es una dirección de esa zona;
 * al núcleo se le da la de verdad (LIN). Los nombres y los números del API
 * son los documentados por Microsoft (sala limpia: nada de Wine).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef W32_H
#define W32_H
#include <stdarg.h>
#include <stddef.h>

typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef short i16;
typedef unsigned int u32;
typedef int i32;
typedef unsigned long long u64;
typedef long long i64;
typedef int BOOL;
typedef void *HANDLE;
typedef HANDLE HWND, HDC, HMENU, HINSTANCE, HMODULE, HGDIOBJ, HBRUSH, HPEN, HFONT, HBITMAP, HICON, HCURSOR, HRGN, HPALETTE, HACCEL, HGLOBAL, HRSRC, HKEY;
typedef u32 WPARAM;
typedef i32 LPARAM, LRESULT;
typedef u32 COLORREF;
typedef u16 WCHAR;
#define WINAPI __attribute__((stdcall))
#define CALLBACK WINAPI
#define EXPC(t) t                      /* (una de C que se exporta: wsprintfA) */
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)-1)

#define BASE_W32 0x40000000u          /* la base de los segmentos */
#define LIN(p) ((u32)(p) + BASE_W32)  /* la dirección de verdad de un puntero */
#define TEB_DIR 0x7FBF0000u           /* la TEB (FS), en el programa */
#define PEB_DIR 0x7FBF1000u

typedef struct { i32 left, top, right, bottom; } RECT;
typedef struct { i32 x, y; } POINT;
typedef struct { i32 cx, cy; } SIZE;
typedef struct { HWND hwnd; u32 message; WPARAM wParam; LPARAM lParam; u32 time; POINT pt; } MSG;
typedef LRESULT (WINAPI *WNDPROC)(HWND, u32, WPARAM, LPARAM);
typedef i32 (WINAPI *DLGPROC)(HWND, u32, WPARAM, LPARAM);

/* ---- las llamadas al sistema de Hispa-Pentium (docs/PENTIUM.md, sección 6) ---- */
static inline u32 sc(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;
	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}
#define ES_ERROR(r) ((u32)(r) >= 0xFFFFFFF0u)
enum {
	E_NO_HAY = 0xFFFFFFFF, E_SIN_DISCO = 0xFFFFFFFE, E_LLENO = 0xFFFFFFFD, E_MAL = 0xFFFFFFFC,
	E_NO_PROGRAMA = 0xFFFFFFFB, E_SIN_MEMORIA = 0xFFFFFFFA, E_NO_VALE = 0xFFFFFFF9,
	E_DISCO_LLENO = 0xFFFFFFF8, E_YA_EXISTE = 0xFFFFFFF7, E_EN_USO = 0xFFFFFFF6,
	E_PROTEGIDO = 0xFFFFFFF5, E_NO_VACIA = 0xFFFFFFF4, E_NOMBRE_MALO = 0xFFFFFFF3,
	E_ES_CARPETA = 0xFFFFFFF2, E_NO_CARPETA = 0xFFFFFFF1,
	E_DENEGADO = 0xFFFFFFF0	/* (PROTEGE) un archivo del sistema, o un secreto de otro */
};
/* una entrada de carpeta (la llamada 12/28) */
typedef struct { char nombre[13]; u8 atributos; u16 reservado; u32 tam; u16 fecha, hora; u32 cluster, libre; } Entrada;
/* un suceso de ventana (la 35) */
typedef struct { u32 clase, a, b, c; } Suceso;
enum { EV_TECLA = 1, EV_BAJA = 2, EV_SUBE = 3, EV_MUEVE = 4, EV_CERRAR = 5, EV_TAMANO = 6, EV_FOCO = 7, EV_FUERA = 8, EV_SALE = 9, EV_MENU = 10, EV_UNIDADES = 12 };
/* lo que se pide dibujar (la 34) */
typedef struct { u32 op; i32 x, y, ancho, alto; u32 tinta, tinta2; u32 datos; i32 n; } Orden;
/* las banderas de las ventanas del núcleo (ventanas.cas) */
enum { V_SIN_MARCO = 1, V_ENCIMA = 2, V_OCULTA = 4, V_MINIMIZADA = 8, V_MAXIMIZADA = 16, V_FIJA = 32, V_COMPLETA = 256 };
#define BORDE_NUCLEO 4
#define TITULO_NUCLEO 28

/* sys.c: las llamadas, con los punteros ya pasados a direcciones de verdad */
void sys_salir(u32 c) __attribute__((noreturn));
u32 sys_escribir(const void *p, u32 n);
u32 sys_tics(void);
void sys_dormir(u32 t);
u32 sys_tarea(void);
u32 sys_tecla(int espera);
u32 sys_abrir(const char *nombre, u32 modo);
u32 sys_leer(u32 m, void *d, u32 n);
u32 sys_escribir_archivo(u32 m, const void *p, u32 n);
void sys_cerrar(u32 m);
u32 sys_mover(u32 m, i32 cuanto, u32 origen);
u32 sys_cortar(u32 m);
u32 sys_largo(u32 m);
u32 sys_entrada_larga(u32 m, Entrada *e, char *largo);
u32 sys_argumentos(char *d, u32 cabe);
u32 sys_dato(u32 cual);
u32 sys_borrar(const char *n);
u32 sys_renombrar(const char *a, const char *b);
u32 sys_crear_carpeta(const char *n);
u32 sys_borrar_carpeta(const char *n);
u32 sys_cambiar_carpeta(const char *n);
u32 sys_carpeta_actual(char *d, u32 cabe);
u32 sys_unidad(u32 letra);
u32 sys_lanzar(const char *n, const char *args, int suelto);
u32 sys_esperar(u32 t, int sin_quedarse);
u32 sys_memoria(u32 dir, u32 n, int quitar);   /* (dir: del programa) */
u32 sys_teclas(u32 *mapa);
u32 sys_ventana_op(u32 op, u32 id, u32 dato);
u32 sys_ventana_nueva(const char *nombre, i32 x, i32 y, i32 an, i32 al, u32 marcas);
u32 sys_cerrar_ventana(u32 v);
u32 sys_titulo(u32 v, const char *t);
u32 sys_suceso(u32 v, Suceso *s);
u32 sys_mapa(u32 v, i32 x, i32 y, i32 an, i32 al, const u32 *puntos);
u32 sys_menu(u32 v, i32 x, i32 y, const char *opciones, u32 inactivas, u32 etiqueta);
u32 sys_portapapeles(u32 que, void *p, u32 n);
void sys_serie(const char *t);
u32 sys_atender_corte(int si);
u32 sys_hay_corte(void);
u32 sys_terminal_hay(void);

/* libc.c */
void *memcpy(void *d, const void *o, size_t n);
void *memmove(void *d, const void *o, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int stricmp_(const char *a, const char *b);
int strnicmp_(const char *a, const char *b, size_t n);
char *strcpy(char *d, const char *s);
char *strcat(char *d, const char *s);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
void copia_n(char *d, const char *s, u32 cabe);   /* como strncpy, acabando siempre en 0 */
int mayus(int c);
int minus(int c);
int formato_win(char *d, u32 cabe, const char *f, va_list va);   /* el de wsprintf */
int fmt(char *d, u32 cabe, const char *f, ...);
void traza(const char *f, ...);       /* al puerto serie, con SET HISPA32=TRAZA */
extern int traza_si;
void aviso_serie(const char *f, ...);  /* al puerto serie siempre */

/* montón.c: la memoria */
void *m_pedir(u32 n);
void *m_cero(u32 n);
void m_soltar(void *p);
void *m_cambiar(void *p, u32 n);
u32 m_tam(void *p);
u32 v_reservar(u32 dir, u32 n, int poner);   /* VirtualAlloc: da la dirección (0 si no) */
int v_soltar(u32 dir, u32 n);
int v_hay(u32 dir, u32 n);

/* cargador.c: los módulos */
typedef struct Exp { const char *nombre; void *f; } Exp;
typedef struct Modulo {
	char nombre[32];        /* «USER32.DLL» */
	char ruta[128];
	u32 base, tam;
	const Exp *propias;     /* las de dentro de WIN32.BIN (KERNEL32...), o 0 */
	u32 nprop;
	u32 entrada;
	int es_exe, refs, iniciada;
	int sin_hilos;          /* (JUEGOS) DisableThreadLibraryCalls: sin DllMain de los hilos */
	u32 rsrc, rsrc_tam;
	struct Modulo *sig;
} Modulo;
extern Modulo *modulos, *mod_exe;
Modulo *mod_cargar(const char *nombre, int *falta);
Modulo *mod_de(HANDLE h);
Modulo *mod_por_nombre(const char *nombre);
void *mod_funcion(Modulo *m, const char *nombre);
void mod_soltar(Modulo *m);
void mods_acabar(void);
extern char ruta_exe[160], carpeta_w32[128];

/* kernel32.c */
extern u32 ultimo_error;
void pon_error(u32 e);
u32 error_de(u32 r);           /* un error del núcleo, como los de Win32 */
void a437(char *d, const char *s, u32 cabe);   /* de la 1252 a la del núcleo */
void a1252(char *d, const char *s, u32 cabe);
extern const u8 t1252_437[128], t437_1252[128];
void w32_salir(u32 c) __attribute__((noreturn));
void carpeta_de_trabajo(char *d, u32 cabe);
void ruta_entera(const char *r, char *d, u32 cabe);
u32 tics_ms(void);

/* (JUEGOS) hilos.c: el cerrojo grande y las esperas */
extern int k_hilos;
u32 gc_suelta_todo(void);
void gc_retoma(u32 n);
void gc_toma(void);
void gc_suelta(void);
u32 fx_espera(volatile void *a, u32 v, u32 ms);
void fx_despierta(volatile void *a, u32 n);
void hilos_prepara(u32 teb, u32 tam_pila);
static inline u32 GetLastError_(void) { u32 v; __asm__ volatile("movl %%fs:0x34, %0" : "=r"(v)); return v; }

/* user32 */
void bomba(int esperar);        /* mira los sucesos del núcleo y los hace mensajes */
void dx_tic(void);
extern int pantalla_an, pantalla_al;

/* letra (datos.h, la hace hazdatos.py) */
extern const u8 letra8x16[256 * 16];

#endif
