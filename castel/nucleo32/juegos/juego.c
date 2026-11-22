/* juego.c - (JUEGOS) el marco de los juegos de Hispa-Pentium (juego.h): la
 * ventana y su lienzo, lo sucio que se sube, la pantalla completa, el bucle
 * (el pulso de 100 por segundo, los cuadros y el sonido), las teclas, los
 * menús de la barra, los cuadros que esperan una respuesta y los récords.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdarg.h>
#include "juego.h"

lienzo LZ;
int jg_completa, jg_activa = 1, jg_cambio = 1, jg_salir, jg_mide;
u32 jg_hz = 100;
char jg_args[256];

static u32 v_ventana, v_normal, v_completa;
static int an_normal, al_normal;
static const struct juego *J;
static char titulo437[64];

/* ---------------------------------------------------------------- varios */
void serie(const char *fmt, ...)
{
	char t[400];
	va_list a;
	int n;

	va_start(a, fmt);
	n = vsnprintf(t, sizeof t, fmt, a);
	va_end(a);
	if (n > (int)sizeof t - 1)
		n = (int)sizeof t - 1;
	{
		char b[400];

		n = a437(t, b, sizeof b);
		sc(SC_SERIE, (u32)b, (u32)n, 0);
	}
}

static u32 semilla = 0x2545F491u;
void azar_semilla(u32 s) { semilla = s ? s : 0x2545F491u; }
u32 azar(void)
{
	semilla ^= semilla << 13;
	semilla ^= semilla >> 17;
	semilla ^= semilla << 5;
	return semilla;
}
int azar_n(int n) { return n > 0 ? (int)(azar() % (u32)n) : 0; }

int arg_hay(const char *o)
{
	const char *p = jg_args;
	size_t n = strlen(o);

	while ((p = strchr(p, o[0])) != 0) {
		if (!strncmp(p, o, n) && (p[n] == 0 || p[n] == ' ' || p[n] == '='))
			return 1;
		p++;
	}
	return 0;
}

int arg_entero(const char *o, int si_no)
{
	const char *p = jg_args;
	size_t n = strlen(o);

	while ((p = strchr(p, o[0])) != 0) {
		if (!strncmp(p, o, n)) {
			int v = 0, s = 1;

			p += n;
			if (*p == '-') {
				s = -1;
				p++;
			}
			if (*p < '0' || *p > '9')
				return si_no;
			while (*p >= '0' && *p <= '9')
				v = v * 10 + (*p++ - '0');
			return v * s;
		}
		p++;
	}
	return si_no;
}

u32 jg_tics(void) { return sc(SC_TICS, 0, 0, 0); }

/* ---------------------------------------------------------------- lo sucio */
#define MAX_SUCIOS 24
static struct { int x, y, an, al; } sucio[MAX_SUCIOS];
static int n_sucios, todo_sucio = 1;
static u32 *subida;
static size_t cabe_subida;

void jg_todo_sucio(void) { todo_sucio = 1; n_sucios = 0; jg_cambio = 1; }

void jg_sucio(int x, int y, int an, int al)
{
	int i;

	jg_cambio = 1;
	if (todo_sucio)
		return;
	if (x < 0) { an += x; x = 0; }
	if (y < 0) { al += y; y = 0; }
	if (x + an > LZ.an) an = LZ.an - x;
	if (y + al > LZ.al) al = LZ.al - y;
	if (an <= 0 || al <= 0)
		return;
	/* con uno que lo toque (o casi), se juntan si no sale mucho más grande */
	for (i = 0; i < n_sucios; i++) {
		int x0 = MIN(x, sucio[i].x), y0 = MIN(y, sucio[i].y);
		int x1 = MAX(x + an, sucio[i].x + sucio[i].an), y1 = MAX(y + al, sucio[i].y + sucio[i].al);
		long u = (long)(x1 - x0) * (y1 - y0), s = (long)an * al + (long)sucio[i].an * sucio[i].al;

		if (u <= s + s / 3 + 256) {
			sucio[i].x = x0;
			sucio[i].y = y0;
			sucio[i].an = x1 - x0;
			sucio[i].al = y1 - y0;
			return;
		}
	}
	if (n_sucios == MAX_SUCIOS) {
		todo_sucio = 1;
		return;
	}
	sucio[n_sucios].x = x;
	sucio[n_sucios].y = y;
	sucio[n_sucios].an = an;
	sucio[n_sucios].al = al;
	n_sucios++;
}

