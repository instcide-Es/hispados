/* nvidia.c - (TERMO) (NVIDIA) las gráficas NVIDIA del Termómetro (nvidia.h).
 *
 * El caso: el MSI GT80 Titan 2QD, con dos GTX 970M (GM204, Maxwell 2) en
 * SLI, que se congela; el Termómetro veía una sola y sin temperatura. Ahora
 * el núcleo (termo.cas, la 225) recorre TODOS los buses y funciones (también
 * detrás de los puertos PCI Express y de un conmutador PLX/PEX) y da de cada
 * NVIDIA lo que se ve (ID, subsistema, BAR0, energía, enlace) y de cada
 * puerto PCI Express su enlace, su ranura y lo que tiene detrás; y aquí se
 * dice, sin colores: «GPU 1», «GPU 2», el porqué si no hay temperatura y,
 * si una no está, en qué puerto falta y cómo está ese puerto.
 *
 * La temperatura, como nouveau (nvkm/subdev/therm, MIT):
 *  - Maxwell GM107/GM108 (gm107_therm) y GM200/GM204/GM206 (gm200_therm):
 *    g84_temp_get: el fusible 1A8h (BAR0+21100h+1A8h) a 1 y BAR0+20400h, en
 *    °C. (BAR0+20460h es de Pascal, no de Maxwell.)
 *  - Pascal GP10x (gp100_therm): BAR0+20460h, válida con el bit 29;
 *    (v y 1FFF8h) >> 8.
 * Lo único que se escribe (lo hace el núcleo, y solo si aquí se pide: el
 * Termómetro lo pide salvo con «sololeer»), todo volátil (se pierde al
 * apagar): lo de g84_sensor_setup si el sensor de una Maxwell está apagado
 * o forzado, y el bit «memoria» y D3hot -> D0 (solo con No_Soft_Reset) de
 * pci_enable_device. Nunca relojes, voltajes, ventiladores ni la VBIOS.
 * Los relojes no se leen: nouveau no tiene cómo leerlos en GM20x (no hay
 * «clk» para GM204) y NVIDIA no lo publica.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include "nvidia.h"
#ifndef NV_ANFITRION
#include "juego.h"
#endif

#define IN(...) (n += snprintf(d + n, (size_t)(cabe - n), __VA_ARGS__), n = n < cabe ? n : cabe - 1)

const char *nv_modelo(uint32_t id)
{
	switch (id >> 16) {
	case 0x13D7: case 0x1617: return "GeForce GTX 980M";
	case 0x13D8: case 0x1618: return "GeForce GTX 970M";
	case 0x13D9: case 0x1619: return "GeForce GTX 965M";
	case 0x13DA: case 0x161A: return "GeForce GTX 980 (portátil)";
	case 0x1392: return "GeForce GTX 860M";
	case 0x139A: return "GeForce GTX 950M";
	case 0x139B: return "GeForce GTX 960M";
	case 0x1C8C: return "GeForce GTX 1050 Ti (portátil)";
	case 0x1C8D: return "GeForce GTX 1050 (portátil)";
	case 0x1C20: return "GeForce GTX 1060 (portátil)";
	case 0x1BA1: return "GeForce GTX 1070 (portátil)";
	case 0x1BE0: return "GeForce GTX 1080 (portátil)";
	}
	return "";
}

const char *nv_chip(uint32_t boot)
{
	if (boot == 0xFFFFFFFFu || boot == 0)
		return "";
	switch ((boot >> 20) & 0x1FF) {
	case 0x117: return "GM107 (Maxwell)";
	case 0x118: return "GM108 (Maxwell)";
	case 0x120: return "GM200 (Maxwell 2)";
	case 0x124: return "GM204 (Maxwell 2)";
	case 0x126: return "GM206 (Maxwell 2)";
	case 0x130: return "GP100 (Pascal)";
	case 0x132: return "GP102 (Pascal)";
	case 0x134: return "GP104 (Pascal)";
	case 0x136: return "GP106 (Pascal)";
	case 0x137: return "GP107 (Pascal)";
	case 0x138: return "GP108 (Pascal)";
	}
	return "";
}

static const char *const estados[NV_E_N] = {
	"bien",
	"apagada (D3): la ha dejado así el firmware",
	"sin la memoria activada (el BAR0 no contesta)",
	"BAR0 sin asignar o raro: el firmware no le ha dado memoria",
	"no es una Maxwell ni una Pascal: no se sabe leer su sensor",
	"sin sensor calibrado (el fusible 1A8h no es 1)",
	"sensor apagado",
	"sensor forzado (da un valor puesto, no el medido)",
	"lectura imposible (más de 130 °C)",
	"dormida o sin enlace: su puerto PCI Express tiene el enlace caído (Optimus la apaga así) o no contesta",
	"el bloque del sensor no contesta (0BADFxxxh: sin reloj o sin iniciar)",
};

const char *nv_estado(uint32_t e)
{
	return e < NV_E_N ? estados[e] : "?";
}

int nv_temperatura(const struct nv_ficha *f)
{
	return f->estado == NV_E_BIEN && f->temp <= 130 ? (int)f->temp * 10 : NV_NO;
}

static int badf(uint32_t v)
{
	return (v & 0xFFF00000u) == 0xBAD00000u;
}

/* lo mismo que hace el núcleo (termo.cas) con lo leído del BAR0, para las
 * pruebas y para que la ficha diga siempre lo mismo que los registros */
