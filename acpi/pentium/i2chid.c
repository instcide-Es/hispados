/* i2chid.c - (I2CHID) ACPI.BIN: los dispositivos HID sobre I2C del equipo
 * (los touchpads de los portátiles de ahora), para el núcleo de
 * Hispa-Pentium (castel/nucleo32/i2chid.cas, la llamada 159). Lo mismo que
 * mira Linux (i2c-hid-acpi, i2c-designware), con ACPICA:
 *
 *  - los dispositivos con _HID o _CID «PNP0C50» o «ACPI0C50», con su _STA
 *    (los que dicen que no están se apuntan igual, con su _STA: ACPI.BIN no
 *    se presenta como Windows 8, y muchas DSDT esconden así el touchpad);
 *  - su _CRS: el I2cSerialBus (la dirección, la velocidad, 7 o 10 bits y la
 *    ruta de su controladora) y la GpioInt (su interrupción, que el núcleo
 *    aún no usa);
 *  - su _DSM de HID sobre I2C (3CDFF6F7-4267-4555-AD05-B30A3D8938DE,
 *    revisión 1, función 1): el registro del descriptor HID;
 *  - de la controladora: su _ADR (el aparato y la función del PCI, en el
 *    bus 0 o en el de su _BBN), sus SSCN y FMCN (los contadores del reloj
 *    SCL a 100 y a 400 kHz y la retención de SDA) y su _PS0, que se ejecuta
 *    (y el del dispositivo), como hace Linux al encenderlos.
 * Lo usan servicio.c (ACPI.BIN) y tests/acpi/anfitrion.c (acpi_sim -i2chid).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "pentium.h"

static const u8 guid_hid[16] = { 0xF7, 0xF6, 0xDF, 0x3C, 0x67, 0x42, 0x55, 0x45,
				 0xAD, 0x05, 0xB3, 0x0A, 0x3D, 0x89, 0x38, 0xDE };

static void copia(char *d, const char *o, u32 cabe)
{
	u32 i;

	for (i = 0; o && i + 1 < cabe && o[i]; i++)
		d[i] = o[i];
	d[i] = 0;
}

static int es_hid(const char *s)
{
	return s && (!strcmp(s, "PNP0C50") || !strcmp(s, "ACPI0C50"));
}

struct busca {
	struct pe_i2chid *f;
	int n, cabe;
};

static ACPI_STATUS un_recurso(ACPI_RESOURCE *r, void *ctx)
{
	struct pe_i2chid *f = ctx;

	if (r->Type == ACPI_RESOURCE_TYPE_SERIAL_BUS && r->Data.CommonSerialBus.Type == ACPI_RESOURCE_SERIAL_TYPE_I2C &&
	    f->velocidad == 0) {
		f->esclavo = r->Data.I2cSerialBus.SlaveAddress;
		f->velocidad = r->Data.I2cSerialBus.ConnectionSpeed;
		f->diez = r->Data.I2cSerialBus.AccessMode == ACPI_I2C_10BIT_MODE;
		copia(f->fuente, r->Data.I2cSerialBus.ResourceSource.StringPtr, sizeof f->fuente);
	} else if (r->Type == ACPI_RESOURCE_TYPE_GPIO && r->Data.Gpio.ConnectionType == ACPI_RESOURCE_GPIO_TYPE_INT &&
		   f->gpio_pin == 0xFFFFFFFFu && r->Data.Gpio.PinTableLength) {
		f->gpio_pin = r->Data.Gpio.PinTable[0];
		f->gpio_modo = r->Data.Gpio.Triggering | r->Data.Gpio.Polarity << 8;
	}
	return AE_OK;
}

/* (lo que no da el _CRS) un ResourceTemplate crudo, como está en la DSDT:
 * su I2cSerialBus (descriptor largo 8Eh, tipo 1) y su GpioInt (8Ch, tipo 0) */
