/* sdhci.c - (SDHCI) ACPI.BIN: las controladoras SD/eMMC (SDHCI) que el
 * equipo tiene en «modo ACPI» (no salen en el PCI: las tabletas Bay Trail,
 * como el Acer One 10, pueden llevar así el eMMC y la ranura microSD), para
 * el núcleo de Hispa-Pentium (castel/nucleo32/sdhci.cas, la llamada 160).
 * Lo mismo que mira Linux (drivers/mmc/host/sdhci-acpi.c), con ACPICA:
 *
 *  - los dispositivos con _HID o _CID de una controladora SDHCI: 80860F14
 *    (Bay Trail; por su _UID: 1 el eMMC, 2 la SDIO, 3 la SD), 80860F16 (la
 *    SD), INT33BB (2 la SDIO, 3 la SD), 80865ACC (Cherry Trail, el eMMC),
 *    80865ACA (la SD), 80865AD0 e INT33C6 (SDIO) y PNP0D40 (cualquiera);
 *  - su _STA (los que dicen que no están se apuntan igual, con su _STA:
 *    ACPI.BIN no se presenta como Windows 8 y muchas DSDT esconden así lo
 *    que va en modo ACPI);
 *  - su _CRS: la memoria (Memory32Fixed, Memory32 o un Address32/64) y la
 *    interrupción (Interrupt o IRQ);
 *  - su _PS0, que se ejecuta (como Linux, acpi_device_fix_up_power: que la
 *    controladora esté encendida).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "pentium.h"

static void copia(char *d, const char *o, u32 cabe)
{
	u32 i;

	for (i = 0; o && i + 1 < cabe && o[i]; i++)
		d[i] = o[i];
	d[i] = 0;
}

/* ¿es de una controladora SDHCI? (y qué es: SDHCI_...) */
static int es_sdhci(const char *s)
{
	static const char *const ids[] = { "80860F14", "80860F16", "INT33BB", "80865ACC", "80865ACA", "80865AD0",
					   "INT33C6", "INT3436", "INT344D", "PNP0D40", 0 };
	int i;

	for (i = 0; s && ids[i]; i++)
		if (!strcmp(s, ids[i]))
			return 1;
	return 0;
}

static u32 clase_de(const char *hid, const char *uid)
{
	int u = uid && uid[0] && !uid[1] ? uid[0] - '0' : -1;

	if (!strcmp(hid, "80860F14"))
		return u == 1 ? SDHCI_EMMC : u == 2 ? SDHCI_SDIO : u == 3 ? SDHCI_SD : SDHCI_NOSE;
	if (!strcmp(hid, "INT33BB"))
		return u == 2 ? SDHCI_SDIO : u == 3 ? SDHCI_SD : SDHCI_NOSE;
	if (!strcmp(hid, "80860F16") || !strcmp(hid, "80865ACA"))
		return SDHCI_SD;
	if (!strcmp(hid, "80865ACC"))
		return SDHCI_EMMC;
	if (!strcmp(hid, "80865AD0") || !strcmp(hid, "INT33C6") || !strcmp(hid, "INT3436") || !strcmp(hid, "INT344D"))
		return SDHCI_SDIO;
	return SDHCI_NOSE;
}

int sdhci_sin_sdio;		/* (RTL8723BS) «sinsdio» */

struct busca {
	struct pe_sdhci *f;
	int n, cabe;
};

static ACPI_STATUS un_recurso(ACPI_RESOURCE *r, void *ctx)
{
	struct pe_sdhci *f = ctx;
	ACPI_RESOURCE_ADDRESS64 a;

	switch (r->Type) {
	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
		if (!f->base) {
			f->base = r->Data.FixedMemory32.Address;
			f->largo = r->Data.FixedMemory32.AddressLength;
		}
		break;
	case ACPI_RESOURCE_TYPE_MEMORY32:
		if (!f->base) {
			f->base = r->Data.Memory32.Minimum;
			f->largo = r->Data.Memory32.AddressLength;
		}
		break;
	case ACPI_RESOURCE_TYPE_ADDRESS32:
	case ACPI_RESOURCE_TYPE_ADDRESS64:
		if (!f->base && ACPI_SUCCESS(AcpiResourceToAddress64(r, &a)) && a.ResourceType == ACPI_MEMORY_RANGE &&
		    a.Address.Minimum < 0x100000000ull) {
			f->base = (u32)a.Address.Minimum;
			f->largo = (u32)a.Address.AddressLength;
		}
		break;
	case ACPI_RESOURCE_TYPE_IRQ:
		if (f->irq == 0xFFFFFFFFu && r->Data.Irq.InterruptCount)
			f->irq = r->Data.Irq.Interrupts[0];
		break;
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		if (f->irq == 0xFFFFFFFFu && r->Data.ExtendedIrq.InterruptCount)
			f->irq = r->Data.ExtendedIrq.Interrupts[0];
		break;
	}
	return AE_OK;
}

