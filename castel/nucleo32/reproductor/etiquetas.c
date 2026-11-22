/* etiquetas.c - el Reproductor: las etiquetas de las canciones (ver
 * etiquetas.h). Todo lo que llega de un archivo se mira antes de tocarlo:
 * los largos, los finales de los textos, las codificaciones.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "etiquetas.h"
#include "util.h"

#define CABEZA (256u * 1024)	/* lo que se lee del principio (sin la carátula) */

static const char *generos[] = { "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop",
	"Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial",
	"Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal",
	"Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel",
	"Noise", "AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock",
	"Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream",
	"Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
	"Native American", "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal",
	"Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock" };

const char *genero_id3(int n)
{
	return n >= 0 && n < (int)(sizeof generos / sizeof generos[0]) ? generos[n] : "";
}

static void pon(char *d, int cabe, const char *o)
{
	/* (sin espacios ni saltos a los lados) */
	int n;

	while (*o == ' ' || *o == '\r' || *o == '\n' || *o == '\t')
		o++;
	copia(d, cabe, o);
	n = (int)strlen(d);
	while (n > 0 && (d[n - 1] == ' ' || d[n - 1] == '\r' || d[n - 1] == '\n' || d[n - 1] == '\t'))
		d[--n] = 0;
	for (n = 0; d[n]; n++)
		if ((u8)d[n] < 32)
			d[n] = ' ';
}

/* «-6.52 dB» en centésimas */
static int centesimas(const char *t)
{
	int v = 0, neg = 0, dec = 0, k = 0;

	while (*t == ' ')
		t++;
	if (*t == '-' || *t == '+')
		neg = *t++ == '-';
	while (*t >= '0' && *t <= '9' && v < 100000)
		v = v * 10 + (*t++ - '0');
	if (*t == '.' || *t == ',') {
		t++;
		while (*t >= '0' && *t <= '9') {
			if (k < 2)
				dec = dec * 10 + (*t - '0'), k++;
			t++;
		}
	}
	while (k++ < 2)
		dec *= 10;
	v = v * 100 + dec;
	return neg ? -v : v;
}

static int milesimas(const char *t)
{
	return centesimas(t) * 10;
}

/* una clave y su valor (de Vorbis, de TXXX o del INFO) */
static void clave(etiquetas *e, const char *k, const char *v)
{
	if (igual_sin_caja(k, "TITLE"))
		pon(e->titulo, sizeof e->titulo, v);
	else if (igual_sin_caja(k, "ARTIST"))
		pon(e->artista, sizeof e->artista, v);
	else if (igual_sin_caja(k, "ALBUM"))
		pon(e->album, sizeof e->album, v);
	else if (igual_sin_caja(k, "DATE") || igual_sin_caja(k, "YEAR"))
		pon(e->anyo, sizeof e->anyo, v);
	else if (igual_sin_caja(k, "TRACKNUMBER"))
		pon(e->pista, sizeof e->pista, v);
	else if (igual_sin_caja(k, "GENRE"))
		pon(e->genero, sizeof e->genero, v);
	else if ((igual_sin_caja(k, "COMMENT") || igual_sin_caja(k, "DESCRIPTION")) && !e->comentario[0])
		pon(e->comentario, sizeof e->comentario, v);
	else if (igual_sin_caja(k, "REPLAYGAIN_TRACK_GAIN")) {
		e->rg_pista = centesimas(v);
		e->hay_rg_pista = 1;
	} else if (igual_sin_caja(k, "REPLAYGAIN_ALBUM_GAIN")) {
		e->rg_album = centesimas(v);
		e->hay_rg_album = 1;
	} else if (igual_sin_caja(k, "REPLAYGAIN_TRACK_PEAK"))
		e->rg_pista_pico = milesimas(v);
	else if (igual_sin_caja(k, "REPLAYGAIN_ALBUM_PEAK"))
		e->rg_album_pico = milesimas(v);
}

static void pon_caratula(etiquetas *e, const u8 *d, u32 n)
{
	if (e->caratula || n < 8 || n > MAX_CARATULA)
		return;
	e->caratula = so_pide(n);
	if (!e->caratula)
		return;
	memcpy(e->caratula, d, n);
	e->caratula_largo = n;
}

