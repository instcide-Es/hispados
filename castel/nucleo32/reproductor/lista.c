/* lista.c - el Reproductor: la lista de reproducción (ver lista.h). Las
 * listas que se abren pueden venir de cualquier sitio (de Windows, de
 * Linux): sus rutas se miran antes de usarlas, con «\» o «/», relativas a la
 * carpeta de la lista o enteras, en UTF-8 o en Latin-1.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "lista.h"
#include "util.h"

void lista_nueva(lista *l)
{
	memset(l, 0, sizeof *l);
	l->actual = -1;
}

void lista_vacia(lista *l)
{
	int i;

	so_suelta(l->c);
	so_suelta(l->orden);
	for (i = 0; i < l->n_pendientes; i++)
		so_suelta(l->pendientes[i]);
	i = l->cambios;
	{
		int al = l->aleatorio, re = l->repetir;

		lista_nueva(l);
		l->aleatorio = al;
		l->repetir = re;
	}
	l->cambios = i + 1;
}

void origen_de_ruta(origen *o, const char *ruta)
{
	memset(o, 0, sizeof *o);
	copia(o->ruta, sizeof o->ruta, ruta);
}

/* el nombre del archivo, sin la extensión ni los «_» */
static void nombre_de_archivo(const char *ruta, char *d, int cabe)
{
	char u[MAX_RUTA];
	int n, i;

	so_a_utf8(base_de(ruta), u, sizeof u);
	copia(d, cabe, u);
	n = (int)strlen(d);
	for (i = n - 1; i > 0; i--)
		if (d[i] == '.') {
			d[i] = 0;
			break;
		}
	for (i = 0; d[i]; i++)
		if (d[i] == '_')
			d[i] = ' ';
}

int lista_anade(lista *l, const origen *o, const char *titulo)
{
	cancion *c;

	if (l->n >= MAX_CANCIONES)
		return -1;
	if (l->n >= l->cabe) {
		int cabe = l->cabe ? l->cabe * 2 : 64;
		cancion *nc = so_repide(l->c, (size_t)cabe * sizeof(cancion));

		if (!nc)
			return -1;
		l->c = nc;
		l->cabe = cabe;
	}
	c = &l->c[l->n];
	memset(c, 0, sizeof *c);
	c->o = *o;
	if (titulo && titulo[0])
		copia(c->titulo, sizeof c->titulo, titulo);
	else
		nombre_de_archivo(o->ruta, c->titulo, sizeof c->titulo);
	l->n++;
	l->cambios++;
	if (l->aleatorio) {
		so_suelta(l->orden);
		l->orden = NULL;	/* (se baraja otra vez con la nueva) */
	}
	return l->n - 1;
}

static int es_lista(const char *e)
{
	return !strcmp(e, "M3U") || !strcmp(e, "M3U8") || !strcmp(e, "PLS");
}

int lista_anade_archivo(lista *l, const char *ruta)
{
	const char *e = dec_ext_de(ruta);
	origen o;
	int carpeta = 0;

	if (so_existe(ruta, &carpeta) && carpeta) {
		lista_anade_carpeta(l, ruta);
		return 0;
	}
	if (es_lista(e))
		return lista_lee(l, ruta) < 0 ? -1 : 0;
	if (!strcmp(e, "CUE"))
		return lista_anade_cue(l, ruta) < 0 ? -1 : 0;
	origen_de_ruta(&o, ruta);
	return lista_anade(l, &o, NULL) < 0 ? -1 : 0;
}

void lista_anade_carpeta(lista *l, const char *ruta)
{
	char *p;
	int n = (int)strlen(ruta);

	if (l->n_pendientes >= MAX_PENDIENTES)
		return;
	p = so_pide((size_t)n + 2);
	if (!p)
		return;
	memcpy(p, ruta, (size_t)n + 1);
	l->pendientes[l->n_pendientes++] = p;
}

/* (las de una carpeta, por orden de nombre: la pista 01 antes que la 02) */
typedef struct { char nombre[MAX_RUTA]; char visto[MAX_RUTA]; int carpeta; } hallado;

static void ordena_hallados(hallado *h, int n)
{
	int i, j;

	for (i = 1; i < n; i++) {
		hallado t = h[i];

		for (j = i; j > 0 && compara_llano(h[j - 1].visto, t.visto) > 0; j--)
			h[j] = h[j - 1];
		h[j] = t;
	}
}

