/* ssh.c - el transporte SSH-2 de Hispa-Pentium (ssh.h): versiones, paquetes,
 * intercambio de claves curve25519-sha256 con la llave ssh-ed25519, y el
 * cifrado chacha20-poly1305@openssh.com. Escrito a partir de las RFC 4251 a
 * 4254, 8709, 8731 y de PROTOCOL y PROTOCOL.chacha20poly1305 de OpenSSH
 * (la descripción, no su código). Con «kex-strict» (la defensa contra
 * Terrapin, CVE-2023-48795).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ssh.h"
#include "cripto.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"

#define VERSION "SSH-2.0-HispaPentium_1.0"

/* ---- escribir y leer mensajes ---- */
void sb_inicia(struct sbuf *b, u8 *d, u32 cabe) { b->d = d, b->n = 0, b->cabe = cabe, b->mal = 0; }

void sb_datos(struct sbuf *b, const void *d, u32 n)
{
	if (b->n + n > b->cabe) {
		b->mal = 1;
		return;
	}
	memcpy(b->d + b->n, d, n);
	b->n += n;
}

void sb_u8(struct sbuf *b, u32 v) { u8 c = v; sb_datos(b, &c, 1); }

void sb_u32(struct sbuf *b, u32 v)
{
	u8 c[4] = { v >> 24, v >> 16, v >> 8, v };

	sb_datos(b, c, 4);
}

void sb_cad(struct sbuf *b, const void *d, u32 n) { sb_u32(b, n); sb_datos(b, d, n); }
void sb_texto(struct sbuf *b, const char *t) { sb_cad(b, t, strlen(t)); }

void sb_mpint(struct sbuf *b, const u8 *d, u32 n)
{
	while (n && !*d)
		d++, n--;
	if (n && (*d & 0x80)) {
		sb_u32(b, n + 1);
		sb_u8(b, 0);
	} else
		sb_u32(b, n);
	sb_datos(b, d, n);
}

void sl_inicia(struct slee *l, const u8 *p, u32 n) { l->p = p, l->n = n, l->i = 0, l->mal = 0; }

u32 sl_u8(struct slee *l)
{
	if (l->i + 1 > l->n) {
		l->mal = 1;
		return 0;
	}
	return l->p[l->i++];
}

u32 sl_u32(struct slee *l)
{
	const u8 *p;

	if (l->i + 4 > l->n) {
		l->mal = 1;
		return 0;
	}
	p = l->p + l->i;
	l->i += 4;
	return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3];
}

const u8 *sl_cad(struct slee *l, u32 *n)
{
	u32 k = sl_u32(l);
	const u8 *p;

	if (l->mal || k > l->n - l->i) {
		l->mal = 1;
		*n = 0;
		return (const u8 *)"";
	}
	p = l->p + l->i;
	l->i += k;
	*n = k;
	return p;
}

int sl_cad_es(const u8 *c, u32 n, const char *t)
{
	return strlen(t) == n && !memcmp(c, t, n);
}

void sl_texto(struct slee *l, char *d, int cabe)
{
	u32 n, i;
	const u8 *c = sl_cad(l, &n);

	for (i = 0; i < n && (int)i < cabe - 1; i++)
		d[i] = c[i] ? c[i] : '?';
	d[i] = 0;
}

/* ---- lo que llega por la red ---- */
static int llena(struct ssh *s, u32 espera)
{
	u32 r;

	if (s->ent_n >= sizeof s->ent)
		return -1;
	r = h_recibir(s->sock, s->ent + s->ent_n, sizeof s->ent - s->ent_n, espera);
	if (r == TIEMPO)
		return 0;
	if (ES_ERROR(r)) {
		formatear(s->error, sizeof s->error, "%s", h_texto_error_red(r));
		return -1;
	}
	if (r == 0) {
		formatear(s->error, sizeof s->error, "la otra punta ha cerrado la conexión");
		return -1;
	}
	s->ent_n += r;
	return 1;
}

static void gasta(struct ssh *s)
{
	if (s->gastar) {
		memmove(s->ent, s->ent + s->gastar, s->ent_n - s->gastar);
		s->ent_n -= s->gastar;
		s->gastar = 0;
	}
}

static void nonce_de(u32 seq, u8 n[8])
{
	n[0] = n[1] = n[2] = n[3] = 0;
	n[4] = seq >> 24, n[5] = seq >> 16, n[6] = seq >> 8, n[7] = seq;
}

