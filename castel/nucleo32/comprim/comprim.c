/* comprim.c - COMPRIM.BIN, «Comprimidos» de Hispa-Pentium: los .ZIP como
 * una carpeta más (docs/PENTIUM.md, «Comprimidos»). Un programa del anillo
 * 3 en C, con su ventana (ui.c) y el motor de zip.c:
 *
 *   COMPRIM [RUTA.ZIP]          la ventana: lo que lleva, como una carpeta
 *                               (entrar, subir, elegir; sacar todo o lo
 *                               elegido, añadir, quitar, abrir uno, sacarlo
 *                               arrastrándolo al escritorio)
 *   COMPRIM /COMPRIMIR LISTA    hace un ZIP con lo de LISTA (un archivo con
 *                               una ruta por línea, que luego se borra): el
 *                               «Comprimir en ZIP» del explorador
 *   COMPRIM /EXTRAER RUTA.ZIP   lo saca todo a una carpeta de al lado (con su
 *                               nombre) y la abre: el «Extraer todo» del explorador
 *   COMPRIM /ZIP ...  y  /UNZIP ...   las órdenes ZIP y UNZIP (ordenes.c; las
 *                               lanzan ZIP.BIN y UNZIP.BIN, lanza.c)
 *
 * Lo que pasa, por el puerto serie («comprim: ...»: lo miran las pruebas).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "pentium.h"
#include "ui.h"
#include "zip.h"
#include "ordenes.h"

static int con_ventana;		/* 0: las órdenes, por la consola */
static char aviso[300];
static char clave[128];
static int con_clave;

static void serie(const char *f, ...)
{
	char t[400];
	va_list a;

	va_start(a, f);
	vsnprintf(t, sizeof(t), f, a);
	va_end(a);
	ui_serie(t);
}

/* ------------------------------------------------ lo de la consola */
void pl_escribe(const char *t) { sc(SC_ESCRIBIR, (uint32_t)t, (uint32_t)strlen(t), 0); }
int pl_cortado(void) { return sc(SC_HUBO_CORTE, 0, 0, 0) == 1; }

const char *pl_pide_clave(const char *archivo)
{
	int n = 0;
	uint32_t k;

	if (con_ventana) {
		clave[0] = 0;
		if (!ui_pide_texto("Contrase\xA4" "a", "Este ZIP va cifrado. Escriba su contrase\xA4" "a:", clave, sizeof(clave), 1))
			return NULL;
		con_clave = 1;
		return clave;
	}
	pl_escribe("Contrase\xA4" "a de ");
	pl_escribe(archivo);
	pl_escribe(": ");
	for (;;) {
		k = sc(SC_TECLA, 0, 0, 0);
		if (ES_INTRO(k))
			break;
		if (k == 3 || k == T_ESC) {
			pl_escribe("\r\n");
			return NULL;
		}
		if (k == T_BORRAR && n > 0) {
			n--;
			pl_escribe("\b \b");
		} else if (k >= 32 && k < 256 && n + 1 < (int)sizeof(clave)) {
			clave[n++] = (char)k;
			pl_escribe("*");
		}
	}
	clave[n] = 0;
	pl_escribe("\r\n");
	return clave;
}

void pl_temporal(const char *ruta, char *sale, int cabe)
{
	char *b;

	snprintf(sale, (size_t)cabe, "%s", ruta);
	b = strrchr(sale, '\\');
	b = b ? b + 1 : (sale[1] == ':' ? sale + 2 : sale);
	snprintf(b, (size_t)(cabe - (b - sale)), "COMPRIM.$$$");
}

/* ---------------------------------------------------- el progreso */
static uint32_t ultimo_tic;
static const char *haciendo = "Trabajando";
static int cancelado;
static void pinta(void);
static void pinta_progreso(uint64_t hecho, uint64_t total, const char *nombre);

int pl_progreso(uint64_t hecho, uint64_t total, const char *nombre)
{
	struct suceso s;
	uint32_t t;

	if (!con_ventana)
		return pl_progreso_orden(hecho, total, nombre);
	t = ui_tics();
	if (t - ultimo_tic < ui_hz() / 6)
		return cancelado;
	ultimo_tic = t;
	pinta_progreso(hecho, total, nombre);
	while (ui_suceso(&s, 0)) {
		if (s.clase == EV_CERRAR || (s.clase == EV_TECLA && s.a == T_ESC))
			cancelado = 1;
		if (s.clase == EV_RATON_BAJA && ui_en_zona((int)s.a, (int)s.b) == 99)
			cancelado = 1;
	}
	return cancelado;
}

/* --------------------------------------------------- lo de dentro */
typedef struct {
	char ver[96];		/* el nombre, en la página 437 */
	char *nombre;		/* el trozo, en UTF-8 (sin la «/») */
	int carpeta;
	int32_t miembro;	/* -1: una carpeta que solo sale en los nombres */
	uint64_t tam, comp;
	uint16_t fecha, hora;
	uint8_t elegido, cifrado, peligro;
	uint32_t dentro_n;	/* (de las carpetas) cuántos lleva */
} fila;

static zarchivo z;
static int abierto;
static char ruta_zip[ZIP_MAX_RUTA];
static char dentro[ZIP_MAX_NOMBRE];	/* la carpeta del ZIP en la que se está («», o «a/b/») */
static fila *filas;
static int n_filas, cabe_filas, cursor, primera, ancla = -1;

#define ARRIBA_LISTA 96
#define ALTO_FILA 20
#define ALTO_ESTADO 26

static int filas_visibles(void)
{
	int n = (ui_al - ARRIBA_LISTA - ALTO_ESTADO) / ALTO_FILA;

	return n < 1 ? 1 : n;
}

static int menor_sin_caja(const char *a, const char *b)
{
	for (; *a && *b; a++, b++) {
		int x = (unsigned char)*a, y = (unsigned char)*b;

		if (x >= 'a' && x <= 'z')
			x -= 32;
		if (y >= 'a' && y <= 'z')
			y -= 32;
		if (x != y)
			return x < y;
	}
	return !*a && *b;
}

static void suelta_filas(void)
{
	int i;

	for (i = 0; i < n_filas; i++)
		free(filas[i].nombre);
	n_filas = 0;
}

static fila *nueva_fila(void)
{
	if (n_filas == cabe_filas) {
		fila *q = realloc(filas, (size_t)(cabe_filas * 2 + 64) * sizeof(fila));

		if (!q)
			return NULL;
		filas = q;
		cabe_filas = cabe_filas * 2 + 64;
	}
	memset(&filas[n_filas], 0, sizeof(fila));
	return &filas[n_filas++];
}