static void sube(int x, int y, int an, int al)
{
	static struct orden o;
	int f;

	if (an <= 0 || al <= 0 || !LZ.p)
		return;
	if (an == LZ.an && LZ.paso == LZ.an) {
		o.datos = LZ.p + (size_t)y * LZ.paso;
	} else {
		if ((size_t)an * al > cabe_subida) {
			free(subida);
			cabe_subida = (size_t)an * al;
			subida = (u32 *)malloc(cabe_subida * 4);
			if (!subida) {
				cabe_subida = 0;
				return;
			}
		}
		for (f = 0; f < al; f++)
			memcpy(subida + (size_t)f * an, LZ.p + (size_t)(y + f) * LZ.paso + x, (size_t)an * 4);
		o.datos = subida;
	}
	o.op = OP_MAPA;
	o.x = x;
	o.y = y;
	o.ancho = an;
	o.alto = al;
	sc(SC_DIBUJAR, v_ventana, (u32)&o, 0);
}

void jg_subir(void)
{
	int i;
	long area = 0;

	for (i = 0; i < n_sucios; i++)
		area += (long)sucio[i].an * sucio[i].al;
	if (todo_sucio || area * 10 > (long)LZ.an * LZ.al * 7) {
		/* de una vez (en trozos de 2048 filas, lo que deja la llamada) */
		for (i = 0; i < LZ.al; i += 2048)
			sube(0, i, LZ.an, MIN(2048, LZ.al - i));
	} else
		for (i = 0; i < n_sucios; i++)
			sube(sucio[i].x, sucio[i].y, sucio[i].an, sucio[i].al);
	n_sucios = 0;
	todo_sucio = 0;
}

/* ---------------------------------------------------------------- la ventana */
void jg_tamano_maximo(int *an, int *al)
{
	u32 w = sc(SC_DATO, DATO_ANCHO_PANTALLA, 0, 0), h = sc(SC_DATO, DATO_ALTO_LIBRE, 0, 0);

	if (ES_ERROR(w) || !w)
		w = 800;
	if (ES_ERROR(h) || !h)
		h = 560;
	*an = (int)w - 12;
	*al = (int)h - 40;
}

static void nuevo_lienzo(int an, int al)
{
	lz_libre(&LZ);
	lz_nuevo(&LZ, an, al);
	if (LZ.p)
		lz_rect(&LZ, 0, 0, LZ.an, LZ.al, 0x203040);
	jg_todo_sucio();
}

int jg_abrir(const char *titulo, int an, int al, int marcas)
{
	int man, mal;
	u32 h = sc(SC_DATO, DATO_HZ, 0, 0);

	jg_hz = ES_ERROR(h) || !h ? 100 : h;
	jg_tamano_maximo(&man, &mal);
	if (an > man)
		an = man;
	if (al > mal)
		al = mal;
	a437(titulo, titulo437, sizeof titulo437);
	v_ventana = sc(SC_VENTANA, (u32)titulo437, (u32)an, (u32)al);
	if (ES_ERROR(v_ventana) || !v_ventana)
		return -1;
	v_normal = v_ventana;
	sc(SC_COLOCAR, v_ventana, 0xFFFFFFFFu, 0);
	nuevo_lienzo(an, al);
	if (!LZ.p)
		return -1;
	if (marcas & JV_COMPLETA)
		jg_pantalla_completa(1);
	return 0;
}

void jg_titulo(const char *t)
{
	a437(t, titulo437, sizeof titulo437);
	sc(SC_TITULO, v_normal, (u32)titulo437, 0);
}

void jg_pantalla_completa(int si)
{
	struct pide_ventana pv;
	u32 W = sc(SC_DATO, DATO_ANCHO_PANTALLA, 0, 0), H = sc(SC_DATO, DATO_ALTO_PANTALLA, 0, 0), v;

	if (!si == !jg_completa)
		return;
	if (si) {
		if (ES_ERROR(W) || !W)
			return;
		pv.nombre = titulo437;
		pv.x = 0;
		pv.y = 0;
		pv.ancho = (int32_t)W;
		pv.alto = (int32_t)H;
		pv.marcas = V_COMPLETA | V_ENCIMA | V_SIN_MARCO;
		v = sc(SC_VENTANAS_W32, 0, 0, (u32)&pv);
		if (ES_ERROR(v) || !v)
			return;
		sc(SC_VENTANAS_W32, 3, v, 0);	/* delante, y la activa */
		v_completa = v;
		an_normal = LZ.an;
		al_normal = LZ.al;
		v_ventana = v;
		jg_completa = 1;
		nuevo_lienzo((int)W, (int)H);
		serie("juego: pantalla completa, %u x %u\n", W, H);
	} else {
		sc(SC_CERRAR_VENTANA, v_completa, 0, 0);
		v_completa = 0;
		v_ventana = v_normal;
		jg_completa = 0;
		nuevo_lienzo(an_normal, al_normal);
		serie("juego: fuera de la pantalla completa\n");
	}
	jg_activa = 1;
	teclas_olvidar();
	if (J && J->tamano)
		J->tamano(LZ.an, LZ.al);
	jg_todo_sucio();
}

