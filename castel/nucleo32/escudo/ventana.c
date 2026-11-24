/* ventana.c - ESCUDO.BIN: la ventana «Escudo» y sus cuadros. Todo se pinta
 * en un lienzo propio (con la letra de 8x16 del escritorio, la de letra.cas:
 * hacer.sh la saca a letra.h) y se manda entero a la ventana (la llamada 34,
 * OP_MAPA). Se maneja con el ratón y con el teclado (Tab y Mayús+Tab de un
 * control a otro, Intro o Espacio para pulsar, las flechas en las listas,
 * Ctrl+Tab de pestaña, Esc para volver). Nada se dice solo con el color:
 * lo que importa lleva su palabra y su signo ([OK], [!], [i]) y el control
 * con el teclado, un borde grueso (como controles.cas).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "logica.h"
#include "letra.h"

/* los colores (los de controles.cas: buen contraste entre sí) */
#define C_FONDO 0xE9EDF2
#define C_PAPEL 0xFFFFFF
#define C_TINTA 0x111827
#define C_SUAVE 0x404B5A
#define C_AZUL 0x1D4ED8
#define C_AZUL_CLARO 0xDBEAFE
#define C_NARANJA 0xB45309
#define C_NARANJA_CLARO 0xFEF3C7
#define C_VERDE 0x166534
#define C_VERDE_CLARO 0xDCFCE7
#define C_ROJO 0x991B1B
#define C_ROJO_CLARO 0xFEE2E2
#define C_BORDE 0x6B7280
#define C_GRIS 0xD1D5DB
#define C_BARRA 0xF3F4F6

#define AN 640
#define AL 480

/* ------------------------------------------------------------ el lienzo */
typedef struct lienzo { u32 *px; int an, al; } lienzo;
static lienzo L;

static void caja(int x, int y, int w, int h, u32 c)
{
	int i, j;
	if (x < 0)
		w += x, x = 0;
	if (y < 0)
		h += y, y = 0;
	if (x + w > L.an)
		w = L.an - x;
	if (y + h > L.al)
		h = L.al - y;
	for (j = 0; j < h; j++) {
		u32 *p = L.px + (y + j) * L.an + x;
		for (i = 0; i < w; i++)
			p[i] = c;
	}
}

static void borde(int x, int y, int w, int h, int g, u32 c)
{
	caja(x, y, w, g, c);
	caja(x, y + h - g, w, g, c);
	caja(x, y, g, h, c);
	caja(x + w - g, y, g, h, c);
}

/* una letra (de la 437) */
static void letra(int x, int y, u8 c, u32 tinta)
{
	const u8 *g = letra8x16 + c * 16;
	int i, j;
	if (x < 0 || y < 0 || x + 8 > L.an || y + 16 > L.al)
		return;
	for (j = 0; j < 16; j++) {
		u32 *p = L.px + (y + j) * L.an + x;
		u8 f = g[j];
		for (i = 0; i < 8; i++)
			if (f & (0x80 >> i))
				p[i] = tinta;
	}
}

/* texto (UTF-8 o ya en la 437), hasta «max» letras (-1: todas); da lo que mide */
static int texto_n(int x, int y, const char *t, int max, u32 tinta, int negrita)
{
	char b[512];
	int n = (int)a437(b, sizeof b, t), i;
	if (max >= 0 && n > max) {
		n = max;
		if (n >= 3)
			b[n - 1] = b[n - 2] = b[n - 3] = '.';
	}
	for (i = 0; i < n; i++) {
		letra(x + 8 * i, y, (u8)b[i], tinta);
		if (negrita)
			letra(x + 8 * i + 1, y, (u8)b[i], tinta);
	}
	return n * 8 + negrita;
}
static int texto(int x, int y, const char *t, u32 tinta) { return texto_n(x, y, t, -1, tinta, 0); }
static int largo437(const char *t)
{
	char b[512];
	return (int)a437(b, sizeof b, t);
}

/* parte un texto en líneas de «cabe» letras (por palabras); da cuántas */
static int parte(const char *t, int cabe, char lineas[][128], int max)
{
	char b[1500];
	int n = (int)a437(b, sizeof b, t), i = 0, k = 0;
	while (i < n && k < max) {
		int fin = i, ult = -1;
		while (fin < n && b[fin] != '\n' && fin - i < cabe) {
			if (b[fin] == ' ')
				ult = fin;
			fin++;
		}
		if (fin < n && b[fin] != '\n' && ult > i)
			fin = ult;
		if (fin - i > 127)
			fin = i + 127;
		memcpy(lineas[k], b + i, (u32)(fin - i));
		lineas[k][fin - i] = 0;
		k++;
		i = fin;
		if (i < n && (b[i] == ' ' || b[i] == '\n'))
			i++;
	}
	return k;
}

/* el escudo (an x al): verde con su marca si todo va bien; si no, con «!» */
static void icono_escudo(int x, int y, int an, int al, int bien)
{
	int j, i;
	u32 borde_c = 0x14336A, arriba = bien ? 0x3B82F6 : 0xD97706, abajo = bien ? 0x1E40AF : 0x92400E;
	for (j = 0; j < al; j++) {
		int w = j < al * 11 / 20 ? an : an * (al - j) / (al - al * 11 / 20);
		int x0 = x + (an - w) / 2;
		u32 c = j == 0 || j == al - 1 ? borde_c : 0;
		if (!c) {
			u32 t = (u32)j * 255 / (u32)al, r, g, b;
			r = (((arriba >> 16) & 255) * (255 - t) + ((abajo >> 16) & 255) * t) / 255;
			g = (((arriba >> 8) & 255) * (255 - t) + ((abajo >> 8) & 255) * t) / 255;
			b = ((arriba & 255) * (255 - t) + (abajo & 255) * t) / 255;
			c = r << 16 | g << 8 | b;
		}
		if (w <= 0)
			continue;
		caja(x0, y + j, w, 1, c);
		caja(x0, y + j, 2, 1, borde_c);
		caja(x0 + w - 2, y + j, 2, 1, borde_c);
	}
	if (bien) {	/* la marca (✓), en blanco */
		for (i = 0; i < an / 4; i++)
			caja(x + an / 4 + i, y + al / 2 + i - 2, 3, 3, 0xFFFFFF);
		for (i = 0; i < an / 2; i++)
			caja(x + an / 2 + i - 1, y + al / 2 + an / 4 - i - 3, 3, 3, 0xFFFFFF);
	} else {	/* «!» */
		caja(x + an / 2 - 2, y + al / 5, 5, al / 2 - 2, 0xFFFFFF);
		caja(x + an / 2 - 2, y + al * 3 / 4 - 2, 5, 5, 0xFFFFFF);
	}
}

/* ------------------------------------------------------------ los controles */
#define Z_BOTON 1
#define Z_PESTANA 2
#define Z_LISTA 3
#define Z_CASILLA 4
typedef struct zona { int x, y, w, h, clase, id, activo; } zona;
static zona zonas[48];
static int nz, foco = -1, foco_id = -1;
static u32 ventana_n;

static int zona_nueva(int x, int y, int w, int h, int clase, int id, int activo)
{
	if (nz >= 48)
		return -1;
	zonas[nz].x = x, zonas[nz].y = y, zonas[nz].w = w, zonas[nz].h = h;
	zonas[nz].clase = clase, zonas[nz].id = id, zonas[nz].activo = activo;
	return nz++;
}


static void boton(int x, int y, int w, const char *t, int id, int principal, int activo)
{
	int h = 28, f;
	zona_nueva(x, y, w, h, Z_BOTON, id, activo);
	f = foco_id == id;
	caja(x, y, w, h, !activo ? C_GRIS : principal ? C_AZUL : C_PAPEL);
	borde(x, y, w, h, f ? 3 : 1, f ? C_TINTA : C_BORDE);
	if (f && principal)
		borde(x + 3, y + 3, w - 6, h - 6, 1, C_PAPEL);
	texto_n(x + (w - largo437(t) * 8) / 2, y + 6, t, (w - 8) / 8, !activo ? C_SUAVE : principal ? C_PAPEL : C_TINTA, principal);
}

