/* hpradio.c - (HPRADIO) ACPI.BIN: el Wi-Fi de los portátiles HP.
 *
 * En los HP (el HP Mini 5103, y casi todos los de 2008 en adelante) la radio
 * del Wi-Fi la enciende y la apaga la BIOS: con la tecla del Wi-Fi (la de la
 * antena; en el 5103, F12), el SMM de la BIOS mueve la patilla W_DISABLE# de
 * la tarjeta Mini PCI Express y lo guarda (el estado vuelve igual en el
 * arranque siguiente). Windows (el «HP Wireless Assistant») y Linux (hp-wmi)
 * le piden a la BIOS que la encienda o la apague por la WMI de HP: el
 * aparato PNP0C14 cuyo _WDG tiene el GUID 5FB7F034-2C63-45E9-BE91-3D44E2C707E4
 * (en el 5103, \_SB.WMID, método WMAA), con la entrada de hp-wmi:
 *   { firma 'SECU', orden (1 leer, 2 escribir), consulta, tamaño, datos[128] }
 * y la consulta 1Bh («wireless2»: la de los HP de esa época; la vieja 05h el
 * 5103 no la tiene). Leer 1Bh da la tabla de las radios (struct
 * bios_rfkill2_state de hp-wmi): 16 octetos de cabecera (el 8.º, cuántas) y
 * 16 por radio: tipo (0 Wi-Fi, 1 Bluetooth, 2 WWAN), bus, fabricante,
 * producto, subsistema, su número (rfkill_id) y su «power»: 01h encendida,
 * 02h la deja el sistema (no apagada por programa), 04h la deja la BIOS
 * (Configuración de la BIOS), 08h la deja la tecla o el interruptor. Escribir
 * 1Bh con { 01h, 00h, número, 1 } la enciende (como «rfkill unblock» en
 * Linux, hp_wmi_rfkill2_set_block): la BIOS (SMI EA75h) suelta la patilla si
 * la tecla y la Configuración la dejan.
 *
 * Aquí, una vez, al arrancar, con la tarea de ACPI quieta: si el equipo
 * tiene esa WMI, se lee la tabla y, si el Wi-Fi está apagado por programa
 * (sin el 02h), se pide encenderlo y se lee otra vez. Lo que se ha hecho va
 * a ACPI.TXT y a RADIOHP.TXT (en \HISPA o en la raíz, como WIFI.LOG), que
 * WIFI.BIN copia a WIFI.LOG («HP: ...»). Sin acentos: WIFI.BIN lo pasa tal
 * cual. Con «sinhpradio», no se hace nada; con «hpradiosolover», solo se lee.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "pentium.h"

#define SC_BORRAR 21

static const u8 hp_guid[16] = { 0x34, 0xF0, 0xB7, 0x5F, 0x63, 0x2C, 0xE9, 0x45,
				0xBE, 0x91, 0x3D, 0x44, 0xE2, 0xC7, 0x07, 0xE4 };
static ACPI_HANDLE hp_dev;
static char hp_metodo[5];
static char hp_txt[2048];
static u32 hp_n;
static char hp_ruta[24];

static void hp_apunta(const char *f, ...)
{
	char l[200];
	va_list a;

	va_start(a, f);
	vsnprintf(l, sizeof l, f, a);
	va_end(a);
	reg("ACPI: (HPRADIO) %s\n", l);
	hp_n += (u32)snprintf(hp_txt + hp_n, sizeof hp_txt - hp_n, "%s\r\n", l);
	if (hp_n >= sizeof hp_txt)
		hp_n = sizeof hp_txt - 1;
}

/* dónde: \HISPA\RADIOHP.TXT si hay \HISPA (el disco instalado), si no en la raíz */
static void hp_pon_ruta(void)
{
#ifdef ANFITRION
	/* (tests/acpi/anfitrion.c, «-hpradio»: sin archivos; lo apuntado sale en el registro) */
	snprintf(hp_ruta, sizeof hp_ruta, "\\RADIOHP.TXT");
	return;
#else
	u32 m = sc(SC_ABRIR, (u32)"\\HISPA", 0, 0);

	if (!ES_ERROR(m)) {
		sc(SC_CERRAR, m, 0, 0);
		snprintf(hp_ruta, sizeof hp_ruta, "\\HISPA\\RADIOHP.TXT");
	} else
		snprintf(hp_ruta, sizeof hp_ruta, "\\RADIOHP.TXT");
#endif
}

/* al empezar cada arranque: el de la vez anterior, fuera (WIFI.BIN no debe
 * leer lo de otro arranque) */
