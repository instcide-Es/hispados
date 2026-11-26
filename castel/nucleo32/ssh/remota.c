/* remota.c - (REMOTA) «Dejar entrar a Claude»: la consola remota de
 * Hispa-Pentium (SSHD /REMOTA), la de CONSOLA de Hispa-DOS 386/486 pasada a
 * Pentium y cifrada: es el servidor SSH de SSHD.BIN (curve25519,
 * ssh-ed25519, chacha20-poly1305), pero
 *
 *   - solo escucha MIENTRAS SU VENTANA ESTÁ ABIERTA (en el puerto 2323, el
 *     de la consola de Hispa-486: el mismo reenvío del router); al cerrarla,
 *     el puerto se cierra (las escuchas de un programa acaban con él);
 *   - la «contraseña» es un CÓDIGO DE 6 CIFRAS que solo se ve en la
 *     pantalla, de un solo uso (una conexión) y que vale 60 s; sin código
 *     vigente no entra nadie. Tres fallos con un código: otro código y una
 *     pausa sin aceptar a nadie (30 s, y el doble cada vez, hasta 16 min);
 *   - solo la abre un administrador (o con el nombre y la contraseña de uno);
 *     el intérprete de dentro es de ese administrador;
 *   - quien entra, cuándo y lo que hace se apunta en \HISPA\REMOTA.LOG, y en
 *     la ventana se ve quién está dentro, con un botón «Echar».
 *
 * Dentro: la Consola (ORDENES), órdenes sueltas (ssh ... orden) y copiar
 * archivos (remota:leer RUTA y remota:escribir RUTA, en sshd.c). El cliente
 * del otro lado: tools/remota_pentium.py (OpenSSH con ControlMaster: un
 * código para toda una visita). docs/PENTIUM.md, «La consola remota».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ssh.h"
#include "remota.h"
#include "cripto.h"
#include "monocypher.h"

int remota;

#define V_ANCHO 600
#define V_ALTO 470
#define FONDO 0xF4F8FE
#define TINTA 0x1A2A44
#define SUAVE 0x5A6E8C
#define AZUL 0x2F74D0
#define ROJO 0xC0504D
#define VERDE 0x3FA24C

static u32 puerto = 2323, caduca = 60;
static u32 v;				/* la ventana (0: en texto) */
static char codigo[8];			/* el vigente («» si no hay) */
static u32 cod_desde, previo;
static int fallos;			/* con el código vigente */
static u32 pausa_hasta, pausa_s;	/* sin aceptar a nadie hasta ese tic */
static int en_pausa;
static int echar, cerrar;
static int dentro;			/* hay alguien conectado */
static char dentro_txt[120];
static u32 dentro_desde;
static char ip_txt[24];
static struct ficha_u adm;		/* el administrador que la abrió */
static int con_adm;
static char huella[64];
static char eventos[4][96];
static u32 ult_pinta;
static u32 ult_ip;
static char ip_local[20], ip_ts[20];

/* ---- la hora y el registro ---- */
static void hora(char *t, int cabe, int con_fecha)
{
	u32 f = sc(SC_DATO, DATO_FECHA, 0, 0), d = f >> 16, h = f & 0xFFFF;

	if (con_fecha)
		formatear(t, cabe, "%02u/%02u/%04u %02u:%02u:%02u", d & 31, d >> 5 & 15, (d >> 9) + 1980, h >> 11, h >> 5 & 63, (h & 31) * 2);
	else
		formatear(t, cabe, "%02u:%02u", h >> 11, h >> 5 & 63);
}

static int al_final(const char *ruta, const char *t, u32 n)
{
	u32 a = sc(SC_ABRIR, (u32)ruta, 3, 0), r;

	if (ES_ERROR(a))
		a = sc(SC_ABRIR, (u32)ruta, 2, 0);
	if (ES_ERROR(a))
		return -1;
	r = sc(SC_ESCRIBIR_ARCHIVO, a, (u32)t, n);
	sc(SC_CERRAR, a, 0, 0);
	return r == n ? 0 : -1;
}

static void pinta(void);

/* una línea en REMOTA.LOG (\HISPA; con una base de antes, que no lo deja,
 * en \HISPA\SSH), en la lista de la ventana y en el registro */
