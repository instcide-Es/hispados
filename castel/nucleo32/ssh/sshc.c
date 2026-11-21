/* sshc.c - SSH.BIN, el cliente SSH de Hispa-Pentium:
 *
 *   SSH [-p puerto] usuario@equipo [orden]
 *   SSH /LLAVE          hace la llave propia (ssh-ed25519) y enseña la pública
 *
 * Entra en la terminal de otro equipo (Linux, macOS, Windows con OpenSSH,
 * otro Hispa-Pentium con SSHD). La sesión va en una ventana suya de 80 x 25
 * con un terminal tipo xterm (colores, cursor, la pantalla alternativa de
 * los programas de pantalla completa como nano, top o mc); sin escritorio,
 * en la pantalla de texto. Con una orden detrás, la hace allí y enseña lo
 * que salga aquí mismo. «~.» al principio de una línea corta la conexión.
 *
 * Las llaves de los equipos conocidos, en \HISPA\SSH\CONOCIDO.TXT: la
 * primera vez se enseña la huella y se pregunta; si luego cambia, no se
 * entra (alguien podría estar en medio). La llave propia, en
 * \HISPA\SSH\ID_ED.SSH (secreta) e ID_ED.PUB (la pública, para poner en el
 * ~/.ssh/authorized_keys del otro equipo).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ssh.h"
#include "cripto.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"

#define COLS 80
#define FILAS 25
#define MAX_TROZO 32768

static struct ssh S;
static char carpeta[48], equipo[128], usuario[64], orden[1024];
static u32 puerto = 22;
static u8 m[SSH_CABE];

/* ---- las llaves de los equipos conocidos ---- */
static void ruta_de(char *d, const char *n) { formatear(d, 80, "%s\\%s", carpeta, n); }

static int lee_si_no(void)
{
	for (;;) {
		u32 c = h_tecla(1);
		if (c == 's' || c == 'S') {
			h_escribe("s\n");
			return 1;
		}
		if (c == 'n' || c == 'N' || c == 3 || c == 27 || c == '\n') {
			h_escribe("n\n");
			return 0;
		}
	}
}

int ssh_comprueba_servidor(struct ssh *s, const u8 *blob, u32 n)
{
	static char t[16384];
	static u8 b[600];
	char r[80], quien[160], huella[64], linea[900];
	int k, i = 0, nq;

	ruta_de(r, "CONOCIDO.TXT");
	if (puerto == 22)
		formatear(quien, sizeof quien, "%s", equipo);
	else
		formatear(quien, sizeof quien, "[%s]:%u", equipo, puerto);
	nq = strlen(quien);
	k = h_lee_archivo(r, t, sizeof t - 1);
	if (k < 0)
		k = 0;
	t[k] = 0;
	ssh_huella(blob, n, huella);
	while (i < k) {
		int ini = i, fin, a, nb;
		while (i < k && t[i] != '\n')
			i++;
		fin = i++;
		if (fin - ini > nq + 13 && !memcmp(t + ini, quien, nq) && t[ini + nq] == ' ') {
			a = ini + nq + 1;
			if (memcmp(t + a, "ssh-ed25519 ", 12))
				continue;
			a += 12;
			{
				int e = a;
				while (e < fin && t[e] > ' ')
					e++;
				nb = h_de_base64(t + a, e - a, b, sizeof b);
			}
			if (nb == (int)n && !memcmp(b, blob, n))
				return 0;
			h_escribe("\n@@@ ¡OJO! LA LLAVE DE ESTE EQUIPO HA CAMBIADO @@@\n");
			h_escribef("La de %s ya no es la de antes (ahora: %s).\n", quien, huella);
			h_escribe("Puede que alguien esté en medio, o que el equipo se haya reinstalado.\n");
			h_escribef("Si sabe que es lo segundo, quite su línea de %s.\n", r);
			formatear(s->error, sizeof s->error, "la llave del equipo ha cambiado: no se entra");
			return -1;
		}
	}
	h_escribef("Es la primera vez que se entra en %s.\nSu llave: ssh-ed25519 %s\n¿Seguir y recordarla? (s/n) ", quien, huella);
	if (!lee_si_no()) {
		formatear(s->error, sizeof s->error, "no se sigue");
		return -1;
	}
	{
		char b64[100];
		int nl;
		h_base64(blob, n, b64, sizeof b64);
		nl = formatear(linea, sizeof linea, "%s ssh-ed25519 %s\r\n", quien, b64);
		if (k + nl < (int)sizeof t) {
			memcpy(t + k, linea, nl);
			if (h_escribe_archivo(r, t, k + nl) < 0)
				h_escribe("(no se ha podido guardar en CONOCIDO.TXT: se preguntará otra vez)\n");
		}
	}
	return 0;
}

/* ---- la llave propia ---- */
static int llave_propia(u8 secreta[64], u8 publica[32])
{
	char r[80];
	u8 sem[32];

	ruta_de(r, "ID_ED.SSH");
	if (h_lee_archivo(r, sem, 32) != 32)
		return -1;
	crypto_ed25519_key_pair(secreta, publica, sem);
	crypto_wipe(sem, 32);
	return 0;
}

