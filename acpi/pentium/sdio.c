/* sdio.c - (RTL8723BS) ACPI.BIN: la controladora SDIO del Wi-Fi de las
 * tabletas Bay Trail (80860F14 _UID 2, INT33BB _UID 2...) y su hijo, el chip
 * (OBDA8723: la Realtek RTL8723BS). Como Linux (sdhci-acpi.c:
 * acpi_device_fix_up_power_extended), se ejecuta el _PS0 de la controladora
 * y el de sus hijos: así se enciende el chip, que suele ir con una patilla
 * GPIO (WL_EN) que mueve ese _PS0 por una OperationRegion GeneralPurposeIo.
 * Para eso hace falta quien atienda esas regiones (ACPICA no lo trae): aquí
 * está, para las controladoras GPIO de Bay Trail (INT33FC: SCORE, NCORE y
 * SUS, por su _UID), como el pinctrl-baytrail de Linux: la patilla de la
 * conexión (GpioIo), su registro (PAD_VAL, en la tabla de cada comunidad) y
 * su valor, como salida.
 *
 * Y se APRENDE (Carlos, 13/11: lo que el sistema ve, lo guarda y lo usa
 * solo): la dirección de la controladora (su _CRS: los registros y la copia
 * de su configuración PCI, el «BAR1»), el hijo y cada escritura en las
 * patillas, con lo que se ha esperado entre una y otra, van a \SDIO.TXT de la
 * unidad del sistema, con el equipo (el puente 00:00.0 y su subsistema).
 * WIFI.BIN (wifi/nucleo/radio_rtl_pc.c) lo usa en los arranques sin ACPI
 * (el Acer One 10 se congela con ACPI.BIN): pone la controladora en D0 por
 * su «BAR1» y repite las escrituras.
 *
 * Los números de los registros de cada patilla son datos del hardware de
 * Intel, comprobados con drivers/pinctrl/intel/pinctrl-baytrail.c de Linux.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "pentium.h"

/* las patillas de cada comunidad: el número de su registro (de 16 octetos) */
static const u8 mapa_score[102] = {
	85, 89, 93, 96, 99, 102, 98, 101, 34, 37,
	36, 38, 39, 35, 40, 84, 62, 61, 64, 59,
	54, 56, 60, 55, 63, 57, 51, 50, 53, 47,
	52, 49, 48, 43, 46, 41, 45, 42, 58, 44,
	95, 105, 70, 68, 67, 66, 69, 71, 65, 72,
	86, 90, 88, 92, 103, 77, 79, 83, 78, 81,
	80, 82, 13, 12, 15, 14, 17, 18, 19, 16,
	2, 1, 0, 4, 6, 7, 9, 8, 33, 32,
	31, 30, 29, 27, 25, 28, 26, 23, 21, 20,
	24, 22, 5, 3, 10, 11, 106, 87, 91, 104,
	97, 100,
};
static const u8 mapa_ncore[28] = {
	19, 18, 17, 20, 21, 22, 24, 25, 23, 16,
	14, 15, 12, 26, 27, 1, 4, 8, 11, 0,
	3, 6, 10, 13, 2, 5, 9, 7,
};
static const u8 mapa_sus[44] = {
	29, 33, 30, 31, 32, 34, 36, 35, 38, 37,
	18, 7, 11, 20, 17, 1, 8, 10, 19, 12,
	0, 2, 23, 39, 28, 27, 22, 21, 24, 25,
	26, 51, 56, 54, 49, 55, 48, 57, 50, 58,
	52, 53, 59, 40,
};
#define BYT_VAL 0x008
#define BYT_DIR_MASK 0x6	/* INPUT_EN (bit 2) y OUTPUT_EN (bit 1): 0 = encendidas */
#define BYT_LEVEL 0x1

/* las controladoras GPIO vistas (por la ruta de la conexión) */
struct gpio_ctl {
	char ruta[48];
	u32 base;
	int uid;		/* 1 SCORE, 2 NCORE, 3 SUS; 0 no se sabe */
};
static struct gpio_ctl ctl[4];
static int nctl;

/* lo aprendido */
#define MAX_OPS 32
static u32 op_dir[MAX_OPS], op_val[MAX_OPS], op_ms[MAX_OPS];
static int nops, grabando, gpio_puesto;
static u64 t_ultima;
static ACPI_CONNECTION_INFO conexion;

static void copia(char *d, const char *o, u32 cabe)
{
	u32 i;

	for (i = 0; o && i + 1 < cabe && o[i]; i++)
		d[i] = o[i];
	d[i] = 0;
}

static ACPI_STATUS memoria_de(ACPI_RESOURCE *r, void *x)
{
	u32 *b = x;

	if (r->Type == ACPI_RESOURCE_TYPE_FIXED_MEMORY32 && !*b)
		*b = r->Data.FixedMemory32.Address;
	else if (r->Type == ACPI_RESOURCE_TYPE_MEMORY32 && !*b)
		*b = r->Data.Memory32.Minimum;
	return AE_OK;
}