static void casilla(int x, int y, const char *t, int marcada, int id)
{
	int f = foco_id == id;
	zona_nueva(x, y, 20 + largo437(t) * 8 + 8, 22, Z_CASILLA, id, 1);
	caja(x, y + 2, 18, 18, C_PAPEL);
	borde(x, y + 2, 18, 18, f ? 3 : 1, f ? C_AZUL : C_BORDE);
	if (marcada) {	/* una «X» gruesa: se ve sin color */
		int i;
		for (i = 0; i < 10; i++) {
			caja(x + 4 + i, y + 6 + i, 2, 2, C_TINTA);
			caja(x + 13 - i, y + 6 + i, 2, 2, C_TINTA);
		}
	}
	texto(x + 26, y + 3, t, C_TINTA);
	if (f)
		caja(x + 26, y + 20, largo437(t) * 8, 1, C_AZUL);
}

/* una lista: «n» filas de texto, con la elegida en blanco sobre azul y «>» */
typedef const char *(*fila_de)(int i, u32 *tinta);
static void lista(int x, int y, int w, int filas, int n, int elegida, int primera, fila_de f, int id)
{
	int i, fc = foco_id == id;
	zona_nueva(x, y, w, filas * 20 + 4, Z_LISTA, id, 1);
	caja(x, y, w, filas * 20 + 4, C_PAPEL);
	borde(x, y, w, filas * 20 + 4, fc ? 3 : 1, fc ? C_AZUL : C_BORDE);
	for (i = 0; i < filas && primera + i < n; i++) {
		u32 tinta = C_TINTA;
		const char *t = f(primera + i, &tinta);
		int yy = y + 2 + i * 20;
		if (primera + i == elegida) {
			caja(x + 3, yy, w - 6, 20, C_AZUL);
			texto(x + 6, yy + 2, ">", C_PAPEL);
			texto_n(x + 18, yy + 2, t, (w - 28) / 8, C_PAPEL, 0);
		} else
			texto_n(x + 18, yy + 2, t, (w - 28) / 8, tinta, 0);
	}
	if (n > filas) {	/* dónde se está: «12-20 de 54» */
		char d[40];
		formatear(d, sizeof d, "%u-%u de %u", primera + 1, primera + filas < n ? primera + filas : n, n);
		texto(x + w - largo437(d) * 8 - 6, y + filas * 20 + 6, d, C_SUAVE);
	}
}

static void sube(void)
{
	struct orden o;
	memset(&o, 0, sizeof o);
	o.op = OP_MAPA;
	o.ancho = L.an, o.alto = L.al;
	o.datos = L.px;
	sc(SC_DIBUJAR, ventana_n, (u32)&o, 0);
}

/* ------------------------------------------------------------ un cuadro
 * Una ventanita con un texto y unos botones: da el botón elegido (o -1). */
static int cuadro(const char *titulo, const char *t, const char *const *botones, int n_b, int bien)
{
	static char lineas[16][128];
	u32 viejo_v = ventana_n;
	lienzo viejo_l = L;
	int nl = parte(t, 58, lineas, 16), w = 560, h = 70 + nl * 20 + 56, i, r = -1, f = 0, viejo_foco = foco_id;
	struct suceso s;
	char tit[80];
	a437(tit, sizeof tit, titulo);
	ventana_n = sc(SC_VENTANA, (u32)tit, (u32)w, (u32)h);
	if (ES_ERROR(ventana_n)) {
		ventana_n = viejo_v;
		return -1;
	}
	sc(SC_COLOCAR, ventana_n, 0xFFFFFFFFu, 0);
	L.an = w, L.al = h;
	L.px = pl_reserva((u32)(w * h * 4));
	if (!L.px) {
		sc(SC_CERRAR_VENTANA, ventana_n, 0, 0);
		ventana_n = viejo_v, L = viejo_l;
		return -1;
	}
	for (;;) {
		int bw = 150, x;
		nz = 0;
		foco_id = 100 + f;
		caja(0, 0, w, h, C_PAPEL);
		icono_escudo(18, 16, 30, 36, bien);
		for (i = 0; i < nl; i++)
			texto_n(64, 18 + i * 20, lineas[i], -1, C_TINTA, i == 0);
		caja(0, h - 52, w, 52, C_BARRA);
		caja(0, h - 52, w, 1, C_GRIS);
		x = w - 16 - n_b * (bw + 10) + 10;
		for (i = 0; i < n_b; i++)
			boton(x + i * (bw + 10), h - 40, bw, botones[i], 100 + i, i == 0, 1);
		sube();
		if (r >= 0)
			break;
		if (sc(SC_SUCESO, ventana_n, (u32)&s, 1) != 1)
			break;
		if (s.clase == EV_CERRAR)
			break;
		if (s.clase == EV_TECLA) {
			if (s.a == 27)
				break;
			if (s.a == '\t' || s.a == T_DERECHA)
				f = (f + 1) % n_b;
			else if (s.a == T_MAYUS_TAB || s.a == T_IZQUIERDA)
				f = (f + n_b - 1) % n_b;
			else if (s.a == '\r' || s.a == '\n' || s.a == ' ')
				r = f;
		} else if (s.clase == EV_RATON_BAJA) {
			for (i = 0; i < nz; i++)
				if ((int)s.a >= zonas[i].x && (int)s.a < zonas[i].x + zonas[i].w && (int)s.b >= zonas[i].y &&
				    (int)s.b < zonas[i].y + zonas[i].h)
					r = zonas[i].id - 100, f = r;
		}
	}
	sc(SC_CERRAR_VENTANA, ventana_n, 0, 0);
	pl_libera(L.px);
	ventana_n = viejo_v, L = viejo_l;
	foco_id = viejo_foco;
	return r;
}

int ventana_pregunta(char letra_u, const char *que)
{
	char t[400];
	static const char *const b[] = { "Revisar", "Ahora no" };
	int r;
	formatear(t, sizeof t,
		  "¿Revisar %s?\nSe acaba de conectar. Escudo puede mirar si lleva algún virus antes de usarla (o antes de llevarla a un Windows). Tarda según lo que lleve.",
		  que);
	serie("escudo: pregunta: ¿revisar %c:?", letra_u);
	r = cuadro("Escudo: ¿revisar la memoria?", t, b, 2, 1);
	serie("escudo: pregunta %c: %s", letra_u, r == 0 ? "revisar" : "ahora no");
	return r == 0;
}

/* (CORTAFUEGOS) un programa quiere recibir conexiones: CF_RED, CF_TODOS,
 * CF_NADIE (que no) o -1 (sin contestar) */
int pregunta_puerto(u32 puerto, u32 tarea)
{
	char t[600];
	static const char *const b[] = { "Desde la red", "Desde todos", "No" };
	const char *np = nombre_puerto(puerto);
	int r;
	formatear(t, sizeof t,
		  "¿Dejar que un programa reciba conexiones?\nUn programa (la tarea %u) quiere recibir conexiones por el puerto TCP %u%s%s%s. "
		  "Hasta que se diga, el cortafuegos no deja entrar nada.\n«Desde la red»: solo desde la red local y desde Tailscale. "
		  "Ojo: en una Wi-Fi pública (un bar, una biblioteca), la red local es también la de los demás. «Desde todos»: también desde Internet.",
		  tarea, puerto, np[0] ? " (el de " : "", np, np[0] ? ")" : "");
	serie("escudo: cortafuegos: pregunta por el puerto %u", puerto);
	r = cuadro("Escudo: cortafuegos", t, b, 3, 1);
	serie("escudo: cortafuegos: puerto %u: %s", puerto, r == 0 ? "desde la red" : r == 1 ? "desde todos" : r == 2 ? "no" : "sin contestar");
	return r == 0 ? CF_RED : r == 1 ? CF_TODOS : r == 2 ? CF_NADIE : -1;
}