static int hace_llave(void)
{
	char r[80], linea[200], b64[100], eq[64];
	u8 sem[32], sec[64], pub[32], blob[51];
	int n;

	ruta_de(r, "ID_ED.SSH");
	if (h_existe(r)) {
		h_escribe("Ya hay una llave propia; esta es su parte pública:\n");
	} else {
		h_azar(sem, 32);
		if (h_escribe_archivo(r, sem, 32) < 0) {
			h_escribe("No se puede escribir la llave (¿se ha arrancado con «escritura»?).\n");
			return 1;
		}
		crypto_wipe(sem, 32);
		h_escribe("Llave nueva hecha (la secreta, en ID_ED.SSH: no la dé a nadie).\n");
	}
	if (llave_propia(sec, pub) < 0)
		return 1;
	crypto_wipe(sec, 64);
	ssh_blob_ed25519(pub, blob);
	h_base64(blob, 51, b64, sizeof b64);
	memset(eq, 0, sizeof eq);
	sc(SC_AJUSTES, 3, (u32)eq, 0);
	eq[63] = 0;
	n = formatear(linea, sizeof linea, "ssh-ed25519 %s hispa@%s\r\n", b64, eq[0] ? eq : "hispa-pentium");
	ruta_de(r, "ID_ED.PUB");
	h_escribe_archivo(r, linea, n);
	h_escribe(linea);
	h_escribef("(también en %s: póngala en el ~/.ssh/authorized_keys del otro equipo)\n", r);
	return 0;
}

/* ---- entrar ---- */
static int lee_clave(char *d, int cabe)
{
	int n = 0;

	for (;;) {
		u32 c = h_tecla(1);
		if (c == '\n' || c == '\r')
			break;
		if (c == 3 || c == 27)
			return -1;
		if (c == 8) {
			if (n > 0)
				n--;
			continue;
		}
		if (c >= ' ' && c < 256 && n < cabe - 4)
			n += h_utf8_de_437(c, d + n);	/* (al otro lado, en UTF-8) */
	}
	d[n] = 0;
	h_escribe("\n");
	return n;
}

static int espera(u8 tipo1, u8 tipo2, const u8 **pl, u32 *n)
{
	for (;;) {
		int r = ssh_lee(&S, pl, n, 100);
		if (r < 0)
			return -1;
		if (r == 0)
			continue;
		if (**pl == SSH_USERAUTH_BANNER) {
			struct slee l;
			static char t[2048];
			sl_inicia(&l, *pl + 1, *n - 1);
			sl_texto(&l, t, sizeof t);
			h_escribe(t);
			continue;
		}
		if (**pl == tipo1 || **pl == tipo2 || tipo1 == 0)
			return 0;
	}
}

static int entra(void)
{
	const u8 *pl;
	u32 n;
	struct sbuf b;
	u8 sec[64], pub[32], blob[51], firma[64];
	static u8 f[1024];
	int intentos;

	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, SSH_SERVICE_REQUEST);
	sb_texto(&b, "ssh-userauth");
	if (ssh_envia(&S, m, b.n) < 0 || espera(SSH_SERVICE_ACCEPT, SSH_SERVICE_ACCEPT, &pl, &n) < 0)
		return -1;
	/* con la llave propia, si la hay */
	if (llave_propia(sec, pub) == 0) {
		u32 nf;
		ssh_blob_ed25519(pub, blob);
		nf = ssh_lo_firmado(&S, f, sizeof f, usuario, (const u8 *)"ssh-ed25519", 11, blob, 51);
		crypto_ed25519_sign(firma, sec, f, nf);
		crypto_wipe(sec, 64);
		sb_inicia(&b, m, sizeof m);
		sb_u8(&b, SSH_USERAUTH_REQUEST);
		sb_texto(&b, usuario);
		sb_texto(&b, "ssh-connection");
		sb_texto(&b, "publickey");
		sb_u8(&b, 1);
		sb_texto(&b, "ssh-ed25519");
		sb_cad(&b, blob, 51);
		sb_u32(&b, 4 + 11 + 4 + 64);
		sb_texto(&b, "ssh-ed25519");
		sb_cad(&b, firma, 64);
		if (ssh_envia(&S, m, b.n) < 0 || espera(SSH_USERAUTH_SUCCESS, SSH_USERAUTH_FAILURE, &pl, &n) < 0)
			return -1;
		if (*pl == SSH_USERAUTH_SUCCESS) {
			h_apunta("dentro, con la llave propia");
			return 0;
		}
	}
	for (intentos = 0; intentos < 3; intentos++) {
		static char clave[256];
		int nc;
		h_escribef("Contraseña de %s@%s: ", usuario, equipo);
		nc = lee_clave(clave, sizeof clave);
		if (nc < 0)
			return -1;
		sb_inicia(&b, m, sizeof m);
		sb_u8(&b, SSH_USERAUTH_REQUEST);
		sb_texto(&b, usuario);
		sb_texto(&b, "ssh-connection");
		sb_texto(&b, "password");
		sb_u8(&b, 0);
		sb_cad(&b, clave, nc);
		crypto_wipe(clave, sizeof clave);
		if (ssh_envia(&S, m, b.n) < 0)
			return -1;
		crypto_wipe(m, b.n);
		if (espera(SSH_USERAUTH_SUCCESS, SSH_USERAUTH_FAILURE, &pl, &n) < 0)
			return -1;
		if (*pl == SSH_USERAUTH_SUCCESS)
			return 0;
		h_escribe("No vale.\n");
	}
	formatear(S.error, sizeof S.error, "no se ha podido entrar");
	return -1;
}