/* ---------------------------------------------------------------- las teclas */
static u32 pulsadas[8], vinieron[8];

/* la 181 da las pulsadas ahora y, detrás, las de la vez anterior más las
 * que se han pulsado desde entonces: lo nuevo es lo que no estaba pulsado
 * en el pulso anterior (así, la repetición del teclado no cuenta) */
void teclas_mirar(void)
{
	u32 m[16];
	int i;

	if (sc(181, (u32)m, 0, 0) != 1) {
		memset(pulsadas, 0, sizeof pulsadas);
		memset(vinieron, 0, sizeof vinieron);
		return;
	}
	for (i = 0; i < 8; i++) {
		vinieron[i] = (m[i] | m[8 + i]) & ~pulsadas[i];
		pulsadas[i] = m[i];
	}
}

void teclas_olvidar(void)
{
	memset(pulsadas, 0, sizeof pulsadas);
	memset(vinieron, 0, sizeof vinieron);
}

int tecla(int k) { return (pulsadas[(k >> 5) & 7] >> (k & 31)) & 1; }
int tecla_vino(int k) { return (vinieron[(k >> 5) & 7] >> (k & 31)) & 1; }

/* ---------------------------------------------------------------- los menús */
#define MAX_MENUS 4
static char barra437[MAX_MENUS][24];
static char opciones437[MAX_MENUS][640];
static u32 grises[MAX_MENUS];
static int n_menus, menu_x[MAX_MENUS + 1];

void jg_menus(const char *barra, const char *const *opciones)
{
	char b[200];
	int i = 0;
	const char *p = barra;

	n_menus = 0;
	while (*p && n_menus < MAX_MENUS) {
		const char *q = strchr(p, '|');
		int n = q ? (int)(q - p) : (int)strlen(p);

		memcpy(b, p, (size_t)n);
		b[n] = 0;
		strncpy(barra437[n_menus], b, sizeof barra437[0] - 1);
		a437(opciones[n_menus], opciones437[n_menus], sizeof opciones437[0]);
		n_menus++;
		p += n;
		if (*p == '|')
			p++;
	}
	for (i = 0; i <= n_menus; i++)
		menu_x[i] = 0;
}

void jg_menu_grises(int cual, u32 g)
{
	if (cual >= 0 && cual < MAX_MENUS)
		grises[cual] = g;
}

void jg_pinta_barra(void)
{
	int i, x = 6;

	lz_degradado(&LZ, 0, 0, LZ.an, ALTO_BARRA, 0xF8FAFC, 0xE2E8F0);
	lz_rect(&LZ, 0, ALTO_BARRA - 1, LZ.an, 1, 0xB8C2D0);
	for (i = 0; i < n_menus; i++) {
		menu_x[i] = x;
		x += lz_texto(&LZ, x + 8, 5, barra437[i], 0x111827, 1) + 16;
	}
	menu_x[n_menus] = x;
	jg_sucio(0, 0, LZ.an, ALTO_BARRA);
}

static void abre_menu(int i, int x, int y)
{
	static struct pide_menu p;

	p.x = x;
	p.y = y;
	p.opciones = opciones437[i];
	p.inactivas = grises[i];
	p.etiqueta = 0x4A470000u | (u32)i;	/* «JG» y el número */
	sc(SC_MENU, v_ventana, (u32)&p, 0);
}

int jg_clic_barra(int x, int y)
{
	int i;

	if (jg_completa || y < 0 || y >= ALTO_BARRA)
		return 0;
	for (i = 0; i < n_menus; i++)
		if (x >= menu_x[i] && x < menu_x[i + 1]) {
			abre_menu(i, menu_x[i], ALTO_BARRA);
			break;
		}
	return 1;
}

void jg_menu_en(int x, int y, int cual)
{
	if (cual >= 0 && cual < n_menus)
		abre_menu(cual, x, y);
}

