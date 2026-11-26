/*
 * radio_brcm.c - La radio de las Broadcom de la familia BCM43224/43225/4313
 * (las que lleva el controlador brcmsmac de Linux) para el Wi-Fi de
 * Hispa-486. Une tres cosas:
 *   - el bus interno de la tarjeta (bcma): lo que en Linux hace
 *     drivers/bcma/main.c, reducido a una tarjeta PCIe;
 *   - lo que brcmsmac espera de mac80211 (alta del equipo, tramas recibidas,
 *     estado de las enviadas, colas);
 *   - la interfaz «struct radio» que usa la gestión 802.11 (w80211.c): canal,
 *     enviar, sondear, punto de acceso. La elección de la velocidad (lo que
 *     en Linux hace minstrel) es una escalera sencilla.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "hispa_linux.h"
#include "base.h"
#include <linux/bcma/bcma.h>
#include "bcma_private.h"
#include <defs.h>
#include "types.h"
#include "d11.h"
#include "pub.h"
#include "mac80211_if.h"
#include "main.h"
#include "phy_int.h"
#include "phy_lcn.h"
#include "nucleo.h"

int hl_modinit_bcma_host_pci_init(void);
int hl_modinit_brcms_module_init(void);
int bcma_host_pci_init(void);

static struct pci_dev tarjeta;
static struct pci_driver *puente;
static struct bcma_driver *ctl80211;
static struct ieee80211_hw *equipo;
static struct ieee80211_vif *interfaz;
static struct ieee80211_sta estacion;
static irq_handler_t irq_rutina;
static void *irq_dato;
static u32 colas_paradas;
static struct radio brcm;
static int arriba;
static u8 bssid_ap[6];

/* la escalera de velocidades: índices de la tabla del controlador
 * (1, 2, 5,5, 11, 6, 9, 12, 18, 24, 36, 48 y 54 Mbit/s) */
static const u8 vel_medios[12] = { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 };
static u8 escalera[12];
static int nesc, peld;
static int seguidas_bien, seguidas_mal;
static u32 tx_bien, tx_mal;

/* ------------------------------------------------- mensajes de brcmsmac */
#define MENSAJE(nombre, nivel) \
void nombre(struct device *dev, const char *fmt, ...) \
{ va_list ap; va_start(ap, fmt); hl_vlog(nivel, fmt, ap); va_end(ap); }
MENSAJE(__brcms_info, 2)
MENSAJE(__brcms_warn, 1)
MENSAJE(__brcms_err, 1)	/* brcmsmac dice «error» de cosas normales (asociado...) */
MENSAJE(__brcms_crit, 0)
struct brcms_pub;
void brcms_debugfs_init(void) { }
void brcms_debugfs_exit(void) { }
void brcms_debugfs_attach(struct brcms_pub *d) { }
void brcms_debugfs_detach(struct brcms_pub *d) { }
void brcms_debugfs_create_files(struct brcms_pub *d) { }

/* ----------------------------------------------------------- el bus bcma */
int pci_register_driver(struct pci_driver *d) { puente = d; return 0; }
int __bcma_driver_register(struct bcma_driver *drv, struct module *owner) { ctl80211 = drv; return 0; }
void bcma_driver_unregister(struct bcma_driver *drv) { }
void bcma_core_pcie2_up(struct bcma_drv_pcie2 *p) { }

struct bcma_device *bcma_find_core_unit(struct bcma_bus *bus, u16 coreid, u8 unit)
{
	struct bcma_device *core;

	list_for_each_entry(core, &bus->cores, list)
		if (core->id.id == coreid && core->core_unit == unit)
			return core;
	return NULL;
}

bool bcma_wait_value(struct bcma_device *core, u16 reg, u32 mask, u32 value, int timeout)
{
	unsigned long limite = jiffies + timeout;
	u32 val;

	do {
		val = bcma_read32(core, reg);
		if ((val & mask) == value)
			return true;
		udelay(10);
	} while (!time_after_eq(jiffies, limite));
	hl_log(1, "bcma: el registro %04x no llega al valor esperado\n", reg);
	return false;
}

void bcma_prepare_core(struct bcma_bus *bus, struct bcma_device *core)
{
	core->dev.parent = bus->dev;
	core->dma_dev = bus->dev;
	core->irq = bus->host_pci->irq;
}

void bcma_init_bus(struct bcma_bus *bus)
{
	INIT_LIST_HEAD(&bus->cores);
	bus->nr_cores = 0;
	bcma_detect_chip(bus);
}

void bcma_unregister_cores(struct bcma_bus *bus) { }
void bcma_bus_unregister(struct bcma_bus *bus) { }