int lista_paso(lista *l)
{
	char ruta[MAX_RUTA], sep = so_separador();
	void *c;
	so_entrada e;
	hallado *h;
	int n = 0, cabe = 256, i, hay_cue = 0;

	if (!l->n_pendientes)
		return 0;
	/* la primera de la cola (así va en orden) */
	copia(ruta, sizeof ruta, l->pendientes[0]);
	so_suelta(l->pendientes[0]);
	memmove(l->pendientes, l->pendientes + 1, (size_t)(l->n_pendientes - 1) * sizeof(char *));
	l->n_pendientes--;
	c = so_abre_carpeta(ruta);
	if (!c)
		return l->n_pendientes > 0;
	h = so_pide((size_t)cabe * sizeof(hallado));
	if (!h) {
		so_cierra_carpeta(c);
		return l->n_pendientes > 0;
	}
	while (so_siguiente(c, &e)) {
		if (e.nombre[0] == '.')
			continue;
		if (!e.carpeta && !dec_ext_vale(e.nombre) && !es_lista(dec_ext_de(e.nombre)))
			continue;
		if (!e.carpeta && !strcmp(dec_ext_de(e.nombre), "CUE"))
			hay_cue = 1;
		if (n >= cabe) {
			hallado *nh = so_repide(h, (size_t)cabe * 2 * sizeof(hallado));

			if (!nh)
				break;
			h = nh;
			cabe *= 2;
		}
		copia(h[n].nombre, MAX_RUTA, e.nombre);
		copia(h[n].visto, MAX_RUTA, e.visto);
		h[n].carpeta = e.carpeta;
		n++;
	}
	so_cierra_carpeta(c);
	ordena_hallados(h, n);
	/* primero las hojas .CUE (sus pistas); luego lo demás, sin los
	 * archivos que ya van por una hoja (el .BIN, el FLAC entero) */
	{
		int primera = l->n, pasada;

		for (pasada = 0; pasada < 2; pasada++)
			for (i = 0; i < n; i++) {
				char r[MAX_RUTA];
				const char *ex = dec_ext_de(h[i].nombre);
				int k = (int)strlen(ruta), j, repetido = 0;

				copia(r, sizeof r, ruta);
				if (k > 0 && r[k - 1] != sep)
					r[k] = sep, r[k + 1] = 0;
				anade(r, sizeof r, h[i].nombre);
				if (pasada == 0) {
					if (!h[i].carpeta && !strcmp(ex, "CUE")) {
						lista_anade_cue(l, r);
						l->anadidas++;
					}
					continue;
				}
				if (h[i].carpeta) {
					lista_anade_carpeta(l, r);
					continue;
				}
				/* (las listas que haya en la carpeta no se abren: serían repetidas) */
				if (es_lista(ex) || !strcmp(ex, "CUE"))
					continue;
				for (j = primera; j < l->n && !repetido; j++)
					if (igual_sin_caja(base_de(l->c[j].o.ruta), h[i].nombre))
						repetido = 1;
				if (repetido)
					continue;
				{
					origen o;
					char t[128], *p;

					origen_de_ruta(&o, r);
					copia(t, sizeof t, h[i].visto);
					p = strrchr(t, '.');
					if (p && p != t)
						*p = 0;
					for (p = t; *p; p++)
						if (*p == '_')
							*p = ' ';
					lista_anade(l, &o, t);
					l->anadidas++;
				}
			}
		(void)hay_cue;
	}
	so_suelta(h);
	return l->n_pendientes > 0;
}

