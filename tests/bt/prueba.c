/* prueba.c - (BLUETOOTH) la pila de BT.BIN (castel/nucleo32/bt/: hci.c,
 * l2cap.c, intel.c, claves.c) en Linux, contra el modelo de adaptador y
 * aparatos de tests/bt/modelo.c: buscar (BR/EDR y BLE, con el nombre por
 * EIR y preguntado), emparejar con SSP (el código tecleado en el teclado,
 * Just Works con el ratón, la comparación numérica con el teléfono; y mal:
 * el código equivocado y el «no»), con el PIN de antes, SDP, HID (el
 * protocolo de arranque del teclado y el de informes del ratón, con su
 * descriptor), lo que teclea y lo que mueve, BT.INI (y uno cambiado a
 * mano), reconectar al «arrancar otra vez» (el aparato que se despierta y
 * el que se llama), olvidar; y los Intel: el 0A2A sin parche y con uno, y
 * el 0AAA en su cargador, con su firmware (de mentira) y sin él.
 *   build/bt_prueba [CARPETA]     (CARPETA: donde dejar los firmwares de mentira)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "bt.h"
#include "modelo.h"

int claves_texto(char *t, int cabe, const u8 llv[16], int encendido);
int claves_lee(const char *t, const u8 llv[16], int *encendido);

static u32 reloj;
static int fallos, compr, guardados, estados;
static const char *carpeta_fw = "/tmp";
static char ultimo_log[400];

/* ================================================= lo que pone la plataforma */
u32 bt_tics(void) { return reloj; }

int tr_envia(int clase, const u8 *d, int n)
{
	mod_recibe(clase, d, n);
	return n;
}

int tr_recibe(u8 *d, int cabe, int *de, u32 espera)
{
	int n;

	mod_tic(reloj);
	n = mod_saca(d, cabe, de, 16, 64);
	if (n > 0)
		return n;
	espera &= 0xFFFF;
	reloj += espera ? espera : 0;
	if (!espera)
		reloj++;
	mod_tic(reloj);
	return mod_saca(d, cabe, de, 16, 64);
}

void bt_log(const char *f, ...)
{
	va_list va;

	va_start(va, f);
	vsnprintf(ultimo_log, sizeof ultimo_log, f, va);
	va_end(va);
	printf("bt: %s", ultimo_log);
}

int bt_firmware(const char *nombre, u8 *d, u32 cabe)
{
	char r[512];
	FILE *f;
	int n;

	snprintf(r, sizeof r, "%s/%s", carpeta_fw, nombre);
	if (!(f = fopen(r, "rb"))) {
		bt_log("Firmware %s: no está (mirado en %s/)\n", nombre, carpeta_fw);
		return -1;
	}
	n = fread(d, 1, cabe, f);
	fclose(f);
	return n;
}

/* lo que el núcleo recibe para los teclados y los ratones */
static int hid_altas[4], hid_clase[4], hid_n;
static u8 hid_ult[4][16];
static int hid_ult_n[4], hid_desc_n[4];

void bt_hid(int op, int r, int clase, const u8 *d, int n)
{
	if (r < 0 || r > 3) {
		printf("prueba: HID con la ranura %d\n", r);
		fallos++;
		return;
	}
	if (op == HID_ALTA) {
		hid_altas[r]++;
		hid_clase[r] = clase;
		hid_desc_n[r] = n;
		printf("prueba: HID alta %d (%s, descriptor de %d)\n", r, clase == HID_TECLADO ? "teclado" : "ratón", n);
	} else if (op == HID_INFORME) {
		hid_n++;
		memcpy(hid_ult[r], d, n > 16 ? 16 : n);
		hid_ult_n[r] = n;
	} else if (op == HID_BAJA) {
		hid_altas[r] = 0;
		printf("prueba: HID baja %d\n", r);
	}
}

void bt_azar(void *d, u32 n)
{
	u8 *p = d;

	while (n--)
		*p++ = rand();
}

void bt_guarda(void) { guardados++; }
void bt_a_437(char *d, const u8 *s, int n, int cabe)
{
	int i;

	for (i = 0; i < n && i < cabe - 1; i++)
		d[i] = s[i] < 32 ? '?' : s[i];
	d[i] = 0;
}
void bt_estado_cambia(void) { estados++; }

