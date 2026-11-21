/* sshd.c - SSHD.BIN, el servidor SSH de Hispa-Pentium: da la Consola (el
 * intérprete de órdenes, ORDENES.BIN) a quien entre desde otro equipo con un
 * cliente SSH (OpenSSH, PuTTY...). Apagado hasta que el usuario lo enciende.
 *
 *   SSHD                el estado (encendido o no, el puerto, la huella)
 *   SSHD /ENCENDER      lo enciende (y queda encendido en los arranques)
 *   SSHD /APAGAR        lo apaga
 *   SSHD /CLAVE         pone la contraseña para entrar (se guarda resumida)
 *   SSHD /PUERTO=n      otro puerto (22 si no se dice)
 *
 * Lo suyo va en \HISPA\SSH de la unidad del sistema: SSHD.INI (encendido=,
 * puerto=), LLAVE.SSH (la llave del equipo: secreta, se hace la primera
 * vez), CLAVE.TXT (la contraseña, resumida con PBKDF2-HMAC-SHA-256 y su
 * sal: nunca en claro) y AUTORIZA.TXT (o authorized_keys: las llaves
 * públicas que pueden entrar sin contraseña, una por línea, como las de
 * OpenSSH). Lo que pasa se apunta por el puerto serie («sshd: ...»).
 *
 * El intérprete corre en un terminal del núcleo (terminal.cas, la llamada
 * 37): lo que escribe llega aquí, se pasa a UTF-8 y sale por el canal; lo
 * que se teclea al otro lado (UTF-8 y las secuencias de un xterm) se pasa a
 * las teclas de Hispa-Pentium. Una sesión a la vez.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ssh.h"
#include "cripto.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"

#define VUELTAS 10000		/* las de PBKDF2 (en un Pentium a 133 MHz, alrededor de 1 s) */
#define VENTANA 65536		/* lo que se deja mandar sin avisar */
#define MAX_TROZO 16384

static struct ssh S;
static char carpeta[48];
static u32 puerto = 22;
static u8 m[SSH_CABE];
static u32 term_hecho;		/* el terminal del núcleo (uno por programa: el de este) */

static void ruta_de(char *d, const char *nombre)
{
	formatear(d, 80, "%s\\%s", carpeta, nombre);
}

static int lee_ini(char *t, int cabe)
{
	char r[80];
	int n;

	ruta_de(r, "SSHD.INI");
	n = h_lee_archivo(r, t, cabe - 1);
	if (n < 0)
		n = 0;
	t[n] = 0;
	return n;
}

static int encendido(void)
{
	char t[256], v[16];

	lee_ini(t, sizeof t);
	return h_ini(t, "encendido", v, sizeof v) && v[0] == '1';
}

static void pon_ini(int enc)
{
	char t[128], r[80];

	formatear(t, sizeof t, "; el servidor SSH de Hispa-Pentium (SSHD)\r\nencendido=%u\r\npuerto=%u\r\n", enc, puerto);
	ruta_de(r, "SSHD.INI");
	h_escribe_archivo(r, t, strlen(t));
}

static void lee_puerto(void)
{
	char t[256], v[16];
	u32 p = 0;
	int i;

	lee_ini(t, sizeof t);
	if (h_ini(t, "puerto", v, sizeof v)) {
		for (i = 0; v[i] >= '0' && v[i] <= '9'; i++)
			p = p * 10 + v[i] - '0';
		if (p > 0 && p < 65536)
			puerto = p;
	}
}

/* la llave del equipo: 32 octetos de semilla en LLAVE.SSH (se hace la primera vez) */
static int llave_equipo(int hacer)
{
	char r[80];
	u8 sem[32];
	int n;

	ruta_de(r, "LLAVE.SSH");
	n = h_lee_archivo(r, sem, 32);
	if (n != 32) {
		if (!hacer)
			return -1;
		h_azar(sem, 32);
		if (h_escribe_archivo(r, sem, 32) < 0) {
			crypto_wipe(sem, 32);
			return -1;
		}
		h_apunta("llave del equipo nueva (ssh-ed25519)");
	}
	crypto_ed25519_key_pair(S.host_secreta, S.host_publica, sem);
	crypto_wipe(sem, 32);
	return 0;
}

static void huella_equipo(char *t)
{
	u8 blob[51];

	ssh_blob_ed25519(S.host_publica, blob);
	ssh_huella(blob, 51, t);
}