/* ================= el terminal (xterm, lo de todos los días) ================= */
struct celda { u8 c, fg, bg, at; };
#define AT_NEGRITA 1
#define AT_INVERSO 2
#define AT_SUBRAYA 4
static struct celda pant[FILAS][COLS], otra[FILAS][COLS];
static u8 sucia[FILAS];
static int cx, cy, gx, gy, envolver, sx, sy, s_at, s_fg, s_bg, arriba, abajo, cursor_visible = 1, teclas_app, alterna;
static u8 fg = 7, bg = 0, at;
static int juego_g0;		/* 1: las líneas del DEC (ESC ( 0) */
static int estado, npar, par[16], privado;
static u32 utf_v, utf_falta;
static u32 ventana;		/* 0: la pantalla de texto */
static u8 respuesta[64];
static u32 n_respuesta;

static void limpia(int f, int x0, int x1)
{
	int x;

	for (x = x0; x < x1 && x < COLS; x++) {
		pant[f][x].c = ' ';
		pant[f][x].fg = fg;
		pant[f][x].bg = bg;
		pant[f][x].at = at & AT_INVERSO;
	}
	sucia[f] = 1;
}

static void sube(int ini, int fin, int n)	/* las filas de ini a fin (incluida) suben n */
{
	int f;

	if (n > fin - ini + 1)
		n = fin - ini + 1;
	for (f = ini; f <= fin - n; f++) {
		memcpy(pant[f], pant[f + n], sizeof pant[f]);
		sucia[f] = 1;
	}
	for (; f <= fin; f++)
		limpia(f, 0, COLS);
}

static void baja(int ini, int fin, int n)
{
	int f;

	if (n > fin - ini + 1)
		n = fin - ini + 1;
	for (f = fin; f >= ini + n; f--) {
		memcpy(pant[f], pant[f - n], sizeof pant[f]);
		sucia[f] = 1;
	}
	for (; f >= ini; f--)
		limpia(f, 0, COLS);
}

static void avance(void)
{
	if (cy == abajo)
		sube(arriba, abajo, 1);
	else if (cy < FILAS - 1)
		cy++;
}

static void pon_letra(u8 c)
{
	if (envolver) {
		cx = 0;
		avance();
		envolver = 0;
	}
	pant[cy][cx].c = c;
	pant[cy][cx].fg = fg;
	pant[cy][cx].bg = bg;
	pant[cy][cx].at = at;
	sucia[cy] = 1;
	if (cx == COLS - 1)
		envolver = 1;
	else
		cx++;
}

static void reinicia(void)
{
	int f;

	fg = 7, bg = 0, at = 0;
	for (f = 0; f < FILAS; f++)
		limpia(f, 0, COLS);
	cx = cy = 0;
	envolver = 0;
	arriba = 0, abajo = FILAS - 1;
	cursor_visible = 1;
	teclas_app = 0;
	juego_g0 = 0;
}

static const u8 dec_lineas[32] = {
	/* ` a b c d e f g h i j k l m n o p q r s t u v w x y z { | } ~ */
	0x04, 0xB1, '?', '?', '?', '?', 0xF8, 0xF1, '?', '?', 0xD9, 0xBF, 0xDA, 0xC0, 0xC5, 0xC4,
	0xC4, 0xC4, 0xC4, 0xC4, 0xC3, 0xB4, 0xC1, 0xC2, 0xB3, 0xF3, 0xF2, 0xE3, '!', 0x9C, 0xFA, ' ',
};

static void sgr(void)
{
	int i;

	if (npar == 0)
		npar = 1, par[0] = 0;
	for (i = 0; i < npar; i++) {
		int p = par[i];
		if (p == 0)
			fg = 7, bg = 0, at = 0;
		else if (p == 1)
			at |= AT_NEGRITA;
		else if (p == 4)
			at |= AT_SUBRAYA;
		else if (p == 7)
			at |= AT_INVERSO;
		else if (p == 22)
			at &= ~AT_NEGRITA;
		else if (p == 24)
			at &= ~AT_SUBRAYA;
		else if (p == 27)
			at &= ~AT_INVERSO;
		else if (p >= 30 && p <= 37)
			fg = p - 30;
		else if (p == 39)
			fg = 7;
		else if (p >= 40 && p <= 47)
			bg = p - 40;
		else if (p == 49)
			bg = 0;
		else if (p >= 90 && p <= 97)
			fg = p - 90 + 8;
		else if (p >= 100 && p <= 107)
			bg = p - 100 + 8;
		else if ((p == 38 || p == 48) && i + 2 < npar && par[i + 1] == 5) {
			if (p == 38)
				fg = par[i + 2];
			else
				bg = par[i + 2];
			i += 2;
		} else if ((p == 38 || p == 48) && i + 4 < npar && par[i + 1] == 2) {
			/* color de 24 bits: al más cercano de los 216 */
			int r = par[i + 2] * 5 / 255, g = par[i + 3] * 5 / 255, b = par[i + 4] * 5 / 255;
			int c = 16 + 36 * r + 6 * g + b;
			if (p == 38)
				fg = c;
			else
				bg = c;
			i += 4;
		}
	}
}

static void responde(const char *t)
{
	int n = strlen(t);

	if (n_respuesta + n < sizeof respuesta) {
		memcpy(respuesta + n_respuesta, t, n);
		n_respuesta += n;
	}
}

