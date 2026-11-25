/* ventana.c - (BLUETOOTH) la ventana del Bluetooth («BT /VENTANA»: el
 * Panel de control > Bluetooth) y la orden BT de la Consola. Las dos le
 * piden las cosas al servicio, BT.BIN (la llamada 246; si no está, el
 * núcleo lo pone en marcha).
 *
 * La ventana: arriba, cómo está (en texto: «Encendido», «Apagado», «Sin
 * adaptador»..., nunca solo con un color), con el adaptador; «Encender» o
 * «Apagar» y «Buscar aparatos»; la lista (los emparejados primero, cada uno
 * con lo que es y cómo está: «emparejado, conectado»; el elegido, con un
 * borde y una flecha); «Emparejar», «Conectar», «Olvidar» y «Cerrar». Al
 * emparejar, el código en grande: «Escriba este código en el teclado y
 * pulse Intro», o «¿Sale este número en el otro aparato?» con «Sí» y «No».
 * Con el teclado: las flechas, Intro (emparejar o conectar), Supr
 * (olvidar), B (buscar) y Esc.
 *
 * La orden: BT (cómo está y la lista), BT /BUSCAR, BT /EMPAREJAR n,
 * BT /CONECTAR n, BT /OLVIDAR n, BT /ENCENDER, BT /APAGAR, BT /VENTANA
 * (n: el número de la lista, o la dirección entera).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "../hispac/hispa.h"
#include "bt.h"

#define EV_RATON_SUBE 3
#define TINTA 0x14294A
#define SUAVE 0x5A7396
#define BLANCO 0xFFFFFF
#define AZUL1 0x5AA6F4
#define AZUL2 0x2468C8
#define VERDE 0x1C7A30
#define ROJO 0xB42318

static struct bt_estado e;
static int hay_servicio;

/* ---- hablar con el servicio ---- */
static u32 pide(u32 op, u32 a, const u8 *dir)
{
	struct bt_pedido p;
	u32 r = 0, n;

	memset(&p, 0, sizeof p);
	p.largo = sizeof p;
	p.op = op, p.a = a;
	if (dir)
		memcpy(p.dir, dir, 6);
	n = sc(SC_BT_PEDIR, (u32)&p, (u32)&r, 4);
	if (ES_ERROR(n))
		return 0xFFFFFFFF;
	return r;
}

static int lee(void)
{
	struct bt_pedido p;
	u32 n;

	memset(&p, 0, sizeof p);
	p.largo = sizeof p;
	p.op = BO_ESTADO;
	n = sc(SC_BT_PEDIR, (u32)&p, (u32)&e, sizeof e);
	hay_servicio = !ES_ERROR(n) && n == sizeof e;
	if (!hay_servicio) {
		memset(&e, 0, sizeof e);
		e.estado = BTE_NADA;
	}
	return hay_servicio;
}

static const char *estado_texto(void)
{
	switch (e.estado) {
	case BTE_ENCENDIDO: return "Encendido";
	case BTE_APAGADO: return "Apagado";
	case BTE_SIN_ADAPTADOR: return "Sin adaptador Bluetooth";
	case BTE_FALLO: return "No se ha podido poner en marcha";
	}
	return "El servicio Bluetooth no está en marcha";
}

static const char *que_es(struct bt_fila *f)
{
	u32 mayor = (f->clase >> 8) & 0x1F, menor = (f->clase >> 2) & 0x3F;

	if (f->hid == 3 || (mayor == 5 && (menor >> 4) == 3))
		return "teclado y ratón";
	if (f->hid == HID_TECLADO || (mayor == 5 && (menor >> 4) == 1) || f->aspecto == 0x03C1)
		return "teclado";
	if (f->hid == HID_RATON || (mayor == 5 && (menor >> 4) == 2) || f->aspecto == 0x03C2)
		return "ratón";
	switch (mayor) {
	case 1: return "ordenador";
	case 2: return "teléfono";
	case 4: return "sonido";
	case 5: return "aparato de entrada";
	}
	return f->banderas & BL_LE ? "aparato BLE" : "aparato";
}