/* ---- la contraseña ---- */
static int hay_clave(void)
{
	char r[80];

	ruta_de(r, "CLAVE.TXT");
	return h_existe(r);
}

static int comprueba_clave(const u8 *c, u32 n)
{
	char r[80], t[300], *p;
	u8 sal[16], bien[32], h[32];
	u32 vueltas = 0;
	int k;

	ruta_de(r, "CLAVE.TXT");
	k = h_lee_archivo(r, t, sizeof t - 1);
	if (k <= 0)
		return 0;
	t[k] = 0;
	if (memcmp(t, "pbkdf2-sha256$", 14))
		return 0;
	for (p = t + 14; *p >= '0' && *p <= '9'; p++)
		vueltas = vueltas * 10 + *p - '0';
	if (*p++ != '$' || !h_de_hex(p, sal, 16) || p[32] != '$' || !h_de_hex(p + 33, bien, 32) || vueltas < 1000)
		return 0;
	pbkdf2_sha256(c, n, sal, 16, vueltas, h);
	k = !crypto_verify32(h, bien);
	crypto_wipe(h, 32);
	return k;
}

static int lee_secreto(char *d, int cabe)
{
	int n = 0;

	for (;;) {
		u32 c = h_tecla(1);
		if (c == '\n' || c == '\r')
			break;
		if (c == 3 || c == 27) {
			crypto_wipe(d, cabe);
			return -1;
		}
		if (c == 8) {
			if (n > 0) {
				n--;
				h_escribe("\b \b");
			}
			continue;
		}
		if (c >= ' ' && c < 256 && n < cabe - 1) {
			d[n++] = c;
			h_escribe("*");
		}
	}
	d[n] = 0;
	h_escribe("\n");
	return n;
}

static int pon_clave(void)
{
	char a[80], b[80], t[200], r[80], hs[40], hh[70];
	u8 sal[16], h[32], u8c[240];
	int na, nb, i, k;

	h_escribe("La contraseña para entrar por SSH (al menos 6 letras): ");
	na = lee_secreto(a, sizeof a);
	if (na < 0)
		return 1;
	h_escribe("Otra vez, para comprobarla: ");
	nb = lee_secreto(b, sizeof b);
	if (nb < 0 || na != nb || memcmp(a, b, na)) {
		h_escribe("No son iguales: no se cambia.\n");
		crypto_wipe(a, sizeof a);
		crypto_wipe(b, sizeof b);
		return 1;
	}
	crypto_wipe(b, sizeof b);
	if (na < 6) {
		h_escribe("Es demasiado corta: no se cambia.\n");
		crypto_wipe(a, sizeof a);
		return 1;
	}
	/* (la otra punta la manda en UTF-8: se resume así) */
	for (i = 0, k = 0; i < na; i++)
		k += h_utf8_de_437((u8)a[i], (char *)u8c + k);
	crypto_wipe(a, sizeof a);
	h_escribe("Un momento...\n");
	h_azar(sal, 16);
	pbkdf2_sha256(u8c, k, sal, 16, VUELTAS, h);
	crypto_wipe(u8c, sizeof u8c);
	h_hex(sal, 16, hs);
	h_hex(h, 32, hh);
	formatear(t, sizeof t, "pbkdf2-sha256$%u$%s$%s\r\n", VUELTAS, hs, hh);
	ruta_de(r, "CLAVE.TXT");
	if (h_escribe_archivo(r, t, strlen(t)) < 0) {
		h_escribe("No se puede escribir en ");
		h_escribe(r);
		h_escribe(" (¿se ha arrancado con «escritura»?).\n");
		return 1;
	}
	h_escribe("Contraseña puesta (guardada resumida, no en claro).\n");
	return 0;
}