void nv_decodifica(struct nv_ficha *f)
{
	uint32_t chip = (f->boot >> 20) & 0x1FF, v;

	if (f->estado != NV_E_BIEN && f->estado != NV_E_OTRA && f->estado != NV_E_FUSIBLE && f->estado != NV_E_APAGADO && f->estado != NV_E_FORZADO &&
	    f->estado != NV_E_IMPOSIBLE && f->estado != NV_E_BADF)
		return;		/* (lo de antes del BAR0: lo dice el núcleo) */
	f->temp = 0;
	if (f->boot != 0xFFFFFFFFu && chip >= 0x130 && chip <= 0x13B) {	/* Pascal: gp100_temp_get */
		v = f->rtemp;
		if (badf(v))
			f->estado = NV_E_BADF;
		else if (!(v & 0x20000000u))
			f->estado = NV_E_APAGADO;
		else {
			f->temp = (v & 0x1FFF8u) >> 8;
			f->estado = f->temp > 130 ? NV_E_IMPOSIBLE : NV_E_BIEN;
		}
		return;
	}
	if (f->boot == 0xFFFFFFFFu || (chip != 0x117 && chip != 0x118 && chip != 0x120 && chip != 0x124 && chip != 0x126)) {
		f->estado = NV_E_OTRA;
		return;
	}
	if (f->fusible != 1) {	/* g84_temp_get: nvkm_fuse_read(1A8h) == 1 */
		f->estado = badf(f->fusible) ? NV_E_BADF : NV_E_FUSIBLE;
		return;
	}
	if (badf(f->crudo))
		f->estado = NV_E_BADF;
	else if (!(f->crudo & 0x80000000u))
		f->estado = NV_E_APAGADO;
	else if (f->crudo & 0x8000u)
		f->estado = NV_E_FORZADO;
	else if (badf(f->rtemp))
		f->estado = NV_E_BADF;
	else {
		f->temp = f->rtemp & 0xFF;
		f->estado = f->temp > 130 ? NV_E_IMPOSIBLE : NV_E_BIEN;
	}
}

static const char *velocidad(uint32_t v)
{
	switch (v & 0xF) {
	case 1: return "1.x (2,5 GT/s)";
	case 2: return "2.0 (5 GT/s)";
	case 3: return "3.0 (8 GT/s)";
	case 4: return "4.0 (16 GT/s)";
	}
	return "?";
}