int formatear(char *d, int cabe, const char *p, ...)
{
	va_list va;
	int r;

	va_start(va, p);
	r = vsnprintf(d, cabe, p, va);
	va_end(va);
	return r < cabe ? r : cabe - 1;
}

/* ================================================================ ayudas */
static void mira(int bien, const char *que)
{
	compr++;
	if (bien)
		printf("prueba: BIEN: %s\n", que);
	else {
		fallos++;
		printf("prueba: MAL: %s\n", que);
	}
}

/* da vueltas (hasta «s» segundos de los simulados) hasta que «cond» */
#define HASTA(s, cond) do { u32 _t = reloj; while (!(cond) && reloj - _t < (s) * 100u) bt_vuelta(); } while (0)

static struct aparato *ap(int r)
{
	u8 d[6] = { 0, 0x10, 0xBE, 0xBA, 0xAD, 0xDE };

	d[0] = r + 1;
	d[1] = 0x10 + r;
	return bt_aparato(d, 0);
}

static const u8 *dir_de(int r)
{
	static u8 d[6];

	d[0] = r + 1, d[1] = 0x10 + r, d[2] = 0xBE, d[3] = 0xBA, d[4] = 0xAD, d[5] = 0xDE;
	return d;
}

static void reinicia_pila(void)
{
	memset(aparatos, 0, sizeof aparatos);
	memset(&bt, 0, sizeof bt);
	strcpy(bt.nombre, "Hispa-Pentium (prueba)");
}

static int arranca(int clase)
{
	mod_inicia(clase, carpeta_fw);
	reinicia_pila();
	return bt_arranca(mod_vid(), mod_pid());
}