static void apunta(const char *f, ...)
{
	char l[300], t[360], r[80], hh[24];
	__builtin_va_list va;
	int i;

	__builtin_va_start(va, f);
	vformatear(l, sizeof l, f, va);
	__builtin_va_end(va);
	h_apunta("remota: %s", l);
	hora(hh, sizeof hh, 1);
	formatear(t, sizeof t, "%s  %s\r\n", hh, l);
	h_ruta(r, sizeof r, "REMOTA.LOG");
	if (al_final(r, t, strlen(t)) < 0) {
		h_ruta(r, sizeof r, "SSH\\REMOTA.LOG");
		al_final(r, t, strlen(t));
	}
	for (i = 0; i < 3; i++)
		memcpy(eventos[i], eventos[i + 1], sizeof eventos[0]);
	hora(hh, sizeof hh, 0);
	formatear(eventos[3], sizeof eventos[3], "%s %s", hh, l);
}

/* ---- el código ---- */
static void nuevo_codigo(const char *por_que)
{
	u32 x, lim = 0xFFFFFFFFu - (0xFFFFFFFFu % 900000u) - 1;

	do {
		do
			h_azar(&x, 4);
		while (x > lim);
		x = 100000 + x % 900000;
	} while (x == previo);
	previo = x;
	formatear(codigo, sizeof codigo, "%06u", x);
	x = 0;
	cod_desde = h_tics();
	fallos = 0;
	(void)por_que;
	pinta();
}

static void sin_codigo(void)
{
	crypto_wipe(codigo, sizeof codigo);
	codigo[0] = 0;
}

int remota_codigo(const char *usuario, const u8 *c, u32 n)
{
	char d[8];
	u32 i, k = 0, dif = 0;

	if (!codigo[0] || en_pausa)
		return -1;
	if ((h_tics() - cod_desde) >= caduca * h_hz) {
		apunta("código caducado desde %s (%s)", ip_txt, usuario);
		h_dormir(2 * h_hz);
		return 0;
	}
	for (i = 0; i < n && k < 7; i++)
		if (c[i] >= '0' && c[i] <= '9')
			d[k++] = c[i];
		else if (c[i] != ' ' && c[i] != '-')
			k = 7;		/* (lo que no es una cifra: no vale) */
	if (k != 6)
		dif = 1;
	/* las 6 enteras, sin pararse en la primera distinta */
	for (i = 0; i < 6; i++)
		dif |= (u32)(u8)(i < k ? d[i] : 0) ^ (u8)codigo[i];
	crypto_wipe(d, sizeof d);
	if (!dif) {
		sin_codigo();	/* (gastado: uno por conexión) */
		return 1;
	}
	fallos++;
	apunta("código erróneo desde %s (%d de 3 con este código)", ip_txt, fallos);
	h_dormir(2 * h_hz);
	if (fallos >= 3) {
		pausa_s = pausa_s ? pausa_s * 2 : 30;
		if (pausa_s > 960)
			pausa_s = 960;
		pausa_hasta = h_tics() + pausa_s * h_hz;
		en_pausa = 1;
		sin_codigo();
		apunta("tres códigos erróneos: otro código dentro de %u s (mientras, no entra nadie)", pausa_s);
		pinta();
		return -1;
	}
	return 0;
}

/* ---- quién está dentro ---- */
void remota_entra(const char *usuario)
{
	char hh[8], u[25], cli[61];

	h_copia(u, usuario[0] ? usuario : "(sin nombre)", sizeof u);
	h_copia(cli, S.v_cli, sizeof cli);
	pausa_s = 0;
	dentro = 1;
	dentro_desde = h_tics();
	hora(hh, sizeof hh, 0);
	formatear(dentro_txt, sizeof dentro_txt, "%s desde %s (a las %s)", u, ip_txt, hh);
	apunta("ENTRA %s desde %s (%s)", u, ip_txt, cli);
	pinta();
}

void remota_hace(const char *que)
{
	char t[201];

	h_copia(t, que, sizeof t);
	apunta("  hace: %s", t);
}

void remota_lanza(void)
{
	if (con_adm)
		entra_usuario(&adm, 1, 0, 0);
}

/* ---- las IP ---- */
static void mira_ips(void)
{
	char r[80], e[24];
	static char t[4096];
	u32 ip = h_mi_ip();
	int n;

	if (ip)
		h_ip_a_texto(ip, ip_local);
	else
		h_copia(ip_local, "(sin red todavía)", sizeof ip_local);
	ip_ts[0] = 0;
	h_ruta(r, sizeof r, "TS\\ESTADO.TXT");
	n = h_lee_archivo(r, t, sizeof t - 1);
	if (n > 0) {
		t[n] = 0;
		if (h_ini(t, "estado", e, sizeof e) && !strcmp(e, "conectado"))
			h_ini(t, "ip", ip_ts, sizeof ip_ts);
	}
}

