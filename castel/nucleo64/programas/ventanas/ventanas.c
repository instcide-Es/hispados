/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ventanas/ventanas.c - Hispa-64, etapa 8: VENTANAS, el
 * servidor de ventanas y el escritorio (docs/64/HISPA-64.md, sección 14),
 * un programa del anillo 3 con musl:
 *
 *  - tiene la pantalla (/dev/fb0, con su búfer de atrás: compone ahí y
 *    copia a la pantalla solo lo que cambia, con FBIO_HISPA_COPIA) y el
 *    teclado y el puntero (/dev/input/event0, agarrados con EVIOCGRAB);
 *  - las ventanas de los programas (el protocolo de hv.h, por memoria
 *    compartida): con su marco y su título (la activa, oscura y en negrita;
 *    las demás, claras: se distinguen por la luz y la letra, no por el
 *    color), los botones de minimizar, maximizar y cerrar (dibujados: _ □ X),
 *    arrastrarlas por el título, cambiarles el tamaño por la esquina de
 *    abajo a la derecha, el foco al pulsar, el doble clic en el título
 *    (maximizar), Alt+F4, los menús emergentes de los programas y el
 *    portapapeles;
 *  - el escritorio, al estilo del de Hispa-Pentium: el tapiz, la barra de
 *    abajo con el botón «Inicio», un botón por ventana (pulsarlo la trae
 *    delante o la minimiza) y el reloj; el menú Inicio con los programas
 *    que haya (Pluma, Cuentas, Notas) y «Salir del escritorio»; la tecla
 *    del logo o Ctrl+Esc lo abren.
 * Las letras, las DejaVu con suavizado (oficina/comun: letras y lienzo).
 *   VENTANAS [PROGRAMA ...]   (los programas, arrancados al empezar)
 * Lo que pasa lo dice por la salida (el puerto serie): lo miran las pruebas.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "so.h"
#include "texto.h"
#include "lienzo.h"
#include "letras.h"
#include "hv.h"
#include "hdisp.h"

/* (las formas del puntero de so.h, por si la de esta rama aún no las trae) */
#ifndef SO_P_TEXTO
#define SO_P_FLECHA 0
#define SO_P_TEXTO 7
#endif

extern char **environ;

/* ---- las medidas y los colores (la luz distingue lo activo, no el tono) ---- */
#define BORDE 1
#define TITULO 28			/* el alto del título (con el borde de arriba) */
#define BARRA 34
#define BOTON 22
#define ESQUINA 14			/* la esquina para cambiar el tamaño */
#define C_TITULO_ACT 0x1F3F6E
#define C_TITULO_INACT 0xD9DDE3
#define C_BORDE_ACT 0x0E2440
#define C_BORDE_INACT 0x8A929C
#define C_FONDO_VENTANA 0xF4F4F4
#define C_BARRA 0x1E2A36
#define C_INICIO 0x2E7D32
#define C_MENU 0xFAFAFA
#define C_MENU_SEL 0x1F3F6E

