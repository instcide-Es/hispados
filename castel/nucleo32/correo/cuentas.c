/* cuentas.c - Correo: las cuentas (CUENTAS.INI), los ajustes (CORREO.INI) y
 * lo pequeño de todos (resumen32, copia_utf8, la ficha de unas cabeceras).
 * Los dos .INI van en la carpeta del correo (so_carpeta_datos: un secreto de
 * CORREO.BIN, que solo lee él, o la carpeta del usuario), en UTF-8, con
 * «clave=valor» por renglón y «[cuenta N]» para cada cuenta. La contraseña,
 * solo si la cuenta dice «guardar_clave=si».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "correo.h"

Cuenta cuentas[MAX_CUENTAS];
int n_cuentas;
Ajustes ajustes;
char dir_datos[200];

/* ---- lo pequeño ---- */
u32 resumen32(const char *t, u32 n)
{
	u32 h = 2166136261u, i;

	for (i = 0; i < n; i++) {
		u8 c = (u8)t[i];

		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
			continue;
		h = (h ^ c) * 16777619u;
	}
	return h ? h : 1;
}

void copia_utf8(char *d, u32 cabe, const char *t, u32 n)
{
	u32 k = 0, i = 0;

	if (!cabe)
		return;
	while (i < n && t[i]) {
		u32 j = i, l;
		u8 c = (u8)t[i];

		l = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
		if (i + l > n || k + l + 1 > cabe)
			break;
		for (; j < i + l; j++) {
			u8 x = (u8)t[j];

			d[k++] = (char)(x == '\r' || x == '\n' || x == '\t' ? ' ' : x);
		}
		i += l;
	}
	d[k] = 0;
}

static int sin_caja(const char *a, const char *b)
{
	for (; *a && *b; a++, b++) {
		char x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a, y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;

		if (x != y)
			return 0;
	}
	return *a == *b;
}

/* lo último de References (o el In-Reply-To): el Message-ID del padre */
static u32 padre_de(const char *irt, const char *refs)
{
	const char *t = irt && strchr(irt, '<') ? irt : refs;
	const char *a, *b;

	if (!t)
		return 0;
	if (t == refs) {
		a = strrchr(t, '<');
	} else
		a = strchr(t, '<');
	if (!a)
		return 0;
	b = strchr(a, '>');
	if (!b)
		return 0;
	return resumen32(a, (u32)(b - a + 1));
}

void ficha_de_cabeceras(Ficha *f, const u8 *cab, u32 n, int de_enviados)
{
	Mensaje *m = mime_analiza(cab, n);
	char *v;

	if (!m)
		return;
	v = mime_cabecera_texto(m, 0, "Subject");
	if (v) {
		copia_utf8(f->asunto, sizeof f->asunto, v, (u32)strlen(v));
		so_suelta(v);
	}
	v = mime_cabecera(m, 0, de_enviados ? "To" : "From", 0);
	if (v) {
		Direccion d[1];

		if (mime_direcciones(v, (u32)strlen(v), d, 1) > 0) {
			copia_utf8(f->de, sizeof f->de, d[0].nombre[0] ? d[0].nombre : d[0].dir, 200);
			copia_utf8(f->dir, sizeof f->dir, d[0].dir, 200);
		}
		so_suelta(v);
	}
	v = mime_cabecera(m, 0, "Date", 0);
	if (v) {
		i64 s = mime_fecha(v, (u32)strlen(v));

		if (s > 0 && s < 0xFFFFFFFFll)
			f->fecha = (u32)s;
		so_suelta(v);
	}
	v = mime_cabecera(m, 0, "Message-ID", 0);
	if (v) {
		const char *a = strchr(v, '<'), *b = a ? strchr(a, '>') : NULL;

		f->hilo = a && b ? resumen32(a, (u32)(b - a + 1)) : resumen32(v, (u32)strlen(v));
		so_suelta(v);
	}
	{
		char *irt = mime_cabecera(m, 0, "In-Reply-To", 0), *refs = mime_cabecera(m, 0, "References", 0);

		f->padre = padre_de(irt, refs);
		so_suelta(irt);
		so_suelta(refs);
	}
	v = mime_cabecera(m, 0, "Content-Type", 0);
	if (v) {
		char *t = mime_valor(v, (u32)strlen(v));

		if (t && (!strcmp(t, "multipart/mixed") || !strncmp(t, "application/", 12) || !strcmp(t, "multipart/report")))
			f->banderas |= B_ADJUNTO;
		so_suelta(t);
		so_suelta(v);
	}
	if (mime_suplantacion(m, NULL, 0) >= 2)
		f->banderas |= B_SUPLANTA;
	mime_libera(m);
}