static void como_esta(struct bt_fila *f, char *t, int cabe)
{
	int k = 0;

	if (f->banderas & BL_LE && !f->clase)
		k = formatear(t, cabe, "BLE%s", f->banderas & BL_HID_LE ? " (HID): emparejarlo llega en la etapa 2 b" : "");
	else if (f->banderas & BL_CONECTADO)
		k = formatear(t, cabe, "emparejado, conectado");
	else if (f->banderas & BL_CONECTANDO)
		k = formatear(t, cabe, "%s, conectando...", f->banderas & BL_EMPAREJADO ? "emparejado" : "sin emparejar");
	else if (f->banderas & BL_EMPAREJADO)
		k = formatear(t, cabe, "emparejado, sin conectar");
	else
		k = formatear(t, cabe, "sin emparejar");
	if (f->banderas & BL_VISTO && k < cabe - 24)
		formatear(t + k, cabe - k, "; visto ahora (%d dBm)", f->rssi);
}

static const char *nombre_fila(struct bt_fila *f)
{
	static char t[64];

	if (f->nombre[0])
		return f->nombre;
	formatear(t, sizeof t, "(sin nombre: %s)", bt_dir_texto(f->dir));
	return t;
}

/* ============================================================= la orden */
static int numero(const char *a, u8 *dir)
{
	int n = 0, i, k = 0;

	while (*a == ' ')
		a++;
	if (strlen(a) >= 17 && a[2] == ':') {
		for (i = 0; i < 6; i++) {
			u8 b[1];
			if (!h_de_hex(a + 3 * i, b, 1))
				return -1;
			dir[5 - i] = b[0];
		}
		return 0;
	}
	while (*a >= '0' && *a <= '9')
		n = n * 10 + *a++ - '0', k++;
	if (!k || n < 1 || n > (int)e.n)
		return -1;
	memcpy(dir, e.fila[n - 1].dir, 6);
	return 0;
}

static void lista(void)
{
	u32 i;
	char t[100];

	h_escribef("Bluetooth: %s", estado_texto());
	if (e.adaptador[0])
		h_escribef(" (%s, %s)", e.adaptador, bt_dir_texto(e.dir));
	h_escribe(e.buscando ? "; buscando...\n" : ".\n");
	if (e.aviso[0])
		h_escribef("  %s\n", e.aviso);
	for (i = 0; i < e.n; i++) {
		como_esta(&e.fila[i], t, sizeof t);
		h_escribef("%2u. %s (%s), %s: %s\n", i + 1, nombre_fila(&e.fila[i]), que_es(&e.fila[i]), bt_dir_texto(e.fila[i].dir), t);
	}
	if (!e.n)
		h_escribe("  (ningún aparato: BT /BUSCAR)\n");
}

static void mensaje_codigo(void)
{
	switch (e.em) {
	case EM_TECLEAR:
		h_escribef("Escriba %06u en el teclado «%s» y pulse Intro (llevan %u de 6).\n", e.codigo, e.em_nombre, e.tecleados);
		break;
	case EM_PIN:
		h_escribef("Escriba %u en el teclado «%s» y pulse Intro.\n", e.codigo, e.em_nombre);
		break;
	case EM_COMPARAR:
		h_escribef("¿Sale %06u en «%s»? (S/N) ", e.codigo, e.em_nombre);
		break;
	}
}