/* ---- la ventana ---- */
struct boton { int x, y, an, al, accion; };
static struct boton botones[3];
static int n_botones;

static void texto(int x, int y, const char *t, u32 tinta)
{
	h_texto_fondo(v, x, y, t, strlen(t), tinta, FONDO);
}

static void boton(int x, int y, int an, const char *t, int accion, u32 color)
{
	int n = strlen(t);

	h_rectangulo(v, x, y, an, 28, 0x2468C8);
	h_rectangulo(v, x + 1, y + 1, an - 2, 26, color);
	h_texto_fondo(v, x + (an - n * 8) / 2, y + 6, t, n, 0xFFFFFF, color);
	if (n_botones < 3)
		botones[n_botones++] = (struct boton){ x, y, an, 28, accion };
}

/* las cifras grandes: 5 x 7 cuadros */
static const u8 cifras[10][7] = {
	{ 14, 17, 19, 21, 25, 17, 14 }, { 4, 12, 4, 4, 4, 4, 14 }, { 14, 17, 1, 2, 4, 8, 31 }, { 31, 2, 4, 2, 1, 17, 14 },
	{ 2, 6, 10, 18, 31, 2, 2 }, { 31, 16, 30, 1, 1, 17, 14 }, { 6, 8, 16, 30, 17, 17, 14 }, { 31, 1, 2, 4, 8, 8, 8 },
	{ 14, 17, 17, 14, 17, 17, 14 }, { 14, 17, 17, 15, 1, 2, 12 },
};

static void cifra_grande(int x, int y, int c, int e, u32 color)
{
	int f, k;

	for (f = 0; f < 7; f++)
		for (k = 0; k < 5; k++)
			if (cifras[c][f] >> (4 - k) & 1)
				h_rectangulo(v, x + k * e, y + f * e, e, e, color);
}

#define Y_COD 96
static void pinta_cuenta(void)
{
	char t[80];
	u32 pasa, queda, ancho = 400;

	if (!v)
		return;
	h_rectangulo(v, 100, Y_COD + 70, ancho, 32, FONDO);
	if (en_pausa) {
		queda = (int)(pausa_hasta - h_tics()) > 0 ? (pausa_hasta - h_tics()) / h_hz + 1 : 0;
		formatear(t, sizeof t, "Demasiados códigos erróneos: otro dentro de %u s", queda);
		h_texto_fondo(v, (V_ANCHO - strlen(t) * 8) / 2, Y_COD + 74, t, strlen(t), ROJO, FONDO);
		return;
	}
	if (!codigo[0])
		return;
	pasa = (h_tics() - cod_desde) / h_hz;
	queda = pasa < caduca ? caduca - pasa : 0;
	h_rectangulo(v, 100, Y_COD + 70, ancho, 8, 0xD8E2F0);
	h_rectangulo(v, 100, Y_COD + 70, ancho * queda / caduca, 8, queda > 10 ? VERDE : ROJO);
	formatear(t, sizeof t, "Caduca en %u s", queda);
	h_texto_fondo(v, (V_ANCHO - strlen(t) * 8) / 2, Y_COD + 84, t, strlen(t), SUAVE, FONDO);
}