static void pantalla_alterna(int si)
{
	if (si == alterna)
		return;
	memcpy(otra, pant, sizeof pant);	/* (se cambian: la de ahora se guarda) */
	if (si) {
		static struct celda g[FILAS][COLS];
		memcpy(g, pant, sizeof pant);
		reinicia();
		memcpy(otra, g, sizeof g);
		alterna = 1;
	} else {
		memcpy(pant, otra, sizeof pant);
		alterna = 0;
		memset(sucia, 1, sizeof sucia);
	}
}

static void csi(u8 f)
{
	int p0 = npar > 0 ? par[0] : 0, p1 = npar > 1 ? par[1] : 0, n = p0 ? p0 : 1, i;

	if (privado == '?') {
		if (f == 'h' || f == 'l') {
			int si = f == 'h';
			for (i = 0; i < npar; i++) {
				if (par[i] == 1)
					teclas_app = si;
				else if (par[i] == 25)
					cursor_visible = si;
				else if (par[i] == 1049 || par[i] == 47 || par[i] == 1047) {
					if (si && par[i] == 1049)
						sx = cx, sy = cy;
					pantalla_alterna(si);
					if (!si && par[i] == 1049)
						cx = sx, cy = sy;
				}
			}
		}
		return;
	}
	if (privado == '>') {
		if (f == 'c')
			responde("\x1b[>0;95;0c");
		return;
	}
	envolver = 0;
	switch (f) {
	case 'A': cy -= n; if (cy < arriba && cy + n >= arriba) cy = arriba; break;
	case 'B': case 'e': cy += n; if (cy > abajo && cy - n <= abajo) cy = abajo; break;
	case 'C': case 'a': cx += n; break;
	case 'D': cx -= n; break;
	case 'E': cy += n, cx = 0; break;
	case 'F': cy -= n, cx = 0; break;
	case 'G': case '`': cx = n - 1; break;
	case 'd': cy = n - 1; break;
	case 'H': case 'f': cy = (p0 ? p0 : 1) - 1, cx = (p1 ? p1 : 1) - 1; break;
	case 'J':
		if (p0 == 0) {
			limpia(cy, cx, COLS);
			for (i = cy + 1; i < FILAS; i++)
				limpia(i, 0, COLS);
		} else if (p0 == 1) {
			for (i = 0; i < cy; i++)
				limpia(i, 0, COLS);
			limpia(cy, 0, cx + 1);
		} else
			for (i = 0; i < FILAS; i++)
				limpia(i, 0, COLS);
		break;
	case 'K':
		if (p0 == 0)
			limpia(cy, cx, COLS);
		else if (p0 == 1)
			limpia(cy, 0, cx + 1);
		else
			limpia(cy, 0, COLS);
		break;
	case 'L': if (cy >= arriba && cy <= abajo) baja(cy, abajo, n); break;
	case 'M': if (cy >= arriba && cy <= abajo) sube(cy, abajo, n); break;
	case 'S': sube(arriba, abajo, n); break;
	case 'T': baja(arriba, abajo, n); break;
	case 'P':
		if (n > COLS - cx)
			n = COLS - cx;
		memmove(&pant[cy][cx], &pant[cy][cx + n], (COLS - cx - n) * sizeof(struct celda));
		limpia(cy, COLS - n, COLS);
		break;
	case '@':
		if (n > COLS - cx)
			n = COLS - cx;
		memmove(&pant[cy][cx + n], &pant[cy][cx], (COLS - cx - n) * sizeof(struct celda));
		limpia(cy, cx, cx + n);
		break;
	case 'X': limpia(cy, cx, cx + n); break;
	case 'm': sgr(); break;
	case 'r':
		arriba = (p0 ? p0 : 1) - 1;
		abajo = (p1 ? p1 : FILAS) - 1;
		if (arriba < 0 || abajo >= FILAS || arriba >= abajo)
			arriba = 0, abajo = FILAS - 1;
		cx = 0, cy = 0;
		break;
	case 's': sx = cx, sy = cy; break;
	case 'u': cx = sx, cy = sy; break;
	case 'n':
		if (p0 == 6) {
			char t[24];
			formatear(t, sizeof t, "\x1b[%u;%uR", cy + 1, cx + 1);
			responde(t);
		} else if (p0 == 5)
			responde("\x1b[0n");
		break;
	case 'c': responde("\x1b[?1;2c"); break;
	}
	if (cx < 0) cx = 0;
	if (cx >= COLS) cx = COLS - 1;
	if (cy < 0) cy = 0;
	if (cy >= FILAS) cy = FILAS - 1;
}

enum { NORMAL, ESC, CSI, OSC, OSC_ESC, JUEGO, OTRO };