char *nv_enlace(char *d, int cabe, uint32_t cap, uint32_t enl)
{
	uint32_t st = enl >> 16;
	int n = 0;

	d[0] = 0;
	if (!cap && !enl) {
		IN("sin capacidad PCI Express");
		return d;
	}
	if (enl & 0x10)
		IN("enlace DESACTIVADO (Link Disable)");
	else if (((st >> 4) & 0x3F) == 0)
		IN("enlace CAÍDO (x0)");
	else
		IN("enlace PCIe %s x%u", velocidad(st), (st >> 4) & 0x3F);
	if (cap)
		IN(" (puede PCIe %s x%u)", velocidad(cap), (cap >> 4) & 0x3F);
	return d;
}

const char *nv_energia(uint32_t pmcsr)
{
	static const char *const e[4] = { "D0 (encendida)", "D1", "D2", "D3hot (apagada)" };

	return pmcsr == 0xFFFFFFFFu ? "sin capacidad de energía" : e[pmcsr & 3];
}

static const char *tipo_puerto(uint32_t cap)
{
	switch ((cap >> 20) & 0xF) {
	case 4: return "raíz";
	case 6: return "de bajada de un conmutador";
	}
	return "puente";
}

static void bdf_texto(char *d, uint32_t bdf)
{
	snprintf(d, 12, "%02X:%02X.%u", (bdf >> 16) & 0xFF, (bdf >> 11) & 31, (bdf >> 8) & 7);
}

static const struct ta_peg *peg_de(const struct nv_todo *t, uint32_t bdf)
{
	uint32_t i;

	if (!t->hay_acpi || (bdf & 0xFF0000))
		return 0;
	for (i = 0; i < t->npeg && i < 4; i++)
		if (t->peg[i].ap_fun == (((bdf >> 11) & 31) << 3 | ((bdf >> 8) & 7)))
			return &t->peg[i];
	return 0;
}

static int objetos(char *d, int cabe, uint8_t o)
{
	static const char *const nom[7] = { "_PR0", "_PR3", "_PS0", "_PS3", "_DSM", "_ON", "_OFF" };
	int i, n = 0;

	d[0] = 0;
	for (i = 0; i < 7; i++)
		if (o & (1 << i))
			IN("%s%s", n ? " " : "", nom[i]);
	if (!n)
		IN("ninguno");
	return n;
}

/* lo del ACPI de un puerto del bus 0 */
static int acpi_texto(const struct ta_peg *g, char *d, int cabe)
{
	char a[64], b[64];
	int n = 0;

	objetos(a, sizeof a, g->puerto);
	IN("el puerto tiene %s", a);
	if (g->hijo & TA_PEG_HAY) {
		objetos(b, sizeof b, (uint8_t)(g->hijo & 0x7F));
		IN("; su gráfica (el hijo de _ADR 0), %s", b);
		if ((g->estado & 0xF) <= 3)
			IN("; _PSC dice D%u", g->estado & 0xF);
	} else
		IN("; sin hijo de _ADR 0");
	if ((g->estado >> 4) == 1)
		IN("; el recurso de energía de _PR0, ENCENDIDO");
	else if ((g->estado >> 4) == 2)
		IN("; el recurso de energía de _PR0, APAGADO");
	return n;
}

/* ¿apagada por el ACPI (Optimus)? */
static int optimus_apagada(const struct ta_peg *g)
{
	return g && ((g->estado >> 4) == 2 || ((g->hijo & TA_PEG_HAY) && (g->estado & 0xF) == 3));
}

static int puerto_caido(const struct nv_puerto *p)
{
	return (p->enl & 0x10) || ((p->enl >> 20) & 0x3F) == 0 || ((p->enl_cap & 0x00100000) && !(p->enl & 0x20000000));
}

static int ranura_ocupada(const struct nv_puerto *p)	/* 1 puesta, 0 vacía, -1 no se sabe */
{
	if (!(p->cap & 0x01000000))	/* Slot Implemented (bit 8 del registro de capacidades) */
		return -1;
	return (p->ranura >> 16) & 0x40 ? 1 : 0;
}