static int empareja_texto(const u8 *dir)
{
	u32 r = pide(BO_EMPAREJAR, 0, dir), t0 = h_tics(), em = 0xFFFF, tec = 0xFFFF;

	if (r == 1) {
		h_escribe("Para emparejar hace falta ser administrador.\n");
		return 1;
	}
	if (r) {
		lee();
		h_escribef("No se puede emparejar%s%s\n", e.aviso[0] ? ": " : ".", e.aviso);
		return 1;
	}
	h_escribe("Emparejando (ponga el aparato en modo de emparejar)...\n");
	while (h_tics() - t0 < 90 * h_hz) {
		lee();
		if (e.em != em || e.tecleados != tec) {
			em = e.em, tec = e.tecleados;
			if (em == EM_COMPARAR) {
				u32 k;
				mensaje_codigo();
				k = h_tecla(1);
				while (!k && e.em == EM_COMPARAR && h_tics() - t0 < 90 * h_hz) {
					h_dormir(5);
					k = h_tecla(0) ;
				}
				h_escribe(k == 's' || k == 'S' ? "S\n" : "N\n");
				pide(BO_RESPONDER, k == 's' || k == 'S', 0);
				continue;
			}
			mensaje_codigo();
			if (em == EM_BIEN || em == EM_MAL || em == EM_NADA) {
				h_escribef("%s\n", e.aviso);
				return em == EM_BIEN ? 0 : 1;
			}
		}
		if (sc(SC_HUBO_CORTE, 0, 0, 0) == 1) {
			pide(BO_CANCELAR, 0, 0);
			h_escribe("Cancelado.\n");
			return 1;
		}
		h_dormir(10);
	}
	pide(BO_CANCELAR, 0, 0);
	h_escribe("Se ha pasado el tiempo.\n");
	return 1;
}

int ventana(void);

int cliente(const char *a)
{
	u8 dir[6];
	u32 r;

	sc(SC_ATENDER_CORTE, 1, 0, 0);
	while (*a == ' ')
		a++;
	if (h_empieza(a, "/VENTANA"))
		return ventana();
	if (h_empieza(a, "/?")) {
		h_escribe("BT: el Bluetooth (los teclados y los ratones).\n"
			  "  BT              cómo está, y los aparatos (los emparejados y los vistos)\n"
			  "  BT /BUSCAR      buscar aparatos (10 segundos)\n"
			  "  BT /EMPAREJAR n emparejar el número n de la lista (o su dirección)\n"
			  "  BT /CONECTAR n  conectar uno emparejado\n"
			  "  BT /OLVIDAR n   olvidar uno emparejado\n"
			  "  BT /ENCENDER    BT /APAGAR    BT /VENTANA\n");
		return 0;
	}
	if (!lee()) {
		h_escribe("Bluetooth: el servicio no está en marcha (¿hay un adaptador Bluetooth?).\n");
		return 1;
	}
	if (h_empieza(a, "/BUSCAR")) {
		u32 t0 = h_tics();
		pide(BO_BUSCAR, 0, 0);
		h_escribe("Buscando aparatos...\n");
		h_dormir(h_hz);
		while (lee() && e.buscando && h_tics() - t0 < 20 * h_hz && sc(SC_HUBO_CORTE, 0, 0, 0) != 1)
			h_dormir(h_hz / 2);
		h_dormir(2 * h_hz);	/* (los nombres que falten) */
		lee();
		lista();
		return 0;
	}
	if (h_empieza(a, "/EMPAREJAR")) {
		if (numero(a + 10, dir) < 0) {
			h_escribe("¿Cuál? BT /EMPAREJAR n (el número de la lista de BT o BT /BUSCAR)\n");
			return 1;
		}
		return empareja_texto(dir);
	}
	if (h_empieza(a, "/OLVIDAR") || h_empieza(a, "/CONECTAR")) {
		int olvida = h_empieza(a, "/OLVIDAR");
		if (numero(a + (olvida ? 8 : 9), dir) < 0) {
			h_escribe("¿Cuál? (el número de la lista de BT)\n");
			return 1;
		}
		r = pide(olvida ? BO_OLVIDAR : BO_CONECTAR, 0, dir);
		h_escribe(r == 1 ? "Para olvidar un aparato hace falta ser administrador.\n" : r ? "No se puede.\n" : olvida ? "Olvidado.\n" : "Conectando...\n");
		return r != 0;
	}
	if (h_empieza(a, "/ENCENDER") || h_empieza(a, "/APAGAR")) {
		r = pide(h_empieza(a, "/ENCENDER") ? BO_ENCENDER : BO_APAGAR, 0, 0);
		lee();
		h_escribef("Bluetooth: %s.\n", estado_texto());
		return r != 0;
	}
	lista();
	return 0;
}