int jg_menu_elegido(const struct suceso *s, int *menu)
{
	if (s->clase != EV_MENU || (s->b & 0xFFFF0000u) != 0x4A470000u || s->a == SIN_ELEGIR)
		return -1;
	*menu = (int)(s->b & 0xFF);
	return (int)s->a;
}

/* ---------------------------------------------------------------- los cuadros */
#define C_CAJA 0xE9EDF2
#define C_TINTA 0x111827
#define C_BORDE_CAJA 0x6B7280

static lienzo guardado;

static void guarda_pantalla(void)
{
	lz_nuevo(&guardado, LZ.an, LZ.al);
	if (guardado.p)
		lz_copia(&guardado, 0, 0, &LZ, 0, 0, LZ.an, LZ.al);
}

static void repone_pantalla(void)
{
	if (guardado.p && guardado.an == LZ.an && guardado.al == LZ.al)
		lz_copia(&LZ, 0, 0, &guardado, 0, 0, LZ.an, LZ.al);
	lz_libre(&guardado);
	jg_todo_sucio();
	jg_subir();
	jg_cambio = 1;
}

/* una caja con su título; da dónde empieza lo de dentro */
static void caja(int x, int y, int an, int al, const char *titulo)
{
	lz_mezcla(&LZ, x + 5, y + 5, an, al, 0, 90);
	lz_rect(&LZ, x, y, an, al, C_BORDE_CAJA);
	lz_rect(&LZ, x + 1, y + 1, an - 2, al - 2, C_CAJA);
	lz_degradado(&LZ, x + 1, y + 1, an - 2, 28, 0x2563EB, 0x1D4ED8);
	lz_negrita(&LZ, x + 12, y + 7, titulo, 0xFFFFFF, 1);
	jg_sucio(x, y, an + 6, al + 6);
}

static int boton(int x, int y, int an, const char *t, int principal)
{
	u32 f = principal ? 0x166534 : 0xE5E7EB, c = principal ? 0xFFFFFF : C_TINTA;

	lz_redondo(&LZ, x, y, an, 28, 4, C_BORDE_CAJA);
	lz_redondo(&LZ, x + 1, y + 1, an - 2, 26, 4, f);
	lz_texto_centro(&LZ, x + an / 2, y + 6, t, c, 1, 1);
	return an;
}

/* espera a uno de los botones (x, y, an, de 28 de alto): da su número, o -1 (Esc o cerrar) */
static int espera_botones(const int *bx, int by, const int *ban, int n, int intro)
{
	struct suceso s;

	jg_subir();
	for (;;) {
		if (sc(SC_SUCESO, v_ventana, (u32)&s, 1) != 1) {
			snd_alimentar();
			continue;
		}
		snd_alimentar();
		if (s.clase == EV_CERRAR)
			return -1;
		if (s.clase == EV_TECLA) {
			int k = (int)s.a;

			if (ES_INTRO(k) || k == ' ')
				return intro;
			if (k == T_ESC)
				return -1;
			if (n == 2 && (k == 's' || k == 'S'))
				return 0;
			if (n == 2 && (k == 'n' || k == 'N'))
				return 1;
		}
		if (s.clase == EV_RATON_BAJA) {
			int x = (int)(int32_t)s.a, y = (int)(int32_t)s.b, i;

			for (i = 0; i < n; i++)
				if (x >= bx[i] && x < bx[i] + ban[i] && y >= by && y < by + 28)
					return i;
			if (n == 1)
				return 0;
		}
		if (s.clase == EV_FOCO)
			jg_activa = (int)s.a;
	}
}

static int cuadro(const char *titulo, const char *texto, const char *b1, const char *b2)
{
	int an = MIN(LZ.an - 20, 460), r, x, y, al, bx[2], ban[2], nb = b2 ? 2 : 1;

	if (an < 200)
		an = LZ.an;
	r = lz_parrafo(0, 0, 0, an - 32, texto, 0, 1, 18);
	al = 28 + 16 + r * 18 + 16 + 28 + 14;
	if (al > LZ.al)
		al = LZ.al;
	x = (LZ.an - an) / 2;
	y = (LZ.al - al) / 2;
	guarda_pantalla();
	caja(x, y, an, al, titulo);
	lz_parrafo(&LZ, x + 16, y + 28 + 16, an - 32, texto, C_TINTA, 1, 18);
	ban[0] = MAX(ancho_texto(b1, 1) + 32, 90);
	ban[1] = b2 ? MAX(ancho_texto(b2, 1) + 32, 90) : 0;
	bx[0] = x + an - 16 - ban[0] - (b2 ? ban[1] + 10 : 0);
	bx[1] = x + an - 16 - ban[1];
	boton(bx[0], y + al - 14 - 28, ban[0], b1, 1);
	if (b2)
		boton(bx[1], y + al - 14 - 28, ban[1], b2, 0);
	r = espera_botones(bx, y + al - 14 - 28, ban, nb, 0);
	repone_pantalla();
	teclas_olvidar();
	return r;
}

