/* zip.c - el motor de los ZIP de Comprimidos (zip.h): leer, sacar, probar
 * y hacer. Deflate e inflate, de miniz (terceros/miniz, MIT: tdefl y tinfl);
 * lo demás (el formato de APPNOTE 6.3.10, ZIP64, los nombres en UTF-8 y en
 * la página 437, ZipCrypto, el AES de WinZip y las comprobaciones de
 * seguridad), aquí.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "zip.h"
#include "cifra.h"

/* (miniz, solo con deflate e inflate: MINIZ_NO_STDIO, MINIZ_NO_TIME,
 * MINIZ_NO_ARCHIVE_APIS, MINIZ_NO_ZLIB_APIS y MINIZ_NO_ZLIB_COMPATIBLE_NAMES,
 * en la línea del compilador: hacer.sh y prueba/prueba.sh) */
#include "miniz.h"

zlimites zip_limites = {
	2u * 1024 * 1024 * 1024,	/* todo junto: 2 GB */
	0xFFFFFFFFu,			/* cada uno: lo que cabe en FAT */
	1100,				/* deflate no da más de 1032 a 1 */
};
const char *zip_excluir[2];		/* (lo que no se mete al juntar: el ZIP que se hace y su temporal) */

const char *zip_texto_error(int e)
{
	switch (e) {
	case ZIP_BIEN: return "bien";
	case ZIP_E_ABRIR: return "no se puede abrir";
	case ZIP_E_NO_ZIP: return "no es un archivo ZIP";
	case ZIP_E_ROTO: return "el ZIP est\x82 roto (su directorio no cuadra)";
	case ZIP_E_MEMORIA: return "no hay memoria bastante";
	case ZIP_E_DEMASIADOS: return "tiene demasiados archivos dentro";
	case ZIP_E_BOMBA: return "descomprimido medir\xA1" "a demasiado (\xBFuna bomba ZIP?)";
	case ZIP_E_SOLAPADO: return "unos datos pisan los de otro archivo (una bomba ZIP)";
	case ZIP_E_RUTA: return "nombre peligroso (con .. o una ruta entera): no se saca";
	case ZIP_E_METODO: return "comprimido de una forma que no se sabe leer";
	case ZIP_E_CLAVE: return "hace falta la contrase\xA4" "a (o no es esa)";
	case ZIP_E_CRC: return "los datos est\xA0n mal (no dan su CRC)";
	case ZIP_E_DATOS: return "los datos comprimidos est\xA0n mal";
	case ZIP_E_LEER: return "no se puede leer";
	case ZIP_E_ESCRIBIR: return "no se puede escribir (\xBF" "disco lleno?)";
	case ZIP_E_CREAR: return "no se puede crear";
	case ZIP_E_YA_ESTA: return "ya est\xA0 (no se reemplaza)";
	case ZIP_E_GRANDE: return "mide m\xA0s de 4 GB (no cabe en FAT)";
	case ZIP_E_CORTADO: return "parado";
	case ZIP_E_HONDO: return "demasiadas carpetas unas dentro de otras";
	case ZIP_E_ENLACE: return "es un enlace simb\xA2lico: no se saca";
	case ZIP_E_NO_HAY: return "no est\xA0";
	case ZIP_E_FUERTE: return "cifrado con el m\x82todo \xAB" "fuerte\xBB de PKWARE (no se sabe)";
	}
	return "error";
}

uint32_t zip_crc32(uint32_t crc, const void *p, size_t n)
{
	return (uint32_t)mz_crc32(crc, p, n);
}

/* ------------------------------------------------------ los octetos */
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t le64(const uint8_t *p) { return le32(p) | (uint64_t)le32(p + 4) << 32; }
static void pon16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void pon32(uint8_t *p, uint32_t v) { pon16(p, v); pon16(p + 2, v >> 16); }
static void pon64(uint8_t *p, uint64_t v) { pon32(p, (uint32_t)v); pon32(p + 4, (uint32_t)(v >> 32)); }

static int lee_en(int h, uint64_t pos, void *b, uint32_t n)
{
	uint8_t *q = b;
	int32_t r;

	if (pl_ir(h, pos))
		return -1;
	while (n) {
		r = pl_leer(h, q, n);
		if (r <= 0)
			return -1;
		q += r;
		n -= (uint32_t)r;
	}
	return 0;
}

static int escribe_todo(int h, const void *b, uint32_t n)
{
	const uint8_t *q = b;
	int32_t r;

	while (n) {
		r = pl_escribir(h, q, n);
		if (r <= 0)
			return ZIP_E_ESCRIBIR;
		q += r;
		n -= (uint32_t)r;
	}
	return 0;
}

static char *copia(const char *s)
{
	size_t n = strlen(s);
	char *d = malloc(n + 1);

	if (d)
		memcpy(d, s, n + 1);
	return d;
}

/* ---------------------------------------------------- los nombres */
/* la página 437, de 0x80 a 0xFF, en Unicode */
static const uint16_t cp437[128] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
	0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
	0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
};

static int pon_utf8(uint32_t c, char *s, int i, int cabe)
{
	if (c < 0x80) {
		if (i + 1 >= cabe)
			return -1;
		s[i++] = (char)c;
	} else if (c < 0x800) {
		if (i + 2 >= cabe)
			return -1;
		s[i++] = (char)(0xC0 | c >> 6);
		s[i++] = (char)(0x80 | (c & 0x3F));
	} else {
		if (i + 3 >= cabe)
			return -1;
		s[i++] = (char)(0xE0 | c >> 12);
		s[i++] = (char)(0x80 | ((c >> 6) & 0x3F));
		s[i++] = (char)(0x80 | (c & 0x3F));
	}
	return i;
}

int de_437_a_utf8(const unsigned char *t, int n, char *s, int cabe)
{
	int i, j = 0, k;

	for (i = 0; i < n; i++) {
		k = pon_utf8(t[i] < 0x80 ? t[i] : cp437[t[i] - 0x80], s, j, cabe);
		if (k < 0)
			break;
		j = k;
	}
	s[j] = 0;
	return j;
}

/* la letra siguiente de un UTF-8 (0xFFFD si está mal) */
static uint32_t saca_utf8(const unsigned char **pp)
{
	const unsigned char *p = *pp;
	uint32_t c = *p++;
	int n = 0;

	if (c >= 0xF0 && c < 0xF8) {
		c &= 7;
		n = 3;
	} else if (c >= 0xE0) {
		c &= 15;
		n = 2;
	} else if (c >= 0xC0) {
		c &= 31;
		n = 1;
	} else if (c >= 0x80) {
		*pp = p;
		return 0xFFFD;
	}
	while (n--) {
		if ((*p & 0xC0) != 0x80) {
			*pp = p;
			return 0xFFFD;
		}
		c = c << 6 | (*p++ & 0x3F);
	}
	*pp = p;
	return c;
}

static int utf8_valido(const unsigned char *p, int n)
{
	const unsigned char *f = p + n;

	while (p < f) {
		if (*p < 0x80) {
			p++;
			continue;
		}
		if (saca_utf8(&p) == 0xFFFD || p > f)
			return 0;
	}
	return 1;
}

