/* enviar.c - (COMPAT) el informe que se manda a Hispa-DOS, con permiso cada
 * vez. Lo que va es SOLO esto (y es exactamente lo que enseña el cuadro):
 *   HISPA-COMPAT 1
 *   hispa-pentium=0.8.7
 *   win32=1.2
 *   asistente=1.0
 *   archivos=2
 *   archivo=<nombre>\t<tamaño>\t<sha256 o ->\t<clase>\t<faltan>\t<si-hace-falta: si | instalador...>
 *   falta=<archivo>\t<DLL>\t<función>
 *   faltadll=<archivo>\t<DLL>\t<funciones>
 *   fin
 * El nombre de cada .EXE y .DLL va sin su carpeta (nunca una ruta: ahí
 * podría ir el nombre de quien lo usa) y solo con letras, cifras y . _ -
 * (lo demás, «_»). Nada de lo de dentro de los archivos, solo sus sumas.
 * Por HTTPS (TLS.BIN) al receptor (tools/compat-subida/informe.php), con el
 * testigo del equipo: 32 octetos al azar, hechos aquí la primera vez y
 * guardados en la carpeta del usuario (COMPAT\TESTIGO.TXT); el servidor
 * solo guarda su SHA-256 y con él lleva los topes del día.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "plat.h"
#include "http.h"
#include "texto.h"

static void limpio(const char *s, char *d, u32 cabe)
{
	u32 j = 0;

	for (; *s && j + 1 < cabe && j < 63; s++) {
		char c = *s;

		if ((u8)c >= 0x80) {
			/* (una letra de fuera del ASCII: una sola «_» por letra) */
			while ((u8)s[1] >= 0x80 && ((u8)s[1] & 0xC0) == 0x80)
				s++;
			c = '_';
		} else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
			c = '_';
		d[j++] = c;
	}
	d[j] = 0;
	if (!j)
		copia(d, "_", cabe);
}

static const char *clase(int c)
{
	switch (c) {
	case A_VENTANAS: return "ventanas";
	case A_CONSOLA: return "consola";
	case A_DLL: return "dll";
	case A_NO_PE: return "no-pe";
	case A_NO_ESTA: return "no-esta";
	}
	return "programa";
}

static const char *importancia(int si)
{
	switch (si) {
	case SI_INSTALADOR: return "instalador";
	case SI_SERVIDOR: return "servidor";
	case SI_GLIDE: return "glide";
	case SI_ACTUALIZA: return "actualizador";
	case SI_FALLOS: return "fallos";
	case SI_STEAM: return "steam";
	case SI_EDITOR: return "editor";
	}
	return "si";
}

char *env_texto(const c_informe *i, const char *win32, const char *hispa)
{
	bufer b;
	int k, n = 0;
	char x[80], y[80], z[100];

	memset(&b, 0, sizeof b);
	for (k = 0; k < i->n_a; k++)
		if (i->a[k].clase != A_NO_ESTA)
			n++;
	limpio(hispa, x, sizeof x);
	limpio(win32, y, sizeof y);
	buf_fmt(&b, "HISPA-COMPAT 1\nhispa-pentium=%s\nwin32=%s\nasistente=%s\narchivos=%d\n", x, y, COMPAT_VERSION, n);
	for (k = 0; k < i->n_a; k++) {
		const c_archivo *a = &i->a[k];

		if (a->clase == A_NO_ESTA)
			continue;
		limpio(a->nombre, x, sizeof x);
		buf_fmt(&b, "archivo=%s\t%u\t%s\t%s\t%u\t%s\n", x, a->tam, a->sha[0] ? a->sha : "-", clase(a->clase), a->n_faltan,
			importancia(a->sin_importancia));
	}
	for (k = 0; k < i->n_f; k++) {
		const c_falta *f = &i->f[k];

		if (i->a[f->archivo].clase == A_NO_ESTA)
			continue;
		limpio(i->a[f->archivo].nombre, x, sizeof x);
		limpio(f->dll, y, sizeof y);
		if (f->fn[0]) {
			/* (la función: tal cual, sin lo que no sea de nombre; «#12» por número) */
			u32 j = 0;
			const char *p;

			for (p = f->fn; *p && j + 1 < sizeof z; p++)
				z[j++] = ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '@' ||
					  *p == '#' || *p == '?' || *p == '$') ? *p : '_';
			z[j] = 0;
			buf_fmt(&b, "falta=%s\t%s\t%s\n", x, y, z);
		} else
			buf_fmt(&b, "faltadll=%s\t%s\t%u\n", x, y, f->n);
	}
	buf_txt(&b, "fin\n");
	buf_c(&b, 0);
	if (b.mal) {
		buf_suelta(&b);
		return NULL;
	}
	return (char *)b.d;
}

/* ---- el testigo del equipo ---- */
/* 32 octetos al azar del núcleo (la llamada 210), en hexadecimal, en la
 * carpeta del usuario (COMPAT\TESTIGO.TXT: solo la lee él); el mismo para
 * todos los informes de ese equipo y ese usuario */