/* ---- las llaves autorizadas ---- */
static int autorizada(const u8 *blob, u32 nb)
{
	static char t[16384];
	static u8 b[600];
	char r[80];
	int n, i = 0, k;
	const char *nombres[2] = { "AUTORIZA.TXT", "authorized_keys" };

	for (k = 0; k < 2; k++) {
		ruta_de(r, nombres[k]);
		n = h_lee_archivo(r, t, sizeof t - 1);
		if (n > 0)
			break;
	}
	if (n <= 0)
		return 0;
	t[n] = 0;
	while (i < n) {
		int ini = i, fin, a;
		while (i < n && t[i] != '\n')
			i++;
		fin = i++;
		while (ini < fin && (t[ini] == ' ' || t[ini] == '\t'))
			ini++;
		if (ini >= fin || t[ini] == '#')
			continue;
		/* «tipo base64 [comentario]» */
		while (ini < fin && t[ini] != ' ' && t[ini] != '\t')
			ini++;
		while (ini < fin && (t[ini] == ' ' || t[ini] == '\t'))
			ini++;
		a = ini;
		while (a < fin && t[a] != ' ' && t[a] != '\t' && t[a] != '\r')
			a++;
		k = h_de_base64(t + ini, a - ini, b, sizeof b);
		if (k > 0 && (u32)k == nb && !memcmp(b, blob, nb))
			return 1;
	}
	return 0;
}

static int hay_autorizadas(void)
{
	char r[80];

	ruta_de(r, "AUTORIZA.TXT");
	if (h_existe(r))
		return 1;
	ruta_de(r, "authorized_keys");
	return h_existe(r);
}

/* ---- la autenticación ---- */
static int envia_fallo(void)
{
	struct sbuf b;
	char metodos[40];

	metodos[0] = 0;
	if (hay_autorizadas())
		formatear(metodos, sizeof metodos, "publickey");
	if (hay_clave())
		formatear(metodos + strlen(metodos), 20, "%spassword", metodos[0] ? "," : "");
	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, SSH_USERAUTH_FAILURE);
	sb_texto(&b, metodos);
	sb_u8(&b, 0);
	return ssh_envia(&S, m, b.n);
}

static int usuario_vale(const char *u)
{
	char mio[64];

	memset(mio, 0, sizeof mio);
	sc(SC_AJUSTES, 1, (u32)mio, 0);
	mio[63] = 0;
	if (!mio[0])
		return 1;	/* (sin usuario puesto en HISPA.INI: el que sea) */
	return h_es_ascii_igual(u, mio);
}

