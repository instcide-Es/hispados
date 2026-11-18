/*
 * radio_ath9k.c - La radio de las Atheros 802.11n de la familia AR9002
 * (AR9285, la del Toshiba NB250; también AR9280 y AR9287) para el Wi-Fi de
 * Hispa-486, con la capa de la tarjeta de ath9k de Linux (ath9k_hw, en
 * wifi/linux/ath/ath9k/, sin tocar): registros, valores de arranque, PHY,
 * calibración, EEPROM, potencias y descriptores de DMA.
 *
 * Lo que en Linux hace el resto de ath9k (el módulo que habla con mac80211:
 * pci.c, init.c, main.c, recv.c, xmit.c, link.c) está aquí, reducido a una
 * estación de una red de casa:
 *   - el alta: buscar la tarjeta, leer su EEPROM y montar ath9k_hw;
 *   - el arranque y el cambio de canal (ath_reset de main.c);
 *   - la recepción: un anillo de descriptores con el «descriptor de
 *     reserva» de recv.c (la tarjeta nunca se queda sin uno al que saltar);
 *   - el envío: una cola (la de datos, BE) con el «descriptor retenido» de
 *     xmit.c (la tarjeta vuelve a leer el enlace del último que mandó);
 *   - la calibración periódica y el ANI (link.c) mientras hay conexión;
 *   - la interfaz «struct radio» de w80211.c, con la misma escalera de
 *     velocidades que radio_brcm.c.
 * Sin cifrado por la tarjeta (CCMP va por programa en wpa.c) ni agregación.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "hispa_linux.h"
#include "base.h"
#include "hw.h"
#include "hw-ops.h"
#include "common.h"
#include "ath9k.h"
#include "nucleo.h"

/* ---------------------------------------------------------------- estado */
/* De ath/main.c de Linux (no se copia entero): ¿es la baliza de nuestro
   punto de acceso? */
bool ath_is_mybeacon(struct ath_common *common, struct ieee80211_hdr *hdr)
{
	return ieee80211_is_beacon(hdr->frame_control) &&
	       !is_zero_ether_addr(common->curbssid) &&
	       ether_addr_equal_64bits(hdr->addr3, common->curbssid);
}

static struct pci_dev tarjeta;
static struct ath_hw hw_ath;
static struct ath_hw *const ah = &hw_ath;
static struct ieee80211_hw equipo;
static struct ath9k_hw_cal_data caldata;
static struct radio ath;
static void __iomem *mem;
static int arriba, montada;
static int qnum = -1;			/* la cola de envío (BE) */
static u16 seq_tx;
static u8 bssid_ap[6];
static int asociado;
static u32 tx_bien, tx_mal, rx_bien, rx_malas, reinicios, rx_sin_sitio;
static u32 isr_fatal, isr_rxorn, isr_rxeol;
static unsigned long t_long, t_short, t_ani, t_reset_cal;

/* la escalera de velocidades (índices de ath9k_legacy_rates: 1, 2, 5,5,
 * 11, 6, 9, 12, 18, 24, 36, 48 y 54 Mbit/s), como en radio_brcm.c */
static const u8 vel_medios[12] = { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 };
static u8 escalera[12];
static int nesc, peld;
static int seguidas_bien, seguidas_mal;

/* ------------------------------------------------- anillos de DMA */
#define NRX 32
#define NTX 16
#define RXTAM 2048		/* una trama de 1500 con cabeceras, CCMP y FCS */
#define TXTAM 2432
#define DTAM 128		/* sitio de cada descriptor (no cruza 4 KB) */
static u8 *dma_zona;
static dma_addr_t dma_fis;
static struct ath_desc *rx_d[NRX], *tx_d[NTX];
static u8 *rx_b[NRX], *tx_b[NTX];
/* recepción: los índices en el orden en que la tarjeta los usa (recv.c) */
static int rx_lista[NRX], rx_cab, rx_n;
static int rx_retenido = -1;		/* sc->rx.buf_hold */
static u32 *rx_enlace;			/* sc->rx.rxlink */
/* envío: un anillo; tx_cab es el más viejo que está en la tarjeta (puede
 * ser el retenido), tx_fin el siguiente libre */
static int tx_cab, tx_fin, tx_retenido;
static u8 tx_datos[NTX];		/* cuenta para la escalera */
static unsigned long tx_cuando[NTX];
static struct ath_desc *tx_enlace;	/* txq->axq_link */

static inline u32 fis(const void *p) { return hl_fis(p); }

/* ---------------------------------------------------- registros y bus */
static unsigned int ath_lee(void *x, u32 off) { return ioread32((u8 *)mem + off); }
static void ath_escribe(void *x, u32 v, u32 off) { iowrite32(v, (u8 *)mem + off); }
static void ath_lee_varios(void *x, u32 *dir, u32 *val, u16 n)
{
	int i;
	for (i = 0; i < n; i++)
		val[i] = ath_lee(x, dir[i]);
}
static u32 ath_rmw(void *x, u32 off, u32 pon, u32 quita)
{
	u32 v = ath_lee(x, off);
	v &= ~quita;
	v |= pon;
	ath_escribe(x, v, off);
	return v;
}