/* las filas: lo que cuelga de «dentro», las carpetas delante */
static void haz_filas(void)
{
	uint32_t i;
	int k, j;
	size_t ld = strlen(dentro);

	suelta_filas();
	for (i = 0; abierto && i < z.n; i++) {
		zmiembro *m = &z.m[i];
		const char *resto, *barra;
		fila *f = NULL;
		size_t lr;

		if (strncmp(m->nombre, dentro, ld))
			continue;
		resto = m->nombre + ld;
		if (!*resto)
			continue;
		barra = strchr(resto, '/');
		lr = barra ? (size_t)(barra - resto) : strlen(resto);
		if (barra) {
			/* una carpeta (que puede salir solo en los nombres de lo de dentro) */
			for (k = 0; k < n_filas; k++)
				if (filas[k].carpeta && strlen(filas[k].nombre) == lr && !strncmp(filas[k].nombre, resto, lr))
					break;
			if (k < n_filas)
				f = &filas[k];
			else {
				f = nueva_fila();
				if (!f)
					break;
				f->nombre = malloc(lr + 1);
				if (!f->nombre) {
					n_filas--;
					break;
				}
				memcpy(f->nombre, resto, lr);
				f->nombre[lr] = 0;
				f->carpeta = 1;
				f->miembro = -1;
			}
			if (!barra[1]) {
				f->miembro = (int32_t)i;
				f->fecha = m->fecha;
				f->hora = m->hora;
			} else {
				f->dentro_n++;
				f->tam += m->tam;
				f->comp += m->comp;
			}
			if (m->peligro)
				f->peligro = 1;
			continue;
		}
		f = nueva_fila();
		if (!f)
			break;
		f->nombre = malloc(lr + 1);
		if (!f->nombre) {
			n_filas--;
			break;
		}
		memcpy(f->nombre, resto, lr + 1);
		f->miembro = (int32_t)i;
		f->tam = m->tam;
		f->comp = m->comp;
		f->fecha = m->fecha;
		f->hora = m->hora;
		f->cifrado = m->cifrado != 0;
		f->peligro = m->peligro != 0 || m->enlace;
	}
	for (k = 0; k < n_filas; k++)
		utf8_a_437(filas[k].nombre, filas[k].ver, sizeof(filas[k].ver));
	/* ordenadas: las carpetas delante, luego por el nombre */
	for (k = 1; k < n_filas; k++) {
		fila t = filas[k];

		for (j = k; j > 0 && (t.carpeta > filas[j - 1].carpeta ||
				      (t.carpeta == filas[j - 1].carpeta && menor_sin_caja(t.ver, filas[j - 1].ver))); j--)
			filas[j] = filas[j - 1];
		filas[j] = t;
	}
	if (cursor >= n_filas)
		cursor = n_filas ? n_filas - 1 : 0;
	if (primera > cursor)
		primera = cursor;
	ancla = -1;
}

static int cuantos_elegidos(void)
{
	int i, n = 0;

	for (i = 0; i < n_filas; i++)
		n += filas[i].elegido;
	return n;
}

/* los miembros de lo elegido (las carpetas, con todo lo de dentro) */
static uint8_t *marca_elegidos(int solo_cursor)
{
	uint8_t *m = calloc(z.n + 1, 1);
	char pref[ZIP_MAX_NOMBRE + 2];
	int i;
	uint32_t k;

	if (!m)
		return NULL;
	for (i = 0; i < n_filas; i++) {
		fila *f = &filas[i];

		if (solo_cursor ? i != cursor : !f->elegido)
			continue;
		if (!f->carpeta) {
			m[f->miembro] = 1;
			continue;
		}
		snprintf(pref, sizeof(pref), "%s%s/", dentro, f->nombre);
		for (k = 0; k < z.n; k++)
			if (!strncmp(z.m[k].nombre, pref, strlen(pref)))
				m[k] = 1;
	}
	return m;
}

/* ------------------------------------------------------- pintar */
#define B_EXTRAER_TODO 1
#define B_EXTRAER 2
#define B_ANADIR 3
#define B_QUITAR 4
#define B_SUBIR 5
#define B_ABRIR 6
#define B_NUEVO 7
#define B_CANCELAR 99

static void tamano(uint64_t n, char *t)
{
	if (n < 1024)
		snprintf(t, 24, "%u B", (uint32_t)n);
	else if (n < 1024 * 1024)
		snprintf(t, 24, "%u KB", (uint32_t)((n + 1023) / 1024));
	else if (n < 10ull * 1024 * 1024 * 1024)
		snprintf(t, 24, "%u,%u MB", (uint32_t)(n >> 20), (uint32_t)(((n & 0xFFFFF) * 10) >> 20));
	else
		snprintf(t, 24, "%u GB", (uint32_t)(n >> 30));
}

static void a_la_derecha(int xfin, int y, const char *t, uint32_t c)
{
	ui_texto(xfin - (int)strlen(t) * ANCHO_LETRA, y, t, 30, c);
}

static void icono_carpeta(int x, int y)
{
	ui_rect(x, y + 3, 7, 3, 0xD4A017);
	ui_rect(x, y + 5, 16, 11, 0xD4A017);
	ui_rect(x + 1, y + 7, 14, 8, 0xFACC15);
}

static void icono_hoja(int x, int y)
{
	ui_rect(x + 2, y + 1, 12, 15, 0x6F87A8);
	ui_rect(x + 3, y + 2, 10, 13, C_PAPEL);
	ui_rect(x + 5, y + 6, 6, 1, 0x7D93B2);
	ui_rect(x + 5, y + 9, 6, 1, 0x7D93B2);
	ui_rect(x + 5, y + 12, 4, 1, 0x7D93B2);
}

static void ruta_visible(char *t, int cabe)
{
	char d[ZIP_MAX_NOMBRE];
	size_t i;

	utf8_a_437(dentro, d, sizeof(d));
	for (i = 0; d[i]; i++)
		if (d[i] == '/')
			d[i] = '\\';
	snprintf(t, (size_t)cabe, "%s%s%s", ruta_zip, d[0] ? "\\" : "", d);
	if (d[0] && t[strlen(t) - 1] == '\\')
		t[strlen(t) - 1] = 0;
}

static void pinta_estado(const char *t)
{
	int y = ui_al - ALTO_ESTADO;

	ui_rect(0, y, ui_an, ALTO_ESTADO, C_BARRA);
	ui_rect(0, y, ui_an, 1, C_GRIS);
	ui_texto(8, y + 5, t, (ui_an - 16) / ANCHO_LETRA, C_SUAVE);
}

