/* veredicto.c - (TERMO) el resumen de la prueba de esfuerzo (veredicto.h).
 *
 * Lo que enseñó el HP Mini 5103 de verdad (pruebas/hpmini-5103-real.log):
 * una zona de ACPI clavada en 50 °C toda la prueba y el procesador de 25 a
 * 38 °C. Medir con «el más caliente» daba «subió de 50 a 50 (+0)» y «volvió
 * a 55 °C en 1 s». Ahora:
 *  - se mide con el sensor del procesador que más sube; si no hay ninguno
 *    del procesador, con el que más sube de los demás;
 *  - un sensor que no cambia en toda la prueba (con medidas) es «fijo»: se
 *    dice, y no cuenta para el veredicto;
 *  - «volver a la de reposo» es quedar a 5 °C o menos de lo que tenía el de
 *    referencia al empezar, y se dice así.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "veredicto.h"

#define CERCA_REPOSO 50		/* 5 °C */

char *ve_grados(char *d, int t)
{
	if (t == VE_NO)
		strcpy(d, "--");
	else if (t < 0)
		snprintf(d, 24, "-%d,%d °C", -t / 10, -t % 10);
	else
		snprintf(d, 24, "%d,%d °C", t / 10, t % 10);
	return d;
}

static void apunta(struct ve_prueba *p, int k, int v)
{
	if (v == VE_NO)
		return;
	if (p->lo[k] == VE_NO || v < p->lo[k])
		p->lo[k] = v;
	if (p->hi[k] == VE_NO || v > p->hi[k])
		p->hi[k] = v;
	p->n[k]++;
}

void ve_empieza(struct ve_prueba *p, int ns, const struct ve_sensor *s, const int *t)
{
	int k;

	memset(p, 0, sizeof *p);	/* (también lo ocupado) */
	p->ns = ns > VE_MAXS ? VE_MAXS : ns;
	p->ref = -1;
	p->t_ini = p->pico_ref = p->t_parada = p->ultimo = VE_NO;
	p->baja30 = p->baja60 = VE_NO;
	p->vuelve_s = -1;
	for (k = 0; k < p->ns; k++) {
		p->s[k] = s[k];
		p->ini[k] = p->pico[k] = t[k];
		p->lo[k] = p->hi[k] = p->parada[k] = VE_NO;
		apunta(p, k, t[k]);
	}
}

void ve_carga(struct ve_prueba *p, const int *t)
{
	int k;

	for (k = 0; k < p->ns; k++) {
		if (t[k] == VE_NO)
			continue;
		if (p->ini[k] == VE_NO)
			p->ini[k] = t[k];	/* (el que aún no daba nada al empezar) */
		if (p->pico[k] == VE_NO || t[k] > p->pico[k])
			p->pico[k] = t[k];
		apunta(p, k, t[k]);
	}
}

void ve_ocupacion(struct ve_prueba *p, int pct)
{
	if (pct < 0 || pct > 100)
		return;
	p->ocup_suma += pct;
	p->ocup_n++;
	if (pct > p->ocup_max)
		p->ocup_max = pct;
}

/* (lo ocupado de media enfriando, o -1 si no se sabe) */
static int ocupado(const struct ve_prueba *p)
{
	return p->ocup_n ? p->ocup_suma / p->ocup_n : -1;
}

void ve_frecuencia(struct ve_prueba *p, int mhz)
{
	if (mhz > 50 && mhz < 10000)
		p->mhz_suma += mhz, p->mhz_n++;
}

int ve_mhz_carga(const struct ve_prueba *p)
{
	return p->mhz_n >= 3 ? p->mhz_suma / p->mhz_n : 0;
}