void hp_radio_borra(void)
{
	hp_pon_ruta();
#ifndef ANFITRION
	sc(SC_BORRAR, (u32)hp_ruta, 0, 0);
#endif
}

static void hp_guarda(void)
{
	u32 m;

#ifdef ANFITRION
	return;
#endif
	if (!hp_n)
		return;
	if (!hp_ruta[0])
		hp_pon_ruta();
	m = sc(SC_ABRIR, (u32)hp_ruta, 2, 0);
	if (ES_ERROR(m)) {
		reg("ACPI: (HPRADIO) no se puede escribir %s (%08X)\n", hp_ruta, m);
		return;
	}
	sc(SC_ESCRIBIR_ARCHIVO, m, (u32)hp_txt, hp_n);
	sc(SC_CERRAR, m, 0, 0);
}

static ACPI_STATUS hp_un_wmi(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *o;
	u32 i;

	(void)nivel; (void)ctx; (void)ret;
	if (hp_dev || ACPI_FAILURE(AcpiEvaluateObject(h, "_WDG", NULL, &b)))
		return AE_OK;
	o = b.Pointer;
	if (o && o->Type == ACPI_TYPE_BUFFER)
		for (i = 0; i + 20 <= o->Buffer.Length; i += 20) {
			const u8 *e = o->Buffer.Pointer + i;

			/* GUID (16), el nombre del objeto (2), instancias, indicadores (2: método) */
			if (!memcmp((void *)e, (void *)hp_guid, 16) && (e[19] & 2)) {
				hp_dev = h;
				hp_metodo[0] = 'W', hp_metodo[1] = 'M', hp_metodo[2] = e[16], hp_metodo[3] = e[17], hp_metodo[4] = 0;
				break;
			}
		}
	ACPI_FREE(b.Pointer);
	return AE_OK;
}

/* la consulta de hp-wmi: orden 1 leer / 2 escribir; da el código de vuelta
 * (0 bien; -1 si el método falla) y, en «sale», lo que devuelve detrás de
 * las dos palabras de cabecera ('PASS' y el código) */
static int hp_consulta(u32 orden, u32 consulta, const u8 *datos, u32 n, u8 *sale, u32 cabe, u32 *salen)
{
	static u8 entrada[16 + 128];
	ACPI_OBJECT a[3], *o;
	ACPI_OBJECT_LIST l = { 3, a };
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_STATUS s;
	u32 metodo, codigo, firma, k;
	int r = -1;

	memset(entrada, 0, sizeof entrada);
	entrada[0] = 'S', entrada[1] = 'E', entrada[2] = 'C', entrada[3] = 'U';
	entrada[4] = orden;
	entrada[8] = consulta;
	entrada[12] = n;
	if (n)
		memcpy(entrada + 16, (void *)datos, n);
	/* el número del método dice cuánto puede devolver (hp-wmi,
	 * encode_outsize_for_pvsz): 1 nada, 2 hasta 4, 3 hasta 128 */
	metodo = cabe == 0 ? 1 : cabe <= 4 ? 2 : 3;
	a[0].Type = ACPI_TYPE_INTEGER;
	a[0].Integer.Value = 0;
	a[1].Type = ACPI_TYPE_INTEGER;
	a[1].Integer.Value = metodo;
	a[2].Type = ACPI_TYPE_BUFFER;
	a[2].Buffer.Length = sizeof entrada;
	a[2].Buffer.Pointer = entrada;
	s = AcpiEvaluateObject(hp_dev, hp_metodo, &l, &b);
	if (ACPI_FAILURE(s)) {
		hp_apunta("%s (consulta %02Xh, orden %u): falla, %s", hp_metodo, consulta, orden, AcpiFormatException(s));
		return -1;
	}
	o = b.Pointer;
	if (o && o->Type == ACPI_TYPE_BUFFER && o->Buffer.Length >= 8) {
		const u8 *p = o->Buffer.Pointer;

		firma = p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
		codigo = p[4] | p[5] << 8 | p[6] << 16 | (u32)p[7] << 24;
		k = o->Buffer.Length - 8;
		if (k > cabe)
			k = cabe;
		if (sale && k)
			memcpy(sale, (void *)(p + 8), k);
		if (salen)
			*salen = k;
		r = (int)codigo;
		if (codigo)
			hp_apunta("%s (consulta %02Xh, orden %u): la BIOS dice %u (%s)%s", hp_metodo, consulta, orden, codigo,
				  codigo == 4 ? "no la conoce" : codigo == 5 ? "tamano que no vale" : codigo == 3 ? "orden que no vale" : "error",
				  firma == 0x53534150 ? "" : ", sin la firma PASS");
	} else
		hp_apunta("%s (consulta %02Xh, orden %u): no devuelve un buffer", hp_metodo, consulta, orden);
	ACPI_FREE(b.Pointer);
	return r;
}

