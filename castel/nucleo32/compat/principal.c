/* principal.c - (COMPAT) COMPAT.BIN, el Asistente de compatibilidad:
 *   COMPAT                      la ventana
 *   COMPAT ruta                 la ventana, con ese programa o carpeta elegido (y comprobado)
 *   COMPAT /FALLO ruta [D!f]    lo lanza WIN32 cuando un programa no puede seguir
 *                               (la ventana, con lo que ha fallado arriba)
 *   COMPAT /REVISA              tras cambiar win32 (PAQUETES) y al arrancar
 *                               (ACTUALIZ /BANDEJA): vuelve a comprobar y avisa
 *   COMPAT /ORDEN ...           lo mismo en la Consola (y para las pruebas):
 *     comprueba RUTA            lo que falta, agrupado por DLL
 *     informe RUTA              lo que se mandaría (exactamente)
 *     envia RUTA /ACEPTO        mandarlo (sin /ACEPTO, solo lo enseña)
 *     lista                     lo comprobado
 *     olvida RUTA
 *     revisa [/FORZAR]
 *     componentes               los de Microsoft, y si están
 *     baja ID | licencia ID     bajarlo (y comprobar su suma) | su licencia
 *     pon ID /ACEPTO            bajarlo, aceptar su licencia y poner sus DLL en \WIN32
 *     saca ARCHIVO ID CARPETA   (sin red) sacar sus DLL de un paquete ya bajado
 *     cab ARCHIVO [NOMBRE DEST] lo que lleva el CAB de un archivo (o sacar uno)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "plat.h"
#include "vista.h"
#include "texto.h"

const char ofi_programa[] = "COMPAT.BIN";

static void di(const char *f, ...)
{
	char t[1024];
	va_list a;

	va_start(a, f);
	vfmt(t, sizeof t, f, a);
	va_end(a);
	plat_escribe(t);
}

/* la palabra k (desde 0) de los argumentos, con las comillas */
static int palabra(const char *a, int k, char *d, u32 cabe)
{
	int i = 0;

	for (;;) {
		u32 j = 0;

		while (*a == ' ')
			a++;
		if (!*a)
			return 0;
		if (*a == '"') {
			a++;
			while (*a && *a != '"') {
				if (j + 1 < cabe)
					d[j++] = *a;
				a++;
			}
			if (*a)
				a++;
		} else
			while (*a && *a != ' ') {
				if (j + 1 < cabe)
					d[j++] = *a;
				a++;
			}
		d[j] = 0;
		if (i++ == k)
			return 1;
	}
}

static int tiene(const char *a, const char *op)
{
	char p[64];
	int k;

	for (k = 0; palabra(a, k, p, sizeof p); k++)
		if (iguales_sin_caja(p, op))
			return 1;
	return 0;
}

static void avance(void *u, const char *que, u32 hecho, u32 total)
{
	(void)u;
	if (total)
		di("%s: %u de %u KB\n", que, hecho / 1024, total / 1024);
}

static const c_redist *busca_redist(c_redist *v, int n, const char *id)
{
	int k;

	for (k = 0; k < n; k++)
		if (iguales_sin_caja(v[k].id, id))
			return &v[k];
	return NULL;
}