/* el paquete siguiente, entero, sin cifrar: 1 (pl, n), 0 (aún no), -1 */
static int paquete(struct ssh *s, const u8 **pl, u32 *n, u32 espera)
{
	u32 largo, total, relleno;
	u8 nonce[8];
	int r;

	gasta(s);
	for (;;) {
		if (s->ent_n >= 4) {
			u8 lb[4];
			if (s->cifrado_ent) {
				nonce_de(s->seq_ent, nonce);
				crypto_chacha20_djb(lb, s->ent, 4, s->k_ent + 32, nonce, 0);
			} else
				memcpy(lb, s->ent, 4);
			largo = (u32)lb[0] << 24 | (u32)lb[1] << 16 | (u32)lb[2] << 8 | lb[3];
			if (largo < 5 || largo > SSH_CABE - 64) {
				formatear(s->error, sizeof s->error, "paquete de largo imposible (%u): ¿se ha estropeado el cifrado?", largo);
				return -1;
			}
			total = 4 + largo + (s->cifrado_ent ? 16 : 0);
			if (s->ent_n >= total) {
				if (s->cifrado_ent) {
					u8 pk[64], marca[16];
					memset(pk, 0, 64);
					crypto_chacha20_djb(pk, pk, 64, s->k_ent, nonce, 0);
					crypto_poly1305(marca, s->ent, 4 + largo, pk);
					crypto_wipe(pk, 64);
					if (crypto_verify16(marca, s->ent + 4 + largo)) {
						formatear(s->error, sizeof s->error, "un paquete no pasa la comprobación (MAC): la conexión no es de fiar");
						return -1;
					}
					crypto_chacha20_djb(s->ent + 4, s->ent + 4, largo, s->k_ent, nonce, 1);
				}
				relleno = s->ent[4];
				if (relleno + 1 > largo) {
					formatear(s->error, sizeof s->error, "paquete mal hecho");
					return -1;
				}
				*pl = s->ent + 5;
				*n = largo - relleno - 1;
				s->gastar = total;
				s->seq_ent++;
				return 1;
			}
		}
		r = llena(s, espera);
		if (r <= 0)
			return r;
		espera = 0;
	}
}

int ssh_envia(struct ssh *s, const u8 *pl, u32 n)
{
	u32 relleno, largo, total;
	u8 nonce[8];

	if (n + 64 > sizeof s->sale)
		return -1;
	/* (con chacha20-poly1305, el largo no cuenta para el relleno) */
	relleno = 8 - ((n + 1 + (s->cifrado_sal ? 0 : 4)) % 8);
	if (relleno < 4)
		relleno += 8;
	largo = 1 + n + relleno;
	s->sale[0] = largo >> 24, s->sale[1] = largo >> 16, s->sale[2] = largo >> 8, s->sale[3] = largo;
	s->sale[4] = relleno;
	memmove(s->sale + 5, pl, n);
	h_azar(s->sale + 5 + n, relleno);
	total = 4 + largo;
	if (s->cifrado_sal) {
		u8 pk[64];
		nonce_de(s->seq_sal, nonce);
		crypto_chacha20_djb(s->sale, s->sale, 4, s->k_sal + 32, nonce, 0);
		crypto_chacha20_djb(s->sale + 4, s->sale + 4, largo, s->k_sal, nonce, 1);
		memset(pk, 0, 64);
		crypto_chacha20_djb(pk, pk, 64, s->k_sal, nonce, 0);
		crypto_poly1305(s->sale + total, s->sale, total, pk);
		crypto_wipe(pk, 64);
		total += 16;
	}
	s->seq_sal++;
	if (ES_ERROR(h_enviar(s->sock, s->sale, total))) {
		formatear(s->error, sizeof s->error, "no se ha podido enviar");
		return -1;
	}
	return 0;
}

void ssh_corta(struct ssh *s, u32 razon, const char *texto)
{
	u8 m[300];
	struct sbuf b;

	sb_inicia(&b, m, sizeof m);
	sb_u8(&b, SSH_DISCONNECT);
	sb_u32(&b, razon);
	sb_texto(&b, texto);
	sb_texto(&b, "");
	ssh_envia(s, m, b.n);
}

/* ---- el intercambio de claves ---- */
static int en_lista(const u8 *l, u32 n, const char *nombre)
{
	u32 i = 0, k = strlen(nombre);

	while (i < n) {
		u32 j = i;
		while (j < n && l[j] != ',')
			j++;
		if (j - i == k && !memcmp(l + i, nombre, k))
			return 1;
		i = j + 1;
	}
	return 0;
}

