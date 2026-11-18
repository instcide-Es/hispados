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

/* el PHY LCN (BCM4313) no se incluye: solo el PHY N */
struct brcms_phy;
bool wlc_phy_attach_lcnphy(struct brcms_phy *pi) { hl_log(0, "esta tarjeta lleva el PHY LCN (BCM4313), que no está incluido\n"); return false; }
void wlc_lcnphy_calib_modes(struct brcms_phy *pi, uint mode) { }
void wlc_lcnphy_deaf_mode(struct brcms_phy *pi, bool mode) { }
s8 wlc_lcnphy_get_tssi(struct brcms_phy *pi, s8 *ofdm, s8 *cck) { return 0; }
int wlc_lcnphy_rx_signal_power(struct brcms_phy *pi, s32 gain) { return 0; }
s8 wlc_lcnphy_tempsense_degree(struct brcms_phy *pi, bool mode) { return 0; }
s8 wlc_lcnphy_vbatsense(struct brcms_phy *pi, bool mode) { return 0; }
bool wlc_phy_tpc_isenabled_lcnphy(struct brcms_phy *pi) { return false; }

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
	hl_log(2, "Chip BCM%u (0x%04X), revisión %u, encapsulado %u\n",
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
	for (i = 0; i < 4; i++) {
		if (irq_rutina)
			irq_rutina(0, irq_dato);
		hl_trabajos();
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

static void brcm_bss(const o8 *bssid, int asociado, int intervalo, int ranura_corta, const o8 *vels, int nvel)
{
	static const u8 nadie[6];
	struct ieee80211_bss_conf *b = &interfaz->bss_conf;
	u32 cambios = BSS_CHANGED_BSSID | BSS_CHANGED_ASSOC;
	int i, j;

	if (!arriba)
		return;
	memcpy(bssid_ap, bssid ? bssid : nadie, 6);
	b->bssid = bssid_ap;
	b->assoc = asociado;
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
			if ((clase >> 24) == 0x02 || (id & 0xffff) == 0x14e4)
				hl_log(2, "PCI %02x:%02x.%d: %04X:%04X clase %06X rev. %02X\n", bus, df >> 3, df & 7,
				       id & 0xffff, id >> 16, clase >> 8, clase & 0xff);
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

/* Paso 1: encontrar la tarjeta, leer su bus interno y montar el controlador
 * (sin encender la radio todavía). */
int brcm_monta(struct radio **r)
{
	const struct pci_device_id *id;
	u32 b0, b1, sub;
	u16 cmd;
	int err;

	if (busca_pci() < 0) {
		hl_log(0, "no hay ninguna tarjeta Wi-Fi Broadcom de las admitidas (BCM43224 14E4:4353, BCM43225 14E4:4357)\n");
		return -1;
	}
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
	tarjeta.bar0 = b0 & ~0xfu;
	brcm.nombre = tarjeta.device == 0x4353 ? "Broadcom BCM43224" : "Broadcom 802.11n";

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

/* Paso 2: encender (microcódigo, PHY y radio), modo estación, canal 1. */
int brcm_enciende(void)
{
	static const struct ieee80211_tx_queue_params cola_normal = { .txop = 0, .cw_min = 15, .cw_max = 1023, .aifs = 2 };
	struct ieee80211_supported_band *sb = equipo->wiphy->bands[NL80211_BAND_2GHZ];
	unsigned int filtro = FIF_BCN_PRBRESP_PROMISC;
	int err, i;

	hl_log(2, "Encendiendo la radio (microcódigo, PHY N, calibración)...\n");
	err = equipo->ops->start(equipo);
	if (err == -ERFKILL) {
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
	hl_log(2, "Radio encendida en el canal 1\n");
	return 0;
}