/* ---- ¿un servidor de casa? ---- */
int servidor_local(const char *s)
{
	u32 a[4] = { 0, 0, 0, 0 }, k = 0, n = (u32)strlen(s);
	const char *p = s;
	int cifras = 0;

	if (sin_caja(s, "localhost"))
		return 1;
	if (n > 6 && sin_caja(s + n - 6, ".local"))
		return 1;
	if (n > 4 && sin_caja(s + n - 4, ".lan"))
		return 1;
	if (n > 5 && sin_caja(s + n - 5, ".home"))
		return 1;
	for (; *p; p++) {
		if (*p >= '0' && *p <= '9') {
			a[k] = a[k] * 10 + (u32)(*p - '0');
			if (a[k] > 255 || ++cifras > 3)
				return 0;
		} else if (*p == '.' && cifras && k < 3) {
			k++;
			cifras = 0;
		} else
			return 0;
	}
	if (k != 3 || !cifras)
		return 0;
	return a[0] == 10 || a[0] == 127 || (a[0] == 192 && a[1] == 168) || (a[0] == 172 && a[1] >= 16 && a[1] <= 31) ||
	       (a[0] == 169 && a[1] == 254) || (a[0] == 100 && a[1] >= 64 && a[1] <= 127);
}

/* ---- leer y escribir los .INI ---- */
static void ruta_de(char *d, u32 cabe, const char *nombre)
{
	snprintf(d, cabe, "%s%c%s", dir_datos, so_barra(), nombre);
}

static void escapa(const char *t, char *d, u32 cabe)
{
	u32 k = 0;

	for (; *t && k + 3 < cabe; t++) {
		if (*t == '\n') {
			d[k++] = '\\';
			d[k++] = 'n';
		} else if (*t == '\\') {
			d[k++] = '\\';
			d[k++] = '\\';
		} else if (*t != '\r')
			d[k++] = *t;
	}
	d[k] = 0;
}

static void desescapa(const char *t, u32 n, char *d, u32 cabe)
{
	u32 k = 0, i;

	for (i = 0; i < n && k + 1 < cabe; i++) {
		if (t[i] == '\\' && i + 1 < n) {
			i++;
			d[k++] = t[i] == 'n' ? '\n' : t[i];
		} else
			d[k++] = t[i];
	}
	d[k] = 0;
}

static u32 num(const char *t)
{
	u32 v = 0;

	while (*t >= '0' && *t <= '9' && v < 100000000)
		v = v * 10 + (u32)(*t++ - '0');
	return v;
}

static int modo_de(const char *t)
{
	if (sin_caja(t, "tls") || sin_caja(t, "ssl"))
		return SO_TLS;
	if (sin_caja(t, "starttls-imap"))
		return SO_STARTTLS_IMAP;
	if (sin_caja(t, "starttls-smtp"))
		return SO_STARTTLS_SMTP;
	if (sin_caja(t, "starttls-pop3"))
		return SO_STARTTLS_POP3;
	if (sin_caja(t, "sin-cifrar"))
		return SO_PLANO;
	return SO_TLS;
}

static const char *texto_modo(int m)
{
	switch (m) {
	case SO_STARTTLS_IMAP: return "starttls-imap";
	case SO_STARTTLS_SMTP: return "starttls-smtp";
	case SO_STARTTLS_POP3: return "starttls-pop3";
	case SO_PLANO: return "sin-cifrar";
	}
	return "tls";
}

/* recorre los renglones de un .INI: f(sección, clave, valor) */
static void recorre(const u8 *d, u32 n, void (*f)(void *u, int seccion, const char *clave, const char *valor, u32 vn), void *u)
{
	u32 i = 0;
	int sec = 0;

	while (i < n) {
		u32 j = i, k;
		char clave[48];

		while (j < n && d[j] != '\n')
			j++;
		k = j;
		if (k > i && d[k - 1] == '\r')
			k--;
		if (k > i && d[i] == '[') {
			const char *p = (const char *)d + i + 1;

			sec = 0;
			if (k - i > 8 && !strncmp(p, "cuenta ", 7))
				sec = (int)num(p + 7);
		} else if (k > i && d[i] != '#' && d[i] != ';') {
			u32 e = i, c;

			while (e < k && d[e] != '=')
				e++;
			if (e < k && e - i < sizeof clave) {
				for (c = 0; c < e - i; c++)
					clave[c] = (char)d[i + c];
				clave[c] = 0;
				f(u, sec, clave, (const char *)d + e + 1, k - e - 1);
			}
		}
		i = j + 1;
	}
}