static void hace_kexinit(struct ssh *s)
{
	struct sbuf b;
	u8 galleta[16];

	sb_inicia(&b, s->kx_mio, sizeof s->kx_mio);
	sb_u8(&b, SSH_KEXINIT);
	h_azar(galleta, 16);
	sb_datos(&b, galleta, 16);
	sb_texto(&b, s->servidor ? "curve25519-sha256,curve25519-sha256@libssh.org,kex-strict-s-v00@openssh.com"
		 : "curve25519-sha256,curve25519-sha256@libssh.org,kex-strict-c-v00@openssh.com");
	sb_texto(&b, "ssh-ed25519");
	sb_texto(&b, "chacha20-poly1305@openssh.com");
	sb_texto(&b, "chacha20-poly1305@openssh.com");
	sb_texto(&b, "hmac-sha2-256");
	sb_texto(&b, "hmac-sha2-256");
	sb_texto(&b, "none");
	sb_texto(&b, "none");
	sb_texto(&b, "");
	sb_texto(&b, "");
	sb_u8(&b, 0);
	sb_u32(&b, 0);
	s->n_mio = b.n;
}

/* mira lo que ofrece la otra punta: 0 si hay algo en común */
static int mira_kexinit(struct ssh *s)
{
	struct slee l;
	const u8 *c;
	u32 n, i;
	static const char *falta[] = { "intercambio de claves (curve25519-sha256)", "llave del equipo (ssh-ed25519)",
		"cifrado (chacha20-poly1305@openssh.com)", "cifrado (chacha20-poly1305@openssh.com)" };

	sl_inicia(&l, s->kx_suyo, s->n_suyo);
	sl_u8(&l);
	l.i += 16;
	for (i = 0; i < 10; i++) {
		c = sl_cad(&l, &n);
		if (l.mal)
			break;
		if (i == 0) {
			if (!en_lista(c, n, "curve25519-sha256") && !en_lista(c, n, "curve25519-sha256@libssh.org"))
				goto no;
			if (!s->hay_sesion)
				s->estricto = en_lista(c, n, s->servidor ? "kex-strict-c-v00@openssh.com" : "kex-strict-s-v00@openssh.com");
		} else if (i == 1) {
			if (!en_lista(c, n, "ssh-ed25519"))
				goto no;
		} else if (i == 2 || i == 3) {
			if (!en_lista(c, n, "chacha20-poly1305@openssh.com"))
				goto no;
		} else if (i == 6 || i == 7) {
			if (!en_lista(c, n, "none"))
				goto no;
		}
		continue;
no:
		formatear(s->error, sizeof s->error, "no hay %s en común con la otra punta", i < 4 ? falta[i] : "compresión «none»");
		return -1;
	}
	if (sl_u8(&l)) {
		/* «first_kex_packet_follows»: lo que adivine, como no hay más que uno, vale */
	}
	if (l.mal) {
		formatear(s->error, sizeof s->error, "KEXINIT mal hecho");
		return -1;
	}
	return 0;
}

/* las claves: HASH(K || H || letra || sesión) y, para llegar a 64, HASH(K || H || lo de antes) */
static void deriva(struct ssh *s, const u8 *kmp, u32 nk, const u8 h[32], char letra, u8 r[64])
{
	sha256_ctx c;

	sha256_inicia(&c);
	sha256_mas(&c, kmp, nk);
	sha256_mas(&c, h, 32);
	sha256_mas(&c, &letra, 1);
	sha256_mas(&c, s->sesion, 32);
	sha256_fin(&c, r);
	sha256_inicia(&c);
	sha256_mas(&c, kmp, nk);
	sha256_mas(&c, h, 32);
	sha256_mas(&c, r, 32);
	sha256_fin(&c, r + 32);
}

void ssh_blob_ed25519(const u8 pub[32], u8 blob[51])
{
	struct sbuf b;

	sb_inicia(&b, blob, 51);
	sb_texto(&b, "ssh-ed25519");
	sb_cad(&b, pub, 32);
}

void ssh_huella(const u8 *blob, u32 n, char *t)
{
	u8 h[32];
	int k;

	sha256(blob, n, h);
	memcpy(t, "SHA256:", 7);
	k = h_base64(h, 32, t + 7, 52);
	while (k > 0 && t[7 + k - 1] == '=')
		k--;
	t[7 + k] = 0;
}