void nv_describe(const struct nv_todo *t, int i, char *d, int cabe)
{
	const struct nv_ficha *f = &t->g[i].f;
	const struct ta_peg *g = 0;
	char b[12], pb[12], e[80];
	int n = 0;

	bdf_texto(b, f->bdf);
	IN("GPU %d: NVIDIA %s", i + 1, nv_modelo(f->id)[0] ? nv_modelo(f->id) : "(modelo sin nombre)");
	if (nv_chip(f->boot)[0])
		IN(", chip %s", nv_chip(f->boot));
	IN(", en el PCI %s", b);
	if (f->id)
		IN(", %04X:%04X", f->id & 0xFFFF, f->id >> 16);
	if (f->clase)
		IN(" clase %04X", f->clase >> 16);
	if (f->subsis && f->subsis != 0xFFFFFFFFu)
		IN(", subsistema %04X:%04X", f->subsis & 0xFFFF, f->subsis >> 16);
	if (f->id) {
		IN(", BAR0 %08Xh", f->bar0);
		if (f->bar0 == 0)
			IN(" (SIN ASIGNAR)");
		IN(", %s", nv_energia(f->pmcsr));
		IN(", memoria %s, manda en el bus %s", (f->orden0 & 2) ? "activada" : "DESACTIVADA", (f->orden0 & 4) ? "sí" : "no");
		IN(", %s", nv_enlace(e, sizeof e, f->enl_cap, f->enl));
	}
	if (f->puerto) {
		bdf_texto(pb, f->puerto);
		IN(", cuelga del puerto %s", pb);
		g = peg_de(t, f->puerto);
	}
	IN(".");
	if (f->estado == NV_E_BIEN)
		IN(" Temperatura: %u °C (%s).", f->temp, ((f->boot >> 20) & 0x1F0) == 0x130 ? "BAR0+20460h, válida" : "BAR0+20400h, sensor calibrado y encendido");
	else
		IN(" Sin temperatura: %s.", nv_estado(f->estado));
	if (f->estado == NV_E_DORMIDA && optimus_apagada(g))
		IN(" La gráfica NVIDIA está apagada por Optimus (lo dice el ACPI); el Termómetro no la enciende.");
	if (f->estado == NV_E_APAGADO || f->estado == NV_E_FORZADO)
		IN(" (nouveau lo enciende con 20008h y 2000Ch: con «sololeer» no se ha hecho)");
	if (f->hecho & (NV_H_MEMORIA | NV_H_D0 | NV_H_SENSOR))
		IN(" Escrito en ella (volátil, se pierde al apagar):%s%s%s.", (f->hecho & NV_H_D0) ? " de D3 a D0 (sin perder la configuración)" : "",
		   (f->hecho & NV_H_MEMORIA) ? " la memoria activada, como pci_enable_device" : "",
		   (f->hecho & NV_H_SENSOR) ? " el sensor encendido como nouveau (20008h y 2000Ch)" : "");
	if (f->hecho & NV_H_D3_NO)
		IN(" (despertarla le borraría la configuración, no dice No_Soft_Reset: no se toca)");
	if (f->hecho & NV_H_FUERA)
		IN(" (su BAR0 cae fuera de la ventana del puerto: no se activa la memoria)");
	if (g) {
		char a[200];

		acpi_texto(g, a, sizeof a);
		IN(" ACPI: %s.", a);
	}
}