static void crudo(const u8 *b, u32 n, struct pe_i2chid *f)
{
	u32 p = 0, l, ln, so;

	while (p + 3 <= n) {
		if (!(b[p] & 0x80)) {
			if ((b[p] >> 3 & 0xF) == 0xF)
				break;
			p += 1 + (b[p] & 7);
			continue;
		}
		l = b[p + 1] | b[p + 2] << 8;
		if (p + 3 + l > n)
			break;
		if (b[p] == 0x8E && l >= 15 && b[p + 5] == 1 && f->velocidad == 0) {
			const u8 *d = b + p + 3;

			ln = d[7] | d[8] << 8;
			f->velocidad = d[9] | d[10] << 8 | d[11] << 16 | (u32)d[12] << 24;
			f->esclavo = d[13] | d[14] << 8;
			f->diez = (d[4] & 1) != 0;
			if (9 + ln < l)
				copia(f->fuente, (const char *)d + 9 + ln, sizeof f->fuente);
		} else if (b[p] == 0x8C && l >= 20 && b[p + 4] == 0 && f->gpio_pin == 0xFFFFFFFFu) {
			const u8 *d = b + p;

			l = d[14] | d[15] << 8;		/* (la tabla de patillas, desde el principio del descriptor) */
			so = d[17] | d[18] << 8;
			if (l + 2 <= so && p + so <= n) {
				f->gpio_pin = d[l] | d[l + 1] << 8;
				f->gpio_modo = (d[7] & 1) | ((d[7] >> 1) & 3) << 8;
			}
		}
		p += 3 + l;
	}
}

static ACPI_STATUS un_buffer(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	struct pe_i2chid *f = ctx;
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *o;

	(void)nivel; (void)ret;
	if (ACPI_FAILURE(AcpiEvaluateObject(h, NULL, NULL, &b)))
		return AE_OK;
	o = b.Pointer;
	if (o && o->Type == ACPI_TYPE_BUFFER && o->Buffer.Length >= 2 && o->Buffer.Pointer[o->Buffer.Length - 2] == 0x79)
		crudo(o->Buffer.Pointer, o->Buffer.Length, f);
	ACPI_FREE(b.Pointer);
	return AE_OK;
}

/* un entero de un método o nombre (o «no» si no lo hay) */
static u32 entero(ACPI_HANDLE h, const char *n, u32 no)
{
	ACPI_OBJECT o;
	ACPI_BUFFER b = { sizeof o, &o };

	if (ACPI_FAILURE(AcpiEvaluateObjectTyped(h, (char *)n, NULL, &b, ACPI_TYPE_INTEGER)))
		return no;
	return (u32)o.Integer.Value;
}

/* SSCN o FMCN: Package (3) {alto, bajo, retención} */
static void contadores(ACPI_HANDLE h, const char *n, u32 *v)
{
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *o;
	u32 i;

	if (ACPI_FAILURE(AcpiEvaluateObject(h, (char *)n, NULL, &b)))
		return;
	o = b.Pointer;
	if (o && o->Type == ACPI_TYPE_PACKAGE && o->Package.Count >= 2)
		for (i = 0; i < 3 && i < o->Package.Count; i++)
			if (o->Package.Elements[i].Type == ACPI_TYPE_INTEGER)
				v[i] = (u32)o->Package.Elements[i].Integer.Value;
	ACPI_FREE(b.Pointer);
}

/* el _DSM de HID sobre I2C, función 1: el registro del descriptor HID */
static u32 dsm(ACPI_HANDLE h)
{
	ACPI_OBJECT a[4], *o;
	ACPI_OBJECT_LIST l = { 4, a };
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	u32 r = 0xFFFFFFFFu, i;

	a[0].Type = ACPI_TYPE_BUFFER;
	a[0].Buffer.Length = 16;
	a[0].Buffer.Pointer = (u8 *)guid_hid;
	a[1].Type = ACPI_TYPE_INTEGER;
	a[1].Integer.Value = 1;
	a[2].Type = ACPI_TYPE_INTEGER;
	a[2].Integer.Value = 1;
	a[3].Type = ACPI_TYPE_PACKAGE;
	a[3].Package.Count = 0;
	a[3].Package.Elements = NULL;
	if (ACPI_FAILURE(AcpiEvaluateObject(h, "_DSM", &l, &b)))
		return r;
	o = b.Pointer;
	if (o && o->Type == ACPI_TYPE_INTEGER)
		r = (u32)o->Integer.Value;
	else if (o && o->Type == ACPI_TYPE_BUFFER && o->Buffer.Length >= 1) {
		r = 0;
		for (i = 0; i < 4 && i < o->Buffer.Length; i++)
			r |= (u32)o->Buffer.Pointer[i] << (8 * i);
	}
	ACPI_FREE(b.Pointer);
	return r;
}