/* H = SHA256(V_C, V_S, I_C, I_S, K_S, Q_C, Q_S, K) */
static void resumen(struct ssh *s, const u8 *ks, u32 nks, const u8 qc[32], const u8 qs[32], const u8 *kmp, u32 nk, u8 h[32])
{
	static u8 t[4096];
	struct sbuf b;

	sb_inicia(&b, t, sizeof t);
	sb_texto(&b, s->v_cli);
	sb_texto(&b, s->v_srv);
	if (s->servidor) {
		sb_cad(&b, s->kx_suyo, s->n_suyo);
		sb_cad(&b, s->kx_mio, s->n_mio);
	} else {
		sb_cad(&b, s->kx_mio, s->n_mio);
		sb_cad(&b, s->kx_suyo, s->n_suyo);
	}
	sb_cad(&b, ks, nks);
	sb_cad(&b, qc, 32);
	sb_cad(&b, qs, 32);
	sb_datos(&b, kmp, nk);
	sha256(t, b.n, h);
}

/* espera un mensaje del intercambio (los IGNORE y DEBUG no cuentan) */
static int espera_kex(struct ssh *s, const u8 **pl, u32 *n)
{
	int r;

	for (;;) {
		u32 ahora = h_tics();
		if ((int)(s->limite - ahora) <= 0) {
			formatear(s->error, sizeof s->error, "el intercambio de claves no acaba a tiempo");
			return -1;
		}
		r = paquete(s, pl, n, 50);
		if (r < 0)
			return -1;
		if (r == 0)
			continue;
		if (**pl == SSH_DISCONNECT) {
			struct slee l;
			char t[120];
			sl_inicia(&l, *pl + 1, *n - 1);
			sl_u32(&l);
			sl_texto(&l, t, sizeof t);
			formatear(s->error, sizeof s->error, "la otra punta corta: %s", t);
			return -1;
		}
		if (!s->estricto || s->hay_sesion) {
			if (**pl == SSH_IGNORE || **pl == SSH_DEBUG)
				continue;
		}
		return 0;
	}
}