static struct gpio_ctl *controladora(const char *ruta)
{
	ACPI_HANDLE h;
	ACPI_DEVICE_INFO *info;
	struct gpio_ctl *c;
	int i;

	for (i = 0; i < nctl; i++)
		if (!strcmp(ctl[i].ruta, ruta))
			return &ctl[i];
	if (nctl >= 4 || ACPI_FAILURE(AcpiGetHandle(NULL, (char *)ruta, &h)))
		return NULL;
	c = &ctl[nctl++];
	memset(c, 0, sizeof *c);
	copia(c->ruta, ruta, sizeof c->ruta);
	if (ACPI_SUCCESS(AcpiGetObjectInfo(h, &info))) {
		if ((info->Valid & ACPI_VALID_HID) && strcmp(info->HardwareId.String, "INT33FC"))
			reg("ACPI: SDIO: la controladora GPIO %s es %s (no la de Bay Trail, INT33FC): no se sabe llevar\n", ruta,
			    info->HardwareId.String);
		else if ((info->Valid & ACPI_VALID_UID) && info->UniqueId.String[0] >= '1' && info->UniqueId.String[0] <= '3')
			c->uid = info->UniqueId.String[0] - '0';
		ACPI_FREE(info);
	}
	AcpiWalkResources(h, METHOD_NAME__CRS, memoria_de, &c->base);
	reg("ACPI: SDIO: controladora GPIO %s: %s, registros en %08Xh\n", ruta,
	    c->uid == 1 ? "SCORE" : c->uid == 2 ? "NCORE" : c->uid == 3 ? "SUS" : "?", c->base);
	return c;
}

/* el registro PAD_VAL de la patilla «p» */
static u32 registro_de(struct gpio_ctl *c, u32 p)
{
	if (!c || !c->base)
		return 0;
	if (c->uid == 1 && p < sizeof mapa_score)
		return c->base + mapa_score[p] * 16 + BYT_VAL;
	if (c->uid == 2 && p < sizeof mapa_ncore)
		return c->base + mapa_ncore[p] * 16 + BYT_VAL;
	if (c->uid == 3 && p < sizeof mapa_sus)
		return c->base + mapa_sus[p] * 16 + BYT_VAL;
	return 0;
}

static void apunta(u32 d, u32 v)
{
	u64 t = AcpiOsGetTimer();

	if (!grabando || nops >= MAX_OPS)
		return;
	if (nops > 0)
		op_ms[nops - 1] = (u32)((t - t_ultima) / 10000);
	op_dir[nops] = d;
	op_val[nops] = v;
	op_ms[nops] = 0;
	nops++;
	t_ultima = t;
}

/* las OperationRegion GeneralPurposeIo: «a» es la patilla de la conexión
 * (su número en la tabla de patillas del GpioIo), «bits» cuántas */
static ACPI_STATUS manejador_gpio(UINT32 f, ACPI_PHYSICAL_ADDRESS a, UINT32 bits, UINT64 *v, void *hc, void *rc)
{
	ACPI_CONNECTION_INFO *ci = hc;
	ACPI_RESOURCE *r = NULL;
	struct gpio_ctl *c;
	u32 i, p, d, x;
	volatile u32 *m;

	(void)rc;
	if (!ci || !ci->Connection || ACPI_FAILURE(AcpiBufferToResource(ci->Connection, ci->Length, &r)) || !r)
		return AE_BAD_PARAMETER;
	if (r->Type != ACPI_RESOURCE_TYPE_GPIO || !r->Data.Gpio.ResourceSource.StringPtr) {
		ACPI_FREE(r);
		return AE_BAD_PARAMETER;
	}
	c = controladora(r->Data.Gpio.ResourceSource.StringPtr);
	if (f == ACPI_READ)
		*v = 0;
	for (i = 0; i < bits && a + i < r->Data.Gpio.PinTableLength; i++) {
		p = r->Data.Gpio.PinTable[a + i];
		d = registro_de(c, p);
		if (!d) {
			reg("ACPI: SDIO: la patilla %u de %s no se sabe dónde está\n", p, r->Data.Gpio.ResourceSource.StringPtr);
			continue;
		}
		m = AcpiOsMapMemory(d & ~0xFFFu, 0x1000);
		if (!m)
			continue;
		x = m[(d & 0xFFF) / 4];
		if (f == ACPI_READ) {
			*v |= (UINT64)(x & BYT_LEVEL) << i;
		} else {
			/* byt_gpio_direction_output: salida, con su nivel, en una escritura */
			x &= ~(BYT_DIR_MASK | BYT_LEVEL);
			x |= (*v >> i) & 1;
			m[(d & 0xFFF) / 4] = x;
			apunta(d, x);
			reg("ACPI: SDIO: %s patilla %u (%08Xh) := %u\n", r->Data.Gpio.ResourceSource.StringPtr, p, d, (u32)((*v >> i) & 1));
		}
		AcpiOsUnmapMemory((void *)m, 0x1000);
	}
	ACPI_FREE(r);
	return AE_OK;
}