/* =============================================================== pruebas */
static void prueba_generico(void)
{
	struct aparato *a;
	char ini[16384];
	u8 llv[16] = "llave de prueba";
	int enc = 0, n;

	printf("== el adaptador genérico (0A12:0001) y los aparatos ==\n");
	mira(arranca(MOD_GENERICO) == 0 && bt.listo, "arranca: reset, versión, dirección, lo que sabe, los sucesos, SSP");
	mira(bt.dir[0] == 0x21 && bt.dir[5] == 0x22 && bt.acl_mtu == 310 && bt.acl_max == 10, "la dirección y el tamaño de los ACL");
	mira(bt.rasgos[6] & 0x08 && bt.rasgos[4] & 0x40, "sabe SSP y BLE");
	mira(bt_enciende() == 0 && bt.encendido, "encender (se deja conectar)");
	/* buscar: el teclado en modo de emparejar, el ratón, el teléfono, el antiguo y el BLE */
	mod_emparejable(R_TECLADO, 1);
	mod_emparejable(R_RATON, 1);
	mod_emparejable(R_TELEFONO, 1);
	mod_emparejable(R_ANTIGUO, 1);
	mod_emparejable(R_BLE, 1);
	bt_busca();
	mira(bt.buscando, "buscar");
	HASTA(20, !bt.buscando);
	HASTA(5, ap(R_RATON) && ap(R_RATON)->nombre[0]);
	a = ap(R_TECLADO);
	mira(a && a->visto && !strcmp(a->nombre, "Teclado de prueba") && ((a->clase >> 8) & 0x1F) == 5, "visto el teclado, con su nombre por EIR");
	a = ap(R_RATON);
	mira(a && a->visto && !strcmp(a->nombre, "Ratón de prueba"), "visto el ratón, y su nombre preguntado (Remote Name Request)");
	a = ap(R_TELEFONO);
	mira(a && a->visto, "visto el teléfono");
	a = ap(R_BLE);
	mira(a && a->le && a->hid_le && a->aspecto == 0x03C2 && !strcmp(a->nombre, "Ratón BLE"), "visto el ratón BLE (HID por GATT, apariencia de ratón)");
	{
		struct bt_estado e;
		memset(&e, 0, sizeof e);
		bt_lista(&e);
		mira(e.n >= 5, "la lista para la ventana");
	}
	mira(bt_empareja(dir_de(R_BLE)) < 0, "el BLE no se empareja aún (la etapa 2 b)");

	/* el teclado: el código, a teclear en él */
	mira(bt_empareja(dir_de(R_TECLADO)) == 0, "emparejar el teclado: empieza");
	HASTA(10, bt.em == EM_TECLEAR);
	mira(bt.em == EM_TECLEAR && bt.em_codigo >= 100000, "pide teclear el código en el teclado (User Passkey Notification)");
	mira(mod_teclea_codigo(R_TECLADO, bt.em_codigo) == 0, "la persona teclea el código y pulsa Intro");
	HASTA(10, bt.em_tecleados == 6 || bt.em == EM_BIEN);
	HASTA(20, ap(R_TECLADO)->estado == A_LISTO);
	a = ap(R_TECLADO);
	mira(a->emparejado && a->tipo_clave == 5, "emparejado: la clave de enlace guardada");
	mira(a->es_hid && a->boot && a->desc_n == 65 && a->hid_clase == HID_TECLADO, "SDP: es HID, con su descriptor (65 octetos, en dos respuestas) y el protocolo de arranque");
	mira(a->estado == A_LISTO && mod_protocolo(R_TECLADO) == 0, "HID: los dos canales abiertos y el protocolo de arranque puesto");
	mira(mod_tecla(R_TECLADO, 0x02, 0x04) == 0, "el teclado manda Mayús+A");
	HASTA(2, hid_n > 0);
	mira(hid_altas[0] == 1 && hid_clase[0] == HID_TECLADO && hid_ult_n[0] == 8 && hid_ult[0][0] == 0x02 && hid_ult[0][2] == 0x04,
	     "al núcleo: un teclado (la ranura 0) y su informe de 8 octetos (Mayús, A)");
	mod_tecla(R_TECLADO, 0, 0);
	HASTA(2, hid_ult[0][2] == 0);
	mira(hid_ult[0][2] == 0, "y al soltar, el informe vacío");

	/* el ratón: Just Works y su descriptor */
	mira(bt_empareja(dir_de(R_RATON)) == 0, "emparejar el ratón: empieza");
	HASTA(20, ap(R_RATON)->estado == A_LISTO);
	a = ap(R_RATON);
	mira(a->emparejado && a->tipo_clave == 4 && bt.em != EM_TECLEAR && bt.em != EM_COMPARAR, "el ratón, emparejado con Just Works (sin preguntar nada)");
	mira(a->es_hid && !a->boot && a->desc_n == 54 && mod_protocolo(R_RATON) == 1, "el ratón: sin protocolo de arranque: el de informes, con su descriptor");
	mod_mueve(R_RATON, 5, -3, 1, 1);
	HASTA(2, hid_altas[1]);
	HASTA(2, hid_ult_n[1]);
	mira(hid_altas[1] == 1 && hid_clase[1] == HID_RATON && hid_desc_n[1] == 54 && hid_ult_n[1] == 5 && hid_ult[1][0] == 2 && hid_ult[1][1] == 1 &&
	     hid_ult[1][2] == 5 && hid_ult[1][3] == (u8)-3, "al núcleo: un ratón con su descriptor y su informe (el 2: botón, +5, -3, la rueda)");

	/* el teléfono: comparación numérica; primero «no» y luego «sí» */
	bt_empareja(dir_de(R_TELEFONO));
	HASTA(10, bt.em == EM_COMPARAR);
	mira(bt.em == EM_COMPARAR, "el teléfono: pregunta si sale el mismo número (comparación numérica)");
	bt_responde(0);
	mod_acepta(R_TELEFONO, 1);
	HASTA(10, bt.em == EM_MAL);
	mira(bt.em == EM_MAL && !ap(R_TELEFONO)->emparejado, "con «No», no se empareja");
	HASTA(20, bt.em == EM_NADA && ap(R_TELEFONO)->estado == A_NADA);
	mod_emparejable(R_TELEFONO, 1);
	bt_empareja(dir_de(R_TELEFONO));
	HASTA(10, bt.em == EM_COMPARAR);
	bt_responde(1);
	mod_acepta(R_TELEFONO, 1);
	HASTA(20, ap(R_TELEFONO)->estado == A_LISTO);
	a = ap(R_TELEFONO);
	mira(a->emparejado && !a->es_hid, "con «Sí», emparejado; y no es un teclado ni un ratón (no se usa)");

	/* el antiguo: el PIN, a teclear */
	bt_empareja(dir_de(R_ANTIGUO));
	HASTA(10, bt.em == EM_PIN);
	mira(bt.em == EM_PIN && bt.em_codigo >= 100000, "el teclado antiguo: un PIN a teclear en él");
	mod_teclea_codigo(R_ANTIGUO, bt.em_codigo);
	HASTA(20, ap(R_ANTIGUO)->estado == A_LISTO);
	mira(ap(R_ANTIGUO)->emparejado && mod_protocolo(R_ANTIGUO) == 0, "emparejado con el PIN; HID con el de arranque");

	/* lo que se guarda */
	n = claves_texto(ini, sizeof ini, llv, 1);
	mira(n > 0 && strstr(ini, "Encendido=SI") && strstr(ini, "; Teclado de prueba (teclado)") && strstr(ini, "Descriptor=011"),
	     "BT.INI: los emparejados, con su nombre a la vista y su descriptor");
	{
		char h[40];
		int i;
		for (i = 0; i < 16; i++)
			sprintf(h + 2 * i, "%02X", ap(R_TECLADO)->clave[i]);
		mira(!strstr(ini, h), "BT.INI: la clave no está a la vista");
	}

	/* arrancar otra vez: los aparatos, desconectados (el adaptador se reinicia) */
	printf("== arrancar otra vez ==\n");
	mod_inicia(MOD_GENERICO, 0);
	/* (el modelo se ha puesto como nuevo: los aparatos tienen que recordar la clave) */
	reinicia_pila();
	mira(claves_lee(ini, llv, &enc) == 4 && enc, "BT.INI leído: 4 emparejados y encendido");
	mira(bt_arranca(mod_vid(), mod_pid()) == 0 && bt_enciende() == 0, "arranca otra vez");
	printf("prueba: (el modelo se ha olvidado de las claves al rehacerse: se las vuelve a dar)\n");
	return;
}