static int intercambio(struct ssh *s)
{
	u8 xs[32], xp[32], suya[32], k[32], kmp[40], h[32], m[400];
	u32 nk, n;
	const u8 *pl;
	struct sbuf b, km;
	struct slee l;

	if (mira_kexinit(s) < 0)
		return -1;
	if (s->estricto && !s->hay_sesion)
		h_apunta("kex-strict: sí (contra Terrapin)");
	h_azar(xs, 32);
	crypto_x25519_public_key(xp, xs);
	if (s->servidor) {
		const u8 *qc, *sig_b;
		u8 blob[51], firma[64];
		u32 nq;
		if (espera_kex(s, &pl, &n) < 0)
			return -1;
		if (*pl != SSH_KEX_ECDH_INIT) {
			formatear(s->error, sizeof s->error, "se esperaba KEX_ECDH_INIT y llega el %u", *pl);
			return -1;
		}
		sl_inicia(&l, pl + 1, n - 1);
		qc = sl_cad(&l, &nq);
		if (l.mal || nq != 32) {
			formatear(s->error, sizeof s->error, "la clave de la otra punta no vale");
			return -1;
		}
		memcpy(suya, qc, 32);
		crypto_x25519(k, xs, suya);
		sb_inicia(&km, kmp, sizeof kmp);
		sb_mpint(&km, k, 32);
		nk = km.n;
		ssh_blob_ed25519(s->host_publica, blob);
		resumen(s, blob, 51, suya, xp, kmp, nk, h);
		crypto_ed25519_sign(firma, s->host_secreta, h, 32);
		sb_inicia(&b, m, sizeof m);
		sb_u8(&b, SSH_KEX_ECDH_REPLY);
		sb_cad(&b, blob, 51);
		sb_cad(&b, xp, 32);
		sb_u32(&b, 4 + 11 + 4 + 64);
		sb_texto(&b, "ssh-ed25519");
		sb_cad(&b, firma, 64);
		(void)sig_b;
		if (ssh_envia(s, m, b.n) < 0)
			return -1;
	} else {
		const u8 *ks, *qs, *sig, *alg, *fir;
		u32 nks, nq, ns, na, nf;
		struct slee f, kl;
		u8 pub[32];
		sb_inicia(&b, m, sizeof m);
		sb_u8(&b, SSH_KEX_ECDH_INIT);
		sb_cad(&b, xp, 32);
		if (ssh_envia(s, m, b.n) < 0)
			return -1;
		if (espera_kex(s, &pl, &n) < 0)
			return -1;
		if (*pl != SSH_KEX_ECDH_REPLY) {
			formatear(s->error, sizeof s->error, "se esperaba KEX_ECDH_REPLY y llega el %u", *pl);
			return -1;
		}
		sl_inicia(&l, pl + 1, n - 1);
		ks = sl_cad(&l, &nks);
		qs = sl_cad(&l, &nq);
		sig = sl_cad(&l, &ns);
		if (l.mal || nq != 32) {
			formatear(s->error, sizeof s->error, "KEX_ECDH_REPLY mal hecho");
			return -1;
		}
		/* la llave del servidor: ssh-ed25519 */
		sl_inicia(&kl, ks, nks);
		alg = sl_cad(&kl, &na);
		fir = sl_cad(&kl, &nf);
		if (kl.mal || !sl_cad_es(alg, na, "ssh-ed25519") || nf != 32) {
			formatear(s->error, sizeof s->error, "la llave del servidor no es ssh-ed25519");
			return -1;
		}
		memcpy(pub, fir, 32);
		memcpy(suya, qs, 32);
		crypto_x25519(k, xs, suya);
		sb_inicia(&km, kmp, sizeof kmp);
		sb_mpint(&km, k, 32);
		nk = km.n;
		resumen(s, ks, nks, xp, suya, kmp, nk, h);
		sl_inicia(&f, sig, ns);
		alg = sl_cad(&f, &na);
		fir = sl_cad(&f, &nf);
		if (f.mal || !sl_cad_es(alg, na, "ssh-ed25519") || nf != 64 || crypto_ed25519_check(fir, pub, h, 32)) {
			formatear(s->error, sizeof s->error, "la firma del servidor no vale: alguien puede estar en medio");
			return -1;
		}
		if (!s->hay_sesion) {
			if (ssh_comprueba_servidor(s, ks, nks)) {
				if (!s->error[0])
					formatear(s->error, sizeof s->error, "no se sigue: la llave del servidor no es de confianza");
				return -1;
			}
		}
	}
	{
		int cero = 1, i;
		for (i = 0; i < 32; i++)
			if (k[i])
				cero = 0;
		if (cero) {
			formatear(s->error, sizeof s->error, "la clave compartida sale a cero");
			return -1;
		}
	}
	if (!s->hay_sesion) {
		memcpy(s->sesion, h, 32);
		s->hay_sesion = 1;
	}
	/* NEWKEYS, en los dos sentidos */
	m[0] = SSH_NEWKEYS;
	if (ssh_envia(s, m, 1) < 0)
		return -1;
	deriva(s, kmp, nk, h, s->servidor ? 'D' : 'C', s->k_sal);
	s->cifrado_sal = 1;
	if (s->estricto)
		s->seq_sal = 0;
	if (espera_kex(s, &pl, &n) < 0)
		return -1;
	if (*pl != SSH_NEWKEYS) {
		formatear(s->error, sizeof s->error, "se esperaba NEWKEYS y llega el %u", *pl);
		return -1;
	}
	deriva(s, kmp, nk, h, s->servidor ? 'C' : 'D', s->k_ent);
	s->cifrado_ent = 1;
	if (s->estricto)
		s->seq_ent = 0;
	crypto_wipe(xs, 32);
	crypto_wipe(k, 32);
	crypto_wipe(kmp, sizeof kmp);
	return 0;
}

void ssh_prepara(struct ssh *s, int servidor, u32 sock)
{
	u8 hs[64], hp[32];

	memcpy(hs, s->host_secreta, 64);
	memcpy(hp, s->host_publica, 32);
	memset(s, 0, sizeof *s);
	memcpy(s->host_secreta, hs, 64);
	memcpy(s->host_publica, hp, 32);
	crypto_wipe(hs, 64);
	s->servidor = servidor;
	s->sock = sock;
	s->error[0] = 0;
}