/* ============================================================ las rutas de una lista */
/* la ruta de un renglón de una lista que está en «carpeta» (con su separador) */
static void ruta_de_renglon(const char *carpeta, const char *r, char *d, int cabe)
{
	char u[MAX_RUTA * 2], s[MAX_RUTA];
	int i;
	char sep = so_separador();

	copia(u, sizeof u, r);
	if (!strncmp(u, "file://", 7)) {
		/* file:///C:/MUSICA/... o file:///home/... (con %20) */
		const char *p = u + 7;
		int k = 0;
		char v[MAX_RUTA * 2];

		if (*p == '/' && p[2] == ':')
			p++;
		while (*p && k < (int)sizeof v - 1) {
			if (*p == '%' && p[1] && p[2]) {
				int a = p[1] <= '9' ? p[1] - '0' : (p[1] | 32) - 'a' + 10, b = p[2] <= '9' ? p[2] - '0' : (p[2] | 32) - 'a' + 10;

				v[k++] = (char)(a * 16 + b);
				p += 3;
			} else
				v[k++] = *p++;
		}
		v[k] = 0;
		copia(u, sizeof u, v);
	}
	for (i = 0; u[i]; i++)
		if (u[i] == '\\' || u[i] == '/')
			u[i] = sep;
	so_de_utf8(u, s, sizeof s);
	if ((s[0] && s[1] == ':') || s[0] == sep)
		copia(d, cabe, s);
	else {
		copia(d, cabe, carpeta);
		anade(d, cabe, s);
	}
}

static int es_url(const char *t)
{
	return empieza_sin_caja(t, "http://") || empieza_sin_caja(t, "https://");
}

/* un renglón de una lista, a UTF-8 (si no lo era: Latin-1) */
static void renglon_utf8(const char *r, char *d, int cabe)
{
	if (es_utf8((const u8 *)r, (int)strlen(r)))
		copia(d, cabe, r);
	else
		latin1_a_utf8((const u8 *)r, (int)strlen(r), d, cabe);
}

int lista_lee(lista *l, const char *ruta)
{
	u32 n, i = 0;
	u8 *t = so_lee_todo(ruta, &n, 4u * 1024 * 1024);
	char carpeta[MAX_RUTA], titulo[256] = "", renglon[1024], u[1024];
	int dur = 0;
	int pls = !strcmp(dec_ext_de(ruta), "PLS"), antes = l->n;

	if (!t)
		return -1;
	carpeta_de(ruta, carpeta, sizeof carpeta);
	if (n >= 3 && t[0] == 0xEF && t[1] == 0xBB && t[2] == 0xBF)
		i = 3;
	while (i < n) {
		u32 k = 0;

		while (i < n && t[i] != '\n' && t[i] != '\r') {
			if (k < sizeof renglon - 1)
				renglon[k++] = (char)t[i];
			i++;
		}
		while (i < n && (t[i] == '\n' || t[i] == '\r'))
			i++;
		renglon[k] = 0;
		while (k > 0 && (renglon[k - 1] == ' ' || renglon[k - 1] == '\t'))
			renglon[--k] = 0;
		if (!renglon[0])
			continue;
		renglon_utf8(renglon, u, sizeof u);
		if (pls) {
			/* FileN=..., TitleN=... (el título va detrás de su archivo) */
			if (empieza_sin_caja(u, "File")) {
				char *p = strchr(u, '='), r[MAX_RUTA];
				origen o;

				if (!p || es_url(p + 1))
					continue;
				ruta_de_renglon(carpeta, p + 1, r, sizeof r);
				origen_de_ruta(&o, r);
				lista_anade(l, &o, NULL);
			} else if (empieza_sin_caja(u, "Title") && l->n > antes) {
				char *p = strchr(u, '=');

				if (p && p[1])
					copia(l->c[l->n - 1].titulo, sizeof l->c[0].titulo, p + 1);
			} else if (empieza_sin_caja(u, "Length") && l->n > antes) {
				char *p = strchr(u, '=');

				if (p && a_entero(p + 1) > 0)
					l->c[l->n - 1].duracion = (u32)a_entero(p + 1);
			}
			continue;
		}
		if (u[0] == '#') {
			/* #EXTINF:segundos,Artista - Título */
			if (empieza_sin_caja(u, "#EXTINF:")) {
				char *p = strchr(u, ',');

				copia(titulo, sizeof titulo, p ? p + 1 : "");
				dur = a_entero(u + 8);
			}
			continue;
		}
		if (es_url(u)) {
			titulo[0] = 0;
			continue;	/* (las radios, en la fase siguiente) */
		}
		{
			char r[MAX_RUTA];
			origen o;
			const char *e;
			int k2;

			ruta_de_renglon(carpeta, u, r, sizeof r);
			e = dec_ext_de(r);
			if (!strcmp(e, "CUE")) {
				lista_anade_cue(l, r);
				titulo[0] = 0;
				continue;
			}
			origen_de_ruta(&o, r);
			k2 = lista_anade(l, &o, titulo);
			if (k2 >= 0 && dur > 0)
				l->c[k2].duracion = (u32)dur;
			dur = 0;
			if (k2 >= 0 && titulo[0]) {
				/* «Artista - Título»: se separa */
				char *g = strstr(l->c[k2].titulo, " - ");

				if (g) {
					*g = 0;
					copia(l->c[k2].artista, sizeof l->c[0].artista, l->c[k2].titulo);
					memmove(l->c[k2].titulo, g + 3, strlen(g + 3) + 1);
				}
			}
			titulo[0] = 0;
		}
	}
	so_suelta(t);
	return l->n - antes;
}