/* El mismo orden que bcma_bus_register() de Linux: núcleos, ChipCommon,
 * núcleo PCIe, SPROM (los datos de fábrica: MAC, potencias, antenas) y, al
 * final, el controlador del núcleo 802.11. */
int bcma_bus_register(struct bcma_bus *bus)
{
	struct bcma_device *core;
	int err;

	err = bcma_bus_scan(bus);
	if (err) {
		hl_log(0, "bcma: no se pueden leer los núcleos de la tarjeta (%d)\n", err);
		return err;
	}
	/* (el número del chip: en decimal si pasa de 0x9999, como 43224; si no,
	 * en hexadecimal, como 0x4313: así los nombra Broadcom) */
	hl_log(2, bus->chipinfo.id > 0x9999 ? "Chip BCM%u (0x%04X), revisión %u, encapsulado %u\n"
					     : "Chip BCM%X (0x%04X), revisión %u, encapsulado %u\n",
	       bus->chipinfo.id, bus->chipinfo.id, bus->chipinfo.rev, bus->chipinfo.pkg);
	list_for_each_entry(core, &bus->cores, list)
		hl_log(2, "  núcleo %d: fabricante %03X, tipo %03X, revisión %d, en %08X (envoltorio %08X)\n",
		       core->core_index, core->id.manuf, core->id.id, core->id.rev, core->addr, core->wrap);
	core = bcma_find_core(bus, BCMA_CORE_CHIPCOMMON);
	if (core) {
		bus->drv_cc.core = core;
		bcma_core_chipcommon_early_init(&bus->drv_cc);
	}
	core = bcma_find_core(bus, BCMA_CORE_PCIE);
	if (core) {
		bus->drv_pci[0].core = core;
		bcma_core_pci_early_init(&bus->drv_pci[0]);
	}
	err = bcma_sprom_get(bus);
	if (err == -ENOENT)
		hl_log(0, "bcma: la tarjeta no tiene SPROM (datos de fábrica)\n");
	else if (err)
		hl_log(0, "bcma: no se puede leer la SPROM (%d)\n", err);
	else
		hl_log(2, "SPROM: revisión %d, placa %04X:%04X rev. %04X, país «%c%c», MAC %pM\n",
		       bus->sprom.revision, bus->boardinfo.vendor, bus->boardinfo.type, bus->sprom.board_rev,
		       bus->sprom.alpha2[0] ? bus->sprom.alpha2[0] : '-', bus->sprom.alpha2[1] ? bus->sprom.alpha2[1] : '-',
		       bus->sprom.il0mac);
	if (!err)
		hl_log(2, "SPROM: indicadores %04X%04X %04X%04X, antenas 2,4 GHz %02X, cadenas tx %X rx %X, potencia máxima %d/%d (cuartos de dBm)\n",
		       bus->sprom.boardflags_hi, bus->sprom.boardflags_lo, bus->sprom.boardflags2_hi, bus->sprom.boardflags2_lo,
		       bus->sprom.ant_available_bg, bus->sprom.txchain, bus->sprom.rxchain,
		       bus->sprom.core_pwr_info[0].maxpwr_2g, bus->sprom.core_pwr_info[1].maxpwr_2g);
	core = bcma_find_core(bus, BCMA_CORE_CHIPCOMMON);
	if (core) {
		bus->drv_cc.core = core;
		bcma_core_chipcommon_init(&bus->drv_cc);
	}
	core = bcma_find_core_unit(bus, BCMA_CORE_PCIE, 0);
	if (core) {
		bus->drv_pci[0].core = core;
		bcma_core_pci_init(&bus->drv_pci[0]);
	}
	core = bcma_find_core(bus, BCMA_CORE_80211);
	if (!core) {
		hl_log(0, "bcma: la tarjeta no tiene núcleo 802.11\n");
		return -ENODEV;
	}
	bcma_prepare_core(bus, core);
	if (!ctl80211)
		return -ENODEV;
	hl_log(2, "Núcleo 802.11 de revisión %d: se monta el controlador\n", core->id.rev);
	return ctl80211->probe(core);
}

/* ---------------------------------------------- lo que hacía mac80211 */
struct ieee80211_hw *ieee80211_alloc_hw(size_t priv, const struct ieee80211_ops *ops)
{
	struct ieee80211_hw *hw = hl_zalloc(sizeof(*hw) + priv + 16);