int ventana_hallazgo(const char *ruta, const char *nombre, const char *dentro, int antes)
{
	char t[900];
	static const char *const b1[] = { "A cuarentena", "Borrarlo", "Dejarlo" };
	static const char *const b2[] = { "A cuarentena", "No abrirlo", "Abrirlo igual" };
	int r;
	formatear(t, sizeof t,
		  "[!] Escudo ha encontrado algo en un archivo.\n%s%s%s\nCoincide con la firma «%s» (de ClamAV).\n%s"
		  "Lo prudente es ponerlo en cuarentena: se guarda aparte, sin que se pueda abrir, y se puede devolver luego si era un error.",
		  ruta, dentro && dentro[0] ? " > " : "", dentro ? dentro : "", nombre,
		  antes ? "No se ha abierto el programa. " : "");
	serie("escudo: aviso: %s: %s", ruta, nombre);
	r = cuadro("Escudo: encontrado", t, antes ? b2 : b1, 3, 0);
	if (r == 0)
		return H_CUARENTENA;
	if (r == 1)
		return antes ? H_DEJAR : H_BORRAR;
	if (r == 2)
		return antes ? H_ABRIR : H_DEJAR;
	return H_DEJAR;
}

/* ------------------------------------------------------------ la ventana */
#define P_REVISAR 0
#define P_CUARENTENA 1
#define P_HISTORIAL 2
#define P_RED 3
#define P_OPCIONES 4
static const char *const nombres_p[5] = { "Revisar", "Cuarentena", "Historial", "Cortafuegos", "Opciones" };
#define M_INICIO 0
#define M_UNIDAD 1
#define M_CARPETA 2
#define M_REVISANDO 3
#define M_HECHO 4
#define M_ACTUALIZANDO 5		/* (RÁPIDO) trayendo las firmas: la barra y «Cancelar» */
static int pestana, modo, elegida, primera, cerrar_ya;
static char aviso_abajo[200];
static int aviso_bien;

/* las unidades */
static struct info_unidad unidades[26];
static int n_unidades;
static void lee_unidades(void)
{
	u32 i;
	n_unidades = 0;
	for (i = 0; n_unidades < 26 && info_unidad(i, &unidades[n_unidades]); i++)
		if (unidades[n_unidades].letra)
			n_unidades++;
}
static const char *que_unidad(const struct info_unidad *u)
{
	if (u->del_sistema)
		return "el disco del sistema";
	if (u->medio == MEDIO_USB || u->quitable)
		return "memoria USB";
	if (u->medio > MEDIO_USB)
		return "CD o DVD";
	if (u->medio == MEDIO_MEMORIA)
		return "en memoria";
	return "disco";
}
static const char *fila_unidad(int i, u32 *tinta)
{
	static char t[120];
	const struct info_unidad *u = &unidades[i];
	(void)tinta;
	formatear(t, sizeof t, "%c:  %s%s%s%s  (%S MB)", u->letra, que_unidad(u), u->etiqueta[0] ? " «" : "", u->etiqueta,
		  u->etiqueta[0] ? "»" : "", u->total_k / 1024);
	return t;
}

/* las carpetas, para elegir una */
#define MAX_CARP 200
static char carp_actual[RUTA];
static char carp_nombres[MAX_CARP][13];
static char carp_largos[MAX_CARP][40];
static int n_carp;
static void lee_carpetas(void)
{
	struct entrada e;
	char largo[256];
	int h = abre_carpeta(carp_actual);
	n_carp = 0;
	if (h < 0)
		return;
	while (n_carp < MAX_CARP && entrada_siguiente(h, &e, largo)) {
		if (!(e.atributos & AT_CARPETA) || (e.nombre[0] == '.'))
			continue;
		formatear(carp_nombres[n_carp], 13, "%s", e.nombre);
		formatear(carp_largos[n_carp], 40, "%s", largo);
		n_carp++;
	}
	pl_cerrar(h);
}
static const char *fila_carpeta(int i, u32 *tinta)
{
	static char t[80];
	(void)tinta;
	if (i == 0)
		return "[..] subir";
	formatear(t, sizeof t, "[+] %s", carp_largos[i - 1]);
	return t;
}

/* lo encontrado */
static const char *fila_hallado(int i, u32 *tinta)
{
	static char t[RUTA + 160];
	const hallado *h = &rev.h[i];
	*tinta = h->aviso ? C_SUAVE : C_ROJO;
	formatear(t, sizeof t, "%s %s%s%s: %s%s", h->aviso ? "[i]" : "[!]", h->ruta, h->dentro[0] ? " > " : "", h->dentro,
		  h->aviso ? "aviso: AUTORUN.INF" : h->nombre,
		  h->hecho == 1 ? " (en cuarentena)" : h->hecho == 2 ? " (borrado)" : h->hecho == 3 ? " (dejado)" : "");
	return t;
}
static const char *fila_cuarentena(int i, u32 *tinta)
{
	static char t[RUTA + 160];
	(void)tinta;
	formatear(t, sizeof t, "%s  %s  (de %s)", cua[i].cuando, cua[i].nombre, cua[i].ruta);
	return t;
}

/* el historial: las líneas de REGISTRO.TXT */
static char reg_texto[16384];
static const char *reg_lineas[400];
static int n_reg;
static void lee_historial(void)
{
	char *p;
	int n = lee_registro(reg_texto, sizeof reg_texto);
	n_reg = 0;
	p = reg_texto;
	if (n >= (int)sizeof reg_texto - 1) {	/* (la primera, a medias) */
		while (*p && *p != '\n')
			p++;
		if (*p)
			p++;
	}
	while (*p && n_reg < 400) {
		char *q = p;
		while (*q && *q != '\n')
			q++;
		reg_lineas[n_reg++] = p;
		if (*q) {
			if (q > p && q[-1] == '\r')
				q[-1] = 0;
			*q++ = 0;
		}
		p = q;
	}
}
static const char *fila_reg(int i, u32 *tinta)
{
	(void)tinta;
	return reg_lineas[i];
}

/* ids de los botones */
enum {
	B_TODO = 1, B_UNIDAD, B_CARPETA, B_ACTUALIZAR, B_VOLVER, B_REVISAR_U, B_REVISAR_C, B_CANCELAR, B_CUARENTENA,
	B_BORRAR, B_DEJAR, B_RESTAURAR, B_BORRAR_C, B_APAGAR_SSH, B_MENOS, B_MAS, B_OTRA,
	B_CF_RED, B_CF_TODOS, B_CF_NO, B_CF_PING, K_CF_ACTIVO, K_CF_AVISAR,
	L_LISTA = 30,
	K_PREGUNTAR = 40, K_DESCARGAS, K_ANTES, K_ZIP, K_SOLA, K_SOLO,
	PEST = 60
};

static int arriba_contenido = 108;

static void cabecera(void)
{
	static char l[3][128];
	char t[300];
	int bien, i, x, n;
	texto_estado(t, sizeof t, &bien);
	caja(0, 0, L.an, 72, bien ? C_AZUL_CLARO : C_NARANJA_CLARO);
	caja(0, 72, L.an, 1, C_GRIS);
	icono_escudo(14, 12, 36, 44, bien);
	texto_n(62, 8, "Escudo: el antivirus de Hispa-Pentium", -1, C_TINTA, 1);
	texto_n(62, 28, bien ? "[OK]" : "[!]", -1, bien ? C_VERDE : C_NARANJA, 1);
	n = parte(t, (L.an - 112) / 8, l, 2);
	for (i = 0; i < n; i++)
		texto(102, 28 + 18 * i, l[i], C_TINTA);
	/* las pestañas */
	x = 10;
	for (i = 0; i < 5; i++) {
		char nn[40];
		int w;
		if (i == P_CUARENTENA)
			formatear(nn, sizeof nn, "Cuarentena (%u)", n_cua);
		else
			formatear(nn, sizeof nn, "%s", nombres_p[i]);
		w = largo437(nn) * 8 + 24;
		zona_nueva(x, 78, w, 26, Z_PESTANA, PEST + i, 1);
		if (i == pestana) {
			caja(x, 76, w, 30, C_FONDO);
			borde(x, 76, w, 30, 1, C_BORDE);
			caja(x + 1, 104, w - 2, 3, C_FONDO);
			caja(x, 76, w, 3, C_AZUL);
			texto_n(x + 12, 84, nn, -1, C_TINTA, 1);
		} else {
			caja(x, 80, w, 24, C_BARRA);
			borde(x, 80, w, 24, 1, C_GRIS);
			texto(x + 12, 84, nn, C_SUAVE);
		}
		if (foco_id == PEST + i)
			borde(x + 3, 82, w - 6, 20, 2, C_AZUL);
		x += w + 4;
	}
	caja(0, 105, L.an, 1, C_BORDE);
}