/* la ruta de la canción, para escribirla en la lista (relativa a su carpeta
 * si cae dentro; en UTF-8) */
static void ruta_para_lista(const char *carpeta, const char *ruta, char *d, int cabe)
{
	int n = (int)strlen(carpeta);
	char s[MAX_RUTA];

	if (n && !strncmp(ruta, carpeta, (size_t)n))
		copia(s, sizeof s, ruta + n);
	else
		copia(s, sizeof s, ruta);
	so_a_utf8(s, d, cabe);
}

int lista_guarda(lista *l, const char *ruta)
{
	so_archivo *a = so_crea(ruta);
	char carpeta[MAX_RUTA], t[MAX_RUTA * 3], r[MAX_RUTA * 2], nom[300];
	int pls = !strcmp(dec_ext_de(ruta), "PLS"), i, k = 0;

	if (!a)
		return -1;
	carpeta_de(ruta, carpeta, sizeof carpeta);
	if (pls) {
		fmt(t, sizeof t, "[playlist]\r\n");
		so_escribe(a, t, (u32)strlen(t));
	} else {
		fmt(t, sizeof t, "#EXTM3U\r\n");
		so_escribe(a, t, (u32)strlen(t));
	}
	for (i = 0; i < l->n; i++) {
		cancion *c = &l->c[i];

		if (c->o.lector || c->o.largo || c->o.tramo_desde || c->o.tramo_hasta)
			continue;	/* (las pistas de un CD o de una hoja .CUE: no van) */
		ruta_para_lista(carpeta, c->o.ruta, r, sizeof r);
		if (c->artista[0])
			fmt(nom, sizeof nom, "%s - %s", c->artista, c->titulo);
		else
			copia(nom, sizeof nom, c->titulo);
		k++;
		if (pls)
			fmt(t, sizeof t, "File%d=%s\r\nTitle%d=%s\r\nLength%d=%d\r\n", k, r, k, nom, k, c->duracion ? (int)c->duracion : -1);
		else
			fmt(t, sizeof t, "#EXTINF:%d,%s\r\n%s\r\n", c->duracion ? (int)c->duracion : -1, nom, r);
		if (so_escribe(a, t, (u32)strlen(t))) {
			so_cierra(a);
			return -1;
		}
	}
	if (pls) {
		fmt(t, sizeof t, "NumberOfEntries=%d\r\nVersion=2\r\n", k);
		so_escribe(a, t, (u32)strlen(t));
	}
	so_cierra(a);
	return 0;
}

/* ============================================================ las hojas .CUE */
static u32 msf(const char *t)
{
	/* mm:ss:ff -> 75 avos de segundo */
	int m = a_entero(t), s = 0, f = 0;
	const char *p = strchr(t, ':');

	if (p) {
		s = a_entero(p + 1);
		p = strchr(p + 1, ':');
		if (p)
			f = a_entero(p + 1);
	}
	if (m < 0 || s < 0 || f < 0)
		return 0;
	return (u32)((m * 60 + s) * 75 + f);
}

/* lo de entre comillas (o la palabra) tras la orden */
static void argumento(const char *t, char *d, int cabe)
{
	int k = 0;

	while (*t == ' ' || *t == '\t')
		t++;
	if (*t == '"') {
		t++;
		while (*t && *t != '"' && k < cabe - 1)
			d[k++] = *t++;
	} else
		while (*t && *t != ' ' && *t != '\t' && k < cabe - 1)
			d[k++] = *t++;
	d[k] = 0;
}