	if (!hw)
		return NULL;
	hw->priv = hw + 1;
	hw->wiphy = &hw->wiphy0;
	hw->wiphy0.hw = hw;
	hw->ops = ops;
	equipo = hw;
	return hw;
}
void ieee80211_free_hw(struct ieee80211_hw *hw) { if (hw == equipo) equipo = NULL; hl_free(hw); }
int ieee80211_register_hw(struct ieee80211_hw *hw) { return 0; }
void ieee80211_unregister_hw(struct ieee80211_hw *hw) { }
void ieee80211_restart_hw(struct ieee80211_hw *hw) { hl_log(0, "el controlador pide reiniciar la tarjeta (no se hace)\n"); }
void hl_mac_propia(const u8 *mac) { memcpy(brcm.mac, mac, 6); }
void ieee80211_wake_queue(struct ieee80211_hw *hw, int q) { colas_paradas &= ~(1u << q); }
void ieee80211_stop_queue(struct ieee80211_hw *hw, int q) { colas_paradas |= 1u << q; }
void ieee80211_wake_queues(struct ieee80211_hw *hw) { colas_paradas = 0; }
void ieee80211_stop_queues(struct ieee80211_hw *hw) { colas_paradas = 0xf; }
int ieee80211_queue_stopped(struct ieee80211_hw *hw, int q) { return (colas_paradas >> q) & 1; }
struct sk_buff *ieee80211_beacon_get_tim(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u16 *o, u16 *l) { return NULL; }
struct sk_buff *ieee80211_proberesp_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif) { return NULL; }
struct ieee80211_rate *ieee80211_get_tx_rate(const struct ieee80211_hw *hw, const struct ieee80211_tx_info *c)
{
	return &hw->wiphy->bands[NL80211_BAND_2GHZ]->bitrates[0];
}
int ieee80211_channel_to_frequency(int canal, enum nl80211_band b)
{
	if (b == NL80211_BAND_2GHZ)
		return canal == 14 ? 2484 : 2407 + 5 * canal;
	return 5000 + 5 * canal;
}
int ieee80211_frequency_to_channel(int f)
{
	if (f == 2484)
		return 14;
	return f < 2484 ? (f - 2407) / 5 : (f - 5000) / 5;
}

/* las normas del país: las de la tabla «mundial» del controlador */
static const struct ieee80211_regdomain *normas;
const struct ieee80211_reg_rule *freq_reg_info(struct wiphy *w, u32 khz)
{
	u32 i;

	for (i = 0; normas && i < normas->n_reg_rules; i++) {
		const struct ieee80211_reg_rule *r = &normas->reg_rules[i];
		if (khz - 10000 >= r->freq_range.start_freq_khz && khz + 10000 <= r->freq_range.end_freq_khz)
			return r;
	}
	return ERR_PTR(-ERANGE);
}
void wiphy_apply_custom_regulatory(struct wiphy *w, const struct ieee80211_regdomain *rd)
{
	int b, i;

	normas = rd;
	for (b = 0; b < NUM_NL80211_BANDS; b++) {
		struct ieee80211_supported_band *sb = w->bands[b];
		if (!sb)
			continue;
		for (i = 0; i < sb->n_channels; i++) {
			struct ieee80211_channel *ch = &sb->channels[i];
			const struct ieee80211_reg_rule *r = freq_reg_info(w, MHZ_TO_KHZ(ch->center_freq));
			if (IS_ERR(r)) {
				ch->flags |= IEEE80211_CHAN_DISABLED;
				continue;
			}
			if (r->flags & NL80211_RRF_NO_IR)
				ch->flags |= IEEE80211_CHAN_NO_IR;
			if (r->flags & NL80211_RRF_NO_OFDM)
				ch->flags |= IEEE80211_CHAN_NO_OFDM;
			if (ch->max_power > (int)MBM_TO_DBM(r->power_rule.max_eirp))
				ch->max_power = MBM_TO_DBM(r->power_rule.max_eirp);
		}
	}
}
int regulatory_hint(struct wiphy *w, const char *alpha2)
{
	hl_log(2, "País de la tarjeta (SROM): «%c%c» (se usan las normas comunes: canales 1 a 11, y 12 y 13 solo tras oír una baliza)\n",
	       alpha2[0], alpha2[1]);
	return 0;
}

int request_irq(unsigned int irq, irq_handler_t h, unsigned long f, const char *n, void *dev)
{
	irq_rutina = h;
	irq_dato = dev;
	return 0;
}
void free_irq(unsigned int irq, void *dev) { irq_rutina = NULL; }

/* una trama recibida: brcmsmac la entrega aquí sin el FCS */
void ieee80211_rx_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct ieee80211_rx_status *st = IEEE80211_SKB_RXCB(skb);

	if (!(st->flag & (RX_FLAG_FAILED_FCS_CRC | RX_FLAG_FAILED_PLCP_CRC)))
		w80211_rx(skb->data, skb->len, st->signal, ieee80211_frequency_to_channel(st->freq));
	kfree_skb(skb);
}