static int testigo(char t[65])
{
	static const char h[] = "0123456789abcdef";
	char c[RUTA_MAX], r[RUTA_MAX], *x;
	u8 a[32];
	int k;

	if (hist_carpeta(c, sizeof c))
		return -1;
	une_ruta(r, sizeof r, c, "TESTIGO.TXT");
	x = lee_texto(r, NULL, 200);
	if (x) {
		int bien = largo(x) >= 64;

		for (k = 0; k < 64 && bien; k++)
			bien = (x[k] >= '0' && x[k] <= '9') || (x[k] >= 'a' && x[k] <= 'f');
		if (bien) {
			memcpy(t, x, 64);
			t[64] = 0;
			so_suelta(x);
			return 0;
		}
		so_suelta(x);
	}
	if (plat_azar(a, 32))
		return -1;
	for (k = 0; k < 32; k++) {
		t[2 * k] = h[a[k] >> 4];
		t[2 * k + 1] = h[a[k] & 15];
	}
	t[64] = 0;
	return escribe_texto(r, t, 64);
}

/* los servidores: de \HISPA\COMPAT\COMPAT.INI («servidor=...», uno o dos) */
static int servidores(char s[2][200])
{
	char c[RUTA_MAX], r[RUTA_MAX], *t;
	const char *p;
	int n = 0;

	plat_carpeta_compat(c, sizeof c);
	une_ruta(r, sizeof r, c, "COMPAT.INI");
	t = lee_texto(r, NULL, 8192);
	if (t) {
		for (p = t; *p && n < 2;) {
			if (empieza(p, "servidor=")) {
				u32 j = 0;

				p += 9;
				while (*p && *p != '\r' && *p != '\n' && j + 1 < 200)
					s[n][j++] = *p++;
				s[n][j] = 0;
				if (empieza(s[n], "https://") || empieza(s[n], "http://127.0.0.1"))
					n++;
			}
			while (*p && *p != '\n')
				p++;
			while (*p == '\n' || *p == '\r')
				p++;
		}
		so_suelta(t);
	}
	if (!n) {
		copia(s[0], "https://actualiza.hispados.es/compat/informe.php", 200);
		copia(s[1], "https://hispados.es/compat/informe.php", 200);
		n = 2;
	}
	return n;
}

typedef struct { char d[512]; u32 n; } resp;
static int a_resp(void *u, const u8 *d, u32 n, u32 total)
{
	resp *r = u;
	u32 k = n < sizeof r->d - 1 - r->n ? n : sizeof r->d - 1 - r->n;

	(void)total;
	memcpy(r->d + r->n, d, k);
	r->n += k;
	r->d[r->n] = 0;
	return 0;
}

int env_manda(const char *texto, char *por_que, u32 cabe)
{
	static const char lim[] = "----HispaCompat5e2c9a71d0";
	char t[65], s[2][200];
	bufer b;
	int ns, k, est, peor = 2;
	u32 n = largo(texto);

	if (n > 240 * 1024) {
		copia(por_que, "el informe es demasiado grande (más de 240 KB)", cabe);
		return 1;
	}
	if (testigo(t)) {
		copia(por_que, "no se puede hacer el testigo del equipo", cabe);
		return 1;
	}
	memset(&b, 0, sizeof b);
	buf_fmt(&b, "--%s\r\nContent-Disposition: form-data; name=\"testigo\"\r\n\r\n%s\r\n", lim, t);
	buf_fmt(&b, "--%s\r\nContent-Disposition: form-data; name=\"protocolo\"\r\n\r\n1\r\n", lim);
	buf_fmt(&b, "--%s\r\nContent-Disposition: form-data; name=\"informe\"; filename=\"INFORME.TXT\"\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n", lim);
	buf_pon(&b, texto, n);
	buf_fmt(&b, "\r\n--%s--\r\n", lim);
	if (b.mal) {
		buf_suelta(&b);
		copia(por_que, "no hay memoria", cabe);
		return 1;
	}
	if (!plat_hay_red(20)) {
		buf_suelta(&b);
		copia(por_que, "no hay red: enchufe el cable o conecte el Wi-Fi (Inicio > Configuración > Red) y pruebe otra vez", cabe);
		return 2;
	}
	ns = servidores(s);
	for (k = 0; k < ns; k++) {
		resp r;
		char tipo[80], e[200];

		memset(&r, 0, sizeof r);
		fmt(tipo, sizeof tipo, "multipart/form-data; boundary=%s", lim);
		if (http_pide(s[k], "POST", tipo, b.d, b.n, a_resp, &r, &est, NULL, 0, e, sizeof e)) {
			fmt(por_que, cabe, "%s", e);
			continue;
		}
		{
			u32 j = largo(r.d);

			while (j && (r.d[j - 1] == '\n' || r.d[j - 1] == '\r'))
				r.d[--j] = 0;
		}
		if (est == 200 && empieza(r.d, "OK")) {
			buf_suelta(&b);
			fmt(por_que, cabe, "%s", r.d);
			return 0;
		}
		if (empieza(r.d, "ERROR")) {
			fmt(por_que, cabe, "el servidor dice: %s", r.d + 6);
			peor = est >= 500 ? 2 : 1;
			if (peor == 1)
				break;
		} else
			fmt(por_que, cabe, "respuesta rara del servidor (HTTP %d)", est);
	}
	buf_suelta(&b);
	return peor;
}