/* el modelo, como nuevo, no recuerda las claves: esta prueba empareja y
 * reconecta en el mismo modelo, con la pila rehecha (lo que pasa al apagar
 * y encender el equipo: el aparato sigue con su clave) */
static void prueba_reconectar(void)
{
	char ini[16384], malo[16384];
	u8 llv[16] = "llave de prueba", otra[16] = "otra llave mala";
	int enc = 0;
	struct aparato *a;
	char *p;

	printf("== reconectar al arrancar otra vez ==\n");
	arranca(MOD_GENERICO);
	bt_enciende();
	mod_emparejable(R_TECLADO, 1);
	mod_emparejable(R_RATON, 1);
	bt_busca();
	HASTA(20, !bt.buscando);
	bt_empareja(dir_de(R_TECLADO));
	HASTA(10, bt.em == EM_TECLEAR);
	mod_teclea_codigo(R_TECLADO, bt.em_codigo);
	HASTA(20, ap(R_TECLADO)->estado == A_LISTO);
	bt_empareja(dir_de(R_RATON));
	HASTA(20, ap(R_RATON)->estado == A_LISTO);
	mira(ap(R_TECLADO)->estado == A_LISTO && ap(R_RATON)->estado == A_LISTO, "emparejados el teclado y el ratón");
	claves_texto(ini, sizeof ini, llv, 1);
	/* «apagar»: se cortan las conexiones (el modelo sigue, con los aparatos y sus claves) */
	bt_apaga();
	mira(!mod_conectado(R_TECLADO) && !mod_conectado(R_RATON), "apagar: se desconectan");
	memset(hid_altas, 0, sizeof hid_altas);
	hid_n = 0;
	reinicia_pila();
	mira(claves_lee(ini, llv, &enc) == 2, "BT.INI leído: los dos");
	mira(bt_arranca(mod_vid(), mod_pid()) == 0 && bt_enciende() == 0, "arranca otra vez (reset)");
	/* el teclado se despierta (una tecla): se conecta él y abre sus canales antes de cifrar */
	mod_despierta(R_TECLADO);
	HASTA(20, ap(R_TECLADO)->estado == A_LISTO && mod_conectado(R_TECLADO));
	HASTA(3, 0);
	mira(mod_conectado(R_TECLADO) && ap(R_TECLADO)->cifrado, "el teclado se conecta solo: aceptado, con la clave guardada, cifrado");
	mira(mod_protocolo(R_TECLADO) == 0, "sus canales (pedidos antes de cifrar: «pendiente» y luego sí) y el protocolo de arranque");
	mira(mod_tecla(R_TECLADO, 0, 0x1E) == 0, "teclea un 1");
	HASTA(2, hid_n > 0);
	mira(hid_n > 0 && hid_ult[0][2] == 0x1E, "al núcleo, el 1");
	/* el ratón no se despierta: se le llama al arrancar */
	bt_reconecta_todos();
	HASTA(30, ap(R_RATON)->estado == A_LISTO);
	mira(ap(R_RATON)->estado == A_LISTO && mod_protocolo(R_RATON) == 1, "el ratón: se le llama (reconectar al arrancar), sin volver a preguntar su SDP");
	/* uno que no está: no se queda colgado */
	/* BT.INI cambiado a mano: la clave del teclado, con un octeto de otro */
	strcpy(malo, ini);
	p = strstr(malo, "Aparato=01");
	if (p) {
		p = strchr(p, '\r') - 3;
		*p = *p == 'A' ? 'B' : 'A';
	}
	reinicia_pila();
	mira(claves_lee(malo, llv, &enc) == 1, "BT.INI con la clave del teclado cambiada: esa no se usa (la firma)");
	reinicia_pila();
	mira(claves_lee(ini, otra, &enc) == 0, "BT.INI con otra BT.LLV: ninguna se usa");
	/* olvidar el teclado: VIRTUAL_CABLE_UNPLUG y se borra su clave */
	reinicia_pila();
	claves_lee(ini, llv, &enc);
	bt_arranca(mod_vid(), mod_pid());
	bt_enciende();
	mod_despierta(R_TECLADO);
	HASTA(20, ap(R_TECLADO)->estado == A_LISTO);
	a = ap(R_TECLADO);
	mira(bt_olvida(a->dir) == 0 && !a->emparejado, "olvidar el teclado");
	HASTA(5, !mod_conectado(R_TECLADO));
	mira(!mod_emparejado(R_TECLADO) && !mod_conectado(R_TECLADO), "el teclado se entera («virtual cable unplug») y se desconecta");
	claves_texto(ini, sizeof ini, llv, 1);
	mira(!strstr(ini, "Aparato=011"), "y ya no está en BT.INI");
	mod_despierta(R_TECLADO);
	HASTA(5, 0);
	mira(!mod_conectado(R_TECLADO), "ya no se le deja conectar");
}