/* ======================================================= base64 */
static int b64(int c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

/* de base64 a d (en su sitio vale): los octetos */
static u32 de_base64(const u8 *t, u32 n, u8 *d)
{
	u32 i, k = 0, v = 0;
	int bits = 0;

	for (i = 0; i < n; i++) {
		int c = b64(t[i]);

		if (c < 0)
			continue;
		v = v << 6 | (u32)c;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			d[k++] = (u8)(v >> bits);
		}
	}
	return k;
}

/* ======================================================= el bloque PICTURE de FLAC */
static void bloque_imagen(const u8 *d, u32 n, etiquetas *e)
{
	u32 p = 4, l;

	if (n < 32)
		return;
	l = be32(d + p);
	if (l > n - p - 4)
		return;
	p += 4 + l;
	if (p + 4 > n)
		return;
	l = be32(d + p);
	if (l > n - p - 4)
		return;
	p += 4 + l;
	if (p + 20 > n)
		return;
	p += 16;
	l = be32(d + p);
	p += 4;
	if (l > n - p)
		return;
	/* (la portada, la 3, o la que haya) */
	if (be32(d) == 3 && e->caratula) {
		so_suelta(e->caratula);
		e->caratula = NULL;
	}
	pon_caratula(e, d + p, l);
}

/* ======================================================= Vorbis */
void etiquetas_vorbis(const u8 *d, u32 n, etiquetas *e, int caratula)
{
	u32 p = 0, l, k, cuantos;
	char t[512];

	if (n < 8)
		return;
	l = le32(d);
	if (l > n - 4)
		return;
	p = 4 + l;
	if (p + 4 > n)
		return;
	cuantos = le32(d + p);
	p += 4;
	for (k = 0; k < cuantos && p + 4 <= n; k++) {
		const u8 *c;
		u32 i;

		l = le32(d + p);
		p += 4;
		if (l > n - p)
			return;
		c = d + p;
		p += l;
		for (i = 0; i < l && c[i] != '='; i++)
			;
		if (i >= l || i == 0 || i > 40)
			continue;
		if (caratula && i == 22 && !memcmp(c, "METADATA_BLOCK_PICTURE", 22)) {
			u8 *b = so_pide(l);

			if (b) {
				u32 m = de_base64(c + 23, l - 23, b);

				bloque_imagen(b, m, e);
				so_suelta(b);
			}
			continue;
		}
		if (caratula && i == 8 && !memcmp(c, "COVERART", 8)) {
			u8 *b = so_pide(l);

			if (b) {
				u32 m = de_base64(c + 9, l - 9, b);

				pon_caratula(e, b, m);
				so_suelta(b);
			}
			continue;
		}
		{
			char k2[48];
			u32 m = l - i - 1;

			memcpy(k2, c, i);
			k2[i] = 0;
			if (m > sizeof t - 1)
				m = sizeof t - 1;
			memcpy(t, c + i + 1, m);
			t[m] = 0;
			clave(e, k2, t);
		}
	}
}

/* ======================================================= FLAC */
void etiquetas_flac(const u8 *d, u32 n, etiquetas *e, int caratula)
{
	u32 p = 4;

	if (n < 8 || memcmp(d, "fLaC", 4))
		return;
	while (p + 4 <= n) {
		int ultimo = d[p] & 0x80, tipo = d[p] & 0x7F;
		u32 l = be24(d + p + 1);

		p += 4;
		if (l > n - p)
			l = n - p;	/* (cortado: lo que haya) */
		if (tipo == 4)
			etiquetas_vorbis(d + p, l, e, caratula);
		else if (tipo == 6 && caratula)
			bloque_imagen(d + p, l, e);
		p += l;
		if (ultimo)
			break;
	}
}

