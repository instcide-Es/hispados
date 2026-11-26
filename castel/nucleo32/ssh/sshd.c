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
 *
 * (USUARIOS) Con usuarios (un sistema instalado desde la 0.9: cuentas.cas),
 * se entra con el nombre de uno de ellos y SU contraseña (la comprueba el
 * núcleo, la llamada 202: Argon2id; los que no tienen contraseña no entran
 * con contraseña) o con una llave de SU \USUARIOS\<carpeta>\AJUSTES\
 * AUTORIZA.TXT (y, los administradores, las de \HISPA\SSH\AUTORIZA.TXT de
 * antes). El intérprete que se lanza es de ese usuario (lee su carpeta y no
 * las de los demás). La contraseña de SSHD /CLAVE de antes vale solo para un
 * administrador que aún no tiene la suya (la migración). Encender, apagar,
 * el puerto y /CLAVE, solo un administrador.
 *
 * (REMOTA) SSHD /REMOTA: la consola remota «Dejar entrar a Claude»
 * (remota.c): solo mientras su ventana está abierta, en el puerto 2323, con
 * un código de 6 cifras de un solo uso que se ve en la pantalla (la
 * «contraseña»); dentro, el intérprete y copiar archivos (remota:leer y
 * remota:escribir). docs/PENTIUM.md, «La consola remota».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ssh.h"
#include "remota.h"
#include "cripto.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"

#define VUELTAS 10000		/* las de PBKDF2 (en un Pentium a 133 MHz, alrededor de 1 s) */
#define VENTANA 65536		/* lo que se deja mandar sin avisar */
#define MAX_TROZO 16384

struct ssh S;
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
int llave_equipo(int hacer)
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

void huella_equipo(char *t)
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

int lee_secreto(char *d, int cabe)
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
static int autorizada_en(const char *ruta, const u8 *blob, u32 nb);
static int autorizada(const u8 *blob, u32 nb)
{
	char r[80];

	ruta_de(r, "AUTORIZA.TXT");
	if (h_existe(r))
		return autorizada_en(r, blob, nb);
	ruta_de(r, "authorized_keys");
	return autorizada_en(r, blob, nb);
}

/* ¿está la llave «blob» en el archivo «ruta» (como el authorized_keys de OpenSSH)? */
static int autorizada_en(const char *ruta, const u8 *blob, u32 nb)
{
	static char t[16384];
	static u8 b[600];
	int n, i = 0, k;

	n = h_lee_archivo(ruta, t, sizeof t - 1);
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
	if (remota)
		formatear(metodos, sizeof metodos, "password");		/* (REMOTA) el código */
	else if (con_usuarios())
		formatear(metodos, sizeof metodos, "publickey,password");	/* (USUARIOS) */
	else if (hay_autorizadas())
		formatear(metodos, sizeof metodos, "publickey");
	if (!remota && !con_usuarios() && hay_clave())
		formatear(metodos + strlen(metodos), 20, "%spassword", metodos[0] ? "," : "");
	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, SSH_USERAUTH_FAILURE);
	sb_texto(&b, metodos);
	sb_u8(&b, 0);
	return ssh_envia(&S, m, b.n);
}

/* ---- (USUARIOS) los usuarios del sistema: las fichas, en remota.h ---- */
int con_usuarios(void)
{
	u32 n = sc(SC_USUARIOS, 0, 0, 0);

	return !ES_ERROR(n) && n > 0;
}

int soy_administrador(void)
{
	return !con_usuarios() || sc(SC_USUARIOS, 3, 0, 0) == 1;
}

/* el nombre que llega por SSH (UTF-8), a la página 437 (la de los nombres) */
static void a_437(const char *u, char *d, int cabe)
{
	const u8 *p = (const u8 *)u;
	int k = 0;

	while (*p && k < cabe - 1) {
		u32 c = *p++;
		if (c >= 0xC0 && c < 0xE0 && (*p & 0xC0) == 0x80)
			c = (c & 31) << 6 | (*p++ & 63);
		else if (c >= 0xE0 && c < 0xF0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) {
			c = (c & 15) << 12 | (p[0] & 63) << 6 | (p[1] & 63);
			p += 2;
		} else if (c >= 0x80)
			c = '?';
		d[k++] = h_437_de_unicode(c);
	}
	d[k] = 0;
}