static void lee_cuenta(void *u, int sec, const char *clave, const char *v, u32 vn)
{
	char t[700];
	Cuenta *c;
	int i;

	(void)u;
	if (sec < 1 || sec > MAX_CUENTAS)
		return;
	for (i = 0; i < n_cuentas && cuentas[i].num != sec; i++)
		;
	if (i == n_cuentas) {
		if (n_cuentas >= MAX_CUENTAS)
			return;
		c = &cuentas[n_cuentas++];
		memset(c, 0, sizeof *c);
		c->num = sec;
		c->dejar_en_servidor = 1;
		c->avisar = 1;
	} else
		c = &cuentas[i];
	desescapa(v, vn, t, sizeof t);
#define TXT(k, campo) else if (!strcmp(clave, k)) snprintf(c->campo, sizeof c->campo, "%s", t)
	if (0)
		;
	TXT("nombre", nombre);
	TXT("correo", correo);
	TXT("visible", visible);
	TXT("usuario", usuario);
	TXT("entrada", entrada);
	TXT("salida", salida);
	TXT("clave", clave);
	TXT("firma", firma);
	TXT("enviados", enviados);
	TXT("borradores", borradores);
	TXT("papelera", papelera);
	else if (!strcmp(clave, "tipo"))
		c->tipo = sin_caja(t, "pop3") ? CT_POP3 : CT_IMAP;
	else if (!strcmp(clave, "puerto_entrada"))
		c->puerto_entrada = num(t);
	else if (!strcmp(clave, "puerto_salida"))
		c->puerto_salida = num(t);
	else if (!strcmp(clave, "cifrado_entrada"))
		c->modo_entrada = modo_de(t);
	else if (!strcmp(clave, "cifrado_salida"))
		c->modo_salida = modo_de(t);
	else if (!strcmp(clave, "permitir_sin_cifrar"))
		c->sin_cifrar = t[0] == 's';
	else if (!strcmp(clave, "guardar_clave"))
		c->guardar_clave = t[0] == 's';
	else if (!strcmp(clave, "dejar_en_servidor"))
		c->dejar_en_servidor = t[0] == 's';
	else if (!strcmp(clave, "max_kb"))
		c->max_kb = num(t);
	else if (!strcmp(clave, "avisar"))
		c->avisar = t[0] == 's';
#undef TXT
}

int cuentas_lee(void)
{
	char r[260];
	u32 n;
	u8 *d;
	int i;

	n_cuentas = 0;
	ruta_de(r, sizeof r, "CUENTAS.INI");
	d = so_lee_archivo(r, &n, 1u << 20);
	if (!d)
		return 0;
	recorre(d, n, lee_cuenta, NULL);
	memset(d, 0, n);
	so_suelta(d);
	for (i = 0; i < n_cuentas; i++)
		if (!cuentas[i].guardar_clave)
			cuentas[i].clave[0] = 0;
	return n_cuentas;
}

int cuentas_guarda(void)
{
	char r[260], nueva[260], t[1400], e[1300];
	void *f;
	int i, ok = 0;

	ruta_de(nueva, sizeof nueva, "CUENTAS.NUE");
	ruta_de(r, sizeof r, "CUENTAS.INI");
	f = so_abre(nueva, SO_CREAR);
	if (!f)
		return -1;
	snprintf(t, sizeof t, "# Las cuentas de Correo (Hispa-Pentium). Solo lo lee CORREO.BIN.\r\n");
	ok |= so_escribe(f, t, (u32)strlen(t));
	for (i = 0; i < n_cuentas; i++) {
		Cuenta *c = &cuentas[i];

		escapa(c->firma, e, sizeof e);
		snprintf(t, sizeof t,
			 "[cuenta %d]\r\nnombre=%s\r\ncorreo=%s\r\nvisible=%s\r\nusuario=%s\r\ntipo=%s\r\n"
			 "entrada=%s\r\npuerto_entrada=%u\r\ncifrado_entrada=%s\r\nsalida=%s\r\npuerto_salida=%u\r\ncifrado_salida=%s\r\n"
			 "permitir_sin_cifrar=%s\r\nguardar_clave=%s\r\ndejar_en_servidor=%s\r\nmax_kb=%u\r\navisar=%s\r\n"
			 "enviados=%s\r\nborradores=%s\r\npapelera=%s\r\nfirma=%s\r\n",
			 c->num, c->nombre, c->correo, c->visible, c->usuario, c->tipo == CT_POP3 ? "pop3" : "imap",
			 c->entrada, (unsigned)c->puerto_entrada, texto_modo(c->modo_entrada), c->salida, (unsigned)c->puerto_salida,
			 texto_modo(c->modo_salida), c->sin_cifrar ? "si" : "no", c->guardar_clave ? "si" : "no",
			 c->dejar_en_servidor ? "si" : "no", (unsigned)c->max_kb, c->avisar ? "si" : "no", c->enviados,
			 c->borradores, c->papelera, e);
		ok |= so_escribe(f, t, (u32)strlen(t));
		if (c->guardar_clave && c->clave[0]) {
			escapa(c->clave, e, sizeof e);
			snprintf(t, sizeof t, "clave=%s\r\n", e);
			ok |= so_escribe(f, t, (u32)strlen(t));
			memset(t, 0, sizeof t);
			memset(e, 0, sizeof e);
		}
	}
	if (so_cierra(f) || ok) {
		so_borra(nueva);
		return -1;
	}
	so_borra(r);
	return so_renombra(nueva, r);
}