void jg_aviso(const char *titulo, const char *texto) { cuadro(titulo, texto, "Aceptar", 0); }

int jg_pregunta(const char *titulo, const char *texto, const char *si, const char *no)
{
	return cuadro(titulo, texto, si, no) == 0;
}

int jg_elige(const char *titulo, const char *const *op, int n, int actual)
{
	int an = MIN(LZ.an - 20, 360), al = 28 + 12 + n * 34 + 8, x, y, i, bx[16], ban[16], r;
	struct suceso s;

	if (n > 16)
		n = 16;
	if (al > LZ.al)
		al = LZ.al;
	x = (LZ.an - an) / 2;
	y = (LZ.al - al) / 2;
	guarda_pantalla();
	caja(x, y, an, al, titulo);
	for (i = 0; i < n; i++) {
		bx[i] = x + 16;
		ban[i] = an - 32;
		boton(bx[i], y + 40 + i * 34, ban[i], op[i], i == actual);
	}
	jg_subir();
	r = -2;
	while (r == -2) {
		if (sc(SC_SUCESO, v_ventana, (u32)&s, 1) != 1)
			continue;
		if (s.clase == EV_CERRAR || (s.clase == EV_TECLA && s.a == T_ESC))
			r = -1;
		else if (s.clase == EV_TECLA && ES_INTRO(s.a))
			r = actual;
		else if (s.clase == EV_TECLA && (s.a == T_ARRIBA || s.a == T_ABAJO)) {
			actual = (actual + (s.a == T_ABAJO ? 1 : n - 1)) % n;
			for (i = 0; i < n; i++)
				boton(bx[i], y + 40 + i * 34, ban[i], op[i], i == actual);
			jg_subir();
		} else if (s.clase == EV_RATON_BAJA) {
			int mx = (int)(int32_t)s.a, my = (int)(int32_t)s.b;

			for (i = 0; i < n; i++)
				if (mx >= bx[i] && mx < bx[i] + ban[i] && my >= y + 40 + i * 34 && my < y + 40 + i * 34 + 28)
					r = i;
		}
	}
	repone_pantalla();
	teclas_olvidar();
	return r;
}

void jg_cartel(lienzo *l, int x, int y, int an, int al, const char *titulo, const char *texto)
{
	int r = texto ? lz_parrafo(0, 0, 0, an - 24, texto, 0, 1, 18) : 0, h = 16 * 2 + 18 + r * 18 + 18, cy;

	if (h > al)
		h = al;
	cy = y + (al - h) / 2;
	lz_mezcla(l, x + 4, cy + 4, an, h, 0, 100);
	lz_redondo(l, x, cy, an, h, 8, 0x1E3A5F);
	lz_redondo(l, x + 2, cy + 2, an - 4, h - 4, 7, 0xF8FAFC);
	lz_texto_centro(l, x + an / 2, cy + 12, titulo, 0x1E3A5F, 2, 1);
	if (texto)
		lz_parrafo(l, x + 12, cy + 12 + 32 + 10, an - 24, texto, 0x1F2937, 1, 18);
}

/* ---------------------------------------------------------------- los récords */
#define MAX_CLAVES 80
static struct { char clave[24]; char valor[100]; } claves[MAX_CLAVES];
static int n_claves, rec_sucio;
static char ruta_rec[160];
static char usuario[40];

typedef struct {
	u8 letra, clase, medio, solo;
	u32 total_k, libre_k, del_sistema;
	char nombre[41], etiqueta[12];
	u8 quitable, relleno[2];
} info_unidad;

static char letra_sistema(void)
{
	info_unidad u;
	int i;

	for (i = 0; i < 40; i++) {
		memset(&u, 0, sizeof u);
		if (sc(43, (u32)i, (u32)&u, 0) != 1)
			break;
		if (u.del_sistema && u.letra)
			return (char)u.letra;
	}
	return 'C';
}