/* la EEPROM, palabra a palabra (lo de ath_pci_eeprom_read de pci.c) */
static bool ath_eeprom_lee(struct ath_common *c, u32 off, u16 *dato)
{
	ah->reg_ops.read(ah, AR5416_EEPROM_OFFSET + (off << AR5416_EEPROM_S));
	if (!ath9k_hw_wait(ah, AR_EEPROM_STATUS_DATA,
			   AR_EEPROM_STATUS_DATA_BUSY | AR_EEPROM_STATUS_DATA_PROT_ACCESS, 0, AH_WAIT_TIMEOUT)) {
		hl_log(0, "ath9k: la EEPROM no contesta (palabra %u)\n", off);
		return false;
	}
	*dato = MS(ah->reg_ops.read(ah, AR_EEPROM_STATUS_DATA), AR_EEPROM_STATUS_DATA_VAL);
	return true;
}
static void ath_cache(struct ath_common *c, int *csz)
{
	u8 v;
	pci_read_config_byte(&tarjeta, PCI_CACHE_LINE_SIZE, &v);
	*csz = v ? v : 16;		/* en palabras de 4 octetos */
}
static const struct ath_bus_ops bus_pci = {
	.ath_bus_type = ATH_PCI,
	.read_cachesize = ath_cache,
	.eeprom_read = ath_eeprom_lee,
};

/* lo que dice ath9k_hw por ath_err/ath_info/ath_dbg (ath/main.c) */
void ath_printk(const char *nivel, const struct ath_common *c, const char *f, ...)
{
	va_list ap;
	int n = 2;

	if (nivel[0] == '\001')
		n = nivel[1] <= '3' ? 0 : nivel[1] == '4' ? 1 : nivel[1] == '7' ? 3 : 2;
	hl_linea();
	hl_log(n, "ath9k: ");
	va_start(ap, f);
	hl_vlog(n, f, ap);
	va_end(ap);
	hl_linea();
}

/* las tarjetas de la familia AR9003 y posteriores no van incluidas: lo que
 * la capa de la tarjeta llama de ellas no se usa nunca con una AR9002 */
#define NO_AR9003(f) { hl_log(0, "ath9k: %s (AR9003) no está incluido\n", f); }
void ar9003_hw_attach_ops(struct ath_hw *a) NO_AR9003("ar9003_hw_attach_ops")
void ar9003_hw_bb_watchdog_config(struct ath_hw *a) { }
void ar9003_hw_disable_phy_restart(struct ath_hw *a) { }
void ar9003_hw_internal_regulator_apply(struct ath_hw *a) { }
void ath9k_hw_reset_txstatus_ring(struct ath_hw *a) { }
void ath9k_hw_set_rx_bufsize(struct ath_hw *a, u16 n) { }
const struct eeprom_ops eep_ar9300_ops;

/* ------------------------------------------------- buscar la tarjeta */
static const u16 admitidas[] = { 0x002B, 0x002A, 0x002E, 0x0029, 0x002D, 0x0024, 0x0023, 0x0027, 0x002C, 0 };

const char *ath_nombre_modelo(u16 dev)
{
	switch (dev) {
	case 0x002B: return "Atheros AR9285";
	case 0x002A: return "Atheros AR9280";
	case 0x002E: return "Atheros AR9287";
	case 0x002D: return "Atheros AR9287 (PCI)";
	case 0x0029: return "Atheros AR9280 (PCI)";
	case 0x0024: return "Atheros AR5418";
	case 0x0023: case 0x0027: return "Atheros AR5416";
	}
	return "Atheros 802.11n";
}

/* Paso 1: ¿está? (solo mira; 1 si no hay ninguna) */
int ath_busca(void)
{
	int bus, df, i;
	u32 b0, sub;
	u16 cmd;

	for (bus = 0; bus < 256; bus++)
		for (df = 0; df < 256; df++) {
			u32 id = hl_pci_lee(bus, df, 0);
			if (id == 0xffffffff || (id & 0xffff) == 0xffff) {
				if (!(df & 7))
					df += 7;
				continue;
			}
			if ((id & 0xffff) == 0x168c)
				for (i = 0; admitidas[i]; i++)
					if ((id >> 16) == admitidas[i])
						goto hallada;
			if (!(df & 7) && !(hl_pci_lee(bus, df, 0x0c) & 0x800000))
				df += 7;
		}
	return 1;
hallada:
	memset(&tarjeta, 0, sizeof(tarjeta));
	tarjeta.bus = &tarjeta.bus0;
	tarjeta.bus0.number = bus;
	tarjeta.devfn = df;
	tarjeta.vendor = 0x168c;
	tarjeta.device = hl_pci_lee(bus, df, 0) >> 16;
	tarjeta.revision = hl_pci_lee(bus, df, 8) & 0xff;
	pci_read_config_dword(&tarjeta, 0x10, &b0);
	pci_read_config_dword(&tarjeta, 0x2c, &sub);
	pci_read_config_word(&tarjeta, PCI_COMMAND, &cmd);
	pci_read_config_byte(&tarjeta, 0x3c, &tarjeta.irq);
	tarjeta.subsystem_vendor = sub & 0xffff;
	tarjeta.subsystem_device = sub >> 16;
	tarjeta.pcie_cap = pci_find_capability(&tarjeta, PCI_CAP_ID_EXP);
	hl_log(2, "Tarjeta Wi-Fi: %s, %04X:%04X rev. %02X (subsistema %04X:%04X) en %02x:%02x.%d, BAR0 %08X, orden %04X, IRQ %d (no se usa), PCIe %s\n",
	       ath_nombre_modelo(tarjeta.device), tarjeta.vendor, tarjeta.device, tarjeta.revision,
	       tarjeta.subsystem_vendor, tarjeta.subsystem_device, bus, df >> 3, df & 7, b0, cmd, tarjeta.irq,
	       tarjeta.pcie_cap ? "sí" : "no");
	for (i = 0; i < 0x40; i += 16)
		hl_log(2, "  PCI %02X: %08X %08X %08X %08X\n", i, hl_pci_lee(bus, df, i), hl_pci_lee(bus, df, i + 4),
		       hl_pci_lee(bus, df, i + 8), hl_pci_lee(bus, df, i + 12));
	if ((b0 & 1) || !(b0 & ~0xfu) || ((b0 & 6) == 4 && hl_pci_lee(bus, df, 0x14))) {
		hl_log(0, "la BIOS no ha dado a la tarjeta una dirección de memoria que se pueda usar (BAR0 %08X)\n", b0);
		return -2;
	}
	tarjeta.bar0 = b0 & ~0xfu;
	ath.nombre = ath_nombre_modelo(tarjeta.device);
	return 0;
}