static ACPI_STATUS bar1_de(ACPI_RESOURCE *r, void *x)
{
	u32 *b = x;

	if (r->Type == ACPI_RESOURCE_TYPE_FIXED_MEMORY32) {
		if (b[1] == 1 && !b[0])
			b[0] = r->Data.FixedMemory32.Address;
		b[1]++;
	}
	return AE_OK;
}

static int ps0(ACPI_HANDLE h, const char *que)
{
	ACPI_HANDLE m;
	ACPI_STATUS e;

	if (ACPI_FAILURE(AcpiGetHandle(h, "_PS0", &m)))
		return 0;
	e = AcpiEvaluateObject(h, "_PS0", NULL, NULL);
	reg("ACPI: SDIO: _PS0 de %s: %s\n", que, ACPI_SUCCESS(e) ? "hecho" : AcpiFormatException(e));
	return ACPI_SUCCESS(e) ? 1 : -1;
}

/* la controladora SDIO «h» (su ficha ya rellena): encenderla con su hijo y
 * apuntarlo en \SDIO.TXT */
void sdio_aprende(void *h, struct pe_sdhci *f)
{
	static char t[1600];
	ACPI_HANDLE hijo = NULL;
	ACPI_DEVICE_INFO *info;
	ACPI_PCI_ID pci = { 0, 0, 0, 0 };
	UINT64 a = 0, b = 0;
	u32 bar1[2] = { 0, 0 }, m, n;
	char nombre[16] = "", ruta[48];
	ACPI_BUFFER bu;
	int i, k;

	if (!gpio_puesto) {
		ACPI_STATUS e = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT, ACPI_ADR_SPACE_GPIO, manejador_gpio, NULL, &conexion);
		gpio_puesto = 1;
		if (ACPI_FAILURE(e))
			reg("ACPI: SDIO: no se pueden atender las regiones GPIO (%s)\n", AcpiFormatException(e));
	}
	AcpiWalkResources(h, METHOD_NAME__CRS, bar1_de, bar1);
	nops = 0;
	grabando = 1;
	t_ultima = AcpiOsGetTimer();
	ps0(h, f->ruta);
	while (ACPI_SUCCESS(AcpiGetNextObject(ACPI_TYPE_DEVICE, h, hijo, &hijo))) {
		if (ACPI_FAILURE(AcpiGetObjectInfo(hijo, &info)))
			continue;
		bu.Length = sizeof ruta;
		bu.Pointer = ruta;
		if (ACPI_FAILURE(AcpiGetName(hijo, ACPI_FULL_PATHNAME, &bu)))
			ruta[0] = 0;
		reg("ACPI: SDIO: hijo %s %s (dirección %X)\n", ruta, (info->Valid & ACPI_VALID_HID) ? info->HardwareId.String : "(sin _HID)",
		    (info->Valid & ACPI_VALID_ADR) ? (u32)info->Address : 0);
		if (!nombre[0] && (info->Valid & ACPI_VALID_HID))
			copia(nombre, info->HardwareId.String, sizeof nombre);
		ACPI_FREE(info);
		ps0(hijo, ruta);
	}
	grabando = 0;
	if (nops > 0)
		op_ms[nops - 1] = (u32)((AcpiOsGetTimer() - t_ultima) / 10000);
	/* \SDIO.TXT */
	AcpiOsReadPciConfiguration(&pci, 0, &a, 32);
	AcpiOsReadPciConfiguration(&pci, 0x2C, &b, 32);
	k = snprintf(t, sizeof t, "; Hispa-Pentium: la controladora SDIO del Wi-Fi y cómo se enciende, de ACPI (acpi/pentium/sdio.c)\r\n"
		     "equipo=%08X %08X\r\nsdio=%08X %u\r\n", (u32)a, (u32)b, f->base, f->irq == 0xFFFFFFFFu ? 0 : f->irq);
	if (bar1[0])
		k += snprintf(t + k, sizeof t - k, "sdio1=%08X\r\n", bar1[0]);
	if (nombre[0])
		k += snprintf(t + k, sizeof t - k, "hijo=%s\r\n", nombre);
	for (i = 0; i < nops && k < (int)sizeof t - 60; i++) {
		k += snprintf(t + k, sizeof t - k, "gpio=%08X %08X 32\r\n", op_dir[i], op_val[i]);
		if (op_ms[i])
			k += snprintf(t + k, sizeof t - k, "espera=%u\r\n", op_ms[i]);
	}
	m = sc(SC_ABRIR, (u32)"\\SDIO.TXT", 2, 0);
	if (ES_ERROR(m)) {
		reg("ACPI: SDIO: no se puede escribir \\SDIO.TXT\n");
		return;
	}
	n = sc(SC_ESCRIBIR_ARCHIVO, m, (u32)t, k);
	sc(SC_CERRAR, m, 0, 0);
	reg("ACPI: SDIO: lo aprendido, en \\SDIO.TXT (%u octetos: la controladora en %08Xh, BAR1 %08Xh, %s, %d escrituras en las patillas)\n",
	    n, f->base, bar1[0], nombre[0] ? nombre : "sin hijo", nops);
}
