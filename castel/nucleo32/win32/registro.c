/* registro.c - WIN32.BIN (JUEGOS): el registro de Windows (ADVAPI32).
 *
 * Un árbol en memoria con las claves y sus valores, que se lee al primer
 * uso y se guarda (si ha cambiado) al cerrar una clave, con RegFlushKey y al
 * acabar el programa. En la unidad del sistema y fuera de la zona protegida:
 *   HKEY_CURRENT_USER   \USUARIOS\<usuario>\REGISTRO.REG (la carpeta de
 *                       diagnostico.c, la del registro de WIN32)
 *   HKEY_LOCAL_MACHINE  \USUARIOS\EQUIPO.REG (de todos los usuarios), con
 *                       unos valores de fábrica debajo (los de un Windows
 *                       98 con DirectX 7: los miran algunos instaladores)
 *   HKEY_CLASSES_ROOT   es HKLM\Software\Classes; HKEY_USERS\.DEFAULT, HKCU
 * Los archivos son de texto, como los .REG de REGEDIT4 (se pueden mirar y
 * cambiar con el Bloc de notas), con las letras del sistema.
 * Hecho con la documentación pública de Win32 (sala limpia).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"
#define API(t) t WINAPI

#define R_HKCR 0x80000000u
#define R_HKCU 0x80000001u
#define R_HKLM 0x80000002u
#define R_HKU 0x80000003u
#define R_HKCC 0x80000005u
#define E_OK 0
#define E_NOHAY 2
#define E_MAL 6
#define E_DENEGADO 5
#define E_PARAM 87
#define E_MAS 234
#define E_NOMAS 259

typedef struct Valor { char nombre[80]; u32 tipo, tam; u8 *datos; struct Valor *sig; } Valor;
typedef struct Clave { char nombre[80]; struct Clave *padre, *hijos, *sig; Valor *valores; int borrada; int colmena; } Clave;
static Clave *raiz_cu, *raiz_lm, *raiz_vacia;
static int cargado, sucia[2];
extern char carpeta_usuario[128];
void diag_apunta(const char *f, ...);
extern char letra_sistema;

/* ---- las abiertas (HKEY) ---- */
#define MAX_ABIERTAS 128
static Clave *abiertas[MAX_ABIERTAS];
static HKEY abre_h(Clave *c)
{
	int k;
	for (k = 0; k < MAX_ABIERTAS; k++)
		if (!abiertas[k]) {
			abiertas[k] = c;
			return (HKEY)(0x00C00000 + k * 4);
		}
	return 0;
}
static void carga(void);
static Clave *busca_hijo(Clave *c, const char *n, u32 largo);
static Clave *clave_de(HKEY h)
{
	u32 v = (u32)h;
	carga();
	switch (v) {
	case R_HKCU: return raiz_cu;
	case R_HKLM: return raiz_lm;
	case R_HKCR: {
		Clave *s = busca_hijo(raiz_lm, "Software", 8);
		Clave *c = s ? busca_hijo(s, "Classes", 7) : 0;
		return c ? c : raiz_vacia;
	}
	case R_HKU: return raiz_vacia;
	case R_HKCC: return raiz_vacia;
	}
	if (v >= 0x00C00000 && v < 0x00C00000 + MAX_ABIERTAS * 4 && !(v & 3) && abiertas[(v - 0x00C00000) / 4])
		return abiertas[(v - 0x00C00000) / 4];
	return 0;
}