/* el resultado de una trama enviada: sube o baja la escalera */
void ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	int bien = (info->flags & IEEE80211_TX_STAT_ACK) != 0;
	int era_datos = skb->queue_mapping == IEEE80211_AC_BE && !skb->protocol;

	if (bien)
		tx_bien++;
	else
		tx_mal++;
	if (era_datos) {
		if (bien) {
			seguidas_mal = 0;
			if (++seguidas_bien >= 12 && peld < nesc - 1) {
				peld++;
				seguidas_bien = 0;
			}
		} else {
			seguidas_bien = 0;
			if (++seguidas_mal >= 2 && peld > 0) {
				peld--;
				seguidas_mal = 0;
			}
		}
		w80211_tx_hecho(bien);
	}
	kfree_skb(skb);
}

/* ------------------------------------------------------------- la radio */
static int indice_de(int medios)
{
	int i;
	for (i = 0; i < 12; i++)
		if (vel_medios[i] == medios)
			return i;
	return 0;
}

static int brcm_canal(int c)
{
	struct ieee80211_supported_band *sb = equipo->wiphy->bands[NL80211_BAND_2GHZ];
	int r;

	if (!arriba || c < 1 || c > sb->n_channels)
		return -1;
	if (sb->channels[c - 1].flags & IEEE80211_CHAN_DISABLED)
		return -1;
	equipo->conf.chandef.chan = &sb->channels[c - 1];
	equipo->conf.chandef.width = NL80211_CHAN_WIDTH_20_NOHT;
	r = equipo->ops->config(equipo, IEEE80211_CONF_CHANGE_CHANNEL);
	if (r)
		hl_log(0, "radio: no se puede poner el canal %d (%d)\n", c, r);
	return r;
}

static int brcm_enviar(const o8 *trama, int n, int vel)
{
	struct ieee80211_tx_control ctl = { .sta = &estacion };
	struct ieee80211_tx_info *info;
	struct sk_buff *skb;
	int datos = vel == 0, cola = datos ? IEEE80211_AC_BE : IEEE80211_AC_VO, i0, i1;

	if (!arriba)
		return -1;
	if (colas_paradas & (1u << cola))
		return -1;		/* la tarjeta tiene la cola llena */
	skb = dev_alloc_skb(equipo->extra_tx_headroom + n + 32);
	if (!skb)
		return -1;
	skb_reserve(skb, equipo->extra_tx_headroom);
	memcpy(skb_put(skb, n), trama, n);
	skb->queue_mapping = cola;
	skb->priority = 0;
	skb->protocol = datos ? 0 : 1;	/* (aquí: 1 = no cuenta para la escalera) */
	info = IEEE80211_SKB_CB(skb);
	memset(info, 0, sizeof(*info));
	info->flags = IEEE80211_TX_CTL_ASSIGN_SEQ | IEEE80211_TX_CTL_FIRST_FRAGMENT;
	info->band = NL80211_BAND_2GHZ;
	if (datos && nesc) {
		i0 = escalera[peld];
		i1 = escalera[peld > 1 ? peld - 2 : 0];
	} else {
		i0 = i1 = indice_de(vel ? vel : 2);
	}
	info->control.rates[0].idx = i0;
	info->control.rates[0].count = 4;
	info->control.rates[1].idx = i1;
	info->control.rates[1].count = 3;
	info->control.rates[2].idx = -1;
	info->control.rates[3].idx = -1;
	info->control.vif = interfaz;
	equipo->ops->tx(equipo, &ctl, skb);
	return 0;
}

static void brcm_sondear(void)
{
	int i;

	if (!arriba)
		return;
	/* la rutina de interrupción de brcmsmac, llamada por sondeo: si la
	 * tarjeta tiene algo, deja pendiente el trabajo que lo atiende */
	for (i = 0; i < 4; i++) {
		int algo = irq_rutina ? irq_rutina(0, irq_dato) : 0;
		hl_trabajos();
		if (algo != IRQ_HANDLED)
			break;
	}
}

static void brcm_barrido(int empieza)
{
	if (!arriba)
		return;
	if (empieza)
		equipo->ops->sw_scan_start(equipo, interfaz, brcm.mac);
	else
		equipo->ops->sw_scan_complete(equipo, interfaz);
}

/* El punto de acceso. Como mac80211: al elegirlo (antes de autenticar) se
 * dan su dirección, sus velocidades básicas, el intervalo de balizas y la
 * ranura; al asociar, solo que ya se está asociado (las velocidades básicas
 * no se pueden cambiar a la vez: brcmsmac las valida contra las de la
 * asociación, que aún no tiene). */