static void pie(const char *t)
{
	caja(0, L.al - 26, L.an, 26, C_BARRA);
	caja(0, L.al - 26, L.an, 1, C_GRIS);
	texto_n(10, L.al - 21, t, (L.an - 20) / 8, C_SUAVE, 0);
}

static void parrafo(int x, int y, int cabe, const char *t, u32 tinta)
{
	static char l[12][128];
	int n = parte(t, cabe, l, 12), i;
	for (i = 0; i < n; i++)
		texto(x, y + i * 18, l[i], tinta);
}

static int movible(const hallado *h);
static void foco_hallado(void);

static void pinta_revisar(void)
{
	int y = arriba_contenido + 10, i;
	char t[400];
	switch (modo) {
	case M_INICIO:
		boton(20, y, 230, "Revisar todo el equipo", B_TODO, 1, 1);
		texto(264, y + 6, "Todas las unidades: discos, memorias y CD.", C_SUAVE);
		boton(20, y + 44, 230, "Revisar una unidad...", B_UNIDAD, 0, 1);
		texto(264, y + 50, "Una memoria USB, un CD o un disco.", C_SUAVE);
		boton(20, y + 88, 230, "Revisar una carpeta...", B_CARPETA, 0, 1);
		texto(264, y + 94, "Solo lo que hay en una carpeta.", C_SUAVE);
		boton(20, y + 132, 230, "Actualizar las firmas", B_ACTUALIZAR, 0, 1);
		texto(264, y + 138, "Traerlas por Internet (actualiza.hispados.es).", C_SUAVE);
		parrafo(20, y + 186, 74,
			"Para revisar una memoria antes de llevarla a un ordenador con Windows: conéctela aquí y elija «Revisar una unidad». "
			"Lo que se encuentre puede ir a la cuarentena (aparte, sin que se pueda abrir).",
			C_TINTA);
		if (aviso_abajo[0]) {
			caja(20, y + 270, L.an - 40, 26, aviso_bien ? C_VERDE_CLARO : C_NARANJA_CLARO);
			texto_n(28, y + 275, aviso_abajo, (L.an - 56) / 8, C_TINTA, 0);
		}
		pie("Tab: de un botón a otro.  Intro: pulsar.  Ctrl+Tab: otra pestaña.  Esc: cerrar.");
		break;
	case M_UNIDAD:
		texto_n(20, y, "Elija la unidad que quiere revisar:", -1, C_TINTA, 1);
		lista(20, y + 24, L.an - 40, 12, n_unidades, elegida, primera, fila_unidad, L_LISTA);
		boton(L.an - 20 - 150 - 10 - 150, L.al - 66, 150, "Revisar", B_REVISAR_U, 1, n_unidades > 0);
		boton(L.an - 20 - 150, L.al - 66, 150, "Volver", B_VOLVER, 0, 1);
		pie("Flechas: elegir.  Intro: revisar esa.  Esc: volver.");
		break;
	case M_CARPETA:
		texto_n(20, y, "La carpeta:", -1, C_TINTA, 1);
		texto_n(116, y, carp_actual, (L.an - 136) / 8, C_AZUL, 1);
		lista(20, y + 24, L.an - 40, 12, n_carp + 1, elegida, primera, fila_carpeta, L_LISTA);
		boton(L.an - 20 - 220 - 10 - 150, L.al - 66, 220, "Revisar esta carpeta", B_REVISAR_C, 1, 1);
		boton(L.an - 20 - 150, L.al - 66, 150, "Volver", B_VOLVER, 0, 1);
		pie("Flechas: elegir.  Intro: entrar.  Retroceso: subir.  Esc: volver.");
		break;
	case M_REVISANDO: {
		int w = L.an - 40, hecho;
		u32 pc = 0;
		formatear(t, sizeof t, "Revisando %s", strcmp(rev.raiz, "*") ? rev.raiz : "todo el equipo");
		texto_n(20, y, t, -1, C_TINTA, 1);
		if (rev.contando) {
			formatear(t, sizeof t, "Contando lo que hay: %S archivos...", rev.contados);
			hecho = 0;
		} else {
			pc = rev.octetos_total >= 100 ? rev.octetos / (rev.octetos_total / 100) : rev.octetos_total ? 100 * rev.octetos / rev.octetos_total : 0;
			if (pc > 100)
				pc = 100;
			formatear(t, sizeof t, "%u %%  (%S de %S archivos)", pc, rev.revisados, rev.contados);
			hecho = (int)((u32)(w - 4) * pc / 100);
		}
		caja(20, y + 30, w, 28, C_PAPEL);
		borde(20, y + 30, w, 28, 1, C_BORDE);
		caja(22, y + 32, hecho, 24, C_AZUL);
		/* la cifra encima, con su fondo (se lee sin depender del color de la barra) */
		caja(20 + w / 2 - largo437(t) * 4 - 6, y + 34, largo437(t) * 8 + 12, 20, C_PAPEL);
		texto(20 + w / 2 - largo437(t) * 4, y + 36, t, C_TINTA);
		texto(20, y + 72, "Ahora:", C_SUAVE);
		texto_n(80, y + 72, rev.actual, (L.an - 100) / 8, C_TINTA, 0);
		formatear(t, sizeof t, "Revisados: %S archivos (%S MB).  Encontrados: %u.", rev.revisados, rev.octetos >> 20,
			  rev.n_hallados);
		texto(20, y + 98, t, rev.n_hallados ? C_ROJO : C_TINTA);
		if (rev.n_hallados)
			lista(20, y + 126, L.an - 40, 8, (int)rev.n_hallados, -1, rev.n_hallados > 8 ? (int)rev.n_hallados - 8 : 0,
			      fila_hallado, L_LISTA + 1);
		boton(L.an - 20 - 150, L.al - 66, 150, "Cancelar", B_CANCELAR, 1, 1);
		pie("Esc o «Cancelar»: parar la revisión.");
		break;
	}
	case M_ACTUALIZANDO: {
		int w = L.an - 40, hecho = 0;
		u32 pc = 0;
		texto_n(20, y, "Actualizando las firmas", -1, C_TINTA, 1);
		if (act_total > 100) {	/* (en K: dentro de un paquete grande) */
			pc = act_hecho / (act_total / 100);
			if (pc > 100)
				pc = 100;
			formatear(t, sizeof t, "%u %%", pc);
			hecho = (int)((u32)(w - 4) * pc / 100);
		} else if (act_total) {
			formatear(t, sizeof t, "%u de %u", act_hecho, act_total);
			hecho = (int)((u32)(w - 4) * (act_hecho > act_total ? act_total : act_hecho) / act_total);
		} else
			formatear(t, sizeof t, "Un momento...");
		caja(20, y + 30, w, 28, C_PAPEL);
		borde(20, y + 30, w, 28, 1, C_BORDE);
		caja(22, y + 32, hecho, 24, C_AZUL);
		caja(20 + w / 2 - largo437(t) * 4 - 6, y + 34, largo437(t) * 8 + 12, 20, C_PAPEL);
		texto(20 + w / 2 - largo437(t) * 4, y + 36, t, C_TINTA);
		texto(20, y + 72, "Ahora:", C_SUAVE);
		texto_n(80, y + 72, act_cancelando ? "Cancelando: se deja todo como estaba..." : act_texto[0] ? act_texto : "Preparando",
			(L.an - 100) / 8, C_TINTA, 0);
		parrafo(20, y + 104, 74,
			"Se traen de actualiza.hispados.es (la primera vez, unos 47 MB: puede tardar). Se puede seguir usando el equipo; "
			"si se cancela, todo queda como estaba.",
			C_SUAVE);
		boton(L.an - 20 - 150, L.al - 66, 150, "Cancelar", B_CANCELAR, 1, 1);
		pie("Esc o «Cancelar»: parar (lo traído a medias no se usa).");
		break;
	}
	case M_HECHO: {
		int malos = 0;
		u32 k;
		for (k = 0; k < rev.n_hallados; k++)
			if (!rev.h[k].aviso)
				malos++;
		formatear(t, sizeof t, "%s %s%s %S archivos (%S %s) en %u s.", rev.cortada ? "Cancelado, en" : "Revisado",
			  strcmp(rev.raiz, "*") ? rev.raiz : "todo el equipo",
			  rev.raiz[0] && rev.raiz[strlen(rev.raiz) - 1] == ':' ? "" : ":", rev.revisados,
			  rev.octetos >= (1u << 20) ? rev.octetos >> 20 : (rev.octetos + 1023) / 1024, rev.octetos >= (1u << 20) ? "MB" : "K",
			  rev.ms / 1000);
		texto_n(20, y, t, (L.an - 40) / 8, C_TINTA, 1);
		if (!rev.n_hallados) {
			caja(20, y + 28, L.an - 40, 30, C_VERDE_CLARO);
			texto_n(30, y + 35, "[OK] No se ha encontrado ningún virus.", -1, C_VERDE, 1);
		} else {
			formatear(t, sizeof t, "%s Encontrados: %u (%u %s). Elija uno y diga qué hacer:", malos ? "[!]" : "[i]",
				  rev.n_hallados, malos, malos == 1 ? "con firma de virus" : "con firmas de virus");
			caja(20, y + 28, L.an - 40, 30, malos ? C_ROJO_CLARO : C_NARANJA_CLARO);
			texto_n(30, y + 35, t, (L.an - 60) / 8, malos ? C_ROJO : C_NARANJA, 1);
			lista(20, y + 66, L.an - 40, 8, (int)rev.n_hallados, elegida, primera, fila_hallado, L_LISTA);
			i = elegida >= 0 && elegida < (int)rev.n_hallados && !rev.h[elegida].hecho;
			{
				/* (RÁPIDO) en un CD o una ISO no se puede mover ni borrar; y un
				 * aviso ([i]) se deja por omisión */
				int mv = i && movible(&rev.h[elegida]), dejar = i && (rev.h[elegida].aviso || !mv);
				boton(20, L.al - 66, 160, "A cuarentena", B_CUARENTENA, !dejar, mv);
				boton(190, L.al - 66, 120, "Borrarlo", B_BORRAR, 0, mv);
				boton(320, L.al - 66, 120, "Dejarlo", B_DEJAR, dejar, i);
			}
		}
		for (k = 0; k < rev.n_hallados; k++)
			if (rev.h[k].aviso) {
				/* (RÁPIDO) en dos líneas si no cabe (antes se cortaba: «podría abrir un ...») */
				parrafo(20, L.al - 130, (L.an - 40) / 8,
					"[i] AUTORUN.INF: aquí no hace nada; en un Windows viejo podría abrir un programa solo al meter el disco o la memoria.",
					C_SUAVE);
				break;
			}
		if (rev.cifrados) {
			formatear(t, sizeof t, "[i] %u partes de ZIP van cifradas (con contraseña): no se han podido mirar.", rev.cifrados);
			texto_n(20, L.al - 92, t, (L.an - 40) / 8, C_SUAVE, 0);
		}
		boton(L.an - 20 - 150, L.al - 66, 150, "Volver", B_VOLVER, !rev.n_hallados, 1);
		pie(rev.n_hallados ? "Lo de la cuarentena se puede devolver luego (pestaña «Cuarentena»)." : "Esc: volver.");
		break;
	}
	}
}