/* ---- el árbol ---- */
static Clave *nueva_clave(Clave *padre, const char *n, u32 largo)
{
	Clave *c = m_cero(sizeof *c);
	if (!c)
		return 0;
	if (largo >= sizeof c->nombre)
		largo = sizeof c->nombre - 1;
	memcpy(c->nombre, n, largo);
	c->nombre[largo] = 0;
	c->padre = padre;
	if (padre) {
		/* en orden (sin mirar mayúsculas), como las enseña Windows */
		Clave **p = &padre->hijos;
		while (*p && stricmp_((*p)->nombre, c->nombre) < 0)
			p = &(*p)->sig;
		c->sig = *p;
		*p = c;
		c->colmena = padre->colmena;
	}
	return c;
}
static Clave *busca_hijo(Clave *c, const char *n, u32 largo)
{
	Clave *h;
	if (!c)
		return 0;
	for (h = c->hijos; h; h = h->sig)
		if (!h->borrada && strlen(h->nombre) == largo && !strnicmp_(h->nombre, n, largo))
			return h;
	return 0;
}
/* sigue la ruta «Uno\Dos\Tres» desde c (creándola, con «crear») */
static Clave *sigue_ruta(Clave *c, const char *r, int crear, int *creada)
{
	if (creada)
		*creada = 0;
	if (!r)
		return c;
	while (*r == '\\')
		r++;
	while (c && *r) {
		const char *f = r;
		Clave *h;
		while (*f && *f != '\\')
			f++;
		h = busca_hijo(c, r, f - r);
		if (!h && crear) {
			h = nueva_clave(c, r, f - r);
			if (creada)
				*creada = 1;
		}
		c = h;
		r = f;
		while (*r == '\\')
			r++;
	}
	return c;
}
static Valor *busca_valor(Clave *c, const char *n)
{
	Valor *v;
	if (!n)
		n = "";
	for (v = c->valores; v; v = v->sig)
		if (!stricmp_(v->nombre, n))
			return v;
	return 0;
}
static int pon_valor(Clave *c, const char *n, u32 tipo, const u8 *d, u32 tam)
{
	Valor *v = busca_valor(c, n), **p;
	u8 *nuevo;
	int texto = tipo == 1 || tipo == 2 || tipo == 7;
	u32 t2 = tam;
	if (texto && (!tam || d[tam - 1]))
		t2 = tam + 1;                   /* (los textos, siempre con su 0) */
	nuevo = m_pedir(t2 ? t2 : 1);
	if (!nuevo)
		return 0;
	if (tam)
		memcpy(nuevo, d, tam);
	if (t2 > tam)
		nuevo[tam] = 0;
	if (!v) {
		v = m_cero(sizeof *v);
		if (!v) {
			m_soltar(nuevo);
			return 0;
		}
		copia_n(v->nombre, n ? n : "", sizeof v->nombre);
		for (p = &c->valores; *p; p = &(*p)->sig)
			;
		*p = v;
	} else if (v->datos)
		m_soltar(v->datos);
	v->tipo = tipo;
	v->datos = nuevo;
	v->tam = t2;
	return 1;
}

/* ---- leer y escribir los archivos (REGEDIT4, con las letras del sistema) ---- */
static void ruta_de(int colmena, char *d, u32 cabe)
{
	if (colmena == 0)
		fmt(d, cabe, "%sREGISTRO.REG", carpeta_usuario);
	else
		fmt(d, cabe, "%c:\\USUARIOS\\EQUIPO.REG", letra_sistema ? letra_sistema : 'C');
}
/* (sin cambiar «/» por «\», como hace a437 con las rutas) */
static void a_sis(char *d, const char *s, u32 cabe)
{
	u32 k;
	for (k = 0; s[k] && k + 1 < cabe; k++)
		d[k] = (u8)s[k] < 128 ? s[k] : (char)t1252_437[(u8)s[k] - 128];
	d[k] = 0;
}
static void de_sis(char *d, const char *s, u32 cabe)
{
	u32 k;
	for (k = 0; s[k] && k + 1 < cabe; k++)
		d[k] = (u8)s[k] < 128 ? s[k] : (char)t437_1252[(u8)s[k] - 128];
	d[k] = 0;
}

