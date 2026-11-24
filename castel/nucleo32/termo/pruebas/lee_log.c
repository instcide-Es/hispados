/* lee_log.c - (TERMO) en el anfitrión: lee un .LOG del Termómetro (de un
 * equipo de verdad) y saca otra vez el resumen de su prueba de esfuerzo con
 * veredicto.c, el mismo de TERMO.BIN. Para que lo que pasó en un equipo
 * (pruebas/hpmini-5103-real.log) no vuelva a pasar:
 *   lee_log REGISTRO.LOG      -> el resumen, por la salida
 * De cada línea DATOS: la hora, la fase y cada «| nombre=38,0 C»; de las
 * SUCESO «sensor X: ... (limite Y C)», el límite; «ACABA la carga a los N s»,
 * lo que duró. Los del procesador: «Nucleo», «Paquete», «Procesador».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "veredicto.h"

static struct ve_sensor sen[VE_MAXS];
static char csv[VE_MAXS][48];
static int ns, lim_de[VE_MAXS], visto[VE_MAXS], atom;

static int segundos(const char *l)
{
	return ((l[0] - '0') * 10 + l[1] - '0') * 3600 + ((l[3] - '0') * 10 + l[4] - '0') * 60 + (l[6] - '0') * 10 + l[7] - '0';
}

static int decimas(const char *t)
{
	int v = 0, neg = 0, d = 0;

	if (*t == '-')
		neg = 1, t++;
	if (*t < '0' || *t > '9')
		return VE_NO;
	while (*t >= '0' && *t <= '9')
		v = v * 10 + *t++ - '0';
	if (*t == ',' && t[1] >= '0' && t[1] <= '9')
		d = t[1] - '0';
	v = v * 10 + d;
	return neg ? -v : v;
}

/* «| nombre=valor» de una línea DATOS, a t[] (los sensores, por orden de aparición) */
static void datos(char *l, int *t)
{
	char *p = strstr(l, " | ");

	while (p) {
		char *n = p + 3, *ig = strchr(n, '='), *sig = strstr(n, " | ");
		int k;

		if (!ig || (sig && ig > sig))
			break;
		*ig = 0;
		if (strcmp(n, "MHz") && strcmp(n, "bateria") && strcmp(n, "ocupado") && strncmp(n, "C0", 2) && strncmp(n, "C6", 2) && strcmp(n, "seg") && strcmp(n, "smi") &&
		    strcmp(n, "parado") && strcmp(n, "V") && strcmp(n, "vent_cpu") && strcmp(n, "gasta") && strcmp(n, "carga")) {
			for (k = 0; k < ns; k++)
				if (!strcmp(csv[k], n))
					break;
			if (k == ns && ns < VE_MAXS) {
				snprintf(csv[ns], sizeof csv[0], "%s", n);
				snprintf(sen[ns].nombre, sizeof sen[0].nombre, "%s", n);
				sen[ns].cpu = !strncmp(n, "Nucleo", 6) || !strncmp(n, "Paquete", 7) || !strncmp(n, "Procesador", 10) || !strncmp(n, "Simulado", 8);
				sen[ns].limite = VE_NO;
				sen[ns].no_fiable = sen[ns].solo_carga = 0;
				sen[ns].aproximado = sen[ns].cpu && atom;
				ns++;
			}
			if (k < ns)
				t[k] = decimas(ig + 1);
		}
		*ig = '=';
		p = sig;
	}
}