static void pinta(void)
{
	char t[160];
	int i, x;

	if (!v)
		return;
	n_botones = 0;
	h_rectangulo(v, 0, 0, V_ANCHO, V_ALTO, FONDO);
	h_rectangulo(v, 0, 0, V_ANCHO, 40, AZUL);
	h_texto_fondo(v, 16, 12, "Dejar entrar a Claude (consola remota)", 38, 0xFFFFFF, AZUL);
	texto(16, 52, "Mientras esta ventana esté abierta, quien tenga el código puede entrar", TINTA);
	texto(16, 70, "en la Consola de este equipo. Al cerrarla, nadie más puede entrar.", TINTA);
	/* el código, grande */
	h_rectangulo(v, 90, Y_COD - 6, V_ANCHO - 180, 66, 0xFFFFFF);
	if (codigo[0] && !dentro) {
		x = (V_ANCHO - (6 * 42 + 18)) / 2;
		for (i = 0; i < 6; i++)
			cifra_grande(x + i * 42 + (i >= 3 ? 18 : 0), Y_COD, codigo[i] - '0', 7, TINTA);
	} else if (dentro)
		texto((V_ANCHO - 30 * 8) / 2, Y_COD + 20, "(el código ya se ha usado)", SUAVE);
	else if (!en_pausa)
		texto((V_ANCHO - 18 * 8) / 2, Y_COD + 20, "(preparando...)", SUAVE);
	pinta_cuenta();
	texto(16, Y_COD + 108, "Dígale el código a Claude: vale 60 segundos y para una sola conexión.", TINTA);
	formatear(t, sizeof t, "Este equipo: %s   puerto %u", ip_local, puerto);
	texto(16, Y_COD + 132, t, TINTA);
	if (ip_ts[0]) {
		formatear(t, sizeof t, "Tailscale: %s (sin abrir puertos en el router)", ip_ts);
		texto(16, Y_COD + 150, t, TINTA);
	}
	formatear(t, sizeof t, "Huella: %s", huella);
	texto(16, Y_COD + 168, t, SUAVE);
	/* quién está dentro */
	h_rectangulo(v, 16, Y_COD + 192, V_ANCHO - 32, 1, 0xA9C6EC);
	if (dentro) {
		h_rectangulo(v, 16, Y_COD + 207, 10, 10, 0x2ECC40);
		texto(32, Y_COD + 204, "CONECTADO:", TINTA);
		texto(120, Y_COD + 204, dentro_txt, TINTA);
		boton(V_ANCHO - 136, Y_COD + 228, 120, "Echar", 1, ROJO);
	} else {
		h_rectangulo(v, 16, Y_COD + 207, 10, 10, 0x9AA8B8);
		texto(32, Y_COD + 204, "Nadie conectado.", TINTA);
	}
	for (i = 0; i < 4; i++)
		if (eventos[i][0])
			h_texto_fondo(v, 16, Y_COD + 264 + i * 18, eventos[i], strlen(eventos[i]) > 70 ? 70 : strlen(eventos[i]), SUAVE, FONDO);
	boton(V_ANCHO - 136, V_ALTO - 40, 120, "Cerrar", 2, AZUL);
	ult_pinta = h_tics();
}

int remota_vigila(void)
{
	struct suceso e;

	if (v) {
		while (h_suceso(v, &e, 0)) {
			if (e.clase == EV_CERRAR || (e.clase == EV_TECLA && e.a == 27))
				cerrar = 1;
			if (e.clase == EV_TECLA && (e.a == 'e' || e.a == 'E') && dentro)
				echar = 1;		/* (Alt+E no hace falta: la E sola, como el botón) */
			if (e.clase == EV_RATON_BAJA) {
				int i;
				for (i = 0; i < n_botones; i++) {
					struct boton *b = &botones[i];
					if ((int)e.a >= b->x && (int)e.a < b->x + b->an && (int)e.b >= b->y && (int)e.b < b->y + b->al) {
						if (b->accion == 1 && dentro)
							echar = 1;
						if (b->accion == 2)
							cerrar = 1;
					}
				}
			}
		}
	} else {
		u32 k;
		while ((k = h_tecla(0)) != 0) {
			if (k == 27)
				cerrar = 1;
			if ((k == 'e' || k == 'E') && dentro)
				echar = 1;
		}
	}
	/* lo que pasa con el tiempo */
	if (en_pausa && (int)(pausa_hasta - h_tics()) <= 0) {
		en_pausa = 0;
		apunta("pasada la pausa: otro código");
		nuevo_codigo("Ya ha pasado la pausa.");
	}
	if (!dentro && codigo[0] && h_tics() - cod_desde >= caduca * h_hz) {
		apunta("el código ha caducado sin usarse: otro");
		nuevo_codigo("El código anterior ha caducado. El nuevo:");
	}
	if (h_tics() - ult_pinta >= h_hz) {
		u32 ip = h_mi_ip();
		if (ip != ult_ip) {
			ult_ip = ip;
			mira_ips();
			pinta();
		} else {
			ult_pinta = h_tics();
			pinta_cuenta();
		}
	}
	if (echar && dentro) {
		echar = 0;
		apunta("ECHADO por el dueño del equipo");
		return 1;
	}
	return cerrar;
}

/* ---- solo un administrador (USUARIOS) ---- */
static int busca_yo(struct ficha_u *f)
{
	u32 i;

	for (i = 0; i < 16 && sc(SC_USUARIOS, 1, i, (u32)f) == 1; i++)
		if (f->es_yo)
			return 1;
	return 0;
}