static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1; }
/* lee una línea «"nombre"=valor» (o «@=valor») de la clave c */
static void lee_valor(Clave *c, char *l)
{
	char n[80];
	u32 k = 0;
	u8 *d;
	u32 nd = 0, tipo;
	char *p = l;
	if (*p == '@') {
		n[0] = 0;
		p++;
	} else if (*p == '"') {
		p++;
		while (*p && *p != '"' && k + 1 < sizeof n) {
			if (*p == '\\' && p[1])
				p++;
			n[k++] = *p++;
		}
		n[k] = 0;
		if (*p == '"')
			p++;
	} else
		return;
	if (*p != '=')
		return;
	p++;
	d = m_pedir(strlen(p) + 4);
	if (!d)
		return;
	if (*p == '"') {
		p++;
		while (*p && *p != '"') {
			if (*p == '\\' && p[1])
				p++;
			d[nd++] = *p++;
		}
		d[nd++] = 0;
		tipo = 1;
	} else if (!strnicmp_(p, "dword:", 6)) {
		u32 v = 0;
		int h;
		for (p += 6; (h = hexv(*p)) >= 0; p++)
			v = v * 16 + h;
		memcpy(d, &v, 4);
		nd = 4;
		tipo = 4;
	} else if (!strnicmp_(p, "hex", 3)) {
		tipo = 3;
		p += 3;
		if (*p == '(') {
			tipo = 0;
			for (p++; hexv(*p) >= 0; p++)
				tipo = tipo * 16 + hexv(*p);
			if (*p == ')')
				p++;
		}
		if (*p == ':')
			p++;
		while (*p) {
			int a = hexv(p[0]), b = a >= 0 ? hexv(p[1]) : -1;
			if (a < 0 || b < 0)
				break;
			d[nd++] = (u8)(a * 16 + b);
			p += 2;
			while (*p == ',' || *p == ' ')
				p++;
		}
	} else {
		m_soltar(d);
		return;
	}
	pon_valor(c, n, tipo, d, nd);
	m_soltar(d);
}
static void lee_archivo(int colmena)
{
	char r[160], r4[160];
	u32 m, n, k;
	char *t, *l;
	Clave *c = 0;
	ruta_de(colmena, r, sizeof r);
	if (colmena == 0 && !carpeta_usuario[0])
		return;
	a437(r4, r, sizeof r4);
	m = sys_abrir(r4, 0);
	if (ES_ERROR(m))
		return;
	n = sys_largo(m);
	if (ES_ERROR(n) || n > 0x400000) {
		sys_cerrar(m);
		return;
	}
	t = m_pedir(n + 2);
	if (!t) {
		sys_cerrar(m);
		return;
	}
	n = sys_leer(m, t, n);
	sys_cerrar(m);
	if (ES_ERROR(n))
		n = 0;
	t[n] = 0;
	/* los valores largos (hex) siguen en la línea de abajo, con «\» al final */
	for (k = 0; k + 1 < n; k++)
		if (t[k] == '\\' && (t[k + 1] == '\r' || t[k + 1] == '\n')) {
			u32 j = k;
			t[j++] = ' ';
			while (t[j] == '\r' || t[j] == '\n' || t[j] == ' ')
				t[j++] = ' ';
		}
	for (l = t; *l; ) {
		char *f = l, linea[1200];
		while (*f && *f != '\n' && *f != '\r')
			f++;
		{
			u32 largo = f - l;
			char crudo[1200];
			if (largo >= sizeof crudo)
				largo = sizeof crudo - 1;
			memcpy(crudo, l, largo);
			crudo[largo] = 0;
			de_sis(linea, crudo, sizeof linea);
		}
		while (*f == '\n' || *f == '\r')
			f++;
		l = f;
		{
			char *p = linea;
			while (*p == ' ' || *p == '\t')
				p++;
			if (*p == '[') {
				char *q = strchr(p, ']'), *ruta;
				Clave *raiz = 0;
				if (!q)
					continue;
				*q = 0;
				ruta = p + 1;
				if (!strnicmp_(ruta, "HKEY_CURRENT_USER", 17)) { raiz = raiz_cu; ruta += 17; }
				else if (!strnicmp_(ruta, "HKEY_LOCAL_MACHINE", 18)) { raiz = raiz_lm; ruta += 18; }
				c = raiz ? sigue_ruta(raiz, ruta, 1, 0) : 0;
			} else if (c && (*p == '"' || *p == '@'))
				lee_valor(c, p);
		}
	}
	m_soltar(t);
}

