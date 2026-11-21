/* cvd.c - ESCUDO.BIN: los contenedores de firmas de ClamAV (.cvd y .cld).
 *
 * Un .cvd es una cabecera de texto de 512 octetos (rellena con espacios):
 *   ClamAV-VDB:fecha:versión:firmas:nivel:MD5:firma digital:autor:segundos
 * y, detrás, un .tar.gz (o un .tar sin comprimir: los .cld) con los archivos
 * de firmas (.hdb, .ndb, .ldb...), su .info y COPYING. La firma digital es la
 * «de siempre» de ClamAV (la legada, que freshclam sigue comprobando cuando no
 * hay .cvd.sign): la suma MD5 de lo que va detrás de la cabecera, elevada con
 * la clave pública RSA de 1024 bits de ClamAV (exponente 100001027, en
 * decimal); la firma va escrita en un base64 propio, de la cifra menos
 * significativa a la más (docs.clamav.net y libclamav/dsig.c, que es la
 * referencia del formato: no se copia código de allí). La cuenta de RSA la
 * hace BearSSL (terceros/bearssl, licencia MIT).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "escudo.h"
#include "bearssl_rsa.h"

/* la clave pública de ClamAV (el módulo, en decimal: es pública) */
static const char clave_n[] =
	"118640995551645342603070001658453189751527774412027743746599405743243142607464144767361060640655844749760788890022283424922"
	"762488917565551002467771109669598189410434699034532232228621591089508178591428456220796841621637175567590476666928698770143"
	"328137383952820383197532047771780196576957695822641224262693037";
#define CLAVE_E 100001027u