/* el aparato PCI de la controladora hc, en f->pci (y su _ADR en f->adr):
 * de su _ADR (en el bus 0 o en el de su _BBN) o, sin _ADR (la controladora
 * en modo ACPI, o la DSDT no lo dice), por su nombre, como las pone Intel:
 * I2C0-3 en 15.0-15.3, I2C4 en 19.2 y I2C5 en 19.1. «padre»: hc es el padre
 * del dispositivo, y solo vale si es una I2C (un _ADR del aparato 0, el
 * puente raíz, no). Da 1 si lo sabe. */
static int pci_de(ACPI_HANDLE hc, struct pe_i2chid *f, int padre)
{
	ACPI_HANDLE pp;
	ACPI_BUFFER b;
	char n[8];
	u32 adr, bus = 0;

	n[0] = 0;
	b.Length = sizeof n;
	b.Pointer = n;
	if (ACPI_FAILURE(AcpiGetName(hc, ACPI_SINGLE_NAME, &b)))
		n[0] = 0;
	adr = entero(hc, "_ADR", 0xFFFFFFFFu);
	if (adr != 0xFFFFFFFFu && (!padre || (adr >> 16 & 0x1F) != 0)) {
		f->adr = adr;
		if (ACPI_SUCCESS(AcpiGetParent(hc, &pp)))
			bus = entero(pp, "_BBN", 0) & 0xFF;
		f->pci = bus << 16 | ((adr >> 16) & 0x1F) << 11 | (adr & 7) << 8;
		return 1;
	}
	if (adr == 0xFFFFFFFFu && !strncmp(n, "I2C", 3) && n[3] >= '0' && n[3] <= '5') {
		static const u8 ap[6] = { 0x15, 0x15, 0x15, 0x15, 0x19, 0x19 };
		static const u8 fu[6] = { 0, 1, 2, 3, 2, 1 };
		u32 k = n[3] - '0';

		f->adr = adr;
		f->pci = (u32)ap[k] << 11 | (u32)fu[k] << 8;
		f->origen |= 2;
		return 1;
	}
	return 0;
}

