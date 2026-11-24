/*
 * radio_iwl_pc.c - La Intel 7265 de radio_iwl.c en un PC de verdad (WIFI.BIN
 * de Hispa-Pentium; vale también para Hispa-486): buscarla en el PCI,
 * encenderla (D0, la memoria y el DMA), sus registros a la vista (BAR0), la
 * memoria de DMA del montón y el reloj, con lo de wifi/compat/base.c.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "hispa_linux.h"
#include "base.h"
#include "nucleo.h"
#include "iwl.h"
#include "iwl_fw.h"

int iwl_arranca(const struct iwl_plat *p, d32 id, const struct iwl_fw *fw, struct radio **r);
const char *const *iwl_nombres_para(d32 id, d32 rev);
const struct iwl_plat *iwl_plat_prueba;

static struct pci_dev tarjeta;
static volatile u32 *regs;
static d32 id_pci, rev_hw;
static struct iwl_fw fw;
static int fw_listo;

static d32 lee(d32 r) { return regs[r / 4]; }
static void pon(d32 r, d32 v) { regs[r / 4] = v; }
static void *dma(d32 tam, d32 *fis)
{
	dma_addr_t f;
	void *p = dma_alloc_coherent(NULL, tam, &f, 0);
	/* (dma_alloc_coherent alinea a DMA_ALIN; aquí hace falta 4 K: se pide de más) */
	if (p && (f & 0xFFF)) {
		dma_free_coherent(NULL, tam, p, f);
		p = dma_alloc_coherent(NULL, tam + 4096, &f, 0);
		if (p) {
			d32 a = (f + 4095) & ~4095u;
			p = (u8 *)p + (a - f);
			f = a;
		}
	}
	if (p)
		*fis = f;
	return p;
}
static void espera(d32 us) { hl_udelay(us); }
static d32 ms(void) { return hl_jiffies(); }
static d32 pci_lee(int r) { u32 v; pci_read_config_dword(&tarjeta, r, &v); return v; }
static void pci_pon(int r, d32 v) { pci_write_config_dword(&tarjeta, r, v); }
static const struct iwl_plat plat = { lee, pon, dma, espera, ms, pci_lee, pci_pon };

int iwl_busca(void)
{
	int bus, df, e, i;
	d32 id, b0, cmd, p, v;
	const char *m = NULL;

	for (bus = 0; bus < 256; bus++)
		for (df = 0; df < 256; df++) {
			id = hl_pci_lee(bus, df, 0);
			if (id == 0xffffffff || (id & 0xffff) == 0xffff) {
				if (!(df & 7))
					df += 7;
				continue;
			}
			e = iwl_reconoce(id, &m);
			if (e >= 0)
				goto hallada;
			if (!(df & 7) && !(hl_pci_lee(bus, df, 0x0c) & 0x800000))
				df += 7;
		}
	return 1;
hallada:
	memset(&tarjeta, 0, sizeof tarjeta);
	tarjeta.bus = &tarjeta.bus0;
	tarjeta.bus0.number = bus;
	tarjeta.devfn = df;
	id_pci = id;
	b0 = pci_lee(0x10);
	hl_log(2, "Tarjeta Wi-Fi: %s, %04X:%04X rev. %02X (subsistema %04X:%04X) en %02x:%02x.%d, BAR0 %08X %08X\n", m,
	       id & 0xffff, id >> 16, pci_lee(8) & 0xff, pci_lee(0x2c) & 0xffff, pci_lee(0x2c) >> 16, bus, df >> 3, df & 7,
	       b0, pci_lee(0x14));
	if (e == 1) {
		hl_log(0, "Esta Wi-Fi de Intel aún no se sabe llevar (docs/PENTIUM.md, «El Wi-Fi de Intel»: el plan por etapas)\n");
		return -2;
	}
	if ((b0 & 1) || !(b0 & ~0xfu) || ((b0 & 6) == 4 && pci_lee(0x14))) {
		hl_log(0, "la BIOS no ha dado a la tarjeta una dirección de memoria que se pueda usar (BAR0 %08X)\n", b0);
		return -2;
	}
	/* D0 (por si otro sistema la dejó dormida), memoria y DMA, y el
	 * «retry timeout» a 0 (el registro 41h, como Linux) */
	p = pci_lee(0x34) & 0xFC;
	for (i = 0; p && i < 48; i++) {
		v = pci_lee(p);
		if ((v & 0xff) == 1) {
			v = pci_lee(p + 4);
			if (v & 3) {
				hl_log(2, "  estaba en D%u: a D0\n", v & 3);
				pci_pon(p + 4, v & ~3u);
				hl_udelay(10000);
			}
			break;
		}
		p = (v >> 8) & 0xFC;
	}
	cmd = pci_lee(4);
	pci_pon(4, (cmd | 6) & ~0x400u);
	pci_pon(0x40, pci_lee(0x40) & ~0xFF00u);
	regs = ioremap(b0 & ~0xfu, 0x2000);
	rev_hw = regs ? regs[0x028 / 4] : 0xFFFFFFFF;
	hl_log(2, "  CSR_HW_REV %08X\n", rev_hw);
	return 0;
}

const char *const *iwl_nombres_fw(void) { return iwl_nombres_para(id_pci, rev_hw); }

int iwl_pon_fw(const o8 *d, d32 n, const char *nombre)
{
	char e[120];

	if (iwl_fw_lee(d, n, &fw, e, sizeof e) < 0) {
		hl_log(0, "%s no vale: %s\n", nombre, e);
		return -1;
	}
	hl_log(2, "Firmware «%s», versión %u; INIT %u octetos, RT %u octetos; escaneo %s\n", fw.texto, fw.ver,
	       iwl_fw_octetos(&fw.img[IWL_IMG_INIT]), iwl_fw_octetos(&fw.img[IWL_IMG_RT]),
	       iwl_fw_capa(&fw, IWL_CAPA_UMAC_SCAN) ? "UMAC" : "LMAC");
	fw_listo = 1;
	return 0;
}

int iwl_monta(struct radio **r)
{
	if (!fw_listo)
		return -1;
	return iwl_arranca(&plat, id_pci, &fw, r);
}

int iwl_enciende(void) { return 0; }