static void terminal(const u8 *d, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		u8 c = d[i];
		if (estado == NORMAL && utf_falta) {
			if ((c & 0xC0) == 0x80) {
				utf_v = utf_v << 6 | (c & 63);
				if (--utf_falta == 0)
					pon_letra(h_437_de_unicode(utf_v));
				continue;
			}
			utf_falta = 0;
		}
		switch (estado) {
		case NORMAL:
			if (c == 27)
				estado = ESC;
			else if (c == '\r')
				cx = 0, envolver = 0;
			else if (c == '\n' || c == 11 || c == 12) {
				avance();
				envolver = 0;
			} else if (c == 8) {
				if (cx > 0)
					cx--;
				envolver = 0;
			} else if (c == '\t') {
				cx = (cx / 8 + 1) * 8;
				if (cx >= COLS)
					cx = COLS - 1;
			} else if (c == 7 || c < 32) {
				/* (la campana y los demás: nada) */
			} else if (c >= 0xC0 && c < 0xE0)
				utf_v = c & 31, utf_falta = 1;
			else if (c >= 0xE0 && c < 0xF0)
				utf_v = c & 15, utf_falta = 2;
			else if (c >= 0xF0 && c < 0xF8)
				utf_v = c & 7, utf_falta = 3;
			else if (c >= 0x80)
				pon_letra('?');
			else if (juego_g0 && c >= 0x60 && c <= 0x7F)
				pon_letra(dec_lineas[c - 0x60]);
			else
				pon_letra(c);
			break;
		case ESC:
			estado = NORMAL;
			if (c == '[') {
				estado = CSI;
				npar = 0;
				privado = 0;
				memset(par, 0, sizeof par);
			} else if (c == ']')
				estado = OSC;
			else if (c == '(' || c == ')' || c == '*' || c == '+')
				estado = c == '(' ? JUEGO : OTRO;
			else if (c == '#' || c == '%' || c == ' ')
				estado = OTRO;
			else if (c == '7')
				sx = cx, sy = cy, s_at = at, s_fg = fg, s_bg = bg;
			else if (c == '8')
				cx = sx, cy = sy, at = s_at, fg = s_fg, bg = s_bg;
			else if (c == 'D')
				avance();
			else if (c == 'E') {
				cx = 0;
				avance();
			} else if (c == 'M') {
				if (cy == arriba)
					baja(arriba, abajo, 1);
				else if (cy > 0)
					cy--;
			} else if (c == 'c')
				reinicia();
			break;
		case CSI:
			if (c >= '0' && c <= '9') {
				if (npar == 0)
					npar = 1;
				par[npar - 1] = par[npar - 1] * 10 + c - '0';
				if (par[npar - 1] > 9999)
					par[npar - 1] = 9999;
			} else if (c == ';' || c == ':') {
				if (npar == 0)
					npar = 1;
				if (npar < 16)
					npar++;
			} else if (c == '?' || c == '>' || c == '=' || c == '!') {
				privado = c;
			} else if (c >= 0x40 && c <= 0x7E) {
				csi(c);
				estado = NORMAL;
			} else if (c == 27)
				estado = ESC;
			break;
		case OSC:
			if (c == 7)
				estado = NORMAL;
			else if (c == 27)
				estado = OSC_ESC;
			break;
		case OSC_ESC:
			estado = c == '\\' ? NORMAL : OSC;
			break;
		case JUEGO:
			juego_g0 = c == '0';
			estado = NORMAL;
			break;
		case OTRO:
			estado = NORMAL;
			break;
		}
	}
}

/* ---- pintarlo ---- */
static u32 rgb(int i)
{
	static const u32 b16[16] = { 0x000000, 0xCD0000, 0x00CD00, 0xCDCD00, 0x2060EE, 0xCD00CD, 0x00CDCD, 0xD6E2F0,
		0x7F7F7F, 0xFF4040, 0x40FF40, 0xFFFF40, 0x6090FF, 0xFF40FF, 0x40FFFF, 0xFFFFFF };
	if (i < 16)
		return b16[i];
	if (i < 232) {
		int r = (i - 16) / 36, g = (i - 16) / 6 % 6, b = (i - 16) % 6;
		r = r ? 55 + r * 40 : 0, g = g ? 55 + g * 40 : 0, b = b ? 55 + b * 40 : 0;
		return r << 16 | g << 8 | b;
	}
	i = 8 + (i - 232) * 10;
	return i << 16 | i << 8 | i;
}

static u8 vga(int i)	/* a los 16 de la pantalla de texto (el orden del PC: azul es el 1) */
{
	static const u8 pc[16] = { 0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15 };
	if (i < 16)
		return pc[i];
	if (i >= 232)
		return i < 238 ? 0 : i < 244 ? 8 : i < 250 ? 7 : 15;
	{
		int r = (i - 16) / 36, g = (i - 16) / 6 % 6, b = (i - 16) % 6, k = 0, brillo = r > 3 || g > 3 || b > 3;
		if (r >= 2) k |= 4;
		if (g >= 2) k |= 2;
		if (b >= 2) k |= 1;
		if (!k && (r || g || b))
			return 8;
		return pc[k | (brillo ? 8 : 0)] ;
	}
}

static void colores(struct celda *c, int *f, int *b)
{
	int x = c->fg, y = c->bg;

	if (c->at & AT_NEGRITA && x < 8)
		x += 8;
	if (c->at & AT_INVERSO) {
		int t = x;
		x = y, y = t;
	}
	*f = x, *b = y;
}

#define MARGEN 4
static int cursor_pintado_x = -1, cursor_pintado_y = -1;

static void pinta_fila(int fl)
{
	int x = 0;

	while (x < COLS) {
		int f0, b0, f1, b1, k = x;
		char t[COLS];
		colores(&pant[fl][x], &f0, &b0);
		while (k < COLS) {
			colores(&pant[fl][k], &f1, &b1);
			if (f1 != f0 || b1 != b0 || (pant[fl][k].at & AT_SUBRAYA) != (pant[fl][x].at & AT_SUBRAYA))
				break;
			t[k - x] = pant[fl][k].c;
			k++;
		}
		if (ventana) {
			h_texto_fondo(ventana, MARGEN + x * 8, MARGEN + fl * 16, t, k - x, rgb(f0), rgb(b0));
			if (pant[fl][x].at & AT_SUBRAYA)
				h_rectangulo(ventana, MARGEN + x * 8, MARGEN + fl * 16 + 14, (k - x) * 8, 1, rgb(f0));
		} else
			sc(SC_TEXTO_EN, x | fl << 8 | (u32)(vga(f0) | vga(b0) << 4) << 16, (u32)t, k - x);
		x = k;
	}
}