/* ------------------------------------------------------- la recepción */
static void rx_enlaza(int i)		/* ath_rx_buf_link */
{
	struct ath_desc *d = rx_d[i];

	d->ds_link = 0;
	d->ds_data = fis(rx_b[i]);
	d->ds_vdata = rx_b[i];
	ath9k_hw_setuprxdesc(ah, d, RXTAM, 0);
	if (rx_enlace)
		*rx_enlace = fis(d);
	else
		ath9k_hw_putrxbuf(ah, fis(d));
	rx_enlace = &d->ds_link;
}

static void rx_vuelve(int i)		/* ath_rx_buf_relink: el anterior, a la tarjeta */
{
	if (rx_retenido >= 0)
		rx_enlaza(rx_retenido);
	rx_retenido = i;
}

static void filtro_y_modo(void)		/* ath_opmode_init */
{
	ath9k_hw_setrxfilter(ah, ATH9K_RX_FILTER_UCAST | ATH9K_RX_FILTER_BCAST |
			     ATH9K_RX_FILTER_MCAST | ATH9K_RX_FILTER_BEACON);
	ath_hw_setbssidmask(ath9k_hw_common(ah));
	ath9k_hw_setopmode(ah);
	ath9k_hw_setmcastfilter(ah, ~0u, ~0u);
}

static void rx_arranca(void)		/* ath_startrecv */
{
	int i;

	rx_retenido = -1;
	rx_enlace = NULL;
	rx_cab = 0;
	rx_n = NRX;
	for (i = 0; i < NRX; i++) {
		rx_lista[i] = i;
		rx_enlaza(i);
	}
	ath9k_hw_putrxbuf(ah, fis(rx_d[0]));
	ath9k_hw_rxena(ah);
	filtro_y_modo();
	ath9k_hw_startpcureceive(ah, false);
}

static void rx_para(void)		/* ath_stoprecv */
{
	bool reset = false;

	ath9k_hw_abortpcurecv(ah);
	ath9k_hw_setrxfilter(ah, 0);
	if (!ath9k_hw_stopdmarecv(ah, &reset))
		hl_log(1, "ath9k: la recepción por DMA no se para\n");
	rx_enlace = NULL;
}

/* el siguiente descriptor que la tarjeta ha llenado (ath_get_next_rx_buf);
 * -1 si no hay */
static int rx_siguiente(struct ath_rx_status *rs)
{
	int i, j;

	if (!rx_n)
		return -1;
	i = rx_lista[rx_cab];
	if (i == rx_retenido)
		return -1;
	if (ath9k_hw_rxprocdesc(ah, rx_d[i], rs) == -EINPROGRESS) {
		struct ath_rx_status trs;
		if (rx_n < 2)
			return -1;
		j = rx_lista[(rx_cab + 1) % NRX];
		memset(&trs, 0, sizeof(trs));
		if (ath9k_hw_rxprocdesc(ah, rx_d[j], &trs) == -EINPROGRESS)
			return -1;
		/* el siguiente está hecho y éste no: éste está estropeado */
		if (ath9k_hw_rxprocdesc(ah, rx_d[i], rs) == -EINPROGRESS) {
			rs->rs_datalen = 0;
			rs->rs_more = true;
		}
	}
	rx_cab = (rx_cab + 1) % NRX;
	rx_n--;
	return i;
}