const char *jg_usuario(void)
{
	if (!usuario[0]) {
		char b[40];

		b[0] = 0;
		sc(68, 1, (u32)b, sizeof b);
		b[sizeof b - 1] = 0;
		/* (en la página 437: se guarda así; se pinta con lz_texto, que espera UTF-8: se deja ASCII) */
		{
			int i, k = 0;

			for (i = 0; b[i] && k < (int)sizeof usuario - 1; i++)
				if ((unsigned char)b[i] >= 32 && (unsigned char)b[i] < 127 && b[i] != '|')
					usuario[k++] = b[i];
			usuario[k] = 0;
		}
		if (!usuario[0])
			strcpy(usuario, "Jugador");
	}
	return usuario;
}

void jg_fecha(char *d, int cabe)
{
	u32 f = sc(SC_DATO, DATO_FECHA, 0, 0) >> 16;

	snprintf(d, (size_t)cabe, "%02u/%02u/%04u", f & 31, (f >> 5) & 15, 1980 + (f >> 9));
}

/* la carpeta de los ajustes: la del usuario (\USUARIOS\<CARPETA>\AJUSTES, rama
 * «usuarios», la llamada 200) o, sin cuentas, \AJUSTES de la unidad del
 * sistema; dentro, JUEGOS */
static void carpeta_ajustes(char *d, int cabe)
{
	char c[128], t[160];
	char l = letra_sistema();
	u32 r = sc(200, 4, (u32)c, sizeof c);

	if (!ES_ERROR(r) && r > 0 && r < sizeof c) {
		c[r] = 0;
		snprintf(t, sizeof t, "%c:%s\\AJUSTES", l, c);
	} else
		snprintf(t, sizeof t, "%c:\\AJUSTES", l);
	sc(SC_HACER_CARPETA, (u32)t, 0, 0);
	snprintf(d, (size_t)cabe, "%s\\JUEGOS", t);
	sc(SC_HACER_CARPETA, (u32)d, 0, 0);
}

void rec_abrir(const char *nombre)
{
	char d[128], *b, *p;
	u32 m, largo, n;

	carpeta_ajustes(d, sizeof d);
	snprintf(ruta_rec, sizeof ruta_rec, "%s\\%s", d, nombre);
	n_claves = 0;
	m = sc(SC_ABRIR, (u32)ruta_rec, PARA_LEER, 0);
	if (ES_ERROR(m))
		return;
	largo = sc(SC_LARGO, m, 0, 0);
	if (ES_ERROR(largo) || largo > 16384) {
		sc(SC_CERRAR, m, 0, 0);
		return;
	}
	b = (char *)malloc(largo + 1);
	if (!b) {
		sc(SC_CERRAR, m, 0, 0);
		return;
	}
	n = sc(SC_LEER, m, (u32)b, largo);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n))
		n = 0;
	b[n] = 0;
	p = b;
	while (*p && n_claves < MAX_CLAVES) {
		char *fin = p, *igual;

		while (*fin && *fin != '\n' && *fin != '\r')
			fin++;
		igual = p;
		while (igual < fin && *igual != '=')
			igual++;
		if (igual < fin && *p != ';' && igual - p < 24) {
			int nv = (int)(fin - igual - 1);

			memcpy(claves[n_claves].clave, p, (size_t)(igual - p));
			claves[n_claves].clave[igual - p] = 0;
			if (nv > 99)
				nv = 99;
			memcpy(claves[n_claves].valor, igual + 1, (size_t)nv);
			claves[n_claves].valor[nv] = 0;
			n_claves++;
		}
		p = fin;
		while (*p == '\r' || *p == '\n')
			p++;
	}
	free(b);
}

const char *rec_ruta(void) { return ruta_rec; }

const char *rec_texto(const char *clave)
{
	int i;

	for (i = 0; i < n_claves; i++)
		if (!strcmp(claves[i].clave, clave))
			return claves[i].valor;
	return 0;
}

int rec_entero(const char *clave, int si_no)
{
	const char *t = rec_texto(clave);
	int v = 0, s = 1;

	if (!t || !*t)
		return si_no;
	if (*t == '-') {
		s = -1;
		t++;
	}
	while (*t >= '0' && *t <= '9')
		v = v * 10 + (*t++ - '0');
	return v * s;
}

void rec_poner(const char *clave, const char *valor)
{
	int i;

	for (i = 0; i < n_claves; i++)
		if (!strcmp(claves[i].clave, clave))
			break;
	if (i == n_claves) {
		if (n_claves == MAX_CLAVES)
			return;
		strncpy(claves[i].clave, clave, 23);
		claves[i].clave[23] = 0;
		n_claves++;
	} else if (!strcmp(claves[i].valor, valor))
		return;
	strncpy(claves[i].valor, valor, 99);
	claves[i].valor[99] = 0;
	rec_sucio = 1;
}