static void pinta_cuarentena(void)
{
	int y = arriba_contenido + 10, hay = elegida >= 0 && elegida < (int)n_cua;
	texto_n(20, y, n_cua ? "Lo que está en cuarentena (aparte, sin que se pueda abrir):" : "La cuarentena está vacía.", -1,
		C_TINTA, 1);
	if (n_cua)
		lista(20, y + 24, L.an - 40, 13, (int)n_cua, elegida, primera, fila_cuarentena, L_LISTA);
	boton(20, L.al - 66, 220, "Devolverlo a su sitio", B_RESTAURAR, 0, hay);
	boton(250, L.al - 66, 200, "Borrar para siempre", B_BORRAR_C, 0, hay);
	pie("Devolverlo solo si se sabe que no es un virus (un error de la firma).");
}

static void pinta_historial(void)
{
	int y = arriba_contenido + 10;
	texto_n(20, y, "Lo que ha hecho Escudo (REGISTRO.TXT), lo último abajo:", -1, C_TINTA, 1);
	lista(20, y + 24, L.an - 40, 15, n_reg, -1, primera, fila_reg, L_LISTA);
	pie("Flechas, RePág y AvPág: moverse por el historial.");
}

/* (CORTAFUEGOS) la lista: lo que escucha y las reglas (de cada puerto, una fila) */
typedef struct fila_cf { u32 puerto, tarea, de; int udp, escucha, con_regla; } fila_cf;
static fila_cf filas_cf[2 * MAX_CF];
static int n_filas_cf;
static void junta_cf(void)
{
	u32 i, k;
	n_filas_cf = 0;
	for (i = 0; i < cf.n_escuchas; i++) {
		fila_cf *f = &filas_cf[n_filas_cf++];
		f->puerto = cf.escuchas[i].puerto, f->tarea = cf.escuchas[i].tarea, f->udp = cf.escuchas[i].udp;
		f->escucha = 1, f->con_regla = 0, f->de = CF_NADIE;
		for (k = 0; !f->udp && k < cf.n_reglas; k++)
			if (cf.reglas[k].puerto == f->puerto)
				f->con_regla = 1, f->de = cf.reglas[k].de;
	}
	for (k = 0; k < cf.n_reglas; k++) {
		for (i = 0; i < (u32)n_filas_cf; i++)
			if (!filas_cf[i].udp && filas_cf[i].puerto == cf.reglas[k].puerto)
				break;
		if (i < (u32)n_filas_cf)
			continue;
		filas_cf[n_filas_cf].puerto = cf.reglas[k].puerto, filas_cf[n_filas_cf].udp = 0, filas_cf[n_filas_cf].escucha = 0;
		filas_cf[n_filas_cf].con_regla = 1, filas_cf[n_filas_cf].de = cf.reglas[k].de, filas_cf[n_filas_cf].tarea = 0;
		n_filas_cf++;
	}
}
static const char *fila_cf_texto(int i, u32 *tinta)
{
	static char t[200];
	const fila_cf *f = &filas_cf[i];
	const char *np = nombre_puerto(f->puerto);
	char quien[40];
	if (f->escucha)
		formatear(quien, sizeof quien, "escucha (tarea %u)", f->tarea);
	else
		formatear(quien, sizeof quien, "nadie escucha");
	if (f->udp)
		formatear(t, sizeof t, "[i] UDP %u%s%s%s: %s; solo entran respuestas a lo que pide", f->puerto, np[0] ? " (" : "", np, np[0] ? ")" : "", quien);
	else {
		*tinta = f->de != CF_NADIE ? C_NARANJA : C_TINTA;
		formatear(t, sizeof t, "%s TCP %u%s%s%s: %s; %s", f->de != CF_NADIE ? "[!]" : "[OK]", f->puerto, np[0] ? " (" : "", np,
			  np[0] ? ")" : "", quien, f->con_regla ? texto_de(f->de) : "no entra (sin regla)");
	}
	return t;
}