/* a la página 437 (para enseñarlo): lo que no está, «?»; las letras con
 * tilde que no tiene en mayúscula, sin tilde */
int utf8_a_437(const char *u, char *s, int cabe)
{
	const unsigned char *p = (const unsigned char *)u;
	int j = 0, k;
	uint32_t c;

	while (*p && j < cabe - 1) {
		c = saca_utf8(&p);
		if (c < 0x80) {
			s[j++] = (char)c;
			continue;
		}
		for (k = 0; k < 128 && cp437[k] != c; k++)
			;
		if (k < 128)
			s[j++] = (char)(0x80 + k);
		else if (c == 0xC1 || c == 0xC0 || c == 0xC2 || c == 0xC3)
			s[j++] = 'A';
		else if (c == 0xCD || c == 0xCC)
			s[j++] = 'I';
		else if (c == 0xD3 || c == 0xD2 || c == 0xD4 || c == 0xD5)
			s[j++] = 'O';
		else if (c == 0xDA || c == 0xD9)
			s[j++] = 'U';
		else if (c == 0xC8 || c == 0xCA || c == 0xCB)
			s[j++] = 'E';
		else if (c == 0x20AC)
			s[j++] = 'E';
		else
			s[j++] = '?';
	}
	s[j] = 0;
	return j;
}

/* ¿se puede sacar con ese nombre? (0, o por qué no) */
static int nombre_seguro(const char *n)
{
	const unsigned char *p = (const unsigned char *)n;
	int largo = 0, hondo = 0;

	if (!*p || *p == '/')
		return ZIP_E_RUTA;
	for (;; p++) {
		if (*p == '/' || !*p) {
			if (largo == 0 && *p)
				return ZIP_E_RUTA;	/* «a//b» */
			if (largo > 255)
				return ZIP_E_RUTA;
			if ((largo == 1 && p[-1] == '.') || (largo == 2 && p[-1] == '.' && p[-2] == '.'))
				return ZIP_E_RUTA;
			if (largo)
				hondo++;
			largo = 0;
			if (!*p || !p[1])
				break;
			continue;
		}
		if (*p < 0x20 || *p == 0x7F || *p == ':' || *p == '\\')
			return ZIP_E_RUTA;
		largo++;
	}
	return hondo > ZIP_HONDO ? ZIP_E_HONDO : 0;
}

/* --------------------------------------------------------- abrir */
static void ordena(uint32_t *ix, uint32_t n, const zmiembro *m)
{
	/* (Shell: sin recursión y sin qsort) */
	uint32_t paso, i, j, t;

	for (paso = n / 2; paso > 0; paso /= 2)
		for (i = paso; i < n; i++) {
			t = ix[i];
			for (j = i; j >= paso && m[ix[j - paso]].local > m[t].local; j -= paso)
				ix[j] = ix[j - paso];
			ix[j] = t;
		}
}

static uint32_t *orden_local;	/* los miembros, por dónde empiezan (para los solapes) */
static uint32_t *siguiente_de;	/* (de cada uno, el que va detrás: dónde acaban sus datos como mucho) */

static int lee_extra(zmiembro *m, const uint8_t *x, uint32_t n, uint32_t comp32, uint32_t tam32, uint32_t local32, char **unicode, uint32_t *ucrc, int *un)
{
	uint32_t id, t, i = 0, j;

	while (i + 4 <= n) {
		id = le16(x + i);
		t = le16(x + i + 2);
		i += 4;
		if (i + t > n)
			return ZIP_E_ROTO;
		if (id == 0x0001) {
			j = 0;
			if (tam32 == 0xFFFFFFFFu) {
				if (j + 8 > t)
					return ZIP_E_ROTO;
				m->tam = le64(x + i + j);
				j += 8;
			}
			if (comp32 == 0xFFFFFFFFu) {
				if (j + 8 > t)
					return ZIP_E_ROTO;
				m->comp = le64(x + i + j);
				j += 8;
			}
			if (local32 == 0xFFFFFFFFu) {
				if (j + 8 > t)
					return ZIP_E_ROTO;
				m->local = le64(x + i + j);
				j += 8;
			}
		} else if (id == 0x7075 && t >= 5 && x[i] == 1) {
			*ucrc = le32(x + i + 1);
			*unicode = (char *)(x + i + 5);
			*un = (int)t - 5;
		} else if (id == 0x9901 && t >= 7) {
			m->aes_version = (uint8_t)le16(x + i);
			m->aes_fuerza = x[i + 4];
			m->metodo_real = le16(x + i + 5);
		}
		i += t;
	}
	return 0;
}