/* comprueba el nombre y la contraseña (en la página 437) de un administrador */
static int vale_admin(const char *nom437, const char *cla437)
{
	char nu[80];
	u8 cu[240];
	int i, k = 0;

	for (i = 0; nom437[i] && k < 70; i++)
		k += h_utf8_de_437((u8)nom437[i], nu + k);
	nu[k] = 0;
	if (!busca_usuario(nu, &adm) || !adm.admin)
		return 0;
	for (i = 0, k = 0; cla437[i] && k < 230; i++)
		k += h_utf8_de_437((u8)cla437[i], (char *)cu + k);
	i = entra_usuario(&adm, 0, cu, k);
	crypto_wipe(cu, sizeof cu);
	return i;
}

/* en la ventana: el nombre y la contraseña de un administrador */
static int pide_admin_ventana(void)
{
	char nom[24] = "", cla[64] = "", t[80];
	int campo = 0, nn = 0, nc = 0, fallo = 0, i;
	struct suceso e;

	for (;;) {
		h_rectangulo(v, 0, 0, V_ANCHO, V_ALTO, FONDO);
		h_rectangulo(v, 0, 0, V_ANCHO, 40, AZUL);
		h_texto_fondo(v, 16, 12, "Dejar entrar a Claude (consola remota)", 38, 0xFFFFFF, AZUL);
		texto(16, 60, "Esto solo lo puede abrir un administrador del equipo.", TINTA);
		texto(16, 80, "Escriba el nombre y la contraseña de un administrador:", TINTA);
		texto(40, 124, "Nombre:", TINTA);
		texto(40, 164, "Contraseña:", TINTA);
		h_rectangulo(v, 150, 118, 260, 28, campo == 0 ? AZUL : 0xA9C6EC);
		h_rectangulo(v, 152, 120, 256, 24, 0xFFFFFF);
		h_texto_fondo(v, 158, 124, nom, nn, TINTA, 0xFFFFFF);
		h_rectangulo(v, 150, 158, 260, 28, campo == 1 ? AZUL : 0xA9C6EC);
		h_rectangulo(v, 152, 160, 256, 24, 0xFFFFFF);
		for (i = 0; i < nc && i < 30; i++)
			t[i] = '*';
		h_texto_fondo(v, 158, 164, t, i, TINTA, 0xFFFFFF);
		if (fallo)
			texto(40, 204, "Ese no es un administrador, o la contraseña no es esa.", ROJO);
		texto(40, 240, "Tab: el otro campo. Intro: aceptar. Esc: no abrirla.", SUAVE);
		while (!h_suceso(v, &e, 0))
			h_dormir(2);
		if (e.clase == EV_CERRAR)
			return 0;
		if (e.clase == EV_RATON_BAJA)
			campo = (int)e.b >= 154;
		if (e.clase != EV_TECLA)
			continue;
		if (e.a == 27)
			return 0;
		if (e.a == 9) {
			campo = !campo;
		} else if (e.a == 10 || e.a == 13) {
			if (campo == 0) {
				campo = 1;
				continue;
			}
			nom[nn] = cla[nc] = 0;
			texto(40, 204, "Un momento...                                          ", SUAVE);
			if (vale_admin(nom, cla)) {
				crypto_wipe(cla, sizeof cla);
				return 1;
			}
			crypto_wipe(cla, sizeof cla);
			nc = 0;
			fallo = 1;
		} else if (e.a == 8) {
			if (campo == 0 && nn > 0)
				nn--;
			if (campo == 1 && nc > 0)
				nc--;
		} else if (e.a >= ' ' && e.a < 256) {
			if (campo == 0 && nn < 23)
				nom[nn++] = e.a;
			if (campo == 1 && nc < 63)
				cla[nc++] = e.a;
		}
	}
}

/* ---- la consola remota ---- */
static u32 numero(const char *a, const char *clave, u32 si_no)
{
	u32 p = 0;
	int i, n = strlen(clave);

	for (; *a; a++)
		if (h_empieza(a, clave)) {
			for (i = n; a[i] >= '0' && a[i] <= '9'; i++)
				p = p * 10 + a[i] - '0';
			return p;
		}
	return si_no;
}