static void pinta(void)
{
	int an = ui_an, x, i, y, vis, max_nombre, x_tam, x_comp, x_fecha, sel;
	char t[400], a[24], b[24];

	ui_olvida_zonas();
	ui_rect(0, 0, an, ui_al, C_FONDO);
	/* la barra de herramientas */
	ui_rect(0, 0, an, 40, C_BARRA);
	ui_rect(0, 40, an, 1, C_GRIS);
	sel = cuantos_elegidos();
	x = 8;
	x += ui_boton(B_EXTRAER_TODO, x, 6, 0, "Extraer todo", abierto && z.n > 0, !abierto || z.n == 0) + 6;
	x += ui_boton(B_EXTRAER, x, 6, 0, "Extraer lo elegido", 0, !abierto || !sel) + 6;
	x += ui_boton(B_ANADIR, x, 6, 0, "A\xA4" "adir...", 0, !abierto) + 6;
	x += ui_boton(B_QUITAR, x, 6, 0, "Quitar", 0, !abierto || !sel) + 6;
	x += ui_boton(B_SUBIR, x, 6, 0, "Subir", 0, !abierto || !dentro[0]) + 6;
	if (x + ui_ancho_boton("Abrir...") + ui_ancho_boton("Nuevo...") + 14 < an) {
		x += ui_boton(B_ABRIR, x, 6, 0, "Abrir...", !abierto, 0) + 6;
		ui_boton(B_NUEVO, x, 6, 0, "Nuevo...", 0, 0);
	}
	/* la dirección */
	ui_rect(8, 48, an - 16, 22, C_BORDE);
	ui_rect(9, 49, an - 18, 20, C_PAPEL);
	if (abierto) {
		ruta_visible(t, sizeof(t));
		ui_texto(14, 51, t, (an - 30) / ANCHO_LETRA, C_TINTA);
	} else
		ui_texto(14, 51, "(ning\xA3n ZIP abierto)", 40, C_SUAVE);
	/* los rótulos */
	x_fecha = an - 8 - 16 * ANCHO_LETRA - 8;
	x_comp = x_fecha - 16;
	x_tam = x_comp - 12 * ANCHO_LETRA;
	max_nombre = (x_tam - 12 * ANCHO_LETRA - 40) / ANCHO_LETRA;
	ui_rect(8, 74, an - 16, 22, 0xE5E7EB);
	ui_negrita(34, 77, "Nombre", 20, C_TINTA);
	a_la_derecha(x_tam, 77, "Tama\xA4o", C_TINTA);
	a_la_derecha(x_comp, 77, "Comprimido", C_TINTA);
	ui_negrita(x_fecha + 8, 77, "Modificado", 20, C_TINTA);
	/* la lista */
	vis = filas_visibles();
	ui_rect(8, ARRIBA_LISTA, an - 16, vis * ALTO_FILA, C_PAPEL);
	if (abierto && n_filas == 0)
		ui_texto(34, ARRIBA_LISTA + 8, "Esta carpeta del ZIP est\xA0 vac\xA1" "a.", 60, C_SUAVE);
	if (!abierto) {
		ui_texto(34, ARRIBA_LISTA + 8, "Abra un .ZIP con dos clics en Mi equipo, o con \xAE" "Abrir...\xAF.", (an - 50) / ANCHO_LETRA, C_SUAVE);
		ui_texto(34, ARRIBA_LISTA + 28, "\xAENuevo...\xAF hace uno vac\xA1o, para meterle cosas con \xAE" "A\xA4" "adir...\xAF.", (an - 50) / ANCHO_LETRA, C_SUAVE);
	}
	for (i = 0; i < vis && primera + i < n_filas; i++) {
		fila *f = &filas[primera + i];
		uint32_t tinta = f->peligro ? C_ROJO : C_TINTA;

		y = ARRIBA_LISTA + i * ALTO_FILA;
		if (f->elegido)
			ui_rect(9, y, an - 18, ALTO_FILA, C_AZUL_CLARO);
		if (primera + i == cursor)
			ui_marco(9, y, an - 18, ALTO_FILA, C_AZUL);
		if (f->carpeta)
			icono_carpeta(12, y + 2);
		else
			icono_hoja(12, y + 2);
		if (f->cifrado) {
			/* el candado */
			ui_rect(20, y + 10, 8, 7, C_NARANJA);
			ui_marco(21, y + 6, 6, 5, C_NARANJA);
		}
		snprintf(t, sizeof(t), "%s%s", f->ver, f->peligro ? "  (PELIGROSO: no se saca)" : "");
		ui_texto(34, y + 2, t, max_nombre, tinta);
		if (f->carpeta)
			snprintf(a, sizeof(a), "%u dentro", f->dentro_n);
		else
			tamano(f->tam, a);
		a_la_derecha(x_tam, y + 2, a, C_SUAVE);
		if (!f->carpeta) {
			tamano(f->comp, b);
			a_la_derecha(x_comp, y + 2, b, C_SUAVE);
		}
		if (f->fecha) {
			snprintf(t, sizeof(t), "%02u/%02u/%04u %02u:%02u", f->fecha & 31, (f->fecha >> 5) & 15, 1980 + (f->fecha >> 9), f->hora >> 11, (f->hora >> 5) & 63);
			ui_texto(x_fecha + 8, y + 2, t, 16, C_SUAVE);
		}
	}
	/* la barra de desplazar */
	if (n_filas > vis) {
		int alto = vis * ALTO_FILA, k = alto * vis / n_filas, d = (alto - k) * primera / (n_filas - vis);

		ui_rect(an - 18, ARRIBA_LISTA, 9, alto, 0xE5E7EB);
		ui_rect(an - 17, ARRIBA_LISTA + d, 7, k < 12 ? 12 : k, 0x9CA3AF);
	}
	/* el estado */
	if (aviso[0])
		pinta_estado(aviso);
	else if (abierto) {
		tamano(z.total_tam, a);
		tamano(z.largo, b);
		{
			char u[40];

			if (sel)
				snprintf(u, sizeof(u), ", %d elegido%s", sel, sel == 1 ? "" : "s");
			else
				u[0] = 0;
			snprintf(t, sizeof(t), "%d elemento%s aqu\xA1%s; en todo el ZIP, %u archivo%s: %s (ocupa %s)%s", n_filas, n_filas == 1 ? "" : "s", u,
				 z.n, z.n == 1 ? "" : "s", a, b, z.cifrados ? "; con contrase\xA4" "a" : "");
		}
		pinta_estado(t);
	} else
		pinta_estado("Comprimidos: los .ZIP como una carpeta.");
}