int zip_abrir(zarchivo *z, const char *ruta)
{
	uint8_t *cola = NULL, *p, *fin;
	uint32_t cola_n, i, k, n, nn;
	uint64_t eocd = 0, cd_en, cd_n, total, base = 0;
	int64_t ajuste;
	int r, hallado = 0;

	memset(z, 0, sizeof(*z));
	z->h = pl_abrir(ruta, PL_LEER);
	if (z->h < 0)
		return ZIP_E_ABRIR;
	z->largo = pl_largo(z->h);
	if (z->largo < 22) {
		r = ZIP_E_NO_ZIP;
		goto mal;
	}
	cola_n = (uint32_t)(z->largo < 65557 + 20 ? z->largo : 65557 + 20);
	cola = malloc(cola_n);
	if (!cola) {
		r = ZIP_E_MEMORIA;
		goto mal;
	}
	if (lee_en(z->h, z->largo - cola_n, cola, cola_n)) {
		r = ZIP_E_LEER;
		goto mal;
	}
	/* el final del directorio central: «PK\5\6», buscándolo desde atrás */
	for (i = cola_n - 22 + 1; i-- > 0;)
		if (cola[i] == 'P' && cola[i + 1] == 'K' && cola[i + 2] == 5 && cola[i + 3] == 6 && i + 22 + le16(cola + i + 20) <= cola_n) {
			hallado = 1;
			break;
		}
	if (!hallado) {
		r = ZIP_E_NO_ZIP;
		goto mal;
	}
	eocd = z->largo - cola_n + i;
	p = cola + i;
	if (le16(p + 4) != 0 || (le16(p + 6) != 0 && le16(p + 6) != 0xFFFF)) {
		r = ZIP_E_ROTO;		/* de varios discos */
		goto mal;
	}
	total = le16(p + 10);
	cd_n = le32(p + 12);
	cd_en = le32(p + 16);
	base = eocd;
	/* ZIP64: su localizador va justo delante */
	if (i >= 20 && le32(cola + i - 20) == 0x07064B50u) {
		uint64_t z64 = le64(cola + i - 20 + 8);
		uint8_t q[56];

		if (z64 + 56 > eocd || lee_en(z->h, z64, q, 56) || le32(q) != 0x06064B50u) {
			r = ZIP_E_ROTO;
			goto mal;
		}
		total = le64(q + 32);
		cd_n = le64(q + 40);
		cd_en = le64(q + 48);
		base = z64;
		z->zip64 = 1;
	}
	/* lo que haya delante del ZIP (un autoextraíble): se corre todo */
	if (cd_en + cd_n > base) {
		r = ZIP_E_ROTO;
		goto mal;
	}
	ajuste = (int64_t)(base - (cd_en + cd_n));
	cd_en += (uint64_t)ajuste;
	if (total > ZIP_MAX_MIEMBROS) {
		r = ZIP_E_DEMASIADOS;
		goto mal;
	}
	if (cd_n > 64u * 1024 * 1024 || cd_n < total * 46) {
		r = ZIP_E_ROTO;
		goto mal;
	}
	free(cola);
	cola = NULL;
	z->dir = malloc((size_t)cd_n + 1);
	z->m = calloc((size_t)total + 1, sizeof(zmiembro));
	orden_local = malloc(((size_t)total + 1) * sizeof(uint32_t));
	siguiente_de = malloc(((size_t)total + 1) * sizeof(uint32_t));
	if (!z->dir || !z->m || !orden_local || !siguiente_de) {
		r = ZIP_E_MEMORIA;
		goto mal;
	}
	z->dir_n = cd_n;
	z->dir_en = cd_en;
	if (cd_n && lee_en(z->h, cd_en, z->dir, (uint32_t)cd_n)) {
		r = ZIP_E_LEER;
		goto mal;
	}
	p = z->dir;
	fin = z->dir + cd_n;
	for (k = 0; k < total; k++) {
		zmiembro *m = &z->m[k];
		uint32_t comp32, tam32, local32, xn, cn, ucrc = 0;
		char *unicode = NULL, *nombre;
		int un = 0, utf;

		if (p + 46 > fin || le32(p) != 0x02014B50u) {
			r = ZIP_E_ROTO;
			goto mal;
		}
		nn = le16(p + 28);
		xn = le16(p + 30);
		cn = le16(p + 32);
		if (p + 46 + nn + xn + cn > fin || nn == 0) {
			r = ZIP_E_ROTO;
			goto mal;
		}
		m->central = (uint32_t)(p - z->dir);
		m->hecho_por = le16(p + 4);
		m->version = le16(p + 6);
		m->marcas = le16(p + 8);
		m->metodo = le16(p + 10);
		m->hora = le16(p + 12);
		m->fecha = le16(p + 14);
		m->crc = le32(p + 16);
		comp32 = le32(p + 20);
		tam32 = le32(p + 24);
		local32 = le32(p + 42);
		m->comp = comp32;
		m->tam = tam32;
		m->local = local32;
		m->ext_atrib = le32(p + 38);
		m->nombre_n = (uint16_t)nn;
		m->extra_n = (uint16_t)xn;
		r = lee_extra(m, p + 46 + nn, xn, comp32, tam32, local32, &unicode, &ucrc, &un);
		if (r)
			goto mal;
		m->local += (uint64_t)ajuste;
		if (m->local + 30 > z->largo || m->comp > z->largo) {
			r = ZIP_E_ROTO;
			goto mal;
		}
		/* el nombre, en UTF-8 */
		nombre = malloc(nn * 3 + 2);
		if (!nombre) {
			r = ZIP_E_MEMORIA;
			goto mal;
		}
		utf = (m->marcas & 0x800) || ((m->hecho_por >> 8) == 3 && utf8_valido(p + 46, (int)nn));
		if (unicode && un > 0 && ucrc == zip_crc32(0, p + 46, nn) && utf8_valido((const uint8_t *)unicode, un)) {
			memcpy(nombre, unicode, (size_t)un);
			nombre[un] = 0;
		} else if (utf) {
			memcpy(nombre, p + 46, nn);
			nombre[nn] = 0;
		} else
			de_437_a_utf8(p + 46, (int)nn, nombre, (int)nn * 3 + 2);
		for (n = 0; nombre[n]; n++)
			if (nombre[n] == '\\')
				nombre[n] = '/';
		m->nombre = nombre;
		z->n = k + 1;		/* (para soltarlo si algo va mal más abajo) */
		n = (uint32_t)strlen(nombre);
		if (n && nombre[n - 1] == '/')
			m->carpeta = 1;
		else if ((m->ext_atrib & 0x10) && (m->hecho_por >> 8) != 3 && m->tam == 0 && m->comp == 0) {
			m->carpeta = 1;
			nombre[n] = '/';
			nombre[n + 1] = 0;
		}
		if ((m->hecho_por >> 8) == 3 && ((m->ext_atrib >> 16) & 0170000) == 0120000)
			m->enlace = 1;
		if (m->marcas & 1) {
			if (m->marcas & 0x40)
				m->cifrado = 3;		/* el «fuerte» de PKWARE */
			else
				m->cifrado = m->metodo == 99 ? ZC_AES : ZC_ZIPCRYPTO;
			z->cifrados++;
		}
		m->peligro = (int8_t)nombre_seguro(nombre);
		if (m->peligro)
			z->peligrosos++;
		z->total_tam += m->tam;
		z->total_comp += m->comp;
		orden_local[k] = k;
		p += 46 + nn + xn + cn;
	}
	z->n = (uint32_t)total;
	/* nadie pisa a nadie: cada uno acaba (como pronto) antes de que empiece
	 * el siguiente, y el último antes del directorio */
	ordena(orden_local, z->n, z->m);
	for (k = 0; k < z->n; k++) {
		zmiembro *a = &z->m[orden_local[k]];
		uint64_t lim = k + 1 < z->n ? z->m[orden_local[k + 1]].local : z->dir_en;

		siguiente_de[orden_local[k]] = k + 1 < z->n ? orden_local[k + 1] : 0xFFFFFFFFu;
		if (a->local + 30 + a->nombre_n + a->comp > lim) {
			r = ZIP_E_SOLAPADO;
			goto mal;
		}
	}
	return 0;
mal:
	free(cola);
	zip_cerrar(z);
	return r;
}

void zip_cerrar(zarchivo *z)
{
	uint32_t i;

	if (z->m) {
		for (i = 0; i < z->n; i++)
			free(z->m[i].nombre);
		free(z->m);
	}
	free(z->dir);
	if (z->h >= 0)
		pl_cerrar(z->h);
	memset(z, 0, sizeof(*z));
	z->h = -1;
	free(orden_local);
	free(siguiente_de);
	orden_local = siguiente_de = NULL;
}

int zip_pide_clave(const zarchivo *z, const uint8_t *elegidos)
{
	uint32_t i;

	for (i = 0; i < z->n; i++)
		if ((!elegidos || elegidos[i]) && z->m[i].cifrado && !z->m[i].carpeta)
			return 1;
	return 0;
}

/* --------------------------------------------------------- sacar */
#define TROZO 32768
static uint8_t *b_ent, *b_dic;
static tinfl_decompressor *infl;
static uint64_t sacado_total;		/* lo descomprimido en esta tanda (para el tope) */

