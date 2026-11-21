/* sys.c - WIN32.BIN: las llamadas al sistema de Hispa-Pentium (int 0x80,
 * docs/PENTIUM.md sección 6), con los punteros pasados a direcciones de
 * verdad (LIN): los segmentos de WIN32.BIN tienen base 0x40000000.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"

void sys_salir(u32 c)
{
	for (;;)
		sc(2, c, 0, 0);
}
u32 sys_escribir(const void *p, u32 n) { return sc(1, LIN(p), n, 0); }
u32 sys_tics(void) { return sc(4, 0, 0, 0); }
void sys_dormir(u32 t) { sc(3, t, 0, 0); }
u32 sys_tarea(void) { return sc(5, 0, 0, 0); }
u32 sys_tecla(int espera) { return sc(6, espera ? 0 : 1, 0, 0); }
u32 sys_abrir(const char *nombre, u32 modo) { return sc(7, LIN(nombre), modo, 0); }
u32 sys_leer(u32 m, void *d, u32 n) { return sc(8, m, LIN(d), n); }
u32 sys_escribir_archivo(u32 m, const void *p, u32 n) { return sc(18, m, LIN(p), n); }
void sys_cerrar(u32 m) { sc(9, m, 0, 0); }
u32 sys_mover(u32 m, i32 cuanto, u32 origen) { return sc(19, m, (u32)cuanto, origen); }
u32 sys_cortar(u32 m) { return sc(20, m, 0, 0); }
u32 sys_largo(u32 m) { return sc(13, m, 0, 0); }
u32 sys_entrada_larga(u32 m, Entrada *e, char *largo) { return sc(28, m, LIN(e), LIN(largo)); }
u32 sys_argumentos(char *d, u32 cabe) { return sc(14, LIN(d), cabe, 0); }
u32 sys_dato(u32 cual) { return sc(15, cual, 0, 0); }
u32 sys_borrar(const char *n) { return sc(21, LIN(n), 0, 0); }
u32 sys_renombrar(const char *a, const char *b) { return sc(22, LIN(a), LIN(b), 0); }
u32 sys_crear_carpeta(const char *n) { return sc(23, LIN(n), 0, 0); }
u32 sys_borrar_carpeta(const char *n) { return sc(24, LIN(n), 0, 0); }
u32 sys_cambiar_carpeta(const char *n) { return sc(25, LIN(n), 0, 0); }
u32 sys_carpeta_actual(char *d, u32 cabe) { return sc(26, LIN(d), cabe, 0); }
u32 sys_unidad(u32 letra) { return sc(42, letra, 0, 0); }
u32 sys_lanzar(const char *n, const char *args, int suelto) { return sc(suelto ? 41 : 10, LIN(n), args ? LIN(args) : 0, 0); }
u32 sys_esperar(u32 t, int sin_quedarse) { return sc(11, t, sin_quedarse ? 1 : 0, 0); }
u32 sys_memoria(u32 dir, u32 n, int quitar) { return sc(180, LIN(dir), n, quitar ? 1 : 0); }
u32 sys_teclas(u32 *mapa) { return sc(181, LIN(mapa), 0, 0); }
u32 sys_ventana_op(u32 op, u32 id, u32 dato) { return sc(182, op, id, dato); }

/* la 182, 0 y 1: lo que se pide (win32.cas, Pide_ventana) */
static struct { u32 nombre; i32 x, y, an, al; u32 marcas; } pv;

u32 sys_ventana_nueva(const char *nombre, i32 x, i32 y, i32 an, i32 al, u32 marcas)
{
	pv.nombre = LIN(nombre);
	pv.x = x; pv.y = y; pv.an = an; pv.al = al; pv.marcas = marcas;
	return sc(182, 0, 0, LIN(&pv));
}
u32 sys_cerrar_ventana(u32 v) { return sc(33, v, 0, 0); }
u32 sys_titulo(u32 v, const char *t) { return sc(36, v, LIN(t), 0); }
u32 sys_suceso(u32 v, Suceso *s) { return sc(35, v, LIN(s), 0); }

static Orden ord;
u32 sys_mapa(u32 v, i32 x, i32 y, i32 an, i32 al, const u32 *puntos)
{
	ord.op = 7;
	ord.x = x; ord.y = y; ord.ancho = an; ord.alto = al;
	ord.datos = LIN(puntos);
	return sc(34, v, LIN(&ord), 0);
}

static struct { i32 x, y; u32 opciones, inactivas, etiqueta; } pm;
u32 sys_menu(u32 v, i32 x, i32 y, const char *opciones, u32 inactivas, u32 etiqueta)
{
	pm.x = x; pm.y = y; pm.opciones = LIN(opciones); pm.inactivas = inactivas; pm.etiqueta = etiqueta;
	return sc(39, v, LIN(&pm), 0);
}
u32 sys_portapapeles(u32 que, void *p, u32 n) { return sc(40, que, p ? LIN(p) : 0, n); }
void sys_serie(const char *t) { sc(61, LIN(t), strlen(t), 0); }
u32 sys_atender_corte(int si) { return sc(29, si ? 1 : 0, 0, 0); }
u32 sys_hay_corte(void) { return sc(30, 0, 0, 0); }