/* «Intel(R) Atom(TM) CPU  Z3735F @ 1.33GHz»: 1330; sin «@», la del TSC */
int ve_mhz_nominal(const char *marca, int mhz_tsc)
{
	const char *a = marca;
	int ent = 0, dec = 0, nd = 0;

	while (*a && *a != '@')
		a++;
	if (*a == '@') {
		a++;
		while (*a == ' ')
			a++;
		while (*a >= '0' && *a <= '9')
			ent = ent * 10 + (*a++ - '0');
		if (*a == '.')
			for (a++; *a >= '0' && *a <= '9'; a++)
				if (nd < 3)
					dec = dec * 10 + (*a - '0'), nd++;
		while (nd < 3)
			dec *= 10, nd++;
		if (a[0] == 'G' && a[1] == 'H' && ent < 10 && (ent || dec))
			return ent * 1000 + dec;
		if (a[0] == 'M' && a[1] == 'H' && ent >= 100)
			return ent;
	}
	return mhz_tsc > 100 && mhz_tsc < 10000 ? mhz_tsc : 0;
}

int ve_lento(const struct ve_prueba *p, char *d, int cabe)
{
	int m = ve_mhz_carga(p);

	d[0] = 0;
	if (!m || p->mhz_nominal <= 0 || m * 10 >= p->mhz_nominal * 6)
		return 0;
	if (p->frenado)
		snprintf(d, (size_t)cabe, "El procesador ha ido a %d MHz de %d, y se ha frenado (ver FRENADOS): la prueba no lo ha calentado de verdad. ", m, p->mhz_nominal);
	else
		snprintf(d, (size_t)cabe, "El procesador ha ido a %d MHz de %d: la prueba no lo ha calentado de verdad; el sistema no ha subido la frecuencia (estado P), y no se ha frenado por calor. ", m, p->mhz_nominal);
	return 1;
}

int ve_creible(int t, int limite)
{
	if (t == VE_NO)
		return 1;
	if (t < 0 || t > 1250)
		return 0;
	return limite == VE_NO || t < limite;
}

int ve_fija(const struct ve_prueba *p, int k)
{
	return p->n[k] >= 2 && p->lo[k] != VE_NO && p->lo[k] == p->hi[k];
}

/* el de referencia: el del procesador que más sube; si no, el que más sube
 * de los que cambian; si ninguno cambia, el más caliente */
static int elige(const struct ve_prueba *p)
{
	int k, mejor = -1, msube = 0, pasada;

	for (pasada = 0; pasada < 3 && mejor < 0; pasada++)
		for (k = 0; k < p->ns; k++) {
			int sube;

			if (p->pico[k] == VE_NO || p->s[k].no_fiable || p->s[k].solo_carga || (p->ini[k] == VE_NO))
				continue;
			if (pasada == 0 && !p->s[k].cpu)
				continue;
			if (pasada < 2 && ve_fija(p, k))
				continue;
			sube = pasada == 2 ? p->pico[k] : p->pico[k] - p->ini[k];
			if (mejor < 0 || sube > msube)
				mejor = k, msube = sube;
		}
	return mejor;
}

int ve_para(struct ve_prueba *p, const int *t)
{
	int k;

	ve_carga(p, t);
	for (k = 0; k < p->ns; k++)
		p->parada[k] = t[k];
	p->ref = elige(p);
	if (p->ref >= 0) {
		p->t_ini = p->ini[p->ref];
		p->pico_ref = p->pico[p->ref];
		p->t_parada = t[p->ref] != VE_NO ? t[p->ref] : p->pico_ref;
	}
	return p->ref;
}

int ve_enfria(struct ve_prueba *p, int seg, const int *t)
{
	int k, v;

	for (k = 0; k < p->ns; k++)
		apunta(p, k, t[k]);
	if (p->ref < 0)
		return seg >= 60;
	v = t[p->ref];
	if (v == VE_NO)
		return 0;
	p->ultimo = v;
	p->ult_seg = seg;
	if (seg >= 30 && p->baja30 == VE_NO)
		p->baja30 = p->t_parada - v;
	if (seg >= 60 && p->baja60 == VE_NO)
		p->baja60 = p->t_parada - v;
	if (p->vuelve_s < 0 && p->t_ini != VE_NO && v <= p->t_ini + CERCA_REPOSO && p->pico_ref - p->t_ini > CERCA_REPOSO)
		p->vuelve_s = seg;
	return (p->vuelve_s >= 0 || p->pico_ref - p->t_ini <= CERCA_REPOSO) && seg >= 60;
}