static int buferes(void)
{
	if (!b_ent)
		b_ent = malloc(TROZO);
	if (!b_dic)
		b_dic = malloc(TINFL_LZ_DICT_SIZE);
	if (!infl)
		infl = malloc(sizeof(tinfl_decompressor));
	return b_ent && b_dic && infl ? 0 : ZIP_E_MEMORIA;
}

int zip_sacar(zarchivo *z, uint32_t i, const char *destino, const char *clave)
{
	zmiembro *m;
	uint8_t cab[30], zc[12], sal[18], firma[10];
	uint64_t datos, lim, queda, sale = 0;
	uint32_t crc = 0, dic = 0, n;
	int h = -1, r = 0, metodo, aes = 0, mira_crc = 1;
	zc_llaves k;
	wz_ctx *wz = NULL;
	size_t hay = 0;
	const uint8_t *sig = NULL;
	tinfl_status st;

	if (i >= z->n)
		return ZIP_E_NO_HAY;
	m = &z->m[i];
	if (m->peligro)
		return m->peligro;
	if (m->enlace)
		return ZIP_E_ENLACE;
	if (m->tam > zip_limites.max_uno || sacado_total + m->tam > zip_limites.max_total)
		return ZIP_E_BOMBA;
	if (m->tam > 16u * 1024 * 1024 && m->comp * zip_limites.max_proporcion < m->tam)
		return ZIP_E_BOMBA;
	if (m->tam > 0xFFFFFFFFu && pl_barra() == '\\')
		return ZIP_E_GRANDE;
	if (m->cifrado == 3)
		return ZIP_E_FUERTE;
	metodo = m->metodo;
	if (m->cifrado == ZC_AES) {
		if (m->aes_fuerza < 1 || m->aes_fuerza > 3 || (m->aes_version != 1 && m->aes_version != 2))
			return ZIP_E_METODO;
		metodo = m->metodo_real;
		aes = 1;
		mira_crc = m->aes_version == 1;
	}
	if (metodo != 0 && metodo != 8)
		return ZIP_E_METODO;
	if (m->cifrado && (!clave || !*clave))
		return ZIP_E_CLAVE;
	if ((r = buferes()))
		return r;
	/* la cabecera local, y que los datos no se salgan de lo suyo */
	if (lee_en(z->h, m->local, cab, 30) || le32(cab) != 0x04034B50u)
		return ZIP_E_ROTO;
	datos = m->local + 30 + le16(cab + 26) + le16(cab + 28);
	lim = siguiente_de && siguiente_de[i] != 0xFFFFFFFFu ? z->m[siguiente_de[i]].local : z->dir_en;
	if (datos + m->comp > lim)
		return ZIP_E_SOLAPADO;
	queda = m->comp;
	if (pl_ir(z->h, datos))
		return ZIP_E_LEER;
	if (m->cifrado == ZC_ZIPCRYPTO) {
		if (queda < 12 || pl_leer(z->h, zc, 12) != 12)
			return ZIP_E_DATOS;
		zc_empieza(&k, clave);
		zc_descifra(&k, zc, 12);
		if (zc[11] != (uint8_t)((m->marcas & 8) ? m->hora >> 8 : m->crc >> 24))
			return ZIP_E_CLAVE;
		queda -= 12;
	} else if (aes) {
		uint32_t ls = 4u + 4u * m->aes_fuerza;

		if (queda < ls + 2 + 10 || lee_en(z->h, datos, sal, ls + 2))
			return ZIP_E_DATOS;
		wz = malloc(sizeof(wz_ctx));
		if (!wz)
			return ZIP_E_MEMORIA;
		if (wz_empieza(wz, m->aes_fuerza, clave, sal, sal + ls)) {
			free(wz);
			return ZIP_E_CLAVE;
		}
		queda -= ls + 2 + 10;
	}
	if (destino) {
		h = pl_abrir(destino, PL_CREAR);
		if (h < 0) {
			free(wz);
			return ZIP_E_CREAR;
		}
	}
	if (metodo == 8)
		tinfl_init(infl);
	for (;;) {
		if (hay == 0 && queda) {
			n = queda > TROZO ? TROZO : (uint32_t)queda;
			if (pl_leer(z->h, b_ent, n) != (int32_t)n) {
				r = ZIP_E_LEER;
				break;
			}
			if (m->cifrado == ZC_ZIPCRYPTO)
				zc_descifra(&k, b_ent, n);
			else if (aes)
				wz_descifra(wz, b_ent, n);
			queda -= n;
			hay = n;
			sig = b_ent;
		}
		if (metodo == 0) {
			if (!hay)
				break;
			n = (uint32_t)hay;
			if (sale + n > m->tam) {
				r = ZIP_E_DATOS;
				break;
			}
			crc = zip_crc32(crc, sig, n);
			if (h >= 0 && escribe_todo(h, sig, n)) {
				r = ZIP_E_ESCRIBIR;
				break;
			}
			sale += n;
			hay = 0;
		} else {
			size_t ent = hay, fuera = TINFL_LZ_DICT_SIZE - dic;

			st = tinfl_decompress(infl, sig, &ent, b_dic, b_dic + dic, &fuera, queda ? TINFL_FLAG_HAS_MORE_INPUT : 0);
			hay -= ent;
			sig += ent;
			if (fuera) {
				if (sale + fuera > m->tam) {
					r = ZIP_E_DATOS;	/* da más de lo que dice: no se sigue */
					break;
				}
				crc = zip_crc32(crc, b_dic + dic, fuera);
				if (h >= 0 && escribe_todo(h, b_dic + dic, (uint32_t)fuera)) {
					r = ZIP_E_ESCRIBIR;
					break;
				}
				sale += fuera;
				dic = (uint32_t)((dic + fuera) & (TINFL_LZ_DICT_SIZE - 1));
			}
			if (st == TINFL_STATUS_DONE)
				break;
			if (st < 0 || (st == TINFL_STATUS_NEEDS_MORE_INPUT && !queda && !hay)) {
				r = ZIP_E_DATOS;
				break;
			}
		}
		if (pl_progreso(sale, m->tam, m->nombre)) {
			r = ZIP_E_CORTADO;
			break;
		}
	}
	if (!r && sale != m->tam)
		r = ZIP_E_DATOS;
	if (!r && aes) {
		if (lee_en(z->h, datos + m->comp - 10, firma, 10))
			r = ZIP_E_LEER;
		else if (wz_comprueba(wz, firma))
			r = ZIP_E_CRC;
	}
	if (!r && mira_crc && crc != m->crc)
		r = ZIP_E_CRC;
	free(wz);
	if (h >= 0) {
		pl_cerrar(h);
		if (r)
			pl_borrar(destino);
		else
			pl_poner_fecha(destino, m->fecha, m->hora);
	}
	if (!r)
		sacado_total += sale;
	return r;
}

/* las carpetas ya hechas en esta tanda: la del ZIP y la de aquí */
typedef struct { char *zip, *local; } zhecha;
static zhecha *hechas;
static char *primero;			/* (zcuenta.primero_local de la tanda) */
static uint32_t n_hechas, cabe_hechas;