static ACPI_STATUS un_dispositivo(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	static const char *const nombres[] = { "?", "eMMC", "SDIO", "SD" };
	struct busca *bu = ctx;
	struct pe_sdhci *f;
	ACPI_DEVICE_INFO *info;
	ACPI_OBJECT o;
	ACPI_BUFFER b;
	ACPI_HANDLE ps0;
	char ruta[64];
	const char *hid = 0;
	u32 i;

	(void)nivel; (void)ret;
	if (bu->n >= bu->cabe)
		return AE_CTRL_TERMINATE;
	if (ACPI_FAILURE(AcpiGetObjectInfo(h, &info)))
		return AE_OK;
	if ((info->Valid & ACPI_VALID_HID) && es_sdhci(info->HardwareId.String))
		hid = info->HardwareId.String;
	if (info->Valid & ACPI_VALID_CID)
		for (i = 0; i < info->CompatibleIdList.Count && !hid; i++)
			if (es_sdhci(info->CompatibleIdList.Ids[i].String))
				hid = info->CompatibleIdList.Ids[i].String;
	if (!hid) {
		ACPI_FREE(info);
		return AE_OK;
	}
	f = &bu->f[bu->n++];
	memset(f, 0, sizeof *f);
	f->version = PE_SDHCI_VERSION;
	f->irq = 0xFFFFFFFFu;
	/* (el _HID de verdad si lo tiene; si no, el _CID que ha casado) */
	copia(f->hid, (info->Valid & ACPI_VALID_HID) ? info->HardwareId.String : hid, sizeof f->hid);
	if (info->Valid & ACPI_VALID_UID)
		copia(f->uid, info->UniqueId.String, sizeof f->uid);
	ACPI_FREE(info);
	f->clase = clase_de(f->hid, f->uid);
	if (f->clase == SDHCI_NOSE && strcmp(hid, f->hid))
		f->clase = clase_de(hid, f->uid);
	b.Length = sizeof o;
	b.Pointer = &o;
	f->sta = ACPI_SUCCESS(AcpiEvaluateObjectTyped(h, "_STA", NULL, &b, ACPI_TYPE_INTEGER)) ? (u32)o.Integer.Value : 0x0F;
	b.Length = sizeof ruta;
	b.Pointer = ruta;
	if (ACPI_FAILURE(AcpiGetName(h, ACPI_FULL_PATHNAME, &b)))
		ruta[0] = 0;
	copia(f->ruta, ruta, sizeof f->ruta);
	/* encendida (D0), como Linux; la SDIO del Wi-Fi, después (sdio.c) */
	if (f->clase != SDHCI_SDIO && ACPI_SUCCESS(AcpiGetHandle(h, "_PS0", &ps0))) {
		f->ps0 = ACPI_SUCCESS(AcpiEvaluateObject(h, "_PS0", NULL, NULL)) ? 1 : 2;
	}
	AcpiWalkResources(h, METHOD_NAME__CRS, un_recurso, f);
	reg("ACPI: SDHCI: %s %s%s%s (%s), _STA %Xh: ", f->ruta, f->hid, f->uid[0] ? " _UID " : "", f->uid,
	    nombres[f->clase < 4 ? f->clase : 0], f->sta);
	if (f->base)
		reg("memoria en %08Xh (%u octetos)", f->base, f->largo);
	else
		reg("sin memoria en su _CRS");
	if (f->irq != 0xFFFFFFFFu)
		reg(", IRQ %u", f->irq);
	if (f->ps0)
		reg(", _PS0 %s", f->ps0 == 1 ? "hecho" : "falla");
	if (f->clase == SDHCI_SDIO)
		reg(sdhci_sin_sdio ? " (la SDIO, la del Wi-Fi: «sinsdio», no se toca)" : " (la SDIO, la del Wi-Fi)");
	reg("\n");
	/* (RTL8723BS) la del Wi-Fi: encenderla con su hijo (el chip) y aprenderlo */
	if (f->clase == SDHCI_SDIO && f->base && !sdhci_sin_sdio)
		sdio_aprende(h, f);
	return AE_OK;
}

/* las controladoras SDHCI del espacio de nombres, en «f» (como mucho
 * «cabe»). Da cuántas. */
int sdhci_busca(struct pe_sdhci *f, int cabe)
{
	struct busca bu = { f, 0, cabe };

	AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, un_dispositivo, NULL, &bu, NULL);
	reg("ACPI: SDHCI: %d controladoras SD/eMMC en ACPI\n", bu.n);
	return bu.n;
}
