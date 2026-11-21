/* k32.h - WIN32.BIN: lo que comparten las partes de KERNEL32 (kernel32.c,
 * archivos.c): los objetos (HANDLE) y unas pocas funciones.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef K32_H
#define K32_H
enum { O_LIBRE, O_ARCHIVO, O_CONSOLA_ENT, O_CONSOLA_SAL, O_BUSQUEDA, O_EVENTO, O_MUTEX, O_SEMAFORO, O_HILO, O_PROCESO, O_MAPA, O_CAMBIOS };
typedef struct {
	int tipo;
	u32 fd;                 /* el manejador del núcleo */
	u32 a, b;               /* según el tipo */
	u32 codigo, hecho;
	void *f, *arg;
	char patron[64];        /* (búsquedas) */
	u32 escribe;            /* abierto para escribir */
} Obj;
#define MAX_OBJ 96
extern Obj objs[MAX_OBJ];
HANDLE nuevo_obj(int tipo);
Obj *obj(HANDLE h);
void cerrar_todo(void);
extern int hilo_n;
extern u32 tls_usados[2];
int entrada_de(const char *ruta, Entrada *en, char *largo);
BOOL WINAPI SetCurrentDirectoryA(const char *d);
void *recurso(HMODULE h, const char *tipo, const char *nombre, u32 *tam);
void de_w(char *d, const WCHAR *s, u32 cabe);
void a_w(WCHAR *d, const char *s, u32 cabe);
u16 a_uni(u8 c);
u8 de_uni(u16 w);
#endif