static void suelta_hechas(void)
{
	uint32_t i;

	for (i = 0; i < n_hechas; i++) {
		free(hechas[i].zip);
		free(hechas[i].local);
	}
	free(hechas);
	hechas = NULL;
	n_hechas = cabe_hechas = 0;
}

static const char *hecha(const char *zipdir)
{
	uint32_t i;

	for (i = n_hechas; i-- > 0;)
		if (!strcmp(hechas[i].zip, zipdir))
			return hechas[i].local;
	return NULL;
}

static int apunta_hecha(const char *zipdir, const char *local)
{
	if (n_hechas == cabe_hechas) {
		zhecha *q = realloc(hechas, (cabe_hechas * 2 + 16) * sizeof(zhecha));

		if (!q)
			return ZIP_E_MEMORIA;
		hechas = q;
		cabe_hechas = cabe_hechas * 2 + 16;
	}
	hechas[n_hechas].zip = copia(zipdir);
	hechas[n_hechas].local = copia(local);
	if (!hechas[n_hechas].zip || !hechas[n_hechas].local)
		return ZIP_E_MEMORIA;
	n_hechas++;
	return 0;
}

static int junta(char *d, int cabe, const char *a, const char *b)
{
	size_t la = strlen(a), lb = strlen(b);

	if (la + lb + 2 > (size_t)cabe)
		return ZIP_E_HONDO;
	if (d != a)
		memcpy(d, a, la);
	if (la && d[la - 1] != pl_barra() && d[la - 1] != ':')
		d[la++] = pl_barra();
	memcpy(d + la, b, lb + 1);
	return 0;
}

/* la carpeta de aquí para «rel» (la parte del ZIP de una carpeta, sin la
 * «/» del final), haciéndola (y las de encima) si hace falta */
static int carpeta_de(const char *raiz, const char *rel, char *sale)
{
	char zipdir[ZIP_MAX_NOMBRE], comp[ZIP_MAX_NOMBRE], local[ZIP_MAX_RUTA], nuevo[ZIP_MAX_RUTA];
	const char *ya, *p = rel;
	int n = 0, r;

	strcpy(local, raiz);
	zipdir[0] = 0;
	while (*p) {
		int c = 0;

		while (*p && *p != '/' && c < ZIP_MAX_NOMBRE - 1)
			comp[c++] = *p++;
		comp[c] = 0;
		if (*p == '/')
			p++;
		if (n + c + 2 >= ZIP_MAX_NOMBRE)
			return ZIP_E_HONDO;
		memcpy(zipdir + n, comp, (size_t)c);
		n += c;
		zipdir[n++] = '/';
		zipdir[n] = 0;
		ya = hecha(zipdir);
		if (ya) {
			strcpy(local, ya);
			continue;
		}
		pl_nombre_local(local, comp, 1, nuevo, sizeof(nuevo));
		if ((r = junta(local, ZIP_MAX_RUTA, local, nuevo)))
			return r;
		if (pl_que_es(local) != 2 && pl_hacer_carpeta(local))
			return ZIP_E_CREAR;
		if (primero && !primero[0])
			strcpy(primero, local);
		if ((r = apunta_hecha(zipdir, local)))
			return r;
	}
	strcpy(sale, local);
	return 0;
}

int zip_sacar_todo(zarchivo *z, const char *carpeta, const uint8_t *elegidos, const char *quita, const char *clave, int reemplazar, zcuenta *c)
{
	uint32_t i;
	uint64_t total = 0;
	size_t lq = quita ? strlen(quita) : 0;
	char rel[ZIP_MAX_NOMBRE], dir[ZIP_MAX_RUTA], nombre[ZIP_MAX_RUTA], ruta[ZIP_MAX_RUTA];
	int r;

	memset(c, 0, sizeof(*c));
	for (i = 0; i < z->n; i++)
		if ((!elegidos || elegidos[i]) && !z->m[i].peligro)
			total += z->m[i].tam;
	if (total > zip_limites.max_total)
		return ZIP_E_BOMBA;
	if (pl_que_es(carpeta) != 2 && pl_hacer_carpeta(carpeta))
		return ZIP_E_CREAR;
	sacado_total = 0;
	primero = c->primero_local;
	for (i = 0; i < z->n; i++) {
		zmiembro *m = &z->m[i];
		char *barra;

		if (elegidos && !elegidos[i])
			continue;
		if (lq && strncmp(m->nombre, quita, lq))
			continue;
		r = m->peligro ? m->peligro : m->enlace ? ZIP_E_ENLACE : 0;
		if (!r && strlen(m->nombre + lq) >= sizeof(rel))
			r = ZIP_E_HONDO;
		if (!r) {
			strcpy(rel, m->nombre + lq);
			if (!rel[0])
				continue;
			if (m->carpeta) {
				rel[strlen(rel) - 1] = 0;
				r = carpeta_de(carpeta, rel, dir);
				if (!r) {
					c->sacados++;
					continue;
				}
			} else {
				barra = strrchr(rel, '/');
				if (barra) {
					*barra = 0;
					r = carpeta_de(carpeta, rel, dir);
					barra++;
				} else {
					strcpy(dir, carpeta);
					barra = rel;
				}
				if (!r) {
					pl_nombre_local(dir, barra, 0, nombre, sizeof(nombre));
					r = junta(ruta, sizeof(ruta), dir, nombre);
				}
				if (!r && pl_que_es(ruta) && !reemplazar) {
					c->saltados++;
					continue;
				}
				if (!r)
					r = zip_sacar(z, i, ruta, clave);
				if (!r) {
					c->sacados++;
					strcpy(c->ultimo_local, ruta);
					if (!c->primero_local[0])
						strcpy(c->primero_local, ruta);
					continue;
				}
			}
		}
		if (r == ZIP_E_CORTADO || r == ZIP_E_ESCRIBIR || r == ZIP_E_MEMORIA || r == ZIP_E_BOMBA) {
			suelta_hechas();
			primero = NULL;
			c->fallos++;
			c->ultimo_error = r;
			strncpy(c->ultimo, m->nombre, sizeof(c->ultimo) - 1);
			return r;
		}
		c->fallos++;
		c->ultimo_error = r;
		strncpy(c->ultimo, m->nombre, sizeof(c->ultimo) - 1);
	}
	suelta_hechas();
	primero = NULL;
	return c->fallos ? c->ultimo_error : 0;
}

/* --------------------------------------------------------- juntar */
static int pon_fuente(zfuentes *l, const char *local, const char *nombre, int carpeta, uint64_t tam, uint16_t fecha, uint16_t hora)
{
	zfuente *f;

	if (l->n >= ZIP_MAX_MIEMBROS)
		return ZIP_E_DEMASIADOS;
	if (l->n == l->cabe) {
		zfuente *q = realloc(l->f, (l->cabe * 2 + 32) * sizeof(zfuente));

		if (!q)
			return ZIP_E_MEMORIA;
		l->f = q;
		l->cabe = l->cabe * 2 + 32;
	}
	f = &l->f[l->n];
	f->local = copia(local);
	f->nombre = copia(nombre);
	if (!f->local || !f->nombre)
		return ZIP_E_MEMORIA;
	f->carpeta = (uint8_t)carpeta;
	f->tam = tam;
	f->fecha = fecha;
	f->hora = hora;
	l->n++;
	return 0;
}