int lista_anade_cue(lista *l, const char *ruta)
{
	u32 n, i = 0;
	u8 *t = so_lee_todo(ruta, &n, 256u * 1024);
	char carpeta[MAX_RUTA], archivo[MAX_RUTA] = "", renglon[600], u[600], arg[400];
	char artista_disco[96] = "", album[96] = "";
	int binario = 0, motorola = 0, antes = l->n, primera_del_archivo = l->n;
	struct { char titulo[128], artista[96]; int audio; u32 inicio; int hay_inicio; } p = { "", "", 0, 0, 0 };
	u8 bin[128];

	if (!t)
		return -1;
	carpeta_de(ruta, carpeta, sizeof carpeta);
	if (n >= 3 && t[0] == 0xEF && t[1] == 0xBB && t[2] == 0xBF)
		i = 3;

/* cierra la pista que se estaba leyendo */
#define CIERRA_PISTA() do { \
		if (p.audio && p.hay_inicio && archivo[0]) { \
			origen o; int k; \
			origen_de_ruta(&o, archivo); \
			if (binario) { o.desde = (u64)p.inicio * 2352; if (motorola) o.lba = 0x80000000u; } \
			else o.tramo_desde = p.inicio; \
			k = lista_anade(l, &o, p.titulo[0] ? p.titulo : NULL); \
			if (k >= 0 && k - antes < 128) bin[k - antes] = (u8)binario; \
			if (k >= 0) { copia(l->c[k].artista, sizeof l->c[0].artista, p.artista[0] ? p.artista : artista_disco); \
				copia(l->c[k].album, sizeof l->c[0].album, album); \
				if (p.titulo[0]) l->c[k].leida = 1; } \
		} \
		p.audio = 0; p.hay_inicio = 0; p.titulo[0] = 0; p.artista[0] = 0; \
	} while (0)

	while (i < n) {
		u32 k = 0;
		const char *r;

		while (i < n && t[i] != '\n' && t[i] != '\r') {
			if (k < sizeof renglon - 1)
				renglon[k++] = (char)t[i];
			i++;
		}
		while (i < n && (t[i] == '\n' || t[i] == '\r'))
			i++;
		renglon[k] = 0;
		renglon_utf8(renglon, u, sizeof u);
		r = u;
		while (*r == ' ' || *r == '\t')
			r++;
		if (empieza_sin_caja(r, "FILE ")) {
			CIERRA_PISTA();
			/* (las pistas del archivo de antes, hasta donde empieza la siguiente) */
			argumento(r + 5, arg, sizeof arg);
			ruta_de_renglon(carpeta, arg, archivo, sizeof archivo);
			{
				const char *tipo = r + 5;

				while (*tipo == ' ')
					tipo++;
				if (*tipo == '"') {
					tipo = strchr(tipo + 1, '"');
					tipo = tipo ? tipo + 1 : "";
				} else
					while (*tipo && *tipo != ' ')
						tipo++;
				while (*tipo == ' ')
					tipo++;
				binario = empieza_sin_caja(tipo, "BINARY") || empieza_sin_caja(tipo, "MOTOROLA");
				motorola = empieza_sin_caja(tipo, "MOTOROLA");
			}
			primera_del_archivo = l->n;
		} else if (empieza_sin_caja(r, "TRACK ")) {
			CIERRA_PISTA();
			p.audio = busca_sin_caja(r, "AUDIO") != NULL;
		} else if (empieza_sin_caja(r, "TITLE ")) {
			argumento(r + 6, arg, sizeof arg);
			if (p.audio)
				copia(p.titulo, sizeof p.titulo, arg);
			else if (l->n == antes && !album[0])
				copia(album, sizeof album, arg);
		} else if (empieza_sin_caja(r, "PERFORMER ")) {
			argumento(r + 10, arg, sizeof arg);
			if (p.audio)
				copia(p.artista, sizeof p.artista, arg);
			else if (l->n == antes)
				copia(artista_disco, sizeof artista_disco, arg);
		} else if (empieza_sin_caja(r, "INDEX ")) {
			int num = a_entero(r + 6);
			const char *q = r + 6;

			while (*q == ' ')
				q++;
			while (*q && *q != ' ')
				q++;
			if (num == 1 && p.audio) {
				u32 ini = msf(q);

				p.inicio = ini;
				p.hay_inicio = 1;
				/* la pista de antes (del mismo archivo) acaba aquí */
				if (l->n > primera_del_archivo) {
					cancion *c = &l->c[l->n - 1];

					if (binario) {
						if (c->o.desde < (u64)ini * 2352)
							c->o.largo = (u64)ini * 2352 - c->o.desde;
					} else if (c->o.tramo_desde < ini)
						c->o.tramo_hasta = ini;
				}
			}
		}
	}
	CIERRA_PISTA();