int main(int argc, char **argv)
{
	FILE *f;
	static char l[2000], res[1600];
	int t[VE_MAXS], k, fase = 0, hecho = 0, t_ini = 0, t_para = 0, seg_carga = -1, seguridad = 0, esc = 0, ult = 0;
	static char motivo[200], lims[VE_MAXS][64], marca[64];
	int procs = 1, frenado = 0;
	int nlims = 0;
	struct ve_prueba p;

	if (argc < 2 || !(f = fopen(argv[1], "r"))) {
		fprintf(stderr, "uso: lee_log REGISTRO.LOG\n");
		return 2;
	}
	memset(lim_de, 0, sizeof lim_de);
	while (fgets(l, sizeof l, f)) {
		char *q, *s;

		l[strcspn(l, "\r\n")] = 0;
		if (strlen(l) < 9 || l[2] != ':')
			continue;
		if (strstr(l, "es aproximada"))
			atom = 1;	/* (un Atom: su sensor, aproximado) */
		if ((s = strstr(l, " SUCESO sensor ")) && (q = strstr(l, "(limite "))) {
			/* «sensor Nucleo 0 del procesador: 22,0 C (limite 90,0 C)» */
			char *dos = strchr(s + 15, ':');

			if (dos && nlims < VE_MAXS) {
				int v = decimas(q + 8);

				snprintf(lims[nlims], sizeof lims[0], "%.*s", (int)(dos - (s + 15)), s + 15);
				lim_de[nlims++] = v;
			}
			continue;
		}
		/* (TERMO) la marca (la frecuencia nominal), en cuántos procesadores
		 * se carga y si se frena en la carga */
		if ((s = strstr(l, "SUCESO Termometro abierto")) && (q = strstr(s, "procesador "))) {
			snprintf(marca, sizeof marca, "%.*s", (int)strcspn(q + 11, ","), q + 11);
			continue;
		}
		if ((s = strstr(l, "la carga, en ")) && strstr(s, " procesadores")) {
			procs = atoi(s + 13);
			continue;
		}
		if (strstr(l, "el procesador se ha frenado") && fase == 1) {
			frenado = 1;
			continue;
		}
		/* (TERMO) la carga rota por un parón (el MSI GT80): vuelta a poner, o no */
		if (strstr(l, "SUCESO AVISO: la carga se ") && fase == 1) {
			if ((q = strstr(l, "(a los ")) && strstr(l, "NO es completa") && !p.carga_cortada) {
				p.carga_cortada = atoi(q + 7) + 1;
				if ((s = strstr(l, "desde las ")))
					snprintf(p.carga_cortada_hora, sizeof p.carga_cortada_hora, "%.8s", s + 10);
			} else if (strstr(l, "se vuelve a poner en marcha"))
				p.carga_vuelta++;
			continue;
		}
		if ((s = strstr(l, "ACABA la carga a los "))) {
			seg_carga = atoi(s + 21);
			t_para = segundos(l);
			seguridad = strstr(s, "PARADA DE SEGURIDAD") != 0;
			esc = strstr(s, "parada con Esc") != 0;
			if (seguridad && (q = strstr(s, "SEGURIDAD: ")))
				snprintf(motivo, sizeof motivo, "%.*s", (int)strcspn(q + 11, ";"), q + 11);
			continue;
		}
		if (!strstr(l, " DATOS fase="))
			continue;
		for (k = 0; k < VE_MAXS; k++)
			t[k] = VE_NO;
		datos(l, t);
		/* los límites de los SUCESO, a su sensor (el nombre largo empieza por
		 * el corto, o acaba con la ruta de la zona) */
		for (k = 0; k < ns; k++) {
			int i;

			for (i = 0; i < nlims && sen[k].limite == VE_NO; i++) {
				size_t a = strlen(lims[i]), b = strlen(csv[k]);

				if (!strncmp(lims[i], csv[k], b) ||
				    (!strncmp(csv[k], "ACPI ", 5) && a >= b - 5 && !strcmp(lims[i] + a - (b - 5), csv[k] + 5)))
					sen[k].limite = lim_de[i];
			}
		}
		/* lo que no es creíble desde la primera lectura (como TERMO.BIN) */
		for (k = 0; k < ns; k++)
			if (!sen[k].no_fiable && !visto[k] && t[k] != VE_NO) {
				visto[k] = 1;
				sen[k].no_fiable = ve_creible(t[k], sen[k].limite) ? 0 : VE_NF_IMPOSIBLE;
			}
		ult = segundos(l);
		if (strstr(l, "fase=carga")) {
			if (fase != 1) {
				ve_empieza(&p, ns, sen, t);
				t_ini = ult;
				fase = 1;
			} else {
				char *mh = strstr(l, "MHz=");

				ve_carga(&p, t);
				if (mh && ult - t_ini >= 5)
					ve_frecuencia(&p, atoi(mh + 4));
			}
			p.procesadores = procs;
			p.mhz_nominal = ve_mhz_nominal(marca, 0);
			p.frenado = frenado;
		} else if (strstr(l, "fase=enfriando")) {
			if (fase == 1) {
				/* (lo último de la carga ya está: la parada, con ello) */
				int u[VE_MAXS];

				memcpy(u, p.parada, sizeof u);
				ve_para(&p, u);
				if (seg_carga < 0)
					t_para = ult - 2;
				fase = 2;
			}
			if (fase == 2) {
				char *o = strstr(l, "ocupado=");

				ve_enfria(&p, ult - t_para, t);
				if (o)
					ve_ocupacion(&p, atoi(o + 8));
			}
		} else if (fase == 2) {
			ve_resumen(&p, res, sizeof res, seg_carga >= 0 ? seg_carga : t_para - t_ini, seguridad, motivo, esc, 0);
			printf("%s\n", res);
			hecho = 1;
			fase = 0;
		}
		/* lo último de la carga, para la parada */
		if (fase == 1)
			for (k = 0; k < VE_MAXS; k++)
				p.parada[k] = t[k];
	}
	fclose(f);
	if (fase == 2) {
		ve_resumen(&p, res, sizeof res, seg_carga >= 0 ? seg_carga : t_para - t_ini, seguridad, motivo, esc, 0);
		printf("%s\n", res);
		hecho = 1;
	}
	return hecho ? 0 : 1;
}