static void pinta_progreso(uint64_t hecho, uint64_t total, const char *nombre)
{
	char t[300], n[200];
	int y = ui_al - ALTO_ESTADO, an = ui_an;

	utf8_a_437(nombre, n, sizeof(n));
	snprintf(t, sizeof(t), "%s: %s", haciendo, n);
	ui_rect(0, y, an, ALTO_ESTADO, C_BARRA);
	ui_rect(0, y, an, 1, C_GRIS);
	ui_texto(8, y + 5, t, (an - 260) / ANCHO_LETRA, C_TINTA);
	ui_barra(an - 240, y + 6, 140, (uint32_t)(total > 0xFFFFFFu ? hecho >> 8 : hecho), (uint32_t)(total > 0xFFFFFFu ? total >> 8 : total));
	ui_olvida_zonas();
	ui_boton(B_CANCELAR, an - 92, y - 2, 84, "Cancelar", 0, 0);
	ui_zona(B_CANCELAR, an - 92, y - 2, 84, ALTO_BOTON);
}

/* ------------------------------------------------------- abrir */
static int abre_zip(const char *ruta)
{
	int r;
	char t[ZIP_MAX_RUTA + 40];

	if (abierto)
		zip_cerrar(&z);
	abierto = 0;
	r = zip_abrir(&z, ruta);
	if (r) {
		snprintf(aviso, sizeof(aviso), "%s: %s", ruta, zip_texto_error(r));
		serie("comprim: no se abre %s: %s\n", ruta, zip_texto_error(r));
		haz_filas();
		return r;
	}
	abierto = 1;
	if (ruta != ruta_zip)
		snprintf(ruta_zip, sizeof(ruta_zip), "%s", ruta);
	serie("comprim: abierto %s (%u archivos, %u peligrosos, %u con clave)\n", ruta_zip, z.n, z.peligrosos, z.cifrados);
	{
		const char *b = strrchr(ruta_zip, '\\');

		snprintf(t, sizeof(t), "%s - Comprimidos", b ? b + 1 : ruta_zip);
		ui_titulo(t);
	}
	haz_filas();
	if (z.peligrosos)
		snprintf(aviso, sizeof(aviso), "Ojo: %u nombres peligrosos (con .. o rutas enteras): se ven en rojo y no se sacan.", z.peligrosos);
	return 0;
}

static void entra(int k)
{
	size_t l = strlen(dentro);

	if (k < 0 || k >= n_filas || !filas[k].carpeta)
		return;
	if (l + strlen(filas[k].nombre) + 2 >= sizeof(dentro))
		return;
	strcat(dentro, filas[k].nombre);
	strcat(dentro, "/");
	cursor = primera = 0;
	haz_filas();
}

static void sube(void)
{
	size_t l = strlen(dentro);
	char antes[ZIP_MAX_NOMBRE];
	int i;

	if (!l)
		return;
	dentro[l - 1] = 0;
	{
		char *b = strrchr(dentro, '/');

		strcpy(antes, b ? b + 1 : dentro);
		if (b)
			b[1] = 0;
		else
			dentro[0] = 0;
	}
	haz_filas();
	for (i = 0; i < n_filas; i++)
		if (filas[i].carpeta && !strcmp(filas[i].nombre, antes))
			cursor = i;
	if (cursor < primera || cursor >= primera + filas_visibles())
		primera = cursor;
}

/* la carpeta en la que está el ZIP (con su «\» al final) */
static void carpeta_del_zip(char *t, int cabe)
{
	char *b;

	snprintf(t, (size_t)cabe, "%s", ruta_zip);
	b = strrchr(t, '\\');
	if (b)
		b[1] = 0;
	else if (t[1] == ':')
		t[2] = 0;
	else
		t[0] = 0;
}

/* «C:\FOTOS\VIAJE.ZIP» -> «C:\FOTOS\VIAJE» */
static void carpeta_de_sacar(const char *zip, char *t, int cabe)
{
	char *p;

	snprintf(t, (size_t)cabe, "%s", zip);
	p = strrchr(t, '.');
	if (p && p > strrchr(t, '\\'))
		*p = 0;
	else
		strcat(t, ".SAL");
}

static int lanza_suelto(const char *prog, const char *args)
{
	uint32_t r = sc(SC_LANZAR_SUELTO, (uint32_t)prog, (uint32_t)args, 0);

	return ES_ERROR(r) ? -1 : 0;
}

/* la contraseña, si hace falta (la de antes, si la hay) */
static const char *clave_para(const uint8_t *marca, int otra_vez)
{
	if (!zip_pide_clave(&z, marca))
		return NULL;
	if (con_clave && !otra_vez)
		return clave;
	return pl_pide_clave(ruta_zip);
}

/* saca lo marcado a «destino»: da el error (0, bien) y deja la cuenta */
static int saca(const uint8_t *marca, const char *destino, const char *quita, zcuenta *c, int reemplazar)
{
	const char *k = NULL;
	int r, vez;

	for (vez = 0; vez < 3; vez++) {
		if (zip_pide_clave(&z, marca)) {
			k = clave_para(marca, vez > 0);
			if (!k)
				return ZIP_E_CLAVE;
		}
		cancelado = 0;
		haciendo = "Sacando";
		r = zip_sacar_todo(&z, destino, marca, quita, k, reemplazar, c);
		if (r != ZIP_E_CLAVE || c->sacados)
			break;
		con_clave = 0;
		ui_aviso("Contrase\xA4" "a", "No es esa contrase\xA4" "a. Pruebe otra vez.");
	}
	serie("comprim: sacados %u a %s (%u ya estaban, %u fallos: %s)\n", c->sacados, destino, c->saltados, c->fallos, zip_texto_error(r));
	return r;
}

static void cuenta_en_aviso(const zcuenta *c, int r, const char *destino)
{
	char u[200];

	if (r == ZIP_E_CORTADO)
		snprintf(aviso, sizeof(aviso), "Parado: se han sacado %u.", c->sacados);
	else if (c->fallos) {
		utf8_a_437(c->ultimo, u, sizeof(u));
		snprintf(aviso, sizeof(aviso), "Sacados %u a %s; %u no (%s: %s).", c->sacados, destino, c->fallos, u, zip_texto_error(c->ultimo_error));
	} else if (r)
		snprintf(aviso, sizeof(aviso), "No se ha sacado: %s.", zip_texto_error(r));
	else
		snprintf(aviso, sizeof(aviso), "Sacados %u a %s%s.", c->sacados, destino, c->saltados ? " (los que ya estaban, sin tocar)" : "");
}