#undef CIERRA_PISTA
	so_suelta(t);
	/* las últimas de cada archivo .BIN: hasta el final del archivo */
	for (i = (u32)antes; i < (u32)l->n; i++) {
		cancion *c = &l->c[i];

		if (i - (u32)antes < 128 && bin[i - antes] && !c->o.largo) {
			so_archivo *a = so_abre(c->o.ruta);

			if (a) {
				u64 tam = so_largo(a);

				so_cierra(a);
				if (tam > c->o.desde)
					c->o.largo = tam - c->o.desde;
			}
			if (!c->o.largo)
				c->fallo = 1;
		}
		if (c->o.largo)
			c->duracion = (u32)(c->o.largo / (2352 * 75));
		else if (c->o.tramo_hasta > c->o.tramo_desde)
			c->duracion = (c->o.tramo_hasta - c->o.tramo_desde) / 75;
	}
	return l->n - antes;
}

/* ============================================================ un CD */
int lista_anade_cd(lista *l, char letra, const so_toc *t)
{
	int i, antes = l->n;

	for (i = 0; i < t->pistas; i++) {
		origen o;
		char titulo[64];
		int k;

		if (t->pista[i].control & 4)
			continue;	/* (una de datos) */
		memset(&o, 0, sizeof o);
		o.lector = letra;
		o.lba = t->pista[i].lba;
		o.sectores = t->pista[i + 1].lba > o.lba ? t->pista[i + 1].lba - o.lba : 0;
		/* (si la siguiente es de datos, lo de entre medias no es sonido: 11.400 sectores) */
		if (i + 1 < t->pistas && (t->pista[i + 1].control & 4) && o.sectores > 11400)
			o.sectores -= 11400;
		fmt(o.ruta, sizeof o.ruta, "%c:", letra);
		fmt(titulo, sizeof titulo, "Pista %d", t->pista[i].numero);
		k = lista_anade(l, &o, titulo);
		if (k >= 0) {
			l->c[k].duracion = o.sectores / 75;
			copia(l->c[k].album, sizeof l->c[0].album, "CD de música");
			l->c[k].leida = 1;
		}
	}
	return l->n - antes;
}

/* ============================================================ quitar, mover, ordenar */
void lista_quita(lista *l, int i)
{
	if (i < 0 || i >= l->n)
		return;
	memmove(&l->c[i], &l->c[i + 1], (size_t)(l->n - i - 1) * sizeof(cancion));
	l->n--;
	if (l->actual == i)
		l->actual = -1;
	else if (l->actual > i)
		l->actual--;
	so_suelta(l->orden);
	l->orden = NULL;
	l->cambios++;
}

void lista_quita_marcadas(lista *l)
{
	int i;

	for (i = l->n - 1; i >= 0; i--)
		if (l->c[i].marcada)
			lista_quita(l, i);
}

void lista_mueve(lista *l, int de, int a)
{
	cancion t;

	if (de < 0 || de >= l->n || a < 0 || a >= l->n || de == a)
		return;
	t = l->c[de];
	if (de < a)
		memmove(&l->c[de], &l->c[de + 1], (size_t)(a - de) * sizeof(cancion));
	else
		memmove(&l->c[a + 1], &l->c[a], (size_t)(de - a) * sizeof(cancion));
	l->c[a] = t;
	if (l->actual == de)
		l->actual = a;
	else if (de < a && l->actual > de && l->actual <= a)
		l->actual--;
	else if (de > a && l->actual >= a && l->actual < de)
		l->actual++;
	l->cambios++;
}

static int compara_por(const cancion *a, const cancion *b, int por)
{
	int r = 0;

	if (por == 1)
		r = compara_llano(a->artista, b->artista);
	if (por == 2 || (!r && por == 1))
		r = compara_llano(a->o.ruta, b->o.ruta);
	if (!r)
		r = compara_llano(a->titulo, b->titulo);
	return r;
}