static void pinta(void)
{
	int f;

	if (cursor_pintado_y >= 0 && cursor_pintado_y < FILAS)
		sucia[cursor_pintado_y] = 1;
	for (f = 0; f < FILAS; f++)
		if (sucia[f]) {
			pinta_fila(f);
			sucia[f] = 0;
		}
	if (ventana) {
		if (cursor_visible) {
			h_rectangulo(ventana, MARGEN + cx * 8, MARGEN + cy * 16 + 13, 8, 2, 0x6FE08A);
			cursor_pintado_x = cx, cursor_pintado_y = cy;
		} else
			cursor_pintado_y = -1;
	} else
		sc(SC_CURSOR_EN, cursor_visible ? (u32)(cx | cy << 8) : (u32)(25 << 8), 0, 0);
}

/* ---- las teclas, a lo que manda un xterm ---- */
static u32 tecla_a_bytes(u32 c, char *o)
{
	const char *t = 0;

	if (c < T_ESPECIAL) {
		if (c == '\n' || c == '\r')
			return o[0] = '\r', 1;
		if (c == 8)
			return o[0] = 127, 1;
		if (c >= 128)
			return h_utf8_de_437(c, o);
		return o[0] = c, 1;
	}
	switch (c) {
	case T_ARRIBA: t = teclas_app ? "\x1bOA" : "\x1b[A"; break;
	case T_ABAJO: t = teclas_app ? "\x1bOB" : "\x1b[B"; break;
	case T_DERECHA: t = teclas_app ? "\x1bOC" : "\x1b[C"; break;
	case T_IZQUIERDA: t = teclas_app ? "\x1bOD" : "\x1b[D"; break;
	case T_INICIO: t = "\x1b[H"; break;
	case T_FIN: t = "\x1b[F"; break;
	case T_SUPR: t = "\x1b[3~"; break;
	case T_INSERT: t = "\x1b[2~"; break;
	case T_RE_PAG: t = "\x1b[5~"; break;
	case T_AV_PAG: t = "\x1b[6~"; break;
	case T_F1: t = "\x1bOP"; break;
	case T_F1 + 1: t = "\x1bOQ"; break;
	case T_F1 + 2: t = "\x1bOR"; break;
	case T_F1 + 3: t = "\x1bOS"; break;
	case T_F1 + 4: t = "\x1b[15~"; break;
	case T_F1 + 5: t = "\x1b[17~"; break;
	case T_F1 + 6: t = "\x1b[18~"; break;
	case T_F1 + 7: t = "\x1b[19~"; break;
	case T_F1 + 8: t = "\x1b[20~"; break;
	case T_F1 + 9: t = "\x1b[21~"; break;
	case T_F11: t = "\x1b[23~"; break;
	case T_F12: t = "\x1b[24~"; break;
	}
	if (!t)
		return 0;
	memcpy(o, t, strlen(t));
	return strlen(t);
}

/* ---- la sesión ---- */
static u32 suyo, ventana_suya, max_suyo;

static int envia_datos(const void *d, u32 n)
{
	struct sbuf b;

	if (!n)
		return 0;
	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, SSH_CHANNEL_DATA);
	sb_u32(&b, suyo);
	sb_cad(&b, d, n);
	return ssh_envia(&S, m, b.n);
}

static int sin_pantalla;	/* con orden: lo que llega, aquí mismo (sin terminal) */

static void a_la_consola(const u8 *d, u32 n)
{
	static char o[4096];
	u32 i, k = 0;

	for (i = 0; i < n; i++) {
		u8 c = d[i];
		if (utf_falta) {
			if ((c & 0xC0) == 0x80) {
				utf_v = utf_v << 6 | (c & 63);
				if (--utf_falta == 0)
					o[k++] = h_437_de_unicode(utf_v);
				goto mira;
			}
			utf_falta = 0;
		}
		if (c >= 0xC0 && c < 0xE0)
			utf_v = c & 31, utf_falta = 1;
		else if (c >= 0xE0 && c < 0xF0)
			utf_v = c & 15, utf_falta = 2;
		else if (c >= 0xF0)
			utf_v = c & 7, utf_falta = 3;
		else if (c != '\r')
			o[k++] = c;
mira:
		if (k > sizeof o - 4) {
			sc(SC_ESCRIBIR, (u32)o, k, 0);
			k = 0;
		}
	}
	if (k)
		sc(SC_ESCRIBIR, (u32)o, k, 0);
}