#define PON(...) (m += snprintf(d + m, (size_t)(cabe > m ? cabe - m : 0), __VA_ARGS__), m = m < cabe ? m : cabe - 1)

static char *tiempo(char *d, int seg)
{
	if (seg >= 60 && seg % 60)
		snprintf(d, 24, "%d min %d s", seg / 60, seg % 60);
	else if (seg >= 60)
		snprintf(d, 24, "%d min", seg / 60);
	else
		snprintf(d, 24, "%d s", seg);
	return d;
}

void ve_resumen(const struct ve_prueba *p, char *d, int cabe, int seg_carga, int seguridad, const char *motivo, int esc, int simula)
{
	char a[24], b[24], c[24], a2[240];
	int lento_dicho = 0;
	int m = 0, k, r = p->ref, sube = 0, lim = VE_NO, nfijas = 0, nsin = 0, b60;

	d[0] = 0;
	if (seguridad)
		PON("PARADA DE SEGURIDAD a los %s: %s. ", tiempo(a, seg_carga), motivo);
	else
		PON("Prueba de %s%s. ", tiempo(a, seg_carga), esc ? " (parada con Esc)" : "");
	if (r < 0) {
		int nf = 0;

		d[0] = 0, m = 0;
		for (k = 0; k < p->ns; k++)
			nf += p->s[k].no_fiable;
		if (nf && p->s[0].no_fiable == VE_NF_IMPOSIBLE)
			PON("No se puede juzgar: ningún sensor da una temperatura creíble (%s marca %s, fuera de lo posible o por encima de su límite estando en reposo). No es que se caliente: es que este equipo no deja leer bien su temperatura así.",
			    p->s[0].nombre, ve_grados(a, p->ini[0] != VE_NO ? p->ini[0] : p->lo[0]));
		else if (nf)
			PON("No se puede juzgar: ningún sensor se ha movido con la carga (%s se queda en %s): este equipo no da la temperatura del procesador.",
			    p->s[0].nombre, ve_grados(a, p->ini[0] != VE_NO ? p->ini[0] : p->lo[0]));
		else
			PON("Ningún sensor ha dado temperaturas durante la prueba: no se puede decir nada.");
		return;
	}
	if (ve_fija(p, r) && p->s[r].cpu && !p->s[r].aproximado) {
		PON("El sensor del procesador («%s») no ha cambiado en toda la prueba (siempre %s): este equipo no da la temperatura del procesador, y no se puede juzgar cómo se calienta.",
		    p->s[r].nombre, ve_grados(a, p->lo[r]));
		return;
	}
	sube = p->pico_ref - p->t_ini;
	lim = p->s[r].limite;
	if (p->s[r].aproximado)
		PON("Medido con «%s» (medida aproximada: la cifra no es de fiar, la subida sí): subió unos %d °C (de %s a %s). ", p->s[r].nombre, (sube + 5) / 10,
		    ve_grados(a, p->t_ini), ve_grados(b, p->pico_ref));
	else
		PON("Medido con «%s»: subió de %s a %s (%s%d °C). ", p->s[r].nombre, ve_grados(a, p->t_ini), ve_grados(b, p->pico_ref), sube >= 0 ? "+" : "", sube / 10);
	/* el enfriamiento */
	b60 = p->baja60 != VE_NO ? p->baja60 : p->baja30;
	if (p->ultimo == VE_NO)
		PON("No se ha medido el enfriamiento. ");
	else {
		if (p->baja60 != VE_NO || p->baja30 != VE_NO) {
			const char *cuando = p->baja60 != VE_NO ? "en el primer minuto" : "en los primeros 30 s";

			if (b60 > 0)
				PON("Al parar, bajó %d,%d °C %s", b60 / 10, b60 % 10, cuando);
			else
				PON("Al parar, %s no bajó (de %s a %s)", cuando, ve_grados(a, p->t_parada), ve_grados(b, p->t_parada - b60));
		} else
			PON("Al parar, en %s pasó de %s a %s", tiempo(a, p->ult_seg), ve_grados(b, p->t_parada), ve_grados(c, p->ultimo));
		if (sube <= CERCA_REPOSO)
			PON(". ");
		else if (p->vuelve_s >= 0)
			PON(", y a los %s ya estaba a 5 °C o menos de la de antes de la prueba (%s). ", tiempo(a, p->vuelve_s), ve_grados(b, p->t_ini));
		else if (b60 != VE_NO && b60 >= 50)
			/* (bajó bien, pero no hasta la de antes: en Hispa-Pentium el
			 * procesador solo descansa con «hlt», sin los estados de ahorro
			 * profundos de un sistema operativo completo: el MacBook Air 2012
			 * se quedaba en 44 °C de 33) */
			PON("; a los %s estaba en %s (antes de la prueba, %s): es normal, sin el ahorro de energía profundo de un sistema operativo completo el procesador queda en reposo unos grados por encima. ",
			    tiempo(a, p->ult_seg), ve_grados(c, p->ultimo), ve_grados(b, p->t_ini));
		else
			PON("; en %s no volvió a la de antes de la prueba (%s): se quedó en %s. ", tiempo(a, p->ult_seg), ve_grados(b, p->t_ini), ve_grados(c, p->ultimo));
	}
	/* los fijos y los que no dan nada */
	for (k = 0; k < p->ns; k++)
		if (k != r && ve_fija(p, k))
			nfijas++;
		else if (p->n[k] == 0)
			nsin++;
	if (nfijas) {
		int i = 0;

		PON(nfijas == 1 ? "No cambia en toda la prueba (fija, no cuenta): " : "No cambian en toda la prueba (fijas, no cuentan): ");
		for (k = 0; k < p->ns; k++)
			if (k != r && ve_fija(p, k))
				PON("%s%s (%s)", i++ ? ", " : "", p->s[k].nombre, ve_grados(a, p->lo[k]));
		PON(". ");
	}
	if (nsin) {
		int i = 0;

		PON("Sin dato: ");
		for (k = 0; k < p->ns; k++)
			if (k != r && !ve_fija(p, k) && p->n[k] == 0)
				PON("%s%s", i++ ? ", " : "", p->s[k].nombre);
		PON(". ");
	}
	PON("\n\n");
	/* el veredicto */
	if (seguridad)
		PON("PELIGRO: se ha parado por seguridad, cerca de su límite. Los datos indican que no saca bien el calor: conviene que un técnico revise el ventilador, el polvo de las rejillas y la pasta térmica. ");
	else if (p->s[r].aproximado && sube >= 50)
		PON("La subida en carga es normal. ");
	else if (lim != VE_NO && p->pico_ref >= lim - 200)
		PON("Se calienta bastante: en carga es normal en un portátil, pero queda cerca del límite (%s); conviene revisar la limpieza del ventilador y de las rejillas. ", ve_grados(a, lim));
	else if (ve_lento(p, a2, (int)sizeof a2) && sube < 100) {
		PON("%s", a2);
		lento_dicho = 1;
	}
	else if (sube < 50 && !simula && p->procesadores > 1)
		PON("Casi no ha subido, con la carga en %d procesadores: se enfría muy bien. ", p->procesadores);
	else if (sube < 50 && !simula)
		PON("Casi no ha subido: o se enfría muy bien, o la carga de un solo núcleo no basta para calentarlo. ");
	else if (lim != VE_NO)
		PON("La temperatura en carga es buena: %s, lejos de su límite (%s). ", ve_grados(a, p->pico_ref), ve_grados(b, lim));
	else
		PON("La temperatura en carga es buena (%s). ", ve_grados(a, p->pico_ref));
	if (!lento_dicho && !seguridad && ve_lento(p, a2, (int)sizeof a2))
		PON("%s", a2);
	/* (el MacBook Air 2012: si algo no deja al procesador en reposo, no baja
	 * aunque el ventilador vaya bien: no se juzga) */
	if (p->ocup_n && p->ocup_max >= 50 && ocupado(p) < 30)
		PON(" (Al enfriar hubo picos de ocupación de hasta el %d %%, cada pocos segundos: son de leer las zonas de la BIOS, del propio Termómetro, no del equipo; de media, el %d %%.)",
		    p->ocup_max, ocupado(p));
	if (sube >= 50 && sube < 100 && !seguridad && p->vuelve_s >= 0)
		PON("Al parar se enfría bien.");
	if (sube >= 100 && !seguridad && p->ultimo != VE_NO && ocupado(p) >= 30)
		PON("No se puede juzgar el enfriamiento: el procesador no ha quedado en reposo después de la prueba (ocupado al %d %% de media, como mucho al %d %%), y así no baja aunque el ventilador vaya bien.",
		    ocupado(p), p->ocup_max);
	else if (sube >= 100 && !seguridad && p->ultimo != VE_NO) {
		int lejos = lim != VE_NO ? p->pico_ref < lim - 300 : p->pico_ref < 700;

		if (b60 != VE_NO && (b60 >= 150 || b60 * 10 >= sube * 4))
			PON("Al parar se enfría deprisa: el ventilador y el disipador sacan bien el calor.");
		else if (lejos && sube < 200)
			PON("Al parar baja despacio, pero como no ha pasado de %s (lejos del límite) no es mala señal: muchos portátiles apenas mueven el ventilador con tan poco calor.", ve_grados(a, p->pico_ref));
		else if (lejos)
			/* (el MacBook Air 2017: 62 °C de 105 y luego en 55-57 °C con C6 = 0 %:
			 * en Hispa-Pentium el procesador solo descansa con «hlt», sin los
			 * estados de ahorro profundos; no es el polvo) */
			PON("Al parar baja despacio, pero sin el ahorro de energía profundo de un sistema operativo completo no se puede juzgar el enfriamiento fino: en carga va bien (%s, lejos de su límite%s%s).",
			    ve_grados(a, p->pico_ref), lim != VE_NO ? " de " : "", lim != VE_NO ? ve_grados(b, lim) : "");
		else if (b60 == VE_NO || b60 < 50)
			PON("Al parar baja muy despacio: los datos indican que el calor no sale bien; conviene revisar el ventilador (que gire), el polvo y la pasta térmica.");
		else
			PON("Al parar baja algo despacio: puede que tenga algo de polvo.");
	}
}