static void pinta_red(void)
{
	char t[400];
	int y = arriba_contenido + 8, hay, i;
	const fila_cf *f;
	lee_cortafuegos();
	junta_cf();
	if (!cf.activo)
		formatear(t, sizeof t, "[!] El cortafuegos está APAGADO: entra todo lo que llegue a un puerto donde escuche un programa.");
	else
		formatear(t, sizeof t, "[OK] Cortafuegos encendido: no entra nada que no haya pedido el equipo, salvo lo que digan las reglas. Bloqueado desde que arrancó la red: %u.%s",
			  cf.bloqueados, cf.hay_red ? "" : " (La red no está en marcha.)");
	caja(20, y, L.an - 40, 38, cf.activo ? C_VERDE_CLARO : C_NARANJA_CLARO);
	parrafo(28, y + 2, (L.an - 56) / 8, t, C_TINTA);
	y += 44;
	casilla(20, y, "Encendido", cf.activo, K_CF_ACTIVO);
	casilla(150, y, "Avisar si un programa quiere recibir", cf.avisar, K_CF_AVISAR);
	formatear(t, sizeof t, "Ping: %s", cf.ping == 2 ? "de todos" : cf.ping == 1 ? "de la red" : "de nadie");
	boton(L.an - 20 - 160, y - 2, 160, t, B_CF_PING, 0, 1);
	y += 28;
	texto_n(20, y, "Lo que escucha en la red, y lo que dejan entrar las reglas:", -1, C_TINTA, 1);
	if (elegida >= n_filas_cf)
		elegida = n_filas_cf - 1;
	lista(20, y + 20, L.an - 40, 6, n_filas_cf, elegida, primera, fila_cf_texto, L_LISTA);
	y += 20 + 6 * 20 + 8;
	f = elegida >= 0 && elegida < n_filas_cf ? &filas_cf[elegida] : 0;
	hay = f && !f->udp;
	boton(20, y, 210, "Desde la red y Tailscale", B_CF_RED, 0, hay);
	boton(240, y, 180, "Desde cualquier sitio", B_CF_TODOS, 0, hay);
	boton(430, y, 140, "Que no entre", B_CF_NO, 0, hay);
	y += 36;
	texto_n(20, y, "Lo último bloqueado:", -1, C_TINTA, 1);
	if (!cf.n_ultimos)
		texto(200, y, "nada", C_SUAVE);
	for (i = 0; i < (int)cf.n_ultimos && i < 3; i++) {
		u32 h = cf.ultimos[i].hora;
		if (cf.ultimos[i].proto == 6)
			formatear(t, sizeof t, "%02u:%02u  TCP desde %s al puerto %u%s%s", h >> 11, (h >> 5) & 63, cf.ultimos[i].ip, cf.ultimos[i].puerto,
				  nombre_puerto(cf.ultimos[i].puerto)[0] ? " de " : "", nombre_puerto(cf.ultimos[i].puerto));
		else
			formatear(t, sizeof t, "%02u:%02u  ping desde %s", h >> 11, (h >> 5) & 63, cf.ultimos[i].ip);
		texto_n(28, y + 18 + 18 * i, t, (L.an - 160) / 8, C_SUAVE, 0);
	}
	boton(L.an - 20 - 130, L.al - 66, 130, "Apagar SSH", B_APAGAR_SSH, 0, ssh_encendido());
	pie("«Red local»: la del cable o la Wi-Fi de ahora (en una Wi-Fi pública, también la de los demás).");
}

static void pinta_opciones(void)
{
	int y = arriba_contenido + 14;
	char t[80];
	casilla(20, y, "Preguntar «¿Revisar?» al conectar una memoria USB o meter un CD", op.preguntar, K_PREGUNTAR);
	casilla(20, y + 30, "Revisar lo que se descarga con el Navegador", op.descargas, K_DESCARGAS);
	casilla(20, y + 60, "Revisar los programas de Windows (.EXE) antes de abrirlos", op.antes_exe, K_ANTES);
	casilla(20, y + 90, "Mirar dentro de los archivos ZIP", op.zip, K_ZIP);
	casilla(20, y + 120, "Poner en cuarentena sin preguntar", op.cuarentena_sola, K_SOLA);
	casilla(20, y + 150, "Actualizar las firmas solas (una vez al día, con red)", op.actualizar_solo, K_SOLO);
	formatear(t, sizeof t, "Mirar por dentro archivos de hasta %u MB (los más grandes, solo su principio).", op.max_mb);
	texto(20, y + 194, t, C_TINTA);
	boton(20, y + 216, 50, "-", B_MENOS, 0, op.max_mb > 1);
	boton(80, y + 216, 50, "+", B_MAS, 0, op.max_mb < 128);
	parrafo(20, y + 262, 74,
		"Las firmas son las de ClamAV (de Cisco Talos), en su formato; el motor es propio. Lo que se mira, en el LEEME.",
		C_SUAVE);
	pie("Espacio: marcar o desmarcar.  Se guarda solo (ESCUDO.INI).");
}

static void pinta(void)
{
	nz = 0;
	caja(0, 0, L.an, L.al, C_FONDO);
	cabecera();
	switch (pestana) {
	case P_REVISAR: pinta_revisar(); break;
	case P_CUARENTENA: pinta_cuarentena(); break;
	case P_HISTORIAL: pinta_historial(); break;
	case P_RED: pinta_red(); break;
	default: pinta_opciones(); break;
	}
	/* el foco: si el que lo tenía ya no está, el primero que se pueda */
	foco = -1;
	{
		int i;
		for (i = 0; i < nz; i++)
			if (zonas[i].id == foco_id && zonas[i].activo)
				foco = i;
	}
	sube();
}

/* ------------------------------------------------------------ lo que se hace */
static void empieza_revision(const char *ruta);

static void a_pestana(int p)
{
	pestana = p;
	elegida = 0, primera = 0;
	if (p == P_REVISAR && modo != M_REVISANDO && modo != M_HECHO && modo != M_ACTUALIZANDO)
		modo = M_INICIO;
	if (p == P_CUARENTENA)
		lee_cuarentena();
	if (p == P_HISTORIAL) {
		lee_historial();
		primera = n_reg > 15 ? n_reg - 15 : 0;
	}
	foco_id = PEST + p;
}

static int filas_lista(void)
{
	if (pestana == P_HISTORIAL)
		return 15;
	if (pestana == P_CUARENTENA)
		return 13;
	if (pestana == P_RED)
		return 6;
	return modo == M_HECHO ? 8 : 12;
}
static int largo_lista(void)
{
	if (pestana == P_CUARENTENA)
		return (int)n_cua;
	if (pestana == P_RED)
		return n_filas_cf;
	if (pestana == P_HISTORIAL)
		return n_reg;
	if (modo == M_UNIDAD)
		return n_unidades;
	if (modo == M_CARPETA)
		return n_carp + 1;
	if (modo == M_HECHO)
		return (int)rev.n_hallados;
	return 0;
}
static void mueve_lista(int d)
{
	int n = largo_lista(), f = filas_lista();
	if (!n)
		return;
	if (pestana == P_HISTORIAL) {	/* aquí se mueve la vista */
		primera += d;
		if (primera > n - f)
			primera = n - f;
		if (primera < 0)
			primera = 0;
		return;
	}
	elegida += d;
	if (elegida >= n)
		elegida = n - 1;
	if (elegida < 0)
		elegida = 0;
	if (elegida < primera)
		primera = elegida;
	if (elegida >= primera + f)
		primera = elegida - f + 1;
	foco_hallado();	/* (RÁPIDO) */
}

static void entra_carpeta(int i)
{
	if (i == 0) {	/* subir */
		char *p = strrchr(carp_actual, '\\');
		if (strlen(carp_actual) <= 3) {	/* de la raíz, a las unidades */
			modo = M_UNIDAD;
			lee_unidades();
			elegida = 0, primera = 0;
			return;
		}
		if (p && p - carp_actual == 2)
			p[1] = 0;
		else if (p)
			*p = 0;
	} else if (i - 1 < n_carp) {
		u32 n = (u32)strlen(carp_actual);
		formatear(carp_actual + n, RUTA - (int)n, "%s%s", carp_actual[n - 1] == '\\' ? "" : "\\", carp_nombres[i - 1]);
	}
	lee_carpetas();
	elegida = 0, primera = 0;
}