void nv_describe_puerto(const struct nv_todo *t, int k, char *d, int cabe)
{
	const struct nv_puerto *p = &t->p[k];
	const struct ta_peg *g = peg_de(t, p->bdf);
	char b[12], e[80];
	int n = 0, r = ranura_ocupada(p), sec = (int)((p->buses >> 8) & 0xFF);

	bdf_texto(b, p->bdf);
	IN("Puerto PCI Express %s (%s, %04X:%04X): %s", b, tipo_puerto(p->cap), p->id & 0xFFFF, p->id >> 16, nv_enlace(e, sizeof e, p->enl_cap, p->enl));
	if (r == 1)
		IN(", ranura con tarjeta puesta");
	else if (r == 0)
		IN(", ranura vacía");
	if (!sec)
		IN("; sin bus de detrás asignado");
	else if ((p->detras_id & 0xFFFF) == 0xFFFF)
		IN("; detrás (bus %02X): nada contesta", sec);
	else
		IN("; detrás (bus %02X): %04X:%04X clase %04X%s", sec, p->detras_id & 0xFFFF, p->detras_id >> 16, p->detras_clase >> 16,
		   (p->detras_id & 0xFFFF) == 0x10DE ? " (NVIDIA)" : (p->detras_id & 0xFFFF) == 0x10B5 ? " (conmutador PLX)" : "");
	if (!(p->orden & 2))
		IN("; el puerto sin la memoria activada");
	IN(".");
	if (g) {
		char a[200];

		acpi_texto(g, a, sizeof a);
		IN(" ACPI: %s.", a);
		if (puerto_caido(p) && optimus_apagada(g))
			IN(" Lo más probable: la gráfica NVIDIA está apagada por Optimus (ACPI); el Termómetro no la enciende.");
		else if (puerto_caido(p) && (g->hijo & TA_PEG_HAY) && (g->hijo & (TA_PEG_DSM | TA_PEG_PR0 | TA_PEG_PR3 | TA_PEG_OFF)))
			IN(" El ACPI declara aquí una gráfica que se puede apagar (Optimus): puede estar apagada así.");
	}
	if (r == 1 && puerto_caido(p))
		IN(" OJO: hay una tarjeta puesta y el enlace no sube: la tarjeta (o su conector) no responde.");
}

void nv_apunta(struct nv_gpu *g, int fase, int t)
{
	if (fase < 0 || fase > 2)
		return;
	if (t == NV_NO) {
		if (g->n[0] + g->n[1] + g->n[2] > 0)
			g->fallos++;
		return;
	}
	if (g->primera == NV_NO || g->n[0] + g->n[1] + g->n[2] == 0)
		g->primera = t;
	if (g->n[fase] == 0 || t < g->min[fase])
		g->min[fase] = t;
	if (g->n[fase] == 0 || t > g->max[fase])
		g->max[fase] = t;
	g->n[fase]++;
	g->suma[fase] += t;
}

static char *gr(char *d, int t)
{
	if (t == NV_NO)
		strcpy(d, "--");
	else
		snprintf(d, 24, "%d,%d °C", t / 10, t % 10);
	return d;
}

static int media(const struct nv_gpu *g, int f)
{
	return g->n[f] ? (g->suma[f] + g->n[f] / 2) / g->n[f] : NV_NO;
}

static int maxima(const struct nv_gpu *g)
{
	int f, m = NV_NO;

	for (f = 0; f < 3; f++)
		if (g->n[f] && (m == NV_NO || g->max[f] > m))
			m = g->max[f];
	return m;
}

/* lo que dicen las temperaturas de una gráfica sin carga (el Termómetro
 * carga el procesador, nunca la gráfica) */
static void lectura(const struct nv_gpu *g, char *d, int cabe)
{
	int n = 0, rep = media(g, 0), mx = maxima(g), car = g->n[1] ? g->max[1] : NV_NO;
	char a[24], b[24];

	d[0] = 0;
	if (mx == NV_NO)
		return;
	if (mx >= 900)
		IN("    PELIGRO: ha pasado de 90 °C (%s) sin que nada la use.\r\n", gr(a, mx));
	if (rep != NV_NO && rep >= 650)
		IN("    Caliente sin trabajar: %s antes de la carga, sin nada que la use. Para una gráfica parada es mucho: puede ser su ventilador parado o lento, "
		   "polvo en el disipador o la pasta térmica seca.\r\n", gr(a, rep));
	else if (rep != NV_NO && car != NV_NO && car - rep >= 150)
		IN("    Ha subido %d °C mientras solo se cargaba el procesador (de %s a %s): le llega el calor del procesador (disipador o salida de aire compartidos, "
		   "o tapados por el polvo).\r\n", (car - rep + 5) / 10, gr(a, rep), gr(b, car));
	else if (rep != NV_NO && mx < 800)
		IN("    Sin carga se mantiene templada: lo normal.\r\n");
}