void rec_poner_entero(const char *clave, int n)
{
	char t[16];

	snprintf(t, sizeof t, "%d", n);
	rec_poner(clave, t);
}

int rec_guardar(void)
{
	static char b[MAX_CLAVES * 130 + 100];
	int n = 0, i;
	u32 m, r;

	if (!rec_sucio || !ruta_rec[0])
		return 0;
	n = snprintf(b, sizeof b, "; %s: lo que se guarda de este juego de Hispa-Pentium\r\n", ruta_rec);
	for (i = 0; i < n_claves; i++)
		n += snprintf(b + n, sizeof b - (size_t)n, "%s=%s\r\n", claves[i].clave, claves[i].valor);
	m = sc(SC_ABRIR, (u32)ruta_rec, PARA_CREAR, 0);
	if (ES_ERROR(m)) {
		serie("juego: no se pueden guardar los récords en %s\n", ruta_rec);
		return 0;
	}
	r = sc(SC_ESCRIBIR_ARCHIVO, m, (u32)b, (u32)n);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(r))
		return 0;
	rec_sucio = 0;
	serie("juego: récords guardados en %s\n", ruta_rec);
	return 1;
}

int rec_tabla(const char *clave, struct record *t, int n)
{
	int i, k = 0;

	for (i = 0; i < n; i++) {
		char c[32];
		const char *v, *a, *b;

		snprintf(c, sizeof c, "%s%d", clave, i + 1);
		v = rec_texto(c);
		if (!v)
			break;
		memset(&t[k], 0, sizeof t[k]);
		t[k].puntos = 0;
		a = v;
		while (*a >= '0' && *a <= '9')
			t[k].puntos = t[k].puntos * 10 + (*a++ - '0');
		if (*a == '|') {
			a++;
			b = strchr(a, '|');
			if (!b)
				b = a + strlen(a);
			memcpy(t[k].nombre, a, (size_t)MIN((int)(b - a), 23));
			if (*b == '|')
				strncpy(t[k].fecha, b + 1, 11);
		}
		k++;
	}
	return k;
}

int rec_meter(const char *clave, int n, int puntos, int menos)
{
	struct record t[REC_N + 1];
	int k = rec_tabla(clave, t, n), i, sitio = k;

	if (n > REC_N)
		n = REC_N;
	if (puntos <= 0)
		return -1;	/* (una partida sin puntos no es un récord) */
	for (i = 0; i < k; i++)
		if (menos ? puntos < t[i].puntos : puntos > t[i].puntos) {
			sitio = i;
			break;
		}
	if (sitio >= n)
		return -1;
	for (i = MIN(k, n - 1); i > sitio; i--)
		t[i] = t[i - 1];
	t[sitio].puntos = puntos;
	strncpy(t[sitio].nombre, jg_usuario(), 23);
	t[sitio].nombre[23] = 0;
	jg_fecha(t[sitio].fecha, sizeof t[sitio].fecha);
	if (k < n)
		k++;
	for (i = 0; i < k; i++) {
		char c[32], v[64];

		snprintf(c, sizeof c, "%s%d", clave, i + 1);
		snprintf(v, sizeof v, "%d|%s|%s", t[i].puntos, t[i].nombre, t[i].fecha);
		rec_poner(c, v);
	}
	rec_guardar();
	return sitio;
}

static void con_puntos(char *d, int n)
{
	/* 12.345.678 */
	char t[16];
	int l, i, k = 0;

	snprintf(t, sizeof t, "%d", n < 0 ? 0 : n);
	l = (int)strlen(t);
	for (i = 0; i < l; i++) {
		d[k++] = t[i];
		if ((l - i - 1) % 3 == 0 && i != l - 1)
			d[k++] = '.';
	}
	d[k] = 0;
}

void rec_ver(const char *titulo, const char *clave, int n, const char *unidad, int menos, int resaltar)
{
	struct record t[REC_N];
	int k = rec_tabla(clave, t, n), i;
	char texto[1400];
	int m = 0;

	(void)menos;
	if (!k)
		m = snprintf(texto, sizeof texto, "Aún no hay ninguno: ¡a jugar!");
	for (i = 0; i < k; i++) {
		char p[20];

		con_puntos(p, t[i].puntos);
		m += snprintf(texto + m, sizeof texto - (size_t)m, "%s%2d. %10s %s  %-12s %s\n", i == resaltar ? "» " : "  ", i + 1, p, unidad, t[i].nombre, t[i].fecha);
	}
	jg_aviso(titulo, texto);
}