static int autentica(void)
{
	const u8 *pl;
	u32 n;
	int intentos = 0, r;
	u32 limite = h_tics() + 120 * h_hz;
	struct sbuf b;

	/* SERVICE_REQUEST «ssh-userauth» */
	for (;;) {
		r = ssh_lee(&S, &pl, &n, 50);
		if (r < 0)
			return -1;
		if ((int)(limite - h_tics()) <= 0)
			return -1;
		if (r == 0)
			continue;
		if (*pl == SSH_SERVICE_REQUEST) {
			struct slee l;
			const u8 *c;
			u32 nc;
			sl_inicia(&l, pl + 1, n - 1);
			c = sl_cad(&l, &nc);
			if (!sl_cad_es(c, nc, "ssh-userauth")) {
				ssh_corta(&S, 7, "solo ssh-userauth");
				return -1;
			}
			sb_inicia(&b, m, sizeof m);
			sb_u8(&b, SSH_SERVICE_ACCEPT);
			sb_texto(&b, "ssh-userauth");
			if (ssh_envia(&S, m, b.n) < 0)
				return -1;
			break;
		}
	}
	for (;;) {
		struct slee l;
		const u8 *srv, *met, *alg, *blob, *firma;
		u32 ns, nm, na, nb, nf;
		char usuario[64];
		r = ssh_lee(&S, &pl, &n, 50);
		if (r < 0)
			return -1;
		if ((int)(limite - h_tics()) <= 0) {
			ssh_corta(&S, 2, "demasiado tiempo sin entrar");
			return -1;
		}
		if (r == 0 || *pl != SSH_USERAUTH_REQUEST)
			continue;
		sl_inicia(&l, pl + 1, n - 1);
		sl_texto(&l, usuario, sizeof usuario);
		srv = sl_cad(&l, &ns);
		met = sl_cad(&l, &nm);
		if (l.mal || !sl_cad_es(srv, ns, "ssh-connection")) {
			envia_fallo();
			continue;
		}
		if (sl_cad_es(met, nm, "none")) {
			envia_fallo();
			continue;
		}
		if (!usuario_vale(usuario)) {
			h_apunta("usuario que no es el del equipo: %s", usuario);
			h_dormir(h_hz);
			envia_fallo();
			if (++intentos >= 6)
				goto fuera;
			continue;
		}
		if (sl_cad_es(met, nm, "publickey")) {
			int con_firma = sl_u8(&l);
			alg = sl_cad(&l, &na);
			blob = sl_cad(&l, &nb);
			if (l.mal || !sl_cad_es(alg, na, "ssh-ed25519") || nb != 51 || !autorizada(blob, nb)) {
				envia_fallo();
				continue;
			}
			if (!con_firma) {
				sb_inicia(&b, m, sizeof m);
				sb_u8(&b, SSH_USERAUTH_PK_OK);
				sb_cad(&b, alg, na);
				sb_cad(&b, blob, nb);
				if (ssh_envia(&S, m, b.n) < 0)
					return -1;
				continue;
			}
			firma = sl_cad(&l, &nf);
			{
				static u8 f[1024];
				struct slee fl;
				const u8 *fa, *fs;
				u32 nfa, nfs, nlf;
				u8 pub[32];
				nlf = ssh_lo_firmado(&S, f, sizeof f, usuario, alg, na, blob, nb);
				sl_inicia(&fl, firma, nf);
				fa = sl_cad(&fl, &nfa);
				fs = sl_cad(&fl, &nfs);
				memcpy(pub, blob + 19, 32);
				if (!l.mal && !fl.mal && nlf && sl_cad_es(fa, nfa, "ssh-ed25519") && nfs == 64 && !crypto_ed25519_check(fs, pub, f, nlf)) {
					h_apunta("entra %s con su llave", usuario);
					goto dentro;
				}
			}
			h_apunta("firma que no vale (%s)", usuario);
			envia_fallo();
			if (++intentos >= 6)
				goto fuera;
			continue;
		}
		if (sl_cad_es(met, nm, "password")) {
			const u8 *c;
			u32 nc;
			sl_u8(&l);
			c = sl_cad(&l, &nc);
			if (!l.mal && hay_clave() && comprueba_clave(c, nc)) {
				h_apunta("entra %s con la contraseña", usuario);
				goto dentro;
			}
			h_apunta("contraseña que no vale (%s)", usuario);
			h_dormir(2 * h_hz);
			envia_fallo();
			if (++intentos >= 3)
				goto fuera;
			continue;
		}
		envia_fallo();
	}
fuera:
	ssh_corta(&S, 14, "demasiados intentos");
	return -1;
dentro:
	m[0] = SSH_USERAUTH_SUCCESS;
	return ssh_envia(&S, m, 1);
}

/* ---- lo que se teclea al otro lado, a las teclas de Hispa-Pentium ---- */
static u16 teclas[1024];
static u32 t_ini, t_n;
static u8 esc[16];
static u32 esc_n, utf_v, utf_falta;

static void tecla(u32 c)
{
	if (t_n < 1024)
		teclas[(t_ini + t_n++) % 1024] = c;
}

static void secuencia(void)
{
	/* ESC [ ... o ESC O ... */
	u32 n = 0, i;
	u8 f = esc[esc_n - 1];

	for (i = 2; i < esc_n - 1; i++)
		if (esc[i] >= '0' && esc[i] <= '9')
			n = n * 10 + esc[i] - '0';
	switch (f) {
	case 'A': tecla(T_ARRIBA); return;
	case 'B': tecla(T_ABAJO); return;
	case 'C': tecla(T_DERECHA); return;
	case 'D': tecla(T_IZQUIERDA); return;
	case 'H': tecla(T_INICIO); return;
	case 'F': tecla(T_FIN); return;
	case 'P': tecla(T_F1); return;
	case 'Q': tecla(T_F1 + 1); return;
	case 'R': tecla(T_F1 + 2); return;
	case 'S': tecla(T_F1 + 3); return;
	case '~':
		switch (n) {
		case 1: case 7: tecla(T_INICIO); return;
		case 4: case 8: tecla(T_FIN); return;
		case 2: tecla(T_INSERT); return;
		case 3: tecla(T_SUPR); return;
		case 5: tecla(T_RE_PAG); return;
		case 6: tecla(T_AV_PAG); return;
		case 15: tecla(T_F1 + 4); return;
		case 17: case 18: case 19: case 20: case 21: tecla(T_F1 + n - 12); return;
		case 23: tecla(T_F11); return;
		case 24: tecla(T_F12); return;
		}
	}
}