static const char *hp_tipo(u32 t)
{
	return t == 0 ? "Wi-Fi" : t == 1 ? "Bluetooth" : t == 2 ? "movil (WWAN)" : t == 3 ? "GPS" : "otra";
}

/* la tabla de las radios (consulta 1Bh); da el «power» del Wi-Fi (y su número),
 * o -1 */
static int hp_tabla(const char *cuando, u32 *id)
{
	u8 t[128];
	u32 n = 0, i, cuantas;
	int wifi = -1;

	memset(t, 0, sizeof t);
	if (hp_consulta(1, 0x1B, NULL, 0, t, sizeof t, &n) != 0)
		return -1;
	cuantas = t[7];
	hp_apunta("%s: la BIOS dice %u radios (%u octetos)", cuando, cuantas, n);
	for (i = 0; i < cuantas && 16 + i * 16 + 16 <= n && i < 7; i++) {
		const u8 *d = t + 16 + i * 16;
		u32 p = d[11];

		hp_apunta("  radio %u: %s, bus %u, %04X:%04X (subsistema %04X:%04X), numero %u, estado %02Xh: %s, %s, %s, %s", i,
			  hp_tipo(d[0]), d[1], d[2] | d[3] << 8, d[4] | d[5] << 8, d[6] | d[7] << 8, d[8] | d[9] << 8, d[10], p,
			  p & 1 ? "ENCENDIDA" : "apagada", p & 2 ? "el sistema la deja" : "APAGADA POR PROGRAMA",
			  p & 4 ? "la BIOS la deja" : "APAGADA EN LA CONFIGURACION DE LA BIOS",
			  p & 8 ? "la tecla la deja" : "APAGADA CON LA TECLA DEL WI-FI");
		if (d[0] == 0 && wifi < 0) {
			wifi = (int)p;
			*id = d[10];
		}
	}
	return wifi;
}

void hp_radio(int solo_ver)
{
	u32 id = 0;
	int p, q;
	u8 pide[4];

	AcpiGetDevices("PNP0C14", hp_un_wmi, NULL, NULL);
	if (!hp_dev)
		return;		/* no es un HP (o no tiene esa WMI): nada */
	hp_apunta("WMI de HP (GUID 5FB7F034-...-3D44E2C707E4) en %s; se mira el Wi-Fi (consulta 1Bh, como hp-wmi de Linux)", hp_metodo);
	p = hp_tabla("al arrancar", &id);
	if (p < 0) {
		hp_apunta("la BIOS no da el Wi-Fi en la tabla de las radios: no se hace nada");
		hp_guarda();
		return;
	}
	if (!(p & 2) && !solo_ver) {
		hp_apunta("el Wi-Fi esta apagado por programa (le falta el 02h): se pide a la BIOS que lo encienda (consulta 1Bh, escribir 01 00 %02X 01)", id);
		pide[0] = 1, pide[1] = 0, pide[2] = id, pide[3] = 1;
		hp_consulta(2, 0x1B, pide, 4, NULL, 0, NULL);
		q = hp_tabla("despues de pedirlo", &id);
		if (q >= 0)
			hp_apunta("%s", q & 2 ? "la BIOS lo ha encendido (por programa)" : "la BIOS NO lo ha encendido");
		p = q < 0 ? p : q;
	} else if (!(p & 2))
		hp_apunta("el Wi-Fi esta apagado por programa, pero con hpradiosolover no se toca");
	if (!(p & 8))
		hp_apunta("la tecla del Wi-Fi lo tiene APAGADO: pulsela (en el HP Mini 5103, F12, la de la antena); WIFI.BIN lo vera y encendera la radio");
	if (!(p & 4))
		hp_apunta("la Configuracion de la BIOS lo tiene APAGADO: F10 > System Configuration > Built-In Device Options > Embedded WLAN Device Radio = Enable");
	if (!(p & 1))
		hp_apunta("si hay un cable de red enchufado y la BIOS tiene LAN/WLAN Switching = Enable, apaga el Wi-Fi mientras haya cable: quitelo "
			  "(o F10 > System Configuration > Built-In Device Options > LAN/WLAN Switching = Disable)");
	if ((p & 0x0E) == 0x0E)
		hp_apunta("la BIOS deja el Wi-Fi encendido (tecla, Configuracion y programa)%s", p & 1 ? "" : "; si la radio sigue apagada, la patilla W_DISABLE# la mueve otra cosa");
	hp_guarda();
}