static ACPI_STATUS un_dispositivo(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	struct busca *bu = ctx;
	struct pe_i2chid *f;
	ACPI_DEVICE_INFO *info;
	ACPI_HANDLE hc, padre;
	ACPI_BUFFER b;
	char ruta[64];
	const char *hid = 0, *cid = 0;
	u32 i;

	(void)nivel; (void)ret;
	if (bu->n >= bu->cabe)
		return AE_CTRL_TERMINATE;
	if (ACPI_FAILURE(AcpiGetObjectInfo(h, &info)))
		return AE_OK;
	if (info->Valid & ACPI_VALID_HID)
		hid = info->HardwareId.String;
	if (info->Valid & ACPI_VALID_CID)
		for (i = 0; i < info->CompatibleIdList.Count && !cid; i++)
			if (es_hid(info->CompatibleIdList.Ids[i].String))
				cid = info->CompatibleIdList.Ids[i].String;
	if (!es_hid(hid) && !cid) {
		ACPI_FREE(info);
		return AE_OK;
	}
	f = &bu->f[bu->n++];
	memset(f, 0, sizeof *f);
	f->version = PE_I2CHID_VERSION;
	f->pci = 0xFFFFFFFFu;
	f->registro = 0xFFFFFFFFu;
	f->gpio_pin = 0xFFFFFFFFu;
	copia(f->cid, cid ? cid : "", sizeof f->cid);
	ACPI_FREE(info);
	f->sta = entero(h, "_STA", 0x0F);
	/* su _INI (en el modo mínimo, ACPI.BIN no ejecuta los _INI de los
	 * dispositivos; el de un touchpad suele decidir cuál es: su _HID, su
	 * dirección y su velocidad, como el del ASUS GL553VD), y su _HID otra vez */
	if (f->sta & 1)
		AcpiEvaluateObject(h, "_INI", NULL, NULL);
	if (ACPI_SUCCESS(AcpiGetObjectInfo(h, &info))) {
		if (info->Valid & ACPI_VALID_HID)
			copia(f->hid, info->HardwareId.String, sizeof f->hid);
		ACPI_FREE(info);
	}
	b.Length = sizeof ruta;
	b.Pointer = ruta;
	if (ACPI_FAILURE(AcpiGetName(h, ACPI_FULL_PATHNAME, &b)))
		ruta[0] = 0;
	copia(f->ruta, ruta, sizeof f->ruta);
	AcpiWalkResources(h, METHOD_NAME__CRS, un_recurso, f);
	if (f->velocidad == 0) {
		/* su _CRS no lo trae (las DSDT de Intel dan solo la interrupción a
		 * quien no es Windows 8: «If (OSYS < 0x07DC) Return (SBFI)»): los
		 * ResourceTemplate del dispositivo, tal cual (SBFB, que su _INI ya ha
		 * puesto con la dirección y la velocidad) */
		AcpiWalkNamespace(ACPI_TYPE_BUFFER, h, 1, un_buffer, NULL, f, NULL);
		if (f->velocidad)
			f->origen = 1;
	}
	f->registro = dsm(h);
	/* la controladora: la que dice su I2cSerialBus; si no existe (las DSDT
	 * de Intel traen «NULL» en la plantilla y el _CRS la pone, solo para
	 * Windows 8: el ThinkPad E14 Gen 5) o no se sabe cuál es, su padre (el
	 * touchpad cuelga de la suya: \_SB.PC00.I2C0.TPD0), como el ACPI_COMPANION
	 * de Linux */
	hc = NULL;
	if (f->fuente[0] && ACPI_SUCCESS(AcpiGetHandle(NULL, f->fuente, &hc)))
		pci_de(hc, f, 0);
	else
		hc = NULL;
	if (f->pci == 0xFFFFFFFFu && ACPI_SUCCESS(AcpiGetParent(h, &padre)) && pci_de(padre, f, 1)) {
		hc = padre;
		f->origen |= 4;
	}
	if (hc) {
		contadores(hc, "SSCN", f->ss);
		contadores(hc, "FMCN", f->fm);
		AcpiEvaluateObject(hc, "_PS0", NULL, NULL);
	}
	AcpiEvaluateObject(h, "_PS0", NULL, NULL);
	reg("ACPI: I2C-HID: %s %s%s%s%s, _STA %Xh: I2cSerialBus %Xh a %u Hz%s por %s%s (",
	    f->ruta, f->hid, f->cid[0] ? " (_CID " : "", f->cid, f->cid[0] ? ")" : "", f->sta, f->esclavo, f->velocidad,
	    f->diez ? " (10 bits)" : "", f->fuente[0] ? f->fuente : "?", f->origen & 1 ? ", de su ResourceTemplate (no del _CRS)" : "");
	if (f->origen & 4) {
		b.Length = sizeof ruta;
		b.Pointer = ruta;
		if (ACPI_FAILURE(AcpiGetParent(h, &padre)) || ACPI_FAILURE(AcpiGetName(padre, ACPI_FULL_PATHNAME, &b)))
			ruta[0] = 0;
		reg("%s: la de su padre, %s, ", f->fuente[0] ? "no existe" : "no la dice", ruta);
	}
	if (f->origen & 2)
		reg("sin _ADR: %02X.%u por su nombre)", f->pci >> 11 & 0x1F, f->pci >> 8 & 7);
	else if (f->pci == 0xFFFFFFFFu)
		reg("no se sabe cuál es)");
	else
		reg("_ADR %08Xh)", f->adr);
	if (f->gpio_pin != 0xFFFFFFFFu)
		reg(", GpioInt %u (disparo %u, polaridad %u)", f->gpio_pin, f->gpio_modo & 0xFF, f->gpio_modo >> 8);
	if (f->registro != 0xFFFFFFFFu)
		reg("; _DSM: descriptor HID en %04Xh", f->registro);
	else
		reg("; sin _DSM de HID");
	if (f->ss[0])
		reg("; SSCN %u/%u/%u", f->ss[0], f->ss[1], f->ss[2]);
	if (f->fm[0])
		reg("; FMCN %u/%u/%u", f->fm[0], f->fm[1], f->fm[2]);
	reg("\n");
	return AE_OK;
}

/* los dispositivos HID sobre I2C del espacio de nombres, en «f» (como mucho
 * «cabe»). Da cuántos. */
int i2chid_busca(struct pe_i2chid *f, int cabe)
{
	struct busca bu = { f, 0, cabe };

	AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, un_dispositivo, NULL, &bu, NULL);
	reg("ACPI: I2C-HID: %d dispositivos HID sobre I2C (PNP0C50)\n", bu.n);
	return bu.n;
}