/* ---------------------------------------------------------------- el bucle */
int jg_correr(const struct juego *j)
{
	struct suceso s;
	u32 t_ant, ult_cuadro = 0xFFFFFFFFu, cada = 0;
	int cuadros = j->cuadros ? j->cuadros : 60;
	/* lo que gasta (/MIDE) */
	u64 trabajo = 0, desde = rdtsc(), t0, en_pulso = 0, en_pinta = 0, en_subir = 0, q;
	u32 tic_mide = 0, pintados = 0;

	J = j;
	{
		u32 n = sc(SC_ARGUMENTOS, (u32)jg_args, sizeof jg_args - 1, 0), i;

		if (ES_ERROR(n))
			n = 0;
		jg_args[MIN(n, sizeof jg_args - 1)] = 0;
		for (i = 0; jg_args[i]; i++)
			if (jg_args[i] >= 'a' && jg_args[i] <= 'z')
				jg_args[i] = (char)(jg_args[i] - 32);
	}
	jg_mide = arg_hay("/MIDE");
	t_ant = jg_tics();
	tic_mide = t_ant;
	while (!jg_salir) {
		u32 t, n;

		t0 = rdtsc();
		while (sc(SC_SUCESO, v_ventana, (u32)&s, 0) == 1) {
			switch (s.clase) {
			case EV_CERRAR:
				if (!j->al_cerrar || j->al_cerrar())
					jg_salir = 1;
				break;
			case EV_TAMANO:
				if ((int)s.a != LZ.an || (int)s.b != LZ.al) {
					nuevo_lienzo((int)s.a, (int)s.b);
					if (j->tamano)
						j->tamano(LZ.an, LZ.al);
				}
				break;
			case EV_FOCO:
				jg_activa = (int)s.a;
				if (!jg_activa)
					teclas_olvidar();
				if (j->suceso)
					j->suceso(&s);
				break;
			default:
				if (j->suceso)
					j->suceso(&s);
			}
			if (jg_salir)
				break;
		}
		if (jg_salir)
			break;
		t = jg_tics();
		n = t - t_ant;
		if (n > 10)
			n = 10;		/* (si se ha parado mucho, no se corre para recuperarlo) */
		t_ant = t;
		while (n--) {
			teclas_mirar();
			/* F11 y F12 no llegan como sucesos (el teclado solo da F1 a F10): por el mapa */
			if (tecla_vino(K_F11))
				jg_pantalla_completa(!jg_completa);
			if (tecla_vino(K_F12) && j->suceso) {
				struct suceso e;

				e.clase = EV_TECLA;
				e.a = 0x158;	/* F12: lo que hay, por el puerto serie (las pruebas) */
				e.b = e.c = 0;
				j->suceso(&e);
			}
			q = rdtsc();
			if (j->pulso)
				j->pulso();
			en_pulso += rdtsc() - q;
		}
		snd_alimentar();
		cada = t * (u32)cuadros / jg_hz;
		if (cada != ult_cuadro && jg_cambio && j->pinta) {
			ult_cuadro = cada;
			jg_cambio = 0;
			q = rdtsc();
			j->pinta();
			en_pinta += rdtsc() - q;
			q = rdtsc();
			jg_subir();
			en_subir += rdtsc() - q;
			pintados++;
		}
		trabajo += rdtsc() - t0;
		if (jg_mide && t - tic_mide >= 5 * jg_hz) {
			u64 todo = rdtsc() - desde;
			u32 por_mil = todo ? (u32)(trabajo * 1000 / todo) : 0;

			serie("juego: gasta el %u,%u %% del procesador (%u cuadros por segundo; el juego %u,%u %%, pintar %u,%u %%, subirlo a la ventana %u,%u %%)\n",
			      por_mil / 10, por_mil % 10, pintados * jg_hz / (t - tic_mide),
			      (u32)(en_pulso * 1000 / todo) / 10, (u32)(en_pulso * 1000 / todo) % 10, (u32)(en_pinta * 1000 / todo) / 10, (u32)(en_pinta * 1000 / todo) % 10,
			      (u32)(en_subir * 1000 / todo) / 10, (u32)(en_subir * 1000 / todo) % 10);
			trabajo = en_pulso = en_pinta = en_subir = 0;
			desde = rdtsc();
			tic_mide = t;
			pintados = 0;
		}
		sc(SC_DORMIR, 1, 0, 0);
	}
	rec_guardar();
	snd_cerrar();
	return 0;
}