typedef struct { zfuentes *l; const char *local, *nombre; int hondo, r; } zjunta;
static int junta_carpeta(zfuentes *l, const char *local, const char *nombre, int hondo, uint16_t fecha, uint16_t hora);

static int cada_uno(void *u, const char *largo, const char *loc, int carpeta, uint64_t tam, uint16_t fecha, uint16_t hora)
{
	zjunta *j = u;
	char ruta[ZIP_MAX_RUTA], nombre[ZIP_MAX_NOMBRE];
	size_t a = strlen(j->nombre), b = strlen(largo);
	int r;

	if (j->r)
		return 1;
	if (junta(ruta, sizeof(ruta), j->local, loc) || a + b + 3 > sizeof(nombre)) {
		j->r = ZIP_E_HONDO;
		return 1;
	}
	if ((zip_excluir[0] && !strcmp(ruta, zip_excluir[0])) || (zip_excluir[1] && !strcmp(ruta, zip_excluir[1])))
		return 0;
	memcpy(nombre, j->nombre, a);
	memcpy(nombre + a, largo, b + 1);
	if (carpeta)
		r = junta_carpeta(j->l, ruta, nombre, j->hondo + 1, fecha, hora);
	else
		r = pon_fuente(j->l, ruta, nombre, 0, tam, fecha, hora);
	if (r) {
		j->r = r;
		return 1;
	}
	return 0;
}

static int junta_carpeta(zfuentes *l, const char *local, const char *nombre, int hondo, uint16_t fecha, uint16_t hora)
{
	char n[ZIP_MAX_NOMBRE];
	size_t a = strlen(nombre);
	zjunta j;
	int r;

	if (hondo > ZIP_HONDO || a + 2 > sizeof(n))
		return ZIP_E_HONDO;
	memcpy(n, nombre, a);
	n[a] = '/';
	n[a + 1] = 0;
	if ((r = pon_fuente(l, local, n, 1, 0, fecha, hora)))
		return r;
	j.l = l;
	j.local = local;
	j.nombre = n;
	j.hondo = hondo;
	j.r = 0;
	if (pl_listar(local, cada_uno, &j) < 0)
		return ZIP_E_LEER;
	return j.r;
}

int zip_juntar(zfuentes *l, const char *ruta, const char *base)
{
	char largo[ZIP_MAX_NOMBRE];
	int carpeta = 0, r;
	uint64_t tam = 0;
	uint16_t fecha = 0, hora = 0;

	if (pl_ficha(ruta, largo, sizeof(largo), &carpeta, &tam, &fecha, &hora))
		return l->error = ZIP_E_ABRIR;
	if (base)
		strncpy(largo, base, sizeof(largo) - 1);
	if (carpeta)
		r = junta_carpeta(l, ruta, largo, 1, fecha, hora);
	else
		r = pon_fuente(l, ruta, largo, 0, tam, fecha, hora);
	if (r)
		l->error = r;
	return r;
}

void zip_suelta_fuentes(zfuentes *l)
{
	uint32_t i;

	for (i = 0; i < l->n; i++) {
		free(l->f[i].local);
		free(l->f[i].nombre);
	}
	free(l->f);
	memset(l, 0, sizeof(*l));
}

/* ----------------------------------------------------------- hacer */
typedef struct { uint8_t *p; uint32_t n, cabe; } zbuf;

static int al_buf(zbuf *b, const void *d, uint32_t n)
{
	if (b->n + n > b->cabe) {
		uint32_t c = (b->cabe + n) * 2 + 1024;
		uint8_t *q = realloc(b->p, c);

		if (!q)
			return ZIP_E_MEMORIA;
		b->p = q;
		b->cabe = c;
	}
	memcpy(b->p + b->n, d, n);
	b->n += n;
	return 0;
}

static int solo_ascii(const char *s)
{
	for (; *s; s++)
		if ((unsigned char)*s >= 0x80)
			return 0;
	return 1;
}

/* la entrada del directorio central de un miembro, con sus extras de
 * siempre (los de antes, sin el de ZIP64, que se hace de nuevo) */
static int al_central(zbuf *cd, const char *nombre, uint16_t hecho_por, uint16_t version, uint16_t marcas, uint16_t metodo,
		      uint16_t hora, uint16_t fecha, uint32_t crc, uint64_t comp, uint64_t tam, uint64_t local, uint32_t ext_atrib,
		      const uint8_t *extra, uint32_t extra_n)
{
	uint8_t c[46], z64[28];
	uint32_t nn = (uint32_t)strlen(nombre), zn = 4, i = 0, id, t;
	int r, grande = comp >= 0xFFFFFFFFu || tam >= 0xFFFFFFFFu || local >= 0xFFFFFFFFu;
	zbuf x = { 0, 0, 0 };

	if (grande) {
		pon16(z64, 1);
		if (tam >= 0xFFFFFFFFu) {
			pon64(z64 + zn, tam);
			zn += 8;
		}
		if (comp >= 0xFFFFFFFFu) {
			pon64(z64 + zn, comp);
			zn += 8;
		}
		if (local >= 0xFFFFFFFFu) {
			pon64(z64 + zn, local);
			zn += 8;
		}
		pon16(z64 + 2, zn - 4);
		if ((r = al_buf(&x, z64, zn)))
			return r;
		if (version < 45)
			version = 45;
	}
	while (extra && i + 4 <= extra_n) {
		id = le16(extra + i);
		t = le16(extra + i + 2);
		if (i + 4 + t > extra_n)
			break;
		if (id != 0x0001 && (r = al_buf(&x, extra + i, 4 + t))) {
			free(x.p);
			return r;
		}
		i += 4 + t;
	}
	pon32(c, 0x02014B50u);
	pon16(c + 4, hecho_por);
	pon16(c + 6, version);
	pon16(c + 8, marcas);
	pon16(c + 10, metodo);
	pon16(c + 12, hora);
	pon16(c + 14, fecha);
	pon32(c + 16, crc);
	pon32(c + 20, comp >= 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)comp);
	pon32(c + 24, tam >= 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)tam);
	pon16(c + 28, nn);
	pon16(c + 30, x.n);
	pon16(c + 32, 0);
	pon16(c + 34, 0);
	pon16(c + 36, 0);
	pon32(c + 38, ext_atrib);
	pon32(c + 42, local >= 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)local);
	r = al_buf(cd, c, 46);
	if (!r)
		r = al_buf(cd, nombre, nn);
	if (!r && x.n)
		r = al_buf(cd, x.p, x.n);
	free(x.p);
	return r;
}

/* la cabecera local; con «z64», el extra de ZIP64 con sitio para los
 * tamaños (que se ponen luego) */