/* ============================================================ la ventana */
struct orden { u32 op; int x, y, ancho, alto; u32 tinta, tinta2; const void *datos; int n; };
static u32 vn;

static void dib(u32 op, int x, int y, int an, int al, u32 t1, u32 t2, const void *d, int n)
{
	struct orden o = { op, x, y, an, al, t1, t2, d, n };

	sc(SC_DIBUJAR, vn, (u32)&o, 0);
}
static void rect(int x, int y, int an, int al, u32 c) { dib(1, x, y, an, al, c, 0, 0, 0); }
static void degr(int x, int y, int an, int al, u32 a, u32 b) { dib(2, x, y, an, al, a, b, 0, 0); }
static void borde(int x, int y, int an, int al, u32 c) { dib(3, x, y, an, al, c, 0, 0, 0); }
static void redondo(int x, int y, int an, int al, int r, u32 a, u32 b) { dib(9, x, y, an, al, a, b, 0, r); }
static void texto(int x, int y, const char *t, u32 c) { dib(5, x, y, 0, 0, c, 0, t, strlen(t)); }
static void negrita(int x, int y, const char *t, u32 c) { dib(10, x, y, 0, 0, c, 0, t, strlen(t)); }
static void linea(int x1, int y1, int x2, int y2, u32 c) { dib(4, x1, y1, x2, y2, c, 0, 0, 0); }

enum { Z_NADA, Z_ENCENDER, Z_BUSCAR, Z_FILA, Z_EMPAREJAR, Z_CONECTAR, Z_OLVIDAR, Z_CERRAR, Z_SI, Z_NO, Z_CANCELAR };
#define MAX_ZONAS 40
static struct { int x, y, an, al, que, dato; } zonas[MAX_ZONAS];
static int n_zonas;

static void zona(int x, int y, int an, int al, int que, int dato)
{
	if (n_zonas < MAX_ZONAS) {
		zonas[n_zonas].x = x, zonas[n_zonas].y = y, zonas[n_zonas].an = an, zonas[n_zonas].al = al;
		zonas[n_zonas].que = que, zonas[n_zonas].dato = dato;
		n_zonas++;
	}
}

static int zona_en(int x, int y, int *dato)
{
	int i;

	for (i = n_zonas - 1; i >= 0; i--)
		if (x >= zonas[i].x && x < zonas[i].x + zonas[i].an && y >= zonas[i].y && y < zonas[i].y + zonas[i].al) {
			*dato = zonas[i].dato;
			return zonas[i].que;
		}
	return Z_NADA;
}

static void boton(int x, int y, int an, int al, const char *t, int que, int vale)
{
	int tx = x + (an - (int)strlen(t) * 8) / 2;

	if (!vale) {
		/* (sin poder usarse: gris y con el texto suave; no se apunta su zona) */
		redondo(x, y, an, al, 6, 0x9AAAC0, 0x9AAAC0);
		redondo(x + 1, y + 1, an - 2, al - 2, 5, 0xDDE3EC, 0xC9D1DD);
		texto(tx, y + (al - 16) / 2, t, 0x7A8699);
		return;
	}
	redondo(x, y, an, al, 6, 0x184A90, 0x184A90);
	redondo(x + 1, y + 1, an - 2, al - 2, 5, AZUL1, AZUL2);
	negrita(tx + 1, y + (al - 16) / 2 + 1, t, 0x10305C);
	negrita(tx, y + (al - 16) / 2, t, BLANCO);
	zona(x, y, an, al, que, 0);
}

/* la runa del Bluetooth (de «lado» de alto) */
static void runa(int x, int y, int lado, u32 c)
{
	int a = lado / 4, m = x + lado / 2 - lado / 8, i;

	for (i = 0; i < 2; i++) {
		linea(m + i, y, m + i, y + lado, c);
		linea(m + i, y, m + a + i, y + a, c);
		linea(m + a + i, y + a, m - a + i, y + 3 * a, c);
		linea(m + i, y + lado, m + a + i, y + 3 * a, c);
		linea(m + a + i, y + 3 * a, m - a + i, y + a, c);
	}
}