/* la línea de la versión de la otra punta (antes, puede haber otras líneas) */
static int version_suya(struct ssh *s, char *v)
{
	u32 i;
	int r;

	for (;;) {
		for (i = 0; i < s->ent_n; i++) {
			if (s->ent[i] == '\n') {
				u32 k = i;
				if (k > 0 && s->ent[k - 1] == '\r')
					k--;
				if (k >= 4 && !memcmp(s->ent, "SSH-", 4)) {
					if (k > 250)
						k = 250;
					memcpy(v, s->ent, k);
					v[k] = 0;
					s->gastar = i + 1;
					gasta(s);
					if (memcmp(v, "SSH-2.0-", 8) && memcmp(v, "SSH-1.99-", 9)) {
						formatear(s->error, sizeof s->error, "la otra punta no habla SSH-2 (%s)", v);
						return -1;
					}
					return 0;
				}
				/* (una línea de antes: fuera) */
				s->gastar = i + 1;
				gasta(s);
				i = (u32)-1;
				continue;
			}
		}
		if (s->ent_n > 1024) {
			formatear(s->error, sizeof s->error, "la otra punta no dice su versión de SSH");
			return -1;
		}
		if ((int)(s->limite - h_tics()) <= 0) {
			formatear(s->error, sizeof s->error, "la otra punta no dice su versión de SSH a tiempo");
			return -1;
		}
		r = llena(s, 50);
		if (r < 0)
			return -1;
	}
}

int ssh_empieza(struct ssh *s)
{
	const u8 *pl;
	u32 n;
	char *mia = s->servidor ? s->v_srv : s->v_cli, *suya = s->servidor ? s->v_cli : s->v_srv;
	char l[64];

	s->limite = h_tics() + 60 * h_hz;
	h_copia(mia, VERSION, 64);
	formatear(l, sizeof l, "%s\r\n", mia);
	if (ES_ERROR(h_enviar(s->sock, l, strlen(l)))) {
		formatear(s->error, sizeof s->error, "no se ha podido enviar");
		return -1;
	}
	if (version_suya(s, suya) < 0)
		return -1;
	h_apunta("la otra punta: %s", suya);
	hace_kexinit(s);
	if (ssh_envia(s, s->kx_mio, s->n_mio) < 0)
		return -1;
	if (espera_kex(s, &pl, &n) < 0)
		return -1;
	if (*pl != SSH_KEXINIT || n > sizeof s->kx_suyo) {
		formatear(s->error, sizeof s->error, "se esperaba KEXINIT y llega el %u", *pl);
		return -1;
	}
	memcpy(s->kx_suyo, pl, n);
	s->n_suyo = n;
	if (intercambio(s) < 0)
		return -1;
	h_apunta("cifrado: curve25519-sha256, ssh-ed25519, chacha20-poly1305@openssh.com");
	return 0;
}

int ssh_lee(struct ssh *s, const u8 **pl, u32 *n, u32 espera)
{
	int r;

	for (;;) {
		r = paquete(s, pl, n, espera);
		if (r <= 0)
			return r;
		switch (**pl) {
		case SSH_IGNORE:
		case SSH_DEBUG:
		case SSH_UNIMPLEMENTED:
		case SSH_EXT_INFO:
			espera = 0;
			continue;
		case SSH_DISCONNECT: {
			struct slee l;
			char t[120];
			sl_inicia(&l, *pl + 1, *n - 1);
			sl_u32(&l);
			sl_texto(&l, t, sizeof t);
			formatear(s->error, sizeof s->error, "la otra punta corta: %s", t);
			return -1;
		}
		case SSH_KEXINIT:
			/* un intercambio de claves nuevo (lo pide la otra punta) */
			if (*n > sizeof s->kx_suyo)
				return -1;
			memcpy(s->kx_suyo, *pl, *n);
			s->n_suyo = *n;
			hace_kexinit(s);
			if (ssh_envia(s, s->kx_mio, s->n_mio) < 0)
				return -1;
			s->limite = h_tics() + 60 * h_hz;
			if (intercambio(s) < 0)
				return -1;
			h_apunta("claves nuevas");
			espera = 0;
			continue;
		}
		return 1;
	}
}

u32 ssh_lo_firmado(struct ssh *s, u8 *d, u32 cabe, const char *usuario, const u8 *alg, u32 nalg, const u8 *blob, u32 nb)
{
	struct sbuf b;

	sb_inicia(&b, d, cabe);
	sb_cad(&b, s->sesion, 32);
	sb_u8(&b, SSH_USERAUTH_REQUEST);
	sb_texto(&b, usuario);
	sb_texto(&b, "ssh-connection");
	sb_texto(&b, "publickey");
	sb_u8(&b, 1);
	sb_cad(&b, alg, nalg);
	sb_cad(&b, blob, nb);
	return b.mal ? 0 : b.n;
}

/* (el servidor no comprueba llaves de servidores: solo el cliente da la suya) */
__attribute__((weak)) int ssh_comprueba_servidor(struct ssh *s, const u8 *blob, u32 n)
{
	(void)s, (void)blob, (void)n;
	return -1;
}