static int sesion(void)
{
	const u8 *pl;
	u32 n, codigo = 0;
	struct sbuf b;
	int r, pide = 0, en_linea = 1, tilde = 0;
	u32 mi_ventana = 1 << 20;

	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, SSH_CHANNEL_OPEN);
	sb_texto(&b, "session");
	sb_u32(&b, 0);
	sb_u32(&b, mi_ventana);
	sb_u32(&b, MAX_TROZO);
	if (ssh_envia(&S, m, b.n) < 0 || espera(SSH_CHANNEL_OPEN_CONFIRMATION, SSH_CHANNEL_OPEN_FAILURE, &pl, &n) < 0)
		return -1;
	if (*pl == SSH_CHANNEL_OPEN_FAILURE) {
		struct slee l;
		char t[120];
		sl_inicia(&l, pl + 1, n - 1);
		sl_u32(&l);
		sl_u32(&l);
		sl_texto(&l, t, sizeof t);
		formatear(S.error, sizeof S.error, "el otro equipo no abre la sesión: %s", t);
		return -1;
	}
	{
		struct slee l;
		sl_inicia(&l, pl + 1, n - 1);
		sl_u32(&l);
		suyo = sl_u32(&l);
		ventana_suya = sl_u32(&l);
		max_suyo = sl_u32(&l);
		if (max_suyo > MAX_TROZO)
			max_suyo = MAX_TROZO;
	}
	if (!sin_pantalla) {
		sb_inicia(&b, m, sizeof m);
		sb_u8(&b, SSH_CHANNEL_REQUEST);
		sb_u32(&b, suyo);
		sb_texto(&b, "pty-req");
		sb_u8(&b, 1);
		sb_texto(&b, "xterm");
		sb_u32(&b, COLS);
		sb_u32(&b, FILAS);
		sb_u32(&b, COLS * 8);
		sb_u32(&b, FILAS * 16);
		sb_cad(&b, "\0", 1);
		if (ssh_envia(&S, m, b.n) < 0)
			return -1;
		pide++;
	}
	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, SSH_CHANNEL_REQUEST);
	sb_u32(&b, suyo);
	if (orden[0]) {
		sb_texto(&b, "exec");
		sb_u8(&b, 1);
		sb_texto(&b, orden);
	} else {
		sb_texto(&b, "shell");
		sb_u8(&b, 1);
	}
	if (ssh_envia(&S, m, b.n) < 0)
		return -1;
	pide++;
	if (!sin_pantalla) {
		if (h_hay_escritorio()) {
			char t[200];
			formatear(t, sizeof t, "SSH %s@%s", usuario, equipo);
			ventana = h_ventana(t, COLS * 8 + 2 * MARGEN, FILAS * 16 + 2 * MARGEN);
			if (ES_ERROR(ventana))
				ventana = 0;
		}
		if (!ventana && sc(SC_DATO, DATO_ESCRITORIO, 0, 0) == 1) {
			formatear(S.error, sizeof S.error, "no se puede abrir la ventana");
			return -1;
		}
		if (!ventana)
			sc(SC_BORRAR_PANTALLA, 0, 0, 0);
		reinicia();
		memset(sucia, 1, sizeof sucia);
		pinta();
	}
	for (;;) {
		int algo = 0;
		u8 tb[64];
		u32 tn = 0;
		r = ssh_lee(&S, &pl, &n, 1);
		if (r < 0)
			break;
		if (r > 0) {
			struct slee l;
			algo = 1;
			sl_inicia(&l, pl + 1, n - 1);
			switch (*pl) {
			case SSH_CHANNEL_DATA:
			case SSH_CHANNEL_EXTENDED_DATA: {
				const u8 *d;
				u32 nd;
				sl_u32(&l);
				if (*pl == SSH_CHANNEL_EXTENDED_DATA)
					sl_u32(&l);
				d = sl_cad(&l, &nd);
				if (l.mal)
					break;
				if (sin_pantalla)
					a_la_consola(d, nd);
				else
					terminal(d, nd);
				mi_ventana -= nd < mi_ventana ? nd : mi_ventana;
				if (mi_ventana < (1 << 19)) {
					sb_inicia(&b, m, sizeof m);
					sb_u8(&b, SSH_CHANNEL_WINDOW_ADJUST);
					sb_u32(&b, suyo);
					sb_u32(&b, (1 << 20) - mi_ventana);
					mi_ventana = 1 << 20;
					if (ssh_envia(&S, m, b.n) < 0)
						goto fin;
				}
				break;
			}
			case SSH_CHANNEL_WINDOW_ADJUST:
				sl_u32(&l);
				ventana_suya += sl_u32(&l);
				break;
			case SSH_CHANNEL_SUCCESS:
				if (pide)
					pide--;
				break;
			case SSH_CHANNEL_FAILURE:
				if (pide)
					pide--;
				break;
			case SSH_CHANNEL_REQUEST: {
				const u8 *q;
				u32 nq;
				sl_u32(&l);
				q = sl_cad(&l, &nq);
				if (sl_cad_es(q, nq, "exit-status")) {
					sl_u8(&l);
					codigo = sl_u32(&l);
				} else if (sl_u8(&l)) {
					sb_inicia(&b, m, sizeof m);
					sb_u8(&b, SSH_CHANNEL_FAILURE);
					sb_u32(&b, suyo);
					ssh_envia(&S, m, b.n);
				}
				break;
			}
			case SSH_GLOBAL_REQUEST: {
				const u8 *q;
				u32 nq;
				q = sl_cad(&l, &nq);
				(void)q;
				if (sl_u8(&l)) {
					m[0] = SSH_REQUEST_FAILURE;
					ssh_envia(&S, m, 1);
				}
				break;
			}
			case SSH_CHANNEL_EOF:
				break;
			case SSH_CHANNEL_CLOSE:
				sb_inicia(&b, m, sizeof m);
				sb_u8(&b, SSH_CHANNEL_CLOSE);
				sb_u32(&b, suyo);
				ssh_envia(&S, m, b.n);
				goto fin;
			}
		}
		if (n_respuesta) {
			envia_datos(respuesta, n_respuesta);
			n_respuesta = 0;
		}
		/* las teclas */
		for (;;) {
			u32 c = 0;
			if (ventana) {
				struct suceso e;
				if (!h_suceso(ventana, &e, 0))
					break;
				if (e.clase == EV_CERRAR)
					goto corta;
				if (e.clase != EV_TECLA)
					continue;
				c = e.a;
			} else {
				c = h_tecla(0);
				if (!c && sc(SC_HUBO_CORTE, 0, 0, 0) == 1)
					c = 3;
				if (!c)
					break;
			}
			/* «~.» al principio de una línea: cortar */
			if (tilde && c == '.')
				goto corta;
			if (en_linea && c == '~') {
				tilde = 1;
				continue;
			}
			if (tilde) {
				tb[tn++] = '~';
				tilde = 0;
			}
			en_linea = c == '\n' || c == '\r';
			tn += tecla_a_bytes(c, (char *)tb + tn);
			if (tn > sizeof tb - 8)
				break;
		}
		if (tn) {
			algo = 1;
			if (envia_datos(tb, tn) < 0)
				break;
		}
		if (!sin_pantalla && (algo || 1))
			pinta();
	}