static void teclea(const u8 *d, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		u8 c = d[i];
		if (esc_n) {
			esc[esc_n++] = c;
			if (esc_n == 2) {
				if (c != '[' && c != 'O') {
					/* Alt+tecla o Esc suelto: el Esc y lo que venga */
					tecla(27);
					esc_n = 0;
					if (c != 27)
						i--;
					else
						esc_n = 1, esc[0] = 27;
				}
				continue;
			}
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~' || esc_n >= sizeof esc) {
				secuencia();
				esc_n = 0;
			}
			continue;
		}
		if (utf_falta) {
			if ((c & 0xC0) != 0x80) {
				utf_falta = 0;
			} else {
				utf_v = utf_v << 6 | (c & 63);
				if (--utf_falta == 0)
					tecla(h_437_de_unicode(utf_v));
				continue;
			}
		}
		if (c == 27) {
			esc[0] = 27;
			esc_n = 1;
		} else if (c >= 0xC0 && c < 0xE0) {
			utf_v = c & 31, utf_falta = 1;
		} else if (c >= 0xE0 && c < 0xF0) {
			utf_v = c & 15, utf_falta = 2;
		} else if (c >= 0xF0) {
			utf_v = c & 7, utf_falta = 3;
		} else if (c == '\r') {
			tecla('\n');
		} else if (c == '\n') {
			/* (CR LF: el LF sobra; uno suelto, Intro) */
			if (i == 0 || d[i - 1] != '\r')
				tecla('\n');
		} else if (c == 127 || c == 8) {
			tecla(8);
		} else if (c == 3) {
			sc(SC_TERMINAL, 3, 0, 0);	/* Ctrl+C: acaba lo que corre dentro */
		} else if (c < 128) {
			tecla(c);
		}
	}
	/* un Esc solo al final del trozo: es la tecla Esc */
	if (esc_n == 1 && n > 0) {
		tecla(27);
		esc_n = 0;
	}
}

static void da_teclas(void)
{
	while (t_n) {
		if (sc(SC_TERMINAL, 2, teclas[t_ini], 0) != 0)
			break;		/* (el tubo, lleno: luego) */
		t_ini = (t_ini + 1) % 1024;
		t_n--;
	}
}

/* lo que escribe el intérprete, en UTF-8 y con CR LF */
static u32 a_utf8(const u8 *d, u32 n, u8 *o)
{
	u32 i, k = 0;

	for (i = 0; i < n; i++) {
		if (d[i] == '\n') {
			o[k++] = '\r';
			o[k++] = '\n';
		} else if (d[i] == 12) {
			memcpy(o + k, "\x1b[H\x1b[2J", 7);
			k += 7;
		} else if (d[i] >= 128) {
			k += h_utf8_de_437(d[i], (char *)o + k);
		} else
			o[k++] = d[i];
	}
	return k;
}

/* ---- la sesión: el canal, el terminal y el intérprete ---- */
static int envia_canal(u32 tipo, u32 suyo)
{
	struct sbuf b;

	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, tipo);
	sb_u32(&b, suyo);
	return ssh_envia(&S, m, b.n);
}

static int lanza_ordenes(const char *args)
{
	char r[40];
	u32 t;
	const char *sitios[3] = { "%c:\\ORDENES.BIN", "%c:\\HISPA\\ORDENES.BIN", "\\ORDENES.BIN" };
	int i;

	for (i = 0; i < 3; i++) {
		formatear(r, sizeof r, sitios[i], h_sistema);
		t = sc(SC_LANZAR, (u32)r, (u32)args, 0);
		if (!ES_ERROR(t))
			return t;
	}
	return -1;
}