/* la ficha del usuario «u» (en UTF-8): 1 si lo hay */
int busca_usuario(const char *u, struct ficha_u *f)
{
	char n[24];
	u32 i;

	a_437(u, n, sizeof n);
	for (i = 0; i < 16 && sc(SC_USUARIOS, 1, i, (u32)f) == 1; i++)
		if (h_es_ascii_igual(f->nombre, n))
			return 1;
	return 0;
}

/* el núcleo: ¿es esa su contraseña? (a = 0), o el de una llave ya
 * comprobada (a = 1). Si vale, el intérprete que se lance será suyo */
int entra_usuario(const struct ficha_u *f, u32 a, const u8 *c, u32 nc)
{
	static struct pedido_u p;
	u32 r;

	memset(&p, 0, sizeof p);
	h_copia(p.nombre, f->nombre, sizeof p.nombre);
	if (nc > 127)
		nc = 127;
	if (c)
		memcpy(p.clave, c, nc);
	p.n_clave = nc;
	r = sc(SC_ENTRAR_SSH, a, (u32)&p, 0);
	crypto_wipe(&p, sizeof p);
	return !ES_ERROR(r);
}

/* ¿es la llave «blob» de las que deja entrar al usuario f? (su AUTORIZA.TXT;
 * un administrador, también las de \HISPA\SSH de antes) */
static int autorizada_en(const char *ruta, const u8 *blob, u32 nb);
static int autorizada_de(const struct ficha_u *f, const u8 *blob, u32 nb)
{
	char r[80];

	if (f->carpeta[0]) {
		formatear(r, sizeof r, "%c:\\USUARIOS\\%s\\AJUSTES\\AUTORIZA.TXT", h_sistema, f->carpeta);
		if (autorizada_en(r, blob, nb))
			return 1;
	}
	if (!f->admin)
		return 0;
	ruta_de(r, "AUTORIZA.TXT");
	if (autorizada_en(r, blob, nb))
		return 1;
	ruta_de(r, "authorized_keys");
	return autorizada_en(r, blob, nb);
}

static int usuario_vale(const char *u)
{
	char mio[64];
	struct ficha_u f;

	if (con_usuarios())
		return busca_usuario(u, &f);		/* (USUARIOS) */

	memset(mio, 0, sizeof mio);
	sc(SC_AJUSTES, 1, (u32)mio, 0);
	mio[63] = 0;
	if (!mio[0])
		return 1;	/* (sin usuario puesto en HISPA.INI: el que sea) */
	return h_es_ascii_igual(u, mio);
}

static char quien[64];		/* (REMOTA) el nombre con el que ha entrado */