int cuenta_nueva(Cuenta *c)
{
	int k, i;

	if (n_cuentas >= MAX_CUENTAS)
		return -1;
	for (k = 1; k <= MAX_CUENTAS; k++) {
		for (i = 0; i < n_cuentas && cuentas[i].num != k; i++)
			;
		if (i == n_cuentas)
			break;
	}
	c->num = k;
	cuentas[n_cuentas++] = *c;
	return k;
}

int cuenta_quita(int i)
{
	if (i < 0 || i >= n_cuentas)
		return -1;
	memset(cuentas[i].clave, 0, sizeof cuentas[i].clave);
	memmove(&cuentas[i], &cuentas[i + 1], (size_t)(n_cuentas - i - 1) * sizeof(Cuenta));
	n_cuentas--;
	return 0;
}

/* ---- los ajustes ---- */
static void lee_ajuste(void *u, int sec, const char *clave, const char *v, u32 vn)
{
	char t[220];

	(void)u;
	if (sec)
		return;
	copia_utf8(t, sizeof t, v, vn);
	if (!strcmp(clave, "max_mensaje_kb"))
		ajustes.max_mensaje_kb = num(t);
	else if (!strcmp(clave, "max_almacen_mb"))
		ajustes.max_almacen_mb = num(t);
	else if (!strcmp(clave, "cada_min"))
		ajustes.cada_min = num(t);
	else if (!strcmp(clave, "html"))
		ajustes.html = t[0] == 's';
	else if (!strcmp(clave, "hilos"))
		ajustes.hilos = t[0] == 's';
	else if (!strcmp(clave, "avisar"))
		ajustes.avisar = t[0] == 's';
	else if (!strcmp(clave, "ispdb"))
		snprintf(ajustes.ispdb, sizeof ajustes.ispdb, "%s", t);
	else if (!strcmp(clave, "orden"))
		ajustes.orden_col = (int)num(t);
	else if (!strcmp(clave, "orden_baja"))
		ajustes.orden_baja = t[0] == 's';
}

void ajustes_lee(void)
{
	char r[260];
	u32 n;
	u8 *d;

	memset(&ajustes, 0, sizeof ajustes);
	ajustes.max_mensaje_kb = 25 * 1024;
	ajustes.max_almacen_mb = 0;
	ajustes.cada_min = 10;
	ajustes.html = 1;
	ajustes.avisar = 1;
	ajustes.orden_col = 2;	/* la fecha */
	ajustes.orden_baja = 1;
	snprintf(ajustes.ispdb, sizeof ajustes.ispdb, "https://autoconfig.thunderbird.net/v1.1/");
	ruta_de(r, sizeof r, "CORREO.INI");
	d = so_lee_archivo(r, &n, 65536);
	if (!d)
		return;
	recorre(d, n, lee_ajuste, NULL);
	so_suelta(d);
	if (ajustes.max_mensaje_kb < 64)
		ajustes.max_mensaje_kb = 64;
}

int ajustes_guarda(void)
{
	char r[260], t[800];
	void *f;
	int ok;

	ruta_de(r, sizeof r, "CORREO.INI");
	f = so_abre(r, SO_CREAR);
	if (!f)
		return -1;
	snprintf(t, sizeof t,
		 "# Los ajustes de Correo (Hispa-Pentium)\r\nmax_mensaje_kb=%u\r\nmax_almacen_mb=%u\r\ncada_min=%u\r\nhtml=%s\r\n"
		 "hilos=%s\r\navisar=%s\r\nispdb=%s\r\norden=%d\r\norden_baja=%s\r\n",
		 (unsigned)ajustes.max_mensaje_kb, (unsigned)ajustes.max_almacen_mb, (unsigned)ajustes.cada_min,
		 ajustes.html ? "si" : "no", ajustes.hilos ? "si" : "no", ajustes.avisar ? "si" : "no", ajustes.ispdb,
		 ajustes.orden_col, ajustes.orden_baja ? "si" : "no");
	ok = so_escribe(f, t, (u32)strlen(t));
	return so_cierra(f) || ok ? -1 : 0;
}
