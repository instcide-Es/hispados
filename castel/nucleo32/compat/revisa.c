/* revisa.c - (COMPAT) «avisar cuando ya esté»: si el paquete win32 ha
 * cambiado desde que se comprobó cada programa (su versión, en la lista
 * del usuario), se vuelve a comprobar solo y se dice qué ha cambiado:
 * «Half-Life: ya no falta nada; pruébelo de nuevo.» o «Half-Life: ahora
 * faltan 12 (antes 142).». Lo lanza PAQUETES al instalar win32 y ACTUALIZ
 * /BANDEJA al arrancar el escritorio (COMPAT /REVISA): si no ha cambiado
 * nada, no se dice nada. Los avisos quedan también en COMPAT\AVISOS.TXT.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "plat.h"
#include "texto.h"

char *revisa(int forzar)
{
	c_programa *v;
	int n, k;
	char w[24];
	bufer b;

	version_win32(w, sizeof w);
	if (hist_lee(&v, &n) || !v)
		return NULL;
	dice("compat: revisa: win32 %s, %d programas comprobados", w, n);
	memset(&b, 0, sizeof b);
	for (k = 0; k < n; k++) {
		c_informe i;
		char e[200];
		u32 ahora, antes = v[k].faltan;

		if (!forzar && iguales(v[k].win32, w))
			continue;
		if (!so_existe(v[k].ruta))
			continue;
		if (comprueba(v[k].ruta, &i, 0, e, sizeof e)) {
			dice("compat: revisa %s: %s", v[k].ruta, e);
			continue;
		}
		ahora = inf_faltan(&i, 0);
		if (!ahora && antes) {
			buf_fmt(&b, "%s: ya no falta nada; pruébelo de nuevo.\n", v[k].nombre);
			dice("compat: aviso: %s: ya no falta nada (antes %u; win32 %s)", v[k].nombre, antes, w);
		} else if (ahora != antes) {
			buf_fmt(&b, "%s: ahora %s %u (antes %u).\n", v[k].nombre, ahora == 1 ? "falta" : "faltan", ahora, antes);
			dice("compat: aviso: %s: ahora faltan %u (antes %u; win32 %s)", v[k].nombre, ahora, antes, w);
		}
		hist_apunta(v[k].ruta, v[k].nombre, w, &i);
		inf_suelta(&i);
	}
	so_suelta(v);
	if (!b.n) {
		buf_suelta(&b);
		return NULL;
	}
	buf_c(&b, 0);
	if (b.mal) {
		buf_suelta(&b);
		return NULL;
	}
	{
		/* a AVISOS.TXT (con la fecha), detrás de lo que hubiera */
		char c[RUTA_MAX], r[RUTA_MAX], *viejo;
		u32 nv = 0;
		int a, me, d, h, mi, s;
		bufer t;

		memset(&t, 0, sizeof t);
		if (!hist_carpeta(c, sizeof c)) {
			une_ruta(r, sizeof r, c, "AVISOS.TXT");
			viejo = lee_texto(r, &nv, 64 * 1024);
			if (viejo && nv > 48 * 1024) {
				/* (solo lo último) */
				buf_pon(&t, viejo + nv - 32 * 1024, 32 * 1024);
			} else if (viejo)
				buf_pon(&t, viejo, nv);
			so_suelta(viejo);
			so_ahora(&a, &me, &d, &h, &mi, &s);
			buf_fmt(&t, "%02d/%02d/%04d %02d:%02d (win32 %s)\r\n", d, me, a, h, mi, w);
			buf_txt(&t, (const char *)b.d);
			if (!t.mal)
				escribe_texto(r, (const char *)t.d, t.n);
			buf_suelta(&t);
		}
	}
	return (char *)b.d;
}