static int cabecera_local(int h, const char *nombre, uint16_t version, uint16_t marcas, uint16_t metodo, uint16_t hora, uint16_t fecha,
			  uint32_t crc, uint64_t comp, uint64_t tam, int z64, const uint8_t *extra, uint32_t extra_n)
{
	uint8_t c[30 + 20];
	uint32_t nn = (uint32_t)strlen(nombre), xn = z64 ? 20 : 0;
	int r;

	pon32(c, 0x04034B50u);
	pon16(c + 4, z64 && version < 45 ? 45 : version);
	pon16(c + 6, marcas);
	pon16(c + 8, metodo);
	pon16(c + 10, hora);
	pon16(c + 12, fecha);
	pon32(c + 14, crc);
	pon32(c + 18, z64 ? 0xFFFFFFFFu : (uint32_t)comp);
	pon32(c + 22, z64 ? 0xFFFFFFFFu : (uint32_t)tam);
	pon16(c + 26, nn);
	pon16(c + 28, xn + extra_n);
	if ((r = escribe_todo(h, c, 30)) || (r = escribe_todo(h, nombre, nn)))
		return r;
	if (z64) {
		pon16(c, 1);
		pon16(c + 2, 16);
		pon64(c + 4, tam);
		pon64(c + 12, comp);
		if ((r = escribe_todo(h, c, 20)))
			return r;
	}
	if (extra_n)
		return escribe_todo(h, extra, extra_n);
	return 0;
}

/* «hecho en Unix» (3.0): así el unzip de Linux no pasa los nombres de la
 * página 437 (los nuestros van en UTF-8, con su marca); los atributos, los
 * de Unix arriba (rw-r--r--, rwxr-xr-x) y los del DOS abajo */
#define HECHO_POR 0x031E
#define ATRIB_ARCHIVO ((0100644u << 16) | 0x20)
#define ATRIB_CARPETA ((040755u << 16) | 0x10)

static tdefl_compressor *defl;
static uint8_t *b_sal;

/* comprime (o guarda) el archivo «f» en h, desde donde está: da su CRC y
 * lo que ocupa comprimido; *metodo puede pasar de 8 a 0 si no se gana */
static int mete_archivo(int h, uint64_t donde, const zfuente *f, int nivel, uint16_t *metodo, uint32_t *crc, uint64_t *comp, uint64_t *tam)
{
	int e = pl_abrir(f->local, PL_LEER), r = 0, vuelta;
	int32_t n;
	uint64_t hecho;

	if (e < 0)
		return ZIP_E_ABRIR;
	if (!b_sal)
		b_sal = malloc(TROZO);
	if (!b_ent)
		b_ent = malloc(TROZO);
	if (!defl)
		defl = malloc(sizeof(tdefl_compressor));
	if (!b_sal || !b_ent || !defl) {
		pl_cerrar(e);
		return ZIP_E_MEMORIA;
	}
	for (vuelta = 0; vuelta < 2; vuelta++) {
		*crc = 0;
		*comp = 0;
		hecho = 0;
		if (vuelta) {
			if (pl_ir(e, 0) || pl_ir(h, donde)) {
				r = ZIP_E_LEER;
				break;
			}
		}
		if (*metodo == 8)
			tdefl_init(defl, NULL, NULL, (int)tdefl_create_comp_flags_from_zip_params(nivel, -15, 0));
		for (;;) {
			n = pl_leer(e, b_ent, TROZO);
			if (n < 0) {
				r = ZIP_E_LEER;
				break;
			}
			*crc = zip_crc32(*crc, b_ent, (size_t)n);
			hecho += (uint64_t)n;
			if (*metodo == 0) {
				if (n && (r = escribe_todo(h, b_ent, (uint32_t)n)))
					break;
				*comp += (uint64_t)n;
			} else {
				const uint8_t *p = b_ent;
				size_t queda = (size_t)n;
				tdefl_status st;

				do {
					size_t ent = queda, sal = TROZO;

					st = tdefl_compress(defl, p, &ent, b_sal, &sal, n ? TDEFL_NO_FLUSH : TDEFL_FINISH);
					p += ent;
					queda -= ent;
					if (sal && (r = escribe_todo(h, b_sal, (uint32_t)sal)))
						break;
					*comp += sal;
					if (st < 0) {
						r = ZIP_E_DATOS;
						break;
					}
				} while (queda || (!n && st != TDEFL_STATUS_DONE));
				if (r)
					break;
			}
			if (pl_progreso(hecho, f->tam, f->nombre)) {
				r = ZIP_E_CORTADO;
				break;
			}
			if (n == 0)
				break;
		}
		if (r)
			break;
		*tam = hecho;
		/* sin ganar nada: guardado, que se lee mejor */
		if (*metodo == 8 && *comp >= hecho && vuelta == 0) {
			*metodo = 0;
			continue;
		}
		break;
	}
	pl_cerrar(e);
	return r;
}