static void extraer(int todo)
{
	char destino[ZIP_MAX_RUTA], t[ZIP_MAX_RUTA + 200];
	uint8_t *marca = NULL;
	zcuenta c;
	int r, rep = 0, q;

	if (!abierto)
		return;
	if (!todo && !(marca = marca_elegidos(0)))
		return;
	carpeta_de_sacar(ruta_zip, destino, sizeof(destino));
	if (!ui_pide_texto(todo ? "Extraer todo" : "Extraer lo elegido", "La carpeta en la que dejarlo (si no est\xA0, se hace):", destino, sizeof(destino), 0)) {
		free(marca);
		return;
	}
	if (pl_que_es(destino) == 2) {
		snprintf(t, sizeof(t), "La carpeta %s ya est\xA0. Si hay archivos con los mismos nombres, \xA8se reemplazan?", destino);
		q = ui_pregunta("Extraer", t, "No reemplazar", "Reemplazar", "Cancelar");
		if (q < 0 || q == 2) {
			free(marca);
			return;
		}
		rep = q == 1;
	}
	r = saca(marca, destino, todo ? "" : dentro, &c, rep);
	cuenta_en_aviso(&c, r, destino);
	free(marca);
	if (c.sacados && r != ZIP_E_CORTADO) {
		pinta();
		snprintf(t, sizeof(t), "%s\n\n\xA8" "Abrir la carpeta?", aviso);
		if (ui_pregunta("Extraer", t, "Abrirla", "No", NULL) == 0)
			lanza_suelto("\\EQUIPO.BIN", destino);
	}
}

/* abre uno (lo saca a una carpeta de paso y lo abre con lo suyo) */
static void abre_uno(int k)
{
	char tmp[ZIP_MAX_RUTA], t[ZIP_MAX_RUTA + 10], pref[ZIP_MAX_NOMBRE];
	uint8_t *marca;
	zcuenta c;
	int r;

	if (k < 0 || k >= n_filas)
		return;
	if (filas[k].carpeta) {
		entra(k);
		return;
	}
	marca = marca_elegidos(1);
	if (!marca)
		return;
	/* la carpeta de paso: \COMPRIM.TMP en la unidad del ZIP */
	tmp[0] = ruta_zip[1] == ':' ? ruta_zip[0] : 'C';
	tmp[1] = 0;
	strcat(tmp, ":\\COMPRIM.TMP");
	snprintf(pref, sizeof(pref), "%s", z.m[filas[k].miembro].nombre);
	{
		char *b = strrchr(pref, '/');

		if (b)
			b[1] = 0;
		else
			pref[0] = 0;
	}
	r = saca(marca, tmp, pref, &c, 1);
	free(marca);
	if (r || !c.sacados) {
		cuenta_en_aviso(&c, r, tmp);
		return;
	}
	snprintf(t, sizeof(t), "/ABRIR %s", c.ultimo_local);
	serie("comprim: abro %s\n", c.ultimo_local);
	if (lanza_suelto("\\EQUIPO.BIN", t))
		snprintf(aviso, sizeof(aviso), "Sacado a %s (no se ha podido abrir).", c.ultimo_local);
	else
		snprintf(aviso, sizeof(aviso), "Abierto %s (una copia: los cambios no vuelven al ZIP).", c.ultimo_local);
}

/* rehace el ZIP (añadir o quitar) y lo vuelve a abrir */
static int rehace(const uint8_t *quitar, zfuentes *nuevos, const char *que)
{
	char tmp[ZIP_MAX_RUTA];
	int r;

	pl_temporal(ruta_zip, tmp, sizeof(tmp));
	zip_excluir[0] = ruta_zip;
	zip_excluir[1] = tmp;
	cancelado = 0;
	haciendo = que;
	r = zip_hacer(ruta_zip, tmp, &z, quitar, nuevos, 6);
	zip_excluir[0] = zip_excluir[1] = NULL;
	serie("comprim: %s: %s\n", que, zip_texto_error(r));
	abre_zip(ruta_zip);
	return r;
}

static void anadir(void)
{
	static char ruta[ZIP_MAX_RUTA];
	zfuentes l;
	uint32_t i;
	int r;

	if (!abierto)
		return;
	if (!ui_pide_texto("A\xA4" "adir al ZIP", "La ruta del archivo o de la carpeta que se mete (por ejemplo, C:\\CARTAS):", ruta, sizeof(ruta), 0) || !ruta[0])
		return;
	memset(&l, 0, sizeof(l));
	zip_excluir[0] = ruta_zip;
	r = zip_juntar(&l, ruta, NULL);
	zip_excluir[0] = NULL;
	if (r) {
		snprintf(aviso, sizeof(aviso), "%s: %s", ruta, zip_texto_error(r));
		zip_suelta_fuentes(&l);
		return;
	}
	/* va a la carpeta del ZIP en la que se está */
	for (i = 0; dentro[0] && i < l.n; i++) {
		char *n = malloc(strlen(dentro) + strlen(l.f[i].nombre) + 1);

		if (!n)
			break;
		strcpy(n, dentro);
		strcat(n, l.f[i].nombre);
		free(l.f[i].nombre);
		l.f[i].nombre = n;
	}
	r = rehace(NULL, &l, "A\xA4" "adiendo");
	if (!r)
		snprintf(aviso, sizeof(aviso), "A\xA4" "adidos %u.", l.n);
	else
		snprintf(aviso, sizeof(aviso), "No se ha podido a\xA4" "adir: %s.", zip_texto_error(r));
	zip_suelta_fuentes(&l);
}

static void quitar(void)
{
	uint8_t *marca;
	char t[200];
	uint32_t i, n = 0;
	int r;

	if (!abierto || !cuantos_elegidos())
		return;
	marca = marca_elegidos(0);
	if (!marca)
		return;
	for (i = 0; i < z.n; i++)
		n += marca[i];
	snprintf(t, sizeof(t), "\xA8Quitar del ZIP lo elegido (%u entre archivos y carpetas)? Lo de fuera del ZIP no se toca.", n);
	if (ui_pregunta("Quitar", t, "Quitar", "Cancelar", NULL) == 0) {
		r = rehace(marca, NULL, "Quitando");
		if (r)
			snprintf(aviso, sizeof(aviso), "No se ha podido quitar: %s.", zip_texto_error(r));
		else
			snprintf(aviso, sizeof(aviso), "Quitados %u.", n);
	}
	free(marca);
}

static void abrir_otro(void)
{
	static char ruta[ZIP_MAX_RUTA];

	if (!ui_pide_texto("Abrir", "La ruta del .ZIP (por ejemplo, C:\\FOTOS\\VIAJE.ZIP):", ruta, sizeof(ruta), 0) || !ruta[0])
		return;
	dentro[0] = 0;
	cursor = primera = 0;
	aviso[0] = 0;
	abre_zip(ruta);
}