/* un trozo al archivo (con un búfer) */
static char *ebuf;
static u32 en, em;
static void sale(const char *s)
{
	u32 n = strlen(s);
	char t[600];
	a_sis(t, s, sizeof t);
	n = strlen(t);
	if (en + n > 8192) {
		sys_escribir_archivo(em, ebuf, en);
		en = 0;
	}
	memcpy(ebuf + en, t, n);
	en += n;
}
static void sale_texto(const char *s)
{
	char b[4];
	sale("\"");
	for (; *s; s++) {
		b[0] = *s;
		b[1] = 0;
		if (*s == '"' || *s == '\\')
			sale("\\");
		sale(b);
	}
	sale("\"");
}
static void escribe_clave(Clave *c, char *ruta, u32 largo)
{
	Clave *h;
	Valor *v;
	u32 k;
	if (c->borrada)
		return;
	if (c->valores || !c->hijos) {
		sale("\r\n[");
		sale(ruta);
		sale("]\r\n");
		for (v = c->valores; v; v = v->sig) {
			char t[16];
			if (v->nombre[0])
				sale_texto(v->nombre);
			else
				sale("@");
			sale("=");
			if (v->tipo == 1 && v->tam && !v->datos[v->tam - 1] && strlen((char *)v->datos) + 1 == v->tam)
				sale_texto((char *)v->datos);
			else if (v->tipo == 4 && v->tam == 4) {
				fmt(t, sizeof t, "dword:%08x", *(u32 *)v->datos);
				sale(t);
			} else {
				if (v->tipo == 3)
					sale("hex:");
				else {
					fmt(t, sizeof t, "hex(%x):", v->tipo);
					sale(t);
				}
				for (k = 0; k < v->tam; k++) {
					fmt(t, sizeof t, k + 1 < v->tam ? "%02x," : "%02x", v->datos[k]);
					sale(t);
					if (k % 24 == 23 && k + 1 < v->tam)
						sale("\\\r\n  ");
				}
			}
			sale("\r\n");
		}
	}
	for (h = c->hijos; h; h = h->sig) {
		u32 n = strlen(h->nombre);
		if (largo + n + 2 >= 600)
			continue;
		ruta[largo] = '\\';
		memcpy(ruta + largo + 1, h->nombre, n + 1);
		escribe_clave(h, ruta, largo + 1 + n);
		ruta[largo] = 0;
	}
}
static void guarda(int colmena)
{
	char r[160], r4[160], ruta[600];
	Clave *raiz = colmena ? raiz_lm : raiz_cu;
	if (!sucia[colmena] || (colmena == 0 && !carpeta_usuario[0]))
		return;
	ruta_de(colmena, r, sizeof r);
	a437(r4, r, sizeof r4);
	em = sys_abrir(r4, 2);
	if (ES_ERROR(em)) {
		diag_apunta("registro: no se puede guardar %s (error %x)", r, em);
		return;
	}
	ebuf = m_pedir(8192);
	if (!ebuf) {
		sys_cerrar(em);
		return;
	}
	en = 0;
	sale("REGEDIT4\r\n");
	strcpy(ruta, colmena ? "HKEY_LOCAL_MACHINE" : "HKEY_CURRENT_USER");
	{
		Clave *h;
		for (h = raiz->hijos; h; h = h->sig) {
			u32 n = strlen(ruta);
			ruta[n] = '\\';
			strcpy(ruta + n + 1, h->nombre);
			escribe_clave(h, ruta, strlen(ruta));
			ruta[n] = 0;
		}
	}
	sale("\r\n");
	if (en)
		sys_escribir_archivo(em, ebuf, en);
	sys_cerrar(em);
	m_soltar(ebuf);
	sucia[colmena] = 0;
	traza("registro: guardado %s", r);
}
void registro_guarda(void) { if (cargado) { guarda(0); guarda(1); } }

/* lo que hay de fábrica en HKLM (lo que miran algunos programas e instaladores) */
static void de_fabrica(void)
{
	Clave *c;
	char t[80];
	u32 v;
	c = sigue_ruta(raiz_lm, "Software\\Microsoft\\Windows\\CurrentVersion", 1, 0);
	pon_valor(c, "VersionNumber", 1, (const u8 *)"4.10.2222", 10);
	pon_valor(c, "Version", 1, (const u8 *)"Windows 98", 11);
	pon_valor(c, "ProductName", 1, (const u8 *)"Hispa-Pentium", 14);
	fmt(t, sizeof t, "%c:\\PROGRAMA", letra_sistema ? letra_sistema : 'C');
	pon_valor(c, "ProgramFilesDir", 1, (const u8 *)t, strlen(t) + 1);
	fmt(t, sizeof t, "%c:\\PROGRAMA\\COMUNES", letra_sistema ? letra_sistema : 'C');
	pon_valor(c, "CommonFilesDir", 1, (const u8 *)t, strlen(t) + 1);
	copia_n(t, carpeta_w32, sizeof t);
	if (strlen(t) > 3)
		t[strlen(t) - 1] = 0;
	pon_valor(c, "SystemRoot", 1, (const u8 *)t, strlen(t) + 1);
	c = sigue_ruta(raiz_lm, "Software\\Microsoft\\DirectX", 1, 0);
	pon_valor(c, "Version", 1, (const u8 *)"4.07.00.0700", 13);
	v = 7;
	pon_valor(c, "InstalledVersion", 3, (const u8 *)"\0\0\0\7\0\0\0\0", 8);
	(void)v;
	sigue_ruta(raiz_lm, "Software\\Classes", 1, 0);
	sigue_ruta(raiz_lm, "System\\CurrentControlSet\\Control", 1, 0);
	c = sigue_ruta(raiz_cu, "Control Panel\\International", 1, 0);
	pon_valor(c, "Locale", 1, (const u8 *)"00000C0A", 9);
	pon_valor(c, "sLanguage", 1, (const u8 *)"ESN", 4);
	sigue_ruta(raiz_cu, "Software", 1, 0);
}