int es_hex(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int val_hex(int c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; }

static u32 num(const char *s)
{
	u32 n = 0;
	while (*s >= '0' && *s <= '9')
		n = n * 10 + (u32)(*s++ - '0');
	return n;
}

/* los días desde 1970 a AAAAMMDD (el algoritmo de los días civiles) */
u32 fecha_de_segundos(u32 s)
{
	i32 z = (i32)(s / 86400) + 719468, era, doe, yoe, y, doy, mp, d, m;

	era = z / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = yoe + era * 400;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	d = doy - (153 * mp + 2) / 5 + 1;
	m = mp < 10 ? mp + 3 : mp - 9;
	if (m <= 2)
		y++;
	return (u32)(y * 10000 + m * 100 + d);
}

int cvd_cabecera(const u8 *c512, cabecera_cvd *c)
{
	char t[513], *campo[9], *p;
	int n = 0, i;

	memcpy(t, c512, 512);
	t[512] = 0;
	if (memcmp(t, "ClamAV-VDB:", 11))
		return -1;
	for (i = 511; i >= 0 && (t[i] == ' ' || t[i] == 0 || t[i] == '\n' || t[i] == '\r'); i--)
		t[i] = 0;
	p = t;
	while (n < 9) {
		campo[n++] = p;
		p = strchr(p, ':');
		if (!p)
			break;
		*p++ = 0;
	}
	if (n < 9)
		return -1;
	memset(c, 0, sizeof *c);
	formatear(c->fecha_texto, sizeof c->fecha_texto, "%s", campo[1]);
	c->version = num(campo[2]);
	c->firmas = num(campo[3]);
	c->flevel = num(campo[4]);
	if (strlen(campo[5]) != 32 || strlen(campo[6]) >= sizeof c->firma)
		return -1;
	memcpy(c->md5, campo[5], 33);
	memcpy(c->firma, campo[6], strlen(campo[6]) + 1);
	formatear(c->autor, sizeof c->autor, "%s", campo[7]);
	c->segundos = num(campo[8]);
	return 0;
}

static int base64_clamav(int c)
{
	if (c >= 'a' && c <= 'z')
		return c - 'a';
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 26;
	if (c >= '0' && c <= '9')
		return c - '0' + 52;
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return -1;
}

int cvd_firma_buena(const cabecera_cvd *c, const u8 md5[16])
{
	u8 n[128], x[128], e[4], le[130];
	const char *s;
	u32 i, j, bit;
	br_rsa_public_key pk;

	/* el módulo, de decimal a binario (128 octetos, el alto primero) */
	memset(n, 0, sizeof n);
	for (s = clave_n; *s; s++) {
		u32 arrastre = (u32)(*s - '0');
		for (i = 128; i-- > 0;) {
			u32 v = n[i] * 10u + arrastre;
			n[i] = (u8)v;
			arrastre = v >> 8;
		}
	}
	e[0] = (u8)(CLAVE_E >> 24), e[1] = (u8)(CLAVE_E >> 16), e[2] = (u8)(CLAVE_E >> 8), e[3] = (u8)CLAVE_E;
	/* la firma: cada letra son 6 bits, la primera los más bajos */
	memset(le, 0, sizeof le);
	bit = 0;
	for (s = c->firma; *s; s++, bit += 6) {
		int v = base64_clamav((u8)*s);
		if (v < 0)
			return 0;
		for (j = 0; j < 6; j++)
			if (v >> j & 1) {
				u32 b = bit + j;
				if (b >= 1024)
					return 0;	/* no cabe: no es una firma de esta clave */
				le[b >> 3] |= (u8)(1u << (b & 7));
			}
	}
	for (i = 0; i < 128; i++)
		x[i] = le[127 - i];
	if (memcmp(x, n, 128) >= 0)
		return 0;
	pk.n = n, pk.nlen = 128, pk.e = e, pk.elen = 4;
	if (!br_rsa_i31_public(x, 128, &pk))
		return 0;
	for (i = 0; i < 112; i++)
		if (x[i])
			return 0;
	/* y lo que sale tiene que ser la suma MD5 de los datos y la de la cabecera */
	for (i = 0; i < 16; i++) {
		if (x[112 + i] != md5[i])
			return 0;
		if (!es_hex(c->md5[2 * i]) || !es_hex(c->md5[2 * i + 1]) ||
		    (u32)(val_hex(c->md5[2 * i]) << 4 | val_hex(c->md5[2 * i + 1])) != md5[i])
			return 0;
	}
	return 1;
}

/* ---- el .tar (comprimido o no) ---- */
typedef struct recorrido {
	int h;
	cvd_archivo uno;
	void *yo;
	u8 cab[512];
	u32 en_cab;			/* lo que se tiene de la cabecera */
	u32 queda;			/* lo que queda del archivo */
	u32 relleno;			/* lo que queda del relleno a 512 */
	char nombre[101];
	int es_archivo;
	int fin;
	int error;
	char *texto_error;
	int cabe_error;
} recorrido;

static u32 octal(const u8 *p, int n)
{
	u32 v = 0;
	int i;
	for (i = 0; i < n && (p[i] == ' ' || p[i] == 0); i++) ;
	for (; i < n && p[i] >= '0' && p[i] <= '7'; i++)
		v = v * 8 + (u32)(p[i] - '0');
	return v;
}

static int al_tar(void *yo, const u8 *d, u32 n)
{
	recorrido *r = yo;

	while (n && !r->fin) {
		if (r->queda) {
			u32 k = n < r->queda ? n : r->queda;
			if (r->es_archivo && r->uno(r->yo, r->nombre, d, k))
				return 1;
			d += k, n -= k, r->queda -= k;
			if (!r->queda && r->es_archivo && r->uno(r->yo, r->nombre, 0, 0))
				return 1;
			continue;
		}
		if (r->relleno) {
			u32 k = n < r->relleno ? n : r->relleno;
			d += k, n -= k, r->relleno -= k;
			continue;
		}
		/* la cabecera de un archivo del tar */
		{
			u32 k = 512 - r->en_cab, i, tam;
			if (k > n)
				k = n;
			memcpy(r->cab + r->en_cab, d, k);
			r->en_cab += k, d += k, n -= k;
			if (r->en_cab < 512)
				continue;
			r->en_cab = 0;
			for (i = 0; i < 512 && !r->cab[i]; i++) ;
			if (i == 512) {	/* dos bloques a cero: el final */
				r->fin = 1;
				break;
			}
			memcpy(r->nombre, r->cab, 100);
			r->nombre[100] = 0;
			tam = octal(r->cab + 124, 12);
			r->es_archivo = r->cab[156] == '0' || r->cab[156] == 0;
			r->queda = tam;
			r->relleno = (512 - (tam & 511)) & 511;
			if (!tam && r->es_archivo && (r->uno(r->yo, r->nombre, (const u8 *)"", 0) || r->uno(r->yo, r->nombre, 0, 0)))
				return 1;
		}
	}
	return pl_cortar() ? 1 : 0;
}

static int lee_archivo(void *yo, u8 *d, u32 cabe)
{
	recorrido *r = yo;
	return pl_leer(r->h, d, cabe);
}

int cvd_recorre(int h, u32 desde, cvd_archivo uno, void *yo, char *error, int cabe)
{
	static recorrido r;
	static inflar z;
	u8 dos[2];
	int e;

	memset(&r, 0, sizeof r);
	r.h = h, r.uno = uno, r.yo = yo;
	pl_mover(h, desde);
	if (pl_leer(h, dos, 2) != 2) {
		formatear(error, cabe, "está vacío");
		return -1;
	}
	pl_mover(h, desde);
	if (dos[0] == 0x1F && dos[1] == 0x8B) {
		memset(&z, 0, sizeof z);
		z.lee = lee_archivo;
		z.saca = al_tar;
		z.yo = &r;
		e = inflar_gzip(&z);
		if (e == INF_PARADO && (r.fin || pl_cortar()))
			e = INF_BIEN;
		if (e != INF_BIEN) {
			formatear(error, cabe, e == INF_SIN_MEMORIA ? "no hay memoria para descomprimirlo" : "lo comprimido está mal (gzip)");
			return -1;
		}
	} else {
		u8 b[4096];
		int n;
		while ((n = pl_leer(h, b, sizeof b)) > 0)
			if (al_tar(&r, b, (u32)n))
				break;
	}
	return 0;
}