static void nuevo(void)
{
	static char ruta[ZIP_MAX_RUTA];
	char tmp[ZIP_MAX_RUTA];
	int r;

	if (!ui_pide_texto("Nuevo ZIP", "La ruta del ZIP nuevo (por ejemplo, C:\\CARTAS.ZIP):", ruta, sizeof(ruta), 0) || !ruta[0])
		return;
	if (pl_que_es(ruta)) {
		ui_aviso("Nuevo ZIP", "Ya hay algo con ese nombre.");
		return;
	}
	pl_temporal(ruta, tmp, sizeof(tmp));
	r = zip_hacer(ruta, tmp, NULL, NULL, NULL, 6);
	if (r) {
		snprintf(aviso, sizeof(aviso), "No se ha podido hacer %s: %s", ruta, zip_texto_error(r));
		return;
	}
	dentro[0] = 0;
	aviso[0] = 0;
	abre_zip(ruta);
}

static void propiedades(int k)
{
	char t[900], a[24], b[24], n[300];
	zmiembro *m;

	if (k < 0 || k >= n_filas)
		return;
	utf8_a_437(filas[k].nombre, n, sizeof(n));
	if (filas[k].miembro < 0 || filas[k].carpeta) {
		tamano(filas[k].tam, a);
		snprintf(t, sizeof(t), "%s\nUna carpeta del ZIP, con %u archivos dentro (%s).", n, filas[k].dentro_n, a);
	} else {
		m = &z.m[filas[k].miembro];
		tamano(m->tam, a);
		tamano(m->comp, b);
		utf8_a_437(m->nombre, n, sizeof(n));
		snprintf(t, sizeof(t), "%s\nMide %s; comprimido, %s (%u %%). Guardado %s%s.\nCRC-32: %08X. %s%s",
			 n, a, b, m->tam ? (uint32_t)(100 - m->comp * 100 / (m->tam ? m->tam : 1)) : 0,
			 (m->cifrado == ZC_AES ? m->metodo_real : m->metodo) == 8 ? "con deflate" : (m->cifrado == ZC_AES ? m->metodo_real : m->metodo) == 0 ? "tal cual" : "de otra forma",
			 m->cifrado == ZC_AES ? (m->aes_fuerza == 3 ? ", cifrado con AES-256" : m->aes_fuerza == 2 ? ", cifrado con AES-192" : ", cifrado con AES-128") :
			 m->cifrado == ZC_ZIPCRYPTO ? ", con contrase\xA4" "a (ZipCrypto)" : "",
			 m->crc, m->peligro ? "\nNombre PELIGROSO: no se saca. " : "", m->tam >= 0xFFFFFFFFu || z.zip64 ? "ZIP64." : "");
	}
	ui_aviso("Propiedades", t);
}

/* arrastrado fuera de la ventana: se saca al lado del ZIP y, si es en el
 * tapiz, el escritorio le pone su acceso (la llamada 171) */
static void suelta_fuera(int x, int y)
{
	char destino[ZIP_MAX_RUTA];
	uint8_t *marca = marca_elegidos(0);
	struct suelta s;
	zcuenta c;
	int r;

	if (!marca)
		return;
	carpeta_del_zip(destino, sizeof(destino));
	if (!destino[0])
		strcpy(destino, ".");
	r = saca(marca, destino, dentro, &c, 0);
	free(marca);
	cuenta_en_aviso(&c, r, destino);
	if (c.primero_local[0]) {
		s.x = x;
		s.y = y;
		s.ruta = c.primero_local;
		sc(SC_SOLTAR_FUERA, ui_v, (uint32_t)&s, 0);
	}
}

/* ----------------------------------------------------- la ventana */
static int pulsado = -1, arrastrando, px, py;

static void elige_solo(int k)
{
	int i;

	for (i = 0; i < n_filas; i++)
		filas[i].elegido = i == k;
}

static void al_cursor(void)
{
	int vis = filas_visibles();

	if (cursor < 0)
		cursor = 0;
	if (cursor >= n_filas)
		cursor = n_filas - 1;
	if (cursor < 0)
		cursor = 0;
	if (cursor < primera)
		primera = cursor;
	if (cursor >= primera + vis)
		primera = cursor - vis + 1;
	if (primera > n_filas - vis)
		primera = n_filas - vis;
	if (primera < 0)
		primera = 0;
}

static void boton(int id)
{
	aviso[0] = 0;
	switch (id) {
	case B_EXTRAER_TODO: extraer(1); break;
	case B_EXTRAER: extraer(0); break;
	case B_ANADIR: anadir(); break;
	case B_QUITAR: quitar(); break;
	case B_SUBIR: sube(); break;
	case B_ABRIR: abrir_otro(); break;
	case B_NUEVO: nuevo(); break;
	}
}

#define M_FILA 1
#define M_FONDO 2

static void tecla(uint32_t k, uint32_t b)
{
	int vis = filas_visibles(), i;

	aviso[0] = 0;
	switch (k) {
	case T_ARRIBA: cursor--; break;
	case T_ABAJO: cursor++; break;
	case T_RE_PAG: cursor -= vis; break;
	case T_AV_PAG: cursor += vis; break;
	case T_INICIO: cursor = 0; break;
	case T_FIN: cursor = n_filas - 1; break;
	case T_INTRO:
	case 13: abre_uno(cursor); return;
	case T_BORRAR: sube(); return;
	case T_SUPR: quitar(); return;
	case ' ':
		if (cursor < n_filas)
			filas[cursor].elegido ^= 1;
		return;
	case 1:		/* Ctrl+A */
		for (i = 0; i < n_filas; i++)
			filas[i].elegido = 1;
		return;
	case T_F5:
		if (abierto)
			abre_zip(ruta_zip);
		return;
	default:
		return;
	}
	al_cursor();
	if (b & 1) {
		/* con Mayús: de la ancla al cursor */
		if (ancla < 0)
			ancla = cursor;
		for (i = 0; i < n_filas; i++)
			filas[i].elegido = (i >= ancla && i <= cursor) || (i <= ancla && i >= cursor);
	} else {
		ancla = cursor;
		elige_solo(cursor);
	}
}

static int fila_en(int x, int y)
{
	int k;

	if (y < ARRIBA_LISTA || x < 8 || x >= ui_an - 8)
		return -1;
	k = primera + (y - ARRIBA_LISTA) / ALTO_FILA;
	if (k >= n_filas || (y - ARRIBA_LISTA) / ALTO_FILA >= filas_visibles())
		return -1;
	return k;
}

