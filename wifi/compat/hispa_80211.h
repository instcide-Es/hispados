/*
 * hispa_80211.h - Las definiciones de 802.11 y la parte de mac80211/cfg80211
 * que usa brcmsmac. Lo que en Linux hace mac80211 (gestión, cifrado, elección
 * de velocidad) lo hace aquí wifi/nucleo/ (w80211.c, wpa.c, radio_brcm.c).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#ifndef HISPA_80211_H
#define HISPA_80211_H

/* ------------------------------------------------------- tramas 802.11 */
#define IEEE80211_FCTL_VERS 0x0003
#define IEEE80211_FCTL_FTYPE 0x000c
#define IEEE80211_FCTL_STYPE 0x00f0
#define IEEE80211_FCTL_TODS 0x0100
#define IEEE80211_FCTL_FROMDS 0x0200
#define IEEE80211_FCTL_MOREFRAGS 0x0400
#define IEEE80211_FCTL_RETRY 0x0800
#define IEEE80211_FCTL_PM 0x1000
#define IEEE80211_FCTL_MOREDATA 0x2000
#define IEEE80211_FCTL_PROTECTED 0x4000
#define IEEE80211_FCTL_ORDER 0x8000
#define IEEE80211_SCTL_FRAG 0x000F
#define IEEE80211_SCTL_SEQ 0xFFF0
#define IEEE80211_FTYPE_MGMT 0x0000
#define IEEE80211_FTYPE_CTL 0x0004
#define IEEE80211_FTYPE_DATA 0x0008
#define IEEE80211_STYPE_ASSOC_REQ 0x0000
#define IEEE80211_STYPE_ASSOC_RESP 0x0010
#define IEEE80211_STYPE_REASSOC_REQ 0x0020
#define IEEE80211_STYPE_REASSOC_RESP 0x0030
#define IEEE80211_STYPE_PROBE_REQ 0x0040
#define IEEE80211_STYPE_PROBE_RESP 0x0050
#define IEEE80211_STYPE_BEACON 0x0080
#define IEEE80211_STYPE_ATIM 0x0090
#define IEEE80211_STYPE_DISASSOC 0x00A0
#define IEEE80211_STYPE_AUTH 0x00B0
#define IEEE80211_STYPE_DEAUTH 0x00C0
#define IEEE80211_STYPE_ACTION 0x00D0
#define IEEE80211_STYPE_BACK_REQ 0x0080
#define IEEE80211_STYPE_BACK 0x0090
#define IEEE80211_STYPE_PSPOLL 0x00A0
#define IEEE80211_STYPE_RTS 0x00B0
#define IEEE80211_STYPE_CTS 0x00C0
#define IEEE80211_STYPE_ACK 0x00D0
#define IEEE80211_STYPE_DATA 0x0000
#define IEEE80211_STYPE_NULLFUNC 0x0040
#define IEEE80211_STYPE_QOS_DATA 0x0080
#define IEEE80211_STYPE_QOS_NULLFUNC 0x00C0
#define IEEE80211_MAX_SSID_LEN 32
#define IEEE80211_MAX_FRAME_LEN 2352
#define IEEE80211_MAX_DATA_LEN 2304
#define IEEE80211_MAX_RTS_THRESHOLD 2353
#define IEEE80211_MAX_FRAG_THRESHOLD 2352
#define IEEE80211_QOS_CTL_LEN 2
#define IEEE80211_QOS_CTL_TID_MASK 0x000f
#define IEEE80211_QOS_CTL_TAG1D_MASK 0x0007
#define IEEE80211_FCS_LEN 4
#define IEEE80211_NUM_ACS 4
#define IEEE80211_NUM_TIDS 16
#define IEEE80211_HT_MCS_MASK_LEN 10
#define IEEE80211_MAX_CHAINS 4
#define FCS_LEN 4
#define DOT11_MAX_FRAG_LEN 2346