/* (TERMO) la TjMax de los Intel que no la dicen (MSR 1A2h): por el modelo,
 * el paso y la marca. El HP Mini 5103 (Atom N455, 06_1C paso Ah) salía con
 * 90 °C, los de los N270: los Pineview (N4xx, N5xx, D4xx, D5xx) son de 100 */
static int contiene(const char *t, const char *o)
{
	size_t n = strlen(o);

	for (; *t; t++)
		if (!strncmp(t, o, n))
			return 1;
	return 0;
}

int ve_tjmax_intel(int fam, int modelo, int pasos, const char *marca, int movil_ee)
{
	if (fam != 6)
		return 100;
	switch (modelo) {
	case 0x1C:		/* Diamondville, Silverthorne y Pineview */
		if (pasos == 10)
			return 100;	/* D4xx, K4xx, N4xx, D5xx, K5xx, N5xx */
		if (contiene(marca, "CPU  230") || contiene(marca, "CPU 230"))
			return 100;
		if (contiene(marca, "CPU  330") || contiene(marca, "CPU 330"))
			return 125;
		if (contiene(marca, " N4") || contiene(marca, " N5") || contiene(marca, " D4") || contiene(marca, " D5"))
			return 100;	/* (por si el paso no es el Ah) */
		return 90;	/* N270, N280, Z5xx */
	case 0x26:		/* Tunnel Creek (Exx), Lincroft (Z6xx) */
	case 0x27:		/* Medfield */
	case 0x35:		/* Cloverview */
		return 90;
	case 0x36:		/* Cedarview: N2xxx, D2xxx */
		return 100;
	case 0x0F: case 0x16: case 0x17:	/* Core 2 */
		return movil_ee ? 85 : 100;	/* (el bit 30 del MSR EEh: los de sobremesa, 85) */
	case 0x0E:		/* Core Duo/Solo (Yonah) */
		return 100;
	}
	return 100;
}