/* las cifras en grande (5 x 7, en cuadraditos de «t» puntos) */
static const u8 cifras[10][7] = {
	{ 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },
	{ 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E },
	{ 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E },
	{ 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
	{ 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C },
};

static void grande(int x, int y, const char *t, int g, u32 c, int hechas)
{
	int i, f, k;

	for (i = 0; t[i]; i++) {
		int d = t[i] - '0';
		u32 col = i < hechas ? VERDE : c;
		if (d < 0 || d > 9)
			continue;
		for (f = 0; f < 7; f++)
			for (k = 0; k < 5; k++)
				if (cifras[d][f] & (0x10 >> k))
					rect(x + i * 6 * g + k * g, y + f * g, g, g, col);
		/* (las ya tecleadas, subrayadas: no solo de otro color) */
		if (i < hechas)
			rect(x + i * 6 * g, y + 8 * g, 5 * g, g / 2 + 1, VERDE);
	}
}

#define AN 600
#define AL 470
#define LISTA_Y 112
#define FILA_AL 38
#define FILAS 7
static int elegida, primera;

static void panel_emparejar(void)
{
	char t[120], c[12];
	int x = 40, y = 120, an = AN - 80, al = 250;

	redondo(x - 2, y - 2, an + 4, al + 4, 10, 0x184A90, 0x184A90);
	redondo(x, y, an, al, 9, 0xFFFFFF, 0xEAF2FD);
	runa(x + 16, y + 16, 28, AZUL2);
	formatear(t, sizeof t, "Emparejar con «%s»", e.em_nombre);
	negrita(x + 56, y + 22, t, TINTA);
	switch (e.em) {
	case EM_TECLEAR:
	case EM_PIN:
		texto(x + 20, y + 60, e.em == EM_PIN ? "Escriba este PIN en el teclado y pulse Intro:" : "Escriba este código en el teclado y pulse Intro:", TINTA);
		formatear(c, sizeof c, e.em == EM_PIN ? "%u" : "%06u", e.codigo);
		grande(x + (an - (int)strlen(c) * 48 + 8) / 2, y + 96, c, 8, TINTA, e.em == EM_TECLEAR ? (int)e.tecleados : 0);
		if (e.em == EM_TECLEAR) {
			formatear(t, sizeof t, "Tecleadas: %u de 6", e.tecleados);
			texto(x + 20, y + 180, t, SUAVE);
		}
		boton(x + an - 130, y + al - 44, 110, 30, "Cancelar", Z_CANCELAR, 1);
		break;
	case EM_COMPARAR:
		texto(x + 20, y + 60, "¿Sale este mismo número en el otro aparato?", TINTA);
		formatear(c, sizeof c, "%06u", e.codigo);
		grande(x + (an - 6 * 48 + 8) / 2, y + 96, c, 8, TINTA, 0);
		boton(x + 20, y + al - 44, 160, 30, "Sí, es el mismo", Z_SI, 1);
		boton(x + 196, y + al - 44, 160, 30, "No", Z_NO, 1);
		break;
	case EM_EN_CURSO:
		texto(x + 20, y + 70, "Emparejando... (el aparato tiene que estar en modo de", TINTA);
		texto(x + 20, y + 88, "emparejar: suele ser un botón que hay que dejar pulsado)", TINTA);
		boton(x + an - 130, y + al - 44, 110, 30, "Cancelar", Z_CANCELAR, 1);
		break;
	case EM_BIEN:
		negrita(x + 20, y + 80, "BIEN: emparejado.", VERDE);
		texto(x + 20, y + 104, e.aviso, TINTA);
		break;
	case EM_MAL:
		negrita(x + 20, y + 80, "NO se ha podido emparejar.", ROJO);
		texto(x + 20, y + 104, e.aviso, TINTA);
		break;
	}
}

static void pinta(void)
{
	char t[160];
	u32 i;
	int y, sel_vale;
	struct bt_fila *f = elegida >= 0 && elegida < (int)e.n ? &e.fila[elegida] : 0;

	n_zonas = 0;
	degr(0, 0, AN, AL, 0xF4F8FE, 0xD6E6FA);
	redondo(14, 12, 40, 40, 8, 0x2E6FD0, 0x1C4FA0);
	runa(20, 18, 28, BLANCO);
	negrita(66, 12, "Bluetooth", TINTA);
	if (e.adaptador[0])
		formatear(t, sizeof t, "%s: %s%s", estado_texto(), e.adaptador, e.buscando ? "; buscando..." : "");
	else
		formatear(t, sizeof t, "%s", estado_texto());
	/* (el estado, en texto; y una marca con su forma: ● encendido, ○ apagado) */
	texto(66, 32, t, e.estado == BTE_FALLO ? ROJO : TINTA);
	boton(14, 64, 120, 30, e.estado == BTE_ENCENDIDO ? "Apagar" : "Encender", Z_ENCENDER, e.estado == BTE_ENCENDIDO || e.estado == BTE_APAGADO);
	boton(144, 64, 170, 30, e.buscando ? "Buscando..." : "Buscar aparatos", Z_BUSCAR, e.estado == BTE_ENCENDIDO && !e.buscando);
	/* la lista */
	rect(14, LISTA_Y - 6, AN - 28, FILAS * FILA_AL + 12, 0x7E97BA);
	rect(15, LISTA_Y - 5, AN - 30, FILAS * FILA_AL + 10, BLANCO);
	if (!e.n)
		texto(30, LISTA_Y + 10, e.estado == BTE_ENCENDIDO ? "Ningún aparato: ponga el suyo en modo de emparejar y pulse «Buscar aparatos»." : "", SUAVE);
	if (elegida >= (int)e.n)
		elegida = e.n - 1;
	if (primera > elegida)
		primera = elegida < 0 ? 0 : elegida;
	if (elegida >= primera + FILAS)
		primera = elegida - FILAS + 1;
	for (i = primera, y = LISTA_Y; i < e.n && y < LISTA_Y + FILAS * FILA_AL; i++, y += FILA_AL) {
		struct bt_fila *g = &e.fila[i];
		if ((int)i == elegida) {
			rect(18, y - 2, AN - 36, FILA_AL - 2, 0x1D4ED8);
			rect(21, y + 1, AN - 42, FILA_AL - 8, 0xDBEAFE);
			negrita(26, y + 9, ">", TINTA);
		}
		formatear(t, sizeof t, "%s (%s)", nombre_fila(g), que_es(g));
		negrita(42, y + 1, t, TINTA);
		como_esta(g, t, sizeof t);
		texto(42, y + 18, t, g->banderas & BL_CONECTADO ? VERDE : SUAVE);
		zona(18, y - 2, AN - 36, FILA_AL - 2, Z_FILA, i);
	}
	if (e.n > FILAS) {
		formatear(t, sizeof t, "%u-%u de %u (las flechas)", primera + 1, primera + FILAS < (int)e.n ? primera + FILAS : (int)e.n, e.n);
		texto(AN - 230, LISTA_Y + FILAS * FILA_AL + 10, t, SUAVE);
	}
	y = AL - 48;
	sel_vale = f && !(f->banderas & BL_LE && !f->clase);
	boton(14, y, 120, 30, f && f->banderas & BL_EMPAREJADO ? "Emparejar" : "Emparejar", Z_EMPAREJAR, sel_vale && e.estado == BTE_ENCENDIDO);
	boton(144, y, 110, 30, "Conectar", Z_CONECTAR, f && f->banderas & BL_EMPAREJADO && !(f->banderas & (BL_CONECTADO | BL_CONECTANDO)) && e.estado == BTE_ENCENDIDO);
	boton(264, y, 100, 30, "Olvidar", Z_OLVIDAR, f && f->banderas & BL_EMPAREJADO);
	boton(AN - 114, y, 100, 30, "Cerrar", Z_CERRAR, 1);
	texto(14, y - 24, e.aviso, TINTA);
	if (e.em != EM_NADA)
		panel_emparejar();
}

static int distinto(struct bt_estado *a)
{
	return memcmp(a, &e, sizeof e) != 0;
}

static void haz(int que)
{
	struct bt_fila *f = elegida >= 0 && elegida < (int)e.n ? &e.fila[elegida] : 0;
	u32 r = 0;

	switch (que) {
	case Z_ENCENDER:
		r = pide(e.estado == BTE_ENCENDIDO ? BO_APAGAR : BO_ENCENDER, 0, 0);
		break;
	case Z_BUSCAR:
		pide(BO_BUSCAR, 0, 0);
		break;
	case Z_EMPAREJAR:
		if (f)
			r = pide(BO_EMPAREJAR, 0, f->dir);
		break;
	case Z_CONECTAR:
		if (f)
			r = pide(BO_CONECTAR, 0, f->dir);
		break;
	case Z_OLVIDAR:
		if (f)
			r = pide(BO_OLVIDAR, 0, f->dir);
		break;
	case Z_SI:
	case Z_NO:
		pide(BO_RESPONDER, que == Z_SI, 0);
		break;
	case Z_CANCELAR:
		pide(BO_CANCELAR, 0, 0);
		break;
	}
	lee();
	if (r == 1)
		formatear(e.aviso, sizeof e.aviso, "Hace falta ser administrador (Configuración > Usuarios)");
	pinta();
}

int ventana(void)
{
	struct suceso s;
	int dato, k;
	u32 ult = h_tics();
	static struct bt_estado a;

	if (!h_hay_escritorio()) {
		h_escribe("BT /VENTANA: solo con el escritorio.\n");
		return 1;
	}
	vn = h_ventana("Bluetooth", AN, AL);
	if (ES_ERROR(vn))
		return 1;
	sc(80, vn, 0xFFFFFFFF, 0);		/* en el centro */
	lee();
	elegida = e.n ? 0 : -1;
	pinta();
	for (;;) {
		if (!h_suceso(vn, &s, 0)) {
			if (h_tics() - ult > h_hz / 2) {
				a = e;
				ult = h_tics();
				lee();
				if (elegida < 0 && e.n)
					elegida = 0;
				if (distinto(&a))
					pinta();
			}
			h_dormir(2);
			continue;
		}
		if (s.clase == EV_CERRAR)
			break;
		if (s.clase == EV_TECLA) {
			if (s.a == 27) {
				if (e.em != EM_NADA && e.em != EM_BIEN && e.em != EM_MAL) {
					haz(Z_CANCELAR);
					continue;
				}
				break;
			}
			if (e.em == EM_COMPARAR && (s.a == 's' || s.a == 'S' || s.a == 'n' || s.a == 'N')) {
				haz(s.a == 's' || s.a == 'S' ? Z_SI : Z_NO);
				continue;
			}
			if (s.a == T_ABAJO && elegida + 1 < (int)e.n)
				elegida++;
			else if (s.a == T_ARRIBA && elegida > 0)
				elegida--;
			else if (s.a == '\n' || s.a == '\r') {
				struct bt_fila *f = elegida >= 0 && elegida < (int)e.n ? &e.fila[elegida] : 0;
				if (f)
					haz(f->banderas & BL_EMPAREJADO ? Z_CONECTAR : Z_EMPAREJAR);
				continue;
			} else if (s.a == T_SUPR) {
				haz(Z_OLVIDAR);
				continue;
			} else if (s.a == 'b' || s.a == 'B') {
				haz(Z_BUSCAR);
				continue;
			}
			pinta();
			continue;
		}
		if (s.clase != EV_RATON_BAJA)
			continue;
		k = zona_en(s.a, s.b, &dato);
		if (k == Z_CERRAR)
			break;
		if (k == Z_FILA) {
			elegida = dato;
			pinta();
		} else if (k != Z_NADA)
			haz(k);
	}
	sc(SC_CERRAR_VENTANA, vn, 0, 0);
	return 0;
}