static void prueba_malos(void)
{
	printf("== lo que sale mal ==\n");
	arranca(MOD_GENERICO);
	bt_enciende();
	mod_emparejable(R_TECLADO, 1);
	bt_busca();
	HASTA(20, !bt.buscando);
	bt_empareja(dir_de(R_TECLADO));
	HASTA(10, bt.em == EM_TECLEAR);
	mod_teclea_codigo(R_TECLADO, (bt.em_codigo + 1) % 1000000);
	HASTA(10, bt.em == EM_MAL);
	mira(bt.em == EM_MAL && !ap(R_TECLADO)->emparejado, "el código mal tecleado: no se empareja y se dice");
	mira(strstr(bt.aviso, "no se ha aceptado el código") != 0, "el aviso: «no se ha aceptado el código»");
	/* uno que no está en modo de emparejar */
	HASTA(20, bt.em == EM_NADA);
	mod_emparejable(R_TECLADO, 0);
	mod_apaga(R_TECLADO);
	bt_empareja(dir_de(R_TECLADO));
	HASTA(15, bt.em == EM_MAL);
	mira(bt.em == EM_MAL && strstr(bt.aviso, "no contesta"), "uno que no contesta: «no contesta (¿está en modo de emparejar?)»");
}

static int escribe(const char *nombre, const u8 *d, int n)
{
	char r[512];
	FILE *f;

	snprintf(r, sizeof r, "%s/%s", carpeta_fw, nombre);
	if (!(f = fopen(r, "wb")))
		return -1;
	fwrite(d, 1, n, f);
	fclose(f);
	return 0;
}

static void borra(const char *nombre)
{
	char r[512];

	snprintf(r, sizeof r, "%s/%s", carpeta_fw, nombre);
	remove(r);
}