static void carga(void)
{
	if (cargado)
		return;
	cargado = 1;
	raiz_cu = nueva_clave(0, "HKEY_CURRENT_USER", 17);
	raiz_lm = nueva_clave(0, "HKEY_LOCAL_MACHINE", 18);
	raiz_vacia = nueva_clave(0, "", 0);
	raiz_lm->colmena = 1;
	raiz_vacia->colmena = 2;
	de_fabrica();
	lee_archivo(1);
	lee_archivo(0);
	sucia[0] = sucia[1] = 0;
}
static void ensucia(Clave *c) { if (c && c->colmena < 2) sucia[c->colmena] = 1; }

/* =========================================================================
 *  ADVAPI32: lo del registro
 * ========================================================================= */
API(i32) RegOpenKeyExA(HKEY k, const char *s, u32 o, u32 a, HKEY *r)
{
	Clave *c = clave_de(k);
	(void)o; (void)a;
	if (!r)
		return E_PARAM;
	*r = 0;
	if (!c)
		return E_MAL;
	if (!s || !*s) {
		/* la misma (otra vez abierta) */
		if ((u32)k >= R_HKCR && (u32)k <= R_HKCC) {
			*r = k;
			return E_OK;
		}
		*r = abre_h(c);
		return *r ? E_OK : E_PARAM;
	}
	c = sigue_ruta(c, s, 0, 0);
	traza("RegOpenKeyEx(%x, %s) = %s", (u32)k, s, c ? "bien" : "no está");
	if (!c)
		return E_NOHAY;
	*r = abre_h(c);
	return *r ? E_OK : E_PARAM;
}
API(i32) RegOpenKeyA(HKEY k, const char *s, HKEY *r) { return RegOpenKeyExA(k, s, 0, 0, r); }
API(i32) RegOpenCurrentUser(u32 a, HKEY *r) { (void)a; *r = (HKEY)R_HKCU; return E_OK; }
API(i32) RegCreateKeyExA(HKEY k, const char *s, u32 res, char *cl, u32 o, u32 a, void *sg, HKEY *r, u32 *d)
{
	Clave *c = clave_de(k);
	int creada = 0;
	(void)res; (void)cl; (void)o; (void)a; (void)sg;
	if (!r)
		return E_PARAM;
	*r = 0;
	if (!c)
		return E_MAL;
	if (c->colmena == 2)
		return E_DENEGADO;
	c = sigue_ruta(c, s, 1, &creada);
	if (!c)
		return E_PARAM;
	if (creada)
		ensucia(c);
	if (d)
		*d = creada ? 1 : 2;            /* REG_CREATED_NEW_KEY, REG_OPENED_EXISTING_KEY */
	traza("RegCreateKeyEx(%x, %s)%s", (u32)k, s ? s : "", creada ? " nueva" : "");
	*r = abre_h(c);
	return *r ? E_OK : E_PARAM;
}
API(i32) RegCreateKeyA(HKEY k, const char *s, HKEY *r) { return RegCreateKeyExA(k, s, 0, 0, 0, 0, 0, r, 0); }
API(i32) RegCloseKey(HKEY k)
{
	u32 v = (u32)k;
	if (v >= R_HKCR && v <= R_HKCC)
		return E_OK;
	if (v >= 0x00C00000 && v < 0x00C00000 + MAX_ABIERTAS * 4 && !(v & 3) && abiertas[(v - 0x00C00000) / 4]) {
		abiertas[(v - 0x00C00000) / 4] = 0;
		registro_guarda();
		return E_OK;
	}
	return E_MAL;
}
API(i32) RegFlushKey(HKEY k) { (void)k; registro_guarda(); return E_OK; }
API(i32) RegQueryValueExA(HKEY k, const char *n, u32 *r, u32 *t, u8 *d, u32 *l)
{
	Clave *c = clave_de(k);
	Valor *v;
	(void)r;
	if (!c)
		return E_MAL;
	v = busca_valor(c, n);
	if (!v) {
		traza("RegQueryValueEx(%s): no está", n ? n : "");
		return E_NOHAY;
	}
	if (t)
		*t = v->tipo;
	if (!d) {
		if (l)
			*l = v->tam;
		return E_OK;
	}
	if (!l)
		return E_PARAM;
	if (*l < v->tam) {
		*l = v->tam;
		return E_MAS;
	}
	memcpy(d, v->datos, v->tam);
	*l = v->tam;
	return E_OK;
}
API(i32) RegSetValueExA(HKEY k, const char *n, u32 r, u32 t, const u8 *d, u32 l)
{
	Clave *c = clave_de(k);
	(void)r;
	if (!c)
		return E_MAL;
	if (c->colmena == 2)
		return E_DENEGADO;
	if (!d && l)
		return E_PARAM;
	if ((t == 1 || t == 2) && d && !l)
		l = strlen((const char *)d) + 1;
	if (!pon_valor(c, n, t, d, l))
		return 8;
	ensucia(c);
	traza("RegSetValueEx(%s, tipo %u, %u octetos)", n ? n : "", t, l);
	return E_OK;
}
/* el valor sin nombre de una subclave (la API de Windows 3.1) */
API(i32) RegQueryValueA(HKEY k, const char *s, char *d, i32 *l)
{
	Clave *c = clave_de(k);
	Valor *v;
	if (!c)
		return E_MAL;
	c = sigue_ruta(c, s, 0, 0);
	if (!c)
		return E_NOHAY;
	v = busca_valor(c, "");
	if (!v || v->tipo != 1) {
		if (d && l && *l > 0)
			d[0] = 0;
		if (l)
			*l = 1;
		return v ? E_OK : E_OK;
	}
	if (!d) {
		if (l)
			*l = v->tam;
		return E_OK;
	}
	if (!l || *l < (i32)v->tam) {
		if (l)
			*l = v->tam;
		return E_MAS;
	}
	memcpy(d, v->datos, v->tam);
	*l = v->tam;
	return E_OK;
}
API(i32) RegSetValueA(HKEY k, const char *s, u32 t, const char *d, u32 l)
{
	HKEY h;
	i32 r;
	(void)l;
	if (t != 1)
		return E_PARAM;
	r = RegCreateKeyExA(k, s, 0, 0, 0, 0, 0, &h, 0);
	if (r)
		return r;
	r = RegSetValueExA(h, "", 0, 1, (const u8 *)d, strlen(d) + 1);
	RegCloseKey(h);
	return r;
}
API(i32) RegEnumKeyExA(HKEY k, u32 i, char *n, u32 *l, u32 *r, char *cl, u32 *cll, void *t)
{
	Clave *c = clave_de(k), *h;
	(void)r;
	if (!c)
		return E_MAL;
	for (h = c->hijos; h; h = h->sig)
		if (!h->borrada && !i--)
			break;
	if (!h)
		return E_NOMAS;
	if (!l || *l <= strlen(h->nombre)) {
		if (l)
			*l = strlen(h->nombre) + 1;
		return E_MAS;
	}
	strcpy(n, h->nombre);
	*l = strlen(n);
	if (cl && cll && *cll)
		cl[0] = 0;
	if (cll)
		*cll = 0;
	if (t)
		memset(t, 0, 8);
	return E_OK;
}
API(i32) RegEnumKeyA(HKEY k, u32 i, char *n, u32 l) { u32 x = l; return RegEnumKeyExA(k, i, n, &x, 0, 0, 0, 0); }
API(i32) RegEnumValueA(HKEY k, u32 i, char *n, u32 *nl, u32 *r, u32 *t, u8 *d, u32 *dl)
{
	Clave *c = clave_de(k);
	Valor *v;
	(void)r;
	if (!c)
		return E_MAL;
	for (v = c->valores; v; v = v->sig)
		if (!i--)
			break;
	if (!v)
		return E_NOMAS;
	if (!nl || *nl <= strlen(v->nombre)) {
		if (nl)
			*nl = strlen(v->nombre) + 1;
		return E_MAS;
	}
	strcpy(n, v->nombre);
	*nl = strlen(n);
	if (t)
		*t = v->tipo;
	if (d && dl) {
		if (*dl < v->tam) {
			*dl = v->tam;
			return E_MAS;
		}
		memcpy(d, v->datos, v->tam);
	}
	if (dl)
		*dl = v->tam;
	return E_OK;
}
API(i32) RegDeleteValueA(HKEY k, const char *n)
{
	Clave *c = clave_de(k);
	Valor **p;
	if (!c)
		return E_MAL;
	for (p = &c->valores; *p; p = &(*p)->sig)
		if (!stricmp_((*p)->nombre, n ? n : "")) {
			Valor *v = *p;
			*p = v->sig;
			if (v->datos)
				m_soltar(v->datos);
			m_soltar(v);
			ensucia(c);
			return E_OK;
		}
	return E_NOHAY;
}
/* (como en Windows 98: borra la clave con todo lo de dentro) */
API(i32) RegDeleteKeyA(HKEY k, const char *s)
{
	Clave *c = clave_de(k), *b;
	if (!c)
		return E_MAL;
	if (!s || !*s)
		return E_PARAM;
	b = sigue_ruta(c, s, 0, 0);
	if (!b || !b->padre)
		return E_NOHAY;
	b->borrada = 1;                         /* (las HKEY abiertas a ella siguen valiendo) */
	ensucia(b->padre);
	return E_OK;
}
API(i32) RegQueryInfoKeyA(HKEY k, char *cl, u32 *cll, u32 *r, u32 *nsub, u32 *maxsub, u32 *maxcl, u32 *nval, u32 *maxn, u32 *maxd, u32 *seg, void *t)
{
	Clave *c = clave_de(k), *h;
	Valor *v;
	u32 a = 0, b = 0, cc = 0, d = 0, e = 0;
	(void)r;
	if (!c)
		return E_MAL;
	for (h = c->hijos; h; h = h->sig)
		if (!h->borrada) {
			a++;
			if (strlen(h->nombre) > b)
				b = strlen(h->nombre);
		}
	for (v = c->valores; v; v = v->sig) {
		cc++;
		if (strlen(v->nombre) > d)
			d = strlen(v->nombre);
		if (v->tam > e)
			e = v->tam;
	}
	if (cl && cll && *cll)
		cl[0] = 0;
	if (cll)
		*cll = 0;
	if (nsub) *nsub = a;
	if (maxsub) *maxsub = b;
	if (maxcl) *maxcl = 0;
	if (nval) *nval = cc;
	if (maxn) *maxn = d;
	if (maxd) *maxd = e;
	if (seg) *seg = 0;
	if (t) memset(t, 0, 8);
	return E_OK;
}
API(i32) RegNotifyChangeKeyValue(HKEY k, BOOL s, u32 f, HANDLE e, BOOL a) { (void)k; (void)s; (void)f; (void)e; (void)a; return E_OK; }
API(i32) RegConnectRegistryA(const char *m, HKEY k, HKEY *r) { (void)m; (void)k; *r = 0; return 53; }
API(i32) RegSaveKeyA(HKEY k, const char *f, void *s) { (void)k; (void)f; (void)s; return E_DENEGADO; }