/* ======================================================= OGG: el segundo paquete */
void etiquetas_ogg(const u8 *d, u32 n, etiquetas *e, int caratula)
{
	u32 p = 0, paquete = 0, en = 0, cabe = 0;
	u8 *b = NULL;
	u32 serie = 0;
	int primera = 1;

	while (p + 27 <= n && paquete < 2) {
		u32 segs, i, datos, s;

		if (memcmp(d + p, "OggS", 4))
			break;
		if (primera) {
			serie = le32(d + p + 14);
			primera = 0;
		} else if (le32(d + p + 14) != serie) {
			/* (otro flujo intercalado: se salta la página) */
		}
		segs = d[p + 26];
		if (p + 27 + segs > n)
			break;
		datos = p + 27 + segs;
		for (i = 0; i < segs; i++) {
			s = d[p + 27 + i];
			if (datos + s > n)
				goto fin;
			if (le32(d + p + 14) == serie && paquete == 1) {
				if (en + s > cabe) {
					u8 *nb;

					cabe = (en + s) * 2 + 4096;
					if (cabe > MAX_CARATULA * 2)
						goto fin;
					nb = so_repide(b, cabe);
					if (!nb)
						goto fin;
					b = nb;
				}
				memcpy(b + en, d + datos, s);
				en += s;
			}
			datos += s;
			if (s < 255 && le32(d + p + 14) == serie)
				paquete++;
		}
		p = datos;
	}
fin:
	if (b && paquete >= 2) {
		if (en > 7 && !memcmp(b, "\x03vorbis", 7))
			etiquetas_vorbis(b + 7, en - 7, e, caratula);
		else if (en > 8 && !memcmp(b, "OpusTags", 8))
			etiquetas_vorbis(b + 8, en - 8, e, caratula);
		else if (en > 4 && (b[0] & 0x7F) == 4)
			etiquetas_vorbis(b + 4, en - 4, e, caratula);	/* (OGG FLAC) */
	}
	so_suelta(b);
}

/* ======================================================= ID3v1 */
void etiquetas_id3v1(const u8 *d, u32 n, etiquetas *e)
{
	char t[64];

	if (n < 128 || memcmp(d, "TAG", 3))
		return;
#define CAMPO(o, l, dest) do { latin1_a_utf8(d + (o), (l), t, sizeof t); if (!(dest)[0]) pon((dest), sizeof(dest), t); } while (0)
	CAMPO(3, 30, e->titulo);
	CAMPO(33, 30, e->artista);
	CAMPO(63, 30, e->album);
	CAMPO(93, 4, e->anyo);
	if (d[125] == 0 && d[126] && !e->pista[0])
		fmt(e->pista, sizeof e->pista, "%d", d[126]);
	CAMPO(97, d[125] == 0 ? 28 : 30, e->comentario);
	if (!e->genero[0])
		copia(e->genero, sizeof e->genero, genero_id3(d[127]));
#undef CAMPO
}

/* ======================================================= ID3v2 */
static void texto_id3(const u8 *d, u32 n, int cod, char *t, int cabe)
{
	if (cabe <= 0)
		return;
	t[0] = 0;
	if (cod == 0)
		latin1_a_utf8(d, (int)n, t, cabe);
	else if (cod == 1) {
		int le = 1;

		if (n >= 2 && d[0] == 0xFE && d[1] == 0xFF)
			le = 0;
		utf16_a_utf8(d, (int)n, le, t, cabe);
	} else if (cod == 2)
		utf16_a_utf8(d, (int)n, 0, t, cabe);
	else {
		u32 m = n < (u32)cabe - 1 ? n : (u32)cabe - 1, i;

		for (i = 0; i < m && d[i]; i++)
			t[i] = (char)d[i];
		t[i] = 0;
		copia(t, cabe, t);
	}
}

/* dónde acaba un texto de esa codificación (el 0, o el 00 de UTF-16) */
static u32 fin_texto(const u8 *d, u32 n, int cod)
{
	u32 i;

	if (cod == 1 || cod == 2) {
		for (i = 0; i + 1 < n; i += 2)
			if (!d[i] && !d[i + 1])
				return i;
		return n;
	}
	for (i = 0; i < n; i++)
		if (!d[i])
			return i;
	return n;
}

static u32 salto_fin(u32 f, u32 n, int cod)
{
	u32 s = f + ((cod == 1 || cod == 2) ? 2 : 1);

	return s > n ? n : s;
}