static void di(const char *f, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void di(const char *f, ...)
{
	va_list a;

	va_start(a, f);
	fputs("VENTANAS: ", stdout);
	vprintf(f, a);
	fputs("\n", stdout);
	fflush(stdout);
	va_end(a);
}

/* ---- la pantalla ---- */
static int fb = -1, ev = -1;
static u8 *fbm;
static size_t fbl;
static u32 *atras;
static int AN, AL, PASO;
static lienzo L;
static u32 *tapiz;
static hv_control *ctl;

/* ---- las ventanas ---- */
typedef struct {
	int usada, hueco;
	int x, y, an, al;		/* lo de dentro, en la pantalla */
	u32 *px;			/* el búfer del programa */
	int ban, bal;
	size_t largo;
	u32 gen;
	int min, max;
	int rx, ry, ran, ral;		/* para restaurar */
	char titulo[160];
	int pid;
} ven;
static ven W[HV_MAX];
static int orden_[HV_MAX], n_orden;	/* de abajo arriba */
static int foco = -1;

/* ---- el puntero, el teclado ---- */
static int px_ = 0, py_ = 0, nx, ny, botones, botones_nuevos;
static int shift, ctrl, alt, logo;
static int arrastra = -1, ax, ay;	/* una ventana que se mueve: lo que va del puntero a su esquina */
static int estira = -1;			/* una que cambia de tamaño */
static int captura = -1;		/* la que recibe el ratón mientras se pulsa */
static u32 ult_clic_t;
static int ult_clic_x, ult_clic_y, ult_veces, ult_boton;
static int forma_puntero;

/* ---- los menús ---- */
static int inicio_abierto, inicio_sel = -1;
static struct {
	const char *nombre, *ruta;
} progs[] = {
	{ "Pluma (textos)", "/PLUMA" },
	{ "Cuentas (hojas de cálculo)", "/CUENTAS" },
	{ "Notas (bloc de notas)", "/NOTAS" },
};
#define N_PROGS 3
static int prog_hay[N_PROGS];
static int emerg = -1;			/* la ventana del menú emergente abierto */
static int em_x, em_y, em_an, em_n, em_sel = -1;
static char em_op[32][120];
static u32 em_grises, em_etiqueta;
static int saliendo;
static int reloj_min = -1;

/* ---- lo que hay que volver a componer: rectángulos ---- */
#define MAX_DANO 24
static int dano[MAX_DANO][4], n_dano;

static void dana(int x, int y, int an, int al)
{
	int i;

	if (x < 0) { an += x; x = 0; }
	if (y < 0) { al += y; y = 0; }
	if (x + an > AN) an = AN - x;
	if (y + al > AL) al = AL - y;
	if (an <= 0 || al <= 0)
		return;
	/* (junto a otro que lo toque, o todo junto si ya hay muchos) */
	for (i = 0; i < n_dano; i++) {
		int *d = dano[i];

		if (x <= d[0] + d[2] && d[0] <= x + an && y <= d[1] + d[3] && d[1] <= y + al) {
			int x1 = x + an > d[0] + d[2] ? x + an : d[0] + d[2];
			int y1 = y + al > d[1] + d[3] ? y + al : d[1] + d[3];

			d[0] = x < d[0] ? x : d[0];
			d[1] = y < d[1] ? y : d[1];
			d[2] = x1 - d[0];
			d[3] = y1 - d[1];
			return;
		}
	}
	if (n_dano == MAX_DANO) {
		n_dano = 1;
		dano[0][0] = 0;
		dano[0][1] = 0;
		dano[0][2] = AN;
		dano[0][3] = AL;
		return;
	}
	dano[n_dano][0] = x;
	dano[n_dano][1] = y;
	dano[n_dano][2] = an;
	dano[n_dano][3] = al;
	n_dano++;
}

static void dana_todo(void)
{
	dana(0, 0, AN, AL);
}

/* el marco entero de la ventana (con el título y el borde) */
static void marco(ven *w, int *x, int *y, int *an, int *al)
{
	*x = w->x - BORDE;
	*y = w->y - TITULO;
	*an = w->an + 2 * BORDE;
	*al = w->al + TITULO + BORDE;
}

static void dana_ventana(int i)
{
	int x, y, an, al;

	marco(&W[i], &x, &y, &an, &al);
	dana(x, y, an, al);
}

/* ---- los sucesos a un programa ---- */
static void manda(int i, hv_suceso s)
{
	hv_ventana *h;

	if (i < 0 || !W[i].usada)
		return;
	h = &ctl->v[W[i].hueco];
	if (h->ev_pon - h->ev_toma >= HV_COLA)
		return;
	h->ev[h->ev_pon % HV_COLA] = s;
	__sync_synchronize();
	h->ev_pon++;
	__sync_fetch_and_add(&h->ev_aviso, 1);
	hv_despierta(&h->ev_aviso);
}

static void manda_clase(int i, int clase, int a, int b)
{
	hv_suceso s;

	memset(&s, 0, sizeof s);
	s.clase = clase;
	s.x = a;
	s.y = b;
	s.foco = a;
	manda(i, s);
}

static void pon_foco(int i)
{
	if (i == foco)
		return;
	if (foco >= 0 && W[foco].usada) {
		manda_clase(foco, SO_FOCO, 0, 0);
		dana_ventana(foco);
	}
	foco = i;
	if (i >= 0) {
		manda_clase(i, SO_FOCO, 1, 0);
		dana_ventana(i);
	}
	dana(0, AL - BARRA, AN, BARRA);
}

static void arriba_del_todo(int i)
{
	int k, j = -1;

	for (k = 0; k < n_orden; k++)
		if (orden_[k] == i)
			j = k;
	if (j < 0)
		return;
	for (k = j; k < n_orden - 1; k++)
		orden_[k] = orden_[k + 1];
	orden_[n_orden - 1] = i;
	dana_ventana(i);
}

/* ---- las ventanas: nuevas, cambios, fuera ---- */
static int mapea_bufer(ven *w, hv_ventana *h)
{
	int fd;
	size_t l;
	u32 *p;

	fd = shm_open(h->buf, O_RDONLY, 0);
	if (fd < 0)
		return 0;
	l = (size_t)h->an * h->al * 4;
	p = mmap(0, l, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (p == MAP_FAILED)
		return 0;
	if (w->px)
		munmap(w->px, w->largo);
	w->px = p;
	w->largo = l;
	w->ban = h->an;
	w->bal = h->al;
	w->gen = h->gen;
	return 1;
}

static void maximiza(int i, int si)
{
	ven *w = &W[i];

	dana_ventana(i);
	if (si && !w->max) {
		w->rx = w->x;
		w->ry = w->y;
		w->ran = w->an;
		w->ral = w->al;
		w->x = BORDE;
		w->y = TITULO;
		w->an = AN - 2 * BORDE;
		w->al = AL - BARRA - TITULO - BORDE;
		w->max = 1;
	} else if (!si && w->max) {
		w->x = w->rx;
		w->y = w->ry;
		w->an = w->ran;
		w->al = w->ral;
		w->max = 0;
	}
	dana_ventana(i);
	if (w->an != w->ban || w->al != w->bal)
		manda_clase(i, SO_TAMANO, w->an, w->al);
}

static void nueva(int k)
{
	hv_ventana *h = &ctl->v[k];
	int i, n = 0;
	ven *w;

	for (i = 0; i < HV_MAX && W[i].usada; i++)
		;
	if (i >= HV_MAX)
		return;
	w = &W[i];
	memset(w, 0, sizeof *w);
	if (!mapea_bufer(w, h)) {
		di("no se puede poner el búfer de la ventana «%s» (%s)", h->titulo, h->buf);
		h->estado = HV_LIBRE;
		return;
	}
	w->usada = 1;
	w->hueco = k;
	w->pid = h->pid;
	w->an = h->an;
	w->al = h->al;
	copia(w->titulo, h->titulo, sizeof w->titulo);
	for (int j = 0; j < HV_MAX; j++)
		if (W[j].usada)
			n++;
	/* (en cascada) */
	w->x = 40 + 28 * ((n - 1) % 8);
	w->y = 20 + TITULO + 28 * ((n - 1) % 8);
	if (w->x + w->an + BORDE > AN)
		w->x = AN - w->an - BORDE;
	if (w->y + w->al + BORDE > AL - BARRA)
		w->y = AL - BARRA - w->al - BORDE;
	if (w->x < BORDE)
		w->x = BORDE;
	if (w->y < TITULO)
		w->y = TITULO;
	orden_[n_orden++] = i;
	h->estado = HV_VIVA;
	di("ventana nueva «%s» del proceso %d: %dx%d", w->titulo, w->pid, w->an, w->al);
	pon_foco(i);
	dana_ventana(i);
}

static void quita(int i, const char *por)
{
	int k, j;
	hv_ventana *h = &ctl->v[W[i].hueco];

	dana_ventana(i);
	di("ventana fuera «%s» (%s)", W[i].titulo, por);
	if (W[i].px)
		munmap(W[i].px, W[i].largo);
	if (h->buf[0])
		shm_unlink(h->buf);
	W[i].usada = 0;
	W[i].px = NULL;
	for (k = j = 0; k < n_orden; k++)
		if (orden_[k] != i)
			orden_[j++] = orden_[k];
	n_orden = j;
	if (captura == i) captura = -1;
	if (arrastra == i) arrastra = -1;
	if (estira == i) estira = -1;
	if (emerg == i) emerg = -1;
	if (foco == i) {
		foco = -1;
		for (k = n_orden - 1; k >= 0; k--)
			if (!W[orden_[k]].min) {
				pon_foco(orden_[k]);
				break;
			}
	}
	memset((void *)h, 0, sizeof *h);
	h->estado = HV_LIBRE;
	dana(0, AL - BARRA, AN, BARRA);
}

static void abre_menu(int i)
{
	hv_ventana *h = &ctl->v[W[i].hueco];
	const char *s = h->menu;
	int n = 0, an = 0;

	while (*s && n < 32) {
		int k = 0;

		while (*s && *s != '|' && k < 119)
			em_op[n][k++] = *s++;
		em_op[n][k] = 0;
		if (*s == '|')
			s++;
		if (lz_ui_ancho(em_op[n]) > an)
			an = lz_ui_ancho(em_op[n]);
		n++;
	}
	if (emerg >= 0)
		dana(em_x, em_y, em_an, em_n * 26 + 8);
	emerg = i;
	em_n = n;
	em_an = an + 40;
	em_x = W[i].x + h->menu_x;
	em_y = W[i].y + h->menu_y;
	em_grises = h->menu_grises;
	em_etiqueta = h->menu_etiqueta;
	em_sel = -1;
	if (em_x + em_an > AN)
		em_x = AN - em_an;
	if (em_y + em_n * 26 + 8 > AL - BARRA)
		em_y = AL - BARRA - em_n * 26 - 8;
	if (em_x < 0) em_x = 0;
	if (em_y < 0) em_y = 0;
	dana(em_x, em_y, em_an, em_n * 26 + 8);
}

static void cierra_emerg(int opcion)
{
	hv_suceso s;

	if (emerg < 0)
		return;
	memset(&s, 0, sizeof s);
	s.clase = SO_MENU;
	s.opcion = opcion;
	s.etiqueta = em_etiqueta;
	manda(emerg, s);
	dana(em_x, em_y, em_an, em_n * 26 + 8);
	emerg = -1;
}

/* lo que piden los programas */
static void mira_huecos(void)
{
	int k, i;
	static u32 vuelta;

	vuelta++;
	for (k = 0; k < HV_MAX; k++) {
		hv_ventana *h = &ctl->v[k];

		if (h->estado == HV_LISTA)
			nueva(k);
	}
	for (i = 0; i < HV_MAX; i++) {
		ven *w = &W[i];
		hv_ventana *h;
		u32 p;

		if (!w->usada)
			continue;
		h = &ctl->v[w->hueco];
		p = __sync_lock_test_and_set(&h->pide, 0);
		if (p & HV_P_CIERRA) {
			quita(i, "la cierra el programa");
			continue;
		}
		if (p & HV_P_TITULO) {
			copia(w->titulo, h->titulo, sizeof w->titulo);
			dana(w->x, w->y - TITULO, w->an, TITULO);
			dana(0, AL - BARRA, AN, BARRA);
		}
		if ((p & HV_P_BUFER) || h->gen != w->gen) {
			mapea_bufer(w, h);
			dana_ventana(i);
		}
		if (p & HV_P_MAXIMIZA)
			maximiza(i, 1);
		if (p & HV_P_CENTRA) {
			dana_ventana(i);
			w->x = (AN - w->an) / 2;
			w->y = (AL - BARRA - w->al + TITULO) / 2;
			if (w->y < TITULO)
				w->y = TITULO;
			dana_ventana(i);
		}
		if (p & HV_P_SUCIO) {
			int x0, y0, x1, y1;

			hv_toma(&h->s_cerrojo);
			x0 = h->sx0; y0 = h->sy0; x1 = h->sx1; y1 = h->sy1;
			h->sx0 = h->sy0 = h->sx1 = h->sy1 = 0;
			hv_suelta(&h->s_cerrojo);
			if (!w->min && x1 > x0 && y1 > y0)
				dana(w->x + x0, w->y + y0, x1 - x0, y1 - y0);
		}
		if (p & HV_P_MENU)
			abre_menu(i);
		/* (cada segundo y pico: ¿sigue vivo el programa?) */
		if ((vuelta & 63) == (u32)i && kill(w->pid, 0) && errno == ESRCH)
			quita(i, "el programa ha acabado");
	}
}

/* ---- dibujar ---- */
/* una raya suavizada de 2 puntos (lz_linea va en dieciseisavos) */
static void raya_g(int x0, int y0, int x1, int y1, u32 c)
{
	lz_linea(&L, x0 * 16 + 8, y0 * 16 + 8, x1 * 16 + 8, y1 * 16 + 8, 32, c);
}

/* los botones del título: minimizar, maximizar, cerrar (de derecha a izquierda: cerrar el último) */
static void boton_titulo(int x, int y, int cual, u32 c)
{
	int cx = x + BOTON / 2, cy = y + 13;

	if (cual == 2) {	/* cerrar: una X */
		raya_g(cx - 5, cy - 5, cx + 5, cy + 5, c);
		raya_g(cx - 5, cy + 5, cx + 5, cy - 5, c);
	} else if (cual == 1) {	/* maximizar: un cuadro */
		lz_marco(&L, cx - 6, cy - 5, 12, 11, c);
		lz_rect(&L, cx - 6, cy - 5, 12, 2, c);
	} else {		/* minimizar: una raya abajo */
		lz_rect(&L, cx - 6, cy + 4, 12, 2, c);
	}
}

static void pinta_ventana(int i)
{
	ven *w = &W[i];
	int act = i == foco, x, y, an, al, f, tx;
	u32 ct = act ? C_TITULO_ACT : C_TITULO_INACT, cl = act ? 0xFFFFFF : 0x30343A;

	marco(w, &x, &y, &an, &al);
	/* el marco y el título */
	lz_rect(&L, x, y, an, TITULO, ct);
	lz_marco(&L, x, y, an, al, act ? C_BORDE_ACT : C_BORDE_INACT);
	/* (la activa lleva además una raya clara debajo del título: se ve sin colores) */
	if (act)
		lz_rect(&L, x + 1, y + TITULO - 3, an - 2, 2, 0x9CC2F0);
	tx = x + 10;
	{
		lienzo t = L;
		int r0 = x + an - 3 * BOTON - 10;

		if (t.rx1 > r0) t.rx1 = r0;
		if (act)
			lz_ui_negrita(&t, tx, y + 19, w->titulo, cl);
		else
			lz_ui(&t, tx, y + 19, w->titulo, cl);
	}
	for (f = 0; f < 3; f++)
		boton_titulo(x + an - (3 - f) * BOTON - 4, y + 1, f, cl);
	/* lo de dentro: el búfer del programa */
	if (w->px) {
		int van = w->ban < w->an ? w->ban : w->an, val = w->bal < w->al ? w->bal : w->al;
		int y0 = w->y > L.ry0 ? w->y : L.ry0, y1 = w->y + val < L.ry1 ? w->y + val : L.ry1;
		int x0 = w->x > L.rx0 ? w->x : L.rx0, x1 = w->x + van < L.rx1 ? w->x + van : L.rx1;
		int fy;

		for (fy = y0; fy < y1 && x1 > x0; fy++)
			memcpy(atras + (size_t)fy * PASO + x0, w->px + (size_t)(fy - w->y) * w->ban + (x0 - w->x), (size_t)(x1 - x0) * 4);
		if (van < w->an)
			lz_rect(&L, w->x + van, w->y, w->an - van, w->al, C_FONDO_VENTANA);
		if (val < w->al)
			lz_rect(&L, w->x, w->y + val, van, w->al - val, C_FONDO_VENTANA);
	} else {
		lz_rect(&L, w->x, w->y, w->an, w->al, C_FONDO_VENTANA);
	}
	/* la esquina de cambiar el tamaño: tres rayitas */
	if (!w->max)
		for (f = 0; f < 3; f++)
			lz_linea(&L, (w->x + w->an - 3 - f * 4) * 16, (w->y + w->al - 2) * 16, (w->x + w->an - 2) * 16, (w->y + w->al - 3 - f * 4) * 16, 16, 0x80868E);
}

static void pinta_barra(void)
{
	int y = AL - BARRA, x, k, an;
	char t[64];
	time_t s = time(NULL);
	struct tm m;

	lz_rect(&L, 0, y, AN, BARRA, C_BARRA);
	lz_raya_h(&L, 0, y, AN, 0x4A5866);
	/* Inicio: verde, redondeado, con su texto (y abierto, hundido: con un marco claro) */
	lz_redondeado(&L, 4, y + 4, 92, BARRA - 8, 8, C_INICIO);
	if (inicio_abierto)
		lz_marco_redondeado(&L, 4, y + 4, 92, BARRA - 8, 8, 0xFFFFFF);
	lz_rect(&L, 14, y + 11, 12, 12, 0xFFFFFF);	/* (el disquete de la casa, sencillo) */
	lz_rect(&L, 17, y + 11, 6, 4, C_INICIO);
	lz_ui_negrita(&L, 32, y + 22, "Inicio", 0xFFFFFF);
	/* un botón por ventana */
	x = 108;
	for (k = 0; k < HV_MAX; k++) {
		ven *w = &W[k];
		lienzo b;

		if (!w->usada)
			continue;
		an = 170;
		if (x + an > AN - 90)
			break;
		lz_redondeado(&L, x, y + 4, an, BARRA - 8, 5, k == foco ? 0x46586B : 0x2C3946);
		if (k == foco)
			lz_rect(&L, x + 6, y + BARRA - 7, an - 12, 3, 0xFFFFFF);	/* (la activa: subrayada) */
		b = L;
		if (b.rx1 > x + an - 6) b.rx1 = x + an - 6;
		if (w->min)
			lz_ui(&b, x + 8, y + 22, "(", 0xC8D0D8), lz_ui(&b, x + 8 + lz_ui_ancho("("), y + 22, w->titulo, 0xC8D0D8);
		else
			lz_ui(&b, x + 8, y + 22, w->titulo, 0xFFFFFF);
		x += an + 6;
	}
	/* el reloj */
	gmtime_r(&s, &m);
	fmt(t, sizeof t, "%02d:%02d", m.tm_hour, m.tm_min);
	lz_ui_negrita(&L, AN - 12 - lz_ui_ancho_negrita(t), y + 22, t, 0xFFFFFF);
}

static int inicio_y(void) { return AL - BARRA - 8 - 40 * (N_PROGS + 1) - 48; }

static void pinta_inicio(void)
{
	int x = 4, y = inicio_y(), an = 300, al = AL - BARRA - y, k, n = 0;

	lz_rect(&L, x, y, an, al, C_MENU);
	lz_marco(&L, x, y, an, al, 0x5A6470);
	lz_rect(&L, x + 1, y + 1, an - 2, 40, C_INICIO);
	lz_ui_negrita(&L, x + 14, y + 27, "Hispa-64", 0xFFFFFF);
	y += 48;
	for (k = 0; k <= N_PROGS; k++) {
		const char *t;
		int hay = k == N_PROGS ? 1 : prog_hay[k];

		if (k < N_PROGS && !hay)
			continue;
		t = k == N_PROGS ? "Salir del escritorio" : progs[k].nombre;
		if (k == N_PROGS)
			lz_raya_h(&L, x + 8, y - 4, an - 16, 0xB0B6BE);
		if (inicio_sel == k) {
			lz_rect(&L, x + 4, y, an - 8, 36, C_MENU_SEL);
			lz_rect(&L, x + 4, y, 4, 36, 0xFFFFFF);
			lz_ui_negrita(&L, x + 18, y + 23, t, 0xFFFFFF);
		} else {
			lz_ui(&L, x + 18, y + 23, t, 0x1A1E24);
		}
		y += 40;
		n++;
	}
}

/* qué opción del menú Inicio hay en (x, y) (-1 ninguna) */
static int en_inicio(int x, int y)
{
	int yy = inicio_y() + 48, k;

	if (x < 4 || x >= 304)
		return -1;
	for (k = 0; k <= N_PROGS; k++) {
		if (k < N_PROGS && !prog_hay[k])
			continue;
		if (y >= yy && y < yy + 36)
			return k;
		yy += 40;
	}
	return -1;
}

static void pinta_emerg(void)
{
	int k, y = em_y + 4;

	lz_rect(&L, em_x, em_y, em_an, em_n * 26 + 8, C_MENU);
	lz_marco(&L, em_x, em_y, em_an, em_n * 26 + 8, 0x5A6470);
	for (k = 0; k < em_n; k++) {
		int gris = (em_grises >> k) & 1;

		if (!strcmp(em_op[k], "-")) {
			lz_raya_h(&L, em_x + 6, y + 13, em_an - 12, 0xB0B6BE);
		} else if (em_sel == k && !gris) {
			lz_rect(&L, em_x + 3, y, em_an - 6, 26, C_MENU_SEL);
			lz_ui_negrita(&L, em_x + 16, y + 18, em_op[k], 0xFFFFFF);
		} else {
			lz_ui(&L, em_x + 16, y + 18, em_op[k], gris ? 0xA0A4AA : 0x1A1E24);
		}
		y += 26;
	}
}

/* el puntero: la flecha (negra con borde blanco) o la I del texto */
static const char *flecha[] = {
	"X", "XX", "X.X", "X..X", "X...X", "X....X", "X.....X", "X......X", "X.......X",
	"X........X", "X.....XXXXX", "X..X..X", "X.X X..X", "XX  X..X", "X    X..X", "     X..X", "      XX", 0
};

static void pinta_puntero(void)
{
	int f, c;

	if (forma_puntero == SO_P_TEXTO) {
		lz_rect(&L, px_ - 1, py_ - 8, 3, 17, 0xFFFFFF);
		lz_rect(&L, px_ - 4, py_ - 9, 9, 3, 0xFFFFFF);
		lz_rect(&L, px_ - 4, py_ + 7, 9, 3, 0xFFFFFF);
		lz_rect(&L, px_, py_ - 7, 1, 15, 0);
		lz_rect(&L, px_ - 3, py_ - 8, 7, 1, 0);
		lz_rect(&L, px_ - 3, py_ + 8, 7, 1, 0);
		return;
	}
	for (f = 0; flecha[f]; f++)
		for (c = 0; flecha[f][c]; c++) {
			if (flecha[f][c] == 'X')
				lz_punto(&L, px_ + c, py_ + f, 0x000000);
			else if (flecha[f][c] == '.')
				lz_punto(&L, px_ + c, py_ + f, 0xFFFFFF);
		}
}

static void compone(int x, int y, int an, int al)
{
	int k, f;
	u32 r[4];

	lz_recorte(&L, x, y, an, al);
	for (f = y; f < y + al; f++)
		memcpy(atras + (size_t)f * PASO + x, tapiz + (size_t)f * AN + x, (size_t)an * 4);
	for (k = 0; k < n_orden; k++) {
		ven *w = &W[orden_[k]];
		int mx, my, man, mal;

		if (w->min)
			continue;
		marco(w, &mx, &my, &man, &mal);
		if (mx >= x + an || my >= y + al || mx + man <= x || my + mal <= y)
			continue;
		pinta_ventana(orden_[k]);
	}
	if (y + al > AL - BARRA)
		pinta_barra();
	if (inicio_abierto)
		pinta_inicio();
	if (emerg >= 0)
		pinta_emerg();
	pinta_puntero();
	lz_sin_recorte(&L);
	r[0] = (u32)x;
	r[1] = (u32)y;
	r[2] = (u32)an;
	r[3] = (u32)al;
	ioctl(fb, FBIO_HISPA_COPIA, r);
}

static int compone_lo_danado(void)
{
	int i, n = n_dano;

	for (i = 0; i < n; i++)
		compone(dano[i][0], dano[i][1], dano[i][2], dano[i][3]);
	n_dano = 0;
	return n;
}

static void hace_tapiz(void)
{
	int f, c;
	lienzo t;
	int cara;

	for (f = 0; f < AL; f++) {
		u32 a = 0x2B5E91, b = 0x0D2238;
		int k = f * 256 / AL;
		u32 col = (((a >> 16) * (256 - k) + (b >> 16) * k) >> 8) << 16 |
			  (((a >> 8 & 255) * (256 - k) + (b >> 8 & 255) * k) >> 8) << 8 |
			  (((a & 255) * (256 - k) + (b & 255) * k) >> 8);

		for (c = 0; c < AN; c++)
			tapiz[(size_t)f * AN + c] = col;
	}
	t.p = tapiz;
	t.an = AN;
	t.al = AL;
	t.paso = AN;
	lz_sin_recorte(&t);
	/* unas olas suaves */
	for (c = 0; c < AN; c += 2) {
		int yy = AL * 2 / 3 + (int)(28.0 * sin(c / 90.0));

		lz_rect_a(&t, c, yy, 2, AL - BARRA - yy, 0x5C8FC4, 40);
	}
	cara = letra_cara(FAM_SANS, 1, 0, NULL);
	lz_texto(&t, cara, 44 * 4, 0, AN - 40 - lz_ancho(cara, 44 * 4, 0, "Hispa-64"), AL - BARRA - 40, "Hispa-64", 0xB8D0EA);
	cara = letra_cara(FAM_SANS, 0, 0, NULL);
	lz_texto(&t, cara, 13 * 4, 0, AN - 40 - lz_ancho(cara, 13 * 4, 0, "etapa 8: las ventanas y la oficina"), AL - BARRA - 18,
		 "etapa 8: las ventanas y la oficina", 0x9FB8D2);
}

/* ---- el ratón ---- */
/* la ventana de arriba en (x, y), o -1 */
static int ventana_en(int x, int y)
{
	int k;

	for (k = n_orden - 1; k >= 0; k--) {
		ven *w = &W[orden_[k]];
		int mx, my, man, mal;

		if (w->min)
			continue;
		marco(w, &mx, &my, &man, &mal);
		if (x >= mx && y >= my && x < mx + man && y < my + mal)
			return orden_[k];
	}
	return -1;
}

static void manda_raton(int i, int clase, int veces)
{
	hv_suceso s;

	memset(&s, 0, sizeof s);
	s.clase = clase;
	s.x = px_ - W[i].x;
	s.y = py_ - W[i].y;
	s.botones = botones;
	s.veces = veces;
	s.mods = (shift ? SO_CON_MAYUS : 0) | (ctrl ? SO_CON_CONTROL : 0);
	manda(i, s);
}

static void lanza(const char *ruta)
{
	char *argv[2];
	pid_t h;

	argv[0] = (char *)ruta;
	argv[1] = NULL;
	if (posix_spawn(&h, ruta, NULL, NULL, argv, environ))
		di("no se puede arrancar %s", ruta);
	else
		di("arranca %s (proceso %d)", ruta, (int)h);
}

static void cierra_inicio(void)
{
	if (!inicio_abierto)
		return;
	inicio_abierto = 0;
	dana(0, inicio_y(), 310, AL - inicio_y());
}

static void abre_inicio(void)
{
	int k;

	for (k = 0; k < N_PROGS; k++)
		prog_hay[k] = access(progs[k].ruta, X_OK) == 0 || access(progs[k].ruta, F_OK) == 0;
	inicio_abierto = 1;
	inicio_sel = -1;
	dana(0, inicio_y(), 310, AL - inicio_y());
	di("menú Inicio abierto");
}

static void elige_inicio(int k)
{
	cierra_inicio();
	if (k == N_PROGS) {
		saliendo = 1;
		di("salir del escritorio");
	} else if (k >= 0) {
		lanza(progs[k].ruta);
	}
}

/* el botón de la barra de la ventana k: dónde */
static int boton_barra_en(int x)
{
	int k, bx = 108;

	for (k = 0; k < HV_MAX; k++) {
		if (!W[k].usada)
			continue;
		if (bx + 170 > AN - 90)
			break;
		if (x >= bx && x < bx + 170)
			return k;
		bx += 176;
	}
	return -1;
}

static void pulsa(int b)
{
	int i, x = px_, y = py_;
	u32 t = so_tics();

	/* (las veces: dos o tres pulsaciones seguidas en el mismo sitio) */
	if (b == ult_boton && t - ult_clic_t < 450 && abs(x - ult_clic_x) < 5 && abs(y - ult_clic_y) < 5 && ult_veces < 3)
		ult_veces++;
	else
		ult_veces = 1;
	ult_boton = b;
	ult_clic_t = t;
	ult_clic_x = x;
	ult_clic_y = y;
	if (emerg >= 0) {
		if (x >= em_x && x < em_x + em_an && y >= em_y && y < em_y + em_n * 26 + 8) {
			int k = (y - em_y - 4) / 26;

			if (k >= 0 && k < em_n && strcmp(em_op[k], "-") && !((em_grises >> k) & 1))
				cierra_emerg(k);
			return;
		}
		cierra_emerg(-1);
	}
	if (inicio_abierto) {
		if (x < 304 && y >= inicio_y() && y < AL - BARRA) {
			elige_inicio(en_inicio(x, y));
			return;
		}
		cierra_inicio();
		if (y >= AL - BARRA && x < 100)
			return;
	}
	if (y >= AL - BARRA) {
		if (x < 100) {
			abre_inicio();
			return;
		}
		i = boton_barra_en(x);
		if (i >= 0) {
			if (W[i].min) {
				W[i].min = 0;
				arriba_del_todo(i);
				pon_foco(i);
			} else if (i == foco) {
				W[i].min = 1;
				dana_ventana(i);
				pon_foco(-1);
			} else {
				arriba_del_todo(i);
				pon_foco(i);
			}
			dana(0, AL - BARRA, AN, BARRA);
		}
		return;
	}
	i = ventana_en(x, y);
	if (i < 0) {
		pon_foco(-1);
		return;
	}
	arriba_del_todo(i);
	pon_foco(i);
	if (y < W[i].y) {
		int mx, my, man, mal;

		marco(&W[i], &mx, &my, &man, &mal);
		/* los botones del título */
		if (x >= mx + man - 3 * BOTON - 4) {
			int f = (x - (mx + man - 3 * BOTON - 4)) / BOTON;

			if (f >= 2) {
				di("cerrar «%s»", W[i].titulo);
				manda_clase(i, SO_CERRAR, 0, 0);
			} else if (f == 1) {
				maximiza(i, !W[i].max);
			} else {
				W[i].min = 1;
				dana_ventana(i);
				pon_foco(-1);
			}
			return;
		}
		if (ult_veces == 2 && b == 1) {
			maximiza(i, !W[i].max);
			return;
		}
		if (b == 1 && !W[i].max) {
			arrastra = i;
			ax = x - W[i].x;
			ay = y - W[i].y;
		}
		return;
	}
	if (b == 1 && !W[i].max && x >= W[i].x + W[i].an - ESQUINA && y >= W[i].y + W[i].al - ESQUINA) {
		estira = i;
		return;
	}
	if (x >= W[i].x && x < W[i].x + W[i].an && y < W[i].y + W[i].al) {
		captura = i;
		manda_raton(i, SO_BAJA, ult_veces);
	}
}

static void suelta_boton(void)
{
	if (arrastra >= 0)
		arrastra = -1;
	if (estira >= 0) {
		ven *w = &W[estira];

		if (w->an != w->ban || w->al != w->bal)
			manda_clase(estira, SO_TAMANO, w->an, w->al);
		estira = -1;
	}
	if (captura >= 0) {
		manda_raton(captura, SO_SUBE, ult_veces);
		if (!botones)
			captura = -1;
	}
}

static int forma_en(int i, int x, int y)
{
	hv_ventana *h = &ctl->v[W[i].hueco];
	int k, f = h->forma;

	x -= W[i].x;
	y -= W[i].y;
	for (k = 0; k < h->n_zonas && k < 16; k++)
		if (x >= h->zona[k][0] && y >= h->zona[k][1] && x < h->zona[k][0] + h->zona[k][2] && y < h->zona[k][1] + h->zona[k][3])
			f = h->zona[k][4];
	return f;
}

static void mueve(void)
{
	int i, ox = px_, oy = py_, f = SO_P_FLECHA;

	px_ = nx < 0 ? 0 : nx >= AN ? AN - 1 : nx;
	py_ = ny < 0 ? 0 : ny >= AL ? AL - 1 : ny;
	if (px_ == ox && py_ == oy)
		return;
	dana(ox - 10, oy - 10, 24, 28);
	dana(px_ - 10, py_ - 10, 24, 28);
	if (arrastra >= 0) {
		ven *w = &W[arrastra];

		dana_ventana(arrastra);
		w->x = px_ - ax;
		w->y = py_ - ay;
		if (w->y < TITULO) w->y = TITULO;
		if (w->y > AL - BARRA - 8) w->y = AL - BARRA - 8;
		dana_ventana(arrastra);
		return;
	}
	if (estira >= 0) {
		ven *w = &W[estira];

		dana_ventana(estira);
		w->an = px_ - w->x + 4;
		w->al = py_ - w->y + 4;
		if (w->an < 160) w->an = 160;
		if (w->al < 100) w->al = 100;
		dana_ventana(estira);
		return;
	}
	if (inicio_abierto) {
		int s = en_inicio(px_, py_);

		if (s != inicio_sel) {
			inicio_sel = s;
			dana(0, inicio_y(), 310, AL - inicio_y());
		}
	}
	if (emerg >= 0) {
		int s = -1;

		if (px_ >= em_x && px_ < em_x + em_an && py_ >= em_y + 4 && py_ < em_y + 4 + em_n * 26)
			s = (py_ - em_y - 4) / 26;
		if (s != em_sel) {
			em_sel = s;
			dana(em_x, em_y, em_an, em_n * 26 + 8);
		}
	}
	i = captura >= 0 ? captura : ventana_en(px_, py_);
	if (i >= 0 && (captura >= 0 || (px_ >= W[i].x && py_ >= W[i].y && px_ < W[i].x + W[i].an && py_ < W[i].y + W[i].al))) {
		manda_raton(i, SO_MUEVE, 1);
		f = forma_en(i, px_, py_);
	}
	forma_puntero = f;
}

/* ---- el teclado ---- */
static void manda_tecla(u32 t, int mods)
{
	hv_suceso s;

	if (foco < 0)
		return;
	memset(&s, 0, sizeof s);
	s.clase = SO_TECLA;
	s.tecla = t;
	s.mods = mods | (shift ? SO_CON_MAYUS : 0);
	manda(foco, s);
}

static void tecla(int codigo, int valor)
{
	int abajo = valor != 0;
	u32 t = 0;

	switch (codigo) {
	case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT: shift = abajo; return;
	case KEY_LEFTCTRL: case KEY_RIGHTCTRL: ctrl = abajo; ctl->control = (u32)abajo; return;
	case KEY_LEFTALT: alt = abajo; return;
	case KEY_RIGHTALT: return;
	case KEY_LEFTMETA: case KEY_RIGHTMETA:
		if (valor == 1)
			logo = 1;
		if (valor == 0 && logo) {
			logo = 0;
			if (inicio_abierto) cierra_inicio(); else abre_inicio();
		}
		return;
	}
	logo = 0;
	if (!abajo)
		return;
	switch (codigo) {
	case KEY_UP: t = SO_T_ARRIBA; break;
	case KEY_DOWN: t = SO_T_ABAJO; break;
	case KEY_LEFT: t = SO_T_IZQUIERDA; break;
	case KEY_RIGHT: t = SO_T_DERECHA; break;
	case KEY_HOME: t = SO_T_INICIO; break;
	case KEY_END: t = SO_T_FIN; break;
	case KEY_PAGEUP: t = SO_T_RE_PAG; break;
	case KEY_PAGEDOWN: t = SO_T_AV_PAG; break;
	case KEY_DELETE: t = SO_T_SUPR; break;
	case KEY_INSERT: t = SO_T_INSERTAR; break;
	case KEY_F11: t = SO_T_F11; break;
	case KEY_F12: t = SO_T_F12; break;
	default:
		if (codigo >= KEY_F1 && codigo <= KEY_F10)
			t = SO_T_F1 + (codigo - KEY_F1);
	}
	if (!t)
		return;
	if (t == SO_T_F4 && alt) {
		if (foco >= 0) {
			di("Alt+F4: cerrar «%s»", W[foco].titulo);
			manda_clase(foco, SO_CERRAR, 0, 0);
		}
		return;
	}
	manda_tecla(t, ctrl ? SO_CON_CONTROL : 0);
}

static void letra(u32 u)
{
	if (inicio_abierto && u == 27) {
		cierra_inicio();
		return;
	}
	if (u == 27 && ctrl) {
		abre_inicio();
		return;
	}
	if (emerg >= 0 && u == 27) {
		cierra_emerg(-1);
		return;
	}
	if (u == 13) {
		manda_tecla(SO_INTRO, 0);
	} else if (u == 127 || (u == 8 && !ctrl)) {
		manda_tecla(SO_RETROCESO, 0);
	} else if (u == 9) {
		if (ctrl)
			manda_tecla(SO_T_CTRL_TAB, 0);
		else if (shift)
			manda_tecla(SO_T_MAYUS_TAB, 0);
		else
			manda_tecla(SO_TAB, 0);
	} else if (u == 27) {
		manda_tecla(SO_ESC, 0);
	} else if (u >= 1 && u <= 26) {
		manda_tecla(u, SO_CON_CONTROL);
	} else if (alt && ((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z'))) {
		manda_tecla(SO_T_ALT + ((u | 32) - 'a'), 0);
	} else if (u >= 32) {
		manda_tecla(u, 0);
	}
}

static void rueda(int pasos)
{
	int i = ventana_en(px_, py_);
	hv_suceso s;

	if (i < 0 || py_ < W[i].y)
		return;
	memset(&s, 0, sizeof s);
	s.clase = SO_RUEDA;
	s.x = px_ - W[i].x;
	s.y = py_ - W[i].y;
	s.pasos = pasos;
	s.mods = (shift ? SO_CON_MAYUS : 0) | (ctrl ? SO_CON_CONTROL : 0);
	manda(i, s);
}

/* lee lo que haya en /dev/input/event0 (sin esperar); cuántos */
static int lee_sucesos(void)
{
	struct input_event e[64];
	int n, k, total = 0;

	while ((n = (int)read(ev, e, sizeof e)) > 0) {
		n /= (int)sizeof e[0];
		for (k = 0; k < n; k++) {
			switch (e[k].type) {
			case EV_ABS:
				if (e[k].code == ABS_X) nx = e[k].value;
				else if (e[k].code == ABS_Y) ny = e[k].value;
				break;
			case EV_KEY:
				if (e[k].code >= BTN_LEFT && e[k].code <= BTN_MIDDLE) {
					int bit = e[k].code == BTN_LEFT ? 1 : e[k].code == BTN_RIGHT ? 2 : 4;

					if (e[k].value) botones_nuevos |= bit; else botones_nuevos &= ~bit;
				} else {
					tecla(e[k].code, e[k].value);
				}
				break;
			case EV_REL:
				if (e[k].code == REL_WHEEL)
					rueda(e[k].value);
				break;
			case EV_LETRA:
				letra((u32)e[k].value);
				break;
			case EV_SYN:
				mueve();
				if (botones_nuevos != botones) {
					int antes = botones, b;

					botones = botones_nuevos;
					for (b = 1; b <= 4; b <<= 1) {
						if ((botones & b) && !(antes & b))
							pulsa(b);
						else if (!(botones & b) && (antes & b))
							suelta_boton();
					}
				}
				break;
			}
		}
		total += n;
	}
	return total;
}

static void al_acabar(int s)
{
	(void)s;
	saliendo = 1;
}

int ofi_principal(const char *args)
{
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	int fd, k;
	char a[1024], *s;

	letras_inicia();
	fb = open("/dev/fb0", O_RDWR);
	if (fb < 0) {
		di("no hay pantalla gráfica (/dev/fb0: %s)", strerror(errno));
		return 1;
	}
	if (ioctl(fb, FBIOGET_VSCREENINFO, &v) || ioctl(fb, FBIOGET_FSCREENINFO, &f) || v.bits_per_pixel != 32) {
		di("la pantalla no es de 32 bits por punto");
		return 1;
	}
	AN = (int)v.xres;
	AL = (int)v.yres;
	PASO = (int)(f.line_length / 4);
	fbl = f.smem_len;
	fbm = mmap(0, fbl, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
	if (fbm == MAP_FAILED) {
		di("no se puede poner la pantalla en la memoria (%s)", strerror(errno));
		return 1;
	}
	atras = (u32 *)(fbm + fbl / 2);
	L.p = atras;
	L.an = AN;
	L.al = AL;
	L.paso = PASO;
	lz_sin_recorte(&L);
	tapiz = malloc((size_t)AN * AL * 4);
	if (!tapiz)
		return 1;
	ev = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	if (ev < 0 || ioctl(ev, EVIOCGRAB, 1)) {
		di("no se puede tener el teclado y el ratón (/dev/input/event0)");
		return 1;
	}
	/* el sitio de las ventanas */
	shm_unlink(HV_NOMBRE);
	fd = shm_open(HV_NOMBRE, O_CREAT | O_EXCL | O_RDWR, 0666);
	if (fd < 0 || ftruncate(fd, sizeof(hv_control))) {
		di("no se puede hacer %s", HV_NOMBRE);
		return 1;
	}
	ctl = mmap(0, sizeof(hv_control), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (ctl == MAP_FAILED)
		return 1;
	memset(ctl, 0, sizeof *ctl);
	ctl->version = 1;
	ctl->servidor = getpid();
	ctl->pant_an = AN;
	ctl->pant_al = AL;
	ctl->libre_an = AN;
	ctl->libre_al = AL - BARRA;
	__sync_synchronize();
	ctl->magia = HV_MAGIA;
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTERM, al_acabar);
	nx = px_ = AN / 2;
	ny = py_ = AL / 2;
	hace_tapiz();
	dana_todo();
	compone_lo_danado();
	di("en marcha: %dx%d, %d puntos por línea; el protocolo en /dev/shm%s (%d KB)", AN, AL, PASO, HV_NOMBRE,
	   (int)(sizeof(hv_control) / 1024));
	/* los programas de la línea */
	copia(a, args, sizeof a);
	for (s = strtok(a, " "); s; s = strtok(NULL, " ")) {
		char r[300];

		fmt(r, sizeof r, "%s%s", s[0] == '/' ? "" : "/", s);
		lanza(r);
	}
	while (!saliendo) {
		u32 visto = ctl->aviso;
		int hubo = 0;
		time_t t = time(NULL);

		hubo += lee_sucesos();
		mira_huecos();
		if ((int)(t / 60) != reloj_min) {
			reloj_min = (int)(t / 60);
			dana(AN - 90, AL - BARRA, 90, BARRA);
		}
		hubo += compone_lo_danado();
		if (!hubo && visto == ctl->aviso)
			hv_espera(&ctl->aviso, visto, 10);
	}
	/* salir: que cierren los programas (2 s), y luego fuera */
	for (k = 0; k < HV_MAX; k++)
		if (W[k].usada)
			manda_clase(k, SO_CERRAR, 0, 0);
	for (int vuelta = 0; vuelta < 200; vuelta++) {
		int quedan = 0;

		mira_huecos();
		for (k = 0; k < HV_MAX; k++)
			if (W[k].usada) {
				if (kill(W[k].pid, 0) && errno == ESRCH)
					quita(k, "el programa ha acabado");
				else
					quedan++;
			}
		if (!quedan)
			break;
		so_duerme(10);
	}
	for (k = 0; k < HV_MAX; k++)
		if (W[k].usada) {
			kill(W[k].pid, SIGTERM);
			quita(k, "se cierra el escritorio");
		}
	ctl->magia = 0;
	shm_unlink(HV_NOMBRE);
	di("fuera; la pantalla vuelve a la consola");
	return 0;
}