static void raton_baja(int x, int y, uint32_t c)
{
	int id = ui_en_zona(x, y), k, i;

	if (id) {
		boton(id);
		return;
	}
	aviso[0] = 0;
	k = fila_en(x, y);
	if (c & BOTON_DERECHO) {
		if (k >= 0) {
			if (!filas[k].elegido)
				elige_solo(k);
			cursor = k;
			ui_menu(x, y, "Abrir|Extraer...|-|Quitar del ZIP|-|Propiedades", 0, M_FILA);
		} else
			ui_menu(x, y, "Extraer todo...|A\xA4" "adir...|-|Nuevo ZIP...|Abrir otro ZIP...", abierto ? 0 : 3, M_FONDO);
		return;
	}
	if (k < 0) {
		if (y >= ARRIBA_LISTA && y < ui_al - ALTO_ESTADO)
			elige_solo(-1);
		return;
	}
	if ((c >> 16) >= 2) {
		abre_uno(k);
		return;
	}
	if (c & CON_CONTROL)
		filas[k].elegido ^= 1;
	else if (c & CON_MAYUS) {
		if (ancla < 0)
			ancla = cursor;
		for (i = 0; i < n_filas; i++)
			filas[i].elegido = (i >= ancla && i <= k) || (i <= ancla && i >= k);
	} else {
		if (!filas[k].elegido)
			elige_solo(k);
		ancla = k;
	}
	cursor = k;
	pulsado = k;
	px = x;
	py = y;
}

static void menu_elegido(uint32_t etiqueta, uint32_t op)
{
	if (op == SIN_ELEGIR)
		return;
	if (etiqueta == M_FILA) {
		switch (op) {
		case 0: abre_uno(cursor); break;
		case 1: extraer(0); break;
		case 3: quitar(); break;
		case 5: propiedades(cursor); break;
		}
	} else {
		switch (op) {
		case 0: extraer(1); break;
		case 1: anadir(); break;
		case 3: nuevo(); break;
		case 4: abrir_otro(); break;
		}
	}
}

static int ventana(const char *ruta)
{
	struct suceso s;
	uint32_t alto = sc(SC_DATO, DATO_ALTO_LIBRE, 0, 0), ancho = sc(SC_DATO, DATO_ANCHO_PANTALLA, 0, 0);
	int an = 720, al = 440, pintar = 1;	/* (PARPADEO) */

	if (ancho && ancho < 760)
		an = (int)ancho - 40;
	if (alto && alto < 480)
		al = (int)alto - 40;
	con_ventana = 1;
	if (ui_ventana("Comprimidos", an, al)) {
		pl_escribe("comprim: no hay escritorio (o no hay memoria para la ventana)\r\n");
		return 1;
	}
	ui_al_repintar(pinta);
	sc(SC_COLOCAR, ui_v, 0xFFFFFFFFu, 0);	/* en el centro */
	if (ruta && ruta[0])
		abre_zip(ruta);
	for (;;) {
		if (pintar)
			pinta();
		pintar = 1;
		if (!ui_suceso(&s, 1))
			break;
		switch (s.clase) {
		case EV_CERRAR:
			if (abierto)
				zip_cerrar(&z);
			return 0;
		case EV_TAMANO:
			ui_an = (int)s.a;
			ui_al = (int)s.b;
			al_cursor();
			break;
		case EV_TECLA:
			if (s.a == T_ESC && !aviso[0])
				break;
			tecla(s.a, s.b);
			break;
		case EV_RATON_BAJA:
			raton_baja((int)(int32_t)s.a, (int)(int32_t)s.b, s.c);
			break;
		case EV_RATON_MUEVE:
			if (pulsado >= 0 && (s.c & BOTON_IZQUIERDO) && !arrastrando &&
			    (abs((int)(int32_t)s.a - px) > 6 || abs((int)(int32_t)s.b - py) > 6)) {
				arrastrando = 1;
				snprintf(aviso, sizeof(aviso), "Su\x82ltelo en el escritorio para sacarlo al lado del ZIP.");
			}
			if (!arrastrando) {
				pintar = 0;	/* (PARPADEO) el ratón que solo pasa no cambia nada: no se repinta */
				continue;
			}
			break;
		case EV_RATON_SUBE:
			if (arrastrando) {
				int x = (int)(int32_t)s.a, y = (int)(int32_t)s.b;

				arrastrando = 0;
				aviso[0] = 0;
				if (x < 0 || y < 0 || x >= ui_an || y >= ui_al)
					suelta_fuera(x, y);
			}
			pulsado = -1;
			break;
		case EV_RUEDA: {
			int pasos = (int)(int8_t)(s.c & 0xFF);

			primera -= pasos * 3;
			if (primera > n_filas - filas_visibles())
				primera = n_filas - filas_visibles();
			if (primera < 0)
				primera = 0;
			break;
		}
		case EV_MENU:
			menu_elegido(s.b, s.a);
			break;
		default:
			pintar = 0;	/* (PARPADEO) */
			continue;
		}
	}
	return 0;
}

/* ---------------------------------------- desde el explorador */
/* el nombre del ZIP para lo de la lista: el del primero, con .ZIP (sin pisar nada) */
static void nombre_para(const char *primero, char *t, int cabe)
{
	char base[ZIP_MAX_RUTA], *p, *b;
	int k;

	snprintf(base, sizeof(base), "%s", primero);
	k = (int)strlen(base);
	while (k > 3 && base[k - 1] == '\\')
		base[--k] = 0;
	b = strrchr(base, '\\');
	p = strrchr(base, '.');
	if (p && (!b || p > b))
		*p = 0;
	b = strrchr(base, '\\');
	b = b ? b + 1 : base;
	if (strlen(b) > 8)
		b[8] = 0;
	if (base[1] == ':' && !base[2])
		strcat(base, "\\DISCO");
	snprintf(t, (size_t)cabe, "%s.ZIP", base);
	for (k = 2; k < 100 && pl_que_es(t); k++) {
		char *c = strrchr(base, '\\');

		c = c ? c + 1 : base;
		if (strlen(c) > (k < 10 ? 7u : 6u))
			c[k < 10 ? 7 : 6] = 0;
		snprintf(t, (size_t)cabe, "%s%d.ZIP", base, k);
	}
}