struct ieee80211_hdr {
	__le16 frame_control;
	__le16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	__le16 seq_ctrl;
	u8 addr4[6];
} __packed __aligned(2);
struct ieee80211_hdr_3addr {
	__le16 frame_control;
	__le16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	__le16 seq_ctrl;
} __packed __aligned(2);
struct ieee80211_rts {
	__le16 frame_control;
	__le16 duration;
	u8 ra[6];
	u8 ta[6];
} __packed __aligned(2);
struct ieee80211_cts {
	__le16 frame_control;
	__le16 duration;
	u8 ra[6];
} __packed __aligned(2);
struct ieee80211_pspoll {
	__le16 frame_control;
	__le16 aid;
	u8 bssid[6];
	u8 ta[6];
} __packed __aligned(2);
struct ieee80211_mcs_info {
	u8 rx_mask[IEEE80211_HT_MCS_MASK_LEN];
	__le16 rx_highest;
	u8 tx_params;
	u8 reserved[3];
} __packed;
struct ieee80211_ht_cap {
	__le16 cap_info;
	u8 ampdu_params_info;
	struct ieee80211_mcs_info mcs;
	__le16 extended_ht_cap_info;
	__le32 tx_BF_cap_info;
	u8 antenna_selection_info;
} __packed;

#define _FC(h) (((const struct ieee80211_hdr *)0, (u16)(h)))
static inline bool ieee80211_has_tods(__le16 fc) { return (fc & IEEE80211_FCTL_TODS) != 0; }
static inline bool ieee80211_has_fromds(__le16 fc) { return (fc & IEEE80211_FCTL_FROMDS) != 0; }
static inline bool ieee80211_has_a4(__le16 fc) { return (fc & 0x0300) == 0x0300; }
static inline bool ieee80211_has_morefrags(__le16 fc) { return (fc & IEEE80211_FCTL_MOREFRAGS) != 0; }
static inline bool ieee80211_has_retry(__le16 fc) { return (fc & IEEE80211_FCTL_RETRY) != 0; }
static inline bool ieee80211_has_protected(__le16 fc) { return (fc & IEEE80211_FCTL_PROTECTED) != 0; }
static inline bool ieee80211_is_mgmt(__le16 fc) { return (fc & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_MGMT; }
static inline bool ieee80211_is_ctl(__le16 fc) { return (fc & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_CTL; }
static inline bool ieee80211_is_data(__le16 fc) { return (fc & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_DATA; }
static inline bool ieee80211_is_data_qos(__le16 fc) { return (fc & (IEEE80211_FCTL_FTYPE | IEEE80211_STYPE_QOS_DATA)) == (IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_DATA); }
static inline bool ieee80211_is_beacon(__le16 fc) { return (fc & 0xfc) == (IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON); }
static inline bool ieee80211_is_probe_resp(__le16 fc) { return (fc & 0xfc) == (IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP); }
static inline bool ieee80211_is_probe_req(__le16 fc) { return (fc & 0xfc) == (IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_REQ); }
static inline bool ieee80211_is_pspoll(__le16 fc) { return (fc & 0xfc) == (IEEE80211_FTYPE_CTL | IEEE80211_STYPE_PSPOLL); }
static inline bool ieee80211_is_rts(__le16 fc) { return (fc & 0xfc) == (IEEE80211_FTYPE_CTL | IEEE80211_STYPE_RTS); }
static inline bool ieee80211_is_cts(__le16 fc) { return (fc & 0xfc) == (IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CTS); }
static inline bool ieee80211_is_ack(__le16 fc) { return (fc & 0xfc) == (IEEE80211_FTYPE_CTL | IEEE80211_STYPE_ACK); }
static inline bool ieee80211_is_nullfunc(__le16 fc) { return (fc & 0xfc) == (IEEE80211_FTYPE_DATA | IEEE80211_STYPE_NULLFUNC); }
static inline bool ieee80211_is_qos_nullfunc(__le16 fc) { return (fc & 0xfc) == (IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC); }
static inline unsigned int ieee80211_hdrlen(__le16 fc)
{
	unsigned int n = 24;
	if (ieee80211_is_data(fc)) {
		if (ieee80211_has_a4(fc)) n = 30;
		if (fc & IEEE80211_STYPE_QOS_DATA) n += 2;
	} else if (ieee80211_is_ctl(fc)) {
		n = ((fc & 0xe0) == 0xc0) ? 10 : 16;
	}
	return n;
}
static inline u8 *ieee80211_get_qos_ctl(struct ieee80211_hdr *h)
{ return ieee80211_has_a4(h->frame_control) ? (u8 *)h + 30 : (u8 *)h + 24; }
static inline u8 *ieee80211_get_DA(struct ieee80211_hdr *h)
{ return ieee80211_has_tods(h->frame_control) ? h->addr3 : h->addr1; }

/* HT (802.11n) */
#define IEEE80211_HT_CAP_LDPC_CODING 0x0001
#define IEEE80211_HT_CAP_SUP_WIDTH_20_40 0x0002
#define IEEE80211_HT_CAP_SM_PS 0x000C
#define IEEE80211_HT_CAP_GRN_FLD 0x0010
#define IEEE80211_HT_CAP_SGI_20 0x0020
#define IEEE80211_HT_CAP_SGI_40 0x0040
#define IEEE80211_HT_CAP_TX_STBC 0x0080
#define IEEE80211_HT_CAP_RX_STBC 0x0300
#define IEEE80211_HT_CAP_DELAY_BA 0x0400
#define IEEE80211_HT_CAP_MAX_AMSDU 0x0800
#define IEEE80211_HT_CAP_DSSSCCK40 0x1000
#define IEEE80211_HT_CAP_40MHZ_INTOLERANT 0x4000
#define IEEE80211_HT_AMPDU_PARM_FACTOR 0x03
#define IEEE80211_HT_AMPDU_PARM_DENSITY 0x1C
#define IEEE80211_HT_MCS_TX_DEFINED 0x01
#define IEEE80211_HT_OP_MODE_PROTECTION 0x0003
#define IEEE80211_HT_OP_MODE_NON_GF_STA_PRSNT 0x0004
#define IEEE80211_HT_OP_MODE_NON_HT_STA_PRSNT 0x0010
enum ieee80211_max_ampdu_length_exp {
	IEEE80211_HT_MAX_AMPDU_8K = 0, IEEE80211_HT_MAX_AMPDU_16K = 1,
	IEEE80211_HT_MAX_AMPDU_32K = 2, IEEE80211_HT_MAX_AMPDU_64K = 3
};
#define IEEE80211_HT_MAX_AMPDU_FACTOR 13
enum ieee80211_min_mpdu_spacing { IEEE80211_HT_MPDU_DENSITY_NONE = 0, IEEE80211_HT_MPDU_DENSITY_4 = 5, IEEE80211_HT_MPDU_DENSITY_8 = 6 };
#define WLAN_EID_SSID 0
#define WLAN_EID_SUPP_RATES 1
#define WLAN_EID_DS_PARAMS 3
#define WLAN_EID_TIM 5
#define WLAN_EID_ERP_INFO 42
#define WLAN_EID_RSN 48
#define WLAN_EID_EXT_SUPP_RATES 50
#define WLAN_EID_VENDOR_SPECIFIC 221

/* ------------------------------------------------------------ cfg80211 */
enum nl80211_band { NL80211_BAND_2GHZ, NL80211_BAND_5GHZ, NL80211_BAND_60GHZ, NUM_NL80211_BANDS };
#define IEEE80211_BAND_2GHZ NL80211_BAND_2GHZ
enum nl80211_iftype {
	NL80211_IFTYPE_UNSPECIFIED, NL80211_IFTYPE_ADHOC, NL80211_IFTYPE_STATION,
	NL80211_IFTYPE_AP, NL80211_IFTYPE_AP_VLAN, NL80211_IFTYPE_WDS,
	NL80211_IFTYPE_MONITOR, NL80211_IFTYPE_MESH_POINT
};
enum nl80211_chan_width { NL80211_CHAN_WIDTH_20_NOHT, NL80211_CHAN_WIDTH_20, NL80211_CHAN_WIDTH_40 };
enum nl80211_channel_type { NL80211_CHAN_NO_HT, NL80211_CHAN_HT20, NL80211_CHAN_HT40MINUS, NL80211_CHAN_HT40PLUS };
enum nl80211_reg_initiator {
	NL80211_REGDOM_SET_BY_CORE, NL80211_REGDOM_SET_BY_USER,
	NL80211_REGDOM_SET_BY_DRIVER, NL80211_REGDOM_SET_BY_COUNTRY_IE
};
enum nl80211_ext_feature_index { NL80211_EXT_FEATURE_CQM_RSSI_LIST = 1 };
enum nl80211_dfs_regions { NL80211_DFS_UNSET };
#define NL80211_RRF_NO_OFDM (1 << 0)
#define NL80211_RRF_NO_CCK (1 << 1)
#define NL80211_RRF_NO_INDOOR (1 << 2)
#define NL80211_RRF_NO_OUTDOOR (1 << 3)
#define NL80211_RRF_DFS (1 << 4)
#define NL80211_RRF_NO_IR (1 << 7)
#define NL80211_RRF_AUTO_BW (1 << 11)
#define NL80211_RRF_PASSIVE_SCAN NL80211_RRF_NO_IR
#define NL80211_RRF_NO_IBSS NL80211_RRF_NO_IR
#define NL80211_RRF_NO_HT40MINUS (1 << 13)
#define NL80211_RRF_NO_HT40PLUS (1 << 14)

enum ieee80211_channel_flags {
	IEEE80211_CHAN_DISABLED = 1 << 0, IEEE80211_CHAN_NO_IR = 1 << 1,
	IEEE80211_CHAN_RADAR = 1 << 3, IEEE80211_CHAN_NO_HT40PLUS = 1 << 4,
	IEEE80211_CHAN_NO_HT40MINUS = 1 << 5, IEEE80211_CHAN_NO_OFDM = 1 << 6,
};
#define IEEE80211_CHAN_NO_HT40 (IEEE80211_CHAN_NO_HT40PLUS | IEEE80211_CHAN_NO_HT40MINUS)
struct ieee80211_channel {
	enum nl80211_band band;
	u32 center_freq;
	u16 hw_value;
	u32 flags;
	int max_antenna_gain;
	int max_power;
	int max_reg_power;
	bool beacon_found;
	u32 orig_flags;
	int orig_mag, orig_mpwr;
};
enum ieee80211_rate_flags { IEEE80211_RATE_SHORT_PREAMBLE = 1 << 0, IEEE80211_RATE_MANDATORY_B = 1 << 2, IEEE80211_RATE_ERP_G = 1 << 4 };
struct ieee80211_rate { u32 flags; u16 bitrate; u16 hw_value, hw_value_short; };
struct ieee80211_sta_ht_cap {
	u16 cap;
	bool ht_supported;
	u8 ampdu_factor;
	u8 ampdu_density;
	struct ieee80211_mcs_info mcs;
};
struct ieee80211_supported_band {
	struct ieee80211_channel *channels;
	struct ieee80211_rate *bitrates;
	enum nl80211_band band;
	int n_channels;
	int n_bitrates;
	struct ieee80211_sta_ht_cap ht_cap;
};
struct cfg80211_chan_def { struct ieee80211_channel *chan; enum nl80211_chan_width width; u32 center_freq1; };
struct ieee80211_freq_range { u32 start_freq_khz, end_freq_khz, max_bandwidth_khz; };
struct ieee80211_power_rule { u32 max_antenna_gain, max_eirp; };
struct ieee80211_reg_rule {
	struct ieee80211_freq_range freq_range;
	struct ieee80211_power_rule power_rule;
	u32 flags;
	u32 dfs_cac_ms;
};
struct ieee80211_regdomain {
	u32 n_reg_rules;
	char alpha2[3];
	enum nl80211_dfs_regions dfs_region;
	struct ieee80211_reg_rule reg_rules[8];
};
struct regulatory_request { enum nl80211_reg_initiator initiator; char alpha2[3]; };
#define MHZ_TO_KHZ(f) ((f) * 1000)
#define KHZ_TO_MHZ(f) ((f) / 1000)
#define DBI_TO_MBI(g) ((g) * 100)
#define MBI_TO_DBI(g) ((g) / 100)
#define DBM_TO_MBM(g) ((g) * 100)
#define MBM_TO_DBM(g) ((g) / 100)
#define REG_RULE(start, end, bw, gain, eirp, reg_flags) { \
	.freq_range.start_freq_khz = MHZ_TO_KHZ(start), \
	.freq_range.end_freq_khz = MHZ_TO_KHZ(end), \
	.freq_range.max_bandwidth_khz = MHZ_TO_KHZ(bw), \
	.power_rule.max_antenna_gain = DBI_TO_MBI(gain), \
	.power_rule.max_eirp = DBM_TO_MBM(eirp), \
	.flags = reg_flags }
#define REGULATORY_CUSTOM_REG 1
#define REGULATORY_STRICT_REG 2
struct wiphy {
	struct ieee80211_supported_band *bands[NUM_NL80211_BANDS];
	u32 flags;
	u32 regulatory_flags;
	u16 interface_modes;
	char fw_version[32];
	void (*reg_notifier)(struct wiphy *w, struct regulatory_request *r);
	struct device dev;
	void *hw;
};
#define wiphy_err(w, f, a...) hl_log(0, f, ##a)
#define wiphy_warn(w, f, a...) hl_log(1, f, ##a)
#define wiphy_info(w, f, a...) hl_log(2, f, ##a)
#define wiphy_debug(w, f, a...) hl_log(3, f, ##a)
#define wiphy_dbg(w, f, a...) hl_log(3, f, ##a)
#define wiphy_name(w) "wifi0"
#define wiphy_dev(w) (&(w)->dev)
#define wiphy_ext_feature_set(w, f) do { } while (0)
#define wiphy_rfkill_set_hw_state(w, b) do { } while (0)
#define wiphy_rfkill_start_polling(w) do { } while (0)
#define wiphy_rfkill_stop_polling(w) do { } while (0)
#define wiphy_to_ieee80211_hw(w) ((struct ieee80211_hw *)(w)->hw)
int ieee80211_channel_to_frequency(int canal, enum nl80211_band b);
int ieee80211_frequency_to_channel(int frec);
const struct ieee80211_reg_rule *freq_reg_info(struct wiphy *w, u32 khz);
void wiphy_apply_custom_regulatory(struct wiphy *w, const struct ieee80211_regdomain *r);
int regulatory_hint(struct wiphy *w, const char *alpha2);

/* ------------------------------------------------------------ mac80211 */
enum ieee80211_ac_numbers { IEEE80211_AC_VO, IEEE80211_AC_VI, IEEE80211_AC_BE, IEEE80211_AC_BK };
enum mac80211_tx_info_flags {
	IEEE80211_TX_CTL_REQ_TX_STATUS = BIT(0), IEEE80211_TX_CTL_ASSIGN_SEQ = BIT(1),
	IEEE80211_TX_CTL_NO_ACK = BIT(2), IEEE80211_TX_CTL_FIRST_FRAGMENT = BIT(4),
	IEEE80211_TX_CTL_AMPDU = BIT(6), IEEE80211_TX_STAT_TX_FILTERED = BIT(8),
	IEEE80211_TX_STAT_ACK = BIT(9), IEEE80211_TX_STAT_AMPDU = BIT(10),
	IEEE80211_TX_STAT_AMPDU_NO_BACK = BIT(11),
};
enum mac80211_rate_control_flags {
	IEEE80211_TX_RC_USE_RTS_CTS = BIT(0), IEEE80211_TX_RC_USE_CTS_PROTECT = BIT(1),
	IEEE80211_TX_RC_USE_SHORT_PREAMBLE = BIT(2), IEEE80211_TX_RC_MCS = BIT(3),
	IEEE80211_TX_RC_GREEN_FIELD = BIT(4), IEEE80211_TX_RC_40_MHZ_WIDTH = BIT(5),
	IEEE80211_TX_RC_DUP_DATA = BIT(6), IEEE80211_TX_RC_SHORT_GI = BIT(7),
};
#define IEEE80211_TX_MAX_RATES 4
#define IEEE80211_TX_INFO_DRIVER_DATA_SIZE 40
#define IEEE80211_TX_INFO_RATE_DRIVER_DATA_SIZE 24
struct ieee80211_tx_rate { s8 idx; u16 count:5, flags:11; } __packed;
struct ieee80211_vif;
struct ieee80211_tx_info {
	u32 flags;
	u8 band;
	u8 hw_queue;
	u16 ack_frame_id;
	union {
		struct {
			union {
				struct {
					struct ieee80211_tx_rate rates[IEEE80211_TX_MAX_RATES];
					s8 rts_cts_rate_idx;
					u8 use_rts:1;
					u8 use_cts_prot:1;
					u8 short_preamble:1;
					u8 skip_table:1;
				};
			};
			struct ieee80211_vif *vif;
			void *hw_key;
			u32 flags;
		} control;
		struct {
			struct ieee80211_tx_rate rates[IEEE80211_TX_MAX_RATES];
			s32 ack_signal;
			u8 ampdu_ack_len;
			u8 ampdu_len;
			u8 antenna;
			u16 tx_time;
			void *status_driver_data[19 / sizeof(void *)];
		} status;
		struct {
			struct ieee80211_tx_rate driver_rates[IEEE80211_TX_MAX_RATES];
			u8 pad[4];
			void *rate_driver_data[IEEE80211_TX_INFO_RATE_DRIVER_DATA_SIZE / sizeof(void *)];
		};
		void *driver_data[IEEE80211_TX_INFO_DRIVER_DATA_SIZE / sizeof(void *)];
	};
};
enum mac80211_rx_flags {
	RX_FLAG_MMIC_ERROR = BIT(0), RX_FLAG_DECRYPTED = BIT(1), RX_FLAG_MACTIME_PLCP_START = BIT(2),
	RX_FLAG_MMIC_STRIPPED = BIT(3), RX_FLAG_IV_STRIPPED = BIT(4), RX_FLAG_FAILED_FCS_CRC = BIT(5),
	RX_FLAG_FAILED_PLCP_CRC = BIT(6), RX_FLAG_MACTIME_START = BIT(7),
};
enum mac80211_rx_encoding { RX_ENC_LEGACY = 0, RX_ENC_HT, RX_ENC_VHT };
enum mac80211_rx_encoding_flags { RX_ENC_FLAG_SHORTPRE = BIT(0), RX_ENC_FLAG_SHORT_GI = BIT(2), RX_ENC_FLAG_HT_GF = BIT(3) };
enum rate_info_bw { RATE_INFO_BW_20 = 0, RATE_INFO_BW_40 = 3 };
struct ieee80211_rx_status {
	u64 mactime;
	u32 flag;
	u16 freq;
	u8 enc_flags;
	u8 encoding:2, bw:3;
	u8 rate_idx;
	u8 band;
	u8 antenna;
	s8 signal;
};
static inline struct ieee80211_tx_info *IEEE80211_SKB_CB(struct sk_buff *s) { return (struct ieee80211_tx_info *)s->cb; }
static inline struct ieee80211_rx_status *IEEE80211_SKB_RXCB(struct sk_buff *s) { return (struct ieee80211_rx_status *)s->cb; }
static inline void ieee80211_tx_info_clear_status(struct ieee80211_tx_info *i)
{
	int k;
	for (k = 0; k < IEEE80211_TX_MAX_RATES; k++) i->status.rates[k].count = 0;
	memset(&i->status.ack_signal, 0, sizeof(*i) - offsetof(struct ieee80211_tx_info, status.ack_signal));
}

enum ieee80211_conf_flags { IEEE80211_CONF_MONITOR = 1 << 0, IEEE80211_CONF_PS = 1 << 1, IEEE80211_CONF_IDLE = 1 << 2 };
enum ieee80211_conf_changed {
	IEEE80211_CONF_CHANGE_SMPS = BIT(1), IEEE80211_CONF_CHANGE_LISTEN_INTERVAL = BIT(2),
	IEEE80211_CONF_CHANGE_MONITOR = BIT(3), IEEE80211_CONF_CHANGE_PS = BIT(4),
	IEEE80211_CONF_CHANGE_POWER = BIT(5), IEEE80211_CONF_CHANGE_CHANNEL = BIT(6),
	IEEE80211_CONF_CHANGE_RETRY_LIMITS = BIT(7), IEEE80211_CONF_CHANGE_IDLE = BIT(8),
};
struct ieee80211_conf {
	u32 flags;
	int power_level;
	u16 listen_interval;
	u8 long_frame_max_tx_count, short_frame_max_tx_count;
	struct cfg80211_chan_def chandef;
};
enum ieee80211_bss_change {
	BSS_CHANGED_ASSOC = 1 << 0, BSS_CHANGED_ERP_CTS_PROT = 1 << 1, BSS_CHANGED_ERP_PREAMBLE = 1 << 2,
	BSS_CHANGED_ERP_SLOT = 1 << 3, BSS_CHANGED_HT = 1 << 4, BSS_CHANGED_BASIC_RATES = 1 << 5,
	BSS_CHANGED_BEACON_INT = 1 << 6, BSS_CHANGED_BSSID = 1 << 7, BSS_CHANGED_BEACON = 1 << 8,
	BSS_CHANGED_BEACON_ENABLED = 1 << 9, BSS_CHANGED_CQM = 1 << 10, BSS_CHANGED_IBSS = 1 << 11,
	BSS_CHANGED_ARP_FILTER = 1 << 12, BSS_CHANGED_QOS = 1 << 13, BSS_CHANGED_IDLE = 1 << 14,
	BSS_CHANGED_SSID = 1 << 15, BSS_CHANGED_AP_PROBE_RESP = 1 << 16, BSS_CHANGED_PS = 1 << 17,
	BSS_CHANGED_TXPOWER = 1 << 18,
};
struct ieee80211_bss_conf {
	const u8 *bssid;
	bool assoc, ibss_joined, ibss_creator;
	u16 aid;
	bool use_cts_prot, use_short_preamble, use_short_slot;
	bool enable_beacon;
	u8 dtim_period;
	u16 beacon_int;
	u16 assoc_capability;
	u32 basic_rates;
	u16 ht_operation_mode;
	s32 cqm_rssi_thold;
	u32 cqm_rssi_hyst;
	struct cfg80211_chan_def chandef;
	__be32 arp_addr_list[4];
	int arp_addr_cnt;
	bool qos;
	bool idle;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	size_t ssid_len;
	int txpower;
};
struct ieee80211_vif {
	enum nl80211_iftype type;
	struct ieee80211_bss_conf bss_conf;
	u8 addr[6];
	bool p2p;
	u8 drv_priv[0] __aligned(sizeof(void *));
};
struct ieee80211_sta {
	u32 supp_rates[NUM_NL80211_BANDS];
	u8 addr[6];
	u16 aid;
	struct ieee80211_sta_ht_cap ht_cap;
	bool wme;
	u8 drv_priv[0] __aligned(sizeof(void *));
};
struct ieee80211_tx_queue_params { u16 txop; u16 cw_min; u16 cw_max; u8 aifs; bool acm; bool uapsd; };
struct ieee80211_tx_control { struct ieee80211_sta *sta; };
enum ieee80211_ampdu_mlme_action {
	IEEE80211_AMPDU_RX_START, IEEE80211_AMPDU_RX_STOP, IEEE80211_AMPDU_TX_START,
	IEEE80211_AMPDU_TX_STOP_CONT, IEEE80211_AMPDU_TX_STOP_FLUSH,
	IEEE80211_AMPDU_TX_STOP_FLUSH_CONT, IEEE80211_AMPDU_TX_OPERATIONAL,
};
#define IEEE80211_AMPDU_TX_START_IMMEDIATE 1
struct ieee80211_ampdu_params {
	enum ieee80211_ampdu_mlme_action action;
	struct ieee80211_sta *sta;
	u16 tid; u16 ssn; u16 buf_size; bool amsdu; u16 timeout;
};
enum ieee80211_filter_flags {
	FIF_ALLMULTI = 1 << 1, FIF_FCSFAIL = 1 << 2, FIF_PLCPFAIL = 1 << 3,
	FIF_BCN_PRBRESP_PROMISC = 1 << 4, FIF_CONTROL = 1 << 5, FIF_OTHER_BSS = 1 << 6,
	FIF_PSPOLL = 1 << 7, FIF_PROBE_REQ = 1 << 8,
};
enum ieee80211_hw_flags {
	IEEE80211_HW_AMPDU_AGGREGATION, IEEE80211_HW_SIGNAL_DBM,
	IEEE80211_HW_REPORTS_TX_ACK_STATUS, NUM_IEEE80211_HW_FLAGS
};
struct ieee80211_hw {
	struct ieee80211_conf conf;
	struct wiphy *wiphy;
	const char *rate_control_algorithm;
	void *priv;
	unsigned long flags[1];
	unsigned int extra_tx_headroom;
	int vif_data_size, sta_data_size;
	u16 queues;
	u16 max_listen_interval;
	s8 max_signal;
	u8 max_rates, max_report_rates, max_rate_tries;
	struct wiphy wiphy0;
	const struct ieee80211_ops *ops;
};
#define ieee80211_hw_set(hw, flg) set_bit(IEEE80211_HW_##flg, (hw)->flags)
struct ieee80211_ops {
	void (*tx)(struct ieee80211_hw *hw, struct ieee80211_tx_control *c, struct sk_buff *skb);
	int (*start)(struct ieee80211_hw *hw);
	void (*stop)(struct ieee80211_hw *hw);
	int (*add_interface)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	void (*remove_interface)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	int (*config)(struct ieee80211_hw *hw, u32 changed);
	void (*bss_info_changed)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
				 struct ieee80211_bss_conf *info, u32 changed);
	void (*configure_filter)(struct ieee80211_hw *hw, unsigned int changed,
				 unsigned int *total, u64 multicast);
	void (*sw_scan_start)(struct ieee80211_hw *hw, struct ieee80211_vif *vif, const u8 *mac);
	void (*sw_scan_complete)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	int (*conf_tx)(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u16 ac,
		       const struct ieee80211_tx_queue_params *p);
	int (*sta_add)(struct ieee80211_hw *hw, struct ieee80211_vif *vif, struct ieee80211_sta *sta);
	int (*ampdu_action)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			    struct ieee80211_ampdu_params *p);
	void (*rfkill_poll)(struct ieee80211_hw *hw);
	void (*flush)(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u32 queues, bool drop);
	u64 (*get_tsf)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	void (*set_tsf)(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u64 tsf);
	int (*set_tim)(struct ieee80211_hw *hw, struct ieee80211_sta *sta, bool set);
};
struct ieee80211_hw *ieee80211_alloc_hw(size_t priv, const struct ieee80211_ops *ops);
void ieee80211_free_hw(struct ieee80211_hw *hw);
int ieee80211_register_hw(struct ieee80211_hw *hw);
void ieee80211_unregister_hw(struct ieee80211_hw *hw);
void ieee80211_restart_hw(struct ieee80211_hw *hw);
void ieee80211_rx_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb);
void ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb);
void ieee80211_wake_queue(struct ieee80211_hw *hw, int q);
void ieee80211_stop_queue(struct ieee80211_hw *hw, int q);
void ieee80211_wake_queues(struct ieee80211_hw *hw);
void ieee80211_stop_queues(struct ieee80211_hw *hw);
int ieee80211_queue_stopped(struct ieee80211_hw *hw, int q);
struct sk_buff *ieee80211_beacon_get_tim(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u16 *tim_offset, u16 *tim_length);
struct sk_buff *ieee80211_proberesp_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
struct ieee80211_rate *ieee80211_get_tx_rate(const struct ieee80211_hw *hw, const struct ieee80211_tx_info *c);
#define ieee80211_queue_delayed_work(hw, d, t) hl_queue_dwork(d, t)
#define ieee80211_start_tx_ba_session(sta, tid, t) (-1)
#define ieee80211_stop_tx_ba_cb_irqsafe(vif, ra, tid) do { } while (0)
#define ieee80211_start_tx_ba_cb_irqsafe(vif, ra, tid) do { } while (0)
#define ieee80211_get_radio_led_name(hw) "wifi"
#define SET_IEEE80211_DEV(hw, d) do { } while (0)
#define SET_IEEE80211_PERM_ADDR(hw, a) hl_mac_propia(a)
void hl_mac_propia(const u8 *mac);

#endif