void lista_ordena(lista *l, int por)
{
	int i, j;
	origen actual;
	int hay = l->actual >= 0 && l->actual < l->n;

	if (hay)
		actual = l->c[l->actual].o;
	/* por inserción (estable; las listas no son tan largas) */
	for (i = 1; i < l->n; i++) {
		cancion t = l->c[i];

		for (j = i; j > 0 && compara_por(&l->c[j - 1], &t, por) > 0; j--)
			l->c[j] = l->c[j - 1];
		l->c[j] = t;
	}
	if (hay)
		for (i = 0; i < l->n; i++)
			if (!memcmp(&l->c[i].o, &actual, sizeof actual)) {
				l->actual = i;
				break;
			}
	l->cambios++;
}

/* ============================================================ el orden */
void lista_baraja(lista *l)
{
	int i;

	so_suelta(l->orden);
	l->orden = l->n ? so_pide((size_t)l->n * sizeof(int)) : NULL;
	if (!l->orden)
		return;
	for (i = 0; i < l->n; i++)
		l->orden[i] = i;
	for (i = l->n - 1; i > 0; i--) {
		int j = (int)(azar() % (u32)(i + 1)), t = l->orden[i];

		l->orden[i] = l->orden[j];
		l->orden[j] = t;
	}
	/* la que suena, la primera */
	l->pos_orden = 0;
	for (i = 0; i < l->n; i++)
		if (l->orden[i] == l->actual) {
			int t = l->orden[0];

			l->orden[0] = l->orden[i];
			l->orden[i] = t;
			break;
		}
}

static int pos_en_orden(lista *l)
{
	int i;

	if (!l->orden)
		lista_baraja(l);
	if (!l->orden)
		return -1;
	if (l->pos_orden >= 0 && l->pos_orden < l->n && l->orden[l->pos_orden] == l->actual)
		return l->pos_orden;
	for (i = 0; i < l->n; i++)
		if (l->orden[i] == l->actual)
			return l->pos_orden = i;
	return -1;
}

int lista_siguiente(lista *l, int a_mano)
{
	int vueltas;

	if (!l->n)
		return -1;
	if (l->actual < -1 || l->actual >= l->n)
		l->actual = -1;	/* (una que ya no está: desde el principio) */
	if (l->repetir == REPITE_UNA && !a_mano && l->actual >= 0)
		return l->actual;
	for (vueltas = 0; vueltas < l->n; vueltas++) {
		int s;

		if (l->aleatorio) {
			int p = pos_en_orden(l);

			if (p + 1 >= l->n) {
				if (l->repetir == REPITE_NO && !a_mano)
					return -1;
				lista_baraja(l);
				l->pos_orden = -1;
				p = -1;
			}
			s = l->orden ? l->orden[p + 1] : 0;
			l->pos_orden = p + 1;
		} else {
			s = l->actual + 1;
			if (s >= l->n) {
				if (l->repetir == REPITE_NO && !a_mano)
					return -1;
				s = 0;
			}
		}
		if (!l->c[s].fallo)
			return s;
		l->actual = s;	/* (se salta la que falló) */
	}
	return -1;
}

int lista_anterior(lista *l)
{
	if (!l->n)
		return -1;
	if (l->actual < -1 || l->actual >= l->n)
		l->actual = -1;
	if (l->aleatorio) {
		int p = pos_en_orden(l);

		if (p > 0) {
			l->pos_orden = p - 1;
			return l->orden[p - 1];
		}
		return l->actual >= 0 ? l->actual : 0;
	}
	if (l->actual > 0)
		return l->actual - 1;
	return l->repetir == REPITE_TODO ? l->n - 1 : 0;
}

u32 lista_duracion(lista *l, int *sin_saber)
{
	u32 s = 0;
	int i;

	*sin_saber = 0;
	for (i = 0; i < l->n; i++) {
		s += l->c[i].duracion;
		if (!l->c[i].duracion)
			(*sin_saber)++;
	}
	return s;
}

void lista_nombre(const cancion *c, char *d, int cabe)
{
	if (c->artista[0])
		fmt(d, cabe, "%s — %s", c->artista, c->titulo);
	else
		copia(d, cabe, c->titulo);
}