static int sesion(void)
{
	static u8 sal[MAX_TROZO], u[MAX_TROZO * 3 + 16];
	const u8 *pl;
	u32 n, suyo = 0, ventana_suya = 0, max_suyo = 32768, mi_ventana = VENTANA;
	int abierto = 0, tarea = -1, cerrando = 0, r, acabado = 0;
	u32 ultimo_ini = h_tics();
	struct sbuf b;
	static char args[1100];

	t_ini = t_n = esc_n = utf_falta = 0;
	/* lo que quedara en el terminal de antes, fuera */
	while (sc(SC_TERMINAL, 1, (u32)sal, sizeof sal) > 0) ;
	for (;;) {
		int algo = 0;
		if (h_tics() - ultimo_ini > 2 * h_hz) {
			ultimo_ini = h_tics();
			if (!encendido()) {
				ssh_corta(&S, 11, "el servidor SSH se ha apagado");
				return -1;
			}
		}
		r = ssh_lee(&S, &pl, &n, tarea >= 0 ? 1 : 20);
		if (r < 0) {
			h_apunta("sesión acabada: %s", S.error);
			return -1;
		}
		if (r > 0) {
			struct slee l;
			algo = 1;
			sl_inicia(&l, pl + 1, n - 1);
			switch (*pl) {
			case SSH_CHANNEL_OPEN: {
				const u8 *tipo;
				u32 nt, su = 0;
				tipo = sl_cad(&l, &nt);
				su = sl_u32(&l);
				if (!abierto && sl_cad_es(tipo, nt, "session")) {
					suyo = su;
					ventana_suya = sl_u32(&l);
					max_suyo = sl_u32(&l);
					if (max_suyo > MAX_TROZO)
						max_suyo = MAX_TROZO;
					if (max_suyo < 256)
						max_suyo = 256;
					abierto = 1;
					sb_inicia(&b, m, sizeof m);
					sb_u8(&b, SSH_CHANNEL_OPEN_CONFIRMATION);
					sb_u32(&b, suyo);
					sb_u32(&b, 0);
					sb_u32(&b, VENTANA);
					sb_u32(&b, MAX_TROZO);
					if (ssh_envia(&S, m, b.n) < 0)
						return -1;
				} else {
					sb_inicia(&b, m, sizeof m);
					sb_u8(&b, SSH_CHANNEL_OPEN_FAILURE);
					sb_u32(&b, su);
					sb_u32(&b, 1);
					sb_texto(&b, abierto ? "una sesión a la vez" : "solo «session»");
					sb_texto(&b, "");
					if (ssh_envia(&S, m, b.n) < 0)
						return -1;
				}
				break;
			}
			case SSH_CHANNEL_REQUEST: {
				const u8 *q;
				u32 nq, quiere;
				int bien = 0;
				sl_u32(&l);
				q = sl_cad(&l, &nq);
				quiere = sl_u8(&l);
				if (sl_cad_es(q, nq, "pty-req") || sl_cad_es(q, nq, "window-change") || sl_cad_es(q, nq, "env")) {
					bien = !sl_cad_es(q, nq, "env");
				} else if ((sl_cad_es(q, nq, "shell") || sl_cad_es(q, nq, "exec")) && tarea < 0) {
					args[0] = 0;
					if (sl_cad_es(q, nq, "exec")) {
						const u8 *o;
						u32 no, i;
						o = sl_cad(&l, &no);
						memcpy(args, "/C ", 3);
						for (i = 0; i < no && i < 1000; i++)
							args[3 + i] = o[i] >= 128 ? '?' : o[i];
						args[3 + i] = 0;
						h_apunta("orden: %s", args + 3);
					}
					tarea = lanza_ordenes(args);
					bien = tarea >= 0;
					if (!bien)
						h_apunta("no se puede lanzar ORDENES.BIN");
				} else if (sl_cad_es(q, nq, "signal") || sl_cad_es(q, nq, "break")) {
					sc(SC_TERMINAL, 3, 0, 0);
					bien = 1;
				}
				if (quiere && envia_canal(bien ? SSH_CHANNEL_SUCCESS : SSH_CHANNEL_FAILURE, suyo) < 0)
					return -1;
				break;
			}
			case SSH_CHANNEL_DATA: {
				const u8 *d;
				u32 nd;
				sl_u32(&l);
				d = sl_cad(&l, &nd);
				if (!l.mal) {
					teclea(d, nd);
					mi_ventana -= nd < mi_ventana ? nd : mi_ventana;
					if (mi_ventana < VENTANA / 2) {
						sb_inicia(&b, m, sizeof m);
						sb_u8(&b, SSH_CHANNEL_WINDOW_ADJUST);
						sb_u32(&b, suyo);
						sb_u32(&b, VENTANA - mi_ventana);
						mi_ventana = VENTANA;
						if (ssh_envia(&S, m, b.n) < 0)
							return -1;
					}
				}
				break;
			}
			case SSH_CHANNEL_WINDOW_ADJUST:
				sl_u32(&l);
				ventana_suya += sl_u32(&l);
				break;
			case SSH_CHANNEL_EOF:
				break;
			case SSH_CHANNEL_CLOSE:
				if (!cerrando)
					envia_canal(SSH_CHANNEL_CLOSE, suyo);
				h_apunta("sesión cerrada");
				return 0;
			case SSH_GLOBAL_REQUEST: {
				const u8 *q;
				u32 nq;
				q = sl_cad(&l, &nq);
				(void)q;
				if (sl_u8(&l)) {
					m[0] = SSH_REQUEST_FAILURE;
					if (ssh_envia(&S, m, 1) < 0)
						return -1;
				}
				break;
			}
			}
		}
		da_teclas();
		/* lo que ha escrito el intérprete */
		if (tarea >= 0 && !cerrando) {
			u32 cabe = ventana_suya < max_suyo ? ventana_suya : max_suyo, k;
			if (cabe > 64) {
				k = sc(SC_TERMINAL, 1, (u32)sal, cabe / 3 > sizeof sal ? sizeof sal : cabe / 3);
				if (k > 0 && !ES_ERROR(k)) {
					u32 nu = a_utf8(sal, k, u);
					algo = 1;
					sb_inicia(&b, m, sizeof m);
					sb_u8(&b, SSH_CHANNEL_DATA);
					sb_u32(&b, suyo);
					sb_cad(&b, u, nu);
					if (ssh_envia(&S, m, b.n) < 0)
						return -1;
					ventana_suya -= nu;
					continue;
				}
			}
			if (acabado || sc(SC_ESPERAR, tarea, 1, 0) != SIGUE) {
				/* ha acabado el intérprete (SALIR): lo último y fuera */
				if (!acabado) {
					acabado = 1;
					continue;	/* (una vuelta más, por lo que quede en el tubo) */
				}
				sb_inicia(&b, m, sizeof m);
				sb_u8(&b, SSH_CHANNEL_REQUEST);
				sb_u32(&b, suyo);
				sb_texto(&b, "exit-status");
				sb_u8(&b, 0);
				sb_u32(&b, 0);
				ssh_envia(&S, m, b.n);
				envia_canal(SSH_CHANNEL_EOF, suyo);
				envia_canal(SSH_CHANNEL_CLOSE, suyo);
				cerrando = 1;
				h_apunta("el intérprete ha acabado");
			}
		}
		(void)algo;
	}
}