static void rx_procesa(void)
{
	struct ath_common *c = ath9k_hw_common(ah);
	static int descarta;
	int vueltas;

	for (vueltas = 0; vueltas < NRX; vueltas++) {
		struct ath_rx_status rs;
		struct ieee80211_rx_status rxs;
		struct ieee80211_hdr *h;
		bool err_cifrado = false;
		int i, n, cab, pad;
		u8 *b;

		memset(&rs, 0, sizeof(rs));
		i = rx_siguiente(&rs);
		if (i < 0)
			break;
		b = rx_b[i];
		memset(&rxs, 0, sizeof(rxs));
		n = rs.rs_datalen;
		if (descarta || rs.rs_more || n < 10 || n > RXTAM ||
		    (rs.rs_status & (ATH9K_RXERR_PHY | ATH9K_RXERR_CORRUPT_DESC))) {
			descarta = rs.rs_more;
			if (!(rs.rs_status & ATH9K_RXERR_PHY))
				rx_malas++;
			goto devuelve;
		}
		h = (struct ieee80211_hdr *)b;
		if (!ath9k_cmn_rx_accept(c, h, &rxs, &rs, &err_cifrado, 0)) {
			rx_malas++;
			goto devuelve;
		}
		rs.is_mybeacon = ath_is_mybeacon(c, h);
		ath9k_cmn_process_rssi(c, &equipo, &rs, &rxs);
		/* la tarjeta rellena la cabecera hasta múltiplo de 4 (QoS: 26 -> 28) */
		cab = ieee80211_hdrlen(h->frame_control);
		pad = cab & 3;
		if (pad && n >= cab + pad + FCS_LEN) {
			memmove(b + pad, b, cab);
			b += pad;
			n -= pad;
		}
		rx_bien++;
		if (!(rxs.flag & RX_FLAG_FAILED_FCS_CRC) && n > FCS_LEN)
			w80211_rx(b, n - FCS_LEN, (rxs.flag & RX_FLAG_NO_SIGNAL_VAL) ? -95 : rxs.signal,
				  ah->curchan ? ieee80211_frequency_to_channel(ah->curchan->channel) : 0);
devuelve:
		/* al final de la lista y a la tarjeta (con el de reserva) */
		rx_lista[(rx_cab + rx_n) % NRX] = i;
		rx_n++;
		rx_vuelve(i);
		ath9k_hw_rxena(ah);
	}
}