int remota_principal(const char *args)
{
	u32 l, c, ip, pu, t;
	int r = 0;

	remota = 1;
	puerto = numero(args, "/PUERTO=", 2323);
	caduca = numero(args, "/CADUCA=", 60);
	if (!puerto || puerto > 65535)
		puerto = 2323;
	if (caduca < 5 || caduca > 3600)
		caduca = 60;
	/* el terminal del núcleo para el intérprete de dentro: uno por programa
	 * (desde la Consola del escritorio no se puede: ya está en uno), así que
	 * entonces se abre aparte, en su ventana */
	if (!h_hay_escritorio()) {
		h_escribe("«Dejar entrar a Claude» (la consola remota) va en el escritorio: ábrala desde el\n"
			  "menú Inicio (Sistema) o con su icono del tapiz.\n");
		return 1;
	}
	t = sc(SC_TERMINAL, 0, 0, 0);
	if (ES_ERROR(t) || t == 0) {
		char ru[40], ar[40];
		if (h_hay_escritorio()) {
			formatear(ru, sizeof ru, "%c:\\HISPA\\SSHD.BIN", h_sistema);
			formatear(ar, sizeof ar, "/REMOTA /PUERTO=%u /CADUCA=%u", puerto, caduca);
			if (!ES_ERROR(sc(SC_LANZAR_SUELTO, (u32)ru, (u32)ar, 0))) {
				h_escribe("La consola remota se abre en su ventana.\n");
				return 0;
			}
		}
		h_escribe("No queda ningún terminal del núcleo libre para la consola remota.\n");
		return 1;
	}
	/* (solo en el escritorio: lo que escribe el dueño de un terminal va a su
	 * tubo, así que en la pantalla de texto no se vería el código) */
	v = h_hay_escritorio() ? h_ventana("Dejar entrar a Claude", V_ANCHO, V_ALTO) : NO_VALE;
	if (ES_ERROR(v)) {
		h_apunta("remota: sin escritorio, no se abre");
		return 1;
	}
	/* solo un administrador (o con el nombre y la contraseña de uno) */
	if (con_usuarios()) {
		if (soy_administrador() && busca_yo(&adm))
			con_adm = 1;
		else if (pide_admin_ventana())
			con_adm = 1;
		else {
			h_apunta("remota: no la abre un administrador");
			if (v)
				sc(SC_CERRAR_VENTANA, v, 0, 0);
			return 1;
		}
	}
	if (llave_equipo(1) < 0) {
		h_escribe("No se puede hacer la llave del equipo (\\HISPA\\SSH).\n");
		if (v)
			sc(SC_CERRAR_VENTANA, v, 0, 0);
		return 1;
	}
	huella_equipo(huella);
	mira_ips();
	ult_ip = h_mi_ip();
	pinta();
	l = h_escuchar(puerto);
	while (ES_ERROR(l) && !cerrar) {
		/* (aún sin red: se vuelve a probar) */
		u32 hasta = h_tics() + 2 * h_hz;
		while ((int)(hasta - h_tics()) > 0 && !remota_vigila())
			h_dormir(h_hz / 10);
		l = h_escuchar(puerto);
	}
	if (!cerrar) {
		apunta("ABIERTA la consola remota (puerto %u)%s%s", puerto, con_adm ? " por " : "", con_adm ? adm.nombre : "");
		nuevo_codigo(0);
	}
	while (!cerrar) {
		if (remota_vigila())
			break;
		c = h_aceptar(l, h_hz / 5, &ip, &pu);
		if (ES_ERROR(c)) {
			if (c != TIEMPO)
				h_dormir(h_hz / 5);
			continue;
		}
		h_ip_a_texto(ip, ip_txt);
		if (en_pausa || !codigo[0]) {
			apunta("conexión de %s rechazada: no hay código vigente (la pausa)", ip_txt);
			h_cerrar_red(c);
			continue;
		}
		if (atiende(c, ip, pu)) {
			u32 s = (h_tics() - dentro_desde) / h_hz;
			apunta("SALE %s (%u min %u s dentro)", ip_txt, s / 60, s % 60);
			dentro = 0;
			echar = 0;
			if (!cerrar)
				nuevo_codigo("El código anterior ya se ha usado. Para la próxima conexión:");
		} else if (!codigo[0] && !en_pausa && !cerrar)
			nuevo_codigo(0);
		pinta();
	}
	h_cerrar_red(l);
	sin_codigo();
	apunta("CERRADA la consola remota: ya no entra nadie");
	sc(SC_CERRAR_VENTANA, v, 0, 0);
	return r;
}