static void brcm_bss(const o8 *bssid, int asociado, int intervalo, int ranura_corta, const o8 *vels, int nvel)
{
	static const u8 nadie[6];
	struct ieee80211_bss_conf *b = &interfaz->bss_conf;
	u32 cambios;
	int i, j;

	if (!arriba)
		return;
	if (bssid && asociado) {
		b->assoc = 1;
		equipo->ops->bss_info_changed(equipo, interfaz, b, BSS_CHANGED_ASSOC);
		return;
	}
	cambios = BSS_CHANGED_BSSID;
	if (b->assoc) {
		b->assoc = 0;
		cambios |= BSS_CHANGED_ASSOC;
	}
	memcpy(bssid_ap, bssid ? bssid : nadie, 6);
	b->bssid = bssid_ap;
	if (bssid) {
		b->beacon_int = intervalo ? intervalo : 100;
		b->use_short_slot = ranura_corta;
		b->basic_rates = 0;
		nesc = 0;
		/* la escalera: las velocidades del punto de acceso, de menor a mayor */
		for (i = 0; i < 12; i++) {
			static const u8 orden[12] = { 0, 1, 2, 4, 3, 5, 6, 7, 8, 9, 10, 11 };
			int x = orden[i];
			for (j = 0; j < nvel; j++)
				if ((vels[j] & 0x7f) == vel_medios[x]) {
					if (x != 5)		/* 9 Mbit/s no gana nada a 6 ni a 12 */
						escalera[nesc++] = x;
					if (vels[j] & 0x80)
						b->basic_rates |= 1u << x;
					break;
				}
		}
		if (!nesc)
			escalera[nesc++] = 0;
		if (!b->basic_rates)
			b->basic_rates = 1u << escalera[0];
		/* se empieza a media escalera; lo demás lo dicen los acuses */
		peld = nesc / 2;
		seguidas_bien = seguidas_mal = 0;
		cambios |= BSS_CHANGED_BEACON_INT | BSS_CHANGED_ERP_SLOT | BSS_CHANGED_BASIC_RATES;
	}
	equipo->ops->bss_info_changed(equipo, interfaz, b, cambios);
}

static void brcm_parar(void)
{
	if (!arriba)
		return;
	arriba = 0;
	equipo->ops->remove_interface(equipo, interfaz);
	equipo->ops->stop(equipo);
	pci_apaga(&tarjeta);
	hl_log(2, "radio: tarjeta parada\n");
}

void brcm_cuentas(d32 *bien, d32 *mal, int *vel)
{
	*bien = tx_bien; *mal = tx_mal;
	*vel = nesc ? vel_medios[escalera[peld]] : 0;
}

/* ---- buscar la tarjeta en el bus PCI ---- */
static int busca_pci(void)
{
	static const u16 admitidas[] = { 0x4353, 0x4357, 0x4727, 0x0576, 0xa8d8, 0 };
	int bus, df, i, hallada = 0;

	for (bus = 0; bus < 256 && !hallada; bus++)
		for (df = 0; df < 256; df++) {
			u32 id = hl_pci_lee(bus, df, 0), clase;
			if (id == 0xffffffff || (id & 0xffff) == 0xffff) {
				if (!(df & 7))
					df += 7;	/* no hay aparato: no se miran sus funciones */
				continue;
			}
			clase = hl_pci_lee(bus, df, 8);
			if ((id & 0xffff) == 0x14e4 && !hallada) {
				for (i = 0; admitidas[i]; i++)
					if ((id >> 16) == admitidas[i])
						hallada = 1;
				if (hallada) {
					tarjeta.bus = &tarjeta.bus0;
					tarjeta.bus0.number = bus;
					tarjeta.devfn = df;
					tarjeta.vendor = id & 0xffff;
					tarjeta.device = id >> 16;
					tarjeta.revision = clase & 0xff;
				}
			}
			if (!(df & 7) && !(hl_pci_lee(bus, df, 0x0c) & 0x800000))
				df += 7;		/* aparato de una sola función */
		}
	return hallada ? 0 : -1;
}

/* Paso 1: encontrar la tarjeta en el bus PCI (solo mira: no la toca).
 * 1 si no hay ninguna de las admitidas. */