static int lee_lista(const char *lista, char ***rutas)
{
	int h = pl_abrir(lista, PL_LEER), n = 0;
	uint64_t l;
	char *t, *p, **r;

	*rutas = NULL;
	if (h < 0)
		return -1;
	l = pl_largo(h);
	if (l > 256 * 1024) {
		pl_cerrar(h);
		return -1;
	}
	t = malloc((size_t)l + 1);
	r = malloc(((size_t)l / 3 + 2) * sizeof(char *));
	if (!t || !r || pl_leer(h, t, (uint32_t)l) != (int32_t)l) {
		pl_cerrar(h);
		return -1;
	}
	pl_cerrar(h);
	t[l] = 0;
	for (p = t; *p;) {
		char *f = p;

		while (*p && *p != '\r' && *p != '\n')
			p++;
		if (*p)
			*p++ = 0;
		while (*p == '\r' || *p == '\n')
			p++;
		if (*f)
			r[n++] = f;
	}
	*rutas = r;
	return n;
}

static int comprimir(const char *lista)
{
	char **rutas, destino[ZIP_MAX_RUTA], tmp[ZIP_MAX_RUTA], t[ZIP_MAX_RUTA + 80];
	int n = lee_lista(lista, &rutas), i, r = 0;
	zfuentes l;

	pl_borrar(lista);
	if (n <= 0) {
		serie("comprim: no hay lista que comprimir (%s)\n", lista);
		return 1;
	}
	nombre_para(rutas[0], destino, sizeof(destino));
	pl_temporal(destino, tmp, sizeof(tmp));
	con_ventana = 1;
	if (ui_ventana("Comprimiendo - Comprimidos", 480, 120))
		return 1;
	sc(SC_COLOCAR, ui_v, 0xFFFFFFFFu, 0);
	ui_al_repintar(pinta);
	ui_rect(0, 0, ui_an, ui_al, C_FONDO);
	snprintf(t, sizeof(t), "Haciendo %s", destino);
	ui_negrita(12, 16, t, (ui_an - 24) / ANCHO_LETRA, C_TINTA);
	memset(&l, 0, sizeof(l));
	zip_excluir[0] = destino;
	zip_excluir[1] = tmp;
	for (i = 0; i < n && !r; i++)
		r = zip_juntar(&l, rutas[i], NULL);
	if (!r) {
		haciendo = "Comprimiendo";
		r = zip_hacer(destino, tmp, NULL, NULL, &l, 6);
	}
	serie("comprim: %s %s con %u archivos y carpetas: %s\n", r ? "no se ha hecho" : "hecho", destino, l.n, zip_texto_error(r));
	if (r && r != ZIP_E_CORTADO) {
		snprintf(t, sizeof(t), "No se ha podido hacer %s: %s.", destino, zip_texto_error(r));
		ui_aviso("Comprimir en ZIP", t);
	}
	zip_suelta_fuentes(&l);
	return r ? 1 : 0;
}

static int extraer_todo(const char *ruta)
{
	char destino[ZIP_MAX_RUTA], t[ZIP_MAX_RUTA + 100];
	zcuenta c;
	int r;

	con_ventana = 1;
	if (ui_ventana("Extrayendo - Comprimidos", 480, 120))
		return 1;
	sc(SC_COLOCAR, ui_v, 0xFFFFFFFFu, 0);
	ui_al_repintar(pinta);
	r = abre_zip(ruta);
	if (r) {
		snprintf(t, sizeof(t), "%s: %s.", ruta, zip_texto_error(r));
		ui_aviso("Extraer todo", t);
		return 1;
	}
	carpeta_de_sacar(ruta, destino, sizeof(destino));
	ui_rect(0, 0, ui_an, ui_al, C_FONDO);
	snprintf(t, sizeof(t), "Sacando a %s", destino);
	ui_negrita(12, 16, t, (ui_an - 24) / ANCHO_LETRA, C_TINTA);
	r = saca(NULL, destino, "", &c, 0);
	cuenta_en_aviso(&c, r, destino);
	if (c.fallos || (r && r != ZIP_E_CORTADO))
		ui_aviso("Extraer todo", aviso);
	if (c.sacados)
		lanza_suelto("\\EQUIPO.BIN", destino);
	zip_cerrar(&z);
	return r ? 1 : 0;
}

/* la ruta entera, con su letra: el explorador da «\CARPETA\X.ZIP» con la
 * unidad de trabajo ya puesta en la suya; una relativa, desde la carpeta de trabajo */
static void ruta_entera(const char *r, char *t, int cabe)
{
	char c[ZIP_MAX_RUTA];
	uint32_t u, n;

	if (r[0] && r[1] == ':') {
		snprintf(t, (size_t)cabe, "%s", r);
	} else if (r[0] == '\\') {
		u = sc(SC_UNIDAD, 0, 0, 0);
		snprintf(t, (size_t)cabe, "%c:%s", (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') ? (char)u : 'C', r);
	} else {
		n = sc(SC_CARPETA, (uint32_t)c, sizeof(c) - 1, 0);
		if (ES_ERROR(n) || n == 0 || n >= sizeof(c)) {
			snprintf(t, (size_t)cabe, "%s", r);
			return;
		}
		c[n] = 0;
		snprintf(t, (size_t)cabe, "%s%s%s", c, c[n - 1] == '\\' ? "" : "\\", r);
	}
	for (n = 0; t[n]; n++)
		if (t[n] >= 'a' && t[n] <= 'z')
			t[n] = (char)(t[n] - 32);
}

int principal(void)
{
	static char args[600];
	char *argv[64];
	uint32_t n;
	int argc;

	n = sc(SC_ARGUMENTOS, (uint32_t)args, sizeof(args) - 1, 0);
	if (ES_ERROR(n))
		n = 0;
	args[n < sizeof(args) ? n : sizeof(args) - 1] = 0;
	argc = trocea(args, argv, 64);
	if (argc > 0 && (!strcmp(argv[0], "/ZIP") || !strcmp(argv[0], "/zip"))) {
		sc(SC_ATENDER_CORTE, 1, 0, 0);
		return orden_zip(argc - 1, argv + 1);
	}
	if (argc > 0 && (!strcmp(argv[0], "/UNZIP") || !strcmp(argv[0], "/unzip"))) {
		sc(SC_ATENDER_CORTE, 1, 0, 0);
		return orden_unzip(argc - 1, argv + 1);
	}
	if (argc > 1 && !strcmp(argv[0], "/COMPRIMIR"))
		return comprimir(argv[1]);
	if (argc > 1 && !strcmp(argv[0], "/EXTRAER")) {
		ruta_entera(argv[1], ruta_zip, sizeof(ruta_zip));
		return extraer_todo(ruta_zip);
	}
	if (argc > 0)
		ruta_entera(argv[0], ruta_zip, sizeof(ruta_zip));
	return ventana(argc > 0 ? ruta_zip : NULL);
}