/* (RÁPIDO) ¿se puede mover o borrar? (no en un CD, una ISO o lo de solo leer) */
static int movible(const hallado *h)
{
	struct info_unidad u;
	u32 k;
	char l = (char)(h->ruta[0] & ~32);
	if (h->ruta[1] != ':')
		return 1;
	for (k = 0; info_unidad(k, &u); k++)
		if (u.letra == (u8)l)
			return !u.solo_lectura && u.medio <= MEDIO_USB;
	return 1;
}

/* el botón con el foco según lo elegido: «Dejarlo» si es un aviso o no se puede mover */
static void foco_hallado(void)
{
	const hallado *h;
	if (modo != M_HECHO || elegida < 0 || elegida >= (int)rev.n_hallados)
		return;
	h = &rev.h[elegida];
	if (foco_id == B_CUARENTENA || foco_id == B_DEJAR || foco_id == B_BORRAR || foco_id == L_LISTA)
		if (foco_id != L_LISTA)
			foco_id = h->aviso || !movible(h) ? B_DEJAR : B_CUARENTENA;
}

static void haz_hallado(int accion)
{
	hallado *h;
	if (elegida < 0 || elegida >= (int)rev.n_hallados)
		return;
	h = &rev.h[elegida];
	if (h->hecho)
		return;
	if (accion == B_CUARENTENA)
		h->hecho = a_cuarentena(h->ruta, h->aviso ? "AUTORUN.INF (aviso)" : h->nombre) ? 0 : 1;
	else if (accion == B_BORRAR) {
		static const char *const b[] = { "Borrarlo", "No" };
		char t[400];
		formatear(t, sizeof t, "¿Borrar %s para siempre?\nNo se podrá recuperar. (La cuarentena sí deja devolverlo.)", h->ruta);
		if (cuadro("Escudo: borrar", t, b, 2, 0) == 0)
			h->hecho = borra_archivo(h->ruta, h->nombre) ? 0 : 2;
	} else if (accion == B_DEJAR) {
		h->hecho = 3;
		apunta("dejado donde estaba (lo ha pedido el usuario): %s (%s)", h->ruta, h->nombre);
	}
	if (h->hecho && elegida + 1 < (int)rev.n_hallados) {
		mueve_lista(1);
		foco_hallado();
	}
}

static int cada_actualiza(void);
static void haz(int id)
{
	if (id >= PEST && id < PEST + 5) {
		a_pestana(id - PEST);
		return;
	}
	switch (id) {
	case B_TODO: empieza_revision("*"); break;
	case B_UNIDAD:
		modo = M_UNIDAD;
		lee_unidades();
		elegida = 0, primera = 0;
		foco_id = L_LISTA;
		break;
	case B_CARPETA:
		modo = M_CARPETA;
		formatear(carp_actual, RUTA, "%c:\\", unidad_sistema());
		lee_carpetas();
		elegida = 0, primera = 0;
		foco_id = L_LISTA;
		break;
	case B_ACTUALIZAR: {
		int r;
		/* (RÁPIDO) sin dejar la ventana parada: la barra y «Cancelar» */
		modo = M_ACTUALIZANDO;
		foco_id = B_CANCELAR;
		pinta();
		r = actualiza_firmas_con(cada_actualiza);
		modo = M_INICIO;
		foco_id = B_TODO;
		if (!r) {
			formatear(aviso_abajo, sizeof aviso_abajo, "Cargando las firmas nuevas...");
			aviso_bien = 1;
			pinta();
			carga_firmas();
			formatear(aviso_abajo, sizeof aviso_abajo, "[OK] Firmas puestas al día.");
		} else if (r == -3)
			formatear(aviso_abajo, sizeof aviso_abajo, "[i] Cancelado: las firmas siguen como estaban.");
		else if (r == -2)
			formatear(aviso_abajo, sizeof aviso_abajo, "[!] Sin PAQUETES: el sistema no está instalado en el disco.");
		else if (act_dice[0] && strstr(act_dice, "lista de paquetes"))
			formatear(aviso_abajo, sizeof aviso_abajo, "[!] Sin la lista de paquetes: mire la red o el Wi-Fi y pruebe otra vez.");	/* (0.8.10: que quepa) */
		else if (act_dice[0])
			formatear(aviso_abajo, sizeof aviso_abajo, "[!] No se han podido poner: %s", act_dice);
		else
			formatear(aviso_abajo, sizeof aviso_abajo, "[!] No se han podido traer (¿hay red? Mire la red en Configuración).");
		aviso_bien = r == 0;
		break;
	}
	case B_VOLVER:
		modo = M_INICIO;
		foco_id = B_TODO;
		break;
	case B_REVISAR_U:
		if (elegida >= 0 && elegida < n_unidades) {
			char x[4];
			formatear(x, sizeof x, "%c:", unidades[elegida].letra);
			empieza_revision(x);
		}
		break;
	case B_REVISAR_C: empieza_revision(carp_actual); break;
	case B_CUARENTENA:
	case B_BORRAR:
	case B_DEJAR: haz_hallado(id); break;
	case B_RESTAURAR:
	case B_BORRAR_C:
		if (elegida >= 0 && elegida < (int)n_cua) {
			static const char *const b1[] = { "Devolverlo", "No" };
			static const char *const b2[] = { "Borrarlo", "No" };
			char t[600], donde[RUTA];
			if (id == B_RESTAURAR)
				formatear(t, sizeof t, "¿Devolver %s a su sitio?\nEscudo lo encontró como «%s». Hágalo solo si sabe que no es un virus.",
					  cua[elegida].ruta, cua[elegida].nombre);
			else
				formatear(t, sizeof t, "¿Borrar para siempre lo que vino de %s?\n(«%s».) No se podrá recuperar.",
					  cua[elegida].ruta, cua[elegida].nombre);
			if (cuadro(id == B_RESTAURAR ? "Escudo: devolver" : "Escudo: borrar", t, id == B_RESTAURAR ? b1 : b2, 2, 0) == 0) {
				if (id == B_RESTAURAR)
					restaura((u32)elegida, donde);
				else
					borra_de_cuarentena((u32)elegida);
			}
			lee_cuarentena();
			if (elegida >= (int)n_cua)
				elegida = (int)n_cua - 1;
		}
		break;
	case B_APAGAR_SSH: apaga_ssh(); break;
	case B_CF_RED:
	case B_CF_TODOS:
	case B_CF_NO:
		if (elegida >= 0 && elegida < n_filas_cf && !filas_cf[elegida].udp)
			pon_regla_cf(filas_cf[elegida].puerto, id == B_CF_RED ? CF_RED : id == B_CF_TODOS ? CF_TODOS : CF_NADIE);
		break;
	case B_CF_PING:
		cf.ping = (cf.ping + 2) % 3;	/* de la red -> de nadie -> de todos */
		guarda_cortafuegos();
		break;
	case K_CF_ACTIVO:
		if (cf.activo) {
			static const char *const b[] = { "Apagarlo", "No" };
			if (cuadro("Escudo: cortafuegos", "¿Apagar el cortafuegos?\nEntrará todo lo que llegue a un puerto donde escuche un programa. Mejor dejarlo encendido y añadir una regla.", b, 2, 0) != 0)
				break;
		}
		cf.activo = !cf.activo;
		guarda_cortafuegos();
		break;
	case K_CF_AVISAR: cf.avisar = !cf.avisar, guarda_cortafuegos(); break;
	case B_MENOS:
		if (op.max_mb > 1)
			op.max_mb = op.max_mb > 8 ? op.max_mb / 2 : op.max_mb - 1;
		guarda_opciones();
		break;
	case B_MAS:
		if (op.max_mb < 128)
			op.max_mb = op.max_mb >= 8 ? op.max_mb * 2 : op.max_mb + 1;
		guarda_opciones();
		break;
	case K_PREGUNTAR: op.preguntar = !op.preguntar, guarda_opciones(); break;
	case K_DESCARGAS: op.descargas = !op.descargas, guarda_opciones(); break;
	case K_ANTES: op.antes_exe = !op.antes_exe, guarda_opciones(); break;
	case K_ZIP: op.zip = !op.zip, guarda_opciones(); break;
	case K_SOLA: op.cuarentena_sola = !op.cuarentena_sola, guarda_opciones(); break;
	case K_SOLO: op.actualizar_solo = !op.actualizar_solo, guarda_opciones(); break;
	case L_LISTA:
		if (pestana == P_REVISAR && modo == M_UNIDAD)
			haz(B_REVISAR_U);
		else if (pestana == P_REVISAR && modo == M_CARPETA)
			entra_carpeta(elegida);
		break;
	}
}