int zip_hacer(const char *ruta, const char *temporal, zarchivo *viejo, const uint8_t *quitar, zfuentes *nuevos, int nivel)
{
	int h, r = 0;
	uint32_t i, j, n = 0;
	uint64_t pos = 0, cd_en;
	zbuf cd = { 0, 0, 0 };
	uint8_t e[56 + 20 + 22];
	uint8_t *repetido = NULL;

	h = pl_abrir(temporal, PL_CREAR);
	if (h < 0)
		return ZIP_E_CREAR;
	if (nuevos && nuevos->n) {
		repetido = calloc(nuevos->n, 1);
		if (!repetido) {
			pl_cerrar(h);
			pl_borrar(temporal);
			return ZIP_E_MEMORIA;
		}
		for (i = 0; i < nuevos->n; i++)
			for (j = 0; j < i && !repetido[i]; j++)
				if (!strcmp(nuevos->f[i].nombre, nuevos->f[j].nombre))
					repetido[i] = 1;
	}
	/* lo de antes, tal cual (sin descomprimir) */
	for (i = 0; viejo && i < viejo->n && !r; i++) {
		zmiembro *m = &viejo->m[i];
		uint8_t cab[30];
		uint64_t datos, queda;
		uint16_t marcas = m->marcas & ~8u;
		const uint8_t *extra = viejo->dir + m->central + 46 + m->nombre_n;
		uint8_t *sin64 = NULL;
		uint32_t sn = 0, k = 0, id, t;
		const char *nombre;

		if (quitar && quitar[i])
			continue;
		for (j = 0; nuevos && j < nuevos->n; j++)
			if (!strcmp(nuevos->f[j].nombre, m->nombre))
				break;
		if (nuevos && j < nuevos->n)
			continue;	/* lo reemplaza uno nuevo */
		if (lee_en(viejo->h, m->local, cab, 30) || le32(cab) != 0x04034B50u) {
			r = ZIP_E_ROTO;
			break;
		}
		datos = m->local + 30 + le16(cab + 26) + le16(cab + 28);
		/* el nombre: el de antes en UTF-8 (con su marca) */
		nombre = m->nombre;
		if (solo_ascii(nombre))
			marcas &= ~0x800u;
		else
			marcas |= 0x800;
		/* los extras de antes, menos el de ZIP64 y el del nombre Unicode */
		sin64 = malloc(m->extra_n + 1u);
		if (!sin64) {
			r = ZIP_E_MEMORIA;
			break;
		}
		while (k + 4 <= m->extra_n) {
			id = le16(extra + k);
			t = le16(extra + k + 2);
			if (k + 4 + t > m->extra_n)
				break;
			if (id != 0x0001 && id != 0x7075) {
				memcpy(sin64 + sn, extra + k, 4 + t);
				sn += 4 + t;
			}
			k += 4 + t;
		}
		r = cabecera_local(h, nombre, m->version, marcas, m->metodo, m->hora, m->fecha, m->crc, m->comp, m->tam,
				   m->comp >= 0xFFFFFFFFu || m->tam >= 0xFFFFFFFFu, sin64, sn);
		if (!r)
			r = al_central(&cd, nombre, m->hecho_por, m->version, marcas, m->metodo, m->hora, m->fecha, m->crc, m->comp, m->tam,
				       pos, m->ext_atrib, sin64, sn);
		free(sin64);
		if (r)
			break;
		if ((r = buferes()))
			break;
		if (pl_ir(viejo->h, datos)) {
			r = ZIP_E_LEER;
			break;
		}
		for (queda = m->comp; queda && !r;) {
			uint32_t c = queda > TROZO ? TROZO : (uint32_t)queda;

			if (pl_leer(viejo->h, b_ent, c) != (int32_t)c)
				r = ZIP_E_LEER;
			else
				r = escribe_todo(h, b_ent, c);
			queda -= c;
		}
		pos += 30 + strlen(nombre) + sn + (m->comp >= 0xFFFFFFFFu || m->tam >= 0xFFFFFFFFu ? 20 : 0) + m->comp;
		n++;
	}
	/* lo nuevo */
	for (i = 0; nuevos && i < nuevos->n && !r; i++) {
		zfuente *f = &nuevos->f[i];
		uint16_t metodo = f->carpeta || f->tam == 0 || nivel == 0 ? 0 : 8, marcas = solo_ascii(f->nombre) ? 0 : 0x800;
		uint32_t crc = 0, nn = (uint32_t)strlen(f->nombre);
		uint64_t comp = 0, tam = 0, desde;
		int z64 = f->tam >= 0xFFFFFFF0u - 0x100000u;
		uint8_t parche[16];

		if (repetido[i])
			continue;
		if (f->carpeta) {
			r = cabecera_local(h, f->nombre, 10, marcas, 0, f->hora, f->fecha, 0, 0, 0, 0, NULL, 0);
			if (!r)
				r = al_central(&cd, f->nombre, HECHO_POR, 10, marcas, 0, f->hora, f->fecha, 0, 0, 0, pos, ATRIB_CARPETA, NULL, 0);
			pos += 30 + nn;
			n++;
			continue;
		}
		if ((r = cabecera_local(h, f->nombre, metodo == 8 ? 20 : 10, marcas, metodo, f->hora, f->fecha, 0, 0, 0, z64, NULL, 0)))
			break;
		desde = pos + 30 + nn + (z64 ? 20 : 0);
		r = mete_archivo(h, desde, f, nivel, &metodo, &crc, &comp, &tam);
		if (r)
			break;
		if (!z64 && (comp >= 0xFFFFFFFFu || tam >= 0xFFFFFFFFu)) {
			r = ZIP_E_GRANDE;	/* (creció mientras se leía) */
			break;
		}
		/* los datos, en su cabecera */
		pon16(parche, metodo);
		if (pl_ir(h, pos + 8) || (r = escribe_todo(h, parche, 2)))
			r = r ? r : ZIP_E_ESCRIBIR;
		if (!r) {
			pon16(parche, metodo == 8 ? 20 : 10);
			if (!z64 && (pl_ir(h, pos + 4) || escribe_todo(h, parche, 2)))
				r = ZIP_E_ESCRIBIR;
		}
		pon32(parche, crc);
		pon32(parche + 4, z64 ? 0xFFFFFFFFu : (uint32_t)comp);
		pon32(parche + 8, z64 ? 0xFFFFFFFFu : (uint32_t)tam);
		if (!r && (pl_ir(h, pos + 14) || escribe_todo(h, parche, 12)))
			r = ZIP_E_ESCRIBIR;
		if (!r && z64) {
			pon64(parche, tam);
			pon64(parche + 8, comp);
			if (pl_ir(h, pos + 30 + nn + 4) || escribe_todo(h, parche, 16))
				r = ZIP_E_ESCRIBIR;
		}
		if (!r)
			r = al_central(&cd, f->nombre, HECHO_POR, metodo == 8 ? 20 : 10, marcas, metodo, f->hora, f->fecha, crc, comp, tam, pos, ATRIB_ARCHIVO, NULL, 0);
		pos = desde + comp;
		if (!r && pl_ir(h, pos))
			r = ZIP_E_ESCRIBIR;
		n++;
	}
	free(repetido);
	/* el directorio central y su final */
	cd_en = pos;
	if (!r && cd.n)
		r = escribe_todo(h, cd.p, cd.n);
	if (!r) {
		uint32_t en = 0;
		int z64 = n >= 0xFFFF || cd_en >= 0xFFFFFFFFu || cd.n >= 0xFFFFFFFFu;

		if (z64) {
			pon32(e, 0x06064B50u);
			pon64(e + 4, 44);
			pon16(e + 12, 45);
			pon16(e + 14, 45);
			pon32(e + 16, 0);
			pon32(e + 20, 0);
			pon64(e + 24, n);
			pon64(e + 32, n);
			pon64(e + 40, cd.n);
			pon64(e + 48, cd_en);
			pon32(e + 56, 0x07064B50u);
			pon32(e + 60, 0);
			pon64(e + 64, cd_en + cd.n);
			pon32(e + 72, 1);
			en = 76;
		}
		pon32(e + en, 0x06054B50u);
		pon16(e + en + 4, 0);
		pon16(e + en + 6, 0);
		pon16(e + en + 8, z64 ? 0xFFFF : n);
		pon16(e + en + 10, z64 ? 0xFFFF : n);
		pon32(e + en + 12, z64 ? 0xFFFFFFFFu : cd.n);
		pon32(e + en + 16, z64 ? 0xFFFFFFFFu : (uint32_t)cd_en);
		pon16(e + en + 20, 0);
		r = escribe_todo(h, e, en + 22);
	}
	free(cd.p);
	if (!r && pl_cortar(h))
		r = ZIP_E_ESCRIBIR;
	pl_cerrar(h);
	if (viejo && viejo->h >= 0) {
		pl_cerrar(viejo->h);
		viejo->h = -1;
	}
	if (r) {
		pl_borrar(temporal);
		return r;
	}
	if (pl_que_es(ruta) == 1 && pl_borrar(ruta)) {
		pl_borrar(temporal);
		return ZIP_E_CREAR;
	}
	if (pl_renombrar(temporal, ruta))
		return ZIP_E_CREAR;
	return 0;
}