/* ---- las de Unicode: los nombres y los textos, de UTF-16 a la 1252 ---- */
#define W(n, b) char b[300]; if (n) de_w(b, n, sizeof b)
API(i32) RegOpenKeyExW(HKEY k, const WCHAR *s, u32 o, u32 a, HKEY *r) { W(s, b); return RegOpenKeyExA(k, s ? b : 0, o, a, r); }
API(i32) RegOpenKeyW(HKEY k, const WCHAR *s, HKEY *r) { W(s, b); return RegOpenKeyExA(k, s ? b : 0, 0, 0, r); }
API(i32) RegCreateKeyExW(HKEY k, const WCHAR *s, u32 res, WCHAR *cl, u32 o, u32 a, void *sg, HKEY *r, u32 *d)
{
	W(s, b);
	(void)cl;
	return RegCreateKeyExA(k, s ? b : 0, res, 0, o, a, sg, r, d);
}
API(i32) RegCreateKeyW(HKEY k, const WCHAR *s, HKEY *r) { W(s, b); return RegCreateKeyExA(k, s ? b : 0, 0, 0, 0, 0, 0, r, 0); }
API(i32) RegDeleteKeyW(HKEY k, const WCHAR *s) { W(s, b); return RegDeleteKeyA(k, s ? b : 0); }
API(i32) RegDeleteValueW(HKEY k, const WCHAR *s) { W(s, b); return RegDeleteValueA(k, s ? b : 0); }
static int es_texto(u32 t) { return t == 1 || t == 2 || t == 7; }
API(i32) RegQueryValueExW(HKEY k, const WCHAR *n, u32 *r, u32 *t, u8 *d, u32 *l)
{
	u32 tipo, tam = 0;
	i32 e;
	u8 *tmp;
	W(n, b);
	e = RegQueryValueExA(k, n ? b : 0, r, &tipo, 0, &tam);
	if (e)
		return e;
	if (t)
		*t = tipo;
	if (!es_texto(tipo))
		return RegQueryValueExA(k, n ? b : 0, r, t, d, l);
	if (!d) {
		if (l)
			*l = tam * 2;
		return E_OK;
	}
	if (!l)
		return E_PARAM;
	if (*l < tam * 2) {
		*l = tam * 2;
		return E_MAS;
	}
	tmp = m_pedir(tam + 1);
	if (!tmp)
		return 8;
	RegQueryValueExA(k, n ? b : 0, r, 0, tmp, &tam);
	{
		u32 j;
		for (j = 0; j < tam; j++)
			((WCHAR *)d)[j] = a_uni(tmp[j]);
	}
	m_soltar(tmp);
	*l = tam * 2;
	return E_OK;
}
API(i32) RegSetValueExW(HKEY k, const WCHAR *n, u32 r, u32 t, const u8 *d, u32 l)
{
	W(n, b);
	if (es_texto(t) && d) {
		u32 j, c = l / 2;
		u8 *tmp = m_pedir(c + 2);
		i32 e;
		if (!tmp)
			return 8;
		for (j = 0; j < c; j++)
			tmp[j] = de_uni(((const WCHAR *)d)[j]);
		tmp[c] = 0;
		e = RegSetValueExA(k, n ? b : 0, r, t, tmp, c);
		m_soltar(tmp);
		return e;
	}
	return RegSetValueExA(k, n ? b : 0, r, t, d, l);
}
API(i32) RegEnumKeyExW(HKEY k, u32 i, WCHAR *n, u32 *l, u32 *r, WCHAR *cl, u32 *cll, void *t)
{
	char b[260];
	u32 x = sizeof b;
	i32 e = RegEnumKeyExA(k, i, b, &x, r, 0, cll, t);
	(void)cl;
	if (e)
		return e;
	if (!l || *l <= x) {
		if (l)
			*l = x + 1;
		return E_MAS;
	}
	a_w(n, b, *l);
	*l = x;
	return E_OK;
}
API(i32) RegEnumKeyW(HKEY k, u32 i, WCHAR *n, u32 l) { u32 x = l; return RegEnumKeyExW(k, i, n, &x, 0, 0, 0, 0); }
API(i32) RegEnumValueW(HKEY k, u32 i, WCHAR *n, u32 *nl, u32 *r, u32 *t, u8 *d, u32 *dl)
{
	char b[260];
	u32 x = sizeof b, tipo = 0;
	i32 e = RegEnumValueA(k, i, b, &x, r, &tipo, 0, 0);
	if (e)
		return e;
	if (!nl || *nl <= x) {
		if (nl)
			*nl = x + 1;
		return E_MAS;
	}
	a_w(n, b, *nl);
	*nl = x;
	if (t)
		*t = tipo;
	if (d || dl) {
		WCHAR wn[260];
		a_w(wn, b, 260);
		return RegQueryValueExW(k, wn, 0, 0, d, dl);
	}
	return E_OK;
}
API(i32) RegQueryInfoKeyW(HKEY k, WCHAR *cl, u32 *cll, u32 *r, u32 *a, u32 *b, u32 *c, u32 *d, u32 *e, u32 *f, u32 *g, void *t)
{
	(void)cl;
	return RegQueryInfoKeyA(k, 0, cll, r, a, b, c, d, e, f, g, t);
}