int brcm_busca(void)
{
	u32 b0, b1, sub;
	u16 cmd;

	if (busca_pci() < 0)
		return 1;
	pci_read_config_dword(&tarjeta, 0x10, &b0);
	pci_read_config_dword(&tarjeta, 0x14, &b1);
	pci_read_config_dword(&tarjeta, 0x2c, &sub);
	pci_read_config_word(&tarjeta, PCI_COMMAND, &cmd);
	pci_read_config_byte(&tarjeta, 0x3c, &tarjeta.irq);
	tarjeta.subsystem_vendor = sub & 0xffff;
	tarjeta.subsystem_device = sub >> 16;
	tarjeta.pcie_cap = pci_find_capability(&tarjeta, PCI_CAP_ID_EXP);
	hl_log(2, "Tarjeta Wi-Fi: %04X:%04X (subsistema %04X:%04X) en %02x:%02x.%d, BAR0 %08X%s, orden %04X, IRQ %d (no se usa), PCIe %s\n",
	       tarjeta.vendor, tarjeta.device, tarjeta.subsystem_vendor, tarjeta.subsystem_device,
	       tarjeta.bus0.number, tarjeta.devfn >> 3, tarjeta.devfn & 7, b0, (b0 & 6) == 4 ? " (64 bits)" : "",
	       cmd, tarjeta.irq, tarjeta.pcie_cap ? "sí" : "no");
	if ((b0 & 1) || !(b0 & ~0xfu) || ((b0 & 6) == 4 && b1)) {
		hl_log(0, "la BIOS no ha dado a la tarjeta una dirección de memoria que se pueda usar (BAR0 %08X, BAR1 %08X)\n", b0, b1);
		return -2;
	}
	{
		/* la configuración PCI entera de la tarjeta, para el registro */
		int i;
		for (i = 0; i < 0x40; i += 16)
			hl_log(2, "  PCI %02X: %08X %08X %08X %08X\n", i, hl_pci_lee(tarjeta.bus0.number, tarjeta.devfn, i),
			       hl_pci_lee(tarjeta.bus0.number, tarjeta.devfn, i + 4), hl_pci_lee(tarjeta.bus0.number, tarjeta.devfn, i + 8),
			       hl_pci_lee(tarjeta.bus0.number, tarjeta.devfn, i + 12));
	}
	tarjeta.bar0 = b0 & ~0xfu;
	brcm.nombre = tarjeta.device == 0x4353 ? "Broadcom BCM43224"
		    : tarjeta.device == 0x4727 ? "Broadcom BCM4313" : "Broadcom 802.11n";
	return 0;
}

/* Paso 2: despertarla, leer su bus interno (núcleos, SPROM) y montar el
 * controlador, sin encender la radio todavía. */
int brcm_monta(struct radio **r)
{
	const struct pci_device_id *id;
	int err;

	bcma_host_pci_init();
	hl_modinit_brcms_module_init();
	if (!puente || !ctl80211)
		return -3;
	for (id = puente->id_table; id->vendor; id++)
		if (id->vendor == tarjeta.vendor && id->device == tarjeta.device)
			break;
	err = puente->probe(&tarjeta, id);
	if (err || !equipo) {
		hl_log(0, "el controlador no se ha podido montar (%d)\n", err);
		return -4;
	}
	{
		struct brcms_info *wl = equipo->priv;
		struct brcms_c_info *wlc = wl->wlc;
		hl_log(2, "Núcleo 802.11 rev. %d; PHY tipo %d rev. %d; radio %04X rev. %d; bandas: %d; placa rev. %04X, indicadores %08X %08X\n",
		       wlc->hw->corerev, wlc->band->phytype, wlc->band->phyrev, wlc->band->radioid, wlc->band->radiorev,
		       wlc->pub->_nbands, wlc->hw->boardrev, wlc->hw->boardflags, wlc->hw->boardflags2);
	}
	hl_log(2, "Controlador montado. MAC %pM\n", brcm.mac);
	brcm.canal = brcm_canal;
	brcm.enviar = brcm_enviar;
	brcm.sondear = brcm_sondear;
	brcm.barrido = brcm_barrido;
	brcm.bss = brcm_bss;
	brcm.parar = brcm_parar;
	*r = &brcm;
	return 0;
}

/* (BCM4313) Lo que ha quedado del PHY LCN al encender, para WIFI.LOG: qué
 * placa dice la SPROM que es (amplificador de dentro o de fuera, Bluetooth
 * al lado: de eso sale la tabla de conmutación de la antena, como en Linux),
 * cómo se controla la potencia y lo que han dado las calibraciones. Solo
 * mira: no mide nada nuevo. */