/* «(17)», «17», «(17)Rock»: el nombre */
static void genero(const char *t, char *d, int cabe)
{
	if (t[0] == '(' && t[1] >= '0' && t[1] <= '9') {
		const char *p = t + 1;
		int v = a_entero(p);

		while (*p >= '0' && *p <= '9')
			p++;
		if (*p == ')' && p[1])
			copia(d, cabe, p + 1);
		else
			copia(d, cabe, genero_id3(v));
		return;
	}
	if (t[0] >= '0' && t[0] <= '9') {
		const char *p = t;

		while (*p >= '0' && *p <= '9')
			p++;
		if (!*p) {
			copia(d, cabe, genero_id3(a_entero(t)));
			return;
		}
	}
	copia(d, cabe, t);
}

static void marco_id3(const char *id, const u8 *d, u32 n, etiquetas *e, int caratula, int v22)
{
	char t[512];
	int cod;
	char *dest = NULL;
	int cabe = 0;

	if (n < 1)
		return;
	cod = d[0];
	if (cod > 3)
		return;
	if (!strcmp(id, v22 ? "TT2" : "TIT2"))
		dest = e->titulo, cabe = sizeof e->titulo;
	else if (!strcmp(id, v22 ? "TP1" : "TPE1"))
		dest = e->artista, cabe = sizeof e->artista;
	else if (!strcmp(id, v22 ? "TAL" : "TALB"))
		dest = e->album, cabe = sizeof e->album;
	else if (!strcmp(id, v22 ? "TYE" : "TYER") || !strcmp(id, "TDRC"))
		dest = e->anyo, cabe = sizeof e->anyo;
	else if (!strcmp(id, v22 ? "TRK" : "TRCK"))
		dest = e->pista, cabe = sizeof e->pista;
	if (dest) {
		texto_id3(d + 1, n - 1, cod, t, sizeof t);
		pon(dest, cabe, t);
		return;
	}
	if (!strcmp(id, v22 ? "TCO" : "TCON")) {
		char g[64];

		texto_id3(d + 1, n - 1, cod, t, sizeof t);
		genero(t, g, sizeof g);
		pon(e->genero, sizeof e->genero, g);
		return;
	}
	if (!strcmp(id, v22 ? "COM" : "COMM")) {
		u32 f;

		if (n < 5 || e->comentario[0])
			return;
		f = fin_texto(d + 4, n - 4, cod);
		f = salto_fin(f, n - 4, cod);
		texto_id3(d + 4 + f, n - 4 - f, cod, t, sizeof t);
		pon(e->comentario, sizeof e->comentario, t);
		return;
	}
	if (!strcmp(id, v22 ? "TXX" : "TXXX")) {
		char k[64];
		u32 f = fin_texto(d + 1, n - 1, cod), s;

		texto_id3(d + 1, f, cod, k, sizeof k);
		s = salto_fin(f, n - 1, cod);
		texto_id3(d + 1 + s, n - 1 - s, cod, t, sizeof t);
		clave(e, k, t);
		return;
	}
	if (caratula && !strcmp(id, v22 ? "PIC" : "APIC")) {
		u32 p = 1, f;
		int tipo;

		if (v22)
			p += 3;
		else {
			while (p < n && d[p])
				p++;
			p++;
		}
		if (p >= n)
			return;
		tipo = d[p++];
		if (p >= n)
			return;
		f = fin_texto(d + p, n - p, cod);
		p += salto_fin(f, n - p, cod);
		if (p >= n)
			return;
		if (tipo == 3 && e->caratula) {
			so_suelta(e->caratula);
			e->caratula = NULL;
		}
		pon_caratula(e, d + p, n - p);
	}
}

/* quita la «desincronización» (FF 00 -> FF), en su sitio: el largo nuevo */
static u32 desincroniza(u8 *d, u32 n)
{
	u32 i, k = 0;

	for (i = 0; i < n; i++) {
		d[k++] = d[i];
		if (d[i] == 0xFF && i + 1 < n && d[i + 1] == 0)
			i++;
	}
	return k;
}