static void atiende(u32 c, u32 ip)
{
	char t[16];

	h_ip_a_texto(ip, t);
	h_apunta("llega una conexión de %s", t);
	ssh_prepara(&S, 1, c);
	if (ssh_empieza(&S) < 0) {
		h_apunta("no se ha podido empezar: %s", S.error);
	} else if (autentica() < 0) {
		h_apunta("no ha entrado: %s", S.error[0] ? S.error : "sin autenticar");
	} else
		sesion();
	h_cerrar_red(c);
	crypto_wipe(S.k_ent, sizeof S.k_ent);
	crypto_wipe(S.k_sal, sizeof S.k_sal);
}

static int servicio(void)
{
	u32 l, c, ip, pu, visto;

	sc(SC_ATENDER_CORTE, 1, 0, 0);
	lee_puerto();
	if (llave_equipo(1) < 0) {
		h_apunta("no se puede hacer la llave del equipo en %s", carpeta);
		return 1;
	}
	if (!hay_clave() && !hay_autorizadas())
		h_apunta("ojo: no hay contraseña (SSHD /CLAVE) ni llaves autorizadas: nadie podrá entrar");
	l = h_escuchar(puerto);
	if (ES_ERROR(l)) {
		h_apunta("no se puede escuchar en el puerto %u: %s", puerto, h_texto_error_red(l));
		return 1;
	}
	term_hecho = sc(SC_TERMINAL, 0, 0, 0);
	if (ES_ERROR(term_hecho) || term_hecho == 0) {
		h_apunta("no queda ningún terminal del núcleo libre");
		return 1;
	}
	{
		char hu[64];
		huella_equipo(hu);
		h_apunta("Hispa-Pentium: el servidor SSH, en el puerto %u (llave %s)", puerto, hu);
	}
	visto = h_tics();
	for (;;) {
		c = h_aceptar(l, 2 * h_hz, &ip, &pu);
		if (!ES_ERROR(c))
			atiende(c, ip);
		else if (c != TIEMPO) {
			h_apunta("al esperar conexiones: %s", h_texto_error_red(c));
			h_dormir(2 * h_hz);
		}
		if (h_tics() - visto >= 2 * h_hz) {
			visto = h_tics();
			if (!encendido()) {
				h_apunta("apagado (SSHD /APAGAR)");
				h_cerrar_red(l);
				return 0;
			}
		}
	}
}