static void brcm_lcn_resumen(void)
{
	struct brcms_info *wl = equipo->priv;
	struct brcms_c_info *wlc = wl->wlc;
	struct brcms_phy *pi;
	struct brcms_phy_lcnphy *lcn;
	struct lcnphy_cal_results *c;
	u32 bf = wlc->hw->boardflags;

	if (wlc->band->phytype != PHY_TYPE_LCN || !wlc->band->pi)
		return;
	pi = container_of(wlc->band->pi, struct brcms_phy, pubpi_ro);
	lcn = pi->u.pi_lcnphy;
	if (!lcn)
		return;
	c = &lcn->lcnphy_cal_results;
	hl_log(2, "PHY LCN rev. %d, radio 2064 rev. %d; amplificador %s; %s; tabla de la antena: %s\n",
	       pi->pubpi.phy_rev, pi->pubpi.radiorev, (bf & BFL_FEM) ? "de fuera (ePA)" : "de dentro (iPA)",
	       (bf & BFL_FEM_BT) ? "la SPROM dice que comparte la antena con el Bluetooth" : "sin Bluetooth al lado",
	       (bf & BFL_FEM) ? ((bf & BFL_FEM_BT) ? (wlc->hw->boardrev < 0x1250 ? "4313_bt_epa" : "4313_bt_epa_p250") : "4313_epa")
			      : ((bf & BFL_FEM_BT) ? "4313_bt_ipa" : "4313"));
	hl_log(2, "PHY LCN: potencia %s; índice de potencia %d; cristal %u kHz; temperatura (última medida) %d\n",
	       pi->hwpwrctrl ? "por TSSI (lazo cerrado)" : pi->temppwrctrl_capable ? "por la temperatura" : "fija",
	       wlc_lcnphy_get_current_tx_pwr_idx(pi), pi->xtalfreq / 1000, lcn->lcnphy_lastsensed_temperature);
	hl_log(2, "PHY LCN: calibración TX IQ a=%d b=%d, LO di/dq %04X, LO radio ei0=%u eq0=%u fi0=%u fq0=%u; RX IQ a0=%d b0=%d\n",
	       (s16)c->txiqlocal_a, (s16)c->txiqlocal_b, c->txiqlocal_didq, c->txiqlocal_ei0, c->txiqlocal_eq0,
	       c->txiqlocal_fi0, c->txiqlocal_fq0, (s16)c->rxiqcal_coeff_a0, (s16)c->rxiqcal_coeff_b0);
}

/* (HPRADIO) ¿la radio está apagada «por hardware»? Lo mismo que mira
 * brcmsmac (brcms_c_check_radio_disabled: el bit RFD, 1 << 16, del registro
 * phydebug, 158h, del núcleo 802.11), que es la patilla RF_DISABLE del chip:
 * en una tarjeta Mini PCI Express, la W_DISABLE# (la 20) del conector, que
 * mueve el equipo (en los HP, la BIOS, con la tecla del Wi-Fi). Lo que
 * Linux mira cada segundo mientras está apagada (rfkill_poll). 1 apagada,
 * 0 encendida, -1 sin tarjeta montada. */
int brcm_radio_apagada(void)
{
	struct brcms_info *wl;

	if (!equipo)
		return -1;
	wl = equipo->priv;
	return brcms_c_check_radio_disabled(wl->wlc) ? 1 : 0;
}

/* (HPRADIO) para WIFI.LOG: el bit RFD y las patillas GPIO del chip (por si
 * en alguna placa el apagado llega por una de ellas, y para comparar los
 * dos HP Mini), sin tocar nada */
void brcm_mira_patilla(const char *cuando)
{
	struct brcms_info *wl;
	struct bcma_drv_cc *cc;
	int a;

	if (!equipo)
		return;
	wl = equipo->priv;
	a = brcms_c_check_radio_disabled(wl->wlc);
	cc = &wl->wlc->hw->d11core->bus->drv_cc;
	hl_log(2, "Radio (%s): bit RFD de phydebug = %d (%s); GPIO del chip: entrada %08X, salida %08X, salidas activas %08X\n",
	       cuando, a, a ? "la patilla W_DISABLE# de la tarjeta dice APAGADA" : "encendida",
	       bcma_cc_read32(cc, BCMA_CC_GPIOIN), bcma_cc_read32(cc, BCMA_CC_GPIOOUT), bcma_cc_read32(cc, BCMA_CC_GPIOOUTEN));
}

/* (HPRADIO) la luz del Wi-Fi: en los HP (el Mini 5103) la enciende la
 * tarjeta, por una de sus patillas GPIO, la que dice la SPROM (ledbh0-3: el
 * comportamiento 3 es «la radio», con 80h, encendida a nivel bajo), como
 * brcms_led_register de Linux (led.c, con el GPIO de bcma): sin esto, la
 * luz se queda naranja o roja aunque la radio vaya. Solo si la SPROM la dice. */