void etiquetas_id3v2(const u8 *d0, u32 n, etiquetas *e, int caratula)
{
	u32 tam, p = 10, fin;
	int v, flags;
	u8 *d;

	if (n < 10 || memcmp(d0, "ID3", 3) || d0[3] < 2 || d0[3] > 4 || d0[4] == 0xFF)
		return;
	v = d0[3];
	flags = d0[5];
	if ((d0[6] | d0[7] | d0[8] | d0[9]) & 0x80)
		return;
	tam = (u32)d0[6] << 21 | (u32)d0[7] << 14 | (u32)d0[8] << 7 | d0[9];
	if (tam > n - 10)
		tam = n - 10;
	/* una copia (para la desincronización, que se hace en su sitio) */
	d = so_pide(tam + 10 + 16);
	if (!d)
		return;
	memcpy(d, d0, tam + 10);
	memset(d + tam + 10, 0, 16);
	fin = tam + 10;
	if ((flags & 0x80) && v < 4)
		fin = 10 + desincroniza(d + 10, tam);
	if ((flags & 0x40) && v >= 3) {
		/* la cabecera extendida */
		u32 x;

		if (p + 4 > fin)
			goto fuera;
		x = v == 3 ? be32(d + p) + 4 : ((u32)d[p] << 21 | (u32)d[p + 1] << 14 | (u32)d[p + 2] << 7 | d[p + 3]);
		if (x > fin - p)
			goto fuera;
		p += x;
	}
	while (p < fin) {
		char id[5];
		u32 l;
		int fl = 0;
		u8 *c;

		if (v == 2) {
			if (p + 6 > fin || !d[p])
				break;
			memcpy(id, d + p, 3);
			id[3] = 0;
			l = be24(d + p + 3);
			p += 6;
		} else {
			if (p + 10 > fin || !d[p])
				break;
			memcpy(id, d + p, 4);
			id[4] = 0;
			l = v == 4 ? ((u32)(d[p + 4] & 127) << 21 | (u32)(d[p + 5] & 127) << 14 | (u32)(d[p + 6] & 127) << 7 | (d[p + 7] & 127))
				   : be32(d + p + 4);
			fl = (int)be16(d + p + 8);
			p += 10;
		}
		if (l > fin - p)
			break;
		c = d + p;
		p += l;
		if (v == 3 && (fl & 0xC0))
			continue;	/* comprimido o cifrado */
		if (v == 4) {
			if (fl & 0x0C)
				continue;	/* comprimido o cifrado */
			if (fl & 0x01) {
				/* el largo de los datos delante */
				if (l < 4)
					continue;
				c += 4;
				l -= 4;
			}
			if ((fl & 0x02) || (flags & 0x80))
				l = desincroniza(c, l);
		}
		marco_id3(id, c, l, e, caratula, v == 2);
	}
fuera:
	so_suelta(d);
}

/* ======================================================= WAV: LIST INFO y «id3 » */
void etiquetas_wav(const u8 *d, u32 n, etiquetas *e, int caratula)
{
	u32 p = 12;

	if (n < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4))
		return;
	while (p + 8 <= n) {
		u32 l = le32(d + p + 4);

		if (l > n - p - 8)
			l = n - p - 8;
		if (!memcmp(d + p, "LIST", 4) && l >= 4 && !memcmp(d + p + 8, "INFO", 4)) {
			u32 q = p + 12, fin = p + 8 + l;

			while (q + 8 <= fin) {
				u32 m = le32(d + q + 4);
				char t[256];

				if (m > fin - q - 8)
					break;
				latin1_a_utf8(d + q + 8, (int)(m < 255 ? m : 255), t, sizeof t);
				if (!es_utf8(d + q + 8, (int)m))
					;
				else {
					u32 k = m < 255 ? m : 255;

					memcpy(t, d + q + 8, k);
					t[k] = 0;
					copia(t, sizeof t, t);
				}
				if (!memcmp(d + q, "INAM", 4)) pon(e->titulo, sizeof e->titulo, t);
				else if (!memcmp(d + q, "IART", 4)) pon(e->artista, sizeof e->artista, t);
				else if (!memcmp(d + q, "IPRD", 4)) pon(e->album, sizeof e->album, t);
				else if (!memcmp(d + q, "ICRD", 4)) pon(e->anyo, sizeof e->anyo, t);
				else if (!memcmp(d + q, "IGNR", 4)) pon(e->genero, sizeof e->genero, t);
				else if (!memcmp(d + q, "ICMT", 4)) pon(e->comentario, sizeof e->comentario, t);
				else if (!memcmp(d + q, "ITRK", 4)) pon(e->pista, sizeof e->pista, t);
				q += 8 + m + (m & 1);
			}
		} else if ((!memcmp(d + p, "id3 ", 4) || !memcmp(d + p, "ID3 ", 4)) && l >= 10)
			etiquetas_id3v2(d + p + 8, l, e, caratula);
		p += 8 + l + (l & 1);
	}
}