/* ----------------------------------------------------------- el envío */
static void tx_hecho(int i, int bien)
{
	if (bien)
		tx_bien++;
	else
		tx_mal++;
	if (!tx_datos[i])
		return;
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

/* lo que la tarjeta ya ha mandado (ath_tx_processq) */
static void tx_procesa(void)
{
	while (tx_cab != tx_fin) {
		struct ath_tx_status ts;
		int i = tx_cab;

		if (tx_retenido) {
			/* el retenido se suelta cuando acaba el siguiente */
			if ((i + 1) % NTX == tx_fin)
				break;
			i = (i + 1) % NTX;
		}
		memset(&ts, 0, sizeof(ts));
		if (ath9k_hw_txprocdesc(ah, tx_d[i], &ts) == -EINPROGRESS)
			break;
		if (tx_retenido)
			tx_cab = i;		/* el viejo retenido queda libre */
		tx_retenido = 1;
		tx_hecho(i, !(ts.ts_status & ATH9K_TXERR_MASK));
		if (ts.ts_status & ATH9K_TXERR_MASK)
			hl_log(3, "ath9k: envío sin acuse (estado %02X, reintentos %u)\n", ts.ts_status, ts.ts_longretry);
	}
}

/* parar el envío y dar por perdidas las tramas pendientes (ath_drain_all_txq) */
static void tx_vacia(void)
{
	ath9k_hw_abort_tx_dma(ah);
	if (tx_retenido && tx_cab != tx_fin)
		tx_cab = (tx_cab + 1) % NTX;
	while (tx_cab != tx_fin) {
		tx_hecho(tx_cab, 0);
		tx_cab = (tx_cab + 1) % NTX;
	}
	tx_retenido = 0;
	tx_enlace = NULL;
}

static int indice_de(int medios)
{
	int i;
	for (i = 0; i < 12; i++)
		if (vel_medios[i] == medios)
			return i;
	return 0;
}

static void pon_serie(struct ath_tx_info *in, int k, int rix, int veces, int n)
{
	struct ath_common *c = ath9k_hw_common(ah);
	const struct ieee80211_rate *r = &c->sbands[NL80211_BAND_2GHZ].bitrates[rix];
	int phy = (r->flags & IEEE80211_RATE_ERP_G) ? WLAN_RC_PHY_OFDM : WLAN_RC_PHY_CCK;

	in->rates[k].Tries = veces;
	in->rates[k].Rate = r->hw_value;
	in->rates[k].ChSel = ah->txchainmask;
	in->rates[k].PktDuration = ath9k_hw_computetxtime(ah, phy, r->bitrate * 100, n, rix, false);
	in->txpower[k] = MAX_RATE_POWER;
}

static int ath_enviar(const o8 *trama, int n, int vel)
{
	struct ath_tx_info in;
	struct ath_desc *d;
	u16 fc;
	int i, cab, pad, datos = vel == 0;
	u8 *b;

	if (!arriba || n < 10 || n + 4 > TXTAM)
		return -1;
	tx_procesa();
	i = tx_fin;
	if ((i + 1) % NTX == tx_cab)
		return -1;			/* cola llena */
	b = tx_b[i];
	d = tx_d[i];
	fc = trama[0] | (trama[1] << 8);
	cab = ieee80211_hdrlen(fc);
	pad = (n > cab) ? (cab & 3) : 0;
	/* la cabecera, rellena hasta múltiplo de 4 (la tarjeta quita el relleno) */
	memcpy(b, trama, cab);
	memset(b + cab, 0, pad);
	memcpy(b + cab + pad, trama + cab, n - cab);
	if (!ieee80211_is_ctl(fc)) {		/* el número de secuencia (como ath9k) */
		seq_tx += 0x10;
		b[22] = (b[22] & 0x0f) | (seq_tx & 0xf0);
		b[23] = seq_tx >> 8;
	}
	memset(&in, 0, sizeof(in));
	in.is_first = in.is_last = true;
	in.qcu = qnum;
	in.type = ieee80211_is_beacon(fc) ? ATH9K_PKT_TYPE_BEACON :
		  ieee80211_is_probe_resp(fc) ? ATH9K_PKT_TYPE_PROBE_RESP :
		  ieee80211_is_pspoll(fc) ? ATH9K_PKT_TYPE_PSPOLL : ATH9K_PKT_TYPE_NORMAL;
	in.flags = ATH9K_TXDESC_INTREQ | ATH9K_TXDESC_CLRDMASK;
	if (trama[4] & 1)
		in.flags |= ATH9K_TXDESC_NOACK;
	in.link = 0;
	in.pkt_len = n + FCS_LEN;
	in.buf_addr[0] = fis(b);
	in.buf_len[0] = n + pad;
	in.keyix = ATH9K_TXKEYIX_INVALID;
	in.keytype = ATH9K_KEY_TYPE_CLEAR;
	in.dur_update = !ieee80211_is_pspoll(fc);
	if (datos && nesc) {
		pon_serie(&in, 0, escalera[peld], 4, n + FCS_LEN);
		pon_serie(&in, 1, escalera[peld > 1 ? peld - 2 : 0], 3, n + FCS_LEN);
		pon_serie(&in, 2, escalera[0], 2, n + FCS_LEN);
	} else {
		pon_serie(&in, 0, indice_de(vel ? vel : 2), 4, n + FCS_LEN);
		pon_serie(&in, 1, indice_de(vel ? vel : 2), 3, n + FCS_LEN);
	}
	ath9k_hw_set_txdesc(ah, d, &in);
	tx_datos[i] = datos;
	tx_cuando[i] = jiffies;
	tx_fin = (i + 1) % NTX;
	/* ath_tx_txqaddbuf: detrás del último o, si no hay, en el registro */
	if (tx_enlace)
		ath9k_hw_set_desc_link(ah, tx_enlace, fis(d));
	else
		ath9k_hw_puttxbuf(ah, qnum, fis(d));
	tx_enlace = d;
	ath9k_hw_txstart(ah, qnum);
	return 0;
}

/* ---------------------------------------- arranque y cambio de canal */
static void pon_interrupciones(void)
{
	/* la máscara de ath9k_start; la línea de la tarjeta sigue apagada en
	 * el PCI: el núcleo mira la causa por sondeo */
	ah->imask = ATH9K_INT_TX | ATH9K_INT_RXEOL | ATH9K_INT_RXORN | ATH9K_INT_FATAL |
		    ATH9K_INT_GLOBAL | ATH9K_INT_RX;
	if (ah->config.hw_hang_checks & HW_BB_WATCHDOG)
		ah->imask |= ATH9K_INT_BB_WATCHDOG;
	if (ah->caps.hw_caps & ATH9K_HW_CAP_HT)
		ah->imask |= ATH9K_INT_CST;
	ath9k_hw_set_interrupts(ah);
	ath9k_hw_enable_interrupts(ah);
}

static void estado_bss(void)		/* ath9k_calculate_summary_state, una estación */
{
	struct ath_common *c = ath9k_hw_common(ah);

	eth_broadcast_addr(c->bssidmask);
	ath_hw_setbssidmask(c);
	ah->opmode = NL80211_IFTYPE_STATION;
	ath9k_hw_setopmode(ah);
	memcpy(c->curbssid, bssid_ap, 6);
	c->curaid = 0;
	ath9k_hw_write_associd(ah);
}

static void completa(void)		/* ath_complete_reset */
{
	u16 cur = 0;

	estado_bss();
	rx_arranca();
	if (ah->curchan)
		ath9k_cmn_update_txpow(ah, 0, 2 * ah->curchan->chan->max_power, &cur);
	pon_interrupciones();
}

static int reinicia(struct ath9k_channel *c)	/* ath_reset_internal */
{
	int r;

	ath9k_hw_disable_interrupts(ah);
	tx_vacia();
	rx_para();
	r = ath9k_hw_reset(ah, c, &caldata, false);
	if (r) {
		hl_log(0, "ath9k: no se puede reiniciar la tarjeta en %u MHz (%d)\n", c->channel, r);
		return r;
	}
	completa();
	reinicios++;
	return 0;
}

static struct ath9k_channel *canal_hw(int c)
{
	struct ath_common *co = ath9k_hw_common(ah);
	struct ieee80211_supported_band *sb = &co->sbands[NL80211_BAND_2GHZ];
	struct cfg80211_chan_def cd;

	if (c < 1 || c > sb->n_channels)
		return NULL;
	if (sb->channels[c - 1].flags & IEEE80211_CHAN_DISABLED)
		return NULL;
	memset(&cd, 0, sizeof(cd));
	cd.chan = &sb->channels[c - 1];
	cd.width = NL80211_CHAN_WIDTH_20_NOHT;
	cd.center_freq1 = cd.chan->center_freq;
	equipo.conf.chandef = cd;
	return ath9k_cmn_get_channel(&equipo, ah, &cd);
}

static int ath_canal(int c)
{
	struct ath9k_channel *h;

	if (!arriba)
		return -1;
	h = canal_hw(c);
	if (!h)
		return -1;
	if (h == ah->curchan)
		return 0;
	return reinicia(h);
}

/* la calibración periódica y el ANI (ath_ani_calibrate de link.c): solo con
 * conexión, como ath9k */
static void calibra(void)
{
	struct ath_common *c = ath9k_hw_common(ah);
	unsigned long t = jiffies;
	bool larga = false, corta = false;
	int r;

	if (!asociado)
		return;
	if (t - t_long >= ATH_LONG_CALINTERVAL) {
		larga = true;
		t_long = t;
	}
	if (!c->ani.caldone) {
		if (t - t_short >= ATH_STA_SHORT_CALINTERVAL) {
			corta = true;
			t_short = t_reset_cal = t;
		}
	} else if (t - t_reset_cal >= ATH_RESTART_CALINTERVAL) {
		c->ani.caldone = ath9k_hw_reset_calvalid(ah);
		if (c->ani.caldone)
			t_reset_cal = t;
	}
	if (t - t_ani >= ah->config.ani_poll_interval) {
		t_ani = t;
		ath9k_hw_ani_monitor(ah, ah->curchan);
	}
	if (larga || corta) {
		r = ath9k_hw_calibrate(ah, ah->curchan, ah->rxchainmask, larga);
		if (r < 0) {
			hl_log(1, "ath9k: la calibración pide reiniciar la tarjeta\n");
			c->ani.caldone = 0;
			reinicia(ah->curchan);
			return;
		}
		c->ani.caldone = r;
		hl_log(3, "ath9k: calibración %s%s: %s, ruido %d dBm\n", larga ? "larga " : "", corta ? "corta" : "",
		       r ? "hecha" : "sigue", ah->noise);
	}
}

static void ath_sondear(void)
{
	u32 st = 0, sync = 0;

	if (!arriba)
		return;
	if (ath9k_hw_intrpend(ah)) {
		ath9k_hw_getisr(ah, (enum ath9k_int *)&st, &sync);
		st &= ah->imask;
		if (st & ATH9K_INT_FATAL) {
			if (isr_fatal++ < 4)
				hl_log(1, "ath9k: error grave de la tarjeta (interrupción %08X, causa %08X): se reinicia\n", st, sync);
			rx_procesa();
			tx_procesa();
			reinicia(ah->curchan);
			return;
		}
		if (st & ATH9K_INT_RXORN)
			isr_rxorn++;
		if (st & ATH9K_INT_RXEOL)
			isr_rxeol++;
	}
	rx_procesa();
	tx_procesa();
	/* una trama que no acaba en un segundo: la cola está atascada */
	if (tx_cab != tx_fin) {
		int i = tx_retenido ? (tx_cab + 1) % NTX : tx_cab;
		if (i != tx_fin && time_after(jiffies, tx_cuando[i] + 1000)) {
			hl_log(1, "ath9k: el envío está atascado; se reinicia la tarjeta\n");
			reinicia(ah->curchan);
		}
	}
	calibra();
}

static void ath_barrido(int empieza) { }

static void ath_bss(const o8 *bssid, int asoc, int intervalo, int ranura_corta, const o8 *vels, int nvel)
{
	struct ath_common *c = ath9k_hw_common(ah);
	int i, j;

	if (!arriba)
		return;
	if (bssid && asoc) {
		/* ya asociados: la calibración periódica y el ANI (ath9k_bss_assoc_info) */
		asociado = 1;
		set_bit(ATH_OP_PRIM_STA_VIF, &c->op_flags);
		set_bit(ATH_OP_ANI_RUN, &c->op_flags);
		t_long = t_short = t_ani = jiffies;
		c->ani.caldone = 0;
		hl_log(2, "ath9k: asociado; calibración periódica y ANI en marcha\n");
		return;
	}
	asociado = 0;
	clear_bit(ATH_OP_PRIM_STA_VIF, &c->op_flags);
	clear_bit(ATH_OP_ANI_RUN, &c->op_flags);
	if (bssid)
		memcpy(bssid_ap, bssid, 6);
	else
		eth_zero_addr(bssid_ap);
	memcpy(c->curbssid, bssid_ap, 6);
	c->curaid = 0;
	ath9k_hw_write_associd(ah);
	if (!bssid)
		return;
	if (ah->slottime != (ranura_corta ? 9 : 20)) {
		ah->slottime = ranura_corta ? 9 : 20;
		ath9k_hw_init_global_settings(ah);
	}
	/* la escalera: las velocidades del punto de acceso, de menor a mayor */
	nesc = 0;
	for (i = 0; i < 12; i++) {
		static const u8 orden[12] = { 0, 1, 2, 4, 3, 5, 6, 7, 8, 9, 10, 11 };
		int x = orden[i];
		for (j = 0; j < nvel; j++)
			if ((vels[j] & 0x7f) == vel_medios[x]) {
				if (x != 5)
					escalera[nesc++] = x;
				break;
			}
	}
	if (!nesc)
		escalera[nesc++] = 0;
	peld = nesc / 2;
	seguidas_bien = seguidas_mal = 0;
}

static void ath_parar(void)		/* ath9k_stop */
{
	if (!arriba)
		return;
	arriba = 0;
	ath9k_hw_disable_interrupts(ah);
	tx_vacia();
	rx_para();
	ath9k_hw_phy_disable(ah);
	ath9k_hw_configpcipowersave(ah, true);
	ath9k_hw_setpower(ah, ATH9K_PM_FULL_SLEEP);
	pci_apaga(&tarjeta);
	hl_log(2, "radio: tarjeta parada\n");
}

void ath_cuentas(d32 *bien, d32 *mal, int *vel)
{
	*bien = tx_bien; *mal = tx_mal;
	*vel = nesc ? vel_medios[escalera[peld]] : 0;
}

void ath_resumen(void)
{
	if (!montada)
		return;
	hl_log(2, "ESTADO: ath9k: recibidas %u (malas %u), reinicios %u, errores graves %u, RXORN %u, RXEOL %u, ruido %d dBm, canal %u MHz\n",
	       rx_bien, rx_malas, reinicios, isr_fatal, isr_rxorn, isr_rxeol, ah->noise,
	       ah->curchan ? ah->curchan->channel : 0);
}

/* Paso 2: despertarla, leer la EEPROM y montar ath9k_hw (init.c y pci.c) */
int ath_monta(struct radio **r)
{
	struct ath_common *c = ath9k_hw_common(ah);
	struct ath9k_tx_queue_info qi;
	struct ieee80211_supported_band *sb;
	char nombre[64];
	u8 csz;
	u32 v;
	int err, i;

	pci_enable_device(&tarjeta);
	/* lo de ath_pci_probe: línea de caché, latencia, sin el «retry timeout» */
	pci_read_config_byte(&tarjeta, PCI_CACHE_LINE_SIZE, &csz);
	if (!csz)
		pci_write_config_byte(&tarjeta, PCI_CACHE_LINE_SIZE, 16);
	pci_write_config_byte(&tarjeta, PCI_LATENCY_TIMER, 0xa8);
	pci_set_master(&tarjeta);
	pci_read_config_dword(&tarjeta, 0x40, &v);
	if (v & 0x0000ff00)
		pci_write_config_dword(&tarjeta, 0x40, v & 0xffff00ff);
	mem = pci_iomap(&tarjeta, 0, 0);

	memset(ah, 0, sizeof(*ah));
	memset(&equipo, 0, sizeof(equipo));
	equipo.wiphy = &equipo.wiphy0;
	equipo.wiphy0.hw = &equipo;
	equipo.priv = NULL;
	ah->dev = &tarjeta.dev;
	ah->hw = &equipo;
	ah->hw_version.devid = tarjeta.device;
	ah->hw_version.subvendorid = tarjeta.subsystem_vendor;
	ah->ah_flags |= AH_USE_EEPROM;
	ah->led_pin = -1;
	ah->reg_ops.read = ath_lee;
	ah->reg_ops.multi_read = ath_lee_varios;
	ah->reg_ops.write = ath_escribe;
	ah->reg_ops.rmw = ath_rmw;
	c->ops = &ah->reg_ops;
	c->bus_ops = &bus_pci;
	c->ah = ah;
	c->hw = &equipo;
	c->priv = NULL;
	c->debug_mask = hl_nivel >= 3 ? (ATH_DBG_RESET | ATH_DBG_EEPROM | ATH_DBG_CALIBRATE | ATH_DBG_REGULATORY |
					  ATH_DBG_CONFIG | ATH_DBG_FATAL | ATH_DBG_ANI) : ATH_DBG_FATAL;
	c->btcoex_enabled = false;
	c->disable_ani = false;
	set_bit(ATH_OP_INVALID, &c->op_flags);
	ath_cache(c, &i);
	c->cachelsz = i << 2;

	err = ath9k_hw_init(ah);
	if (err) {
		hl_log(0, "ath9k: la tarjeta no se puede poner en marcha (%d)\n", err);
		return -4;
	}
	ath9k_hw_name(ah, nombre, sizeof(nombre));
	hl_log(2, "Chip: %s (MAC %03X rev. %d, PHY %02X, radio %02X); EEPROM versión %d.%d\n", nombre,
	       ah->hw_version.macVersion, ah->hw_version.macRev, ah->hw_version.phyRev,
	       ah->hw_version.analog5GhzRev, ah->eep_ops->get_eeprom_ver(ah), ah->eep_ops->get_eeprom_rev(ah));
	hl_log(2, "EEPROM: MAC %pM, dominio %04X, cadenas tx %X rx %X, capacidades %08X, bandas%s%s\n",
	       c->macaddr, c->regulatory.current_rd, ah->caps.tx_chainmask, ah->caps.rx_chainmask, ah->caps.hw_caps,
	       (ah->caps.hw_caps & ATH9K_HW_CAP_2GHZ) ? " 2,4 GHz" : "", (ah->caps.hw_caps & ATH9K_HW_CAP_5GHZ) ? " 5 GHz" : "");
	if (!(ah->caps.hw_caps & ATH9K_HW_CAP_2GHZ)) {
		hl_log(0, "ath9k: la tarjeta no tiene 2,4 GHz\n");
		return -5;
	}
	if (ah->caps.hw_caps & ATH9K_HW_CAP_EDMA) {
		hl_log(0, "ath9k: tarjeta de la familia AR9003, que no está incluida\n");
		return -5;
	}
	memset(&qi, 0, sizeof(qi));
	qi.tqi_subtype = ATH_TXQ_AC_BE;
	qi.tqi_aifs = ATH9K_TXQ_USEDEFAULT;
	qi.tqi_cwmin = ATH9K_TXQ_USEDEFAULT;
	qi.tqi_cwmax = ATH9K_TXQ_USEDEFAULT;
	qi.tqi_qflags = TXQ_FLAG_TXEOLINT_ENABLE | TXQ_FLAG_TXDESCINT_ENABLE;
	qnum = ath9k_hw_setuptxqueue(ah, ATH9K_TX_QUEUE_DATA, &qi);
	if (qnum < 0) {
		hl_log(0, "ath9k: no hay cola de envío\n");
		return -6;
	}
	err = ath9k_cmn_init_channels_rates(c);
	if (err)
		return -6;
	equipo.wiphy->bands[NL80211_BAND_2GHZ] = &c->sbands[NL80211_BAND_2GHZ];
	ath9k_cmn_init_crypto(ah);
	c->last_rssi = ATH_RSSI_DUMMY_MARKER;
	eth_broadcast_addr(c->bssidmask);
	ah->slottime = 9;
	err = ath_regd_init(&c->regulatory, equipo.wiphy, NULL);
	if (err)
		hl_log(1, "ath9k: dominio regulador de la EEPROM no válido (%d): normas comunes\n", err);
	hl_log(2, "País de la tarjeta (EEPROM): «%c%c», código %u (se usan las normas comunes: canales 1 a 11, y 12 y 13 solo tras oír una baliza)\n",
	       c->regulatory.alpha2[0] ? c->regulatory.alpha2[0] : '-', c->regulatory.alpha2[1] ? c->regulatory.alpha2[1] : '-',
	       c->regulatory.country_code);
	/* las potencias de cada canal (ath9k_init_txpower_limits) */
	sb = &c->sbands[NL80211_BAND_2GHZ];
	for (i = 0; i < sb->n_channels; i++) {
		ah->curchan = canal_hw(i + 1) ? &ah->channels[sb->channels[i].hw_value] : NULL;
		if (ah->curchan)
			ath9k_hw_set_txpowerlimit(ah, MAX_COMBINED_POWER, true);
	}
	ah->curchan = NULL;
	for (i = 0; i < sb->n_channels; i++)
		hl_log(3, "  canal %d: %d MHz, indicadores %x, potencia %d dBm\n", i + 1,
		       sb->channels[i].center_freq, sb->channels[i].flags, sb->channels[i].max_power);
	/* los anillos de DMA, en un trozo */
	dma_zona = dma_alloc_coherent(&tarjeta.dev, (NRX + NTX) * DTAM + NRX * RXTAM + NTX * TXTAM, &dma_fis, 0);
	if (!dma_zona)
		return -7;
	for (i = 0; i < NRX; i++) {
		rx_d[i] = (struct ath_desc *)(dma_zona + i * DTAM);
		rx_b[i] = dma_zona + (NRX + NTX) * DTAM + i * RXTAM;
	}
	for (i = 0; i < NTX; i++) {
		tx_d[i] = (struct ath_desc *)(dma_zona + (NRX + i) * DTAM);
		tx_b[i] = dma_zona + (NRX + NTX) * DTAM + NRX * RXTAM + i * TXTAM;
	}
	memcpy(ath.mac, c->macaddr, 6);
	ath.canal = ath_canal;
	ath.enviar = ath_enviar;
	ath.sondear = ath_sondear;
	ath.barrido = ath_barrido;
	ath.bss = ath_bss;
	ath.parar = ath_parar;
	montada = 1;
	*r = &ath;
	hl_log(2, "Controlador montado. MAC %pM, cola de envío %d, %d descriptores de recepción\n", ath.mac, qnum, NRX);
	return 0;
}

/* Paso 3: encender la radio en el canal 1 (ath9k_start) */
int ath_enciende(void)
{
	struct ath_common *c = ath9k_hw_common(ah);
	struct ath9k_channel *h;
	int r;

	hl_log(2, "Encendiendo la radio (valores de arranque, PHY, calibración)...\n");
	if (ah->caps.hw_caps & ATH9K_HW_CAP_RFSILENT) {
		u32 g = ath9k_hw_gpio_get(ah, ah->rfkill_gpio);
		hl_log(2, "ath9k: interruptor de la radio en la pata GPIO %d (polaridad %d): ahora %d\n",
		       ah->rfkill_gpio, ah->rfkill_polarity, g);
		if (g == ah->rfkill_polarity)
			hl_log(0, "la radio parece APAGADA por el interruptor o el botón del Wi-Fi del equipo: enciéndalo y vuelva a cargar HSPWIFI\n");
	}
	h = canal_hw(1);
	ath9k_hw_configpcipowersave(ah, false);
	atomic_set(&ah->intr_ref_cnt, -1);
	r = ath9k_hw_reset(ah, h, &caldata, false);
	if (r) {
		hl_log(0, "la radio no se enciende: ath9k_hw_reset devuelve %d\n", r);
		return -11;
	}
	clear_bit(ATH_OP_INVALID, &c->op_flags);
	arriba = 1;
	completa();
	ath9k_cmn_init_crypto(ah);
	ath9k_hw_reset_tsf(ah);
	hl_log(2, "Radio encendida en el canal 1 (ruido %d dBm, potencia máxima %d dBm)\n", ah->noise,
	       c->regulatory.max_power_level / 2);
	return 0;
}