int nv_informe(const struct nv_todo *t, char *d, int cabe)
{
	int n = 0, i, k, intel_peg = 0;
	char l[900], a[24], b[24], c[24];

	d[0] = 0;
	IN("\r\nGRÁFICAS (NVIDIA)\r\n");
	if (t->n == 0)
		IN("  No hay gráficas NVIDIA en el PCI (se han mirado todos los buses, también detrás de los puertos PCI Express).\r\n");
	else
		IN("  %d %s en el PCI (se han mirado todos los buses, también detrás de los puertos PCI Express):\r\n", t->n, t->n == 1 ? "gráfica NVIDIA" : "gráficas NVIDIA");
	for (i = 0; i < t->n; i++) {
		const struct nv_gpu *g = &t->g[i];

		nv_describe(t, i, l, sizeof l);
		IN("  %s\r\n", l);
		if (g->n[0] + g->n[1] + g->n[2] == 0) {
			IN("    Sin ninguna temperatura en toda la prueba.\r\n");
			continue;
		}
		IN("    Antes de la carga: %s de media", gr(a, media(g, 0)));
		if (g->n[1])
			IN("; durante la carga: mínima %s, media %s, máxima %s", gr(a, g->min[1]), gr(b, media(g, 1)), gr(c, g->max[1]));
		else
			IN("; durante la carga: sin dato");
		if (g->n[2])
			IN("; enfriando: máxima %s", gr(a, g->max[2]));
		IN("; al acabar: %s.\r\n", gr(a, g->t));
		if (g->fallos)
			IN("    %d %s sin dato después de haber dado temperatura%s.\r\n", g->fallos, g->fallos == 1 ? "lectura" : "lecturas",
			   g->calla ? ": HA DEJADO DE CONTESTAR durante la prueba" : "");
		lectura(g, l, sizeof l);
		IN("%s", l);
	}
	if (t->np) {
		IN("  Puertos PCI Express (por si falta una gráfica: cómo está cada puerto y qué tiene detrás):\r\n");
		for (k = 0; k < t->np; k++) {
			nv_describe_puerto(t, k, l, sizeof l);
			IN("    %s\r\n", l);
			if (!(t->p[k].bdf & 0xFF0000) && ((t->p[k].bdf >> 11) & 31) == 1)
				intel_peg = 1;
		}
	}
	if (t->np && t->p[0].deven != 0xFFFFFFFFu && (intel_peg || t->n)) {
		uint32_t v = t->p[0].deven;

		IN("  Los puertos de gráficas del procesador (Intel, DEVEN %08Xh): 00:01.0 %s, 00:01.1 %s, 00:01.2 %s.%s\r\n", v, (v & 8) ? "activo" : "DESACTIVADO",
		   (v & 4) ? "activo" : "DESACTIVADO", (v & 2) ? "activo" : "DESACTIVADO",
		   (v & 8) && !(v & 4) && t->n == 1 ? " Con dos gráficas en SLI, la segunda suele ir en 00:01.1: si está desactivado, la BIOS no ha visto la tarjeta (o la ha apagado)." : "");
	}
	IN("  El Termómetro carga el procesador, no las gráficas: aquí se ve cómo están sin trabajar. Solo se leen; si hace falta (sensor apagado, memoria sin "
	   "activar) se escribe lo mismo que nouveau o Linux al arrancar, en la propia tarjeta y volátil (se pierde al apagar); nunca relojes, voltajes, "
	   "ventiladores ni la BIOS de la tarjeta. Los relojes de la gráfica no se pueden leer sin su controlador.\r\n");
	return n;
}