void etiquetas_suelta(etiquetas *e)
{
	so_suelta(e->caratula);
	e->caratula = NULL;
	e->caratula_largo = 0;
}

/* ======================================================= de un archivo */
int etiquetas_lee(const char *ruta, etiquetas *e, int caratula)
{
	so_archivo *a = so_abre(ruta);
	u8 *b;
	u32 n, quiero = CABEZA;
	int k;
	u64 largo;

	memset(e, 0, sizeof *e);
	if (!a)
		return -1;
	largo = so_largo(a);
	b = so_pide(CABEZA + 16);
	if (!b) {
		so_cierra(a);
		return -1;
	}
	k = so_lee(a, b, 10);
	n = k > 0 ? (u32)k : 0;
	/* un ID3v2 grande (con su carátula): entero, hasta lo que dejamos */
	if (n == 10 && !memcmp(b, "ID3", 3)) {
		u32 t = ((u32)(b[6] & 127) << 21 | (u32)(b[7] & 127) << 14 | (u32)(b[8] & 127) << 7 | (b[9] & 127)) + 10;

		if (caratula && t > CABEZA) {
			if (t > MAX_CARATULA + CABEZA)
				t = MAX_CARATULA + CABEZA;
			so_suelta(b);
			b = so_pide(t + 16);
			if (!b) {
				so_cierra(a);
				return -1;
			}
			so_salta_a(a, 0);
			n = 0;
			quiero = t;
		}
	} else if (caratula && n >= 4 && (!memcmp(b, "fLaC", 4) || !memcmp(b, "OggS", 4) || !memcmp(b, "RIFF", 4))) {
		/* (la imagen de un FLAC o un OGG puede ir lejos: hasta 4 MB) */
		u32 t = largo > 4u * 1024 * 1024 ? 4u * 1024 * 1024 : (u32)largo;

		if (t > CABEZA) {
			so_suelta(b);
			b = so_pide(t + 16);
			if (!b) {
				so_cierra(a);
				return -1;
			}
			so_salta_a(a, 0);
			n = 0;
			quiero = t;
		}
	}
	while (n < quiero) {
		k = so_lee(a, b + n, quiero - n > 65536 ? 65536 : quiero - n);
		if (k <= 0)
			break;
		n += (u32)k;
	}
	if (n >= 10 && !memcmp(b, "ID3", 3)) {
		u32 t = ((u32)(b[6] & 127) << 21 | (u32)(b[7] & 127) << 14 | (u32)(b[8] & 127) << 7 | (b[9] & 127)) + 10;

		etiquetas_id3v2(b, n, e, caratula);
		if (t < n && n - t >= 4 && !memcmp(b + t, "fLaC", 4))
			etiquetas_flac(b + t, n - t, e, caratula);
	} else if (n >= 4 && !memcmp(b, "fLaC", 4))
		etiquetas_flac(b, n, e, caratula);
	else if (n >= 4 && !memcmp(b, "OggS", 4))
		etiquetas_ogg(b, n, e, caratula);
	else if (n >= 12 && !memcmp(b, "RIFF", 4))
		etiquetas_wav(b, n, e, caratula);
	/* el ID3v1 del final (lo que falte) */
	if (largo >= 128 && so_salta_a(a, largo - 128) == 0 && so_lee(a, b, 128) == 128)
		etiquetas_id3v1(b, 128, e);
	so_suelta(b);
	so_cierra(a);
	return 0;
}