static void estado(void)
{
	char hu[64];

	lee_puerto();
	h_escribef("El servidor SSH de Hispa-Pentium: %s (puerto %u).\n", encendido() ? "encendido" : "apagado", puerto);
	if (llave_equipo(0) == 0) {
		huella_equipo(hu);
		h_escribef("La llave de este equipo: ssh-ed25519 %s\n", hu);
	} else
		h_escribe("Aún no tiene llave (se hace al encenderlo).\n");
	h_escribef("Contraseña: %s. Llaves autorizadas (%s\\AUTORIZA.TXT): %s.\n", hay_clave() ? "puesta" : "sin poner (SSHD /CLAVE)",
		   carpeta, hay_autorizadas() ? "sí" : "ninguna");
	h_escribe("Para entrar desde otro equipo: ssh usuario@");
	{
		u32 ip = h_mi_ip();
		char t[16];
		if (ip) {
			h_ip_a_texto(ip, t);
			h_escribe(t);
		} else
			h_escribe("(la IP de este equipo)");
	}
	if (puerto != 22)
		h_escribef(" -p %u", puerto);
	h_escribe("\n");
}

static void ayuda(void)
{
	h_escribe("SSHD: el servidor SSH (entrar en la Consola de este equipo desde otro).\n"
		  "  SSHD             cómo está\n"
		  "  SSHD /ENCENDER   lo enciende (y en los arranques siguientes)\n"
		  "  SSHD /APAGAR     lo apaga\n"
		  "  SSHD /CLAVE      pone la contraseña para entrar\n"
		  "  SSHD /PUERTO=n   el puerto (22 si no se dice)\n"
		  "Las llaves públicas que pueden entrar sin contraseña, en \\HISPA\\SSH\\AUTORIZA.TXT.\n");
}

int principal(void)
{
	char a[128];

	h_argumentos(a, sizeof a);
	h_inicia("sshd");
	h_ruta(carpeta, sizeof carpeta, "SSH");
	h_carpeta(carpeta);
	if (h_empieza(a, "/SERVICIO"))
		return servicio();
	if (h_empieza(a, "/PUERTO=")) {
		u32 p = 0;
		int i;
		for (i = 8; a[i] >= '0' && a[i] <= '9'; i++)
			p = p * 10 + a[i] - '0';
		if (!p || p > 65535) {
			h_escribe("Ese puerto no vale.\n");
			return 1;
		}
		puerto = p;
		pon_ini(encendido());
		h_escribef("Puerto %u (si está encendido, vale desde que se vuelva a encender).\n", p);
		return 0;
	}
	if (h_empieza(a, "/ENCENDER")) {
		u32 t;
		lee_puerto();
		pon_ini(1);
		if (!hay_clave() && !hay_autorizadas())
			h_escribe("Ojo: no hay contraseña ni llaves autorizadas; póngala con SSHD /CLAVE.\n");
		{
			char r[40];
			formatear(r, sizeof r, "%c:\\HISPA\\SSHD.BIN", h_sistema);
			t = sc(SC_LANZAR_SUELTO, (u32)r, (u32)"/SERVICIO", 0);
			if (ES_ERROR(t)) {
				formatear(r, sizeof r, "%c:\\SSHD.BIN", h_sistema);
				t = sc(SC_LANZAR_SUELTO, (u32)r, (u32)"/SERVICIO", 0);
			}
		}
		if (ES_ERROR(t)) {
			h_escribe("No se puede poner en marcha SSHD.BIN.\n");
			return 1;
		}
		h_escribef("Servidor SSH encendido (puerto %u).\n", puerto);
		return 0;
	}
	if (h_empieza(a, "/APAGAR")) {
		lee_puerto();
		pon_ini(0);
		h_escribe("Servidor SSH apagado (si había alguien dentro, sale en unos segundos).\n");
		return 0;
	}
	if (h_empieza(a, "/CLAVE"))
		return pon_clave();
	if (a[0] == '/' || a[0] == '?') {
		ayuda();
		return 0;
	}
	estado();
	return 0;
}