static void prueba_intel(void)
{
	static u8 sfi[300000];
	int n = 0, i, k;

	printf("== Intel ==\n");
	borra("ibt-hw-37.8.10-fw-1.10.3.11.e.bseq");
	borra("ibt-hw-37.8.bseq");
	mira(arranca(MOD_INTEL_ROM) == 0 && bt.listo && mod_parche == 0, "8087:0A2A (de ROM): arranca sin parche, como Linux");
	{
		/* un parche: 0xFC8E con su Command Complete (01 8E FC 00) */
		u8 b[] = { 0x01, 0x8E, 0xFC, 0x02, 0x11, 0x22, 0x02, 0x0E, 0x04, 0x01, 0x8E, 0xFC, 0x00 };
		escribe("ibt-hw-37.8.10-fw-1.10.3.11.e.bseq", b, sizeof b);
		mira(arranca(MOD_INTEL_ROM) == 0 && bt.listo && mod_parche >= 3, "con su parche (ibt-hw-37.8.10-fw-1.10.3.11.e.bseq): modo de fabricante, la orden y fuera");
		borra("ibt-hw-37.8.10-fw-1.10.3.11.e.bseq");
	}
	/* el 0AAA, en su cargador: sin firmware */
	borra("ibt-17-16-1.sfi");
	borra("ibt-17-16-1.ddc");
	mira(arranca(MOD_INTEL_CARGADOR) < 0 && strstr(bt.aviso, "ibt-17-16-1.sfi"), "8087:0AAA sin su firmware: no arranca y dice cuál falta (ibt-17-16-1.sfi)");
	/* con uno de mentira: la cabecera (644) y órdenes con su NOP para el múltiplo de 4 */
	for (i = 0; i < 644; i++)
		sfi[n++] = i * 7;
	/* la de arranque (0xFC0E): la dirección 0x00A55A00 */
	sfi[n++] = 0x0E, sfi[n++] = 0xFC, sfi[n++] = 7;
	sfi[n++] = 0x00, sfi[n++] = 0x5A, sfi[n++] = 0xA5, sfi[n++] = 0x00, sfi[n++] = 1, sfi[n++] = 2, sfi[n++] = 3;
	sfi[n++] = 0x02, sfi[n++] = 0xFC, sfi[n++] = 2, sfi[n++] = 0, sfi[n++] = 0;	/* (10 + 5 = 15... + NOP) */
	sfi[n++] = 0x02, sfi[n++] = 0xFC, sfi[n++] = 2, sfi[n++] = 0, sfi[n++] = 0;
	for (k = 0; k < 600; k++) {
		int pl = 20 + k % 200, m = 3 + pl;
		sfi[n++] = 0x8E, sfi[n++] = 0xFC, sfi[n++] = pl;
		for (i = 0; i < pl; i++)
			sfi[n++] = k + i;
		/* un NOP que deja el trozo en múltiplo de 4 */
		m = (4 - (m + 3) % 4) % 4;
		sfi[n++] = 0x02, sfi[n++] = 0xFC, sfi[n++] = m;
		for (i = 0; i < m; i++)
			sfi[n++] = 0;
	}
	escribe("ibt-17-16-1.sfi", sfi, n);
	{
		u8 ddc[] = { 3, 0x01, 0x02, 0x33, 4, 0x05, 0x00, 0x01, 0x02 };
		escribe("ibt-17-16-1.ddc", ddc, sizeof ddc);
	}
	printf("prueba: firmware de mentira de %d octetos\n", n);
	i = arranca(MOD_INTEL_CARGADOR);
	mira(mod_firmware_bien, "8087:0AAA: el firmware (ibt-17-16-1.sfi) llega entero y en orden, por el extremo a granel, con la cabecera, la llave y la firma");
	mira(i == 0 && bt.listo, "arranca su firmware (0xFC01 con la dirección de la 0xFC0E) y luego el reset de siempre");
	mira(mod_ddc == 2, "su configuración (ibt-17-16-1.ddc: 2 partes)");
	borra("ibt-17-16-1.sfi");
	borra("ibt-17-16-1.ddc");
}

int main(int argc, char **argv)
{
	if (argc > 1)
		carpeta_fw = argv[1];
	setvbuf(stdout, 0, _IONBF, 0);
	prueba_generico();
	prueba_reconectar();
	prueba_malos();
	prueba_intel();
	printf("prueba: el modelo ha visto %d cosas mal hechas\n", mod_fallos);
	if (mod_fallos)
		fallos++;
	printf("Bluetooth en el anfitrión: %d comprobaciones, %d fallos\n", compr, fallos);
	return fallos != 0;
}