static int autentica(void)
{
	const u8 *pl;
	u32 n;
	int intentos = 0, r;
	u32 limite = h_tics() + (remota ? 45 : 120) * h_hz;
	struct sbuf b;

	quien[0] = 0;
	/* SERVICE_REQUEST «ssh-userauth» */
	for (;;) {
		r = ssh_lee(&S, &pl, &n, remota ? 10 : 50);
		if (r < 0)
			return -1;
		if ((int)(limite - h_tics()) <= 0)
			return -1;
		if (remota && remota_vigila()) {
			ssh_corta(&S, 11, "el dueño del equipo ha cerrado la puerta");
			return -1;
		}
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
		struct ficha_u fu;
		r = ssh_lee(&S, &pl, &n, remota ? 10 : 50);
		if (r < 0)
			return -1;
		if ((int)(limite - h_tics()) <= 0) {
			ssh_corta(&S, 2, "demasiado tiempo sin entrar");
			return -1;
		}
		if (remota && remota_vigila()) {
			ssh_corta(&S, 11, "el dueño del equipo ha cerrado la puerta");
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
		if (remota) {
			/* (REMOTA) el nombre da igual: entra quien tenga el código de la
			 * pantalla (la «contraseña»); ni llaves ni contraseñas de nadie */
			const u8 *c;
			u32 nc;
			int bien;
			if (!sl_cad_es(met, nm, "password")) {
				envia_fallo();
				continue;
			}
			sl_u8(&l);
			c = sl_cad(&l, &nc);
			bien = l.mal ? 0 : remota_codigo(usuario, c, nc);
			if (bien > 0) {
				h_copia(quien, usuario, sizeof quien);
				goto dentro;
			}
			/* (remota_codigo ya ha esperado 2 s; con el tercer fallo con un
			 * código, o sin código vigente —en la pausa—, fuera) */
			if (bien < 0 || ++intentos >= 3)
				goto fuera;
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
			if (l.mal || !sl_cad_es(alg, na, "ssh-ed25519") || nb != 51 ||
			    (con_usuarios() ? !busca_usuario(usuario, &fu) || !autorizada_de(&fu, blob, nb) : !autorizada(blob, nb))) {
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
				if (!l.mal && !fl.mal && nlf && sl_cad_es(fa, nfa, "ssh-ed25519") && nfs == 64 && !crypto_ed25519_check(fs, pub, f, nlf) &&
				    (!con_usuarios() || entra_usuario(&fu, 1, 0, 0))) {
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
			if (!l.mal && con_usuarios()) {
				/* (USUARIOS) la suya, la comprueba el núcleo; la de SSHD /CLAVE
				 * de antes, solo un administrador que aún no tiene la suya */
				if (busca_usuario(usuario, &fu) &&
				    (fu.con_clave ? entra_usuario(&fu, 0, c, nc) : fu.admin && hay_clave() && comprueba_clave(c, nc) && entra_usuario(&fu, 1, 0, 0))) {
					h_apunta("entra %s con la contraseña", usuario);
					goto dentro;
				}
			} else if (!l.mal && hay_clave() && comprueba_clave(c, nc)) {
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
static int con_pty;		/* (sin terminal —una orden—, los finales de línea van como en Unix) */

static u32 a_utf8(const u8 *d, u32 n, u8 *o)
{
	u32 i, k = 0;

	for (i = 0; i < n; i++) {
		if (d[i] == '\n' && con_pty) {
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
		if (remota)
			remota_lanza();		/* (REMOTA) del administrador que la abrió */
		t = sc(SC_LANZAR, (u32)r, (u32)args, 0);
		if (!ES_ERROR(t))
			return t;
	}
	return -1;
}

/* (REMOTA) copiar archivos por el canal, sin el intérprete: «remota:leer
 * RUTA» manda el archivo tal cual (como «cat»); «remota:escribir RUTA» lo
 * escribe con lo que llegue hasta el EOF. La ruta llega en UTF-8 */
#define ARCH_NADA 0
#define ARCH_LEER 1
#define ARCH_ESCRIBIR 2

static int envia_estado(u32 suyo, u32 codigo)
{
	struct sbuf b;

	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, SSH_CHANNEL_REQUEST);
	sb_u32(&b, suyo);
	sb_texto(&b, "exit-status");
	sb_u8(&b, 0);
	sb_u32(&b, codigo);
	return ssh_envia(&S, m, b.n);
}

static int envia_error(u32 suyo, const char *t)
{
	struct sbuf b;

	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, SSH_CHANNEL_EXTENDED_DATA);
	sb_u32(&b, suyo);
	sb_u32(&b, 1);
	sb_texto(&b, t);
	return ssh_envia(&S, m, b.n);
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
	int arch = ARCH_NADA;		/* (REMOTA) copiar un archivo */
	u32 fa = 0, n_arch = 0;		/* su archivo abierto y lo que se lleva */
	int con_fa = 0;			/* (el número 0 es un archivo como otro: aparte) */
	int arch_mal = 0;

	t_ini = t_n = esc_n = utf_falta = 0;
	con_pty = 0;
	/* lo que quedara en el terminal de antes, fuera */
	while (sc(SC_TERMINAL, 1, (u32)sal, sizeof sal) > 0) ;
	for (;;) {
		int algo = 0;
		if (!remota && h_tics() - ultimo_ini > 2 * h_hz) {
			ultimo_ini = h_tics();
			if (!encendido()) {
				ssh_corta(&S, 11, "el servidor SSH se ha apagado");
				return -1;
			}
		}
		if (remota && remota_vigila()) {
			/* (REMOTA) «Echar», o se cierra la ventana */
			ssh_corta(&S, 11, "el dueño del equipo ha cerrado la sesión");
			h_apunta("echado por el dueño del equipo");
			if (con_fa)
				sc(SC_CERRAR, fa, 0, 0);
			return 1;
		}
		/* (REMOTA) un canal acabado (los dos han mandado CLOSE): se puede abrir otro */
		if (remota && !abierto && cerrando) {
			cerrando = acabado = 0;
			tarea = -1;
		}
		r = ssh_lee(&S, &pl, &n, (tarea >= 0 || arch == ARCH_LEER) ? 1 : (remota ? 5 : 20));
		if (r < 0) {
			h_apunta("sesión acabada: %s", S.error);
			if (con_fa)
				sc(SC_CERRAR, fa, 0, 0);
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
				if (!abierto && !cerrando && sl_cad_es(tipo, nt, "session")) {
					suyo = su;
					ventana_suya = sl_u32(&l);
					max_suyo = sl_u32(&l);
					if (max_suyo > MAX_TROZO)
						max_suyo = MAX_TROZO;
					if (max_suyo < 256)
						max_suyo = 256;
					abierto = 1;
					mi_ventana = VENTANA;
					con_pty = 0;
					t_ini = t_n = esc_n = utf_falta = 0;
					arch = ARCH_NADA;
					arch_mal = 0;
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
				if (sl_cad_es(q, nq, "pty-req"))
					con_pty = 1;
				if (sl_cad_es(q, nq, "pty-req") || sl_cad_es(q, nq, "window-change") || sl_cad_es(q, nq, "env")) {
					bien = !sl_cad_es(q, nq, "env");
				} else if ((sl_cad_es(q, nq, "shell") || sl_cad_es(q, nq, "exec")) && tarea < 0 && arch == ARCH_NADA && abierto && !cerrando) {
					args[0] = 0;
					if (sl_cad_es(q, nq, "exec")) {
						const u8 *o;
						u32 no, i;
						o = sl_cad(&l, &no);
						memcpy(args, "/C ", 3);
						for (i = 0; i < no && i < 1000; i++)
							args[3 + i] = o[i] >= 128 ? '?' : o[i];
						args[3 + i] = 0;
						/* (REMOTA) copiar archivos: remota:leer RUTA, remota:escribir RUTA */
						if (remota && (h_empieza(args + 3, "remota:leer ") || h_empieza(args + 3, "remota:escribir "))) {
							static char u8r[1004];
							char ruta[300], t[340];
							int leer = h_empieza(args + 3, "remota:leer ");
							memcpy(u8r, o, no < 1000 ? no : 1000);
							u8r[no < 1000 ? no : 1000] = 0;
							a_437(u8r + (leer ? 12 : 16), ruta, sizeof ruta);
							fa = sc(SC_ABRIR, (u32)ruta, leer ? 0 : 2, 0);
							formatear(t, sizeof t, "%s %s", leer ? "lee" : "escribe", ruta);
							remota_hace(t);
							h_apunta("%s", t);
							n_arch = 0;
							con_fa = !ES_ERROR(fa);
							if (!con_fa) {
								fa = 0;
								formatear(t, sizeof t, "remota: no se puede %s %s\n", leer ? "leer" : "escribir", ruta);
								bien = 1;
								if (quiere && envia_canal(SSH_CHANNEL_SUCCESS, suyo) < 0)
									return -1;
								quiere = 0;
								envia_error(suyo, t);
								envia_estado(suyo, 1);
								envia_canal(SSH_CHANNEL_EOF, suyo);
								envia_canal(SSH_CHANNEL_CLOSE, suyo);
								cerrando = 1;
							} else {
								arch = leer ? ARCH_LEER : ARCH_ESCRIBIR;
								bien = 1;
							}
							goto contesta;
						}
						h_apunta("orden: %s", args + 3);
						if (remota)
							remota_hace(args + 3);
					} else if (remota)
						remota_hace("(la consola)");
					while (sc(SC_TERMINAL, 1, (u32)sal, sizeof sal) > 0) ;
					tarea = lanza_ordenes(args);
					bien = tarea >= 0;
					if (!bien)
						h_apunta("no se puede lanzar ORDENES.BIN");
				} else if (sl_cad_es(q, nq, "signal") || sl_cad_es(q, nq, "break")) {
					sc(SC_TERMINAL, 3, 0, 0);
					bien = 1;
				}
contesta:
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
					if (arch == ARCH_ESCRIBIR) {
						/* (REMOTA) lo que llega, al archivo */
						if (con_fa && !arch_mal && nd && sc(SC_ESCRIBIR_ARCHIVO, fa, (u32)d, nd) != nd)
							arch_mal = 1;
						n_arch += nd;
					} else if (arch == ARCH_NADA)
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
				if (arch == ARCH_ESCRIBIR) {
					/* (REMOTA) ya está todo: se cierra el archivo y se dice cómo ha ido */
					char t[80];
					if (con_fa)
						sc(SC_CERRAR, fa, 0, 0);
					fa = 0, con_fa = 0;
					arch = ARCH_NADA;
					h_apunta("escritos %u octetos%s", n_arch, arch_mal ? " (con un error)" : "");
					if (arch_mal) {
						formatear(t, sizeof t, "remota: error al escribir (¿lleno, o de solo lectura?)\n");
						envia_error(suyo, t);
					}
					envia_estado(suyo, arch_mal ? 1 : 0);
					envia_canal(SSH_CHANNEL_EOF, suyo);
					envia_canal(SSH_CHANNEL_CLOSE, suyo);
					cerrando = 1;
				}
				break;
			case SSH_CHANNEL_CLOSE:
				if (!cerrando)
					envia_canal(SSH_CHANNEL_CLOSE, suyo);
				h_apunta("sesión cerrada");
				if (!remota)
					return 0;
				/* (REMOTA) el canal se acaba, la conexión no: puede venir otro
				 * (ssh -S, con ControlMaster: un código para varias cosas) */
				if (con_fa)
					sc(SC_CERRAR, fa, 0, 0);
				fa = 0, con_fa = 0;
				arch = ARCH_NADA;
				if (tarea >= 0 && sc(SC_ESPERAR, tarea, 1, 0) == SIGUE) {
					/* el intérprete sigue: Ctrl+C y SALIR (y, si no, que siga hasta el final) */
					u32 hasta = h_tics() + 3 * h_hz;
					const char *salir = "\nsalir\n";
					sc(SC_TERMINAL, 3, 0, 0);
					for (; *salir; salir++)
						tecla(*salir);
					while ((int)(hasta - h_tics()) > 0 && sc(SC_ESPERAR, tarea, 1, 0) == SIGUE) {
						da_teclas();
						while (sc(SC_TERMINAL, 1, (u32)sal, sizeof sal) > 0) ;
						h_dormir(2);
					}
				}
				while (sc(SC_TERMINAL, 1, (u32)sal, sizeof sal) > 0) ;
				abierto = 0;
				cerrando = 1;	/* (y en la vuelta siguiente, todo a cero) */
				t_ini = t_n = 0;
				break;
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
		/* (REMOTA) un archivo que se lee: a trozos, lo que deje la ventana del otro */
		if (arch == ARCH_LEER && !cerrando) {
			u32 cabe = ventana_suya < max_suyo ? ventana_suya : max_suyo, k;
			if (cabe > sizeof sal)
				cabe = sizeof sal;
			if (cabe >= 1024) {
				k = sc(SC_LEER, fa, (u32)sal, cabe - 64);
				if (!ES_ERROR(k) && k > 0) {
					sb_inicia(&b, m, sizeof m);
					sb_u8(&b, SSH_CHANNEL_DATA);
					sb_u32(&b, suyo);
					sb_cad(&b, sal, k);
					if (ssh_envia(&S, m, b.n) < 0)
						return -1;
					ventana_suya -= k;
					n_arch += k;
					continue;
				}
				sc(SC_CERRAR, fa, 0, 0);
				fa = 0, con_fa = 0;
				arch = ARCH_NADA;
				h_apunta("leídos %u octetos", n_arch);
				if (ES_ERROR(k))
					envia_error(suyo, "remota: error al leer\n");
				envia_estado(suyo, ES_ERROR(k) ? 1 : 0);
				envia_canal(SSH_CHANNEL_EOF, suyo);
				envia_canal(SSH_CHANNEL_CLOSE, suyo);
				cerrando = 1;
			}
		}
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
				envia_estado(suyo, 0);
				envia_canal(SSH_CHANNEL_EOF, suyo);
				envia_canal(SSH_CHANNEL_CLOSE, suyo);
				cerrando = 1;
				h_apunta("el intérprete ha acabado");
			}
		}
		(void)algo;
	}
}

int atiende(u32 c, u32 ip, u32 pu)
{
	char t[16];
	int dentro = 0;

	h_ip_a_texto(ip, t);
	h_apunta("llega una conexión de %s:%u", t, pu);
	ssh_prepara(&S, 1, c);
	if (ssh_empieza(&S) < 0) {
		h_apunta("no se ha podido empezar: %s", S.error);
	} else if (autentica() < 0) {
		h_apunta("no ha entrado: %s", S.error[0] ? S.error : "sin autenticar");
	} else {
		dentro = 1;
		if (remota)
			remota_entra(quien);
		sesion();
	}
	h_cerrar_red(c);
	crypto_wipe(S.k_ent, sizeof S.k_ent);
	crypto_wipe(S.k_sal, sizeof S.k_sal);
	return dentro;
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
	if (!con_usuarios() && !hay_clave() && !hay_autorizadas())
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
			atiende(c, ip, pu);
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
	if (con_usuarios())
		h_escribe("Se entra con el nombre y la contraseña de un usuario de este equipo (sin contraseña,\n"
			  "solo con su llave: \\USUARIOS\\<su carpeta>\\AJUSTES\\AUTORIZA.TXT).\n");
	else
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
		  "  SSHD /REMOTA     «Dejar entrar a Claude»: la consola remota, con un código\n"
		  "                   de un solo uso en la pantalla, mientras está abierta\n"
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
	/* (REMOTA) «Dejar entrar a Claude»: la consola remota (remota.c) */
	if (h_empieza(a, "/REMOTA"))
		return remota_principal(a + 7);
	/* (al arrancar el escritorio: solo si se dejó encendido) */
	if (h_empieza(a, "/ARRANQUE"))
		return encendido() ? servicio() : 0;
	/* (USUARIOS) cambiarlo, solo un administrador */
	if ((h_empieza(a, "/PUERTO=") || h_empieza(a, "/ENCENDER") || h_empieza(a, "/APAGAR") || h_empieza(a, "/CLAVE")) && !soy_administrador()) {
		h_escribe("Eso solo lo puede hacer un administrador.\n");
		return 1;
	}
	if (h_empieza(a, "/CLAVE") && con_usuarios()) {
		h_escribe("Con usuarios, por SSH se entra con el nombre y la contraseña de cada uno\n"
			  "(Configuración > Usuarios, o USUARIOS /CLAVE nombre en la Consola).\n");
		return 1;
	}
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
		if (!con_usuarios() && !hay_clave() && !hay_autorizadas())
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