fin:
	if (ventana)
		sc(SC_CERRAR_VENTANA, ventana, 0, 0);
	else if (!sin_pantalla)
		sc(SC_BORRAR_PANTALLA, 0, 0, 0);
	if (S.error[0] && r < 0)
		return -1;
	h_apunta("sesión acabada (código %u)", codigo);
	return codigo;
corta:
	ssh_corta(&S, 11, "el usuario corta");
	if (ventana)
		sc(SC_CERRAR_VENTANA, ventana, 0, 0);
	else if (!sin_pantalla)
		sc(SC_BORRAR_PANTALLA, 0, 0, 0);
	h_escribe("Conexión cortada.\n");
	return 0;
}

static void ayuda(void)
{
	h_escribe("SSH: entrar en la terminal de otro equipo.\n"
		  "  SSH [-p puerto] usuario@equipo [orden]\n"
		  "  SSH /LLAVE      hace la llave propia (y enseña la pública)\n"
		  "En la sesión, «~.» al principio de una línea corta la conexión.\n");
}

int principal(void)
{
	char a[1200], *p, *q;
	u32 ip, c;
	int r;

	h_argumentos(a, sizeof a);
	h_inicia("ssh");
	h_ruta(carpeta, sizeof carpeta, "SSH");
	h_carpeta(carpeta);
	sc(SC_ATENDER_CORTE, 1, 0, 0);
	p = a;
	while (*p == ' ')
		p++;
	if (!*p || *p == '?' || h_empieza(p, "/?") || h_empieza(p, "/AYUDA")) {
		ayuda();
		return 0;
	}
	if (h_empieza(p, "/LLAVE"))
		return hace_llave();
	if (h_empieza(p, "-p ")) {
		p += 3;
		puerto = 0;
		while (*p >= '0' && *p <= '9')
			puerto = puerto * 10 + *p++ - '0';
		while (*p == ' ')
			p++;
		if (!puerto || puerto > 65535) {
			h_escribe("Ese puerto no vale.\n");
			return 1;
		}
	}
	/* usuario@equipo */
	q = p;
	while (*q && *q != ' ')
		q++;
	if (*q) {
		*q++ = 0;
		while (*q == ' ')
			q++;
		h_copia(orden, q, sizeof orden);
	}
	q = p;
	while (*q && *q != '@')
		q++;
	if (*q == '@') {
		*q = 0;
		h_copia(usuario, p, sizeof usuario);
		h_copia(equipo, q + 1, sizeof equipo);
	} else {
		memset(usuario, 0, sizeof usuario);
		sc(SC_AJUSTES, 1, (u32)usuario, 0);
		usuario[63] = 0;
		if (!usuario[0])
			h_copia(usuario, "hispa", sizeof usuario);
		h_copia(equipo, p, sizeof equipo);
	}
	/* (los nombres de los usuarios de Linux van en minúsculas: el intérprete los da en mayúsculas si se escriben así) */
	sin_pantalla = orden[0] != 0;
	r = h_resolver(equipo, &ip);
	if (ES_ERROR(r)) {
		h_escribef("SSH: %s: %s\n", equipo, h_texto_error_red(r));
		return 1;
	}
	{
		char t[16];
		h_ip_a_texto(ip, t);
		h_apunta("conectando con %s (%s), puerto %u", equipo, t, puerto);
	}
	c = h_conectar(ip, puerto, 10 * h_hz);
	if (ES_ERROR(c)) {
		h_escribef("SSH: %s: %s\n", equipo, h_texto_error_red(c));
		return 1;
	}
	ssh_prepara(&S, 0, c);
	if (ssh_empieza(&S) < 0 || entra() < 0) {
		h_escribef("SSH: %s\n", S.error);
		h_apunta("%s", S.error);
		h_cerrar_red(c);
		return 1;
	}
	r = sesion();
	h_cerrar_red(c);
	crypto_wipe(S.k_ent, sizeof S.k_ent);
	crypto_wipe(S.k_sal, sizeof S.k_sal);
	if (r < 0) {
		h_escribef("SSH: %s\n", S.error);
		return 1;
	}
	if (!sin_pantalla)
		h_escribef("Conexión con %s cerrada.\n", equipo);
	return r;
}