static void brcm_luz(int encendida)
{
	struct brcms_info *wl = equipo->priv;
	struct bcma_bus *bus = wl->wlc->hw->d11core->bus;
	const u8 leds[4] = { bus->sprom.gpio0, bus->sprom.gpio1, bus->sprom.gpio2, bus->sprom.gpio3 };
	int i;

	for (i = 0; i < 4; i++)
		if (leds[i] != 0xff && (leds[i] & 0x7f) == 3)
			break;
	if (i == 4) {
		hl_log(2, "Luz del Wi-Fi: la SPROM no dice que ninguna patilla GPIO de la tarjeta lleve la luz de la radio (ledbh0-3 = %02X %02X %02X %02X): no se toca\n",
		       leds[0], leds[1], leds[2], leds[3]);
		return;
	}
	bcma_chipco_gpio_control(&bus->drv_cc, BIT(i), 0);
	bcma_chipco_gpio_outen(&bus->drv_cc, BIT(i), BIT(i));
	bcma_chipco_gpio_out(&bus->drv_cc, BIT(i), (!!encendida ^ !!(leds[i] & 0x80)) ? BIT(i) : 0);
	hl_log(2, "Luz del Wi-Fi: la de la patilla GPIO %d de la tarjeta (ledbh%d = %02X%s), %s\n", i, i, leds[i],
	       leds[i] & 0x80 ? ", a nivel bajo" : "", encendida ? "ENCENDIDA (blanca o azul)" : "apagada");
}

/* Paso 3: encender (microcódigo, PHY y radio), modo estación, canal 1. */
int brcm_enciende(void)
{
	static const struct ieee80211_tx_queue_params cola_normal = { .txop = 0, .cw_min = 15, .cw_max = 1023, .aifs = 2 };
	struct ieee80211_supported_band *sb = equipo->wiphy->bands[NL80211_BAND_2GHZ];
	unsigned int filtro = FIF_BCN_PRBRESP_PROMISC;
	int err, i;

	hl_log(2, "Encendiendo la radio (microcódigo, PHY %s, calibración)...\n",
	       ((struct brcms_info *)equipo->priv)->wlc->band->phytype == PHY_TYPE_LCN ? "LCN" : "N");
	/* como mac80211: el canal y los límites tienen que estar puestos antes
	 * de arrancar (brcms_c_init los lee) */
	equipo->conf.chandef.chan = &sb->channels[0];
	equipo->conf.chandef.width = NL80211_CHAN_WIDTH_20_NOHT;
	equipo->conf.power_level = sb->channels[0].max_power;
	equipo->conf.listen_interval = 1;
	equipo->conf.short_frame_max_tx_count = 7;
	equipo->conf.long_frame_max_tx_count = 4;
	err = equipo->ops->start(equipo);
	if (err == -ERFKILL) {
		brcm_mira_patilla("al encender");
		hl_log(0, "la radio está APAGADA por el interruptor o el botón del Wi-Fi del equipo: enciéndalo y vuelva a cargar HSPWIFI\n");
		return -10;
	}
	if (err) {
		hl_log(0, "la radio no se enciende (%d)\n", err);
		return -11;
	}
	interfaz = hl_zalloc(sizeof(*interfaz) + 64);
	interfaz->type = NL80211_IFTYPE_STATION;
	memcpy(interfaz->addr, brcm.mac, 6);
	memcpy(estacion.addr, brcm.mac, 6);
	err = equipo->ops->add_interface(equipo, interfaz);
	if (err)
		return -12;
	arriba = 1;
	for (i = 0; i < sb->n_channels; i++)
		hl_log(3, "  canal %d: %d MHz, indicadores %x, potencia %d dBm\n", sb->channels[i].hw_value,
		       sb->channels[i].center_freq, sb->channels[i].flags, sb->channels[i].max_power);
	equipo->conf.chandef.chan = &sb->channels[0];
	equipo->conf.chandef.width = NL80211_CHAN_WIDTH_20_NOHT;
	equipo->conf.power_level = sb->channels[0].max_power;
	equipo->conf.listen_interval = 1;
	equipo->conf.short_frame_max_tx_count = 7;
	equipo->conf.long_frame_max_tx_count = 4;
	err = equipo->ops->config(equipo, IEEE80211_CONF_CHANGE_CHANNEL | IEEE80211_CONF_CHANGE_POWER |
				  IEEE80211_CONF_CHANGE_RETRY_LIMITS | IEEE80211_CONF_CHANGE_LISTEN_INTERVAL);
	if (err)
		hl_log(1, "radio: la configuración inicial devuelve %d\n", err);
	equipo->ops->configure_filter(equipo, filtro, &filtro, 0);
	for (i = 0; i < 4; i++)
		equipo->ops->conf_tx(equipo, interfaz, i, &cola_normal);
	{
		struct brcms_info *wl = equipo->priv;
		hl_log(2, "Microcódigo en marcha: versión %d.%d; potencia pedida %d dBm\n",
		       wl->wlc->ucode_rev >> 16, wl->wlc->ucode_rev & 0xffff, equipo->conf.power_level);
	}
	brcm_lcn_resumen();
	brcm_luz(1);
	hl_log(2, "Radio encendida en el canal 1\n");
	return 0;
}