int nv_veredicto(const struct nv_todo *t, char *d, int cabe)
{
	int n = 0, i, k;
	char a[24];

	d[0] = 0;
	for (i = 0; i < t->n; i++) {
		const struct nv_gpu *g = &t->g[i];
		int mx = maxima(g);
		const char *m = nv_modelo(g->f.id);

		if (mx != NV_NO && mx >= 900)
			IN(" GRÁFICA EN PELIGRO: la GPU %d (%s) ha llegado a %s sin que nada la use: apáguelo y llévelo al técnico (ventilador, polvo o pasta térmica de la gráfica).",
			   i + 1, m[0] ? m : "NVIDIA", gr(a, mx));
		if (g->calla)
			IN(" La GPU %d (%s) HA DEJADO DE RESPONDER durante la prueba: puede ser la gráfica, su conector o su alimentación.", i + 1, m[0] ? m : "NVIDIA");
		else if (g->f.estado == NV_E_DORMIDA && !optimus_apagada(peg_de(t, g->f.puerto)) && !g->n[0] && !g->n[1] && !g->n[2])
			IN(" La GPU %d (%s) NO RESPONDE: su enlace PCI Express está caído.", i + 1, m[0] ? m : "NVIDIA");
		else if ((g->f.estado == NV_E_BADF || g->f.estado == NV_E_IMPOSIBLE) && !g->n[0] && !g->n[1] && !g->n[2])
			IN(" La GPU %d (%s) no da una temperatura de verdad (%s).", i + 1, m[0] ? m : "NVIDIA", nv_estado(g->f.estado));
	}
	for (k = 0; k < t->np; k++)
		if (ranura_ocupada(&t->p[k]) == 1 && puerto_caido(&t->p[k])) {
			char b[12];

			bdf_texto(b, t->p[k].bdf);
			IN(" En el puerto %s hay una tarjeta puesta que NO RESPONDE (el enlace no sube).", b);
		}
	return n;
}

void nv_acpi(struct nv_todo *t, const struct termo_acpi *a)
{
	if (a->version < 2 || a->npeg == 0)
		return;
	t->hay_acpi = 1;
	t->npeg = a->npeg > 4 ? 4 : a->npeg;
	memcpy(t->peg, a->peg, sizeof t->peg);
}

#ifndef NV_ANFITRION
#define SC_GPU 225

int nv_busca(struct nv_todo *t, int escribe)
{
	int i;

	memset(t, 0, sizeof *t);
	for (i = 0; i < NV_MAX; i++) {
		struct nv_gpu *g = &t->g[t->n];

		if (sc(SC_GPU, (uint32_t)i, (uint32_t)&g->f, 1u | (escribe ? 2u : 0u)) != 1)
			break;
		nv_decodifica(&g->f);
		g->vista = 1;
		g->t = g->primera = NV_NO;
		g->estado_antes = g->f.estado;
		t->n++;
	}
	for (i = 0; i < NV_PUERTOS; i++)
		if (sc(SC_GPU, 0x100u + (uint32_t)i, (uint32_t)&t->p[t->np], 1) == 1)
			t->np++;
		else
			break;
	return t->n;
}

int nv_lee(struct nv_todo *t, int i, int escribe)
{
	struct nv_gpu *g = &t->g[i];
	struct nv_ficha f;
	int v;

	if (sc(SC_GPU, (uint32_t)i, (uint32_t)&f, 1u | (escribe ? 2u : 0u)) != 1)
		return g->t = NV_NO;
	nv_decodifica(&f);
	g->f = f;
	v = nv_temperatura(&f);
	if (v == NV_NO && g->n[0] + g->n[1] + g->n[2] > 0 && (f.estado == NV_E_DORMIDA || f.boot == 0xFFFFFFFFu))
		g->calla = 1;
	return g->t = v;
}
#endif