static int orden(const char *a)
{
	char o[32], r[RUTA_MAX], e[300], w[24], h[24];
	c_informe i;

	if (!palabra(a, 1, o, sizeof o)) {
		di("Uso: COMPAT /ORDEN comprueba|informe|envia|lista|olvida|revisa|componentes|baja|licencia|pon|saca ...\n");
		return 1;
	}
	version_win32(w, sizeof w);
	version_hispa(h, sizeof h);
	if (iguales_sin_caja(o, "comprueba") || iguales_sin_caja(o, "informe") || iguales_sin_caja(o, "envia")) {
		int envia = iguales_sin_caja(o, "envia"), solo_texto = iguales_sin_caja(o, "informe") || (envia && !tiene(a, "/ACEPTO"));
		char nombre[64];

		if (!palabra(a, 2, r, sizeof r)) {
			di("Falta la ruta del programa o de la carpeta.\n");
			return 1;
		}
		if (comprueba(r, &i, envia || solo_texto, e, sizeof e)) {
			di("No se puede comprobar: %s\n", e);
			return 1;
		}
		hist_nombre(r, nombre, sizeof nombre);
		if (iguales_sin_caja(o, "comprueba")) {
			vista v;
			char *t;

			vista_haz(&v, &i, nombre);
			t = vista_texto(&v);
			if (t)
				plat_escribe(t);
			so_suelta(t);
			vista_suelta(&v);
			hist_apunta(r, nombre, w, &i);
			di("(apuntado en la lista del usuario: win32 %s)\n", w);
		} else {
			char *t = env_texto(&i, w, h), pq[300];

			if (!t) {
				di("No hay memoria.\n");
				inf_suelta(&i);
				return 1;
			}
			di("---- lo que se manda (exactamente) ----\n");
			plat_escribe(t);
			di("---- fin ----\n");
			if (envia && !solo_texto) {
				int x = env_manda(t, pq, sizeof pq);

				di(x == 0 ? "Mandado: %s\n" : "No se ha mandado: %s\n", pq);
				so_suelta(t);
				inf_suelta(&i);
				return x ? 1 : 0;
			}
			if (envia)
				di("(No se ha mandado nada: para mandarlo, añada /ACEPTO.)\n");
			so_suelta(t);
		}
		inf_suelta(&i);
		return 0;
	}
	if (iguales_sin_caja(o, "lista")) {
		c_programa *v;
		int n, k;

		if (hist_lee(&v, &n) || !v) {
			di("No hay carpeta del usuario.\n");
			return 1;
		}
		di("%d %s:\n", n, n == 1 ? "programa comprobado" : "programas comprobados");
		for (k = 0; k < n; k++)
			di("  %s  (%s)  win32 %s: %s %u, %u importantes  [%s]\n", v[k].nombre, v[k].ruta, v[k].win32, v[k].faltan == 1 ? "falta" : "faltan",
			   v[k].faltan, v[k].importantes, v[k].fecha);
		so_suelta(v);
		return 0;
	}
	if (iguales_sin_caja(o, "olvida")) {
		if (!palabra(a, 2, r, sizeof r) || hist_olvida(r)) {
			di("No se puede olvidar.\n");
			return 1;
		}
		di("Olvidado.\n");
		return 0;
	}
	if (iguales_sin_caja(o, "revisa")) {
		char *t = revisa(tiene(a, "/FORZAR"));

		if (t) {
			di("Novedades (win32 %s):\n%s", w, t);
			so_suelta(t);
		} else
			di("Nada nuevo (win32 %s).\n", w);
		return 0;
	}
	if (iguales_sin_caja(o, "cab")) {
		char nom[160], des[RUTA_MAX];
		int x;

		if (!palabra(a, 2, r, sizeof r)) {
			di("Uso: COMPAT /ORDEN cab ARCHIVO [NOMBRE DESTINO]\n");
			return 1;
		}
		x = palabra(a, 3, nom, sizeof nom) && palabra(a, 4, des, sizeof des) ? red_cab(r, nom, des, di, e, sizeof e) : red_cab(r, NULL, NULL, di, e, sizeof e);
		if (x)
			di("%s\n", e);
		return x ? 1 : 0;
	}
	if (iguales_sin_caja(o, "componentes") || iguales_sin_caja(o, "baja") || iguales_sin_caja(o, "licencia") || iguales_sin_caja(o, "pon") ||
	    iguales_sin_caja(o, "saca")) {
		c_redist *v;
		int n, k, x;
		const c_redist *c;
		char id[24], ruta[RUTA_MAX];

		if (red_lista(&v, &n) || !v) {
			di("No está la lista de componentes (\\HISPA\\COMPAT\\REDIST.LST).\n");
			return 1;
		}
		if (iguales_sin_caja(o, "componentes")) {
			for (k = 0; k < n; k++) {
				int p = red_puesto(&v[k]);

				di("%s  %s  [%s]\n    %s\n    DLL: %s\n    de %s (%u KB)\n", v[k].id, v[k].nombre,
				   p == 2 ? "puesto" : p == 1 ? "a medias" : "sin poner", v[k].para, v[k].dlls, v[k].url, v[k].tam / 1024);
			}
			so_suelta(v);
			return 0;
		}
		if (iguales_sin_caja(o, "saca")) {
			char car[RUTA_MAX];

			if (!palabra(a, 2, ruta, sizeof ruta) || !palabra(a, 3, id, sizeof id) || !palabra(a, 4, car, sizeof car) ||
			    !(c = busca_redist(v, n, id))) {
				di("Uso: COMPAT /ORDEN saca ARCHIVO ID CARPETA\n");
				so_suelta(v);
				return 1;
			}
			x = red_saca_a(c, ruta, car, NULL, NULL, e, sizeof e);
			if (x < 0)
				di("No se ha podido: %s\n", e);
			else
				di("Sacadas: %d.\n", x);
			so_suelta(v);
			return x < 0;
		}
		if (!palabra(a, 2, id, sizeof id) || !(c = busca_redist(v, n, id))) {
			di("No está ese componente (COMPAT /ORDEN componentes).\n");
			so_suelta(v);
			return 1;
		}
		if (red_baja(c, ruta, sizeof ruta, avance, NULL, e, sizeof e)) {
			di("%s\n", e);
			so_suelta(v);
			return 1;
		}
		di("Bajado y comprobado (SHA-256 %s): %s\n", c->sha, ruta);
		if (iguales_sin_caja(o, "licencia") || iguales_sin_caja(o, "pon")) {
			char *l = red_licencia(c, ruta, e, sizeof e);

			if (l) {
				di("---- la licencia de Microsoft ----\n");
				plat_escribe(l);
				di("\n---- fin de la licencia ----\n");
				so_suelta(l);
			} else
				di("%s\n", e);
			if (!l && !c->licencia_url[0]) {
				so_suelta(v);
				return 1;
			}
		}
		if (iguales_sin_caja(o, "pon")) {
			if (!tiene(a, "/ACEPTO")) {
				di("No se pone nada: para aceptar la licencia y poner las DLL, añada /ACEPTO.\n");
				so_suelta(v);
				return 1;
			}
			x = red_saca(c, ruta, avance, NULL, e, sizeof e);
			if (x < 0)
				di("No se han puesto: %s\n", e);
			else
				di("Puestas en \\WIN32: %d.\n", x);
			so_suelta(v);
			return x < 0;
		}
		so_suelta(v);
		return 0;
	}
	di("No se conoce la orden «%s».\n", o);
	return 1;
}

int ofi_principal(const char *args)
{
	char p[RUTA_MAX];

	if (palabra(args, 0, p, sizeof p) && iguales_sin_caja(p, "/ORDEN"))
		return orden(args);
	if (palabra(args, 0, p, sizeof p) && iguales_sin_caja(p, "/REVISA")) {
		char *t = revisa(tiene(args, "/FORZAR"));

		if (!t)
			return 0;
		if (plat_hay_escritorio())
			ventana_aviso(t);
		else
			plat_escribe(t);
		so_suelta(t);
		return 0;
	}
	if (palabra(args, 0, p, sizeof p) && iguales_sin_caja(p, "/FALLO")) {
		char r[RUTA_MAX], f[120];

		r[0] = f[0] = 0;
		palabra(args, 1, r, sizeof r);
		palabra(args, 2, f, sizeof f);
		return ventana_principal(r, f[0] ? f : "?");
	}
	if (palabra(args, 0, p, sizeof p))
		return ventana_principal(p, NULL);
	return ventana_principal(NULL, NULL);
}