/* Tab: el control siguiente que se pueda usar */
static void tabula(int d)
{
	int i, k;
	if (!nz)
		return;
	k = foco < 0 ? (d > 0 ? -1 : 0) : foco;
	for (i = 0; i < nz; i++) {
		k = (k + d + nz) % nz;
		if (zonas[k].activo && zonas[k].clase != Z_PESTANA) {
			foco_id = zonas[k].id;
			return;
		}
		if (zonas[k].clase == Z_PESTANA && zonas[k].id == PEST + pestana) {
			foco_id = zonas[k].id;
			return;
		}
	}
}

static int en(const zona *z, int x, int y) { return x >= z->x && x < z->x + z->w && y >= z->y && y < z->y + z->h; }

/* un suceso de la ventana; durante la revisión, solo «cancelar» y cerrar */
static int atiende(const struct suceso *s, int revisando)
{
	int i;
	switch (s->clase) {
	case EV_CERRAR:
		cerrar_ya = 1;
		return 1;
	case EV_TECLA:
		if (revisando) {
			if (s->a == 27 || ((s->a == '\r' || s->a == ' ') && foco_id == B_CANCELAR))
				return 1;
			return 0;
		}
		if (s->a == 27) {
			if (pestana == P_REVISAR && modo != M_INICIO) {
				modo = M_INICIO;
				foco_id = B_TODO;
			} else
				cerrar_ya = 1;
		} else if (s->a == '\t')
			tabula(1);
		else if (s->a == T_MAYUS_TAB)
			tabula(-1);
		else if (s->a == T_CTRL_TAB)
			a_pestana((pestana + 1) % 5);
		else if (s->a == T_CTRL_MAYUS_TAB)
			a_pestana((pestana + 4) % 5);
		else if (foco_id >= PEST && foco_id < PEST + 5 && (s->a == T_DERECHA || s->a == T_IZQUIERDA))
			a_pestana((pestana + (s->a == T_DERECHA ? 1 : 4)) % 5);
		else if (s->a == T_ARRIBA || s->a == T_ABAJO) {
			if (largo_lista()) {
				foco_id = L_LISTA;
				mueve_lista(s->a == T_ARRIBA ? -1 : 1);
			}
		} else if (s->a == T_RE_PAG || s->a == T_AV_PAG)
			mueve_lista(s->a == T_RE_PAG ? -filas_lista() : filas_lista());
		else if (s->a == T_INICIO || s->a == T_FIN)
			mueve_lista(s->a == T_INICIO ? -100000 : 100000);
		else if (s->a == 8 && pestana == P_REVISAR && modo == M_CARPETA)
			entra_carpeta(0);
		else if (s->a == '\r' || s->a == '\n' || s->a == ' ') {
			if (foco >= 0 && zonas[foco].activo)
				haz(zonas[foco].id);
		} else if (s->a == T_F5 && pestana == P_HISTORIAL)
			a_pestana(P_HISTORIAL);
		return 0;
	case EV_RATON_BAJA:
		for (i = 0; i < nz; i++)
			if (en(&zonas[i], (int)s->a, (int)s->b) && zonas[i].activo) {
				if (revisando)
					return zonas[i].id == B_CANCELAR;
				foco_id = zonas[i].id;
				if (zonas[i].clase == Z_LISTA) {
					int fila = ((int)s->b - zonas[i].y - 2) / 20, n = largo_lista();
					if (pestana != P_HISTORIAL && primera + fila < n && fila >= 0) {
						int ya = elegida == primera + fila;
						elegida = primera + fila;
						foco_hallado();
						if (ya && (modo == M_CARPETA || modo == M_UNIDAD))
							haz(L_LISTA);	/* un segundo clic: entrar */
					}
				} else
					haz(zonas[i].id);
				break;
			}
		return 0;
	case EV_RUEDA: {
		int pasos = (int)(signed char)(s->c & 0xFF);
		if (!revisando)
			mueve_lista(-pasos * 3);
		return 0;
	}
	}
	return 0;
}

/* (desde pl_cortar, mientras se revisa): 1 si hay que parar */
static u32 ultimo_pintado;
/* (RÁPIDO) mientras PAQUETES trae las firmas: la ventana atendida y pintada; 1: «Cancelar» */
static int cada_actualiza(void)
{
	struct suceso e;
	int parar = 0;
	while (sc(SC_SUCESO, ventana_n, (u32)&e, 0) == 1)
		if (atiende(&e, 1))
			parar = 1;
	if (tics() - ultimo_pintado >= hz() / 4) {
		ultimo_pintado = tics();
		pinta();
	}
	return parar;
}
int atiende_ventana(void)
{
	struct suceso s;
	int parar = 0;
	while (sc(SC_SUCESO, ventana_n, (u32)&s, 0) == 1)
		if (atiende(&s, 1))
			parar = 1;
	if (tics() - ultimo_pintado >= hz() / 4) {
		ultimo_pintado = tics();
		pinta();
	}
	return parar;
}

static void al_avanzar_ventana(void)
{
	if (tics() - ultimo_pintado >= hz() / 4) {
		ultimo_pintado = tics();
		pinta();
	}
}

static void empieza_revision(const char *ruta)
{
	char r[RUTA];
	formatear(r, RUTA, "%s", ruta);
	pestana = P_REVISAR;
	modo = M_REVISANDO;
	foco_id = B_CANCELAR;
	memset(&rev, 0, sizeof rev);
	formatear(rev.raiz, RUTA, "%s", r);
	rev.contando = 1;
	pinta();
	pl_atiende = 1;
	al_avanzar = al_avanzar_ventana;
	revisa(r, 0);
	al_avanzar = 0;
	pl_atiende = 0;
	modo = M_HECHO;
	elegida = rev.n_hallados ? 0 : -1;
	primera = 0;
	foco_id = rev.n_hallados ? B_CUARENTENA : B_VOLVER;
	foco_hallado();
	serie("escudo: ventana: revisión de %s acabada: %u archivos, %u encontrados%s", r, rev.revisados, rev.n_hallados,
	      rev.cortada ? " (cancelada)" : "");
	if (rev.n_hallados && !rev.h[0].hecho) {
		foco_id = B_CUARENTENA;
		foco_hallado();
	}
}

int ventana_principal(const char *revisar_ya, int pest)
{
	struct suceso s;
	ventana_n = sc(SC_VENTANA, (u32)"Escudo", AN, AL);
	if (ES_ERROR(ventana_n))
		return 2;
	sc(SC_COLOCAR, ventana_n, 0xFFFFFFFFu, 0);
	L.an = AN, L.al = AL;
	L.px = pl_reserva(AN * AL * 4);
	if (!L.px)
		return 2;
	serie("escudo: ventana abierta");
	carga_firmas();
	lee_cuarentena();
	a_pestana(pest >= 0 && pest < 5 ? pest : 0);
	modo = M_INICIO;
	foco_id = B_TODO;
	pinta();
	if (revisar_ya)
		empieza_revision(revisar_ya);
	while (!cerrar_ya) {
		pinta();
		if (sc(SC_SUCESO, ventana_n, (u32)&s, 1) != 1)
			break;
		atiende(&s, 0);
		while (!cerrar_ya && sc(SC_SUCESO, ventana_n, (u32)&s, 0) == 1)
			atiende(&s, 0);
	}
	sc(SC_CERRAR_VENTANA, ventana_n, 0, 0);
	serie("escudo: ventana cerrada");
	return rev.n_hallados ? 1 : 0;
}
